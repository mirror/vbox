/* $Revision$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Common code.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
#define SUPDRV_AGNOSTIC
#include "SUPDrvInternal.h"
#ifndef PAGE_SHIFT
# include <iprt/param.h>
#endif
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/asm-math.h>
#include <iprt/cpuset.h>
#include <iprt/handletable.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/power.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>
#include <iprt/net.h>
#include <iprt/crc.h>
#include <iprt/string.h>
#include <iprt/timer.h>
#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
# include <iprt/rand.h>
# include <iprt/path.h>
#endif
#include <iprt/x86.h>

#include <VBox/param.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/vmm/hwacc_svm.h>
#include <VBox/vmm/hwacc_vmx.h>

#if defined(RT_OS_SOLARIS) || defined(RT_OS_DARWIN)
# include "dtrace/SUPDrv.h"
#else
# define VBOXDRV_SESSION_CREATE(pvSession, fUser) do { } while (0)
# define VBOXDRV_SESSION_CLOSE(pvSession) do { } while (0)
# define VBOXDRV_IOCTL_ENTRY(pvSession, uIOCtl, pvReqHdr) do { } while (0)
# define VBOXDRV_IOCTL_RETURN(pvSession, uIOCtl, pvReqHdr, rcRet, rcReq) do { } while (0)
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
 * instantiation in log-vbox.c(pp).
 */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The frequency by which we recalculate the u32UpdateHz and
 * u32UpdateIntervalNS GIP members. The value must be a power of 2. */
#define GIP_UPDATEHZ_RECALC_FREQ            0x800

/** @def VBOX_SVN_REV
 * The makefile should define this if it can. */
#ifndef VBOX_SVN_REV
# define VBOX_SVN_REV 0
#endif

#if 0 /* Don't start the GIP timers. Useful when debugging the IPRT timer code. */
# define DO_NOT_START_GIP
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int)    supdrvSessionObjHandleRetain(RTHANDLETABLE hHandleTable, void *pvObj, void *pvCtx, void *pvUser);
static DECLCALLBACK(void)   supdrvSessionObjHandleDelete(RTHANDLETABLE hHandleTable, uint32_t h, void *pvObj, void *pvCtx, void *pvUser);
static int                  supdrvMemAdd(PSUPDRVMEMREF pMem, PSUPDRVSESSION pSession);
static int                  supdrvMemRelease(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, SUPDRVMEMREFTYPE eType);
static int                  supdrvIOCtl_LdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDROPEN pReq);
static int                  supdrvIOCtl_LdrLoad(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRLOAD pReq);
static int                  supdrvIOCtl_LdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRFREE pReq);
static int                  supdrvIOCtl_LdrGetSymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRGETSYMBOL pReq);
static int                  supdrvIDC_LdrGetSymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPDRVIDCREQGETSYM pReq);
static int                  supdrvLdrSetVMMR0EPs(PSUPDRVDEVEXT pDevExt, void *pvVMMR0, void *pvVMMR0EntryInt, void *pvVMMR0EntryFast, void *pvVMMR0EntryEx);
static void                 supdrvLdrUnsetVMMR0EPs(PSUPDRVDEVEXT pDevExt);
static int                  supdrvLdrAddUsage(PSUPDRVSESSION pSession, PSUPDRVLDRIMAGE pImage);
static void                 supdrvLdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage);
DECLINLINE(int)             supdrvLdrLock(PSUPDRVDEVEXT pDevExt);
DECLINLINE(int)             supdrvLdrUnlock(PSUPDRVDEVEXT pDevExt);
static int                  supdrvIOCtl_CallServiceModule(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPCALLSERVICE pReq);
static int                  supdrvIOCtl_LoggerSettings(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLOGGERSETTINGS pReq);
static int                  supdrvGipCreate(PSUPDRVDEVEXT pDevExt);
static void                 supdrvGipDestroy(PSUPDRVDEVEXT pDevExt);
static DECLCALLBACK(void)   supdrvGipSyncTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick);
static DECLCALLBACK(void)   supdrvGipAsyncTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick);
static DECLCALLBACK(void)   supdrvGipMpEvent(RTMPEVENT enmEvent, RTCPUID idCpu, void *pvUser);
static void                 supdrvGipInit(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE pGip, RTHCPHYS HCPhys,
                                          uint64_t u64NanoTS, unsigned uUpdateHz, unsigned cCpus);
static DECLCALLBACK(void)   supdrvGipInitOnCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static void                 supdrvGipTerm(PSUPGLOBALINFOPAGE pGip);
static void                 supdrvGipUpdate(PSUPGLOBALINFOPAGE pGip, uint64_t u64NanoTS, uint64_t u64TSC, RTCPUID idCpu, uint64_t iTick);
static void                 supdrvGipUpdatePerCpu(PSUPGLOBALINFOPAGE pGip, uint64_t u64NanoTS, uint64_t u64TSC,
                                                  RTCPUID idCpu, uint8_t idApic, uint64_t iTick);
static void                 supdrvGipInitCpu(PSUPGLOBALINFOPAGE pGip, PSUPGIPCPU pCpu, uint64_t u64NanoTS);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
DECLEXPORT(PSUPGLOBALINFOPAGE) g_pSUPGlobalInfoPage = NULL;

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
    { "SUPR0VtgFireProbe",                      (void *)SUPR0VtgFireProbe },
        /* Normal function pointers: */
    { "SUPR0ComponentRegisterFactory",          (void *)SUPR0ComponentRegisterFactory },
    { "SUPR0ComponentDeregisterFactory",        (void *)SUPR0ComponentDeregisterFactory },
    { "SUPR0ComponentQueryFactory",             (void *)SUPR0ComponentQueryFactory },
    { "SUPR0ObjRegister",                       (void *)SUPR0ObjRegister },
    { "SUPR0ObjAddRef",                         (void *)SUPR0ObjAddRef },
    { "SUPR0ObjAddRefEx",                       (void *)SUPR0ObjAddRefEx },
    { "SUPR0ObjRelease",                        (void *)SUPR0ObjRelease },
    { "SUPR0ObjVerifyAccess",                   (void *)SUPR0ObjVerifyAccess },
    { "SUPR0LockMem",                           (void *)SUPR0LockMem },
    { "SUPR0UnlockMem",                         (void *)SUPR0UnlockMem },
    { "SUPR0ContAlloc",                         (void *)SUPR0ContAlloc },
    { "SUPR0ContFree",                          (void *)SUPR0ContFree },
    { "SUPR0LowAlloc",                          (void *)SUPR0LowAlloc },
    { "SUPR0LowFree",                           (void *)SUPR0LowFree },
    { "SUPR0MemAlloc",                          (void *)SUPR0MemAlloc },
    { "SUPR0MemGetPhys",                        (void *)SUPR0MemGetPhys },
    { "SUPR0MemFree",                           (void *)SUPR0MemFree },
    { "SUPR0PageAllocEx",                       (void *)SUPR0PageAllocEx },
    { "SUPR0PageFree",                          (void *)SUPR0PageFree },
    { "SUPR0Printf",                            (void *)SUPR0Printf }, /** @todo needs wrapping? */
    { "SUPSemEventCreate",                      (void *)SUPSemEventCreate },
    { "SUPSemEventClose",                       (void *)SUPSemEventClose },
    { "SUPSemEventSignal",                      (void *)SUPSemEventSignal },
    { "SUPSemEventWait",                        (void *)SUPSemEventWait },
    { "SUPSemEventWaitNoResume",                (void *)SUPSemEventWaitNoResume },
    { "SUPSemEventWaitNsAbsIntr",               (void *)SUPSemEventWaitNsAbsIntr },
    { "SUPSemEventWaitNsRelIntr",               (void *)SUPSemEventWaitNsRelIntr },
    { "SUPSemEventGetResolution",               (void *)SUPSemEventGetResolution },
    { "SUPSemEventMultiCreate",                 (void *)SUPSemEventMultiCreate },
    { "SUPSemEventMultiClose",                  (void *)SUPSemEventMultiClose },
    { "SUPSemEventMultiSignal",                 (void *)SUPSemEventMultiSignal },
    { "SUPSemEventMultiReset",                  (void *)SUPSemEventMultiReset },
    { "SUPSemEventMultiWait",                   (void *)SUPSemEventMultiWait },
    { "SUPSemEventMultiWaitNoResume",           (void *)SUPSemEventMultiWaitNoResume },
    { "SUPSemEventMultiWaitNsAbsIntr",          (void *)SUPSemEventMultiWaitNsAbsIntr },
    { "SUPSemEventMultiWaitNsRelIntr",          (void *)SUPSemEventMultiWaitNsRelIntr },
    { "SUPSemEventMultiGetResolution",          (void *)SUPSemEventMultiGetResolution },
    { "SUPR0GetPagingMode",                     (void *)SUPR0GetPagingMode },
    { "SUPR0EnableVTx",                         (void *)SUPR0EnableVTx },
    { "SUPR0VtgRegisterModule",                 (void *)SUPR0VtgRegisterModule },
    { "SUPGetGIP",                              (void *)SUPGetGIP },
    { "g_pSUPGlobalInfoPage",                   (void *)&g_pSUPGlobalInfoPage },
    { "RTMemAllocTag",                          (void *)RTMemAllocTag },
    { "RTMemAllocZTag",                         (void *)RTMemAllocZTag },
    { "RTMemAllocVarTag",                       (void *)RTMemAllocVarTag },
    { "RTMemAllocZVarTag",                      (void *)RTMemAllocZVarTag },
    { "RTMemFree",                              (void *)RTMemFree },
    { "RTMemDupTag",                            (void *)RTMemDupTag },
    { "RTMemDupExTag",                          (void *)RTMemDupExTag },
    { "RTMemReallocTag",                        (void *)RTMemReallocTag },
    { "RTR0MemObjAllocLowTag",                  (void *)RTR0MemObjAllocLowTag },
    { "RTR0MemObjAllocPageTag",                 (void *)RTR0MemObjAllocPageTag },
    { "RTR0MemObjAllocPhysTag",                 (void *)RTR0MemObjAllocPhysTag },
    { "RTR0MemObjAllocPhysExTag",               (void *)RTR0MemObjAllocPhysExTag },
    { "RTR0MemObjAllocPhysNCTag",               (void *)RTR0MemObjAllocPhysNCTag },
    { "RTR0MemObjAllocContTag",                 (void *)RTR0MemObjAllocContTag },
    { "RTR0MemObjEnterPhysTag",                 (void *)RTR0MemObjEnterPhysTag },
    { "RTR0MemObjLockUserTag",                  (void *)RTR0MemObjLockUserTag },
    { "RTR0MemObjMapKernelTag",                 (void *)RTR0MemObjMapKernelTag },
    { "RTR0MemObjMapKernelExTag",               (void *)RTR0MemObjMapKernelExTag },
    { "RTR0MemObjMapUserTag",                   (void *)RTR0MemObjMapUserTag },
    { "RTR0MemObjProtect",                      (void *)RTR0MemObjProtect },
    { "RTR0MemObjAddress",                      (void *)RTR0MemObjAddress },
    { "RTR0MemObjAddressR3",                    (void *)RTR0MemObjAddressR3 },
    { "RTR0MemObjSize",                         (void *)RTR0MemObjSize },
    { "RTR0MemObjIsMapping",                    (void *)RTR0MemObjIsMapping },
    { "RTR0MemObjGetPagePhysAddr",              (void *)RTR0MemObjGetPagePhysAddr },
    { "RTR0MemObjFree",                         (void *)RTR0MemObjFree },
    { "RTR0MemUserCopyFrom",                    (void *)RTR0MemUserCopyFrom },
    { "RTR0MemUserCopyTo",                      (void *)RTR0MemUserCopyTo },
    { "RTR0MemUserIsValidAddr",                 (void *)RTR0MemUserIsValidAddr },
    { "RTR0MemKernelIsValidAddr",               (void *)RTR0MemKernelIsValidAddr },
    { "RTR0MemAreKrnlAndUsrDifferent",          (void *)RTR0MemAreKrnlAndUsrDifferent },
    { "RTSemMutexCreate",                       (void *)RTSemMutexCreate },
    { "RTSemMutexRequest",                      (void *)RTSemMutexRequest },
    { "RTSemMutexRequestDebug",                 (void *)RTSemMutexRequestDebug },
    { "RTSemMutexRequestNoResume",              (void *)RTSemMutexRequestNoResume },
    { "RTSemMutexRequestNoResumeDebug",         (void *)RTSemMutexRequestNoResumeDebug },
    { "RTSemMutexRelease",                      (void *)RTSemMutexRelease },
    { "RTSemMutexDestroy",                      (void *)RTSemMutexDestroy },
    { "RTProcSelf",                             (void *)RTProcSelf },
    { "RTR0ProcHandleSelf",                     (void *)RTR0ProcHandleSelf },
    { "RTSemFastMutexCreate",                   (void *)RTSemFastMutexCreate },
    { "RTSemFastMutexDestroy",                  (void *)RTSemFastMutexDestroy },
    { "RTSemFastMutexRequest",                  (void *)RTSemFastMutexRequest },
    { "RTSemFastMutexRelease",                  (void *)RTSemFastMutexRelease },
    { "RTSemEventCreate",                       (void *)RTSemEventCreate },
    { "RTSemEventSignal",                       (void *)RTSemEventSignal },
    { "RTSemEventWait",                         (void *)RTSemEventWait },
    { "RTSemEventWaitNoResume",                 (void *)RTSemEventWaitNoResume },
    { "RTSemEventWaitEx",                       (void *)RTSemEventWaitEx },
    { "RTSemEventWaitExDebug",                  (void *)RTSemEventWaitExDebug },
    { "RTSemEventGetResolution",                (void *)RTSemEventGetResolution },
    { "RTSemEventDestroy",                      (void *)RTSemEventDestroy },
    { "RTSemEventMultiCreate",                  (void *)RTSemEventMultiCreate },
    { "RTSemEventMultiSignal",                  (void *)RTSemEventMultiSignal },
    { "RTSemEventMultiReset",                   (void *)RTSemEventMultiReset },
    { "RTSemEventMultiWait",                    (void *)RTSemEventMultiWait },
    { "RTSemEventMultiWaitNoResume",            (void *)RTSemEventMultiWaitNoResume },
    { "RTSemEventMultiWaitEx",                  (void *)RTSemEventMultiWaitEx },
    { "RTSemEventMultiWaitExDebug",             (void *)RTSemEventMultiWaitExDebug },
    { "RTSemEventMultiGetResolution",           (void *)RTSemEventMultiGetResolution },
    { "RTSemEventMultiDestroy",                 (void *)RTSemEventMultiDestroy },
    { "RTSpinlockCreate",                       (void *)RTSpinlockCreate },
    { "RTSpinlockDestroy",                      (void *)RTSpinlockDestroy },
    { "RTSpinlockAcquire",                      (void *)RTSpinlockAcquire },
    { "RTSpinlockRelease",                      (void *)RTSpinlockRelease },
    { "RTSpinlockAcquireNoInts",                (void *)RTSpinlockAcquireNoInts },
    { "RTSpinlockReleaseNoInts",                (void *)RTSpinlockReleaseNoInts },
    { "RTTimeNanoTS",                           (void *)RTTimeNanoTS },
    { "RTTimeMilliTS",                          (void *)RTTimeMilliTS },
    { "RTTimeSystemNanoTS",                     (void *)RTTimeSystemNanoTS },
    { "RTTimeSystemMilliTS",                    (void *)RTTimeSystemMilliTS },
    { "RTThreadNativeSelf",                     (void *)RTThreadNativeSelf },
    { "RTThreadSleep",                          (void *)RTThreadSleep },
    { "RTThreadYield",                          (void *)RTThreadYield },
    { "RTThreadSelf",                           (void *)RTThreadSelf },
    { "RTThreadCreate",                         (void *)RTThreadCreate },
    { "RTThreadGetNative",                      (void *)RTThreadGetNative },
    { "RTThreadWait",                           (void *)RTThreadWait },
    { "RTThreadWaitNoResume",                   (void *)RTThreadWaitNoResume },
    { "RTThreadGetName",                        (void *)RTThreadGetName },
    { "RTThreadSelfName",                       (void *)RTThreadSelfName },
    { "RTThreadGetType",                        (void *)RTThreadGetType },
    { "RTThreadUserSignal",                     (void *)RTThreadUserSignal },
    { "RTThreadUserReset",                      (void *)RTThreadUserReset },
    { "RTThreadUserWait",                       (void *)RTThreadUserWait },
    { "RTThreadUserWaitNoResume",               (void *)RTThreadUserWaitNoResume },
    { "RTThreadPreemptIsEnabled",               (void *)RTThreadPreemptIsEnabled },
    { "RTThreadPreemptIsPending",               (void *)RTThreadPreemptIsPending },
    { "RTThreadPreemptIsPendingTrusty",         (void *)RTThreadPreemptIsPendingTrusty },
    { "RTThreadPreemptIsPossible",              (void *)RTThreadPreemptIsPossible },
    { "RTThreadPreemptDisable",                 (void *)RTThreadPreemptDisable },
    { "RTThreadPreemptRestore",                 (void *)RTThreadPreemptRestore },
    { "RTThreadIsInInterrupt",                  (void *)RTThreadIsInInterrupt },
    { "RTTimerCreate",                          (void *)RTTimerCreate },
    { "RTTimerCreateEx",                        (void *)RTTimerCreateEx },
    { "RTTimerDestroy",                         (void *)RTTimerDestroy },
    { "RTTimerStart",                           (void *)RTTimerStart },
    { "RTTimerStop",                            (void *)RTTimerStop },
    { "RTTimerChangeInterval",                  (void *)RTTimerChangeInterval },
    { "RTTimerGetSystemGranularity",            (void *)RTTimerGetSystemGranularity },
    { "RTTimerRequestSystemGranularity",        (void *)RTTimerRequestSystemGranularity },
    { "RTTimerReleaseSystemGranularity",        (void *)RTTimerReleaseSystemGranularity },
    { "RTTimerCanDoHighResolution",             (void *)RTTimerCanDoHighResolution },

    { "RTLogDefaultInstance",                   (void *)RTLogDefaultInstance },
    { "RTMpCpuId",                              (void *)RTMpCpuId },
    { "RTMpCpuIdFromSetIndex",                  (void *)RTMpCpuIdFromSetIndex },
    { "RTMpCpuIdToSetIndex",                    (void *)RTMpCpuIdToSetIndex },
    { "RTMpGetArraySize",                       (void *)RTMpGetArraySize },
    { "RTMpIsCpuPossible",                      (void *)RTMpIsCpuPossible },
    { "RTMpGetCount",                           (void *)RTMpGetCount },
    { "RTMpGetMaxCpuId",                        (void *)RTMpGetMaxCpuId },
    { "RTMpGetOnlineCount",                     (void *)RTMpGetOnlineCount },
    { "RTMpGetOnlineSet",                       (void *)RTMpGetOnlineSet },
    { "RTMpGetSet",                             (void *)RTMpGetSet },
    { "RTMpIsCpuOnline",                        (void *)RTMpIsCpuOnline },
    { "RTMpIsCpuWorkPending",                   (void *)RTMpIsCpuWorkPending },
    { "RTMpNotificationRegister",               (void *)RTMpNotificationRegister },
    { "RTMpNotificationDeregister",             (void *)RTMpNotificationDeregister },
    { "RTMpOnAll",                              (void *)RTMpOnAll },
    { "RTMpOnOthers",                           (void *)RTMpOnOthers },
    { "RTMpOnSpecific",                         (void *)RTMpOnSpecific },
    { "RTMpPokeCpu",                            (void *)RTMpPokeCpu },
    { "RTPowerNotificationRegister",            (void *)RTPowerNotificationRegister },
    { "RTPowerNotificationDeregister",          (void *)RTPowerNotificationDeregister },
    { "RTLogRelDefaultInstance",                (void *)RTLogRelDefaultInstance },
    { "RTLogSetDefaultInstanceThread",          (void *)RTLogSetDefaultInstanceThread },
    { "RTLogLoggerExV",                         (void *)RTLogLoggerExV },
    { "RTLogPrintfV",                           (void *)RTLogPrintfV },
    { "RTR0AssertPanicSystem",                  (void *)RTR0AssertPanicSystem },
    { "RTAssertMsg1",                           (void *)RTAssertMsg1 },
    { "RTAssertMsg2V",                          (void *)RTAssertMsg2V },
    { "RTAssertMsg2AddV",                       (void *)RTAssertMsg2AddV },
    { "RTAssertSetQuiet",                       (void *)RTAssertSetQuiet },
    { "RTAssertMayPanic",                       (void *)RTAssertMayPanic },
    { "RTAssertSetMayPanic",                    (void *)RTAssertSetMayPanic },
    { "RTAssertAreQuiet",                       (void *)RTAssertAreQuiet },
    { "RTStrFormat",                            (void *)RTStrFormat },
    { "RTStrFormatNumber",                      (void *)RTStrFormatNumber },
    { "RTStrFormatTypeDeregister",              (void *)RTStrFormatTypeDeregister },
    { "RTStrFormatTypeRegister",                (void *)RTStrFormatTypeRegister },
    { "RTStrFormatTypeSetUser",                 (void *)RTStrFormatTypeSetUser },
    { "RTStrFormatV",                           (void *)RTStrFormatV },
    { "RTStrPrintf",                            (void *)RTStrPrintf },
    { "RTStrPrintfEx",                          (void *)RTStrPrintfEx },
    { "RTStrPrintfExV",                         (void *)RTStrPrintfExV },
    { "RTStrPrintfV",                           (void *)RTStrPrintfV },
    { "RTHandleTableAllocWithCtx",              (void *)RTHandleTableAllocWithCtx },
    { "RTHandleTableCreate",                    (void *)RTHandleTableCreate },
    { "RTHandleTableCreateEx",                  (void *)RTHandleTableCreateEx },
    { "RTHandleTableDestroy",                   (void *)RTHandleTableDestroy },
    { "RTHandleTableFreeWithCtx",               (void *)RTHandleTableFreeWithCtx },
    { "RTHandleTableLookupWithCtx",             (void *)RTHandleTableLookupWithCtx },
    { "RTNetIPv4AddDataChecksum",               (void *)RTNetIPv4AddDataChecksum },
    { "RTNetIPv4AddTCPChecksum",                (void *)RTNetIPv4AddTCPChecksum },
    { "RTNetIPv4AddUDPChecksum",                (void *)RTNetIPv4AddUDPChecksum },
    { "RTNetIPv4FinalizeChecksum",              (void *)RTNetIPv4FinalizeChecksum },
    { "RTNetIPv4HdrChecksum",                   (void *)RTNetIPv4HdrChecksum },
    { "RTNetIPv4IsDHCPValid",                   (void *)RTNetIPv4IsDHCPValid },
    { "RTNetIPv4IsHdrValid",                    (void *)RTNetIPv4IsHdrValid },
    { "RTNetIPv4IsTCPSizeValid",                (void *)RTNetIPv4IsTCPSizeValid },
    { "RTNetIPv4IsTCPValid",                    (void *)RTNetIPv4IsTCPValid },
    { "RTNetIPv4IsUDPSizeValid",                (void *)RTNetIPv4IsUDPSizeValid },
    { "RTNetIPv4IsUDPValid",                    (void *)RTNetIPv4IsUDPValid },
    { "RTNetIPv4PseudoChecksum",                (void *)RTNetIPv4PseudoChecksum },
    { "RTNetIPv4PseudoChecksumBits",            (void *)RTNetIPv4PseudoChecksumBits },
    { "RTNetIPv4TCPChecksum",                   (void *)RTNetIPv4TCPChecksum },
    { "RTNetIPv4UDPChecksum",                   (void *)RTNetIPv4UDPChecksum },
    { "RTNetIPv6PseudoChecksum",                (void *)RTNetIPv6PseudoChecksum },
    { "RTNetIPv6PseudoChecksumBits",            (void *)RTNetIPv6PseudoChecksumBits },
    { "RTNetIPv6PseudoChecksumEx",              (void *)RTNetIPv6PseudoChecksumEx },
    { "RTNetTCPChecksum",                       (void *)RTNetTCPChecksum },
    { "RTNetUDPChecksum",                       (void *)RTNetUDPChecksum },
    { "RTCrc32",                                (void *)RTCrc32 },
    { "RTCrc32Finish",                          (void *)RTCrc32Finish },
    { "RTCrc32Process",                         (void *)RTCrc32Process },
    { "RTCrc32Start",                           (void *)RTCrc32Start },
};

