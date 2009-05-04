/* $Id$ */
/** @file
 * HWACCM VMX - Host Context Ring 0.
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
#include <VBox/pgm.h>
#include <VBox/pdm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/selm.h>
#include <VBox/iom.h>
#include <VBox/rem.h>
#include <iprt/param.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include "HWVMXR0.h"

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
static void VMXR0ReportWorldSwitchError(PVM pVM, PVMCPU pVCpu, int rc, PCPUMCTX pCtx);
static void vmxR0SetupTLBEPT(PVM pVM, PVMCPU pVCpu);
static void vmxR0SetupTLBVPID(PVM pVM, PVMCPU pVCpu);
static void vmxR0SetupTLBDummy(PVM pVM, PVMCPU pVCpu);
static void vmxR0FlushEPT(PVM pVM, PVMCPU pVCpu, VMX_FLUSH enmFlush, RTGCPHYS GCPhys);
static void vmxR0FlushVPID(PVM pVM, PVMCPU pVCpu, VMX_FLUSH enmFlush, RTGCPTR GCPtr);
static void vmxR0UpdateExceptionBitmap(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
#ifdef VBOX_STRICT
static bool vmxR0IsValidReadField(uint32_t idxField);
static bool vmxR0IsValidWriteField(uint32_t idxField);
#endif

static void VMXR0CheckError(PVM pVM, PVMCPU pVCpu, int rc)
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
 * Sets up and activates VT-x on the current CPU
 *
 * @returns VBox status code.
 * @param   pCpu            CPU info struct
 * @param   pVM             The VM to operate on. (can be NULL after a resume!!)
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
VMMR0DECL(int) VMXR0EnableCpu(PHWACCM_CPUINFO pCpu, PVM pVM, void *pvPageCpu, RTHCPHYS pPageCpuPhys)
{
    AssertReturn(pPageCpuPhys, VERR_INVALID_PARAMETER);
    AssertReturn(pvPageCpu, VERR_INVALID_PARAMETER);

#if defined(LOG_ENABLED) && !defined(DEBUG_bird)
    SUPR0Printf("VMXR0EnableCpu cpu %d page (%x) %x\n", pCpu->idCpu, pvPageCpu, (uint32_t)pPageCpuPhys);
#endif
    if (pVM)
    {
        /* Set revision dword at the beginning of the VMXON structure. */
        *(uint32_t *)pvPageCpu = MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hwaccm.s.vmx.msr.vmx_basic_info);
    }

    /** @todo we should unmap the two pages from the virtual address space in order to prevent accidental corruption.
     * (which can have very bad consequences!!!)
     */

    /* Make sure the VMX instructions don't cause #UD faults. */
    ASMSetCR4(ASMGetCR4() | X86_CR4_VMXE);

    /* Enter VMX Root Mode */
    int rc = VMXEnable(pPageCpuPhys);
    if (RT_FAILURE(rc))
    {
        if (pVM)
            VMXR0CheckError(pVM, &pVM->aCpus[0], rc);
        ASMSetCR4(ASMGetCR4() & ~X86_CR4_VMXE);
        return VERR_VMX_VMXON_FAILED;
    }
    return VINF_SUCCESS;
}

/**
 * Deactivates VT-x on the current CPU
 *
 * @returns VBox status code.
 * @param   pCpu            CPU info struct
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
VMMR0DECL(int) VMXR0DisableCpu(PHWACCM_CPUINFO pCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys)
{
    AssertReturn(pPageCpuPhys, VERR_INVALID_PARAMETER);
    AssertReturn(pvPageCpu, VERR_INVALID_PARAMETER);

    /* Leave VMX Root Mode. */
    VMXDisable();

    /* And clear the X86_CR4_VMXE bit */
    ASMSetCR4(ASMGetCR4() & ~X86_CR4_VMXE);

#if defined(LOG_ENABLED) && !defined(DEBUG_bird)
    SUPR0Printf("VMXR0DisableCpu cpu %d\n", pCpu->idCpu);
#endif
    return VINF_SUCCESS;
}

/**
 * Does Ring-0 per VM VT-x init.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
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
        /* Allocate one page for the virtual APIC mmio cache. */
        rc = RTR0MemObjAllocCont(&pVM->hwaccm.s.vmx.pMemObjAPIC, 1 << PAGE_SHIFT, true /* executable R0 mapping */);
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

    /* Allocate the MSR bitmap if this feature is supported. */
    if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS)
    {
        rc = RTR0MemObjAllocCont(&pVM->hwaccm.s.vmx.pMemObjMSRBitmap, 1 << PAGE_SHIFT, true /* executable R0 mapping */);
        AssertRC(rc);
        if (RT_FAILURE(rc))
            return rc;

        pVM->hwaccm.s.vmx.pMSRBitmap     = (uint8_t *)RTR0MemObjAddress(pVM->hwaccm.s.vmx.pMemObjMSRBitmap);
        pVM->hwaccm.s.vmx.pMSRBitmapPhys = RTR0MemObjGetPagePhysAddr(pVM->hwaccm.s.vmx.pMemObjMSRBitmap, 0);
        memset(pVM->hwaccm.s.vmx.pMSRBitmap, 0xff, PAGE_SIZE);
    }

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    {
        rc = RTR0MemObjAllocCont(&pVM->hwaccm.s.vmx.pMemObjScratch, 1 << PAGE_SHIFT, true /* executable R0 mapping */);
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

    /* Allocate VMCBs for all guest CPUs. */
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        pVCpu->hwaccm.s.vmx.pMemObjVMCS = NIL_RTR0MEMOBJ;

        /* Allocate one page for the VM control structure (VMCS). */
        rc = RTR0MemObjAllocCont(&pVCpu->hwaccm.s.vmx.pMemObjVMCS, 1 << PAGE_SHIFT, true /* executable R0 mapping */);
        AssertRC(rc);
        if (RT_FAILURE(rc))
            return rc;

        pVCpu->hwaccm.s.vmx.pVMCS     = RTR0MemObjAddress(pVCpu->hwaccm.s.vmx.pMemObjVMCS);
        pVCpu->hwaccm.s.vmx.pVMCSPhys = RTR0MemObjGetPagePhysAddr(pVCpu->hwaccm.s.vmx.pMemObjVMCS, 0);
        ASMMemZero32(pVCpu->hwaccm.s.vmx.pVMCS, PAGE_SIZE);

        pVCpu->hwaccm.s.vmx.cr0_mask = 0;
        pVCpu->hwaccm.s.vmx.cr4_mask = 0;

        /* Current guest paging mode. */
        pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode = PGMMODE_REAL;

#ifdef LOG_ENABLED
        SUPR0Printf("VMXR0InitVM %x VMCS=%x (%x)\n", pVM, pVCpu->hwaccm.s.vmx.pVMCS, (uint32_t)pVCpu->hwaccm.s.vmx.pVMCSPhys);
#endif
    }

    return VINF_SUCCESS;
}

/**
 * Does Ring-0 per VM VT-x termination.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) VMXR0TermVM(PVM pVM)
{
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        if (pVM->aCpus[i].hwaccm.s.vmx.pMemObjVMCS != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVM->aCpus[i].hwaccm.s.vmx.pMemObjVMCS, false);
            pVM->aCpus[i].hwaccm.s.vmx.pMemObjVMCS = NIL_RTR0MEMOBJ;
            pVM->aCpus[i].hwaccm.s.vmx.pVMCS       = 0;
            pVM->aCpus[i].hwaccm.s.vmx.pVMCSPhys   = 0;
        }
    }
    if (pVM->hwaccm.s.vmx.pMemObjAPIC != NIL_RTR0MEMOBJ)
    {
        RTR0MemObjFree(pVM->hwaccm.s.vmx.pMemObjAPIC, false);
        pVM->hwaccm.s.vmx.pMemObjAPIC = NIL_RTR0MEMOBJ;
        pVM->hwaccm.s.vmx.pAPIC       = 0;
        pVM->hwaccm.s.vmx.pAPICPhys   = 0;
    }
    if (pVM->hwaccm.s.vmx.pMemObjMSRBitmap != NIL_RTR0MEMOBJ)
    {
        RTR0MemObjFree(pVM->hwaccm.s.vmx.pMemObjMSRBitmap, false);
        pVM->hwaccm.s.vmx.pMemObjMSRBitmap = NIL_RTR0MEMOBJ;
        pVM->hwaccm.s.vmx.pMSRBitmap       = 0;
        pVM->hwaccm.s.vmx.pMSRBitmapPhys   = 0;
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
 * Sets up VT-x for the specified VM
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) VMXR0SetupVM(PVM pVM)
{
    int rc = VINF_SUCCESS;
    uint32_t val;

    AssertReturn(pVM, VERR_INVALID_PARAMETER);

    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        Assert(pVCpu->hwaccm.s.vmx.pVMCS);

        /* Set revision dword at the beginning of the VMCS structure. */
        *(uint32_t *)pVCpu->hwaccm.s.vmx.pVMCS  = MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hwaccm.s.vmx.msr.vmx_basic_info);

        /* Clear VM Control Structure. */
        Log(("pVMCSPhys  = %RHp\n", pVCpu->hwaccm.s.vmx.pVMCSPhys));
        rc = VMXClearVMCS(pVCpu->hwaccm.s.vmx.pVMCSPhys);
        if (RT_FAILURE(rc))
            goto vmx_end;

        /* Activate the VM Control Structure. */
        rc = VMXActivateVMCS(pVCpu->hwaccm.s.vmx.pVMCSPhys);
        if (RT_FAILURE(rc))
            goto vmx_end;

        /* VMX_VMCS_CTRL_PIN_EXEC_CONTROLS
         * Set required bits to one and zero according to the MSR capabilities.
         */
        val  = pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.disallowed0;
        /* External and non-maskable interrupts cause VM-exits. */
        val  = val | VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT | VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT;
        val &= pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.allowed1;

        rc = VMXWriteVMCS(VMX_VMCS_CTRL_PIN_EXEC_CONTROLS, val);
        AssertRC(rc);

        /* VMX_VMCS_CTRL_PROC_EXEC_CONTROLS
         * Set required bits to one and zero according to the MSR capabilities.
         */
        val = pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.disallowed0;
        /* Program which event cause VM-exits and which features we want to use. */
        val = val | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT
                  | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET
                  | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT
                  | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT
                  | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT
                  | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT;    /* don't execute mwait or else we'll idle inside the guest (host thinks the cpu load is high) */

        /* Without nested paging we should intercept invlpg and cr3 mov instructions. */
        if (!pVM->hwaccm.s.fNestedPaging)
            val |=  VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT
                  | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT
                  | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT;

        /* Note: VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT might cause a vmlaunch failure with an invalid control fields error. (combined with some other exit reasons) */
        if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW)
        {
            /* CR8 reads from the APIC shadow page; writes cause an exit is they lower the TPR below the threshold */
            val |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW;
            Assert(pVM->hwaccm.s.vmx.pAPIC);
        }
        else
            /* Exit on CR8 reads & writes in case the TPR shadow feature isn't present. */
            val |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT;

#ifdef VBOX_WITH_VTX_MSR_BITMAPS
        if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS)
        {
            Assert(pVM->hwaccm.s.vmx.pMSRBitmapPhys);
            val |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS;
        }
#endif

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
            /* VMX_VMCS_CTRL_PROC_EXEC_CONTROLS2
             * Set required bits to one and zero according to the MSR capabilities.
             */
            val  = pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.disallowed0;
            val |= VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT;

#ifdef HWACCM_VTX_WITH_EPT
            if (pVM->hwaccm.s.fNestedPaging)
                val |= VMX_VMCS_CTRL_PROC_EXEC2_EPT;
#endif /* HWACCM_VTX_WITH_EPT */
#ifdef HWACCM_VTX_WITH_VPID
            else
            if (pVM->hwaccm.s.vmx.fVPID)
                val |= VMX_VMCS_CTRL_PROC_EXEC2_VPID;
#endif /* HWACCM_VTX_WITH_VPID */

            /* Mask away the bits that the CPU doesn't support */
            /** @todo make sure they don't conflict with the above requirements. */
            val &= pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1;

            rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS2, val);
            AssertRC(rc);
        }

        /* VMX_VMCS_CTRL_CR3_TARGET_COUNT
         * Set required bits to one and zero according to the MSR capabilities.
         */
        rc = VMXWriteVMCS(VMX_VMCS_CTRL_CR3_TARGET_COUNT, 0);
        AssertRC(rc);

        /* Forward all exception except #NM & #PF to the guest.
         * We always need to check pagefaults since our shadow page table can be out of sync.
         * And we always lazily sync the FPU & XMM state.
         */

        /** @todo Possible optimization:
         * Keep the FPU and XMM state current in the EM thread. That way there's no need to
         * lazily sync anything, but the downside is that we can't use the FPU stack or XMM
         * registers ourselves of course.
         *
         * Note: only possible if the current state is actually ours (X86_CR0_TS flag)
         */

        /* Don't filter page faults; all of them should cause a switch. */
        rc  = VMXWriteVMCS(VMX_VMCS_CTRL_PAGEFAULT_ERROR_MASK, 0);
        rc |= VMXWriteVMCS(VMX_VMCS_CTRL_PAGEFAULT_ERROR_MATCH, 0);
        AssertRC(rc);

        /* Init TSC offset to zero. */
        rc = VMXWriteVMCS64(VMX_VMCS_CTRL_TSC_OFFSET_FULL, 0);
        AssertRC(rc);

        rc = VMXWriteVMCS64(VMX_VMCS_CTRL_IO_BITMAP_A_FULL, 0);
        AssertRC(rc);

        rc = VMXWriteVMCS64(VMX_VMCS_CTRL_IO_BITMAP_B_FULL, 0);
        AssertRC(rc);

        /* Set the MSR bitmap address. */
        if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS)
        {
            /* Optional */
            rc = VMXWriteVMCS64(VMX_VMCS_CTRL_MSR_BITMAP_FULL, pVM->hwaccm.s.vmx.pMSRBitmapPhys);
            AssertRC(rc);
        }

        /* Clear MSR controls. */
        rc  = VMXWriteVMCS64(VMX_VMCS_CTRL_VMEXIT_MSR_STORE_FULL, 0);
        rc |= VMXWriteVMCS64(VMX_VMCS_CTRL_VMEXIT_MSR_LOAD_FULL, 0);
        rc |= VMXWriteVMCS64(VMX_VMCS_CTRL_VMENTRY_MSR_LOAD_FULL, 0);
        rc |= VMXWriteVMCS(VMX_VMCS_CTRL_EXIT_MSR_STORE_COUNT, 0);
        rc |= VMXWriteVMCS(VMX_VMCS_CTRL_EXIT_MSR_LOAD_COUNT, 0);
        AssertRC(rc);

        if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW)
        {
            Assert(pVM->hwaccm.s.vmx.pMemObjAPIC);
            /* Optional */
            rc  = VMXWriteVMCS(VMX_VMCS_CTRL_TPR_THRESHOLD, 0);
            rc |= VMXWriteVMCS64(VMX_VMCS_CTRL_VAPIC_PAGEADDR_FULL, pVM->hwaccm.s.vmx.pAPICPhys);
            AssertRC(rc);
        }

        /* Set link pointer to -1. Not currently used. */
        rc = VMXWriteVMCS64(VMX_VMCS_GUEST_LINK_PTR_FULL, 0xFFFFFFFFFFFFFFFFULL);
        AssertRC(rc);

        /* Clear VM Control Structure. Marking it inactive, clearing implementation specific data and writing back VMCS data to memory. */
        rc = VMXClearVMCS(pVCpu->hwaccm.s.vmx.pVMCSPhys);
        AssertRC(rc);

        /* Configure the VMCS read cache. */
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

        VMX_SETUP_SELREG(ES, pCache);
        VMX_SETUP_SELREG(SS, pCache);
        VMX_SETUP_SELREG(CS, pCache);
        VMX_SETUP_SELREG(DS, pCache);
        VMX_SETUP_SELREG(FS, pCache);
        VMX_SETUP_SELREG(GS, pCache);
        VMX_SETUP_SELREG(LDTR, pCache);
        VMX_SETUP_SELREG(TR, pCache);

        /* Status code VMCS reads. */
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

    /* Choose the right TLB setup function. */
    if (pVM->hwaccm.s.fNestedPaging)
    {
        pVM->hwaccm.s.vmx.pfnSetupTaggedTLB = vmxR0SetupTLBEPT;

        /* Default values for flushing. */
        pVM->hwaccm.s.vmx.enmFlushPage    = VMX_FLUSH_ALL_CONTEXTS;
        pVM->hwaccm.s.vmx.enmFlushContext = VMX_FLUSH_ALL_CONTEXTS;

        /* If the capabilities specify we can do more, then make use of it. */
        if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_INDIV)
            pVM->hwaccm.s.vmx.enmFlushPage = VMX_FLUSH_PAGE;
        else
        if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_CONTEXT)
            pVM->hwaccm.s.vmx.enmFlushPage = VMX_FLUSH_SINGLE_CONTEXT;

        if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_CONTEXT)
            pVM->hwaccm.s.vmx.enmFlushContext = VMX_FLUSH_SINGLE_CONTEXT;
    }
