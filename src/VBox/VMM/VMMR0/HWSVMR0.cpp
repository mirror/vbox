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
#include "HWSVMR0.h"

static int SVMR0InterpretInvpg(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t uASID);

/**
 * Sets up and activates AMD-V on the current CPU
 *
 * @returns VBox status code.
 * @param   pCpu            CPU info struct
 * @param   pVM             The VM to operate on.
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
HWACCMR0DECL(int) SVMR0EnableCpu(PHWACCM_CPUINFO pCpu, PVM pVM, void *pvPageCpu, RTHCPHYS pPageCpuPhys)
{
    AssertReturn(pPageCpuPhys, VERR_INVALID_PARAMETER);
    AssertReturn(pVM, VERR_INVALID_PARAMETER);
    AssertReturn(pvPageCpu, VERR_INVALID_PARAMETER);

    /* We must turn on AMD-V and setup the host state physical address, as those MSRs are per-cpu/core. */

#ifdef LOG_ENABLED
    SUPR0Printf("SVMR0EnableCpu cpu %d page (%x) %x\n", pCpu->idCpu, pvPageCpu, (uint32_t)pPageCpuPhys);
#endif

    /* Turn on AMD-V in the EFER MSR. */
    uint64_t val = ASMRdMsr(MSR_K6_EFER);
    if (!(val & MSR_K6_EFER_SVME))
        ASMWrMsr(MSR_K6_EFER, val | MSR_K6_EFER_SVME);

    /* Write the physical page address where the CPU will store the host state while executing the VM. */
    ASMWrMsr(MSR_K8_VM_HSAVE_PA, pPageCpuPhys);

    pCpu->uCurrentASID = 0;   /* we'll aways increment this the first time (host uses ASID 0) */
    pCpu->cTLBFlushes  = 0;
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
HWACCMR0DECL(int) SVMR0DisableCpu(PHWACCM_CPUINFO pCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys)
{
    AssertReturn(pPageCpuPhys, VERR_INVALID_PARAMETER);
    AssertReturn(pvPageCpu, VERR_INVALID_PARAMETER);

#ifdef LOG_ENABLED
    SUPR0Printf("SVMR0DisableCpu cpu %d\n", pCpu->idCpu);
#endif

    /* Turn off AMD-V in the EFER MSR. */
    uint64_t val = ASMRdMsr(MSR_K6_EFER);
    ASMWrMsr(MSR_K6_EFER, val & ~MSR_K6_EFER_SVME);

    /* Invalidate host state physical address. */
    ASMWrMsr(MSR_K8_VM_HSAVE_PA, 0);
    pCpu->uCurrentASID = 0;

    return VINF_SUCCESS;
}

/**
 * Does Ring-0 per VM AMD-V init.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0InitVM(PVM pVM)
{
    int rc;

    /* Allocate one page for the VM control block (VMCB). */
    rc = RTR0MemObjAllocCont(&pVM->hwaccm.s.svm.pMemObjVMCB, 1 << PAGE_SHIFT, true /* executable R0 mapping */);
    if (RT_FAILURE(rc))
        return rc;

    pVM->hwaccm.s.svm.pVMCB     = RTR0MemObjAddress(pVM->hwaccm.s.svm.pMemObjVMCB);
    pVM->hwaccm.s.svm.pVMCBPhys = RTR0MemObjGetPagePhysAddr(pVM->hwaccm.s.svm.pMemObjVMCB, 0);
    ASMMemZero32(pVM->hwaccm.s.svm.pVMCB, PAGE_SIZE);

    /* Allocate one page for the host context */
    rc = RTR0MemObjAllocCont(&pVM->hwaccm.s.svm.pMemObjVMCBHost, 1 << PAGE_SHIFT, true /* executable R0 mapping */);
    if (RT_FAILURE(rc))
        return rc;

    pVM->hwaccm.s.svm.pVMCBHost     = RTR0MemObjAddress(pVM->hwaccm.s.svm.pMemObjVMCBHost);
    pVM->hwaccm.s.svm.pVMCBHostPhys = RTR0MemObjGetPagePhysAddr(pVM->hwaccm.s.svm.pMemObjVMCBHost, 0);
    ASMMemZero32(pVM->hwaccm.s.svm.pVMCBHost, PAGE_SIZE);

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

    /* Invalidate the last cpu we were running on. */
    pVM->hwaccm.s.svm.idLastCpu = NIL_RTCPUID;
    return VINF_SUCCESS;
}

