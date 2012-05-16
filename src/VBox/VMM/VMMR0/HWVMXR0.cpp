/* $Id$ */
/** @file
 * HM VMX (VT-x) - Host Context Ring-0.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_HWACCM
#include <iprt/asm-amd64-x86.h>
#include <VBox/vmm/hwaccm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/dbgftrace.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/iom.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include <VBox/vmm/tm.h>
#include "HWACCMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/time.h>
#ifdef VBOX_WITH_VMMR0_DISABLE_PREEMPTION
# include <iprt/thread.h>
#endif
#include <iprt/x86.h>
#include "HWVMXR0.h"

#include "dtrace/VBoxVMM.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#if defined(RT_ARCH_AMD64)
# define VMX_IS_64BIT_HOST_MODE()   (true)
#elif defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
# define VMX_IS_64BIT_HOST_MODE()   (g_fVMXIs64bitHost != 0)
#else
# define VMX_IS_64BIT_HOST_MODE()   (false)
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/* IO operation lookup arrays. */
static uint32_t const g_aIOSize[4]  = {1, 2, 0, 4};
static uint32_t const g_aIOOpAnd[4] = {0xff, 0xffff, 0, 0xffffffff};

#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
/** See HWACCMR0A.asm. */
extern "C" uint32_t g_fVMXIs64bitHost;
#endif


/*******************************************************************************
*   Local Functions                                                            *
*******************************************************************************/
static DECLCALLBACK(void) hmR0VmxSetupTLBEPT(PVM pVM, PVMCPU pVCpu);
static DECLCALLBACK(void) hmR0VmxSetupTLBVPID(PVM pVM, PVMCPU pVCpu);
static DECLCALLBACK(void) hmR0VmxSetupTLBBoth(PVM pVM, PVMCPU pVCpu);
static DECLCALLBACK(void) hmR0VmxSetupTLBDummy(PVM pVM, PVMCPU pVCpu);
static void hmR0VmxFlushEPT(PVM pVM, PVMCPU pVCpu, VMX_FLUSH_EPT enmFlush);
static void hmR0VmxFlushVPID(PVM pVM, PVMCPU pVCpu, VMX_FLUSH_VPID enmFlush, RTGCPTR GCPtr);
static void hmR0VmxUpdateExceptionBitmap(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
static void hmR0VmxSetMSRPermission(PVMCPU pVCpu, unsigned ulMSR, bool fRead, bool fWrite);
static void hmR0VmxReportWorldSwitchError(PVM pVM, PVMCPU pVCpu, VBOXSTRICTRC rc, PCPUMCTX pCtx);


/**
 * Updates error from VMCS to HWACCMCPU's lasterror record.
 *
 * @param    pVM            Pointer to the VM.
 * @param    pVCpu          Pointer to the VMCPU.
 * @param    rc             The error code.
 */
static void hmR0VmxCheckError(PVM pVM, PVMCPU pVCpu, int rc)
{
    if (rc == VERR_VMX_GENERIC)
    {
        RTCCUINTREG instrError;

        VMXReadVMCS(VMX_VMCS32_RO_VM_INSTR_ERROR, &instrError);
        pVCpu->hwaccm.s.vmx.lasterror.ulInstrError = instrError;
    }
    pVM->hwaccm.s.lLastError = rc;
}


/**
 * Sets up and activates VT-x on the current CPU.
 *
 * @returns VBox status code.
 * @param   pCpu            Pointer to the CPU info struct.
 * @param   pVM             Pointer to the VM. (can be NULL after a resume!!)
 * @param   pvCpuPage       Pointer to the global CPU page.
 * @param   HCPhysCpuPage   Physical address of the global CPU page.
 */
VMMR0DECL(int) VMXR0EnableCpu(PHMGLOBLCPUINFO pCpu, PVM pVM, void *pvCpuPage, RTHCPHYS HCPhysCpuPage)
{
    AssertReturn(HCPhysCpuPage != 0 && HCPhysCpuPage != NIL_RTHCPHYS, VERR_INVALID_PARAMETER);
    AssertReturn(pvCpuPage, VERR_INVALID_PARAMETER);

    if (pVM)
    {
        /* Set revision dword at the beginning of the VMXON structure. */
        *(uint32_t *)pvCpuPage = MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hwaccm.s.vmx.msr.vmx_basic_info);
    }

    /** @todo we should unmap the two pages from the virtual address space in order to prevent accidental corruption.
     * (which can have very bad consequences!!!)
     */

    if (ASMGetCR4() & X86_CR4_VMXE)
        return VERR_VMX_IN_VMX_ROOT_MODE;

    ASMSetCR4(ASMGetCR4() | X86_CR4_VMXE);    /* Make sure the VMX instructions don't cause #UD faults. */

    /*
     * Enter VM root mode.
     */
    int rc = VMXEnable(HCPhysCpuPage);
    if (RT_FAILURE(rc))
    {
        ASMSetCR4(ASMGetCR4() & ~X86_CR4_VMXE);
        return VERR_VMX_VMXON_FAILED;
    }

    /*
     * Flush all VPIDs (in case we or any other hypervisor have been using VPIDs) so that
     * we can avoid an explicit flush while using new VPIDs. We would still need to flush
     * each time while reusing a VPID after hitting the MaxASID limit once.
     */
    if (   pVM
        && pVM->hwaccm.s.vmx.fVPID
        && (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_ALL_CONTEXTS))
    {
        hmR0VmxFlushVPID(pVM, NULL /* pvCpu */, VMX_FLUSH_VPID_ALL_CONTEXTS, 0 /* GCPtr */);
        pCpu->fFlushASIDBeforeUse = false;
    }
    else
        pCpu->fFlushASIDBeforeUse = true;

    return VINF_SUCCESS;
}


/**
 * Deactivates VT-x on the current CPU.
 *
 * @returns VBox status code.
 * @param   pCpu            Pointer to the CPU info struct.
 * @param   pvCpuPage       Pointer to the global CPU page.
 * @param   HCPhysCpuPage   Physical address of the global CPU page.
 */
VMMR0DECL(int) VMXR0DisableCpu(PHMGLOBLCPUINFO pCpu, void *pvCpuPage, RTHCPHYS HCPhysCpuPage)
{
    AssertReturn(HCPhysCpuPage != 0 && HCPhysCpuPage != NIL_RTHCPHYS, VERR_INVALID_PARAMETER);
    AssertReturn(pvCpuPage, VERR_INVALID_PARAMETER);
    NOREF(pCpu);

    /* If we're somehow not in VMX root mode, then we shouldn't dare leaving it. */
    if (!(ASMGetCR4() & X86_CR4_VMXE))
        return VERR_VMX_NOT_IN_VMX_ROOT_MODE;

    /* Leave VMX Root Mode. */
    VMXDisable();

    /* And clear the X86_CR4_VMXE bit. */
    ASMSetCR4(ASMGetCR4() & ~X86_CR4_VMXE);
    return VINF_SUCCESS;
}


/**
 * Does Ring-0 per VM VT-x initialization.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0DECL(int) VMXR0InitVM(PVM pVM)
{
    int rc;

#ifdef LOG_ENABLED
    SUPR0Printf("VMXR0InitVM %x\n", pVM);
#endif

    pVM->hwaccm.s.vmx.pMemObjAPIC = NIL_RTR0MEMOBJ;

    if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW)
    {
        /* Allocate one page for the APIC physical page (serves for filtering accesses). */
        rc = RTR0MemObjAllocCont(&pVM->hwaccm.s.vmx.pMemObjAPIC, PAGE_SIZE, true /* executable R0 mapping */);
        AssertRC(rc);
        if (RT_FAILURE(rc))
            return rc;

        pVM->hwaccm.s.vmx.pAPIC     = (uint8_t *)RTR0MemObjAddress(pVM->hwaccm.s.vmx.pMemObjAPIC);
        pVM->hwaccm.s.vmx.pAPICPhys = RTR0MemObjGetPagePhysAddr(pVM->hwaccm.s.vmx.pMemObjAPIC, 0);
        ASMMemZero32(pVM->hwaccm.s.vmx.pAPIC, PAGE_SIZE);
    }
    else
    {
        pVM->hwaccm.s.vmx.pMemObjAPIC = 0;
        pVM->hwaccm.s.vmx.pAPIC       = 0;
        pVM->hwaccm.s.vmx.pAPICPhys   = 0;
    }

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    {
        rc = RTR0MemObjAllocCont(&pVM->hwaccm.s.vmx.pMemObjScratch, PAGE_SIZE, true /* executable R0 mapping */);
        AssertRC(rc);
        if (RT_FAILURE(rc))
            return rc;

        pVM->hwaccm.s.vmx.pScratch     = (uint8_t *)RTR0MemObjAddress(pVM->hwaccm.s.vmx.pMemObjScratch);
        pVM->hwaccm.s.vmx.pScratchPhys = RTR0MemObjGetPagePhysAddr(pVM->hwaccm.s.vmx.pMemObjScratch, 0);

        ASMMemZero32(pVM->hwaccm.s.vmx.pScratch, PAGE_SIZE);
        strcpy((char *)pVM->hwaccm.s.vmx.pScratch, "SCRATCH Magic");
        *(uint64_t *)(pVM->hwaccm.s.vmx.pScratch + 16) = UINT64_C(0xDEADBEEFDEADBEEF);
    }
#endif

    /* Allocate VMCSs for all guest CPUs. */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        pVCpu->hwaccm.s.vmx.hMemObjVMCS = NIL_RTR0MEMOBJ;

        /* Allocate one page for the VM control structure (VMCS). */
        rc = RTR0MemObjAllocCont(&pVCpu->hwaccm.s.vmx.hMemObjVMCS, PAGE_SIZE, true /* executable R0 mapping */);
        AssertRC(rc);
        if (RT_FAILURE(rc))
            return rc;

        pVCpu->hwaccm.s.vmx.pvVMCS     = RTR0MemObjAddress(pVCpu->hwaccm.s.vmx.hMemObjVMCS);
        pVCpu->hwaccm.s.vmx.HCPhysVMCS = RTR0MemObjGetPagePhysAddr(pVCpu->hwaccm.s.vmx.hMemObjVMCS, 0);
        ASMMemZeroPage(pVCpu->hwaccm.s.vmx.pvVMCS);

        pVCpu->hwaccm.s.vmx.cr0_mask = 0;
        pVCpu->hwaccm.s.vmx.cr4_mask = 0;

        /* Allocate one page for the virtual APIC page for TPR caching. */
        rc = RTR0MemObjAllocCont(&pVCpu->hwaccm.s.vmx.hMemObjVAPIC, PAGE_SIZE, true /* executable R0 mapping */);
        AssertRC(rc);
        if (RT_FAILURE(rc))
            return rc;

        pVCpu->hwaccm.s.vmx.pbVAPIC     = (uint8_t *)RTR0MemObjAddress(pVCpu->hwaccm.s.vmx.hMemObjVAPIC);
        pVCpu->hwaccm.s.vmx.HCPhysVAPIC = RTR0MemObjGetPagePhysAddr(pVCpu->hwaccm.s.vmx.hMemObjVAPIC, 0);
        ASMMemZeroPage(pVCpu->hwaccm.s.vmx.pbVAPIC);

        /* Allocate the MSR bitmap if this feature is supported. */
        if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS)
        {
            rc = RTR0MemObjAllocCont(&pVCpu->hwaccm.s.vmx.pMemObjMSRBitmap, PAGE_SIZE, true /* executable R0 mapping */);
            AssertRC(rc);
            if (RT_FAILURE(rc))
                return rc;

            pVCpu->hwaccm.s.vmx.pMSRBitmap     = (uint8_t *)RTR0MemObjAddress(pVCpu->hwaccm.s.vmx.pMemObjMSRBitmap);
            pVCpu->hwaccm.s.vmx.pMSRBitmapPhys = RTR0MemObjGetPagePhysAddr(pVCpu->hwaccm.s.vmx.pMemObjMSRBitmap, 0);
            memset(pVCpu->hwaccm.s.vmx.pMSRBitmap, 0xff, PAGE_SIZE);
        }

#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
        /* Allocate one page for the guest MSR load area (for preloading guest MSRs during the world switch). */
        rc = RTR0MemObjAllocCont(&pVCpu->hwaccm.s.vmx.pMemObjGuestMSR, PAGE_SIZE, true /* executable R0 mapping */);
        AssertRC(rc);
        if (RT_FAILURE(rc))
            return rc;

        pVCpu->hwaccm.s.vmx.pGuestMSR     = (uint8_t *)RTR0MemObjAddress(pVCpu->hwaccm.s.vmx.pMemObjGuestMSR);
        pVCpu->hwaccm.s.vmx.pGuestMSRPhys = RTR0MemObjGetPagePhysAddr(pVCpu->hwaccm.s.vmx.pMemObjGuestMSR, 0);
        memset(pVCpu->hwaccm.s.vmx.pGuestMSR, 0, PAGE_SIZE);

        /* Allocate one page for the host MSR load area (for restoring host MSRs after the world switch back). */
        rc = RTR0MemObjAllocCont(&pVCpu->hwaccm.s.vmx.pMemObjHostMSR, PAGE_SIZE, true /* executable R0 mapping */);
        AssertRC(rc);
        if (RT_FAILURE(rc))
            return rc;

        pVCpu->hwaccm.s.vmx.pHostMSR     = (uint8_t *)RTR0MemObjAddress(pVCpu->hwaccm.s.vmx.pMemObjHostMSR);
        pVCpu->hwaccm.s.vmx.pHostMSRPhys = RTR0MemObjGetPagePhysAddr(pVCpu->hwaccm.s.vmx.pMemObjHostMSR, 0);
        memset(pVCpu->hwaccm.s.vmx.pHostMSR, 0, PAGE_SIZE);
#endif /* VBOX_WITH_AUTO_MSR_LOAD_RESTORE */

        /* Current guest paging mode. */
        pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode = PGMMODE_REAL;

#ifdef LOG_ENABLED
        SUPR0Printf("VMXR0InitVM %x VMCS=%x (%x)\n", pVM, pVCpu->hwaccm.s.vmx.pvVMCS, (uint32_t)pVCpu->hwaccm.s.vmx.HCPhysVMCS);
#endif
    }

    return VINF_SUCCESS;
}


/**
 * Does Ring-0 per VM VT-x termination.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0DECL(int) VMXR0TermVM(PVM pVM)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        if (pVCpu->hwaccm.s.vmx.hMemObjVMCS != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVCpu->hwaccm.s.vmx.hMemObjVMCS, false);
            pVCpu->hwaccm.s.vmx.hMemObjVMCS = NIL_RTR0MEMOBJ;
            pVCpu->hwaccm.s.vmx.pvVMCS      = 0;
            pVCpu->hwaccm.s.vmx.HCPhysVMCS  = 0;
        }
        if (pVCpu->hwaccm.s.vmx.hMemObjVAPIC != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVCpu->hwaccm.s.vmx.hMemObjVAPIC, false);
            pVCpu->hwaccm.s.vmx.hMemObjVAPIC = NIL_RTR0MEMOBJ;
            pVCpu->hwaccm.s.vmx.pbVAPIC      = 0;
            pVCpu->hwaccm.s.vmx.HCPhysVAPIC  = 0;
        }
        if (pVCpu->hwaccm.s.vmx.pMemObjMSRBitmap != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVCpu->hwaccm.s.vmx.pMemObjMSRBitmap, false);
            pVCpu->hwaccm.s.vmx.pMemObjMSRBitmap = NIL_RTR0MEMOBJ;
            pVCpu->hwaccm.s.vmx.pMSRBitmap       = 0;
            pVCpu->hwaccm.s.vmx.pMSRBitmapPhys   = 0;
        }
#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
        if (pVCpu->hwaccm.s.vmx.pMemObjHostMSR != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVCpu->hwaccm.s.vmx.pMemObjHostMSR, false);
            pVCpu->hwaccm.s.vmx.pMemObjHostMSR = NIL_RTR0MEMOBJ;
            pVCpu->hwaccm.s.vmx.pHostMSR       = 0;
            pVCpu->hwaccm.s.vmx.pHostMSRPhys   = 0;
        }
        if (pVCpu->hwaccm.s.vmx.pMemObjGuestMSR != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVCpu->hwaccm.s.vmx.pMemObjGuestMSR, false);
            pVCpu->hwaccm.s.vmx.pMemObjGuestMSR = NIL_RTR0MEMOBJ;
            pVCpu->hwaccm.s.vmx.pGuestMSR       = 0;
            pVCpu->hwaccm.s.vmx.pGuestMSRPhys   = 0;
        }
#endif /* VBOX_WITH_AUTO_MSR_LOAD_RESTORE */
    }
    if (pVM->hwaccm.s.vmx.pMemObjAPIC != NIL_RTR0MEMOBJ)
    {
        RTR0MemObjFree(pVM->hwaccm.s.vmx.pMemObjAPIC, false);
        pVM->hwaccm.s.vmx.pMemObjAPIC = NIL_RTR0MEMOBJ;
        pVM->hwaccm.s.vmx.pAPIC       = 0;
        pVM->hwaccm.s.vmx.pAPICPhys   = 0;
    }
#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    if (pVM->hwaccm.s.vmx.pMemObjScratch != NIL_RTR0MEMOBJ)
    {
        ASMMemZero32(pVM->hwaccm.s.vmx.pScratch, PAGE_SIZE);
        RTR0MemObjFree(pVM->hwaccm.s.vmx.pMemObjScratch, false);
        pVM->hwaccm.s.vmx.pMemObjScratch = NIL_RTR0MEMOBJ;
        pVM->hwaccm.s.vmx.pScratch       = 0;
        pVM->hwaccm.s.vmx.pScratchPhys   = 0;
    }
#endif
    return VINF_SUCCESS;
}


/**
 * Sets up VT-x for the specified VM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0DECL(int) VMXR0SetupVM(PVM pVM)
{
    int rc = VINF_SUCCESS;
    uint32_t val;

    AssertReturn(pVM, VERR_INVALID_PARAMETER);

    /* Initialize these always, see hwaccmR3InitFinalizeR0().*/
    pVM->hwaccm.s.vmx.enmFlushEPT  = VMX_FLUSH_EPT_NONE;
    pVM->hwaccm.s.vmx.enmFlushVPID = VMX_FLUSH_VPID_NONE;

    /* Determine optimal flush type for EPT. */
    if (pVM->hwaccm.s.fNestedPaging)
    {
        if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_SINGLE_CONTEXT)
            pVM->hwaccm.s.vmx.enmFlushEPT = VMX_FLUSH_EPT_SINGLE_CONTEXT;
        else if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_ALL_CONTEXTS)
            pVM->hwaccm.s.vmx.enmFlushEPT = VMX_FLUSH_EPT_ALL_CONTEXTS;
        else
        {
            /*
             * Should never really happen. EPT is supported but no suitable flush types supported.
             * We cannot ignore EPT at this point as we've already setup Unrestricted Guest execution.
             */
            pVM->hwaccm.s.vmx.enmFlushEPT = VMX_FLUSH_EPT_NOT_SUPPORTED;
            return VERR_VMX_GENERIC;
        }
    }

    /* Determine optimal flush type for VPID. */
    if (pVM->hwaccm.s.vmx.fVPID)
    {
        if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_SINGLE_CONTEXT)
            pVM->hwaccm.s.vmx.enmFlushVPID = VMX_FLUSH_VPID_SINGLE_CONTEXT;
        else if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_ALL_CONTEXTS)
            pVM->hwaccm.s.vmx.enmFlushVPID = VMX_FLUSH_VPID_ALL_CONTEXTS;
        else
        {
            /*
             * Neither SINGLE nor ALL context flush types for VPID supported by the CPU.
             * We do not handle other flush type combinations, ignore VPID capabilities.
             */
            if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_INDIV_ADDR)
                Log(("VMXR0SetupVM: Only VMX_FLUSH_VPID_INDIV_ADDR supported. Ignoring VPID.\n"));
            if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_SINGLE_CONTEXT_RETAIN_GLOBALS)
                Log(("VMXR0SetupVM: Only VMX_FLUSH_VPID_SINGLE_CONTEXT_RETAIN_GLOBALS supported. Ignoring VPID.\n"));
            pVM->hwaccm.s.vmx.enmFlushVPID = VMX_FLUSH_VPID_NOT_SUPPORTED;
            pVM->hwaccm.s.vmx.fVPID = false;
        }
    }

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        AssertPtr(pVCpu->hwaccm.s.vmx.pvVMCS);

        /* Set revision dword at the beginning of the VMCS structure. */
        *(uint32_t *)pVCpu->hwaccm.s.vmx.pvVMCS = MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hwaccm.s.vmx.msr.vmx_basic_info);

        /*
         * Clear and activate the VMCS.
         */
        Log(("HCPhysVMCS  = %RHp\n", pVCpu->hwaccm.s.vmx.HCPhysVMCS));
        rc = VMXClearVMCS(pVCpu->hwaccm.s.vmx.HCPhysVMCS);
        if (RT_FAILURE(rc))
            goto vmx_end;

        rc = VMXActivateVMCS(pVCpu->hwaccm.s.vmx.HCPhysVMCS);
        if (RT_FAILURE(rc))
            goto vmx_end;

        /*
         * VMX_VMCS_CTRL_PIN_EXEC_CONTROLS
         * Set required bits to one and zero according to the MSR capabilities.
         */
        val = pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.disallowed0;
        val |=    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT      /* External interrupts */
                | VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT;         /* Non-maskable interrupts */

        /*
         * Enable the VMX preemption timer.
         */
        if (pVM->hwaccm.s.vmx.fUsePreemptTimer)
            val |= VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_PREEMPT_TIMER;
        val &= pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.allowed1;

        rc = VMXWriteVMCS(VMX_VMCS_CTRL_PIN_EXEC_CONTROLS, val);
        AssertRC(rc);

        /*
         * VMX_VMCS_CTRL_PROC_EXEC_CONTROLS
         * Set required bits to one and zero according to the MSR capabilities.
         */
        val = pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.disallowed0;
        /* Program which event cause VM-exits and which features we want to use. */
        val |=   VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT
               | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET
               | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT
               | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT
               | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT
               | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_EXIT
               | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT;     /* don't execute mwait or else we'll idle inside
                                                                      the guest (host thinks the cpu load is high) */

        /* Without nested paging we should intercept invlpg and cr3 mov instructions. */
        if (!pVM->hwaccm.s.fNestedPaging)
        {
            val |=   VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT
                   | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT
                   | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT;
        }

        /*
         * VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT might cause a vmlaunch
         * failure with an invalid control fields error. (combined with some other exit reasons)
         */
        if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW)
        {
            /* CR8 reads from the APIC shadow page; writes cause an exit is they lower the TPR below the threshold */
            val |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW;
            Assert(pVM->hwaccm.s.vmx.pAPIC);
        }
        else
            /* Exit on CR8 reads & writes in case the TPR shadow feature isn't present. */
            val |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT;

        if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS)
        {
            Assert(pVCpu->hwaccm.s.vmx.pMSRBitmapPhys);
            val |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS;
        }

        /* We will use the secondary control if it's present. */
        val |= VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL;

        /* Mask away the bits that the CPU doesn't support */
        /** @todo make sure they don't conflict with the above requirements. */
        val &= pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1;
        pVCpu->hwaccm.s.vmx.proc_ctls = val;

        rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, val);
        AssertRC(rc);

        if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
        {
            /*
             * VMX_VMCS_CTRL_PROC_EXEC_CONTROLS2
             * Set required bits to one and zero according to the MSR capabilities.
             */
            val  = pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.disallowed0;
            val |= VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT;

            if (pVM->hwaccm.s.fNestedPaging)
                val |= VMX_VMCS_CTRL_PROC_EXEC2_EPT;

            if (pVM->hwaccm.s.vmx.fVPID)
                val |= VMX_VMCS_CTRL_PROC_EXEC2_VPID;

            if (pVM->hwaccm.s.fHasIoApic)
                val |= VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC;

            if (pVM->hwaccm.s.vmx.fUnrestrictedGuest)
                val |= VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE;

            /* Mask away the bits that the CPU doesn't support */
            /** @todo make sure they don't conflict with the above requirements. */
            val &= pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1;
            pVCpu->hwaccm.s.vmx.proc_ctls2 = val;
            rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS2, val);
            AssertRC(rc);
        }

        /*
         * VMX_VMCS_CTRL_CR3_TARGET_COUNT
         * Set required bits to one and zero according to the MSR capabilities.
         */
        rc = VMXWriteVMCS(VMX_VMCS_CTRL_CR3_TARGET_COUNT, 0);
        AssertRC(rc);

        /*
         * Forward all exception except #NM & #PF to the guest.
         * We always need to check pagefaults since our shadow page table can be out of sync.
         * And we always lazily sync the FPU & XMM state.                                                           .
         */

        /** @todo Possible optimization:
         * Keep the FPU and XMM state current in the EM thread. That way there's no need to
         * lazily sync anything, but the downside is that we can't use the FPU stack or XMM
         * registers ourselves of course.
         *
         * Note: only possible if the current state is actually ours (X86_CR0_TS flag)
         */

        /*
         * Don't filter page faults, all of them should cause a world switch.
         */
        rc  = VMXWriteVMCS(VMX_VMCS_CTRL_PAGEFAULT_ERROR_MASK, 0);
        AssertRC(rc);
        rc = VMXWriteVMCS(VMX_VMCS_CTRL_PAGEFAULT_ERROR_MATCH, 0);
        AssertRC(rc);

        rc = VMXWriteVMCS64(VMX_VMCS_CTRL_TSC_OFFSET_FULL, 0);
        AssertRC(rc);
        rc = VMXWriteVMCS64(VMX_VMCS_CTRL_IO_BITMAP_A_FULL, 0);
        AssertRC(rc);
        rc = VMXWriteVMCS64(VMX_VMCS_CTRL_IO_BITMAP_B_FULL, 0);
        AssertRC(rc);

        /*
         * Set the MSR bitmap address.
         */
        if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS)
        {
            Assert(pVCpu->hwaccm.s.vmx.pMSRBitmapPhys);

            rc = VMXWriteVMCS64(VMX_VMCS_CTRL_MSR_BITMAP_FULL, pVCpu->hwaccm.s.vmx.pMSRBitmapPhys);
            AssertRC(rc);

            /* Allow the guest to directly modify these MSRs; they are restored and saved automatically. */
            hmR0VmxSetMSRPermission(pVCpu, MSR_IA32_SYSENTER_CS, true, true);
            hmR0VmxSetMSRPermission(pVCpu, MSR_IA32_SYSENTER_ESP, true, true);
            hmR0VmxSetMSRPermission(pVCpu, MSR_IA32_SYSENTER_EIP, true, true);
            hmR0VmxSetMSRPermission(pVCpu, MSR_K8_LSTAR, true, true);
            hmR0VmxSetMSRPermission(pVCpu, MSR_K6_STAR, true, true);
            hmR0VmxSetMSRPermission(pVCpu, MSR_K8_SF_MASK, true, true);
            hmR0VmxSetMSRPermission(pVCpu, MSR_K8_KERNEL_GS_BASE, true, true);
            hmR0VmxSetMSRPermission(pVCpu, MSR_K8_GS_BASE, true, true);
            hmR0VmxSetMSRPermission(pVCpu, MSR_K8_FS_BASE, true, true);
        }

#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
        /*
         * Set the guest & host MSR load/store physical addresses.
         */
        Assert(pVCpu->hwaccm.s.vmx.pGuestMSRPhys);
        rc = VMXWriteVMCS64(VMX_VMCS_CTRL_VMENTRY_MSR_LOAD_FULL, pVCpu->hwaccm.s.vmx.pGuestMSRPhys);
        AssertRC(rc);
        rc = VMXWriteVMCS64(VMX_VMCS_CTRL_VMEXIT_MSR_STORE_FULL, pVCpu->hwaccm.s.vmx.pGuestMSRPhys);
        AssertRC(rc);

        Assert(pVCpu->hwaccm.s.vmx.pHostMSRPhys);
        rc = VMXWriteVMCS64(VMX_VMCS_CTRL_VMEXIT_MSR_LOAD_FULL,  pVCpu->hwaccm.s.vmx.pHostMSRPhys);
        AssertRC(rc);
