/* $Id$ */
/** @file
 * VBoxModBallooning - Module for handling the automatic ballooning of VMs.
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifndef VBOX_ONLY_DOCS
# include <VBox/com/errorprint.h>
#endif /* !VBOX_ONLY_DOCS */

#include "VBoxWatchdogInternal.h"
#include <iprt/system.h>

using namespace com;

#define VBOX_MOD_BALLOONING_NAME "balloonctrl"

/**
 * The module's RTGetOpt-IDs for the command line.
 */
enum GETOPTDEF_BALLOONCTRL
{
    GETOPTDEF_BALLOONCTRL_BALLOONINC = 2000,
    GETOPTDEF_BALLOONCTRL_BALLOONDEC,
    GETOPTDEF_BALLOONCTRL_BALLOONLOWERLIMIT,
    GETOPTDEF_BALLOONCTRL_BALLOONMAX,
    GETOPTDEF_BALLOONCTRL_BALLOONSAFETY,
    GETOPTDEF_BALLOONCTRL_TIMEOUTMS,
    GETOPTDEF_BALLOONCTRL_GROUPS
};

/**
 * The module's command line arguments.
 */
static const RTGETOPTDEF g_aBalloonOpts[] = {
    { "--balloon-dec",            GETOPTDEF_BALLOONCTRL_BALLOONDEC,        RTGETOPT_REQ_UINT32 },
    { "--balloon-groups",         GETOPTDEF_BALLOONCTRL_GROUPS,            RTGETOPT_REQ_STRING },
    { "--balloon-inc",            GETOPTDEF_BALLOONCTRL_BALLOONINC,        RTGETOPT_REQ_UINT32 },
    { "--balloon-interval",       GETOPTDEF_BALLOONCTRL_TIMEOUTMS,         RTGETOPT_REQ_UINT32 },
    { "--balloon-lower-limit",    GETOPTDEF_BALLOONCTRL_BALLOONLOWERLIMIT, RTGETOPT_REQ_UINT32 },
    { "--balloon-max",            GETOPTDEF_BALLOONCTRL_BALLOONMAX,        RTGETOPT_REQ_UINT32 },
    { "--balloon-safety-margin",  GETOPTDEF_BALLOONCTRL_BALLOONSAFETY,     RTGETOPT_REQ_UINT32 }
};

static unsigned long g_ulMemoryBalloonTimeoutMS = 0;
static unsigned long g_ulMemoryBalloonIncrementMB = 0;
static unsigned long g_ulMemoryBalloonDecrementMB = 0;
/** Global balloon limit is 0, so disabled. Can be overridden by a per-VM
 *  "VBoxInternal/Guest/BalloonSizeMax" value. */
static unsigned long g_ulMemoryBalloonMaxMB = 0;
static unsigned long g_ulMemoryBalloonLowerLimitMB = 0;
static unsigned long g_ulMemoryBalloonSafetyMB = _1K;

/** The ballooning module's payload. */
typedef struct VBOXWATCHDOG_BALLOONCTRL_PAYLOAD
{
    /** The maximum ballooning size for the VM.
     *  Specify 0 for ballooning disabled. */
    unsigned long ulBalloonSizeMax;
} VBOXWATCHDOG_BALLOONCTRL_PAYLOAD, *PVBOXWATCHDOG_BALLOONCTRL_PAYLOAD;


/**
 * Retrieves the current delta value
 *
 * @return  long                                Delta (MB) of the balloon to be deflated (<0) or inflated (>0).
 * @param   ulCurrentDesktopBalloonSize         The balloon's current size.
 * @param   ulDesktopFreeMemory                 The VM's current free memory.
 * @param   ulMaxBalloonSize                    The maximum balloon size (MB) it can inflate to.
 */
