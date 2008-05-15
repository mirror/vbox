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


#if !defined(IN_VMM_R3) && !defined(IN_VMM_R0) && !defined(IN_VMM_GC)
# error "Not in VMM! This is an internal header!"
#endif


/** @defgroup grp_vmm_int   Internals
 * @ingroup grp_vmm
 * @internal
 * @{
 */

/** @def VBOX_WITH_GC_AND_R0_RELEASE_LOG
 * Enabled GC and R0 release logging (the latter is not implemented yet). */
#define VBOX_WITH_GC_AND_R0_RELEASE_LOG


/**
 * Converts a VMM pointer into a VM pointer.
 * @returns Pointer to the VM structure the VMM is part of.
 * @param   pVMM   Pointer to VMM instance data.
 */
#define VMM2VM(pVMM)  ( (PVM)((char*)pVMM - pVMM->offVM) )


/**
 * Switcher function, HC to GC.
 *
 * @param   pVM         The VM handle.
 * @returns Return code indicating the action to take.
 */
typedef DECLASMTYPE(int) FNVMMSWITCHERHC(PVM pVM);
/** Pointer to switcher function. */
typedef FNVMMSWITCHERHC *PFNVMMSWITCHERHC;

/**
 * Switcher function, GC to HC.
 *
 * @param   rc          VBox status code.
 */
typedef DECLASMTYPE(void) FNVMMSWITCHERGC(int rc);
/** Pointer to switcher function. */
typedef FNVMMSWITCHERGC *PFNVMMSWITCHERGC;


/**
 * The ring-0 logger instance.
 * We need to be able to find the VM handle from the logger instance.
 */
typedef struct VMMR0LOGGER
{
    /** Pointer to the VM handle. */
    R0PTRTYPE(PVM)              pVM;
    /** Size of the allocated logger instance (Logger). */
    uint32_t                    cbLogger;
    /** Flag indicating whether we've create the logger Ring-0 instance yet. */
    bool                        fCreated;
#if HC_ARCH_BITS == 32
    uint32_t                    u32Alignment;
#endif
    /** The ring-0 logger instance. This extends beyon the size.*/
    RTLOGGER                    Logger;
} VMMR0LOGGER, *PVMMR0LOGGER;


/**
 * Jump buffer for the setjmp/longjmp like constructs used to
 * quickly 'call' back into Ring-3.
 */
typedef struct VMMR0JMPBUF
{
    /** Tranditional jmp_buf stuff
     * @{ */
#if HC_ARCH_BITS == 32
    uint32_t    ebx;
    uint32_t    esi;
    uint32_t    edi;
    uint32_t    ebp;
    uint32_t    esp;
    uint32_t    eip;
    uint32_t    u32Padding;
#endif
#if HC_ARCH_BITS == 64
    uint64_t    rbx;
# ifdef RT_OS_WINDOWS
    uint64_t    rsi;
    uint64_t    rdi;
# endif
    uint64_t    rbp;
    uint64_t    r12;
    uint64_t    r13;
    uint64_t    r14;
    uint64_t    r15;
    uint64_t    rsp;
    uint64_t    rip;
#endif
    /** @} */

    /** Flag that indicates that we've done a ring-3 call. */
    bool        fInRing3Call;
    /** The number of bytes we've saved. */
    uint32_t    cbSavedStack;
    /** Pointer to the buffer used to save the stack.
     * This is assumed to be 8KB. */
    RTR0PTR     pvSavedStack;
    /** Esp we we match against esp on resume to make sure the stack wasn't relocated. */
    RTHCUINTREG SpCheck;
    /** The esp we should resume execution with after the restore. */
    RTHCUINTREG SpResume;
} VMMR0JMPBUF, *PVMMR0JMPBUF;


/**
 * VMM Data (part of VMM)
 */
