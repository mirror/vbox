/* $Revision$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Common code.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
#include "SUPDrvInternal.h"
#ifndef PAGE_SHIFT
# include <iprt/param.h>
#endif
#include <iprt/alloc.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/thread.h>
#include <iprt/process.h>
#include <iprt/mp.h>
#include <iprt/power.h>
#include <iprt/cpuset.h>
#include <iprt/uuid.h>
#include <VBox/param.h>
#include <VBox/log.h>
#include <VBox/err.h>
#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
# include <iprt/crc32.h>
# include <iprt/net.h>
# include <iprt/string.h>
#endif
/* VBox/x86.h not compatible with the Linux kernel sources */
#ifdef RT_OS_LINUX
# define X86_CPUID_VENDOR_AMD_EBX       0x68747541
# define X86_CPUID_VENDOR_AMD_ECX       0x444d4163
# define X86_CPUID_VENDOR_AMD_EDX       0x69746e65
#else
# include <VBox/x86.h>
#endif

/*
 * Logging assignments:
 *      Log     - useful stuff, like failures.
 *      LogFlow - program flow, except the really noisy bits.
 *      Log2    - Cleanup.
 *      Log3    - Loader flow noise.
 *      Log4    - Call VMMR0 flow noise.
 *      Log5    - Native yet-to-be-defined noise.
 *      Log6    - Native ioctl flow noise.
 *
 * Logging requires BUILD_TYPE=debug and possibly changes to the logger
 * instanciation in log-vbox.c(pp).
 */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/* from x86.h - clashes with linux thus this duplication */
#undef X86_CR0_PG
#define X86_CR0_PG                          RT_BIT(31)
#undef X86_CR0_PE
#define X86_CR0_PE                          RT_BIT(0)
#undef X86_CPUID_AMD_FEATURE_EDX_NX
#define X86_CPUID_AMD_FEATURE_EDX_NX        RT_BIT(20)
#undef MSR_K6_EFER
#define MSR_K6_EFER                         0xc0000080
#undef MSR_K6_EFER_NXE
#define MSR_K6_EFER_NXE                     RT_BIT(11)
#undef MSR_K6_EFER_LMA
#define  MSR_K6_EFER_LMA                    RT_BIT(10)
#undef X86_CR4_PGE
#define X86_CR4_PGE                         RT_BIT(7)
#undef X86_CR4_PAE
#define X86_CR4_PAE                         RT_BIT(5)
#undef X86_CPUID_AMD_FEATURE_EDX_LONG_MODE
#define X86_CPUID_AMD_FEATURE_EDX_LONG_MODE RT_BIT(29)


/** The frequency by which we recalculate the u32UpdateHz and
 * u32UpdateIntervalNS GIP members. The value must be a power of 2. */
#define GIP_UPDATEHZ_RECALC_FREQ            0x800

/**
 * Validates a session pointer.
 *
 * @returns true/false accordingly.
 * @param   pSession    The session.
 */
#define SUP_IS_SESSION_VALID(pSession)  \
    (   VALID_PTR(pSession) \
     && pSession->u32Cookie == BIRD_INV)

/** @def VBOX_SVN_REV
 * The makefile should define this if it can. */
#ifndef VBOX_SVN_REV
# define VBOX_SVN_REV 0
#endif

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int      supdrvMemAdd(PSUPDRVMEMREF pMem, PSUPDRVSESSION pSession);
static int      supdrvMemRelease(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, SUPDRVMEMREFTYPE eType);
static int      supdrvIOCtl_LdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDROPEN pReq);
static int      supdrvIOCtl_LdrLoad(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRLOAD pReq);
static int      supdrvIOCtl_LdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRFREE pReq);
static int      supdrvIOCtl_LdrGetSymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRGETSYMBOL pReq);
static int      supdrvIDC_LdrGetSymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPDRVIDCREQGETSYM pReq);
static int      supdrvLdrSetVMMR0EPs(PSUPDRVDEVEXT pDevExt, void *pvVMMR0, void *pvVMMR0EntryInt, void *pvVMMR0EntryFast, void *pvVMMR0EntryEx);
static void     supdrvLdrUnsetVMMR0EPs(PSUPDRVDEVEXT pDevExt);
static int      supdrvLdrAddUsage(PSUPDRVSESSION pSession, PSUPDRVLDRIMAGE pImage);
static void     supdrvLdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage);
static int      supdrvIOCtl_CallServiceModule(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPCALLSERVICE pReq);
static int      supdrvIOCtl_LoggerSettings(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLOGGERSETTINGS pReq);
static SUPGIPMODE supdrvGipDeterminTscMode(PSUPDRVDEVEXT pDevExt);
#ifdef RT_OS_WINDOWS
static int      supdrvPageGetPhys(PSUPDRVSESSION pSession, RTR3PTR pvR3, uint32_t cPages, PRTHCPHYS paPages);
static bool     supdrvPageWasLockedByPageAlloc(PSUPDRVSESSION pSession, RTR3PTR pvR3);
#endif /* RT_OS_WINDOWS */
static int      supdrvGipCreate(PSUPDRVDEVEXT pDevExt);
static void     supdrvGipDestroy(PSUPDRVDEVEXT pDevExt);
static DECLCALLBACK(void) supdrvGipSyncTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick);
static DECLCALLBACK(void) supdrvGipAsyncTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick);
static DECLCALLBACK(void) supdrvGipMpEvent(RTMPEVENT enmEvent, RTCPUID idCpu, void *pvUser);

#ifdef RT_WITH_W64_UNWIND_HACK
DECLASM(int)    supdrvNtWrapVMMR0EntryEx(PFNRT pfnVMMR0EntryEx, PVM pVM, unsigned idCpu, unsigned uOperation, PSUPVMMR0REQHDR pReq, uint64_t u64Arg, PSUPDRVSESSION pSession);
DECLASM(int)    supdrvNtWrapVMMR0EntryFast(PFNRT pfnVMMR0EntryFast, PVM pVM, unsigned idCpu, unsigned uOperation);
DECLASM(void)   supdrvNtWrapObjDestructor(PFNRT pfnDestruction, void *pvObj, void *pvUser1, void *pvUser2);
DECLASM(void *) supdrvNtWrapQueryFactoryInterface(PFNRT pfnQueryFactoryInterface, struct SUPDRVFACTORY const *pSupDrvFactory, PSUPDRVSESSION pSession, const char *pszInterfaceUuid);
DECLASM(int)    supdrvNtWrapModuleInit(PFNRT pfnModuleInit);
DECLASM(void)   supdrvNtWrapModuleTerm(PFNRT pfnModuleTerm);
DECLASM(int)    supdrvNtWrapServiceReqHandler(PFNRT pfnServiceReqHandler, PSUPDRVSESSION pSession, uint32_t uOperation, uint64_t u64Arg, PSUPR0SERVICEREQHDR pReqHdr);

DECLASM(int)    UNWIND_WRAP(SUPR0ComponentRegisterFactory)(PSUPDRVSESSION pSession, PCSUPDRVFACTORY pFactory);
DECLASM(int)    UNWIND_WRAP(SUPR0ComponentDeregisterFactory)(PSUPDRVSESSION pSession, PCSUPDRVFACTORY pFactory);
DECLASM(int)    UNWIND_WRAP(SUPR0ComponentQueryFactory)(PSUPDRVSESSION pSession, const char *pszName, const char *pszInterfaceUuid, void **ppvFactoryIf);
DECLASM(void *) UNWIND_WRAP(SUPR0ObjRegister)(PSUPDRVSESSION pSession, SUPDRVOBJTYPE enmType, PFNSUPDRVDESTRUCTOR pfnDestructor, void *pvUser1, void *pvUser2);
DECLASM(int)    UNWIND_WRAP(SUPR0ObjAddRef)(void *pvObj, PSUPDRVSESSION pSession);
DECLASM(int)    UNWIND_WRAP(SUPR0ObjAddRefEx)(void *pvObj, PSUPDRVSESSION pSession, bool fNoPreempt);
DECLASM(int)    UNWIND_WRAP(SUPR0ObjRelease)(void *pvObj, PSUPDRVSESSION pSession);
DECLASM(int)    UNWIND_WRAP(SUPR0ObjVerifyAccess)(void *pvObj, PSUPDRVSESSION pSession, const char *pszObjName);
DECLASM(int)    UNWIND_WRAP(SUPR0LockMem)(PSUPDRVSESSION pSession, RTR3PTR pvR3, uint32_t cPages, PRTHCPHYS paPages);
DECLASM(int)    UNWIND_WRAP(SUPR0UnlockMem)(PSUPDRVSESSION pSession, RTR3PTR pvR3);
DECLASM(int)    UNWIND_WRAP(SUPR0ContAlloc)(PSUPDRVSESSION pSession, uint32_t cPages, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PRTHCPHYS pHCPhys);
DECLASM(int)    UNWIND_WRAP(SUPR0ContFree)(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr);
DECLASM(int)    UNWIND_WRAP(SUPR0LowAlloc)(PSUPDRVSESSION pSession, uint32_t cPages, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PRTHCPHYS paPages);
DECLASM(int)    UNWIND_WRAP(SUPR0LowFree)(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr);
DECLASM(int)    UNWIND_WRAP(SUPR0MemAlloc)(PSUPDRVSESSION pSession, uint32_t cb, PRTR0PTR ppvR0, PRTR3PTR ppvR3);
DECLASM(int)    UNWIND_WRAP(SUPR0MemGetPhys)(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, PSUPPAGE paPages);
DECLASM(int)    UNWIND_WRAP(SUPR0MemFree)(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr);
DECLASM(int)    UNWIND_WRAP(SUPR0PageAlloc)(PSUPDRVSESSION pSession, uint32_t cPages, PRTR3PTR ppvR3, PRTHCPHYS paPages);
DECLASM(int)    UNWIND_WRAP(SUPR0PageFree)(PSUPDRVSESSION pSession, RTR3PTR pvR3);
//DECLASM(int)    UNWIND_WRAP(SUPR0Printf)(const char *pszFormat, ...);
DECLASM(SUPPAGINGMODE) UNWIND_WRAP(SUPR0GetPagingMode)(void);
DECLASM(void *) UNWIND_WRAP(RTMemAlloc)(size_t cb) RT_NO_THROW;
DECLASM(void *) UNWIND_WRAP(RTMemAllocZ)(size_t cb) RT_NO_THROW;
DECLASM(void)   UNWIND_WRAP(RTMemFree)(void *pv) RT_NO_THROW;
DECLASM(void *) UNWIND_WRAP(RTMemDup)(const void *pvSrc, size_t cb) RT_NO_THROW;
DECLASM(void *) UNWIND_WRAP(RTMemDupEx)(const void *pvSrc, size_t cbSrc, size_t cbExtra) RT_NO_THROW;
DECLASM(void *) UNWIND_WRAP(RTMemRealloc)(void *pvOld, size_t cbNew) RT_NO_THROW;
DECLASM(int)    UNWIND_WRAP(RTR0MemObjAllocLow)(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable);
DECLASM(int)    UNWIND_WRAP(RTR0MemObjAllocPage)(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable);
DECLASM(int)    UNWIND_WRAP(RTR0MemObjAllocPhys)(PRTR0MEMOBJ pMemObj, size_t cb, RTHCPHYS PhysHighest);
DECLASM(int)    UNWIND_WRAP(RTR0MemObjAllocPhysNC)(PRTR0MEMOBJ pMemObj, size_t cb, RTHCPHYS PhysHighest);
DECLASM(int)    UNWIND_WRAP(RTR0MemObjAllocCont)(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable);
DECLASM(int)    UNWIND_WRAP(RTR0MemObjEnterPhys)(PRTR0MEMOBJ pMemObj, RTHCPHYS Phys, size_t cb);
DECLASM(int)    UNWIND_WRAP(RTR0MemObjLockUser)(PRTR0MEMOBJ pMemObj, RTR3PTR R3Ptr, size_t cb, RTR0PROCESS R0Process);
DECLASM(int)    UNWIND_WRAP(RTR0MemObjMapKernel)(PRTR0MEMOBJ pMemObj, RTR0MEMOBJ MemObjToMap, void *pvFixed, size_t uAlignment, unsigned fProt);
DECLASM(int)    UNWIND_WRAP(RTR0MemObjMapKernelEx)(PRTR0MEMOBJ pMemObj, RTR0MEMOBJ MemObjToMap, void *pvFixed, size_t uAlignment, unsigned fProt, size_t offSub, size_t cbSub);
DECLASM(int)    UNWIND_WRAP(RTR0MemObjMapUser)(PRTR0MEMOBJ pMemObj, RTR0MEMOBJ MemObjToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process);
/*DECLASM(void *) UNWIND_WRAP(RTR0MemObjAddress)(RTR0MEMOBJ MemObj); - not necessary */
/*DECLASM(RTR3PTR) UNWIND_WRAP(RTR0MemObjAddressR3)(RTR0MEMOBJ MemObj); - not necessary */
/*DECLASM(size_t) UNWIND_WRAP(RTR0MemObjSize)(RTR0MEMOBJ MemObj); - not necessary */
/*DECLASM(bool)   UNWIND_WRAP(RTR0MemObjIsMapping)(RTR0MEMOBJ MemObj); - not necessary */
/*DECLASM(RTHCPHYS) UNWIND_WRAP(RTR0MemObjGetPagePhysAddr)(RTR0MEMOBJ MemObj, size_t iPage); - not necessary */
DECLASM(int)    UNWIND_WRAP(RTR0MemObjFree)(RTR0MEMOBJ MemObj, bool fFreeMappings);
/* RTProcSelf             - not necessary */
/* RTR0ProcHandleSelf     - not necessary */
DECLASM(int)    UNWIND_WRAP(RTSemFastMutexCreate)(PRTSEMFASTMUTEX pMutexSem);
DECLASM(int)    UNWIND_WRAP(RTSemFastMutexDestroy)(RTSEMFASTMUTEX MutexSem);
DECLASM(int)    UNWIND_WRAP(RTSemFastMutexRequest)(RTSEMFASTMUTEX MutexSem);
DECLASM(int)    UNWIND_WRAP(RTSemFastMutexRelease)(RTSEMFASTMUTEX MutexSem);
DECLASM(int)    UNWIND_WRAP(RTSemEventCreate)(PRTSEMEVENT pEventSem);
DECLASM(int)    UNWIND_WRAP(RTSemEventSignal)(RTSEMEVENT EventSem);
DECLASM(int)    UNWIND_WRAP(RTSemEventWait)(RTSEMEVENT EventSem, unsigned cMillies);
DECLASM(int)    UNWIND_WRAP(RTSemEventWaitNoResume)(RTSEMEVENT EventSem, unsigned cMillies);
DECLASM(int)    UNWIND_WRAP(RTSemEventDestroy)(RTSEMEVENT EventSem);
DECLASM(int)    UNWIND_WRAP(RTSemEventMultiCreate)(PRTSEMEVENTMULTI pEventMultiSem);
DECLASM(int)    UNWIND_WRAP(RTSemEventMultiSignal)(RTSEMEVENTMULTI EventMultiSem);
DECLASM(int)    UNWIND_WRAP(RTSemEventMultiReset)(RTSEMEVENTMULTI EventMultiSem);
DECLASM(int)    UNWIND_WRAP(RTSemEventMultiWait)(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies);
DECLASM(int)    UNWIND_WRAP(RTSemEventMultiWaitNoResume)(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies);
DECLASM(int)    UNWIND_WRAP(RTSemEventMultiDestroy)(RTSEMEVENTMULTI EventMultiSem);
DECLASM(int)    UNWIND_WRAP(RTSpinlockCreate)(PRTSPINLOCK pSpinlock);
DECLASM(int)    UNWIND_WRAP(RTSpinlockDestroy)(RTSPINLOCK Spinlock);
DECLASM(void)   UNWIND_WRAP(RTSpinlockAcquire)(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp);
DECLASM(void)   UNWIND_WRAP(RTSpinlockRelease)(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp);
DECLASM(void)   UNWIND_WRAP(RTSpinlockAcquireNoInts)(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp);
DECLASM(void)   UNWIND_WRAP(RTSpinlockReleaseNoInts)(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp);
/* RTTimeNanoTS           - not necessary */
/* RTTimeMilliTS          - not necessary */
/* RTTimeSystemNanoTS     - not necessary */
/* RTTimeSystemMilliTS    - not necessary */
/* RTThreadNativeSelf     - not necessary */
DECLASM(int)    UNWIND_WRAP(RTThreadSleep)(unsigned cMillies);
DECLASM(bool)   UNWIND_WRAP(RTThreadYield)(void);
#if 0
/* RTThreadSelf           - not necessary */
DECLASM(int)    UNWIND_WRAP(RTThreadCreate)(PRTTHREAD pThread, PFNRTTHREAD pfnThread, void *pvUser, size_t cbStack,
                                            RTTHREADTYPE enmType, unsigned fFlags, const char *pszName);
DECLASM(RTNATIVETHREAD) UNWIND_WRAP(RTThreadGetNative)(RTTHREAD Thread);
DECLASM(int)    UNWIND_WRAP(RTThreadWait)(RTTHREAD Thread, unsigned cMillies, int *prc);
DECLASM(int)    UNWIND_WRAP(RTThreadWaitNoResume)(RTTHREAD Thread, unsigned cMillies, int *prc);
DECLASM(const char *) UNWIND_WRAP(RTThreadGetName)(RTTHREAD Thread);
DECLASM(const char *) UNWIND_WRAP(RTThreadSelfName)(void);
DECLASM(RTTHREADTYPE) UNWIND_WRAP(RTThreadGetType)(RTTHREAD Thread);
DECLASM(int)    UNWIND_WRAP(RTThreadUserSignal)(RTTHREAD Thread);
DECLASM(int)    UNWIND_WRAP(RTThreadUserReset)(RTTHREAD Thread);
DECLASM(int)    UNWIND_WRAP(RTThreadUserWait)(RTTHREAD Thread, unsigned cMillies);
DECLASM(int)    UNWIND_WRAP(RTThreadUserWaitNoResume)(RTTHREAD Thread, unsigned cMillies);
#endif
/* RTLogDefaultInstance   - a bit of a gamble, but we do not want the overhead! */
/* RTMpCpuId              - not necessary */
/* RTMpCpuIdFromSetIndex  - not necessary */
/* RTMpCpuIdToSetIndex    - not necessary */
/* RTMpIsCpuPossible      - not necessary */
/* RTMpGetCount           - not necessary */
/* RTMpGetMaxCpuId        - not necessary */
/* RTMpGetOnlineCount     - not necessary */
/* RTMpGetOnlineSet       - not necessary */
/* RTMpGetSet             - not necessary */
/* RTMpIsCpuOnline        - not necessary */
DECLASM(int)   UNWIND_WRAP(RTMpOnAll)(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2);
DECLASM(int)   UNWIND_WRAP(RTMpOnOthers)(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2);
DECLASM(int)   UNWIND_WRAP(RTMpOnSpecific)(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2);
DECLASM(int)   UNWIND_WRAP(RTMpIsCpuWorkPending)(void);
/* RTLogRelDefaultInstance - not necessary. */
DECLASM(int)   UNWIND_WRAP(RTLogSetDefaultInstanceThread)(PRTLOGGER pLogger, uintptr_t uKey);
/* RTLogLogger            - can't wrap this buster.  */
/* RTLogLoggerEx          - can't wrap this buster. */
DECLASM(void)  UNWIND_WRAP(RTLogLoggerExV)(PRTLOGGER pLogger, unsigned fFlags, unsigned iGroup, const char *pszFormat, va_list args);
/* RTLogPrintf            - can't wrap this buster. */  /** @todo provide va_list log wrappers in RuntimeR0. */
DECLASM(void)  UNWIND_WRAP(RTLogPrintfV)(const char *pszFormat, va_list args);
DECLASM(void)  UNWIND_WRAP(AssertMsg1)(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction);
/* AssertMsg2             - can't wrap this buster. */
#endif /* RT_WITH_W64_UNWIND_HACK */


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Array of the R0 SUP API.
 */
