/** @file
 * HM - VMX Structures and Definitions. (VMM)
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef VBOX_INCLUDED_vmm_hmvmxinline_h
#define VBOX_INCLUDED_vmm_hmvmxinline_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/hm_vmx.h>
#include <VBox/err.h>

/* In Visual C++ versions prior to 2012, the vmx intrinsics are only available
   when targeting AMD64. */
#if RT_INLINE_ASM_USES_INTRIN >= 16 && defined(RT_ARCH_AMD64)
# pragma warning(push)
# pragma warning(disable:4668) /* Several incorrect __cplusplus uses. */
# pragma warning(disable:4255) /* Incorrect __slwpcb prototype. */
# include <intrin.h>
# pragma warning(pop)
/* We always want them as intrinsics, no functions. */
# pragma intrinsic(__vmx_on)
# pragma intrinsic(__vmx_off)
# pragma intrinsic(__vmx_vmclear)
# pragma intrinsic(__vmx_vmptrld)
# pragma intrinsic(__vmx_vmread)
# pragma intrinsic(__vmx_vmwrite)
# define VMX_USE_MSC_INTRINSICS 1
#else
# define VMX_USE_MSC_INTRINSICS 0
#endif


/** @defgroup grp_hm_vmx_inline    VMX Inline Helpers
 * @ingroup grp_hm_vmx
 * @{
 */
/**
 * Gets the effective width of a VMCS field given it's encoding adjusted for
 * HIGH/FULL access for 64-bit fields.
 *
 * @returns The effective VMCS field width.
 * @param   uFieldEnc   The VMCS field encoding.
 *
 * @remarks Warning! This function does not verify the encoding is for a valid and
 *          supported VMCS field.
 */
DECLINLINE(uint8_t) HMVmxGetVmcsFieldWidthEff(uint32_t uFieldEnc)
{
    /* Only the "HIGH" parts of all 64-bit fields have bit 0 set. */
    if (uFieldEnc & RT_BIT(0))
        return VMXVMCSFIELDWIDTH_32BIT;

    /* Bits 13:14 contains the width of the VMCS field, see VMXVMCSFIELDWIDTH_XXX. */
    return (uFieldEnc >> 13) & 0x3;
}

/**
 * Returns whether the given VMCS field is a read-only VMCS field or not.
 *
 * @returns @c true if it's a read-only field, @c false otherwise.
 * @param   uFieldEnc   The VMCS field encoding.
 *
 * @remarks Warning! This function does not verify the encoding is for a valid and
 *          supported VMCS field.
 */
DECLINLINE(bool) HMVmxIsVmcsFieldReadOnly(uint32_t uFieldEnc)
{
    /* See Intel spec. B.4.2 "Natural-Width Read-Only Data Fields". */
    return (RT_BF_GET(uFieldEnc, VMX_BF_VMCS_ENC_TYPE) == VMXVMCSFIELDTYPE_VMEXIT_INFO);
}

/**
 * Returns whether the given VM-entry interruption-information type is valid or not.
 *
 * @returns @c true if it's a valid type, @c false otherwise.
 * @param   fSupportsMTF    Whether the Monitor-Trap Flag CPU feature is supported.
 * @param   uType           The VM-entry interruption-information type.
 */
DECLINLINE(bool) HMVmxIsEntryIntInfoTypeValid(bool fSupportsMTF, uint8_t uType)
{
    /* See Intel spec. 26.2.1.3 "VM-Entry Control Fields". */
    switch (uType)
    {
        case VMX_ENTRY_INT_INFO_TYPE_EXT_INT:
        case VMX_ENTRY_INT_INFO_TYPE_NMI:
        case VMX_ENTRY_INT_INFO_TYPE_HW_XCPT:
        case VMX_ENTRY_INT_INFO_TYPE_SW_INT:
        case VMX_ENTRY_INT_INFO_TYPE_PRIV_SW_XCPT:
        case VMX_ENTRY_INT_INFO_TYPE_SW_XCPT:           return true;
        case VMX_ENTRY_INT_INFO_TYPE_OTHER_EVENT:       return fSupportsMTF;
        default:
            return false;
    }
}

/**
 * Returns whether the given VM-entry interruption-information vector and type
 * combination is valid or not.
 *
 * @returns @c true if it's a valid vector/type combination, @c false otherwise.
 * @param   uVector     The VM-entry interruption-information vector.
 * @param   uType       The VM-entry interruption-information type.
 *
 * @remarks Warning! This function does not validate the type field individually.
 *          Use it after verifying type is valid using HMVmxIsEntryIntInfoTypeValid.
 */
