/* $Id$ */
/** @file
 * HWACCM VT-x - Internal header file.
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

#ifndef ___HWVMXR0_h
#define ___HWVMXR0_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/em.h>
#include <VBox/stam.h>
#include <VBox/dis.h>
#include <VBox/hwaccm.h>
#include <VBox/pgm.h>
#include <VBox/hwacc_vmx.h>

__BEGIN_DECLS

/** @defgroup grp_vmx_int   Internal
 * @ingroup grp_vmx
 * @internal
 * @{
 */

/* Read cache indices. */
#define VMX_VMCS64_GUEST_RIP_CACHE_IDX                                      0
#define VMX_VMCS64_GUEST_RSP_CACHE_IDX                                      1
#define VMX_VMCS_GUEST_RFLAGS_CACHE_IDX                                     2
#define VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE_CACHE_IDX                   3
#define VMX_VMCS_CTRL_CR0_READ_SHADOW_CACHE_IDX                             4
#define VMX_VMCS64_GUEST_CR0_CACHE_IDX                                      5
#define VMX_VMCS_CTRL_CR4_READ_SHADOW_CACHE_IDX                             6
#define VMX_VMCS64_GUEST_CR4_CACHE_IDX                                      7
#define VMX_VMCS64_GUEST_DR7_CACHE_IDX                                      8
#define VMX_VMCS32_GUEST_SYSENTER_CS_CACHE_IDX                              9
#define VMX_VMCS64_GUEST_SYSENTER_EIP_CACHE_IDX                             10
#define VMX_VMCS64_GUEST_SYSENTER_ESP_CACHE_IDX                             11
#define VMX_VMCS32_GUEST_GDTR_LIMIT_CACHE_IDX                               12
#define VMX_VMCS64_GUEST_GDTR_BASE_CACHE_IDX                                13
#define VMX_VMCS32_GUEST_IDTR_LIMIT_CACHE_IDX                               14
#define VMX_VMCS64_GUEST_IDTR_BASE_CACHE_IDX                                15
#define VMX_VMCS16_GUEST_FIELD_CS_CACHE_IDX                                 16
#define VMX_VMCS32_GUEST_CS_LIMIT_CACHE_IDX                                 17
#define VMX_VMCS64_GUEST_CS_BASE_CACHE_IDX                                  18
#define VMX_VMCS32_GUEST_CS_ACCESS_RIGHTS_CACHE_IDX                         19
#define VMX_VMCS16_GUEST_FIELD_DS_CACHE_IDX                                 20
#define VMX_VMCS32_GUEST_DS_LIMIT_CACHE_IDX                                 21
#define VMX_VMCS64_GUEST_DS_BASE_CACHE_IDX                                  22
#define VMX_VMCS32_GUEST_DS_ACCESS_RIGHTS_CACHE_IDX                         23
#define VMX_VMCS16_GUEST_FIELD_ES_CACHE_IDX                                 24
#define VMX_VMCS32_GUEST_ES_LIMIT_CACHE_IDX                                 25
#define VMX_VMCS64_GUEST_ES_BASE_CACHE_IDX                                  26
#define VMX_VMCS32_GUEST_ES_ACCESS_RIGHTS_CACHE_IDX                         27
#define VMX_VMCS16_GUEST_FIELD_FS_CACHE_IDX                                 28
#define VMX_VMCS32_GUEST_FS_LIMIT_CACHE_IDX                                 29
#define VMX_VMCS64_GUEST_FS_BASE_CACHE_IDX                                  30
#define VMX_VMCS32_GUEST_FS_ACCESS_RIGHTS_CACHE_IDX                         31
#define VMX_VMCS16_GUEST_FIELD_GS_CACHE_IDX                                 32
#define VMX_VMCS32_GUEST_GS_LIMIT_CACHE_IDX                                 33
#define VMX_VMCS64_GUEST_GS_BASE_CACHE_IDX                                  34
#define VMX_VMCS32_GUEST_GS_ACCESS_RIGHTS_CACHE_IDX                         35
#define VMX_VMCS16_GUEST_FIELD_SS_CACHE_IDX                                 36
#define VMX_VMCS32_GUEST_SS_LIMIT_CACHE_IDX                                 37
#define VMX_VMCS64_GUEST_SS_BASE_CACHE_IDX                                  38
#define VMX_VMCS32_GUEST_SS_ACCESS_RIGHTS_CACHE_IDX                         39
#define VMX_VMCS16_GUEST_FIELD_TR_CACHE_IDX                                 40
#define VMX_VMCS32_GUEST_TR_LIMIT_CACHE_IDX                                 41
#define VMX_VMCS64_GUEST_TR_BASE_CACHE_IDX                                  42
#define VMX_VMCS32_GUEST_TR_ACCESS_RIGHTS_CACHE_IDX                         43
#define VMX_VMCS16_GUEST_FIELD_LDTR_CACHE_IDX                               44
#define VMX_VMCS32_GUEST_LDTR_LIMIT_CACHE_IDX                               45
#define VMX_VMCS64_GUEST_LDTR_BASE_CACHE_IDX                                46
#define VMX_VMCS32_GUEST_LDTR_ACCESS_RIGHTS_CACHE_IDX                       47
#define VMX_VMCS32_RO_EXIT_REASON_CACHE_IDX                                 48
#define VMX_VMCS32_RO_VM_INSTR_ERROR_CACHE_IDX                              49
#define VMX_VMCS32_RO_EXIT_INSTR_LENGTH_CACHE_IDX                           50
#define VMX_VMCS32_RO_EXIT_INTERRUPTION_ERRCODE_CACHE_IDX                   51
#define VMX_VMCS32_RO_EXIT_INSTR_INFO_CACHE_IDX                             52
#define VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO_CACHE_IDX                      53
#define VMX_VMCS_RO_EXIT_QUALIFICATION_CACHE_IDX                            54
#define VMX_VMCS32_RO_IDT_INFO_CACHE_IDX                                    55
#define VMX_VMCS32_RO_IDT_ERRCODE_CACHE_IDX                                 56
#define VMX_VMCS_MAX_CACHE_IDX                                              (VMX_VMCS32_RO_IDT_ERRCODE_CACHE_IDX+1)
#define VMX_VMCS64_GUEST_CR3_CACHE_IDX                                      57
#define VMX_VMCS_EXIT_PHYS_ADDR_FULL_CACHE_IDX                              58
#define VMX_VMCS_MAX_NESTED_PAGING_CACHE_IDX                                (VMX_VMCS_EXIT_PHYS_ADDR_FULL_CACHE_IDX+1)


