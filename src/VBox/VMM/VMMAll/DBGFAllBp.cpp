/* $Id$ */
/** @file
 * DBGF - Debugger Facility, All Context breakpoint management part.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/selm.h>
#include <VBox/log.h>
#include "DBGFInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/err.h>
#include <iprt/assert.h>


#ifdef IN_RC
# error "You lucky person have the pleasure to implement the raw mode part for this!"
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

#ifdef VBOX_WITH_LOTS_OF_DBGF_BPS
# ifdef IN_RING0
/**
 * Returns the internal breakpoint state for the given handle.
 *
 * @returns Pointer to the internal breakpoint state or NULL if the handle is invalid.
 * @param   pVM                 The ring-0 VM structure pointer.
 * @param   hBp                 The breakpoint handle to resolve.
 * @param   ppBpR0              Where to store the pointer to the ring-0 only part of the breakpoint
 *                              on success, optional.
 */
DECLINLINE(PDBGFBPINT) dbgfR0BpGetByHnd(PVMCC pVM, DBGFBP hBp, PDBGFBPINTR0 *ppBpR0)
{
    uint32_t idChunk  = DBGF_BP_HND_GET_CHUNK_ID(hBp);
    uint32_t idxEntry = DBGF_BP_HND_GET_ENTRY(hBp);

    AssertReturn(idChunk < DBGF_BP_CHUNK_COUNT, NULL);
    AssertReturn(idxEntry < DBGF_BP_COUNT_PER_CHUNK, NULL);

    PDBGFBPCHUNKR0 pBpChunk = &pVM->dbgfr0.s.aBpChunks[idChunk];
    AssertPtrReturn(pBpChunk->paBpBaseSharedR0, NULL);

    if (ppBpR0)
        *ppBpR0 = &pBpChunk->paBpBaseR0Only[idxEntry];
    return &pBpChunk->paBpBaseSharedR0[idxEntry];
}


/**
 * Executes the actions associated with the given breakpoint.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @param   hBp         The breakpoint handle which hit.
 * @param   pBp         The shared breakpoint state.
 * @param   pBpR0       The ring-0 only breakpoint state.
 */
DECLINLINE(int) dbgfR0BpHit(PVMCC pVM, PVMCPUCC pVCpu, PCPUMCTXCORE pRegFrame,
                            DBGFBP hBp, PDBGFBPINT pBp, PDBGFBPINTR0 pBpR0)
{
    uint64_t cHits = ASMAtomicIncU64(&pBp->Pub.cHits);
    pVCpu->dbgf.s.hBpActive = hBp;

    /** @todo Owner handling. */
    RT_NOREF(pVM, pRegFrame, pBpR0);

    LogFlow(("dbgfRZBpHit: hit breakpoint %u at %04x:%RGv cHits=0x%RX64\n",
             hBp, pRegFrame->cs.Sel, pRegFrame->rip, cHits));
    return VINF_EM_DBG_BREAKPOINT;
}


/**
 * Returns the pointer to the L2 table entry from the given index.
 *
 * @returns Current context pointer to the L2 table entry or NULL if the provided index value is invalid.
 * @param   pVM         The cross context VM structure.
 * @param   idxL2       The L2 table index to resolve.
 *
 * @note The content of the resolved L2 table entry is not validated!.
 */
DECLINLINE(PCDBGFBPL2ENTRY) dbgfR0BpL2GetByIdx(PVMCC pVM, uint32_t idxL2)
{
    uint32_t idChunk  = DBGF_BP_L2_IDX_GET_CHUNK_ID(idxL2);
    uint32_t idxEntry = DBGF_BP_L2_IDX_GET_ENTRY(idxL2);

    AssertReturn(idChunk < DBGF_BP_L2_TBL_CHUNK_COUNT, NULL);
    AssertReturn(idxEntry < DBGF_BP_L2_TBL_ENTRIES_PER_CHUNK, NULL);

    PDBGFBPL2TBLCHUNKR0 pL2Chunk = &pVM->dbgfr0.s.aBpL2TblChunks[idChunk];
    AssertPtrReturn(pL2Chunk->paBpL2TblBaseSharedR0, NULL);

    return &pL2Chunk->CTX_SUFF(paBpL2TblBaseShared)[idxEntry];
}


