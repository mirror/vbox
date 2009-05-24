/* $Id$ */
/** @file
 * HWACCM SVM - Host Context Ring 0.
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
#define LOG_GROUP LOG_GROUP_HWACCM
#include <VBox/hwaccm.h>
#include "HWACCMInternal.h"
#include <VBox/vm.h>
#include <VBox/x86.h>
#include <VBox/hwacc_svm.h>
#include <VBox/pgm.h>
#include <VBox/pdm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/selm.h>
#include <VBox/iom.h>
#include <VBox/dis.h>
#include <VBox/dbgf.h>
#include <VBox/disopcode.h>
#include <iprt/param.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/cpuset.h>
#include <iprt/mp.h>
#ifdef VBOX_WITH_VMMR0_DISABLE_PREEMPTION
# include <iprt/thread.h>
#endif
#include "HWSVMR0.h"

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int SVMR0InterpretInvpg(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, uint32_t uASID);

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/* IO operation lookup arrays. */
static uint32_t const g_aIOSize[4] = {1, 2, 0, 4};

/**
 * Sets up and activates AMD-V on the current CPU
 *
 * @returns VBox status code.
 * @param   pCpu            CPU info struct
 * @param   pVM             The VM to operate on. (can be NULL after a resume!!)
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
VMMR0DECL(int) SVMR0EnableCpu(PHWACCM_CPUINFO pCpu, PVM pVM, void *pvPageCpu, RTHCPHYS pPageCpuPhys)
{
    AssertReturn(pPageCpuPhys, VERR_INVALID_PARAMETER);
    AssertReturn(pvPageCpu, VERR_INVALID_PARAMETER);

    /* We must turn on AMD-V and setup the host state physical address, as those MSRs are per-cpu/core. */

#if defined(LOG_ENABLED) && !defined(DEBUG_bird)
    SUPR0Printf("SVMR0EnableCpu cpu %d page (%x) %x\n", pCpu->idCpu, pvPageCpu, (uint32_t)pPageCpuPhys);
#endif

    /* Turn on AMD-V in the EFER MSR. */
    uint64_t val = ASMRdMsr(MSR_K6_EFER);
    if (!(val & MSR_K6_EFER_SVME))
        ASMWrMsr(MSR_K6_EFER, val | MSR_K6_EFER_SVME);

    /* Write the physical page address where the CPU will store the host state while executing the VM. */
    ASMWrMsr(MSR_K8_VM_HSAVE_PA, pPageCpuPhys);

    return VINF_SUCCESS;
}

/**
 * Deactivates AMD-V on the current CPU
 *
 * @returns VBox status code.
 * @param   pCpu            CPU info struct
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
VMMR0DECL(int) SVMR0DisableCpu(PHWACCM_CPUINFO pCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys)
{
    AssertReturn(pPageCpuPhys, VERR_INVALID_PARAMETER);
    AssertReturn(pvPageCpu, VERR_INVALID_PARAMETER);

#if defined(LOG_ENABLED) && !defined(DEBUG_bird)
    SUPR0Printf("SVMR0DisableCpu cpu %d\n", pCpu->idCpu);
#endif

    /* Turn off AMD-V in the EFER MSR. */
    uint64_t val = ASMRdMsr(MSR_K6_EFER);
    ASMWrMsr(MSR_K6_EFER, val & ~MSR_K6_EFER_SVME);

    /* Invalidate host state physical address. */
    ASMWrMsr(MSR_K8_VM_HSAVE_PA, 0);

    return VINF_SUCCESS;
}

/**
 * Does Ring-0 per VM AMD-V init.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) SVMR0InitVM(PVM pVM)
{
    int rc;

    pVM->hwaccm.s.svm.pMemObjVMCBHost = NIL_RTR0MEMOBJ;
    pVM->hwaccm.s.svm.pMemObjIOBitmap = NIL_RTR0MEMOBJ;
    pVM->hwaccm.s.svm.pMemObjMSRBitmap = NIL_RTR0MEMOBJ;

    /* Allocate one page for the host context */
    rc = RTR0MemObjAllocCont(&pVM->hwaccm.s.svm.pMemObjVMCBHost, 1 << PAGE_SHIFT, true /* executable R0 mapping */);
    if (RT_FAILURE(rc))
        return rc;

    pVM->hwaccm.s.svm.pVMCBHost     = RTR0MemObjAddress(pVM->hwaccm.s.svm.pMemObjVMCBHost);
    pVM->hwaccm.s.svm.pVMCBHostPhys = RTR0MemObjGetPagePhysAddr(pVM->hwaccm.s.svm.pMemObjVMCBHost, 0);
    ASMMemZeroPage(pVM->hwaccm.s.svm.pVMCBHost);

    /* Allocate 12 KB for the IO bitmap (doesn't seem to be a way to convince SVM not to use it) */
    rc = RTR0MemObjAllocCont(&pVM->hwaccm.s.svm.pMemObjIOBitmap, 3 << PAGE_SHIFT, true /* executable R0 mapping */);
    if (RT_FAILURE(rc))
        return rc;

    pVM->hwaccm.s.svm.pIOBitmap     = RTR0MemObjAddress(pVM->hwaccm.s.svm.pMemObjIOBitmap);
    pVM->hwaccm.s.svm.pIOBitmapPhys = RTR0MemObjGetPagePhysAddr(pVM->hwaccm.s.svm.pMemObjIOBitmap, 0);
    /* Set all bits to intercept all IO accesses. */
    ASMMemFill32(pVM->hwaccm.s.svm.pIOBitmap, PAGE_SIZE*3, 0xffffffff);

    /* Allocate 8 KB for the MSR bitmap (doesn't seem to be a way to convince SVM not to use it) */
    rc = RTR0MemObjAllocCont(&pVM->hwaccm.s.svm.pMemObjMSRBitmap, 2 << PAGE_SHIFT, true /* executable R0 mapping */);
    if (RT_FAILURE(rc))
        return rc;

    pVM->hwaccm.s.svm.pMSRBitmap     = RTR0MemObjAddress(pVM->hwaccm.s.svm.pMemObjMSRBitmap);
    pVM->hwaccm.s.svm.pMSRBitmapPhys = RTR0MemObjGetPagePhysAddr(pVM->hwaccm.s.svm.pMemObjMSRBitmap, 0);
    /* Set all bits to intercept all MSR accesses. */
    ASMMemFill32(pVM->hwaccm.s.svm.pMSRBitmap, PAGE_SIZE*2, 0xffffffff);

    /* Erratum 170 which requires a forced TLB flush for each world switch:
     * See http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/33610.pdf
     *
     * All BH-G1/2 and DH-G1/2 models include a fix:
     * Athlon X2:   0x6b 1/2
     *              0x68 1/2
     * Athlon 64:   0x7f 1
     *              0x6f 2
     * Sempron:     0x7f 1/2
     *              0x6f 2
     *              0x6c 2
     *              0x7c 2
     * Turion 64:   0x68 2
     *
     */
    uint32_t u32Dummy;
    uint32_t u32Version, u32Family, u32Model, u32Stepping, u32BaseFamily;
    ASMCpuId(1, &u32Version, &u32Dummy, &u32Dummy, &u32Dummy);
    u32BaseFamily= (u32Version >> 8) & 0xf;
    u32Family    = u32BaseFamily + (u32BaseFamily == 0xf ? ((u32Version >> 20) & 0x7f) : 0);
    u32Model     = ((u32Version >> 4) & 0xf);
    u32Model     = u32Model | ((u32BaseFamily == 0xf ? (u32Version >> 16) & 0x0f : 0) << 4);
    u32Stepping  = u32Version & 0xf;
    if (    u32Family == 0xf
        &&  !((u32Model == 0x68 || u32Model == 0x6b || u32Model == 0x7f) &&  u32Stepping >= 1)
        &&  !((u32Model == 0x6f || u32Model == 0x6c || u32Model == 0x7c) &&  u32Stepping >= 2))
    {
        Log(("SVMR0InitVM: AMD cpu with erratum 170 family %x model %x stepping %x\n", u32Family, u32Model, u32Stepping));
        pVM->hwaccm.s.svm.fAlwaysFlushTLB = true;
    }

    /* Allocate VMCBs for all guest CPUs. */
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        pVM->aCpus[i].hwaccm.s.svm.pMemObjVMCB = NIL_RTR0MEMOBJ;

        /* Allocate one page for the VM control block (VMCB). */
        rc = RTR0MemObjAllocCont(&pVM->aCpus[i].hwaccm.s.svm.pMemObjVMCB, 1 << PAGE_SHIFT, true /* executable R0 mapping */);
        if (RT_FAILURE(rc))
            return rc;

        pVM->aCpus[i].hwaccm.s.svm.pVMCB     = RTR0MemObjAddress(pVM->aCpus[i].hwaccm.s.svm.pMemObjVMCB);
        pVM->aCpus[i].hwaccm.s.svm.pVMCBPhys = RTR0MemObjGetPagePhysAddr(pVM->aCpus[i].hwaccm.s.svm.pMemObjVMCB, 0);
        ASMMemZeroPage(pVM->aCpus[i].hwaccm.s.svm.pVMCB);
    }

    return VINF_SUCCESS;
}

/**
 * Does Ring-0 per VM AMD-V termination.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) SVMR0TermVM(PVM pVM)
{
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        if (pVM->aCpus[i].hwaccm.s.svm.pMemObjVMCB != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVM->aCpus[i].hwaccm.s.svm.pMemObjVMCB, false);
            pVM->aCpus[i].hwaccm.s.svm.pVMCB       = 0;
            pVM->aCpus[i].hwaccm.s.svm.pVMCBPhys   = 0;
            pVM->aCpus[i].hwaccm.s.svm.pMemObjVMCB = NIL_RTR0MEMOBJ;
        }
    }
    if (pVM->hwaccm.s.svm.pMemObjVMCBHost != NIL_RTR0MEMOBJ)
    {
        RTR0MemObjFree(pVM->hwaccm.s.svm.pMemObjVMCBHost, false);
        pVM->hwaccm.s.svm.pVMCBHost       = 0;
        pVM->hwaccm.s.svm.pVMCBHostPhys   = 0;
        pVM->hwaccm.s.svm.pMemObjVMCBHost = NIL_RTR0MEMOBJ;
    }
    if (pVM->hwaccm.s.svm.pMemObjIOBitmap != NIL_RTR0MEMOBJ)
    {
        RTR0MemObjFree(pVM->hwaccm.s.svm.pMemObjIOBitmap, false);
        pVM->hwaccm.s.svm.pIOBitmap       = 0;
        pVM->hwaccm.s.svm.pIOBitmapPhys   = 0;
        pVM->hwaccm.s.svm.pMemObjIOBitmap = NIL_RTR0MEMOBJ;
    }
    if (pVM->hwaccm.s.svm.pMemObjMSRBitmap != NIL_RTR0MEMOBJ)
    {
        RTR0MemObjFree(pVM->hwaccm.s.svm.pMemObjMSRBitmap, false);
        pVM->hwaccm.s.svm.pMSRBitmap       = 0;
        pVM->hwaccm.s.svm.pMSRBitmapPhys   = 0;
        pVM->hwaccm.s.svm.pMemObjMSRBitmap = NIL_RTR0MEMOBJ;
    }
    return VINF_SUCCESS;
}

/**
 * Sets up AMD-V for the specified VM
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) SVMR0SetupVM(PVM pVM)
{
    int         rc = VINF_SUCCESS;
    SVM_VMCB   *pVMCB;

    AssertReturn(pVM, VERR_INVALID_PARAMETER);

    Assert(pVM->hwaccm.s.svm.fSupported);

    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        pVMCB = (SVM_VMCB *)pVM->aCpus[i].hwaccm.s.svm.pVMCB;
        AssertMsgReturn(pVMCB, ("Invalid pVMCB\n"), VERR_EM_INTERNAL_ERROR);

        /* Program the control fields. Most of them never have to be changed again. */
        /* CR0/3/4 reads must be intercepted, our shadow values are not necessarily the same as the guest's. */
        /* Note: CR0 & CR4 can be safely read when guest and shadow copies are identical. */
        if (!pVM->hwaccm.s.fNestedPaging)
            pVMCB->ctrl.u16InterceptRdCRx = RT_BIT(0) | RT_BIT(3) | RT_BIT(4);
        else
            pVMCB->ctrl.u16InterceptRdCRx = RT_BIT(0) | RT_BIT(4);

        /*
        * CR0/3/4 writes must be intercepted for obvious reasons.
        */
        if (!pVM->hwaccm.s.fNestedPaging)
            pVMCB->ctrl.u16InterceptWrCRx = RT_BIT(0) | RT_BIT(3) | RT_BIT(4);
        else
            pVMCB->ctrl.u16InterceptWrCRx = RT_BIT(0) | RT_BIT(4) | RT_BIT(8);

        /* Intercept all DRx reads and writes by default. Changed later on. */
        pVMCB->ctrl.u16InterceptRdDRx = 0xFFFF;
        pVMCB->ctrl.u16InterceptWrDRx = 0xFFFF;

        /* Currently we don't care about DRx reads or writes. DRx registers are trashed.
        * All breakpoints are automatically cleared when the VM exits.
        */

        pVMCB->ctrl.u32InterceptException = HWACCM_SVM_TRAP_MASK;
#ifndef DEBUG
        if (pVM->hwaccm.s.fNestedPaging)
            pVMCB->ctrl.u32InterceptException &= ~RT_BIT(X86_XCPT_PF);   /* no longer need to intercept #PF. */
