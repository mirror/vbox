/** @file
 *
 * XPCOM server process start point
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
 */

#include <ipcIService.h>
#include <ipcCID.h>

#include <nsIComponentRegistrar.h>

#if defined(XPCOM_GLUE)
#include <nsXPCOMGlue.h>
#endif

#include <nsEventQueueUtils.h>
#include <nsGenericFactory.h>

#include "linux/server.h"

#include "Logging.h"

#include <iprt/runtime.h>
#include <iprt/path.h>
#include <iprt/critsect.h>
#include <iprt/timer.h>

#include <VBox/param.h>
#include <VBox/version.h>

#include <VBox/com/com.h>

#include <stdio.h>

// for the signal handler
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

// for the backtrace signal handler
#if defined(DEBUG) && defined(RT_OS_LINUX)
# define USE_BACKTRACE
#endif
#if defined(USE_BACKTRACE)
# include <execinfo.h>
// get REG_EIP/RIP from ucontext.h
# ifndef __USE_GNU
#  define __USE_GNU
# endif
# include <ucontext.h>
# ifdef RT_ARCH_AMD64
#  define REG_PC REG_RIP
# else
#  define REG_PC REG_EIP
# endif
#endif

/////////////////////////////////////////////////////////////////////////////
// VirtualBox component instantiation
/////////////////////////////////////////////////////////////////////////////

#include <nsIGenericFactory.h>

#include <VirtualBox_XPCOM.h>
#include <VirtualBoxImpl.h>
#include <MachineImpl.h>
#include <SnapshotImpl.h>
#include <HardDiskImpl.h>
#include <ProgressImpl.h>
#include <DVDDriveImpl.h>
#include <FloppyDriveImpl.h>
#include <VRDPServerImpl.h>
#include <DVDImageImpl.h>
#include <FloppyImageImpl.h>
#include <SharedFolderImpl.h>
#include <HostImpl.h>
#include <HostDVDDriveImpl.h>
#include <HostFloppyDriveImpl.h>
#include <HostUSBDeviceImpl.h>
#include <GuestOSTypeImpl.h>
#include <NetworkAdapterImpl.h>
#include <SerialPortImpl.h>
#include <ParallelPortImpl.h>
#include <USBControllerImpl.h>
#include <USBDeviceImpl.h>
#include <SATAControllerImpl.h>
#include <AudioAdapterImpl.h>
#include <SystemPropertiesImpl.h>
#include <Collection.h>