#ifdef IN_RING0

/**
 * Enters the VT-x session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCpu        CPU info struct
 */
VMMR0DECL(int) VMXR0Enter(PVM pVM, PVMCPU pVCpu, PHWACCM_CPUINFO pCpu);

/**
 * Leaves the VT-x session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        CPU context
 */
VMMR0DECL(int) VMXR0Leave(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);


/**
 * Sets up and activates VT-x on the current CPU
 *
 * @returns VBox status code.
 * @param   pCpu            CPU info struct
 * @param   pVM             The VM to operate on. (can be NULL after a resume)
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
VMMR0DECL(int) VMXR0EnableCpu(PHWACCM_CPUINFO pCpu, PVM pVM, void *pvPageCpu, RTHCPHYS pPageCpuPhys);

/**
 * Deactivates VT-x on the current CPU
 *
 * @returns VBox status code.
 * @param   pCpu            CPU info struct
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
VMMR0DECL(int) VMXR0DisableCpu(PHWACCM_CPUINFO pCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys);

/**
 * Does Ring-0 per VM VT-x init.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) VMXR0InitVM(PVM pVM);

/**
 * Does Ring-0 per VM VT-x termination.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) VMXR0TermVM(PVM pVM);

/**
 * Sets up VT-x for the specified VM
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) VMXR0SetupVM(PVM pVM);


/**
 * Save the host state
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 */
VMMR0DECL(int) VMXR0SaveHostState(PVM pVM, PVMCPU pVCpu);

/**
 * Loads the guest state
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        Guest context
 */
VMMR0DECL(int) VMXR0LoadGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);


/**
 * Runs guest code in a VT-x VM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        Guest context
 */
VMMR0DECL(int) VMXR0RunGuestCode(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);


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
VMMR0DECL(int) VMXR0Execute64BitsHandler(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, RTRCPTR pfnHandler, uint32_t cbParam, uint32_t *paParam);

