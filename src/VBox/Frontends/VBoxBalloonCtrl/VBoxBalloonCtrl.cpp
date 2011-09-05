/* $Id$ */
/** @file
 * VBoxBalloonCtrl - VirtualBox Ballooning Control Service.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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

#include "VBoxBalloonCtrl.h"

using namespace com;

/* When defined, use a global performance collector instead
 * of a per-machine based one. */
#define VBOX_BALLOONCTRL_GLOBAL_PERFCOL

/** The critical section for keep our stuff in sync. */
static RTCRITSECT g_MapCritSect;

/** Set by the signal handler. */
static volatile bool g_fCanceled = false;

static uint32_t      g_cHistory = 10;                   /* Enable log rotation, 10 files. */
static uint32_t      g_uHistoryFileTime = RT_SEC_1DAY;  /* Max 1 day per file. */
static uint64_t      g_uHistoryFileSize = 100 * _1M;    /* Max 100MB per file. */

static bool          g_fVerbose = false;

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
/** Run in background. */
static bool          g_fDaemonize = false;
#endif

/**
 * RTGetOpt-IDs for the command line.
 */
enum GETOPTDEF_BALLOONCTRL
{
    GETOPTDEF_BALLOONCTRL_BALLOOINC = 1000,
    GETOPTDEF_BALLOONCTRL_BALLOONDEC,
    GETOPTDEF_BALLOONCTRL_BALLOONLOWERLIMIT,
    GETOPTDEF_BALLOONCTRL_BALLOONMAX
};

/**
 * Command line arguments.
 */
static const RTGETOPTDEF g_aOptions[] = {
#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
    { "--background",           'b',                                       RTGETOPT_REQ_NOTHING },
#endif
    /** For displayHelp(). */
    { "--help",                 'h',                                       RTGETOPT_REQ_NOTHING },
    /** Sets g_ulTimeoutMS. */
    { "--interval",             'i',                                       RTGETOPT_REQ_INT32 },
    /** Sets g_ulMemoryBalloonIncrementMB. */
    { "--balloon-inc",          GETOPTDEF_BALLOONCTRL_BALLOOINC,           RTGETOPT_REQ_INT32 },
    /** Sets g_ulMemoryBalloonDecrementMB. */
    { "--balloon-dec",          GETOPTDEF_BALLOONCTRL_BALLOONDEC,          RTGETOPT_REQ_INT32 },
    { "--balloon-lower-limit",  GETOPTDEF_BALLOONCTRL_BALLOONLOWERLIMIT,   RTGETOPT_REQ_INT32 },
    /** Global max. balloon limit. */
    { "--balloon-max",          GETOPTDEF_BALLOONCTRL_BALLOONMAX,          RTGETOPT_REQ_INT32 },
    { "--verbose",              'v',                                       RTGETOPT_REQ_NOTHING },
    { "--pidfile",              'P',                                       RTGETOPT_REQ_STRING },
    { "--logfile",              'F',                                       RTGETOPT_REQ_STRING },
    { "--logrotate",            'R',                                       RTGETOPT_REQ_UINT32 },
    { "--logsize",              'S',                                       RTGETOPT_REQ_UINT64 },
    { "--loginterval",          'I',                                       RTGETOPT_REQ_UINT32 }
};

static unsigned long g_ulTimeoutMS = 30 * 1000; /* Default is 30 seconds timeout. */
static unsigned long g_ulMemoryBalloonIncrementMB = 256;
static unsigned long g_ulMemoryBalloonDecrementMB = 128;
/** Global balloon limit is 0, so disabled. Can be overridden by a per-VM
 *  "VBoxInternal/Guest/BalloonSizeMax" value. */
static unsigned long g_ulMemoryBalloonMaxMB = 0;
static unsigned long g_ulLowerMemoryLimitMB = 64;

/** Global objects. */
static ComPtr<IVirtualBoxClient> g_pVirtualBoxClient = NULL;
static ComPtr<IVirtualBox> g_pVirtualBox = NULL;
static ComPtr<ISession> g_pSession = NULL;
static ComPtr<IEventSource> g_pEventSource = NULL;
static ComPtr<IEventSource> g_pEventSourceClient = NULL;
static ComPtr<IEventListener> g_pVBoxEventListener = NULL;
# ifdef VBOX_BALLOONCTRL_GLOBAL_PERFCOL
static ComPtr<IPerformanceCollector> g_pPerfCollector = NULL;
# endif
static EventQueue *g_pEventQ = NULL;

/** A machine's internal entry. */
typedef struct VBOXBALLOONCTRL_MACHINE
{
    ComPtr<IMachine> machine;
    unsigned long ulBalloonSizeMax;
#ifndef VBOX_BALLOONCTRL_GLOBAL_PERFCOL
    ComPtr<IPerformanceCollector> collector;
#endif
} VBOXBALLOONCTRL_MACHINE, *PVBOXBALLOONCTRL_MACHINE;
typedef std::map<Bstr, VBOXBALLOONCTRL_MACHINE> mapVM;
typedef std::map<Bstr, VBOXBALLOONCTRL_MACHINE>::iterator mapVMIter;
typedef std::map<Bstr, VBOXBALLOONCTRL_MACHINE>::const_iterator mapVMIterConst;
static mapVM g_mapVM;

/* Prototypes. */
#define serviceLogVerbose(a) if (g_fVerbose) { serviceLog a; }
static void serviceLog(const char *pszFormat, ...);

