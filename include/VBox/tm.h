/** @file
 * TM - Time Monitor.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___VBox_tm_h
#define ___VBox_tm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#ifdef IN_RING3
# include <iprt/time.h>
#endif

__BEGIN_DECLS

/** @defgroup grp_tm        The Time Monitor API
 * @{
 */

/** Enable a timer hack which improves the timer response/resolution a bit. */
#define VBOX_HIGH_RES_TIMERS_HACK


/**
 * Clock type.
 */
typedef enum TMCLOCK
{
    /** Real host time.
     * This clock ticks all the time, so use with care. */
    TMCLOCK_REAL = 0,
    /** Virtual guest time.
     * This clock only ticks when the guest is running. It's implemented
     * as an offset to real time. */
    TMCLOCK_VIRTUAL,
    /** Virtual guest synchronized timer time.
     * This is a special clock and timer queue for synchronizing virtual timers and
     * virtual time sources. This clock is trying to keep up with TMCLOCK_VIRTUAL,
     * but will wait for timers to be executed. If it lags too far behind TMCLOCK_VIRTUAL,
     * it will try speed up to close the distance. */
    TMCLOCK_VIRTUAL_SYNC,
    /** Virtual CPU timestamp. (Running only when we're executing guest code.) */
    TMCLOCK_TSC,
    /** Number of clocks. */
    TMCLOCK_MAX
} TMCLOCK;


/** @name Real Clock Methods
 * @{
 */
/**
 * Gets the current TMCLOCK_REAL time.
 *
 * @returns Real time.
 * @param   pVM             The VM handle.
 */
TMDECL(uint64_t) TMRealGet(PVM pVM);

/**
 * Gets the frequency of the TMCLOCK_REAL clock.
 *
 * @returns frequency.
 * @param   pVM             The VM handle.
 */
TMDECL(uint64_t) TMRealGetFreq(PVM pVM);
/** @} */


/** @name Virtual Clock Methods
 * @{
 */
/**
 * Gets the current TMCLOCK_VIRTUAL time.
 *
 * @returns The timestamp.
 * @param   pVM     VM handle.
 *
 * @remark  While the flow of time will never go backwards, the speed of the
 *          progress varies due to inaccurate RTTimeNanoTS and TSC. The latter can be
 *          influenced by power saving (SpeedStep, PowerNow!), while the former
 *          makes use of TSC and kernel timers.
 */
TMDECL(uint64_t) TMVirtualGet(PVM pVM);

/**
 * Gets the current TMCLOCK_VIRTUAL time
 *
 * @returns The timestamp.
 * @param   pVM             VM handle.
 * @param   fCheckTimers    Check timers or not
 *
 * @remark  While the flow of time will never go backwards, the speed of the
 *          progress varies due to inaccurate RTTimeNanoTS and TSC. The latter can be
 *          influenced by power saving (SpeedStep, PowerNow!), while the former
 *          makes use of TSC and kernel timers.
 */
TMDECL(uint64_t) TMVirtualGetEx(PVM pVM, bool fCheckTimers);

/**
 * Gets the current lag of the synchronous virtual clock (relative to the virtual clock).
 *
 * @return  The current lag.
 * @param   pVM     VM handle.
 */
TMDECL(uint64_t) TMVirtualSyncGetLag(PVM pVM);

/**
 * Get the current catch-up percent.
 *
 * @return  The current catch0up percent. 0 means running at the same speed as the virtual clock.
 * @param   pVM     VM handle.
 */
TMDECL(uint32_t) TMVirtualSyncGetCatchUpPct(PVM pVM);

/**
 * Gets the current TMCLOCK_VIRTUAL frequency.
 *
 * @returns The freqency.
 * @param   pVM     VM handle.
 */
TMDECL(uint64_t) TMVirtualGetFreq(PVM pVM);

/**
 * Gets the current TMCLOCK_VIRTUAL_SYNC time.
 *
 * @returns The timestamp.
 * @param   pVM     VM handle.
 * @param   fCheckTimers    Check timers or not
 * @thread  EMT.
 */
TMDECL(uint64_t) TMVirtualSyncGetEx(PVM pVM, bool fCheckTimers);

/**
 * Gets the current TMCLOCK_VIRTUAL_SYNC time.
 *
 * @returns The timestamp.
 * @param   pVM     VM handle.
 * @thread  EMT.
 */
TMDECL(uint64_t) TMVirtualSyncGet(PVM pVM);

