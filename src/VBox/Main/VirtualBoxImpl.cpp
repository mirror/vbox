/* $Id$ */

/** @file
 * Implementation of IVirtualBox in VBoxSVC.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <iprt/buildconfig.h>
#include <iprt/cpputils.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>
#include <iprt/xml_cpp.h>

#include <VBox/com/com.h>
#include <VBox/com/array.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/VBoxHDD.h>
#include <VBox/settings.h>
#include <VBox/version.h>

#include <package-generated.h>

#include <algorithm>
#include <set>
#include <memory> // for auto_ptr

#include <typeinfo>

#include "VirtualBoxImpl.h"

#include "Global.h"
#include "MachineImpl.h"
#include "HardDiskImpl.h"
#include "MediumImpl.h"
#include "SharedFolderImpl.h"
#include "ProgressImpl.h"
#include "HostImpl.h"
#include "USBControllerImpl.h"
#include "SystemPropertiesImpl.h"
#include "GuestOSTypeImpl.h"
#include "DHCPServerRunner.h"
#include "DHCPServerImpl.h"

#include "VirtualBoxXMLUtil.h"

#include "Logging.h"

#ifdef RT_OS_WINDOWS
# include "win/svchlp.h"
#endif

// #include <stdio.h>
// #include <stdlib.h>

// defines
/////////////////////////////////////////////////////////////////////////////

#define VBOX_GLOBAL_SETTINGS_FILE "VirtualBox.xml"

// globals
/////////////////////////////////////////////////////////////////////////////

// static
Bstr VirtualBox::sVersion;

// static
ULONG VirtualBox::sRevision;

// static
Bstr VirtualBox::sPackageType;

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

VirtualBox::VirtualBox()
    : mAsyncEventThread (NIL_RTTHREAD)
    , mAsyncEventQ (NULL)
{}

VirtualBox::~VirtualBox() {}

HRESULT VirtualBox::FinalConstruct()
{
    LogFlowThisFunc(("\n"));

    return init();
}

void VirtualBox::FinalRelease()
{
    LogFlowThisFunc(("\n"));

    uninit();
}

VirtualBox::Data::Data()
{
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the VirtualBox object.
 *
 *  @return COM result code
 */
HRESULT VirtualBox::init()
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    LogFlow (("===========================================================\n"));
    LogFlowThisFuncEnter();

    if (sVersion.isNull())
        sVersion = VBOX_VERSION_STRING;
    sRevision = RTBldCfgRevision();
    if (sPackageType.isNull())
        sPackageType = VBOX_PACKAGE_STRING;
    LogFlowThisFunc(("Version: %ls, Package: %ls\n", sVersion.raw(), sPackageType.raw()));

    /* Get the VirtualBox home directory. */
    {
        char homeDir[RTPATH_MAX];
        int vrc = com::GetVBoxUserHomeDirectory(homeDir, sizeof(homeDir));
        if (RT_FAILURE(vrc))
            return setError (E_FAIL,
                tr ("Could not create the VirtualBox home directory '%s'"
                    "(%Rrc)"),
                homeDir, vrc);

        unconst(mData.mHomeDir) = homeDir;
    }

    /* compose the VirtualBox.xml file name */
    unconst(m_strSettingsFilePath) = Utf8StrFmt("%s%c%s",
                                                mData.mHomeDir.raw(),
                                                RTPATH_DELIMITER,
                                                VBOX_GLOBAL_SETTINGS_FILE);
    HRESULT rc = S_OK;
    bool fCreate = false;
    try
    {
        // load and parse VirtualBox.xml; this will throw on XML or logic errors
        try
        {
            m_pMainConfigFile = new settings::MainConfigFile(&m_strSettingsFilePath);
        }
        catch (xml::EIPRTFailure &e)
        {
            // this is thrown by the XML backend if the RTOpen() call fails;
            // only if the main settings file does not exist, create it,
            // if there's something more serious, then do fail!
            if (e.rc() == VERR_FILE_NOT_FOUND)
                fCreate = true;
            else
                throw;
        }

        if (fCreate)
            m_pMainConfigFile = new settings::MainConfigFile(NULL);

#ifdef VBOX_WITH_RESOURCE_USAGE_API
        /* create the performance collector object BEFORE host */
        unconst (mData.mPerformanceCollector).createObject();
        rc = mData.mPerformanceCollector->init();
        ComAssertComRCThrowRC(rc);
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

        /* create the host object early, machines will need it */
        unconst (mData.mHost).createObject();
        rc = mData.mHost->init(this);
        ComAssertComRCThrowRC(rc);

        rc = mData.mHost->loadSettings(m_pMainConfigFile->host);
        CheckComRCThrowRC(rc);

        /* create the system properties object, someone may need it too */
        unconst (mData.mSystemProperties).createObject();
        rc = mData.mSystemProperties->init(this);
        ComAssertComRCThrowRC (rc);

        rc = mData.mSystemProperties->loadSettings(m_pMainConfigFile->systemProperties);
        CheckComRCThrowRC(rc);

        /* guest OS type objects, needed by machines */
        for (size_t i = 0; i < RT_ELEMENTS (Global::sOSTypes); ++ i)
        {
            ComObjPtr <GuestOSType> guestOSTypeObj;
            rc = guestOSTypeObj.createObject();
            if (SUCCEEDED(rc))
            {
                rc = guestOSTypeObj->init(Global::sOSTypes [i].familyId,
                                          Global::sOSTypes [i].familyDescription,
                                          Global::sOSTypes [i].id,
                                          Global::sOSTypes [i].description,
                                          Global::sOSTypes [i].osType,
                                          Global::sOSTypes [i].osHint,
                                          Global::sOSTypes [i].recommendedRAM,
                                          Global::sOSTypes [i].recommendedVRAM,
                                          Global::sOSTypes [i].recommendedHDD,
                                          Global::sOSTypes [i].networkAdapterType,
                                          Global::sOSTypes [i].numSerialEnabled);
                if (SUCCEEDED(rc))
                    mData.mGuestOSTypes.push_back (guestOSTypeObj);
            }
            ComAssertComRCThrowRC (rc);
        }

        /* all registered media, needed by machines */
        if (FAILED(rc = initMedia()))
            throw rc;

        /* machines */
        if (FAILED(rc = initMachines()))
            throw rc;

        /* net services */
        for (settings::DHCPServersList::const_iterator it = m_pMainConfigFile->llDhcpServers.begin();
             it != m_pMainConfigFile->llDhcpServers.end();
             ++it)
        {
            const settings::DHCPServer &data = *it;

            ComObjPtr<DHCPServer> pDhcpServer;
            if (SUCCEEDED(rc = pDhcpServer.createObject()))
                rc = pDhcpServer->init(this, data);
            CheckComRCThrowRC(rc);

            rc = registerDHCPServer(pDhcpServer, false /* aSaveRegistry */);
            CheckComRCThrowRC(rc);
        }
    }
    catch (HRESULT err)
    {
        /* we assume that error info is set by the thrower */
        rc = err;
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions(RT_SRC_POS);
    }

    if (SUCCEEDED(rc))
    {
        /* start the client watcher thread */
#if defined(RT_OS_WINDOWS)
        unconst(mWatcherData.mUpdateReq) = ::CreateEvent (NULL, FALSE, FALSE, NULL);
#elif defined(RT_OS_OS2)
        RTSemEventCreate (&unconst(mWatcherData.mUpdateReq));
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
        RTSemEventCreate (&unconst(mWatcherData.mUpdateReq));
#else
# error "Port me!"
#endif
        int vrc = RTThreadCreate (&unconst(mWatcherData.mThread),
                                  ClientWatcher, (void *) this,
                                  0, RTTHREADTYPE_MAIN_WORKER,
                                  RTTHREADFLAGS_WAITABLE, "Watcher");
        ComAssertRC (vrc);
        if (RT_FAILURE(vrc))
            rc = E_FAIL;
    }

    if (SUCCEEDED(rc)) do
    {
        /* start the async event handler thread */
        int vrc = RTThreadCreate (&unconst(mAsyncEventThread), AsyncEventHandler,
                                  &unconst(mAsyncEventQ),
                                  0, RTTHREADTYPE_MAIN_WORKER,
                                  RTTHREADFLAGS_WAITABLE, "EventHandler");
        ComAssertRCBreak (vrc, rc = E_FAIL);

        /* wait until the thread sets mAsyncEventQ */
        RTThreadUserWait (mAsyncEventThread, RT_INDEFINITE_WAIT);
        ComAssertBreak (mAsyncEventQ, rc = E_FAIL);
    }
    while (0);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    LogFlow (("===========================================================\n"));
    return rc;
}

HRESULT VirtualBox::initMachines()
{
    for (settings::MachinesRegistry::const_iterator it = m_pMainConfigFile->llMachines.begin();
         it != m_pMainConfigFile->llMachines.end();
         ++it)
    {
        HRESULT rc = S_OK;
        const settings::MachineRegistryEntry &xmlMachine = *it;
        Guid uuid = xmlMachine.uuid;

        ComObjPtr<Machine> pMachine;
        if (SUCCEEDED(rc = pMachine.createObject()))
        {
            rc = pMachine->init(this,
                                xmlMachine.strSettingsFile,
                                Machine::Init_Registered,
                                NULL,
                                NULL,
                                FALSE,
                                &uuid);
            if (SUCCEEDED(rc))
                rc = registerMachine(pMachine);
            if (FAILED(rc))
                return rc;
        }
    }

    return S_OK;
}

HRESULT VirtualBox::initMedia()
{
    HRESULT rc = S_OK;
    settings::MediaList::const_iterator it;
    for (it = m_pMainConfigFile->llHardDisks.begin();
         it != m_pMainConfigFile->llHardDisks.end();
         ++it)
    {
        const settings::Medium &xmlHD = *it;

        ComObjPtr<HardDisk> pHardDisk;
        if (SUCCEEDED(rc = pHardDisk.createObject()))
            rc = pHardDisk->init(this,
                                 NULL,           // parent
                                 xmlHD);         // XML data; this recurses to processes the children
        CheckComRCReturnRC(rc);

        rc = registerHardDisk(pHardDisk, false /* aSaveRegistry */);
        CheckComRCReturnRC(rc);
    }

    for (it = m_pMainConfigFile->llDvdImages.begin();
         it != m_pMainConfigFile->llDvdImages.end();
         ++it)
    {
        const settings::Medium &xmlDvd = *it;

        ComObjPtr<DVDImage> pImage;
        if (SUCCEEDED(pImage.createObject()))
            rc = pImage->init(this, xmlDvd);
        CheckComRCReturnRC(rc);

        rc = registerDVDImage(pImage, false /* aSaveRegistry */);
        CheckComRCReturnRC(rc);
    }

    for (it = m_pMainConfigFile->llFloppyImages.begin();
         it != m_pMainConfigFile->llFloppyImages.end();
         ++it)
    {
        const settings::Medium &xmlFloppy = *it;

        ComObjPtr<FloppyImage> pImage;
        if (SUCCEEDED(pImage.createObject()))
            rc = pImage->init(this, xmlFloppy);
        CheckComRCReturnRC(rc);

        rc = registerFloppyImage(pImage, false /* aSaveRegistry */);
        CheckComRCReturnRC(rc);
    }

    return S_OK;
}

void VirtualBox::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    LogFlow (("===========================================================\n"));
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("initFailed()=%d\n", autoUninitSpan.initFailed()));

    /* tell all our child objects we've been uninitialized */

    LogFlowThisFunc(("Uninitializing machines (%d)...\n", mData.mMachines.size()));
    if (mData.mMachines.size())
    {
        MachineList::iterator it = mData.mMachines.begin();
        while (it != mData.mMachines.end())
            (*it++)->uninit();
        mData.mMachines.clear();
    }

    /* Uninit all other children still referenced by clients (unregistered
     * machines, hard disks, DVD/floppy images, server-side progress
     * operations). */
    uninitDependentChildren();

    mData.mHardDiskMap.clear();

    mData.mFloppyImages.clear();
    mData.mDVDImages.clear();
    mData.mHardDisks.clear();
    mData.mDHCPServers.clear();

    mData.mProgressOperations.clear();

    mData.mGuestOSTypes.clear();

    /* Note that we release singleton children after we've all other children.
     * In some cases this is important because these other children may use
     * some resources of the singletons which would prevent them from
     * uninitializing (as for example, mSystemProperties which owns
     * HardDiskFormat objects which HardDisk objects refer to) */
    if (mData.mSystemProperties)
    {
        mData.mSystemProperties->uninit();
        unconst(mData.mSystemProperties).setNull();
    }

    if (mData.mHost)
    {
        mData.mHost->uninit();
        unconst(mData.mHost).setNull();
    }

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    if (mData.mPerformanceCollector)
    {
        mData.mPerformanceCollector->uninit();
        unconst(mData.mPerformanceCollector).setNull();
    }
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    LogFlowThisFunc(("Releasing callbacks...\n"));
    if (mData.mCallbacks.size())
    {
        /* release all callbacks */
        LogWarningFunc (("%d unregistered callbacks!\n",
                         mData.mCallbacks.size()));
        mData.mCallbacks.clear();
    }

    LogFlowThisFunc(("Terminating the async event handler...\n"));
    if (mAsyncEventThread != NIL_RTTHREAD)
    {
        /* signal to exit the event loop */
        if (mAsyncEventQ->postEvent (NULL))
        {
            /*
             *  Wait for thread termination (only if we've successfully posted
             *  a NULL event!)
             */
            int vrc = RTThreadWait (mAsyncEventThread, 60000, NULL);
            if (RT_FAILURE(vrc))
                LogWarningFunc (("RTThreadWait(%RTthrd) -> %Rrc\n",
                                 mAsyncEventThread, vrc));
        }
        else
        {
            AssertMsgFailed (("postEvent(NULL) failed\n"));
            RTThreadWait (mAsyncEventThread, 0, NULL);
        }

        unconst(mAsyncEventThread) = NIL_RTTHREAD;
        unconst(mAsyncEventQ) = NULL;
    }

    LogFlowThisFunc(("Terminating the client watcher...\n"));
    if (mWatcherData.mThread != NIL_RTTHREAD)
    {
        /* signal the client watcher thread */
        updateClientWatcher();
        /* wait for the termination */
        RTThreadWait (mWatcherData.mThread, RT_INDEFINITE_WAIT, NULL);
        unconst(mWatcherData.mThread) = NIL_RTTHREAD;
    }
    mWatcherData.mProcesses.clear();
#if defined(RT_OS_WINDOWS)
    if (mWatcherData.mUpdateReq != NULL)
    {
        ::CloseHandle (mWatcherData.mUpdateReq);
        unconst(mWatcherData.mUpdateReq) = NULL;
    }
#elif defined(RT_OS_OS2)
    if (mWatcherData.mUpdateReq != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy (mWatcherData.mUpdateReq);
        unconst(mWatcherData.mUpdateReq) = NIL_RTSEMEVENT;
    }
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    if (mWatcherData.mUpdateReq != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy (mWatcherData.mUpdateReq);
        unconst(mWatcherData.mUpdateReq) = NIL_RTSEMEVENT;
    }
