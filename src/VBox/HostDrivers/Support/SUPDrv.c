/* $Id$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Common code.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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
#include <VBox/vmm/hm_svm.h>
#include <VBox/vmm/hm_vmx.h>

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
 * u32UpdateIntervalNS GIP members. The value must be a power of 2.
 *
 * Warning: Bumping this too high might overflow u32UpdateIntervalNS.
 */
#define GIP_UPDATEHZ_RECALC_FREQ            0x800

/** A reserved TSC value used for synchronization as well as measurement of
 *  TSC deltas. */
#define GIP_TSC_DELTA_RSVD                  UINT64_MAX
/** The number of TSC delta measurement loops in total (includes primer and
 *  read-time loops). */
#define GIP_TSC_DELTA_LOOPS                 96
/** The number of cache primer loops. */
#define GIP_TSC_DELTA_PRIMER_LOOPS          4
/** The number of loops until we keep computing the minumum read time. */
#define GIP_TSC_DELTA_READ_TIME_LOOPS       24
/** Stop measurement of TSC delta. */
#define GIP_TSC_DELTA_SYNC_STOP             0
/** Start measurement of TSC delta. */
#define GIP_TSC_DELTA_SYNC_START            1
/** Worker thread is ready for reading the TSC. */
#define GIP_TSC_DELTA_SYNC_WORKER_READY     2
/** Worker thread is done updating TSC delta info. */
#define GIP_TSC_DELTA_SYNC_WORKER_DONE      3
/** When IPRT is isn't concurrent safe: Master is ready and will wait for worker
 *  with a timeout. */
#define GIP_TSC_DELTA_SYNC_PRESTART_MASTER  4
/** When IPRT is isn't concurrent safe: Worker is ready after waiting for
 *  master with a timeout. */
#define GIP_TSC_DELTA_SYNC_PRESTART_WORKER  5

AssertCompile(GIP_TSC_DELTA_PRIMER_LOOPS < GIP_TSC_DELTA_READ_TIME_LOOPS);
AssertCompile(GIP_TSC_DELTA_PRIMER_LOOPS + GIP_TSC_DELTA_READ_TIME_LOOPS < GIP_TSC_DELTA_LOOPS);

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
static int                  supdrvIOCtl_MsrProber(PSUPDRVDEVEXT pDevExt, PSUPMSRPROBER pReq);
static int                  supdrvIOCtl_TscDeltaMeasure(PSUPDRVDEVEXT pDevExt, PSUPTSCDELTAMEASURE pReq);
static int                  supdrvIOCtl_TscRead(PSUPDRVDEVEXT pDevExt, PSUPTSCREAD pReq);
static int                  supdrvGipCreate(PSUPDRVDEVEXT pDevExt);
static void                 supdrvGipDestroy(PSUPDRVDEVEXT pDevExt);
static DECLCALLBACK(void)   supdrvGipSyncTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick);
static DECLCALLBACK(void)   supdrvGipAsyncTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick);
static DECLCALLBACK(void)   supdrvGipMpEvent(RTMPEVENT enmEvent, RTCPUID idCpu, void *pvUser);
static bool                 supdrvIsInvariantTsc(void);
static void                 supdrvGipInit(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE pGip, RTHCPHYS HCPhys,
                                          uint64_t u64NanoTS, unsigned uUpdateHz, unsigned uUpdateIntervalNS, unsigned cCpus);
static DECLCALLBACK(void)   supdrvGipInitOnCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static void                 supdrvGipTerm(PSUPGLOBALINFOPAGE pGip);
static void                 supdrvGipUpdate(PSUPDRVDEVEXT pDevExt, uint64_t u64NanoTS, uint64_t u64TSC, RTCPUID idCpu, uint64_t iTick);
static void                 supdrvGipUpdatePerCpu(PSUPDRVDEVEXT pDevExt, uint64_t u64NanoTS, uint64_t u64TSC,
                                                  RTCPUID idCpu, uint8_t idApic, uint64_t iTick);
static void                 supdrvGipInitCpu(PSUPGLOBALINFOPAGE pGip, PSUPGIPCPU pCpu, uint64_t u64NanoTS);
static int                  supdrvMeasureTscDeltas(PSUPDRVDEVEXT pDevExt, uint32_t *pidxMaster);
static int                  supdrvMeasureTscDeltaOne(PSUPDRVDEVEXT pDevExt, uint32_t idxWorker);
static int                  supdrvIOCtl_ResumeSuspendedKbds(void);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
DECLEXPORT(PSUPGLOBALINFOPAGE) g_pSUPGlobalInfoPage = NULL;

/**
 * The TSC delta synchronization struct. rounded to cache line size.
 */
typedef union SUPTSCDELTASYNC
{
    /** The synchronization variable, holds values GIP_TSC_DELTA_SYNC_*. */
    volatile uint32_t   u;
    /** Padding to cache line size. */
    uint8_t             u8Padding[64];
} SUPTSCDELTASYNC;
AssertCompileSize(SUPTSCDELTASYNC, 64);
typedef SUPTSCDELTASYNC *PSUPTSCDELTASYNC;

/** Pointer to the TSC delta sync. struct. */
static void                *g_pvTscDeltaSync;
/** Aligned pointer to the TSC delta sync. struct. */
static PSUPTSCDELTASYNC     g_pTscDeltaSync;
/** The TSC delta measurement initiator Cpu Id. */
static volatile RTCPUID     g_idTscDeltaInitiator = NIL_RTCPUID;
/** Number of online/offline events, incremented each time a CPU goes online
 *  or offline. */
static volatile uint32_t    g_cMpOnOffEvents;

/**
 * Array of the R0 SUP API.
 */
static SUPFUNC g_aFunctions[] =
{
/* SED: START */
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
    { "g_pSUPGlobalInfoPage",                   (void *)&g_pSUPGlobalInfoPage },            /* SED: DATA */
    { "SUPGetGIP",                              (void *)SUPGetGIP },
    { "SUPR0ComponentDeregisterFactory",        (void *)SUPR0ComponentDeregisterFactory },
    { "SUPR0ComponentQueryFactory",             (void *)SUPR0ComponentQueryFactory },
    { "SUPR0ComponentRegisterFactory",          (void *)SUPR0ComponentRegisterFactory },
    { "SUPR0ContAlloc",                         (void *)SUPR0ContAlloc },
    { "SUPR0ContFree",                          (void *)SUPR0ContFree },
    { "SUPR0EnableVTx",                         (void *)SUPR0EnableVTx },
    { "SUPR0SuspendVTxOnCpu",                   (void *)SUPR0SuspendVTxOnCpu },
    { "SUPR0ResumeVTxOnCpu",                    (void *)SUPR0ResumeVTxOnCpu },
    { "SUPR0GetPagingMode",                     (void *)SUPR0GetPagingMode },
    { "SUPR0GetKernelFeatures",                 (void *)SUPR0GetKernelFeatures },
    { "SUPR0LockMem",                           (void *)SUPR0LockMem },
    { "SUPR0LowAlloc",                          (void *)SUPR0LowAlloc },
    { "SUPR0LowFree",                           (void *)SUPR0LowFree },
    { "SUPR0MemAlloc",                          (void *)SUPR0MemAlloc },
    { "SUPR0MemFree",                           (void *)SUPR0MemFree },
    { "SUPR0MemGetPhys",                        (void *)SUPR0MemGetPhys },
    { "SUPR0ObjAddRef",                         (void *)SUPR0ObjAddRef },
    { "SUPR0ObjAddRefEx",                       (void *)SUPR0ObjAddRefEx },
    { "SUPR0ObjRegister",                       (void *)SUPR0ObjRegister },
    { "SUPR0ObjRelease",                        (void *)SUPR0ObjRelease },
    { "SUPR0ObjVerifyAccess",                   (void *)SUPR0ObjVerifyAccess },
    { "SUPR0PageAllocEx",                       (void *)SUPR0PageAllocEx },
    { "SUPR0PageFree",                          (void *)SUPR0PageFree },
    { "SUPR0Printf",                            (void *)SUPR0Printf },
    { "SUPR0TracerDeregisterDrv",               (void *)SUPR0TracerDeregisterDrv },
    { "SUPR0TracerDeregisterImpl",              (void *)SUPR0TracerDeregisterImpl },
    { "SUPR0TracerFireProbe",                   (void *)SUPR0TracerFireProbe },
    { "SUPR0TracerRegisterDrv",                 (void *)SUPR0TracerRegisterDrv },
    { "SUPR0TracerRegisterImpl",                (void *)SUPR0TracerRegisterImpl },
    { "SUPR0TracerRegisterModule",              (void *)SUPR0TracerRegisterModule },
    { "SUPR0TracerUmodProbeFire",               (void *)SUPR0TracerUmodProbeFire },
    { "SUPR0UnlockMem",                         (void *)SUPR0UnlockMem },
    { "SUPSemEventClose",                       (void *)SUPSemEventClose },
    { "SUPSemEventCreate",                      (void *)SUPSemEventCreate },
    { "SUPSemEventGetResolution",               (void *)SUPSemEventGetResolution },
    { "SUPSemEventMultiClose",                  (void *)SUPSemEventMultiClose },
    { "SUPSemEventMultiCreate",                 (void *)SUPSemEventMultiCreate },
    { "SUPSemEventMultiGetResolution",          (void *)SUPSemEventMultiGetResolution },
    { "SUPSemEventMultiReset",                  (void *)SUPSemEventMultiReset },
    { "SUPSemEventMultiSignal",                 (void *)SUPSemEventMultiSignal },
    { "SUPSemEventMultiWait",                   (void *)SUPSemEventMultiWait },
    { "SUPSemEventMultiWaitNoResume",           (void *)SUPSemEventMultiWaitNoResume },
    { "SUPSemEventMultiWaitNsAbsIntr",          (void *)SUPSemEventMultiWaitNsAbsIntr },
    { "SUPSemEventMultiWaitNsRelIntr",          (void *)SUPSemEventMultiWaitNsRelIntr },
    { "SUPSemEventSignal",                      (void *)SUPSemEventSignal },
    { "SUPSemEventWait",                        (void *)SUPSemEventWait },
    { "SUPSemEventWaitNoResume",                (void *)SUPSemEventWaitNoResume },
    { "SUPSemEventWaitNsAbsIntr",               (void *)SUPSemEventWaitNsAbsIntr },
    { "SUPSemEventWaitNsRelIntr",               (void *)SUPSemEventWaitNsRelIntr },

    { "RTAssertAreQuiet",                       (void *)RTAssertAreQuiet },
    { "RTAssertMayPanic",                       (void *)RTAssertMayPanic },
    { "RTAssertMsg1",                           (void *)RTAssertMsg1 },
    { "RTAssertMsg2AddV",                       (void *)RTAssertMsg2AddV },
    { "RTAssertMsg2V",                          (void *)RTAssertMsg2V },
    { "RTAssertSetMayPanic",                    (void *)RTAssertSetMayPanic },
    { "RTAssertSetQuiet",                       (void *)RTAssertSetQuiet },
    { "RTCrc32",                                (void *)RTCrc32 },
    { "RTCrc32Finish",                          (void *)RTCrc32Finish },
    { "RTCrc32Process",                         (void *)RTCrc32Process },
    { "RTCrc32Start",                           (void *)RTCrc32Start },
    { "RTErrConvertFromErrno",                  (void *)RTErrConvertFromErrno },
    { "RTErrConvertToErrno",                    (void *)RTErrConvertToErrno },
    { "RTHandleTableAllocWithCtx",              (void *)RTHandleTableAllocWithCtx },
    { "RTHandleTableCreate",                    (void *)RTHandleTableCreate },
    { "RTHandleTableCreateEx",                  (void *)RTHandleTableCreateEx },
    { "RTHandleTableDestroy",                   (void *)RTHandleTableDestroy },
    { "RTHandleTableFreeWithCtx",               (void *)RTHandleTableFreeWithCtx },
    { "RTHandleTableLookupWithCtx",             (void *)RTHandleTableLookupWithCtx },
    { "RTLogDefaultInstance",                   (void *)RTLogDefaultInstance },
    { "RTLogGetDefaultInstance",                (void *)RTLogGetDefaultInstance },
    { "RTLogLoggerExV",                         (void *)RTLogLoggerExV },
    { "RTLogPrintfV",                           (void *)RTLogPrintfV },
    { "RTLogRelDefaultInstance",                (void *)RTLogRelDefaultInstance },
    { "RTLogSetDefaultInstanceThread",          (void *)RTLogSetDefaultInstanceThread },
    { "RTMemAllocExTag",                        (void *)RTMemAllocExTag },
    { "RTMemAllocTag",                          (void *)RTMemAllocTag },
    { "RTMemAllocVarTag",                       (void *)RTMemAllocVarTag },
    { "RTMemAllocZTag",                         (void *)RTMemAllocZTag },
    { "RTMemAllocZVarTag",                      (void *)RTMemAllocZVarTag },
    { "RTMemDupExTag",                          (void *)RTMemDupExTag },
    { "RTMemDupTag",                            (void *)RTMemDupTag },
    { "RTMemFree",                              (void *)RTMemFree },
    { "RTMemFreeEx",                            (void *)RTMemFreeEx },
    { "RTMemReallocTag",                        (void *)RTMemReallocTag },
    { "RTMpCpuId",                              (void *)RTMpCpuId },
    { "RTMpCpuIdFromSetIndex",                  (void *)RTMpCpuIdFromSetIndex },
    { "RTMpCpuIdToSetIndex",                    (void *)RTMpCpuIdToSetIndex },
    { "RTMpGetArraySize",                       (void *)RTMpGetArraySize },
    { "RTMpGetCount",                           (void *)RTMpGetCount },
    { "RTMpGetMaxCpuId",                        (void *)RTMpGetMaxCpuId },
    { "RTMpGetOnlineCount",                     (void *)RTMpGetOnlineCount },
    { "RTMpGetOnlineSet",                       (void *)RTMpGetOnlineSet },
    { "RTMpGetSet",                             (void *)RTMpGetSet },
    { "RTMpIsCpuOnline",                        (void *)RTMpIsCpuOnline },
    { "RTMpIsCpuPossible",                      (void *)RTMpIsCpuPossible },
    { "RTMpIsCpuWorkPending",                   (void *)RTMpIsCpuWorkPending },
    { "RTMpNotificationDeregister",             (void *)RTMpNotificationDeregister },
    { "RTMpNotificationRegister",               (void *)RTMpNotificationRegister },
    { "RTMpOnAll",                              (void *)RTMpOnAll },
    { "RTMpOnOthers",                           (void *)RTMpOnOthers },
    { "RTMpOnSpecific",                         (void *)RTMpOnSpecific },
    { "RTMpPokeCpu",                            (void *)RTMpPokeCpu },
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
    { "RTPowerNotificationDeregister",          (void *)RTPowerNotificationDeregister },
    { "RTPowerNotificationRegister",            (void *)RTPowerNotificationRegister },
    { "RTProcSelf",                             (void *)RTProcSelf },
    { "RTR0AssertPanicSystem",                  (void *)RTR0AssertPanicSystem },
    { "RTR0MemAreKrnlAndUsrDifferent",          (void *)RTR0MemAreKrnlAndUsrDifferent },
    { "RTR0MemKernelIsValidAddr",               (void *)RTR0MemKernelIsValidAddr },
    { "RTR0MemKernelCopyFrom",                  (void *)RTR0MemKernelCopyFrom },
    { "RTR0MemKernelCopyTo",                    (void *)RTR0MemKernelCopyTo },
    { "RTR0MemObjAddress",                      (void *)RTR0MemObjAddress },
    { "RTR0MemObjAddressR3",                    (void *)RTR0MemObjAddressR3 },
    { "RTR0MemObjAllocContTag",                 (void *)RTR0MemObjAllocContTag },
    { "RTR0MemObjAllocLowTag",                  (void *)RTR0MemObjAllocLowTag },
    { "RTR0MemObjAllocPageTag",                 (void *)RTR0MemObjAllocPageTag },
    { "RTR0MemObjAllocPhysExTag",               (void *)RTR0MemObjAllocPhysExTag },
    { "RTR0MemObjAllocPhysNCTag",               (void *)RTR0MemObjAllocPhysNCTag },
    { "RTR0MemObjAllocPhysTag",                 (void *)RTR0MemObjAllocPhysTag },
    { "RTR0MemObjEnterPhysTag",                 (void *)RTR0MemObjEnterPhysTag },
    { "RTR0MemObjFree",                         (void *)RTR0MemObjFree },
    { "RTR0MemObjGetPagePhysAddr",              (void *)RTR0MemObjGetPagePhysAddr },
    { "RTR0MemObjIsMapping",                    (void *)RTR0MemObjIsMapping },
    { "RTR0MemObjLockUserTag",                  (void *)RTR0MemObjLockUserTag },
    { "RTR0MemObjMapKernelExTag",               (void *)RTR0MemObjMapKernelExTag },
    { "RTR0MemObjMapKernelTag",                 (void *)RTR0MemObjMapKernelTag },
    { "RTR0MemObjMapUserTag",                   (void *)RTR0MemObjMapUserTag },
    { "RTR0MemObjProtect",                      (void *)RTR0MemObjProtect },
    { "RTR0MemObjSize",                         (void *)RTR0MemObjSize },
    { "RTR0MemUserCopyFrom",                    (void *)RTR0MemUserCopyFrom },
    { "RTR0MemUserCopyTo",                      (void *)RTR0MemUserCopyTo },
    { "RTR0MemUserIsValidAddr",                 (void *)RTR0MemUserIsValidAddr },
    { "RTR0ProcHandleSelf",                     (void *)RTR0ProcHandleSelf },
    { "RTSemEventCreate",                       (void *)RTSemEventCreate },
    { "RTSemEventDestroy",                      (void *)RTSemEventDestroy },
    { "RTSemEventGetResolution",                (void *)RTSemEventGetResolution },
    { "RTSemEventMultiCreate",                  (void *)RTSemEventMultiCreate },
    { "RTSemEventMultiDestroy",                 (void *)RTSemEventMultiDestroy },
    { "RTSemEventMultiGetResolution",           (void *)RTSemEventMultiGetResolution },
    { "RTSemEventMultiReset",                   (void *)RTSemEventMultiReset },
    { "RTSemEventMultiSignal",                  (void *)RTSemEventMultiSignal },
    { "RTSemEventMultiWait",                    (void *)RTSemEventMultiWait },
    { "RTSemEventMultiWaitEx",                  (void *)RTSemEventMultiWaitEx },
    { "RTSemEventMultiWaitExDebug",             (void *)RTSemEventMultiWaitExDebug },
    { "RTSemEventMultiWaitNoResume",            (void *)RTSemEventMultiWaitNoResume },
    { "RTSemEventSignal",                       (void *)RTSemEventSignal },
    { "RTSemEventWait",                         (void *)RTSemEventWait },
    { "RTSemEventWaitEx",                       (void *)RTSemEventWaitEx },
    { "RTSemEventWaitExDebug",                  (void *)RTSemEventWaitExDebug },
    { "RTSemEventWaitNoResume",                 (void *)RTSemEventWaitNoResume },
    { "RTSemFastMutexCreate",                   (void *)RTSemFastMutexCreate },
    { "RTSemFastMutexDestroy",                  (void *)RTSemFastMutexDestroy },
    { "RTSemFastMutexRelease",                  (void *)RTSemFastMutexRelease },
    { "RTSemFastMutexRequest",                  (void *)RTSemFastMutexRequest },
    { "RTSemMutexCreate",                       (void *)RTSemMutexCreate },
    { "RTSemMutexDestroy",                      (void *)RTSemMutexDestroy },
    { "RTSemMutexRelease",                      (void *)RTSemMutexRelease },
    { "RTSemMutexRequest",                      (void *)RTSemMutexRequest },
    { "RTSemMutexRequestDebug",                 (void *)RTSemMutexRequestDebug },
    { "RTSemMutexRequestNoResume",              (void *)RTSemMutexRequestNoResume },
    { "RTSemMutexRequestNoResumeDebug",         (void *)RTSemMutexRequestNoResumeDebug },
    { "RTSpinlockAcquire",                      (void *)RTSpinlockAcquire },
    { "RTSpinlockCreate",                       (void *)RTSpinlockCreate },
    { "RTSpinlockDestroy",                      (void *)RTSpinlockDestroy },
    { "RTSpinlockRelease",                      (void *)RTSpinlockRelease },
    { "RTStrCopy",                              (void *)RTStrCopy },
    { "RTStrDupTag",                            (void *)RTStrDupTag },
    { "RTStrFormat",                            (void *)RTStrFormat },
    { "RTStrFormatNumber",                      (void *)RTStrFormatNumber },
    { "RTStrFormatTypeDeregister",              (void *)RTStrFormatTypeDeregister },
    { "RTStrFormatTypeRegister",                (void *)RTStrFormatTypeRegister },
    { "RTStrFormatTypeSetUser",                 (void *)RTStrFormatTypeSetUser },
    { "RTStrFormatV",                           (void *)RTStrFormatV },
    { "RTStrFree",                              (void *)RTStrFree },
    { "RTStrNCmp",                              (void *)RTStrNCmp },
    { "RTStrPrintf",                            (void *)RTStrPrintf },
    { "RTStrPrintfEx",                          (void *)RTStrPrintfEx },
    { "RTStrPrintfExV",                         (void *)RTStrPrintfExV },
    { "RTStrPrintfV",                           (void *)RTStrPrintfV },
    { "RTThreadCreate",                         (void *)RTThreadCreate },
    { "RTThreadCtxHooksAreRegistered",          (void *)RTThreadCtxHooksAreRegistered },
    { "RTThreadCtxHooksCreate",                 (void *)RTThreadCtxHooksCreate },
    { "RTThreadCtxHooksDeregister",             (void *)RTThreadCtxHooksDeregister },
    { "RTThreadCtxHooksRegister",               (void *)RTThreadCtxHooksRegister },
    { "RTThreadCtxHooksRelease",                (void *)RTThreadCtxHooksRelease },
    { "RTThreadCtxHooksRetain",                 (void *)RTThreadCtxHooksRetain },
    { "RTThreadGetName",                        (void *)RTThreadGetName },
    { "RTThreadGetNative",                      (void *)RTThreadGetNative },
    { "RTThreadGetType",                        (void *)RTThreadGetType },
    { "RTThreadIsInInterrupt",                  (void *)RTThreadIsInInterrupt },
    { "RTThreadNativeSelf",                     (void *)RTThreadNativeSelf },
    { "RTThreadPreemptDisable",                 (void *)RTThreadPreemptDisable },
    { "RTThreadPreemptIsEnabled",               (void *)RTThreadPreemptIsEnabled },
    { "RTThreadPreemptIsPending",               (void *)RTThreadPreemptIsPending },
    { "RTThreadPreemptIsPendingTrusty",         (void *)RTThreadPreemptIsPendingTrusty },
    { "RTThreadPreemptIsPossible",              (void *)RTThreadPreemptIsPossible },
    { "RTThreadPreemptRestore",                 (void *)RTThreadPreemptRestore },
    { "RTThreadSelf",                           (void *)RTThreadSelf },
    { "RTThreadSelfName",                       (void *)RTThreadSelfName },
    { "RTThreadSleep",                          (void *)RTThreadSleep },
    { "RTThreadUserReset",                      (void *)RTThreadUserReset },
    { "RTThreadUserSignal",                     (void *)RTThreadUserSignal },
    { "RTThreadUserWait",                       (void *)RTThreadUserWait },
    { "RTThreadUserWaitNoResume",               (void *)RTThreadUserWaitNoResume },
    { "RTThreadWait",                           (void *)RTThreadWait },
    { "RTThreadWaitNoResume",                   (void *)RTThreadWaitNoResume },
    { "RTThreadYield",                          (void *)RTThreadYield },
    { "RTTimeMilliTS",                          (void *)RTTimeMilliTS },
    { "RTTimeNanoTS",                           (void *)RTTimeNanoTS },
    { "RTTimeNow",                              (void *)RTTimeNow },
    { "RTTimerCanDoHighResolution",             (void *)RTTimerCanDoHighResolution },
    { "RTTimerChangeInterval",                  (void *)RTTimerChangeInterval },
    { "RTTimerCreate",                          (void *)RTTimerCreate },
    { "RTTimerCreateEx",                        (void *)RTTimerCreateEx },
    { "RTTimerDestroy",                         (void *)RTTimerDestroy },
    { "RTTimerGetSystemGranularity",            (void *)RTTimerGetSystemGranularity },
    { "RTTimerReleaseSystemGranularity",        (void *)RTTimerReleaseSystemGranularity },
    { "RTTimerRequestSystemGranularity",        (void *)RTTimerRequestSystemGranularity },
    { "RTTimerStart",                           (void *)RTTimerStart },
    { "RTTimerStop",                            (void *)RTTimerStop },
    { "RTTimeSystemMilliTS",                    (void *)RTTimeSystemMilliTS },
    { "RTTimeSystemNanoTS",                     (void *)RTTimeSystemNanoTS },
    { "RTUuidCompare",                          (void *)RTUuidCompare },
    { "RTUuidCompareStr",                       (void *)RTUuidCompareStr },
    { "RTUuidFromStr",                          (void *)RTUuidFromStr },
/* SED: END */
};

