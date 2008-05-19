/* $Id$ */
/** @file
 * TM - Timeout Manager.
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


/** @page pg_tm        TM - The Time Manager
 *
 * The Time Manager abstracts the CPU clocks and manages timers used by the VMM,
 * device and drivers.
 *
 *
 * @section sec_tm_clocks   Clocks
 *
 * There are currently 4 clocks:
 *   - Virtual (guest).
 *   - Synchronous virtual (guest).
 *   - CPU Tick (TSC) (guest). Only current use is rdtsc emulation. Usually a
 *     function of the virtual clock.
 *   - Real (host). The only current use is display updates for not real
 *     good reason...
 *
 * The interesting clocks are two first ones, the virtual and synchronous virtual
 * clock. The synchronous virtual clock is tied to the virtual clock except that
 * it will take into account timer delivery lag caused by host scheduling. It will
 * normally never advance beyond the header timer, and when lagging too far behind
 * it will gradually speed up to catch up with the virtual clock.
 *
 * The CPU tick (TSC) is normally virtualized as a function of the virtual time,
 * where the frequency defaults to the host cpu frequency (as we measure it). It
 * can also use the host TSC as source and either present it with an offset or
 * unmodified. It is of course possible to configure the TSC frequency and mode
 * of operation.
 *
 * @subsection subsec_tm_timesync Guest Time Sync / UTC time
 *
 * Guest time syncing is primarily taken care of by the VMM device. The principle
 * is very simple, the guest additions periodically asks the VMM device what the
 * current UTC time is and makes adjustments accordingly. Now, because the
 * synchronous virtual clock might be doing catchups and we would therefore
 * deliver more than the normal rate for a little while, some adjusting of the
 * UTC time is required before passing it on to the guest. This is why TM provides
 * an API for query the current UTC time.
 *
 *
 * @section sec_tm_timers   Timers
 *
 * The timers can use any of the TM clocks described in the previous section. Each
 * clock has its own scheduling facility, or timer queue if you like. There are
 * a few factors which makes it a bit complex. First there is the usual R0 vs R3
 * vs. GC thing. Then there is multiple threads, and then there is the timer thread
 * that periodically checks whether any timers has expired without EMT noticing. On
 * the API level, all but the create and save APIs must be mulithreaded. EMT will
 * always run the timers.
 *
 * The design is using a doubly linked list of active timers which is ordered
 * by expire date. This list is only modified by the EMT thread. Updates to the
 * list are are batched in a singly linked list, which is then process by the EMT
 * thread at the first opportunity (immediately, next time EMT modifies a timer
 * on that clock, or next timer timeout). Both lists are offset based and all
 * the elements therefore allocated from the hyper heap.
 *
 * For figuring out when there is need to schedule and run timers TM will:
 *    - Poll whenever somebody queries the virtual clock.
 *    - Poll the virtual clocks from the EM and REM loops.
 *    - Poll the virtual clocks from trap exit path.
 *    - Poll the virtual clocks and calculate first timeout from the halt loop.
 *    - Employ a thread which periodically (100Hz) polls all the timer queues.
 *
 *
 * @section sec_tm_timer    Logging
 *
 * Level 2: Logs a most of the timer state transitions and queue servicing.
 * Level 3: Logs a few oddments.
 * Level 4: Logs TMCLOCK_VIRTUAL_SYNC catch-up events.
 *
 */




/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_TM
#include <VBox/tm.h>
#include <VBox/vmm.h>
#include <VBox/mm.h>
#include <VBox/ssm.h>
#include <VBox/dbgf.h>
#include <VBox/rem.h>
#include <VBox/pdm.h>
#include "TMInternal.h"
#include <VBox/vm.h>

#include <VBox/param.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include <iprt/timer.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/env.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The current saved state version.*/
#define TM_SAVED_STATE_VERSION  3


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static bool                 tmR3HasFixedTSC(void);
static uint64_t             tmR3CalibrateTSC(void);
static DECLCALLBACK(int)    tmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)    tmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
static DECLCALLBACK(void)   tmR3TimerCallback(PRTTIMER pTimer, void *pvUser);
static void                 tmR3TimerQueueRun(PVM pVM, PTMTIMERQUEUE pQueue);
static void                 tmR3TimerQueueRunVirtualSync(PVM pVM);
static DECLCALLBACK(void)   tmR3TimerInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void)   tmR3TimerInfoActive(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void)   tmR3InfoClocks(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);


/**
 * Internal function for getting the clock time.
 *
 * @returns clock time.
 * @param   pVM         The VM handle.
 * @param   enmClock    The clock.
 */
DECLINLINE(uint64_t) tmClock(PVM pVM, TMCLOCK enmClock)
{
    switch (enmClock)
    {
        case TMCLOCK_VIRTUAL:       return TMVirtualGet(pVM);
        case TMCLOCK_VIRTUAL_SYNC:  return TMVirtualSyncGet(pVM);
        case TMCLOCK_REAL:          return TMRealGet(pVM);
        case TMCLOCK_TSC:           return TMCpuTickGet(pVM);
        default:
            AssertMsgFailed(("enmClock=%d\n", enmClock));
            return ~(uint64_t)0;
    }
}


/**
 * Initializes the TM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMR3DECL(int) TMR3Init(PVM pVM)
{
    LogFlow(("TMR3Init:\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertRelease(!(RT_OFFSETOF(VM, tm.s) & 31));
    AssertRelease(sizeof(pVM->tm.s) <= sizeof(pVM->tm.padding));

    /*
     * Init the structure.
     */
    void *pv;
    int rc = MMHyperAlloc(pVM, sizeof(pVM->tm.s.paTimerQueuesR3[0]) * TMCLOCK_MAX, 0, MM_TAG_TM, &pv);
    AssertRCReturn(rc, rc);
    pVM->tm.s.paTimerQueuesR3 = (PTMTIMERQUEUE)pv;

    pVM->tm.s.offVM = RT_OFFSETOF(VM, tm.s);
    pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL].enmClock        = TMCLOCK_VIRTUAL;
    pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL].u64Expire       = INT64_MAX;
    pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL_SYNC].enmClock   = TMCLOCK_VIRTUAL_SYNC;
    pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL_SYNC].u64Expire  = INT64_MAX;
    pVM->tm.s.paTimerQueuesR3[TMCLOCK_REAL].enmClock           = TMCLOCK_REAL;
    pVM->tm.s.paTimerQueuesR3[TMCLOCK_REAL].u64Expire          = INT64_MAX;
    pVM->tm.s.paTimerQueuesR3[TMCLOCK_TSC].enmClock            = TMCLOCK_TSC;
    pVM->tm.s.paTimerQueuesR3[TMCLOCK_TSC].u64Expire           = INT64_MAX;

    /*
     * We directly use the GIP to calculate the virtual time. We map the
     * the GIP into the guest context so we can do this calculation there
     * as well and save costly world switches.
     */
    pVM->tm.s.pvGIPR3 = (void *)g_pSUPGlobalInfoPage;
    AssertMsgReturn(pVM->tm.s.pvGIPR3, ("GIP support is now required!\n"), VERR_INTERNAL_ERROR);
    RTHCPHYS HCPhysGIP;
    rc = SUPGipGetPhys(&HCPhysGIP);
    AssertMsgRCReturn(rc, ("Failed to get GIP physical address!\n"), rc);

    rc = MMR3HyperMapHCPhys(pVM, pVM->tm.s.pvGIPR3, HCPhysGIP, PAGE_SIZE, "GIP", &pVM->tm.s.pvGIPGC);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to map GIP into GC, rc=%Vrc!\n", rc));
        return rc;
    }
    LogFlow(("TMR3Init: HCPhysGIP=%RHp at %VGv\n", HCPhysGIP, pVM->tm.s.pvGIPGC));
    MMR3HyperReserve(pVM, PAGE_SIZE, "fence", NULL);

    /* Check assumptions made in TMAllVirtual.cpp about the GIP update interval. */
    if (    g_pSUPGlobalInfoPage->u32Magic == SUPGLOBALINFOPAGE_MAGIC
        &&  g_pSUPGlobalInfoPage->u32UpdateIntervalNS >= 250000000 /* 0.25s */)
        return VMSetError(pVM, VERR_INTERNAL_ERROR, RT_SRC_POS,
                          N_("The GIP update interval is too big. u32UpdateIntervalNS=%RU32 (u32UpdateHz=%RU32)"),
                          g_pSUPGlobalInfoPage->u32UpdateIntervalNS, g_pSUPGlobalInfoPage->u32UpdateHz);

    /*
     * Setup the VirtualGetRaw backend.
     */
    pVM->tm.s.VirtualGetRawDataR3.pu64Prev = &pVM->tm.s.u64VirtualRawPrev;
    pVM->tm.s.VirtualGetRawDataR3.pfnBad = tmVirtualNanoTSBad;
    pVM->tm.s.VirtualGetRawDataR3.pfnRediscover = tmVirtualNanoTSRediscover;
    if (ASMCpuId_EDX(1) & X86_CPUID_FEATURE_EDX_SSE2)
    {
        if (g_pSUPGlobalInfoPage->u32Mode == SUPGIPMODE_SYNC_TSC)
            pVM->tm.s.pfnVirtualGetRawR3 = RTTimeNanoTSLFenceSync;
        else
            pVM->tm.s.pfnVirtualGetRawR3 = RTTimeNanoTSLFenceAsync;
    }
    else
    {
        if (g_pSUPGlobalInfoPage->u32Mode == SUPGIPMODE_SYNC_TSC)
            pVM->tm.s.pfnVirtualGetRawR3 = RTTimeNanoTSLegacySync;
        else
            pVM->tm.s.pfnVirtualGetRawR3 = RTTimeNanoTSLegacyAsync;
    }

    pVM->tm.s.VirtualGetRawDataGC.pu64Prev = MMHyperR3ToGC(pVM, (void *)&pVM->tm.s.u64VirtualRawPrev);
    pVM->tm.s.VirtualGetRawDataR0.pu64Prev = MMHyperR3ToR0(pVM, (void *)&pVM->tm.s.u64VirtualRawPrev);
    AssertReturn(pVM->tm.s.VirtualGetRawDataR0.pu64Prev, VERR_INTERNAL_ERROR);
    /* The rest is done in TMR3InitFinalize since it's too early to call PDM. */


    /*
     * Get our CFGM node, create it if necessary.
     */
    PCFGMNODE pCfgHandle = CFGMR3GetChild(CFGMR3GetRoot(pVM), "TM");
    if (!pCfgHandle)
    {
        rc = CFGMR3InsertNode(CFGMR3GetRoot(pVM), "TM", &pCfgHandle);
        AssertRCReturn(rc, rc);
    }

    /*
     * Determin the TSC configuration and frequency.
     */
    /* mode */
    rc = CFGMR3QueryBool(pCfgHandle, "TSCVirtualized", &pVM->tm.s.fTSCVirtualized);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->tm.s.fTSCVirtualized = true; /* trap rdtsc */
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS,
                          N_("Configuration error: Failed to querying bool value \"UseRealTSC\""));

    /* source */
    rc = CFGMR3QueryBool(pCfgHandle, "UseRealTSC", &pVM->tm.s.fTSCUseRealTSC);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->tm.s.fTSCUseRealTSC = false; /* use virtual time */
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS,
                          N_("Configuration error: Failed to querying bool value \"UseRealTSC\""));
    if (!pVM->tm.s.fTSCUseRealTSC)
        pVM->tm.s.fTSCVirtualized = true;

    /* TSC reliability */
    rc = CFGMR3QueryBool(pCfgHandle, "MaybeUseOffsettedHostTSC", &pVM->tm.s.fMaybeUseOffsettedHostTSC);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        if (!pVM->tm.s.fTSCUseRealTSC)
            pVM->tm.s.fMaybeUseOffsettedHostTSC = tmR3HasFixedTSC();
        else
            pVM->tm.s.fMaybeUseOffsettedHostTSC = true;
    }

    /* frequency */
    rc = CFGMR3QueryU64(pCfgHandle, "TSCTicksPerSecond", &pVM->tm.s.cTSCTicksPerSecond);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        pVM->tm.s.cTSCTicksPerSecond = tmR3CalibrateTSC();
        if (    !pVM->tm.s.fTSCUseRealTSC
            &&  pVM->tm.s.cTSCTicksPerSecond >= _4G)
        {
            pVM->tm.s.cTSCTicksPerSecond = _4G - 1; /* (A limitation of our math code) */
            pVM->tm.s.fMaybeUseOffsettedHostTSC = false;
        }
    }
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS,
                          N_("Configuration error: Failed to querying uint64_t value \"TSCTicksPerSecond\""));
    else if (   pVM->tm.s.cTSCTicksPerSecond < _1M
             || pVM->tm.s.cTSCTicksPerSecond >= _4G)
        return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                          N_("Configuration error: \"TSCTicksPerSecond\" = %RI64 is not in the range 1MHz..4GHz-1"),
                          pVM->tm.s.cTSCTicksPerSecond);
    else
    {
        pVM->tm.s.fTSCUseRealTSC = pVM->tm.s.fMaybeUseOffsettedHostTSC = false;
        pVM->tm.s.fTSCVirtualized = true;
    }

    /* setup and report */
    if (pVM->tm.s.fTSCVirtualized)
        CPUMR3SetCR4Feature(pVM, X86_CR4_TSD, ~X86_CR4_TSD);
    else
        CPUMR3SetCR4Feature(pVM, 0, ~X86_CR4_TSD);
    LogRel(("TM: cTSCTicksPerSecond=%#RX64 (%RU64) fTSCVirtualized=%RTbool fTSCUseRealTSC=%RTbool fMaybeUseOffsettedHostTSC=%RTbool\n",
            pVM->tm.s.cTSCTicksPerSecond, pVM->tm.s.cTSCTicksPerSecond, pVM->tm.s.fTSCVirtualized,
            pVM->tm.s.fTSCUseRealTSC, pVM->tm.s.fMaybeUseOffsettedHostTSC));

    /*
     * Configure the timer synchronous virtual time.
     */
    rc = CFGMR3QueryU32(pCfgHandle, "ScheduleSlack", &pVM->tm.s.u32VirtualSyncScheduleSlack);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->tm.s.u32VirtualSyncScheduleSlack           =   100000; /* 0.100ms (ASSUMES virtual time is nanoseconds) */
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS,
                          N_("Configuration error: Failed to querying 32-bit integer value \"ScheduleSlack\""));

    rc = CFGMR3QueryU64(pCfgHandle, "CatchUpStopThreshold", &pVM->tm.s.u64VirtualSyncCatchUpStopThreshold);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->tm.s.u64VirtualSyncCatchUpStopThreshold    =   500000; /* 0.5ms */
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS,
                          N_("Configuration error: Failed to querying 64-bit integer value \"CatchUpStopThreshold\""));

    rc = CFGMR3QueryU64(pCfgHandle, "CatchUpGiveUpThreshold", &pVM->tm.s.u64VirtualSyncCatchUpGiveUpThreshold);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->tm.s.u64VirtualSyncCatchUpGiveUpThreshold  = UINT64_C(60000000000); /* 60 sec */
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS,
                          N_("Configuration error: Failed to querying 64-bit integer value \"CatchUpGiveUpThreshold\""));