/**
 * Does Ring-0 per VM AMD-V termination.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0TermVM(PVM pVM)
{
    if (pVM->hwaccm.s.svm.pMemObjVMCB)
    {
        RTR0MemObjFree(pVM->hwaccm.s.svm.pMemObjVMCB, false);
        pVM->hwaccm.s.svm.pVMCB       = 0;
        pVM->hwaccm.s.svm.pVMCBPhys   = 0;
        pVM->hwaccm.s.svm.pMemObjVMCB = 0;
    }
    if (pVM->hwaccm.s.svm.pMemObjVMCBHost)
    {
        RTR0MemObjFree(pVM->hwaccm.s.svm.pMemObjVMCBHost, false);
        pVM->hwaccm.s.svm.pVMCBHost       = 0;
        pVM->hwaccm.s.svm.pVMCBHostPhys   = 0;
        pVM->hwaccm.s.svm.pMemObjVMCBHost = 0;
    }
    if (pVM->hwaccm.s.svm.pMemObjIOBitmap)
    {
        RTR0MemObjFree(pVM->hwaccm.s.svm.pMemObjIOBitmap, false);
        pVM->hwaccm.s.svm.pIOBitmap       = 0;
        pVM->hwaccm.s.svm.pIOBitmapPhys   = 0;
        pVM->hwaccm.s.svm.pMemObjIOBitmap = 0;
    }
    if (pVM->hwaccm.s.svm.pMemObjMSRBitmap)
    {
        RTR0MemObjFree(pVM->hwaccm.s.svm.pMemObjMSRBitmap, false);
        pVM->hwaccm.s.svm.pMSRBitmap       = 0;
        pVM->hwaccm.s.svm.pMSRBitmapPhys   = 0;
        pVM->hwaccm.s.svm.pMemObjMSRBitmap = 0;
    }
    return VINF_SUCCESS;
}

/**
 * Sets up AMD-V for the specified VM
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0SetupVM(PVM pVM)
{
    int         rc = VINF_SUCCESS;
    SVM_VMCB   *pVMCB;

    AssertReturn(pVM, VERR_INVALID_PARAMETER);

    Assert(pVM->hwaccm.s.svm.fSupported);

    pVMCB = (SVM_VMCB *)pVM->hwaccm.s.svm.pVMCB;
    AssertMsgReturn(pVMCB, ("Invalid pVMCB\n"), VERR_EM_INTERNAL_ERROR);

    /* Program the control fields. Most of them never have to be changed again. */
    /* CR0/3/4 reads must be intercepted, our shadow values are not necessarily the same as the guest's. */
    /* Note: CR8 reads will refer to V_TPR, so no need to catch them. */
    /** @note CR0 & CR4 can be safely read when guest and shadow copies are identical. */
    if (!pVM->hwaccm.s.svm.fNestedPaging)
        pVMCB->ctrl.u16InterceptRdCRx = RT_BIT(0) | RT_BIT(3) | RT_BIT(4);
    else    
        pVMCB->ctrl.u16InterceptRdCRx = RT_BIT(0);

    /*
     * CR0/3/4 writes must be intercepted for obvious reasons.
     */
    if (!pVM->hwaccm.s.svm.fNestedPaging)
        pVMCB->ctrl.u16InterceptWrCRx = RT_BIT(0) | RT_BIT(3) | RT_BIT(4) | RT_BIT(8);
    else
        pVMCB->ctrl.u16InterceptWrCRx = RT_BIT(0) | RT_BIT(8);

    /* Intercept all DRx reads and writes. */
    pVMCB->ctrl.u16InterceptRdDRx = RT_BIT(0) | RT_BIT(1) | RT_BIT(2) | RT_BIT(3) | RT_BIT(4) | RT_BIT(5) | RT_BIT(6) | RT_BIT(7);
    pVMCB->ctrl.u16InterceptWrDRx = RT_BIT(0) | RT_BIT(1) | RT_BIT(2) | RT_BIT(3) | RT_BIT(4) | RT_BIT(5) | RT_BIT(6) | RT_BIT(7);

    /* Currently we don't care about DRx reads or writes. DRx registers are trashed.
     * All breakpoints are automatically cleared when the VM exits.
     */

    pVMCB->ctrl.u32InterceptException = HWACCM_SVM_TRAP_MASK;
    if (pVM->hwaccm.s.svm.fNestedPaging)
        pVMCB->ctrl.u32InterceptException &= ~RT_BIT(14);   /* no longer need to intercept #PF. */

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
    if (pVM->hwaccm.s.svm.fNestedPaging)
        pVMCB->ctrl.u32InterceptCtrl1 &= ~SVM_CTRL1_INTERCEPT_INVLPG;

    pVMCB->ctrl.u32InterceptCtrl2 =   SVM_CTRL2_INTERCEPT_VMRUN         /* required */
                                    | SVM_CTRL2_INTERCEPT_VMMCALL
                                    | SVM_CTRL2_INTERCEPT_VMLOAD
                                    | SVM_CTRL2_INTERCEPT_VMSAVE
                                    | SVM_CTRL2_INTERCEPT_STGI
                                    | SVM_CTRL2_INTERCEPT_CLGI
                                    | SVM_CTRL2_INTERCEPT_SKINIT
                                    | SVM_CTRL2_INTERCEPT_RDTSCP        /* AMD only; we don't support this one */
                                    | SVM_CTRL2_INTERCEPT_WBINVD
                                    | SVM_CTRL2_INTERCEPT_MWAIT_UNCOND; /* don't execute mwait or else we'll idle inside the guest (host thinks the cpu load is high) */
                                    ;
    Log(("pVMCB->ctrl.u32InterceptException = %x\n", pVMCB->ctrl.u32InterceptException));
    Log(("pVMCB->ctrl.u32InterceptCtrl1 = %x\n", pVMCB->ctrl.u32InterceptCtrl1));
    Log(("pVMCB->ctrl.u32InterceptCtrl2 = %x\n", pVMCB->ctrl.u32InterceptCtrl2));

    /* Virtualize masking of INTR interrupts. (reads/writes from/to CR8 go to the V_TPR register) */
    pVMCB->ctrl.IntCtrl.n.u1VIrqMasking = 1;

    /* Set IO and MSR bitmap addresses. */
    pVMCB->ctrl.u64IOPMPhysAddr  = pVM->hwaccm.s.svm.pIOBitmapPhys;
    pVMCB->ctrl.u64MSRPMPhysAddr = pVM->hwaccm.s.svm.pMSRBitmapPhys;

    /* No LBR virtualization. */
    pVMCB->ctrl.u64LBRVirt      = 0;

    /** The ASID must start at 1; the host uses 0. */
    pVMCB->ctrl.TLBCtrl.n.u32ASID = 1;

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
        Log(("SVM: Inject int %d at %VGv error code=%08x CR2=%08x intInfo=%08x\n", pEvent->n.u8Vector, pCtx->eip, pEvent->n.u32ErrorCode, pCtx->cr2, pEvent->au64[0]));
    else
    if (pEvent->n.u8Vector < 0x20)
        Log(("SVM: Inject int %d at %VGv error code=%08x\n", pEvent->n.u8Vector, pCtx->eip, pEvent->n.u32ErrorCode));
    else
    {
        Log(("INJ-EI: %x at %VGv\n", pEvent->n.u8Vector, pCtx->eip));
        Assert(!VM_FF_ISSET(pVM, VM_FF_INHIBIT_INTERRUPTS));
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
 * @param   pVMCB       SVM control block
 * @param   pCtx        CPU Context
 */
static int SVMR0CheckPendingInterrupt(PVM pVM, SVM_VMCB *pVMCB, CPUMCTX *pCtx)
{
    int rc;

    /* Dispatch any pending interrupts. (injected before, but a VM exit occurred prematurely) */
    if (pVM->hwaccm.s.Event.fPending)
    {
        SVM_EVENT Event;

        Log(("Reinjecting event %08x %08x at %VGv\n", pVM->hwaccm.s.Event.intInfo, pVM->hwaccm.s.Event.errCode, pCtx->eip));
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatIntReinject);
        Event.au64[0] = pVM->hwaccm.s.Event.intInfo;
        SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

        pVM->hwaccm.s.Event.fPending = false;
        return VINF_SUCCESS;
    }

    /* When external interrupts are pending, we should exit the VM when IF is set. */
    if (    !TRPMHasTrap(pVM)
        &&  VM_FF_ISPENDING(pVM, (VM_FF_INTERRUPT_APIC|VM_FF_INTERRUPT_PIC)))
    {
        if (!(pCtx->eflags.u32 & X86_EFL_IF))
        {
            if (!pVMCB->ctrl.IntCtrl.n.u1VIrqValid)
            {
                Log(("Enable irq window exit!\n"));
                /** @todo use virtual interrupt method to inject a pending irq; dispatched as soon as guest.IF is set. */
                pVMCB->ctrl.u32InterceptCtrl1 |= SVM_CTRL1_INTERCEPT_VINTR;
                pVMCB->ctrl.IntCtrl.n.u1VIrqValid    = 1;
                pVMCB->ctrl.IntCtrl.n.u1IgnoreTPR    = 1; /* ignore the priority in the TPR; just deliver it */
                pVMCB->ctrl.IntCtrl.n.u8VIrqVector   = 0; /* don't care */
            }
        }
        else
        if (!VM_FF_ISSET(pVM, VM_FF_INHIBIT_INTERRUPTS))
        {
            uint8_t u8Interrupt;

            rc = PDMGetInterrupt(pVM, &u8Interrupt);
            Log(("Dispatch interrupt: u8Interrupt=%x (%d) rc=%Vrc\n", u8Interrupt, u8Interrupt, rc));
            if (VBOX_SUCCESS(rc))
            {
                rc = TRPMAssertTrap(pVM, u8Interrupt, TRPM_HARDWARE_INT);
                AssertRC(rc);
            }
            else
            {
                /* Can only happen in rare cases where a pending interrupt is cleared behind our back */
                Assert(!VM_FF_ISPENDING(pVM, (VM_FF_INTERRUPT_APIC|VM_FF_INTERRUPT_PIC)));
                STAM_COUNTER_INC(&pVM->hwaccm.s.StatSwitchGuestIrq);
                /* Just continue */
            }
        }
        else
            Log(("Pending interrupt blocked at %VGv by VM_FF_INHIBIT_INTERRUPTS!!\n", pCtx->eip));
    }

#ifdef VBOX_STRICT
    if (TRPMHasTrap(pVM))
    {
        uint8_t     u8Vector;
        rc = TRPMQueryTrapAll(pVM, &u8Vector, 0, 0, 0);
        AssertRC(rc);
    }
#endif

    if (    pCtx->eflags.u32 & X86_EFL_IF
        && (!VM_FF_ISSET(pVM, VM_FF_INHIBIT_INTERRUPTS))
        && TRPMHasTrap(pVM)
       )
    {
        uint8_t     u8Vector;
        int         rc;
        TRPMEVENT   enmType;
        SVM_EVENT   Event;
        uint32_t    u32ErrorCode;

        Event.au64[0] = 0;

        /* If a new event is pending, then dispatch it now. */
        rc = TRPMQueryTrapAll(pVM, &u8Vector, &enmType, &u32ErrorCode, 0);
        AssertRC(rc);
        Assert(pCtx->eflags.Bits.u1IF == 1 || enmType == TRPM_TRAP);
        Assert(enmType != TRPM_SOFTWARE_INT);

        /* Clear the pending trap. */
        rc = TRPMResetTrap(pVM);
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

        STAM_COUNTER_INC(&pVM->hwaccm.s.StatIntInject);
        SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);
    } /* if (interrupts can be dispatched) */

    return VINF_SUCCESS;
}


/**
 * Loads the guest state
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Guest context
 */
