/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-3 macOS backend using Hypervisor.framework.
 *
 * Log group 2: Exit logging.
 * Log group 3: Log context on exit.
 * Log group 5: Ring-3 memory management
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#define VMCPU_INCL_CPUM_GST_CTX
#include <Hypervisor/hv.h>
#include <Hypervisor/hv_arch_x86.h>
#include <Hypervisor/hv_arch_vmx.h>
#include <Hypervisor/hv_vmx.h>

#include <VBox/vmm/nem.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/apic.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/hm_vmx.h>
#include <VBox/vmm/dbgftrace.h>
#include "VMXInternal.h"
#include "NEMInternal.h"
#include <VBox/vmm/vmcc.h>
#include "dtrace/VBoxVMM.h"

#include <iprt/asm.h>
#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/utf16.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/* No nested hwvirt (for now). */
#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
# undef VBOX_WITH_NESTED_HWVIRT_VMX
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** NEM_DARWIN_PAGE_STATE_XXX names. */
NEM_TMPL_STATIC const char * const g_apszPageStates[4] = { "not-set", "unmapped", "readable", "writable" };
/** MSRs. */
SUPHWVIRTMSRS           g_HmMsrs;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Converts a HV return code to a VBox status code.
 *
 * @returns VBox status code.
 * @param   hrc                 The HV return code to convert.
 */
DECLINLINE(int) nemR3DarwinHvSts2Rc(hv_return_t hrc)
{
    if (hrc == HV_SUCCESS)
        return VINF_SUCCESS;

    switch (hrc)
    {
        case HV_ERROR:        return VERR_INVALID_STATE;
        case HV_BUSY:         return VERR_RESOURCE_BUSY;
        case HV_BAD_ARGUMENT: return VERR_INVALID_PARAMETER;
        case HV_NO_RESOURCES: return VERR_OUT_OF_RESOURCES;
        case HV_NO_DEVICE:    return VERR_NOT_FOUND;
        case HV_UNSUPPORTED:  return VERR_NOT_SUPPORTED;
    }

    return VERR_IPE_UNEXPECTED_STATUS;
}


/**
 * Unmaps the given guest physical address range (page aligned).
 *
 * @returns VBox status code.
 * @param   GCPhys              The guest physical address to start unmapping at.
 * @param   cb                  The size of the range to unmap in bytes.
 */
DECLINLINE(int) nemR3DarwinUnmap(RTGCPHYS GCPhys, size_t cb)
{
    LogFlowFunc(("Unmapping %RGp LB %zu\n", GCPhys, cb));
    hv_return_t hrc = hv_vm_unmap(GCPhys, cb);
    return nemR3DarwinHvSts2Rc(hrc);
}


/**
 * Maps a given guest physical address range backed by the given memory with the given
 * protection flags.
 *
 * @returns VBox status code.
 * @param   GCPhys              The guest physical address to start mapping.
 * @param   pvRam               The R3 pointer of the memory to back the range with.
 * @param   cb                  The size of the range, page aligned.
 * @param   fPageProt           The page protection flags to use for this range, combination of NEM_PAGE_PROT_XXX
 */
DECLINLINE(int) nemR3DarwinMap(RTGCPHYS GCPhys, void *pvRam, size_t cb, uint32_t fPageProt)
{
    LogFlowFunc(("Mapping %RGp LB %zu fProt=%#x\n", GCPhys, cb, fPageProt));

    hv_memory_flags_t fHvMemProt = 0;
    if (fPageProt & NEM_PAGE_PROT_READ)
        fHvMemProt |= HV_MEMORY_READ;
    if (fPageProt & NEM_PAGE_PROT_WRITE)
        fHvMemProt |= HV_MEMORY_WRITE;
    if (fPageProt & NEM_PAGE_PROT_EXECUTE)
        fHvMemProt |= HV_MEMORY_EXEC;

    hv_return_t hrc = hv_vm_map(pvRam, GCPhys, cb, fHvMemProt);
    return nemR3DarwinHvSts2Rc(hrc);
}


#if 0 /* unused */
DECLINLINE(int) nemR3DarwinProtectPage(RTGCPHYS GCPhys, size_t cb, uint32_t fPageProt)
{
    hv_memory_flags_t fHvMemProt = 0;
    if (fPageProt & NEM_PAGE_PROT_READ)
        fHvMemProt |= HV_MEMORY_READ;
    if (fPageProt & NEM_PAGE_PROT_WRITE)
        fHvMemProt |= HV_MEMORY_WRITE;
    if (fPageProt & NEM_PAGE_PROT_EXECUTE)
        fHvMemProt |= HV_MEMORY_EXEC;

    hv_return_t hrc = hv_vm_protect(GCPhys, cb, fHvMemProt);
    return nemR3DarwinHvSts2Rc(hrc);
}
#endif


DECLINLINE(int) nemR3NativeGCPhys2R3PtrReadOnly(PVM pVM, RTGCPHYS GCPhys, const void **ppv)
{
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys, ppv, &Lock);
    if (RT_SUCCESS(rc))
        PGMPhysReleasePageMappingLock(pVM, &Lock);
    return rc;
}


DECLINLINE(int) nemR3NativeGCPhys2R3PtrWriteable(PVM pVM, RTGCPHYS GCPhys, void **ppv)
{
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPhys2CCPtr(pVM, GCPhys, ppv, &Lock);
    if (RT_SUCCESS(rc))
        PGMPhysReleasePageMappingLock(pVM, &Lock);
    return rc;
}


/**
 * Worker that maps pages into Hyper-V.
 *
 * This is used by the PGM physical page notifications as well as the memory
 * access VMEXIT handlers.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 * @param   GCPhysSrc       The source page address.
 * @param   GCPhysDst       The hyper-V destination page.  This may differ from
 *                          GCPhysSrc when A20 is disabled.
 * @param   fPageProt       NEM_PAGE_PROT_XXX.
 * @param   pu2State        Our page state (input/output).
 * @param   fBackingChanged Set if the page backing is being changed.
 * @thread  EMT(pVCpu)
 */
NEM_TMPL_STATIC int nemHCNativeSetPhysPage(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhysSrc, RTGCPHYS GCPhysDst,
                                           uint32_t fPageProt, uint8_t *pu2State, bool fBackingChanged)
{
    /*
     * Looks like we need to unmap a page before we can change the backing
     * or even modify the protection.  This is going to be *REALLY* efficient.
     * PGM lends us two bits to keep track of the state here.
     */
    RT_NOREF(pVCpu);
    uint8_t const u2OldState = *pu2State;
    uint8_t const u2NewState = fPageProt & NEM_PAGE_PROT_WRITE ? NEM_DARWIN_PAGE_STATE_WRITABLE
                             : fPageProt & NEM_PAGE_PROT_READ  ? NEM_DARWIN_PAGE_STATE_READABLE : NEM_DARWIN_PAGE_STATE_UNMAPPED;
    if (   fBackingChanged
        || u2NewState != u2OldState)
    {
        if (u2OldState > NEM_DARWIN_PAGE_STATE_UNMAPPED)
        {
            int rc = nemR3DarwinUnmap(GCPhysDst, X86_PAGE_SIZE);
            if (RT_SUCCESS(rc))
            {
                *pu2State = NEM_DARWIN_PAGE_STATE_UNMAPPED;
                STAM_REL_COUNTER_INC(&pVM->nem.s.StatUnmapPage);
                uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
                if (u2NewState == NEM_DARWIN_PAGE_STATE_UNMAPPED)
                {
                    Log5(("NEM GPA unmapped/set: %RGp (was %s, cMappedPages=%u)\n",
                          GCPhysDst, g_apszPageStates[u2OldState], cMappedPages));
                    return VINF_SUCCESS;
                }
            }
            else
            {
                STAM_REL_COUNTER_INC(&pVM->nem.s.StatUnmapPageFailed);
                LogRel(("nemHCNativeSetPhysPage/unmap: GCPhysDst=%RGp rc=%Rrc\n", GCPhysDst, rc));
                return VERR_NEM_INIT_FAILED;
            }
        }
    }

    /*
     * Writeable mapping?
     */
    if (fPageProt & NEM_PAGE_PROT_WRITE)
    {
        void *pvPage;
        int rc = nemR3NativeGCPhys2R3PtrWriteable(pVM, GCPhysSrc, &pvPage);
        if (RT_SUCCESS(rc))
        {
            rc = nemR3DarwinMap(GCPhysDst, pvPage, X86_PAGE_SIZE, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_WRITE | NEM_PAGE_PROT_EXECUTE);
            if (RT_SUCCESS(rc))
            {
                *pu2State = NEM_DARWIN_PAGE_STATE_WRITABLE;
                STAM_REL_COUNTER_INC(&pVM->nem.s.StatMapPage);
                uint32_t cMappedPages = ASMAtomicIncU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
                Log5(("NEM GPA mapped/set: %RGp %s (was %s, cMappedPages=%u)\n",
                      GCPhysDst, g_apszPageStates[u2NewState], g_apszPageStates[u2OldState], cMappedPages));
                return VINF_SUCCESS;
            }
            STAM_REL_COUNTER_INC(&pVM->nem.s.StatMapPageFailed);
            LogRel(("nemHCNativeSetPhysPage/writable: GCPhysDst=%RGp rc=%Rrc\n", GCPhysDst));
            return VERR_NEM_INIT_FAILED;
        }
        LogRel(("nemHCNativeSetPhysPage/writable: GCPhysSrc=%RGp rc=%Rrc\n", GCPhysSrc, rc));
        return rc;
    }

    if (fPageProt & NEM_PAGE_PROT_READ)
    {
        const void *pvPage;
        int rc = nemR3NativeGCPhys2R3PtrReadOnly(pVM, GCPhysSrc, &pvPage);
        if (RT_SUCCESS(rc))
        {
            rc = nemR3DarwinMap(GCPhysDst, (void *)pvPage, X86_PAGE_SIZE, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_EXECUTE);
            if (RT_SUCCESS(rc))
            {
                *pu2State = NEM_DARWIN_PAGE_STATE_READABLE;
                STAM_REL_COUNTER_INC(&pVM->nem.s.StatMapPage);
                uint32_t cMappedPages = ASMAtomicIncU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
                Log5(("NEM GPA mapped/set: %RGp %s (was %s, cMappedPages=%u)\n",
                      GCPhysDst, g_apszPageStates[u2NewState], g_apszPageStates[u2OldState], cMappedPages));
                return VINF_SUCCESS;
            }
            STAM_REL_COUNTER_INC(&pVM->nem.s.StatMapPageFailed);
            LogRel(("nemHCNativeSetPhysPage/readonly: GCPhysDst=%RGp rc=%Rrc\n", GCPhysDst, rc));
            return VERR_NEM_INIT_FAILED;
        }
        LogRel(("nemHCNativeSetPhysPage/readonly: GCPhysSrc=%RGp rc=%Rrc\n", GCPhysSrc, rc));
        return rc;
    }

    /* We already unmapped it above. */
    *pu2State = NEM_DARWIN_PAGE_STATE_UNMAPPED;
    return VINF_SUCCESS;
}


#ifdef LOG_ENABLED
/**
 * Logs the current CPU state.
 */
static void nemR3DarwinLogState(PVMCC pVM, PVMCPUCC pVCpu)
{
    if (LogIs3Enabled())
    {
#if 0
        char szRegs[4096];
        DBGFR3RegPrintf(pVM->pUVM, pVCpu->idCpu, &szRegs[0], sizeof(szRegs),
                        "rax=%016VR{rax} rbx=%016VR{rbx} rcx=%016VR{rcx} rdx=%016VR{rdx}\n"
                        "rsi=%016VR{rsi} rdi=%016VR{rdi} r8 =%016VR{r8} r9 =%016VR{r9}\n"
                        "r10=%016VR{r10} r11=%016VR{r11} r12=%016VR{r12} r13=%016VR{r13}\n"
                        "r14=%016VR{r14} r15=%016VR{r15} %VRF{rflags}\n"
                        "rip=%016VR{rip} rsp=%016VR{rsp} rbp=%016VR{rbp}\n"
                        "cs={%04VR{cs} base=%016VR{cs_base} limit=%08VR{cs_lim} flags=%04VR{cs_attr}} cr0=%016VR{cr0}\n"
                        "ds={%04VR{ds} base=%016VR{ds_base} limit=%08VR{ds_lim} flags=%04VR{ds_attr}} cr2=%016VR{cr2}\n"
                        "es={%04VR{es} base=%016VR{es_base} limit=%08VR{es_lim} flags=%04VR{es_attr}} cr3=%016VR{cr3}\n"
                        "fs={%04VR{fs} base=%016VR{fs_base} limit=%08VR{fs_lim} flags=%04VR{fs_attr}} cr4=%016VR{cr4}\n"
                        "gs={%04VR{gs} base=%016VR{gs_base} limit=%08VR{gs_lim} flags=%04VR{gs_attr}} cr8=%016VR{cr8}\n"
                        "ss={%04VR{ss} base=%016VR{ss_base} limit=%08VR{ss_lim} flags=%04VR{ss_attr}}\n"
                        "dr0=%016VR{dr0} dr1=%016VR{dr1} dr2=%016VR{dr2} dr3=%016VR{dr3}\n"
                        "dr6=%016VR{dr6} dr7=%016VR{dr7}\n"
                        "gdtr=%016VR{gdtr_base}:%04VR{gdtr_lim}  idtr=%016VR{idtr_base}:%04VR{idtr_lim}  rflags=%08VR{rflags}\n"
                        "ldtr={%04VR{ldtr} base=%016VR{ldtr_base} limit=%08VR{ldtr_lim} flags=%08VR{ldtr_attr}}\n"
                        "tr  ={%04VR{tr} base=%016VR{tr_base} limit=%08VR{tr_lim} flags=%08VR{tr_attr}}\n"
                        "    sysenter={cs=%04VR{sysenter_cs} eip=%08VR{sysenter_eip} esp=%08VR{sysenter_esp}}\n"
                        "        efer=%016VR{efer}\n"
                        "         pat=%016VR{pat}\n"
                        "     sf_mask=%016VR{sf_mask}\n"
                        "krnl_gs_base=%016VR{krnl_gs_base}\n"
                        "       lstar=%016VR{lstar}\n"
                        "        star=%016VR{star} cstar=%016VR{cstar}\n"
                        "fcw=%04VR{fcw} fsw=%04VR{fsw} ftw=%04VR{ftw} mxcsr=%04VR{mxcsr} mxcsr_mask=%04VR{mxcsr_mask}\n"
                        );

        char szInstr[256];
        DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, 0, 0,
                           DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                           szInstr, sizeof(szInstr), NULL);
        Log3(("%s%s\n", szRegs, szInstr));
#else
        RT_NOREF(pVM, pVCpu);
#endif
    }
}
#endif /* LOG_ENABLED */


DECLINLINE(int) nemR3DarwinReadVmcs16(PVMCPUCC pVCpu, uint32_t uFieldEnc, uint16_t *pData)
{
    uint64_t u64Data;
    hv_return_t hrc = hv_vmx_vcpu_read_vmcs(pVCpu->nem.s.hVCpuId, uFieldEnc, &u64Data);
    if (RT_LIKELY(hrc == HV_SUCCESS))
    {
        *pData = (uint16_t)u64Data;
        return VINF_SUCCESS;
    }

    return nemR3DarwinHvSts2Rc(hrc);
}


