/* $Id$ */
/** @file
 * XPCOM server process (VBoxSVC) start point.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <ipcIService.h>
#include <ipcCID.h>

#include <nsIComponentRegistrar.h>

#ifdef XPCOM_GLUE
# include <nsXPCOMGlue.h>
#endif

#include <nsEventQueueUtils.h>
#include <nsGenericFactory.h>

#include "prio.h"
#include "prproces.h"

#include "server.h"

#include "Logging.h"

#include <VBox/param.h>
#include <VBox/version.h>

#include <iprt/buildconfig.h>
#include <iprt/initterm.h>
#include <iprt/critsect.h>
#include <iprt/getopt.h>
#include <iprt/message.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/path.h>
#include <iprt/timer.h>
#include <iprt/env.h>

#include <signal.h>     // for the signal handler
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>

/////////////////////////////////////////////////////////////////////////////
// VirtualBox component instantiation
/////////////////////////////////////////////////////////////////////////////

#include <nsIGenericFactory.h>
#include <VirtualBox_XPCOM.h>

#include "ApplianceImpl.h"
#include "AudioAdapterImpl.h"
#include "BandwidthControlImpl.h"
#include "BandwidthGroupImpl.h"
#include "DHCPServerRunner.h"
#include "DHCPServerImpl.h"
#include "GuestOSTypeImpl.h"
#include "HostImpl.h"
#include "HostNetworkInterfaceImpl.h"
#include "MachineImpl.h"
#include "MediumFormatImpl.h"
#include "MediumImpl.h"
#include "NATEngineImpl.h"
#include "NetworkAdapterImpl.h"
#include "ParallelPortImpl.h"
#include "ProgressCombinedImpl.h"
#include "ProgressProxyImpl.h"
#include "SerialPortImpl.h"
#include "SharedFolderImpl.h"
#include "SnapshotImpl.h"
#include "StorageControllerImpl.h"
#include "SystemPropertiesImpl.h"
#include "USBControllerImpl.h"
#include "VFSExplorerImpl.h"
#include "VirtualBoxImpl.h"
#include "VRDEServerImpl.h"
#ifdef VBOX_WITH_USB
# include "HostUSBDeviceImpl.h"
# include "USBDeviceFilterImpl.h"
# include "USBDeviceImpl.h"
#endif
#ifdef VBOX_WITH_EXTPACK
# include "ExtPackManagerImpl.h"
#endif

/* implement nsISupports parts of our objects with support for nsIClassInfo */

NS_DECL_CLASSINFO(VirtualBox)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VirtualBox, IVirtualBox)

NS_DECL_CLASSINFO(Machine)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Machine, IMachine)

NS_DECL_CLASSINFO(VFSExplorer)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VFSExplorer, IVFSExplorer)

NS_DECL_CLASSINFO(Appliance)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Appliance, IAppliance)

NS_DECL_CLASSINFO(VirtualSystemDescription)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VirtualSystemDescription, IVirtualSystemDescription)

NS_DECL_CLASSINFO(SessionMachine)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(SessionMachine, IMachine, IInternalMachineControl)

NS_DECL_CLASSINFO(SnapshotMachine)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(SnapshotMachine, IMachine)

NS_DECL_CLASSINFO(Snapshot)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Snapshot, ISnapshot)

NS_DECL_CLASSINFO(Medium)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Medium, IMedium)

NS_DECL_CLASSINFO(MediumFormat)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(MediumFormat, IMediumFormat)

NS_DECL_CLASSINFO(MediumAttachment)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(MediumAttachment, IMediumAttachment)

NS_DECL_CLASSINFO(Progress)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Progress, IProgress)

NS_DECL_CLASSINFO(CombinedProgress)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(CombinedProgress, IProgress)

NS_DECL_CLASSINFO(ProgressProxy)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(ProgressProxy, IProgress)

NS_DECL_CLASSINFO(SharedFolder)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(SharedFolder, ISharedFolder)

NS_DECL_CLASSINFO(VRDEServer)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VRDEServer, IVRDEServer)

NS_DECL_CLASSINFO(Host)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Host, IHost)

NS_DECL_CLASSINFO(HostNetworkInterface)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(HostNetworkInterface, IHostNetworkInterface)

NS_DECL_CLASSINFO(DHCPServer)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(DHCPServer, IDHCPServer)