// implement nsISupports parts of our objects with support for nsIClassInfo
NS_DECL_CLASSINFO(VirtualBox)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VirtualBox, IVirtualBox)
NS_DECL_CLASSINFO(Machine)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Machine, IMachine)
NS_DECL_CLASSINFO(SessionMachine)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(SessionMachine, IMachine, IInternalMachineControl)
NS_DECL_CLASSINFO(SnapshotMachine)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(SnapshotMachine, IMachine)
NS_DECL_CLASSINFO(Snapshot)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Snapshot, ISnapshot)
NS_DECL_CLASSINFO(HardDisk)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(HardDisk, IHardDisk)
NS_DECL_CLASSINFO(HVirtualDiskImage)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(HVirtualDiskImage, IHardDisk, IVirtualDiskImage)
NS_DECL_CLASSINFO(HISCSIHardDisk)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(HISCSIHardDisk, IHardDisk, IISCSIHardDisk)
NS_DECL_CLASSINFO(HVMDKImage)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(HVMDKImage, IHardDisk, IVMDKImage)
NS_DECL_CLASSINFO(HCustomHardDisk)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(HCustomHardDisk, IHardDisk, ICustomHardDisk)
NS_DECL_CLASSINFO(HVHDImage)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(HVHDImage, IHardDisk, IVHDImage)
NS_DECL_CLASSINFO(HardDiskAttachment)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(HardDiskAttachment, IHardDiskAttachment)
NS_DECL_CLASSINFO(Progress)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Progress, IProgress)
NS_DECL_CLASSINFO(CombinedProgress)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(CombinedProgress, IProgress)
NS_DECL_CLASSINFO(DVDDrive)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(DVDDrive, IDVDDrive)
NS_DECL_CLASSINFO(FloppyDrive)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(FloppyDrive, IFloppyDrive)
NS_DECL_CLASSINFO(SharedFolder)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(SharedFolder, ISharedFolder)
#ifdef VBOX_VRDP
NS_DECL_CLASSINFO(VRDPServer)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VRDPServer, IVRDPServer)
#endif
NS_DECL_CLASSINFO(DVDImage)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(DVDImage, IDVDImage)
NS_DECL_CLASSINFO(FloppyImage)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(FloppyImage, IFloppyImage)
NS_DECL_CLASSINFO(Host)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Host, IHost)
NS_DECL_CLASSINFO(HostDVDDrive)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(HostDVDDrive, IHostDVDDrive)
NS_DECL_CLASSINFO(HostFloppyDrive)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(HostFloppyDrive, IHostFloppyDrive)
NS_DECL_CLASSINFO(GuestOSType)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(GuestOSType, IGuestOSType)
NS_DECL_CLASSINFO(NetworkAdapter)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(NetworkAdapter, INetworkAdapter)
NS_DECL_CLASSINFO(SerialPort)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(SerialPort, ISerialPort)
NS_DECL_CLASSINFO(ParallelPort)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(ParallelPort, IParallelPort)
NS_DECL_CLASSINFO(USBController)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(USBController, IUSBController)
NS_DECL_CLASSINFO(SATAController)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(SATAController, ISATAController)
NS_DECL_CLASSINFO(USBDeviceFilter)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(USBDeviceFilter, IUSBDeviceFilter)
NS_DECL_CLASSINFO(HostUSBDevice)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(HostUSBDevice, IUSBDevice, IHostUSBDevice)
NS_DECL_CLASSINFO(HostUSBDeviceFilter)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(HostUSBDeviceFilter, IUSBDeviceFilter, IHostUSBDeviceFilter)
NS_DECL_CLASSINFO(AudioAdapter)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(AudioAdapter, IAudioAdapter)
NS_DECL_CLASSINFO(SystemProperties)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(SystemProperties, ISystemProperties)
NS_DECL_CLASSINFO(BIOSSettings)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(BIOSSettings, IBIOSSettings)

// collections and enumerators
COM_IMPL_READONLY_ENUM_AND_COLLECTION(Machine)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(Snapshot)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(HardDiskAttachment)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(GuestOSType)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(USBDeviceFilter)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(HostDVDDrive)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(HostFloppyDrive)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(HostUSBDevice)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(HostUSBDeviceFilter)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(HardDisk)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(DVDImage)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(FloppyImage)
COM_IMPL_READONLY_ENUM_AND_COLLECTION(SharedFolder)

COM_IMPL_READONLY_ENUM_AND_COLLECTION_AS(Progress, IProgress)
COM_IMPL_READONLY_ENUM_AND_COLLECTION_AS(IfaceUSBDevice, IUSBDevice)

////////////////////////////////////////////////////////////////////////////////

enum
{
    /* Delay before shutting down the VirtualBox server after the last
     * VirtualBox instance is released, in ms */
    VBoxSVC_ShutdownDelay = 5000,
};

static bool gAutoShutdown = false;