#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
/**
 * Drag in the rest of IRPT since we share it with the
 * rest of the kernel modules on darwin.
 */
PFNRT g_apfnVBoxDrvIPRTDeps[] =
{
    /* VBoxNetFlt */
    (PFNRT)RTErrConvertFromErrno,
    (PFNRT)RTUuidCompare,
    (PFNRT)RTUuidCompareStr,
    (PFNRT)RTUuidFromStr,
    (PFNRT)RTStrDupTag,
    (PFNRT)RTStrFree,
    (PFNRT)RTStrCopy,
    (PFNRT)RTStrNCmp,
    /* VBoxNetAdp */
    (PFNRT)RTRandBytes,
    /* VBoxUSB */
    (PFNRT)RTPathStripFilename,
    NULL
};
#endif  /* RT_OS_DARWIN || RT_OS_SOLARIS || RT_OS_SOLARIS */


/**
 * Initializes the device extentsion structure.
 *
 * @returns IPRT status code.
 * @param   pDevExt     The device extension to initialize.
 * @param   cbSession   The size of the session structure.  The size of
 *                      SUPDRVSESSION may be smaller when SUPDRV_AGNOSTIC is
 *                      defined because we're skipping the OS specific members
 *                      then.
 */
int VBOXCALL supdrvInitDevExt(PSUPDRVDEVEXT pDevExt, size_t cbSession)
{
    int rc;

#ifdef SUPDRV_WITH_RELEASE_LOGGER
    /*
     * Create the release log.
     */
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    PRTLOGGER pRelLogger;
    rc = RTLogCreate(&pRelLogger, 0 /* fFlags */, "all",
                     "VBOX_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups, RTLOGDEST_STDOUT | RTLOGDEST_DEBUGGER, NULL);
    if (RT_SUCCESS(rc))
        RTLogRelSetDefaultInstance(pRelLogger);
    /** @todo Add native hook for getting logger config parameters and setting
     *        them. On linux we should use the module parameter stuff... */
#endif

    /*
     * Initialize it.
     */
    memset(pDevExt, 0, sizeof(*pDevExt));
    rc = RTSpinlockCreate(&pDevExt->Spinlock);
    if (RT_SUCCESS(rc))
    {
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
        rc = RTSemMutexCreate(&pDevExt->mtxLdr);
#else
        rc = RTSemFastMutexCreate(&pDevExt->mtxLdr);
#endif
        if (RT_SUCCESS(rc))
        {
            rc = RTSemFastMutexCreate(&pDevExt->mtxComponentFactory);
            if (RT_SUCCESS(rc))
            {
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
                rc = RTSemMutexCreate(&pDevExt->mtxGip);
#else
                rc = RTSemFastMutexCreate(&pDevExt->mtxGip);
#endif
                if (RT_SUCCESS(rc))
                {
                    rc = supdrvGipCreate(pDevExt);
                    if (RT_SUCCESS(rc))
                    {
#ifdef VBOX_WITH_DTRACE_R0DRV
                        rc = supdrvVtgInit(pDevExt, &g_aFunctions[10]);
                        if (RT_SUCCESS(rc))
#endif
                        {
                            pDevExt->pLdrInitImage  = NULL;
                            pDevExt->hLdrInitThread = NIL_RTNATIVETHREAD;
                            pDevExt->u32Cookie      = BIRD;  /** @todo make this random? */
                            pDevExt->cbSession      = (uint32_t)cbSession;

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

#ifdef VBOX_WITH_DTRACE_R0DRV
                        supdrvGipDestroy(pDevExt);
#endif
                    }

#ifdef SUPDRV_USE_MUTEX_FOR_GIP
                    RTSemMutexDestroy(pDevExt->mtxGip);
                    pDevExt->mtxGip = NIL_RTSEMMUTEX;
#else
                    RTSemFastMutexDestroy(pDevExt->mtxGip);
                    pDevExt->mtxGip = NIL_RTSEMFASTMUTEX;
#endif
                }
                RTSemFastMutexDestroy(pDevExt->mtxComponentFactory);
                pDevExt->mtxComponentFactory = NIL_RTSEMFASTMUTEX;
            }
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
            RTSemMutexDestroy(pDevExt->mtxLdr);
            pDevExt->mtxLdr = NIL_RTSEMMUTEX;
#else
            RTSemFastMutexDestroy(pDevExt->mtxLdr);
            pDevExt->mtxLdr = NIL_RTSEMFASTMUTEX;
#endif
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
#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSemMutexDestroy(pDevExt->mtxGip);
    pDevExt->mtxGip = NIL_RTSEMMUTEX;
#else
    RTSemFastMutexDestroy(pDevExt->mtxGip);
    pDevExt->mtxGip = NIL_RTSEMFASTMUTEX;
#endif
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
    RTSemMutexDestroy(pDevExt->mtxLdr);
    pDevExt->mtxLdr = NIL_RTSEMMUTEX;
#else
    RTSemFastMutexDestroy(pDevExt->mtxLdr);
    pDevExt->mtxLdr = NIL_RTSEMFASTMUTEX;
#endif
    RTSpinlockDestroy(pDevExt->Spinlock);
    pDevExt->Spinlock = NIL_RTSPINLOCK;
    RTSemFastMutexDestroy(pDevExt->mtxComponentFactory);
    pDevExt->mtxComponentFactory = NIL_RTSEMFASTMUTEX;

    /*
     * Free lists.
     */
    /* objects. */
    pObj = pDevExt->pObjs;
    Assert(!pObj);                      /* (can trigger on forced unloads) */
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

#ifdef VBOX_WITH_DTRACE_R0DRV
    supdrvVtgTerm(pDevExt);
#endif

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
    int             rc;
    PSUPDRVSESSION  pSession = *ppSession = (PSUPDRVSESSION)RTMemAllocZ(pDevExt->cbSession);
    if (pSession)
    {
        /* Initialize session data. */
        rc = RTSpinlockCreate(&pSession->Spinlock);
        if (!rc)
        {
            rc = RTHandleTableCreateEx(&pSession->hHandleTable,
                                       RTHANDLETABLE_FLAGS_LOCKED | RTHANDLETABLE_FLAGS_CONTEXT,
                                       1 /*uBase*/, 32768 /*cMax*/, supdrvSessionObjHandleRetain, pSession);
            if (RT_SUCCESS(rc))
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

                VBOXDRV_SESSION_CREATE(pSession, fUser);
                LogFlow(("Created session %p initial cookie=%#x\n", pSession, pSession->u32Cookie));
                return VINF_SUCCESS;
            }

            RTSpinlockDestroy(pSession->Spinlock);
        }
        RTMemFree(pSession);
        *ppSession = NULL;
        Log(("Failed to create spinlock, rc=%d!\n", rc));
    }
    else
        rc = VERR_NO_MEMORY;

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
    VBOXDRV_SESSION_CLOSE(pSession);

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
    int                 rc;
    PSUPDRVBUNDLE       pBundle;
    LogFlow(("supdrvCleanupSession: pSession=%p\n", pSession));

    /*
     * Remove logger instances related to this session.
     */
    RTLogSetDefaultInstanceThread(NULL, (uintptr_t)pSession);

    /*
     * Destroy the handle table.
     */
    rc = RTHandleTableDestroy(pSession->hHandleTable, supdrvSessionObjHandleDelete, pSession);
    AssertRC(rc);
    pSession->hHandleTable = NIL_RTHANDLETABLE;

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
                    pObj->pfnDestructor(pObj, pObj->pvUser1, pObj->pvUser2);
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
    supdrvLdrLock(pDevExt);
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
    supdrvLdrUnlock(pDevExt);
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
 * RTHandleTableDestroy callback used by supdrvCleanupSession.
 *
 * @returns IPRT status code, see SUPR0ObjAddRef.
 * @param   hHandleTable    The handle table handle. Ignored.
 * @param   pvObj           The object pointer.
 * @param   pvCtx           Context, the handle type. Ignored.
 * @param   pvUser          Session pointer.
 */
static DECLCALLBACK(int) supdrvSessionObjHandleRetain(RTHANDLETABLE hHandleTable, void *pvObj, void *pvCtx, void *pvUser)
{
    NOREF(pvCtx);
    NOREF(hHandleTable);
    return SUPR0ObjAddRefEx(pvObj, (PSUPDRVSESSION)pvUser, true /*fNoBlocking*/);
}


/**
 * RTHandleTableDestroy callback used by supdrvCleanupSession.
 *
 * @param   hHandleTable    The handle table handle. Ignored.
 * @param   h               The handle value. Ignored.
 * @param   pvObj           The object pointer.
 * @param   pvCtx           Context, the handle type. Ignored.
 * @param   pvUser          Session pointer.
 */
static DECLCALLBACK(void) supdrvSessionObjHandleDelete(RTHANDLETABLE hHandleTable, uint32_t h, void *pvObj, void *pvCtx, void *pvUser)
{
    NOREF(pvCtx);
    NOREF(h);
    NOREF(hHandleTable);
    SUPR0ObjRelease(pvObj, (PSUPDRVSESSION)pvUser);
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
int VBOXCALL supdrvIOCtlFast(uintptr_t uIOCtl, VMCPUID idCpu, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    /*
     * We check the two prereqs after doing this only to allow the compiler to optimize things better.
     */
    if (RT_LIKELY(   RT_VALID_PTR(pSession)
                  && pSession->pVM
                  && pDevExt->pfnVMMR0EntryFast))
    {
        switch (uIOCtl)
        {
            case SUP_IOCTL_FAST_DO_RAW_RUN:
                pDevExt->pfnVMMR0EntryFast(pSession->pVM, idCpu, SUP_VMMR0_DO_RAW_RUN);
                break;
            case SUP_IOCTL_FAST_DO_HWACC_RUN:
                pDevExt->pfnVMMR0EntryFast(pSession->pVM, idCpu, SUP_VMMR0_DO_HWACC_RUN);
                break;
            case SUP_IOCTL_FAST_DO_NOP:
                pDevExt->pfnVMMR0EntryFast(pSession->pVM, idCpu, SUP_VMMR0_DO_NOP);
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
 * @returns  1 if pszStr does contain any character of pszChars, 0 otherwise.
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
 * @returns IPRT status code.
 * @retval  VERR_INVALID_PARAMETER if the request is invalid.
 *
 * @param   uIOCtl      Function number.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 * @param   pReqHdr     The request header.
 */
int VBOXCALL supdrvIOCtl(uintptr_t uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPREQHDR pReqHdr)
{
    VBOXDRV_IOCTL_ENTRY(pSession, uIOCtl, pReqHdr);

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
        VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VINF_SUCCESS);
        return VERR_INVALID_PARAMETER;
    }
    if (RT_UNLIKELY(!RT_VALID_PTR(pSession)))
    {
        OSDBGPRINT(("vboxdrv: Invalid pSession valud %p (ioctl=%p)\n", pSession, (void *)uIOCtl));
        VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VINF_SUCCESS);
        return VERR_INVALID_PARAMETER;
    }
    if (RT_UNLIKELY(uIOCtl == SUP_IOCTL_COOKIE))
    {
        if (pReqHdr->u32Cookie != SUPCOOKIE_INITIAL_COOKIE)
        {
            OSDBGPRINT(("SUP_IOCTL_COOKIE: bad cookie %#lx\n", (long)pReqHdr->u32Cookie));
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VINF_SUCCESS);
            return VERR_INVALID_PARAMETER;
        }
    }
    else if (RT_UNLIKELY(    pReqHdr->u32Cookie != pDevExt->u32Cookie
                         ||  pReqHdr->u32SessionCookie != pSession->u32Cookie))
    {
        OSDBGPRINT(("vboxdrv: bad cookie %#lx / %#lx.\n", (long)pReqHdr->u32Cookie, (long)pReqHdr->u32SessionCookie));
        VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VINF_SUCCESS);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VERR_INVALID_PARAMETER); \
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VERR_INVALID_PARAMETER); \
            return pReq->Hdr.rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_SIZE_OUT(Name, cbOutExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cbOut != (cbOutExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cbOut=%ld expected %ld.\n", \
                        (long)pReq->Hdr.cbOut, (long)(cbOutExpect))); \
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VERR_INVALID_PARAMETER); \
            return pReq->Hdr.rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_EXPR(Name, expr) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            OSDBGPRINT(( #Name ": %s\n", #expr)); \
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VERR_INVALID_PARAMETER); \
            return pReq->Hdr.rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_EXPR_FMT(expr, fmt) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            OSDBGPRINT( fmt ); \
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VERR_INVALID_PARAMETER); \
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
                VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, VERR_INVALID_MAGIC);
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
                VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, VERR_PERMISSION_DENIED);
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
                VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_UNLOCK):
        {
            /* validate */
            PSUPPAGEUNLOCK pReq = (PSUPPAGEUNLOCK)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_PAGE_UNLOCK);

            /* execute */
            pReq->Hdr.rc = SUPR0UnlockMem(pSession, pReq->u.In.pvR3);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CONT_FREE):
        {
            /* validate */
            PSUPCONTFREE pReq = (PSUPCONTFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_CONT_FREE);

            /* execute */
            pReq->Hdr.rc = SUPR0ContFree(pSession, (RTHCUINTPTR)pReq->u.In.pvR3);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_OPEN):
        {
            /* validate */
            PSUPLDROPEN pReq = (PSUPLDROPEN)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LDR_OPEN);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImageWithTabs > 0);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImageWithTabs < 16*_1M);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImageBits > 0);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImageBits > 0);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImageBits < pReq->u.In.cbImageWithTabs);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.szName[0]);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, RTStrEnd(pReq->u.In.szName, sizeof(pReq->u.In.szName)));
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, !supdrvCheckInvalidChar(pReq->u.In.szName, ";:()[]{}/\\|&*%#@!~`\"'"));
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, RTStrEnd(pReq->u.In.szFilename, sizeof(pReq->u.In.szFilename)));

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrOpen(pDevExt, pSession, pReq);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_LOAD):
        {
            /* validate */
            PSUPLDRLOAD pReq = (PSUPLDRLOAD)pReqHdr;
            REQ_CHECK_EXPR(Name, pReq->Hdr.cbIn >= sizeof(*pReq));
            REQ_CHECK_SIZES_EX(SUP_IOCTL_LDR_LOAD, SUP_IOCTL_LDR_LOAD_SIZE_IN(pReq->u.In.cbImageWithTabs), SUP_IOCTL_LDR_LOAD_SIZE_OUT);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_LOAD, pReq->u.In.cSymbols <= 16384);
            REQ_CHECK_EXPR_FMT(     !pReq->u.In.cSymbols
                               ||   (   pReq->u.In.offSymbols < pReq->u.In.cbImageWithTabs
                                     && pReq->u.In.offSymbols + pReq->u.In.cSymbols * sizeof(SUPLDRSYM) <= pReq->u.In.cbImageWithTabs),
                               ("SUP_IOCTL_LDR_LOAD: offSymbols=%#lx cSymbols=%#lx cbImageWithTabs=%#lx\n", (long)pReq->u.In.offSymbols,
                                (long)pReq->u.In.cSymbols, (long)pReq->u.In.cbImageWithTabs));
            REQ_CHECK_EXPR_FMT(     !pReq->u.In.cbStrTab
                               ||   (   pReq->u.In.offStrTab < pReq->u.In.cbImageWithTabs
                                     && pReq->u.In.offStrTab + pReq->u.In.cbStrTab <= pReq->u.In.cbImageWithTabs
                                     && pReq->u.In.cbStrTab <= pReq->u.In.cbImageWithTabs),
                               ("SUP_IOCTL_LDR_LOAD: offStrTab=%#lx cbStrTab=%#lx cbImageWithTabs=%#lx\n", (long)pReq->u.In.offStrTab,
                                (long)pReq->u.In.cbStrTab, (long)pReq->u.In.cbImageWithTabs));

            if (pReq->u.In.cSymbols)
            {
                uint32_t i;
                PSUPLDRSYM paSyms = (PSUPLDRSYM)&pReq->u.In.abImage[pReq->u.In.offSymbols];
                for (i = 0; i < pReq->u.In.cSymbols; i++)
                {
                    REQ_CHECK_EXPR_FMT(paSyms[i].offSymbol < pReq->u.In.cbImageWithTabs,
                                       ("SUP_IOCTL_LDR_LOAD: sym #%ld: symb off %#lx (max=%#lx)\n", (long)i, (long)paSyms[i].offSymbol, (long)pReq->u.In.cbImageWithTabs));
                    REQ_CHECK_EXPR_FMT(paSyms[i].offName < pReq->u.In.cbStrTab,
                                       ("SUP_IOCTL_LDR_LOAD: sym #%ld: name off %#lx (max=%#lx)\n", (long)i, (long)paSyms[i].offName, (long)pReq->u.In.cbImageWithTabs));
                    REQ_CHECK_EXPR_FMT(RTStrEnd((char const *)&pReq->u.In.abImage[pReq->u.In.offStrTab + paSyms[i].offName],
                                                pReq->u.In.cbStrTab - paSyms[i].offName),
                                       ("SUP_IOCTL_LDR_LOAD: sym #%ld: unterminated name! (%#lx / %#lx)\n", (long)i, (long)paSyms[i].offName, (long)pReq->u.In.cbImageWithTabs));
                }
            }

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrLoad(pDevExt, pSession, pReq);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_FREE):
        {
            /* validate */
            PSUPLDRFREE pReq = (PSUPLDRFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LDR_FREE);

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrFree(pDevExt, pSession, pReq);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_GET_SYMBOL):
        {
            /* validate */
            PSUPLDRGETSYMBOL pReq = (PSUPLDRGETSYMBOL)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LDR_GET_SYMBOL);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_GET_SYMBOL, RTStrEnd(pReq->u.In.szSymbol, sizeof(pReq->u.In.szSymbol)));

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrGetSymbol(pDevExt, pSession, pReq);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
                    pReq->Hdr.rc = pDevExt->pfnVMMR0EntryEx(pReq->u.In.pVMR0, pReq->u.In.idCpu, pReq->u.In.uOperation, NULL, pReq->u.In.u64Arg, pSession);
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
                    pReq->Hdr.rc = pDevExt->pfnVMMR0EntryEx(pReq->u.In.pVMR0, pReq->u.In.idCpu, pReq->u.In.uOperation, pVMMReq, pReq->u.In.u64Arg, pSession);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CALL_VMMR0_BIG):
        {
            /* validate */
            PSUPCALLVMMR0 pReq = (PSUPCALLVMMR0)pReqHdr;
            PSUPVMMR0REQHDR pVMMReq;
            Log4(("SUP_IOCTL_CALL_VMMR0_BIG: op=%u in=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                  pReq->u.In.uOperation, pReq->Hdr.cbIn, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));

            pVMMReq = (PSUPVMMR0REQHDR)&pReq->abReqPkt[0];
            REQ_CHECK_EXPR_FMT(pReq->Hdr.cbIn >= SUP_IOCTL_CALL_VMMR0_BIG_SIZE(sizeof(SUPVMMR0REQHDR)),
                               ("SUP_IOCTL_CALL_VMMR0_BIG: cbIn=%#x < %#lx\n", pReq->Hdr.cbIn, SUP_IOCTL_CALL_VMMR0_BIG_SIZE(sizeof(SUPVMMR0REQHDR))));
            REQ_CHECK_EXPR(SUP_IOCTL_CALL_VMMR0_BIG, pVMMReq->u32Magic == SUPVMMR0REQHDR_MAGIC);
            REQ_CHECK_SIZES_EX(SUP_IOCTL_CALL_VMMR0_BIG, SUP_IOCTL_CALL_VMMR0_BIG_SIZE_IN(pVMMReq->cbReq), SUP_IOCTL_CALL_VMMR0_BIG_SIZE_OUT(pVMMReq->cbReq));

            /* execute */
            if (RT_LIKELY(pDevExt->pfnVMMR0EntryEx))
                pReq->Hdr.rc = pDevExt->pfnVMMR0EntryEx(pReq->u.In.pVMR0, pReq->u.In.idCpu, pReq->u.In.uOperation, pVMMReq, pReq->u.In.u64Arg, pSession);
            else
                pReq->Hdr.rc = VERR_WRONG_ORDER;

            if (    RT_FAILURE(pReq->Hdr.rc)
                &&  pReq->Hdr.rc != VERR_INTERRUPTED
                &&  pReq->Hdr.rc != VERR_TIMEOUT)
                Log(("SUP_IOCTL_CALL_VMMR0_BIG: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                     pReq->Hdr.rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
            else
                Log4(("SUP_IOCTL_CALL_VMMR0_BIG: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                      pReq->Hdr.rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LOW_FREE):
        {
            /* validate */
            PSUPLOWFREE pReq = (PSUPLOWFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LOW_FREE);

            /* execute */
            pReq->Hdr.rc = SUPR0LowFree(pSession, (RTHCUINTPTR)pReq->u.In.pvR3);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_GIP_UNMAP):
        {
            /* validate */
            PSUPGIPUNMAP pReq = (PSUPGIPUNMAP)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_GIP_UNMAP);

            /* execute */
            pReq->Hdr.rc = SUPR0GipUnmap(pSession);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_PROTECT):
        {
            /* validate */
            PSUPPAGEPROTECT pReq = (PSUPPAGEPROTECT)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_PAGE_PROTECT);
            REQ_CHECK_EXPR_FMT(!(pReq->u.In.fProt & ~(RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC | RTMEM_PROT_NONE)),
                               ("SUP_IOCTL_PAGE_PROTECT: fProt=%#x!\n", pReq->u.In.fProt));
            REQ_CHECK_EXPR_FMT(!(pReq->u.In.offSub & PAGE_OFFSET_MASK), ("SUP_IOCTL_PAGE_PROTECT: offSub=%#x\n", pReq->u.In.offSub));
            REQ_CHECK_EXPR_FMT(pReq->u.In.cbSub && !(pReq->u.In.cbSub & PAGE_OFFSET_MASK),
                               ("SUP_IOCTL_PAGE_PROTECT: cbSub=%#x\n", pReq->u.In.cbSub));

            /* execute */
            pReq->Hdr.rc = SUPR0PageProtect(pSession, pReq->u.In.pvR3, pReq->u.In.pvR0, pReq->u.In.offSub, pReq->u.In.cbSub, pReq->u.In.fProt);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_FREE):
        {
            /* validate */
            PSUPPAGEFREE pReq = (PSUPPAGEFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_PAGE_FREE);

            /* execute */
            pReq->Hdr.rc = SUPR0PageFree(pSession, pReq->u.In.pvR3);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            REQ_CHECK_EXPR(SUP_IOCTL_CALL_SERVICE, RTStrEnd(pReq->u.In.szName, sizeof(pReq->u.In.szName)));

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_CallServiceModule(pDevExt, pSession, pReq);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
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
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_SEM_OP2):
        {
            /* validate */
            PSUPSEMOP2 pReq = (PSUPSEMOP2)pReqHdr;
            REQ_CHECK_SIZES_EX(SUP_IOCTL_SEM_OP2, SUP_IOCTL_SEM_OP2_SIZE_IN, SUP_IOCTL_SEM_OP2_SIZE_OUT);
            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP2, pReq->u.In.uReserved == 0);

            /* execute */
            switch (pReq->u.In.uType)
            {
                case SUP_SEM_TYPE_EVENT:
                {
                    SUPSEMEVENT hEvent = (SUPSEMEVENT)(uintptr_t)pReq->u.In.hSem;
                    switch (pReq->u.In.uOp)
                    {
                        case SUPSEMOP2_WAIT_MS_REL:
                            pReq->Hdr.rc = SUPSemEventWaitNoResume(pSession, hEvent, pReq->u.In.uArg.cRelMsTimeout);
                            break;
                        case SUPSEMOP2_WAIT_NS_ABS:
                            pReq->Hdr.rc = SUPSemEventWaitNsAbsIntr(pSession, hEvent, pReq->u.In.uArg.uAbsNsTimeout);
                            break;
                        case SUPSEMOP2_WAIT_NS_REL:
                            pReq->Hdr.rc = SUPSemEventWaitNsRelIntr(pSession, hEvent, pReq->u.In.uArg.cRelNsTimeout);
                            break;
                        case SUPSEMOP2_SIGNAL:
                            pReq->Hdr.rc = SUPSemEventSignal(pSession, hEvent);
                            break;
                        case SUPSEMOP2_CLOSE:
                            pReq->Hdr.rc = SUPSemEventClose(pSession, hEvent);
                            break;
                        case SUPSEMOP2_RESET:
                        default:
                            pReq->Hdr.rc = VERR_INVALID_FUNCTION;
                            break;
                    }
                    break;
                }

                case SUP_SEM_TYPE_EVENT_MULTI:
                {
                    SUPSEMEVENTMULTI hEventMulti = (SUPSEMEVENTMULTI)(uintptr_t)pReq->u.In.hSem;
                    switch (pReq->u.In.uOp)
                    {
                        case SUPSEMOP2_WAIT_MS_REL:
                            pReq->Hdr.rc = SUPSemEventMultiWaitNoResume(pSession, hEventMulti, pReq->u.In.uArg.cRelMsTimeout);
                            break;
                        case SUPSEMOP2_WAIT_NS_ABS:
                            pReq->Hdr.rc = SUPSemEventMultiWaitNsAbsIntr(pSession, hEventMulti, pReq->u.In.uArg.uAbsNsTimeout);
                            break;
                        case SUPSEMOP2_WAIT_NS_REL:
                            pReq->Hdr.rc = SUPSemEventMultiWaitNsRelIntr(pSession, hEventMulti, pReq->u.In.uArg.cRelNsTimeout);
                            break;
                        case SUPSEMOP2_SIGNAL:
                            pReq->Hdr.rc = SUPSemEventMultiSignal(pSession, hEventMulti);
                            break;
                        case SUPSEMOP2_CLOSE:
                            pReq->Hdr.rc = SUPSemEventMultiClose(pSession, hEventMulti);
                            break;
                        case SUPSEMOP2_RESET:
                            pReq->Hdr.rc = SUPSemEventMultiReset(pSession, hEventMulti);
                            break;
                        default:
                            pReq->Hdr.rc = VERR_INVALID_FUNCTION;
                            break;
                    }
                    break;
                }

                default:
                    pReq->Hdr.rc = VERR_INVALID_PARAMETER;
                    break;
            }
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_SEM_OP3):
        {
            /* validate */
            PSUPSEMOP3 pReq = (PSUPSEMOP3)pReqHdr;
            REQ_CHECK_SIZES_EX(SUP_IOCTL_SEM_OP3, SUP_IOCTL_SEM_OP3_SIZE_IN, SUP_IOCTL_SEM_OP3_SIZE_OUT);
            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP3, pReq->u.In.u32Reserved == 0 && pReq->u.In.u64Reserved == 0);

            /* execute */
            switch (pReq->u.In.uType)
            {
                case SUP_SEM_TYPE_EVENT:
                {
                    SUPSEMEVENT hEvent = (SUPSEMEVENT)(uintptr_t)pReq->u.In.hSem;
                    switch (pReq->u.In.uOp)
                    {
                        case SUPSEMOP3_CREATE:
                            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP3, hEvent == NIL_SUPSEMEVENT);
                            pReq->Hdr.rc = SUPSemEventCreate(pSession, &hEvent);
                            pReq->u.Out.hSem = (uint32_t)(uintptr_t)hEvent;
                            break;
                        case SUPSEMOP3_GET_RESOLUTION:
                            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP3, hEvent == NIL_SUPSEMEVENT);
                            pReq->Hdr.rc = VINF_SUCCESS;
                            pReq->Hdr.cbOut = sizeof(*pReq);
                            pReq->u.Out.cNsResolution = SUPSemEventGetResolution(pSession);
                            break;
                        default:
                            pReq->Hdr.rc = VERR_INVALID_FUNCTION;
                            break;
                    }
                    break;
                }

                case SUP_SEM_TYPE_EVENT_MULTI:
                {
                    SUPSEMEVENTMULTI hEventMulti = (SUPSEMEVENTMULTI)(uintptr_t)pReq->u.In.hSem;
                    switch (pReq->u.In.uOp)
                    {
                        case SUPSEMOP3_CREATE:
                            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP3, hEventMulti == NIL_SUPSEMEVENTMULTI);
                            pReq->Hdr.rc = SUPSemEventMultiCreate(pSession, &hEventMulti);
                            pReq->u.Out.hSem = (uint32_t)(uintptr_t)hEventMulti;
                            break;
                        case SUPSEMOP3_GET_RESOLUTION:
                            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP3, hEventMulti == NIL_SUPSEMEVENTMULTI);
                            pReq->Hdr.rc = VINF_SUCCESS;
                            pReq->u.Out.cNsResolution = SUPSemEventMultiGetResolution(pSession);
                            break;
                        default:
                            pReq->Hdr.rc = VERR_INVALID_FUNCTION;
                            break;
                    }
                    break;
                }

                default:
                    pReq->Hdr.rc = VERR_INVALID_PARAMETER;
                    break;
            }
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_VT_CAPS):
        {
            /* validate */
            PSUPVTCAPS pReq = (PSUPVTCAPS)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_VT_CAPS);
            REQ_CHECK_EXPR(SUP_IOCTL_VT_CAPS, pReq->Hdr.cbIn <= SUP_IOCTL_VT_CAPS_SIZE_IN);

            /* execute */
            pReq->Hdr.rc = SUPR0QueryVTCaps(pSession, &pReq->u.Out.Caps);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VINF_SUCCESS, pReq->Hdr.rc);
            return 0;
        }

        default:
            Log(("Unknown IOCTL %#lx\n", (long)uIOCtl));
            break;
    }
    VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_GENERAL_FAILURE, VERR_GENERAL_FAILURE);
    return VERR_GENERAL_FAILURE;
}