NS_DECL_CLASSINFO(GuestOSType)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(GuestOSType, IGuestOSType)

NS_DECL_CLASSINFO(NetworkAdapter)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(NetworkAdapter, INetworkAdapter)

NS_DECL_CLASSINFO(NATEngine)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(NATEngine, INATEngine)


NS_DECL_CLASSINFO(SerialPort)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(SerialPort, ISerialPort)

NS_DECL_CLASSINFO(ParallelPort)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(ParallelPort, IParallelPort)

NS_DECL_CLASSINFO(USBController)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(USBController, IUSBController)

NS_DECL_CLASSINFO(StorageController)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(StorageController, IStorageController)

#ifdef VBOX_WITH_USB
NS_DECL_CLASSINFO(USBDeviceFilter)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(USBDeviceFilter, IUSBDeviceFilter)

NS_DECL_CLASSINFO(HostUSBDevice)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(HostUSBDevice, IUSBDevice, IHostUSBDevice)

NS_DECL_CLASSINFO(HostUSBDeviceFilter)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(HostUSBDeviceFilter, IUSBDeviceFilter, IHostUSBDeviceFilter)
#endif

NS_DECL_CLASSINFO(AudioAdapter)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(AudioAdapter, IAudioAdapter)

NS_DECL_CLASSINFO(SystemProperties)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(SystemProperties, ISystemProperties)

#ifdef VBOX_WITH_RESOURCE_USAGE_API
NS_DECL_CLASSINFO(PerformanceCollector)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(PerformanceCollector, IPerformanceCollector)
NS_DECL_CLASSINFO(PerformanceMetric)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(PerformanceMetric, IPerformanceMetric)
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

NS_DECL_CLASSINFO(BIOSSettings)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(BIOSSettings, IBIOSSettings)

#ifdef VBOX_WITH_EXTPACK
NS_DECL_CLASSINFO(ExtPackFile)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(ExtPackFile, IExtPackFile)

NS_DECL_CLASSINFO(ExtPack)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(ExtPack, IExtPack)

NS_DECL_CLASSINFO(ExtPackManager)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(ExtPackManager, IExtPackManager)
#endif

NS_DECL_CLASSINFO(BandwidthGroup)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(BandwidthGroup, IBandwidthGroup)

NS_DECL_CLASSINFO(BandwidthControl)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(BandwidthControl, IBandwidthControl)

////////////////////////////////////////////////////////////////////////////////

enum
{
    /* Delay before shutting down the VirtualBox server after the last
     * VirtualBox instance is released, in ms */
    VBoxSVC_ShutdownDelay = 5000
};

static bool gAutoShutdown = false;

static nsIEventQueue  *gEventQ          = nsnull;
static PRBool volatile gKeepRunning     = PR_TRUE;
static PRBool volatile gAllowSigUsrQuit = PR_TRUE;

/////////////////////////////////////////////////////////////////////////////

/**
 * Simple but smart PLEvent wrapper.
 *
 * @note Instances must be always created with <tt>operator new</tt>!
 */
class MyEvent
{
public:

    MyEvent()
    {
        mEv.that = NULL;
    };

    /**
     * Posts this event to the given message queue. This method may only be
     * called once. @note On success, the event will be deleted automatically
     * after it is delivered and handled. On failure, the event will delete
     * itself before this method returns! The caller must not delete it in
     * either case.
     */
    nsresult postTo(nsIEventQueue *aEventQ)
    {
        AssertReturn(mEv.that == NULL, NS_ERROR_FAILURE);
        AssertReturn(aEventQ, NS_ERROR_FAILURE);
        nsresult rv = aEventQ->InitEvent(&mEv.e, NULL,
                                         eventHandler, eventDestructor);
        if (NS_SUCCEEDED(rv))
        {
            mEv.that = this;
            rv = aEventQ->PostEvent(&mEv.e);
            if (NS_SUCCEEDED(rv))
                return rv;
        }
        delete this;
        return rv;
    }

    virtual void *handler() = 0;

private:

    struct Ev
    {
        PLEvent e;
        MyEvent *that;
    } mEv;

    static void *PR_CALLBACK eventHandler(PLEvent *self)
    {
        return reinterpret_cast<Ev *>(self)->that->handler();
    }

    static void PR_CALLBACK eventDestructor(PLEvent *self)
    {
        delete reinterpret_cast<Ev *>(self)->that;
    }
};