DECLINLINE(int) nemR3DarwinReadVmcs32(PVMCPUCC pVCpu, uint32_t uFieldEnc, uint32_t *pData)
{
    uint64_t u64Data;
    hv_return_t hrc = hv_vmx_vcpu_read_vmcs(pVCpu->nem.s.hVCpuId, uFieldEnc, &u64Data);
    if (RT_LIKELY(hrc == HV_SUCCESS))
    {
        *pData = (uint32_t)u64Data;
        return VINF_SUCCESS;
    }

    return nemR3DarwinHvSts2Rc(hrc);
}


DECLINLINE(int) nemR3DarwinReadVmcs64(PVMCPUCC pVCpu, uint32_t uFieldEnc, uint64_t *pData)
{
    hv_return_t hrc = hv_vmx_vcpu_read_vmcs(pVCpu->nem.s.hVCpuId, uFieldEnc, pData);
    if (RT_LIKELY(hrc == HV_SUCCESS))
        return VINF_SUCCESS;

    return nemR3DarwinHvSts2Rc(hrc);
}


DECLINLINE(int) nemR3DarwinWriteVmcs16(PVMCPUCC pVCpu, uint32_t uFieldEnc, uint16_t u16Val)
{
    hv_return_t hrc = hv_vmx_vcpu_write_vmcs(pVCpu->nem.s.hVCpuId, uFieldEnc, u16Val);
    if (RT_LIKELY(hrc == HV_SUCCESS))
        return VINF_SUCCESS;

    return nemR3DarwinHvSts2Rc(hrc);
}


DECLINLINE(int) nemR3DarwinWriteVmcs32(PVMCPUCC pVCpu, uint32_t uFieldEnc, uint32_t u32Val)
{
    hv_return_t hrc = hv_vmx_vcpu_write_vmcs(pVCpu->nem.s.hVCpuId, uFieldEnc, u32Val);
    if (RT_LIKELY(hrc == HV_SUCCESS))
        return VINF_SUCCESS;

    return nemR3DarwinHvSts2Rc(hrc);
}


DECLINLINE(int) nemR3DarwinWriteVmcs64(PVMCPUCC pVCpu, uint32_t uFieldEnc, uint64_t u64Val)
{
    hv_return_t hrc = hv_vmx_vcpu_write_vmcs(pVCpu->nem.s.hVCpuId, uFieldEnc, u64Val);
    if (RT_LIKELY(hrc == HV_SUCCESS))
        return VINF_SUCCESS;

    return nemR3DarwinHvSts2Rc(hrc);
}


static int nemR3DarwinCopyStateToHv(PVMCC pVM, PVMCPUCC pVCpu)
{
#define WRITE_GREG(a_GReg, a_Value) \
    do \
    { \
        hrc = hv_vcpu_write_register(pVCpu->nem.s.hVCpuId, (a_GReg), (a_Value)); \
        if (RT_LIKELY(hrc == HV_SUCCESS)) \
        { /* likely */ } \
        else \
            return VERR_INTERNAL_ERROR; \
    } while(0)
#define WRITE_VMCS_FIELD(a_Field, a_Value) \
    do \
    { \
        hrc = hv_vmx_vcpu_write_vmcs(pVCpu->nem.s.hVCpuId, (a_Field), (a_Value)); \
        if (RT_LIKELY(hrc == HV_SUCCESS)) \
        { /* likely */ } \
        else \
            return VERR_INTERNAL_ERROR; \
    } while(0)

    RT_NOREF(pVM);

    uint64_t const fWhat = ~pVCpu->cpum.GstCtx.fExtrn & (CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK);
    if (!fWhat)
        return VINF_SUCCESS;

    hv_return_t hrc;
    if (fWhat & CPUMCTX_EXTRN_GPRS_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_RAX)
            WRITE_GREG(HV_X86_RAX, pVCpu->cpum.GstCtx.rax);
        if (fWhat & CPUMCTX_EXTRN_RCX)
            WRITE_GREG(HV_X86_RCX, pVCpu->cpum.GstCtx.rcx);
        if (fWhat & CPUMCTX_EXTRN_RDX)
            WRITE_GREG(HV_X86_RDX, pVCpu->cpum.GstCtx.rdx);
        if (fWhat & CPUMCTX_EXTRN_RBX)
            WRITE_GREG(HV_X86_RBX, pVCpu->cpum.GstCtx.rbx);
        if (fWhat & CPUMCTX_EXTRN_RSP)
            WRITE_GREG(HV_X86_RSP, pVCpu->cpum.GstCtx.rsp);
        if (fWhat & CPUMCTX_EXTRN_RBP)
            WRITE_GREG(HV_X86_RBP, pVCpu->cpum.GstCtx.rbp);
        if (fWhat & CPUMCTX_EXTRN_RSI)
            WRITE_GREG(HV_X86_RSI, pVCpu->cpum.GstCtx.rsi);
        if (fWhat & CPUMCTX_EXTRN_RDI)
            WRITE_GREG(HV_X86_RDI, pVCpu->cpum.GstCtx.rdi);
        if (fWhat & CPUMCTX_EXTRN_R8_R15)
        {
            WRITE_GREG(HV_X86_R8, pVCpu->cpum.GstCtx.r8);
            WRITE_GREG(HV_X86_R9, pVCpu->cpum.GstCtx.r9);
            WRITE_GREG(HV_X86_R10, pVCpu->cpum.GstCtx.r10);
            WRITE_GREG(HV_X86_R11, pVCpu->cpum.GstCtx.r11);
            WRITE_GREG(HV_X86_R12, pVCpu->cpum.GstCtx.r12);
            WRITE_GREG(HV_X86_R13, pVCpu->cpum.GstCtx.r13);
            WRITE_GREG(HV_X86_R14, pVCpu->cpum.GstCtx.r14);
            WRITE_GREG(HV_X86_R15, pVCpu->cpum.GstCtx.r15);
        }
    }

    /* RIP & Flags */
    if (fWhat & CPUMCTX_EXTRN_RIP)
        WRITE_GREG(HV_X86_RIP, pVCpu->cpum.GstCtx.rip);
    if (fWhat & CPUMCTX_EXTRN_RFLAGS)
        WRITE_GREG(HV_X86_RFLAGS, pVCpu->cpum.GstCtx.rflags.u);

    /* Segments */
#define ADD_SEG(a_enmName, a_SReg) \
        do { \
            WRITE_VMCS_FIELD(VMX_VMCS16_GUEST_ ## a_enmName ## _SEL,           (a_SReg).Sel); \
            WRITE_VMCS_FIELD(VMX_VMCS32_GUEST_ ## a_enmName ## _LIMIT,         (a_SReg).u32Limit); \
            WRITE_VMCS_FIELD(VMX_VMCS32_GUEST_ ## a_enmName ## _ACCESS_RIGHTS, (a_SReg).Attr.u); \
            WRITE_VMCS_FIELD(VMX_VMCS_GUEST_ ## a_enmName ## _BASE,            (a_SReg).u64Base); \
        } while (0)
    if (fWhat & CPUMCTX_EXTRN_SREG_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_ES)
            ADD_SEG(ES, pVCpu->cpum.GstCtx.es);
        if (fWhat & CPUMCTX_EXTRN_CS)
            ADD_SEG(CS, pVCpu->cpum.GstCtx.cs);
        if (fWhat & CPUMCTX_EXTRN_SS)
            ADD_SEG(SS, pVCpu->cpum.GstCtx.ss);
        if (fWhat & CPUMCTX_EXTRN_DS)
            ADD_SEG(DS, pVCpu->cpum.GstCtx.ds);
        if (fWhat & CPUMCTX_EXTRN_FS)
            ADD_SEG(FS, pVCpu->cpum.GstCtx.fs);
        if (fWhat & CPUMCTX_EXTRN_GS)
            ADD_SEG(GS, pVCpu->cpum.GstCtx.gs);
    }

    /* Descriptor tables & task segment. */
    if (fWhat & CPUMCTX_EXTRN_TABLE_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_LDTR)
            ADD_SEG(LDTR, pVCpu->cpum.GstCtx.ldtr);
        if (fWhat & CPUMCTX_EXTRN_TR)
            ADD_SEG(TR,   pVCpu->cpum.GstCtx.tr);
        if (fWhat & CPUMCTX_EXTRN_IDTR)
        {
            WRITE_VMCS_FIELD(VMCS_GUEST_IDTR_LIMIT, pVCpu->cpum.GstCtx.idtr.cbIdt);
            WRITE_VMCS_FIELD(VMCS_GUEST_IDTR_BASE,  pVCpu->cpum.GstCtx.idtr.pIdt);
        }
        if (fWhat & CPUMCTX_EXTRN_GDTR)
        {
            WRITE_VMCS_FIELD(VMCS_GUEST_GDTR_LIMIT, pVCpu->cpum.GstCtx.gdtr.cbGdt);
            WRITE_VMCS_FIELD(VMCS_GUEST_GDTR_BASE,  pVCpu->cpum.GstCtx.gdtr.pGdt);
        }
    }

    /* Control registers. */
    if (fWhat & CPUMCTX_EXTRN_CR_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_CR0)
        {
            uint64_t u64GuestCr0 = pVCpu->cpum.GstCtx.cr0;

            /* Apply the hardware specified CR0 fixed bits and enable caching. */
            u64GuestCr0 |= VMX_V_CR0_FIXED0_UX;
            u64GuestCr0 &= ~0;
            u64GuestCr0 &= ~(uint64_t)(X86_CR0_CD | X86_CR0_NW);
            WRITE_GREG(HV_X86_CR0, u64GuestCr0);
        }
        if (fWhat & CPUMCTX_EXTRN_CR2)
            WRITE_GREG(HV_X86_CR2, pVCpu->cpum.GstCtx.cr2);
        if (fWhat & CPUMCTX_EXTRN_CR3)
            WRITE_GREG(HV_X86_CR3, pVCpu->cpum.GstCtx.cr3);
        if (fWhat & CPUMCTX_EXTRN_CR4)
        {
            uint64_t u64GuestCr4 = pVCpu->cpum.GstCtx.cr4;

            u64GuestCr4 |= VMX_V_CR4_FIXED0;
            u64GuestCr4 &= ~0;

            WRITE_GREG(HV_X86_CR4, u64GuestCr4);
        }
    }
    if (fWhat & CPUMCTX_EXTRN_APIC_TPR)
        WRITE_GREG(HV_X86_TPR, CPUMGetGuestCR8(pVCpu));

    /* Debug registers. */
    if (fWhat & CPUMCTX_EXTRN_DR0_DR3)
    {
        WRITE_GREG(HV_X86_DR0, pVCpu->cpum.GstCtx.dr[0]); // CPUMGetHyperDR0(pVCpu));
        WRITE_GREG(HV_X86_DR1, pVCpu->cpum.GstCtx.dr[1]); // CPUMGetHyperDR1(pVCpu));
        WRITE_GREG(HV_X86_DR2, pVCpu->cpum.GstCtx.dr[2]); // CPUMGetHyperDR2(pVCpu));
        WRITE_GREG(HV_X86_DR3, pVCpu->cpum.GstCtx.dr[3]); // CPUMGetHyperDR3(pVCpu));
    }
    if (fWhat & CPUMCTX_EXTRN_DR6)
        WRITE_GREG(HV_X86_DR6, pVCpu->cpum.GstCtx.dr[6]); // CPUMGetHyperDR6(pVCpu));
    if (fWhat & CPUMCTX_EXTRN_DR7)
        WRITE_GREG(HV_X86_DR7, pVCpu->cpum.GstCtx.dr[7]); // CPUMGetHyperDR7(pVCpu));

    /* MSRs */
    // WHvX64RegisterTsc - don't touch
    if (fWhat & CPUMCTX_EXTRN_EFER)
        WRITE_VMCS_FIELD(VMCS_GUEST_IA32_EFER, pVCpu->cpum.GstCtx.msrEFER);
#if 0
    if (fWhat & CPUMCTX_EXTRN_KERNEL_GS_BASE)
        ADD_REG64(WHvX64RegisterKernelGsBase, pVCpu->cpum.GstCtx.msrKERNELGSBASE);
    if (fWhat & CPUMCTX_EXTRN_SYSENTER_MSRS)
    {
        ADD_REG64(WHvX64RegisterSysenterCs, pVCpu->cpum.GstCtx.SysEnter.cs);
        ADD_REG64(WHvX64RegisterSysenterEip, pVCpu->cpum.GstCtx.SysEnter.eip);
        ADD_REG64(WHvX64RegisterSysenterEsp, pVCpu->cpum.GstCtx.SysEnter.esp);
    }
    if (fWhat & CPUMCTX_EXTRN_SYSCALL_MSRS)
    {
        ADD_REG64(WHvX64RegisterStar, pVCpu->cpum.GstCtx.msrSTAR);
        ADD_REG64(WHvX64RegisterLstar, pVCpu->cpum.GstCtx.msrLSTAR);
        ADD_REG64(WHvX64RegisterCstar, pVCpu->cpum.GstCtx.msrCSTAR);
        ADD_REG64(WHvX64RegisterSfmask, pVCpu->cpum.GstCtx.msrSFMASK);
    }
    if (fWhat & CPUMCTX_EXTRN_OTHER_MSRS)
    {
        ADD_REG64(WHvX64RegisterApicBase, APICGetBaseMsrNoCheck(pVCpu));
        ADD_REG64(WHvX64RegisterPat, pVCpu->cpum.GstCtx.msrPAT);
#if 0 /** @todo check if WHvX64RegisterMsrMtrrCap works here... */
        ADD_REG64(WHvX64RegisterMsrMtrrCap, CPUMGetGuestIa32MtrrCap(pVCpu));
#endif
        PCPUMCTXMSRS pCtxMsrs = CPUMQueryGuestCtxMsrsPtr(pVCpu);
        ADD_REG64(WHvX64RegisterMsrMtrrDefType, pCtxMsrs->msr.MtrrDefType);
        ADD_REG64(WHvX64RegisterMsrMtrrFix64k00000, pCtxMsrs->msr.MtrrFix64K_00000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix16k80000, pCtxMsrs->msr.MtrrFix16K_80000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix16kA0000, pCtxMsrs->msr.MtrrFix16K_A0000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kC0000,  pCtxMsrs->msr.MtrrFix4K_C0000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kC8000,  pCtxMsrs->msr.MtrrFix4K_C8000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kD0000,  pCtxMsrs->msr.MtrrFix4K_D0000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kD8000,  pCtxMsrs->msr.MtrrFix4K_D8000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kE0000,  pCtxMsrs->msr.MtrrFix4K_E0000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kE8000,  pCtxMsrs->msr.MtrrFix4K_E8000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kF0000,  pCtxMsrs->msr.MtrrFix4K_F0000);
        ADD_REG64(WHvX64RegisterMsrMtrrFix4kF8000,  pCtxMsrs->msr.MtrrFix4K_F8000);
        ADD_REG64(WHvX64RegisterTscAux, pCtxMsrs->msr.TscAux);
#if 0 /** @todo these registers aren't available? Might explain something.. .*/
        const CPUMCPUVENDOR enmCpuVendor = CPUMGetHostCpuVendor(pVM);
        if (enmCpuVendor != CPUMCPUVENDOR_AMD)
        {
            ADD_REG64(HvX64RegisterIa32MiscEnable, pCtxMsrs->msr.MiscEnable);
            ADD_REG64(HvX64RegisterIa32FeatureControl, CPUMGetGuestIa32FeatureControl(pVCpu));
        }
#endif
    }
#endif

    WRITE_VMCS_FIELD(VMX_VMCS_CTRL_CR0_MASK, 0x60000000);
    WRITE_VMCS_FIELD(VMX_VMCS_CTRL_CR0_READ_SHADOW, 0x00000000);

    WRITE_VMCS_FIELD(VMX_VMCS_CTRL_CR4_MASK,        VMX_V_CR4_FIXED0);
    WRITE_VMCS_FIELD(VMX_VMCS_CTRL_CR4_READ_SHADOW, 0);

    WRITE_VMCS_FIELD(VMX_VMCS64_GUEST_DEBUGCTL_FULL, MSR_IA32_DEBUGCTL_LBR);

#if 0 /** @todo */
    WRITE_GREG(HV_X86_TSS_BASE, );
    WRITE_GREG(HV_X86_TSS_LIMIT, );
    WRITE_GREG(HV_X86_TSS_AR, );
    WRITE_GREG(HV_X86_XCR0, );
#endif

    hv_vcpu_invalidate_tlb(pVCpu->nem.s.hVCpuId);
    hv_vcpu_flush(pVCpu->nem.s.hVCpuId);

    pVCpu->cpum.GstCtx.fExtrn |= CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_NEM_WIN_MASK | CPUMCTX_EXTRN_KEEPER_NEM;
    return VINF_SUCCESS;
#undef WRITE_GREG
#undef WRITE_VMCS_FIELD
}


