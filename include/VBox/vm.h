/** @file
 * VM - The Virtual Machine, data.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_vm_h
#define ___VBox_vm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/cpum.h>
#include <VBox/stam.h>
#include <VBox/vmapi.h>
#include <VBox/sup.h>


/** @defgroup grp_vm    The Virtual Machine
 * @{
 */

/** The name of the Guest Context VMM Core module. */
#define VMMGC_MAIN_MODULE_NAME          "VMMGC.gc"
/** The name of the Ring 0 Context VMM Core module. */
#define VMMR0_MAIN_MODULE_NAME          "VMMR0.r0"

/** VM Forced Action Flags.
 *
 * Use the VM_FF_SET() and VM_FF_CLEAR() macros to change the force
 * action mask of a VM.
 *
 * @{
 */
/** This action forces the VM to service check and pending interrups on the APIC. */
#define VM_FF_INTERRUPT_APIC            RT_BIT_32(0)
/** This action forces the VM to service check and pending interrups on the PIC. */
#define VM_FF_INTERRUPT_PIC             RT_BIT_32(1)
/** This action forces the VM to schedule and run pending timer (TM). */
#define VM_FF_TIMER                     RT_BIT_32(2)
/** PDM Queues are pending. */
#define VM_FF_PDM_QUEUES                RT_BIT_32(3)
/** PDM DMA transfers are pending. */
#define VM_FF_PDM_DMA                   RT_BIT_32(4)
/** PDM critical section unlocking is pending, process promptly upon return to R3. */
#define VM_FF_PDM_CRITSECT              RT_BIT_32(5)

/** This action forces the VM to call DBGF so DBGF can service debugger
 * requests in the emulation thread.
 * This action flag stays asserted till DBGF clears it.*/
#define VM_FF_DBGF                      RT_BIT_32(8)
/** This action forces the VM to service pending requests from other
 * thread or requests which must be executed in another context. */
#define VM_FF_REQUEST                   RT_BIT_32(9)
/** Terminate the VM immediately. */
#define VM_FF_TERMINATE                 RT_BIT_32(10)
/** Reset the VM. (postponed) */
#define VM_FF_RESET                     RT_BIT_32(11)

/** This action forces the VM to resync the page tables before going
 * back to execute guest code. (GLOBAL FLUSH) */
#define VM_FF_PGM_SYNC_CR3              RT_BIT_32(16)
/** Same as VM_FF_PGM_SYNC_CR3 except that global pages can be skipped.
 * (NON-GLOBAL FLUSH) */
#define VM_FF_PGM_SYNC_CR3_NON_GLOBAL   RT_BIT_32(17)
/** PGM needs to allocate handy pages. */
#define VM_FF_PGM_NEED_HANDY_PAGES      RT_BIT_32(18)
/** Check the interupt and trap gates */
#define VM_FF_TRPM_SYNC_IDT             RT_BIT_32(19)
/** Check Guest's TSS ring 0 stack */
#define VM_FF_SELM_SYNC_TSS             RT_BIT_32(20)
/** Check Guest's GDT table */
#define VM_FF_SELM_SYNC_GDT             RT_BIT_32(21)
/** Check Guest's LDT table */
#define VM_FF_SELM_SYNC_LDT             RT_BIT_32(22)
/** Inhibit interrupts pending. See EMGetInhibitInterruptsPC(). */
#define VM_FF_INHIBIT_INTERRUPTS        RT_BIT_32(23)

/** CSAM needs to scan the page that's being executed */
#define VM_FF_CSAM_SCAN_PAGE            RT_BIT_32(24)
/** CSAM needs to do some homework. */
#define VM_FF_CSAM_PENDING_ACTION       RT_BIT_32(25)

/** Force return to Ring-3. */
#define VM_FF_TO_R3                     RT_BIT_32(28)

/** Suspend the VM - debug only. */
#define VM_FF_DEBUG_SUSPEND             RT_BIT_32(31)

