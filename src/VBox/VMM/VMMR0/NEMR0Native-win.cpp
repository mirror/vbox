/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-0 Windows backend.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_NEM
#include <iprt/nt/hyperv.h>

#include <VBox/vmm/nem.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/apic.h>
#include "NEMInternal.h"
#include <VBox/vmm/gvm.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/gvmm.h>
#include <VBox/param.h>

#include <iprt/dbg.h>
#include <iprt/memobj.h>
#include <iprt/string.h>


/* Assert compile context sanity. */
#ifndef RT_OS_WINDOWS
# error "Windows only file!"
#endif
#ifndef RT_ARCH_AMD64
# error "AMD64 only file!"
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static uint64_t (* g_pfnHvlInvokeHypercall)(uint64_t uCallInfo, uint64_t GCPhysInput, uint64_t GCPhysOutput);



/**
 * Called by NEMR3Init to make sure we've got what we need.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM handle.
 * @param   pVM             The cross context VM handle.
 * @thread  EMT(0)
 */
VMMR0_INT_DECL(int) NEMR0InitVM(PGVM pGVM, PVM pVM)
{
    int rc = GVMMR0ValidateGVMandVMandEMT(pGVM, pVM, 0);
    AssertRCReturn(rc, rc);

    /*
     * We want to perform hypercalls here.  The NT kernel started to expose a very low
     * level interface to do this thru somewhere between build 14271 and 16299.  Since
     * we need build 17083 to get anywhere at all, the exact build is not relevant here.
     */
    RTDBGKRNLINFO hKrnlInfo;
    rc = RTR0DbgKrnlInfoOpen(&hKrnlInfo, 0);
    if (RT_SUCCESS(rc))
    {
        rc = RTR0DbgKrnlInfoQuerySymbol(hKrnlInfo, NULL, "HvlInvokeHypercall", (void **)&g_pfnHvlInvokeHypercall);
        RTR0DbgKrnlInfoRelease(hKrnlInfo);
        if (RT_SUCCESS(rc))
        {
            /*
             * Allocate a page for each VCPU to place hypercall data on.
             */
            for (VMCPUID i = 0; i < pGVM->cCpus; i++)
            {
                PGVMCPU pGVCpu = &pGVM->aCpus[i];
                rc = RTR0MemObjAllocPage(&pGVCpu->nem.s.hHypercallDataMemObj, PAGE_SIZE, false /*fExecutable*/);
                if (RT_SUCCESS(rc))
                {
                    pGVCpu->nem.s.HCPhysHypercallData = RTR0MemObjGetPagePhysAddr(pGVCpu->nem.s.hHypercallDataMemObj, 0 /*iPage*/);
                    pGVCpu->nem.s.pbHypercallData     = (uint8_t *)RTR0MemObjAddress(pGVCpu->nem.s.hHypercallDataMemObj);
                    AssertStmt(pGVCpu->nem.s.HCPhysHypercallData != NIL_RTHCPHYS, rc = VERR_INTERNAL_ERROR_3);
                    AssertStmt(pGVCpu->nem.s.pbHypercallData, rc = VERR_INTERNAL_ERROR_3);
                }
                else
                    pGVCpu->nem.s.hHypercallDataMemObj = NIL_RTR0MEMOBJ;
                if (RT_FAILURE(rc))
                {
                    /* bail. */
                    do
                    {
                        RTR0MemObjFree(pGVCpu->nem.s.hHypercallDataMemObj, true /*fFreeMappings*/);
                        pGVCpu->nem.s.hHypercallDataMemObj = NIL_RTR0MEMOBJ;
                        pGVCpu->nem.s.HCPhysHypercallData  = NIL_RTHCPHYS;
                        pGVCpu->nem.s.pbHypercallData      = NULL;
                    } while (i-- > 0);
                    return rc;
                }
            }
            /*
             * So far, so good.
             */
            /** @todo would be good if we could establish the partition ID ourselves. */
            /** @todop this is too EARLY!   */
            pGVM->nem.s.idHvPartition = pVM->nem.s.idHvPartition;
            return rc;
        }

        rc = VERR_NEM_MISSING_KERNEL_API;
    }

    RT_NOREF(pGVM, pVM);
    return rc;
}


/**
 * Cleanup the NEM parts of the VM in ring-0.
 *
 * This is always called and must deal the state regardless of whether
 * NEMR0InitVM() was called or not.  So, take care here.
 *
 * @param   pGVM            The ring-0 VM handle.
 */
VMMR0_INT_DECL(void) NEMR0CleanupVM(PGVM pGVM)
{
    pGVM->nem.s.idHvPartition = HV_PARTITION_ID_INVALID;

    /* Free the hypercall pages. */
    VMCPUID i = pGVM->cCpus;
    while (i-- > 0)
    {
        PGVMCPU pGVCpu = &pGVM->aCpus[i];
        if (pGVCpu->nem.s.pbHypercallData)
        {
            pGVCpu->nem.s.pbHypercallData = NULL;
            int rc = RTR0MemObjFree(pGVCpu->nem.s.hHypercallDataMemObj, true /*fFreeMappings*/);
            AssertRC(rc);
        }
        pGVCpu->nem.s.hHypercallDataMemObj = NIL_RTR0MEMOBJ;
        pGVCpu->nem.s.HCPhysHypercallData  = NIL_RTHCPHYS;
    }
}