/**
 * Inter-Driver Communication (IDC) worker.
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
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: Hdr.pSession=%p expected NULL!\n", pReq->Hdr.pSession));
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
            if (pSession != NULL)
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: pSession=%p expected NULL!\n", pSession));
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

            pReq->Hdr.rc = supdrvCreateSession(pDevExt, false /* fUser */, &pSession);
            if (RT_FAILURE(pReq->Hdr.rc))
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: failed to create session, rc=%d\n", pReq->Hdr.rc));
                return VINF_SUCCESS;
            }

            pReq->u.Out.pSession = pSession;
            pReq->Hdr.pSession = pSession;

            return VINF_SUCCESS;
        }

        case SUPDRV_IDC_REQ_DISCONNECT:
        {
            REQ_CHECK_IDC_SIZE(SUPDRV_IDC_REQ_DISCONNECT, sizeof(*pReqHdr));

            supdrvCloseSession(pDevExt, pSession);
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
            pObj->pfnDestructor(pObj, pObj->pvUser1, pObj->pvUser2);
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

    /*
     * Let IPRT do the job.
     */
    Mem.eType = MEMREF_TYPE_LOCKED;
    rc = RTR0MemObjLockUser(&Mem.MemObj, pvR3, cb, RTMEM_PROT_READ | RTMEM_PROT_WRITE, RTR0ProcHandleSelf());
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
        Log(("SUPR0PageAlloc: Illegal request cb=%u; must be greater than 0 and smaller than %uMB (VBOX_MAX_ALLOC_PAGE_COUNT pages).\n", cPages, VBOX_MAX_ALLOC_PAGE_COUNT * (_1M / _4K)));
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
 * Maps a chunk of memory previously allocated by SUPR0PageAllocEx into kernel
 * space.
 *
 * @returns IPRT status code.
 * @param   pSession    The session to associated the allocation with.
 * @param   pvR3        The ring-3 address returned by SUPR0PageAllocEx.
 * @param   offSub      Where to start mapping. Must be page aligned.
 * @param   cbSub       How much to map. Must be page aligned.
 * @param   fFlags      Flags, MBZ.
 * @param   ppvR0       Where to return the address of the ring-0 mapping on
 *                      success.
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
         * Do some further input validations before calling IPRT.
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


/**
 * Changes the page level protection of one or more pages previously allocated
 * by SUPR0PageAllocEx.
 *
 * @returns IPRT status code.
 * @param   pSession    The session to associated the allocation with.
 * @param   pvR3        The ring-3 address returned by SUPR0PageAllocEx.
 *                      NIL_RTR3PTR if the ring-3 mapping should be unaffected.
 * @param   pvR0        The ring-0 address returned by SUPR0PageAllocEx.
 *                      NIL_RTR0PTR if the ring-0 mapping should be unaffected.
 * @param   offSub      Where to start changing. Must be page aligned.
 * @param   cbSub       How much to change. Must be page aligned.
 * @param   fProt       The new page level protection, see RTMEM_PROT_*.
 */
SUPR0DECL(int) SUPR0PageProtect(PSUPDRVSESSION pSession, RTR3PTR pvR3, RTR0PTR pvR0, uint32_t offSub, uint32_t cbSub, uint32_t fProt)
{
    int             rc;
    PSUPDRVBUNDLE   pBundle;
    RTSPINLOCKTMP   SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    RTR0MEMOBJ      hMemObjR0 = NIL_RTR0MEMOBJ;
    RTR0MEMOBJ      hMemObjR3 = NIL_RTR0MEMOBJ;
    LogFlow(("SUPR0PageProtect: pSession=%p pvR3=%p pvR0=%p offSub=%#x cbSub=%#x fProt-%#x\n", pSession, pvR3, pvR0, offSub, cbSub, fProt));

    /*
     * Validate input. The allowed allocation size must be at least equal to the maximum guest VRAM size.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertReturn(!(fProt & ~(RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC | RTMEM_PROT_NONE)), VERR_INVALID_PARAMETER);
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
                if (   pBundle->aMem[i].eType == MEMREF_TYPE_PAGE
                    && pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                    && (   pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                        || pvR3 == NIL_RTR3PTR)
                    && (   pvR0 == NIL_RTR0PTR
                        || RTR0MemObjAddress(pBundle->aMem[i].MemObj) == pvR0)
                    && (   pvR3 == NIL_RTR3PTR
                        || RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3) == pvR3))
                {
                    if (pvR0 != NIL_RTR0PTR)
                        hMemObjR0 = pBundle->aMem[i].MemObj;
                    if (pvR3 != NIL_RTR3PTR)
                        hMemObjR3 = pBundle->aMem[i].MapObjR3;
                    break;
                }
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock, &SpinlockTmp);

    rc = VERR_INVALID_PARAMETER;
    if (    hMemObjR0 != NIL_RTR0MEMOBJ
        ||  hMemObjR3 != NIL_RTR0MEMOBJ)
    {
        /*
         * Do some further input validations before calling IPRT.
         */
        size_t cbMemObj = hMemObjR0 != NIL_RTR0PTR ? RTR0MemObjSize(hMemObjR0) : RTR0MemObjSize(hMemObjR3);
        if (    offSub < cbMemObj
            &&  cbSub <= cbMemObj
            &&  offSub + cbSub <= cbMemObj)
        {
            rc = VINF_SUCCESS;
            if (hMemObjR3 != NIL_RTR0PTR)
                rc = RTR0MemObjProtect(hMemObjR3, offSub, cbSub, fProt);
            if (hMemObjR0 != NIL_RTR0PTR && RT_SUCCESS(rc))
                rc = RTR0MemObjProtect(hMemObjR0, offSub, cbSub, fProt);
        }
        else
            SUPR0Printf("SUPR0PageMapKernel: cbMemObj=%#x offSub=%#x cbSub=%#x\n", cbMemObj, offSub, cbSub);

    }
    return rc;

}


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