#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
/**
 * Drag in the rest of IRPT since we share it with the
 * rest of the kernel modules on darwin.
 */
PFNRT g_apfnVBoxDrvIPRTDeps[] =
{
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
    memset(pDevExt, 0, sizeof(*pDevExt)); /* Does not wipe OS specific tail section of the structure. */
    pDevExt->Spinlock = NIL_RTSPINLOCK;
    pDevExt->hGipSpinlock = NIL_RTSPINLOCK;
    pDevExt->hSessionHashTabSpinlock = NIL_RTSPINLOCK;
    rc = RTSpinlockCreate(&pDevExt->Spinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "SUPDrvDevExt");
    if (RT_SUCCESS(rc))
        rc = RTSpinlockCreate(&pDevExt->hGipSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "SUPDrvGip");
    if (RT_SUCCESS(rc))
        rc = RTSpinlockCreate(&pDevExt->hSessionHashTabSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "SUPDrvSession");

    if (RT_SUCCESS(rc))
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
                    rc = supdrvTracerInit(pDevExt);
                    if (RT_SUCCESS(rc))
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

                    supdrvGipDestroy(pDevExt);
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
    RTSpinlockDestroy(pDevExt->hGipSpinlock);
    pDevExt->hGipSpinlock = NIL_RTSPINLOCK;
    RTSpinlockDestroy(pDevExt->hSessionHashTabSpinlock);
    pDevExt->hSessionHashTabSpinlock = NIL_RTSPINLOCK;

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
    RTSpinlockDestroy(pDevExt->hSessionHashTabSpinlock);
    pDevExt->hSessionHashTabSpinlock = NIL_RTSPINLOCK;

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
    RTSpinlockDestroy(pDevExt->hGipSpinlock);
    pDevExt->hGipSpinlock = NIL_RTSPINLOCK;

    supdrvTracerTerm(pDevExt);

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
 * @param   pDevExt         Device extension.
 * @param   fUser           Flag indicating whether this is a user or kernel
 *                          session.
 * @param   fUnrestricted   Unrestricted access (system) or restricted access
 *                          (user)?
 * @param   ppSession       Where to store the pointer to the session data.
 */
int VBOXCALL supdrvCreateSession(PSUPDRVDEVEXT pDevExt, bool fUser, bool fUnrestricted, PSUPDRVSESSION *ppSession)
{
    int             rc;
    PSUPDRVSESSION  pSession;

    if (!SUP_IS_DEVEXT_VALID(pDevExt))
        return VERR_INVALID_PARAMETER;

    /*
     * Allocate memory for the session data.
     */
    pSession = *ppSession = (PSUPDRVSESSION)RTMemAllocZ(pDevExt->cbSession);
    if (pSession)
    {
        /* Initialize session data. */
        rc = RTSpinlockCreate(&pSession->Spinlock, RTSPINLOCK_FLAGS_INTERRUPT_UNSAFE, "SUPDrvSession");
        if (!rc)
        {
            rc = RTHandleTableCreateEx(&pSession->hHandleTable,
                                       RTHANDLETABLE_FLAGS_LOCKED_IRQ_SAFE | RTHANDLETABLE_FLAGS_CONTEXT,
                                       1 /*uBase*/, 32768 /*cMax*/, supdrvSessionObjHandleRetain, pSession);
            if (RT_SUCCESS(rc))
            {
                Assert(pSession->Spinlock != NIL_RTSPINLOCK);
                pSession->pDevExt           = pDevExt;
                pSession->u32Cookie         = BIRD_INV;
                pSession->fUnrestricted     = fUnrestricted;
                /*pSession->fInHashTable      = false; */
                pSession->cRefs             = 1;
                /*pSession->pCommonNextHash   = NULL;
                pSession->ppOsSessionPtr    = NULL; */
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
                /*pSession->pLdrUsage         = NULL;
                pSession->pVM               = NULL;
                pSession->pUsage            = NULL;
                pSession->pGip              = NULL;
                pSession->fGipReferenced    = false;
                pSession->Bundle.cUsed      = 0; */
                pSession->Uid               = NIL_RTUID;
                pSession->Gid               = NIL_RTGID;
                /*pSession->uTracerData       = 0;*/
                pSession->hTracerCaller     = NIL_RTNATIVETHREAD;
                RTListInit(&pSession->TpProviders);
                /*pSession->cTpProviders      = 0;*/
                /*pSession->cTpProbesFiring   = 0;*/
                RTListInit(&pSession->TpUmods);
                /*RT_ZERO(pSession->apTpLookupTable);*/

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
 * Cleans up the session in the context of the process to which it belongs, the
 * caller will free the session and the session spinlock.
 *
 * This should normally occur when the session is closed or as the process
 * exits.  Careful reference counting in the OS specfic code makes sure that
 * there cannot be any races between process/handle cleanup callbacks and
 * threads doing I/O control calls.
 *
 * @param   pDevExt     The device extension.
 * @param   pSession    Session data.
 */
static void supdrvCleanupSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    int                 rc;
    PSUPDRVBUNDLE       pBundle;
    LogFlow(("supdrvCleanupSession: pSession=%p\n", pSession));

    Assert(!pSession->fInHashTable);
    Assert(!pSession->ppOsSessionPtr);
    AssertReleaseMsg(pSession->R0Process == RTR0ProcHandleSelf() || pSession->R0Process == NIL_RTR0PROCESS,
                     ("R0Process=%p cur=%p; Process=%u curpid=%u\n", RTR0ProcHandleSelf(), RTProcSelf()));

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
        PSUPDRVUSAGE    pUsage;
        RTSpinlockAcquire(pDevExt->Spinlock);

        while ((pUsage = pSession->pUsage) != NULL)
        {
            PSUPDRVOBJ  pObj = pUsage->pObj;
            pSession->pUsage = pUsage->pNext;

            AssertMsg(pUsage->cUsage >= 1 && pObj->cUsage >= pUsage->cUsage, ("glob %d; sess %d\n", pObj->cUsage, pUsage->cUsage));
            if (pUsage->cUsage < pObj->cUsage)
            {
                pObj->cUsage -= pUsage->cUsage;
                RTSpinlockRelease(pDevExt->Spinlock);
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
                RTSpinlockRelease(pDevExt->Spinlock);

                Log(("supdrvCleanupSession: destroying %p/%d (%p/%p) cpid=%RTproc pid=%RTproc dtor=%p\n",
                     pObj, pObj->enmType, pObj->pvUser1, pObj->pvUser2, pObj->CreatorProcess, RTProcSelf(), pObj->pfnDestructor));
                if (pObj->pfnDestructor)
                    pObj->pfnDestructor(pObj, pObj->pvUser1, pObj->pvUser2);
                RTMemFree(pObj);
            }

            /* free it and continue. */
            RTMemFree(pUsage);

            RTSpinlockAcquire(pDevExt->Spinlock);
        }

        RTSpinlockRelease(pDevExt->Spinlock);
        AssertMsg(!pSession->pUsage, ("Some buster reregistered an object during desturction!\n"));
    }
    Log2(("release objects - done\n"));

    /*
     * Do tracer cleanups related to this session.
     */
    Log2(("release tracer stuff - start\n"));
    supdrvTracerCleanupSession(pDevExt, pSession);
    Log2(("release tracer stuff - end\n"));

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
 * Common code for freeing a session when the reference count reaches zero.
 *
 * @param   pDevExt     Device extension.
 * @param   pSession    Session data.
 *                      This data will be freed by this routine.
 */
static void supdrvDestroySession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    VBOXDRV_SESSION_CLOSE(pSession);

    /*
     * Cleanup the session first.
     */
    supdrvCleanupSession(pDevExt, pSession);
    supdrvOSCleanupSession(pDevExt, pSession);

    /*
     * Free the rest of the session stuff.
     */
    RTSpinlockDestroy(pSession->Spinlock);
    pSession->Spinlock = NIL_RTSPINLOCK;
    pSession->pDevExt = NULL;
    RTMemFree(pSession);
    LogFlow(("supdrvDestroySession: returns\n"));
}


/**
 * Inserts the session into the global hash table.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_WRONG_ORDER if the session was already inserted (asserted).
 * @retval  VERR_INVALID_PARAMETER if the session handle is invalid or a ring-0
 *          session (asserted).
 * @retval  VERR_DUPLICATE if there is already a session for that pid.
 *
 * @param   pDevExt         The device extension.
 * @param   pSession        The session.
 * @param   ppOsSessionPtr  Pointer to the OS session pointer, if any is
 *                          available and used.  This will set to point to the
 *                          session while under the protection of the session
 *                          hash table spinlock.  It will also be kept in
 *                          PSUPDRVSESSION::ppOsSessionPtr for lookup and
 *                          cleanup use.
 * @param   pvUser          Argument for supdrvOSSessionHashTabInserted.
 */
int VBOXCALL supdrvSessionHashTabInsert(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPDRVSESSION *ppOsSessionPtr,
                                        void *pvUser)
{
    PSUPDRVSESSION  pCur;
    unsigned        iHash;

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertReturn(pSession->R0Process != NIL_RTR0PROCESS, VERR_INVALID_PARAMETER);

    /*
     * Calculate the hash table index and acquire the spinlock.
     */
    iHash = SUPDRV_SESSION_HASH(pSession->Process);

    RTSpinlockAcquire(pDevExt->hSessionHashTabSpinlock);

    /*
     * If there are a collisions, we need to carefully check if we got a
     * duplicate.  There can only be one open session per process.
     */
    pCur = pDevExt->apSessionHashTab[iHash];
    if (pCur)
    {
        while (pCur && pCur->Process != pSession->Process)
            pCur = pCur->pCommonNextHash;

        if (pCur)
        {
            RTSpinlockRelease(pDevExt->hSessionHashTabSpinlock);
            if (pCur == pSession)
            {
                Assert(pSession->fInHashTable);
                AssertFailed();
                return VERR_WRONG_ORDER;
            }
            Assert(!pSession->fInHashTable);
            if (pCur->R0Process == pSession->R0Process)
                return VERR_RESOURCE_IN_USE;
            return VERR_DUPLICATE;
        }
    }
    Assert(!pSession->fInHashTable);
    Assert(!pSession->ppOsSessionPtr);

    /*
     * Insert it, doing a callout to the OS specific code in case it has
     * anything it wishes to do while we're holding the spinlock.
     */
    pSession->pCommonNextHash = pDevExt->apSessionHashTab[iHash];
    pDevExt->apSessionHashTab[iHash] = pSession;
    pSession->fInHashTable    = true;
    ASMAtomicIncS32(&pDevExt->cSessions);

    pSession->ppOsSessionPtr = ppOsSessionPtr;
    if (ppOsSessionPtr)
        ASMAtomicWritePtr(ppOsSessionPtr, pSession);

    supdrvOSSessionHashTabInserted(pDevExt, pSession, pvUser);

    /*
     * Retain a reference for the pointer in the session table.
     */
    ASMAtomicIncU32(&pSession->cRefs);

    RTSpinlockRelease(pDevExt->hSessionHashTabSpinlock);
    return VINF_SUCCESS;
}


/**
 * Removes the session from the global hash table.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_FOUND if the session was already removed (asserted).
 * @retval  VERR_INVALID_PARAMETER if the session handle is invalid or a ring-0
 *          session (asserted).
 *
 * @param   pDevExt     The device extension.
 * @param   pSession    The session. The caller is expected to have a reference
 *                      to this so it won't croak on us when we release the hash
 *                      table reference.
 * @param   pvUser      OS specific context value for the
 *                      supdrvOSSessionHashTabInserted callback.
 */
int VBOXCALL supdrvSessionHashTabRemove(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, void *pvUser)
{
    PSUPDRVSESSION  pCur;
    unsigned        iHash;
    int32_t         cRefs;

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertReturn(pSession->R0Process != NIL_RTR0PROCESS, VERR_INVALID_PARAMETER);

    /*
     * Calculate the hash table index and acquire the spinlock.
     */
    iHash = SUPDRV_SESSION_HASH(pSession->Process);

    RTSpinlockAcquire(pDevExt->hSessionHashTabSpinlock);

    /*
     * Unlink it.
     */
    pCur = pDevExt->apSessionHashTab[iHash];
    if (pCur == pSession)
        pDevExt->apSessionHashTab[iHash] = pSession->pCommonNextHash;
    else
    {
        PSUPDRVSESSION pPrev = pCur;
        while (pCur && pCur != pSession)
        {
            pPrev = pCur;
            pCur  = pCur->pCommonNextHash;
        }
        if (pCur)
            pPrev->pCommonNextHash = pCur->pCommonNextHash;
        else
        {
            Assert(!pSession->fInHashTable);
            RTSpinlockRelease(pDevExt->hSessionHashTabSpinlock);
            return VERR_NOT_FOUND;
        }
    }

    pSession->pCommonNextHash = NULL;
    pSession->fInHashTable    = false;

    ASMAtomicDecS32(&pDevExt->cSessions);

    /*
     * Clear OS specific session pointer if available and do the OS callback.
     */
    if (pSession->ppOsSessionPtr)
    {
        ASMAtomicCmpXchgPtr(pSession->ppOsSessionPtr, NULL, pSession);
        pSession->ppOsSessionPtr = NULL;
    }

    supdrvOSSessionHashTabRemoved(pDevExt, pSession, pvUser);

    RTSpinlockRelease(pDevExt->hSessionHashTabSpinlock);

    /*
     * Drop the reference the hash table had to the session.  This shouldn't
     * be the last reference!
     */
    cRefs = ASMAtomicDecU32(&pSession->cRefs);
    Assert(cRefs > 0 && cRefs < _1M);
    if (cRefs == 0)
        supdrvDestroySession(pDevExt, pSession);

    return VINF_SUCCESS;
}


/**
 * Looks up the session for the current process in the global hash table or in
 * OS specific pointer.
 *
 * @returns Pointer to the session with a reference that the caller must
 *          release.  If no valid session was found, NULL is returned.
 *
 * @param   pDevExt         The device extension.
 * @param   Process         The process ID.
 * @param   R0Process       The ring-0 process handle.
 * @param   ppOsSessionPtr  The OS session pointer if available.  If not NULL,
 *                          this is used instead of the hash table.  For
 *                          additional safety it must then be equal to the
 *                          SUPDRVSESSION::ppOsSessionPtr member.
 *                          This can be NULL even if the OS has a session
 *                          pointer.
 */
PSUPDRVSESSION VBOXCALL supdrvSessionHashTabLookup(PSUPDRVDEVEXT pDevExt, RTPROCESS Process, RTR0PROCESS R0Process,
                                                   PSUPDRVSESSION *ppOsSessionPtr)
{
    PSUPDRVSESSION  pCur;
    unsigned        iHash;

    /*
     * Validate input.
     */
    AssertReturn(R0Process != NIL_RTR0PROCESS, NULL);

    /*
     * Calculate the hash table index and acquire the spinlock.
     */
    iHash = SUPDRV_SESSION_HASH(Process);

    RTSpinlockAcquire(pDevExt->hSessionHashTabSpinlock);

    /*
     * If an OS session pointer is provided, always use it.
     */
    if (ppOsSessionPtr)
    {
        pCur = *ppOsSessionPtr;
        if (   pCur
            && (   pCur->ppOsSessionPtr != ppOsSessionPtr
                || pCur->Process        != Process
                || pCur->R0Process      != R0Process) )
            pCur = NULL;
    }
    else
    {
        /*
         * Otherwise, do the hash table lookup.
         */
        pCur = pDevExt->apSessionHashTab[iHash];
        while (   pCur
               && (   pCur->Process   != Process
                   || pCur->R0Process != R0Process) )
            pCur = pCur->pCommonNextHash;
    }

    /*
     * Retain the session.
     */
    if (pCur)
    {
        uint32_t cRefs = ASMAtomicIncU32(&pCur->cRefs);
        NOREF(cRefs);
        Assert(cRefs > 1 && cRefs < _1M);
    }

    RTSpinlockRelease(pDevExt->hSessionHashTabSpinlock);

    return pCur;
}


/**
 * Retain a session to make sure it doesn't go away while it is in use.
 *
 * @returns New reference count on success, UINT32_MAX on failure.
 * @param   pSession    Session data.
 */
uint32_t VBOXCALL supdrvSessionRetain(PSUPDRVSESSION pSession)
{
    uint32_t cRefs;
    AssertPtrReturn(pSession, UINT32_MAX);
    AssertReturn(SUP_IS_SESSION_VALID(pSession), UINT32_MAX);

    cRefs = ASMAtomicIncU32(&pSession->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pSession));
    return cRefs;
}


/**
 * Releases a given session.
 *
 * @returns New reference count on success (0 if closed), UINT32_MAX on failure.
 * @param   pSession    Session data.
 */
uint32_t VBOXCALL supdrvSessionRelease(PSUPDRVSESSION pSession)
{
    uint32_t cRefs;
    AssertPtrReturn(pSession, UINT32_MAX);
    AssertReturn(SUP_IS_SESSION_VALID(pSession), UINT32_MAX);

    cRefs = ASMAtomicDecU32(&pSession->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pSession));
    if (cRefs == 0)
        supdrvDestroySession(pSession->pDevExt, pSession);
    return cRefs;
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
            case SUP_IOCTL_FAST_DO_HM_RUN:
                pDevExt->pfnVMMR0EntryFast(pSession->pVM, idCpu, SUP_VMMR0_DO_HM_RUN);
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
 * I/O Control inner worker (tracing reasons).
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_PARAMETER if the request is invalid.
 *
 * @param   uIOCtl      Function number.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 * @param   pReqHdr     The request header.
 */
static int supdrvIOCtlInnerUnrestricted(uintptr_t uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPREQHDR pReqHdr)
{
    /*
     * Validation macros
     */
#define REQ_CHECK_SIZES_EX(Name, cbInExpect, cbOutExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cbIn != (cbInExpect) || pReqHdr->cbOut != (cbOutExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n", \
                        (long)pReqHdr->cbIn, (long)(cbInExpect), (long)pReqHdr->cbOut, (long)(cbOutExpect))); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_SIZES(Name) REQ_CHECK_SIZES_EX(Name, Name ## _SIZE_IN, Name ## _SIZE_OUT)

#define REQ_CHECK_SIZE_IN(Name, cbInExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cbIn != (cbInExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cbIn=%ld expected %ld.\n", \
                        (long)pReqHdr->cbIn, (long)(cbInExpect))); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_SIZE_OUT(Name, cbOutExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cbOut != (cbOutExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cbOut=%ld expected %ld.\n", \
                        (long)pReqHdr->cbOut, (long)(cbOutExpect))); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_EXPR(Name, expr) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            OSDBGPRINT(( #Name ": %s\n", #expr)); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_EXPR_FMT(expr, fmt) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            OSDBGPRINT( fmt ); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
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
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_GET_SYMBOL, RTStrEnd(pReq->u.In.szSymbol, sizeof(pReq->u.In.szSymbol)));

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
            REQ_CHECK_EXPR(SUP_IOCTL_CALL_SERVICE, RTStrEnd(pReq->u.In.szName, sizeof(pReq->u.In.szName)));

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

            /* execute */
            pReq->Hdr.rc = SUPR0QueryVTCaps(pSession, &pReq->u.Out.Caps);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_OPEN):
        {
            /* validate */
            PSUPTRACEROPEN pReq = (PSUPTRACEROPEN)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_OPEN);

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_TracerOpen(pDevExt, pSession, pReq->u.In.uCookie, pReq->u.In.uArg);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_CLOSE):
        {
            /* validate */
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_CLOSE);

            /* execute */
            pReqHdr->rc = supdrvIOCtl_TracerClose(pDevExt, pSession);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_IOCTL):
        {
            /* validate */
            PSUPTRACERIOCTL pReq = (PSUPTRACERIOCTL)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_IOCTL);

            /* execute */
            pReqHdr->rc = supdrvIOCtl_TracerIOCtl(pDevExt, pSession, pReq->u.In.uCmd, pReq->u.In.uArg, &pReq->u.Out.iRetVal);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_UMOD_REG):
        {
            /* validate */
            PSUPTRACERUMODREG pReq = (PSUPTRACERUMODREG)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_UMOD_REG);
            if (!RTStrEnd(pReq->u.In.szName, sizeof(pReq->u.In.szName)))
                return VERR_INVALID_PARAMETER;

            /* execute */
            pReqHdr->rc = supdrvIOCtl_TracerUmodRegister(pDevExt, pSession,
                                                         pReq->u.In.R3PtrVtgHdr, pReq->u.In.uVtgHdrAddr,
                                                         pReq->u.In.R3PtrStrTab, pReq->u.In.cbStrTab,
                                                         pReq->u.In.szName, pReq->u.In.fFlags);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_UMOD_DEREG):
        {
            /* validate */
            PSUPTRACERUMODDEREG pReq = (PSUPTRACERUMODDEREG)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_UMOD_DEREG);

            /* execute */
            pReqHdr->rc = supdrvIOCtl_TracerUmodDeregister(pDevExt, pSession, pReq->u.In.pVtgHdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_UMOD_FIRE_PROBE):
        {
            /* validate */
            PSUPTRACERUMODFIREPROBE pReq = (PSUPTRACERUMODFIREPROBE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_UMOD_FIRE_PROBE);

            supdrvIOCtl_TracerUmodProbeFire(pDevExt, pSession, &pReq->u.In);
            pReqHdr->rc = VINF_SUCCESS;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_MSR_PROBER):
        {
            /* validate */
            PSUPMSRPROBER pReq = (PSUPMSRPROBER)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_MSR_PROBER);
            REQ_CHECK_EXPR(SUP_IOCTL_MSR_PROBER,
                           pReq->u.In.enmOp > SUPMSRPROBEROP_INVALID && pReq->u.In.enmOp < SUPMSRPROBEROP_END);

            pReqHdr->rc = supdrvIOCtl_MsrProber(pDevExt, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_RESUME_SUSPENDED_KBDS):
        {
            /* validate */
            REQ_CHECK_SIZES(SUP_IOCTL_RESUME_SUSPENDED_KBDS);

            pReqHdr->rc = supdrvIOCtl_ResumeSuspendedKbds();
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TSC_DELTA_MEASURE):
        {
            /* validate */
            PSUPTSCDELTAMEASURE pReq = (PSUPTSCDELTAMEASURE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TSC_DELTA_MEASURE);

            pReqHdr->rc = supdrvIOCtl_TscDeltaMeasure(pDevExt, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TSC_READ):
        {
            /* validate */
            PSUPTSCREAD pReq = (PSUPTSCREAD)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TSC_READ);

            pReqHdr->rc = supdrvIOCtl_TscRead(pDevExt, pReq);
            return 0;
        }

        default:
            Log(("Unknown IOCTL %#lx\n", (long)uIOCtl));
            break;
    }
    return VERR_GENERAL_FAILURE;
}


