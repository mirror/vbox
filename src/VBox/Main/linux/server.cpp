/** @file
 *
 * XPCOM server process start point
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include <ipcIService.h>
#include <ipcCID.h>

#include <nsIServiceManager.h>
#include <nsIComponentRegistrar.h>

#include <nsXPCOMGlue.h>
#include <nsEventQueueUtils.h>

// for NS_InitXPCOM2 with bin dir parameter
#include <nsEmbedString.h>
#include <nsIFile.h>
#include <nsILocalFile.h>

#include "linux/server.h"
#include "Logging.h"

#include <iprt/runtime.h>
#include <iprt/path.h>
#include <iprt/critsect.h>
#include <iprt/timer.h>

#include <VBox/param.h>
#include <VBox/version.h>

// for nsMyFactory
#include "nsIGenericFactory.h"
#include "nsIClassInfo.h"

#include <stdio.h>

// for the signal handler
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

// for the backtrace signal handler
#if defined(DEBUG) && defined(__LINUX__)
# define USE_BACKTRACE
#endif
#if defined(USE_BACKTRACE)
# include <execinfo.h>
// get REG_EIP/RIP from ucontext.h
# ifndef __USE_GNU
#  define __USE_GNU
# endif
# include <ucontext.h>
# ifdef __AMD64__
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
#include <USBControllerImpl.h>
#include <USBDeviceImpl.h>
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
NS_DECL_CLASSINFO(USBController)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(USBController, IUSBController)
NS_DECL_CLASSINFO(USBDeviceFilter)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(USBDeviceFilter, IUSBDeviceFilter)
NS_DECL_CLASSINFO(HostUSBDevice)
NS_IMPL_THREADSAFE_ISUPPORTS2_CI(HostUSBDevice, IUSBDevice, IHostUSBDevice)
NS_DECL_CLASSINFO(HostUSBDeviceFilter)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(HostUSBDeviceFilter, IHostUSBDeviceFilter)
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

        /* Instruct the main event loop to terminate. Note that it's enough
         * to set gKeepRunning to false because we are on the main thread
         * already (i.e. no need to post events there). */
        if (gAutoShutdown)
            gKeepRunning = PR_FALSE;
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
                LogFlowFunc (("Last VirtualBox instance was released, "
                              "scheduling server shutdown in %d ms...\n",
                              VBoxSVC_ShutdownDelay));

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

    /* Returns the current value of the reference counter. */
    nsrefcnt GetRefCount()
    {
        /* we don't use our own Release() to avoid its "side effect" */
        nsrefcnt count = VirtualBox::AddRef();
        count = VirtualBox::Release();
        return count;
    }

    /* called on the main thread */
    static void *PR_CALLBACK DestructEventHandler (PLEvent* self)
    {
        Assert (RTCritSectIsInitialized (&sLock));

        /* stop accepting GetInstance() requests during possible destruction */
        RTCritSectEnter (&sLock);

        Assert (sInstance);

        nsrefcnt count = sInstance->GetRefCount();
        AssertMsg (count >= 1, ("count=%d\n", count));

        if (count > 1)
        {
            /* This case is very unlikely because we stop the timer when a new
             * client connects after the instance was scheduled for
             * destruction, but it's still possible. This is the only reason
             * for the above GetRefCount() btw.  */
            LogFlowFunc (("Destruction is canceled (refcnt=%d).\n", count));
        }
        else
        {
            /* release the last (first) reference we added in GetInstance()
             * (this must call the destructor) */
            nsrefcnt count = sInstance->Release();
            AssertMsg (count == 0, ("count=%d\n", count));
            NOREF(count);
        }

        RTCritSectLeave (&sLock);

        return 0;
    }

    static void PR_CALLBACK DestructEventDestructor (PLEvent* self)
    {
        delete self;
    }

    static void ShutdownTimer (PRTTIMER pTimer, void *pvUser)
    {
        NOREF (pTimer);
        NOREF (pvUser);

        /* A "too late" event is theoretically possible if somebody
         * manually ended the server after a destruction has been scheduled
         * and this method was so lucky that it got a chance to run before
         * the timer was killed. */
        AssertReturnVoid (gEventQ);

        /* post a destruction event to the main thread to safely release the
         * extra reference added in VirtualBoxClassFactory::GetInstance() */

        LogFlowFunc (("Posting VirtualBox destruction & shtutdown event...\n"));

        PLEvent *ev = new PLEvent;
        gEventQ->InitEvent (ev, NULL, DestructEventHandler,
                            DestructEventDestructor);
        nsresult rv = gEventQ->PostEvent (ev);
        if (NS_FAILED (rv))
        {
            /* this means we've been already stopped (for example
             * by Ctrl-C). FactoryDestructor() (NS_ShutdownXPCOM())
             * will do the job. */
            PL_DestroyEvent (ev);
        }
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
             * Release the extra reference we added in GetInstance(). */
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
                    /* on failure diring VirtualBox initialization, delete it
                     * immediately on the current thread, ignoring the reference
                     * count (VirtualBox should be aware of that meaning that it
                     * has already completely unintialized itself in this
                     * case) */
                    LogFlowFunc (("VirtualBox creation failed "
                                  "(rc=%08X), deleting immediately...\n", rv));
                    delete sInstance;
                    sInstance = 0;
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
                LogFlowFunc (("Another client has requested "
                              "a reference of VirtualBox scheduled for destruction, "
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
 *  nsModuleComponentInfo lacks the factory construction callback,
 *  here we add it. This callback is called by NS_NewMyFactory() after
 *  a nsMyFactory instance is successfully created.
 */
struct nsMyModuleComponentInfo : nsModuleComponentInfo
{
    nsMyModuleComponentInfo () {}
    nsMyModuleComponentInfo (int) {}

    nsMyModuleComponentInfo (
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

static const nsMyModuleComponentInfo components[] =
{
    nsMyModuleComponentInfo (
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
 *  Generic component factory.
 *
 *  The code below is stolen from nsGenericFactory.h / nsGenericFactory.cpp,
 *  because we get a segmentation fault for some unknown reason when VBoxSVC
 *  starts up (somewhere during the initialization of the libipcdc.so module)
 *  when we just reference XPCOM's NS_NewGenericFactory() from here (i.e. even
 *  before actually calling it) and run VBoxSVC using the debug XPCOM
 *  libraries.
 *
 *  Actually, I know why, but I find it too stupid even to discuss.
 */
class nsMyFactory : public nsIGenericFactory, public nsIClassInfo {
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_GENERICFACTORY_CID);

    nsMyFactory(const nsModuleComponentInfo *info = NULL);

    NS_DECL_ISUPPORTS
    NS_DECL_NSICLASSINFO

    /* nsIGenericFactory methods */
    NS_IMETHOD SetComponentInfo(const nsModuleComponentInfo *info);
    NS_IMETHOD GetComponentInfo(const nsModuleComponentInfo **infop);

    NS_IMETHOD CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

    static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
private:
    ~nsMyFactory();

    const nsModuleComponentInfo *mInfo;
};

nsMyFactory::nsMyFactory(const nsModuleComponentInfo *info)
    : mInfo(info)
{
    if (mInfo && mInfo->mClassInfoGlobal)
        *mInfo->mClassInfoGlobal = NS_STATIC_CAST(nsIClassInfo *, this);
}

nsMyFactory::~nsMyFactory()
{
    if (mInfo) {
        if (mInfo->mFactoryDestructor)
            mInfo->mFactoryDestructor();
        if (mInfo->mClassInfoGlobal)
            *mInfo->mClassInfoGlobal = 0;
    }
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsMyFactory,
                              nsIGenericFactory,
                              nsIFactory,
                              nsIClassInfo)

NS_IMETHODIMP nsMyFactory::CreateInstance(nsISupports *aOuter,
                                               REFNSIID aIID, void **aResult)
{
    if (mInfo->mConstructor)
        return mInfo->mConstructor(aOuter, aIID, aResult);

    return NS_ERROR_FACTORY_NOT_REGISTERED;
}

NS_IMETHODIMP nsMyFactory::LockFactory(PRBool aLock)
{
    // XXX do we care if (mInfo->mFlags & THREADSAFE)?
    return NS_OK;
}

NS_IMETHODIMP nsMyFactory::GetInterfaces(PRUint32 *countp,
                                              nsIID* **array)
{
    if (!mInfo->mGetInterfacesProc) {
        *countp = 0;
        *array = nsnull;
        return NS_OK;
    }
    return mInfo->mGetInterfacesProc(countp, array);
}

NS_IMETHODIMP nsMyFactory::GetHelperForLanguage(PRUint32 language,
                                                     nsISupports **helper)
{
    if (mInfo->mGetLanguageHelperProc)
        return mInfo->mGetLanguageHelperProc(language, helper);
    *helper = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsMyFactory::GetContractID(char **aContractID)
{
    if (mInfo->mContractID) {
        *aContractID = (char *)nsMemory::Alloc(strlen(mInfo->mContractID) + 1);
        if (!*aContractID)
            return NS_ERROR_OUT_OF_MEMORY;
        strcpy(*aContractID, mInfo->mContractID);
    } else {
        *aContractID = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP nsMyFactory::GetClassDescription(char * *aClassDescription)
{
    if (mInfo->mDescription) {
        *aClassDescription = (char *)
            nsMemory::Alloc(strlen(mInfo->mDescription) + 1);
        if (!*aClassDescription)
            return NS_ERROR_OUT_OF_MEMORY;
        strcpy(*aClassDescription, mInfo->mDescription);
    } else {
        *aClassDescription = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP nsMyFactory::GetClassID(nsCID * *aClassID)
{
    *aClassID =
        NS_REINTERPRET_CAST(nsCID*,
                            nsMemory::Clone(&mInfo->mCID, sizeof mInfo->mCID));
    if (! *aClassID)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP nsMyFactory::GetClassIDNoAlloc(nsCID *aClassID)
{
    *aClassID = mInfo->mCID;
    return NS_OK;
}

NS_IMETHODIMP nsMyFactory::GetImplementationLanguage(PRUint32 *langp)
{
    *langp = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

NS_IMETHODIMP nsMyFactory::GetFlags(PRUint32 *flagsp)
{
    *flagsp = mInfo->mFlags;
    return NS_OK;
}

// nsIGenericFactory: component-info accessors
NS_IMETHODIMP nsMyFactory::SetComponentInfo(const nsModuleComponentInfo *info)
{
    if (mInfo && mInfo->mClassInfoGlobal)
        *mInfo->mClassInfoGlobal = 0;
    mInfo = info;
    if (mInfo && mInfo->mClassInfoGlobal)
        *mInfo->mClassInfoGlobal = NS_STATIC_CAST(nsIClassInfo *, this);
    return NS_OK;
}

NS_IMETHODIMP nsMyFactory::GetComponentInfo(const nsModuleComponentInfo **infop)
{
    *infop = mInfo;
    return NS_OK;
}

NS_METHOD nsMyFactory::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    // sorry, aggregation not spoken here.
    nsresult res = NS_ERROR_NO_AGGREGATION;
    if (outer == NULL) {
        nsMyFactory* factory = new nsMyFactory;
        if (factory != NULL) {
            res = factory->QueryInterface(aIID, aInstancePtr);
            if (res != NS_OK)
                delete factory;
        } else {
            res = NS_ERROR_OUT_OF_MEMORY;
        }
    }
    return res;
}

/**
 *  Instantiates a new factory and calls
 *  nsMyModuleComponentInfo::mFactoryConstructor.
 */
NS_COM nsresult
NS_NewMyFactory(nsIGenericFactory* *result,
                const nsMyModuleComponentInfo *info)
{
    nsresult rv;
    nsMyFactory* fact;
    rv = nsMyFactory::Create(NULL, NS_GET_IID(nsIGenericFactory), (void**)&fact);
    if (NS_FAILED(rv)) return rv;
    rv = fact->SetComponentInfo(info);
    if (NS_FAILED(rv)) goto error;
    if (info && info->mFactoryConstructor) {
        rv = info->mFactoryConstructor();
        if (NS_FAILED(rv)) goto error;
    }
    *result = fact;
    return rv;

  error:
    NS_RELEASE(fact);
    return rv;
}

/////////////////////////////////////////////////////////////////////////////

/**
 * Hhelper function to register self components upon start-up
 * of the out-of-proc server.
 */
static nsresult
RegisterSelfComponents (nsIComponentRegistrar *registrar,
                        const nsMyModuleComponentInfo *components,
                        PRUint32 count)
{
    nsresult rc = NS_OK;
    const nsMyModuleComponentInfo *info = components;
    for (PRUint32 i = 0; i < count && NS_SUCCEEDED (rc); i++, info++)
    {
        /* skip components w/o a constructor */
        if (!info->mConstructor) continue;
        /* create a new generic factory for a component and register it */
        nsIGenericFactory *factory;
        rc = NS_NewGenericFactory (&factory, info);
        rc = NS_NewMyFactory (&factory, info);
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

void* PR_CALLBACK quitEventHandler (PLEvent* self) { gKeepRunning = PR_FALSE; return 0; }
void PR_CALLBACK quitEventDestructor (PLEvent* self) { delete self; }

static void signal_handler (int sig)
{
    if (gEventQ && gKeepRunning)
    {
        /* post a quit event to the queue */
        PLEvent *ev = new PLEvent;
        gEventQ->InitEvent (ev, NULL, quitEventHandler, quitEventDestructor);
        gEventQ->PostEvent (ev);
    }
    if (pszPidFile)
    {
        RTFileDelete(pszPidFile);
    }
};

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
        { "automate",  no_argument,       NULL, 'a' },
        { "daemonize", no_argument,       NULL, 'd' },
        { "pidfile",   required_argument, NULL, 'p' },
        { NULL,        0,                 NULL,  0  }
    };
    int c;

    bool fDaemonize = false;

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

            default:
            {
                /* exit on invalid options */
                return 1;
            }
        }
    }

    static int daemon_pipe_fds[2];
    static RTFILE pidFile = NIL_RTFILE;

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

        /* Redirect standard i/o streams to /dev/null */
        freopen ("/dev/null", "r", stdin);
        freopen ("/dev/null", "w", stdout);
        freopen ("/dev/null", "w", stderr);

        /* close the reading end of the pipe */
        close(daemon_pipe_fds[0]);
    }

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
        XPCOMGlueStartup (nsnull);

        char path [RTPATH_MAX];
        path [0] = '\0';

        nsCOMPtr<nsIFile> nsAppPath;
        {
            /* get the path to the executable */
            char *appPath = NULL;
#if defined (DEBUG)
            appPath = getenv ("VIRTUALBOX_APP_HOME");
            if (appPath)
                RTPathReal (appPath, path, RTPATH_MAX);
            else
#endif
                RTPathProgram (path, RTPATH_MAX);
            appPath = path;

            nsCOMPtr<nsILocalFile> file;
            rc = NS_NewNativeLocalFile (nsEmbedCString (appPath),
                                        PR_FALSE, getter_AddRefs (file));
            if (NS_SUCCEEDED (rc))
                nsAppPath = do_QueryInterface (file, &rc);
        }
        if (NS_FAILED (rc))
        {
            printf ("ERROR: Failed to create file object! (rc=%08X)\n", rc);
            break;
        }

        /* not really necessary at the moment */
#if 0
        if (!RTProcGetExecutableName (path, sizeof (path)))
        {
            printf ("ERROR: Failed to get executable name!\n");
            break;
        }
#endif

        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2 (getter_AddRefs (servMan), nsAppPath, nsnull);
        if (!servMan)
        {
            printf ("ERROR: Failed to get service manager!\n");
            break;
        }

        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface (servMan);
        if (!registrar)
        {
            printf ("ERROR: Failed to get component registrar!\n");
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
                              "InnoTek VirtualBox XPCOM Server Version %s",
                              VBOX_VERSION_STRING);
            for (int i=iSize; i>0; i--)
                putchar('*');
            printf ("\n%s\n", szBuf);
            printf ("(C) 2004-2007 InnoTek Systemberatung GmbH\n");
            printf ("All rights reserved.\n");
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

        if (fDaemonize)
        {
            printf ("\nStarting event loop....\n[send TERM signal to quit]\n");
            /* now we're ready, signal the parent process */
            write(daemon_pipe_fds[1], "READY", strlen("READY"));
        }
        else
        {
            printf ("\nStarting event loop....\n[press Ctrl-C to quit]\n");
        }

        if (pszPidFile)
        {
            char szBuf[32];
            char *lf = "\n";
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

        gIpcServ->RemoveName (path);

        // stop accepting new events
        gEventQ->StopAcceptingEvents();
        // process any remaining events
        gEventQ->ProcessPendingEvents();

        printf ("Terminated event loop.\n");

    }
    while (0); // this scopes the nsCOMPtrs

    NS_IF_RELEASE (gIpcServ);
    NS_IF_RELEASE (gEventQ);

    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    LogFlowFunc (("Calling NS_ShutdownXPCOM()...\n"));
    rc = NS_ShutdownXPCOM (nsnull);
    LogFlowFunc (("Finished NS_ShutdownXPCOM() (rc=%08X)\n", rc));

    if (NS_FAILED (rc))
        printf ("ERROR: Failed to shutdown XPCOM! (rc=%08X)\n", rc);

    XPCOMGlueShutdown();

    printf ("XPCOM server has shutdown.\n");

    if (pszPidFile)
    {
        RTFileDelete(pszPidFile);
    }

    if (fDaemonize)
    {
        /* close writing end of the pipe as well */
        close(daemon_pipe_fds[1]);
    }

    return 0;
}