#define TM_CFG_PERIOD(iPeriod, DefStart, DefPct) \
    do \
    { \
        uint64_t u64; \
        rc = CFGMR3QueryU64(pCfgHandle, "CatchUpStartThreshold" #iPeriod, &u64); \
        if (rc == VERR_CFGM_VALUE_NOT_FOUND) \
            u64 = UINT64_C(DefStart); \
        else if (VBOX_FAILURE(rc)) \
            return VMSetError(pVM, rc, RT_SRC_POS, N_("Configuration error: Failed to querying 64-bit integer value \"CatchUpThreshold" #iPeriod "\"")); \
        if (    (iPeriod > 0 && u64 <= pVM->tm.s.aVirtualSyncCatchUpPeriods[iPeriod - 1].u64Start) \
            ||  u64 >= pVM->tm.s.u64VirtualSyncCatchUpGiveUpThreshold) \
            return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS, N_("Configuration error: Invalid start of period #" #iPeriod ": %RU64"), u64); \
        pVM->tm.s.aVirtualSyncCatchUpPeriods[iPeriod].u64Start = u64; \
        rc = CFGMR3QueryU32(pCfgHandle, "CatchUpPrecentage" #iPeriod, &pVM->tm.s.aVirtualSyncCatchUpPeriods[iPeriod].u32Percentage); \
        if (rc == VERR_CFGM_VALUE_NOT_FOUND) \
            pVM->tm.s.aVirtualSyncCatchUpPeriods[iPeriod].u32Percentage = (DefPct); \
        else if (VBOX_FAILURE(rc)) \
            return VMSetError(pVM, rc, RT_SRC_POS, N_("Configuration error: Failed to querying 32-bit integer value \"CatchUpPrecentage" #iPeriod "\"")); \
    } while (0)
    /* This needs more tuning. Not sure if we really need so many period and be so gentle. */
    TM_CFG_PERIOD(0,     750000,   5); /* 0.75ms at 1.05x */
    TM_CFG_PERIOD(1,    1500000,  10); /* 1.50ms at 1.10x */
    TM_CFG_PERIOD(2,    8000000,  25); /*    8ms at 1.25x */
    TM_CFG_PERIOD(3,   30000000,  50); /*   30ms at 1.50x */
    TM_CFG_PERIOD(4,   75000000,  75); /*   75ms at 1.75x */
    TM_CFG_PERIOD(5,  175000000, 100); /*  175ms at 2x */
    TM_CFG_PERIOD(6,  500000000, 200); /*  500ms at 3x */
    TM_CFG_PERIOD(7, 3000000000, 300); /*    3s  at 4x */
    TM_CFG_PERIOD(8,30000000000, 400); /*   30s  at 5x */
    TM_CFG_PERIOD(9,55000000000, 500); /*   55s  at 6x */
    AssertCompile(RT_ELEMENTS(pVM->tm.s.aVirtualSyncCatchUpPeriods) == 10);
#undef TM_CFG_PERIOD

    /*
     * Configure real world time (UTC).
     */
    rc = CFGMR3QueryS64(pCfgHandle, "UTCOffset", &pVM->tm.s.offUTC);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->tm.s.offUTC = 0; /* ns */
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS,
                          N_("Configuration error: Failed to querying 64-bit integer value \"UTCOffset\""));

    /*
     * Setup the warp drive.
     */
    rc = CFGMR3QueryU32(pCfgHandle, "WarpDrivePercentage", &pVM->tm.s.u32VirtualWarpDrivePercentage);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        rc = CFGMR3QueryU32(CFGMR3GetRoot(pVM), "WarpDrivePercentage", &pVM->tm.s.u32VirtualWarpDrivePercentage); /* legacy */
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->tm.s.u32VirtualWarpDrivePercentage = 100;
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS,
                          N_("Configuration error: Failed to querying uint32_t value \"WarpDrivePercent\""));
    else if (   pVM->tm.s.u32VirtualWarpDrivePercentage < 2
             || pVM->tm.s.u32VirtualWarpDrivePercentage > 20000)
        return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                          N_("Configuration error: \"WarpDrivePercent\" = %RI32 is not in the range 2..20000"),
                          pVM->tm.s.u32VirtualWarpDrivePercentage);
    pVM->tm.s.fVirtualWarpDrive = pVM->tm.s.u32VirtualWarpDrivePercentage != 100;
    if (pVM->tm.s.fVirtualWarpDrive)
        LogRel(("TM: u32VirtualWarpDrivePercentage=%RI32\n", pVM->tm.s.u32VirtualWarpDrivePercentage));

    /*
     * Start the timer (guard against REM not yielding).
     */
    uint32_t u32Millies;
    rc = CFGMR3QueryU32(pCfgHandle, "TimerMillies", &u32Millies);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        u32Millies = 10;
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS,
                          N_("Configuration error: Failed to query uint32_t value \"TimerMillies\""));
    rc = RTTimerCreate(&pVM->tm.s.pTimer, u32Millies, tmR3TimerCallback, pVM);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to create timer, u32Millies=%d rc=%Vrc.\n", u32Millies, rc));
        return rc;
    }
    Log(("TM: Created timer %p firing every %d millieseconds\n", pVM->tm.s.pTimer, u32Millies));
    pVM->tm.s.u32TimerMillies = u32Millies;

    /*
     * Register saved state.
     */
    rc = SSMR3RegisterInternal(pVM, "tm", 1, TM_SAVED_STATE_VERSION, sizeof(uint64_t) * 8,
                               NULL, tmR3Save, NULL,
                               NULL, tmR3Load, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Register statistics.
     */
    STAM_REL_REG_USED(pVM, (void *)&pVM->tm.s.VirtualGetRawDataR3.c1nsSteps,   STAMTYPE_U32, "/TM/R3/1nsSteps",                  STAMUNIT_OCCURENCES, "Virtual time 1ns steps (due to TSC / GIP variations).");
    STAM_REL_REG_USED(pVM, (void *)&pVM->tm.s.VirtualGetRawDataR3.cBadPrev,    STAMTYPE_U32, "/TM/R3/cBadPrev",                  STAMUNIT_OCCURENCES, "Times the previous virtual time was considered erratic (shouldn't ever happen).");
    STAM_REL_REG_USED(pVM, (void *)&pVM->tm.s.VirtualGetRawDataR0.c1nsSteps,   STAMTYPE_U32, "/TM/R0/1nsSteps",                  STAMUNIT_OCCURENCES, "Virtual time 1ns steps (due to TSC / GIP variations).");
    STAM_REL_REG_USED(pVM, (void *)&pVM->tm.s.VirtualGetRawDataR0.cBadPrev,    STAMTYPE_U32, "/TM/R0/cBadPrev",                  STAMUNIT_OCCURENCES, "Times the previous virtual time was considered erratic (shouldn't ever happen).");
    STAM_REL_REG_USED(pVM, (void *)&pVM->tm.s.VirtualGetRawDataGC.c1nsSteps,   STAMTYPE_U32, "/TM/GC/1nsSteps",                  STAMUNIT_OCCURENCES, "Virtual time 1ns steps (due to TSC / GIP variations).");
    STAM_REL_REG_USED(pVM, (void *)&pVM->tm.s.VirtualGetRawDataGC.cBadPrev,    STAMTYPE_U32, "/TM/GC/cBadPrev",                  STAMUNIT_OCCURENCES, "Times the previous virtual time was considered erratic (shouldn't ever happen).");
    STAM_REL_REG(     pVM, (void *)&pVM->tm.s.offVirtualSync,                  STAMTYPE_U64, "/TM/VirtualSync/CurrentOffset",   STAMUNIT_NS,          "The current offset. (subtract GivenUp to get the lag)");
    STAM_REL_REG_USED(pVM, (void *)&pVM->tm.s.offVirtualSyncGivenUp,           STAMTYPE_U64, "/TM/VirtualSync/GivenUp",         STAMUNIT_NS,          "Nanoseconds of the 'CurrentOffset' that's been given up and won't ever be attemted caught up with.");

#ifdef VBOX_WITH_STATISTICS
    STAM_REG_USED(    pVM, (void *)&pVM->tm.s.VirtualGetRawDataR3.cExpired,    STAMTYPE_U32, "/TM/R3/cExpired",                  STAMUNIT_OCCURENCES, "Times the TSC interval expired (overlaps 1ns steps).");
    STAM_REG_USED(    pVM, (void *)&pVM->tm.s.VirtualGetRawDataR3.cUpdateRaces,STAMTYPE_U32, "/TM/R3/cUpdateRaces",              STAMUNIT_OCCURENCES, "Thread races when updating the previous timestamp.");
    STAM_REG_USED(    pVM, (void *)&pVM->tm.s.VirtualGetRawDataR0.cExpired,    STAMTYPE_U32, "/TM/R0/cExpired",                  STAMUNIT_OCCURENCES, "Times the TSC interval expired (overlaps 1ns steps).");
    STAM_REG_USED(    pVM, (void *)&pVM->tm.s.VirtualGetRawDataR0.cUpdateRaces,STAMTYPE_U32, "/TM/R0/cUpdateRaces",              STAMUNIT_OCCURENCES, "Thread races when updating the previous timestamp.");
    STAM_REG_USED(    pVM, (void *)&pVM->tm.s.VirtualGetRawDataGC.cExpired,    STAMTYPE_U32, "/TM/GC/cExpired",                  STAMUNIT_OCCURENCES, "Times the TSC interval expired (overlaps 1ns steps).");
    STAM_REG_USED(    pVM, (void *)&pVM->tm.s.VirtualGetRawDataGC.cUpdateRaces,STAMTYPE_U32, "/TM/GC/cUpdateRaces",              STAMUNIT_OCCURENCES, "Thread races when updating the previous timestamp.");

    STAM_REG(pVM, &pVM->tm.s.StatDoQueues,          STAMTYPE_PROFILE,       "/TM/DoQueues",         STAMUNIT_TICKS_PER_CALL,    "Profiling timer TMR3TimerQueuesDo.");
    STAM_REG(pVM, &pVM->tm.s.StatDoQueuesSchedule,  STAMTYPE_PROFILE_ADV,   "/TM/DoQueues/Schedule",STAMUNIT_TICKS_PER_CALL,    "The scheduling part.");
    STAM_REG(pVM, &pVM->tm.s.StatDoQueuesRun,       STAMTYPE_PROFILE_ADV,   "/TM/DoQueues/Run",     STAMUNIT_TICKS_PER_CALL,    "The run part.");

    STAM_REG(pVM, &pVM->tm.s.StatPollAlreadySet,    STAMTYPE_COUNTER,       "/TM/PollAlreadySet",   STAMUNIT_OCCURENCES,        "TMTimerPoll calls where the FF was already set.");
    STAM_REG(pVM, &pVM->tm.s.StatPollVirtual,       STAMTYPE_COUNTER,       "/TM/PollHitsVirtual",  STAMUNIT_OCCURENCES,        "The number of times TMTimerPoll found an expired TMCLOCK_VIRTUAL queue.");
    STAM_REG(pVM, &pVM->tm.s.StatPollVirtualSync,   STAMTYPE_COUNTER,       "/TM/PollHitsVirtualSync",STAMUNIT_OCCURENCES,      "The number of times TMTimerPoll found an expired TMCLOCK_VIRTUAL_SYNC queue.");
    STAM_REG(pVM, &pVM->tm.s.StatPollMiss,          STAMTYPE_COUNTER,       "/TM/PollMiss",         STAMUNIT_OCCURENCES,        "TMTimerPoll calls where nothing had expired.");

    STAM_REG(pVM, &pVM->tm.s.StatPostponedR3,       STAMTYPE_COUNTER,       "/TM/PostponedR3",      STAMUNIT_OCCURENCES,        "Postponed due to unschedulable state, in ring-3.");
    STAM_REG(pVM, &pVM->tm.s.StatPostponedR0,       STAMTYPE_COUNTER,       "/TM/PostponedR0",      STAMUNIT_OCCURENCES,        "Postponed due to unschedulable state, in ring-0.");
    STAM_REG(pVM, &pVM->tm.s.StatPostponedGC,       STAMTYPE_COUNTER,       "/TM/PostponedGC",      STAMUNIT_OCCURENCES,        "Postponed due to unschedulable state, in GC.");

    STAM_REG(pVM, &pVM->tm.s.StatScheduleOneGC,     STAMTYPE_PROFILE,       "/TM/ScheduleOneGC",    STAMUNIT_TICKS_PER_CALL,    "Profiling the scheduling of one queue during a TMTimer* call in EMT.\n");
    STAM_REG(pVM, &pVM->tm.s.StatScheduleOneR0,     STAMTYPE_PROFILE,       "/TM/ScheduleOneR0",    STAMUNIT_TICKS_PER_CALL,    "Profiling the scheduling of one queue during a TMTimer* call in EMT.\n");
    STAM_REG(pVM, &pVM->tm.s.StatScheduleOneR3,     STAMTYPE_PROFILE,       "/TM/ScheduleOneR3",    STAMUNIT_TICKS_PER_CALL,    "Profiling the scheduling of one queue during a TMTimer* call in EMT.\n");
    STAM_REG(pVM, &pVM->tm.s.StatScheduleSetFF,     STAMTYPE_COUNTER,       "/TM/ScheduleSetFF",    STAMUNIT_OCCURENCES,        "The number of times the timer FF was set instead of doing scheduling.");

    STAM_REG(pVM, &pVM->tm.s.StatTimerSetGC,        STAMTYPE_PROFILE,       "/TM/TimerSetGC",       STAMUNIT_TICKS_PER_CALL,    "Profiling TMTimerSet calls made in GC.");
    STAM_REG(pVM, &pVM->tm.s.StatTimerSetR0,        STAMTYPE_PROFILE,       "/TM/TimerSetR0",       STAMUNIT_TICKS_PER_CALL,    "Profiling TMTimerSet calls made in ring-0.");
    STAM_REG(pVM, &pVM->tm.s.StatTimerSetR3,        STAMTYPE_PROFILE,       "/TM/TimerSetR3",       STAMUNIT_TICKS_PER_CALL,    "Profiling TMTimerSet calls made in ring-3.");

    STAM_REG(pVM, &pVM->tm.s.StatTimerStopGC,       STAMTYPE_PROFILE,       "/TM/TimerStopGC",      STAMUNIT_TICKS_PER_CALL,    "Profiling TMTimerStop calls made in GC.");
    STAM_REG(pVM, &pVM->tm.s.StatTimerStopR0,       STAMTYPE_PROFILE,       "/TM/TimerStopR0",      STAMUNIT_TICKS_PER_CALL,    "Profiling TMTimerStop calls made in ring-0.");
    STAM_REG(pVM, &pVM->tm.s.StatTimerStopR3,       STAMTYPE_PROFILE,       "/TM/TimerStopR3",      STAMUNIT_TICKS_PER_CALL,    "Profiling TMTimerStop calls made in ring-3.");

    STAM_REG(pVM, &pVM->tm.s.StatVirtualGet,        STAMTYPE_COUNTER,       "/TM/VirtualGet",       STAMUNIT_OCCURENCES,        "The number of times TMTimerGet was called when the clock was running.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualGetSetFF,   STAMTYPE_COUNTER,       "/TM/VirtualGetSetFF",  STAMUNIT_OCCURENCES,        "Times we set the FF when calling TMTimerGet.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualGetSync,    STAMTYPE_COUNTER,       "/TM/VirtualGetSync",   STAMUNIT_OCCURENCES,        "The number of times TMTimerGetSync was called when the clock was running.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualGetSyncSetFF,STAMTYPE_COUNTER,      "/TM/VirtualGetSyncSetFF",STAMUNIT_OCCURENCES,      "Times we set the FF when calling TMTimerGetSync.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualPause,      STAMTYPE_COUNTER,       "/TM/VirtualPause",     STAMUNIT_OCCURENCES,        "The number of times TMR3TimerPause was called.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualResume,     STAMTYPE_COUNTER,       "/TM/VirtualResume",    STAMUNIT_OCCURENCES,        "The number of times TMR3TimerResume was called.");

    STAM_REG(pVM, &pVM->tm.s.StatTimerCallbackSetFF,STAMTYPE_COUNTER,       "/TM/CallbackSetFF",    STAMUNIT_OCCURENCES,        "The number of times the timer callback set FF.");


    STAM_REG(pVM, &pVM->tm.s.StatVirtualSyncCatchup,        STAMTYPE_PROFILE_ADV,   "/TM/VirtualSync/CatchUp",              STAMUNIT_TICKS_PER_OCCURENCE, "Counting and measuring the times spent catching up.");
    STAM_REG(pVM, (void *)&pVM->tm.s.fVirtualSyncCatchUp,       STAMTYPE_U8,        "/TM/VirtualSync/CatchUpActive",        STAMUNIT_NONE,           "Catch-Up active indicator.");
    STAM_REG(pVM, (void *)&pVM->tm.s.u32VirtualSyncCatchUpPercentage, STAMTYPE_U32, "/TM/VirtualSync/CatchUpPercentage",    STAMUNIT_PCT,           "The catch-up percentage. (+100/100 to get clock multiplier)");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualSyncGiveUp,             STAMTYPE_COUNTER,   "/TM/VirtualSync/GiveUp",               STAMUNIT_OCCURENCES,    "Times the catch-up was abandoned.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualSyncGiveUpBeforeStarting,STAMTYPE_COUNTER,  "/TM/VirtualSync/GiveUpBeforeStarting", STAMUNIT_OCCURENCES,    "Times the catch-up was abandoned before even starting. (Typically debugging++.)");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualSyncRun,                STAMTYPE_COUNTER,   "/TM/VirtualSync/Run",                  STAMUNIT_OCCURENCES,    "Times the virtual sync timer queue was considered.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualSyncRunRestart,         STAMTYPE_COUNTER,   "/TM/VirtualSync/Run/Restarts",         STAMUNIT_OCCURENCES,    "Times the clock was restarted after a run.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualSyncRunStop,            STAMTYPE_COUNTER,   "/TM/VirtualSync/Run/Stop",             STAMUNIT_OCCURENCES,    "Times the clock was stopped when calculating the current time before examining the timers.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualSyncRunStoppedAlready,  STAMTYPE_COUNTER,   "/TM/VirtualSync/Run/StoppedAlready",   STAMUNIT_OCCURENCES,    "Times the clock was already stopped elsewhere (TMVirtualSyncGet).");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualSyncRunSlack,           STAMTYPE_PROFILE,   "/TM/VirtualSync/Run/Slack",            STAMUNIT_NS_PER_OCCURENCE, "The scheduling slack. (Catch-up handed out when running timers.)");
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->tm.s.aVirtualSyncCatchUpPeriods); i++)
    {
        STAMR3RegisterF(pVM, &pVM->tm.s.aVirtualSyncCatchUpPeriods[i].u32Percentage,    STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_PCT,          "The catch-up percentage.",         "/TM/VirtualSync/Periods/%u", i);
        STAMR3RegisterF(pVM, &pVM->tm.s.aStatVirtualSyncCatchupAdjust[i],           STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,   "Times adjusted to this period.",   "/TM/VirtualSync/Periods/%u/Adjust", i);
        STAMR3RegisterF(pVM, &pVM->tm.s.aStatVirtualSyncCatchupInitial[i],          STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,   "Times started in this period.",    "/TM/VirtualSync/Periods/%u/Initial", i);
        STAMR3RegisterF(pVM, &pVM->tm.s.aVirtualSyncCatchUpPeriods[i].u64Start,         STAMTYPE_U64, STAMVISIBILITY_ALWAYS, STAMUNIT_NS,           "Start of this period (lag).",      "/TM/VirtualSync/Periods/%u/Start", i);
    }