#ifdef HWACCM_VTX_WITH_VPID
    else
    if (pVM->hwaccm.s.vmx.fVPID)
    {
        pVM->hwaccm.s.vmx.pfnSetupTaggedTLB = vmxR0SetupTLBVPID;

        /* Default values for flushing. */
        pVM->hwaccm.s.vmx.enmFlushPage    = VMX_FLUSH_ALL_CONTEXTS;
        pVM->hwaccm.s.vmx.enmFlushContext = VMX_FLUSH_ALL_CONTEXTS;

        /* If the capabilities specify we can do more, then make use of it. */
        if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_INDIV)
            pVM->hwaccm.s.vmx.enmFlushPage = VMX_FLUSH_PAGE;
        else
        if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_CONTEXT)
            pVM->hwaccm.s.vmx.enmFlushPage = VMX_FLUSH_SINGLE_CONTEXT;

        if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_CONTEXT)
            pVM->hwaccm.s.vmx.enmFlushContext = VMX_FLUSH_SINGLE_CONTEXT;
    }
#endif /* HWACCM_VTX_WITH_VPID */
    else
        pVM->hwaccm.s.vmx.pfnSetupTaggedTLB = vmxR0SetupTLBDummy;

vmx_end:
    VMXR0CheckError(pVM, &pVM->aCpus[0], rc);
    return rc;
}


/**
 * Injects an event (trap or external interrupt)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        CPU Context
 * @param   intInfo     VMX interrupt info
 * @param   cbInstr     Opcode length of faulting instruction
 * @param   errCode     Error code (optional)
 */
static int VMXR0InjectEvent(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, uint32_t intInfo, uint32_t cbInstr, uint32_t errCode)
{
    int         rc;
    uint32_t    iGate = VMX_EXIT_INTERRUPTION_INFO_VECTOR(intInfo);

#ifdef VBOX_STRICT
    if (iGate == 0xE)
        LogFlow(("VMXR0InjectEvent: Injecting interrupt %d at %RGv error code=%08x CR2=%RGv intInfo=%08x\n", iGate, (RTGCPTR)pCtx->rip, errCode, pCtx->cr2, intInfo));
    else
    if (iGate < 0x20)
        LogFlow(("VMXR0InjectEvent: Injecting interrupt %d at %RGv error code=%08x\n", iGate, (RTGCPTR)pCtx->rip, errCode));
    else
    {
        LogFlow(("INJ-EI: %x at %RGv\n", iGate, (RTGCPTR)pCtx->rip));
        Assert(VMX_EXIT_INTERRUPTION_INFO_TYPE(intInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_SW || !VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));
        Assert(VMX_EXIT_INTERRUPTION_INFO_TYPE(intInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_SW || pCtx->eflags.u32 & X86_EFL_IF);
    }
#endif

#ifdef HWACCM_VMX_EMULATE_REALMODE
    if (CPUMIsGuestInRealModeEx(pCtx))
    {
        RTGCPHYS GCPhysHandler;
        uint16_t offset, ip;
        RTSEL    sel;

        /* Injecting events doesn't work right with real mode emulation.
         * (#GP if we try to inject external hardware interrupts)
         * Inject the interrupt or trap directly instead.
         *
         * ASSUMES no access handlers for the bits we read or write below (should be safe).
         */
        Log(("Manual interrupt/trap '%x' inject (real mode)\n", iGate));

        /* Check if the interrupt handler is present. */
        if (iGate * 4 + 3 > pCtx->idtr.cbIdt)
        {
            Log(("IDT cbIdt violation\n"));
            if (iGate != X86_XCPT_DF)
            {
                RTGCUINTPTR intInfo;

                intInfo  = (iGate == X86_XCPT_GP) ? (uint32_t)X86_XCPT_DF : iGate;
                intInfo |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
                intInfo |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
                intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HWEXCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

                return VMXR0InjectEvent(pVM, pVCpu, pCtx, intInfo, 0, 0 /* no error code according to the Intel docs */);
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

        /* Read the selector:offset pair of the interrupt handler. */
        GCPhysHandler = (RTGCPHYS)pCtx->idtr.pIdt + iGate * 4;
        rc = PGMPhysSimpleReadGCPhys(pVM, &offset, GCPhysHandler,     sizeof(offset)); AssertRC(rc);
        rc = PGMPhysSimpleReadGCPhys(pVM, &sel,    GCPhysHandler + 2, sizeof(sel));    AssertRC(rc);

        LogFlow(("IDT handler %04X:%04X\n", sel, offset));

        /* Construct the stack frame. */
        /** @todo should check stack limit. */
        pCtx->sp -= 2;
        LogFlow(("ss:sp %04X:%04X eflags=%x\n", pCtx->ss, pCtx->sp, pCtx->eflags.u));
        rc = PGMPhysSimpleWriteGCPhys(pVM, pCtx->ssHid.u64Base + pCtx->sp, &pCtx->eflags, sizeof(uint16_t)); AssertRC(rc);
        pCtx->sp -= 2;
        LogFlow(("ss:sp %04X:%04X cs=%x\n", pCtx->ss, pCtx->sp, pCtx->cs));
        rc = PGMPhysSimpleWriteGCPhys(pVM, pCtx->ssHid.u64Base + pCtx->sp, &pCtx->cs, sizeof(uint16_t)); AssertRC(rc);
        pCtx->sp -= 2;
        LogFlow(("ss:sp %04X:%04X ip=%x\n", pCtx->ss, pCtx->sp, ip));
        rc = PGMPhysSimpleWriteGCPhys(pVM, pCtx->ssHid.u64Base + pCtx->sp, &ip, sizeof(ip)); AssertRC(rc);

        /* Update the CPU state for executing the handler. */
        pCtx->rip           = offset;
        pCtx->cs            = sel;
        pCtx->csHid.u64Base = sel << 4;
        pCtx->eflags.u     &= ~(X86_EFL_IF|X86_EFL_TF|X86_EFL_RF|X86_EFL_AC);

        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_SEGMENT_REGS;
        return VINF_SUCCESS;
    }
#endif /* HWACCM_VMX_EMULATE_REALMODE */

    /* Set event injection state. */
    rc  = VMXWriteVMCS(VMX_VMCS_CTRL_ENTRY_IRQ_INFO, intInfo | (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT));

    rc |= VMXWriteVMCS(VMX_VMCS_CTRL_ENTRY_INSTR_LENGTH, cbInstr);
    rc |= VMXWriteVMCS(VMX_VMCS_CTRL_ENTRY_EXCEPTION_ERRCODE, errCode);

    AssertRC(rc);
    return rc;
}


/**
 * Checks for pending guest interrupts and injects them
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        CPU Context
 */
static int VMXR0CheckPendingInterrupt(PVM pVM, PVMCPU pVCpu, CPUMCTX *pCtx)
{
    int rc;

    /* Dispatch any pending interrupts. (injected before, but a VM exit occurred prematurely) */
    if (pVCpu->hwaccm.s.Event.fPending)
    {
        Log(("Reinjecting event %RX64 %08x at %RGv cr2=%RX64\n", pVCpu->hwaccm.s.Event.intInfo, pVCpu->hwaccm.s.Event.errCode, (RTGCPTR)pCtx->rip, pCtx->cr2));
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatIntReinject);
        rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, pVCpu->hwaccm.s.Event.intInfo, 0, pVCpu->hwaccm.s.Event.errCode);
        AssertRC(rc);

        pVCpu->hwaccm.s.Event.fPending = false;
        return VINF_SUCCESS;
    }

    if (pVM->hwaccm.s.fInjectNMI)
    {
        RTGCUINTPTR intInfo;

        intInfo  = X86_XCPT_NMI;
        intInfo |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
        intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

        rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, intInfo, 0, 0);
        AssertRC(rc);

        pVM->hwaccm.s.fInjectNMI = false;
        return VINF_SUCCESS;
    }

    /* When external interrupts are pending, we should exit the VM when IF is set. */
    if (    !TRPMHasTrap(pVCpu)
        &&  VMCPU_FF_ISPENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC|VMCPU_FF_INTERRUPT_PIC)))
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
        else
        if (!VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
        {
            uint8_t u8Interrupt;

            rc = PDMGetInterrupt(pVCpu, &u8Interrupt);
            Log(("Dispatch interrupt: u8Interrupt=%x (%d) rc=%Rrc cs:rip=%04X:%RGv\n", u8Interrupt, u8Interrupt, rc, pCtx->cs, (RTGCPTR)pCtx->rip));
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

#ifdef VBOX_STRICT
    if (TRPMHasTrap(pVCpu))
    {
        uint8_t     u8Vector;
        rc = TRPMQueryTrapAll(pVCpu, &u8Vector, 0, 0, 0);
        AssertRC(rc);
    }
#endif

    if (    pCtx->eflags.u32 & X86_EFL_IF
        && (!VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
        && TRPMHasTrap(pVCpu)
       )
    {
        uint8_t     u8Vector;
        int         rc;
        TRPMEVENT   enmType;
        RTGCUINTPTR intInfo;
        RTGCUINT    errCode;

        /* If a new event is pending, then dispatch it now. */
        rc = TRPMQueryTrapAll(pVCpu, &u8Vector, &enmType, &errCode, 0);
        AssertRC(rc);
        Assert(pCtx->eflags.Bits.u1IF == 1 || enmType == TRPM_TRAP);
        Assert(enmType != TRPM_SOFTWARE_INT);

        /* Clear the pending trap. */
        rc = TRPMResetTrap(pVCpu);
        AssertRC(rc);

        intInfo  = u8Vector;
        intInfo |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);

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
                intInfo |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
                break;
            default:
                break;
            }
            if (u8Vector == X86_XCPT_BP || u8Vector == X86_XCPT_OF)
                intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SWEXCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
            else
                intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HWEXCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
        }
        else
            intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatIntInject);
        rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, intInfo, 0, errCode);
        AssertRC(rc);
    } /* if (interrupts can be dispatched) */

    return VINF_SUCCESS;
}

/**
 * Save the host state
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 */
VMMR0DECL(int) VMXR0SaveHostState(PVM pVM, PVMCPU pVCpu)
{
    int rc = VINF_SUCCESS;

    /*
     * Host CPU Context
     */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_HOST_CONTEXT)
    {
        RTIDTR      idtr;
        RTGDTR      gdtr;
        RTSEL       SelTR;
        PX86DESCHC  pDesc;
        uintptr_t   trBase;
        RTSEL       cs;
        RTSEL       ss;
        uint64_t    cr3;

        /* Control registers */
        rc  = VMXWriteVMCS(VMX_VMCS_HOST_CR0,               ASMGetCR0());
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        if (VMX_IS_64BIT_HOST_MODE())
        {
            cr3 = hwaccmR0Get64bitCR3();
            rc |= VMXWriteVMCS64(VMX_VMCS_HOST_CR3,         cr3);
        }
        else
#endif
        {
            cr3 = ASMGetCR3();
            rc |= VMXWriteVMCS(VMX_VMCS_HOST_CR3,           cr3);
        }
        rc |= VMXWriteVMCS(VMX_VMCS_HOST_CR4,               ASMGetCR4());
        AssertRC(rc);
        Log2(("VMX_VMCS_HOST_CR0 %08x\n", ASMGetCR0()));
        Log2(("VMX_VMCS_HOST_CR3 %08RX64\n", cr3));
        Log2(("VMX_VMCS_HOST_CR4 %08x\n", ASMGetCR4()));

        /* Selector registers. */
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

        /* GDTR & IDTR */
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


        /* Save the base address of the TR selector. */
        if (SelTR > gdtr.cbGdt)
        {
            AssertMsgFailed(("Invalid TR selector %x. GDTR.cbGdt=%x\n", SelTR, gdtr.cbGdt));
            return VERR_VMX_INVALID_HOST_STATE;
        }

#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        if (VMX_IS_64BIT_HOST_MODE())
        {
            pDesc  = &((PX86DESCHC)gdtr.pGdt)[SelTR >> X86_SEL_SHIFT_HC]; /// ????
            uint64_t trBase64 = X86DESC64_BASE(*(PX86DESC64)pDesc);
            rc = VMXWriteVMCS64(VMX_VMCS_HOST_TR_BASE, trBase64);
            Log2(("VMX_VMCS_HOST_TR_BASE %RX64\n", trBase64));
            AssertRC(rc);
        }
        else
#endif
        {
            pDesc  = &((PX86DESCHC)gdtr.pGdt)[SelTR >> X86_SEL_SHIFT_HC];
#if HC_ARCH_BITS == 64
            trBase = X86DESC64_BASE(*pDesc);
#else
            trBase = X86DESC_BASE(*pDesc);
#endif
            rc = VMXWriteVMCS(VMX_VMCS_HOST_TR_BASE, trBase);
            AssertRC(rc);
            Log2(("VMX_VMCS_HOST_TR_BASE %RHv\n", trBase));
        }

        /* FS and GS base. */
#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        if (VMX_IS_64BIT_HOST_MODE())
        {
            Log2(("MSR_K8_FS_BASE = %RX64\n", ASMRdMsr(MSR_K8_FS_BASE)));
            Log2(("MSR_K8_GS_BASE = %RX64\n", ASMRdMsr(MSR_K8_GS_BASE)));
            rc  = VMXWriteVMCS64(VMX_VMCS_HOST_FS_BASE,     ASMRdMsr(MSR_K8_FS_BASE));
            rc |= VMXWriteVMCS64(VMX_VMCS_HOST_GS_BASE,     ASMRdMsr(MSR_K8_GS_BASE));
        }
#endif
        AssertRC(rc);

        /* Sysenter MSRs. */
        /** @todo expensive!! */
        rc  = VMXWriteVMCS(VMX_VMCS32_HOST_SYSENTER_CS,       ASMRdMsr_Low(MSR_IA32_SYSENTER_CS));
        Log2(("VMX_VMCS_HOST_SYSENTER_CS  %08x\n", ASMRdMsr_Low(MSR_IA32_SYSENTER_CS)));
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
            rc |= VMXWriteVMCS(VMX_VMCS_HOST_SYSENTER_ESP,  ASMRdMsr_Low(MSR_IA32_SYSENTER_ESP));
            rc |= VMXWriteVMCS(VMX_VMCS_HOST_SYSENTER_EIP,  ASMRdMsr_Low(MSR_IA32_SYSENTER_EIP));
            Log2(("VMX_VMCS_HOST_SYSENTER_EIP %RX32\n",     ASMRdMsr_Low(MSR_IA32_SYSENTER_EIP)));
            Log2(("VMX_VMCS_HOST_SYSENTER_ESP %RX32\n",     ASMRdMsr_Low(MSR_IA32_SYSENTER_ESP)));
        }
#elif HC_ARCH_BITS == 32
        rc |= VMXWriteVMCS(VMX_VMCS_HOST_SYSENTER_ESP,      ASMRdMsr_Low(MSR_IA32_SYSENTER_ESP));
        rc |= VMXWriteVMCS(VMX_VMCS_HOST_SYSENTER_EIP,      ASMRdMsr_Low(MSR_IA32_SYSENTER_EIP));
        Log2(("VMX_VMCS_HOST_SYSENTER_EIP %RX32\n", ASMRdMsr_Low(MSR_IA32_SYSENTER_EIP)));
        Log2(("VMX_VMCS_HOST_SYSENTER_ESP %RX32\n", ASMRdMsr_Low(MSR_IA32_SYSENTER_ESP)));