static SUPFUNC g_aFunctions[] =
{
    /* name                                     function */
        /* Entries with absolute addresses determined at runtime, fixup
           code makes ugly ASSUMPTIONS about the order here: */
    { "SUPR0AbsIs64bit",                        (void *)0 },
    { "SUPR0Abs64bitKernelCS",                  (void *)0 },
    { "SUPR0Abs64bitKernelSS",                  (void *)0 },
    { "SUPR0Abs64bitKernelDS",                  (void *)0 },
    { "SUPR0AbsKernelCS",                       (void *)0 },
    { "SUPR0AbsKernelSS",                       (void *)0 },
    { "SUPR0AbsKernelDS",                       (void *)0 },
    { "SUPR0AbsKernelES",                       (void *)0 },
    { "SUPR0AbsKernelFS",                       (void *)0 },
    { "SUPR0AbsKernelGS",                       (void *)0 },
        /* Normal function pointers: */
    { "SUPR0ComponentRegisterFactory",          (void *)UNWIND_WRAP(SUPR0ComponentRegisterFactory) },
    { "SUPR0ComponentDeregisterFactory",        (void *)UNWIND_WRAP(SUPR0ComponentDeregisterFactory) },
    { "SUPR0ComponentQueryFactory",             (void *)UNWIND_WRAP(SUPR0ComponentQueryFactory) },
    { "SUPR0ObjRegister",                       (void *)UNWIND_WRAP(SUPR0ObjRegister) },
    { "SUPR0ObjAddRef",                         (void *)UNWIND_WRAP(SUPR0ObjAddRef) },
    { "SUPR0ObjAddRefEx",                       (void *)UNWIND_WRAP(SUPR0ObjAddRefEx) },
    { "SUPR0ObjRelease",                        (void *)UNWIND_WRAP(SUPR0ObjRelease) },
    { "SUPR0ObjVerifyAccess",                   (void *)UNWIND_WRAP(SUPR0ObjVerifyAccess) },
    { "SUPR0LockMem",                           (void *)UNWIND_WRAP(SUPR0LockMem) },
    { "SUPR0UnlockMem",                         (void *)UNWIND_WRAP(SUPR0UnlockMem) },
    { "SUPR0ContAlloc",                         (void *)UNWIND_WRAP(SUPR0ContAlloc) },
    { "SUPR0ContFree",                          (void *)UNWIND_WRAP(SUPR0ContFree) },
    { "SUPR0LowAlloc",                          (void *)UNWIND_WRAP(SUPR0LowAlloc) },
    { "SUPR0LowFree",                           (void *)UNWIND_WRAP(SUPR0LowFree) },
    { "SUPR0MemAlloc",                          (void *)UNWIND_WRAP(SUPR0MemAlloc) },
    { "SUPR0MemGetPhys",                        (void *)UNWIND_WRAP(SUPR0MemGetPhys) },
    { "SUPR0MemFree",                           (void *)UNWIND_WRAP(SUPR0MemFree) },
    { "SUPR0PageAlloc",                         (void *)UNWIND_WRAP(SUPR0PageAlloc) },
    { "SUPR0PageFree",                          (void *)UNWIND_WRAP(SUPR0PageFree) },
    { "SUPR0Printf",                            (void *)SUPR0Printf }, /** @todo needs wrapping? */
    { "SUPR0GetPagingMode",                     (void *)UNWIND_WRAP(SUPR0GetPagingMode) },
    { "SUPR0EnableVTx",                         (void *)SUPR0EnableVTx },
    { "RTMemAlloc",                             (void *)UNWIND_WRAP(RTMemAlloc) },
    { "RTMemAllocZ",                            (void *)UNWIND_WRAP(RTMemAllocZ) },
    { "RTMemFree",                              (void *)UNWIND_WRAP(RTMemFree) },
    /*{ "RTMemDup",                               (void *)UNWIND_WRAP(RTMemDup) },
    { "RTMemDupEx",                             (void *)UNWIND_WRAP(RTMemDupEx) },*/
    { "RTMemRealloc",                           (void *)UNWIND_WRAP(RTMemRealloc) },
    { "RTR0MemObjAllocLow",                     (void *)UNWIND_WRAP(RTR0MemObjAllocLow) },
    { "RTR0MemObjAllocPage",                    (void *)UNWIND_WRAP(RTR0MemObjAllocPage) },
    { "RTR0MemObjAllocPhys",                    (void *)UNWIND_WRAP(RTR0MemObjAllocPhys) },
    { "RTR0MemObjAllocPhysNC",                  (void *)UNWIND_WRAP(RTR0MemObjAllocPhysNC) },
    { "RTR0MemObjAllocCont",                    (void *)UNWIND_WRAP(RTR0MemObjAllocCont) },
    { "RTR0MemObjEnterPhys",                    (void *)UNWIND_WRAP(RTR0MemObjEnterPhys) },
    { "RTR0MemObjLockUser",                     (void *)UNWIND_WRAP(RTR0MemObjLockUser) },
    { "RTR0MemObjMapKernel",                    (void *)UNWIND_WRAP(RTR0MemObjMapKernel) },
    { "RTR0MemObjMapKernelEx",                  (void *)UNWIND_WRAP(RTR0MemObjMapKernelEx) },
    { "RTR0MemObjMapUser",                      (void *)UNWIND_WRAP(RTR0MemObjMapUser) },
    { "RTR0MemObjAddress",                      (void *)RTR0MemObjAddress },
    { "RTR0MemObjAddressR3",                    (void *)RTR0MemObjAddressR3 },
    { "RTR0MemObjSize",                         (void *)RTR0MemObjSize },
    { "RTR0MemObjIsMapping",                    (void *)RTR0MemObjIsMapping },
    { "RTR0MemObjGetPagePhysAddr",              (void *)RTR0MemObjGetPagePhysAddr },
    { "RTR0MemObjFree",                         (void *)UNWIND_WRAP(RTR0MemObjFree) },
/* These don't work yet on linux - use fast mutexes!
    { "RTSemMutexCreate",                       (void *)RTSemMutexCreate },
    { "RTSemMutexRequest",                      (void *)RTSemMutexRequest },
    { "RTSemMutexRelease",                      (void *)RTSemMutexRelease },
    { "RTSemMutexDestroy",                      (void *)RTSemMutexDestroy },
*/
    { "RTProcSelf",                             (void *)RTProcSelf },
    { "RTR0ProcHandleSelf",                     (void *)RTR0ProcHandleSelf },
    { "RTSemFastMutexCreate",                   (void *)UNWIND_WRAP(RTSemFastMutexCreate) },
    { "RTSemFastMutexDestroy",                  (void *)UNWIND_WRAP(RTSemFastMutexDestroy) },
    { "RTSemFastMutexRequest",                  (void *)UNWIND_WRAP(RTSemFastMutexRequest) },
    { "RTSemFastMutexRelease",                  (void *)UNWIND_WRAP(RTSemFastMutexRelease) },
    { "RTSemEventCreate",                       (void *)UNWIND_WRAP(RTSemEventCreate) },
    { "RTSemEventSignal",                       (void *)UNWIND_WRAP(RTSemEventSignal) },
    { "RTSemEventWait",                         (void *)UNWIND_WRAP(RTSemEventWait) },
    { "RTSemEventWaitNoResume",                 (void *)UNWIND_WRAP(RTSemEventWaitNoResume) },
    { "RTSemEventDestroy",                      (void *)UNWIND_WRAP(RTSemEventDestroy) },
    { "RTSemEventMultiCreate",                  (void *)UNWIND_WRAP(RTSemEventMultiCreate) },
    { "RTSemEventMultiSignal",                  (void *)UNWIND_WRAP(RTSemEventMultiSignal) },
    { "RTSemEventMultiReset",                   (void *)UNWIND_WRAP(RTSemEventMultiReset) },
    { "RTSemEventMultiWait",                    (void *)UNWIND_WRAP(RTSemEventMultiWait) },
    { "RTSemEventMultiWaitNoResume",            (void *)UNWIND_WRAP(RTSemEventMultiWaitNoResume) },
    { "RTSemEventMultiDestroy",                 (void *)UNWIND_WRAP(RTSemEventMultiDestroy) },
    { "RTSpinlockCreate",                       (void *)UNWIND_WRAP(RTSpinlockCreate) },
    { "RTSpinlockDestroy",                      (void *)UNWIND_WRAP(RTSpinlockDestroy) },
    { "RTSpinlockAcquire",                      (void *)UNWIND_WRAP(RTSpinlockAcquire) },
    { "RTSpinlockRelease",                      (void *)UNWIND_WRAP(RTSpinlockRelease) },
    { "RTSpinlockAcquireNoInts",                (void *)UNWIND_WRAP(RTSpinlockAcquireNoInts) },
    { "RTSpinlockReleaseNoInts",                (void *)UNWIND_WRAP(RTSpinlockReleaseNoInts) },
    { "RTTimeNanoTS",                           (void *)RTTimeNanoTS },
    { "RTTimeMillieTS",                         (void *)RTTimeMilliTS },
    { "RTTimeSystemNanoTS",                     (void *)RTTimeSystemNanoTS },
    { "RTTimeSystemMillieTS",                   (void *)RTTimeSystemMilliTS },
    { "RTThreadNativeSelf",                     (void *)RTThreadNativeSelf },
    { "RTThreadSleep",                          (void *)UNWIND_WRAP(RTThreadSleep) },
    { "RTThreadYield",                          (void *)UNWIND_WRAP(RTThreadYield) },
#if 0 /* Thread APIs, Part 2. */
    { "RTThreadSelf",                           (void *)UNWIND_WRAP(RTThreadSelf) },
    { "RTThreadCreate",                         (void *)UNWIND_WRAP(RTThreadCreate) }, /** @todo need to wrap the callback */
    { "RTThreadGetNative",                      (void *)UNWIND_WRAP(RTThreadGetNative) },
    { "RTThreadWait",                           (void *)UNWIND_WRAP(RTThreadWait) },
    { "RTThreadWaitNoResume",                   (void *)UNWIND_WRAP(RTThreadWaitNoResume) },
    { "RTThreadGetName",                        (void *)UNWIND_WRAP(RTThreadGetName) },
    { "RTThreadSelfName",                       (void *)UNWIND_WRAP(RTThreadSelfName) },
    { "RTThreadGetType",                        (void *)UNWIND_WRAP(RTThreadGetType) },
    { "RTThreadUserSignal",                     (void *)UNWIND_WRAP(RTThreadUserSignal) },
    { "RTThreadUserReset",                      (void *)UNWIND_WRAP(RTThreadUserReset) },
    { "RTThreadUserWait",                       (void *)UNWIND_WRAP(RTThreadUserWait) },
    { "RTThreadUserWaitNoResume",               (void *)UNWIND_WRAP(RTThreadUserWaitNoResume) },
#endif
    { "RTLogDefaultInstance",                   (void *)RTLogDefaultInstance },
    { "RTMpCpuId",                              (void *)RTMpCpuId },
    { "RTMpCpuIdFromSetIndex",                  (void *)RTMpCpuIdFromSetIndex },
    { "RTMpCpuIdToSetIndex",                    (void *)RTMpCpuIdToSetIndex },
    { "RTMpIsCpuPossible",                      (void *)RTMpIsCpuPossible },
    { "RTMpGetCount",                           (void *)RTMpGetCount },
    { "RTMpGetMaxCpuId",                        (void *)RTMpGetMaxCpuId },
    { "RTMpGetOnlineCount",                     (void *)RTMpGetOnlineCount },
    { "RTMpGetOnlineSet",                       (void *)RTMpGetOnlineSet },
    { "RTMpGetSet",                             (void *)RTMpGetSet },
    { "RTMpIsCpuOnline",                        (void *)RTMpIsCpuOnline },
    { "RTMpIsCpuWorkPending",                   (void *)UNWIND_WRAP(RTMpIsCpuWorkPending) },
    { "RTMpOnAll",                              (void *)UNWIND_WRAP(RTMpOnAll) },
    { "RTMpOnOthers",                           (void *)UNWIND_WRAP(RTMpOnOthers) },
    { "RTMpOnSpecific",                         (void *)UNWIND_WRAP(RTMpOnSpecific) },
    { "RTPowerNotificationRegister",            (void *)RTPowerNotificationRegister },
    { "RTPowerNotificationDeregister",          (void *)RTPowerNotificationDeregister },
    { "RTLogRelDefaultInstance",                (void *)RTLogRelDefaultInstance },
    { "RTLogSetDefaultInstanceThread",          (void *)UNWIND_WRAP(RTLogSetDefaultInstanceThread) },
    { "RTLogLogger",                            (void *)RTLogLogger }, /** @todo remove this */
    { "RTLogLoggerEx",                          (void *)RTLogLoggerEx }, /** @todo remove this */
    { "RTLogLoggerExV",                         (void *)UNWIND_WRAP(RTLogLoggerExV) },
    { "RTLogPrintf",                            (void *)RTLogPrintf }, /** @todo remove this */
    { "RTLogPrintfV",                           (void *)UNWIND_WRAP(RTLogPrintfV) },
    { "AssertMsg1",                             (void *)UNWIND_WRAP(AssertMsg1) },
    { "AssertMsg2",                             (void *)AssertMsg2 }, /** @todo replace this by RTAssertMsg2V */
#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
    { "RTR0AssertPanicSystem",                  (void *)RTR0AssertPanicSystem },
#endif
#if defined(RT_OS_DARWIN)
    { "RTAssertMsg1",                           (void *)RTAssertMsg1 },
    { "RTAssertMsg2",                           (void *)RTAssertMsg2 },
    { "RTAssertMsg2V",                          (void *)RTAssertMsg2V },
#endif
};

#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
/**
 * Drag in the rest of IRPT since we share it with the
 * rest of the kernel modules on darwin.
 */
PFNRT g_apfnVBoxDrvIPRTDeps[] =
{
    (PFNRT)RTCrc32,
    (PFNRT)RTErrConvertFromErrno,
    (PFNRT)RTNetIPv4IsHdrValid,
    (PFNRT)RTNetIPv4TCPChecksum,
    (PFNRT)RTNetIPv4UDPChecksum,
    (PFNRT)RTUuidCompare,
    (PFNRT)RTUuidCompareStr,
    (PFNRT)RTUuidFromStr,
    (PFNRT)RTStrDup,
    (PFNRT)RTStrFree,
    NULL
};
#endif  /* RT_OS_DARWIN || RT_OS_SOLARIS */


/**
 * Initializes the device extentsion structure.
 *
 * @returns IPRT status code.
 * @param   pDevExt     The device extension to initialize.
 */
int VBOXCALL supdrvInitDevExt(PSUPDRVDEVEXT pDevExt)
{
    int rc;

#ifdef SUPDRV_WITH_RELEASE_LOGGER
    /*
     * Create the release log.
     */
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    PRTLOGGER pRelLogger;
    rc = RTLogCreate(&pRelLogger, 0 /* fFlags */, "all",
                     "VBOX_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups,
                     RTLOGDEST_STDOUT | RTLOGDEST_DEBUGGER, NULL);
    if (RT_SUCCESS(rc))
        RTLogRelSetDefaultInstance(pRelLogger);
#endif

    /*
     * Initialize it.
     */
    memset(pDevExt, 0, sizeof(*pDevExt));
    rc = RTSpinlockCreate(&pDevExt->Spinlock);
    if (!rc)
    {
        rc = RTSemFastMutexCreate(&pDevExt->mtxLdr);
        if (!rc)
        {
            rc = RTSemFastMutexCreate(&pDevExt->mtxComponentFactory);
            if (!rc)
            {
                rc = RTSemFastMutexCreate(&pDevExt->mtxGip);
                if (!rc)
                {
                    rc = supdrvGipCreate(pDevExt);
                    if (RT_SUCCESS(rc))
                    {
                        pDevExt->u32Cookie = BIRD;  /** @todo make this random? */

                        /*
                         * Fixup the absolute symbols.
                         *
                         * Because of the table indexing assumptions we'll have a little #ifdef orgy
                         * here rather than distributing this to OS specific files. At least for now.
                         */
#ifdef RT_OS_DARWIN
# if ARCH_BITS == 32
                        if (SUPR0GetPagingMode() >= SUPPAGINGMODE_AMD64)
                        {
                            g_aFunctions[0].pfn = (void *)1;                    /* SUPR0AbsIs64bit */
                            g_aFunctions[1].pfn = (void *)0x80;                 /* SUPR0Abs64bitKernelCS - KERNEL64_CS, seg.h */
                            g_aFunctions[2].pfn = (void *)0x88;                 /* SUPR0Abs64bitKernelSS - KERNEL64_SS, seg.h */
                            g_aFunctions[3].pfn = (void *)0x88;                 /* SUPR0Abs64bitKernelDS - KERNEL64_SS, seg.h */
                        }
                        else
                            g_aFunctions[0].pfn = g_aFunctions[1].pfn = g_aFunctions[2].pfn = g_aFunctions[4].pfn = (void *)0;
                        g_aFunctions[4].pfn = (void *)0x08;                     /* SUPR0AbsKernelCS - KERNEL_CS, seg.h */
                        g_aFunctions[5].pfn = (void *)0x10;                     /* SUPR0AbsKernelSS - KERNEL_DS, seg.h */
                        g_aFunctions[6].pfn = (void *)0x10;                     /* SUPR0AbsKernelDS - KERNEL_DS, seg.h */
                        g_aFunctions[7].pfn = (void *)0x10;                     /* SUPR0AbsKernelES - KERNEL_DS, seg.h */
                        g_aFunctions[8].pfn = (void *)0x10;                     /* SUPR0AbsKernelFS - KERNEL_DS, seg.h */
                        g_aFunctions[9].pfn = (void *)0x48;                     /* SUPR0AbsKernelGS - CPU_DATA_GS, seg.h */
# else /* 64-bit darwin: */
                        g_aFunctions[0].pfn = (void *)1;                        /* SUPR0AbsIs64bit */
                        g_aFunctions[1].pfn = (void *)(uintptr_t)ASMGetCS();    /* SUPR0Abs64bitKernelCS */
                        g_aFunctions[2].pfn = (void *)(uintptr_t)ASMGetSS();    /* SUPR0Abs64bitKernelSS */
                        g_aFunctions[3].pfn = (void *)0;                        /* SUPR0Abs64bitKernelDS */
                        g_aFunctions[4].pfn = (void *)(uintptr_t)ASMGetCS();    /* SUPR0AbsKernelCS */
                        g_aFunctions[5].pfn = (void *)(uintptr_t)ASMGetSS();    /* SUPR0AbsKernelSS */
                        g_aFunctions[6].pfn = (void *)0;                        /* SUPR0AbsKernelDS */
                        g_aFunctions[7].pfn = (void *)0;                        /* SUPR0AbsKernelES */
                        g_aFunctions[8].pfn = (void *)0;                        /* SUPR0AbsKernelFS */
                        g_aFunctions[9].pfn = (void *)0;                        /* SUPR0AbsKernelGS */

# endif
#else  /* !RT_OS_DARWIN */
# if ARCH_BITS == 64
                        g_aFunctions[0].pfn = (void *)1;                        /* SUPR0AbsIs64bit */
                        g_aFunctions[1].pfn = (void *)(uintptr_t)ASMGetCS();    /* SUPR0Abs64bitKernelCS */
                        g_aFunctions[2].pfn = (void *)(uintptr_t)ASMGetSS();    /* SUPR0Abs64bitKernelSS */
                        g_aFunctions[3].pfn = (void *)(uintptr_t)ASMGetDS();    /* SUPR0Abs64bitKernelDS */
# else
                        g_aFunctions[0].pfn = g_aFunctions[1].pfn = g_aFunctions[2].pfn = g_aFunctions[4].pfn = (void *)0;
# endif
                        g_aFunctions[4].pfn = (void *)(uintptr_t)ASMGetCS();    /* SUPR0AbsKernelCS */
                        g_aFunctions[5].pfn = (void *)(uintptr_t)ASMGetSS();    /* SUPR0AbsKernelSS */
                        g_aFunctions[6].pfn = (void *)(uintptr_t)ASMGetDS();    /* SUPR0AbsKernelDS */
                        g_aFunctions[7].pfn = (void *)(uintptr_t)ASMGetES();    /* SUPR0AbsKernelES */
                        g_aFunctions[8].pfn = (void *)(uintptr_t)ASMGetFS();    /* SUPR0AbsKernelFS */
                        g_aFunctions[9].pfn = (void *)(uintptr_t)ASMGetGS();    /* SUPR0AbsKernelGS */
#endif /* !RT_OS_DARWIN */
                        return VINF_SUCCESS;
                    }

                    RTSemFastMutexDestroy(pDevExt->mtxGip);
                    pDevExt->mtxGip = NIL_RTSEMFASTMUTEX;
                }
                RTSemFastMutexDestroy(pDevExt->mtxComponentFactory);
                pDevExt->mtxComponentFactory = NIL_RTSEMFASTMUTEX;
            }
            RTSemFastMutexDestroy(pDevExt->mtxLdr);
            pDevExt->mtxLdr = NIL_RTSEMFASTMUTEX;
        }
        RTSpinlockDestroy(pDevExt->Spinlock);
        pDevExt->Spinlock = NIL_RTSPINLOCK;
    }
#ifdef SUPDRV_WITH_RELEASE_LOGGER
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogDestroy(RTLogSetDefaultInstance(NULL));
#endif

    return rc;
}


/**
 * Delete the device extension (e.g. cleanup members).
 *
 * @param   pDevExt     The device extension to delete.
 */
void VBOXCALL supdrvDeleteDevExt(PSUPDRVDEVEXT pDevExt)
{
    PSUPDRVOBJ          pObj;
    PSUPDRVUSAGE        pUsage;

    /*
     * Kill mutexes and spinlocks.
     */
    RTSemFastMutexDestroy(pDevExt->mtxGip);
    pDevExt->mtxGip = NIL_RTSEMFASTMUTEX;
    RTSemFastMutexDestroy(pDevExt->mtxLdr);
    pDevExt->mtxLdr = NIL_RTSEMFASTMUTEX;
    RTSpinlockDestroy(pDevExt->Spinlock);
    pDevExt->Spinlock = NIL_RTSPINLOCK;
    RTSemFastMutexDestroy(pDevExt->mtxComponentFactory);
    pDevExt->mtxComponentFactory = NIL_RTSEMFASTMUTEX;

    /*
     * Free lists.
     */
    /* objects. */
    pObj = pDevExt->pObjs;
#if !defined(DEBUG_bird) || !defined(RT_OS_LINUX) /* breaks unloading, temporary, remove me! */
    Assert(!pObj);                      /* (can trigger on forced unloads) */
#endif
    pDevExt->pObjs = NULL;
    while (pObj)
    {
        void *pvFree = pObj;
        pObj = pObj->pNext;
        RTMemFree(pvFree);
    }

    /* usage records. */
    pUsage = pDevExt->pUsageFree;
    pDevExt->pUsageFree = NULL;
    while (pUsage)
    {
        void *pvFree = pUsage;
        pUsage = pUsage->pNext;
        RTMemFree(pvFree);
    }

    /* kill the GIP. */
    supdrvGipDestroy(pDevExt);

#ifdef SUPDRV_WITH_RELEASE_LOGGER
    /* destroy the loggers. */
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogDestroy(RTLogSetDefaultInstance(NULL));
#endif
}


/**
 * Create session.
 *
 * @returns IPRT status code.
 * @param   pDevExt     Device extension.
 * @param   fUser       Flag indicating whether this is a user or kernel session.
 * @param   ppSession   Where to store the pointer to the session data.
 */
int VBOXCALL supdrvCreateSession(PSUPDRVDEVEXT pDevExt, bool fUser, PSUPDRVSESSION *ppSession)
{
    /*
     * Allocate memory for the session data.
     */
    int rc = VERR_NO_MEMORY;
    PSUPDRVSESSION pSession = *ppSession = (PSUPDRVSESSION)RTMemAllocZ(sizeof(*pSession));
    if (pSession)
    {
        /* Initialize session data. */
        rc = RTSpinlockCreate(&pSession->Spinlock);
        if (!rc)
        {
            Assert(pSession->Spinlock != NIL_RTSPINLOCK);
            pSession->pDevExt           = pDevExt;
            pSession->u32Cookie         = BIRD_INV;
            /*pSession->pLdrUsage         = NULL;
            pSession->pVM               = NULL;
            pSession->pUsage            = NULL;
            pSession->pGip              = NULL;
            pSession->fGipReferenced    = false;
            pSession->Bundle.cUsed      = 0; */
            pSession->Uid               = NIL_RTUID;
            pSession->Gid               = NIL_RTGID;
            if (fUser)
            {
                pSession->Process       = RTProcSelf();
                pSession->R0Process     = RTR0ProcHandleSelf();
            }
            else
            {
                pSession->Process       = NIL_RTPROCESS;
                pSession->R0Process     = NIL_RTR0PROCESS;
            }

            LogFlow(("Created session %p initial cookie=%#x\n", pSession, pSession->u32Cookie));
            return VINF_SUCCESS;
        }

        RTMemFree(pSession);
        *ppSession = NULL;
        Log(("Failed to create spinlock, rc=%d!\n", rc));
    }

    return rc;
}


/**
 * Shared code for cleaning up a session.
 *
 * @param   pDevExt     Device extension.
 * @param   pSession    Session data.
 *                      This data will be freed by this routine.
 */
void VBOXCALL supdrvCloseSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    /*
     * Cleanup the session first.
     */
    supdrvCleanupSession(pDevExt, pSession);

    /*
     * Free the rest of the session stuff.
     */
    RTSpinlockDestroy(pSession->Spinlock);
    pSession->Spinlock = NIL_RTSPINLOCK;
    pSession->pDevExt = NULL;
    RTMemFree(pSession);
    LogFlow(("supdrvCloseSession: returns\n"));
}


/**
 * Shared code for cleaning up a session (but not quite freeing it).
 *
 * This is primarily intended for MAC OS X where we have to clean up the memory
 * stuff before the file handle is closed.
 *
 * @param   pDevExt     Device extension.
 * @param   pSession    Session data.
 *                      This data will be freed by this routine.
 */
void VBOXCALL supdrvCleanupSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    PSUPDRVBUNDLE       pBundle;
    LogFlow(("supdrvCleanupSession: pSession=%p\n", pSession));

    /*
     * Remove logger instances related to this session.
     */
    RTLogSetDefaultInstanceThread(NULL, (uintptr_t)pSession);

    /*
     * Release object references made in this session.
     * In theory there should be noone racing us in this session.
     */
    Log2(("release objects - start\n"));
    if (pSession->pUsage)
    {
        RTSPINLOCKTMP   SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
        PSUPDRVUSAGE    pUsage;
        RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);

        while ((pUsage = pSession->pUsage) != NULL)
        {
            PSUPDRVOBJ  pObj = pUsage->pObj;
            pSession->pUsage = pUsage->pNext;

            AssertMsg(pUsage->cUsage >= 1 && pObj->cUsage >= pUsage->cUsage, ("glob %d; sess %d\n", pObj->cUsage, pUsage->cUsage));
            if (pUsage->cUsage < pObj->cUsage)
            {
                pObj->cUsage -= pUsage->cUsage;
                RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);
            }
            else
            {
                /* Destroy the object and free the record. */
                if (pDevExt->pObjs == pObj)
                    pDevExt->pObjs = pObj->pNext;
                else
                {
                    PSUPDRVOBJ pObjPrev;
                    for (pObjPrev = pDevExt->pObjs; pObjPrev; pObjPrev = pObjPrev->pNext)
                        if (pObjPrev->pNext == pObj)
                        {
                            pObjPrev->pNext = pObj->pNext;
                            break;
                        }
                    Assert(pObjPrev);
                }
                RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);

                Log(("supdrvCleanupSession: destroying %p/%d (%p/%p) cpid=%RTproc pid=%RTproc dtor=%p\n",
                     pObj, pObj->enmType, pObj->pvUser1, pObj->pvUser2, pObj->CreatorProcess, RTProcSelf(), pObj->pfnDestructor));
                if (pObj->pfnDestructor)
#ifdef RT_WITH_W64_UNWIND_HACK
                    supdrvNtWrapObjDestructor((PFNRT)pObj->pfnDestructor, pObj, pObj->pvUser1, pObj->pvUser2);
#else
                    pObj->pfnDestructor(pObj, pObj->pvUser1, pObj->pvUser2);
