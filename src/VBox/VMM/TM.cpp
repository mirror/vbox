/* $Id$ */
/** @file
 * TM - Timeout Manager.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/** @page pg_tm        TM - The Time Manager
 *
 * The Time Manager abstracts the CPU clocks and manages timers used by VM device.
 *
 *
 *
 * @section sec_tm_timers   Timers
 *
 * The timers supports multiple clocks. Currently there are two clocks in the
 * TM, the host real time clock and the guest virtual clock. Each clock has it's
 * own set of scheduling facilities which are identical but for the clock source.
 *
 * Take one such timer scheduling facility, or timer queue if you like. There are
 * a few factors which makes it a bit complex. First there is the usual GC vs. HC
 * thing. Then there is multiple threads, and then there is the fact that on Unix
 * we might just as well take a timer signal which checks whether it's wise to
 * schedule timers while we're scheduling them. On API level, all but the create
 * and save APIs must be mulithreaded.
 *
 * The design is using a doubly linked HC list of active timers which is ordered
 * by expire date. Updates to the list is batched in a singly linked list (linked
 * by handle not pointer for atomically update support in both GC and HC) and
 * will be processed by the emulation thread.
 *
 * For figuring out when there is need to schedule timers a high frequency
 * asynchronous timer is employed using Host OS services. Its task is to check if
 * there are anything batched up or if a head has expired. If this is the case
 * a forced action is signals and the emulation thread will process this ASAP.
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
#define TM_SAVED_STATE_VERSION  2


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static uint64_t             tmR3Calibrate(void);
static DECLCALLBACK(int)    tmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)    tmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
static DECLCALLBACK(void)   tmR3TimerCallback(PRTTIMER pTimer, void *pvUser);
static void                 tmR3TimerQueueRun(PVM pVM, PTMTIMERQUEUE pQueue);
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
        case TMCLOCK_VIRTUAL_SYNC:  return TMVirtualGetSync(pVM);
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
     * We indirectly - thru RTTimeNanoTS and RTTimeMilliTS - use the global
     * info page (GIP) for both the virtual and the real clock. By mapping
     * the GIP into guest context we can get just as accurate time even there.
     * All that's required is that the g_pSUPGlobalInfoPage symbol is available
     * to the GC Runtime.
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

    /*
     * Determin the TSC configuration and frequency.
     */
    /* mode */
    rc = CFGMR3QueryBool(CFGMR3GetRoot(pVM), "TSCVirtualized", &pVM->tm.s.fTSCVirtualized);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->tm.s.fTSCVirtualized = true; /* trap rdtsc */
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS, 
                          N_("Configuration error: Failed to querying bool value \"UseRealTSC\". (%Vrc)"), rc); 

    /* source */
    rc = CFGMR3QueryBool(CFGMR3GetRoot(pVM), "UseRealTSC", &pVM->tm.s.fTSCTicking);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->tm.s.fTSCUseRealTSC = false; /* use virtual time */
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS, 
                          N_("Configuration error: Failed to querying bool value \"UseRealTSC\". (%Vrc)"), rc); 
    if (!pVM->tm.s.fTSCUseRealTSC)
        pVM->tm.s.fTSCVirtualized = true;

    /* frequency */
    rc = CFGMR3QueryU64(CFGMR3GetRoot(pVM), "TSCTicksPerSecond", &pVM->tm.s.cTSCTicksPerSecond);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        pVM->tm.s.cTSCTicksPerSecond = tmR3Calibrate();
        if (    !pVM->tm.s.fTSCUseRealTSC
            &&  pVM->tm.s.cTSCTicksPerSecond >= _4G)
            pVM->tm.s.cTSCTicksPerSecond = _4G - 1; /* (A limitation of our math code) */
    }
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS, 
                          N_("Configuration error: Failed to querying uint64_t value \"TSCTicksPerSecond\". (%Vrc)"), rc); 
    else if (   pVM->tm.s.cTSCTicksPerSecond < _1M
             || pVM->tm.s.cTSCTicksPerSecond >= _4G)
        return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS, 
                          N_("Configuration error: \"TSCTicksPerSecond\" = %RI64 is not in the range 1MHz..4GHz-1!"),
                          pVM->tm.s.cTSCTicksPerSecond);
    else
    {
        pVM->tm.s.fTSCUseRealTSC = false;
        pVM->tm.s.fTSCVirtualized = true;
    }

    /* setup and report */
    if (pVM->tm.s.fTSCUseRealTSC)
        CPUMR3SetCR4Feature(pVM, 0, ~X86_CR4_TSD);
    else
        CPUMR3SetCR4Feature(pVM, X86_CR4_TSD, ~X86_CR4_TSD);
    LogRel(("TM: cTSCTicksPerSecond=%#RX64 (%RU64) fTSCVirtualized=%RTbool fTSCUseRealTSC=%RTbool\n", 
            pVM->tm.s.cTSCTicksPerSecond, pVM->tm.s.cTSCTicksPerSecond, pVM->tm.s.fTSCVirtualized, pVM->tm.s.fTSCUseRealTSC));

    /*
     * Register saved state.
     */
    rc = SSMR3RegisterInternal(pVM, "tm", 1, TM_SAVED_STATE_VERSION, sizeof(uint64_t) * 8,
                               NULL, tmR3Save, NULL,
                               NULL, tmR3Load, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Setup the warp drive.
     */
    rc = CFGMR3QueryU32(CFGMR3GetRoot(pVM), "WarpDrivePercentage", &pVM->tm.s.u32VirtualWarpDrivePercentage);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->tm.s.u32VirtualWarpDrivePercentage = 100;
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS, 
                          N_("Configuration error: Failed to querying uint32_t value \"WarpDrivePercent\". (%Vrc)"), rc); 
    else if (   pVM->tm.s.u32VirtualWarpDrivePercentage < 2 
             || pVM->tm.s.u32VirtualWarpDrivePercentage > 20000)
        return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS, 
                          N_("Configuration error: \"WarpDrivePercent\" = %RI32 is not in the range 2..20000!"),
                          pVM->tm.s.u32VirtualWarpDrivePercentage);
    pVM->tm.s.fVirtualWarpDrive = pVM->tm.s.u32VirtualWarpDrivePercentage != 100;
    if (pVM->tm.s.fVirtualWarpDrive)
        LogRel(("TM: u32VirtualWarpDrivePercentage=%RI32\n", pVM->tm.s.u32VirtualWarpDrivePercentage));

    /*
     * Start the timer (guard against REM not yielding).
     */
    uint32_t u32Millies;
    rc = CFGMR3QueryU32(CFGMR3GetRoot(pVM), "TimerMillies", &u32Millies);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        u32Millies = 10;
    else if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS, 
                          N_("Configuration error: Failed to query uint32_t value \"TimerMillies\", rc=%Vrc.\n"), rc);
    rc = RTTimerCreate(&pVM->tm.s.pTimer, u32Millies, tmR3TimerCallback, pVM);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to create timer, u32Millies=%d rc=%Vrc.\n", u32Millies, rc));
        return rc;
    }
    Log(("TM: Created timer %p firing every %d millieseconds\n", pVM->tm.s.pTimer, u32Millies));
    pVM->tm.s.u32TimerMillies = u32Millies;