#else
        Log2(("VMX_VMCS_HOST_SYSENTER_EIP %RX64\n", ASMRdMsr(MSR_IA32_SYSENTER_EIP)));
        Log2(("VMX_VMCS_HOST_SYSENTER_ESP %RX64\n", ASMRdMsr(MSR_IA32_SYSENTER_ESP)));
        rc |= VMXWriteVMCS64(VMX_VMCS_HOST_SYSENTER_ESP,      ASMRdMsr(MSR_IA32_SYSENTER_ESP));
        rc |= VMXWriteVMCS64(VMX_VMCS_HOST_SYSENTER_EIP,      ASMRdMsr(MSR_IA32_SYSENTER_EIP));
#endif
        AssertRC(rc);

#if 0 /* @todo deal with 32/64 */
        /* Restore the host EFER - on CPUs that support it. */
        if (pVM->hwaccm.s.vmx.msr.vmx_exit.n.allowed1 & VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_EFER_MSR)
        {
            uint64_t msrEFER = ASMRdMsr(MSR_IA32_EFER);
            rc = VMXWriteVMCS64(VMX_VMCS_HOST_FIELD_EFER_FULL, msrEFER);
            AssertRC(rc);
        }
#endif
        pVCpu->hwaccm.s.fContextUseFlags &= ~HWACCM_CHANGED_HOST_CONTEXT;
    }
    return rc;
}

/**
 * Prefetch the 4 PDPT pointers (PAE and nested paging only)
 *
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        Guest context
 */
static void vmxR0PrefetchPAEPdptrs(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    if (CPUMIsGuestInPAEModeEx(pCtx))
    {
        X86PDPE Pdpe;

        for (unsigned i=0;i<4;i++)
        {
            Pdpe = PGMGstGetPaePDPtr(pVCpu, i);
            int rc = VMXWriteVMCS64(VMX_VMCS_GUEST_PDPTR0_FULL + i*2, Pdpe.u);
            AssertRC(rc);
        }
    }
}

/**
 * Update the exception bitmap according to the current CPU state
 *
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        Guest context
 */
static void vmxR0UpdateExceptionBitmap(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    uint32_t u32TrapMask;
    Assert(pCtx);

    u32TrapMask = HWACCM_VMX_TRAP_MASK;
#ifndef DEBUG
    if (pVM->hwaccm.s.fNestedPaging)
        u32TrapMask &= ~RT_BIT(X86_XCPT_PF);   /* no longer need to intercept #PF. */
#endif

    /* Also catch floating point exceptions as we need to report them to the guest in a different way. */
    if (    CPUMIsGuestFPUStateActive(pVCpu) == true
        && !(pCtx->cr0 & X86_CR0_NE)
        && !pVCpu->hwaccm.s.fFPUOldStyleOverride)
    {
        u32TrapMask |= RT_BIT(X86_XCPT_MF);
        pVCpu->hwaccm.s.fFPUOldStyleOverride = true;
    }

#ifdef DEBUG /* till after branching, enable it by default then. */
    /* Intercept X86_XCPT_DB if stepping is enabled */
    if (DBGFIsStepping(pVCpu))
        u32TrapMask |= RT_BIT(X86_XCPT_DB);
    /** @todo Don't trap it unless the debugger has armed breakpoints.  */
    u32TrapMask |= RT_BIT(X86_XCPT_BP);
#endif

#ifdef VBOX_STRICT
    Assert(u32TrapMask & RT_BIT(X86_XCPT_GP));
#endif

# ifdef HWACCM_VMX_EMULATE_REALMODE
    /* Intercept all exceptions in real mode as none of them can be injected directly (#GP otherwise). */
    if (CPUMIsGuestInRealModeEx(pCtx) && pVM->hwaccm.s.vmx.pRealModeTSS)
        u32TrapMask |= HWACCM_VMX_TRAP_MASK_REALMODE;
# endif /* HWACCM_VMX_EMULATE_REALMODE */

    int rc = VMXWriteVMCS(VMX_VMCS_CTRL_EXCEPTION_BITMAP, u32TrapMask);
    AssertRC(rc);
}

/**
 * Loads the guest state
 *
 * NOTE: Don't do anything here that can cause a jump back to ring 3!!!!!
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        Guest context
 */
VMMR0DECL(int) VMXR0LoadGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    int         rc = VINF_SUCCESS;
    RTGCUINTPTR val;
    X86EFLAGS   eflags;

    /* VMX_VMCS_CTRL_ENTRY_CONTROLS
     * Set required bits to one and zero according to the MSR capabilities.
     */
    val  = pVM->hwaccm.s.vmx.msr.vmx_entry.n.disallowed0;
    /* Load guest debug controls (dr7 & IA32_DEBUGCTL_MSR) (forced to 1 on the 'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs) */
    val |= VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_DEBUG;
#if 0 /* @todo deal with 32/64 */
    /* Required for the EFER write below, not supported on all CPUs. */
    val |= VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_EFER_MSR;
#endif
    /* 64 bits guest mode? */
    if (pCtx->msrEFER & MSR_K6_EFER_LMA)
        val |= VMX_VMCS_CTRL_ENTRY_CONTROLS_IA64_MODE;
    /* else Must be zero when AMD64 is not available. */

    /* Mask away the bits that the CPU doesn't support */
    val &= pVM->hwaccm.s.vmx.msr.vmx_entry.n.allowed1;
    rc = VMXWriteVMCS(VMX_VMCS_CTRL_ENTRY_CONTROLS, val);
    AssertRC(rc);

    /* VMX_VMCS_CTRL_EXIT_CONTROLS
     * Set required bits to one and zero according to the MSR capabilities.
     */
    val  = pVM->hwaccm.s.vmx.msr.vmx_exit.n.disallowed0;

    /* Save debug controls (dr7 & IA32_DEBUGCTL_MSR) (forced to 1 on the 'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs) */
#if 0 /* @todo deal with 32/64 */
    val |= VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG | VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_EFER_MSR;
#else
    val |= VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG;
#endif

#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (VMX_IS_64BIT_HOST_MODE())
        val |= VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64;
    /* else: Must be zero when AMD64 is not available. */
#elif HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
    if (pCtx->msrEFER & MSR_K6_EFER_LMA)
        val |= VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64;      /* our switcher goes to long mode */
    else
        Assert(!(val & VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64));
#endif
    val &= pVM->hwaccm.s.vmx.msr.vmx_exit.n.allowed1;
    /* Don't acknowledge external interrupts on VM-exit. */
    rc = VMXWriteVMCS(VMX_VMCS_CTRL_EXIT_CONTROLS, val);
    AssertRC(rc);

    /* Guest CPU context: ES, CS, SS, DS, FS, GS. */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_SEGMENT_REGS)
    {
#ifdef HWACCM_VMX_EMULATE_REALMODE
        if (pVM->hwaccm.s.vmx.pRealModeTSS)
        {
            PGMMODE enmGuestMode = PGMGetGuestMode(pVCpu);
            if (pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode != enmGuestMode)
            {
                /* Correct weird requirements for switching to protected mode. */
                if (    pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode == PGMMODE_REAL
                    &&  enmGuestMode >= PGMMODE_PROTECTED)
                {
                    /* Flush the recompiler code cache as it's not unlikely
                     * the guest will rewrite code it will later execute in real
                     * mode (OpenBSD 4.0 is one such example)
                     */
                    REMFlushTBs(pVM);

                    /* DPL of all hidden selector registers must match the current CPL (0). */
                    pCtx->csHid.Attr.n.u2Dpl  = 0;
                    pCtx->csHid.Attr.n.u4Type = X86_SEL_TYPE_CODE | X86_SEL_TYPE_RW_ACC;

                    pCtx->dsHid.Attr.n.u2Dpl  = 0;
                    pCtx->esHid.Attr.n.u2Dpl  = 0;
                    pCtx->fsHid.Attr.n.u2Dpl  = 0;
                    pCtx->gsHid.Attr.n.u2Dpl  = 0;
                    pCtx->ssHid.Attr.n.u2Dpl  = 0;

                    /* The limit must correspond to the granularity bit. */
                    if (!pCtx->csHid.Attr.n.u1Granularity)
                        pCtx->csHid.u32Limit &= 0xffff;
                    if (!pCtx->dsHid.Attr.n.u1Granularity)
                        pCtx->dsHid.u32Limit &= 0xffff;
                    if (!pCtx->esHid.Attr.n.u1Granularity)
                        pCtx->esHid.u32Limit &= 0xffff;
                    if (!pCtx->fsHid.Attr.n.u1Granularity)
                        pCtx->fsHid.u32Limit &= 0xffff;
                    if (!pCtx->gsHid.Attr.n.u1Granularity)
                        pCtx->gsHid.u32Limit &= 0xffff;
                    if (!pCtx->ssHid.Attr.n.u1Granularity)
                        pCtx->ssHid.u32Limit &= 0xffff;
                }
                else
                /* Switching from protected mode to real mode. */
                if (    pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode >= PGMMODE_PROTECTED
                    &&  enmGuestMode == PGMMODE_REAL)
                {
                    /* The limit must also be set to 0xffff. */
                    pCtx->csHid.u32Limit = 0xffff;
                    pCtx->dsHid.u32Limit = 0xffff;
                    pCtx->esHid.u32Limit = 0xffff;
                    pCtx->fsHid.u32Limit = 0xffff;
                    pCtx->gsHid.u32Limit = 0xffff;
                    pCtx->ssHid.u32Limit = 0xffff;

                    Assert(pCtx->csHid.u64Base <= 0xfffff);
                    Assert(pCtx->dsHid.u64Base <= 0xfffff);
                    Assert(pCtx->esHid.u64Base <= 0xfffff);
                    Assert(pCtx->fsHid.u64Base <= 0xfffff);
                    Assert(pCtx->gsHid.u64Base <= 0xfffff);
                }
                pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode = enmGuestMode;
            }
            else
            /* VT-x will fail with a guest invalid state otherwise... (CPU state after a reset) */
            if (   CPUMIsGuestInRealModeEx(pCtx)
                && pCtx->csHid.u64Base == 0xffff0000)
            {
                pCtx->csHid.u64Base = 0xf0000;
                pCtx->cs = 0xf000;
            }
        }
#endif /* HWACCM_VMX_EMULATE_REALMODE */

        VMX_WRITE_SELREG(ES, es);
        AssertRC(rc);

        VMX_WRITE_SELREG(CS, cs);
        AssertRC(rc);

        VMX_WRITE_SELREG(SS, ss);
        AssertRC(rc);

        VMX_WRITE_SELREG(DS, ds);
        AssertRC(rc);

        /* The base values in the hidden fs & gs registers are not in sync with the msrs; they are cut to 32 bits. */
        VMX_WRITE_SELREG(FS, fs);
        AssertRC(rc);

        VMX_WRITE_SELREG(GS, gs);
        AssertRC(rc);
    }

    /* Guest CPU context: LDTR. */
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
    /* Guest CPU context: TR. */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_TR)
    {
#ifdef HWACCM_VMX_EMULATE_REALMODE
        /* Real mode emulation using v86 mode with CR4.VME (interrupt redirection using the int bitmap in the TSS) */
        if (CPUMIsGuestInRealModeEx(pCtx))
        {
            RTGCPHYS GCPhys;

            /* We convert it here every time as pci regions could be reconfigured. */
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
#endif /* HWACCM_VMX_EMULATE_REALMODE */
        {
            rc =  VMXWriteVMCS(VMX_VMCS16_GUEST_FIELD_TR,         pCtx->tr);
            rc |= VMXWriteVMCS(VMX_VMCS32_GUEST_TR_LIMIT,         pCtx->trHid.u32Limit);
            rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_TR_BASE,        pCtx->trHid.u64Base);

            val = pCtx->trHid.Attr.u;

            /* The TSS selector must be busy. */
            if ((val & 0xF) == X86_SEL_TYPE_SYS_286_TSS_AVAIL)
                val = (val & ~0xF) | X86_SEL_TYPE_SYS_286_TSS_BUSY;
            else
                /* Default even if no TR selector has been set (otherwise vmlaunch will fail!) */
                val = (val & ~0xF) | X86_SEL_TYPE_SYS_386_TSS_BUSY;

        }
        rc |= VMXWriteVMCS(VMX_VMCS32_GUEST_TR_ACCESS_RIGHTS, val);
        AssertRC(rc);
    }
    /* Guest CPU context: GDTR. */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_GDTR)
    {
        rc  = VMXWriteVMCS(VMX_VMCS32_GUEST_GDTR_LIMIT,       pCtx->gdtr.cbGdt);
        rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_GDTR_BASE,      pCtx->gdtr.pGdt);
        AssertRC(rc);
    }
    /* Guest CPU context: IDTR. */
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_IDTR)
    {
        rc  = VMXWriteVMCS(VMX_VMCS32_GUEST_IDTR_LIMIT,       pCtx->idtr.cbIdt);
        rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_IDTR_BASE,      pCtx->idtr.pIdt);
        AssertRC(rc);
    }

    /*
     * Sysenter MSRs (unconditional)
     */
    rc  = VMXWriteVMCS(VMX_VMCS32_GUEST_SYSENTER_CS,    pCtx->SysEnter.cs);
    rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_SYSENTER_EIP, pCtx->SysEnter.eip);
    rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_SYSENTER_ESP, pCtx->SysEnter.esp);
    AssertRC(rc);

    /* Control registers */
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
        val |= X86_CR0_PE | X86_CR0_PG;
        if (pVM->hwaccm.s.fNestedPaging)
        {
            if (CPUMIsGuestInPagedProtectedModeEx(pCtx))
            {
                /* Disable cr3 read/write monitoring as we don't need it for EPT. */
                pVCpu->hwaccm.s.vmx.proc_ctls &=  ~(  VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT
                                                    | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT);
            }
            else
            {
                /* Reenable cr3 read/write monitoring as our identity mapped page table is active. */
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
        /* CR0 flags owned by the host; if the guests attempts to change them, then
         * the VM will exit.
         */
        val =   X86_CR0_PE  /* Must monitor this bit (assumptions are made for real mode emulation) */
              | X86_CR0_WP  /* Must monitor this bit (it must always be enabled). */
              | X86_CR0_PG  /* Must monitor this bit (assumptions are made for real mode & protected mode without paging emulation) */
              | X86_CR0_TS
              | X86_CR0_ET  /* Bit not restored during VM-exit! */
              | X86_CR0_CD  /* Bit not restored during VM-exit! */
              | X86_CR0_NW  /* Bit not restored during VM-exit! */
              | X86_CR0_NE
              | X86_CR0_MP;
        pVCpu->hwaccm.s.vmx.cr0_mask = val;

        rc |= VMXWriteVMCS(VMX_VMCS_CTRL_CR0_MASK, val);
        Log2(("Guest CR0-mask %08x\n", val));
        AssertRC(rc);
    }
    if (pVCpu->hwaccm.s.fContextUseFlags & HWACCM_CHANGED_GUEST_CR4)
    {
        /* CR4 */
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
            default:                   /* shut up gcc */
                AssertFailed();
                return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
            }
        }
        else
        if (!CPUMIsGuestInPagedProtectedModeEx(pCtx))
        {
            /* We use 4 MB pages in our identity mapping page table for real and protected mode without paging. */
            val |= X86_CR4_PSE;
            /* Our identity mapping is a 32 bits page directory. */
            val &= ~X86_CR4_PAE;
        }

        rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_CR4,            val);
        Log2(("Guest CR4 %08x\n", val));
        /* CR4 flags owned by the host; if the guests attempts to change them, then
         * the VM will exit.
         */
        val =   0
              | X86_CR4_PAE
              | X86_CR4_PGE
              | X86_CR4_PSE
              | X86_CR4_VMXE;
        pVCpu->hwaccm.s.vmx.cr4_mask = val;

        rc |= VMXWriteVMCS(VMX_VMCS_CTRL_CR4_MASK, val);
        Log2(("Guest CR4-mask %08x\n", val));
        AssertRC(rc);
    }

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

            if (!CPUMIsGuestInPagedProtectedModeEx(pCtx))
            {
                RTGCPHYS GCPhys;

                /* We convert it here every time as pci regions could be reconfigured. */
                rc = PDMVMMDevHeapR3ToGCPhys(pVM, pVM->hwaccm.s.vmx.pNonPagingModeEPTPageTable, &GCPhys);
                AssertRC(rc);

                /* We use our identity mapping page table here as we need to map guest virtual to guest physical addresses; EPT will
                 * take care of the translation to host physical addresses.
                 */
                val = GCPhys;
            }
            else
            {
                /* Save the real guest CR3 in VMX_VMCS_GUEST_CR3 */
                val = pCtx->cr3;
                /* Prefetch the four PDPT entries in PAE mode. */
                vmxR0PrefetchPAEPdptrs(pVM, pVCpu, pCtx);
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

    /* Debug registers. */
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

        /* Sync the debug state now if any breakpoint is armed. */
        if (    (pCtx->dr[7] & (X86_DR7_ENABLED_MASK|X86_DR7_GD))
            &&  !CPUMIsGuestDebugStateActive(pVCpu)
            &&  !DBGFIsStepping(pVCpu))
        {
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatDRxArmed);

            /* Disable drx move intercepts. */
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
        rc |= VMXWriteVMCS(VMX_VMCS_GUEST_DEBUG_EXCEPTIONS,         0);
        AssertRC(rc);
    }

    /* EIP, ESP and EFLAGS */
    rc  = VMXWriteVMCS64(VMX_VMCS64_GUEST_RIP, pCtx->rip);
    rc |= VMXWriteVMCS64(VMX_VMCS64_GUEST_RSP, pCtx->rsp);
    AssertRC(rc);

    /* Bits 22-31, 15, 5 & 3 must be zero. Bit 1 must be 1. */
    eflags      = pCtx->eflags;
    eflags.u32 &= VMX_EFLAGS_RESERVED_0;
    eflags.u32 |= VMX_EFLAGS_RESERVED_1;