DECLINLINE(bool) HMVmxIsEntryIntInfoVectorValid(uint8_t uVector, uint8_t uType)
{
    /* See Intel spec. 26.2.1.3 "VM-Entry Control Fields". */
    if (   uType == VMX_ENTRY_INT_INFO_TYPE_NMI
        && uVector != X86_XCPT_NMI)
        return false;
    if (   uType == VMX_ENTRY_INT_INFO_TYPE_HW_XCPT
        && uVector > X86_XCPT_LAST)
        return false;
    if (   uType == VMX_ENTRY_INT_INFO_TYPE_OTHER_EVENT
        && uVector != VMX_ENTRY_INT_INFO_VECTOR_MTF)
        return false;
    return true;
}


/**
 * Returns whether or not the VM-exit is trap-like or fault-like.
 *
 * @returns @c true if it's a trap-like VM-exit, @c false otherwise.
 * @param   uExitReason     The VM-exit reason.
 *
 * @remarks Warning! This does not validate the VM-exit reason.
 */
DECLINLINE(bool) HMVmxIsVmexitTrapLike(uint32_t uExitReason)
{
    /*
     * Trap-like VM-exits - The instruction causing the VM-exit completes before the
     * VM-exit occurs.
     *
     * Fault-like VM-exits - The instruction causing the VM-exit is not completed before
     * the VM-exit occurs.
     *
     * See Intel spec. 25.5.2 "Monitor Trap Flag".
     * See Intel spec. 29.1.4 "EOI Virtualization".
     * See Intel spec. 29.4.3.3 "APIC-Write VM Exits".
     * See Intel spec. 29.1.2 "TPR Virtualization".
     */
    /** @todo NSTVMX: r=ramshankar: What about VM-exits due to debug traps (single-step,
     *        I/O breakpoints, data breakpoints), debug exceptions (data breakpoint)
     *        delayed by MovSS blocking, machine-check exceptions. */
    switch (uExitReason)
    {
        case VMX_EXIT_MTF:
        case VMX_EXIT_VIRTUALIZED_EOI:
        case VMX_EXIT_APIC_WRITE:
        case VMX_EXIT_TPR_BELOW_THRESHOLD:
            return true;
    }
    return false;
}


/**
 * Returns whether the VM-entry is vectoring or not given the VM-entry interruption
 * information field.
 *
 * @returns @c true if the VM-entry is vectoring, @c false otherwise.
 * @param   uEntryIntInfo       The VM-entry interruption information field.
 * @param   pEntryIntInfoType   The VM-entry interruption information type field.
 *                              Optional, can be NULL. Only updated when this
 *                              function returns @c true.
 */
DECLINLINE(bool) HMVmxIsVmentryVectoring(uint32_t uEntryIntInfo, uint8_t *pEntryIntInfoType)
{
    /*
     * The definition of what is a vectoring VM-entry is taken
     * from Intel spec. 26.6 "Special Features of VM Entry".
     */
    if (!VMX_ENTRY_INT_INFO_IS_VALID(uEntryIntInfo))
        return false;

    /* Scope and keep variable defines on top to satisy archaic c89 nonsense. */
    {
        uint8_t const uType = VMX_ENTRY_INT_INFO_TYPE(uEntryIntInfo);
        switch (uType)
        {
            case VMX_ENTRY_INT_INFO_TYPE_EXT_INT:
            case VMX_ENTRY_INT_INFO_TYPE_NMI:
            case VMX_ENTRY_INT_INFO_TYPE_HW_XCPT:
            case VMX_ENTRY_INT_INFO_TYPE_SW_INT:
            case VMX_ENTRY_INT_INFO_TYPE_PRIV_SW_XCPT:
            case VMX_ENTRY_INT_INFO_TYPE_SW_XCPT:
            {
                if (pEntryIntInfoType)
                    *pEntryIntInfoType = uType;
                return true;
            }
        }
    }
    return false;
}
/** @} */


/** @defgroup grp_hm_vmx_asm    VMX Assembly Helpers
 * @{
 */

/**
 * Restores some host-state fields that need not be done on every VM-exit.
 *
 * @returns VBox status code.
 * @param   fRestoreHostFlags   Flags of which host registers needs to be
 *                              restored.
 * @param   pRestoreHost        Pointer to the host-restore structure.
 */