#endif /* VBOX_WITH_STATISTICS */

    /*
     * Register info handlers.
     */
    DBGFR3InfoRegisterInternalEx(pVM, "timers",       "Dumps all timers. No arguments.",          tmR3TimerInfo,        DBGFINFO_FLAGS_RUN_ON_EMT);
    DBGFR3InfoRegisterInternalEx(pVM, "activetimers", "Dumps active all timers. No arguments.",   tmR3TimerInfoActive,  DBGFINFO_FLAGS_RUN_ON_EMT);
    DBGFR3InfoRegisterInternalEx(pVM, "clocks",       "Display the time of the various clocks.",  tmR3InfoClocks,       DBGFINFO_FLAGS_RUN_ON_EMT);

    return VINF_SUCCESS;
}


/**
 * Checks if the host CPU has a fixed TSC frequency.
 *
 * @returns true if it has, false if it hasn't.
 *
 * @remark  This test doesn't bother with very old CPUs that don't do power
 *          management or any other stuff that might influence the TSC rate.
 *          This isn't currently relevant.
 */
static bool tmR3HasFixedTSC(void)
{
    if (ASMHasCpuId())
    {
        uint32_t uEAX, uEBX, uECX, uEDX;
        ASMCpuId(0, &uEAX, &uEBX, &uECX, &uEDX);
        if (    uEAX >= 1
            &&  uEBX == X86_CPUID_VENDOR_AMD_EBX
            &&  uECX == X86_CPUID_VENDOR_AMD_ECX
            &&  uEDX == X86_CPUID_VENDOR_AMD_EDX)
        {
            /*
             * AuthenticAMD - Check for APM support and that TscInvariant is set.
             *
             * This test isn't correct with respect to fixed/non-fixed TSC and
             * older models, but this isn't relevant since the result is currently
             * only used for making a descision on AMD-V models.
             */
            ASMCpuId(0x80000000, &uEAX, &uEBX, &uECX, &uEDX);
            if (uEAX >= 0x80000007)
            {
                PSUPGLOBALINFOPAGE pGip = g_pSUPGlobalInfoPage;

                ASMCpuId(0x80000007, &uEAX, &uEBX, &uECX, &uEDX);
                if (   (uEDX & RT_BIT(8)) /* TscInvariant */
                    && pGip->u32Mode == SUPGIPMODE_SYNC_TSC /* no fixed tsc if the gip timer is in async mode */)
                    return true;
            }
        }
        else if (    uEAX >= 1
                 &&  uEBX == X86_CPUID_VENDOR_INTEL_EBX
                 &&  uECX == X86_CPUID_VENDOR_INTEL_ECX
                 &&  uEDX == X86_CPUID_VENDOR_INTEL_EDX)
        {
            /*
             * GenuineIntel - Check the model number.
             *
             * This test is lacking in the same way and for the same reasons
             * as the AMD test above.
             */
            ASMCpuId(1, &uEAX, &uEBX, &uECX, &uEDX);
            unsigned uModel  = (uEAX >> 4) & 0x0f;
            unsigned uFamily = (uEAX >> 8) & 0x0f;
            if (uFamily == 0x0f)
                uFamily += (uEAX >> 20) & 0xff;
            if (uFamily >= 0x06)
                uModel += ((uEAX >> 16) & 0x0f) << 4;
            if (    (uFamily == 0x0f /*P4*/     && uModel >= 0x03)
                ||  (uFamily == 0x06 /*P2/P3*/  && uModel >= 0x0e))
                return true;
        }
    }
    return false;
}