/** Externally forced actions. Used to quit the idle/wait loop. */
#define VM_FF_EXTERNAL_SUSPENDED_MASK   (VM_FF_TERMINATE | VM_FF_DBGF | VM_FF_REQUEST)
/** Externally forced actions. Used to quit the idle/wait loop. */
#define VM_FF_EXTERNAL_HALTED_MASK      (VM_FF_TERMINATE | VM_FF_DBGF | VM_FF_TIMER | VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC | VM_FF_REQUEST | VM_FF_PDM_QUEUES | VM_FF_PDM_DMA)
/** High priority pre-execution actions. */
#define VM_FF_HIGH_PRIORITY_PRE_MASK    (VM_FF_TERMINATE | VM_FF_DBGF | VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC | VM_FF_TIMER | VM_FF_DEBUG_SUSPEND \
                                        | VM_FF_PGM_SYNC_CR3 | VM_FF_PGM_SYNC_CR3_NON_GLOBAL | VM_FF_SELM_SYNC_TSS | VM_FF_TRPM_SYNC_IDT | VM_FF_SELM_SYNC_GDT | VM_FF_SELM_SYNC_LDT | VM_FF_PGM_NEED_HANDY_PAGES)
/** High priority pre raw-mode execution mask. */
#define VM_FF_HIGH_PRIORITY_PRE_RAW_MASK (VM_FF_PGM_SYNC_CR3 | VM_FF_PGM_SYNC_CR3_NON_GLOBAL | VM_FF_SELM_SYNC_TSS | VM_FF_TRPM_SYNC_IDT | VM_FF_SELM_SYNC_GDT | VM_FF_SELM_SYNC_LDT | VM_FF_PGM_NEED_HANDY_PAGES \
                                        | VM_FF_INHIBIT_INTERRUPTS)
/** High priority post-execution actions. */
#define VM_FF_HIGH_PRIORITY_POST_MASK   (VM_FF_PDM_CRITSECT | VM_FF_CSAM_PENDING_ACTION)
/** Normal priority post-execution actions. */
#define VM_FF_NORMAL_PRIORITY_POST_MASK (VM_FF_TERMINATE | VM_FF_DBGF | VM_FF_RESET | VM_FF_CSAM_SCAN_PAGE)
/** Normal priority actions. */
#define VM_FF_NORMAL_PRIORITY_MASK      (VM_FF_REQUEST | VM_FF_PDM_QUEUES | VM_FF_PDM_DMA)
/** Flags to check before resuming guest execution. */
#define VM_FF_RESUME_GUEST_MASK         (VM_FF_TO_R3)
/** All the forced flags. */
#define VM_FF_ALL_MASK                  (~0U)
/** All the forced flags. */
#define VM_FF_ALL_BUT_RAW_MASK          (~(VM_FF_HIGH_PRIORITY_PRE_RAW_MASK | VM_FF_CSAM_PENDING_ACTION | VM_FF_PDM_CRITSECT))

/** @} */

/** @def VM_FF_SET
 * Sets a force action flag.
 *
 * @param   pVM     VM Handle.
 * @param   fFlag   The flag to set.
 */