HWACCMR0DECL(int) SVMR0LoadGuestState(PVM pVM, CPUMCTX *pCtx)
{
    RTGCUINTPTR val;
    SVM_VMCB *pVMCB;

    if (pVM == NULL)
        return VERR_INVALID_PARAMETER;

    /* Setup AMD SVM. */
    Assert(pVM->hwaccm.s.svm.fSupported);

    pVMCB = (SVM_VMCB *)pVM->hwaccm.s.svm.pVMCB;
    AssertMsgReturn(pVMCB, ("Invalid pVMCB\n"), VERR_EM_INTERNAL_ERROR);

    /* Guest CPU context: ES, CS, SS, DS, FS, GS. */
    if (pVM->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_SEGMENT_REGS)
    {
        SVM_WRITE_SELREG(CS, cs);
        SVM_WRITE_SELREG(SS, ss);
        SVM_WRITE_SELREG(DS, ds);
        SVM_WRITE_SELREG(ES, es);
        SVM_WRITE_SELREG(FS, fs);
        SVM_WRITE_SELREG(GS, gs);
    }

    /* Guest CPU context: LDTR. */
    if (pVM->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_LDTR)
    {
        SVM_WRITE_SELREG(LDTR, ldtr);
    }

    /* Guest CPU context: TR. */
    if (pVM->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_TR)
    {
        SVM_WRITE_SELREG(TR, tr);
    }

    /* Guest CPU context: GDTR. */
    if (pVM->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_GDTR)
    {
        pVMCB->guest.GDTR.u32Limit = pCtx->gdtr.cbGdt;
        pVMCB->guest.GDTR.u64Base  = pCtx->gdtr.pGdt;
    }

    /* Guest CPU context: IDTR. */
    if (pVM->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_IDTR)
    {
        pVMCB->guest.IDTR.u32Limit = pCtx->idtr.cbIdt;
        pVMCB->guest.IDTR.u64Base  = pCtx->idtr.pIdt;
    }

    /*
     * Sysenter MSRs
     */
    if (pVM->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_SYSENTER_MSR)
    {
        pVMCB->guest.u64SysEnterCS  = pCtx->SysEnter.cs;
        pVMCB->guest.u64SysEnterEIP = pCtx->SysEnter.eip;
        pVMCB->guest.u64SysEnterESP = pCtx->SysEnter.esp;
    }

    /* Control registers */
    if (pVM->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_CR0)
    {
        val = pCtx->cr0;
        if (CPUMIsGuestFPUStateActive(pVM) == false)
        {
            /* Always use #NM exceptions to load the FPU/XMM state on demand. */
            val |= X86_CR0_TS | X86_CR0_ET | X86_CR0_NE | X86_CR0_MP;
        }
        else
        {
            Assert(pVM->hwaccm.s.svm.fResumeVM == true);
            /** @todo check if we support the old style mess correctly. */
            if (!(val & X86_CR0_NE))
            {
                Log(("Forcing X86_CR0_NE!!!\n"));

                /* Also catch floating point exceptions as we need to report them to the guest in a different way. */
                if (!pVM->hwaccm.s.fFPUOldStyleOverride)
                {
                    pVMCB->ctrl.u32InterceptException |= RT_BIT(16);
                    pVM->hwaccm.s.fFPUOldStyleOverride = true;
                }
            }
            val |= X86_CR0_NE;  /* always turn on the native mechanism to report FPU errors (old style uses interrupts) */
        }
        /* Always enable caching. */
        val &= ~(X86_CR0_CD|X86_CR0_NW);

        /* Note: WP is not relevant in nested paging mode as we catch accesses on the (host) physical level. */
        /* Note: In nested paging mode the guest is allowed to run with paging disabled; the guest physical to host physical translation will remain active. */
        if (!pVM->hwaccm.s.svm.fNestedPaging)
        {
            val |= X86_CR0_PG;          /* Paging is always enabled; even when the guest is running in real mode or PE without paging. */
            val |= X86_CR0_WP;          /* Must set this as we rely on protect various pages and supervisor writes must be caught. */
        }
        pVMCB->guest.u64CR0 = val;
    }
    /* CR2 as well */
    pVMCB->guest.u64CR2 = pCtx->cr2;

    if (pVM->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_CR3)
    {
        /* Save our shadow CR3 register. */
        if (pVM->hwaccm.s.svm.fNestedPaging)
        {
            pVMCB->ctrl.u64NestedPagingCR3  = PGMGetHyperCR3(pVM);
            pVMCB->guest.u64CR3             = pCtx->cr3;
        }
        else
            pVMCB->guest.u64CR3             = PGMGetHyperCR3(pVM);
    }

    if (pVM->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_CR4)
    {
        val = pCtx->cr4;
        if (!pVM->hwaccm.s.svm.fNestedPaging)
        {
            switch(pVM->hwaccm.s.enmShadowMode)
            {
            case PGMMODE_REAL:
            case PGMMODE_PROTECTED:     /* Protected mode, no paging. */
                AssertFailed();
                return VERR_PGM_UNSUPPORTED_HOST_PAGING_MODE;

            case PGMMODE_32_BIT:        /* 32-bit paging. */
                break;

            case PGMMODE_PAE:           /* PAE paging. */
            case PGMMODE_PAE_NX:        /* PAE paging with NX enabled. */
                /** @todo use normal 32 bits paging */
                val |= X86_CR4_PAE;
                break;

            case PGMMODE_AMD64:         /* 64-bit AMD paging (long mode). */
            case PGMMODE_AMD64_NX:      /* 64-bit AMD paging (long mode) with NX enabled. */
                AssertFailed();
                return VERR_PGM_UNSUPPORTED_HOST_PAGING_MODE;

            default:                    /* shut up gcc */
                AssertFailed();
                return VERR_PGM_UNSUPPORTED_HOST_PAGING_MODE;
            }
        }
        pVMCB->guest.u64CR4 = val;
    }

    /* Debug registers. */
    if (pVM->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_DEBUG)
    {
        /** @todo DR0-6 */
        val  = pCtx->dr7;
        val &= ~(RT_BIT(11) | RT_BIT(12) | RT_BIT(14) | RT_BIT(15));    /* must be zero */
        val |= 0x400;                                       /* must be one */
#ifdef VBOX_STRICT
        val = 0x400;
#endif
        pVMCB->guest.u64DR7 = val;

        pVMCB->guest.u64DR6 = pCtx->dr6;
    }

    /* EIP, ESP and EFLAGS */
    pVMCB->guest.u64RIP    = pCtx->eip;
    pVMCB->guest.u64RSP    = pCtx->esp;
    pVMCB->guest.u64RFlags = pCtx->eflags.u32;

    /* Set CPL */
    pVMCB->guest.u8CPL     = pCtx->ssHid.Attr.n.u2Dpl;

    /* RAX/EAX too, as VMRUN uses RAX as an implicit parameter. */
    pVMCB->guest.u64RAX    = pCtx->eax;

    /* vmrun will fail otherwise. */
    pVMCB->guest.u64EFER   = MSR_K6_EFER_SVME;

    /** TSC offset. */
    if (TMCpuTickCanUseRealTSC(pVM, &pVMCB->ctrl.u64TSCOffset))
        pVMCB->ctrl.u32InterceptCtrl1 &= ~SVM_CTRL1_INTERCEPT_RDTSC;
    else
        pVMCB->ctrl.u32InterceptCtrl1 |= SVM_CTRL1_INTERCEPT_RDTSC;

    /** @todo 64 bits stuff (?):
     * - STAR
     * - LSTAR
     * - CSTAR
     * - SFMASK
     * - KernelGSBase
     */

#ifdef DEBUG
    /* Intercept X86_XCPT_DB if stepping is enabled */
    if (DBGFIsStepping(pVM))
        pVMCB->ctrl.u32InterceptException |=  RT_BIT(1);
    else
        pVMCB->ctrl.u32InterceptException &= ~RT_BIT(1);
#endif

    /* Done. */
    pVM->hwaccm.s.fContextUseFlags &= ~HWACCM_CHANGED_ALL_GUEST;

    return VINF_SUCCESS;
}


/**
 * Runs guest code in an SVM VM.
 *
 * @todo This can be much more efficient, when we only sync that which has actually changed. (this is the first attempt only)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Guest context
 * @param   pCpu        CPU info struct
 */