#endif
                RTMemFree(pObj);
            }

            /* free it and continue. */
            RTMemFree(pUsage);

            RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);
        }

        RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);
        AssertMsg(!pSession->pUsage, ("Some buster reregistered an object during desturction!\n"));
    }
    Log2(("release objects - done\n"));

    /*
     * Release memory allocated in the session.
     *
     * We do not serialize this as we assume that the application will
     * not allocated memory while closing the file handle object.
     */
    Log2(("freeing memory:\n"));
    pBundle = &pSession->Bundle;
    while (pBundle)
    {
        PSUPDRVBUNDLE   pToFree;
        unsigned        i;

        /*
         * Check and unlock all entries in the bundle.
         */
        for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
        {
            if (pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ)
            {
                int rc;
                Log2(("eType=%d pvR0=%p pvR3=%p cb=%ld\n", pBundle->aMem[i].eType, RTR0MemObjAddress(pBundle->aMem[i].MemObj),
                      (void *)RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3), (long)RTR0MemObjSize(pBundle->aMem[i].MemObj)));
                if (pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ)
                {
                    rc = RTR0MemObjFree(pBundle->aMem[i].MapObjR3, false);
                    AssertRC(rc); /** @todo figure out how to handle this. */
                    pBundle->aMem[i].MapObjR3 = NIL_RTR0MEMOBJ;
                }
                rc = RTR0MemObjFree(pBundle->aMem[i].MemObj, true /* fFreeMappings */);
                AssertRC(rc); /** @todo figure out how to handle this. */
                pBundle->aMem[i].MemObj = NIL_RTR0MEMOBJ;
                pBundle->aMem[i].eType = MEMREF_TYPE_UNUSED;
            }
        }

        /*
         * Advance and free previous bundle.
         */
        pToFree = pBundle;
        pBundle = pBundle->pNext;

        pToFree->pNext = NULL;
        pToFree->cUsed = 0;
        if (pToFree != &pSession->Bundle)
            RTMemFree(pToFree);
    }
    Log2(("freeing memory - done\n"));

    /*
     * Deregister component factories.
     */
    RTSemFastMutexRequest(pDevExt->mtxComponentFactory);
    Log2(("deregistering component factories:\n"));
    if (pDevExt->pComponentFactoryHead)
    {
        PSUPDRVFACTORYREG pPrev = NULL;
        PSUPDRVFACTORYREG pCur = pDevExt->pComponentFactoryHead;
        while (pCur)
        {
            if (pCur->pSession == pSession)
            {
                /* unlink it */
                PSUPDRVFACTORYREG pNext = pCur->pNext;
                if (pPrev)
                    pPrev->pNext = pNext;
                else
                    pDevExt->pComponentFactoryHead = pNext;

                /* free it */
                pCur->pNext = NULL;
                pCur->pSession = NULL;
                pCur->pFactory = NULL;
                RTMemFree(pCur);

                /* next */
                pCur = pNext;
            }
            else
            {
                /* next */
                pPrev = pCur;
                pCur = pCur->pNext;
            }
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxComponentFactory);
    Log2(("deregistering component factories - done\n"));

    /*
     * Loaded images needs to be dereferenced and possibly freed up.
     */
    RTSemFastMutexRequest(pDevExt->mtxLdr);
    Log2(("freeing images:\n"));
    if (pSession->pLdrUsage)
    {
        PSUPDRVLDRUSAGE pUsage = pSession->pLdrUsage;
        pSession->pLdrUsage = NULL;
        while (pUsage)
        {
            void           *pvFree = pUsage;
            PSUPDRVLDRIMAGE pImage = pUsage->pImage;
            if (pImage->cUsage > pUsage->cUsage)
                pImage->cUsage -= pUsage->cUsage;
            else
                supdrvLdrFree(pDevExt, pImage);
            pUsage->pImage = NULL;
            pUsage = pUsage->pNext;
            RTMemFree(pvFree);
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxLdr);
    Log2(("freeing images - done\n"));

    /*
     * Unmap the GIP.
     */
    Log2(("umapping GIP:\n"));
    if (pSession->GipMapObjR3 != NIL_RTR0MEMOBJ)
    {
        SUPR0GipUnmap(pSession);
        pSession->fGipReferenced = 0;
    }
    Log2(("umapping GIP - done\n"));
}


/**
 * Fast path I/O Control worker.
 *
 * @returns VBox status code that should be passed down to ring-3 unchanged.
 * @param   uIOCtl      Function number.
 * @param   idCpu       VMCPU id.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 */
int VBOXCALL supdrvIOCtlFast(uintptr_t uIOCtl, unsigned idCpu, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    /*
     * We check the two prereqs after doing this only to allow the compiler to optimize things better.
     */
    if (RT_LIKELY(pSession->pVM && pDevExt->pfnVMMR0EntryFast))
    {
        switch (uIOCtl)
        {
            case SUP_IOCTL_FAST_DO_RAW_RUN:
#ifdef RT_WITH_W64_UNWIND_HACK
                supdrvNtWrapVMMR0EntryFast((PFNRT)pDevExt->pfnVMMR0EntryFast, pSession->pVM, idCpu, SUP_VMMR0_DO_RAW_RUN);
#else
                pDevExt->pfnVMMR0EntryFast(pSession->pVM, idCpu, SUP_VMMR0_DO_RAW_RUN);
#endif
                break;
            case SUP_IOCTL_FAST_DO_HWACC_RUN:
#ifdef RT_WITH_W64_UNWIND_HACK
                supdrvNtWrapVMMR0EntryFast((PFNRT)pDevExt->pfnVMMR0EntryFast, pSession->pVM, idCpu, SUP_VMMR0_DO_HWACC_RUN);
#else
                pDevExt->pfnVMMR0EntryFast(pSession->pVM, idCpu, SUP_VMMR0_DO_HWACC_RUN);
#endif
                break;
            case SUP_IOCTL_FAST_DO_NOP:
#ifdef RT_WITH_W64_UNWIND_HACK
                supdrvNtWrapVMMR0EntryFast((PFNRT)pDevExt->pfnVMMR0EntryFast, pSession->pVM, idCpu, SUP_VMMR0_DO_NOP);
#else
                pDevExt->pfnVMMR0EntryFast(pSession->pVM, idCpu, SUP_VMMR0_DO_NOP);
#endif
                break;
            default:
                return VERR_INTERNAL_ERROR;
        }
        return VINF_SUCCESS;
    }
    return VERR_INTERNAL_ERROR;
}


/**
 * Helper for supdrvIOCtl. Check if pszStr contains any character of pszChars.
 * We would use strpbrk here if this function would be contained in the RedHat kABI white
 * list, see http://www.kerneldrivers.org/RHEL5.
 *
 * @return 1 if pszStr does contain any character of pszChars, 0 otherwise.
 * @param    pszStr     String to check
 * @param    pszChars   Character set
 */
static int supdrvCheckInvalidChar(const char *pszStr, const char *pszChars)
{
    int chCur;
    while ((chCur = *pszStr++) != '\0')
    {
        int ch;
        const char *psz = pszChars;
        while ((ch = *psz++) != '\0')
            if (ch == chCur)
                return 1;

    }
    return 0;
}


/**
 * I/O Control worker.
 *
 * @returns 0 on success.
 * @returns VERR_INVALID_PARAMETER if the request is invalid.
 *
 * @param   uIOCtl      Function number.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 * @param   pReqHdr     The request header.
 */
int VBOXCALL supdrvIOCtl(uintptr_t uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPREQHDR pReqHdr)
{
    /*
     * Validate the request.
     */
    /* this first check could probably be omitted as its also done by the OS specific code... */
    if (RT_UNLIKELY(    (pReqHdr->fFlags & SUPREQHDR_FLAGS_MAGIC_MASK) != SUPREQHDR_FLAGS_MAGIC
                    ||  pReqHdr->cbIn < sizeof(*pReqHdr)
                    ||  pReqHdr->cbOut < sizeof(*pReqHdr)))
    {
        OSDBGPRINT(("vboxdrv: Bad ioctl request header; cbIn=%#lx cbOut=%#lx fFlags=%#lx\n",
                    (long)pReqHdr->cbIn, (long)pReqHdr->cbOut, (long)pReqHdr->fFlags));
        return VERR_INVALID_PARAMETER;
    }
    if (RT_UNLIKELY(uIOCtl == SUP_IOCTL_COOKIE))
    {
        if (pReqHdr->u32Cookie != SUPCOOKIE_INITIAL_COOKIE)
        {
            OSDBGPRINT(("SUP_IOCTL_COOKIE: bad cookie %#lx\n", (long)pReqHdr->u32Cookie));
            return VERR_INVALID_PARAMETER;
        }
    }
    else if (RT_UNLIKELY(    pReqHdr->u32Cookie != pDevExt->u32Cookie
                         ||  pReqHdr->u32SessionCookie != pSession->u32Cookie))
    {
        OSDBGPRINT(("vboxdrv: bad cookie %#lx / %#lx.\n", (long)pReqHdr->u32Cookie, (long)pReqHdr->u32SessionCookie));
        return VERR_INVALID_PARAMETER;
    }

/*
 * Validation macros
 */
#define REQ_CHECK_SIZES_EX(Name, cbInExpect, cbOutExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cbIn != (cbInExpect) || pReqHdr->cbOut != (cbOutExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n", \
                        (long)pReq->Hdr.cbIn, (long)(cbInExpect), (long)pReq->Hdr.cbOut, (long)(cbOutExpect))); \
            return pReq->Hdr.rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_SIZES(Name) REQ_CHECK_SIZES_EX(Name, Name ## _SIZE_IN, Name ## _SIZE_OUT)

#define REQ_CHECK_SIZE_IN(Name, cbInExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cbIn != (cbInExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cbIn=%ld expected %ld.\n", \
                        (long)pReq->Hdr.cbIn, (long)(cbInExpect))); \
            return pReq->Hdr.rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_SIZE_OUT(Name, cbOutExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cbOut != (cbOutExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cbOut=%ld expected %ld.\n", \
                        (long)pReq->Hdr.cbOut, (long)(cbOutExpect))); \
            return pReq->Hdr.rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_EXPR(Name, expr) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            OSDBGPRINT(( #Name ": %s\n", #expr)); \
            return pReq->Hdr.rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_EXPR_FMT(expr, fmt) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            OSDBGPRINT( fmt ); \
            return pReq->Hdr.rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)


    /*
     * The switch.
     */
    switch (SUP_CTL_CODE_NO_SIZE(uIOCtl))
    {
        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_COOKIE):
        {
            PSUPCOOKIE pReq = (PSUPCOOKIE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_COOKIE);
            if (strncmp(pReq->u.In.szMagic, SUPCOOKIE_MAGIC, sizeof(pReq->u.In.szMagic)))
            {
                OSDBGPRINT(("SUP_IOCTL_COOKIE: invalid magic %.16s\n", pReq->u.In.szMagic));
                pReq->Hdr.rc = VERR_INVALID_MAGIC;
                return 0;
            }

#if 0
            /*
             * Call out to the OS specific code and let it do permission checks on the
             * client process.
             */
            if (!supdrvOSValidateClientProcess(pDevExt, pSession))
            {
                pReq->u.Out.u32Cookie         = 0xffffffff;
                pReq->u.Out.u32SessionCookie  = 0xffffffff;
                pReq->u.Out.u32SessionVersion = 0xffffffff;
                pReq->u.Out.u32DriverVersion  = SUPDRV_IOC_VERSION;
                pReq->u.Out.pSession          = NULL;
                pReq->u.Out.cFunctions        = 0;
                pReq->Hdr.rc = VERR_PERMISSION_DENIED;
                return 0;
            }
#endif

            /*
             * Match the version.
             * The current logic is very simple, match the major interface version.
             */
            if (    pReq->u.In.u32MinVersion > SUPDRV_IOC_VERSION
                ||  (pReq->u.In.u32MinVersion & 0xffff0000) != (SUPDRV_IOC_VERSION & 0xffff0000))
            {
                OSDBGPRINT(("SUP_IOCTL_COOKIE: Version mismatch. Requested: %#x  Min: %#x  Current: %#x\n",
                            pReq->u.In.u32ReqVersion, pReq->u.In.u32MinVersion, SUPDRV_IOC_VERSION));
                pReq->u.Out.u32Cookie         = 0xffffffff;
                pReq->u.Out.u32SessionCookie  = 0xffffffff;
                pReq->u.Out.u32SessionVersion = 0xffffffff;
                pReq->u.Out.u32DriverVersion  = SUPDRV_IOC_VERSION;
                pReq->u.Out.pSession          = NULL;
                pReq->u.Out.cFunctions        = 0;
                pReq->Hdr.rc = VERR_VERSION_MISMATCH;
                return 0;
            }

            /*
             * Fill in return data and be gone.
             * N.B. The first one to change SUPDRV_IOC_VERSION shall makes sure that
             *      u32SessionVersion <= u32ReqVersion!
             */
            /** @todo Somehow validate the client and negotiate a secure cookie... */
            pReq->u.Out.u32Cookie         = pDevExt->u32Cookie;
            pReq->u.Out.u32SessionCookie  = pSession->u32Cookie;
            pReq->u.Out.u32SessionVersion = SUPDRV_IOC_VERSION;
            pReq->u.Out.u32DriverVersion  = SUPDRV_IOC_VERSION;
            pReq->u.Out.pSession          = pSession;
            pReq->u.Out.cFunctions        = sizeof(g_aFunctions) / sizeof(g_aFunctions[0]);
            pReq->Hdr.rc = VINF_SUCCESS;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_QUERY_FUNCS(0)):
        {
            /* validate */
            PSUPQUERYFUNCS pReq = (PSUPQUERYFUNCS)pReqHdr;
            REQ_CHECK_SIZES_EX(SUP_IOCTL_QUERY_FUNCS, SUP_IOCTL_QUERY_FUNCS_SIZE_IN, SUP_IOCTL_QUERY_FUNCS_SIZE_OUT(RT_ELEMENTS(g_aFunctions)));

            /* execute */
            pReq->u.Out.cFunctions = RT_ELEMENTS(g_aFunctions);
            memcpy(&pReq->u.Out.aFunctions[0], g_aFunctions, sizeof(g_aFunctions));
            pReq->Hdr.rc = VINF_SUCCESS;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_IDT_INSTALL):
        {
            /* validate */
            PSUPIDTINSTALL pReq = (PSUPIDTINSTALL)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_IDT_INSTALL);

            /* execute */
            pReq->u.Out.u8Idt = 3;
            pReq->Hdr.rc = VERR_NOT_SUPPORTED;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_IDT_REMOVE):
        {
            /* validate */
            PSUPIDTREMOVE pReq = (PSUPIDTREMOVE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_IDT_REMOVE);

            /* execute */
            pReq->Hdr.rc = VERR_NOT_SUPPORTED;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_LOCK):
        {
            /* validate */
            PSUPPAGELOCK pReq = (PSUPPAGELOCK)pReqHdr;
            REQ_CHECK_SIZE_IN(SUP_IOCTL_PAGE_LOCK, SUP_IOCTL_PAGE_LOCK_SIZE_IN);
            REQ_CHECK_SIZE_OUT(SUP_IOCTL_PAGE_LOCK, SUP_IOCTL_PAGE_LOCK_SIZE_OUT(pReq->u.In.cPages));
            REQ_CHECK_EXPR(SUP_IOCTL_PAGE_LOCK, pReq->u.In.cPages > 0);
            REQ_CHECK_EXPR(SUP_IOCTL_PAGE_LOCK, pReq->u.In.pvR3 >= PAGE_SIZE);

            /* execute */
            pReq->Hdr.rc = SUPR0LockMem(pSession, pReq->u.In.pvR3, pReq->u.In.cPages, &pReq->u.Out.aPages[0]);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_UNLOCK):
        {
            /* validate */
            PSUPPAGEUNLOCK pReq = (PSUPPAGEUNLOCK)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_PAGE_UNLOCK);

            /* execute */
            pReq->Hdr.rc = SUPR0UnlockMem(pSession, pReq->u.In.pvR3);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CONT_ALLOC):
        {
            /* validate */
            PSUPCONTALLOC pReq = (PSUPCONTALLOC)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_CONT_ALLOC);

            /* execute */
            pReq->Hdr.rc = SUPR0ContAlloc(pSession, pReq->u.In.cPages, &pReq->u.Out.pvR0, &pReq->u.Out.pvR3, &pReq->u.Out.HCPhys);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CONT_FREE):
        {
            /* validate */
            PSUPCONTFREE pReq = (PSUPCONTFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_CONT_FREE);

            /* execute */
            pReq->Hdr.rc = SUPR0ContFree(pSession, (RTHCUINTPTR)pReq->u.In.pvR3);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_OPEN):
        {
            /* validate */
            PSUPLDROPEN pReq = (PSUPLDROPEN)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LDR_OPEN);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImage > 0);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImage < _1M*16);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.szName[0]);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, memchr(pReq->u.In.szName, '\0', sizeof(pReq->u.In.szName)));
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, !supdrvCheckInvalidChar(pReq->u.In.szName, ";:()[]{}/\\|&*%#@!~`\"'"));

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrOpen(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_LOAD):
        {
            /* validate */
            PSUPLDRLOAD pReq = (PSUPLDRLOAD)pReqHdr;
            REQ_CHECK_EXPR(Name, pReq->Hdr.cbIn >= sizeof(*pReq));
            REQ_CHECK_SIZES_EX(SUP_IOCTL_LDR_LOAD, SUP_IOCTL_LDR_LOAD_SIZE_IN(pReq->u.In.cbImage), SUP_IOCTL_LDR_LOAD_SIZE_OUT);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_LOAD, pReq->u.In.cSymbols <= 16384);
            REQ_CHECK_EXPR_FMT(     !pReq->u.In.cSymbols
                               ||   (   pReq->u.In.offSymbols < pReq->u.In.cbImage
                                     && pReq->u.In.offSymbols + pReq->u.In.cSymbols * sizeof(SUPLDRSYM) <= pReq->u.In.cbImage),
                               ("SUP_IOCTL_LDR_LOAD: offSymbols=%#lx cSymbols=%#lx cbImage=%#lx\n", (long)pReq->u.In.offSymbols,
                                (long)pReq->u.In.cSymbols, (long)pReq->u.In.cbImage));
            REQ_CHECK_EXPR_FMT(     !pReq->u.In.cbStrTab
                               ||   (   pReq->u.In.offStrTab < pReq->u.In.cbImage
                                     && pReq->u.In.offStrTab + pReq->u.In.cbStrTab <= pReq->u.In.cbImage
                                     && pReq->u.In.cbStrTab <= pReq->u.In.cbImage),
                               ("SUP_IOCTL_LDR_LOAD: offStrTab=%#lx cbStrTab=%#lx cbImage=%#lx\n", (long)pReq->u.In.offStrTab,
                                (long)pReq->u.In.cbStrTab, (long)pReq->u.In.cbImage));

            if (pReq->u.In.cSymbols)
            {
                uint32_t i;
                PSUPLDRSYM paSyms = (PSUPLDRSYM)&pReq->u.In.achImage[pReq->u.In.offSymbols];
                for (i = 0; i < pReq->u.In.cSymbols; i++)
                {
                    REQ_CHECK_EXPR_FMT(paSyms[i].offSymbol < pReq->u.In.cbImage,
                                       ("SUP_IOCTL_LDR_LOAD: sym #%ld: symb off %#lx (max=%#lx)\n", (long)i, (long)paSyms[i].offSymbol, (long)pReq->u.In.cbImage));
                    REQ_CHECK_EXPR_FMT(paSyms[i].offName < pReq->u.In.cbStrTab,
                                       ("SUP_IOCTL_LDR_LOAD: sym #%ld: name off %#lx (max=%#lx)\n", (long)i, (long)paSyms[i].offName, (long)pReq->u.In.cbImage));
                    REQ_CHECK_EXPR_FMT(memchr(&pReq->u.In.achImage[pReq->u.In.offStrTab + paSyms[i].offName], '\0', pReq->u.In.cbStrTab - paSyms[i].offName),
                                       ("SUP_IOCTL_LDR_LOAD: sym #%ld: unterminated name! (%#lx / %#lx)\n", (long)i, (long)paSyms[i].offName, (long)pReq->u.In.cbImage));
                }
            }

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrLoad(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_FREE):
        {
            /* validate */
            PSUPLDRFREE pReq = (PSUPLDRFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LDR_FREE);

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrFree(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_GET_SYMBOL):
        {
            /* validate */
            PSUPLDRGETSYMBOL pReq = (PSUPLDRGETSYMBOL)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LDR_GET_SYMBOL);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_GET_SYMBOL, memchr(pReq->u.In.szSymbol, '\0', sizeof(pReq->u.In.szSymbol)));

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrGetSymbol(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CALL_VMMR0(0)):
        {
            /* validate */
            PSUPCALLVMMR0 pReq = (PSUPCALLVMMR0)pReqHdr;
            Log4(("SUP_IOCTL_CALL_VMMR0: op=%u in=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                  pReq->u.In.uOperation, pReq->Hdr.cbIn, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));

            if (pReq->Hdr.cbIn == SUP_IOCTL_CALL_VMMR0_SIZE(0))
            {
                REQ_CHECK_SIZES_EX(SUP_IOCTL_CALL_VMMR0, SUP_IOCTL_CALL_VMMR0_SIZE_IN(0), SUP_IOCTL_CALL_VMMR0_SIZE_OUT(0));

                /* execute */
                if (RT_LIKELY(pDevExt->pfnVMMR0EntryEx))
#ifdef RT_WITH_W64_UNWIND_HACK
                    pReq->Hdr.rc = supdrvNtWrapVMMR0EntryEx((PFNRT)pDevExt->pfnVMMR0EntryEx, pReq->u.In.pVMR0, pReq->u.In.idCpu, pReq->u.In.uOperation, NULL, pReq->u.In.u64Arg, pSession);
#else
                    pReq->Hdr.rc = pDevExt->pfnVMMR0EntryEx(pReq->u.In.pVMR0, pReq->u.In.idCpu, pReq->u.In.uOperation, NULL, pReq->u.In.u64Arg, pSession);
#endif
                else
                    pReq->Hdr.rc = VERR_WRONG_ORDER;
            }
            else
            {
                PSUPVMMR0REQHDR pVMMReq = (PSUPVMMR0REQHDR)&pReq->abReqPkt[0];
                REQ_CHECK_EXPR_FMT(pReq->Hdr.cbIn >= SUP_IOCTL_CALL_VMMR0_SIZE(sizeof(SUPVMMR0REQHDR)),
                                   ("SUP_IOCTL_CALL_VMMR0: cbIn=%#x < %#lx\n", pReq->Hdr.cbIn, SUP_IOCTL_CALL_VMMR0_SIZE(sizeof(SUPVMMR0REQHDR))));
                REQ_CHECK_EXPR(SUP_IOCTL_CALL_VMMR0, pVMMReq->u32Magic == SUPVMMR0REQHDR_MAGIC);
                REQ_CHECK_SIZES_EX(SUP_IOCTL_CALL_VMMR0, SUP_IOCTL_CALL_VMMR0_SIZE_IN(pVMMReq->cbReq), SUP_IOCTL_CALL_VMMR0_SIZE_OUT(pVMMReq->cbReq));

                /* execute */
                if (RT_LIKELY(pDevExt->pfnVMMR0EntryEx))
#ifdef RT_WITH_W64_UNWIND_HACK
                    pReq->Hdr.rc = supdrvNtWrapVMMR0EntryEx((PFNRT)pDevExt->pfnVMMR0EntryEx, pReq->u.In.pVMR0, pReq->u.In.idCpu, pReq->u.In.uOperation, pVMMReq, pReq->u.In.u64Arg, pSession);
#else
                    pReq->Hdr.rc = pDevExt->pfnVMMR0EntryEx(pReq->u.In.pVMR0, pReq->u.In.idCpu, pReq->u.In.uOperation, pVMMReq, pReq->u.In.u64Arg, pSession);
#endif
                else
                    pReq->Hdr.rc = VERR_WRONG_ORDER;
            }

            if (    RT_FAILURE(pReq->Hdr.rc)
                &&  pReq->Hdr.rc != VERR_INTERRUPTED
                &&  pReq->Hdr.rc != VERR_TIMEOUT)
                Log(("SUP_IOCTL_CALL_VMMR0: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                     pReq->Hdr.rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
            else
                Log4(("SUP_IOCTL_CALL_VMMR0: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                      pReq->Hdr.rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_GET_PAGING_MODE):
        {
            /* validate */
            PSUPGETPAGINGMODE pReq = (PSUPGETPAGINGMODE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_GET_PAGING_MODE);

            /* execute */
            pReq->Hdr.rc = VINF_SUCCESS;
            pReq->u.Out.enmMode = SUPR0GetPagingMode();
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LOW_ALLOC):
        {
            /* validate */
            PSUPLOWALLOC pReq = (PSUPLOWALLOC)pReqHdr;
            REQ_CHECK_EXPR(SUP_IOCTL_LOW_ALLOC, pReq->Hdr.cbIn <= SUP_IOCTL_LOW_ALLOC_SIZE_IN);
            REQ_CHECK_SIZES_EX(SUP_IOCTL_LOW_ALLOC, SUP_IOCTL_LOW_ALLOC_SIZE_IN, SUP_IOCTL_LOW_ALLOC_SIZE_OUT(pReq->u.In.cPages));

            /* execute */
            pReq->Hdr.rc = SUPR0LowAlloc(pSession, pReq->u.In.cPages, &pReq->u.Out.pvR0, &pReq->u.Out.pvR3, &pReq->u.Out.aPages[0]);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LOW_FREE):
        {
            /* validate */
            PSUPLOWFREE pReq = (PSUPLOWFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LOW_FREE);

            /* execute */
            pReq->Hdr.rc = SUPR0LowFree(pSession, (RTHCUINTPTR)pReq->u.In.pvR3);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_GIP_MAP):
        {
            /* validate */
            PSUPGIPMAP pReq = (PSUPGIPMAP)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_GIP_MAP);

            /* execute */
            pReq->Hdr.rc = SUPR0GipMap(pSession, &pReq->u.Out.pGipR3, &pReq->u.Out.HCPhysGip);
            if (RT_SUCCESS(pReq->Hdr.rc))
                pReq->u.Out.pGipR0 = pDevExt->pGip;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_GIP_UNMAP):
        {
            /* validate */
            PSUPGIPUNMAP pReq = (PSUPGIPUNMAP)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_GIP_UNMAP);

            /* execute */
            pReq->Hdr.rc = SUPR0GipUnmap(pSession);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_SET_VM_FOR_FAST):
        {
            /* validate */
            PSUPSETVMFORFAST pReq = (PSUPSETVMFORFAST)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_SET_VM_FOR_FAST);
            REQ_CHECK_EXPR_FMT(     !pReq->u.In.pVMR0
                               ||   (   VALID_PTR(pReq->u.In.pVMR0)
                                     && !((uintptr_t)pReq->u.In.pVMR0 & (PAGE_SIZE - 1))),
                               ("SUP_IOCTL_SET_VM_FOR_FAST: pVMR0=%p!\n", pReq->u.In.pVMR0));
            /* execute */
            pSession->pVM = pReq->u.In.pVMR0;
            pReq->Hdr.rc = VINF_SUCCESS;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_ALLOC):
        {
            /* validate */
            PSUPPAGEALLOC pReq = (PSUPPAGEALLOC)pReqHdr;
            REQ_CHECK_EXPR(SUP_IOCTL_PAGE_ALLOC, pReq->Hdr.cbIn <= SUP_IOCTL_PAGE_ALLOC_SIZE_IN);
            REQ_CHECK_SIZES_EX(SUP_IOCTL_PAGE_ALLOC, SUP_IOCTL_PAGE_ALLOC_SIZE_IN, SUP_IOCTL_PAGE_ALLOC_SIZE_OUT(pReq->u.In.cPages));

            /* execute */
            pReq->Hdr.rc = SUPR0PageAlloc(pSession, pReq->u.In.cPages, &pReq->u.Out.pvR3, &pReq->u.Out.aPages[0]);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_ALLOC_EX):
        {
            /* validate */
            PSUPPAGEALLOCEX pReq = (PSUPPAGEALLOCEX)pReqHdr;
            REQ_CHECK_EXPR(SUP_IOCTL_PAGE_ALLOC_EX, pReq->Hdr.cbIn <= SUP_IOCTL_PAGE_ALLOC_EX_SIZE_IN);
            REQ_CHECK_SIZES_EX(SUP_IOCTL_PAGE_ALLOC_EX, SUP_IOCTL_PAGE_ALLOC_EX_SIZE_IN, SUP_IOCTL_PAGE_ALLOC_EX_SIZE_OUT(pReq->u.In.cPages));
            REQ_CHECK_EXPR_FMT(pReq->u.In.fKernelMapping || pReq->u.In.fUserMapping,
                               ("SUP_IOCTL_PAGE_ALLOC_EX: No mapping requested!\n"));
            REQ_CHECK_EXPR_FMT(pReq->u.In.fUserMapping,
                               ("SUP_IOCTL_PAGE_ALLOC_EX: Must have user mapping!\n"));
            REQ_CHECK_EXPR_FMT(!pReq->u.In.fReserved0 && !pReq->u.In.fReserved1,
                               ("SUP_IOCTL_PAGE_ALLOC_EX: fReserved0=%d fReserved1=%d\n", pReq->u.In.fReserved0, pReq->u.In.fReserved1));

            /* execute */
            pReq->Hdr.rc = SUPR0PageAllocEx(pSession, pReq->u.In.cPages, 0 /* fFlags */,
                                            pReq->u.In.fUserMapping   ? &pReq->u.Out.pvR3 : NULL,
                                            pReq->u.In.fKernelMapping ? &pReq->u.Out.pvR0 : NULL,
                                            &pReq->u.Out.aPages[0]);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_MAP_KERNEL):
        {
            /* validate */
            PSUPPAGEMAPKERNEL pReq = (PSUPPAGEMAPKERNEL)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_PAGE_MAP_KERNEL);
            REQ_CHECK_EXPR_FMT(!pReq->u.In.fFlags, ("SUP_IOCTL_PAGE_MAP_KERNEL: fFlags=%#x! MBZ\n", pReq->u.In.fFlags));
            REQ_CHECK_EXPR_FMT(!(pReq->u.In.offSub & PAGE_OFFSET_MASK), ("SUP_IOCTL_PAGE_MAP_KERNEL: offSub=%#x\n", pReq->u.In.offSub));
            REQ_CHECK_EXPR_FMT(pReq->u.In.cbSub && !(pReq->u.In.cbSub & PAGE_OFFSET_MASK),
                               ("SUP_IOCTL_PAGE_MAP_KERNEL: cbSub=%#x\n", pReq->u.In.cbSub));

            /* execute */
            pReq->Hdr.rc = SUPR0PageMapKernel(pSession, pReq->u.In.pvR3, pReq->u.In.offSub, pReq->u.In.cbSub,
                                              pReq->u.In.fFlags, &pReq->u.Out.pvR0);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_FREE):
        {
            /* validate */
            PSUPPAGEFREE pReq = (PSUPPAGEFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_PAGE_FREE);

            /* execute */
            pReq->Hdr.rc = SUPR0PageFree(pSession, pReq->u.In.pvR3);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CALL_SERVICE(0)):
        {
            /* validate */
            PSUPCALLSERVICE pReq = (PSUPCALLSERVICE)pReqHdr;
            Log4(("SUP_IOCTL_CALL_SERVICE: op=%u in=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                  pReq->u.In.uOperation, pReq->Hdr.cbIn, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));

            if (pReq->Hdr.cbIn == SUP_IOCTL_CALL_SERVICE_SIZE(0))
                REQ_CHECK_SIZES_EX(SUP_IOCTL_CALL_SERVICE, SUP_IOCTL_CALL_SERVICE_SIZE_IN(0), SUP_IOCTL_CALL_SERVICE_SIZE_OUT(0));
            else
            {
                PSUPR0SERVICEREQHDR pSrvReq = (PSUPR0SERVICEREQHDR)&pReq->abReqPkt[0];
                REQ_CHECK_EXPR_FMT(pReq->Hdr.cbIn >= SUP_IOCTL_CALL_SERVICE_SIZE(sizeof(SUPR0SERVICEREQHDR)),
                                   ("SUP_IOCTL_CALL_SERVICE: cbIn=%#x < %#lx\n", pReq->Hdr.cbIn, SUP_IOCTL_CALL_SERVICE_SIZE(sizeof(SUPR0SERVICEREQHDR))));
                REQ_CHECK_EXPR(SUP_IOCTL_CALL_SERVICE, pSrvReq->u32Magic == SUPR0SERVICEREQHDR_MAGIC);
                REQ_CHECK_SIZES_EX(SUP_IOCTL_CALL_SERVICE, SUP_IOCTL_CALL_SERVICE_SIZE_IN(pSrvReq->cbReq), SUP_IOCTL_CALL_SERVICE_SIZE_OUT(pSrvReq->cbReq));
            }
            REQ_CHECK_EXPR(SUP_IOCTL_CALL_SERVICE, memchr(pReq->u.In.szName, '\0', sizeof(pReq->u.In.szName)));

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_CallServiceModule(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LOGGER_SETTINGS(0)):
        {
            /* validate */
            PSUPLOGGERSETTINGS pReq = (PSUPLOGGERSETTINGS)pReqHdr;
            size_t cbStrTab;
            REQ_CHECK_SIZE_OUT(SUP_IOCTL_LOGGER_SETTINGS, SUP_IOCTL_LOGGER_SETTINGS_SIZE_OUT);
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->Hdr.cbIn >= SUP_IOCTL_LOGGER_SETTINGS_SIZE_IN(1));
            cbStrTab = pReq->Hdr.cbIn - SUP_IOCTL_LOGGER_SETTINGS_SIZE_IN(0);
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->u.In.offGroups      < cbStrTab);
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->u.In.offFlags       < cbStrTab);
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->u.In.offDestination < cbStrTab);
            REQ_CHECK_EXPR_FMT(pReq->u.In.szStrings[cbStrTab - 1] == '\0',
                               ("SUP_IOCTL_LOGGER_SETTINGS: cbIn=%#x cbStrTab=%#zx LastChar=%d\n",
                                pReq->Hdr.cbIn, cbStrTab, pReq->u.In.szStrings[cbStrTab - 1]));
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->u.In.fWhich <= SUPLOGGERSETTINGS_WHICH_RELEASE);
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->u.In.fWhat  <= SUPLOGGERSETTINGS_WHAT_DESTROY);

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LoggerSettings(pDevExt, pSession, pReq);
            return 0;
        }

        default:
            Log(("Unknown IOCTL %#lx\n", (long)uIOCtl));
            break;
    }
    return SUPDRV_ERR_GENERAL_FAILURE;
}