////////////////////////////////////////////////////////////////////////////////

/**
 *  VirtualBox class factory that destroys the created instance right after
 *  the last reference to it is released by the client, and recreates it again
 *  when necessary (so VirtualBox acts like a singleton object).
 */
class VirtualBoxClassFactory : public VirtualBox
{
public:

    virtual ~VirtualBoxClassFactory()
    {
        LogFlowFunc(("Deleting VirtualBox...\n"));

        FinalRelease();
        sInstance = NULL;

        LogFlowFunc(("VirtualBox object deleted.\n"));
        RTPrintf("Informational: VirtualBox object deleted.\n");
    }

    NS_IMETHOD_(nsrefcnt) Release()
    {
        /* we overload Release() to guarantee the VirtualBox destructor is
         * always called on the main thread */

        nsrefcnt count = VirtualBox::Release();

        if (count == 1)
        {
            /* the last reference held by clients is being released
             * (see GetInstance()) */

            PRBool onMainThread = PR_TRUE;
            if (gEventQ)
                gEventQ->IsOnCurrentThread(&onMainThread);

            PRBool timerStarted = PR_FALSE;

            /* sTimer is null if this call originates from FactoryDestructor()*/
            if (sTimer != NULL)
            {
                LogFlowFunc(("Last VirtualBox instance was released.\n"));
                LogFlowFunc(("Scheduling server shutdown in %d ms...\n",
                             VBoxSVC_ShutdownDelay));

                /* make sure the previous timer (if any) is stopped;
                 * otherwise RTTimerStart() will definitely fail. */
                RTTimerLRStop(sTimer);

                int vrc = RTTimerLRStart(sTimer, uint64_t(VBoxSVC_ShutdownDelay) * 1000000);
                AssertRC(vrc);
                timerStarted = SUCCEEDED(vrc);
            }
            else
            {
                LogFlowFunc(("Last VirtualBox instance was released "
                             "on XPCOM shutdown.\n"));
                Assert(onMainThread);
            }

            gAllowSigUsrQuit = PR_TRUE;

            if (!timerStarted)
            {
                if (!onMainThread)
                {
                    /* Failed to start the timer, post the shutdown event
                     * manually if not on the main thread already. */
                    ShutdownTimer(NULL, NULL, 0);
                }
                else
                {
                    /* Here we come if:
                     *
                     * a) gEventQ is 0 which means either FactoryDestructor() is called
                     *    or the IPC/DCONNECT shutdown sequence is initiated by the
                     *    XPCOM shutdown routine (NS_ShutdownXPCOM()), which always
                     *    happens on the main thread.
                     *
                     * b) gEventQ has reported we're on the main thread. This means
                     *    that DestructEventHandler() has been called, but another
                     *    client was faster and requested VirtualBox again.
                     *
                     * In either case, there is nothing to do.
                     *
                     * Note: case b) is actually no more valid since we don't
                     * call Release() from DestructEventHandler() in this case
                     * any more. Thus, we assert below.
                     */

                    Assert(gEventQ == NULL);
                }
            }
        }

        return count;
    }

    class MaybeQuitEvent : public MyEvent
    {
        /* called on the main thread */
        void *handler()
        {
            LogFlowFunc(("\n"));

            Assert(RTCritSectIsInitialized(&sLock));

            /* stop accepting GetInstance() requests on other threads during
             * possible destruction */
            RTCritSectEnter(&sLock);

            nsrefcnt count = 0;

            /* sInstance is NULL here if it was deleted immediately after
             * creation due to initialization error. See GetInstance(). */
            if (sInstance != NULL)
            {
                /* Release the guard reference added in GetInstance() */
                count = sInstance->Release();
            }

            if (count == 0)
            {
                if (gAutoShutdown)
                {
                    Assert(sInstance == NULL);
                    LogFlowFunc(("Terminating the server process...\n"));
                    /* make it leave the event loop */
                    gKeepRunning = PR_FALSE;
                }
            }
            else
            {
                /* This condition is quite rare: a new client happened to
                 * connect after this event has been posted to the main queue
                 * but before it started to process it. */
                LogFlowFunc(("Destruction is canceled (refcnt=%d).\n", count));
            }

            RTCritSectLeave(&sLock);

            return NULL;
        }
    };