#endif

        pVMCB->ctrl.u32InterceptCtrl1 =   SVM_CTRL1_INTERCEPT_INTR
                                        | SVM_CTRL1_INTERCEPT_VINTR
                                        | SVM_CTRL1_INTERCEPT_NMI
                                        | SVM_CTRL1_INTERCEPT_SMI
                                        | SVM_CTRL1_INTERCEPT_INIT
                                        | SVM_CTRL1_INTERCEPT_RDPMC
                                        | SVM_CTRL1_INTERCEPT_CPUID
                                        | SVM_CTRL1_INTERCEPT_RSM
                                        | SVM_CTRL1_INTERCEPT_HLT
                                        | SVM_CTRL1_INTERCEPT_INOUT_BITMAP
                                        | SVM_CTRL1_INTERCEPT_MSR_SHADOW
                                        | SVM_CTRL1_INTERCEPT_INVLPG
                                        | SVM_CTRL1_INTERCEPT_INVLPGA       /* AMD only */
                                        | SVM_CTRL1_INTERCEPT_TASK_SWITCH
                                        | SVM_CTRL1_INTERCEPT_SHUTDOWN      /* fatal */
                                        | SVM_CTRL1_INTERCEPT_FERR_FREEZE;  /* Legacy FPU FERR handling. */
                                        ;
        /* With nested paging we don't care about invlpg anymore. */
        if (pVM->hwaccm.s.fNestedPaging)
            pVMCB->ctrl.u32InterceptCtrl1 &= ~SVM_CTRL1_INTERCEPT_INVLPG;

        pVMCB->ctrl.u32InterceptCtrl2 =   SVM_CTRL2_INTERCEPT_VMRUN         /* required */
                                        | SVM_CTRL2_INTERCEPT_VMMCALL
                                        | SVM_CTRL2_INTERCEPT_VMLOAD
                                        | SVM_CTRL2_INTERCEPT_VMSAVE
                                        | SVM_CTRL2_INTERCEPT_STGI
                                        | SVM_CTRL2_INTERCEPT_CLGI
                                        | SVM_CTRL2_INTERCEPT_SKINIT
                                        | SVM_CTRL2_INTERCEPT_WBINVD
                                        | SVM_CTRL2_INTERCEPT_MWAIT_UNCOND; /* don't execute mwait or else we'll idle inside the guest (host thinks the cpu load is high) */
                                        ;
        Log(("pVMCB->ctrl.u32InterceptException = %x\n", pVMCB->ctrl.u32InterceptException));
        Log(("pVMCB->ctrl.u32InterceptCtrl1 = %x\n", pVMCB->ctrl.u32InterceptCtrl1));
        Log(("pVMCB->ctrl.u32InterceptCtrl2 = %x\n", pVMCB->ctrl.u32InterceptCtrl2));

        /* Virtualize masking of INTR interrupts. (reads/writes from/to CR8 go to the V_TPR register) */
        pVMCB->ctrl.IntCtrl.n.u1VIrqMasking = 1;
        /* Ignore the priority in the TPR; just deliver it when we tell it to. */
        pVMCB->ctrl.IntCtrl.n.u1IgnoreTPR   = 1;

        /* Set IO and MSR bitmap addresses. */
        pVMCB->ctrl.u64IOPMPhysAddr  = pVM->hwaccm.s.svm.pIOBitmapPhys;
        pVMCB->ctrl.u64MSRPMPhysAddr = pVM->hwaccm.s.svm.pMSRBitmapPhys;

        /* No LBR virtualization. */
        pVMCB->ctrl.u64LBRVirt      = 0;

        /** The ASID must start at 1; the host uses 0. */
        pVMCB->ctrl.TLBCtrl.n.u32ASID = 1;

        /** Setup the PAT msr (nested paging only) */
        pVMCB->guest.u64GPAT = 0x0007040600070406ULL;
    }
    return rc;
}


/**
 * Injects an event (trap or external interrupt)
 *
 * @param   pVM         The VM to operate on.
 * @param   pVMCB       SVM control block
 * @param   pCtx        CPU Context
 * @param   pIntInfo    SVM interrupt info
 */
inline void SVMR0InjectEvent(PVM pVM, SVM_VMCB *pVMCB, CPUMCTX *pCtx, SVM_EVENT* pEvent)
{
#ifdef VBOX_STRICT
    if (pEvent->n.u8Vector == 0xE)
        Log(("SVM: Inject int %d at %RGv error code=%02x CR2=%RGv intInfo=%08x\n", pEvent->n.u8Vector, (RTGCPTR)pCtx->rip, pEvent->n.u32ErrorCode, (RTGCPTR)pCtx->cr2, pEvent->au64[0]));
    else
    if (pEvent->n.u8Vector < 0x20)
        Log(("SVM: Inject int %d at %RGv error code=%08x\n", pEvent->n.u8Vector, (RTGCPTR)pCtx->rip, pEvent->n.u32ErrorCode));
    else
    {
        Log(("INJ-EI: %x at %RGv\n", pEvent->n.u8Vector, (RTGCPTR)pCtx->rip));
        Assert(!VMCPU_FF_ISSET(VMMGetCpu(pVM), VMCPU_FF_INHIBIT_INTERRUPTS));
        Assert(pCtx->eflags.u32 & X86_EFL_IF);
    }
#endif

    /* Set event injection state. */
    pVMCB->ctrl.EventInject.au64[0] = pEvent->au64[0];
}


/**
 * Checks for pending guest interrupts and injects them
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   pVMCB       SVM control block
 * @param   pCtx        CPU Context
 */
static int SVMR0CheckPendingInterrupt(PVM pVM, PVMCPU pVCpu, SVM_VMCB *pVMCB, CPUMCTX *pCtx)
{
    int rc;

    /* Dispatch any pending interrupts. (injected before, but a VM exit occurred prematurely) */
    if (pVCpu->hwaccm.s.Event.fPending)
    {
        SVM_EVENT Event;

        Log(("Reinjecting event %08x %08x at %RGv\n", pVCpu->hwaccm.s.Event.intInfo, pVCpu->hwaccm.s.Event.errCode, (RTGCPTR)pCtx->rip));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatIntReinject);
        Event.au64[0] = pVCpu->hwaccm.s.Event.intInfo;
        SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

        pVCpu->hwaccm.s.Event.fPending = false;
        return VINF_SUCCESS;
    }

    if (pVM->hwaccm.s.fInjectNMI)
    {
        SVM_EVENT Event;

        Event.n.u8Vector     = X86_XCPT_NMI;
        Event.n.u1Valid      = 1;
        Event.n.u32ErrorCode = 0;
        Event.n.u3Type       = SVM_EVENT_NMI;

        SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);
        pVM->hwaccm.s.fInjectNMI = false;
        return VINF_SUCCESS;
    }

    /* When external interrupts are pending, we should exit the VM when IF is set. */
    if (    !TRPMHasTrap(pVCpu)
        &&  VMCPU_FF_ISPENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC|VMCPU_FF_INTERRUPT_PIC)))
    {
        if (    !(pCtx->eflags.u32 & X86_EFL_IF)
            ||  VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
        {
            if (!pVMCB->ctrl.IntCtrl.n.u1VIrqValid)
            {
                if (!VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
                    LogFlow(("Enable irq window exit!\n"));
                else
                    Log(("Pending interrupt blocked at %RGv by VM_FF_INHIBIT_INTERRUPTS -> irq window exit\n", (RTGCPTR)pCtx->rip));

                /** @todo use virtual interrupt method to inject a pending irq; dispatched as soon as guest.IF is set. */
                pVMCB->ctrl.u32InterceptCtrl1 |= SVM_CTRL1_INTERCEPT_VINTR;
                pVMCB->ctrl.IntCtrl.n.u1VIrqValid    = 1;
                pVMCB->ctrl.IntCtrl.n.u8VIrqVector   = 0; /* don't care */
            }
        }
        else
        {
            uint8_t u8Interrupt;

            rc = PDMGetInterrupt(pVCpu, &u8Interrupt);
            Log(("Dispatch interrupt: u8Interrupt=%x (%d) rc=%Rrc\n", u8Interrupt, u8Interrupt, rc));
            if (RT_SUCCESS(rc))
            {
                rc = TRPMAssertTrap(pVCpu, u8Interrupt, TRPM_HARDWARE_INT);
                AssertRC(rc);
            }
            else
            {
                /* Can only happen in rare cases where a pending interrupt is cleared behind our back */
                Assert(!VMCPU_FF_ISPENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC|VMCPU_FF_INTERRUPT_PIC)));
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatSwitchGuestIrq);
                /* Just continue */
            }
        }
    }

#ifdef VBOX_STRICT
    if (TRPMHasTrap(pVCpu))
    {
        uint8_t     u8Vector;
        rc = TRPMQueryTrapAll(pVCpu, &u8Vector, 0, 0, 0);
        AssertRC(rc);
    }
#endif

    if (   (pCtx->eflags.u32 & X86_EFL_IF)
        && (!VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
        && TRPMHasTrap(pVCpu)
       )
    {
        uint8_t     u8Vector;
        int         rc;
        TRPMEVENT   enmType;
        SVM_EVENT   Event;
        RTGCUINT    u32ErrorCode;

        Event.au64[0] = 0;

        /* If a new event is pending, then dispatch it now. */
        rc = TRPMQueryTrapAll(pVCpu, &u8Vector, &enmType, &u32ErrorCode, 0);
        AssertRC(rc);
        Assert(pCtx->eflags.Bits.u1IF == 1 || enmType == TRPM_TRAP);
        Assert(enmType != TRPM_SOFTWARE_INT);

        /* Clear the pending trap. */
        rc = TRPMResetTrap(pVCpu);
        AssertRC(rc);

        Event.n.u8Vector = u8Vector;
        Event.n.u1Valid  = 1;
        Event.n.u32ErrorCode = u32ErrorCode;

        if (enmType == TRPM_TRAP)
        {
            switch (u8Vector) {
            case 8:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 17:
                /* Valid error codes. */
                Event.n.u1ErrorCodeValid = 1;
                break;
            default:
                break;
            }
            if (u8Vector == X86_XCPT_NMI)
                Event.n.u3Type = SVM_EVENT_NMI;
            else
                Event.n.u3Type = SVM_EVENT_EXCEPTION;
        }
        else
            Event.n.u3Type = SVM_EVENT_EXTERNAL_IRQ;

        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatIntInject);
        SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);
    } /* if (interrupts can be dispatched) */

    return VINF_SUCCESS;
}

/**
 * Save the host state
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 */
VMMR0DECL(int) SVMR0SaveHostState(PVM pVM, PVMCPU pVCpu)
{
    NOREF(pVM);
    NOREF(pVCpu);
    /* Nothing to do here. */
    return VINF_SUCCESS;
}

/**
 * Loads the guest state
 *
 * NOTE: Don't do anything here that can cause a jump back to ring 3!!!!!
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   pCtx        Guest context
 */
