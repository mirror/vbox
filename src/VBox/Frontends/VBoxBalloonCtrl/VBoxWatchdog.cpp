/* $Id$ */
/** @file
 * VBoxBalloonCtrl - VirtualBox Ballooning Control Service.
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
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/time.h>


#include <string>
#include <signal.h>

#include "VBoxWatchdogInternal.h"

using namespace com;

/* When defined, use a global performance collector instead
 * of a per-machine based one. */
#define VBOX_WATCHDOG_GLOBAL_PERFCOL

/** External globals. */
bool                 g_fVerbose    = false;
ComPtr<IVirtualBox>  g_pVirtualBox = NULL;
ComPtr<ISession>     g_pSession    = NULL;

/** The critical section for keep our stuff in sync. */
static RTCRITSECT    g_MapCritSect;

/** Set by the signal handler. */
static volatile bool g_fCanceled = false;

/** Logging parameters. */
static uint32_t      g_cHistory = 10;                   /* Enable log rotation, 10 files. */
static uint32_t      g_uHistoryFileTime = RT_SEC_1DAY;  /* Max 1 day per file. */
static uint64_t      g_uHistoryFileSize = 100 * _1M;    /* Max 100MB per file. */

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
/** Run in background. */
static bool          g_fDaemonize = false;
#endif

/**
 * The details of the services that has been compiled in.
 */
static struct
{
    /** Pointer to the service descriptor. */
    PCVBOXMODULE    pDesc;
    /** Whether Pre-init was called. */
    bool            fPreInited;
    /** Whether the module is enabled or not. */
    bool            fEnabled;
} g_aModules[] =
{
    { &g_ModBallooning, false /* Pre-inited */, true /* Enabled */ }
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

/** Global static objects. */
static ComPtr<IVirtualBoxClient> g_pVirtualBoxClient = NULL;
static ComPtr<IEventSource> g_pEventSource = NULL;
static ComPtr<IEventSource> g_pEventSourceClient = NULL;
static ComPtr<IEventListener> g_pVBoxEventListener = NULL;
# ifdef VBOX_WATCHDOG_GLOBAL_PERFCOL
static ComPtr<IPerformanceCollector> g_pPerfCollector = NULL;
# endif
static EventQueue *g_pEventQ = NULL;
static mapVM g_mapVM;

/* Prototypes. */
static bool machineIsRunning(MachineState_T enmState);
static bool machineHandled(const Bstr &strUuid);
static int machineAdd(const Bstr &strUuid);
static int machineRemove(const Bstr &strUuid);
static int machineUpdate(const Bstr &strUuid, MachineState_T enmState);
static HRESULT watchdogSetup();
static void watchdogTeardown();

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
                            for (unsigned j = 0; j < RT_ELEMENTS(g_aModules); j++)
                                if (g_aModules[j].fEnabled)
                                {
                                    int rc2 = fRegistered
                                            ? g_aModules[j].pDesc->pfnOnMachineRegistered(uuid)
                                            : g_aModules[j].pDesc->pfnOnMachineUnregistered(uuid);
                                    if (RT_FAILURE(rc2))
                                        serviceLog("Module '%s' reported an error: %Rrc\n",
                                                   g_aModules[j].pDesc->pszName, rc);
                                    /* Keep going. */
                                }

                            #if 0
                            if (fRegistered && machineHandled(uuid))
                                rc = machineAdd(uuid);
                            else if (!fRegistered)
                                 rc = machineRemove(uuid);

                            int rc2 = RTCritSectLeave(&g_MapCritSect);
                            if (RT_SUCCESS(rc))
                                rc = rc2;
                            AssertRC(rc);
                            #endif
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
                            for (unsigned j = 0; j < RT_ELEMENTS(g_aModules); j++)
                                if (g_aModules[j].fEnabled)
                                {
                                    int rc2 = g_aModules[j].pDesc->pfnOnMachineStateChanged(uuid,
                                                                                            machineState);
                                    if (RT_FAILURE(rc2))
                                        serviceLog("Module '%s' reported an error: %Rrc\n",
                                                   g_aModules[j].pDesc->pszName, rc);
                                    /* Keep going. */
                                }

                            //rc = machineUpdate(uuid, machineState);
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

                    /* First, notify all modules. */
                    for (unsigned j = 0; j < RT_ELEMENTS(g_aModules); j++)
                        if (g_aModules[j].fEnabled)
                        {
                            int rc2 = g_aModules[j].pDesc->pfnOnServiceStateChanged(RT_BOOL(fAvailable));
                            if (RT_FAILURE(rc2))
                                serviceLog("Module '%s' reported an error: %Rrc\n",
                                           g_aModules[j].pDesc->pszName, rc2);
                            /* Keep going. */
                        }

                    /* Do global teardown/re-creation stuff. */
                    if (!fAvailable)
                    {
                        serviceLog("VBoxSVC became unavailable\n");
                        watchdogTeardown();
                    }
                    else
                    {
                        serviceLog("VBoxSVC became available\n");
                        HRESULT hrc = watchdogSetup();
                        if (FAILED(hrc))
                            serviceLog("Unable to re-set up watchdog (rc=%Rhrc)!\n", hrc);
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

    #if 0
        if (   balloonGetMaxSize(machine)
            && machineIsRunning(machineState))
        {
            serviceLogVerbose(("Handling machine \"%ls\"\n", strUuid.raw()));
            fHandled = true;
        }
    #endif
    }
    while (0);

    return fHandled;
}

