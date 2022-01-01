/* $Id$ */
/** @file
 * MM - Memory Manager - Hypervisor Memory Area.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MM_HYPER
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/dbgf.h>
#include "MMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/gvm.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int mmR3HyperMap(PVM pVM, const size_t cb, const char *pszDesc, PRTGCPTR pGCPtr, PMMLOOKUPHYPER *ppLookup);
static int mmR3HyperHeapCreate(PVM pVM, const size_t cb, PMMHYPERHEAP *ppHeap, PRTR0PTR pR0PtrHeap);
static int mmR3HyperHeapMap(PVM pVM, PMMHYPERHEAP pHeap, PRTGCPTR ppHeapGC);
static DECLCALLBACK(void) mmR3HyperInfoHma(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static int MMR3HyperReserveFence(PVM pVM);
static int MMR3HyperMapPages(PVM pVM, void *pvR3, RTR0PTR pvR0, size_t cPages, PCSUPPAGE paPages,
                             const char *pszDesc, PRTGCPTR pGCPtr);


/**
 * Determin the default heap size.
 *
 * @returns The heap size in bytes.
 * @param   pVM     The cross context VM structure.
 */
static uint32_t mmR3HyperComputeHeapSize(PVM pVM)
{
    /** @todo Redo after moving allocations off the hyper heap. */

    /*
     * Gather parameters.
     */
    bool        fCanUseLargerHeap = true;
    //bool        fCanUseLargerHeap;
    //int rc = CFGMR3QueryBoolDef(CFGMR3GetChild(CFGMR3GetRoot(pVM), "MM"), "CanUseLargerHeap", &fCanUseLargerHeap, false);
    //AssertStmt(RT_SUCCESS(rc), fCanUseLargerHeap = false);

    uint64_t    cbRam;
    int rc = CFGMR3QueryU64(CFGMR3GetRoot(pVM), "RamSize", &cbRam);
    AssertStmt(RT_SUCCESS(rc), cbRam = _1G);

    /*
     * We need to keep saved state compatibility if raw-mode is an option,
     * so lets filter out that case first.
     */
    if (   !fCanUseLargerHeap
        && VM_IS_RAW_MODE_ENABLED(pVM)
        && cbRam < 16*_1G64)
        return 1280 * _1K;

    /*
     * Calculate the heap size.
     */
    uint32_t cbHeap = _1M;

    /* The newer chipset may have more devices attached, putting additional
       pressure on the heap. */
    if (fCanUseLargerHeap)
        cbHeap += _1M;

    /* More CPUs means some extra memory usage. */
    if (pVM->cCpus > 1)
        cbHeap += pVM->cCpus * _64K;

    /* Lots of memory means extra memory consumption as well (pool). */
    if (cbRam > 16*_1G64)
        cbHeap += _2M; /** @todo figure out extactly how much */

    return RT_ALIGN(cbHeap, _256K);
}


/**
 * Initializes the hypervisor related MM stuff without
 * calling down to PGM.
 *
 * PGM is not initialized at this  point, PGM relies on
 * the heap to initialize.
 *
 * @returns VBox status code.
 */