typedef struct VMM
{
    /** Offset to the VM structure.
     * See VMM2VM(). */
    RTINT                       offVM;

    /** Size of the core code. */
    RTUINT                      cbCoreCode;
    /** Physical address of core code. */
    RTHCPHYS                    HCPhysCoreCode;
/** @todo pvHCCoreCodeR3 -> pvCoreCodeR3, pvHCCoreCodeR0 -> pvCoreCodeR0 */
    /** Pointer to core code ring-3 mapping - contiguous memory.
     * At present this only means the context switcher code. */
    RTR3PTR                     pvHCCoreCodeR3;
    /** Pointer to core code ring-0 mapping - contiguous memory.
     * At present this only means the context switcher code. */
    RTR0PTR                     pvHCCoreCodeR0;
    /** Pointer to core code guest context mapping. */
    RTGCPTR                     pvGCCoreCode;
#ifdef VBOX_WITH_NMI
    /** The guest context address of the APIC (host) mapping. */
    RTGCPTR                     GCPtrApicBase;
    RTGCPTR                     pGCPadding0; /**< Alignment padding */
#endif
    /** The current switcher.
     * This will be set before the VMM is fully initialized. */
    VMMSWITCHER                 enmSwitcher;
    /** Array of offsets to the different switchers within the core code. */
    RTUINT                      aoffSwitchers[VMMSWITCHER_MAX];
    /** Flag to disable the switcher permanently (VMX) (boolean) */
    bool                        fSwitcherDisabled;

    /** Host to guest switcher entry point. */
    R0PTRTYPE(PFNVMMSWITCHERHC) pfnR0HostToGuest;
    /** Guest to host switcher entry point. */
    GCPTRTYPE(PFNVMMSWITCHERGC) pfnGCGuestToHost;
    /** Call Trampoline. See vmmGCCallTrampoline(). */
    RTGCPTR                     pfnGCCallTrampoline;

    /** Resume Guest Execution. See CPUMGCResumeGuest(). */
    RTGCPTR                     pfnCPUMGCResumeGuest;
    /** Resume Guest Execution in V86 mode. See CPUMGCResumeGuestV86(). */
    RTGCPTR                     pfnCPUMGCResumeGuestV86;
    /** The last GC return code. */
    RTINT                       iLastGCRc;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32
    uint32_t                    u32Padding0; /**< Alignment padding. */
#endif

    /** VMM stack, pointer to the top of the stack in HC.
     * Stack is allocated from the hypervisor heap and is page aligned
     * and always writable in GC. */
    R3PTRTYPE(uint8_t *)        pbHCStack;
    /** Pointer to the bottom of the stack - needed for doing relocations. */
    GCPTRTYPE(uint8_t *)        pbGCStack;
    /** Pointer to the bottom of the stack - needed for doing relocations. */
    GCPTRTYPE(uint8_t *)        pbGCStackBottom;

    /** Pointer to the GC logger instance - GC Ptr.
     * This is NULL if logging is disabled. */
    GCPTRTYPE(PRTLOGGERGC)      pLoggerGC;
    /** Size of the allocated logger instance (pLoggerGC/pLoggerHC). */
    RTUINT                      cbLoggerGC;
    /** Pointer to the GC logger instance - HC Ptr.
     * This is NULL if logging is disabled. */
    R3PTRTYPE(PRTLOGGERGC)      pLoggerHC;

    /** Pointer to the R0 logger instance.
     * This is NULL if logging is disabled. */
    R3R0PTRTYPE(PVMMR0LOGGER)   pR0Logger;

#ifdef VBOX_WITH_GC_AND_R0_RELEASE_LOG
    /** Pointer to the GC release logger instance - GC Ptr. */
    GCPTRTYPE(PRTLOGGERGC)      pRelLoggerGC;
    /** Size of the allocated release logger instance (pRelLoggerGC/pRelLoggerHC).
     * This may differ from cbLoggerGC. */
    RTUINT                      cbRelLoggerGC;
    /** Pointer to the GC release logger instance - HC Ptr. */
    R3PTRTYPE(PRTLOGGERGC)      pRelLoggerHC;
#endif /* VBOX_WITH_GC_AND_R0_RELEASE_LOG */

    /** Global VM critical section. */
    RTCRITSECT                  CritSectVMLock;

    /** The EMT yield timer. */
    PTMTIMERR3                  pYieldTimer;
    /** The period to the next timeout when suspended or stopped.
     * This is 0 when running. */
    uint32_t                    cYieldResumeMillies;
    /** The EMT yield timer interval (milliseconds). */
    uint32_t                    cYieldEveryMillies;
#if HC_ARCH_BITS == 32
    uint32_t                    u32Padding0; /**< Alignment padding. */
#endif
    /** The timestamp of the previous yield. (nano) */
    uint64_t                    u64LastYield;

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

    /** Number of VMMR0_DO_RUN_GC calls. */
    STAMCOUNTER                 StatRunGC;
    /** Statistics for each of the GC return codes.
     * @{ */
    STAMCOUNTER                 StatGCRetNormal;
    STAMCOUNTER                 StatGCRetInterrupt;
    STAMCOUNTER                 StatGCRetInterruptHyper;
    STAMCOUNTER                 StatGCRetGuestTrap;
    STAMCOUNTER                 StatGCRetRingSwitch;
    STAMCOUNTER                 StatGCRetRingSwitchInt;
    STAMCOUNTER                 StatGCRetExceptionPrivilege;
    STAMCOUNTER                 StatGCRetStaleSelector;
    STAMCOUNTER                 StatGCRetIRETTrap;
    STAMCOUNTER                 StatGCRetEmulate;
    STAMCOUNTER                 StatGCRetPatchEmulate;
    STAMCOUNTER                 StatGCRetIORead;
    STAMCOUNTER                 StatGCRetIOWrite;
    STAMCOUNTER                 StatGCRetMMIORead;
    STAMCOUNTER                 StatGCRetMMIOWrite;
    STAMCOUNTER                 StatGCRetMMIOPatchRead;
    STAMCOUNTER                 StatGCRetMMIOPatchWrite;
    STAMCOUNTER                 StatGCRetMMIOReadWrite;
    STAMCOUNTER                 StatGCRetLDTFault;
    STAMCOUNTER                 StatGCRetGDTFault;
    STAMCOUNTER                 StatGCRetIDTFault;
    STAMCOUNTER                 StatGCRetTSSFault;
    STAMCOUNTER                 StatGCRetPDFault;
    STAMCOUNTER                 StatGCRetCSAMTask;
    STAMCOUNTER                 StatGCRetSyncCR3;
    STAMCOUNTER                 StatGCRetMisc;
    STAMCOUNTER                 StatGCRetPatchInt3;
    STAMCOUNTER                 StatGCRetPatchPF;
    STAMCOUNTER                 StatGCRetPatchGP;
    STAMCOUNTER                 StatGCRetPatchIretIRQ;
    STAMCOUNTER                 StatGCRetPageOverflow;
    STAMCOUNTER                 StatGCRetRescheduleREM;
    STAMCOUNTER                 StatGCRetToR3;
    STAMCOUNTER                 StatGCRetTimerPending;
    STAMCOUNTER                 StatGCRetInterruptPending;
    STAMCOUNTER                 StatGCRetCallHost;
    STAMCOUNTER                 StatGCRetPATMDuplicateFn;
    STAMCOUNTER                 StatGCRetPGMChangeMode;
    STAMCOUNTER                 StatGCRetEmulHlt;
    STAMCOUNTER                 StatGCRetPendingRequest;
    STAMCOUNTER                 StatGCRetPGMGrowRAM;
    STAMCOUNTER                 StatGCRetPDMLock;
    STAMCOUNTER                 StatGCRetHyperAssertion;
    STAMCOUNTER                 StatGCRetLogFlush;
    STAMCOUNTER                 StatGCRetPDMQueueFlush;
    STAMCOUNTER                 StatGCRetPGMPoolGrow;
    STAMCOUNTER                 StatGCRetRemReplay;
    STAMCOUNTER                 StatGCRetVMSetError;
    STAMCOUNTER                 StatGCRetVMSetRuntimeError;
    STAMCOUNTER                 StatGCRetPGMLock;

    /** @} */


} VMM, *PVMM;


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
typedef DECLCALLBACK(int) FNVMMR0SETJMP(PVM pVM);
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
DECLASM(int)    vmmR0CallHostSetJmp(PVMMR0JMPBUF pJmpBuf, PFNVMMR0SETJMP pfn, PVM pVM);

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