    static void ShutdownTimer(RTTIMERLR hTimerLR, void *pvUser, uint64_t /*iTick*/)
    {
        NOREF(hTimerLR);
        NOREF(pvUser);

        /* A "too late" event is theoretically possible if somebody
         * manually ended the server after a destruction has been scheduled
         * and this method was so lucky that it got a chance to run before
         * the timer was killed. */
        AssertReturnVoid(gEventQ);

        /* post a quit event to the main queue */
        MaybeQuitEvent *ev = new MaybeQuitEvent();
        nsresult rv = ev->postTo(gEventQ);
        NOREF(rv);

        /* A failure above means we've been already stopped (for example
         * by Ctrl-C). FactoryDestructor() (NS_ShutdownXPCOM())
         * will do the job. Nothing to do. */
    }

    static NS_IMETHODIMP FactoryConstructor()
    {
        LogFlowFunc(("\n"));

        /* create a critsect to protect object construction */
        if (RT_FAILURE(RTCritSectInit(&sLock)))
            return NS_ERROR_OUT_OF_MEMORY;

        int vrc = RTTimerLRCreateEx(&sTimer, 0, 0, ShutdownTimer, NULL);
        if (RT_FAILURE(vrc))
        {
            LogFlowFunc(("Failed to create a timer! (vrc=%Rrc)\n", vrc));
            return NS_ERROR_FAILURE;
        }

        return NS_OK;
    }

    static NS_IMETHODIMP FactoryDestructor()
    {
        LogFlowFunc(("\n"));

        RTTimerLRDestroy(sTimer);
        sTimer = NULL;

        RTCritSectDelete(&sLock);

        if (sInstance != NULL)
        {
            /* Either posting a destruction event failed for some reason (most
             * likely, the quit event has been received before the last release),
             * or the client has terminated abnormally w/o releasing its
             * VirtualBox instance (so NS_ShutdownXPCOM() is doing a cleanup).
             * Release the guard reference we added in GetInstance(). */
            sInstance->Release();
        }

        return NS_OK;
    }

