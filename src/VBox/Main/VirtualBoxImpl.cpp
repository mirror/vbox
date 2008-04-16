/* $Id$ */
/** @file
 * Implmentation of IVirtualBox in VBoxSVC.
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

#include "VirtualBoxImpl.h"
#include "MachineImpl.h"
#include "HardDiskImpl.h"
#include "DVDImageImpl.h"
#include "FloppyImageImpl.h"
#include "SharedFolderImpl.h"
#include "ProgressImpl.h"
#include "HostImpl.h"
#include "USBControllerImpl.h"
#include "SystemPropertiesImpl.h"
#include "GuestOSTypeImpl.h"

#include "VirtualBoxXMLUtil.h"

#include "Logging.h"

#ifdef RT_OS_WINDOWS
#include "win32/svchlp.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/thread.h>
#include <iprt/process.h>
#include <iprt/env.h>
#include <iprt/cpputils.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/VBoxHDD.h>
#include <VBox/VBoxHDD-new.h>
#include <VBox/ostypes.h>
#include <VBox/version.h>

#include <VBox/com/com.h>
#include <VBox/com/array.h>

#include <algorithm>
#include <set>
#include <memory> // for auto_ptr

#include <typeinfo>

// defines
/////////////////////////////////////////////////////////////////////////////

#define VBOX_GLOBAL_SETTINGS_FILE "VirtualBox.xml"

// globals
/////////////////////////////////////////////////////////////////////////////

static const char DefaultGlobalConfig [] =
{
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" RTFILE_LINEFEED
    "<!-- innotek VirtualBox Global Configuration -->" RTFILE_LINEFEED
    "<VirtualBox xmlns=\"" VBOX_XML_NAMESPACE "\" "
        "version=\"" VBOX_XML_VERSION_FULL "\">" RTFILE_LINEFEED
    "  <Global>"RTFILE_LINEFEED
    "    <MachineRegistry/>"RTFILE_LINEFEED
    "    <DiskRegistry/>"RTFILE_LINEFEED
    "    <USBDeviceFilters/>"RTFILE_LINEFEED
    "    <SystemProperties/>"RTFILE_LINEFEED
    "  </Global>"RTFILE_LINEFEED
    "</VirtualBox>"RTFILE_LINEFEED
};

// static
Bstr VirtualBox::sVersion;

// static
Bstr VirtualBox::sSettingsFormatVersion;

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

VirtualBox::VirtualBox()
    : mAsyncEventThread (NIL_RTTHREAD)
    , mAsyncEventQ (NULL)
{}

VirtualBox::~VirtualBox() {}

HRESULT VirtualBox::FinalConstruct()
{
    LogFlowThisFunc (("\n"));

    return init();
}

void VirtualBox::FinalRelease()
{
    LogFlowThisFunc (("\n"));

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
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    LogFlow (("===========================================================\n"));
    LogFlowThisFuncEnter();

    if (sVersion.isNull())
        sVersion = VBOX_VERSION_STRING;
    LogFlowThisFunc (("Version: %ls\n", sVersion.raw()));

    if (sSettingsFormatVersion.isNull())
        sSettingsFormatVersion = VBOX_XML_VERSION_FULL;
    LogFlowThisFunc (("Settings Format Version: %ls\n",
                      sSettingsFormatVersion.raw()));

    /* Get the VirtualBox home directory. */
    {
        char homeDir [RTPATH_MAX];
        int vrc = com::GetVBoxUserHomeDirectory (homeDir, sizeof (homeDir));
        if (VBOX_FAILURE (vrc))
            return setError (E_FAIL,
                tr ("Could not create the VirtualBox home directory '%s'"
                    "(%Vrc)"),
                homeDir, vrc);

        unconst (mData.mHomeDir) = homeDir;
    }

    /* compose the global config file name (always full path) */
    Utf8StrFmt vboxConfigFile ("%s%c%s", mData.mHomeDir.raw(),
                               RTPATH_DELIMITER, VBOX_GLOBAL_SETTINGS_FILE);

    /* store the config file name */
    unconst (mData.mCfgFile.mName) = vboxConfigFile;

    /* lock the config file */
    HRESULT rc = lockConfig();
    if (SUCCEEDED (rc))
    {
        if (!isConfigLocked())
        {
            /*
             *  This means the config file not found. This is not fatal --
             *  we just create an empty one.
             */
            RTFILE handle = NIL_RTFILE;
            int vrc = RTFileOpen (&handle, vboxConfigFile,
                                  RTFILE_O_READWRITE | RTFILE_O_CREATE |
                                  RTFILE_O_DENY_WRITE);
            if (VBOX_SUCCESS (vrc))
                vrc = RTFileWrite (handle,
                                   (void *) DefaultGlobalConfig,
                                   sizeof (DefaultGlobalConfig), NULL);
            if (VBOX_FAILURE (vrc))
            {
                rc = setError (E_FAIL, tr ("Could not create the default settings file "
                                           "'%s' (%Vrc)"),
                                       vboxConfigFile.raw(), vrc);
            }
            else
            {
                mData.mCfgFile.mHandle = handle;
                /* we do not close the file to simulate lockConfig() */
            }
        }
    }

    if (SUCCEEDED (rc))
    {
        try
        {
            using namespace settings;

#if 0
            /// @todo disabled until made thread-safe by using handle duplicates
            File file (File::ReadWrite, mData.mCfgFile.mHandle, vboxConfigFile);
#else
            File file (File::Read, vboxConfigFile);
#endif
            XmlTreeBackend tree;

            rc = VirtualBox::loadSettingsTree_FirstTime (tree, file,
                                                         mData.mSettingsFileVersion);
            CheckComRCThrowRC (rc);

            Key global = tree.rootKey().key ("Global");

            /* create the host object early, machines will need it */
            unconst (mData.mHost).createObject();
            rc = mData.mHost->init (this);
            ComAssertComRCThrowRC (rc);

            rc = mData.mHost->loadSettings (global);
            CheckComRCThrowRC (rc);

            /* create the system properties object */
            unconst (mData.mSystemProperties).createObject();
            rc = mData.mSystemProperties->init (this);
            ComAssertComRCThrowRC (rc);

            rc = mData.mSystemProperties->loadSettings (global);
            CheckComRCThrowRC (rc);

            /* guest OS type objects, needed by machines */
            rc = registerGuestOSTypes();
            ComAssertComRCThrowRC (rc);

            /* hard disks, needed by machines */
            rc = loadDisks (global);
            CheckComRCThrowRC (rc);

            /* machines */
            rc = loadMachines (global);
            CheckComRCThrowRC (rc);

            /* check hard disk consistency */
/// @todo (r=dmik) add IVirtualBox::cleanupHardDisks() instead or similar
//            for (HardDiskList::const_iterator it = mData.mHardDisks.begin();
//                 it != mData.mHardDisks.end() && SUCCEEDED (rc);
//                 ++ it)
//            {
//                rc = (*it)->checkConsistency();
//            }
//            CheckComRCBreakRC ((rc));

            /// @todo (dmik) if successful, check for orphan (unused) diffs
            //  that might be left because of the server crash, and remove
            //  Hmm, is it the same remark as above?..
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
    }

    if (SUCCEEDED (rc))
    {
        /* start the client watcher thread */
#if defined(RT_OS_WINDOWS)
        unconst (mWatcherData.mUpdateReq) = ::CreateEvent (NULL, FALSE, FALSE, NULL);
#elif defined(RT_OS_OS2)
        RTSemEventCreate (&unconst (mWatcherData.mUpdateReq));
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
        RTSemEventCreate (&unconst (mWatcherData.mUpdateReq));
#else
# error "Port me!"
#endif
        int vrc = RTThreadCreate (&unconst (mWatcherData.mThread),
                                  ClientWatcher, (void *) this,
                                  0, RTTHREADTYPE_MAIN_WORKER,
                                  RTTHREADFLAGS_WAITABLE, "Watcher");
        ComAssertRC (vrc);
        if (VBOX_FAILURE (vrc))
            rc = E_FAIL;
    }

    if (SUCCEEDED (rc)) do
    {
        /* start the async event handler thread */
        int vrc = RTThreadCreate (&unconst (mAsyncEventThread), AsyncEventHandler,
                                  &unconst (mAsyncEventQ),
                                  0, RTTHREADTYPE_MAIN_WORKER,
                                  RTTHREADFLAGS_WAITABLE, "EventHandler");
        ComAssertRCBreak (vrc, rc = E_FAIL);

        /* wait until the thread sets mAsyncEventQ */
        RTThreadUserWait (mAsyncEventThread, RT_INDEFINITE_WAIT);
        ComAssertBreak (mAsyncEventQ, rc = E_FAIL);
    }
    while (0);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    LogFlow (("===========================================================\n"));
    return rc;
}

void VirtualBox::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    LogFlow (("===========================================================\n"));
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("initFailed()=%d\n", autoUninitSpan.initFailed()));

    /* tell all our child objects we've been uninitialized */

    LogFlowThisFunc (("Uninitializing machines (%d)...\n", mData.mMachines.size()));
    if (mData.mMachines.size())
    {
        MachineList::iterator it = mData.mMachines.begin();
        while (it != mData.mMachines.end())
            (*it++)->uninit();
        mData.mMachines.clear();
    }

    if (mData.mSystemProperties)
    {
        mData.mSystemProperties->uninit();
        unconst (mData.mSystemProperties).setNull();
    }

    if (mData.mHost)
    {
        mData.mHost->uninit();
        unconst (mData.mHost).setNull();
    }

    /*
     *  Uninit all other children still referenced by clients
     *  (unregistered machines, hard disks, DVD/floppy images,
     *  server-side progress operations).
     */
    uninitDependentChildren();

    mData.mFloppyImages.clear();
    mData.mDVDImages.clear();
    mData.mHardDisks.clear();

    mData.mHardDiskMap.clear();

    mData.mProgressOperations.clear();

    mData.mGuestOSTypes.clear();

    /* unlock the config file */
    unlockConfig();

    LogFlowThisFunc (("Releasing callbacks...\n"));
    if (mData.mCallbacks.size())
    {
        /* release all callbacks */
        LogWarningFunc (("%d unregistered callbacks!\n",
                         mData.mCallbacks.size()));
        mData.mCallbacks.clear();
    }

    LogFlowThisFunc (("Terminating the async event handler...\n"));
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
            if (VBOX_FAILURE (vrc))
                LogWarningFunc (("RTThreadWait(%RTthrd) -> %Vrc\n",
                                 mAsyncEventThread, vrc));
        }
        else
        {
            AssertMsgFailed (("postEvent(NULL) failed\n"));
            RTThreadWait (mAsyncEventThread, 0, NULL);
        }

        unconst (mAsyncEventThread) = NIL_RTTHREAD;
        unconst (mAsyncEventQ) = NULL;
    }

    LogFlowThisFunc (("Terminating the client watcher...\n"));
    if (mWatcherData.mThread != NIL_RTTHREAD)
    {
        /* signal the client watcher thread */
        updateClientWatcher();
        /* wait for the termination */
        RTThreadWait (mWatcherData.mThread, RT_INDEFINITE_WAIT, NULL);
        unconst (mWatcherData.mThread) = NIL_RTTHREAD;
    }
    mWatcherData.mProcesses.clear();
#if defined(RT_OS_WINDOWS)
    if (mWatcherData.mUpdateReq != NULL)
    {
        ::CloseHandle (mWatcherData.mUpdateReq);
        unconst (mWatcherData.mUpdateReq) = NULL;
    }
#elif defined(RT_OS_OS2)
    if (mWatcherData.mUpdateReq != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy (mWatcherData.mUpdateReq);
        unconst (mWatcherData.mUpdateReq) = NIL_RTSEMEVENT;
    }
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    if (mWatcherData.mUpdateReq != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy (mWatcherData.mUpdateReq);
        unconst (mWatcherData.mUpdateReq) = NIL_RTSEMEVENT;
    }
#else
# error "Port me!"
#endif

    LogFlowThisFuncLeave();
    LogFlow (("===========================================================\n"));
}

