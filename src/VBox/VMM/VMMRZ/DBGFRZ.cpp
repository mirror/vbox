/* $Id$ */
/** @file
 * DBGF - Debugger Facility, RZ part.
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
#ifdef IN_RC
# include <VBox/vmm/trpm.h>
#endif
#include <VBox/log.h>
#include "DBGFInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/err.h>
#include <iprt/assert.h>

#ifdef IN_RC
DECLASM(void) TRPMRCHandlerAsmTrap03(void);
#endif


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
VMMRZ_INT_DECL(int) DBGFRZTrap01Handler(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, RTGCUINTREG uDr6, bool fAltStepping)
{
#ifdef IN_RC
    const bool fInHyper = !(pRegFrame->ss.Sel & X86_SEL_RPL) && !pRegFrame->eflags.Bits.u1VM;
#else
    NOREF(pRegFrame);
    const bool fInHyper = false;
#endif

    /** @todo Intel docs say that X86_DR6_BS has the highest priority... */
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

                return fInHyper ? VINF_EM_DBG_HYPER_BREAKPOINT : VINF_EM_DBG_BREAKPOINT;
            }
#else
            if (    ((uint32_t)uDr6 & RT_BIT_32(iBp))
                &&  pVM->dbgf.s.aHwBreakpoints[iBp].hBp != NIL_DBGFBP)
            {
                pVCpu->dbgf.s.hBpActive = pVM->dbgf.s.aHwBreakpoints[iBp].hBp;
                pVCpu->dbgf.s.fSingleSteppingRaw = false;
                LogFlow(("DBGFRZTrap03Handler: hit hw breakpoint %x at %04x:%RGv\n",
                         pVM->dbgf.s.aHwBreakpoints[iBp].hBp, pRegFrame->cs.Sel, pRegFrame->rip));

                return fInHyper ? VINF_EM_DBG_HYPER_BREAKPOINT : VINF_EM_DBG_BREAKPOINT;
            }
#endif
        }
    }

    /*
     * Single step?
     * Are we single stepping or is it the guest?
     */
    if (    (uDr6 & X86_DR6_BS)
        &&  (fInHyper || pVCpu->dbgf.s.fSingleSteppingRaw || fAltStepping))
    {
        pVCpu->dbgf.s.fSingleSteppingRaw = false;
        LogFlow(("DBGFRZTrap01Handler: single step at %04x:%RGv\n", pRegFrame->cs.Sel, pRegFrame->rip));
        return fInHyper ? VINF_EM_DBG_HYPER_STEPPED : VINF_EM_DBG_STEPPED;
    }

#ifdef IN_RC
    /*
     * Either an ICEBP in hypervisor code or a guest related debug exception
     * of sorts.
     */
    if (RT_UNLIKELY(fInHyper))
    {
        /*
         * Is this a guest debug event that was delayed past a ring transition?
         *
         * Since we do no allow sysenter/syscall in raw-mode, the  only
         * non-trap/fault type transitions that can occur are thru interrupt gates.
         * Of those, only INT3 (#BP) has a DPL other than 0 with a CS.RPL of 0.
         * See bugref:9171 and bs3-cpu-weird-1 for more details.
         *
         * We need to reconstruct the guest register state from the hypervisor one
         * here, so here is the layout of the IRET frame on the stack:
         *    20:[8] GS          (V86 only)
         *    1C:[7] FS          (V86 only)
         *    18:[6] DS          (V86 only)
         *    14:[5] ES          (V86 only)
         *    10:[4] SS
         *    0c:[3] ESP
         *    08:[2] EFLAGS
         *    04:[1] CS
         *    00:[0] EIP
         */
        if (pRegFrame->rip == (uintptr_t)TRPMRCHandlerAsmTrap03)
        {
            uint32_t const *pu32Stack = (uint32_t const *)pRegFrame->esp;
            if (   (pu32Stack[2] & X86_EFL_VM)
                || (pu32Stack[1] & X86_SEL_RPL))
            {
                LogFlow(("DBGFRZTrap01Handler: Detected guest #DB delayed past ring transition %04x:%RX32 %#x\n",
                         pu32Stack[1] & 0xffff, pu32Stack[0], pu32Stack[2]));
                PCPUMCTX pGstCtx = CPUMQueryGuestCtxPtr(pVCpu);
                pGstCtx->rip      = pu32Stack[0];
                pGstCtx->cs.Sel   = pu32Stack[1];
                pGstCtx->eflags.u = pu32Stack[2];
                pGstCtx->rsp      = pu32Stack[3];
                pGstCtx->ss.Sel   = pu32Stack[4];
                if (pu32Stack[2] & X86_EFL_VM)
                {
                    pGstCtx->es.Sel = pu32Stack[5];
                    pGstCtx->ds.Sel = pu32Stack[6];
                    pGstCtx->fs.Sel = pu32Stack[7];
                    pGstCtx->gs.Sel = pu32Stack[8];
                }
                else
                {
                    pGstCtx->es.Sel = pRegFrame->es.Sel;
                    pGstCtx->ds.Sel = pRegFrame->ds.Sel;
                    pGstCtx->fs.Sel = pRegFrame->fs.Sel;
                    pGstCtx->gs.Sel = pRegFrame->gs.Sel;
                }
                pGstCtx->rax      = pRegFrame->rax;
                pGstCtx->rcx      = pRegFrame->rcx;
                pGstCtx->rdx      = pRegFrame->rdx;
                pGstCtx->rbx      = pRegFrame->rbx;
                pGstCtx->rsi      = pRegFrame->rsi;
                pGstCtx->rdi      = pRegFrame->rdi;
                pGstCtx->rbp      = pRegFrame->rbp;

                /*
                 * We should assert a #BP followed by a #DB here, but TRPM cannot
                 * do that.  So, we'll just assert the #BP and ignore the #DB, even
                 * if that isn't strictly correct.
                 */
                TRPMResetTrap(pVCpu);
                TRPMAssertTrap(pVCpu, X86_XCPT_BP, TRPM_SOFTWARE_INT);
                return VINF_EM_RAW_GUEST_TRAP;
            }
        }

        LogFlow(("DBGFRZTrap01Handler: Unknown bp at %04x:%RGv\n", pRegFrame->cs.Sel, pRegFrame->rip));
        return VERR_DBGF_HYPER_DB_XCPT;
    }