HWACCMR0DECL(int) SVMR0RunGuestCode(PVM pVM, CPUMCTX *pCtx, PHWACCM_CPUINFO pCpu)
{
    int         rc = VINF_SUCCESS;
    uint64_t    exitCode = (uint64_t)SVM_EXIT_INVALID;
    SVM_VMCB   *pVMCB;
    bool        fGuestStateSynced = false;
    unsigned    cResume = 0;

    STAM_PROFILE_ADV_START(&pVM->hwaccm.s.StatEntry, x);

    Assert(!pVM->hwaccm.s.svm.fNestedPaging);
    AssertReturn(pCpu->fSVMConfigured, VERR_EM_INTERNAL_ERROR);

    pVMCB = (SVM_VMCB *)pVM->hwaccm.s.svm.pVMCB;
    AssertMsgReturn(pVMCB, ("Invalid pVMCB\n"), VERR_EM_INTERNAL_ERROR);

    /* We can jump to this point to resume execution after determining that a VM-exit is innocent.
     */
ResumeExecution:
    /* Safety precaution; looping for too long here can have a very bad effect on the host */
    if (++cResume > HWACCM_MAX_RESUME_LOOPS)
    {
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitMaxResume);
        rc = VINF_EM_RAW_INTERRUPT;
        goto end;
    }

    /* Check for irq inhibition due to instruction fusing (sti, mov ss). */
    if (VM_FF_ISSET(pVM, VM_FF_INHIBIT_INTERRUPTS))
    {
        Log(("VM_FF_INHIBIT_INTERRUPTS at %VGv successor %VGv\n", pCtx->eip, EMGetInhibitInterruptsPC(pVM)));
        if (pCtx->eip != EMGetInhibitInterruptsPC(pVM))
        {
            /** @note we intentionally don't clear VM_FF_INHIBIT_INTERRUPTS here.
             *  Before we are able to execute this instruction in raw mode (iret to guest code) an external interrupt might
             *  force a world switch again. Possibly allowing a guest interrupt to be dispatched in the process. This could
             *  break the guest. Sounds very unlikely, but such timing sensitive problem are not as rare as you might think.
             */
            VM_FF_CLEAR(pVM, VM_FF_INHIBIT_INTERRUPTS);
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
    if (!DBGFIsStepping(pVM))
#endif
    {
        if (VM_FF_ISPENDING(pVM, VM_FF_TO_R3 | VM_FF_TIMER))
        {
            VM_FF_CLEAR(pVM, VM_FF_TO_R3);
            STAM_COUNTER_INC(&pVM->hwaccm.s.StatSwitchToR3);
            STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatEntry, x);
            rc = VINF_EM_RAW_TO_R3;
            goto end;
        }
    }

    /* Pending request packets might contain actions that need immediate attention, such as pending hardware interrupts. */
    if (VM_FF_ISPENDING(pVM, VM_FF_REQUEST))
    {
        STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatEntry, x);
        rc = VINF_EM_PENDING_REQUEST;
        goto end;
    }

    /* When external interrupts are pending, we should exit the VM when IF is set. */
    /** @note *after* VM_FF_INHIBIT_INTERRUPTS check!!! */
    rc = SVMR0CheckPendingInterrupt(pVM, pVMCB, pCtx);
    if (VBOX_FAILURE(rc))
    {
        STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatEntry, x);
        goto end;
    }

    /* Load the guest state */
    rc = SVMR0LoadGuestState(pVM, pCtx);
    if (rc != VINF_SUCCESS)
    {
        STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatEntry, x);
        goto end;
    }
    fGuestStateSynced = true;

    /* All done! Let's start VM execution. */
    STAM_PROFILE_ADV_START(&pVM->hwaccm.s.StatInGC, x);

    /* Enable nested paging if necessary (disabled each time after #VMEXIT). */
    pVMCB->ctrl.NestedPaging.n.u1NestedPaging = pVM->hwaccm.s.svm.fNestedPaging;

    /* Force a TLB flush for the first world switch if the current cpu differs from the one we ran on last. */
    if (!pVM->hwaccm.s.svm.fResumeVM)
    {
        if (    pVM->hwaccm.s.svm.idLastCpu != pCpu->idCpu
                /* if the tlb flush count has changed, another VM has flushed the TLB of this cpu, so we can't use our current ASID anymore. */
            ||  pVM->hwaccm.s.svm.cTLBFlushes != pCpu->cTLBFlushes)
        {
            /* Force a TLB flush on VM entry. */
            pVM->hwaccm.s.svm.fForceTLBFlush = true;
        }
        pVM->hwaccm.s.svm.idLastCpu = pCpu->idCpu;
    }

    /* Make sure we flush the TLB when required. Switch ASID to achieve the same thing, but without actually flushing the whole TLB (which is expensive). */
    if (    pVM->hwaccm.s.svm.fForceTLBFlush
        && !pVM->hwaccm.s.svm.fAlwaysFlushTLB)
    {
        if (++pCpu->uCurrentASID >= pVM->hwaccm.s.svm.u32MaxASID)
        {
            pCpu->uCurrentASID               = 1;       /* start at 1; host uses 0 */
            pVMCB->ctrl.TLBCtrl.n.u1TLBFlush = 1;       /* wrap around; flush TLB */
            pCpu->cTLBFlushes++;
        }
        else
            STAM_COUNTER_INC(&pVM->hwaccm.s.StatFlushASID);

        pVM->hwaccm.s.svm.cTLBFlushes = pCpu->cTLBFlushes;
    }
    else
    {
        /* We never increase uCurrentASID in the fAlwaysFlushTLB (erratum 170) case. */
        if (!pCpu->uCurrentASID)
            pCpu->uCurrentASID = 1;

        pVMCB->ctrl.TLBCtrl.n.u1TLBFlush = pVM->hwaccm.s.svm.fForceTLBFlush;
    }

    AssertMsg(pCpu->uCurrentASID >= 1 && pCpu->uCurrentASID < pVM->hwaccm.s.svm.u32MaxASID, ("cpu%d uCurrentASID = %x\n", pCpu->idCpu, pCpu->uCurrentASID));
    pVMCB->ctrl.TLBCtrl.n.u32ASID = pCpu->uCurrentASID;

