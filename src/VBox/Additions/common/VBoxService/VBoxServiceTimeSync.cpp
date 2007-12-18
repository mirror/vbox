/** $Id$ */
/** @file
 * VBoxService - Guest Additions TimeSync Service.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_vboxservice_timesync       The Time Sync Service
 *
 * The time sync service plays along with the Time Manager (TM) in the VMM
 * to keep the guest time accurate using the host machine as reference.
 * TM will try its best to make sure all timer ticks gets delivered so that
 * there isn't normally any need to adjust the guest time.
 *
 * There are three normal (= acceptable) cases:
 *      -# When the service starts up. This is because ticks and such might
 *         be lost during VM and OS startup. (Need to figure out exactly why!)
 *      -# When the TM is unable to deliver all the ticks and swallows a
 *         backlog of ticks. The threshold for this is configurable with
 *         a default of 60 seconds.
 *      -# The time is adjusted on the host. This can be caused manually by
 *         the user or by some time sync daemon (NTP, LAN server, etc.).
 *
 * There are a number of very odd case where adjusting is needed. Here
 * are some of them:
 *      -# Timer device emulation inaccurancies (like rounding).
 *      -# Inaccurancies in time source VirtualBox uses.
 *      -# The Guest and/or Host OS doesn't perform proper time keeping. This
 *         come about as a result of OS and/or hardware issues.
 *
 * The TM is our source for the host time and will make adjustments for
 * current timer delivery lag. The simplistic approach taken by TM is to
 * adjust the host time by the current guest timer delivery lag, meaning that
 * if the guest is behind 1 second with PIT/RTC/++ ticks this should be reflected
 * in the guest wall time as well.
 *
 * Now, there is any amount of trouble we can cause by changing the time.
 * Most applications probably uses the wall time when they need to measure
 * things. A walltime that is being juggled about every so often, even if just
 * a little bit, could occationally upset these measurements by for instance
 * yielding negative results.
 *
 * This bottom line here is that the time sync service isn't really supposed
 * to do anything and will try avoid having to do anything when possible.
 *
 * The implementation uses the latency it takes to query host time as the
 * absolute maximum precision to avoid messing up under timer tick catchup
 * and/or heavy host/guest load. (Rational is that a *lot* of stuff may happen
 * on our way back from ring-3 and TM/VMMDev since we're taking the route
 * thru the inner EM loop with it's force action processing.)
 *
 * But this latency has to be measured from our perspective, which means it
 * could just as easily come out as 0. (OS/2 and Windows guest only updates
 * the current time when the timer ticks for instance.) The good thing is
 * that this isn't really a problem since we won't ever do anything unless
 * the drift is noticable.
 *
 * It now boils down to these three (configuration) factors:
 *  -# g_TimesyncMinAdjust - The minimum drift we will ever bother with.
 *  -# g_TimesyncLatencyFactor - The factor we multiply the latency by to
 *     calculate the dynamic minimum adjust factor.
 *  -# g_TimesyncMaxLatency - When to start discarding the data as utterly
 *     useless and take a rest (someone is too busy to give us good data).
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifdef RT_OS_WINDOWS
#else
# include <unistd.h>
# include <errno.h>
# include <time.h>
# include <sys/time.h>
#endif

#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/time.h>
#include <iprt/assert.h>
#include <VBox/VBoxGuest.h>
#include "VBoxServiceInternal.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The timesync interval (millseconds). */
uint32_t g_TimeSyncInterval = 0;
/**
 * @see pg_vboxservice_timesync
 *
 * @remark  OS/2: There is either a 1 second resolution on the DosSetDateTime
 *                API or a but in the settimeofday implementation. Thus, don't
 *                bother unless there is at least a 1 second drift.
 */
#ifdef RT_OS_OS2
static uint32_t g_TimeSyncMinAdjust = 1000;
#else
static uint32_t g_TimeSyncMinAdjust = 100;
#endif
/** @see pg_vboxservice_timesync */
static uint32_t g_TimeSyncLatencyFactor = 8;
/** @see pg_vboxservice_timesync */
static uint32_t g_TimeSyncMaxLatency = 250;

/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI g_TimeSyncEvent = NIL_RTSEMEVENTMULTI;