/**
 * Resumes the virtual clock.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VINF_INTERNAL_ERROR and VBOX_STRICT assertion if called out of order.
 * @param   pVM     VM handle.
 */
TMDECL(int) TMVirtualResume(PVM pVM);

/**
 * Pauses the virtual clock.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VINF_INTERNAL_ERROR and VBOX_STRICT assertion if called out of order.
 * @param   pVM     VM handle.
 */
TMDECL(int) TMVirtualPause(PVM pVM);

/**
 * Converts from virtual ticks to nanoseconds.
 *
 * @returns nanoseconds.
 * @param   pVM             The VM handle.
 * @param   u64VirtualTicks The virtual ticks to convert.
 * @remark  There could be rounding errors here. We just do a simple integere divide
 *          without any adjustments.
 */
TMDECL(uint64_t) TMVirtualToNano(PVM pVM, uint64_t u64VirtualTicks);

/**
 * Converts from virtual ticks to microseconds.
 *
 * @returns microseconds.
 * @param   pVM             The VM handle.
 * @param   u64VirtualTicks The virtual ticks to convert.
 * @remark  There could be rounding errors here. We just do a simple integere divide
 *          without any adjustments.
 */
TMDECL(uint64_t) TMVirtualToMicro(PVM pVM, uint64_t u64VirtualTicks);

/**
 * Converts from virtual ticks to milliseconds.
 *
 * @returns milliseconds.
 * @param   pVM             The VM handle.
 * @param   u64VirtualTicks The virtual ticks to convert.
 * @remark  There could be rounding errors here. We just do a simple integere divide
 *          without any adjustments.
 */
TMDECL(uint64_t) TMVirtualToMilli(PVM pVM, uint64_t u64VirtualTicks);

/**
 * Converts from nanoseconds to virtual ticks.
 *
 * @returns virtual ticks.
 * @param   pVM             The VM handle.
 * @param   u64NanoTS       The nanosecond value ticks to convert.
 * @remark  There could be rounding and overflow errors here.
 */
TMDECL(uint64_t) TMVirtualFromNano(PVM pVM, uint64_t u64NanoTS);

/**
 * Converts from microseconds to virtual ticks.
 *
 * @returns virtual ticks.
 * @param   pVM             The VM handle.
 * @param   u64MicroTS      The microsecond value ticks to convert.
 * @remark  There could be rounding and overflow errors here.
 */
TMDECL(uint64_t) TMVirtualFromMicro(PVM pVM, uint64_t u64MicroTS);

/**
 * Converts from milliseconds to virtual ticks.
 *
 * @returns virtual ticks.
 * @param   pVM             The VM handle.
 * @param   u64MilliTS      The millisecond value ticks to convert.
 * @remark  There could be rounding and overflow errors here.
 */
TMDECL(uint64_t) TMVirtualFromMilli(PVM pVM, uint64_t u64MilliTS);

/**
 * Gets the current warp drive percent.
 *
 * @returns The warp drive percent.
 * @param   pVM         The VM handle.
 */
TMDECL(uint32_t) TMVirtualGetWarpDrive(PVM pVM);

/**
 * Sets the warp drive percent of the virtual time.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   u32Percent  The new percentage. 100 means normal operation.
 */
TMDECL(int) TMVirtualSetWarpDrive(PVM pVM, uint32_t u32Percent);

/** @} */


/** @name CPU Clock Methods
 * @{
 */
/**
 * Resumes the CPU timestamp counter ticking.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMDECL(int) TMCpuTickResume(PVM pVM);

/**
 * Pauses the CPU timestamp counter ticking.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMDECL(int) TMCpuTickPause(PVM pVM);

/**
 * Read the current CPU timstamp counter.
 *
 * @returns Gets the CPU tsc.
 * @param   pVM         The VM to operate on.
 */
TMDECL(uint64_t) TMCpuTickGet(PVM pVM);

/**
 * Returns the TSC offset (virtual TSC - host TSC)
 *
 * @returns TSC ofset
 * @param   pVM         The VM to operate on.
 * @todo    Remove this when the code has been switched to TMCpuTickCanUseRealTSC.
 */
TMDECL(uint64_t) TMCpuTickGetOffset(PVM pVM);

/**
 * Checks if AMD-V / VT-x can use an offsetted hardware TSC or not.
 *
 * @returns true/false accordingly.
 * @param   pVM             The VM handle.
 * @param   poffRealTSC     The offset against the TSC of the current CPU.
 *                          Can be NULL.
 * @thread EMT.
 */