#ifdef VBOX_WITH_STATISTICS
    if (pVMCB->ctrl.TLBCtrl.n.u1TLBFlush)
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatFlushTLBWorldSwitch);
    else
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatNoFlushTLBWorldSwitch);
#endif

    /* In case we execute a goto ResumeExecution later on. */
    pVM->hwaccm.s.svm.fResumeVM      = true;
    pVM->hwaccm.s.svm.fForceTLBFlush = pVM->hwaccm.s.svm.fAlwaysFlushTLB;

    Assert(sizeof(pVM->hwaccm.s.svm.pVMCBPhys) == 8);
    Assert(pVMCB->ctrl.u32InterceptCtrl2 == ( SVM_CTRL2_INTERCEPT_VMRUN         /* required */
                                             | SVM_CTRL2_INTERCEPT_VMMCALL
                                             | SVM_CTRL2_INTERCEPT_VMLOAD
                                             | SVM_CTRL2_INTERCEPT_VMSAVE
                                             | SVM_CTRL2_INTERCEPT_STGI
                                             | SVM_CTRL2_INTERCEPT_CLGI
                                             | SVM_CTRL2_INTERCEPT_SKINIT
                                             | SVM_CTRL2_INTERCEPT_RDTSCP        /* AMD only; we don't support this one */
                                             | SVM_CTRL2_INTERCEPT_WBINVD
                                             | SVM_CTRL2_INTERCEPT_MWAIT_UNCOND  /* don't execute mwait or else we'll idle inside the guest (host thinks the cpu load is high) */
                                            ));
    Assert(pVMCB->ctrl.IntCtrl.n.u1VIrqMasking);
    Assert(pVMCB->ctrl.u64IOPMPhysAddr  == pVM->hwaccm.s.svm.pIOBitmapPhys);
    Assert(pVMCB->ctrl.u64MSRPMPhysAddr == pVM->hwaccm.s.svm.pMSRBitmapPhys);
    Assert(pVMCB->ctrl.u64LBRVirt == 0);

    SVMVMRun(pVM->hwaccm.s.svm.pVMCBHostPhys, pVM->hwaccm.s.svm.pVMCBPhys, pCtx);
    STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatInGC, x);

    /**
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * IMPORTANT: WE CAN'T DO ANY LOGGING OR OPERATIONS THAT CAN DO A LONGJMP BACK TO RING 3 *BEFORE* WE'VE SYNCED BACK (MOST OF) THE GUEST STATE
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */

    STAM_PROFILE_ADV_START(&pVM->hwaccm.s.StatExit, x);

    /* Reason for the VM exit */
    exitCode = pVMCB->ctrl.u64ExitCode;

    if (exitCode == (uint64_t)SVM_EXIT_INVALID)          /* Invalid guest state. */
    {
        HWACCMDumpRegs(pCtx);
#ifdef DEBUG
        Log(("ctrl.u16InterceptRdCRx            %x\n",      pVMCB->ctrl.u16InterceptRdCRx));
        Log(("ctrl.u16InterceptWrCRx            %x\n",      pVMCB->ctrl.u16InterceptWrCRx));
        Log(("ctrl.u16InterceptRdDRx            %x\n",      pVMCB->ctrl.u16InterceptRdDRx));
        Log(("ctrl.u16InterceptWrDRx            %x\n",      pVMCB->ctrl.u16InterceptWrDRx));
        Log(("ctrl.u32InterceptException        %x\n",      pVMCB->ctrl.u32InterceptException));
        Log(("ctrl.u32InterceptCtrl1            %x\n",      pVMCB->ctrl.u32InterceptCtrl1));
        Log(("ctrl.u32InterceptCtrl2            %x\n",      pVMCB->ctrl.u32InterceptCtrl2));
        Log(("ctrl.u64IOPMPhysAddr              %VX64\n",   pVMCB->ctrl.u64IOPMPhysAddr));
        Log(("ctrl.u64MSRPMPhysAddr             %VX64\n",   pVMCB->ctrl.u64MSRPMPhysAddr));
        Log(("ctrl.u64TSCOffset                 %VX64\n",   pVMCB->ctrl.u64TSCOffset));

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

        Log(("ctrl.u64IntShadow                 %VX64\n",   pVMCB->ctrl.u64IntShadow));
        Log(("ctrl.u64ExitCode                  %VX64\n",   pVMCB->ctrl.u64ExitCode));
        Log(("ctrl.u64ExitInfo1                 %VX64\n",   pVMCB->ctrl.u64ExitInfo1));
        Log(("ctrl.u64ExitInfo2                 %VX64\n",   pVMCB->ctrl.u64ExitInfo2));
        Log(("ctrl.ExitIntInfo.u8Vector         %x\n",      pVMCB->ctrl.ExitIntInfo.n.u8Vector));
        Log(("ctrl.ExitIntInfo.u3Type           %x\n",      pVMCB->ctrl.ExitIntInfo.n.u3Type));
        Log(("ctrl.ExitIntInfo.u1ErrorCodeValid %x\n",      pVMCB->ctrl.ExitIntInfo.n.u1ErrorCodeValid));
        Log(("ctrl.ExitIntInfo.u19Reserved      %x\n",      pVMCB->ctrl.ExitIntInfo.n.u19Reserved));
        Log(("ctrl.ExitIntInfo.u1Valid          %x\n",      pVMCB->ctrl.ExitIntInfo.n.u1Valid));
        Log(("ctrl.ExitIntInfo.u32ErrorCode     %x\n",      pVMCB->ctrl.ExitIntInfo.n.u32ErrorCode));
        Log(("ctrl.NestedPaging                 %VX64\n",   pVMCB->ctrl.NestedPaging.au64));
        Log(("ctrl.EventInject.u8Vector         %x\n",      pVMCB->ctrl.EventInject.n.u8Vector));
        Log(("ctrl.EventInject.u3Type           %x\n",      pVMCB->ctrl.EventInject.n.u3Type));
        Log(("ctrl.EventInject.u1ErrorCodeValid %x\n",      pVMCB->ctrl.EventInject.n.u1ErrorCodeValid));
        Log(("ctrl.EventInject.u19Reserved      %x\n",      pVMCB->ctrl.EventInject.n.u19Reserved));
        Log(("ctrl.EventInject.u1Valid          %x\n",      pVMCB->ctrl.EventInject.n.u1Valid));
        Log(("ctrl.EventInject.u32ErrorCode     %x\n",      pVMCB->ctrl.EventInject.n.u32ErrorCode));

        Log(("ctrl.u64NestedPagingCR3           %VX64\n",   pVMCB->ctrl.u64NestedPagingCR3));
        Log(("ctrl.u64LBRVirt                   %VX64\n",   pVMCB->ctrl.u64LBRVirt));

        Log(("guest.CS.u16Sel                   %04X\n",    pVMCB->guest.CS.u16Sel));
        Log(("guest.CS.u16Attr                  %04X\n",    pVMCB->guest.CS.u16Attr));
        Log(("guest.CS.u32Limit                 %X\n",      pVMCB->guest.CS.u32Limit));
        Log(("guest.CS.u64Base                  %VX64\n",   pVMCB->guest.CS.u64Base));
        Log(("guest.DS.u16Sel                   %04X\n",    pVMCB->guest.DS.u16Sel));
        Log(("guest.DS.u16Attr                  %04X\n",    pVMCB->guest.DS.u16Attr));
        Log(("guest.DS.u32Limit                 %X\n",      pVMCB->guest.DS.u32Limit));
        Log(("guest.DS.u64Base                  %VX64\n",   pVMCB->guest.DS.u64Base));
        Log(("guest.ES.u16Sel                   %04X\n",    pVMCB->guest.ES.u16Sel));
        Log(("guest.ES.u16Attr                  %04X\n",    pVMCB->guest.ES.u16Attr));
        Log(("guest.ES.u32Limit                 %X\n",      pVMCB->guest.ES.u32Limit));
        Log(("guest.ES.u64Base                  %VX64\n",   pVMCB->guest.ES.u64Base));
        Log(("guest.FS.u16Sel                   %04X\n",    pVMCB->guest.FS.u16Sel));
        Log(("guest.FS.u16Attr                  %04X\n",    pVMCB->guest.FS.u16Attr));
        Log(("guest.FS.u32Limit                 %X\n",      pVMCB->guest.FS.u32Limit));
        Log(("guest.FS.u64Base                  %VX64\n",   pVMCB->guest.FS.u64Base));
        Log(("guest.GS.u16Sel                   %04X\n",    pVMCB->guest.GS.u16Sel));
        Log(("guest.GS.u16Attr                  %04X\n",    pVMCB->guest.GS.u16Attr));
        Log(("guest.GS.u32Limit                 %X\n",      pVMCB->guest.GS.u32Limit));
        Log(("guest.GS.u64Base                  %VX64\n",   pVMCB->guest.GS.u64Base));

        Log(("guest.GDTR.u32Limit               %X\n",      pVMCB->guest.GDTR.u32Limit));
        Log(("guest.GDTR.u64Base                %VX64\n",   pVMCB->guest.GDTR.u64Base));

        Log(("guest.LDTR.u16Sel                 %04X\n",    pVMCB->guest.LDTR.u16Sel));
        Log(("guest.LDTR.u16Attr                %04X\n",    pVMCB->guest.LDTR.u16Attr));
        Log(("guest.LDTR.u32Limit               %X\n",      pVMCB->guest.LDTR.u32Limit));
        Log(("guest.LDTR.u64Base                %VX64\n",   pVMCB->guest.LDTR.u64Base));

        Log(("guest.IDTR.u32Limit               %X\n",      pVMCB->guest.IDTR.u32Limit));
        Log(("guest.IDTR.u64Base                %VX64\n",   pVMCB->guest.IDTR.u64Base));

        Log(("guest.TR.u16Sel                   %04X\n",    pVMCB->guest.TR.u16Sel));
        Log(("guest.TR.u16Attr                  %04X\n",    pVMCB->guest.TR.u16Attr));
        Log(("guest.TR.u32Limit                 %X\n",      pVMCB->guest.TR.u32Limit));
        Log(("guest.TR.u64Base                  %VX64\n",   pVMCB->guest.TR.u64Base));

        Log(("guest.u8CPL                       %X\n",      pVMCB->guest.u8CPL));
        Log(("guest.u64CR0                      %VX64\n",   pVMCB->guest.u64CR0));
        Log(("guest.u64CR2                      %VX64\n",   pVMCB->guest.u64CR2));
        Log(("guest.u64CR3                      %VX64\n",   pVMCB->guest.u64CR3));
        Log(("guest.u64CR4                      %VX64\n",   pVMCB->guest.u64CR4));
        Log(("guest.u64DR6                      %VX64\n",   pVMCB->guest.u64DR6));
        Log(("guest.u64DR7                      %VX64\n",   pVMCB->guest.u64DR7));

        Log(("guest.u64RIP                      %VX64\n",   pVMCB->guest.u64RIP));
        Log(("guest.u64RSP                      %VX64\n",   pVMCB->guest.u64RSP));
        Log(("guest.u64RAX                      %VX64\n",   pVMCB->guest.u64RAX));
        Log(("guest.u64RFlags                   %VX64\n",   pVMCB->guest.u64RFlags));

        Log(("guest.u64SysEnterCS               %VX64\n",   pVMCB->guest.u64SysEnterCS));
        Log(("guest.u64SysEnterEIP              %VX64\n",   pVMCB->guest.u64SysEnterEIP));
        Log(("guest.u64SysEnterESP              %VX64\n",   pVMCB->guest.u64SysEnterESP));

        Log(("guest.u64EFER                     %VX64\n",   pVMCB->guest.u64EFER));
        Log(("guest.u64STAR                     %VX64\n",   pVMCB->guest.u64STAR));
        Log(("guest.u64LSTAR                    %VX64\n",   pVMCB->guest.u64LSTAR));
        Log(("guest.u64CSTAR                    %VX64\n",   pVMCB->guest.u64CSTAR));
        Log(("guest.u64SFMASK                   %VX64\n",   pVMCB->guest.u64SFMASK));
        Log(("guest.u64KernelGSBase             %VX64\n",   pVMCB->guest.u64KernelGSBase));
        Log(("guest.u64GPAT                     %VX64\n",   pVMCB->guest.u64GPAT));
        Log(("guest.u64DBGCTL                   %VX64\n",   pVMCB->guest.u64DBGCTL));
        Log(("guest.u64BR_FROM                  %VX64\n",   pVMCB->guest.u64BR_FROM));
        Log(("guest.u64BR_TO                    %VX64\n",   pVMCB->guest.u64BR_TO));
        Log(("guest.u64LASTEXCPFROM             %VX64\n",   pVMCB->guest.u64LASTEXCPFROM));
        Log(("guest.u64LASTEXCPTO               %VX64\n",   pVMCB->guest.u64LASTEXCPTO));

#endif
        rc = VERR_SVM_UNABLE_TO_START_VM;
        goto end;
    }

    /* Let's first sync back eip, esp, and eflags. */
    pCtx->eip        = pVMCB->guest.u64RIP;
    pCtx->esp        = pVMCB->guest.u64RSP;
    pCtx->eflags.u32 = pVMCB->guest.u64RFlags;
    /* eax is saved/restore across the vmrun instruction */
    pCtx->eax        = pVMCB->guest.u64RAX;

    /* Guest CPU context: ES, CS, SS, DS, FS, GS. */
    SVM_READ_SELREG(SS, ss);
    SVM_READ_SELREG(CS, cs);
    SVM_READ_SELREG(DS, ds);
    SVM_READ_SELREG(ES, es);
    SVM_READ_SELREG(FS, fs);
    SVM_READ_SELREG(GS, gs);

    /* Note: no reason to sync back the CRx and DRx registers. They can't be changed by the guest. */
    /* Note: only in the nested paging case can CR3 & CR4 be changed by the guest. */ 
    if (pVM->hwaccm.s.svm.fNestedPaging)
    {
        CPUMSetGuestCR3(pVM, pVMCB->guest.u64CR3);
        CPUMSetGuestCR4(pVM, pVMCB->guest.u64CR4);
    }

    /** @note NOW IT'S SAFE FOR LOGGING! */

    /* Take care of instruction fusing (sti, mov ss) */
    if (pVMCB->ctrl.u64IntShadow & SVM_INTERRUPT_SHADOW_ACTIVE)
    {
        Log(("uInterruptState %x eip=%VGv\n", pVMCB->ctrl.u64IntShadow, pCtx->eip));
        EMSetInhibitInterruptsPC(pVM, pCtx->eip);
    }
    else
        VM_FF_CLEAR(pVM, VM_FF_INHIBIT_INTERRUPTS);

    Log2(("exitCode = %x\n", exitCode));

    /* Check if an injected event was interrupted prematurely. */
    pVM->hwaccm.s.Event.intInfo = pVMCB->ctrl.ExitIntInfo.au64[0];
    if (    pVMCB->ctrl.ExitIntInfo.n.u1Valid
        &&  pVMCB->ctrl.ExitIntInfo.n.u3Type != SVM_EVENT_SOFTWARE_INT /* we don't care about 'int xx' as the instruction will be restarted. */)
    {
        Log(("Pending inject %VX64 at %08x exit=%08x\n", pVM->hwaccm.s.Event.intInfo, pCtx->eip, exitCode));
        pVM->hwaccm.s.Event.fPending = true;
        /* Error code present? (redundant) */
        if (pVMCB->ctrl.ExitIntInfo.n.u1ErrorCodeValid)
        {
            pVM->hwaccm.s.Event.errCode  = pVMCB->ctrl.ExitIntInfo.n.u32ErrorCode;
        }
        else
            pVM->hwaccm.s.Event.errCode  = 0;
    }
    STAM_COUNTER_INC(&pVM->hwaccm.s.pStatExitReasonR0[exitCode & MASK_EXITREASON_STAT]);

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
#ifdef DEBUG
        case X86_XCPT_DB:
            rc = DBGFR0Trap01Handler(pVM, CPUMCTX2CORE(pCtx), pVMCB->guest.u64DR6);
            Assert(rc != VINF_EM_RAW_GUEST_TRAP);
            break;