    static nsresult GetInstance(VirtualBox **inst)
    {
        LogFlowFunc(("Getting VirtualBox object...\n"));

        RTCritSectEnter(&sLock);

        if (!gKeepRunning)
        {
            LogFlowFunc(("Process termination requested first. Refusing.\n"));

            RTCritSectLeave(&sLock);

            /* this rv is what CreateInstance() on the client side returns
             * when the server process stops accepting events. Do the same
             * here. The client wrapper should attempt to start a new process in
             * response to a failure from us. */
            return NS_ERROR_ABORT;
        }

        nsresult rv = NS_OK;

        if (sInstance == NULL)
        {
            LogFlowFunc (("Creating new VirtualBox object...\n"));
            sInstance = new VirtualBoxClassFactory();
            if (sInstance != NULL)
            {
                /* make an extra AddRef to take the full control
                 * on the VirtualBox destruction (see FinalRelease()) */
                sInstance->AddRef();

                sInstance->AddRef(); /* protect FinalConstruct() */
                rv = sInstance->FinalConstruct();
                RTPrintf("Informational: VirtualBox object created (rc=%Rhrc).\n", rv);
                if (NS_FAILED(rv))
                {
                    /* On failure diring VirtualBox initialization, delete it
                     * immediately on the current thread by releasing all
                     * references in order to properly schedule the server
                     * shutdown. Since the object is fully deleted here, there
                     * is a chance to fix the error and request a new
                     * instantiation before the server terminates. However,
                     * the main reason to maintain the shutdown delay on
                     * failure is to let the front-end completely fetch error
                     * info from a server-side IVirtualBoxErrorInfo object. */
                    sInstance->Release();
                    sInstance->Release();
                    Assert(sInstance == NULL);
                }
                else
                {
                    /* On success, make sure the previous timer is stopped to
                     * cancel a scheduled server termination (if any). */
                    gAllowSigUsrQuit = PR_FALSE;
                    RTTimerLRStop(sTimer);
                }
            }
            else
            {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
        }
        else
        {
            LogFlowFunc(("Using existing VirtualBox object...\n"));
            nsrefcnt count = sInstance->AddRef();
            Assert(count > 1);

            if (count == 2)
            {
                LogFlowFunc(("Another client has requested a reference to VirtualBox, canceling destruction...\n"));

                /* make sure the previous timer is stopped */
                gAllowSigUsrQuit = PR_FALSE;
                RTTimerLRStop(sTimer);
            }
        }

        *inst = sInstance;

        RTCritSectLeave(&sLock);

        return rv;
    }

private:

    /* Don't be confused that sInstance is of the *ClassFactory type. This is
     * actually a singleton instance (*ClassFactory inherits the singleton
     * class; we combined them just for "simplicity" and used "static" for
     * factory methods. *ClassFactory here is necessary for a couple of extra
     * methods. */

    static VirtualBoxClassFactory *sInstance;
    static RTCRITSECT sLock;

    static RTTIMERLR sTimer;
};

VirtualBoxClassFactory *VirtualBoxClassFactory::sInstance = NULL;
RTCRITSECT VirtualBoxClassFactory::sLock;

RTTIMERLR VirtualBoxClassFactory::sTimer = NIL_RTTIMERLR;

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR_WITH_RC(VirtualBox, VirtualBoxClassFactory::GetInstance)

////////////////////////////////////////////////////////////////////////////////

typedef NSFactoryDestructorProcPtr NSFactoryConstructorProcPtr;

/**
 *  Enhanced module component information structure.
 *
 *  nsModuleComponentInfo lacks the factory construction callback, here we add
 *  it. This callback is called straight after a nsGenericFactory instance is
 *  successfully created in RegisterSelfComponents.
 */
struct nsModuleComponentInfoPlusFactoryConstructor
{
    /** standard module component information */
    const nsModuleComponentInfo *mpModuleComponentInfo;
    /** (optional) Factory Construction Callback */
    NSFactoryConstructorProcPtr mFactoryConstructor;
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Helper function to register self components upon start-up
 * of the out-of-proc server.
 */
static nsresult
RegisterSelfComponents(nsIComponentRegistrar *registrar,
                       const nsModuleComponentInfoPlusFactoryConstructor *aComponents,
                       PRUint32 count)
{
    nsresult rc = NS_OK;
    const nsModuleComponentInfoPlusFactoryConstructor *info = aComponents;
    for (PRUint32 i = 0; i < count && NS_SUCCEEDED(rc); i++, info++)
    {
        /* skip components w/o a constructor */
        if (!info->mpModuleComponentInfo->mConstructor)
            continue;
        /* create a new generic factory for a component and register it */
        nsIGenericFactory *factory;
        rc = NS_NewGenericFactory(&factory, info->mpModuleComponentInfo);
        if (NS_SUCCEEDED(rc) && info->mFactoryConstructor)
        {
            rc = info->mFactoryConstructor();
            if (NS_FAILED(rc))
                NS_RELEASE(factory);
        }
        if (NS_SUCCEEDED(rc))
        {
            rc = registrar->RegisterFactory(info->mpModuleComponentInfo->mCID,
                                            info->mpModuleComponentInfo->mDescription,
                                            info->mpModuleComponentInfo->mContractID,
                                            factory);
            NS_RELEASE(factory);
        }
    }
    return rc;
}

/////////////////////////////////////////////////////////////////////////////

static ipcIService *gIpcServ = nsnull;
static const char *g_pszPidFile = NULL;

class ForceQuitEvent : public MyEvent
{
    void *handler()
    {
        LogFlowFunc(("\n"));

        gKeepRunning = PR_FALSE;

        if (g_pszPidFile)
            RTFileDelete(g_pszPidFile);

        return NULL;
    }
};

static void signal_handler(int sig)
{
    if (gEventQ && gKeepRunning)
    {
        if (sig == SIGUSR1)
        {
            if (gAllowSigUsrQuit)
            {
                VirtualBoxClassFactory::MaybeQuitEvent *ev = new VirtualBoxClassFactory::MaybeQuitEvent();
                ev->postTo(gEventQ);
            }
            /* else do nothing */
        }
        else
        {
            /* post a force quit event to the queue */
            ForceQuitEvent *ev = new ForceQuitEvent();
            ev->postTo(gEventQ);
        }
    }
}

static nsresult vboxsvcSpawnDaemonByReExec(const char *pszPath, bool fAutoShutdown, const char *pszPidFile)
{
    PRFileDesc *readable = nsnull, *writable = nsnull;
    PRProcessAttr *attr = nsnull;
    nsresult rv = NS_ERROR_FAILURE;
    PRFileDesc *devNull;
    unsigned args_index = 0;
    // The ugly casts are necessary because the PR_CreateProcessDetached has
    // a const array of writable strings as a parameter. It won't write. */
    char * args[1 + 1 + 2 + 1];
    args[args_index++] = (char *)pszPath;
    if (fAutoShutdown)
        args[args_index++] = (char *)"--auto-shutdown";
    if (pszPidFile)
    {
        args[args_index++] = (char *)"--pidfile";
        args[args_index++] = (char *)pszPidFile;
    }
    args[args_index++] = 0;

    // Use a pipe to determine when the daemon process is in the position
    // to actually process requests. The daemon will write "READY" to the pipe.
    if (PR_CreatePipe(&readable, &writable) != PR_SUCCESS)
        goto end;
    PR_SetFDInheritable(writable, PR_TRUE);

    attr = PR_NewProcessAttr();
    if (!attr)
        goto end;

    if (PR_ProcessAttrSetInheritableFD(attr, writable, VBOXSVC_STARTUP_PIPE_NAME) != PR_SUCCESS)
        goto end;

    devNull = PR_Open("/dev/null", PR_RDWR, 0);
    if (!devNull)
        goto end;

    PR_ProcessAttrSetStdioRedirect(attr, PR_StandardInput, devNull);
    PR_ProcessAttrSetStdioRedirect(attr, PR_StandardOutput, devNull);
    PR_ProcessAttrSetStdioRedirect(attr, PR_StandardError, devNull);

    if (PR_CreateProcessDetached(pszPath, (char * const *)args, nsnull, attr) != PR_SUCCESS)
        goto end;

    // Close /dev/null
    PR_Close(devNull);
    // Close the child end of the pipe to make it the only owner of the
    // file descriptor, so that unexpected closing can be detected.
    PR_Close(writable);
    writable = nsnull;

    char msg[10];
    memset(msg, '\0', sizeof(msg));
    if (   PR_Read(readable, msg, sizeof(msg)-1) != 5
        || strcmp(msg, "READY"))
        goto end;

    rv = NS_OK;

end:
    if (readable)
        PR_Close(readable);
    if (writable)
        PR_Close(writable);
    if (attr)
        PR_DestroyProcessAttr(attr);
    return rv;
}

int main(int argc, char **argv)
{
    /*
     * Initialize the VBox runtime without loading
     * the support driver
     */
    RTR3Init();

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--automate",         'a', RTGETOPT_REQ_NOTHING },
        { "--auto-shutdown",    'A', RTGETOPT_REQ_NOTHING },
        { "--daemonize",        'd', RTGETOPT_REQ_NOTHING },
        { "--pidfile",          'p', RTGETOPT_REQ_STRING  },
        { "--logfile",          'F', RTGETOPT_REQ_STRING },
        { "--logrotate",        'R', RTGETOPT_REQ_UINT32 },
        { "--logsize",          'S', RTGETOPT_REQ_UINT64 },
        { "--loginterval",      'I', RTGETOPT_REQ_UINT32 }
    };