/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServiceTimeSyncPreInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnOption */
static DECLCALLBACK(int) VBoxServiceTimeSyncOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    int rc = -1;
    if (ppszShort)
        /* no short options */;
    else if (!strcmp(argv[*pi], "--timesync-interval"))
        rc = VBoxServiceArgUInt32(argc, argv, "", pi,
                                  &g_TimeSyncInterval, 1, UINT32_MAX - 1);
    else if (!strcmp(argv[*pi], "--timesync-min-adjust"))
        rc = VBoxServiceArgUInt32(argc, argv, "", pi,
                                  &g_TimeSyncMinAdjust, 0, 3600000);
    else if (!strcmp(argv[*pi], "--timesync-latency-factor"))
        rc = VBoxServiceArgUInt32(argc, argv, "", pi,
                                  &g_TimeSyncLatencyFactor, 1, 1024);
    else if (!strcmp(argv[*pi], "--timesync-max-latency"))
        rc = VBoxServiceArgUInt32(argc, argv, "", pi,
                                  &g_TimeSyncMaxLatency, 1, 3600000);
    return rc;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceTimeSyncInit(void)
{
    /*
     * If not specified, find the right interval default.
     * Then create the event sem to block on.
     */
    if (!g_TimeSyncInterval)
        g_TimeSyncInterval = g_DefaultInterval * 1000;
    if (!g_TimeSyncInterval)
        g_TimeSyncInterval = 10 * 1000;

    int rc = RTSemEventMultiCreate(&g_TimeSyncEvent);
    AssertRC(rc);
    return rc;
}