/**
 * Adds a specified machine to the list (map) of handled machines.
 * Does not do locking -- needs to be done by caller!
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

    #if 0
        if (   !balloonGetMaxSize(machine)
            || !machineIsRunning(machineState))
        {
            /* This machine does not need to be added, just skip it! */
            break;
        }
    #endif

        VBOXWATCHDOG_MACHINE m;
        m.machine = machine;

////// TODO: Put this in module!

        /*
         * Setup metrics.
         */
        com::SafeArray<BSTR> metricNames(1);
        com::SafeIfaceArray<IUnknown> metricObjects(1);
        com::SafeIfaceArray<IPerformanceMetric> metricAffected;

        Bstr strMetricNames(L"Guest/RAM/Usage");
        strMetricNames.cloneTo(&metricNames[0]);

        m.machine.queryInterfaceTo(&metricObjects[0]);

#ifdef VBOX_WATCHDOG_GLOBAL_PERFCOL
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

///////////////// TODO END

        /*
         * Add machine to map.
         */
        mapVMIter it = g_mapVM.find(strUuid);
        Assert(it == g_mapVM.end());

        /* Register all module payloads. */
        /* TODO */

        g_mapVM.insert(std::make_pair(strUuid, m));

        serviceLogVerbose(("Added machine \"%ls\"\n", strUuid.raw()));

    } while (0);

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_COM_IPRT_ERROR; /* @todo Find a better error! */
}

/**
 * Removes a specified machine from the list of handled machines.
 * Does not do locking -- needs to be done by caller!
 *
 * @return  IPRT status code.
 * @param   strUuid                 UUID of the specified machine.
 */
static int machineRemove(const Bstr &strUuid)
{
    int rc = VINF_SUCCESS;

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

    return rc;
}

/**
 * Updates a specified machine according to its current machine state.
 * That currently also could mean that a machine gets removed if it doesn't
 * fit in our criteria anymore or a machine gets added if we need to handle
 * it now (and didn't before).
 * Does not do locking -- needs to be done by caller!
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
#if 0
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
#endif
    }

    /*
     * Ballooning stuff - end.
     */

    return rc;
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
#ifndef VBOX_WATCHDOG_GLOBAL_PERFCOL
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

/**
 * Lazily calls the pfnPreInit method on each service.
 *
 * @returns VBox status code, error message displayed.
 */
static int watchdogLazyPreInit(void)
{
    for (unsigned j = 0; j < RT_ELEMENTS(g_aModules); j++)
        if (!g_aModules[j].fPreInited)
        {
            int rc = g_aModules[j].pDesc->pfnPreInit();
            if (RT_FAILURE(rc))
            {
                serviceLog("Module '%s' failed pre-init: %Rrc\n",
                           g_aModules[j].pDesc->pszName, rc);
                return rc;
            }
            g_aModules[j].fPreInited = true;
        }
    return VINF_SUCCESS;
}

/**
 * Starts all registered modules.
 *
 * @return  IPRT status code.
 * @return  int
 */
static int watchdogStartModules()
{
    int rc = VINF_SUCCESS;

    for (unsigned j = 0; j < RT_ELEMENTS(g_aModules); j++)
        if (g_aModules[j].fEnabled)
        {
            rc = g_aModules[j].pDesc->pfnInit();
            if (RT_FAILURE(rc))
            {
                if (rc != VERR_SERVICE_DISABLED)
                {
                    serviceLog("Module '%s' failed to initialize: %Rrc\n",
                               g_aModules[j].pDesc->pszName, rc);
                    return rc;
                }
                g_aModules[j].fEnabled = false;
                serviceLog(0, "Module '%s' was disabled because of missing functionality\n",
                           g_aModules[j].pDesc->pszName);

            }
        }

    return rc;
}

static int watchdogShutdownModules()
{
    int rc = VINF_SUCCESS;

    for (unsigned j = 0; j < RT_ELEMENTS(g_aModules); j++)
        if (g_aModules[j].fEnabled)
        {
            int rc2 = g_aModules[j].pDesc->pfnStop();
            if (RT_FAILURE(rc2))
            {
                serviceLog("Module '%s' failed to stop: %Rrc\n",
                           g_aModules[j].pDesc->pszName, rc);
                /* Keep original rc. */
                if (RT_SUCCESS(rc))
                    rc = rc2;
            }
            /* Keep going. */
        }

    for (unsigned j = 0; j < RT_ELEMENTS(g_aModules); j++)
        if (g_aModules[j].fEnabled)
        {
            g_aModules[j].pDesc->pfnTerm();
        }

    return rc;
}

