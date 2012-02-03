/* $Id$ */
/** @file
 * VBoxModBallooning - Module for handling the automatic ballooning of VMs.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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
# include <VBox/com/com.h>
# include <VBox/com/string.h>
# include <VBox/com/Guid.h>
# include <VBox/com/array.h>
# include <VBox/com/ErrorInfo.h>
# include <VBox/com/errorprint.h>

# include <VBox/com/EventQueue.h>
# include <VBox/com/listeners.h>
# include <VBox/com/VirtualBox.h>
#endif /* !VBOX_ONLY_DOCS */

#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/version.h>

#include <package-generated.h>

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/critsect.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/system.h>

#include <map>
#include <string>
#include <signal.h>

#include "VBoxWatchdogInternal.h"

using namespace com;

#define VBOX_MOD_BALLOONING_NAME "balloonctrl"

/**
 * The module's RTGetOpt-IDs for the command line.
 */
enum GETOPTDEF_BALLOONCTRL
{
    GETOPTDEF_BALLOONCTRL_BALLOOINC = 1000,
    GETOPTDEF_BALLOONCTRL_BALLOONDEC,
    GETOPTDEF_BALLOONCTRL_BALLOONLOWERLIMIT,
    GETOPTDEF_BALLOONCTRL_BALLOONMAX,
    GETOPTDEF_BALLOONCTRL_TIMEOUTMS,
    GETOPTDEF_BALLOONCTRL_GROUPS
};

/**
 * The module's command line arguments.
 */
static const RTGETOPTDEF g_aBalloonOpts[] = {
    { "--balloon-dec",            GETOPTDEF_BALLOONCTRL_BALLOONDEC,        RTGETOPT_REQ_UINT32 },
    { "--balloon-groups",         GETOPTDEF_BALLOONCTRL_GROUPS,            RTGETOPT_REQ_STRING },
    { "--balloon-inc",            GETOPTDEF_BALLOONCTRL_BALLOOINC,         RTGETOPT_REQ_UINT32 },
    { "--balloon-interval",       GETOPTDEF_BALLOONCTRL_TIMEOUTMS,         RTGETOPT_REQ_UINT32 },
    { "--balloon-lower-limit",    GETOPTDEF_BALLOONCTRL_BALLOONLOWERLIMIT, RTGETOPT_REQ_UINT32 },
    { "--balloon-max",            GETOPTDEF_BALLOONCTRL_BALLOONMAX,        RTGETOPT_REQ_UINT32 }
};

static unsigned long g_ulMemoryBalloonTimeoutMS = 30 * 1000; /* Default is 30 seconds timeout. */
static unsigned long g_ulMemoryBalloonIncrementMB = 256;
static unsigned long g_ulMemoryBalloonDecrementMB = 128;
/** Global balloon limit is 0, so disabled. Can be overridden by a per-VM
 *  "VBoxInternal/Guest/BalloonSizeMax" value. */
static unsigned long g_ulMemoryBalloonMaxMB = 0;
static unsigned long g_ulMemoryBalloonLowerLimitMB = 64;

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
    unsigned long ulBalloonMax = g_ulMemoryBalloonMaxMB; /* Use global limit as default. */
    if (!ulBalloonMax) /* Not set by command line? */
    {
        /* Try per-VM approach. */
        Bstr strValue;
        HRESULT rc = rptrMachine->GetExtraData(Bstr("VBoxInternal/Guest/BalloonSizeMax").raw(),
                                               strValue.asOutParam());
        if (   SUCCEEDED(rc)
            && !strValue.isEmpty())
        {
            ulBalloonMax = Utf8Str(strValue).toUInt32();
        }
    }
    if (!ulBalloonMax) /* Still not set by per-VM value? */
    {
        /* Try global approach. */
        Bstr strValue;
        HRESULT rc = g_pVirtualBox->GetExtraData(Bstr("VBoxInternal/Guest/BalloonSizeMax").raw(),
                                                 strValue.asOutParam());
        if (   SUCCEEDED(rc)
            && !strValue.isEmpty())
        {
            ulBalloonMax = Utf8Str(strValue).toUInt32();
        }
    }
    if (!ulBalloonMax)
    {
        /** @todo ("VBoxInternal2/Watchdog/BalloonCtrl/BalloonSizeMax") */
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
                                              getPayload(pMachine, VBOX_MOD_BALLOONING_NAME);
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
                                                  getPayload(pMachine, VBOX_MOD_BALLOONING_NAME);
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

            HRESULT rc;

            /* Open a session for the VM. */
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
            } while (0);

            /* Unlock the machine again. */
            g_pSession->UnlockMachine();
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

static DECLCALLBACK(int) VBoxModBallooningOption(int argc, char **argv)
{
    if (!argc) /* Take a shortcut. */
        return -1;

    AssertPtrReturn(argv, VERR_INVALID_PARAMETER);

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

            case GETOPTDEF_BALLOONCTRL_BALLOOINC:
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

            /** @todo Add (legacy) "--inverval" setting! */
            /** @todo This option is a common moudle option! Put
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
    }

    return rc;
}

static DECLCALLBACK(int) VBoxModBallooningInit(void)
{
    return VINF_SUCCESS; /* Nothing to do here right now. */
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
    AssertPtrReturn(pMachine, VERR_INVALID_PARAMETER);

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