static int nemR3DarwinCopyStateFromHv(PVMCC pVM, PVMCPUCC pVCpu, uint64_t fWhat)
{
#define READ_GREG(a_GReg, a_Value) \
    do \
    { \
        hrc = hv_vcpu_read_register(pVCpu->nem.s.hVCpuId, (a_GReg), &(a_Value)); \
        if (RT_LIKELY(hrc == HV_SUCCESS)) \
        { /* likely */ } \
        else \
            return VERR_INTERNAL_ERROR; \
    } while(0)
#define READ_VMCS_FIELD(a_Field, a_Value) \
    do \
    { \
        hrc = hv_vmx_vcpu_read_vmcs(pVCpu->nem.s.hVCpuId, (a_Field), &(a_Value)); \
        if (RT_LIKELY(hrc == HV_SUCCESS)) \
        { /* likely */ } \
        else \
            return VERR_INTERNAL_ERROR; \
    } while(0)
#define READ_VMCS16_FIELD(a_Field, a_Value) \
    do \
    { \
        uint64_t u64Data; \
        hrc = hv_vmx_vcpu_read_vmcs(pVCpu->nem.s.hVCpuId, (a_Field), &u64Data); \
        if (RT_LIKELY(hrc == HV_SUCCESS)) \
        { (a_Value) = (uint16_t)u64Data; } \
        else \
            return VERR_INTERNAL_ERROR; \
    } while(0)
#define READ_VMCS32_FIELD(a_Field, a_Value) \
    do \
    { \
        uint64_t u64Data; \
        hrc = hv_vmx_vcpu_read_vmcs(pVCpu->nem.s.hVCpuId, (a_Field), &u64Data); \
        if (RT_LIKELY(hrc == HV_SUCCESS)) \
        { (a_Value) = (uint32_t)u64Data; } \
        else \
            return VERR_INTERNAL_ERROR; \
    } while(0)

    RT_NOREF(pVM);
    fWhat &= pVCpu->cpum.GstCtx.fExtrn;

    /* GPRs */
    hv_return_t hrc;
    if (fWhat & CPUMCTX_EXTRN_GPRS_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_RAX)
            READ_GREG(HV_X86_RAX, pVCpu->cpum.GstCtx.rax);
        if (fWhat & CPUMCTX_EXTRN_RCX)
            READ_GREG(HV_X86_RCX, pVCpu->cpum.GstCtx.rcx);
        if (fWhat & CPUMCTX_EXTRN_RDX)
            READ_GREG(HV_X86_RDX, pVCpu->cpum.GstCtx.rdx);
        if (fWhat & CPUMCTX_EXTRN_RBX)
            READ_GREG(HV_X86_RBX, pVCpu->cpum.GstCtx.rbx);
        if (fWhat & CPUMCTX_EXTRN_RSP)
            READ_GREG(HV_X86_RSP, pVCpu->cpum.GstCtx.rsp);
        if (fWhat & CPUMCTX_EXTRN_RBP)
            READ_GREG(HV_X86_RBP, pVCpu->cpum.GstCtx.rbp);
        if (fWhat & CPUMCTX_EXTRN_RSI)
            READ_GREG(HV_X86_RSI, pVCpu->cpum.GstCtx.rsi);
        if (fWhat & CPUMCTX_EXTRN_RDI)
            READ_GREG(HV_X86_RDI, pVCpu->cpum.GstCtx.rdi);
        if (fWhat & CPUMCTX_EXTRN_R8_R15)
        {
            READ_GREG(HV_X86_R8, pVCpu->cpum.GstCtx.r8);
            READ_GREG(HV_X86_R9, pVCpu->cpum.GstCtx.r9);
            READ_GREG(HV_X86_R10, pVCpu->cpum.GstCtx.r10);
            READ_GREG(HV_X86_R11, pVCpu->cpum.GstCtx.r11);
            READ_GREG(HV_X86_R12, pVCpu->cpum.GstCtx.r12);
            READ_GREG(HV_X86_R13, pVCpu->cpum.GstCtx.r13);
            READ_GREG(HV_X86_R14, pVCpu->cpum.GstCtx.r14);
            READ_GREG(HV_X86_R15, pVCpu->cpum.GstCtx.r15);
        }
    }

    /* RIP & Flags */
    if (fWhat & CPUMCTX_EXTRN_RIP)
        READ_GREG(HV_X86_RIP, pVCpu->cpum.GstCtx.rip);
    if (fWhat & CPUMCTX_EXTRN_RFLAGS)
        READ_GREG(HV_X86_RFLAGS, pVCpu->cpum.GstCtx.rflags.u);

    /* Segments */
#define READ_SEG(a_SReg, a_enmName) \
        do { \
            READ_VMCS16_FIELD(VMX_VMCS16_GUEST_ ## a_enmName ## _SEL,           (a_SReg).Sel); \
            READ_VMCS32_FIELD(VMX_VMCS32_GUEST_ ## a_enmName ## _LIMIT,         (a_SReg).u32Limit); \
            READ_VMCS32_FIELD(VMX_VMCS32_GUEST_ ## a_enmName ## _ACCESS_RIGHTS, (a_SReg).Attr.u); \
            READ_VMCS_FIELD(VMX_VMCS_GUEST_ ## a_enmName ## _BASE,            (a_SReg).u64Base); \
            (a_SReg).ValidSel = (a_SReg).Sel; \
        } while (0)
    if (fWhat & CPUMCTX_EXTRN_SREG_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_ES)
            READ_SEG(pVCpu->cpum.GstCtx.es, ES);
        if (fWhat & CPUMCTX_EXTRN_CS)
            READ_SEG(pVCpu->cpum.GstCtx.cs, CS);
        if (fWhat & CPUMCTX_EXTRN_SS)
            READ_SEG(pVCpu->cpum.GstCtx.ss, SS);
        if (fWhat & CPUMCTX_EXTRN_DS)
            READ_SEG(pVCpu->cpum.GstCtx.ds, DS);
        if (fWhat & CPUMCTX_EXTRN_FS)
            READ_SEG(pVCpu->cpum.GstCtx.fs, FS);
        if (fWhat & CPUMCTX_EXTRN_GS)
            READ_SEG(pVCpu->cpum.GstCtx.gs, GS);
    }

    /* Descriptor tables and the task segment. */
    if (fWhat & CPUMCTX_EXTRN_TABLE_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_LDTR)
            READ_SEG(pVCpu->cpum.GstCtx.ldtr, LDTR);

        if (fWhat & CPUMCTX_EXTRN_TR)
        {
            /* AMD-V likes loading TR with in AVAIL state, whereas intel insists on BUSY.  So,
               avoid to trigger sanity assertions around the code, always fix this. */
            READ_SEG(pVCpu->cpum.GstCtx.tr, TR);
            switch (pVCpu->cpum.GstCtx.tr.Attr.n.u4Type)
            {
                case X86_SEL_TYPE_SYS_386_TSS_BUSY:
                case X86_SEL_TYPE_SYS_286_TSS_BUSY:
                    break;
                case X86_SEL_TYPE_SYS_386_TSS_AVAIL:
                    pVCpu->cpum.GstCtx.tr.Attr.n.u4Type = X86_SEL_TYPE_SYS_386_TSS_BUSY;
                    break;
                case X86_SEL_TYPE_SYS_286_TSS_AVAIL:
                    pVCpu->cpum.GstCtx.tr.Attr.n.u4Type = X86_SEL_TYPE_SYS_286_TSS_BUSY;
                    break;
            }
        }
        if (fWhat & CPUMCTX_EXTRN_IDTR)
        {
            READ_VMCS32_FIELD(VMCS_GUEST_IDTR_LIMIT, pVCpu->cpum.GstCtx.idtr.cbIdt);
            READ_VMCS_FIELD(VMCS_GUEST_IDTR_BASE,  pVCpu->cpum.GstCtx.idtr.pIdt);
        }
        if (fWhat & CPUMCTX_EXTRN_GDTR)
        {
            READ_VMCS32_FIELD(VMCS_GUEST_GDTR_LIMIT, pVCpu->cpum.GstCtx.gdtr.cbGdt);
            READ_VMCS_FIELD(VMCS_GUEST_GDTR_BASE,  pVCpu->cpum.GstCtx.gdtr.pGdt);
        }
    }

    /* Control registers. */
    bool fMaybeChangedMode = false;
    bool fUpdateCr3        = false;
    if (fWhat & CPUMCTX_EXTRN_CR_MASK)
    {
        uint64_t u64CrTmp = 0;

        if (fWhat & CPUMCTX_EXTRN_CR0)
        {
            READ_GREG(HV_X86_CR0, u64CrTmp);
            if (pVCpu->cpum.GstCtx.cr0 != u64CrTmp)
            {
                CPUMSetGuestCR0(pVCpu, u64CrTmp);
                fMaybeChangedMode = true;
            }
        }
        if (fWhat & CPUMCTX_EXTRN_CR2)
            READ_GREG(HV_X86_CR2, pVCpu->cpum.GstCtx.cr2);
        if (fWhat & CPUMCTX_EXTRN_CR3)
        {
            READ_GREG(HV_X86_CR3, u64CrTmp);
            if (pVCpu->cpum.GstCtx.cr3 != u64CrTmp)
            {
                CPUMSetGuestCR3(pVCpu, u64CrTmp);
                fUpdateCr3 = true;
            }
        }
        if (fWhat & CPUMCTX_EXTRN_CR4)
        {
            READ_GREG(HV_X86_CR4, u64CrTmp);
            u64CrTmp &= ~VMX_V_CR4_FIXED0;

            if (pVCpu->cpum.GstCtx.cr4 != u64CrTmp)
            {
                CPUMSetGuestCR4(pVCpu, u64CrTmp);
                fMaybeChangedMode = true;
            }
        }
    }
    if (fWhat & CPUMCTX_EXTRN_APIC_TPR)
    {
        uint64_t u64Cr8 = 0;

        READ_GREG(HV_X86_TPR, u64Cr8);
        APICSetTpr(pVCpu, u64Cr8);
    }

    /* Debug registers. */
    if (fWhat & CPUMCTX_EXTRN_DR7)
    {
        uint64_t u64Dr7;
        READ_GREG(HV_X86_DR7, u64Dr7);
        if (pVCpu->cpum.GstCtx.dr[7] != u64Dr7)
            CPUMSetGuestDR7(pVCpu, u64Dr7);
        pVCpu->cpum.GstCtx.fExtrn &= ~CPUMCTX_EXTRN_DR7; /* Hack alert! Avoids asserting when processing CPUMCTX_EXTRN_DR0_DR3. */
    }
    if (fWhat & CPUMCTX_EXTRN_DR0_DR3)
    {
        uint64_t u64DrTmp;

        READ_GREG(HV_X86_DR0, u64DrTmp);
        if (pVCpu->cpum.GstCtx.dr[0] != u64DrTmp)
            CPUMSetGuestDR0(pVCpu, u64DrTmp);
        READ_GREG(HV_X86_DR1, u64DrTmp);
        if (pVCpu->cpum.GstCtx.dr[1] != u64DrTmp)
            CPUMSetGuestDR1(pVCpu, u64DrTmp);
        READ_GREG(HV_X86_DR3, u64DrTmp);
        if (pVCpu->cpum.GstCtx.dr[2] != u64DrTmp)
            CPUMSetGuestDR2(pVCpu, u64DrTmp);
        READ_GREG(HV_X86_DR3, u64DrTmp);
        if (pVCpu->cpum.GstCtx.dr[3] != u64DrTmp)
            CPUMSetGuestDR3(pVCpu, u64DrTmp);
    }
    if (fWhat & CPUMCTX_EXTRN_DR6)
    {
        uint64_t u64Dr6;
        READ_GREG(HV_X86_DR7, u64Dr6);
        if (pVCpu->cpum.GstCtx.dr[6] != u64Dr6)
            CPUMSetGuestDR6(pVCpu, u64Dr6);
    }

#if 0
    /* Floating point state. */
    if (fWhat & CPUMCTX_EXTRN_X87)
    {
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aRegs[0].au64[0], pVCpu->cpum.GstCtx.XState.x87.aRegs[0].au64[1], WHvX64RegisterFpMmx0);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aRegs[1].au64[0], pVCpu->cpum.GstCtx.XState.x87.aRegs[1].au64[1], WHvX64RegisterFpMmx1);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aRegs[2].au64[0], pVCpu->cpum.GstCtx.XState.x87.aRegs[2].au64[1], WHvX64RegisterFpMmx2);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aRegs[3].au64[0], pVCpu->cpum.GstCtx.XState.x87.aRegs[3].au64[1], WHvX64RegisterFpMmx3);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aRegs[4].au64[0], pVCpu->cpum.GstCtx.XState.x87.aRegs[4].au64[1], WHvX64RegisterFpMmx4);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aRegs[5].au64[0], pVCpu->cpum.GstCtx.XState.x87.aRegs[5].au64[1], WHvX64RegisterFpMmx5);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aRegs[6].au64[0], pVCpu->cpum.GstCtx.XState.x87.aRegs[6].au64[1], WHvX64RegisterFpMmx6);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aRegs[7].au64[0], pVCpu->cpum.GstCtx.XState.x87.aRegs[7].au64[1], WHvX64RegisterFpMmx7);

        Assert(aenmNames[iReg] == WHvX64RegisterFpControlStatus);
        pVCpu->cpum.GstCtx.XState.x87.FCW        = aValues[iReg].FpControlStatus.FpControl;
        pVCpu->cpum.GstCtx.XState.x87.FSW        = aValues[iReg].FpControlStatus.FpStatus;
        pVCpu->cpum.GstCtx.XState.x87.FTW        = aValues[iReg].FpControlStatus.FpTag
                                        /*| (aValues[iReg].FpControlStatus.Reserved << 8)*/;
        pVCpu->cpum.GstCtx.XState.x87.FOP        = aValues[iReg].FpControlStatus.LastFpOp;
        pVCpu->cpum.GstCtx.XState.x87.FPUIP      = (uint32_t)aValues[iReg].FpControlStatus.LastFpRip;
        pVCpu->cpum.GstCtx.XState.x87.CS         = (uint16_t)(aValues[iReg].FpControlStatus.LastFpRip >> 32);
        pVCpu->cpum.GstCtx.XState.x87.Rsrvd1     = (uint16_t)(aValues[iReg].FpControlStatus.LastFpRip >> 48);
        iReg++;
    }

    if (fWhat & (CPUMCTX_EXTRN_X87 | CPUMCTX_EXTRN_SSE_AVX))
    {
        Assert(aenmNames[iReg] == WHvX64RegisterXmmControlStatus);
        if (fWhat & CPUMCTX_EXTRN_X87)
        {
            pVCpu->cpum.GstCtx.XState.x87.FPUDP  = (uint32_t)aValues[iReg].XmmControlStatus.LastFpRdp;
            pVCpu->cpum.GstCtx.XState.x87.DS     = (uint16_t)(aValues[iReg].XmmControlStatus.LastFpRdp >> 32);
            pVCpu->cpum.GstCtx.XState.x87.Rsrvd2 = (uint16_t)(aValues[iReg].XmmControlStatus.LastFpRdp >> 48);
        }
        pVCpu->cpum.GstCtx.XState.x87.MXCSR      = aValues[iReg].XmmControlStatus.XmmStatusControl;
        pVCpu->cpum.GstCtx.XState.x87.MXCSR_MASK = aValues[iReg].XmmControlStatus.XmmStatusControlMask; /** @todo ??? (Isn't this an output field?) */
        iReg++;
    }

    /* Vector state. */
    if (fWhat & CPUMCTX_EXTRN_SSE_AVX)
    {
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[ 0].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[ 0].uXmm.s.Hi, WHvX64RegisterXmm0);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[ 1].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[ 1].uXmm.s.Hi, WHvX64RegisterXmm1);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[ 2].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[ 2].uXmm.s.Hi, WHvX64RegisterXmm2);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[ 3].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[ 3].uXmm.s.Hi, WHvX64RegisterXmm3);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[ 4].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[ 4].uXmm.s.Hi, WHvX64RegisterXmm4);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[ 5].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[ 5].uXmm.s.Hi, WHvX64RegisterXmm5);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[ 6].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[ 6].uXmm.s.Hi, WHvX64RegisterXmm6);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[ 7].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[ 7].uXmm.s.Hi, WHvX64RegisterXmm7);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[ 8].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[ 8].uXmm.s.Hi, WHvX64RegisterXmm8);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[ 9].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[ 9].uXmm.s.Hi, WHvX64RegisterXmm9);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[10].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[10].uXmm.s.Hi, WHvX64RegisterXmm10);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[11].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[11].uXmm.s.Hi, WHvX64RegisterXmm11);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[12].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[12].uXmm.s.Hi, WHvX64RegisterXmm12);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[13].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[13].uXmm.s.Hi, WHvX64RegisterXmm13);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[14].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[14].uXmm.s.Hi, WHvX64RegisterXmm14);
        GET_REG128(pVCpu->cpum.GstCtx.XState.x87.aXMM[15].uXmm.s.Lo, pVCpu->cpum.GstCtx.XState.x87.aXMM[15].uXmm.s.Hi, WHvX64RegisterXmm15);
    }
