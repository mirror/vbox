/* $Id$ */
/** @file
 * PGM Shadow Page Pool.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM_POOL
#include <VBox/pgm.h>
#include <VBox/mm.h>
#include <VBox/em.h>
#include <VBox/cpum.h>
#ifdef IN_RC
# include <VBox/patm.h>
#endif
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <VBox/disopcode.h>
#include <VBox/hwacc_vmx.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/string.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
static void pgmPoolFlushAllInt(PPGMPOOL pPool);
#ifdef PGMPOOL_WITH_USER_TRACKING
DECLINLINE(unsigned) pgmPoolTrackGetShadowEntrySize(PGMPOOLKIND enmKind);
DECLINLINE(unsigned) pgmPoolTrackGetGuestEntrySize(PGMPOOLKIND enmKind);
static void pgmPoolTrackDeref(PPGMPOOL pPool, PPGMPOOLPAGE pPage);
#endif
#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
static void pgmPoolTracDerefGCPhysHint(PPGMPOOL pPool, PPGMPOOLPAGE pPage, RTHCPHYS HCPhys, RTGCPHYS GCPhysHint);
#endif
#ifdef PGMPOOL_WITH_CACHE
static int pgmPoolTrackAddUser(PPGMPOOL pPool, PPGMPOOLPAGE pPage, uint16_t iUser, uint32_t iUserTable);
#endif
#ifdef PGMPOOL_WITH_MONITORING
static void pgmPoolMonitorModifiedRemove(PPGMPOOL pPool, PPGMPOOLPAGE pPage);
#endif
#ifndef IN_RING3
DECLEXPORT(int) pgmPoolAccessHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser);
#endif
#ifdef LOG_ENABLED
static const char *pgmPoolPoolKindToStr(uint8_t enmKind);
#endif
__END_DECLS


/**
 * Checks if the specified page pool kind is for a 4MB or 2MB guest page.
 *
 * @returns true if it's the shadow of a 4MB or 2MB guest page, otherwise false.
 * @param   enmKind     The page kind.
 */
DECLINLINE(bool) pgmPoolIsBigPage(PGMPOOLKIND enmKind)
{
    switch (enmKind)
    {
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
            return true;
        default:
            return false;
    }
}

/** @def PGMPOOL_PAGE_2_LOCKED_PTR
 * Maps a pool page pool into the current context and lock it (RC only).
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 * @param   pPage   The pool page.
 *
 * @remark  In RC this uses PGMGCDynMapHCPage(), so it will consume of the
 *          small page window employeed by that function. Be careful.
 * @remark  There is no need to assert on the result.
 */
#if defined(IN_RC)
DECLINLINE(void *) PGMPOOL_PAGE_2_LOCKED_PTR(PVM pVM, PPGMPOOLPAGE pPage)
{
    void *pv = pgmPoolMapPageInlined(&pVM->pgm.s, pPage);

    /* Make sure the dynamic mapping will not be reused. */
    if (pv)
        PGMDynLockHCPage(pVM, (uint8_t *)pv);

    return pv;
}
#else
# define PGMPOOL_PAGE_2_LOCKED_PTR(pVM, pPage)  PGMPOOL_PAGE_2_PTR(pVM, pPage)
#endif

/** @def PGMPOOL_UNLOCK_PTR
 * Unlock a previously locked dynamic caching (RC only).
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 * @param   pPage   The pool page.
 *
 * @remark  In RC this uses PGMGCDynMapHCPage(), so it will consume of the
 *          small page window employeed by that function. Be careful.
 * @remark  There is no need to assert on the result.
 */
#if defined(IN_RC)
DECLINLINE(void) PGMPOOL_UNLOCK_PTR(PVM pVM, void *pvPage)
{
    if (pvPage)
        PGMDynUnlockHCPage(pVM, (uint8_t *)pvPage);
}
#else
# define PGMPOOL_UNLOCK_PTR(pVM, pPage)  do {} while (0)
#endif


#ifdef PGMPOOL_WITH_MONITORING
/**
 * Determin the size of a write instruction.
 * @returns number of bytes written.
 * @param   pDis        The disassembler state.
 */
static unsigned pgmPoolDisasWriteSize(PDISCPUSTATE pDis)
{
    /*
     * This is very crude and possibly wrong for some opcodes,
     * but since it's not really supposed to be called we can
     * probably live with that.
     */
    return DISGetParamSize(pDis, &pDis->param1);
}


/**
 * Flushes a chain of pages sharing the same access monitor.
 *
 * @returns VBox status code suitable for scheduling.
 * @param   pPool       The pool.
 * @param   pPage       A page in the chain.
 */
int pgmPoolMonitorChainFlush(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    LogFlow(("pgmPoolMonitorChainFlush: Flush page %RGp type=%d\n", pPage->GCPhys, pPage->enmKind));

    /*
     * Find the list head.
     */
    uint16_t idx = pPage->idx;
    if (pPage->iMonitoredPrev != NIL_PGMPOOL_IDX)
    {
        while (pPage->iMonitoredPrev != NIL_PGMPOOL_IDX)
        {
            idx = pPage->iMonitoredPrev;
            Assert(idx != pPage->idx);
            pPage = &pPool->aPages[idx];
        }
    }

    /*
     * Iterate the list flushing each shadow page.
     */
    int rc = VINF_SUCCESS;
    for (;;)
    {
        idx = pPage->iMonitoredNext;
        Assert(idx != pPage->idx);
        if (pPage->idx >= PGMPOOL_IDX_FIRST)
        {
            int rc2 = pgmPoolFlushPage(pPool, pPage);
            AssertRC(rc2);
        }
        /* next */
        if (idx == NIL_PGMPOOL_IDX)
            break;
        pPage = &pPool->aPages[idx];
    }
    return rc;
}


/**
 * Wrapper for getting the current context pointer to the entry being modified.
 *
 * @returns VBox status code suitable for scheduling.
 * @param   pVM         VM Handle.
 * @param   pvDst       Destination address
 * @param   pvSrc       Source guest virtual address.
 * @param   GCPhysSrc   The source guest physical address.
 * @param   cb          Size of data to read
 */
DECLINLINE(int) pgmPoolPhysSimpleReadGCPhys(PVM pVM, void *pvDst, CTXTYPE(RTGCPTR, RTHCPTR, RTGCPTR) pvSrc, RTGCPHYS GCPhysSrc, size_t cb)
{
#if defined(IN_RING3)
    memcpy(pvDst, (RTHCPTR)((uintptr_t)pvSrc & ~(RTHCUINTPTR)(cb - 1)), cb);
    return VINF_SUCCESS;
#else
    /* @todo in RC we could attempt to use the virtual address, although this can cause many faults (PAE Windows XP guest). */
    return PGMPhysSimpleReadGCPhys(pVM, pvDst, GCPhysSrc & ~(RTGCPHYS)(cb - 1), cb);
#endif
}

/**
 * Process shadow entries before they are changed by the guest.
 *
 * For PT entries we will clear them. For PD entries, we'll simply check
 * for mapping conflicts and set the SyncCR3 FF if found.
 *
 * @param   pVCpu       VMCPU handle
 * @param   pPool       The pool.
 * @param   pPage       The head page.
 * @param   GCPhysFault The guest physical fault address.
 * @param   uAddress    In R0 and GC this is the guest context fault address (flat).
 *                      In R3 this is the host context 'fault' address.
 * @param   pCpu        The disassembler state for figuring out the write size.
 *                      This need not be specified if the caller knows we won't do cross entry accesses.
 */
void pgmPoolMonitorChainChanging(PVMCPU pVCpu, PPGMPOOL pPool, PPGMPOOLPAGE pPage, RTGCPHYS GCPhysFault, CTXTYPE(RTGCPTR, RTHCPTR, RTGCPTR) pvAddress, PDISCPUSTATE pCpu)
{
    Assert(pPage->iMonitoredPrev == NIL_PGMPOOL_IDX);
    const unsigned off     = GCPhysFault & PAGE_OFFSET_MASK;
    const unsigned cbWrite = (pCpu) ? pgmPoolDisasWriteSize(pCpu) : 0;
    PVM pVM = pPool->CTX_SUFF(pVM);

    LogFlow(("pgmPoolMonitorChainChanging: %RGv phys=%RGp kind=%s cbWrite=%d\n", (RTGCPTR)pvAddress, GCPhysFault, pgmPoolPoolKindToStr(pPage->enmKind), cbWrite));
    for (;;)
    {
       union
        {
            void       *pv;
            PX86PT      pPT;
            PX86PTPAE   pPTPae;
            PX86PD      pPD;
            PX86PDPAE   pPDPae;
            PX86PDPT    pPDPT;
            PX86PML4    pPML4;
        } uShw;

        uShw.pv = NULL;
        switch (pPage->enmKind)
        {
            case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
            {
                uShw.pv = PGMPOOL_PAGE_2_LOCKED_PTR(pVM, pPage);
                const unsigned iShw = off / sizeof(X86PTE);
                LogFlow(("PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT iShw=%x\n", iShw));
                if (uShw.pPT->a[iShw].n.u1Present)
                {
#  ifdef PGMPOOL_WITH_GCPHYS_TRACKING
                    X86PTE GstPte;

                    int rc = pgmPoolPhysSimpleReadGCPhys(pVM, &GstPte, pvAddress, GCPhysFault, sizeof(GstPte));
                    AssertRC(rc);
                    Log4(("pgmPoolMonitorChainChanging 32_32: deref %016RX64 GCPhys %08RX32\n", uShw.pPT->a[iShw].u & X86_PTE_PAE_PG_MASK, GstPte.u & X86_PTE_PG_MASK));
                    pgmPoolTracDerefGCPhysHint(pPool, pPage,
                                               uShw.pPT->a[iShw].u & X86_PTE_PAE_PG_MASK,
                                               GstPte.u & X86_PTE_PG_MASK);
#  endif
                    uShw.pPT->a[iShw].u = 0;
                }
                break;
            }

            /* page/2 sized */
            case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
            {
                uShw.pv = PGMPOOL_PAGE_2_LOCKED_PTR(pVM, pPage);
                if (!((off ^ pPage->GCPhys) & (PAGE_SIZE / 2)))
                {
                    const unsigned iShw = (off / sizeof(X86PTE)) & (X86_PG_PAE_ENTRIES - 1);
                    LogFlow(("PGMPOOLKIND_PAE_PT_FOR_32BIT_PT iShw=%x\n", iShw));
                    if (uShw.pPTPae->a[iShw].n.u1Present)
                    {
#  ifdef PGMPOOL_WITH_GCPHYS_TRACKING
                        X86PTE GstPte;
                        int rc = pgmPoolPhysSimpleReadGCPhys(pVM, &GstPte, pvAddress, GCPhysFault, sizeof(GstPte));
                        AssertRC(rc);

                        Log4(("pgmPoolMonitorChainChanging pae_32: deref %016RX64 GCPhys %08RX32\n", uShw.pPT->a[iShw].u & X86_PTE_PAE_PG_MASK, GstPte.u & X86_PTE_PG_MASK));
                        pgmPoolTracDerefGCPhysHint(pPool, pPage,
                                                   uShw.pPTPae->a[iShw].u & X86_PTE_PAE_PG_MASK,
                                                   GstPte.u & X86_PTE_PG_MASK);
#  endif
                        uShw.pPTPae->a[iShw].u = 0;
                    }
                }
                break;
            }

            case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
            case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
            case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
            case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
            {
                unsigned iGst     = off / sizeof(X86PDE);
                unsigned iShwPdpt = iGst / 256;
                unsigned iShw     = (iGst % 256) * 2;
                uShw.pv = PGMPOOL_PAGE_2_LOCKED_PTR(pVM, pPage);

                LogFlow(("pgmPoolMonitorChainChanging PAE for 32 bits: iGst=%x iShw=%x idx = %d page idx=%d\n", iGst, iShw, iShwPdpt, pPage->enmKind - PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD));
                if (iShwPdpt == pPage->enmKind - (unsigned)PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD)
                {
                    for (unsigned i = 0; i < 2; i++)
                    {
#  ifndef IN_RING0
                        if ((uShw.pPDPae->a[iShw + i].u & (PGM_PDFLAGS_MAPPING | X86_PDE_P)) == (PGM_PDFLAGS_MAPPING | X86_PDE_P))
                        {
                            Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
                            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                            LogFlow(("pgmPoolMonitorChainChanging: Detected conflict at iShwPdpt=%#x iShw=%#x!\n", iShwPdpt, iShw+i));
                            break;
                        }
                        else
#  endif /* !IN_RING0 */
                        if (uShw.pPDPae->a[iShw+i].n.u1Present)
                        {
                            LogFlow(("pgmPoolMonitorChainChanging: pae pd iShw=%#x: %RX64 -> freeing it!\n", iShw+i, uShw.pPDPae->a[iShw+i].u));
                            pgmPoolFree(pVM,
                                        uShw.pPDPae->a[iShw+i].u & X86_PDE_PAE_PG_MASK,
                                        pPage->idx,
                                        iShw + i);
                            uShw.pPDPae->a[iShw+i].u = 0;
                        }

                        /* paranoia / a bit assumptive. */
                        if (   pCpu
                            && (off & 3)
                            && (off & 3) + cbWrite > 4)
                        {
                            const unsigned iShw2 = iShw + 2 + i;
                            if (iShw2 < RT_ELEMENTS(uShw.pPDPae->a))
                            {
#  ifndef IN_RING0
                                if ((uShw.pPDPae->a[iShw2].u & (PGM_PDFLAGS_MAPPING | X86_PDE_P)) == (PGM_PDFLAGS_MAPPING | X86_PDE_P))
                                {
                                    Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
                                    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                                    LogFlow(("pgmPoolMonitorChainChanging: Detected conflict at iShwPdpt=%#x iShw2=%#x!\n", iShwPdpt, iShw2));
                                    break;
                                }
                                else
#  endif /* !IN_RING0 */
                                if (uShw.pPDPae->a[iShw2].n.u1Present)
                                {
                                    LogFlow(("pgmPoolMonitorChainChanging: pae pd iShw=%#x: %RX64 -> freeing it!\n", iShw2, uShw.pPDPae->a[iShw2].u));
                                    pgmPoolFree(pVM,
                                                uShw.pPDPae->a[iShw2].u & X86_PDE_PAE_PG_MASK,
                                                pPage->idx,
                                                iShw2);
                                    uShw.pPDPae->a[iShw2].u = 0;
                                }
                            }
                        }
                    }
                }
                break;
            }

            case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
            {
                uShw.pv = PGMPOOL_PAGE_2_LOCKED_PTR(pVM, pPage);
                const unsigned iShw = off / sizeof(X86PTEPAE);
                if (uShw.pPTPae->a[iShw].n.u1Present)
                {
#  ifdef PGMPOOL_WITH_GCPHYS_TRACKING
                    X86PTEPAE GstPte;
                    int rc = pgmPoolPhysSimpleReadGCPhys(pVM, &GstPte, pvAddress, GCPhysFault, sizeof(GstPte));
                    AssertRC(rc);

                    Log4(("pgmPoolMonitorChainChanging pae: deref %016RX64 GCPhys %016RX64\n", uShw.pPTPae->a[iShw].u & X86_PTE_PAE_PG_MASK, GstPte.u & X86_PTE_PAE_PG_MASK));
                    pgmPoolTracDerefGCPhysHint(pPool, pPage,
                                               uShw.pPTPae->a[iShw].u & X86_PTE_PAE_PG_MASK,
                                               GstPte.u & X86_PTE_PAE_PG_MASK);
#  endif
                    uShw.pPTPae->a[iShw].u = 0;
                }

                /* paranoia / a bit assumptive. */
                if (   pCpu
                    && (off & 7)
                    && (off & 7) + cbWrite > sizeof(X86PTEPAE))
                {
                    const unsigned iShw2 = (off + cbWrite - 1) / sizeof(X86PTEPAE);
                    AssertBreak(iShw2 < RT_ELEMENTS(uShw.pPTPae->a));

                    if (uShw.pPTPae->a[iShw2].n.u1Present)
                    {
#  ifdef PGMPOOL_WITH_GCPHYS_TRACKING
                        X86PTEPAE GstPte;
#   ifdef IN_RING3
                        int rc = pgmPoolPhysSimpleReadGCPhys(pVM, &GstPte, (RTHCPTR)((RTHCUINTPTR)pvAddress + sizeof(GstPte)), GCPhysFault + sizeof(GstPte), sizeof(GstPte));
#   else
                        int rc = pgmPoolPhysSimpleReadGCPhys(pVM, &GstPte, pvAddress + sizeof(GstPte), GCPhysFault + sizeof(GstPte), sizeof(GstPte));
#   endif
                        AssertRC(rc);
                        Log4(("pgmPoolMonitorChainChanging pae: deref %016RX64 GCPhys %016RX64\n", uShw.pPTPae->a[iShw2].u & X86_PTE_PAE_PG_MASK, GstPte.u & X86_PTE_PAE_PG_MASK));
                        pgmPoolTracDerefGCPhysHint(pPool, pPage,
                                                   uShw.pPTPae->a[iShw2].u & X86_PTE_PAE_PG_MASK,
                                                   GstPte.u & X86_PTE_PAE_PG_MASK);
#  endif
                        uShw.pPTPae->a[iShw2].u = 0;
                    }
                }
                break;
            }

            case PGMPOOLKIND_32BIT_PD:
            {
                uShw.pv = PGMPOOL_PAGE_2_LOCKED_PTR(pVM, pPage);
                const unsigned iShw = off / sizeof(X86PTE);         // ASSUMING 32-bit guest paging!

                LogFlow(("pgmPoolMonitorChainChanging: PGMPOOLKIND_32BIT_PD %x\n", iShw));
#  ifndef IN_RING0
                if (uShw.pPD->a[iShw].u & PGM_PDFLAGS_MAPPING)
                {
                    Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
                    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                    STAM_COUNTER_INC(&(pVCpu->pgm.s.StatRZGuestCR3WriteConflict));
                    LogFlow(("pgmPoolMonitorChainChanging: Detected conflict at iShw=%#x!\n", iShw));
                    break;
                }
#  endif /* !IN_RING0 */
#   ifndef IN_RING0
                else
#   endif /* !IN_RING0 */
                {
                    if (uShw.pPD->a[iShw].n.u1Present)
                    {
                        LogFlow(("pgmPoolMonitorChainChanging: 32 bit pd iShw=%#x: %RX64 -> freeing it!\n", iShw, uShw.pPD->a[iShw].u));
                        pgmPoolFree(pVM,
                                    uShw.pPD->a[iShw].u & X86_PDE_PAE_PG_MASK,
                                    pPage->idx,
                                    iShw);
                        uShw.pPD->a[iShw].u = 0;
                    }
                }
                /* paranoia / a bit assumptive. */
                if (   pCpu
                    && (off & 3)
                    && (off & 3) + cbWrite > sizeof(X86PTE))
                {
                    const unsigned iShw2 = (off + cbWrite - 1) / sizeof(X86PTE);
                    if (    iShw2 != iShw
                        &&  iShw2 < RT_ELEMENTS(uShw.pPD->a))
                    {
#  ifndef IN_RING0
                        if (uShw.pPD->a[iShw2].u & PGM_PDFLAGS_MAPPING)
                        {
                            Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
                            STAM_COUNTER_INC(&(pVCpu->pgm.s.StatRZGuestCR3WriteConflict));
                            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                            LogFlow(("pgmPoolMonitorChainChanging: Detected conflict at iShw2=%#x!\n", iShw2));
                            break;
                        }
#  endif /* !IN_RING0 */
#   ifndef IN_RING0
                        else
#   endif /* !IN_RING0 */
                        {
                            if (uShw.pPD->a[iShw2].n.u1Present)
                            {
                                LogFlow(("pgmPoolMonitorChainChanging: 32 bit pd iShw=%#x: %RX64 -> freeing it!\n", iShw2, uShw.pPD->a[iShw2].u));
                                pgmPoolFree(pVM,
                                            uShw.pPD->a[iShw2].u & X86_PDE_PAE_PG_MASK,
                                            pPage->idx,
                                            iShw2);
                                uShw.pPD->a[iShw2].u = 0;
                            }
                        }
                    }
                }
#if 0 /* useful when running PGMAssertCR3(), a bit too troublesome for general use (TLBs). */
                if (    uShw.pPD->a[iShw].n.u1Present
                    &&  !VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3))
                {
                    LogFlow(("pgmPoolMonitorChainChanging: iShw=%#x: %RX32 -> freeing it!\n", iShw, uShw.pPD->a[iShw].u));
# ifdef IN_RC       /* TLB load - we're pushing things a bit... */
                    ASMProbeReadByte(pvAddress);
# endif
                    pgmPoolFree(pVM, uShw.pPD->a[iShw].u & X86_PDE_PG_MASK, pPage->idx, iShw);
                    uShw.pPD->a[iShw].u = 0;
                }
#endif
                break;
            }

            case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
            {
                uShw.pv = PGMPOOL_PAGE_2_LOCKED_PTR(pVM, pPage);
                const unsigned iShw = off / sizeof(X86PDEPAE);
#ifndef IN_RING0
                if (uShw.pPDPae->a[iShw].u & PGM_PDFLAGS_MAPPING)
                {
                    Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
                    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                    STAM_COUNTER_INC(&(pVCpu->pgm.s.StatRZGuestCR3WriteConflict));
                    LogFlow(("pgmPoolMonitorChainChanging: Detected conflict at iShw=%#x!\n", iShw));
                    break;
                }
#endif /* !IN_RING0 */
                /*
                 * Causes trouble when the guest uses a PDE to refer to the whole page table level
                 * structure. (Invalidate here; faults later on when it tries to change the page
                 * table entries -> recheck; probably only applies to the RC case.)
                 */
# ifndef IN_RING0
                else
# endif /* !IN_RING0 */
                {
                    if (uShw.pPDPae->a[iShw].n.u1Present)
                    {
                        LogFlow(("pgmPoolMonitorChainChanging: pae pd iShw=%#x: %RX64 -> freeing it!\n", iShw, uShw.pPDPae->a[iShw].u));
                        pgmPoolFree(pVM,
                                    uShw.pPDPae->a[iShw].u & X86_PDE_PAE_PG_MASK,
                                    pPage->idx,
                                    iShw);
                        uShw.pPDPae->a[iShw].u = 0;
                    }
                }
                /* paranoia / a bit assumptive. */
                if (   pCpu
                    && (off & 7)
                    && (off & 7) + cbWrite > sizeof(X86PDEPAE))
                {
                    const unsigned iShw2 = (off + cbWrite - 1) / sizeof(X86PDEPAE);
                    AssertBreak(iShw2 < RT_ELEMENTS(uShw.pPDPae->a));

#ifndef IN_RING0
                    if (    iShw2 != iShw
                        &&  uShw.pPDPae->a[iShw2].u & PGM_PDFLAGS_MAPPING)
                    {
                        Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
                        VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                        STAM_COUNTER_INC(&(pVCpu->pgm.s.StatRZGuestCR3WriteConflict));
                        LogFlow(("pgmPoolMonitorChainChanging: Detected conflict at iShw2=%#x!\n", iShw2));
                        break;
                    }
#endif /* !IN_RING0 */
# ifndef IN_RING0
                    else
# endif /* !IN_RING0 */
                    if (uShw.pPDPae->a[iShw2].n.u1Present)
                    {
                        LogFlow(("pgmPoolMonitorChainChanging: pae pd iShw2=%#x: %RX64 -> freeing it!\n", iShw2, uShw.pPDPae->a[iShw2].u));
                        pgmPoolFree(pVM,
                                    uShw.pPDPae->a[iShw2].u & X86_PDE_PAE_PG_MASK,
                                    pPage->idx,
                                    iShw2);
                        uShw.pPDPae->a[iShw2].u = 0;
                    }
                }
                break;
            }

            case PGMPOOLKIND_PAE_PDPT:
            {
                /*
                 * Hopefully this doesn't happen very often:
                 * - touching unused parts of the page
                 * - messing with the bits of pd pointers without changing the physical address
                 */
                /* PDPT roots are not page aligned; 32 byte only! */
                const unsigned offPdpt = GCPhysFault - pPage->GCPhys;

                uShw.pv = PGMPOOL_PAGE_2_LOCKED_PTR(pVM, pPage);
                const unsigned iShw = offPdpt / sizeof(X86PDPE);
                if (iShw < X86_PG_PAE_PDPE_ENTRIES)          /* don't use RT_ELEMENTS(uShw.pPDPT->a), because that's for long mode only */
                {
# ifndef IN_RING0
                    if (uShw.pPDPT->a[iShw].u & PGM_PLXFLAGS_MAPPING)
                    {
                        Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
                        STAM_COUNTER_INC(&(pVCpu->pgm.s.StatRZGuestCR3WriteConflict));
                        VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                        LogFlow(("pgmPoolMonitorChainChanging: Detected pdpt conflict at iShw=%#x!\n", iShw));
                        break;
                    }
# endif /* !IN_RING0 */
#  ifndef IN_RING0
                    else
#  endif /* !IN_RING0 */
                    if (uShw.pPDPT->a[iShw].n.u1Present)
                    {
                        LogFlow(("pgmPoolMonitorChainChanging: pae pdpt iShw=%#x: %RX64 -> freeing it!\n", iShw, uShw.pPDPT->a[iShw].u));
                        pgmPoolFree(pVM,
                                    uShw.pPDPT->a[iShw].u & X86_PDPE_PG_MASK,
                                    pPage->idx,
                                    iShw);
                        uShw.pPDPT->a[iShw].u = 0;
                    }

                    /* paranoia / a bit assumptive. */
                    if (   pCpu
                        && (offPdpt & 7)
                        && (offPdpt & 7) + cbWrite > sizeof(X86PDPE))
                    {
                        const unsigned iShw2 = (offPdpt + cbWrite - 1) / sizeof(X86PDPE);
                        if (    iShw2 != iShw
                            &&  iShw2 < X86_PG_PAE_PDPE_ENTRIES)
                        {
# ifndef IN_RING0
                            if (uShw.pPDPT->a[iShw2].u & PGM_PLXFLAGS_MAPPING)
                            {
                                Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
                                STAM_COUNTER_INC(&(pVCpu->pgm.s.StatRZGuestCR3WriteConflict));
                                VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                                LogFlow(("pgmPoolMonitorChainChanging: Detected conflict at iShw2=%#x!\n", iShw2));
                                break;
                            }
# endif /* !IN_RING0 */
#  ifndef IN_RING0
                            else
#  endif /* !IN_RING0 */
                            if (uShw.pPDPT->a[iShw2].n.u1Present)
                            {
                                LogFlow(("pgmPoolMonitorChainChanging: pae pdpt iShw=%#x: %RX64 -> freeing it!\n", iShw2, uShw.pPDPT->a[iShw2].u));
                                pgmPoolFree(pVM,
                                            uShw.pPDPT->a[iShw2].u & X86_PDPE_PG_MASK,
                                            pPage->idx,
                                            iShw2);
                                uShw.pPDPT->a[iShw2].u = 0;
                            }
                        }
                    }
                }
                break;
            }

#ifndef IN_RC
            case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
            {
                uShw.pv = PGMPOOL_PAGE_2_LOCKED_PTR(pVM, pPage);
                const unsigned iShw = off / sizeof(X86PDEPAE);
                Assert(!(uShw.pPDPae->a[iShw].u & PGM_PDFLAGS_MAPPING));
                if (uShw.pPDPae->a[iShw].n.u1Present)
                {
                    LogFlow(("pgmPoolMonitorChainChanging: pae pd iShw=%#x: %RX64 -> freeing it!\n", iShw, uShw.pPDPae->a[iShw].u));
                    pgmPoolFree(pVM,
                                uShw.pPDPae->a[iShw].u & X86_PDE_PAE_PG_MASK,
                                pPage->idx,
                                iShw);
                    uShw.pPDPae->a[iShw].u = 0;
                }
                /* paranoia / a bit assumptive. */
                if (   pCpu
                    && (off & 7)
                    && (off & 7) + cbWrite > sizeof(X86PDEPAE))
                {
                    const unsigned iShw2 = (off + cbWrite - 1) / sizeof(X86PDEPAE);
                    AssertBreak(iShw2 < RT_ELEMENTS(uShw.pPDPae->a));

                    Assert(!(uShw.pPDPae->a[iShw2].u & PGM_PDFLAGS_MAPPING));
                    if (uShw.pPDPae->a[iShw2].n.u1Present)
                    {
                        LogFlow(("pgmPoolMonitorChainChanging: pae pd iShw2=%#x: %RX64 -> freeing it!\n", iShw2, uShw.pPDPae->a[iShw2].u));
                        pgmPoolFree(pVM,
                                    uShw.pPDPae->a[iShw2].u & X86_PDE_PAE_PG_MASK,
                                    pPage->idx,
                                    iShw2);
                        uShw.pPDPae->a[iShw2].u = 0;
                    }
                }
                break;
            }

            case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
            {
                /*
                 * Hopefully this doesn't happen very often:
                 * - messing with the bits of pd pointers without changing the physical address
                 */
                if (!VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3))
                {
                    uShw.pv = PGMPOOL_PAGE_2_LOCKED_PTR(pVM, pPage);
                    const unsigned iShw = off / sizeof(X86PDPE);
                    if (uShw.pPDPT->a[iShw].n.u1Present)
                    {
                        LogFlow(("pgmPoolMonitorChainChanging: pdpt iShw=%#x: %RX64 -> freeing it!\n", iShw, uShw.pPDPT->a[iShw].u));
                        pgmPoolFree(pVM, uShw.pPDPT->a[iShw].u & X86_PDPE_PG_MASK, pPage->idx, iShw);
                        uShw.pPDPT->a[iShw].u = 0;
                    }
                    /* paranoia / a bit assumptive. */
                    if (   pCpu
                        && (off & 7)
                        && (off & 7) + cbWrite > sizeof(X86PDPE))
                    {
                        const unsigned iShw2 = (off + cbWrite - 1) / sizeof(X86PDPE);
                        if (uShw.pPDPT->a[iShw2].n.u1Present)
                        {
                            LogFlow(("pgmPoolMonitorChainChanging: pdpt iShw2=%#x: %RX64 -> freeing it!\n", iShw2, uShw.pPDPT->a[iShw2].u));
                            pgmPoolFree(pVM, uShw.pPDPT->a[iShw2].u & X86_PDPE_PG_MASK, pPage->idx, iShw2);
                            uShw.pPDPT->a[iShw2].u = 0;
                        }
                    }
                }
                break;
            }

            case PGMPOOLKIND_64BIT_PML4:
            {
                /*
                 * Hopefully this doesn't happen very often:
                 * - messing with the bits of pd pointers without changing the physical address
                 */
                if (!VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3))
                {
                    uShw.pv = PGMPOOL_PAGE_2_LOCKED_PTR(pVM, pPage);
                    const unsigned iShw = off / sizeof(X86PDPE);
                    if (uShw.pPML4->a[iShw].n.u1Present)
                    {
                        LogFlow(("pgmPoolMonitorChainChanging: pml4 iShw=%#x: %RX64 -> freeing it!\n", iShw, uShw.pPML4->a[iShw].u));
                        pgmPoolFree(pVM, uShw.pPML4->a[iShw].u & X86_PML4E_PG_MASK, pPage->idx, iShw);
                        uShw.pPML4->a[iShw].u = 0;
                    }
                    /* paranoia / a bit assumptive. */
                    if (   pCpu
                        && (off & 7)
                        && (off & 7) + cbWrite > sizeof(X86PDPE))
                    {
                        const unsigned iShw2 = (off + cbWrite - 1) / sizeof(X86PML4E);
                        if (uShw.pPML4->a[iShw2].n.u1Present)
                        {
                            LogFlow(("pgmPoolMonitorChainChanging: pml4 iShw2=%#x: %RX64 -> freeing it!\n", iShw2, uShw.pPML4->a[iShw2].u));
                            pgmPoolFree(pVM, uShw.pPML4->a[iShw2].u & X86_PML4E_PG_MASK, pPage->idx, iShw2);
                            uShw.pPML4->a[iShw2].u = 0;
                        }
                    }
                }
                break;
            }
