/* $Id$ */
/** @file
 * CPUM - CPU Monitor(/Manager) - Getters and Setters.
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
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/cpum.h>
#include <VBox/patm.h>
#include <VBox/dbgf.h>
#include <VBox/mm.h>
#include "CPUMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/dis.h>
#include <VBox/log.h>
#include <VBox/hwaccm.h>
#include <VBox/tm.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#ifdef IN_RING3
#include <iprt/thread.h>
#endif

/** Disable stack frame pointer generation here. */
#if defined(_MSC_VER) && !defined(DEBUG)
# pragma optimize("y", off)
#endif


/**
 * Sets or resets an alternative hypervisor context core.
 *
 * This is called when we get a hypervisor trap set switch the context
 * core with the trap frame on the stack. It is called again to reset
 * back to the default context core when resuming hypervisor execution.
 *
 * @param   pVCpu       The VMCPU handle.
 * @param   pCtxCore    Pointer to the alternative context core or NULL
 *                      to go back to the default context core.
 */
VMMDECL(void) CPUMHyperSetCtxCore(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    LogFlow(("CPUMHyperSetCtxCore: %p/%p/%p -> %p\n", pVCpu->cpum.s.CTX_SUFF(pHyperCore), pCtxCore));
    if (!pCtxCore)
    {
        pCtxCore = CPUMCTX2CORE(&pVCpu->cpum.s.Hyper);
        pVCpu->cpum.s.pHyperCoreR3 = (R3PTRTYPE(PCPUMCTXCORE))VM_R3_ADDR(pVM, pCtxCore);
        pVCpu->cpum.s.pHyperCoreR0 = (R0PTRTYPE(PCPUMCTXCORE))VM_R0_ADDR(pVM, pCtxCore);
        pVCpu->cpum.s.pHyperCoreRC = (RCPTRTYPE(PCPUMCTXCORE))VM_RC_ADDR(pVM, pCtxCore);
    }
    else
    {
        pVCpu->cpum.s.pHyperCoreR3 = (R3PTRTYPE(PCPUMCTXCORE))MMHyperCCToR3(pVM, pCtxCore);
        pVCpu->cpum.s.pHyperCoreR0 = (R0PTRTYPE(PCPUMCTXCORE))MMHyperCCToR0(pVM, pCtxCore);
        pVCpu->cpum.s.pHyperCoreRC = (RCPTRTYPE(PCPUMCTXCORE))MMHyperCCToRC(pVM, pCtxCore);
    }
}


/**
 * Gets the pointer to the internal CPUMCTXCORE structure for the hypervisor.
 * This is only for reading in order to save a few calls.
 *
 * @param   pVM         Handle to the virtual machine.
 */
VMMDECL(PCCPUMCTXCORE) CPUMGetHyperCtxCore(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore);
}


/**
 * Queries the pointer to the internal CPUMCTX structure for the hypervisor.
 *
 * @returns VBox status code.
 * @param   pVM         Handle to the virtual machine.
 * @param   ppCtx       Receives the hyper CPUMCTX pointer when successful.
 *
 * @deprecated  This will *not* (and has never) given the right picture of the
 *              hypervisor register state. With CPUMHyperSetCtxCore() this is
 *              getting much worse. So, use the individual functions for getting
 *              and esp. setting the hypervisor registers.
 */
VMMDECL(int) CPUMQueryHyperCtxPtr(PVMCPU pVCpu, PCPUMCTX *ppCtx)
{
    *ppCtx = &pVCpu->cpum.s.Hyper;
    return VINF_SUCCESS;
}


VMMDECL(void) CPUMSetHyperGDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit)
{
    pVCpu->cpum.s.Hyper.gdtr.cbGdt = limit;
    pVCpu->cpum.s.Hyper.gdtr.pGdt  = addr;
    pVCpu->cpum.s.Hyper.gdtrPadding = 0;
}


VMMDECL(void) CPUMSetHyperIDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit)
{
    pVCpu->cpum.s.Hyper.idtr.cbIdt = limit;
    pVCpu->cpum.s.Hyper.idtr.pIdt = addr;
    pVCpu->cpum.s.Hyper.idtrPadding = 0;
}


VMMDECL(void) CPUMSetHyperCR3(PVMCPU pVCpu, uint32_t cr3)
{
    pVCpu->cpum.s.Hyper.cr3 = cr3;

#ifdef IN_RC
    /* Update the current CR3. */
    ASMSetCR3(cr3);
#endif
}

VMMDECL(uint32_t) CPUMGetHyperCR3(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Hyper.cr3;
}


VMMDECL(void) CPUMSetHyperCS(PVMCPU pVCpu, RTSEL SelCS)
{
    pVCpu->cpum.s.CTX_SUFF(pHyperCore)->cs = SelCS;
}


VMMDECL(void) CPUMSetHyperDS(PVMCPU pVCpu, RTSEL SelDS)
{
    pVCpu->cpum.s.CTX_SUFF(pHyperCore)->ds = SelDS;
}


VMMDECL(void) CPUMSetHyperES(PVMCPU pVCpu, RTSEL SelES)
{
    pVCpu->cpum.s.CTX_SUFF(pHyperCore)->es = SelES;
}


VMMDECL(void) CPUMSetHyperFS(PVMCPU pVCpu, RTSEL SelFS)
{
    pVCpu->cpum.s.CTX_SUFF(pHyperCore)->fs = SelFS;
}


VMMDECL(void) CPUMSetHyperGS(PVMCPU pVCpu, RTSEL SelGS)
{
    pVCpu->cpum.s.CTX_SUFF(pHyperCore)->gs = SelGS;
}


VMMDECL(void) CPUMSetHyperSS(PVMCPU pVCpu, RTSEL SelSS)
{
    pVCpu->cpum.s.CTX_SUFF(pHyperCore)->ss = SelSS;
}


VMMDECL(void) CPUMSetHyperESP(PVMCPU pVCpu, uint32_t u32ESP)
{
    pVCpu->cpum.s.CTX_SUFF(pHyperCore)->esp = u32ESP;
}


VMMDECL(int) CPUMSetHyperEFlags(PVMCPU pVCpu, uint32_t Efl)
{
    pVCpu->cpum.s.CTX_SUFF(pHyperCore)->eflags.u32 = Efl;
    return VINF_SUCCESS;
}


VMMDECL(void) CPUMSetHyperEIP(PVMCPU pVCpu, uint32_t u32EIP)
{
    pVCpu->cpum.s.CTX_SUFF(pHyperCore)->eip = u32EIP;
}


VMMDECL(void) CPUMSetHyperTR(PVMCPU pVCpu, RTSEL SelTR)
{
    pVCpu->cpum.s.Hyper.tr = SelTR;
}


VMMDECL(void) CPUMSetHyperLDTR(PVMCPU pVCpu, RTSEL SelLDTR)
{
    pVCpu->cpum.s.Hyper.ldtr = SelLDTR;
}


VMMDECL(void) CPUMSetHyperDR0(PVMCPU pVCpu, RTGCUINTREG uDr0)
{
    pVCpu->cpum.s.Hyper.dr[0] = uDr0;
    /** @todo in GC we must load it! */
}


VMMDECL(void) CPUMSetHyperDR1(PVMCPU pVCpu, RTGCUINTREG uDr1)
{
    pVCpu->cpum.s.Hyper.dr[1] = uDr1;
    /** @todo in GC we must load it! */
}


VMMDECL(void) CPUMSetHyperDR2(PVMCPU pVCpu, RTGCUINTREG uDr2)
{
    pVCpu->cpum.s.Hyper.dr[2] = uDr2;
    /** @todo in GC we must load it! */
}


VMMDECL(void) CPUMSetHyperDR3(PVMCPU pVCpu, RTGCUINTREG uDr3)
{
    pVCpu->cpum.s.Hyper.dr[3] = uDr3;
    /** @todo in GC we must load it! */
}


VMMDECL(void) CPUMSetHyperDR6(PVMCPU pVCpu, RTGCUINTREG uDr6)
{
    pVCpu->cpum.s.Hyper.dr[6] = uDr6;
    /** @todo in GC we must load it! */
}


VMMDECL(void) CPUMSetHyperDR7(PVMCPU pVCpu, RTGCUINTREG uDr7)
{
    pVCpu->cpum.s.Hyper.dr[7] = uDr7;
    /** @todo in GC we must load it! */
}


VMMDECL(RTSEL) CPUMGetHyperCS(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->cs;
}


VMMDECL(RTSEL) CPUMGetHyperDS(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->ds;
}


VMMDECL(RTSEL) CPUMGetHyperES(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->es;
}