#endif /* VBOX_WITH_AUTO_MSR_LOAD_RESTORE */

        rc = VMXWriteVMCS(VMX_VMCS_CTRL_ENTRY_MSR_LOAD_COUNT, 0);
        AssertRC(rc);
        rc = VMXWriteVMCS(VMX_VMCS_CTRL_EXIT_MSR_STORE_COUNT, 0);
        AssertRC(rc);

        if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW)
        {
            Assert(pVM->hwaccm.s.vmx.pMemObjAPIC);
            /* Optional */
            rc  = VMXWriteVMCS(VMX_VMCS_CTRL_TPR_THRESHOLD, 0);
            rc |= VMXWriteVMCS64(VMX_VMCS_CTRL_VAPIC_PAGEADDR_FULL, pVCpu->hwaccm.s.vmx.HCPhysVAPIC);

            if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
                rc |= VMXWriteVMCS64(VMX_VMCS_CTRL_APIC_ACCESSADDR_FULL, pVM->hwaccm.s.vmx.pAPICPhys);

            AssertRC(rc);
        }

        /* Set link pointer to -1. Not currently used. */
        rc = VMXWriteVMCS64(VMX_VMCS_GUEST_LINK_PTR_FULL, 0xFFFFFFFFFFFFFFFFULL);
        AssertRC(rc);

        /*
         * Clear VMCS, marking it inactive. Clear implementation specific data and writing back
         * VMCS data back to memory.
         */
        rc = VMXClearVMCS(pVCpu->hwaccm.s.vmx.HCPhysVMCS);
        AssertRC(rc);

        /*
         * Configure the VMCS read cache.
         */
        PVMCSCACHE pCache = &pVCpu->hwaccm.s.vmx.VMCSCache;

        VMXSetupCachedReadVMCS(pCache, VMX_VMCS64_GUEST_RIP);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS64_GUEST_RSP);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS_GUEST_RFLAGS);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS_CTRL_CR0_READ_SHADOW);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS64_GUEST_CR0);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS_CTRL_CR4_READ_SHADOW);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS64_GUEST_CR4);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS64_GUEST_DR7);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_GUEST_SYSENTER_CS);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS64_GUEST_SYSENTER_EIP);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS64_GUEST_SYSENTER_ESP);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_GUEST_GDTR_LIMIT);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS64_GUEST_GDTR_BASE);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_GUEST_IDTR_LIMIT);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS64_GUEST_IDTR_BASE);

        VMX_SETUP_SELREG(ES,   pCache);
        VMX_SETUP_SELREG(SS,   pCache);
        VMX_SETUP_SELREG(CS,   pCache);
        VMX_SETUP_SELREG(DS,   pCache);
        VMX_SETUP_SELREG(FS,   pCache);
        VMX_SETUP_SELREG(GS,   pCache);
        VMX_SETUP_SELREG(LDTR, pCache);
        VMX_SETUP_SELREG(TR,   pCache);

        /*
         * Status code VMCS reads.
         */
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_RO_EXIT_REASON);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_RO_VM_INSTR_ERROR);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_RO_EXIT_INSTR_LENGTH);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_RO_EXIT_INTERRUPTION_ERRCODE);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_RO_EXIT_INSTR_INFO);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS_RO_EXIT_QUALIFICATION);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_RO_IDT_INFO);
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_RO_IDT_ERRCODE);

        if (pVM->hwaccm.s.fNestedPaging)
        {
            VMXSetupCachedReadVMCS(pCache, VMX_VMCS64_GUEST_CR3);
            VMXSetupCachedReadVMCS(pCache, VMX_VMCS_EXIT_PHYS_ADDR_FULL);
            pCache->Read.cValidEntries = VMX_VMCS_MAX_NESTED_PAGING_CACHE_IDX;
        }
        else
            pCache->Read.cValidEntries = VMX_VMCS_MAX_CACHE_IDX;
    } /* for each VMCPU */

    /*
     * Setup the right TLB function based on CPU capabilities.
     */
    if (pVM->hwaccm.s.fNestedPaging && pVM->hwaccm.s.vmx.fVPID)
        pVM->hwaccm.s.vmx.pfnSetupTaggedTLB = hmR0VmxSetupTLBBoth;
    else if (pVM->hwaccm.s.fNestedPaging)
        pVM->hwaccm.s.vmx.pfnSetupTaggedTLB = hmR0VmxSetupTLBEPT;
    else if (pVM->hwaccm.s.vmx.fVPID)
        pVM->hwaccm.s.vmx.pfnSetupTaggedTLB = hmR0VmxSetupTLBVPID;
    else
        pVM->hwaccm.s.vmx.pfnSetupTaggedTLB = hmR0VmxSetupTLBDummy;

vmx_end:
    hmR0VmxCheckError(pVM, &pVM->aCpus[0], rc);
    return rc;
}


/**
 * Sets the permission bits for the specified MSR.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   ulMSR       The MSR value.
 * @param   fRead       Whether reading is allowed.
 * @param   fWrite      Whether writing is allowed.
 */
static void hmR0VmxSetMSRPermission(PVMCPU pVCpu, unsigned ulMSR, bool fRead, bool fWrite)
{
    unsigned ulBit;
    uint8_t *pMSRBitmap = (uint8_t *)pVCpu->hwaccm.s.vmx.pMSRBitmap;

    /*
     * Layout:
     * 0x000 - 0x3ff - Low MSR read bits
     * 0x400 - 0x7ff - High MSR read bits
     * 0x800 - 0xbff - Low MSR write bits
     * 0xc00 - 0xfff - High MSR write bits
     */
    if (ulMSR <= 0x00001FFF)
    {
        /* Pentium-compatible MSRs */
        ulBit    = ulMSR;
    }
    else if (   ulMSR >= 0xC0000000
             && ulMSR <= 0xC0001FFF)
    {
        /* AMD Sixth Generation x86 Processor MSRs */
        ulBit = (ulMSR - 0xC0000000);
        pMSRBitmap += 0x400;
    }
    else
    {
        AssertFailed();
        return;
    }

    Assert(ulBit <= 0x1fff);
    if (fRead)
        ASMBitClear(pMSRBitmap, ulBit);
    else
        ASMBitSet(pMSRBitmap, ulBit);

    if (fWrite)
        ASMBitClear(pMSRBitmap + 0x800, ulBit);
    else
        ASMBitSet(pMSRBitmap + 0x800, ulBit);
}


/**
 * Injects an event (trap or external interrupt).
 *
 * @returns VBox status code.  Note that it may return VINF_EM_RESET to
 *          indicate a triple fault when injecting X86_XCPT_DF.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU Context.
 * @param   intInfo     VMX interrupt info.
 * @param   cbInstr     Opcode length of faulting instruction.
 * @param   errCode     Error code (optional).
 */
static int hmR0VmxInjectEvent(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, uint32_t intInfo, uint32_t cbInstr, uint32_t errCode)
{
    int         rc;
    uint32_t    iGate = VMX_EXIT_INTERRUPTION_INFO_VECTOR(intInfo);

#ifdef VBOX_WITH_STATISTICS
    STAM_COUNTER_INC(&pVCpu->hwaccm.s.paStatInjectedIrqsR0[iGate & MASK_INJECT_IRQ_STAT]);
#endif

#ifdef VBOX_STRICT
    if (iGate == 0xE)
    {
        LogFlow(("hmR0VmxInjectEvent: Injecting interrupt %d at %RGv error code=%08x CR2=%RGv intInfo=%08x\n", iGate,
                 (RTGCPTR)pCtx->rip, errCode, pCtx->cr2, intInfo));
    }
    else if (iGate < 0x20)
    {
        LogFlow(("hmR0VmxInjectEvent: Injecting interrupt %d at %RGv error code=%08x\n", iGate, (RTGCPTR)pCtx->rip,
                 errCode));
    }
    else
    {
        LogFlow(("INJ-EI: %x at %RGv\n", iGate, (RTGCPTR)pCtx->rip));
        Assert(   VMX_EXIT_INTERRUPTION_INFO_TYPE(intInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_SW
               || !VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));
        Assert(   VMX_EXIT_INTERRUPTION_INFO_TYPE(intInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_SW
               || pCtx->eflags.u32 & X86_EFL_IF);
    }
#endif

    if (    CPUMIsGuestInRealModeEx(pCtx)
        &&  pVM->hwaccm.s.vmx.pRealModeTSS)
    {
        RTGCPHYS GCPhysHandler;
        uint16_t offset, ip;
        RTSEL    sel;

        /*
         * Injecting events doesn't work right with real mode emulation.
         * (#GP if we try to inject external hardware interrupts)
         * Inject the interrupt or trap directly instead.
         *
         * ASSUMES no access handlers for the bits we read or write below (should be safe).
         */
        Log(("Manual interrupt/trap '%x' inject (real mode)\n", iGate));

        /*
         * Check if the interrupt handler is present.
         */
        if (iGate * 4 + 3 > pCtx->idtr.cbIdt)
        {
            Log(("IDT cbIdt violation\n"));
            if (iGate != X86_XCPT_DF)
            {
                uint32_t intInfo2;

                intInfo2  = (iGate == X86_XCPT_GP) ? (uint32_t)X86_XCPT_DF : iGate;
                intInfo2 |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
                intInfo2 |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
                intInfo2 |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HWEXCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

                return hmR0VmxInjectEvent(pVM, pVCpu, pCtx, intInfo2, 0, 0 /* no error code according to the Intel docs */);
            }
            Log(("Triple fault -> reset the VM!\n"));
            return VINF_EM_RESET;
        }
        if (    VMX_EXIT_INTERRUPTION_INFO_TYPE(intInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_SW
            ||  iGate == 3 /* Both #BP and #OF point to the instruction after. */
            ||  iGate == 4)
        {
            ip = pCtx->ip + cbInstr;
        }
        else
            ip = pCtx->ip;

        /*
         * Read the selector:offset pair of the interrupt handler.
         */
        GCPhysHandler = (RTGCPHYS)pCtx->idtr.pIdt + iGate * 4;
        rc = PGMPhysSimpleReadGCPhys(pVM, &offset, GCPhysHandler,     sizeof(offset)); AssertRC(rc);
        rc = PGMPhysSimpleReadGCPhys(pVM, &sel,    GCPhysHandler + 2, sizeof(sel));    AssertRC(rc);

        LogFlow(("IDT handler %04X:%04X\n", sel, offset));

        /*
         * Construct the stack frame.
         */
        /** @todo Check stack limit. */
        pCtx->sp -= 2;
        LogFlow(("ss:sp %04X:%04X eflags=%x\n", pCtx->ss, pCtx->sp, pCtx->eflags.u));
        rc = PGMPhysSimpleWriteGCPhys(pVM, pCtx->ssHid.u64Base + pCtx->sp, &pCtx->eflags, sizeof(uint16_t)); AssertRC(rc);
        pCtx->sp -= 2;
        LogFlow(("ss:sp %04X:%04X cs=%x\n", pCtx->ss, pCtx->sp, pCtx->cs));
        rc = PGMPhysSimpleWriteGCPhys(pVM, pCtx->ssHid.u64Base + pCtx->sp, &pCtx->cs, sizeof(uint16_t)); AssertRC(rc);
        pCtx->sp -= 2;
        LogFlow(("ss:sp %04X:%04X ip=%x\n", pCtx->ss, pCtx->sp, ip));
        rc = PGMPhysSimpleWriteGCPhys(pVM, pCtx->ssHid.u64Base + pCtx->sp, &ip, sizeof(ip)); AssertRC(rc);

        /*
         * Update the CPU state for executing the handler.
         */
        pCtx->rip           = offset;
        pCtx->cs            = sel;
        pCtx->csHid.u64Base = sel << 4;
        pCtx->eflags.u     &= ~(X86_EFL_IF | X86_EFL_TF | X86_EFL_RF | X86_EFL_AC);

        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_SEGMENT_REGS;
        return VINF_SUCCESS;
    }

    /*
     * Set event injection state.
     */
    rc  = VMXWriteVMCS(VMX_VMCS_CTRL_ENTRY_IRQ_INFO, intInfo | (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT));
    rc |= VMXWriteVMCS(VMX_VMCS_CTRL_ENTRY_INSTR_LENGTH, cbInstr);
    rc |= VMXWriteVMCS(VMX_VMCS_CTRL_ENTRY_EXCEPTION_ERRCODE, errCode);

    AssertRC(rc);
    return rc;
}


/**
 * Checks for pending guest interrupts and injects them.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
static int hmR0VmxCheckPendingInterrupt(PVM pVM, PVMCPU pVCpu, CPUMCTX *pCtx)
{
    int rc;

    /*
     * Dispatch any pending interrupts (injected before, but a VM exit occurred prematurely).
     */
    if (pVCpu->hwaccm.s.Event.fPending)
    {
        Log(("CPU%d: Reinjecting event %RX64 %08x at %RGv cr2=%RX64\n", pVCpu->idCpu, pVCpu->hwaccm.s.Event.intInfo,
             pVCpu->hwaccm.s.Event.errCode, (RTGCPTR)pCtx->rip, pCtx->cr2));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatIntReinject);
        rc = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, pVCpu->hwaccm.s.Event.intInfo, 0, pVCpu->hwaccm.s.Event.errCode);
        AssertRC(rc);

        pVCpu->hwaccm.s.Event.fPending = false;
        return VINF_SUCCESS;
    }

    /*
     * If an active trap is already pending, we must forward it first!
     */
    if (!TRPMHasTrap(pVCpu))
    {
        if (VMCPU_FF_TESTANDCLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI))
        {
            RTGCUINTPTR intInfo;

            Log(("CPU%d: injecting #NMI\n", pVCpu->idCpu));

            intInfo  = X86_XCPT_NMI;
            intInfo |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
            intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

            rc = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, intInfo, 0, 0);
            AssertRC(rc);

            return VINF_SUCCESS;
        }

        /** @todo SMI interrupts. */

        /*
         * When external interrupts are pending, we should exit the VM when IF is set.
         */
        if (VMCPU_FF_ISPENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC|VMCPU_FF_INTERRUPT_PIC)))
        {
            if (!(pCtx->eflags.u32 & X86_EFL_IF))
            {
                if (!(pVCpu->hwaccm.s.vmx.proc_ctls & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_IRQ_WINDOW_EXIT))
                {
                    LogFlow(("Enable irq window exit!\n"));
                    pVCpu->hwaccm.s.vmx.proc_ctls |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_IRQ_WINDOW_EXIT;
                    rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
                    AssertRC(rc);
                }
                /* else nothing to do but wait */
            }
            else if (!VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
            {
                uint8_t u8Interrupt;

                rc = PDMGetInterrupt(pVCpu, &u8Interrupt);
                Log(("CPU%d: Dispatch interrupt: u8Interrupt=%x (%d) rc=%Rrc cs:rip=%04X:%RGv\n", pVCpu->idCpu,
                     u8Interrupt, u8Interrupt, rc, pCtx->cs, (RTGCPTR)pCtx->rip));
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
            else
                Log(("Pending interrupt blocked at %RGv by VM_FF_INHIBIT_INTERRUPTS!!\n", (RTGCPTR)pCtx->rip));
        }
    }

#ifdef VBOX_STRICT
    if (TRPMHasTrap(pVCpu))
    {
        uint8_t u8Vector;
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
        TRPMEVENT   enmType;
        RTGCUINTPTR intInfo;
        RTGCUINT    errCode;

        /*
         * If a new event is pending, dispatch it now.
         */
        rc = TRPMQueryTrapAll(pVCpu, &u8Vector, &enmType, &errCode, 0);
        AssertRC(rc);
        Assert(pCtx->eflags.Bits.u1IF == 1 || enmType == TRPM_TRAP);
        Assert(enmType != TRPM_SOFTWARE_INT);

        /*
         * Clear the pending trap.
         */
        rc = TRPMResetTrap(pVCpu);
        AssertRC(rc);

        intInfo  = u8Vector;
        intInfo |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);

        if (enmType == TRPM_TRAP)
        {
            switch (u8Vector)
            {
                case X86_XCPT_DF:
                case X86_XCPT_TS:
                case X86_XCPT_NP:
                case X86_XCPT_SS:
                case X86_XCPT_GP:
                case X86_XCPT_PF:
                case X86_XCPT_AC:
                {
                    /* Valid error codes. */
                    intInfo |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
                    break;
                }

                default:
                    break;
            }

            if (   u8Vector == X86_XCPT_BP
                || u8Vector == X86_XCPT_OF)
            {
                intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SWEXCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
            }
            else
                intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HWEXCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
        }
        else
            intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatIntInject);
        rc = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, intInfo, 0, errCode);
        AssertRC(rc);
    } /* if (interrupts can be dispatched) */

    return VINF_SUCCESS;
}


/**
 * Save the host state into the VMCS.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
VMMR0DECL(int) VMXR0SaveHostState(PVM pVM, PVMCPU pVCpu)
{
    int rc = VINF_SUCCESS;
    NOREF(pVM);

    /*
     * Host CPU Context.
     */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_HOST_CONTEXT)
    {
        RTIDTR      idtr;
        RTGDTR      gdtr;
        RTSEL       SelTR;
        PCX86DESCHC pDesc;
        uintptr_t   trBase;
        RTSEL       cs;
        RTSEL       ss;
        uint64_t    cr3;

        /*
         * Control registers.
         */
        rc  = VMXWriteVMCS(VMX_VMCS_HOST_CR0,           ASMGetCR0());
        Log2(("VMX_VMCS_HOST_CR0 %08x\n",               ASMGetCR0()));
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        if (VMX_IS_64BIT_HOST_MODE())
        {
            cr3 = hwaccmR0Get64bitCR3();
            rc |= VMXWriteVMCS64(VMX_VMCS_HOST_CR3,     cr3);
        }
        else
#endif
        {
            cr3 = ASMGetCR3();
            rc |= VMXWriteVMCS(VMX_VMCS_HOST_CR3,       cr3);
        }
        Log2(("VMX_VMCS_HOST_CR3 %08RX64\n",            cr3));
        rc |= VMXWriteVMCS(VMX_VMCS_HOST_CR4,           ASMGetCR4());
        Log2(("VMX_VMCS_HOST_CR4 %08x\n",               ASMGetCR4()));
        AssertRC(rc);

        /*
         * Selector registers.
         */
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        if (VMX_IS_64BIT_HOST_MODE())
        {
            cs = (RTSEL)(uintptr_t)&SUPR0Abs64bitKernelCS;
            ss = (RTSEL)(uintptr_t)&SUPR0Abs64bitKernelSS;
        }
        else
        {
            /* sysenter loads LDT cs & ss, VMX doesn't like this. Load the GDT ones (safe). */
            cs = (RTSEL)(uintptr_t)&SUPR0AbsKernelCS;
            ss = (RTSEL)(uintptr_t)&SUPR0AbsKernelSS;
        }
#else
        cs = ASMGetCS();
        ss = ASMGetSS();
#endif
        Assert(!(cs & X86_SEL_LDT)); Assert((cs & X86_SEL_RPL) == 0);
        Assert(!(ss & X86_SEL_LDT)); Assert((ss & X86_SEL_RPL) == 0);
        rc  = VMXWriteVMCS(VMX_VMCS16_HOST_FIELD_CS,          cs);
        /* Note: VMX is (again) very picky about the RPL of the selectors here; we'll restore them manually. */
        rc |= VMXWriteVMCS(VMX_VMCS16_HOST_FIELD_DS,          0);
        rc |= VMXWriteVMCS(VMX_VMCS16_HOST_FIELD_ES,          0);
#if HC_ARCH_BITS == 32
        if (!VMX_IS_64BIT_HOST_MODE())
        {
            rc |= VMXWriteVMCS(VMX_VMCS16_HOST_FIELD_FS,      0);
            rc |= VMXWriteVMCS(VMX_VMCS16_HOST_FIELD_GS,      0);
        }
#endif
        rc |= VMXWriteVMCS(VMX_VMCS16_HOST_FIELD_SS,          ss);
        SelTR = ASMGetTR();
        rc |= VMXWriteVMCS(VMX_VMCS16_HOST_FIELD_TR,          SelTR);
        AssertRC(rc);
        Log2(("VMX_VMCS_HOST_FIELD_CS %08x (%08x)\n", cs, ASMGetSS()));
        Log2(("VMX_VMCS_HOST_FIELD_DS 00000000 (%08x)\n", ASMGetDS()));
        Log2(("VMX_VMCS_HOST_FIELD_ES 00000000 (%08x)\n", ASMGetES()));
        Log2(("VMX_VMCS_HOST_FIELD_FS 00000000 (%08x)\n", ASMGetFS()));
        Log2(("VMX_VMCS_HOST_FIELD_GS 00000000 (%08x)\n", ASMGetGS()));
        Log2(("VMX_VMCS_HOST_FIELD_SS %08x (%08x)\n", ss, ASMGetSS()));
        Log2(("VMX_VMCS_HOST_FIELD_TR %08x\n", ASMGetTR()));

        /*
         * GDTR & IDTR.
         */
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        if (VMX_IS_64BIT_HOST_MODE())
        {
            X86XDTR64 gdtr64, idtr64;
            hwaccmR0Get64bitGDTRandIDTR(&gdtr64, &idtr64);
            rc  = VMXWriteVMCS64(VMX_VMCS_HOST_GDTR_BASE, gdtr64.uAddr);
            rc |= VMXWriteVMCS64(VMX_VMCS_HOST_IDTR_BASE, gdtr64.uAddr);
            AssertRC(rc);
            Log2(("VMX_VMCS_HOST_GDTR_BASE %RX64\n", gdtr64.uAddr));
            Log2(("VMX_VMCS_HOST_IDTR_BASE %RX64\n", idtr64.uAddr));
            gdtr.cbGdt = gdtr64.cb;
            gdtr.pGdt  = (uintptr_t)gdtr64.uAddr;
        }
        else
#endif
        {
            ASMGetGDTR(&gdtr);
            rc  = VMXWriteVMCS(VMX_VMCS_HOST_GDTR_BASE, gdtr.pGdt);
            ASMGetIDTR(&idtr);
            rc |= VMXWriteVMCS(VMX_VMCS_HOST_IDTR_BASE, idtr.pIdt);
            AssertRC(rc);
            Log2(("VMX_VMCS_HOST_GDTR_BASE %RHv\n", gdtr.pGdt));
            Log2(("VMX_VMCS_HOST_IDTR_BASE %RHv\n", idtr.pIdt));
        }

        /*
         * Save the base address of the TR selector.
         */
        if (SelTR > gdtr.cbGdt)
        {
            AssertMsgFailed(("Invalid TR selector %x. GDTR.cbGdt=%x\n", SelTR, gdtr.cbGdt));
            return VERR_VMX_INVALID_HOST_STATE;
        }

        pDesc = (PCX86DESCHC)(gdtr.pGdt + (SelTR & X86_SEL_MASK));
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        if (VMX_IS_64BIT_HOST_MODE())
        {
            uint64_t trBase64 = X86DESC64_BASE(*(PX86DESC64)pDesc);
            rc = VMXWriteVMCS64(VMX_VMCS_HOST_TR_BASE, trBase64);
            Log2(("VMX_VMCS_HOST_TR_BASE %RX64\n", trBase64));
            AssertRC(rc);
        }
        else
#endif
        {
#if HC_ARCH_BITS == 64
            trBase = X86DESC64_BASE(*pDesc);
#else
            trBase = X86DESC_BASE(*pDesc);
#endif
            rc = VMXWriteVMCS(VMX_VMCS_HOST_TR_BASE, trBase);
            AssertRC(rc);
            Log2(("VMX_VMCS_HOST_TR_BASE %RHv\n", trBase));
        }

        /*
         * FS base and GS base.
         */
#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        if (VMX_IS_64BIT_HOST_MODE())
        {
            Log2(("MSR_K8_FS_BASE = %RX64\n", ASMRdMsr(MSR_K8_FS_BASE)));
            Log2(("MSR_K8_GS_BASE = %RX64\n", ASMRdMsr(MSR_K8_GS_BASE)));
            rc  = VMXWriteVMCS64(VMX_VMCS_HOST_FS_BASE,         ASMRdMsr(MSR_K8_FS_BASE));
            rc |= VMXWriteVMCS64(VMX_VMCS_HOST_GS_BASE,         ASMRdMsr(MSR_K8_GS_BASE));
        }
#endif
        AssertRC(rc);

        /*
         * Sysenter MSRs.
         */
        /** @todo expensive!! */
        rc  = VMXWriteVMCS(VMX_VMCS32_HOST_SYSENTER_CS,         ASMRdMsr_Low(MSR_IA32_SYSENTER_CS));
        Log2(("VMX_VMCS_HOST_SYSENTER_CS  %08x\n",              ASMRdMsr_Low(MSR_IA32_SYSENTER_CS)));
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        if (VMX_IS_64BIT_HOST_MODE())
        {
            Log2(("VMX_VMCS_HOST_SYSENTER_EIP %RX64\n",         ASMRdMsr(MSR_IA32_SYSENTER_EIP)));
            Log2(("VMX_VMCS_HOST_SYSENTER_ESP %RX64\n",         ASMRdMsr(MSR_IA32_SYSENTER_ESP)));
            rc |= VMXWriteVMCS64(VMX_VMCS_HOST_SYSENTER_ESP,    ASMRdMsr(MSR_IA32_SYSENTER_ESP));
            rc |= VMXWriteVMCS64(VMX_VMCS_HOST_SYSENTER_EIP,    ASMRdMsr(MSR_IA32_SYSENTER_EIP));
        }
        else
        {
            rc |= VMXWriteVMCS(VMX_VMCS_HOST_SYSENTER_ESP,      ASMRdMsr_Low(MSR_IA32_SYSENTER_ESP));
            rc |= VMXWriteVMCS(VMX_VMCS_HOST_SYSENTER_EIP,      ASMRdMsr_Low(MSR_IA32_SYSENTER_EIP));
            Log2(("VMX_VMCS_HOST_SYSENTER_EIP %RX32\n",         ASMRdMsr_Low(MSR_IA32_SYSENTER_EIP)));
            Log2(("VMX_VMCS_HOST_SYSENTER_ESP %RX32\n",         ASMRdMsr_Low(MSR_IA32_SYSENTER_ESP)));
        }
#elif HC_ARCH_BITS == 32
        rc |= VMXWriteVMCS(VMX_VMCS_HOST_SYSENTER_ESP,          ASMRdMsr_Low(MSR_IA32_SYSENTER_ESP));
        rc |= VMXWriteVMCS(VMX_VMCS_HOST_SYSENTER_EIP,          ASMRdMsr_Low(MSR_IA32_SYSENTER_EIP));
        Log2(("VMX_VMCS_HOST_SYSENTER_EIP %RX32\n",             ASMRdMsr_Low(MSR_IA32_SYSENTER_EIP)));
        Log2(("VMX_VMCS_HOST_SYSENTER_ESP %RX32\n",             ASMRdMsr_Low(MSR_IA32_SYSENTER_ESP)));
#else
        Log2(("VMX_VMCS_HOST_SYSENTER_EIP %RX64\n",             ASMRdMsr(MSR_IA32_SYSENTER_EIP)));
        Log2(("VMX_VMCS_HOST_SYSENTER_ESP %RX64\n",             ASMRdMsr(MSR_IA32_SYSENTER_ESP)));
            rc |= VMXWriteVMCS64(VMX_VMCS_HOST_SYSENTER_ESP,    ASMRdMsr(MSR_IA32_SYSENTER_ESP));
        rc |= VMXWriteVMCS64(VMX_VMCS_HOST_SYSENTER_EIP,        ASMRdMsr(MSR_IA32_SYSENTER_EIP));
#endif
        AssertRC(rc);


#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
        /*
         * Store all host MSRs in the VM-Exit load area, so they will be reloaded after
         * the world switch back to the host.
         */
        PVMXMSR pMsr = (PVMXMSR)pVCpu->hwaccm.s.vmx.pHostMSR;
        unsigned idxMsr = 0;

        /*
         * Check if EFER MSR present.
         */
        if (ASMCpuId_EDX(0x80000001) & (X86_CPUID_AMD_FEATURE_EDX_NX|X86_CPUID_AMD_FEATURE_EDX_LONG_MODE))
        {
            if (ASMCpuId_EDX(0x80000001) & X86_CPUID_AMD_FEATURE_EDX_SEP)
            {
                pMsr->u32IndexMSR = MSR_K6_STAR;
                pMsr->u32Reserved = 0;
                pMsr->u64Value    = ASMRdMsr(MSR_K6_STAR);                   /* legacy syscall eip, cs & ss */
                pMsr++; idxMsr++;
            }

            pMsr->u32IndexMSR = MSR_K6_EFER;
            pMsr->u32Reserved = 0;
# if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
            if (CPUMIsGuestInLongMode(pVCpu))
            {
                /* Must match the EFER value in our 64 bits switcher. */
                pMsr->u64Value    = ASMRdMsr(MSR_K6_EFER) | MSR_K6_EFER_LME | MSR_K6_EFER_SCE | MSR_K6_EFER_NXE;
            }
            else
# endif
                pMsr->u64Value    = ASMRdMsr(MSR_K6_EFER);
            pMsr++; idxMsr++;
        }

# if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        if (VMX_IS_64BIT_HOST_MODE())
        {
            pMsr->u32IndexMSR = MSR_K8_LSTAR;
            pMsr->u32Reserved = 0;
            pMsr->u64Value    = ASMRdMsr(MSR_K8_LSTAR);             /* 64 bits mode syscall rip */
            pMsr++; idxMsr++;
            pMsr->u32IndexMSR = MSR_K8_SF_MASK;
            pMsr->u32Reserved = 0;
            pMsr->u64Value    = ASMRdMsr(MSR_K8_SF_MASK);           /* syscall flag mask */
            pMsr++; idxMsr++;
            pMsr->u32IndexMSR = MSR_K8_KERNEL_GS_BASE;
            pMsr->u32Reserved = 0;
            pMsr->u64Value    = ASMRdMsr(MSR_K8_KERNEL_GS_BASE);    /* swapgs exchange value */
            pMsr++; idxMsr++;
        }
# endif
        rc = VMXWriteVMCS(VMX_VMCS_CTRL_EXIT_MSR_LOAD_COUNT, idxMsr);
        AssertRC(rc);
#endif /* VBOX_WITH_AUTO_MSR_LOAD_RESTORE */

        pVCpu->hwaccm.s.fContextUseFlags &= ~HWACCM_CHANGED_HOST_CONTEXT;
    }
    return rc;
}