#endif /* IN_RING0 */

            default:
                AssertFatalMsgFailed(("enmKind=%d\n", pPage->enmKind));
        }
        PGMPOOL_UNLOCK_PTR(pVM, uShw.pv);

        /* next */
        if (pPage->iMonitoredNext == NIL_PGMPOOL_IDX)
            return;
        pPage = &pPool->aPages[pPage->iMonitoredNext];
    }
}

# ifndef IN_RING3
/**
 * Checks if a access could be a fork operation in progress.
 *
 * Meaning, that the guest is setuping up the parent process for Copy-On-Write.
 *
 * @returns true if it's likly that we're forking, otherwise false.
 * @param   pPool       The pool.
 * @param   pCpu        The disassembled instruction.
 * @param   offFault    The access offset.
 */
DECLINLINE(bool) pgmPoolMonitorIsForking(PPGMPOOL pPool, PDISCPUSTATE pCpu, unsigned offFault)
{
    /*
     * i386 linux is using btr to clear X86_PTE_RW.
     * The functions involved are (2.6.16 source inspection):
     *      clear_bit
     *      ptep_set_wrprotect
     *      copy_one_pte
     *      copy_pte_range
     *      copy_pmd_range
     *      copy_pud_range
     *      copy_page_range
     *      dup_mmap
     *      dup_mm
     *      copy_mm
     *      copy_process
     *      do_fork
     */
    if (    pCpu->pCurInstr->opcode == OP_BTR
        &&  !(offFault & 4)
        /** @todo Validate that the bit index is X86_PTE_RW. */
            )
    {
        STAM_COUNTER_INC(&pPool->CTX_MID_Z(StatMonitor,Fork));
        return true;
    }
    return false;
}


/**
 * Determine whether the page is likely to have been reused.
 *
 * @returns true if we consider the page as being reused for a different purpose.
 * @returns false if we consider it to still be a paging page.
 * @param   pVM         VM Handle.
 * @param   pPage       The page in question.
 * @param   pRegFrame   Trap register frame.
 * @param   pCpu        The disassembly info for the faulting instruction.
 * @param   pvFault     The fault address.
 *
 * @remark  The REP prefix check is left to the caller because of STOSD/W.
 */
DECLINLINE(bool) pgmPoolMonitorIsReused(PVM pVM, PPGMPOOLPAGE pPage, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu, RTGCPTR pvFault)
{
#ifndef IN_RC
    /** @todo could make this general, faulting close to rsp should be safe reuse heuristic. */
    if (   HWACCMHasPendingIrq(pVM)
        && (pRegFrame->rsp - pvFault) < 32)
    {
        /* Fault caused by stack writes while trying to inject an interrupt event. */
        Log(("pgmPoolMonitorIsReused: reused %RGv for interrupt stack (rsp=%RGv).\n", pvFault, pRegFrame->rsp));
        return true;
    }
#else
    NOREF(pVM); NOREF(pvFault);
#endif

    switch (pCpu->pCurInstr->opcode)
    {
        /* call implies the actual push of the return address faulted */
        case OP_CALL:
            Log4(("pgmPoolMonitorIsReused: CALL\n"));
            return true;
        case OP_PUSH:
            Log4(("pgmPoolMonitorIsReused: PUSH\n"));
            return true;
        case OP_PUSHF:
            Log4(("pgmPoolMonitorIsReused: PUSHF\n"));
            return true;
        case OP_PUSHA:
            Log4(("pgmPoolMonitorIsReused: PUSHA\n"));
            return true;
        case OP_FXSAVE:
            Log4(("pgmPoolMonitorIsReused: FXSAVE\n"));
            return true;
        case OP_MOVNTI:     /* solaris - block_zero_no_xmm */
            Log4(("pgmPoolMonitorIsReused: MOVNTI\n"));
            return true;
        case OP_MOVNTDQ:    /* solaris - hwblkclr & hwblkpagecopy */
            Log4(("pgmPoolMonitorIsReused: MOVNTDQ\n"));
            return true;
        case OP_MOVSWD:
        case OP_STOSWD:
            if (    pCpu->prefix == (PREFIX_REP|PREFIX_REX)
                &&  pRegFrame->rcx >= 0x40
               )
            {
                Assert(pCpu->mode == CPUMODE_64BIT);

                Log(("pgmPoolMonitorIsReused: OP_STOSQ\n"));
                return true;
            }
            return false;
    }
    if (    (pCpu->param1.flags & USE_REG_GEN32)
        &&  (pCpu->param1.base.reg_gen == USE_REG_ESP))
    {
        Log4(("pgmPoolMonitorIsReused: ESP\n"));
        return true;
    }

    //if (pPage->fCR3Mix)
    //    return false;
    return false;
}


/**
 * Flushes the page being accessed.
 *
 * @returns VBox status code suitable for scheduling.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pPool       The pool.
 * @param   pPage       The pool page (head).
 * @param   pCpu        The disassembly of the write instruction.
 * @param   pRegFrame   The trap register frame.
 * @param   GCPhysFault The fault address as guest physical address.
 * @param   pvFault     The fault address.
 */
static int pgmPoolAccessHandlerFlush(PVM pVM, PVMCPU pVCpu, PPGMPOOL pPool, PPGMPOOLPAGE pPage, PDISCPUSTATE pCpu,
                                     PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, RTGCPTR pvFault)
{
    /*
     * First, do the flushing.
     */
    int rc = pgmPoolMonitorChainFlush(pPool, pPage);

    /*
     * Emulate the instruction (xp/w2k problem, requires pc/cr2/sp detection).
     */
    uint32_t cbWritten;
    int rc2 = EMInterpretInstructionCPU(pVM, pVCpu, pCpu, pRegFrame, pvFault, &cbWritten);
    if (RT_SUCCESS(rc2))
        pRegFrame->rip += pCpu->opsize;
    else if (rc2 == VERR_EM_INTERPRETER)
    {
#ifdef IN_RC
        if (PATMIsPatchGCAddr(pVM, (RTRCPTR)pRegFrame->eip))
        {
            LogFlow(("pgmPoolAccessHandlerPTWorker: Interpretation failed for patch code %04x:%RGv, ignoring.\n",
                     pRegFrame->cs, (RTGCPTR)pRegFrame->eip));
            rc = VINF_SUCCESS;
            STAM_COUNTER_INC(&pPool->StatMonitorRZIntrFailPatch2);
        }
        else
#endif
        {
            rc = VINF_EM_RAW_EMULATE_INSTR;
            STAM_COUNTER_INC(&pPool->CTX_MID_Z(StatMonitor,EmulateInstr));
        }
    }
    else
        rc = rc2;

    /* See use in pgmPoolAccessHandlerSimple(). */
    PGM_INVL_GUEST_TLBS();

    LogFlow(("pgmPoolAccessHandlerPT: returns %Rrc (flushed)\n", rc));
    return rc;

}


/**
 * Handles the STOSD write accesses.
 *
 * @returns VBox status code suitable for scheduling.
 * @param   pVM         The VM handle.
 * @param   pPool       The pool.
 * @param   pPage       The pool page (head).
 * @param   pCpu        The disassembly of the write instruction.
 * @param   pRegFrame   The trap register frame.
 * @param   GCPhysFault The fault address as guest physical address.
 * @param   pvFault     The fault address.
 */
DECLINLINE(int) pgmPoolAccessHandlerSTOSD(PVM pVM, PPGMPOOL pPool, PPGMPOOLPAGE pPage, PDISCPUSTATE pCpu,
                                          PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, RTGCPTR pvFault)
{
    Assert(pCpu->mode == CPUMODE_32BIT);

    Log3(("pgmPoolAccessHandlerSTOSD\n"));

    /*
     * Increment the modification counter and insert it into the list
     * of modified pages the first time.
     */
    if (!pPage->cModifications++)
        pgmPoolMonitorModifiedInsert(pPool, pPage);

    /*
     * Execute REP STOSD.
     *
     * This ASSUMES that we're not invoked by Trap0e on in a out-of-sync
     * write situation, meaning that it's safe to write here.
     */
    PVMCPU      pVCpu = VMMGetCpu(pPool->CTX_SUFF(pVM));
    RTGCUINTPTR pu32 = (RTGCUINTPTR)pvFault;
    while (pRegFrame->ecx)
    {
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
        uint32_t iPrevSubset = PGMDynMapPushAutoSubset(pVCpu);
        pgmPoolMonitorChainChanging(pVCpu, pPool, pPage, GCPhysFault, (RTGCPTR)pu32, NULL);
        PGMDynMapPopAutoSubset(pVCpu, iPrevSubset);
#else
        pgmPoolMonitorChainChanging(pVCpu, pPool, pPage, GCPhysFault, (RTGCPTR)pu32, NULL);
#endif
#ifdef IN_RC
        *(uint32_t *)pu32 = pRegFrame->eax;
#else
        PGMPhysSimpleWriteGCPhys(pVM, GCPhysFault, &pRegFrame->eax, 4);
#endif
        pu32           += 4;
        GCPhysFault    += 4;
        pRegFrame->edi += 4;
        pRegFrame->ecx--;
    }
    pRegFrame->rip += pCpu->opsize;

#ifdef IN_RC
    /* See use in pgmPoolAccessHandlerSimple(). */
    PGM_INVL_GUEST_TLBS();
#endif

    LogFlow(("pgmPoolAccessHandlerSTOSD: returns\n"));
    return VINF_SUCCESS;
}


/**
 * Handles the simple write accesses.
 *
 * @returns VBox status code suitable for scheduling.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pPool       The pool.
 * @param   pPage       The pool page (head).
 * @param   pCpu        The disassembly of the write instruction.
 * @param   pRegFrame   The trap register frame.
 * @param   GCPhysFault The fault address as guest physical address.
 * @param   pvFault     The fault address.
 */
DECLINLINE(int) pgmPoolAccessHandlerSimple(PVM pVM, PVMCPU pVCpu, PPGMPOOL pPool, PPGMPOOLPAGE pPage, PDISCPUSTATE pCpu,
                                           PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, RTGCPTR pvFault)
{
    Log3(("pgmPoolAccessHandlerSimple\n"));
    /*
     * Increment the modification counter and insert it into the list
     * of modified pages the first time.
     */
    if (!pPage->cModifications++)
        pgmPoolMonitorModifiedInsert(pPool, pPage);

    /*
     * Clear all the pages. ASSUMES that pvFault is readable.
     */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    uint32_t    iPrevSubset = PGMDynMapPushAutoSubset(pVCpu);
    pgmPoolMonitorChainChanging(pVCpu, pPool, pPage, GCPhysFault, pvFault, pCpu);
    PGMDynMapPopAutoSubset(pVCpu, iPrevSubset);
#else
    pgmPoolMonitorChainChanging(pVCpu, pPool, pPage, GCPhysFault, pvFault, pCpu);
#endif

    /*
     * Interpret the instruction.
     */
    uint32_t cb;
    int rc = EMInterpretInstructionCPU(pVM, pVCpu, pCpu, pRegFrame, pvFault, &cb);
    if (RT_SUCCESS(rc))
        pRegFrame->rip += pCpu->opsize;
    else if (rc == VERR_EM_INTERPRETER)
    {
        LogFlow(("pgmPoolAccessHandlerPTWorker: Interpretation failed for %04x:%RGv - opcode=%d\n",
                  pRegFrame->cs, (RTGCPTR)pRegFrame->rip, pCpu->pCurInstr->opcode));
        rc = VINF_EM_RAW_EMULATE_INSTR;
        STAM_COUNTER_INC(&pPool->CTX_MID_Z(StatMonitor,EmulateInstr));
    }

#ifdef IN_RC
    /*
     * Quick hack, with logging enabled we're getting stale
     * code TLBs but no data TLB for EIP and crash in EMInterpretDisasOne.
     * Flushing here is BAD and expensive, I think EMInterpretDisasOne will
     * have to be fixed to support this. But that'll have to wait till next week.
     *
     * An alternative is to keep track of the changed PTEs together with the
     * GCPhys from the guest PT. This may proove expensive though.
     *
     * At the moment, it's VITAL that it's done AFTER the instruction interpreting
     * because we need the stale TLBs in some cases (XP boot). This MUST be fixed properly!
     */
    PGM_INVL_GUEST_TLBS();
#endif

    LogFlow(("pgmPoolAccessHandlerSimple: returns %Rrc cb=%d\n", rc, cb));
    return rc;
}

/**
 * \#PF Handler callback for PT write accesses.
 *
 * @returns VBox status code (appropriate for GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 *                      NULL on DMA and other non CPU access.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument.
 */
