/* $Id$ */
/** @file
 * CPUM - CPU Monitor(/Manager) - Gets and Sets.
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
#include <iprt/assert.h>
#include <iprt/asm.h>



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
 * @param   pVM         The VM handle.
 * @param   pCtxCore    Pointer to the alternative context core or NULL
 *                      to go back to the default context core.
 */
CPUMDECL(void) CPUMHyperSetCtxCore(PVM pVM, PCPUMCTXCORE pCtxCore)
{
    LogFlow(("CPUMHyperSetCtxCore: %p/%p/%p -> %p\n", pVM->cpum.s.CTXALLSUFF(pHyperCore), pCtxCore));
    if (!pCtxCore)
    {
        pCtxCore = CPUMCTX2CORE(&pVM->cpum.s.Hyper);
        pVM->cpum.s.pHyperCoreR3 = (R3PTRTYPE(PCPUMCTXCORE))VM_R3_ADDR(pVM, pCtxCore);
        pVM->cpum.s.pHyperCoreR0 = (R0PTRTYPE(PCPUMCTXCORE))VM_R0_ADDR(pVM, pCtxCore);
        pVM->cpum.s.pHyperCoreGC = (RCPTRTYPE(PCPUMCTXCORE))VM_GUEST_ADDR(pVM, pCtxCore);
    }
    else
    {
        pVM->cpum.s.pHyperCoreR3 = (R3PTRTYPE(PCPUMCTXCORE))MMHyperCCToR3(pVM, pCtxCore);
        pVM->cpum.s.pHyperCoreR0 = (R0PTRTYPE(PCPUMCTXCORE))MMHyperCCToR0(pVM, pCtxCore);
        pVM->cpum.s.pHyperCoreGC = (RCPTRTYPE(PCPUMCTXCORE))MMHyperCCToGC(pVM, pCtxCore);
    }
}


/**
 * Gets the pointer to the internal CPUMCTXCORE structure for the hypervisor.
 * This is only for reading in order to save a few calls.
 *
 * @param   pVM         Handle to the virtual machine.
 */
CPUMDECL(PCCPUMCTXCORE) CPUMGetHyperCtxCore(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore);
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
CPUMDECL(int) CPUMQueryHyperCtxPtr(PVM pVM, PCPUMCTX *ppCtx)
{
    *ppCtx = &pVM->cpum.s.Hyper;
    return VINF_SUCCESS;
}

CPUMDECL(void) CPUMSetHyperGDTR(PVM pVM, uint32_t addr, uint16_t limit)
{
    pVM->cpum.s.Hyper.gdtr.cbGdt = limit;
    pVM->cpum.s.Hyper.gdtr.pGdt  = addr;
    pVM->cpum.s.Hyper.gdtrPadding = 0;
}

CPUMDECL(void) CPUMSetHyperIDTR(PVM pVM, uint32_t addr, uint16_t limit)
{
    pVM->cpum.s.Hyper.idtr.cbIdt = limit;
    pVM->cpum.s.Hyper.idtr.pIdt = addr;
    pVM->cpum.s.Hyper.idtrPadding = 0;
}

CPUMDECL(void) CPUMSetHyperCR3(PVM pVM, uint32_t cr3)
{
    pVM->cpum.s.Hyper.cr3 = cr3;
}

CPUMDECL(void) CPUMSetHyperCS(PVM pVM, RTSEL SelCS)
{
    pVM->cpum.s.CTXALLSUFF(pHyperCore)->cs = SelCS;
}

CPUMDECL(void) CPUMSetHyperDS(PVM pVM, RTSEL SelDS)
{
    pVM->cpum.s.CTXALLSUFF(pHyperCore)->ds = SelDS;
}

CPUMDECL(void) CPUMSetHyperES(PVM pVM, RTSEL SelES)
{
    pVM->cpum.s.CTXALLSUFF(pHyperCore)->es = SelES;
}

CPUMDECL(void) CPUMSetHyperFS(PVM pVM, RTSEL SelFS)
{
    pVM->cpum.s.CTXALLSUFF(pHyperCore)->fs = SelFS;
}

CPUMDECL(void) CPUMSetHyperGS(PVM pVM, RTSEL SelGS)
{
    pVM->cpum.s.CTXALLSUFF(pHyperCore)->gs = SelGS;
}

CPUMDECL(void) CPUMSetHyperSS(PVM pVM, RTSEL SelSS)
{
    pVM->cpum.s.CTXALLSUFF(pHyperCore)->ss = SelSS;
}

CPUMDECL(void) CPUMSetHyperESP(PVM pVM, uint32_t u32ESP)
{
    pVM->cpum.s.CTXALLSUFF(pHyperCore)->esp = u32ESP;
}

CPUMDECL(int) CPUMSetHyperEFlags(PVM pVM, uint32_t Efl)
{
    pVM->cpum.s.CTXALLSUFF(pHyperCore)->eflags.u32 = Efl;
    return VINF_SUCCESS;
}

CPUMDECL(void) CPUMSetHyperEIP(PVM pVM, uint32_t u32EIP)
{
    pVM->cpum.s.CTXALLSUFF(pHyperCore)->eip = u32EIP;
}

CPUMDECL(void) CPUMSetHyperTR(PVM pVM, RTSEL SelTR)
{
    pVM->cpum.s.Hyper.tr = SelTR;
}