static RTEXITCODE watchdogMain(HandlerArg *a)
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
         * Set up modules.
         */
        rc = watchdogStartModules();
        if (FAILED(rc))
            break;

        for (;;)
        {
            /*
             * Do the actual work.
             */
            /*
            vrc = balloonCtrlCheck();
            if (RT_FAILURE(vrc))
            {
                serviceLog("Error while doing ballooning control; rc=%Rrc\n", vrc);
                break;
            }*/
            for (unsigned j = 0; j < RT_ELEMENTS(g_aModules); j++)
                if (g_aModules[j].fEnabled)
                {
                    int rc2 = g_aModules[j].pDesc->pfnMain();
                    if (RT_FAILURE(rc2))
                        serviceLog("Module '%s' reported an error: %Rrc\n",
                                   g_aModules[j].pDesc->pszName, rc);
                    /* Keep going. */
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

        vrc = watchdogShutdownModules();
        AssertRC(vrc);

        RTCritSectDelete(&g_MapCritSect);

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

/**
 * Displays the help.
 *
 * @param   pszImage                Name of program name (image).
 */
static void displayHelp(const char *pszImage)
{
    AssertPtrReturnVoid(pszImage);

    RTStrmPrintf(g_pStdErr, "\nUsage: %s [options]\n\nSupported options (default values in brackets):\n",
                 pszImage);
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

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
            case 'b':
                pcszDescr = "Run in background (daemon mode).";
                break;
#endif
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

    RTStrmPrintf(g_pStdErr, "Module options:\n");

    /** @todo Add module options here. */

    /** @todo Change VBOXBALLOONCTRL_RELEASE_LOG to WATCHDOG*. */
    RTStrmPrintf(g_pStdErr, "\nUse environment variable VBOXBALLOONCTRL_RELEASE_LOG for logging options.\n");
}

/**
 * Creates all global COM objects.
 *
 * @return  HRESULT
 */
static HRESULT watchdogSetup()
{
    serviceLogVerbose(("Creating local objects ...\n"));

    HRESULT rc = g_pVirtualBoxClient->COMGETTER(VirtualBox)(g_pVirtualBox.asOutParam());
    if (FAILED(rc))
    {
        RTMsgError("Failed to get VirtualBox object (rc=%Rhrc)!", rc);
    }
    else
    {
        rc = g_pSession.createInprocObject(CLSID_Session);
        if (FAILED(rc))
            RTMsgError("Failed to create a session object (rc=%Rhrc)!", rc);
    }

    do
    {
        /*
         * Setup metrics.
         */
#ifdef VBOX_WATCHDOG_GLOBAL_PERFCOL
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

static void watchdogTeardown()
{
    serviceLogVerbose(("Deleting local objects ...\n"));

    vmListDestroy();

#ifdef VBOX_WATCHDOG_GLOBAL_PERFCOL
    g_pPerfCollector.setNull();
#endif

    g_pSession.setNull();
    g_pVirtualBox.setNull();
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

    RTPrintf(VBOX_PRODUCT " Watchdog " VBOX_VERSION_STRING "\n"
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
    RTGetOptInit(&GetState, argc, argv,
                 g_aOptions, RT_ELEMENTS(g_aOptions), 1 /* First */, 0 /*fFlags*/);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'h':
                displayHelp(argv[0]);
                return 0;

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
            {
                bool fFound = false;

                /** @todo Add "--disable-<module>" etc. here! */

                if (!fFound)
                {
                    rc = watchdogLazyPreInit();
                    if (rc != RTEXITCODE_SUCCESS)
                        return rc;

                    for (unsigned j = 0; !fFound && j < RT_ELEMENTS(g_aModules); j++)
                    {
                        rc = g_aModules[j].pDesc->pfnOption(&ValueUnion, c);
                        fFound = rc == 0;
                        if (fFound)
                            break;
                        if (rc != -1)
                            return rc;
                    }
                }
                if (!fFound)
                    return RTGetOptPrintError(c, &ValueUnion);
                continue;
            }
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
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to initialize COM (%Rhrc)!", hrc);

    hrc = g_pVirtualBoxClient.createInprocObject(CLSID_VirtualBoxClient);
    if (FAILED(hrc))
    {
        RTMsgError("Failed to create the VirtualBoxClient object (%Rhrc)!", hrc);
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

    hrc = watchdogSetup();
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    HandlerArg handlerArg = { argc, argv };
    RTEXITCODE rcExit = watchdogMain(&handlerArg);

    EventQueue::getMainEventQueue()->processEventQueue(0);

    watchdogTeardown();

    g_pVirtualBoxClient.setNull();

    com::Shutdown();

    return rcExit;
#else  /* VBOX_ONLY_DOCS */
    return RTEXITCODE_SUCCESS;
#endif /* VBOX_ONLY_DOCS */
}