#ifdef IN_GC
/**
 * Internal GC logger worker: Logger wrapper.
 */
VMMGCDECL(void) vmmGCLoggerWrapper(const char *pszFormat, ...);

/**
 * Internal GC release logger worker: Logger wrapper.
 */
VMMGCDECL(void) vmmGCRelLoggerWrapper(const char *pszFormat, ...);

/**
 * Internal GC logger worker: Flush logger.
 *
 * @returns VINF_SUCCESS.
 * @param   pLogger     The logger instance to flush.
 * @remark  This function must be exported!
 */
VMMGCDECL(int) vmmGCLoggerFlush(PRTLOGGERGC pLogger);

/** @name Trap testcases and related labels.
 * @{ */
DECLASM(void) vmmGCEnableWP(void);
DECLASM(void) vmmGCDisableWP(void);
DECLASM(int) vmmGCTestTrap3(void);
DECLASM(int) vmmGCTestTrap8(void);
DECLASM(int) vmmGCTestTrap0d(void);
DECLASM(int) vmmGCTestTrap0e(void);
DECLASM(int) vmmGCTestTrap0e_FaultEIP(void); /**< a label */
DECLASM(int) vmmGCTestTrap0e_ResumeEIP(void); /**< a label */
/** @} */

#endif /* IN_GC */

__END_DECLS

/** @} */

#endif