VMMDECL(RTSEL) CPUMGetHyperFS(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->fs;
}


VMMDECL(RTSEL) CPUMGetHyperGS(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->gs;
}


VMMDECL(RTSEL) CPUMGetHyperSS(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->ss;
}


VMMDECL(uint32_t) CPUMGetHyperEAX(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->eax;
}


VMMDECL(uint32_t) CPUMGetHyperEBX(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->ebx;
}


VMMDECL(uint32_t) CPUMGetHyperECX(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->ecx;
}


VMMDECL(uint32_t) CPUMGetHyperEDX(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->edx;
}


VMMDECL(uint32_t) CPUMGetHyperESI(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->esi;
}


VMMDECL(uint32_t) CPUMGetHyperEDI(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->edi;
}


VMMDECL(uint32_t) CPUMGetHyperEBP(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->ebp;
}


VMMDECL(uint32_t) CPUMGetHyperESP(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->esp;
}


VMMDECL(uint32_t) CPUMGetHyperEFlags(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->eflags.u32;
}


VMMDECL(uint32_t) CPUMGetHyperEIP(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->eip;
}


VMMDECL(uint64_t) CPUMGetHyperRIP(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.CTX_SUFF(pHyperCore)->rip;
}


VMMDECL(uint32_t) CPUMGetHyperIDTR(PVMCPU pVCpu, uint16_t *pcbLimit)
{
    if (pcbLimit)
        *pcbLimit = pVCpu->cpum.s.Hyper.idtr.cbIdt;
    return pVCpu->cpum.s.Hyper.idtr.pIdt;
}


VMMDECL(uint32_t) CPUMGetHyperGDTR(PVMCPU pVCpu, uint16_t *pcbLimit)
{
    if (pcbLimit)
        *pcbLimit = pVCpu->cpum.s.Hyper.gdtr.cbGdt;
    return pVCpu->cpum.s.Hyper.gdtr.pGdt;
}


VMMDECL(RTSEL) CPUMGetHyperLDTR(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Hyper.ldtr;
}


VMMDECL(RTGCUINTREG) CPUMGetHyperDR0(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Hyper.dr[0];
}


VMMDECL(RTGCUINTREG) CPUMGetHyperDR1(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Hyper.dr[1];
}


VMMDECL(RTGCUINTREG) CPUMGetHyperDR2(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Hyper.dr[2];
}


VMMDECL(RTGCUINTREG) CPUMGetHyperDR3(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Hyper.dr[3];
}


VMMDECL(RTGCUINTREG) CPUMGetHyperDR6(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Hyper.dr[6];
}


VMMDECL(RTGCUINTREG) CPUMGetHyperDR7(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Hyper.dr[7];
}


/**
 * Gets the pointer to the internal CPUMCTXCORE structure.
 * This is only for reading in order to save a few calls.
 *
 * @param   pVCpu       Handle to the virtual cpu.
 */
VMMDECL(PCCPUMCTXCORE) CPUMGetGuestCtxCore(PVMCPU pVCpu)
{
    return CPUMCTX2CORE(&pVCpu->cpum.s.Guest);
}


/**
 * Sets the guest context core registers.
 *
 * @param   pVCpu       Handle to the virtual cpu.
 * @param   pCtxCore    The new context core values.
 */
VMMDECL(void) CPUMSetGuestCtxCore(PVMCPU pVCpu, PCCPUMCTXCORE pCtxCore)
{
    /** @todo #1410 requires selectors to be checked. (huh? 1410?) */

    PCPUMCTXCORE pCtxCoreDst = CPUMCTX2CORE(&pVCpu->cpum.s.Guest);
    *pCtxCoreDst = *pCtxCore;

    /* Mask away invalid parts of the cpu context. */
    if (!CPUMIsGuestInLongMode(pVCpu))
    {
        uint64_t u64Mask = UINT64_C(0xffffffff);

        pCtxCoreDst->rip        &= u64Mask;
        pCtxCoreDst->rax        &= u64Mask;
        pCtxCoreDst->rbx        &= u64Mask;
        pCtxCoreDst->rcx        &= u64Mask;
        pCtxCoreDst->rdx        &= u64Mask;
        pCtxCoreDst->rsi        &= u64Mask;
        pCtxCoreDst->rdi        &= u64Mask;
        pCtxCoreDst->rbp        &= u64Mask;
        pCtxCoreDst->rsp        &= u64Mask;
        pCtxCoreDst->rflags.u   &= u64Mask;

        pCtxCoreDst->r8         = 0;
        pCtxCoreDst->r9         = 0;
        pCtxCoreDst->r10        = 0;
        pCtxCoreDst->r11        = 0;
        pCtxCoreDst->r12        = 0;
        pCtxCoreDst->r13        = 0;
        pCtxCoreDst->r14        = 0;
        pCtxCoreDst->r15        = 0;
    }
}


/**
 * Queries the pointer to the internal CPUMCTX structure
 *
 * @returns The CPUMCTX pointer.
 * @param   pVCpu       Handle to the virtual cpu.
 */
VMMDECL(PCPUMCTX) CPUMQueryGuestCtxPtr(PVMCPU pVCpu)
{
    return &pVCpu->cpum.s.Guest;
}

VMMDECL(int) CPUMSetGuestGDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit)
{
    pVCpu->cpum.s.Guest.gdtr.cbGdt = limit;
    pVCpu->cpum.s.Guest.gdtr.pGdt  = addr;
    pVCpu->cpum.s.fChanged |= CPUM_CHANGED_GDTR;
    return VINF_SUCCESS;
}

VMMDECL(int) CPUMSetGuestIDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit)
{
    pVCpu->cpum.s.Guest.idtr.cbIdt = limit;
    pVCpu->cpum.s.Guest.idtr.pIdt  = addr;
    pVCpu->cpum.s.fChanged |= CPUM_CHANGED_IDTR;
    return VINF_SUCCESS;
}

VMMDECL(int) CPUMSetGuestTR(PVMCPU pVCpu, uint16_t tr)
{
    AssertMsgFailed(("Need to load the hidden bits too!\n"));

    pVCpu->cpum.s.Guest.tr  = tr;
    pVCpu->cpum.s.fChanged |= CPUM_CHANGED_TR;
    return VINF_SUCCESS;
}

VMMDECL(int) CPUMSetGuestLDTR(PVMCPU pVCpu, uint16_t ldtr)
{
    pVCpu->cpum.s.Guest.ldtr = ldtr;
    pVCpu->cpum.s.fChanged  |= CPUM_CHANGED_LDTR;
    return VINF_SUCCESS;
}


/**
 * Set the guest CR0.
 *
 * When called in GC, the hyper CR0 may be updated if that is
 * required. The caller only has to take special action if AM,
 * WP, PG or PE changes.
 *
 * @returns VINF_SUCCESS (consider it void).
 * @param   pVCpu   Handle to the virtual cpu.
 * @param   cr0     The new CR0 value.
 */
VMMDECL(int) CPUMSetGuestCR0(PVMCPU pVCpu, uint64_t cr0)
{
#ifdef IN_RC
    /*
     * Check if we need to change hypervisor CR0 because
     * of math stuff.
     */
    if (    (cr0                 & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP))
        !=  (pVCpu->cpum.s.Guest.cr0 & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP)))
    {
        if (!(pVCpu->cpum.s.fUseFlags & CPUM_USED_FPU))
        {
            /*
             * We haven't saved the host FPU state yet, so TS and MT are both set
             * and EM should be reflecting the guest EM (it always does this).
             */
            if ((cr0 & X86_CR0_EM) != (pVCpu->cpum.s.Guest.cr0 & X86_CR0_EM))
            {
                uint32_t HyperCR0 = ASMGetCR0();
                AssertMsg((HyperCR0 & (X86_CR0_TS | X86_CR0_MP)) == (X86_CR0_TS | X86_CR0_MP), ("%#x\n", HyperCR0));
                AssertMsg((HyperCR0 & X86_CR0_EM) == (pVCpu->cpum.s.Guest.cr0 & X86_CR0_EM), ("%#x\n", HyperCR0));
                HyperCR0 &= ~X86_CR0_EM;
                HyperCR0 |= cr0 & X86_CR0_EM;
                Log(("CPUM New HyperCR0=%#x\n", HyperCR0));
                ASMSetCR0(HyperCR0);
            }
# ifdef VBOX_STRICT
            else
            {
                uint32_t HyperCR0 = ASMGetCR0();
                AssertMsg((HyperCR0 & (X86_CR0_TS | X86_CR0_MP)) == (X86_CR0_TS | X86_CR0_MP), ("%#x\n", HyperCR0));
                AssertMsg((HyperCR0 & X86_CR0_EM) == (pVCpu->cpum.s.Guest.cr0 & X86_CR0_EM), ("%#x\n", HyperCR0));
            }
# endif
        }
        else
        {
            /*
             * Already saved the state, so we're just mirroring
             * the guest flags.
             */
            uint32_t HyperCR0 = ASMGetCR0();
            AssertMsg(     (HyperCR0             & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP))
                      ==   (pVCpu->cpum.s.Guest.cr0  & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP)),
                      ("%#x %#x\n", HyperCR0, pVCpu->cpum.s.Guest.cr0));
            HyperCR0 &= ~(X86_CR0_TS | X86_CR0_EM | X86_CR0_MP);
            HyperCR0 |= cr0 & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP);
            Log(("CPUM New HyperCR0=%#x\n", HyperCR0));
            ASMSetCR0(HyperCR0);
        }
    }