DECLEXPORT(int) pgmPoolAccessHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser)
{
    STAM_PROFILE_START(&pVM->pgm.s.CTX_SUFF(pPool)->CTX_SUFF_Z(StatMonitor), a);
    PPGMPOOL        pPool = pVM->pgm.s.CTX_SUFF(pPool);
    PPGMPOOLPAGE    pPage = (PPGMPOOLPAGE)pvUser;
    PVMCPU          pVCpu = VMMGetCpu(pVM);

    LogFlow(("pgmPoolAccessHandler: pvFault=%RGv pPage=%p:{.idx=%d} GCPhysFault=%RGp\n", pvFault, pPage, pPage->idx, GCPhysFault));

    /*
     * We should ALWAYS have the list head as user parameter. This
     * is because we use that page to record the changes.
     */
    Assert(pPage->iMonitoredPrev == NIL_PGMPOOL_IDX);

    /*
     * Disassemble the faulting instruction.
     */
    DISCPUSTATE Cpu;
    int rc = EMInterpretDisasOne(pVM, pVCpu, pRegFrame, &Cpu, NULL);
    AssertRCReturn(rc, rc);

    /*
     * Check if it's worth dealing with.
     */
    bool fReused = false;
    if (    (   pPage->cModifications < 48   /** @todo #define */ /** @todo need to check that it's not mapping EIP. */ /** @todo adjust this! */
             || pgmPoolIsPageLocked(&pVM->pgm.s, pPage)
            )
        &&  !(fReused = pgmPoolMonitorIsReused(pVM, pPage, pRegFrame, &Cpu, pvFault))
        &&  !pgmPoolMonitorIsForking(pPool, &Cpu, GCPhysFault & PAGE_OFFSET_MASK))
    {
        /*
         * Simple instructions, no REP prefix.
         */
        if (!(Cpu.prefix & (PREFIX_REP | PREFIX_REPNE)))
        {
             rc = pgmPoolAccessHandlerSimple(pVM, pVCpu, pPool, pPage, &Cpu, pRegFrame, GCPhysFault, pvFault);
             STAM_PROFILE_STOP_EX(&pVM->pgm.s.CTX_SUFF(pPool)->CTX_SUFF_Z(StatMonitor), &pPool->CTX_MID_Z(StatMonitor,Handled), a);
             return rc;
        }

        /*
         * Windows is frequently doing small memset() operations (netio test 4k+).
         * We have to deal with these or we'll kill the cache and performance.
         */
        if (    Cpu.pCurInstr->opcode == OP_STOSWD
            &&  CPUMGetGuestCPL(pVCpu, pRegFrame) == 0
            &&  pRegFrame->ecx <= 0x20
            &&  pRegFrame->ecx * 4 <= PAGE_SIZE - ((uintptr_t)pvFault & PAGE_OFFSET_MASK)
            &&  !((uintptr_t)pvFault & 3)
            &&  (pRegFrame->eax == 0 || pRegFrame->eax == 0x80) /* the two values observed. */
            &&  Cpu.mode == CPUMODE_32BIT
            &&  Cpu.opmode == CPUMODE_32BIT
            &&  Cpu.addrmode == CPUMODE_32BIT
            &&  Cpu.prefix == PREFIX_REP
            &&  !pRegFrame->eflags.Bits.u1DF
            )
        {
             rc = pgmPoolAccessHandlerSTOSD(pVM, pPool, pPage, &Cpu, pRegFrame, GCPhysFault, pvFault);
             STAM_PROFILE_STOP_EX(&pVM->pgm.s.CTX_SUFF(pPool)->CTX_SUFF_Z(StatMonitor), &pPool->CTX_MID_Z(StatMonitor,RepStosd), a);
             return rc;
        }

        /* REP prefix, don't bother. */
        STAM_COUNTER_INC(&pPool->CTX_MID_Z(StatMonitor,RepPrefix));
        Log4(("pgmPoolAccessHandler: eax=%#x ecx=%#x edi=%#x esi=%#x rip=%RGv opcode=%d prefix=%#x\n",
              pRegFrame->eax, pRegFrame->ecx, pRegFrame->edi, pRegFrame->esi, (RTGCPTR)pRegFrame->rip, Cpu.pCurInstr->opcode, Cpu.prefix));
    }

    /*
     * Not worth it, so flush it.
     *
     * If we considered it to be reused, don't to back to ring-3
     * to emulate failed instructions since we usually cannot
     * interpret then. This may be a bit risky, in which case
     * the reuse detection must be fixed.
     */
    rc = pgmPoolAccessHandlerFlush(pVM, pVCpu, pPool, pPage, &Cpu, pRegFrame, GCPhysFault, pvFault);
    if (rc == VINF_EM_RAW_EMULATE_INSTR && fReused)
        rc = VINF_SUCCESS;
    STAM_PROFILE_STOP_EX(&pVM->pgm.s.CTX_SUFF(pPool)->CTX_SUFF_Z(StatMonitor), &pPool->CTX_MID_Z(StatMonitor,FlushPage), a);
    return rc;
}

# endif /* !IN_RING3 */
#endif  /* PGMPOOL_WITH_MONITORING */

#ifdef PGMPOOL_WITH_CACHE

/**
 * Inserts a page into the GCPhys hash table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 */
DECLINLINE(void) pgmPoolHashInsert(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    Log3(("pgmPoolHashInsert: %RGp\n", pPage->GCPhys));
    Assert(pPage->GCPhys != NIL_RTGCPHYS); Assert(pPage->iNext == NIL_PGMPOOL_IDX);
    uint16_t iHash = PGMPOOL_HASH(pPage->GCPhys);
    pPage->iNext = pPool->aiHash[iHash];
    pPool->aiHash[iHash] = pPage->idx;
}


/**
 * Removes a page from the GCPhys hash table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 */
DECLINLINE(void) pgmPoolHashRemove(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    Log3(("pgmPoolHashRemove: %RGp\n", pPage->GCPhys));
    uint16_t iHash = PGMPOOL_HASH(pPage->GCPhys);
    if (pPool->aiHash[iHash] == pPage->idx)
        pPool->aiHash[iHash] = pPage->iNext;
    else
    {
        uint16_t iPrev = pPool->aiHash[iHash];
        for (;;)
        {
            const int16_t i = pPool->aPages[iPrev].iNext;
            if (i == pPage->idx)
            {
                pPool->aPages[iPrev].iNext = pPage->iNext;
                break;
            }
            if (i == NIL_PGMPOOL_IDX)
            {
                AssertReleaseMsgFailed(("GCPhys=%RGp idx=%#x\n", pPage->GCPhys, pPage->idx));
                break;
            }
            iPrev = i;
        }
    }
    pPage->iNext = NIL_PGMPOOL_IDX;
}


/**
 * Frees up one cache page.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @param   pPool       The pool.
 * @param   iUser       The user index.
 */
static int pgmPoolCacheFreeOne(PPGMPOOL pPool, uint16_t iUser)
{
#ifndef IN_RC
    const PVM pVM = pPool->CTX_SUFF(pVM);
#endif
    Assert(pPool->iAgeHead != pPool->iAgeTail); /* We shouldn't be here if there < 2 cached entries! */
    STAM_COUNTER_INC(&pPool->StatCacheFreeUpOne);

    /*
     * Select one page from the tail of the age list.
     */
    PPGMPOOLPAGE    pPage;
    for (unsigned iLoop = 0; ; iLoop++)
    {
        uint16_t iToFree = pPool->iAgeTail;
        if (iToFree == iUser)
            iToFree = pPool->aPages[iToFree].iAgePrev;
/* This is the alternative to the SyncCR3 pgmPoolCacheUsed calls.
        if (pPool->aPages[iToFree].iUserHead != NIL_PGMPOOL_USER_INDEX)
        {
            uint16_t i = pPool->aPages[iToFree].iAgePrev;
            for (unsigned j = 0; j < 10 && i != NIL_PGMPOOL_USER_INDEX; j++, i = pPool->aPages[i].iAgePrev)
            {
                if (pPool->aPages[iToFree].iUserHead == NIL_PGMPOOL_USER_INDEX)
                    continue;
                iToFree = i;
                break;
            }
        }
*/
        Assert(iToFree != iUser);
        AssertRelease(iToFree != NIL_PGMPOOL_IDX);
        pPage = &pPool->aPages[iToFree];

        /*
         * Reject any attempts at flushing the currently active shadow CR3 mapping.
         * Call pgmPoolCacheUsed to move the page to the head of the age list.
         */
        if (!pgmPoolIsPageLocked(&pPool->CTX_SUFF(pVM)->pgm.s, pPage))
            break;
        LogFlow(("pgmPoolCacheFreeOne: refuse CR3 mapping\n"));
        pgmPoolCacheUsed(pPool, pPage);
        AssertLogRelReturn(iLoop < 8192, VERR_INTERNAL_ERROR);
    }

    /*
     * Found a usable page, flush it and return.
     */
    int rc = pgmPoolFlushPage(pPool, pPage);
    if (rc == VINF_SUCCESS)
        PGM_INVL_GUEST_TLBS(); /* see PT handler. */
    return rc;
}


/**
 * Checks if a kind mismatch is really a page being reused
 * or if it's just normal remappings.
 *
 * @returns true if reused and the cached page (enmKind1) should be flushed
 * @returns false if not reused.
 * @param   enmKind1    The kind of the cached page.
 * @param   enmKind2    The kind of the requested page.
 */
static bool pgmPoolCacheReusedByKind(PGMPOOLKIND enmKind1, PGMPOOLKIND enmKind2)
{
    switch (enmKind1)
    {
        /*
         * Never reuse them. There is no remapping in non-paging mode.
         */
        case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
        case PGMPOOLKIND_32BIT_PD_PHYS:
        case PGMPOOLKIND_PAE_PT_FOR_PHYS:
        case PGMPOOLKIND_PAE_PD_PHYS:
        case PGMPOOLKIND_PAE_PDPT_PHYS:
        case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
        case PGMPOOLKIND_EPT_PT_FOR_PHYS:
        case PGMPOOLKIND_EPT_PD_FOR_PHYS:
        case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_PAE_PDPT_FOR_32BIT: /* never reuse them for other types */
            return false;

        /*
         * It's perfectly fine to reuse these, except for PAE and non-paging stuff.
         */
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
        case PGMPOOLKIND_32BIT_PD:
        case PGMPOOLKIND_PAE_PDPT:
            switch (enmKind2)
            {
                case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
                case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
                case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
                case PGMPOOLKIND_64BIT_PML4:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
                case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
                case PGMPOOLKIND_PAE_PT_FOR_PHYS:
                case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
                case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
                case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
                case PGMPOOLKIND_EPT_PD_FOR_PHYS:
                case PGMPOOLKIND_EPT_PT_FOR_PHYS:
                    return true;
                default:
                    return false;
            }

        /*
         * It's perfectly fine to reuse these, except for PAE and non-paging stuff.
         */
        case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
        case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
        case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
        case PGMPOOLKIND_64BIT_PML4:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
            switch (enmKind2)
            {
                case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
                case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
                case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
                case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
                case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
                case PGMPOOLKIND_PAE_PT_FOR_PHYS:
                case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
                case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
                case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
                case PGMPOOLKIND_EPT_PD_FOR_PHYS:
                case PGMPOOLKIND_EPT_PT_FOR_PHYS:
                    return true;
                default:
                    return false;
            }

        /*
         * These cannot be flushed, and it's common to reuse the PDs as PTs.
         */
        case PGMPOOLKIND_ROOT_NESTED:
            return false;

        default:
            AssertFatalMsgFailed(("enmKind1=%d\n", enmKind1));
    }
}


/**
 * Attempts to satisfy a pgmPoolAlloc request from the cache.
 *
 * @returns VBox status code.
 * @retval  VINF_PGM_CACHED_PAGE on success.
 * @retval  VERR_FILE_NOT_FOUND if not found.
 * @param   pPool       The pool.
 * @param   GCPhys      The GC physical address of the page we're gonna shadow.
 * @param   enmKind     The kind of mapping.
 * @param   iUser       The shadow page pool index of the user table.
 * @param   iUserTable  The index into the user table (shadowed).
 * @param   ppPage      Where to store the pointer to the page.
 */
static int pgmPoolCacheAlloc(PPGMPOOL pPool, RTGCPHYS GCPhys, PGMPOOLKIND enmKind, uint16_t iUser, uint32_t iUserTable, PPPGMPOOLPAGE ppPage)
{
#ifndef IN_RC
    const PVM pVM = pPool->CTX_SUFF(pVM);
#endif
    /*
     * Look up the GCPhys in the hash.
     */
    unsigned i = pPool->aiHash[PGMPOOL_HASH(GCPhys)];
    Log3(("pgmPoolCacheAlloc: %RGp kind %s iUser=%x iUserTable=%x SLOT=%d\n", GCPhys, pgmPoolPoolKindToStr(enmKind), iUser, iUserTable, i));
    if (i != NIL_PGMPOOL_IDX)
    {
        do
        {
            PPGMPOOLPAGE pPage = &pPool->aPages[i];
            Log4(("pgmPoolCacheAlloc: slot %d found page %RGp\n", i, pPage->GCPhys));
            if (pPage->GCPhys == GCPhys)
            {
                if ((PGMPOOLKIND)pPage->enmKind == enmKind)
                {
                    /* Put it at the start of the use list to make sure pgmPoolTrackAddUser
                     * doesn't flush it in case there are no more free use records.
                     */
                    pgmPoolCacheUsed(pPool, pPage);

                    int rc = pgmPoolTrackAddUser(pPool, pPage, iUser, iUserTable);
                    if (RT_SUCCESS(rc))
                    {
                        Assert((PGMPOOLKIND)pPage->enmKind == enmKind);
                        *ppPage = pPage;
                        STAM_COUNTER_INC(&pPool->StatCacheHits);
                        return VINF_PGM_CACHED_PAGE;
                    }
                    return rc;
                }

                /*
                 * The kind is different. In some cases we should now flush the page
                 * as it has been reused, but in most cases this is normal remapping
                 * of PDs as PT or big pages using the GCPhys field in a slightly
                 * different way than the other kinds.
                 */
                if (pgmPoolCacheReusedByKind((PGMPOOLKIND)pPage->enmKind, enmKind))
                {
                    STAM_COUNTER_INC(&pPool->StatCacheKindMismatches);
                    pgmPoolFlushPage(pPool, pPage);
                    PGM_INVL_GUEST_TLBS(); /* see PT handler. */
                    break;
                }
            }

            /* next */
            i = pPage->iNext;
        } while (i != NIL_PGMPOOL_IDX);
    }

    Log3(("pgmPoolCacheAlloc: Missed GCPhys=%RGp enmKind=%s\n", GCPhys, pgmPoolPoolKindToStr(enmKind)));
    STAM_COUNTER_INC(&pPool->StatCacheMisses);
    return VERR_FILE_NOT_FOUND;
}


/**
 * Inserts a page into the cache.
 *
 * @param   pPool           The pool.
 * @param   pPage           The cached page.
 * @param   fCanBeCached    Set if the page is fit for caching from the caller's point of view.
 */
static void pgmPoolCacheInsert(PPGMPOOL pPool, PPGMPOOLPAGE pPage, bool fCanBeCached)
{
    /*
     * Insert into the GCPhys hash if the page is fit for that.
     */
    Assert(!pPage->fCached);
    if (fCanBeCached)
    {
        pPage->fCached = true;
        pgmPoolHashInsert(pPool, pPage);
        Log3(("pgmPoolCacheInsert: Caching %p:{.Core=%RHp, .idx=%d, .enmKind=%s, GCPhys=%RGp}\n",
              pPage, pPage->Core.Key, pPage->idx, pgmPoolPoolKindToStr(pPage->enmKind), pPage->GCPhys));
        STAM_COUNTER_INC(&pPool->StatCacheCacheable);
    }
    else
    {
        Log3(("pgmPoolCacheInsert: Not caching %p:{.Core=%RHp, .idx=%d, .enmKind=%s, GCPhys=%RGp}\n",
              pPage, pPage->Core.Key, pPage->idx, pgmPoolPoolKindToStr(pPage->enmKind), pPage->GCPhys));
        STAM_COUNTER_INC(&pPool->StatCacheUncacheable);
    }

    /*
     * Insert at the head of the age list.
     */
    pPage->iAgePrev = NIL_PGMPOOL_IDX;
    pPage->iAgeNext = pPool->iAgeHead;
    if (pPool->iAgeHead != NIL_PGMPOOL_IDX)
        pPool->aPages[pPool->iAgeHead].iAgePrev = pPage->idx;
    else
        pPool->iAgeTail = pPage->idx;
    pPool->iAgeHead = pPage->idx;
}


/**
 * Flushes a cached page.
 *
 * @param   pPool       The pool.
 * @param   pPage       The cached page.
 */
static void pgmPoolCacheFlushPage(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    Log3(("pgmPoolCacheFlushPage: %RGp\n", pPage->GCPhys));

    /*
     * Remove the page from the hash.
     */
    if (pPage->fCached)
    {
        pPage->fCached = false;
        pgmPoolHashRemove(pPool, pPage);
    }
    else
        Assert(pPage->iNext == NIL_PGMPOOL_IDX);

    /*
     * Remove it from the age list.
     */
    if (pPage->iAgeNext != NIL_PGMPOOL_IDX)
        pPool->aPages[pPage->iAgeNext].iAgePrev = pPage->iAgePrev;
    else
        pPool->iAgeTail = pPage->iAgePrev;
    if (pPage->iAgePrev != NIL_PGMPOOL_IDX)
        pPool->aPages[pPage->iAgePrev].iAgeNext = pPage->iAgeNext;
    else
        pPool->iAgeHead = pPage->iAgeNext;
    pPage->iAgeNext = NIL_PGMPOOL_IDX;
    pPage->iAgePrev = NIL_PGMPOOL_IDX;
}

#endif /* PGMPOOL_WITH_CACHE */
#ifdef PGMPOOL_WITH_MONITORING

/**
 * Looks for pages sharing the monitor.
 *
 * @returns Pointer to the head page.
 * @returns NULL if not found.
 * @param   pPool       The Pool
 * @param   pNewPage    The page which is going to be monitored.
 */
static PPGMPOOLPAGE pgmPoolMonitorGetPageByGCPhys(PPGMPOOL pPool, PPGMPOOLPAGE pNewPage)
{
#ifdef PGMPOOL_WITH_CACHE
    /*
     * Look up the GCPhys in the hash.
     */
    RTGCPHYS GCPhys = pNewPage->GCPhys & ~(RTGCPHYS)(PAGE_SIZE - 1);
    unsigned i = pPool->aiHash[PGMPOOL_HASH(GCPhys)];
    if (i == NIL_PGMPOOL_IDX)
        return NULL;
    do
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[i];
        if (    pPage->GCPhys - GCPhys < PAGE_SIZE
            &&  pPage != pNewPage)
        {
            switch (pPage->enmKind)
            {
                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
                case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
                case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
                case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
                case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
                case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
                case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
                case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
                case PGMPOOLKIND_64BIT_PML4:
                case PGMPOOLKIND_32BIT_PD:
                case PGMPOOLKIND_PAE_PDPT:
                {
                    /* find the head */
                    while (pPage->iMonitoredPrev != NIL_PGMPOOL_IDX)
                    {
                        Assert(pPage->iMonitoredPrev != pPage->idx);
                        pPage = &pPool->aPages[pPage->iMonitoredPrev];
                    }
                    return pPage;
                }

                /* ignore, no monitoring. */
                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
                case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
                case PGMPOOLKIND_PAE_PT_FOR_PHYS:
                case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
                case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
                case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
                case PGMPOOLKIND_EPT_PD_FOR_PHYS:
                case PGMPOOLKIND_EPT_PT_FOR_PHYS:
                case PGMPOOLKIND_ROOT_NESTED:
                case PGMPOOLKIND_PAE_PD_PHYS:
                case PGMPOOLKIND_PAE_PDPT_PHYS:
                case PGMPOOLKIND_32BIT_PD_PHYS:
                case PGMPOOLKIND_PAE_PDPT_FOR_32BIT:
                    break;
                default:
                    AssertFatalMsgFailed(("enmKind=%d idx=%d\n", pPage->enmKind, pPage->idx));
            }
        }

        /* next */
        i = pPage->iNext;
    } while (i != NIL_PGMPOOL_IDX);
#endif
    return NULL;
}


/**
 * Enabled write monitoring of a guest page.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @param   pPool       The pool.
 * @param   pPage       The cached page.
 */
static int pgmPoolMonitorInsert(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    LogFlow(("pgmPoolMonitorInsert %RGp\n", pPage->GCPhys & ~(RTGCPHYS)(PAGE_SIZE - 1)));

    /*
     * Filter out the relevant kinds.
     */
    switch (pPage->enmKind)
    {
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
        case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
        case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
        case PGMPOOLKIND_64BIT_PML4:
        case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
        case PGMPOOLKIND_32BIT_PD:
        case PGMPOOLKIND_PAE_PDPT:
            break;

        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
        case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
        case PGMPOOLKIND_PAE_PT_FOR_PHYS:
        case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
        case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_EPT_PD_FOR_PHYS:
        case PGMPOOLKIND_EPT_PT_FOR_PHYS:
        case PGMPOOLKIND_ROOT_NESTED:
            /* Nothing to monitor here. */
            return VINF_SUCCESS;

        case PGMPOOLKIND_32BIT_PD_PHYS:
        case PGMPOOLKIND_PAE_PDPT_PHYS:
        case PGMPOOLKIND_PAE_PD_PHYS:
        case PGMPOOLKIND_PAE_PDPT_FOR_32BIT:
            /* Nothing to monitor here. */
            return VINF_SUCCESS;
#ifdef PGMPOOL_WITH_MIXED_PT_CR3
            break;
#else
        case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
#endif
        default:
            AssertFatalMsgFailed(("This can't happen! enmKind=%d\n", pPage->enmKind));
    }

    /*
     * Install handler.
     */
    int rc;
    PPGMPOOLPAGE pPageHead = pgmPoolMonitorGetPageByGCPhys(pPool, pPage);
    if (pPageHead)
    {
        Assert(pPageHead != pPage); Assert(pPageHead->iMonitoredNext != pPage->idx);
        Assert(pPageHead->iMonitoredPrev != pPage->idx);
        pPage->iMonitoredPrev = pPageHead->idx;
        pPage->iMonitoredNext = pPageHead->iMonitoredNext;
        if (pPageHead->iMonitoredNext != NIL_PGMPOOL_IDX)
            pPool->aPages[pPageHead->iMonitoredNext].iMonitoredPrev = pPage->idx;
        pPageHead->iMonitoredNext = pPage->idx;
        rc = VINF_SUCCESS;
    }
    else
    {
        Assert(pPage->iMonitoredNext == NIL_PGMPOOL_IDX); Assert(pPage->iMonitoredPrev == NIL_PGMPOOL_IDX);
        PVM pVM = pPool->CTX_SUFF(pVM);
        const RTGCPHYS GCPhysPage = pPage->GCPhys & ~(RTGCPHYS)(PAGE_SIZE - 1);
        rc = PGMHandlerPhysicalRegisterEx(pVM, PGMPHYSHANDLERTYPE_PHYSICAL_WRITE,
                                          GCPhysPage, GCPhysPage + (PAGE_SIZE - 1),
                                          pPool->pfnAccessHandlerR3, MMHyperCCToR3(pVM, pPage),
                                          pPool->pfnAccessHandlerR0, MMHyperCCToR0(pVM, pPage),
                                          pPool->pfnAccessHandlerRC, MMHyperCCToRC(pVM, pPage),
                                          pPool->pszAccessHandler);
        /** @todo we should probably deal with out-of-memory conditions here, but for now increasing
         * the heap size should suffice. */
        AssertFatalRC(rc);
        Assert(!(pVM->pgm.s.fGlobalSyncFlags & PGM_GLOBAL_SYNC_CLEAR_PGM_POOL) || VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3));
    }
    pPage->fMonitored = true;
    return rc;
}