/**
 * Inter-Driver Communcation (IDC) worker.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_PARAMETER if the request is invalid.
 * @retval  VERR_NOT_SUPPORTED if the request isn't supported.
 *
 * @param   uReq        The request (function) code.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 * @param   pReqHdr     The request header.
 */
int VBOXCALL supdrvIDC(uintptr_t uReq, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPDRVIDCREQHDR pReqHdr)
{
    /*
     * The OS specific code has already validated the pSession
     * pointer, and the request size being greater or equal to
     * size of the header.
     *
     * So, just check that pSession is a kernel context session.
     */
    if (RT_UNLIKELY(    pSession
                    &&  pSession->R0Process != NIL_RTR0PROCESS))
        return VERR_INVALID_PARAMETER;

/*
 * Validation macro.
 */
#define REQ_CHECK_IDC_SIZE(Name, cbExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cb != (cbExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cb=%ld expected %ld.\n", \
                        (long)pReqHdr->cb, (long)(cbExpect))); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

    switch (uReq)
    {
        case SUPDRV_IDC_REQ_CONNECT:
        {
            PSUPDRVIDCREQCONNECT pReq = (PSUPDRVIDCREQCONNECT)pReqHdr;
            REQ_CHECK_IDC_SIZE(SUPDRV_IDC_REQ_CONNECT, sizeof(*pReq));

            /*
             * Validate the cookie and other input.
             */
            if (pReq->Hdr.pSession != NULL)
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: pSession=%p expected NULL!\n", pReq->Hdr.pSession));
                return pReqHdr->rc = VERR_INVALID_PARAMETER;
            }
            if (pReq->u.In.u32MagicCookie != SUPDRVIDCREQ_CONNECT_MAGIC_COOKIE)
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: u32MagicCookie=%#x expected %#x!\n",
                            (unsigned)pReq->u.In.u32MagicCookie, (unsigned)SUPDRVIDCREQ_CONNECT_MAGIC_COOKIE));
                return pReqHdr->rc = VERR_INVALID_PARAMETER;
            }
            if (    pReq->u.In.uMinVersion > pReq->u.In.uReqVersion
                ||  (pReq->u.In.uMinVersion & UINT32_C(0xffff0000)) != (pReq->u.In.uReqVersion & UINT32_C(0xffff0000)))
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: uMinVersion=%#x uMaxVersion=%#x doesn't match!\n",
                            pReq->u.In.uMinVersion, pReq->u.In.uReqVersion));
                return pReqHdr->rc = VERR_INVALID_PARAMETER;
            }

            /*
             * Match the version.
             * The current logic is very simple, match the major interface version.
             */
            if (    pReq->u.In.uMinVersion > SUPDRV_IDC_VERSION
                ||  (pReq->u.In.uMinVersion & 0xffff0000) != (SUPDRV_IDC_VERSION & 0xffff0000))
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: Version mismatch. Requested: %#x  Min: %#x  Current: %#x\n",
                            pReq->u.In.uReqVersion, pReq->u.In.uMinVersion, (unsigned)SUPDRV_IDC_VERSION));
                pReq->u.Out.pSession        = NULL;
                pReq->u.Out.uSessionVersion = 0xffffffff;
                pReq->u.Out.uDriverVersion  = SUPDRV_IDC_VERSION;
                pReq->u.Out.uDriverRevision = VBOX_SVN_REV;
                pReq->Hdr.rc = VERR_VERSION_MISMATCH;
                return VINF_SUCCESS;
            }

            pReq->u.Out.pSession        = NULL;
            pReq->u.Out.uSessionVersion = SUPDRV_IDC_VERSION;
            pReq->u.Out.uDriverVersion  = SUPDRV_IDC_VERSION;
            pReq->u.Out.uDriverRevision = VBOX_SVN_REV;

            /*
             * On NT we will already have a session associated with the
             * client, just like with the SUP_IOCTL_COOKIE request, while
             * the other doesn't.
             */
#ifdef RT_OS_WINDOWS
            pReq->Hdr.rc = VINF_SUCCESS;
#else
            AssertReturn(!pSession, VERR_INTERNAL_ERROR);
            pReq->Hdr.rc = supdrvCreateSession(pDevExt, false /* fUser */, &pSession);
            if (RT_FAILURE(pReq->Hdr.rc))
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: failed to create session, rc=%d\n", pReq->Hdr.rc));
                return VINF_SUCCESS;
            }
#endif

            pReq->u.Out.pSession = pSession;
            pReq->Hdr.pSession = pSession;

            return VINF_SUCCESS;
        }

        case SUPDRV_IDC_REQ_DISCONNECT:
        {
            REQ_CHECK_IDC_SIZE(SUPDRV_IDC_REQ_DISCONNECT, sizeof(*pReqHdr));

#ifdef RT_OS_WINDOWS
            /* Windows will destroy the session when the file object is destroyed. */
#else
            supdrvCloseSession(pDevExt, pSession);
#endif
            return pReqHdr->rc = VINF_SUCCESS;
        }

        case SUPDRV_IDC_REQ_GET_SYMBOL:
        {
            PSUPDRVIDCREQGETSYM pReq = (PSUPDRVIDCREQGETSYM)pReqHdr;
            REQ_CHECK_IDC_SIZE(SUPDRV_IDC_REQ_GET_SYMBOL, sizeof(*pReq));

            pReq->Hdr.rc = supdrvIDC_LdrGetSymbol(pDevExt, pSession, pReq);
            return VINF_SUCCESS;
        }

        case SUPDRV_IDC_REQ_COMPONENT_REGISTER_FACTORY:
        {
            PSUPDRVIDCREQCOMPREGFACTORY pReq = (PSUPDRVIDCREQCOMPREGFACTORY)pReqHdr;
            REQ_CHECK_IDC_SIZE(SUPDRV_IDC_REQ_COMPONENT_REGISTER_FACTORY, sizeof(*pReq));

            pReq->Hdr.rc = SUPR0ComponentRegisterFactory(pSession, pReq->u.In.pFactory);
            return VINF_SUCCESS;
        }

        case SUPDRV_IDC_REQ_COMPONENT_DEREGISTER_FACTORY:
        {
            PSUPDRVIDCREQCOMPDEREGFACTORY pReq = (PSUPDRVIDCREQCOMPDEREGFACTORY)pReqHdr;
            REQ_CHECK_IDC_SIZE(SUPDRV_IDC_REQ_COMPONENT_DEREGISTER_FACTORY, sizeof(*pReq));

            pReq->Hdr.rc = SUPR0ComponentDeregisterFactory(pSession, pReq->u.In.pFactory);
            return VINF_SUCCESS;
        }

        default:
            Log(("Unknown IDC %#lx\n", (long)uReq));
            break;
    }

#undef REQ_CHECK_IDC_SIZE
    return VERR_NOT_SUPPORTED;
}


/**
 * Register a object for reference counting.
 * The object is registered with one reference in the specified session.
 *
 * @returns Unique identifier on success (pointer).
 *          All future reference must use this identifier.
 * @returns NULL on failure.
 * @param   pfnDestructor   The destructore function which will be called when the reference count reaches 0.
 * @param   pvUser1         The first user argument.
 * @param   pvUser2         The second user argument.
 */
SUPR0DECL(void *) SUPR0ObjRegister(PSUPDRVSESSION pSession, SUPDRVOBJTYPE enmType, PFNSUPDRVDESTRUCTOR pfnDestructor, void *pvUser1, void *pvUser2)
{
    RTSPINLOCKTMP   SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    PSUPDRVDEVEXT   pDevExt     = pSession->pDevExt;
    PSUPDRVOBJ      pObj;
    PSUPDRVUSAGE    pUsage;

    /*
     * Validate the input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), NULL);
    AssertReturn(enmType > SUPDRVOBJTYPE_INVALID && enmType < SUPDRVOBJTYPE_END, NULL);
    AssertPtrReturn(pfnDestructor, NULL);

    /*
     * Allocate and initialize the object.
     */
    pObj = (PSUPDRVOBJ)RTMemAlloc(sizeof(*pObj));
    if (!pObj)
        return NULL;
    pObj->u32Magic      = SUPDRVOBJ_MAGIC;
    pObj->enmType       = enmType;
    pObj->pNext         = NULL;
    pObj->cUsage        = 1;
    pObj->pfnDestructor = pfnDestructor;
    pObj->pvUser1       = pvUser1;
    pObj->pvUser2       = pvUser2;
    pObj->CreatorUid    = pSession->Uid;
    pObj->CreatorGid    = pSession->Gid;
    pObj->CreatorProcess= pSession->Process;
    supdrvOSObjInitCreator(pObj, pSession);

    /*
     * Allocate the usage record.
     * (We keep freed usage records around to simplify SUPR0ObjAddRefEx().)
     */
    RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);

    pUsage = pDevExt->pUsageFree;
    if (pUsage)
        pDevExt->pUsageFree = pUsage->pNext;
    else
    {
        RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);
        pUsage = (PSUPDRVUSAGE)RTMemAlloc(sizeof(*pUsage));
        if (!pUsage)
        {
            RTMemFree(pObj);
            return NULL;
        }
        RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);
    }

    /*
     * Insert the object and create the session usage record.
     */
    /* The object. */
    pObj->pNext         = pDevExt->pObjs;
    pDevExt->pObjs      = pObj;

    /* The session record. */
    pUsage->cUsage      = 1;
    pUsage->pObj        = pObj;
    pUsage->pNext       = pSession->pUsage;
    /* Log2(("SUPR0ObjRegister: pUsage=%p:{.pObj=%p, .pNext=%p}\n", pUsage, pUsage->pObj, pUsage->pNext)); */
    pSession->pUsage    = pUsage;

    RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);

    Log(("SUPR0ObjRegister: returns %p (pvUser1=%p, pvUser=%p)\n", pObj, pvUser1, pvUser2));
    return pObj;
}


/**
 * Increment the reference counter for the object associating the reference
 * with the specified session.
 *
 * @returns IPRT status code.
 * @param   pvObj           The identifier returned by SUPR0ObjRegister().
 * @param   pSession        The session which is referencing the object.
 *
 * @remarks The caller should not own any spinlocks and must carefully protect
 *          itself against potential race with the destructor so freed memory
 *          isn't accessed here.
 */
SUPR0DECL(int) SUPR0ObjAddRef(void *pvObj, PSUPDRVSESSION pSession)
{
    return SUPR0ObjAddRefEx(pvObj, pSession, false /* fNoBlocking */);
}


/**
 * Increment the reference counter for the object associating the reference
 * with the specified session.
 *
 * @returns IPRT status code.
 * @retval  VERR_TRY_AGAIN if fNoBlocking was set and a new usage record
 *          couldn't be allocated. (If you see this you're not doing the right
 *          thing and it won't ever work reliably.)
 *
 * @param   pvObj           The identifier returned by SUPR0ObjRegister().
 * @param   pSession        The session which is referencing the object.
 * @param   fNoBlocking     Set if it's not OK to block. Never try to make the
 *                          first reference to an object in a session with this
 *                          argument set.
 *
 * @remarks The caller should not own any spinlocks and must carefully protect
 *          itself against potential race with the destructor so freed memory
 *          isn't accessed here.
 */
SUPR0DECL(int) SUPR0ObjAddRefEx(void *pvObj, PSUPDRVSESSION pSession, bool fNoBlocking)
{
    RTSPINLOCKTMP   SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    PSUPDRVDEVEXT   pDevExt     = pSession->pDevExt;
    PSUPDRVOBJ      pObj        = (PSUPDRVOBJ)pvObj;
    int             rc          = VINF_SUCCESS;
    PSUPDRVUSAGE    pUsagePre;
    PSUPDRVUSAGE    pUsage;

    /*
     * Validate the input.
     * Be ready for the destruction race (someone might be stuck in the
     * destructor waiting a lock we own).
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pObj, VERR_INVALID_POINTER);
    AssertMsgReturn(pObj->u32Magic == SUPDRVOBJ_MAGIC || pObj->u32Magic == SUPDRVOBJ_MAGIC_DEAD,
                    ("Invalid pvObj=%p magic=%#x (expected %#x or %#x)\n", pvObj, pObj->u32Magic, SUPDRVOBJ_MAGIC, SUPDRVOBJ_MAGIC_DEAD),
                    VERR_INVALID_PARAMETER);

    RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);

    if (RT_UNLIKELY(pObj->u32Magic != SUPDRVOBJ_MAGIC))
    {
        RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);

        AssertMsgFailed(("pvObj=%p magic=%#x\n", pvObj, pObj->u32Magic));
        return VERR_WRONG_ORDER;
    }

    /*
     * Preallocate the usage record if we can.
     */
    pUsagePre = pDevExt->pUsageFree;
    if (pUsagePre)
        pDevExt->pUsageFree = pUsagePre->pNext;
    else if (!fNoBlocking)
    {
        RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);
        pUsagePre = (PSUPDRVUSAGE)RTMemAlloc(sizeof(*pUsagePre));
        if (!pUsagePre)
            return VERR_NO_MEMORY;

        RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);
        if (RT_UNLIKELY(pObj->u32Magic != SUPDRVOBJ_MAGIC))
        {
            RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);

            AssertMsgFailed(("pvObj=%p magic=%#x\n", pvObj, pObj->u32Magic));
            return VERR_WRONG_ORDER;
        }
    }

    /*
     * Reference the object.
     */
    pObj->cUsage++;

    /*
     * Look for the session record.
     */
    for (pUsage = pSession->pUsage; pUsage; pUsage = pUsage->pNext)
    {
        /*Log(("SUPR0AddRef: pUsage=%p:{.pObj=%p, .pNext=%p}\n", pUsage, pUsage->pObj, pUsage->pNext));*/
        if (pUsage->pObj == pObj)
            break;
    }
    if (pUsage)
        pUsage->cUsage++;
    else if (pUsagePre)
    {
        /* create a new session record. */
        pUsagePre->cUsage   = 1;
        pUsagePre->pObj     = pObj;
        pUsagePre->pNext    = pSession->pUsage;
        pSession->pUsage    = pUsagePre;
        /*Log(("SUPR0AddRef: pUsagePre=%p:{.pObj=%p, .pNext=%p}\n", pUsagePre, pUsagePre->pObj, pUsagePre->pNext));*/

        pUsagePre = NULL;
    }
    else
    {
        pObj->cUsage--;
        rc = VERR_TRY_AGAIN;
    }

    /*
     * Put any unused usage record into the free list..
     */
    if (pUsagePre)
    {
        pUsagePre->pNext = pDevExt->pUsageFree;
        pDevExt->pUsageFree = pUsagePre;
    }

    RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);

    return rc;
}


/**
 * Decrement / destroy a reference counter record for an object.
 *
 * The object is uniquely identified by pfnDestructor+pvUser1+pvUser2.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS if not destroyed.
 * @retval  VINF_OBJECT_DESTROYED if it's destroyed by this release call.
 * @retval  VERR_INVALID_PARAMETER if the object isn't valid. Will assert in
 *          string builds.
 *
 * @param   pvObj           The identifier returned by SUPR0ObjRegister().
 * @param   pSession        The session which is referencing the object.
 */
SUPR0DECL(int) SUPR0ObjRelease(void *pvObj, PSUPDRVSESSION pSession)
{
    RTSPINLOCKTMP       SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    PSUPDRVDEVEXT       pDevExt     = pSession->pDevExt;
    PSUPDRVOBJ          pObj        = (PSUPDRVOBJ)pvObj;
    int                 rc          = VERR_INVALID_PARAMETER;
    PSUPDRVUSAGE        pUsage;
    PSUPDRVUSAGE        pUsagePrev;

    /*
     * Validate the input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pObj) && pObj->u32Magic == SUPDRVOBJ_MAGIC,
                    ("Invalid pvObj=%p magic=%#x (exepcted %#x)\n", pvObj, pObj ? pObj->u32Magic : 0, SUPDRVOBJ_MAGIC),
                    VERR_INVALID_PARAMETER);

    /*
     * Acquire the spinlock and look for the usage record.
     */
    RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);

    for (pUsagePrev = NULL, pUsage = pSession->pUsage;
         pUsage;
         pUsagePrev = pUsage, pUsage = pUsage->pNext)
    {
        /*Log2(("SUPR0ObjRelease: pUsage=%p:{.pObj=%p, .pNext=%p}\n", pUsage, pUsage->pObj, pUsage->pNext));*/
        if (pUsage->pObj == pObj)
        {
            rc = VINF_SUCCESS;
            AssertMsg(pUsage->cUsage >= 1 && pObj->cUsage >= pUsage->cUsage, ("glob %d; sess %d\n", pObj->cUsage, pUsage->cUsage));
            if (pUsage->cUsage > 1)
            {
                pObj->cUsage--;
                pUsage->cUsage--;
            }
            else
            {
                /*
                 * Free the session record.
                 */
                if (pUsagePrev)
                    pUsagePrev->pNext = pUsage->pNext;
                else
                    pSession->pUsage = pUsage->pNext;
                pUsage->pNext = pDevExt->pUsageFree;
                pDevExt->pUsageFree = pUsage;

                /* What about the object? */
                if (pObj->cUsage > 1)
                    pObj->cUsage--;
                else
                {
                    /*
                     * Object is to be destroyed, unlink it.
                     */
                    pObj->u32Magic = SUPDRVOBJ_MAGIC_DEAD;
                    rc = VINF_OBJECT_DESTROYED;
                    if (pDevExt->pObjs == pObj)
                        pDevExt->pObjs = pObj->pNext;
                    else
                    {
                        PSUPDRVOBJ pObjPrev;
                        for (pObjPrev = pDevExt->pObjs; pObjPrev; pObjPrev = pObjPrev->pNext)
                            if (pObjPrev->pNext == pObj)
                            {
                                pObjPrev->pNext = pObj->pNext;
                                break;
                            }
                        Assert(pObjPrev);
                    }
                }
            }
            break;
        }
    }

    RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);

    /*
     * Call the destructor and free the object if required.
     */
    if (rc == VINF_OBJECT_DESTROYED)
    {
        Log(("SUPR0ObjRelease: destroying %p/%d (%p/%p) cpid=%RTproc pid=%RTproc dtor=%p\n",
             pObj, pObj->enmType, pObj->pvUser1, pObj->pvUser2, pObj->CreatorProcess, RTProcSelf(), pObj->pfnDestructor));
        if (pObj->pfnDestructor)
#ifdef RT_WITH_W64_UNWIND_HACK
            supdrvNtWrapObjDestructor((PFNRT)pObj->pfnDestructor, pObj, pObj->pvUser1, pObj->pvUser2);
#else
            pObj->pfnDestructor(pObj, pObj->pvUser1, pObj->pvUser2);
#endif
        RTMemFree(pObj);
    }

    AssertMsg(pUsage, ("pvObj=%p\n", pvObj));
    return rc;
}


/**
 * Verifies that the current process can access the specified object.
 *
 * @returns The following IPRT status code:
 * @retval  VINF_SUCCESS if access was granted.
 * @retval  VERR_PERMISSION_DENIED if denied access.
 * @retval  VERR_INVALID_PARAMETER if invalid parameter.
 *
 * @param   pvObj           The identifier returned by SUPR0ObjRegister().
 * @param   pSession        The session which wishes to access the object.
 * @param   pszObjName      Object string name. This is optional and depends on the object type.
 *
 * @remark  The caller is responsible for making sure the object isn't removed while
 *          we're inside this function. If uncertain about this, just call AddRef before calling us.
 */
SUPR0DECL(int) SUPR0ObjVerifyAccess(void *pvObj, PSUPDRVSESSION pSession, const char *pszObjName)
{
    PSUPDRVOBJ  pObj = (PSUPDRVOBJ)pvObj;
    int         rc;

    /*
     * Validate the input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pObj) && pObj->u32Magic == SUPDRVOBJ_MAGIC,
                    ("Invalid pvObj=%p magic=%#x (exepcted %#x)\n", pvObj, pObj ? pObj->u32Magic : 0, SUPDRVOBJ_MAGIC),
                    VERR_INVALID_PARAMETER);

    /*
     * Check access. (returns true if a decision has been made.)
     */
    rc = VERR_INTERNAL_ERROR;
    if (supdrvOSObjCanAccess(pObj, pSession, pszObjName, &rc))
        return rc;

    /*
     * Default policy is to allow the user to access his own
     * stuff but nothing else.
     */
    if (pObj->CreatorUid == pSession->Uid)
        return VINF_SUCCESS;
    return VERR_PERMISSION_DENIED;
}


/**
 * Lock pages.
 *
 * @returns IPRT status code.
 * @param   pSession    Session to which the locked memory should be associated.
 * @param   pvR3        Start of the memory range to lock.
 *                      This must be page aligned.
 * @param   cPages      Number of pages to lock.
 * @param   paPages     Where to put the physical addresses of locked memory.
 */
SUPR0DECL(int) SUPR0LockMem(PSUPDRVSESSION pSession, RTR3PTR pvR3, uint32_t cPages, PRTHCPHYS paPages)
{
    int             rc;
    SUPDRVMEMREF    Mem = { NIL_RTR0MEMOBJ, NIL_RTR0MEMOBJ, MEMREF_TYPE_UNUSED };
    const size_t    cb = (size_t)cPages << PAGE_SHIFT;
    LogFlow(("SUPR0LockMem: pSession=%p pvR3=%p cPages=%d paPages=%p\n", pSession, (void *)pvR3, cPages, paPages));

    /*
     * Verify input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(paPages, VERR_INVALID_PARAMETER);
    if (    RT_ALIGN_R3PT(pvR3, PAGE_SIZE, RTR3PTR) != pvR3
        ||  !pvR3)
    {
        Log(("pvR3 (%p) must be page aligned and not NULL!\n", (void *)pvR3));
        return VERR_INVALID_PARAMETER;
    }

#ifdef RT_OS_WINDOWS /* A temporary hack for windows, will be removed once all ring-3 code has been cleaned up. */
    /* First check if we allocated it using SUPPageAlloc; if so then we don't need to lock it again */
    rc = supdrvPageGetPhys(pSession, pvR3, cPages, paPages);
    if (RT_SUCCESS(rc))
        return rc;
#endif

    /*
     * Let IPRT do the job.
     */
    Mem.eType = MEMREF_TYPE_LOCKED;
    rc = RTR0MemObjLockUser(&Mem.MemObj, pvR3, cb, RTR0ProcHandleSelf());
    if (RT_SUCCESS(rc))
    {
        uint32_t iPage = cPages;
        AssertMsg(RTR0MemObjAddressR3(Mem.MemObj) == pvR3, ("%p == %p\n", RTR0MemObjAddressR3(Mem.MemObj), pvR3));
        AssertMsg(RTR0MemObjSize(Mem.MemObj) == cb, ("%x == %x\n", RTR0MemObjSize(Mem.MemObj), cb));

        while (iPage-- > 0)
        {
            paPages[iPage] = RTR0MemObjGetPagePhysAddr(Mem.MemObj, iPage);
            if (RT_UNLIKELY(paPages[iPage] == NIL_RTCCPHYS))
            {
                AssertMsgFailed(("iPage=%d\n", iPage));
                rc = VERR_INTERNAL_ERROR;
                break;
            }
        }
        if (RT_SUCCESS(rc))
            rc = supdrvMemAdd(&Mem, pSession);
        if (RT_FAILURE(rc))
        {
            int rc2 = RTR0MemObjFree(Mem.MemObj, false);
            AssertRC(rc2);
        }
    }

    return rc;
}