static long balloonGetDelta(unsigned long ulCurrentDesktopBalloonSize,
                            unsigned long ulDesktopFreeMemory, unsigned long ulMaxBalloonSize)
{
    if (ulCurrentDesktopBalloonSize > ulMaxBalloonSize)
        return (ulMaxBalloonSize - ulCurrentDesktopBalloonSize);

    long lBalloonDelta = 0;
    if (ulDesktopFreeMemory < g_ulMemoryBalloonLowerLimitMB)
    {
        /* Guest is running low on memory, we need to
         * deflate the balloon. */
        lBalloonDelta = (g_ulMemoryBalloonDecrementMB * -1);

        /* Ensure that the delta will not return a negative
         * balloon size. */
        if ((long)ulCurrentDesktopBalloonSize + lBalloonDelta < 0)
            lBalloonDelta = 0;
    }
    else if (ulMaxBalloonSize > ulCurrentDesktopBalloonSize)
    {
        /* We want to inflate the balloon if we have room. */
        long lIncrement = g_ulMemoryBalloonIncrementMB;
        while (lIncrement >= 16 && (ulDesktopFreeMemory - lIncrement) < g_ulMemoryBalloonLowerLimitMB)
        {
            lIncrement = (lIncrement / 2);
        }

        if ((ulDesktopFreeMemory - lIncrement) > g_ulMemoryBalloonLowerLimitMB)
            lBalloonDelta = lIncrement;
    }
    if (ulCurrentDesktopBalloonSize + lBalloonDelta > ulMaxBalloonSize)
        lBalloonDelta = (ulMaxBalloonSize - ulCurrentDesktopBalloonSize);

    /* Limit the ballooning to the available memory, leaving some free.
     * If anything fails clamp the delta to 0. */
    if (lBalloonDelta < 0)
    {
        uint64_t cbSafety = (uint64_t)g_ulMemoryBalloonSafetyMB * _1M;
        uint64_t cbHostRamAvail = 0;
        int vrc = RTSystemQueryAvailableRam(&cbHostRamAvail);
        if (RT_SUCCESS(vrc))
        {
            if (cbHostRamAvail < cbSafety)
                lBalloonDelta = 0;
            else if ((uint64_t)(-lBalloonDelta) > (cbHostRamAvail - cbSafety) / _1M)
                lBalloonDelta = -(long)((cbHostRamAvail - cbSafety) / _1M);
        }
        else
            lBalloonDelta = 0;
    }

    return lBalloonDelta;
}

/**
 * Determines the maximum balloon size to set for the specified machine.
 *
 * @return  unsigned long           Balloon size (in MB) to set, 0 if no ballooning required.
 * @param   rptrMachine             Pointer to interface of specified machine.
 */
static unsigned long balloonGetMaxSize(const ComPtr<IMachine> &rptrMachine)
{
    /*
     * Try to retrieve the balloon maximum size via the following order:
     *  - command line parameter ("--balloon-max")
     *  Legacy (VBoxBalloonCtrl):
     *  - per-VM parameter ("VBoxInternal/Guest/BalloonSizeMax")
     *  Global:
     *  - global parameter ("VBoxInternal/Guest/BalloonSizeMax")
     *  New:
     *  - per-VM parameter ("VBoxInternal2/Watchdog/BalloonCtrl/BalloonSizeMax")
     *
     *  By default (e.g. if none of above is set), ballooning is disabled.
     */
    unsigned long ulBalloonMax = g_ulMemoryBalloonMaxMB;
    if (!ulBalloonMax)
    {
        int vrc = cfgGetValueULong(g_pVirtualBox, rptrMachine,
                                  "VBoxInternal/Guest/BalloonSizeMax", "VBoxInternal/Guest/BalloonSizeMax", &ulBalloonMax, 0 /* Ballooning disabled */);
        if (RT_FAILURE(vrc))
        {
            /* Try (new) VBoxWatch per-VM approach. */
            Bstr strValue;
            HRESULT rc = rptrMachine->GetExtraData(Bstr("VBoxInternal2/Watchdog/BalloonCtrl/BalloonSizeMax").raw(),
                                                   strValue.asOutParam());
            if (   SUCCEEDED(rc)
                && !strValue.isEmpty())
            {
                ulBalloonMax = Utf8Str(strValue).toUInt32();
            }
        }
    }

    return ulBalloonMax;
}