/**
 * Disables write monitoring of a guest page.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @param   pPool       The pool.
 * @param   pPage       The cached page.
 */
static int pgmPoolMonitorFlush(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    /*
     * Filter out the relevant kinds.
     */
    switch (pPage->enmKind)
    {
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
        case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
        case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
        case PGMPOOLKIND_64BIT_PML4:
        case PGMPOOLKIND_32BIT_PD:
        case PGMPOOLKIND_PAE_PDPT:
        case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
            break;

        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
        case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
        case PGMPOOLKIND_PAE_PT_FOR_PHYS:
        case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
        case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_EPT_PD_FOR_PHYS:
        case PGMPOOLKIND_EPT_PT_FOR_PHYS:
        case PGMPOOLKIND_ROOT_NESTED:
        case PGMPOOLKIND_PAE_PD_PHYS:
        case PGMPOOLKIND_PAE_PDPT_PHYS:
        case PGMPOOLKIND_32BIT_PD_PHYS:
            /* Nothing to monitor here. */
            return VINF_SUCCESS;

#ifdef PGMPOOL_WITH_MIXED_PT_CR3
            break;
#endif
        default:
            AssertFatalMsgFailed(("This can't happen! enmKind=%d\n", pPage->enmKind));
    }

    /*
     * Remove the page from the monitored list or uninstall it if last.
     */
    const PVM pVM = pPool->CTX_SUFF(pVM);
    int rc;
    if (    pPage->iMonitoredNext != NIL_PGMPOOL_IDX
        ||  pPage->iMonitoredPrev != NIL_PGMPOOL_IDX)
    {
        if (pPage->iMonitoredPrev == NIL_PGMPOOL_IDX)
        {
            PPGMPOOLPAGE pNewHead = &pPool->aPages[pPage->iMonitoredNext];
            pNewHead->iMonitoredPrev = NIL_PGMPOOL_IDX;
            rc = PGMHandlerPhysicalChangeCallbacks(pVM, pPage->GCPhys & ~(RTGCPHYS)(PAGE_SIZE - 1),
                                                   pPool->pfnAccessHandlerR3, MMHyperCCToR3(pVM, pNewHead),
                                                   pPool->pfnAccessHandlerR0, MMHyperCCToR0(pVM, pNewHead),
                                                   pPool->pfnAccessHandlerRC, MMHyperCCToRC(pVM, pNewHead),
                                                   pPool->pszAccessHandler);
            AssertFatalRCSuccess(rc);
            pPage->iMonitoredNext = NIL_PGMPOOL_IDX;
        }
        else
        {
            pPool->aPages[pPage->iMonitoredPrev].iMonitoredNext = pPage->iMonitoredNext;
            if (pPage->iMonitoredNext != NIL_PGMPOOL_IDX)
            {
                pPool->aPages[pPage->iMonitoredNext].iMonitoredPrev = pPage->iMonitoredPrev;
                pPage->iMonitoredNext = NIL_PGMPOOL_IDX;
            }
            pPage->iMonitoredPrev = NIL_PGMPOOL_IDX;
            rc = VINF_SUCCESS;
        }
    }
    else
    {
        rc = PGMHandlerPhysicalDeregister(pVM, pPage->GCPhys & ~(RTGCPHYS)(PAGE_SIZE - 1));
        AssertFatalRC(rc);
        AssertMsg(!(pVM->pgm.s.fGlobalSyncFlags & PGM_GLOBAL_SYNC_CLEAR_PGM_POOL) || VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3),
                  ("%#x %#x\n", pVM->pgm.s.fGlobalSyncFlags, pVM->fForcedActions));
    }
    pPage->fMonitored = false;

    /*
     * Remove it from the list of modified pages (if in it).
     */
    pgmPoolMonitorModifiedRemove(pPool, pPage);

    return rc;
}


/**
 * Inserts the page into the list of modified pages.
 *
 * @param   pPool   The pool.
 * @param   pPage   The page.
 */
void pgmPoolMonitorModifiedInsert(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    Log3(("pgmPoolMonitorModifiedInsert: idx=%d\n", pPage->idx));
    AssertMsg(   pPage->iModifiedNext == NIL_PGMPOOL_IDX
              && pPage->iModifiedPrev == NIL_PGMPOOL_IDX
              && pPool->iModifiedHead != pPage->idx,
              ("Next=%d Prev=%d idx=%d cModifications=%d Head=%d cModifiedPages=%d\n",
               pPage->iModifiedNext, pPage->iModifiedPrev, pPage->idx, pPage->cModifications,
               pPool->iModifiedHead, pPool->cModifiedPages));

    pPage->iModifiedNext = pPool->iModifiedHead;
    if (pPool->iModifiedHead != NIL_PGMPOOL_IDX)
        pPool->aPages[pPool->iModifiedHead].iModifiedPrev = pPage->idx;
    pPool->iModifiedHead = pPage->idx;
    pPool->cModifiedPages++;
#ifdef VBOX_WITH_STATISTICS
    if (pPool->cModifiedPages > pPool->cModifiedPagesHigh)
        pPool->cModifiedPagesHigh = pPool->cModifiedPages;
#endif
}


/**
 * Removes the page from the list of modified pages and resets the
 * moficiation counter.
 *
 * @param   pPool   The pool.
 * @param   pPage   The page which is believed to be in the list of modified pages.
 */
static void pgmPoolMonitorModifiedRemove(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    Log3(("pgmPoolMonitorModifiedRemove: idx=%d cModifications=%d\n", pPage->idx, pPage->cModifications));
    if (pPool->iModifiedHead == pPage->idx)
    {
        Assert(pPage->iModifiedPrev == NIL_PGMPOOL_IDX);
        pPool->iModifiedHead = pPage->iModifiedNext;
        if (pPage->iModifiedNext != NIL_PGMPOOL_IDX)
        {
            pPool->aPages[pPage->iModifiedNext].iModifiedPrev = NIL_PGMPOOL_IDX;
            pPage->iModifiedNext = NIL_PGMPOOL_IDX;
        }
        pPool->cModifiedPages--;
    }
    else if (pPage->iModifiedPrev != NIL_PGMPOOL_IDX)
    {
        pPool->aPages[pPage->iModifiedPrev].iModifiedNext = pPage->iModifiedNext;
        if (pPage->iModifiedNext != NIL_PGMPOOL_IDX)
        {
            pPool->aPages[pPage->iModifiedNext].iModifiedPrev = pPage->iModifiedPrev;
            pPage->iModifiedNext = NIL_PGMPOOL_IDX;
        }
        pPage->iModifiedPrev = NIL_PGMPOOL_IDX;
        pPool->cModifiedPages--;
    }
    else
        Assert(pPage->iModifiedPrev == NIL_PGMPOOL_IDX);
    pPage->cModifications = 0;
}


/**
 * Zaps the list of modified pages, resetting their modification counters in the process.
 *
 * @param   pVM     The VM handle.
 */
void pgmPoolMonitorModifiedClearAll(PVM pVM)
{
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    LogFlow(("pgmPoolMonitorModifiedClearAll: cModifiedPages=%d\n", pPool->cModifiedPages));

    unsigned cPages = 0; NOREF(cPages);
    uint16_t idx = pPool->iModifiedHead;
    pPool->iModifiedHead = NIL_PGMPOOL_IDX;
    while (idx != NIL_PGMPOOL_IDX)
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[idx];
        idx = pPage->iModifiedNext;
        pPage->iModifiedNext = NIL_PGMPOOL_IDX;
        pPage->iModifiedPrev = NIL_PGMPOOL_IDX;
        pPage->cModifications = 0;
        Assert(++cPages);
    }
    AssertMsg(cPages == pPool->cModifiedPages, ("%d != %d\n", cPages, pPool->cModifiedPages));
    pPool->cModifiedPages = 0;
}


#ifdef IN_RING3
/**
 * Clear all shadow pages and clear all modification counters.
 *
 * @param   pVM     The VM handle.
 * @remark  Should only be used when monitoring is available, thus placed in
 *          the PGMPOOL_WITH_MONITORING #ifdef.
 */
void pgmPoolClearAll(PVM pVM)
{
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    STAM_PROFILE_START(&pPool->StatClearAll, c);
    LogFlow(("pgmPoolClearAll: cUsedPages=%d\n", pPool->cUsedPages));

    /*
     * Iterate all the pages until we've encountered all that in use.
     * This is simple but not quite optimal solution.
     */
    unsigned cModifiedPages = 0; NOREF(cModifiedPages);
    unsigned cLeft = pPool->cUsedPages;
    unsigned iPage = pPool->cCurPages;
    while (--iPage >= PGMPOOL_IDX_FIRST)
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[iPage];
        if (pPage->GCPhys != NIL_RTGCPHYS)
        {
            switch (pPage->enmKind)
            {
                /*
                 * We only care about shadow page tables.
                 */
                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
                case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
                case PGMPOOLKIND_PAE_PT_FOR_PHYS:
                {
#ifdef PGMPOOL_WITH_USER_TRACKING
                    if (pPage->cPresent)
#endif
                    {
                        void *pvShw = PGMPOOL_PAGE_2_PTR(pPool->CTX_SUFF(pVM), pPage);
                        STAM_PROFILE_START(&pPool->StatZeroPage, z);
                        ASMMemZeroPage(pvShw);
                        STAM_PROFILE_STOP(&pPool->StatZeroPage, z);
#ifdef PGMPOOL_WITH_USER_TRACKING
                        pPage->cPresent = 0;
                        pPage->iFirstPresent = ~0;
#endif
                    }
                }
                /* fall thru */

                default:
                    Assert(!pPage->cModifications || ++cModifiedPages);
                    Assert(pPage->iModifiedNext == NIL_PGMPOOL_IDX || pPage->cModifications);
                    Assert(pPage->iModifiedPrev == NIL_PGMPOOL_IDX || pPage->cModifications);
                    pPage->iModifiedNext = NIL_PGMPOOL_IDX;
                    pPage->iModifiedPrev = NIL_PGMPOOL_IDX;
                    pPage->cModifications = 0;
                    break;

            }
            if (!--cLeft)
                break;
        }
    }

    /* swipe the special pages too. */
    for (iPage = PGMPOOL_IDX_FIRST_SPECIAL; iPage < PGMPOOL_IDX_FIRST; iPage++)
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[iPage];
        if (pPage->GCPhys != NIL_RTGCPHYS)
        {
            Assert(!pPage->cModifications || ++cModifiedPages);
            Assert(pPage->iModifiedNext == NIL_PGMPOOL_IDX || pPage->cModifications);
            Assert(pPage->iModifiedPrev == NIL_PGMPOOL_IDX || pPage->cModifications);
            pPage->iModifiedNext = NIL_PGMPOOL_IDX;
            pPage->iModifiedPrev = NIL_PGMPOOL_IDX;
            pPage->cModifications = 0;
        }
    }

#ifndef DEBUG_michael
    AssertMsg(cModifiedPages == pPool->cModifiedPages, ("%d != %d\n", cModifiedPages, pPool->cModifiedPages));
#endif
    pPool->iModifiedHead = NIL_PGMPOOL_IDX;
    pPool->cModifiedPages = 0;

#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    /*
     * Clear all the GCPhys links and rebuild the phys ext free list.
     */
    for (PPGMRAMRANGE pRam = pPool->CTX_SUFF(pVM)->pgm.s.CTX_SUFF(pRamRanges);
         pRam;
         pRam = pRam->CTX_SUFF(pNext))
    {
        unsigned iPage = pRam->cb >> PAGE_SHIFT;
        while (iPage-- > 0)
            PGM_PAGE_SET_TRACKING(&pRam->aPages[iPage], 0);
    }

    pPool->iPhysExtFreeHead = 0;
    PPGMPOOLPHYSEXT paPhysExts = pPool->CTX_SUFF(paPhysExts);
    const unsigned cMaxPhysExts = pPool->cMaxPhysExts;
    for (unsigned i = 0; i < cMaxPhysExts; i++)
    {
        paPhysExts[i].iNext = i + 1;
        paPhysExts[i].aidx[0] = NIL_PGMPOOL_IDX;
        paPhysExts[i].aidx[1] = NIL_PGMPOOL_IDX;
        paPhysExts[i].aidx[2] = NIL_PGMPOOL_IDX;
    }
    paPhysExts[cMaxPhysExts - 1].iNext = NIL_PGMPOOL_PHYSEXT_INDEX;
#endif


    pPool->cPresent = 0;
    STAM_PROFILE_STOP(&pPool->StatClearAll, c);
}
#endif /* IN_RING3 */


/**
 * Handle SyncCR3 pool tasks
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if successfully added.
 * @retval  VINF_PGM_SYNC_CR3 is it needs to be deferred to ring 3 (GC only)
 * @param   pVM     The VM handle.
 * @remark  Should only be used when monitoring is available, thus placed in
 *          the PGMPOOL_WITH_MONITORING #ifdef.
 */
int pgmPoolSyncCR3(PVM pVM)
{
    LogFlow(("pgmPoolSyncCR3\n"));
    /*
     * When monitoring shadowed pages, we reset the modification counters on CR3 sync.
     * Occasionally we will have to clear all the shadow page tables because we wanted
     * to monitor a page which was mapped by too many shadowed page tables. This operation
     * sometimes refered to as a 'lightweight flush'.
     */
    if (!(pVM->pgm.s.fGlobalSyncFlags & PGM_GLOBAL_SYNC_CLEAR_PGM_POOL))
        pgmPoolMonitorModifiedClearAll(pVM);
    else
    {
# ifdef IN_RING3 /* Don't flush in ring-0 or raw mode, it's taking too long. */
        /** @todo SMP support! */
        Assert(pVM->cCPUs == 1);
        pVM->pgm.s.fGlobalSyncFlags &= ~PGM_GLOBAL_SYNC_CLEAR_PGM_POOL;
        pgmPoolClearAll(pVM);
# else  /* !IN_RING3 */
        LogFlow(("SyncCR3: PGM_GLOBAL_SYNC_CLEAR_PGM_POOL is set -> VINF_PGM_SYNC_CR3\n"));
        VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3); /** @todo no need to do global sync, right? */
        return VINF_PGM_SYNC_CR3;
# endif /* !IN_RING3 */
    }
    return VINF_SUCCESS;
}

#endif /* PGMPOOL_WITH_MONITORING */
#ifdef PGMPOOL_WITH_USER_TRACKING

/**
 * Frees up at least one user entry.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if successfully added.
 * @retval  VERR_PGM_POOL_FLUSHED if the pool was flushed.
 * @param   pPool       The pool.
 * @param   iUser       The user index.
 */
static int pgmPoolTrackFreeOneUser(PPGMPOOL pPool, uint16_t iUser)
{
    STAM_COUNTER_INC(&pPool->StatTrackFreeUpOneUser);
#ifdef PGMPOOL_WITH_CACHE
    /*
     * Just free cached pages in a braindead fashion.
     */
    /** @todo walk the age list backwards and free the first with usage. */
    int rc = VINF_SUCCESS;
    do
    {
        int rc2 = pgmPoolCacheFreeOne(pPool, iUser);
        if (RT_FAILURE(rc2) && rc == VINF_SUCCESS)
            rc = rc2;
    } while (pPool->iUserFreeHead == NIL_PGMPOOL_USER_INDEX);
    return rc;
#else
    /*
     * Lazy approach.
     */
    /* @todo This path no longer works (CR3 root pages will be flushed)!! */
    AssertCompileFailed();
    Assert(!CPUMIsGuestInLongMode(pVM));
    pgmPoolFlushAllInt(pPool);
    return VERR_PGM_POOL_FLUSHED;
#endif
}


/**
 * Inserts a page into the cache.
 *
 * This will create user node for the page, insert it into the GCPhys
 * hash, and insert it into the age list.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if successfully added.
 * @retval  VERR_PGM_POOL_FLUSHED if the pool was flushed.
 * @param   pPool       The pool.
 * @param   pPage       The cached page.
 * @param   GCPhys      The GC physical address of the page we're gonna shadow.
 * @param   iUser       The user index.
 * @param   iUserTable  The user table index.
 */
DECLINLINE(int) pgmPoolTrackInsert(PPGMPOOL pPool, PPGMPOOLPAGE pPage, RTGCPHYS GCPhys, uint16_t iUser, uint32_t iUserTable)
{
    int rc = VINF_SUCCESS;
    PPGMPOOLUSER paUsers = pPool->CTX_SUFF(paUsers);

    LogFlow(("pgmPoolTrackInsert GCPhys=%RGp iUser %x iUserTable %x\n", GCPhys, iUser, iUserTable));

#ifdef VBOX_STRICT
    /*
     * Check that the entry doesn't already exists.
     */
    if (pPage->iUserHead != NIL_PGMPOOL_USER_INDEX)
    {
        uint16_t i = pPage->iUserHead;
        do
        {
            Assert(i < pPool->cMaxUsers);
            AssertMsg(paUsers[i].iUser != iUser || paUsers[i].iUserTable != iUserTable, ("%x %x vs new %x %x\n", paUsers[i].iUser, paUsers[i].iUserTable, iUser, iUserTable));
            i = paUsers[i].iNext;
        } while (i != NIL_PGMPOOL_USER_INDEX);
    }
#endif

    /*
     * Find free a user node.
     */
    uint16_t i = pPool->iUserFreeHead;
    if (i == NIL_PGMPOOL_USER_INDEX)
    {
        int rc = pgmPoolTrackFreeOneUser(pPool, iUser);
        if (RT_FAILURE(rc))
            return rc;
        i = pPool->iUserFreeHead;
    }

    /*
     * Unlink the user node from the free list,
     * initialize and insert it into the user list.
     */
    pPool->iUserFreeHead = paUsers[i].iNext;
    paUsers[i].iNext = NIL_PGMPOOL_USER_INDEX;
    paUsers[i].iUser = iUser;
    paUsers[i].iUserTable = iUserTable;
    pPage->iUserHead = i;

    /*
     * Insert into cache and enable monitoring of the guest page if enabled.
     *
     * Until we implement caching of all levels, including the CR3 one, we'll
     * have to make sure we don't try monitor & cache any recursive reuse of
     * a monitored CR3 page. Because all windows versions are doing this we'll
     * have to be able to do combined access monitoring, CR3 + PT and
     * PD + PT (guest PAE).
     *
     * Update:
     * We're now cooperating with the CR3 monitor if an uncachable page is found.
     */
#if defined(PGMPOOL_WITH_MONITORING) || defined(PGMPOOL_WITH_CACHE)
# ifdef PGMPOOL_WITH_MIXED_PT_CR3
    const bool fCanBeMonitored = true;
# else
    bool fCanBeMonitored = pPool->CTX_SUFF(pVM)->pgm.s.GCPhysGstCR3Monitored == NIL_RTGCPHYS
                         || (GCPhys & X86_PTE_PAE_PG_MASK) != (pPool->CTX_SUFF(pVM)->pgm.s.GCPhysGstCR3Monitored & X86_PTE_PAE_PG_MASK)
                         || pgmPoolIsBigPage((PGMPOOLKIND)pPage->enmKind);
# endif
# ifdef PGMPOOL_WITH_CACHE
    pgmPoolCacheInsert(pPool, pPage, fCanBeMonitored); /* This can be expanded. */
# endif
    if (fCanBeMonitored)
    {
# ifdef PGMPOOL_WITH_MONITORING
        rc = pgmPoolMonitorInsert(pPool, pPage);
        AssertRC(rc);
    }
# endif
#endif /* PGMPOOL_WITH_MONITORING */
    return rc;
}


# ifdef PGMPOOL_WITH_CACHE /* (only used when the cache is enabled.) */
/**
 * Adds a user reference to a page.
 *
 * This will move the page to the head of the
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if successfully added.
 * @retval  VERR_PGM_POOL_FLUSHED if the pool was flushed.
 * @param   pPool       The pool.
 * @param   pPage       The cached page.
 * @param   iUser       The user index.
 * @param   iUserTable  The user table.
 */
static int pgmPoolTrackAddUser(PPGMPOOL pPool, PPGMPOOLPAGE pPage, uint16_t iUser, uint32_t iUserTable)
{
    PPGMPOOLUSER paUsers = pPool->CTX_SUFF(paUsers);

    Log3(("pgmPoolTrackAddUser GCPhys = %RGp iUser %x iUserTable %x\n", pPage->GCPhys, iUser, iUserTable));
#  ifdef VBOX_STRICT
    /*
     * Check that the entry doesn't already exists.
     */
    if (pPage->iUserHead != NIL_PGMPOOL_USER_INDEX)
    {
        uint16_t i = pPage->iUserHead;
        do
        {
            Assert(i < pPool->cMaxUsers);
            AssertMsg(paUsers[i].iUser != iUser || paUsers[i].iUserTable != iUserTable, ("%x %x vs new %x %x\n", paUsers[i].iUser, paUsers[i].iUserTable, iUser, iUserTable));
            i = paUsers[i].iNext;
        } while (i != NIL_PGMPOOL_USER_INDEX);
    }
#  endif

    /*
     * Allocate a user node.
     */
    uint16_t i = pPool->iUserFreeHead;
    if (i == NIL_PGMPOOL_USER_INDEX)
    {
        int rc = pgmPoolTrackFreeOneUser(pPool, iUser);
        if (RT_FAILURE(rc))
            return rc;
        i = pPool->iUserFreeHead;
    }
    pPool->iUserFreeHead = paUsers[i].iNext;

    /*
     * Initialize the user node and insert it.
     */
    paUsers[i].iNext = pPage->iUserHead;
    paUsers[i].iUser = iUser;
    paUsers[i].iUserTable = iUserTable;
    pPage->iUserHead = i;

#  ifdef PGMPOOL_WITH_CACHE
    /*
     * Tell the cache to update its replacement stats for this page.
     */
    pgmPoolCacheUsed(pPool, pPage);
#  endif
    return VINF_SUCCESS;
}
# endif /* PGMPOOL_WITH_CACHE */