#ifdef VBOX_WITH_STATISTICS
    /*
     * Register statistics.
     */
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

    STAM_REG(pVM, &pVM->tm.s.StatVirtualGet,        STAMTYPE_COUNTER,       "/TM/VirtualGet",       STAMUNIT_OCCURENCES,        "The number of times TMR3TimerGet was called when the clock was running.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualGetSync,    STAMTYPE_COUNTER,       "/TM/VirtualGetSync",   STAMUNIT_OCCURENCES,        "The number of times TMR3TimerGetSync was called when the clock was running.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualPause,      STAMTYPE_COUNTER,       "/TM/VirtualPause",     STAMUNIT_OCCURENCES,        "The number of times TMR3TimerPause was called.");
    STAM_REG(pVM, &pVM->tm.s.StatVirtualResume,     STAMTYPE_COUNTER,       "/TM/VirtualResume",    STAMUNIT_OCCURENCES,        "The number of times TMR3TimerResume was called.");

    STAM_REG(pVM, &pVM->tm.s.StatTimerCallbackSetFF,STAMTYPE_COUNTER,       "/TM/CallbackSetFF",    STAMUNIT_OCCURENCES,        "The number of times the timer callback set FF.");
#endif /* VBOX_WITH_STATISTICS */

    /*
     * Register info handlers.
     */
    DBGFR3InfoRegisterInternal(pVM, "timers",       "Dumps all timers. No arguments.",          tmR3TimerInfo);
    DBGFR3InfoRegisterInternal(pVM, "activetimers", "Dumps active all timers. No arguments.",   tmR3TimerInfoActive);
    DBGFR3InfoRegisterInternal(pVM, "clocks",       "Display the time of the various clocks.",  tmR3InfoClocks);

    return VINF_SUCCESS;
}


/**
 * Calibrate the CPU tick.
 *
 * @returns Number of ticks per second.
 */