VMMR0DECL(int) SVMR0LoadGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    RTGCUINTPTR val;
    SVM_VMCB *pVMCB;

    if (pVM == NULL)
        return VERR_INVALID_PARAMETER;

    /* Setup AMD SVM. */
    Assert(pVM->hwaccm.s.svm.fSupported);

    pVMCB = (SVM_VMCB *)pVCpu->hwaccm.s.svm.pVMCB;
    AssertMsgReturn(pVMCB, ("Invalid pVMCB\n"), VERR_EM_INTERNAL_ERROR);

    /* Guest CPU context: ES, CS, SS, DS, FS, GS. */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_SEGMENT_REGS)
    {
        SVM_WRITE_SELREG(CS, cs);
        SVM_WRITE_SELREG(SS, ss);
        SVM_WRITE_SELREG(DS, ds);
        SVM_WRITE_SELREG(ES, es);
        SVM_WRITE_SELREG(FS, fs);
        SVM_WRITE_SELREG(GS, gs);
    }

    /* Guest CPU context: LDTR. */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_LDTR)
    {
        SVM_WRITE_SELREG(LDTR, ldtr);
    }

    /* Guest CPU context: TR. */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_TR)
    {
        SVM_WRITE_SELREG(TR, tr);
    }

    /* Guest CPU context: GDTR. */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_GDTR)
    {
        pVMCB->guest.GDTR.u32Limit = pCtx->gdtr.cbGdt;
        pVMCB->guest.GDTR.u64Base  = pCtx->gdtr.pGdt;
    }

    /* Guest CPU context: IDTR. */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_IDTR)
    {
        pVMCB->guest.IDTR.u32Limit = pCtx->idtr.cbIdt;
        pVMCB->guest.IDTR.u64Base  = pCtx->idtr.pIdt;
    }

    /*
     * Sysenter MSRs (unconditional)
     */
    pVMCB->guest.u64SysEnterCS  = pCtx->SysEnter.cs;
    pVMCB->guest.u64SysEnterEIP = pCtx->SysEnter.eip;
    pVMCB->guest.u64SysEnterESP = pCtx->SysEnter.esp;

    /* Control registers */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_CR0)
    {
        val = pCtx->cr0;
        if (!CPUMIsGuestFPUStateActive(pVCpu))
        {
            /* Always use #NM exceptions to load the FPU/XMM state on demand. */
            val |= X86_CR0_TS | X86_CR0_ET | X86_CR0_NE | X86_CR0_MP;
        }
        else
        {
            /** @todo check if we support the old style mess correctly. */
            if (!(val & X86_CR0_NE))
            {
                Log(("Forcing X86_CR0_NE!!!\n"));

                /* Also catch floating point exceptions as we need to report them to the guest in a different way. */
                if (!pVCpu->hwaccm.s.fFPUOldStyleOverride)
                {
                    pVMCB->ctrl.u32InterceptException |= RT_BIT(X86_XCPT_MF);
                    pVCpu->hwaccm.s.fFPUOldStyleOverride = true;
                }
            }
            val |= X86_CR0_NE;  /* always turn on the native mechanism to report FPU errors (old style uses interrupts) */
        }
        /* Always enable caching. */
        val &= ~(X86_CR0_CD|X86_CR0_NW);

        /* Note: WP is not relevant in nested paging mode as we catch accesses on the (guest) physical level. */
        /* Note: In nested paging mode the guest is allowed to run with paging disabled; the guest physical to host physical translation will remain active. */
        if (!pVM->hwaccm.s.fNestedPaging)
        {
            val |= X86_CR0_PG;          /* Paging is always enabled; even when the guest is running in real mode or PE without paging. */
            val |= X86_CR0_WP;          /* Must set this as we rely on protect various pages and supervisor writes must be caught. */
        }
        pVMCB->guest.u64CR0 = val;
    }
    /* CR2 as well */
    pVMCB->guest.u64CR2 = pCtx->cr2;

    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_CR3)
    {
        /* Save our shadow CR3 register. */
        if (pVM->hwaccm.s.fNestedPaging)
        {
            PGMMODE enmShwPagingMode;

#if HC_ARCH_BITS == 32
            if (CPUMIsGuestInLongModeEx(pCtx))
                enmShwPagingMode = PGMMODE_AMD64_NX;
            else
#endif
                enmShwPagingMode = PGMGetHostMode(pVM);

            pVMCB->ctrl.u64NestedPagingCR3  = PGMGetNestedCR3(pVCpu, enmShwPagingMode);
            Assert(pVMCB->ctrl.u64NestedPagingCR3);
            pVMCB->guest.u64CR3             = pCtx->cr3;
        }
        else
        {
            pVMCB->guest.u64CR3             = PGMGetHyperCR3(pVCpu);
            Assert(pVMCB->guest.u64CR3 || VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL));
        }
    }

    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_CR4)
    {
        val = pCtx->cr4;
        if (!pVM->hwaccm.s.fNestedPaging)
        {
            switch(pVCpu->hwaccm.s.enmShadowMode)
            {
            case PGMMODE_REAL:
            case PGMMODE_PROTECTED:     /* Protected mode, no paging. */
                AssertFailed();
                return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;

            case PGMMODE_32_BIT:        /* 32-bit paging. */
                val &= ~X86_CR4_PAE;
                break;

            case PGMMODE_PAE:           /* PAE paging. */
            case PGMMODE_PAE_NX:        /* PAE paging with NX enabled. */
                /** @todo use normal 32 bits paging */
                val |= X86_CR4_PAE;
                break;

            case PGMMODE_AMD64:         /* 64-bit AMD paging (long mode). */
            case PGMMODE_AMD64_NX:      /* 64-bit AMD paging (long mode) with NX enabled. */
#ifdef VBOX_ENABLE_64_BITS_GUESTS
                break;
#else
                AssertFailed();
                return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
#endif

            default:                    /* shut up gcc */
                AssertFailed();
                return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
            }
        }
        pVMCB->guest.u64CR4 = val;
    }

    /* Debug registers. */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_DEBUG)
    {
        pCtx->dr[6] |= X86_DR6_INIT_VAL;                                          /* set all reserved bits to 1. */
        pCtx->dr[6] &= ~RT_BIT(12);                                               /* must be zero. */

        pCtx->dr[7] &= 0xffffffff;                                                /* upper 32 bits reserved */
        pCtx->dr[7] &= ~(RT_BIT(11) | RT_BIT(12) | RT_BIT(14) | RT_BIT(15));      /* must be zero */
        pCtx->dr[7] |= 0x400;                                                     /* must be one */

        pVMCB->guest.u64DR7 = pCtx->dr[7];
        pVMCB->guest.u64DR6 = pCtx->dr[6];

        /* Sync the debug state now if any breakpoint is armed. */
        if (    (pCtx->dr[7] & (X86_DR7_ENABLED_MASK|X86_DR7_GD))
            &&  !CPUMIsGuestDebugStateActive(pVCpu)
            &&  !DBGFIsStepping(pVCpu))
        {
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatDRxArmed);

            /* Disable drx move intercepts. */
            pVMCB->ctrl.u16InterceptRdDRx = 0;
            pVMCB->ctrl.u16InterceptWrDRx = 0;

            /* Save the host and load the guest debug state. */
            int rc = CPUMR0LoadGuestDebugState(pVM, pVCpu, pCtx, false /* exclude DR6 */);
            AssertRC(rc);
        }
    }

    /* EIP, ESP and EFLAGS */
    pVMCB->guest.u64RIP    = pCtx->rip;
    pVMCB->guest.u64RSP    = pCtx->rsp;
    pVMCB->guest.u64RFlags = pCtx->eflags.u32;

    /* Set CPL */
    pVMCB->guest.u8CPL     = pCtx->csHid.Attr.n.u2Dpl;

    /* RAX/EAX too, as VMRUN uses RAX as an implicit parameter. */
    pVMCB->guest.u64RAX    = pCtx->rax;

    /* vmrun will fail without MSR_K6_EFER_SVME. */
    pVMCB->guest.u64EFER   = pCtx->msrEFER | MSR_K6_EFER_SVME;

    /* 64 bits guest mode? */
    if (pCtx->msrEFER & MSR_K6_EFER_LMA)
    {
#if !defined(VBOX_ENABLE_64_BITS_GUESTS)
        return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
#elif HC_ARCH_BITS == 32 && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        pVCpu->hwaccm.s.svm.pfnVMRun = SVMR0VMSwitcherRun64;
#else
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        if (!pVM->hwaccm.s.fAllow64BitGuests)
            return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
# endif
        pVCpu->hwaccm.s.svm.pfnVMRun = SVMR0VMRun64;
#endif
        /* Unconditionally update these as wrmsr might have changed them. (HWACCM_CHANGED_GUEST_SEGMENT_REGS will not be set) */
        pVMCB->guest.FS.u64Base    = pCtx->fsHid.u64Base;
        pVMCB->guest.GS.u64Base    = pCtx->gsHid.u64Base;
    }
    else
    {
        /* Filter out the MSR_K6_LME bit or else AMD-V expects amd64 shadow paging. */
        pVMCB->guest.u64EFER &= ~MSR_K6_EFER_LME;

        pVCpu->hwaccm.s.svm.pfnVMRun = SVMR0VMRun;
    }

    /* TSC offset. */
    if (TMCpuTickCanUseRealTSC(pVCpu, &pVMCB->ctrl.u64TSCOffset))
    {
        pVMCB->ctrl.u32InterceptCtrl1 &= ~SVM_CTRL1_INTERCEPT_RDTSC;
        pVMCB->ctrl.u32InterceptCtrl2 &= ~SVM_CTRL2_INTERCEPT_RDTSCP;
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTSCOffset);
    }
    else
    {
        pVMCB->ctrl.u32InterceptCtrl1 |= SVM_CTRL1_INTERCEPT_RDTSC;
        pVMCB->ctrl.u32InterceptCtrl2 |= SVM_CTRL2_INTERCEPT_RDTSCP;
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTSCIntercept);
    }

    /* Sync the various msrs for 64 bits mode. */
    pVMCB->guest.u64STAR            = pCtx->msrSTAR;            /* legacy syscall eip, cs & ss */
    pVMCB->guest.u64LSTAR           = pCtx->msrLSTAR;           /* 64 bits mode syscall rip */
    pVMCB->guest.u64CSTAR           = pCtx->msrCSTAR;           /* compatibility mode syscall rip */
    pVMCB->guest.u64SFMASK          = pCtx->msrSFMASK;          /* syscall flag mask */
    pVMCB->guest.u64KernelGSBase    = pCtx->msrKERNELGSBASE;    /* swapgs exchange value */

#ifdef DEBUG
    /* Intercept X86_XCPT_DB if stepping is enabled */
    if (DBGFIsStepping(pVCpu))
        pVMCB->ctrl.u32InterceptException |=  RT_BIT(X86_XCPT_DB);
    else
        pVMCB->ctrl.u32InterceptException &= ~RT_BIT(X86_XCPT_DB);
#endif

    /* Done. */
    pVCpu->hwaccm.s.fContextUseFlags &= ~HWACCM_CHANGED_ALL_GUEST;

    return VINF_SUCCESS;
}


/**
 * Runs guest code in an AMD-V VM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   pCtx        Guest context
 */
VMMR0DECL(int) SVMR0RunGuestCode(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    int         rc = VINF_SUCCESS;
    uint64_t    exitCode = (uint64_t)SVM_EXIT_INVALID;
    SVM_VMCB   *pVMCB;
    bool        fSyncTPR = false;
    unsigned    cResume = 0;
    uint8_t     u8LastVTPR;
    PHWACCM_CPUINFO pCpu = 0;
    RTCCUINTREG uOldEFlags;
#ifdef VBOX_STRICT
    RTCPUID  idCpuCheck;
#endif

    STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatEntry, x);

    pVMCB = (SVM_VMCB *)pVCpu->hwaccm.s.svm.pVMCB;
    AssertMsgReturn(pVMCB, ("Invalid pVMCB\n"), VERR_EM_INTERNAL_ERROR);

    /* We can jump to this point to resume execution after determining that a VM-exit is innocent.
     */
ResumeExecution:
    Assert(!HWACCMR0SuspendPending());

    /* Safety precaution; looping for too long here can have a very bad effect on the host */
    if (++cResume > HWACCM_MAX_RESUME_LOOPS)
    {
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitMaxResume);
        rc = VINF_EM_RAW_INTERRUPT;
        goto end;
    }

    /* Check for irq inhibition due to instruction fusing (sti, mov ss). */
    if (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
    {
        Log(("VM_FF_INHIBIT_INTERRUPTS at %RGv successor %RGv\n", (RTGCPTR)pCtx->rip, EMGetInhibitInterruptsPC(pVCpu)));
        if (pCtx->rip != EMGetInhibitInterruptsPC(pVCpu))
        {
            /* Note: we intentionally don't clear VM_FF_INHIBIT_INTERRUPTS here.
             * Before we are able to execute this instruction in raw mode (iret to guest code) an external interrupt might
             * force a world switch again. Possibly allowing a guest interrupt to be dispatched in the process. This could
             * break the guest. Sounds very unlikely, but such timing sensitive problems are not as rare as you might think.
             */
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
            /* Irq inhibition is no longer active; clear the corresponding SVM state. */
            pVMCB->ctrl.u64IntShadow = 0;
        }
    }
    else
    {
        /* Irq inhibition is no longer active; clear the corresponding SVM state. */
        pVMCB->ctrl.u64IntShadow = 0;
    }

    /* Check for pending actions that force us to go back to ring 3. */
#ifdef DEBUG
    /* Intercept X86_XCPT_DB if stepping is enabled */
    if (!DBGFIsStepping(pVCpu))
#endif
    {
        if (    VM_FF_ISPENDING(pVM, VM_FF_HWACCM_TO_R3_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_HWACCM_TO_R3_MASK))
        {
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TO_R3);
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatSwitchToR3);
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatEntry, x);
            rc = RT_UNLIKELY(VM_FF_ISPENDING(pVM, VM_FF_PGM_NO_MEMORY)) ? VINF_EM_NO_MEMORY : VINF_EM_RAW_TO_R3;
            goto end;
        }
    }

    /* Pending request packets might contain actions that need immediate attention, such as pending hardware interrupts. */
    if (    VM_FF_ISPENDING(pVM, VM_FF_REQUEST)
        ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_REQUEST))
    {
        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatEntry, x);
        rc = VINF_EM_PENDING_REQUEST;
        goto end;
    }

#ifdef VBOX_WITH_VMMR0_DISABLE_PREEMPTION
    /** @todo This must be repeated (or moved down) after we've disabled interrupts
     *        below because a rescheduling request (IPI) might arrive before we get
     *        there and we end up exceeding our timeslice. (Putting it here for
     *        now because I don't want to mess up anything.) */
    if (RTThreadPreemptIsPending(NIL_RTTHREAD))
    {
        rc = VINF_EM_RAW_INTERRUPT_HYPER;
        goto end;
    }
#endif

    /* When external interrupts are pending, we should exit the VM when IF is set. */
    /* Note! *After* VM_FF_INHIBIT_INTERRUPTS check!!! */
    rc = SVMR0CheckPendingInterrupt(pVM, pVCpu, pVMCB, pCtx);
    if (RT_FAILURE(rc))
    {
        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatEntry, x);
        goto end;
    }

    /* TPR caching using CR8 is only available in 64 bits mode */
    /* Note the 32 bits exception for AMD (X86_CPUID_AMD_FEATURE_ECX_CR8L), but that appears missing in Intel CPUs */
    /* Note: we can't do this in LoadGuestState as PDMApicGetTPR can jump back to ring 3 (lock)!!!!!!!! */
    if (pCtx->msrEFER & MSR_K6_EFER_LMA)
    {
        bool fPending;

        /* TPR caching in CR8 */
        int rc = PDMApicGetTPR(pVM, &u8LastVTPR, &fPending);
        AssertRC(rc);
        pVMCB->ctrl.IntCtrl.n.u8VTPR = u8LastVTPR;

        if (fPending)
        {
            /* A TPR change could activate a pending interrupt, so catch cr8 writes. */
            pVMCB->ctrl.u16InterceptWrCRx |= RT_BIT(8);
        }
        else
            /* No interrupts are pending, so we don't need to be explicitely notified.
             * There are enough world switches for detecting pending interrupts.
             */
            pVMCB->ctrl.u16InterceptWrCRx &= ~RT_BIT(8);

        fSyncTPR = !fPending;
    }

    /* All done! Let's start VM execution. */
    STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatInGC, x);

    /* Enable nested paging if necessary (disabled each time after #VMEXIT). */
    pVMCB->ctrl.NestedPaging.n.u1NestedPaging = pVM->hwaccm.s.fNestedPaging;

#ifdef LOG_ENABLED
    pCpu = HWACCMR0GetCurrentCpu();
    if (    pVCpu->hwaccm.s.idLastCpu   != pCpu->idCpu
        ||  pVCpu->hwaccm.s.cTLBFlushes != pCpu->cTLBFlushes)
    {
        if (pVCpu->hwaccm.s.idLastCpu != pCpu->idCpu)
            Log(("Force TLB flush due to rescheduling to a different cpu (%d vs %d)\n", pVCpu->hwaccm.s.idLastCpu, pCpu->idCpu));
        else
            Log(("Force TLB flush due to changed TLB flush count (%x vs %x)\n", pVCpu->hwaccm.s.cTLBFlushes, pCpu->cTLBFlushes));
    }
    if (pCpu->fFlushTLB)
        Log(("Force TLB flush: first time cpu %d is used -> flush\n", pCpu->idCpu));
#endif

    /*
     * NOTE: DO NOT DO ANYTHING AFTER THIS POINT THAT MIGHT JUMP BACK TO RING 3!
     *       (until the actual world switch)
     */