/**
 * Frees a user record associated with a page.
 *
 * This does not clear the entry in the user table, it simply replaces the
 * user record to the chain of free records.
 *
 * @param   pPool       The pool.
 * @param   HCPhys      The HC physical address of the shadow page.
 * @param   iUser       The shadow page pool index of the user table.
 * @param   iUserTable  The index into the user table (shadowed).
 */
static void pgmPoolTrackFreeUser(PPGMPOOL pPool, PPGMPOOLPAGE pPage, uint16_t iUser, uint32_t iUserTable)
{
    /*
     * Unlink and free the specified user entry.
     */
    PPGMPOOLUSER paUsers = pPool->CTX_SUFF(paUsers);

    Log3(("pgmPoolTrackFreeUser %RGp %x %x\n", pPage->GCPhys, iUser, iUserTable));
    /* Special: For PAE and 32-bit paging, there is usually no more than one user. */
    uint16_t i = pPage->iUserHead;
    if (    i != NIL_PGMPOOL_USER_INDEX
        &&  paUsers[i].iUser == iUser
        &&  paUsers[i].iUserTable == iUserTable)
    {
        pPage->iUserHead = paUsers[i].iNext;

        paUsers[i].iUser = NIL_PGMPOOL_IDX;
        paUsers[i].iNext = pPool->iUserFreeHead;
        pPool->iUserFreeHead = i;
        return;
    }

    /* General: Linear search. */
    uint16_t iPrev = NIL_PGMPOOL_USER_INDEX;
    while (i != NIL_PGMPOOL_USER_INDEX)
    {
        if (    paUsers[i].iUser == iUser
            &&  paUsers[i].iUserTable == iUserTable)
        {
            if (iPrev != NIL_PGMPOOL_USER_INDEX)
                paUsers[iPrev].iNext = paUsers[i].iNext;
            else
                pPage->iUserHead = paUsers[i].iNext;

            paUsers[i].iUser = NIL_PGMPOOL_IDX;
            paUsers[i].iNext = pPool->iUserFreeHead;
            pPool->iUserFreeHead = i;
            return;
        }
        iPrev = i;
        i = paUsers[i].iNext;
    }

    /* Fatal: didn't find it */
    AssertFatalMsgFailed(("Didn't find the user entry! iUser=%#x iUserTable=%#x GCPhys=%RGp\n",
                          iUser, iUserTable, pPage->GCPhys));
}


/**
 * Gets the entry size of a shadow table.
 *
 * @param   enmKind     The kind of page.
 *
 * @returns The size of the entry in bytes. That is, 4 or 8.
 * @returns If the kind is not for a table, an assertion is raised and 0 is
 *          returned.
 */
DECLINLINE(unsigned) pgmPoolTrackGetShadowEntrySize(PGMPOOLKIND enmKind)
{
    switch (enmKind)
    {
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_32BIT_PD:
        case PGMPOOLKIND_32BIT_PD_PHYS:
            return 4;

        case PGMPOOLKIND_PAE_PT_FOR_PHYS:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
        case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
        case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
        case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
        case PGMPOOLKIND_64BIT_PML4:
        case PGMPOOLKIND_PAE_PDPT:
        case PGMPOOLKIND_ROOT_NESTED:
        case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
        case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_EPT_PD_FOR_PHYS:
        case PGMPOOLKIND_EPT_PT_FOR_PHYS:
        case PGMPOOLKIND_PAE_PD_PHYS:
        case PGMPOOLKIND_PAE_PDPT_PHYS:
            return 8;

        default:
            AssertFatalMsgFailed(("enmKind=%d\n", enmKind));
    }
}


/**
 * Gets the entry size of a guest table.
 *
 * @param   enmKind     The kind of page.
 *
 * @returns The size of the entry in bytes. That is, 0, 4 or 8.
 * @returns If the kind is not for a table, an assertion is raised and 0 is
 *          returned.
 */
DECLINLINE(unsigned) pgmPoolTrackGetGuestEntrySize(PGMPOOLKIND enmKind)
{
    switch (enmKind)
    {
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_32BIT_PD:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
            return 4;

        case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
        case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
        case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
        case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
        case PGMPOOLKIND_64BIT_PML4:
        case PGMPOOLKIND_PAE_PDPT:
            return 8;

        case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
        case PGMPOOLKIND_PAE_PT_FOR_PHYS:
        case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
        case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_EPT_PD_FOR_PHYS:
        case PGMPOOLKIND_EPT_PT_FOR_PHYS:
        case PGMPOOLKIND_ROOT_NESTED:
        case PGMPOOLKIND_PAE_PD_PHYS:
        case PGMPOOLKIND_PAE_PDPT_PHYS:
        case PGMPOOLKIND_32BIT_PD_PHYS:
            /** @todo can we return 0? (nobody is calling this...) */
            AssertFailed();
            return 0;

        default:
            AssertFatalMsgFailed(("enmKind=%d\n", enmKind));
    }
}

#ifdef PGMPOOL_WITH_GCPHYS_TRACKING

/**
 * Scans one shadow page table for mappings of a physical page.
 *
 * @param   pVM         The VM handle.
 * @param   pPhysPage   The guest page in question.
 * @param   iShw        The shadow page table.
 * @param   cRefs       The number of references made in that PT.
 */
static void pgmPoolTrackFlushGCPhysPTInt(PVM pVM, PCPGMPAGE pPhysPage, uint16_t iShw, uint16_t cRefs)
{
    LogFlow(("pgmPoolTrackFlushGCPhysPT: pPhysPage=%R[pgmpage] iShw=%d cRefs=%d\n", pPhysPage, iShw, cRefs));
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);

    /*
     * Assert sanity.
     */
    Assert(cRefs == 1);
    AssertFatalMsg(iShw < pPool->cCurPages && iShw != NIL_PGMPOOL_IDX, ("iShw=%d\n", iShw));
    PPGMPOOLPAGE pPage = &pPool->aPages[iShw];

    /*
     * Then, clear the actual mappings to the page in the shadow PT.
     */
    switch (pPage->enmKind)
    {
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
        {
            const uint32_t  u32 = PGM_PAGE_GET_HCPHYS(pPhysPage) | X86_PTE_P;
            PX86PT          pPT = (PX86PT)PGMPOOL_PAGE_2_PTR(pVM, pPage);
            for (unsigned i = pPage->iFirstPresent; i < RT_ELEMENTS(pPT->a); i++)
                if ((pPT->a[i].u & (X86_PTE_PG_MASK | X86_PTE_P)) == u32)
                {
                    Log4(("pgmPoolTrackFlushGCPhysPTs: i=%d pte=%RX32 cRefs=%#x\n", i, pPT->a[i], cRefs));
                    pPT->a[i].u = 0;
                    cRefs--;
                    if (!cRefs)
                        return;
                }
#ifdef LOG_ENABLED
            RTLogPrintf("cRefs=%d iFirstPresent=%d cPresent=%d\n", cRefs, pPage->iFirstPresent, pPage->cPresent);
            for (unsigned i = 0; i < RT_ELEMENTS(pPT->a); i++)
                if ((pPT->a[i].u & (X86_PTE_PG_MASK | X86_PTE_P)) == u32)
                {
                    RTLogPrintf("i=%d cRefs=%d\n", i, cRefs--);
                    pPT->a[i].u = 0;
                }
#endif
            AssertFatalMsgFailed(("cRefs=%d iFirstPresent=%d cPresent=%d\n", cRefs, pPage->iFirstPresent, pPage->cPresent));
            break;
        }

        case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
        case PGMPOOLKIND_PAE_PT_FOR_PHYS:
        {
            const uint64_t  u64 = PGM_PAGE_GET_HCPHYS(pPhysPage) | X86_PTE_P;
            PX86PTPAE       pPT = (PX86PTPAE)PGMPOOL_PAGE_2_PTR(pVM, pPage);
            for (unsigned i = pPage->iFirstPresent; i < RT_ELEMENTS(pPT->a); i++)
                if ((pPT->a[i].u & (X86_PTE_PAE_PG_MASK | X86_PTE_P)) == u64)
                {
                    Log4(("pgmPoolTrackFlushGCPhysPTs: i=%d pte=%RX64 cRefs=%#x\n", i, pPT->a[i], cRefs));
                    pPT->a[i].u = 0;
                    cRefs--;
                    if (!cRefs)
                        return;
                }
#ifdef LOG_ENABLED
            RTLogPrintf("cRefs=%d iFirstPresent=%d cPresent=%d\n", cRefs, pPage->iFirstPresent, pPage->cPresent);
            for (unsigned i = 0; i < RT_ELEMENTS(pPT->a); i++)
                if ((pPT->a[i].u & (X86_PTE_PAE_PG_MASK | X86_PTE_P)) == u64)
                {
                    RTLogPrintf("i=%d cRefs=%d\n", i, cRefs--);
                    pPT->a[i].u = 0;
                }
#endif
            AssertFatalMsgFailed(("cRefs=%d iFirstPresent=%d cPresent=%d u64=%RX64\n", cRefs, pPage->iFirstPresent, pPage->cPresent, u64));
            break;
        }

        case PGMPOOLKIND_EPT_PT_FOR_PHYS:
        {
            const uint64_t  u64 = PGM_PAGE_GET_HCPHYS(pPhysPage) | X86_PTE_P;
            PEPTPT          pPT = (PEPTPT)PGMPOOL_PAGE_2_PTR(pVM, pPage);
            for (unsigned i = pPage->iFirstPresent; i < RT_ELEMENTS(pPT->a); i++)
                if ((pPT->a[i].u & (EPT_PTE_PG_MASK | X86_PTE_P)) == u64)
                {
                    Log4(("pgmPoolTrackFlushGCPhysPTs: i=%d pte=%RX64 cRefs=%#x\n", i, pPT->a[i], cRefs));
                    pPT->a[i].u = 0;
                    cRefs--;
                    if (!cRefs)
                        return;
                }
#ifdef LOG_ENABLED
            RTLogPrintf("cRefs=%d iFirstPresent=%d cPresent=%d\n", cRefs, pPage->iFirstPresent, pPage->cPresent);
            for (unsigned i = 0; i < RT_ELEMENTS(pPT->a); i++)
                if ((pPT->a[i].u & (EPT_PTE_PG_MASK | X86_PTE_P)) == u64)
                {
                    RTLogPrintf("i=%d cRefs=%d\n", i, cRefs--);
                    pPT->a[i].u = 0;
                }
#endif
            AssertFatalMsgFailed(("cRefs=%d iFirstPresent=%d cPresent=%d\n", cRefs, pPage->iFirstPresent, pPage->cPresent));
            break;
        }

        default:
            AssertFatalMsgFailed(("enmKind=%d iShw=%d\n", pPage->enmKind, iShw));
    }
}


/**
 * Scans one shadow page table for mappings of a physical page.
 *
 * @param   pVM         The VM handle.
 * @param   pPhysPage   The guest page in question.
 * @param   iShw        The shadow page table.
 * @param   cRefs       The number of references made in that PT.
 */
void pgmPoolTrackFlushGCPhysPT(PVM pVM, PPGMPAGE pPhysPage, uint16_t iShw, uint16_t cRefs)
{
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool); NOREF(pPool);
    LogFlow(("pgmPoolTrackFlushGCPhysPT: pPhysPage=%R[pgmpage] iShw=%d cRefs=%d\n", pPhysPage, iShw, cRefs));
    STAM_PROFILE_START(&pPool->StatTrackFlushGCPhysPT, f);
    pgmPoolTrackFlushGCPhysPTInt(pVM, pPhysPage, iShw, cRefs);
    PGM_PAGE_SET_TRACKING(pPhysPage, 0);
    STAM_PROFILE_STOP(&pPool->StatTrackFlushGCPhysPT, f);
}


/**
 * Flushes a list of shadow page tables mapping the same physical page.
 *
 * @param   pVM         The VM handle.
 * @param   pPhysPage   The guest page in question.
 * @param   iPhysExt    The physical cross reference extent list to flush.
 */
void pgmPoolTrackFlushGCPhysPTs(PVM pVM, PPGMPAGE pPhysPage, uint16_t iPhysExt)
{
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    STAM_PROFILE_START(&pPool->StatTrackFlushGCPhysPTs, f);
    LogFlow(("pgmPoolTrackFlushGCPhysPTs: pPhysPage=%R[pgmpage] iPhysExt\n", pPhysPage, iPhysExt));

    const uint16_t  iPhysExtStart = iPhysExt;
    PPGMPOOLPHYSEXT pPhysExt;
    do
    {
        Assert(iPhysExt < pPool->cMaxPhysExts);
        pPhysExt = &pPool->CTX_SUFF(paPhysExts)[iPhysExt];
        for (unsigned i = 0; i < RT_ELEMENTS(pPhysExt->aidx); i++)
            if (pPhysExt->aidx[i] != NIL_PGMPOOL_IDX)
            {
                pgmPoolTrackFlushGCPhysPTInt(pVM, pPhysPage, pPhysExt->aidx[i], 1);
                pPhysExt->aidx[i] = NIL_PGMPOOL_IDX;
            }

        /* next */
        iPhysExt = pPhysExt->iNext;
    } while (iPhysExt != NIL_PGMPOOL_PHYSEXT_INDEX);

    /* insert the list into the free list and clear the ram range entry. */
    pPhysExt->iNext = pPool->iPhysExtFreeHead;
    pPool->iPhysExtFreeHead = iPhysExtStart;
    PGM_PAGE_SET_TRACKING(pPhysPage, 0);

    STAM_PROFILE_STOP(&pPool->StatTrackFlushGCPhysPTs, f);
}

#endif /* PGMPOOL_WITH_GCPHYS_TRACKING */

/**
 * Flushes all shadow page table mappings of the given guest page.
 *
 * This is typically called when the host page backing the guest one has been
 * replaced or when the page protection was changed due to an access handler.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if all references has been successfully cleared.
 * @retval  VINF_PGM_SYNC_CR3 if we're better off with a CR3 sync and a page
 *          pool cleaning. FF and sync flags are set.
 *
 * @param   pVM         The VM handle.
 * @param   pPhysPage   The guest page in question.
 * @param   pfFlushTLBs This is set to @a true if the shadow TLBs should be
 *                      flushed, it is NOT touched if this isn't necessary.
 *                      The caller MUST initialized this to @a false.
 */
int pgmPoolTrackFlushGCPhys(PVM pVM, PPGMPAGE pPhysPage, bool *pfFlushTLBs)
{
    int rc = VINF_SUCCESS;
#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    const uint16_t u16 = PGM_PAGE_GET_TRACKING(pPhysPage);
    if (u16)
    {
        /*
         * The zero page is currently screwing up the tracking and we'll
         * have to flush the whole shebang. Unless VBOX_WITH_NEW_LAZY_PAGE_ALLOC
         * is defined, zero pages won't normally be mapped. Some kind of solution
         * will be needed for this problem of course, but it will have to wait...
         */
        if (PGM_PAGE_IS_ZERO(pPhysPage))
            rc = VINF_PGM_GCPHYS_ALIASED;
        else
        {
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
            /* Start a subset here because pgmPoolTrackFlushGCPhysPTsSlow and
               pgmPoolTrackFlushGCPhysPTs will/may kill the pool otherwise. */
            PVMCPU pVCpu = VMMGetCpu(pVM);
            uint32_t iPrevSubset = PGMDynMapPushAutoSubset(pVCpu);
# endif

            if (PGMPOOL_TD_GET_CREFS(u16) != PGMPOOL_TD_CREFS_PHYSEXT)
                pgmPoolTrackFlushGCPhysPT(pVM,
                                          pPhysPage,
                                          PGMPOOL_TD_GET_IDX(u16),
                                          PGMPOOL_TD_GET_CREFS(u16));
            else if (u16 != PGMPOOL_TD_MAKE(PGMPOOL_TD_CREFS_PHYSEXT, PGMPOOL_TD_IDX_OVERFLOWED))
                pgmPoolTrackFlushGCPhysPTs(pVM, pPhysPage, PGMPOOL_TD_GET_IDX(u16));
            else
                rc = pgmPoolTrackFlushGCPhysPTsSlow(pVM, pPhysPage);
            *pfFlushTLBs = true;

# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
            PGMDynMapPopAutoSubset(pVCpu, iPrevSubset);
# endif
        }
    }

#elif defined(PGMPOOL_WITH_CACHE)
    if (PGM_PAGE_IS_ZERO(pPhysPage))
        rc = VINF_PGM_GCPHYS_ALIASED;
    else
    {
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
        /* Start a subset here because pgmPoolTrackFlushGCPhysPTsSlow kill the pool otherwise. */
        PVMCPU pVCpu = VMMGetCpu(pVM);
        uint32_t iPrevSubset = PGMDynMapPushAutoSubset(pVCpu);
# endif
        rc = pgmPoolTrackFlushGCPhysPTsSlow(pVM, pPhysPage);
        if (rc == VINF_SUCCESS)
            *pfFlushTLBs = true;
    }

# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PGMDynMapPopAutoSubset(pVCpu, iPrevSubset);
# endif

#else
    rc = VINF_PGM_GCPHYS_ALIASED;
#endif

    if (rc == VINF_PGM_GCPHYS_ALIASED)
    {
        pVM->pgm.s.fGlobalSyncFlags |= PGM_GLOBAL_SYNC_CLEAR_PGM_POOL;
        VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
        rc = VINF_PGM_SYNC_CR3;
    }

    return rc;
}


/**
 * Scans all shadow page tables for mappings of a physical page.
 *
 * This may be slow, but it's most likely more efficient than cleaning
 * out the entire page pool / cache.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if all references has been successfully cleared.
 * @retval  VINF_PGM_GCPHYS_ALIASED if we're better off with a CR3 sync and
 *          a page pool cleaning.
 *
 * @param   pVM         The VM handle.
 * @param   pPhysPage   The guest page in question.
 */
int pgmPoolTrackFlushGCPhysPTsSlow(PVM pVM, PPGMPAGE pPhysPage)
{
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    STAM_PROFILE_START(&pPool->StatTrackFlushGCPhysPTsSlow, s);
    LogFlow(("pgmPoolTrackFlushGCPhysPTsSlow: cUsedPages=%d cPresent=%d pPhysPage=%R[pgmpage]\n",
             pPool->cUsedPages, pPool->cPresent, pPhysPage));

#if 1
    /*
     * There is a limit to what makes sense.
     */
    if (pPool->cPresent > 1024)
    {
        LogFlow(("pgmPoolTrackFlushGCPhysPTsSlow: giving up... (cPresent=%d)\n", pPool->cPresent));
        STAM_PROFILE_STOP(&pPool->StatTrackFlushGCPhysPTsSlow, s);
        return VINF_PGM_GCPHYS_ALIASED;
    }
#endif

    /*
     * Iterate all the pages until we've encountered all that in use.
     * This is simple but not quite optimal solution.
     */
    const uint64_t  u64   = PGM_PAGE_GET_HCPHYS(pPhysPage) | X86_PTE_P;
    const uint32_t  u32   = u64;
    unsigned        cLeft = pPool->cUsedPages;
    unsigned        iPage = pPool->cCurPages;
    while (--iPage >= PGMPOOL_IDX_FIRST)
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[iPage];
        if (pPage->GCPhys != NIL_RTGCPHYS)
        {
            switch (pPage->enmKind)
            {
                /*
                 * We only care about shadow page tables.
                 */
                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
                {
                    unsigned    cPresent = pPage->cPresent;
                    PX86PT      pPT = (PX86PT)PGMPOOL_PAGE_2_PTR(pVM, pPage);
                    for (unsigned i = pPage->iFirstPresent; i < RT_ELEMENTS(pPT->a); i++)
                        if (pPT->a[i].n.u1Present)
                        {
                            if ((pPT->a[i].u & (X86_PTE_PG_MASK | X86_PTE_P)) == u32)
                            {
                                //Log4(("pgmPoolTrackFlushGCPhysPTsSlow: idx=%d i=%d pte=%RX32\n", iPage, i, pPT->a[i]));
                                pPT->a[i].u = 0;
                            }
                            if (!--cPresent)
                                break;
                        }
                    break;
                }

                case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
                case PGMPOOLKIND_PAE_PT_FOR_PHYS:
                {
                    unsigned  cPresent = pPage->cPresent;
                    PX86PTPAE pPT = (PX86PTPAE)PGMPOOL_PAGE_2_PTR(pVM, pPage);
                    for (unsigned i = pPage->iFirstPresent; i < RT_ELEMENTS(pPT->a); i++)
                        if (pPT->a[i].n.u1Present)
                        {
                            if ((pPT->a[i].u & (X86_PTE_PAE_PG_MASK | X86_PTE_P)) == u64)
                            {
                                //Log4(("pgmPoolTrackFlushGCPhysPTsSlow: idx=%d i=%d pte=%RX64\n", iPage, i, pPT->a[i]));
                                pPT->a[i].u = 0;
                            }
                            if (!--cPresent)
                                break;
                        }
                    break;
                }
            }
            if (!--cLeft)
                break;
        }
    }

    PGM_PAGE_SET_TRACKING(pPhysPage, 0);
    STAM_PROFILE_STOP(&pPool->StatTrackFlushGCPhysPTsSlow, s);
    return VINF_SUCCESS;
}


/**
 * Clears the user entry in a user table.
 *
 * This is used to remove all references to a page when flushing it.
 */