#ifdef HWACCM_VMX_EMULATE_REALMODE
    /* Real mode emulation using v86 mode. */
    if (CPUMIsGuestInRealModeEx(pCtx))
    {
        pVCpu->hwaccm.s.vmx.RealMode.eflags = eflags;

        eflags.Bits.u1VM   = 1;
        eflags.Bits.u2IOPL = 0; /* must always be 0 or else certain instructions won't cause faults. */
    }
#endif /* HWACCM_VMX_EMULATE_REALMODE */
    rc   = VMXWriteVMCS(VMX_VMCS_GUEST_RFLAGS,           eflags.u32);
    AssertRC(rc);

    /* TSC offset. */
    uint64_t u64TSCOffset;

    if (TMCpuTickCanUseRealTSC(pVCpu, &u64TSCOffset))
    {
        /* Note: VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT takes precedence over TSC_OFFSET */
        rc = VMXWriteVMCS64(VMX_VMCS_CTRL_TSC_OFFSET_FULL, u64TSCOffset);
        AssertRC(rc);

        pVCpu->hwaccm.s.vmx.proc_ctls &= ~VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT;
        rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
        AssertRC(rc);
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTSCOffset);
    }
    else
    {
        pVCpu->hwaccm.s.vmx.proc_ctls |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT;
        rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
        AssertRC(rc);
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTSCIntercept);
    }

    /* 64 bits guest mode? */
    if (pCtx->msrEFER & MSR_K6_EFER_LMA)
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
        /* Unconditionally update these as wrmsr might have changed them. */
        rc = VMXWriteVMCS64(VMX_VMCS64_GUEST_FS_BASE, pCtx->fsHid.u64Base);
        AssertRC(rc);
        rc = VMXWriteVMCS64(VMX_VMCS64_GUEST_GS_BASE, pCtx->gsHid.u64Base);
        AssertRC(rc);
    }
    else
    {
        pVCpu->hwaccm.s.vmx.pfnStartVM  = VMXR0StartVM32;
    }

#if 0 /* @todo deal with 32/64 */
    /* Unconditionally update the guest EFER - on CPUs that supports it. */
    if (pVM->hwaccm.s.vmx.msr.vmx_entry.n.allowed1 & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_EFER_MSR)
    {
        rc = VMXWriteVMCS64(VMX_VMCS_GUEST_EFER_FULL, pCtx->msrEFER);
        AssertRC(rc);
    }
#endif

    vmxR0UpdateExceptionBitmap(pVM, pVCpu, pCtx);

    /* Done. */
    pVCpu->hwaccm.s.fContextUseFlags &= ~HWACCM_CHANGED_ALL_GUEST;

    return rc;
}

/**
 * Syncs back the guest state
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        Guest context
 */
DECLINLINE(int) VMXR0SaveGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    RTGCUINTREG val, valShadow;
    RTGCUINTPTR uInterruptState;
    int         rc;

    /* Let's first sync back eip, esp, and eflags. */
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
        Log(("uInterruptState %x eip=%RGv\n", uInterruptState, pCtx->rip));
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

    /* Note: no reason to sync back the CRx registers. They can't be changed by the guest. */
    /* Note: only in the nested paging case can CR3 & CR4 be changed by the guest. */
    if (    pVM->hwaccm.s.fNestedPaging
        &&  CPUMIsGuestInPagedProtectedModeEx(pCtx))
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
        /* Prefetch the four PDPT entries in PAE mode. */
        vmxR0PrefetchPAEPdptrs(pVM, pVCpu, pCtx);
    }

    /* Sync back DR7 here. */
    VMXReadCachedVMCS(VMX_VMCS64_GUEST_DR7, &val);
    pCtx->dr[7] = val;

    /* Guest CPU context: ES, CS, SS, DS, FS, GS. */
    VMX_READ_SELREG(ES, es);
    VMX_READ_SELREG(SS, ss);
    VMX_READ_SELREG(CS, cs);
    VMX_READ_SELREG(DS, ds);
    VMX_READ_SELREG(FS, fs);
    VMX_READ_SELREG(GS, gs);

    /*
     * System MSRs
     */
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

#ifdef HWACCM_VMX_EMULATE_REALMODE
    /* Real mode emulation using v86 mode. */
    if (CPUMIsGuestInRealModeEx(pCtx))
    {
        /* Hide our emulation flags */
        pCtx->eflags.Bits.u1VM   = 0;

        /* Restore original IOPL setting as we always use 0. */
        pCtx->eflags.Bits.u2IOPL = pVCpu->hwaccm.s.vmx.RealMode.eflags.Bits.u2IOPL;

        /* Force a TR resync every time in case we switch modes. */
        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_TR;
    }
    else
#endif /* HWACCM_VMX_EMULATE_REALMODE */
    {
        /* In real mode we have a fake TSS, so only sync it back when it's supposed to be valid. */
        VMX_READ_SELREG(TR, tr);
    }
    return VINF_SUCCESS;
}

/**
 * Dummy placeholder
 *
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 */
static void vmxR0SetupTLBDummy(PVM pVM, PVMCPU pVCpu)
{
    NOREF(pVM);
    NOREF(pVCpu);
    return;
}

/**
 * Setup the tagged TLB for EPT
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 */
static void vmxR0SetupTLBEPT(PVM pVM, PVMCPU pVCpu)
{
    PHWACCM_CPUINFO pCpu;

    Assert(pVM->hwaccm.s.fNestedPaging);
    Assert(!pVM->hwaccm.s.vmx.fVPID);

    /* Deal with tagged TLBs if VPID or EPT is supported. */
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
        Assert(!pCpu->fFlushTLB);

    pVCpu->hwaccm.s.idLastCpu = pCpu->idCpu;
    pCpu->fFlushTLB           = false;

    if (pVCpu->hwaccm.s.fForceTLBFlush)
    {
        vmxR0FlushEPT(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushContext, 0);
    }
    else
    if (VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_TLB_SHOOTDOWN))
    {
        /* Deal with pending TLB shootdown actions which were queued when we were not executing code. */
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTlbShootdown);
        for (unsigned i=0;i<pVCpu->hwaccm.s.cTlbShootdownPages;i++)
        {
            /* aTlbShootdownPages contains physical addresses in this case. */
            vmxR0FlushEPT(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushContext, pVCpu->hwaccm.s.aTlbShootdownPages[i]);
        }
    }
    pVCpu->hwaccm.s.cTlbShootdownPages = 0;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);

#ifdef VBOX_WITH_STATISTICS
    if (pVCpu->hwaccm.s.fForceTLBFlush)
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushTLBWorldSwitch);
    else
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatNoFlushTLBWorldSwitch);
#endif
}

#ifdef HWACCM_VTX_WITH_VPID
/**
 * Setup the tagged TLB for VPID
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 */
static void vmxR0SetupTLBVPID(PVM pVM, PVMCPU pVCpu)
{
    PHWACCM_CPUINFO pCpu;

    Assert(pVM->hwaccm.s.vmx.fVPID);
    Assert(!pVM->hwaccm.s.fNestedPaging);

    /* Deal with tagged TLBs if VPID or EPT is supported. */
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
        Assert(!pCpu->fFlushTLB);

    pVCpu->hwaccm.s.idLastCpu = pCpu->idCpu;

    /* Make sure we flush the TLB when required. Switch ASID to achieve the same thing, but without actually flushing the whole TLB (which is expensive). */
    if (pVCpu->hwaccm.s.fForceTLBFlush)
    {
        if (    ++pCpu->uCurrentASID >= pVM->hwaccm.s.uMaxASID
            ||  pCpu->fFlushTLB)
        {
            pCpu->fFlushTLB                  = false;
            pCpu->uCurrentASID               = 1;       /* start at 1; host uses 0 */
            pCpu->cTLBFlushes++;
        }
        else
        {
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushASID);
            pVCpu->hwaccm.s.fForceTLBFlush     = false;
        }

        pVCpu->hwaccm.s.cTLBFlushes  = pCpu->cTLBFlushes;
        pVCpu->hwaccm.s.uCurrentASID = pCpu->uCurrentASID;
    }
    else
    {
        Assert(!pCpu->fFlushTLB);
        Assert(pVCpu->hwaccm.s.uCurrentASID && pCpu->uCurrentASID);

        if (VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_TLB_SHOOTDOWN))
        {
            /* Deal with pending TLB shootdown actions which were queued when we were not executing code. */
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatTlbShootdown);
            for (unsigned i=0;i<pVCpu->hwaccm.s.cTlbShootdownPages;i++)
                vmxR0FlushVPID(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushContext, pVCpu->hwaccm.s.aTlbShootdownPages[i]);
        }
    }
    pVCpu->hwaccm.s.cTlbShootdownPages = 0;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);

    AssertMsg(pVCpu->hwaccm.s.cTLBFlushes == pCpu->cTLBFlushes, ("Flush count mismatch for cpu %d (%x vs %x)\n", pCpu->idCpu, pVCpu->hwaccm.s.cTLBFlushes, pCpu->cTLBFlushes));
    AssertMsg(pCpu->uCurrentASID >= 1 && pCpu->uCurrentASID < pVM->hwaccm.s.uMaxASID, ("cpu%d uCurrentASID = %x\n", pCpu->idCpu, pCpu->uCurrentASID));
    AssertMsg(pVCpu->hwaccm.s.uCurrentASID >= 1 && pVCpu->hwaccm.s.uCurrentASID < pVM->hwaccm.s.uMaxASID, ("cpu%d VM uCurrentASID = %x\n", pCpu->idCpu, pVCpu->hwaccm.s.uCurrentASID));

    int rc  = VMXWriteVMCS(VMX_VMCS16_GUEST_FIELD_VPID, pVCpu->hwaccm.s.uCurrentASID);
    AssertRC(rc);

    if (pVCpu->hwaccm.s.fForceTLBFlush)
        vmxR0FlushVPID(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushContext, 0);

#ifdef VBOX_WITH_STATISTICS
    if (pVCpu->hwaccm.s.fForceTLBFlush)
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatFlushTLBWorldSwitch);
    else
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatNoFlushTLBWorldSwitch);
#endif
}
#endif /* HWACCM_VTX_WITH_VPID */

/**
 * Runs guest code in a VT-x VM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        Guest context
 */
VMMR0DECL(int) VMXR0RunGuestCode(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    int         rc = VINF_SUCCESS;
    RTGCUINTREG val;
    RTGCUINTREG exitReason = VMX_EXIT_INVALID;
    RTGCUINTREG instrError, cbInstr;
    RTGCUINTPTR exitQualification;
    RTGCUINTPTR intInfo = 0; /* shut up buggy gcc 4 */
    RTGCUINTPTR errCode, instrInfo;
    bool        fSyncTPR = false;
    PHWACCM_CPUINFO pCpu = 0;
    unsigned    cResume = 0;
#ifdef VBOX_STRICT
    RTCPUID  idCpuCheck;
#endif
#ifdef VBOX_WITH_STATISTICS
    bool fStatEntryStarted = true;
    bool fStatExit2Started = false;
#endif

    Log2(("\nE"));

    STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatEntry, x);

#ifdef VBOX_STRICT
    {
        RTCCUINTREG val;

        rc = VMXReadVMCS(VMX_VMCS_CTRL_PIN_EXEC_CONTROLS, &val);
        AssertRC(rc);
        Log2(("VMX_VMCS_CTRL_PIN_EXEC_CONTROLS = %08x\n", val));

        /* allowed zero */
        if ((val & pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.disallowed0) != pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.disallowed0)
            Log(("Invalid VMX_VMCS_CTRL_PIN_EXEC_CONTROLS: zero\n"));

        /* allowed one */
        if ((val & ~pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.allowed1) != 0)
            Log(("Invalid VMX_VMCS_CTRL_PIN_EXEC_CONTROLS: one\n"));

        rc = VMXReadVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, &val);
        AssertRC(rc);
        Log2(("VMX_VMCS_CTRL_PROC_EXEC_CONTROLS = %08x\n", val));

        /* Must be set according to the MSR, but can be cleared in case of EPT. */
        if (pVM->hwaccm.s.fNestedPaging)
            val |=   VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT
                   | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT
                   | VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT;

        /* allowed zero */
        if ((val & pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.disallowed0) != pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.disallowed0)
            Log(("Invalid VMX_VMCS_CTRL_PROC_EXEC_CONTROLS: zero\n"));

        /* allowed one */
        if ((val & ~pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1) != 0)
            Log(("Invalid VMX_VMCS_CTRL_PROC_EXEC_CONTROLS: one\n"));

        rc = VMXReadVMCS(VMX_VMCS_CTRL_ENTRY_CONTROLS, &val);
        AssertRC(rc);
        Log2(("VMX_VMCS_CTRL_ENTRY_CONTROLS = %08x\n", val));

        /* allowed zero */
        if ((val & pVM->hwaccm.s.vmx.msr.vmx_entry.n.disallowed0) != pVM->hwaccm.s.vmx.msr.vmx_entry.n.disallowed0)
            Log(("Invalid VMX_VMCS_CTRL_ENTRY_CONTROLS: zero\n"));

        /* allowed one */
        if ((val & ~pVM->hwaccm.s.vmx.msr.vmx_entry.n.allowed1) != 0)
            Log(("Invalid VMX_VMCS_CTRL_ENTRY_CONTROLS: one\n"));

        rc = VMXReadVMCS(VMX_VMCS_CTRL_EXIT_CONTROLS, &val);
        AssertRC(rc);
        Log2(("VMX_VMCS_CTRL_EXIT_CONTROLS = %08x\n", val));

        /* allowed zero */
        if ((val & pVM->hwaccm.s.vmx.msr.vmx_exit.n.disallowed0) != pVM->hwaccm.s.vmx.msr.vmx_exit.n.disallowed0)
            Log(("Invalid VMX_VMCS_CTRL_EXIT_CONTROLS: zero\n"));

        /* allowed one */
        if ((val & ~pVM->hwaccm.s.vmx.msr.vmx_exit.n.allowed1) != 0)
            Log(("Invalid VMX_VMCS_CTRL_EXIT_CONTROLS: one\n"));
    }
#endif

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    pVCpu->hwaccm.s.vmx.VMCSCache.u64TimeEntry = RTTimeNanoTS();
#endif

    /* We can jump to this point to resume execution after determining that a VM-exit is innocent.
     */