// IVirtualBox properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VirtualBox::COMGETTER(Version) (BSTR *aVersion)
{
    if (!aVersion)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    sVersion.cloneTo (aVersion);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(HomeFolder) (BSTR *aHomeFolder)
{
    if (!aHomeFolder)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mHomeDir is const and doesn't need a lock */
    mData.mHomeDir.cloneTo (aHomeFolder);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(SettingsFilePath) (BSTR *aSettingsFilePath)
{
    if (!aSettingsFilePath)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mCfgFile.mName is const and doesn't need a lock */
    mData.mCfgFile.mName.cloneTo (aSettingsFilePath);
    return S_OK;
}

STDMETHODIMP VirtualBox::
COMGETTER(SettingsFileVersion) (BSTR *aSettingsFileVersion)
{
    if (!aSettingsFileVersion)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData.mSettingsFileVersion.cloneTo (aSettingsFileVersion);
    return S_OK;
}

STDMETHODIMP VirtualBox::
COMGETTER(SettingsFormatVersion) (BSTR *aSettingsFormatVersion)
{
    if (!aSettingsFormatVersion)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    sSettingsFormatVersion.cloneTo (aSettingsFormatVersion);
    return S_OK;
}

STDMETHODIMP VirtualBox::COMGETTER(Host) (IHost **aHost)
{
    if (!aHost)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    mData.mHost.queryInterfaceTo (aHost);
    return S_OK;
}

STDMETHODIMP
VirtualBox::COMGETTER(SystemProperties) (ISystemProperties **aSystemProperties)
{
    if (!aSystemProperties)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    mData.mSystemProperties.queryInterfaceTo (aSystemProperties);
    return S_OK;
}

/**
 * @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::COMGETTER(Machines) (IMachineCollection **aMachines)
{
    if (!aMachines)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ComObjPtr <MachineCollection> collection;
    collection.createObject();

    AutoReaderLock alock (this);
    collection->init (mData.mMachines);
    collection.queryInterfaceTo (aMachines);

    return S_OK;
}

/**
 * @note Locks this object for reading.
 */
STDMETHODIMP
VirtualBox::COMGETTER(Machines2) (ComSafeArrayOut (IMachine *, aMachines))
{
    if (ComSafeArrayOutIsNull (aMachines))
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    SafeIfaceArray <IMachine> machines (mData.mMachines);
    machines.detachTo (ComSafeArrayOutArg (aMachines));

    return S_OK;
}

/**
 * @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::COMGETTER(HardDisks) (IHardDiskCollection **aHardDisks)
{
    if (!aHardDisks)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ComObjPtr <HardDiskCollection> collection;
    collection.createObject();

    AutoReaderLock alock (this);
    collection->init (mData.mHardDisks);
    collection.queryInterfaceTo (aHardDisks);

    return S_OK;
}

/**
 * @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::COMGETTER(DVDImages) (IDVDImageCollection **aDVDImages)
{
    if (!aDVDImages)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ComObjPtr <DVDImageCollection> collection;
    collection.createObject();

    AutoReaderLock alock (this);
    collection->init (mData.mDVDImages);
    collection.queryInterfaceTo (aDVDImages);

    return S_OK;
}

/**
 * @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::COMGETTER(FloppyImages) (IFloppyImageCollection **aFloppyImages)
{
    if (!aFloppyImages)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ComObjPtr <FloppyImageCollection> collection;
    collection.createObject();

    AutoReaderLock alock (this);
    collection->init (mData.mFloppyImages);
    collection.queryInterfaceTo (aFloppyImages);

    return S_OK;
}

/**
 * @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::COMGETTER(ProgressOperations) (IProgressCollection **aOperations)
{
    if (!aOperations)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ComObjPtr <ProgressCollection> collection;
    collection.createObject();

    AutoReaderLock alock (this);
    collection->init (mData.mProgressOperations);
    collection.queryInterfaceTo (aOperations);

    return S_OK;
}

/**
 * @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::COMGETTER(GuestOSTypes) (IGuestOSTypeCollection **aGuestOSTypes)
{
    if (!aGuestOSTypes)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ComObjPtr <GuestOSTypeCollection> collection;
    collection.createObject();

    AutoReaderLock alock (this);
    collection->init (mData.mGuestOSTypes);
    collection.queryInterfaceTo (aGuestOSTypes);

    return S_OK;
}

STDMETHODIMP
VirtualBox::COMGETTER(SharedFolders) (ISharedFolderCollection **aSharedFolders)
{
    if (!aSharedFolders)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    return setError (E_NOTIMPL, "Not yet implemented");
}

// IVirtualBox methods
/////////////////////////////////////////////////////////////////////////////

/** @note Locks mSystemProperties object for reading. */
STDMETHODIMP VirtualBox::CreateMachine (INPTR BSTR aBaseFolder,
                                        INPTR BSTR aName,
                                        INPTR GUIDPARAM aId,
                                        IMachine **aMachine)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aBaseFolder='%ls', aName='%ls' aMachine={%p}\n",
                      aBaseFolder, aName, aMachine));

    if (!aName)
        return E_INVALIDARG;
    if (!aMachine)
        return E_POINTER;

    if (!*aName)
        return setError (E_INVALIDARG,
            tr ("Machine name cannot be empty"));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* Compose the settings file name using the following scheme:
     *
     *     <base_folder>/<machine_name>/<machine_name>.xml
     *
     * If a non-null and non-empty base folder is specified, the default
     * machine folder will be used as a base folder.
     */
    Bstr settingsFile = aBaseFolder;
    if (settingsFile.isEmpty())
    {
        AutoReaderLock propsLock (systemProperties());
        /* we use the non-full folder value below to keep the path relative */
        settingsFile = systemProperties()->defaultMachineFolder();
    }
    settingsFile = Utf8StrFmt ("%ls%c%ls%c%ls.xml",
                               settingsFile.raw(), RTPATH_DELIMITER,
                               aName, RTPATH_DELIMITER, aName);

    HRESULT rc = E_FAIL;

    /* create a new object */
    ComObjPtr <Machine> machine;
    rc = machine.createObject();
    if (SUCCEEDED (rc))
    {
        /* Create UUID if an empty one was specified. */
        Guid id = aId;
        if (id.isEmpty())
            id.create();

        /* initialize the machine object */
        rc = machine->init (this, settingsFile, Machine::Init_New, aName, TRUE, &id);
        if (SUCCEEDED (rc))
        {
            /* set the return value */
            rc = machine.queryInterfaceTo (aMachine);
            ComAssertComRC (rc);
        }
    }

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP VirtualBox::CreateLegacyMachine (INPTR BSTR aSettingsFile,
                                              INPTR BSTR aName,
                                              INPTR GUIDPARAM aId,
                                              IMachine **aMachine)
{
    /* null and empty strings are not allowed as path names */
    if (!aSettingsFile || !(*aSettingsFile))
        return E_INVALIDARG;

    if (!aName)
        return E_INVALIDARG;
    if (!aMachine)
        return E_POINTER;

    if (!*aName)
        return setError (E_INVALIDARG,
            tr ("Machine name cannot be empty"));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    HRESULT rc = E_FAIL;

    Utf8Str settingsFile = aSettingsFile;
    /* append the default extension if none */
    if (!RTPathHaveExt (settingsFile))
        settingsFile = Utf8StrFmt ("%s.xml", settingsFile.raw());

    /* create a new object */
    ComObjPtr<Machine> machine;
    rc = machine.createObject();
    if (SUCCEEDED (rc))
    {
        /* Create UUID if an empty one was specified. */
        Guid id = aId;
        if (id.isEmpty())
            id.create();

        /* initialize the machine object */
        rc = machine->init (this, Bstr (settingsFile), Machine::Init_New,
                            aName, FALSE /* aNameSync */, &id);
        if (SUCCEEDED (rc))
        {
            /* set the return value */
            rc = machine.queryInterfaceTo (aMachine);
            ComAssertComRC (rc);
        }
    }
    return rc;
}

STDMETHODIMP VirtualBox::OpenMachine (INPTR BSTR aSettingsFile,
                                      IMachine **aMachine)
{
    /* null and empty strings are not allowed as path names */
    if (!aSettingsFile || !(*aSettingsFile))
        return E_INVALIDARG;

    if (!aMachine)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    HRESULT rc = E_FAIL;

    /* create a new object */
    ComObjPtr<Machine> machine;
    rc = machine.createObject();
    if (SUCCEEDED (rc))
    {
        /* initialize the machine object */
        rc = machine->init (this, aSettingsFile, Machine::Init_Existing);
        if (SUCCEEDED (rc))
        {
            /* set the return value */
            rc = machine.queryInterfaceTo (aMachine);
            ComAssertComRC (rc);
        }
    }

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::RegisterMachine (IMachine *aMachine)
{
    if (!aMachine)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    HRESULT rc;

    Bstr name;
    rc = aMachine->COMGETTER(Name) (name.asOutParam());
    CheckComRCReturnRC (rc);

    /*
     *  we can safely cast child to Machine * here because only Machine
     *  implementations of IMachine can be among our children
     */
    Machine *machine = static_cast <Machine *> (getDependentChild (aMachine));
    if (!machine)
    {
        /*
         *  this machine was not created by CreateMachine()
         *  or opened by OpenMachine() or loaded during startup
         */
        return setError (E_FAIL,
            tr ("The machine named '%ls' is not created within this "
                "VirtualBox instance"), name.raw());
    }

    AutoCaller machCaller (machine);
    ComAssertComRCRetRC (machCaller.rc());

    rc = registerMachine (machine);

    /* fire an event */
    if (SUCCEEDED (rc))
        onMachineRegistered (machine->uuid(), TRUE);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::GetMachine (INPTR GUIDPARAM aId, IMachine **aMachine)
{
    if (!aMachine)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ComObjPtr <Machine> machine;
    HRESULT rc = findMachine (Guid (aId), true /* setError */, &machine);

    /* the below will set *aMachine to NULL if machine is null */
    machine.queryInterfaceTo (aMachine);

    return rc;
}

/** @note Locks this object for reading, then some machine objects for reading. */
STDMETHODIMP VirtualBox::FindMachine (INPTR BSTR aName, IMachine **aMachine)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aName=\"%ls\", aMachine={%p}\n", aName, aMachine));

    if (!aName)
        return E_INVALIDARG;
    if (!aMachine)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* start with not found */
    ComObjPtr <Machine> machine;
    MachineList machines;
    {
        /* take a copy for safe iteration outside the lock */
        AutoReaderLock alock (this);
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
            AutoReaderLock machLock (*it);
            if ((*it)->name() == aName)
                machine = *it;
        }
    }

    /* this will set (*machine) to NULL if machineObj is null */
    machine.queryInterfaceTo (aMachine);

    HRESULT rc = machine
        ? S_OK
        : setError (E_INVALIDARG,
            tr ("Could not find a registered machine named '%ls'"), aName);

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::UnregisterMachine (INPTR GUIDPARAM aId,
                                            IMachine **aMachine)
{
    Guid id = aId;
    if (id.isEmpty())
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    ComObjPtr <Machine> machine;

    HRESULT rc = findMachine (id, true /* setError */, &machine);
    CheckComRCReturnRC (rc);

    rc = machine->trySetRegistered (FALSE);
    CheckComRCReturnRC (rc);

    /* remove from the collection of registered machines */
    mData.mMachines.remove (machine);

    /* save the global registry */
    rc = saveSettings();

    /* return the unregistered machine to the caller */
    machine.queryInterfaceTo (aMachine);

    /* fire an event */
    onMachineRegistered (id, FALSE);

    return rc;
}

STDMETHODIMP VirtualBox::CreateHardDisk (HardDiskStorageType_T aStorageType,
                                         IHardDisk **aHardDisk)
{
    if (!aHardDisk)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    HRESULT rc = E_FAIL;

    ComObjPtr <HardDisk> hardDisk;

    switch (aStorageType)
    {
        case HardDiskStorageType_VirtualDiskImage:
        {
            ComObjPtr <HVirtualDiskImage> vdi;
            vdi.createObject();
            rc = vdi->init (this, NULL, NULL);
            hardDisk = vdi;
            break;
        }

        case HardDiskStorageType_ISCSIHardDisk:
        {
            ComObjPtr <HISCSIHardDisk> iscsi;
            iscsi.createObject();
            rc = iscsi->init (this);
            hardDisk = iscsi;
            break;
        }

       case HardDiskStorageType_VMDKImage:
       {
            ComObjPtr <HVMDKImage> vmdk;
            vmdk.createObject();
            rc = vmdk->init (this, NULL, NULL);
            hardDisk = vmdk;
            break;
       }
       case HardDiskStorageType_CustomHardDisk:
       {
            ComObjPtr <HCustomHardDisk> custom;
            custom.createObject();
            rc = custom->init (this, NULL, NULL);
            hardDisk = custom;
            break;
       }
       case HardDiskStorageType_VHDImage:
       {
            ComObjPtr <HVHDImage> vhd;
            vhd.createObject();
            rc = vhd->init (this, NULL, NULL);
            hardDisk = vhd;
            break;
       }
       default:
           AssertFailed();
    };

    if (SUCCEEDED (rc))
        hardDisk.queryInterfaceTo (aHardDisk);

    return rc;
}

/** @note Locks mSystemProperties object for reading. */
STDMETHODIMP VirtualBox::OpenHardDisk (INPTR BSTR aLocation, IHardDisk **aHardDisk)
{
    /* null and empty strings are not allowed locations */
    if (!aLocation || !(*aLocation))
        return E_INVALIDARG;

    if (!aHardDisk)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* Currently, the location is always a path. So, append the
     * default path if only a name is given. */
    Bstr location = aLocation;
    {
        Utf8Str loc = aLocation;
        if (!RTPathHavePath (loc))
        {
            AutoLock propsLock (mData.mSystemProperties);
            location = Utf8StrFmt ("%ls%c%s",
                                   mData.mSystemProperties->defaultVDIFolder().raw(),
                                   RTPATH_DELIMITER,
                                   loc.raw());
        }
    }

    ComObjPtr <HardDisk> hardDisk;
    HRESULT rc = HardDisk::openHardDisk (this, location, hardDisk);
    if (SUCCEEDED (rc))
        hardDisk.queryInterfaceTo (aHardDisk);

    return rc;
}