DECLASM(int) VMXRestoreHostState(uint32_t fRestoreHostFlags, PVMXRESTOREHOST pRestoreHost);


/**
 * Dispatches an NMI to the host.
 */
DECLASM(int) VMXDispatchHostNmi(void);


/**
 * Executes VMXON.
 *
 * @returns VBox status code.
 * @param   HCPhysVmxOn      Physical address of VMXON structure.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXEnable(RTHCPHYS HCPhysVmxOn);
#else
DECLINLINE(int) VMXEnable(RTHCPHYS HCPhysVmxOn)
{
# if RT_INLINE_ASM_GNU_STYLE
    int rc = VINF_SUCCESS;
    __asm__ __volatile__ (
       "push     %3                                             \n\t"
       "push     %2                                             \n\t"
       ".byte    0xf3, 0x0f, 0xc7, 0x34, 0x24  # VMXON [esp]    \n\t"
       "ja       2f                                             \n\t"
       "je       1f                                             \n\t"
       "movl     $" RT_XSTR(VERR_VMX_INVALID_VMXON_PTR)", %0    \n\t"
       "jmp      2f                                             \n\t"
       "1:                                                      \n\t"
       "movl     $" RT_XSTR(VERR_VMX_VMXON_FAILED)", %0         \n\t"
       "2:                                                      \n\t"
       "add      $8, %%esp                                      \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "ir"((uint32_t)HCPhysVmxOn),        /* don't allow direct memory reference here, */
        "ir"((uint32_t)(HCPhysVmxOn >> 32)) /* this would not work with -fomit-frame-pointer */
       :"memory"
       );
    return rc;

# elif VMX_USE_MSC_INTRINSICS
    unsigned char rcMsc = __vmx_on(&HCPhysVmxOn);
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return rcMsc == 2 ? VERR_VMX_INVALID_VMXON_PTR : VERR_VMX_VMXON_FAILED;

# else
    int rc = VINF_SUCCESS;
    __asm
    {
        push    dword ptr [HCPhysVmxOn + 4]
        push    dword ptr [HCPhysVmxOn]
        _emit   0xf3
        _emit   0x0f
        _emit   0xc7
        _emit   0x34
        _emit   0x24     /* VMXON [esp] */
        jnc     vmxon_good
        mov     dword ptr [rc], VERR_VMX_INVALID_VMXON_PTR
        jmp     the_end

vmxon_good:
        jnz     the_end
        mov     dword ptr [rc], VERR_VMX_VMXON_FAILED
the_end:
        add     esp, 8
    }
    return rc;
# endif
}
#endif


/**
 * Executes VMXOFF.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(void) VMXDisable(void);
#else
DECLINLINE(void) VMXDisable(void)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ (
       ".byte 0x0f, 0x01, 0xc4  # VMXOFF                        \n\t"
       );

# elif VMX_USE_MSC_INTRINSICS
    __vmx_off();

# else
    __asm
    {
        _emit   0x0f
        _emit   0x01
        _emit   0xc4   /* VMXOFF */
    }
# endif
}
#endif


/**
 * Executes VMCLEAR.
 *
 * @returns VBox status code.
 * @param   HCPhysVmcs       Physical address of VM control structure.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXClearVmcs(RTHCPHYS HCPhysVmcs);
#else
DECLINLINE(int) VMXClearVmcs(RTHCPHYS HCPhysVmcs)
{
# if RT_INLINE_ASM_GNU_STYLE
    int rc = VINF_SUCCESS;
    __asm__ __volatile__ (
       "push    %3                                              \n\t"
       "push    %2                                              \n\t"
       ".byte   0x66, 0x0f, 0xc7, 0x34, 0x24  # VMCLEAR [esp]   \n\t"
       "jnc     1f                                              \n\t"
       "movl    $" RT_XSTR(VERR_VMX_INVALID_VMCS_PTR)", %0      \n\t"
       "1:                                                      \n\t"
       "add     $8, %%esp                                       \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "ir"((uint32_t)HCPhysVmcs),        /* don't allow direct memory reference here, */
        "ir"((uint32_t)(HCPhysVmcs >> 32)) /* this would not work with -fomit-frame-pointer */
       :"memory"
       );
    return rc;