ResumeExecution:
    STAM_STATS({
        if (fStatExit2Started)   { STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2, y); fStatExit2Started = false; }
        if (!fStatEntryStarted) { STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatEntry, x); fStatEntryStarted = true; }
    });
    AssertMsg(pVCpu->hwaccm.s.idEnteredCpu == RTMpCpuId(),
              ("Expected %d, I'm %d; cResume=%d exitReason=%RGv exitQualification=%RGv\n",
               (int)pVCpu->hwaccm.s.idEnteredCpu, (int)RTMpCpuId(), cResume, exitReason, exitQualification));
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
            /* Irq inhibition is no longer active; clear the corresponding VMX state. */
            rc = VMXWriteVMCS(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE,   0);
            AssertRC(rc);
        }
    }
    else
    {
        /* Irq inhibition is no longer active; clear the corresponding VMX state. */
        rc = VMXWriteVMCS(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE,   0);
        AssertRC(rc);
    }

    /* Check for pending actions that force us to go back to ring 3. */
    if (    VM_FF_ISPENDING(pVM, VM_FF_HWACCM_TO_R3_MASK)
        ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_HWACCM_TO_R3_MASK))
    {
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TO_R3);
        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatSwitchToR3);
        rc = RT_UNLIKELY(VM_FF_ISPENDING(pVM, VM_FF_PGM_NO_MEMORY)) ? VINF_EM_NO_MEMORY : VINF_EM_RAW_TO_R3;
        goto end;
    }
    /* Pending request packets might contain actions that need immediate attention, such as pending hardware interrupts. */
    if (VM_FF_ISPENDING(pVM, VM_FF_REQUEST))
    {
        rc = VINF_EM_PENDING_REQUEST;
        goto end;
    }

    /* When external interrupts are pending, we should exit the VM when IF is set. */
    /* Note! *After* VM_FF_INHIBIT_INTERRUPTS check!!! */
    rc = VMXR0CheckPendingInterrupt(pVM, pVCpu, pCtx);
    if (RT_FAILURE(rc))
        goto end;

    /** @todo check timers?? */

    /* TPR caching using CR8 is only available in 64 bits mode */
    /* Note the 32 bits exception for AMD (X86_CPUID_AMD_FEATURE_ECX_CR8L), but that appears missing in Intel CPUs */
    /* Note: we can't do this in LoadGuestState as PDMApicGetTPR can jump back to ring 3 (lock)!!!!! */
    /**
     * @todo reduce overhead
     */
    if (   (pCtx->msrEFER & MSR_K6_EFER_LMA)
        &&  pVM->hwaccm.s.vmx.pAPIC)
    {
        /* TPR caching in CR8 */
        uint8_t u8TPR;
        bool    fPending;

        int rc = PDMApicGetTPR(pVM, &u8TPR, &fPending);
        AssertRC(rc);
        /* The TPR can be found at offset 0x80 in the APIC mmio page. */
        pVM->hwaccm.s.vmx.pAPIC[0x80] = u8TPR << 4; /* bits 7-4 contain the task priority */

        /* Two options here:
         * - external interrupt pending, but masked by the TPR value.
         *   -> a CR8 update that lower the current TPR value should cause an exit
         * - no pending interrupts
         *   -> We don't need to be explicitely notified. There are enough world switches for detecting pending interrupts.
         */
        rc  = VMXWriteVMCS(VMX_VMCS_CTRL_TPR_THRESHOLD, (fPending) ? u8TPR : 0);
        AssertRC(rc);

        /* Always sync back the TPR; we should optimize this though */ /** @todo optimize TPR sync. */
        fSyncTPR = true;
    }

#if defined(HWACCM_VTX_WITH_EPT) && defined(LOG_ENABLED)
    if (    pVM->hwaccm.s.fNestedPaging
# ifdef HWACCM_VTX_WITH_VPID
        ||  pVM->hwaccm.s.vmx.fVPID
# endif /* HWACCM_VTX_WITH_VPID */
        )
    {
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
        else
        if (pVCpu->hwaccm.s.fForceTLBFlush)
            LogFlow(("Manual TLB flush\n"));
    }
#endif
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PGMDynMapFlushAutoSet(pVCpu);
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
    /* Save the host state first. */
    rc  = VMXR0SaveHostState(pVM, pVCpu);
    if (rc != VINF_SUCCESS)
        goto end;
    /* Load the guest state */
    rc = VMXR0LoadGuestState(pVM, pVCpu, pCtx);
    if (rc != VINF_SUCCESS)
        goto end;

    /* Deal with tagged TLB setup and invalidation. */
    pVM->hwaccm.s.vmx.pfnSetupTaggedTLB(pVM, pVCpu);

    /* Non-register state Guest Context */
    /** @todo change me according to cpu state */
    rc = VMXWriteVMCS(VMX_VMCS32_GUEST_ACTIVITY_STATE,           VMX_CMS_GUEST_ACTIVITY_ACTIVE);
    AssertRC(rc);

    STAM_STATS({ STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatEntry, x); fStatEntryStarted = false; });

    /* Manual save and restore:
     * - General purpose registers except RIP, RSP
     *
     * Trashed:
     * - CR2 (we don't care)
     * - LDTR (reset to 0)
     * - DRx (presumably not changed at all)
     * - DR7 (reset to 0x400)
     * - EFLAGS (reset to RT_BIT(1); not relevant)
     *
     */


    /* All done! Let's start VM execution. */
    STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatInGC, z);
#ifdef VBOX_STRICT
    Assert(idCpuCheck == RTMpCpuId());
#endif

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    pVCpu->hwaccm.s.vmx.VMCSCache.cResume = cResume;
    pVCpu->hwaccm.s.vmx.VMCSCache.u64TimeSwitch = RTTimeNanoTS();
#endif

    TMNotifyStartOfExecution(pVCpu);
    rc = pVCpu->hwaccm.s.vmx.pfnStartVM(pVCpu->hwaccm.s.fResumeVM, pCtx, &pVCpu->hwaccm.s.vmx.VMCSCache, pVM, pVCpu);
    TMNotifyEndOfExecution(pVCpu);

    AssertMsg(!pVCpu->hwaccm.s.vmx.VMCSCache.Write.cValidEntries, ("pVCpu->hwaccm.s.vmx.VMCSCache.Write.cValidEntries=%d\n", pVCpu->hwaccm.s.vmx.VMCSCache.Write.cValidEntries));

    /* In case we execute a goto ResumeExecution later on. */
    pVCpu->hwaccm.s.fResumeVM  = true;
    pVCpu->hwaccm.s.fForceTLBFlush = false;

    /*
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * IMPORTANT: WE CAN'T DO ANY LOGGING OR OPERATIONS THAT CAN DO A LONGJMP BACK TO RING 3 *BEFORE* WE'VE SYNCED BACK (MOST OF) THE GUEST STATE
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */
    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatInGC, z);
    STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatExit1, v);

    if (rc != VINF_SUCCESS)
    {
        VMXR0ReportWorldSwitchError(pVM, pVCpu, rc, pCtx);
        goto end;
    }

    /* Success. Query the guest state and figure out what has happened. */

    /* Investigate why there was a VM-exit. */
    rc  = VMXReadCachedVMCS(VMX_VMCS32_RO_EXIT_REASON, &exitReason);
    STAM_COUNTER_INC(&pVCpu->hwaccm.s.paStatExitReasonR0[exitReason & MASK_EXITREASON_STAT]);

    exitReason &= 0xffff;   /* bit 0-15 contain the exit code. */
    rc |= VMXReadCachedVMCS(VMX_VMCS32_RO_VM_INSTR_ERROR, &instrError);
    rc |= VMXReadCachedVMCS(VMX_VMCS32_RO_EXIT_INSTR_LENGTH, &cbInstr);
    rc |= VMXReadCachedVMCS(VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO, &intInfo);
    /* might not be valid; depends on VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_IS_VALID. */
    rc |= VMXReadCachedVMCS(VMX_VMCS32_RO_EXIT_INTERRUPTION_ERRCODE, &errCode);
    rc |= VMXReadCachedVMCS(VMX_VMCS32_RO_EXIT_INSTR_INFO, &instrInfo);
    rc |= VMXReadCachedVMCS(VMX_VMCS_RO_EXIT_QUALIFICATION, &exitQualification);
    AssertRC(rc);

    /* Sync back the guest state */
    rc = VMXR0SaveGuestState(pVM, pVCpu, pCtx);
    AssertRC(rc);

    /* Note! NOW IT'S SAFE FOR LOGGING! */
#ifdef LOG_LOGGING
    VMMR0LogFlushEnable(pVCpu);
#endif
    Log2(("Raw exit reason %08x\n", exitReason));

    /* Check if an injected event was interrupted prematurely. */
    rc = VMXReadCachedVMCS(VMX_VMCS32_RO_IDT_INFO,            &val);
    AssertRC(rc);
    pVCpu->hwaccm.s.Event.intInfo = VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(val);
    if (    VMX_EXIT_INTERRUPTION_INFO_VALID(pVCpu->hwaccm.s.Event.intInfo)
        /* Ignore 'int xx' as they'll be restarted anyway. */
        &&  VMX_EXIT_INTERRUPTION_INFO_TYPE(pVCpu->hwaccm.s.Event.intInfo) != VMX_EXIT_INTERRUPTION_INFO_TYPE_SW
        /* Ignore software exceptions (such as int3) as they're reoccur when we restart the instruction anyway. */
        &&  VMX_EXIT_INTERRUPTION_INFO_TYPE(pVCpu->hwaccm.s.Event.intInfo) != VMX_EXIT_INTERRUPTION_INFO_TYPE_SWEXCPT)
    {
        pVCpu->hwaccm.s.Event.fPending = true;
        /* Error code present? */
        if (VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_IS_VALID(pVCpu->hwaccm.s.Event.intInfo))
        {
            rc = VMXReadCachedVMCS(VMX_VMCS32_RO_IDT_ERRCODE, &val);
            AssertRC(rc);
            pVCpu->hwaccm.s.Event.errCode  = val;
            Log(("Pending inject %RX64 at %RGv exit=%08x intInfo=%08x exitQualification=%RGv pending error=%RX64\n", pVCpu->hwaccm.s.Event.intInfo, (RTGCPTR)pCtx->rip, exitReason, intInfo, exitQualification, val));
        }
        else
        {
            Log(("Pending inject %RX64 at %RGv exit=%08x intInfo=%08x exitQualification=%RGv\n", pVCpu->hwaccm.s.Event.intInfo, (RTGCPTR)pCtx->rip, exitReason, intInfo, exitQualification));
            pVCpu->hwaccm.s.Event.errCode  = 0;
        }
    }
#ifdef VBOX_STRICT
    else
    if (    VMX_EXIT_INTERRUPTION_INFO_VALID(pVCpu->hwaccm.s.Event.intInfo)
        /* Ignore software exceptions (such as int3) as they're reoccur when we restart the instruction anyway. */
        &&  VMX_EXIT_INTERRUPTION_INFO_TYPE(pVCpu->hwaccm.s.Event.intInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_SWEXCPT)
    {
        Log(("Ignore pending inject %RX64 at %RGv exit=%08x intInfo=%08x exitQualification=%RGv\n", pVCpu->hwaccm.s.Event.intInfo, (RTGCPTR)pCtx->rip, exitReason, intInfo, exitQualification));
    }

    if (exitReason == VMX_EXIT_ERR_INVALID_GUEST_STATE)
        HWACCMDumpRegs(pVM, pVCpu, pCtx);
#endif

    Log2(("E%d: New EIP=%RGv\n", exitReason, (RTGCPTR)pCtx->rip));
    Log2(("Exit reason %d, exitQualification %RGv\n", (uint32_t)exitReason, exitQualification));
    Log2(("instrInfo=%d instrError=%d instr length=%d\n", (uint32_t)instrInfo, (uint32_t)instrError, (uint32_t)cbInstr));
    Log2(("Interruption error code %d\n", (uint32_t)errCode));
    Log2(("IntInfo = %08x\n", (uint32_t)intInfo));

    if (fSyncTPR)
    {
        rc = PDMApicSetTPR(pVM, pVM->hwaccm.s.vmx.pAPIC[0x80] >> 4);
        AssertRC(rc);
    }

    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit1, v);
    STAM_STATS({ STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatExit2, y); fStatExit2Started = true; });

    /* Some cases don't need a complete resync of the guest CPU state; handle them here. */
    switch (exitReason)
    {
    case VMX_EXIT_EXCEPTION:            /* 0 Exception or non-maskable interrupt (NMI). */
    case VMX_EXIT_EXTERNAL_IRQ:         /* 1 External interrupt. */
    {
        uint32_t vector = VMX_EXIT_INTERRUPTION_INFO_VECTOR(intInfo);

        if (!VMX_EXIT_INTERRUPTION_INFO_VALID(intInfo))
        {
            Assert(exitReason == VMX_EXIT_EXTERNAL_IRQ);
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
                rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo), cbInstr, 0);
                AssertRC(rc);
                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                goto ResumeExecution;
            }

            case X86_XCPT_PF: /* Page fault */
            {
#ifdef DEBUG
                if (pVM->hwaccm.s.fNestedPaging)
                {   /* A genuine pagefault.
                     * Forward the trap to the guest by injecting the exception and resuming execution.
                     */
                    Log(("Guest page fault at %RGv cr2=%RGv error code %x rsp=%RGv\n", (RTGCPTR)pCtx->rip, exitQualification, errCode, (RTGCPTR)pCtx->rsp));

                    Assert(CPUMIsGuestInPagedProtectedModeEx(pCtx));

                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestPF);

                    /* Now we must update CR2. */
                    pCtx->cr2 = exitQualification;
                    rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo), cbInstr, errCode);
                    AssertRC(rc);

                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
#endif
                Assert(!pVM->hwaccm.s.fNestedPaging);

                Log2(("Page fault at %RGv error code %x\n", exitQualification, errCode));
                /* Exit qualification contains the linear address of the page fault. */
                TRPMAssertTrap(pVCpu, X86_XCPT_PF, TRPM_TRAP);
                TRPMSetErrorCode(pVCpu, errCode);
                TRPMSetFaultAddress(pVCpu, exitQualification);

                /* Forward it to our trap handler first, in case our shadow pages are out of sync. */
                rc = PGMTrap0eHandler(pVCpu, errCode, CPUMCTX2CORE(pCtx), (RTGCPTR)exitQualification);
                Log2(("PGMTrap0eHandler %RGv returned %Rrc\n", (RTGCPTR)pCtx->rip, rc));
                if (rc == VINF_SUCCESS)
                {   /* We've successfully synced our shadow pages, so let's just continue execution. */
                    Log2(("Shadow page fault at %RGv cr2=%RGv error code %x\n", (RTGCPTR)pCtx->rip, exitQualification ,errCode));
                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitShadowPF);

                    TRPMResetTrap(pVCpu);

                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
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
                    pCtx->cr2 = exitQualification;
                    rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo), cbInstr, errCode);
                    AssertRC(rc);

                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
#ifdef VBOX_STRICT
                if (rc != VINF_EM_RAW_EMULATE_INSTR && rc != VINF_EM_RAW_EMULATE_IO_BLOCK)
                    Log2(("PGMTrap0eHandler failed with %d\n", rc));
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
                Log(("Trap %x at %04X:%RGv\n", vector, pCtx->cs, (RTGCPTR)pCtx->rip));
                rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo), cbInstr, errCode);
                AssertRC(rc);

                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                goto ResumeExecution;
            }

            case X86_XCPT_DB:   /* Debug exception. */
            {
                uint64_t uDR6;

                /* DR6, DR7.GD and IA32_DEBUGCTL.LBR are not updated yet.
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
                Assert(DBGFIsStepping(pVCpu) || CPUMIsGuestInRealModeEx(pCtx));

                uDR6  = X86_DR6_INIT_VAL;
                uDR6 |= (exitQualification & (X86_DR6_B0|X86_DR6_B1|X86_DR6_B2|X86_DR6_B3|X86_DR6_BD|X86_DR6_BS));
                rc = DBGFRZTrap01Handler(pVM, pVCpu, CPUMCTX2CORE(pCtx), uDR6);
                if (rc == VINF_EM_RAW_GUEST_TRAP)
                {
                    /** @todo this isn't working, but we'll never get here normally. */

                    /* Update DR6 here. */
                    pCtx->dr[6]  = uDR6;

                    /* X86_DR7_GD will be cleared if drx accesses should be trapped inside the guest. */
                    pCtx->dr[7] &= ~X86_DR7_GD;

                    /* Paranoia. */
                    pCtx->dr[7] &= 0xffffffff;                                              /* upper 32 bits reserved */
                    pCtx->dr[7] &= ~(RT_BIT(11) | RT_BIT(12) | RT_BIT(14) | RT_BIT(15));    /* must be zero */
                    pCtx->dr[7] |= 0x400;                                                   /* must be one */

                    /* Resync DR7 */
                    rc = VMXWriteVMCS64(VMX_VMCS64_GUEST_DR7, pCtx->dr[7]);
                    AssertRC(rc);

                    Log(("Trap %x (debug) at %RGv exit qualification %RX64\n", vector, (RTGCPTR)pCtx->rip, exitQualification));
                    rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo), cbInstr, errCode);
                    AssertRC(rc);

                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
                /* Return to ring 3 to deal with the debug exit code. */
                break;
            }

            case X86_XCPT_BP:   /* Breakpoint. */
            {
                rc = DBGFRZTrap03Handler(pVM, pVCpu, CPUMCTX2CORE(pCtx));
                if (rc == VINF_EM_RAW_GUEST_TRAP)
                {
                    Log(("Guest #BP at %04x:%RGv\n", pCtx->cs, pCtx->rip));
                    rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo), cbInstr, errCode);
                    AssertRC(rc);
                    goto ResumeExecution;
                }
                if (rc == VINF_SUCCESS)
                    goto ResumeExecution;
                Log(("Debugger BP at %04x:%RGv (rc=%Rrc)\n", pCtx->cs, pCtx->rip, rc));
                break;
            }

            case X86_XCPT_GP:   /* General protection failure exception.*/
            {
                uint32_t    cbOp;
                uint32_t    cbSize;
                DISCPUSTATE Cpu;

                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitGuestGP);