#else
# error "Port me!"
#endif

    /* Unload hard disk plugin backends. */
    VDShutdown();

    LogFlowThisFuncLeave();
    LogFlow (("===========================================================\n"));
}

// IVirtualBox properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualBox::COMGETTER(Version) (BSTR *aVersion)
{
    CheckComArgNotNull(aVersion);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    sVersion.cloneTo(aVersion);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(Revision) (ULONG *aRevision)
{
    CheckComArgNotNull(aRevision);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    *aRevision = sRevision;
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(PackageType) (BSTR *aPackageType)
{
    CheckComArgNotNull(aPackageType);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    sPackageType.cloneTo(aPackageType);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(HomeFolder) (BSTR *aHomeFolder)
{
    CheckComArgNotNull(aHomeFolder);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mHomeDir is const and doesn't need a lock */
    mData.mHomeDir.cloneTo(aHomeFolder);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(SettingsFilePath) (BSTR *aSettingsFilePath)
{
    CheckComArgNotNull(aSettingsFilePath);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mCfgFile.mName is const and doesn't need a lock */
    m_strSettingsFilePath.cloneTo(aSettingsFilePath);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(Host) (IHost **aHost)
{
    CheckComArgOutSafeArrayPointerValid(aHost);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mHost is const, no need to lock */
    mData.mHost.queryInterfaceTo(aHost);
    return S_OK;
}

STDMETHODIMP
VirtualBox::COMGETTER(SystemProperties) (ISystemProperties **aSystemProperties)
{
    CheckComArgOutSafeArrayPointerValid(aSystemProperties);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mSystemProperties is const, no need to lock */
    mData.mSystemProperties.queryInterfaceTo(aSystemProperties);
    return S_OK;
}

STDMETHODIMP
VirtualBox::COMGETTER(Machines) (ComSafeArrayOut(IMachine *, aMachines))
{
    if (ComSafeArrayOutIsNull(aMachines))
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IMachine> machines (mData.mMachines);
    machines.detachTo(ComSafeArrayOutArg(aMachines));

    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(HardDisks) (ComSafeArrayOut(IHardDisk *, aHardDisks))
{
    if (ComSafeArrayOutIsNull(aHardDisks))
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IHardDisk> hardDisks (mData.mHardDisks);
    hardDisks.detachTo(ComSafeArrayOutArg(aHardDisks));

    return S_OK;
}

STDMETHODIMP
VirtualBox::COMGETTER(DVDImages) (ComSafeArrayOut(IDVDImage *, aDVDImages))
{
    if (ComSafeArrayOutIsNull(aDVDImages))
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IDVDImage> images (mData.mDVDImages);
    images.detachTo(ComSafeArrayOutArg(aDVDImages));

    return S_OK;
}

STDMETHODIMP
VirtualBox::COMGETTER(FloppyImages) (ComSafeArrayOut(IFloppyImage *, aFloppyImages))
{
    if (ComSafeArrayOutIsNull(aFloppyImages))
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IFloppyImage> images (mData.mFloppyImages);
    images.detachTo(ComSafeArrayOutArg(aFloppyImages));

    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(ProgressOperations) (ComSafeArrayOut(IProgress *, aOperations))
{
    CheckComArgOutSafeArrayPointerValid(aOperations);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* protect mProgressOperations */
    AutoReadLock safeLock (mSafeLock);

    SafeIfaceArray<IProgress> progress (mData.mProgressOperations);
    progress.detachTo(ComSafeArrayOutArg(aOperations));

    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(GuestOSTypes) (ComSafeArrayOut(IGuestOSType *, aGuestOSTypes))
{
    CheckComArgOutSafeArrayPointerValid(aGuestOSTypes);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IGuestOSType> ostypes (mData.mGuestOSTypes);
    ostypes.detachTo(ComSafeArrayOutArg(aGuestOSTypes));

    return S_OK;
}

STDMETHODIMP
VirtualBox::COMGETTER(SharedFolders) (ComSafeArrayOut(ISharedFolder *, aSharedFolders))
{
#ifndef RT_OS_WINDOWS
    NOREF(aSharedFoldersSize);
#endif /* RT_OS_WINDOWS */

    CheckComArgOutSafeArrayPointerValid(aSharedFolders);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    return setError (E_NOTIMPL, "Not yet implemented");
}

STDMETHODIMP
VirtualBox::COMGETTER(PerformanceCollector) (IPerformanceCollector **aPerformanceCollector)
{
#ifdef VBOX_WITH_RESOURCE_USAGE_API
    CheckComArgOutSafeArrayPointerValid(aPerformanceCollector);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mPerformanceCollector is const, no need to lock */
    mData.mPerformanceCollector.queryInterfaceTo(aPerformanceCollector);

    return S_OK;
#else /* !VBOX_WITH_RESOURCE_USAGE_API */
    ReturnComNotImplemented();
#endif /* !VBOX_WITH_RESOURCE_USAGE_API */
}

STDMETHODIMP
VirtualBox::COMGETTER(DHCPServers) (ComSafeArrayOut(IDHCPServer *, aDHCPServers))
{
    if (ComSafeArrayOutIsNull(aDHCPServers))
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IDHCPServer> svrs (mData.mDHCPServers);
    svrs.detachTo(ComSafeArrayOutArg(aDHCPServers));

    return S_OK;
}

// IVirtualBox methods
/////////////////////////////////////////////////////////////////////////////

/** @note Locks mSystemProperties object for reading. */
STDMETHODIMP VirtualBox::CreateMachine(IN_BSTR aName,
                                       IN_BSTR aOsTypeId,
                                       IN_BSTR aBaseFolder,
                                       IN_BSTR aId,
                                       IMachine **aMachine)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aName=\"%ls\",aOsTypeId =\"%ls\",aBaseFolder=\"%ls\"\n", aName, aOsTypeId, aBaseFolder));

    CheckComArgStrNotEmptyOrNull (aName);
    /** @todo tighten checks on aId? */
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* Compose the settings file name using the following scheme:
     *
     *     <base_folder>/<machine_name>/<machine_name>.xml
     *
     * If a non-null and non-empty base folder is specified, the default
     * machine folder will be used as a base folder.
     */
    Utf8Str strSettingsFile = aBaseFolder;
    if (strSettingsFile.isEmpty())
        /* we use the non-full folder value below to keep the path relative */
        strSettingsFile = getDefaultMachineFolder();

    strSettingsFile = Utf8StrFmt("%s%c%ls%c%ls.xml",
                                 strSettingsFile.raw(),
                                 RTPATH_DELIMITER,
                                 aName,
                                 RTPATH_DELIMITER,
                                 aName);

    HRESULT rc = E_FAIL;

    /* create a new object */
    ComObjPtr<Machine> machine;
    rc = machine.createObject();
    CheckComRCReturnRC(rc);

    /* Create UUID if an empty one was specified. */
    Guid id(aId);
    if (id.isEmpty())
        id.create();

    /* Look for a GuestOSType object */
    AssertMsg (mData.mGuestOSTypes.size() != 0,
               ("Guest OS types array must be filled"));

    GuestOSType *osType = NULL;
    if (aOsTypeId != NULL)
    {
        for (GuestOSTypeList::const_iterator it = mData.mGuestOSTypes.begin();
             it != mData.mGuestOSTypes.end();
             ++ it)
        {
            if ((*it)->id() == aOsTypeId)
            {
                osType = *it;
                break;
            }
        }

        if (osType == NULL)
            return setError(VBOX_E_OBJECT_NOT_FOUND,
                            tr("Guest OS type '%ls' is invalid"),
                            aOsTypeId);
    }

    /* initialize the machine object */
    rc = machine->init(this,
                       strSettingsFile,
                       Machine::Init_New,
                       aName,
                       osType,
                       TRUE /* aNameSync */,
                       &id);
    if (SUCCEEDED(rc))
    {
        /* set the return value */
        rc = machine.queryInterfaceTo(aMachine);
        AssertComRC (rc);
    }

    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP VirtualBox::CreateLegacyMachine(IN_BSTR aName,
                                             IN_BSTR aOsTypeId,
                                             IN_BSTR aSettingsFile,
                                             IN_BSTR aId,
                                             IMachine **aMachine)
{
    CheckComArgStrNotEmptyOrNull (aName);
    CheckComArgStrNotEmptyOrNull (aSettingsFile);
    /** @todo tighten checks on aId? */
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    HRESULT rc = E_FAIL;

    Utf8Str settingsFile = aSettingsFile;
    /* append the default extension if none */
    if (!RTPathHaveExt(settingsFile.c_str()))
        settingsFile = Utf8StrFmt("%s.xml", settingsFile.raw());

    /* create a new object */
    ComObjPtr<Machine> machine;
    rc = machine.createObject();
    CheckComRCReturnRC(rc);

    /* Create UUID if an empty one was specified. */
    Guid id(aId);
    if (id.isEmpty())
        id.create();

    /* Look for a GuestOSType object */
    AssertMsg (mData.mGuestOSTypes.size() != 0,
               ("Guest OS types array must be filled"));

    GuestOSType *osType = NULL;
    if (aOsTypeId != NULL)
    {
        for (GuestOSTypeList::const_iterator it = mData.mGuestOSTypes.begin();
             it != mData.mGuestOSTypes.end(); ++ it)
        {
            if ((*it)->id() == aOsTypeId)
            {
                osType = *it;
                break;
            }
        }

        if (osType == NULL)
            return setError (VBOX_E_OBJECT_NOT_FOUND,
                tr ("Guest OS type '%ls' is invalid"), aOsTypeId);
    }

    /* initialize the machine object */
    rc = machine->init(this,
                       settingsFile,
                       Machine::Init_New,
                       aName,
                       osType,
                       FALSE /* aNameSync */,
                       &id);
    if (SUCCEEDED(rc))
    {
        /* set the return value */
        rc = machine.queryInterfaceTo(aMachine);
        AssertComRC (rc);
    }

    return rc;
}

STDMETHODIMP VirtualBox::OpenMachine(IN_BSTR aSettingsFile,
                                     IMachine **aMachine)
{
    CheckComArgStrNotEmptyOrNull(aSettingsFile);
    CheckComArgOutSafeArrayPointerValid(aMachine);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    HRESULT rc = E_FAIL;

    /* create a new object */
    ComObjPtr<Machine> machine;
    rc = machine.createObject();
    if (SUCCEEDED(rc))
    {
        /* initialize the machine object */
        rc = machine->init(this,
                           aSettingsFile,
                           Machine::Init_Import);
        if (SUCCEEDED(rc))
        {
            /* set the return value */
            rc = machine.queryInterfaceTo(aMachine);
            ComAssertComRC (rc);
        }
    }

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::RegisterMachine (IMachine *aMachine)
{
    CheckComArgNotNull(aMachine);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    HRESULT rc;

    Bstr name;
    rc = aMachine->COMGETTER(Name) (name.asOutParam());
    CheckComRCReturnRC(rc);

    /* We need the children map lock here to keep the getDependentChild() result
     * valid until we finish */
    AutoReadLock chLock (childrenLock());

    /* We can safely cast child to Machine * here because only Machine
     * implementations of IMachine can be among our children. */
    Machine *machine = static_cast <Machine *> (getDependentChild (aMachine));
    if (machine == NULL)
    {
        /* this machine was not created by CreateMachine() or opened by
         * OpenMachine() or loaded during startup */
        return setError (VBOX_E_INVALID_OBJECT_STATE,
            tr ("The machine named '%ls' is not created within this "
                "VirtualBox instance"), name.raw());
    }

    AutoCaller machCaller (machine);
    ComAssertComRCRetRC (machCaller.rc());

    rc = registerMachine (machine);

    /* fire an event */
    if (SUCCEEDED(rc))
        onMachineRegistered (machine->id(), TRUE);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::GetMachine (IN_BSTR aId, IMachine **aMachine)
{
    CheckComArgOutSafeArrayPointerValid(aMachine);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    ComObjPtr<Machine> machine;
    HRESULT rc = findMachine (Guid (aId), true /* setError */, &machine);

    /* the below will set *aMachine to NULL if machine is null */
    machine.queryInterfaceTo(aMachine);

    return rc;
}

/** @note Locks this object for reading, then some machine objects for reading. */
STDMETHODIMP VirtualBox::FindMachine (IN_BSTR aName, IMachine **aMachine)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aName=\"%ls\", aMachine={%p}\n", aName, aMachine));

    CheckComArgNotNull(aName);
    CheckComArgOutSafeArrayPointerValid(aMachine);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* start with not found */
    ComObjPtr<Machine> machine;
    MachineList machines;
    {
        /* take a copy for safe iteration outside the lock */
        AutoReadLock alock(this);
        machines = mData.mMachines;
    }

    for (MachineList::iterator it = machines.begin();
         !machine && it != machines.end();
         ++ it)
    {
        AutoLimitedCaller machCaller (*it);
        AssertComRC (machCaller.rc());

        /* skip inaccessible machines */
        if (machCaller.state() == Machine::Ready)
        {
            AutoReadLock machLock (*it);
            if ((*it)->name() == aName)
                machine = *it;
        }
    }

    /* this will set (*machine) to NULL if machineObj is null */
    machine.queryInterfaceTo(aMachine);

    HRESULT rc = machine
        ? S_OK
        : setError (VBOX_E_OBJECT_NOT_FOUND,
            tr ("Could not find a registered machine named '%ls'"), aName);

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::UnregisterMachine (IN_BSTR  aId,
                                            IMachine **aMachine)
{
    Guid id(aId);
    if (id.isEmpty())
        return E_INVALIDARG;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    ComObjPtr<Machine> machine;

    HRESULT rc = findMachine (id, true /* setError */, &machine);
    CheckComRCReturnRC(rc);

    rc = machine->trySetRegistered (FALSE);
    CheckComRCReturnRC(rc);

    /* remove from the collection of registered machines */
    mData.mMachines.remove (machine);

    /* save the global registry */
    rc = saveSettings();

    /* return the unregistered machine to the caller */
    machine.queryInterfaceTo(aMachine);

    /* fire an event */
    onMachineRegistered (id, FALSE);

    return rc;
}

STDMETHODIMP VirtualBox::CreateHardDisk(IN_BSTR aFormat,
                                        IN_BSTR aLocation,
                                        IHardDisk **aHardDisk)
{
    CheckComArgOutPointerValid(aHardDisk);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* we don't access non-const data members so no need to lock */

    Bstr format = aFormat;
    if (format.isEmpty())
        format = getDefaultHardDiskFormat();

    HRESULT rc = E_FAIL;

    ComObjPtr<HardDisk> hardDisk;
    hardDisk.createObject();
    rc = hardDisk->init(this, format, aLocation);

    if (SUCCEEDED(rc))
        hardDisk.queryInterfaceTo(aHardDisk);

    return rc;
}

STDMETHODIMP VirtualBox::OpenHardDisk(IN_BSTR aLocation,
                                      AccessMode_T accessMode,
                                      BOOL aSetImageId, IN_BSTR aImageId,
                                      BOOL aSetParentId, IN_BSTR aParentId,
                                      IHardDisk **aHardDisk)
{
    CheckComArgNotNull(aLocation);
    CheckComArgNotNull(aImageId);
    CheckComArgNotNull(aParentId);
    CheckComArgOutSafeArrayPointerValid(aHardDisk);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* we don't access non-const data members so no need to lock */

    HRESULT rc = E_FAIL;

    ComObjPtr<HardDisk> hardDisk;
    hardDisk.createObject();
    Guid imageId, parentId;
    if (aSetImageId)
    {
        imageId = Guid(aImageId);
        if (imageId.isEmpty())
            return setError (E_INVALIDARG, tr ("Argument %s is empty"), "aImageId");
    }
    if (aSetParentId)
        parentId = Guid(aParentId);
    rc = hardDisk->init(this,
                        aLocation,
                        (accessMode == AccessMode_ReadWrite) ? HardDisk::OpenReadWrite : HardDisk::OpenReadOnly,
                        aSetImageId, imageId,
                        aSetParentId, parentId);

    if (SUCCEEDED(rc))
    {
        rc = registerHardDisk (hardDisk);

        /* Note that it's important to call uninit() on failure to register
         * because the differencing hard disk would have been already associated
         * with the parent and this association needs to be broken. */

        if (SUCCEEDED(rc))
            hardDisk.queryInterfaceTo(aHardDisk);
        else
            hardDisk->uninit();
    }

    return rc;
}

STDMETHODIMP VirtualBox::GetHardDisk(IN_BSTR   aId,
                                     IHardDisk **aHardDisk)
{
    CheckComArgOutSafeArrayPointerValid(aHardDisk);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    Guid id(aId);
    ComObjPtr<HardDisk> hardDisk;
    HRESULT rc = findHardDisk(&id, NULL, true /* setError */, &hardDisk);

    /* the below will set *aHardDisk to NULL if hardDisk is null */
    hardDisk.queryInterfaceTo(aHardDisk);

    return rc;
}

STDMETHODIMP VirtualBox::FindHardDisk(IN_BSTR aLocation,
                                      IHardDisk **aHardDisk)
{
    CheckComArgNotNull(aLocation);
    CheckComArgOutSafeArrayPointerValid(aHardDisk);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    ComObjPtr<HardDisk> hardDisk;
    HRESULT rc = findHardDisk(NULL, aLocation, true /* setError */, &hardDisk);

    /* the below will set *aHardDisk to NULL if hardDisk is null */
    hardDisk.queryInterfaceTo(aHardDisk);

    return rc;
}

/** @note Doesn't lock anything. */
STDMETHODIMP VirtualBox::OpenDVDImage (IN_BSTR aLocation, IN_BSTR aId,
                                       IDVDImage **aDVDImage)
{
    CheckComArgStrNotEmptyOrNull(aLocation);
    CheckComArgOutSafeArrayPointerValid(aDVDImage);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    HRESULT rc = VBOX_E_FILE_ERROR;

    Guid id(aId);
    /* generate an UUID if not specified */
    if (id.isEmpty())
        id.create();

    ComObjPtr<DVDImage> image;
    image.createObject();
    rc = image->init (this, aLocation, id);
    if (SUCCEEDED(rc))
    {
        rc = registerDVDImage (image);

        if (SUCCEEDED(rc))
            image.queryInterfaceTo(aDVDImage);
    }

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::GetDVDImage (IN_BSTR aId, IDVDImage **aDVDImage)
{
    CheckComArgOutSafeArrayPointerValid(aDVDImage);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    Guid id(aId);
    ComObjPtr<DVDImage> image;
    HRESULT rc = findDVDImage (&id, NULL, true /* setError */, &image);

    /* the below will set *aDVDImage to NULL if image is null */
    image.queryInterfaceTo(aDVDImage);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::FindDVDImage (IN_BSTR aLocation, IDVDImage **aDVDImage)
{
    CheckComArgNotNull(aLocation);
    CheckComArgOutSafeArrayPointerValid(aDVDImage);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    ComObjPtr<DVDImage> image;
    HRESULT rc = findDVDImage (NULL, aLocation, true /* setError */, &image);

    /* the below will set *aDVDImage to NULL if dvd is null */
    image.queryInterfaceTo(aDVDImage);

    return rc;
}

/** @note Doesn't lock anything. */
STDMETHODIMP VirtualBox::OpenFloppyImage (IN_BSTR aLocation, IN_BSTR aId,
                                          IFloppyImage **aFloppyImage)
{
    CheckComArgStrNotEmptyOrNull(aLocation);
    CheckComArgOutSafeArrayPointerValid(aFloppyImage);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    HRESULT rc = VBOX_E_FILE_ERROR;

    Guid id(aId);
    /* generate an UUID if not specified */
    if (id.isEmpty())
        id.create();

    ComObjPtr<FloppyImage> image;
    image.createObject();
    rc = image->init (this, aLocation, id);
    if (SUCCEEDED(rc))
    {
        rc = registerFloppyImage (image);

        if (SUCCEEDED(rc))
            image.queryInterfaceTo(aFloppyImage);
    }

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::GetFloppyImage (IN_BSTR aId,
                                         IFloppyImage **aFloppyImage)

{
    CheckComArgOutSafeArrayPointerValid(aFloppyImage);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    Guid id(aId);
    ComObjPtr<FloppyImage> image;
    HRESULT rc = findFloppyImage (&id, NULL, true /* setError */, &image);

    /* the below will set *aFloppyImage to NULL if image is null */
    image.queryInterfaceTo(aFloppyImage);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::FindFloppyImage (IN_BSTR aLocation,
                                          IFloppyImage **aFloppyImage)
{
    CheckComArgNotNull(aLocation);
    CheckComArgOutSafeArrayPointerValid(aFloppyImage);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    ComObjPtr<FloppyImage> image;
    HRESULT rc = findFloppyImage(NULL, aLocation, true /* setError */, &image);

    /* the below will set *aFloppyImage to NULL if img is null */
    image.queryInterfaceTo(aFloppyImage);

    return rc;
}

/** @note Locks this object for reading. */
STDMETHODIMP VirtualBox::GetGuestOSType (IN_BSTR aId, IGuestOSType **aType)
{
    /* Old ID to new ID conversion table. See r39691 for a source */
    static const wchar_t *kOldNewIDs[] =
    {
        L"unknown", L"Other",
        L"win31", L"Windows31",
        L"win95", L"Windows95",
        L"win98", L"Windows98",
        L"winme", L"WindowsMe",
        L"winnt4", L"WindowsNT4",
        L"win2k", L"Windows2000",
        L"winxp", L"WindowsXP",
        L"win2k3", L"Windows2003",
        L"winvista", L"WindowsVista",
        L"win2k8", L"Windows2008",
        L"ecs", L"OS2eCS",
        L"fedoracore", L"Fedora",
        /* the rest is covered by the case-insensitive comparison */
    };

    CheckComArgNotNull (aType);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* first, look for a substitution */
    Bstr id = aId;
    for (size_t i = 0; i < RT_ELEMENTS (kOldNewIDs) / 2; i += 2)
    {
        if (id == kOldNewIDs [i])
        {
            id = kOldNewIDs [i + 1];
            break;
        }
    }

    *aType = NULL;

    AutoReadLock alock(this);

    for (GuestOSTypeList::iterator it = mData.mGuestOSTypes.begin();
         it != mData.mGuestOSTypes.end();
         ++ it)
    {
        const Bstr &typeId = (*it)->id();
        AssertMsg (!!typeId, ("ID must not be NULL"));
        if (typeId.compareIgnoreCase (id) == 0)
        {
            (*it).queryInterfaceTo(aType);
            break;
        }
    }

    return (*aType) ? S_OK :
        setError (E_INVALIDARG,
            tr ("'%ls' is not a valid Guest OS type"),
            aId);
}

STDMETHODIMP
VirtualBox::CreateSharedFolder (IN_BSTR aName, IN_BSTR aHostPath, BOOL /* aWritable */)
{
    CheckComArgNotNull(aName);
    CheckComArgNotNull(aHostPath);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    return setError (E_NOTIMPL, "Not yet implemented");
}

STDMETHODIMP VirtualBox::RemoveSharedFolder (IN_BSTR aName)
{
    CheckComArgNotNull(aName);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    return setError (E_NOTIMPL, "Not yet implemented");
}

/**
 *  @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::GetExtraDataKeys(ComSafeArrayOut(BSTR, aKeys))
{
    using namespace settings;

    if (ComSafeArrayOutIsNull(aKeys))
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    com::SafeArray<BSTR> saKeys(m_pMainConfigFile->mapExtraDataItems.size());
    int i = 0;
    for (ExtraDataItemsMap::const_iterator it = m_pMainConfigFile->mapExtraDataItems.begin();
         it != m_pMainConfigFile->mapExtraDataItems.end();
         ++it, ++i)
    {
        const Utf8Str &strName = it->first;     // the key
        strName.cloneTo(&saKeys[i]);
    }
    saKeys.detachTo(ComSafeArrayOutArg(aKeys));

    return S_OK;
}

/**
 *  @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::GetExtraData(IN_BSTR aKey,
                                      BSTR *aValue)
{
    CheckComArgNotNull(aKey);
    CheckComArgNotNull(aValue);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* start with nothing found */
    Bstr("").cloneTo(aValue);

    settings::ExtraDataItemsMap::const_iterator it = m_pMainConfigFile->mapExtraDataItems.find(Utf8Str(aKey));
    if (it != m_pMainConfigFile->mapExtraDataItems.end())
    {
        // found:
        const Utf8Str &strValue = it->second;
        strValue.cloneTo(aValue);
    }

    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP VirtualBox::SetExtraData(IN_BSTR aKey,
                                      IN_BSTR aValue)
{
    CheckComArgNotNull(aKey);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    Utf8Str strKey(aKey);
    Utf8Str strValue(aValue);
    Utf8Str strOldValue;            // empty

    // locking note: we only hold the read lock briefly to look up the old value,
    // then release it and call the onExtraCanChange callbacks. There is a small
    // chance of a race insofar as the callback might be called twice if two callers
    // change the same key at the same time, but that's a much better solution
    // than the deadlock we had here before. The actual changing of the extradata
    // is then performed under the write lock and race-free.

    // look up the old value first; if nothing's changed then we need not do anything
    {
        AutoReadLock alock(this); // hold read lock only while looking up
        settings::ExtraDataItemsMap::const_iterator it = m_pMainConfigFile->mapExtraDataItems.find(strKey);
        if (it != m_pMainConfigFile->mapExtraDataItems.end())
            strOldValue = it->second;
    }

    bool fChanged;
    if ((fChanged = (strOldValue != strValue)))
    {
        // ask for permission from all listeners outside the locks;
        // onExtraDataCanChange() only briefly requests the VirtualBox
        // lock to copy the list of callbacks to invoke
        Bstr error;
        Bstr bstrValue;
        if (aValue)
            bstrValue = aValue;
        else
            bstrValue = (const char *)"";

        if (!onExtraDataCanChange(Guid::Empty, aKey, bstrValue, error))
        {
            const char *sep = error.isEmpty() ? "" : ": ";
            CBSTR err = error.isNull() ? (CBSTR) L"" : error.raw();
            LogWarningFunc(("Someone vetoed! Change refused%s%ls\n",
                            sep, err));
            return setError(E_ACCESSDENIED,
                            tr("Could not set extra data because someone refused the requested change of '%ls' to '%ls'%s%ls"),
                            aKey,
                            bstrValue.raw(),
                            sep,
                            err);
        }

        // data is changing and change not vetoed: then write it out under the lock

        AutoWriteLock alock(this);

        if (strValue.isEmpty())
            m_pMainConfigFile->mapExtraDataItems.erase(strKey);
        else
            m_pMainConfigFile->mapExtraDataItems[strKey] = strValue;
                // creates a new key if needed

        /* save settings on success */
        HRESULT rc = saveSettings();
        CheckComRCReturnRC (rc);
    }

    // fire notification outside the lock
    if (fChanged)
        onExtraDataChange(Guid::Empty, aKey, aValue);

    return S_OK;
}

/**
 *  @note Locks objects!
 */
STDMETHODIMP VirtualBox::OpenSession (ISession *aSession, IN_BSTR aMachineId)
{
    CheckComArgNotNull(aSession);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    Guid id(aMachineId);
    ComObjPtr<Machine> machine;

    HRESULT rc = findMachine (id, true /* setError */, &machine);
    CheckComRCReturnRC(rc);

    /* check the session state */
    SessionState_T state;
    rc = aSession->COMGETTER(State) (&state);
    CheckComRCReturnRC(rc);

    if (state != SessionState_Closed)
        return setError (VBOX_E_INVALID_OBJECT_STATE,
            tr ("The given session is already open or being opened"));

    /* get the IInternalSessionControl interface */
    ComPtr<IInternalSessionControl> control = aSession;
    ComAssertMsgRet (!!control, ("No IInternalSessionControl interface"),
                     E_INVALIDARG);

    rc = machine->openSession (control);

    if (SUCCEEDED(rc))
    {
        /*
         *  tell the client watcher thread to update the set of
         *  machines that have open sessions
         */
        updateClientWatcher();

        /* fire an event */
        onSessionStateChange (id, SessionState_Open);
    }

    return rc;
}

/**
 *  @note Locks objects!
 */
STDMETHODIMP VirtualBox::OpenRemoteSession (ISession *aSession,
                                            IN_BSTR aMachineId,
                                            IN_BSTR aType,
                                            IN_BSTR aEnvironment,
                                            IProgress **aProgress)
{
    LogRel(("remotesession=%s\n", Utf8Str(aMachineId).c_str()));

    CheckComArgNotNull(aMachineId);
    CheckComArgNotNull(aSession);
    CheckComArgNotNull(aType);
    CheckComArgOutSafeArrayPointerValid(aProgress);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    Guid id(aMachineId);
    ComObjPtr<Machine> machine;

    HRESULT rc = findMachine (id, true /* setError */, &machine);
    CheckComRCReturnRC(rc);

    /* check the session state */
    SessionState_T state;
    rc = aSession->COMGETTER(State) (&state);
    CheckComRCReturnRC(rc);

    if (state != SessionState_Closed)
        return setError (VBOX_E_INVALID_OBJECT_STATE,
            tr ("The given session is already open or being opened"));

    /* get the IInternalSessionControl interface */
    ComPtr<IInternalSessionControl> control = aSession;
    ComAssertMsgRet (!!control, ("No IInternalSessionControl interface"),
                     E_INVALIDARG);

    /* create a progress object */
    ComObjPtr<Progress> progress;
    progress.createObject();
    progress->init (this, static_cast <IMachine *> (machine),
                    Bstr (tr ("Spawning session")),
                    FALSE /* aCancelable */);

    rc = machine->openRemoteSession (control, aType, aEnvironment, progress);

    if (SUCCEEDED(rc))
    {
        progress.queryInterfaceTo(aProgress);

        /* signal the client watcher thread */
        updateClientWatcher();

        /* fire an event */
        onSessionStateChange (id, SessionState_Spawning);
    }

    return rc;
}

/**
 *  @note Locks objects!
 */
STDMETHODIMP VirtualBox::OpenExistingSession (ISession *aSession,
                                              IN_BSTR  aMachineId)
{
    CheckComArgNotNull(aSession);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    Guid id(aMachineId);
    ComObjPtr<Machine> machine;

    HRESULT rc = findMachine (id, true /* setError */, &machine);
    CheckComRCReturnRC(rc);

    /* check the session state */
    SessionState_T state;
    rc = aSession->COMGETTER(State) (&state);
    CheckComRCReturnRC(rc);

    if (state != SessionState_Closed)
        return setError (VBOX_E_INVALID_OBJECT_STATE,
            tr ("The given session is already open or being opened"));

    /* get the IInternalSessionControl interface */
    ComPtr<IInternalSessionControl> control = aSession;
    ComAssertMsgRet (!!control, ("No IInternalSessionControl interface"),
                     E_INVALIDARG);

    rc = machine->openExistingSession (control);

    return rc;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP VirtualBox::RegisterCallback (IVirtualBoxCallback *aCallback)
{
    LogFlowThisFunc(("aCallback=%p\n", aCallback));

    CheckComArgNotNull(aCallback);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

#if 0 /** @todo r=bird,r=pritesh: must check that the interface id match correct or we might screw up with old code! */
    void *dummy;
    HRESULT hrc = aCallback->QueryInterface(NS_GET_IID(IVirtualBoxCallback), &dummy);
    if (FAILED(hrc))
        return hrc;
    aCallback->Release();
#endif

    AutoWriteLock alock(this);
    mData.mCallbacks.push_back (CallbackList::value_type (aCallback));

    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP VirtualBox::UnregisterCallback (IVirtualBoxCallback *aCallback)
{
    CheckComArgNotNull(aCallback);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    HRESULT rc = S_OK;

    AutoWriteLock alock(this);

    CallbackList::iterator it;
    it = std::find (mData.mCallbacks.begin(),
                    mData.mCallbacks.end(),
                    CallbackList::value_type (aCallback));
    if (it == mData.mCallbacks.end())
        rc = E_INVALIDARG;
    else
        mData.mCallbacks.erase (it);

    LogFlowThisFunc(("aCallback=%p, rc=%08X\n", aCallback, rc));
    return rc;
}


STDMETHODIMP VirtualBox::WaitForPropertyChange (IN_BSTR /* aWhat */, ULONG /* aTimeout */,
                                                BSTR * /* aChanged */, BSTR * /* aValues */)
{
    ReturnComNotImplemented();
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  Posts an event to the event queue that is processed asynchronously
 *  on a dedicated thread.
 *
 *  Posting events to the dedicated event queue is useful to perform secondary
 *  actions outside any object locks -- for example, to iterate over a list
 *  of callbacks and inform them about some change caused by some object's
 *  method call.
 *
 *  @param event    event to post
 *                  (must be allocated using |new|, will be deleted automatically
 *                  by the event thread after processing)
 *
 *  @note Doesn't lock any object.
 */
HRESULT VirtualBox::postEvent (Event *event)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    if (autoCaller.state() != Ready)
    {
        LogWarningFunc (("VirtualBox has been uninitialized (state=%d), "
                         "the event is discarded!\n",
                         autoCaller.state()));
        return S_OK;
    }

    AssertReturn(event, E_FAIL);
    AssertReturn(mAsyncEventQ, E_FAIL);

    if (mAsyncEventQ->postEvent (event))
        return S_OK;

    return E_FAIL;
}

/**
 * Adds a progress to the global collection of pending operations.
 * Usually gets called upon progress object initialization.
 *
 * @param aProgress Operation to add to the collection.
 *
 * @note Doesn't lock objects.
 */
HRESULT VirtualBox::addProgress (IProgress *aProgress)
{
    CheckComArgNotNull(aProgress);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    Bstr id;
    HRESULT rc = aProgress->COMGETTER(Id) (id.asOutParam());
    AssertComRCReturnRC(rc);

    /* protect mProgressOperations */
    AutoWriteLock safeLock (mSafeLock);

    mData.mProgressOperations.insert (ProgressMap::value_type (Guid(id), aProgress));
    return S_OK;
}

/**
 * Removes the progress from the global collection of pending operations.
 * Usually gets called upon progress completion.
 *
 * @param aId   UUID of the progress operation to remove
 *
 * @note Doesn't lock objects.
 */
HRESULT VirtualBox::removeProgress (IN_GUID aId)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    ComPtr<IProgress> progress;

    /* protect mProgressOperations */
    AutoWriteLock safeLock (mSafeLock);

    size_t cnt = mData.mProgressOperations.erase (aId);
    Assert (cnt == 1);
    NOREF(cnt);

    return S_OK;
}

#ifdef RT_OS_WINDOWS

struct StartSVCHelperClientData
{
    ComObjPtr<VirtualBox> that;
    ComObjPtr<Progress> progress;
    bool privileged;
    VirtualBox::SVCHelperClientFunc func;
    void *user;
};

/**
 *  Helper method that starts a worker thread that:
 *  - creates a pipe communication channel using SVCHlpClient;
 *  - starts an SVC Helper process that will inherit this channel;
 *  - executes the supplied function by passing it the created SVCHlpClient
 *    and opened instance to communicate to the Helper process and the given
 *    Progress object.
 *
 *  The user function is supposed to communicate to the helper process
 *  using the \a aClient argument to do the requested job and optionally expose
 *  the progress through the \a aProgress object. The user function should never
 *  call notifyComplete() on it: this will be done automatically using the
 *  result code returned by the function.
 *
 *  Before the user function is started, the communication channel passed to
 *  the \a aClient argument is fully set up, the function should start using
 *  its write() and read() methods directly.
 *
 *  The \a aVrc parameter of the user function may be used to return an error
 *  code if it is related to communication errors (for example, returned by
 *  the SVCHlpClient members when they fail). In this case, the correct error
 *  message using this value will be reported to the caller. Note that the
 *  value of \a aVrc is inspected only if the user function itself returns
 *  success.
 *
 *  If a failure happens anywhere before the user function would be normally
 *  called, it will be called anyway in special "cleanup only" mode indicated
 *  by \a aClient, \a aProgress and \aVrc arguments set to NULL. In this mode,
 *  all the function is supposed to do is to cleanup its aUser argument if
 *  necessary (it's assumed that the ownership of this argument is passed to
 *  the user function once #startSVCHelperClient() returns a success, thus
 *  making it responsible for the cleanup).
 *
 *  After the user function returns, the thread will send the SVCHlpMsg::Null
 *  message to indicate a process termination.
 *
 *  @param  aPrivileged |true| to start the SVC Helper process as a privileged
 *                      user that can perform administrative tasks
 *  @param  aFunc       user function to run
 *  @param  aUser       argument to the user function
 *  @param  aProgress   progress object that will track operation completion
 *
 *  @note aPrivileged is currently ignored (due to some unsolved problems in
 *        Vista) and the process will be started as a normal (unprivileged)
 *        process.
 *
 *  @note Doesn't lock anything.
 */
HRESULT VirtualBox::startSVCHelperClient (bool aPrivileged,
                                          SVCHelperClientFunc aFunc,
                                          void *aUser, Progress *aProgress)
{
    AssertReturn(aFunc, E_POINTER);
    AssertReturn(aProgress, E_POINTER);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* create the SVCHelperClientThread() argument */
    std::auto_ptr <StartSVCHelperClientData>
        d (new StartSVCHelperClientData());
    AssertReturn(d.get(), E_OUTOFMEMORY);

    d->that = this;
    d->progress = aProgress;
    d->privileged = aPrivileged;
    d->func = aFunc;
    d->user = aUser;

    RTTHREAD tid = NIL_RTTHREAD;
    int vrc = RTThreadCreate (&tid, SVCHelperClientThread,
                              static_cast <void *> (d.get()),
                              0, RTTHREADTYPE_MAIN_WORKER,
                              RTTHREADFLAGS_WAITABLE, "SVCHelper");

    ComAssertMsgRCRet (vrc, ("Could not create SVCHelper thread (%Rrc)", vrc),
                       E_FAIL);

    /* d is now owned by SVCHelperClientThread(), so release it */
    d.release();

    return S_OK;
}

/**
 *  Worker thread for startSVCHelperClient().
 */
/* static */
DECLCALLBACK(int)
VirtualBox::SVCHelperClientThread (RTTHREAD aThread, void *aUser)
{
    LogFlowFuncEnter();

    std::auto_ptr <StartSVCHelperClientData>
        d (static_cast <StartSVCHelperClientData *> (aUser));

    HRESULT rc = S_OK;
    bool userFuncCalled = false;

    do
    {
        AssertBreakStmt (d.get(), rc = E_POINTER);
        AssertReturn(!d->progress.isNull(), E_POINTER);

        /* protect VirtualBox from uninitialization */
        AutoCaller autoCaller(d->that);
        if (!autoCaller.isOk())
        {
            /* it's too late */
            rc = autoCaller.rc();
            break;
        }

        int vrc = VINF_SUCCESS;

        Guid id;
        id.create();
        SVCHlpClient client;
        vrc = client.create(Utf8StrFmt("VirtualBox\\SVCHelper\\{%RTuuid}",
                                       id.raw()).c_str());
        if (RT_FAILURE(vrc))
        {
            rc = setError(E_FAIL,
                          tr("Could not create the communication channel (%Rrc)"), vrc);
            break;
        }

        /* get the path to the executable */
        char exePathBuf [RTPATH_MAX];
        char *exePath = RTProcGetExecutableName (exePathBuf, RTPATH_MAX);
        ComAssertBreak (exePath, E_FAIL);

        Utf8Str argsStr = Utf8StrFmt ("/Helper %s", client.name().raw());

        LogFlowFunc (("Starting '\"%s\" %s'...\n", exePath, argsStr.raw()));

        RTPROCESS pid = NIL_RTPROCESS;

        if (d->privileged)
        {
            /* Attempt to start a privileged process using the Run As dialog */

            Bstr file = exePath;
            Bstr parameters = argsStr;

            SHELLEXECUTEINFO shExecInfo;

            shExecInfo.cbSize = sizeof (SHELLEXECUTEINFO);

            shExecInfo.fMask = NULL;
            shExecInfo.hwnd = NULL;
            shExecInfo.lpVerb = L"runas";
            shExecInfo.lpFile = file;
            shExecInfo.lpParameters = parameters;
            shExecInfo.lpDirectory = NULL;
            shExecInfo.nShow = SW_NORMAL;
            shExecInfo.hInstApp = NULL;

            if (!ShellExecuteEx (&shExecInfo))
            {
                int vrc2 = RTErrConvertFromWin32 (GetLastError());
                /* hide excessive details in case of a frequent error
                 * (pressing the Cancel button to close the Run As dialog) */
                if (vrc2 == VERR_CANCELLED)
                    rc = setError (E_FAIL,
                        tr ("Operation cancelled by the user"));
                else
                    rc = setError (E_FAIL,
                        tr ("Could not launch a privileged process '%s' (%Rrc)"),
                        exePath, vrc2);
                break;
            }
        }
        else
        {
            const char *args[] = { exePath, "/Helper", client.name().c_str(), 0 };
            vrc = RTProcCreate (exePath, args, RTENV_DEFAULT, 0, &pid);
            if (RT_FAILURE(vrc))
            {
                rc = setError (E_FAIL,
                    tr ("Could not launch a process '%s' (%Rrc)"), exePath, vrc);
                break;
            }
        }

        /* wait for the client to connect */
        vrc = client.connect();
        if (RT_SUCCESS(vrc))
        {
            /* start the user supplied function */
            rc = d->func (&client, d->progress, d->user, &vrc);
            userFuncCalled = true;
        }

        /* send the termination signal to the process anyway */
        {
            int vrc2 = client.write (SVCHlpMsg::Null);
            if (RT_SUCCESS(vrc))
                vrc = vrc2;
        }

        if (SUCCEEDED(rc) && RT_FAILURE(vrc))
        {
            rc = setError (E_FAIL,
                tr ("Could not operate the communication channel (%Rrc)"), vrc);
            break;
        }
    }
    while (0);

    if (FAILED (rc) && !userFuncCalled)
    {
        /* call the user function in the "cleanup only" mode
         * to let it free resources passed to in aUser */
        d->func (NULL, NULL, d->user, NULL);
    }

    d->progress->notifyComplete (rc);

    LogFlowFuncLeave();
    return 0;
}

#endif /* RT_OS_WINDOWS */

/**
 *  Sends a signal to the client watcher thread to rescan the set of machines
 *  that have open sessions.
 *
 *  @note Doesn't lock anything.
 */
void VirtualBox::updateClientWatcher()
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    AssertReturn(mWatcherData.mThread != NIL_RTTHREAD, (void) 0);

    /* sent an update request */
#if defined(RT_OS_WINDOWS)
    ::SetEvent (mWatcherData.mUpdateReq);
#elif defined(RT_OS_OS2)
    RTSemEventSignal (mWatcherData.mUpdateReq);
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    RTSemEventSignal (mWatcherData.mUpdateReq);
#else
# error "Port me!"
#endif
}

/**
 *  Adds the given child process ID to the list of processes to be reaped.
 *  This call should be followed by #updateClientWatcher() to take the effect.
 */
void VirtualBox::addProcessToReap (RTPROCESS pid)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    /// @todo (dmik) Win32?
#ifndef RT_OS_WINDOWS
    AutoWriteLock alock(this);
    mWatcherData.mProcesses.push_back (pid);
#endif
}

/** Event for onMachineStateChange(), onMachineDataChange(), onMachineRegistered() */
struct MachineEvent : public VirtualBox::CallbackEvent
{
    enum What { DataChanged, StateChanged, Registered };

    MachineEvent (VirtualBox *aVB, const Guid &aId)
        : CallbackEvent (aVB), what (DataChanged), id (aId)
        {}

    MachineEvent (VirtualBox *aVB, const Guid &aId, MachineState_T aState)
        : CallbackEvent (aVB), what (StateChanged), id (aId)
        , state (aState)
        {}

    MachineEvent (VirtualBox *aVB, const Guid &aId, BOOL aRegistered)
        : CallbackEvent (aVB), what (Registered), id (aId)
        , registered (aRegistered)
        {}

    void handleCallback (const ComPtr<IVirtualBoxCallback> &aCallback)
    {
        switch (what)
        {
            case DataChanged:
                LogFlow (("OnMachineDataChange: id={%RTuuid}\n", id.ptr()));
                aCallback->OnMachineDataChange (id.toUtf16());
                break;

            case StateChanged:
                LogFlow (("OnMachineStateChange: id={%RTuuid}, state=%d\n",
                          id.ptr(), state));
                aCallback->OnMachineStateChange (id.toUtf16(), state);
                break;

            case Registered:
                LogFlow (("OnMachineRegistered: id={%RTuuid}, registered=%d\n",
                          id.ptr(), registered));
                aCallback->OnMachineRegistered (id.toUtf16(), registered);
                break;
        }
    }

    const What what;

    Guid id;
    MachineState_T state;
    BOOL registered;
};

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onMachineStateChange(const Guid &aId, MachineState_T aState)
{
    postEvent (new MachineEvent (this, aId, aState));
}

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onMachineDataChange(const Guid &aId)
{
    postEvent (new MachineEvent (this, aId));
}

/**
 *  @note Locks this object for reading.
 */
BOOL VirtualBox::onExtraDataCanChange (const Guid &aId, IN_BSTR aKey, IN_BSTR aValue,
                                       Bstr &aError)
{
    LogFlowThisFunc(("machine={%s} aKey={%ls} aValue={%ls}\n",
                      aId.toString().raw(), aKey, aValue));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    CallbackList list;
    {
        AutoReadLock alock(this);
        list = mData.mCallbacks;
    }

    BOOL allowChange = TRUE;
    CallbackList::iterator it = list.begin();
    Bstr id = aId.toUtf16();
    while ((it != list.end()) && allowChange)
    {
        HRESULT rc = (*it++)->OnExtraDataCanChange (id, aKey, aValue,
                                                    aError.asOutParam(), &allowChange);
        if (FAILED (rc))
        {
            /* if a call to this method fails for some reason (for ex., because
             * the other side is dead), we ensure allowChange stays true
             * (MS COM RPC implementation seems to zero all output vars before
             * issuing an IPC call or after a failure, so it's essential
             * there) */
            allowChange = TRUE;
        }
    }

    LogFlowThisFunc(("allowChange=%RTbool\n", allowChange));
    return allowChange;
}

/** Event for onExtraDataChange() */
struct ExtraDataEvent : public VirtualBox::CallbackEvent
{
    ExtraDataEvent (VirtualBox *aVB, const Guid &aMachineId,
                    IN_BSTR aKey, IN_BSTR aVal)
        : CallbackEvent (aVB), machineId (aMachineId)
        , key (aKey), val (aVal)
        {}

    void handleCallback (const ComPtr<IVirtualBoxCallback> &aCallback)
    {
        LogFlow (("OnExtraDataChange: machineId={%RTuuid}, key='%ls', val='%ls'\n",
                  machineId.ptr(), key.raw(), val.raw()));
        aCallback->OnExtraDataChange (machineId.toUtf16(), key, val);
    }

    Guid machineId;
    Bstr key, val;
};

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onExtraDataChange (const Guid &aId, IN_BSTR aKey, IN_BSTR aValue)
{
    postEvent (new ExtraDataEvent (this, aId, aKey, aValue));
}

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onMachineRegistered (const Guid &aId, BOOL aRegistered)
{
    postEvent (new MachineEvent (this, aId, aRegistered));
}

/** Event for onSessionStateChange() */
struct SessionEvent : public VirtualBox::CallbackEvent
{
    SessionEvent (VirtualBox *aVB, const Guid &aMachineId, SessionState_T aState)
        : CallbackEvent (aVB), machineId (aMachineId), sessionState (aState)
        {}

    void handleCallback (const ComPtr<IVirtualBoxCallback> &aCallback)
    {
        LogFlow (("OnSessionStateChange: machineId={%RTuuid}, sessionState=%d\n",
                  machineId.ptr(), sessionState));
        aCallback->OnSessionStateChange (machineId.toUtf16(), sessionState);
    }

    Guid machineId;
    SessionState_T sessionState;
};

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onSessionStateChange (const Guid &aId, SessionState_T aState)
{
    postEvent (new SessionEvent (this, aId, aState));
}

/** Event for onSnapshotTaken(), onSnapshotRemoved() and onSnapshotChange() */
struct SnapshotEvent : public VirtualBox::CallbackEvent
{
    enum What { Taken, Discarded, Changed };

    SnapshotEvent (VirtualBox *aVB, const Guid &aMachineId, const Guid &aSnapshotId,
                   What aWhat)
        : CallbackEvent (aVB)
        , what (aWhat)
        , machineId (aMachineId), snapshotId (aSnapshotId)
        {}

    void handleCallback (const ComPtr<IVirtualBoxCallback> &aCallback)
    {
        Bstr mid = machineId.toUtf16();
	Bstr sid = snapshotId.toUtf16();

        switch (what)
        {
            case Taken:
                LogFlow (("OnSnapshotTaken: machineId={%RTuuid}, snapshotId={%RTuuid}\n",
                          machineId.ptr(), snapshotId.ptr()));
                aCallback->OnSnapshotTaken (mid, sid);
                break;

            case Discarded:
                LogFlow (("OnSnapshotDiscarded: machineId={%RTuuid}, snapshotId={%RTuuid}\n",
                          machineId.ptr(), snapshotId.ptr()));
                aCallback->OnSnapshotDiscarded (mid, sid);
                break;

            case Changed:
                LogFlow (("OnSnapshotChange: machineId={%RTuuid}, snapshotId={%RTuuid}\n",
                          machineId.ptr(), snapshotId.ptr()));
                aCallback->OnSnapshotChange (mid, sid);
                break;
        }
    }

    const What what;

    Guid machineId;
    Guid snapshotId;
};

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onSnapshotTaken (const Guid &aMachineId, const Guid &aSnapshotId)
{
    postEvent (new SnapshotEvent (this, aMachineId, aSnapshotId, SnapshotEvent::Taken));
}

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onSnapshotDiscarded (const Guid &aMachineId, const Guid &aSnapshotId)
{
    postEvent (new SnapshotEvent (this, aMachineId, aSnapshotId, SnapshotEvent::Discarded));
}

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onSnapshotChange (const Guid &aMachineId, const Guid &aSnapshotId)
{
    postEvent (new SnapshotEvent (this, aMachineId, aSnapshotId, SnapshotEvent::Changed));
}

/** Event for onGuestPropertyChange() */
struct GuestPropertyEvent : public VirtualBox::CallbackEvent
{
    GuestPropertyEvent (VirtualBox *aVBox, const Guid &aMachineId,
                        IN_BSTR aName, IN_BSTR aValue, IN_BSTR aFlags)
        : CallbackEvent (aVBox), machineId (aMachineId)
        , name (aName), value (aValue), flags(aFlags)
        {}

    void handleCallback (const ComPtr<IVirtualBoxCallback> &aCallback)
    {
        LogFlow (("OnGuestPropertyChange: machineId={%RTuuid}, name='%ls', value='%ls', flags='%ls'\n",
                  machineId.ptr(), name.raw(), value.raw(), flags.raw()));
        aCallback->OnGuestPropertyChange (machineId.toUtf16(), name, value, flags);
    }

    Guid machineId;
    Bstr name, value, flags;
};

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onGuestPropertyChange (const Guid &aMachineId, IN_BSTR aName,
                                        IN_BSTR aValue, IN_BSTR aFlags)
{
    postEvent (new GuestPropertyEvent (this, aMachineId, aName, aValue, aFlags));
}

/**
 *  @note Locks this object for reading.
 */
ComObjPtr<GuestOSType> VirtualBox::getUnknownOSType()
{
    ComObjPtr<GuestOSType> type;

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), type);

    AutoReadLock alock(this);

    /* unknown type must always be the first */
    ComAssertRet (mData.mGuestOSTypes.size() > 0, type);

    type = mData.mGuestOSTypes.front();
    return type;
}

/**
 * Returns the list of opened machines (machines having direct sessions opened
 * by client processes) and optionally the list of direct session controls.
 *
 * @param aMachines     Where to put opened machines (will be empty if none).
 * @param aControls     Where to put direct session controls (optional).
 *
 * @note The returned lists contain smart pointers. So, clear it as soon as
 * it becomes no more necessary to release instances.
 *
 * @note It can be possible that a session machine from the list has been
 * already uninitialized, so do a usual AutoCaller/AutoReadLock sequence
 * when accessing unprotected data directly.
 *
 * @note Locks objects for reading.
 */
void VirtualBox::getOpenedMachines (SessionMachineVector &aMachines,
                                    InternalControlVector *aControls /*= NULL*/)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    aMachines.clear();
    if (aControls)
        aControls->clear();

    AutoReadLock alock(this);

    for (MachineList::iterator it = mData.mMachines.begin();
         it != mData.mMachines.end();
         ++ it)
    {
        ComObjPtr<SessionMachine> sm;
        ComPtr<IInternalSessionControl> ctl;
        if ((*it)->isSessionOpen (sm, &ctl))
        {
            aMachines.push_back (sm);
            if (aControls)
                aControls->push_back (ctl);
        }
    }
}

/**
 *  Searches for a Machine object with the given ID in the collection
 *  of registered machines.
 *
 *  @param id
 *      ID of the machine
 *  @param doSetError
 *      if TRUE, the appropriate error info is set in case when the machine
 *      is not found
 *  @param machine
 *      where to store the found machine object (can be NULL)
 *
 *  @return
 *      S_OK when found or VBOX_E_OBJECT_NOT_FOUND when not found
 *
 *  @note Locks this object for reading.
 */
HRESULT VirtualBox::findMachine (const Guid &aId, bool aSetError,
                                 ComObjPtr<Machine> *aMachine /* = NULL */)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    bool found = false;

    {
        AutoReadLock alock(this);

        for (MachineList::iterator it = mData.mMachines.begin();
             !found && it != mData.mMachines.end();
             ++ it)
        {
            /* sanity */
            AutoLimitedCaller machCaller (*it);
            AssertComRC (machCaller.rc());

            found = (*it)->id() == aId;
            if (found && aMachine)
                *aMachine = *it;
        }
    }

    HRESULT rc = found ? S_OK : VBOX_E_OBJECT_NOT_FOUND;

    if (aSetError && !found)
    {
        setError (VBOX_E_OBJECT_NOT_FOUND,
            tr ("Could not find a registered machine with UUID {%RTuuid}"),
            aId.raw());
    }

    return rc;
}

/**
 * Searches for a HardDisk object with the given ID or location in the list of
 * registered hard disks. If both ID and location are specified, the first
 * object that matches either of them (not necessarily both) is returned.
 *
 * @param aId           ID of the hard disk (unused when NULL).
 * @param aLocation     Full location specification (unused NULL).
 * @param aSetError     If @c true , the appropriate error info is set in case
 *                      when the hard disk is not found.
 * @param aHardDisk     Where to store the found hard disk object (can be NULL).
 *
 * @return S_OK when found or E_INVALIDARG when not found.
 *
 * @note Locks this object and hard disk objects for reading.
 */
HRESULT VirtualBox::findHardDisk(const Guid *aId,
                                 CBSTR aLocation,
                                 bool aSetError,
                                 ComObjPtr<HardDisk> *aHardDisk /*= NULL*/)
{
    AssertReturn(aId || aLocation, E_INVALIDARG);

    AutoReadLock alock(this);

    /* first, look up by UUID in the map if UUID is provided */
    if (aId)
    {
        HardDiskMap::const_iterator it = mData.mHardDiskMap.find (*aId);
        if (it != mData.mHardDiskMap.end())
        {
            if (aHardDisk)
                *aHardDisk = (*it).second;
            return S_OK;
        }
    }

    /* then iterate and search by location */
    int result = -1;
    if (aLocation)
    {
        Utf8Str location = aLocation;

        for (HardDiskMap::const_iterator it = mData.mHardDiskMap.begin();
             it != mData.mHardDiskMap.end();
             ++ it)
        {
            const ComObjPtr<HardDisk> &hd = (*it).second;

            HRESULT rc = hd->compareLocationTo(location, result);
            CheckComRCReturnRC(rc);

            if (result == 0)
            {
                if (aHardDisk)
                    *aHardDisk = hd;
                break;
            }
        }
    }

    HRESULT rc = result == 0 ? S_OK : VBOX_E_OBJECT_NOT_FOUND;

    if (aSetError && result != 0)
    {
        if (aId)
            setError(rc,
                     tr("Could not find a hard disk with UUID {%RTuuid} in the media registry ('%s')"),
                     aId->raw(),
                     m_strSettingsFilePath.raw());
        else
            setError(rc,
                     tr("Could not find a hard disk with location '%ls' in the media registry ('%s')"),
                     aLocation,
                     m_strSettingsFilePath.raw());
    }

    return rc;
}

/**
 * Searches for a DVDImage object with the given ID or location in the list of
 * registered DVD images. If both ID and file path are specified, the first
 * object that matches either of them (not necessarily both) is returned.
 *
 * @param aId       ID of the DVD image (unused when NULL).
 * @param aLocation Full path to the image file (unused when NULL).
 * @param aSetError If @c true, the appropriate error info is set in case when
 *                  the image is not found.
 * @param aImage    Where to store the found image object (can be NULL).
 *
 * @return S_OK when found or E_INVALIDARG when not found.
 *
 * @note Locks this object and image objects for reading.
 */
HRESULT VirtualBox::findDVDImage(const Guid *aId,
                                 CBSTR aLocation,
                                 bool aSetError,
                                 ComObjPtr<DVDImage> *aImage /* = NULL */)
{
    AssertReturn(aId || aLocation, E_INVALIDARG);

    Utf8Str location;

    if (aLocation != NULL)
    {
        int vrc = calculateFullPath(Utf8Str(aLocation), location);
        if (RT_FAILURE(vrc))
            return setError(VBOX_E_FILE_ERROR,
                            tr("Invalid image file location '%ls' (%Rrc)"),
                            aLocation,
                            vrc);
    }

    AutoReadLock alock(this);

    bool found = false;

    for (DVDImageList::const_iterator it = mData.mDVDImages.begin();
         it != mData.mDVDImages.end();
         ++ it)
    {
        /* no AutoCaller, registered image life time is bound to this */
        AutoReadLock imageLock (*it);

        found = (aId && (*it)->id() == *aId) ||
                (aLocation != NULL &&
                 RTPathCompare(location.c_str(),
                               Utf8Str((*it)->locationFull()).c_str()
                              ) == 0);
        if (found)
        {
            if (aImage)
                *aImage = *it;
            break;
        }
    }

    HRESULT rc = found ? S_OK : VBOX_E_OBJECT_NOT_FOUND;

    if (aSetError && !found)
    {
        if (aId)
            setError(rc,
                     tr("Could not find a CD/DVD image with UUID {%RTuuid} in the media registry ('%s')"),
                     aId->raw(),
                     m_strSettingsFilePath.raw());
        else
            setError(rc,
                     tr("Could not find a CD/DVD image with location '%ls' in the media registry ('%s')"),
                     aLocation,
                     m_strSettingsFilePath.raw());
    }

    return rc;
}

/**
 * Searches for a FloppyImage object with the given ID or location in the
 * collection of registered DVD images. If both ID and file path are specified,
 * the first object that matches either of them (not necessarily both) is
 * returned.
 *
 * @param aId       ID of the DVD image (unused when NULL).
 * @param aLocation Full path to the image file (unused when NULL).
 * @param aSetError If @c true, the appropriate error info is set in case when
 *                  the image is not found.
 * @param aImage    Where to store the found image object (can be NULL).
 *
 * @return S_OK when found or E_INVALIDARG when not found.
 *
 * @note Locks this object and image objects for reading.
 */
HRESULT VirtualBox::findFloppyImage(const Guid *aId, CBSTR aLocation,
                                    bool aSetError,
                                    ComObjPtr<FloppyImage> *aImage /* = NULL */)
{
    AssertReturn(aId || aLocation, E_INVALIDARG);

    Utf8Str location;

    if (aLocation != NULL)
    {
        int vrc = calculateFullPath(Utf8Str(aLocation), location);
        if (RT_FAILURE(vrc))
            return setError (VBOX_E_FILE_ERROR,
                tr ("Invalid image file location '%ls' (%Rrc)"),
                aLocation, vrc);
    }

    AutoReadLock alock(this);

    bool found = false;

    for (FloppyImageList::const_iterator it = mData.mFloppyImages.begin();
         it != mData.mFloppyImages.end();
         ++ it)
    {
        /* no AutoCaller, registered image life time is bound to this */
        AutoReadLock imageLock (*it);

        found = (aId && (*it)->id() == *aId) ||
                (aLocation != NULL &&
                 RTPathCompare(location.c_str(),
                               Utf8Str((*it)->locationFull()).c_str()
                              ) == 0);
        if (found)
        {
            if (aImage)
                *aImage = *it;
            break;
        }
    }

    HRESULT rc = found ? S_OK : VBOX_E_OBJECT_NOT_FOUND;

    if (aSetError && !found)
    {
        if (aId)
            setError(rc,
                     tr("Could not find a floppy image with UUID {%RTuuid} in the media registry ('%s')"),
                     aId->raw(),
                     m_strSettingsFilePath.raw());
        else
            setError(rc,
                     tr("Could not find a floppy image with location '%ls' in the media registry ('%s')"),
                     aLocation,
                     m_strSettingsFilePath.raw());
    }

    return rc;
}

/**
 * Returns the default machine folder from the system properties
 * with proper locking.
 * @return
 */
const Utf8Str& VirtualBox::getDefaultMachineFolder() const
{
    AutoReadLock propsLock(mData.mSystemProperties);
    return mData.mSystemProperties->m_strDefaultMachineFolder;
}

/**
 * Returns the default hard disk folder from the system properties
 * with proper locking.
 * @return
 */
const Utf8Str& VirtualBox::getDefaultHardDiskFolder() const
{
    AutoReadLock propsLock(mData.mSystemProperties);
    return mData.mSystemProperties->m_strDefaultHardDiskFolder;
}

/**
 * Returns the default hard disk format from the system properties
 * with proper locking.
 * @return
 */
const Utf8Str& VirtualBox::getDefaultHardDiskFormat() const
{
    AutoReadLock propsLock(mData.mSystemProperties);
    return mData.mSystemProperties->m_strDefaultHardDiskFormat;
}

/**
 * Calculates the absolute path of the given path taking the VirtualBox home
 * directory as the current directory.
 *
 * @param  aPath    Path to calculate the absolute path for.
 * @param  aResult  Where to put the result (used only on success, can be the
 *                  same Utf8Str instance as passed in @a aPath).
 * @return IPRT result.
 *
 * @note Doesn't lock any object.
 */
int VirtualBox::calculateFullPath(const Utf8Str &strPath, Utf8Str &aResult)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), VERR_GENERAL_FAILURE);

    /* no need to lock since mHomeDir is const */

    char folder[RTPATH_MAX];
    int vrc = RTPathAbsEx(mData.mHomeDir.c_str(), strPath.c_str(), folder, sizeof(folder));
    if (RT_SUCCESS(vrc))
        aResult = folder;

    return vrc;
}

/**
 * Tries to calculate the relative path of the given absolute path using the
 * directory of the VirtualBox settings file as the base directory.
 *
 * @param  aPath    Absolute path to calculate the relative path for.
 * @param  aResult  Where to put the result (used only when it's possible to
 *                  make a relative path from the given absolute path; otherwise
 *                  left untouched).
 *
 * @note Doesn't lock any object.
 */
void VirtualBox::calculateRelativePath(const Utf8Str &strPath, Utf8Str &aResult)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* no need to lock since mHomeDir is const */

    Utf8Str settingsDir = mData.mHomeDir;

    if (RTPathStartsWith(strPath.c_str(), settingsDir.c_str()))
    {
        /* when assigning, we create a separate Utf8Str instance because both
         * aPath and aResult can point to the same memory location when this
         * func is called (if we just do aResult = aPath, aResult will be freed
         * first, and since its the same as aPath, an attempt to copy garbage
         * will be made. */
        aResult = Utf8Str(strPath.c_str() + settingsDir.length() + 1);
    }
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Checks if there is a hard disk, DVD or floppy image with the given ID or
 * location already registered.
 *
 * On return, sets @aConflict to the string describing the conflicting medium,
 * or sets it to @c Null if no conflicting media is found. Returns S_OK in
 * either case. A failure is unexpected.
 *
 * @param aId           UUID to check.
 * @param aLocation     Location to check.
 * @param aConflict     Where to return parameters of the conflicting medium.
 *
 * @note Locks this object and media objects for reading.
 */
HRESULT VirtualBox::checkMediaForConflicts2 (const Guid &aId,
                                             const Bstr &aLocation,
                                             Utf8Str &aConflict)
{
    aConflict.setNull();

    AssertReturn(!aId.isEmpty() && !aLocation.isEmpty(), E_FAIL);

    AutoReadLock alock(this);

    HRESULT rc = S_OK;

    {
        ComObjPtr<HardDisk> hardDisk;
        rc = findHardDisk(&aId, aLocation, false /* aSetError */, &hardDisk);
        if (SUCCEEDED(rc))
        {
            /* Note: no AutoCaller since bound to this */
            AutoReadLock mediaLock (hardDisk);
            aConflict = Utf8StrFmt (
                tr ("hard disk '%ls' with UUID {%RTuuid}"),
                hardDisk->locationFull().raw(), hardDisk->id().raw());
            return S_OK;
        }
    }

    {
        ComObjPtr<DVDImage> image;
        rc = findDVDImage (&aId, aLocation, false /* aSetError */, &image);
        if (SUCCEEDED(rc))
        {
            /* Note: no AutoCaller since bound to this */
            AutoReadLock mediaLock (image);
            aConflict = Utf8StrFmt (
                tr ("CD/DVD image '%ls' with UUID {%RTuuid}"),
                image->locationFull().raw(), image->id().raw());
            return S_OK;
        }
    }

    {
        ComObjPtr<FloppyImage> image;
        rc = findFloppyImage(&aId, aLocation, false /* aSetError */, &image);
        if (SUCCEEDED(rc))
        {
            /* Note: no AutoCaller since bound to this */
            AutoReadLock mediaLock (image);
            aConflict = Utf8StrFmt (
                tr ("floppy image '%ls' with UUID {%RTuuid}"),
                image->locationFull().raw(), image->id().raw());
            return S_OK;
        }
    }

    return S_OK;
}

/**
 *  Helper function to write out the configuration tree.
 *
 *  @note Locks this object for writing and child objects for reading/writing!
 */
HRESULT VirtualBox::saveSettings()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AssertReturn(!m_strSettingsFilePath.isEmpty(), E_FAIL);

    HRESULT rc = S_OK;

    /* serialize file access (prevents concurrent reads and writes) */
    AutoWriteLock alock(this);

    try
    {
        // machines
        m_pMainConfigFile->llMachines.clear();
        for (MachineList::iterator it = mData.mMachines.begin();
             it != mData.mMachines.end();
             ++it)
        {
            settings::MachineRegistryEntry mre;
            rc = (*it)->saveRegistryEntry(mre);
            m_pMainConfigFile->llMachines.push_back(mre);
        }

        // hard disks
        m_pMainConfigFile->llHardDisks.clear();
        for (HardDiskList::const_iterator it = mData.mHardDisks.begin();
             it != mData.mHardDisks.end();
             ++it)
        {
            settings::Medium m;
            rc = (*it)->saveSettings(m);
            m_pMainConfigFile->llHardDisks.push_back(m);
            CheckComRCThrowRC(rc);
        }

        /* CD/DVD images */
        m_pMainConfigFile->llDvdImages.clear();
        for (DVDImageList::const_iterator it = mData.mDVDImages.begin();
             it != mData.mDVDImages.end();
             ++it)
        {
            settings::Medium m;
            rc = (*it)->saveSettings(m);
            CheckComRCThrowRC(rc);
            m_pMainConfigFile->llDvdImages.push_back(m);
        }

        /* floppy images */
        m_pMainConfigFile->llFloppyImages.clear();
        for (FloppyImageList::const_iterator it = mData.mFloppyImages.begin();
             it != mData.mFloppyImages.end();
             ++it)
        {
            settings::Medium m;
            rc = (*it)->saveSettings(m);
            CheckComRCThrowRC(rc);
            m_pMainConfigFile->llFloppyImages.push_back(m);
        }

        m_pMainConfigFile->llDhcpServers.clear();
        for (DHCPServerList::const_iterator it =
                mData.mDHCPServers.begin();
                it != mData.mDHCPServers.end();
                ++ it)
        {
            settings::DHCPServer d;
            rc = (*it)->saveSettings(d);
            CheckComRCThrowRC(rc);
            m_pMainConfigFile->llDhcpServers.push_back(d);
        }

        /* host data (USB filters) */
        rc = mData.mHost->saveSettings(m_pMainConfigFile->host);
        CheckComRCThrowRC(rc);

        rc = mData.mSystemProperties->saveSettings(m_pMainConfigFile->systemProperties);
        CheckComRCThrowRC(rc);

        // now write out the XML
        m_pMainConfigFile->write(m_strSettingsFilePath);
    }
    catch (HRESULT err)
    {
        /* we assume that error info is set by the thrower */
        rc = err;
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions (RT_SRC_POS);
    }

    return rc;
}

/**
 *  Helper to register the machine.
 *
 *  When called during VirtualBox startup, adds the given machine to the
 *  collection of registered machines. Otherwise tries to mark the machine
 *  as registered, and, if succeeded, adds it to the collection and
 *  saves global settings.
 *
 *  @note The caller must have added itself as a caller of the @a aMachine
 *  object if calls this method not on VirtualBox startup.
 *
 *  @param aMachine     machine to register
 *
 *  @note Locks objects!
 */
HRESULT VirtualBox::registerMachine (Machine *aMachine)
{
    ComAssertRet (aMachine, E_INVALIDARG);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = S_OK;

    {
        ComObjPtr<Machine> m;
        rc = findMachine (aMachine->id(), false /* aDoSetError */, &m);
        if (SUCCEEDED(rc))
        {
            /* sanity */
            AutoLimitedCaller machCaller (m);
            AssertComRC (machCaller.rc());

            return setError (E_INVALIDARG,
                tr ("Registered machine with UUID {%RTuuid} ('%ls') already exists"),
                aMachine->id().raw(), m->settingsFileFull().raw());
        }

        ComAssertRet (rc == VBOX_E_OBJECT_NOT_FOUND, rc);
        rc = S_OK;
    }

    if (autoCaller.state() != InInit)
    {
        /* Machine::trySetRegistered() will commit and save machine settings */
        rc = aMachine->trySetRegistered (TRUE);
        CheckComRCReturnRC(rc);
    }

    /* add to the collection of registered machines */
    mData.mMachines.push_back(aMachine);

    if (autoCaller.state() != InInit)
        rc = saveSettings();

    return rc;
}

/**
 * Remembers the given hard disk by storing it in the hard disk registry.
 *
 * @param aHardDisk     Hard disk object to remember.
 * @param aSaveRegistry @c true to save hard disk registry to disk (default).
 *
 * When @a aSaveRegistry is @c true, this operation may fail because of the
 * failed #saveSettings() method it calls. In this case, the hard disk object
 * will not be remembered. It is therefore the responsibility of the caller to
 * call this method as the last step of some action that requires registration
 * in order to make sure that only fully functional hard disk objects get
 * registered.
 *
 * @note Locks this object for writing and @a aHardDisk for reading.
 */
HRESULT VirtualBox::registerHardDisk(HardDisk *aHardDisk,
                                     bool aSaveRegistry /*= true*/)
{
    AssertReturn(aHardDisk != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AutoCaller hardDiskCaller (aHardDisk);
    AssertComRCReturn (hardDiskCaller.rc(), hardDiskCaller.rc());

    AutoReadLock hardDiskLock (aHardDisk);

    Utf8Str strConflict;
    HRESULT rc = checkMediaForConflicts2(aHardDisk->id(),
                                         aHardDisk->locationFull(),
                                         strConflict);
    CheckComRCReturnRC(rc);

    if (strConflict.length())
    {
        return setError(E_INVALIDARG,
                        tr("Cannot register the hard disk '%ls' with UUID {%RTuuid} because a %s already exists in the media registry ('%s')"),
                        aHardDisk->locationFull().raw(),
                        aHardDisk->id().raw(),
                        strConflict.raw(),
                        m_strSettingsFilePath.raw());
    }

    if (aHardDisk->parent().isNull())
    {
        /* base (root) hard disk */
        mData.mHardDisks.push_back (aHardDisk);
    }

    mData.mHardDiskMap
        .insert (HardDiskMap::value_type (
            aHardDisk->id(), HardDiskMap::mapped_type (aHardDisk)));

    if (aSaveRegistry)
    {
        rc = saveSettings();
        if (FAILED (rc))
            unregisterHardDisk(aHardDisk, false /* aSaveRegistry */);
    }

    return rc;
}

/**
 * Removes the given hard disk from the hard disk registry.
 *
 * @param aHardDisk     Hard disk object to remove.
 * @param aSaveRegistry @c true to save hard disk registry to disk (default).
 *
 * When @a aSaveRegistry is @c true, this operation may fail because of the
 * failed #saveSettings() method it calls. In this case, the hard disk object
 * will NOT be removed from the registry when this method returns. It is
 * therefore the responsibility of the caller to call this method as the first
 * step of some action that requires unregistration, before calling uninit() on
 * @a aHardDisk.
 *
 * @note Locks this object for writing and @a aHardDisk for reading.
 */
HRESULT VirtualBox::unregisterHardDisk(HardDisk *aHardDisk,
                                       bool aSaveRegistry /*= true*/)
{
    AssertReturn(aHardDisk != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AutoCaller hardDiskCaller (aHardDisk);
    AssertComRCReturn (hardDiskCaller.rc(), hardDiskCaller.rc());

    AutoReadLock hardDiskLock (aHardDisk);

    size_t cnt = mData.mHardDiskMap.erase (aHardDisk->id());
    Assert (cnt == 1);
    NOREF(cnt);

    if (aHardDisk->parent().isNull())
    {
        /* base (root) hard disk */
        mData.mHardDisks.remove (aHardDisk);
    }

    HRESULT rc = S_OK;

    if (aSaveRegistry)
    {
        rc = saveSettings();
        if (FAILED (rc))
            registerHardDisk(aHardDisk, false /* aSaveRegistry */);
    }

    return rc;
}

/**
 * Remembers the given image by storing it in the CD/DVD image registry.
 *
 * @param aImage        Image object to remember.
 * @param aSaveRegistry @c true to save the image registry to disk (default).
 *
 * When @a aSaveRegistry is @c true, this operation may fail because of the
 * failed #saveSettings() method it calls. In this case, the image object
 * will not be remembered. It is therefore the responsibility of the caller to
 * call this method as the last step of some action that requires registration
 * in order to make sure that only fully functional image objects get
 * registered.
 *
 * @note Locks this object for writing and @a aImage for reading.
 */
HRESULT VirtualBox::registerDVDImage (DVDImage *aImage,
                                      bool aSaveRegistry /*= true*/)
{
    AssertReturn(aImage != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AutoCaller imageCaller (aImage);
    AssertComRCReturn (imageCaller.rc(), imageCaller.rc());

    AutoReadLock imageLock (aImage);

    Utf8Str strConflict;
    HRESULT rc = checkMediaForConflicts2(aImage->id(),
                                         aImage->locationFull(),
                                         strConflict);
    CheckComRCReturnRC(rc);

    if (strConflict.length())
    {
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("Cannot register the CD/DVD image '%ls' with UUID {%RTuuid} because a %s already exists in the media registry ('%s')"),
                        aImage->locationFull().raw(),
                        aImage->id().raw(),
                        strConflict.raw(),
                        m_strSettingsFilePath.raw());
    }

    /* add to the collection */
    mData.mDVDImages.push_back (aImage);

    if (aSaveRegistry)
    {
        rc = saveSettings();
        if (FAILED (rc))
            unregisterDVDImage (aImage, false /* aSaveRegistry */);
    }

    return rc;
}

/**
 * Removes the given image from the CD/DVD image registry registry.
 *
 * @param aImage        Image object to remove.
 * @param aSaveRegistry @c true to save hard disk registry to disk (default).
 *
 * When @a aSaveRegistry is @c true, this operation may fail because of the
 * failed #saveSettings() method it calls. In this case, the image object
 * will NOT be removed from the registry when this method returns. It is
 * therefore the responsibility of the caller to call this method as the first
 * step of some action that requires unregistration, before calling uninit() on
 * @a aImage.
 *
 * @note Locks this object for writing and @a aImage for reading.
 */
HRESULT VirtualBox::unregisterDVDImage (DVDImage *aImage,
                                        bool aSaveRegistry /*= true*/)
{
    AssertReturn(aImage != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AutoCaller imageCaller (aImage);
    AssertComRCReturn (imageCaller.rc(), imageCaller.rc());

    AutoReadLock imageLock (aImage);

    mData.mDVDImages.remove (aImage);

    HRESULT rc = S_OK;

    if (aSaveRegistry)
    {
        rc = saveSettings();
        if (FAILED (rc))
            registerDVDImage (aImage, false /* aSaveRegistry */);
    }

    return rc;
}

/**
 * Remembers the given image by storing it in the floppy image registry.
 *
 * @param aImage        Image object to remember.
 * @param aSaveRegistry @c true to save the image registry to disk (default).
 *
 * When @a aSaveRegistry is @c true, this operation may fail because of the
 * failed #saveSettings() method it calls. In this case, the image object
 * will not be remembered. It is therefore the responsibility of the caller to
 * call this method as the last step of some action that requires registration
 * in order to make sure that only fully functional image objects get
 * registered.
 *
 * @note Locks this object for writing and @a aImage for reading.
 */
HRESULT VirtualBox::registerFloppyImage(FloppyImage *aImage,
                                        bool aSaveRegistry /*= true*/)
{
    AssertReturn(aImage != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AutoCaller imageCaller (aImage);
    AssertComRCReturn (imageCaller.rc(), imageCaller.rc());

    AutoReadLock imageLock (aImage);

    Utf8Str strConflict;
    HRESULT rc = checkMediaForConflicts2(aImage->id(),
                                         aImage->locationFull(),
                                         strConflict);
    CheckComRCReturnRC(rc);

    if (strConflict.length())
    {
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("Cannot register the floppy image '%ls' with UUID {%RTuuid} because a %s already exists in the media registry ('%s')"),
                        aImage->locationFull().raw(),
                        aImage->id().raw(),
                        strConflict.raw(),
                        m_strSettingsFilePath.raw());
    }

    /* add to the collection */
    mData.mFloppyImages.push_back (aImage);

    if (aSaveRegistry)
    {
        rc = saveSettings();
        if (FAILED (rc))
            unregisterFloppyImage(aImage, false /* aSaveRegistry */);
    }

    return rc;
}

/**
 * Removes the given image from the floppy image registry registry.
 *
 * @param aImage        Image object to remove.
 * @param aSaveRegistry @c true to save hard disk registry to disk (default).
 *
 * When @a aSaveRegistry is @c true, this operation may fail because of the
 * failed #saveSettings() method it calls. In this case, the image object
 * will NOT be removed from the registry when this method returns. It is
 * therefore the responsibility of the caller to call this method as the first
 * step of some action that requires unregistration, before calling uninit() on
 * @a aImage.
 *
 * @note Locks this object for writing and @a aImage for reading.
 */
HRESULT VirtualBox::unregisterFloppyImage(FloppyImage *aImage,
                                          bool aSaveRegistry /*= true*/)
{
    AssertReturn(aImage != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AutoCaller imageCaller (aImage);
    AssertComRCReturn (imageCaller.rc(), imageCaller.rc());

    AutoReadLock imageLock (aImage);

    mData.mFloppyImages.remove (aImage);

    HRESULT rc = S_OK;

    if (aSaveRegistry)
    {
        rc = saveSettings();
        if (FAILED (rc))
            registerFloppyImage (aImage, false /* aSaveRegistry */);
    }

    return rc;
}

/**
 * Attempts to cast from a raw interface pointer to an underlying object.
 * On success, @a aTo will contain the object reference. On failure, @a aTo will
 * be set to @c null and an extended error info will be returned.
 *
 * @param aFrom     Interface pointer to cast from.
 * @param aTo       Where to store a reference to the underlying object.
 *
 * @note Locks #childrenLock() for reading.
 */
HRESULT VirtualBox::cast (IHardDisk *aFrom, ComObjPtr<HardDisk> &aTo)
{
    AssertReturn(aFrom != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* We need the children map lock here to keep the getDependentChild() result
     * valid until we finish */
    AutoReadLock chLock (childrenLock());

    VirtualBoxBase *child = getDependentChild (aFrom);
    if (!child)
        return setError (E_FAIL, tr ("The given hard disk object is not created "
                                     "within this VirtualBox instance"));

    /* we can safely cast child to HardDisk * here because only HardDisk
     * implementations of IHardDisk can be among our children */

    aTo = static_cast<HardDisk*>(child);

    return S_OK;
}

/**
 * Helper to update the global settings file when the name of some machine
 * changes so that file and directory renaming occurs. This method ensures that
 * all affected paths in the disk registry are properly updated.
 *
 * @param aOldPath  Old path (full).
 * @param aNewPath  New path (full).
 *
 * @note Locks this object + DVD, Floppy and HardDisk children for writing.
 */
HRESULT VirtualBox::updateSettings (const char *aOldPath, const char *aNewPath)
{
    LogFlowThisFunc(("aOldPath={%s} aNewPath={%s}\n", aOldPath, aNewPath));

    AssertReturn(aOldPath, E_INVALIDARG);
    AssertReturn(aNewPath, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    /* check DVD paths */
    for (DVDImageList::iterator it = mData.mDVDImages.begin();
         it != mData.mDVDImages.end();
         ++ it)
    {
        (*it)->updatePath (aOldPath, aNewPath);
    }

    /* check Floppy paths */
    for (FloppyImageList::iterator it = mData.mFloppyImages.begin();
         it != mData.mFloppyImages  .end();
         ++ it)
    {
        (*it)->updatePath (aOldPath, aNewPath);
    }

    /* check HardDisk paths */
    for (HardDiskList::const_iterator it = mData.mHardDisks.begin();
         it != mData.mHardDisks.end();
         ++ it)
    {
        (*it)->updatePaths (aOldPath, aNewPath);
    }

    HRESULT rc = saveSettings();

    return rc;
}

/**
 * Creates the path to the specified file according to the path information
 * present in the file name.
 *
 * Note that the given file name must contain the full path otherwise the
 * extracted relative path will be created based on the current working
 * directory which is normally unknown.
 *
 * @param aFileName     Full file name which path needs to be created.
 *
 * @return Extended error information on failure to create the path.
 */
/* static */
HRESULT VirtualBox::ensureFilePathExists(const Utf8Str &strFileName)
{
    Utf8Str strDir(strFileName);
    strDir.stripFilename();
    if (!RTDirExists(strDir.c_str()))
    {
        int vrc = RTDirCreateFullPath(strDir.c_str(), 0777);
        if (RT_FAILURE(vrc))
            return setError(E_FAIL,
                            tr("Could not create the directory '%s' (%Rrc)"),
                            strDir.c_str(),
                            vrc);
    }

    return S_OK;
}

/**
 * Handles unexpected exceptions by turning them into COM errors in release
 * builds or by hitting a breakpoint in the release builds.
 *
 * Usage pattern:
 * @code
        try
        {
            // ...
        }
        catch (LaLalA)
        {
            // ...
        }
        catch (...)
        {
            rc = VirtualBox::handleUnexpectedExceptions (RT_SRC_POS);
        }
 * @endcode
 *
 * @param RT_SRC_POS_DECL "RT_SRC_POS" macro instantiation.
 */
/* static */
HRESULT VirtualBox::handleUnexpectedExceptions(RT_SRC_POS_DECL)
{
    try
    {
        /* re-throw the current exception */
        throw;
    }
    catch (const xml::Error &err)
    {
        return setError(E_FAIL, tr("%s.\n%s[%d] (%s)"),
                                err.what(),
                                pszFile, iLine, pszFunction);
    }
    catch (const std::exception &err)
    {
        return setError(E_FAIL, tr("Unexpected exception: %s [%s]\n%s[%d] (%s)"),
                                err.what(), typeid(err).name(),
                                pszFile, iLine, pszFunction);
    }
    catch (...)
    {
        return setError(E_FAIL, tr("Unknown exception\n%s[%d] (%s)"),
                                pszFile, iLine, pszFunction);
    }

    /* should not get here */
    AssertFailed();
    return E_FAIL;
}

/**
 *  Thread function that watches the termination of all client processes
 *  that have opened sessions using IVirtualBox::OpenSession()
 */
// static
DECLCALLBACK(int) VirtualBox::ClientWatcher (RTTHREAD /* thread */, void *pvUser)
{
    LogFlowFuncEnter();

    VirtualBox *that = (VirtualBox *) pvUser;
    Assert (that);

    SessionMachineVector machines;
    MachineVector spawnedMachines;

    size_t cnt = 0;
    size_t cntSpawned = 0;

#if defined(RT_OS_WINDOWS)

    HRESULT hrc = CoInitializeEx (NULL,
                                  COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE |
                                  COINIT_SPEED_OVER_MEMORY);
    AssertComRC (hrc);

    /// @todo (dmik) processes reaping!

    HANDLE handles [MAXIMUM_WAIT_OBJECTS];
    handles [0] = that->mWatcherData.mUpdateReq;

    do
    {
        AutoCaller autoCaller(that);
        /* VirtualBox has been early uninitialized, terminate */
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            DWORD rc = ::WaitForMultipleObjects ((DWORD)(1 + cnt + cntSpawned),
                                                 handles, FALSE, INFINITE);

            /* Restore the caller before using VirtualBox. If it fails, this
             * means VirtualBox is being uninitialized and we must terminate. */
            autoCaller.add();
            if (!autoCaller.isOk())
                break;

            bool update = false;

            if (rc == WAIT_OBJECT_0)
            {
                /* update event is signaled */
                update = true;
            }
            else if (rc > WAIT_OBJECT_0 && rc <= (WAIT_OBJECT_0 + cnt))
            {
                /* machine mutex is released */
                (machines [rc - WAIT_OBJECT_0 - 1])->checkForDeath();
                update = true;
            }
            else if (rc > WAIT_ABANDONED_0 && rc <= (WAIT_ABANDONED_0 + cnt))
            {
                /* machine mutex is abandoned due to client process termination */
                (machines [rc - WAIT_ABANDONED_0 - 1])->checkForDeath();
                update = true;
            }
            else if (rc > WAIT_OBJECT_0 + cnt && rc <= (WAIT_OBJECT_0 + cntSpawned))
            {
                /* spawned VM process has terminated (normally or abnormally) */
                (spawnedMachines [rc - WAIT_OBJECT_0 - cnt - 1])->
                    checkForSpawnFailure();
                update = true;
            }

            if (update)
            {
                /* close old process handles */
                for (size_t i = 1 + cnt; i < 1 + cnt + cntSpawned; ++ i)
                    CloseHandle (handles [i]);

                AutoReadLock thatLock (that);

                /* obtain a new set of opened machines */
                cnt = 0;
                machines.clear();

                for (MachineList::iterator it = that->mData.mMachines.begin();
                     it != that->mData.mMachines.end(); ++ it)
                {
                    /// @todo handle situations with more than 64 objects
                    AssertMsgBreak ((1 + cnt) <= MAXIMUM_WAIT_OBJECTS,
                                    ("MAXIMUM_WAIT_OBJECTS reached"));

                    ComObjPtr<SessionMachine> sm;
                    HANDLE ipcSem;
                    if ((*it)->isSessionOpenOrClosing (sm, NULL, &ipcSem))
                    {
                        machines.push_back (sm);
                        handles [1 + cnt] = ipcSem;
                        ++ cnt;
                    }
                }

                LogFlowFunc (("UPDATE: direct session count = %d\n", cnt));

                /* obtain a new set of spawned machines */
                cntSpawned = 0;
                spawnedMachines.clear();

                for (MachineList::iterator it = that->mData.mMachines.begin();
                     it != that->mData.mMachines.end(); ++ it)
                {
                    /// @todo handle situations with more than 64 objects
                    AssertMsgBreak ((1 + cnt + cntSpawned) <= MAXIMUM_WAIT_OBJECTS,
                                    ("MAXIMUM_WAIT_OBJECTS reached"));

                    RTPROCESS pid;
                    if ((*it)->isSessionSpawning (&pid))
                    {
                        HANDLE ph = OpenProcess (SYNCHRONIZE, FALSE, pid);
                        AssertMsg (ph != NULL, ("OpenProcess (pid=%d) failed with %d\n",
                                                pid, GetLastError()));
                        if (rc == 0)
                        {
                            spawnedMachines.push_back (*it);
                            handles [1 + cnt + cntSpawned] = ph;
                            ++ cntSpawned;
                        }
                    }
                }

                LogFlowFunc (("UPDATE: spawned session count = %d\n", cntSpawned));
            }
        }
        while (true);
    }
    while (0);

    /* close old process handles */
    for (size_t i = 1 + cnt; i < 1 + cnt + cntSpawned; ++ i)
        CloseHandle (handles [i]);

    /* release sets of machines if any */
    machines.clear();
    spawnedMachines.clear();

    ::CoUninitialize();

#elif defined (RT_OS_OS2)

    /// @todo (dmik) processes reaping!

    /* according to PMREF, 64 is the maximum for the muxwait list */
    SEMRECORD handles [64];

    HMUX muxSem = NULLHANDLE;

    do
    {
        AutoCaller autoCaller(that);
        /* VirtualBox has been early uninitialized, terminate */
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            int vrc = RTSemEventWait (that->mWatcherData.mUpdateReq, 500);

            /* Restore the caller before using VirtualBox. If it fails, this
             * means VirtualBox is being uninitialized and we must terminate. */
            autoCaller.add();
            if (!autoCaller.isOk())
                break;

            bool update = false;
            bool updateSpawned = false;

            if (RT_SUCCESS(vrc))
            {
                /* update event is signaled */
                update = true;
                updateSpawned = true;
            }
            else
            {
                AssertMsg (vrc == VERR_TIMEOUT || vrc == VERR_INTERRUPTED,
                           ("RTSemEventWait returned %Rrc\n", vrc));

                /* are there any mutexes? */
                if (cnt > 0)
                {
                    /* figure out what's going on with machines */

                    unsigned long semId = 0;
                    APIRET arc = ::DosWaitMuxWaitSem (muxSem,
                                                      SEM_IMMEDIATE_RETURN, &semId);

                    if (arc == NO_ERROR)
                    {
                        /* machine mutex is normally released */
                        Assert (semId >= 0 && semId < cnt);
                        if (semId >= 0 && semId < cnt)
                        {
#ifdef DEBUG
                            {
                                AutoReadLock machineLock (machines [semId]);
                                LogFlowFunc (("released mutex: machine='%ls'\n",
                                              machines [semId]->name().raw()));
                            }
#endif
                            machines [semId]->checkForDeath();
                        }
                        update = true;
                    }
                    else if (arc == ERROR_SEM_OWNER_DIED)
                    {
                        /* machine mutex is abandoned due to client process
                         * termination; find which mutex is in the Owner Died
                         * state */
                        for (size_t i = 0; i < cnt; ++ i)
                        {
                            PID pid; TID tid;
                            unsigned long reqCnt;
                            arc = DosQueryMutexSem ((HMTX) handles [i].hsemCur, &pid,
                                                    &tid, &reqCnt);
                            if (arc == ERROR_SEM_OWNER_DIED)
                            {
                                /* close the dead mutex as asked by PMREF */
                                ::DosCloseMutexSem ((HMTX) handles [i].hsemCur);

                                Assert (i >= 0 && i < cnt);
                                if (i >= 0 && i < cnt)
                                {
#ifdef DEBUG
                                    {
                                        AutoReadLock machineLock (machines [semId]);
                                        LogFlowFunc (("mutex owner dead: machine='%ls'\n",
                                                      machines [i]->name().raw()));
                                    }
#endif
                                    machines [i]->checkForDeath();
                                }
                            }
                        }
                        update = true;
                    }
                    else
                        AssertMsg (arc == ERROR_INTERRUPT || arc == ERROR_TIMEOUT,
                                   ("DosWaitMuxWaitSem returned %d\n", arc));
                }

                /* are there any spawning sessions? */
                if (cntSpawned > 0)
                {
                    for (size_t i = 0; i < cntSpawned; ++ i)
                        updateSpawned |= (spawnedMachines [i])->
                            checkForSpawnFailure();
                }
            }

            if (update || updateSpawned)
            {
                AutoReadLock thatLock (that);

                if (update)
                {
                    /* close the old muxsem */
                    if (muxSem != NULLHANDLE)
                        ::DosCloseMuxWaitSem (muxSem);

                    /* obtain a new set of opened machines */
                    cnt = 0;
                    machines.clear();

                    for (MachineList::iterator it = that->mData.mMachines.begin();
                         it != that->mData.mMachines.end(); ++ it)
                    {
                        /// @todo handle situations with more than 64 objects
                        AssertMsg (cnt <= 64 /* according to PMREF */,
                                   ("maximum of 64 mutex semaphores reached (%d)",
                                    cnt));

                        ComObjPtr<SessionMachine> sm;
                        HMTX ipcSem;
                        if ((*it)->isSessionOpenOrClosing (sm, NULL, &ipcSem))
                        {
                            machines.push_back (sm);
                            handles [cnt].hsemCur = (HSEM) ipcSem;
                            handles [cnt].ulUser = cnt;
                            ++ cnt;
                        }
                    }

                    LogFlowFunc (("UPDATE: direct session count = %d\n", cnt));

                    if (cnt > 0)
                    {
                        /* create a new muxsem */
                        APIRET arc = ::DosCreateMuxWaitSem (NULL, &muxSem, cnt,
                                                            handles,
                                                            DCMW_WAIT_ANY);
                        AssertMsg (arc == NO_ERROR,
                                   ("DosCreateMuxWaitSem returned %d\n", arc));
                        NOREF(arc);
                    }
                }

                if (updateSpawned)
                {
                    /* obtain a new set of spawned machines */
                    spawnedMachines.clear();

                    for (MachineList::iterator it = that->mData.mMachines.begin();
                         it != that->mData.mMachines.end(); ++ it)
                    {
                        if ((*it)->isSessionSpawning())
                            spawnedMachines.push_back (*it);
                    }

                    cntSpawned = spawnedMachines.size();
                    LogFlowFunc (("UPDATE: spawned session count = %d\n", cntSpawned));
                }
            }
        }
        while (true);
    }
    while (0);

    /* close the muxsem */
    if (muxSem != NULLHANDLE)
        ::DosCloseMuxWaitSem (muxSem);

    /* release sets of machines if any */
    machines.clear();
    spawnedMachines.clear();

#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)

    bool update = false;
    bool updateSpawned = false;

    do
    {
        AutoCaller autoCaller(that);
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            int rc = RTSemEventWait (that->mWatcherData.mUpdateReq, 500);

            /*
             *  Restore the caller before using VirtualBox. If it fails, this
             *  means VirtualBox is being uninitialized and we must terminate.
             */
            autoCaller.add();
            if (!autoCaller.isOk())
                break;

            if (RT_SUCCESS(rc) || update || updateSpawned)
            {
                /* RT_SUCCESS(rc) means an update event is signaled */

                AutoReadLock thatLock (that);

                if (RT_SUCCESS(rc) || update)
                {
                    /* obtain a new set of opened machines */
                    machines.clear();

                    for (MachineList::iterator it = that->mData.mMachines.begin();
                         it != that->mData.mMachines.end(); ++ it)
                    {
                        ComObjPtr<SessionMachine> sm;
                        if ((*it)->isSessionOpenOrClosing (sm))
                            machines.push_back (sm);
                    }

                    cnt = machines.size();
                    LogFlowFunc (("UPDATE: direct session count = %d\n", cnt));
                }

                if (RT_SUCCESS(rc) || updateSpawned)
                {
                    /* obtain a new set of spawned machines */
                    spawnedMachines.clear();

                    for (MachineList::iterator it = that->mData.mMachines.begin();
                         it != that->mData.mMachines.end(); ++ it)
                    {
                        if ((*it)->isSessionSpawning())
                            spawnedMachines.push_back (*it);
                    }

                    cntSpawned = spawnedMachines.size();
                    LogFlowFunc (("UPDATE: spawned session count = %d\n", cntSpawned));
                }
            }

            update = false;
            for (size_t i = 0; i < cnt; ++ i)
                update |= (machines [i])->checkForDeath();

            updateSpawned = false;
            for (size_t i = 0; i < cntSpawned; ++ i)
                updateSpawned |= (spawnedMachines [i])->checkForSpawnFailure();

            /* reap child processes */
            {
                AutoWriteLock alock(that);
                if (that->mWatcherData.mProcesses.size())
                {
                    LogFlowFunc (("UPDATE: child process count = %d\n",
                                  that->mWatcherData.mProcesses.size()));
                    ClientWatcherData::ProcessList::iterator it =
                        that->mWatcherData.mProcesses.begin();
                    while (it != that->mWatcherData.mProcesses.end())
                    {
                        RTPROCESS pid = *it;
                        RTPROCSTATUS status;
                        int vrc = ::RTProcWait (pid, RTPROCWAIT_FLAGS_NOBLOCK,
                                                &status);
                        if (vrc == VINF_SUCCESS)
                        {
                            LogFlowFunc (("pid %d (%x) was reaped, "
                                          "status=%d, reason=%d\n",
                                          pid, pid, status.iStatus,
                                          status.enmReason));
                            it = that->mWatcherData.mProcesses.erase (it);
                        }
                        else
                        {
                            LogFlowFunc (("pid %d (%x) was NOT reaped, vrc=%Rrc\n",
                                          pid, pid, vrc));
                            if (vrc != VERR_PROCESS_RUNNING)
                            {
                                /* remove the process if it is not already running */
                                it = that->mWatcherData.mProcesses.erase (it);
                            }
                            else
                                ++ it;
                        }
                    }
                }
            }
        }
        while (true);
    }
    while (0);

    /* release sets of machines if any */
    machines.clear();
    spawnedMachines.clear();

#else
# error "Port me!"
#endif

    LogFlowFuncLeave();
    return 0;
}

/**
 *  Thread function that handles custom events posted using #postEvent().
 */
// static
DECLCALLBACK(int) VirtualBox::AsyncEventHandler (RTTHREAD thread, void *pvUser)
{
    LogFlowFuncEnter();

    AssertReturn(pvUser, VERR_INVALID_POINTER);

    // create an event queue for the current thread
    EventQueue *eventQ = new EventQueue();
    AssertReturn(eventQ, VERR_NO_MEMORY);

    // return the queue to the one who created this thread
    *(static_cast <EventQueue **> (pvUser)) = eventQ;
    // signal that we're ready
    RTThreadUserSignal (thread);

    BOOL ok = TRUE;
    Event *event = NULL;

    while ((ok = eventQ->waitForEvent (&event)) && event)
        eventQ->handleEvent (event);

    AssertReturn(ok, VERR_GENERAL_FAILURE);

    delete eventQ;

    LogFlowFuncLeave();

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

/**
 *  Takes the current list of registered callbacks of the managed VirtualBox
 *  instance, and calls #handleCallback() for every callback item from the
 *  list, passing the item as an argument.
 *
 *  @note Locks the managed VirtualBox object for reading but leaves the lock
 *        before iterating over callbacks and calling their methods.
 */
void *VirtualBox::CallbackEvent::handler()
{
    if (mVirtualBox.isNull())
        return NULL;

    AutoCaller autoCaller(mVirtualBox);
    if (!autoCaller.isOk())
    {
        LogWarningFunc (("VirtualBox has been uninitialized (state=%d), "
                         "the callback event is discarded!\n",
                         autoCaller.state()));
        /* We don't need mVirtualBox any more, so release it */
        mVirtualBox.setNull();
        return NULL;
    }

    CallbackVector callbacks;
    {
        /* Make a copy to release the lock before iterating */
        AutoReadLock alock(mVirtualBox);
        callbacks = CallbackVector (mVirtualBox->mData.mCallbacks.begin(),
                                    mVirtualBox->mData.mCallbacks.end());
        /* We don't need mVirtualBox any more, so release it */
        mVirtualBox.setNull();
    }

    for (VirtualBox::CallbackVector::const_iterator it = callbacks.begin();
         it != callbacks.end(); ++ it)
        handleCallback (*it);

    return NULL;
}

//STDMETHODIMP VirtualBox::CreateDHCPServerForInterface (/*IHostNetworkInterface * aIinterface,*/ IDHCPServer ** aServer)
//{
//    return E_NOTIMPL;
//}

STDMETHODIMP VirtualBox::CreateDHCPServer (IN_BSTR aName, IDHCPServer ** aServer)
{
    CheckComArgNotNull(aName);
    CheckComArgNotNull(aServer);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    ComObjPtr<DHCPServer> dhcpServer;
    dhcpServer.createObject();
    HRESULT rc = dhcpServer->init (this, aName);
    CheckComRCReturnRC(rc);

    rc = registerDHCPServer(dhcpServer, true);
    CheckComRCReturnRC(rc);

    dhcpServer.queryInterfaceTo(aServer);

    return rc;
}

//STDMETHODIMP VirtualBox::FindDHCPServerForInterface (IHostNetworkInterface * aIinterface, IDHCPServer ** aServer)
//{
//    return E_NOTIMPL;
//}

STDMETHODIMP VirtualBox::FindDHCPServerByNetworkName (IN_BSTR aName, IDHCPServer ** aServer)
{
    CheckComArgNotNull(aName);
    CheckComArgNotNull(aServer);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc;
    Bstr bstr;
    ComPtr<DHCPServer> found;

    for (DHCPServerList::const_iterator it =
            mData.mDHCPServers.begin();
         it != mData.mDHCPServers.end();
         ++ it)
    {
        rc = (*it)->COMGETTER(NetworkName) (bstr.asOutParam());
        CheckComRCThrowRC(rc);

        if(bstr == aName)
        {
            found = *it;
            break;
        }
    }

    if (!found)
        return E_INVALIDARG;

    return found.queryInterfaceTo(aServer);
}

STDMETHODIMP VirtualBox::RemoveDHCPServer (IDHCPServer * aServer)
{
    CheckComArgNotNull(aServer);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    HRESULT rc = unregisterDHCPServer(static_cast<DHCPServer *>(aServer), true);

    return rc;
}

/**
 * Remembers the given dhcp server by storing it in the hard disk registry.
 *
 * @param aDHCPServer     Dhcp Server object to remember.
 * @param aSaveRegistry @c true to save hard disk registry to disk (default).
 *
 * When @a aSaveRegistry is @c true, this operation may fail because of the
 * failed #saveSettings() method it calls. In this case, the dhcp server object
 * will not be remembered. It is therefore the responsibility of the caller to
 * call this method as the last step of some action that requires registration
 * in order to make sure that only fully functional dhcp server objects get
 * registered.
 *
 * @note Locks this object for writing and @a aDHCPServer for reading.
 */
HRESULT VirtualBox::registerDHCPServer(DHCPServer *aDHCPServer,
                                     bool aSaveRegistry /*= true*/)
{
    AssertReturn(aDHCPServer != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AutoCaller dhcpServerCaller (aDHCPServer);
    AssertComRCReturn (dhcpServerCaller.rc(), dhcpServerCaller.rc());

    AutoReadLock dhcpServerLock (aDHCPServer);

    Bstr name;
    HRESULT rc;
    rc = aDHCPServer->COMGETTER(NetworkName) (name.asOutParam());
    CheckComRCReturnRC(rc);

    ComPtr<IDHCPServer> existing;
    rc = FindDHCPServerByNetworkName(name.mutableRaw(), existing.asOutParam());
    if(SUCCEEDED(rc))
    {
        return E_INVALIDARG;
    }
    rc = S_OK;

    mData.mDHCPServers.push_back (aDHCPServer);

    if (aSaveRegistry)
    {
        rc = saveSettings();
        if (FAILED (rc))
            unregisterDHCPServer(aDHCPServer, false /* aSaveRegistry */);
    }

    return rc;
}

/**
 * Removes the given hard disk from the hard disk registry.
 *
 * @param aHardDisk     Hard disk object to remove.
 * @param aSaveRegistry @c true to save hard disk registry to disk (default).
 *
 * When @a aSaveRegistry is @c true, this operation may fail because of the
 * failed #saveSettings() method it calls. In this case, the hard disk object
 * will NOT be removed from the registry when this method returns. It is
 * therefore the responsibility of the caller to call this method as the first
 * step of some action that requires unregistration, before calling uninit() on
 * @a aHardDisk.
 *
 * @note Locks this object for writing and @a aHardDisk for reading.
 */
HRESULT VirtualBox::unregisterDHCPServer(DHCPServer *aDHCPServer,
                                         bool aSaveRegistry /*= true*/)
{
    AssertReturn(aDHCPServer != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AutoCaller dhcpServerCaller (aDHCPServer);
    AssertComRCReturn (dhcpServerCaller.rc(), dhcpServerCaller.rc());

    AutoReadLock dhcpServerLock (aDHCPServer);

    mData.mDHCPServers.remove (aDHCPServer);

    HRESULT rc = S_OK;

    if (aSaveRegistry)
    {
        rc = saveSettings();
        if (FAILED (rc))
            registerDHCPServer(aDHCPServer, false /* aSaveRegistry */);
    }

    return rc;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