static uint64_t tmR3Calibrate(void)
{
    /*
     * Use GIP when available present.
     */
    uint64_t    u64Hz;
    PCSUPGLOBALINFOPAGE pGip = g_pSUPGlobalInfoPage;
    if (    pGip 
        &&  pGip->u32Magic == SUPGLOBALINFOPAGE_MAGIC)
    {
        unsigned iCpu = pGip->u32Mode != SUPGIPMODE_ASYNC_TSC ? 0 : ASMGetApicId();
        if (iCpu >= RT_ELEMENTS(pGip->aCPUs))
            AssertReleaseMsgFailed(("iCpu=%d - the ApicId is too high. send VBox.log and hardware specs!\n", iCpu));
        else
        {
            RTThreadSleep(32);              /* To preserve old behaviour and to get a good CpuHz at startup. */
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
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The VM.
 * @param   offDelta    Relocation delta relative to old location.
 */
TMR3DECL(void) TMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("TMR3Relocate\n"));
    pVM->tm.s.pvGIPGC = MMHyperR3ToGC(pVM, pVM->tm.s.pvGIPR3);
    pVM->tm.s.paTimerQueuesGC = MMHyperR3ToGC(pVM, pVM->tm.s.paTimerQueuesR3);
    pVM->tm.s.paTimerQueuesR0 = MMHyperR3ToR0(pVM, pVM->tm.s.paTimerQueuesR3);

    /*
     * Iterate the timers updating the pVMGC pointers.
     */
    for (PTMTIMER pTimer = pVM->tm.s.pCreated; pTimer; pTimer = pTimer->pBigNext)
    {
        pTimer->pVMGC = pVM->pVMGC;
        pTimer->pVMR0 = (PVMR0)pVM->pVMHC; /// @todo pTimer->pVMR0 = pVM->pVMR0;
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
    SSMR3PutU64(pSSM, pVM->tm.s.u64VirtualSyncOffset);
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
    SSMR3GetU64(pSSM, &pVM->tm.s.u64VirtualSync);
    uint64_t u64;
    SSMR3GetU64(pSSM, &u64);
    pVM->tm.s.u64VirtualSyncOffset = u64;
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


/** @todo doc */
static int tmr3TimerCreate(PVM pVM, TMCLOCK enmClock, const char *pszDesc, PPTMTIMERHC ppTimer)
{
    VM_ASSERT_EMT(pVM);

    /*
     * Allocate the timer.
     */
    PTMTIMERHC pTimer = NULL;
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
    pTimer->pVMR0           = (PVMR0)pVM->pVMHC; /// @todo pTimer->pVMR0 = pVM->pVMR0;
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
TMR3DECL(int) TMR3TimerCreateDevice(PVM pVM, PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMERDEV pfnCallback, const char *pszDesc, PPTMTIMERHC ppTimer)
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
TMR3DECL(int) TMR3TimerCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, TMCLOCK enmClock, PFNTMTIMERDRV pfnCallback, const char *pszDesc, PPTMTIMERHC ppTimer)
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
TMR3DECL(int) TMR3TimerCreateInternal(PVM pVM, TMCLOCK enmClock, PFNTMTIMERINT pfnCallback, void *pvUser, const char *pszDesc, PPTMTIMERHC ppTimer)
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
TMR3DECL(PTMTIMERHC) TMR3TimerCreateExternal(PVM pVM, TMCLOCK enmClock, PFNTMTIMEREXT pfnCallback, void *pvUser, const char *pszDesc)
{
    /*
     * Allocate and init stuff.
     */
    PTMTIMERHC pTimer;
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
 * Checks if a queue has a pending timer.
 *
 * @returns true if it has a pending timer.
 * @returns false is no pending timer.
 *
 * @param   pVM         The VM handle.
 * @param   enmClock    The queue.
 */
DECLINLINE(bool) tmR3HasPending(PVM pVM, TMCLOCK enmClock)
{
    const uint64_t u64Expire = pVM->tm.s.CTXALLSUFF(paTimerQueues)[enmClock].u64Expire;
    return u64Expire != INT64_MAX && u64Expire <= tmClock(pVM, enmClock);
}


/**
 * Schedulation timer callback.
 *
 * @param   pTimer      Timer handle.
 * @param   pvUser      VM handle.
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
            ||  tmR3HasPending(pVM, TMCLOCK_VIRTUAL_SYNC)
            ||  tmR3HasPending(pVM, TMCLOCK_VIRTUAL)
            ||  tmR3HasPending(pVM, TMCLOCK_REAL)
            ||  tmR3HasPending(pVM, TMCLOCK_TSC)
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

    /* TMCLOCK_VIRTUAL */
    STAM_PROFILE_ADV_START(&pVM->tm.s.StatDoQueuesSchedule, s1);
    tmTimerQueueSchedule(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL]);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesSchedule, s1);
    STAM_PROFILE_ADV_START(&pVM->tm.s.StatDoQueuesRun, r1);
    tmR3TimerQueueRun(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL]);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesRun, r1);

    /* TMCLOCK_VIRTUAL_SYNC */
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesSchedule, s1);
    tmTimerQueueSchedule(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL_SYNC]);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesSchedule, s2);
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesRun, r1);
    tmR3TimerQueueRun(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_VIRTUAL_SYNC]);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesRun, r2);

    /* TMCLOCK_REAL */
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesSchedule, s2);
    tmTimerQueueSchedule(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_REAL]);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesSchedule, s3);
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesRun, r2);
    tmR3TimerQueueRun(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_REAL]);
    STAM_PROFILE_ADV_SUSPEND(&pVM->tm.s.StatDoQueuesRun, r3);

    /* TMCLOCK_TSC */
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesSchedule, s3);
    tmTimerQueueSchedule(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_TSC]);
    STAM_PROFILE_ADV_STOP(&pVM->tm.s.StatDoQueuesSchedule, s3);
    STAM_PROFILE_ADV_RESUME(&pVM->tm.s.StatDoQueuesRun, r3);
    tmR3TimerQueueRun(pVM, &pVM->tm.s.paTimerQueuesR3[TMCLOCK_TSC]);
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
    /** @todo deal with the VIRTUAL_SYNC pausing and catch calcs ++ */
    uint64_t u64Now = tmClock(pVM, pQueue->enmClock);
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
 * Saves the state of a timer to a saved state.
 *
 * @returns VBox status.
 * @param   pTimer          Timer to save.
 * @param   pSSM            Save State Manager handle.
 */
