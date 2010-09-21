/* $Id$ */
/** @file
 * IPRT R0 Testcase - Timers.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
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
#include <iprt/timer.h>

#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include <VBox/sup.h>
#include "tstRTR0Timer.h"
#include "tstRTR0Common.h"

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct
{
    /** Array of nano second timestamp of the first few shots. */
    uint64_t volatile   aShotNsTSes[10];
    /** The number of shots. */
    uint32_t volatile   cShots;
    /** The shot at which action is to be taken. */
    uint32_t            iActionShot;
    /** The RC of whatever operation performed in the handler. */
    int volatile        rc;
} TSTRTR0TIMERS1;
typedef TSTRTR0TIMERS1 *PTSTRTR0TIMERS1;


/**
 * Callback which increments destroy the timer when it fires.
 *
 * @param   pTimer      The timer.
 * @param   iTick       The current tick.
 * @param   pvUser      The user argument.
 */
static DECLCALLBACK(void) tstRTR0TimerCallbackDestroyOnce(PRTTIMER pTimer, void *pvUser, uint64_t iTick)
{
    PTSTRTR0TIMERS1 pState = (PTSTRTR0TIMERS1)pvUser;
    uint32_t        iShot  = ASMAtomicIncU32(&pState->cShots);

    if (iShot <= RT_ELEMENTS(pState->aShotNsTSes))
        pState->aShotNsTSes[iShot - 1] = RTTimeSystemNanoTS();

    if (iShot == pState->iActionShot + 1)
        RTR0TESTR0_CHECK_RC(pState->rc = RTTimerDestroy(pTimer), VINF_SUCCESS);
}


/**
 * Callback which increments restarts a timer once.
 *
 * @param   pTimer      The timer.
 * @param   iTick       The current tick.
 * @param   pvUser      The user argument.
 */
static DECLCALLBACK(void) tstRTR0TimerCallbackRestartOnce(PRTTIMER pTimer, void *pvUser, uint64_t iTick)
{
    PTSTRTR0TIMERS1 pState = (PTSTRTR0TIMERS1)pvUser;
    uint32_t        iShot  = ASMAtomicIncU32(&pState->cShots);

    if (iShot <= RT_ELEMENTS(pState->aShotNsTSes))
        pState->aShotNsTSes[iShot - 1] = RTTimeSystemNanoTS();

    if (iShot == pState->iActionShot + 1)
        RTR0TESTR0_CHECK_RC(pState->rc = RTTimerStart(pTimer, 10000000 /* 10ms */), VINF_SUCCESS);
}


/**
 * Callback which increments a 32-bit counter.
 *
 * @param   pTimer      The timer.
 * @param   iTick       The current tick.
 * @param   pvUser      The user argument.
 */
static DECLCALLBACK(void) tstRTR0TimerCallbackU32Counter(PRTTIMER pTimer, void *pvUser, uint64_t iTick)
{
    PTSTRTR0TIMERS1 pState = (PTSTRTR0TIMERS1)pvUser;
    uint32_t        iShot  = ASMAtomicIncU32(&pState->cShots);

    if (iShot <= RT_ELEMENTS(pState->aShotNsTSes))
        pState->aShotNsTSes[iShot - 1] = RTTimeSystemNanoTS();
}


/**
 * Checks that the interval between two timer shots are within the specified
 * range.
 *
 * @returns 0 if ok, 1 if bad.
 * @param   iShot               The shot number (for bitching).
 * @param   uPrevTS             The time stamp of the previous shot (ns).
 * @param   uThisTS             The timer stamp of this shot (ns).
 * @param   uMin                The minimum interval (ns).
 * @param   uMax                The maximum interval (ns).
 */
static int tstRTR0TimerCheckShotInterval(uint32_t iShot, uint64_t uPrevTS, uint64_t uThisTS, uint32_t uMin, uint32_t uMax)
{
    uint64_t uDelta = uThisTS - uPrevTS;
    RTR0TESTR0_CHECK_MSG_RET(uDelta >= uMin, ("iShot=%u uDelta=%lld uMin=%u\n", iShot, uDelta, uMin), 1);
    RTR0TESTR0_CHECK_MSG_RET(uDelta <= uMax, ("iShot=%u uDelta=%lld uMax=%u\n", iShot, uDelta, uMax), 1);
    return 0;
}