/** @todo document me */
SUPR0DECL(int) SUPR0QueryVTCaps(PSUPDRVSESSION pSession, uint32_t *pfCaps)
{
    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfCaps, VERR_INVALID_POINTER);

    *pfCaps = 0;

    if (ASMHasCpuId())
    {
        uint32_t u32FeaturesECX;
        uint32_t u32Dummy;
        uint32_t u32FeaturesEDX;
        uint32_t u32VendorEBX, u32VendorECX, u32VendorEDX, u32AMDFeatureEDX, u32AMDFeatureECX;
        uint64_t val;

        ASMCpuId(0, &u32Dummy, &u32VendorEBX, &u32VendorECX, &u32VendorEDX);
        ASMCpuId(1, &u32Dummy, &u32Dummy, &u32FeaturesECX, &u32FeaturesEDX);
        /* Query AMD features. */
        ASMCpuId(0x80000001, &u32Dummy, &u32Dummy, &u32AMDFeatureECX, &u32AMDFeatureEDX);

        if (    u32VendorEBX == X86_CPUID_VENDOR_INTEL_EBX
            &&  u32VendorECX == X86_CPUID_VENDOR_INTEL_ECX
            &&  u32VendorEDX == X86_CPUID_VENDOR_INTEL_EDX
           )
        {
            if (    (u32FeaturesECX & X86_CPUID_FEATURE_ECX_VMX)
                 && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
                 && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
               )
            {
                val = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);
                /*
                 * Both the LOCK and VMXON bit must be set; otherwise VMXON will generate a #GP.
                 * Once the lock bit is set, this MSR can no longer be modified.
                 */
                if (       (val & (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK))
                        ==        (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK) /* enabled and locked */
                    ||  !(val & MSR_IA32_FEATURE_CONTROL_LOCK) /* not enabled, but not locked either */
                   )
                {
                    VMX_CAPABILITY vtCaps;

                    *pfCaps |= SUPVTCAPS_VT_X;

                    vtCaps.u = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS);
                    if (vtCaps.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                    {
                        vtCaps.u = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS2);
                        if (vtCaps.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                            *pfCaps |= SUPVTCAPS_NESTED_PAGING;
                    }
                    return VINF_SUCCESS;
                }
                return VERR_VMX_MSR_LOCKED_OR_DISABLED;
            }
            return VERR_VMX_NO_VMX;
        }

        if (    u32VendorEBX == X86_CPUID_VENDOR_AMD_EBX
            &&  u32VendorECX == X86_CPUID_VENDOR_AMD_ECX
            &&  u32VendorEDX == X86_CPUID_VENDOR_AMD_EDX
           )
        {
            if (   (u32AMDFeatureECX & X86_CPUID_AMD_FEATURE_ECX_SVM)
                && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
                && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
               )
            {
                /* Check if SVM is disabled */
                val = ASMRdMsr(MSR_K8_VM_CR);
                if (!(val & MSR_K8_VM_CR_SVM_DISABLE))
                {
                    *pfCaps |= SUPVTCAPS_AMD_V;

                    /* Query AMD features. */
                    ASMCpuId(0x8000000A, &u32Dummy, &u32Dummy, &u32Dummy, &u32FeaturesEDX);

                    if (u32FeaturesEDX & AMD_CPUID_SVM_FEATURE_EDX_NESTED_PAGING)
                        *pfCaps |= SUPVTCAPS_NESTED_PAGING;

                    return VINF_SUCCESS;
                }
                return VERR_SVM_DISABLED;
            }
            return VERR_SVM_NO_SVM;
        }
    }

    return VERR_UNSUPPORTED_CPU;
}