/**
 * Calibrate the CPU tick.
 *
 * @returns Number of ticks per second.
 */
static uint64_t tmR3CalibrateTSC(void)
{
    /*
     * Use GIP when available present.
     */
    uint64_t    u64Hz;
    PSUPGLOBALINFOPAGE pGip = g_pSUPGlobalInfoPage;
    if (    pGip
        &&  pGip->u32Magic == SUPGLOBALINFOPAGE_MAGIC)
    {
        unsigned iCpu = pGip->u32Mode != SUPGIPMODE_ASYNC_TSC ? 0 : ASMGetApicId();
        if (iCpu >= RT_ELEMENTS(pGip->aCPUs))
            AssertReleaseMsgFailed(("iCpu=%d - the ApicId is too high. send VBox.log and hardware specs!\n", iCpu));
        else
        {
            if (tmR3HasFixedTSC())
                /* Sleep a bit to get a more reliable CpuHz value. */
                RTThreadSleep(32);
            else
            {
                /* Spin for 40ms to try push up the CPU frequency and get a more reliable CpuHz value. */
                const uint64_t u64 = RTTimeMilliTS();
                while ((RTTimeMilliTS() - u64) < 40 /*ms*/)
                    /* nothing */;
            }

            pGip = g_pSUPGlobalInfoPage;
            if (    pGip
                &&  pGip->u32Magic == SUPGLOBALINFOPAGE_MAGIC
                &&  (u64Hz = pGip->aCPUs[iCpu].u64CpuHz)
                &&  u64Hz != ~(uint64_t)0)
                return u64Hz;
        }
    }

    /* call this once first to make sure it's initialized. */
    RTTimeNanoTS();

    /*
     * Yield the CPU to increase our chances of getting
     * a correct value.
     */
    RTThreadYield();                    /* Try avoid interruptions between TSC and NanoTS samplings. */
    static const unsigned   s_auSleep[5] = { 50, 30, 30, 40, 40 };
    uint64_t                au64Samples[5];
    unsigned                i;
    for (i = 0; i < ELEMENTS(au64Samples); i++)
    {
        unsigned    cMillies;
        int         cTries   = 5;
        uint64_t    u64Start = ASMReadTSC();
        uint64_t    u64End;
        uint64_t    StartTS  = RTTimeNanoTS();
        uint64_t    EndTS;
        do
        {
            RTThreadSleep(s_auSleep[i]);
            u64End = ASMReadTSC();
            EndTS  = RTTimeNanoTS();
            cMillies = (unsigned)((EndTS - StartTS + 500000) / 1000000);
        } while (   cMillies == 0       /* the sleep may be interrupted... */
                 || (cMillies < 20 && --cTries > 0));
        uint64_t    u64Diff = u64End - u64Start;

        au64Samples[i] = (u64Diff * 1000) / cMillies;
        AssertMsg(cTries > 0, ("cMillies=%d i=%d\n", cMillies, i));
    }

    /*
     * Discard the highest and lowest results and calculate the average.
     */
    unsigned iHigh = 0;
    unsigned iLow = 0;
    for (i = 1; i < ELEMENTS(au64Samples); i++)
    {
        if (au64Samples[i] < au64Samples[iLow])
            iLow = i;
        if (au64Samples[i] > au64Samples[iHigh])
            iHigh = i;
    }
    au64Samples[iLow] = 0;
    au64Samples[iHigh] = 0;

    u64Hz = au64Samples[0];
    for (i = 1; i < ELEMENTS(au64Samples); i++)
        u64Hz += au64Samples[i];
    u64Hz /= ELEMENTS(au64Samples) - 2;

    return u64Hz;
}


/**
 * Finalizes the TM initialization.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMR3DECL(int) TMR3InitFinalize(PVM pVM)
{
    int rc;

    rc = PDMR3GetSymbolGCLazy(pVM, NULL, "tmVirtualNanoTSBad",          &pVM->tm.s.VirtualGetRawDataGC.pfnBad);
    AssertRCReturn(rc, rc);
    rc = PDMR3GetSymbolGCLazy(pVM, NULL, "tmVirtualNanoTSRediscover",   &pVM->tm.s.VirtualGetRawDataGC.pfnRediscover);
    AssertRCReturn(rc, rc);
    if (pVM->tm.s.pfnVirtualGetRawR3       == RTTimeNanoTSLFenceSync)
        rc = PDMR3GetSymbolGCLazy(pVM, NULL, "RTTimeNanoTSLFenceSync",  &pVM->tm.s.pfnVirtualGetRawGC);
    else if (pVM->tm.s.pfnVirtualGetRawR3  == RTTimeNanoTSLFenceAsync)
        rc = PDMR3GetSymbolGCLazy(pVM, NULL, "RTTimeNanoTSLFenceAsync", &pVM->tm.s.pfnVirtualGetRawGC);
    else if (pVM->tm.s.pfnVirtualGetRawR3  == RTTimeNanoTSLegacySync)
        rc = PDMR3GetSymbolGCLazy(pVM, NULL, "RTTimeNanoTSLegacySync",  &pVM->tm.s.pfnVirtualGetRawGC);
    else if (pVM->tm.s.pfnVirtualGetRawR3  == RTTimeNanoTSLegacyAsync)
        rc = PDMR3GetSymbolGCLazy(pVM, NULL, "RTTimeNanoTSLegacyAsync", &pVM->tm.s.pfnVirtualGetRawGC);
    else
        AssertFatalFailed();
    AssertRCReturn(rc, rc);

    rc = PDMR3GetSymbolR0Lazy(pVM, NULL, "tmVirtualNanoTSBad",          &pVM->tm.s.VirtualGetRawDataR0.pfnBad);
    AssertRCReturn(rc, rc);
    rc = PDMR3GetSymbolR0Lazy(pVM, NULL, "tmVirtualNanoTSRediscover",   &pVM->tm.s.VirtualGetRawDataR0.pfnRediscover);
    AssertRCReturn(rc, rc);
    if (pVM->tm.s.pfnVirtualGetRawR3       == RTTimeNanoTSLFenceSync)
        rc = PDMR3GetSymbolR0Lazy(pVM, NULL, "RTTimeNanoTSLFenceSync",  &pVM->tm.s.pfnVirtualGetRawR0);
    else if (pVM->tm.s.pfnVirtualGetRawR3  == RTTimeNanoTSLFenceAsync)
        rc = PDMR3GetSymbolR0Lazy(pVM, NULL, "RTTimeNanoTSLFenceAsync", &pVM->tm.s.pfnVirtualGetRawR0);
    else if (pVM->tm.s.pfnVirtualGetRawR3  == RTTimeNanoTSLegacySync)
        rc = PDMR3GetSymbolR0Lazy(pVM, NULL, "RTTimeNanoTSLegacySync",  &pVM->tm.s.pfnVirtualGetRawR0);
    else if (pVM->tm.s.pfnVirtualGetRawR3  == RTTimeNanoTSLegacyAsync)
        rc = PDMR3GetSymbolR0Lazy(pVM, NULL, "RTTimeNanoTSLegacyAsync", &pVM->tm.s.pfnVirtualGetRawR0);
    else
        AssertFatalFailed();
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The VM.
 * @param   offDelta    Relocation delta relative to old location.
 */
TMR3DECL(void) TMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    int rc;
    LogFlow(("TMR3Relocate\n"));

    pVM->tm.s.pvGIPGC = MMHyperR3ToGC(pVM, pVM->tm.s.pvGIPR3);
    pVM->tm.s.paTimerQueuesGC = MMHyperR3ToGC(pVM, pVM->tm.s.paTimerQueuesR3);
    pVM->tm.s.paTimerQueuesR0 = MMHyperR3ToR0(pVM, pVM->tm.s.paTimerQueuesR3);

    pVM->tm.s.VirtualGetRawDataGC.pu64Prev = MMHyperR3ToGC(pVM, (void *)&pVM->tm.s.u64VirtualRawPrev);
    AssertFatal(pVM->tm.s.VirtualGetRawDataGC.pu64Prev);
    rc = PDMR3GetSymbolGCLazy(pVM, NULL, "tmVirtualNanoTSBad",          &pVM->tm.s.VirtualGetRawDataGC.pfnBad);
    AssertFatalRC(rc);
    rc = PDMR3GetSymbolGCLazy(pVM, NULL, "tmVirtualNanoTSRediscover",   &pVM->tm.s.VirtualGetRawDataGC.pfnRediscover);
    AssertFatalRC(rc);

    if (pVM->tm.s.pfnVirtualGetRawR3       == RTTimeNanoTSLFenceSync)
        rc = PDMR3GetSymbolGCLazy(pVM, NULL, "RTTimeNanoTSLFenceSync",  &pVM->tm.s.pfnVirtualGetRawGC);
    else if (pVM->tm.s.pfnVirtualGetRawR3  == RTTimeNanoTSLFenceAsync)
        rc = PDMR3GetSymbolGCLazy(pVM, NULL, "RTTimeNanoTSLFenceAsync", &pVM->tm.s.pfnVirtualGetRawGC);
    else if (pVM->tm.s.pfnVirtualGetRawR3  == RTTimeNanoTSLegacySync)
        rc = PDMR3GetSymbolGCLazy(pVM, NULL, "RTTimeNanoTSLegacySync",  &pVM->tm.s.pfnVirtualGetRawGC);
    else if (pVM->tm.s.pfnVirtualGetRawR3  == RTTimeNanoTSLegacyAsync)
        rc = PDMR3GetSymbolGCLazy(pVM, NULL, "RTTimeNanoTSLegacyAsync", &pVM->tm.s.pfnVirtualGetRawGC);
    else
        AssertFatalFailed();
    AssertFatalRC(rc);

    /*
     * Iterate the timers updating the pVMGC pointers.
     */
    for (PTMTIMER pTimer = pVM->tm.s.pCreated; pTimer; pTimer = pTimer->pBigNext)
    {
        pTimer->pVMGC = pVM->pVMGC;
        pTimer->pVMR0 = pVM->pVMR0;
    }
}


/**
 * Terminates the TM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMR3DECL(int) TMR3Term(PVM pVM)
{
    AssertMsg(pVM->tm.s.offVM, ("bad init order!\n"));
    if (pVM->tm.s.pTimer)
    {
        int rc = RTTimerDestroy(pVM->tm.s.pTimer);
        AssertRC(rc);
        pVM->tm.s.pTimer = NULL;
    }

    return VINF_SUCCESS;
}


/**
 * The VM is being reset.
 *
 * For the TM component this means that a rescheduling is preformed,
 * the FF is cleared and but without running the queues. We'll have to
 * check if this makes sense or not, but it seems like a good idea now....
 *
 * @param   pVM     VM handle.
 */
TMR3DECL(void) TMR3Reset(PVM pVM)
{
    LogFlow(("TMR3Reset:\n"));
    VM_ASSERT_EMT(pVM);

    /*
     * Abort any pending catch up.
     * This isn't perfect,
     */
    if (pVM->tm.s.fVirtualSyncCatchUp)
    {
        const uint64_t offVirtualNow = TMVirtualGetEx(pVM, false /* don't check timers */);
        const uint64_t offVirtualSyncNow = TMVirtualSyncGetEx(pVM, false /* don't check timers */);
        if (pVM->tm.s.fVirtualSyncCatchUp)
        {
            STAM_PROFILE_ADV_STOP(&pVM->tm.s.StatVirtualSyncCatchup, c);

            const uint64_t offOld = pVM->tm.s.offVirtualSyncGivenUp;
            const uint64_t offNew = offVirtualNow - offVirtualSyncNow;
            Assert(offOld <= offNew);
            ASMAtomicXchgU64((uint64_t volatile *)&pVM->tm.s.offVirtualSyncGivenUp, offNew);
            ASMAtomicXchgU64((uint64_t volatile *)&pVM->tm.s.offVirtualSync, offNew);
            ASMAtomicXchgBool(&pVM->tm.s.fVirtualSyncCatchUp, false);
            LogRel(("TM: Aborting catch-up attempt on reset with a %RU64 ns lag on reset; new total: %RU64 ns\n", offNew - offOld, offNew));
        }
    }

    /*
     * Process the queues.
     */
    for (int i = 0; i < TMCLOCK_MAX; i++)
        tmTimerQueueSchedule(pVM, &pVM->tm.s.paTimerQueuesR3[i]);
#ifdef VBOX_STRICT
    tmTimerQueuesSanityChecks(pVM, "TMR3Reset");
#endif
    VM_FF_CLEAR(pVM, VM_FF_TIMER);
}