/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceTimeSyncWorker(bool volatile *pfShutdown)
{
    RTTIME Time;
    char sz[64];
    int rc;

    unsigned cErrors = 0;
    for (;;)
    {
        /*
         * Try get a reliable time reading.
         */
        int cTries = 3;
        do
        {
            /* query it. */
            RTTIMESPEC GuestNow0, GuestNow, HostNow;
            RTTimeNow(&GuestNow0);
            int rc2 = VbglR3GetHostTime(&HostNow);
            if (RT_FAILURE(rc2))
            {
                if (cErrors++ < 10)
                    VBoxServiceError("VbglR3GetHostTime failed; rc2=%Rrc\n", rc2);
                break;
            }
            RTTimeNow(&GuestNow);

            /* calc latency and check if it's ok. */
            RTTIMESPEC GuestElapsed = GuestNow;
            RTTimeSpecSub(&GuestElapsed, &GuestNow0);
            if ((uint32_t)RTTimeSpecGetMilli(&GuestElapsed) < g_TimeSyncMaxLatency)
            {
                /*
                 * Calculate the adjustment threshold and the current drift.
                 */
                uint32_t MinAdjust = RTTimeSpecGetMilli(&GuestElapsed) * g_TimeSyncLatencyFactor;
                if (MinAdjust < g_TimeSyncMinAdjust)
                    MinAdjust = g_TimeSyncMinAdjust;

                RTTIMESPEC Drift = HostNow;
                RTTimeSpecSub(&Drift, &GuestNow);
                if (RTTimeSpecGetMilli(&Drift) < 0)
                    MinAdjust += g_TimeSyncMinAdjust; /* extra buffer against moving time backwards. */

                RTTIMESPEC AbsDrift = Drift;
                RTTimeSpecAbsolute(&AbsDrift);
                if (g_cVerbosity >= 3)
                {
                    VBoxServiceVerbose(3, "Host:    %s    (MinAdjust: %RU32 ms)\n",
                                       RTTimeToString(RTTimeExplode(&Time, &HostNow), sz, sizeof(sz)), MinAdjust);
                    VBoxServiceVerbose(3, "Guest: - %s => %RDtimespec drift\n",
                                       RTTimeToString(RTTimeExplode(&Time, &GuestNow), sz, sizeof(sz)),
                                       &Drift);
                }
                if (RTTimeSpecGetMilli(&AbsDrift) > MinAdjust)
                {
                    /*
                     * The drift is to big, we have to make adjustments. :-/
                     * If we've got adjtime around, try that first - most
                     * *NIX systems have it. Fall back on settimeofday.
                     */
#ifdef RT_OS_WINDOWS
                    /* just make sure it compiles for now, but later:
                     SetSystemTimeAdjustment and fall back on SetSystemTime.
                     */
                    AssertFatalFailed();
#else
                    struct timeval tv;
# if !defined(RT_OS_OS2) /* PORTME */
                    RTTimeSpecGetTimeval(&Drift, &tv);
                    if (adjtime(&tv, NULL) == 0)
                    {
                        if (g_cVerbosity >= 1)
                            VBoxServiceVerbose(1, "adjtime by %RDtimespec\n", &Drift);
                        cErrors = 0;
                    }
                    else
# endif
                    {
                        errno = 0;
                        if (!gettimeofday(&tv, NULL))
                        {
                            RTTIMESPEC Tmp;
                            RTTimeSpecAdd(RTTimeSpecSetTimeval(&Tmp, &tv), &Drift);
                            if (!settimeofday(RTTimeSpecGetTimeval(&Tmp, &tv), NULL))
                            {
                                if (g_cVerbosity >= 1)
                                    VBoxServiceVerbose(1, "settimeofday to %s\n",
                                                       RTTimeToString(RTTimeExplode(&Time, &Tmp), sz, sizeof(sz)));
# ifdef DEBUG
                                if (g_cVerbosity >= 3)
                                    VBoxServiceVerbose(2, "       new time %s\n",
                                                       RTTimeToString(RTTimeExplode(&Time, RTTimeNow(&Tmp)), sz, sizeof(sz)));
# endif
                                cErrors = 0;
                            }
                            else if (cErrors++ < 10)
                                VBoxServiceError("settimeofday failed; errno=%d: %s\n", errno, strerror(errno));
                        }
                        else if (cErrors++ < 10)
                            VBoxServiceError("gettimeofday failed; errno=%d: %s\n", errno, strerror(errno));
                    }
#endif /* !RT_OS_WINDOWS */
                }
                break;
            }
            VBoxServiceVerbose(3, "%RDtimespec: latency too high (%RDtimespec) sleeping 1s\n", GuestElapsed);
            RTThreadSleep(1000);
        } while (--cTries > 0);

        /*
         * Block for a while.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
        int rc2 = RTSemEventMultiWait(g_TimeSyncEvent, g_TimeSyncInterval);
        if (*pfShutdown)
            break;
        if (rc2 != VERR_TIMEOUT && RT_FAILURE(rc2))
        {
            VBoxServiceError("RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
            rc = rc2;
            break;
        }
    }

    RTSemEventMultiDestroy(g_TimeSyncEvent);
    g_TimeSyncEvent = NIL_RTSEMEVENTMULTI;
    return rc;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceTimeSyncStop(void)
{
    RTSemEventMultiSignal(g_TimeSyncEvent);
}


/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceTimeSyncTerm(void)
{
    RTSemEventMultiDestroy(g_TimeSyncEvent);
    g_TimeSyncEvent = NIL_RTSEMEVENTMULTI;
}


/**
 * The 'timesync' service description.
 */
VBOXSERVICE g_TimeSync =
{
    /* pszName. */
    "timesync",
    /* pszDescription. */
    "Time synchronization",
    /* pszUsage. */
    "[--timesync-interval <ms>] [--timesync-min-adjust <ms>] "
    "[--timesync-latency-factor <x>] [--time-sync-max-latency <ms>]"
    ,
    /* pszOptions. */
    "    --timesync-interval Specifies the interval at which to synchronize the\n"
    "                        time with the host. The default is 10000 ms.\n"
    "    --timesync-min-adjust      The minimum absolute drift drift value measured\n"
    "                        in milliseconds to make adjustments for.\n"
    "                        The default is 1000 ms on OS/2 and 100 ms elsewhere.\n"
    "    --timesync-latency-factor  The factor to multiply the time query latency\n"
    "                        with to calculate the dynamic minimum adjust time.\n"
    "                        The default is 8 times.\n"
    "    --timesync-max-latency     The max host timer query latency to accept.\n"
    "                        The default is 250 ms.\n"
    ,
    /* methods */
    VBoxServiceTimeSyncPreInit,
    VBoxServiceTimeSyncOption,
    VBoxServiceTimeSyncInit,
    VBoxServiceTimeSyncWorker,
    VBoxServiceTimeSyncStop,
    VBoxServiceTimeSyncTerm
};