/**
 * Unlocks the memory pointed to by pv.
 *
 * @returns IPRT status code.
 * @param   pSession    Session to which the memory was locked.
 * @param   pvR3        Memory to unlock.
 */
SUPR0DECL(int) SUPR0UnlockMem(PSUPDRVSESSION pSession, RTR3PTR pvR3)
{
    LogFlow(("SUPR0UnlockMem: pSession=%p pvR3=%p\n", pSession, (void *)pvR3));
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
#ifdef RT_OS_WINDOWS
    /*
     * Temporary hack for windows - SUPR0PageFree will unlock SUPR0PageAlloc
     * allocations; ignore this call.
     */
    if (supdrvPageWasLockedByPageAlloc(pSession, pvR3))
    {
        LogFlow(("Page will be unlocked in SUPR0PageFree -> ignore\n"));
        return VINF_SUCCESS;
    }
#endif
    return supdrvMemRelease(pSession, (RTHCUINTPTR)pvR3, MEMREF_TYPE_LOCKED);
}


/**
 * Allocates a chunk of page aligned memory with contiguous and fixed physical
 * backing.
 *
 * @returns IPRT status code.
 * @param   pSession    Session data.
 * @param   cPages      Number of pages to allocate.
 * @param   ppvR0       Where to put the address of Ring-0 mapping the allocated memory.
 * @param   ppvR3       Where to put the address of Ring-3 mapping the allocated memory.
 * @param   pHCPhys     Where to put the physical address of allocated memory.
 */
SUPR0DECL(int) SUPR0ContAlloc(PSUPDRVSESSION pSession, uint32_t cPages, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PRTHCPHYS pHCPhys)
{
    int             rc;
    SUPDRVMEMREF    Mem = { NIL_RTR0MEMOBJ, NIL_RTR0MEMOBJ, MEMREF_TYPE_UNUSED };
    LogFlow(("SUPR0ContAlloc: pSession=%p cPages=%d ppvR0=%p ppvR3=%p pHCPhys=%p\n", pSession, cPages, ppvR0, ppvR3, pHCPhys));

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    if (!ppvR3 || !ppvR0 || !pHCPhys)
    {
        Log(("Null pointer. All of these should be set: pSession=%p ppvR0=%p ppvR3=%p pHCPhys=%p\n",
             pSession, ppvR0, ppvR3, pHCPhys));
        return VERR_INVALID_PARAMETER;

    }
    if (cPages < 1 || cPages >= 256)
    {
        Log(("Illegal request cPages=%d, must be greater than 0 and smaller than 256.\n", cPages));
        return VERR_PAGE_COUNT_OUT_OF_RANGE;
    }

    /*
     * Let IPRT do the job.
     */
    rc = RTR0MemObjAllocCont(&Mem.MemObj, cPages << PAGE_SHIFT, true /* executable R0 mapping */);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        rc = RTR0MemObjMapUser(&Mem.MapObjR3, Mem.MemObj, (RTR3PTR)-1, 0,
                               RTMEM_PROT_EXEC | RTMEM_PROT_WRITE | RTMEM_PROT_READ, RTR0ProcHandleSelf());
        if (RT_SUCCESS(rc))
        {
            Mem.eType = MEMREF_TYPE_CONT;
            rc = supdrvMemAdd(&Mem, pSession);
            if (!rc)
            {
                *ppvR0 = RTR0MemObjAddress(Mem.MemObj);
                *ppvR3 = RTR0MemObjAddressR3(Mem.MapObjR3);
                *pHCPhys = RTR0MemObjGetPagePhysAddr(Mem.MemObj, 0);
                return 0;
            }

            rc2 = RTR0MemObjFree(Mem.MapObjR3, false);
            AssertRC(rc2);
        }
        rc2 = RTR0MemObjFree(Mem.MemObj, false);
        AssertRC(rc2);
    }

    return rc;
}


/**
 * Frees memory allocated using SUPR0ContAlloc().
 *
 * @returns IPRT status code.
 * @param   pSession    The session to which the memory was allocated.
 * @param   uPtr        Pointer to the memory (ring-3 or ring-0).
 */
SUPR0DECL(int) SUPR0ContFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr)
{
    LogFlow(("SUPR0ContFree: pSession=%p uPtr=%p\n", pSession, (void *)uPtr));
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    return supdrvMemRelease(pSession, uPtr, MEMREF_TYPE_CONT);
}


/**
 * Allocates a chunk of page aligned memory with fixed physical backing below 4GB.
 *
 * The memory isn't zeroed.
 *
 * @returns IPRT status code.
 * @param   pSession    Session data.
 * @param   cPages      Number of pages to allocate.
 * @param   ppvR0       Where to put the address of Ring-0 mapping of the allocated memory.
 * @param   ppvR3       Where to put the address of Ring-3 mapping of the allocated memory.
 * @param   paPages     Where to put the physical addresses of allocated memory.
 */
SUPR0DECL(int) SUPR0LowAlloc(PSUPDRVSESSION pSession, uint32_t cPages, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PRTHCPHYS paPages)
{
    unsigned        iPage;
    int             rc;
    SUPDRVMEMREF    Mem = { NIL_RTR0MEMOBJ, NIL_RTR0MEMOBJ, MEMREF_TYPE_UNUSED };
    LogFlow(("SUPR0LowAlloc: pSession=%p cPages=%d ppvR3=%p ppvR0=%p paPages=%p\n", pSession, cPages, ppvR3, ppvR0, paPages));

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    if (!ppvR3 || !ppvR0 || !paPages)
    {
        Log(("Null pointer. All of these should be set: pSession=%p ppvR3=%p ppvR0=%p paPages=%p\n",
             pSession, ppvR3, ppvR0, paPages));
        return VERR_INVALID_PARAMETER;

    }
    if (cPages < 1 || cPages >= 256)
    {
        Log(("Illegal request cPages=%d, must be greater than 0 and smaller than 256.\n", cPages));
        return VERR_PAGE_COUNT_OUT_OF_RANGE;
    }

    /*
     * Let IPRT do the work.
     */
    rc = RTR0MemObjAllocLow(&Mem.MemObj, cPages << PAGE_SHIFT, true /* executable ring-0 mapping */);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        rc = RTR0MemObjMapUser(&Mem.MapObjR3, Mem.MemObj, (RTR3PTR)-1, 0,
                               RTMEM_PROT_EXEC | RTMEM_PROT_WRITE | RTMEM_PROT_READ, RTR0ProcHandleSelf());
        if (RT_SUCCESS(rc))
        {
            Mem.eType = MEMREF_TYPE_LOW;
            rc = supdrvMemAdd(&Mem, pSession);
            if (!rc)
            {
                for (iPage = 0; iPage < cPages; iPage++)
                {
                    paPages[iPage] = RTR0MemObjGetPagePhysAddr(Mem.MemObj, iPage);
                    AssertMsg(!(paPages[iPage] & (PAGE_SIZE - 1)), ("iPage=%d Phys=%RHp\n", paPages[iPage]));
                }
                *ppvR0 = RTR0MemObjAddress(Mem.MemObj);
                *ppvR3 = RTR0MemObjAddressR3(Mem.MapObjR3);
                return 0;
            }

            rc2 = RTR0MemObjFree(Mem.MapObjR3, false);
            AssertRC(rc2);
        }

        rc2 = RTR0MemObjFree(Mem.MemObj, false);
        AssertRC(rc2);
    }

    return rc;
}


/**
 * Frees memory allocated using SUPR0LowAlloc().
 *
 * @returns IPRT status code.
 * @param   pSession    The session to which the memory was allocated.
 * @param   uPtr        Pointer to the memory (ring-3 or ring-0).
 */
SUPR0DECL(int) SUPR0LowFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr)
{
    LogFlow(("SUPR0LowFree: pSession=%p uPtr=%p\n", pSession, (void *)uPtr));
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    return supdrvMemRelease(pSession, uPtr, MEMREF_TYPE_LOW);
}



/**
 * Allocates a chunk of memory with both R0 and R3 mappings.
 * The memory is fixed and it's possible to query the physical addresses using SUPR0MemGetPhys().
 *
 * @returns IPRT status code.
 * @param   pSession    The session to associated the allocation with.
 * @param   cb          Number of bytes to allocate.
 * @param   ppvR0       Where to store the address of the Ring-0 mapping.
 * @param   ppvR3       Where to store the address of the Ring-3 mapping.
 */
SUPR0DECL(int) SUPR0MemAlloc(PSUPDRVSESSION pSession, uint32_t cb, PRTR0PTR ppvR0, PRTR3PTR ppvR3)
{
    int             rc;
    SUPDRVMEMREF    Mem = { NIL_RTR0MEMOBJ, NIL_RTR0MEMOBJ, MEMREF_TYPE_UNUSED };
    LogFlow(("SUPR0MemAlloc: pSession=%p cb=%d ppvR0=%p ppvR3=%p\n", pSession, cb, ppvR0, ppvR3));

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvR0, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvR3, VERR_INVALID_POINTER);
    if (cb < 1 || cb >= _4M)
    {
        Log(("Illegal request cb=%u; must be greater than 0 and smaller than 4MB.\n", cb));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Let IPRT do the work.
     */
    rc = RTR0MemObjAllocPage(&Mem.MemObj, cb, true /* executable ring-0 mapping */);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        rc = RTR0MemObjMapUser(&Mem.MapObjR3, Mem.MemObj, (RTR3PTR)-1, 0,
                               RTMEM_PROT_EXEC | RTMEM_PROT_WRITE | RTMEM_PROT_READ, RTR0ProcHandleSelf());
        if (RT_SUCCESS(rc))
        {
            Mem.eType = MEMREF_TYPE_MEM;
            rc = supdrvMemAdd(&Mem, pSession);
            if (!rc)
            {
                *ppvR0 = RTR0MemObjAddress(Mem.MemObj);
                *ppvR3 = RTR0MemObjAddressR3(Mem.MapObjR3);
                return VINF_SUCCESS;
            }

            rc2 = RTR0MemObjFree(Mem.MapObjR3, false);
            AssertRC(rc2);
        }

        rc2 = RTR0MemObjFree(Mem.MemObj, false);
        AssertRC(rc2);
    }

    return rc;
}


/**
 * Get the physical addresses of memory allocated using SUPR0MemAlloc().
 *
 * @returns IPRT status code.
 * @param   pSession        The session to which the memory was allocated.
 * @param   uPtr            The Ring-0 or Ring-3 address returned by SUPR0MemAlloc().
 * @param   paPages         Where to store the physical addresses.
 */
SUPR0DECL(int) SUPR0MemGetPhys(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, PSUPPAGE paPages) /** @todo switch this bugger to RTHCPHYS */
{
    PSUPDRVBUNDLE pBundle;
    RTSPINLOCKTMP SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    LogFlow(("SUPR0MemGetPhys: pSession=%p uPtr=%p paPages=%p\n", pSession, (void *)uPtr, paPages));

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(paPages, VERR_INVALID_POINTER);
    AssertReturn(uPtr, VERR_INVALID_PARAMETER);

    /*
     * Search for the address.
     */
    RTSpinlockAcquire(pSession->Spinlock, &SpinlockTmp);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed > 0)
        {
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
            {
                if (    pBundle->aMem[i].eType == MEMREF_TYPE_MEM
                    &&  pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                    &&  (   (RTHCUINTPTR)RTR0MemObjAddress(pBundle->aMem[i].MemObj) == uPtr
                         || (   pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                             && RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3) == uPtr)
                        )
                   )
                {
                    const size_t cPages = RTR0MemObjSize(pBundle->aMem[i].MemObj) >> PAGE_SHIFT;
                    size_t iPage;
                    for (iPage = 0; iPage < cPages; iPage++)
                    {
                        paPages[iPage].Phys = RTR0MemObjGetPagePhysAddr(pBundle->aMem[i].MemObj, iPage);
                        paPages[iPage].uReserved = 0;
                    }
                    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
                    return VINF_SUCCESS;
                }
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
    Log(("Failed to find %p!!!\n", (void *)uPtr));
    return VERR_INVALID_PARAMETER;
}


/**
 * Free memory allocated by SUPR0MemAlloc().
 *
 * @returns IPRT status code.
 * @param   pSession        The session owning the allocation.
 * @param   uPtr            The Ring-0 or Ring-3 address returned by SUPR0MemAlloc().
 */
SUPR0DECL(int) SUPR0MemFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr)
{
    LogFlow(("SUPR0MemFree: pSession=%p uPtr=%p\n", pSession, (void *)uPtr));
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    return supdrvMemRelease(pSession, uPtr, MEMREF_TYPE_MEM);
}


/**
 * Allocates a chunk of memory with only a R3 mappings.
 *
 * The memory is fixed and it's possible to query the physical addresses using
 * SUPR0MemGetPhys().
 *
 * @returns IPRT status code.
 * @param   pSession    The session to associated the allocation with.
 * @param   cPages      The number of pages to allocate.
 * @param   ppvR3       Where to store the address of the Ring-3 mapping.
 * @param   paPages     Where to store the addresses of the pages. Optional.
 */
SUPR0DECL(int) SUPR0PageAlloc(PSUPDRVSESSION pSession, uint32_t cPages, PRTR3PTR ppvR3, PRTHCPHYS paPages)
{
    AssertPtrReturn(ppvR3, VERR_INVALID_POINTER);
    return SUPR0PageAllocEx(pSession, cPages, 0 /*fFlags*/, ppvR3, NULL, paPages);
}


/**
 * Allocates a chunk of memory with a kernel or/and a user mode mapping.
 *
 * The memory is fixed and it's possible to query the physical addresses using
 * SUPR0MemGetPhys().
 *
 * @returns IPRT status code.
 * @param   pSession    The session to associated the allocation with.
 * @param   cPages      The number of pages to allocate.
 * @param   fFlags      Flags, reserved for the future. Must be zero.
 * @param   ppvR3       Where to store the address of the Ring-3 mapping.
 *                      NULL if no ring-3 mapping.
 * @param   ppvR3       Where to store the address of the Ring-0 mapping.
 *                      NULL if no ring-0 mapping.
 * @param   paPages     Where to store the addresses of the pages. Optional.
 */
SUPR0DECL(int) SUPR0PageAllocEx(PSUPDRVSESSION pSession, uint32_t cPages, uint32_t fFlags, PRTR3PTR ppvR3, PRTR0PTR ppvR0, PRTHCPHYS paPages)
{
    int             rc;
    SUPDRVMEMREF    Mem = { NIL_RTR0MEMOBJ, NIL_RTR0MEMOBJ, MEMREF_TYPE_UNUSED };
    LogFlow(("SUPR0PageAlloc: pSession=%p cb=%d ppvR3=%p\n", pSession, cPages, ppvR3));

    /*
     * Validate input. The allowed allocation size must be at least equal to the maximum guest VRAM size.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(ppvR3, VERR_INVALID_POINTER);
    AssertPtrNullReturn(ppvR0, VERR_INVALID_POINTER);
    AssertReturn(ppvR3 || ppvR0, VERR_INVALID_PARAMETER);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);
    if (cPages < 1 || cPages > VBOX_MAX_ALLOC_PAGE_COUNT)
    {
        Log(("SUPR0PageAlloc: Illegal request cb=%u; must be greater than 0 and smaller than 128MB.\n", cPages));
        return VERR_PAGE_COUNT_OUT_OF_RANGE;
    }

    /*
     * Let IPRT do the work.
     */
    if (ppvR0)
        rc = RTR0MemObjAllocPage(&Mem.MemObj, (size_t)cPages * PAGE_SIZE, true /* fExecutable */);
    else
        rc = RTR0MemObjAllocPhysNC(&Mem.MemObj, (size_t)cPages * PAGE_SIZE, NIL_RTHCPHYS);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        if (ppvR3)
            rc = RTR0MemObjMapUser(&Mem.MapObjR3, Mem.MemObj, (RTR3PTR)-1, 0,
                                   RTMEM_PROT_EXEC | RTMEM_PROT_WRITE | RTMEM_PROT_READ, RTR0ProcHandleSelf());
        else
            Mem.MapObjR3 = NIL_RTR0MEMOBJ;
        if (RT_SUCCESS(rc))
        {
            Mem.eType = MEMREF_TYPE_PAGE;
            rc = supdrvMemAdd(&Mem, pSession);
            if (!rc)
            {
                if (ppvR3)
                    *ppvR3 = RTR0MemObjAddressR3(Mem.MapObjR3);
                if (ppvR0)
                    *ppvR0 = RTR0MemObjAddress(Mem.MemObj);
                if (paPages)
                {
                    uint32_t iPage = cPages;
                    while (iPage-- > 0)
                    {
                        paPages[iPage] = RTR0MemObjGetPagePhysAddr(Mem.MapObjR3, iPage);
                        Assert(paPages[iPage] != NIL_RTHCPHYS);
                    }
                }
                return VINF_SUCCESS;
            }

            rc2 = RTR0MemObjFree(Mem.MapObjR3, false);
            AssertRC(rc2);
        }

        rc2 = RTR0MemObjFree(Mem.MemObj, false);
        AssertRC(rc2);
    }
    return rc;
}


/**
 * Allocates a chunk of memory with a kernel or/and a user mode mapping.
 *
 * The memory is fixed and it's possible to query the physical addresses using
 * SUPR0MemGetPhys().
 *
 * @returns IPRT status code.
 * @param   pSession    The session to associated the allocation with.
 * @param   cPages      The number of pages to allocate.
 * @param   fFlags      Flags, reserved for the future. Must be zero.
 * @param   ppvR3       Where to store the address of the Ring-3 mapping.
 *                      NULL if no ring-3 mapping.
 * @param   ppvR3       Where to store the address of the Ring-0 mapping.
 *                      NULL if no ring-0 mapping.
 * @param   paPages     Where to store the addresses of the pages. Optional.
 */
SUPR0DECL(int) SUPR0PageMapKernel(PSUPDRVSESSION pSession, RTR3PTR pvR3, uint32_t offSub, uint32_t cbSub,
                                  uint32_t fFlags, PRTR0PTR ppvR0)
{
    int             rc;
    PSUPDRVBUNDLE   pBundle;
    RTSPINLOCKTMP   SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    RTR0MEMOBJ      hMemObj = NIL_RTR0MEMOBJ;
    LogFlow(("SUPR0PageMapKernel: pSession=%p pvR3=%p offSub=%#x cbSub=%#x\n", pSession, pvR3, offSub, cbSub));

    /*
     * Validate input. The allowed allocation size must be at least equal to the maximum guest VRAM size.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(ppvR0, VERR_INVALID_POINTER);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);
    AssertReturn(!(offSub & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!(cbSub & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(cbSub, VERR_INVALID_PARAMETER);

    /*
     * Find the memory object.
     */
    RTSpinlockAcquire(pSession->Spinlock, &SpinlockTmp);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed > 0)
        {
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
            {
                if (    (   pBundle->aMem[i].eType == MEMREF_TYPE_PAGE
                         && pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                         && pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                         && RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3) == pvR3)
                    ||  (   pBundle->aMem[i].eType == MEMREF_TYPE_LOCKED
                         && pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                         && pBundle->aMem[i].MapObjR3 == NIL_RTR0MEMOBJ
                         && RTR0MemObjAddressR3(pBundle->aMem[i].MemObj) == pvR3))
                {
                    hMemObj = pBundle->aMem[i].MemObj;
                    break;
                }
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);

    rc = VERR_INVALID_PARAMETER;
    if (hMemObj != NIL_RTR0MEMOBJ)
    {
        /*
         * Do some furter input validations before calling IPRT.
         * (Cleanup is done indirectly by telling RTR0MemObjFree to include mappings.)
         */
        size_t cbMemObj = RTR0MemObjSize(hMemObj);
        if (    offSub < cbMemObj
            &&  cbSub <= cbMemObj
            &&  offSub + cbSub <= cbMemObj)
        {
            RTR0MEMOBJ hMapObj;
            rc = RTR0MemObjMapKernelEx(&hMapObj, hMemObj, (void *)-1, 0,
                                       RTMEM_PROT_READ | RTMEM_PROT_WRITE, offSub, cbSub);
            if (RT_SUCCESS(rc))
                *ppvR0 = RTR0MemObjAddress(hMapObj);
        }
        else
            SUPR0Printf("SUPR0PageMapKernel: cbMemObj=%#x offSub=%#x cbSub=%#x\n", cbMemObj, offSub, cbSub);

    }
    return rc;
}



#ifdef RT_OS_WINDOWS
/**
 * Check if the pages were locked by SUPR0PageAlloc
 *
 * This function will be removed along with the lock/unlock hacks when
 * we've cleaned up the ring-3 code properly.
 *
 * @returns boolean
 * @param   pSession        The session to which the memory was allocated.
 * @param   pvR3            The Ring-3 address returned by SUPR0PageAlloc().
 */
static bool supdrvPageWasLockedByPageAlloc(PSUPDRVSESSION pSession, RTR3PTR pvR3)
{
    PSUPDRVBUNDLE pBundle;
    RTSPINLOCKTMP SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    LogFlow(("SUPR0PageIsLockedByPageAlloc: pSession=%p pvR3=%p\n", pSession, (void *)pvR3));

    /*
     * Search for the address.
     */
    RTSpinlockAcquire(pSession->Spinlock, &SpinlockTmp);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed > 0)
        {
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
            {
                if (    pBundle->aMem[i].eType == MEMREF_TYPE_PAGE
                    &&  pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                    &&  pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                    &&  RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3) == pvR3)
                {
                    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
                    return true;
                }
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
    return false;
}


/**
 * Get the physical addresses of memory allocated using SUPR0PageAllocEx().
 *
 * This function will be removed along with the lock/unlock hacks when
 * we've cleaned up the ring-3 code properly.
 *
 * @returns IPRT status code.
 * @param   pSession        The session to which the memory was allocated.
 * @param   pvR3            The Ring-3 address returned by SUPR0PageAlloc().
 * @param   cPages          Number of pages in paPages
 * @param   paPages         Where to store the physical addresses.
 */
static int supdrvPageGetPhys(PSUPDRVSESSION pSession, RTR3PTR pvR3, uint32_t cPages, PRTHCPHYS paPages)
{
    PSUPDRVBUNDLE pBundle;
    RTSPINLOCKTMP SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    LogFlow(("supdrvPageGetPhys: pSession=%p pvR3=%p cPages=%#lx paPages=%p\n", pSession, (void *)pvR3, (long)cPages, paPages));

    /*
     * Search for the address.
     */
    RTSpinlockAcquire(pSession->Spinlock, &SpinlockTmp);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed > 0)
        {
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
            {
                if (    pBundle->aMem[i].eType == MEMREF_TYPE_PAGE
                    &&  pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                    &&  pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                    &&  RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3) == pvR3)
                {
                    uint32_t iPage;
                    size_t cMaxPages = RTR0MemObjSize(pBundle->aMem[i].MemObj) >> PAGE_SHIFT;
                    cPages = (uint32_t)RT_MIN(cMaxPages, cPages);
                    for (iPage = 0; iPage < cPages; iPage++)
                        paPages[iPage] = RTR0MemObjGetPagePhysAddr(pBundle->aMem[i].MemObj, iPage);
                    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
                    return VINF_SUCCESS;
                }
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
    return VERR_INVALID_PARAMETER;
}
#endif /* RT_OS_WINDOWS */


/**
 * Free memory allocated by SUPR0PageAlloc() and SUPR0PageAllocEx().
 *
 * @returns IPRT status code.
 * @param   pSession        The session owning the allocation.
 * @param   pvR3             The Ring-3 address returned by SUPR0PageAlloc() or
 *                           SUPR0PageAllocEx().
 */
SUPR0DECL(int) SUPR0PageFree(PSUPDRVSESSION pSession, RTR3PTR pvR3)
{
    LogFlow(("SUPR0PageFree: pSession=%p pvR3=%p\n", pSession, (void *)pvR3));
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    return supdrvMemRelease(pSession, (RTHCUINTPTR)pvR3, MEMREF_TYPE_PAGE);
}


/**
 * Maps the GIP into userspace and/or get the physical address of the GIP.
 *
 * @returns IPRT status code.
 * @param   pSession        Session to which the GIP mapping should belong.
 * @param   ppGipR3         Where to store the address of the ring-3 mapping. (optional)
 * @param   pHCPhysGip      Where to store the physical address. (optional)
 *
 * @remark  There is no reference counting on the mapping, so one call to this function
 *          count globally as one reference. One call to SUPR0GipUnmap() is will unmap GIP
 *          and remove the session as a GIP user.
 */
