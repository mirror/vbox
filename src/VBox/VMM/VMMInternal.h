/* $Id$ */
/** @file
 * VMM - Internal header file.
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

#ifndef ___VMMInternal_h
#define ___VMMInternal_h

#include <VBox/cdefs.h>
#include <VBox/stam.h>
#include <VBox/log.h>
#include <iprt/critsect.h>


#if !defined(IN_VMM_R3) && !defined(IN_VMM_R0) && !defined(IN_VMM_RC)
# error "Not in VMM! This is an internal header!"
#endif


/** @defgroup grp_vmm_int   Internals
 * @ingroup grp_vmm
 * @internal
 * @{
 */

/** @def VBOX_WITH_RC_RELEASE_LOGGING
 * Enables RC release logging. */
#define VBOX_WITH_RC_RELEASE_LOGGING

/** @def VBOX_WITH_R0_LOGGING
 * Enables Ring-0 logging (non-release).
 *
 * Ring-0 logging isn't 100% safe yet (thread id reuse / process exit cleanup),
 * so you have to sign up here by adding your defined(DEBUG_<userid>) to the
 * #if, or by adding VBOX_WITH_R0_LOGGING to your LocalConfig.kmk.
 *
 * You might also wish to enable the AssertMsg1/2 overrides in VMMR0.cpp when
 * enabling this.
 */
#if defined(DEBUG_sandervl) || defined(DEBUG_frank) || defined(DOXYGEN_RUNNING)
# define VBOX_WITH_R0_LOGGING
#endif


/**
 * Converts a VMM pointer into a VM pointer.
 * @returns Pointer to the VM structure the VMM is part of.
 * @param   pVMM   Pointer to VMM instance data.
 */
#define VMM2VM(pVMM)  ( (PVM)((char*)pVMM - pVMM->offVM) )


/**
 * Switcher function, HC to RC.
 *
 * @param   pVM         The VM handle.
 * @returns Return code indicating the action to take.
 */
typedef DECLASMTYPE(int) FNVMMSWITCHERHC(PVM pVM);
/** Pointer to switcher function. */
typedef FNVMMSWITCHERHC *PFNVMMSWITCHERHC;

/**
 * Switcher function, RC to HC.
 *
 * @param   rc          VBox status code.
 */
typedef DECLASMTYPE(void) FNVMMSWITCHERRC(int rc);
/** Pointer to switcher function. */
typedef FNVMMSWITCHERRC *PFNVMMSWITCHERRC;


/**
 * The ring-0 logger instance wrapper.
 *
 * We need to be able to find the VM handle from the logger instance, so we wrap
 * it in this structure.
 */
typedef struct VMMR0LOGGER
{
    /** Pointer to the VM handle. */
    R0PTRTYPE(PVM)              pVM;
    /** Size of the allocated logger instance (Logger). */
    uint32_t                    cbLogger;
    /** Flag indicating whether we've create the logger Ring-0 instance yet. */
    bool                        fCreated;
    /** Flag indicating whether we've disabled flushing (world switch) or not. */
    bool                        fFlushingDisabled;
#if HC_ARCH_BITS == 32
    uint32_t                    u32Alignment;
#endif
    /** The ring-0 logger instance. This extends beyond the size.  */
    RTLOGGER                    Logger;
} VMMR0LOGGER;
/** Pointer to a ring-0 logger instance wrapper. */
typedef VMMR0LOGGER *PVMMR0LOGGER;


/**
 * Jump buffer for the setjmp/longjmp like constructs used to
 * quickly 'call' back into Ring-3.
 */