int mmR3HyperInit(PVM pVM)
{
    LogFlow(("mmR3HyperInit:\n"));

    /*
     * Decide Hypervisor mapping in the guest context
     * And setup various hypervisor area and heap parameters.
     */
    pVM->mm.s.pvHyperAreaGC = (RTGCPTR)MM_HYPER_AREA_ADDRESS;
    pVM->mm.s.cbHyperArea   = MM_HYPER_AREA_MAX_SIZE;
    AssertRelease(RT_ALIGN_T(pVM->mm.s.pvHyperAreaGC, 1 << X86_PD_SHIFT, RTGCPTR) == pVM->mm.s.pvHyperAreaGC);
    Assert(pVM->mm.s.pvHyperAreaGC < 0xff000000);

    /** @todo @bugref{1865}, @bugref{3202}: Change the cbHyperHeap default
     *        depending on whether VT-x/AMD-V is enabled or not! Don't waste
     *        precious kernel space on heap for the PATM.
     */
    PCFGMNODE pMM = CFGMR3GetChild(CFGMR3GetRoot(pVM), "MM");
    uint32_t cbHyperHeap;
    int rc = CFGMR3QueryU32Def(pMM, "cbHyperHeap", &cbHyperHeap, mmR3HyperComputeHeapSize(pVM));
    AssertLogRelRCReturn(rc, rc);

    cbHyperHeap = RT_ALIGN_32(cbHyperHeap, PAGE_SIZE);
    LogRel(("MM: cbHyperHeap=%#x (%u)\n", cbHyperHeap, cbHyperHeap));

    /*
     * Allocate the hypervisor heap.
     *
     * (This must be done before we start adding memory to the
     * hypervisor static area because lookup records are allocated from it.)
     */
    rc = mmR3HyperHeapCreate(pVM, cbHyperHeap, &pVM->mm.s.pHyperHeapR3, &pVM->mm.s.pHyperHeapR0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Make a small head fence to fend of accidental sequential access.
         */
        MMR3HyperReserveFence(pVM);

        /*
         * Map the VM structure into the hypervisor space.
         * Note! Keeping the mappings here for now in case someone is using
         *       MMHyperR3ToR0 or similar.
         */
        AssertCompileSizeAlignment(VM, PAGE_SIZE);
        AssertCompileSizeAlignment(VMCPU, PAGE_SIZE);
        AssertCompileSizeAlignment(GVM, PAGE_SIZE);
        AssertCompileSizeAlignment(GVMCPU, PAGE_SIZE);
        AssertRelease(pVM->cbSelf == sizeof(VM));
        AssertRelease(pVM->cbVCpu == sizeof(VMCPU));
/** @todo get rid of this (don't dare right now because of
 *        possible MMHyperYYToXX use on the VM structure.) */
        RTGCPTR GCPtr;
        if (SUPR3IsDriverless())
            GCPtr = _1G;
        else
        {
            rc = MMR3HyperMapPages(pVM, pVM, pVM->pVMR0ForCall, sizeof(VM) >> PAGE_SHIFT, pVM->paVMPagesR3, "VM", &GCPtr);
            uint32_t offPages = RT_UOFFSETOF_DYN(GVM, aCpus) >> PAGE_SHIFT; /* (Using the _DYN variant avoids -Winvalid-offset) */
            for (uint32_t idCpu = 0; idCpu < pVM->cCpus && RT_SUCCESS(rc); idCpu++, offPages += sizeof(GVMCPU) >> PAGE_SHIFT)
            {
                PVMCPU pVCpu = pVM->apCpusR3[idCpu];
                RTGCPTR GCPtrIgn;
                rc = MMR3HyperMapPages(pVM, pVCpu, pVM->pVMR0ForCall + offPages * PAGE_SIZE,
                                       sizeof(VMCPU) >> PAGE_SHIFT, &pVM->paVMPagesR3[offPages], "VMCPU", &GCPtrIgn);
            }
        }
        if (RT_SUCCESS(rc))
        {
            pVM->pVMRC = (RTRCPTR)GCPtr;
            for (VMCPUID i = 0; i < pVM->cCpus; i++)
                pVM->apCpusR3[i]->pVMRC = pVM->pVMRC;

            /* Reserve a page for fencing. */
            MMR3HyperReserveFence(pVM);

            /*
             * Map the heap into the hypervisor space.
             */
            rc = mmR3HyperHeapMap(pVM, pVM->mm.s.pHyperHeapR3, &GCPtr);
            if (RT_SUCCESS(rc))
            {
                pVM->mm.s.pHyperHeapRC = (RTRCPTR)GCPtr;
                Assert(pVM->mm.s.pHyperHeapRC == GCPtr);

                /*
                 * Register info handlers.
                 */
                DBGFR3InfoRegisterInternal(pVM, "hma", "Show the layout of the Hypervisor Memory Area.", mmR3HyperInfoHma);

                LogFlow(("mmR3HyperInit: returns VINF_SUCCESS\n"));
                return VINF_SUCCESS;
            }
            /* Caller will do proper cleanup. */
        }
    }

    LogFlow(("mmR3HyperInit: returns %Rrc\n", rc));
    return rc;
}


/**
 * Cleans up the hypervisor heap.
 *
 * @returns VBox status code.
 */
int mmR3HyperTerm(PVM pVM)
{
    if (pVM->mm.s.pHyperHeapR3)
        PDMR3CritSectDelete(pVM, &pVM->mm.s.pHyperHeapR3->Lock);

    return VINF_SUCCESS;
}


/**
 * Finalizes the HMA mapping (obsolete).
 *
 * This is called later during init, most (all) HMA allocations should be done
 * by the time this function is called.
 *
 * @returns VBox status code.
 */