/**
 * I/O Control inner worker for the restricted operations.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_PARAMETER if the request is invalid.
 *
 * @param   uIOCtl      Function number.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 * @param   pReqHdr     The request header.
 */
static int supdrvIOCtlInnerRestricted(uintptr_t uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPREQHDR pReqHdr)
{
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
            pReq->u.Out.cFunctions        = 0;
            pReq->Hdr.rc = VINF_SUCCESS;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_VT_CAPS):
        {
            /* validate */
            PSUPVTCAPS pReq = (PSUPVTCAPS)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_VT_CAPS);

            /* execute */
            pReq->Hdr.rc = SUPR0QueryVTCaps(pSession, &pReq->u.Out.Caps);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        default:
            Log(("Unknown IOCTL %#lx\n", (long)uIOCtl));
            break;
    }
    return VERR_GENERAL_FAILURE;
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
int VBOXCALL supdrvIOCtl(uintptr_t uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPREQHDR pReqHdr, size_t cbReq)
{
    int rc;
    VBOXDRV_IOCTL_ENTRY(pSession, uIOCtl, pReqHdr);

    /*
     * Validate the request.
     */
    if (RT_UNLIKELY(cbReq < sizeof(*pReqHdr)))
    {
        OSDBGPRINT(("vboxdrv: Bad ioctl request size; cbReq=%#lx\n", (long)cbReq));
        VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VINF_SUCCESS);
        return VERR_INVALID_PARAMETER;
    }
    if (RT_UNLIKELY(   (pReqHdr->fFlags & SUPREQHDR_FLAGS_MAGIC_MASK) != SUPREQHDR_FLAGS_MAGIC
                    || pReqHdr->cbIn < sizeof(*pReqHdr)
                    || pReqHdr->cbIn > cbReq
                    || pReqHdr->cbOut < sizeof(*pReqHdr)
                    || pReqHdr->cbOut > cbReq))
    {
        OSDBGPRINT(("vboxdrv: Bad ioctl request header; cbIn=%#lx cbOut=%#lx fFlags=%#lx\n",
                    (long)pReqHdr->cbIn, (long)pReqHdr->cbOut, (long)pReqHdr->fFlags));
        VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VINF_SUCCESS);
        return VERR_INVALID_PARAMETER;
    }
    if (RT_UNLIKELY(!RT_VALID_PTR(pSession)))
    {
        OSDBGPRINT(("vboxdrv: Invalid pSession value %p (ioctl=%p)\n", pSession, (void *)uIOCtl));
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
     * Hand it to an inner function to avoid lots of unnecessary return tracepoints.
     */
    if (pSession->fUnrestricted)
        rc = supdrvIOCtlInnerUnrestricted(uIOCtl, pDevExt, pSession, pReqHdr);
    else
        rc = supdrvIOCtlInnerRestricted(uIOCtl, pDevExt, pSession, pReqHdr);

    VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, pReqHdr->rc, rc);
    return rc;
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

            pReq->Hdr.rc = supdrvCreateSession(pDevExt, false /* fUser */, true /*fUnrestricted*/, &pSession);
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

            supdrvSessionRelease(pSession);
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
    RTSpinlockAcquire(pDevExt->Spinlock);

    pUsage = pDevExt->pUsageFree;
    if (pUsage)
        pDevExt->pUsageFree = pUsage->pNext;
    else
    {
        RTSpinlockRelease(pDevExt->Spinlock);
        pUsage = (PSUPDRVUSAGE)RTMemAlloc(sizeof(*pUsage));
        if (!pUsage)
        {
            RTMemFree(pObj);
            return NULL;
        }
        RTSpinlockAcquire(pDevExt->Spinlock);
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

    RTSpinlockRelease(pDevExt->Spinlock);

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

    RTSpinlockAcquire(pDevExt->Spinlock);

    if (RT_UNLIKELY(pObj->u32Magic != SUPDRVOBJ_MAGIC))
    {
        RTSpinlockRelease(pDevExt->Spinlock);

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
        RTSpinlockRelease(pDevExt->Spinlock);
        pUsagePre = (PSUPDRVUSAGE)RTMemAlloc(sizeof(*pUsagePre));
        if (!pUsagePre)
            return VERR_NO_MEMORY;

        RTSpinlockAcquire(pDevExt->Spinlock);
        if (RT_UNLIKELY(pObj->u32Magic != SUPDRVOBJ_MAGIC))
        {
            RTSpinlockRelease(pDevExt->Spinlock);

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

    RTSpinlockRelease(pDevExt->Spinlock);

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
    RTSpinlockAcquire(pDevExt->Spinlock);

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

    RTSpinlockRelease(pDevExt->Spinlock);

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
    RTSpinlockAcquire(pSession->Spinlock);
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
                    RTSpinlockRelease(pSession->Spinlock);
                    return VINF_SUCCESS;
                }
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock);
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
    RTSpinlockAcquire(pSession->Spinlock);
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
    RTSpinlockRelease(pSession->Spinlock);

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
    RTSpinlockAcquire(pSession->Spinlock);
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
    RTSpinlockRelease(pSession->Spinlock);

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
            uint32_t fExtFeatures = ASMCpuId_EDX(0x80000001);
            if (fExtFeatures & (X86_CPUID_EXT_FEATURE_EDX_NX | X86_CPUID_EXT_FEATURE_EDX_LONG_MODE))
            {
                uint64_t efer = ASMRdMsr(MSR_K6_EFER);
                if ((fExtFeatures & X86_CPUID_EXT_FEATURE_EDX_NX)        && (efer & MSR_K6_EFER_NXE))
                    fNXEPlusLMA |= RT_BIT(0);
                if ((fExtFeatures & X86_CPUID_EXT_FEATURE_EDX_LONG_MODE) && (efer & MSR_K6_EFER_LMA))
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
 * Suspends hardware virtualization extensions using the native OS API.
 *
 * This is called prior to entering raw-mode context.
 *
 * @returns @c true if suspended, @c false if not.
 */
SUPR0DECL(bool) SUPR0SuspendVTxOnCpu(void)
{
#ifdef RT_OS_DARWIN
    return supdrvOSSuspendVTxOnCpu();
#else
    return false;
#endif
}


/**
 * Resumes hardware virtualization extensions using the native OS API.
 *
 * This is called after to entering raw-mode context.
 *
 * @param   fSuspended      The return value of SUPR0SuspendVTxOnCpu.
 */
SUPR0DECL(void) SUPR0ResumeVTxOnCpu(bool fSuspended)
{
#ifdef RT_OS_DARWIN
    supdrvOSResumeVTxOnCpu(fSuspended);
#else
    Assert(!fSuspended);
#endif
}


/**
 * Queries the AMD-V and VT-x capabilities of the calling CPU.
 *
 * @returns VBox status code.
 * @retval  VERR_VMX_NO_VMX
 * @retval  VERR_VMX_MSR_ALL_VMXON_DISABLED
 * @retval  VERR_VMX_MSR_VMXON_DISABLED
 * @retval  VERR_VMX_MSR_LOCKING_FAILED
 * @retval  VERR_SVM_NO_SVM
 * @retval  VERR_SVM_DISABLED
 * @retval  VERR_UNSUPPORTED_CPU if not identifiable as an AMD, Intel or VIA
 *          (centaur) CPU.
 *
 * @param   pSession        The session handle.
 * @param   pfCaps          Where to store the capabilities.
 */
SUPR0DECL(int) SUPR0QueryVTCaps(PSUPDRVSESSION pSession, uint32_t *pfCaps)
{
    int  rc = VERR_UNSUPPORTED_CPU;
    bool fIsSmxModeAmbiguous = false;
    RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;

    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfCaps, VERR_INVALID_POINTER);

    *pfCaps = 0;
    /* We may modify MSRs and re-read them, disable preemption so we make sure we don't migrate CPUs. */
    RTThreadPreemptDisable(&PreemptState);
    if (ASMHasCpuId())
    {
        uint32_t fFeaturesECX, fFeaturesEDX, uDummy;
        uint32_t uMaxId, uVendorEBX, uVendorECX, uVendorEDX;

        ASMCpuId(0, &uMaxId, &uVendorEBX, &uVendorECX, &uVendorEDX);
        ASMCpuId(1, &uDummy, &uDummy, &fFeaturesECX, &fFeaturesEDX);

        if (   ASMIsValidStdRange(uMaxId)
            && (   ASMIsIntelCpuEx(     uVendorEBX, uVendorECX, uVendorEDX)
                || ASMIsViaCentaurCpuEx(uVendorEBX, uVendorECX, uVendorEDX) )
           )
        {
            if (    (fFeaturesECX & X86_CPUID_FEATURE_ECX_VMX)
                 && (fFeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
                 && (fFeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
               )
            {
                /** @todo Unify code with hmR0InitIntelCpu(). */
                uint64_t   u64FeatMsr     = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);
                bool const fMaybeSmxMode  = RT_BOOL(ASMGetCR4() & X86_CR4_SMXE);
                bool       fMsrLocked     = RT_BOOL(u64FeatMsr & MSR_IA32_FEATURE_CONTROL_LOCK);
                bool       fSmxVmxAllowed = RT_BOOL(u64FeatMsr & MSR_IA32_FEATURE_CONTROL_SMX_VMXON);
                bool       fVmxAllowed    = RT_BOOL(u64FeatMsr & MSR_IA32_FEATURE_CONTROL_VMXON);

                /* Check if the LOCK bit is set but excludes the required VMXON bit. */
                if (fMsrLocked)
                {
                    if (fVmxAllowed && fSmxVmxAllowed)
                        rc = VINF_SUCCESS;
                    else if (!fVmxAllowed && !fSmxVmxAllowed)
                        rc = VERR_VMX_MSR_ALL_VMXON_DISABLED;
                    else if (!fMaybeSmxMode)
                    {
                        if (fVmxAllowed)
                            rc = VINF_SUCCESS;
                        else
                            rc = VERR_VMX_MSR_VMXON_DISABLED;
                    }
                    else
                    {
                        /*
                         * CR4.SMXE is set but this doesn't mean the CPU is necessarily in SMX mode. We shall assume
                         * that it is -not- and that it is a stupid BIOS/OS setting CR4.SMXE for no good reason.
                         * See @bugref{6873}.
                         */
                        Assert(fMaybeSmxMode == true);
                        fIsSmxModeAmbiguous = true;
                        rc = VINF_SUCCESS;
                    }
                }
                else
                {
                    /*
                     * MSR is not yet locked; we can change it ourselves here.
                     * Once the lock bit is set, this MSR can no longer be modified.
                     *
                     * Set both the VMXON and SMX_VMXON bits as we can't determine SMX mode
                     * accurately. See @bugref{6873}.
                     */
                    u64FeatMsr |=   MSR_IA32_FEATURE_CONTROL_LOCK
                                  | MSR_IA32_FEATURE_CONTROL_SMX_VMXON
                                  | MSR_IA32_FEATURE_CONTROL_VMXON;
                    ASMWrMsr(MSR_IA32_FEATURE_CONTROL, u64FeatMsr);

                    /* Verify. */
                    u64FeatMsr     = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);
                    fMsrLocked     = RT_BOOL(u64FeatMsr & MSR_IA32_FEATURE_CONTROL_LOCK);
                    fSmxVmxAllowed = fMsrLocked && RT_BOOL(u64FeatMsr & MSR_IA32_FEATURE_CONTROL_SMX_VMXON);
                    fVmxAllowed    = fMsrLocked && RT_BOOL(u64FeatMsr & MSR_IA32_FEATURE_CONTROL_VMXON);
                    if (fSmxVmxAllowed && fVmxAllowed)
                        rc = VINF_SUCCESS;
                    else
                        rc = VERR_VMX_MSR_LOCKING_FAILED;
                }

                if (rc == VINF_SUCCESS)
                {
                    VMXCAPABILITY vtCaps;

                    *pfCaps |= SUPVTCAPS_VT_X;

                    vtCaps.u = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS);
                    if (vtCaps.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                    {
                        vtCaps.u = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS2);
                        if (vtCaps.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                            *pfCaps |= SUPVTCAPS_NESTED_PAGING;
                    }
                }
            }
            else
                rc = VERR_VMX_NO_VMX;
        }
        else if (   ASMIsAmdCpuEx(uVendorEBX, uVendorECX, uVendorEDX)
                 && ASMIsValidStdRange(uMaxId))
        {
            uint32_t fExtFeaturesEcx, uExtMaxId;
            ASMCpuId(0x80000000, &uExtMaxId, &uDummy, &uDummy, &uDummy);
            ASMCpuId(0x80000001, &uDummy, &uDummy, &fExtFeaturesEcx, &uDummy);
            if (   ASMIsValidExtRange(uExtMaxId)
                && uExtMaxId >= 0x8000000a
                && (fExtFeaturesEcx & X86_CPUID_AMD_FEATURE_ECX_SVM)
                && (fFeaturesEDX    & X86_CPUID_FEATURE_EDX_MSR)
                && (fFeaturesEDX    & X86_CPUID_FEATURE_EDX_FXSR)
               )
            {
                /* Check if SVM is disabled */
                uint64_t u64FeatMsr = ASMRdMsr(MSR_K8_VM_CR);
                if (!(u64FeatMsr & MSR_K8_VM_CR_SVM_DISABLE))
                {
                    uint32_t fSvmFeatures;
                    *pfCaps |= SUPVTCAPS_AMD_V;

                    /* Query AMD-V features. */
                    ASMCpuId(0x8000000a, &uDummy, &uDummy, &uDummy, &fSvmFeatures);
                    if (fSvmFeatures & AMD_CPUID_SVM_FEATURE_EDX_NESTED_PAGING)
                        *pfCaps |= SUPVTCAPS_NESTED_PAGING;

                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_SVM_DISABLED;
            }
            else
                rc = VERR_SVM_NO_SVM;
        }
    }

    RTThreadPreemptRestore(&PreemptState);
    if (fIsSmxModeAmbiguous)
        SUPR0Printf(("WARNING! CR4 hints SMX mode but your CPU is too secretive. Proceeding anyway... We wish you good luck!\n"));
    return rc;
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
                    for (i = 0; i < pGipR0->cCpus; i++)
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

    /*
     * Find free entry and record the allocation.
     */
    RTSpinlockAcquire(pSession->Spinlock);
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
                    RTSpinlockRelease(pSession->Spinlock);
                    return VINF_SUCCESS;
                }
            }
            AssertFailed();             /* !!this can't be happening!!! */
        }
    }
    RTSpinlockRelease(pSession->Spinlock);

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
    RTSpinlockAcquire(pSession->Spinlock);
    pBundle->pNext = pSession->Bundle.pNext;
    pSession->Bundle.pNext = pBundle;
    RTSpinlockRelease(pSession->Spinlock);

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
    RTSpinlockAcquire(pSession->Spinlock);
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
                    RTSpinlockRelease(pSession->Spinlock);

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
    RTSpinlockRelease(pSession->Spinlock);
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
            if (RT_LIKELY(pImage->cUsage < UINT32_MAX / 2U))
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
            supdrvLdrUnlock(pDevExt);
            Log(("supdrvIOCtl_LdrOpen: To many existing references to '%s'!\n", pReq->u.In.szName));
            return VERR_INTERNAL_ERROR_3; /** @todo add VERR_TOO_MANY_REFERENCES */
        }
    }
    /* (not found - add it!) */

    /*
     * Allocate memory.
     */
    Assert(cchName < sizeof(pImage->szName));
    pv = RTMemAlloc(sizeof(SUPDRVLDRIMAGE));
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
    supdrvOSLdrNotifyOpened(pDevExt, pImage);

    supdrvLdrUnlock(pDevExt);
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
        {
            memcpy(pImage->pvImage, &pReq->u.In.abImage[0], pImage->cbImageBits);
            Log(("vboxdrv: Loaded '%s' at %p\n", pImage->szName, pImage->pvImage));
        }
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
    SUPR0Printf("vboxdrv: %p %s\n", pImage->pvImage, pImage->szName);

    if (RT_FAILURE(rc))
    {
        /* Inform the tracing component in case ModuleInit registered TPs. */
        supdrvTracerModuleUnloading(pDevExt, pImage);

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
        RTSpinlockAcquire(pDevExt->Spinlock);
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
        RTSpinlockRelease(pDevExt->Spinlock);
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
        RTSpinlockAcquire(pDevExt->Spinlock);
        for (pObj = pDevExt->pObjs; pObj; pObj = pObj->pNext)
            if (RT_UNLIKELY((uintptr_t)pObj->pfnDestructor - (uintptr_t)pImage->pvImage < pImage->cbImageBits))
            {
                pObj->pfnDestructor = NULL;
                cObjs++;
            }
        RTSpinlockRelease(pDevExt->Spinlock);
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

    /* Inform the tracing component. */
    supdrvTracerModuleUnloading(pDevExt, pImage);

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
#if !defined(RT_OS_WINDOWS) || defined(RT_ARCH_AMD64) || defined(DEBUG)
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
#else  /* RT_OS_WINDOWS && !RT_ARCH_AMD64 && !DEBUG */
    return VERR_NOT_IMPLEMENTED;
#endif /* RT_OS_WINDOWS && !RT_ARCH_AMD64 && !DEBUG */
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
 * Implements the MSR prober operations.
 *
 * @returns VBox status code.
 * @param   pDevExt     The device extension.
 * @param   pReq        The request.
 */
static int supdrvIOCtl_MsrProber(PSUPDRVDEVEXT pDevExt, PSUPMSRPROBER pReq)
{
#ifdef SUPDRV_WITH_MSR_PROBER
    RTCPUID const idCpu = pReq->u.In.idCpu == UINT32_MAX ? NIL_RTCPUID : pReq->u.In.idCpu;
    int rc;

    switch (pReq->u.In.enmOp)
    {
        case SUPMSRPROBEROP_READ:
        {
            uint64_t uValue;
            rc = supdrvOSMsrProberRead(pReq->u.In.uMsr, idCpu, &uValue);
            if (RT_SUCCESS(rc))
            {
                pReq->u.Out.uResults.Read.uValue = uValue;
                pReq->u.Out.uResults.Read.fGp    = false;
            }
            else if (rc == VERR_ACCESS_DENIED)
            {
                pReq->u.Out.uResults.Read.uValue = 0;
                pReq->u.Out.uResults.Read.fGp    = true;
                rc  = VINF_SUCCESS;
            }
            break;
        }

        case SUPMSRPROBEROP_WRITE:
            rc = supdrvOSMsrProberWrite(pReq->u.In.uMsr, idCpu, pReq->u.In.uArgs.Write.uToWrite);
            if (RT_SUCCESS(rc))
                pReq->u.Out.uResults.Write.fGp   = false;
            else if (rc == VERR_ACCESS_DENIED)
            {
                pReq->u.Out.uResults.Write.fGp   = true;
                rc  = VINF_SUCCESS;
            }
            break;

        case SUPMSRPROBEROP_MODIFY:
        case SUPMSRPROBEROP_MODIFY_FASTER:
            rc = supdrvOSMsrProberModify(idCpu, pReq);
            break;

        default:
            return VERR_INVALID_FUNCTION;
    }
    return rc;
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}


#ifdef SUPDRV_USE_TSC_DELTA_THREAD
/**
 * Switches the TSC-delta measurement thread into the butchered state.
 *
 * @returns VBox status code.
 * @param pDevExt           Pointer to the device instance data.
 * @param fSpinlockHeld     Whether the TSC-delta spinlock is held or not.
 * @param pszFailed         An error message to log.
 * @param rcFailed          The error code to exit the thread with.
 */
static int supdrvTscDeltaThreadButchered(PSUPDRVDEVEXT pDevExt, bool fSpinlockHeld, const char *pszFailed, int rcFailed)
{
    if (!fSpinlockHeld)
        RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);

    pDevExt->enmTscDeltaState = kSupDrvTscDeltaState_Butchered;
    RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
    OSDBGPRINT(("supdrvTscDeltaThreadButchered: %s. rc=%Rrc\n", rcFailed));
    return rcFailed;
}