# elif VMX_USE_MSC_INTRINSICS
    unsigned char rcMsc = __vmx_vmclear(&HCPhysVmcs);
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return VERR_VMX_INVALID_VMCS_PTR;

# else
    int rc = VINF_SUCCESS;
    __asm
    {
        push    dword ptr [HCPhysVmcs + 4]
        push    dword ptr [HCPhysVmcs]
        _emit   0x66
        _emit   0x0f
        _emit   0xc7
        _emit   0x34
        _emit   0x24     /* VMCLEAR [esp] */
        jnc     success
        mov     dword ptr [rc], VERR_VMX_INVALID_VMCS_PTR
success:
        add     esp, 8
    }
    return rc;
# endif
}
#endif


/**
 * Executes VMPTRLD.
 *
 * @returns VBox status code.
 * @param   HCPhysVmcs       Physical address of VMCS structure.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXActivateVmcs(RTHCPHYS HCPhysVmcs);
#else
DECLINLINE(int) VMXActivateVmcs(RTHCPHYS HCPhysVmcs)
{
# if RT_INLINE_ASM_GNU_STYLE
    int rc = VINF_SUCCESS;
    __asm__ __volatile__ (
       "push    %3                                              \n\t"
       "push    %2                                              \n\t"
       ".byte   0x0f, 0xc7, 0x34, 0x24  # VMPTRLD [esp]         \n\t"
       "jnc     1f                                              \n\t"
       "movl    $" RT_XSTR(VERR_VMX_INVALID_VMCS_PTR)", %0      \n\t"
       "1:                                                      \n\t"
       "add     $8, %%esp                                       \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "ir"((uint32_t)HCPhysVmcs),        /* don't allow direct memory reference here, */
        "ir"((uint32_t)(HCPhysVmcs >> 32)) /* this will not work with -fomit-frame-pointer */
       );
    return rc;

# elif VMX_USE_MSC_INTRINSICS
    unsigned char rcMsc = __vmx_vmptrld(&HCPhysVmcs);
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return VERR_VMX_INVALID_VMCS_PTR;

# else
    int rc = VINF_SUCCESS;
    __asm
    {
        push    dword ptr [HCPhysVmcs + 4]
        push    dword ptr [HCPhysVmcs]
        _emit   0x0f
        _emit   0xc7
        _emit   0x34
        _emit   0x24     /* VMPTRLD [esp] */
        jnc     success
        mov     dword ptr [rc], VERR_VMX_INVALID_VMCS_PTR

success:
        add     esp, 8
    }
    return rc;
# endif
}
#endif


/**
 * Executes VMPTRST.
 *
 * @returns VBox status code.
 * @param   pHCPhysVmcs    Where to store the physical address of the current
 *                         VMCS.
 */
DECLASM(int) VMXGetActivatedVmcs(RTHCPHYS *pHCPhysVmcs);


/**
 * Executes VMWRITE.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_VMX_INVALID_VMCS_PTR.
 * @retval  VERR_VMX_INVALID_VMCS_FIELD.
 *
 * @param   uFieldEnc       VMCS field encoding.
 * @param   u32Val          The 32-bit value to set.
 *
 * @remarks The values of the two status codes can be OR'ed together, the result
 *          will be VERR_VMX_INVALID_VMCS_PTR.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXWriteVmcs32(uint32_t uFieldEnc, uint32_t u32Val);
#else
DECLINLINE(int) VMXWriteVmcs32(uint32_t uFieldEnc, uint32_t u32Val)
{
# if RT_INLINE_ASM_GNU_STYLE
    int rc = VINF_SUCCESS;
    __asm__ __volatile__ (
       ".byte  0x0f, 0x79, 0xc2        # VMWRITE eax, edx       \n\t"
       "ja     2f                                               \n\t"
       "je     1f                                               \n\t"
       "movl   $" RT_XSTR(VERR_VMX_INVALID_VMCS_PTR)", %0       \n\t"
       "jmp    2f                                               \n\t"
       "1:                                                      \n\t"
       "movl   $" RT_XSTR(VERR_VMX_INVALID_VMCS_FIELD)", %0     \n\t"
       "2:                                                      \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "a"(uFieldEnc),
        "d"(u32Val)
       );
    return rc;

# elif VMX_USE_MSC_INTRINSICS
     unsigned char rcMsc = __vmx_vmwrite(uFieldEnc, u32Val);
     if (RT_LIKELY(rcMsc == 0))
         return VINF_SUCCESS;
     return rcMsc == 2 ? VERR_VMX_INVALID_VMCS_PTR : VERR_VMX_INVALID_VMCS_FIELD;

#else
    int rc = VINF_SUCCESS;
    __asm
    {
        push   dword ptr [u32Val]
        mov    eax, [uFieldEnc]
        _emit  0x0f
        _emit  0x79
        _emit  0x04
        _emit  0x24     /* VMWRITE eax, [esp] */
        jnc    valid_vmcs
        mov    dword ptr [rc], VERR_VMX_INVALID_VMCS_PTR
        jmp    the_end