#endif

        case X86_XCPT_NM:
        {
            uint32_t oldCR0;

            Log(("#NM fault at %VGv\n", pCtx->eip));

            /** @todo don't intercept #NM exceptions anymore when we've activated the guest FPU state. */
            oldCR0 = ASMGetCR0();
            /* If we sync the FPU/XMM state on-demand, then we can continue execution as if nothing has happened. */
            rc = CPUMHandleLazyFPU(pVM);
            if (rc == VINF_SUCCESS)
            {
                Assert(CPUMIsGuestFPUStateActive(pVM));

                /* CPUMHandleLazyFPU could have changed CR0; restore it. */
                ASMSetCR0(oldCR0);

                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitShadowNM);

                /* Continue execution. */
                STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
                pVM->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0;

                goto ResumeExecution;
            }

            Log(("Forward #NM fault to the guest\n"));
            STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitGuestNM);

            Event.au64[0]    = 0;
            Event.n.u3Type   = SVM_EVENT_EXCEPTION;
            Event.n.u1Valid  = 1;
            Event.n.u8Vector = X86_XCPT_NM;

            SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);
            STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
            goto ResumeExecution;
        }

        case X86_XCPT_PF: /* Page fault */
        {
            uint32_t    errCode        = pVMCB->ctrl.u64ExitInfo1;     /* EXITINFO1 = error code */
            RTGCUINTPTR uFaultAddress  = pVMCB->ctrl.u64ExitInfo2;     /* EXITINFO2 = fault address */

            Assert(!pVM->hwaccm.s.svm.fNestedPaging);

            Log2(("Page fault at %VGv cr2=%VGv error code %x\n", pCtx->eip, uFaultAddress, errCode));
            /* Exit qualification contains the linear address of the page fault. */
            TRPMAssertTrap(pVM, X86_XCPT_PF, TRPM_TRAP);
            TRPMSetErrorCode(pVM, errCode);
            TRPMSetFaultAddress(pVM, uFaultAddress);

            /* Forward it to our trap handler first, in case our shadow pages are out of sync. */
            rc = PGMTrap0eHandler(pVM, errCode, CPUMCTX2CORE(pCtx), (RTGCPTR)uFaultAddress);
            Log2(("PGMTrap0eHandler %VGv returned %Vrc\n", pCtx->eip, rc));
            if (rc == VINF_SUCCESS)
            {   /* We've successfully synced our shadow pages, so let's just continue execution. */
                Log2(("Shadow page fault at %VGv cr2=%VGv error code %x\n", pCtx->eip, uFaultAddress, errCode));
                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitShadowPF);

                TRPMResetTrap(pVM);

                STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
                goto ResumeExecution;
            }
            else
            if (rc == VINF_EM_RAW_GUEST_TRAP)
            {   /* A genuine pagefault.
                 * Forward the trap to the guest by injecting the exception and resuming execution.
                 */
                Log2(("Forward page fault to the guest\n"));
                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitGuestPF);
                /* The error code might have been changed. */
                errCode = TRPMGetErrorCode(pVM);

                TRPMResetTrap(pVM);

                /* Now we must update CR2. */
                pCtx->cr2 = uFaultAddress;

                Event.au64[0]               = 0;
                Event.n.u3Type              = SVM_EVENT_EXCEPTION;
                Event.n.u1Valid             = 1;
                Event.n.u8Vector            = X86_XCPT_PF;
                Event.n.u1ErrorCodeValid    = 1;
                Event.n.u32ErrorCode        = errCode;

                SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

                STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
                goto ResumeExecution;
            }
#ifdef VBOX_STRICT
            if (rc != VINF_EM_RAW_EMULATE_INSTR)
                LogFlow(("PGMTrap0eHandler failed with %d\n", rc));
#endif
            /* Need to go back to the recompiler to emulate the instruction. */
            TRPMResetTrap(pVM);
            break;
        }

        case X86_XCPT_MF: /* Floating point exception. */
        {
            STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitGuestMF);
            if (!(pCtx->cr0 & X86_CR0_NE))
            {
                /* old style FPU error reporting needs some extra work. */
                /** @todo don't fall back to the recompiler, but do it manually. */
                rc = VINF_EM_RAW_EMULATE_INSTR;
                break;
            }
            Log(("Trap %x at %VGv\n", vector, pCtx->eip));

            Event.au64[0]    = 0;
            Event.n.u3Type   = SVM_EVENT_EXCEPTION;
            Event.n.u1Valid  = 1;
            Event.n.u8Vector = X86_XCPT_MF;

            SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

            STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
            goto ResumeExecution;
        }

#ifdef VBOX_STRICT
        case X86_XCPT_GP:   /* General protection failure exception.*/
        case X86_XCPT_UD:   /* Unknown opcode exception. */
        case X86_XCPT_DE:   /* Debug exception. */
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
                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitGuestGP);
                Event.n.u1ErrorCodeValid    = 1;
                Event.n.u32ErrorCode        = pVMCB->ctrl.u64ExitInfo1; /* EXITINFO1 = error code */
                break;
            case X86_XCPT_DE:
                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitGuestDE);
                break;
            case X86_XCPT_UD:
                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitGuestUD);
                break;
            case X86_XCPT_SS:
                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitGuestSS);
                Event.n.u1ErrorCodeValid    = 1;
                Event.n.u32ErrorCode        = pVMCB->ctrl.u64ExitInfo1; /* EXITINFO1 = error code */
                break;
            case X86_XCPT_NP:
                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitGuestNP);
                Event.n.u1ErrorCodeValid    = 1;
                Event.n.u32ErrorCode        = pVMCB->ctrl.u64ExitInfo1; /* EXITINFO1 = error code */
                break;
            }
            Log(("Trap %x at %VGv esi=%x\n", vector, pCtx->eip, pCtx->esi));
            SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

            STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
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

        Assert(pVM->hwaccm.s.svm.fNestedPaging);

        Log2(("Page fault at %VGp cr2=%VGv error code %x\n", pCtx->eip, uFaultAddress, errCode));
        /* Exit qualification contains the linear address of the page fault. */
        TRPMAssertTrap(pVM, X86_XCPT_PF, TRPM_TRAP);
        TRPMSetErrorCode(pVM, errCode);
        TRPMSetFaultAddress(pVM, uFaultAddress);

        /* Handle the pagefault trap for the nested shadow table. */
        rc = PGMR0Trap0eHandlerNestedPaging(pVM, PGMGetShadowMode(pVM), errCode, CPUMCTX2CORE(pCtx), uFaultAddress);
        Log2(("PGMR0Trap0eHandlerNestedPaging %VGv returned %Vrc\n", pCtx->eip, rc));
        if (rc == VINF_SUCCESS)
        {   /* We've successfully synced our shadow pages, so let's just continue execution. */
            Log2(("Shadow page fault at %VGv cr2=%VGp error code %x\n", pCtx->eip, uFaultAddress, errCode));
            STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitShadowPF);

            TRPMResetTrap(pVM);

            STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
            goto ResumeExecution;
        }