/**
 * Loads the 4 PDPEs into the guest state when nested paging is used and the
 * guest operates in PAE mode.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
static int hmR0VmxLoadPaePdpes(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    if (CPUMIsGuestInPAEModeEx(pCtx))
    {
        X86PDPE aPdpes[4];
        int rc = PGMGstGetPaePdpes(pVCpu, &aPdpes[0]);
        AssertRCReturn(rc, rc);

        rc = VMXWriteVMCS64(VMX_VMCS_GUEST_PDPTR0_FULL, aPdpes[0].u); AssertRCReturn(rc, rc);
        rc = VMXWriteVMCS64(VMX_VMCS_GUEST_PDPTR1_FULL, aPdpes[1].u); AssertRCReturn(rc, rc);
        rc = VMXWriteVMCS64(VMX_VMCS_GUEST_PDPTR2_FULL, aPdpes[2].u); AssertRCReturn(rc, rc);
        rc = VMXWriteVMCS64(VMX_VMCS_GUEST_PDPTR3_FULL, aPdpes[3].u); AssertRCReturn(rc, rc);
    }
    return VINF_SUCCESS;
}


/**
 * Saves the 4 PDPEs into the guest state when nested paging is used and the
 * guest operates in PAE mode.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VM CPU.
 * @param   pCtx        Pointer to the guest CPU context.
 *
 * @remarks Tell PGM about CR3 changes before calling this helper.
 */
static int hmR0VmxSavePaePdpes(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    if (CPUMIsGuestInPAEModeEx(pCtx))
    {
        int rc;
        X86PDPE aPdpes[4];
        rc = VMXReadVMCS64(VMX_VMCS_GUEST_PDPTR0_FULL, &aPdpes[0].u); AssertRCReturn(rc, rc);
        rc = VMXReadVMCS64(VMX_VMCS_GUEST_PDPTR1_FULL, &aPdpes[1].u); AssertRCReturn(rc, rc);
        rc = VMXReadVMCS64(VMX_VMCS_GUEST_PDPTR2_FULL, &aPdpes[2].u); AssertRCReturn(rc, rc);
        rc = VMXReadVMCS64(VMX_VMCS_GUEST_PDPTR3_FULL, &aPdpes[3].u); AssertRCReturn(rc, rc);

        rc = PGMGstUpdatePaePdpes(pVCpu, &aPdpes[0]);
        AssertRCReturn(rc, rc);
    }
    return VINF_SUCCESS;
}


/**
 * Update the exception bitmap according to the current CPU state.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
static void hmR0VmxUpdateExceptionBitmap(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    uint32_t u32TrapMask;
    Assert(pCtx);

    /*
     * Set up a mask for intercepting traps.
     */
    /** @todo Do we really need to always intercept #DB? */
    u32TrapMask  =   RT_BIT(X86_XCPT_DB)
                   | RT_BIT(X86_XCPT_NM)
#ifdef VBOX_ALWAYS_TRAP_PF
                   | RT_BIT(X86_XCPT_PF)
#endif
#ifdef VBOX_STRICT
                   | RT_BIT(X86_XCPT_BP)
                   | RT_BIT(X86_XCPT_DB)
                   | RT_BIT(X86_XCPT_DE)
                   | RT_BIT(X86_XCPT_NM)
                   | RT_BIT(X86_XCPT_UD)
                   | RT_BIT(X86_XCPT_NP)
                   | RT_BIT(X86_XCPT_SS)
                   | RT_BIT(X86_XCPT_GP)
                   | RT_BIT(X86_XCPT_MF)
#endif
                   ;

    /*
     * Without nested paging, #PF must be intercepted to implement shadow paging.
     */
    /** @todo NP state won't change so maybe we should build the initial trap mask up front? */
    if (!pVM->hwaccm.s.fNestedPaging)
        u32TrapMask |= RT_BIT(X86_XCPT_PF);

    /* Catch floating point exceptions if we need to report them to the guest in a different way. */
    if (!(pCtx->cr0 & X86_CR0_NE))
        u32TrapMask |= RT_BIT(X86_XCPT_MF);

#ifdef VBOX_STRICT
    Assert(u32TrapMask & RT_BIT(X86_XCPT_GP));
#endif

    /*
     * Intercept all exceptions in real mode as none of them can be injected directly (#GP otherwise).
     */
    /** @todo Despite the claim to intercept everything, with NP we do not intercept #PF. Should we? */
    if (    CPUMIsGuestInRealModeEx(pCtx)
        &&  pVM->hwaccm.s.vmx.pRealModeTSS)
    {
        u32TrapMask |=   RT_BIT(X86_XCPT_DE)
                       | RT_BIT(X86_XCPT_DB)
                       | RT_BIT(X86_XCPT_NMI)
                       | RT_BIT(X86_XCPT_BP)
                       | RT_BIT(X86_XCPT_OF)
                       | RT_BIT(X86_XCPT_BR)
                       | RT_BIT(X86_XCPT_UD)
                       | RT_BIT(X86_XCPT_DF)
                       | RT_BIT(X86_XCPT_CO_SEG_OVERRUN)
                       | RT_BIT(X86_XCPT_TS)
                       | RT_BIT(X86_XCPT_NP)
                       | RT_BIT(X86_XCPT_SS)
                       | RT_BIT(X86_XCPT_GP)
                       | RT_BIT(X86_XCPT_MF)
                       | RT_BIT(X86_XCPT_AC)
                       | RT_BIT(X86_XCPT_MC)
                       | RT_BIT(X86_XCPT_XF)
                       ;
    }

    int rc = VMXWriteVMCS(VMX_VMCS_CTRL_EXCEPTION_BITMAP, u32TrapMask);
    AssertRC(rc);
}


/**
 * Loads a minimal guest state.
 *
 * NOTE: Don't do anything here that can cause a jump back to ring 3!!!!!
 *
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
VMMR0DECL(void) VMXR0LoadMinimalGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    int rc;
    X86EFLAGS   eflags;

    Assert(!(pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_ALL_GUEST));

    /*
     * Load EIP, ESP and EFLAGS.
     */
    rc  = VMXWriteVMCS64(VMX_VMCS64_GUEST_RIP, pCtx->rip);
    rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_RSP, pCtx->rsp);
    AssertRC(rc);

    /*
     * Bits 22-31, 15, 5 & 3 must be zero. Bit 1 must be 1.
     */
    eflags      = pCtx->eflags;
    eflags.u32 &= VMX_EFLAGS_RESERVED_0;
    eflags.u32 |= VMX_EFLAGS_RESERVED_1;

    /*
     * Check if real mode emulation using v86 mode.
     */
    if (    CPUMIsGuestInRealModeEx(pCtx)
        &&  pVM->hwaccm.s.vmx.pRealModeTSS)
    {
        pVCpu->hwaccm.s.vmx.RealMode.eflags = eflags;

        eflags.Bits.u1VM   = 1;
        eflags.Bits.u2IOPL = 0; /* must always be 0 or else certain instructions won't cause faults. */
    }
    rc   = VMXWriteVMCS(VMX_VMCS_GUEST_RFLAGS,           eflags.u32);
    AssertRC(rc);
}


/**
 * Loads the guest state.
 *
 * NOTE: Don't do anything here that can cause a jump back to ring 3!!!!!
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
VMMR0DECL(int) VMXR0LoadGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    int         rc = VINF_SUCCESS;
    RTGCUINTPTR val;

    /*
     * VMX_VMCS_CTRL_ENTRY_CONTROLS
     * Set required bits to one and zero according to the MSR capabilities.
     */
    val  = pVM->hwaccm.s.vmx.msr.vmx_entry.n.disallowed0;

    /*
     * Load guest debug controls (DR7 & IA32_DEBUGCTL_MSR).
     * Forced to 1 on the 'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs
     */
    val |= VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_DEBUG;

    if (CPUMIsGuestInLongModeEx(pCtx))
        val |= VMX_VMCS_CTRL_ENTRY_CONTROLS_IA64_MODE;
    /* else Must be zero when AMD64 is not available. */

    /*
     * Mask away the bits that the CPU doesn't support.
     */
    val &= pVM->hwaccm.s.vmx.msr.vmx_entry.n.allowed1;
    rc = VMXWriteVMCS(VMX_VMCS_CTRL_ENTRY_CONTROLS, val);
    AssertRC(rc);

    /*
     * VMX_VMCS_CTRL_EXIT_CONTROLS
     * Set required bits to one and zero according to the MSR capabilities.
     */
    val  = pVM->hwaccm.s.vmx.msr.vmx_exit.n.disallowed0;

    /*
     * Save debug controls (DR7 & IA32_DEBUGCTL_MSR)
     * Forced to 1 on the 'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs
     */
    val |= VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG;

#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (VMX_IS_64BIT_HOST_MODE())
        val |= VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64;
    /* else Must be zero when AMD64 is not available. */
#elif HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
    if (CPUMIsGuestInLongModeEx(pCtx))
        val |= VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64;      /* our switcher goes to long mode */
    else
        Assert(!(val & VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64));
#endif
    val &= pVM->hwaccm.s.vmx.msr.vmx_exit.n.allowed1;

    /*
     * Don't acknowledge external interrupts on VM-exit.
     */
    rc = VMXWriteVMCS(VMX_VMCS_CTRL_EXIT_CONTROLS, val);
    AssertRC(rc);

    /*
     * Guest CPU context: ES, CS, SS, DS, FS, GS.
     */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_SEGMENT_REGS)
    {
        if (pVM->hwaccm.s.vmx.pRealModeTSS)
        {
            PGMMODE enmGuestMode = PGMGetGuestMode(pVCpu);
            if (pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode != enmGuestMode)
            {
                /*
                 * Correct weird requirements for switching to protected mode.
                 */
                if (    pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode == PGMMODE_REAL
                    &&  enmGuestMode >= PGMMODE_PROTECTED)
                {
#ifdef VBOX_WITH_REM
                    /*
                     * Flush the recompiler code cache as it's not unlikely the guest will rewrite code
                     * it will later execute in real mode (OpenBSD 4.0 is one such example)
                     */
                    REMFlushTBs(pVM);
#endif

                    /*
                     * DPL of all hidden selector registers must match the current CPL (0).
                     */
                    pCtx->csHid.Attr.n.u2Dpl  = 0;
                    pCtx->csHid.Attr.n.u4Type = X86_SEL_TYPE_CODE | X86_SEL_TYPE_RW_ACC;

                    pCtx->dsHid.Attr.n.u2Dpl  = 0;
                    pCtx->esHid.Attr.n.u2Dpl  = 0;
                    pCtx->fsHid.Attr.n.u2Dpl  = 0;
                    pCtx->gsHid.Attr.n.u2Dpl  = 0;
                    pCtx->ssHid.Attr.n.u2Dpl  = 0;
                }
                pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode = enmGuestMode;
            }
            else if (   CPUMIsGuestInRealModeEx(pCtx)
                     && pCtx->csHid.u64Base == 0xffff0000)
            {
                /* VT-x will fail with a guest invalid state otherwise... (CPU state after a reset) */
                pCtx->csHid.u64Base = 0xf0000;
                pCtx->cs = 0xf000;
            }
        }

        VMX_WRITE_SELREG(ES, es);
        AssertRC(rc);

        VMX_WRITE_SELREG(CS, cs);
        AssertRC(rc);

        VMX_WRITE_SELREG(SS, ss);
        AssertRC(rc);

        VMX_WRITE_SELREG(DS, ds);
        AssertRC(rc);

        VMX_WRITE_SELREG(FS, fs);
        AssertRC(rc);

        VMX_WRITE_SELREG(GS, gs);
        AssertRC(rc);
    }

    /*
     * Guest CPU context: LDTR.
     */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_LDTR)
    {
        if (pCtx->ldtr == 0)
        {
            rc =  VMXWriteVMCS(VMX_VMCS16_GUEST_FIELD_LDTR,         0);
            rc |= VMXWriteVMCS(VMX_VMCS32_GUEST_LDTR_LIMIT,         0);
            rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_LDTR_BASE,        0);
            /* Note: vmlaunch will fail with 0 or just 0x02. No idea why. */
            rc |= VMXWriteVMCS(VMX_VMCS32_GUEST_LDTR_ACCESS_RIGHTS, 0x82 /* present, LDT */);
        }
        else
        {
            rc =  VMXWriteVMCS(VMX_VMCS16_GUEST_FIELD_LDTR,         pCtx->ldtr);
            rc |= VMXWriteVMCS(VMX_VMCS32_GUEST_LDTR_LIMIT,         pCtx->ldtrHid.u32Limit);
            rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_LDTR_BASE,        pCtx->ldtrHid.u64Base);
            rc |= VMXWriteVMCS(VMX_VMCS32_GUEST_LDTR_ACCESS_RIGHTS, pCtx->ldtrHid.Attr.u);
        }
        AssertRC(rc);
    }

    /*
     * Guest CPU context: TR.
     */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_TR)
    {
        /*
         * Real mode emulation using v86 mode with CR4.VME (interrupt redirection
         * using the int bitmap in the TSS).
         */
        if (    CPUMIsGuestInRealModeEx(pCtx)
            &&  pVM->hwaccm.s.vmx.pRealModeTSS)
        {
            RTGCPHYS GCPhys;

            /* We convert it here every time as PCI regions could be reconfigured. */
            rc = PDMVMMDevHeapR3ToGCPhys(pVM, pVM->hwaccm.s.vmx.pRealModeTSS, &GCPhys);
            AssertRC(rc);

            rc =  VMXWriteVMCS(VMX_VMCS16_GUEST_FIELD_TR,         0);
            rc |= VMXWriteVMCS(VMX_VMCS32_GUEST_TR_LIMIT,         HWACCM_VTX_TSS_SIZE);
            rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_TR_BASE,          GCPhys /* phys = virt in this mode */);

            X86DESCATTR attr;

            attr.u              = 0;
            attr.n.u1Present    = 1;
            attr.n.u4Type       = X86_SEL_TYPE_SYS_386_TSS_BUSY;
            val                 = attr.u;
        }
        else
        {
            rc =  VMXWriteVMCS(VMX_VMCS16_GUEST_FIELD_TR,         pCtx->tr);
            rc |= VMXWriteVMCS(VMX_VMCS32_GUEST_TR_LIMIT,         pCtx->trHid.u32Limit);
            rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_TR_BASE,        pCtx->trHid.u64Base);

            val = pCtx->trHid.Attr.u;

            /* The TSS selector must be busy (REM bugs? see defect #XXXX). */
            if (!(val & X86_SEL_TYPE_SYS_TSS_BUSY_MASK))
            {
                if (val & 0xf)
                    val |= X86_SEL_TYPE_SYS_TSS_BUSY_MASK;
                else
                    /* Default if no TR selector has been set (otherwise vmlaunch will fail!) */
                    val = (val & ~0xF) | X86_SEL_TYPE_SYS_386_TSS_BUSY;
            }
            AssertMsg((val & 0xf) == X86_SEL_TYPE_SYS_386_TSS_BUSY || (val & 0xf) == X86_SEL_TYPE_SYS_286_TSS_BUSY, ("%#x\n", val));
        }
        rc |= VMXWriteVMCS(VMX_VMCS32_GUEST_TR_ACCESS_RIGHTS, val);
        AssertRC(rc);
    }

    /*
     * Guest CPU context: GDTR.
     */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_GDTR)
    {
        rc  = VMXWriteVMCS(VMX_VMCS32_GUEST_GDTR_LIMIT,       pCtx->gdtr.cbGdt);
        rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_GDTR_BASE,      pCtx->gdtr.pGdt);
        AssertRC(rc);
    }

    /*
     * Guest CPU context: IDTR.
     */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_IDTR)
    {
        rc  = VMXWriteVMCS(VMX_VMCS32_GUEST_IDTR_LIMIT,       pCtx->idtr.cbIdt);
        rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_IDTR_BASE,      pCtx->idtr.pIdt);
        AssertRC(rc);
    }

    /*
     * Sysenter MSRs.
     */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_MSR)
    {
        rc  = VMXWriteVMCS(VMX_VMCS32_GUEST_SYSENTER_CS,    pCtx->SysEnter.cs);
        rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_SYSENTER_EIP, pCtx->SysEnter.eip);
        rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_SYSENTER_ESP, pCtx->SysEnter.esp);
        AssertRC(rc);
    }

    /*
     * Guest CPU context: Control registers.
     */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_CR0)
    {
        val = pCtx->cr0;
        rc  = VMXWriteVMCS(VMX_VMCS_CTRL_CR0_READ_SHADOW,   val);
        Log2(("Guest CR0-shadow %08x\n", val));
        if (CPUMIsGuestFPUStateActive(pVCpu) == false)
        {
            /* Always use #NM exceptions to load the FPU/XMM state on demand. */
            val |= X86_CR0_TS | X86_CR0_ET | X86_CR0_NE | X86_CR0_MP;
        }
        else
        {
            /** @todo check if we support the old style mess correctly. */
            if (!(val & X86_CR0_NE))
                Log(("Forcing X86_CR0_NE!!!\n"));

            val |= X86_CR0_NE;  /* always turn on the native mechanism to report FPU errors (old style uses interrupts) */
        }
        /* Note: protected mode & paging are always enabled; we use them for emulating real and protected mode without paging too. */
        if (!pVM->hwaccm.s.vmx.fUnrestrictedGuest)
            val |= X86_CR0_PE | X86_CR0_PG;

        if (pVM->hwaccm.s.fNestedPaging)
        {
            if (CPUMIsGuestInPagedProtectedModeEx(pCtx))
            {
                /* Disable CR3 read/write monitoring as we don't need it for EPT. */
                pVCpu->hwaccm.s.vmx.proc_ctls &=  ~(  VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT
                                                    | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT);
            }
            else
            {
                /* Reenable CR3 read/write monitoring as our identity mapped page table is active. */
                pVCpu->hwaccm.s.vmx.proc_ctls |=   VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT
                                                 | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT;
            }
            rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
            AssertRC(rc);
        }
        else
        {
            /* Note: We must also set this as we rely on protecting various pages for which supervisor writes must be caught. */
            val |= X86_CR0_WP;
        }

        /* Always enable caching. */
        val &= ~(X86_CR0_CD|X86_CR0_NW);

        rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_CR0,            val);
        Log2(("Guest CR0 %08x\n", val));

        /*
         * CR0 flags owned by the host; if the guests attempts to change them, then the VM will exit.
         */
        val =   X86_CR0_PE  /* Must monitor this bit (assumptions are made for real mode emulation) */
              | X86_CR0_WP  /* Must monitor this bit (it must always be enabled). */
              | X86_CR0_PG  /* Must monitor this bit (assumptions are made for real mode & protected mode without paging emulation) */
              | X86_CR0_CD  /* Bit not restored during VM-exit! */
              | X86_CR0_NW  /* Bit not restored during VM-exit! */
              | X86_CR0_NE;

        /*
         * When the guest's FPU state is active, then we no longer care about the FPU related bits.
         */
        if (CPUMIsGuestFPUStateActive(pVCpu) == false)
            val |= X86_CR0_TS | X86_CR0_ET | X86_CR0_MP;

        pVCpu->hwaccm.s.vmx.cr0_mask = val;

        rc |= VMXWriteVMCS(VMX_VMCS_CTRL_CR0_MASK, val);
        Log2(("Guest CR0-mask %08x\n", val));
        AssertRC(rc);
    }

    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_CR4)
    {
        rc  = VMXWriteVMCS(VMX_VMCS_CTRL_CR4_READ_SHADOW,   pCtx->cr4);
        Log2(("Guest CR4-shadow %08x\n", pCtx->cr4));
        /* Set the required bits in cr4 too (currently X86_CR4_VMXE). */
        val = pCtx->cr4 | (uint32_t)pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed0;

        if (!pVM->hwaccm.s.fNestedPaging)
        {
            switch(pVCpu->hwaccm.s.enmShadowMode)
            {
                case PGMMODE_REAL:          /* Real mode                 -> emulated using v86 mode */
                case PGMMODE_PROTECTED:     /* Protected mode, no paging -> emulated using identity mapping. */
                case PGMMODE_32_BIT:        /* 32-bit paging. */
                    val &= ~X86_CR4_PAE;
                    break;

                case PGMMODE_PAE:           /* PAE paging. */
                case PGMMODE_PAE_NX:        /* PAE paging with NX enabled. */
                    /** Must use PAE paging as we could use physical memory > 4 GB */
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
                default:                   /* shut up gcc */
                    AssertFailed();
                    return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
            }
        }
        else if (   !CPUMIsGuestInPagedProtectedModeEx(pCtx)
                 && !pVM->hwaccm.s.vmx.fUnrestrictedGuest)
        {
            /* We use 4 MB pages in our identity mapping page table for real and protected mode without paging. */
            val |= X86_CR4_PSE;
            /* Our identity mapping is a 32 bits page directory. */
            val &= ~X86_CR4_PAE;
        }

        /*
         * Turn off VME if we're in emulated real mode.
         */
        if (    CPUMIsGuestInRealModeEx(pCtx)
            &&  pVM->hwaccm.s.vmx.pRealModeTSS)
        {
            val &= ~X86_CR4_VME;
        }

        rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_CR4,            val);
        Log2(("Guest CR4 %08x\n", val));

        /*
         * CR4 flags owned by the host; if the guests attempts to change them, then the VM will exit.
         */
        val =   0
              | X86_CR4_VME
              | X86_CR4_PAE
              | X86_CR4_PGE
              | X86_CR4_PSE
              | X86_CR4_VMXE;
        pVCpu->hwaccm.s.vmx.cr4_mask = val;

        rc |= VMXWriteVMCS(VMX_VMCS_CTRL_CR4_MASK, val);
        Log2(("Guest CR4-mask %08x\n", val));
        AssertRC(rc);
    }

#if 0
    /* Enable single stepping if requested and CPU supports it. */
    if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG)
        if (DBGFIsStepping(pVCpu))
        {
            pVCpu->hwaccm.s.vmx.proc_ctls |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG;
            rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
            AssertRC(rc);
        }
#endif

    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_CR3)
    {
        if (pVM->hwaccm.s.fNestedPaging)
        {
            Assert(PGMGetHyperCR3(pVCpu));
            pVCpu->hwaccm.s.vmx.GCPhysEPTP = PGMGetHyperCR3(pVCpu);

            Assert(!(pVCpu->hwaccm.s.vmx.GCPhysEPTP & 0xfff));
            /** @todo Check the IA32_VMX_EPT_VPID_CAP MSR for other supported memory types. */
            pVCpu->hwaccm.s.vmx.GCPhysEPTP |=   VMX_EPT_MEMTYPE_WB
                                             | (VMX_EPT_PAGE_WALK_LENGTH_DEFAULT << VMX_EPT_PAGE_WALK_LENGTH_SHIFT);

            rc = VMXWriteVMCS64(VMX_VMCS_CTRL_EPTP_FULL, pVCpu->hwaccm.s.vmx.GCPhysEPTP);
            AssertRC(rc);

            if (    !CPUMIsGuestInPagedProtectedModeEx(pCtx)
                &&  !pVM->hwaccm.s.vmx.fUnrestrictedGuest)
            {
                RTGCPHYS GCPhys;

                /* We convert it here every time as PCI regions could be reconfigured. */
                rc = PDMVMMDevHeapR3ToGCPhys(pVM, pVM->hwaccm.s.vmx.pNonPagingModeEPTPageTable, &GCPhys);
                AssertMsgRC(rc, ("pNonPagingModeEPTPageTable = %RGv\n", pVM->hwaccm.s.vmx.pNonPagingModeEPTPageTable));

                /*
                 * We use our identity mapping page table here as we need to map guest virtual to
                 * guest physical addresses; EPT will take care of the translation to host physical addresses.
                 */
                val = GCPhys;
            }
            else
            {
                /* Save the real guest CR3 in VMX_VMCS_GUEST_CR3 */
                val = pCtx->cr3;
                rc = hmR0VmxLoadPaePdpes(pVCpu, pCtx);
                AssertRCReturn(rc, rc);
            }
        }
        else
        {
            val = PGMGetHyperCR3(pVCpu);
            Assert(val || VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL));
        }

        /* Save our shadow CR3 register. */
        rc = VMXWriteVMCS64(VMX_VMCS64_GUEST_CR3, val);
        AssertRC(rc);
    }

    /*
     * Guest CPU context: Debug registers.
     */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_DEBUG)
    {
        pCtx->dr[6] |= X86_DR6_INIT_VAL;                                          /* set all reserved bits to 1. */
        pCtx->dr[6] &= ~RT_BIT(12);                                               /* must be zero. */

        pCtx->dr[7] &= 0xffffffff;                                                /* upper 32 bits reserved */
        pCtx->dr[7] &= ~(RT_BIT(11) | RT_BIT(12) | RT_BIT(14) | RT_BIT(15));      /* must be zero */
        pCtx->dr[7] |= 0x400;                                                     /* must be one */

        /* Resync DR7 */
        rc = VMXWriteVMCS64(VMX_VMCS64_GUEST_DR7, pCtx->dr[7]);
        AssertRC(rc);

#ifdef DEBUG
        /* Sync the hypervisor debug state now if any breakpoint is armed. */
        if (    CPUMGetHyperDR7(pVCpu) & (X86_DR7_ENABLED_MASK|X86_DR7_GD)
            &&  !CPUMIsHyperDebugStateActive(pVCpu)
            &&  !DBGFIsStepping(pVCpu))
        {
            /* Save the host and load the hypervisor debug state. */
            rc = CPUMR0LoadHyperDebugState(pVM, pVCpu, pCtx, true /* include DR6 */);
            AssertRC(rc);

            /* DRx intercepts remain enabled. */

            /* Override dr7 with the hypervisor value. */
            rc = VMXWriteVMCS64(VMX_VMCS64_GUEST_DR7, CPUMGetHyperDR7(pVCpu));
            AssertRC(rc);
        }
        else
#endif
        /* Sync the debug state now if any breakpoint is armed. */
        if (    (pCtx->dr[7] & (X86_DR7_ENABLED_MASK|X86_DR7_GD))
            &&  !CPUMIsGuestDebugStateActive(pVCpu)
            &&  !DBGFIsStepping(pVCpu))
        {
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatDRxArmed);

            /* Disable DRx move intercepts. */
            pVCpu->hwaccm.s.vmx.proc_ctls &= ~VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT;
            rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
            AssertRC(rc);

            /* Save the host and load the guest debug state. */
            rc = CPUMR0LoadGuestDebugState(pVM, pVCpu, pCtx, true /* include DR6 */);
            AssertRC(rc);
        }

        /* IA32_DEBUGCTL MSR. */
        rc = VMXWriteVMCS64(VMX_VMCS_GUEST_DEBUGCTL_FULL,    0);
        AssertRC(rc);

        /** @todo do we really ever need this? */
        rc |= VMXWriteVMCS(VMX_VMCS_GUEST_DEBUG_EXCEPTIONS,  0);
        AssertRC(rc);
    }

    /*
     * 64-bit guest mode.
     */
    if (CPUMIsGuestInLongModeEx(pCtx))
    {
#if !defined(VBOX_ENABLE_64_BITS_GUESTS)
        return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
#elif HC_ARCH_BITS == 32 && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        pVCpu->hwaccm.s.vmx.pfnStartVM  = VMXR0SwitcherStartVM64;
#else
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        if (!pVM->hwaccm.s.fAllow64BitGuests)
            return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
# endif
        pVCpu->hwaccm.s.vmx.pfnStartVM  = VMXR0StartVM64;
#endif
        if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_MSR)
        {
            /* Update these as wrmsr might have changed them. */
            rc = VMXWriteVMCS64(VMX_VMCS64_GUEST_FS_BASE, pCtx->fsHid.u64Base);
            AssertRC(rc);
            rc = VMXWriteVMCS64(VMX_VMCS64_GUEST_GS_BASE, pCtx->gsHid.u64Base);
            AssertRC(rc);
        }
    }
    else
    {
        pVCpu->hwaccm.s.vmx.pfnStartVM  = VMXR0StartVM32;
    }

    hmR0VmxUpdateExceptionBitmap(pVM, pVCpu, pCtx);

#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
    /* Store all guest MSRs in the VM-Entry load area, so they will be loaded during the world switch. */
    PVMXMSR pMsr = (PVMXMSR)pVCpu->hwaccm.s.vmx.pGuestMSR;
    unsigned idxMsr = 0;

    uint32_t ulEdx;
    uint32_t ulTemp;
    CPUMGetGuestCpuId(pVCpu, 0x80000001, &ulTemp, &ulTemp, &ulTemp, &ulEdx);
    /* EFER MSR present? */
    if (ulEdx & (X86_CPUID_AMD_FEATURE_EDX_NX|X86_CPUID_AMD_FEATURE_EDX_LONG_MODE))
    {
        pMsr->u32IndexMSR = MSR_K6_EFER;
        pMsr->u32Reserved = 0;
        pMsr->u64Value    = pCtx->msrEFER;
        /* VT-x will complain if only MSR_K6_EFER_LME is set. */
        if (!CPUMIsGuestInLongModeEx(pCtx))
            pMsr->u64Value &= ~(MSR_K6_EFER_LMA|MSR_K6_EFER_LME);
        pMsr++; idxMsr++;

        if (ulEdx & X86_CPUID_AMD_FEATURE_EDX_LONG_MODE)
        {
            pMsr->u32IndexMSR = MSR_K8_LSTAR;
            pMsr->u32Reserved = 0;
            pMsr->u64Value    = pCtx->msrLSTAR;           /* 64 bits mode syscall rip */
            pMsr++; idxMsr++;
            pMsr->u32IndexMSR = MSR_K6_STAR;
            pMsr->u32Reserved = 0;
            pMsr->u64Value    = pCtx->msrSTAR;            /* legacy syscall eip, cs & ss */
            pMsr++; idxMsr++;
            pMsr->u32IndexMSR = MSR_K8_SF_MASK;
            pMsr->u32Reserved = 0;
            pMsr->u64Value    = pCtx->msrSFMASK;          /* syscall flag mask */
            pMsr++; idxMsr++;
            pMsr->u32IndexMSR = MSR_K8_KERNEL_GS_BASE;
            pMsr->u32Reserved = 0;
            pMsr->u64Value    = pCtx->msrKERNELGSBASE;    /* swapgs exchange value */
            pMsr++; idxMsr++;
        }
    }
    pVCpu->hwaccm.s.vmx.cCachedMSRs = idxMsr;

    rc = VMXWriteVMCS(VMX_VMCS_CTRL_ENTRY_MSR_LOAD_COUNT, idxMsr);
    AssertRC(rc);

    rc = VMXWriteVMCS(VMX_VMCS_CTRL_EXIT_MSR_STORE_COUNT, idxMsr);
    AssertRC(rc);