static bool machineIsRunning(MachineState_T enmState);
static bool machineHandled(const Bstr &strUuid);
static int machineAdd(const Bstr &strUuid);
static int machineRemove(const Bstr &strUuid);
static int machineUpdate(const Bstr &strUuid, MachineState_T enmState);

static unsigned long balloonGetMaxSize(const ComPtr<IMachine> &rptrMachine);
static bool balloonIsRequired(PVBOXBALLOONCTRL_MACHINE pMachine);
static int balloonUpdate(const Bstr &strUuid, PVBOXBALLOONCTRL_MACHINE pMachine);

static HRESULT balloonCtrlSetup();
static void balloonCtrlShutdown();

static HRESULT createGlobalObjects();
static void deleteGlobalObjects();

#ifdef RT_OS_WINDOWS
/* Required for ATL. */
static CComModule _Module;
#endif

/**
 *  Handler for global events.
 */
class VirtualBoxEventListener
{
    public:
        VirtualBoxEventListener()
        {
        }

        virtual ~VirtualBoxEventListener()
        {
        }

        HRESULT init()
        {
            return S_OK;
        }

        void uninit()
        {
        }

        STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent *aEvent)
        {
            switch (aType)
            {
                case VBoxEventType_OnMachineRegistered:
                {
                    ComPtr<IMachineRegisteredEvent> pEvent = aEvent;
                    Assert(pEvent);

                    Bstr uuid;
                    BOOL fRegistered;
                    HRESULT hr = pEvent->COMGETTER(Registered)(&fRegistered);
                    if (SUCCEEDED(hr))
                        hr = pEvent->COMGETTER(MachineId)(uuid.asOutParam());

                    if (SUCCEEDED(hr))
                    {
                        int rc = RTCritSectEnter(&g_MapCritSect);
                        if (RT_SUCCESS(rc))
                        {
                            if (fRegistered && machineHandled(uuid))
                                rc = machineAdd(uuid);
                            else if (!fRegistered)
                                 rc = machineRemove(uuid);

                            int rc2 = RTCritSectLeave(&g_MapCritSect);
                            if (RT_SUCCESS(rc))
                                rc = rc2;
                            AssertRC(rc);
                        }
                    }
                    break;
                }

                case VBoxEventType_OnMachineStateChanged:
                {
                    ComPtr<IMachineStateChangedEvent> pEvent = aEvent;
                    Assert(pEvent);

                    MachineState_T machineState;
                    Bstr uuid;

                    HRESULT hr = pEvent->COMGETTER(State)(&machineState);
                    if (SUCCEEDED(hr))
                        hr = pEvent->COMGETTER(MachineId)(uuid.asOutParam());

                    if (SUCCEEDED(hr))
                    {
                        int rc = RTCritSectEnter(&g_MapCritSect);
                        if (RT_SUCCESS(rc))
                        {
                            rc = machineUpdate(uuid, machineState);
                            int rc2 = RTCritSectLeave(&g_MapCritSect);
                            if (RT_SUCCESS(rc))
                                rc = rc2;
                            AssertRC(rc);
                        }
                    }
                    break;
                }

                case VBoxEventType_OnVBoxSVCAvailabilityChanged:
                {
                    ComPtr<IVBoxSVCAvailabilityChangedEvent> pVSACEv = aEvent;
                    Assert(pVSACEv);
                    BOOL fAvailable = FALSE;
                    pVSACEv->COMGETTER(Available)(&fAvailable);
                    if (!fAvailable)
                    {
                        serviceLog("VBoxSVC became unavailable\n");

                        balloonCtrlShutdown();
                        deleteGlobalObjects();
                    }
                    else
                    {
                        serviceLog("VBoxSVC became available\n");
                        HRESULT hrc = createGlobalObjects();
                        if (FAILED(hrc))
                            serviceLog("Unable to re-create local COM objects (rc=%Rhrc)!\n", hrc);
                        else
                        {
                            hrc = balloonCtrlSetup();
                            if (FAILED(hrc))
                                serviceLog("Unable to re-set up ballooning (rc=%Rhrc)!\n", hrc);
                        }
                    }
                    break;
                }

                default:
                    /* Not handled event, just skip it. */
                    break;
            }

            return S_OK;
        }

    private:
};
typedef ListenerImpl<VirtualBoxEventListener> VirtualBoxEventListenerImpl;
VBOX_LISTENER_DECLARE(VirtualBoxEventListenerImpl)


/**
 * Signal handler that sets g_fGuestCtrlCanceled.
 *
 * This can be executed on any thread in the process, on Windows it may even be
 * a thread dedicated to delivering this signal.  Do not doing anything
 * unnecessary here.
 */
static void signalHandler(int iSignal)
{
    NOREF(iSignal);
    ASMAtomicWriteBool(&g_fCanceled, true);

    if (!g_pEventQ)
    {
        int rc = g_pEventQ->interruptEventQueueProcessing();
        if (RT_FAILURE(rc))
            serviceLog("Error: interruptEventQueueProcessing failed with rc=%Rrc\n", rc);
    }
}

/**
 * Installs a custom signal handler to get notified
 * whenever the user wants to intercept the program.
 */
static void signalHandlerInstall()
{
    signal(SIGINT,   signalHandler);
#ifdef SIGBREAK
    signal(SIGBREAK, signalHandler);
#endif
}