typedef struct VMMR0JMPBUF
{
    /** Traditional jmp_buf stuff
     * @{ */
#if HC_ARCH_BITS == 32
    uint32_t                    ebx;
    uint32_t                    esi;
    uint32_t                    edi;
    uint32_t                    ebp;
    uint32_t                    esp;
    uint32_t                    eip;
    uint32_t                    u32Padding;
#endif
#if HC_ARCH_BITS == 64
    uint64_t                    rbx;
# ifdef RT_OS_WINDOWS
    uint64_t                    rsi;
    uint64_t                    rdi;
# endif
    uint64_t                    rbp;
    uint64_t                    r12;
    uint64_t                    r13;
    uint64_t                    r14;
    uint64_t                    r15;
    uint64_t                    rsp;
    uint64_t                    rip;
#endif
    /** @} */

    /** Flag that indicates that we've done a ring-3 call. */
    bool                        fInRing3Call;
    /** The number of bytes we've saved. */
    uint32_t                    cbSavedStack;
    /** Pointer to the buffer used to save the stack.
     * This is assumed to be 8KB. */
    RTR0PTR                     pvSavedStack;
    /** Esp we we match against esp on resume to make sure the stack wasn't relocated. */
    RTHCUINTREG                 SpCheck;
    /** The esp we should resume execution with after the restore. */
    RTHCUINTREG                 SpResume;
} VMMR0JMPBUF;
/** Pointer to a ring-0 jump buffer. */
typedef VMMR0JMPBUF *PVMMR0JMPBUF;


/**
 * VMM Data (part of VM)
 */