SUPR0DECL(int) SUPR0GipMap(PSUPDRVSESSION pSession, PRTR3PTR ppGipR3, PRTHCPHYS pHCPhysGip)
{
    int             rc = VINF_SUCCESS;
    PSUPDRVDEVEXT   pDevExt = pSession->pDevExt;
    RTR3PTR         pGip = NIL_RTR3PTR;
    RTHCPHYS        HCPhys = NIL_RTHCPHYS;
    LogFlow(("SUPR0GipMap: pSession=%p ppGipR3=%p pHCPhysGip=%p\n", pSession, ppGipR3, pHCPhysGip));

    /*
     * Validate
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(ppGipR3, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pHCPhysGip, VERR_INVALID_POINTER);

    RTSemFastMutexRequest(pDevExt->mtxGip);
    if (pDevExt->pGip)
    {
        /*
         * Map it?
         */
        if (ppGipR3)
        {
            if (pSession->GipMapObjR3 == NIL_RTR0MEMOBJ)
                rc = RTR0MemObjMapUser(&pSession->GipMapObjR3, pDevExt->GipMemObj, (RTR3PTR)-1, 0,
                                       RTMEM_PROT_READ, RTR0ProcHandleSelf());
            if (RT_SUCCESS(rc))
            {
                pGip = RTR0MemObjAddressR3(pSession->GipMapObjR3);
                rc = VINF_SUCCESS; /** @todo remove this and replace the !rc below with RT_SUCCESS(rc). */
            }
        }

        /*
         * Get physical address.
         */
        if (pHCPhysGip && !rc)
            HCPhys = pDevExt->HCPhysGip;

        /*
         * Reference globally.
         */
        if (!pSession->fGipReferenced && !rc)
        {
            pSession->fGipReferenced = 1;
            pDevExt->cGipUsers++;
            if (pDevExt->cGipUsers == 1)
            {
                PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
                unsigned i;

                LogFlow(("SUPR0GipMap: Resumes GIP updating\n"));

                for (i = 0; i < RT_ELEMENTS(pGip->aCPUs); i++)
                    ASMAtomicXchgU32(&pGip->aCPUs[i].u32TransactionId, pGip->aCPUs[i].u32TransactionId & ~(GIP_UPDATEHZ_RECALC_FREQ * 2 - 1));
                ASMAtomicXchgU64(&pGip->u64NanoTSLastUpdateHz, 0);

                rc = RTTimerStart(pDevExt->pGipTimer, 0);
                AssertRC(rc); rc = VINF_SUCCESS;
            }
        }
    }
    else
    {
        rc = SUPDRV_ERR_GENERAL_FAILURE;
        Log(("SUPR0GipMap: GIP is not available!\n"));
    }
    RTSemFastMutexRelease(pDevExt->mtxGip);

    /*
     * Write returns.
     */
    if (pHCPhysGip)
        *pHCPhysGip = HCPhys;
    if (ppGipR3)
        *ppGipR3 = pGip;

#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("SUPR0GipMap: returns %d *pHCPhysGip=%lx pGip=%p\n", rc, (unsigned long)HCPhys, (void *)pGip));
#else
    LogFlow((   "SUPR0GipMap: returns %d *pHCPhysGip=%lx pGip=%p\n", rc, (unsigned long)HCPhys, (void *)pGip));
#endif
    return rc;
}


/**
 * Unmaps any user mapping of the GIP and terminates all GIP access
 * from this session.
 *
 * @returns IPRT status code.
 * @param   pSession        Session to which the GIP mapping should belong.
 */
SUPR0DECL(int) SUPR0GipUnmap(PSUPDRVSESSION pSession)
{
    int                     rc = VINF_SUCCESS;
    PSUPDRVDEVEXT           pDevExt = pSession->pDevExt;
#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("SUPR0GipUnmap: pSession=%p pGip=%p GipMapObjR3=%p\n",
                pSession,
                pSession->GipMapObjR3 != NIL_RTR0MEMOBJ ? RTR0MemObjAddress(pSession->GipMapObjR3) : NULL,
                pSession->GipMapObjR3));
#else
    LogFlow(("SUPR0GipUnmap: pSession=%p\n", pSession));
#endif
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);

    RTSemFastMutexRequest(pDevExt->mtxGip);

    /*
     * Unmap anything?
     */
    if (pSession->GipMapObjR3 != NIL_RTR0MEMOBJ)
    {
        rc = RTR0MemObjFree(pSession->GipMapObjR3, false);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
            pSession->GipMapObjR3 = NIL_RTR0MEMOBJ;
    }

    /*
     * Dereference global GIP.
     */
    if (pSession->fGipReferenced && !rc)
    {
        pSession->fGipReferenced = 0;
        if (    pDevExt->cGipUsers > 0
            &&  !--pDevExt->cGipUsers)
        {
            LogFlow(("SUPR0GipUnmap: Suspends GIP updating\n"));
            rc = RTTimerStop(pDevExt->pGipTimer); AssertRC(rc); rc = VINF_SUCCESS;
        }
    }

    RTSemFastMutexRelease(pDevExt->mtxGip);

    return rc;
}


/**
 * Register a component factory with the support driver.
 *
 * This is currently restricted to kernel sessions only.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NO_MEMORY if we're out of memory.
 * @retval  VERR_ALREADY_EXISTS if the factory has already been registered.
 * @retval  VERR_ACCESS_DENIED if it isn't a kernel session.
 * @retval  VERR_INVALID_PARAMETER on invalid parameter.
 * @retval  VERR_INVALID_POINTER on invalid pointer parameter.
 *
 * @param   pSession        The SUPDRV session (must be a ring-0 session).
 * @param   pFactory        Pointer to the component factory registration structure.
 *
 * @remarks This interface is also available via SUPR0IdcComponentRegisterFactory.
 */
SUPR0DECL(int) SUPR0ComponentRegisterFactory(PSUPDRVSESSION pSession, PCSUPDRVFACTORY pFactory)
{
    PSUPDRVFACTORYREG pNewReg;
    const char *psz;
    int rc;

    /*
     * Validate parameters.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertReturn(pSession->R0Process == NIL_RTR0PROCESS, VERR_ACCESS_DENIED);
    AssertPtrReturn(pFactory, VERR_INVALID_POINTER);
    AssertPtrReturn(pFactory->pfnQueryFactoryInterface, VERR_INVALID_POINTER);
    psz = (const char *)memchr(pFactory->szName, '\0', sizeof(pFactory->szName));
    AssertReturn(psz, VERR_INVALID_PARAMETER);

    /*
     * Allocate and initialize a new registration structure.
     */
    pNewReg = (PSUPDRVFACTORYREG)RTMemAlloc(sizeof(SUPDRVFACTORYREG));
    if (pNewReg)
    {
        pNewReg->pNext = NULL;
        pNewReg->pFactory = pFactory;
        pNewReg->pSession = pSession;
        pNewReg->cchName = psz - &pFactory->szName[0];

        /*
         * Add it to the tail of the list after checking for prior registration.
         */
        rc = RTSemFastMutexRequest(pSession->pDevExt->mtxComponentFactory);
        if (RT_SUCCESS(rc))
        {
            PSUPDRVFACTORYREG pPrev = NULL;
            PSUPDRVFACTORYREG pCur = pSession->pDevExt->pComponentFactoryHead;
            while (pCur && pCur->pFactory != pFactory)
            {
                pPrev = pCur;
                pCur = pCur->pNext;
            }
            if (!pCur)
            {
                if (pPrev)
                    pPrev->pNext = pNewReg;
                else
                    pSession->pDevExt->pComponentFactoryHead = pNewReg;
                rc = VINF_SUCCESS;
            }
            else
                rc = VERR_ALREADY_EXISTS;

            RTSemFastMutexRelease(pSession->pDevExt->mtxComponentFactory);
        }

        if (RT_FAILURE(rc))
            RTMemFree(pNewReg);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Deregister a component factory.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_FOUND if the factory wasn't registered.
 * @retval  VERR_ACCESS_DENIED if it isn't a kernel session.
 * @retval  VERR_INVALID_PARAMETER on invalid parameter.
 * @retval  VERR_INVALID_POINTER on invalid pointer parameter.
 *
 * @param   pSession        The SUPDRV session (must be a ring-0 session).
 * @param   pFactory        Pointer to the component factory registration structure
 *                          previously passed SUPR0ComponentRegisterFactory().
 *
 * @remarks This interface is also available via SUPR0IdcComponentDeregisterFactory.
 */
SUPR0DECL(int) SUPR0ComponentDeregisterFactory(PSUPDRVSESSION pSession, PCSUPDRVFACTORY pFactory)
{
    int rc;

    /*
     * Validate parameters.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertReturn(pSession->R0Process == NIL_RTR0PROCESS, VERR_ACCESS_DENIED);
    AssertPtrReturn(pFactory, VERR_INVALID_POINTER);

    /*
     * Take the lock and look for the registration record.
     */
    rc = RTSemFastMutexRequest(pSession->pDevExt->mtxComponentFactory);
    if (RT_SUCCESS(rc))
    {
        PSUPDRVFACTORYREG pPrev = NULL;
        PSUPDRVFACTORYREG pCur = pSession->pDevExt->pComponentFactoryHead;
        while (pCur && pCur->pFactory != pFactory)
        {
            pPrev = pCur;
            pCur = pCur->pNext;
        }
        if (pCur)
        {
            if (!pPrev)
                pSession->pDevExt->pComponentFactoryHead = pCur->pNext;
            else
                pPrev->pNext = pCur->pNext;

            pCur->pNext = NULL;
            pCur->pFactory = NULL;
            pCur->pSession = NULL;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_NOT_FOUND;

        RTSemFastMutexRelease(pSession->pDevExt->mtxComponentFactory);

        RTMemFree(pCur);
    }
    return rc;
}


/**
 * Queries a component factory.
 *
 * @returns VBox status code.
 * @retval  VERR_INVALID_PARAMETER on invalid parameter.
 * @retval  VERR_INVALID_POINTER on invalid pointer parameter.
 * @retval  VERR_SUPDRV_COMPONENT_NOT_FOUND if the component factory wasn't found.
 * @retval  VERR_SUPDRV_INTERFACE_NOT_SUPPORTED if the interface wasn't supported.
 *
 * @param   pSession            The SUPDRV session.
 * @param   pszName             The name of the component factory.
 * @param   pszInterfaceUuid    The UUID of the factory interface (stringified).
 * @param   ppvFactoryIf        Where to store the factory interface.
 */
SUPR0DECL(int) SUPR0ComponentQueryFactory(PSUPDRVSESSION pSession, const char *pszName, const char *pszInterfaceUuid, void **ppvFactoryIf)
{
    const char *pszEnd;
    size_t cchName;
    int rc;

    /*
     * Validate parameters.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);

    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    pszEnd = memchr(pszName, '\0', RT_SIZEOFMEMB(SUPDRVFACTORY, szName));
    AssertReturn(pszEnd, VERR_INVALID_PARAMETER);
    cchName = pszEnd - pszName;

    AssertPtrReturn(pszInterfaceUuid, VERR_INVALID_POINTER);
    pszEnd = memchr(pszInterfaceUuid, '\0', RTUUID_STR_LENGTH);
    AssertReturn(pszEnd, VERR_INVALID_PARAMETER);

    AssertPtrReturn(ppvFactoryIf, VERR_INVALID_POINTER);
    *ppvFactoryIf = NULL;

    /*
     * Take the lock and try all factories by this name.
     */
    rc = RTSemFastMutexRequest(pSession->pDevExt->mtxComponentFactory);
    if (RT_SUCCESS(rc))
    {
        PSUPDRVFACTORYREG pCur = pSession->pDevExt->pComponentFactoryHead;
        rc = VERR_SUPDRV_COMPONENT_NOT_FOUND;
        while (pCur)
        {
            if (    pCur->cchName == cchName
                &&  !memcmp(pCur->pFactory->szName, pszName, cchName))
            {
#ifdef RT_WITH_W64_UNWIND_HACK
                void *pvFactory = supdrvNtWrapQueryFactoryInterface((PFNRT)pCur->pFactory->pfnQueryFactoryInterface, pCur->pFactory, pSession, pszInterfaceUuid);
#else
                void *pvFactory = pCur->pFactory->pfnQueryFactoryInterface(pCur->pFactory, pSession, pszInterfaceUuid);
#endif
                if (pvFactory)
                {
                    *ppvFactoryIf = pvFactory;
                    rc = VINF_SUCCESS;
                    break;
                }
                rc = VERR_SUPDRV_INTERFACE_NOT_SUPPORTED;
            }

            /* next */
            pCur = pCur->pNext;
        }

        RTSemFastMutexRelease(pSession->pDevExt->mtxComponentFactory);
    }
    return rc;
}


/**
 * Adds a memory object to the session.
 *
 * @returns IPRT status code.
 * @param   pMem        Memory tracking structure containing the
 *                      information to track.
 * @param   pSession    The session.
 */
static int supdrvMemAdd(PSUPDRVMEMREF pMem, PSUPDRVSESSION pSession)
{
    PSUPDRVBUNDLE pBundle;
    RTSPINLOCKTMP SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;

    /*
     * Find free entry and record the allocation.
     */
    RTSpinlockAcquire(pSession->Spinlock, &SpinlockTmp);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed < RT_ELEMENTS(pBundle->aMem))
        {
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
            {
                if (pBundle->aMem[i].MemObj == NIL_RTR0MEMOBJ)
                {
                    pBundle->cUsed++;
                    pBundle->aMem[i] = *pMem;
                    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
                    return VINF_SUCCESS;
                }
            }
            AssertFailed();             /* !!this can't be happening!!! */
        }
    }
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);

    /*
     * Need to allocate a new bundle.
     * Insert into the last entry in the bundle.
     */
    pBundle = (PSUPDRVBUNDLE)RTMemAllocZ(sizeof(*pBundle));
    if (!pBundle)
        return VERR_NO_MEMORY;

    /* take last entry. */
    pBundle->cUsed++;
    pBundle->aMem[RT_ELEMENTS(pBundle->aMem) - 1] = *pMem;

    /* insert into list. */
    RTSpinlockAcquire(pSession->Spinlock, &SpinlockTmp);
    pBundle->pNext = pSession->Bundle.pNext;
    pSession->Bundle.pNext = pBundle;
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);

    return VINF_SUCCESS;
}


/**
 * Releases a memory object referenced by pointer and type.
 *
 * @returns IPRT status code.
 * @param   pSession    Session data.
 * @param   uPtr        Pointer to memory. This is matched against both the R0 and R3 addresses.
 * @param   eType       Memory type.
 */
static int supdrvMemRelease(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, SUPDRVMEMREFTYPE eType)
{
    PSUPDRVBUNDLE pBundle;
    RTSPINLOCKTMP SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;

    /*
     * Validate input.
     */
    if (!uPtr)
    {
        Log(("Illegal address %p\n", (void *)uPtr));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Search for the address.
     */
    RTSpinlockAcquire(pSession->Spinlock, &SpinlockTmp);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed > 0)
        {
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
            {
                if (    pBundle->aMem[i].eType == eType
                    &&  pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                    &&  (   (RTHCUINTPTR)RTR0MemObjAddress(pBundle->aMem[i].MemObj) == uPtr
                         || (   pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                             && RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3) == uPtr))
                   )
                {
                    /* Make a copy of it and release it outside the spinlock. */
                    SUPDRVMEMREF Mem = pBundle->aMem[i];
                    pBundle->aMem[i].eType = MEMREF_TYPE_UNUSED;
                    pBundle->aMem[i].MemObj = NIL_RTR0MEMOBJ;
                    pBundle->aMem[i].MapObjR3 = NIL_RTR0MEMOBJ;
                    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);

                    if (Mem.MapObjR3 != NIL_RTR0MEMOBJ)
                    {
                        int rc = RTR0MemObjFree(Mem.MapObjR3, false);
                        AssertRC(rc); /** @todo figure out how to handle this. */
                    }
                    if (Mem.MemObj != NIL_RTR0MEMOBJ)
                    {
                        int rc = RTR0MemObjFree(Mem.MemObj, true /* fFreeMappings */);
                        AssertRC(rc); /** @todo figure out how to handle this. */
                    }
                    return VINF_SUCCESS;
                }
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);
    Log(("Failed to find %p!!! (eType=%d)\n", (void *)uPtr, eType));
    return VERR_INVALID_PARAMETER;
}


/**
 * Opens an image. If it's the first time it's opened the call must upload
 * the bits using the supdrvIOCtl_LdrLoad() / SUPDRV_IOCTL_LDR_LOAD function.
 *
 * This is the 1st step of the loading.
 *
 * @returns IPRT status code.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pReq        The open request.
 */
static int supdrvIOCtl_LdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDROPEN pReq)
{
    PSUPDRVLDRIMAGE pImage;
    unsigned        cb;
    void           *pv;
    size_t          cchName = strlen(pReq->u.In.szName); /* (caller checked < 32). */
    LogFlow(("supdrvIOCtl_LdrOpen: szName=%s cbImage=%d\n", pReq->u.In.szName, pReq->u.In.cbImage));

    /*
     * Check if we got an instance of the image already.
     */
    RTSemFastMutexRequest(pDevExt->mtxLdr);
    for (pImage = pDevExt->pLdrImages; pImage; pImage = pImage->pNext)
    {
        if (    pImage->szName[cchName] == '\0'
            &&  !memcmp(pImage->szName, pReq->u.In.szName, cchName))
        {
            pImage->cUsage++;
            pReq->u.Out.pvImageBase   = pImage->pvImage;
            pReq->u.Out.fNeedsLoading = pImage->uState == SUP_IOCTL_LDR_OPEN;
            supdrvLdrAddUsage(pSession, pImage);
            RTSemFastMutexRelease(pDevExt->mtxLdr);
            return VINF_SUCCESS;
        }
    }
    /* (not found - add it!) */

    /*
     * Allocate memory.
     */
    cb = pReq->u.In.cbImage + sizeof(SUPDRVLDRIMAGE) + 31;
    pv = RTMemExecAlloc(cb);
    if (!pv)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        Log(("supdrvIOCtl_LdrOpen: RTMemExecAlloc(%u) failed\n", cb));
        return VERR_NO_MEMORY;
    }

    /*
     * Setup and link in the LDR stuff.
     */
    pImage = (PSUPDRVLDRIMAGE)pv;
    pImage->pvImage         = RT_ALIGN_P(pImage + 1, 32);
    pImage->cbImage         = pReq->u.In.cbImage;
    pImage->pfnModuleInit   = NULL;
    pImage->pfnModuleTerm   = NULL;
    pImage->pfnServiceReqHandler = NULL;
    pImage->uState          = SUP_IOCTL_LDR_OPEN;
    pImage->cUsage          = 1;
    memcpy(pImage->szName, pReq->u.In.szName, cchName + 1);

    pImage->pNext           = pDevExt->pLdrImages;
    pDevExt->pLdrImages     = pImage;

    supdrvLdrAddUsage(pSession, pImage);

    pReq->u.Out.pvImageBase = pImage->pvImage;
    pReq->u.Out.fNeedsLoading = true;
    RTSemFastMutexRelease(pDevExt->mtxLdr);

#if defined(RT_OS_WINDOWS) && defined(DEBUG)
    SUPR0Printf("VBoxDrv: windbg> .reload /f %s=%#p\n", pImage->szName, pImage->pvImage);
#endif
    return VINF_SUCCESS;
}


/**
 * Loads the image bits.
 *
 * This is the 2nd step of the loading.
 *
 * @returns IPRT status code.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pReq        The request.
 */
static int supdrvIOCtl_LdrLoad(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRLOAD pReq)
{
    PSUPDRVLDRUSAGE pUsage;
    PSUPDRVLDRIMAGE pImage;
    int             rc;
    LogFlow(("supdrvIOCtl_LdrLoad: pvImageBase=%p cbImage=%d\n", pReq->u.In.pvImageBase, pReq->u.In.cbImage));

    /*
     * Find the ldr image.
     */
    RTSemFastMutexRequest(pDevExt->mtxLdr);
    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pReq->u.In.pvImageBase)
        pUsage = pUsage->pNext;
    if (!pUsage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        Log(("SUP_IOCTL_LDR_LOAD: couldn't find image!\n"));
        return VERR_INVALID_HANDLE;
    }
    pImage = pUsage->pImage;
    if (pImage->cbImage != pReq->u.In.cbImage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        Log(("SUP_IOCTL_LDR_LOAD: image size mismatch!! %d(prep) != %d(load)\n", pImage->cbImage, pReq->u.In.cbImage));
        return VERR_INVALID_HANDLE;
    }
    if (pImage->uState != SUP_IOCTL_LDR_OPEN)
    {
        unsigned uState = pImage->uState;
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        if (uState != SUP_IOCTL_LDR_LOAD)
            AssertMsgFailed(("SUP_IOCTL_LDR_LOAD: invalid image state %d (%#x)!\n", uState, uState));
        return SUPDRV_ERR_ALREADY_LOADED;
    }
    switch (pReq->u.In.eEPType)
    {
        case SUPLDRLOADEP_NOTHING:
            break;

        case SUPLDRLOADEP_VMMR0:
            if (    !pReq->u.In.EP.VMMR0.pvVMMR0
                ||  !pReq->u.In.EP.VMMR0.pvVMMR0EntryInt
                ||  !pReq->u.In.EP.VMMR0.pvVMMR0EntryFast
                ||  !pReq->u.In.EP.VMMR0.pvVMMR0EntryEx)
            {
                RTSemFastMutexRelease(pDevExt->mtxLdr);
                Log(("NULL pointer: pvVMMR0=%p pvVMMR0EntryInt=%p pvVMMR0EntryFast=%p pvVMMR0EntryEx=%p!\n",
                     pReq->u.In.EP.VMMR0.pvVMMR0, pReq->u.In.EP.VMMR0.pvVMMR0EntryInt,
                     pReq->u.In.EP.VMMR0.pvVMMR0EntryFast, pReq->u.In.EP.VMMR0.pvVMMR0EntryEx));
                return VERR_INVALID_PARAMETER;
            }
            /** @todo validate pReq->u.In.EP.VMMR0.pvVMMR0 against pvImage! */
            if (    (uintptr_t)pReq->u.In.EP.VMMR0.pvVMMR0EntryInt  - (uintptr_t)pImage->pvImage >= pReq->u.In.cbImage
                ||  (uintptr_t)pReq->u.In.EP.VMMR0.pvVMMR0EntryFast - (uintptr_t)pImage->pvImage >= pReq->u.In.cbImage
                ||  (uintptr_t)pReq->u.In.EP.VMMR0.pvVMMR0EntryEx   - (uintptr_t)pImage->pvImage >= pReq->u.In.cbImage)
            {
                RTSemFastMutexRelease(pDevExt->mtxLdr);
                Log(("Out of range (%p LB %#x): pvVMMR0EntryInt=%p, pvVMMR0EntryFast=%p or pvVMMR0EntryEx=%p is NULL!\n",
                     pImage->pvImage, pReq->u.In.cbImage, pReq->u.In.EP.VMMR0.pvVMMR0EntryInt,
                     pReq->u.In.EP.VMMR0.pvVMMR0EntryFast, pReq->u.In.EP.VMMR0.pvVMMR0EntryEx));
                return VERR_INVALID_PARAMETER;
            }
            break;

        case SUPLDRLOADEP_SERVICE:
            if (!pReq->u.In.EP.Service.pfnServiceReq)
            {
                RTSemFastMutexRelease(pDevExt->mtxLdr);
                Log(("NULL pointer: pfnServiceReq=%p!\n", pReq->u.In.EP.Service.pfnServiceReq));
                return VERR_INVALID_PARAMETER;
            }
            if ((uintptr_t)pReq->u.In.EP.Service.pfnServiceReq  - (uintptr_t)pImage->pvImage >= pReq->u.In.cbImage)
            {
                RTSemFastMutexRelease(pDevExt->mtxLdr);
                Log(("Out of range (%p LB %#x): pfnServiceReq=%p, pvVMMR0EntryFast=%p or pvVMMR0EntryEx=%p is NULL!\n",
                     pImage->pvImage, pReq->u.In.cbImage, pReq->u.In.EP.Service.pfnServiceReq));
                return VERR_INVALID_PARAMETER;
            }
            if (    pReq->u.In.EP.Service.apvReserved[0] != NIL_RTR0PTR
                ||  pReq->u.In.EP.Service.apvReserved[1] != NIL_RTR0PTR
                ||  pReq->u.In.EP.Service.apvReserved[2] != NIL_RTR0PTR)
            {
                RTSemFastMutexRelease(pDevExt->mtxLdr);
                Log(("Out of range (%p LB %#x): apvReserved={%p,%p,%p} MBZ!\n",
                     pImage->pvImage, pReq->u.In.cbImage,
                     pReq->u.In.EP.Service.apvReserved[0],
                     pReq->u.In.EP.Service.apvReserved[1],
                     pReq->u.In.EP.Service.apvReserved[2]));
                return VERR_INVALID_PARAMETER;
            }
            break;

        default:
            RTSemFastMutexRelease(pDevExt->mtxLdr);
            Log(("Invalid eEPType=%d\n", pReq->u.In.eEPType));
            return VERR_INVALID_PARAMETER;
    }
    if (    pReq->u.In.pfnModuleInit
        &&  (uintptr_t)pReq->u.In.pfnModuleInit - (uintptr_t)pImage->pvImage >= pReq->u.In.cbImage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        Log(("SUP_IOCTL_LDR_LOAD: pfnModuleInit=%p is outside the image (%p %d bytes)\n",
             pReq->u.In.pfnModuleInit, pImage->pvImage, pReq->u.In.cbImage));
        return VERR_INVALID_PARAMETER;
    }
    if (    pReq->u.In.pfnModuleTerm
        &&  (uintptr_t)pReq->u.In.pfnModuleTerm - (uintptr_t)pImage->pvImage >= pReq->u.In.cbImage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        Log(("SUP_IOCTL_LDR_LOAD: pfnModuleTerm=%p is outside the image (%p %d bytes)\n",
             pReq->u.In.pfnModuleTerm, pImage->pvImage, pReq->u.In.cbImage));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Copy the memory.
     */
    /* no need to do try/except as this is a buffered request. */
    memcpy(pImage->pvImage, &pReq->u.In.achImage[0], pImage->cbImage);
    pImage->uState = SUP_IOCTL_LDR_LOAD;
    pImage->pfnModuleInit = pReq->u.In.pfnModuleInit;
    pImage->pfnModuleTerm = pReq->u.In.pfnModuleTerm;
    pImage->offSymbols    = pReq->u.In.offSymbols;
    pImage->cSymbols      = pReq->u.In.cSymbols;
    pImage->offStrTab     = pReq->u.In.offStrTab;
    pImage->cbStrTab      = pReq->u.In.cbStrTab;

    /*
     * Update any entry points.
     */
    switch (pReq->u.In.eEPType)
    {
        default:
        case SUPLDRLOADEP_NOTHING:
            rc = VINF_SUCCESS;
            break;
        case SUPLDRLOADEP_VMMR0:
            rc = supdrvLdrSetVMMR0EPs(pDevExt, pReq->u.In.EP.VMMR0.pvVMMR0, pReq->u.In.EP.VMMR0.pvVMMR0EntryInt,
                                      pReq->u.In.EP.VMMR0.pvVMMR0EntryFast, pReq->u.In.EP.VMMR0.pvVMMR0EntryEx);
            break;
        case SUPLDRLOADEP_SERVICE:
            pImage->pfnServiceReqHandler = pReq->u.In.EP.Service.pfnServiceReq;
            rc = VINF_SUCCESS;
            break;
    }

    /*
     * On success call the module initialization.
     */
    LogFlow(("supdrvIOCtl_LdrLoad: pfnModuleInit=%p\n", pImage->pfnModuleInit));
    if (RT_SUCCESS(rc) && pImage->pfnModuleInit)
    {
        Log(("supdrvIOCtl_LdrLoad: calling pfnModuleInit=%p\n", pImage->pfnModuleInit));
#ifdef RT_WITH_W64_UNWIND_HACK
        rc = supdrvNtWrapModuleInit((PFNRT)pImage->pfnModuleInit);
#else
        rc = pImage->pfnModuleInit();
#endif
        if (rc && pDevExt->pvVMMR0 == pImage->pvImage)
            supdrvLdrUnsetVMMR0EPs(pDevExt);
    }

    if (rc)
        pImage->uState = SUP_IOCTL_LDR_OPEN;

    RTSemFastMutexRelease(pDevExt->mtxLdr);
    return rc;
}