/**
 * Uninstalls a previously installed signal handler.
 */
static void signalHandlerUninstall()
{
    signal(SIGINT,   SIG_DFL);
#ifdef SIGBREAK
    signal(SIGBREAK, SIG_DFL);
#endif
}

/**
 * Retrieves the current delta value
 *
 * @return  long                                Delta (MB) of the balloon to be deflated (<0) or inflated (>0).
 * @param   ulCurrentDesktopBalloonSize         The balloon's current size.
 * @param   ulDesktopFreeMemory                 The VM's current free memory.
 * @param   ulMaxBalloonSize                    The maximum balloon size (MB) it can inflate to.
 */
static long getlBalloonDelta(unsigned long ulCurrentDesktopBalloonSize, unsigned long ulDesktopFreeMemory, unsigned long ulMaxBalloonSize)
{
    if (ulCurrentDesktopBalloonSize > ulMaxBalloonSize)
        return (ulMaxBalloonSize - ulCurrentDesktopBalloonSize);

    long lBalloonDelta = 0;
    if (ulDesktopFreeMemory < g_ulLowerMemoryLimitMB)
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
        while (lIncrement >= 16 && (ulDesktopFreeMemory - lIncrement) < g_ulLowerMemoryLimitMB)
        {
            lIncrement = (lIncrement / 2);
        }

        if ((ulDesktopFreeMemory - lIncrement) > g_ulLowerMemoryLimitMB)
            lBalloonDelta = lIncrement;
    }
    if (ulCurrentDesktopBalloonSize + lBalloonDelta > ulMaxBalloonSize)
        lBalloonDelta = (ulMaxBalloonSize - ulCurrentDesktopBalloonSize);
    return lBalloonDelta;
}

/**
 * Indicates whether a VM is up and running (regardless of its running
 * state, could be paused as well).
 *
 * @return  bool            Flag indicating whether the VM is running or not.
 * @param   enmState        The VM's machine state to judge whether it's running or not.
 */
static bool machineIsRunning(MachineState_T enmState)
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
 * Determines whether the specified machine needs to be handled
 * by this service.
 *
 * @return  bool                    True if the machine needs handling, false if not.
 * @param   strUuid                 UUID of the specified machine.
 */
static bool machineHandled(const Bstr &strUuid)
{
    bool fHandled = false;

    do
    {
        HRESULT rc;

        ComPtr <IMachine> machine;
        CHECK_ERROR_BREAK(g_pVirtualBox, FindMachine(strUuid.raw(), machine.asOutParam()));

        MachineState_T machineState;
        CHECK_ERROR_BREAK(machine, COMGETTER(State)(&machineState));

        if (   balloonGetMaxSize(machine)
            && machineIsRunning(machineState))
        {
            serviceLogVerbose(("Handling machine \"%ls\"\n", strUuid.raw()));
            fHandled = true;
        }
    }
    while (0);

    return fHandled;
}

/**
 * Adds a specified machine to the list (map) of handled machines.
 *
 * @return  IPRT status code.
 * @param   strUuid                 UUID of the specified machine.
 */
static int machineAdd(const Bstr &strUuid)
{
    HRESULT rc;

    do
    {
        ComPtr <IMachine> machine;
        CHECK_ERROR_BREAK(g_pVirtualBox, FindMachine(strUuid.raw(), machine.asOutParam()));

        MachineState_T machineState;
        CHECK_ERROR_BREAK(machine, COMGETTER(State)(&machineState));

        if (   !balloonGetMaxSize(machine)
            || !machineIsRunning(machineState))
        {
            /* This machine does not need to be added, just skip it! */
            break;
        }

        VBOXBALLOONCTRL_MACHINE m;
        m.machine = machine;

        /*
         * Setup metrics.
         */
        com::SafeArray<BSTR> metricNames(1);
        com::SafeIfaceArray<IUnknown> metricObjects(1);
        com::SafeIfaceArray<IPerformanceMetric> metricAffected;

        Bstr strMetricNames(L"Guest/RAM/Usage");
        strMetricNames.cloneTo(&metricNames[0]);

        m.machine.queryInterfaceTo(&metricObjects[0]);

#ifdef VBOX_BALLOONCTRL_GLOBAL_PERFCOL
        CHECK_ERROR_BREAK(g_pPerfCollector, SetupMetrics(ComSafeArrayAsInParam(metricNames),
                                                         ComSafeArrayAsInParam(metricObjects),
                                                         5 /* 5 seconds */,
                                                         1 /* One sample is enough */,
                                                         ComSafeArrayAsOutParam(metricAffected)));
#else
        CHECK_ERROR_BREAK(g_pVirtualBox, COMGETTER(PerformanceCollector)(m.collector.asOutParam()));
        CHECK_ERROR_BREAK(m.collector, SetupMetrics(ComSafeArrayAsInParam(metricNames),
                                                    ComSafeArrayAsInParam(metricObjects),
                                                    5 /* 5 seconds */,
                                                    1 /* One sample is enough */,
                                                    ComSafeArrayAsOutParam(metricAffected)));
#endif
        /*
         * Add machine to map.
         */
        mapVMIter it = g_mapVM.find(strUuid);
        Assert(it == g_mapVM.end());

        g_mapVM.insert(std::make_pair(strUuid, m));

        serviceLogVerbose(("Added machine \"%ls\"\n", strUuid.raw()));

    } while (0);

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_COM_IPRT_ERROR; /* @todo Find a better error! */
}