valid_vmcs:
        jnz    the_end
        mov    dword ptr [rc], VERR_VMX_INVALID_VMCS_FIELD
the_end:
        add    esp, 4
    }
    return rc;
# endif
}
#endif

/**
 * Executes VMWRITE.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_VMX_INVALID_VMCS_PTR.
 * @retval  VERR_VMX_INVALID_VMCS_FIELD.
 *
 * @param   uFieldEnc       The VMCS field encoding.
 * @param   u64Val          The 16, 32 or 64-bit value to set.
 *
 * @remarks The values of the two status codes can be OR'ed together, the result
 *          will be VERR_VMX_INVALID_VMCS_PTR.
 */
#if !defined(RT_ARCH_X86)
# if !VMX_USE_MSC_INTRINSICS || ARCH_BITS != 64
DECLASM(int) VMXWriteVmcs64(uint32_t uFieldEnc, uint64_t u64Val);
# else  /* VMX_USE_MSC_INTRINSICS */
DECLINLINE(int) VMXWriteVmcs64(uint32_t uFieldEnc, uint64_t u64Val)
{
    unsigned char rcMsc = __vmx_vmwrite(uFieldEnc, u64Val);
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return rcMsc == 2 ? VERR_VMX_INVALID_VMCS_PTR : VERR_VMX_INVALID_VMCS_FIELD;
}
# endif /* VMX_USE_MSC_INTRINSICS */
#else
# define VMXWriteVmcs64(uFieldEnc, u64Val)    VMXWriteVmcs64Ex(pVCpu, uFieldEnc, u64Val) /** @todo dead ugly, picking up pVCpu like this */
VMMR0DECL(int) VMXWriteVmcs64Ex(PVMCPU pVCpu, uint32_t uFieldEnc, uint64_t u64Val);
#endif

#if ARCH_BITS == 32
# define VMXWriteVmcsHstN                       VMXWriteVmcs32
# define VMXWriteVmcsGstN(uFieldEnc, u64Val)     VMXWriteVmcs64Ex(pVCpu, uFieldEnc, u64Val)
#else  /* ARCH_BITS == 64 */
# define VMXWriteVmcsHstN                       VMXWriteVmcs64
# define VMXWriteVmcsGstN                       VMXWriteVmcs64
#endif


/**
 * Invalidate a page using INVEPT.
 *
 * @returns VBox status code.
 * @param   enmFlush        Type of flush.
 * @param   pDescriptor     Pointer to the descriptor.
 */
DECLASM(int) VMXR0InvEPT(VMXTLBFLUSHEPT enmFlush, uint64_t *pDescriptor);


/**
 * Invalidate a page using INVVPID.
 *
 * @returns VBox status code.
 * @param   enmFlush        Type of flush.
 * @param   pDescriptor     Pointer to the descriptor.
 */
DECLASM(int) VMXR0InvVPID(VMXTLBFLUSHVPID enmFlush, uint64_t *pDescriptor);