/**
 * Resolve a builtin GC symbol.
 * Called by PDM when loading or relocating GC modules.
 *
 * @returns VBox status
 * @param   pVM         VM Handle.
 * @param   pszSymbol   Symbol to resolv
 * @param   pGCPtrValue Where to store the symbol value.
 * @remark  This has to work before TMR3Relocate() is called.
 */
TMR3DECL(int) TMR3GetImportGC(PVM pVM, const char *pszSymbol, PRTGCPTR pGCPtrValue)
{
    if (!strcmp(pszSymbol, "g_pSUPGlobalInfoPage"))
        *pGCPtrValue = MMHyperHC2GC(pVM, &pVM->tm.s.pvGIPGC);
    //else if (..)
    else
        return VERR_SYMBOL_NOT_FOUND;
    return VINF_SUCCESS;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) tmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    LogFlow(("tmR3Save:\n"));
    Assert(!pVM->tm.s.fTSCTicking);
    Assert(!pVM->tm.s.fVirtualTicking);
    Assert(!pVM->tm.s.fVirtualSyncTicking);

    /*
     * Save the virtual clocks.
     */
    /* the virtual clock. */
    SSMR3PutU64(pSSM, TMCLOCK_FREQ_VIRTUAL);
    SSMR3PutU64(pSSM, pVM->tm.s.u64Virtual);

    /* the virtual timer synchronous clock. */
    SSMR3PutU64(pSSM, pVM->tm.s.u64VirtualSync);
    SSMR3PutU64(pSSM, pVM->tm.s.offVirtualSync);
    SSMR3PutU64(pSSM, pVM->tm.s.offVirtualSyncGivenUp);
    SSMR3PutU64(pSSM, pVM->tm.s.u64VirtualSyncCatchUpPrev);
    SSMR3PutBool(pSSM, pVM->tm.s.fVirtualSyncCatchUp);

    /* real time clock */
    SSMR3PutU64(pSSM, TMCLOCK_FREQ_REAL);

    /* the cpu tick clock. */
    SSMR3PutU64(pSSM, TMCpuTickGet(pVM));
    return SSMR3PutU64(pSSM, pVM->tm.s.cTSCTicksPerSecond);
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   u32Version      Data layout version.
 */
static DECLCALLBACK(int) tmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    LogFlow(("tmR3Load:\n"));
    Assert(!pVM->tm.s.fTSCTicking);
    Assert(!pVM->tm.s.fVirtualTicking);
    Assert(!pVM->tm.s.fVirtualSyncTicking);

    /*
     * Validate version.
     */
    if (u32Version != TM_SAVED_STATE_VERSION)
    {
        Log(("tmR3Load: Invalid version u32Version=%d!\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Load the virtual clock.
     */
    pVM->tm.s.fVirtualTicking = false;
    /* the virtual clock. */
    uint64_t u64Hz;
    int rc = SSMR3GetU64(pSSM, &u64Hz);
    if (VBOX_FAILURE(rc))
        return rc;
    if (u64Hz != TMCLOCK_FREQ_VIRTUAL)
    {
        AssertMsgFailed(("The virtual clock frequency differs! Saved: %RU64 Binary: %RU64\n",
                         u64Hz, TMCLOCK_FREQ_VIRTUAL));
        return VERR_SSM_VIRTUAL_CLOCK_HZ;
    }
    SSMR3GetU64(pSSM, &pVM->tm.s.u64Virtual);
    pVM->tm.s.u64VirtualOffset = 0;

    /* the virtual timer synchronous clock. */
    pVM->tm.s.fVirtualSyncTicking = false;
    uint64_t u64;
    SSMR3GetU64(pSSM, &u64);
    pVM->tm.s.u64VirtualSync = u64;
    SSMR3GetU64(pSSM, &u64);
    pVM->tm.s.offVirtualSync = u64;
    SSMR3GetU64(pSSM, &u64);
    pVM->tm.s.offVirtualSyncGivenUp = u64;
    SSMR3GetU64(pSSM, &u64);
    pVM->tm.s.u64VirtualSyncCatchUpPrev = u64;
    bool f;
    SSMR3GetBool(pSSM, &f);
    pVM->tm.s.fVirtualSyncCatchUp = f;

    /* the real clock  */
    rc = SSMR3GetU64(pSSM, &u64Hz);
    if (VBOX_FAILURE(rc))
        return rc;
    if (u64Hz != TMCLOCK_FREQ_REAL)
    {
        AssertMsgFailed(("The real clock frequency differs! Saved: %RU64 Binary: %RU64\n",
                         u64Hz, TMCLOCK_FREQ_REAL));
        return VERR_SSM_VIRTUAL_CLOCK_HZ; /* missleading... */
    }

    /* the cpu tick clock. */
    pVM->tm.s.fTSCTicking = false;
    SSMR3GetU64(pSSM, &pVM->tm.s.u64TSC);
    rc = SSMR3GetU64(pSSM, &u64Hz);
    if (VBOX_FAILURE(rc))
        return rc;
    if (pVM->tm.s.fTSCUseRealTSC)
        pVM->tm.s.u64TSCOffset = 0; /** @todo TSC restore stuff and HWACC. */
    else
        pVM->tm.s.cTSCTicksPerSecond = u64Hz;
    LogRel(("TM: cTSCTicksPerSecond=%#RX64 (%RU64) fTSCVirtualized=%RTbool fTSCUseRealTSC=%RTbool (state load)\n",
            pVM->tm.s.cTSCTicksPerSecond, pVM->tm.s.cTSCTicksPerSecond, pVM->tm.s.fTSCVirtualized, pVM->tm.s.fTSCUseRealTSC));

    /*
     * Make sure timers get rescheduled immediately.
     */
    VM_FF_SET(pVM, VM_FF_TIMER);

    return VINF_SUCCESS;
}


/**
 * Internal TMR3TimerCreate worker.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   enmClock    The timer clock.
 * @param   pszDesc     The timer description.
 * @param   ppTimer     Where to store the timer pointer on success.
 */
static int tmr3TimerCreate(PVM pVM, TMCLOCK enmClock, const char *pszDesc, PPTMTIMERR3 ppTimer)
{
    VM_ASSERT_EMT(pVM);

    /*
     * Allocate the timer.
     */
    PTMTIMERR3 pTimer = NULL;
    if (pVM->tm.s.pFree && VM_IS_EMT(pVM))
    {
        pTimer = pVM->tm.s.pFree;
        pVM->tm.s.pFree = pTimer->pBigNext;
        Log3(("TM: Recycling timer %p, new free head %p.\n", pTimer, pTimer->pBigNext));
    }

    if (!pTimer)
    {
        int rc = MMHyperAlloc(pVM, sizeof(*pTimer), 0, MM_TAG_TM, (void **)&pTimer);
        if (VBOX_FAILURE(rc))
            return rc;
        Log3(("TM: Allocated new timer %p\n", pTimer));
    }

    /*
     * Initialize it.
     */
    pTimer->u64Expire       = 0;
    pTimer->enmClock        = enmClock;
    pTimer->pVMR3           = pVM;
    pTimer->pVMR0           = pVM->pVMR0;
    pTimer->pVMGC           = pVM->pVMGC;
    pTimer->enmState        = TMTIMERSTATE_STOPPED;
    pTimer->offScheduleNext = 0;
    pTimer->offNext         = 0;
    pTimer->offPrev         = 0;
    pTimer->pszDesc         = pszDesc;

    /* insert into the list of created timers. */
    pTimer->pBigPrev        = NULL;
    pTimer->pBigNext        = pVM->tm.s.pCreated;
    pVM->tm.s.pCreated      = pTimer;
    if (pTimer->pBigNext)
        pTimer->pBigNext->pBigPrev = pTimer;
#ifdef VBOX_STRICT
    tmTimerQueuesSanityChecks(pVM, "tmR3TimerCreate");
#endif

    *ppTimer = pTimer;
    return VINF_SUCCESS;
}


/**
 * Creates a device timer.
 *
 * @returns VBox status.
 * @param   pVM             The VM to create the timer in.
 * @param   pDevIns         Device instance.
 * @param   enmClock        The clock to use on this timer.
 * @param   pfnCallback     Callback function.
 * @param   pszDesc         Pointer to description string which must stay around
 *                          until the timer is fully destroyed (i.e. a bit after TMTimerDestroy()).
 * @param   ppTimer         Where to store the timer on success.
 */
TMR3DECL(int) TMR3TimerCreateDevice(PVM pVM, PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMERDEV pfnCallback, const char *pszDesc, PPTMTIMERR3 ppTimer)
{
    /*
     * Allocate and init stuff.
     */
    int rc = tmr3TimerCreate(pVM, enmClock, pszDesc, ppTimer);
    if (VBOX_SUCCESS(rc))
    {
        (*ppTimer)->enmType         = TMTIMERTYPE_DEV;
        (*ppTimer)->u.Dev.pfnTimer  = pfnCallback;
        (*ppTimer)->u.Dev.pDevIns   = pDevIns;
        Log(("TM: Created device timer %p clock %d callback %p '%s'\n", (*ppTimer), enmClock, pfnCallback, pszDesc));
    }

    return rc;
}


/**
 * Creates a driver timer.
 *
 * @returns VBox status.
 * @param   pVM             The VM to create the timer in.
 * @param   pDrvIns         Driver instance.
 * @param   enmClock        The clock to use on this timer.
 * @param   pfnCallback     Callback function.
 * @param   pszDesc         Pointer to description string which must stay around
 *                          until the timer is fully destroyed (i.e. a bit after TMTimerDestroy()).
 * @param   ppTimer         Where to store the timer on success.
 */
TMR3DECL(int) TMR3TimerCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, TMCLOCK enmClock, PFNTMTIMERDRV pfnCallback, const char *pszDesc, PPTMTIMERR3 ppTimer)
{
    /*
     * Allocate and init stuff.
     */
    int rc = tmr3TimerCreate(pVM, enmClock, pszDesc, ppTimer);
    if (VBOX_SUCCESS(rc))
    {
        (*ppTimer)->enmType         = TMTIMERTYPE_DRV;
        (*ppTimer)->u.Drv.pfnTimer  = pfnCallback;
        (*ppTimer)->u.Drv.pDrvIns   = pDrvIns;
        Log(("TM: Created device timer %p clock %d callback %p '%s'\n", (*ppTimer), enmClock, pfnCallback, pszDesc));
    }

    return rc;
}


/**
 * Creates an internal timer.
 *
 * @returns VBox status.
 * @param   pVM             The VM to create the timer in.
 * @param   enmClock        The clock to use on this timer.
 * @param   pfnCallback     Callback function.
 * @param   pvUser          User argument to be passed to the callback.
 * @param   pszDesc         Pointer to description string which must stay around
 *                          until the timer is fully destroyed (i.e. a bit after TMTimerDestroy()).
 * @param   ppTimer         Where to store the timer on success.
 */
TMR3DECL(int) TMR3TimerCreateInternal(PVM pVM, TMCLOCK enmClock, PFNTMTIMERINT pfnCallback, void *pvUser, const char *pszDesc, PPTMTIMERR3 ppTimer)
{
    /*
     * Allocate and init  stuff.
     */
    PTMTIMER pTimer;
    int rc = tmr3TimerCreate(pVM, enmClock, pszDesc, &pTimer);
    if (VBOX_SUCCESS(rc))
    {
        pTimer->enmType             = TMTIMERTYPE_INTERNAL;
        pTimer->u.Internal.pfnTimer = pfnCallback;
        pTimer->u.Internal.pvUser   = pvUser;
        *ppTimer = pTimer;
        Log(("TM: Created internal timer %p clock %d callback %p '%s'\n", pTimer, enmClock, pfnCallback, pszDesc));
    }

    return rc;
}

/**
 * Creates an external timer.
 *
 * @returns Timer handle on success.
 * @returns NULL on failure.
 * @param   pVM             The VM to create the timer in.
 * @param   enmClock        The clock to use on this timer.
 * @param   pfnCallback     Callback function.
 * @param   pvUser          User argument.
 * @param   pszDesc         Pointer to description string which must stay around
 *                          until the timer is fully destroyed (i.e. a bit after TMTimerDestroy()).
 */
TMR3DECL(PTMTIMERR3) TMR3TimerCreateExternal(PVM pVM, TMCLOCK enmClock, PFNTMTIMEREXT pfnCallback, void *pvUser, const char *pszDesc)
{
    /*
     * Allocate and init stuff.
     */
    PTMTIMERR3 pTimer;
    int rc = tmr3TimerCreate(pVM, enmClock, pszDesc, &pTimer);
    if (VBOX_SUCCESS(rc))
    {
        pTimer->enmType             = TMTIMERTYPE_EXTERNAL;
        pTimer->u.External.pfnTimer = pfnCallback;
        pTimer->u.External.pvUser   = pvUser;
        Log(("TM: Created external timer %p clock %d callback %p '%s'\n", pTimer, enmClock, pfnCallback, pszDesc));
        return pTimer;
    }

    return NULL;
}


/**
 * Destroy all timers owned by a device.
 *
 * @returns VBox status.
 * @param   pVM             VM handle.
 * @param   pDevIns         Device which timers should be destroyed.
 */
TMR3DECL(int) TMR3TimerDestroyDevice(PVM pVM, PPDMDEVINS pDevIns)
{
    LogFlow(("TMR3TimerDestroyDevice: pDevIns=%p\n", pDevIns));
    if (!pDevIns)
        return VERR_INVALID_PARAMETER;

    PTMTIMER    pCur = pVM->tm.s.pCreated;
    while (pCur)
    {
        PTMTIMER pDestroy = pCur;
        pCur = pDestroy->pBigNext;
        if (    pDestroy->enmType == TMTIMERTYPE_DEV
            &&  pDestroy->u.Dev.pDevIns == pDevIns)
        {
            int rc = TMTimerDestroy(pDestroy);
            AssertRC(rc);
        }
    }
    LogFlow(("TMR3TimerDestroyDevice: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Destroy all timers owned by a driver.
 *
 * @returns VBox status.
 * @param   pVM             VM handle.
 * @param   pDrvIns         Driver which timers should be destroyed.
 */
TMR3DECL(int) TMR3TimerDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns)
{
    LogFlow(("TMR3TimerDestroyDriver: pDrvIns=%p\n", pDrvIns));
    if (!pDrvIns)
        return VERR_INVALID_PARAMETER;

    PTMTIMER    pCur = pVM->tm.s.pCreated;
    while (pCur)
    {
        PTMTIMER pDestroy = pCur;
        pCur = pDestroy->pBigNext;
        if (    pDestroy->enmType == TMTIMERTYPE_DRV
            &&  pDestroy->u.Drv.pDrvIns == pDrvIns)
        {
            int rc = TMTimerDestroy(pDestroy);
            AssertRC(rc);
        }
    }
    LogFlow(("TMR3TimerDestroyDriver: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Checks if the sync queue has one or more expired timers.
 *
 * @returns true / false.
 *
 * @param   pVM         The VM handle.
 * @param   enmClock    The queue.
 */
DECLINLINE(bool) tmR3HasExpiredTimer(PVM pVM, TMCLOCK enmClock)
{
    const uint64_t u64Expire = pVM->tm.s.CTXALLSUFF(paTimerQueues)[enmClock].u64Expire;
    return u64Expire != INT64_MAX && u64Expire <= tmClock(pVM, enmClock);
}


/**
 * Checks for expired timers in all the queues.
 *
 * @returns true / false.
 * @param   pVM         The VM handle.
 */
DECLINLINE(bool) tmR3AnyExpiredTimers(PVM pVM)
{
    /*
     * Combine the time calculation for the first two since we're not on EMT
     * TMVirtualSyncGet only permits EMT.
     */
    uint64_t u64Now = TMVirtualGet(pVM);
    if (pVM->tm.s.CTXALLSUFF(paTimerQueues)[TMCLOCK_VIRTUAL].u64Expire <= u64Now)
        return true;
    u64Now = pVM->tm.s.fVirtualSyncTicking
           ? u64Now - pVM->tm.s.offVirtualSync
           : pVM->tm.s.u64VirtualSync;
    if (pVM->tm.s.CTXALLSUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire <= u64Now)
        return true;

    /*
     * The remaining timers.
     */
    if (tmR3HasExpiredTimer(pVM, TMCLOCK_REAL))
        return true;
    if (tmR3HasExpiredTimer(pVM, TMCLOCK_TSC))
        return true;
    return false;
}


/**
 * Schedulation timer callback.
 *
 * @param   pTimer      Timer handle.
 * @param   pvUser      VM handle.
 * @thread  Timer thread.
 *
 * @remark  We cannot do the scheduling and queues running from a timer handler
 *          since it's not executing in EMT, and even if it was it would be async
 *          and we wouldn't know the state of the affairs.
 *          So, we'll just raise the timer FF and force any REM execution to exit.
 */
static DECLCALLBACK(void) tmR3TimerCallback(PRTTIMER pTimer, void *pvUser)
{
    PVM pVM = (PVM)pvUser;
    AssertCompile(TMCLOCK_MAX == 4);
#ifdef DEBUG_Sander /* very annoying, keep it private. */
    if (VM_FF_ISSET(pVM, VM_FF_TIMER))
        Log(("tmR3TimerCallback: timer event still pending!!\n"));
#endif
    if (    !VM_FF_ISSET(pVM, VM_FF_TIMER)
        &&  (   pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL_SYNC].offSchedule
            ||  pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL].offSchedule
            ||  pVM->tm.s.paTimerQueuesR3[TMCLOCK_REAL].offSchedule
            ||  pVM->tm.s.paTimerQueuesR3[TMCLOCK_TSC].offSchedule
            ||  tmR3AnyExpiredTimers(pVM)
            )
        && !VM_FF_ISSET(pVM, VM_FF_TIMER)
       )
    {
        VM_FF_SET(pVM, VM_FF_TIMER);
        REMR3NotifyTimerPending(pVM);
        VMR3NotifyFF(pVM, true);
        STAM_COUNTER_INC(&pVM->tm.s.StatTimerCallbackSetFF);
    }
}


/**
 * Schedules and runs any pending timers.
 *
 * This is normally called from a forced action handler in EMT.
 *
 * @param   pVM             The VM to run the timers for.
 */
TMR3DECL(void) TMR3TimerQueuesDo(PVM pVM)
{
    STAM_PROFILE_START(&pVM->tm.s.StatDoQueues, a);
    Log2(("TMR3TimerQueuesDo:\n"));

    /*
     * Process the queues.
     */
    AssertCompile(TMCLOCK_MAX == 4);

    /* TMCLOCK_VIRTUAL_SYNC */
    STAM_PROFILE_ADV_START(&pVM->tm.s.StatDoQueuesSchedule, s1);
    tmTimerQueueSchedule(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL_SYNC]);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesSchedule, s1);
    STAM_PROFILE_ADV_START(&pVM->tm.s.StatDoQueuesRun, r1);
    tmR3TimerQueueRunVirtualSync(pVM);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesRun, r1);

    /* TMCLOCK_VIRTUAL */
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesSchedule, s1);
    tmTimerQueueSchedule(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL]);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesSchedule, s2);
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesRun, r1);
    tmR3TimerQueueRun(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL]);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesRun, r2);