TMR3DECL(int) TMR3TimerSave(PTMTIMERHC pTimer, PSSMHANDLE pSSM)
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
TMR3DECL(int) TMR3TimerLoad(PTMTIMERHC pTimer, PSSMHANDLE pSSM)
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
    for (PTMTIMERHC pTimer = pVM->tm.s.pCreated; pTimer; pTimer = pTimer->pBigNext)
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
        for (PTMTIMERHC pTimer = TMTIMER_GET_HEAD(&pVM->tm.s.paTimerQueuesR3[iQueue]);
             pTimer;
             pTimer = TMTIMER_GET_NEXT(pTimer))
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

    /* TSC */
    uint64_t u64 = TMCpuTickGet(pVM);
    pHlp->pfnPrintf(pHlp,
                    "Cpu Tick: %#RX64 (%RU64) %RU64Hz %s%s", 
                    u64, u64, TMCpuTicksPerSecond(pVM),
                    pVM->tm.s.fTSCTicking ? "ticking" : "paused",
                    pVM->tm.s.fTSCVirtualized ? " - virtualized" : "");
    if (pVM->tm.s.fTSCUseRealTSC)
    {
        pHlp->pfnPrintf(pHlp, " - real tsc");
        if (pVM->tm.s.u64TSCOffset)
            pHlp->pfnPrintf(pHlp, "\n          offset %#RX64", pVM->tm.s.u64TSCOffset);
    }
    else
        pHlp->pfnPrintf(pHlp, " - virtual clock");
    pHlp->pfnPrintf(pHlp, "\n");

    /* virtual */
    u64 = TMVirtualGet(pVM);
    pHlp->pfnPrintf(pHlp,
                    " Virtual: %#RX64 (%RU64) %RU64Hz %s",
                    u64, u64, TMVirtualGetFreq(pVM),
                    pVM->tm.s.fVirtualTicking ? "ticking" : "paused");
    if (pVM->tm.s.fVirtualWarpDrive)
        pHlp->pfnPrintf(pHlp, " WarpDrive %RU32 %%", pVM->tm.s.u32VirtualWarpDrivePercentage);
    pHlp->pfnPrintf(pHlp, "\n");

    /* virtual sync */
    u64 = TMVirtualGetSync(pVM);
    pHlp->pfnPrintf(pHlp,
                    "VirtSync: %#RX64 (%RU64) %s%s",
                    u64, u64, 
                    pVM->tm.s.fVirtualSyncTicking ? "ticking" : "paused",
                    pVM->tm.s.fVirtualSyncCatchUp ? " - catchup" : "");
    if (pVM->tm.s.u64VirtualSyncOffset)
        pHlp->pfnPrintf(pHlp, "\n          offset %#RX64", pVM->tm.s.u64VirtualSyncOffset);
    pHlp->pfnPrintf(pHlp, "\n");

    /* real */
    u64 = TMRealGet(pVM);
    pHlp->pfnPrintf(pHlp,
                    "    Real: %#RX64 (%RU64) %RU64Hz\n",
                    u64, u64, TMRealGetFreq(pVM));
}