/**
 * Indicates whether ballooning on the specified machine state is
 * possible -- this only is true if the machine is up and running.
 *
 * @return  bool            Flag indicating whether the VM is running or not.
 * @param   enmState        The VM's machine state to judge whether it's running or not.
 */
static bool balloonIsPossible(MachineState_T enmState)
{
    switch (enmState)
    {
        case MachineState_Running:
#if 0
        /* Not required for ballooning. */
        case MachineState_Teleporting:
        case MachineState_LiveSnapshotting:
        case MachineState_Paused:
        case MachineState_TeleportingPausedVM:
#endif
            return true;
        default:
            break;
    }
    return false;
}

/**
 * Determines whether ballooning is required to the specified machine.
 *
 * @return  bool                    True if ballooning is required, false if not.
 * @param   pMachine                Machine to determine ballooning for.
 */
static bool balloonIsRequired(PVBOXWATCHDOG_MACHINE pMachine)
{
    AssertPtrReturn(pMachine, false);

    /* Only do ballooning if we have a maximum balloon size set. */
    PVBOXWATCHDOG_BALLOONCTRL_PAYLOAD pData = (PVBOXWATCHDOG_BALLOONCTRL_PAYLOAD)
                                              payloadFrom(pMachine, VBOX_MOD_BALLOONING_NAME);
    AssertPtr(pData);
    pData->ulBalloonSizeMax = pMachine->machine.isNull()
                              ? 0 : balloonGetMaxSize(pMachine->machine);

    /** @todo Add grouping as a criteria! */

    return pData->ulBalloonSizeMax ? true : false;
}

int balloonMachineSetup(const Bstr& strUuid)
{
    int vrc = VINF_SUCCESS;

    do
    {
        PVBOXWATCHDOG_MACHINE pMachine = getMachine(strUuid);
        AssertPtrBreakStmt(pMachine, vrc=VERR_INVALID_PARAMETER);

        ComPtr<IMachine> m = pMachine->machine;

        /*
         * Setup metrics required for ballooning.
         */
        com::SafeArray<BSTR> metricNames(1);
        com::SafeIfaceArray<IUnknown> metricObjects(1);
        com::SafeIfaceArray<IPerformanceMetric> metricAffected;

        Bstr strMetricNames(L"Guest/RAM/Usage");
        strMetricNames.cloneTo(&metricNames[0]);

        HRESULT rc = m.queryInterfaceTo(&metricObjects[0]);

#ifdef VBOX_WATCHDOG_GLOBAL_PERFCOL
        CHECK_ERROR_BREAK(g_pPerfCollector, SetupMetrics(ComSafeArrayAsInParam(metricNames),
                                                         ComSafeArrayAsInParam(metricObjects),
                                                         5 /* 5 seconds */,
                                                         1 /* One sample is enough */,
                                                         ComSafeArrayAsOutParam(metricAffected)));
#else
        ComPtr<IPerformanceCollector> coll = pMachine->collector;

        CHECK_ERROR_BREAK(g_pVirtualBox, COMGETTER(PerformanceCollector)(coll.asOutParam()));
        CHECK_ERROR_BREAK(coll, SetupMetrics(ComSafeArrayAsInParam(metricNames),
                                             ComSafeArrayAsInParam(metricObjects),
                                             5 /* 5 seconds */,
                                             1 /* One sample is enough */,
                                             ComSafeArrayAsOutParam(metricAffected)));
#endif
        if (FAILED(rc))
            vrc = VERR_COM_IPRT_ERROR; /* @todo Find better rc! */

    } while (0);

    return vrc;
}