#ifdef VBOX_STRICT
    idCpuCheck = RTMpCpuId();
#endif
#ifdef LOG_LOGGING
    VMMR0LogFlushDisable(pVCpu);
#endif

    /* Load the guest state; *must* be here as it sets up the shadow cr0 for lazy fpu syncing! */
    rc = SVMR0LoadGuestState(pVM, pVCpu, pCtx);
    if (rc != VINF_SUCCESS)
    {
        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatEntry, x);
        goto end;
    }

    /* Disable interrupts to make sure a poke will interrupt execution.
     * This must be done *before* we check for TLB flushes; TLB shootdowns rely on this.
     */
    uOldEFlags = ASMIntDisableFlags();
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC);

    pCpu = HWACCMR0GetCurrentCpu();
    /* Force a TLB flush for the first world switch if the current cpu differs from the one we ran on last. */
    /* Note that this can happen both for start and resume due to long jumps back to ring 3. */
    if (    pVCpu->hwaccm.s.idLastCpu != pCpu->idCpu
            /* if the tlb flush count has changed, another VM has flushed the TLB of this cpu, so we can't use our current ASID anymore. */
        ||  pVCpu->hwaccm.s.cTLBFlushes != pCpu->cTLBFlushes)
    {
        /* Force a TLB flush on VM entry. */
        pVCpu->hwaccm.s.fForceTLBFlush = true;
    }
    else
        Assert(!pCpu->fFlushTLB || pVM->hwaccm.s.svm.fAlwaysFlushTLB);

    pVCpu->hwaccm.s.idLastCpu = pCpu->idCpu;

    /* Check for tlb shootdown flushes. */
    if (VMCPU_FF_TESTANDCLEAR(pVCpu, VMCPU_FF_TLB_FLUSH_BIT))
        pVCpu->hwaccm.s.fForceTLBFlush = true;

    /* Make sure we flush the TLB when required. Switch ASID to achieve the same thing, but without actually flushing the whole TLB (which is expensive). */
    if (    pVCpu->hwaccm.s.fForceTLBFlush
        && !pVM->hwaccm.s.svm.fAlwaysFlushTLB)
    {
        if (    ++pCpu->uCurrentASID >= pVM->hwaccm.s.uMaxASID
            ||  pCpu->fFlushTLB)
        {
            pCpu->fFlushTLB                  = false;
            pCpu->uCurrentASID               = 1;       /* start at 1; host uses 0 */
            pVMCB->ctrl.TLBCtrl.n.u1TLBFlush = 1;       /* wrap around; flush TLB */
            pCpu->cTLBFlushes++;
        }
        else
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushASID);

        pVCpu->hwaccm.s.cTLBFlushes  = pCpu->cTLBFlushes;
        pVCpu->hwaccm.s.uCurrentASID = pCpu->uCurrentASID;
    }
    else
    {
        Assert(!pCpu->fFlushTLB || pVM->hwaccm.s.svm.fAlwaysFlushTLB);

        /* We never increase uCurrentASID in the fAlwaysFlushTLB (erratum 170) case. */
        if (!pCpu->uCurrentASID || !pVCpu->hwaccm.s.uCurrentASID)
            pVCpu->hwaccm.s.uCurrentASID = pCpu->uCurrentASID = 1;

        Assert(!pVM->hwaccm.s.svm.fAlwaysFlushTLB || pVCpu->hwaccm.s.fForceTLBFlush);
        pVMCB->ctrl.TLBCtrl.n.u1TLBFlush = pVCpu->hwaccm.s.fForceTLBFlush;

        if (    !pVM->hwaccm.s.svm.fAlwaysFlushTLB
            &&  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_TLB_SHOOTDOWN))
        {
            /* Deal with pending TLB shootdown actions which were queued when we were not executing code. */
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTlbShootdown);
            for (unsigned i=0;i<pVCpu->hwaccm.s.TlbShootdown.cPages;i++)
                SVMR0InvlpgA(pVCpu->hwaccm.s.TlbShootdown.aPages[i], pVMCB->ctrl.TLBCtrl.n.u32ASID);
        }
    }
    pVCpu->hwaccm.s.TlbShootdown.cPages = 0;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);

    AssertMsg(pVCpu->hwaccm.s.cTLBFlushes == pCpu->cTLBFlushes, ("Flush count mismatch for cpu %d (%x vs %x)\n", pCpu->idCpu, pVCpu->hwaccm.s.cTLBFlushes, pCpu->cTLBFlushes));
    AssertMsg(pCpu->uCurrentASID >= 1 && pCpu->uCurrentASID < pVM->hwaccm.s.uMaxASID, ("cpu%d uCurrentASID = %x\n", pCpu->idCpu, pCpu->uCurrentASID));
    AssertMsg(pVCpu->hwaccm.s.uCurrentASID >= 1 && pVCpu->hwaccm.s.uCurrentASID < pVM->hwaccm.s.uMaxASID, ("cpu%d VM uCurrentASID = %x\n", pCpu->idCpu, pVCpu->hwaccm.s.uCurrentASID));
    pVMCB->ctrl.TLBCtrl.n.u32ASID = pVCpu->hwaccm.s.uCurrentASID;

#ifdef VBOX_WITH_STATISTICS
    if (pVMCB->ctrl.TLBCtrl.n.u1TLBFlush)
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushTLBWorldSwitch);
    else
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatNoFlushTLBWorldSwitch);
#endif

    /* In case we execute a goto ResumeExecution later on. */
    pVCpu->hwaccm.s.fResumeVM      = true;
    pVCpu->hwaccm.s.fForceTLBFlush = pVM->hwaccm.s.svm.fAlwaysFlushTLB;

    Assert(sizeof(pVCpu->hwaccm.s.svm.pVMCBPhys) == 8);
    Assert(pVMCB->ctrl.IntCtrl.n.u1VIrqMasking);
    Assert(pVMCB->ctrl.u64IOPMPhysAddr  == pVM->hwaccm.s.svm.pIOBitmapPhys);
    Assert(pVMCB->ctrl.u64MSRPMPhysAddr == pVM->hwaccm.s.svm.pMSRBitmapPhys);
    Assert(pVMCB->ctrl.u64LBRVirt == 0);

#ifdef VBOX_STRICT
    Assert(idCpuCheck == RTMpCpuId());