/**
 * Walks the L2 table starting at the given root index searching for the given key.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @param   idxL2Root   L2 table index of the table root.
 * @param   GCPtrKey    The key to search for.
 */
static int dbgfBpL2Walk(PVMCC pVM, PVMCPUCC pVCpu, PCPUMCTXCORE pRegFrame,
                        uint32_t idxL2Root, RTGCUINTPTR GCPtrKey)
{
    /** @todo We don't use the depth right now but abort the walking after a fixed amount of levels. */
    uint8_t iDepth = 32;
    PCDBGFBPL2ENTRY pL2Entry = dbgfR0BpL2GetByIdx(pVM, idxL2Root);

    while (RT_LIKELY(   iDepth-- > 0
                     && pL2Entry))
    {
        /* Make a copy of the entry before verification. */
        DBGFBPL2ENTRY L2Entry;
        L2Entry.u64GCPtrKeyAndBpHnd1       = ASMAtomicReadU64((volatile uint64_t *)&pL2Entry->u64GCPtrKeyAndBpHnd1);
        L2Entry.u64LeftRightIdxDepthBpHnd2 = ASMAtomicReadU64((volatile uint64_t *)&pL2Entry->u64LeftRightIdxDepthBpHnd2);

        RTGCUINTPTR GCPtrL2Entry = DBGF_BP_L2_ENTRY_GET_GCPTR(L2Entry.u64GCPtrKeyAndBpHnd1);
        if (GCPtrKey == GCPtrL2Entry)
        {
            DBGFBP hBp = DBGF_BP_L2_ENTRY_GET_BP_HND(L2Entry.u64GCPtrKeyAndBpHnd1, L2Entry.u64LeftRightIdxDepthBpHnd2);

            /* Query the internal breakpoint state from the handle. */
            PDBGFBPINTR0 pBpR0 = NULL;
            PDBGFBPINT pBp = dbgfR0BpGetByHnd(pVM, hBp, &pBpR0);
            if (   pBp
                && DBGF_BP_PUB_GET_TYPE(pBp->Pub.fFlagsAndType) == DBGFBPTYPE_INT3)
                return dbgfR0BpHit(pVM, pVCpu, pRegFrame, hBp, pBp, pBpR0);

            /* The entry got corrupted, just abort. */
            return VERR_DBGF_BP_L2_LOOKUP_FAILED;
        }

        /* Not found, get to the next level. */
        uint32_t idxL2Next =   (GCPtrKey < GCPtrL2Entry)
                             ? DBGF_BP_L2_ENTRY_GET_IDX_LEFT(L2Entry.u64LeftRightIdxDepthBpHnd2)
                             : DBGF_BP_L2_ENTRY_GET_IDX_RIGHT(L2Entry.u64LeftRightIdxDepthBpHnd2);
        /* It is genuine guest trap or we hit some assertion if we are at the end. */
        if (idxL2Next == DBGF_BP_L2_ENTRY_IDX_END)
            return VINF_EM_RAW_GUEST_TRAP;

        pL2Entry = dbgfR0BpL2GetByIdx(pVM, idxL2Next);
    }

    return VERR_DBGF_BP_L2_LOOKUP_FAILED;
}
# endif /* !IN_RING0 */
#endif /* !VBOX_WITH_LOTS_OF_DBGF_BPS */


/**
 * \#DB (Debug event) handler.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pRegFrame       Pointer to the register frame for the trap.
 * @param   uDr6            The DR6 hypervisor register value.
 * @param   fAltStepping    Alternative stepping indicator.
 */