#ifdef VBOX_STRICT
                if (!CPUMIsGuestInRealModeEx(pCtx))
                {
                    Log(("Trap %x at %04X:%RGv errorCode=%x\n", vector, pCtx->cs, (RTGCPTR)pCtx->rip, errCode));
                    rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo), cbInstr, errCode);
                    AssertRC(rc);
                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
#endif
                Assert(CPUMIsGuestInRealModeEx(pCtx));

                LogFlow(("Real mode X86_XCPT_GP instruction emulation at %RGv\n", (RTGCPTR)pCtx->rip));

                rc = EMInterpretDisasOne(pVM, pVCpu, CPUMCTX2CORE(pCtx), &Cpu, &cbOp);
                if (RT_SUCCESS(rc))
                {
                    bool fUpdateRIP = true;

                    Assert(cbOp == Cpu.opsize);
                    switch (Cpu.pCurInstr->opcode)
                    {
                    case OP_CLI:
                        pCtx->eflags.Bits.u1IF = 0;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitCli);
                        break;

                    case OP_STI:
                        pCtx->eflags.Bits.u1IF = 1;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitSti);
                        break;

                    case OP_HLT:
                        fUpdateRIP = false;
                        rc = VINF_EM_HALT;
                        pCtx->rip += Cpu.opsize;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitHlt);
                        break;

                    case OP_POPF:
                    {
                        RTGCPTR   GCPtrStack;
                        uint32_t  cbParm;
                        uint32_t  uMask;
                        X86EFLAGS eflags;

                        if (Cpu.prefix & PREFIX_OPSIZE)
                        {
                            cbParm = 4;
                            uMask  = 0xffffffff;
                        }
                        else
                        {
                            cbParm = 2;
                            uMask  = 0xffff;
                        }

                        rc = SELMToFlatEx(pVM, DIS_SELREG_SS, CPUMCTX2CORE(pCtx), pCtx->esp & uMask, 0, &GCPtrStack);
                        if (RT_FAILURE(rc))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }
                        eflags.u = 0;
                        rc = PGMPhysRead(pVM, (RTGCPHYS)GCPtrStack, &eflags.u, cbParm);
                        if (RT_FAILURE(rc))
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

                        if (Cpu.prefix & PREFIX_OPSIZE)
                        {
                            cbParm = 4;
                            uMask  = 0xffffffff;
                        }
                        else
                        {
                            cbParm = 2;
                            uMask  = 0xffff;
                        }

                        rc = SELMToFlatEx(pVM, DIS_SELREG_SS, CPUMCTX2CORE(pCtx), (pCtx->esp - cbParm) & uMask, 0, &GCPtrStack);
                        if (RT_FAILURE(rc))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }
                        eflags = pCtx->eflags;
                        /* RF & VM cleared when pushed in real mode; see pushf description in AMD manual. */
                        eflags.Bits.u1RF = 0;
                        eflags.Bits.u1VM = 0;

                        rc = PGMPhysWrite(pVM, (RTGCPHYS)GCPtrStack, &eflags.u, cbParm);
                        if (RT_FAILURE(rc))
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

                        if (Cpu.prefix & (PREFIX_OPSIZE | PREFIX_ADDRSIZE))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }

                        rc = SELMToFlatEx(pVM, DIS_SELREG_SS, CPUMCTX2CORE(pCtx), pCtx->esp & uMask, 0, &GCPtrStack);
                        if (RT_FAILURE(rc))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }
                        rc = PGMPhysRead(pVM, (RTGCPHYS)GCPtrStack, &aIretFrame[0], sizeof(aIretFrame));
                        if (RT_FAILURE(rc))
                        {
                            rc = VERR_EM_INTERPRETER;
                            break;
                        }
                        pCtx->ip            = aIretFrame[0];
                        pCtx->cs            = aIretFrame[1];
                        pCtx->csHid.u64Base = pCtx->cs << 4;
                        pCtx->eflags.u      = (pCtx->eflags.u & ~(X86_EFL_POPF_BITS & uMask)) | (aIretFrame[2] & X86_EFL_POPF_BITS & uMask);
                        pCtx->sp           += sizeof(aIretFrame);

                        LogFlow(("iret to %04x:%x\n", pCtx->cs, pCtx->ip));
                        fUpdateRIP = false;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIret);
                        break;
                    }

                    case OP_INT:
                    {
                        RTGCUINTPTR intInfo;

                        LogFlow(("Realmode: INT %x\n", Cpu.param1.parval & 0xff));
                        intInfo  = Cpu.param1.parval & 0xff;
                        intInfo |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
                        intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

                        rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, intInfo, cbOp, 0);
                        AssertRC(rc);
                        fUpdateRIP = false;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitInt);
                        break;
                    }

                    case OP_INTO:
                    {
                        if (pCtx->eflags.Bits.u1OF)
                        {
                            RTGCUINTPTR intInfo;

                            LogFlow(("Realmode: INTO\n"));
                            intInfo  = X86_XCPT_OF;
                            intInfo |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
                            intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

                            rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, intInfo, cbOp, 0);
                            AssertRC(rc);
                            fUpdateRIP = false;
                            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitInt);
                        }
                        break;
                    }

                    case OP_INT3:
                    {
                        RTGCUINTPTR intInfo;

                        LogFlow(("Realmode: INT 3\n"));
                        intInfo  = 3;
                        intInfo |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
                        intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

                        rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, intInfo, cbOp, 0);
                        AssertRC(rc);
                        fUpdateRIP = false;
                        STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitInt);
                        break;
                    }

                    default:
                        rc = EMInterpretInstructionCPU(pVM, pVCpu, &Cpu, CPUMCTX2CORE(pCtx), 0, &cbSize);
                        break;
                    }

                    if (rc == VINF_SUCCESS)
                    {
                        if (fUpdateRIP)
                            pCtx->rip += cbOp; /* Move on to the next instruction. */

                        /* lidt, lgdt can end up here. In the future crx changes as well. Just reload the whole context to be done with it. */
                        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_ALL;

                        /* Only resume if successful. */
                        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                        goto ResumeExecution;
                    }
                }
                else
                    rc = VERR_EM_INTERPRETER;

                AssertMsg(rc == VERR_EM_INTERPRETER || rc == VINF_PGM_CHANGE_MODE || rc == VINF_EM_HALT, ("Unexpected rc=%Rrc\n", rc));
                break;
            }

#ifdef VBOX_STRICT
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
                }

                Log(("Trap %x at %04X:%RGv\n", vector, pCtx->cs, (RTGCPTR)pCtx->rip));
                rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo), cbInstr, errCode);
                AssertRC(rc);

                STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                goto ResumeExecution;
            }
#endif
            default:
#ifdef HWACCM_VMX_EMULATE_REALMODE
                if (CPUMIsGuestInRealModeEx(pCtx))
                {
                    Log(("Real Mode Trap %x at %04x:%04X error code %x\n", vector, pCtx->cs, pCtx->eip, errCode));
                    rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo), cbInstr, errCode);
                    AssertRC(rc);

                    /* Go back to ring 3 in case of a triple fault. */
                    if (    vector == X86_XCPT_DF
                        &&  rc == VINF_EM_RESET)
                        break;

                    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
                    goto ResumeExecution;
                }