#endif
    TMNotifyStartOfExecution(pVCpu);
    pVCpu->hwaccm.s.svm.pfnVMRun(pVM->hwaccm.s.svm.pVMCBHostPhys, pVCpu->hwaccm.s.svm.pVMCBPhys, pCtx, pVM, pVCpu);
    TMNotifyEndOfExecution(pVCpu);
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);
    ASMSetFlags(uOldEFlags);
    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatInGC, x);

    /*
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * IMPORTANT: WE CAN'T DO ANY LOGGING OR OPERATIONS THAT CAN DO A LONGJMP BACK TO RING 3 *BEFORE* WE'VE SYNCED BACK (MOST OF) THE GUEST STATE
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */

    STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatExit1, x);

    /* Reason for the VM exit */
    exitCode = pVMCB->ctrl.u64ExitCode;

    if (exitCode == (uint64_t)SVM_EXIT_INVALID)          /* Invalid guest state. */
    {
        HWACCMDumpRegs(pVM, pVCpu, pCtx);
#ifdef DEBUG
        Log(("ctrl.u16InterceptRdCRx            %x\n",      pVMCB->ctrl.u16InterceptRdCRx));
        Log(("ctrl.u16InterceptWrCRx            %x\n",      pVMCB->ctrl.u16InterceptWrCRx));
        Log(("ctrl.u16InterceptRdDRx            %x\n",      pVMCB->ctrl.u16InterceptRdDRx));
        Log(("ctrl.u16InterceptWrDRx            %x\n",      pVMCB->ctrl.u16InterceptWrDRx));
        Log(("ctrl.u32InterceptException        %x\n",      pVMCB->ctrl.u32InterceptException));
        Log(("ctrl.u32InterceptCtrl1            %x\n",      pVMCB->ctrl.u32InterceptCtrl1));
        Log(("ctrl.u32InterceptCtrl2            %x\n",      pVMCB->ctrl.u32InterceptCtrl2));
        Log(("ctrl.u64IOPMPhysAddr              %RX64\n",   pVMCB->ctrl.u64IOPMPhysAddr));
        Log(("ctrl.u64MSRPMPhysAddr             %RX64\n",   pVMCB->ctrl.u64MSRPMPhysAddr));
        Log(("ctrl.u64TSCOffset                 %RX64\n",   pVMCB->ctrl.u64TSCOffset));

        Log(("ctrl.TLBCtrl.u32ASID              %x\n",      pVMCB->ctrl.TLBCtrl.n.u32ASID));
        Log(("ctrl.TLBCtrl.u1TLBFlush           %x\n",      pVMCB->ctrl.TLBCtrl.n.u1TLBFlush));
        Log(("ctrl.TLBCtrl.u7Reserved           %x\n",      pVMCB->ctrl.TLBCtrl.n.u7Reserved));
        Log(("ctrl.TLBCtrl.u24Reserved          %x\n",      pVMCB->ctrl.TLBCtrl.n.u24Reserved));

        Log(("ctrl.IntCtrl.u8VTPR               %x\n",      pVMCB->ctrl.IntCtrl.n.u8VTPR));
        Log(("ctrl.IntCtrl.u1VIrqValid          %x\n",      pVMCB->ctrl.IntCtrl.n.u1VIrqValid));
        Log(("ctrl.IntCtrl.u7Reserved           %x\n",      pVMCB->ctrl.IntCtrl.n.u7Reserved));
        Log(("ctrl.IntCtrl.u4VIrqPriority       %x\n",      pVMCB->ctrl.IntCtrl.n.u4VIrqPriority));
        Log(("ctrl.IntCtrl.u1IgnoreTPR          %x\n",      pVMCB->ctrl.IntCtrl.n.u1IgnoreTPR));
        Log(("ctrl.IntCtrl.u3Reserved           %x\n",      pVMCB->ctrl.IntCtrl.n.u3Reserved));
        Log(("ctrl.IntCtrl.u1VIrqMasking        %x\n",      pVMCB->ctrl.IntCtrl.n.u1VIrqMasking));
        Log(("ctrl.IntCtrl.u7Reserved2          %x\n",      pVMCB->ctrl.IntCtrl.n.u7Reserved2));
        Log(("ctrl.IntCtrl.u8VIrqVector         %x\n",      pVMCB->ctrl.IntCtrl.n.u8VIrqVector));
        Log(("ctrl.IntCtrl.u24Reserved          %x\n",      pVMCB->ctrl.IntCtrl.n.u24Reserved));

        Log(("ctrl.u64IntShadow                 %RX64\n",   pVMCB->ctrl.u64IntShadow));
        Log(("ctrl.u64ExitCode                  %RX64\n",   pVMCB->ctrl.u64ExitCode));
        Log(("ctrl.u64ExitInfo1                 %RX64\n",   pVMCB->ctrl.u64ExitInfo1));
        Log(("ctrl.u64ExitInfo2                 %RX64\n",   pVMCB->ctrl.u64ExitInfo2));
        Log(("ctrl.ExitIntInfo.u8Vector         %x\n",      pVMCB->ctrl.ExitIntInfo.n.u8Vector));
        Log(("ctrl.ExitIntInfo.u3Type           %x\n",      pVMCB->ctrl.ExitIntInfo.n.u3Type));
        Log(("ctrl.ExitIntInfo.u1ErrorCodeValid %x\n",      pVMCB->ctrl.ExitIntInfo.n.u1ErrorCodeValid));
        Log(("ctrl.ExitIntInfo.u19Reserved      %x\n",      pVMCB->ctrl.ExitIntInfo.n.u19Reserved));
        Log(("ctrl.ExitIntInfo.u1Valid          %x\n",      pVMCB->ctrl.ExitIntInfo.n.u1Valid));
        Log(("ctrl.ExitIntInfo.u32ErrorCode     %x\n",      pVMCB->ctrl.ExitIntInfo.n.u32ErrorCode));
        Log(("ctrl.NestedPaging                 %RX64\n",   pVMCB->ctrl.NestedPaging.au64));
        Log(("ctrl.EventInject.u8Vector         %x\n",      pVMCB->ctrl.EventInject.n.u8Vector));
        Log(("ctrl.EventInject.u3Type           %x\n",      pVMCB->ctrl.EventInject.n.u3Type));
        Log(("ctrl.EventInject.u1ErrorCodeValid %x\n",      pVMCB->ctrl.EventInject.n.u1ErrorCodeValid));
        Log(("ctrl.EventInject.u19Reserved      %x\n",      pVMCB->ctrl.EventInject.n.u19Reserved));
        Log(("ctrl.EventInject.u1Valid          %x\n",      pVMCB->ctrl.EventInject.n.u1Valid));
        Log(("ctrl.EventInject.u32ErrorCode     %x\n",      pVMCB->ctrl.EventInject.n.u32ErrorCode));

        Log(("ctrl.u64NestedPagingCR3           %RX64\n",   pVMCB->ctrl.u64NestedPagingCR3));
        Log(("ctrl.u64LBRVirt                   %RX64\n",   pVMCB->ctrl.u64LBRVirt));

        Log(("guest.CS.u16Sel                   %04X\n",    pVMCB->guest.CS.u16Sel));
        Log(("guest.CS.u16Attr                  %04X\n",    pVMCB->guest.CS.u16Attr));
        Log(("guest.CS.u32Limit                 %X\n",      pVMCB->guest.CS.u32Limit));
        Log(("guest.CS.u64Base                  %RX64\n",   pVMCB->guest.CS.u64Base));
        Log(("guest.DS.u16Sel                   %04X\n",    pVMCB->guest.DS.u16Sel));
        Log(("guest.DS.u16Attr                  %04X\n",    pVMCB->guest.DS.u16Attr));
        Log(("guest.DS.u32Limit                 %X\n",      pVMCB->guest.DS.u32Limit));
        Log(("guest.DS.u64Base                  %RX64\n",   pVMCB->guest.DS.u64Base));
        Log(("guest.ES.u16Sel                   %04X\n",    pVMCB->guest.ES.u16Sel));
        Log(("guest.ES.u16Attr                  %04X\n",    pVMCB->guest.ES.u16Attr));
        Log(("guest.ES.u32Limit                 %X\n",      pVMCB->guest.ES.u32Limit));
        Log(("guest.ES.u64Base                  %RX64\n",   pVMCB->guest.ES.u64Base));
        Log(("guest.FS.u16Sel                   %04X\n",    pVMCB->guest.FS.u16Sel));
        Log(("guest.FS.u16Attr                  %04X\n",    pVMCB->guest.FS.u16Attr));
        Log(("guest.FS.u32Limit                 %X\n",      pVMCB->guest.FS.u32Limit));
        Log(("guest.FS.u64Base                  %RX64\n",   pVMCB->guest.FS.u64Base));
        Log(("guest.GS.u16Sel                   %04X\n",    pVMCB->guest.GS.u16Sel));
        Log(("guest.GS.u16Attr                  %04X\n",    pVMCB->guest.GS.u16Attr));
        Log(("guest.GS.u32Limit                 %X\n",      pVMCB->guest.GS.u32Limit));
        Log(("guest.GS.u64Base                  %RX64\n",   pVMCB->guest.GS.u64Base));

        Log(("guest.GDTR.u32Limit               %X\n",      pVMCB->guest.GDTR.u32Limit));
        Log(("guest.GDTR.u64Base                %RX64\n",   pVMCB->guest.GDTR.u64Base));

        Log(("guest.LDTR.u16Sel                 %04X\n",    pVMCB->guest.LDTR.u16Sel));
        Log(("guest.LDTR.u16Attr                %04X\n",    pVMCB->guest.LDTR.u16Attr));
        Log(("guest.LDTR.u32Limit               %X\n",      pVMCB->guest.LDTR.u32Limit));
        Log(("guest.LDTR.u64Base                %RX64\n",   pVMCB->guest.LDTR.u64Base));

        Log(("guest.IDTR.u32Limit               %X\n",      pVMCB->guest.IDTR.u32Limit));
        Log(("guest.IDTR.u64Base                %RX64\n",   pVMCB->guest.IDTR.u64Base));

        Log(("guest.TR.u16Sel                   %04X\n",    pVMCB->guest.TR.u16Sel));
        Log(("guest.TR.u16Attr                  %04X\n",    pVMCB->guest.TR.u16Attr));
        Log(("guest.TR.u32Limit                 %X\n",      pVMCB->guest.TR.u32Limit));
        Log(("guest.TR.u64Base                  %RX64\n",   pVMCB->guest.TR.u64Base));

        Log(("guest.u8CPL                       %X\n",      pVMCB->guest.u8CPL));
        Log(("guest.u64CR0                      %RX64\n",   pVMCB->guest.u64CR0));
        Log(("guest.u64CR2                      %RX64\n",   pVMCB->guest.u64CR2));
        Log(("guest.u64CR3                      %RX64\n",   pVMCB->guest.u64CR3));
        Log(("guest.u64CR4                      %RX64\n",   pVMCB->guest.u64CR4));
        Log(("guest.u64DR6                      %RX64\n",   pVMCB->guest.u64DR6));
        Log(("guest.u64DR7                      %RX64\n",   pVMCB->guest.u64DR7));

        Log(("guest.u64RIP                      %RX64\n",   pVMCB->guest.u64RIP));
        Log(("guest.u64RSP                      %RX64\n",   pVMCB->guest.u64RSP));
        Log(("guest.u64RAX                      %RX64\n",   pVMCB->guest.u64RAX));
        Log(("guest.u64RFlags                   %RX64\n",   pVMCB->guest.u64RFlags));

        Log(("guest.u64SysEnterCS               %RX64\n",   pVMCB->guest.u64SysEnterCS));
        Log(("guest.u64SysEnterEIP              %RX64\n",   pVMCB->guest.u64SysEnterEIP));
        Log(("guest.u64SysEnterESP              %RX64\n",   pVMCB->guest.u64SysEnterESP));

        Log(("guest.u64EFER                     %RX64\n",   pVMCB->guest.u64EFER));
        Log(("guest.u64STAR                     %RX64\n",   pVMCB->guest.u64STAR));
        Log(("guest.u64LSTAR                    %RX64\n",   pVMCB->guest.u64LSTAR));
        Log(("guest.u64CSTAR                    %RX64\n",   pVMCB->guest.u64CSTAR));
        Log(("guest.u64SFMASK                   %RX64\n",   pVMCB->guest.u64SFMASK));
        Log(("guest.u64KernelGSBase             %RX64\n",   pVMCB->guest.u64KernelGSBase));
        Log(("guest.u64GPAT                     %RX64\n",   pVMCB->guest.u64GPAT));
        Log(("guest.u64DBGCTL                   %RX64\n",   pVMCB->guest.u64DBGCTL));
        Log(("guest.u64BR_FROM                  %RX64\n",   pVMCB->guest.u64BR_FROM));
        Log(("guest.u64BR_TO                    %RX64\n",   pVMCB->guest.u64BR_TO));
        Log(("guest.u64LASTEXCPFROM             %RX64\n",   pVMCB->guest.u64LASTEXCPFROM));
        Log(("guest.u64LASTEXCPTO               %RX64\n",   pVMCB->guest.u64LASTEXCPTO));

#endif
        rc = VERR_SVM_UNABLE_TO_START_VM;
        goto end;
    }

    /* Let's first sync back eip, esp, and eflags. */
    pCtx->rip        = pVMCB->guest.u64RIP;
    pCtx->rsp        = pVMCB->guest.u64RSP;
    pCtx->eflags.u32 = pVMCB->guest.u64RFlags;
    /* eax is saved/restore across the vmrun instruction */
    pCtx->rax        = pVMCB->guest.u64RAX;

    pCtx->msrKERNELGSBASE = pVMCB->guest.u64KernelGSBase;    /* swapgs exchange value */

    /* Can be updated behind our back in the nested paging case. */
    pCtx->cr2        = pVMCB->guest.u64CR2;

    /* Guest CPU context: ES, CS, SS, DS, FS, GS. */
    SVM_READ_SELREG(SS, ss);
    SVM_READ_SELREG(CS, cs);
    SVM_READ_SELREG(DS, ds);
    SVM_READ_SELREG(ES, es);
    SVM_READ_SELREG(FS, fs);
    SVM_READ_SELREG(GS, gs);

    /*
     * System MSRs
     */
    pCtx->SysEnter.cs       = pVMCB->guest.u64SysEnterCS;
    pCtx->SysEnter.eip      = pVMCB->guest.u64SysEnterEIP;
    pCtx->SysEnter.esp      = pVMCB->guest.u64SysEnterESP;

    /* Remaining guest CPU context: TR, IDTR, GDTR, LDTR; must sync everything otherwise we can get out of sync when jumping to ring 3. */
    SVM_READ_SELREG(LDTR, ldtr);
    SVM_READ_SELREG(TR, tr);

    pCtx->gdtr.cbGdt        = pVMCB->guest.GDTR.u32Limit;
    pCtx->gdtr.pGdt         = pVMCB->guest.GDTR.u64Base;

    pCtx->idtr.cbIdt        = pVMCB->guest.IDTR.u32Limit;
    pCtx->idtr.pIdt         = pVMCB->guest.IDTR.u64Base;

    /* Note: no reason to sync back the CRx and DRx registers. They can't be changed by the guest. */
    /* Note: only in the nested paging case can CR3 & CR4 be changed by the guest. */
    if (    pVM->hwaccm.s.fNestedPaging
        &&  pCtx->cr3 != pVMCB->guest.u64CR3)
    {
        CPUMSetGuestCR3(pVCpu, pVMCB->guest.u64CR3);
        PGMUpdateCR3(pVCpu, pVMCB->guest.u64CR3);
    }

    /* Note! NOW IT'S SAFE FOR LOGGING! */
#ifdef LOG_LOGGING
    VMMR0LogFlushEnable(pVCpu);
#endif

    /* Take care of instruction fusing (sti, mov ss) (see 15.20.5 Interrupt Shadows) */
    if (pVMCB->ctrl.u64IntShadow & SVM_INTERRUPT_SHADOW_ACTIVE)
    {
        Log(("uInterruptState %x rip=%RGv\n", pVMCB->ctrl.u64IntShadow, (RTGCPTR)pCtx->rip));
        EMSetInhibitInterruptsPC(pVCpu, pCtx->rip);
    }
    else
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);

    Log2(("exitCode = %x\n", exitCode));

    /* Sync back DR6 as it could have been changed by hitting breakpoints. */
    pCtx->dr[6] = pVMCB->guest.u64DR6;
    /* DR7.GD can be cleared by debug exceptions, so sync it back as well. */
    pCtx->dr[7] = pVMCB->guest.u64DR7;

    /* Check if an injected event was interrupted prematurely. */
    pVCpu->hwaccm.s.Event.intInfo = pVMCB->ctrl.ExitIntInfo.au64[0];
    if (    pVMCB->ctrl.ExitIntInfo.n.u1Valid
        &&  pVMCB->ctrl.ExitIntInfo.n.u3Type != SVM_EVENT_SOFTWARE_INT /* we don't care about 'int xx' as the instruction will be restarted. */)
    {
        Log(("Pending inject %RX64 at %RGv exit=%08x\n", pVCpu->hwaccm.s.Event.intInfo, (RTGCPTR)pCtx->rip, exitCode));

#ifdef LOG_ENABLED
        SVM_EVENT Event;
        Event.au64[0] = pVCpu->hwaccm.s.Event.intInfo;

        if (    exitCode == SVM_EXIT_EXCEPTION_E
            &&  Event.n.u8Vector == 0xE)
        {
            Log(("Double fault!\n"));
        }
#endif

        pVCpu->hwaccm.s.Event.fPending = true;
        /* Error code present? (redundant) */
        if (pVMCB->ctrl.ExitIntInfo.n.u1ErrorCodeValid)
        {
            pVCpu->hwaccm.s.Event.errCode  = pVMCB->ctrl.ExitIntInfo.n.u32ErrorCode;
        }
        else
            pVCpu->hwaccm.s.Event.errCode  = 0;
    }
#ifdef VBOX_WITH_STATISTICS
    if (exitCode == SVM_EXIT_NPF)
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitReasonNPF);
    else
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.paStatExitReasonR0[exitCode & MASK_EXITREASON_STAT]);
#endif

    if (fSyncTPR)
    {
        rc = PDMApicSetTPR(pVM, pVMCB->ctrl.IntCtrl.n.u8VTPR);
        AssertRC(rc);
    }

    /* Deal with the reason of the VM-exit. */
    switch (exitCode)
    {
    case SVM_EXIT_EXCEPTION_0:  case SVM_EXIT_EXCEPTION_1:  case SVM_EXIT_EXCEPTION_2:  case SVM_EXIT_EXCEPTION_3:
    case SVM_EXIT_EXCEPTION_4:  case SVM_EXIT_EXCEPTION_5:  case SVM_EXIT_EXCEPTION_6:  case SVM_EXIT_EXCEPTION_7:
    case SVM_EXIT_EXCEPTION_8:  case SVM_EXIT_EXCEPTION_9:  case SVM_EXIT_EXCEPTION_A:  case SVM_EXIT_EXCEPTION_B:
    case SVM_EXIT_EXCEPTION_C:  case SVM_EXIT_EXCEPTION_D:  case SVM_EXIT_EXCEPTION_E:  case SVM_EXIT_EXCEPTION_F:
    case SVM_EXIT_EXCEPTION_10: case SVM_EXIT_EXCEPTION_11: case SVM_EXIT_EXCEPTION_12: case SVM_EXIT_EXCEPTION_13:
    case SVM_EXIT_EXCEPTION_14: case SVM_EXIT_EXCEPTION_15: case SVM_EXIT_EXCEPTION_16: case SVM_EXIT_EXCEPTION_17:
    case SVM_EXIT_EXCEPTION_18: case SVM_EXIT_EXCEPTION_19: case SVM_EXIT_EXCEPTION_1A: case SVM_EXIT_EXCEPTION_1B:
    case SVM_EXIT_EXCEPTION_1C: case SVM_EXIT_EXCEPTION_1D: case SVM_EXIT_EXCEPTION_1E: case SVM_EXIT_EXCEPTION_1F:
    {
        /* Pending trap. */
        SVM_EVENT   Event;
        uint32_t    vector = exitCode - SVM_EXIT_EXCEPTION_0;

        Log2(("Hardware/software interrupt %d\n", vector));
        switch (vector)
        {
        case X86_XCPT_DB:
        {
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestDB);

            /* Note that we don't support guest and host-initiated debugging at the same time. */
            Assert(DBGFIsStepping(pVCpu));

            rc = DBGFRZTrap01Handler(pVM, pVCpu, CPUMCTX2CORE(pCtx), pCtx->dr[6]);
            if (rc == VINF_EM_RAW_GUEST_TRAP)
            {
                Log(("Trap %x (debug) at %016RX64\n", vector, pCtx->rip));

                /* Reinject the exception. */
                Event.au64[0]    = 0;
                Event.n.u3Type   = SVM_EVENT_EXCEPTION; /* trap or fault */
                Event.n.u1Valid  = 1;
                Event.n.u8Vector = X86_XCPT_DB;

                SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
                goto ResumeExecution;
            }
            /* Return to ring 3 to deal with the debug exit code. */
            break;
        }

        case X86_XCPT_NM:
        {
            Log(("#NM fault at %RGv\n", (RTGCPTR)pCtx->rip));

            /** @todo don't intercept #NM exceptions anymore when we've activated the guest FPU state. */
            /* If we sync the FPU/XMM state on-demand, then we can continue execution as if nothing has happened. */
            rc = CPUMR0LoadGuestFPU(pVM, pVCpu, pCtx);
            if (rc == VINF_SUCCESS)
            {
                Assert(CPUMIsGuestFPUStateActive(pVCpu));
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitShadowNM);

                /* Continue execution. */
                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
                pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0;

                goto ResumeExecution;
            }

            Log(("Forward #NM fault to the guest\n"));
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestNM);

            Event.au64[0]    = 0;
            Event.n.u3Type   = SVM_EVENT_EXCEPTION;
            Event.n.u1Valid  = 1;
            Event.n.u8Vector = X86_XCPT_NM;

            SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }

        case X86_XCPT_PF: /* Page fault */
        {
            uint32_t    errCode        = pVMCB->ctrl.u64ExitInfo1;     /* EXITINFO1 = error code */
            RTGCUINTPTR uFaultAddress  = pVMCB->ctrl.u64ExitInfo2;     /* EXITINFO2 = fault address */

#ifdef DEBUG
            if (pVM->hwaccm.s.fNestedPaging)
            {   /* A genuine pagefault.
                 * Forward the trap to the guest by injecting the exception and resuming execution.
                 */
                Log(("Guest page fault at %RGv cr2=%RGv error code %x rsp=%RGv\n", (RTGCPTR)pCtx->rip, uFaultAddress, errCode, (RTGCPTR)pCtx->rsp));
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestPF);

                /* Now we must update CR2. */
                pCtx->cr2 = uFaultAddress;

                Event.au64[0]               = 0;
                Event.n.u3Type              = SVM_EVENT_EXCEPTION;
                Event.n.u1Valid             = 1;
                Event.n.u8Vector            = X86_XCPT_PF;
                Event.n.u1ErrorCodeValid    = 1;
                Event.n.u32ErrorCode        = errCode;

                SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
                goto ResumeExecution;
            }