static void pgmPoolTrackClearPageUser(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PCPGMPOOLUSER pUser)
{
    Assert(pUser->iUser != NIL_PGMPOOL_IDX);
    Assert(pUser->iUser < pPool->cCurPages);
    uint32_t iUserTable = pUser->iUserTable;

    /*
     * Map the user page.
     */
    PPGMPOOLPAGE pUserPage = &pPool->aPages[pUser->iUser];
    union
    {
        uint64_t       *pau64;
        uint32_t       *pau32;
    } u;
    u.pau64 = (uint64_t *)PGMPOOL_PAGE_2_PTR(pPool->CTX_SUFF(pVM), pUserPage);

    LogFlow(("pgmPoolTrackClearPageUser: clear %x in %s (%RGp) (flushing %s)\n", iUserTable, pgmPoolPoolKindToStr(pUserPage->enmKind), pUserPage->Core.Key, pgmPoolPoolKindToStr(pPage->enmKind)));

    /* Safety precaution in case we change the paging for other modes too in the future. */
    Assert(!pgmPoolIsPageLocked(&pPool->CTX_SUFF(pVM)->pgm.s, pPage));

#ifdef VBOX_STRICT
    /*
     * Some sanity checks.
     */
    switch (pUserPage->enmKind)
    {
        case PGMPOOLKIND_32BIT_PD:
 	    case PGMPOOLKIND_32BIT_PD_PHYS:
            Assert(iUserTable < X86_PG_ENTRIES);
            break;
        case PGMPOOLKIND_PAE_PDPT:
        case PGMPOOLKIND_PAE_PDPT_FOR_32BIT:
        case PGMPOOLKIND_PAE_PDPT_PHYS:
            Assert(iUserTable < 4);
            Assert(!(u.pau64[iUserTable] & PGM_PLXFLAGS_PERMANENT));
            break;
        case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
        case PGMPOOLKIND_PAE_PD_PHYS:
            Assert(iUserTable < X86_PG_PAE_ENTRIES);
            break;
        case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
            Assert(iUserTable < X86_PG_PAE_ENTRIES);
            Assert(!(u.pau64[iUserTable] & PGM_PDFLAGS_MAPPING));
            break;
        case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
            Assert(iUserTable < X86_PG_PAE_ENTRIES);
            Assert(!(u.pau64[iUserTable] & PGM_PLXFLAGS_PERMANENT));
            break;
        case PGMPOOLKIND_64BIT_PML4:
            Assert(!(u.pau64[iUserTable] & PGM_PLXFLAGS_PERMANENT));
            /* GCPhys >> PAGE_SHIFT is the index here */
            break;
        case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
            Assert(iUserTable < X86_PG_PAE_ENTRIES);
            break;

        case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_EPT_PD_FOR_PHYS:
            Assert(iUserTable < X86_PG_PAE_ENTRIES);
            break;

        case PGMPOOLKIND_ROOT_NESTED:
            Assert(iUserTable < X86_PG_PAE_ENTRIES);
            break;

        default:
            AssertMsgFailed(("enmKind=%d\n", pUserPage->enmKind));
            break;
    }
#endif /* VBOX_STRICT */

    /*
     * Clear the entry in the user page.
     */
    switch (pUserPage->enmKind)
    {
        /* 32-bit entries */
        case PGMPOOLKIND_32BIT_PD:
 	    case PGMPOOLKIND_32BIT_PD_PHYS:
            u.pau32[iUserTable] = 0;
            break;

        /* 64-bit entries */
        case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
#if defined(IN_RC)
            /* In 32 bits PAE mode we *must* invalidate the TLB when changing a PDPT entry; the CPU fetches them only during cr3 load, so any
             * non-present PDPT will continue to cause page faults.
             */
            ASMReloadCR3();
#endif
            /* no break */
        case PGMPOOLKIND_PAE_PD_PHYS:
        case PGMPOOLKIND_PAE_PDPT_PHYS:
        case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
        case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
        case PGMPOOLKIND_64BIT_PML4:
        case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
        case PGMPOOLKIND_PAE_PDPT:
        case PGMPOOLKIND_PAE_PDPT_FOR_32BIT:
        case PGMPOOLKIND_ROOT_NESTED:
        case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_EPT_PD_FOR_PHYS:
            u.pau64[iUserTable] = 0;
            break;

        default:
            AssertFatalMsgFailed(("enmKind=%d iUser=%#x iUserTable=%#x\n", pUserPage->enmKind, pUser->iUser, pUser->iUserTable));
    }
}


/**
 * Clears all users of a page.
 */
static void pgmPoolTrackClearPageUsers(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    /*
     * Free all the user records.
     */
    LogFlow(("pgmPoolTrackClearPageUsers %RGp\n", pPage->GCPhys));

    PPGMPOOLUSER paUsers = pPool->CTX_SUFF(paUsers);
    uint16_t i = pPage->iUserHead;
    while (i != NIL_PGMPOOL_USER_INDEX)
    {
        /* Clear enter in user table. */
        pgmPoolTrackClearPageUser(pPool, pPage, &paUsers[i]);

        /* Free it. */
        const uint16_t iNext = paUsers[i].iNext;
        paUsers[i].iUser = NIL_PGMPOOL_IDX;
        paUsers[i].iNext = pPool->iUserFreeHead;
        pPool->iUserFreeHead = i;

        /* Next. */
        i = iNext;
    }
    pPage->iUserHead = NIL_PGMPOOL_USER_INDEX;
}

#ifdef PGMPOOL_WITH_GCPHYS_TRACKING

/**
 * Allocates a new physical cross reference extent.
 *
 * @returns Pointer to the allocated extent on success. NULL if we're out of them.
 * @param   pVM         The VM handle.
 * @param   piPhysExt   Where to store the phys ext index.
 */
PPGMPOOLPHYSEXT pgmPoolTrackPhysExtAlloc(PVM pVM, uint16_t *piPhysExt)
{
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    uint16_t iPhysExt = pPool->iPhysExtFreeHead;
    if (iPhysExt == NIL_PGMPOOL_PHYSEXT_INDEX)
    {
        STAM_COUNTER_INC(&pPool->StamTrackPhysExtAllocFailures);
        return NULL;
    }
    PPGMPOOLPHYSEXT pPhysExt = &pPool->CTX_SUFF(paPhysExts)[iPhysExt];
    pPool->iPhysExtFreeHead = pPhysExt->iNext;
    pPhysExt->iNext = NIL_PGMPOOL_PHYSEXT_INDEX;
    *piPhysExt = iPhysExt;
    return pPhysExt;
}


/**
 * Frees a physical cross reference extent.
 *
 * @param   pVM         The VM handle.
 * @param   iPhysExt    The extent to free.
 */
void pgmPoolTrackPhysExtFree(PVM pVM, uint16_t iPhysExt)
{
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    Assert(iPhysExt < pPool->cMaxPhysExts);
    PPGMPOOLPHYSEXT pPhysExt = &pPool->CTX_SUFF(paPhysExts)[iPhysExt];
    for (unsigned i = 0; i < RT_ELEMENTS(pPhysExt->aidx); i++)
        pPhysExt->aidx[i] = NIL_PGMPOOL_IDX;
    pPhysExt->iNext = pPool->iPhysExtFreeHead;
    pPool->iPhysExtFreeHead = iPhysExt;
}


/**
 * Frees a physical cross reference extent.
 *
 * @param   pVM         The VM handle.
 * @param   iPhysExt    The extent to free.
 */
void pgmPoolTrackPhysExtFreeList(PVM pVM, uint16_t iPhysExt)
{
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);

    const uint16_t  iPhysExtStart = iPhysExt;
    PPGMPOOLPHYSEXT pPhysExt;
    do
    {
        Assert(iPhysExt < pPool->cMaxPhysExts);
        pPhysExt = &pPool->CTX_SUFF(paPhysExts)[iPhysExt];
        for (unsigned i = 0; i < RT_ELEMENTS(pPhysExt->aidx); i++)
            pPhysExt->aidx[i] = NIL_PGMPOOL_IDX;

        /* next */
        iPhysExt = pPhysExt->iNext;
    } while (iPhysExt != NIL_PGMPOOL_PHYSEXT_INDEX);

    pPhysExt->iNext = pPool->iPhysExtFreeHead;
    pPool->iPhysExtFreeHead = iPhysExtStart;
}


/**
 * Insert a reference into a list of physical cross reference extents.
 *
 * @returns The new tracking data for PGMPAGE.
 *
 * @param   pVM         The VM handle.
 * @param   iPhysExt    The physical extent index of the list head.
 * @param   iShwPT      The shadow page table index.
 *
 */
static uint16_t pgmPoolTrackPhysExtInsert(PVM pVM, uint16_t iPhysExt, uint16_t iShwPT)
{
    PPGMPOOL        pPool = pVM->pgm.s.CTX_SUFF(pPool);
    PPGMPOOLPHYSEXT paPhysExts = pPool->CTX_SUFF(paPhysExts);

    /* special common case. */
    if (paPhysExts[iPhysExt].aidx[2] == NIL_PGMPOOL_IDX)
    {
        paPhysExts[iPhysExt].aidx[2] = iShwPT;
        STAM_COUNTER_INC(&pVM->pgm.s.StatTrackAliasedMany);
        LogFlow(("pgmPoolTrackPhysExtAddref: %d:{,,%d}\n", iPhysExt, iShwPT));
        return PGMPOOL_TD_MAKE(PGMPOOL_TD_CREFS_PHYSEXT, iPhysExt);
    }

    /* general treatment. */
    const uint16_t iPhysExtStart = iPhysExt;
    unsigned cMax = 15;
    for (;;)
    {
        Assert(iPhysExt < pPool->cMaxPhysExts);
        for (unsigned i = 0; i < RT_ELEMENTS(paPhysExts[iPhysExt].aidx); i++)
            if (paPhysExts[iPhysExt].aidx[i] == NIL_PGMPOOL_IDX)
            {
                paPhysExts[iPhysExt].aidx[i] = iShwPT;
                STAM_COUNTER_INC(&pVM->pgm.s.StatTrackAliasedMany);
                LogFlow(("pgmPoolTrackPhysExtAddref: %d:{%d} i=%d cMax=%d\n", iPhysExt, iShwPT, i, cMax));
                return PGMPOOL_TD_MAKE(PGMPOOL_TD_CREFS_PHYSEXT, iPhysExtStart);
            }
        if (!--cMax)
        {
            STAM_COUNTER_INC(&pVM->pgm.s.StatTrackOverflows);
            pgmPoolTrackPhysExtFreeList(pVM, iPhysExtStart);
            LogFlow(("pgmPoolTrackPhysExtAddref: overflow (1) iShwPT=%d\n", iShwPT));
            return PGMPOOL_TD_MAKE(PGMPOOL_TD_CREFS_PHYSEXT, PGMPOOL_TD_IDX_OVERFLOWED);
        }
    }

    /* add another extent to the list. */
    PPGMPOOLPHYSEXT pNew = pgmPoolTrackPhysExtAlloc(pVM, &iPhysExt);
    if (!pNew)
    {
        STAM_COUNTER_INC(&pVM->pgm.s.StatTrackOverflows);
        pgmPoolTrackPhysExtFreeList(pVM, iPhysExtStart);
        return PGMPOOL_TD_MAKE(PGMPOOL_TD_CREFS_PHYSEXT, PGMPOOL_TD_IDX_OVERFLOWED);
    }
    pNew->iNext = iPhysExtStart;
    pNew->aidx[0] = iShwPT;
    LogFlow(("pgmPoolTrackPhysExtAddref: added new extent %d:{%d}->%d\n", iPhysExt, iShwPT, iPhysExtStart));
    return PGMPOOL_TD_MAKE(PGMPOOL_TD_CREFS_PHYSEXT, iPhysExt);
}


/**
 * Add a reference to guest physical page where extents are in use.
 *
 * @returns The new tracking data for PGMPAGE.
 *
 * @param   pVM         The VM handle.
 * @param   u16         The ram range flags (top 16-bits).
 * @param   iShwPT      The shadow page table index.
 */
uint16_t pgmPoolTrackPhysExtAddref(PVM pVM, uint16_t u16, uint16_t iShwPT)
{
    if (PGMPOOL_TD_GET_CREFS(u16) != PGMPOOL_TD_CREFS_PHYSEXT)
    {
        /*
         * Convert to extent list.
         */
        Assert(PGMPOOL_TD_GET_CREFS(u16) == 1);
        uint16_t iPhysExt;
        PPGMPOOLPHYSEXT pPhysExt = pgmPoolTrackPhysExtAlloc(pVM, &iPhysExt);
        if (pPhysExt)
        {
            LogFlow(("pgmPoolTrackPhysExtAddref: new extent: %d:{%d, %d}\n", iPhysExt, PGMPOOL_TD_GET_IDX(u16), iShwPT));
            STAM_COUNTER_INC(&pVM->pgm.s.StatTrackAliased);
            pPhysExt->aidx[0] = PGMPOOL_TD_GET_IDX(u16);
            pPhysExt->aidx[1] = iShwPT;
            u16 = PGMPOOL_TD_MAKE(PGMPOOL_TD_CREFS_PHYSEXT, iPhysExt);
        }
        else
            u16 = PGMPOOL_TD_MAKE(PGMPOOL_TD_CREFS_PHYSEXT, PGMPOOL_TD_IDX_OVERFLOWED);
    }
    else if (u16 != PGMPOOL_TD_MAKE(PGMPOOL_TD_CREFS_PHYSEXT, PGMPOOL_TD_IDX_OVERFLOWED))
    {
        /*
         * Insert into the extent list.
         */
        u16 = pgmPoolTrackPhysExtInsert(pVM, PGMPOOL_TD_GET_IDX(u16), iShwPT);
    }
    else
        STAM_COUNTER_INC(&pVM->pgm.s.StatTrackAliasedLots);
    return u16;
}


/**
 * Clear references to guest physical memory.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pPhysPage   Pointer to the aPages entry in the ram range.
 */
void pgmPoolTrackPhysExtDerefGCPhys(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PPGMPAGE pPhysPage)
{
    const unsigned cRefs = PGM_PAGE_GET_TD_CREFS(pPhysPage);
    AssertFatalMsg(cRefs == PGMPOOL_TD_CREFS_PHYSEXT, ("cRefs=%d pPhysPage=%R[pgmpage] pPage=%p:{.idx=%d}\n", cRefs, pPhysPage, pPage, pPage->idx));

    uint16_t iPhysExt = PGM_PAGE_GET_TD_IDX(pPhysPage);
    if (iPhysExt != PGMPOOL_TD_IDX_OVERFLOWED)
    {
        uint16_t        iPhysExtPrev = NIL_PGMPOOL_PHYSEXT_INDEX;
        PPGMPOOLPHYSEXT paPhysExts = pPool->CTX_SUFF(paPhysExts);
        do
        {
            Assert(iPhysExt < pPool->cMaxPhysExts);

            /*
             * Look for the shadow page and check if it's all freed.
             */
            for (unsigned i = 0; i < RT_ELEMENTS(paPhysExts[iPhysExt].aidx); i++)
            {
                if (paPhysExts[iPhysExt].aidx[i] == pPage->idx)
                {
                    paPhysExts[iPhysExt].aidx[i] = NIL_PGMPOOL_IDX;

                    for (i = 0; i < RT_ELEMENTS(paPhysExts[iPhysExt].aidx); i++)
                        if (paPhysExts[iPhysExt].aidx[i] != NIL_PGMPOOL_IDX)
                        {
                            Log2(("pgmPoolTrackPhysExtDerefGCPhys: pPhysPage=%R[pgmpage] idx=%d\n", pPhysPage, pPage->idx));
                            return;
                        }

                    /* we can free the node. */
                    PVM pVM = pPool->CTX_SUFF(pVM);
                    const uint16_t iPhysExtNext = paPhysExts[iPhysExt].iNext;
                    if (    iPhysExtPrev == NIL_PGMPOOL_PHYSEXT_INDEX
                        &&  iPhysExtNext == NIL_PGMPOOL_PHYSEXT_INDEX)
                    {
                        /* lonely node */
                        pgmPoolTrackPhysExtFree(pVM, iPhysExt);
                        Log2(("pgmPoolTrackPhysExtDerefGCPhys: pPhysPage=%R[pgmpage] idx=%d lonely\n", pPhysPage, pPage->idx));
                        PGM_PAGE_SET_TRACKING(pPhysPage, 0);
                    }
                    else if (iPhysExtPrev == NIL_PGMPOOL_PHYSEXT_INDEX)
                    {
                        /* head */
                        Log2(("pgmPoolTrackPhysExtDerefGCPhys: pPhysPage=%R[pgmpage] idx=%d head\n", pPhysPage, pPage->idx));
                        PGM_PAGE_SET_TRACKING(pPhysPage, PGMPOOL_TD_MAKE(PGMPOOL_TD_CREFS_PHYSEXT, iPhysExtNext));
                        pgmPoolTrackPhysExtFree(pVM, iPhysExt);
                    }
                    else
                    {
                        /* in list */
                        Log2(("pgmPoolTrackPhysExtDerefGCPhys: pPhysPage=%R[pgmpage] idx=%d\n", pPhysPage, pPage->idx));
                        paPhysExts[iPhysExtPrev].iNext = iPhysExtNext;
                        pgmPoolTrackPhysExtFree(pVM, iPhysExt);
                    }
                    iPhysExt = iPhysExtNext;
                    return;
                }
            }

            /* next */
            iPhysExtPrev = iPhysExt;
            iPhysExt = paPhysExts[iPhysExt].iNext;
        } while (iPhysExt != NIL_PGMPOOL_PHYSEXT_INDEX);

        AssertFatalMsgFailed(("not-found! cRefs=%d pPhysPage=%R[pgmpage] pPage=%p:{.idx=%d}\n", cRefs, pPhysPage, pPage, pPage->idx));
    }
    else /* nothing to do */
        Log2(("pgmPoolTrackPhysExtDerefGCPhys: pPhysPage=%R[pgmpage]\n", pPhysPage));
}


/**
 * Clear references to guest physical memory.
 *
 * This is the same as pgmPoolTracDerefGCPhys except that the guest physical address
 * is assumed to be correct, so the linear search can be skipped and we can assert
 * at an earlier point.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   HCPhys      The host physical address corresponding to the guest page.
 * @param   GCPhys      The guest physical address corresponding to HCPhys.
 */
static void pgmPoolTracDerefGCPhys(PPGMPOOL pPool, PPGMPOOLPAGE pPage, RTHCPHYS HCPhys, RTGCPHYS GCPhys)
{
    /*
     * Walk range list.
     */
    PPGMRAMRANGE pRam = pPool->CTX_SUFF(pVM)->pgm.s.CTX_SUFF(pRamRanges);
    while (pRam)
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (off < pRam->cb)
        {
            /* does it match? */
            const unsigned iPage = off >> PAGE_SHIFT;
            Assert(PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]));
#ifdef LOG_ENABLED
RTHCPHYS HCPhysPage = PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]);
Log2(("pgmPoolTracDerefGCPhys %RHp vs %RHp\n", HCPhysPage, HCPhys));
#endif
            if (PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]) == HCPhys)
            {
                pgmTrackDerefGCPhys(pPool, pPage, &pRam->aPages[iPage]);
                return;
            }
            break;
        }
        pRam = pRam->CTX_SUFF(pNext);
    }
    AssertFatalMsgFailed(("HCPhys=%RHp GCPhys=%RGp\n", HCPhys, GCPhys));
}


/**
 * Clear references to guest physical memory.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   HCPhys      The host physical address corresponding to the guest page.
 * @param   GCPhysHint  The guest physical address which may corresponding to HCPhys.
 */
static void pgmPoolTracDerefGCPhysHint(PPGMPOOL pPool, PPGMPOOLPAGE pPage, RTHCPHYS HCPhys, RTGCPHYS GCPhysHint)
{
    Log4(("pgmPoolTracDerefGCPhysHint %RHp %RGp\n", HCPhys, GCPhysHint));

    /*
     * Walk range list.
     */
    PPGMRAMRANGE pRam = pPool->CTX_SUFF(pVM)->pgm.s.CTX_SUFF(pRamRanges);
    while (pRam)
    {
        RTGCPHYS off = GCPhysHint - pRam->GCPhys;
        if (off < pRam->cb)
        {
            /* does it match? */
            const unsigned iPage = off >> PAGE_SHIFT;
            Assert(PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]));
            if (PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]) == HCPhys)
            {
                pgmTrackDerefGCPhys(pPool, pPage, &pRam->aPages[iPage]);
                return;
            }
            break;
        }
        pRam = pRam->CTX_SUFF(pNext);
    }

    /*
     * Damn, the hint didn't work. We'll have to do an expensive linear search.
     */
    STAM_COUNTER_INC(&pPool->StatTrackLinearRamSearches);
    pRam = pPool->CTX_SUFF(pVM)->pgm.s.CTX_SUFF(pRamRanges);
    while (pRam)
    {
        unsigned iPage = pRam->cb >> PAGE_SHIFT;
        while (iPage-- > 0)
        {
            if (PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]) == HCPhys)
            {
                Log4(("pgmPoolTracDerefGCPhysHint: Linear HCPhys=%RHp GCPhysHint=%RGp GCPhysReal=%RGp\n",
                      HCPhys, GCPhysHint, pRam->GCPhys + (iPage << PAGE_SHIFT)));
                pgmTrackDerefGCPhys(pPool, pPage, &pRam->aPages[iPage]);
                return;
            }
        }
        pRam = pRam->CTX_SUFF(pNext);
    }

    AssertFatalMsgFailed(("HCPhys=%RHp GCPhysHint=%RGp\n", HCPhys, GCPhysHint));
}


/**
 * Clear references to guest physical memory in a 32-bit / 32-bit page table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPT      The shadow page table (mapping of the page).
 * @param   pGstPT      The guest page table.
 */
DECLINLINE(void) pgmPoolTrackDerefPT32Bit32Bit(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PX86PT pShwPT, PCX86PT pGstPT)
{
    for (unsigned i = pPage->iFirstPresent; i < RT_ELEMENTS(pShwPT->a); i++)
        if (pShwPT->a[i].n.u1Present)
        {
            Log4(("pgmPoolTrackDerefPT32Bit32Bit: i=%d pte=%RX32 hint=%RX32\n",
                  i, pShwPT->a[i].u & X86_PTE_PG_MASK, pGstPT->a[i].u & X86_PTE_PG_MASK));
            pgmPoolTracDerefGCPhysHint(pPool, pPage, pShwPT->a[i].u & X86_PTE_PG_MASK, pGstPT->a[i].u & X86_PTE_PG_MASK);
            if (!--pPage->cPresent)
                break;
        }
}


/**
 * Clear references to guest physical memory in a PAE / 32-bit page table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPT      The shadow page table (mapping of the page).
 * @param   pGstPT      The guest page table (just a half one).
 */