VMM_INT_DECL(int) DBGFTrap01Handler(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, RTGCUINTREG uDr6, bool fAltStepping)
{
    /** @todo Intel docs say that X86_DR6_BS has the highest priority... */
    RT_NOREF(pRegFrame);

    /*
     * A breakpoint?
     */
    AssertCompile(X86_DR6_B0 == 1 && X86_DR6_B1 == 2 && X86_DR6_B2 == 4 && X86_DR6_B3 == 8);
    if (   (uDr6 & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3))
        && pVM->dbgf.s.cEnabledHwBreakpoints > 0)
    {
        for (unsigned iBp = 0; iBp < RT_ELEMENTS(pVM->dbgf.s.aHwBreakpoints); iBp++)
        {
#ifndef VBOX_WITH_LOTS_OF_DBGF_BPS
            if (    ((uint32_t)uDr6 & RT_BIT_32(iBp))
                &&  pVM->dbgf.s.aHwBreakpoints[iBp].enmType == DBGFBPTYPE_REG)
            {
                pVCpu->dbgf.s.iActiveBp = pVM->dbgf.s.aHwBreakpoints[iBp].iBp;
                pVCpu->dbgf.s.fSingleSteppingRaw = false;
                LogFlow(("DBGFRZTrap03Handler: hit hw breakpoint %d at %04x:%RGv\n",
                         pVM->dbgf.s.aHwBreakpoints[iBp].iBp, pRegFrame->cs.Sel, pRegFrame->rip));

                return VINF_EM_DBG_BREAKPOINT;
            }
#else
            if (    ((uint32_t)uDr6 & RT_BIT_32(iBp))
                &&  pVM->dbgf.s.aHwBreakpoints[iBp].hBp != NIL_DBGFBP)
            {
                pVCpu->dbgf.s.hBpActive = pVM->dbgf.s.aHwBreakpoints[iBp].hBp;
                pVCpu->dbgf.s.fSingleSteppingRaw = false;
                LogFlow(("DBGFRZTrap03Handler: hit hw breakpoint %x at %04x:%RGv\n",
                         pVM->dbgf.s.aHwBreakpoints[iBp].hBp, pRegFrame->cs.Sel, pRegFrame->rip));

                return VINF_EM_DBG_BREAKPOINT;
            }
#endif
        }
    }

    /*
     * Single step?
     * Are we single stepping or is it the guest?
     */
    if (    (uDr6 & X86_DR6_BS)
        &&  (pVCpu->dbgf.s.fSingleSteppingRaw || fAltStepping))
    {
        pVCpu->dbgf.s.fSingleSteppingRaw = false;
        LogFlow(("DBGFRZTrap01Handler: single step at %04x:%RGv\n", pRegFrame->cs.Sel, pRegFrame->rip));
        return VINF_EM_DBG_STEPPED;
    }

    LogFlow(("DBGFRZTrap01Handler: guest debug event %#x at %04x:%RGv!\n", (uint32_t)uDr6, pRegFrame->cs.Sel, pRegFrame->rip));
    return VINF_EM_RAW_GUEST_TRAP;
}


/**
 * \#BP (Breakpoint) handler.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pRegFrame   Pointer to the register frame for the trap.
 */