/** @note Locks mSystemProperties object for reading. */
STDMETHODIMP VirtualBox::OpenVirtualDiskImage (INPTR BSTR aFilePath,
                                               IVirtualDiskImage **aImage)
{
    /* null and empty strings are not allowed as path names here */
    if (!aFilePath || !(*aFilePath))
        return E_INVALIDARG;

    if (!aImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* append the default path if only a name is given */
    Bstr path = aFilePath;
    {
        Utf8Str fp = aFilePath;
        if (!RTPathHavePath (fp))
        {
            AutoLock propsLock (mData.mSystemProperties);
            path = Utf8StrFmt ("%ls%c%s",
                               mData.mSystemProperties->defaultVDIFolder().raw(),
                               RTPATH_DELIMITER,
                               fp.raw());
        }
    }

    ComObjPtr <HVirtualDiskImage> vdi;
    vdi.createObject();
    HRESULT rc = vdi->init (this, NULL, path);

    if (SUCCEEDED (rc))
        vdi.queryInterfaceTo (aImage);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::RegisterHardDisk (IHardDisk *aHardDisk)
{
    if (!aHardDisk)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    VirtualBoxBase *child = getDependentChild (aHardDisk);
    if (!child)
        return setError (E_FAIL, tr ("The given hard disk is not created within "
                                     "this VirtualBox instance"));

    /*
     *  we can safely cast child to HardDisk * here because only HardDisk
     *  implementations of IHardDisk can be among our children
     */

    return registerHardDisk (static_cast <HardDisk *> (child), RHD_External);
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::GetHardDisk (INPTR GUIDPARAM aId, IHardDisk **aHardDisk)
{
    if (!aHardDisk)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    Guid id = aId;
    ComObjPtr <HardDisk> hd;
    HRESULT rc = findHardDisk (&id, NULL, true /* setError */, &hd);

    /* the below will set *aHardDisk to NULL if hd is null */
    hd.queryInterfaceTo (aHardDisk);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::FindHardDisk (INPTR BSTR aLocation,
                                       IHardDisk **aHardDisk)
{
   if (!aLocation)
        return E_INVALIDARG;
   if (!aHardDisk)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    Utf8Str location = aLocation;
    if (strncmp (location, "iscsi:", 6) == 0)
    {
        /* nothing special */
    }
    else
    {
        /* For locations represented by file paths, append the default path if
         * only a name is given, and then get the full path. */
        if (!RTPathHavePath (location))
        {
            AutoLock propsLock (mData.mSystemProperties);
            location = Utf8StrFmt ("%ls%c%s",
                                   mData.mSystemProperties->defaultVDIFolder().raw(),
                                   RTPATH_DELIMITER,
                                   location.raw());
        }

        /* get the full file name */
        char buf [RTPATH_MAX];
        int vrc = RTPathAbsEx (mData.mHomeDir, location, buf, sizeof (buf));
        if (VBOX_FAILURE (vrc))
            return setError (E_FAIL, tr ("Invalid hard disk location '%ls' (%Vrc)"),
                             aLocation, vrc);
        location = buf;
    }

    ComObjPtr <HardDisk> hardDisk;
    HRESULT rc = findHardDisk (NULL, Bstr (location), true /* setError */,
                               &hardDisk);

    /* the below will set *aHardDisk to NULL if hardDisk is null */
    hardDisk.queryInterfaceTo (aHardDisk);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::FindVirtualDiskImage (INPTR BSTR aFilePath,
                                               IVirtualDiskImage **aImage)
{
   if (!aFilePath)
        return E_INVALIDARG;
   if (!aImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* append the default path if only a name is given */
    Utf8Str path = aFilePath;
    {
        Utf8Str fp = path;
        if (!RTPathHavePath (fp))
        {
            AutoLock propsLock (mData.mSystemProperties);
            path = Utf8StrFmt ("%ls%c%s",
                               mData.mSystemProperties->defaultVDIFolder().raw(),
                               RTPATH_DELIMITER,
                               fp.raw());
        }
    }

    /* get the full file name */
    char buf [RTPATH_MAX];
    int vrc = RTPathAbsEx (mData.mHomeDir, path, buf, sizeof (buf));
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL, tr ("Invalid image file path '%ls' (%Vrc)"),
                                 aFilePath, vrc);

    ComObjPtr <HVirtualDiskImage> vdi;
    HRESULT rc = findVirtualDiskImage (NULL, Bstr (buf), true /* setError */,
                                       &vdi);

    /* the below will set *aImage to NULL if vdi is null */
    vdi.queryInterfaceTo (aImage);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::UnregisterHardDisk (INPTR GUIDPARAM aId, IHardDisk **aHardDisk)
{
    if (!aHardDisk)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aHardDisk = NULL;

    Guid id = aId;
    ComObjPtr <HardDisk> hd;
    HRESULT rc = findHardDisk (&id, NULL, true /* setError */, &hd);
    CheckComRCReturnRC (rc);

    rc = unregisterHardDisk (hd);
    if (SUCCEEDED (rc))
        hd.queryInterfaceTo (aHardDisk);

    return rc;
}

/** @note Doesn't lock anything. */
STDMETHODIMP VirtualBox::OpenDVDImage (INPTR BSTR aFilePath, INPTR GUIDPARAM aId,
                                       IDVDImage **aDVDImage)
{
    /* null and empty strings are not allowed as path names */
    if (!aFilePath || !(*aFilePath))
        return E_INVALIDARG;

    if (!aDVDImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    HRESULT rc = E_FAIL;

    Guid uuid = aId;
    /* generate an UUID if not specified */
    if (uuid.isEmpty())
        uuid.create();

    ComObjPtr <DVDImage> dvdImage;
    dvdImage.createObject();
    rc = dvdImage->init (this, aFilePath, FALSE /* !isRegistered */, uuid);
    if (SUCCEEDED (rc))
        dvdImage.queryInterfaceTo (aDVDImage);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::RegisterDVDImage (IDVDImage *aDVDImage)
{
    if (!aDVDImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    VirtualBoxBase *child = getDependentChild (aDVDImage);
    if (!child)
        return setError (E_FAIL, tr ("The given CD/DVD image is not created within "
                                     "this VirtualBox instance"));

    /*
     *  we can safely cast child to DVDImage * here because only DVDImage
     *  implementations of IDVDImage can be among our children
     */

    return registerDVDImage (static_cast <DVDImage *> (child),
                             FALSE /* aOnStartUp */);
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::GetDVDImage (INPTR GUIDPARAM aId, IDVDImage **aDVDImage)
{
    if (!aDVDImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    Guid uuid = aId;
    ComObjPtr <DVDImage> dvd;
    HRESULT rc = findDVDImage (&uuid, NULL, true /* setError */, &dvd);

    /* the below will set *aDVDImage to NULL if dvd is null */
    dvd.queryInterfaceTo (aDVDImage);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::FindDVDImage (INPTR BSTR aFilePath, IDVDImage **aDVDImage)
{
   if (!aFilePath)
        return E_INVALIDARG;
   if (!aDVDImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* get the full file name */
    char buf [RTPATH_MAX];
    int vrc = RTPathAbsEx (mData.mHomeDir, Utf8Str (aFilePath), buf, sizeof (buf));
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL, tr ("Invalid image file path: '%ls' (%Vrc)"),
                                 aFilePath, vrc);

    ComObjPtr <DVDImage> dvd;
    HRESULT rc = findDVDImage (NULL, Bstr (buf), true /* setError */, &dvd);

    /* the below will set *dvdImage to NULL if dvd is null */
    dvd.queryInterfaceTo (aDVDImage);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::GetDVDImageUsage (INPTR GUIDPARAM aId,
                                           ResourceUsage_T aUsage,
                                           BSTR *aMachineIDs)
{
    if (!aMachineIDs)
        return E_POINTER;
    if (aUsage == ResourceUsage_Null)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Guid uuid = Guid (aId);
    HRESULT rc = findDVDImage (&uuid, NULL, true /* setError */, NULL);
    if (FAILED (rc))
        return rc;

    Bstr ids;
    getDVDImageUsage (uuid, aUsage, &ids);
    ids.cloneTo (aMachineIDs);

    return S_OK;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::UnregisterDVDImage (INPTR GUIDPARAM aId,
                                             IDVDImage **aDVDImage)
{
    if (!aDVDImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    *aDVDImage = NULL;

    Guid uuid = aId;
    ComObjPtr <DVDImage> dvd;
    HRESULT rc = findDVDImage (&uuid, NULL, true /* setError */, &dvd);
    CheckComRCReturnRC (rc);

    if (!getDVDImageUsage (aId, ResourceUsage_All))
    {
        /* remove from the collection */
        mData.mDVDImages.remove (dvd);

        /* save the global config file */
        rc = saveSettings();

        if (SUCCEEDED (rc))
        {
            rc = dvd.queryInterfaceTo (aDVDImage);
            ComAssertComRC (rc);
        }
    }
    else
        rc = setError(E_FAIL,
            tr ("The CD/DVD image with the UUID {%s} is currently in use"),
            uuid.toString().raw());

    return rc;
}

/** @note Doesn't lock anything. */
STDMETHODIMP VirtualBox::OpenFloppyImage (INPTR BSTR aFilePath, INPTR GUIDPARAM aId,
                                          IFloppyImage **aFloppyImage)
{
    /* null and empty strings are not allowed as path names */
    if (!aFilePath || !(*aFilePath))
        return E_INVALIDARG;

    if (!aFloppyImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    HRESULT rc = E_FAIL;

    Guid uuid = aId;
    /* generate an UUID if not specified */
    if (Guid::isEmpty (aId))
        uuid.create();

    ComObjPtr <FloppyImage> floppyImage;
    floppyImage.createObject();
    rc = floppyImage->init (this, aFilePath, FALSE /* !isRegistered */, uuid);
    if (SUCCEEDED (rc))
        floppyImage.queryInterfaceTo (aFloppyImage);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::RegisterFloppyImage (IFloppyImage *aFloppyImage)
{
    if (!aFloppyImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    VirtualBoxBase *child = getDependentChild (aFloppyImage);
    if (!child)
        return setError (E_FAIL, tr ("The given floppy image is not created within "
                                     "this VirtualBox instance"));

    /*
     *  we can safely cast child to FloppyImage * here because only FloppyImage
     *  implementations of IFloppyImage can be among our children
     */

    return registerFloppyImage (static_cast <FloppyImage *> (child),
                                FALSE /* aOnStartUp */);
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::GetFloppyImage (INPTR GUIDPARAM aId,
                                         IFloppyImage **aFloppyImage)
{
    if (!aFloppyImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    Guid uuid = aId;
    ComObjPtr <FloppyImage> floppy;
    HRESULT rc = findFloppyImage (&uuid, NULL, true /* setError */, &floppy);

    /* the below will set *aFloppyImage to NULL if dvd is null */
    floppy.queryInterfaceTo (aFloppyImage);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::FindFloppyImage (INPTR BSTR aFilePath,
                                          IFloppyImage **aFloppyImage)
{
   if (!aFilePath)
        return E_INVALIDARG;
   if (!aFloppyImage)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* get the full file name */
    char buf [RTPATH_MAX];
    int vrc = RTPathAbsEx (mData.mHomeDir, Utf8Str (aFilePath), buf, sizeof (buf));
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL, tr ("Invalid image file path: '%ls' (%Vrc)"),
                                 aFilePath, vrc);

    ComObjPtr <FloppyImage> floppy;
    HRESULT rc = findFloppyImage (NULL, Bstr (buf), true /* setError */, &floppy);

    /* the below will set *image to NULL if img is null */
    floppy.queryInterfaceTo (aFloppyImage);

    return rc;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::GetFloppyImageUsage (INPTR GUIDPARAM aId,
                                              ResourceUsage_T aUsage,
                                              BSTR *aMachineIDs)
{
    if (!aMachineIDs)
        return E_POINTER;
    if (aUsage == ResourceUsage_Null)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Guid uuid = Guid (aId);
    HRESULT rc = findFloppyImage (&uuid, NULL, true /* setError */, NULL);
    if (FAILED (rc))
        return rc;

    Bstr ids;
    getFloppyImageUsage (uuid, aUsage, &ids);
    ids.cloneTo (aMachineIDs);

    return S_OK;
}

/** @note Locks objects! */
STDMETHODIMP VirtualBox::UnregisterFloppyImage (INPTR GUIDPARAM aId,
                                                IFloppyImage **aFloppyImage)
{
    if (!aFloppyImage)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    *aFloppyImage = NULL;

    Guid uuid = aId;
    ComObjPtr <FloppyImage> floppy;
    HRESULT rc = findFloppyImage (&uuid, NULL, true /* setError */, &floppy);
    CheckComRCReturnRC (rc);

    if (!getFloppyImageUsage (aId, ResourceUsage_All))
    {
        /* remove from the collection */
        mData.mFloppyImages.remove (floppy);

        /* save the global config file */
        rc = saveSettings();
        if (SUCCEEDED (rc))
        {
            rc = floppy.queryInterfaceTo (aFloppyImage);
            ComAssertComRC (rc);
        }
    }
    else
        rc = setError(E_FAIL,
            tr ("A floppy image with UUID {%s} is currently in use"),
            uuid.toString().raw());

    return rc;
}

/** @note Locks this object for reading. */
STDMETHODIMP VirtualBox::GetGuestOSType (INPTR BSTR aId, IGuestOSType **aType)
{
    if (!aType)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aType = NULL;

    AutoReaderLock alock (this);

    for (GuestOSTypeList::iterator it = mData.mGuestOSTypes.begin();
         it != mData.mGuestOSTypes.end();
         ++ it)
    {
        const Bstr &typeId = (*it)->id();
        AssertMsg (!!typeId, ("ID must not be NULL"));
        if (typeId == aId)
        {
            (*it).queryInterfaceTo (aType);
            break;
        }
    }

    return (*aType) ? S_OK :
        setError (E_INVALIDARG,
            tr ("'%ls' is not a valid Guest OS type"),
            aId);
}

STDMETHODIMP
VirtualBox::CreateSharedFolder (INPTR BSTR aName, INPTR BSTR aHostPath, BOOL aWritable)
{
    if (!aName || !aHostPath)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    return setError (E_NOTIMPL, "Not yet implemented");
}

STDMETHODIMP VirtualBox::RemoveSharedFolder (INPTR BSTR aName)
{
    if (!aName)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    return setError (E_NOTIMPL, "Not yet implemented");
}

/**
 *  @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::
GetNextExtraDataKey (INPTR BSTR aKey, BSTR *aNextKey, BSTR *aNextValue)
{
    if (!aNextKey)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* start with nothing found */
    *aNextKey = NULL;
    if (aNextValue)
        *aNextValue = NULL;

    HRESULT rc = S_OK;

    /* serialize config file access */
    AutoReaderLock alock (this);

    try
    {
        using namespace settings;

        /* load the config file */
#if 0
        /// @todo disabled until made thread-safe by using handle duplicates
        File file (File::ReadWrite, mData.mCfgFile.mHandle,
                   Utf8Str (mData.mCfgFile.mName));
#else
        File file (File::Read, Utf8Str (mData.mCfgFile.mName));
#endif
        XmlTreeBackend tree;

        rc = VirtualBox::loadSettingsTree_Again (tree, file);
        CheckComRCReturnRC (rc);

        Key globalNode = tree.rootKey().key ("Global");
        Key extraDataNode = tree.rootKey().findKey ("ExtraData");

        if (!extraDataNode.isNull())
        {
            Key::List items = extraDataNode.keys ("ExtraDataItem");
            if (items.size())
            {
                for (Key::List::const_iterator it = items.begin();
                     it != items.end(); ++ it)
                {
                    Bstr key = (*it).stringValue ("name");

                    /* if we're supposed to return the first one */
                    if (aKey == NULL)
                    {
                        key.cloneTo (aNextKey);
                        if (aNextValue)
                        {
                            Bstr val = (*it).stringValue ("value");
                            val.cloneTo (aNextValue);
                        }
                        return S_OK;
                    }

                    /* did we find the key we're looking for? */
                    if (key == aKey)
                    {
                        ++ it;
                        /* is there another item? */
                        if (it != items.end())
                        {
                            Bstr key = (*it).stringValue ("name");
                            key.cloneTo (aNextKey);
                            if (aNextValue)
                            {
                                Bstr val = (*it).stringValue ("value");
                                val.cloneTo (aNextValue);
                            }
                        }
                        /* else it's the last one, arguments are already NULL */
                        return S_OK;
                    }
                }
            }
        }

        /* Here we are when a) there are no items at all or b) there are items
         * but none of them equals to the requested non-NULL key. b) is an
         * error as well as a) if the key is non-NULL. When the key is NULL
         * (which is the case only when there are no items), we just fall
         * through to return NULLs and S_OK. */

        if (aKey != NULL)
            return setError (E_FAIL,
                tr ("Could not find the extra data key '%ls'"), aKey);
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions (RT_SRC_POS);
    }

    return rc;
}

/**
 *  @note Locks this object for reading.
 */
STDMETHODIMP VirtualBox::GetExtraData (INPTR BSTR aKey, BSTR *aValue)
{
    if (!aKey)
        return E_INVALIDARG;
    if (!aValue)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* start with nothing found */
    *aValue = NULL;

    HRESULT rc = S_OK;

    /* serialize file access */
    AutoReaderLock alock (this);

    try
    {
        using namespace settings;

        /* load the config file */
#if 0
        /// @todo disabled until made thread-safe by using handle duplicates
        File file (File::ReadWrite, mData.mCfgFile.mHandle,
                   Utf8Str (mData.mCfgFile.mName));
#else
        File file (File::Read, Utf8Str (mData.mCfgFile.mName));
#endif
        XmlTreeBackend tree;

        rc = VirtualBox::loadSettingsTree_Again (tree, file);
        CheckComRCReturnRC (rc);

        const Utf8Str key = aKey;

        Key globalNode = tree.rootKey().key ("Global");
        Key extraDataNode = globalNode.findKey ("ExtraData");

        if (!extraDataNode.isNull())
        {
            /* check if the key exists */
            Key::List items = extraDataNode.keys ("ExtraDataItem");
            for (Key::List::const_iterator it = items.begin();
                 it != items.end(); ++ it)
            {
                if (key == (*it).stringValue ("name"))
                {
                    Bstr val = (*it).stringValue ("value");
                    val.cloneTo (aValue);
                    break;
                }
            }
        }
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions (RT_SRC_POS);
    }

    return rc;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP VirtualBox::SetExtraData (INPTR BSTR aKey, INPTR BSTR aValue)
{
    if (!aKey)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    Guid emptyGuid;

    bool changed = false;
    HRESULT rc = S_OK;

    /* serialize file access */
    AutoLock alock (this);

    try
    {
        using namespace settings;

        /* load the config file */
#if 0
        /// @todo disabled until made thread-safe by using handle duplicates
        File file (File::ReadWrite, mData.mCfgFile.mHandle,
                   Utf8Str (mData.mCfgFile.mName));
#else
        File file (File::ReadWrite, Utf8Str (mData.mCfgFile.mName));
#endif
        XmlTreeBackend tree;

        rc = VirtualBox::loadSettingsTree_ForUpdate (tree, file);
        CheckComRCReturnRC (rc);

        const Utf8Str key = aKey;
        Bstr oldVal;

        Key globalNode = tree.rootKey().key ("Global");
        Key extraDataNode = globalNode.createKey ("ExtraData");
        Key extraDataItemNode;

        Key::List items = extraDataNode.keys ("ExtraDataItem");
        for (Key::List::const_iterator it = items.begin();
             it != items.end(); ++ it)
        {
            if (key == (*it).stringValue ("name"))
            {
                extraDataItemNode = *it;
                oldVal = (*it).stringValue ("value");
                break;
            }
        }

        /* When no key is found, oldVal is null */
        changed = oldVal != aValue;

        if (changed)
        {
            /* ask for permission from all listeners */
            Bstr error;
            if (!onExtraDataCanChange (Guid::Empty, aKey, aValue, error))
            {
                const char *sep = error.isEmpty() ? "" : ": ";
                const BSTR err = error.isNull() ? (const BSTR) L"" : error.raw();
                LogWarningFunc (("Someone vetoed! Change refused%s%ls\n",
                                 sep, err));
                return setError (E_ACCESSDENIED,
                    tr ("Could not set extra data because someone refused "
                        "the requested change of '%ls' to '%ls'%s%ls"),
                    aKey, aValue, sep, err);
            }

            if (aValue != NULL)
            {
                if (extraDataItemNode.isNull())
                {
                    extraDataItemNode = extraDataNode.appendKey ("ExtraDataItem");
                    extraDataItemNode.setStringValue ("name", key);
                }
                extraDataItemNode.setStringValue ("value", Utf8Str (aValue));
            }
            else
            {
                /* an old value does for sure exist here (XML schema
                 * guarantees that "value" may not absent in the
                 * <ExtraDataItem> element) */
                Assert (!extraDataItemNode.isNull());
                extraDataItemNode.zap();
            }

            /* save settings on success */
            rc = VirtualBox::saveSettingsTree (tree, file,
                                               mData.mSettingsFileVersion);
            CheckComRCReturnRC (rc);
        }
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions (RT_SRC_POS);
    }

    /* fire a notification */
    if (SUCCEEDED (rc) && changed)
        onExtraDataChange (Guid::Empty, aKey, aValue);

    return rc;
}

/**
 *  @note Locks objects!
 */
STDMETHODIMP VirtualBox::OpenSession (ISession *aSession, INPTR GUIDPARAM aMachineId)
{
    if (!aSession)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    Guid id = aMachineId;
    ComObjPtr <Machine> machine;

    HRESULT rc = findMachine (id, true /* setError */, &machine);
    CheckComRCReturnRC (rc);

    /* check the session state */
    SessionState_T state;
    rc = aSession->COMGETTER(State) (&state);
    CheckComRCReturnRC (rc);

    if (state != SessionState_Closed)
        return setError (E_INVALIDARG,
            tr ("The given session is already open or being opened"));

    /* get the IInternalSessionControl interface */
    ComPtr <IInternalSessionControl> control = aSession;
    ComAssertMsgRet (!!control, ("No IInternalSessionControl interface"),
                     E_INVALIDARG);

    rc = machine->openSession (control);

    if (SUCCEEDED (rc))
    {
        /*
         *  tell the client watcher thread to update the set of
         *  machines that have open sessions
         */
        updateClientWatcher();

        /* fire an event */
        onSessionStateChange (aMachineId, SessionState_Open);
    }

    return rc;
}

/**
 *  @note Locks objects!
 */
STDMETHODIMP VirtualBox::OpenRemoteSession (ISession *aSession,
                                            INPTR GUIDPARAM aMachineId,
                                            INPTR BSTR aType,
                                            INPTR BSTR aEnvironment,
                                            IProgress **aProgress)
{
    if (!aSession || !aType)
        return E_INVALIDARG;
    if (!aProgress)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    Guid id = aMachineId;
    ComObjPtr <Machine> machine;

    HRESULT rc = findMachine (id, true /* setError */, &machine);
    CheckComRCReturnRC (rc);

    /* check the session state */
    SessionState_T state;
    rc = aSession->COMGETTER(State) (&state);
    CheckComRCReturnRC (rc);

    if (state != SessionState_Closed)
        return setError (E_INVALIDARG,
            tr ("The given session is already open or being opened"));

    /* get the IInternalSessionControl interface */
    ComPtr <IInternalSessionControl> control = aSession;
    ComAssertMsgRet (!!control, ("No IInternalSessionControl interface"),
                     E_INVALIDARG);

    /* create a progress object */
    ComObjPtr <Progress> progress;
    progress.createObject();
    progress->init (this, static_cast <IMachine *> (machine),
                    Bstr (tr ("Spawning session")),
                    FALSE /* aCancelable */);

    rc = machine->openRemoteSession (control, aType, aEnvironment, progress);

    if (SUCCEEDED (rc))
    {
        progress.queryInterfaceTo (aProgress);

        /* fire an event */
        onSessionStateChange (aMachineId, SessionState_Spawning);
    }

    return rc;
}

/**
 *  @note Locks objects!
 */
STDMETHODIMP VirtualBox::OpenExistingSession (ISession *aSession,
                                              INPTR GUIDPARAM aMachineId)
{
    if (!aSession)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    Guid id = aMachineId;
    ComObjPtr <Machine> machine;

    HRESULT rc = findMachine (id, true /* setError */, &machine);
    CheckComRCReturnRC (rc);

    /* check the session state */
    SessionState_T state;
    rc = aSession->COMGETTER(State) (&state);
    CheckComRCReturnRC (rc);

    if (state != SessionState_Closed)
        return setError (E_INVALIDARG,
            tr ("The given session is already open or being opened"));

    /* get the IInternalSessionControl interface */
    ComPtr <IInternalSessionControl> control = aSession;
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
    LogFlowThisFunc (("aCallback=%p\n", aCallback));

    if (!aCallback)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);
    mData.mCallbacks.push_back (CallbackList::value_type (aCallback));

    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP VirtualBox::UnregisterCallback (IVirtualBoxCallback *aCallback)
{
    if (!aCallback)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    HRESULT rc = S_OK;

    AutoLock alock (this);

    CallbackList::iterator it;
    it = std::find (mData.mCallbacks.begin(),
                    mData.mCallbacks.end(),
                    CallbackList::value_type (aCallback));
    if (it == mData.mCallbacks.end())
        rc = E_INVALIDARG;
    else
        mData.mCallbacks.erase (it);

    LogFlowThisFunc (("aCallback=%p, rc=%08X\n", aCallback, rc));
    return rc;
}

STDMETHODIMP VirtualBox::WaitForPropertyChange (INPTR BSTR aWhat, ULONG aTimeout,
                                                BSTR *aChanged, BSTR *aValues)
{
    return E_NOTIMPL;
}

STDMETHODIMP VirtualBox::SaveSettings()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    return saveSettings();
}

STDMETHODIMP VirtualBox::SaveSettingsWithBackup (BSTR *aBakFileName)
{
    if (!aBakFileName)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* saveSettings() needs write lock */
    AutoLock alock (this);

    /* perform backup only when there was auto-conversion */
    if (mData.mSettingsFileVersion != VBOX_XML_VERSION_FULL)
    {
        Bstr bakFileName;

        HRESULT rc = backupSettingsFile (mData.mCfgFile.mName,
                                         mData.mSettingsFileVersion,
                                         bakFileName);
        CheckComRCReturnRC (rc);

        bakFileName.cloneTo (aBakFileName);
    }

    return saveSettings();
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
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    if (autoCaller.state() != Ready)
    {
        LogWarningFunc (("VirtualBox has been uninitialized (state=%d), "
                         "the event is discarded!\n",
                         autoCaller.state()));
        return S_OK;
    }

    AssertReturn (event, E_FAIL);
    AssertReturn (mAsyncEventQ, E_FAIL);

    AutoLock alock (mAsyncEventQLock);
    if (mAsyncEventQ->postEvent (event))
        return S_OK;

    return E_FAIL;
}

/**
 *  Helper method to add a progress to the global collection of pending
 *  operations.
 *
 *  @param   aProgress  operation to add to the collection
 *  @return  COM status code
 *
 *  @note Locks this object for writing.
 */
HRESULT VirtualBox::addProgress (IProgress *aProgress)
{
    if (!aProgress)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);
    mData.mProgressOperations.push_back (aProgress);
    return S_OK;
}

/**
 *  Helper method to remove the progress from the global collection of pending
 *  operations. Usualy gets called upon progress completion.
 *
 *  @param   aId    UUID of the progress operation to remove
 *  @return  COM status code
 *
 *  @note Locks this object for writing.
 */
HRESULT VirtualBox::removeProgress (INPTR GUIDPARAM aId)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ComPtr <IProgress> progress;

    AutoLock alock (this);

    for (ProgressList::iterator it = mData.mProgressOperations.begin();
         it != mData.mProgressOperations.end();
         ++ it)
    {
        Guid id;
        (*it)->COMGETTER(Id) (id.asOutParam());
        if (id == aId)
        {
            mData.mProgressOperations.erase (it);
            return S_OK;
        }
    }

    AssertFailed(); /* should never happen */

    return E_FAIL;
}

#ifdef RT_OS_WINDOWS

struct StartSVCHelperClientData
{
    ComObjPtr <VirtualBox> that;
    ComObjPtr <Progress> progress;
    bool privileged;
    VirtualBox::SVCHelperClientFunc func;
    void *user;
};

/**
 *  Helper method to that starts a worker thread that:
 *  - creates a pipe communication channel using SVCHlpClient;
 *  - starts a SVC Helper process that will inherit this channel;
 *  - executes the supplied function by passing it the created SVCHlpClient
 *    and opened instance to communicate to the Helper process and the given
 *    Progress object.
 *
 *  The user function is supposed to communicate to the helper process
 *  using the \a aClient argument to do the requested job and optionally expose
 *  the prgress through the \a aProgress object. The user function should never
 *  call notifyComplete() on it: this will be done automatically using the
 *  result code returned by the function.
 *
 *  Before the user function is stared, the communication channel passed to in
 *  the \a aClient argument, is fully set up, the function should start using
 *  it's write() and read() methods directly.
 *
 *  The \a aVrc parameter of the user function may be used to return an error
 *  code if it is related to communication errors (for example, returned by
 *  the SVCHlpClient members when they fail). In this case, the correct error
 *  message using this value will be reported to the caller. Note that the
 *  value of \a aVrc is inspected only if the user function itself returns
 *  a success.
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
 *  @param  aPrivileged |true| to start the SVC Hepler process as a privlieged
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
    AssertReturn (aFunc, E_POINTER);
    AssertReturn (aProgress, E_POINTER);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* create the SVCHelperClientThread() argument */
    std::auto_ptr <StartSVCHelperClientData>
        d (new StartSVCHelperClientData());
    AssertReturn (d.get(), E_OUTOFMEMORY);

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

    ComAssertMsgRCRet (vrc, ("Could not create SVCHelper thread (%Vrc)\n", vrc),
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
        AssertBreak (d.get(), rc = E_POINTER);
        AssertReturn (!d->progress.isNull(), E_POINTER);

        /* protect VirtualBox from uninitialization */
        AutoCaller autoCaller (d->that);
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
        vrc = client.create (Utf8StrFmt ("VirtualBox\\SVCHelper\\{%Vuuid}",
                                         id.raw()));
        if (VBOX_FAILURE (vrc))
        {
            rc = setError (E_FAIL,
                tr ("Could not create the communication channel (%Vrc)"), vrc);
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
                        tr ("Operatiion cancelled by the user"));
                else
                    rc = setError (E_FAIL,
                        tr ("Could not launch a privileged process '%s' (%Vrc)"),
                        exePath, vrc2);
                break;
            }
        }
        else
        {
            const char *args[] = { exePath, "/Helper", client.name(), 0 };
            vrc = RTProcCreate (exePath, args, RTENV_DEFAULT, 0, &pid);
            if (VBOX_FAILURE (vrc))
            {
                rc = setError (E_FAIL,
                    tr ("Could not launch a process '%s' (%Vrc)"), exePath, vrc);
                break;
            }
        }

        /* wait for the client to connect */
        vrc = client.connect();
        if (VBOX_SUCCESS (vrc))
        {
            /* start the user supplied function */
            rc = d->func (&client, d->progress, d->user, &vrc);
            userFuncCalled = true;
        }

        /* send the termination signal to the process anyway */
        {
            int vrc2 = client.write (SVCHlpMsg::Null);
            if (VBOX_SUCCESS (vrc))
                vrc = vrc2;
        }

        if (SUCCEEDED (rc) && VBOX_FAILURE (vrc))
        {
            rc = setError (E_FAIL,
                tr ("Could not operate the communication channel (%Vrc)"), vrc);
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
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    AssertReturn (mWatcherData.mThread != NIL_RTTHREAD, (void) 0);

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
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    /// @todo (dmik) Win32?
#ifndef RT_OS_WINDOWS
    AutoLock alock (this);
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

    void handleCallback (const ComPtr <IVirtualBoxCallback> &aCallback)
    {
        switch (what)
        {
            case DataChanged:
                LogFlow (("OnMachineDataChange: id={%Vuuid}\n", id.ptr()));
                aCallback->OnMachineDataChange (id);
                break;

            case StateChanged:
                LogFlow (("OnMachineStateChange: id={%Vuuid}, state=%d\n",
                          id.ptr(), state));
                aCallback->OnMachineStateChange (id, state);
                break;

            case Registered:
                LogFlow (("OnMachineRegistered: id={%Vuuid}, registered=%d\n",
                          id.ptr(), registered));
                aCallback->OnMachineRegistered (id, registered);
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
void VirtualBox::onMachineStateChange (const Guid &aId, MachineState_T aState)
{
    postEvent (new MachineEvent (this, aId, aState));
}

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onMachineDataChange (const Guid &aId)
{
    postEvent (new MachineEvent (this, aId));
}

/**
 *  @note Locks this object for reading.
 */
BOOL VirtualBox::onExtraDataCanChange (const Guid &aId, INPTR BSTR aKey, INPTR BSTR aValue,
                                       Bstr &aError)
{
    LogFlowThisFunc (("machine={%s} aKey={%ls} aValue={%ls}\n",
                      aId.toString().raw(), aKey, aValue));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    CallbackList list;
    {
        AutoReaderLock alock (this);
        list = mData.mCallbacks;
    }

    BOOL allowChange = TRUE;
    CallbackList::iterator it = list.begin();
    while ((it != list.end()) && allowChange)
    {
        HRESULT rc = (*it++)->OnExtraDataCanChange (aId, aKey, aValue,
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

    LogFlowThisFunc (("allowChange=%RTbool\n", allowChange));
    return allowChange;
}

/** Event for onExtraDataChange() */
struct ExtraDataEvent : public VirtualBox::CallbackEvent
{
    ExtraDataEvent (VirtualBox *aVB, const Guid &aMachineId,
                    INPTR BSTR aKey, INPTR BSTR aVal)
        : CallbackEvent (aVB), machineId (aMachineId)
        , key (aKey), val (aVal)
        {}

    void handleCallback (const ComPtr <IVirtualBoxCallback> &aCallback)
    {
        LogFlow (("OnExtraDataChange: machineId={%Vuuid}, key='%ls', val='%ls'\n",
                  machineId.ptr(), key.raw(), val.raw()));
        aCallback->OnExtraDataChange (machineId, key, val);
    }

    Guid machineId;
    Bstr key, val;
};

/**
 *  @note Doesn't lock any object.
 */
void VirtualBox::onExtraDataChange (const Guid &aId, INPTR BSTR aKey, INPTR BSTR aValue)
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

    void handleCallback (const ComPtr <IVirtualBoxCallback> &aCallback)
    {
        LogFlow (("OnSessionStateChange: machineId={%Vuuid}, sessionState=%d\n",
                  machineId.ptr(), sessionState));
        aCallback->OnSessionStateChange (machineId, sessionState);
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

    void handleCallback (const ComPtr <IVirtualBoxCallback> &aCallback)
    {
        switch (what)
        {
            case Taken:
                LogFlow (("OnSnapshotTaken: machineId={%Vuuid}, snapshotId={%Vuuid}\n",
                          machineId.ptr(), snapshotId.ptr()));
                aCallback->OnSnapshotTaken (machineId, snapshotId);
                break;

            case Discarded:
                LogFlow (("OnSnapshotDiscarded: machineId={%Vuuid}, snapshotId={%Vuuid}\n",
                          machineId.ptr(), snapshotId.ptr()));
                aCallback->OnSnapshotDiscarded (machineId, snapshotId);
                break;

            case Changed:
                LogFlow (("OnSnapshotChange: machineId={%Vuuid}, snapshotId={%Vuuid}\n",
                          machineId.ptr(), snapshotId.ptr()));
                aCallback->OnSnapshotChange (machineId, snapshotId);
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

/**
 *  @note Locks this object for reading.
 */
ComObjPtr <GuestOSType> VirtualBox::getUnknownOSType()
{
    ComObjPtr <GuestOSType> type;

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), type);

    AutoReaderLock alock (this);

    /* unknown type must always be the first */
    ComAssertRet (mData.mGuestOSTypes.size() > 0, type);

    type = mData.mGuestOSTypes.front();
    return type;
}

/**
 *  Returns the list of opened machines (i.e. machines having direct sessions
 *  opened by client processes).
 *
 *  @note the returned list contains smart pointers. So, clear it as soon as
 *  it becomes no more necessary to release instances.
 *  @note it can be possible that a session machine from the list has been
 *  already uninitialized, so a) lock the instance and b) chheck for
 *  instance->isReady() return value before manipulating the object directly
 *  (i.e. not through COM methods).
 *
 *  @note Locks objects for reading.
 */
void VirtualBox::getOpenedMachines (SessionMachineVector &aVector)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    std::list <ComObjPtr <SessionMachine> > list;

    {
        AutoReaderLock alock (this);

        for (MachineList::iterator it = mData.mMachines.begin();
             it != mData.mMachines.end();
             ++ it)
        {
            ComObjPtr <SessionMachine> sm = (*it)->sessionMachine();
            /* SessionMachine is null when there are no open sessions */
            if (!sm.isNull())
                list.push_back (sm);
        }
    }

    aVector = SessionMachineVector (list.begin(), list.end());
    return;
}

/**
 *  Helper to find machines that use the given DVD image.
 *
 *  This method also checks whether an existing snapshot refers to the given
 *  image. However, only the machine ID is returned in this case, not IDs of
 *  individual snapshots.
 *
 *  @param aId          Image ID to get usage for.
 *  @param aUsage       Type of the check.
 *  @param aMachineIDs  Where to store the list of machine IDs (can be NULL)
 *
 *  @return @c true if at least one machine or its snapshot found and @c false
 *  otherwise.
 *
 *  @note For now, we just scan all the machines. We can optimize this later
 *  if required by adding the corresponding field to DVDImage and requiring all
 *  IDVDImage instances to be DVDImage objects.
 *
 *  @note Locks objects for reading.
 */
bool VirtualBox::getDVDImageUsage (const Guid &aId,
                                   ResourceUsage_T aUsage,
                                   Bstr *aMachineIDs)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), FALSE);

    typedef std::set <Guid> Set;
    Set idSet;

    {
        AutoReaderLock alock (this);

        for (MachineList::const_iterator mit = mData.mMachines.begin();
             mit != mData.mMachines.end();
             ++ mit)
        {
            ComObjPtr <Machine> m = *mit;

            AutoLimitedCaller machCaller (m);
            AssertComRC (machCaller.rc());

            /* ignore inaccessible machines */
            if (machCaller.state() == Machine::Ready)
            {
                if (m->isDVDImageUsed (aId, aUsage))
                {
                    /* if not interested in the list, return shortly */
                    if (aMachineIDs == NULL)
                        return true;

                    idSet.insert (m->uuid());
                }
            }
        }
    }

    if (aMachineIDs)
    {
        if (!idSet.empty())
        {
            /* convert to a string of UUIDs */
            char *idList = (char *) RTMemTmpAllocZ (RTUUID_STR_LENGTH * idSet.size());
            char *idListPtr = idList;
            for (Set::iterator it = idSet.begin(); it != idSet.end(); ++ it)
            {
                RTUuidToStr (*it, idListPtr, RTUUID_STR_LENGTH);
                idListPtr += RTUUID_STR_LENGTH - 1;
                /* replace EOS with a space char */
                *(idListPtr ++) = ' ';
            }
            Assert (int (idListPtr - idList) == int (RTUUID_STR_LENGTH * idSet.size()));
            /* remove the trailing space */
            *(-- idListPtr) = 0;
            /* copy the string */
            *aMachineIDs = idList;
            RTMemTmpFree (idList);
        }
        else
        {
            (*aMachineIDs).setNull();
        }
    }

    return !idSet.empty();
}

/**
 *  Helper to find machines that use the given Floppy image.
 *
 *  This method also checks whether an existing snapshot refers to the given
 *  image. However, only the machine ID is returned in this case, not IDs of
 *  individual snapshots.
 *
 *  @param aId          Image ID to get usage for.
 *  @param aUsage       Type of the check.
 *  @param aMachineIDs  Where to store the list of machine IDs (can be NULL)
 *
 *  @return @c true if at least one machine or its snapshot found and @c false
 *  otherwise.
 *
 *  @note For now, we just scan all the machines. We can optimize this later
 *  if required by adding the corresponding field to FloppyImage and requiring all
 *  FloppyImage instances to be FloppyImage objects.
 *
 *  @note Locks objects for reading.
 */
bool VirtualBox::getFloppyImageUsage (const Guid &aId,
                                      ResourceUsage_T aUsage,
                                      Bstr *aMachineIDs)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), FALSE);

    typedef std::set <Guid> Set;
    Set idSet;

    {
        AutoReaderLock alock (this);

        for (MachineList::const_iterator mit = mData.mMachines.begin();
             mit != mData.mMachines.end();
             ++ mit)
        {
            ComObjPtr <Machine> m = *mit;

            AutoLimitedCaller machCaller (m);
            AssertComRC (machCaller.rc());

            /* ignore inaccessible machines */
            if (machCaller.state() == Machine::Ready)
            {
                if (m->isFloppyImageUsed (aId, aUsage))
                {
                    /* if not interested in the list, return shortly */
                    if (aMachineIDs == NULL)
                        return true;

                    idSet.insert (m->uuid());
                }
            }
        }
    }

    if (aMachineIDs)
    {
        if (!idSet.empty())
        {
            /* convert to a string of UUIDs */
            char *idList = (char *) RTMemTmpAllocZ (RTUUID_STR_LENGTH * idSet.size());
            char *idListPtr = idList;
            for (Set::iterator it = idSet.begin(); it != idSet.end(); ++ it)
            {
                RTUuidToStr (*it, idListPtr, RTUUID_STR_LENGTH);
                idListPtr += RTUUID_STR_LENGTH - 1;
                /* replace EOS with a space char */
                *(idListPtr ++) = ' ';
            }
            Assert (int (idListPtr - idList) == int (RTUUID_STR_LENGTH * idSet.size()));
            /* remove the trailing space */
            *(-- idListPtr) = 0;
            /* copy the string */
            *aMachineIDs = idList;
            RTMemTmpFree (idList);
        }
        else
        {
            (*aMachineIDs).setNull();
        }
    }

    return !idSet.empty();
}

/**
 *  Tries to calculate the relative path of the given absolute path using the
 *  directory of the VirtualBox settings file as the base directory.
 *
 *  @param  aPath   absolute path to calculate the relative path for
 *  @param  aResult where to put the result (used only when it's possible to
 *                  make a relative path from the given absolute path;
 *                  otherwise left untouched)
 *
 *  @note Doesn't lock any object.
 */
void VirtualBox::calculateRelativePath (const char *aPath, Utf8Str &aResult)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    /* no need to lock since mHomeDir is const */

    Utf8Str settingsDir = mData.mHomeDir;

    if (RTPathStartsWith (aPath, settingsDir))
    {
        /* when assigning, we create a separate Utf8Str instance because both
         * aPath and aResult can point to the same memory location when this
         * func is called (if we just do aResult = aPath, aResult will be freed
         * first, and since its the same as aPath, an attempt to copy garbage
         * will be made. */
        aResult = Utf8Str (aPath + settingsDir.length() + 1);
    }
}

// private methods
/////////////////////////////////////////////////////////////////////////////

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
 *      S_OK when found or E_INVALIDARG when not found
 *
 *  @note Locks this object for reading.
 */
HRESULT VirtualBox::findMachine (const Guid &aId, bool aSetError,
                                 ComObjPtr <Machine> *aMachine /* = NULL */)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    bool found = false;

    {
        AutoReaderLock alock (this);

        for (MachineList::iterator it = mData.mMachines.begin();
             !found && it != mData.mMachines.end();
             ++ it)
        {
            /* sanity */
            AutoLimitedCaller machCaller (*it);
            AssertComRC (machCaller.rc());

            found = (*it)->uuid() == aId;
            if (found && aMachine)
                *aMachine = *it;
        }
    }

    HRESULT rc = found ? S_OK : E_INVALIDARG;

    if (aSetError && !found)
    {
        setError (E_INVALIDARG,
            tr ("Could not find a registered machine with UUID {%Vuuid}"),
            aId.raw());
    }

    return rc;
}

/**
 *  Searches for a HardDisk object with the given ID or location specification
 *  in the collection of registered hard disks. If both ID and location are
 *  specified, the first object that matches either of them (not necessarily
 *  both) is returned.
 *
 *  @param aId          ID of the hard disk (NULL when unused)
 *  @param aLocation    full location specification (NULL when unused)
 *  @param aSetError    if TRUE, the appropriate error info is set in case when
 *                      the disk is not found and only one search criteria (ID
 *                      or file name) is specified.
 *  @param aHardDisk    where to store the found hard disk object (can be NULL)
 *
 *  @return
 *      S_OK when found or E_INVALIDARG when not found
 *
 *  @note Locks objects for reading!
 */
HRESULT VirtualBox::
findHardDisk (const Guid *aId, const BSTR aLocation,
              bool aSetError, ComObjPtr <HardDisk> *aHardDisk /* = NULL */)
{
    ComAssertRet (aId || aLocation, E_INVALIDARG);

    AutoReaderLock alock (this);

    /* first lookup the map by UUID if UUID is provided */
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

    /* then iterate and find by location */
    bool found = false;
    if (aLocation)
    {
        Utf8Str location = aLocation;

        for (HardDiskMap::const_iterator it = mData.mHardDiskMap.begin();
             !found && it != mData.mHardDiskMap.end();
             ++ it)
        {
            const ComObjPtr <HardDisk> &hd = (*it).second;
            AutoReaderLock hdLock (hd);

            if (hd->storageType() == HardDiskStorageType_VirtualDiskImage ||
                hd->storageType() == HardDiskStorageType_VMDKImage ||
                hd->storageType() == HardDiskStorageType_VHDImage)
            {
                /* locations of VDI and VMDK hard disks for now are just
                 * file paths */
                found = RTPathCompare (location,
                                       Utf8Str (hd->toString
                                                (false /* aShort */))) == 0;
            }
            else
            {
                found = aLocation == hd->toString (false /* aShort */);
            }

            if (found && aHardDisk)
                *aHardDisk = hd;
        }
    }

    HRESULT rc = found ? S_OK : E_INVALIDARG;

    if (aSetError && !found)
    {
        if (aId && !aLocation)
            setError (rc, tr ("Could not find a registered hard disk "
                              "with UUID {%Vuuid}"), aId->raw());
        else if (aLocation && !aId)
            setError (rc, tr ("Could not find a registered hard disk "
                              "with location '%ls'"), aLocation);
    }

    return rc;
}

/**
 *  @deprecated Use #findHardDisk() instead.
 *
 *  Searches for a HVirtualDiskImage object with the given ID or file path in the
 *  collection of registered hard disks. If both ID and file path are specified,
 *  the first object that matches either of them (not necessarily both)
 *  is returned.
 *
 *  @param aId          ID of the hard disk (NULL when unused)
 *  @param filePathFull full path to the image file (NULL when unused)
 *  @param aSetError    if TRUE, the appropriate error info is set in case when
 *                      the disk is not found and only one search criteria (ID
 *                      or file name) is specified.
 *  @param aHardDisk    where to store the found hard disk object (can be NULL)
 *
 *  @return
 *      S_OK when found or E_INVALIDARG when not found
 *
 *  @note Locks objects for reading!
 */
HRESULT VirtualBox::
findVirtualDiskImage (const Guid *aId, const BSTR aFilePathFull,
                      bool aSetError, ComObjPtr <HVirtualDiskImage> *aImage /* = NULL */)
{
    ComAssertRet (aId || aFilePathFull, E_INVALIDARG);

    AutoReaderLock alock (this);

    /* first lookup the map by UUID if UUID is provided */
    if (aId)
    {
        HardDiskMap::const_iterator it = mData.mHardDiskMap.find (*aId);
        if (it != mData.mHardDiskMap.end())
        {
            AutoReaderLock hdLock ((*it).second);
            if ((*it).second->storageType() == HardDiskStorageType_VirtualDiskImage)
            {
                if (aImage)
                    *aImage = (*it).second->asVDI();
                return S_OK;
            }
        }
    }

    /* then iterate and find by name */
    bool found = false;
    if (aFilePathFull)
    {
        for (HardDiskMap::const_iterator it = mData.mHardDiskMap.begin();
             !found && it != mData.mHardDiskMap.end();
             ++ it)
        {
            const ComObjPtr <HardDisk> &hd = (*it).second;
            AutoReaderLock hdLock (hd);
            if (hd->storageType() != HardDiskStorageType_VirtualDiskImage)
                continue;

            found = RTPathCompare (Utf8Str (aFilePathFull),
                                   Utf8Str (hd->asVDI()->filePathFull())) == 0;
            if (found && aImage)
                *aImage = hd->asVDI();
        }
    }

    HRESULT rc = found ? S_OK : E_INVALIDARG;

    if (aSetError && !found)
    {
        if (aId && !aFilePathFull)
            setError (rc, tr ("Could not find a registered VDI hard disk "
                              "with UUID {%Vuuid}"), aId->raw());
        else if (aFilePathFull && !aId)
            setError (rc, tr ("Could not find a registered VDI hard disk "
                              "with the file path '%ls'"), aFilePathFull);
    }

    return rc;
}

/**
 *  Searches for a DVDImage object with the given ID or file path in the
 *  collection of registered DVD images. If both ID and file path are specified,
 *  the first object that matches either of them (not necessarily both)
 *  is returned.
 *
 *  @param aId
 *      ID of the DVD image (unused when NULL)
 *  @param aFilePathFull
 *      full path to the image file (unused when NULL)
 *  @param aSetError
 *      if TRUE, the appropriate error info is set in case when the image is not
 *      found and only one search criteria (ID or file name) is specified.
 *  @param aImage
 *      where to store the found DVD image object (can be NULL)
 *
 *  @return
 *      S_OK when found or E_INVALIDARG when not found
 *
 *  @note Locks this object for reading.
 */
HRESULT VirtualBox::findDVDImage (const Guid *aId, const BSTR aFilePathFull,
                                  bool aSetError,
                                  ComObjPtr <DVDImage> *aImage /* = NULL */)
{
    ComAssertRet (aId || aFilePathFull, E_INVALIDARG);

    bool found = false;

    {
        AutoReaderLock alock (this);

        for (DVDImageList::const_iterator it = mData.mDVDImages.begin();
             !found && it != mData.mDVDImages.end();
             ++ it)
        {
            /* DVDImage fields are constant, so no need to lock  */
            found = (aId && (*it)->id() == *aId) ||
                    (aFilePathFull &&
                     RTPathCompare (Utf8Str (aFilePathFull),
                                    Utf8Str ((*it)->filePathFull())) == 0);
            if (found && aImage)
                *aImage = *it;
        }
    }

    HRESULT rc = found ? S_OK : E_INVALIDARG;

    if (aSetError && !found)
    {
        if (aId && !aFilePathFull)
            setError (rc, tr ("Could not find a registered CD/DVD image "
                              "with UUID {%s}"), aId->toString().raw());
        else if (aFilePathFull && !aId)
            setError (rc, tr ("Could not find a registered CD/DVD image "
                              "with the file path '%ls'"), aFilePathFull);
    }

    return rc;
}

/**
 *  Searches for a FloppyImage object with the given ID or file path in the
 *  collection of registered floppy images. If both ID and file path are specified,
 *  the first object that matches either of them (not necessarily both)
 *  is returned.
 *
 *  @param aId
 *      ID of the floppy image (unused when NULL)
 *  @param aFilePathFull
 *      full path to the image file (unused when NULL)
 *  @param aSetError
 *      if TRUE, the appropriate error info is set in case when the image is not
 *      found and only one search criteria (ID or file name) is specified.
 *  @param aImage
 *      where to store the found floppy image object (can be NULL)
 *
 *  @return
 *      S_OK when found or E_INVALIDARG when not found
 *
 *  @note Locks this object for reading.
 */
HRESULT VirtualBox::findFloppyImage (const Guid *aId, const BSTR aFilePathFull,
                                     bool aSetError,
                                     ComObjPtr <FloppyImage> *aImage /* = NULL */)
{
    ComAssertRet (aId || aFilePathFull, E_INVALIDARG);

    bool found = false;

    {
        AutoReaderLock alock (this);

        for (FloppyImageList::iterator it = mData.mFloppyImages.begin();
             !found && it != mData.mFloppyImages.end();
             ++ it)
        {
            /* FloppyImage fields are constant, so no need to lock  */
            found = (aId && (*it)->id() == *aId) ||
                    (aFilePathFull &&
                     RTPathCompare (Utf8Str (aFilePathFull),
                                    Utf8Str ((*it)->filePathFull())) == 0);
            if (found && aImage)
                *aImage = *it;
        }
    }

    HRESULT rc = found ? S_OK : E_INVALIDARG;

    if (aSetError && !found)
    {
        if (aId && !aFilePathFull)
            setError (rc, tr ("Could not find a registered floppy image "
                              "with UUID {%s}"), aId->toString().raw());
        else if (aFilePathFull && !aId)
            setError (rc, tr ("Could not find a registered floppy image "
                              "with the file path '%ls'"), aFilePathFull);
    }

    return rc;
}

/**
 *  When \a aHardDisk is not NULL, searches for an object equal to the given
 *  hard disk in the collection of registered hard disks, or, if the given hard
 *  disk is HVirtualDiskImage, for an object with the given file path in the
 *  collection of all registered non-hard disk images (DVDs and floppies).
 *  Other parameters are unused.
 *
 *  When \a aHardDisk is NULL, searches for an object with the given ID or file
 *  path in the collection of all registered images (VDIs, DVDs and floppies).
 *  If both ID and file path are specified, matching either of them will satisfy
 *  the search.
 *
 *  If a matching object is found, this method returns E_INVALIDARG and sets the
 *  appropriate error info. Otherwise, S_OK is returned.
 *
 *  @param aHardDisk        hard disk object to check against registered media
 *                          (NULL when unused)
 *  @param aId              UUID of the media to check (NULL when unused)
 *  @param aFilePathFull    full path to the image file (NULL when unused)
 *
 *  @note Locks objects!
 */
HRESULT VirtualBox::checkMediaForConflicts (HardDisk *aHardDisk,
                                            const Guid *aId,
                                            const BSTR aFilePathFull)
{
    AssertReturn (aHardDisk || aId || aFilePathFull, E_FAIL);

    HRESULT rc = S_OK;

    AutoReaderLock alock (this);

    if (aHardDisk)
    {
        for (HardDiskMap::const_iterator it = mData.mHardDiskMap.begin();
             it != mData.mHardDiskMap.end();
             ++ it)
        {
            const ComObjPtr <HardDisk> &hd = (*it).second;
            if (hd->sameAs (aHardDisk))
                return setError (E_INVALIDARG,
                    tr ("A hard disk with UUID {%Vuuid} or with the same properties "
                        "('%ls') is already registered"),
                        aHardDisk->id().raw(), aHardDisk->toString().raw());
        }

        aId = &aHardDisk->id();
        if (aHardDisk->storageType() == HardDiskStorageType_VirtualDiskImage)
#if !defined (VBOX_WITH_XPCOM)
            /// @todo (dmik) stupid BSTR declaration lacks the BCSTR counterpart
            const_cast <BSTR> (aFilePathFull) = aHardDisk->asVDI()->filePathFull();
#else
            aFilePathFull = aHardDisk->asVDI()->filePathFull();
#endif
    }

    bool found = false;

    if (aId || aFilePathFull) do
    {
        if (!aHardDisk)
        {
            rc = findHardDisk (aId, aFilePathFull, false /* aSetError */);
            found = SUCCEEDED (rc);
            if (found)
                break;
        }

        rc = findDVDImage (aId, aFilePathFull, false /* aSetError */);
        found = SUCCEEDED (rc);
        if (found)
            break;

        rc = findFloppyImage (aId, aFilePathFull, false /* aSetError */);
        found = SUCCEEDED (rc);
        if (found)
            break;
    }
    while (0);

    if (found)
    {
        if (aId && !aFilePathFull)
            rc = setError (E_INVALIDARG,
                tr ("A disk image with UUID {%Vuuid} is already registered"),
                aId->raw());
        else if (aFilePathFull && !aId)
            rc = setError (E_INVALIDARG,
                tr ("A disk image with file path '%ls' is already registered"),
                aFilePathFull);
        else
            rc = setError (E_INVALIDARG,
                tr ("A disk image with UUID {%Vuuid} or file path '%ls' "
                    "is already registered"), aId->raw(), aFilePathFull);
    }
    else
        rc = S_OK;

    return rc;
}

/**
 *  Reads in the machine definitions from the configuration loader
 *  and creates the relevant objects.
 *
 *  @param aGlobal  <Global> node.
 *
 *  @note Can be called only from #init().
 *  @note Doesn't lock anything.
 */
HRESULT VirtualBox::loadMachines (const settings::Key &aGlobal)
{
    using namespace settings;

    AutoCaller autoCaller (this);
    AssertReturn (autoCaller.state() == InInit, E_FAIL);

    HRESULT rc = S_OK;

    Key::List machines = aGlobal.key ("MachineRegistry").keys ("MachineEntry");
    for (Key::List::const_iterator it = machines.begin();
         it != machines.end(); ++ it)
    {
        /* required */
        Guid uuid = (*it).value <Guid> ("uuid");
        /* required */
        Bstr src = (*it).stringValue ("src");

        /* create a new machine object */
        ComObjPtr <Machine> machine;
        rc = machine.createObject();
        if (SUCCEEDED (rc))
        {
            /* initialize the machine object and register it */
            rc = machine->init (this, src, Machine::Init_Registered,
                                NULL, FALSE, &uuid);
            if (SUCCEEDED (rc))
                rc = registerMachine (machine);
        }
    }

    return rc;
}

/**
 *  Reads in the disk registration entries from the global settings file
 *  and creates the relevant objects
 *
 *  @param aGlobal  <Global> node
 *
 *  @note Can be called only from #init().
 *  @note Doesn't lock anything.
 */
HRESULT VirtualBox::loadDisks (const settings::Key &aGlobal)
{
    using namespace settings;

    AutoCaller autoCaller (this);
    AssertReturn (autoCaller.state() == InInit, E_FAIL);

    HRESULT rc = S_OK;

    Key registry = aGlobal.key ("DiskRegistry");

    const char *kMediaNodes[] = { "HardDisks", "DVDImages", "FloppyImages" };

    for (size_t n = 0; n < ELEMENTS (kMediaNodes); ++ n)
    {
        /* All three media nodes are optional */
        Key node = registry.findKey (kMediaNodes [n]);
        if (node.isNull())
            continue;

        if (n == 0)
        {
            /* HardDisks node */
            rc = loadHardDisks (node);
            continue;
        }

        Key::List images = node.keys ("Image");
        for (Key::List::const_iterator it = images.begin();
             it != images.end(); ++ it)
        {
            /* required */
            Guid uuid = (*it).value <Guid> ("uuid");
            /* required */
            Bstr src = (*it).stringValue ("src");

            switch (n)
            {
                case 1: /* DVDImages */
                {
                    ComObjPtr <DVDImage> image;
                    image.createObject();
                    rc = image->init (this, src, TRUE /* isRegistered */, uuid);
                    if (SUCCEEDED (rc))
                        rc = registerDVDImage (image, TRUE /* aOnStartUp */);

                    break;
                }
                case 2: /* FloppyImages */
                {
                    ComObjPtr <FloppyImage> image;
                    image.createObject();
                    rc = image->init (this, src, TRUE /* isRegistered */, uuid);
                    if (SUCCEEDED (rc))
                        rc = registerFloppyImage (image, TRUE /* aOnStartUp */);

                    break;
                }
                default:
                    AssertFailed();
            }

            CheckComRCBreakRC (rc);
        }

        CheckComRCBreakRC (rc);
    }

    return rc;
}

/**
 *  Loads all hard disks from the given <HardDisks> node.
 *  Note that all loaded hard disks register themselves within this VirtualBox.
 *
 *  @param aNode        <HardDisks> node.
 *
 *  @note Can be called only from #init().
 *  @note Doesn't lock anything.
 */
HRESULT VirtualBox::loadHardDisks (const settings::Key &aNode)
{
    using namespace settings;

    AutoCaller autoCaller (this);
    AssertReturn (autoCaller.state() == InInit, E_FAIL);

    AssertReturn (!aNode.isNull(), E_INVALIDARG);

    HRESULT rc = S_OK;

    Key::List disks = aNode.keys ("HardDisk");
    for (Key::List::const_iterator it = disks.begin();
         it != disks.end(); ++ it)
    {
        Key storageNode;

        /* detect the type of the hard disk (either one of VirtualDiskImage,
         * ISCSIHardDisk, VMDKImage or HCustomHardDisk) */

        do
        {
            storageNode = (*it).findKey ("VirtualDiskImage");
            if (!storageNode.isNull())
            {
                ComObjPtr <HVirtualDiskImage> vdi;
                vdi.createObject();
                rc = vdi->init (this, NULL, (*it), storageNode);
                break;
            }

            storageNode = (*it).findKey ("ISCSIHardDisk");
            if (!storageNode.isNull())
            {
                ComObjPtr <HISCSIHardDisk> iscsi;
                iscsi.createObject();
                rc = iscsi->init (this, (*it), storageNode);
                break;
            }

            storageNode = (*it).findKey ("VMDKImage");
            if (!storageNode.isNull())
            {
                ComObjPtr <HVMDKImage> vmdk;
                vmdk.createObject();
                rc = vmdk->init (this, NULL, (*it), storageNode);
                break;
            }

            storageNode = (*it).findKey ("CustomHardDisk");
            if (!storageNode.isNull())
            {
                ComObjPtr <HCustomHardDisk> custom;
                custom.createObject();
                rc = custom->init (this, NULL, (*it), storageNode);
                break;
            }

            storageNode = (*it).findKey ("VHDImage");
            if (!storageNode.isNull())
            {
                ComObjPtr <HVHDImage> vhd;
                vhd.createObject();
                rc = vhd->init (this, NULL, (*it), storageNode);
                break;
            }

            ComAssertMsgFailedBreak (("No valid hard disk storage node!\n"),
                                     rc = E_FAIL);
        }
        while (0);
    }

    return rc;
}

/**
 *  Helper function to write out the configuration tree.
 *
 *  @note Locks objects for reading!
 */
HRESULT VirtualBox::saveSettings()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AssertReturn (!!mData.mCfgFile.mName, E_FAIL);

    HRESULT rc = S_OK;

    AutoReaderLock alock (this);

    try
    {
        using namespace settings;

#if 0
        /// @todo disabled until made thread-safe by using handle duplicates
        File file (File::ReadWrite, mData.mCfgFile.mHandle,
                   Utf8Str (mData.mCfgFile.mName));
#else
        File file (File::ReadWrite, Utf8Str (mData.mCfgFile.mName));
#endif
        XmlTreeBackend tree;

        rc = VirtualBox::loadSettingsTree_ForUpdate (tree, file);
        CheckComRCThrowRC (rc);

        Key global = tree.rootKey().createKey ("Global");

        /* machines */
        {
            /* first, delete the entire machine registry */
            Key registryNode = global.findKey ("MachineRegistry");
            if (!registryNode.isNull())
                registryNode.zap();
            /* then, recreate it */
            registryNode = global.createKey ("MachineRegistry");

            /* write out the machines */
            for (MachineList::iterator it = mData.mMachines.begin();
                 it != mData.mMachines.end();
                 ++ it)
            {
                Key entryNode = registryNode.appendKey ("MachineEntry");
                rc = (*it)->saveRegistryEntry (entryNode);
                CheckComRCThrowRC (rc);
            }
        }

        /* disk images */
        {
            /* first, delete the entire disk image registr */
            Key registryNode = global.findKey ("DiskRegistry");
            if (!registryNode.isNull())
                registryNode.zap();
            /* then, recreate it */
            registryNode = global.createKey ("DiskRegistry");

            /* write out the hard disks */
            {
                Key imagesNode = registryNode.createKey ("HardDisks");
                rc = saveHardDisks (imagesNode);
                CheckComRCThrowRC (rc);
            }

            /* write out the CD/DVD images */
            {
                Key imagesNode = registryNode.createKey ("DVDImages");

                for (DVDImageList::iterator it = mData.mDVDImages.begin();
                     it != mData.mDVDImages.end();
                     ++ it)
                {
                    ComObjPtr <DVDImage> dvd = *it;
                    /* no need to lock: fields are constant */
                    Key imageNode = imagesNode.appendKey ("Image");
                    imageNode.setValue <Guid> ("uuid", dvd->id());
                    imageNode.setValue <Bstr> ("src", dvd->filePath());
                }
            }

            /* write out the floppy images */
            {
                Key imagesNode = registryNode.createKey ("FloppyImages");

                for (FloppyImageList::iterator it = mData.mFloppyImages.begin();
                     it != mData.mFloppyImages.end();
                     ++ it)
                {
                    ComObjPtr <FloppyImage> fd = *it;
                    /* no need to lock: fields are constant */
                    Key imageNode = imagesNode.appendKey ("Image");
                    imageNode.setValue <Guid> ("uuid", fd->id());
                    imageNode.setValue <Bstr> ("src", fd->filePath());
                }
            }
        }

        /* host data (USB filters) */
        rc = mData.mHost->saveSettings (global);
        CheckComRCThrowRC (rc);

        rc = mData.mSystemProperties->saveSettings (global);
        CheckComRCThrowRC (rc);

        /* save the settings on success */
        rc = VirtualBox::saveSettingsTree (tree, file,
                                           mData.mSettingsFileVersion);
        CheckComRCThrowRC (rc);
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
 *  Saves all hard disks to the given <HardDisks> node.
 *
 *  @param aNode        <HardDisks> node.
 *
 *  @note Locks this object for reding.
 */
HRESULT VirtualBox::saveHardDisks (settings::Key &aNode)
{
    using namespace settings;

    AssertReturn (!aNode.isNull(), E_INVALIDARG);

    HRESULT rc = S_OK;

    AutoReaderLock alock (this);

    for (HardDiskList::const_iterator it = mData.mHardDisks.begin();
         it != mData.mHardDisks.end();
         ++ it)
    {
        ComObjPtr <HardDisk> hd = *it;
        AutoReaderLock hdLock (hd);

        Key hdNode = aNode.appendKey ("HardDisk");

        switch (hd->storageType())
        {
            case HardDiskStorageType_VirtualDiskImage:
            {
                Key storageNode = hdNode.createKey ("VirtualDiskImage");
                rc = hd->saveSettings (hdNode, storageNode);
                break;
            }

            case HardDiskStorageType_ISCSIHardDisk:
            {
                Key storageNode = hdNode.createKey ("ISCSIHardDisk");
                rc = hd->saveSettings (hdNode, storageNode);
                break;
            }

            case HardDiskStorageType_VMDKImage:
            {
                Key storageNode = hdNode.createKey ("VMDKImage");
                rc = hd->saveSettings (hdNode, storageNode);
                break;
            }
            case HardDiskStorageType_CustomHardDisk:
            {
                Key storageNode = hdNode.createKey ("CustomHardDisk");
                rc = hd->saveSettings (hdNode, storageNode);
                break;
            }

            case HardDiskStorageType_VHDImage:
            {
                Key storageNode = hdNode.createKey ("VHDImage");
                rc = hd->saveSettings (hdNode, storageNode);
                break;
            }
        }

        CheckComRCBreakRC (rc);
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

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = S_OK;

    {
        ComObjPtr <Machine> m;
        rc = findMachine (aMachine->uuid(), false /* aDoSetError */, &m);
        if (SUCCEEDED (rc))
        {
            /* sanity */
            AutoLimitedCaller machCaller (m);
            AssertComRC (machCaller.rc());

            return setError (E_INVALIDARG,
                tr ("Registered machine with UUID {%Vuuid} ('%ls') already exists"),
                aMachine->uuid().raw(), m->settingsFileFull().raw());
        }

        ComAssertRet (rc == E_INVALIDARG, rc);
        rc = S_OK;
    }

    if (autoCaller.state() != InInit)
    {
        /* Machine::trySetRegistered() will commit and save machine settings */
        rc = aMachine->trySetRegistered (TRUE);
        CheckComRCReturnRC (rc);
    }

    /* add to the collection of registered machines */
    mData.mMachines.push_back (aMachine);

    if (autoCaller.state() != InInit)
        rc = saveSettings();

    return rc;
}

/**
 *  Helper to register the hard disk.
 *
 *  @param aHardDisk    object to register
 *  @param aFlags       one of RHD_* values
 *
 *  @note Locks objects!
 */
HRESULT VirtualBox::registerHardDisk (HardDisk *aHardDisk, RHD_Flags aFlags)
{
    ComAssertRet (aHardDisk, E_INVALIDARG);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkMediaForConflicts (aHardDisk, NULL, NULL);
    CheckComRCReturnRC (rc);

    /* mark the hard disk as registered only when registration is external */
    if (aFlags == RHD_External)
    {
        rc = aHardDisk->trySetRegistered (TRUE);
        CheckComRCReturnRC (rc);
    }

    if (!aHardDisk->parent())
    {
        /* add to the collection of top-level images */
        mData.mHardDisks.push_back (aHardDisk);
    }

    /* insert to the map of hard disks */
    mData.mHardDiskMap
        .insert (HardDiskMap::value_type (aHardDisk->id(), aHardDisk));

    /* save global config file if not on startup */
    /// @todo (dmik) optimize later to save only the <HardDisks> node
    if (aFlags != RHD_OnStartUp)
        rc = saveSettings();

    return rc;
}

/**
 *  Helper to unregister the hard disk.
 *
 *  If the hard disk is a differencing hard disk and if the unregistration
 *  succeeds, the hard disk image is deleted and the object is uninitialized.
 *
 *  @param aHardDisk    hard disk to unregister
 *
 *  @note Locks objects!
 */
HRESULT VirtualBox::unregisterHardDisk (HardDisk *aHardDisk)
{
    AssertReturn (aHardDisk, E_INVALIDARG);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    LogFlowThisFunc (("image='%ls'\n", aHardDisk->toString().raw()));

    AutoLock alock (this);

    /* Lock the hard disk to ensure nobody registers it again before we delete
     * the differencing image (sanity check actually -- should never happen). */
    AutoLock hdLock (aHardDisk);

    /* try to unregister */
    HRESULT rc = aHardDisk->trySetRegistered (FALSE);
    CheckComRCReturnRC (rc);

    /* remove from the map of hard disks */
    mData.mHardDiskMap.erase (aHardDisk->id());

    if (!aHardDisk->parent())
    {
        /* non-differencing hard disk:
         * remove from the collection of top-level hard disks */
        mData.mHardDisks.remove (aHardDisk);
    }
    else
    {
        Assert (aHardDisk->isDifferencing());

        /* differencing hard disk: delete and uninitialize
         *
         * Note that we ignore errors because this operation may be a result
         * of unregistering a missing (inaccessible) differencing hard disk
         * in which case a failure to implicitly delete the image will not
         * prevent it from being unregistered and therefore should not pop up
         * on the caller's side. */
        rc = aHardDisk->asVDI()->deleteImage (true /* aIgnoreErrors*/);
        aHardDisk->uninit();
    }

    /* save the global config file anyway (already unregistered) */
    /// @todo (dmik) optimize later to save only the <HardDisks> node
    HRESULT rc2 = saveSettings();
    if (SUCCEEDED (rc))
        rc = rc2;

    return rc;
}

/**
 *  Helper to unregister the differencing hard disk image.
 *  Resets machine ID of the hard disk (to let the unregistration succeed)
 *  and then calls #unregisterHardDisk().
 *
 *  @param aHardDisk    differencing hard disk image to unregister
 *
 *  @note Locks objects!
 */
HRESULT VirtualBox::unregisterDiffHardDisk (HardDisk *aHardDisk)
{
    AssertReturn (aHardDisk, E_INVALIDARG);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoLock alock (this);

    /*
     *  Note: it's safe to lock aHardDisk here because the same object
     *  will be locked by #unregisterHardDisk().
     */
    AutoLock hdLock (aHardDisk);

    AssertReturn (aHardDisk->isDifferencing(), E_INVALIDARG);

    /*
     *  deassociate the machine from the hard disk
     *  (otherwise trySetRegistered() will definitely fail)
     */
    aHardDisk->setMachineId (Guid());

    return unregisterHardDisk (aHardDisk);
}


/**
 *  Helper to update the global settings file when the name of some machine
 *  changes so that file and directory renaming occurs. This method ensures
 *  that all affected paths in the disk registry are properly updated.
 *
 *  @param aOldPath old path (full)
 *  @param aNewPath new path (full)
 *
 *  @note Locks this object + DVD, Floppy and HardDisk children for writing.
 */
HRESULT VirtualBox::updateSettings (const char *aOldPath, const char *aNewPath)
{
    LogFlowThisFunc (("aOldPath={%s} aNewPath={%s}\n", aOldPath, aNewPath));

    AssertReturn (aOldPath, E_INVALIDARG);
    AssertReturn (aNewPath, E_INVALIDARG);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoLock alock (this);

    size_t oldPathLen = strlen (aOldPath);

    /* check DVD paths */
    for (DVDImageList::iterator it = mData.mDVDImages.begin();
         it != mData.mDVDImages.end();
         ++ it)
    {
        ComObjPtr <DVDImage> image = *it;

        /* no need to lock: fields are constant */
        Utf8Str path = image->filePathFull();
        LogFlowThisFunc (("DVD.fullPath={%s}\n", path.raw()));

        if (RTPathStartsWith (path, aOldPath))
        {
            Utf8Str newPath = Utf8StrFmt ("%s%s", aNewPath,
                                          path.raw() + oldPathLen);
            path = newPath;
            calculateRelativePath (path, path);
            image->updatePath (newPath, path);

            LogFlowThisFunc (("-> updated: full={%s} rel={%s}\n",
                              newPath.raw(), path.raw()));
        }
    }

    /* check Floppy paths */
    for (FloppyImageList::iterator it = mData.mFloppyImages.begin();
         it != mData.mFloppyImages.end();
         ++ it)
    {
        ComObjPtr <FloppyImage> image = *it;

        /* no need to lock: fields are constant */
        Utf8Str path = image->filePathFull();
        LogFlowThisFunc (("Floppy.fullPath={%s}\n", path.raw()));

        if (RTPathStartsWith (path, aOldPath))
        {
            Utf8Str newPath = Utf8StrFmt ("%s%s", aNewPath,
                                          path.raw() + oldPathLen);
            path = newPath;
            calculateRelativePath (path, path);
            image->updatePath (newPath, path);

            LogFlowThisFunc (("-> updated: full={%s} rel={%s}\n",
                              newPath.raw(), path.raw()));
        }
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
 * Helper method to load the setting tree and turn expected exceptions into
 * COM errors, according to arguments.
 *
 * Note that this method will not catch unexpected errors so it may still
 * throw something.
 *
 * @param aTree             Tree to load into settings.
 * @param aFile             File to load settings from.
 * @param aValidate         @c @true to enable tree validation.
 * @param aCatchLoadErrors  @c true to catch exceptions caused by file
 *                          access or validation errors.
 * @param aAddDefaults      @c true to cause the substitution of default
 *                          values for for missing attributes that have
 *                          defaults in the XML schema.
 * @param aFormatVersion    Where to store the current format version of the
 *                          loaded settings tree (optional, may be NULL).
 */
/* static */
HRESULT VirtualBox::loadSettingsTree (settings::XmlTreeBackend &aTree,
                                      settings::File &aFile,
                                      bool aValidate,
                                      bool aCatchLoadErrors,
                                      bool aAddDefaults,
                                      Utf8Str *aFormatVersion /* = NULL */)
{
    using namespace settings;

    try
    {
        SettingsTreeHelper helper = SettingsTreeHelper();

        aTree.setInputResolver (helper);
        aTree.setAutoConverter (helper);

        aTree.read (aFile, aValidate ? VBOX_XML_SCHEMA : NULL,
                    aAddDefaults ? XmlTreeBackend::Read_AddDefaults : 0);

        aTree.resetAutoConverter();
        aTree.resetInputResolver();

        /* on success, memorize the current settings file version or set it to
         * the most recent version if no settings conversion took place. Note
         * that it's not necessary to do it every time we load the settings file
         * (i.e. only loadSettingsTree_FirstTime() passes a non-NULL
         * aFormatVersion value) because currently we keep the settings
         * files locked so that the only legal way to change the format version
         * while VirtualBox is running is saveSettingsTree(). */
        if (aFormatVersion != NULL)
        {
            *aFormatVersion = aTree.oldVersion();
            if (aFormatVersion->isNull())
                *aFormatVersion = VBOX_XML_VERSION_FULL;
        }
    }
    catch (const EIPRTFailure &err)
    {
        if (!aCatchLoadErrors)
            throw;

        return setError (E_FAIL,
                         tr ("Could not load the settings file '%s' (%Vrc)"),
                         aFile.uri(), err.rc());
    }
    catch (const XmlTreeBackend::Error &err)
    {
        Assert (err.what() != NULL);

        if (!aCatchLoadErrors)
            throw;

        return setError (E_FAIL,
                         tr ("Could not load the settings file '%s'.\n%s"),
                         aFile.uri(),
                         err.what() ? err.what() : "Unknown error");
    }

    return S_OK;
}

/**
 * Helper method to save the settings tree and turn expected exceptions to COM
 * errors.
 *
 * Note that this method will not catch unexpected errors so it may still
 * throw something.
 *
 * @param aTree             Tree to save.
 * @param aFile             File to save the tree to.
 * @param aFormatVersion    Where to store the (recent) format version of the
 *                          saved settings tree on success.
 */
/* static */
HRESULT VirtualBox::saveSettingsTree (settings::TreeBackend &aTree,
                                      settings::File &aFile,
                                      Utf8Str &aFormatVersion)
{
    using namespace settings;

    try
    {
        aTree.write (aFile);

        /* set the current settings file version to the most recent version on
         * success. See also VirtualBox::loadSettingsTree(). */
        if (aFormatVersion != VBOX_XML_VERSION_FULL)
            aFormatVersion = VBOX_XML_VERSION_FULL;
    }
    catch (const EIPRTFailure &err)
    {
        /* this is the only expected exception for now */
        return setError (E_FAIL,
                         tr ("Could not save the settings file '%s' (%Vrc)"),
                         aFile.uri(), err.rc());
    }

    return S_OK;
}

/**
 * Creates a backup copy of the given settings file by suffixing it with the
 * supplied version format string and optionally with numbers from .0 to .9
 * if the backup file already exists.
 *
 * @param aFileName     Orignal settings file name.
 * @param aOldFormat    Version of the original format.
 * @param aBakFileName  File name of the created backup copy (only on success).
 */
/* static */
HRESULT VirtualBox::backupSettingsFile (const Bstr &aFileName,
                                        const Utf8Str &aOldFormat,
                                        Bstr &aBakFileName)
{
    Utf8Str of = aFileName;
    Utf8Str nf = Utf8StrFmt ("%s.%s.bak", of.raw(), aOldFormat.raw());

    int vrc = RTFileCopyEx (of, nf, RTFILECOPY_FLAGS_NO_SRC_DENY_WRITE,
                            NULL, NULL);

    /* try progressive suffix from .0 to .9 on failure */
    if (vrc == VERR_ALREADY_EXISTS)
    {
        Utf8Str tmp = nf;
        for (int i = 0; i <= 9 && RT_FAILURE (vrc); ++ i)
        {
            nf = Utf8StrFmt ("%s.%d", tmp.raw(), i);
            vrc = RTFileCopyEx (of, nf, RTFILECOPY_FLAGS_NO_SRC_DENY_WRITE,
                                NULL, NULL);
        }
    }

    if (RT_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Could not copy the settings file '%s' to '%s' (%Vrc)"),
            of.raw(), nf.raw(), vrc);

    aBakFileName = nf;

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
HRESULT VirtualBox::handleUnexpectedExceptions (RT_SRC_POS_DECL)
{
    try
    {
        /* rethrow the current exception */
        throw;
    }
    catch (const std::exception &err)
    {
        ComAssertMsgFailedPos (("Unexpected exception '%s' (%s)\n",
                                typeid (err).name(), err.what()),
                               pszFile, iLine, pszFunction);
        return E_FAIL;
    }
    catch (...)
    {
        ComAssertMsgFailedPos (("Unknown exception\n"),
                               pszFile, iLine, pszFunction);
        return E_FAIL;
    }

    /* should not get here */
    AssertFailed();
    return E_FAIL;
}

/**
 *  Helper to register the DVD image.
 *
 *  @param aImage       object to register
 *  @param aOnStartUp   whether this method called during VirtualBox init or not
 *
 *  @return COM status code
 *
 *  @note Locks objects!
 */
HRESULT VirtualBox::registerDVDImage (DVDImage *aImage, bool aOnStartUp)
{
    AssertReturn (aImage, E_INVALIDARG);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkMediaForConflicts (NULL, &aImage->id(),
                                         aImage->filePathFull());
    CheckComRCReturnRC (rc);

    /* add to the collection */
    mData.mDVDImages.push_back (aImage);

    /* save global config file if we're supposed to */
    if (!aOnStartUp)
        rc = saveSettings();

    return rc;
}

/**
 *  Helper to register the floppy image.
 *
 *  @param aImage       object to register
 *  @param aOnStartUp   whether this method called during VirtualBox init or not
 *
 *  @return COM status code
 *
 *  @note Locks objects!
 */
HRESULT VirtualBox::registerFloppyImage (FloppyImage *aImage, bool aOnStartUp)
{
    AssertReturn (aImage, E_INVALIDARG);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkMediaForConflicts (NULL, &aImage->id(),
                                         aImage->filePathFull());
    CheckComRCReturnRC (rc);

    /* add to the collection */
    mData.mFloppyImages.push_back (aImage);

    /* save global config file if we're supposed to */
    if (!aOnStartUp)
        rc = saveSettings();

    return rc;
}

/**
 * Helper function to create the guest OS type objects and our collection
 *
 * @returns COM status code
 */
HRESULT VirtualBox::registerGuestOSTypes()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), E_FAIL);
    AssertReturn (autoCaller.state() == InInit, E_FAIL);

    HRESULT rc = S_OK;

    // this table represents our os type / string mapping
    static struct
    {
        const char    *id;          // utf-8
        const char    *description; // utf-8
        const VBOXOSTYPE osType;
        const uint32_t recommendedRAM;
        const uint32_t recommendedVRAM;
        const uint32_t recommendedHDD;
    } OSTypes [SchemaDefs::OSTypeId_COUNT] =
    {
        /// @todo (dmik) get the list of OS types from the XML schema
        /* NOTE1: we assume that unknown is always the first entry!
         * NOTE2: please use powers of 2 when specifying the size of harddisks since
         *        '2GB' looks better than '1.95GB' (= 2000MB) */
        { SchemaDefs_OSTypeId_unknown,   tr ("Other/Unknown"),  VBOXOSTYPE_Unknown,    64,   4,  2 * _1K },
        { SchemaDefs_OSTypeId_dos,       "DOS",                 VBOXOSTYPE_DOS,        32,   4,      512 },
        { SchemaDefs_OSTypeId_win31,     "Windows 3.1",         VBOXOSTYPE_Win31,      32,   4,  1 * _1K },
        { SchemaDefs_OSTypeId_win95,     "Windows 95",          VBOXOSTYPE_Win95,      64,   4,  2 * _1K },
        { SchemaDefs_OSTypeId_win98,     "Windows 98",          VBOXOSTYPE_Win98,      64,   4,  2 * _1K },
        { SchemaDefs_OSTypeId_winme,     "Windows Me",          VBOXOSTYPE_WinMe,      64,   4,  4 * _1K },
        { SchemaDefs_OSTypeId_winnt4,    "Windows NT 4",        VBOXOSTYPE_WinNT4,    128,   4,  2 * _1K },
        { SchemaDefs_OSTypeId_win2k,     "Windows 2000",        VBOXOSTYPE_Win2k,     168,  12,  4 * _1K },
        { SchemaDefs_OSTypeId_winxp,     "Windows XP",          VBOXOSTYPE_WinXP,     192,  12, 10 * _1K },
        { SchemaDefs_OSTypeId_win2k3,    "Windows Server 2003", VBOXOSTYPE_Win2k3,    256,  12, 20 * _1K },
        { SchemaDefs_OSTypeId_winvista,  "Windows Vista",       VBOXOSTYPE_WinVista,  512,  12, 20 * _1K },
        { SchemaDefs_OSTypeId_win2k8,    "Windows Server 2008", VBOXOSTYPE_Win2k8,    256,  12, 20 * _1K },
        { SchemaDefs_OSTypeId_os2warp3,  "OS/2 Warp 3",         VBOXOSTYPE_OS2Warp3,   48,   4,  1 * _1K },
        { SchemaDefs_OSTypeId_os2warp4,  "OS/2 Warp 4",         VBOXOSTYPE_OS2Warp4,   64,   4,  2 * _1K },
        { SchemaDefs_OSTypeId_os2warp45, "OS/2 Warp 4.5",       VBOXOSTYPE_OS2Warp45,  96,   4,  2 * _1K },
        { SchemaDefs_OSTypeId_ecs,       "eComStation",         VBOXOSTYPE_ECS,        96,   4,  2 * _1K },
        { SchemaDefs_OSTypeId_linux22,   "Linux 2.2",           VBOXOSTYPE_Linux22,    64,   4,  2 * _1K },
        { SchemaDefs_OSTypeId_linux24,   "Linux 2.4",           VBOXOSTYPE_Linux24,   128,   4,  4 * _1K },
        { SchemaDefs_OSTypeId_linux26,   "Linux 2.6",           VBOXOSTYPE_Linux26,   256,   4,  8 * _1K },
        { SchemaDefs_OSTypeId_archlinux, "Arch Linux",          VBOXOSTYPE_ArchLinux, 256,  12,  8 * _1K },
        { SchemaDefs_OSTypeId_debian,    "Debian",              VBOXOSTYPE_Debian,    256,  12,  8 * _1K },
        { SchemaDefs_OSTypeId_opensuse,  "openSUSE",            VBOXOSTYPE_OpenSUSE,  256,  12,  8 * _1K },
        { SchemaDefs_OSTypeId_fedoracore,"Fedora",              VBOXOSTYPE_FedoraCore,256,  12,  8 * _1K },
        { SchemaDefs_OSTypeId_gentoo,    "Gentoo Linux",        VBOXOSTYPE_Gentoo,    256,  12,  8 * _1K },
        { SchemaDefs_OSTypeId_mandriva,  "Mandriva",            VBOXOSTYPE_Mandriva,  256,  12,  8 * _1K },
        { SchemaDefs_OSTypeId_redhat,    "Red Hat",             VBOXOSTYPE_RedHat,    256,  12,  8 * _1K },
        { SchemaDefs_OSTypeId_ubuntu,    "Ubuntu",              VBOXOSTYPE_Ubuntu,    256,  12,  8 * _1K },
        { SchemaDefs_OSTypeId_xandros,   "Xandros",             VBOXOSTYPE_Xandros,   256,  12,  8 * _1K },
        { SchemaDefs_OSTypeId_freebsd,   "FreeBSD",             VBOXOSTYPE_FreeBSD,    64,   4,  2 * _1K },
        { SchemaDefs_OSTypeId_openbsd,   "OpenBSD",             VBOXOSTYPE_OpenBSD,    64,   4,  2 * _1K },
        { SchemaDefs_OSTypeId_netbsd,    "NetBSD",              VBOXOSTYPE_NetBSD,     64,   4,  2 * _1K },
        { SchemaDefs_OSTypeId_netware,   "Netware",             VBOXOSTYPE_Netware,   128,   4,  4 * _1K },
        { SchemaDefs_OSTypeId_solaris,   "Solaris",             VBOXOSTYPE_Solaris,   512,  12, 16 * _1K },
        { SchemaDefs_OSTypeId_l4,        "L4",                  VBOXOSTYPE_L4,         64,   4,  2 * _1K }
    };

    for (uint32_t i = 0; i < ELEMENTS (OSTypes) && SUCCEEDED (rc); i++)
    {
        ComObjPtr <GuestOSType> guestOSTypeObj;
        rc = guestOSTypeObj.createObject();
        if (SUCCEEDED (rc))
        {
            rc = guestOSTypeObj->init (OSTypes[i].id,
                                       OSTypes[i].description,
                                       OSTypes[i].osType,
                                       OSTypes[i].recommendedRAM,
                                       OSTypes[i].recommendedVRAM,
                                       OSTypes[i].recommendedHDD);
            if (SUCCEEDED (rc))
                mData.mGuestOSTypes.push_back (guestOSTypeObj);
        }
    }

    return rc;
}

/**
 *  Helper to lock the VirtualBox configuration for write access.
 *
 *  @note This method is not thread safe (must be called only from #init()
 *  or #uninit()).
 *
 *  @note If the configuration file is not found, the method returns
 *  S_OK, but subsequent #isConfigLocked() will return FALSE. This is used
 *  in some places to determine the (valid) situation when no config file
 *  exists yet, and therefore a new one should be created from scatch.
 */
HRESULT VirtualBox::lockConfig()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());
    AssertReturn (autoCaller.state() == InInit, E_FAIL);

    HRESULT rc = S_OK;

    Assert (!isConfigLocked());
    if (!isConfigLocked())
    {
        /* open the associated config file */
        int vrc = RTFileOpen (&mData.mCfgFile.mHandle,
                             Utf8Str (mData.mCfgFile.mName),
                             RTFILE_O_READWRITE | RTFILE_O_OPEN |
                             RTFILE_O_DENY_WRITE);
        if (VBOX_FAILURE (vrc))
        {
            mData.mCfgFile.mHandle = NIL_RTFILE;

            /*
             *  It is ok if the file is not found, it will be created by
             *  init(). Otherwise return an error.
             */
            if (vrc != VERR_FILE_NOT_FOUND)
                rc = setError (E_FAIL,
                    tr ("Could not lock the settings file '%ls' (%Vrc)"),
                    mData.mCfgFile.mName.raw(), vrc);
        }

        LogFlowThisFunc (("mCfgFile.mName='%ls', mCfgFile.mHandle=%d, rc=%08X\n",
                          mData.mCfgFile.mName.raw(), mData.mCfgFile.mHandle, rc));
    }

    return rc;
}

/**
 *  Helper to unlock the VirtualBox configuration from write access.
 *
 *  @note This method is not thread safe (must be called only from #init()
 *  or #uninit()).
 */
HRESULT VirtualBox::unlockConfig()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), E_FAIL);
    AssertReturn (autoCaller.state() == InUninit, E_FAIL);

    HRESULT rc = S_OK;

    if (isConfigLocked())
    {
        RTFileFlush (mData.mCfgFile.mHandle);
        RTFileClose (mData.mCfgFile.mHandle);
        /** @todo flush the directory too. */
        mData.mCfgFile.mHandle = NIL_RTFILE;
        LogFlowThisFunc (("\n"));
    }

    return rc;
}

/**
 *  Thread function that watches the termination of all client processes
 *  that have opened sessions using IVirtualBox::OpenSession()
 */
// static
DECLCALLBACK(int) VirtualBox::ClientWatcher (RTTHREAD thread, void *pvUser)
{
    LogFlowFuncEnter();

    VirtualBox *that = (VirtualBox *) pvUser;
    Assert (that);

    SessionMachineVector machines;
    size_t cnt = 0;

#if defined(RT_OS_WINDOWS)

    HRESULT hrc = CoInitializeEx (NULL,
                                  COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE |
                                  COINIT_SPEED_OVER_MEMORY);
    AssertComRC (hrc);

    /// @todo (dmik) processes reaping!

    HANDLE *handles = new HANDLE [1];
    handles [0] = that->mWatcherData.mUpdateReq;

    do
    {
        AutoCaller autoCaller (that);
        /* VirtualBox has been early uninitialized, terminate */
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            DWORD rc = ::WaitForMultipleObjects (cnt + 1, handles, FALSE, INFINITE);

            /*
             *  Restore the caller before using VirtualBox. If it fails, this
             *  means VirtualBox is being uninitialized and we must terminate.
             */
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
            if (update)
            {
                /* obtain a new set of opened machines */
                that->getOpenedMachines (machines);
                cnt = machines.size();
                LogFlowFunc (("UPDATE: direct session count = %d\n", cnt));
                AssertMsg ((cnt + 1) <= MAXIMUM_WAIT_OBJECTS,
                           ("MAXIMUM_WAIT_OBJECTS reached"));
                /* renew the set of event handles */
                delete [] handles;
                handles = new HANDLE [cnt + 1];
                handles [0] = that->mWatcherData.mUpdateReq;
                for (size_t i = 0; i < cnt; ++ i)
                    handles [i + 1] = (machines [i])->ipcSem();
            }
        }
        while (true);
    }
    while (0);

    /* delete the set of event handles */
    delete [] handles;

    /* delete the set of opened machines if any */
    machines.clear();

    ::CoUninitialize();

#elif defined (RT_OS_OS2)

    /// @todo (dmik) processes reaping!

    /* according to PMREF, 64 is the maximum for the muxwait list */
    SEMRECORD handles [64];

    HMUX muxSem = NULLHANDLE;

    do
    {
        AutoCaller autoCaller (that);
        /* VirtualBox has been early uninitialized, terminate */
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            int vrc = RTSemEventWait (that->mWatcherData.mUpdateReq, 500);

            /*
             *  Restore the caller before using VirtualBox. If it fails, this
             *  means VirtualBox is being uninitialized and we must terminate.
             */
            autoCaller.add();
            if (!autoCaller.isOk())
                break;

            bool update = false;

            if (VBOX_SUCCESS (vrc))
            {
                /* update event is signaled */
                update = true;
            }
            else
            {
                AssertMsg (vrc == VERR_TIMEOUT || vrc == VERR_INTERRUPTED,
                           ("RTSemEventWait returned %Vrc\n", vrc));

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
                                AutoReaderLock machieLock (machines [semId]);
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
                                        AutoReaderLock machieLock (machines [semId]);
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
            }

            if (update)
            {
                /* close the old muxsem */
                if (muxSem != NULLHANDLE)
                    ::DosCloseMuxWaitSem (muxSem);
                /* obtain a new set of opened machines */
                that->getOpenedMachines (machines);
                cnt = machines.size();
                LogFlowFunc (("UPDATE: direct session count = %d\n", cnt));
                /// @todo use several muxwait sems if cnt is > 64
                AssertMsg (cnt <= 64 /* according to PMREF */,
                           ("maximum of 64 mutex semaphores reached (%d)", cnt));
                if (cnt > 64)
                    cnt = 64;
                if (cnt > 0)
                {
                    /* renew the set of event handles */
                    for (size_t i = 0; i < cnt; ++ i)
                    {
                        handles [i].hsemCur = (HSEM) machines [i]->ipcSem();
                        handles [i].ulUser = i;
                    }
                    /* create a new muxsem */
                    APIRET arc = ::DosCreateMuxWaitSem (NULL, &muxSem, cnt, handles,
                                                        DCMW_WAIT_ANY);
                    AssertMsg (arc == NO_ERROR,
                               ("DosCreateMuxWaitSem returned %d\n", arc)); NOREF(arc);
                }
            }
        }
        while (true);
    }
    while (0);

    /* close the muxsem */
    if (muxSem != NULLHANDLE)
        ::DosCloseMuxWaitSem (muxSem);

    /* delete the set of opened machines if any */
    machines.clear();

#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)

    bool need_update = false;

    do
    {
        AutoCaller autoCaller (that);
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

            if (VBOX_SUCCESS (rc) || need_update)
            {
                /* VBOX_SUCCESS (rc) means an update event is signaled */

                /* obtain a new set of opened machines */
                that->getOpenedMachines (machines);
                cnt = machines.size();
                LogFlowFunc (("UPDATE: direct session count = %d\n", cnt));
            }

            need_update = false;
            for (size_t i = 0; i < cnt; ++ i)
                need_update |= (machines [i])->checkForDeath();

            /* reap child processes */
            {
                AutoLock alock (that);
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
                            LogFlowFunc (("pid %d (%x) was NOT reaped, vrc=%Vrc\n",
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

    /* delete the set of opened machines if any */
    machines.clear();

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

    AssertReturn (pvUser, VERR_INVALID_POINTER);

    // create an event queue for the current thread
    EventQueue *eventQ = new EventQueue();
    AssertReturn (eventQ, VERR_NO_MEMORY);

    // return the queue to the one who created this thread
    *(static_cast <EventQueue **> (pvUser)) = eventQ;
    // signal that we're ready
    RTThreadUserSignal (thread);

    BOOL ok = TRUE;
    Event *event = NULL;

    while ((ok = eventQ->waitForEvent (&event)) && event)
        eventQ->handleEvent (event);

    AssertReturn (ok, VERR_GENERAL_FAILURE);

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

    AutoCaller autoCaller (mVirtualBox);
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
        AutoReaderLock alock (mVirtualBox);
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