/**
 * Does the actual ballooning and assumes the machine is
 * capable and ready for ballooning.
 *
 * @return  IPRT status code.
 * @param   strUuid                 UUID of the specified machine.
 * @param   pMachine                Pointer to the machine's internal structure.
 */
static int balloonMachineUpdate(const Bstr &strUuid, PVBOXWATCHDOG_MACHINE pMachine)
{
    AssertPtrReturn(pMachine, VERR_INVALID_POINTER);

    /*
     * Get metrics collected at this point.
     */
    LONG lMemFree, lBalloonCur;
    int vrc = getMetric(pMachine, L"Guest/RAM/Usage/Free", &lMemFree);
    if (RT_SUCCESS(vrc))
        vrc = getMetric(pMachine, L"Guest/RAM/Usage/Balloon", &lBalloonCur);

    if (RT_SUCCESS(vrc))
    {
        /* If guest statistics are not up and running yet, skip this iteration
         * and try next time. */
        if (lMemFree <= 0)
        {
#ifdef DEBUG
            serviceLogVerbose(("%ls: No metrics available yet!\n", strUuid.raw()));
#endif
            return VINF_SUCCESS;
        }

        lMemFree /= 1024;
        lBalloonCur /= 1024;

        PVBOXWATCHDOG_BALLOONCTRL_PAYLOAD pData = (PVBOXWATCHDOG_BALLOONCTRL_PAYLOAD)
                                                  payloadFrom(pMachine, VBOX_MOD_BALLOONING_NAME);
        AssertPtr(pData);

        serviceLogVerbose(("%ls: Balloon: %ld, Free mem: %ld, Max ballon: %ld\n",
                           strUuid.raw(),
                           lBalloonCur, lMemFree, pData->ulBalloonSizeMax));

        /* Calculate current balloon delta. */
        long lDelta = balloonGetDelta(lBalloonCur, lMemFree, pData->ulBalloonSizeMax);
        if (lDelta) /* Only do ballooning if there's really smth. to change ... */
        {
            lBalloonCur = lBalloonCur + lDelta;
            Assert(lBalloonCur > 0);

            serviceLog("%ls: %s balloon by %ld to %ld ...\n",
                       strUuid.raw(),
                       lDelta > 0 ? "Inflating" : "Deflating", lDelta, lBalloonCur);

            if (!g_fDryrun)
            {
                /* Open a session for the VM. */
                HRESULT rc;
                CHECK_ERROR(pMachine->machine, LockMachine(g_pSession, LockType_Shared));

                do
                {
                    /* Get the associated console. */
                    ComPtr<IConsole> console;
                    CHECK_ERROR_BREAK(g_pSession, COMGETTER(Console)(console.asOutParam()));

                    ComPtr <IGuest> guest;
                    rc = console->COMGETTER(Guest)(guest.asOutParam());
                    if (SUCCEEDED(rc))
                        CHECK_ERROR_BREAK(guest, COMSETTER(MemoryBalloonSize)(lBalloonCur));
                    else
                        serviceLog("Error: Unable to set new balloon size %ld for machine \"%ls\", rc=%Rhrc",
                                   lBalloonCur, strUuid.raw(), rc);
                    if (FAILED(rc))
                        vrc = VERR_COM_IPRT_ERROR;
                } while (0);

                /* Unlock the machine again. */
                g_pSession->UnlockMachine();
            }
        }
    }
    else
        serviceLog("Error: Unable to retrieve metrics for machine \"%ls\", rc=%Rrc",
                   strUuid.raw(), vrc);
    return vrc;
}