#endif

    /* MSRs */
    // WHvX64RegisterTsc - don't touch
    if (fWhat & CPUMCTX_EXTRN_EFER)
    {
        uint64_t u64Efer;

        READ_VMCS_FIELD(VMCS_GUEST_IA32_EFER, u64Efer);
        if (u64Efer != pVCpu->cpum.GstCtx.msrEFER)
        {
            Log7(("NEM/%u: MSR EFER changed %RX64 -> %RX64\n", pVCpu->idCpu, pVCpu->cpum.GstCtx.msrEFER, u64Efer));
            if ((u64Efer ^ pVCpu->cpum.GstCtx.msrEFER) & MSR_K6_EFER_NXE)
                PGMNotifyNxeChanged(pVCpu, RT_BOOL(u64Efer & MSR_K6_EFER_NXE));
            pVCpu->cpum.GstCtx.msrEFER = u64Efer;
            fMaybeChangedMode = true;
        }
    }
#if 0
    if (fWhat & CPUMCTX_EXTRN_KERNEL_GS_BASE)
        GET_REG64_LOG7(pVCpu->cpum.GstCtx.msrKERNELGSBASE, WHvX64RegisterKernelGsBase, "MSR KERNEL_GS_BASE");
    if (fWhat & CPUMCTX_EXTRN_SYSENTER_MSRS)
    {
        GET_REG64_LOG7(pVCpu->cpum.GstCtx.SysEnter.cs,  WHvX64RegisterSysenterCs,  "MSR SYSENTER.CS");
        GET_REG64_LOG7(pVCpu->cpum.GstCtx.SysEnter.eip, WHvX64RegisterSysenterEip, "MSR SYSENTER.EIP");
        GET_REG64_LOG7(pVCpu->cpum.GstCtx.SysEnter.esp, WHvX64RegisterSysenterEsp, "MSR SYSENTER.ESP");
    }
    if (fWhat & CPUMCTX_EXTRN_SYSCALL_MSRS)
    {
        GET_REG64_LOG7(pVCpu->cpum.GstCtx.msrSTAR,   WHvX64RegisterStar,   "MSR STAR");
        GET_REG64_LOG7(pVCpu->cpum.GstCtx.msrLSTAR,  WHvX64RegisterLstar,  "MSR LSTAR");
        GET_REG64_LOG7(pVCpu->cpum.GstCtx.msrCSTAR,  WHvX64RegisterCstar,  "MSR CSTAR");
        GET_REG64_LOG7(pVCpu->cpum.GstCtx.msrSFMASK, WHvX64RegisterSfmask, "MSR SFMASK");
    }
    if (fWhat & CPUMCTX_EXTRN_OTHER_MSRS)
    {
        Assert(aenmNames[iReg] == WHvX64RegisterApicBase);
        const uint64_t uOldBase = APICGetBaseMsrNoCheck(pVCpu);
        if (aValues[iReg].Reg64 != uOldBase)
        {
            Log7(("NEM/%u: MSR APICBase changed %RX64 -> %RX64 (%RX64)\n",
                  pVCpu->idCpu, uOldBase, aValues[iReg].Reg64, aValues[iReg].Reg64 ^ uOldBase));
            int rc2 = APICSetBaseMsr(pVCpu, aValues[iReg].Reg64);
            AssertLogRelMsg(rc2 == VINF_SUCCESS, ("%Rrc %RX64\n", rc2, aValues[iReg].Reg64));
        }
        iReg++;

        GET_REG64_LOG7(pVCpu->cpum.GstCtx.msrPAT, WHvX64RegisterPat, "MSR PAT");
#if 0 /*def LOG_ENABLED*/ /** @todo something's wrong with HvX64RegisterMtrrCap? (AMD) */
        GET_REG64_LOG7(pVCpu->cpum.GstCtx.msrPAT, WHvX64RegisterMsrMtrrCap);
#endif
        PCPUMCTXMSRS pCtxMsrs = CPUMQueryGuestCtxMsrsPtr(pVCpu);
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrDefType,      WHvX64RegisterMsrMtrrDefType,     "MSR MTRR_DEF_TYPE");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix64K_00000, WHvX64RegisterMsrMtrrFix64k00000, "MSR MTRR_FIX_64K_00000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix16K_80000, WHvX64RegisterMsrMtrrFix16k80000, "MSR MTRR_FIX_16K_80000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix16K_A0000, WHvX64RegisterMsrMtrrFix16kA0000, "MSR MTRR_FIX_16K_A0000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_C0000,  WHvX64RegisterMsrMtrrFix4kC0000,  "MSR MTRR_FIX_4K_C0000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_C8000,  WHvX64RegisterMsrMtrrFix4kC8000,  "MSR MTRR_FIX_4K_C8000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_D0000,  WHvX64RegisterMsrMtrrFix4kD0000,  "MSR MTRR_FIX_4K_D0000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_D8000,  WHvX64RegisterMsrMtrrFix4kD8000,  "MSR MTRR_FIX_4K_D8000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_E0000,  WHvX64RegisterMsrMtrrFix4kE0000,  "MSR MTRR_FIX_4K_E0000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_E8000,  WHvX64RegisterMsrMtrrFix4kE8000,  "MSR MTRR_FIX_4K_E8000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_F0000,  WHvX64RegisterMsrMtrrFix4kF0000,  "MSR MTRR_FIX_4K_F0000");
        GET_REG64_LOG7(pCtxMsrs->msr.MtrrFix4K_F8000,  WHvX64RegisterMsrMtrrFix4kF8000,  "MSR MTRR_FIX_4K_F8000");
        GET_REG64_LOG7(pCtxMsrs->msr.TscAux,           WHvX64RegisterTscAux,             "MSR TSC_AUX");
        /** @todo look for HvX64RegisterIa32MiscEnable and HvX64RegisterIa32FeatureControl? */
    }

    /* Interruptibility. */
    if (fWhat & (CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI))
    {
        Assert(aenmNames[iReg] == WHvRegisterInterruptState);
        Assert(aenmNames[iReg + 1] == WHvX64RegisterRip);

        if (!(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT))
        {
            pVCpu->nem.s.fLastInterruptShadow = aValues[iReg].InterruptState.InterruptShadow;
            if (aValues[iReg].InterruptState.InterruptShadow)
                EMSetInhibitInterruptsPC(pVCpu, aValues[iReg + 1].Reg64);
            else
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
        }

        if (!(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI))
        {
            if (aValues[iReg].InterruptState.NmiMasked)
                VMCPU_FF_SET(pVCpu, VMCPU_FF_BLOCK_NMIS);
            else
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_BLOCK_NMIS);
        }

        fWhat |= CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI;
        iReg += 2;
    }
#endif

    /* Almost done, just update extrn flags and maybe change PGM mode. */
    pVCpu->cpum.GstCtx.fExtrn &= ~fWhat;
    if (!(pVCpu->cpum.GstCtx.fExtrn & (CPUMCTX_EXTRN_ALL | (CPUMCTX_EXTRN_NEM_WIN_MASK & ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT))))
        pVCpu->cpum.GstCtx.fExtrn = 0;

    /* Typical. */
    if (!fMaybeChangedMode && !fUpdateCr3)
        return VINF_SUCCESS;

    /*
     * Slow.
     */
    if (fMaybeChangedMode)
    {
        int rc = PGMChangeMode(pVCpu, pVCpu->cpum.GstCtx.cr0, pVCpu->cpum.GstCtx.cr4, pVCpu->cpum.GstCtx.msrEFER);
        AssertMsgReturn(rc == VINF_SUCCESS, ("rc=%Rrc\n", rc), RT_FAILURE_NP(rc) ? rc : VERR_NEM_IPE_1);
    }

    if (fUpdateCr3)
    {
        int rc = PGMUpdateCR3(pVCpu, pVCpu->cpum.GstCtx.cr3, false /*fPdpesMapped*/);
        if (rc == VINF_SUCCESS)
        { /* likely */ }
        else
            AssertMsgFailedReturn(("rc=%Rrc\n", rc), RT_FAILURE_NP(rc) ? rc : VERR_NEM_IPE_2);
    }

    return VINF_SUCCESS;
#undef READ_GREG
#undef READ_VMCS_FIELD
#undef READ_VMCS32_FIELD
#undef READ_SEG
}


/**
 * Wrapper around nemR3DarwinCopyStateFromHv.
 *
 * Unlike the wrapped APIs, this checks whether it's necessary.
 *
 * @returns VBox strict status code.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   fWhat           What to import.
 */
DECLINLINE(VBOXSTRICTRC) nemR3DarwinImportStateIfNeededStrict(PVMCPUCC pVCpu, uint64_t fWhat)
{
    if (pVCpu->cpum.GstCtx.fExtrn & fWhat)
    {
        int rc = nemR3DarwinCopyStateFromHv(pVCpu->pVMR3, pVCpu, fWhat);
        AssertRCReturn(rc, rc);
    }
    return VINF_SUCCESS;
}


/**
 * State to pass between nemHCWinHandleMemoryAccess / nemR3WinWHvHandleMemoryAccess
 * and nemHCWinHandleMemoryAccessPageCheckerCallback.
 */
typedef struct NEMHCDARWINHMACPCCSTATE
{
    /** Input: Write access. */
    bool    fWriteAccess;
    /** Output: Set if we did something. */
    bool    fDidSomething;
    /** Output: Set it we should resume. */
    bool    fCanResume;
} NEMHCDARWINHMACPCCSTATE;

/**
 * @callback_method_impl{FNPGMPHYSNEMCHECKPAGE,
 *      Worker for nemR3WinHandleMemoryAccess; pvUser points to a
 *      NEMHCDARWINHMACPCCSTATE structure. }
 */
static DECLCALLBACK(int)
nemR3DarwinHandleMemoryAccessPageCheckerCallback(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhys, PPGMPHYSNEMPAGEINFO pInfo, void *pvUser)
{
    NEMHCDARWINHMACPCCSTATE *pState = (NEMHCDARWINHMACPCCSTATE *)pvUser;
    pState->fDidSomething = false;
    pState->fCanResume    = false;

    uint8_t  u2State = pInfo->u2NemState;

    /*
     * Consolidate current page state with actual page protection and access type.
     * We don't really consider downgrades here, as they shouldn't happen.
     */
    int rc;
    switch (u2State)
    {
        case NEM_DARWIN_PAGE_STATE_UNMAPPED:
        case NEM_DARWIN_PAGE_STATE_NOT_SET:
            if (pInfo->fNemProt == NEM_PAGE_PROT_NONE)
            {
                Log4(("nemR3DarwinHandleMemoryAccessPageCheckerCallback: %RGp - #1\n", GCPhys));
                return VINF_SUCCESS;
            }

            /* Don't bother remapping it if it's a write request to a non-writable page. */
            if (   pState->fWriteAccess
                && !(pInfo->fNemProt & NEM_PAGE_PROT_WRITE))
            {
                Log4(("nemR3DarwinHandleMemoryAccessPageCheckerCallback: %RGp - #1w\n", GCPhys));
                return VINF_SUCCESS;
            }

            /* Map the page. */
            rc = nemHCNativeSetPhysPage(pVM,
                                        pVCpu,
                                        GCPhys & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK,
                                        GCPhys & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK,
                                        pInfo->fNemProt,
                                        &u2State,
                                        true /*fBackingState*/);
            pInfo->u2NemState = u2State;
            Log4(("nemR3DarwinHandleMemoryAccessPageCheckerCallback: %RGp - synced => %s + %Rrc\n",
                  GCPhys, g_apszPageStates[u2State], rc));
            pState->fDidSomething = true;
            pState->fCanResume    = true;
            return rc;

        case NEM_DARWIN_PAGE_STATE_READABLE:
            if (   !(pInfo->fNemProt & NEM_PAGE_PROT_WRITE)
                && (pInfo->fNemProt & (NEM_PAGE_PROT_READ | NEM_PAGE_PROT_EXECUTE)))
            {
                pState->fCanResume = true;
                Log4(("nemR3DarwinHandleMemoryAccessPageCheckerCallback: %RGp - #2\n", GCPhys));
                return VINF_SUCCESS;
            }
            break;

        case NEM_DARWIN_PAGE_STATE_WRITABLE:
            if (pInfo->fNemProt & NEM_PAGE_PROT_WRITE)
            {
                /* We get spurious EPT exit violations when everything is fine (#3a case) but can resume without issues here... */
                pState->fCanResume = true;
                if (pInfo->u2OldNemState == NEM_DARWIN_PAGE_STATE_WRITABLE)
                    Log4(("nemR3DarwinHandleMemoryAccessPageCheckerCallback: %RGp - #3a\n", GCPhys));
                else
                    Log4(("nemR3DarwinHandleMemoryAccessPageCheckerCallback: %RGp - #3b (%s -> %s)\n",
                          GCPhys, g_apszPageStates[pInfo->u2OldNemState], g_apszPageStates[u2State]));
                return VINF_SUCCESS;
            }

            break;

        default:
            AssertLogRelMsgFailedReturn(("u2State=%#x\n", u2State), VERR_NEM_IPE_4);
    }

    /*
     * Unmap and restart the instruction.
     * If this fails, which it does every so often, just unmap everything for now.
     */
    rc = nemR3DarwinUnmap(GCPhys, X86_PAGE_SIZE);
    if (RT_SUCCESS(rc))
    {
        pState->fDidSomething = true;
        pState->fCanResume    = true;
        pInfo->u2NemState = NEM_DARWIN_PAGE_STATE_UNMAPPED;
        STAM_REL_COUNTER_INC(&pVM->nem.s.StatUnmapPage);
        uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
        Log5(("NEM GPA unmapped/exit: %RGp (was %s, cMappedPages=%u)\n", GCPhys, g_apszPageStates[u2State], cMappedPages));
        return VINF_SUCCESS;
    }
    STAM_REL_COUNTER_INC(&pVM->nem.s.StatUnmapPageFailed);
    LogRel(("nemR3DarwinHandleMemoryAccessPageCheckerCallback/unmap: GCPhysDst=%RGp %s rc=%Rrc\n",
            GCPhys, g_apszPageStates[u2State], rc));
    return VERR_NEM_UNMAP_PAGES_FAILED;
}