DECLINLINE(void) pgmPoolTrackDerefPTPae32Bit(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PX86PTPAE pShwPT, PCX86PT pGstPT)
{
    for (unsigned i = 0; i < RT_ELEMENTS(pShwPT->a); i++)
        if (pShwPT->a[i].n.u1Present)
        {
            Log4(("pgmPoolTrackDerefPTPae32Bit: i=%d pte=%RX64 hint=%RX32\n",
                  i, pShwPT->a[i].u & X86_PTE_PAE_PG_MASK, pGstPT->a[i].u & X86_PTE_PG_MASK));
            pgmPoolTracDerefGCPhysHint(pPool, pPage, pShwPT->a[i].u & X86_PTE_PAE_PG_MASK, pGstPT->a[i].u & X86_PTE_PG_MASK);
        }
}


/**
 * Clear references to guest physical memory in a PAE / PAE page table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPT      The shadow page table (mapping of the page).
 * @param   pGstPT      The guest page table.
 */
DECLINLINE(void) pgmPoolTrackDerefPTPaePae(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PX86PTPAE pShwPT, PCX86PTPAE pGstPT)
{
    for (unsigned i = 0; i < RT_ELEMENTS(pShwPT->a); i++)
        if (pShwPT->a[i].n.u1Present)
        {
            Log4(("pgmPoolTrackDerefPTPaePae: i=%d pte=%RX32 hint=%RX32\n",
                  i, pShwPT->a[i].u & X86_PTE_PAE_PG_MASK, pGstPT->a[i].u & X86_PTE_PAE_PG_MASK));
            pgmPoolTracDerefGCPhysHint(pPool, pPage, pShwPT->a[i].u & X86_PTE_PAE_PG_MASK, pGstPT->a[i].u & X86_PTE_PAE_PG_MASK);
        }
}


/**
 * Clear references to guest physical memory in a 32-bit / 4MB page table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPT      The shadow page table (mapping of the page).
 */
DECLINLINE(void) pgmPoolTrackDerefPT32Bit4MB(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PX86PT pShwPT)
{
    RTGCPHYS GCPhys = pPage->GCPhys;
    for (unsigned i = 0; i < RT_ELEMENTS(pShwPT->a); i++, GCPhys += PAGE_SIZE)
        if (pShwPT->a[i].n.u1Present)
        {
            Log4(("pgmPoolTrackDerefPT32Bit4MB: i=%d pte=%RX32 GCPhys=%RGp\n",
                  i, pShwPT->a[i].u & X86_PTE_PG_MASK, GCPhys));
            pgmPoolTracDerefGCPhys(pPool, pPage, pShwPT->a[i].u & X86_PTE_PG_MASK, GCPhys);
        }
}


/**
 * Clear references to guest physical memory in a PAE / 2/4MB page table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPT      The shadow page table (mapping of the page).
 */
DECLINLINE(void) pgmPoolTrackDerefPTPaeBig(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PX86PTPAE pShwPT)
{
    RTGCPHYS GCPhys = pPage->GCPhys;
    for (unsigned i = 0; i < RT_ELEMENTS(pShwPT->a); i++, GCPhys += PAGE_SIZE)
        if (pShwPT->a[i].n.u1Present)
        {
            Log4(("pgmPoolTrackDerefPTPaeBig: i=%d pte=%RX64 hint=%RGp\n",
                  i, pShwPT->a[i].u & X86_PTE_PAE_PG_MASK, GCPhys));
            pgmPoolTracDerefGCPhys(pPool, pPage, pShwPT->a[i].u & X86_PTE_PAE_PG_MASK, GCPhys);
        }
}

#endif /* PGMPOOL_WITH_GCPHYS_TRACKING */


/**
 * Clear references to shadowed pages in a 32 bits page directory.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPD      The shadow page directory (mapping of the page).
 */
DECLINLINE(void) pgmPoolTrackDerefPD(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PX86PD pShwPD)
{
    for (unsigned i = 0; i < RT_ELEMENTS(pShwPD->a); i++)
    {
        if (    pShwPD->a[i].n.u1Present
            &&  !(pShwPD->a[i].u & PGM_PDFLAGS_MAPPING)
           )
        {
            PPGMPOOLPAGE pSubPage = (PPGMPOOLPAGE)RTAvloHCPhysGet(&pPool->HCPhysTree, pShwPD->a[i].u & X86_PDE_PG_MASK);
            if (pSubPage)
                pgmPoolTrackFreeUser(pPool, pSubPage, pPage->idx, i);
            else
                AssertFatalMsgFailed(("%x\n", pShwPD->a[i].u & X86_PDE_PG_MASK));
        }
    }
}

/**
 * Clear references to shadowed pages in a PAE (legacy or 64 bits) page directory.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPD      The shadow page directory (mapping of the page).
 */
DECLINLINE(void) pgmPoolTrackDerefPDPae(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PX86PDPAE pShwPD)
{
    for (unsigned i = 0; i < RT_ELEMENTS(pShwPD->a); i++)
    {
        if (    pShwPD->a[i].n.u1Present
            &&  !(pShwPD->a[i].u & PGM_PDFLAGS_MAPPING)
           )
        {
            PPGMPOOLPAGE pSubPage = (PPGMPOOLPAGE)RTAvloHCPhysGet(&pPool->HCPhysTree, pShwPD->a[i].u & X86_PDE_PAE_PG_MASK);
            if (pSubPage)
                pgmPoolTrackFreeUser(pPool, pSubPage, pPage->idx, i);
            else
                AssertFatalMsgFailed(("%RX64\n", pShwPD->a[i].u & X86_PDE_PAE_PG_MASK));
            /** @todo 64-bit guests: have to ensure that we're not exhausting the dynamic mappings! */
        }
    }
}

/**
 * Clear references to shadowed pages in a PAE page directory pointer table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPDPT   The shadow page directory pointer table (mapping of the page).
 */
DECLINLINE(void) pgmPoolTrackDerefPDPTPae(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PX86PDPT pShwPDPT)
{
    for (unsigned i = 0; i < X86_PG_PAE_PDPE_ENTRIES; i++)
    {
        if (    pShwPDPT->a[i].n.u1Present
            &&  !(pShwPDPT->a[i].u & PGM_PLXFLAGS_MAPPING)
           )
        {
            PPGMPOOLPAGE pSubPage = (PPGMPOOLPAGE)RTAvloHCPhysGet(&pPool->HCPhysTree, pShwPDPT->a[i].u & X86_PDPE_PG_MASK);
            if (pSubPage)
                pgmPoolTrackFreeUser(pPool, pSubPage, pPage->idx, i);
            else
                AssertFatalMsgFailed(("%RX64\n", pShwPDPT->a[i].u & X86_PDPE_PG_MASK));
        }
    }
}


/**
 * Clear references to shadowed pages in a 64-bit page directory pointer table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPDPT   The shadow page directory pointer table (mapping of the page).
 */
DECLINLINE(void) pgmPoolTrackDerefPDPT64Bit(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PX86PDPT pShwPDPT)
{
    for (unsigned i = 0; i < RT_ELEMENTS(pShwPDPT->a); i++)
    {
        Assert(!(pShwPDPT->a[i].u & PGM_PLXFLAGS_MAPPING));
        if (pShwPDPT->a[i].n.u1Present)
        {
            PPGMPOOLPAGE pSubPage = (PPGMPOOLPAGE)RTAvloHCPhysGet(&pPool->HCPhysTree, pShwPDPT->a[i].u & X86_PDPE_PG_MASK);
            if (pSubPage)
                pgmPoolTrackFreeUser(pPool, pSubPage, pPage->idx, i);
            else
                AssertFatalMsgFailed(("%RX64\n", pShwPDPT->a[i].u & X86_PDPE_PG_MASK));
            /** @todo 64-bit guests: have to ensure that we're not exhausting the dynamic mappings! */
        }
    }
}


/**
 * Clear references to shadowed pages in a 64-bit level 4 page table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPML4    The shadow page directory pointer table (mapping of the page).
 */
DECLINLINE(void) pgmPoolTrackDerefPML464Bit(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PX86PML4 pShwPML4)
{
    for (unsigned i = 0; i < RT_ELEMENTS(pShwPML4->a); i++)
    {
        if (pShwPML4->a[i].n.u1Present)
        {
            PPGMPOOLPAGE pSubPage = (PPGMPOOLPAGE)RTAvloHCPhysGet(&pPool->HCPhysTree, pShwPML4->a[i].u & X86_PDPE_PG_MASK);
            if (pSubPage)
                pgmPoolTrackFreeUser(pPool, pSubPage, pPage->idx, i);
            else
                AssertFatalMsgFailed(("%RX64\n", pShwPML4->a[i].u & X86_PML4E_PG_MASK));
            /** @todo 64-bit guests: have to ensure that we're not exhausting the dynamic mappings! */
        }
    }
}


/**
 * Clear references to shadowed pages in an EPT page table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPML4    The shadow page directory pointer table (mapping of the page).
 */
DECLINLINE(void) pgmPoolTrackDerefPTEPT(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PEPTPT pShwPT)
{
    RTGCPHYS GCPhys = pPage->GCPhys;
    for (unsigned i = 0; i < RT_ELEMENTS(pShwPT->a); i++, GCPhys += PAGE_SIZE)
        if (pShwPT->a[i].n.u1Present)
        {
            Log4(("pgmPoolTrackDerefPTEPT: i=%d pte=%RX64 GCPhys=%RX64\n",
                  i, pShwPT->a[i].u & EPT_PTE_PG_MASK, pPage->GCPhys));
            pgmPoolTracDerefGCPhys(pPool, pPage, pShwPT->a[i].u & EPT_PTE_PG_MASK, GCPhys);
        }
}


/**
 * Clear references to shadowed pages in an EPT page directory.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPD      The shadow page directory (mapping of the page).
 */
DECLINLINE(void) pgmPoolTrackDerefPDEPT(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PEPTPD pShwPD)
{
    for (unsigned i = 0; i < RT_ELEMENTS(pShwPD->a); i++)
    {
        if (pShwPD->a[i].n.u1Present)
        {
            PPGMPOOLPAGE pSubPage = (PPGMPOOLPAGE)RTAvloHCPhysGet(&pPool->HCPhysTree, pShwPD->a[i].u & EPT_PDE_PG_MASK);
            if (pSubPage)
                pgmPoolTrackFreeUser(pPool, pSubPage, pPage->idx, i);
            else
                AssertFatalMsgFailed(("%RX64\n", pShwPD->a[i].u & EPT_PDE_PG_MASK));
            /** @todo 64-bit guests: have to ensure that we're not exhausting the dynamic mappings! */
        }
    }
}


/**
 * Clear references to shadowed pages in an EPT page directory pointer table.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 * @param   pShwPDPT   The shadow page directory pointer table (mapping of the page).
 */
DECLINLINE(void) pgmPoolTrackDerefPDPTEPT(PPGMPOOL pPool, PPGMPOOLPAGE pPage, PEPTPDPT pShwPDPT)
{
    for (unsigned i = 0; i < RT_ELEMENTS(pShwPDPT->a); i++)
    {
        if (pShwPDPT->a[i].n.u1Present)
        {
            PPGMPOOLPAGE pSubPage = (PPGMPOOLPAGE)RTAvloHCPhysGet(&pPool->HCPhysTree, pShwPDPT->a[i].u & EPT_PDPTE_PG_MASK);
            if (pSubPage)
                pgmPoolTrackFreeUser(pPool, pSubPage, pPage->idx, i);
            else
                AssertFatalMsgFailed(("%RX64\n", pShwPDPT->a[i].u & EPT_PDPTE_PG_MASK));
            /** @todo 64-bit guests: have to ensure that we're not exhausting the dynamic mappings! */
        }
    }
}


/**
 * Clears all references made by this page.
 *
 * This includes other shadow pages and GC physical addresses.
 *
 * @param   pPool       The pool.
 * @param   pPage       The page.
 */
static void pgmPoolTrackDeref(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    /*
     * Map the shadow page and take action according to the page kind.
     */
    void *pvShw = PGMPOOL_PAGE_2_LOCKED_PTR(pPool->CTX_SUFF(pVM), pPage);
    switch (pPage->enmKind)
    {
#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
        {
            STAM_PROFILE_START(&pPool->StatTrackDerefGCPhys, g);
            void *pvGst;
            int rc = PGM_GCPHYS_2_PTR(pPool->CTX_SUFF(pVM), pPage->GCPhys, &pvGst); AssertReleaseRC(rc);
            pgmPoolTrackDerefPT32Bit32Bit(pPool, pPage, (PX86PT)pvShw, (PCX86PT)pvGst);
            STAM_PROFILE_STOP(&pPool->StatTrackDerefGCPhys, g);
            break;
        }

        case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
        {
            STAM_PROFILE_START(&pPool->StatTrackDerefGCPhys, g);
            void *pvGst;
            int rc = PGM_GCPHYS_2_PTR_EX(pPool->CTX_SUFF(pVM), pPage->GCPhys, &pvGst); AssertReleaseRC(rc);
            pgmPoolTrackDerefPTPae32Bit(pPool, pPage, (PX86PTPAE)pvShw, (PCX86PT)pvGst);
            STAM_PROFILE_STOP(&pPool->StatTrackDerefGCPhys, g);
            break;
        }

        case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
        {
            STAM_PROFILE_START(&pPool->StatTrackDerefGCPhys, g);
            void *pvGst;
            int rc = PGM_GCPHYS_2_PTR(pPool->CTX_SUFF(pVM), pPage->GCPhys, &pvGst); AssertReleaseRC(rc);
            pgmPoolTrackDerefPTPaePae(pPool, pPage, (PX86PTPAE)pvShw, (PCX86PTPAE)pvGst);
            STAM_PROFILE_STOP(&pPool->StatTrackDerefGCPhys, g);
            break;
        }

        case PGMPOOLKIND_32BIT_PT_FOR_PHYS: /* treat it like a 4 MB page */
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
        {
            STAM_PROFILE_START(&pPool->StatTrackDerefGCPhys, g);
            pgmPoolTrackDerefPT32Bit4MB(pPool, pPage, (PX86PT)pvShw);
            STAM_PROFILE_STOP(&pPool->StatTrackDerefGCPhys, g);
            break;
        }

        case PGMPOOLKIND_PAE_PT_FOR_PHYS:   /* treat it like a 2 MB page */
        case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
        {
            STAM_PROFILE_START(&pPool->StatTrackDerefGCPhys, g);
            pgmPoolTrackDerefPTPaeBig(pPool, pPage, (PX86PTPAE)pvShw);
            STAM_PROFILE_STOP(&pPool->StatTrackDerefGCPhys, g);
            break;
        }

#else /* !PGMPOOL_WITH_GCPHYS_TRACKING */
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
        case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
        case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
        case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
        case PGMPOOLKIND_PAE_PT_FOR_PHYS:
            break;
#endif /* !PGMPOOL_WITH_GCPHYS_TRACKING */

        case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
        case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
        case PGMPOOLKIND_PAE_PD_PHYS:
        case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
        case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
            pgmPoolTrackDerefPDPae(pPool, pPage, (PX86PDPAE)pvShw);
            break;

        case PGMPOOLKIND_32BIT_PD_PHYS:
        case PGMPOOLKIND_32BIT_PD:
            pgmPoolTrackDerefPD(pPool, pPage, (PX86PD)pvShw);
            break;

        case PGMPOOLKIND_PAE_PDPT_FOR_32BIT:
        case PGMPOOLKIND_PAE_PDPT:
        case PGMPOOLKIND_PAE_PDPT_PHYS:
            pgmPoolTrackDerefPDPTPae(pPool, pPage, (PX86PDPT)pvShw);
            break;

        case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
        case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
            pgmPoolTrackDerefPDPT64Bit(pPool, pPage, (PX86PDPT)pvShw);
            break;

        case PGMPOOLKIND_64BIT_PML4:
            pgmPoolTrackDerefPML464Bit(pPool, pPage, (PX86PML4)pvShw);
            break;

        case PGMPOOLKIND_EPT_PT_FOR_PHYS:
            pgmPoolTrackDerefPTEPT(pPool, pPage, (PEPTPT)pvShw);
            break;

        case PGMPOOLKIND_EPT_PD_FOR_PHYS:
            pgmPoolTrackDerefPDEPT(pPool, pPage, (PEPTPD)pvShw);
            break;

        case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
            pgmPoolTrackDerefPDPTEPT(pPool, pPage, (PEPTPDPT)pvShw);
            break;

        default:
            AssertFatalMsgFailed(("enmKind=%d\n", pPage->enmKind));
    }

    /* paranoia, clear the shadow page. Remove this laser (i.e. let Alloc and ClearAll do it). */
    STAM_PROFILE_START(&pPool->StatZeroPage, z);
    ASMMemZeroPage(pvShw);
    STAM_PROFILE_STOP(&pPool->StatZeroPage, z);
    pPage->fZeroed = true;
    PGMPOOL_UNLOCK_PTR(pPool->CTX_SUFF(pVM), pvShw);
}

#endif /* PGMPOOL_WITH_USER_TRACKING */
#ifdef IN_RING3

/**
 * Flushes all the special root pages as part of a pgmPoolFlushAllInt operation.
 *
 * @param   pPool       The pool.
 */
static void pgmPoolFlushAllSpecialRoots(PPGMPOOL pPool)
{
    /*
     * These special pages are all mapped into the indexes 1..PGMPOOL_IDX_FIRST.
     */
    Assert(NIL_PGMPOOL_IDX == 0);

    /*
     * Paranoia (to be removed), flag a global CR3 sync.
     */
    VM_FF_SET(pPool->CTX_SUFF(pVM), VM_FF_PGM_SYNC_CR3);
}


/**
 * Flushes the entire cache.
 *
 * It will assert a global CR3 flush (FF) and assumes the caller is aware of this
 * and execute this CR3 flush.
 *
 * @param   pPool       The pool.
 */
static void pgmPoolFlushAllInt(PPGMPOOL pPool)
{
    PVM pVM = pPool->CTX_SUFF(pVM);

    STAM_PROFILE_START(&pPool->StatFlushAllInt, a);
    LogFlow(("pgmPoolFlushAllInt:\n"));

    /*
     * If there are no pages in the pool, there is nothing to do.
     */
    if (pPool->cCurPages <= PGMPOOL_IDX_FIRST)
    {
        STAM_PROFILE_STOP(&pPool->StatFlushAllInt, a);
        return;
    }

    /* @todo Need to synchronize this across all VCPUs! */
    Assert(pVM->cCPUs == 1);
    PVMCPU pVCpu = &pVM->aCpus[0];  /* to get it compiled... */

    /* Unmap the old CR3 value before flushing everything. */
    int rc = PGM_BTH_PFN(UnmapCR3, pVCpu)(pVM, pVCpu);
    AssertRC(rc);

    /* Exit the current shadow paging mode as well; nested paging and EPT use a root CR3 which will get flushed here. */
    rc = PGM_SHW_PFN(Exit, pVCpu)(pVCpu);
    AssertRC(rc);

    /*
     * Nuke the free list and reinsert all pages into it.
     */
    for (unsigned i = pPool->cCurPages - 1; i >= PGMPOOL_IDX_FIRST; i--)
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[i];

#ifdef IN_RING3
        Assert(pPage->Core.Key == MMPage2Phys(pVM, pPage->pvPageR3));
#endif
#ifdef PGMPOOL_WITH_MONITORING
        if (pPage->fMonitored)
            pgmPoolMonitorFlush(pPool, pPage);
        pPage->iModifiedNext = NIL_PGMPOOL_IDX;
        pPage->iModifiedPrev = NIL_PGMPOOL_IDX;
        pPage->iMonitoredNext = NIL_PGMPOOL_IDX;
        pPage->iMonitoredPrev = NIL_PGMPOOL_IDX;
        pPage->cModifications = 0;
#endif
        pPage->GCPhys    = NIL_RTGCPHYS;
        pPage->enmKind   = PGMPOOLKIND_FREE;
        Assert(pPage->idx == i);
        pPage->iNext     = i + 1;
        pPage->fZeroed   = false;       /* This could probably be optimized, but better safe than sorry. */
        pPage->fSeenNonGlobal = false;
        pPage->fMonitored= false;
        pPage->fCached   = false;
        pPage->fReusedFlushPending = false;
#ifdef PGMPOOL_WITH_USER_TRACKING
        pPage->iUserHead = NIL_PGMPOOL_USER_INDEX;
#else
        pPage->fCR3Mix = false;
#endif
#ifdef PGMPOOL_WITH_CACHE
        pPage->iAgeNext  = NIL_PGMPOOL_IDX;
        pPage->iAgePrev  = NIL_PGMPOOL_IDX;
#endif
        pPage->fLocked   = false;
    }
    pPool->aPages[pPool->cCurPages - 1].iNext = NIL_PGMPOOL_IDX;
    pPool->iFreeHead = PGMPOOL_IDX_FIRST;
    pPool->cUsedPages = 0;

#ifdef PGMPOOL_WITH_USER_TRACKING
    /*
     * Zap and reinitialize the user records.
     */
    pPool->cPresent = 0;
    pPool->iUserFreeHead = 0;
    PPGMPOOLUSER paUsers = pPool->CTX_SUFF(paUsers);
    const unsigned cMaxUsers = pPool->cMaxUsers;
    for (unsigned i = 0; i < cMaxUsers; i++)
    {
        paUsers[i].iNext = i + 1;
        paUsers[i].iUser = NIL_PGMPOOL_IDX;
        paUsers[i].iUserTable = 0xfffffffe;
    }
    paUsers[cMaxUsers - 1].iNext = NIL_PGMPOOL_USER_INDEX;
#endif

#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    /*
     * Clear all the GCPhys links and rebuild the phys ext free list.
     */
    for (PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRanges);
         pRam;
         pRam = pRam->CTX_SUFF(pNext))
    {
        unsigned iPage = pRam->cb >> PAGE_SHIFT;
        while (iPage-- > 0)
            PGM_PAGE_SET_TRACKING(&pRam->aPages[iPage], 0);
    }

    pPool->iPhysExtFreeHead = 0;
    PPGMPOOLPHYSEXT paPhysExts = pPool->CTX_SUFF(paPhysExts);
    const unsigned cMaxPhysExts = pPool->cMaxPhysExts;
    for (unsigned i = 0; i < cMaxPhysExts; i++)
    {
        paPhysExts[i].iNext = i + 1;
        paPhysExts[i].aidx[0] = NIL_PGMPOOL_IDX;
        paPhysExts[i].aidx[1] = NIL_PGMPOOL_IDX;
        paPhysExts[i].aidx[2] = NIL_PGMPOOL_IDX;
    }
    paPhysExts[cMaxPhysExts - 1].iNext = NIL_PGMPOOL_PHYSEXT_INDEX;