VMM_INT_DECL(int) DBGFTrap03Handler(PVMCC pVM, PVMCPUCC pVCpu, PCPUMCTXCORE pRegFrame)
{
#ifndef VBOX_WITH_LOTS_OF_DBGF_BPS
    /*
     * Get the trap address and look it up in the breakpoint table.
     * Don't bother if we don't have any breakpoints.
     */
    unsigned cToSearch = pVM->dbgf.s.Int3.cToSearch;
    if (cToSearch > 0)
    {
        RTGCPTR pPc;
        int rc = SELMValidateAndConvertCSAddr(pVCpu, pRegFrame->eflags, pRegFrame->ss.Sel, pRegFrame->cs.Sel, &pRegFrame->cs,
                                              pRegFrame->rip /* no -1 in R0 */,
                                              &pPc);
        AssertRCReturn(rc, rc);

        unsigned iBp = pVM->dbgf.s.Int3.iStartSearch;
        while (cToSearch-- > 0)
        {
            if (   pVM->dbgf.s.aBreakpoints[iBp].u.GCPtr == (RTGCUINTPTR)pPc
                && pVM->dbgf.s.aBreakpoints[iBp].enmType == DBGFBPTYPE_INT3)
            {
                pVM->dbgf.s.aBreakpoints[iBp].cHits++;
                pVCpu->dbgf.s.iActiveBp = pVM->dbgf.s.aBreakpoints[iBp].iBp;

                LogFlow(("DBGFRZTrap03Handler: hit breakpoint %d at %RGv (%04x:%RGv) cHits=0x%RX64\n",
                         pVM->dbgf.s.aBreakpoints[iBp].iBp, pPc, pRegFrame->cs.Sel, pRegFrame->rip,
                         pVM->dbgf.s.aBreakpoints[iBp].cHits));
                return VINF_EM_DBG_BREAKPOINT;
            }
            iBp++;
        }
    }
#else
#ifndef IN_RING0
# error "Todo"
#endif
    if (pVM->dbgfr0.s.CTX_SUFF(paBpLocL1))
    {
        RTGCPTR GCPtrBp;
        int rc = SELMValidateAndConvertCSAddr(pVCpu, pRegFrame->eflags, pRegFrame->ss.Sel, pRegFrame->cs.Sel, &pRegFrame->cs,
                                              pRegFrame->rip /* no -1 in R0 */,
                                              &GCPtrBp);
        AssertRCReturn(rc, rc);

        const uint16_t idxL1      = DBGF_BP_INT3_L1_IDX_EXTRACT_FROM_ADDR(GCPtrBp);
        const uint32_t u32L1Entry = ASMAtomicReadU32(&pVM->dbgfr0.s.CTX_SUFF(paBpLocL1)[idxL1]);

        LogFlowFunc(("GCPtrBp=%RGv idxL1=%u u32L1Entry=%#x\n", GCPtrBp, idxL1, u32L1Entry));
        rc = VINF_EM_RAW_GUEST_TRAP;
        if (u32L1Entry != DBGF_BP_INT3_L1_ENTRY_TYPE_NULL)
        {
            uint8_t u8Type = DBGF_BP_INT3_L1_ENTRY_GET_TYPE(u32L1Entry);
            if (u8Type == DBGF_BP_INT3_L1_ENTRY_TYPE_BP_HND)
            {
                DBGFBP hBp = DBGF_BP_INT3_L1_ENTRY_GET_BP_HND(u32L1Entry);

                /* Query the internal breakpoint state from the handle. */
                PDBGFBPINTR0 pBpR0 = NULL;
                PDBGFBPINT pBp = dbgfR0BpGetByHnd(pVM, hBp, &pBpR0);
                if (   pBp
                    && DBGF_BP_PUB_GET_TYPE(pBp->Pub.fFlagsAndType) == DBGFBPTYPE_INT3)
                {
                    if (pBp->Pub.u.Int3.GCPtr == (RTGCUINTPTR)GCPtrBp)
                        rc = dbgfR0BpHit(pVM, pVCpu, pRegFrame, hBp, pBp, pBpR0);
                    /* else: Genuine guest trap. */
                }
                else /* Invalid breakpoint handle or not an int3 breakpoint. */
                    rc = VERR_DBGF_BP_L1_LOOKUP_FAILED;
            }
            else if (u8Type == DBGF_BP_INT3_L1_ENTRY_TYPE_L2_IDX)
                rc = dbgfBpL2Walk(pVM, pVCpu, pRegFrame, DBGF_BP_INT3_L1_ENTRY_GET_L2_IDX(u32L1Entry),
                                  DBGF_BP_INT3_L2_KEY_EXTRACT_FROM_ADDR((RTGCUINTPTR)GCPtrBp));
            else /* Some invalid type. */
                rc = VERR_DBGF_BP_L1_LOOKUP_FAILED;
        }
        /* else: Genuine guest trap. */
    }
#endif /* !VBOX_WITH_LOTS_OF_DBGF_BPS */

    return VINF_EM_RAW_GUEST_TRAP;
}