#endif /* VBOX_WITH_AUTO_MSR_LOAD_RESTORE */

    bool fOffsettedTsc;
    if (pVM->hwaccm.s.vmx.fUsePreemptTimer)
    {
        uint64_t cTicksToDeadline = TMCpuTickGetDeadlineAndTscOffset(pVCpu, &fOffsettedTsc, &pVCpu->hwaccm.s.vmx.u64TSCOffset);

        /* Make sure the returned values have sane upper and lower boundaries. */
        uint64_t u64CpuHz = SUPGetCpuHzFromGIP(g_pSUPGlobalInfoPage);

        cTicksToDeadline = RT_MIN(cTicksToDeadline, u64CpuHz / 64);   /* 1/64 of a second */
        cTicksToDeadline = RT_MAX(cTicksToDeadline, u64CpuHz / 2048); /* 1/2048th of a second */

        cTicksToDeadline >>= pVM->hwaccm.s.vmx.cPreemptTimerShift;
        uint32_t cPreemptionTickCount = (uint32_t)RT_MIN(cTicksToDeadline, UINT32_MAX - 16);
        rc = VMXWriteVMCS(VMX_VMCS32_GUEST_PREEMPTION_TIMER_VALUE, cPreemptionTickCount);
        AssertRC(rc);
    }
    else
        fOffsettedTsc = TMCpuTickCanUseRealTSC(pVCpu, &pVCpu->hwaccm.s.vmx.u64TSCOffset);

    if (fOffsettedTsc)
    {
        uint64_t u64CurTSC = ASMReadTSC();
        if (u64CurTSC + pVCpu->hwaccm.s.vmx.u64TSCOffset >= TMCpuTickGetLastSeen(pVCpu))
        {
            /* Note: VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT takes precedence over TSC_OFFSET */
            rc = VMXWriteVMCS64(VMX_VMCS_CTRL_TSC_OFFSET_FULL, pVCpu->hwaccm.s.vmx.u64TSCOffset);
            AssertRC(rc);

            pVCpu->hwaccm.s.vmx.proc_ctls &= ~VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT;
            rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
            AssertRC(rc);
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTSCOffset);
        }
        else
        {
            /* Fall back to rdtsc emulation as we would otherwise pass decreasing tsc values to the guest. */
            LogFlow(("TSC %RX64 offset %RX64 time=%RX64 last=%RX64 (diff=%RX64, virt_tsc=%RX64)\n", u64CurTSC,
                     pVCpu->hwaccm.s.vmx.u64TSCOffset, u64CurTSC + pVCpu->hwaccm.s.vmx.u64TSCOffset,
                     TMCpuTickGetLastSeen(pVCpu), TMCpuTickGetLastSeen(pVCpu) - u64CurTSC - pVCpu->hwaccm.s.vmx.u64TSCOffset,
                     TMCpuTickGet(pVCpu)));
            pVCpu->hwaccm.s.vmx.proc_ctls |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT;
            rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
            AssertRC(rc);
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTSCInterceptOverFlow);
        }
    }
    else
    {
        pVCpu->hwaccm.s.vmx.proc_ctls |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT;
        rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
        AssertRC(rc);
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTSCIntercept);
    }

    /* Done with the major changes */
    pVCpu->hwaccm.s.fContextUseFlags &= ~HWACCM_CHANGED_ALL_GUEST;

    /* Minimal guest state update (ESP, EIP, EFLAGS mostly) */
    VMXR0LoadMinimalGuestState(pVM, pVCpu, pCtx);
    return rc;
}


/**
 * Syncs back the guest state from VMCS.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer the guest CPU context.
 */
DECLINLINE(int) VMXR0SaveGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    RTGCUINTREG val, valShadow;
    RTGCUINTPTR uInterruptState;
    int         rc;

    /* First sync back EIP, ESP, and EFLAGS. */
    rc = VMXReadCachedVMCS(VMX_VMCS64_GUEST_RIP,              &val);
    AssertRC(rc);
    pCtx->rip               = val;
    rc = VMXReadCachedVMCS(VMX_VMCS64_GUEST_RSP,              &val);
    AssertRC(rc);
    pCtx->rsp               = val;
    rc = VMXReadCachedVMCS(VMX_VMCS_GUEST_RFLAGS,           &val);
    AssertRC(rc);
    pCtx->eflags.u32        = val;

    /* Take care of instruction fusing (sti, mov ss) */
    rc |= VMXReadCachedVMCS(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE, &val);
    uInterruptState = val;
    if (uInterruptState != 0)
    {
        Assert(uInterruptState <= 2);    /* only sti & mov ss */
        Log(("uInterruptState %x eip=%RGv\n", (uint32_t)uInterruptState, pCtx->rip));
        EMSetInhibitInterruptsPC(pVCpu, pCtx->rip);
    }
    else
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);

    /* Control registers. */
    VMXReadCachedVMCS(VMX_VMCS_CTRL_CR0_READ_SHADOW,     &valShadow);
    VMXReadCachedVMCS(VMX_VMCS64_GUEST_CR0,              &val);
    val = (valShadow & pVCpu->hwaccm.s.vmx.cr0_mask) | (val & ~pVCpu->hwaccm.s.vmx.cr0_mask);
    CPUMSetGuestCR0(pVCpu, val);

    VMXReadCachedVMCS(VMX_VMCS_CTRL_CR4_READ_SHADOW,     &valShadow);
    VMXReadCachedVMCS(VMX_VMCS64_GUEST_CR4,              &val);
    val = (valShadow & pVCpu->hwaccm.s.vmx.cr4_mask) | (val & ~pVCpu->hwaccm.s.vmx.cr4_mask);
    CPUMSetGuestCR4(pVCpu, val);

    /*
     * No reason to sync back the CRx registers. They can't be changed by the guest unless in
     * the nested paging case where CR3 & CR4 can be changed by the guest.
     */
    if (   pVM->hwaccm.s.fNestedPaging
        && CPUMIsGuestInPagedProtectedModeEx(pCtx)) /** @todo check if we will always catch mode switches and such... */
    {
        PVMCSCACHE pCache = &pVCpu->hwaccm.s.vmx.VMCSCache;

        /* Can be updated behind our back in the nested paging case. */
        CPUMSetGuestCR2(pVCpu, pCache->cr2);

        VMXReadCachedVMCS(VMX_VMCS64_GUEST_CR3, &val);

        if (val != pCtx->cr3)
        {
            CPUMSetGuestCR3(pVCpu, val);
            PGMUpdateCR3(pVCpu, val);
        }
        rc = hmR0VmxSavePaePdpes(pVCpu, pCtx);
        AssertRCReturn(rc, rc);
    }

    /* Sync back DR7. */
    VMXReadCachedVMCS(VMX_VMCS64_GUEST_DR7, &val);
    pCtx->dr[7] = val;

    /* Guest CPU context: ES, CS, SS, DS, FS, GS. */
    VMX_READ_SELREG(ES, es);
    VMX_READ_SELREG(SS, ss);
    VMX_READ_SELREG(CS, cs);
    VMX_READ_SELREG(DS, ds);
    VMX_READ_SELREG(FS, fs);
    VMX_READ_SELREG(GS, gs);

    /* System MSRs */
    VMXReadCachedVMCS(VMX_VMCS32_GUEST_SYSENTER_CS,    &val);
    pCtx->SysEnter.cs       = val;
    VMXReadCachedVMCS(VMX_VMCS64_GUEST_SYSENTER_EIP,     &val);
    pCtx->SysEnter.eip      = val;
    VMXReadCachedVMCS(VMX_VMCS64_GUEST_SYSENTER_ESP,     &val);
    pCtx->SysEnter.esp      = val;

    /* Misc. registers; must sync everything otherwise we can get out of sync when jumping to ring 3. */
    VMX_READ_SELREG(LDTR, ldtr);

    VMXReadCachedVMCS(VMX_VMCS32_GUEST_GDTR_LIMIT,     &val);
    pCtx->gdtr.cbGdt        = val;
    VMXReadCachedVMCS(VMX_VMCS64_GUEST_GDTR_BASE,        &val);
    pCtx->gdtr.pGdt         = val;

    VMXReadCachedVMCS(VMX_VMCS32_GUEST_IDTR_LIMIT,     &val);
    pCtx->idtr.cbIdt        = val;
    VMXReadCachedVMCS(VMX_VMCS64_GUEST_IDTR_BASE,        &val);
    pCtx->idtr.pIdt         = val;

    /* Real mode emulation using v86 mode. */
    if (    CPUMIsGuestInRealModeEx(pCtx)
        &&  pVM->hwaccm.s.vmx.pRealModeTSS)
    {
        /* Hide our emulation flags */
        pCtx->eflags.Bits.u1VM   = 0;

        /* Restore original IOPL setting as we always use 0. */
        pCtx->eflags.Bits.u2IOPL = pVCpu->hwaccm.s.vmx.RealMode.eflags.Bits.u2IOPL;

        /* Force a TR resync every time in case we switch modes. */
        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_TR;
    }
    else
    {
        /* In real mode we have a fake TSS, so only sync it back when it's supposed to be valid. */
        VMX_READ_SELREG(TR, tr);
    }

#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
    /* Save the possibly changed MSRs that we automatically restore and save during a world switch. */
    for (unsigned i = 0; i < pVCpu->hwaccm.s.vmx.cCachedMSRs; i++)
    {
        PVMXMSR pMsr = (PVMXMSR)pVCpu->hwaccm.s.vmx.pGuestMSR;
        pMsr += i;

        switch (pMsr->u32IndexMSR)
        {
        case MSR_K8_LSTAR:
            pCtx->msrLSTAR = pMsr->u64Value;
            break;
        case MSR_K6_STAR:
            pCtx->msrSTAR = pMsr->u64Value;
            break;
        case MSR_K8_SF_MASK:
            pCtx->msrSFMASK = pMsr->u64Value;
            break;
        case MSR_K8_KERNEL_GS_BASE:
            pCtx->msrKERNELGSBASE = pMsr->u64Value;
            break;
        case MSR_K6_EFER:
            /* EFER can't be changed without causing a VM-exit. */
            /* Assert(pCtx->msrEFER == pMsr->u64Value); */
            break;
        default:
            AssertFailed();
            return VERR_HM_UNEXPECTED_LD_ST_MSR;
        }
    }
#endif /* VBOX_WITH_AUTO_MSR_LOAD_RESTORE */
    return VINF_SUCCESS;
}


/**
 * Dummy placeholder for TLB flush handling before VM-entry. Used in the case
 * where neither EPT nor VPID is supported by the CPU.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
static DECLCALLBACK(void) hmR0VmxSetupTLBDummy(PVM pVM, PVMCPU pVCpu)
{
    NOREF(pVM);
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH);
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);
    pVCpu->hwaccm.s.TlbShootdown.cPages = 0;
    return;
}


/**
 * Setup the tagged TLB for EPT+VPID.
 *
 * @param    pVM        Pointer to the VM.
 * @param    pVCpu      Pointer to the VMCPU.
 */
static DECLCALLBACK(void) hmR0VmxSetupTLBBoth(PVM pVM, PVMCPU pVCpu)
{
    PHMGLOBLCPUINFO pCpu;

    Assert(pVM->hwaccm.s.fNestedPaging && pVM->hwaccm.s.vmx.fVPID);

    pCpu = HWACCMR0GetCurrentCpu();

    /*
     * Force a TLB flush for the first world switch if the current CPU differs from the one we ran on last
     * This can happen both for start & resume due to long jumps back to ring-3.
     * If the TLB flush count changed, another VM (VCPU rather) has hit the ASID limit while flushing the TLB,
     * so we cannot reuse the current ASID anymore.
     */
    bool fNewASID = false;
    if (    pVCpu->hwaccm.s.idLastCpu != pCpu->idCpu
        ||  pVCpu->hwaccm.s.cTLBFlushes != pCpu->cTLBFlushes)
    {
        pVCpu->hwaccm.s.fForceTLBFlush = true;
        fNewASID = true;
    }

    /*
     * Check for explicit TLB shootdowns.
     */
    if (VMCPU_FF_TESTANDCLEAR(pVCpu, VMCPU_FF_TLB_FLUSH))
        pVCpu->hwaccm.s.fForceTLBFlush = true;

    pVCpu->hwaccm.s.idLastCpu = pCpu->idCpu;

    if (pVCpu->hwaccm.s.fForceTLBFlush)
    {
        if (fNewASID)
        {
            ++pCpu->uCurrentASID;
            if (pCpu->uCurrentASID >= pVM->hwaccm.s.uMaxASID)
            {
                pCpu->uCurrentASID = 1;       /* start at 1; host uses 0 */
                pCpu->cTLBFlushes++;
                pCpu->fFlushASIDBeforeUse = true;
            }

            pVCpu->hwaccm.s.uCurrentASID = pCpu->uCurrentASID;
            if (pCpu->fFlushASIDBeforeUse)
            {
                hmR0VmxFlushVPID(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushVPID, 0 /* GCPtr */);
#ifdef VBOX_WITH_STATISTICS
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushASID);
#endif
            }
        }
        else
        {
            if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_SINGLE_CONTEXT)
                hmR0VmxFlushVPID(pVM, pVCpu, VMX_FLUSH_VPID_SINGLE_CONTEXT, 0 /* GCPtr */);
            else
                hmR0VmxFlushEPT(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushEPT);

#ifdef VBOX_WITH_STATISTICS
            /*
             * This is not terribly accurate (i.e. we don't have any StatFlushEPT counter). We currently count these
             * as ASID flushes too, better than including them under StatFlushTLBWorldSwitch.
             */
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushASID);
#endif
        }

        pVCpu->hwaccm.s.cTLBFlushes    = pCpu->cTLBFlushes;
        pVCpu->hwaccm.s.fForceTLBFlush = false;
    }
    else
    {
        Assert(pVCpu->hwaccm.s.uCurrentASID && pCpu->uCurrentASID);

        /** @todo We never set VMCPU_FF_TLB_SHOOTDOWN anywhere so this path should
         *        not be executed. See hwaccmQueueInvlPage() where it is commented
         *        out. Support individual entry flushing someday. */
        if (VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_TLB_SHOOTDOWN))
        {
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTlbShootdown);

            /*
             * Flush individual guest entries using VPID from the TLB or as little as possible with EPT
             * as supported by the CPU.
             */
            if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_INDIV_ADDR)
            {
                for (unsigned i = 0; i < pVCpu->hwaccm.s.TlbShootdown.cPages; i++)
                    hmR0VmxFlushVPID(pVM, pVCpu, VMX_FLUSH_VPID_INDIV_ADDR, pVCpu->hwaccm.s.TlbShootdown.aPages[i]);
            }
            else
                hmR0VmxFlushEPT(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushEPT);
        }
        else
        {
#ifdef VBOX_WITH_STATISTICS
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatNoFlushTLBWorldSwitch);
#endif
        }
    }
    pVCpu->hwaccm.s.TlbShootdown.cPages = 0;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);

    AssertMsg(pVCpu->hwaccm.s.cTLBFlushes == pCpu->cTLBFlushes,
              ("Flush count mismatch for cpu %d (%x vs %x)\n", pCpu->idCpu, pVCpu->hwaccm.s.cTLBFlushes, pCpu->cTLBFlushes));
    AssertMsg(pCpu->uCurrentASID >= 1 && pCpu->uCurrentASID < pVM->hwaccm.s.uMaxASID,
              ("cpu%d uCurrentASID = %x\n", pCpu->idCpu, pCpu->uCurrentASID));
    AssertMsg(pVCpu->hwaccm.s.uCurrentASID >= 1 && pVCpu->hwaccm.s.uCurrentASID < pVM->hwaccm.s.uMaxASID,
              ("cpu%d VM uCurrentASID = %x\n", pCpu->idCpu, pVCpu->hwaccm.s.uCurrentASID));

    /* Update VMCS with the VPID. */
    int rc  = VMXWriteVMCS(VMX_VMCS16_GUEST_FIELD_VPID, pVCpu->hwaccm.s.uCurrentASID);
    AssertRC(rc);
}


/**
 * Setup the tagged TLB for EPT only.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
static DECLCALLBACK(void) hmR0VmxSetupTLBEPT(PVM pVM, PVMCPU pVCpu)
{
    PHMGLOBLCPUINFO pCpu;

    Assert(pVM->hwaccm.s.fNestedPaging);
    Assert(!pVM->hwaccm.s.vmx.fVPID);

    pCpu = HWACCMR0GetCurrentCpu();

    /*
     * Force a TLB flush for the first world switch if the current CPU differs from the one we ran on last
     * This can happen both for start & resume due to long jumps back to ring-3.
     * If the TLB flush count shouldn't really change in this EPT-only case.
     */
    if (    pVCpu->hwaccm.s.idLastCpu != pCpu->idCpu
        ||  pVCpu->hwaccm.s.cTLBFlushes != pCpu->cTLBFlushes)
    {
        pVCpu->hwaccm.s.fForceTLBFlush = true;
    }

    /*
     * Check for explicit TLB shootdown flushes.
     */
    if (VMCPU_FF_TESTANDCLEAR(pVCpu, VMCPU_FF_TLB_FLUSH))
        pVCpu->hwaccm.s.fForceTLBFlush = true;

    pVCpu->hwaccm.s.idLastCpu = pCpu->idCpu;

    if (pVCpu->hwaccm.s.fForceTLBFlush)
        hmR0VmxFlushEPT(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushEPT);
    else
    {
        /** @todo We never set VMCPU_FF_TLB_SHOOTDOWN anywhere so this path should
         *        not be executed. See hwaccmQueueInvlPage() where it is commented
         *        out. Support individual entry flushing someday. */
        if (VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_TLB_SHOOTDOWN))
        {
            /*
             * We cannot flush individual entries without VPID support. Flush using EPT.
             */
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTlbShootdown);
            hmR0VmxFlushEPT(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushEPT);
        }
    }
    pVCpu->hwaccm.s.TlbShootdown.cPages= 0;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);

#ifdef VBOX_WITH_STATISTICS
    if (pVCpu->hwaccm.s.fForceTLBFlush)
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushTLBWorldSwitch);
    else
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatNoFlushTLBWorldSwitch);
#endif
}


/**
 * Setup the tagged TLB for VPID.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
static DECLCALLBACK(void) hmR0VmxSetupTLBVPID(PVM pVM, PVMCPU pVCpu)
{
    PHMGLOBLCPUINFO pCpu;

    Assert(pVM->hwaccm.s.vmx.fVPID);
    Assert(!pVM->hwaccm.s.fNestedPaging);

    pCpu = HWACCMR0GetCurrentCpu();

    /*
     * Force a TLB flush for the first world switch if the current CPU differs from the one we ran on last
     * This can happen both for start & resume due to long jumps back to ring-3.
     * If the TLB flush count changed, another VM (VCPU rather) has hit the ASID limit while flushing the TLB,
     * so we cannot reuse the current ASID anymore.
     */
    if (    pVCpu->hwaccm.s.idLastCpu != pCpu->idCpu
        ||  pVCpu->hwaccm.s.cTLBFlushes != pCpu->cTLBFlushes)
    {
        /* Force a TLB flush on VM entry. */
        pVCpu->hwaccm.s.fForceTLBFlush = true;
    }

    /*
     * Check for explicit TLB shootdown flushes.
     */
    if (VMCPU_FF_TESTANDCLEAR(pVCpu, VMCPU_FF_TLB_FLUSH))
        pVCpu->hwaccm.s.fForceTLBFlush = true;

    pVCpu->hwaccm.s.idLastCpu = pCpu->idCpu;

    if (pVCpu->hwaccm.s.fForceTLBFlush)
    {
        ++pCpu->uCurrentASID;
        if (pCpu->uCurrentASID >= pVM->hwaccm.s.uMaxASID)
        {
            pCpu->uCurrentASID               = 1;       /* start at 1; host uses 0 */
            pCpu->cTLBFlushes++;
            pCpu->fFlushASIDBeforeUse        = true;
        }
        else
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushASID);

        pVCpu->hwaccm.s.fForceTLBFlush = false;
        pVCpu->hwaccm.s.cTLBFlushes    = pCpu->cTLBFlushes;
        pVCpu->hwaccm.s.uCurrentASID   = pCpu->uCurrentASID;
        if (pCpu->fFlushASIDBeforeUse)
            hmR0VmxFlushVPID(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushVPID, 0 /* GCPtr */);
    }
    else
    {
        Assert(pVCpu->hwaccm.s.uCurrentASID && pCpu->uCurrentASID);

        /** @todo We never set VMCPU_FF_TLB_SHOOTDOWN anywhere so this path should
         *        not be executed. See hwaccmQueueInvlPage() where it is commented
         *        out. Support individual entry flushing someday. */
        if (VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_TLB_SHOOTDOWN))
        {
            /*
             * Flush individual guest entries using VPID from the TLB or as little as possible with EPT
             * as supported by the CPU.
             */
            if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_INDIV_ADDR)
            {
                for (unsigned i = 0; i < pVCpu->hwaccm.s.TlbShootdown.cPages; i++)
                    hmR0VmxFlushVPID(pVM, pVCpu, VMX_FLUSH_VPID_INDIV_ADDR, pVCpu->hwaccm.s.TlbShootdown.aPages[i]);
            }
            else
                hmR0VmxFlushVPID(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushVPID, 0 /* GCPtr */);
        }
    }
    pVCpu->hwaccm.s.TlbShootdown.cPages = 0;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);

    AssertMsg(pVCpu->hwaccm.s.cTLBFlushes == pCpu->cTLBFlushes, ("Flush count mismatch for cpu %d (%x vs %x)\n", pCpu->idCpu, pVCpu->hwaccm.s.cTLBFlushes, pCpu->cTLBFlushes));
    AssertMsg(pCpu->uCurrentASID >= 1 && pCpu->uCurrentASID < pVM->hwaccm.s.uMaxASID, ("cpu%d uCurrentASID = %x\n", pCpu->idCpu, pCpu->uCurrentASID));
    AssertMsg(pVCpu->hwaccm.s.uCurrentASID >= 1 && pVCpu->hwaccm.s.uCurrentASID < pVM->hwaccm.s.uMaxASID, ("cpu%d VM uCurrentASID = %x\n", pCpu->idCpu, pVCpu->hwaccm.s.uCurrentASID));

    int rc  = VMXWriteVMCS(VMX_VMCS16_GUEST_FIELD_VPID, pVCpu->hwaccm.s.uCurrentASID);
    AssertRC(rc);

# ifdef VBOX_WITH_STATISTICS
    if (pVCpu->hwaccm.s.fForceTLBFlush)
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushTLBWorldSwitch);
    else
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatNoFlushTLBWorldSwitch);
# endif
}


/**
 * Runs guest code in a VT-x VM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
VMMR0DECL(int) VMXR0RunGuestCode(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatEntry, x);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hwaccm.s.StatExit1);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hwaccm.s.StatExit2);

    VBOXSTRICTRC rc = VINF_SUCCESS;
    int         rc2;
    RTGCUINTREG val;
    RTGCUINTREG exitReason = (RTGCUINTREG)VMX_EXIT_INVALID;
    RTGCUINTREG instrError, cbInstr;
    RTGCUINTPTR exitQualification = 0;
    RTGCUINTPTR intInfo = 0; /* shut up buggy gcc 4 */
    RTGCUINTPTR errCode, instrInfo;
    bool        fSetupTPRCaching = false;
    uint64_t    u64OldLSTAR = 0;
    uint8_t     u8LastTPR = 0;
    RTCCUINTREG uOldEFlags = ~(RTCCUINTREG)0;
    unsigned    cResume = 0;
#ifdef VBOX_STRICT
    RTCPUID     idCpuCheck;
    bool        fWasInLongMode = false;
#endif
#ifdef VBOX_HIGH_RES_TIMERS_HACK_IN_RING0
    uint64_t    u64LastTime = RTTimeMilliTS();
#endif

    Assert(!(pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC) || (pVCpu->hwaccm.s.vmx.pbVAPIC && pVM->hwaccm.s.vmx.pAPIC));

    /*
     * Check if we need to use TPR shadowing.
     */
    if (    CPUMIsGuestInLongModeEx(pCtx)
        || (   ((pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC) || pVM->hwaccm.s.fTRPPatchingAllowed)
            &&  pVM->hwaccm.s.fHasIoApic)
       )
    {
        fSetupTPRCaching = true;
    }

    Log2(("\nE"));

#ifdef VBOX_STRICT
    {
        RTCCUINTREG val2;

        rc2 = VMXReadVMCS(VMX_VMCS_CTRL_PIN_EXEC_CONTROLS, &val2);
        AssertRC(rc2);
        Log2(("VMX_VMCS_CTRL_PIN_EXEC_CONTROLS = %08x\n",  val2));

        /* allowed zero */
        if ((val2 & pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.disallowed0) != pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.disallowed0)
            Log(("Invalid VMX_VMCS_CTRL_PIN_EXEC_CONTROLS: zero\n"));

        /* allowed one */
        if ((val2 & ~pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.allowed1) != 0)
            Log(("Invalid VMX_VMCS_CTRL_PIN_EXEC_CONTROLS: one\n"));

        rc2 = VMXReadVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, &val2);
        AssertRC(rc2);
        Log2(("VMX_VMCS_CTRL_PROC_EXEC_CONTROLS = %08x\n",  val2));

        /*
         * Must be set according to the MSR, but can be cleared if nested paging is used.
         */
        if (pVM->hwaccm.s.fNestedPaging)
        {
            val2 |=   VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT
                    | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT
                    | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT;
        }

        /* allowed zero */
        if ((val2 & pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.disallowed0) != pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.disallowed0)
            Log(("Invalid VMX_VMCS_CTRL_PROC_EXEC_CONTROLS: zero\n"));

        /* allowed one */
        if ((val2 & ~pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1) != 0)
            Log(("Invalid VMX_VMCS_CTRL_PROC_EXEC_CONTROLS: one\n"));

        rc2 = VMXReadVMCS(VMX_VMCS_CTRL_ENTRY_CONTROLS, &val2);
        AssertRC(rc2);
        Log2(("VMX_VMCS_CTRL_ENTRY_CONTROLS = %08x\n",  val2));

        /* allowed zero */
        if ((val2 & pVM->hwaccm.s.vmx.msr.vmx_entry.n.disallowed0) != pVM->hwaccm.s.vmx.msr.vmx_entry.n.disallowed0)
            Log(("Invalid VMX_VMCS_CTRL_ENTRY_CONTROLS: zero\n"));

        /* allowed one */
        if ((val2 & ~pVM->hwaccm.s.vmx.msr.vmx_entry.n.allowed1) != 0)
            Log(("Invalid VMX_VMCS_CTRL_ENTRY_CONTROLS: one\n"));

        rc2 = VMXReadVMCS(VMX_VMCS_CTRL_EXIT_CONTROLS, &val2);
        AssertRC(rc2);
        Log2(("VMX_VMCS_CTRL_EXIT_CONTROLS = %08x\n",  val2));

        /* allowed zero */
        if ((val2 & pVM->hwaccm.s.vmx.msr.vmx_exit.n.disallowed0) != pVM->hwaccm.s.vmx.msr.vmx_exit.n.disallowed0)
            Log(("Invalid VMX_VMCS_CTRL_EXIT_CONTROLS: zero\n"));

        /* allowed one */
        if ((val2 & ~pVM->hwaccm.s.vmx.msr.vmx_exit.n.allowed1) != 0)
            Log(("Invalid VMX_VMCS_CTRL_EXIT_CONTROLS: one\n"));
    }
    fWasInLongMode = CPUMIsGuestInLongModeEx(pCtx);
#endif /* VBOX_STRICT */

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    pVCpu->hwaccm.s.vmx.VMCSCache.u64TimeEntry = RTTimeNanoTS();
#endif

    /*
     * We can jump to this point to resume execution after determining that a VM-exit is innocent.
     */