#endif
                AssertMsgFailed(("Unexpected vm-exit caused by exception %x\n", vector));
                rc = VERR_VMX_UNEXPECTED_EXCEPTION;
                break;
            } /* switch (vector) */

            break;

        default:
            rc = VERR_VMX_UNEXPECTED_INTERRUPTION_EXIT_CODE;
            AssertMsgFailed(("Unexpected interuption code %x\n", intInfo));
            break;
        }

        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub3, y3);
        break;
    }

    case VMX_EXIT_EPT_VIOLATION:        /* 48 EPT violation. An attempt to access memory with a guest-physical address was disallowed by the configuration of the EPT paging structures. */
    {
        RTGCPHYS GCPhys;

        Assert(pVM->hwaccm.s.fNestedPaging);

        rc = VMXReadVMCS64(VMX_VMCS_EXIT_PHYS_ADDR_FULL, &GCPhys);
        AssertRC(rc);
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

        Log(("EPT Page fault %x at %RGp error code %x\n", (uint32_t)exitQualification, GCPhys, errCode));

        /* GCPhys contains the guest physical address of the page fault. */
        TRPMAssertTrap(pVCpu, X86_XCPT_PF, TRPM_TRAP);
        TRPMSetErrorCode(pVCpu, errCode);
        TRPMSetFaultAddress(pVCpu, GCPhys);

        /* Handle the pagefault trap for the nested shadow table. */
        rc = PGMR0Trap0eHandlerNestedPaging(pVM, pVCpu, PGMMODE_EPT, errCode, CPUMCTX2CORE(pCtx), GCPhys);
        Log2(("PGMR0Trap0eHandlerNestedPaging %RGv returned %Rrc\n", (RTGCPTR)pCtx->rip, rc));
        if (rc == VINF_SUCCESS)
        {   /* We've successfully synced our shadow pages, so let's just continue execution. */
            Log2(("Shadow page fault at %RGv cr2=%RGp error code %x\n", (RTGCPTR)pCtx->rip, exitQualification , errCode));
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitReasonNPF);

            TRPMResetTrap(pVCpu);

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

    case VMX_EXIT_EPT_MISCONFIG:
    {
        RTGCPHYS GCPhys;

        Assert(pVM->hwaccm.s.fNestedPaging);

        rc = VMXReadVMCS64(VMX_VMCS_EXIT_PHYS_ADDR_FULL, &GCPhys);
        AssertRC(rc);

        Log(("VMX_EXIT_EPT_MISCONFIG for %VGp\n", GCPhys));
        break;
    }

    case VMX_EXIT_IRQ_WINDOW:           /* 7 Interrupt window. */
        /* Clear VM-exit on IF=1 change. */
        LogFlow(("VMX_EXIT_IRQ_WINDOW %RGv pending=%d IF=%d\n", (RTGCPTR)pCtx->rip, VMCPU_FF_ISPENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC|VMCPU_FF_INTERRUPT_PIC)), pCtx->eflags.Bits.u1IF));
        pVCpu->hwaccm.s.vmx.proc_ctls &= ~VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_IRQ_WINDOW_EXIT;
        rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
        AssertRC(rc);
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
        AssertMsgFailed(("EMU: cpuid failed with %Rrc\n", rc));
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
        AssertMsg(rc == VERR_EM_INTERPRETER, ("EMU: invlpg %RGv failed with %Rrc\n", exitQualification, rc));
        break;
    }

    case VMX_EXIT_RDMSR:                /* 31 RDMSR. Guest software attempted to execute RDMSR. */
    case VMX_EXIT_WRMSR:                /* 32 WRMSR. Guest software attempted to execute WRMSR. */
    {
        uint32_t cbSize;

        /* Note: the intel manual claims there's a REX version of RDMSR that's slightly different, so we play safe by completely disassembling the instruction. */
        Log2(("VMX: %s\n", (exitReason == VMX_EXIT_RDMSR) ? "rdmsr" : "wrmsr"));
        rc = EMInterpretInstruction(pVM, pVCpu, CPUMCTX2CORE(pCtx), 0, &cbSize);
        if (rc == VINF_SUCCESS)
        {
            /* EIP has been updated already. */

            /* Only resume if successful. */
            goto ResumeExecution;
        }
        AssertMsg(rc == VERR_EM_INTERPRETER, ("EMU: %s failed with %Rrc\n", (exitReason == VMX_EXIT_RDMSR) ? "rdmsr" : "wrmsr", rc));
        break;
    }

    case VMX_EXIT_CRX_MOVE:             /* 28 Control-register accesses. */
    {
        STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatExit2Sub2, y2);

        switch (VMX_EXIT_QUALIFICATION_CRX_ACCESS(exitQualification))
        {
        case VMX_EXIT_QUALIFICATION_CRX_ACCESS_WRITE:
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
                Assert(!(pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW));
                break;

            default:
                AssertFailed();
                break;
            }
            /* Check if a sync operation is pending. */
            if (    rc == VINF_SUCCESS /* don't bother if we are going to ring 3 anyway */
                &&  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL))
            {
                rc = PGMSyncCR3(pVCpu, pCtx->cr0, pCtx->cr3, pCtx->cr4, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3));
                AssertRC(rc);
            }
            break;

        case VMX_EXIT_QUALIFICATION_CRX_ACCESS_READ:
            Log2(("VMX: mov x, crx\n"));
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitCRxRead[VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification)]);

            Assert(!pVM->hwaccm.s.fNestedPaging || !CPUMIsGuestInPagedProtectedModeEx(pCtx) || VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification) != USE_REG_CR3);

            /* CR8 reads only cause an exit when the TPR shadow feature isn't present. */
            Assert(VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification) != 8 || !(pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW));

            rc = EMInterpretCRxRead(pVM, pVCpu, CPUMCTX2CORE(pCtx),
                                    VMX_EXIT_QUALIFICATION_CRX_GENREG(exitQualification),
                                    VMX_EXIT_QUALIFICATION_CRX_REGISTER(exitQualification));
            break;

        case VMX_EXIT_QUALIFICATION_CRX_ACCESS_CLTS:
            Log2(("VMX: clts\n"));
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitCLTS);
            rc = EMInterpretCLTS(pVM, pVCpu);
            pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0;
            break;

        case VMX_EXIT_QUALIFICATION_CRX_ACCESS_LMSW:
            Log2(("VMX: lmsw %x\n", VMX_EXIT_QUALIFICATION_CRX_LMSW_DATA(exitQualification)));
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitLMSW);
            rc = EMInterpretLMSW(pVM, pVCpu, CPUMCTX2CORE(pCtx), VMX_EXIT_QUALIFICATION_CRX_LMSW_DATA(exitQualification));
            pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0;
            break;
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
        if (!DBGFIsStepping(pVCpu))
        {
            /* Disable drx move intercepts. */
            pVCpu->hwaccm.s.vmx.proc_ctls &= ~VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT;
            rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
            AssertRC(rc);

            /* Save the host and load the guest debug state. */
            rc = CPUMR0LoadGuestDebugState(pVM, pVCpu, pCtx, true /* include DR6 */);
            AssertRC(rc);

#ifdef VBOX_WITH_STATISTICS
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatDRxContextSwitch);
            if (VMX_EXIT_QUALIFICATION_DRX_DIRECTION(exitQualification) == VMX_EXIT_QUALIFICATION_DRX_DIRECTION_WRITE)
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitDRxWrite);
            else
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitDRxRead);
#endif

            goto ResumeExecution;
        }

        /** @todo clear VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT after the first time and restore drx registers afterwards */
        if (VMX_EXIT_QUALIFICATION_DRX_DIRECTION(exitQualification) == VMX_EXIT_QUALIFICATION_DRX_DIRECTION_WRITE)
        {
            Log2(("VMX: mov drx%d, genreg%d\n", VMX_EXIT_QUALIFICATION_DRX_REGISTER(exitQualification), VMX_EXIT_QUALIFICATION_DRX_GENREG(exitQualification)));
            STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitDRxWrite);
            rc = EMInterpretDRxWrite(pVM, pVCpu, CPUMCTX2CORE(pCtx),
                                     VMX_EXIT_QUALIFICATION_DRX_REGISTER(exitQualification),
                                     VMX_EXIT_QUALIFICATION_DRX_GENREG(exitQualification));
            pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_DEBUG;
            Log2(("DR7=%08x\n", pCtx->dr[7]));
        }
        else
        {
            Log2(("VMX: mov x, drx\n"));
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
        uint32_t uIOWidth = VMX_EXIT_QUALIFICATION_IO_WIDTH(exitQualification);
        uint32_t uPort;
        bool     fIOWrite = (VMX_EXIT_QUALIFICATION_IO_DIRECTION(exitQualification) == VMX_EXIT_QUALIFICATION_IO_DIRECTION_OUT);

        /** @todo necessary to make the distinction? */
        if (VMX_EXIT_QUALIFICATION_IO_ENCODING(exitQualification) == VMX_EXIT_QUALIFICATION_IO_ENCODING_DX)
        {
            uPort = pCtx->edx & 0xffff;
        }
        else
            uPort = VMX_EXIT_QUALIFICATION_IO_PORT(exitQualification);  /* Immediate encoding. */

        /* paranoia */
        if (RT_UNLIKELY(uIOWidth == 2 || uIOWidth >= 4))
        {
            rc = fIOWrite ? VINF_IOM_HC_IOPORT_WRITE : VINF_IOM_HC_IOPORT_READ;
            STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub1, y1);
            break;
        }

        uint32_t cbSize = g_aIOSize[uIOWidth];

        if (VMX_EXIT_QUALIFICATION_IO_STRING(exitQualification))
        {
            /* ins/outs */
            DISCPUSTATE Cpu;

            /* Disassemble manually to deal with segment prefixes. */
            /** @todo VMX_VMCS_EXIT_GUEST_LINEAR_ADDR contains the flat pointer operand of the instruction. */
            /** @todo VMX_VMCS32_RO_EXIT_INSTR_INFO also contains segment prefix info. */
            rc = EMInterpretDisasOne(pVM, pVCpu, CPUMCTX2CORE(pCtx), &Cpu, NULL);
            if (rc == VINF_SUCCESS)
            {
                if (fIOWrite)
                {
                    Log2(("IOMInterpretOUTSEx %RGv %x size=%d\n", (RTGCPTR)pCtx->rip, uPort, cbSize));
                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIOStringWrite);
                    rc = IOMInterpretOUTSEx(pVM, CPUMCTX2CORE(pCtx), uPort, Cpu.prefix, cbSize);
                }
                else
                {
                    Log2(("IOMInterpretINSEx  %RGv %x size=%d\n", (RTGCPTR)pCtx->rip, uPort, cbSize));
                    STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIOStringRead);
                    rc = IOMInterpretINSEx(pVM, CPUMCTX2CORE(pCtx), uPort, Cpu.prefix, cbSize);
                }
            }
            else
                rc = VINF_EM_RAW_EMULATE_INSTR;
        }
        else
        {
            /* normal in/out */
            uint32_t uAndVal = g_aIOOpAnd[uIOWidth];

            Assert(!VMX_EXIT_QUALIFICATION_IO_REP(exitQualification));

            if (fIOWrite)
            {
                STAM_COUNTER_INC(&pVCpu->hwaccm.s.StatExitIOWrite);
                rc = IOMIOPortWrite(pVM, uPort, pCtx->eax & uAndVal, cbSize);
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
                    for (unsigned i=0;i<4;i++)
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

                            /* Note: AMD64 Architecture Programmer's Manual 13.1:
                             * Bits 15:13 of the DR6 register is never cleared by the processor and must be cleared by software after
                             * the contents have been read.
                             */
                            ASMSetDR6(uDR6);

                            /* X86_DR7_GD will be cleared if drx accesses should be trapped inside the guest. */
                            pCtx->dr[7] &= ~X86_DR7_GD;

                            /* Paranoia. */
                            pCtx->dr[7] &= 0xffffffff;                                              /* upper 32 bits reserved */
                            pCtx->dr[7] &= ~(RT_BIT(11) | RT_BIT(12) | RT_BIT(14) | RT_BIT(15));    /* must be zero */
                            pCtx->dr[7] |= 0x400;                                                   /* must be one */

                            /* Resync DR7 */
                            rc = VMXWriteVMCS64(VMX_VMCS64_GUEST_DR7, pCtx->dr[7]);
                            AssertRC(rc);

                            /* Construct inject info. */
                            intInfo  = X86_XCPT_DB;
                            intInfo |= (1 << VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT);
                            intInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HWEXCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

                            Log(("Inject IO debug trap at %RGv\n", (RTGCPTR)pCtx->rip));
                            rc = VMXR0InjectEvent(pVM, pVCpu, pCtx, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(intInfo), 0, 0);
                            AssertRC(rc);

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
        if (rc == VINF_IOM_HC_IOPORT_READ)
            Assert(!fIOWrite);
        else if (rc == VINF_IOM_HC_IOPORT_WRITE)
            Assert(fIOWrite);
        else
            AssertMsg(RT_FAILURE(rc) || rc == VINF_EM_RAW_EMULATE_INSTR || rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_TRPM_XCPT_DISPATCHED, ("%Rrc\n", rc));
#endif
        STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2Sub1, y1);
        break;
    }

    case VMX_EXIT_TPR:                  /* 43 TPR below threshold. Guest software executed MOV to CR8. */
        LogFlow(("VMX_EXIT_TPR\n"));
        /* RIP is already set to the next instruction and the TPR has been synced back. Just resume. */
        goto ResumeExecution;

    case VMX_EXIT_PREEMPTION_TIMER:     /* 52 VMX-preemption timer expired. The preemption timer counted down to zero. */
        goto ResumeExecution;

    default:
        /* The rest is handled after syncing the entire CPU state. */
        break;
    }

    /* Note: the guest state isn't entirely synced back at this stage. */

    /* Investigate why there was a VM-exit. (part 2) */
    switch (exitReason)
    {
    case VMX_EXIT_EXCEPTION:            /* 0 Exception or non-maskable interrupt (NMI). */
    case VMX_EXIT_EXTERNAL_IRQ:         /* 1 External interrupt. */
    case VMX_EXIT_EPT_VIOLATION:
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

    case VMX_EXIT_TASK_SWITCH:          /* 9 Task switch. */
        rc = VERR_EM_INTERPRETER;
        break;

    case VMX_EXIT_HLT:                  /* 12 Guest software attempted to execute HLT. */
        /** Check if external interrupts are pending; if so, don't switch back. */
        pCtx->rip++;    /* skip hlt */
        if (    pCtx->eflags.Bits.u1IF
            &&  VMCPU_FF_ISPENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC|VMCPU_FF_INTERRUPT_PIC)))
            goto ResumeExecution;

        rc = VINF_EM_HALT;
        break;

    case VMX_EXIT_RSM:                  /* 17 Guest software attempted to execute RSM in SMM. */
        AssertFailed(); /* can't happen. */
        rc = VINF_EM_RAW_EXCEPTION_PRIVILEGED;
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
        rc = VINF_EM_RAW_EXCEPTION_PRIVILEGED;
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
                  || rc == VINF_IOM_HC_IOPORT_READ
                  || rc == VINF_IOM_HC_IOPORT_WRITE
                  || rc == VINF_EM_RAW_GUEST_TRAP
                  || rc == VINF_TRPM_XCPT_DISPATCHED
                  || rc == VINF_EM_RESCHEDULE_REM,
                  ("rc = %d\n", rc));
        break;

    case VMX_EXIT_TPR:                  /* 43 TPR below threshold. Guest software executed MOV to CR8. */
    case VMX_EXIT_RDMSR:                /* 31 RDMSR. Guest software attempted to execute RDMSR. */
    case VMX_EXIT_WRMSR:                /* 32 WRMSR. Guest software attempted to execute WRMSR. */
        /* Note: If we decide to emulate them here, then we must sync the MSRs that could have been changed (sysenter, fs/gs base)!!! */
        rc = VERR_EM_INTERPRETER;
        break;

    case VMX_EXIT_MWAIT:                /* 36 Guest software executed MWAIT. */
    case VMX_EXIT_MONITOR:              /* 39 Guest software attempted to execute MONITOR. */
    case VMX_EXIT_PAUSE:                /* 40 Guest software attempted to execute PAUSE. */
        rc = VINF_EM_RAW_EXCEPTION_PRIVILEGED;
        break;

    case VMX_EXIT_IRQ_WINDOW:           /* 7 Interrupt window. */
        Assert(rc == VINF_EM_RAW_INTERRUPT);
        break;

    case VMX_EXIT_ERR_INVALID_GUEST_STATE:  /* 33 VM-entry failure due to invalid guest state. */
    {
#ifdef VBOX_STRICT
        RTCCUINTREG val = 0;

        Log(("VMX_EXIT_ERR_INVALID_GUEST_STATE\n"));

        VMXReadVMCS(VMX_VMCS64_GUEST_RIP, &val);
        Log(("Old eip %RGv new %RGv\n", (RTGCPTR)pCtx->rip, (RTGCPTR)val));

        VMXReadVMCS(VMX_VMCS64_GUEST_CR0, &val);
        Log(("VMX_VMCS_GUEST_CR0        %RX64\n", (uint64_t)val));

        VMXReadVMCS(VMX_VMCS64_GUEST_CR3, &val);
        Log(("VMX_VMCS_GUEST_CR3        %RX64\n", (uint64_t)val));

        VMXReadVMCS(VMX_VMCS64_GUEST_CR4, &val);
        Log(("VMX_VMCS_GUEST_CR4        %RX64\n", (uint64_t)val));

        VMXReadVMCS(VMX_VMCS_GUEST_RFLAGS, &val);
        Log(("VMX_VMCS_GUEST_RFLAGS     %08x\n", val));

        VMX_LOG_SELREG(CS, "CS");
        VMX_LOG_SELREG(DS, "DS");
        VMX_LOG_SELREG(ES, "ES");
        VMX_LOG_SELREG(FS, "FS");
        VMX_LOG_SELREG(GS, "GS");
        VMX_LOG_SELREG(SS, "SS");
        VMX_LOG_SELREG(TR, "TR");
        VMX_LOG_SELREG(LDTR, "LDTR");

        VMXReadVMCS(VMX_VMCS64_GUEST_GDTR_BASE, &val);
        Log(("VMX_VMCS_GUEST_GDTR_BASE    %RX64\n", (uint64_t)val));
        VMXReadVMCS(VMX_VMCS64_GUEST_IDTR_BASE, &val);
        Log(("VMX_VMCS_GUEST_IDTR_BASE    %RX64\n", (uint64_t)val));
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

    /* Signal changes for the recompiler. */
    CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_SYSENTER_MSR | CPUM_CHANGED_LDTR | CPUM_CHANGED_GDTR | CPUM_CHANGED_IDTR | CPUM_CHANGED_TR | CPUM_CHANGED_HIDDEN_SEL_REGS);

    /* If we executed vmlaunch/vmresume and an external irq was pending, then we don't have to do a full sync the next time. */
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

    /* translate into a less severe return code */
    if (rc == VERR_EM_INTERPRETER)
        rc = VINF_EM_RAW_EMULATE_INSTR;
    else
    /* Try to extract more information about what might have gone wrong here. */
    if (rc == VERR_VMX_INVALID_VMCS_PTR)
    {
        VMXGetActivateVMCS(&pVCpu->hwaccm.s.vmx.lasterror.u64VMCSPhys);
        pVCpu->hwaccm.s.vmx.lasterror.ulVMCSRevision = *(uint32_t *)pVCpu->hwaccm.s.vmx.pVMCS;
        pVCpu->hwaccm.s.vmx.lasterror.idEnteredCpu   = pVCpu->hwaccm.s.idEnteredCpu;
        pVCpu->hwaccm.s.vmx.lasterror.idCurrentCpu   = RTMpCpuId();
    }

    STAM_STATS({
        if (fStatExit2Started)      STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatExit2, y);
        else if (fStatEntryStarted) STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatEntry, x);
    });
    Log2(("X"));
    return rc;
}


/**
 * Enters the VT-x session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCpu        CPU info struct
 */
VMMR0DECL(int) VMXR0Enter(PVM pVM, PVMCPU pVCpu, PHWACCM_CPUINFO pCpu)
{
    Assert(pVM->hwaccm.s.vmx.fSupported);

    unsigned cr4 = ASMGetCR4();
    if (!(cr4 & X86_CR4_VMXE))
    {
        AssertMsgFailed(("X86_CR4_VMXE should be set!\n"));
        return VERR_VMX_X86_CR4_VMXE_CLEARED;
    }

    /* Activate the VM Control Structure. */
    int rc = VMXActivateVMCS(pVCpu->hwaccm.s.vmx.pVMCSPhys);
    if (RT_FAILURE(rc))
        return rc;

    pVCpu->hwaccm.s.fResumeVM = false;
    return VINF_SUCCESS;
}


/**
 * Leaves the VT-x session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        CPU context
 */
VMMR0DECL(int) VMXR0Leave(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    Assert(pVM->hwaccm.s.vmx.fSupported);

    /* Save the guest debug state if necessary. */
    if (CPUMIsGuestDebugStateActive(pVCpu))
    {
        CPUMR0SaveGuestDebugState(pVM, pVCpu, pCtx, true /* save DR6 */);

        /* Enable drx move intercepts again. */
        pVCpu->hwaccm.s.vmx.proc_ctls |= VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT;
        int rc = VMXWriteVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, pVCpu->hwaccm.s.vmx.proc_ctls);
        AssertRC(rc);

        /* Resync the debug registers the next time. */
        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_DEBUG;
    }
    else
        Assert(pVCpu->hwaccm.s.vmx.proc_ctls & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT);

    /* Clear VM Control Structure. Marking it inactive, clearing implementation specific data and writing back VMCS data to memory. */
    int rc = VMXClearVMCS(pVCpu->hwaccm.s.vmx.pVMCSPhys);
    AssertRC(rc);

    return VINF_SUCCESS;
}

/**
 * Flush the TLB (EPT)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   enmFlush    Type of flush
 * @param   GCPhys      Physical address of the page to flush
 */
static void vmxR0FlushEPT(PVM pVM, PVMCPU pVCpu, VMX_FLUSH enmFlush, RTGCPHYS GCPhys)
{
    uint64_t descriptor[2];

    LogFlow(("vmxR0FlushEPT %d %RGv\n", enmFlush, GCPhys));
    Assert(pVM->hwaccm.s.fNestedPaging);
    descriptor[0] = pVCpu->hwaccm.s.vmx.GCPhysEPTP;
    descriptor[1] = GCPhys;
    int rc = VMXR0InvEPT(enmFlush, &descriptor[0]);
    AssertRC(rc);
}

#ifdef HWACCM_VTX_WITH_VPID
/**
 * Flush the TLB (EPT)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   enmFlush    Type of flush
 * @param   GCPtr       Virtual address of the page to flush
 */
static void vmxR0FlushVPID(PVM pVM, PVMCPU pVCpu, VMX_FLUSH enmFlush, RTGCPTR GCPtr)
{
#if HC_ARCH_BITS == 32
    /* If we get a flush in 64 bits guest mode, then force a full TLB flush. Invvpid probably takes only 32 bits addresses. (@todo) */
    if (   CPUMIsGuestInLongMode(pVCpu)
        && !VMX_IS_64BIT_HOST_MODE())
    {
        pVCpu->hwaccm.s.fForceTLBFlush = true;
    }
    else
#endif
    {
        uint64_t descriptor[2];

        Assert(pVM->hwaccm.s.vmx.fVPID);
        descriptor[0] = pVCpu->hwaccm.s.uCurrentASID;
        descriptor[1] = GCPtr;
        int rc = VMXR0InvVPID(enmFlush, &descriptor[0]);
        AssertRC(rc);
    }
}
#endif /* HWACCM_VTX_WITH_VPID */

/**
 * Invalidates a guest page
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   GCVirt      Page to invalidate
 */
VMMR0DECL(int) VMXR0InvalidatePage(PVM pVM, PVMCPU pVCpu, RTGCPTR GCVirt)
{
    bool fFlushPending = pVCpu->hwaccm.s.fForceTLBFlush;

    Log2(("VMXR0InvalidatePage %RGv\n", GCVirt));

    /* Only relevant if we want to use VPID.
     * In the nested paging case we still see such calls, but
     * can safely ignore them. (e.g. after cr3 updates)
     */
#ifdef HWACCM_VTX_WITH_VPID
    /* Skip it if a TLB flush is already pending. */
    if (   !fFlushPending
        && pVM->hwaccm.s.vmx.fVPID)
        vmxR0FlushVPID(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushPage, GCVirt);
#endif /* HWACCM_VTX_WITH_VPID */

    return VINF_SUCCESS;
}

/**
 * Invalidates a guest page by physical address
 *
 * NOTE: Assumes the current instruction references this physical page though a virtual address!!
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VM CPU to operate on.
 * @param   GCPhys      Page to invalidate
 */