CPUMDECL(void) CPUMSetHyperLDTR(PVM pVM, RTSEL SelLDTR)
{
    pVM->cpum.s.Hyper.ldtr = SelLDTR;
}

CPUMDECL(void) CPUMSetHyperDR0(PVM pVM, RTGCUINTREG uDr0)
{
    pVM->cpum.s.Hyper.dr0 = uDr0;
    /** @todo in GC we must load it! */
}

CPUMDECL(void) CPUMSetHyperDR1(PVM pVM, RTGCUINTREG uDr1)
{
    pVM->cpum.s.Hyper.dr1 = uDr1;
    /** @todo in GC we must load it! */
}

CPUMDECL(void) CPUMSetHyperDR2(PVM pVM, RTGCUINTREG uDr2)
{
    pVM->cpum.s.Hyper.dr2 = uDr2;
    /** @todo in GC we must load it! */
}

CPUMDECL(void) CPUMSetHyperDR3(PVM pVM, RTGCUINTREG uDr3)
{
    pVM->cpum.s.Hyper.dr3 = uDr3;
    /** @todo in GC we must load it! */
}

CPUMDECL(void) CPUMSetHyperDR6(PVM pVM, RTGCUINTREG uDr6)
{
    pVM->cpum.s.Hyper.dr6 = uDr6;
    /** @todo in GC we must load it! */
}

CPUMDECL(void) CPUMSetHyperDR7(PVM pVM, RTGCUINTREG uDr7)
{
    pVM->cpum.s.Hyper.dr7 = uDr7;
    /** @todo in GC we must load it! */
}


CPUMDECL(RTSEL) CPUMGetHyperCS(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->cs;
}

CPUMDECL(RTSEL) CPUMGetHyperDS(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->ds;
}

CPUMDECL(RTSEL) CPUMGetHyperES(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->es;
}

CPUMDECL(RTSEL) CPUMGetHyperFS(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->fs;
}

CPUMDECL(RTSEL) CPUMGetHyperGS(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->gs;
}

CPUMDECL(RTSEL) CPUMGetHyperSS(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->ss;
}

#if 0 /* these are not correct. */

CPUMDECL(uint32_t) CPUMGetHyperCR0(PVM pVM)
{
    return pVM->cpum.s.Hyper.cr0;
}

CPUMDECL(uint32_t) CPUMGetHyperCR2(PVM pVM)
{
    return pVM->cpum.s.Hyper.cr2;
}

CPUMDECL(uint32_t) CPUMGetHyperCR3(PVM pVM)
{
    return pVM->cpum.s.Hyper.cr3;
}

CPUMDECL(uint32_t) CPUMGetHyperCR4(PVM pVM)
{
    return pVM->cpum.s.Hyper.cr4;
}

#endif /* not correct */

CPUMDECL(uint32_t) CPUMGetHyperEAX(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->eax;
}

CPUMDECL(uint32_t) CPUMGetHyperEBX(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->ebx;
}

CPUMDECL(uint32_t) CPUMGetHyperECX(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->ecx;
}

CPUMDECL(uint32_t) CPUMGetHyperEDX(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->edx;
}

CPUMDECL(uint32_t) CPUMGetHyperESI(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->esi;
}

CPUMDECL(uint32_t) CPUMGetHyperEDI(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->edi;
}

CPUMDECL(uint32_t) CPUMGetHyperEBP(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->ebp;
}

CPUMDECL(uint32_t) CPUMGetHyperESP(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->esp;
}

CPUMDECL(uint32_t) CPUMGetHyperEFlags(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->eflags.u32;
}

CPUMDECL(uint32_t) CPUMGetHyperEIP(PVM pVM)
{
    return pVM->cpum.s.CTXALLSUFF(pHyperCore)->eip;
}

CPUMDECL(uint32_t) CPUMGetHyperIDTR(PVM pVM, uint16_t *pcbLimit)
{
    if (pcbLimit)
        *pcbLimit = pVM->cpum.s.Hyper.idtr.cbIdt;
    return pVM->cpum.s.Hyper.idtr.pIdt;
}

CPUMDECL(uint32_t) CPUMGetHyperGDTR(PVM pVM, uint16_t *pcbLimit)
{
    if (pcbLimit)
        *pcbLimit = pVM->cpum.s.Hyper.gdtr.cbGdt;
    return pVM->cpum.s.Hyper.gdtr.pGdt;
}

CPUMDECL(RTSEL) CPUMGetHyperLDTR(PVM pVM)
{
    return pVM->cpum.s.Hyper.ldtr;
}

CPUMDECL(RTGCUINTREG) CPUMGetHyperDR0(PVM pVM)
{
    return pVM->cpum.s.Hyper.dr0;
}

CPUMDECL(RTGCUINTREG) CPUMGetHyperDR1(PVM pVM)
{
    return pVM->cpum.s.Hyper.dr1;
}

CPUMDECL(RTGCUINTREG) CPUMGetHyperDR2(PVM pVM)
{
    return pVM->cpum.s.Hyper.dr2;
}

CPUMDECL(RTGCUINTREG) CPUMGetHyperDR3(PVM pVM)
{
    return pVM->cpum.s.Hyper.dr3;
}

CPUMDECL(RTGCUINTREG) CPUMGetHyperDR6(PVM pVM)
{
    return pVM->cpum.s.Hyper.dr6;
}

CPUMDECL(RTGCUINTREG) CPUMGetHyperDR7(PVM pVM)
{
    return pVM->cpum.s.Hyper.dr7;
}