ResumeExecution:
    if (!STAM_REL_PROFILE_ADV_IS_RUNNING(&pVCpu->hwaccm.s.StatEntry))
        STAM_REL_PROFILE_ADV_STOP_START(&pVCpu->hwaccm.s.StatExit2, &pVCpu->hwaccm.s.StatEntry, x);
    AssertMsg(pVCpu->hwaccm.s.idEnteredCpu == RTMpCpuId(),
              ("Expected %d, I'm %d; cResume=%d exitReason=%RGv exitQualification=%RGv\n",
               (int)pVCpu->hwaccm.s.idEnteredCpu, (int)RTMpCpuId(), cResume, exitReason, exitQualification));
    Assert(!HWACCMR0SuspendPending());
    /* Not allowed to switch modes without reloading the host state (32->64 switcher)!! */
    Assert(fWasInLongMode == CPUMIsGuestInLongModeEx(pCtx));

    /*
     * Safety precaution; looping for too long here can have a very bad effect on the host.
     */
    if (RT_UNLIKELY(++cResume > pVM->hwaccm.s.cMaxResumeLoops))
    {
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitMaxResume);
        rc = VINF_EM_RAW_INTERRUPT;
        goto end;
    }

    /*
     * Check for IRQ inhibition due to instruction fusing (sti, mov ss).
     */
    if (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
    {
        Log(("VM_FF_INHIBIT_INTERRUPTS at %RGv successor %RGv\n", (RTGCPTR)pCtx->rip, EMGetInhibitInterruptsPC(pVCpu)));
        if (pCtx->rip != EMGetInhibitInterruptsPC(pVCpu))
        {
            /*
             * Note: we intentionally don't clear VM_FF_INHIBIT_INTERRUPTS here.
             * Before we are able to execute this instruction in raw mode (iret to guest code) an external interrupt might
             * force a world switch again. Possibly allowing a guest interrupt to be dispatched in the process. This could
             * break the guest. Sounds very unlikely, but such timing sensitive problems are not as rare as you might think.
             */
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
            /* Irq inhibition is no longer active; clear the corresponding VMX state. */
            rc2 = VMXWriteVMCS(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE,   0);
            AssertRC(rc2);
        }
    }
    else
    {
        /* Irq inhibition is no longer active; clear the corresponding VMX state. */
        rc2 = VMXWriteVMCS(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE,   0);
        AssertRC(rc2);
    }

#ifdef VBOX_HIGH_RES_TIMERS_HACK_IN_RING0
    if (RT_UNLIKELY((cResume & 0xf) == 0))
    {
        uint64_t u64CurTime = RTTimeMilliTS();

        if (RT_UNLIKELY(u64CurTime > u64LastTime))
        {
            u64LastTime = u64CurTime;
            TMTimerPollVoid(pVM, pVCpu);
        }
    }
#endif

    /*
     * Check for pending actions that force us to go back to ring-3.
     */
    if (    VM_FF_ISPENDING(pVM, VM_FF_HWACCM_TO_R3_MASK | VM_FF_REQUEST | VM_FF_PGM_POOL_FLUSH_PENDING | VM_FF_PDM_DMA)
        ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_HWACCM_TO_R3_MASK | VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL | VMCPU_FF_REQUEST))
    {
        /* Check if a sync operation is pending. */
        if (VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL))
        {
            rc = PGMSyncCR3(pVCpu, pCtx->cr0, pCtx->cr3, pCtx->cr4, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3));
            if (rc != VINF_SUCCESS)
            {
                AssertRC(VBOXSTRICTRC_VAL(rc));
                Log(("Pending pool sync is forcing us back to ring 3; rc=%d\n", VBOXSTRICTRC_VAL(rc)));
                goto end;
            }
        }

#ifdef DEBUG
        /* Intercept X86_XCPT_DB if stepping is enabled */
        if (!DBGFIsStepping(pVCpu))
#endif
        {
            if (    VM_FF_ISPENDING(pVM, VM_FF_HWACCM_TO_R3_MASK)
                ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_HWACCM_TO_R3_MASK))
            {
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatSwitchToR3);
                rc = RT_UNLIKELY(VM_FF_ISPENDING(pVM, VM_FF_PGM_NO_MEMORY)) ? VINF_EM_NO_MEMORY : VINF_EM_RAW_TO_R3;
                goto end;
            }
        }

        /* Pending request packets might contain actions that need immediate attention, such as pending hardware interrupts. */
        if (    VM_FF_ISPENDING(pVM, VM_FF_REQUEST)
            ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_REQUEST))
        {
            rc = VINF_EM_PENDING_REQUEST;
            goto end;
        }

        /* Check if a pgm pool flush is in progress. */
        if (VM_FF_ISPENDING(pVM, VM_FF_PGM_POOL_FLUSH_PENDING))
        {
            rc = VINF_PGM_POOL_FLUSH_PENDING;
            goto end;
        }

        /* Check if DMA work is pending (2nd+ run). */
        if (VM_FF_ISPENDING(pVM, VM_FF_PDM_DMA) && cResume > 1)
        {
            rc = VINF_EM_RAW_TO_R3;
            goto end;
        }
    }

#ifdef VBOX_WITH_VMMR0_DISABLE_PREEMPTION
    /*
     * Exit to ring-3 preemption/work is pending.
     *
     * Interrupts are disabled before the call to make sure we don't miss any interrupt
     * that would flag preemption (IPI, timer tick, ++). (Would've been nice to do this
     * further down, but hmR0VmxCheckPendingInterrupt makes that impossible.)
     *
     * Note! Interrupts must be disabled done *before* we check for TLB flushes; TLB
     *       shootdowns rely on this.
     */
    uOldEFlags = ASMIntDisableFlags();
    if (RTThreadPreemptIsPending(NIL_RTTHREAD))
    {
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitPreemptPending);
        rc = VINF_EM_RAW_INTERRUPT;
        goto end;
    }
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC);
#endif

    /*
     * When external interrupts are pending, we should exit the VM when IF is et.
     * Note: *After* VM_FF_INHIBIT_INTERRUPTS check!
     */
    rc = hmR0VmxCheckPendingInterrupt(pVM, pVCpu, pCtx);
    if (RT_FAILURE(rc))
        goto end;

    /** @todo check timers?? */

    /*
     * TPR caching using CR8 is only available in 64-bit mode.
     * Note: The 32-bit exception for AMD (X86_CPUID_AMD_FEATURE_ECX_CR8L), but this appears missing in Intel CPUs.
     * Note: We can't do this in LoadGuestState() as PDMApicGetTPR can jump back to ring-3 (lock)!! (no longer true)                                                                                                           .
     */
    /** @todo query and update the TPR only when it could have been changed (mmio
     *        access & wrsmr (x2apic) */
    if (fSetupTPRCaching)
    {
        /* TPR caching in CR8 */
        bool    fPending;

        rc2 = PDMApicGetTPR(pVCpu, &u8LastTPR, &fPending);
        AssertRC(rc2);
        /* The TPR can be found at offset 0x80 in the APIC mmio page. */
        pVCpu->hwaccm.s.vmx.pbVAPIC[0x80] = u8LastTPR;

        /*
         * Two options here:
         * - external interrupt pending, but masked by the TPR value.
         *   -> a CR8 update that lower the current TPR value should cause an exit
         * - no pending interrupts
         *   -> We don't need to be explicitely notified. There are enough world switches for detecting pending interrupts.
         */
        rc  = VMXWriteVMCS(VMX_VMCS_CTRL_TPR_THRESHOLD, (fPending) ? (u8LastTPR >> 4) : 0);     /* cr8 bits 3-0 correspond to bits 7-4 of the task priority mmio register. */
        AssertRC(VBOXSTRICTRC_VAL(rc));

        if (pVM->hwaccm.s.fTPRPatchingActive)
        {
            Assert(!CPUMIsGuestInLongModeEx(pCtx));
            /* Our patch code uses LSTAR for TPR caching. */
            pCtx->msrLSTAR = u8LastTPR;

            if (fPending)
            {
                /* A TPR change could activate a pending interrupt, so catch lstar writes. */
                hmR0VmxSetMSRPermission(pVCpu, MSR_K8_LSTAR, true, false);
            }
            else
            {
                /*
                 * No interrupts are pending, so we don't need to be explicitely notified.
                 * There are enough world switches for detecting pending interrupts.
                 */
                hmR0VmxSetMSRPermission(pVCpu, MSR_K8_LSTAR, true, true);
            }
        }
    }

#ifdef LOG_ENABLED
    if (    pVM->hwaccm.s.fNestedPaging
        ||  pVM->hwaccm.s.vmx.fVPID)
    {
        PHMGLOBLCPUINFO pCpu = HWACCMR0GetCurrentCpu();
        if (    pVCpu->hwaccm.s.idLastCpu   != pCpu->idCpu
            ||  pVCpu->hwaccm.s.cTLBFlushes != pCpu->cTLBFlushes)
        {
            if (pVCpu->hwaccm.s.idLastCpu != pCpu->idCpu)
                LogFlow(("Force TLB flush due to rescheduling to a different cpu (%d vs %d)\n", pVCpu->hwaccm.s.idLastCpu, pCpu->idCpu));
            else
                LogFlow(("Force TLB flush due to changed TLB flush count (%x vs %x)\n", pVCpu->hwaccm.s.cTLBFlushes, pCpu->cTLBFlushes));
        }
        else if (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_TLB_FLUSH))
            LogFlow(("Manual TLB flush\n"));
    }
#endif
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PGMRZDynMapFlushAutoSet(pVCpu);
#endif

    /*
     * NOTE: DO NOT DO ANYTHING AFTER THIS POINT THAT MIGHT JUMP BACK TO RING-3!
     *       (until the actual world switch)
     */
#ifdef VBOX_STRICT
    idCpuCheck = RTMpCpuId();
#endif
#ifdef LOG_ENABLED
    VMMR0LogFlushDisable(pVCpu);
#endif

    /*
     * Save the host state first.
     */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_HOST_CONTEXT)
    {
        rc  = VMXR0SaveHostState(pVM, pVCpu);
        if (RT_UNLIKELY(rc != VINF_SUCCESS))
        {
            VMMR0LogFlushEnable(pVCpu);
            goto end;
        }
    }

    /*
     * Load the guest state.
     */
    if (!pVCpu->hwaccm.s.fContextUseFlags)
    {
        VMXR0LoadMinimalGuestState(pVM, pVCpu, pCtx);
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatLoadMinimal);
    }
    else
    {
        rc = VMXR0LoadGuestState(pVM, pVCpu, pCtx);
        if (RT_UNLIKELY(rc != VINF_SUCCESS))
        {
            VMMR0LogFlushEnable(pVCpu);
            goto end;
        }
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatLoadFull);
    }

#ifndef VBOX_WITH_VMMR0_DISABLE_PREEMPTION
    /*
     * Disable interrupts to make sure a poke will interrupt execution.
     * This must be done *before* we check for TLB flushes; TLB shootdowns rely on this.
     */
    uOldEFlags = ASMIntDisableFlags();
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC);
#endif

    /* Non-register state Guest Context */
    /** @todo change me according to cpu state */
    rc2 = VMXWriteVMCS(VMX_VMCS32_GUEST_ACTIVITY_STATE,           VMX_CMS_GUEST_ACTIVITY_ACTIVE);
    AssertRC(rc2);

    /* Set TLB flush state as checked until we return from the world switch. */
    ASMAtomicWriteBool(&pVCpu->hwaccm.s.fCheckedTLBFlush, true);
    /* Deal with tagged TLB setup and invalidation. */
    pVM->hwaccm.s.vmx.pfnSetupTaggedTLB(pVM, pVCpu);

    /*
     * Manual save and restore:
     * - General purpose registers except RIP, RSP
     *
     * Trashed:
     * - CR2 (we don't care)
     * - LDTR (reset to 0)
     * - DRx (presumably not changed at all)
     * - DR7 (reset to 0x400)
     * - EFLAGS (reset to RT_BIT(1); not relevant)
     */

    /* All done! Let's start VM execution. */
    STAM_PROFILE_ADV_STOP_START(&pVCpu->hwaccm.s.StatEntry, &pVCpu->hwaccm.s.StatInGC, x);
    Assert(idCpuCheck == RTMpCpuId());

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    pVCpu->hwaccm.s.vmx.VMCSCache.cResume = cResume;
    pVCpu->hwaccm.s.vmx.VMCSCache.u64TimeSwitch = RTTimeNanoTS();
#endif

    /*
     * Save the current TPR value in the LSTAR MSR so our patches can access it.
     */
    if (pVM->hwaccm.s.fTPRPatchingActive)
    {
        Assert(pVM->hwaccm.s.fTPRPatchingActive);
        u64OldLSTAR = ASMRdMsr(MSR_K8_LSTAR);
        ASMWrMsr(MSR_K8_LSTAR, u8LastTPR);
    }

    TMNotifyStartOfExecution(pVCpu);
#ifdef VBOX_WITH_KERNEL_USING_XMM
    rc = hwaccmR0VMXStartVMWrapXMM(pVCpu->hwaccm.s.fResumeVM, pCtx, &pVCpu->hwaccm.s.vmx.VMCSCache, pVM, pVCpu, pVCpu->hwaccm.s.vmx.pfnStartVM);
#else
    rc = pVCpu->hwaccm.s.vmx.pfnStartVM(pVCpu->hwaccm.s.fResumeVM, pCtx, &pVCpu->hwaccm.s.vmx.VMCSCache, pVM, pVCpu);
#endif
    ASMAtomicWriteBool(&pVCpu->hwaccm.s.fCheckedTLBFlush, false);
    ASMAtomicIncU32(&pVCpu->hwaccm.s.cWorldSwitchExits);
    /* Possibly the last TSC value seen by the guest (too high) (only when we're in TSC offset mode). */
    if (!(pVCpu->hwaccm.s.vmx.proc_ctls & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT))
    {
        TMCpuTickSetLastSeen(pVCpu,
                             ASMReadTSC() + pVCpu->hwaccm.s.vmx.u64TSCOffset - 0x400 /* guestimate of world switch overhead in clock ticks */);
    }

    TMNotifyEndOfExecution(pVCpu);
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);
    Assert(!(ASMGetFlags() & X86_EFL_IF));

    /*
     * Restore the host LSTAR MSR if the guest could have changed it.
     */
    if (pVM->hwaccm.s.fTPRPatchingActive)
    {
        Assert(pVM->hwaccm.s.fTPRPatchingActive);
        pVCpu->hwaccm.s.vmx.pbVAPIC[0x80] = pCtx->msrLSTAR = ASMRdMsr(MSR_K8_LSTAR);
        ASMWrMsr(MSR_K8_LSTAR, u64OldLSTAR);
    }

    STAM_PROFILE_ADV_STOP_START(&pVCpu->hwaccm.s.StatInGC, &pVCpu->hwaccm.s.StatExit1, x);
    ASMSetFlags(uOldEFlags);
#ifdef VBOX_WITH_VMMR0_DISABLE_PREEMPTION
    uOldEFlags = ~(RTCCUINTREG)0;
#endif

    AssertMsg(!pVCpu->hwaccm.s.vmx.VMCSCache.Write.cValidEntries, ("pVCpu->hwaccm.s.vmx.VMCSCache.Write.cValidEntries=%d\n", pVCpu->hwaccm.s.vmx.VMCSCache.Write.cValidEntries));

    /* In case we execute a goto ResumeExecution later on. */
    pVCpu->hwaccm.s.fResumeVM  = true;
    pVCpu->hwaccm.s.fForceTLBFlush = false;

    /*
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * IMPORTANT: WE CAN'T DO ANY LOGGING OR OPERATIONS THAT CAN DO A LONGJMP BACK TO RING 3 *BEFORE* WE'VE SYNCED BACK (MOST OF) THE GUEST STATE
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */

    if (RT_UNLIKELY(rc != VINF_SUCCESS))
    {
        hmR0VmxReportWorldSwitchError(pVM, pVCpu, rc, pCtx);
        VMMR0LogFlushEnable(pVCpu);
        goto end;
    }

    /* Success. Query the guest state and figure out what has happened. */

    /* Investigate why there was a VM-exit. */
    rc2  = VMXReadCachedVMCS(VMX_VMCS32_RO_EXIT_REASON, &exitReason);
    STAM_COUNTER_INC(&pVCpu->hwaccm.s.paStatExitReasonR0[exitReason & MASK_EXITREASON_STAT]);

    exitReason &= 0xffff;   /* bit 0-15 contain the exit code. */
    rc2 |= VMXReadCachedVMCS(VMX_VMCS32_RO_VM_INSTR_ERROR, &instrError);
    rc2 |= VMXReadCachedVMCS(VMX_VMCS32_RO_EXIT_INSTR_LENGTH, &cbInstr);
    rc2 |= VMXReadCachedVMCS(VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO, &intInfo);
    /* might not be valid; depends on VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_IS_VALID. */
    rc2 |= VMXReadCachedVMCS(VMX_VMCS32_RO_EXIT_INTERRUPTION_ERRCODE, &errCode);
    rc2 |= VMXReadCachedVMCS(VMX_VMCS32_RO_EXIT_INSTR_INFO, &instrInfo);
    rc2 |= VMXReadCachedVMCS(VMX_VMCS_RO_EXIT_QUALIFICATION, &exitQualification);
    AssertRC(rc2);

    /*
     * Sync back the guest state.
     */
    rc2 = VMXR0SaveGuestState(pVM, pVCpu, pCtx);
    AssertRC(rc2);

    /* Note! NOW IT'S SAFE FOR LOGGING! */
    VMMR0LogFlushEnable(pVCpu);
    Log2(("Raw exit reason %08x\n", exitReason));
#if ARCH_BITS == 64 /* for the time being */
    VBOXVMM_R0_HMVMX_VMEXIT(pVCpu, pCtx, exitReason);
#endif

    /*
     * Check if an injected event was interrupted prematurely.
     */
    rc2 = VMXReadCachedVMCS(VMX_VMCS32_RO_IDT_INFO,            &val);
    AssertRC(rc2);
    pVCpu->hwaccm.s.Event.intInfo = VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(val);
    if (    VMX_EXIT_INTERRUPTION_INFO_VALID(pVCpu->hwaccm.s.Event.intInfo)
        /* Ignore 'int xx' as they'll be restarted anyway. */
        &&  VMX_EXIT_INTERRUPTION_INFO_TYPE(pVCpu->hwaccm.s.Event.intInfo) != VMX_EXIT_INTERRUPTION_INFO_TYPE_SW
        /* Ignore software exceptions (such as int3) as they'll reoccur when we restart the instruction anyway. */
        &&  VMX_EXIT_INTERRUPTION_INFO_TYPE(pVCpu->hwaccm.s.Event.intInfo) != VMX_EXIT_INTERRUPTION_INFO_TYPE_SWEXCPT)
    {
        Assert(!pVCpu->hwaccm.s.Event.fPending);
        pVCpu->hwaccm.s.Event.fPending = true;
        /* Error code present? */
        if (VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_IS_VALID(pVCpu->hwaccm.s.Event.intInfo))
        {
            rc2 = VMXReadCachedVMCS(VMX_VMCS32_RO_IDT_ERRCODE, &val);
            AssertRC(rc2);
            pVCpu->hwaccm.s.Event.errCode  = val;
            Log(("Pending inject %RX64 at %RGv exit=%08x intInfo=%08x exitQualification=%RGv pending error=%RX64\n",
                 pVCpu->hwaccm.s.Event.intInfo, (RTGCPTR)pCtx->rip, exitReason, intInfo, exitQualification, val));
        }
        else
        {
            Log(("Pending inject %RX64 at %RGv exit=%08x intInfo=%08x exitQualification=%RGv\n", pVCpu->hwaccm.s.Event.intInfo,
                 (RTGCPTR)pCtx->rip, exitReason, intInfo, exitQualification));
            pVCpu->hwaccm.s.Event.errCode  = 0;
        }
    }
#ifdef VBOX_STRICT
    else if (   VMX_EXIT_INTERRUPTION_INFO_VALID(pVCpu->hwaccm.s.Event.intInfo)
                /* Ignore software exceptions (such as int3) as they're reoccur when we restart the instruction anyway. */
             && VMX_EXIT_INTERRUPTION_INFO_TYPE(pVCpu->hwaccm.s.Event.intInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_SWEXCPT)
    {
        Log(("Ignore pending inject %RX64 at %RGv exit=%08x intInfo=%08x exitQualification=%RGv\n",
             pVCpu->hwaccm.s.Event.intInfo, (RTGCPTR)pCtx->rip, exitReason, intInfo, exitQualification));
    }

    if (exitReason == VMX_EXIT_ERR_INVALID_GUEST_STATE)
        HWACCMDumpRegs(pVM, pVCpu, pCtx);
#endif

    Log2(("E%d: New EIP=%x:%RGv\n", (uint32_t)exitReason, pCtx->cs, (RTGCPTR)pCtx->rip));
    Log2(("Exit reason %d, exitQualification %RGv\n", (uint32_t)exitReason, exitQualification));
    Log2(("instrInfo=%d instrError=%d instr length=%d\n", (uint32_t)instrInfo, (uint32_t)instrError, (uint32_t)cbInstr));
    Log2(("Interruption error code %d\n", (uint32_t)errCode));
    Log2(("IntInfo = %08x\n", (uint32_t)intInfo));

    /*
     * Sync back the TPR if it was changed.
     */
    if (    fSetupTPRCaching
        &&  u8LastTPR != pVCpu->hwaccm.s.vmx.pbVAPIC[0x80])
    {
        rc2 = PDMApicSetTPR(pVCpu, pVCpu->hwaccm.s.vmx.pbVAPIC[0x80]);
        AssertRC(rc2);
    }

#ifdef DBGFTRACE_ENABLED /** @todo DTrace later. */
    RTTraceBufAddMsgF(pVM->CTX_SUFF(hTraceBuf), "vmexit %08x %016RX64 at %04:%08RX64 %RX64",
                      exitReason, (uint64_t)exitQualification, pCtx->cs, pCtx->rip, (uint64_t)intInfo);
#endif
    STAM_PROFILE_ADV_STOP_START(&pVCpu->hwaccm.s.StatExit1, &pVCpu->hwaccm.s.StatExit2, x);

    /* Some cases don't need a complete resync of the guest CPU state; handle them here. */
    Assert(rc == VINF_SUCCESS); /* might consider VERR_IPE_UNINITIALIZED_STATUS here later... */
    switch (exitReason)
    {
    case VMX_EXIT_EXCEPTION:            /* 0 Exception or non-maskable interrupt (NMI). */
    case VMX_EXIT_EXTERNAL_IRQ:         /* 1 External interrupt. */
    {
        uint32_t vector = VMX_EXIT_INTERRUPTION_INFO_VECTOR(intInfo);

        if (!VMX_EXIT_INTERRUPTION_INFO_VALID(intInfo))
        {
            Assert(exitReason == VMX_EXIT_EXTERNAL_IRQ);
#if 0 //def VBOX_WITH_VMMR0_DISABLE_PREEMPTION
            if (    RTThreadPreemptIsPendingTrusty()
                &&  !RTThreadPreemptIsPending(NIL_RTTHREAD))
                goto ResumeExecution;
#endif
            /* External interrupt; leave to allow it to be dispatched again. */
            rc = VINF_EM_RAW_INTERRUPT;
            break;
        }
        STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
        switch (VMX_EXIT_INTERRUPTION_INFO_TYPE(intInfo))
        {
        case VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI:   /* Non-maskable interrupt. */
            /* External interrupt; leave to allow it to be dispatched again. */
            rc = VINF_EM_RAW_INTERRUPT;
            break;

        case VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT:   /* External hardware interrupt. */
            AssertFailed(); /* can't come here; fails the first check. */
            break;

        case VMX_EXIT_INTERRUPTION_INFO_TYPE_DBEXCPT:   /* Unknown why we get this type for #DB */
        case VMX_EXIT_INTERRUPTION_INFO_TYPE_SWEXCPT:   /* Software exception. (#BP or #OF) */
            Assert(vector == 1 || vector == 3 || vector == 4);
            /* no break */
        case VMX_EXIT_INTERRUPTION_INFO_TYPE_HWEXCPT:   /* Hardware exception. */
            Log2(("Hardware/software interrupt %d\n", vector));
            switch (vector)
            {
            case X86_XCPT_NM:
            {
                Log(("#NM fault at %RGv error code %x\n", (RTGCPTR)pCtx->rip, errCode));

                /** @todo don't intercept #NM exceptions anymore when we've activated the guest FPU state. */
                /* If we sync the FPU/XMM state on-demand, then we can continue execution as if nothing has happened. */
                rc = CPUMR0LoadGuestFPU(pVM, pVCpu, pCtx);
                if (rc == VINF_SUCCESS)
                {
                    Assert(CPUMIsGuestFPUStateActive(pVCpu));

                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitShadowNM);

                    /* Continue execution. */
                    pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0;

                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }

                Log(("Forward #NM fault to the guest\n"));
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestNM);
                rc2 = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo),
                                         cbInstr, 0);
                AssertRC(rc2);
                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                goto ResumeExecution;
            }

            case X86_XCPT_PF: /* Page fault */
            {
#ifdef VBOX_ALWAYS_TRAP_PF
                if (pVM->hwaccm.s.fNestedPaging)
                {
                    /*
                     * A genuine pagefault. Forward the trap to the guest by injecting the exception and resuming execution.
                     */
                    Log(("Guest page fault at %RGv cr2=%RGv error code %RGv rsp=%RGv\n", (RTGCPTR)pCtx->rip, exitQualification,
                         errCode, (RTGCPTR)pCtx->rsp));

                    Assert(CPUMIsGuestInPagedProtectedModeEx(pCtx));

                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestPF);

                    /* Now we must update CR2. */
                    pCtx->cr2 = exitQualification;
                    rc2 = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo),
                                             cbInstr, errCode);
                    AssertRC(rc2);

                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
#else
                Assert(!pVM->hwaccm.s.fNestedPaging);
#endif

#ifdef VBOX_HWACCM_WITH_GUEST_PATCHING
                /* Shortcut for APIC TPR reads and writes; 32 bits guests only */
                if (    pVM->hwaccm.s.fTRPPatchingAllowed
                    &&  pVM->hwaccm.s.pGuestPatchMem
                    &&  (exitQualification & 0xfff) == 0x080
                    &&  !(errCode & X86_TRAP_PF_P)  /* not present */
                    &&  CPUMGetGuestCPL(pVCpu, CPUMCTX2CORE(pCtx)) == 0
                    &&  !CPUMIsGuestInLongModeEx(pCtx)
                    &&  pVM->hwaccm.s.cPatches < RT_ELEMENTS(pVM->hwaccm.s.aPatches))
                {
                    RTGCPHYS GCPhysApicBase, GCPhys;
                    PDMApicGetBase(pVM, &GCPhysApicBase);   /** @todo cache this */
                    GCPhysApicBase &= PAGE_BASE_GC_MASK;

                    rc = PGMGstGetPage(pVCpu, (RTGCPTR)exitQualification, NULL, &GCPhys);
                    if (    rc == VINF_SUCCESS
                        &&  GCPhys == GCPhysApicBase)
                    {
                        /* Only attempt to patch the instruction once. */
                        PHWACCMTPRPATCH pPatch = (PHWACCMTPRPATCH)RTAvloU32Get(&pVM->hwaccm.s.PatchTree, (AVLOU32KEY)pCtx->eip);
                        if (!pPatch)
                        {
                            rc = VINF_EM_HWACCM_PATCH_TPR_INSTR;
                            break;
                        }
                    }
                }
#endif

                Log2(("Page fault at %RGv error code %x\n", exitQualification, errCode));
                /* Exit qualification contains the linear address of the page fault. */
                TRPMAssertTrap(pVCpu, X86_XCPT_PF, TRPM_TRAP);
                TRPMSetErrorCode(pVCpu, errCode);
                TRPMSetFaultAddress(pVCpu, exitQualification);

                /* Shortcut for APIC TPR reads and writes. */
                if (    (exitQualification & 0xfff) == 0x080
                    &&  !(errCode & X86_TRAP_PF_P)  /* not present */
                    &&  fSetupTPRCaching
                    &&  (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC))
                {
                    RTGCPHYS GCPhysApicBase, GCPhys;
                    PDMApicGetBase(pVM, &GCPhysApicBase);   /* @todo cache this */
                    GCPhysApicBase &= PAGE_BASE_GC_MASK;

                    rc = PGMGstGetPage(pVCpu, (RTGCPTR)exitQualification, NULL, &GCPhys);
                    if (    rc == VINF_SUCCESS
                        &&  GCPhys == GCPhysApicBase)
                    {
                        Log(("Enable VT-x virtual APIC access filtering\n"));
                        rc2 = IOMMMIOMapMMIOHCPage(pVM, GCPhysApicBase, pVM->hwaccm.s.vmx.pAPICPhys, X86_PTE_RW | X86_PTE_P);
                        AssertRC(rc2);
                    }
                }

                /* Forward it to our trap handler first, in case our shadow pages are out of sync. */
                rc = PGMTrap0eHandler(pVCpu, errCode, CPUMCTX2CORE(pCtx), (RTGCPTR)exitQualification);
                Log2(("PGMTrap0eHandler %RGv returned %Rrc\n", (RTGCPTR)pCtx->rip, VBOXSTRICTRC_VAL(rc)));

                if (rc == VINF_SUCCESS)
                {   /* We've successfully synced our shadow pages, so let's just continue execution. */
                    Log2(("Shadow page fault at %RGv cr2=%RGv error code %x\n", (RTGCPTR)pCtx->rip, exitQualification ,errCode));
                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitShadowPF);

                    TRPMResetTrap(pVCpu);
                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
                else if (rc == VINF_EM_RAW_GUEST_TRAP)
                {
                    /*
                     * A genuine pagefault. Forward the trap to the guest by injecting the exception and resuming execution.
                     */
                    Log2(("Forward page fault to the guest\n"));

                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestPF);
                    /* The error code might have been changed. */
                    errCode = TRPMGetErrorCode(pVCpu);

                    TRPMResetTrap(pVCpu);

                    /* Now we must update CR2. */
                    pCtx->cr2 = exitQualification;
                    rc2 = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo),
                                             cbInstr, errCode);
                    AssertRC(rc2);

                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
#ifdef VBOX_STRICT
                if (rc != VINF_EM_RAW_EMULATE_INSTR && rc != VINF_EM_RAW_EMULATE_IO_BLOCK)
                    Log2(("PGMTrap0eHandler failed with %d\n", VBOXSTRICTRC_VAL(rc)));