#if 1
# define VM_FF_SET(pVM, fFlag)           ASMAtomicOrU32(&(pVM)->fForcedActions, (fFlag))
#else
# define VM_FF_SET(pVM, fFlag) \
    do { ASMAtomicOrU32(&(pVM)->fForcedActions, (fFlag)); \
         RTLogPrintf("VM_FF_SET  : %08x %s - %s(%d) %s\n", (pVM)->fForcedActions, #fFlag, __FILE__, __LINE__, __FUNCTION__); \
    } while (0)
#endif

/** @def VM_FF_CLEAR
 * Clears a force action flag.
 *
 * @param   pVM     VM Handle.
 * @param   fFlag   The flag to clear.
 */
#if 1
# define VM_FF_CLEAR(pVM, fFlag)         ASMAtomicAndU32(&(pVM)->fForcedActions, ~(fFlag))
#else
# define VM_FF_CLEAR(pVM, fFlag) \
    do { ASMAtomicAndU32(&(pVM)->fForcedActions, ~(fFlag)); \
         RTLogPrintf("VM_FF_CLEAR: %08x %s - %s(%d) %s\n", (pVM)->fForcedActions, #fFlag, __FILE__, __LINE__, __FUNCTION__); \
    } while (0)
#endif

/** @def VM_FF_ISSET
 * Checks if a force action flag is set.
 *
 * @param   pVM     VM Handle.
 * @param   fFlag   The flag to check.
 */
#define VM_FF_ISSET(pVM, fFlag)         (((pVM)->fForcedActions & (fFlag)) == (fFlag))

/** @def VM_FF_ISPENDING
 * Checks if one or more force action in the specified set is pending.
 *
 * @param   pVM     VM Handle.
 * @param   fFlags  The flags to check for.
 */
#define VM_FF_ISPENDING(pVM, fFlags)    ((pVM)->fForcedActions & (fFlags))


/** @def VM_IS_EMT
 * Checks if the current thread is the emulation thread (EMT).
 *
 * @remark  The ring-0 variation will need attention if we expand the ring-0
 *          code to let threads other than EMT mess around with the VM.
 */
#ifdef IN_GC
# define VM_IS_EMT(pVM)                 true
#elif defined(IN_RING0)
# define VM_IS_EMT(pVM)                 true
#else
# define VM_IS_EMT(pVM)                 ((pVM)->NativeThreadEMT == RTThreadNativeSelf())
#endif

/** @def VM_ASSERT_EMT
 * Asserts that the current thread IS the emulation thread (EMT).
 */
#ifdef IN_GC
# define VM_ASSERT_EMT(pVM) Assert(VM_IS_EMT(pVM))
#elif defined(IN_RING0)
# define VM_ASSERT_EMT(pVM) Assert(VM_IS_EMT(pVM))
#else
# define VM_ASSERT_EMT(pVM) \
    AssertMsg(VM_IS_EMT(pVM), \
        ("Not emulation thread! Thread=%RTnthrd ThreadEMT=%RTnthrd\n", RTThreadNativeSelf(), pVM->NativeThreadEMT))
#endif

/** @def VM_ASSERT_EMT_RETURN
 * Asserts that the current thread IS the emulation thread (EMT) and returns if it isn't.
 */
#ifdef IN_GC
# define VM_ASSERT_EMT_RETURN(pVM, rc)  AssertReturn(VM_IS_EMT(pVM), (rc))
#elif defined(IN_RING0)
# define VM_ASSERT_EMT_RETURN(pVM, rc)  AssertReturn(VM_IS_EMT(pVM), (rc))
#else
# define VM_ASSERT_EMT_RETURN(pVM, rc) \
    AssertMsgReturn(VM_IS_EMT(pVM), \
        ("Not emulation thread! Thread=%RTnthrd ThreadEMT=%RTnthrd\n", RTThreadNativeSelf(), pVM->NativeThreadEMT), \
        (rc))
#endif

/**
 * Asserts that the current thread is NOT the emulation thread.
 */
#define VM_ASSERT_OTHER_THREAD(pVM) \
    AssertMsg(!VM_IS_EMT(pVM), ("Not other thread!!\n"))


/** @def VM_ASSERT_STATE_RETURN
 * Asserts a certain VM state.
 */
#define VM_ASSERT_STATE(pVM, _enmState) \
        AssertMsg((pVM)->enmVMState == (_enmState), \
                  ("state %s, expected %s\n", VMGetStateName(pVM->enmVMState), VMGetStateName(_enmState)))

/** @def VM_ASSERT_STATE_RETURN
 * Asserts a certain VM state and returns if it doesn't match.
 */
#define VM_ASSERT_STATE_RETURN(pVM, _enmState, rc) \
        AssertMsgReturn((pVM)->enmVMState == (_enmState), \
                        ("state %s, expected %s\n", VMGetStateName(pVM->enmVMState), VMGetStateName(_enmState)), \
                        (rc))




/** This is the VM structure.
 *
 * It contains (nearly?) all the VM data which have to be available in all
 * contexts. Even if it contains all the data the idea is to use APIs not
 * to modify all the members all around the place. Therefore we make use of
 * unions to hide everything which isn't local to the current source module.
 * This means we'll have to pay a little bit of attention when adding new
 * members to structures in the unions and make sure to keep the padding sizes
 * up to date.
 *
 * Run tstVMStructSize after update!
 */
typedef struct VM
{
    /** The state of the VM.
     * This field is read only to everyone except the VM and EM. */
    VMSTATE                     enmVMState;
    /** Forced action flags.
     * See the VM_FF_* \#defines. Updated atomically.
     */
    volatile uint32_t           fForcedActions;
    /** Pointer to the array of page descriptors for the VM structure allocation. */
    R3PTRTYPE(PSUPPAGE)         paVMPagesR3;
    /** Session handle. For use when calling SUPR0 APIs. */
    PSUPDRVSESSION              pSession;
    /** Pointer to the ring-3 VM structure. */
    PUVM                        pUVM;
    /** Ring-3 Host Context VM Pointer. */
    R3PTRTYPE(struct VM *)      pVMR3;
    /** Ring-0 Host Context VM Pointer. */
    R0PTRTYPE(struct VM *)      pVMR0;
    /** Guest Context VM Pointer. */
    RCPTRTYPE(struct VM *)      pVMGC;

    /** The GVM VM handle. Only the GVM should modify this field. */
    uint32_t                    hSelf;
    /** Reserved / padding. */
    uint32_t                    u32Reserved;

    /** @name Public VMM Switcher APIs
     * @{ */
    /**
     * Assembly switch entry point for returning to host context.
     * This function will clean up the stack frame.
     *
     * @param   eax         The return code, register.
     * @param   Ctx         The guest core context.
     * @remark  Assume interrupts disabled.
     */
    RTGCPTR32           pfnVMMGCGuestToHostAsmGuestCtx/*(int32_t eax, CPUMCTXCORE Ctx)*/;

    /**
     * Assembly switch entry point for returning to host context.
     *
     * This is an alternative entry point which we'll be using when the we have the
     * hypervisor context and need to save  that before going to the host.
     *
     * This is typically useful when abandoning the hypervisor because of a trap
     * and want the trap state to be saved.
     *
     * @param   eax         The return code, register.
     * @param   ecx         Pointer to the  hypervisor core context, register.
     * @remark  Assume interrupts disabled.
     */
    RTGCPTR32           pfnVMMGCGuestToHostAsmHyperCtx/*(int32_t eax, PCPUMCTXCORE ecx)*/;

    /**
     * Assembly switch entry point for returning to host context.
     *
     * This is an alternative to the two *Ctx APIs and implies that the context has already
     * been saved, or that it's just a brief return to HC and that the caller intends to resume
     * whatever it is doing upon 'return' from this call.
     *
     * @param   eax         The return code, register.
     * @remark  Assume interrupts disabled.
     */
    RTGCPTR32           pfnVMMGCGuestToHostAsm/*(int32_t eax)*/;
    /** @} */


    /** @name Various VM data owned by VM.
     * @{ */
    /** The thread handle of the emulation thread.
     * Use the VM_IS_EMT() macro to check if executing in EMT. */
    RTTHREAD            ThreadEMT;
    /** The native handle of ThreadEMT. Getting the native handle
     * is generally faster than getting the IPRT one (except on OS/2 :-). */
    RTNATIVETHREAD      NativeThreadEMT;
    /** @} */


    /** @name Various items that are frequently accessed.
     * @{ */
    /** Raw ring-3 indicator.  */
    bool                fRawR3Enabled;
    /** Raw ring-0 indicator. */
    bool                fRawR0Enabled;
    /** PATM enabled flag.
     * This is placed here for performance reasons. */
    bool                fPATMEnabled;
    /** CSAM enabled flag.
     * This is placed here for performance reasons. */
    bool                fCSAMEnabled;

    /** Hardware VM support is available and enabled.
     * This is placed here for performance reasons. */
    bool                fHWACCMEnabled;
    /** @} */


    /* padding to make gnuc put the StatQemuToGC where msc does. */
#if HC_ARCH_BITS == 32
    uint32_t            padding0;
#endif

    /** Profiling the total time from Qemu to GC. */
    STAMPROFILEADV      StatTotalQemuToGC;
    /** Profiling the total time from GC to Qemu. */
    STAMPROFILEADV      StatTotalGCToQemu;
    /** Profiling the total time spent in GC. */
    STAMPROFILEADV      StatTotalInGC;
    /** Profiling the total time spent not in Qemu. */
    STAMPROFILEADV      StatTotalInQemu;
    /** Profiling the VMMSwitcher code for going to GC. */
    STAMPROFILEADV      StatSwitcherToGC;
    /** Profiling the VMMSwitcher code for going to HC. */
    STAMPROFILEADV      StatSwitcherToHC;
    STAMPROFILEADV      StatSwitcherSaveRegs;
    STAMPROFILEADV      StatSwitcherSysEnter;
    STAMPROFILEADV      StatSwitcherDebug;
    STAMPROFILEADV      StatSwitcherCR0;
    STAMPROFILEADV      StatSwitcherCR4;
    STAMPROFILEADV      StatSwitcherJmpCR3;
    STAMPROFILEADV      StatSwitcherRstrRegs;
    STAMPROFILEADV      StatSwitcherLgdt;
    STAMPROFILEADV      StatSwitcherLidt;
    STAMPROFILEADV      StatSwitcherLldt;
    STAMPROFILEADV      StatSwitcherTSS;

    /* padding - the unions must be aligned on 32 bytes boundraries. */
    uint32_t            padding[HC_ARCH_BITS == 32 ? 4 : 6];

    /** CPUM part. */
    union
    {
#ifdef ___CPUMInternal_h
        struct CPUM s;
#endif
        char        padding[4384];      /* multiple of 32 */
    } cpum;

    /** VMM part. */
    union
    {
#ifdef ___VMMInternal_h
        struct VMM  s;
#endif
        char        padding[1024];       /* multiple of 32 */
    } vmm;

    /** PGM part. */
    union
    {
#ifdef ___PGMInternal_h
        struct PGM  s;
#endif
        char        padding[50*1024];   /* multiple of 32 */
    } pgm;

    /** HWACCM part. */
    union
    {
#ifdef ___HWACCMInternal_h
        struct HWACCM s;
#endif
        char        padding[1024];       /* multiple of 32 */
    } hwaccm;

    /** TRPM part. */
    union
    {
#ifdef ___TRPMInternal_h
        struct TRPM s;
#endif
        char        padding[5344];      /* multiple of 32 */
    } trpm;

    /** SELM part. */
    union
    {
#ifdef ___SELMInternal_h
        struct SELM s;
#endif
        char        padding[544];      /* multiple of 32 */
    } selm;

    /** MM part. */
    union
    {
#ifdef ___MMInternal_h
        struct MM   s;
#endif
        char        padding[128];       /* multiple of 32 */
    } mm;

    /** CFGM part. */
    union
    {
#ifdef ___CFGMInternal_h
        struct CFGM s;
#endif
        char        padding[32];        /* multiple of 32 */
    } cfgm;

    /** PDM part. */
    union
    {
#ifdef ___PDMInternal_h
        struct PDM s;
#endif
        char        padding[1056];      /* multiple of 32 */
    } pdm;

    /** IOM part. */
    union
    {
#ifdef ___IOMInternal_h
        struct IOM s;
#endif
        char        padding[4544];      /* multiple of 32 */
    } iom;

    /** PATM part. */
    union
    {
#ifdef ___PATMInternal_h
        struct PATM s;
#endif
        char        padding[768];       /* multiple of 32 */
    } patm;

    /** CSAM part. */
    union
    {
#ifdef ___CSAMInternal_h
        struct CSAM s;
#endif
        char        padding[3328];    /* multiple of 32 */
    } csam;

    /** EM part. */
    union
    {
#ifdef ___EMInternal_h
        struct EM   s;
#endif
        char        padding[1344];      /* multiple of 32 */
    } em;

    /** TM part. */
    union
    {
#ifdef ___TMInternal_h
        struct TM   s;
#endif
        char        padding[1344];      /* multiple of 32 */
    } tm;

    /** DBGF part. */
    union
    {
#ifdef ___DBGFInternal_h
        struct DBGF s;
#endif
        char        padding[2368];      /* multiple of 32 */
    } dbgf;

    /** SSM part. */
    union
    {
#ifdef ___SSMInternal_h
        struct SSM  s;
#endif
        char        padding[32];        /* multiple of 32 */
    } ssm;

    /** VM part. */
    union
    {
#ifdef ___VMInternal_h
        struct VMINT    s;
#endif
        char        padding[768];       /* multiple of 32 */
    } vm;

    /** REM part. */
    union
    {
#ifdef ___REMInternal_h
        struct REM  s;
#endif
#if GC_ARCH_BITS == 32
        char        padding[HC_ARCH_BITS == 32 ? 0x6f00 : 0xbf00];    /* multiple of 32 */
#else
        char        padding[HC_ARCH_BITS == 32 ? 0x8f00 : 0xdf00];    /* multiple of 32 */
#endif
    } rem;
} VM;

/** Pointer to a VM. */
#ifndef ___VBox_types_h
typedef struct VM *PVM;
#endif


#ifdef IN_GC
__BEGIN_DECLS

/** The VM structure.
 * This is imported from the VMMGCBuiltin module, i.e. it's a one
 * of those magic globals which we should avoid using.
 */
extern DECLIMPORT(VM)   g_VM;

__END_DECLS
#endif

/** @} */

#endif