typedef struct VMM
{
    /** Offset to the VM structure.
     * See VMM2VM(). */
    RTINT                       offVM;

    /** @name World Switcher and Related
     * @{
     */
    /** Size of the core code. */
    RTUINT                      cbCoreCode;
    /** Physical address of core code. */
    RTHCPHYS                    HCPhysCoreCode;
    /** Pointer to core code ring-3 mapping - contiguous memory.
     * At present this only means the context switcher code. */
    RTR3PTR                     pvCoreCodeR3;
    /** Pointer to core code ring-0 mapping - contiguous memory.
     * At present this only means the context switcher code. */
    RTR0PTR                     pvCoreCodeR0;
    /** Pointer to core code guest context mapping. */
    RTRCPTR                     pvCoreCodeRC;
    RTRCPTR                     pRCPadding0; /**< Alignment padding */
#ifdef VBOX_WITH_NMI
    /** The guest context address of the APIC (host) mapping. */
    RTRCPTR                     GCPtrApicBase;
    RTRCPTR                     pRCPadding1; /**< Alignment padding */
#endif
    /** The current switcher.
     * This will be set before the VMM is fully initialized. */
    VMMSWITCHER                 enmSwitcher;
    /** Flag to disable the switcher permanently (VMX) (boolean) */
    bool                        fSwitcherDisabled;
    /** Array of offsets to the different switchers within the core code. */
    RTUINT                      aoffSwitchers[VMMSWITCHER_MAX];
    uint32_t                    u32Padding0; /**< Alignment padding. */

    /** The last RC/R0 return code. */
    RTINT                       iLastGZRc;
    /** Resume Guest Execution. See CPUMGCResumeGuest(). */
    RTRCPTR                     pfnCPUMRCResumeGuest;
    /** Resume Guest Execution in V86 mode. See CPUMGCResumeGuestV86(). */
    RTRCPTR                     pfnCPUMRCResumeGuestV86;
    /** Call Trampoline. See vmmGCCallTrampoline(). */
    RTRCPTR                     pfnCallTrampolineRC;
    /** Guest to host switcher entry point. */
    RCPTRTYPE(PFNVMMSWITCHERRC) pfnGuestToHostRC;
    /** Host to guest switcher entry point. */
    R0PTRTYPE(PFNVMMSWITCHERHC) pfnHostToGuestR0;
    /** @}  */

    /** @name Logging
     * @{
     */
    /** Size of the allocated logger instance (pRCLoggerRC/pRCLoggerR3). */
    uint32_t                    cbRCLogger;
    /** Pointer to the RC logger instance - RC Ptr.
     * This is NULL if logging is disabled. */
    RCPTRTYPE(PRTLOGGERRC)      pRCLoggerRC;
    /** Pointer to the GC logger instance - R3 Ptr.
     * This is NULL if logging is disabled. */
    R3PTRTYPE(PRTLOGGERRC)      pRCLoggerR3;
    /** Pointer to the R0 logger instance - R3 Ptr.
     * This is NULL if logging is disabled. */
    R3PTRTYPE(PVMMR0LOGGER)     pR0LoggerR3;
    /** Pointer to the R0 logger instance - R0 Ptr.
     * This is NULL if logging is disabled. */
    R0PTRTYPE(PVMMR0LOGGER)     pR0LoggerR0;
    /** Pointer to the GC release logger instance - R3 Ptr. */
    R3PTRTYPE(PRTLOGGERRC)      pRCRelLoggerR3;
    /** Pointer to the GC release logger instance - RC Ptr. */
    RCPTRTYPE(PRTLOGGERRC)      pRCRelLoggerRC;
    /** Size of the allocated release logger instance (pRCRelLoggerRC/pRCRelLoggerR3).
     * This may differ from cbRCLogger. */
    uint32_t                    cbRCRelLogger;
    /** @} */

    /** The EMT yield timer. */
    PTMTIMERR3                  pYieldTimer;
    /** The period to the next timeout when suspended or stopped.
     * This is 0 when running. */
    uint32_t                    cYieldResumeMillies;
    /** The EMT yield timer interval (milliseconds). */
    uint32_t                    cYieldEveryMillies;
#if HC_ARCH_BITS == 32
    RTR3PTR                     pR3Padding1; /**< Alignment padding. */
#endif
    /** The timestamp of the previous yield. (nano) */
    uint64_t                    u64LastYield;

    /** Buffer for storing the standard assertion message for a ring-0 assertion.
     * Used for saving the assertion message text for the release log and guru
     * meditation dump. */
    char                        szRing0AssertMsg1[512];
    /** Buffer for storing the custom message for a ring-0 assertion. */
    char                        szRing0AssertMsg2[256];

    /** Number of VMMR0_DO_RUN_GC calls. */
    STAMCOUNTER                 StatRunRC;

    /** Statistics for each of the RC/R0 return codes.
     * @{ */
    STAMCOUNTER                 StatRZRetNormal;
    STAMCOUNTER                 StatRZRetInterrupt;
    STAMCOUNTER                 StatRZRetInterruptHyper;
    STAMCOUNTER                 StatRZRetGuestTrap;
    STAMCOUNTER                 StatRZRetRingSwitch;
    STAMCOUNTER                 StatRZRetRingSwitchInt;
    STAMCOUNTER                 StatRZRetExceptionPrivilege;
    STAMCOUNTER                 StatRZRetStaleSelector;
    STAMCOUNTER                 StatRZRetIRETTrap;
    STAMCOUNTER                 StatRZRetEmulate;
    STAMCOUNTER                 StatRZRetIOBlockEmulate;
    STAMCOUNTER                 StatRZRetPatchEmulate;
    STAMCOUNTER                 StatRZRetIORead;
    STAMCOUNTER                 StatRZRetIOWrite;
    STAMCOUNTER                 StatRZRetMMIORead;
    STAMCOUNTER                 StatRZRetMMIOWrite;
    STAMCOUNTER                 StatRZRetMMIOPatchRead;
    STAMCOUNTER                 StatRZRetMMIOPatchWrite;
    STAMCOUNTER                 StatRZRetMMIOReadWrite;
    STAMCOUNTER                 StatRZRetLDTFault;
    STAMCOUNTER                 StatRZRetGDTFault;
    STAMCOUNTER                 StatRZRetIDTFault;
    STAMCOUNTER                 StatRZRetTSSFault;
    STAMCOUNTER                 StatRZRetPDFault;
    STAMCOUNTER                 StatRZRetCSAMTask;
    STAMCOUNTER                 StatRZRetSyncCR3;
    STAMCOUNTER                 StatRZRetMisc;
    STAMCOUNTER                 StatRZRetPatchInt3;
    STAMCOUNTER                 StatRZRetPatchPF;
    STAMCOUNTER                 StatRZRetPatchGP;
    STAMCOUNTER                 StatRZRetPatchIretIRQ;
    STAMCOUNTER                 StatRZRetPageOverflow;
    STAMCOUNTER                 StatRZRetRescheduleREM;
    STAMCOUNTER                 StatRZRetToR3;
    STAMCOUNTER                 StatRZRetTimerPending;
    STAMCOUNTER                 StatRZRetInterruptPending;
    STAMCOUNTER                 StatRZRetCallHost;
    STAMCOUNTER                 StatRZRetPATMDuplicateFn;
    STAMCOUNTER                 StatRZRetPGMChangeMode;
    STAMCOUNTER                 StatRZRetEmulHlt;
    STAMCOUNTER                 StatRZRetPendingRequest;
    STAMCOUNTER                 StatRZCallPDMLock;
    STAMCOUNTER                 StatRZCallLogFlush;
    STAMCOUNTER                 StatRZCallPDMQueueFlush;
    STAMCOUNTER                 StatRZCallPGMPoolGrow;
    STAMCOUNTER                 StatRZCallPGMMapChunk;
    STAMCOUNTER                 StatRZCallPGMAllocHandy;
    STAMCOUNTER                 StatRZCallRemReplay;
    STAMCOUNTER                 StatRZCallVMSetError;
    STAMCOUNTER                 StatRZCallVMSetRuntimeError;
    STAMCOUNTER                 StatRZCallPGMLock;
    /** @} */
} VMM;
/** Pointer to VMM. */
typedef VMM *PVMM;