/**
 * Removes a specified machine from the list of handled machines.
 *
 * @return  IPRT status code.
 * @param   strUuid                 UUID of the specified machine.
 */
static int machineRemove(const Bstr &strUuid)
{
    int rc = RTCritSectEnter(&g_MapCritSect);
    if (RT_SUCCESS(rc))
    {
        mapVMIter it = g_mapVM.find(strUuid);
        if (it != g_mapVM.end())
        {
            /* Must log before erasing the iterator because of the UUID ref! */
            serviceLogVerbose(("Removing machine \"%ls\"\n", strUuid.raw()));

            /*
             * Remove machine from map.
             */
            g_mapVM.erase(it);
        }
        else
        {
            serviceLogVerbose(("Warning: Removing not added machine \"%ls\"\n", strUuid.raw()));
            rc = VERR_NOT_FOUND;
        }

        int rc2 = RTCritSectLeave(&g_MapCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}

/**
 * Updates a specified machine according to its current machine state.
 * That currently also could mean that a machine gets removed if it doesn't
 * fit in our criteria anymore or a machine gets added if we need to handle
 * it now (and didn't before).
 *
 * @return  IPRT status code.
 * @param   strUuid                 UUID of the specified machine.
 * @param   enmState                The machine's current state.
 */
static int machineUpdate(const Bstr &strUuid, MachineState_T enmState)
{
    int rc = VINF_SUCCESS;

    mapVMIter it = g_mapVM.find(strUuid);
    if (it == g_mapVM.end())
    {
        if (machineHandled(strUuid))
        {
            rc  = machineAdd(strUuid);
            if (RT_SUCCESS(rc))
                it = g_mapVM.find(strUuid);
        }
        else
        {
            serviceLogVerbose(("Machine \"%ls\" (state: %u) does not need to be updated\n",
                               strUuid.raw(), enmState));
        }
    }

    if (it != g_mapVM.end())
    {
        /*
         * Ballooning stuff - start.
         */

        /* Our actual ballooning criteria. */
        if (   !balloonIsRequired(&it->second)
            || !machineIsRunning(enmState))
        {
            /* Current machine is not suited for ballooning anymore -
             * remove it from our map. */
            rc = machineRemove(strUuid);
        }
        else
        {
            rc = balloonUpdate(strUuid, &it->second);
            AssertRC(rc);
        }
    }

    /*
     * Ballooning stuff - end.
     */

    return rc;
}

/**
 * Retrieves a metric from a specified machine.
 *
 * @return  IPRT status code.
 * @param   pMachine                Pointer to the machine's internal structure.
 * @param   strName                 Name of metric to retrieve.
 * @param   pulData                 Pointer to value to retrieve the actual metric value.
 */
static int getMetric(PVBOXBALLOONCTRL_MACHINE pMachine, const Bstr& strName, LONG *pulData)
{
    AssertPtrReturn(pMachine, VERR_INVALID_POINTER);
    AssertPtrReturn(pulData, VERR_INVALID_POINTER);

    /* Input. */
    com::SafeArray<BSTR> metricNames(1);
    com::SafeIfaceArray<IUnknown> metricObjects(1);
    pMachine->machine.queryInterfaceTo(&metricObjects[0]);

    /* Output. */
    com::SafeArray<BSTR>          retNames;
    com::SafeIfaceArray<IUnknown> retObjects;
    com::SafeArray<BSTR>          retUnits;
    com::SafeArray<ULONG>         retScales;
    com::SafeArray<ULONG>         retSequenceNumbers;
    com::SafeArray<ULONG>         retIndices;
    com::SafeArray<ULONG>         retLengths;
    com::SafeArray<LONG>          retData;

    /* Query current memory free. */
    strName.cloneTo(&metricNames[0]);
#ifdef VBOX_BALLOONCTRL_GLOBAL_PERFCOL
    Assert(!g_pPerfCollector.isNull());
    HRESULT hrc = g_pPerfCollector->QueryMetricsData(
#else
    Assert(!pMachine->collector.isNull());
    HRESULT hrc = pMachine->collector->QueryMetricsData(
#endif
                                                ComSafeArrayAsInParam(metricNames),
                                                ComSafeArrayAsInParam(metricObjects),
                                                ComSafeArrayAsOutParam(retNames),
                                                ComSafeArrayAsOutParam(retObjects),
                                                ComSafeArrayAsOutParam(retUnits),
                                                ComSafeArrayAsOutParam(retScales),
                                                ComSafeArrayAsOutParam(retSequenceNumbers),
                                                ComSafeArrayAsOutParam(retIndices),
                                                ComSafeArrayAsOutParam(retLengths),
                                                ComSafeArrayAsOutParam(retData));
#if 0
    /* Useful for metrics debugging. */
    for (unsigned j = 0; j < retNames.size(); j++)
    {
        Bstr metricUnit(retUnits[j]);
        Bstr metricName(retNames[j]);
        RTPrintf("%-20ls ", metricName.raw());
        const char *separator = "";
        for (unsigned k = 0; k < retLengths[j]; k++)
        {
            if (retScales[j] == 1)
                RTPrintf("%s%d %ls", separator, retData[retIndices[j] + k], metricUnit.raw());
            else
                RTPrintf("%s%d.%02d%ls", separator, retData[retIndices[j] + k] / retScales[j],
                         (retData[retIndices[j] + k] * 100 / retScales[j]) % 100, metricUnit.raw());
            separator = ", ";
        }
        RTPrintf("\n");
    }
#endif

    if (SUCCEEDED(hrc))
        *pulData = retData.size() ? retData[retIndices[0]] : 0;

    return SUCCEEDED(hrc) ? VINF_SUCCESS : VINF_NOT_SUPPORTED;
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
     *  - per-VM parameter ("VBoxInternal/Guest/BalloonSizeMax")
     *  - global parameter ("VBoxInternal/Guest/BalloonSizeMax")
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
    return ulBalloonMax;
}

/**
 * Determines whether ballooning is required to the specified machine.
 *
 * @return  bool                    True if ballooning is required, false if not.
 * @param   strUuid                 UUID of the specified machine.
 */
static bool balloonIsRequired(PVBOXBALLOONCTRL_MACHINE pMachine)
{
    AssertPtrReturn(pMachine, false);

    /* Only do ballooning if we have a maximum balloon size set. */
    pMachine->ulBalloonSizeMax = pMachine->machine.isNull()
                               ? 0 : balloonGetMaxSize(pMachine->machine);

    return pMachine->ulBalloonSizeMax ? true : false;
}

/**
 * Does the actual ballooning and assumes the machine is
 * capable and ready for ballooning.
 *
 * @return  IPRT status code.
 * @param   strUuid                 UUID of the specified machine.
 * @param   pMachine                Pointer to the machine's internal structure.
 */
static int balloonUpdate(const Bstr &strUuid, PVBOXBALLOONCTRL_MACHINE pMachine)
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

        serviceLogVerbose(("%ls: Balloon: %ld, Free mem: %ld, Max ballon: %ld\n",
                           strUuid.raw(),
                           lBalloonCur, lMemFree, pMachine->ulBalloonSizeMax));

        /* Calculate current balloon delta. */
        long lDelta = getlBalloonDelta(lBalloonCur, lMemFree, pMachine->ulBalloonSizeMax);
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

static void vmListDestroy()
{
    serviceLogVerbose(("Destroying VM list ...\n"));

    int rc = RTCritSectEnter(&g_MapCritSect);
    if (RT_SUCCESS(rc))
    {
        mapVMIter it = g_mapVM.begin();
        while (it != g_mapVM.end())
        {
#ifndef VBOX_BALLOONCTRL_GLOBAL_PERFCOL
            it->second.collector.setNull();
#endif
            it->second.machine.setNull();
            it++;
        }

        g_mapVM.clear();

        rc = RTCritSectLeave(&g_MapCritSect);
    }
    AssertRC(rc);
}

static int vmListBuild()
{
    serviceLogVerbose(("Building VM list ...\n"));

    int rc = RTCritSectEnter(&g_MapCritSect);
    if (RT_SUCCESS(rc))
    {
        /*
         * Make sure the list is empty.
         */
        g_mapVM.clear();

        /*
         * Get the list of all _running_ VMs
         */
        com::SafeIfaceArray<IMachine> machines;
        HRESULT hrc = g_pVirtualBox->COMGETTER(Machines)(ComSafeArrayAsOutParam(machines));
        if (SUCCEEDED(hrc))
        {
            /*
             * Iterate through the collection
             */
            for (size_t i = 0; i < machines.size(); ++i)
            {
                if (machines[i])
                {
                    Bstr strUUID;
                    CHECK_ERROR_BREAK(machines[i], COMGETTER(Id)(strUUID.asOutParam()));

                    BOOL fAccessible;
                    CHECK_ERROR_BREAK(machines[i], COMGETTER(Accessible)(&fAccessible));
                    if (!fAccessible)
                    {
                        serviceLogVerbose(("Machine \"%ls\" is inaccessible, skipping\n",
                                           strUUID.raw()));
                        continue;
                    }

                    rc = machineAdd(strUUID);
                    if (RT_FAILURE(rc))
                        break;
                }
            }

            if (!machines.size())
                serviceLogVerbose(("No machines to add found at the moment!\n"));
        }

        int rc2 = RTCritSectLeave(&g_MapCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}

static int balloonCtrlCheck()
{
    static uint64_t uLast = UINT64_MAX;
    uint64_t uNow = RTTimeProgramMilliTS() / g_ulTimeoutMS;
    if (uLast == uNow)
        return VINF_SUCCESS;
    uLast = uNow;

    int rc = RTCritSectEnter(&g_MapCritSect);
    if (RT_SUCCESS(rc))
    {
        mapVMIter it = g_mapVM.begin();
        while (it != g_mapVM.end())
        {
            MachineState_T machineState;
            HRESULT hrc = it->second.machine->COMGETTER(State)(&machineState);
            if (SUCCEEDED(hrc))
            {
                rc = machineUpdate(it->first /* UUID */, machineState);
                if (RT_FAILURE(rc))
                    break;
            }
            it++;
        }

        int rc2 = RTCritSectLeave(&g_MapCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}

static HRESULT balloonCtrlSetup()
{
    HRESULT rc = S_OK;

    serviceLog("Setting up ballooning ...\n");

    do
    {
        /*
         * Setup metrics.
         */
#ifdef VBOX_BALLOONCTRL_GLOBAL_PERFCOL
        CHECK_ERROR_BREAK(g_pVirtualBox, COMGETTER(PerformanceCollector)(g_pPerfCollector.asOutParam()));
#endif

        /*
         * Build up initial VM list.
         */
        int vrc = vmListBuild();
        if (RT_FAILURE(vrc))
        {
            rc = VBOX_E_IPRT_ERROR;
            break;
        }

    } while (0);

    return rc;
}

static void balloonCtrlShutdown()
{
    serviceLog("Shutting down ballooning ...\n");

    vmListDestroy();

#ifdef VBOX_BALLOONCTRL_GLOBAL_PERFCOL
    g_pPerfCollector.setNull();
#endif
}

static RTEXITCODE balloonCtrlMain(HandlerArg *a)
{
    HRESULT rc = S_OK;

    do
    {
        int vrc = VINF_SUCCESS;

        /* Initialize global weak references. */
        g_pEventQ = com::EventQueue::getMainEventQueue();

        RTCritSectInit(&g_MapCritSect);

        /*
         * Install signal handlers.
         */
        signal(SIGINT,   signalHandler);
    #ifdef SIGBREAK
        signal(SIGBREAK, signalHandler);
    #endif

        /*
         * Setup the global event listeners:
         * - g_pEventSource for machine events
         * - g_pEventSourceClient for VBoxClient events (like VBoxSVC handling)
         */
        CHECK_ERROR_BREAK(g_pVirtualBox, COMGETTER(EventSource)(g_pEventSource.asOutParam()));
        CHECK_ERROR_BREAK(g_pVirtualBoxClient, COMGETTER(EventSource)(g_pEventSourceClient.asOutParam()));

        ComObjPtr<VirtualBoxEventListenerImpl> vboxListenerImpl;
        vboxListenerImpl.createObject();
        vboxListenerImpl->init(new VirtualBoxEventListener());

        com::SafeArray <VBoxEventType_T> eventTypes;
        eventTypes.push_back(VBoxEventType_OnMachineRegistered);
        eventTypes.push_back(VBoxEventType_OnMachineStateChanged);
        eventTypes.push_back(VBoxEventType_OnVBoxSVCAvailabilityChanged); /* Processed by g_pEventSourceClient. */

        g_pVBoxEventListener = vboxListenerImpl;
        CHECK_ERROR_BREAK(g_pEventSource, RegisterListener(g_pVBoxEventListener, ComSafeArrayAsInParam(eventTypes), true /* Active listener */));
        CHECK_ERROR_BREAK(g_pEventSourceClient, RegisterListener(g_pVBoxEventListener, ComSafeArrayAsInParam(eventTypes), true /* Active listener */));

        /*
         * Set up ballooning stuff.
         */
        rc = balloonCtrlSetup();
        if (FAILED(rc))
            break;

        for (;;)
        {
            /*
             * Do the actual work.
             */
            vrc = balloonCtrlCheck();
            if (RT_FAILURE(vrc))
            {
                serviceLog("Error while doing ballooning control; rc=%Rrc\n", vrc);
                break;
            }

            /*
             * Process pending events, then wait for new ones. Note, this
             * processes NULL events signalling event loop termination.
             */
            g_pEventQ->processEventQueue(g_ulTimeoutMS / 10);

            if (g_fCanceled)
            {
                serviceLog("Signal caught, exiting ...\n");
                break;
            }
        }

        signal(SIGINT,   SIG_DFL);
    #ifdef SIGBREAK
        signal(SIGBREAK, SIG_DFL);
    #endif

        /* VirtualBox callback unregistration. */
        if (g_pVBoxEventListener)
        {
            if (!g_pEventSource.isNull())
                CHECK_ERROR(g_pEventSource, UnregisterListener(g_pVBoxEventListener));
            g_pVBoxEventListener.setNull();
        }

        g_pEventSource.setNull();
        g_pEventSourceClient.setNull();

        balloonCtrlShutdown();

        RTCritSectDelete(&g_MapCritSect);

        if (RT_FAILURE(vrc))
            rc = VBOX_E_IPRT_ERROR;

    } while (0);

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static void serviceLog(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    va_end(args);

    LogRel(("%s", psz));

    RTStrFree(psz);
}

static void logHeaderFooter(PRTLOGGER pLoggerRelease, RTLOGPHASE enmPhase, PFNRTLOGPHASEMSG pfnLog)
{
    /* Some introductory information. */
    static RTTIMESPEC s_TimeSpec;
    char szTmp[256];
    if (enmPhase == RTLOGPHASE_BEGIN)
        RTTimeNow(&s_TimeSpec);
    RTTimeSpecToString(&s_TimeSpec, szTmp, sizeof(szTmp));

    switch (enmPhase)
    {
        case RTLOGPHASE_BEGIN:
        {
            pfnLog(pLoggerRelease,
                   "VirtualBox Ballooning Control Service %s r%u %s (%s %s) release log\n"
#ifdef VBOX_BLEEDING_EDGE
                   "EXPERIMENTAL build " VBOX_BLEEDING_EDGE "\n"
#endif
                   "Log opened %s\n",
                   VBOX_VERSION_STRING, RTBldCfgRevision(), VBOX_BUILD_TARGET,
                   __DATE__, __TIME__, szTmp);

            int vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Product: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Release: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Version: %s\n", szTmp);
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Service Pack: %s\n", szTmp);

            /* the package type is interesting for Linux distributions */
            char szExecName[RTPATH_MAX];
            char *pszExecName = RTProcGetExecutablePath(szExecName, sizeof(szExecName));
            pfnLog(pLoggerRelease,
                   "Executable: %s\n"
                   "Process ID: %u\n"
                   "Package type: %s"
#ifdef VBOX_OSE
                   " (OSE)"
#endif
                   "\n",
                   pszExecName ? pszExecName : "unknown",
                   RTProcSelf(),
                   VBOX_PACKAGE_STRING);
            break;
        }

        case RTLOGPHASE_PREROTATE:
            pfnLog(pLoggerRelease, "Log rotated - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_POSTROTATE:
            pfnLog(pLoggerRelease, "Log continuation - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_END:
            pfnLog(pLoggerRelease, "End of log file - Log started %s\n", szTmp);
            break;

        default:
            /* nothing */;
    }
}

static void displayHelp()
{
    RTStrmPrintf(g_pStdErr, "\nUsage: VBoxBalloonCtrl [options]\n\nSupported options (default values in brackets):\n");
    for (unsigned i = 0;
         i < RT_ELEMENTS(g_aOptions);
         ++i)
    {
        std::string str(g_aOptions[i].pszLong);
        if (g_aOptions[i].iShort < 1000) /* Don't show short options which are defined by an ID! */
        {
            str += ", -";
            str += g_aOptions[i].iShort;
        }
        str += ":";

        const char *pcszDescr = "";

        switch (g_aOptions[i].iShort)
        {
            case 'h':
                pcszDescr = "Print this help message and exit.";
                break;

            case 'i': /* Interval. */
                pcszDescr = "Sets the check interval in ms (30 seconds).";
                break;

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
            case 'b':
                pcszDescr = "Run in background (daemon mode).";
                break;
#endif
            case GETOPTDEF_BALLOONCTRL_BALLOOINC:
                pcszDescr = "Sets the ballooning increment in MB (256 MB).";
                break;

            case GETOPTDEF_BALLOONCTRL_BALLOONDEC:
                pcszDescr = "Sets the ballooning decrement in MB (128 MB).";
                break;

            case GETOPTDEF_BALLOONCTRL_BALLOONLOWERLIMIT:
                pcszDescr = "Sets the ballooning lower limit in MB (64 MB).";
                break;

            case GETOPTDEF_BALLOONCTRL_BALLOONMAX:
                pcszDescr = "Sets the balloon maximum limit in MB (0 MB).";
                break;

            case 'P':
                pcszDescr = "Name of the PID file which is created when the daemon was started.";
                break;

            case 'F':
                pcszDescr = "Name of file to write log to (no file).";
                break;

            case 'R':
                pcszDescr = "Number of log files (0 disables log rotation).";
                break;

            case 'S':
                pcszDescr = "Maximum size of a log file to trigger rotation (bytes).";
                break;

            case 'I':
                pcszDescr = "Maximum time interval to trigger log rotation (seconds).";
                break;
        }

        RTStrmPrintf(g_pStdErr, "%-23s%s\n", str.c_str(), pcszDescr);
    }

    RTStrmPrintf(g_pStdErr, "\nUse environment variable VBOXBALLOONCTRL_RELEASE_LOG for logging options.\n"
                            "Set \"VBoxInternal/Guest/BalloonSizeMax\" for a per-VM maximum ballooning size.\n");
}

static void deleteGlobalObjects()
{
    serviceLogVerbose(("Deleting local objects ...\n"));

    g_pSession.setNull();
    g_pVirtualBox.setNull();
}

/**
 * Creates all global COM objects.
 *
 * @return  HRESULT
 */
static HRESULT createGlobalObjects()
{
    serviceLogVerbose(("Creating local objects ...\n"));

    HRESULT hrc = g_pVirtualBoxClient->COMGETTER(VirtualBox)(g_pVirtualBox.asOutParam());
    if (FAILED(hrc))
    {
        RTMsgError("Failed to get VirtualBox object (rc=%Rhrc)!", hrc);
    }
    else
    {
        hrc = g_pSession.createInprocObject(CLSID_Session);
        if (FAILED(hrc))
            RTMsgError("Failed to create a session object (rc=%Rhrc)!", hrc);
    }

    return hrc;
}

int main(int argc, char *argv[])
{
    /*
     * Before we do anything, init the runtime without loading
     * the support driver.
     */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    RTPrintf(VBOX_PRODUCT " Balloon Control " VBOX_VERSION_STRING "\n"
             "(C) " VBOX_C_YEAR " " VBOX_VENDOR "\n"
             "All rights reserved.\n\n");

    /*
     * Parse the global options
    */
    int c;
    const char *pszLogFile = NULL;
    const char *pszPidFile = NULL;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aOptions, RT_ELEMENTS(g_aOptions), 1, 0 /*fFlags*/);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'h':
                displayHelp();
                return 0;

            case 'i': /* Interval. */
                g_ulTimeoutMS = ValueUnion.u32;
                if (g_ulTimeoutMS < 500)
                    g_ulTimeoutMS = 500;
                break;

            case 'v':
                g_fVerbose = true;
                break;

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
            case 'b':
                g_fDaemonize = true;
                break;
#endif
            case 'V':
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return 0;

            case GETOPTDEF_BALLOONCTRL_BALLOOINC:
                g_ulMemoryBalloonIncrementMB = ValueUnion.u32;
                break;

            case GETOPTDEF_BALLOONCTRL_BALLOONDEC:
                g_ulMemoryBalloonDecrementMB = ValueUnion.u32;
                break;

            case GETOPTDEF_BALLOONCTRL_BALLOONLOWERLIMIT:
                g_ulLowerMemoryLimitMB = ValueUnion.u32;
                break;

            case GETOPTDEF_BALLOONCTRL_BALLOONMAX:
                g_ulMemoryBalloonMaxMB = ValueUnion.u32;
                break;

            case 'P':
                pszPidFile = ValueUnion.psz;
                break;

            case 'F':
                pszLogFile = ValueUnion.psz;
                break;

            case 'R':
                g_cHistory = ValueUnion.u32;
                break;

            case 'S':
                g_uHistoryFileSize = ValueUnion.u64;
                break;

            case 'I':
                g_uHistoryFileTime = ValueUnion.u32;
                break;

            default:
                rc = RTGetOptPrintError(c, &ValueUnion);
                return rc;
        }
    }

    /* create release logger */
    PRTLOGGER pLoggerRelease;
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    RTUINT fFlags = RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    fFlags |= RTLOGFLAGS_USECRLF;
#endif
    char szError[RTPATH_MAX + 128] = "";
    rc = RTLogCreateEx(&pLoggerRelease, fFlags, "all",
                       "VBOXBALLOONCTRL_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups, RTLOGDEST_STDOUT,
                       logHeaderFooter, g_cHistory, g_uHistoryFileSize, g_uHistoryFileTime,
                       szError, sizeof(szError), pszLogFile);
    if (RT_SUCCESS(rc))
    {
        /* register this logger as the release logger */
        RTLogRelSetDefaultInstance(pLoggerRelease);

        /* Explicitly flush the log in case of VBOXWEBSRV_RELEASE_LOG=buffered. */
        RTLogFlush(pLoggerRelease);
    }
    else
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open release log (%s, %Rrc)", szError, rc);

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
    if (g_fDaemonize)
    {
        /* prepare release logging */
        char szLogFile[RTPATH_MAX];

        rc = com::GetVBoxUserHomeDirectory(szLogFile, sizeof(szLogFile));
        if (RT_FAILURE(rc))
             return RTMsgErrorExit(RTEXITCODE_FAILURE, "could not get base directory for logging: %Rrc", rc);
        rc = RTPathAppend(szLogFile, sizeof(szLogFile), "vboxballoonctrl.log");
        if (RT_FAILURE(rc))
             return RTMsgErrorExit(RTEXITCODE_FAILURE, "could not construct logging path: %Rrc", rc);

        rc = RTProcDaemonizeUsingFork(false /* fNoChDir */, false /* fNoClose */, pszPidFile);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to daemonize, rc=%Rrc. exiting.", rc);

        /* create release logger */
        PRTLOGGER pLoggerReleaseFile;
        static const char * const s_apszGroupsFile[] = VBOX_LOGGROUP_NAMES;
        RTUINT fFlagsFile = RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        fFlagsFile |= RTLOGFLAGS_USECRLF;
#endif
        char szErrorFile[RTPATH_MAX + 128] = "";
        int vrc = RTLogCreateEx(&pLoggerReleaseFile, fFlagsFile, "all",
                                "VBOXBALLOONCTRL_RELEASE_LOG", RT_ELEMENTS(s_apszGroupsFile), s_apszGroupsFile, RTLOGDEST_FILE,
                                logHeaderFooter, g_cHistory, g_uHistoryFileSize, g_uHistoryFileTime,
                                szErrorFile, sizeof(szErrorFile), szLogFile);
        if (RT_SUCCESS(vrc))
        {
            /* register this logger as the release logger */
            RTLogRelSetDefaultInstance(pLoggerReleaseFile);

            /* Explicitly flush the log in case of VBOXBALLOONCTRL_RELEASE_LOG=buffered. */
            RTLogFlush(pLoggerReleaseFile);
        }
        else
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open release log (%s, %Rrc)", szErrorFile, vrc);
    }
#endif

#ifndef VBOX_ONLY_DOCS
    /*
     * Initialize COM.
     */
    using namespace com;
    HRESULT hrc = com::Initialize();
    if (FAILED(hrc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to initialize COM!");

    hrc = g_pVirtualBoxClient.createInprocObject(CLSID_VirtualBoxClient);
    if (FAILED(hrc))
    {
        RTMsgError("failed to create the VirtualBoxClient object!");
        com::ErrorInfo info;
        if (!info.isFullAvailable() && !info.isBasicAvailable())
        {
            com::GluePrintRCMessage(hrc);
            RTMsgError("Most likely, the VirtualBox COM server is not running or failed to start.");
        }
        else
            com::GluePrintErrorInfo(info);
        return RTEXITCODE_FAILURE;
    }

    hrc = createGlobalObjects();
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    HandlerArg handlerArg = { argc, argv };
    RTEXITCODE rcExit = balloonCtrlMain(&handlerArg);

    EventQueue::getMainEventQueue()->processEventQueue(0);

    deleteGlobalObjects();

    g_pVirtualBoxClient.setNull();

    com::Shutdown();

    return rcExit;
#else  /* VBOX_ONLY_DOCS */
    return RTEXITCODE_SUCCESS;
#endif /* VBOX_ONLY_DOCS */
}