TMDECL(bool) TMCpuTickCanUseRealTSC(PVM pVM, uint64_t *poffRealTSC);

/**
 * Sets the current CPU timestamp counter.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   u64Tick     The new timestamp value.
 */
TMDECL(int) TMCpuTickSet(PVM pVM, uint64_t u64Tick);

/**
 * Get the timestamp frequency.
 *
 * @returns Number of ticks per second.
 * @param   pVM     The VM.
 */
TMDECL(uint64_t) TMCpuTicksPerSecond(PVM pVM);

/** @} */


/** @name Timer Methods
 * @{
 */
/**
 * Device timer callback function.
 *
 * @param   pDevIns         Device instance of the device which registered the timer.
 * @param   pTimer          The timer handle.
 */
typedef DECLCALLBACK(void) FNTMTIMERDEV(PPDMDEVINS pDevIns, PTMTIMER pTimer);
/** Pointer to a device timer callback function. */
typedef FNTMTIMERDEV *PFNTMTIMERDEV;

/**
 * Driver timer callback function.
 *
 * @param   pDrvIns         Device instance of the device which registered the timer.
 * @param   pTimer          The timer handle.
 */
typedef DECLCALLBACK(void) FNTMTIMERDRV(PPDMDRVINS pDrvIns, PTMTIMER pTimer);
/** Pointer to a driver timer callback function. */
typedef FNTMTIMERDRV *PFNTMTIMERDRV;

/**
 * Service timer callback function.
 *
 * @param   pSrvIns         Service instance of the device which registered the timer.
 * @param   pTimer          The timer handle.
 */
typedef DECLCALLBACK(void) FNTMTIMERSRV(PPDMSRVINS pSrvIns, PTMTIMER pTimer);
/** Pointer to a service timer callback function. */
typedef FNTMTIMERSRV *PFNTMTIMERSRV;

/**
 * Internal timer callback function.
 *
 * @param   pVM             The VM.
 * @param   pTimer          The timer handle.
 * @param   pvUser          User argument specified upon timer creation.
 */
typedef DECLCALLBACK(void) FNTMTIMERINT(PVM pVM, PTMTIMER pTimer, void *pvUser);
/** Pointer to internal timer callback function. */
typedef FNTMTIMERINT *PFNTMTIMERINT;

/**
 * External timer callback function.
 *
 * @param   pvUser          User argument as specified when the timer was created.
 */
typedef DECLCALLBACK(void) FNTMTIMEREXT(void *pvUser);
/** Pointer to an external timer callback function. */
typedef FNTMTIMEREXT *PFNTMTIMEREXT;


/**
 * Gets the host context ring-3 pointer of the timer.
 *
 * @returns HC R3 pointer.
 * @param   pTimer      Timer handle as returned by one of the create functions.
 */
TMDECL(PTMTIMERR3) TMTimerR3Ptr(PTMTIMER pTimer);

/**
 * Gets the host context ring-0 pointer of the timer.
 *
 * @returns HC R0 pointer.
 * @param   pTimer      Timer handle as returned by one of the create functions.
 */
TMDECL(PTMTIMERR0) TMTimerR0Ptr(PTMTIMER pTimer);

/**
 * Gets the GC pointer of the timer.
 *
 * @returns GC pointer.
 * @param   pTimer      Timer handle as returned by one of the create functions.
 */
TMDECL(PTMTIMERGC) TMTimerGCPtr(PTMTIMER pTimer);


/**
 * Destroy a timer
 *
 * @returns VBox status.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 */
TMDECL(int) TMTimerDestroy(PTMTIMER pTimer);

/**
 * Arm a timer with a (new) expire time.
 *
 * @returns VBox status.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 * @param   u64Expire       New expire time.
 */
TMDECL(int) TMTimerSet(PTMTIMER pTimer, uint64_t u64Expire);

/**
 * Arm a timer with a (new) expire time relative to current clock.
 *
 * @returns VBox status.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 * @param   cMilliesToNext  Number of millieseconds to the next tick.
 */
TMDECL(int) TMTimerSetMillies(PTMTIMER pTimer, uint32_t cMilliesToNext);
TMDECL(int) TMTimerSetMicro(PTMTIMER pTimer, uint64_t cMicrosToNext);
TMDECL(int) TMTimerSetNano(PTMTIMER pTimer, uint64_t cNanosToNext);