#endif
                /* Need to go back to the recompiler to emulate the instruction. */
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitShadowPFEM);
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
                Log(("Trap %x at %04X:%RGv\n", vector, pCtx->cs, (RTGCPTR)pCtx->rip));
                rc2 = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo),
                                         cbInstr, errCode);
                AssertRC(rc2);

                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                goto ResumeExecution;
            }

            case X86_XCPT_DB:   /* Debug exception. */
            {
                uint64_t uDR6;

                /*
                 * DR6, DR7.GD and IA32_DEBUGCTL.LBR are not updated yet.
                 *
                 * Exit qualification bits:
                 *  3:0     B0-B3 which breakpoint condition was met
                 * 12:4     Reserved (0)
                 * 13       BD - debug register access detected
                 * 14       BS - single step execution or branch taken
                 * 63:15    Reserved (0)
                 */
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestDB);

                /* Note that we don't support guest and host-initiated debugging at the same time. */

                uDR6  = X86_DR6_INIT_VAL;
                uDR6 |= (exitQualification & (X86_DR6_B0|X86_DR6_B1|X86_DR6_B2|X86_DR6_B3|X86_DR6_BD|X86_DR6_BS));
                rc = DBGFRZTrap01Handler(pVM, pVCpu, CPUMCTX2CORE(pCtx), uDR6);
                if (rc == VINF_EM_RAW_GUEST_TRAP)
                {
                    /* Update DR6 here. */
                    pCtx->dr[6]  = uDR6;

                    /* Resync DR6 if the debug state is active. */
                    if (CPUMIsGuestDebugStateActive(pVCpu))
                        ASMSetDR6(pCtx->dr[6]);

                    /* X86_DR7_GD will be cleared if DRx accesses should be trapped inside the guest. */
                    pCtx->dr[7] &= ~X86_DR7_GD;

                    /* Paranoia. */
                    pCtx->dr[7] &= 0xffffffff;                                              /* upper 32 bits reserved */
                    pCtx->dr[7] &= ~(RT_BIT(11) | RT_BIT(12) | RT_BIT(14) | RT_BIT(15));    /* must be zero */
                    pCtx->dr[7] |= 0x400;                                                   /* must be one */

                    /* Resync DR7 */
                    rc2 = VMXWriteVMCS64(VMX_VMCS64_GUEST_DR7, pCtx->dr[7]);
                    AssertRC(rc2);

                    Log(("Trap %x (debug) at %RGv exit qualification %RX64 dr6=%x dr7=%x\n", vector, (RTGCPTR)pCtx->rip,
                         exitQualification, (uint32_t)pCtx->dr[6], (uint32_t)pCtx->dr[7]));
                    rc2 = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo),
                                             cbInstr, errCode);
                    AssertRC(rc2);

                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
                /* Return to ring 3 to deal with the debug exit code. */
                Log(("Debugger hardware BP at %04x:%RGv (rc=%Rrc)\n", pCtx->cs, pCtx->rip, VBOXSTRICTRC_VAL(rc)));
                break;
            }

            case X86_XCPT_BP:   /* Breakpoint. */
            {
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestBP);
                rc = DBGFRZTrap03Handler(pVM, pVCpu, CPUMCTX2CORE(pCtx));
                if (rc == VINF_EM_RAW_GUEST_TRAP)
                {
                    Log(("Guest #BP at %04x:%RGv\n", pCtx->cs, pCtx->rip));
                    rc2 = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo),
                                             cbInstr, errCode);
                    AssertRC(rc2);
                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
                if (rc == VINF_SUCCESS)
                {
                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
                Log(("Debugger BP at %04x:%RGv (rc=%Rrc)\n", pCtx->cs, pCtx->rip, VBOXSTRICTRC_VAL(rc)));
                break;
            }

            case X86_XCPT_GP:   /* General protection failure exception. */
            {
                uint32_t     cbOp;
                PDISCPUSTATE pDis = &pVCpu->hwaccm.s.DisState;

                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestGP);
#ifdef VBOX_STRICT
                if (    !CPUMIsGuestInRealModeEx(pCtx)
                    ||  !pVM->hwaccm.s.vmx.pRealModeTSS)
                {
                    Log(("Trap %x at %04X:%RGv errorCode=%RGv\n", vector, pCtx->cs, (RTGCPTR)pCtx->rip, errCode));
                    rc2 = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo),
                                             cbInstr, errCode);
                    AssertRC(rc2);
                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
#endif
                Assert(CPUMIsGuestInRealModeEx(pCtx));

                LogFlow(("Real mode X86_XCPT_GP instruction emulation at %x:%RGv\n", pCtx->cs, (RTGCPTR)pCtx->rip));

                rc2 = EMInterpretDisasOne(pVM, pVCpu, CPUMCTX2CORE(pCtx), pDis, &cbOp);
                if (RT_SUCCESS(rc2))
                {
                    bool fUpdateRIP = true;

                    rc = VINF_SUCCESS;
                    Assert(cbOp == pDis->opsize);
                    switch (pDis->pCurInstr->opcode)
                    {
                    case OP_CLI:
                        pCtx->eflags.Bits.u1IF = 0;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitCli);
                        break;

                    case OP_STI:
                        pCtx->eflags.Bits.u1IF = 1;
                        EMSetInhibitInterruptsPC(pVCpu, pCtx->rip + pDis->opsize);
                        Assert(VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));
                        rc2 = VMXWriteVMCS(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE,
                                           VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI);
                        AssertRC(rc2);
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitSti);
                        break;

                    case OP_HLT:
                        fUpdateRIP = false;
                        rc = VINF_EM_HALT;
                        pCtx->rip += pDis->opsize;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitHlt);
                        break;

                    case OP_POPF:
                    {
                        RTGCPTR   GCPtrStack;
                        uint32_t  cbParm;
                        uint32_t  uMask;
                        X86EFLAGS eflags;

                        if (pDis->prefix & PREFIX_OPSIZE)
                        {
                            cbParm = 4;
                            uMask  = 0xffffffff;
                        }
                        else
                        {
                            cbParm = 2;
                            uMask  = 0xffff;
                        }

                        rc2 = SELMToFlatEx(pVCpu, DIS_SELREG_SS, CPUMCTX2CORE(pCtx), pCtx->esp & uMask, 0, &GCPtrStack);
                        if (RT_FAILURE(rc2))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }
                        eflags.u = 0;
                        rc2 = PGMPhysRead(pVM, (RTGCPHYS)GCPtrStack, &eflags.u, cbParm);
                        if (RT_FAILURE(rc2))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }
                        LogFlow(("POPF %x -> %RGv mask=%x\n", eflags.u, pCtx->rsp, uMask));
                        pCtx->eflags.u = (pCtx->eflags.u & ~(X86_EFL_POPF_BITS & uMask)) | (eflags.u & X86_EFL_POPF_BITS & uMask);
                        /* RF cleared when popped in real mode; see pushf description in AMD manual. */
                        pCtx->eflags.Bits.u1RF = 0;
                        pCtx->esp += cbParm;
                        pCtx->esp &= uMask;

                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitPopf);
                        break;
                    }

                    case OP_PUSHF:
                    {
                        RTGCPTR   GCPtrStack;
                        uint32_t  cbParm;
                        uint32_t  uMask;
                        X86EFLAGS eflags;

                        if (pDis->prefix & PREFIX_OPSIZE)
                        {
                            cbParm = 4;
                            uMask  = 0xffffffff;
                        }
                        else
                        {
                            cbParm = 2;
                            uMask  = 0xffff;
                        }

                        rc2 = SELMToFlatEx(pVCpu, DIS_SELREG_SS, CPUMCTX2CORE(pCtx), (pCtx->esp - cbParm) & uMask, 0,
                                           &GCPtrStack);
                        if (RT_FAILURE(rc2))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }
                        eflags = pCtx->eflags;
                        /* RF & VM cleared when pushed in real mode; see pushf description in AMD manual. */
                        eflags.Bits.u1RF = 0;
                        eflags.Bits.u1VM = 0;

                        rc2 = PGMPhysWrite(pVM, (RTGCPHYS)GCPtrStack, &eflags.u, cbParm);
                        if (RT_FAILURE(rc2))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }
                        LogFlow(("PUSHF %x -> %RGv\n", eflags.u, GCPtrStack));
                        pCtx->esp -= cbParm;
                        pCtx->esp &= uMask;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitPushf);
                        break;
                    }

                    case OP_IRET:
                    {
                        RTGCPTR   GCPtrStack;
                        uint32_t  uMask = 0xffff;
                        uint16_t  aIretFrame[3];

                        if (pDis->prefix & (PREFIX_OPSIZE | PREFIX_ADDRSIZE))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }

                        rc2 = SELMToFlatEx(pVCpu, DIS_SELREG_SS, CPUMCTX2CORE(pCtx), pCtx->esp & uMask, 0, &GCPtrStack);
                        if (RT_FAILURE(rc2))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }
                        rc2 = PGMPhysRead(pVM, (RTGCPHYS)GCPtrStack, &aIretFrame[0], sizeof(aIretFrame));
                        if (RT_FAILURE(rc2))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }
                        pCtx->ip            = aIretFrame[0];
                        pCtx->cs            = aIretFrame[1];
                        pCtx->csHid.u64Base = pCtx->cs << 4;
                        pCtx->eflags.u      =   (pCtx->eflags.u & ~(X86_EFL_POPF_BITS & uMask))
                                              | (aIretFrame[2] & X86_EFL_POPF_BITS & uMask);
                        pCtx->sp           += sizeof(aIretFrame);

                        LogFlow(("iret to %04x:%x\n", pCtx->cs, pCtx->ip));
                        fUpdateRIP = false;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIret);
                        break;
                    }

                    case OP_INT:
                    {
                        uint32_t intInfo2;

                        LogFlow(("Realmode: INT %x\n", pDis->param1.parval & 0xff));
                        intInfo2  = pDis->param1.parval & 0xff;
                        intInfo2 |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
                        intInfo2 |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

                        rc = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, intInfo2, cbOp, 0);
                        AssertRC(VBOXSTRICTRC_VAL(rc));
                        fUpdateRIP = false;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitInt);
                        break;
                    }

                    case OP_INTO:
                    {
                        if (pCtx->eflags.Bits.u1OF)
                        {
                            uint32_t intInfo2;

                            LogFlow(("Realmode: INTO\n"));
                            intInfo2  = X86_XCPT_OF;
                            intInfo2 |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
                            intInfo2 |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

                            rc = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, intInfo2, cbOp, 0);
                            AssertRC(VBOXSTRICTRC_VAL(rc));
                            fUpdateRIP = false;
                            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitInt);
                        }
                        break;
                    }

                    case OP_INT3:
                    {
                        uint32_t intInfo2;

                        LogFlow(("Realmode: INT 3\n"));
                        intInfo2  = 3;
                        intInfo2 |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
                        intInfo2 |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

                        rc = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, intInfo2, cbOp, 0);
                        AssertRC(VBOXSTRICTRC_VAL(rc));
                        fUpdateRIP = false;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitInt);
                        break;
                    }

                    default:
                        rc = EMInterpretInstructionDisasState(pVCpu, pDis, CPUMCTX2CORE(pCtx), 0, EMCODETYPE_SUPERVISOR);
                        fUpdateRIP = false;
                        break;
                    }

                    if (rc == VINF_SUCCESS)
                    {
                        if (fUpdateRIP)
                            pCtx->rip += cbOp; /* Move on to the next instruction. */

                        /*
                         * LIDT, LGDT can end up here. In the future CRx changes as well. Just reload the
                         * whole context to be done with it.
                         */
                        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_ALL;

                        /* Only resume if successful. */
                        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                        goto ResumeExecution;
                    }
                }
                else
                    rc = VERR_EM_INTERPRETER;

                AssertMsg(rc == VERR_EM_INTERPRETER || rc == VINF_PGM_CHANGE_MODE || rc == VINF_EM_HALT, ("Unexpected rc=%Rrc\n", VBOXSTRICTRC_VAL(rc)));
                break;
            }

#ifdef VBOX_STRICT
            case X86_XCPT_XF:   /* SIMD exception. */
            case X86_XCPT_DE:   /* Divide error. */
            case X86_XCPT_UD:   /* Unknown opcode exception. */
            case X86_XCPT_SS:   /* Stack segment exception. */
            case X86_XCPT_NP:   /* Segment not present exception. */
            {
                switch(vector)
                {
                    case X86_XCPT_DE:
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestDE);
                        break;
                    case X86_XCPT_UD:
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestUD);
                        break;
                    case X86_XCPT_SS:
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestSS);
                        break;
                    case X86_XCPT_NP:
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestNP);
                        break;
                    case X86_XCPT_XF:
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestXF);
                        break;
                }

                Log(("Trap %x at %04X:%RGv\n", vector, pCtx->cs, (RTGCPTR)pCtx->rip));
                rc2 = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo),
                                         cbInstr, errCode);
                AssertRC(rc2);

                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                goto ResumeExecution;
            }
#endif
            default:
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestXcpUnk);
                if (    CPUMIsGuestInRealModeEx(pCtx)
                    &&  pVM->hwaccm.s.vmx.pRealModeTSS)
                {
                    Log(("Real Mode Trap %x at %04x:%04X error code %x\n", vector, pCtx->cs, pCtx->eip, errCode));
                    rc = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo),
                                            cbInstr, errCode);
                    AssertRC(VBOXSTRICTRC_VAL(rc)); /* Strict RC check below. */

                    /* Go back to ring-3 in case of a triple fault. */
                    if (   vector == X86_XCPT_DF
                        && rc == VINF_EM_RESET)
                    {
                        break;
                    }

                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
                AssertMsgFailed(("Unexpected vm-exit caused by exception %x\n", vector));
                rc = VERR_VMX_UNEXPECTED_EXCEPTION;
                break;
            } /* switch (vector) */

            break;

        default:
            rc = VERR_VMX_UNEXPECTED_INTERRUPTION_EXIT_CODE;
            AssertMsgFailed(("Unexpected interruption code %x\n", intInfo));
            break;
        }

        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
        break;
    }

    /*
     * 48 EPT violation. An attemp to access memory with a guest-physical address was disallowed
     * by the configuration of the EPT paging structures.
     */
    case VMX_EXIT_EPT_VIOLATION:
    {
        RTGCPHYS GCPhys;

        Assert(pVM->hwaccm.s.fNestedPaging);

        rc2 = VMXReadVMCS64(VMX_VMCS_EXIT_PHYS_ADDR_FULL, &GCPhys);
        AssertRC(rc2);
        Assert(((exitQualification >> 7) & 3) != 2);

        /* Determine the kind of violation. */
        errCode = 0;
        if (exitQualification & VMX_EXIT_QUALIFICATION_EPT_INSTR_FETCH)
            errCode |= X86_TRAP_PF_ID;

        if (exitQualification & VMX_EXIT_QUALIFICATION_EPT_DATA_WRITE)
            errCode |= X86_TRAP_PF_RW;

        /* If the page is present, then it's a page level protection fault. */
        if (exitQualification & VMX_EXIT_QUALIFICATION_EPT_ENTRY_PRESENT)
            errCode |= X86_TRAP_PF_P;
        else
        {
            /* Shortcut for APIC TPR reads and writes. */
            if (    (GCPhys & 0xfff) == 0x080
                &&  GCPhys > 0x1000000          /* to skip VGA frame buffer accesses */
                &&  fSetupTPRCaching
                &&  (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC))
            {
                RTGCPHYS GCPhysApicBase;
                PDMApicGetBase(pVM, &GCPhysApicBase);   /* @todo cache this */
                GCPhysApicBase &= PAGE_BASE_GC_MASK;
                if (GCPhys == GCPhysApicBase + 0x80)
                {
                    Log(("Enable VT-x virtual APIC access filtering\n"));
                    rc2 = IOMMMIOMapMMIOHCPage(pVM, GCPhysApicBase, pVM->hwaccm.s.vmx.pAPICPhys, X86_PTE_RW | X86_PTE_P);
                    AssertRC(rc2);
                }
            }
        }
        Log(("EPT Page fault %x at %RGp error code %x\n", (uint32_t)exitQualification, GCPhys, errCode));

        /* GCPhys contains the guest physical address of the page fault. */
        TRPMAssertTrap(pVCpu, X86_XCPT_PF, TRPM_TRAP);
        TRPMSetErrorCode(pVCpu, errCode);
        TRPMSetFaultAddress(pVCpu, GCPhys);

        /* Handle the pagefault trap for the nested shadow table. */
        rc = PGMR0Trap0eHandlerNestedPaging(pVM, pVCpu, PGMMODE_EPT, errCode, CPUMCTX2CORE(pCtx), GCPhys);

        /*
         * Same case as PGMR0Trap0eHandlerNPMisconfig(). See comment below, @bugref{6043}.
         */
        if (   rc == VINF_SUCCESS
            || rc == VERR_PAGE_TABLE_NOT_PRESENT
            || rc == VERR_PAGE_NOT_PRESENT)
        {
            /* We've successfully synced our shadow pages, so let's just continue execution. */
            Log2(("Shadow page fault at %RGv cr2=%RGp error code %x\n", (RTGCPTR)pCtx->rip, exitQualification , errCode));
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitReasonNPF);

            TRPMResetTrap(pVCpu);
            goto ResumeExecution;
        }

#ifdef VBOX_STRICT
        if (rc != VINF_EM_RAW_EMULATE_INSTR)
            LogFlow(("PGMTrap0eHandlerNestedPaging at %RGv failed with %Rrc\n", (RTGCPTR)pCtx->rip, VBOXSTRICTRC_VAL(rc)));
#endif
        /* Need to go back to the recompiler to emulate the instruction. */
        TRPMResetTrap(pVCpu);
        break;
    }

    case VMX_EXIT_EPT_MISCONFIG:
    {
        RTGCPHYS GCPhys;

        Assert(pVM->hwaccm.s.fNestedPaging);

        rc2 = VMXReadVMCS64(VMX_VMCS_EXIT_PHYS_ADDR_FULL, &GCPhys);
        AssertRC(rc2);
        Log(("VMX_EXIT_EPT_MISCONFIG for %RGp\n", GCPhys));

        /* Shortcut for APIC TPR reads and writes. */
        if (    (GCPhys & 0xfff) == 0x080
            &&  GCPhys > 0x1000000              /* to skip VGA frame buffer accesses */
            &&  fSetupTPRCaching
            &&  (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC))
        {
            RTGCPHYS GCPhysApicBase;
            PDMApicGetBase(pVM, &GCPhysApicBase);   /* @todo cache this */
            GCPhysApicBase &= PAGE_BASE_GC_MASK;
            if (GCPhys == GCPhysApicBase + 0x80)
            {
                Log(("Enable VT-x virtual APIC access filtering\n"));
                rc2 = IOMMMIOMapMMIOHCPage(pVM, GCPhysApicBase, pVM->hwaccm.s.vmx.pAPICPhys, X86_PTE_RW | X86_PTE_P);
                AssertRC(rc2);
            }
        }

        rc = PGMR0Trap0eHandlerNPMisconfig(pVM, pVCpu, PGMMODE_EPT, CPUMCTX2CORE(pCtx), GCPhys, UINT32_MAX);

        /*
         * If we succeed, resume execution.
         * Or, if fail in interpreting the instruction because we couldn't get the guest physical address
         * of the page containing the instruction via the guest's page tables (we would invalidate the guest page
         * in the host TLB), resume execution which would cause a guest page fault to let the guest handle this
         * weird case. See @bugref{6043}.
         */
        if (   rc == VINF_SUCCESS
            || rc == VERR_PAGE_TABLE_NOT_PRESENT
            || rc == VERR_PAGE_NOT_PRESENT)
        {
            Log2(("PGMR0Trap0eHandlerNPMisconfig(,,,%RGp) at %RGv -> resume\n", GCPhys, (RTGCPTR)pCtx->rip));
            goto ResumeExecution;
        }

        Log2(("PGMR0Trap0eHandlerNPMisconfig(,,,%RGp) at %RGv -> %Rrc\n", GCPhys, (RTGCPTR)pCtx->rip, VBOXSTRICTRC_VAL(rc)));
        break;
    }

    case VMX_EXIT_IRQ_WINDOW:           /* 7 Interrupt window. */
        /* Clear VM-exit on IF=1 change. */
        LogFlow(("VMX_EXIT_IRQ_WINDOW %RGv pending=%d IF=%d\n", (RTGCPTR)pCtx->rip,
                 VMCPU_FF_ISPENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC|VMCPU_FF_INTERRUPT_PIC)), pCtx->eflags.Bits.u1IF));
        pVCpu->hwaccm.s.vmx.proc_ctls &= ~VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_IRQ_WINDOW_EXIT;
        rc2 = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
        AssertRC(rc2);
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIrqWindow);
        goto ResumeExecution;   /* we check for pending guest interrupts there */

    case VMX_EXIT_WBINVD:               /* 54 Guest software attempted to execute WBINVD. (conditional) */
    case VMX_EXIT_INVD:                 /* 13 Guest software attempted to execute INVD. (unconditional) */
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitInvd);
        /* Skip instruction and continue directly. */
        pCtx->rip += cbInstr;
        /* Continue execution.*/
        goto ResumeExecution;

    case VMX_EXIT_CPUID:                /* 10 Guest software attempted to execute CPUID. */
    {
        Log2(("VMX: Cpuid %x\n", pCtx->eax));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitCpuid);
        rc = EMInterpretCpuId(pVM, pVCpu, CPUMCTX2CORE(pCtx));
        if (rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            Assert(cbInstr == 2);
            pCtx->rip += cbInstr;
            goto ResumeExecution;
        }
        AssertMsgFailed(("EMU: cpuid failed with %Rrc\n", VBOXSTRICTRC_VAL(rc)));
        rc = VINF_EM_RAW_EMULATE_INSTR;
        break;
    }

    case VMX_EXIT_RDPMC:                /* 15 Guest software attempted to execute RDPMC. */
    {
        Log2(("VMX: Rdpmc %x\n", pCtx->ecx));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitRdpmc);
        rc = EMInterpretRdpmc(pVM, pVCpu, CPUMCTX2CORE(pCtx));
        if (rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            Assert(cbInstr == 2);
            pCtx->rip += cbInstr;
            goto ResumeExecution;
        }
        rc = VINF_EM_RAW_EMULATE_INSTR;
        break;
    }

    case VMX_EXIT_RDTSC:                /* 16 Guest software attempted to execute RDTSC. */
    {
        Log2(("VMX: Rdtsc\n"));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitRdtsc);
        rc = EMInterpretRdtsc(pVM, pVCpu, CPUMCTX2CORE(pCtx));
        if (rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            Assert(cbInstr == 2);
            pCtx->rip += cbInstr;
            goto ResumeExecution;
        }
        rc = VINF_EM_RAW_EMULATE_INSTR;
        break;
    }

    case VMX_EXIT_INVPG:                /* 14 Guest software attempted to execute INVPG. */
    {
        Log2(("VMX: invlpg\n"));
        Assert(!pVM->hwaccm.s.fNestedPaging);

        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitInvpg);
        rc = EMInterpretInvlpg(pVM, pVCpu, CPUMCTX2CORE(pCtx), exitQualification);
        if (rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            pCtx->rip += cbInstr;
            goto ResumeExecution;
        }
        AssertMsg(rc == VERR_EM_INTERPRETER, ("EMU: invlpg %RGv failed with %Rrc\n", exitQualification, VBOXSTRICTRC_VAL(rc)));
        break;
    }

    case VMX_EXIT_MONITOR:              /* 39 Guest software attempted to execute MONITOR. */
    {
        Log2(("VMX: monitor\n"));

        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitMonitor);
        rc = EMInterpretMonitor(pVM, pVCpu, CPUMCTX2CORE(pCtx));
        if (rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            pCtx->rip += cbInstr;
            goto ResumeExecution;
        }
        AssertMsg(rc == VERR_EM_INTERPRETER, ("EMU: monitor failed with %Rrc\n", VBOXSTRICTRC_VAL(rc)));
        break;
    }

    case VMX_EXIT_WRMSR:                /* 32 WRMSR. Guest software attempted to execute WRMSR. */
        /* When an interrupt is pending, we'll let MSR_K8_LSTAR writes fault in our TPR patch code. */
        if (    pVM->hwaccm.s.fTPRPatchingActive
            &&  pCtx->ecx == MSR_K8_LSTAR)
        {
            Assert(!CPUMIsGuestInLongModeEx(pCtx));
            if ((pCtx->eax & 0xff) != u8LastTPR)
            {
                Log(("VMX: Faulting MSR_K8_LSTAR write with new TPR value %x\n", pCtx->eax & 0xff));

                /* Our patch code uses LSTAR for TPR caching. */
                rc2 = PDMApicSetTPR(pVCpu, pCtx->eax & 0xff);
                AssertRC(rc2);
            }

            /* Skip the instruction and continue. */
            pCtx->rip += cbInstr;     /* wrmsr = [0F 30] */

            /* Only resume if successful. */
            goto ResumeExecution;
        }
        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_MSR;
        /* no break */
    case VMX_EXIT_RDMSR:                /* 31 RDMSR. Guest software attempted to execute RDMSR. */
    {
        STAM_COUNTER_INC((exitReason == VMX_EXIT_RDMSR) ? &pVCpu->hwaccm.s.StatExitRdmsr : &pVCpu->hwaccm.s.StatExitWrmsr);

        /*
         * Note: The Intel spec. claims there's an REX version of RDMSR that's slightly different,
         * so we play safe by completely disassembling the instruction.
         */
        Log2(("VMX: %s\n", (exitReason == VMX_EXIT_RDMSR) ? "rdmsr" : "wrmsr"));
        rc = EMInterpretInstruction(pVCpu, CPUMCTX2CORE(pCtx), 0);
        if (rc == VINF_SUCCESS)
        {
            /* EIP has been updated already. */
            /* Only resume if successful. */
            goto ResumeExecution;
        }
        AssertMsg(rc == VERR_EM_INTERPRETER, ("EMU: %s failed with %Rrc\n",
                                              (exitReason == VMX_EXIT_RDMSR) ? "rdmsr" : "wrmsr", VBOXSTRICTRC_VAL(rc)));
        break;
    }

    case VMX_EXIT_CRX_MOVE:             /* 28 Control-register accesses. */
    {
        STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatExit2Sub2, y2);

        switch (VMX_EXIT_QUALIFICATION_CRX_ACCESS(exitQualification))
        {
            case VMX_EXIT_QUALIFICATION_CRX_ACCESS_WRITE:
            {
                Log2(("VMX: %RGv mov cr%d, x\n", (RTGCPTR)pCtx->rip, VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification)));
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitCRxWrite[VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification)]);
                rc = EMInterpretCRxWrite(pVM, pVCpu, CPUMCTX2CORE(pCtx),
                                         VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification),
                                         VMX_EXIT_QUALIFICATION_CRX_GENREG(exitQualification));
                switch (VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification))
                {
                    case 0:
                        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0 | HWACCM_CHANGED_GUEST_CR3;
                        break;
                    case 2:
                        break;
                    case 3:
                        Assert(!pVM->hwaccm.s.fNestedPaging || !CPUMIsGuestInPagedProtectedModeEx(pCtx));
                        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR3;
                        break;
                    case 4:
                        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR4;
                        break;
                    case 8:
                        /* CR8 contains the APIC TPR */
                        Assert(!(pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1
                                 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW));
                        break;

                    default:
                        AssertFailed();
                        break;
                }
                break;
            }

            case VMX_EXIT_QUALIFICATION_CRX_ACCESS_READ:
            {
                Log2(("VMX: mov x, crx\n"));
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitCRxRead[VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification)]);

                Assert(   !pVM->hwaccm.s.fNestedPaging
                       || !CPUMIsGuestInPagedProtectedModeEx(pCtx)
                       || VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification) != USE_REG_CR3);

                /* CR8 reads only cause an exit when the TPR shadow feature isn't present. */
                Assert(   VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification) != 8
                       || !(pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW));

                rc = EMInterpretCRxRead(pVM, pVCpu, CPUMCTX2CORE(pCtx),
                                        VMX_EXIT_QUALIFICATION_CRX_GENREG(exitQualification),
                                        VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification));
                break;
            }

            case VMX_EXIT_QUALIFICATION_CRX_ACCESS_CLTS:
            {
                Log2(("VMX: clts\n"));
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitCLTS);
                rc = EMInterpretCLTS(pVM, pVCpu);
                pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0;
                break;
            }

            case VMX_EXIT_QUALIFICATION_CRX_ACCESS_LMSW:
            {
                Log2(("VMX: lmsw %x\n", VMX_EXIT_QUALIFICATION_CRX_LMSW_DATA(exitQualification)));
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitLMSW);
                rc = EMInterpretLMSW(pVM, pVCpu, CPUMCTX2CORE(pCtx), VMX_EXIT_QUALIFICATION_CRX_LMSW_DATA(exitQualification));
                pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0;
                break;
            }
        }

        /* Update EIP if no error occurred. */
        if (RT_SUCCESS(rc))
            pCtx->rip += cbInstr;

        if (rc == VINF_SUCCESS)
        {
            /* Only resume if successful. */
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub2, y2);
            goto ResumeExecution;
        }
        Assert(rc == VERR_EM_INTERPRETER || rc == VINF_PGM_CHANGE_MODE || rc == VINF_PGM_SYNC_CR3);
        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub2, y2);
        break;
    }

    case VMX_EXIT_DRX_MOVE:             /* 29 Debug-register accesses. */
    {
        if (    !DBGFIsStepping(pVCpu)
            &&  !CPUMIsHyperDebugStateActive(pVCpu))
        {
            /* Disable DRx move intercepts. */
            pVCpu->hwaccm.s.vmx.proc_ctls &= ~VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT;
            rc2 = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
            AssertRC(rc2);

            /* Save the host and load the guest debug state. */
            rc2 = CPUMR0LoadGuestDebugState(pVM, pVCpu, pCtx, true /* include DR6 */);
            AssertRC(rc2);

#ifdef LOG_ENABLED
            if (VMX_EXIT_QUALIFICATION_DRX_DIRECTION(exitQualification) == VMX_EXIT_QUALIFICATION_DRX_DIRECTION_WRITE)
            {
                Log(("VMX_EXIT_DRX_MOVE: write DR%d genreg %d\n", VMX_EXIT_QUALIFICATION_DRX_REGISTER(exitQualification),
                     VMX_EXIT_QUALIFICATION_DRX_GENREG(exitQualification)));
            }
            else
                Log(("VMX_EXIT_DRX_MOVE: read DR%d\n", VMX_EXIT_QUALIFICATION_DRX_REGISTER(exitQualification)));
#endif

#ifdef VBOX_WITH_STATISTICS
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatDRxContextSwitch);
            if (VMX_EXIT_QUALIFICATION_DRX_DIRECTION(exitQualification) == VMX_EXIT_QUALIFICATION_DRX_DIRECTION_WRITE)
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitDRxWrite);
            else
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitDRxRead);
#endif

            goto ResumeExecution;
        }

        /** @todo clear VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT after the first
         *        time and restore DRx registers afterwards */
        if (VMX_EXIT_QUALIFICATION_DRX_DIRECTION(exitQualification) == VMX_EXIT_QUALIFICATION_DRX_DIRECTION_WRITE)
        {
            Log2(("VMX: mov DRx%d, genreg%d\n", VMX_EXIT_QUALIFICATION_DRX_REGISTER(exitQualification),
                  VMX_EXIT_QUALIFICATION_DRX_GENREG(exitQualification)));
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitDRxWrite);
            rc = EMInterpretDRxWrite(pVM, pVCpu, CPUMCTX2CORE(pCtx),
                                     VMX_EXIT_QUALIFICATION_DRX_REGISTER(exitQualification),
                                     VMX_EXIT_QUALIFICATION_DRX_GENREG(exitQualification));
            pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_DEBUG;
            Log2(("DR7=%08x\n", pCtx->dr[7]));
        }
        else
        {
            Log2(("VMX: mov x, DRx\n"));
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitDRxRead);
            rc = EMInterpretDRxRead(pVM, pVCpu, CPUMCTX2CORE(pCtx),
                                    VMX_EXIT_QUALIFICATION_DRX_GENREG(exitQualification),
                                    VMX_EXIT_QUALIFICATION_DRX_REGISTER(exitQualification));
        }
        /* Update EIP if no error occurred. */
        if (RT_SUCCESS(rc))
            pCtx->rip += cbInstr;

        if (rc == VINF_SUCCESS)
        {
            /* Only resume if successful. */
            goto ResumeExecution;
        }
        Assert(rc == VERR_EM_INTERPRETER);
        break;
    }

    /* Note: We'll get a #GP if the IO instruction isn't allowed (IOPL or TSS bitmap); no need to double check. */
    case VMX_EXIT_PORT_IO:              /* 30 I/O instruction. */
    {
        STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatExit2Sub1, y1);
        uint32_t uPort;
        uint32_t uIOWidth = VMX_EXIT_QUALIFICATION_IO_WIDTH(exitQualification);
        bool     fIOWrite = (VMX_EXIT_QUALIFICATION_IO_DIRECTION(exitQualification) == VMX_EXIT_QUALIFICATION_IO_DIRECTION_OUT);

        /** @todo necessary to make the distinction? */
        if (VMX_EXIT_QUALIFICATION_IO_ENCODING(exitQualification) == VMX_EXIT_QUALIFICATION_IO_ENCODING_DX)
            uPort = pCtx->edx & 0xffff;
        else
            uPort = VMX_EXIT_QUALIFICATION_IO_PORT(exitQualification);  /* Immediate encoding. */

        if (RT_UNLIKELY(uIOWidth == 2 || uIOWidth >= 4))         /* paranoia */
        {
            rc = fIOWrite ? VINF_IOM_R3_IOPORT_WRITE : VINF_IOM_R3_IOPORT_READ;
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub1, y1);
            break;
        }

        uint32_t cbSize = g_aIOSize[uIOWidth];
        if (VMX_EXIT_QUALIFICATION_IO_STRING(exitQualification))
        {
            /* ins/outs */
            PDISCPUSTATE pDis = &pVCpu->hwaccm.s.DisState;

            /* Disassemble manually to deal with segment prefixes. */
            /** @todo VMX_VMCS_EXIT_GUEST_LINEAR_ADDR contains the flat pointer operand of the instruction. */
            /** @todo VMX_VMCS32_RO_EXIT_INSTR_INFO also contains segment prefix info. */
            rc2 = EMInterpretDisasOne(pVM, pVCpu, CPUMCTX2CORE(pCtx), pDis, NULL);
            if (RT_SUCCESS(rc))
            {
                if (fIOWrite)
                {
                    Log2(("IOMInterpretOUTSEx %RGv %x size=%d\n", (RTGCPTR)pCtx->rip, uPort, cbSize));
                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIOStringWrite);
                    rc = IOMInterpretOUTSEx(pVM, CPUMCTX2CORE(pCtx), uPort, pDis->prefix, pDis->addrmode, cbSize);
                }
                else
                {
                    Log2(("IOMInterpretINSEx  %RGv %x size=%d\n", (RTGCPTR)pCtx->rip, uPort, cbSize));
                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIOStringRead);
                    rc = IOMInterpretINSEx(pVM, CPUMCTX2CORE(pCtx), uPort, pDis->prefix, pDis->addrmode, cbSize);
                }
            }
            else
                rc = VINF_EM_RAW_EMULATE_INSTR;
        }
        else
        {
            /* Normal in/out */
            uint32_t uAndVal = g_aIOOpAnd[uIOWidth];

            Assert(!VMX_EXIT_QUALIFICATION_IO_REP(exitQualification));

            if (fIOWrite)
            {
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIOWrite);
                rc = IOMIOPortWrite(pVM, uPort, pCtx->eax & uAndVal, cbSize);
                if (rc == VINF_IOM_R3_IOPORT_WRITE)
                    HWACCMR0SavePendingIOPortWrite(pVCpu, pCtx->rip, pCtx->rip + cbInstr, uPort, uAndVal, cbSize);
            }
            else
            {
                uint32_t u32Val = 0;

                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIORead);
                rc = IOMIOPortRead(pVM, uPort, &u32Val, cbSize);
                if (IOM_SUCCESS(rc))
                {
                    /* Write back to the EAX register. */
                    pCtx->eax = (pCtx->eax & ~uAndVal) | (u32Val & uAndVal);
                }
                else
                if (rc == VINF_IOM_R3_IOPORT_READ)
                    HWACCMR0SavePendingIOPortRead(pVCpu, pCtx->rip, pCtx->rip + cbInstr, uPort, uAndVal, cbSize);
            }
        }

        /*
         * Handled the I/O return codes.
         * (The unhandled cases end up with rc == VINF_EM_RAW_EMULATE_INSTR.)
         */
        if (IOM_SUCCESS(rc))
        {
            /* Update EIP and continue execution. */
            pCtx->rip += cbInstr;
            if (RT_LIKELY(rc == VINF_SUCCESS))
            {
                /* If any IO breakpoints are armed, then we should check if a debug trap needs to be generated. */
                if (pCtx->dr[7] & X86_DR7_ENABLED_MASK)
                {
                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatDRxIOCheck);
                    for (unsigned i = 0; i < 4; i++)
                    {
                        unsigned uBPLen = g_aIOSize[X86_DR7_GET_LEN(pCtx->dr[7], i)];

                        if (    (uPort >= pCtx->dr[i] && uPort < pCtx->dr[i] + uBPLen)
                            &&  (pCtx->dr[7] & (X86_DR7_L(i) | X86_DR7_G(i)))
                            &&  (pCtx->dr[7] & X86_DR7_RW(i, X86_DR7_RW_IO)) == X86_DR7_RW(i, X86_DR7_RW_IO))
                        {
                            uint64_t uDR6;

                            Assert(CPUMIsGuestDebugStateActive(pVCpu));

                            uDR6 = ASMGetDR6();

                            /* Clear all breakpoint status flags and set the one we just hit. */
                            uDR6 &= ~(X86_DR6_B0|X86_DR6_B1|X86_DR6_B2|X86_DR6_B3);
                            uDR6 |= (uint64_t)RT_BIT(i);

                            /*
                             * Note: AMD64 Architecture Programmer's Manual 13.1:
                             * Bits 15:13 of the DR6 register is never cleared by the processor and must
                             * be cleared by software after the contents have been read.
                             */
                            ASMSetDR6(uDR6);

                            /* X86_DR7_GD will be cleared if DRx accesses should be trapped inside the guest. */
                            pCtx->dr[7] &= ~X86_DR7_GD;

                            /* Paranoia. */
                            pCtx->dr[7] &= 0xffffffff;                                              /* upper 32 bits reserved */
                            pCtx->dr[7] &= ~(RT_BIT(11) | RT_BIT(12) | RT_BIT(14) | RT_BIT(15));    /* must be zero */
                            pCtx->dr[7] |= 0x400;                                                   /* must be one */

                            /* Resync DR7 */
                            rc2 = VMXWriteVMCS64(VMX_VMCS64_GUEST_DR7, pCtx->dr[7]);
                            AssertRC(rc2);

                            /* Construct inject info. */
                            intInfo  = X86_XCPT_DB;
                            intInfo |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
                            intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HWEXCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

                            Log(("Inject IO debug trap at %RGv\n", (RTGCPTR)pCtx->rip));
                            rc2 = hmR0VmxInjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo),
                                                     0 /* cbInstr */, 0 /* errCode */);
                            AssertRC(rc2);

                            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub1, y1);
                            goto ResumeExecution;
                        }
                    }
                }
                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub1, y1);
                goto ResumeExecution;
            }
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub1, y1);
            break;
        }