/**
 * VMMCPU Data (part of VMCPU)
 */
typedef struct VMMCPU
{
    /** Offset to the VMCPU structure.
     * See VMM2VMCPU(). */
    RTINT                       offVMCPU;

    /** VMM stack, pointer to the top of the stack in R3.
     * Stack is allocated from the hypervisor heap and is page aligned
     * and always writable in RC. */
    R3PTRTYPE(uint8_t *)        pbEMTStackR3;
    /** Pointer to the bottom of the stack - needed for doing relocations. */
    RCPTRTYPE(uint8_t *)        pbEMTStackRC;
    /** Pointer to the bottom of the stack - needed for doing relocations. */
    RCPTRTYPE(uint8_t *)        pbEMTStackBottomRC;

    /** @name CallHost
     * @{ */
    /** The pending operation. */
    VMMCALLHOST                 enmCallHostOperation;
    /** The result of the last operation. */
    int32_t                     rcCallHost;
    /** The argument to the operation. */
    uint64_t                    u64CallHostArg;
    /** The Ring-0 jmp buffer. */
    VMMR0JMPBUF                 CallHostR0JmpBuf;
    /** @} */

} VMMCPU;
/** Pointer to VMMCPU. */
typedef VMMCPU *PVMMCPU;


/**
 * The VMMGCEntry() codes.
 */
typedef enum VMMGCOPERATION
{
    /** Do GC module init. */
    VMMGC_DO_VMMGC_INIT = 1,

    /** The first Trap testcase. */
    VMMGC_DO_TESTCASE_TRAP_FIRST = 0x0dead000,
    /** Trap 0 testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_0 = VMMGC_DO_TESTCASE_TRAP_FIRST,
    /** Trap 1 testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_1,
    /** Trap 2 testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_2,
    /** Trap 3 testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_3,
    /** Trap 4 testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_4,
    /** Trap 5 testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_5,
    /** Trap 6 testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_6,
    /** Trap 7 testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_7,
    /** Trap 8 testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_8,
    /** Trap 9 testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_9,
    /** Trap 0a testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_0A,
    /** Trap 0b testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_0B,
    /** Trap 0c testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_0C,
    /** Trap 0d testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_0D,
    /** Trap 0e testcases, uArg selects the variation. */
    VMMGC_DO_TESTCASE_TRAP_0E,
    /** The last trap testcase (exclusive). */
    VMMGC_DO_TESTCASE_TRAP_LAST,
    /** Testcase for checking interrupt forwarding. */
    VMMGC_DO_TESTCASE_HYPER_INTERRUPT,
    /** Switching testing and profiling stub. */
    VMMGC_DO_TESTCASE_NOP,
    /** Testcase for checking interrupt masking.. */
    VMMGC_DO_TESTCASE_INTERRUPT_MASKING,
    /** Switching testing and profiling stub. */
    VMMGC_DO_TESTCASE_HWACCM_NOP,

    /** The usual 32-bit hack. */
    VMMGC_DO_32_BIT_HACK = 0x7fffffff
} VMMGCOPERATION;


__BEGIN_DECLS

#ifdef IN_RING3
int  vmmR3SwitcherInit(PVM pVM);
void vmmR3SwitcherRelocate(PVM pVM, RTGCINTPTR offDelta);
#endif /* IN_RING3 */

#ifdef IN_RING0
/**
 * World switcher assembly routine.
 * It will call VMMGCEntry().
 *
 * @returns return code from VMMGCEntry().
 * @param   pVM     The VM in question.
 * @param   uArg    See VMMGCEntry().
 * @internal
 */