#ifdef VBOX_STRICT
        if (rc != VINF_EM_RAW_EMULATE_INSTR)
            LogFlow(("PGMTrap0eHandlerNestedPaging failed with %d\n", rc));
#endif
        /* Need to go back to the recompiler to emulate the instruction. */
        TRPMResetTrap(pVM);
        break;
    }

    case SVM_EXIT_VINTR:
        /* A virtual interrupt is about to be delivered, which means IF=1. */
        Log(("SVM_EXIT_VINTR IF=%d\n", pCtx->eflags.Bits.u1IF));
        pVMCB->ctrl.IntCtrl.n.u1VIrqValid    = 0;
        pVMCB->ctrl.IntCtrl.n.u1IgnoreTPR    = 0;
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
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitInvd);
        /* Skip instruction and continue directly. */
        pCtx->eip += 2;     /** @note hardcoded opcode size! */
        /* Continue execution.*/
        STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
        goto ResumeExecution;

    case SVM_EXIT_CPUID:                /* Guest software attempted to execute CPUID. */
    {
        Log2(("SVM: Cpuid %x\n", pCtx->eax));
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitCpuid);
        rc = EMInterpretCpuId(pVM, CPUMCTX2CORE(pCtx));
        if (rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            pCtx->eip += 2;             /** @note hardcoded opcode size! */
            STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
            goto ResumeExecution;
        }
        AssertMsgFailed(("EMU: cpuid failed with %Vrc\n", rc));
        rc = VINF_EM_RAW_EMULATE_INSTR;
        break;
    }

    case SVM_EXIT_RDTSC:                /* Guest software attempted to execute RDTSC. */
    {
        Log2(("SVM: Rdtsc\n"));
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitRdtsc);
        rc = EMInterpretRdtsc(pVM, CPUMCTX2CORE(pCtx));
        if (rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            pCtx->eip += 2;             /** @note hardcoded opcode size! */
            STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
            goto ResumeExecution;
        }
        AssertMsgFailed(("EMU: rdtsc failed with %Vrc\n", rc));
        rc = VINF_EM_RAW_EMULATE_INSTR;
        break;
    }

    case SVM_EXIT_INVLPG:               /* Guest software attempted to execute INVPG. */
    {
        Log2(("SVM: invlpg\n"));
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitInvpg);

        Assert(!pVM->hwaccm.s.svm.fNestedPaging);

        /* Truly a pita. Why can't SVM give the same information as VMX? */
        rc = SVMR0InterpretInvpg(pVM, CPUMCTX2CORE(pCtx), pVMCB->ctrl.TLBCtrl.n.u32ASID);
        if (rc == VINF_SUCCESS)
        {
            STAM_COUNTER_INC(&pVM->hwaccm.s.StatFlushPageInvlpg);
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

        Log2(("SVM: %VGv mov cr%d, \n", pCtx->eip, exitCode - SVM_EXIT_WRITE_CR0));
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitCRxWrite);
        rc = EMInterpretInstruction(pVM, CPUMCTX2CORE(pCtx), 0, &cbSize);

        switch (exitCode - SVM_EXIT_WRITE_CR0)
        {
        case 0:
            pVM->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0;
            break;
        case 2:
            break;
        case 3:
            Assert(!pVM->hwaccm.s.svm.fNestedPaging);
            pVM->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR3;
            break;
        case 4:
            Assert(!pVM->hwaccm.s.svm.fNestedPaging);
            pVM->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR4;
            break;
        default:
            AssertFailed();
        }
        /* Check if a sync operation is pending. */
        if (    rc == VINF_SUCCESS /* don't bother if we are going to ring 3 anyway */
            &&  VM_FF_ISPENDING(pVM, VM_FF_PGM_SYNC_CR3 | VM_FF_PGM_SYNC_CR3_NON_GLOBAL))
        {
            rc = PGMSyncCR3(pVM, CPUMGetGuestCR0(pVM), CPUMGetGuestCR3(pVM), CPUMGetGuestCR4(pVM), VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3));
            AssertRC(rc);

            STAM_COUNTER_INC(&pVM->hwaccm.s.StatFlushTLBCRxChange);
            
            /** @note Force a TLB flush. SVM requires us to do it manually. */
            pVM->hwaccm.s.svm.fForceTLBFlush = true;
        }
        if (rc == VINF_SUCCESS)
        {
            /* EIP has been updated already. */

            /* Only resume if successful. */
            STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
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

        Log2(("SVM: %VGv mov x, cr%d\n", pCtx->eip, exitCode - SVM_EXIT_READ_CR0));
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitCRxRead);
        rc = EMInterpretInstruction(pVM, CPUMCTX2CORE(pCtx), 0, &cbSize);
        if (rc == VINF_SUCCESS)
        {
            /* EIP has been updated already. */

            /* Only resume if successful. */
            STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
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

        Log2(("SVM: %VGv mov dr%d, x\n", pCtx->eip, exitCode - SVM_EXIT_WRITE_DR0));
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitDRxRead);
        rc = EMInterpretInstruction(pVM, CPUMCTX2CORE(pCtx), 0, &cbSize);
        if (rc == VINF_SUCCESS)
        {
            /* EIP has been updated already. */

            /* Only resume if successful. */
            STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
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

        Log2(("SVM: %VGv mov dr%d, x\n", pCtx->eip, exitCode - SVM_EXIT_READ_DR0));
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitDRxRead);
        rc = EMInterpretInstruction(pVM, CPUMCTX2CORE(pCtx), 0, &cbSize);
        if (rc == VINF_SUCCESS)
        {
            /* EIP has been updated already. */

            /* Only resume if successful. */
            STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
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
            uint32_t prefix = 0;
            if (IoExitInfo.n.u1REP)
                prefix |= PREFIX_REP;

            if (IoExitInfo.n.u1Type == 0)
            {
                Log2(("IOMInterpretOUTSEx %VGv %x size=%d\n", pCtx->eip, IoExitInfo.n.u16Port, uIOSize));
                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitIOStringWrite);
                rc = IOMInterpretOUTSEx(pVM, CPUMCTX2CORE(pCtx), IoExitInfo.n.u16Port, prefix, uIOSize);
            }
            else
            {
                Log2(("IOMInterpretINSEx  %VGv %x size=%d\n", pCtx->eip, IoExitInfo.n.u16Port, uIOSize));
                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitIOStringRead);
                rc = IOMInterpretINSEx(pVM, CPUMCTX2CORE(pCtx), IoExitInfo.n.u16Port, prefix, uIOSize);
            }
        }
        else
        {
            /* normal in/out */
            Assert(!IoExitInfo.n.u1REP);

            if (IoExitInfo.n.u1Type == 0)
            {
                Log2(("IOMIOPortWrite %VGv %x %x size=%d\n", pCtx->eip, IoExitInfo.n.u16Port, pCtx->eax & uAndVal, uIOSize));
                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitIOWrite);
                rc = IOMIOPortWrite(pVM, IoExitInfo.n.u16Port, pCtx->eax & uAndVal, uIOSize);
            }
            else
            {
                uint32_t u32Val = 0;

                STAM_COUNTER_INC(&pVM->hwaccm.s.StatExitIORead);
                rc = IOMIOPortRead(pVM, IoExitInfo.n.u16Port, &u32Val, uIOSize);
                if (IOM_SUCCESS(rc))
                {
                    /* Write back to the EAX register. */
                    pCtx->eax = (pCtx->eax & ~uAndVal) | (u32Val & uAndVal);
                    Log2(("IOMIOPortRead %VGv %x %x size=%d\n", pCtx->eip, IoExitInfo.n.u16Port, u32Val & uAndVal, uIOSize));
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
            pCtx->eip = pVMCB->ctrl.u64ExitInfo2;      /* RIP/EIP of the next instruction is saved in EXITINFO2. */
            if (RT_LIKELY(rc == VINF_SUCCESS))
            {
                STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
                goto ResumeExecution;
            }
            Log2(("EM status from IO at %VGv %x size %d: %Vrc\n", pCtx->eip, IoExitInfo.n.u16Port, uIOSize, rc));
            break;
        }

#ifdef VBOX_STRICT
        if (rc == VINF_IOM_HC_IOPORT_READ)
            Assert(IoExitInfo.n.u1Type != 0);
        else if (rc == VINF_IOM_HC_IOPORT_WRITE)
            Assert(IoExitInfo.n.u1Type == 0);
        else
            AssertMsg(VBOX_FAILURE(rc) || rc == VINF_EM_RAW_EMULATE_INSTR || rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_TRPM_XCPT_DISPATCHED, ("%Vrc\n", rc));
#endif
        Log2(("Failed IO at %VGv %x size %d\n", pCtx->eip, IoExitInfo.n.u16Port, uIOSize));
        break;
    }

    case SVM_EXIT_HLT:
        /** Check if external interrupts are pending; if so, don't switch back. */
        if (VM_FF_ISPENDING(pVM, (VM_FF_INTERRUPT_APIC|VM_FF_INTERRUPT_PIC)))
        {
            pCtx->eip++;    /* skip hlt */
            goto ResumeExecution;
        }

        rc = VINF_EM_RAW_EMULATE_INSTR_HLT;
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
    case SVM_EXIT_RDTSCP:
    {
        /* Unsupported instructions. */
        SVM_EVENT Event;

        Event.au64[0]    = 0;
        Event.n.u3Type   = SVM_EVENT_EXCEPTION;
        Event.n.u1Valid  = 1;
        Event.n.u8Vector = X86_XCPT_UD;

        Log(("Forced #UD trap at %VGv\n", pCtx->eip));
        SVMR0InjectEvent(pVM, pVMCB, pCtx, &Event);

        STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
        goto ResumeExecution;
    }

    /* Emulate in ring 3. */
    case SVM_EXIT_MONITOR:
    case SVM_EXIT_RDPMC:
    case SVM_EXIT_PAUSE:
    case SVM_EXIT_MWAIT_UNCOND:
    case SVM_EXIT_MWAIT_ARMED:
    case SVM_EXIT_MSR:
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
    if (fGuestStateSynced)
    {
        /* Remaining guest CPU context: TR, IDTR, GDTR, LDTR. */
        SVM_READ_SELREG(LDTR, ldtr);
        SVM_READ_SELREG(TR, tr);

        pCtx->gdtr.cbGdt        = pVMCB->guest.GDTR.u32Limit;
        pCtx->gdtr.pGdt         = pVMCB->guest.GDTR.u64Base;

        pCtx->idtr.cbIdt        = pVMCB->guest.IDTR.u32Limit;
        pCtx->idtr.pIdt         = pVMCB->guest.IDTR.u64Base;

        /*
         * System MSRs
         */
        pCtx->SysEnter.cs       = pVMCB->guest.u64SysEnterCS;
        pCtx->SysEnter.eip      = pVMCB->guest.u64SysEnterEIP;
        pCtx->SysEnter.esp      = pVMCB->guest.u64SysEnterESP;
    }

    /* Signal changes for the recompiler. */
    CPUMSetChangedFlags(pVM, CPUM_CHANGED_SYSENTER_MSR | CPUM_CHANGED_LDTR | CPUM_CHANGED_GDTR | CPUM_CHANGED_IDTR | CPUM_CHANGED_TR | CPUM_CHANGED_HIDDEN_SEL_REGS);

    /* If we executed vmrun and an external irq was pending, then we don't have to do a full sync the next time. */
    if (exitCode == SVM_EXIT_INTR)
    {
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatPendingHostIrq);
        /* On the next entry we'll only sync the host context. */
        pVM->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_HOST_CONTEXT;
    }
    else
    {
        /* On the next entry we'll sync everything. */
        /** @todo we can do better than this */
        pVM->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_ALL;
    }

    /* translate into a less severe return code */
    if (rc == VERR_EM_INTERPRETER)
        rc = VINF_EM_RAW_EMULATE_INSTR;

    STAM_PROFILE_ADV_STOP(&pVM->hwaccm.s.StatExit, x);
    return rc;
}