/* Callbacks. */
static DECLCALLBACK(int) VBoxModBallooningPreInit(void)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) VBoxModBallooningOption(int argc, char *argv[], int *piConsumed)
{
    if (!argc) /* Take a shortcut. */
        return -1;

    AssertPtrReturn(argv, VERR_INVALID_POINTER);
    AssertPtrReturn(piConsumed, VERR_INVALID_POINTER);

    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, argc, argv,
                          g_aBalloonOpts, RT_ELEMENTS(g_aBalloonOpts),
                          0 /* First */, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return rc;

    rc = 0; /* Set default parsing result to valid. */

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case GETOPTDEF_BALLOONCTRL_BALLOONDEC:
                g_ulMemoryBalloonDecrementMB = ValueUnion.u32;
                break;

            case GETOPTDEF_BALLOONCTRL_BALLOONINC:
                g_ulMemoryBalloonIncrementMB = ValueUnion.u32;
                break;

            case GETOPTDEF_BALLOONCTRL_GROUPS:
                /** @todo Add ballooning groups cmd line arg. */
                break;

            case GETOPTDEF_BALLOONCTRL_BALLOONLOWERLIMIT:
                g_ulMemoryBalloonLowerLimitMB = ValueUnion.u32;
                break;

            case GETOPTDEF_BALLOONCTRL_BALLOONMAX:
                g_ulMemoryBalloonMaxMB = ValueUnion.u32;
                break;

            case GETOPTDEF_BALLOONCTRL_BALLOONSAFETY:
                g_ulMemoryBalloonSafetyMB = ValueUnion.u32;
                break;

            /** @todo This option is a common module option! Put
             *        this into a utility function! */
            case GETOPTDEF_BALLOONCTRL_TIMEOUTMS:
                g_ulMemoryBalloonTimeoutMS = ValueUnion.u32;
                if (g_ulMemoryBalloonTimeoutMS < 500)
                    g_ulMemoryBalloonTimeoutMS = 500;
                break;

            default:
                rc = -1; /* We don't handle this option, skip. */
                break;
        }

        /* At the moment we only process one option at a time. */
        break;
    }

    *piConsumed += GetState.iNext - 1;

    return rc;
}

static DECLCALLBACK(int) VBoxModBallooningInit(void)
{
    if (!g_ulMemoryBalloonTimeoutMS)
        cfgGetValueULong(g_pVirtualBox, NULL /* Machine */,
                         "VBoxInternal2/Watchdog/BalloonCtrl/TimeoutMS", NULL /* Per-machine */,
                         &g_ulMemoryBalloonTimeoutMS, 30 * 1000 /* Default is 30 seconds timeout. */);

    if (!g_ulMemoryBalloonIncrementMB)
        cfgGetValueULong(g_pVirtualBox, NULL /* Machine */,
                         "VBoxInternal2/Watchdog/BalloonCtrl/BalloonIncrementMB", NULL /* Per-machine */,
                         &g_ulMemoryBalloonIncrementMB, 256);

    if (!g_ulMemoryBalloonDecrementMB)
        cfgGetValueULong(g_pVirtualBox, NULL /* Machine */,
                         "VBoxInternal2/Watchdog/BalloonCtrl/BalloonDecrementMB", NULL /* Per-machine */,
                         &g_ulMemoryBalloonDecrementMB, 128);

    if (!g_ulMemoryBalloonLowerLimitMB)
        cfgGetValueULong(g_pVirtualBox, NULL /* Machine */,
                         "VBoxInternal2/Watchdog/BalloonCtrl/BalloonLowerLimitMB", NULL /* Per-machine */,
                         &g_ulMemoryBalloonLowerLimitMB, 128);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) VBoxModBallooningMain(void)
{
    static uint64_t uLast = UINT64_MAX;
    uint64_t uNow = RTTimeProgramMilliTS() / g_ulMemoryBalloonTimeoutMS;
    if (uLast == uNow)
        return VINF_SUCCESS;
    uLast = uNow;

    int rc = VINF_SUCCESS;

    /** @todo Provide API for enumerating/working w/ machines inside a module! */
    mapVMIter it = g_mapVM.begin();
    while (it != g_mapVM.end())
    {
        MachineState_T state = getMachineState(&it->second);

        /* Our actual ballooning criteria. */
        if (   balloonIsPossible(state)
            && balloonIsRequired(&it->second))
        {
            rc = balloonMachineUpdate(it->first /* UUID */,
                                      &it->second /* Machine */);
            AssertRC(rc);
        }
        if (RT_FAILURE(rc))
            break;

        it++;
    }

    return rc;
}