/**
 * Get the current clock time.
 * Handy for calculating the new expire time.
 *
 * @returns Current clock time.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 */
TMDECL(uint64_t) TMTimerGet(PTMTIMER pTimer);

/**
 * Get the current clock time as nanoseconds.
 *
 * @returns The timer clock as nanoseconds.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 */
TMDECL(uint64_t) TMTimerGetNano(PTMTIMER pTimer);

/**
 * Get the current clock time as microseconds.
 *
 * @returns The timer clock as microseconds.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 */
TMDECL(uint64_t) TMTimerGetMicro(PTMTIMER pTimer);

/**
 * Get the current clock time as milliseconds.
 *
 * @returns The timer clock as milliseconds.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 */
TMDECL(uint64_t) TMTimerGetMilli(PTMTIMER pTimer);

/**
 * Get the freqency of the timer clock.
 *
 * @returns Clock frequency (as Hz of course).
 * @param   pTimer          Timer handle as returned by one of the create functions.
 */
TMDECL(uint64_t) TMTimerGetFreq(PTMTIMER pTimer);

/**
 * Get the expire time of the timer.
 * Only valid for active timers.
 *
 * @returns Expire time of the timer.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 */
TMDECL(uint64_t) TMTimerGetExpire(PTMTIMER pTimer);

/**
 * Converts the specified timer clock time to nanoseconds.
 *
 * @returns nanoseconds.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 * @param   u64Ticks        The clock ticks.
 * @remark  There could be rounding errors here. We just do a simple integere divide
 *          without any adjustments.
 */
TMDECL(uint64_t) TMTimerToNano(PTMTIMER pTimer, uint64_t u64Ticks);

/**
 * Converts the specified timer clock time to microseconds.
 *
 * @returns microseconds.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 * @param   u64Ticks        The clock ticks.
 * @remark  There could be rounding errors here. We just do a simple integere divide
 *          without any adjustments.
 */
TMDECL(uint64_t) TMTimerToMicro(PTMTIMER pTimer, uint64_t u64Ticks);

/**
 * Converts the specified timer clock time to milliseconds.
 *
 * @returns milliseconds.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 * @param   u64Ticks        The clock ticks.
 * @remark  There could be rounding errors here. We just do a simple integere divide
 *          without any adjustments.
 */
TMDECL(uint64_t) TMTimerToMilli(PTMTIMER pTimer, uint64_t u64Ticks);

/**
 * Converts the specified nanosecond timestamp to timer clock ticks.
 *
 * @returns timer clock ticks.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 * @param   u64NanoTS       The nanosecond value ticks to convert.
 * @remark  There could be rounding and overflow errors here.
 */
TMDECL(uint64_t) TMTimerFromNano(PTMTIMER pTimer, uint64_t u64NanoTS);

/**
 * Converts the specified microsecond timestamp to timer clock ticks.
 *
 * @returns timer clock ticks.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 * @param   u64MicroTS      The microsecond value ticks to convert.
 * @remark  There could be rounding and overflow errors here.
 */
TMDECL(uint64_t) TMTimerFromMicro(PTMTIMER pTimer, uint64_t u64MicroTS);

/**
 * Converts the specified millisecond timestamp to timer clock ticks.
 *
 * @returns timer clock ticks.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 * @param   u64MilliTS      The millisecond value ticks to convert.
 * @remark  There could be rounding and overflow errors here.
 */
TMDECL(uint64_t) TMTimerFromMilli(PTMTIMER pTimer, uint64_t u64MilliTS);

/**
 * Stop the timer.
 * Use TMR3TimerArm() to "un-stop" the timer.
 *
 * @returns VBox status.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 */
TMDECL(int) TMTimerStop(PTMTIMER pTimer);

/**
 * Checks if a timer is active or not.
 *
 * @returns True if active.
 * @returns False if not active.
 * @param   pTimer          Timer handle as returned by one of the create functions.
 */
TMDECL(bool) TMTimerIsActive(PTMTIMER pTimer);

/**
 * Set FF if we've passed the next virtual event.
 *
 * This function is called before FFs are checked in the inner execution EM loops.
 *
 * @returns Virtual timer ticks to the next event.
 * @param   pVM         Pointer to the shared VM structure.
 * @thread  The emulation thread.
 */
TMDECL(uint64_t) TMTimerPoll(PVM pVM);