/**
 * The TSC-delta measurement thread.
 *
 * @returns VBox status code.
 * @param hThread   The thread handle.
 * @param pvUser    Opaque pointer to the device instance data.
 */
static DECLCALLBACK(int) supdrvTscDeltaThread(RTTHREAD hThread, void *pvUser)
{
    PSUPDRVDEVEXT     pDevExt = (PSUPDRVDEVEXT)pvUser;
    static uint32_t   cTimesMeasured = 0;
    uint32_t          cConsecutiveTimeouts = 0;
    int               rc = VERR_INTERNAL_ERROR_2;
    for (;;)
    {
        /*
         * Switch on the current state.
         */
        SUPDRVTSCDELTASTATE enmState;
        RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
        enmState = pDevExt->enmTscDeltaState;
        switch (enmState)
        {
            case kSupDrvTscDeltaState_Creating:
            {
                pDevExt->enmTscDeltaState = kSupDrvTscDeltaState_Listening;
                rc = RTSemEventSignal(pDevExt->hTscDeltaEvent);
                if (RT_FAILURE(rc))
                    return supdrvTscDeltaThreadButchered(pDevExt, true /* fSpinlockHeld */, "RTSemEventSignal", rc);
                /* fall thru */
            }

            case kSupDrvTscDeltaState_Listening:
            {
                RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);

                /* Simple adaptive timeout. */
                if (cConsecutiveTimeouts++ == 10)
                {
                    if (pDevExt->cMsTscDeltaTimeout == 1)           /* 10 ms */
                        pDevExt->cMsTscDeltaTimeout = 10;
                    else if (pDevExt->cMsTscDeltaTimeout == 10)     /* +100 ms */
                        pDevExt->cMsTscDeltaTimeout = 100;
                    else if (pDevExt->cMsTscDeltaTimeout == 100)    /* +1000 ms */
                        pDevExt->cMsTscDeltaTimeout = 500;
                    cConsecutiveTimeouts = 0;
                }
                rc = RTThreadUserWait(pDevExt->hTscDeltaThread, pDevExt->cMsTscDeltaTimeout);
                if (   RT_FAILURE(rc)
                    && rc != VERR_TIMEOUT)
                    return supdrvTscDeltaThreadButchered(pDevExt, false /* fSpinlockHeld */, "RTThreadUserWait", rc);
                break;
            }

            case kSupDrvTscDeltaState_WaitAndMeasure:
            {
                pDevExt->enmTscDeltaState = kSupDrvTscDeltaState_Measuring;
                rc = RTSemEventSignal(pDevExt->hTscDeltaEvent);
                if (RT_FAILURE(rc))
                    return supdrvTscDeltaThreadButchered(pDevExt, true /* fSpinlockHeld */, "RTSemEventSignal", rc);
                RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
                pDevExt->cMsTscDeltaTimeout = 1;
                RTThreadSleep(10);
                /* fall thru */
            }

            case kSupDrvTscDeltaState_Measuring:
            {
                cConsecutiveTimeouts = 0;
                if (!cTimesMeasured++)
                    rc = supdrvMeasureTscDeltas(pDevExt, NULL /* pidxMaster */);
                else
                {
                    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
                    unsigned iCpu;

                    if (cTimesMeasured == UINT32_MAX)
                        cTimesMeasured = 1;

                    /* Measure TSC-deltas only for the CPUs that are in the set. */
                    rc = VINF_SUCCESS;
                    for (iCpu = 0; iCpu < pGip->cCpus; iCpu++)
                    {
                        PSUPGIPCPU pGipCpuWorker = &pGip->aCPUs[iCpu];
                        if (   pGipCpuWorker->i64TSCDelta == INT64_MAX
                            && RTCpuSetIsMember(&pDevExt->TscDeltaCpuSet, pGipCpuWorker->idCpu))
                        {
                            rc |= supdrvMeasureTscDeltaOne(pDevExt, iCpu);
                            RTCpuSetDel(&pDevExt->TscDeltaCpuSet, pGipCpuWorker->idCpu);
                        }
                    }
                }
                RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
                if (pDevExt->enmTscDeltaState == kSupDrvTscDeltaState_Measuring)
                    pDevExt->enmTscDeltaState = kSupDrvTscDeltaState_Listening;
                RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
                pDevExt->rcTscDelta = rc;
                break;
            }

            case kSupDrvTscDeltaState_Terminating:
                RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
                return VINF_SUCCESS;

            case kSupDrvTscDeltaState_Butchered:
            default:
                return supdrvTscDeltaThreadButchered(pDevExt, true /* fSpinlockHeld */, "Invalid state", VERR_INVALID_STATE);
        }
    }

    return rc;
}