/**
 * (Re-)initializes the per-cpu structure prior to starting or resuming the GIP
 * updating.
 *
 * @param   pGipCpu          The per CPU structure for this CPU.
 * @param   u64NanoTS        The current time.
 */
static void supdrvGipReInitCpu(PSUPGIPCPU pGipCpu, uint64_t u64NanoTS)
{
    pGipCpu->u64TSC    = ASMReadTSC() - pGipCpu->u32UpdateIntervalTSC;
    pGipCpu->u64NanoTS = u64NanoTS;
}


/**
 * Set the current TSC and NanoTS value for the CPU.
 *
 * @param   idCpu            The CPU ID. Unused - we have to use the APIC ID.
 * @param   pvUser1          Pointer to the ring-0 GIP mapping.
 * @param   pvUser2          Pointer to the variable holding the current time.
 */
static DECLCALLBACK(void) supdrvGipReInitCpuCallback(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PSUPGLOBALINFOPAGE  pGip = (PSUPGLOBALINFOPAGE)pvUser1;
    unsigned            iCpu = pGip->aiCpuFromApicId[ASMGetApicId()];

    if (RT_LIKELY(iCpu < pGip->cCpus && pGip->aCPUs[iCpu].idCpu == idCpu))
        supdrvGipReInitCpu(&pGip->aCPUs[iCpu], *(uint64_t *)pvUser2);

    NOREF(pvUser2);
    NOREF(idCpu);
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
    int             rc;
    PSUPDRVDEVEXT   pDevExt = pSession->pDevExt;
    RTR3PTR         pGipR3  = NIL_RTR3PTR;
    RTHCPHYS        HCPhys  = NIL_RTHCPHYS;
    LogFlow(("SUPR0GipMap: pSession=%p ppGipR3=%p pHCPhysGip=%p\n", pSession, ppGipR3, pHCPhysGip));

    /*
     * Validate
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(ppGipR3, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pHCPhysGip, VERR_INVALID_POINTER);

#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSemMutexRequest(pDevExt->mtxGip, RT_INDEFINITE_WAIT);
#else
    RTSemFastMutexRequest(pDevExt->mtxGip);
#endif
    if (pDevExt->pGip)
    {
        /*
         * Map it?
         */
        rc = VINF_SUCCESS;
        if (ppGipR3)
        {
            if (pSession->GipMapObjR3 == NIL_RTR0MEMOBJ)
                rc = RTR0MemObjMapUser(&pSession->GipMapObjR3, pDevExt->GipMemObj, (RTR3PTR)-1, 0,
                                       RTMEM_PROT_READ, RTR0ProcHandleSelf());
            if (RT_SUCCESS(rc))
                pGipR3 = RTR0MemObjAddressR3(pSession->GipMapObjR3);
        }

        /*
         * Get physical address.
         */
        if (pHCPhysGip && RT_SUCCESS(rc))
            HCPhys = pDevExt->HCPhysGip;

        /*
         * Reference globally.
         */
        if (!pSession->fGipReferenced && RT_SUCCESS(rc))
        {
            pSession->fGipReferenced = 1;
            pDevExt->cGipUsers++;
            if (pDevExt->cGipUsers == 1)
            {
                PSUPGLOBALINFOPAGE pGipR0 = pDevExt->pGip;
                uint64_t u64NanoTS;
                uint32_t u32SystemResolution;
                unsigned i;

                LogFlow(("SUPR0GipMap: Resumes GIP updating\n"));

                /*
                 * Try bump up the system timer resolution.
                 * The more interrupts the better...
                 */
                if (   RT_SUCCESS_NP(RTTimerRequestSystemGranularity(  976563 /* 1024 HZ */, &u32SystemResolution))
                    || RT_SUCCESS_NP(RTTimerRequestSystemGranularity( 1000000 /* 1000 HZ */, &u32SystemResolution))
                    || RT_SUCCESS_NP(RTTimerRequestSystemGranularity( 1953125 /*  512 HZ */, &u32SystemResolution))
                    || RT_SUCCESS_NP(RTTimerRequestSystemGranularity( 2000000 /*  500 HZ */, &u32SystemResolution))
                   )
                {
                    Assert(RTTimerGetSystemGranularity() <= u32SystemResolution);
                    pDevExt->u32SystemTimerGranularityGrant = u32SystemResolution;
                }

                if (pGipR0->aCPUs[0].u32TransactionId != 2 /* not the first time */)
                {
                    for (i = 0; i < RT_ELEMENTS(pGipR0->aCPUs); i++)
                        ASMAtomicUoWriteU32(&pGipR0->aCPUs[i].u32TransactionId,
                                            (pGipR0->aCPUs[i].u32TransactionId + GIP_UPDATEHZ_RECALC_FREQ * 2)
                                            & ~(GIP_UPDATEHZ_RECALC_FREQ * 2 - 1));
                    ASMAtomicWriteU64(&pGipR0->u64NanoTSLastUpdateHz, 0);
                }

                u64NanoTS = RTTimeSystemNanoTS() - pGipR0->u32UpdateIntervalNS;
                if (   pGipR0->u32Mode == SUPGIPMODE_SYNC_TSC
                    || RTMpGetOnlineCount() == 1)
                    supdrvGipReInitCpu(&pGipR0->aCPUs[0], u64NanoTS);
                else
                    RTMpOnAll(supdrvGipReInitCpuCallback, pGipR0, &u64NanoTS);

#ifndef DO_NOT_START_GIP
                rc = RTTimerStart(pDevExt->pGipTimer, 0); AssertRC(rc);
#endif
                rc = VINF_SUCCESS;
            }
        }
    }
    else
    {
        rc = VERR_GENERAL_FAILURE;
        Log(("SUPR0GipMap: GIP is not available!\n"));
    }
#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSemMutexRelease(pDevExt->mtxGip);
#else
    RTSemFastMutexRelease(pDevExt->mtxGip);
#endif

    /*
     * Write returns.
     */
    if (pHCPhysGip)
        *pHCPhysGip = HCPhys;
    if (ppGipR3)
        *ppGipR3 = pGipR3;

#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("SUPR0GipMap: returns %d *pHCPhysGip=%lx pGipR3=%p\n", rc, (unsigned long)HCPhys, (void *)pGipR3));
#else
    LogFlow((   "SUPR0GipMap: returns %d *pHCPhysGip=%lx pGipR3=%p\n", rc, (unsigned long)HCPhys, (void *)pGipR3));
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

#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSemMutexRequest(pDevExt->mtxGip, RT_INDEFINITE_WAIT);
#else
    RTSemFastMutexRequest(pDevExt->mtxGip);
#endif

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
#ifndef DO_NOT_START_GIP
            rc = RTTimerStop(pDevExt->pGipTimer); AssertRC(rc); rc = VINF_SUCCESS;
#endif

            if (pDevExt->u32SystemTimerGranularityGrant)
            {
                int rc2 = RTTimerReleaseSystemGranularity(pDevExt->u32SystemTimerGranularityGrant);
                AssertRC(rc2);
                pDevExt->u32SystemTimerGranularityGrant = 0;
            }
        }
    }

#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSemMutexRelease(pDevExt->mtxGip);
#else
    RTSemFastMutexRelease(pDevExt->mtxGip);
#endif

    return rc;
}


/**
 * Gets the GIP pointer.
 *
 * @returns Pointer to the GIP or NULL.
 */
SUPDECL(PSUPGLOBALINFOPAGE) SUPGetGIP(void)
{
    return g_pSUPGlobalInfoPage;
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
    psz = RTStrEnd(pFactory->szName, sizeof(pFactory->szName));
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
    pszEnd = RTStrEnd(pszName, RT_SIZEOFMEMB(SUPDRVFACTORY, szName));
    AssertReturn(pszEnd, VERR_INVALID_PARAMETER);
    cchName = pszEnd - pszName;

    AssertPtrReturn(pszInterfaceUuid, VERR_INVALID_POINTER);
    pszEnd = RTStrEnd(pszInterfaceUuid, RTUUID_STR_LENGTH);
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
                void *pvFactory = pCur->pFactory->pfnQueryFactoryInterface(pCur->pFactory, pSession, pszInterfaceUuid);
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


#if defined(VBOX_WITH_DTRACE_R0DRV) || defined(RT_OS_SOLARIS)
/**
 * Stub function.
 */
SUPR0DECL(void) SUPR0VtgFireProbe(uint32_t idProbe, uintptr_t uArg0, uintptr_t uArg1, uintptr_t uArg2, 
                                  uintptr_t uArg3, uintptr_t uArg4)
{
    NOREF(idProbe); NOREF(uArg0); NOREF(uArg1); NOREF(uArg2); NOREF(uArg3); NOREF(uArg4);
}
#endif

#ifndef VBOX_WITH_DTRACE_R0DRV
/**
 * Stub function.
 */
SUPR0DECL(int) SUPR0VtgRegisterModule(void *hMod, struct VTGOBJHDR *pVtgHdr)
{
    NOREF(hMod); NOREF(pVtgHdr);
    return VINF_SUCCESS;
}
#endif


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
    int             rc;
    PSUPDRVLDRIMAGE pImage;
    void           *pv;
    size_t          cchName = strlen(pReq->u.In.szName); /* (caller checked < 32). */
    LogFlow(("supdrvIOCtl_LdrOpen: szName=%s cbImageWithTabs=%d\n", pReq->u.In.szName, pReq->u.In.cbImageWithTabs));

    /*
     * Check if we got an instance of the image already.
     */
    supdrvLdrLock(pDevExt);
    for (pImage = pDevExt->pLdrImages; pImage; pImage = pImage->pNext)
    {
        if (    pImage->szName[cchName] == '\0'
            &&  !memcmp(pImage->szName, pReq->u.In.szName, cchName))
        {
            /** @todo check cbImageBits and cbImageWithTabs here, if they differs that indicates that the images are different. */
            pImage->cUsage++;
            pReq->u.Out.pvImageBase   = pImage->pvImage;
            pReq->u.Out.fNeedsLoading = pImage->uState == SUP_IOCTL_LDR_OPEN;
            pReq->u.Out.fNativeLoader = pImage->fNative;
            supdrvLdrAddUsage(pSession, pImage);
            supdrvLdrUnlock(pDevExt);
            return VINF_SUCCESS;
        }
    }
    /* (not found - add it!) */

    /*
     * Allocate memory.
     */
    pv = RTMemAlloc(RT_OFFSETOF(SUPDRVLDRIMAGE, szName[cchName + 1]));
    if (!pv)
    {
        supdrvLdrUnlock(pDevExt);
        Log(("supdrvIOCtl_LdrOpen: RTMemAlloc() failed\n"));
        return /*VERR_NO_MEMORY*/ VERR_INTERNAL_ERROR_2;
    }

    /*
     * Setup and link in the LDR stuff.
     */
    pImage = (PSUPDRVLDRIMAGE)pv;
    pImage->pvImage         = NULL;
    pImage->pvImageAlloc    = NULL;
    pImage->cbImageWithTabs = pReq->u.In.cbImageWithTabs;
    pImage->cbImageBits     = pReq->u.In.cbImageBits;
    pImage->cSymbols        = 0;
    pImage->paSymbols       = NULL;
    pImage->pachStrTab      = NULL;
    pImage->cbStrTab        = 0;
    pImage->pfnModuleInit   = NULL;
    pImage->pfnModuleTerm   = NULL;
    pImage->pfnServiceReqHandler = NULL;
    pImage->uState          = SUP_IOCTL_LDR_OPEN;
    pImage->cUsage          = 1;
    pImage->pDevExt         = pDevExt;
    memcpy(pImage->szName, pReq->u.In.szName, cchName + 1);

    /*
     * Try load it using the native loader, if that isn't supported, fall back
     * on the older method.
     */
    pImage->fNative         = true;
    rc = supdrvOSLdrOpen(pDevExt, pImage, pReq->u.In.szFilename);
    if (rc == VERR_NOT_SUPPORTED)
    {
        pImage->pvImageAlloc = RTMemExecAlloc(pImage->cbImageBits + 31);
        pImage->pvImage     = RT_ALIGN_P(pImage->pvImageAlloc, 32);
        pImage->fNative     = false;
        rc = pImage->pvImageAlloc ? VINF_SUCCESS : VERR_NO_EXEC_MEMORY;
    }
    if (RT_FAILURE(rc))
    {
        supdrvLdrUnlock(pDevExt);
        RTMemFree(pImage);
        Log(("supdrvIOCtl_LdrOpen(%s): failed - %Rrc\n", pReq->u.In.szName, rc));
        return rc;
    }
    Assert(VALID_PTR(pImage->pvImage) || RT_FAILURE(rc));

    /*
     * Link it.
     */
    pImage->pNext           = pDevExt->pLdrImages;
    pDevExt->pLdrImages     = pImage;

    supdrvLdrAddUsage(pSession, pImage);

    pReq->u.Out.pvImageBase   = pImage->pvImage;
    pReq->u.Out.fNeedsLoading = true;
    pReq->u.Out.fNativeLoader = pImage->fNative;
    supdrvLdrUnlock(pDevExt);

#if defined(RT_OS_WINDOWS) && defined(DEBUG)
    SUPR0Printf("VBoxDrv: windbg> .reload /f %s=%#p\n", pImage->szName, pImage->pvImage);
#endif
    return VINF_SUCCESS;
}


