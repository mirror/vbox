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

/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI g_BalloonControlEvent = NIL_RTSEMEVENTMULTI;

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

unsigned long g_ulTimeoutMS = 30 * 1000; /* Default is 30 seconds timeout. */
unsigned long g_ulMemoryBalloonIncrementMB = 256;
unsigned long g_ulMemoryBalloonDecrementMB = 128;
/** Global balloon limit is 0, so disabled. Can be overridden by a per-VM
 *  "VBoxInternal/Guest/BalloonSizeMax" value. */
unsigned long g_ulMemoryBalloonMaxMB = 0;
unsigned long g_ulLowerMemoryLimitMB = 64;

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
mapVM g_mapVM;

/* Prototypes. */
#define serviceLogVerbose(a) if (g_fVerbose) { serviceLog a; }
void serviceLog(const char *pszFormat, ...);

bool machineIsRunning(MachineState_T enmState);
mapVMIter machineGetByUUID(const Bstr &strUUID);
int machineAdd(const ComPtr<IMachine> &rptrMachine);
int machineUpdate(const ComPtr<IMachine> &rptrMachine, MachineState_T enmState);
int machineUpdate(mapVMIter it, MachineState_T enmState);
void machineRemove(mapVMIter it);

unsigned long balloonGetMaxSize(const ComPtr<IMachine> &rptrMachine);
bool balloonIsRequired(mapVMIter it);
int balloonUpdate(mapVMIterConst it);

HRESULT balloonCtrlSetup();
void balloonCtrlShutdown();