#if 0 /** @todo if ever used, remove this and fix the stam prefixes on TMCLOCK_REAL below. */
    /* TMCLOCK_TSC */
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesSchedule, s2);
    tmTimerQueueSchedule(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_TSC]);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesSchedule, s3);
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesRun, r2);
    tmR3TimerQueueRun(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_TSC]);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesRun, r3);
#endif

    /* TMCLOCK_REAL */
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesSchedule, s2);
    tmTimerQueueSchedule(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_REAL]);
    STAM_PROFILE_ADV_STOP(&pVM->tm.s.StatDoQueuesSchedule, s3);
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesRun, r2);
    tmR3TimerQueueRun(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_REAL]);
    STAM_PROFILE_ADV_STOP(&pVM->tm.s.StatDoQueuesRun, r3);

    /* done. */
    VM_FF_CLEAR(pVM, VM_FF_TIMER);

#ifdef VBOX_STRICT
    /* check that we didn't screwup. */
    tmTimerQueuesSanityChecks(pVM, "TMR3TimerQueuesDo");
#endif

    Log2(("TMR3TimerQueuesDo: returns void\n"));
    STAM_PROFILE_STOP(&pVM->tm.s.StatDoQueues, a);
}


/**
 * Schedules and runs any pending times in the specified queue.
 *
 * This is normally called from a forced action handler in EMT.
 *
 * @param   pVM             The VM to run the timers for.
 * @param   pQueue          The queue to run.
 */
static void tmR3TimerQueueRun(PVM pVM, PTMTIMERQUEUE pQueue)
{
    VM_ASSERT_EMT(pVM);

    /*
     * Run timers.
     *
     * We check the clock once and run all timers which are ACTIVE
     * and have an expire time less or equal to the time we read.
     *
     * N.B. A generic unlink must be applied since other threads
     *      are allowed to mess with any active timer at any time.
     *      However, we only allow EMT to handle EXPIRED_PENDING
     *      timers, thus enabling the timer handler function to
     *      arm the timer again.
     */
    PTMTIMER pNext = TMTIMER_GET_HEAD(pQueue);
    if (!pNext)
        return;
    const uint64_t u64Now = tmClock(pVM, pQueue->enmClock);
    while (pNext && pNext->u64Expire <= u64Now)
    {
        PTMTIMER pTimer = pNext;
        pNext = TMTIMER_GET_NEXT(pTimer);
        Log2(("tmR3TimerQueueRun: pTimer=%p:{.enmState=%s, .enmClock=%d, .enmType=%d, u64Expire=%llx (now=%llx) .pszDesc=%s}\n",
              pTimer, tmTimerState(pTimer->enmState), pTimer->enmClock, pTimer->enmType, pTimer->u64Expire, u64Now, pTimer->pszDesc));
        bool fRc;
        TM_TRY_SET_STATE(pTimer, TMTIMERSTATE_EXPIRED, TMTIMERSTATE_ACTIVE, fRc);
        if (fRc)
        {
            Assert(!pTimer->offScheduleNext); /* this can trigger falsely */

            /* unlink */
            const PTMTIMER pPrev = TMTIMER_GET_PREV(pTimer);
            if (pPrev)
                TMTIMER_SET_NEXT(pPrev, pNext);
            else
            {
                TMTIMER_SET_HEAD(pQueue, pNext);
                pQueue->u64Expire = pNext ? pNext->u64Expire : INT64_MAX;
            }
            if (pNext)
                TMTIMER_SET_PREV(pNext, pPrev);
            pTimer->offNext = 0;
            pTimer->offPrev = 0;


            /* fire */
            switch (pTimer->enmType)
            {
                case TMTIMERTYPE_DEV:       pTimer->u.Dev.pfnTimer(pTimer->u.Dev.pDevIns, pTimer); break;
                case TMTIMERTYPE_DRV:       pTimer->u.Drv.pfnTimer(pTimer->u.Drv.pDrvIns, pTimer); break;
                case TMTIMERTYPE_INTERNAL:  pTimer->u.Internal.pfnTimer(pVM, pTimer, pTimer->u.Internal.pvUser); break;
                case TMTIMERTYPE_EXTERNAL:  pTimer->u.External.pfnTimer(pTimer->u.External.pvUser); break;
                default:
                    AssertMsgFailed(("Invalid timer type %d (%s)\n", pTimer->enmType, pTimer->pszDesc));
                    break;
            }

            /* change the state if it wasn't changed already in the handler. */
            TM_TRY_SET_STATE(pTimer, TMTIMERSTATE_STOPPED, TMTIMERSTATE_EXPIRED, fRc);
            Log2(("tmR3TimerQueueRun: new state %s\n", tmTimerState(pTimer->enmState)));
        }
    } /* run loop */
}


/**
 * Schedules and runs any pending times in the timer queue for the
 * synchronous virtual clock.
 *
 * This scheduling is a bit different from the other queues as it need
 * to implement the special requirements of the timer synchronous virtual
 * clock, thus this 2nd queue run funcion.
 *
 * @param   pVM             The VM to run the timers for.
 */