/**
 * Worker that validates a pointer to an image entrypoint.
 *
 * @returns IPRT status code.
 * @param   pDevExt     The device globals.
 * @param   pImage      The loader image.
 * @param   pv          The pointer into the image.
 * @param   fMayBeNull  Whether it may be NULL.
 * @param   pszWhat     What is this entrypoint? (for logging)
 * @param   pbImageBits The image bits prepared by ring-3.
 *
 * @remarks Will leave the lock on failure.
 */
static int supdrvLdrValidatePointer(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, void *pv,
                                    bool fMayBeNull, const uint8_t *pbImageBits, const char *pszWhat)
{
    if (!fMayBeNull || pv)
    {
        if ((uintptr_t)pv - (uintptr_t)pImage->pvImage >= pImage->cbImageBits)
        {
            supdrvLdrUnlock(pDevExt);
            Log(("Out of range (%p LB %#x): %s=%p\n", pImage->pvImage, pImage->cbImageBits, pszWhat, pv));
            return VERR_INVALID_PARAMETER;
        }

        if (pImage->fNative)
        {
            int rc = supdrvOSLdrValidatePointer(pDevExt, pImage, pv, pbImageBits);
            if (RT_FAILURE(rc))
            {
                supdrvLdrUnlock(pDevExt);
                Log(("Bad entry point address: %s=%p (rc=%Rrc)\n", pszWhat, pv, rc));
                return rc;
            }
        }
    }
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
    LogFlow(("supdrvIOCtl_LdrLoad: pvImageBase=%p cbImageWithBits=%d\n", pReq->u.In.pvImageBase, pReq->u.In.cbImageWithTabs));

    /*
     * Find the ldr image.
     */
    supdrvLdrLock(pDevExt);
    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pReq->u.In.pvImageBase)
        pUsage = pUsage->pNext;
    if (!pUsage)
    {
        supdrvLdrUnlock(pDevExt);
        Log(("SUP_IOCTL_LDR_LOAD: couldn't find image!\n"));
        return VERR_INVALID_HANDLE;
    }
    pImage = pUsage->pImage;

    /*
     * Validate input.
     */
    if (   pImage->cbImageWithTabs != pReq->u.In.cbImageWithTabs
        || pImage->cbImageBits     != pReq->u.In.cbImageBits)
    {
        supdrvLdrUnlock(pDevExt);
        Log(("SUP_IOCTL_LDR_LOAD: image size mismatch!! %d(prep) != %d(load) or %d != %d\n",
             pImage->cbImageWithTabs, pReq->u.In.cbImageWithTabs, pImage->cbImageBits, pReq->u.In.cbImageBits));
        return VERR_INVALID_HANDLE;
    }

    if (pImage->uState != SUP_IOCTL_LDR_OPEN)
    {
        unsigned uState = pImage->uState;
        supdrvLdrUnlock(pDevExt);
        if (uState != SUP_IOCTL_LDR_LOAD)
            AssertMsgFailed(("SUP_IOCTL_LDR_LOAD: invalid image state %d (%#x)!\n", uState, uState));
        return VERR_ALREADY_LOADED;
    }

    switch (pReq->u.In.eEPType)
    {
        case SUPLDRLOADEP_NOTHING:
            break;

        case SUPLDRLOADEP_VMMR0:
            rc = supdrvLdrValidatePointer(    pDevExt, pImage, pReq->u.In.EP.VMMR0.pvVMMR0,          false, pReq->u.In.abImage, "pvVMMR0");
            if (RT_SUCCESS(rc))
                rc = supdrvLdrValidatePointer(pDevExt, pImage, pReq->u.In.EP.VMMR0.pvVMMR0EntryInt,  false, pReq->u.In.abImage, "pvVMMR0EntryInt");
            if (RT_SUCCESS(rc))
                rc = supdrvLdrValidatePointer(pDevExt, pImage, pReq->u.In.EP.VMMR0.pvVMMR0EntryFast, false, pReq->u.In.abImage, "pvVMMR0EntryFast");
            if (RT_SUCCESS(rc))
                rc = supdrvLdrValidatePointer(pDevExt, pImage, pReq->u.In.EP.VMMR0.pvVMMR0EntryEx,   false, pReq->u.In.abImage, "pvVMMR0EntryEx");
            if (RT_FAILURE(rc))
                return rc;
            break;

        case SUPLDRLOADEP_SERVICE:
            rc = supdrvLdrValidatePointer(pDevExt, pImage, pReq->u.In.EP.Service.pfnServiceReq, false, pReq->u.In.abImage, "pfnServiceReq");
            if (RT_FAILURE(rc))
                return rc;
            if (    pReq->u.In.EP.Service.apvReserved[0] != NIL_RTR0PTR
                ||  pReq->u.In.EP.Service.apvReserved[1] != NIL_RTR0PTR
                ||  pReq->u.In.EP.Service.apvReserved[2] != NIL_RTR0PTR)
            {
                supdrvLdrUnlock(pDevExt);
                Log(("Out of range (%p LB %#x): apvReserved={%p,%p,%p} MBZ!\n",
                     pImage->pvImage, pReq->u.In.cbImageWithTabs,
                     pReq->u.In.EP.Service.apvReserved[0],
                     pReq->u.In.EP.Service.apvReserved[1],
                     pReq->u.In.EP.Service.apvReserved[2]));
                return VERR_INVALID_PARAMETER;
            }
            break;

        default:
            supdrvLdrUnlock(pDevExt);
            Log(("Invalid eEPType=%d\n", pReq->u.In.eEPType));
            return VERR_INVALID_PARAMETER;
    }

    rc = supdrvLdrValidatePointer(pDevExt, pImage, pReq->u.In.pfnModuleInit, true, pReq->u.In.abImage, "pfnModuleInit");
    if (RT_FAILURE(rc))
        return rc;
    rc = supdrvLdrValidatePointer(pDevExt, pImage, pReq->u.In.pfnModuleTerm, true, pReq->u.In.abImage, "pfnModuleTerm");
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Allocate and copy the tables.
     * (No need to do try/except as this is a buffered request.)
     */
    pImage->cbStrTab = pReq->u.In.cbStrTab;
    if (pImage->cbStrTab)
    {
        pImage->pachStrTab = (char *)RTMemAlloc(pImage->cbStrTab);
        if (pImage->pachStrTab)
            memcpy(pImage->pachStrTab, &pReq->u.In.abImage[pReq->u.In.offStrTab], pImage->cbStrTab);
        else
            rc = /*VERR_NO_MEMORY*/ VERR_INTERNAL_ERROR_3;
    }

    pImage->cSymbols = pReq->u.In.cSymbols;
    if (RT_SUCCESS(rc) && pImage->cSymbols)
    {
        size_t  cbSymbols = pImage->cSymbols * sizeof(SUPLDRSYM);
        pImage->paSymbols = (PSUPLDRSYM)RTMemAlloc(cbSymbols);
        if (pImage->paSymbols)
            memcpy(pImage->paSymbols, &pReq->u.In.abImage[pReq->u.In.offSymbols], cbSymbols);
        else
            rc = /*VERR_NO_MEMORY*/ VERR_INTERNAL_ERROR_4;
    }

    /*
     * Copy the bits / complete native loading.
     */
    if (RT_SUCCESS(rc))
    {
        pImage->uState = SUP_IOCTL_LDR_LOAD;
        pImage->pfnModuleInit = pReq->u.In.pfnModuleInit;
        pImage->pfnModuleTerm = pReq->u.In.pfnModuleTerm;

        if (pImage->fNative)
            rc = supdrvOSLdrLoad(pDevExt, pImage, pReq->u.In.abImage, pReq);
        else
            memcpy(pImage->pvImage, &pReq->u.In.abImage[0], pImage->cbImageBits);
    }

    /*
     * Update any entry points.
     */
    if (RT_SUCCESS(rc))
    {
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
    }

    /*
     * On success call the module initialization.
     */
    LogFlow(("supdrvIOCtl_LdrLoad: pfnModuleInit=%p\n", pImage->pfnModuleInit));
    if (RT_SUCCESS(rc) && pImage->pfnModuleInit)
    {
        Log(("supdrvIOCtl_LdrLoad: calling pfnModuleInit=%p\n", pImage->pfnModuleInit));
        pDevExt->pLdrInitImage  = pImage;
        pDevExt->hLdrInitThread = RTThreadNativeSelf();
        rc = pImage->pfnModuleInit(pImage);
        pDevExt->pLdrInitImage  = NULL;
        pDevExt->hLdrInitThread = NIL_RTNATIVETHREAD;
        if (RT_FAILURE(rc) && pDevExt->pvVMMR0 == pImage->pvImage)
            supdrvLdrUnsetVMMR0EPs(pDevExt);
    }

    if (RT_FAILURE(rc))
    {
#ifdef VBOX_WITH_DTRACE_R0DRV
        /* Inform the tracing component in case ModuleInit registered TPs. */
        supdrvVtgModuleUnloading(pDevExt, pImage);
#endif

        pImage->uState              = SUP_IOCTL_LDR_OPEN;
        pImage->pfnModuleInit       = NULL;
        pImage->pfnModuleTerm       = NULL;
        pImage->pfnServiceReqHandler= NULL;
        pImage->cbStrTab            = 0;
        RTMemFree(pImage->pachStrTab);
        pImage->pachStrTab          = NULL;
        RTMemFree(pImage->paSymbols);
        pImage->paSymbols           = NULL;
        pImage->cSymbols            = 0;
    }

    supdrvLdrUnlock(pDevExt);
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
    supdrvLdrLock(pDevExt);
    pUsagePrev = NULL;
    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pReq->u.In.pvImageBase)
    {
        pUsagePrev = pUsage;
        pUsage = pUsage->pNext;
    }
    if (!pUsage)
    {
        supdrvLdrUnlock(pDevExt);
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
                if (RT_UNLIKELY((uintptr_t)pObj->pfnDestructor - (uintptr_t)pImage->pvImage < pImage->cbImageBits))
                {
                    rc = VERR_DANGLING_OBJECTS;
                    break;
                }
        }
        else
        {
            PSUPDRVUSAGE pGenUsage;
            for (pGenUsage = pSession->pUsage; pGenUsage; pGenUsage = pGenUsage->pNext)
                if (RT_UNLIKELY((uintptr_t)pGenUsage->pObj->pfnDestructor - (uintptr_t)pImage->pvImage < pImage->cbImageBits))
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
             * Dereference the image.
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

    supdrvLdrUnlock(pDevExt);
    return rc;
}


/**
 * Gets the address of a symbol in an open image.
 *
 * @returns IPRT status code.
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
    supdrvLdrLock(pDevExt);
    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pReq->u.In.pvImageBase)
        pUsage = pUsage->pNext;
    if (!pUsage)
    {
        supdrvLdrUnlock(pDevExt);
        Log(("SUP_IOCTL_LDR_GET_SYMBOL: couldn't find image!\n"));
        return VERR_INVALID_HANDLE;
    }
    pImage = pUsage->pImage;
    if (pImage->uState != SUP_IOCTL_LDR_LOAD)
    {
        unsigned uState = pImage->uState;
        supdrvLdrUnlock(pDevExt);
        Log(("SUP_IOCTL_LDR_GET_SYMBOL: invalid image state %d (%#x)!\n", uState, uState)); NOREF(uState);
        return VERR_ALREADY_LOADED;
    }

    /*
     * Search the symbol strings.
     *
     * Note! The int32_t is for native loading on solaris where the data
     *       and text segments are in very different places.
     */
    pchStrings = pImage->pachStrTab;
    paSyms     = pImage->paSymbols;
    for (i = 0; i < pImage->cSymbols; i++)
    {
        if (    paSyms[i].offName + cbSymbol <= pImage->cbStrTab
            &&  !memcmp(pchStrings + paSyms[i].offName, pReq->u.In.szSymbol, cbSymbol))
        {
            pvSymbol = (uint8_t *)pImage->pvImage + (int32_t)paSyms[i].offSymbol;
            rc = VINF_SUCCESS;
            break;
        }
    }
    supdrvLdrUnlock(pDevExt);
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
    pszEnd = RTStrEnd(pszSymbol, 512);
    AssertReturn(pszEnd, VERR_INVALID_PARAMETER);
    cbSymbol = pszEnd - pszSymbol + 1;

    if (pszModule)
    {
        AssertPtrReturn(pszModule, VERR_INVALID_POINTER);
        pszEnd = RTStrEnd(pszModule, 64);
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

        supdrvLdrLock(pDevExt);

        for (pImage = pDevExt->pLdrImages; pImage; pImage = pImage->pNext)
            if (!strcmp(pImage->szName, pszModule))
                break;
        if (pImage && pImage->uState == SUP_IOCTL_LDR_LOAD)
        {
            /*
             * Search the symbol strings.
             */
            const char *pchStrings = pImage->pachStrTab;
            PCSUPLDRSYM paSyms     = pImage->paSymbols;
            for (i = 0; i < pImage->cSymbols; i++)
            {
                if (    paSyms[i].offName + cbSymbol <= pImage->cbStrTab
                    &&  !memcmp(pchStrings + paSyms[i].offName, pszSymbol, cbSymbol))
                {
                    /*
                     * Found it! Calc the symbol address and add a reference to the module.
                     */
                    pReq->u.Out.pfnSymbol = (PFNRT)((uint8_t *)pImage->pvImage + (int32_t)paSyms[i].offSymbol);
                    rc = supdrvLdrAddUsage(pSession, pImage);
                    break;
                }
            }
        }
        else
            rc = pImage ? VERR_WRONG_ORDER : VERR_MODULE_NOT_FOUND;

        supdrvLdrUnlock(pDevExt);
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
    AssertReturn(pUsage, /*VERR_NO_MEMORY*/ VERR_INTERNAL_ERROR_5);
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
            if (RT_UNLIKELY((uintptr_t)pObj->pfnDestructor - (uintptr_t)pImage->pvImage < pImage->cbImageBits))
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
        pImage->pfnModuleTerm(pImage);
    }