/*
 * Instantiate the code we share with ring-0.
 */
#define HMVMX_ALWAYS_TRAP_ALL_XCPTS
#define VCPU_2_VMXSTATE(a_pVCpu) (a_pVCpu)->nem.s

#define VMX_VMCS_WRITE_16(a_pVCpu, a_FieldEnc, a_Val) nemR3DarwinWriteVmcs16((a_pVCpu), (a_FieldEnc), (a_Val))
#define VMX_VMCS_WRITE_32(a_pVCpu, a_FieldEnc, a_Val) nemR3DarwinWriteVmcs32((a_pVCpu), (a_FieldEnc), (a_Val))
#define VMX_VMCS_WRITE_64(a_pVCpu, a_FieldEnc, a_Val) nemR3DarwinWriteVmcs64((a_pVCpu), (a_FieldEnc), (a_Val))
#define VMX_VMCS_WRITE_NW(a_pVCpu, a_FieldEnc, a_Val) nemR3DarwinWriteVmcs64((a_pVCpu), (a_FieldEnc), (a_Val))

#define VMX_VMCS_READ_16(a_pVCpu, a_FieldEnc, a_pVal) nemR3DarwinReadVmcs16((a_pVCpu), (a_FieldEnc), (a_pVal))
#define VMX_VMCS_READ_32(a_pVCpu, a_FieldEnc, a_pVal) nemR3DarwinReadVmcs32((a_pVCpu), (a_FieldEnc), (a_pVal))
#define VMX_VMCS_READ_64(a_pVCpu, a_FieldEnc, a_pVal) nemR3DarwinReadVmcs64((a_pVCpu), (a_FieldEnc), (a_pVal))
#define VMX_VMCS_READ_NW(a_pVCpu, a_FieldEnc, a_pVal) nemR3DarwinReadVmcs64((a_pVCpu), (a_FieldEnc), (a_pVal))

#include "../VMMAll/VMXAllTemplate.cpp.h"

#undef VMX_VMCS_WRITE_16
#undef VMX_VMCS_WRITE_32
#undef VMX_VMCS_WRITE_64
#undef VMX_VMCS_WRITE_NW

#undef VMX_VMCS_READ_16
#undef VMX_VMCS_READ_32
#undef VMX_VMCS_READ_64
#undef VMX_VMCS_READ_NW


/**
 * Handles an exit from hv_vcpu_run().
 *
 * @returns VBox strict status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 */
static VBOXSTRICTRC nemR3DarwinHandleExit(PVM pVM, PVMCPU pVCpu)
{
    VMXTRANSIENT VmxTransient;
    RT_ZERO(VmxTransient);

    uint32_t uExitReason;
    int rc = nemR3DarwinReadVmcs32(pVCpu, VMX_VMCS32_RO_EXIT_REASON, &uExitReason);
    AssertRC(rc);
    VmxTransient.pVmcsInfo      = &pVCpu->nem.s.VmcsInfo;
    VmxTransient.uExitReason    = VMX_EXIT_REASON_BASIC(uExitReason);
    VmxTransient.fVMEntryFailed = VMX_EXIT_REASON_HAS_ENTRY_FAILED(uExitReason);

    if (RT_UNLIKELY(VmxTransient.fVMEntryFailed))
        AssertLogRelMsgFailedReturn(("Running guest failed for CPU #%u: %#x %u\n",
                                    pVCpu->idCpu, VmxTransient.uExitReason, vmxHCCheckGuestState(pVCpu, &pVCpu->nem.s.VmcsInfo)),
                                    VERR_NEM_IPE_0);

    /** @todo Only copy the state on demand (requires changing to adhere to fCtxChanged from th VMX code
     * flags instead of the fExtrn one living in CPUM.
     */
    rc = nemR3DarwinCopyStateFromHv(pVM, pVCpu, UINT64_MAX);
    AssertRCReturn(rc, rc);

#ifndef HMVMX_USE_FUNCTION_TABLE
    return vmxHCHandleExit(pVCpu, &VmxTransient);
#else
    return g_aVMExitHandlers[VmxTransient.uExitReason].pfn(pVCpu, &VmxTransient);
#endif
}


/**
 * Read and initialize the global capabilities supported by this CPU.
 *
 * @returns VBox status code.
 */
static int nemR3DarwinCapsInit(void)
{
    RT_ZERO(g_HmMsrs);

    hv_return_t hrc = hv_vmx_read_capability(HV_VMX_CAP_PINBASED, &g_HmMsrs.u.vmx.PinCtls.u);
    if (hrc == HV_SUCCESS)
        hrc = hv_vmx_read_capability(HV_VMX_CAP_PROCBASED, &g_HmMsrs.u.vmx.ProcCtls.u);
#if 0 /* Not available with our SDK. */
    if (hrc == HV_SUCCESS)
        hrc = hv_vmx_read_capability(HV_VMX_CAP_BASIC, &g_HmMsrs.u.vmx.u64Basic);
#endif
    if (hrc == HV_SUCCESS)
        hrc = hv_vmx_read_capability(HV_VMX_CAP_ENTRY, &g_HmMsrs.u.vmx.EntryCtls.u);
    if (hrc == HV_SUCCESS)
        hrc = hv_vmx_read_capability(HV_VMX_CAP_EXIT, &g_HmMsrs.u.vmx.ExitCtls.u);
#if 0 /* Not available with our SDK. */
    if (hrc == HV_SUCCESS)
        hrc = hv_vmx_read_capability(HV_VMX_CAP_MISC, &g_HmMsrs.u.vmx.u64Misc);
    if (hrc == HV_SUCCESS)
        hrc = hv_vmx_read_capability(HV_VMX_CAP_CR0_FIXED0, &g_HmMsrs.u.vmx.u64Cr0Fixed0);
    if (hrc == HV_SUCCESS)
        hrc = hv_vmx_read_capability(HV_VMX_CAP_CR0_FIXED1, &g_HmMsrs.u.vmx.u64Cr0Fixed1);
    if (hrc == HV_SUCCESS)
        hrc = hv_vmx_read_capability(HV_VMX_CAP_CR4_FIXED0, &g_HmMsrs.u.vmx.u64Cr4Fixed0);
    if (hrc == HV_SUCCESS)
        hrc = hv_vmx_read_capability(HV_VMX_CAP_CR4_FIXED1, &g_HmMsrs.u.vmx.u64Cr4Fixed1);
    if (hrc == HV_SUCCESS)
        hrc = hv_vmx_read_capability(HV_VMX_CAP_VMCS_ENUM, &g_HmMsrs.u.vmx.u64VmcsEnum);
    if (   hrc == HV_SUCCESS
        && RT_BF_GET(g_HmMsrs.u.vmx.u64Basic, VMX_BF_BASIC_TRUE_CTLS))
    {
        hrc = hv_vmx_read_capability(HV_VMX_CAP_TRUE_PINBASED, &g_HmMsrs.u.vmx.TruePinCtls.u);
        if (hrc == HV_SUCCESS)
            hrc = hv_vmx_read_capability(HV_VMX_CAP_TRUE_PROCBASED, &g_HmMsrs.u.vmx.TrueProcCtls.u);
        if (hrc == HV_SUCCESS)
            hrc = hv_vmx_read_capability(HV_VMX_CAP_TRUE_ENTRY, &g_HmMsrs.u.vmx.TrueEntryCtls.u);
        if (hrc == HV_SUCCESS)
            hrc = hv_vmx_read_capability(HV_VMX_CAP_TRUE_EXIT, &g_HmMsrs.u.vmx.TrueExitCtls.u);
    }
#endif

    if (   hrc == HV_SUCCESS
        && g_HmMsrs.u.vmx.ProcCtls.n.allowed1 & VMX_PROC_CTLS_USE_SECONDARY_CTLS)
    {
        hrc = hv_vmx_read_capability(HV_VMX_CAP_PROCBASED2, &g_HmMsrs.u.vmx.ProcCtls2.u);

#if 0 /* Not available with our SDK. */
        if (  hrc == HV_SUCCESS
            & g_HmMsrs.u.vmx.ProcCtls2.n.allowed1 & (VMX_PROC_CTLS2_EPT | VMX_PROC_CTLS2_VPID))
            hrc = hv_vmx_read_capability(HV_VMX_CAP_EPT_VPID_CAP, &g_HmMsrs.u.vmx.u64EptVpidCaps);
#endif
        g_HmMsrs.u.vmx.u64VmFunc = 0; /* No way to read that on macOS. */
    }

    return nemR3DarwinHvSts2Rc(hrc);
}


/**
 * Sets up pin-based VM-execution controls in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcsInfo   The VMCS info. object.
 */