VMMR0DECL(int) VMXR0InvalidatePhysPage(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys)
{
    bool fFlushPending = pVCpu->hwaccm.s.fForceTLBFlush;

    Assert(pVM->hwaccm.s.fNestedPaging);

    LogFlow(("VMXR0InvalidatePhysPage %RGp\n", GCPhys));

    /* Skip it if a TLB flush is already pending. */
    if (!fFlushPending)
        vmxR0FlushEPT(pVM, pVCpu, pVM->hwaccm.s.vmx.enmFlushPage, GCPhys);

    return VINF_SUCCESS;
}

/**
 * Report world switch error and dump some useful debug info
 *
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   rc          Return code
 * @param   pCtx        Current CPU context (not updated)
 */
static void VMXR0ReportWorldSwitchError(PVM pVM, PVMCPU pVCpu, int rc, PCPUMCTX pCtx)
{
    switch (rc)
    {
    case VERR_VMX_INVALID_VMXON_PTR:
        AssertFailed();
        break;

    case VERR_VMX_UNABLE_TO_START_VM:
    case VERR_VMX_UNABLE_TO_RESUME_VM:
    {
        int         rc;
        RTCCUINTREG exitReason, instrError;

        rc  = VMXReadVMCS(VMX_VMCS32_RO_EXIT_REASON, &exitReason);
        rc |= VMXReadVMCS(VMX_VMCS32_RO_VM_INSTR_ERROR, &instrError);
        AssertRC(rc);
        if (rc == VINF_SUCCESS)
        {
            Log(("Unable to start/resume VM for reason: %x. Instruction error %x\n", (uint32_t)exitReason, (uint32_t)instrError));
            Log(("Current stack %08x\n", &rc));

            pVCpu->hwaccm.s.vmx.lasterror.ulInstrError = instrError;
            pVCpu->hwaccm.s.vmx.lasterror.ulExitReason = exitReason;

#ifdef VBOX_STRICT
            RTGDTR      gdtr;
            PX86DESCHC  pDesc;
            RTCCUINTREG val;

            ASMGetGDTR(&gdtr);

            VMXReadVMCS(VMX_VMCS64_GUEST_RIP, &val);
            Log(("Old eip %RGv new %RGv\n", (RTGCPTR)pCtx->rip, (RTGCPTR)val));
            VMXReadVMCS(VMX_VMCS_CTRL_PIN_EXEC_CONTROLS, &val);
            Log(("VMX_VMCS_CTRL_PIN_EXEC_CONTROLS   %08x\n", val));
            VMXReadVMCS(VMX_VMCS_CTRL_PROC_EXEC_CONTROLS, &val);
            Log(("VMX_VMCS_CTRL_PROC_EXEC_CONTROLS  %08x\n", val));
            VMXReadVMCS(VMX_VMCS_CTRL_ENTRY_CONTROLS, &val);
            Log(("VMX_VMCS_CTRL_ENTRY_CONTROLS      %08x\n", val));
            VMXReadVMCS(VMX_VMCS_CTRL_EXIT_CONTROLS, &val);
            Log(("VMX_VMCS_CTRL_EXIT_CONTROLS       %08x\n", val));

            VMXReadVMCS(VMX_VMCS_HOST_CR0, &val);
            Log(("VMX_VMCS_HOST_CR0 %08x\n", val));

            VMXReadVMCS(VMX_VMCS_HOST_CR3, &val);
            Log(("VMX_VMCS_HOST_CR3 %08x\n", val));

            VMXReadVMCS(VMX_VMCS_HOST_CR4, &val);
            Log(("VMX_VMCS_HOST_CR4 %08x\n", val));

            VMXReadVMCS(VMX_VMCS16_HOST_FIELD_CS, &val);
            Log(("VMX_VMCS_HOST_FIELD_CS %08x\n", val));

            VMXReadVMCS(VMX_VMCS_GUEST_RFLAGS, &val);
            Log(("VMX_VMCS_GUEST_RFLAGS %08x\n", val));

            if (val < gdtr.cbGdt)
            {
                pDesc  = &((PX86DESCHC)gdtr.pGdt)[val >> X86_SEL_SHIFT_HC];
                HWACCMR0DumpDescriptor(pDesc, val, "CS: ");
            }

            VMXReadVMCS(VMX_VMCS16_HOST_FIELD_DS, &val);
            Log(("VMX_VMCS_HOST_FIELD_DS %08x\n", val));
            if (val < gdtr.cbGdt)
            {
                pDesc  = &((PX86DESCHC)gdtr.pGdt)[val >> X86_SEL_SHIFT_HC];
                HWACCMR0DumpDescriptor(pDesc, val, "DS: ");
            }

            VMXReadVMCS(VMX_VMCS16_HOST_FIELD_ES, &val);
            Log(("VMX_VMCS_HOST_FIELD_ES %08x\n", val));
            if (val < gdtr.cbGdt)
            {
                pDesc  = &((PX86DESCHC)gdtr.pGdt)[val >> X86_SEL_SHIFT_HC];
                HWACCMR0DumpDescriptor(pDesc, val, "ES: ");
            }

            VMXReadVMCS(VMX_VMCS16_HOST_FIELD_FS, &val);
            Log(("VMX_VMCS16_HOST_FIELD_FS %08x\n", val));
            if (val < gdtr.cbGdt)
            {
                pDesc  = &((PX86DESCHC)gdtr.pGdt)[val >> X86_SEL_SHIFT_HC];
                HWACCMR0DumpDescriptor(pDesc, val, "FS: ");
            }

            VMXReadVMCS(VMX_VMCS16_HOST_FIELD_GS, &val);
            Log(("VMX_VMCS16_HOST_FIELD_GS %08x\n", val));
            if (val < gdtr.cbGdt)
            {
                pDesc  = &((PX86DESCHC)gdtr.pGdt)[val >> X86_SEL_SHIFT_HC];
                HWACCMR0DumpDescriptor(pDesc, val, "GS: ");
            }

            VMXReadVMCS(VMX_VMCS16_HOST_FIELD_SS, &val);
            Log(("VMX_VMCS16_HOST_FIELD_SS %08x\n", val));
            if (val < gdtr.cbGdt)
            {
                pDesc  = &((PX86DESCHC)gdtr.pGdt)[val >> X86_SEL_SHIFT_HC];
                HWACCMR0DumpDescriptor(pDesc, val, "SS: ");
            }

            VMXReadVMCS(VMX_VMCS16_HOST_FIELD_TR, &val);
            Log(("VMX_VMCS16_HOST_FIELD_TR %08x\n", val));
            if (val < gdtr.cbGdt)
            {
                pDesc  = &((PX86DESCHC)gdtr.pGdt)[val >> X86_SEL_SHIFT_HC];
                HWACCMR0DumpDescriptor(pDesc, val, "TR: ");
            }

            VMXReadVMCS(VMX_VMCS_HOST_TR_BASE, &val);
            Log(("VMX_VMCS_HOST_TR_BASE %RHv\n", val));

            VMXReadVMCS(VMX_VMCS_HOST_GDTR_BASE, &val);
            Log(("VMX_VMCS_HOST_GDTR_BASE %RHv\n", val));
            VMXReadVMCS(VMX_VMCS_HOST_IDTR_BASE, &val);
            Log(("VMX_VMCS_HOST_IDTR_BASE %RHv\n", val));

            VMXReadVMCS(VMX_VMCS32_HOST_SYSENTER_CS, &val);
            Log(("VMX_VMCS_HOST_SYSENTER_CS  %08x\n", val));

            VMXReadVMCS(VMX_VMCS_HOST_SYSENTER_EIP, &val);
            Log(("VMX_VMCS_HOST_SYSENTER_EIP %RHv\n", val));

            VMXReadVMCS(VMX_VMCS_HOST_SYSENTER_ESP, &val);
            Log(("VMX_VMCS_HOST_SYSENTER_ESP %RHv\n", val));

            VMXReadVMCS(VMX_VMCS_HOST_RSP, &val);
            Log(("VMX_VMCS_HOST_RSP %RHv\n", val));
            VMXReadVMCS(VMX_VMCS_HOST_RIP, &val);
            Log(("VMX_VMCS_HOST_RIP %RHv\n", val));

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
        AssertMsgFailed(("%Rrc (%#x)\n", rc, rc));
        break;
    }
}

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
/**
 * Prepares for and executes VMLAUNCH (64 bits guest mode)
 *
 * @returns VBox status code
 * @param   fResume     vmlauch/vmresume
 * @param   pCtx        Guest context
 * @param   pCache      VMCS cache
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 */
DECLASM(int) VMXR0SwitcherStartVM64(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu)
{
    uint32_t        aParam[6];
    PHWACCM_CPUINFO pCpu;
    RTHCPHYS        pPageCpuPhys;
    int             rc;

    pCpu = HWACCMR0GetCurrentCpu();
    pPageCpuPhys = RTR0MemObjGetPagePhysAddr(pCpu->pMemObj, 0);

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    pCache->uPos = 1;
    pCache->interPD = PGMGetInterPaeCR3(pVM);
    pCache->pSwitcher = (uint64_t)pVM->hwaccm.s.pfnHost32ToGuest64R0;
#endif

#ifdef DEBUG
    pCache->TestIn.pPageCpuPhys = 0;
    pCache->TestIn.pVMCSPhys    = 0;
    pCache->TestIn.pCache       = 0;
    pCache->TestOut.pVMCSPhys   = 0;
    pCache->TestOut.pCache      = 0;
    pCache->TestOut.pCtx        = 0;
    pCache->TestOut.eflags      = 0;
#endif

    aParam[0] = (uint32_t)(pPageCpuPhys);                                   /* Param 1: VMXON physical address - Lo. */
    aParam[1] = (uint32_t)(pPageCpuPhys >> 32);                             /* Param 1: VMXON physical address - Hi. */
    aParam[2] = (uint32_t)(pVCpu->hwaccm.s.vmx.pVMCSPhys);                  /* Param 2: VMCS physical address - Lo. */
    aParam[3] = (uint32_t)(pVCpu->hwaccm.s.vmx.pVMCSPhys >> 32);            /* Param 2: VMCS physical address - Hi. */
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
    AssertMsg(pCache->TestIn.pPageCpuPhys == pPageCpuPhys, ("%RHp vs %RHp\n", pCache->TestIn.pPageCpuPhys, pPageCpuPhys));
    AssertMsg(pCache->TestIn.pVMCSPhys    == pVCpu->hwaccm.s.vmx.pVMCSPhys, ("%RHp vs %RHp\n", pCache->TestIn.pVMCSPhys, pVCpu->hwaccm.s.vmx.pVMCSPhys));
    AssertMsg(pCache->TestIn.pVMCSPhys    == pCache->TestOut.pVMCSPhys, ("%RHp vs %RHp\n", pCache->TestIn.pVMCSPhys, pCache->TestOut.pVMCSPhys));
    AssertMsg(pCache->TestIn.pCache       == pCache->TestOut.pCache, ("%RGv vs %RGv\n", pCache->TestIn.pCache, pCache->TestOut.pCache));
    AssertMsg(pCache->TestIn.pCache       == VM_RC_ADDR(pVM, &pVM->aCpus[pVCpu->idCpu].hwaccm.s.vmx.VMCSCache), ("%RGv vs %RGv\n", pCache->TestIn.pCache, VM_RC_ADDR(pVM, &pVM->aCpus[pVCpu->idCpu].hwaccm.s.vmx.VMCSCache)));
    AssertMsg(pCache->TestIn.pCtx         == pCache->TestOut.pCtx, ("%RGv vs %RGv\n", pCache->TestIn.pCtx, pCache->TestOut.pCtx));
    Assert(!(pCache->TestOut.eflags & X86_EFL_IF));
#endif
    return rc;
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
VMMR0DECL(int) VMXR0Execute64BitsHandler(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, RTRCPTR pfnHandler, uint32_t cbParam, uint32_t *paParam)
{
    int             rc, rc2;
    PHWACCM_CPUINFO pCpu;
    RTHCPHYS        pPageCpuPhys;

    /* @todo This code is not guest SMP safe (hyper stack) */
    AssertReturn(pVM->cCPUs == 1, VERR_ACCESS_DENIED);
    AssertReturn(pVM->hwaccm.s.pfnHost32ToGuest64R0, VERR_INTERNAL_ERROR);
    Assert(pVCpu->hwaccm.s.vmx.VMCSCache.Write.cValidEntries <= RT_ELEMENTS(pVCpu->hwaccm.s.vmx.VMCSCache.Write.aField));
    Assert(pVCpu->hwaccm.s.vmx.VMCSCache.Read.cValidEntries <= RT_ELEMENTS(pVCpu->hwaccm.s.vmx.VMCSCache.Read.aField));

#ifdef VBOX_STRICT
    for (unsigned i=0;i<pVCpu->hwaccm.s.vmx.VMCSCache.Write.cValidEntries;i++)
        Assert(vmxR0IsValidWriteField(pVCpu->hwaccm.s.vmx.VMCSCache.Write.aField[i]));

    for (unsigned i=0;i<pVCpu->hwaccm.s.vmx.VMCSCache.Read.cValidEntries;i++)
        Assert(vmxR0IsValidReadField(pVCpu->hwaccm.s.vmx.VMCSCache.Read.aField[i]));
#endif

    pCpu = HWACCMR0GetCurrentCpu();
    pPageCpuPhys = RTR0MemObjGetPagePhysAddr(pCpu->pMemObj, 0);

    /* Clear VM Control Structure. Marking it inactive, clearing implementation specific data and writing back VMCS data to memory. */
    VMXClearVMCS(pVCpu->hwaccm.s.vmx.pVMCSPhys);

    /* Leave VMX Root Mode. */
    VMXDisable();

    ASMSetCR4(ASMGetCR4() & ~X86_CR4_VMXE);

    CPUMSetHyperESP(pVCpu, VMMGetStackRC(pVM));
    CPUMSetHyperEIP(pVCpu, pfnHandler);
    for (int i=(int)cbParam-1;i>=0;i--)
        CPUMPushHyper(pVCpu, paParam[i]);

    STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatWorldSwitch3264, z);
    /* Call switcher. */
    rc = pVM->hwaccm.s.pfnHost32ToGuest64R0(pVM);
    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatWorldSwitch3264, z);

    /* Make sure the VMX instructions don't cause #UD faults. */
    ASMSetCR4(ASMGetCR4() | X86_CR4_VMXE);

    /* Enter VMX Root Mode */
    rc2 = VMXEnable(pPageCpuPhys);
    if (RT_FAILURE(rc2))
    {
        if (pVM)
            VMXR0CheckError(pVM, pVCpu, rc2);
        ASMSetCR4(ASMGetCR4() & ~X86_CR4_VMXE);
        return VERR_VMX_VMXON_FAILED;
    }

    rc2 = VMXActivateVMCS(pVCpu->hwaccm.s.vmx.pVMCSPhys);
    AssertRCReturn(rc2, rc2);
#ifdef RT_OS_WINDOWS
    Assert(ASMGetFlags() & X86_EFL_IF);
#else
    Assert(!(ASMGetFlags() & X86_EFL_IF));
#endif
    return rc;
}

#endif /* HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL) */


#if HC_ARCH_BITS == 32 && !defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
/**
 * Executes VMWRITE
 *
 * @returns VBox status code
 * @param   pVCpu           The VMCPU to operate on.
 * @param   idxField        VMCS index
 * @param   u64Val          16, 32 or 64 bits value
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
 * @param   pVCpu       The VMCPU to operate on.
 * @param   idxField    VMCS field
 * @param   u64Val      Value
 */
VMMR0DECL(int) VMXWriteCachedVMCSEx(PVMCPU pVCpu, uint32_t idxField, uint64_t u64Val)
{
    PVMCSCACHE pCache = &pVCpu->hwaccm.s.vmx.VMCSCache;

    AssertMsgReturn(pCache->Write.cValidEntries < VMCSCACHE_MAX_ENTRY - 1, ("entries=%x\n", pCache->Write.cValidEntries), VERR_ACCESS_DENIED);

    /* Make sure there are no duplicates. */
    for (unsigned i=0;i<pCache->Write.cValidEntries;i++)
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

#ifdef VBOX_STRICT
static bool vmxR0IsValidReadField(uint32_t idxField)
{
    switch(idxField)
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

static bool vmxR0IsValidWriteField(uint32_t idxField)
{
    switch(idxField)
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

#endif