/**
 * Enters the AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCpu        CPU info struct
 */
HWACCMR0DECL(int) SVMR0Enter(PVM pVM, PHWACCM_CPUINFO pCpu)
{
    Assert(pVM->hwaccm.s.svm.fSupported);

    LogFlow(("SVMR0Enter cpu%d last=%d asid=%d\n", pCpu->idCpu, pVM->hwaccm.s.svm.idLastCpu, pCpu->uCurrentASID));
    pVM->hwaccm.s.svm.fResumeVM = false;

    /* Force to reload LDTR, so we'll execute VMLoad to load additional guest state. */
    pVM->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_LDTR;

    return VINF_SUCCESS;
}


/**
 * Leaves the AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0Leave(PVM pVM)
{
    Assert(pVM->hwaccm.s.svm.fSupported);
    return VINF_SUCCESS;
}


static int svmInterpretInvlPg(PVM pVM, PDISCPUSTATE pCpu, PCPUMCTXCORE pRegFrame, uint32_t uASID)
{
    OP_PARAMVAL param1;
    RTGCPTR     addr;

    int rc = DISQueryParamVal(pRegFrame, pCpu, &pCpu->param1, &param1, PARAM_SOURCE);
    if(VBOX_FAILURE(rc))
        return VERR_EM_INTERPRETER;

    switch(param1.type)
    {
    case PARMTYPE_IMMEDIATE:
    case PARMTYPE_ADDRESS:
        if(!(param1.flags & PARAM_VAL32))
            return VERR_EM_INTERPRETER;
        addr = (RTGCPTR)param1.val.val32;
        break;

    default:
        return VERR_EM_INTERPRETER;
    }

    /** @todo is addr always a flat linear address or ds based
     * (in absence of segment override prefixes)????
     */
    rc = PGMInvalidatePage(pVM, addr);
    if (VBOX_SUCCESS(rc))
    {
        /* Manually invalidate the page for the VM's TLB. */
        Log(("SVMInvlpgA %VGv ASID=%d\n", addr, uASID));
        SVMInvlpgA(addr, uASID);
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
static int SVMR0InterpretInvpg(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t uASID)
{
    /*
     * Only allow 32-bit code.
     */
    if (SELMIsSelector32Bit(pVM, pRegFrame->eflags, pRegFrame->cs, &pRegFrame->csHid))
    {
        RTGCPTR pbCode;
        int rc = SELMValidateAndConvertCSAddr(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs, &pRegFrame->csHid, (RTGCPTR)pRegFrame->eip, &pbCode);
        if (VBOX_SUCCESS(rc))
        {
            uint32_t    cbOp;
            DISCPUSTATE Cpu;

            Cpu.mode = CPUMODE_32BIT;
            rc = EMInterpretDisasOneEx(pVM, pbCode, pRegFrame, &Cpu, &cbOp);
            Assert(VBOX_FAILURE(rc) || Cpu.pCurInstr->opcode == OP_INVLPG);
            if (VBOX_SUCCESS(rc) && Cpu.pCurInstr->opcode == OP_INVLPG)
            {
                Assert(cbOp == Cpu.opsize);
                rc = svmInterpretInvlPg(pVM, &Cpu, pRegFrame, uASID);
                if (VBOX_SUCCESS(rc))
                {
                    pRegFrame->eip += cbOp; /* Move on to the next instruction. */
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
 * @param   GCVirt      Page to invalidate
 */
HWACCMR0DECL(int) SVMR0InvalidatePage(PVM pVM, RTGCPTR GCVirt)
{
    bool fFlushPending = pVM->hwaccm.s.svm.fAlwaysFlushTLB | pVM->hwaccm.s.svm.fForceTLBFlush;

    /* Skip it if a TLB flush is already pending. */
    if (!fFlushPending)
    {
        SVM_VMCB   *pVMCB;

        Log2(("SVMR0InvalidatePage %VGv\n", GCVirt));
        AssertReturn(pVM, VERR_INVALID_PARAMETER);
        Assert(pVM->hwaccm.s.svm.fSupported);

        pVMCB = (SVM_VMCB *)pVM->hwaccm.s.svm.pVMCB;
        AssertMsgReturn(pVMCB, ("Invalid pVMCB\n"), VERR_EM_INTERNAL_ERROR);

        STAM_COUNTER_INC(&pVM->hwaccm.s.StatFlushPageManual);
        SVMInvlpgA(GCVirt, pVMCB->ctrl.TLBCtrl.n.u32ASID);
    }
    return VINF_SUCCESS;
}