#endif
            Assert(!pVM->hwaccm.s.fNestedPaging);

            Log2(("Page fault at %RGv cr2=%RGv error code %x\n", (RTGCPTR)pCtx->rip, uFaultAddress, errCode));
            /* Exit qualification contains the linear address of the page fault. */
            TRPMAssertTrap(pVCpu, X86_XCPT_PF, TRPM_TRAP);
            TRPMSetErrorCode(pVCpu, errCode);
            TRPMSetFaultAddress(pVCpu, uFaultAddress);

            /* Forward it to our trap handler first, in case our shadow pages are out of sync. */
            rc = PGMTrap0eHandler(pVCpu, errCode, CPUMCTX2CORE(pCtx), (RTGCPTR)uFaultAddress);
            Log2(("PGMTrap0eHandler %RGv returned %Rrc\n", (RTGCPTR)pCtx->rip, rc));
            if (rc == VINF_SUCCESS)
            {   /* We've successfully synced our shadow pages, so let's just continue execution. */
                Log2(("Shadow page fault at %RGv cr2=%RGv error code %x\n", (RTGCPTR)pCtx->rip, uFaultAddress, errCode));
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitShadowPF);

                TRPMResetTrap(pVCpu);

                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
                goto ResumeExecution;
            }
            else
            if (rc == VINF_EM_RAW_GUEST_TRAP)
            {   /* A genuine pagefault.
                 * Forward the trap to the guest by injecting the exception and resuming execution.
                 */
                Log2(("Forward page fault to the guest\n"));
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestPF);
                /* The error code might have been changed. */
                errCode = TRPMGetErrorCode(pVCpu);

                TRPMResetTrap(pVCpu);

                /* Now we must update CR2. */
                pCtx->cr2 = uFaultAddress;

                Event.au64[0]               = 0;
                Event.n.u3Type              = SVM_EVENT_EXCEPTION;
                Event.n.u1Valid             = 1;
                Event.n.u8Vector            = X86_XCPT_PF;
                Event.n.u1ErrorCodeValid    = 1;
                Event.n.u32ErrorCode        = errCode;

                SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
                goto ResumeExecution;
            }
#ifdef VBOX_STRICT
            if (rc != VINF_EM_RAW_EMULATE_INSTR && rc != VINF_EM_RAW_EMULATE_IO_BLOCK)
                LogFlow(("PGMTrap0eHandler failed with %d\n", rc));
#endif
            /* Need to go back to the recompiler to emulate the instruction. */
            TRPMResetTrap(pVCpu);
            break;
        }

        case X86_XCPT_MF: /* Floating point exception. */
        {
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestMF);
            if (!(pCtx->cr0 & X86_CR0_NE))
            {
                /* old style FPU error reporting needs some extra work. */
                /** @todo don't fall back to the recompiler, but do it manually. */
                rc = VINF_EM_RAW_EMULATE_INSTR;
                break;
            }
            Log(("Trap %x at %RGv\n", vector, (RTGCPTR)pCtx->rip));

            Event.au64[0]    = 0;
            Event.n.u3Type   = SVM_EVENT_EXCEPTION;
            Event.n.u1Valid  = 1;
            Event.n.u8Vector = X86_XCPT_MF;

            SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }

#ifdef VBOX_STRICT
        case X86_XCPT_GP:   /* General protection failure exception.*/
        case X86_XCPT_UD:   /* Unknown opcode exception. */
        case X86_XCPT_DE:   /* Divide error. */
        case X86_XCPT_SS:   /* Stack segment exception. */
        case X86_XCPT_NP:   /* Segment not present exception. */
        {
            Event.au64[0]    = 0;
            Event.n.u3Type   = SVM_EVENT_EXCEPTION;
            Event.n.u1Valid  = 1;
            Event.n.u8Vector = vector;

            switch(vector)
            {
            case X86_XCPT_GP:
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestGP);
                Event.n.u1ErrorCodeValid    = 1;
                Event.n.u32ErrorCode        = pVMCB->ctrl.u64ExitInfo1; /* EXITINFO1 = error code */
                break;
            case X86_XCPT_DE:
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestDE);
                break;
            case X86_XCPT_UD:
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestUD);
                break;
            case X86_XCPT_SS:
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestSS);
                Event.n.u1ErrorCodeValid    = 1;
                Event.n.u32ErrorCode        = pVMCB->ctrl.u64ExitInfo1; /* EXITINFO1 = error code */
                break;
            case X86_XCPT_NP:
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestNP);
                Event.n.u1ErrorCodeValid    = 1;
                Event.n.u32ErrorCode        = pVMCB->ctrl.u64ExitInfo1; /* EXITINFO1 = error code */
                break;
            }
            Log(("Trap %x at %RGv esi=%x\n", vector, (RTGCPTR)pCtx->rip, pCtx->esi));
            SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }
#endif
        default:
            AssertMsgFailed(("Unexpected vm-exit caused by exception %x\n", vector));
            rc = VERR_EM_INTERNAL_ERROR;
            break;

        } /* switch (vector) */
        break;
    }

    case SVM_EXIT_NPF:
    {
        /* EXITINFO1 contains fault errorcode; EXITINFO2 contains the guest physical address causing the fault. */
        uint32_t    errCode        = pVMCB->ctrl.u64ExitInfo1;     /* EXITINFO1 = error code */
        RTGCPHYS    uFaultAddress  = pVMCB->ctrl.u64ExitInfo2;     /* EXITINFO2 = fault address */
        PGMMODE     enmShwPagingMode;

        Assert(pVM->hwaccm.s.fNestedPaging);
        Log(("Nested page fault at %RGv cr2=%RGp error code %x\n", (RTGCPTR)pCtx->rip, uFaultAddress, errCode));
        /* Exit qualification contains the linear address of the page fault. */
        TRPMAssertTrap(pVCpu, X86_XCPT_PF, TRPM_TRAP);
        TRPMSetErrorCode(pVCpu, errCode);
        TRPMSetFaultAddress(pVCpu, uFaultAddress);

        /* Handle the pagefault trap for the nested shadow table. */
#if HC_ARCH_BITS == 32
        if (CPUMIsGuestInLongModeEx(pCtx))
            enmShwPagingMode = PGMMODE_AMD64_NX;
        else
#endif
            enmShwPagingMode = PGMGetHostMode(pVM);

        rc = PGMR0Trap0eHandlerNestedPaging(pVM, pVCpu, enmShwPagingMode, errCode, CPUMCTX2CORE(pCtx), uFaultAddress);
        Log2(("PGMR0Trap0eHandlerNestedPaging %RGv returned %Rrc\n", (RTGCPTR)pCtx->rip, rc));
        if (rc == VINF_SUCCESS)
        {   /* We've successfully synced our shadow pages, so let's just continue execution. */
            Log2(("Shadow page fault at %RGv cr2=%RGp error code %x\n", (RTGCPTR)pCtx->rip, uFaultAddress, errCode));
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitShadowPF);

            TRPMResetTrap(pVCpu);

            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }

#ifdef VBOX_STRICT
        if (rc != VINF_EM_RAW_EMULATE_INSTR)
            LogFlow(("PGMTrap0eHandlerNestedPaging failed with %d\n", rc));