#ifdef VBOX_WITH_DTRACE_R0DRV
    /* Inform the tracing component. */
    supdrvVtgModuleUnloading(pDevExt, pImage);
#endif

    /* do native unload if appropriate. */
    if (pImage->fNative)
        supdrvOSLdrUnload(pDevExt, pImage);

    /* free the image */
    pImage->cUsage  = 0;
    pImage->pDevExt = NULL;
    pImage->pNext   = NULL;
    pImage->uState  = SUP_IOCTL_LDR_FREE;
    RTMemExecFree(pImage->pvImageAlloc, pImage->cbImageBits + 31);
    pImage->pvImageAlloc = NULL;
    RTMemFree(pImage->pachStrTab);
    pImage->pachStrTab = NULL;
    RTMemFree(pImage->paSymbols);
    pImage->paSymbols = NULL;
    RTMemFree(pImage);
}


/**
 * Acquires the loader lock.
 *
 * @returns IPRT status code.
 * @param   pDevExt         The device extension.
 */
DECLINLINE(int) supdrvLdrLock(PSUPDRVDEVEXT pDevExt)
{
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
    int rc = RTSemMutexRequest(pDevExt->mtxLdr, RT_INDEFINITE_WAIT);
#else
    int rc = RTSemFastMutexRequest(pDevExt->mtxLdr);
#endif
    AssertRC(rc);
    return rc;
}


/**
 * Releases the loader lock.
 *
 * @returns IPRT status code.
 * @param   pDevExt         The device extension.
 */