#endif /* IN_RC */

    /*
     * Check for changes causing TLB flushes (for REM).
     * The caller is responsible for calling PGM when appropriate.
     */
    if (    (cr0                 & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE))
        !=  (pVCpu->cpum.s.Guest.cr0 & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)))
        pVCpu->cpum.s.fChanged |= CPUM_CHANGED_GLOBAL_TLB_FLUSH;
    pVCpu->cpum.s.fChanged |= CPUM_CHANGED_CR0;

    pVCpu->cpum.s.Guest.cr0 = cr0 | X86_CR0_ET;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestCR2(PVMCPU pVCpu, uint64_t cr2)
{
    pVCpu->cpum.s.Guest.cr2 = cr2;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestCR3(PVMCPU pVCpu, uint64_t cr3)
{
    pVCpu->cpum.s.Guest.cr3 = cr3;
    pVCpu->cpum.s.fChanged |= CPUM_CHANGED_CR3;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestCR4(PVMCPU pVCpu, uint64_t cr4)
{
    if (    (cr4                 & (X86_CR4_PGE | X86_CR4_PAE | X86_CR4_PSE))
        !=  (pVCpu->cpum.s.Guest.cr4 & (X86_CR4_PGE | X86_CR4_PAE | X86_CR4_PSE)))
        pVCpu->cpum.s.fChanged |= CPUM_CHANGED_GLOBAL_TLB_FLUSH;
    pVCpu->cpum.s.fChanged |= CPUM_CHANGED_CR4;
    if (!CPUMSupportsFXSR(pVCpu->CTX_SUFF(pVM)))
        cr4 &= ~X86_CR4_OSFSXR;
    pVCpu->cpum.s.Guest.cr4 = cr4;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestEFlags(PVMCPU pVCpu, uint32_t eflags)
{
    pVCpu->cpum.s.Guest.eflags.u32 = eflags;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestEIP(PVMCPU pVCpu, uint32_t eip)
{
    pVCpu->cpum.s.Guest.eip = eip;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestEAX(PVMCPU pVCpu, uint32_t eax)
{
    pVCpu->cpum.s.Guest.eax = eax;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestEBX(PVMCPU pVCpu, uint32_t ebx)
{
    pVCpu->cpum.s.Guest.ebx = ebx;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestECX(PVMCPU pVCpu, uint32_t ecx)
{
    pVCpu->cpum.s.Guest.ecx = ecx;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestEDX(PVMCPU pVCpu, uint32_t edx)
{
    pVCpu->cpum.s.Guest.edx = edx;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestESP(PVMCPU pVCpu, uint32_t esp)
{
    pVCpu->cpum.s.Guest.esp = esp;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestEBP(PVMCPU pVCpu, uint32_t ebp)
{
    pVCpu->cpum.s.Guest.ebp = ebp;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestESI(PVMCPU pVCpu, uint32_t esi)
{
    pVCpu->cpum.s.Guest.esi = esi;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestEDI(PVMCPU pVCpu, uint32_t edi)
{
    pVCpu->cpum.s.Guest.edi = edi;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestSS(PVMCPU pVCpu, uint16_t ss)
{
    pVCpu->cpum.s.Guest.ss = ss;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestCS(PVMCPU pVCpu, uint16_t cs)
{
    pVCpu->cpum.s.Guest.cs = cs;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestDS(PVMCPU pVCpu, uint16_t ds)
{
    pVCpu->cpum.s.Guest.ds = ds;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestES(PVMCPU pVCpu, uint16_t es)
{
    pVCpu->cpum.s.Guest.es = es;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestFS(PVMCPU pVCpu, uint16_t fs)
{
    pVCpu->cpum.s.Guest.fs = fs;
    return VINF_SUCCESS;
}


VMMDECL(int) CPUMSetGuestGS(PVMCPU pVCpu, uint16_t gs)
{
    pVCpu->cpum.s.Guest.gs = gs;
    return VINF_SUCCESS;
}


VMMDECL(void) CPUMSetGuestEFER(PVMCPU pVCpu, uint64_t val)
{
    pVCpu->cpum.s.Guest.msrEFER = val;
}


VMMDECL(uint64_t)  CPUMGetGuestMsr(PVMCPU pVCpu, unsigned idMsr)
{
    uint64_t u64 = 0;

    switch (idMsr)
    {
        case MSR_IA32_TSC:
            u64 = TMCpuTickGet(pVCpu);
            break;

        case MSR_IA32_CR_PAT:
            u64 = pVCpu->cpum.s.Guest.msrPAT;
            break;

        case MSR_IA32_SYSENTER_CS:
            u64 = pVCpu->cpum.s.Guest.SysEnter.cs;
            break;

        case MSR_IA32_SYSENTER_EIP:
            u64 = pVCpu->cpum.s.Guest.SysEnter.eip;
            break;

        case MSR_IA32_SYSENTER_ESP:
            u64 = pVCpu->cpum.s.Guest.SysEnter.esp;
            break;

        case MSR_K6_EFER:
            u64 = pVCpu->cpum.s.Guest.msrEFER;
            break;

        case MSR_K8_SF_MASK:
            u64 = pVCpu->cpum.s.Guest.msrSFMASK;
            break;

        case MSR_K6_STAR:
            u64 = pVCpu->cpum.s.Guest.msrSTAR;
            break;

        case MSR_K8_LSTAR:
            u64 = pVCpu->cpum.s.Guest.msrLSTAR;
            break;

        case MSR_K8_CSTAR:
            u64 = pVCpu->cpum.s.Guest.msrCSTAR;
            break;

        case MSR_K8_KERNEL_GS_BASE:
            u64 = pVCpu->cpum.s.Guest.msrKERNELGSBASE;
            break;

        case MSR_K8_TSC_AUX:
            u64 = pVCpu->cpum.s.GuestMsr.msr.tscAux;
            break;

        /* fs & gs base skipped on purpose as the current context might not be up-to-date. */
        default:
            AssertFailed();
            break;
    }
    return u64;
}

VMMDECL(void) CPUMSetGuestMsr(PVMCPU pVCpu, unsigned idMsr, uint64_t valMsr)
{
    /* On purpose only a limited number of MSRs; use the emulation function to update the others. */
    switch (idMsr)
    {
        case MSR_K8_TSC_AUX:
            pVCpu->cpum.s.GuestMsr.msr.tscAux = valMsr;
            break;

        default:
            AssertFailed();
            break;
    }
}

VMMDECL(RTGCPTR) CPUMGetGuestIDTR(PVMCPU pVCpu, uint16_t *pcbLimit)
{
    if (pcbLimit)
        *pcbLimit = pVCpu->cpum.s.Guest.idtr.cbIdt;
    return pVCpu->cpum.s.Guest.idtr.pIdt;
}


VMMDECL(RTSEL) CPUMGetGuestTR(PVMCPU pVCpu, PCPUMSELREGHID pHidden)
{
    if (pHidden)
        *pHidden = pVCpu->cpum.s.Guest.trHid;
    return pVCpu->cpum.s.Guest.tr;
}


VMMDECL(RTSEL) CPUMGetGuestCS(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.cs;
}


VMMDECL(RTSEL) CPUMGetGuestDS(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.ds;
}


VMMDECL(RTSEL) CPUMGetGuestES(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.es;
}


VMMDECL(RTSEL) CPUMGetGuestFS(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.fs;
}


VMMDECL(RTSEL) CPUMGetGuestGS(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.gs;
}


VMMDECL(RTSEL) CPUMGetGuestSS(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.ss;
}


VMMDECL(RTSEL) CPUMGetGuestLDTR(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.ldtr;
}


VMMDECL(uint64_t) CPUMGetGuestCR0(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.cr0;
}


VMMDECL(uint64_t) CPUMGetGuestCR2(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.cr2;
}


VMMDECL(uint64_t) CPUMGetGuestCR3(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.cr3;
}


VMMDECL(uint64_t) CPUMGetGuestCR4(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.cr4;
}


VMMDECL(void) CPUMGetGuestGDTR(PVMCPU pVCpu, PVBOXGDTR pGDTR)
{
    *pGDTR = pVCpu->cpum.s.Guest.gdtr;
}


VMMDECL(uint32_t) CPUMGetGuestEIP(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.eip;
}


VMMDECL(uint64_t) CPUMGetGuestRIP(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.rip;
}


VMMDECL(uint32_t) CPUMGetGuestEAX(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.eax;
}


VMMDECL(uint32_t) CPUMGetGuestEBX(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.ebx;
}


VMMDECL(uint32_t) CPUMGetGuestECX(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.ecx;
}


VMMDECL(uint32_t) CPUMGetGuestEDX(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.edx;
}


VMMDECL(uint32_t) CPUMGetGuestESI(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.esi;
}


VMMDECL(uint32_t) CPUMGetGuestEDI(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.edi;
}


VMMDECL(uint32_t) CPUMGetGuestESP(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.esp;
}


VMMDECL(uint32_t) CPUMGetGuestEBP(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.ebp;
}


VMMDECL(uint32_t) CPUMGetGuestEFlags(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.eflags.u32;
}


///@todo: crx should be an array
VMMDECL(int) CPUMGetGuestCRx(PVMCPU pVCpu, unsigned iReg, uint64_t *pValue)
{
    switch (iReg)
    {
        case USE_REG_CR0:
            *pValue = pVCpu->cpum.s.Guest.cr0;
            break;
        case USE_REG_CR2:
            *pValue = pVCpu->cpum.s.Guest.cr2;
            break;
        case USE_REG_CR3:
            *pValue = pVCpu->cpum.s.Guest.cr3;
            break;
        case USE_REG_CR4:
            *pValue = pVCpu->cpum.s.Guest.cr4;
            break;
        default:
            return VERR_INVALID_PARAMETER;
    }
    return VINF_SUCCESS;
}


VMMDECL(uint64_t) CPUMGetGuestDR0(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.dr[0];
}


VMMDECL(uint64_t) CPUMGetGuestDR1(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.dr[1];
}


VMMDECL(uint64_t) CPUMGetGuestDR2(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.dr[2];
}


VMMDECL(uint64_t) CPUMGetGuestDR3(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.dr[3];
}


VMMDECL(uint64_t) CPUMGetGuestDR6(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.dr[6];
}


VMMDECL(uint64_t) CPUMGetGuestDR7(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.dr[7];
}


VMMDECL(int) CPUMGetGuestDRx(PVMCPU pVCpu, uint32_t iReg, uint64_t *pValue)
{
    AssertReturn(iReg <= USE_REG_DR7, VERR_INVALID_PARAMETER);
    /* DR4 is an alias for DR6, and DR5 is an alias for DR7. */
    if (iReg == 4 || iReg == 5)
        iReg += 2;
    *pValue = pVCpu->cpum.s.Guest.dr[iReg];
    return VINF_SUCCESS;
}


VMMDECL(uint64_t) CPUMGetGuestEFER(PVMCPU pVCpu)
{
    return pVCpu->cpum.s.Guest.msrEFER;
}


/**
 * Gets a CpuId leaf.
 *
 * @param   pVM     The VM handle.
 * @param   iLeaf   The CPUID leaf to get.
 * @param   pEax    Where to store the EAX value.
 * @param   pEbx    Where to store the EBX value.
 * @param   pEcx    Where to store the ECX value.
 * @param   pEdx    Where to store the EDX value.
 */
VMMDECL(void) CPUMGetGuestCpuId(PVM pVM, uint32_t iLeaf, uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx)
{
    PCCPUMCPUID pCpuId;
    if (iLeaf < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd))
        pCpuId = &pVM->cpum.s.aGuestCpuIdStd[iLeaf];
    else if (iLeaf - UINT32_C(0x80000000) < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt))
        pCpuId = &pVM->cpum.s.aGuestCpuIdExt[iLeaf - UINT32_C(0x80000000)];
    else if (iLeaf - UINT32_C(0xc0000000) < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur))
        pCpuId = &pVM->cpum.s.aGuestCpuIdCentaur[iLeaf - UINT32_C(0xc0000000)];
    else
        pCpuId = &pVM->cpum.s.GuestCpuIdDef;

    *pEax = pCpuId->eax;
    *pEbx = pCpuId->ebx;
    *pEcx = pCpuId->ecx;
    *pEdx = pCpuId->edx;
    Log2(("CPUMGetGuestCpuId: iLeaf=%#010x %RX32 %RX32 %RX32 %RX32\n", iLeaf, *pEax, *pEbx, *pEcx, *pEdx));
}

/**
 * Gets a number of standard CPUID leafs.
 *
 * @returns Number of leafs.
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
VMMDECL(uint32_t) CPUMGetGuestCpuIdStdMax(PVM pVM)
{
    return RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd);
}


/**
 * Gets a number of extended CPUID leafs.
 *
 * @returns Number of leafs.
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
VMMDECL(uint32_t) CPUMGetGuestCpuIdExtMax(PVM pVM)
{
    return RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt);
}


/**
 * Gets a number of centaur CPUID leafs.
 *
 * @returns Number of leafs.
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
VMMDECL(uint32_t) CPUMGetGuestCpuIdCentaurMax(PVM pVM)
{
    return RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur);
}


/**
 * Sets a CPUID feature bit.
 *
 * @param   pVM             The VM Handle.
 * @param   enmFeature      The feature to set.
 */
VMMDECL(void) CPUMSetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature)
{
    switch (enmFeature)
    {
        /*
         * Set the APIC bit in both feature masks.
         */
        case CPUMCPUIDFEATURE_APIC:
            if (pVM->cpum.s.aGuestCpuIdStd[0].eax >= 1)
                pVM->cpum.s.aGuestCpuIdStd[1].edx |= X86_CPUID_FEATURE_EDX_APIC;
            if (    pVM->cpum.s.aGuestCpuIdExt[0].eax >= 0x80000001
                &&  pVM->cpum.s.enmCPUVendor == CPUMCPUVENDOR_AMD)
                pVM->cpum.s.aGuestCpuIdExt[1].edx |= X86_CPUID_AMD_FEATURE_EDX_APIC;
            LogRel(("CPUMSetGuestCpuIdFeature: Enabled APIC\n"));
            break;

       /*
        * Set the x2APIC bit in the standard feature mask.
        */
        case CPUMCPUIDFEATURE_X2APIC:
            if (pVM->cpum.s.aGuestCpuIdStd[0].eax >= 1)
                pVM->cpum.s.aGuestCpuIdStd[1].ecx |= X86_CPUID_FEATURE_ECX_X2APIC;
            LogRel(("CPUMSetGuestCpuIdFeature: Enabled x2APIC\n"));
            break;

        /*
         * Set the sysenter/sysexit bit in the standard feature mask.
         * Assumes the caller knows what it's doing! (host must support these)
         */
        case CPUMCPUIDFEATURE_SEP:
        {
            if (!(ASMCpuId_EDX(1) & X86_CPUID_FEATURE_EDX_SEP))
            {
                AssertMsgFailed(("ERROR: Can't turn on SEP when the host doesn't support it!!\n"));
                return;
            }

            if (pVM->cpum.s.aGuestCpuIdStd[0].eax >= 1)
                pVM->cpum.s.aGuestCpuIdStd[1].edx |= X86_CPUID_FEATURE_EDX_SEP;
            LogRel(("CPUMSetGuestCpuIdFeature: Enabled sysenter/exit\n"));
            break;
        }

        /*
         * Set the syscall/sysret bit in the extended feature mask.
         * Assumes the caller knows what it's doing! (host must support these)
         */
        case CPUMCPUIDFEATURE_SYSCALL:
        {
            if (    pVM->cpum.s.aGuestCpuIdExt[0].eax < 0x80000001
                ||  !(ASMCpuId_EDX(0x80000001) & X86_CPUID_AMD_FEATURE_EDX_SEP))
            {
#if HC_ARCH_BITS == 32
                /* X86_CPUID_AMD_FEATURE_EDX_SEP not set it seems in 32 bits mode.
                 * Even when the cpu is capable of doing so in 64 bits mode.
                 */
                if (    pVM->cpum.s.aGuestCpuIdExt[0].eax < 0x80000001
                    ||  !(ASMCpuId_EDX(0x80000001) & X86_CPUID_AMD_FEATURE_EDX_LONG_MODE)
                    ||  !(ASMCpuId_EDX(1) & X86_CPUID_FEATURE_EDX_SEP))
#endif
                {
                    LogRel(("WARNING: Can't turn on SYSCALL/SYSRET when the host doesn't support it!!\n"));
                    return;
                }
            }
            /* Valid for both Intel and AMD CPUs, although only in 64 bits mode for Intel. */
            pVM->cpum.s.aGuestCpuIdExt[1].edx |= X86_CPUID_AMD_FEATURE_EDX_SEP;
            LogRel(("CPUMSetGuestCpuIdFeature: Enabled syscall/ret\n"));
            break;
        }

        /*
         * Set the PAE bit in both feature masks.
         * Assumes the caller knows what it's doing! (host must support these)
         */
        case CPUMCPUIDFEATURE_PAE:
        {
            if (!(ASMCpuId_EDX(1) & X86_CPUID_FEATURE_EDX_PAE))
            {
                LogRel(("WARNING: Can't turn on PAE when the host doesn't support it!!\n"));
                return;
            }

            if (pVM->cpum.s.aGuestCpuIdStd[0].eax >= 1)
                pVM->cpum.s.aGuestCpuIdStd[1].edx |= X86_CPUID_FEATURE_EDX_PAE;
            if (    pVM->cpum.s.aGuestCpuIdExt[0].eax >= 0x80000001
                &&  pVM->cpum.s.enmCPUVendor == CPUMCPUVENDOR_AMD)
                pVM->cpum.s.aGuestCpuIdExt[1].edx |= X86_CPUID_AMD_FEATURE_EDX_PAE;
            LogRel(("CPUMSetGuestCpuIdFeature: Enabled PAE\n"));
            break;
        }

        /*
         * Set the LONG MODE bit in the extended feature mask.
         * Assumes the caller knows what it's doing! (host must support these)
         */
        case CPUMCPUIDFEATURE_LONG_MODE:
        {
            if (    pVM->cpum.s.aGuestCpuIdExt[0].eax < 0x80000001
                ||  !(ASMCpuId_EDX(0x80000001) & X86_CPUID_AMD_FEATURE_EDX_LONG_MODE))
            {
                LogRel(("WARNING: Can't turn on LONG MODE when the host doesn't support it!!\n"));
                return;
            }

            /* Valid for both Intel and AMD. */
            pVM->cpum.s.aGuestCpuIdExt[1].edx |= X86_CPUID_AMD_FEATURE_EDX_LONG_MODE;
            LogRel(("CPUMSetGuestCpuIdFeature: Enabled LONG MODE\n"));
            break;
        }

        /*
         * Set the NXE bit in the extended feature mask.
         * Assumes the caller knows what it's doing! (host must support these)
         */
        case CPUMCPUIDFEATURE_NXE:
        {
            if (    pVM->cpum.s.aGuestCpuIdExt[0].eax < 0x80000001
                ||  !(ASMCpuId_EDX(0x80000001) & X86_CPUID_AMD_FEATURE_EDX_NX))
            {
                LogRel(("WARNING: Can't turn on NXE when the host doesn't support it!!\n"));
                return;
            }

            /* Valid for both Intel and AMD. */
            pVM->cpum.s.aGuestCpuIdExt[1].edx |= X86_CPUID_AMD_FEATURE_EDX_NX;
            LogRel(("CPUMSetGuestCpuIdFeature: Enabled NXE\n"));
            break;
        }

        case CPUMCPUIDFEATURE_LAHF:
        {
            if (    pVM->cpum.s.aGuestCpuIdExt[0].eax < 0x80000001
                ||  !(ASMCpuId_ECX(0x80000001) & X86_CPUID_AMD_FEATURE_ECX_LAHF_SAHF))
            {
                LogRel(("WARNING: Can't turn on LAHF/SAHF when the host doesn't support it!!\n"));
                return;
            }

            pVM->cpum.s.aGuestCpuIdExt[1].ecx |= X86_CPUID_AMD_FEATURE_ECX_LAHF_SAHF;
            LogRel(("CPUMSetGuestCpuIdFeature: Enabled LAHF/SAHF\n"));
            break;
        }

        case CPUMCPUIDFEATURE_PAT:
        {
            if (pVM->cpum.s.aGuestCpuIdStd[0].eax >= 1)
                pVM->cpum.s.aGuestCpuIdStd[1].edx |= X86_CPUID_FEATURE_EDX_PAT;
            if (    pVM->cpum.s.aGuestCpuIdExt[0].eax >= 0x80000001
                &&  pVM->cpum.s.enmCPUVendor == CPUMCPUVENDOR_AMD)
                pVM->cpum.s.aGuestCpuIdExt[1].edx |= X86_CPUID_AMD_FEATURE_EDX_PAT;
            LogRel(("CPUMClearGuestCpuIdFeature: Enabled PAT\n"));
            break;
        }

        case CPUMCPUIDFEATURE_RDTSCP:
        {
            if (    pVM->cpum.s.aGuestCpuIdExt[0].eax < 0x80000001
                ||  !(ASMCpuId_EDX(0x80000001) & X86_CPUID_AMD_FEATURE_EDX_RDTSCP))
            {
                LogRel(("WARNING: Can't turn on RDTSCP when the host doesn't support it!!\n"));
                return;
            }

            /* Valid for AMD only (for now). */
            pVM->cpum.s.aGuestCpuIdExt[1].edx |= X86_CPUID_AMD_FEATURE_EDX_RDTSCP;
            LogRel(("CPUMSetGuestCpuIdFeature: Enabled RDTSCP.\n"));
            break;
        }

        default:
            AssertMsgFailed(("enmFeature=%d\n", enmFeature));
            break;
    }
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        pVCpu->cpum.s.fChanged |= CPUM_CHANGED_CPUID;
    }
}


/**
 * Queries a CPUID feature bit.
 *
 * @returns boolean for feature presence
 * @param   pVM             The VM Handle.
 * @param   enmFeature      The feature to query.
 */
VMMDECL(bool) CPUMGetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature)
{
    switch (enmFeature)
    {
        case CPUMCPUIDFEATURE_PAE:
        {
            if (pVM->cpum.s.aGuestCpuIdStd[0].eax >= 1)
                return !!(pVM->cpum.s.aGuestCpuIdStd[1].edx & X86_CPUID_FEATURE_EDX_PAE);
            break;
        }

        case CPUMCPUIDFEATURE_RDTSCP:
        {
            if (pVM->cpum.s.aGuestCpuIdExt[0].eax >= 0x80000001)
                return !!(pVM->cpum.s.aGuestCpuIdExt[1].edx & X86_CPUID_AMD_FEATURE_EDX_RDTSCP);
            break;
        }

        case CPUMCPUIDFEATURE_LONG_MODE:
        {
            if (pVM->cpum.s.aGuestCpuIdExt[0].eax >= 0x80000001)
                return !!(pVM->cpum.s.aGuestCpuIdExt[1].edx & X86_CPUID_AMD_FEATURE_EDX_LONG_MODE);
            break;
        }

        default:
            AssertMsgFailed(("enmFeature=%d\n", enmFeature));
            break;
    }
    return false;
}


/**
 * Clears a CPUID feature bit.
 *
 * @param   pVM             The VM Handle.
 * @param   enmFeature      The feature to clear.
 */
VMMDECL(void) CPUMClearGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature)
{
    switch (enmFeature)
    {
        /*
         * Set the APIC bit in both feature masks.
         */
        case CPUMCPUIDFEATURE_APIC:
            if (pVM->cpum.s.aGuestCpuIdStd[0].eax >= 1)
                pVM->cpum.s.aGuestCpuIdStd[1].edx &= ~X86_CPUID_FEATURE_EDX_APIC;
            if (    pVM->cpum.s.aGuestCpuIdExt[0].eax >= 0x80000001
                &&  pVM->cpum.s.enmCPUVendor == CPUMCPUVENDOR_AMD)
                pVM->cpum.s.aGuestCpuIdExt[1].edx &= ~X86_CPUID_AMD_FEATURE_EDX_APIC;
            Log(("CPUMSetGuestCpuIdFeature: Disabled APIC\n"));
            break;

        /*
         * Clear the x2APIC bit in the standard feature mask.
         */
        case CPUMCPUIDFEATURE_X2APIC:
            if (pVM->cpum.s.aGuestCpuIdStd[0].eax >= 1)
                pVM->cpum.s.aGuestCpuIdStd[1].ecx &= ~X86_CPUID_FEATURE_ECX_X2APIC;
            LogRel(("CPUMSetGuestCpuIdFeature: Disabled x2APIC\n"));
            break;

        case CPUMCPUIDFEATURE_PAE:
        {
            if (pVM->cpum.s.aGuestCpuIdStd[0].eax >= 1)
                pVM->cpum.s.aGuestCpuIdStd[1].edx &= ~X86_CPUID_FEATURE_EDX_PAE;
            if (    pVM->cpum.s.aGuestCpuIdExt[0].eax >= 0x80000001
                &&  pVM->cpum.s.enmCPUVendor == CPUMCPUVENDOR_AMD)
                pVM->cpum.s.aGuestCpuIdExt[1].edx &= ~X86_CPUID_AMD_FEATURE_EDX_PAE;
            LogRel(("CPUMClearGuestCpuIdFeature: Disabled PAE!\n"));
            break;
        }

        case CPUMCPUIDFEATURE_PAT:
        {
            if (pVM->cpum.s.aGuestCpuIdStd[0].eax >= 1)
                pVM->cpum.s.aGuestCpuIdStd[1].edx &= ~X86_CPUID_FEATURE_EDX_PAT;
            if (    pVM->cpum.s.aGuestCpuIdExt[0].eax >= 0x80000001
                &&  pVM->cpum.s.enmCPUVendor == CPUMCPUVENDOR_AMD)
                pVM->cpum.s.aGuestCpuIdExt[1].edx &= ~X86_CPUID_AMD_FEATURE_EDX_PAT;
            LogRel(("CPUMClearGuestCpuIdFeature: Disabled PAT!\n"));
            break;
        }

        case CPUMCPUIDFEATURE_LONG_MODE:
        {
            if (pVM->cpum.s.aGuestCpuIdExt[0].eax >= 0x80000001)
                pVM->cpum.s.aGuestCpuIdExt[1].edx &= ~X86_CPUID_AMD_FEATURE_EDX_LONG_MODE;
            break;
        }

        case CPUMCPUIDFEATURE_LAHF:
        {
            if (pVM->cpum.s.aGuestCpuIdExt[0].eax >= 0x80000001)
                pVM->cpum.s.aGuestCpuIdExt[1].ecx &= ~X86_CPUID_AMD_FEATURE_ECX_LAHF_SAHF;
            break;
        }

        default:
            AssertMsgFailed(("enmFeature=%d\n", enmFeature));
            break;
    }
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        pVCpu->cpum.s.fChanged |= CPUM_CHANGED_CPUID;
    }
}


/**
 * Gets the CPU vendor
 *
 * @returns CPU vendor
 * @param   pVM     The VM handle.
 */
VMMDECL(CPUMCPUVENDOR) CPUMGetCPUVendor(PVM pVM)
{
    return pVM->cpum.s.enmCPUVendor;
}


VMMDECL(int) CPUMSetGuestDR0(PVMCPU pVCpu, uint64_t uDr0)
{
    pVCpu->cpum.s.Guest.dr[0] = uDr0;
    return CPUMRecalcHyperDRx(pVCpu);
}


VMMDECL(int) CPUMSetGuestDR1(PVMCPU pVCpu, uint64_t uDr1)
{
    pVCpu->cpum.s.Guest.dr[1] = uDr1;
    return CPUMRecalcHyperDRx(pVCpu);
}


VMMDECL(int) CPUMSetGuestDR2(PVMCPU pVCpu, uint64_t uDr2)
{
    pVCpu->cpum.s.Guest.dr[2] = uDr2;
    return CPUMRecalcHyperDRx(pVCpu);
}


VMMDECL(int) CPUMSetGuestDR3(PVMCPU pVCpu, uint64_t uDr3)
{
    pVCpu->cpum.s.Guest.dr[3] = uDr3;
    return CPUMRecalcHyperDRx(pVCpu);
}


VMMDECL(int) CPUMSetGuestDR6(PVMCPU pVCpu, uint64_t uDr6)
{
    pVCpu->cpum.s.Guest.dr[6] = uDr6;
    return CPUMRecalcHyperDRx(pVCpu);
}


VMMDECL(int) CPUMSetGuestDR7(PVMCPU pVCpu, uint64_t uDr7)
{
    pVCpu->cpum.s.Guest.dr[7] = uDr7;
    return CPUMRecalcHyperDRx(pVCpu);
}


VMMDECL(int) CPUMSetGuestDRx(PVMCPU pVCpu, uint32_t iReg, uint64_t Value)
{
    AssertReturn(iReg <= USE_REG_DR7, VERR_INVALID_PARAMETER);
    /* DR4 is an alias for DR6, and DR5 is an alias for DR7. */
    if (iReg == 4 || iReg == 5)
        iReg += 2;
    pVCpu->cpum.s.Guest.dr[iReg] = Value;
    return CPUMRecalcHyperDRx(pVCpu);
}


/**
 * Recalculates the hypvervisor DRx register values based on
 * current guest registers and DBGF breakpoints.
 *
 * This is called whenever a guest DRx register is modified and when DBGF
 * sets a hardware breakpoint. In guest context this function will reload
 * any (hyper) DRx registers which comes out with a different value.
 *
 * @returns VINF_SUCCESS.
 * @param   pVCpu       The VMCPU handle.
 */
VMMDECL(int) CPUMRecalcHyperDRx(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    /*
     * Compare the DR7s first.
     *
     * We only care about the enabled flags. The GE and LE flags are always
     * set and we don't care if the guest doesn't set them. GD is virtualized
     * when we dispatch #DB, we never enable it.
     */
    const RTGCUINTREG uDbgfDr7 = DBGFBpGetDR7(pVM);
#ifdef CPUM_VIRTUALIZE_DRX
    const RTGCUINTREG uGstDr7  = CPUMGetGuestDR7(pVCpu);
#else
    const RTGCUINTREG uGstDr7  = 0;
#endif
    if ((uGstDr7 | uDbgfDr7) & X86_DR7_ENABLED_MASK)
    {
        /*
         * Ok, something is enabled. Recalc each of the breakpoints.
         * Straight forward code, not optimized/minimized in any way.
         */
        RTGCUINTREG uNewDr7 = X86_DR7_GE | X86_DR7_LE | X86_DR7_MB1_MASK;

        /* bp 0 */
        RTGCUINTREG uNewDr0;
        if (uDbgfDr7 & (X86_DR7_L0 | X86_DR7_G0))
        {
            uNewDr7 |= uDbgfDr7 & (X86_DR7_L0 | X86_DR7_G0 | X86_DR7_RW0_MASK | X86_DR7_LEN0_MASK);
            uNewDr0 = DBGFBpGetDR0(pVM);
        }
        else if (uGstDr7 & (X86_DR7_L0 | X86_DR7_G0))
        {
            uNewDr7 |= uGstDr7 & (X86_DR7_L0 | X86_DR7_G0 | X86_DR7_RW0_MASK | X86_DR7_LEN0_MASK);
            uNewDr0 = CPUMGetGuestDR0(pVCpu);
        }
        else
            uNewDr0 = pVCpu->cpum.s.Hyper.dr[0];

        /* bp 1 */
        RTGCUINTREG uNewDr1;
        if (uDbgfDr7 & (X86_DR7_L1 | X86_DR7_G1))
        {
            uNewDr7 |= uDbgfDr7 & (X86_DR7_L1 | X86_DR7_G1 | X86_DR7_RW1_MASK | X86_DR7_LEN1_MASK);
            uNewDr1 = DBGFBpGetDR1(pVM);
        }
        else if (uGstDr7 & (X86_DR7_L1 | X86_DR7_G1))
        {
            uNewDr7 |= uGstDr7 & (X86_DR7_L1 | X86_DR7_G1 | X86_DR7_RW1_MASK | X86_DR7_LEN1_MASK);
            uNewDr1 = CPUMGetGuestDR1(pVCpu);
        }
        else
            uNewDr1 = pVCpu->cpum.s.Hyper.dr[1];

        /* bp 2 */
        RTGCUINTREG uNewDr2;
        if (uDbgfDr7 & (X86_DR7_L2 | X86_DR7_G2))
        {
            uNewDr7 |= uDbgfDr7 & (X86_DR7_L2 | X86_DR7_G2 | X86_DR7_RW2_MASK | X86_DR7_LEN2_MASK);
            uNewDr2 = DBGFBpGetDR2(pVM);
        }
        else if (uGstDr7 & (X86_DR7_L2 | X86_DR7_G2))
        {
            uNewDr7 |= uGstDr7 & (X86_DR7_L2 | X86_DR7_G2 | X86_DR7_RW2_MASK | X86_DR7_LEN2_MASK);
            uNewDr2 = CPUMGetGuestDR2(pVCpu);
        }
        else
            uNewDr2 = pVCpu->cpum.s.Hyper.dr[2];

        /* bp 3 */
        RTGCUINTREG uNewDr3;
        if (uDbgfDr7 & (X86_DR7_L3 | X86_DR7_G3))
        {
            uNewDr7 |= uDbgfDr7 & (X86_DR7_L3 | X86_DR7_G3 | X86_DR7_RW3_MASK | X86_DR7_LEN3_MASK);
            uNewDr3 = DBGFBpGetDR3(pVM);
        }
        else if (uGstDr7 & (X86_DR7_L3 | X86_DR7_G3))
        {
            uNewDr7 |= uGstDr7 & (X86_DR7_L3 | X86_DR7_G3 | X86_DR7_RW3_MASK | X86_DR7_LEN3_MASK);
            uNewDr3 = CPUMGetGuestDR3(pVCpu);
        }
        else
            uNewDr3 = pVCpu->cpum.s.Hyper.dr[3];

        /*
         * Apply the updates.
         */
#ifdef IN_RC
        if (!(pVCpu->cpum.s.fUseFlags & CPUM_USE_DEBUG_REGS))
        {
            /** @todo save host DBx registers. */
        }
#endif
        pVCpu->cpum.s.fUseFlags |= CPUM_USE_DEBUG_REGS;
        if (uNewDr3 != pVCpu->cpum.s.Hyper.dr[3])
            CPUMSetHyperDR3(pVCpu, uNewDr3);
        if (uNewDr2 != pVCpu->cpum.s.Hyper.dr[2])
            CPUMSetHyperDR2(pVCpu, uNewDr2);
        if (uNewDr1 != pVCpu->cpum.s.Hyper.dr[1])
            CPUMSetHyperDR1(pVCpu, uNewDr1);
        if (uNewDr0 != pVCpu->cpum.s.Hyper.dr[0])
            CPUMSetHyperDR0(pVCpu, uNewDr0);
        if (uNewDr7 != pVCpu->cpum.s.Hyper.dr[7])
            CPUMSetHyperDR7(pVCpu, uNewDr7);
    }
    else
    {
#ifdef IN_RC
        if (pVCpu->cpum.s.fUseFlags & CPUM_USE_DEBUG_REGS)
        {
            /** @todo restore host DBx registers. */
        }
#endif
        pVCpu->cpum.s.fUseFlags &= ~CPUM_USE_DEBUG_REGS;
    }
    Log2(("CPUMRecalcHyperDRx: fUseFlags=%#x %RGr %RGr %RGr %RGr  %RGr %RGr\n",
          pVCpu->cpum.s.fUseFlags, pVCpu->cpum.s.Hyper.dr[0], pVCpu->cpum.s.Hyper.dr[1],
         pVCpu->cpum.s.Hyper.dr[2], pVCpu->cpum.s.Hyper.dr[3], pVCpu->cpum.s.Hyper.dr[6],
         pVCpu->cpum.s.Hyper.dr[7]));

    return VINF_SUCCESS;
}

#ifndef IN_RING0  /** @todo I don't think we need this in R0, so move it to CPUMAll.cpp? */

/**
 * Transforms the guest CPU state to raw-ring mode.
 *
 * This function will change the any of the cs and ss register with DPL=0 to DPL=1.
 *
 * @returns VBox status. (recompiler failure)
 * @param   pVCpu       The VMCPU handle.
 * @param   pCtxCore    The context core (for trap usage).
 * @see     @ref pg_raw
 */
VMMDECL(int) CPUMRawEnter(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    Assert(!pVM->cpum.s.fRawEntered);
    if (!pCtxCore)
        pCtxCore = CPUMCTX2CORE(&pVCpu->cpum.s.Guest);

    /*
     * Are we in Ring-0?
     */
    if (    pCtxCore->ss && (pCtxCore->ss & X86_SEL_RPL) == 0
        &&  !pCtxCore->eflags.Bits.u1VM)
    {
        /*
         * Enter execution mode.
         */
        PATMRawEnter(pVM, pCtxCore);

        /*
         * Set CPL to Ring-1.
         */
        pCtxCore->ss |= 1;
        if (pCtxCore->cs && (pCtxCore->cs & X86_SEL_RPL) == 0)
            pCtxCore->cs |= 1;
    }
    else
    {
        AssertMsg((pCtxCore->ss & X86_SEL_RPL) >= 2 || pCtxCore->eflags.Bits.u1VM,
                  ("ring-1 code not supported\n"));
        /*
         * PATM takes care of IOPL and IF flags for Ring-3 and Ring-2 code as well.
         */
        PATMRawEnter(pVM, pCtxCore);
    }

    /*
     * Assert sanity.
     */
    AssertMsg((pCtxCore->eflags.u32 & X86_EFL_IF), ("X86_EFL_IF is clear\n"));
    AssertReleaseMsg(   pCtxCore->eflags.Bits.u2IOPL < (unsigned)(pCtxCore->ss & X86_SEL_RPL)
                     || pCtxCore->eflags.Bits.u1VM,
                     ("X86_EFL_IOPL=%d CPL=%d\n", pCtxCore->eflags.Bits.u2IOPL, pCtxCore->ss & X86_SEL_RPL));
    Assert((pVCpu->cpum.s.Guest.cr0 & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)) == (X86_CR0_PG | X86_CR0_PE | X86_CR0_WP));
    pCtxCore->eflags.u32        |= X86_EFL_IF; /* paranoia */

    pVM->cpum.s.fRawEntered = true;
    return VINF_SUCCESS;
}


/**
 * Transforms the guest CPU state from raw-ring mode to correct values.
 *
 * This function will change any selector registers with DPL=1 to DPL=0.
 *
 * @returns Adjusted rc.
 * @param   pVCpu       The VMCPU handle.
 * @param   rc          Raw mode return code
 * @param   pCtxCore    The context core (for trap usage).
 * @see     @ref pg_raw
 */
VMMDECL(int) CPUMRawLeave(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, int rc)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    /*
     * Don't leave if we've already left (in GC).
     */
    Assert(pVM->cpum.s.fRawEntered);
    if (!pVM->cpum.s.fRawEntered)
        return rc;
    pVM->cpum.s.fRawEntered = false;

    PCPUMCTX pCtx = &pVCpu->cpum.s.Guest;
    if (!pCtxCore)
        pCtxCore = CPUMCTX2CORE(pCtx);
    Assert(pCtxCore->eflags.Bits.u1VM || (pCtxCore->ss & X86_SEL_RPL));
    AssertMsg(pCtxCore->eflags.Bits.u1VM || pCtxCore->eflags.Bits.u2IOPL < (unsigned)(pCtxCore->ss & X86_SEL_RPL),
              ("X86_EFL_IOPL=%d CPL=%d\n", pCtxCore->eflags.Bits.u2IOPL, pCtxCore->ss & X86_SEL_RPL));

    /*
     * Are we executing in raw ring-1?
     */
    if (    (pCtxCore->ss & X86_SEL_RPL) == 1
        &&  !pCtxCore->eflags.Bits.u1VM)
    {
        /*
         * Leave execution mode.
         */
        PATMRawLeave(pVM, pCtxCore, rc);
        /* Not quite sure if this is really required, but shouldn't harm (too much anyways). */
        /** @todo See what happens if we remove this. */
        if ((pCtxCore->ds & X86_SEL_RPL) == 1)
            pCtxCore->ds &= ~X86_SEL_RPL;
        if ((pCtxCore->es & X86_SEL_RPL) == 1)
            pCtxCore->es &= ~X86_SEL_RPL;
        if ((pCtxCore->fs & X86_SEL_RPL) == 1)
            pCtxCore->fs &= ~X86_SEL_RPL;
        if ((pCtxCore->gs & X86_SEL_RPL) == 1)
            pCtxCore->gs &= ~X86_SEL_RPL;

        /*
         * Ring-1 selector => Ring-0.
         */
        pCtxCore->ss &= ~X86_SEL_RPL;
        if ((pCtxCore->cs & X86_SEL_RPL) == 1)
            pCtxCore->cs &= ~X86_SEL_RPL;
    }
    else
    {
        /*
         * PATM is taking care of the IOPL and IF flags for us.
         */
        PATMRawLeave(pVM, pCtxCore, rc);
        if (!pCtxCore->eflags.Bits.u1VM)
        {
            /** @todo See what happens if we remove this. */
            if ((pCtxCore->ds & X86_SEL_RPL) == 1)
                pCtxCore->ds &= ~X86_SEL_RPL;
            if ((pCtxCore->es & X86_SEL_RPL) == 1)
                pCtxCore->es &= ~X86_SEL_RPL;
            if ((pCtxCore->fs & X86_SEL_RPL) == 1)
                pCtxCore->fs &= ~X86_SEL_RPL;
            if ((pCtxCore->gs & X86_SEL_RPL) == 1)
                pCtxCore->gs &= ~X86_SEL_RPL;
        }
    }

    return rc;
}

/**
 * Updates the EFLAGS while we're in raw-mode.
 *
 * @param   pVCpu       The VMCPU handle.
 * @param   pCtxCore    The context core.
 * @param   eflags      The new EFLAGS value.
 */
VMMDECL(void) CPUMRawSetEFlags(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, uint32_t eflags)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    if (!pVM->cpum.s.fRawEntered)
    {
        pCtxCore->eflags.u32 = eflags;
        return;
    }
    PATMRawSetEFlags(pVM, pCtxCore, eflags);
}

#endif /* !IN_RING0 */

/**
 * Gets the EFLAGS while we're in raw-mode.
 *
 * @returns The eflags.
 * @param   pVCpu       The VMCPU handle.
 * @param   pCtxCore    The context core.
 */
VMMDECL(uint32_t) CPUMRawGetEFlags(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore)
{
#ifdef IN_RING0
    return pCtxCore->eflags.u32;
#else
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    if (!pVM->cpum.s.fRawEntered)
        return pCtxCore->eflags.u32;
    return PATMRawGetEFlags(pVM, pCtxCore);
#endif
}


/**
 * Gets and resets the changed flags (CPUM_CHANGED_*).
 * Only REM should call this function.
 *
 * @returns The changed flags.
 * @param   pVCpu       The VMCPU handle.
 */
VMMDECL(unsigned) CPUMGetAndClearChangedFlagsREM(PVMCPU pVCpu)
{
    unsigned fFlags = pVCpu->cpum.s.fChanged;
    pVCpu->cpum.s.fChanged = 0;
    /** @todo change the switcher to use the fChanged flags. */
    if (pVCpu->cpum.s.fUseFlags & CPUM_USED_FPU_SINCE_REM)
    {
        fFlags |= CPUM_CHANGED_FPU_REM;
        pVCpu->cpum.s.fUseFlags &= ~CPUM_USED_FPU_SINCE_REM;
    }
    return fFlags;
}


/**
 * Sets the specified changed flags (CPUM_CHANGED_*).
 *
 * @param   pVCpu       The VMCPU handle.
 */
VMMDECL(void) CPUMSetChangedFlags(PVMCPU pVCpu, uint32_t fChangedFlags)
{
    pVCpu->cpum.s.fChanged |= fChangedFlags;
}


/**
 * Checks if the CPU supports the FXSAVE and FXRSTOR instruction.
 * @returns true if supported.
 * @returns false if not supported.
 * @param   pVM     The VM handle.
 */
VMMDECL(bool) CPUMSupportsFXSR(PVM pVM)
{
    return pVM->cpum.s.CPUFeatures.edx.u1FXSR != 0;
}


/**
 * Checks if the host OS uses the SYSENTER / SYSEXIT instructions.
 * @returns true if used.
 * @returns false if not used.
 * @param   pVM       The VM handle.
 */
VMMDECL(bool) CPUMIsHostUsingSysEnter(PVM pVM)
{
    return (pVM->cpum.s.fHostUseFlags & CPUM_USE_SYSENTER) != 0;
}


/**
 * Checks if the host OS uses the SYSCALL / SYSRET instructions.
 * @returns true if used.
 * @returns false if not used.
 * @param   pVM       The VM handle.
 */
VMMDECL(bool) CPUMIsHostUsingSysCall(PVM pVM)
{
    return (pVM->cpum.s.fHostUseFlags & CPUM_USE_SYSCALL) != 0;
}

#ifndef IN_RING3

/**
 * Lazily sync in the FPU/XMM state
 *
 * @returns VBox status code.
 * @param   pVCpu       VMCPU handle
 */
VMMDECL(int) CPUMHandleLazyFPU(PVMCPU pVCpu)
{
    return cpumHandleLazyFPUAsm(&pVCpu->cpum.s);
}

#endif /* !IN_RING3 */

/**
 * Checks if we activated the FPU/XMM state of the guest OS
 * @returns true if we did.
 * @returns false if not.
 * @param   pVCpu   The VMCPU handle.
 */
VMMDECL(bool) CPUMIsGuestFPUStateActive(PVMCPU pVCpu)
{
    return (pVCpu->cpum.s.fUseFlags & CPUM_USED_FPU) != 0;
}


/**
 * Deactivate the FPU/XMM state of the guest OS
 * @param   pVCpu       The VMCPU handle.
 */
VMMDECL(void) CPUMDeactivateGuestFPUState(PVMCPU pVCpu)
{
    pVCpu->cpum.s.fUseFlags &= ~CPUM_USED_FPU;
}


/**
 * Checks if the guest debug state is active
 *
 * @returns boolean
 * @param   pVM         VM handle.
 */
VMMDECL(bool) CPUMIsGuestDebugStateActive(PVMCPU pVCpu)
{
    return (pVCpu->cpum.s.fUseFlags & CPUM_USE_DEBUG_REGS) != 0;
}


/**
 * Mark the guest's debug state as inactive
 *
 * @returns boolean
 * @param   pVM         VM handle.
 */
VMMDECL(void) CPUMDeactivateGuestDebugState(PVMCPU pVCpu)
{
    pVCpu->cpum.s.fUseFlags &= ~CPUM_USE_DEBUG_REGS;
}


/**
 * Checks if the hidden selector registers are valid
 * @returns true if they are.
 * @returns false if not.
 * @param   pVM     The VM handle.
 */
VMMDECL(bool) CPUMAreHiddenSelRegsValid(PVM pVM)
{
    return HWACCMIsEnabled(pVM);
}



/**
 * Get the current privilege level of the guest.
 *
 * @returns cpl
 * @param   pVM         VM Handle.
 * @param   pRegFrame   Trap register frame.
 */
VMMDECL(uint32_t) CPUMGetGuestCPL(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore)
{
    uint32_t cpl;

    if (CPUMAreHiddenSelRegsValid(pVCpu->CTX_SUFF(pVM)))
    {
        /*
         * The hidden CS.DPL register is always equal to the CPL, it is
         * not affected by loading a conforming coding segment.
         *
         * This only seems to apply to AMD-V; in the VT-x case we *do* need to look
         * at SS. (ACP2 regression during install after a far call to ring 2)
         */
        if (RT_LIKELY(pVCpu->cpum.s.Guest.cr0 & X86_CR0_PE))
            cpl = pCtxCore->ssHid.Attr.n.u2Dpl;
        else
            cpl = 0;  /* CPL set to 3 for VT-x real-mode emulation. */
    }
    else if (RT_LIKELY(pVCpu->cpum.s.Guest.cr0 & X86_CR0_PE))
    {
        if (RT_LIKELY(!pCtxCore->eflags.Bits.u1VM))
        {
            /*
             * The SS RPL is always equal to the CPL, while the CS RPL
             * isn't necessarily equal if the segment is conforming.
             * See section 4.11.1 in the AMD manual.
             */
            cpl = (pCtxCore->ss & X86_SEL_RPL);
#ifndef IN_RING0
            if (cpl == 1)
                cpl = 0;
#endif
        }
        else
            cpl = 3;
    }
    else
        cpl = 0;        /* real mode; cpl is zero */

    return cpl;
}


/**
 * Gets the current guest CPU mode.
 *
 * If paging mode is what you need, check out PGMGetGuestMode().
 *
 * @returns The CPU mode.
 * @param   pVCpu       The VMCPU handle.
 */
VMMDECL(CPUMMODE) CPUMGetGuestMode(PVMCPU pVCpu)
{
    CPUMMODE enmMode;
    if (!(pVCpu->cpum.s.Guest.cr0 & X86_CR0_PE))
        enmMode = CPUMMODE_REAL;
    else if (!(pVCpu->cpum.s.Guest.msrEFER & MSR_K6_EFER_LMA))
        enmMode = CPUMMODE_PROTECTED;
    else
        enmMode = CPUMMODE_LONG;

    return enmMode;
}