#ifdef VBOX_STRICT
        if (rc == VINF_IOM_R3_IOPORT_READ)
            Assert(!fIOWrite);
        else if (rc == VINF_IOM_R3_IOPORT_WRITE)
            Assert(fIOWrite);
        else
        {
            AssertMsg(   RT_FAILURE(rc)
                      || rc == VINF_EM_RAW_EMULATE_INSTR
                      || rc == VINF_EM_RAW_GUEST_TRAP
                      || rc == VINF_TRPM_XCPT_DISPATCHED, ("%Rrc\n", VBOXSTRICTRC_VAL(rc)));
        }
#endif
        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub1, y1);
        break;
    }

    case VMX_EXIT_TPR:                  /* 43 TPR below threshold. Guest software executed MOV to CR8. */
        LogFlow(("VMX_EXIT_TPR\n"));
        /* RIP is already set to the next instruction and the TPR has been synced back. Just resume. */
        goto ResumeExecution;

    case VMX_EXIT_APIC_ACCESS:          /* 44 APIC access. Guest software attempted to access memory at a physical address
                                                            on the APIC-access page. */
    {
        LogFlow(("VMX_EXIT_APIC_ACCESS\n"));
        unsigned uAccessType = VMX_EXIT_QUALIFICATION_APIC_ACCESS_TYPE(exitQualification);

        switch(uAccessType)
        {
            case VMX_APIC_ACCESS_TYPE_LINEAR_READ:
            case VMX_APIC_ACCESS_TYPE_LINEAR_WRITE:
            {
                RTGCPHYS GCPhys;
                PDMApicGetBase(pVM, &GCPhys);
                GCPhys &= PAGE_BASE_GC_MASK;
                GCPhys += VMX_EXIT_QUALIFICATION_APIC_ACCESS_OFFSET(exitQualification);

                LogFlow(("Apic access at %RGp\n", GCPhys));
                rc = IOMMMIOPhysHandler(pVM, (uAccessType == VMX_APIC_ACCESS_TYPE_LINEAR_READ) ? 0 : X86_TRAP_PF_RW,
                                        CPUMCTX2CORE(pCtx), GCPhys);
                if (rc == VINF_SUCCESS)
                    goto ResumeExecution;   /* rip already updated */
                break;
            }

            default:
                rc = VINF_EM_RAW_EMULATE_INSTR;
                break;
        }
        break;
    }

    case VMX_EXIT_PREEMPTION_TIMER:     /* 52 VMX-preemption timer expired. The preemption timer counted down to zero. */
        if (!TMTimerPollBool(pVM, pVCpu))
            goto ResumeExecution;
        rc = VINF_EM_RAW_TIMER_PENDING;
        break;

    default:
        /* The rest is handled after syncing the entire CPU state. */
        break;
    }


    /*
     * Note: The guest state is not entirely synced back at this stage!
     */

    /* Investigate why there was a VM-exit. (part 2) */
    switch (exitReason)
    {
    case VMX_EXIT_EXCEPTION:            /* 0 Exception or non-maskable interrupt (NMI). */
    case VMX_EXIT_EXTERNAL_IRQ:         /* 1 External interrupt. */
    case VMX_EXIT_EPT_VIOLATION:
    case VMX_EXIT_EPT_MISCONFIG:        /* 49 EPT misconfig is used by the PGM/MMIO optimizations. */
    case VMX_EXIT_PREEMPTION_TIMER:     /* 52 VMX-preemption timer expired. The preemption timer counted down to zero. */
        /* Already handled above. */
        break;

    case VMX_EXIT_TRIPLE_FAULT:         /* 2 Triple fault. */
        rc = VINF_EM_RESET;             /* Triple fault equals a reset. */
        break;

    case VMX_EXIT_INIT_SIGNAL:          /* 3 INIT signal. */
    case VMX_EXIT_SIPI:                 /* 4 Start-up IPI (SIPI). */
        rc = VINF_EM_RAW_INTERRUPT;
        AssertFailed();                 /* Can't happen. Yet. */
        break;

    case VMX_EXIT_IO_SMI_IRQ:           /* 5 I/O system-management interrupt (SMI). */
    case VMX_EXIT_SMI_IRQ:              /* 6 Other SMI. */
        rc = VINF_EM_RAW_INTERRUPT;
        AssertFailed();                 /* Can't happen afaik. */
        break;

    case VMX_EXIT_TASK_SWITCH:          /* 9 Task switch: too complicated to emulate, so fall back to the recompiler */
        Log(("VMX_EXIT_TASK_SWITCH: exit=%RX64\n", exitQualification));
        if (    (VMX_EXIT_QUALIFICATION_TASK_SWITCH_TYPE(exitQualification) == VMX_EXIT_QUALIFICATION_TASK_SWITCH_TYPE_IDT)
            &&  pVCpu->hwaccm.s.Event.fPending)
        {
            /* Caused by an injected interrupt. */
            pVCpu->hwaccm.s.Event.fPending = false;

            Log(("VMX_EXIT_TASK_SWITCH: reassert trap %d\n", VMX_EXIT_INTERRUPTION_INFO_VECTOR(pVCpu->hwaccm.s.Event.intInfo)));
            Assert(!VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_IS_VALID(pVCpu->hwaccm.s.Event.intInfo));
            rc2 = TRPMAssertTrap(pVCpu, VMX_EXIT_INTERRUPTION_INFO_VECTOR(pVCpu->hwaccm.s.Event.intInfo), TRPM_HARDWARE_INT);
            AssertRC(rc2);
        }
        /* else Exceptions and software interrupts can just be restarted. */
        rc = VERR_EM_INTERPRETER;
        break;

    case VMX_EXIT_HLT:                  /* 12 Guest software attempted to execute HLT. */
        /* Check if external interrupts are pending; if so, don't switch back. */
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitHlt);
        pCtx->rip++;    /* skip hlt */
        if (EMShouldContinueAfterHalt(pVCpu, pCtx))
            goto ResumeExecution;

        rc = VINF_EM_HALT;
        break;

    case VMX_EXIT_MWAIT:                /* 36 Guest software executed MWAIT. */
        Log2(("VMX: mwait\n"));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitMwait);
        rc = EMInterpretMWait(pVM, pVCpu, CPUMCTX2CORE(pCtx));
        if (    rc == VINF_EM_HALT
            ||  rc == VINF_SUCCESS)
        {
            /* Update EIP and continue execution. */
            pCtx->rip += cbInstr;

            /* Check if external interrupts are pending; if so, don't switch back. */
            if (    rc == VINF_SUCCESS
                ||  (   rc == VINF_EM_HALT
                     && EMShouldContinueAfterHalt(pVCpu, pCtx))
               )
                goto ResumeExecution;
        }
        AssertMsg(rc == VERR_EM_INTERPRETER || rc == VINF_EM_HALT, ("EMU: mwait failed with %Rrc\n", VBOXSTRICTRC_VAL(rc)));
        break;

    case VMX_EXIT_RSM:                  /* 17 Guest software attempted to execute RSM in SMM. */
        AssertFailed(); /* can't happen. */
        rc = VERR_EM_INTERPRETER;
        break;

    case VMX_EXIT_MTF:                  /* 37 Exit due to Monitor Trap Flag. */
        LogFlow(("VMX_EXIT_MTF at %RGv\n", (RTGCPTR)pCtx->rip));
        pVCpu->hwaccm.s.vmx.proc_ctls &= ~VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG;
        rc2 = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
        AssertRC(rc2);
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitMTF);
#if 0
        DBGFDoneStepping(pVCpu);
#endif
        rc = VINF_EM_DBG_STOP;
        break;

    case VMX_EXIT_VMCALL:               /* 18 Guest software executed VMCALL. */
    case VMX_EXIT_VMCLEAR:              /* 19 Guest software executed VMCLEAR. */
    case VMX_EXIT_VMLAUNCH:             /* 20 Guest software executed VMLAUNCH. */
    case VMX_EXIT_VMPTRLD:              /* 21 Guest software executed VMPTRLD. */
    case VMX_EXIT_VMPTRST:              /* 22 Guest software executed VMPTRST. */
    case VMX_EXIT_VMREAD:               /* 23 Guest software executed VMREAD. */
    case VMX_EXIT_VMRESUME:             /* 24 Guest software executed VMRESUME. */
    case VMX_EXIT_VMWRITE:              /* 25 Guest software executed VMWRITE. */
    case VMX_EXIT_VMXOFF:               /* 26 Guest software executed VMXOFF. */
    case VMX_EXIT_VMXON:                /* 27 Guest software executed VMXON. */
        /** @todo inject #UD immediately */
        rc = VERR_EM_INTERPRETER;
        break;

    case VMX_EXIT_CPUID:                /* 10 Guest software attempted to execute CPUID. */
    case VMX_EXIT_RDTSC:                /* 16 Guest software attempted to execute RDTSC. */
    case VMX_EXIT_INVPG:                /* 14 Guest software attempted to execute INVPG. */
    case VMX_EXIT_CRX_MOVE:             /* 28 Control-register accesses. */
    case VMX_EXIT_DRX_MOVE:             /* 29 Debug-register accesses. */
    case VMX_EXIT_PORT_IO:              /* 30 I/O instruction. */
    case VMX_EXIT_RDPMC:                /* 15 Guest software attempted to execute RDPMC. */
        /* already handled above */
        AssertMsg(   rc == VINF_PGM_CHANGE_MODE
                  || rc == VINF_EM_RAW_INTERRUPT
                  || rc == VERR_EM_INTERPRETER
                  || rc == VINF_EM_RAW_EMULATE_INSTR
                  || rc == VINF_PGM_SYNC_CR3
                  || rc == VINF_IOM_R3_IOPORT_READ
                  || rc == VINF_IOM_R3_IOPORT_WRITE
                  || rc == VINF_EM_RAW_GUEST_TRAP
                  || rc == VINF_TRPM_XCPT_DISPATCHED
                  || rc == VINF_EM_RESCHEDULE_REM,
                  ("rc = %d\n", VBOXSTRICTRC_VAL(rc)));
        break;

    case VMX_EXIT_TPR:                  /* 43 TPR below threshold. Guest software executed MOV to CR8. */
    case VMX_EXIT_RDMSR:                /* 31 RDMSR. Guest software attempted to execute RDMSR. */
    case VMX_EXIT_WRMSR:                /* 32 WRMSR. Guest software attempted to execute WRMSR. */
    case VMX_EXIT_PAUSE:                /* 40 Guest software attempted to execute PAUSE. */
    case VMX_EXIT_MONITOR:              /* 39 Guest software attempted to execute MONITOR. */
    case VMX_EXIT_APIC_ACCESS:          /* 44 APIC access. Guest software attempted to access memory at a physical address
                                                        on the APIC-access page. */
    {
        /*
         * If we decided to emulate them here, then we must sync the MSRs that could have been changed (sysenter, FS/GS base)
         */
        rc = VERR_EM_INTERPRETER;
        break;
    }

    case VMX_EXIT_IRQ_WINDOW:           /* 7 Interrupt window. */
        Assert(rc == VINF_EM_RAW_INTERRUPT);
        break;

    case VMX_EXIT_ERR_INVALID_GUEST_STATE:  /* 33 VM-entry failure due to invalid guest state. */
    {
#ifdef VBOX_STRICT
        RTCCUINTREG val2 = 0;

        Log(("VMX_EXIT_ERR_INVALID_GUEST_STATE\n"));

        VMXReadVMCS(VMX_VMCS64_GUEST_RIP, &val2);
        Log(("Old eip %RGv new %RGv\n", (RTGCPTR)pCtx->rip, (RTGCPTR)val2));

        VMXReadVMCS(VMX_VMCS64_GUEST_CR0, &val2);
        Log(("VMX_VMCS_GUEST_CR0        %RX64\n", (uint64_t)val2));

        VMXReadVMCS(VMX_VMCS64_GUEST_CR3, &val2);
        Log(("VMX_VMCS_GUEST_CR3        %RX64\n", (uint64_t)val2));

        VMXReadVMCS(VMX_VMCS64_GUEST_CR4, &val2);
        Log(("VMX_VMCS_GUEST_CR4        %RX64\n", (uint64_t)val2));

        VMXReadVMCS(VMX_VMCS_GUEST_RFLAGS, &val2);
        Log(("VMX_VMCS_GUEST_RFLAGS     %08x\n", val2));

        VMX_LOG_SELREG(CS,   "CS",   val2);
        VMX_LOG_SELREG(DS,   "DS",   val2);
        VMX_LOG_SELREG(ES,   "ES",   val2);
        VMX_LOG_SELREG(FS,   "FS",   val2);
        VMX_LOG_SELREG(GS,   "GS",   val2);
        VMX_LOG_SELREG(SS,   "SS",   val2);
        VMX_LOG_SELREG(TR,   "TR",   val2);
        VMX_LOG_SELREG(LDTR, "LDTR", val2);

        VMXReadVMCS(VMX_VMCS64_GUEST_GDTR_BASE, &val2);
        Log(("VMX_VMCS_GUEST_GDTR_BASE    %RX64\n", (uint64_t)val2));
        VMXReadVMCS(VMX_VMCS64_GUEST_IDTR_BASE, &val2);
        Log(("VMX_VMCS_GUEST_IDTR_BASE    %RX64\n", (uint64_t)val2));
#endif /* VBOX_STRICT */
        rc = VERR_VMX_INVALID_GUEST_STATE;
        break;
    }

    case VMX_EXIT_ERR_MSR_LOAD:         /* 34 VM-entry failure due to MSR loading. */
    case VMX_EXIT_ERR_MACHINE_CHECK:    /* 41 VM-entry failure due to machine-check. */
    default:
        rc = VERR_VMX_UNEXPECTED_EXIT_CODE;
        AssertMsgFailed(("Unexpected exit code %d\n", exitReason));                 /* Can't happen. */
        break;

    }

end:
    /* We now going back to ring-3, so clear the action flag. */
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TO_R3);

    /*
     * Signal changes for the recompiler.
     */
    CPUMSetChangedFlags(pVCpu,
                          CPUM_CHANGED_SYSENTER_MSR
                        | CPUM_CHANGED_LDTR
                        | CPUM_CHANGED_GDTR
                        | CPUM_CHANGED_IDTR
                        | CPUM_CHANGED_TR
                        | CPUM_CHANGED_HIDDEN_SEL_REGS);

    /*
     * If we executed vmlaunch/vmresume and an external IRQ was pending, then we don't have to do a full sync the next time.
     */
    if (    exitReason == VMX_EXIT_EXTERNAL_IRQ
        &&  !VMX_EXIT_INTERRUPTION_INFO_VALID(intInfo))
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

    /* Translate into a less severe return code */
    if (rc == VERR_EM_INTERPRETER)
        rc = VINF_EM_RAW_EMULATE_INSTR;
    else if (rc == VERR_VMX_INVALID_VMCS_PTR)
    {
        /* Try to extract more information about what might have gone wrong here. */
        VMXGetActivateVMCS(&pVCpu->hwaccm.s.vmx.lasterror.u64VMCSPhys);
        pVCpu->hwaccm.s.vmx.lasterror.ulVMCSRevision = *(uint32_t *)pVCpu->hwaccm.s.vmx.pvVMCS;
        pVCpu->hwaccm.s.vmx.lasterror.idEnteredCpu   = pVCpu->hwaccm.s.idEnteredCpu;
        pVCpu->hwaccm.s.vmx.lasterror.idCurrentCpu   = RTMpCpuId();
    }

    /* Just set the correct state here instead of trying to catch every goto above. */
    VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC);

#ifdef VBOX_WITH_VMMR0_DISABLE_PREEMPTION
    /* Restore interrupts if we exited after disabling them. */
    if (uOldEFlags != ~(RTCCUINTREG)0)
        ASMSetFlags(uOldEFlags);
#endif

    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2, x);
    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, x);
    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatEntry, x);
    Log2(("X"));
    return VBOXSTRICTRC_TODO(rc);
}


/**
 * Enters the VT-x session.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCpu        Pointer to the CPU info struct.
 */
VMMR0DECL(int) VMXR0Enter(PVM pVM, PVMCPU pVCpu, PHMGLOBLCPUINFO pCpu)
{
    Assert(pVM->hwaccm.s.vmx.fSupported);
    NOREF(pCpu);

    unsigned cr4 = ASMGetCR4();
    if (!(cr4 & X86_CR4_VMXE))
    {
        AssertMsgFailed(("X86_CR4_VMXE should be set!\n"));
        return VERR_VMX_X86_CR4_VMXE_CLEARED;
    }

    /* Activate the VMCS. */
    int rc = VMXActivateVMCS(pVCpu->hwaccm.s.vmx.HCPhysVMCS);
    if (RT_FAILURE(rc))
        return rc;

    pVCpu->hwaccm.s.fResumeVM = false;
    return VINF_SUCCESS;
}


/**
 * Leaves the VT-x session.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guests CPU context.
 */
VMMR0DECL(int) VMXR0Leave(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    Assert(pVM->hwaccm.s.vmx.fSupported);

#ifdef DEBUG
    if (CPUMIsHyperDebugStateActive(pVCpu))
    {
        CPUMR0LoadHostDebugState(pVM, pVCpu);
        Assert(pVCpu->hwaccm.s.vmx.proc_ctls & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT);
    }
    else
#endif

    /*
     * Save the guest debug state if necessary.
     */
    if (CPUMIsGuestDebugStateActive(pVCpu))
    {
        CPUMR0SaveGuestDebugState(pVM, pVCpu, pCtx, true /* save DR6 */);

        /* Enable DRx move intercepts again. */
        pVCpu->hwaccm.s.vmx.proc_ctls |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT;
        int rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
        AssertRC(rc);

        /* Resync the debug registers the next time. */
        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_DEBUG;
    }
    else
        Assert(pVCpu->hwaccm.s.vmx.proc_ctls & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT);

    /*
     * Clear VMCS, marking it inactive, clearing implementation-specific data and writing
     * VMCS data back to memory.
     */
    int rc = VMXClearVMCS(pVCpu->hwaccm.s.vmx.HCPhysVMCS);
    AssertRC(rc);

    return VINF_SUCCESS;
}


/**
 * Flush the TLB using EPT.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   enmFlush    Type of flush.
 */
static void hmR0VmxFlushEPT(PVM pVM, PVMCPU pVCpu, VMX_FLUSH_EPT enmFlush)
{
    uint64_t descriptor[2];

    LogFlow(("hmR0VmxFlushEPT %d\n", enmFlush));
    Assert(pVM->hwaccm.s.fNestedPaging);
    descriptor[0] = pVCpu->hwaccm.s.vmx.GCPhysEPTP;
    descriptor[1] = 0; /* MBZ. Intel spec. 33.3 VMX Instructions */
    int rc = VMXR0InvEPT(enmFlush, &descriptor[0]);
    AssertMsg(rc == VINF_SUCCESS, ("VMXR0InvEPT %x %RGv failed with %d\n", enmFlush, pVCpu->hwaccm.s.vmx.GCPhysEPTP, rc));
}


/**
 * Flush the TLB using VPID.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU (can be NULL depending on @a
 *                      enmFlush).
 * @param   enmFlush    Type of flush.
 * @param   GCPtr       Virtual address of the page to flush (can be 0 depending
 *                      on @a enmFlush).
 */