/**
 * Checks that the interval between timer shots are within a certain range.
 *
 * @returns Number of violations (i.e. 0 is ok).
 * @param   pState              The state.
 * @param   uStartNsTS          The start time stamp (ns).
 * @param   uMin                The minimum interval (ns).
 * @param   uMax                The maximum interval (ns).
 */
static int tstRTR0TimerCheckShotIntervals(PTSTRTR0TIMERS1 pState, uint64_t uStartNsTS, uint32_t uMin, uint32_t uMax)
{
    uint64_t uMaxDelta = 0;
    uint64_t uMinDelta = UINT64_MAX;
    uint32_t cBadShots = 0;
    uint32_t cShots    = pState->cShots;
    uint64_t uPrevTS   = uStartNsTS;
    for (uint32_t iShot = 0; iShot < cShots; iShot++)
    {
        uint64_t uThisTS = pState->aShotNsTSes[iShot];
        uint64_t uDelta  = uThisTS - uPrevTS;
        if (uDelta > uMaxDelta)
            uMaxDelta = uDelta;
        if (uDelta < uMinDelta)
            uMinDelta = uDelta;
        cBadShots += !(uDelta >= uMin && uDelta <= uMax);

        RTR0TESTR0_CHECK_MSG(uDelta >= uMin, ("iShot=%u uDelta=%lld uMin=%u\n", iShot, uDelta, uMin));
        RTR0TESTR0_CHECK_MSG(uDelta <= uMax, ("iShot=%u uDelta=%lld uMax=%u\n", iShot, uDelta, uMax));

        uPrevTS = uThisTS;
    }

    RTR0TestR0Info("uMaxDelta=%llu uMinDelta=%llu\n", uMaxDelta, uMinDelta);
    return cBadShots;
}


/**
 * Service request callback function.
 *
 * @returns VBox status code.
 * @param   pSession    The caller's session.
 * @param   u64Arg      64-bit integer argument.
 * @param   pReqHdr     The request header. Input / Output. Optional.
 */