/**
 * Waits for the TSC-delta measurement thread to respond to a state change.
 *
 * @returns VINF_SUCCESS on success, VERR_TIMEOUT if it doesn't respond in time,
 *        other error code on internal error.
 *
 * @param   pThis           Pointer to the grant service instance data.
 * @param   enmCurState     The current state.
 * @param   enmNewState     The new state we're waiting for it to enter.
 */
static int supdrvTscDeltaThreadWait(PSUPDRVDEVEXT pDevExt, SUPDRVTSCDELTASTATE enmCurState, SUPDRVTSCDELTASTATE enmNewState)
{
    /*
     * Wait a short while for the expected state transition.
     */
    int rc;
    RTSemEventWait(pDevExt->hTscDeltaEvent, RT_MS_1SEC);
    RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
    if (pDevExt->enmTscDeltaState == enmNewState)
    {
        RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
        rc = VINF_SUCCESS;
    }
    else if (pDevExt->enmTscDeltaState == enmCurState)
    {
        /*
         * Wait longer if the state has not yet transitioned to the one we want.
         */
        RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
        rc = RTSemEventWait(pDevExt->hTscDeltaEvent, 50 * RT_MS_1SEC);
        if (   RT_SUCCESS(rc)
            || rc == VERR_TIMEOUT)
        {
            /*
             * Check the state whether we've succeeded.
             */
            SUPDRVTSCDELTASTATE enmState;
            RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
            enmState = pDevExt->enmTscDeltaState;
            RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
            if (enmState == enmNewState)
                rc = VINF_SUCCESS;
            else if (enmState == enmCurState)
            {
                rc = VERR_TIMEOUT;
                OSDBGPRINT(("supdrvTscDeltaThreadWait: timed out state transition. enmState=%d enmNewState=%d\n", enmState,
                            enmNewState));
            }
            else
            {
                rc = VERR_INTERNAL_ERROR;
                OSDBGPRINT(("supdrvTscDeltaThreadWait: invalid state transition from %d to %d, expected %d\n", enmCurState,
                            enmState, enmNewState));
            }
        }
        else
            OSDBGPRINT(("supdrvTscDeltaThreadWait: RTSemEventWait failed. rc=%Rrc\n", rc));
    }
    else
    {
        RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
        OSDBGPRINT(("supdrvTscDeltaThreadWait: invalid state transition from %d to %d\n", enmCurState, enmNewState));
        rc = VERR_INTERNAL_ERROR;
    }

    return rc;
}


/**
 * Terminates the TSC-delta measurement thread.
 *
 * @param   pDevExt   Pointer to the device instance data.
 */
static void supdrvTscDeltaThreadTerminate(PSUPDRVDEVEXT pDevExt)
{
    int rc;
    RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
    pDevExt->enmTscDeltaState = kSupDrvTscDeltaState_Terminating;
    RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
    RTThreadUserSignal(pDevExt->hTscDeltaThread);
    rc = RTThreadWait(pDevExt->hTscDeltaThread, 50 * RT_MS_1SEC, NULL /* prc */);
    if (RT_FAILURE(rc))
    {
        /* Signal a few more times before giving up. */
        int cTries = 5;
        while (--cTries > 0)
        {
            RTThreadUserSignal(pDevExt->hTscDeltaThread);
            rc = RTThreadWait(pDevExt->hTscDeltaThread, 2 * RT_MS_1SEC, NULL /* prc */);
            if (rc != VERR_TIMEOUT)
                break;
        }
    }
}


/**
 * Initializes and spawns the TSC-delta measurement thread.
 *
 * A thread is required for servicing re-measurement requests from events like
 * CPUs coming online, suspend/resume etc. as it cannot be done synchronously
 * under all contexts on all OSs.
 *
 * @returns VBox status code.
 * @param   pDevExt           Pointer to the device instance data.
 *
 * @remarks Must only be called -after- initializing GIP and setting up MP
 *          notifications!
 */
static int supdrvTscDeltaInit(PSUPDRVDEVEXT pDevExt)
{
    int rc = RTSpinlockCreate(&pDevExt->hTscDeltaSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_UNSAFE, "VBoxTscSpnLck");
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventCreate(&pDevExt->hTscDeltaEvent);
        if (RT_SUCCESS(rc))
        {
            pDevExt->enmTscDeltaState   = kSupDrvTscDeltaState_Creating;
            pDevExt->cMsTscDeltaTimeout = 1;
            RTCpuSetEmpty(&pDevExt->TscDeltaCpuSet);
            rc = RTThreadCreate(&pDevExt->hTscDeltaThread, supdrvTscDeltaThread, pDevExt, 0 /* cbStack */,
                                RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "VBoxTscThread");
            if (RT_SUCCESS(rc))
            {
                rc = supdrvTscDeltaThreadWait(pDevExt, kSupDrvTscDeltaState_Creating, kSupDrvTscDeltaState_Listening);
                if (RT_SUCCESS(rc))
                {
                    pDevExt->rcTscDelta = VERR_NOT_AVAILABLE;
                    return rc;
                }

                OSDBGPRINT(("supdrvTscDeltaInit: supdrvTscDeltaThreadWait failed. rc=%Rrc\n", rc));
                supdrvTscDeltaThreadTerminate(pDevExt);
            }
            else
                OSDBGPRINT(("supdrvTscDeltaInit: RTThreadCreate failed. rc=%Rrc\n", rc));
            RTSemEventDestroy(pDevExt->hTscDeltaEvent);
            pDevExt->hTscDeltaEvent = NIL_RTSEMEVENT;
        }
        else
            OSDBGPRINT(("supdrvTscDeltaInit: RTSemEventCreate failed. rc=%Rrc\n", rc));
        RTSpinlockDestroy(pDevExt->hTscDeltaSpinlock);
        pDevExt->hTscDeltaSpinlock = NIL_RTSPINLOCK;
    }
    else
        OSDBGPRINT(("supdrvTscDeltaInit: RTSpinlockCreate failed. rc=%Rrc\n", rc));

    return rc;
}


/**
 * Terminates the TSC-delta measurement thread and cleanup.
 *
 * @param   pDevExt         Pointer to the device instance data.
 */