/**
 * Set FF if we've passed the next virtual event.
 *
 * This function is called before FFs are checked in the inner execution EM loops.
 *
 * @returns The GIP timestamp of the next event.
 *          0 if the next event has already expired.
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pu64Delta   Where to store the delta.
 * @thread  The emulation thread.
 */
TMDECL(uint64_t) TMTimerPollGIP(PVM pVM, uint64_t *pu64Delta);

/** @} */


#ifdef IN_RING3
/** @defgroup grp_tm_r3     The TM Host Context Ring-3 API
 * @ingroup grp_tm
 * @{
 */

/**
 * Initializes the TM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMR3DECL(int) TMR3Init(PVM pVM);

/**
 * Finalizes the TM initialization.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMR3DECL(int) TMR3InitFinalize(PVM pVM);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The VM.
 * @param   offDelta    Relocation delta relative to old location.
 */
TMR3DECL(void) TMR3Relocate(PVM pVM, RTGCINTPTR offDelta);

/**
 * Terminates the TM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMR3DECL(int) TMR3Term(PVM pVM);

/**
 * The VM is being reset.
 *
 * For the TM component this means that a rescheduling is preformed,
 * the FF is cleared and but without running the queues. We'll have to
 * check if this makes sense or not, but it seems like a good idea now....
 *
 * @param   pVM     VM handle.
 */
TMR3DECL(void) TMR3Reset(PVM pVM);

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
TMR3DECL(int) TMR3GetImportGC(PVM pVM, const char *pszSymbol, PRTGCPTR pGCPtrValue);

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
TMR3DECL(int) TMR3TimerCreateDevice(PVM pVM, PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMERDEV pfnCallback, const char *pszDesc, PPTMTIMERR3 ppTimer);

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
TMR3DECL(int) TMR3TimerCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, TMCLOCK enmClock, PFNTMTIMERDRV pfnCallback, const char *pszDesc, PPTMTIMERR3 ppTimer);

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
TMR3DECL(int) TMR3TimerCreateInternal(PVM pVM, TMCLOCK enmClock, PFNTMTIMERINT pfnCallback, void *pvUser, const char *pszDesc, PPTMTIMERR3 ppTimer);

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
TMR3DECL(PTMTIMERR3) TMR3TimerCreateExternal(PVM pVM, TMCLOCK enmClock, PFNTMTIMEREXT pfnCallback, void *pvUser, const char *pszDesc);

/**
 * Destroy all timers owned by a device.
 *
 * @returns VBox status.
 * @param   pVM             VM handle.
 * @param   pDevIns         Device which timers should be destroyed.
 */
TMR3DECL(int) TMR3TimerDestroyDevice(PVM pVM, PPDMDEVINS pDevIns);

/**
 * Destroy all timers owned by a driver.
 *
 * @returns VBox status.
 * @param   pVM             VM handle.
 * @param   pDrvIns         Driver which timers should be destroyed.
 */
TMR3DECL(int) TMR3TimerDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns);

/**
 * Saves the state of a timer to a saved state.
 *
 * @returns VBox status.
 * @param   pTimer          Timer to save.
 * @param   pSSM            Save State Manager handle.
 */
TMR3DECL(int) TMR3TimerSave(PTMTIMERR3 pTimer, PSSMHANDLE pSSM);

/**
 * Loads the state of a timer from a saved state.
 *
 * @returns VBox status.
 * @param   pTimer          Timer to restore.
 * @param   pSSM            Save State Manager handle.
 */
TMR3DECL(int) TMR3TimerLoad(PTMTIMERR3 pTimer, PSSMHANDLE pSSM);

/**
 * Schedules and runs any pending timers.
 *
 * This is normally called from a forced action handler in EMT.
 *
 * @param   pVM             The VM to run the timers for.
 * @thread  The emulation thread.
 */
TMR3DECL(void) TMR3TimerQueuesDo(PVM pVM);

/**
 * Get the real world UTC time adjusted for VM lag.
 *
 * @returns pTime.
 * @param   pVM             The VM instance.
 * @param   pTime           Where to store the time.
 */
TMR3DECL(PRTTIMESPEC) TMR3UTCNow(PVM pVM, PRTTIMESPEC pTime);

/** @} */
#endif


#ifdef IN_GC
/** @defgroup grp_tm_gc     The TM Guest Context API
 * @ingroup grp_tm
 * @{
 */


/** @} */
#endif

/** @} */

__END_DECLS

#endif