static void tmR3TimerQueueRunVirtualSync(PVM pVM)
{
    PTMTIMERQUEUE const pQueue = &pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL_SYNC];
    VM_ASSERT_EMT(pVM);

    /*
     * Any timers?
     */
    PTMTIMER pNext = TMTIMER_GET_HEAD(pQueue);
    if (RT_UNLIKELY(!pNext))
    {
        Assert(pVM->tm.s.fVirtualSyncTicking || !pVM->tm.s.fVirtualTicking);
        return;
    }
    STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncRun);

    /*
     * Calculate the time frame for which we will dispatch timers.
     *
     * We use a time frame ranging from the current sync time (which is most likely the
     * same as the head timer) and some configurable period (100000ns) up towards the
     * current virtual time. This period might also need to be restricted by the catch-up
     * rate so frequent calls to this function won't accelerate the time too much, however
     * this will be implemented at a later point if neccessary.
     *
     * Without this frame we would 1) having to run timers much more frequently
     * and 2) lag behind at a steady rate.
     */
    const uint64_t u64VirtualNow = TMVirtualGetEx(pVM, false /* don't check timers */);
    uint64_t u64Now;
    if (!pVM->tm.s.fVirtualSyncTicking)
    {
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncRunStoppedAlready);
        u64Now = pVM->tm.s.u64VirtualSync;
        Assert(u64Now <= pNext->u64Expire);
    }
    else
    {
        /* Calc 'now'. (update order doesn't really matter here) */
        uint64_t off = pVM->tm.s.offVirtualSync;
        if (pVM->tm.s.fVirtualSyncCatchUp)
        {
            uint64_t u64Delta = u64VirtualNow - pVM->tm.s.u64VirtualSyncCatchUpPrev;
            if (RT_LIKELY(!(u64Delta >> 32)))
            {
                uint64_t u64Sub = ASMMultU64ByU32DivByU32(u64Delta, pVM->tm.s.u32VirtualSyncCatchUpPercentage, 100);
                if (off > u64Sub + pVM->tm.s.offVirtualSyncGivenUp)
                {
                    off -= u64Sub;
                    Log4(("TM: %RU64/%RU64: sub %RU64 (run)\n", u64VirtualNow - off, off - pVM->tm.s.offVirtualSyncGivenUp, u64Sub));
                }
                else
                {
                    STAM_PROFILE_ADV_STOP(&pVM->tm.s.StatVirtualSyncCatchup, c);
                    ASMAtomicXchgBool(&pVM->tm.s.fVirtualSyncCatchUp, false);
                    off = pVM->tm.s.offVirtualSyncGivenUp;
                    Log4(("TM: %RU64/0: caught up (run)\n", u64VirtualNow));
                }
            }
            ASMAtomicXchgU64(&pVM->tm.s.offVirtualSync, off);
            pVM->tm.s.u64VirtualSyncCatchUpPrev = u64VirtualNow;
        }
        u64Now = u64VirtualNow - off;

        /* Check if stopped by expired timer. */
        if (u64Now >= pNext->u64Expire)
        {
            STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncRunStop);
            u64Now = pNext->u64Expire;
            ASMAtomicXchgU64(&pVM->tm.s.u64VirtualSync, u64Now);
            ASMAtomicXchgBool(&pVM->tm.s.fVirtualSyncTicking, false);
            Log4(("TM: %RU64/%RU64: exp tmr (run)\n", u64Now, u64VirtualNow - u64Now - pVM->tm.s.offVirtualSyncGivenUp));

        }
    }

    /* calc end of frame. */
    uint64_t u64Max = u64Now + pVM->tm.s.u32VirtualSyncScheduleSlack;
    if (u64Max > u64VirtualNow - pVM->tm.s.offVirtualSyncGivenUp)
        u64Max = u64VirtualNow - pVM->tm.s.offVirtualSyncGivenUp;

    /* assert sanity */
    Assert(u64Now <= u64VirtualNow - pVM->tm.s.offVirtualSyncGivenUp);
    Assert(u64Max <= u64VirtualNow - pVM->tm.s.offVirtualSyncGivenUp);
    Assert(u64Now <= u64Max);

    /*
     * Process the expired timers moving the clock along as we progress.
     */
#ifdef VBOX_STRICT
    uint64_t u64Prev = u64Now; NOREF(u64Prev);
#endif
    while (pNext && pNext->u64Expire <= u64Max)
    {
        PTMTIMER pTimer = pNext;
        pNext = TMTIMER_GET_NEXT(pTimer);
        Log2(("tmR3TimerQueueRun: pTimer=%p:{.enmState=%s, .enmClock=%d, .enmType=%d, u64Expire=%llx (now=%llx) .pszDesc=%s}\n",
              pTimer, tmTimerState(pTimer->enmState), pTimer->enmClock, pTimer->enmType, pTimer->u64Expire, u64Now, pTimer->pszDesc));
        bool fRc;
        TM_TRY_SET_STATE(pTimer, TMTIMERSTATE_EXPIRED, TMTIMERSTATE_ACTIVE, fRc);
        if (fRc)
        {
            /* unlink */
            const PTMTIMER pPrev = TMTIMER_GET_PREV(pTimer);
            if (pPrev)
                TMTIMER_SET_NEXT(pPrev, pNext);
            else
            {
                TMTIMER_SET_HEAD(pQueue, pNext);
                pQueue->u64Expire = pNext ? pNext->u64Expire : INT64_MAX;
            }
            if (pNext)
                TMTIMER_SET_PREV(pNext, pPrev);
            pTimer->offNext = 0;
            pTimer->offPrev = 0;

            /* advance the clock - don't permit timers to be out of order or armed in the 'past'. */
#ifdef VBOX_STRICT
            AssertMsg(pTimer->u64Expire >= u64Prev, ("%RU64 < %RU64 %s\n", pTimer->u64Expire, u64Prev, pTimer->pszDesc));
            u64Prev = pTimer->u64Expire;
#endif
            ASMAtomicXchgSize(&pVM->tm.s.fVirtualSyncTicking, false);
            ASMAtomicXchgU64(&pVM->tm.s.u64VirtualSync, pTimer->u64Expire);

            /* fire */
            switch (pTimer->enmType)
            {
                case TMTIMERTYPE_DEV:       pTimer->u.Dev.pfnTimer(pTimer->u.Dev.pDevIns, pTimer); break;
                case TMTIMERTYPE_DRV:       pTimer->u.Drv.pfnTimer(pTimer->u.Drv.pDrvIns, pTimer); break;
                case TMTIMERTYPE_INTERNAL:  pTimer->u.Internal.pfnTimer(pVM, pTimer, pTimer->u.Internal.pvUser); break;
                case TMTIMERTYPE_EXTERNAL:  pTimer->u.External.pfnTimer(pTimer->u.External.pvUser); break;
                default:
                    AssertMsgFailed(("Invalid timer type %d (%s)\n", pTimer->enmType, pTimer->pszDesc));
                    break;
            }

            /* change the state if it wasn't changed already in the handler. */
            TM_TRY_SET_STATE(pTimer, TMTIMERSTATE_STOPPED, TMTIMERSTATE_EXPIRED, fRc);
            Log2(("tmR3TimerQueueRun: new state %s\n", tmTimerState(pTimer->enmState)));
        }
    } /* run loop */

    /*
     * Restart the clock if it was stopped to serve any timers,
     * and start/adjust catch-up if necessary.
     */
    if (    !pVM->tm.s.fVirtualSyncTicking
        &&  pVM->tm.s.fVirtualTicking)
    {
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncRunRestart);

        /* calc the slack we've handed out. */
        const uint64_t u64VirtualNow2 = TMVirtualGetEx(pVM, false /* don't check timers */);
        Assert(u64VirtualNow2 >= u64VirtualNow);
        AssertMsg(pVM->tm.s.u64VirtualSync >= u64Now, ("%RU64 < %RU64\n", pVM->tm.s.u64VirtualSync, u64Now));
        const uint64_t offSlack = pVM->tm.s.u64VirtualSync - u64Now;
        STAM_STATS({
            if (offSlack)
            {
                PSTAMPROFILE p = &pVM->tm.s.StatVirtualSyncRunSlack;
                p->cPeriods++;
                p->cTicks += offSlack;
                if (p->cTicksMax < offSlack) p->cTicksMax = offSlack;
                if (p->cTicksMin > offSlack) p->cTicksMin = offSlack;
            }
        });

        /* Let the time run a little bit while we were busy running timers(?). */
        uint64_t u64Elapsed;
#define MAX_ELAPSED 30000 /* ns */
        if (offSlack > MAX_ELAPSED)
            u64Elapsed = 0;
        else
        {
            u64Elapsed = u64VirtualNow2 - u64VirtualNow;
            if (u64Elapsed > MAX_ELAPSED)
                u64Elapsed = MAX_ELAPSED;
            u64Elapsed = u64Elapsed > offSlack ? u64Elapsed - offSlack : 0;
        }
#undef MAX_ELAPSED

        /* Calc the current offset. */
        uint64_t offNew = u64VirtualNow2 - pVM->tm.s.u64VirtualSync - u64Elapsed;
        Assert(!(offNew & RT_BIT_64(63)));
        uint64_t offLag = offNew - pVM->tm.s.offVirtualSyncGivenUp;
        Assert(!(offLag & RT_BIT_64(63)));

        /*
         * Deal with starting, adjusting and stopping catchup.
         */
        if (pVM->tm.s.fVirtualSyncCatchUp)
        {
            if (offLag <= pVM->tm.s.u64VirtualSyncCatchUpStopThreshold)
            {
                /* stop */
                STAM_PROFILE_ADV_STOP(&pVM->tm.s.StatVirtualSyncCatchup, c);
                ASMAtomicXchgBool(&pVM->tm.s.fVirtualSyncCatchUp, false);
                Log4(("TM: %RU64/%RU64: caught up\n", u64VirtualNow2 - offNew, offLag));
            }
            else if (offLag <= pVM->tm.s.u64VirtualSyncCatchUpGiveUpThreshold)
            {
                /* adjust */
                unsigned i = 0;
                while (     i + 1 < RT_ELEMENTS(pVM->tm.s.aVirtualSyncCatchUpPeriods)
                       &&   offLag >= pVM->tm.s.aVirtualSyncCatchUpPeriods[i + 1].u64Start)
                    i++;
                if (pVM->tm.s.u32VirtualSyncCatchUpPercentage < pVM->tm.s.aVirtualSyncCatchUpPeriods[i].u32Percentage)
                {
                    STAM_COUNTER_INC(&pVM->tm.s.aStatVirtualSyncCatchupAdjust[i]);
                    ASMAtomicXchgU32(&pVM->tm.s.u32VirtualSyncCatchUpPercentage, pVM->tm.s.aVirtualSyncCatchUpPeriods[i].u32Percentage);
                    Log4(("TM: %RU64/%RU64: adj %u%%\n", u64VirtualNow2 - offNew, offLag, pVM->tm.s.u32VirtualSyncCatchUpPercentage));
                }
                pVM->tm.s.u64VirtualSyncCatchUpPrev = u64VirtualNow2;
            }
            else
            {
                /* give up */
                STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGiveUp);
                STAM_PROFILE_ADV_STOP(&pVM->tm.s.StatVirtualSyncCatchup, c);
                ASMAtomicXchgU64((uint64_t volatile *)&pVM->tm.s.offVirtualSyncGivenUp, offNew);
                ASMAtomicXchgBool(&pVM->tm.s.fVirtualSyncCatchUp, false);
                Log4(("TM: %RU64/%RU64: give up %u%%\n", u64VirtualNow2 - offNew, offLag, pVM->tm.s.u32VirtualSyncCatchUpPercentage));
                LogRel(("TM: Giving up catch-up attempt at a %RU64 ns lag; new total: %RU64 ns\n", offLag, offNew));
            }
        }
        else if (offLag >= pVM->tm.s.aVirtualSyncCatchUpPeriods[0].u64Start)
        {
            if (offLag <= pVM->tm.s.u64VirtualSyncCatchUpGiveUpThreshold)
            {
                /* start */
                STAM_PROFILE_ADV_START(&pVM->tm.s.StatVirtualSyncCatchup, c);
                unsigned i = 0;
                while (     i + 1 < RT_ELEMENTS(pVM->tm.s.aVirtualSyncCatchUpPeriods)
                       &&   offLag >= pVM->tm.s.aVirtualSyncCatchUpPeriods[i + 1].u64Start)
                    i++;
                STAM_COUNTER_INC(&pVM->tm.s.aStatVirtualSyncCatchupInitial[i]);
                ASMAtomicXchgU32(&pVM->tm.s.u32VirtualSyncCatchUpPercentage, pVM->tm.s.aVirtualSyncCatchUpPeriods[i].u32Percentage);
                ASMAtomicXchgBool(&pVM->tm.s.fVirtualSyncCatchUp, true);
                Log4(("TM: %RU64/%RU64: catch-up %u%%\n", u64VirtualNow2 - offNew, offLag, pVM->tm.s.u32VirtualSyncCatchUpPercentage));
            }
            else
            {
                /* don't bother */
                STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGiveUpBeforeStarting);
                ASMAtomicXchgU64((uint64_t volatile *)&pVM->tm.s.offVirtualSyncGivenUp, offNew);
                Log4(("TM: %RU64/%RU64: give up\n", u64VirtualNow2 - offNew, offLag));
                LogRel(("TM: Not bothering to attempt catching up a %RU64 ns lag; new total: %RU64\n", offLag, offNew));
            }
        }

        /*
         * Update the offset and restart the clock.
         */
        Assert(!(offNew & RT_BIT_64(63)));
        ASMAtomicXchgU64(&pVM->tm.s.offVirtualSync, offNew);
        ASMAtomicXchgBool(&pVM->tm.s.fVirtualSyncTicking, true);
    }
}