    const char      *pszLogFile = NULL;
    uint32_t        cHistory = 10;                  // enable log rotation, 10 files
    uint32_t        uHistoryFileTime = RT_SEC_1DAY; // max 1 day per file
    uint64_t        uHistoryFileSize = 100 * _1M;   // max 100MB per file
    bool            fDaemonize = false;
    PRFileDesc      *daemon_pipe_wr = nsnull;

    RTGETOPTSTATE   GetOptState;
    int vrc = RTGetOptInit(&GetOptState, argc, argv, &s_aOptions[0], RT_ELEMENTS(s_aOptions), 1, 0 /*fFlags*/);
    AssertRC(vrc);

    RTGETOPTUNION   ValueUnion;
    while ((vrc = RTGetOpt(&GetOptState, &ValueUnion)))
    {
        switch (vrc)
        {
            case 'a':
            {
                /* --automate mode means we are started by XPCOM on
                 * demand. Daemonize ourselves and activate
                 * auto-shutdown. */
                gAutoShutdown = true;
                fDaemonize = true;
                break;
            }

            /* --auto-shutdown mode means we're already daemonized. */
            case 'A':
            {
                gAutoShutdown = true;
                break;
            }

            case 'd':
            {
                fDaemonize = true;
                break;
            }

            case 'p':
            {
                g_pszPidFile = ValueUnion.psz;
                break;
            }

            case 'F':
                pszLogFile = ValueUnion.psz;
                break;

            case 'R':
                cHistory = ValueUnion.u32;
                break;

            case 'S':
                uHistoryFileSize = ValueUnion.u64;
                break;

            case 'I':
                uHistoryFileTime = ValueUnion.u32;
                break;

            case 'h':
            {
                RTPrintf("no help\n");
                return 1;
            }

            case 'V':
            {
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return 0;
            }

            default:
                return RTGetOptPrintError(vrc, &ValueUnion);
        }
    }