/**
 * Executes VMREAD for a 32-bit field.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_VMX_INVALID_VMCS_PTR.
 * @retval  VERR_VMX_INVALID_VMCS_FIELD.
 *
 * @param   uFieldEnc       The VMCS field encoding.
 * @param   pData           Where to store VMCS field value.
 *
 * @remarks The values of the two status codes can be OR'ed together, the result
 *          will be VERR_VMX_INVALID_VMCS_PTR.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXReadVmcs32(uint32_t uFieldEnc, uint32_t *pData);
#else
DECLINLINE(int) VMXReadVmcs32(uint32_t uFieldEnc, uint32_t *pData)
{
# if RT_INLINE_ASM_GNU_STYLE
    int rc = VINF_SUCCESS;
    __asm__ __volatile__ (
       "movl   $" RT_XSTR(VINF_SUCCESS)", %0                     \n\t"
       ".byte  0x0f, 0x78, 0xc2        # VMREAD eax, edx         \n\t"
       "ja     2f                                                \n\t"
       "je     1f                                                \n\t"
       "movl   $" RT_XSTR(VERR_VMX_INVALID_VMCS_PTR)", %0        \n\t"
       "jmp    2f                                                \n\t"
       "1:                                                       \n\t"
       "movl   $" RT_XSTR(VERR_VMX_INVALID_VMCS_FIELD)", %0      \n\t"
       "2:                                                       \n\t"
       :"=&r"(rc),
        "=d"(*pData)
       :"a"(uFieldEnc),
        "d"(0)
       );
    return rc;

# elif VMX_USE_MSC_INTRINSICS
    unsigned char rcMsc;
#  if ARCH_BITS == 32
    rcMsc = __vmx_vmread(uFieldEnc, pData);
#  else
    uint64_t u64Tmp;
    rcMsc = __vmx_vmread(uFieldEnc, &u64Tmp);
    *pData = (uint32_t)u64Tmp;
#  endif
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return rcMsc == 2 ? VERR_VMX_INVALID_VMCS_PTR : VERR_VMX_INVALID_VMCS_FIELD;

#else
    int rc = VINF_SUCCESS;
    __asm
    {
        sub     esp, 4
        mov     dword ptr [esp], 0
        mov     eax, [uFieldEnc]
        _emit   0x0f
        _emit   0x78
        _emit   0x04
        _emit   0x24     /* VMREAD eax, [esp] */
        mov     edx, pData
        pop     dword ptr [edx]
        jnc     valid_vmcs
        mov     dword ptr [rc], VERR_VMX_INVALID_VMCS_PTR
        jmp     the_end

valid_vmcs:
        jnz     the_end
        mov     dword ptr [rc], VERR_VMX_INVALID_VMCS_FIELD
the_end:
    }
    return rc;
# endif
}
#endif

/**
 * Executes VMREAD for a 64-bit field.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_VMX_INVALID_VMCS_PTR.
 * @retval  VERR_VMX_INVALID_VMCS_FIELD.
 *
 * @param   uFieldEnc       The VMCS field encoding.
 * @param   pData           Where to store VMCS field value.
 *
 * @remarks The values of the two status codes can be OR'ed together, the result
 *          will be VERR_VMX_INVALID_VMCS_PTR.
 */
#if (!defined(RT_ARCH_X86) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXReadVmcs64(uint32_t uFieldEnc, uint64_t *pData);
#else
DECLINLINE(int) VMXReadVmcs64(uint32_t uFieldEnc, uint64_t *pData)
{
# if VMX_USE_MSC_INTRINSICS
    unsigned char rcMsc;
#  if ARCH_BITS == 32
    size_t        uLow;
    size_t        uHigh;
    rcMsc  = __vmx_vmread(uFieldEnc, &uLow);
    rcMsc |= __vmx_vmread(uFieldEnc + 1, &uHigh);
    *pData = RT_MAKE_U64(uLow, uHigh);
# else
    rcMsc = __vmx_vmread(uFieldEnc, pData);
# endif
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return rcMsc == 2 ? VERR_VMX_INVALID_VMCS_PTR : VERR_VMX_INVALID_VMCS_FIELD;

# elif ARCH_BITS == 32
    int rc;
    uint32_t val_hi, val;
    rc  = VMXReadVmcs32(uFieldEnc, &val);
    rc |= VMXReadVmcs32(uFieldEnc + 1, &val_hi);
    AssertRC(rc);
    *pData = RT_MAKE_U64(val, val_hi);
    return rc;

# else
#  error "Shouldn't be here..."
# endif
}
#endif


/**
 * Gets the last instruction error value from the current VMCS.
 *
 * @returns VBox status code.
 */
DECLINLINE(uint32_t) VMXGetLastError(void)
{
#if ARCH_BITS == 64
    uint64_t uLastError = 0;
    int rc = VMXReadVmcs64(VMX_VMCS32_RO_VM_INSTR_ERROR, &uLastError);
    AssertRC(rc);
    return (uint32_t)uLastError;

#else /* 32-bit host: */
    uint32_t uLastError = 0;
    int rc = VMXReadVmcs32(VMX_VMCS32_RO_VM_INSTR_ERROR, &uLastError);
    AssertRC(rc);
    return uLastError;
#endif
}

/** @} */

#endif