/**
 * Saves the state of a timer to a saved state.
 *
 * @returns VBox status.
 * @param   pTimer          Timer to save.
 * @param   pSSM            Save State Manager handle.
 */
TMR3DECL(int) TMR3TimerSave(PTMTIMERR3 pTimer, PSSMHANDLE pSSM)
{
    LogFlow(("TMR3TimerSave: pTimer=%p:{enmState=%s, .pszDesc={%s}} pSSM=%p\n", pTimer, tmTimerState(pTimer->enmState), pTimer->pszDesc, pSSM));
    switch (pTimer->enmState)
    {
        case TMTIMERSTATE_STOPPED:
        case TMTIMERSTATE_PENDING_STOP:
        case TMTIMERSTATE_PENDING_STOP_SCHEDULE:
            return SSMR3PutU8(pSSM, (uint8_t)TMTIMERSTATE_PENDING_STOP);

        case TMTIMERSTATE_PENDING_SCHEDULE_SET_EXPIRE:
        case TMTIMERSTATE_PENDING_RESCHEDULE_SET_EXPIRE:
            AssertMsgFailed(("u64Expire is being updated! (%s)\n", pTimer->pszDesc));
            if (!RTThreadYield())
                RTThreadSleep(1);
            /* fall thru */
        case TMTIMERSTATE_ACTIVE:
        case TMTIMERSTATE_PENDING_SCHEDULE:
        case TMTIMERSTATE_PENDING_RESCHEDULE:
            SSMR3PutU8(pSSM, (uint8_t)TMTIMERSTATE_PENDING_SCHEDULE);
            return SSMR3PutU64(pSSM, pTimer->u64Expire);

        case TMTIMERSTATE_EXPIRED:
        case TMTIMERSTATE_PENDING_DESTROY:
        case TMTIMERSTATE_PENDING_STOP_DESTROY:
        case TMTIMERSTATE_FREE:
            AssertMsgFailed(("Invalid timer state %d %s (%s)\n", pTimer->enmState, tmTimerState(pTimer->enmState), pTimer->pszDesc));
            return SSMR3HandleSetStatus(pSSM, VERR_TM_INVALID_STATE);
    }

    AssertMsgFailed(("Unknown timer state %d (%s)\n", pTimer->enmState, pTimer->pszDesc));
    return SSMR3HandleSetStatus(pSSM, VERR_TM_UNKNOWN_STATE);
}


/**
 * Loads the state of a timer from a saved state.
 *
 * @returns VBox status.
 * @param   pTimer          Timer to restore.
 * @param   pSSM            Save State Manager handle.
 */
TMR3DECL(int) TMR3TimerLoad(PTMTIMERR3 pTimer, PSSMHANDLE pSSM)
{
    Assert(pTimer); Assert(pSSM); VM_ASSERT_EMT(pTimer->pVMR3);
    LogFlow(("TMR3TimerLoad: pTimer=%p:{enmState=%s, .pszDesc={%s}} pSSM=%p\n", pTimer, tmTimerState(pTimer->enmState), pTimer->pszDesc, pSSM));

    /*
     * Load the state and validate it.
     */
    uint8_t u8State;
    int rc = SSMR3GetU8(pSSM, &u8State);
    if (VBOX_FAILURE(rc))
        return rc;
    TMTIMERSTATE enmState = (TMTIMERSTATE)u8State;
    if (    enmState != TMTIMERSTATE_PENDING_STOP
        &&  enmState != TMTIMERSTATE_PENDING_SCHEDULE
        &&  enmState != TMTIMERSTATE_PENDING_STOP_SCHEDULE)
    {
        AssertMsgFailed(("enmState=%d %s\n", enmState, tmTimerState(enmState)));
        return SSMR3HandleSetStatus(pSSM, VERR_TM_LOAD_STATE);
    }

    if (enmState == TMTIMERSTATE_PENDING_SCHEDULE)
    {
        /*
         * Load the expire time.
         */
        uint64_t u64Expire;
        rc = SSMR3GetU64(pSSM, &u64Expire);
        if (VBOX_FAILURE(rc))
            return rc;

        /*
         * Set it.
         */
        Log(("enmState=%d %s u64Expire=%llu\n", enmState, tmTimerState(enmState), u64Expire));
        rc = TMTimerSet(pTimer, u64Expire);
    }
    else
    {
        /*
         * Stop it.
         */
        Log(("enmState=%d %s\n", enmState, tmTimerState(enmState)));
        rc = TMTimerStop(pTimer);
    }

    /*
     * On failure set SSM status.
     */
    if (VBOX_FAILURE(rc))
        rc = SSMR3HandleSetStatus(pSSM, rc);
    return rc;
}


/**
 * Get the real world UTC time adjusted for VM lag.
 *
 * @returns pTime.
 * @param   pVM             The VM instance.
 * @param   pTime           Where to store the time.
 */
TMR3DECL(PRTTIMESPEC) TMR3UTCNow(PVM pVM, PRTTIMESPEC pTime)
{
    RTTimeNow(pTime);
    RTTimeSpecSubNano(pTime, pVM->tm.s.offVirtualSync - pVM->tm.s.offVirtualSyncGivenUp);
    RTTimeSpecAddNano(pTime, pVM->tm.s.offUTC);
    return pTime;
}


/**
 * Display all timers.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) tmR3TimerInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);
    pHlp->pfnPrintf(pHlp,
                    "Timers (pVM=%p)\n"
                    "%.*s %.*s %.*s %.*s Clock %-18s %-18s %-25s Description\n",
                    pVM,
                    sizeof(RTR3PTR) * 2,        "pTimerR3        ",
                    sizeof(int32_t) * 2,        "offNext         ",
                    sizeof(int32_t) * 2,        "offPrev         ",
                    sizeof(int32_t) * 2,        "offSched        ",
                                                "Time",
                                                "Expire",
                                                "State");
    for (PTMTIMERR3 pTimer = pVM->tm.s.pCreated; pTimer; pTimer = pTimer->pBigNext)
    {
        pHlp->pfnPrintf(pHlp,
                        "%p %08RX32 %08RX32 %08RX32 %s %18RU64 %18RU64 %-25s %s\n",
                        pTimer,
                        pTimer->offNext,
                        pTimer->offPrev,
                        pTimer->offScheduleNext,
                        pTimer->enmClock == TMCLOCK_REAL ? "Real " : "Virt ",
                        TMTimerGet(pTimer),
                        pTimer->u64Expire,
                        tmTimerState(pTimer->enmState),
                        pTimer->pszDesc);
    }
}


/**
 * Display all active timers.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) tmR3TimerInfoActive(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);
    pHlp->pfnPrintf(pHlp,
                    "Active Timers (pVM=%p)\n"
                    "%.*s %.*s %.*s %.*s Clock %-18s %-18s %-25s Description\n",
                    pVM,
                    sizeof(RTR3PTR) * 2,        "pTimerR3        ",
                    sizeof(int32_t) * 2,        "offNext         ",
                    sizeof(int32_t) * 2,        "offPrev         ",
                    sizeof(int32_t) * 2,        "offSched        ",
                                                "Time",
                                                "Expire",
                                                "State");
    for (unsigned iQueue = 0; iQueue < TMCLOCK_MAX; iQueue++)
    {
        for (PTMTIMERR3 pTimer = TMTIMER_GET_HEAD(&pVM->tm.s.paTimerQueuesR3[iQueue]);
             pTimer;
             pTimer = TMTIMER_GET_NEXT(pTimer))
        {
            pHlp->pfnPrintf(pHlp,
                            "%p %08RX32 %08RX32 %08RX32 %s %18RU64 %18RU64 %-25s %s\n",
                            pTimer,
                            pTimer->offNext,
                            pTimer->offPrev,
                            pTimer->offScheduleNext,
                            pTimer->enmClock == TMCLOCK_REAL
                            ? "Real "
                            : pTimer->enmClock == TMCLOCK_VIRTUAL
                            ? "Virt "
                            : pTimer->enmClock == TMCLOCK_VIRTUAL_SYNC
                            ? "VrSy "
                            : "TSC  ",
                            TMTimerGet(pTimer),
                            pTimer->u64Expire,
                            tmTimerState(pTimer->enmState),
                            pTimer->pszDesc);
        }
    }
}


/**
 * Display all clocks.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) tmR3InfoClocks(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);

    /*
     * Read the times first to avoid more than necessary time variation.
     */
    const uint64_t u64TSC = TMCpuTickGet(pVM);
    const uint64_t u64Virtual = TMVirtualGet(pVM);
    const uint64_t u64VirtualSync = TMVirtualSyncGet(pVM);
    const uint64_t u64Real = TMRealGet(pVM);

    /*
     * TSC
     */
    pHlp->pfnPrintf(pHlp,
                    "Cpu Tick: %18RU64 (%#016RX64) %RU64Hz %s%s",
                    u64TSC, u64TSC, TMCpuTicksPerSecond(pVM),
                    pVM->tm.s.fTSCTicking ? "ticking" : "paused",
                    pVM->tm.s.fTSCVirtualized ? " - virtualized" : "");
    if (pVM->tm.s.fTSCUseRealTSC)
    {
        pHlp->pfnPrintf(pHlp, " - real tsc");
        if (pVM->tm.s.u64TSCOffset)
            pHlp->pfnPrintf(pHlp, "\n          offset %RU64", pVM->tm.s.u64TSCOffset);
    }
    else
        pHlp->pfnPrintf(pHlp, " - virtual clock");
    pHlp->pfnPrintf(pHlp, "\n");

    /*
     * virtual
     */
    pHlp->pfnPrintf(pHlp,
                    " Virtual: %18RU64 (%#016RX64) %RU64Hz %s",
                    u64Virtual, u64Virtual, TMVirtualGetFreq(pVM),
                    pVM->tm.s.fVirtualTicking ? "ticking" : "paused");
    if (pVM->tm.s.fVirtualWarpDrive)
        pHlp->pfnPrintf(pHlp, " WarpDrive %RU32 %%", pVM->tm.s.u32VirtualWarpDrivePercentage);
    pHlp->pfnPrintf(pHlp, "\n");

    /*
     * virtual sync
     */
    pHlp->pfnPrintf(pHlp,
                    "VirtSync: %18RU64 (%#016RX64) %s%s",
                    u64VirtualSync, u64VirtualSync,
                    pVM->tm.s.fVirtualSyncTicking ? "ticking" : "paused",
                    pVM->tm.s.fVirtualSyncCatchUp ? " - catchup" : "");
    if (pVM->tm.s.offVirtualSync)
    {
        pHlp->pfnPrintf(pHlp, "\n          offset %RU64", pVM->tm.s.offVirtualSync);
        if (pVM->tm.s.u32VirtualSyncCatchUpPercentage)
            pHlp->pfnPrintf(pHlp, "  catch-up rate %u %%", pVM->tm.s.u32VirtualSyncCatchUpPercentage);
    }
    pHlp->pfnPrintf(pHlp, "\n");

    /*
     * real
     */
    pHlp->pfnPrintf(pHlp,
                    "    Real: %18RU64 (%#016RX64) %RU64Hz\n",
                    u64Real, u64Real, TMRealGetFreq(pVM));
}