    if (fDaemonize)
    {
        vboxsvcSpawnDaemonByReExec(argv[0], gAutoShutdown, g_pszPidFile);
        exit(126);
    }

    nsresult rc;

    if (!pszLogFile)
    {
        char szLogFile[RTPATH_MAX];
        vrc = com::GetVBoxUserHomeDirectory(szLogFile, sizeof(szLogFile));
        if (RT_SUCCESS(vrc))
            vrc = RTPathAppend(szLogFile, sizeof(szLogFile), "VBoxSVC.log");
        if (RT_SUCCESS(vrc))
            pszLogFile = RTStrDup(szLogFile);
    }
    VBoxSVCLogRelCreate(pszLogFile, cHistory, uHistoryFileTime, uHistoryFileSize);

    daemon_pipe_wr = PR_GetInheritedFD(VBOXSVC_STARTUP_PIPE_NAME);
    RTEnvUnset("NSPR_INHERIT_FDS");

    const nsModuleComponentInfo VirtualBoxInfo = {
        "VirtualBox component",
        NS_VIRTUALBOX_CID,
        NS_VIRTUALBOX_CONTRACTID,
        VirtualBoxConstructor, // constructor function
        NULL, // registration function
        NULL, // deregistration function
        VirtualBoxClassFactory::FactoryDestructor, // factory destructor function
        NS_CI_INTERFACE_GETTER_NAME(VirtualBox),
        NULL, // language helper
        &NS_CLASSINFO_NAME(VirtualBox),
        0 // flags
    };

    const nsModuleComponentInfoPlusFactoryConstructor components[] = {
        {
            &VirtualBoxInfo,
            VirtualBoxClassFactory::FactoryConstructor // factory constructor function
        }
    };