VMMR3DECL(int) MMR3HyperInitFinalize(PVM pVM)
{
    LogFlow(("MMR3HyperInitFinalize:\n"));

    /*
     * Initialize the hyper heap critical section.
     */
    int rc = PDMR3CritSectInit(pVM, &pVM->mm.s.pHyperHeapR3->Lock, RT_SRC_POS, "MM-HYPER");
    AssertRC(rc);

    pVM->mm.s.fPGMInitialized = true;

    LogFlow(("MMR3HyperInitFinalize: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Maps locked R3 virtual memory into the hypervisor region in the GC.
 *
 * @return VBox status code.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pvR3        The ring-3 address of the memory, must be page aligned.
 * @param   pvR0        The ring-0 address of the memory, must be page aligned. (optional)
 * @param   cPages      The number of pages.
 * @param   paPages     The page descriptors.
 * @param   pszDesc     Mapping description.
 * @param   pGCPtr      Where to store the GC address corresponding to pvR3.
 */
static int MMR3HyperMapPages(PVM pVM, void *pvR3, RTR0PTR pvR0, size_t cPages, PCSUPPAGE paPages,
                             const char *pszDesc, PRTGCPTR pGCPtr)
{
    LogFlow(("MMR3HyperMapPages: pvR3=%p pvR0=%p cPages=%zu paPages=%p pszDesc=%p:{%s} pGCPtr=%p\n",
             pvR3, pvR0, cPages, paPages, pszDesc, pszDesc, pGCPtr));

    /*
     * Validate input.
     */
    AssertPtrReturn(pvR3, VERR_INVALID_POINTER);
    AssertPtrReturn(paPages, VERR_INVALID_POINTER);
    AssertReturn(cPages > 0, VERR_PAGE_COUNT_OUT_OF_RANGE);
    AssertReturn(cPages <= VBOX_MAX_ALLOC_PAGE_COUNT, VERR_PAGE_COUNT_OUT_OF_RANGE);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertReturn(*pszDesc, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pGCPtr, VERR_INVALID_PARAMETER);

    /*
     * Add the memory to the hypervisor area.
     */
    RTGCPTR         GCPtr;
    PMMLOOKUPHYPER  pLookup;
    int rc = mmR3HyperMap(pVM, cPages << PAGE_SHIFT, pszDesc, &GCPtr, &pLookup);
    if (RT_SUCCESS(rc))
    {
        /*
         * Copy the physical page addresses and tell PGM about them.
         */
        PRTHCPHYS paHCPhysPages = (PRTHCPHYS)MMR3HeapAlloc(pVM, MM_TAG_MM, sizeof(RTHCPHYS) * cPages);
        if (paHCPhysPages)
        {
            bool const fDriverless = SUPR3IsDriverless();
            for (size_t i = 0; i < cPages; i++)
            {
                AssertReleaseMsgReturn(   (   paPages[i].Phys != 0
                                           && paPages[i].Phys != NIL_RTHCPHYS
                                           && !(paPages[i].Phys & PAGE_OFFSET_MASK))
                                       || fDriverless,
                                       ("i=%#zx Phys=%RHp %s\n", i, paPages[i].Phys, pszDesc),
                                       VERR_INTERNAL_ERROR);
                paHCPhysPages[i] = paPages[i].Phys;
            }

            pLookup->enmType = MMLOOKUPHYPERTYPE_LOCKED;
            pLookup->u.Locked.pvR3          = pvR3;
            pLookup->u.Locked.pvR0          = pvR0;
            pLookup->u.Locked.paHCPhysPages = paHCPhysPages;

            /* done. */
            *pGCPtr   = GCPtr;
            return rc;
        }
        /* Don't care about failure clean, we're screwed if this fails anyway. */
    }

    return rc;
}


/**
 * Reserves an electric fence page.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
static int MMR3HyperReserveFence(PVM pVM)
{
    RT_NOREF(pVM);
    return VINF_SUCCESS;
}


/**
 * Adds memory to the hypervisor memory arena.
 *
 * @return VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   cb          Size of the memory. Will be rounded up to nearest page.
 * @param   pszDesc     The description of the memory.
 * @param   pGCPtr      Where to store the GC address.
 * @param   ppLookup    Where to store the pointer to the lookup record.
 * @remark  We assume the threading structure of VBox imposes natural
 *          serialization of most functions, this one included.
 */
static int mmR3HyperMap(PVM pVM, const size_t cb, const char *pszDesc, PRTGCPTR pGCPtr, PMMLOOKUPHYPER *ppLookup)
{
    /*
     * Validate input.
     */
    const uint32_t cbAligned = RT_ALIGN_32(cb, PAGE_SIZE);
    AssertReturn(cbAligned >= cb, VERR_INVALID_PARAMETER);
    if (pVM->mm.s.offHyperNextStatic + cbAligned >= pVM->mm.s.cbHyperArea) /* don't use the last page, it's a fence. */
    {
        AssertMsgFailed(("Out of static mapping space in the HMA! offHyperAreaGC=%x cbAligned=%x cbHyperArea=%x\n",
                         pVM->mm.s.offHyperNextStatic, cbAligned, pVM->mm.s.cbHyperArea));
        return VERR_NO_MEMORY;
    }

    /*
     * Allocate lookup record.
     */
    PMMLOOKUPHYPER  pLookup;
    int rc = MMHyperAlloc(pVM, sizeof(*pLookup), 1, MM_TAG_MM, (void **)&pLookup);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize it and insert it.
         */
        pLookup->offNext        = pVM->mm.s.offLookupHyper;
        pLookup->cb             = cbAligned;
        pLookup->off            = pVM->mm.s.offHyperNextStatic;
        pVM->mm.s.offLookupHyper = (uint8_t *)pLookup - (uint8_t *)pVM->mm.s.pHyperHeapR3;
        if (pLookup->offNext != (int32_t)NIL_OFFSET)
            pLookup->offNext   -= pVM->mm.s.offLookupHyper;
        pLookup->enmType        = MMLOOKUPHYPERTYPE_INVALID;
        memset(&pLookup->u, 0xff, sizeof(pLookup->u));
        pLookup->pszDesc        = pszDesc;

        /* Mapping. */
        *pGCPtr = pVM->mm.s.pvHyperAreaGC + pVM->mm.s.offHyperNextStatic;
        pVM->mm.s.offHyperNextStatic += cbAligned;

        /* Return pointer. */
        *ppLookup = pLookup;
    }

    AssertRC(rc);
    LogFlow(("mmR3HyperMap: returns %Rrc *pGCPtr=%RGv\n", rc, *pGCPtr));
    return rc;
}


/**
 * Allocates a new heap.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   cb          The size of the new heap.
 * @param   ppHeap      Where to store the heap pointer on successful return.
 * @param   pR0PtrHeap  Where to store the ring-0 address of the heap on
 *                      success.
 */
static int mmR3HyperHeapCreate(PVM pVM, const size_t cb, PMMHYPERHEAP *ppHeap, PRTR0PTR pR0PtrHeap)
{
    /*
     * Allocate the hypervisor heap.
     */
    const uint32_t  cbAligned = RT_ALIGN_32(cb, PAGE_SIZE);
    AssertReturn(cbAligned >= cb, VERR_INVALID_PARAMETER);
    uint32_t const  cPages = cbAligned >> PAGE_SHIFT;
    PSUPPAGE        paPages = (PSUPPAGE)MMR3HeapAlloc(pVM, MM_TAG_MM, cPages * sizeof(paPages[0]));
    if (!paPages)
        return VERR_NO_MEMORY;
    void           *pv;
    RTR0PTR         pvR0 = NIL_RTR0PTR;
    int rc = SUPR3PageAllocEx(cPages,
                              0 /*fFlags*/,
                              &pv,
                              &pvR0,
                              paPages);
    if (RT_SUCCESS(rc))
    {
        Assert((pvR0 != NIL_RTR0PTR && !(PAGE_OFFSET_MASK & pvR0)) || SUPR3IsDriverless());
        memset(pv, 0, cbAligned);

        /*
         * Initialize the heap and first free chunk.
         */
        PMMHYPERHEAP pHeap = (PMMHYPERHEAP)pv;
        pHeap->u32Magic             = MMHYPERHEAP_MAGIC;
        pHeap->pbHeapR3             = (uint8_t *)pHeap + MMYPERHEAP_HDR_SIZE;
        pHeap->pbHeapR0             = pvR0 + MMYPERHEAP_HDR_SIZE;
        //pHeap->pbHeapRC           = 0; // set by mmR3HyperHeapMap()
        pHeap->pVMR3                = pVM;
        pHeap->pVMR0                = pVM->pVMR0ForCall;
        pHeap->pVMRC                = pVM->pVMRC;
        pHeap->cbHeap               = cbAligned - MMYPERHEAP_HDR_SIZE;
        pHeap->cbFree               = pHeap->cbHeap - sizeof(MMHYPERCHUNK);
        //pHeap->offFreeHead        = 0;
        //pHeap->offFreeTail        = 0;
        pHeap->offPageAligned       = pHeap->cbHeap;
        //pHeap->HyperHeapStatTree  = 0;
        pHeap->paPages              = paPages;

        PMMHYPERCHUNKFREE pFree = (PMMHYPERCHUNKFREE)pHeap->pbHeapR3;
        pFree->cb                   = pHeap->cbFree;
        //pFree->core.offNext       = 0;
        MMHYPERCHUNK_SET_TYPE(&pFree->core, MMHYPERCHUNK_FLAGS_FREE);
        pFree->core.offHeap         = -(int32_t)MMYPERHEAP_HDR_SIZE;
        //pFree->offNext            = 0;
        //pFree->offPrev            = 0;

        STAMR3Register(pVM, &pHeap->cbHeap, STAMTYPE_U32, STAMVISIBILITY_ALWAYS, "/MM/HyperHeap/cbHeap",  STAMUNIT_BYTES, "The heap size.");
        STAMR3Register(pVM, &pHeap->cbFree, STAMTYPE_U32, STAMVISIBILITY_ALWAYS, "/MM/HyperHeap/cbFree",  STAMUNIT_BYTES, "The free space.");

        *ppHeap = pHeap;
        *pR0PtrHeap = pvR0;
        return VINF_SUCCESS;
    }
    AssertMsgFailed(("SUPR3PageAllocEx(%d,,,,) -> %Rrc\n", cbAligned >> PAGE_SHIFT, rc));

    *ppHeap = NULL;
    return rc;
}


/**
 * Allocates a new heap.
 */
static int mmR3HyperHeapMap(PVM pVM, PMMHYPERHEAP pHeap, PRTGCPTR ppHeapGC)
{
    Assert(RT_ALIGN_Z(pHeap->cbHeap + MMYPERHEAP_HDR_SIZE, PAGE_SIZE) == pHeap->cbHeap + MMYPERHEAP_HDR_SIZE);
    Assert(pHeap->pbHeapR0);
    Assert(pHeap->paPages);
    int rc = MMR3HyperMapPages(pVM,
                               pHeap,
                               pHeap->pbHeapR0 - MMYPERHEAP_HDR_SIZE,
                               (pHeap->cbHeap + MMYPERHEAP_HDR_SIZE) >> PAGE_SHIFT,
                               pHeap->paPages,
                               "Heap", ppHeapGC);
    if (RT_SUCCESS(rc))
    {
        pHeap->pVMRC    = pVM->pVMRC;
        pHeap->pbHeapRC = *ppHeapGC + MMYPERHEAP_HDR_SIZE;
        /* Reserve a page for fencing. */
        MMR3HyperReserveFence(pVM);

        /* We won't need these any more. */
        MMR3HeapFree(pHeap->paPages);
        pHeap->paPages = NULL;
    }
    return rc;
}


/**
 * Allocates memory in the Hypervisor (GC VMM) area which never will
 * be freed and doesn't have any offset based relation to other heap blocks.
 *
 * The latter means that two blocks allocated by this API will not have the
 * same relative position to each other in GC and HC. In short, never use
 * this API for allocating nodes for an offset based AVL tree!
 *
 * The returned memory is of course zeroed.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   cb          Number of bytes to allocate.
 * @param   uAlignment  Required memory alignment in bytes.
 *                      Values are 0,8,16,32 and PAGE_SIZE.
 *                      0 -> default alignment, i.e. 8 bytes.
 * @param   enmTag      The statistics tag.
 * @param   ppv         Where to store the address to the allocated
 *                      memory.
 * @remark  This is assumed not to be used at times when serialization is required.
 */
VMMR3DECL(int) MMR3HyperAllocOnceNoRel(PVM pVM, size_t cb, unsigned uAlignment, MMTAG enmTag, void **ppv)
{
    return MMR3HyperAllocOnceNoRelEx(pVM, cb, uAlignment, enmTag, 0/*fFlags*/, ppv);
}


/**
 * Allocates memory in the Hypervisor (GC VMM) area which never will
 * be freed and doesn't have any offset based relation to other heap blocks.
 *
 * The latter means that two blocks allocated by this API will not have the
 * same relative position to each other in GC and HC. In short, never use
 * this API for allocating nodes for an offset based AVL tree!
 *
 * The returned memory is of course zeroed.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   cb          Number of bytes to allocate.
 * @param   uAlignment  Required memory alignment in bytes.
 *                      Values are 0,8,16,32 and PAGE_SIZE.
 *                      0 -> default alignment, i.e. 8 bytes.
 * @param   enmTag      The statistics tag.
 * @param   fFlags      Flags, see MMHYPER_AONR_FLAGS_KERNEL_MAPPING.
 * @param   ppv         Where to store the address to the allocated memory.
 * @remark  This is assumed not to be used at times when serialization is required.
 */
VMMR3DECL(int) MMR3HyperAllocOnceNoRelEx(PVM pVM, size_t cb, unsigned uAlignment, MMTAG enmTag, uint32_t fFlags, void **ppv)
{
    AssertMsg(cb >= 8, ("Hey! Do you really mean to allocate less than 8 bytes?! cb=%d\n", cb));
    Assert(!(fFlags & ~(MMHYPER_AONR_FLAGS_KERNEL_MAPPING)));

    /*
     * Choose between allocating a new chunk of HMA memory
     * and the heap. We will only do BIG allocations from HMA and
     * only at creation time.
     */
    if (   (   cb < _64K
            && (   uAlignment != PAGE_SIZE
                || cb < 48*_1K)
            && !(fFlags & MMHYPER_AONR_FLAGS_KERNEL_MAPPING)
           )
        ||  VMR3GetState(pVM) != VMSTATE_CREATING
       )
    {
        Assert(!(fFlags & MMHYPER_AONR_FLAGS_KERNEL_MAPPING));
        int rc = MMHyperAlloc(pVM, cb, uAlignment, enmTag, ppv);
        if (    rc != VERR_MM_HYPER_NO_MEMORY
            ||  cb <= 8*_1K)
        {
            Log2(("MMR3HyperAllocOnceNoRel: cb=%#zx uAlignment=%#x returns %Rrc and *ppv=%p\n",
                  cb, uAlignment, rc, *ppv));
            return rc;
        }
    }

    /*
     * Validate alignment.
     */
    switch (uAlignment)
    {
        case 0:
        case 8:
        case 16:
        case 32:
        case PAGE_SIZE:
            break;
        default:
            AssertMsgFailed(("Invalid alignment %u\n", uAlignment));
            return VERR_INVALID_PARAMETER;
    }

    /*
     * Allocate the pages and map them into HMA space.
     */
    uint32_t const  cbAligned = RT_ALIGN_32(cb, PAGE_SIZE);
    AssertReturn(cbAligned >= cb, VERR_INVALID_PARAMETER);
    uint32_t const  cPages    = cbAligned >> PAGE_SHIFT;
    PSUPPAGE        paPages   = (PSUPPAGE)RTMemTmpAlloc(cPages * sizeof(paPages[0]));
    if (!paPages)
        return VERR_NO_TMP_MEMORY;
    void           *pvPages;
    RTR0PTR         pvR0 = NIL_RTR0PTR;
    int rc = SUPR3PageAllocEx(cPages,
                              0 /*fFlags*/,
                              &pvPages,
                              &pvR0,
                              paPages);
    if (RT_SUCCESS(rc))
    {
        Assert(pvR0 != NIL_RTR0PTR || SUPR3IsDriverless());
        memset(pvPages, 0, cbAligned);

        RTGCPTR GCPtr;
        rc = MMR3HyperMapPages(pVM,
                               pvPages,
                               pvR0,
                               cPages,
                               paPages,
                               MMR3HeapAPrintf(pVM, MM_TAG_MM, "alloc once (%s)", mmGetTagName(enmTag)),
                               &GCPtr);
        /* not needed anymore */
        RTMemTmpFree(paPages);
        if (RT_SUCCESS(rc))
        {
            *ppv = pvPages;
            Log2(("MMR3HyperAllocOnceNoRel: cbAligned=%#x uAlignment=%#x returns VINF_SUCCESS and *ppv=%p\n",
                  cbAligned, uAlignment, *ppv));
            MMR3HyperReserveFence(pVM);
            return rc;
        }
        AssertMsgFailed(("Failed to allocate %zd bytes! %Rrc\n", cbAligned, rc));
        SUPR3PageFreeEx(pvPages, cPages);


        /*
         * HACK ALERT! Try allocate it off the heap so that we don't freak
         * out during vga/vmmdev mmio2 allocation with certain ram sizes.
         */
        /** @todo make a proper fix for this so we will never end up in this kind of situation! */
        Log(("MMR3HyperAllocOnceNoRel: MMR3HyperMapHCRam failed with rc=%Rrc, try MMHyperAlloc(,%#x,,) instead\n",  rc, cb));
        int rc2 = MMHyperAlloc(pVM, cb, uAlignment, enmTag, ppv);
        if (RT_SUCCESS(rc2))
        {
            Log2(("MMR3HyperAllocOnceNoRel: cb=%#x uAlignment=%#x returns %Rrc and *ppv=%p\n",
                  cb, uAlignment, rc, *ppv));
            return rc;
        }
    }
    else
        AssertMsgFailed(("Failed to allocate %zd bytes! %Rrc\n", cbAligned, rc));

    if (rc == VERR_NO_MEMORY)
        rc = VERR_MM_HYPER_NO_MEMORY;
    LogRel(("MMR3HyperAllocOnceNoRel: cb=%#zx uAlignment=%#x returns %Rrc\n", cb, uAlignment, rc));
    return rc;
}


/**
 * Convert hypervisor HC virtual address to HC physical address.
 *
 * @returns HC physical address.
 * @param   pVM         The cross context VM structure.
 * @param   pvR3        Host context virtual address.
 */
VMMR3DECL(RTHCPHYS) MMR3HyperHCVirt2HCPhys(PVM pVM, void *pvR3)
{
    PMMLOOKUPHYPER  pLookup = (PMMLOOKUPHYPER)((uint8_t *)pVM->mm.s.pHyperHeapR3 + pVM->mm.s.offLookupHyper);
    for (;;)
    {
        switch (pLookup->enmType)
        {
            case MMLOOKUPHYPERTYPE_LOCKED:
            {
                unsigned off = (uint8_t *)pvR3 - (uint8_t *)pLookup->u.Locked.pvR3;
                if (off < pLookup->cb)
                    return pLookup->u.Locked.paHCPhysPages[off >> PAGE_SHIFT] | (off & PAGE_OFFSET_MASK);
                break;
            }

            case MMLOOKUPHYPERTYPE_HCPHYS:
            {
                unsigned off = (uint8_t *)pvR3 - (uint8_t *)pLookup->u.HCPhys.pvR3;
                if (off < pLookup->cb)
                    return pLookup->u.HCPhys.HCPhys + off;
                break;
            }

            case MMLOOKUPHYPERTYPE_GCPHYS:
            case MMLOOKUPHYPERTYPE_MMIO2:
            case MMLOOKUPHYPERTYPE_DYNAMIC:
                /* can (or don't want to) convert these kind of records. */
                break;

            default:
                AssertMsgFailed(("enmType=%d\n", pLookup->enmType));
                break;
        }

        /* next */
        if ((unsigned)pLookup->offNext == NIL_OFFSET)
            break;
        pLookup = (PMMLOOKUPHYPER)((uint8_t *)pLookup + pLookup->offNext);
    }

    AssertMsgFailed(("pvR3=%p is not inside the hypervisor memory area!\n", pvR3));
    return NIL_RTHCPHYS;
}


/**
 * Info handler for 'hma', it dumps the list of lookup records for the hypervisor memory area.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) mmR3HyperInfoHma(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);

    pHlp->pfnPrintf(pHlp, "Hypervisor Memory Area (HMA) Layout: Base %RGv, 0x%08x bytes\n",
                    pVM->mm.s.pvHyperAreaGC, pVM->mm.s.cbHyperArea);

    PMMLOOKUPHYPER  pLookup = (PMMLOOKUPHYPER)((uint8_t *)pVM->mm.s.pHyperHeapR3 + pVM->mm.s.offLookupHyper);
    for (;;)
    {
        switch (pLookup->enmType)
        {
            case MMLOOKUPHYPERTYPE_LOCKED:
                pHlp->pfnPrintf(pHlp, "%RGv-%RGv %RHv %RHv LOCKED  %-*s %s\n",
                                pLookup->off + pVM->mm.s.pvHyperAreaGC,
                                pLookup->off + pVM->mm.s.pvHyperAreaGC + pLookup->cb,
                                pLookup->u.Locked.pvR3,
                                pLookup->u.Locked.pvR0,
                                sizeof(RTHCPTR) * 2, "",
                                pLookup->pszDesc);
                break;

            case MMLOOKUPHYPERTYPE_HCPHYS:
                pHlp->pfnPrintf(pHlp, "%RGv-%RGv %RHv %RHv HCPHYS  %RHp %s\n",
                                pLookup->off + pVM->mm.s.pvHyperAreaGC,
                                pLookup->off + pVM->mm.s.pvHyperAreaGC + pLookup->cb,
                                pLookup->u.HCPhys.pvR3,
                                pLookup->u.HCPhys.pvR0,
                                pLookup->u.HCPhys.HCPhys,
                                pLookup->pszDesc);
                break;

            case MMLOOKUPHYPERTYPE_GCPHYS:
                pHlp->pfnPrintf(pHlp, "%RGv-%RGv %*s GCPHYS  %RGp%*s %s\n",
                                pLookup->off + pVM->mm.s.pvHyperAreaGC,
                                pLookup->off + pVM->mm.s.pvHyperAreaGC + pLookup->cb,
                                sizeof(RTHCPTR) * 2 * 2 + 1, "",
                                pLookup->u.GCPhys.GCPhys, RT_ABS((int)(sizeof(RTHCPHYS) - sizeof(RTGCPHYS))) * 2, "",
                                pLookup->pszDesc);
                break;

            case MMLOOKUPHYPERTYPE_MMIO2:
                pHlp->pfnPrintf(pHlp, "%RGv-%RGv %*s MMIO2   %RGp%*s %s\n",
                                pLookup->off + pVM->mm.s.pvHyperAreaGC,
                                pLookup->off + pVM->mm.s.pvHyperAreaGC + pLookup->cb,
                                sizeof(RTHCPTR) * 2 * 2 + 1, "",
                                pLookup->u.MMIO2.off, RT_ABS((int)(sizeof(RTHCPHYS) - sizeof(RTGCPHYS))) * 2, "",
                                pLookup->pszDesc);
                break;

            case MMLOOKUPHYPERTYPE_DYNAMIC:
                pHlp->pfnPrintf(pHlp, "%RGv-%RGv %*s DYNAMIC %*s %s\n",
                                pLookup->off + pVM->mm.s.pvHyperAreaGC,
                                pLookup->off + pVM->mm.s.pvHyperAreaGC + pLookup->cb,
                                sizeof(RTHCPTR) * 2 * 2 + 1, "",
                                sizeof(RTHCPTR) * 2, "",
                                pLookup->pszDesc);
                break;

            default:
                AssertMsgFailed(("enmType=%d\n", pLookup->enmType));
                break;
        }

        /* next */
        if ((unsigned)pLookup->offNext == NIL_OFFSET)
            break;
        pLookup = (PMMLOOKUPHYPER)((uint8_t *)pLookup + pLookup->offNext);
    }
}