static void supdrvTscDeltaTerm(PSUPDRVDEVEXT pDevExt)
{
    if (   pDevExt->hTscDeltaSpinlock != NIL_RTSPINLOCK
        && pDevExt->hTscDeltaEvent != NIL_RTSEMEVENT)
    {
        supdrvTscDeltaThreadTerminate(pDevExt);
    }

    if (pDevExt->hTscDeltaSpinlock != NIL_RTSPINLOCK)
    {
        RTSpinlockDestroy(pDevExt->hTscDeltaSpinlock);
        pDevExt->hTscDeltaSpinlock = NIL_RTSPINLOCK;
    }

    if (pDevExt->hTscDeltaEvent != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(pDevExt->hTscDeltaEvent);
        pDevExt->hTscDeltaEvent = NIL_RTSEMEVENT;
    }

    pDevExt->rcTscDelta = VERR_NOT_AVAILABLE;
}
#endif /* SUPDRV_USE_TSC_DELTA_THREAD */


#if 0
/**
 * Measures the nominal TSC frequency.
 *
 * Uses a busy-wait method for the async. case as it is intended to help push
 * the CPU frequency up, while for the invariant cases using a sleeping method.
 *
 * @returns VBox status code.
 * @param   pGip        Pointer to the GIP.
 *
 * @remarks Must be called only after measuring the TSC deltas.
 */
static int supdrvGipMeasureNominalTscFreq(PSUPGLOBALINFOPAGE pGip)
{
    int cTriesLeft = 4;

    /* Assert order. */
    AssertReturn(pGip, VERR_INVALID_PARAMETER);
    AssertReturn(pGip->u32Magic == SUPGLOBALINFOPAGE_MAGIC, VERR_WRONG_ORDER);

    while (cTriesLeft-- > 0)
    {
        RTCCUINTREG uFlags;
        uint64_t    u64NanoTs;
        uint64_t    u64NanoTsAfter;
        uint64_t    u64TscBefore;
        uint64_t    u64TscAfter;
        uint8_t     idApicBefore;
        uint8_t     idApicAfter;

        /*
         * Synchronize with the host OS clock tick before reading the TSC.
         * Especially important on Windows where the granularity is terrible.
         */
        u64NanoTs = RTTimeSystemNanoTS();
        while (RTTimeSystemNanoTS() == u64NanoTs)
            ASMNopPause();

        uFlags       = ASMIntDisableFlags();
        idApicBefore = ASMGetApicId();
        u64TscBefore = ASMReadTSC();
        u64NanoTs    = RTTimeSystemNanoTS();
        ASMSetFlags(uFlags);

        if (supdrvIsInvariantTsc())
        {
            /*
             * Sleep wait since the TSC frequency is constant, eases host load.
             * Shorter interval produces more variance in the frequency (esp. Windows).
             */
            RTThreadSleep(200);   /* Sleeping shorter produces a tad more variance in the frequency than I'd like. */
            u64NanoTsAfter = RTTimeSystemNanoTS();
            while (RTTimeSystemNanoTS() == u64NanoTsAfter)
                ASMNopPause();
            u64NanoTsAfter = RTTimeSystemNanoTS();
        }
        else
        {
            /* Busy wait, ramps up the CPU frequency on async systems. */
            for (;;)
            {
                u64NanoTsAfter = RTTimeSystemNanoTS();
                if (u64NanoTsAfter < RT_NS_100MS + u64NanoTs)
                    ASMNopPause();
                else
                    break;
            }
        }

        uFlags      = ASMIntDisableFlags();
        u64TscAfter = ASMReadTSC();
        idApicAfter = ASMGetApicId();
        ASMSetFlags(uFlags);

        /** @todo replace with enum check. */
        if (supdrvIsInvariantTsc())
        {
            PSUPGIPCPU pGipCpuBefore;
            PSUPGIPCPU pGipCpuAfter;

            uint16_t iCpuBefore = pGip->aiCpuFromApicId[idApicBefore];
            uint16_t iCpuAfter  = pGip->aiCpuFromApicId[idApicAfter];
            AssertMsgReturn(iCpuBefore < pGip->cCpus, ("iCpuBefore=%u cCpus=%u\n", iCpuBefore, pGip->cCpus), VERR_WRONG_ORDER);
            AssertMsgReturn(iCpuAfter  < pGip->cCpus, ("iCpuAfter=%u cCpus=%u\n", iCpuAfter, pGip->cCpus), VERR_WRONG_ORDER);
            pGipCpuBefore = &pGip->aCPUs[iCpuBefore];
            pGipCpuAfter  = &pGip->aCPUs[iCpuAfter];

            if (   pGipCpuBefore->i64TSCDelta != INT64_MAX
                && pGipCpuAfter->i64TSCDelta  != INT64_MAX)
            {
                u64TscBefore -= pGipCpuBefore->i64TSCDelta;
                u64TscAfter  -= pGipCpuAfter->i64TSCDelta;

                SUPR0Printf("vboxdrv: TSC frequency is %lu Hz - invariant, kernel timer granularity is %lu Ns\n",
                            ((u64TscAfter - u64TscBefore) * RT_NS_1SEC_64) / (u64NanoTsAfter - u64NanoTs),
                            RTTimerGetSystemGranularity());
                return VINF_SUCCESS;
            }
            else
            {
                SUPR0Printf("vboxdrv: supdrvGipMeasureNominalTscFreq: iCpuBefore=%u iCpuAfter=%u cTriesLeft=%u\n", iCpuBefore,
                            iCpuAfter, cTriesLeft);
            }
        }
        else
        {
            SUPR0Printf("vboxdrv: TSC frequency is %lu Hz - maybe variant, kernel timer granularity is %lu Ns\n",
                        ((u64TscAfter - u64TscBefore) * RT_NS_1SEC_64) / (u64NanoTsAfter - u64NanoTs),
                        RTTimerGetSystemGranularity());
            return VINF_SUCCESS;
        }
    }

    return VERR_SUPDRV_TSC_FREQ_MEASUREMENT_FAILED;
}
#endif


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
    uint32_t            u32MinInterval;
    uint32_t            uMod;
    unsigned            cCpus;
    int                 rc;

    LogFlow(("supdrvGipCreate:\n"));

    /* Assert order. */
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
    /** @todo figure out why using a 100Ms interval upsets timekeeping in VMs.
     *        See @bugref{6710}. */
    u32MinInterval      = RT_NS_10MS;
    u32SystemResolution = RTTimerGetSystemGranularity();
    u32Interval         = u32MinInterval;
    uMod                = u32MinInterval % u32SystemResolution;
    if (uMod)
        u32Interval += u32SystemResolution - uMod;

    supdrvGipInit(pDevExt, pGip, HCPhysGip, RTTimeSystemNanoTS(), RT_NS_1SEC / u32Interval /*=Hz*/, u32Interval, cCpus);

#ifdef SUPDRV_USE_TSC_DELTA_THREAD
    /* Initialize TSC-delta measurement thread before executing any Mp event callbacks. */
    rc = supdrvTscDeltaInit(pDevExt);
#endif
    if (RT_SUCCESS(rc))
    {
        rc = RTMpNotificationRegister(supdrvGipMpEvent, pDevExt);
        if (RT_SUCCESS(rc))
        {
            rc = RTMpOnAll(supdrvGipInitOnCpu, pDevExt, pGip);
            if (RT_SUCCESS(rc))
            {
#ifndef SUPDRV_USE_TSC_DELTA_THREAD
                /*
                 * Measure the TSC deltas now that we have MP notifications.
                 */
                int      cTries = 5;
                uint16_t iCpu;
                do
                {
                    rc = supdrvMeasureTscDeltas(pDevExt, NULL /* pidxMaster */);
                    if (rc != VERR_TRY_AGAIN)
                        break;
                } while (--cTries > 0);
                for (iCpu = 0; iCpu < pGip->cCpus; iCpu++)
                    Log(("supdrvTscDeltaInit: cpu[%u] delta %lld\n", iCpu, pGip->aCPUs[iCpu].i64TSCDelta));
#endif

#if 0
                /** @todo refactor later and use the nominal TSC rate for invariant case as
                 *        the real and constant TSC rate. */
                supdrvGipMeasureNominalTscFreq(pGip);
#endif

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
                    rc = RTTimerCreateEx(&pDevExt->pGipTimer, u32Interval, 0 /* fFlags */, supdrvGipSyncTimer, pDevExt);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * We're good.
                     */
                    Log(("supdrvGipCreate: %u ns interval.\n", u32Interval));
                    g_pSUPGlobalInfoPage = pGip;
                    return VINF_SUCCESS;
                }
                else
                {
                    OSDBGPRINT(("supdrvGipCreate: failed create GIP timer at %u ns interval. rc=%Rrc\n", u32Interval, rc));
                    Assert(!pDevExt->pGipTimer);
                }
            }
            else
                OSDBGPRINT(("supdrvGipCreate: RTMpOnAll failed. rc=%Rrc\n", rc));
        }
        else
            OSDBGPRINT(("supdrvGipCreate: failed to register MP event notfication. rc=%Rrc\n", rc));
    }
    else
        OSDBGPRINT(("supdrvGipCreate: supdrvTscDeltaInit failed. rc=%Rrc\n", rc));

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
     * Stop receiving MP notifications before tearing anything else down.
     */
    RTMpNotificationDeregister(supdrvGipMpEvent, pDevExt);

#ifdef SUPDRV_USE_TSC_DELTA_THREAD
    /*
     * Terminate the TSC-delta measurement thread and resources.
     */
    supdrvTscDeltaTerm(pDevExt);
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
    uint64_t        u64TSC    = ASMReadTSC();
    uint64_t        NanoTS    = RTTimeSystemNanoTS();
    PSUPDRVDEVEXT   pDevExt   = (PSUPDRVDEVEXT)pvUser;

    if (supdrvIsInvariantTsc())
    {
        PSUPGIPCPU         pGipCpu;
        PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
        unsigned           iCpu = pGip->aiCpuFromApicId[ASMGetApicId()];
        AssertReturnVoid(iCpu < pGip->cCpus);
        pGipCpu = &pGip->aCPUs[iCpu];
        AssertReturnVoid(pGipCpu->idCpu == RTMpCpuId());

        /*
         * The calculations in supdrvGipUpdate() is very timing sensitive and doesn't handle
         * missed timer ticks. So for now it is better to use a delta of 0 and have the TSC rate
         * affected a bit until we get proper TSC deltas than implementing options like
         * rescheduling the tick to be delivered on the right CPU or missing the tick entirely.
         *
         * The likely hood of this happening is really low. On Windows, Linux timers
         * fire on the CPU they were registered/started on. Darwin, Solaris need verification.
         */
        if (pGipCpu->i64TSCDelta != INT64_MAX)
            u64TSC -= pGipCpu->i64TSCDelta;
    }

    supdrvGipUpdate(pDevExt, NanoTS, u64TSC, NIL_RTCPUID, iTick);

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
        supdrvGipUpdate(pDevExt, NanoTS, u64TSC, idCpu, iTick);
    else
        supdrvGipUpdatePerCpu(pDevExt, NanoTS, u64TSC, idCpu, ASMGetApicId(), iTick);

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
 * @param   pDevExt             The device extension.
 * @param   idCpu               The CPU ID.
 */
static void supdrvGipMpEventOnline(PSUPDRVDEVEXT pDevExt, RTCPUID idCpu)
{
    int         iCpuSet = 0;
    uint16_t    idApic = UINT16_MAX;
    uint32_t    i = 0;
    uint64_t    u64NanoTS = 0;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;

    AssertPtrReturnVoid(pGip);
    AssertRelease(idCpu == RTMpCpuId());
    Assert(pGip->cPossibleCpus == RTMpGetCount());

    /*
     * Do this behind a spinlock with interrupts disabled as this can fire
     * on all CPUs simultaneously, see @bugref{6110}.
     */
    RTSpinlockAcquire(pDevExt->hGipSpinlock);

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

    /* Update the Mp online/offline counter. */
    ASMAtomicIncU32(&g_cMpOnOffEvents);

#ifdef SUPDRV_USE_TSC_DELTA_THREAD
    /*
     * Add this CPU to the set of CPUs that require their TSC delta to be measured.
     *
     * We cannot poke the TSC-delta measurement thread from this context (on all OSs), so we only
     * update the state and it'll get serviced when the thread's listening interval times out.
     */
    if (supdrvIsInvariantTsc())
    {
        RTCpuSetAdd(&pDevExt->TscDeltaCpuSet, idCpu);
        RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
        if (   pDevExt->enmTscDeltaState == kSupDrvTscDeltaState_Listening
            || pDevExt->enmTscDeltaState == kSupDrvTscDeltaState_Measuring)
        {
            pDevExt->enmTscDeltaState = kSupDrvTscDeltaState_WaitAndMeasure;
        }
        RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
    }
#endif

    /* commit it */
    ASMAtomicWriteSize(&pGip->aCPUs[i].enmState, SUPGIPCPUSTATE_ONLINE);

    RTSpinlockRelease(pDevExt->hGipSpinlock);
}


/**
 * The CPU should be accounted as offline, update the GIP accordingly.
 *
 * This is used by supdrvGipMpEvent.
 *
 * @param   pDevExt             The device extension.
 * @param   idCpu               The CPU ID.
 */
static void supdrvGipMpEventOffline(PSUPDRVDEVEXT pDevExt, RTCPUID idCpu)
{
    int         iCpuSet;
    unsigned    i;

    PSUPGLOBALINFOPAGE pGip   = pDevExt->pGip;

    AssertPtrReturnVoid(pGip);
    RTSpinlockAcquire(pDevExt->hGipSpinlock);

    iCpuSet = RTMpCpuIdToSetIndex(idCpu);
    AssertReturnVoid(iCpuSet >= 0);

    i = pGip->aiCpuFromCpuSetIdx[iCpuSet];
    AssertReturnVoid(i < pGip->cCpus);
    AssertReturnVoid(pGip->aCPUs[i].idCpu == idCpu);

    Assert(RTCpuSetIsMemberByIndex(&pGip->PossibleCpuSet, iCpuSet));
    RTCpuSetDelByIndex(&pGip->OnlineCpuSet, iCpuSet);

    /* Update the Mp online/offline counter. */
    ASMAtomicIncU32(&g_cMpOnOffEvents);

    /* If we are the initiator going offline while measuring the TSC delta, unspin other waiting CPUs! */
    if (ASMAtomicReadU32(&g_idTscDeltaInitiator) == idCpu)
    {
        ASMAtomicWriteU32(&g_pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_START);
        ASMAtomicWriteU64(&pGip->aCPUs[i].u64TSCSample, ~GIP_TSC_DELTA_RSVD);
    }

    /* Reset the TSC delta, we will recalculate it lazily. */
    ASMAtomicWriteS64(&pGip->aCPUs[i].i64TSCDelta, INT64_MAX);

    /* commit it */
    ASMAtomicWriteSize(&pGip->aCPUs[i].enmState, SUPGIPCPUSTATE_OFFLINE);

    RTSpinlockRelease(pDevExt->hGipSpinlock);
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
                supdrvGipMpEventOnline(pDevExt, idCpu);
                break;
            case RTMPEVENT_OFFLINE:
                supdrvGipMpEventOffline(pDevExt, idCpu);
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
            int64_t     iTSCDelta;
            uint32_t    idxNewGipMaster;
            RTCPUID     idNewGipMaster = NIL_RTCPUID;
            RTCPUSET    OnlineCpus;
            RTMpGetOnlineSet(&OnlineCpus);

            for (i = 0; i < RTCPUSET_MAX_CPUS; i++)
            {
                RTCPUID idCurCpu = RTMpCpuIdFromSetIndex(i);
                if (   RTCpuSetIsMember(&OnlineCpus, idCurCpu)
                    && idCurCpu != idGipMaster)
                {
                    idNewGipMaster = idCurCpu;
                    break;
                }
            }

            Log(("supdrvGipMpEvent: Gip master %#lx -> %#lx\n", (long)idGipMaster, (long)idNewGipMaster));
            ASMAtomicCmpXchgSize(&pDevExt->idGipMaster, idNewGipMaster, idGipMaster, fIgnored);
            NOREF(fIgnored);

            /*
             * Adjust all the TSC deltas against the new GIP master.
             */
            if (pGip)
            {
                idxNewGipMaster = supdrvGipCpuIndexFromCpuId(pGip, idNewGipMaster);
                iTSCDelta       = pGip->aCPUs[idxNewGipMaster].i64TSCDelta;
                Assert(iTSCDelta != UINT64_MAX);
                for (i = 0; i < pGip->cCpus; i++)
                {
                    PSUPGIPCPU pGipCpu      = &pGip->aCPUs[i];
                    int64_t    iWorkerDelta = pGipCpu->i64TSCDelta;
                    if (iWorkerDelta != INT64_MAX)
                        iWorkerDelta -= iTSCDelta;
                    ASMAtomicWriteS64(&pGipCpu->i64TSCDelta, iWorkerDelta);
                }
                Assert(pGip->aCPUs[idxNewGipMaster].i64TSCDelta == 0);
            }
        }
    }
}