# define VMX_WRITE_SELREG(REG, reg) \
{                                                                                               \
        rc  = VMXWriteCachedVMCS(VMX_VMCS16_GUEST_FIELD_##REG,      pCtx->reg);                 \
        rc |= VMXWriteCachedVMCS(VMX_VMCS32_GUEST_##REG##_LIMIT,    pCtx->reg##Hid.u32Limit);   \
        rc |= VMXWriteCachedVMCS(VMX_VMCS64_GUEST_##REG##_BASE,     pCtx->reg##Hid.u64Base);    \
        if ((pCtx->eflags.u32 & X86_EFL_VM))                                                    \
            val = pCtx->reg##Hid.Attr.u;                                                        \
        else                                                                                    \
        if (CPUMIsGuestInRealModeEx(pCtx))                                                      \
        {                                                                                       \
            /* Must override this or else VT-x will fail with invalid guest state errors. */    \
            /* DPL=3, present, code/data, r/w/accessed. */                                      \
            val = 0xf3;                                                                         \
        }                                                                                       \
        else                                                                                    \
        if (   ((!pCtx->csHid.Attr.n.u1DefBig && !CPUMIsGuestIn64BitCodeEx(pCtx)) || pCtx->reg) \
            && pCtx->reg##Hid.Attr.n.u1Present == 1)                                            \
            val = pCtx->reg##Hid.Attr.u | X86_SEL_TYPE_ACCESSED;                                \
        else                                                                                    \
            val = 0x10000;  /* Invalid guest state error otherwise. (BIT(16) = Unusable) */     \
                                                                                                \
        rc |= VMXWriteCachedVMCS(VMX_VMCS32_GUEST_##REG##_ACCESS_RIGHTS, val);                  \
}

# define VMX_READ_SELREG(REG, reg) \
{                                                                    \
        VMXReadCachedVMCS(VMX_VMCS16_GUEST_FIELD_##REG,           &val);   \
        pCtx->reg                = val;                              \
        VMXReadCachedVMCS(VMX_VMCS32_GUEST_##REG##_LIMIT,         &val);   \
        pCtx->reg##Hid.u32Limit    = val;                            \
        VMXReadCachedVMCS(VMX_VMCS64_GUEST_##REG##_BASE,          &val);   \
        pCtx->reg##Hid.u64Base     = val;                            \
        VMXReadCachedVMCS(VMX_VMCS32_GUEST_##REG##_ACCESS_RIGHTS, &val);   \
        pCtx->reg##Hid.Attr.u    = val;                              \
}

/* Don't read from the cache in this macro; used only in case of failure where the cache is out of sync. */
# define VMX_LOG_SELREG(REG, szSelReg) \
{                                                                    \
        VMXReadVMCS(VMX_VMCS16_GUEST_FIELD_##REG,           &val);   \
        Log(("%s Selector     %x\n", szSelReg, val));                \
        VMXReadVMCS(VMX_VMCS32_GUEST_##REG##_LIMIT,         &val);   \
        Log(("%s Limit        %x\n", szSelReg, val));                \
        VMXReadVMCS(VMX_VMCS64_GUEST_##REG##_BASE,          &val);   \
        Log(("%s Base         %RX64\n", szSelReg, val));             \
        VMXReadVMCS(VMX_VMCS32_GUEST_##REG##_ACCESS_RIGHTS, &val);   \
        Log(("%s Attributes   %x\n", szSelReg, val));                \
}

#ifdef VMX_USE_CACHED_VMCS_ACCESSES
/**
 * Cache VMCS writes for performance reasons (Darwin) and for running 64 bits guests on 32 bits hosts.
 *
 * @param   pVCpu       The VMCPU to operate on.
 * @param   idxField    VMCS field
 * @param   u64Val      Value
 */
DECLINLINE(int) VMXWriteCachedVMCSEx(PVMCPU pVCpu, uint32_t idxField, uint64_t u64Val)
{
    PVMCSCACHE pCache = &pVCpu->hwaccm.s.vmx.VMCSCache;

    Assert(pCache->Write.cValidEntries < VMCSCACHE_MAX_ENTRY - 1);
    pCache->Write.aField[pCache->Write.cValidEntries]    = idxField;
    pCache->Write.aFieldVal[pCache->Write.cValidEntries] = u64Val;
    pCache->Write.cValidEntries++;
    return VINF_SUCCESS;
}
#endif

/**
 * Cache VMCS writes for performance reasons (Darwin) and for running 64 bits guests on 32 bits hosts.
 *
 * @param   idxField    VMCS field
 * @param   val         Field value
 */
#ifdef VMX_USE_CACHED_VMCS_ACCESSES
# define VMXWriteCachedVMCS(idxField, uVal)              VMXWriteCachedVMCSEx(pVCpu, idxField, uVal)
# define VMXWriteCachedVMCS64(idxField, uVal)            VMXWriteVMCS64(idxField, (uVal))
# if 0
#  if HC_ARCH_BITS == 64 || defined(RT_OS_DARWIN)
#   define VMXWriteCachedVMCS64(idxField, uVal)           VMXWriteCachedVMCSEx(pVCpu, idxField, uVal)
#  else
#   define VMXWriteCachedVMCS64(idxField, uVal)           (CPUMIsGuestInLongModeEx(pCtx)) ? VMXWriteCachedVMCSEx(pVCpu, idxField, uVal) : (VMXWriteCachedVMCSEx(pVCpu, idxField, uVal) | VMXWriteCachedVMCSEx(pVCpu, idxField + 1, (uVal >> 32ULL)))
#  endif
# endif
#else
# define VMXWriteCachedVMCS(idxField, uVal)              VMXWriteVMCS(idxField, (uVal))
# define VMXWriteCachedVMCS64(idxField, uVal)            VMXWriteVMCS64(idxField, (uVal))
#endif

#ifdef VMX_USE_CACHED_VMCS_ACCESSES
/**
 * Return value of cached VMCS read for performance reasons (Darwin) and for running 64 bits guests on 32 bits hosts.
 *
 * @param   pVCpu       The VMCPU to operate on.
 * @param   idxField    VMCS cache index (not VMCS field index!)
 * @param   pVal        Value
 */
DECLINLINE(int) VMXReadCachedVMCSEx(PVMCPU pVCpu, uint32_t idxCache, RTCCUINTREG *pVal)
{
    Assert(idxCache <= VMX_VMCS_MAX_NESTED_PAGING_CACHE_IDX);
    *pVal = pVCpu->hwaccm.s.vmx.VMCSCache.Read.aFieldVal[idxCache];
    return VINF_SUCCESS;
}
#endif

/**
 * Return value of cached VMCS read for performance reasons (Darwin) and for running 64 bits guests on 32 bits hosts.
 *
 * @param   idxField    VMCS field
 * @param   pVal        Value pointer (out)
 */
#ifdef VMX_USE_CACHED_VMCS_ACCESSES
# define VMXReadCachedVMCS(idxField, pVal)              VMXReadCachedVMCSEx(pVCpu, idxField##_CACHE_IDX, pVal)
#else
# define VMXReadCachedVMCS(idxField, pVal)              VMXReadVMCS(idxField, pVal)
#endif

/**
 * Setup cached VMCS for performance reasons (Darwin) and for running 64 bits guests on 32 bits hosts.
 *
 * @param   pVCpu       The VMCPU to operate on.
 * @param   idxField    VMCS field
 */
#define VMXSetupCachedReadVMCS(pCache, idxField)                        \
{                                                                       \
    Assert(pCache->Read.aField[idxField##_CACHE_IDX] == 0);             \
    pCache->Read.aField[idxField##_CACHE_IDX] = idxField;               \
    pCache->Read.aFieldVal[idxField##_CACHE_IDX] = 0;                   \
}

#define VMX_SETUP_SELREG(REG, pCache)                                               \
{                                                                                   \
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS16_GUEST_FIELD_##REG);               \
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_GUEST_##REG##_LIMIT);             \
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS64_GUEST_##REG##_BASE);              \
        VMXSetupCachedReadVMCS(pCache, VMX_VMCS32_GUEST_##REG##_ACCESS_RIGHTS);     \
}

/**
 * Prepares for and executes VMLAUNCH (32 bits guest mode)
 *
 * @returns VBox status code
 * @param   fResume     vmlauch/vmresume
 * @param   pCtx        Guest context
 * @param   pCache      VMCS cache
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 */
DECLASM(int) VMXR0StartVM32(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu);

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
DECLASM(int) VMXR0StartVM64(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu);

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
DECLASM(int) VMXR0SwitcherStartVM64(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu);

#endif /* IN_RING0 */

/** @} */

__END_DECLS

#endif