#if 0
/**
 * Re-allocates memory from the hyper heap.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pvOld           The existing block of memory in the hyper heap to
 *                          re-allocate (can be NULL).
 * @param   cbOld           Size of the existing block.
 * @param   uAlignmentNew   Required memory alignment in bytes. Values are
 *                          0,8,16,32 and PAGE_SIZE. 0 -> default alignment,
 *                          i.e. 8 bytes.
 * @param   enmTagNew       The statistics tag.
 * @param   cbNew           The required size of the new block.
 * @param   ppv             Where to store the address to the re-allocated
 *                          block.
 *
 * @remarks This does not work like normal realloc() on failure, the memory
 *          pointed to by @a pvOld is lost if there isn't sufficient space on
 *          the hyper heap for the re-allocation to succeed.
*/
VMMR3DECL(int) MMR3HyperRealloc(PVM pVM, void *pvOld, size_t cbOld, unsigned uAlignmentNew, MMTAG enmTagNew, size_t cbNew,
                                void **ppv)
{
    if (!pvOld)
        return MMHyperAlloc(pVM, cbNew, uAlignmentNew, enmTagNew, ppv);

    if (!cbNew && pvOld)
        return MMHyperFree(pVM, pvOld);

    if (cbOld == cbNew)
        return VINF_SUCCESS;

    size_t cbData = RT_MIN(cbNew, cbOld);
    void *pvTmp = RTMemTmpAlloc(cbData);
    if (RT_UNLIKELY(!pvTmp))
    {
        MMHyperFree(pVM, pvOld);
        return VERR_NO_TMP_MEMORY;
    }
    memcpy(pvTmp, pvOld, cbData);

    int rc = MMHyperFree(pVM, pvOld);
    if (RT_SUCCESS(rc))
    {
        rc = MMHyperAlloc(pVM, cbNew, uAlignmentNew, enmTagNew, ppv);
        if (RT_SUCCESS(rc))
        {
            Assert(cbData <= cbNew);
            memcpy(*ppv, pvTmp, cbData);
        }
    }
    else
        AssertMsgFailed(("Failed to free hyper heap block pvOld=%p cbOld=%u\n", pvOld, cbOld));

    RTMemTmpFree(pvTmp);
    return rc;
}
#endif