static int nemR3DarwinVmxSetupVmcsPinCtls(PVMCPUCC pVCpu, PVMXVMCSINFO pVmcsInfo)
{
    //PVMCC pVM = pVCpu->CTX_SUFF(pVM);
    uint32_t       fVal = g_HmMsrs.u.vmx.PinCtls.n.allowed0;      /* Bits set here must always be set. */
    uint32_t const fZap = g_HmMsrs.u.vmx.PinCtls.n.allowed1;      /* Bits cleared here must always be cleared. */

    if (g_HmMsrs.u.vmx.PinCtls.n.allowed1 & VMX_PIN_CTLS_VIRT_NMI)
        fVal |= VMX_PIN_CTLS_VIRT_NMI;                       /* Use virtual NMIs and virtual-NMI blocking features. */

#if 0 /** @todo Use preemption timer */
    /* Enable the VMX-preemption timer. */
    if (pVM->hmr0.s.vmx.fUsePreemptTimer)
    {
        Assert(g_HmMsrs.u.vmx.PinCtls.n.allowed1 & VMX_PIN_CTLS_PREEMPT_TIMER);
        fVal |= VMX_PIN_CTLS_PREEMPT_TIMER;
    }

    /* Enable posted-interrupt processing. */
    if (pVM->hm.s.fPostedIntrs)
    {
        Assert(g_HmMsrs.u.vmx.PinCtls.n.allowed1  & VMX_PIN_CTLS_POSTED_INT);
        Assert(g_HmMsrs.u.vmx.ExitCtls.n.allowed1 & VMX_EXIT_CTLS_ACK_EXT_INT);
        fVal |= VMX_PIN_CTLS_POSTED_INT;
    }
#endif

    if ((fVal & fZap) != fVal)
    {
        LogRelFunc(("Invalid pin-based VM-execution controls combo! Cpu=%#RX32 fVal=%#RX32 fZap=%#RX32\n",
                    g_HmMsrs.u.vmx.PinCtls.n.allowed0, fVal, fZap));
        pVCpu->nem.s.u32HMError = VMX_UFC_CTRL_PIN_EXEC;
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    /* Commit it to the VMCS and update our cache. */
    int rc = nemR3DarwinWriteVmcs32(pVCpu, VMX_VMCS32_CTRL_PIN_EXEC, fVal);
    AssertRC(rc);
    pVmcsInfo->u32PinCtls = fVal;

    return VINF_SUCCESS;
}


/**
 * Sets up secondary processor-based VM-execution controls in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcsInfo   The VMCS info. object.
 */
static int nemR3DarwinVmxSetupVmcsProcCtls2(PVMCPUCC pVCpu, PVMXVMCSINFO pVmcsInfo)
{
    PVMCC pVM = pVCpu->CTX_SUFF(pVM);
    uint32_t       fVal = g_HmMsrs.u.vmx.ProcCtls2.n.allowed0;    /* Bits set here must be set in the VMCS. */
    uint32_t const fZap = g_HmMsrs.u.vmx.ProcCtls2.n.allowed1;    /* Bits cleared here must be cleared in the VMCS. */

    /* WBINVD causes a VM-exit. */
    if (g_HmMsrs.u.vmx.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_WBINVD_EXIT)
        fVal |= VMX_PROC_CTLS2_WBINVD_EXIT;

    /* Enable the INVPCID instruction if we expose it to the guest and is supported
       by the hardware. Without this, guest executing INVPCID would cause a #UD. */
    if (   pVM->cpum.ro.GuestFeatures.fInvpcid
        && (g_HmMsrs.u.vmx.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_INVPCID))
        fVal |= VMX_PROC_CTLS2_INVPCID;

#if 0 /** @todo */
    /* Enable VPID. */
    if (pVM->hmr0.s.vmx.fVpid)
        fVal |= VMX_PROC_CTLS2_VPID;

    if (pVM->hm.s.fVirtApicRegs)
    {
        /* Enable APIC-register virtualization. */
        Assert(g_HmMsrs.u.vmx.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_APIC_REG_VIRT);
        fVal |= VMX_PROC_CTLS2_APIC_REG_VIRT;

        /* Enable virtual-interrupt delivery. */
        Assert(g_HmMsrs.u.vmx.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_VIRT_INTR_DELIVERY);
        fVal |= VMX_PROC_CTLS2_VIRT_INTR_DELIVERY;
    }

    /* Virtualize-APIC accesses if supported by the CPU. The virtual-APIC page is
       where the TPR shadow resides. */
    /** @todo VIRT_X2APIC support, it's mutually exclusive with this. So must be
     *        done dynamically. */
    if (g_HmMsrs.u.vmx.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_VIRT_APIC_ACCESS)
    {
        fVal |= VMX_PROC_CTLS2_VIRT_APIC_ACCESS;
        hmR0VmxSetupVmcsApicAccessAddr(pVCpu);
    }

    /* Enable the RDTSCP instruction if we expose it to the guest and is supported
       by the hardware. Without this, guest executing RDTSCP would cause a #UD. */
    if (   pVM->cpum.ro.GuestFeatures.fRdTscP
        && (g_HmMsrs.u.vmx.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_RDTSCP))
        fVal |= VMX_PROC_CTLS2_RDTSCP;

    /* Enable Pause-Loop exiting. */
    if (   (g_HmMsrs.u.vmx.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_PAUSE_LOOP_EXIT)
        && pVM->hm.s.vmx.cPleGapTicks
        && pVM->hm.s.vmx.cPleWindowTicks)
    {
        fVal |= VMX_PROC_CTLS2_PAUSE_LOOP_EXIT;

        int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PLE_GAP, pVM->hm.s.vmx.cPleGapTicks);          AssertRC(rc);
        rc     = VMXWriteVmcs32(VMX_VMCS32_CTRL_PLE_WINDOW, pVM->hm.s.vmx.cPleWindowTicks);    AssertRC(rc);
    }
#endif

    if ((fVal & fZap) != fVal)
    {
        LogRelFunc(("Invalid secondary processor-based VM-execution controls combo! cpu=%#RX32 fVal=%#RX32 fZap=%#RX32\n",
                    g_HmMsrs.u.vmx.ProcCtls2.n.allowed0, fVal, fZap));
        pVCpu->nem.s.u32HMError = VMX_UFC_CTRL_PROC_EXEC2;
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    /* Commit it to the VMCS and update our cache. */
    int rc = nemR3DarwinWriteVmcs32(pVCpu, VMX_VMCS32_CTRL_PROC_EXEC2, fVal);
    AssertRC(rc);
    pVmcsInfo->u32ProcCtls2 = fVal;

    return VINF_SUCCESS;
}


/**
 * Sets up processor-based VM-execution controls in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcsInfo   The VMCS info. object.
 */
static int nemR3DarwinVmxSetupVmcsProcCtls(PVMCPUCC pVCpu, PVMXVMCSINFO pVmcsInfo)
{
    PVMCC pVM = pVCpu->CTX_SUFF(pVM);
    uint32_t       fVal = g_HmMsrs.u.vmx.ProcCtls.n.allowed0;     /* Bits set here must be set in the VMCS. */
    uint32_t const fZap = g_HmMsrs.u.vmx.ProcCtls.n.allowed1;     /* Bits cleared here must be cleared in the VMCS. */

    fVal |= VMX_PROC_CTLS_HLT_EXIT                                    /* HLT causes a VM-exit. */
//         |  VMX_PROC_CTLS_USE_TSC_OFFSETTING                          /* Use TSC-offsetting. */
         |  VMX_PROC_CTLS_MOV_DR_EXIT                                 /* MOV DRx causes a VM-exit. */
         |  VMX_PROC_CTLS_UNCOND_IO_EXIT                              /* All IO instructions cause a VM-exit. */
         |  VMX_PROC_CTLS_RDPMC_EXIT                                  /* RDPMC causes a VM-exit. */
         |  VMX_PROC_CTLS_MONITOR_EXIT                                /* MONITOR causes a VM-exit. */
         |  VMX_PROC_CTLS_MWAIT_EXIT;                                 /* MWAIT causes a VM-exit. */

    /* We toggle VMX_PROC_CTLS_MOV_DR_EXIT later, check if it's not -always- needed to be set or clear. */
    if (   !(g_HmMsrs.u.vmx.ProcCtls.n.allowed1 & VMX_PROC_CTLS_MOV_DR_EXIT)
        ||  (g_HmMsrs.u.vmx.ProcCtls.n.allowed0 & VMX_PROC_CTLS_MOV_DR_EXIT))
    {
        pVCpu->nem.s.u32HMError = VMX_UFC_CTRL_PROC_MOV_DRX_EXIT;
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

#if 0 /** @todo */
    /* Use TPR shadowing if supported by the CPU. */
    if (   PDMHasApic(pVM)
        && (g_HmMsrs.u.vmx.ProcCtls.n.allowed1 & VMX_PROC_CTLS_USE_TPR_SHADOW))
    {
        fVal |= VMX_PROC_CTLS_USE_TPR_SHADOW;                /* CR8 reads from the Virtual-APIC page. */
                                                             /* CR8 writes cause a VM-exit based on TPR threshold. */
        Assert(!(fVal & VMX_PROC_CTLS_CR8_STORE_EXIT));
        Assert(!(fVal & VMX_PROC_CTLS_CR8_LOAD_EXIT));
        hmR0VmxSetupVmcsVirtApicAddr(pVmcsInfo);
    }
    else
#endif
    {
        fVal |= VMX_PROC_CTLS_CR8_STORE_EXIT             /* CR8 reads cause a VM-exit. */
             |  VMX_PROC_CTLS_CR8_LOAD_EXIT;             /* CR8 writes cause a VM-exit. */
    }

#if 0 /** @todo */
    /* Use MSR-bitmaps if supported by the CPU. */
    if (g_HmMsrs.u.vmx.ProcCtls.n.allowed1 & VMX_PROC_CTLS_USE_MSR_BITMAPS)
    {
        fVal |= VMX_PROC_CTLS_USE_MSR_BITMAPS;
        hmR0VmxSetupVmcsMsrBitmapAddr(pVmcsInfo);
    }
#endif

    /* Use the secondary processor-based VM-execution controls if supported by the CPU. */
    if (g_HmMsrs.u.vmx.ProcCtls.n.allowed1 & VMX_PROC_CTLS_USE_SECONDARY_CTLS)
        fVal |= VMX_PROC_CTLS_USE_SECONDARY_CTLS;

    if ((fVal & fZap) != fVal)
    {
        LogRelFunc(("Invalid processor-based VM-execution controls combo! cpu=%#RX32 fVal=%#RX32 fZap=%#RX32\n",
                    g_HmMsrs.u.vmx.ProcCtls.n.allowed0, fVal, fZap));
        pVCpu->nem.s.u32HMError = VMX_UFC_CTRL_PROC_EXEC;
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    /* Commit it to the VMCS and update our cache. */
    int rc = nemR3DarwinWriteVmcs32(pVCpu, VMX_VMCS32_CTRL_PROC_EXEC, fVal);
    AssertRC(rc);
    pVmcsInfo->u32ProcCtls = fVal;

#if 0
    /* Set up MSR permissions that don't change through the lifetime of the VM. */
    if (pVmcsInfo->u32ProcCtls & VMX_PROC_CTLS_USE_MSR_BITMAPS)
        hmR0VmxSetupVmcsMsrPermissions(pVCpu, pVmcsInfo);
#endif

    /*
     * Set up secondary processor-based VM-execution controls
     * (we assume the CPU to always support it as we rely on unrestricted guest execution support).
     */
    Assert(pVmcsInfo->u32ProcCtls & VMX_PROC_CTLS_USE_SECONDARY_CTLS);
    return nemR3DarwinVmxSetupVmcsProcCtls2(pVCpu, pVmcsInfo);
}


/**
 * Sets up miscellaneous (everything other than Pin, Processor and secondary
 * Processor-based VM-execution) control fields in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcsInfo   The VMCS info. object.
 */
static int nemR3DarwinVmxSetupVmcsMiscCtls(PVMCPUCC pVCpu, PVMXVMCSINFO pVmcsInfo)
{
    int rc = VINF_SUCCESS;
    //rc = hmR0VmxSetupVmcsAutoLoadStoreMsrAddrs(pVmcsInfo); TODO
    if (RT_SUCCESS(rc))
    {
        uint64_t const u64Cr0Mask = vmxHCGetFixedCr0Mask(pVCpu);
        uint64_t const u64Cr4Mask = vmxHCGetFixedCr4Mask(pVCpu);

        rc = nemR3DarwinWriteVmcs64(pVCpu, VMX_VMCS_CTRL_CR0_MASK, u64Cr0Mask);    AssertRC(rc);
        rc = nemR3DarwinWriteVmcs64(pVCpu, VMX_VMCS_CTRL_CR4_MASK, u64Cr4Mask);    AssertRC(rc);

        pVmcsInfo->u64Cr0Mask = u64Cr0Mask;
        pVmcsInfo->u64Cr4Mask = u64Cr4Mask;

#if 0 /** @todo */
        if (pVCpu->CTX_SUFF(pVM)->hmr0.s.vmx.fLbr)
        {
            rc = VMXWriteVmcsNw(VMX_VMCS64_GUEST_DEBUGCTL_FULL, MSR_IA32_DEBUGCTL_LBR);
            AssertRC(rc);
        }
#endif
        return VINF_SUCCESS;
    }
    else
        LogRelFunc(("Failed to initialize VMCS auto-load/store MSR addresses. rc=%Rrc\n", rc));
    return rc;
}


/**
 * Sets up the initial exception bitmap in the VMCS based on static conditions.
 *
 * We shall setup those exception intercepts that don't change during the
 * lifetime of the VM here. The rest are done dynamically while loading the
 * guest state.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcsInfo   The VMCS info. object.
 */
static void nemR3DarwinVmxSetupVmcsXcptBitmap(PVMCPUCC pVCpu, PVMXVMCSINFO pVmcsInfo)
{
    /*
     * The following exceptions are always intercepted:
     *
     * #AC - To prevent the guest from hanging the CPU and for dealing with
     *       split-lock detecting host configs.
     * #DB - To maintain the DR6 state even when intercepting DRx reads/writes and
     *       recursive #DBs can cause a CPU hang.
     */
    uint32_t const uXcptBitmap = RT_BIT(X86_XCPT_AC)
                               | RT_BIT(X86_XCPT_DB);

    /* Commit it to the VMCS. */
    int rc = nemR3DarwinWriteVmcs32(pVCpu, VMX_VMCS32_CTRL_EXCEPTION_BITMAP, uXcptBitmap);
    AssertRC(rc);

    /* Update our cache of the exception bitmap. */
    pVmcsInfo->u32XcptBitmap = uXcptBitmap;
}


/**
 * Initialize the VMCS information field for the given vCPU.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 */
static int nemR3DarwinInitVmcs(PVMCPU pVCpu)
{
    int rc = nemR3DarwinVmxSetupVmcsPinCtls(pVCpu, &pVCpu->nem.s.VmcsInfo);
    if (RT_SUCCESS(rc))
    {
        rc = nemR3DarwinVmxSetupVmcsProcCtls(pVCpu, &pVCpu->nem.s.VmcsInfo);
        if (RT_SUCCESS(rc))
        {
            rc = nemR3DarwinVmxSetupVmcsMiscCtls(pVCpu, &pVCpu->nem.s.VmcsInfo);
            if (RT_SUCCESS(rc))
            {
                nemR3DarwinVmxSetupVmcsXcptBitmap(pVCpu, &pVCpu->nem.s.VmcsInfo);
                return VINF_SUCCESS;
            }
            else
                LogRelFunc(("Failed to setup miscellaneous controls. rc=%Rrc\n", rc));
        }
        else
            LogRelFunc(("Failed to setup processor-based VM-execution controls. rc=%Rrc\n", rc));
    }
    else
        LogRelFunc(("Failed to setup pin-based controls. rc=%Rrc\n", rc));

    return rc;
}


/**
 * Try initialize the native API.
 *
 * This may only do part of the job, more can be done in
 * nemR3NativeInitAfterCPUM() and nemR3NativeInitCompleted().
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   fFallback       Whether we're in fallback mode or use-NEM mode. In
 *                          the latter we'll fail if we cannot initialize.
 * @param   fForced         Whether the HMForced flag is set and we should
 *                          fail if we cannot initialize.
 */
int nemR3NativeInit(PVM pVM, bool fFallback, bool fForced)
{
    AssertReturn(!pVM->nem.s.fCreatedVm, VERR_WRONG_ORDER);

    /*
     * Some state init.
     */

    /*
     * Error state.
     * The error message will be non-empty on failure and 'rc' will be set too.
     */
    RTERRINFOSTATIC ErrInfo;
    PRTERRINFO pErrInfo = RTErrInfoInitStatic(&ErrInfo);
    int rc = VINF_SUCCESS;
    hv_return_t hrc = hv_vm_create(HV_VM_DEFAULT);
    if (hrc == HV_SUCCESS)
    {
        pVM->nem.s.fCreatedVm = true;

        VM_SET_MAIN_EXECUTION_ENGINE(pVM, VM_EXEC_ENGINE_NATIVE_API);
        Log(("NEM: Marked active!\n"));
        PGMR3EnableNemMode(pVM);

        /* Register release statistics */
        for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
        {
            PNEMCPU pNemCpu = &pVM->apCpusR3[idCpu]->nem.s;
            STAMR3RegisterF(pVM, &pNemCpu->StatExitPortIo,          STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of port I/O exits",               "/NEM/CPU%u/ExitPortIo", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitMemUnmapped,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of unmapped memory exits",        "/NEM/CPU%u/ExitMemUnmapped", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitMemIntercept,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of intercepted memory exits",     "/NEM/CPU%u/ExitMemIntercept", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitHalt,            STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of HLT exits",                    "/NEM/CPU%u/ExitHalt", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitInterruptWindow, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of HLT exits",                    "/NEM/CPU%u/ExitInterruptWindow", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitCpuId,           STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of CPUID exits",                  "/NEM/CPU%u/ExitCpuId", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitMsr,             STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of MSR access exits",             "/NEM/CPU%u/ExitMsr", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitException,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of exception exits",              "/NEM/CPU%u/ExitException", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionBp,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #BP exits",                    "/NEM/CPU%u/ExitExceptionBp", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionDb,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #DB exits",                    "/NEM/CPU%u/ExitExceptionDb", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionGp,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #GP exits",                    "/NEM/CPU%u/ExitExceptionGp", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionGpMesa, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #GP exits from mesa driver",   "/NEM/CPU%u/ExitExceptionGpMesa", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionUd,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #UD exits",                    "/NEM/CPU%u/ExitExceptionUd", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionUdHandled, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of handled #UD exits",         "/NEM/CPU%u/ExitExceptionUdHandled", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatExitUnrecoverable,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of unrecoverable exits",          "/NEM/CPU%u/ExitUnrecoverable", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatGetMsgTimeout,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of get message timeouts/alerts",  "/NEM/CPU%u/GetMsgTimeout", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuSuccess,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of successful CPU stops",         "/NEM/CPU%u/StopCpuSuccess", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuPending,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of pending CPU stops",            "/NEM/CPU%u/StopCpuPending", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuPendingAlerts,STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of pending CPU stop alerts",      "/NEM/CPU%u/StopCpuPendingAlerts", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuPendingOdd,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of odd pending CPU stops (see code)", "/NEM/CPU%u/StopCpuPendingOdd", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatCancelChangedState,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of cancel changed state",         "/NEM/CPU%u/CancelChangedState", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatCancelAlertedThread, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of cancel alerted EMT",           "/NEM/CPU%u/CancelAlertedEMT", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnFFPre,        STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of pre execution FF breaks",      "/NEM/CPU%u/BreakOnFFPre", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnFFPost,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of post execution FF breaks",     "/NEM/CPU%u/BreakOnFFPost", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnCancel,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of cancel execution breaks",      "/NEM/CPU%u/BreakOnCancel", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnStatus,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of status code breaks",           "/NEM/CPU%u/BreakOnStatus", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatImportOnDemand,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of on-demand state imports",      "/NEM/CPU%u/ImportOnDemand", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatImportOnReturn,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of state imports on loop return", "/NEM/CPU%u/ImportOnReturn", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatImportOnReturnSkipped, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of skipped state imports on loop return", "/NEM/CPU%u/ImportOnReturnSkipped", idCpu);
            STAMR3RegisterF(pVM, &pNemCpu->StatQueryCpuTick,        STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of TSC queries",                  "/NEM/CPU%u/QueryCpuTick", idCpu);
        }
    }
    else
        rc = RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                           "hv_vm_create() failed: %#x", hrc);

    /*
     * We only fail if in forced mode, otherwise just log the complaint and return.
     */
    Assert(pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API || RTErrInfoIsSet(pErrInfo));
    if (   (fForced || !fFallback)
        && pVM->bMainExecutionEngine != VM_EXEC_ENGINE_NATIVE_API)
        return VMSetError(pVM, RT_SUCCESS_NP(rc) ? VERR_NEM_NOT_AVAILABLE : rc, RT_SRC_POS, "%s", pErrInfo->pszMsg);

    if (RTErrInfoIsSet(pErrInfo))
        LogRel(("NEM: Not available: %s\n", pErrInfo->pszMsg));
    return VINF_SUCCESS;
}


/**
 * Worker to create the vCPU handle on the EMT running it later on (as required by HV).
 *
 * @returns VBox status code
 * @param   pVM                 The VM handle.
 * @param   pVCpu               The vCPU handle.
 * @param   idCpu               ID of the CPU to create.
 */
static DECLCALLBACK(int) nemR3DarwinNativeInitVCpuOnEmt(PVM pVM, PVMCPU pVCpu, VMCPUID idCpu)
{
    hv_return_t hrc = hv_vcpu_create(&pVCpu->nem.s.hVCpuId, HV_VCPU_DEFAULT);
    if (hrc != HV_SUCCESS)
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Call to hv_vcpu_create failed on vCPU %u: %#x (%Rrc)", idCpu, hrc, nemR3DarwinHvSts2Rc(hrc));

    if (idCpu == 0)
    {
        /* First call initializs the MSR structure holding the capabilities of the host CPU. */
        int rc = nemR3DarwinCapsInit();
        AssertRCReturn(rc, rc);
    }

    int rc = nemR3DarwinInitVmcs(pVCpu);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * Worker to destroy the vCPU handle on the EMT running it later on (as required by HV).
 *
 * @returns VBox status code
 * @param   pVCpu               The vCPU handle.
 */
static DECLCALLBACK(int) nemR3DarwinNativeTermVCpuOnEmt(PVMCPU pVCpu)
{
    hv_return_t hrc = hv_vcpu_destroy(pVCpu->nem.s.hVCpuId);
    Assert(hrc == HV_SUCCESS);
    return VINF_SUCCESS;
}


/**
 * This is called after CPUMR3Init is done.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle..
 */
int nemR3NativeInitAfterCPUM(PVM pVM)
{
    /*
     * Validate sanity.
     */
    AssertReturn(!pVM->nem.s.fCreatedEmts, VERR_WRONG_ORDER);
    AssertReturn(pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API, VERR_WRONG_ORDER);

    /*
     * Setup the EMTs.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];

        int rc = VMR3ReqCallWait(pVM, idCpu, (PFNRT)nemR3DarwinNativeInitVCpuOnEmt, 3, pVM, pVCpu, idCpu);
        if (RT_FAILURE(rc))
        {
            /* Rollback. */
            while (idCpu--)
                VMR3ReqCallWait(pVM, idCpu, (PFNRT)nemR3DarwinNativeTermVCpuOnEmt, 1, pVCpu);

            return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS, "Call to hv_vcpu_create failed: %Rrc", rc);
        }
    }

    pVM->nem.s.fCreatedEmts = true;

    //CPUMR3ClearGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SEP);
    return VINF_SUCCESS;
}


int nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    NOREF(pVM); NOREF(enmWhat);
    return VINF_SUCCESS;
}


int nemR3NativeTerm(PVM pVM)
{
    /*
     * Delete the VM.
     */

    for (VMCPUID idCpu = pVM->cCpus - 1; idCpu > 0; idCpu--)
    {
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];

        /*
         * Apple's documentation states that the vCPU should be destroyed
         * on the thread running the vCPU but as all the other EMTs are gone
         * at this point, destroying the VM would hang.
         *
         * We seem to be at luck here though as destroying apparently works
         * from EMT(0) as well.
         */
        hv_return_t hrc = hv_vcpu_destroy(pVCpu->nem.s.hVCpuId);
        Assert(hrc == HV_SUCCESS);
    }

    hv_vcpu_destroy(pVM->apCpusR3[0]->nem.s.hVCpuId);
    pVM->nem.s.fCreatedEmts = false;

    if (pVM->nem.s.fCreatedVm)
    {
        hv_return_t hrc = hv_vm_destroy();
        if (hrc != HV_SUCCESS)
            LogRel(("NEM: hv_vm_destroy() failed with %#x\n", hrc));

        pVM->nem.s.fCreatedVm = false;
    }
    return VINF_SUCCESS;
}