static DECLCALLBACK(int) VBoxModBallooningStop(void)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) VBoxModBallooningTerm(void)
{
}

static DECLCALLBACK(int) VBoxModBallooningOnMachineRegistered(const Bstr &strUuid)
{
    PVBOXWATCHDOG_MACHINE pMachine = getMachine(strUuid);
    AssertPtrReturn(pMachine, VERR_INVALID_PARAMETER);

    PVBOXWATCHDOG_BALLOONCTRL_PAYLOAD pData;
    int rc = payloadAlloc(pMachine, VBOX_MOD_BALLOONING_NAME,
                          sizeof(VBOXWATCHDOG_BALLOONCTRL_PAYLOAD), (void**)&pData);
    if (RT_SUCCESS(rc))
        rc = balloonMachineUpdate(strUuid, pMachine);

    return rc;
}

static DECLCALLBACK(int) VBoxModBallooningOnMachineUnregistered(const Bstr &strUuid)
{
    PVBOXWATCHDOG_MACHINE pMachine = getMachine(strUuid);
    AssertPtrReturn(pMachine, VERR_INVALID_PARAMETER);

    payloadFree(pMachine, VBOX_MOD_BALLOONING_NAME);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) VBoxModBallooningOnMachineStateChanged(const Bstr &strUuid,
                                                                MachineState_T enmState)
{
    PVBOXWATCHDOG_MACHINE pMachine = getMachine(strUuid);
    /* Note: The machine state will change to "setting up" when machine gets deleted,
     *       so pMachine might be NULL here. */
    if (!pMachine)
        return VINF_SUCCESS;

    return balloonMachineUpdate(strUuid, pMachine);
}

static DECLCALLBACK(int) VBoxModBallooningOnServiceStateChanged(bool fAvailable)
{
    return VINF_SUCCESS;
}

/**
 * The 'balloonctrl' module description.
 */
VBOXMODULE g_ModBallooning =
{
    /* pszName. */
    VBOX_MOD_BALLOONING_NAME,
    /* pszDescription. */
    "Memory Ballooning Control",
    /* pszDepends. */
    NULL,
    /* uPriority. */
    0 /* Not used */,
    /* pszUsage. */
    " [--balloon-dec=<MB>] [--balloon-groups=<string>] [--balloon-inc=<MB>]\n"
    " [--balloon-interval=<ms>] [--balloon-lower-limit=<MB>]\n"
    " [--balloon-max=<MB>]\n",
    /* pszOptions. */
    "--balloon-dec          Sets the ballooning decrement in MB (128 MB).\n"
    "--balloon-groups       Sets the VM groups for ballooning (all).\n"
    "--balloon-inc          Sets the ballooning increment in MB (256 MB).\n"
    "--balloon-interval     Sets the check interval in ms (30 seconds).\n"
    "--balloon-lower-limit  Sets the ballooning lower limit in MB (64 MB).\n"
    "--balloon-max          Sets the balloon maximum limit in MB (0 MB).\n"
    "                       Specifying \"0\" means disabled ballooning.\n"
#if 1
    /* (Legacy) note. */
    "Set \"VBoxInternal/Guest/BalloonSizeMax\" for a per-VM maximum ballooning size.\n"
#endif
    ,
    /* methods. */
    VBoxModBallooningPreInit,
    VBoxModBallooningOption,
    VBoxModBallooningInit,
    VBoxModBallooningMain,
    VBoxModBallooningStop,
    VBoxModBallooningTerm,
    /* callbacks. */
    VBoxModBallooningOnMachineRegistered,
    VBoxModBallooningOnMachineUnregistered,
    VBoxModBallooningOnMachineStateChanged,
    VBoxModBallooningOnServiceStateChanged
};