#endif

#ifdef PGMPOOL_WITH_MONITORING
    /*
     * Just zap the modified list.
     */
    pPool->cModifiedPages = 0;
    pPool->iModifiedHead = NIL_PGMPOOL_IDX;
#endif

#ifdef PGMPOOL_WITH_CACHE
    /*
     * Clear the GCPhys hash and the age list.
     */
    for (unsigned i = 0; i < RT_ELEMENTS(pPool->aiHash); i++)
        pPool->aiHash[i] = NIL_PGMPOOL_IDX;
    pPool->iAgeHead = NIL_PGMPOOL_IDX;
    pPool->iAgeTail = NIL_PGMPOOL_IDX;
#endif

    /*
     * Flush all the special root pages.
     * Reinsert active pages into the hash and ensure monitoring chains are correct.
     */
    pgmPoolFlushAllSpecialRoots(pPool);
    for (unsigned i = PGMPOOL_IDX_FIRST_SPECIAL; i < PGMPOOL_IDX_FIRST; i++)
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[i];
        pPage->iNext = NIL_PGMPOOL_IDX;
#ifdef PGMPOOL_WITH_MONITORING
        pPage->iModifiedNext = NIL_PGMPOOL_IDX;
        pPage->iModifiedPrev = NIL_PGMPOOL_IDX;
        pPage->cModifications = 0;
        /* ASSUMES that we're not sharing with any of the other special pages (safe for now). */
        pPage->iMonitoredNext = NIL_PGMPOOL_IDX;
        pPage->iMonitoredPrev = NIL_PGMPOOL_IDX;
        if (pPage->fMonitored)
        {
            int rc = PGMHandlerPhysicalChangeCallbacks(pVM, pPage->GCPhys & ~(RTGCPHYS)(PAGE_SIZE - 1),
                                                       pPool->pfnAccessHandlerR3, MMHyperCCToR3(pVM, pPage),
                                                       pPool->pfnAccessHandlerR0, MMHyperCCToR0(pVM, pPage),
                                                       pPool->pfnAccessHandlerRC, MMHyperCCToRC(pVM, pPage),
                                                       pPool->pszAccessHandler);
            AssertFatalRCSuccess(rc);
# ifdef PGMPOOL_WITH_CACHE
            pgmPoolHashInsert(pPool, pPage);
# endif
        }
#endif
#ifdef PGMPOOL_WITH_USER_TRACKING
        Assert(pPage->iUserHead == NIL_PGMPOOL_USER_INDEX); /* for now */
#endif
#ifdef PGMPOOL_WITH_CACHE
        Assert(pPage->iAgeNext == NIL_PGMPOOL_IDX);
        Assert(pPage->iAgePrev == NIL_PGMPOOL_IDX);
#endif
    }

    /*
     * Force a shadow mode reinit (necessary for nested paging and ept).
     * Reinit the current shadow paging mode as well; nested paging and
     * EPT use a root CR3 which will get flushed here.
     */
    pVCpu->pgm.s.enmShadowMode = PGMMODE_INVALID;
    rc = PGMR3ChangeMode(pVM, pVCpu, PGMGetGuestMode(pVCpu));
    AssertRC(rc);

    /*
     * Finally, assert the FF.
     */
    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);

    STAM_PROFILE_STOP(&pPool->StatFlushAllInt, a);
}

#endif /* IN_RING3 */

/**
 * Flushes a pool page.
 *
 * This moves the page to the free list after removing all user references to it.
 * In GC this will cause a CR3 reload if the page is traced back to an active root page.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @param   pPool       The pool.
 * @param   HCPhys      The HC physical address of the shadow page.
 */
int pgmPoolFlushPage(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    int rc = VINF_SUCCESS;
    STAM_PROFILE_START(&pPool->StatFlushPage, f);
    LogFlow(("pgmPoolFlushPage: pPage=%p:{.Key=%RHp, .idx=%d, .enmKind=%s, .GCPhys=%RGp}\n",
             pPage, pPage->Core.Key, pPage->idx, pgmPoolPoolKindToStr(pPage->enmKind), pPage->GCPhys));

    /*
     * Quietly reject any attempts at flushing any of the special root pages.
     */
    if (pPage->idx < PGMPOOL_IDX_FIRST)
    {
        AssertFailed(); /* can no longer happen */
        Log(("pgmPoolFlushPage: special root page, rejected. enmKind=%s idx=%d\n", pgmPoolPoolKindToStr(pPage->enmKind), pPage->idx));
        return VINF_SUCCESS;
    }

    /*
     * Quietly reject any attempts at flushing the currently active shadow CR3 mapping
     */
    if (pgmPoolIsPageLocked(&pPool->CTX_SUFF(pVM)->pgm.s, pPage))
    {
        AssertMsg(   pPage->enmKind == PGMPOOLKIND_64BIT_PML4
                  || pPage->enmKind == PGMPOOLKIND_PAE_PDPT
                  || pPage->enmKind == PGMPOOLKIND_PAE_PDPT_FOR_32BIT
                  || pPage->enmKind == PGMPOOLKIND_32BIT_PD
                  || pPage->enmKind == PGMPOOLKIND_PAE_PD_FOR_PAE_PD
                  || pPage->enmKind == PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD
                  || pPage->enmKind == PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD
                  || pPage->enmKind == PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD
                  || pPage->enmKind == PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD,
                  ("Can't free the shadow CR3! (%RHp vs %RHp kind=%d\n", PGMGetHyperCR3(VMMGetCpu(pPool->CTX_SUFF(pVM))), pPage->Core.Key, pPage->enmKind));
        Log(("pgmPoolFlushPage: current active shadow CR3, rejected. enmKind=%s idx=%d\n", pgmPoolPoolKindToStr(pPage->enmKind), pPage->idx));
        return VINF_SUCCESS;
    }

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    /* Start a subset so we won't run out of mapping space. */
    PVMCPU pVCpu = VMMGetCpu(pPool->CTX_SUFF(pVM));
    uint32_t iPrevSubset = PGMDynMapPushAutoSubset(pVCpu);
#endif

    /*
     * Mark the page as being in need of a ASMMemZeroPage().
     */
    pPage->fZeroed = false;

#ifdef PGMPOOL_WITH_USER_TRACKING
    /*
     * Clear the page.
     */
    pgmPoolTrackClearPageUsers(pPool, pPage);
    STAM_PROFILE_START(&pPool->StatTrackDeref,a);
    pgmPoolTrackDeref(pPool, pPage);
    STAM_PROFILE_STOP(&pPool->StatTrackDeref,a);
#endif

#ifdef PGMPOOL_WITH_CACHE
    /*
     * Flush it from the cache.
     */
    pgmPoolCacheFlushPage(pPool, pPage);
#endif /* PGMPOOL_WITH_CACHE */

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    /* Heavy stuff done. */
    PGMDynMapPopAutoSubset(pVCpu, iPrevSubset);
#endif

#ifdef PGMPOOL_WITH_MONITORING
    /*
     * Deregistering the monitoring.
     */
    if (pPage->fMonitored)
        rc = pgmPoolMonitorFlush(pPool, pPage);
#endif

    /*
     * Free the page.
     */
    Assert(pPage->iNext == NIL_PGMPOOL_IDX);
    pPage->iNext = pPool->iFreeHead;
    pPool->iFreeHead = pPage->idx;
    pPage->enmKind = PGMPOOLKIND_FREE;
    pPage->GCPhys = NIL_RTGCPHYS;
    pPage->fReusedFlushPending = false;

    pPool->cUsedPages--;
    STAM_PROFILE_STOP(&pPool->StatFlushPage, f);
    return rc;
}


/**
 * Frees a usage of a pool page.
 *
 * The caller is responsible to updating the user table so that it no longer
 * references the shadow page.
 *
 * @param   pPool       The pool.
 * @param   HCPhys      The HC physical address of the shadow page.
 * @param   iUser       The shadow page pool index of the user table.
 * @param   iUserTable  The index into the user table (shadowed).
 */
void pgmPoolFreeByPage(PPGMPOOL pPool, PPGMPOOLPAGE pPage, uint16_t iUser, uint32_t iUserTable)
{
    STAM_PROFILE_START(&pPool->StatFree, a);
    LogFlow(("pgmPoolFreeByPage: pPage=%p:{.Key=%RHp, .idx=%d, enmKind=%s} iUser=%#x iUserTable=%#x\n",
             pPage, pPage->Core.Key, pPage->idx, pgmPoolPoolKindToStr(pPage->enmKind), iUser, iUserTable));
    Assert(pPage->idx >= PGMPOOL_IDX_FIRST);
#ifdef PGMPOOL_WITH_USER_TRACKING
    pgmPoolTrackFreeUser(pPool, pPage, iUser, iUserTable);
#endif
#ifdef PGMPOOL_WITH_CACHE
    if (!pPage->fCached)
#endif
        pgmPoolFlushPage(pPool, pPage);
    STAM_PROFILE_STOP(&pPool->StatFree, a);
}


/**
 * Makes one or more free page free.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_POOL_FLUSHED if the pool was flushed.
 *
 * @param   pPool       The pool.
 * @param   enmKind     Page table kind
 * @param   iUser       The user of the page.
 */
static int pgmPoolMakeMoreFreePages(PPGMPOOL pPool, PGMPOOLKIND enmKind, uint16_t iUser)
{
    LogFlow(("pgmPoolMakeMoreFreePages: iUser=%#x\n", iUser));

    /*
     * If the pool isn't full grown yet, expand it.
     */
    if (    pPool->cCurPages < pPool->cMaxPages
#if defined(IN_RC)
        /* Hack alert: we can't deal with jumps to ring 3 when called from MapCR3 and allocating pages for PAE PDs. */
        &&  enmKind != PGMPOOLKIND_PAE_PD_FOR_PAE_PD
        &&  (enmKind < PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD || enmKind > PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD)
#endif
        )
    {
        STAM_PROFILE_ADV_SUSPEND(&pPool->StatAlloc, a);
#ifdef IN_RING3
        int rc = PGMR3PoolGrow(pPool->pVMR3);
#else
        int rc = CTXALLMID(VMM, CallHost)(pPool->CTX_SUFF(pVM), VMMCALLHOST_PGM_POOL_GROW, 0);
#endif
        if (RT_FAILURE(rc))
            return rc;
        STAM_PROFILE_ADV_RESUME(&pPool->StatAlloc, a);
        if (pPool->iFreeHead != NIL_PGMPOOL_IDX)
            return VINF_SUCCESS;
    }

#ifdef PGMPOOL_WITH_CACHE
    /*
     * Free one cached page.
     */
    return pgmPoolCacheFreeOne(pPool, iUser);
#else
    /*
     * Flush the pool.
     *
     * If we have tracking enabled, it should be possible to come up with
     * a cheap replacement strategy...
     */
    /* @todo This path no longer works (CR3 root pages will be flushed)!! */
    AssertCompileFailed();
    Assert(!CPUMIsGuestInLongMode(pVM));
    pgmPoolFlushAllInt(pPool);
    return VERR_PGM_POOL_FLUSHED;
#endif
}


/**
 * Allocates a page from the pool.
 *
 * This page may actually be a cached page and not in need of any processing
 * on the callers part.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if a NEW page was allocated.
 * @retval  VINF_PGM_CACHED_PAGE if a CACHED page was returned.
 * @retval  VERR_PGM_POOL_FLUSHED if the pool was flushed.
 * @param   pVM         The VM handle.
 * @param   GCPhys      The GC physical address of the page we're gonna shadow.
 *                      For 4MB and 2MB PD entries, it's the first address the
 *                      shadow PT is covering.
 * @param   enmKind     The kind of mapping.
 * @param   iUser       The shadow page pool index of the user table.
 * @param   iUserTable  The index into the user table (shadowed).
 * @param   ppPage      Where to store the pointer to the page. NULL is stored here on failure.
 */
int pgmPoolAlloc(PVM pVM, RTGCPHYS GCPhys, PGMPOOLKIND enmKind, uint16_t iUser, uint32_t iUserTable, PPPGMPOOLPAGE ppPage)
{
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    STAM_PROFILE_ADV_START(&pPool->StatAlloc, a);
    LogFlow(("pgmPoolAlloc: GCPhys=%RGp enmKind=%s iUser=%#x iUserTable=%#x\n", GCPhys, pgmPoolPoolKindToStr(enmKind), iUser, iUserTable));
    *ppPage = NULL;
    /** @todo CSAM/PGMPrefetchPage messes up here during CSAMR3CheckGates
     *  (TRPMR3SyncIDT) because of FF priority. Try fix that?
     *  Assert(!(pVM->pgm.s.fGlobalSyncFlags & PGM_GLOBAL_SYNC_CLEAR_PGM_POOL)); */

#ifdef PGMPOOL_WITH_CACHE
    if (pPool->fCacheEnabled)
    {
        int rc2 = pgmPoolCacheAlloc(pPool, GCPhys, enmKind, iUser, iUserTable, ppPage);
        if (RT_SUCCESS(rc2))
        {
            STAM_PROFILE_ADV_STOP(&pPool->StatAlloc, a);
            LogFlow(("pgmPoolAlloc: cached returns %Rrc *ppPage=%p:{.Key=%RHp, .idx=%d}\n", rc2, *ppPage, (*ppPage)->Core.Key, (*ppPage)->idx));
            return rc2;
        }
    }
#endif

    /*
     * Allocate a new one.
     */
    int         rc = VINF_SUCCESS;
    uint16_t    iNew = pPool->iFreeHead;
    if (iNew == NIL_PGMPOOL_IDX)
    {
        rc = pgmPoolMakeMoreFreePages(pPool, enmKind, iUser);
        if (RT_FAILURE(rc))
        {
            Log(("pgmPoolAlloc: returns %Rrc (Free)\n", rc));
            STAM_PROFILE_ADV_STOP(&pPool->StatAlloc, a);
            return rc;
        }
        iNew = pPool->iFreeHead;
        AssertReleaseReturn(iNew != NIL_PGMPOOL_IDX, VERR_INTERNAL_ERROR);
    }

    /* unlink the free head */
    PPGMPOOLPAGE pPage = &pPool->aPages[iNew];
    pPool->iFreeHead = pPage->iNext;
    pPage->iNext = NIL_PGMPOOL_IDX;

    /*
     * Initialize it.
     */
    pPool->cUsedPages++;                /* physical handler registration / pgmPoolTrackFlushGCPhysPTsSlow requirement. */
    pPage->enmKind = enmKind;
    pPage->GCPhys = GCPhys;
    pPage->fSeenNonGlobal = false;      /* Set this to 'true' to disable this feature. */
    pPage->fMonitored = false;
    pPage->fCached = false;
    pPage->fReusedFlushPending = false;
#ifdef PGMPOOL_WITH_MONITORING
    pPage->cModifications = 0;
    pPage->iModifiedNext = NIL_PGMPOOL_IDX;
    pPage->iModifiedPrev = NIL_PGMPOOL_IDX;
#else
    pPage->fCR3Mix = false;
#endif
#ifdef PGMPOOL_WITH_USER_TRACKING
    pPage->cPresent = 0;
    pPage->iFirstPresent = ~0;

    /*
     * Insert into the tracking and cache. If this fails, free the page.
     */
    int rc3 = pgmPoolTrackInsert(pPool, pPage, GCPhys, iUser, iUserTable);
    if (RT_FAILURE(rc3))
    {
        pPool->cUsedPages--;
        pPage->enmKind = PGMPOOLKIND_FREE;
        pPage->GCPhys = NIL_RTGCPHYS;
        pPage->iNext = pPool->iFreeHead;
        pPool->iFreeHead = pPage->idx;
        STAM_PROFILE_ADV_STOP(&pPool->StatAlloc, a);
        Log(("pgmPoolAlloc: returns %Rrc (Insert)\n", rc3));
        return rc3;
    }
#endif /* PGMPOOL_WITH_USER_TRACKING */

    /*
     * Commit the allocation, clear the page and return.
     */
#ifdef VBOX_WITH_STATISTICS
    if (pPool->cUsedPages > pPool->cUsedPagesHigh)
        pPool->cUsedPagesHigh = pPool->cUsedPages;
#endif

    if (!pPage->fZeroed)
    {
        STAM_PROFILE_START(&pPool->StatZeroPage, z);
        void *pv = PGMPOOL_PAGE_2_PTR(pVM, pPage);
        ASMMemZeroPage(pv);
        STAM_PROFILE_STOP(&pPool->StatZeroPage, z);
    }

    *ppPage = pPage;
    LogFlow(("pgmPoolAlloc: returns %Rrc *ppPage=%p:{.Key=%RHp, .idx=%d, .fCached=%RTbool, .fMonitored=%RTbool}\n",
             rc, pPage, pPage->Core.Key, pPage->idx, pPage->fCached, pPage->fMonitored));
    STAM_PROFILE_ADV_STOP(&pPool->StatAlloc, a);
    return rc;
}


/**
 * Frees a usage of a pool page.
 *
 * @param   pVM         The VM handle.
 * @param   HCPhys      The HC physical address of the shadow page.
 * @param   iUser       The shadow page pool index of the user table.
 * @param   iUserTable  The index into the user table (shadowed).
 */
void pgmPoolFree(PVM pVM, RTHCPHYS HCPhys, uint16_t iUser, uint32_t iUserTable)
{
    LogFlow(("pgmPoolFree: HCPhys=%RHp iUser=%#x iUserTable=%#x\n", HCPhys, iUser, iUserTable));
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    pgmPoolFreeByPage(pPool, pgmPoolGetPage(pPool, HCPhys), iUser, iUserTable);
}


/**
 * Gets a in-use page in the pool by it's physical address.
 *
 * @returns Pointer to the page.
 * @param   pVM     The VM handle.
 * @param   HCPhys  The HC physical address of the shadow page.
 * @remark  This function will NEVER return NULL. It will assert if HCPhys is invalid.
 */
PPGMPOOLPAGE pgmPoolGetPageByHCPhys(PVM pVM, RTHCPHYS HCPhys)
{
    /** @todo profile this! */
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    PPGMPOOLPAGE pPage = pgmPoolGetPage(pPool, HCPhys);
#ifndef DEBUG_bird /* extremely noisy */
    Log5(("pgmPoolGetPageByHCPhys: HCPhys=%RHp -> %p:{.idx=%d .GCPhys=%RGp .enmKind=%s}\n",
          HCPhys, pPage, pPage->idx, pPage->GCPhys, pgmPoolPoolKindToStr(pPage->enmKind)));
#endif
    return pPage;
}

#ifdef IN_RING3
/**
 * Flushes the entire cache.
 *
 * It will assert a global CR3 flush (FF) and assumes the caller is aware of this
 * and execute this CR3 flush.
 *
 * @param   pPool       The pool.
 */
void pgmPoolFlushAll(PVM pVM)
{
    LogFlow(("pgmPoolFlushAll:\n"));
    pgmPoolFlushAllInt(pVM->pgm.s.CTX_SUFF(pPool));
}
#endif /* IN_RING3 */

#ifdef LOG_ENABLED
static const char *pgmPoolPoolKindToStr(uint8_t enmKind)
{
    switch(enmKind)
    {
    case PGMPOOLKIND_INVALID:
        return "PGMPOOLKIND_INVALID";
    case PGMPOOLKIND_FREE:
        return "PGMPOOLKIND_FREE";
    case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
        return "PGMPOOLKIND_32BIT_PT_FOR_PHYS";
    case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
        return "PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT";
    case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
        return "PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB";
    case PGMPOOLKIND_PAE_PT_FOR_PHYS:
        return "PGMPOOLKIND_PAE_PT_FOR_PHYS";
    case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
        return "PGMPOOLKIND_PAE_PT_FOR_32BIT_PT";
    case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
        return "PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB";
    case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
        return "PGMPOOLKIND_PAE_PT_FOR_PAE_PT";
    case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
        return "PGMPOOLKIND_PAE_PT_FOR_PAE_2MB";
    case PGMPOOLKIND_32BIT_PD:
        return "PGMPOOLKIND_32BIT_PD";
    case PGMPOOLKIND_32BIT_PD_PHYS:
        return "PGMPOOLKIND_32BIT_PD_PHYS";
    case PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD:
        return "PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD";
    case PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD:
        return "PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD";
    case PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD:
        return "PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD";
    case PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD:
        return "PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD";
    case PGMPOOLKIND_PAE_PD_FOR_PAE_PD:
        return "PGMPOOLKIND_PAE_PD_FOR_PAE_PD";
    case PGMPOOLKIND_PAE_PD_PHYS:
        return "PGMPOOLKIND_PAE_PD_PHYS";
    case PGMPOOLKIND_PAE_PDPT_FOR_32BIT:
        return "PGMPOOLKIND_PAE_PDPT_FOR_32BIT";
    case PGMPOOLKIND_PAE_PDPT:
        return "PGMPOOLKIND_PAE_PDPT";
    case PGMPOOLKIND_PAE_PDPT_PHYS:
        return "PGMPOOLKIND_PAE_PDPT_PHYS";
    case PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT:
        return "PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT";
    case PGMPOOLKIND_64BIT_PDPT_FOR_PHYS:
        return "PGMPOOLKIND_64BIT_PDPT_FOR_PHYS";
    case PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD:
        return "PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD";
    case PGMPOOLKIND_64BIT_PD_FOR_PHYS:
        return "PGMPOOLKIND_64BIT_PD_FOR_PHYS";
    case PGMPOOLKIND_64BIT_PML4:
        return "PGMPOOLKIND_64BIT_PML4";
    case PGMPOOLKIND_EPT_PDPT_FOR_PHYS:
        return "PGMPOOLKIND_EPT_PDPT_FOR_PHYS";
    case PGMPOOLKIND_EPT_PD_FOR_PHYS:
        return "PGMPOOLKIND_EPT_PD_FOR_PHYS";
    case PGMPOOLKIND_EPT_PT_FOR_PHYS:
        return "PGMPOOLKIND_EPT_PT_FOR_PHYS";
    case PGMPOOLKIND_ROOT_NESTED:
        return "PGMPOOLKIND_ROOT_NESTED";
    }
    return "Unknown kind!";
}
#endif /* LOG_ENABLED*/