#endif

    LogFlow(("DBGFRZTrap01Handler: guest debug event %#x at %04x:%RGv!\n", (uint32_t)uDr6, pRegFrame->cs.Sel, pRegFrame->rip));
    return VINF_EM_RAW_GUEST_TRAP;
}

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
# endif


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
 * @param   fInHyper    Flag whether the breakpoint triggered in hypervisor code.
 */
DECLINLINE(int) dbgfRZBpHit(PVMCC pVM, PVMCPUCC pVCpu, PCPUMCTXCORE pRegFrame,
                            DBGFBP hBp, PDBGFBPINT pBp, PDBGFBPINTR0 pBpR0, bool fInHyper)
{
    uint64_t cHits = ASMAtomicIncU64(&pBp->Pub.cHits);
    pVCpu->dbgf.s.hBpActive = hBp;

    /** @todo Owner handling. */
    RT_NOREF(pVM, pRegFrame, pBpR0);

    LogFlow(("dbgfRZBpHit: hit breakpoint %u at %04x:%RGv cHits=0x%RX64\n",
             hBp, pRegFrame->cs.Sel, pRegFrame->rip, cHits));
    return fInHyper
         ? VINF_EM_DBG_HYPER_BREAKPOINT
         : VINF_EM_DBG_BREAKPOINT;
}
#endif /* !VBOX_WITH_LOTS_OF_DBGF_BPS */

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
VMMRZ_INT_DECL(int) DBGFRZTrap03Handler(PVMCC pVM, PVMCPUCC pVCpu, PCPUMCTXCORE pRegFrame)
{
#ifdef IN_RC
    const bool fInHyper = !(pRegFrame->ss.Sel & X86_SEL_RPL) && !pRegFrame->eflags.Bits.u1VM;
#else
    const bool fInHyper = false;
#endif

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
# ifdef IN_RC
                                              pRegFrame->eip - 1,
# else
                                              pRegFrame->rip /* no -1 in R0 */,
# endif
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
                return fInHyper
                     ? VINF_EM_DBG_HYPER_BREAKPOINT
                     : VINF_EM_DBG_BREAKPOINT;
            }
            iBp++;
        }
    }
#else
# ifdef IN_RC
# error "You lucky person have the pleasure to implement the raw mode part for this!"
# endif

    if (pVM->dbgfr0.s.CTX_SUFF(paBpLocL1))
    {
        RTGCPTR GCPtrBp;
        int rc = SELMValidateAndConvertCSAddr(pVCpu, pRegFrame->eflags, pRegFrame->ss.Sel, pRegFrame->cs.Sel, &pRegFrame->cs,
# ifdef IN_RC
                                              pRegFrame->eip - 1,
# else
                                              pRegFrame->rip /* no -1 in R0 */,
# endif
                                              &GCPtrBp);
        AssertRCReturn(rc, rc);

        const uint16_t idxL1      = DBGF_BP_INT3_L1_IDX_EXTRACT_FROM_ADDR(GCPtrBp);
        const uint32_t u32L1Entry = ASMAtomicReadU32(&pVM->dbgfr0.s.CTX_SUFF(paBpLocL1)[idxL1]);

        LogFlowFunc(("GCPtrBp=%RGv idxL1=%u u32L1Entry=%#x\n", GCPtrBp, idxL1, u32L1Entry));
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
                        return dbgfRZBpHit(pVM, pVCpu, pRegFrame, hBp, pBp, pBpR0, fInHyper);
                    /* else Genuine guest trap. */
                }
                /** @todo else Guru meditation */
            }
            else if (u8Type == DBGF_BP_INT3_L1_ENTRY_TYPE_L2_IDX)
            {
                /** @todo Walk the L2 tree searching for the correct spot. */
            }
            /** @todo else Guru meditation */
        }
    }
#endif /* !VBOX_WITH_LOTS_OF_DBGF_BPS */

    return fInHyper
         ? VINF_EM_DBG_HYPER_ASSERTION
         : VINF_EM_RAW_GUEST_TRAP;
}