/**
 * Frees a previously loaded (prep'ed) image.
 *
 * @returns IPRT status code.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pReq        The request.
 */
static int supdrvIOCtl_LdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRFREE pReq)
{
    int             rc;
    PSUPDRVLDRUSAGE pUsagePrev;
    PSUPDRVLDRUSAGE pUsage;
    PSUPDRVLDRIMAGE pImage;
    LogFlow(("supdrvIOCtl_LdrFree: pvImageBase=%p\n", pReq->u.In.pvImageBase));

    /*
     * Find the ldr image.
     */
    RTSemFastMutexRequest(pDevExt->mtxLdr);
    pUsagePrev = NULL;
    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pReq->u.In.pvImageBase)
    {
        pUsagePrev = pUsage;
        pUsage = pUsage->pNext;
    }
    if (!pUsage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        Log(("SUP_IOCTL_LDR_FREE: couldn't find image!\n"));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Check if we can remove anything.
     */
    rc = VINF_SUCCESS;
    pImage = pUsage->pImage;
    if (pImage->cUsage <= 1 || pUsage->cUsage <= 1)
    {
        /*
         * Check if there are any objects with destructors in the image, if
         * so leave it for the session cleanup routine so we get a chance to
         * clean things up in the right order and not leave them all dangling.
         */
        RTSPINLOCKTMP   SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
        RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);
        if (pImage->cUsage <= 1)
        {
            PSUPDRVOBJ pObj;
            for (pObj = pDevExt->pObjs; pObj; pObj = pObj->pNext)
                if (RT_UNLIKELY((uintptr_t)pObj->pfnDestructor - (uintptr_t)pImage->pvImage < pImage->cbImage))
                {
                    rc = VERR_DANGLING_OBJECTS;
                    break;
                }
        }
        else
        {
            PSUPDRVUSAGE pGenUsage;
            for (pGenUsage = pSession->pUsage; pGenUsage; pGenUsage = pGenUsage->pNext)
                if (RT_UNLIKELY((uintptr_t)pGenUsage->pObj->pfnDestructor - (uintptr_t)pImage->pvImage < pImage->cbImage))
                {
                    rc = VERR_DANGLING_OBJECTS;
                    break;
                }
        }
        RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);
        if (rc == VINF_SUCCESS)
        {
            /* unlink it */
            if (pUsagePrev)
                pUsagePrev->pNext = pUsage->pNext;
            else
                pSession->pLdrUsage = pUsage->pNext;

            /* free it */
            pUsage->pImage = NULL;
            pUsage->pNext = NULL;
            RTMemFree(pUsage);

            /*
             * Derefrence the image.
             */
            if (pImage->cUsage <= 1)
                supdrvLdrFree(pDevExt, pImage);
            else
                pImage->cUsage--;
        }
        else
        {
            Log(("supdrvIOCtl_LdrFree: Dangling objects in %p/%s!\n", pImage->pvImage, pImage->szName));
            rc = VINF_SUCCESS; /** @todo BRANCH-2.1: remove this after branching. */
        }
    }
    else
    {
        /*
         * Dereference both image and usage.
         */
        pImage->cUsage--;
        pUsage->cUsage--;
    }

    RTSemFastMutexRelease(pDevExt->mtxLdr);
    return rc;
}


/**
 * Gets the address of a symbol in an open image.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pReq        The request buffer.
 */
static int supdrvIOCtl_LdrGetSymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRGETSYMBOL pReq)
{
    PSUPDRVLDRIMAGE pImage;
    PSUPDRVLDRUSAGE pUsage;
    uint32_t        i;
    PSUPLDRSYM      paSyms;
    const char     *pchStrings;
    const size_t    cbSymbol = strlen(pReq->u.In.szSymbol) + 1;
    void           *pvSymbol = NULL;
    int             rc = VERR_GENERAL_FAILURE;
    Log3(("supdrvIOCtl_LdrGetSymbol: pvImageBase=%p szSymbol=\"%s\"\n", pReq->u.In.pvImageBase, pReq->u.In.szSymbol));

    /*
     * Find the ldr image.
     */
    RTSemFastMutexRequest(pDevExt->mtxLdr);
    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pReq->u.In.pvImageBase)
        pUsage = pUsage->pNext;
    if (!pUsage)
    {
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        Log(("SUP_IOCTL_LDR_GET_SYMBOL: couldn't find image!\n"));
        return VERR_INVALID_HANDLE;
    }
    pImage = pUsage->pImage;
    if (pImage->uState != SUP_IOCTL_LDR_LOAD)
    {
        unsigned uState = pImage->uState;
        RTSemFastMutexRelease(pDevExt->mtxLdr);
        Log(("SUP_IOCTL_LDR_GET_SYMBOL: invalid image state %d (%#x)!\n", uState, uState)); NOREF(uState);
        return VERR_ALREADY_LOADED;
    }

    /*
     * Search the symbol strings.
     */
    pchStrings = (const char *)((uint8_t *)pImage->pvImage + pImage->offStrTab);
    paSyms     =   (PSUPLDRSYM)((uint8_t *)pImage->pvImage + pImage->offSymbols);
    for (i = 0; i < pImage->cSymbols; i++)
    {
        if (    paSyms[i].offSymbol < pImage->cbImage /* paranoia */
            &&  paSyms[i].offName + cbSymbol <= pImage->cbStrTab
            &&  !memcmp(pchStrings + paSyms[i].offName, pReq->u.In.szSymbol, cbSymbol))
        {
            pvSymbol = (uint8_t *)pImage->pvImage + paSyms[i].offSymbol;
            rc = VINF_SUCCESS;
            break;
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxLdr);
    pReq->u.Out.pvSymbol = pvSymbol;
    return rc;
}


/**
 * Gets the address of a symbol in an open image or the support driver.
 *
 * @returns VINF_SUCCESS on success.
 * @returns
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pReq        The request buffer.
 */
static int supdrvIDC_LdrGetSymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPDRVIDCREQGETSYM pReq)
{
    int             rc = VINF_SUCCESS;
    const char     *pszSymbol = pReq->u.In.pszSymbol;
    const char     *pszModule = pReq->u.In.pszModule;
    size_t          cbSymbol;
    char const     *pszEnd;
    uint32_t        i;

    /*
     * Input validation.
     */
    AssertPtrReturn(pszSymbol, VERR_INVALID_POINTER);
    pszEnd = (char *)memchr(pszSymbol, '\0', 512);
    AssertReturn(pszEnd, VERR_INVALID_PARAMETER);
    cbSymbol = pszEnd - pszSymbol + 1;

    if (pszModule)
    {
        AssertPtrReturn(pszModule, VERR_INVALID_POINTER);
        pszEnd = (char *)memchr(pszModule, '\0', 64);
        AssertReturn(pszEnd, VERR_INVALID_PARAMETER);
    }
    Log3(("supdrvIDC_LdrGetSymbol: pszModule=%p:{%s} pszSymbol=%p:{%s}\n", pszModule, pszModule, pszSymbol, pszSymbol));


    if (    !pszModule
        ||  !strcmp(pszModule, "SupDrv"))
    {
        /*
         * Search the support driver export table.
         */
        for (i = 0; i < RT_ELEMENTS(g_aFunctions); i++)
            if (!strcmp(g_aFunctions[i].szName, pszSymbol))
            {
                pReq->u.Out.pfnSymbol = g_aFunctions[i].pfn;
                break;
            }
    }
    else
    {
        /*
         * Find the loader image.
         */
        PSUPDRVLDRIMAGE pImage;

        RTSemFastMutexRequest(pDevExt->mtxLdr);

        for (pImage = pDevExt->pLdrImages; pImage; pImage = pImage->pNext)
            if (!strcmp(pImage->szName, pszModule))
                break;
        if (pImage && pImage->uState == SUP_IOCTL_LDR_LOAD)
        {
            /*
             * Search the symbol strings.
             */
            const char *pchStrings = (const char *)((uint8_t *)pImage->pvImage + pImage->offStrTab);
            PCSUPLDRSYM paSyms     =  (PCSUPLDRSYM)((uint8_t *)pImage->pvImage + pImage->offSymbols);
            for (i = 0; i < pImage->cSymbols; i++)
            {
                if (    paSyms[i].offSymbol < pImage->cbImage /* paranoia */
                    &&  paSyms[i].offName + cbSymbol <= pImage->cbStrTab
                    &&  !memcmp(pchStrings + paSyms[i].offName, pszSymbol, cbSymbol))
                {
                    /*
                     * Found it! Calc the symbol address and add a reference to the module.
                     */
                    pReq->u.Out.pfnSymbol = (PFNRT)((uint8_t *)pImage->pvImage + paSyms[i].offSymbol);
                    rc = supdrvLdrAddUsage(pSession, pImage);
                    break;
                }
            }
        }
        else
            rc = pImage ? VERR_WRONG_ORDER : VERR_MODULE_NOT_FOUND;

        RTSemFastMutexRelease(pDevExt->mtxLdr);
    }
    return rc;
}


/**
 * Updates the VMMR0 entry point pointers.
 *
 * @returns IPRT status code.
 * @param   pDevExt             Device globals.
 * @param   pSession            Session data.
 * @param   pVMMR0              VMMR0 image handle.
 * @param   pvVMMR0EntryInt     VMMR0EntryInt address.
 * @param   pvVMMR0EntryFast    VMMR0EntryFast address.
 * @param   pvVMMR0EntryEx      VMMR0EntryEx address.
 * @remark  Caller must own the loader mutex.
 */
static int supdrvLdrSetVMMR0EPs(PSUPDRVDEVEXT pDevExt, void *pvVMMR0, void *pvVMMR0EntryInt, void *pvVMMR0EntryFast, void *pvVMMR0EntryEx)
{
    int rc = VINF_SUCCESS;
    LogFlow(("supdrvLdrSetR0EP pvVMMR0=%p pvVMMR0EntryInt=%p\n", pvVMMR0, pvVMMR0EntryInt));


    /*
     * Check if not yet set.
     */
    if (!pDevExt->pvVMMR0)
    {
        pDevExt->pvVMMR0            = pvVMMR0;
        pDevExt->pfnVMMR0EntryInt   = pvVMMR0EntryInt;
        pDevExt->pfnVMMR0EntryFast  = pvVMMR0EntryFast;
        pDevExt->pfnVMMR0EntryEx    = pvVMMR0EntryEx;
    }
    else
    {
        /*
         * Return failure or success depending on whether the values match or not.
         */
        if (    pDevExt->pvVMMR0 != pvVMMR0
            ||  (void *)pDevExt->pfnVMMR0EntryInt   != pvVMMR0EntryInt
            ||  (void *)pDevExt->pfnVMMR0EntryFast  != pvVMMR0EntryFast
            ||  (void *)pDevExt->pfnVMMR0EntryEx    != pvVMMR0EntryEx)
        {
            AssertMsgFailed(("SUP_IOCTL_LDR_SETR0EP: Already set pointing to a different module!\n"));
            rc = VERR_INVALID_PARAMETER;
        }
    }
    return rc;
}


/**
 * Unsets the VMMR0 entry point installed by supdrvLdrSetR0EP.
 *
 * @param   pDevExt     Device globals.
 */
static void supdrvLdrUnsetVMMR0EPs(PSUPDRVDEVEXT pDevExt)
{
    pDevExt->pvVMMR0            = NULL;
    pDevExt->pfnVMMR0EntryInt   = NULL;
    pDevExt->pfnVMMR0EntryFast  = NULL;
    pDevExt->pfnVMMR0EntryEx    = NULL;
}


/**
 * Adds a usage reference in the specified session of an image.
 *
 * Called while owning the loader semaphore.
 *
 * @returns VINF_SUCCESS on success and VERR_NO_MEMORY on failure.
 * @param   pSession    Session in question.
 * @param   pImage      Image which the session is using.
 */
static int supdrvLdrAddUsage(PSUPDRVSESSION pSession, PSUPDRVLDRIMAGE pImage)
{
    PSUPDRVLDRUSAGE pUsage;
    LogFlow(("supdrvLdrAddUsage: pImage=%p\n", pImage));

    /*
     * Referenced it already?
     */
    pUsage = pSession->pLdrUsage;
    while (pUsage)
    {
        if (pUsage->pImage == pImage)
        {
            pUsage->cUsage++;
            return VINF_SUCCESS;
        }
        pUsage = pUsage->pNext;
    }

    /*
     * Allocate new usage record.
     */
    pUsage = (PSUPDRVLDRUSAGE)RTMemAlloc(sizeof(*pUsage));
    AssertReturn(pUsage, VERR_NO_MEMORY);
    pUsage->cUsage = 1;
    pUsage->pImage = pImage;
    pUsage->pNext  = pSession->pLdrUsage;
    pSession->pLdrUsage = pUsage;
    return VINF_SUCCESS;
}


/**
 * Frees a load image.
 *
 * @param   pDevExt     Pointer to device extension.
 * @param   pImage      Pointer to the image we're gonna free.
 *                      This image must exit!
 * @remark  The caller MUST own SUPDRVDEVEXT::mtxLdr!
 */
static void supdrvLdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    PSUPDRVLDRIMAGE pImagePrev;
    LogFlow(("supdrvLdrFree: pImage=%p\n", pImage));

    /* find it - arg. should've used doubly linked list. */
    Assert(pDevExt->pLdrImages);
    pImagePrev = NULL;
    if (pDevExt->pLdrImages != pImage)
    {
        pImagePrev = pDevExt->pLdrImages;
        while (pImagePrev->pNext != pImage)
            pImagePrev = pImagePrev->pNext;
        Assert(pImagePrev->pNext == pImage);
    }

    /* unlink */
    if (pImagePrev)
        pImagePrev->pNext = pImage->pNext;
    else
        pDevExt->pLdrImages = pImage->pNext;

    /* check if this is VMMR0.r0 unset its entry point pointers. */
    if (pDevExt->pvVMMR0 == pImage->pvImage)
        supdrvLdrUnsetVMMR0EPs(pDevExt);

    /* check for objects with destructors in this image. (Shouldn't happen.) */
    if (pDevExt->pObjs)
    {
        unsigned        cObjs = 0;
        PSUPDRVOBJ      pObj;
        RTSPINLOCKTMP   SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
        RTSpinlockAcquire(pDevExt->Spinlock, &SpinlockTmp);
        for (pObj = pDevExt->pObjs; pObj; pObj = pObj->pNext)
            if (RT_UNLIKELY((uintptr_t)pObj->pfnDestructor - (uintptr_t)pImage->pvImage < pImage->cbImage))
            {
                pObj->pfnDestructor = NULL;
                cObjs++;
            }
        RTSpinlockRelease(pDevExt->Spinlock, &SpinlockTmp);
        if (cObjs)
            OSDBGPRINT(("supdrvLdrFree: Image '%s' has %d dangling objects!\n", pImage->szName, cObjs));
    }

    /* call termination function if fully loaded. */
    if (    pImage->pfnModuleTerm
        &&  pImage->uState == SUP_IOCTL_LDR_LOAD)
    {
        LogFlow(("supdrvIOCtl_LdrLoad: calling pfnModuleTerm=%p\n", pImage->pfnModuleTerm));
#ifdef RT_WITH_W64_UNWIND_HACK
        supdrvNtWrapModuleTerm(pImage->pfnModuleTerm);
#else
        pImage->pfnModuleTerm();
#endif
    }

    /* free the image */
    pImage->cUsage = 0;
    pImage->pNext  = 0;
    pImage->uState = SUP_IOCTL_LDR_FREE;
    RTMemExecFree(pImage);
}


/**
 * Implements the service call request.
 *
 * @returns VBox status code.
 * @param   pDevExt         The device extension.
 * @param   pSession        The calling session.
 * @param   pReq            The request packet, valid.
 */
static int supdrvIOCtl_CallServiceModule(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPCALLSERVICE pReq)
{
#if !defined(RT_OS_WINDOWS) || defined(DEBUG)
    int rc;

    /*
     * Find the module first in the module referenced by the calling session.
     */
    rc = RTSemFastMutexRequest(pDevExt->mtxLdr);
    if (RT_SUCCESS(rc))
    {
        PFNSUPR0SERVICEREQHANDLER   pfnServiceReqHandler = NULL;
        PSUPDRVLDRUSAGE             pUsage;

        for (pUsage = pSession->pLdrUsage; pUsage; pUsage = pUsage->pNext)
            if (    pUsage->pImage->pfnServiceReqHandler
                &&  !strcmp(pUsage->pImage->szName, pReq->u.In.szName))
            {
                pfnServiceReqHandler = pUsage->pImage->pfnServiceReqHandler;
                break;
            }
        RTSemFastMutexRelease(pDevExt->mtxLdr);

        if (pfnServiceReqHandler)
        {
            /*
             * Call it.
             */
            if (pReq->Hdr.cbIn == SUP_IOCTL_CALL_SERVICE_SIZE(0))
#ifdef RT_WITH_W64_UNWIND_HACK
                rc = supdrvNtWrapServiceReqHandler((PFNRT)pfnServiceReqHandler, pSession, pReq->u.In.uOperation, pReq->u.In.u64Arg, NULL);
#else
                rc = pfnServiceReqHandler(pSession, pReq->u.In.uOperation, pReq->u.In.u64Arg, NULL);
#endif
            else
#ifdef RT_WITH_W64_UNWIND_HACK
                rc = supdrvNtWrapServiceReqHandler((PFNRT)pfnServiceReqHandler, pSession, pReq->u.In.uOperation,
                                                   pReq->u.In.u64Arg, (PSUPR0SERVICEREQHDR)&pReq->abReqPkt[0]);
#else
                rc = pfnServiceReqHandler(pSession, pReq->u.In.uOperation, pReq->u.In.u64Arg, (PSUPR0SERVICEREQHDR)&pReq->abReqPkt[0]);
#endif
        }
        else
            rc = VERR_SUPDRV_SERVICE_NOT_FOUND;
    }

    /* log it */
    if (    RT_FAILURE(rc)
        &&  rc != VERR_INTERRUPTED
        &&  rc != VERR_TIMEOUT)
        Log(("SUP_IOCTL_CALL_SERVICE: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
             rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
    else
        Log4(("SUP_IOCTL_CALL_SERVICE: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
              rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
    return rc;
#else  /* RT_OS_WINDOWS && !DEBUG */
    return VERR_NOT_IMPLEMENTED;
#endif /* RT_OS_WINDOWS && !DEBUG */
}


/**
 * Implements the logger settings request.
 *
 * @returns VBox status code.
 * @param   pDevExt     The device extension.
 * @param   pSession    The caller's session.
 * @param   pReq        The request.
 */
static int supdrvIOCtl_LoggerSettings(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLOGGERSETTINGS pReq)
{
    const char *pszGroup = &pReq->u.In.szStrings[pReq->u.In.offGroups];
    const char *pszFlags = &pReq->u.In.szStrings[pReq->u.In.offFlags];
    const char *pszDest  = &pReq->u.In.szStrings[pReq->u.In.offDestination];
    PRTLOGGER   pLogger  = NULL;
    int         rc;

    /*
     * Some further validation.
     */
    switch (pReq->u.In.fWhat)
    {
        case SUPLOGGERSETTINGS_WHAT_SETTINGS:
        case SUPLOGGERSETTINGS_WHAT_CREATE:
            break;

        case SUPLOGGERSETTINGS_WHAT_DESTROY:
            if (*pszGroup || *pszFlags || *pszDest)
                return VERR_INVALID_PARAMETER;
            if (pReq->u.In.fWhich == SUPLOGGERSETTINGS_WHICH_RELEASE)
                return VERR_ACCESS_DENIED;
            break;

        default:
            return VERR_INTERNAL_ERROR;
    }

    /*
     * Get the logger.
     */
    switch (pReq->u.In.fWhich)
    {
        case SUPLOGGERSETTINGS_WHICH_DEBUG:
            pLogger = RTLogGetDefaultInstance();
            break;

        case SUPLOGGERSETTINGS_WHICH_RELEASE:
            pLogger = RTLogRelDefaultInstance();
            break;

        default:
            return VERR_INTERNAL_ERROR;
    }

    /*
     * Do the job.
     */
    switch (pReq->u.In.fWhat)
    {
        case SUPLOGGERSETTINGS_WHAT_SETTINGS:
            if (pLogger)
            {
                rc = RTLogFlags(pLogger, pszFlags);
                if (RT_SUCCESS(rc))
                    rc = RTLogGroupSettings(pLogger, pszGroup);
                NOREF(pszDest);
            }
            else
                rc = VERR_NOT_FOUND;
            break;

        case SUPLOGGERSETTINGS_WHAT_CREATE:
        {
            if (pLogger)
                rc = VERR_ALREADY_EXISTS;
            else
            {
                static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;

                rc = RTLogCreate(&pLogger,
                                 0 /* fFlags */,
                                 pszGroup,
                                 pReq->u.In.fWhich == SUPLOGGERSETTINGS_WHICH_DEBUG
                                 ? "VBOX_LOG"
                                 : "VBOX_RELEASE_LOG",
                                 RT_ELEMENTS(s_apszGroups),
                                 s_apszGroups,
                                 RTLOGDEST_STDOUT | RTLOGDEST_DEBUGGER,
                                 NULL);
                if (RT_SUCCESS(rc))
                {
                    rc = RTLogFlags(pLogger, pszFlags);
                    NOREF(pszDest);
                    if (RT_SUCCESS(rc))
                    {
                        switch (pReq->u.In.fWhich)
                        {
                            case SUPLOGGERSETTINGS_WHICH_DEBUG:
                                pLogger = RTLogSetDefaultInstance(pLogger);
                                break;
                            case SUPLOGGERSETTINGS_WHICH_RELEASE:
                                pLogger = RTLogRelSetDefaultInstance(pLogger);
                                break;
                        }
                    }
                    RTLogDestroy(pLogger);
                }
            }
            break;
        }

        case SUPLOGGERSETTINGS_WHAT_DESTROY:
            switch (pReq->u.In.fWhich)
            {
                case SUPLOGGERSETTINGS_WHICH_DEBUG:
                    pLogger = RTLogSetDefaultInstance(NULL);
                    break;
                case SUPLOGGERSETTINGS_WHICH_RELEASE:
                    pLogger = RTLogRelSetDefaultInstance(NULL);
                    break;
            }
            rc = RTLogDestroy(pLogger);
            break;

        default:
        {
            rc = VERR_INTERNAL_ERROR;
            break;
        }
    }

    return rc;
}


/**
 * Gets the paging mode of the current CPU.
 *
 * @returns Paging mode, SUPPAGEINGMODE_INVALID on error.
 */
SUPR0DECL(SUPPAGINGMODE) SUPR0GetPagingMode(void)
{
    SUPPAGINGMODE enmMode;

    RTR0UINTREG cr0 = ASMGetCR0();
    if ((cr0 & (X86_CR0_PG | X86_CR0_PE)) != (X86_CR0_PG | X86_CR0_PE))
        enmMode = SUPPAGINGMODE_INVALID;
    else
    {
        RTR0UINTREG cr4 = ASMGetCR4();
        uint32_t fNXEPlusLMA = 0;
        if (cr4 & X86_CR4_PAE)
        {
            uint32_t fAmdFeatures = ASMCpuId_EDX(0x80000001);
            if (fAmdFeatures & (X86_CPUID_AMD_FEATURE_EDX_NX | X86_CPUID_AMD_FEATURE_EDX_LONG_MODE))
            {
                uint64_t efer = ASMRdMsr(MSR_K6_EFER);
                if ((fAmdFeatures & X86_CPUID_AMD_FEATURE_EDX_NX)        && (efer & MSR_K6_EFER_NXE))
                    fNXEPlusLMA |= RT_BIT(0);
                if ((fAmdFeatures & X86_CPUID_AMD_FEATURE_EDX_LONG_MODE) && (efer & MSR_K6_EFER_LMA))
                    fNXEPlusLMA |= RT_BIT(1);
            }
        }

        switch ((cr4 & (X86_CR4_PAE | X86_CR4_PGE)) | fNXEPlusLMA)
        {
            case 0:
                enmMode = SUPPAGINGMODE_32_BIT;
                break;

            case X86_CR4_PGE:
                enmMode = SUPPAGINGMODE_32_BIT_GLOBAL;
                break;

            case X86_CR4_PAE:
                enmMode = SUPPAGINGMODE_PAE;
                break;

            case X86_CR4_PAE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_PAE_NX;
                break;

            case X86_CR4_PAE | X86_CR4_PGE:
                enmMode = SUPPAGINGMODE_PAE_GLOBAL;
                break;

            case X86_CR4_PAE | X86_CR4_PGE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_PAE_GLOBAL;
                break;

            case RT_BIT(1) | X86_CR4_PAE:
                enmMode = SUPPAGINGMODE_AMD64;
                break;

            case RT_BIT(1) | X86_CR4_PAE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_AMD64_NX;
                break;

            case RT_BIT(1) | X86_CR4_PAE | X86_CR4_PGE:
                enmMode = SUPPAGINGMODE_AMD64_GLOBAL;
                break;

            case RT_BIT(1) | X86_CR4_PAE | X86_CR4_PGE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_AMD64_GLOBAL_NX;
                break;

            default:
                AssertMsgFailed(("Cannot happen! cr4=%#x fNXEPlusLMA=%d\n", cr4, fNXEPlusLMA));
                enmMode = SUPPAGINGMODE_INVALID;
                break;
        }
    }
    return enmMode;
}


/**
 * Enables or disabled hardware virtualization extensions using native OS APIs.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_SUPPORTED if not supported by the native OS.
 *
 * @param   fEnable         Whether to enable or disable.
 */
SUPR0DECL(int) SUPR0EnableVTx(bool fEnable)
{
#ifdef RT_OS_DARWIN
    return supdrvOSEnableVTx(fEnable);
#else
    return VERR_NOT_SUPPORTED;
#endif
}


/**
 * Creates the GIP.
 *
 * @returns VBox status code.
 * @param   pDevExt     Instance data. GIP stuff may be updated.
 */
static int supdrvGipCreate(PSUPDRVDEVEXT pDevExt)
{
    PSUPGLOBALINFOPAGE pGip;
    RTHCPHYS HCPhysGip;
    uint32_t u32SystemResolution;
    uint32_t u32Interval;
    int rc;

    LogFlow(("supdrvGipCreate:\n"));

    /* assert order */
    Assert(pDevExt->u32SystemTimerGranularityGrant == 0);
    Assert(pDevExt->GipMemObj == NIL_RTR0MEMOBJ);
    Assert(!pDevExt->pGipTimer);

    /*
     * Allocate a suitable page with a default kernel mapping.
     */
    rc = RTR0MemObjAllocLow(&pDevExt->GipMemObj, PAGE_SIZE, false);
    if (RT_FAILURE(rc))
    {
        OSDBGPRINT(("supdrvGipCreate: failed to allocate the GIP page. rc=%d\n", rc));
        return rc;
    }
    pGip = (PSUPGLOBALINFOPAGE)RTR0MemObjAddress(pDevExt->GipMemObj); AssertPtr(pGip);
    HCPhysGip = RTR0MemObjGetPagePhysAddr(pDevExt->GipMemObj, 0); Assert(HCPhysGip != NIL_RTHCPHYS);

#if 0 /** @todo Disabled this as we didn't used to do it before and causes unnecessary stress on laptops.
       * It only applies to Windows and should probably revisited later, if possible made part of the
       * timer code (return min granularity in RTTimerGetSystemGranularity and set it in RTTimerStart). */
    /*
     * Try bump up the system timer resolution.
     * The more interrupts the better...
     */
    if (   RT_SUCCESS(RTTimerRequestSystemGranularity(  488281 /* 2048 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity(  500000 /* 2000 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity(  976563 /* 1024 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity( 1000000 /* 1000 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity( 1953125 /*  512 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity( 2000000 /*  500 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity( 3906250 /*  256 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity( 4000000 /*  250 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity( 7812500 /*  128 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity(10000000 /*  100 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity(15625000 /*   64 HZ */, &u32SystemResolution))
        || RT_SUCCESS(RTTimerRequestSystemGranularity(31250000 /*   32 HZ */, &u32SystemResolution))
       )
    {
        Assert(RTTimerGetSystemGranularity() <= u32SystemResolution);
        pDevExt->u32SystemTimerGranularityGrant = u32SystemResolution;
    }
#endif

    /*
     * Find a reasonable update interval and initialize the structure.
     */
    u32Interval = u32SystemResolution = RTTimerGetSystemGranularity();
    while (u32Interval < 10000000 /* 10 ms */)
        u32Interval += u32SystemResolution;

    supdrvGipInit(pDevExt, pGip, HCPhysGip, RTTimeSystemNanoTS(), 1000000000 / u32Interval /*=Hz*/);

    /*
     * Create the timer.
     * If CPU_ALL isn't supported we'll have to fall back to synchronous mode.
     */
    if (pGip->u32Mode == SUPGIPMODE_ASYNC_TSC)
    {
        rc = RTTimerCreateEx(&pDevExt->pGipTimer, u32Interval, RTTIMER_FLAGS_CPU_ALL, supdrvGipAsyncTimer, pDevExt);
        if (rc == VERR_NOT_SUPPORTED)
        {
            OSDBGPRINT(("supdrvGipCreate: omni timer not supported, falling back to synchronous mode\n"));
            pGip->u32Mode = SUPGIPMODE_SYNC_TSC;
        }
    }
    if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
        rc = RTTimerCreateEx(&pDevExt->pGipTimer, u32Interval, 0, supdrvGipSyncTimer, pDevExt);
    if (RT_SUCCESS(rc))
    {
        if (pGip->u32Mode == SUPGIPMODE_ASYNC_TSC)
            rc = RTMpNotificationRegister(supdrvGipMpEvent, pDevExt);
        if (RT_SUCCESS(rc))
        {
            /*
             * We're good.
             */
            dprintf(("supdrvGipCreate: %ld ns interval.\n", (long)u32Interval));
            return VINF_SUCCESS;
        }

        OSDBGPRINT(("supdrvGipCreate: failed register MP event notfication. rc=%d\n", rc));
    }
    else
    {
        OSDBGPRINT(("supdrvGipCreate: failed create GIP timer at %ld ns interval. rc=%d\n", (long)u32Interval, rc));
        Assert(!pDevExt->pGipTimer);
    }
    supdrvGipDestroy(pDevExt);
    return rc;
}


/**
 * Terminates the GIP.
 *
 * @param   pDevExt     Instance data. GIP stuff may be updated.
 */
static void supdrvGipDestroy(PSUPDRVDEVEXT pDevExt)
{
    int rc;
#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("supdrvGipDestroy: pDevExt=%p pGip=%p pGipTimer=%p GipMemObj=%p\n", pDevExt,
                pDevExt->GipMemObj != NIL_RTR0MEMOBJ ? RTR0MemObjAddress(pDevExt->GipMemObj) : NULL,
                pDevExt->pGipTimer, pDevExt->GipMemObj));
#endif

    /*
     * Invalid the GIP data.
     */
    if (pDevExt->pGip)
    {
        supdrvGipTerm(pDevExt->pGip);
        pDevExt->pGip = NULL;
    }

    /*
     * Destroy the timer and free the GIP memory object.
     */
    if (pDevExt->pGipTimer)
    {
        rc = RTTimerDestroy(pDevExt->pGipTimer); AssertRC(rc);
        pDevExt->pGipTimer = NULL;
    }

    if (pDevExt->GipMemObj != NIL_RTR0MEMOBJ)
    {
        rc = RTR0MemObjFree(pDevExt->GipMemObj, true /* free mappings */); AssertRC(rc);
        pDevExt->GipMemObj = NIL_RTR0MEMOBJ;
    }

    /*
     * Finally, release the system timer resolution request if one succeeded.
     */
    if (pDevExt->u32SystemTimerGranularityGrant)
    {
        rc = RTTimerReleaseSystemGranularity(pDevExt->u32SystemTimerGranularityGrant); AssertRC(rc);
        pDevExt->u32SystemTimerGranularityGrant = 0;
    }
}


/**
 * Timer callback function sync GIP mode.
 * @param   pTimer      The timer.
 * @param   pvUser      The device extension.
 */
static DECLCALLBACK(void) supdrvGipSyncTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick)
{
    RTCCUINTREG     fOldFlags = ASMIntDisableFlags(); /* No interruptions please (real problem on S10). */
    PSUPDRVDEVEXT   pDevExt   = (PSUPDRVDEVEXT)pvUser;

    supdrvGipUpdate(pDevExt->pGip, RTTimeSystemNanoTS());

    ASMSetFlags(fOldFlags);
}


/**
 * Timer callback function for async GIP mode.
 * @param   pTimer      The timer.
 * @param   pvUser      The device extension.
 */
static DECLCALLBACK(void) supdrvGipAsyncTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick)
{
    RTCCUINTREG     fOldFlags = ASMIntDisableFlags(); /* No interruptions please (real problem on S10). */
    PSUPDRVDEVEXT   pDevExt   = (PSUPDRVDEVEXT)pvUser;
    RTCPUID         idCpu     = RTMpCpuId();
    uint64_t        NanoTS    = RTTimeSystemNanoTS();

    /** @todo reset the transaction number and whatnot when iTick == 1. */
    if (pDevExt->idGipMaster == idCpu)
        supdrvGipUpdate(pDevExt->pGip, NanoTS);
    else
        supdrvGipUpdatePerCpu(pDevExt->pGip, NanoTS, ASMGetApicId());

    ASMSetFlags(fOldFlags);
}