/**
 * Returns whether the host CPU sports an invariant TSC or not.
 *
 * @returns true if invariant TSC is supported, false otherwise.
 */
static bool supdrvIsInvariantTsc(void)
{
    static bool s_fQueried        = false;
    static bool s_fIsInvariantTsc = false;
    if (!s_fQueried)
    {
        uint32_t uEax, uEbx, uEcx, uEdx;
        ASMCpuId(0x80000000, &uEax, &uEbx, &uEcx, &uEdx);
        if (uEax >= 0x80000007)
        {
            ASMCpuId(0x80000007, &uEax, &uEbx, &uEcx, &uEdx);
            if (uEdx & X86_CPUID_AMD_ADVPOWER_EDX_TSCINVAR)
                s_fIsInvariantTsc = true;
        }
        s_fQueried = true;
    }

    return s_fIsInvariantTsc;
}


/**
 * Callback used by supdrvMeasureTscDeltas() to read the TSC on two CPUs and
 * compute the delta between them.
 *
 * @param   idCpu       The CPU we are current scheduled on.
 * @param   pvUser1     Opaque pointer to the GIP.
 * @param   pvUser2     Opaque pointer to the worker Cpu Id.
 *
 * @remarks Measuring TSC deltas between the CPUs is tricky because we need to
 *     read the TSC at exactly the same time on both the master and the worker
 *     CPUs. Due to DMA, bus arbitration, cache locality, contention, SMI,
 *     pipelining etc. there is no guaranteed way of doing this on x86 CPUs. We
 *     try to minimize the measurement error by computing the minimum read time
 *     of the compare statement in the worker by taking TSC measurements across
 *     it.
 *
 *     We ignore the first few runs of the loop in order to prime the cache.
 *     Also, be careful about using 'pause' instruction in critical busy-wait
 *     loops in this code - it can cause undesired behaviour with
 *     hyperthreading.
 *
 *     It must be noted that the computed minimum read time is mostly to
 *     eliminate huge deltas when the worker is too early and doesn't by itself
 *     help produce more accurate deltas. We allow two times the computed
 *     minimum as an arbibtrary acceptable threshold. Therefore, it is still
 *     possible to get negative deltas where there are none when the worker is
 *     earlier. As long as these occasional negative deltas are lower than the
 *     time it takes to exit guest-context and the OS to reschedule EMT on a
 *     different CPU we won't expose a TSC that jumped backwards. It is because
 *     of the existence of the negative deltas we don't recompute the delta with
 *     the master and worker interchanged to eliminate the remaining measurement
 *     error.
 */
static DECLCALLBACK(void) supdrvMeasureTscDeltaCallback(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PSUPGLOBALINFOPAGE pGip      = (PSUPGLOBALINFOPAGE)pvUser1;
    uint32_t          *pidWorker = (uint32_t *)pvUser2;
    RTCPUID            idMaster  = ASMAtomicUoReadU32(&g_idTscDeltaInitiator);
    unsigned           idxMaster = supdrvGipCpuIndexFromCpuId(pGip, idMaster);
    unsigned           idxWorker = supdrvGipCpuIndexFromCpuId(pGip, *pidWorker);
    PSUPGIPCPU         pGipCpuMaster = &pGip->aCPUs[idxMaster];
    PSUPGIPCPU         pGipCpuWorker = &pGip->aCPUs[idxWorker];
    int                cTriesLeft = 12;

    if (   idCpu != idMaster
        && idCpu != *pidWorker)
        return;

    /* If the IPRT API isn't concurrent safe, the master and worker wait for each other
       with a timeout to avoid deadlocking the entire system. */
    if (!RTMpOnAllIsConcurrentSafe())
    {
        uint64_t       uTscNow;
        uint64_t       uTscStart;
        uint64_t const cWaitTicks = 130000;  /* Arbitrary value, can be tweaked later. */

        ASMSerializeInstruction();
        uTscStart = ASMReadTSC();
        if (idCpu == idMaster)
        {
            ASMAtomicWriteU32(&g_pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_PRESTART_MASTER);
            while (ASMAtomicReadU32(&g_pTscDeltaSync->u) != GIP_TSC_DELTA_SYNC_PRESTART_WORKER)
            {
                ASMSerializeInstruction();
                uTscNow = ASMReadTSC();
                if (uTscNow - uTscStart > cWaitTicks)
                {
                    /* Set the worker delta to indicate failure, not the master. */
                    ASMAtomicWriteS64(&pGipCpuWorker->i64TSCDelta, INT64_MAX);
                    return;
                }

                ASMNopPause();
            }
        }
        else
        {
            while (ASMAtomicReadU32(&g_pTscDeltaSync->u) != GIP_TSC_DELTA_SYNC_PRESTART_MASTER)
            {
                ASMSerializeInstruction();
                uTscNow = ASMReadTSC();
                if (uTscNow - uTscStart > cWaitTicks)
                {
                    ASMAtomicWriteS64(&pGipCpuWorker->i64TSCDelta, INT64_MAX);
                    return;
                }

                ASMNopPause();
            }
            ASMAtomicWriteU32(&g_pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_PRESTART_WORKER);
        }
    }

    Assert(pGipCpuWorker->i64TSCDelta == INT64_MAX);
    while (cTriesLeft-- > 0)
    {
        unsigned    i;
        uint64_t    uMinCmpReadTime = UINT64_MAX;
        for (i = 0; i < GIP_TSC_DELTA_LOOPS; i++)
        {
            RTCCUINTREG uFlags = ASMIntDisableFlags();   /* Disable interrupts per-iteration, see @bugref{6710} comment #38. */
            if (idCpu == idMaster)
            {
                /*
                 * The master.
                 */
                Assert(pGipCpuMaster->u64TSCSample == GIP_TSC_DELTA_RSVD);
                ASMAtomicWriteU32(&g_pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_START);
                while (ASMAtomicReadU32(&g_pTscDeltaSync->u) == GIP_TSC_DELTA_SYNC_START)
                    ;

                do
                {
                    ASMSerializeInstruction();
                    ASMAtomicWriteU64(&pGipCpuMaster->u64TSCSample, ASMReadTSC());
                } while (pGipCpuMaster->u64TSCSample == GIP_TSC_DELTA_RSVD);

                while (ASMAtomicReadU32(&g_pTscDeltaSync->u) != GIP_TSC_DELTA_SYNC_WORKER_DONE)
                    ;

                if (i > GIP_TSC_DELTA_PRIMER_LOOPS + GIP_TSC_DELTA_READ_TIME_LOOPS)
                {
                    if (pGipCpuWorker->u64TSCSample != GIP_TSC_DELTA_RSVD)
                    {
                        int64_t iDelta = pGipCpuWorker->u64TSCSample - pGipCpuMaster->u64TSCSample;
                        if (iDelta < pGipCpuWorker->i64TSCDelta)
                            pGipCpuWorker->i64TSCDelta = iDelta;
                    }
                }

                ASMAtomicWriteU64(&pGipCpuMaster->u64TSCSample, GIP_TSC_DELTA_RSVD);
                ASMAtomicWriteU32(&g_pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_STOP);
            }
            else
            {
                /*
                 * The worker.
                 */
                uint64_t uTscWorker;
                uint64_t uTscWorkerFlushed;
                uint64_t uCmpReadTime;

                ASMAtomicReadU64(&pGipCpuMaster->u64TSCSample);     /* Warm the cache line. */
                while (ASMAtomicReadU32(&g_pTscDeltaSync->u) != GIP_TSC_DELTA_SYNC_START)
                    ;
                Assert(pGipCpuMaster->u64TSCSample == GIP_TSC_DELTA_RSVD);
                ASMAtomicWriteU32(&g_pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_WORKER_READY);

                /*
                 * Keep reading the TSC until we notice that the master has read his. Reading
                 * the TSC -after- the master has updated the memory is way too late. We thus
                 * compensate by trying to measure how long it took for the worker to notice
                 * the memory flushed from the master.
                 */
                do
                {
                    ASMSerializeInstruction();
                    uTscWorker = ASMReadTSC();
                } while (pGipCpuMaster->u64TSCSample == GIP_TSC_DELTA_RSVD);
                ASMSerializeInstruction();
                uTscWorkerFlushed = ASMReadTSC();

                uCmpReadTime = uTscWorkerFlushed - uTscWorker;
                if (i > GIP_TSC_DELTA_PRIMER_LOOPS + GIP_TSC_DELTA_READ_TIME_LOOPS)
                {
                    /* This is totally arbitrary a.k.a I don't like it but I have no better ideas for now. */
                    if (uCmpReadTime < (uMinCmpReadTime << 1))
                    {
                        ASMAtomicWriteU64(&pGipCpuWorker->u64TSCSample, uTscWorker);
                        if (uCmpReadTime < uMinCmpReadTime)
                            uMinCmpReadTime = uCmpReadTime;
                    }
                    else
                        ASMAtomicWriteU64(&pGipCpuWorker->u64TSCSample, GIP_TSC_DELTA_RSVD);
                }
                else if (i > GIP_TSC_DELTA_PRIMER_LOOPS)
                {
                    if (uCmpReadTime < uMinCmpReadTime)
                        uMinCmpReadTime = uCmpReadTime;
                }

                ASMAtomicWriteU32(&g_pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_WORKER_DONE);
                while (ASMAtomicReadU32(&g_pTscDeltaSync->u) == GIP_TSC_DELTA_SYNC_WORKER_DONE)
                    ASMNopPause();
            }

            ASMSetFlags(uFlags);
        }

        if (pGipCpuWorker->i64TSCDelta != INT64_MAX)
            break;
    }
}


/**
 * Clears all TSCs on the per-CPUs GIP struct. as well as the delta
 * synchronization variable. Optionally also clears the deltas on the per-CPU
 * GIP struct. as well.
 *
 * @param   pGip            Pointer to the GIP.
 * @param   fClearDeltas    Whether the deltas are also to be cleared.
 */
DECLINLINE(void) supdrvClearTscSamples(PSUPGLOBALINFOPAGE pGip, bool fClearDeltas)
{
    unsigned iCpu;
    for (iCpu = 0; iCpu < pGip->cCpus; iCpu++)
    {
        PSUPGIPCPU pGipCpu = &pGip->aCPUs[iCpu];
        ASMAtomicWriteU64(&pGipCpu->u64TSCSample, GIP_TSC_DELTA_RSVD);
        if (fClearDeltas)
            ASMAtomicWriteS64(&pGipCpu->i64TSCDelta, INT64_MAX);
    }
    ASMAtomicWriteU32(&g_pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_STOP);
}


/**
 * Measures the TSC delta between the master GIP CPU and one specified worker
 * CPU.
 *
 * @returns VBox status code.
 * @param   pDevExt         Pointer to the device instance data.
 * @param   idxWorker       The index of the worker CPU from the GIP's array of
 *                          CPUs.
 *
 * @remarks This can be called with preemption disabled!
 */
static int supdrvMeasureTscDeltaOne(PSUPDRVDEVEXT pDevExt, uint32_t idxWorker)
{
    int rc;
    PSUPGLOBALINFOPAGE pGip;
    PSUPGIPCPU pGipCpuWorker;
    RTCPUID idMaster;

    AssertReturn(pDevExt, VERR_INVALID_PARAMETER);
    AssertReturn(pDevExt->pGip, VERR_INVALID_PARAMETER);

    pGip          = pDevExt->pGip;
    idMaster      = pDevExt->idGipMaster;
    pGipCpuWorker = &pGip->aCPUs[idxWorker];

    if (pGipCpuWorker->idCpu == idMaster)
    {
        ASMAtomicWriteS64(&pGipCpuWorker->i64TSCDelta, 0);
        return VINF_SUCCESS;
    }

    /* Set the master TSC as the initiator. */
    while (ASMAtomicCmpXchgU32(&g_idTscDeltaInitiator, idMaster, NIL_RTCPUID) == false)
    {
        /*
         * Sleep here rather than spin as there is a parallel measurement
         * being executed and that can take a good while to be done.
         */
        RTThreadSleep(1);
    }

    if (RTCpuSetIsMember(&pGip->OnlineCpuSet, pGipCpuWorker->idCpu))
    {
        /* Fire TSC-read workers on all CPUs but only synchronize between master and one worker to ease memory contention. */
        ASMAtomicWriteS64(&pGipCpuWorker->i64TSCDelta, INT64_MAX);
        ASMAtomicWriteU32(&g_pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_STOP);
        rc = RTMpOnAll(supdrvMeasureTscDeltaCallback, pGip, &pGipCpuWorker->idCpu);
        if (RT_SUCCESS(rc))
        {
            if (RT_UNLIKELY(pGipCpuWorker->i64TSCDelta == INT64_MAX))
                rc = VERR_SUPDRV_TSC_DELTA_MEASUREMENT_FAILED;
        }
    }
    else
        rc = VERR_CPU_OFFLINE;

    ASMAtomicWriteU32(&g_idTscDeltaInitiator, NIL_RTCPUID);
    return rc;
}


/**
 * Measures the TSC deltas between CPUs.
 *
 * @param   pDevExt     Pointer to the device instance data.
 * @param   pidxMaster  Where to store the index of the chosen master TSC if we
 *                      managed to determine the TSC deltas successfully.
 *                      Optional, can be NULL.
 *
 * @returns VBox status code.
 * @remarks Must be called only after supdrvGipInitOnCpu() as this function uses
 *          idCpu, GIP's online CPU set which are populated in
 *          supdrvGipInitOnCpu().
 */