DECLASM(int)    vmmR0WorldSwitch(PVM pVM, unsigned uArg);

/**
 * Callback function for vmmR0CallHostSetJmp.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
typedef DECLCALLBACK(int) FNVMMR0SETJMP(PVM pVM, PVMCPU pVCpu);
/** Pointer to FNVMMR0SETJMP(). */
typedef FNVMMR0SETJMP *PFNVMMR0SETJMP;

/**
 * The setjmp variant used for calling Ring-3.
 *
 * This differs from the normal setjmp in that it will resume VMMR0CallHost if we're
 * in the middle of a ring-3 call. Another differences is the function pointer and
 * argument. This has to do with resuming code and the stack frame of the caller.
 *
 * @returns VINF_SUCCESS on success or whatever is passed to vmmR0CallHostLongJmp.
 * @param   pJmpBuf     The jmp_buf to set.
 * @param   pfn         The function to be called when not resuming..
 * @param   pVM         The argument of that function.
 */
DECLASM(int)    vmmR0CallHostSetJmp(PVMMR0JMPBUF pJmpBuf, PFNVMMR0SETJMP pfn, PVM pVM, PVMCPU pVCpu);

/**
 * Callback function for vmmR0CallHostSetJmpEx.
 *
 * @returns VBox status code.
 * @param   pvUser      The user argument.
 */
typedef DECLCALLBACK(int) FNVMMR0SETJMPEX(void *pvUser);
/** Pointer to FNVMMR0SETJMP(). */
typedef FNVMMR0SETJMPEX *PFNVMMR0SETJMPEX;

/**
 * Same as vmmR0CallHostSetJmp except for the function signature.
 *
 * @returns VINF_SUCCESS on success or whatever is passed to vmmR0CallHostLongJmp.
 * @param   pJmpBuf     The jmp_buf to set.
 * @param   pfn         The function to be called when not resuming..
 * @param   pvUser      The argument of that function.
 */
DECLASM(int)    vmmR0CallHostSetJmpEx(PVMMR0JMPBUF pJmpBuf, PFNVMMR0SETJMPEX pfn, void *pvUser);


/**
 * Worker for VMMR0CallHost.
 * This will save the stack and registers.
 *
 * @returns rc.
 * @param   pJmpBuf         Pointer to the jump buffer.
 * @param   rc              The return code.
 */
DECLASM(int)    vmmR0CallHostLongJmp(PVMMR0JMPBUF pJmpBuf, int rc);

/**
 * Internal R0 logger worker: Logger wrapper.
 */
VMMR0DECL(void) vmmR0LoggerWrapper(const char *pszFormat, ...);

/**
 * Internal R0 logger worker: Flush logger.
 *
 * @param   pLogger     The logger instance to flush.
 * @remark  This function must be exported!
 */
VMMR0DECL(void) vmmR0LoggerFlush(PRTLOGGER pLogger);

#endif /* IN_RING0 */
#ifdef IN_RC

/**
 * Internal GC logger worker: Logger wrapper.
 */
VMMRCDECL(void) vmmGCLoggerWrapper(const char *pszFormat, ...);

/**
 * Internal GC release logger worker: Logger wrapper.
 */
VMMRCDECL(void) vmmGCRelLoggerWrapper(const char *pszFormat, ...);

/**
 * Internal GC logger worker: Flush logger.
 *
 * @returns VINF_SUCCESS.
 * @param   pLogger     The logger instance to flush.
 * @remark  This function must be exported!
 */
VMMRCDECL(int)  vmmGCLoggerFlush(PRTLOGGERRC pLogger);

/** @name Trap testcases and related labels.
 * @{ */
DECLASM(void)   vmmGCEnableWP(void);
DECLASM(void)   vmmGCDisableWP(void);
DECLASM(int)    vmmGCTestTrap3(void);
DECLASM(int)    vmmGCTestTrap8(void);
DECLASM(int)    vmmGCTestTrap0d(void);
DECLASM(int)    vmmGCTestTrap0e(void);
DECLASM(int)    vmmGCTestTrap0e_FaultEIP(void); /**< a label */
DECLASM(int)    vmmGCTestTrap0e_ResumeEIP(void); /**< a label */
/** @} */

#endif /* IN_RC */

__END_DECLS

/** @} */

#endif