DECLINLINE(int) supdrvLdrUnlock(PSUPDRVDEVEXT pDevExt)
{
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
    return RTSemMutexRelease(pDevExt->mtxLdr);
#else
    return RTSemFastMutexRelease(pDevExt->mtxLdr);
#endif
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
    rc = supdrvLdrLock(pDevExt);
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
        supdrvLdrUnlock(pDevExt);

        if (pfnServiceReqHandler)
        {
            /*
             * Call it.
             */
            if (pReq->Hdr.cbIn == SUP_IOCTL_CALL_SERVICE_SIZE(0))
                rc = pfnServiceReqHandler(pSession, pReq->u.In.uOperation, pReq->u.In.u64Arg, NULL);
            else
                rc = pfnServiceReqHandler(pSession, pReq->u.In.uOperation, pReq->u.In.u64Arg, (PSUPR0SERVICEREQHDR)&pReq->abReqPkt[0]);
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
 * Creates the GIP.
 *
 * @returns VBox status code.
 * @param   pDevExt     Instance data. GIP stuff may be updated.
 */
static int supdrvGipCreate(PSUPDRVDEVEXT pDevExt)
{
    PSUPGLOBALINFOPAGE  pGip;
    RTHCPHYS            HCPhysGip;
    uint32_t            u32SystemResolution;
    uint32_t            u32Interval;
    unsigned            cCpus;
    int                 rc;


    LogFlow(("supdrvGipCreate:\n"));

    /* assert order */
    Assert(pDevExt->u32SystemTimerGranularityGrant == 0);
    Assert(pDevExt->GipMemObj == NIL_RTR0MEMOBJ);
    Assert(!pDevExt->pGipTimer);

    /*
     * Check the CPU count.
     */
    cCpus = RTMpGetArraySize();
    if (   cCpus > RTCPUSET_MAX_CPUS
        || cCpus > 256 /*ApicId is used for the mappings*/)
    {
        SUPR0Printf("VBoxDrv: Too many CPUs (%u) for the GIP (max %u)\n", cCpus, RT_MIN(RTCPUSET_MAX_CPUS, 256));
        return VERR_TOO_MANY_CPUS;
    }

    /*
     * Allocate a contiguous set of pages with a default kernel mapping.
     */
    rc = RTR0MemObjAllocCont(&pDevExt->GipMemObj, RT_UOFFSETOF(SUPGLOBALINFOPAGE, aCPUs[cCpus]), false /*fExecutable*/);
    if (RT_FAILURE(rc))
    {
        OSDBGPRINT(("supdrvGipCreate: failed to allocate the GIP page. rc=%d\n", rc));
        return rc;
    }
    pGip = (PSUPGLOBALINFOPAGE)RTR0MemObjAddress(pDevExt->GipMemObj); AssertPtr(pGip);
    HCPhysGip = RTR0MemObjGetPagePhysAddr(pDevExt->GipMemObj, 0); Assert(HCPhysGip != NIL_RTHCPHYS);

    /*
     * Find a reasonable update interval and initialize the structure.
     */
    u32Interval = u32SystemResolution = RTTimerGetSystemGranularity();
    while (u32Interval < 10000000 /* 10 ms */)
        u32Interval += u32SystemResolution;

    supdrvGipInit(pDevExt, pGip, HCPhysGip, RTTimeSystemNanoTS(), 1000000000 / u32Interval /*=Hz*/, cCpus);

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
        rc = RTMpNotificationRegister(supdrvGipMpEvent, pDevExt);
        if (RT_SUCCESS(rc))
        {
            rc = RTMpOnAll(supdrvGipInitOnCpu, pDevExt, pGip);
            if (RT_SUCCESS(rc))
            {
                /*
                 * We're good.
                 */
                Log(("supdrvGipCreate: %u ns interval.\n", u32Interval));
                g_pSUPGlobalInfoPage = pGip;
                return VINF_SUCCESS;
            }

            OSDBGPRINT(("supdrvGipCreate: RTMpOnAll failed with rc=%Rrc\n", rc));
            RTMpNotificationDeregister(supdrvGipMpEvent, pDevExt);

        }
        else
            OSDBGPRINT(("supdrvGipCreate: failed to register MP event notfication. rc=%Rrc\n", rc));
    }
    else
    {
        OSDBGPRINT(("supdrvGipCreate: failed create GIP timer at %u ns interval. rc=%Rrc\n", u32Interval, rc));
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
    g_pSUPGlobalInfoPage = NULL;

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
     * Finally, make sure we've release the system timer resolution request
     * if one actually succeeded and is still pending.
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
    uint64_t        u64TSC    = ASMReadTSC();
    uint64_t        NanoTS    = RTTimeSystemNanoTS();

    supdrvGipUpdate(pDevExt->pGip, NanoTS, u64TSC, NIL_RTCPUID, iTick);

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
    uint64_t        u64TSC    = ASMReadTSC();
    uint64_t        NanoTS    = RTTimeSystemNanoTS();

    /** @todo reset the transaction number and whatnot when iTick == 1. */
    if (pDevExt->idGipMaster == idCpu)
        supdrvGipUpdate(pDevExt->pGip, NanoTS, u64TSC, idCpu, iTick);
    else
        supdrvGipUpdatePerCpu(pDevExt->pGip, NanoTS, u64TSC, idCpu, ASMGetApicId(), iTick);

    ASMSetFlags(fOldFlags);
}


/**
 * Finds our (@a idCpu) entry, or allocates a new one if not found.
 *
 * @returns Index of the CPU in the cache set.
 * @param   pGip                The GIP.
 * @param   idCpu               The CPU ID.
 */
static uint32_t supdrvGipCpuIndexFromCpuId(PSUPGLOBALINFOPAGE pGip, RTCPUID idCpu)
{
    uint32_t i, cTries;

    /*
     * ASSUMES that CPU IDs are constant.
     */
    for (i = 0; i < pGip->cCpus; i++)
        if (pGip->aCPUs[i].idCpu == idCpu)
            return i;

    cTries = 0;
    do
    {
        for (i = 0; i < pGip->cCpus; i++)
        {
            bool fRc;
            ASMAtomicCmpXchgSize(&pGip->aCPUs[i].idCpu, idCpu, NIL_RTCPUID, fRc);
            if (fRc)
                return i;
        }
    } while (cTries++ < 32);
    AssertReleaseFailed();
    return i - 1;
}


/**
 * The calling CPU should be accounted as online, update GIP accordingly.
 *
 * This is used by supdrvGipMpEvent as well as the supdrvGipCreate.
 *
 * @param   pGip                The GIP.
 * @param   idCpu               The CPU ID.
 */
static void supdrvGipMpEventOnline(PSUPGLOBALINFOPAGE pGip, RTCPUID idCpu)
{
    int         iCpuSet = 0;
    uint16_t    idApic = UINT16_MAX;
    uint32_t    i = 0;
    uint64_t    u64NanoTS = 0;

    AssertRelease(idCpu == RTMpCpuId());
    Assert(pGip->cPossibleCpus == RTMpGetCount());

    /*
     * Update the globals.
     */
    ASMAtomicWriteU16(&pGip->cPresentCpus,  RTMpGetPresentCount());
    ASMAtomicWriteU16(&pGip->cOnlineCpus,   RTMpGetOnlineCount());
    iCpuSet = RTMpCpuIdToSetIndex(idCpu);
    if (iCpuSet >= 0)
    {
        Assert(RTCpuSetIsMemberByIndex(&pGip->PossibleCpuSet, iCpuSet));
        RTCpuSetAddByIndex(&pGip->OnlineCpuSet, iCpuSet);
        RTCpuSetAddByIndex(&pGip->PresentCpuSet, iCpuSet);
    }

    /*
     * Update the entry.
     */
    u64NanoTS = RTTimeSystemNanoTS() - pGip->u32UpdateIntervalNS;
    i = supdrvGipCpuIndexFromCpuId(pGip, idCpu);
    supdrvGipInitCpu(pGip, &pGip->aCPUs[i], u64NanoTS);
    idApic = ASMGetApicId();
    ASMAtomicWriteU16(&pGip->aCPUs[i].idApic,  idApic);
    ASMAtomicWriteS16(&pGip->aCPUs[i].iCpuSet, (int16_t)iCpuSet);
    ASMAtomicWriteSize(&pGip->aCPUs[i].idCpu,  idCpu);

    /*
     * Update the APIC ID and CPU set index mappings.
     */
    ASMAtomicWriteU16(&pGip->aiCpuFromApicId[idApic],     i);
    ASMAtomicWriteU16(&pGip->aiCpuFromCpuSetIdx[iCpuSet], i);

    /* commit it */
    ASMAtomicWriteSize(&pGip->aCPUs[i].enmState, SUPGIPCPUSTATE_ONLINE);
}


/**
 * The CPU should be accounted as offline, update the GIP accordingly.
 *
 * This is used by supdrvGipMpEvent.
 *
 * @param   pGip                The GIP.
 * @param   idCpu               The CPU ID.
 */
static void supdrvGipMpEventOffline(PSUPGLOBALINFOPAGE pGip, RTCPUID idCpu)
{
    int         iCpuSet;
    unsigned    i;

    iCpuSet = RTMpCpuIdToSetIndex(idCpu);
    AssertReturnVoid(iCpuSet >= 0);

    i = pGip->aiCpuFromCpuSetIdx[iCpuSet];
    AssertReturnVoid(i < pGip->cCpus);
    AssertReturnVoid(pGip->aCPUs[i].idCpu == idCpu);

    Assert(RTCpuSetIsMemberByIndex(&pGip->PossibleCpuSet, iCpuSet));
    RTCpuSetDelByIndex(&pGip->OnlineCpuSet, iCpuSet);

    /* commit it */
    ASMAtomicWriteSize(&pGip->aCPUs[i].enmState, SUPGIPCPUSTATE_OFFLINE);
}


/**
 * Multiprocessor event notification callback.
 *
 * This is used to make sure that the GIP master gets passed on to
 * another CPU.  It also updates the associated CPU data.
 *
 * @param   enmEvent    The event.
 * @param   idCpu       The cpu it applies to.
 * @param   pvUser      Pointer to the device extension.
 *
 * @remarks This function -must- fire on the newly online'd CPU for the
 *          RTMPEVENT_ONLINE case and can fire on any CPU for the
 *          RTMPEVENT_OFFLINE case.
 */
static DECLCALLBACK(void) supdrvGipMpEvent(RTMPEVENT enmEvent, RTCPUID idCpu, void *pvUser)
{
    PSUPDRVDEVEXT       pDevExt = (PSUPDRVDEVEXT)pvUser;
    PSUPGLOBALINFOPAGE  pGip    = pDevExt->pGip;

    AssertRelease(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /*
     * Update the GIP CPU data.
     */
    if (pGip)
    {
        switch (enmEvent)
        {
            case RTMPEVENT_ONLINE:
                AssertRelease(idCpu == RTMpCpuId());
                supdrvGipMpEventOnline(pGip, idCpu);
                break;
            case RTMPEVENT_OFFLINE:
                supdrvGipMpEventOffline(pGip, idCpu);
                break;

        }
    }

    /*
     * Make sure there is a master GIP.
     */
    if (enmEvent == RTMPEVENT_OFFLINE)
    {
        RTCPUID idGipMaster = ASMAtomicReadU32(&pDevExt->idGipMaster);
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

            Log(("supdrvGipMpEvent: Gip master %#lx -> %#lx\n", (long)idGipMaster, (long)idNewGipMaster));
            ASMAtomicCmpXchgSize(&pDevExt->idGipMaster, idNewGipMaster, idGipMaster, fIgnored);
            NOREF(fIgnored);
        }
    }
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
 * @return  false if the time stamp counters appear to be synchronized, true otherwise.
 */
static bool supdrvDetermineAsyncTsc(uint64_t *poffMin)
{
    /*
     * Just iterate all the cpus 8 times and make sure that the TSC is
     * ever increasing. We don't bother taking TSC rollover into account.
     */
    int         iEndCpu = RTMpGetArraySize();
    int         iCpu;
    int         cLoops = 8;
    bool        fAsync = false;
    int         rc = VINF_SUCCESS;
    uint64_t    offMax = 0;
    uint64_t    offMin = ~(uint64_t)0;
    uint64_t    PrevTsc = ASMReadTSC();

    while (cLoops-- > 0)
    {
        for (iCpu = 0; iCpu < iEndCpu; iCpu++)
        {
            uint64_t CurTsc;
            rc = RTMpOnSpecific(RTMpCpuIdFromSetIndex(iCpu), supdrvDetermineAsyncTscWorker, &CurTsc, NULL);
            if (RT_SUCCESS(rc))
            {
                if (CurTsc <= PrevTsc)
                {
                    fAsync = true;
                    offMin = offMax = PrevTsc - CurTsc;
                    Log(("supdrvDetermineAsyncTsc: iCpu=%d cLoops=%d CurTsc=%llx PrevTsc=%llx\n",
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
                    Log2(("%d/%d: off=%llx\n", cLoops, iCpu, off));
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
        if (iCpu < iEndCpu)
            break;
    }

    *poffMin = offMin; /* Almost RTMpOnSpecific profiling. */
    Log(("supdrvDetermineAsyncTsc: returns %d; iEndCpu=%d rc=%d offMin=%llx offMax=%llx\n",
         fAsync, iEndCpu, rc, offMin, offMax));
#if !defined(RT_OS_SOLARIS) && !defined(RT_OS_OS2) && !defined(RT_OS_WINDOWS)
    OSDBGPRINT(("vboxdrv: fAsync=%d offMin=%#lx offMax=%#lx\n", fAsync, (long)offMin, (long)offMax));
#endif
    return fAsync;
}


/**
 * Determine the GIP TSC mode.
 *
 * @returns The most suitable TSC mode.
 * @param   pDevExt     Pointer to the device instance data.
 */
static SUPGIPMODE supdrvGipDeterminTscMode(PSUPDRVDEVEXT pDevExt)
{
    /*
     * On SMP we're faced with two problems:
     *      (1) There might be a skew between the CPU, so that cpu0
     *          returns a TSC that is slightly different from cpu1.
     *      (2) Power management (and other things) may cause the TSC
     *          to run at a non-constant speed, and cause the speed
     *          to be different on the cpus. This will result in (1).
     *
     * So, on SMP systems we'll have to select the ASYNC update method
     * if there are symptoms of these problems.
     */
    if (RTMpGetCount() > 1)
    {
        uint32_t uEAX, uEBX, uECX, uEDX;
        uint64_t u64DiffCoresIgnored;

        /* Permit the user and/or the OS specific bits to force async mode. */
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
 * Initializes per-CPU GIP information.
 *
 * @param   pGip        Pointer to the read-write kernel mapping of the GIP.
 * @param   pCpu        Pointer to which GIP CPU to initalize.
 * @param   u64NanoTS   The current nanosecond timestamp.
 */
static void supdrvGipInitCpu(PSUPGLOBALINFOPAGE pGip, PSUPGIPCPU pCpu, uint64_t u64NanoTS)
{
    pCpu->u32TransactionId   = 2;
    pCpu->u64NanoTS          = u64NanoTS;
    pCpu->u64TSC             = ASMReadTSC();

    ASMAtomicWriteSize(&pCpu->enmState, SUPGIPCPUSTATE_INVALID);
    ASMAtomicWriteSize(&pCpu->idCpu,    NIL_RTCPUID);
    ASMAtomicWriteS16(&pCpu->iCpuSet,   -1);
    ASMAtomicWriteU16(&pCpu->idApic,    UINT16_MAX);

    /*
     * We don't know the following values until we've executed updates.
     * So, we'll just pretend it's a 4 GHz CPU and adjust the history it on
     * the 2nd timer callout.
     */
    pCpu->u64CpuHz          = _4G + 1; /* tstGIP-2 depends on this. */
    pCpu->u32UpdateIntervalTSC
        = pCpu->au32TSCHistory[0]
        = pCpu->au32TSCHistory[1]
        = pCpu->au32TSCHistory[2]
        = pCpu->au32TSCHistory[3]
        = pCpu->au32TSCHistory[4]
        = pCpu->au32TSCHistory[5]
        = pCpu->au32TSCHistory[6]
        = pCpu->au32TSCHistory[7]
        = (uint32_t)(_4G / pGip->u32UpdateHz);
}


/**
 * Initializes the GIP data.
 *
 * @param   pDevExt     Pointer to the device instance data.
 * @param   pGip        Pointer to the read-write kernel mapping of the GIP.
 * @param   HCPhys      The physical address of the GIP.
 * @param   u64NanoTS   The current nanosecond timestamp.
 * @param   uUpdateHz   The update frequency.
 * @param   cCpus       The CPU count.
 */
static void supdrvGipInit(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE pGip, RTHCPHYS HCPhys,
                          uint64_t u64NanoTS, unsigned uUpdateHz, unsigned cCpus)
{
    size_t const    cbGip = RT_ALIGN_Z(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs[cCpus]), PAGE_SIZE);
    unsigned        i;
#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("supdrvGipInit: pGip=%p HCPhys=%lx u64NanoTS=%llu uUpdateHz=%d cCpus=%u\n", pGip, (long)HCPhys, u64NanoTS, uUpdateHz, cCpus));
#else
    LogFlow(("supdrvGipInit: pGip=%p HCPhys=%lx u64NanoTS=%llu uUpdateHz=%d cCpus=%u\n", pGip, (long)HCPhys, u64NanoTS, uUpdateHz, cCpus));
#endif

    /*
     * Initialize the structure.
     */
    memset(pGip, 0, cbGip);
    pGip->u32Magic              = SUPGLOBALINFOPAGE_MAGIC;
    pGip->u32Version            = SUPGLOBALINFOPAGE_VERSION;
    pGip->u32Mode               = supdrvGipDeterminTscMode(pDevExt);
    pGip->cCpus                 = (uint16_t)cCpus;
    pGip->cPages                = (uint16_t)(cbGip / PAGE_SIZE);
    pGip->u32UpdateHz           = uUpdateHz;
    pGip->u32UpdateIntervalNS   = 1000000000 / uUpdateHz;
    pGip->u64NanoTSLastUpdateHz = u64NanoTS;
    RTCpuSetEmpty(&pGip->OnlineCpuSet);
    RTCpuSetEmpty(&pGip->PresentCpuSet);
    RTMpGetSet(&pGip->PossibleCpuSet);
    pGip->cOnlineCpus           = RTMpGetOnlineCount();
    pGip->cPresentCpus          = RTMpGetPresentCount();
    pGip->cPossibleCpus         = RTMpGetCount();
    pGip->idCpuMax              = RTMpGetMaxCpuId();
    for (i = 0; i < RT_ELEMENTS(pGip->aiCpuFromApicId); i++)
        pGip->aiCpuFromApicId[i]    = 0;
    for (i = 0; i < RT_ELEMENTS(pGip->aiCpuFromCpuSetIdx); i++)
        pGip->aiCpuFromCpuSetIdx[i] = UINT16_MAX;

    for (i = 0; i < cCpus; i++)
        supdrvGipInitCpu(pGip, &pGip->aCPUs[i], u64NanoTS);

    /*
     * Link it to the device extension.
     */
    pDevExt->pGip      = pGip;
    pDevExt->HCPhysGip = HCPhys;
    pDevExt->cGipUsers = 0;
}


/**
 * On CPU initialization callback for RTMpOnAll.
 *
 * @param   idCpu               The CPU ID.
 * @param   pvUser1             The device extension.
 * @param   pvUser2             The GIP.
 */
static DECLCALLBACK(void) supdrvGipInitOnCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    /* This is good enough, even though it will update some of the globals a
       bit to much. */
    supdrvGipMpEventOnline((PSUPGLOBALINFOPAGE)pvUser2, idCpu);
}


/**
 * Invalidates the GIP data upon termination.
 *
 * @param   pGip        Pointer to the read-write kernel mapping of the GIP.
 */
static void supdrvGipTerm(PSUPGLOBALINFOPAGE pGip)
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
 * @param   u64TSC          The current TSC.
 * @param   iTick           The current timer tick.
 */
static void supdrvGipDoUpdateCpu(PSUPGLOBALINFOPAGE pGip, PSUPGIPCPU pGipCpu, uint64_t u64NanoTS, uint64_t u64TSC, uint64_t iTick)
{
    uint64_t    u64TSCDelta;
    uint32_t    u32UpdateIntervalTSC;
    uint32_t    u32UpdateIntervalTSCSlack;
    unsigned    iTSCHistoryHead;
    uint64_t    u64CpuHz;
    uint32_t    u32TransactionId;

    /* Delta between this and the previous update. */
    ASMAtomicUoWriteU32(&pGipCpu->u32PrevUpdateIntervalNS, (uint32_t)(u64NanoTS - pGipCpu->u64NanoTS));

    /*
     * Update the NanoTS.
     */
    ASMAtomicWriteU64(&pGipCpu->u64NanoTS, u64NanoTS);

    /*
     * Calc TSC delta.
     */
    /** @todo validate the NanoTS delta, don't trust the OS to call us when it should... */
    u64TSCDelta = u64TSC - pGipCpu->u64TSC;
    ASMAtomicWriteU64(&pGipCpu->u64TSC, u64TSC);

    if (u64TSCDelta >> 32)
    {
        u64TSCDelta = pGipCpu->u32UpdateIntervalTSC;
        pGipCpu->cErrors++;
    }

    /*
     * On the 2nd and 3rd callout, reset the history with the current TSC
     * interval since the values entered by supdrvGipInit are totally off.
     * The interval on the 1st callout completely unreliable, the 2nd is a bit
     * better, while the 3rd should be most reliable.
     */
    u32TransactionId = pGipCpu->u32TransactionId;
    if (RT_UNLIKELY(   (   u32TransactionId == 5
                        || u32TransactionId == 7)
                    && (   iTick == 2
                        || iTick == 3) ))
    {
        unsigned i;
        for (i = 0; i < RT_ELEMENTS(pGipCpu->au32TSCHistory); i++)
            ASMAtomicUoWriteU32(&pGipCpu->au32TSCHistory[i], (uint32_t)u64TSCDelta);
    }

    /*
     * TSC History.
     */
    Assert(RT_ELEMENTS(pGipCpu->au32TSCHistory) == 8);
    iTSCHistoryHead = (pGipCpu->iTSCHistoryHead + 1) & 7;
    ASMAtomicWriteU32(&pGipCpu->iTSCHistoryHead, iTSCHistoryHead);
    ASMAtomicWriteU32(&pGipCpu->au32TSCHistory[iTSCHistoryHead], (uint32_t)u64TSCDelta);

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

        /* Value chosen for a 2GHz Athlon64 running linux 2.6.10/11, . */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 14;
    }
    else if (pGip->u32UpdateHz >= 90)
    {
        u32UpdateIntervalTSC  = (uint32_t)u64TSCDelta;
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[(iTSCHistoryHead - 1) & 7];
        u32UpdateIntervalTSC >>= 1;

        /* value chosen on a 2GHz thinkpad running windows */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 7;
    }
    else
    {
        u32UpdateIntervalTSC  = (uint32_t)u64TSCDelta;

        /* This value hasn't be checked yet.. waiting for OS/2 and 33Hz timers.. :-) */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 6;
    }
    ASMAtomicWriteU32(&pGipCpu->u32UpdateIntervalTSC, u32UpdateIntervalTSC + u32UpdateIntervalTSCSlack);

    /*
     * CpuHz.
     */
    u64CpuHz = ASMMult2xU32RetU64(u32UpdateIntervalTSC, pGip->u32UpdateHz);
    ASMAtomicWriteU64(&pGipCpu->u64CpuHz, u64CpuHz);
}


/**
 * Updates the GIP.
 *
 * @param   pGip            Pointer to the GIP.
 * @param   u64NanoTS       The current nanosecond timesamp.
 * @param   u64TSC          The current TSC timesamp.
 * @param   idCpu           The CPU ID.
 * @param   iTick           The current timer tick.
 */
static void supdrvGipUpdate(PSUPGLOBALINFOPAGE pGip, uint64_t u64NanoTS, uint64_t u64TSC, RTCPUID idCpu, uint64_t iTick)
{
    /*
     * Determine the relevant CPU data.
     */
    PSUPGIPCPU pGipCpu;
    if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
        pGipCpu = &pGip->aCPUs[0];
    else
    {
        unsigned iCpu = pGip->aiCpuFromApicId[ASMGetApicId()];
        if (RT_UNLIKELY(iCpu >= pGip->cCpus))
            return;
        pGipCpu = &pGip->aCPUs[iCpu];
        if (RT_UNLIKELY(pGipCpu->idCpu != idCpu))
            return;
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
                ASMAtomicWriteU32(&pGip->u32UpdateHz, u32UpdateHz);
                ASMAtomicWriteU32(&pGip->u32UpdateIntervalNS, 1000000000 / u32UpdateHz);
            }
#endif
        }
        ASMAtomicWriteU64(&pGip->u64NanoTSLastUpdateHz, u64NanoTS);
    }

    /*
     * Update the data.
     */
    supdrvGipDoUpdateCpu(pGip, pGipCpu, u64NanoTS, u64TSC, iTick);

    /*
     * Complete transaction.
     */
    ASMAtomicIncU32(&pGipCpu->u32TransactionId);
}


/**
 * Updates the per cpu GIP data for the calling cpu.
 *
 * @param   pGip            Pointer to the GIP.
 * @param   u64NanoTS       The current nanosecond timesamp.
 * @param   u64TSC          The current TSC timesamp.
 * @param   idCpu           The CPU ID.
 * @param   idApic          The APIC id for the CPU index.
 * @param   iTick           The current timer tick.
 */
static void supdrvGipUpdatePerCpu(PSUPGLOBALINFOPAGE pGip, uint64_t u64NanoTS, uint64_t u64TSC,
                                  RTCPUID idCpu, uint8_t idApic, uint64_t iTick)
{
    uint32_t iCpu;

    /*
     * Avoid a potential race when a CPU online notification doesn't fire on
     * the onlined CPU but the tick creeps in before the event notification is
     * run.
     */
    if (RT_UNLIKELY(iTick == 1))
    {
        iCpu = supdrvGipCpuIndexFromCpuId(pGip, idCpu);
        if (pGip->aCPUs[iCpu].enmState == SUPGIPCPUSTATE_OFFLINE)
            supdrvGipMpEventOnline(pGip, idCpu);
    }

    iCpu = pGip->aiCpuFromApicId[idApic];
    if (RT_LIKELY(iCpu < pGip->cCpus))
    {
        PSUPGIPCPU pGipCpu = &pGip->aCPUs[iCpu];
        if (pGipCpu->idCpu == idCpu)
        {
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
            supdrvGipDoUpdateCpu(pGip, pGipCpu, u64NanoTS, u64TSC, iTick);

            /*
             * Complete transaction.
             */
            ASMAtomicIncU32(&pGipCpu->u32TransactionId);
        }
    }
}