/**
 * Multiprocessor event notification callback.
 *
 * This is used to make sue that the GIP master gets passed on to
 * another CPU.
 *
 * @param   enmEvent    The event.
 * @param   idCpu       The cpu it applies to.
 * @param   pvUser      Pointer to the device extension.
 */
static DECLCALLBACK(void) supdrvGipMpEvent(RTMPEVENT enmEvent, RTCPUID idCpu, void *pvUser)
{
    PSUPDRVDEVEXT   pDevExt = (PSUPDRVDEVEXT)pvUser;
    if (enmEvent == RTMPEVENT_OFFLINE)
    {
        RTCPUID idGipMaster;
        ASMAtomicReadSize(&pDevExt->idGipMaster, &idGipMaster);
        if (idGipMaster == idCpu)
        {
            /*
             * Find a new GIP master.
             */
            bool        fIgnored;
            unsigned    i;
            RTCPUID     idNewGipMaster = NIL_RTCPUID;
            RTCPUSET    OnlineCpus;
            RTMpGetOnlineSet(&OnlineCpus);

            for (i = 0; i < RTCPUSET_MAX_CPUS; i++)
            {
                RTCPUID idCurCpu = RTMpCpuIdFromSetIndex(i);
                if (    RTCpuSetIsMember(&OnlineCpus, idCurCpu)
                    &&  idCurCpu != idGipMaster)
                {
                    idNewGipMaster = idCurCpu;
                    break;
                }
            }

            dprintf(("supdrvGipMpEvent: Gip master %#lx -> %#lx\n", (long)idGipMaster, (long)idNewGipMaster));
            ASMAtomicCmpXchgSize(&pDevExt->idGipMaster, idNewGipMaster, idGipMaster, fIgnored);
            NOREF(fIgnored);
        }
    }
}


/**
 * Initializes the GIP data.
 *
 * @returns IPRT status code.
 * @param   pDevExt     Pointer to the device instance data.
 * @param   pGip        Pointer to the read-write kernel mapping of the GIP.
 * @param   HCPhys      The physical address of the GIP.
 * @param   u64NanoTS   The current nanosecond timestamp.
 * @param   uUpdateHz   The update freqence.
 */
int VBOXCALL supdrvGipInit(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE pGip, RTHCPHYS HCPhys, uint64_t u64NanoTS, unsigned uUpdateHz)
{
    unsigned i;
#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("supdrvGipInit: pGip=%p HCPhys=%lx u64NanoTS=%llu uUpdateHz=%d\n", pGip, (long)HCPhys, u64NanoTS, uUpdateHz));
#else
    LogFlow(("supdrvGipInit: pGip=%p HCPhys=%lx u64NanoTS=%llu uUpdateHz=%d\n", pGip, (long)HCPhys, u64NanoTS, uUpdateHz));
#endif

    /*
     * Initialize the structure.
     */
    memset(pGip, 0, PAGE_SIZE);
    pGip->u32Magic          = SUPGLOBALINFOPAGE_MAGIC;
    pGip->u32Version        = SUPGLOBALINFOPAGE_VERSION;
    pGip->u32Mode           = supdrvGipDeterminTscMode(pDevExt);
    pGip->u32UpdateHz       = uUpdateHz;
    pGip->u32UpdateIntervalNS = 1000000000 / uUpdateHz;
    pGip->u64NanoTSLastUpdateHz = u64NanoTS;

    for (i = 0; i < RT_ELEMENTS(pGip->aCPUs); i++)
    {
        pGip->aCPUs[i].u32TransactionId  = 2;
        pGip->aCPUs[i].u64NanoTS         = u64NanoTS;
        pGip->aCPUs[i].u64TSC            = ASMReadTSC();

        /*
         * We don't know the following values until we've executed updates.
         * So, we'll just insert very high values.
         */
        pGip->aCPUs[i].u64CpuHz          = _4G + 1;
        pGip->aCPUs[i].u32UpdateIntervalTSC = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[0] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[1] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[2] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[3] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[4] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[5] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[6] = _2G / 4;
        pGip->aCPUs[i].au32TSCHistory[7] = _2G / 4;
    }

    /*
     * Link it to the device extension.
     */
    pDevExt->pGip = pGip;
    pDevExt->HCPhysGip = HCPhys;
    pDevExt->cGipUsers = 0;

    return VINF_SUCCESS;
}


/**
 * Callback used by supdrvDetermineAsyncTSC to read the TSC on a CPU.
 *
 * @param   idCpu       Ignored.
 * @param   pvUser1     Where to put the TSC.
 * @param   pvUser2     Ignored.
 */
static DECLCALLBACK(void) supdrvDetermineAsyncTscWorker(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
#if 1
    ASMAtomicWriteU64((uint64_t volatile *)pvUser1, ASMReadTSC());
#else
    *(uint64_t *)pvUser1 = ASMReadTSC();
#endif
}


/**
 * Determine if Async GIP mode is required because of TSC drift.
 *
 * When using the default/normal timer code it is essential that the time stamp counter
 * (TSC) runs never backwards, that is, a read operation to the counter should return
 * a bigger value than any previous read operation. This is guaranteed by the latest
 * AMD CPUs and by newer Intel CPUs which never enter the C2 state (P4). In any other
 * case we have to choose the asynchronous timer mode.
 *
 * @param   poffMin     Pointer to the determined difference between different cores.
 * @return  false if the time stamp counters appear to be synchron, true otherwise.
 */
bool VBOXCALL supdrvDetermineAsyncTsc(uint64_t *poffMin)
{
    /*
     * Just iterate all the cpus 8 times and make sure that the TSC is
     * ever increasing. We don't bother taking TSC rollover into account.
     */
    RTCPUSET    CpuSet;
    int         iLastCpu = RTCpuLastIndex(RTMpGetSet(&CpuSet));
    int         iCpu;
    int         cLoops = 8;
    bool        fAsync = false;
    int         rc = VINF_SUCCESS;
    uint64_t    offMax = 0;
    uint64_t    offMin = ~(uint64_t)0;
    uint64_t    PrevTsc = ASMReadTSC();

    while (cLoops-- > 0)
    {
        for (iCpu = 0; iCpu <= iLastCpu; iCpu++)
        {
            uint64_t CurTsc;
            rc = RTMpOnSpecific(RTMpCpuIdFromSetIndex(iCpu), supdrvDetermineAsyncTscWorker, &CurTsc, NULL);
            if (RT_SUCCESS(rc))
            {
                if (CurTsc <= PrevTsc)
                {
                    fAsync = true;
                    offMin = offMax = PrevTsc - CurTsc;
                    dprintf(("supdrvDetermineAsyncTsc: iCpu=%d cLoops=%d CurTsc=%llx PrevTsc=%llx\n",
                             iCpu, cLoops, CurTsc, PrevTsc));
                    break;
                }

                /* Gather statistics (except the first time). */
                if (iCpu != 0 || cLoops != 7)
                {
                    uint64_t off = CurTsc - PrevTsc;
                    if (off < offMin)
                        offMin = off;
                    if (off > offMax)
                        offMax = off;
                    dprintf2(("%d/%d: off=%llx\n", cLoops, iCpu, off));
                }

                /* Next */
                PrevTsc = CurTsc;
            }
            else if (rc == VERR_NOT_SUPPORTED)
                break;
            else
                AssertMsg(rc == VERR_CPU_NOT_FOUND || rc == VERR_CPU_OFFLINE, ("%d\n", rc));
        }

        /* broke out of the loop. */
        if (iCpu <= iLastCpu)
            break;
    }

    *poffMin = offMin; /* Almost RTMpOnSpecific profiling. */
    dprintf(("supdrvDetermineAsyncTsc: returns %d; iLastCpu=%d rc=%d offMin=%llx offMax=%llx\n",
             fAsync, iLastCpu, rc, offMin, offMax));
#if !defined(RT_OS_SOLARIS) && !defined(RT_OS_OS2) && !defined(RT_OS_WINDOWS)
    OSDBGPRINT(("vboxdrv: fAsync=%d offMin=%#lx offMax=%#lx\n", fAsync, (long)offMin, (long)offMax));
#endif
    return fAsync;
}


/**
 * Determin the GIP TSC mode.
 *
 * @returns The most suitable TSC mode.
 * @param   pDevExt     Pointer to the device instance data.
 */
static SUPGIPMODE supdrvGipDeterminTscMode(PSUPDRVDEVEXT pDevExt)
{
    /*
     * On SMP we're faced with two problems:
     *      (1) There might be a skew between the CPU, so that cpu0
     *          returns a TSC that is sligtly different from cpu1.
     *      (2) Power management (and other things) may cause the TSC
     *          to run at a non-constant speed, and cause the speed
     *          to be different on the cpus. This will result in (1).
     *
     * So, on SMP systems we'll have to select the ASYNC update method
     * if there are symphoms of these problems.
     */
    if (RTMpGetCount() > 1)
    {
        uint32_t uEAX, uEBX, uECX, uEDX;
        uint64_t u64DiffCoresIgnored;

        /* Permit the user and/or the OS specfic bits to force async mode. */
        if (supdrvOSGetForcedAsyncTscMode(pDevExt))
            return SUPGIPMODE_ASYNC_TSC;

        /* Try check for current differences between the cpus. */
        if (supdrvDetermineAsyncTsc(&u64DiffCoresIgnored))
            return SUPGIPMODE_ASYNC_TSC;

        /*
         * If the CPU supports power management and is an AMD one we
         * won't trust it unless it has the TscInvariant bit is set.
         */
        /* Check for "AuthenticAMD" */
        ASMCpuId(0, &uEAX, &uEBX, &uECX, &uEDX);
        if (    uEAX >= 1
            &&  uEBX == X86_CPUID_VENDOR_AMD_EBX
            &&  uECX == X86_CPUID_VENDOR_AMD_ECX
            &&  uEDX == X86_CPUID_VENDOR_AMD_EDX)
        {
            /* Check for APM support and that TscInvariant is cleared. */
            ASMCpuId(0x80000000, &uEAX, &uEBX, &uECX, &uEDX);
            if (uEAX >= 0x80000007)
            {
                ASMCpuId(0x80000007, &uEAX, &uEBX, &uECX, &uEDX);
                if (    !(uEDX & RT_BIT(8))/* TscInvariant */
                    &&  (uEDX & 0x3e))  /* STC|TM|THERMTRIP|VID|FID. Ignore TS. */
                    return SUPGIPMODE_ASYNC_TSC;
            }
        }
    }
    return SUPGIPMODE_SYNC_TSC;
}


/**
 * Invalidates the GIP data upon termination.
 *
 * @param   pGip        Pointer to the read-write kernel mapping of the GIP.
 */
void VBOXCALL supdrvGipTerm(PSUPGLOBALINFOPAGE pGip)
{
    unsigned i;
    pGip->u32Magic = 0;
    for (i = 0; i < RT_ELEMENTS(pGip->aCPUs); i++)
    {
        pGip->aCPUs[i].u64NanoTS = 0;
        pGip->aCPUs[i].u64TSC = 0;
        pGip->aCPUs[i].iTSCHistoryHead = 0;
    }
}


/**
 * Worker routine for supdrvGipUpdate and supdrvGipUpdatePerCpu that
 * updates all the per cpu data except the transaction id.
 *
 * @param   pGip            The GIP.
 * @param   pGipCpu         Pointer to the per cpu data.
 * @param   u64NanoTS       The current time stamp.
 */
static void supdrvGipDoUpdateCpu(PSUPGLOBALINFOPAGE pGip, PSUPGIPCPU pGipCpu, uint64_t u64NanoTS)
{
    uint64_t    u64TSC;
    uint64_t    u64TSCDelta;
    uint32_t    u32UpdateIntervalTSC;
    uint32_t    u32UpdateIntervalTSCSlack;
    unsigned    iTSCHistoryHead;
    uint64_t    u64CpuHz;

    /*
     * Update the NanoTS.
     */
    ASMAtomicXchgU64(&pGipCpu->u64NanoTS, u64NanoTS);

    /*
     * Calc TSC delta.
     */
    /** @todo validate the NanoTS delta, don't trust the OS to call us when it should... */
    u64TSC = ASMReadTSC();
    u64TSCDelta = u64TSC - pGipCpu->u64TSC;
    ASMAtomicXchgU64(&pGipCpu->u64TSC, u64TSC);

    if (u64TSCDelta >> 32)
    {
        u64TSCDelta = pGipCpu->u32UpdateIntervalTSC;
        pGipCpu->cErrors++;
    }

    /*
     * TSC History.
     */
    Assert(RT_ELEMENTS(pGipCpu->au32TSCHistory) == 8);

    iTSCHistoryHead = (pGipCpu->iTSCHistoryHead + 1) & 7;
    ASMAtomicXchgU32(&pGipCpu->iTSCHistoryHead, iTSCHistoryHead);
    ASMAtomicXchgU32(&pGipCpu->au32TSCHistory[iTSCHistoryHead], (uint32_t)u64TSCDelta);

    /*
     * UpdateIntervalTSC = average of last 8,2,1 intervals depending on update HZ.
     */
    if (pGip->u32UpdateHz >= 1000)
    {
        uint32_t u32;
        u32  = pGipCpu->au32TSCHistory[0];
        u32 += pGipCpu->au32TSCHistory[1];
        u32 += pGipCpu->au32TSCHistory[2];
        u32 += pGipCpu->au32TSCHistory[3];
        u32 >>= 2;
        u32UpdateIntervalTSC  = pGipCpu->au32TSCHistory[4];
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[5];
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[6];
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[7];
        u32UpdateIntervalTSC >>= 2;
        u32UpdateIntervalTSC += u32;
        u32UpdateIntervalTSC >>= 1;

        /* Value choosen for a 2GHz Athlon64 running linux 2.6.10/11, . */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 14;
    }
    else if (pGip->u32UpdateHz >= 90)
    {
        u32UpdateIntervalTSC  = (uint32_t)u64TSCDelta;
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[(iTSCHistoryHead - 1) & 7];
        u32UpdateIntervalTSC >>= 1;

        /* value choosen on a 2GHz thinkpad running windows */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 7;
    }
    else
    {
        u32UpdateIntervalTSC  = (uint32_t)u64TSCDelta;

        /* This value hasn't be checked yet.. waiting for OS/2 and 33Hz timers.. :-) */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 6;
    }
    ASMAtomicXchgU32(&pGipCpu->u32UpdateIntervalTSC, u32UpdateIntervalTSC + u32UpdateIntervalTSCSlack);

    /*
     * CpuHz.
     */
    u64CpuHz = ASMMult2xU32RetU64(u32UpdateIntervalTSC, pGip->u32UpdateHz);
    ASMAtomicXchgU64(&pGipCpu->u64CpuHz, u64CpuHz);
}


/**
 * Updates the GIP.
 *
 * @param   pGip        Pointer to the GIP.
 * @param   u64NanoTS   The current nanosecond timesamp.
 */
void VBOXCALL supdrvGipUpdate(PSUPGLOBALINFOPAGE pGip, uint64_t u64NanoTS)
{
    /*
     * Determin the relevant CPU data.
     */
    PSUPGIPCPU pGipCpu;
    if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
        pGipCpu = &pGip->aCPUs[0];
    else
    {
        unsigned iCpu = ASMGetApicId();
        if (RT_LIKELY(iCpu >= RT_ELEMENTS(pGip->aCPUs)))
            return;
        pGipCpu = &pGip->aCPUs[iCpu];
    }

    /*
     * Start update transaction.
     */
    if (!(ASMAtomicIncU32(&pGipCpu->u32TransactionId) & 1))
    {
        /* this can happen on win32 if we're taking to long and there are more CPUs around. shouldn't happen though. */
        AssertMsgFailed(("Invalid transaction id, %#x, not odd!\n", pGipCpu->u32TransactionId));
        ASMAtomicIncU32(&pGipCpu->u32TransactionId);
        pGipCpu->cErrors++;
        return;
    }

    /*
     * Recalc the update frequency every 0x800th time.
     */
    if (!(pGipCpu->u32TransactionId & (GIP_UPDATEHZ_RECALC_FREQ * 2 - 2)))
    {
        if (pGip->u64NanoTSLastUpdateHz)
        {
#ifdef RT_ARCH_AMD64 /** @todo fix 64-bit div here to work on x86 linux. */
            uint64_t u64Delta = u64NanoTS - pGip->u64NanoTSLastUpdateHz;
            uint32_t u32UpdateHz = (uint32_t)((UINT64_C(1000000000) * GIP_UPDATEHZ_RECALC_FREQ) / u64Delta);
            if (u32UpdateHz <= 2000 && u32UpdateHz >= 30)
            {
                ASMAtomicXchgU32(&pGip->u32UpdateHz, u32UpdateHz);
                ASMAtomicXchgU32(&pGip->u32UpdateIntervalNS, 1000000000 / u32UpdateHz);
            }
#endif
        }
        ASMAtomicXchgU64(&pGip->u64NanoTSLastUpdateHz, u64NanoTS);
    }

    /*
     * Update the data.
     */
    supdrvGipDoUpdateCpu(pGip, pGipCpu, u64NanoTS);

    /*
     * Complete transaction.
     */
    ASMAtomicIncU32(&pGipCpu->u32TransactionId);
}


/**
 * Updates the per cpu GIP data for the calling cpu.
 *
 * @param   pGip        Pointer to the GIP.
 * @param   u64NanoTS   The current nanosecond timesamp.
 * @param   iCpu        The CPU index.
 */
void VBOXCALL supdrvGipUpdatePerCpu(PSUPGLOBALINFOPAGE pGip, uint64_t u64NanoTS, unsigned iCpu)
{
    PSUPGIPCPU  pGipCpu;

    if (RT_LIKELY(iCpu < RT_ELEMENTS(pGip->aCPUs)))
    {
        pGipCpu = &pGip->aCPUs[iCpu];

        /*
         * Start update transaction.
         */
        if (!(ASMAtomicIncU32(&pGipCpu->u32TransactionId) & 1))
        {
            AssertMsgFailed(("Invalid transaction id, %#x, not odd!\n", pGipCpu->u32TransactionId));
            ASMAtomicIncU32(&pGipCpu->u32TransactionId);
            pGipCpu->cErrors++;
            return;
        }

        /*
         * Update the data.
         */
        supdrvGipDoUpdateCpu(pGip, pGipCpu, u64NanoTS);

        /*
         * Complete transaction.
         */
        ASMAtomicIncU32(&pGipCpu->u32TransactionId);
    }
}