DECLEXPORT(int) TSTRTR0TimerSrvReqHandler(PSUPDRVSESSION pSession, uint32_t uOperation,
                                          uint64_t u64Arg, PSUPR0SERVICEREQHDR pReqHdr)
{
    RTR0TESTR0_SRV_REQ_PROLOG_RET(pReqHdr);
    if (u64Arg)
        return VERR_INVALID_PARAMETER;

    /*
     * The big switch.
     */
    uint32_t const cNsSysHz = RTTimerGetSystemGranularity();
    TSTRTR0TIMERS1 State;
    switch (uOperation)
    {
        RTR0TESTR0_IMPLEMENT_SANITY_CASES();
        RTR0TESTR0_IMPLEMENT_DEFAULT_CASE(uOperation);

        case TSTRTR0TIMER_ONE_SHOT_BASIC:
        case TSTRTR0TIMER_ONE_SHOT_BASIC_HIRES:
        {
            /* Check that RTTimerGetSystemGranularity works. */
            RTR0TESTR0_CHECK_MSG_BREAK(cNsSysHz > UINT32_C(0) && cNsSysHz < UINT32_C(1000000000), ("%u", cNsSysHz));

            /* Create a one-shot timer and take one shot. */
            PRTTIMER        pTimer;
            uint32_t        fFlags = uOperation != TSTRTR0TIMER_ONE_SHOT_BASIC_HIRES ? RTTIMER_FLAGS_HIGH_RES : 0;
            RTR0TESTR0_CHECK_RC_BREAK(RTTimerCreateEx(&pTimer, 0, fFlags, tstRTR0TimerCallbackU32Counter, &State),
                                      VINF_SUCCESS);

            do /* break loop */
            {
                RT_ZERO(State);
                RTR0TESTR0_CHECK_RC_BREAK(RTTimerStart(pTimer, 0), VINF_SUCCESS);
                for (uint32_t i = 0; i < 1000 && !ASMAtomicUoReadU32(&State.cShots); i++)
                    RTThreadSleep(5);
                RTR0TESTR0_CHECK_MSG_BREAK(ASMAtomicUoReadU32(&State.cShots) == 1, ("cShots=%u\n", State.cShots));

                /* check that it is restartable. */
                RT_ZERO(State);
                RTR0TESTR0_CHECK_RC_BREAK(RTTimerStart(pTimer, 0), VINF_SUCCESS);
                for (uint32_t i = 0; i < 1000 && !ASMAtomicUoReadU32(&State.cShots); i++)
                    RTThreadSleep(5);
                RTR0TESTR0_CHECK_MSG_BREAK(ASMAtomicUoReadU32(&State.cShots) == 1, ("cShots=%u\n", State.cShots));

                /* check that it respects the timeout value and can be cancelled. */
                RT_ZERO(State);
                RTR0TESTR0_CHECK_RC(RTTimerStart(pTimer, 5*UINT64_C(1000000000)), VINF_SUCCESS);
                RTR0TESTR0_CHECK_RC(RTTimerStop(pTimer), VINF_SUCCESS);
                RTThreadSleep(1);
                RTR0TESTR0_CHECK_MSG_BREAK(ASMAtomicUoReadU32(&State.cShots) == 0, ("cShots=%u\n", State.cShots));

                /* Check some double starts and stops (shall not assert). */
                RT_ZERO(State);
                RTR0TESTR0_CHECK_RC(RTTimerStart(pTimer, 5*UINT64_C(1000000000)), VINF_SUCCESS);
                RTR0TESTR0_CHECK_RC(RTTimerStart(pTimer, 0), VERR_TIMER_ACTIVE);
                RTR0TESTR0_CHECK_RC(RTTimerStop(pTimer), VINF_SUCCESS);
                RTR0TESTR0_CHECK_RC(RTTimerStop(pTimer), VERR_TIMER_SUSPENDED);
                RTThreadSleep(1);
                RTR0TESTR0_CHECK_MSG_BREAK(ASMAtomicUoReadU32(&State.cShots) == 0, ("cShots=%u\n", State.cShots));
            } while (0);
            RTR0TESTR0_CHECK_RC(RTTimerDestroy(pTimer), VINF_SUCCESS);
            RTR0TESTR0_CHECK_RC(RTTimerDestroy(NULL), VINF_SUCCESS);
            break;
        }

#if 1 /* might have to disable this for some host... */
        case TSTRTR0TIMER_ONE_SHOT_RESTART:
        case TSTRTR0TIMER_ONE_SHOT_RESTART_HIRES:
        {
            /* Create a one-shot timer and restart it in the callback handler. */
            PRTTIMER            pTimer;
            uint32_t            fFlags = uOperation != TSTRTR0TIMER_ONE_SHOT_RESTART_HIRES ? RTTIMER_FLAGS_HIGH_RES : 0;
            for (uint32_t iTest = 0; iTest < 2; iTest++)
            {
                RTR0TESTR0_CHECK_RC_BREAK(RTTimerCreateEx(&pTimer, 0, fFlags, tstRTR0TimerCallbackRestartOnce, &State),
                                          VINF_SUCCESS);

                RT_ZERO(State);
                State.iActionShot = 0;
                do /* break loop */
                {
                    RTR0TESTR0_CHECK_RC_BREAK(RTTimerStart(pTimer, cNsSysHz * iTest), VINF_SUCCESS);
                    for (uint32_t i = 0; i < 1000 && ASMAtomicUoReadU32(&State.cShots) < 2; i++)
                        RTThreadSleep(5);
                    RTR0TESTR0_CHECK_MSG_BREAK(ASMAtomicUoReadU32(&State.cShots) == 2, ("cShots=%u\n", State.cShots));
                } while (0);
                RTR0TESTR0_CHECK_RC(RTTimerDestroy(pTimer), VINF_SUCCESS);
            }
            break;
        }
#endif

#if 1 /* might have to disable this for some host... */
        case TSTRTR0TIMER_ONE_SHOT_DESTROY:
        case TSTRTR0TIMER_ONE_SHOT_DESTROY_HIRES:
        {
            /* Create a one-shot timer and destroy it in the callback handler. */
            PRTTIMER            pTimer;
            uint32_t            fFlags = uOperation != TSTRTR0TIMER_ONE_SHOT_DESTROY_HIRES ? RTTIMER_FLAGS_HIGH_RES : 0;
            for (uint32_t iTest = 0; iTest < 2; iTest++)
            {
                RTR0TESTR0_CHECK_RC_BREAK(RTTimerCreateEx(&pTimer, 0, fFlags, tstRTR0TimerCallbackDestroyOnce, &State),
                                          VINF_SUCCESS);

                RT_ZERO(State);
                State.rc = VERR_IPE_UNINITIALIZED_STATUS;
                State.iActionShot = 0;
                do /* break loop */
                {
                    RTR0TESTR0_CHECK_RC_BREAK(RTTimerStart(pTimer, cNsSysHz * iTest), VINF_SUCCESS);
                    for (uint32_t i = 0; i < 1000 && ASMAtomicUoReadU32(&State.cShots) < 1; i++)
                        RTThreadSleep(5);
                    RTR0TESTR0_CHECK_MSG_BREAK(ASMAtomicReadU32(&State.cShots) == 1, ("cShots=%u\n", State.cShots));
                    RTR0TESTR0_CHECK_MSG_BREAK(State.rc == VINF_SUCCESS, ("rc=%Rrc\n", State.rc));
                } while (0);
                if (RT_FAILURE(State.rc))
                    RTR0TESTR0_CHECK_RC(RTTimerDestroy(pTimer), VINF_SUCCESS);
            }
            break;
        }
#endif

        case TSTRTR0TIMER_PERIODIC_BASIC:
        case TSTRTR0TIMER_PERIODIC_BASIC_HIRES:
        {
            /* Create a periodic timer running at 10 HZ. */
            uint32_t const  u10HzAsNs    = 100000000;
            uint32_t const  u10HzAsNsMin = u10HzAsNs - u10HzAsNs / 2;
            uint32_t const  u10HzAsNsMax = u10HzAsNs + u10HzAsNs / 2;
            PRTTIMER        pTimer;
            uint32_t        fFlags = uOperation != TSTRTR0TIMER_ONE_SHOT_BASIC_HIRES ? RTTIMER_FLAGS_HIGH_RES : 0;
            RTR0TESTR0_CHECK_RC_BREAK(RTTimerCreateEx(&pTimer, u10HzAsNs, fFlags, tstRTR0TimerCallbackU32Counter, &State),
                                      VINF_SUCCESS);

            for (uint32_t iTest = 0; iTest < 2; iTest++)
            {
                RT_ZERO(State);
                uint64_t uStartNsTS = RTTimeSystemNanoTS();
                RTR0TESTR0_CHECK_RC_BREAK(RTTimerStart(pTimer, u10HzAsNs), VINF_SUCCESS);
                for (uint32_t i = 0; i < 1000 && ASMAtomicUoReadU32(&State.cShots) < 10; i++)
                    RTThreadSleep(10);
                RTR0TESTR0_CHECK_RC_BREAK(RTTimerStop(pTimer), VINF_SUCCESS);
                RTR0TESTR0_CHECK_MSG_BREAK(ASMAtomicUoReadU32(&State.cShots) == 10, ("cShots=%u\n", State.cShots));
                if (tstRTR0TimerCheckShotIntervals(&State, uStartNsTS, u10HzAsNsMin, u10HzAsNsMax))
                    break;
            }
            RTR0TESTR0_CHECK_RC(RTTimerDestroy(pTimer), VINF_SUCCESS);
            RTR0TESTR0_CHECK_RC(RTTimerDestroy(NULL), VINF_SUCCESS);
            break;
        }

        case TSTRTR0TIMER_PERIODIC_CSSD_LOOPS:
        case TSTRTR0TIMER_PERIODIC_CSSD_LOOPS_HIRES:
        {
            /* create, start, stop & destroy high res timers a number of times. */
            uint32_t fFlags = uOperation != TSTRTR0TIMER_PERIODIC_CSSD_LOOPS_HIRES ? RTTIMER_FLAGS_HIGH_RES : 0;
            for (uint32_t i = 0; i < 40; i++)
            {
                PRTTIMER pTimer;
                RTR0TESTR0_CHECK_RC_BREAK(RTTimerCreateEx(&pTimer, cNsSysHz, fFlags, tstRTR0TimerCallbackU32Counter, &State),
                                          VINF_SUCCESS);
                for (uint32_t j = 0; j < 10; j++)
                {
                    RT_ZERO(State);
                    RTR0TESTR0_CHECK_RC_BREAK(RTTimerStart(pTimer, i < 20 ? 0 : cNsSysHz), VINF_SUCCESS);
                    for (uint32_t k = 0; k < 1000 && ASMAtomicUoReadU32(&State.cShots) < 2; k++)
                        RTThreadSleep(1);
                    RTR0TESTR0_CHECK_RC_BREAK(RTTimerStop(pTimer), VINF_SUCCESS);
                }
                RTR0TESTR0_CHECK_RC(RTTimerDestroy(pTimer), VINF_SUCCESS);
            }
            break;
        }

    }

    RTR0TESTR0_SRV_REQ_EPILOG(pReqHdr);
    /* The error indicator is the '!' in the message buffer. */
    return VINF_SUCCESS;
}