static nsIEventQueue* gEventQ = nsnull;
static PRBool volatile gKeepRunning = PR_TRUE;

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
    nsresult postTo (nsIEventQueue *aEventQ)
    {
        AssertReturn (mEv.that == NULL, NS_ERROR_FAILURE);
        AssertReturn (aEventQ, NS_ERROR_FAILURE);
        nsresult rv = aEventQ->InitEvent (&mEv.e, NULL,
                                          eventHandler, eventDestructor);
        if (NS_SUCCEEDED (rv))
        {
            mEv.that = this;
            rv = aEventQ->PostEvent (&mEv.e);
            if (NS_SUCCEEDED (rv))
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

    static void *PR_CALLBACK eventHandler (PLEvent *self)
    {
        return reinterpret_cast <Ev *> (self)->that->handler();
    }

    static void PR_CALLBACK eventDestructor (PLEvent *self)
    {
        delete reinterpret_cast <Ev *> (self)->that;
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
        LogFlowFunc (("Deleting VirtualBox...\n"));

        FinalRelease();
        sInstance = 0;

        LogFlowFunc (("VirtualBox object deleted.\n"));
        printf ("Informational: VirtualBox object deleted.\n");
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
                gEventQ->IsOnCurrentThread (&onMainThread);

            PRBool timerStarted = PR_FALSE;

            /* sTimes is null if this call originates from
             * FactoryDestructor() */
            if (sTimer != NULL)
            {
                LogFlowFunc (("Last VirtualBox instance was released.\n"));
                LogFlowFunc (("Scheduling server shutdown in %d ms...\n",
                              VBoxSVC_ShutdownDelay));

                /* make sure the previous timer (if any) is stopped;
                 * otherwise RTTimerStart() will definitely fail. */
                RTTimerStop (sTimer);

                int vrc = RTTimerStart (sTimer, uint64_t (VBoxSVC_ShutdownDelay) * 1000000);
                AssertRC (vrc);
                timerStarted = SUCCEEDED (vrc);
            }
            else
            {
                LogFlowFunc (("Last VirtualBox instance was released "
                              "on XPCOM shutdown.\n"));
                Assert (onMainThread);
            }

            if (!timerStarted)
            {
                if (!onMainThread)
                {
                    /* Failed to start the timer, post the shutdown event
                     * manually if not on the main thread alreay. */
                    ShutdownTimer (NULL, NULL);
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

                    Assert (gEventQ == NULL);
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
            LogFlowFunc (("\n"));

            Assert (RTCritSectIsInitialized (&sLock));

            /* stop accepting GetInstance() requests on other threads during
             * possible destruction */
            RTCritSectEnter (&sLock);

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
                    Assert (sInstance == NULL);
                    LogFlowFunc (("Terminating the server process...\n"));
                    /* make it leave the event loop */
                    gKeepRunning = PR_FALSE;
                }
            }
            else
            {
                /* This condition is quite rare: a new client will have to
                 * connect after this event has been posted to the main queue
                 * but before it started to process it. */
                LogFlowFunc (("Destruction is canceled (refcnt=%d).\n", count));
            }

            RTCritSectLeave (&sLock);

            return NULL;
        }
    };

    static void ShutdownTimer (PRTTIMER pTimer, void *pvUser)
    {
        NOREF (pTimer);
        NOREF (pvUser);

        /* A "too late" event is theoretically possible if somebody
         * manually ended the server after a destruction has been scheduled
         * and this method was so lucky that it got a chance to run before
         * the timer was killed. */
        AssertReturnVoid (gEventQ);

        /* post a quit event to the main queue */
        MaybeQuitEvent *ev = new MaybeQuitEvent();
        nsresult rv = ev->postTo (gEventQ);
        NOREF (rv);

        /* A failure above means we've been already stopped (for example
         * by Ctrl-C). FactoryDestructor() (NS_ShutdownXPCOM())
         * will do the job. Nothing to do. */
    }

    static NS_IMETHODIMP FactoryConstructor()
    {
        LogFlowFunc (("\n"));

        /* create a critsect to protect object construction */
        if (VBOX_FAILURE (RTCritSectInit (&sLock)))
            return NS_ERROR_OUT_OF_MEMORY;

        int vrc = RTTimerCreateEx (&sTimer, 0, 0, ShutdownTimer, NULL);
        if (VBOX_FAILURE (vrc))
        {
            LogFlowFunc (("Failed to create a timer! (vrc=%Vrc)\n", vrc));
            return NS_ERROR_FAILURE;
        }

        return NS_OK;
    }

    static NS_IMETHODIMP FactoryDestructor()
    {
        LogFlowFunc (("\n"));

        RTTimerDestroy (sTimer);
        sTimer = NULL;

        RTCritSectDelete (&sLock);

        if (sInstance)
        {
            /* Either posting a destruction event falied for some reason (most
             * likely, the quit event has been received before the last release),
             * or the client has terminated abnormally w/o releasing its
             * VirtualBox instance (so NS_ShutdownXPCOM() is doing a cleanup).
             * Release the guard reference we added in GetInstance(). */
            sInstance->Release();
        }

        return NS_OK;
    }

    static nsresult GetInstance (VirtualBox **inst)
    {
        LogFlowFunc (("Getting VirtualBox object...\n"));

        RTCritSectEnter (&sLock);

        int rv = NS_OK;

        if (sInstance == 0)
        {
            LogFlowFunc (("Creating new VirtualBox object...\n"));
            sInstance = new VirtualBoxClassFactory();
            if (sInstance)
            {
                /* make an extra AddRef to take the full control
                 * on the VirtualBox destruction (see FinalRelease()) */
                sInstance->AddRef();

                sInstance->AddRef(); /* protect FinalConstruct() */
                rv = sInstance->FinalConstruct();
                printf ("Informational: VirtualBox object created (rc=%08X).\n", rv);
                if (NS_FAILED (rv))
                {
                    /* On failure diring VirtualBox initialization, delete it
                     * immediately on the current thread by releasing all
                     * references in order to properly schedule the server
                     * shutdown. Since the object is fully deleted here, there
                     * is a chance to fix the error and request a new
                     * instantiation before the server terminates. However,
                     * the main reason to maintain the shoutdown delay on
                     * failure is to let the front-end completely fetch error
                     * info from a server-side IVirtualBoxErrorInfo object. */
                    sInstance->Release();
                    sInstance->Release();
                    Assert (sInstance == 0);
                }
                else
                {
                    /* On success, make sure the previous timer is stopped to
                     * cancel a scheduled server termination (if any). */
                    RTTimerStop (sTimer);
                }
            }
            else
            {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
        }
        else
        {
            LogFlowFunc (("Using existing VirtualBox object...\n"));
            nsrefcnt count = sInstance->AddRef();
            Assert (count > 1);

            if (count == 2)
            {
                LogFlowFunc (("Another client has requested a reference to VirtualBox, "
                              "canceling detruction...\n"));

                /* make sure the previous timer is stopped */
                RTTimerStop (sTimer);
            }
        }

        *inst = sInstance;

        RTCritSectLeave (&sLock);

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

    static PRTTIMER sTimer;
};

VirtualBoxClassFactory *VirtualBoxClassFactory::sInstance = 0;
RTCRITSECT VirtualBoxClassFactory::sLock = {0};

PRTTIMER VirtualBoxClassFactory::sTimer = NULL;

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR_WITH_RC
    (VirtualBox, VirtualBoxClassFactory::GetInstance)

////////////////////////////////////////////////////////////////////////////////

typedef NSFactoryDestructorProcPtr NSFactoryConsructorProcPtr;

/**
 *  Enhanced module component information structure.
 *
 *  nsModuleComponentInfo lacks the factory construction callback, here we add
 *  it. This callback is called by NS_NewGenericFactoryEx() after a
 *  nsGenericFactory instance is successfully created.
 */
struct nsModuleComponentInfoEx : nsModuleComponentInfo
{
    nsModuleComponentInfoEx () {}
    nsModuleComponentInfoEx (int) {}

    nsModuleComponentInfoEx (
        const char*                                 aDescription,
        const nsCID&                                aCID,
        const char*                                 aContractID,
        NSConstructorProcPtr                        aConstructor,
        NSRegisterSelfProcPtr                       aRegisterSelfProc,
        NSUnregisterSelfProcPtr                     aUnregisterSelfProc,
        NSFactoryDestructorProcPtr                  aFactoryDestructor,
        NSGetInterfacesProcPtr                      aGetInterfacesProc,
        NSGetLanguageHelperProcPtr                  aGetLanguageHelperProc,
        nsIClassInfo **                             aClassInfoGlobal,
        PRUint32                                    aFlags,
        NSFactoryConsructorProcPtr                  aFactoryConstructor)
    {
        mDescription = aDescription;
        mCID = aCID;
        mContractID = aContractID;
        mConstructor = aConstructor;
        mRegisterSelfProc = aRegisterSelfProc;
        mUnregisterSelfProc = aUnregisterSelfProc;
        mFactoryDestructor = aFactoryDestructor;
        mGetInterfacesProc = aGetInterfacesProc;
        mGetLanguageHelperProc = aGetLanguageHelperProc;
        mClassInfoGlobal = aClassInfoGlobal;
        mFlags = aFlags;
        mFactoryConstructor = aFactoryConstructor;
    }

    /** (optional) Factory Construction Callback */
    NSFactoryConsructorProcPtr mFactoryConstructor;
};

////////////////////////////////////////////////////////////////////////////////

static const nsModuleComponentInfoEx components[] =
{
    nsModuleComponentInfoEx (
        "VirtualBox component",
        (nsCID) NS_VIRTUALBOX_CID,
        NS_VIRTUALBOX_CONTRACTID,
        VirtualBoxConstructor, // constructor funcion
        NULL, // registration function
        NULL, // deregistration function
        VirtualBoxClassFactory::FactoryDestructor, // factory destructor function
        NS_CI_INTERFACE_GETTER_NAME(VirtualBox),
        NULL, // language helper
        &NS_CLASSINFO_NAME(VirtualBox),
        0, // flags
        VirtualBoxClassFactory::FactoryConstructor // factory constructor function
    )
};

/////////////////////////////////////////////////////////////////////////////

/**
 *  Extends NS_NewGenericFactory() by immediately calling
 *  nsModuleComponentInfoEx::mFactoryConstructor before returning to the
 *  caller.
 */
nsresult
NS_NewGenericFactoryEx (nsIGenericFactory **result,
                        const nsModuleComponentInfoEx *info)
{
    AssertReturn (result, NS_ERROR_INVALID_POINTER);

    nsresult rv = NS_NewGenericFactory (result, info);
    if (NS_SUCCEEDED (rv) && info && info->mFactoryConstructor)
    {
        rv = info->mFactoryConstructor();
        if (NS_FAILED (rv))
            NS_RELEASE (*result);
    }

    return rv;
}

/////////////////////////////////////////////////////////////////////////////

/**
 * Hhelper function to register self components upon start-up
 * of the out-of-proc server.
 */
static nsresult
RegisterSelfComponents (nsIComponentRegistrar *registrar,
                        const nsModuleComponentInfoEx *components,
                        PRUint32 count)
{
    nsresult rc = NS_OK;
    const nsModuleComponentInfoEx *info = components;
    for (PRUint32 i = 0; i < count && NS_SUCCEEDED (rc); i++, info++)
    {
        /* skip components w/o a constructor */
        if (!info->mConstructor) continue;
        /* create a new generic factory for a component and register it */
        nsIGenericFactory *factory;
        rc = NS_NewGenericFactoryEx (&factory, info);
        if (NS_SUCCEEDED (rc))
        {
            rc = registrar->RegisterFactory (info->mCID,
                                             info->mDescription,
                                             info->mContractID,
                                             factory);
            factory->Release();
        }
    }
    return rc;
}

/////////////////////////////////////////////////////////////////////////////

static ipcIService *gIpcServ = nsnull;
static char *pszPidFile = NULL;

class ForceQuitEvent : public MyEvent
{
    void *handler()
    {
        LogFlowFunc (("\n"));

        gKeepRunning = PR_FALSE;

        if (pszPidFile)
            RTFileDelete(pszPidFile);

        return NULL;
    }
};

static void signal_handler (int sig)
{
    if (gEventQ && gKeepRunning)
    {
        /* post a quit event to the queue */
        ForceQuitEvent *ev = new ForceQuitEvent();
        ev->postTo (gEventQ);
    }
}

#if defined(USE_BACKTRACE)
/**
 * the signal handler that prints out a backtrace of the call stack.
 * the code is taken from http://www.linuxjournal.com/article/6391.
 */
static void bt_sighandler (int sig, siginfo_t *info, void *secret)
{

    void *trace[16];
    char **messages = (char **)NULL;
    int i, trace_size = 0;
    ucontext_t *uc = (ucontext_t *)secret;

    // Do something useful with siginfo_t
    if (sig == SIGSEGV)
        Log (("Got signal %d, faulty address is %p, from %p\n",
               sig, info->si_addr, uc->uc_mcontext.gregs[REG_PC]));
    else
        Log (("Got signal %d\n", sig));

    trace_size = backtrace (trace, 16);
    // overwrite sigaction with caller's address
    trace[1] = (void *) uc->uc_mcontext.gregs [REG_PC];

    messages = backtrace_symbols (trace, trace_size);
    // skip first stack frame (points here)
    Log (("[bt] Execution path:\n"));
    for (i = 1; i < trace_size; ++i)
        Log (("[bt] %s\n", messages[i]));

    exit (0);
}
#endif

int main (int argc, char **argv)
{
    const struct option options[] =
    {
        { "automate",       no_argument,        NULL, 'a' },
#ifdef RT_OS_DARWIN
        { "auto-shutdown",  no_argument,        NULL, 'A' },
#endif
        { "daemonize",      no_argument,        NULL, 'd' },
        { "pidfile",        required_argument,  NULL, 'p' },
#ifdef RT_OS_DARWIN
        { "pipe",           required_argument,  NULL, 'P' },
#endif
        { NULL,             0,                  NULL,  0  }
    };
    int c;

    bool fDaemonize = false;
#ifndef RT_OS_OS2
    static int daemon_pipe_fds[2] = {-1, -1};
#endif

    for (;;)
    {
        c = getopt_long(argc, argv, "", options, NULL);
        if (c == -1)
            break;
        switch (c)
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

#ifdef RT_OS_DARWIN
            /* Used together with '-P', see below. Internal use only. */
            case 'A':
            {
                gAutoShutdown = true;
                break;
            }
#endif

            case 'd':
            {
                fDaemonize = true;
                break;
            }

            case 'p':
            {
                pszPidFile = optarg;
                break;
            }

#ifdef RT_OS_DARWIN
            /* we need to exec on darwin, this is just an internal
             * hack for passing the pipe fd along to the final child. */
            case 'P':
            {
                daemon_pipe_fds[1] = atoi(optarg);
                break;
            }
#endif

            default:
            {
                /* exit on invalid options */
                return 1;
            }
        }
    }

    static RTFILE pidFile = NIL_RTFILE;

#ifdef RT_OS_OS2

    /* nothing to do here, the process is supposed to be already
     * started daemonized when it is necessary */
    NOREF(fDaemonize);

#else // ifdef RT_OS_OS2

    if (fDaemonize)
    {
        /* create a pipe for communication between child and parent */
        if (pipe(daemon_pipe_fds) < 0)
        {
            printf("ERROR: pipe() failed (errno = %d)\n", errno);
            return 1;
        }

        pid_t childpid = fork();
        if (childpid == -1)
        {
            printf("ERROR: fork() failed (errno = %d)\n", errno);
            return 1;
        }

        if (childpid != 0)
        {
            /* we're the parent process */
            bool fSuccess = false;

            /* close the writing end of the pipe */
            close(daemon_pipe_fds[1]);

            /* try to read a message from the pipe */
            char msg[10] = {0}; /* initialize so it's NULL terminated */
            if (read(daemon_pipe_fds[0], msg, sizeof(msg)) > 0)
            {
                if (strcmp(msg, "READY") == 0)
                    fSuccess = true;
                else
                    printf ("ERROR: Unknown message from child "
                            "process (%s)\n", msg);
            }
            else
                printf ("ERROR: 0 bytes read from child process\n");

            /* close the reading end of the pipe as well and exit */
            close(daemon_pipe_fds[0]);
            return fSuccess ? 0 : 1;
        }
        /* we're the child process */

        /* Create a new SID for the child process */
        pid_t sid = setsid();
        if (sid < 0)
        {
            printf("ERROR: setsid() failed (errno = %d)\n", errno);
            return 1;
        }

        /* Need to do another for to get rid of the session leader status.
         * Otherwise any accidentally opened tty will automatically become a
         * controlling tty for the daemon process. */
        childpid = fork();
        if (childpid == -1)
        {
            printf("ERROR: second fork() failed (errno = %d)\n", errno);
            return 1;
        }

        if (childpid != 0)
        {
            /* we're the parent process, just a dummy so terminate now */
            exit(0);
        }

        /* Redirect standard i/o streams to /dev/null */
        if (daemon_pipe_fds[0] > 2)
        {
            freopen ("/dev/null", "r", stdin);
            freopen ("/dev/null", "w", stdout);
            freopen ("/dev/null", "w", stderr);
        }

        /* close the reading end of the pipe */
        close(daemon_pipe_fds[0]);

# ifdef RT_OS_DARWIN
        /*
         * On leopard we're no longer allowed to use some of the core API's
         * after forking - this will cause us to hit an int3.
         * So, we'll have to execv VBoxSVC once again and hand it the pipe
         * and all other relevant options.
         */
        const char *apszArgs[7];
        unsigned i = 0;
        apszArgs[i++] = argv[0];
        apszArgs[i++] = "--pipe";
        char szPipeArg[32];
        RTStrPrintf (szPipeArg, sizeof (szPipeArg), "%d", daemon_pipe_fds[1]);
        apszArgs[i++] = szPipeArg;
        if (pszPidFile)
        {
            apszArgs[i++] = "--pidfile";
            apszArgs[i++] = pszPidFile;
        }
        if (gAutoShutdown)
            apszArgs[i++] = "--auto-shutdown";
        apszArgs[i++] = NULL; Assert(i <= RT_ELEMENTS(apszArgs));
        execv (apszArgs[0], (char * const *)apszArgs);
        exit (0);
# endif
    }

#endif // ifdef RT_OS_OS2

#if defined(USE_BACKTRACE)
    {
        /* install our signal handler to backtrace the call stack */
        struct sigaction sa;
        sa.sa_sigaction = bt_sighandler;
        sigemptyset (&sa.sa_mask);
        sa.sa_flags = SA_RESTART | SA_SIGINFO;
        sigaction (SIGSEGV, &sa, NULL);
        sigaction (SIGBUS, &sa, NULL);
        sigaction (SIGUSR1, &sa, NULL);
    }
#endif

    /*
     * Initialize the VBox runtime without loading
     * the support driver
     */
    RTR3Init(false);

    nsresult rc;

    do
    {
        rc = com::Initialize();
        if (NS_FAILED (rc))
        {
            printf ("ERROR: Failed to initialize XPCOM! (rc=%08X)\n", rc);
            break;
        }

        nsCOMPtr <nsIComponentRegistrar> registrar;
        rc = NS_GetComponentRegistrar (getter_AddRefs (registrar));
        if (NS_FAILED (rc))
        {
            printf ("ERROR: Failed to get component registrar! (rc=%08X)\n", rc);
            break;
        }

        registrar->AutoRegister (nsnull);
        rc = RegisterSelfComponents (registrar, components,
                                     NS_ARRAY_LENGTH (components));
        if (NS_FAILED (rc))
        {
            printf ("ERROR: Failed to register server components! (rc=%08X)\n", rc);
            break;
        }

        /* get the main thread's event queue (afaik, the dconnect service always
         * gets created upon XPCOM startup, so it will use the main (this)
         * thread's event queue to receive IPC events) */
        rc = NS_GetMainEventQ (&gEventQ);
        if (NS_FAILED (rc))
        {
            printf ("ERROR: Failed to get the main event queue! (rc=%08X)\n", rc);
            break;
        }

        nsCOMPtr<ipcIService> ipcServ (do_GetService(IPC_SERVICE_CONTRACTID, &rc));
        if (NS_FAILED (rc))
        {
            printf ("ERROR: Failed to get IPC service! (rc=%08X)\n", rc);
            break;
        }

        NS_ADDREF (gIpcServ = ipcServ);

        LogFlowFunc (("Will use \"%s\" as server name.\n", VBOXSVC_IPC_NAME));

        rc = gIpcServ->AddName (VBOXSVC_IPC_NAME);
        if (NS_FAILED (rc))
        {
            printf ("ERROR: Failed to register VirtualBoxServer! (rc=%08X)\n", rc);
            NS_RELEASE (gIpcServ);
            break;
        }

        {
            /* setup signal handling to convert some signals to a quit event */
            struct sigaction sa;
            sa.sa_handler = signal_handler;
            sigemptyset (&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction (SIGINT, &sa, NULL);
            sigaction (SIGQUIT, &sa, NULL);
            sigaction (SIGTERM, &sa, NULL);
            sigaction (SIGTRAP, &sa, NULL);
        }

        {
            char szBuf[80];
            int  iSize;

            iSize = snprintf (szBuf, sizeof(szBuf),
                              "innotek VirtualBox XPCOM Server Version "
                              VBOX_VERSION_STRING);
            for (int i=iSize; i>0; i--)
                putchar('*');
            printf ("\n%s\n", szBuf);
            printf ("(C) 2004-2008 innotek GmbH\n"
                    "All rights reserved.\n");
#ifdef DEBUG
            printf ("Debug version.\n");
#endif
#if 0
            /* in my opinion two lines enclosing the text look better */
            for (int i=iSize; i>0; i--)
                putchar('*');
            putchar('\n');
#endif
        }

#ifndef RT_OS_OS2
        if (daemon_pipe_fds[1] >= 0)
        {
            printf ("\nStarting event loop....\n[send TERM signal to quit]\n");
            /* now we're ready, signal the parent process */
            write(daemon_pipe_fds[1], "READY", strlen("READY"));
        }
        else
#endif
        {
            printf ("\nStarting event loop....\n[press Ctrl-C to quit]\n");
        }

        if (pszPidFile)
        {
            char szBuf[32];
            const char *lf = "\n";
            RTFileOpen(&pidFile, pszPidFile, RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE);
            RTStrFormatNumber(szBuf, getpid(), 10, 0, 0, 0);
            RTFileWrite(pidFile, szBuf, strlen(szBuf), NULL);
            RTFileWrite(pidFile, lf, strlen(lf), NULL);
            RTFileClose(pidFile);
        }

        PLEvent *ev;
        while (gKeepRunning)
        {
            gEventQ->WaitForEvent (&ev);
            gEventQ->HandleEvent (ev);
        }

        gIpcServ->RemoveName (VBOXSVC_IPC_NAME);

        /* stop accepting new events */
        gEventQ->StopAcceptingEvents();

        /* process any remaining events */
        gEventQ->ProcessPendingEvents();

        printf ("Terminated event loop.\n");
    }
    while (0); // this scopes the nsCOMPtrs

    NS_IF_RELEASE (gIpcServ);
    NS_IF_RELEASE (gEventQ);

    /* no nsCOMPtrs are allowed to be alive when you call com::Shutdown(). */

    LogFlowFunc (("Calling com::Shutdown()...\n"));
    rc = com::Shutdown();
    LogFlowFunc (("Finished com::Shutdown() (rc=%08X)\n", rc));

    if (NS_FAILED (rc))
        printf ("ERROR: Failed to shutdown XPCOM! (rc=%08X)\n", rc);

    printf ("XPCOM server has shutdown.\n");

    if (pszPidFile)
    {
        RTFileDelete(pszPidFile);
    }

#ifndef RT_OS_OS2
    if (daemon_pipe_fds[1] >= 0)
    {
        /* close writing end of the pipe as well */
        close(daemon_pipe_fds[1]);
    }
#endif

    return 0;
}