#endif
        /* Need to go back to the recompiler to emulate the instruction. */
        TRPMResetTrap(pVCpu);
        break;
    }

    case SVM_EXIT_VINTR:
        /* A virtual interrupt is about to be delivered, which means IF=1. */
        Log(("SVM_EXIT_VINTR IF=%d\n", pCtx->eflags.Bits.u1IF));
        pVMCB->ctrl.IntCtrl.n.u1VIrqValid    = 0;
        pVMCB->ctrl.IntCtrl.n.u8VIrqVector   = 0;
        goto ResumeExecution;

    case SVM_EXIT_FERR_FREEZE:
    case SVM_EXIT_INTR:
    case SVM_EXIT_NMI:
    case SVM_EXIT_SMI:
    case SVM_EXIT_INIT:
        /* External interrupt; leave to allow it to be dispatched again. */
        rc = VINF_EM_RAW_INTERRUPT;
        break;

    case SVM_EXIT_WBINVD:
    case SVM_EXIT_INVD:                 /* Guest software attempted to execute INVD. */
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitInvd);
        /* Skip instruction and continue directly. */
        pCtx->rip += 2;     /* Note! hardcoded opcode size! */
        /* Continue execution.*/
        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
        goto ResumeExecution;

    case SVM_EXIT_CPUID:                /* Guest software attempted to execute CPUID. */
    {
        Log2(("SVM: Cpuid at %RGv for %x\n", (RTGCPTR)pCtx->rip, pCtx->eax));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitCpuid);
        rc = EMInterpretCpuId(pVM, pVCpu, CPUMCTX2CORE(pCtx));
        if (rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            pCtx->rip += 2;             /* Note! hardcoded opcode size! */
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }
        AssertMsgFailed(("EMU: cpuid failed with %Rrc\n", rc));
        rc = VINF_EM_RAW_EMULATE_INSTR;
        break;
    }

    case SVM_EXIT_RDTSC:                /* Guest software attempted to execute RDTSC. */
    {
        Log2(("SVM: Rdtsc\n"));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitRdtsc);
        rc = EMInterpretRdtsc(pVM, pVCpu, CPUMCTX2CORE(pCtx));
        if (rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            pCtx->rip += 2;             /* Note! hardcoded opcode size! */
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }
        rc = VINF_EM_RAW_EMULATE_INSTR;
        break;
    }

    case SVM_EXIT_RDPMC:                /* Guest software attempted to execute RDPMC. */
    {
        Log2(("SVM: Rdpmc %x\n", pCtx->ecx));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitRdpmc);
        rc = EMInterpretRdpmc(pVM, pVCpu, CPUMCTX2CORE(pCtx));
        if (rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            pCtx->rip += 2;             /* Note! hardcoded opcode size! */
            goto ResumeExecution;
        }
        rc = VINF_EM_RAW_EMULATE_INSTR;
        break;
    }

    case SVM_EXIT_RDTSCP:                /* Guest software attempted to execute RDTSCP. */
    {
        Log2(("SVM: Rdtscp\n"));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitRdtsc);
        rc = EMInterpretRdtscp(pVM, pVCpu, pCtx);
        if (rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            pCtx->rip += 3;             /* Note! hardcoded opcode size! */
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }
        AssertMsgFailed(("EMU: rdtscp failed with %Rrc\n", rc));
        rc = VINF_EM_RAW_EMULATE_INSTR;
        break;
    }

    case SVM_EXIT_INVLPG:               /* Guest software attempted to execute INVPG. */
    {
        Log2(("SVM: invlpg\n"));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitInvpg);

        Assert(!pVM->hwaccm.s.fNestedPaging);

        /* Truly a pita. Why can't SVM give the same information as VT-x? */
        rc = SVMR0InterpretInvpg(pVM, pVCpu, CPUMCTX2CORE(pCtx), pVMCB->ctrl.TLBCtrl.n.u32ASID);
        if (rc == VINF_SUCCESS)
        {
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushPageInvlpg);
            goto ResumeExecution;   /* eip already updated */
        }
        break;
    }

    case SVM_EXIT_WRITE_CR0:  case SVM_EXIT_WRITE_CR1:  case SVM_EXIT_WRITE_CR2:  case SVM_EXIT_WRITE_CR3:
    case SVM_EXIT_WRITE_CR4:  case SVM_EXIT_WRITE_CR5:  case SVM_EXIT_WRITE_CR6:  case SVM_EXIT_WRITE_CR7:
    case SVM_EXIT_WRITE_CR8:  case SVM_EXIT_WRITE_CR9:  case SVM_EXIT_WRITE_CR10: case SVM_EXIT_WRITE_CR11:
    case SVM_EXIT_WRITE_CR12: case SVM_EXIT_WRITE_CR13: case SVM_EXIT_WRITE_CR14: case SVM_EXIT_WRITE_CR15:
    {
        uint32_t cbSize;

        Log2(("SVM: %RGv mov cr%d, \n", (RTGCPTR)pCtx->rip, exitCode - SVM_EXIT_WRITE_CR0));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitCRxWrite[exitCode - SVM_EXIT_WRITE_CR0]);
        rc = EMInterpretInstruction(pVM, pVCpu, CPUMCTX2CORE(pCtx), 0, &cbSize);

        switch (exitCode - SVM_EXIT_WRITE_CR0)
        {
        case 0:
            pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0;
            break;
        case 2:
            break;
        case 3:
            Assert(!pVM->hwaccm.s.fNestedPaging);
            pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR3;
            break;
        case 4:
            pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR4;
            break;
        case 8:
            break;
        default:
            AssertFailed();
        }
        /* Check if a sync operation is pending. */
        if (    rc == VINF_SUCCESS /* don't bother if we are going to ring 3 anyway */
            &&  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL))
        {
            rc = PGMSyncCR3(pVCpu, pCtx->cr0, pCtx->cr3, pCtx->cr4, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3));
            AssertRC(rc);

            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushTLBCRxChange);

            /* Must be set by PGMSyncCR3 */
            Assert(rc != VINF_SUCCESS || PGMGetGuestMode(pVCpu) <= PGMMODE_PROTECTED || pVCpu->hwaccm.s.fForceTLBFlush);
        }
        if (rc == VINF_SUCCESS)
        {
            /* EIP has been updated already. */

            /* Only resume if successful. */
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }
        Assert(rc == VERR_EM_INTERPRETER || rc == VINF_PGM_CHANGE_MODE || rc == VINF_PGM_SYNC_CR3);
        break;
    }

    case SVM_EXIT_READ_CR0:   case SVM_EXIT_READ_CR1:   case SVM_EXIT_READ_CR2:   case SVM_EXIT_READ_CR3:
    case SVM_EXIT_READ_CR4:   case SVM_EXIT_READ_CR5:   case SVM_EXIT_READ_CR6:   case SVM_EXIT_READ_CR7:
    case SVM_EXIT_READ_CR8:   case SVM_EXIT_READ_CR9:   case SVM_EXIT_READ_CR10:  case SVM_EXIT_READ_CR11:
    case SVM_EXIT_READ_CR12:  case SVM_EXIT_READ_CR13:  case SVM_EXIT_READ_CR14:  case SVM_EXIT_READ_CR15:
    {
        uint32_t cbSize;

        Log2(("SVM: %RGv mov x, cr%d\n", (RTGCPTR)pCtx->rip, exitCode - SVM_EXIT_READ_CR0));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitCRxRead[exitCode - SVM_EXIT_READ_CR0]);
        rc = EMInterpretInstruction(pVM, pVCpu, CPUMCTX2CORE(pCtx), 0, &cbSize);
        if (rc == VINF_SUCCESS)
        {
            /* EIP has been updated already. */

            /* Only resume if successful. */
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }
        Assert(rc == VERR_EM_INTERPRETER || rc == VINF_PGM_CHANGE_MODE || rc == VINF_PGM_SYNC_CR3);
        break;
    }

    case SVM_EXIT_WRITE_DR0:   case SVM_EXIT_WRITE_DR1:   case SVM_EXIT_WRITE_DR2:   case SVM_EXIT_WRITE_DR3:
    case SVM_EXIT_WRITE_DR4:   case SVM_EXIT_WRITE_DR5:   case SVM_EXIT_WRITE_DR6:   case SVM_EXIT_WRITE_DR7:
    case SVM_EXIT_WRITE_DR8:   case SVM_EXIT_WRITE_DR9:   case SVM_EXIT_WRITE_DR10:  case SVM_EXIT_WRITE_DR11:
    case SVM_EXIT_WRITE_DR12:  case SVM_EXIT_WRITE_DR13:  case SVM_EXIT_WRITE_DR14:  case SVM_EXIT_WRITE_DR15:
    {
        uint32_t cbSize;

        Log2(("SVM: %RGv mov dr%d, x\n", (RTGCPTR)pCtx->rip, exitCode - SVM_EXIT_WRITE_DR0));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitDRxWrite);

        if (!DBGFIsStepping(pVCpu))
        {
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatDRxContextSwitch);

            /* Disable drx move intercepts. */
            pVMCB->ctrl.u16InterceptRdDRx = 0;
            pVMCB->ctrl.u16InterceptWrDRx = 0;

            /* Save the host and load the guest debug state. */
            rc = CPUMR0LoadGuestDebugState(pVM, pVCpu, pCtx, false /* exclude DR6 */);
            AssertRC(rc);

            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }

        rc = EMInterpretInstruction(pVM, pVCpu, CPUMCTX2CORE(pCtx), 0, &cbSize);
        if (rc == VINF_SUCCESS)
        {
            /* EIP has been updated already. */
            pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_DEBUG;

            /* Only resume if successful. */
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }
        Assert(rc == VERR_EM_INTERPRETER || rc == VINF_PGM_CHANGE_MODE || rc == VINF_PGM_SYNC_CR3);
        break;
    }

    case SVM_EXIT_READ_DR0:   case SVM_EXIT_READ_DR1:   case SVM_EXIT_READ_DR2:   case SVM_EXIT_READ_DR3:
    case SVM_EXIT_READ_DR4:   case SVM_EXIT_READ_DR5:   case SVM_EXIT_READ_DR6:   case SVM_EXIT_READ_DR7:
    case SVM_EXIT_READ_DR8:   case SVM_EXIT_READ_DR9:   case SVM_EXIT_READ_DR10:  case SVM_EXIT_READ_DR11:
    case SVM_EXIT_READ_DR12:  case SVM_EXIT_READ_DR13:  case SVM_EXIT_READ_DR14:  case SVM_EXIT_READ_DR15:
    {
        uint32_t cbSize;

        Log2(("SVM: %RGv mov x, dr%d\n", (RTGCPTR)pCtx->rip, exitCode - SVM_EXIT_READ_DR0));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitDRxRead);

        if (!DBGFIsStepping(pVCpu))
        {
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatDRxContextSwitch);

            /* Disable drx move intercepts. */
            pVMCB->ctrl.u16InterceptRdDRx = 0;
            pVMCB->ctrl.u16InterceptWrDRx = 0;

            /* Save the host and load the guest debug state. */
            rc = CPUMR0LoadGuestDebugState(pVM, pVCpu, pCtx, false /* exclude DR6 */);
            AssertRC(rc);

            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }

        rc = EMInterpretInstruction(pVM, pVCpu, CPUMCTX2CORE(pCtx), 0, &cbSize);
        if (rc == VINF_SUCCESS)
        {
            /* EIP has been updated already. */

            /* Only resume if successful. */
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }
        Assert(rc == VERR_EM_INTERPRETER || rc == VINF_PGM_CHANGE_MODE || rc == VINF_PGM_SYNC_CR3);
        break;
    }

    /* Note: We'll get a #GP if the IO instruction isn't allowed (IOPL or TSS bitmap); no need to double check. */
    case SVM_EXIT_IOIO:              /* I/O instruction. */
    {
        SVM_IOIO_EXIT   IoExitInfo;
        uint32_t        uIOSize, uAndVal;

        IoExitInfo.au32[0] = pVMCB->ctrl.u64ExitInfo1;

        /** @todo could use a lookup table here */
        if (IoExitInfo.n.u1OP8)
        {
            uIOSize = 1;
            uAndVal = 0xff;
        }
        else
        if (IoExitInfo.n.u1OP16)
        {
            uIOSize = 2;
            uAndVal = 0xffff;
        }
        else
        if (IoExitInfo.n.u1OP32)
        {
            uIOSize = 4;
            uAndVal = 0xffffffff;
        }
        else
        {
            AssertFailed(); /* should be fatal. */
            rc = VINF_EM_RAW_EMULATE_INSTR;
            break;
        }

        if (IoExitInfo.n.u1STR)
        {
            /* ins/outs */
            DISCPUSTATE Cpu;

            /* Disassemble manually to deal with segment prefixes. */
            rc = EMInterpretDisasOne(pVM, pVCpu, CPUMCTX2CORE(pCtx), &Cpu, NULL);
            if (rc == VINF_SUCCESS)
            {
                if (IoExitInfo.n.u1Type == 0)
                {
                    Log2(("IOMInterpretOUTSEx %RGv %x size=%d\n", (RTGCPTR)pCtx->rip, IoExitInfo.n.u16Port, uIOSize));
                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIOStringWrite);
                    rc = IOMInterpretOUTSEx(pVM, CPUMCTX2CORE(pCtx), IoExitInfo.n.u16Port, Cpu.prefix, uIOSize);
                }
                else
                {
                    Log2(("IOMInterpretINSEx  %RGv %x size=%d\n", (RTGCPTR)pCtx->rip, IoExitInfo.n.u16Port, uIOSize));
                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIOStringRead);
                    rc = IOMInterpretINSEx(pVM, CPUMCTX2CORE(pCtx), IoExitInfo.n.u16Port, Cpu.prefix, uIOSize);
                }
            }
            else
                rc = VINF_EM_RAW_EMULATE_INSTR;
        }
        else
        {
            /* normal in/out */
            Assert(!IoExitInfo.n.u1REP);

            if (IoExitInfo.n.u1Type == 0)
            {
                Log2(("IOMIOPortWrite %RGv %x %x size=%d\n", (RTGCPTR)pCtx->rip, IoExitInfo.n.u16Port, pCtx->eax & uAndVal, uIOSize));
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIOWrite);
                rc = IOMIOPortWrite(pVM, IoExitInfo.n.u16Port, pCtx->eax & uAndVal, uIOSize);
            }
            else
            {
                uint32_t u32Val = 0;

                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIORead);
                rc = IOMIOPortRead(pVM, IoExitInfo.n.u16Port, &u32Val, uIOSize);
                if (IOM_SUCCESS(rc))
                {
                    /* Write back to the EAX register. */
                    pCtx->eax = (pCtx->eax & ~uAndVal) | (u32Val & uAndVal);
                    Log2(("IOMIOPortRead %RGv %x %x size=%d\n", (RTGCPTR)pCtx->rip, IoExitInfo.n.u16Port, u32Val & uAndVal, uIOSize));
                }
            }
        }
        /*
         * Handled the I/O return codes.
         * (The unhandled cases end up with rc == VINF_EM_RAW_EMULATE_INSTR.)
         */
        if (IOM_SUCCESS(rc))
        {
            /* Update EIP and continue execution. */
            pCtx->rip = pVMCB->ctrl.u64ExitInfo2;      /* RIP/EIP of the next instruction is saved in EXITINFO2. */
            if (RT_LIKELY(rc == VINF_SUCCESS))
            {
                /* If any IO breakpoints are armed, then we should check if a debug trap needs to be generated. */
                if (pCtx->dr[7] & X86_DR7_ENABLED_MASK)
                {
                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatDRxIOCheck);
                    for (unsigned i=0;i<4;i++)
                    {
                        unsigned uBPLen = g_aIOSize[X86_DR7_GET_LEN(pCtx->dr[7], i)];

                        if (    (IoExitInfo.n.u16Port >= pCtx->dr[i] && IoExitInfo.n.u16Port < pCtx->dr[i] + uBPLen)
                            &&  (pCtx->dr[7] & (X86_DR7_L(i) | X86_DR7_G(i)))
                            &&  (pCtx->dr[7] & X86_DR7_RW(i, X86_DR7_RW_IO)) == X86_DR7_RW(i, X86_DR7_RW_IO))
                        {
                            SVM_EVENT Event;

                            Assert(CPUMIsGuestDebugStateActive(pVCpu));

                            /* Clear all breakpoint status flags and set the one we just hit. */
                            pCtx->dr[6] &= ~(X86_DR6_B0|X86_DR6_B1|X86_DR6_B2|X86_DR6_B3);
                            pCtx->dr[6] |= (uint64_t)RT_BIT(i);

                            /* Note: AMD64 Architecture Programmer's Manual 13.1:
                             * Bits 15:13 of the DR6 register is never cleared by the processor and must be cleared by software after
                             * the contents have been read.
                             */
                            pVMCB->guest.u64DR6 = pCtx->dr[6];

                            /* X86_DR7_GD will be cleared if drx accesses should be trapped inside the guest. */
                            pCtx->dr[7] &= ~X86_DR7_GD;

                            /* Paranoia. */
                            pCtx->dr[7] &= 0xffffffff;                                              /* upper 32 bits reserved */
                            pCtx->dr[7] &= ~(RT_BIT(11) | RT_BIT(12) | RT_BIT(14) | RT_BIT(15));    /* must be zero */
                            pCtx->dr[7] |= 0x400;                                                   /* must be one */

                            pVMCB->guest.u64DR7 = pCtx->dr[7];

                            /* Inject the exception. */
                            Log(("Inject IO debug trap at %RGv\n", (RTGCPTR)pCtx->rip));

                            Event.au64[0]    = 0;
                            Event.n.u3Type   = SVM_EVENT_EXCEPTION; /* trap or fault */
                            Event.n.u1Valid  = 1;
                            Event.n.u8Vector = X86_XCPT_DB;

                            SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

                            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
                            goto ResumeExecution;
                        }
                    }
                }

                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
                goto ResumeExecution;
            }
            Log2(("EM status from IO at %RGv %x size %d: %Rrc\n", (RTGCPTR)pCtx->rip, IoExitInfo.n.u16Port, uIOSize, rc));
            break;
        }

#ifdef VBOX_STRICT
        if (rc == VINF_IOM_HC_IOPORT_READ)
            Assert(IoExitInfo.n.u1Type != 0);
        else if (rc == VINF_IOM_HC_IOPORT_WRITE)
            Assert(IoExitInfo.n.u1Type == 0);
        else
            AssertMsg(RT_FAILURE(rc) || rc == VINF_EM_RAW_EMULATE_INSTR || rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_TRPM_XCPT_DISPATCHED, ("%Rrc\n", rc));