/**
 * VM reset notification.
 *
 * @param   pVM         The cross context VM structure.
 */
void nemR3NativeReset(PVM pVM)
{
    RT_NOREF(pVM);
}


/**
 * Reset CPU due to INIT IPI or hot (un)plugging.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the CPU being
 *                      reset.
 * @param   fInitIpi    Whether this is the INIT IPI or hot (un)plugging case.
 */
void nemR3NativeResetCpu(PVMCPU pVCpu, bool fInitIpi)
{
    RT_NOREF(pVCpu, fInitIpi);
}


VBOXSTRICTRC nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu)
{
    LogFlow(("NEM/%u: %04x:%08RX64 efl=%#08RX64 <=\n", pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, pVCpu->cpum.GstCtx.rflags));
#ifdef LOG_ENABLED
    if (LogIs3Enabled())
        nemR3DarwinLogState(pVM, pVCpu);
#endif

    /*
     * Try switch to NEM runloop state.
     */
    if (VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED))
    { /* likely */ }
    else
    {
        VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED);
        LogFlow(("NEM/%u: returning immediately because canceled\n", pVCpu->idCpu));
        return VINF_SUCCESS;
    }

    /*
     * The run loop.
     *
     * Current approach to state updating to use the sledgehammer and sync
     * everything every time.  This will be optimized later.
     */
    const bool      fSingleStepping = DBGFIsStepping(pVCpu);
    VBOXSTRICTRC    rcStrict        = VINF_SUCCESS;
    for (unsigned iLoop = 0;; iLoop++)
    {
        /*
         * Check and process force flag actions, some of which might require us to go back to ring-3.
         */
        rcStrict = vmxHCCheckForceFlags(pVCpu, false /*fIsNestedGuest*/, fSingleStepping);
        if (rcStrict == VINF_SUCCESS)
        { /*likely */ }
        else
            break;

        /*
         * Evaluate events to be injected into the guest.
         *
         * Events in TRPM can be injected without inspecting the guest state.
         * If any new events (interrupts/NMI) are pending currently, we try to set up the
         * guest to cause a VM-exit the next time they are ready to receive the event.
         */
        if (TRPMHasTrap(pVCpu))
            vmxHCTrpmTrapToPendingEvent(pVCpu);

        uint32_t fIntrState;
        rcStrict = vmxHCEvaluatePendingEvent(pVCpu, &pVCpu->nem.s.VmcsInfo, false /*fIsNestedGuest*/, &fIntrState);

        /*
         * Event injection may take locks (currently the PGM lock for real-on-v86 case) and thus
         * needs to be done with longjmps or interrupts + preemption enabled. Event injection might
         * also result in triple-faulting the VM.
         *
         * With nested-guests, the above does not apply since unrestricted guest execution is a
         * requirement. Regardless, we do this here to avoid duplicating code elsewhere.
         */
        rcStrict = vmxHCInjectPendingEvent(pVCpu, &pVCpu->nem.s.VmcsInfo, false /*fIsNestedGuest*/, fIntrState, fSingleStepping);
        if (RT_LIKELY(rcStrict == VINF_SUCCESS))
        { /* likely */ }
        else
        {
            AssertMsg(rcStrict == VINF_EM_RESET || (rcStrict == VINF_EM_DBG_STEPPED && fSingleStepping),
                      ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
            break;
        }

        /** @todo Only copy the state selectively. */
        {
            int rc2 = nemR3DarwinCopyStateToHv(pVM, pVCpu);
            AssertRCReturn(rc2, rc2);
        }

        /*
         * Poll timers and run for a bit.
         */
        /** @todo See if we cannot optimize this TMTimerPollGIP by only redoing
         *        the whole polling job when timers have changed... */
        uint64_t       offDeltaIgnored;
        uint64_t const nsNextTimerEvt = TMTimerPollGIP(pVM, pVCpu, &offDeltaIgnored); NOREF(nsNextTimerEvt);
        if (   !VM_FF_IS_ANY_SET(pVM, VM_FF_EMT_RENDEZVOUS | VM_FF_TM_VIRTUAL_SYNC)
            && !VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
        {
            if (VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM_WAIT, VMCPUSTATE_STARTED_EXEC_NEM))
            {
                LogFlowFunc(("Running vCPU\n"));
                pVCpu->nem.s.Event.fPending = false;
                hv_return_t hrc = hv_vcpu_run(pVCpu->nem.s.hVCpuId); /** @todo Use hv_vcpu_run_until() when available (11.0+). */
                VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_WAIT);
                if (hrc == HV_SUCCESS)
                {
                    /*
                     * Deal with the message.
                     */
                    rcStrict = nemR3DarwinHandleExit(pVM, pVCpu);
                    if (rcStrict == VINF_SUCCESS)
                    { /* hopefully likely */ }
                    else
                    {
                        LogFlow(("NEM/%u: breaking: nemR3DarwinHandleExit -> %Rrc\n", pVCpu->idCpu, VBOXSTRICTRC_VAL(rcStrict) ));
                        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnStatus);
                        break;
                    }
                }
                else
                {
                    AssertLogRelMsgFailedReturn(("hv_vcpu_run failed for CPU #%u: %#x\n", pVCpu->idCpu, hrc),
                                                VERR_NEM_IPE_0);
                }

                /*
                 * If no relevant FFs are pending, loop.
                 */
                if (   !VM_FF_IS_ANY_SET(   pVM,   !fSingleStepping ? VM_FF_HP_R0_PRE_HM_MASK    : VM_FF_HP_R0_PRE_HM_STEP_MASK)
                    && !VMCPU_FF_IS_ANY_SET(pVCpu, !fSingleStepping ? VMCPU_FF_HP_R0_PRE_HM_MASK : VMCPU_FF_HP_R0_PRE_HM_STEP_MASK) )
                    continue;

                /** @todo Try handle pending flags, not just return to EM loops.  Take care
                 *        not to set important RCs here unless we've handled a message. */
                LogFlow(("NEM/%u: breaking: pending FF (%#x / %#RX64)\n",
                         pVCpu->idCpu, pVM->fGlobalForcedActions, (uint64_t)pVCpu->fLocalForcedActions));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnFFPost);
            }
            else
            {
                LogFlow(("NEM/%u: breaking: canceled %d (pre exec)\n", pVCpu->idCpu, VMCPU_GET_STATE(pVCpu) ));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnCancel);
            }
        }
        else
        {
            LogFlow(("NEM/%u: breaking: pending FF (pre exec)\n", pVCpu->idCpu));
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnFFPre);
        }
        break;
    } /* the run loop */


    /*
     * Convert any pending HM events back to TRPM due to premature exits.
     *
     * This is because execution may continue from IEM and we would need to inject
     * the event from there (hence place it back in TRPM).
     */
    if (pVCpu->nem.s.Event.fPending)
    {
        vmxHCPendingEventToTrpmTrap(pVCpu);
        Assert(!pVCpu->nem.s.Event.fPending);

        /* Clear the events from the VMCS. */
        int rc = nemR3DarwinWriteVmcs32(pVCpu, VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO, 0);    AssertRC(rc);
        rc     = nemR3DarwinWriteVmcs32(pVCpu, VMX_VMCS_GUEST_PENDING_DEBUG_XCPTS, 0);         AssertRC(rc);
    }


    if (!VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM))
        VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED);

    if (pVCpu->cpum.GstCtx.fExtrn & (CPUMCTX_EXTRN_ALL | (CPUMCTX_EXTRN_NEM_WIN_MASK & ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT)))
    {
        /* Try anticipate what we might need. */
        uint64_t fImport = IEM_CPUMCTX_EXTRN_MUST_MASK | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_INT | CPUMCTX_EXTRN_NEM_WIN_INHIBIT_NMI;
        if (   (rcStrict >= VINF_EM_FIRST && rcStrict <= VINF_EM_LAST)
            || RT_FAILURE(rcStrict))
            fImport = CPUMCTX_EXTRN_ALL | (CPUMCTX_EXTRN_NEM_WIN_MASK & ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT);
        else if (VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_INTERRUPT_PIC | VMCPU_FF_INTERRUPT_APIC
                                          | VMCPU_FF_INTERRUPT_NMI | VMCPU_FF_INTERRUPT_SMI))
            fImport |= IEM_CPUMCTX_EXTRN_XCPT_MASK;

        fImport = CPUMCTX_EXTRN_ALL;
        if (pVCpu->cpum.GstCtx.fExtrn & fImport)
        {
            /* Only import what is external currently. */
            int rc2 = nemR3DarwinCopyStateFromHv(pVM, pVCpu, fImport | CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT);
            if (RT_SUCCESS(rc2))
                pVCpu->cpum.GstCtx.fExtrn &= ~fImport;
            else if (RT_SUCCESS(rcStrict))
                rcStrict = rc2;
            if (!(pVCpu->cpum.GstCtx.fExtrn & (CPUMCTX_EXTRN_ALL | (CPUMCTX_EXTRN_NEM_WIN_MASK & ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT))))
                pVCpu->cpum.GstCtx.fExtrn = 0;
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnReturn);
        }
        else
        {
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnReturnSkipped);
            pVCpu->cpum.GstCtx.fExtrn &= ~CPUMCTX_EXTRN_NEM_WIN_EVENT_INJECT;
        }
    }
    else
    {
        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnReturnSkipped);
        pVCpu->cpum.GstCtx.fExtrn = 0;
    }

    LogFlow(("NEM/%u: %04x:%08RX64 efl=%#08RX64 => %Rrc\n",
             pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, pVCpu->cpum.GstCtx.rflags, VBOXSTRICTRC_VAL(rcStrict) ));
    return rcStrict;
}


VMMR3_INT_DECL(bool) NEMR3CanExecuteGuest(PVM pVM, PVMCPU pVCpu)
{
    NOREF(pVM);
    return PGMPhysIsA20Enabled(pVCpu);
}


bool nemR3NativeSetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(fEnable);
    return false;
}


/**
 * Forced flag notification call from VMEmt.h.
 *
 * This is only called when pVCpu is in the VMCPUSTATE_STARTED_EXEC_NEM state.
 *
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the CPU
 *                          to be notified.
 * @param   fFlags          Notification flags, VMNOTIFYFF_FLAGS_XXX.
 */