static int supdrvMeasureTscDeltas(PSUPDRVDEVEXT pDevExt, uint32_t *pidxMaster)
{
    PSUPGIPCPU pGipCpuMaster;
    unsigned   iCpu;
    PSUPGLOBALINFOPAGE pGip   = pDevExt->pGip;
    uint32_t   idxMaster      = UINT32_MAX;
    int        rc             = VINF_SUCCESS;
    uint32_t   cMpOnOffEvents = ASMAtomicReadU32(&g_cMpOnOffEvents);
    uint32_t   cOnlineCpus    = pGip->cOnlineCpus;

    /*
     * If we determined the TSC is async., don't bother with measuring deltas.
     */
    if (RT_UNLIKELY(pGip->u32Mode == SUPGIPMODE_ASYNC_TSC))
        return VINF_SUCCESS;

    /*
     * Pick the first CPU online as the master TSC and make it the new GIP master based
     * on the APIC ID.
     *
     * Technically we can simply use "idGipMaster" but doing this gives us master as CPU 0
     * in most cases making it nicer/easier for comparisons. It is safe to update the GIP
     * master as this point since the sync/async timer isn't created yet.
     */
    supdrvClearTscSamples(pGip, true /* fClearDeltas */);
    for (iCpu = 0; iCpu < RT_ELEMENTS(pGip->aiCpuFromApicId); iCpu++)
    {
        uint16_t idxCpu = pGip->aiCpuFromApicId[iCpu];
        if (idxCpu != UINT16_MAX)
        {
            PSUPGIPCPU pGipCpu = &pGip->aCPUs[idxCpu];
            if (RTCpuSetIsMember(&pGip->OnlineCpuSet, pGipCpu->idCpu))
            {
                idxMaster = idxCpu;
                pGipCpu->i64TSCDelta = 0;
                break;
            }
        }
    }
    AssertReturn(idxMaster != UINT32_MAX, VERR_CPU_NOT_FOUND);
    pGipCpuMaster = &pGip->aCPUs[idxMaster];
    ASMAtomicWriteSize(&pDevExt->idGipMaster, pGipCpuMaster->idCpu);

    AssertReturn(cOnlineCpus > 0, VERR_INTERNAL_ERROR_5);
    if (pGip->cOnlineCpus <= 1)
    {
        if (pidxMaster)
            *pidxMaster = idxMaster;
        return VINF_SUCCESS;
    }

    for (iCpu = 0; iCpu < pGip->cCpus; iCpu++)
    {
        PSUPGIPCPU pGipCpuWorker = &pGip->aCPUs[iCpu];
        if (   iCpu != idxMaster
            && RTCpuSetIsMember(&pGip->OnlineCpuSet, pGipCpuWorker->idCpu))
        {
            rc = supdrvMeasureTscDeltaOne(pDevExt, iCpu);
            if (RT_FAILURE(rc))
            {
                SUPR0Printf("supdrvMeasureTscDeltaOne failed. rc=%d CPU[%u].idCpu=%u Master[%u].idCpu=%u\n", rc, iCpu,
                            pGipCpuWorker->idCpu, idxMaster, pDevExt->idGipMaster, pGipCpuMaster->idCpu);
                break;
            }

            if (ASMAtomicReadU32(&g_cMpOnOffEvents) != cMpOnOffEvents)
            {
                SUPR0Printf("One or more CPUs transitioned between online & offline states. I'm confused, retrying...\n");
                rc = VERR_TRY_AGAIN;
                break;
            }
        }
    }

    if (   RT_SUCCESS(rc)
        && !pGipCpuMaster->i64TSCDelta
        && pidxMaster)
    {
        *pidxMaster = idxMaster;
    }
    return rc;
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
    ASMAtomicWriteU64((uint64_t volatile *)pvUser1, ASMReadTSC());
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
#if 0
    if (supdrvIsInvariantTsc())
        return SUPGIPMODE_SYNC_TSC;     /** @todo Switch to SUPGIPMODE_INVARIANT_TSC later. */
#endif

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
        if (   uEAX >= 1
            && ASMIsAmdCpuEx(uEBX, uECX, uEDX))
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
    pCpu->u64TSCSample       = GIP_TSC_DELTA_RSVD;
    pCpu->i64TSCDelta        = INT64_MAX;

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
 * @param   pDevExt             Pointer to the device instance data.
 * @param   pGip                Pointer to the read-write kernel mapping of the GIP.
 * @param   HCPhys              The physical address of the GIP.
 * @param   u64NanoTS           The current nanosecond timestamp.
 * @param   uUpdateHz           The update frequency.
 * @param   uUpdateIntervalNS   The update interval in nanoseconds.
 * @param   cCpus               The CPU count.
 */
static void supdrvGipInit(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE pGip, RTHCPHYS HCPhys,
                          uint64_t u64NanoTS, unsigned uUpdateHz, unsigned uUpdateIntervalNS, unsigned cCpus)
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
    pGip->u32UpdateIntervalNS   = uUpdateIntervalNS;
    RTCpuSetEmpty(&pGip->OnlineCpuSet);
    RTCpuSetEmpty(&pGip->PresentCpuSet);
    RTMpGetSet(&pGip->PossibleCpuSet);
    pGip->cOnlineCpus           = RTMpGetOnlineCount();
    pGip->cPresentCpus          = RTMpGetPresentCount();
    pGip->cPossibleCpus         = RTMpGetCount();
    pGip->idCpuMax              = RTMpGetMaxCpuId();
    for (i = 0; i < RT_ELEMENTS(pGip->aiCpuFromApicId); i++)
        pGip->aiCpuFromApicId[i]    = UINT16_MAX;
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

    /*
     * Allocate the TSC delta sync. struct. on a separate cache line.
     */
    g_pvTscDeltaSync = RTMemAllocZ(sizeof(SUPTSCDELTASYNC) + 63);
    g_pTscDeltaSync  = RT_ALIGN_PT(g_pvTscDeltaSync, 64, PSUPTSCDELTASYNC);
    Assert(RT_ALIGN_PT(g_pTscDeltaSync, 64, PSUPTSCDELTASYNC) == g_pTscDeltaSync);
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
    supdrvGipMpEventOnline((PSUPDRVDEVEXT)pvUser1, idCpu);
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
    for (i = 0; i < pGip->cCpus; i++)
    {
        pGip->aCPUs[i].u64NanoTS = 0;
        pGip->aCPUs[i].u64TSC = 0;
        pGip->aCPUs[i].iTSCHistoryHead = 0;
        pGip->aCPUs[i].u64TSCSample = 0;
        pGip->aCPUs[i].i64TSCDelta = INT64_MAX;
    }

    if (g_pvTscDeltaSync)
    {
        RTMemFree(g_pvTscDeltaSync);
        g_pTscDeltaSync  = NULL;
        g_pvTscDeltaSync = NULL;
    }
}


/**
 * Worker routine for supdrvGipUpdate and supdrvGipUpdatePerCpu that
 * updates all the per cpu data except the transaction id.
 *
 * @param   pDevExt         The device extension.
 * @param   pGipCpu         Pointer to the per cpu data.
 * @param   u64NanoTS       The current time stamp.
 * @param   u64TSC          The current TSC.
 * @param   iTick           The current timer tick.
 */
static void supdrvGipDoUpdateCpu(PSUPDRVDEVEXT pDevExt, PSUPGIPCPU pGipCpu, uint64_t u64NanoTS, uint64_t u64TSC, uint64_t iTick)
{
    uint64_t    u64TSCDelta;
    uint32_t    u32UpdateIntervalTSC;
    uint32_t    u32UpdateIntervalTSCSlack;
    unsigned    iTSCHistoryHead;
    uint64_t    u64CpuHz;
    uint32_t    u32TransactionId;

    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
    AssertPtrReturnVoid(pGip);

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
     *
     * On Windows, we have an occasional (but recurring) sour value that messed up
     * the history but taking only 1 interval reduces the precision overall.
     * However, this problem existed before the invariant mode was introduced.
     */
    if (   supdrvIsInvariantTsc()
        || pGip->u32UpdateHz >= 1000)
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

        /* Value chosen for a 2GHz Athlon64 running linux 2.6.10/11. */
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
    u64CpuHz = ASMMult2xU32RetU64(u32UpdateIntervalTSC, RT_NS_1SEC_64);
    u64CpuHz /= pGip->u32UpdateIntervalNS;
    ASMAtomicWriteU64(&pGipCpu->u64CpuHz, u64CpuHz);
}


/**
 * Updates the GIP.
 *
 * @param   pDevExt         The device extension.
 * @param   u64NanoTS       The current nanosecond timesamp.
 * @param   u64TSC          The current TSC timesamp.
 * @param   idCpu           The CPU ID.
 * @param   iTick           The current timer tick.
 */
static void supdrvGipUpdate(PSUPDRVDEVEXT pDevExt, uint64_t u64NanoTS, uint64_t u64TSC, RTCPUID idCpu, uint64_t iTick)
{
    /*
     * Determine the relevant CPU data.
     */
    PSUPGIPCPU pGipCpu;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
    AssertPtrReturnVoid(pGip);

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
            uint32_t u32UpdateHz = (uint32_t)((RT_NS_1SEC_64 * GIP_UPDATEHZ_RECALC_FREQ) / u64Delta);
            if (u32UpdateHz <= 2000 && u32UpdateHz >= 30)
            {
                /** @todo r=ramshankar: Changing u32UpdateHz might screw up TSC frequency
                 *        calculation on non-invariant hosts if it changes the history decision
                 *        taken in supdrvGipDoUpdateCpu(). */
                ASMAtomicWriteU32(&pGip->u32UpdateHz, u32UpdateHz);
                ASMAtomicWriteU32(&pGip->u32UpdateIntervalNS, u64Delta / GIP_UPDATEHZ_RECALC_FREQ);
            }
#endif
        }
        ASMAtomicWriteU64(&pGip->u64NanoTSLastUpdateHz, u64NanoTS + 1);
    }

    /*
     * Update the data.
     */
    supdrvGipDoUpdateCpu(pDevExt, pGipCpu, u64NanoTS, u64TSC, iTick);

    /*
     * Complete transaction.
     */
    ASMAtomicIncU32(&pGipCpu->u32TransactionId);
}


/**
 * Updates the per cpu GIP data for the calling cpu.
 *
 * @param   pDevExt         The device extension.
 * @param   u64NanoTS       The current nanosecond timesamp.
 * @param   u64TSC          The current TSC timesamp.
 * @param   idCpu           The CPU ID.
 * @param   idApic          The APIC id for the CPU index.
 * @param   iTick           The current timer tick.
 */
static void supdrvGipUpdatePerCpu(PSUPDRVDEVEXT pDevExt, uint64_t u64NanoTS, uint64_t u64TSC,
                                  RTCPUID idCpu, uint8_t idApic, uint64_t iTick)
{
    uint32_t iCpu;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;

    /*
     * Avoid a potential race when a CPU online notification doesn't fire on
     * the onlined CPU but the tick creeps in before the event notification is
     * run.
     */
    if (RT_UNLIKELY(iTick == 1))
    {
        iCpu = supdrvGipCpuIndexFromCpuId(pGip, idCpu);
        if (pGip->aCPUs[iCpu].enmState == SUPGIPCPUSTATE_OFFLINE)
            supdrvGipMpEventOnline(pDevExt, idCpu);
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
            supdrvGipDoUpdateCpu(pDevExt, pGipCpu, u64NanoTS, u64TSC, iTick);

            /*
             * Complete transaction.
             */
            ASMAtomicIncU32(&pGipCpu->u32TransactionId);
        }
    }
}


/**
 * Resume built-in keyboard on MacBook Air and Pro hosts.
 * If there is no built-in keyboard device, return success anyway.
 *
 * @returns 0 on Mac OS X platform, VERR_NOT_IMPLEMENTED on the other ones.
 */
static int supdrvIOCtl_ResumeSuspendedKbds(void)
{
#if defined(RT_OS_DARWIN)
    return supdrvDarwinResumeSuspendedKbds();
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * Service a TSC-delta measurement request.
 *
 * @returns VBox status code.
 * @param   pDevExt             Pointer to the device instance data.
 * @param   pReq                Pointer to the TSC-delta measurement request.
 */
static int supdrvIOCtl_TscDeltaMeasure(PSUPDRVDEVEXT pDevExt, PSUPTSCDELTAMEASURE pReq)
{
    PSUPGLOBALINFOPAGE pGip;
    RTCPUID            idCpuWorker;
    int                rc = VERR_CPU_NOT_FOUND;
    int16_t            cTries;
    RTMSINTERVAL       cMsWaitRetry;
    uint16_t           iCpu;

    /*
     * Validate.
     */
    AssertReturn(pDevExt, VERR_INVALID_PARAMETER);
    AssertReturn(pReq, VERR_INVALID_PARAMETER);
    AssertReturn(pDevExt->pGip, VERR_INVALID_PARAMETER);
    idCpuWorker = pReq->u.In.idCpu;
    if (idCpuWorker == NIL_RTCPUID)
        return VERR_INVALID_CPU_ID;

    cTries       = RT_MAX(pReq->u.In.cRetries + 1, 10);
    cMsWaitRetry = RT_MAX(pReq->u.In.cMsWaitRetry, 5);
    pGip = pDevExt->pGip;
    for (iCpu = 0; iCpu < pGip->cCpus; iCpu++)
    {
        PSUPGIPCPU pGipCpuWorker = &pGip->aCPUs[iCpu];
        if (pGipCpuWorker->idCpu == idCpuWorker)
        {
            if (   pGipCpuWorker->i64TSCDelta != INT64_MAX
                && !pReq->u.In.fForce)
                return VINF_SUCCESS;

#ifdef SUPDRV_USE_TSC_DELTA_THREAD
            if (pReq->u.In.fAsync)
            {
                /** @todo Async. doesn't implement options like retries, waiting. We'll need
                 *        to pass those options to the thread somehow and implement it in the
                 *        thread. Check if anyone uses/needs fAsync before implementing this. */
                RTCpuSetAdd(&pDevExt->TscDeltaCpuSet, idCpu);
                RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
                if (   pDevExt->enmTscDeltaState == kSupDrvTscDeltaState_Listening
                    || pDevExt->enmTscDeltaState == kSupDrvTscDeltaState_Measuring)
                {
                    pDevExt->enmTscDeltaState = kSupDrvTscDeltaState_WaitAndMeasure;
                }
                RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
                RTThreadUserSignal(pDevExt->hTscDeltaThread);
                return VINF_SUCCESS;
            }
#endif

            while (cTries--)
            {
                rc = supdrvMeasureTscDeltaOne(pDevExt, iCpu);
                if (RT_SUCCESS(rc))
                {
                    Assert(pGipCpuWorker->i64TSCDelta != INT64_MAX);
                    break;
                }

                if (cMsWaitRetry)
                    RTThreadSleep(cMsWaitRetry);
            }

            break;
        }
    }
    return rc;
}


/**
 * Reads the TSC and TSC-delta atomically, applies the TSC delta.
 *
 * @returns VBox status code.
 * @param   pDevExt         Pointer to the device instance data.
 * @param   pReq            Pointer to the TSC-read request.
 */
static int supdrvIOCtl_TscRead(PSUPDRVDEVEXT pDevExt, PSUPTSCREAD pReq)
{
    uint64_t uTsc;
    uint16_t idApic;
    int16_t cTries;
    PSUPGLOBALINFOPAGE pGip;
    int rc;

    /*
     * Validate.
     */
    AssertReturn(pDevExt, VERR_INVALID_PARAMETER);
    AssertReturn(pReq, VERR_INVALID_PARAMETER);
    AssertReturn(pDevExt->pGip, VERR_INVALID_PARAMETER);
    pGip = pDevExt->pGip;

    cTries = 4;
    while (cTries-- > 0)
    {
        rc = SUPReadTsc(&uTsc, &idApic);
        if (RT_SUCCESS(rc))
        {
            pReq->u.Out.u64AdjustedTsc = uTsc;
            pReq->u.Out.idApic = idApic;
            return VINF_SUCCESS;
        }
        else
        {
            int rc2;

            /* If we failed to have a delta, measurement the delta and retry. */
            AssertReturn(idApic < RT_ELEMENTS(pGip->aiCpuFromApicId), VERR_INVALID_CPU_ID);
            uint16_t iCpu = pGip->aiCpuFromApicId[idApic];
            AssertMsgReturn(iCpu < pGip->cCpus, ("iCpu=%u cCpus=%u\n", iCpu, pGip->cCpus), VERR_WRONG_ORDER);

            rc2 = supdrvMeasureTscDeltaOne(pDevExt, iCpu);
            if (RT_SUCCESS(rc2))
                AssertReturn(pGip->aCPUs[iCpu].i64TSCDelta != INT64_MAX, VERR_INTERNAL_ERROR_2);
        }
    }

    return rc;
}