HRESULT createGlobalObjects();
void deleteGlobalObjects();

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
                            if (fRegistered)
                            {
                                ComPtr <IMachine> machine;
                                hr = g_pVirtualBox->FindMachine(uuid.raw(), machine.asOutParam());
                                if (SUCCEEDED(hr))
                                    rc = machineAdd(machine);
                                else
                                    rc = VERR_NOT_FOUND;
                            }
                            else
                                machineRemove(machineGetByUUID(uuid));
                            AssertRC(rc);

                            int rc2 = RTCritSectLeave(&g_MapCritSect);
                            if (RT_SUCCESS(rc))
                                rc = rc2;
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
                            mapVMIter it = machineGetByUUID(uuid);
                            if (it == g_mapVM.end())
                            {
                                /* Use the machine object to figure out if we
                                 * need to do something. */
                                ComPtr <IMachine> machine;
                                hr = g_pVirtualBox->FindMachine(uuid.raw(), machine.asOutParam());
                                if (SUCCEEDED(hr))
                                    rc = machineUpdate(machine, machineState);
                            }
                            else /* Update an existing machine. */
                                rc = machineUpdate(it, machineState);
                            AssertRC(rc);

                            int rc2 = RTCritSectLeave(&g_MapCritSect);
                            if (RT_SUCCESS(rc))
                                rc = rc2;
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
                        {
                            balloonCtrlShutdown();
                            deleteGlobalObjects();
                        }
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

    if (g_BalloonControlEvent != NIL_RTSEMEVENTMULTI)
    {
        int rc = RTSemEventMultiSignal(g_BalloonControlEvent);
        if (RT_FAILURE(rc))
            serviceLog("Error: RTSemEventMultiSignal failed with rc=%Rrc\n");
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

long getlBalloonDelta(unsigned long ulCurrentDesktopBalloonSize, unsigned long ulDesktopFreeMemory, unsigned long ulMaxBalloonSize)
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
bool machineIsRunning(MachineState_T enmState)
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

mapVMIter machineGetByUUID(const Bstr &strUUID)
{
    return g_mapVM.find(strUUID);
}

int machineAdd(const ComPtr<IMachine> &rptrMachine)
{
    HRESULT rc;

    do
    {
        VBOXBALLOONCTRL_MACHINE m;
        m.machine = rptrMachine;

        /*
         * Setup metrics.
         */
        com::SafeArray<BSTR> metricNames(1);
        com::SafeIfaceArray<IUnknown> metricObjects(1);
        com::SafeIfaceArray<IPerformanceMetric> metricAffected;

        Bstr strMetricNames(L"Guest/RAM*");
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
        Bstr strUUID;
        CHECK_ERROR_BREAK(rptrMachine, COMGETTER(Id)(strUUID.asOutParam()));

        mapVMIter it = g_mapVM.find(strUUID);
        Assert(it == g_mapVM.end());

        g_mapVM.insert(std::make_pair(strUUID, m));

        serviceLogVerbose(("Added machine \"%s\"\n", Utf8Str(strUUID).c_str()));

    } while (0);

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_COM_IPRT_ERROR; /* @todo Find a better error! */
}

void machineRemove(mapVMIter it)
{
    if (it != g_mapVM.end())
    {
        /* Must log before erasing the iterator because of the UUID ref! */
        serviceLogVerbose(("Removing machine \"%s\"\n", Utf8Str(it->first).c_str()));

        /*
         * Remove machine from map.
         */
        g_mapVM.erase(it);
    }
}

int machineUpdate(const ComPtr<IMachine> &rptrMachine, MachineState_T enmState)
{
    if (   !balloonGetMaxSize(rptrMachine)
        || !machineIsRunning(enmState))
    {
        return VINF_SUCCESS; /* Machine is not required to be added. */
    }
    return machineAdd(rptrMachine);
}

int machineUpdate(mapVMIter it, MachineState_T enmState)
{
    Assert(it != g_mapVM.end());

    if (   !balloonIsRequired(it)
        || !machineIsRunning(enmState))
    {
        machineRemove(it);
        return VINF_SUCCESS;
    }

    return balloonUpdate(it);
}

int getMetric(mapVMIterConst it, const Bstr& strName, LONG *pulData)
{
    AssertPtrReturn(pulData, VERR_INVALID_PARAMETER);

    /* Input. */
    com::SafeArray<BSTR> metricNames(1);
    com::SafeIfaceArray<IUnknown> metricObjects(1);
    it->second.machine.queryInterfaceTo(&metricObjects[0]);

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
    HRESULT hrc = g_pPerfCollector->QueryMetricsData(
#else
    HRESULT hrc = it->second.collector->QueryMetricsData(
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
 * @param   rptrMachine             Pointer to specified machine.
 */
unsigned long balloonGetMaxSize(const ComPtr<IMachine> &rptrMachine)
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
 * Determines whether ballooning is required to the spcified VM.
 *
 * @return  bool        True if ballooning is required, false if not.
 * @param   it          Iterator pointing to the VM to be processed.
 */
bool balloonIsRequired(mapVMIter it)
{
    /* Only do ballooning if we have a maximum balloon size set. */
    it->second.ulBalloonSizeMax = balloonGetMaxSize(it->second.machine);

    return it->second.ulBalloonSizeMax ? true : false;
}

/**
 * Does the actual ballooning and assumes the machine is
 * capable and ready for ballooning.
 *
 * @return  IPRT status code.
 * @param   it          Iterator pointing to the VM to be processed.
 */
int balloonUpdate(mapVMIterConst it)
{
    /*
     * Get metrics collected at this point.
     */
    LONG lMemFree, lBalloonCur;
    int vrc = getMetric(it, L"Guest/RAM/Usage/Free", &lMemFree);
    if (RT_SUCCESS(vrc))
        vrc = getMetric(it, L"Guest/RAM/Usage/Balloon", &lBalloonCur);

    if (RT_SUCCESS(vrc))
    {
        /* If guest statistics are not up and running yet, skip this iteration
         * and try next time. */
        if (lMemFree <= 0)
        {
#ifdef DEBUG
            serviceLogVerbose(("%s: No metrics available yet!\n", Utf8Str(it->first).c_str()));
#endif
            return VINF_SUCCESS;
        }

        lMemFree /= 1024;
        lBalloonCur /= 1024;

        serviceLogVerbose(("%s: Balloon: %ld, Free mem: %ld, Max ballon: %ld\n",
                           Utf8Str(it->first).c_str(),
                           lBalloonCur, lMemFree, it->second.ulBalloonSizeMax));

        /* Calculate current balloon delta. */
        long lDelta = getlBalloonDelta(lBalloonCur, lMemFree, it->second.ulBalloonSizeMax);
        if (lDelta) /* Only do ballooning if there's really smth. to change ... */
        {
            lBalloonCur = lBalloonCur + lDelta;
            Assert(lBalloonCur > 0);

            serviceLog("%s: %s balloon by %ld to %ld ...\n",
                       Utf8Str(it->first).c_str(),
                       lDelta > 0 ? "Inflating" : "Deflating", lDelta, lBalloonCur);

            HRESULT rc;

            /* Open a session for the VM. */
            CHECK_ERROR(it->second.machine, LockMachine(g_pSession, LockType_Shared));

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
                    serviceLog("Error: Unable to set new balloon size %ld for machine \"%s\", rc=%Rhrc",
                               lBalloonCur, Utf8Str(it->first).c_str(), rc);
            } while (0);

            /* Unlock the machine again. */
            g_pSession->UnlockMachine();
        }
    }
    else
        serviceLog("Error: Unable to retrieve metrics for machine \"%s\", rc=%Rrc",
                   Utf8Str(it->first).c_str(), vrc);
    return vrc;
}

void vmListDestroy()
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

int vmListBuild()
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
                        serviceLogVerbose(("Machine \"%s\" is inaccessible, skipping\n",
                                           Utf8Str(strUUID).c_str()));
                        continue;
                    }

                    MachineState_T machineState;
                    CHECK_ERROR_BREAK(machines[i], COMGETTER(State)(&machineState));

                    if (g_fVerbose)
                        serviceLogVerbose(("Processing machine \"%s\" (state: %ld)\n",
                                           Utf8Str(strUUID).c_str(), machineState));

                    if (machineIsRunning(machineState))
                    {
                        rc = machineAdd(machines[i]);
                        if (RT_FAILURE(rc))
                            break;
                    }
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

int balloonCtrlCheck()
{
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
                rc = machineUpdate(it, machineState);
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

HRESULT balloonCtrlSetup()
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

void balloonCtrlShutdown()
{
    serviceLog("Shutting down ballooning ...\n");

    vmListDestroy();

#ifdef VBOX_BALLOONCTRL_GLOBAL_PERFCOL
    g_pPerfCollector.setNull();
#endif
}

RTEXITCODE balloonCtrlMain(HandlerArg *a)
{
    HRESULT rc = S_OK;

    do
    {
        /* Initialize global weak references. */
        g_pEventQ = com::EventQueue::getMainEventQueue();

        RTCritSectInit(&g_MapCritSect);

        int vrc = RTSemEventMultiCreate(&g_BalloonControlEvent);
        AssertRCReturn(vrc, RTEXITCODE_FAILURE);

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
            g_pEventQ->processEventQueue(500);

            if (g_fCanceled)
            {
                serviceLog("Signal caught, exiting ...\n");
                break;
            }

            vrc = RTSemEventMultiWait(g_BalloonControlEvent, g_ulTimeoutMS);
            if (vrc != VERR_TIMEOUT && RT_FAILURE(vrc))
            {
                serviceLog("Error: RTSemEventMultiWait failed; rc=%Rrc\n", vrc);
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
        RTSemEventMultiDestroy(g_BalloonControlEvent);
        g_BalloonControlEvent = NIL_RTSEMEVENTMULTI;

        if (RT_FAILURE(vrc))
            rc = VBOX_E_IPRT_ERROR;

    } while (0);

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

void serviceLog(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    va_end(args);

    LogRel(("%s", psz));

    RTStrFree(psz);
}

void logHeaderFooter(PRTLOGGER pLoggerRelease, RTLOGPHASE enmPhase, PFNRTLOGPHASEMSG pfnLog)
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

void displayHelp()
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

void deleteGlobalObjects()
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
HRESULT createGlobalObjects()
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
    int rc = RTR3Init();
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