#endif
        Log2(("Failed IO at %RGv %x size %d\n", (RTGCPTR)pCtx->rip, IoExitInfo.n.u16Port, uIOSize));
        break;
    }

    case SVM_EXIT_HLT:
        /** Check if external interrupts are pending; if so, don't switch back. */
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitHlt);
        pCtx->rip++;    /* skip hlt */
        if (    pCtx->eflags.Bits.u1IF
            &&  VMCPU_FF_ISPENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC|VMCPU_FF_INTERRUPT_PIC)))
            goto ResumeExecution;

        rc = VINF_EM_HALT;
        break;

    case SVM_EXIT_MWAIT_UNCOND:
        Log2(("SVM: mwait\n"));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitMwait);
        rc = EMInterpretMWait(pVM, pVCpu, CPUMCTX2CORE(pCtx));
        if (    rc == VINF_EM_HALT
            ||  rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            pCtx->rip += 3;     /* Note: hardcoded opcode size assumption! */

            /** Check if external interrupts are pending; if so, don't switch back. */
            if (    rc == VINF_SUCCESS
                ||  (   rc == VINF_EM_HALT
                     && pCtx->eflags.Bits.u1IF
                     && VMCPU_FF_ISPENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC|VMCPU_FF_INTERRUPT_PIC)))
               )
                goto ResumeExecution;
        }
        AssertMsg(rc == VERR_EM_INTERPRETER || rc == VINF_EM_HALT, ("EMU: mwait failed with %Rrc\n", rc));
        break;

    case SVM_EXIT_RSM:
    case SVM_EXIT_INVLPGA:
    case SVM_EXIT_VMRUN:
    case SVM_EXIT_VMMCALL:
    case SVM_EXIT_VMLOAD:
    case SVM_EXIT_VMSAVE:
    case SVM_EXIT_STGI:
    case SVM_EXIT_CLGI:
    case SVM_EXIT_SKINIT:
    {
        /* Unsupported instructions. */
        SVM_EVENT Event;

        Event.au64[0]    = 0;
        Event.n.u3Type   = SVM_EVENT_EXCEPTION;
        Event.n.u1Valid  = 1;
        Event.n.u8Vector = X86_XCPT_UD;

        Log(("Forced #UD trap at %RGv\n", (RTGCPTR)pCtx->rip));
        SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
        goto ResumeExecution;
    }

    /* Emulate in ring 3. */
    case SVM_EXIT_MSR:
    {
        uint32_t cbSize;

        /* Note: the intel manual claims there's a REX version of RDMSR that's slightly different, so we play safe by completely disassembling the instruction. */
        STAM_COUNTER_INC((pVMCB->ctrl.u64ExitInfo1 == 0) ? &pVCpu->hwaccm.s.StatExitRdmsr : &pVCpu->hwaccm.s.StatExitWrmsr);
        Log(("SVM: %s\n", (pVMCB->ctrl.u64ExitInfo1 == 0) ? "rdmsr" : "wrmsr"));
        rc = EMInterpretInstruction(pVM, pVCpu, CPUMCTX2CORE(pCtx), 0, &cbSize);
        if (rc == VINF_SUCCESS)
        {
            /* EIP has been updated already. */

            /* Only resume if successful. */
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
            goto ResumeExecution;
        }
        AssertMsg(rc == VERR_EM_INTERPRETER, ("EMU: %s failed with %Rrc\n", (pVMCB->ctrl.u64ExitInfo1 == 0) ? "rdmsr" : "wrmsr", rc));
        break;
    }

    case SVM_EXIT_MONITOR:
    case SVM_EXIT_PAUSE:
    case SVM_EXIT_MWAIT_ARMED:
    case SVM_EXIT_TASK_SWITCH:          /* can change CR3; emulate */
        rc = VINF_EM_RAW_EXCEPTION_PRIVILEGED;
        break;

    case SVM_EXIT_SHUTDOWN:
        rc = VINF_EM_RESET;             /* Triple fault equals a reset. */
        break;

    case SVM_EXIT_IDTR_READ:
    case SVM_EXIT_GDTR_READ:
    case SVM_EXIT_LDTR_READ:
    case SVM_EXIT_TR_READ:
    case SVM_EXIT_IDTR_WRITE:
    case SVM_EXIT_GDTR_WRITE:
    case SVM_EXIT_LDTR_WRITE:
    case SVM_EXIT_TR_WRITE:
    case SVM_EXIT_CR0_SEL_WRITE:
    default:
        /* Unexpected exit codes. */
        rc = VERR_EM_INTERNAL_ERROR;
        AssertMsgFailed(("Unexpected exit code %x\n", exitCode));                 /* Can't happen. */
        break;
    }

end:

    /* Signal changes for the recompiler. */
    CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_SYSENTER_MSR | CPUM_CHANGED_LDTR | CPUM_CHANGED_GDTR | CPUM_CHANGED_IDTR | CPUM_CHANGED_TR | CPUM_CHANGED_HIDDEN_SEL_REGS);

    /* If we executed vmrun and an external irq was pending, then we don't have to do a full sync the next time. */
    if (exitCode == SVM_EXIT_INTR)
    {
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatPendingHostIrq);
        /* On the next entry we'll only sync the host context. */
        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_HOST_CONTEXT;
    }
    else
    {
        /* On the next entry we'll sync everything. */
        /** @todo we can do better than this */
        /* Not in the VINF_PGM_CHANGE_MODE though! */
        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_ALL;
    }

    /* translate into a less severe return code */
    if (rc == VERR_EM_INTERPRETER)
        rc = VINF_EM_RAW_EMULATE_INSTR;

    /* Just set the correct state here instead of trying to catch every goto above. */
    VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC);

    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
    return rc;
}

/**
 * Enters the AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   pCpu        CPU info struct
 */
VMMR0DECL(int) SVMR0Enter(PVM pVM, PVMCPU pVCpu, PHWACCM_CPUINFO pCpu)
{
    Assert(pVM->hwaccm.s.svm.fSupported);

    LogFlow(("SVMR0Enter cpu%d last=%d asid=%d\n", pCpu->idCpu, pVCpu->hwaccm.s.idLastCpu, pVCpu->hwaccm.s.uCurrentASID));
    pVCpu->hwaccm.s.fResumeVM = false;

    /* Force to reload LDTR, so we'll execute VMLoad to load additional guest state. */
    pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_LDTR;

    return VINF_SUCCESS;
}


/**
 * Leaves the AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   pCtx        CPU context
 */
VMMR0DECL(int) SVMR0Leave(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    SVM_VMCB *pVMCB = (SVM_VMCB *)pVCpu->hwaccm.s.svm.pVMCB;

    Assert(pVM->hwaccm.s.svm.fSupported);

    /* Save the guest debug state if necessary. */
    if (CPUMIsGuestDebugStateActive(pVCpu))
    {
        CPUMR0SaveGuestDebugState(pVM, pVCpu, pCtx, false /* skip DR6 */);

        /* Intercept all DRx reads and writes again. Changed later on. */
        pVMCB->ctrl.u16InterceptRdDRx = 0xFFFF;
        pVMCB->ctrl.u16InterceptWrDRx = 0xFFFF;

        /* Resync the debug registers the next time. */
        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_DEBUG;
    }
    else
        Assert(pVMCB->ctrl.u16InterceptRdDRx == 0xFFFF && pVMCB->ctrl.u16InterceptWrDRx == 0xFFFF);

    return VINF_SUCCESS;
}


static int svmR0InterpretInvlPg(PVMCPU pVCpu, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, uint32_t uASID)
{
    OP_PARAMVAL param1;
    RTGCPTR     addr;

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(RT_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    switch(param1.type)
    {
    case PARMTYPE_IMMEDIATE:
    case PARMTYPE_ADDRESS:
        if(!(param1.flags & (PARAM_VAL32|PARAM_VAL64)))
            return VERR_EM_INTERPRETER;
        addr = param1.val.val64;
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    /** @todo is addr always a flat linear address or ds based
     * (in absence of segment override prefixes)????
     */
    rc = PGMInvalidatePage(pVCpu, addr);
    if (RT_SUCCESS(rc))
    {
        /* Manually invalidate the page for the VM's TLB. */
        Log(("SVMR0InvlpgA %RGv ASID=%d\n", addr, uASID));
        SVMR0InvlpgA(addr, uASID);
        return VINF_SUCCESS;
    }
    Assert(rc == VERR_REM_FLUSHED_PAGES_OVERFLOW);
    return rc;
}

/**
 * Interprets INVLPG
 *
 * @returns VBox status code.
 * @retval  VINF_*                  Scheduling instructions.
 * @retval  VERR_EM_INTERPRETER     Something we can't cope with.
 * @retval  VERR_*                  Fatal errors.
 *
 * @param   pVM         The VM handle.
 * @param   pRegFrame   The register frame.
 * @param   ASID        Tagged TLB id for the guest
 *
 *                      Updates the EIP if an instruction was executed successfully.
 */
static int SVMR0InterpretInvpg(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, uint32_t uASID)
{
    /*
     * Only allow 32 & 64 bits code.
     */
    DISCPUMODE enmMode = SELMGetCpuModeFromSelector(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid);
    if (enmMode != CPUMODE_16BIT)
    {
        RTGCPTR pbCode;
        int rc = SELMValidateAndConvertCSAddr(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs, &pRegFrame->csHid, (RTGCPTR)pRegFrame->rip, &pbCode);
        if (RT_SUCCESS(rc))
        {
            uint32_t    cbOp;
            DISCPUSTATE Cpu;

            Cpu.mode = enmMode;
            rc = EMInterpretDisasOneEx(pVM, pVCpu, pbCode, pRegFrame, &Cpu, &cbOp);
            Assert(RT_FAILURE(rc) || Cpu.pCurInstr->opcode == OP_INVLPG);
            if (RT_SUCCESS(rc) && Cpu.pCurInstr->opcode == OP_INVLPG)
            {
                Assert(cbOp == Cpu.opsize);
                rc = svmR0InterpretInvlPg(pVCpu, &Cpu, pRegFrame, uASID);
                if (RT_SUCCESS(rc))
                {
                    pRegFrame->rip += cbOp; /* Move on to the next instruction. */
                }
                return rc;
            }
        }
    }
    return VERR_EM_INTERPRETER;
}


/**
 * Invalidates a guest page
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   GCVirt      Page to invalidate
 */
VMMR0DECL(int) SVMR0InvalidatePage(PVM pVM, PVMCPU pVCpu, RTGCPTR GCVirt)
{
    bool fFlushPending = pVM->hwaccm.s.svm.fAlwaysFlushTLB | pVCpu->hwaccm.s.fForceTLBFlush;

    /* Skip it if a TLB flush is already pending. */
    if (!fFlushPending)
    {
        SVM_VMCB   *pVMCB;

        Log2(("SVMR0InvalidatePage %RGv\n", GCVirt));
        AssertReturn(pVM, VERR_INVALID_PARAMETER);
        Assert(pVM->hwaccm.s.svm.fSupported);

        /* @todo SMP */
        pVMCB = (SVM_VMCB *)pVM->aCpus[0].hwaccm.s.svm.pVMCB;
        AssertMsgReturn(pVMCB, ("Invalid pVMCB\n"), VERR_EM_INTERNAL_ERROR);

#if HC_ARCH_BITS == 32
        /* If we get a flush in 64 bits guest mode, then force a full TLB flush. Invlpga takes only 32 bits addresses. */
        if (CPUMIsGuestInLongMode(pVCpu))
            pVCpu->hwaccm.s.fForceTLBFlush = true;
        else
#endif
            SVMR0InvlpgA(GCVirt, pVMCB->ctrl.TLBCtrl.n.u32ASID);
    }
    return VINF_SUCCESS;
}


#if 0 /* obsolete, but left here for clarification. */
/**
 * Invalidates a guest page by physical address
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   GCPhys      Page to invalidate
 */
VMMR0DECL(int) SVMR0InvalidatePhysPage(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys)
{
    Assert(pVM->hwaccm.s.fNestedPaging);
    /* invlpga only invalidates TLB entries for guest virtual addresses; we have no choice but to force a TLB flush here. */
    pVCpu->hwaccm.s.fForceTLBFlush = true;
    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushTLBInvlpga);
    return VINF_SUCCESS;
}
#endif

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
/**
 * Prepares for and executes VMRUN (64 bits guests from a 32 bits hosts).
 *
 * @returns VBox status code.
 * @param   pVMCBHostPhys   Physical address of host VMCB.
 * @param   pVMCBPhys       Physical address of the VMCB.
 * @param   pCtx            Guest context.
 * @param   pVM             The VM to operate on.
 * @param   pVCpu           The VMCPU to operate on.
 */
DECLASM(int) SVMR0VMSwitcherRun64(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu)
{
    uint32_t aParam[4];

    aParam[0] = (uint32_t)(pVMCBHostPhys);                  /* Param 1: pVMCBHostPhys - Lo. */
    aParam[1] = (uint32_t)(pVMCBHostPhys >> 32);            /* Param 1: pVMCBHostPhys - Hi. */
    aParam[2] = (uint32_t)(pVMCBPhys);                      /* Param 2: pVMCBPhys - Lo. */
    aParam[3] = (uint32_t)(pVMCBPhys >> 32);                /* Param 2: pVMCBPhys - Hi. */

    return SVMR0Execute64BitsHandler(pVM, pVCpu, pCtx, pVM->hwaccm.s.pfnSVMGCVMRun64, 4, &aParam[0]);
}

/**
 * Executes the specified handler in 64 mode
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        Guest context
 * @param   pfnHandler  RC handler
 * @param   cbParam     Number of parameters
 * @param   paParam     Array of 32 bits parameters
 */
VMMR0DECL(int) SVMR0Execute64BitsHandler(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, RTRCPTR pfnHandler, uint32_t cbParam, uint32_t *paParam)
{
    int             rc;
    RTHCUINTREG     uOldEFlags;

    /* @todo This code is not guest SMP safe (hyper stack) */
    AssertReturn(pVM->cCPUs == 1, VERR_ACCESS_DENIED);
    Assert(pfnHandler);

    uOldEFlags = ASMIntDisableFlags();

    CPUMSetHyperESP(pVCpu, VMMGetStackRC(pVM));
    CPUMSetHyperEIP(pVCpu, pfnHandler);
    for (int i=(int)cbParam-1;i>=0;i--)
        CPUMPushHyper(pVCpu, paParam[i]);

    STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatWorldSwitch3264, z);
    /* Call switcher. */
    rc = pVM->hwaccm.s.pfnHost32ToGuest64R0(pVM);
    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatWorldSwitch3264, z);

    ASMSetFlags(uOldEFlags);
    return rc;
}

#endif /* HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) */