/**
 * Maps pages into the guest physical address space.
 *
 * Generally the caller will be under the PGM lock already, so no extra effort
 * is needed to make sure all changes happens under it.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM handle.
 * @param   pVM             The cross context VM handle.
 * @param   idCpu           The calling EMT.  Necessary for getting the
 *                          hypercall page and arguments.
 * @thread  EMT(idCpu)
 */
VMMR0_INT_DECL(int) NEMR0MapPages(PGVM pGVM, PVM pVM, VMCPUID idCpu)
{
    /*
     * Validate the call.
     */
    int rc = GVMMR0ValidateGVMandVMandEMT(pGVM, pVM, idCpu);
    if (RT_SUCCESS(rc))
    {
        PVMCPU  pVCpu  = &pVM->aCpus[idCpu];
        PGVMCPU pGVCpu = &pGVM->aCpus[idCpu];
        AssertReturn(g_pfnHvlInvokeHypercall, VERR_NEM_MISSING_KERNEL_API);

        RTGCPHYS                GCPhysSrc = pVCpu->nem.s.Hypercall.MapPages.GCPhysSrc;
        RTGCPHYS const          GCPhysDst = pVCpu->nem.s.Hypercall.MapPages.GCPhysDst;
        uint32_t const          cPages    = pVCpu->nem.s.Hypercall.MapPages.cPages;
        HV_MAP_GPA_FLAGS const  fFlags    = pVCpu->nem.s.Hypercall.MapPages.fFlags;

        AssertReturn(cPages > 0, VERR_OUT_OF_RANGE);
        AssertReturn(cPages <= NEM_MAX_MAP_PAGES, VERR_OUT_OF_RANGE);
        AssertReturn(!(fFlags & ~(HV_MAP_GPA_MAYBE_ACCESS_MASK & ~HV_MAP_GPA_DUNNO_ACCESS)), VERR_INVALID_FLAGS);
        AssertMsgReturn(!(GCPhysDst & X86_PAGE_OFFSET_MASK), ("GCPhysDst=%RGp\n", GCPhysDst), VERR_OUT_OF_RANGE);
        AssertReturn(GCPhysDst < _1E, VERR_OUT_OF_RANGE);
        if (GCPhysSrc != GCPhysDst)
        {
            AssertMsgReturn(!(GCPhysSrc & X86_PAGE_OFFSET_MASK), ("GCPhysSrc=%RGp\n", GCPhysSrc), VERR_OUT_OF_RANGE);
            AssertReturn(GCPhysSrc < _1E, VERR_OUT_OF_RANGE);
        }

        /** @todo fix pGVM->nem.s.idHvPartition init. */
        if (pGVM->nem.s.idHvPartition == 0)
            pGVM->nem.s.idHvPartition = pVM->nem.s.idHvPartition;

        /*
         * Compose and make the hypercall.
         * Ring-3 is not allowed to fill in the host physical addresses of the call.
         */
        HV_INPUT_MAP_GPA_PAGES *pMapPages = (HV_INPUT_MAP_GPA_PAGES *)pGVCpu->nem.s.pbHypercallData;
        AssertPtrReturn(pMapPages, VERR_INTERNAL_ERROR_3);
        pMapPages->TargetPartitionId    = pGVM->nem.s.idHvPartition;
        pMapPages->TargetGpaBase        = GCPhysDst >> X86_PAGE_SHIFT;
        pMapPages->MapFlags             = fFlags;
        pMapPages->u32ExplicitPadding   = 0;
        for (uint32_t iPage = 0; iPage < cPages; iPage++, GCPhysSrc += X86_PAGE_SIZE)
        {
            RTHCPHYS HCPhys = NIL_RTGCPHYS;
            rc = PGMPhysGCPhys2HCPhys(pVM, GCPhysSrc, &HCPhys);
            AssertRCBreak(rc);
            pMapPages->PageList[iPage] = HCPhys >> X86_PAGE_SHIFT;
        }
        if (RT_SUCCESS(rc))
        {
            uint64_t uResult = g_pfnHvlInvokeHypercall(HvCallMapGpaPages | ((uint64_t)cPages << 32),
                                                       pGVCpu->nem.s.HCPhysHypercallData, 0);
            Log6(("NEMR0MapPages: %RGp/%RGp L %u prot %#x -> %#RX64\n",
                  GCPhysDst, GCPhysSrc - cPages * X86_PAGE_SIZE, cPages, fFlags, uResult));
            if (uResult == ((uint64_t)cPages << 32))
                rc = VINF_SUCCESS;
            else
            {
                rc = VERR_NEM_MAP_PAGES_FAILED;
                LogRel(("g_pfnHvlInvokeHypercall/MapGpaPages -> %#RX64\n", uResult));
            }
        }
    }
    return rc;
}