static void hmR0VmxFlushVPID(PVM pVM, PVMCPU pVCpu, VMX_FLUSH_VPID enmFlush, RTGCPTR GCPtr)
{
#if HC_ARCH_BITS == 32
    /*
     * If we get a flush in 64-bit guest mode, then force a full TLB flush. invvpid probably takes only 32-bit addresses.
     */
    if (   CPUMIsGuestInLongMode(pVCpu)
        && !VMX_IS_64BIT_HOST_MODE())
    {
        VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
    }
    else
#endif
    {
        uint64_t descriptor[2];

        Assert(pVM->hwaccm.s.vmx.fVPID);
        if (enmFlush == VMX_FLUSH_VPID_ALL_CONTEXTS)
        {
            descriptor[0] = 0;
            descriptor[1] = 0;
        }
        else
        {
            AssertPtr(pVCpu);
            Assert(pVCpu->hwaccm.s.uCurrentASID != 0);
            descriptor[0] = pVCpu->hwaccm.s.uCurrentASID;
            descriptor[1] = GCPtr;
        }
        int rc = VMXR0InvVPID(enmFlush, &descriptor[0]); NOREF(rc);
        AssertMsg(rc == VINF_SUCCESS,
                  ("VMXR0InvVPID %x %x %RGv failed with %d\n", enmFlush, pVCpu ? pVCpu->hwaccm.s.uCurrentASID : 0, GCPtr, rc));
    }
}


/**
 * Invalidates a guest page by guest virtual address. Only relevant for
 * EPT/VPID, otherwise there is nothing really to invalidate.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   GCVirt      Guest virtual address of the page to invalidate.
 */
VMMR0DECL(int) VMXR0InvalidatePage(PVM pVM, PVMCPU pVCpu, RTGCPTR GCVirt)
{
    bool fFlushPending = VMCPU_FF_ISSET(pVCpu, VMCPU_FF_TLB_FLUSH);

    Log2(("VMXR0InvalidatePage %RGv\n", GCVirt));

    if (!fFlushPending)
    {
        /*
         * We must invalidate the guest TLB entry in either case, we cannot ignore it even for the EPT case
         * See @bugref{6043} and @bugref{6177}
         *
         * Set the VMCPU_FF_TLB_FLUSH force flag and flush before VMENTRY in hmR0VmxSetupTLB*() as this
         * function maybe called in a loop with individual addresses.
         */
        if (pVM->hwaccm.s.vmx.fVPID)
        {
            /* If we can flush just this page do it, otherwise flush as little as possible. */
            if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_INDIV_ADDR)
                hmR0VmxFlushVPID(pVM, pVCpu, VMX_FLUSH_VPID_INDIV_ADDR, GCVirt);
            else
                VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
        }
        else if (pVM->hwaccm.s.fNestedPaging)
            VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
    }

    return VINF_SUCCESS;
}


/**
 * Invalidates a guest page by physical address. Only relevant for EPT/VPID,
 * otherwise there is nothing really to invalidate.
 *
 * NOTE: Assumes the current instruction references this physical page though a virtual address!!
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   GCPhys      Guest physical address of the page to invalidate.
 */
VMMR0DECL(int) VMXR0InvalidatePhysPage(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys)
{
    LogFlow(("VMXR0InvalidatePhysPage %RGp\n", GCPhys));

    /*
     * We cannot flush a page by guest-physical address. invvpid takes only a linear address
     * while invept only flushes by EPT not individual addresses. We update the force flag here
     * and flush before VMENTRY in hmR0VmxSetupTLB*(). This function might be called in a loop.
     */
    VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
    return VINF_SUCCESS;
}


/**
 * Report world switch error and dump some useful debug info.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   rc          Return code.
 * @param   pCtx        Pointer to the current guest CPU context (not updated).
 */
static void hmR0VmxReportWorldSwitchError(PVM pVM, PVMCPU pVCpu, VBOXSTRICTRC rc, PCPUMCTX pCtx)
{
    NOREF(pVM);

    switch (VBOXSTRICTRC_VAL(rc))
    {
        case VERR_VMX_INVALID_VMXON_PTR:
            AssertFailed();
            break;

        case VERR_VMX_UNABLE_TO_START_VM:
        case VERR_VMX_UNABLE_TO_RESUME_VM:
        {
            int         rc2;
            RTCCUINTREG exitReason, instrError;

            rc2  = VMXReadVMCS(VMX_VMCS32_RO_EXIT_REASON, &exitReason);
            rc2 |= VMXReadVMCS(VMX_VMCS32_RO_VM_INSTR_ERROR, &instrError);
            AssertRC(rc2);
            if (rc2 == VINF_SUCCESS)
            {
                Log(("Unable to start/resume VM for reason: %x. Instruction error %x\n", (uint32_t)exitReason,
                     (uint32_t)instrError));
                Log(("Current stack %08x\n", &rc2));

                pVCpu->hwaccm.s.vmx.lasterror.ulInstrError = instrError;
                pVCpu->hwaccm.s.vmx.lasterror.ulExitReason = exitReason;

#ifdef VBOX_STRICT
                RTGDTR      gdtr;
                PCX86DESCHC pDesc;
                RTCCUINTREG val;

                ASMGetGDTR(&gdtr);

                VMXReadVMCS(VMX_VMCS64_GUEST_RIP, &val);
                Log(("Old eip %RGv new %RGv\n", (RTGCPTR)pCtx->rip, (RTGCPTR)val));
                VMXReadVMCS(VMX_VMCS_CTRL_PIN_EXEC_CONTROLS,    &val);
                Log(("VMX_VMCS_CTRL_PIN_EXEC_CONTROLS   %08x\n", val));
                VMXReadVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS,   &val);
                Log(("VMX_VMCS_CTRL_PROC_EXEC_CONTROLS  %08x\n", val));
                VMXReadVMCS(VMX_VMCS_CTRL_ENTRY_CONTROLS,       &val);
                Log(("VMX_VMCS_CTRL_ENTRY_CONTROLS      %08x\n", val));
                VMXReadVMCS(VMX_VMCS_CTRL_EXIT_CONTROLS,        &val);
                Log(("VMX_VMCS_CTRL_EXIT_CONTROLS       %08x\n", val));

                VMXReadVMCS(VMX_VMCS_HOST_CR0,  &val);
                Log(("VMX_VMCS_HOST_CR0 %08x\n", val));
                VMXReadVMCS(VMX_VMCS_HOST_CR3,  &val);
                Log(("VMX_VMCS_HOST_CR3 %08x\n", val));
                VMXReadVMCS(VMX_VMCS_HOST_CR4,  &val);
                Log(("VMX_VMCS_HOST_CR4 %08x\n", val));

                VMXReadVMCS(VMX_VMCS16_HOST_FIELD_CS, &val);
                Log(("VMX_VMCS_HOST_FIELD_CS %08x\n",  val));
                VMXReadVMCS(VMX_VMCS_GUEST_RFLAGS,  &val);
                Log(("VMX_VMCS_GUEST_RFLAGS %08x\n", val));

                if (val < gdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(gdtr.pGdt + (val & X86_SEL_MASK));
                    HWACCMR0DumpDescriptor(pDesc, val, "CS: ");
                }

                VMXReadVMCS(VMX_VMCS16_HOST_FIELD_DS, &val);
                Log(("VMX_VMCS_HOST_FIELD_DS %08x\n",  val));
                if (val < gdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(gdtr.pGdt + (val & X86_SEL_MASK));
                    HWACCMR0DumpDescriptor(pDesc, val, "DS: ");
                }

                VMXReadVMCS(VMX_VMCS16_HOST_FIELD_ES, &val);
                Log(("VMX_VMCS_HOST_FIELD_ES %08x\n",  val));
                if (val < gdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(gdtr.pGdt + (val & X86_SEL_MASK));
                    HWACCMR0DumpDescriptor(pDesc, val, "ES: ");
                }

                VMXReadVMCS(VMX_VMCS16_HOST_FIELD_FS, &val);
                Log(("VMX_VMCS16_HOST_FIELD_FS %08x\n", val));
                if (val < gdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(gdtr.pGdt + (val & X86_SEL_MASK));
                    HWACCMR0DumpDescriptor(pDesc, val, "FS: ");
                }

                VMXReadVMCS(VMX_VMCS16_HOST_FIELD_GS,  &val);
                Log(("VMX_VMCS16_HOST_FIELD_GS %08x\n", val));
                if (val < gdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(gdtr.pGdt + (val & X86_SEL_MASK));
                    HWACCMR0DumpDescriptor(pDesc, val, "GS: ");
                }

                VMXReadVMCS(VMX_VMCS16_HOST_FIELD_SS,  &val);
                Log(("VMX_VMCS16_HOST_FIELD_SS %08x\n", val));
                if (val < gdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(gdtr.pGdt + (val & X86_SEL_MASK));
                    HWACCMR0DumpDescriptor(pDesc, val, "SS: ");
                }

                VMXReadVMCS(VMX_VMCS16_HOST_FIELD_TR,  &val);
                Log(("VMX_VMCS16_HOST_FIELD_TR %08x\n", val));
                if (val < gdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(gdtr.pGdt + (val & X86_SEL_MASK));
                    HWACCMR0DumpDescriptor(pDesc, val, "TR: ");
                }

                VMXReadVMCS(VMX_VMCS_HOST_TR_BASE,         &val);
                Log(("VMX_VMCS_HOST_TR_BASE %RHv\n",        val));
                VMXReadVMCS(VMX_VMCS_HOST_GDTR_BASE,       &val);
                Log(("VMX_VMCS_HOST_GDTR_BASE %RHv\n",      val));
                VMXReadVMCS(VMX_VMCS_HOST_IDTR_BASE,       &val);
                Log(("VMX_VMCS_HOST_IDTR_BASE %RHv\n",      val));
                VMXReadVMCS(VMX_VMCS32_HOST_SYSENTER_CS,   &val);
                Log(("VMX_VMCS_HOST_SYSENTER_CS  %08x\n",   val));
                VMXReadVMCS(VMX_VMCS_HOST_SYSENTER_EIP,    &val);
                Log(("VMX_VMCS_HOST_SYSENTER_EIP %RHv\n",   val));
                VMXReadVMCS(VMX_VMCS_HOST_SYSENTER_ESP,    &val);
                Log(("VMX_VMCS_HOST_SYSENTER_ESP %RHv\n",   val));
                VMXReadVMCS(VMX_VMCS_HOST_RSP,             &val);
                Log(("VMX_VMCS_HOST_RSP %RHv\n",            val));
                VMXReadVMCS(VMX_VMCS_HOST_RIP,             &val);
                Log(("VMX_VMCS_HOST_RIP %RHv\n",            val));
# if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
                if (VMX_IS_64BIT_HOST_MODE())
                {
                    Log(("MSR_K6_EFER       = %RX64\n", ASMRdMsr(MSR_K6_EFER)));
                    Log(("MSR_K6_STAR       = %RX64\n", ASMRdMsr(MSR_K6_STAR)));
                    Log(("MSR_K8_LSTAR      = %RX64\n", ASMRdMsr(MSR_K8_LSTAR)));
                    Log(("MSR_K8_CSTAR      = %RX64\n", ASMRdMsr(MSR_K8_CSTAR)));
                    Log(("MSR_K8_SF_MASK    = %RX64\n", ASMRdMsr(MSR_K8_SF_MASK)));
                }
# endif
#endif /* VBOX_STRICT */
            }
            break;
        }

        default:
            /* impossible */
            AssertMsgFailed(("%Rrc (%#x)\n", VBOXSTRICTRC_VAL(rc), VBOXSTRICTRC_VAL(rc)));
            break;
    }
}


#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
/**
 * Prepares for and executes VMLAUNCH (64 bits guest mode).
 *
 * @returns VBox status code.
 * @param   fResume     Whether to vmlauch/vmresume.
 * @param   pCtx        Pointer to the guest CPU context.
 * @param   pCache      Pointer to the VMCS cache.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
DECLASM(int) VMXR0SwitcherStartVM64(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu)
{
    uint32_t        aParam[6];
    PHMGLOBLCPUINFO pCpu;
    RTHCPHYS        HCPhysCpuPage;
    int             rc;

    pCpu = HWACCMR0GetCurrentCpu();
    HCPhysCpuPage = RTR0MemObjGetPagePhysAddr(pCpu->hMemObj, 0);

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    pCache->uPos = 1;
    pCache->interPD = PGMGetInterPaeCR3(pVM);
    pCache->pSwitcher = (uint64_t)pVM->hwaccm.s.pfnHost32ToGuest64R0;
#endif

#ifdef DEBUG
    pCache->TestIn.HCPhysCpuPage= 0;
    pCache->TestIn.HCPhysVMCS   = 0;
    pCache->TestIn.pCache       = 0;
    pCache->TestOut.HCPhysVMCS  = 0;
    pCache->TestOut.pCache      = 0;
    pCache->TestOut.pCtx        = 0;
    pCache->TestOut.eflags      = 0;
#endif

    aParam[0] = (uint32_t)(HCPhysCpuPage);                                  /* Param 1: VMXON physical address - Lo. */
    aParam[1] = (uint32_t)(HCPhysCpuPage >> 32);                            /* Param 1: VMXON physical address - Hi. */
    aParam[2] = (uint32_t)(pVCpu->hwaccm.s.vmx.HCPhysVMCS);                 /* Param 2: VMCS physical address - Lo. */
    aParam[3] = (uint32_t)(pVCpu->hwaccm.s.vmx.HCPhysVMCS >> 32);           /* Param 2: VMCS physical address - Hi. */
    aParam[4] = VM_RC_ADDR(pVM, &pVM->aCpus[pVCpu->idCpu].hwaccm.s.vmx.VMCSCache);
    aParam[5] = 0;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    pCtx->dr[4] = pVM->hwaccm.s.vmx.pScratchPhys + 16 + 8;
    *(uint32_t *)(pVM->hwaccm.s.vmx.pScratch + 16 + 8) = 1;
#endif
    rc = VMXR0Execute64BitsHandler(pVM, pVCpu, pCtx, pVM->hwaccm.s.pfnVMXGCStartVM64, 6, &aParam[0]);

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    Assert(*(uint32_t *)(pVM->hwaccm.s.vmx.pScratch + 16 + 8) == 5);
    Assert(pCtx->dr[4] == 10);
    *(uint32_t *)(pVM->hwaccm.s.vmx.pScratch + 16 + 8) = 0xff;
#endif

#ifdef DEBUG
    AssertMsg(pCache->TestIn.HCPhysCpuPage== HCPhysCpuPage, ("%RHp vs %RHp\n", pCache->TestIn.HCPhysCpuPage, HCPhysCpuPage));
    AssertMsg(pCache->TestIn.HCPhysVMCS   == pVCpu->hwaccm.s.vmx.HCPhysVMCS, ("%RHp vs %RHp\n", pCache->TestIn.HCPhysVMCS,
                                                                              pVCpu->hwaccm.s.vmx.HCPhysVMCS));
    AssertMsg(pCache->TestIn.HCPhysVMCS   == pCache->TestOut.HCPhysVMCS, ("%RHp vs %RHp\n", pCache->TestIn.HCPhysVMCS,
                                                                          pCache->TestOut.HCPhysVMCS));
    AssertMsg(pCache->TestIn.pCache       == pCache->TestOut.pCache, ("%RGv vs %RGv\n", pCache->TestIn.pCache,
                                                                      pCache->TestOut.pCache));
    AssertMsg(pCache->TestIn.pCache       == VM_RC_ADDR(pVM, &pVM->aCpus[pVCpu->idCpu].hwaccm.s.vmx.VMCSCache),
              ("%RGv vs %RGv\n", pCache->TestIn.pCache, VM_RC_ADDR(pVM, &pVM->aCpus[pVCpu->idCpu].hwaccm.s.vmx.VMCSCache)));
    AssertMsg(pCache->TestIn.pCtx         == pCache->TestOut.pCtx, ("%RGv vs %RGv\n", pCache->TestIn.pCtx,
                                                                    pCache->TestOut.pCtx));
    Assert(!(pCache->TestOut.eflags & X86_EFL_IF));
#endif
    return rc;
}


# ifdef VBOX_STRICT
static bool hmR0VmxIsValidReadField(uint32_t idxField)
{
    switch (idxField)
    {
        case VMX_VMCS64_GUEST_RIP:
        case VMX_VMCS64_GUEST_RSP:
        case VMX_VMCS_GUEST_RFLAGS:
        case VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE:
        case VMX_VMCS_CTRL_CR0_READ_SHADOW:
        case VMX_VMCS64_GUEST_CR0:
        case VMX_VMCS_CTRL_CR4_READ_SHADOW:
        case VMX_VMCS64_GUEST_CR4:
        case VMX_VMCS64_GUEST_DR7:
        case VMX_VMCS32_GUEST_SYSENTER_CS:
        case VMX_VMCS64_GUEST_SYSENTER_EIP:
        case VMX_VMCS64_GUEST_SYSENTER_ESP:
        case VMX_VMCS32_GUEST_GDTR_LIMIT:
        case VMX_VMCS64_GUEST_GDTR_BASE:
        case VMX_VMCS32_GUEST_IDTR_LIMIT:
        case VMX_VMCS64_GUEST_IDTR_BASE:
        case VMX_VMCS16_GUEST_FIELD_CS:
        case VMX_VMCS32_GUEST_CS_LIMIT:
        case VMX_VMCS64_GUEST_CS_BASE:
        case VMX_VMCS32_GUEST_CS_ACCESS_RIGHTS:
        case VMX_VMCS16_GUEST_FIELD_DS:
        case VMX_VMCS32_GUEST_DS_LIMIT:
        case VMX_VMCS64_GUEST_DS_BASE:
        case VMX_VMCS32_GUEST_DS_ACCESS_RIGHTS:
        case VMX_VMCS16_GUEST_FIELD_ES:
        case VMX_VMCS32_GUEST_ES_LIMIT:
        case VMX_VMCS64_GUEST_ES_BASE:
        case VMX_VMCS32_GUEST_ES_ACCESS_RIGHTS:
        case VMX_VMCS16_GUEST_FIELD_FS:
        case VMX_VMCS32_GUEST_FS_LIMIT:
        case VMX_VMCS64_GUEST_FS_BASE:
        case VMX_VMCS32_GUEST_FS_ACCESS_RIGHTS:
        case VMX_VMCS16_GUEST_FIELD_GS:
        case VMX_VMCS32_GUEST_GS_LIMIT:
        case VMX_VMCS64_GUEST_GS_BASE:
        case VMX_VMCS32_GUEST_GS_ACCESS_RIGHTS:
        case VMX_VMCS16_GUEST_FIELD_SS:
        case VMX_VMCS32_GUEST_SS_LIMIT:
        case VMX_VMCS64_GUEST_SS_BASE:
        case VMX_VMCS32_GUEST_SS_ACCESS_RIGHTS:
        case VMX_VMCS16_GUEST_FIELD_LDTR:
        case VMX_VMCS32_GUEST_LDTR_LIMIT:
        case VMX_VMCS64_GUEST_LDTR_BASE:
        case VMX_VMCS32_GUEST_LDTR_ACCESS_RIGHTS:
        case VMX_VMCS16_GUEST_FIELD_TR:
        case VMX_VMCS32_GUEST_TR_LIMIT:
        case VMX_VMCS64_GUEST_TR_BASE:
        case VMX_VMCS32_GUEST_TR_ACCESS_RIGHTS:
        case VMX_VMCS32_RO_EXIT_REASON:
        case VMX_VMCS32_RO_VM_INSTR_ERROR:
        case VMX_VMCS32_RO_EXIT_INSTR_LENGTH:
        case VMX_VMCS32_RO_EXIT_INTERRUPTION_ERRCODE:
        case VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO:
        case VMX_VMCS32_RO_EXIT_INSTR_INFO:
        case VMX_VMCS_RO_EXIT_QUALIFICATION:
        case VMX_VMCS32_RO_IDT_INFO:
        case VMX_VMCS32_RO_IDT_ERRCODE:
        case VMX_VMCS64_GUEST_CR3:
        case VMX_VMCS_EXIT_PHYS_ADDR_FULL:
            return true;
    }
    return false;
}


static bool hmR0VmxIsValidWriteField(uint32_t idxField)
{
    switch (idxField)
    {
        case VMX_VMCS64_GUEST_LDTR_BASE:
        case VMX_VMCS64_GUEST_TR_BASE:
        case VMX_VMCS64_GUEST_GDTR_BASE:
        case VMX_VMCS64_GUEST_IDTR_BASE:
        case VMX_VMCS64_GUEST_SYSENTER_EIP:
        case VMX_VMCS64_GUEST_SYSENTER_ESP:
        case VMX_VMCS64_GUEST_CR0:
        case VMX_VMCS64_GUEST_CR4:
        case VMX_VMCS64_GUEST_CR3:
        case VMX_VMCS64_GUEST_DR7:
        case VMX_VMCS64_GUEST_RIP:
        case VMX_VMCS64_GUEST_RSP:
        case VMX_VMCS64_GUEST_CS_BASE:
        case VMX_VMCS64_GUEST_DS_BASE:
        case VMX_VMCS64_GUEST_ES_BASE:
        case VMX_VMCS64_GUEST_FS_BASE:
        case VMX_VMCS64_GUEST_GS_BASE:
        case VMX_VMCS64_GUEST_SS_BASE:
            return true;
    }
    return false;
}
# endif /* VBOX_STRICT */


/**
 * Executes the specified handler in 64-bit mode.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 * @param   pfnHandler  Pointer to the RC handler function.
 * @param   cbParam     Number of parameters.
 * @param   paParam     Array of 32-bit parameters.
 */
VMMR0DECL(int) VMXR0Execute64BitsHandler(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, RTRCPTR pfnHandler, uint32_t cbParam,
                                         uint32_t *paParam)
{
    int             rc, rc2;
    PHMGLOBLCPUINFO pCpu;
    RTHCPHYS        HCPhysCpuPage;
    RTHCUINTREG     uOldEFlags;

    AssertReturn(pVM->hwaccm.s.pfnHost32ToGuest64R0, VERR_HM_NO_32_TO_64_SWITCHER);
    Assert(pfnHandler);
    Assert(pVCpu->hwaccm.s.vmx.VMCSCache.Write.cValidEntries <= RT_ELEMENTS(pVCpu->hwaccm.s.vmx.VMCSCache.Write.aField));
    Assert(pVCpu->hwaccm.s.vmx.VMCSCache.Read.cValidEntries <= RT_ELEMENTS(pVCpu->hwaccm.s.vmx.VMCSCache.Read.aField));

#ifdef VBOX_STRICT
    for (unsigned i=0;i<pVCpu->hwaccm.s.vmx.VMCSCache.Write.cValidEntries;i++)
        Assert(hmR0VmxIsValidWriteField(pVCpu->hwaccm.s.vmx.VMCSCache.Write.aField[i]));

    for (unsigned i=0;i<pVCpu->hwaccm.s.vmx.VMCSCache.Read.cValidEntries;i++)
        Assert(hmR0VmxIsValidReadField(pVCpu->hwaccm.s.vmx.VMCSCache.Read.aField[i]));
#endif

    /* Disable interrupts. */
    uOldEFlags = ASMIntDisableFlags();

    pCpu = HWACCMR0GetCurrentCpu();
    HCPhysCpuPage = RTR0MemObjGetPagePhysAddr(pCpu->hMemObj, 0);

    /* Clear VMCS. Marking it inactive, clearing implementation-specific data and writing VMCS data back to memory. */
    VMXClearVMCS(pVCpu->hwaccm.s.vmx.HCPhysVMCS);

    /* Leave VMX Root Mode. */
    VMXDisable();

    ASMSetCR4(ASMGetCR4() & ~X86_CR4_VMXE);

    CPUMSetHyperESP(pVCpu, VMMGetStackRC(pVCpu));
    CPUMSetHyperEIP(pVCpu, pfnHandler);
    for (int i=(int)cbParam-1;i>=0;i--)
        CPUMPushHyper(pVCpu, paParam[i]);

    STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatWorldSwitch3264, z);

    /* Call switcher. */
    rc = pVM->hwaccm.s.pfnHost32ToGuest64R0(pVM, RT_OFFSETOF(VM, aCpus[pVCpu->idCpu].cpum) - RT_OFFSETOF(VM, cpum));
    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatWorldSwitch3264, z);

    /* Make sure the VMX instructions don't cause #UD faults. */
    ASMSetCR4(ASMGetCR4() | X86_CR4_VMXE);

    /* Enter VMX Root Mode */
    rc2 = VMXEnable(HCPhysCpuPage);
    if (RT_FAILURE(rc2))
    {
        ASMSetCR4(ASMGetCR4() & ~X86_CR4_VMXE);
        ASMSetFlags(uOldEFlags);
        return VERR_VMX_VMXON_FAILED;
    }

    rc2 = VMXActivateVMCS(pVCpu->hwaccm.s.vmx.HCPhysVMCS);
    AssertRC(rc2);
    Assert(!(ASMGetFlags() & X86_EFL_IF));
    ASMSetFlags(uOldEFlags);
    return rc;
}
#endif /* HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL) */


#if HC_ARCH_BITS == 32 && !defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
/**
 * Executes VMWRITE.
 *
 * @returns VBox status code
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   idxField        VMCS field index.
 * @param   u64Val          16, 32 or 64 bits value.
 */
VMMR0DECL(int) VMXWriteVMCS64Ex(PVMCPU pVCpu, uint32_t idxField, uint64_t u64Val)
{
    int rc;
    switch (idxField)
    {
        case VMX_VMCS_CTRL_TSC_OFFSET_FULL:
        case VMX_VMCS_CTRL_IO_BITMAP_A_FULL:
        case VMX_VMCS_CTRL_IO_BITMAP_B_FULL:
        case VMX_VMCS_CTRL_MSR_BITMAP_FULL:
        case VMX_VMCS_CTRL_VMEXIT_MSR_STORE_FULL:
        case VMX_VMCS_CTRL_VMEXIT_MSR_LOAD_FULL:
        case VMX_VMCS_CTRL_VMENTRY_MSR_LOAD_FULL:
        case VMX_VMCS_CTRL_VAPIC_PAGEADDR_FULL:
        case VMX_VMCS_CTRL_APIC_ACCESSADDR_FULL:
        case VMX_VMCS_GUEST_LINK_PTR_FULL:
        case VMX_VMCS_GUEST_PDPTR0_FULL:
        case VMX_VMCS_GUEST_PDPTR1_FULL:
        case VMX_VMCS_GUEST_PDPTR2_FULL:
        case VMX_VMCS_GUEST_PDPTR3_FULL:
        case VMX_VMCS_GUEST_DEBUGCTL_FULL:
        case VMX_VMCS_GUEST_EFER_FULL:
        case VMX_VMCS_CTRL_EPTP_FULL:
            /* These fields consist of two parts, which are both writable in 32 bits mode. */
            rc  = VMXWriteVMCS32(idxField, u64Val);
            rc |= VMXWriteVMCS32(idxField + 1, (uint32_t)(u64Val >> 32ULL));
            AssertRC(rc);
            return rc;

        case VMX_VMCS64_GUEST_LDTR_BASE:
        case VMX_VMCS64_GUEST_TR_BASE:
        case VMX_VMCS64_GUEST_GDTR_BASE:
        case VMX_VMCS64_GUEST_IDTR_BASE:
        case VMX_VMCS64_GUEST_SYSENTER_EIP:
        case VMX_VMCS64_GUEST_SYSENTER_ESP:
        case VMX_VMCS64_GUEST_CR0:
        case VMX_VMCS64_GUEST_CR4:
        case VMX_VMCS64_GUEST_CR3:
        case VMX_VMCS64_GUEST_DR7:
        case VMX_VMCS64_GUEST_RIP:
        case VMX_VMCS64_GUEST_RSP:
        case VMX_VMCS64_GUEST_CS_BASE:
        case VMX_VMCS64_GUEST_DS_BASE:
        case VMX_VMCS64_GUEST_ES_BASE:
        case VMX_VMCS64_GUEST_FS_BASE:
        case VMX_VMCS64_GUEST_GS_BASE:
        case VMX_VMCS64_GUEST_SS_BASE:
            /* Queue a 64 bits value as we can't set it in 32 bits host mode. */
            if (u64Val >> 32ULL)
                rc = VMXWriteCachedVMCSEx(pVCpu, idxField, u64Val);
            else
                rc = VMXWriteVMCS32(idxField, (uint32_t)u64Val);

            return rc;

        default:
            AssertMsgFailed(("Unexpected field %x\n", idxField));
            return VERR_INVALID_PARAMETER;
    }
}


/**
 * Cache VMCS writes for performance reasons (Darwin) and for running 64 bits guests on 32 bits hosts.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idxField    VMCS field index.
 * @param   u64Val      16, 32 or 64 bits value.
 */
VMMR0DECL(int) VMXWriteCachedVMCSEx(PVMCPU pVCpu, uint32_t idxField, uint64_t u64Val)
{
    PVMCSCACHE pCache = &pVCpu->hwaccm.s.vmx.VMCSCache;

    AssertMsgReturn(pCache->Write.cValidEntries < VMCSCACHE_MAX_ENTRY - 1,
                    ("entries=%x\n", pCache->Write.cValidEntries), VERR_ACCESS_DENIED);

    /* Make sure there are no duplicates. */
    for (unsigned i = 0; i < pCache->Write.cValidEntries; i++)
    {
        if (pCache->Write.aField[i] == idxField)
        {
            pCache->Write.aFieldVal[i] = u64Val;
            return VINF_SUCCESS;
        }
    }

    pCache->Write.aField[pCache->Write.cValidEntries]    = idxField;
    pCache->Write.aFieldVal[pCache->Write.cValidEntries] = u64Val;
    pCache->Write.cValidEntries++;
    return VINF_SUCCESS;
}

#endif /* HC_ARCH_BITS == 32 && !VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0 */