    do
    {
        rc = com::Initialize();
        if (NS_FAILED(rc))
        {
            RTMsgError("Failed to initialize XPCOM! (rc=%Rhrc)\n", rc);
            break;
        }

        nsCOMPtr <nsIComponentRegistrar> registrar;
        rc = NS_GetComponentRegistrar(getter_AddRefs(registrar));
        if (NS_FAILED(rc))
        {
            RTMsgError("Failed to get component registrar! (rc=%Rhrc)", rc);
            break;
        }

        registrar->AutoRegister(nsnull);
        rc = RegisterSelfComponents(registrar, components,
                                    NS_ARRAY_LENGTH(components));
        if (NS_FAILED(rc))
        {
            RTMsgError("Failed to register server components! (rc=%Rhrc)", rc);
            break;
        }

        /* get the main thread's event queue (afaik, the dconnect service always
         * gets created upon XPCOM startup, so it will use the main (this)
         * thread's event queue to receive IPC events) */
        rc = NS_GetMainEventQ(&gEventQ);
        if (NS_FAILED(rc))
        {
            RTMsgError("Failed to get the main event queue! (rc=%Rhrc)", rc);
            break;
        }

        nsCOMPtr<ipcIService> ipcServ (do_GetService(IPC_SERVICE_CONTRACTID, &rc));
        if (NS_FAILED(rc))
        {
            RTMsgError("Failed to get IPC service! (rc=%Rhrc)", rc);
            break;
        }

        NS_ADDREF(gIpcServ = ipcServ);

        LogFlowFunc(("Will use \"%s\" as server name.\n", VBOXSVC_IPC_NAME));

        rc = gIpcServ->AddName(VBOXSVC_IPC_NAME);
        if (NS_FAILED(rc))
        {
            LogFlowFunc(("Failed to register the server name (rc=%Rhrc (%08X))!\n"
                         "Is another server already running?\n", rc, rc));

            RTMsgError("Failed to register the server name \"%s\" (rc=%Rhrc)!\n"
                       "Is another server already running?\n",
                       VBOXSVC_IPC_NAME, rc);
            NS_RELEASE(gIpcServ);
            break;
        }

        {
            /* setup signal handling to convert some signals to a quit event */
            struct sigaction sa;
            sa.sa_handler = signal_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(SIGINT, &sa, NULL);
            sigaction(SIGQUIT, &sa, NULL);
            sigaction(SIGTERM, &sa, NULL);
            sigaction(SIGTRAP, &sa, NULL);
            sigaction(SIGUSR1, &sa, NULL);
        }

        {
            char szBuf[80];
            int  iSize;

            iSize = RTStrPrintf(szBuf, sizeof(szBuf),
                                VBOX_PRODUCT" XPCOM Server Version "
                                VBOX_VERSION_STRING);
            for (int i = iSize; i > 0; i--)
                putchar('*');
            RTPrintf("\n%s\n", szBuf);
            RTPrintf("(C) 2008-" VBOX_C_YEAR " " VBOX_VENDOR "\n"
                     "All rights reserved.\n");
#ifdef DEBUG
            RTPrintf("Debug version.\n");
#endif
        }

        if (daemon_pipe_wr != nsnull)
        {
            RTPrintf("\nStarting event loop....\n[send TERM signal to quit]\n");
            /* now we're ready, signal the parent process */
            PR_Write(daemon_pipe_wr, "READY", strlen("READY"));
            /* close writing end of the pipe, its job is done */
            PR_Close(daemon_pipe_wr);
        }
        else
            RTPrintf("\nStarting event loop....\n[press Ctrl-C to quit]\n");

        if (g_pszPidFile)
        {
            RTFILE hPidFile = NIL_RTFILE;
            vrc = RTFileOpen(&hPidFile, g_pszPidFile, RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE);
            if (RT_SUCCESS(vrc))
            {
                char szBuf[32];
                const char *lf = "\n";
                RTStrFormatNumber(szBuf, getpid(), 10, 0, 0, 0);
                RTFileWrite(hPidFile, szBuf, strlen(szBuf), NULL);
                RTFileWrite(hPidFile, lf, strlen(lf), NULL);
                RTFileClose(hPidFile);
            }
        }

        // Increase the file table size to 10240 or as high as possible.
        struct rlimit lim;
        if (getrlimit(RLIMIT_NOFILE, &lim) == 0)
        {
            if (    lim.rlim_cur < 10240
                &&  lim.rlim_cur < lim.rlim_max)
            {
                lim.rlim_cur = RT_MIN(lim.rlim_max, 10240);
                if (setrlimit(RLIMIT_NOFILE, &lim) == -1)
                    RTPrintf("WARNING: failed to increase file descriptor limit. (%d)\n", errno);
            }
        }
        else
            RTPrintf("WARNING: failed to obtain per-process file-descriptor limit (%d).\n", errno);

        PLEvent *ev;
        while (gKeepRunning)
        {
            gEventQ->WaitForEvent(&ev);
            gEventQ->HandleEvent(ev);
        }

        /* stop accepting new events. Clients that happen to resolve our
         * name and issue a CreateInstance() request after this point will
         * get NS_ERROR_ABORT once we handle the remaining messages. As a
         * result, they should try to start a new server process. */
        gEventQ->StopAcceptingEvents();

        /* unregister ourselves. After this point, clients will start a new
         * process because they won't be able to resolve the server name.*/
        gIpcServ->RemoveName(VBOXSVC_IPC_NAME);

        /* process any remaining events. These events may include
         * CreateInstance() requests received right before we called
         * StopAcceptingEvents() above. We will detect this case below,
         * restore gKeepRunning and continue to serve. */
        gEventQ->ProcessPendingEvents();

        RTPrintf("Terminated event loop.\n");
    }
    while (0); // this scopes the nsCOMPtrs

    NS_IF_RELEASE(gIpcServ);
    NS_IF_RELEASE(gEventQ);

    /* no nsCOMPtrs are allowed to be alive when you call com::Shutdown(). */

    LogFlowFunc(("Calling com::Shutdown()...\n"));
    rc = com::Shutdown();
    LogFlowFunc(("Finished com::Shutdown() (rc=%Rhrc)\n", rc));

    if (NS_FAILED(rc))
        RTMsgError("Failed to shutdown XPCOM! (rc=%Rhrc)", rc);

    RTPrintf("XPCOM server has shutdown.\n");

    if (g_pszPidFile)
        RTFileDelete(g_pszPidFile);

    return 0;
}