void nemR3NativeNotifyFF(PVM pVM, PVMCPU pVCpu, uint32_t fFlags)
{
    LogFlowFunc(("pVM=%p pVCpu=%p fFlags=%#x\n", pVM, pVCpu, fFlags));

    hv_return_t hrc = hv_vcpu_interrupt(&pVCpu->nem.s.hVCpuId, 1);
    if (hrc != HV_SUCCESS)
        LogRel(("NEM: hv_vcpu_interrupt(%u, 1) failed with %#x\n", pVCpu->nem.s.hVCpuId, hrc));
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvR3,
                                               uint8_t *pu2State, uint32_t *puNemRange)
{
    RT_NOREF(pVM, puNemRange);

    Log5(("NEMR3NotifyPhysRamRegister: %RGp LB %RGp, pvR3=%p\n", GCPhys, cb, pvR3));
#if defined(VBOX_WITH_PGM_NEM_MODE)
    if (pvR3)
    {
        int rc = nemR3DarwinMap(GCPhys, pvR3, cb, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_WRITE | NEM_PAGE_PROT_EXECUTE);
        if (RT_SUCCESS(rc))
            *pu2State = NEM_DARWIN_PAGE_STATE_WRITABLE;
        else
        {
            LogRel(("NEMR3NotifyPhysRamRegister: GCPhys=%RGp LB %RGp pvR3=%p rc=%Rrc\n", GCPhys, cb, pvR3, rc));
            return VERR_NEM_MAP_PAGES_FAILED;
        }
    }
    return VINF_SUCCESS;
#else
    RT_NOREF(pVM, GCPhys, cb, pvR3);
    return VERR_NEM_MAP_PAGES_FAILED;
#endif
}


VMMR3_INT_DECL(bool) NEMR3IsMmio2DirtyPageTrackingSupported(PVM pVM)
{
    RT_NOREF(pVM);
    return false;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExMapEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags,
                                                  void *pvRam, void *pvMmio2, uint8_t *pu2State, uint32_t *puNemRange)
{
    RT_NOREF(pVM, puNemRange);

    Log5(("NEMR3NotifyPhysMmioExMapEarly: %RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p pu2State=%p (%d)\n",
          GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State, *pu2State));

#if defined(VBOX_WITH_PGM_NEM_MODE)
    /*
     * Unmap the RAM we're replacing.
     */
    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE)
    {
        int rc = nemR3DarwinUnmap(GCPhys, cb);
        if (RT_SUCCESS(rc))
        { /* likely */ }
        else if (pvMmio2)
            LogRel(("NEMR3NotifyPhysMmioExMapEarly: GCPhys=%RGp LB %RGp fFlags=%#x: Unmap -> rc=%Rc(ignored)\n",
                    GCPhys, cb, fFlags, rc));
        else
        {
            LogRel(("NEMR3NotifyPhysMmioExMapEarly: GCPhys=%RGp LB %RGp fFlags=%#x: Unmap -> rc=%Rrc\n",
                    GCPhys, cb, fFlags, rc));
            return VERR_NEM_UNMAP_PAGES_FAILED;
        }
    }

    /*
     * Map MMIO2 if any.
     */
    if (pvMmio2)
    {
        Assert(fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2);
        int rc = nemR3DarwinMap(GCPhys, pvMmio2, cb, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_WRITE | NEM_PAGE_PROT_EXECUTE);
        if (RT_SUCCESS(rc))
            *pu2State = NEM_DARWIN_PAGE_STATE_WRITABLE;
        else
        {
            LogRel(("NEMR3NotifyPhysMmioExMapEarly: GCPhys=%RGp LB %RGp fFlags=%#x pvMmio2=%p: Map -> rc=%Rrc\n",
                    GCPhys, cb, fFlags, pvMmio2, rc));
            return VERR_NEM_MAP_PAGES_FAILED;
        }
    }
    else
    {
        Assert(!(fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2));
        *pu2State = NEM_DARWIN_PAGE_STATE_UNMAPPED;
    }

#else
    RT_NOREF(pVM, GCPhys, cb, pvRam, pvMmio2);
    *pu2State = (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE) ? UINT8_MAX : NEM_DARWIN_PAGE_STATE_UNMAPPED;
#endif
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExMapLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags,
                                                 void *pvRam, void *pvMmio2, uint32_t *puNemRange)
{
    RT_NOREF(pVM, GCPhys, cb, fFlags, pvRam, pvMmio2, puNemRange);
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExUnmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags, void *pvRam,
                                               void *pvMmio2, uint8_t *pu2State)
{
    RT_NOREF(pVM);

    Log5(("NEMR3NotifyPhysMmioExUnmap: %RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p pu2State=%p\n",
          GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State));

    int rc = VINF_SUCCESS;
#if defined(VBOX_WITH_PGM_NEM_MODE)
    /*
     * Unmap the MMIO2 pages.
     */
    /** @todo If we implement aliasing (MMIO2 page aliased into MMIO range),
     *        we may have more stuff to unmap even in case of pure MMIO... */
    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2)
    {
        rc = nemR3DarwinUnmap(GCPhys, cb);
        if (RT_FAILURE(rc))
        {
            LogRel2(("NEMR3NotifyPhysMmioExUnmap: GCPhys=%RGp LB %RGp fFlags=%#x: Unmap -> rc=%Rrc\n",
                     GCPhys, cb, fFlags, rc));
            rc = VERR_NEM_UNMAP_PAGES_FAILED;
        }
    }

    /*
     * Restore the RAM we replaced.
     */
    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE)
    {
        AssertPtr(pvRam);
        rc = nemR3DarwinMap(GCPhys, pvRam, cb, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_WRITE | NEM_PAGE_PROT_EXECUTE);
        if (RT_SUCCESS(rc))
        { /* likely */ }
        else
        {
            LogRel(("NEMR3NotifyPhysMmioExUnmap: GCPhys=%RGp LB %RGp pvMmio2=%p rc=%Rrc\n", GCPhys, cb, pvMmio2, rc));
            rc = VERR_NEM_MAP_PAGES_FAILED;
        }
        if (pu2State)
            *pu2State = NEM_DARWIN_PAGE_STATE_WRITABLE;
    }
    /* Mark the pages as unmapped if relevant. */
    else if (pu2State)
        *pu2State = NEM_DARWIN_PAGE_STATE_UNMAPPED;

    RT_NOREF(pvMmio2);
#else
    RT_NOREF(pVM, GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State);
    if (pu2State)
        *pu2State = UINT8_MAX;
    rc = VERR_NEM_UNMAP_PAGES_FAILED;
#endif
    return rc;
}


VMMR3_INT_DECL(int) NEMR3PhysMmio2QueryAndResetDirtyBitmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t uNemRange,
                                                           void *pvBitmap, size_t cbBitmap)
{
    RT_NOREF(pVM, GCPhys, cb, uNemRange, pvBitmap, cbBitmap);
    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvPages, uint32_t fFlags,
                                                     uint8_t *pu2State)
{
    RT_NOREF(pVM, GCPhys, cb, pvPages, fFlags);

    Log5(("nemR3NativeNotifyPhysRomRegisterEarly: %RGp LB %RGp pvPages=%p fFlags=%#x\n", GCPhys, cb, pvPages, fFlags));
    *pu2State = UINT8_MAX;
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvPages,
                                                    uint32_t fFlags, uint8_t *pu2State)
{
    Log5(("nemR3NativeNotifyPhysRomRegisterLate: %RGp LB %RGp pvPages=%p fFlags=%#x pu2State=%p\n",
          GCPhys, cb, pvPages, fFlags, pu2State));
    *pu2State = UINT8_MAX;

#if defined(VBOX_WITH_PGM_NEM_MODE)
    /*
     * (Re-)map readonly.
     */
    AssertPtrReturn(pvPages, VERR_INVALID_POINTER);
    int rc = nemR3DarwinMap(GCPhys, pvPages, cb, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_EXECUTE);
    if (RT_SUCCESS(rc))
        *pu2State = NEM_DARWIN_PAGE_STATE_READABLE;
    else
    {
        LogRel(("nemR3NativeNotifyPhysRomRegisterLate: GCPhys=%RGp LB %RGp pvPages=%p fFlags=%#x rc=%Rrc\n",
                GCPhys, cb, pvPages, fFlags, rc));
        return VERR_NEM_MAP_PAGES_FAILED;
    }
    RT_NOREF(pVM, fFlags);
    return VINF_SUCCESS;
#else
    RT_NOREF(pVM, GCPhys, cb, pvPages, fFlags);
    return VERR_NEM_MAP_PAGES_FAILED;
#endif
}


VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalDeregister(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb,
                                                        RTR3PTR pvMemR3, uint8_t *pu2State)
{
    RT_NOREF(pVM);

    Log5(("NEMHCNotifyHandlerPhysicalDeregister: %RGp LB %RGp enmKind=%d pvMemR3=%p pu2State=%p (%d)\n",
          GCPhys, cb, enmKind, pvMemR3, pu2State, *pu2State));

    *pu2State = UINT8_MAX;
#if defined(VBOX_WITH_PGM_NEM_MODE)
    if (pvMemR3)
    {
        int rc = nemR3DarwinMap(GCPhys, pvMemR3, cb, NEM_PAGE_PROT_READ | NEM_PAGE_PROT_WRITE | NEM_PAGE_PROT_EXECUTE);
        if (RT_SUCCESS(rc))
            *pu2State = NEM_DARWIN_PAGE_STATE_WRITABLE;
        else
            AssertLogRelMsgFailed(("NEMHCNotifyHandlerPhysicalDeregister: nemR3DarwinMap(,%p,%RGp,%RGp,) -> %Rrc\n",
                                   pvMemR3, GCPhys, cb, rc));
    }
    RT_NOREF(enmKind);
#else
    RT_NOREF(pVM, enmKind, GCPhys, cb, pvMemR3);
    AssertFailed();
#endif
}


static int nemHCJustUnmapPage(PVMCC pVM, RTGCPHYS GCPhysDst, uint8_t *pu2State)
{
    if (*pu2State <= NEM_DARWIN_PAGE_STATE_UNMAPPED)
    {
        Log5(("nemHCJustUnmapPage: %RGp == unmapped\n", GCPhysDst));
        *pu2State = NEM_DARWIN_PAGE_STATE_UNMAPPED;
        return VINF_SUCCESS;
    }

    int rc = nemR3DarwinUnmap(GCPhysDst & ~(RTGCPHYS)X86_PAGE_OFFSET_MASK, X86_PAGE_SIZE);
    if (RT_SUCCESS(rc))
    {
        STAM_REL_COUNTER_INC(&pVM->nem.s.StatUnmapPage);
        uint32_t cMappedPages = ASMAtomicDecU32(&pVM->nem.s.cMappedPages); NOREF(cMappedPages);
        *pu2State = NEM_DARWIN_PAGE_STATE_UNMAPPED;
        Log5(("nemHCJustUnmapPage: %RGp => unmapped (total %u)\n", GCPhysDst, cMappedPages));
        return VINF_SUCCESS;
    }
    STAM_REL_COUNTER_INC(&pVM->nem.s.StatUnmapPageFailed);
    LogRel(("nemHCJustUnmapPage(%RGp): failed! rc=%Rrc\n",
            GCPhysDst, rc));
    return VERR_NEM_IPE_6;
}


/**
 * Called when the A20 state changes.
 *
 * @param   pVCpu           The CPU the A20 state changed on.
 * @param   fEnabled        Whether it was enabled (true) or disabled.
 */
VMMR3_INT_DECL(void) NEMR3NotifySetA20(PVMCPU pVCpu, bool fEnabled)
{
    Log(("NEMR3NotifySetA20: fEnabled=%RTbool\n", fEnabled));
    RT_NOREF(pVCpu, fEnabled);
}


void nemHCNativeNotifyHandlerPhysicalRegister(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb)
{
    Log5(("nemHCNativeNotifyHandlerPhysicalRegister: %RGp LB %RGp enmKind=%d\n", GCPhys, cb, enmKind));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhys); NOREF(cb);
}


void nemHCNativeNotifyHandlerPhysicalModify(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                            RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM)
{
    Log5(("nemHCNativeNotifyHandlerPhysicalModify: %RGp LB %RGp -> %RGp enmKind=%d fRestoreAsRAM=%d\n",
          GCPhysOld, cb, GCPhysNew, enmKind, fRestoreAsRAM));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhysOld); NOREF(GCPhysNew); NOREF(cb); NOREF(fRestoreAsRAM);
}


int nemHCNativeNotifyPhysPageAllocated(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                       PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("nemHCNativeNotifyPhysPageAllocated: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF_PV(HCPhys); RT_NOREF_PV(enmType);

    return nemHCJustUnmapPage(pVM, GCPhys, pu2State);
}


VMM_INT_DECL(void) NEMHCNotifyPhysPageProtChanged(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, RTR3PTR pvR3, uint32_t fPageProt,
                                                  PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("NEMHCNotifyPhysPageProtChanged: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF(HCPhys, pvR3, fPageProt, enmType)

    nemHCJustUnmapPage(pVM, GCPhys, pu2State);
}


VMM_INT_DECL(void) NEMHCNotifyPhysPageChanged(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew,
                                              RTR3PTR pvNewR3, uint32_t fPageProt, PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("NEMHCNotifyPhysPageChanged: %RGp HCPhys=%RHp->%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhysPrev, HCPhysNew, fPageProt, enmType, *pu2State));
    RT_NOREF(HCPhysPrev, HCPhysNew, pvNewR3, fPageProt, enmType);

    nemHCJustUnmapPage(pVM, GCPhys, pu2State);
}


/**
 * Interface for importing state on demand (used by IEM).
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context CPU structure.
 * @param   fWhat       What to import, CPUMCTX_EXTRN_XXX.
 */
VMM_INT_DECL(int) NEMImportStateOnDemand(PVMCPUCC pVCpu, uint64_t fWhat)
{
    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnDemand);

    return nemR3DarwinCopyStateFromHv(pVCpu->pVMR3, pVCpu, fWhat);
}


/**
 * Query the CPU tick counter and optionally the TSC_AUX MSR value.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context CPU structure.
 * @param   pcTicks     Where to return the CPU tick count.
 * @param   puAux       Where to return the TSC_AUX register value.
 */
VMM_INT_DECL(int) NEMHCQueryCpuTick(PVMCPUCC pVCpu, uint64_t *pcTicks, uint32_t *puAux)
{
    LogFlowFunc(("pVCpu=%p pcTicks=%RX64 puAux=%RX32\n", pVCpu, pcTicks, puAux));
    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatQueryCpuTick);

    RT_NOREF(pVCpu, pcTicks, puAux);
    return VINF_SUCCESS;
}


/**
 * Resumes CPU clock (TSC) on all virtual CPUs.
 *
 * This is called by TM when the VM is started, restored, resumed or similar.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context CPU structure of the calling EMT.
 * @param   uPausedTscValue The TSC value at the time of pausing.
 */
VMM_INT_DECL(int) NEMHCResumeCpuTickOnAll(PVMCC pVM, PVMCPUCC pVCpu, uint64_t uPausedTscValue)
{
    LogFlowFunc(("pVM=%p pVCpu=%p uPausedTscValue=%RX64\n", pVCpu, uPausedTscValue));
    VMCPU_ASSERT_EMT_RETURN(pVCpu, VERR_VM_THREAD_NOT_EMT);
    AssertReturn(VM_IS_NEM_ENABLED(pVM), VERR_NEM_IPE_9);

    RT_NOREF(uPausedTscValue);
    return VINF_SUCCESS;
}


/** @page pg_nem_darwin NEM/darwin - Native Execution Manager, macOS.
 *
 * @todo Add notes as the implementation progresses...
 */