#if 0 /* for debugging GPA unmapping.  */
static int nemR3WinDummyReadGpa(PGVM pGVM, PGVMCPU pGVCpu, RTGCPHYS GCPhys)
{
    PHV_INPUT_READ_GPA  pIn  = (PHV_INPUT_READ_GPA)pGVCpu->nem.s.pbHypercallData;
    PHV_OUTPUT_READ_GPA pOut = (PHV_OUTPUT_READ_GPA)(pIn + 1);
    pIn->PartitionId            = pGVM->nem.s.idHvPartition;
    pIn->VpIndex                = pGVCpu->idCpu;
    pIn->ByteCount              = 0x10;
    pIn->BaseGpa                = GCPhys;
    pIn->ControlFlags.AsUINT64  = 0;
    pIn->ControlFlags.CacheType = HvCacheTypeX64WriteCombining;
    memset(pOut, 0xfe, sizeof(*pOut));
    uint64_t volatile uResult = g_pfnHvlInvokeHypercall(HvCallReadGpa, pGVCpu->nem.s.HCPhysHypercallData,
                                                        pGVCpu->nem.s.HCPhysHypercallData + sizeof(*pIn));
    LogRel(("nemR3WinDummyReadGpa: %RGp -> %#RX64; code=%u rsvd=%u abData=%.16Rhxs\n",
            GCPhys, uResult, pOut->AccessResult.ResultCode, pOut->AccessResult.Reserved, pOut->Data));
    __debugbreak();

    return uResult != 0 ? VERR_READ_ERROR : VINF_SUCCESS;
}
#endif


/**
 * Unmaps pages from the guest physical address space.
 *
 * Generally the caller will be under the PGM lock already, so no extra effort
 * is needed to make sure all changes happens under it.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM handle.
 * @param   pVM             The cross context VM handle.
 * @param   idCpu           The calling EMT.  Necessary for getting the
 *                          hypercall page and arguments.
 * @thread  EMT(idCpu)
 */
VMMR0_INT_DECL(int) NEMR0UnmapPages(PGVM pGVM, PVM pVM, VMCPUID idCpu)
{
    /*
     * Validate the call.
     */
    int rc = GVMMR0ValidateGVMandVMandEMT(pGVM, pVM, idCpu);
    if (RT_SUCCESS(rc))
    {
        PVMCPU  pVCpu  = &pVM->aCpus[idCpu];
        PGVMCPU pGVCpu = &pGVM->aCpus[idCpu];
        AssertReturn(g_pfnHvlInvokeHypercall, VERR_NEM_MISSING_KERNEL_API);

        RTGCPHYS                GCPhys = pVCpu->nem.s.Hypercall.UnmapPages.GCPhys;
        uint32_t const          cPages = pVCpu->nem.s.Hypercall.UnmapPages.cPages;

        AssertReturn(cPages > 0, VERR_OUT_OF_RANGE);
        AssertReturn(cPages <= NEM_MAX_UNMAP_PAGES, VERR_OUT_OF_RANGE);
        AssertMsgReturn(!(GCPhys & X86_PAGE_OFFSET_MASK), ("%RGp\n", GCPhys), VERR_OUT_OF_RANGE);
        AssertReturn(GCPhys < _1E, VERR_OUT_OF_RANGE);

        /** @todo fix pGVM->nem.s.idHvPartition init. */
        if (pGVM->nem.s.idHvPartition == 0)
            pGVM->nem.s.idHvPartition = pVM->nem.s.idHvPartition;

        /*
         * Compose and make the hypercall.
         */
        HV_INPUT_UNMAP_GPA_PAGES *pUnmapPages = (HV_INPUT_UNMAP_GPA_PAGES *)pGVCpu->nem.s.pbHypercallData;
        AssertPtrReturn(pUnmapPages, VERR_INTERNAL_ERROR_3);
        pUnmapPages->TargetPartitionId    = pGVM->nem.s.idHvPartition;
        pUnmapPages->TargetGpaBase        = GCPhys >> X86_PAGE_SHIFT;
        pUnmapPages->fFlags               = 0;

        uint64_t uResult = g_pfnHvlInvokeHypercall(HvCallUnmapGpaPages | ((uint64_t)cPages << 32),
                                                   pGVCpu->nem.s.HCPhysHypercallData, 0);
        Log6(("NEMR0UnmapPages: %RGp L %u -> %#RX64\n", GCPhys, cPages, uResult));
        if (uResult == ((uint64_t)cPages << 32))
        {
#if 1       /* Do we need to do this? Hopefully not... */
            uint64_t volatile uR = g_pfnHvlInvokeHypercall(HvCallUncommitGpaPages | ((uint64_t)cPages << 32),
                                                           pGVCpu->nem.s.HCPhysHypercallData, 0);
            AssertMsg(uR == ((uint64_t)cPages << 32), ("uR=%#RX64\n", uR));
#endif
            rc = VINF_SUCCESS;
        }
        else
        {
            rc = VERR_NEM_UNMAP_PAGES_FAILED;
            LogRel(("g_pfnHvlInvokeHypercall/UnmapGpaPages -> %#RX64\n", uResult));
        }
    }
    return rc;
}