/**
 * Gets the pointer to the internal CPUMCTXCORE structure.
 * This is only for reading in order to save a few calls.
 *
 * @param   pVM         Handle to the virtual machine.
 */
CPUMDECL(PCCPUMCTXCORE) CPUMGetGuestCtxCore(PVM pVM)
{
    return CPUMCTX2CORE(&pVM->cpum.s.Guest);
}


/**
 * Sets the guest context core registers.
 *
 * @param   pVM         Handle to the virtual machine.
 * @param   pCtxCore    The new context core values.
 */
CPUMDECL(void) CPUMSetGuestCtxCore(PVM pVM, PCCPUMCTXCORE pCtxCore)
{
    /** @todo #1410 requires selectors to be checked. */

    PCPUMCTXCORE pCtxCoreDst = CPUMCTX2CORE(&pVM->cpum.s.Guest);
    *pCtxCoreDst = *pCtxCore;

    /* Mask away invalid parts of the cpu context. */
    if (!CPUMIsGuestInLongMode(pVM))
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
 * @returns VBox status code.
 * @param   pVM         Handle to the virtual machine.
 * @param   ppCtx       Receives the CPUMCTX pointer when successful.
 */
CPUMDECL(int) CPUMQueryGuestCtxPtr(PVM pVM, PCPUMCTX *ppCtx)
{
    *ppCtx = &pVM->cpum.s.Guest;
    return VINF_SUCCESS;
}


CPUMDECL(int) CPUMSetGuestGDTR(PVM pVM, uint32_t addr, uint16_t limit)
{
    pVM->cpum.s.Guest.gdtr.cbGdt = limit;
    pVM->cpum.s.Guest.gdtr.pGdt  = addr;
    pVM->cpum.s.fChanged |= CPUM_CHANGED_GDTR;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestIDTR(PVM pVM, uint32_t addr, uint16_t limit)
{
    pVM->cpum.s.Guest.idtr.cbIdt = limit;
    pVM->cpum.s.Guest.idtr.pIdt = addr;
    pVM->cpum.s.fChanged |= CPUM_CHANGED_IDTR;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestTR(PVM pVM, uint16_t tr)
{
    pVM->cpum.s.Guest.tr = tr;
    pVM->cpum.s.fChanged |= CPUM_CHANGED_TR;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestLDTR(PVM pVM, uint16_t ldtr)
{
    pVM->cpum.s.Guest.ldtr = ldtr;
    pVM->cpum.s.fChanged |= CPUM_CHANGED_LDTR;
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
 * @param   pVM     Pointer to the shared VM structure.
 * @param   cr0     The new CR0 value.
 */
CPUMDECL(int) CPUMSetGuestCR0(PVM pVM, uint64_t cr0)
{
#ifdef IN_GC
    /*
     * Check if we need to change hypervisor CR0 because
     * of math stuff.
     */
    if (    (cr0                   & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP))
        !=  (pVM->cpum.s.Guest.cr0 & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP)))
    {
        if (!(pVM->cpum.s.fUseFlags & CPUM_USED_FPU))
        {
            /*
             * We haven't saved the host FPU state yet, so TS and MT are both set
             * and EM should be reflecting the guest EM (it always does this).
             */
            if ((cr0 & X86_CR0_EM) != (pVM->cpum.s.Guest.cr0 & X86_CR0_EM))
            {
                uint32_t HyperCR0 = ASMGetCR0();
                AssertMsg((HyperCR0 & (X86_CR0_TS | X86_CR0_MP)) == (X86_CR0_TS | X86_CR0_MP), ("%#x\n", HyperCR0));
                AssertMsg((HyperCR0 & X86_CR0_EM) == (pVM->cpum.s.Guest.cr0 & X86_CR0_EM), ("%#x\n", HyperCR0));
                HyperCR0 &= ~X86_CR0_EM;
                HyperCR0 |= cr0 & X86_CR0_EM;
                Log(("CPUM New HyperCR0=%#x\n", HyperCR0));
                ASMSetCR0(HyperCR0);
            }
#ifdef VBOX_STRICT
            else
            {
                uint32_t HyperCR0 = ASMGetCR0();
                AssertMsg((HyperCR0 & (X86_CR0_TS | X86_CR0_MP)) == (X86_CR0_TS | X86_CR0_MP), ("%#x\n", HyperCR0));
                AssertMsg((HyperCR0 & X86_CR0_EM) == (pVM->cpum.s.Guest.cr0 & X86_CR0_EM), ("%#x\n", HyperCR0));
            }
#endif
        }
        else
        {
            /*
             * Already saved the state, so we're just mirroring
             * the guest flags.
             */
            uint32_t HyperCR0 = ASMGetCR0();
            AssertMsg(     (HyperCR0               & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP))
                      ==   (pVM->cpum.s.Guest.cr0  & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP)),
                      ("%#x %#x\n", HyperCR0, pVM->cpum.s.Guest.cr0));
            HyperCR0 &= ~(X86_CR0_TS | X86_CR0_EM | X86_CR0_MP);
            HyperCR0 |= cr0 & (X86_CR0_TS | X86_CR0_EM | X86_CR0_MP);
            Log(("CPUM New HyperCR0=%#x\n", HyperCR0));
            ASMSetCR0(HyperCR0);
        }
    }
#endif

    /*
     * Check for changes causing TLB flushes (for REM).
     * The caller is responsible for calling PGM when appropriate.
     */
    if (    (cr0                   & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE))
        !=  (pVM->cpum.s.Guest.cr0 & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)))
        pVM->cpum.s.fChanged |= CPUM_CHANGED_GLOBAL_TLB_FLUSH;
    pVM->cpum.s.fChanged |= CPUM_CHANGED_CR0;

    pVM->cpum.s.Guest.cr0 = cr0 | X86_CR0_ET;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestCR2(PVM pVM, uint64_t cr2)
{
    pVM->cpum.s.Guest.cr2 = cr2;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestCR3(PVM pVM, uint64_t cr3)
{
    pVM->cpum.s.Guest.cr3 = cr3;
    pVM->cpum.s.fChanged |= CPUM_CHANGED_CR3;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestCR4(PVM pVM, uint64_t cr4)
{
    if (    (cr4                   & (X86_CR4_PGE | X86_CR4_PAE | X86_CR4_PSE))
        !=  (pVM->cpum.s.Guest.cr4 & (X86_CR4_PGE | X86_CR4_PAE | X86_CR4_PSE)))
        pVM->cpum.s.fChanged |= CPUM_CHANGED_GLOBAL_TLB_FLUSH;
    pVM->cpum.s.fChanged |= CPUM_CHANGED_CR4;
    if (!CPUMSupportsFXSR(pVM))
        cr4 &= ~X86_CR4_OSFSXR;
    pVM->cpum.s.Guest.cr4 = cr4;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestEFlags(PVM pVM, uint32_t eflags)
{
    pVM->cpum.s.Guest.eflags.u32 = eflags;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestEIP(PVM pVM, uint32_t eip)
{
    pVM->cpum.s.Guest.eip = eip;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestEAX(PVM pVM, uint32_t eax)
{
    pVM->cpum.s.Guest.eax = eax;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestEBX(PVM pVM, uint32_t ebx)
{
    pVM->cpum.s.Guest.ebx = ebx;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestECX(PVM pVM, uint32_t ecx)
{
    pVM->cpum.s.Guest.ecx = ecx;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestEDX(PVM pVM, uint32_t edx)
{
    pVM->cpum.s.Guest.edx = edx;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestESP(PVM pVM, uint32_t esp)
{
    pVM->cpum.s.Guest.esp = esp;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestEBP(PVM pVM, uint32_t ebp)
{
    pVM->cpum.s.Guest.ebp = ebp;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestESI(PVM pVM, uint32_t esi)
{
    pVM->cpum.s.Guest.esi = esi;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestEDI(PVM pVM, uint32_t edi)
{
    pVM->cpum.s.Guest.edi = edi;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestSS(PVM pVM, uint16_t ss)
{
    pVM->cpum.s.Guest.ss = ss;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestCS(PVM pVM, uint16_t cs)
{
    pVM->cpum.s.Guest.cs = cs;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestDS(PVM pVM, uint16_t ds)
{
    pVM->cpum.s.Guest.ds = ds;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestES(PVM pVM, uint16_t es)
{
    pVM->cpum.s.Guest.es = es;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestFS(PVM pVM, uint16_t fs)
{
    pVM->cpum.s.Guest.fs = fs;
    return VINF_SUCCESS;
}

CPUMDECL(int) CPUMSetGuestGS(PVM pVM, uint16_t gs)
{
    pVM->cpum.s.Guest.gs = gs;
    return VINF_SUCCESS;
}

CPUMDECL(void) CPUMSetGuestEFER(PVM pVM, uint64_t val)
{
    pVM->cpum.s.Guest.msrEFER = val;
}

CPUMDECL(uint32_t) CPUMGetGuestIDTR(PVM pVM, uint16_t *pcbLimit)
{
    if (pcbLimit)
        *pcbLimit = pVM->cpum.s.Guest.idtr.cbIdt;
    return pVM->cpum.s.Guest.idtr.pIdt;
}

CPUMDECL(RTSEL) CPUMGetGuestTR(PVM pVM)
{
    return pVM->cpum.s.Guest.tr;
}

CPUMDECL(RTSEL) CPUMGetGuestCS(PVM pVM)
{
    return pVM->cpum.s.Guest.cs;
}

CPUMDECL(RTSEL) CPUMGetGuestDS(PVM pVM)
{
    return pVM->cpum.s.Guest.ds;
}

CPUMDECL(RTSEL) CPUMGetGuestES(PVM pVM)
{
    return pVM->cpum.s.Guest.es;
}

CPUMDECL(RTSEL) CPUMGetGuestFS(PVM pVM)
{
    return pVM->cpum.s.Guest.fs;
}

CPUMDECL(RTSEL) CPUMGetGuestGS(PVM pVM)
{
    return pVM->cpum.s.Guest.gs;
}

CPUMDECL(RTSEL) CPUMGetGuestSS(PVM pVM)
{
    return pVM->cpum.s.Guest.ss;
}

CPUMDECL(RTSEL) CPUMGetGuestLDTR(PVM pVM)
{
    return pVM->cpum.s.Guest.ldtr;
}

CPUMDECL(uint64_t) CPUMGetGuestCR0(PVM pVM)
{
    return pVM->cpum.s.Guest.cr0;
}

CPUMDECL(uint64_t) CPUMGetGuestCR2(PVM pVM)
{
    return pVM->cpum.s.Guest.cr2;
}

CPUMDECL(uint64_t) CPUMGetGuestCR3(PVM pVM)
{
    return pVM->cpum.s.Guest.cr3;
}

CPUMDECL(uint64_t) CPUMGetGuestCR4(PVM pVM)
{
    return pVM->cpum.s.Guest.cr4;
}

CPUMDECL(void) CPUMGetGuestGDTR(PVM pVM, PVBOXGDTR pGDTR)
{
    *pGDTR = pVM->cpum.s.Guest.gdtr;
}

CPUMDECL(uint32_t) CPUMGetGuestEIP(PVM pVM)
{
    return pVM->cpum.s.Guest.eip;
}

CPUMDECL(uint32_t) CPUMGetGuestEAX(PVM pVM)
{
    return pVM->cpum.s.Guest.eax;
}

CPUMDECL(uint32_t) CPUMGetGuestEBX(PVM pVM)
{
    return pVM->cpum.s.Guest.ebx;
}

CPUMDECL(uint32_t) CPUMGetGuestECX(PVM pVM)
{
    return pVM->cpum.s.Guest.ecx;
}

CPUMDECL(uint32_t) CPUMGetGuestEDX(PVM pVM)
{
    return pVM->cpum.s.Guest.edx;
}

CPUMDECL(uint32_t) CPUMGetGuestESI(PVM pVM)
{
    return pVM->cpum.s.Guest.esi;
}

CPUMDECL(uint32_t) CPUMGetGuestEDI(PVM pVM)
{
    return pVM->cpum.s.Guest.edi;
}

CPUMDECL(uint32_t) CPUMGetGuestESP(PVM pVM)
{
    return pVM->cpum.s.Guest.esp;
}

CPUMDECL(uint32_t) CPUMGetGuestEBP(PVM pVM)
{
    return pVM->cpum.s.Guest.ebp;
}

CPUMDECL(uint32_t) CPUMGetGuestEFlags(PVM pVM)
{
    return pVM->cpum.s.Guest.eflags.u32;
}

CPUMDECL(CPUMSELREGHID *) CPUMGetGuestTRHid(PVM pVM)
{
    return &pVM->cpum.s.Guest.trHid;
}

//@todo: crx should be an array
CPUMDECL(int) CPUMGetGuestCRx(PVM pVM, unsigned iReg, uint64_t *pValue)
{
    switch (iReg)
    {
        case USE_REG_CR0:
            *pValue = pVM->cpum.s.Guest.cr0;
            break;
        case USE_REG_CR2:
            *pValue = pVM->cpum.s.Guest.cr2;
            break;
        case USE_REG_CR3:
            *pValue = pVM->cpum.s.Guest.cr3;
            break;
        case USE_REG_CR4:
            *pValue = pVM->cpum.s.Guest.cr4;
            break;
        default:
            return VERR_INVALID_PARAMETER;
    }
    return VINF_SUCCESS;
}

CPUMDECL(uint64_t) CPUMGetGuestDR0(PVM pVM)
{
    return pVM->cpum.s.Guest.dr0;
}

CPUMDECL(uint64_t) CPUMGetGuestDR1(PVM pVM)
{
    return pVM->cpum.s.Guest.dr1;
}

CPUMDECL(uint64_t) CPUMGetGuestDR2(PVM pVM)
{
    return pVM->cpum.s.Guest.dr2;
}

CPUMDECL(uint64_t) CPUMGetGuestDR3(PVM pVM)
{
    return pVM->cpum.s.Guest.dr3;
}

CPUMDECL(uint64_t) CPUMGetGuestDR6(PVM pVM)
{
    return pVM->cpum.s.Guest.dr6;
}

CPUMDECL(uint64_t) CPUMGetGuestDR7(PVM pVM)
{
    return pVM->cpum.s.Guest.dr7;
}

/** @todo drx should be an array */
CPUMDECL(int) CPUMGetGuestDRx(PVM pVM, uint32_t iReg, uint64_t *pValue)
{
    switch (iReg)
    {
        case USE_REG_DR0:
            *pValue = pVM->cpum.s.Guest.dr0;
            break;
        case USE_REG_DR1:
            *pValue = pVM->cpum.s.Guest.dr1;
            break;
        case USE_REG_DR2:
            *pValue = pVM->cpum.s.Guest.dr2;
            break;
        case USE_REG_DR3:
            *pValue = pVM->cpum.s.Guest.dr3;
            break;
        case USE_REG_DR4:
        case USE_REG_DR6:
            *pValue = pVM->cpum.s.Guest.dr6;
            break;
        case USE_REG_DR5:
        case USE_REG_DR7:
            *pValue = pVM->cpum.s.Guest.dr7;
            break;

        default:
            return VERR_INVALID_PARAMETER;
    }
    return VINF_SUCCESS;
}

CPUMDECL(uint64_t) CPUMGetGuestEFER(PVM pVM)
{
    return pVM->cpum.s.Guest.msrEFER;
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
CPUMDECL(void) CPUMGetGuestCpuId(PVM pVM, uint32_t iLeaf, uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx)
{
    PCCPUMCPUID pCpuId;
    if (iLeaf < ELEMENTS(pVM->cpum.s.aGuestCpuIdStd))
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
 * Gets a pointer to the array of standard CPUID leafs.
 *
 * CPUMGetGuestCpuIdStdMax() give the size of the array.
 *
 * @returns Pointer to the standard CPUID leafs (read-only).
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
CPUMDECL(RCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdStdGCPtr(PVM pVM)
{
    return RCPTRTYPE(PCCPUMCPUID)VM_GUEST_ADDR(pVM, &pVM->cpum.s.aGuestCpuIdStd[0]);
}

/**
 * Gets a pointer to the array of extended CPUID leafs.
 *
 * CPUMGetGuestCpuIdExtMax() give the size of the array.
 *
 * @returns Pointer to the extended CPUID leafs (read-only).
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
CPUMDECL(RCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdExtGCPtr(PVM pVM)
{
    return RCPTRTYPE(PCCPUMCPUID)VM_GUEST_ADDR(pVM, &pVM->cpum.s.aGuestCpuIdExt[0]);
}

/**
 * Gets a pointer to the array of centaur CPUID leafs.
 *
 * CPUMGetGuestCpuIdCentaurMax() give the size of the array.
 *
 * @returns Pointer to the centaur CPUID leafs (read-only).
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
CPUMDECL(RCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdCentaurGCPtr(PVM pVM)
{
    return RCPTRTYPE(PCCPUMCPUID)VM_GUEST_ADDR(pVM, &pVM->cpum.s.aGuestCpuIdCentaur[0]);
}

/**
 * Gets a pointer to the default CPUID leaf.
 *
 * @returns Pointer to the default CPUID leaf (read-only).
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
CPUMDECL(RCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdDefGCPtr(PVM pVM)
{
    return RCPTRTYPE(PCCPUMCPUID)VM_GUEST_ADDR(pVM, &pVM->cpum.s.GuestCpuIdDef);
}

/**
 * Gets a number of standard CPUID leafs.
 *
 * @returns Number of leafs.
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
CPUMDECL(uint32_t) CPUMGetGuestCpuIdStdMax(PVM pVM)
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
CPUMDECL(uint32_t) CPUMGetGuestCpuIdExtMax(PVM pVM)
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
CPUMDECL(uint32_t) CPUMGetGuestCpuIdCentaurMax(PVM pVM)
{
    return RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur);
}

/**
 * Sets a CPUID feature bit.
 *
 * @param   pVM             The VM Handle.
 * @param   enmFeature      The feature to set.
 */
CPUMDECL(void) CPUMSetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature)
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
                LogRel(("WARNING: Can't turn on SYSCALL/SYSRET when the host doesn't support it!!\n"));
                return;
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

        default:
            AssertMsgFailed(("enmFeature=%d\n", enmFeature));
            break;
    }
    pVM->cpum.s.fChanged |= CPUM_CHANGED_CPUID;
}

/**
 * Queries a CPUID feature bit.
 *
 * @returns boolean for feature presence
 * @param   pVM             The VM Handle.
 * @param   enmFeature      The feature to query.
 */
CPUMDECL(bool) CPUMGetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature)
{
    switch (enmFeature)
    {
        case CPUMCPUIDFEATURE_PAE:
        {
            if (pVM->cpum.s.aGuestCpuIdStd[0].eax >= 1)
                return !!(pVM->cpum.s.aGuestCpuIdStd[1].edx & X86_CPUID_FEATURE_EDX_PAE);
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
CPUMDECL(void) CPUMClearGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature)
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

        default:
            AssertMsgFailed(("enmFeature=%d\n", enmFeature));
            break;
    }
    pVM->cpum.s.fChanged |= CPUM_CHANGED_CPUID;
}

/**
 * Gets the CPU vendor 
 *
 * @returns CPU vendor
 * @param   pVM     The VM handle.
 */
CPUMDECL(CPUMCPUVENDOR) CPUMGetCPUVendor(PVM pVM)
{
    return pVM->cpum.s.enmCPUVendor;
}


CPUMDECL(int) CPUMSetGuestDR0(PVM pVM, uint64_t uDr0)
{
    pVM->cpum.s.Guest.dr0 = uDr0;
    return CPUMRecalcHyperDRx(pVM);
}

CPUMDECL(int) CPUMSetGuestDR1(PVM pVM, uint64_t uDr1)
{
    pVM->cpum.s.Guest.dr1 = uDr1;
    return CPUMRecalcHyperDRx(pVM);
}

CPUMDECL(int) CPUMSetGuestDR2(PVM pVM, uint64_t uDr2)
{
    pVM->cpum.s.Guest.dr2 = uDr2;
    return CPUMRecalcHyperDRx(pVM);
}

CPUMDECL(int) CPUMSetGuestDR3(PVM pVM, uint64_t uDr3)
{
    pVM->cpum.s.Guest.dr3 = uDr3;
    return CPUMRecalcHyperDRx(pVM);
}

CPUMDECL(int) CPUMSetGuestDR6(PVM pVM, uint64_t uDr6)
{
    pVM->cpum.s.Guest.dr6 = uDr6;
    return CPUMRecalcHyperDRx(pVM);
}

CPUMDECL(int) CPUMSetGuestDR7(PVM pVM, uint64_t uDr7)
{
    pVM->cpum.s.Guest.dr7 = uDr7;
    return CPUMRecalcHyperDRx(pVM);
}

/** @todo drx should be an array */
CPUMDECL(int) CPUMSetGuestDRx(PVM pVM, uint32_t iReg, uint64_t Value)
{
    switch (iReg)
    {
        case USE_REG_DR0:
            pVM->cpum.s.Guest.dr0 = Value;
            break;
        case USE_REG_DR1:
            pVM->cpum.s.Guest.dr1 = Value;
            break;
        case USE_REG_DR2:
            pVM->cpum.s.Guest.dr2 = Value;
            break;
        case USE_REG_DR3:
            pVM->cpum.s.Guest.dr3 = Value;
            break;
        case USE_REG_DR4:
        case USE_REG_DR6:
            pVM->cpum.s.Guest.dr6 = Value;
            break;
        case USE_REG_DR5:
        case USE_REG_DR7:
            pVM->cpum.s.Guest.dr7 = Value;
            break;

        default:
            return VERR_INVALID_PARAMETER;
    }
    return CPUMRecalcHyperDRx(pVM);
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
 * @param   pVM     The VM handle.
 */
CPUMDECL(int) CPUMRecalcHyperDRx(PVM pVM)
{
    /*
     * Compare the DR7s first.
     *
     * We only care about the enabled flags. The GE and LE flags are always
     * set and we don't care if the guest doesn't set them. GD is virtualized
     * when we dispatch #DB, we never enable it.
     */
    const RTGCUINTREG uDbgfDr7 = DBGFBpGetDR7(pVM);
#ifdef CPUM_VIRTUALIZE_DRX
    const RTGCUINTREG uGstDr7  = CPUMGetGuestDR7(pVM);
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
            uNewDr0 = CPUMGetGuestDR0(pVM);
        }
        else
            uNewDr0 = pVM->cpum.s.Hyper.dr0;

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
            uNewDr1 = CPUMGetGuestDR1(pVM);
        }
        else
            uNewDr1 = pVM->cpum.s.Hyper.dr1;

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
            uNewDr2 = CPUMGetGuestDR2(pVM);
        }
        else
            uNewDr2 = pVM->cpum.s.Hyper.dr2;

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
            uNewDr3 = CPUMGetGuestDR3(pVM);
        }
        else
            uNewDr3 = pVM->cpum.s.Hyper.dr3;

        /*
         * Apply the updates.
         */
#ifdef IN_GC
        if (!(pVM->cpum.s.fUseFlags & CPUM_USE_DEBUG_REGS))
        {
            /** @todo save host DBx registers. */
        }
#endif
        pVM->cpum.s.fUseFlags |= CPUM_USE_DEBUG_REGS;
        if (uNewDr3 != pVM->cpum.s.Hyper.dr3)
            CPUMSetHyperDR3(pVM, uNewDr3);
        if (uNewDr2 != pVM->cpum.s.Hyper.dr2)
            CPUMSetHyperDR2(pVM, uNewDr2);
        if (uNewDr1 != pVM->cpum.s.Hyper.dr1)
            CPUMSetHyperDR1(pVM, uNewDr1);
        if (uNewDr0 != pVM->cpum.s.Hyper.dr0)
            CPUMSetHyperDR0(pVM, uNewDr0);
        if (uNewDr7 != pVM->cpum.s.Hyper.dr7)
            CPUMSetHyperDR7(pVM, uNewDr7);
    }
    else
    {
#ifdef IN_GC
        if (pVM->cpum.s.fUseFlags & CPUM_USE_DEBUG_REGS)
        {
            /** @todo restore host DBx registers. */
        }
#endif
        pVM->cpum.s.fUseFlags &= ~CPUM_USE_DEBUG_REGS;
    }
    Log2(("CPUMRecalcHyperDRx: fUseFlags=%#x %RGr %RGr %RGr %RGr  %RGr %RGr\n",
          pVM->cpum.s.fUseFlags, pVM->cpum.s.Hyper.dr0, pVM->cpum.s.Hyper.dr1,
         pVM->cpum.s.Hyper.dr2, pVM->cpum.s.Hyper.dr3, pVM->cpum.s.Hyper.dr6,
         pVM->cpum.s.Hyper.dr7));

    return VINF_SUCCESS;
}

#ifndef IN_RING0  /** @todo I don't think we need this in R0, so move it to CPUMAll.cpp? */

/**
 * Transforms the guest CPU state to raw-ring mode.
 *
 * This function will change the any of the cs and ss register with DPL=0 to DPL=1.
 *
 * @returns VBox status. (recompiler failure)
 * @param   pVM         VM handle.
 * @param   pCtxCore    The context core (for trap usage).
 * @see     @ref pg_raw
 */
CPUMDECL(int) CPUMRawEnter(PVM pVM, PCPUMCTXCORE pCtxCore)
{
    Assert(!pVM->cpum.s.fRawEntered);
    if (!pCtxCore)
        pCtxCore = CPUMCTX2CORE(&pVM->cpum.s.Guest);

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
    Assert((pVM->cpum.s.Guest.cr0 & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)) == (X86_CR0_PG | X86_CR0_PE | X86_CR0_WP));
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
 * @param   pVM         VM handle.
 * @param   rc          Raw mode return code
 * @param   pCtxCore    The context core (for trap usage).
 * @see     @ref pg_raw
 */
CPUMDECL(int) CPUMRawLeave(PVM pVM, PCPUMCTXCORE pCtxCore, int rc)
{
    /*
     * Don't leave if we've already left (in GC).
     */
    Assert(pVM->cpum.s.fRawEntered);
    if (!pVM->cpum.s.fRawEntered)
        return rc;
    pVM->cpum.s.fRawEntered = false;

    PCPUMCTX pCtx = &pVM->cpum.s.Guest;
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
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 * @param   eflags      The new EFLAGS value.
 */
CPUMDECL(void) CPUMRawSetEFlags(PVM pVM, PCPUMCTXCORE pCtxCore, uint32_t eflags)
{
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
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 */
CPUMDECL(uint32_t) CPUMRawGetEFlags(PVM pVM, PCPUMCTXCORE pCtxCore)
{
#ifdef IN_RING0
    return pCtxCore->eflags.u32;
#else
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
 * @param   pVM         The VM handle.
 */
CPUMDECL(unsigned) CPUMGetAndClearChangedFlagsREM(PVM pVM)
{
    unsigned fFlags = pVM->cpum.s.fChanged;
    pVM->cpum.s.fChanged = 0;
    /** @todo change the switcher to use the fChanged flags. */
    if (pVM->cpum.s.fUseFlags & CPUM_USED_FPU_SINCE_REM)
    {
        fFlags |= CPUM_CHANGED_FPU_REM;
        pVM->cpum.s.fUseFlags &= ~CPUM_USED_FPU_SINCE_REM;
    }
    return fFlags;
}

/**
 * Sets the specified changed flags (CPUM_CHANGED_*).
 *
 * @param   pVM     The VM handle.
 */
CPUMDECL(void) CPUMSetChangedFlags(PVM pVM, uint32_t fChangedFlags)
{
    pVM->cpum.s.fChanged |= fChangedFlags;
}

/**
 * Checks if the CPU supports the FXSAVE and FXRSTOR instruction.
 * @returns true if supported.
 * @returns false if not supported.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMSupportsFXSR(PVM pVM)
{
    return pVM->cpum.s.CPUFeatures.edx.u1FXSR != 0;
}


/**
 * Checks if the host OS uses the SYSENTER / SYSEXIT instructions.
 * @returns true if used.
 * @returns false if not used.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMIsHostUsingSysEnter(PVM pVM)
{
    return (pVM->cpum.s.fUseFlags & CPUM_USE_SYSENTER) != 0;
}


/**
 * Checks if the host OS uses the SYSCALL / SYSRET instructions.
 * @returns true if used.
 * @returns false if not used.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMIsHostUsingSysCall(PVM pVM)
{
    return (pVM->cpum.s.fUseFlags & CPUM_USE_SYSCALL) != 0;
}


#ifndef IN_RING3
/**
 * Lazily sync in the FPU/XMM state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 */
CPUMDECL(int) CPUMHandleLazyFPU(PVM pVM)
{
    return CPUMHandleLazyFPUAsm(&pVM->cpum.s);
}


/**
 * Restore host FPU/XMM state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 */
CPUMDECL(int) CPUMRestoreHostFPUState(PVM pVM)
{
    Assert(pVM->cpum.s.CPUFeatures.edx.u1FXSR);
    return CPUMRestoreHostFPUStateAsm(&pVM->cpum.s);
}
#endif /* !IN_RING3 */


/**
 * Checks if we activated the FPU/XMM state of the guest OS
 * @returns true if we did.
 * @returns false if not.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMIsGuestFPUStateActive(PVM pVM)
{
    return (pVM->cpum.s.fUseFlags & CPUM_USED_FPU) != 0;
}


/**
 * Deactivate the FPU/XMM state of the guest OS
 * @param   pVM     The VM handle.
 */
CPUMDECL(void) CPUMDeactivateGuestFPUState(PVM pVM)
{
    pVM->cpum.s.fUseFlags &= ~CPUM_USED_FPU;
}


/**
 * Checks if the hidden selector registers are valid
 * @returns true if they are.
 * @returns false if not.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMAreHiddenSelRegsValid(PVM pVM)
{
    return !!pVM->cpum.s.fValidHiddenSelRegs; /** @todo change fValidHiddenSelRegs to bool! */
}


/**
 * Checks if the hidden selector registers are valid
 * @param   pVM     The VM handle.
 * @param   fValid  Valid or not
 */
CPUMDECL(void) CPUMSetHiddenSelRegsValid(PVM pVM, bool fValid)
{
    pVM->cpum.s.fValidHiddenSelRegs = fValid;
}


/**
 * Get the current privilege level of the guest.
 *
 * @returns cpl
 * @param   pVM         VM Handle.
 * @param   pRegFrame   Trap register frame.
 */
CPUMDECL(uint32_t) CPUMGetGuestCPL(PVM pVM, PCPUMCTXCORE pCtxCore)
{
    uint32_t cpl;

    if (CPUMAreHiddenSelRegsValid(pVM))
        cpl = pCtxCore->ssHid.Attr.n.u2Dpl;
    else if (RT_LIKELY(pVM->cpum.s.Guest.cr0 & X86_CR0_PE))
    {
        if (RT_LIKELY(!pCtxCore->eflags.Bits.u1VM))
        {
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
 * @param   pVM         The VM handle.
 */
CPUMDECL(CPUMMODE) CPUMGetGuestMode(PVM pVM)
{
    CPUMMODE enmMode;
    if (!(pVM->cpum.s.Guest.cr0 & X86_CR0_PE))
        enmMode = CPUMMODE_REAL;
    else 
    if (!(pVM->cpum.s.Guest.msrEFER & MSR_K6_EFER_LMA))
        enmMode = CPUMMODE_PROTECTED;
    else
        enmMode = CPUMMODE_LONG;

    return enmMode;
}
