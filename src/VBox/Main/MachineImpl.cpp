/* $Id$ */
/** @file
 * Implementation of IMachine in VBoxSVC.
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

#if defined(RT_OS_WINDOWS)
#elif defined(RT_OS_LINUX)
#endif

#ifdef VBOX_WITH_SYS_V_IPC_SESSION_WATCHER
#   include <errno.h>
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <sys/ipc.h>
#   include <sys/sem.h>
#endif

#include "VirtualBoxImpl.h"
#include "MachineImpl.h"
#include "HardDiskImpl.h"
#include "ProgressImpl.h"
#include "HardDiskAttachmentImpl.h"
#include "USBControllerImpl.h"
#include "HostImpl.h"
#include "SystemPropertiesImpl.h"
#include "SharedFolderImpl.h"
#include "GuestOSTypeImpl.h"
#include "VirtualBoxErrorInfoImpl.h"
#include "GuestImpl.h"
#include "SATAControllerImpl.h"

#ifdef VBOX_WITH_USB
# include "USBProxyService.h"
#endif

#include "VirtualBoxXMLUtil.h"

#include "Logging.h"

#include <stdio.h>
#include <stdlib.h>

#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/asm.h>
#include <iprt/process.h>
#include <iprt/cpputils.h>
#include <iprt/env.h>

#include <VBox/err.h>
#include <VBox/param.h>

#include <algorithm>

#include <typeinfo>

#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
#define HOSTSUFF_EXE ".exe"
#else /* !RT_OS_WINDOWS */
#define HOSTSUFF_EXE ""
#endif /* !RT_OS_WINDOWS */

// defines / prototypes
/////////////////////////////////////////////////////////////////////////////

// globals
/////////////////////////////////////////////////////////////////////////////

/**
 *  @note The template is NOT completely valid according to VBOX_XML_SCHEMA
 *  (when loading a newly created settings file, validation will be turned off)
 */
static const char DefaultMachineConfig[] =
{
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" RTFILE_LINEFEED
    "<!-- innotek VirtualBox Machine Configuration -->" RTFILE_LINEFEED
    "<VirtualBox xmlns=\"" VBOX_XML_NAMESPACE "\" "
        "version=\"" VBOX_XML_VERSION_FULL "\">" RTFILE_LINEFEED
    "</VirtualBox>" RTFILE_LINEFEED
};

/**
 *  Progress callback handler for lengthy operations
 *  (corresponds to the FNRTPROGRESS typedef).
 *
 *  @param uPercentage  Completetion precentage (0-100).
 *  @param pvUser       Pointer to the Progress instance.
 */
static DECLCALLBACK(int) progressCallback (unsigned uPercentage, void *pvUser)
{
    Progress *progress = static_cast <Progress *> (pvUser);

    /* update the progress object */
    if (progress)
        progress->notifyProgress (uPercentage);

    return VINF_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////
// Machine::Data structure
/////////////////////////////////////////////////////////////////////////////

Machine::Data::Data()
{
    mRegistered = FALSE;
    /* mUuid is initialized in Machine::init() */

    mMachineState = MachineState_PoweredOff;
    RTTimeNow (&mLastStateChange);

    mMachineStateDeps = 0;
    mMachineStateDepsSem = NIL_RTSEMEVENTMULTI;
    mMachineStateChangePending = 0;

    mCurrentStateModified = TRUE;
    mHandleCfgFile = NIL_RTFILE;

    mSession.mPid = NIL_RTPROCESS;
    mSession.mState = SessionState_Closed;
}

Machine::Data::~Data()
{
    if (mMachineStateDepsSem != NIL_RTSEMEVENTMULTI)
    {
        RTSemEventMultiDestroy (mMachineStateDepsSem);
        mMachineStateDepsSem = NIL_RTSEMEVENTMULTI;
    }
}

/////////////////////////////////////////////////////////////////////////////
// Machine::UserData structure
/////////////////////////////////////////////////////////////////////////////

Machine::UserData::UserData()
{
    /* default values for a newly created machine */

    mNameSync = TRUE;

    /* mName, mOSTypeId, mSnapshotFolder, mSnapshotFolderFull are initialized in
     * Machine::init() */
}

Machine::UserData::~UserData()
{
}

/////////////////////////////////////////////////////////////////////////////
// Machine::HWData structure
/////////////////////////////////////////////////////////////////////////////

Machine::HWData::HWData()
{
    /* default values for a newly created machine */
    mMemorySize = 128;
    mMemoryBalloonSize = 0;
    mStatisticsUpdateInterval = 0;
    mVRAMSize = 8;
    mMonitorCount = 1;
    mHWVirtExEnabled = TSBool_False;
    mPAEEnabled = false;

    /* default boot order: floppy - DVD - HDD */
    mBootOrder [0] = DeviceType_Floppy;
    mBootOrder [1] = DeviceType_DVD;
    mBootOrder [2] = DeviceType_HardDisk;
    for (size_t i = 3; i < ELEMENTS (mBootOrder); i++)
        mBootOrder [i] = DeviceType_Null;

    mClipboardMode = ClipboardMode_Bidirectional;
}

Machine::HWData::~HWData()
{
}

bool Machine::HWData::operator== (const HWData &that) const
{
    if (this == &that)
        return true;

    if (mMemorySize != that.mMemorySize ||
        mMemoryBalloonSize != that.mMemoryBalloonSize ||
        mStatisticsUpdateInterval != that.mStatisticsUpdateInterval ||
        mVRAMSize != that.mVRAMSize ||
        mMonitorCount != that.mMonitorCount ||
        mHWVirtExEnabled != that.mHWVirtExEnabled ||
        mPAEEnabled != that.mPAEEnabled ||
        mClipboardMode != that.mClipboardMode)
        return false;

    for (size_t i = 0; i < ELEMENTS (mBootOrder); ++ i)
        if (mBootOrder [i] != that.mBootOrder [i])
            return false;

    if (mSharedFolders.size() != that.mSharedFolders.size())
        return false;

    if (mSharedFolders.size() == 0)
        return true;

    /* Make copies to speed up comparison */
    SharedFolderList folders = mSharedFolders;
    SharedFolderList thatFolders = that.mSharedFolders;

    SharedFolderList::iterator it = folders.begin();
    while (it != folders.end())
    {
        bool found = false;
        SharedFolderList::iterator thatIt = thatFolders.begin();
        while (thatIt != thatFolders.end())
        {
            if ((*it)->name() == (*thatIt)->name() &&
                RTPathCompare (Utf8Str ((*it)->hostPath()),
                               Utf8Str ((*thatIt)->hostPath())) == 0)
            {
                thatFolders.erase (thatIt);
                found = true;
                break;
            }
            else
                ++ thatIt;
        }
        if (found)
            it = folders.erase (it);
        else
            return false;
    }

    Assert (folders.size() == 0 && thatFolders.size() == 0);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Machine::HDData structure
/////////////////////////////////////////////////////////////////////////////

Machine::HDData::HDData()
{
    /* default values for a newly created machine */
    mHDAttachmentsChanged = false;
}

Machine::HDData::~HDData()
{
}

bool Machine::HDData::operator== (const HDData &that) const
{
    if (this == &that)
        return true;

    if (mHDAttachments.size() != that.mHDAttachments.size())
        return false;

    if (mHDAttachments.size() == 0)
        return true;

    /* Make copies to speed up comparison */
    HDAttachmentList atts = mHDAttachments;
    HDAttachmentList thatAtts = that.mHDAttachments;

    HDAttachmentList::iterator it = atts.begin();
    while (it != atts.end())
    {
        bool found = false;
        HDAttachmentList::iterator thatIt = thatAtts.begin();
        while (thatIt != thatAtts.end())
        {
            if ((*it)->bus() == (*thatIt)->bus() &&
                (*it)->channel() == (*thatIt)->channel() &&
                (*it)->device() == (*thatIt)->device() &&
                (*it)->hardDisk().equalsTo ((*thatIt)->hardDisk()))
            {
                thatAtts.erase (thatIt);
                found = true;
                break;
            }
            else
                ++ thatIt;
        }
        if (found)
            it = atts.erase (it);
        else
            return false;
    }

    Assert (atts.size() == 0 && thatAtts.size() == 0);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Machine class
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

Machine::Machine() : mType (IsMachine) {}

Machine::~Machine() {}

HRESULT Machine::FinalConstruct()
{
    LogFlowThisFunc (("\n"));
    return S_OK;
}

void Machine::FinalRelease()
{
    LogFlowThisFunc (("\n"));
    uninit();
}

/**
 *  Initializes the instance.
 *
 *  @param aParent      Associated parent object
 *  @param aConfigFile  Local file system path to the VM settings file (can
 *                      be relative to the VirtualBox config directory).
 *  @param aMode        Init_New, Init_Existing or Init_Registered
 *  @param aName        name for the machine when aMode is Init_New
 *                      (ignored otherwise)
 *  @param aNameSync    |TRUE| to automatically sync settings dir and file
 *                      name with the machine name. |FALSE| is used for legacy
 *                      machines where the file name is specified by the
 *                      user and should never change. Used only in Init_New
 *                      mode (ignored otherwise).
 *  @param aId          UUID of the machine. Required for aMode==Init_Registered
 *                      and optional for aMode==Init_New. Used for consistency
 *                      check when aMode is Init_Registered; must match UUID
 *                      stored in the settings file. Used for predefining the
 *                      UUID of a VM when aMode is Init_New.
 *
 *  @return  Success indicator. if not S_OK, the machine object is invalid
 */
HRESULT Machine::init (VirtualBox *aParent, const BSTR aConfigFile,
                       InitMode aMode, const BSTR aName /* = NULL */,
                       BOOL aNameSync /* = TRUE */,
                       const Guid *aId /* = NULL */)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aConfigFile='%ls', aMode=%d\n", aConfigFile, aMode));

    AssertReturn (aParent, E_INVALIDARG);
    AssertReturn (aConfigFile, E_INVALIDARG);
    AssertReturn (aMode != Init_New || (aName != NULL && *aName != '\0'),
			      E_INVALIDARG);
    AssertReturn (aMode != Init_Registered || aId != NULL, E_FAIL);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    HRESULT rc = S_OK;

    /* share the parent weakly */
    unconst (mParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    /* allocate the essential machine data structure (the rest will be
     * allocated later by initDataAndChildObjects() */
    mData.allocate();

    char configFileFull [RTPATH_MAX] = {0};

    /* memorize the config file name (as provided) */
    mData->mConfigFile = aConfigFile;

    /* get the full file name */
    int vrc = RTPathAbsEx (mParent->homeDir(), Utf8Str (aConfigFile),
                           configFileFull, sizeof (configFileFull));
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Invalid settings file name: '%ls' (%Vrc)"),
                aConfigFile, vrc);
    mData->mConfigFileFull = configFileFull;

    /* start with accessible */
    mData->mAccessible = TRUE;

    if (aMode != Init_New)
    {
        /* lock the settings file */
        rc = lockConfig();

        if (aMode == Init_Registered && FAILED (rc))
        {
            /* If the machine is registered, then, instead of returning a
             * failure, we mark it as inaccessible and set the result to
             * success to give it a try later */
            mData->mAccessible = FALSE;
            /* fetch the current error info */
            mData->mAccessError = com::ErrorInfo();
            LogWarning (("Machine {%Vuuid} is inaccessible! [%ls]\n",
                         mData->mUuid.raw(),
                         mData->mAccessError.getText().raw()));
            rc = S_OK;
        }
    }
    else
    {
        /* check for the file existence */
        RTFILE f = NIL_RTFILE;
        int vrc = RTFileOpen (&f, configFileFull, RTFILE_O_READ);
        if (VBOX_SUCCESS (vrc) || vrc == VERR_SHARING_VIOLATION)
        {
            rc = setError (E_FAIL,
                tr ("Settings file '%s' already exists"), configFileFull);
            if (VBOX_SUCCESS (vrc))
                RTFileClose (f);
        }
        else
        {
            if (vrc != VERR_FILE_NOT_FOUND && vrc != VERR_PATH_NOT_FOUND)
                rc = setError (E_FAIL,
                    tr ("Invalid settings file name: '%ls' (%Vrc)"),
                        mData->mConfigFileFull.raw(), vrc);
        }

        /* reset mAccessible to make sure uninit() called by the AutoInitSpan
         * destructor will not call uninitDataAndChildObjects() (we haven't
         * initialized anything yet) */
        if (FAILED (rc))
            mData->mAccessible = FALSE;
    }

    CheckComRCReturnRC (rc);

    if (aMode == Init_Registered)
    {
        /* store the supplied UUID (will be used to check for UUID consistency
         * in loadSettings() */
        unconst (mData->mUuid) = *aId;
        /* try to  load settings only if the settings file is accessible */
        if (mData->mAccessible)
            rc = registeredInit();
    }
    else
    {
        rc = initDataAndChildObjects();

        if (SUCCEEDED (rc))
        {
            if (aMode != Init_New)
            {
                rc = loadSettings (false /* aRegistered */);
            }
            else
            {
                /* create the machine UUID */
                if (aId)
                    unconst (mData->mUuid) = *aId;
                else
                    unconst (mData->mUuid).create();

                /* memorize the provided new machine's name */
                mUserData->mName = aName;
                mUserData->mNameSync = aNameSync;

                /* initialize the default snapshots folder
                 * (note: depends on the name value set above!) */
                rc = COMSETTER(SnapshotFolder) (NULL);
                AssertComRC (rc);
            }

            /* commit all changes made during the initialization */
            if (SUCCEEDED (rc))
                commit();
        }
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
    {
        if (mData->mAccessible)
            autoInitSpan.setSucceeded();
        else
            autoInitSpan.setLimited();
    }

    LogFlowThisFunc (("mName='%ls', mRegistered=%RTbool, mAccessible=%RTbool "
                      "rc=%08X\n",
                      !!mUserData ? mUserData->mName.raw() : NULL,
                      mData->mRegistered, mData->mAccessible, rc));

    LogFlowThisFuncLeave();

    return rc;
}

/**
 *  Initializes the registered machine by loading the settings file.
 *  This method is separated from #init() in order to make it possible to
 *  retry the operation after VirtualBox startup instead of refusing to
 *  startup the whole VirtualBox server in case if the settings file of some
 *  registered VM is invalid or inaccessible.
 *
 *  @note Must be always called from this object's write lock
 *        (unless called from #init() that doesn't need any locking).
 *  @note Locks the mUSBController method for writing.
 *  @note Subclasses must not call this method.
 */
HRESULT Machine::registeredInit()
{
    AssertReturn (mType == IsMachine, E_FAIL);
    AssertReturn (!mData->mUuid.isEmpty(), E_FAIL);

    HRESULT rc = initDataAndChildObjects();
    CheckComRCReturnRC (rc);

    if (!mData->mAccessible)
        rc = lockConfig();

    /* Temporarily reset the registered flag in order to let setters potentially
     * called from loadSettings() succeed (isMutable() used in all setters
     * will return FALSE for a Machine instance if mRegistered is TRUE). */
    mData->mRegistered = FALSE;

    if (SUCCEEDED (rc))
    {
        rc = loadSettings (true /* aRegistered */);

        if (FAILED (rc))
            unlockConfig();
    }

    if (SUCCEEDED (rc))
    {
        mData->mAccessible = TRUE;

        /* commit all changes made during loading the settings file */
        commit();

        /* VirtualBox will not call trySetRegistered(), so
         * inform the USB proxy about all attached USB filters */
        mUSBController->onMachineRegistered (TRUE);
    }
    else
    {
        /* If the machine is registered, then, instead of returning a
         * failure, we mark it as inaccessible and set the result to
         * success to give it a try later */
        mData->mAccessible = FALSE;
        /* fetch the current error info */
        mData->mAccessError = com::ErrorInfo();
        LogWarning (("Machine {%Vuuid} is inaccessible! [%ls]\n",
                     mData->mUuid.raw(),
                     mData->mAccessError.getText().raw()));

        /* rollback all changes */
        rollback (false /* aNotify */);

        /* uninitialize the common part to make sure all data is reset to
         * default (null) values */
        uninitDataAndChildObjects();

        rc = S_OK;
    }

    /* Restore the registered flag (even on failure) */
    mData->mRegistered = TRUE;

    return rc;
}

/**
 *  Uninitializes the instance.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 *
 *  @note The caller of this method must make sure that this object
 *  a) doesn't have active callers on the current thread and b) is not locked
 *  by the current thread; otherwise uninit() will hang either a) due to
 *  AutoUninitSpan waiting for a number of calls to drop to zero or b) due to
 *  a dead-lock caused by this thread waiting for all callers on the other
 *  threads are are done but preventing them from doing so by holding a lock.
 */
void Machine::uninit()
{
    LogFlowThisFuncEnter();

    Assert (!isLockedOnCurrentThread());

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    Assert (mType == IsMachine);
    Assert (!!mData);

    LogFlowThisFunc (("initFailed()=%d\n", autoUninitSpan.initFailed()));
    LogFlowThisFunc (("mRegistered=%d\n", mData->mRegistered));

    /* Enter this object lock because there may be a SessionMachine instance
     * somewhere around, that shares our data and lock but doesn't use our
     * addCaller()/removeCaller(), and it may be also accessing the same data
     * members. mParent lock is necessary as well because of
     * SessionMachine::uninit(), etc.
     */
    AutoMultiWriteLock2 alock (mParent, this);

    if (!mData->mSession.mMachine.isNull())
    {
        /* Theoretically, this can only happen if the VirtualBox server has been
         * terminated while there were clients running that owned open direct
         * sessions. Since in this case we are definitely called by
         * VirtualBox::uninit(), we may be sure that SessionMachine::uninit()
         * won't happen on the client watcher thread (because it does
         * VirtualBox::addCaller() for the duration of the
         * SessionMachine::checkForDeath() call, so that VirtualBox::uninit()
         * cannot happen until the VirtualBox caller is released). This is
         * important, because SessionMachine::uninit() cannot correctly operate
         * after we return from this method (it expects the Machine instance is
         * still valid). We'll call it ourselves below.
         */
        LogWarningThisFunc (("Session machine is not NULL (%p), "
                             "the direct session is still open!\n",
                             (SessionMachine *) mData->mSession.mMachine));

        if (mData->mMachineState >= MachineState_Running)
        {
            LogWarningThisFunc (("Setting state to Aborted!\n"));
            /* set machine state using SessionMachine reimplementation */
            static_cast <Machine *> (mData->mSession.mMachine)
                ->setMachineState (MachineState_Aborted);
        }

        /*
         *  Uninitialize SessionMachine using public uninit() to indicate
         *  an unexpected uninitialization.
         */
        mData->mSession.mMachine->uninit();
        /* SessionMachine::uninit() must set mSession.mMachine to null */
        Assert (mData->mSession.mMachine.isNull());
    }

    /* the lock is no more necessary (SessionMachine is uninitialized) */
    alock.leave();

    /* make sure the configuration is unlocked */
    unlockConfig();

    if (isModified())
    {
        LogWarningThisFunc (("Discarding unsaved settings changes!\n"));
        rollback (false /* aNotify */);
    }

    if (mData->mAccessible)
        uninitDataAndChildObjects();

    /* free the essential data structure last */
    mData.free();

    mParent->removeDependentChild (this);

    LogFlowThisFuncLeave();
}

// IMachine properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Machine::COMGETTER(Parent) (IVirtualBox **aParent)
{
    if (!aParent)
        return E_POINTER;

    AutoLimitedCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mParent is constant during life time, no need to lock */
    mParent.queryInterfaceTo (aParent);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Accessible) (BOOL *aAccessible)
{
    if (!aAccessible)
        return E_POINTER;

    AutoLimitedCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = S_OK;

    if (!mData->mAccessible)
    {
        /* try to initialize the VM once more if not accessible */

        AutoReadySpan autoReadySpan (this);
        AssertReturn (autoReadySpan.isOk(), E_FAIL);

        rc = registeredInit();

        if (mData->mAccessible)
            autoReadySpan.setSucceeded();
    }

    if (SUCCEEDED (rc))
        *aAccessible = mData->mAccessible;

    return rc;
}

STDMETHODIMP Machine::COMGETTER(AccessError) (IVirtualBoxErrorInfo **aAccessError)
{
    if (!aAccessError)
        return E_POINTER;

    AutoLimitedCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    if (mData->mAccessible || !mData->mAccessError.isBasicAvailable())
    {
        /* return shortly */
        aAccessError = NULL;
        return S_OK;
    }

    HRESULT rc = S_OK;

    ComObjPtr <VirtualBoxErrorInfo> errorInfo;
    rc = errorInfo.createObject();
    if (SUCCEEDED (rc))
    {
        errorInfo->init (mData->mAccessError.getResultCode(),
                         mData->mAccessError.getInterfaceID(),
                         mData->mAccessError.getComponent(),
                         mData->mAccessError.getText());
        rc = errorInfo.queryInterfaceTo (aAccessError);
    }

    return rc;
}

STDMETHODIMP Machine::COMGETTER(Name) (BSTR *aName)
{
    if (!aName)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mUserData->mName.cloneTo (aName);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Name) (INPTR BSTR aName)
{
    if (!aName)
        return E_INVALIDARG;

    if (!*aName)
        return setError (E_INVALIDARG,
            tr ("Machine name cannot be empty"));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mUserData.backup();
    mUserData->mName = aName;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Description) (BSTR *aDescription)
{
    if (!aDescription)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mUserData->mDescription.cloneTo (aDescription);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Description) (INPTR BSTR aDescription)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mUserData.backup();
    mUserData->mDescription = aDescription;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Id) (GUIDPARAMOUT aId)
{
    if (!aId)
        return E_POINTER;

    AutoLimitedCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mUuid.cloneTo (aId);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(OSTypeId) (BSTR *aOSTypeId)
{
    if (!aOSTypeId)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mUserData->mOSTypeId.cloneTo (aOSTypeId);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(OSTypeId) (INPTR BSTR aOSTypeId)
{
    if (!aOSTypeId)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* look up the object by Id to check it is valid */
    ComPtr <IGuestOSType> guestOSType;
    HRESULT rc = mParent->GetGuestOSType (aOSTypeId,
                                          guestOSType.asOutParam());
    CheckComRCReturnRC (rc);

    AutoLock alock (this);

    rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mUserData.backup();
    mUserData->mOSTypeId = aOSTypeId;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(MemorySize) (ULONG *memorySize)
{
    if (!memorySize)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *memorySize = mHWData->mMemorySize;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(MemorySize) (ULONG memorySize)
{
    /* check RAM limits */
    if (memorySize < SchemaDefs::MinGuestRAM ||
        memorySize > SchemaDefs::MaxGuestRAM)
        return setError (E_INVALIDARG,
            tr ("Invalid RAM size: %lu MB (must be in range [%lu, %lu] MB)"),
                memorySize, SchemaDefs::MinGuestRAM, SchemaDefs::MaxGuestRAM);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mMemorySize = memorySize;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(VRAMSize) (ULONG *memorySize)
{
    if (!memorySize)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *memorySize = mHWData->mVRAMSize;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(VRAMSize) (ULONG memorySize)
{
    /* check VRAM limits */
    if (memorySize < SchemaDefs::MinGuestVRAM ||
        memorySize > SchemaDefs::MaxGuestVRAM)
        return setError (E_INVALIDARG,
            tr ("Invalid VRAM size: %lu MB (must be in range [%lu, %lu] MB)"),
                memorySize, SchemaDefs::MinGuestVRAM, SchemaDefs::MaxGuestVRAM);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mVRAMSize = memorySize;

    return S_OK;
}

/** @todo this method should not be public */
STDMETHODIMP Machine::COMGETTER(MemoryBalloonSize) (ULONG *memoryBalloonSize)
{
    if (!memoryBalloonSize)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *memoryBalloonSize = mHWData->mMemoryBalloonSize;

    return S_OK;
}

/** @todo this method should not be public */
STDMETHODIMP Machine::COMSETTER(MemoryBalloonSize) (ULONG memoryBalloonSize)
{
    /* check limits */
    if (memoryBalloonSize >= VMMDEV_MAX_MEMORY_BALLOON(mHWData->mMemorySize))
        return setError (E_INVALIDARG,
            tr ("Invalid memory balloon size: %lu MB (must be in range [%lu, %lu] MB)"),
                memoryBalloonSize, 0, VMMDEV_MAX_MEMORY_BALLOON(mHWData->mMemorySize));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mMemoryBalloonSize = memoryBalloonSize;

    return S_OK;
}

/** @todo this method should not be public */
STDMETHODIMP Machine::COMGETTER(StatisticsUpdateInterval) (ULONG *statisticsUpdateInterval)
{
    if (!statisticsUpdateInterval)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *statisticsUpdateInterval = mHWData->mStatisticsUpdateInterval;

    return S_OK;
}

/** @todo this method should not be public */
STDMETHODIMP Machine::COMSETTER(StatisticsUpdateInterval) (ULONG statisticsUpdateInterval)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mStatisticsUpdateInterval = statisticsUpdateInterval;

    return S_OK;
}


STDMETHODIMP Machine::COMGETTER(MonitorCount) (ULONG *monitorCount)
{
    if (!monitorCount)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *monitorCount = mHWData->mMonitorCount;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(MonitorCount) (ULONG monitorCount)
{
    /* make sure monitor count is a sensible number */
    if (monitorCount < 1 || monitorCount > SchemaDefs::MaxGuestMonitors)
        return setError (E_INVALIDARG,
            tr ("Invalid monitor count: %lu (must be in range [%lu, %lu])"),
                monitorCount, 1, SchemaDefs::MaxGuestMonitors);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mMonitorCount = monitorCount;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(BIOSSettings)(IBIOSSettings **biosSettings)
{
    if (!biosSettings)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mBIOSSettings is constant during life time, no need to lock */
    mBIOSSettings.queryInterfaceTo (biosSettings);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(HWVirtExEnabled)(TSBool_T *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *enabled = mHWData->mHWVirtExEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(HWVirtExEnabled)(TSBool_T enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /** @todo check validity! */

    mHWData.backup();
    mHWData->mHWVirtExEnabled = enable;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(PAEEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *enabled = mHWData->mPAEEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(PAEEnabled)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /** @todo check validity! */

    mHWData.backup();
    mHWData->mPAEEnabled = enable;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SnapshotFolder) (BSTR *aSnapshotFolder)
{
    if (!aSnapshotFolder)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mUserData->mSnapshotFolderFull.cloneTo (aSnapshotFolder);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(SnapshotFolder) (INPTR BSTR aSnapshotFolder)
{
    /* @todo (r=dmik):
     *  1. Allow to change the name of the snapshot folder containing snapshots
     *  2. Rename the folder on disk instead of just changing the property
     *     value (to be smart and not to leave garbage). Note that it cannot be
     *     done here because the change may be rolled back. Thus, the right
     *     place is #saveSettings().
     */

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    if (!mData->mCurrentSnapshot.isNull())
        return setError (E_FAIL,
            tr ("The snapshot folder of a machine with snapshots cannot "
                "be changed (please discard all snapshots first)"));

    Utf8Str snapshotFolder = aSnapshotFolder;

    if (snapshotFolder.isEmpty())
    {
        if (isInOwnDir())
        {
            /* the default snapshots folder is 'Snapshots' in the machine dir */
            snapshotFolder = Utf8Str ("Snapshots");
        }
        else
        {
            /* the default snapshots folder is {UUID}, for backwards
             * compatibility and to resolve conflicts */
            snapshotFolder = Utf8StrFmt ("{%Vuuid}", mData->mUuid.raw());
        }
    }

    int vrc = calculateFullPath (snapshotFolder, snapshotFolder);
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Invalid snapshot folder: '%ls' (%Vrc)"),
                aSnapshotFolder, vrc);

    mUserData.backup();
    mUserData->mSnapshotFolder = aSnapshotFolder;
    mUserData->mSnapshotFolderFull = snapshotFolder;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(HardDiskAttachments) (IHardDiskAttachmentCollection **attachments)
{
    if (!attachments)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    ComObjPtr <HardDiskAttachmentCollection> collection;
    collection.createObject();
    collection->init (mHDData->mHDAttachments);
    collection.queryInterfaceTo (attachments);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(VRDPServer)(IVRDPServer **vrdpServer)
{
#ifdef VBOX_VRDP
    if (!vrdpServer)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Assert (!!mVRDPServer);
    mVRDPServer.queryInterfaceTo (vrdpServer);

    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Machine::COMGETTER(DVDDrive) (IDVDDrive **dvdDrive)
{
    if (!dvdDrive)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Assert (!!mDVDDrive);
    mDVDDrive.queryInterfaceTo (dvdDrive);
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(FloppyDrive) (IFloppyDrive **floppyDrive)
{
    if (!floppyDrive)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Assert (!!mFloppyDrive);
    mFloppyDrive.queryInterfaceTo (floppyDrive);
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(AudioAdapter)(IAudioAdapter **audioAdapter)
{
    if (!audioAdapter)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mAudioAdapter.queryInterfaceTo (audioAdapter);
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(USBController) (IUSBController **aUSBController)
{
#ifdef VBOX_WITH_USB
    if (!aUSBController)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    MultiResult rc = mParent->host()->checkUSBProxyService();
    CheckComRCReturnRC (rc);

    AutoReaderLock alock (this);

    return rc = mUSBController.queryInterfaceTo (aUSBController);
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treting it as a failure), for example, as in OSE */
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Machine::COMGETTER(SATAController) (ISATAController **aSATAController)
{
#ifdef VBOX_WITH_AHCI
    if (!aSATAController)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    return mSATAController.queryInterfaceTo (aSATAController);
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that SATA is simply not available
     * (w/o treting it as a failure), for example, as in OSE */
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Machine::COMGETTER(SettingsFilePath) (BSTR *aFilePath)
{
    if (!aFilePath)
        return E_POINTER;

    AutoLimitedCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mConfigFileFull.cloneTo (aFilePath);
    return S_OK;
}

STDMETHODIMP Machine::
COMGETTER(SettingsFileVersion) (BSTR *aSettingsFileVersion)
{
    if (!aSettingsFileVersion)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mSettingsFileVersion.cloneTo (aSettingsFileVersion);
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SettingsModified) (BOOL *aModified)
{
    if (!aModified)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    if (!isConfigLocked())
    {
        /*
         *  if we're ready and isConfigLocked() is FALSE then it means
         *  that no config file exists yet, so always return TRUE
         */
        *aModified = TRUE;
    }
    else
    {
        *aModified = isModified();
    }

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SessionState) (SessionState_T *aSessionState)
{
    if (!aSessionState)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aSessionState = mData->mSession.mState;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SessionType) (BSTR *aSessionType)
{
    if (!aSessionType)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mSession.mType.cloneTo (aSessionType);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SessionPid) (ULONG *aSessionPid)
{
    if (!aSessionPid)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aSessionPid = mData->mSession.mPid;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(State) (MachineState_T *machineState)
{
    if (!machineState)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *machineState = mData->mMachineState;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(LastStateChange) (LONG64 *aLastStateChange)
{
    if (!aLastStateChange)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aLastStateChange = RTTimeSpecGetMilli (&mData->mLastStateChange);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(StateFilePath) (BSTR *aStateFilePath)
{
    if (!aStateFilePath)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mSSData->mStateFilePath.cloneTo (aStateFilePath);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(LogFolder) (BSTR *aLogFolder)
{
    if (!aLogFolder)
        return E_POINTER;

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Utf8Str logFolder;
    getLogFolder (logFolder);

    Bstr (logFolder).cloneTo (aLogFolder);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CurrentSnapshot) (ISnapshot **aCurrentSnapshot)
{
    if (!aCurrentSnapshot)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mCurrentSnapshot.queryInterfaceTo (aCurrentSnapshot);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SnapshotCount) (ULONG *aSnapshotCount)
{
    if (!aSnapshotCount)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aSnapshotCount = !mData->mFirstSnapshot ? 0 :
                      mData->mFirstSnapshot->descendantCount() + 1 /* self */;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CurrentStateModified) (BOOL *aCurrentStateModified)
{
    if (!aCurrentStateModified)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    /*
     *  Note: for machines with no snapshots, we always return FALSE
     *  (mData->mCurrentStateModified will be TRUE in this case, for historical
     *  reasons :)
     */

    *aCurrentStateModified = !mData->mFirstSnapshot ? FALSE :
                             mData->mCurrentStateModified;

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(SharedFolders) (ISharedFolderCollection **aSharedFolders)
{
    if (!aSharedFolders)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    ComObjPtr <SharedFolderCollection> coll;
    coll.createObject();
    coll->init (mHWData->mSharedFolders);
    coll.queryInterfaceTo (aSharedFolders);

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(ClipboardMode) (ClipboardMode_T *aClipboardMode)
{
    if (!aClipboardMode)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aClipboardMode = mHWData->mClipboardMode;

    return S_OK;
}

STDMETHODIMP
Machine::COMSETTER(ClipboardMode) (ClipboardMode_T aClipboardMode)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mClipboardMode = aClipboardMode;

    return S_OK;
}

// IMachine methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Machine::SetBootOrder (ULONG aPosition, DeviceType_T aDevice)
{
    if (aPosition < 1 || aPosition > SchemaDefs::MaxBootPosition)
        return setError (E_INVALIDARG,
            tr ("Invalid boot position: %lu (must be in range [1, %lu])"),
                aPosition, SchemaDefs::MaxBootPosition);

    if (aDevice == DeviceType_USB)
        return setError (E_FAIL,
            tr ("Booting from USB devices is not currently supported"));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mBootOrder [aPosition - 1] = aDevice;

    return S_OK;
}

STDMETHODIMP Machine::GetBootOrder (ULONG aPosition, DeviceType_T *aDevice)
{
    if (aPosition < 1 || aPosition > SchemaDefs::MaxBootPosition)
        return setError (E_INVALIDARG,
            tr ("Invalid boot position: %lu (must be in range [1, %lu])"),
                aPosition, SchemaDefs::MaxBootPosition);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aDevice = mHWData->mBootOrder [aPosition - 1];

    return S_OK;
}

STDMETHODIMP Machine::AttachHardDisk (INPTR GUIDPARAM aId,
                                      StorageBus_T aBus, LONG aChannel, LONG aDevice)
{
    Guid id = aId;

    if (id.isEmpty() || aBus == StorageBus_Null)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::getHardDisk() need read lock */
    AutoMultiLock2 alock (mParent->rlock(), this->wlock());

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    if (!mData->mRegistered)
        return setError (E_FAIL,
            tr ("Cannot attach hard disks to an unregistered machine"));

    AssertReturn (mData->mMachineState != MachineState_Saved, E_FAIL);

    if (mData->mMachineState >= MachineState_Running)
        return setError (E_FAIL,
            tr ("Invalid machine state: %d"), mData->mMachineState);

    /* see if the device on the controller is already busy */
    for (HDData::HDAttachmentList::const_iterator it = mHDData->mHDAttachments.begin();
         it != mHDData->mHDAttachments.end(); ++ it)
    {
        ComObjPtr <HardDiskAttachment> hda = *it;
        if (hda->bus() == aBus && hda->channel() == aChannel && hda->device() == aDevice)
        {
            ComObjPtr <HardDisk> hd = hda->hardDisk();
            AutoLock hdLock (hd);
            return setError (E_FAIL,
                tr ("Hard disk '%ls' is already attached to device slot %d on "
                    "channel %d of bus %d"),
                hd->toString().raw(), aDevice, aChannel, aBus);
        }
    }

    /* find a hard disk by UUID */
    ComObjPtr <HardDisk> hd;
    rc = mParent->getHardDisk (id, hd);
    CheckComRCReturnRC (rc);

    /* create an attachment object early to let it check argiuments */
    ComObjPtr <HardDiskAttachment> attachment;
    attachment.createObject();
    rc = attachment->init (hd, aBus, aChannel, aDevice, false /* aDirty */);
    CheckComRCReturnRC (rc);

    AutoLock hdLock (hd);

    if (hd->isDifferencing())
        return setError (E_FAIL,
            tr ("Cannot attach the differencing hard disk '%ls'"),
            hd->toString().raw());

    bool dirty = false;

    switch (hd->type())
    {
        case HardDiskType_Immutable:
        {
            Assert (hd->machineId().isEmpty());
            /*
             *  increase readers to protect from unregistration
             *  until rollback()/commit() is done
             */
            hd->addReader();
            Log3 (("A: %ls proteced\n", hd->toString().raw()));
            dirty = true;
            break;
        }
        case HardDiskType_Writethrough:
        {
            Assert (hd->children().size() == 0);
            Assert (hd->snapshotId().isEmpty());
            /* fall through */
        }
        case HardDiskType_Normal:
        {
            if (hd->machineId().isEmpty())
            {
                /* attach directly */
                hd->setMachineId (mData->mUuid);
                Log3 (("A: %ls associated with %Vuuid\n",
                       hd->toString().raw(), mData->mUuid.raw()));
                dirty = true;
            }
            else
            {
                /* determine what the hard disk is already attached to */
                if (hd->snapshotId().isEmpty())
                {
                    /* attached to some VM in its current state */
                    if (hd->machineId() == mData->mUuid)
                    {
                        /*
                         *  attached to us, either in the backed up list of the
                         *  attachments or in the current one; the former is ok
                         *  (reattachment takes place within the same
                         *  "transaction") the latter is an error so check for it
                         */
                        for (HDData::HDAttachmentList::const_iterator it =
                                mHDData->mHDAttachments.begin();
                             it != mHDData->mHDAttachments.end(); ++ it)
                        {
                            if ((*it)->hardDisk().equalsTo (hd))
                            {
                                return setError (E_FAIL,
                                    tr ("Normal/Writethrough hard disk '%ls' is "
                                        "currently attached to device slot %d on channel %d "
                                        "of bus %d of this machine"),
                                    hd->toString().raw(),
                                    (*it)->device(),
                                    (*it)->channel(), (*it)->bus());
                            }
                        }
                        /*
                         *  dirty = false to indicate we didn't set machineId
                         *  and prevent it from being reset in DetachHardDisk()
                         */
                        Log3 (("A: %ls found in old\n", hd->toString().raw()));
                    }
                    else
                    {
                        /* attached to other VM */
                        return setError (E_FAIL,
                            tr ("Normal/Writethrough hard disk '%ls' is "
                                "currently attached to a machine with "
                                "UUID {%Vuuid}"),
                            hd->toString().raw(), hd->machineId().raw());
                    }
                }
                else
                {
                    /*
                     *  here we go when the HardDiskType_Normal
                     *  is attached to some VM (probably to this one, too)
                     *  at some particular snapshot, so we can create a diff
                     *  based on it
                     */
                    Assert (!hd->machineId().isEmpty());
                    /*
                     *  increase readers to protect from unregistration
                     *  until rollback()/commit() is done
                     */
                    hd->addReader();
                    Log3 (("A: %ls proteced\n", hd->toString().raw()));
                    dirty = true;
                }
            }

            break;
        }
    }

    attachment->setDirty (dirty);

    mHDData.backup();
    mHDData->mHDAttachments.push_back (attachment);
    Log3 (("A: %ls attached\n", hd->toString().raw()));

    /* note: diff images are actually created only in commit() */

    return S_OK;
}

STDMETHODIMP Machine::GetHardDisk (StorageBus_T aBus, LONG aChannel,
                                   LONG aDevice, IHardDisk **aHardDisk)
{
    if (aBus == StorageBus_Null)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aHardDisk = NULL;

    for (HDData::HDAttachmentList::const_iterator it = mHDData->mHDAttachments.begin();
        it != mHDData->mHDAttachments.end(); ++ it)
    {
        ComObjPtr <HardDiskAttachment> hda = *it;
        if (hda->bus() == aBus && hda->channel() == aChannel && hda->device() == aDevice)
        {
            hda->hardDisk().queryInterfaceTo (aHardDisk);
            return S_OK;
        }
    }

    return setError (E_INVALIDARG,
        tr ("No hard disk attached to device slot %d on channel %d of bus %d"),
            aDevice, aChannel, aBus);
}

STDMETHODIMP Machine::DetachHardDisk (StorageBus_T aBus, LONG aChannel, LONG aDevice)
{
    if (aBus == StorageBus_Null)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    AssertReturn (mData->mMachineState != MachineState_Saved, E_FAIL);

    if (mData->mMachineState >= MachineState_Running)
        return setError (E_FAIL,
            tr ("Invalid machine state: %d"), mData->mMachineState);

    for (HDData::HDAttachmentList::iterator it = mHDData->mHDAttachments.begin();
         it != mHDData->mHDAttachments.end(); ++ it)
    {
        ComObjPtr <HardDiskAttachment> hda = *it;
        if (hda->bus() == aBus && hda->channel() == aChannel && hda->device() == aDevice)
        {
            ComObjPtr <HardDisk> hd = hda->hardDisk();
            AutoLock hdLock (hd);

            ComAssertRet (hd->children().size() == 0 &&
                          hd->machineId() == mData->mUuid, E_FAIL);

            if (hda->isDirty())
            {
                switch (hd->type())
                {
                    case HardDiskType_Immutable:
                    {
                        /* decrease readers increased in AttachHardDisk() */
                        hd->releaseReader();
                        Log3 (("D: %ls released\n", hd->toString().raw()));
                        break;
                    }
                    case HardDiskType_Writethrough:
                    {
                        /* deassociate from this machine */
                        hd->setMachineId (Guid());
                        Log3 (("D: %ls deassociated\n", hd->toString().raw()));
                        break;
                    }
                    case HardDiskType_Normal:
                    {
                        if (hd->snapshotId().isEmpty())
                        {
                            /* deassociate from this machine */
                            hd->setMachineId (Guid());
                            Log3 (("D: %ls deassociated\n", hd->toString().raw()));
                        }
                        else
                        {
                            /* decrease readers increased in AttachHardDisk() */
                            hd->releaseReader();
                            Log3 (("%ls released\n", hd->toString().raw()));
                        }

                        break;
                    }
                }
            }

            mHDData.backup();
            /*
             *  we cannot use erase (it) below because backup() above will create
             *  a copy of the list and make this copy active, but the iterator
             *  still refers to the original and is not valid for a copy
             */
            mHDData->mHDAttachments.remove (hda);
            Log3 (("D: %ls detached\n", hd->toString().raw()));

            /*
             *  note: Non-dirty hard disks are actually deassociated
             *  and diff images are deleted only in commit()
             */

            return S_OK;
        }
    }

    return setError (E_INVALIDARG,
        tr ("No hard disk attached to device slot %d on channel %d of bus %d"),
        aDevice, aChannel, aBus);
}

STDMETHODIMP Machine::GetSerialPort (ULONG slot, ISerialPort **port)
{
    if (!port)
        return E_POINTER;
    if (slot >= ELEMENTS (mSerialPorts))
        return setError (E_INVALIDARG, tr ("Invalid slot number: %d"), slot);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mSerialPorts [slot].queryInterfaceTo (port);

    return S_OK;
}

STDMETHODIMP Machine::GetParallelPort (ULONG slot, IParallelPort **port)
{
    if (!port)
        return E_POINTER;
    if (slot >= ELEMENTS (mParallelPorts))
        return setError (E_INVALIDARG, tr ("Invalid slot number: %d"), slot);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mParallelPorts [slot].queryInterfaceTo (port);

    return S_OK;
}

STDMETHODIMP Machine::GetNetworkAdapter (ULONG slot, INetworkAdapter **adapter)
{
    if (!adapter)
        return E_POINTER;
    if (slot >= ELEMENTS (mNetworkAdapters))
        return setError (E_INVALIDARG, tr ("Invalid slot number: %d"), slot);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mNetworkAdapters [slot].queryInterfaceTo (adapter);

    return S_OK;
}

/**
 *  @note Locks this object for reading.
 */
STDMETHODIMP Machine::GetNextExtraDataKey (INPTR BSTR aKey, BSTR *aNextKey, BSTR *aNextValue)
{
    if (!aNextKey)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    /* start with nothing found */
    *aNextKey = NULL;
    if (aNextValue)
        *aNextValue = NULL;

    /* if we're ready and isConfigLocked() is FALSE then it means
     * that no config file exists yet, so return shortly */
    if (!isConfigLocked())
        return S_OK;

    HRESULT rc = S_OK;

    try
    {
        using namespace settings;

        /* load the config file */
#if 0
        /// @todo disabled until made thread-safe by using handle duplicates
        File file (File::ReadWrite, mData->mHandleCfgFile,
                   Utf8Str (mData->mConfigFileFull));
#else
        File file (File::Read, Utf8Str (mData->mConfigFileFull));
#endif
        XmlTreeBackend tree;

        rc = VirtualBox::loadSettingsTree_Again (tree, file);
        CheckComRCReturnRC (rc);

        Key machineNode = tree.rootKey().key ("Machine");
        Key extraDataNode = machineNode.findKey ("ExtraData");

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
STDMETHODIMP Machine::GetExtraData (INPTR BSTR aKey, BSTR *aValue)
{
    if (!aKey)
        return E_INVALIDARG;
    if (!aValue)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    /* start with nothing found */
    *aValue = NULL;

    /* if we're ready and isConfigLocked() is FALSE then it means
     * that no config file exists yet, so return shortly */
    if (!isConfigLocked())
        return S_OK;

    HRESULT rc = S_OK;

    try
    {
        using namespace settings;

        /* load the config file */
#if 0
        /// @todo disabled until made thread-safe by using handle duplicates
        File file (File::ReadWrite, mData->mHandleCfgFile,
                   Utf8Str (mData->mConfigFileFull));
#else
        File file (File::Read, Utf8Str (mData->mConfigFileFull));
#endif
        XmlTreeBackend tree;

        rc = VirtualBox::loadSettingsTree_Again (tree, file);
        CheckComRCReturnRC (rc);

        const Utf8Str key = aKey;

        Key machineNode = tree.rootKey().key ("Machine");
        Key extraDataNode = machineNode.findKey ("ExtraData");

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
 *  @note Locks mParent for writing + this object for writing.
 */
STDMETHODIMP Machine::SetExtraData (INPTR BSTR aKey, INPTR BSTR aValue)
{
    if (!aKey)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::onExtraDataCanChange() and saveSettings() need mParent
     * lock (saveSettings() needs a write one) */
    AutoMultiWriteLock2 alock (mParent, this);

    if (mType == IsSnapshotMachine)
    {
        HRESULT rc = checkStateDependency (MutableStateDep);
        CheckComRCReturnRC (rc);
    }

    bool changed = false;
    HRESULT rc = S_OK;

    /* If we're ready and isConfigLocked() is FALSE then it means that no
     * config file exists yet, so call saveSettings() to create one. */
    if (!isConfigLocked())
    {
        rc = saveSettings (false /* aMarkCurStateAsModified */);
        CheckComRCReturnRC (rc);
    }

    try
    {
        using namespace settings;

        /* load the config file */
#if 0
        /// @todo disabled until made thread-safe by using handle duplicates
        File file (File::ReadWrite, mData->mHandleCfgFile,
                   Utf8Str (mData->mConfigFileFull));
#else
        File file (File::ReadWrite, Utf8Str (mData->mConfigFileFull));
#endif
        XmlTreeBackend tree;

        rc = VirtualBox::loadSettingsTree_ForUpdate (tree, file);
        CheckComRCReturnRC (rc);

        const Utf8Str key = aKey;
        Bstr oldVal;

        Key machineNode = tree.rootKey().key ("Machine");
        Key extraDataNode = machineNode.createKey ("ExtraData");
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
            if (!mParent->onExtraDataCanChange (mData->mUuid, aKey, aValue, error))
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
                                               mData->mSettingsFileVersion);
            CheckComRCReturnRC (rc);
        }
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions (RT_SRC_POS);
    }

    /* fire a notification */
    if (SUCCEEDED (rc) && changed)
        mParent->onExtraDataChange (mData->mUuid, aKey, aValue);

    return rc;
}

STDMETHODIMP Machine::SaveSettings()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* saveSettings() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /* the settings file path may never be null */
    ComAssertRet (mData->mConfigFileFull, E_FAIL);

    /* save all VM data excluding snapshots */
    return saveSettings();
}

STDMETHODIMP Machine::SaveSettingsWithBackup (BSTR *aBakFileName)
{
    if (!aBakFileName)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* saveSettings() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /* the settings file path may never be null */
    ComAssertRet (mData->mConfigFileFull, E_FAIL);

    /* perform backup only when there was auto-conversion */
    if (mData->mSettingsFileVersion != VBOX_XML_VERSION_FULL)
    {
        Bstr bakFileName;

        HRESULT rc = VirtualBox::backupSettingsFile (mData->mConfigFileFull,
                                                     mData->mSettingsFileVersion,
                                                     bakFileName);
        CheckComRCReturnRC (rc);

        bakFileName.cloneTo (aBakFileName);
    }

    /* save all VM data excluding snapshots */
    return saveSettings();
}

STDMETHODIMP Machine::DiscardSettings()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /*
     *  during this rollback, the session will be notified if data has
     *  been actually changed
     */
    rollback (true /* aNotify */);

    return S_OK;
}

STDMETHODIMP Machine::DeleteSettings()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    if (mData->mRegistered)
        return setError (E_FAIL,
            tr ("Cannot delete settings of a registered machine"));

    /* delete the settings only when the file actually exists */
    if (isConfigLocked())
    {
        unlockConfig();
        int vrc = RTFileDelete (Utf8Str (mData->mConfigFileFull));
        if (VBOX_FAILURE (vrc))
            return setError (E_FAIL,
                tr ("Could not delete the settings file '%ls' (%Vrc)"),
                mData->mConfigFileFull.raw(), vrc);

        /* delete the Logs folder, nothing important should be left
         * there (we don't check for errors because the user might have
         * some private files there that we don't want to delete) */
        Utf8Str logFolder;
        getLogFolder (logFolder);
        Assert (!logFolder.isEmpty());
        if (RTDirExists (logFolder))
        {
            /* Delete all VBox.log[.N] files from the Logs folder
             * (this must be in sync with the rotation logic in
             * Console::powerUpThread()). Also, delete the VBox.png[.N]
             * files that may have been created by the GUI. */
            Utf8Str log = Utf8StrFmt ("%s/VBox.log", logFolder.raw());
            RTFileDelete (log);
            log = Utf8StrFmt ("%s/VBox.png", logFolder.raw());
            RTFileDelete (log);
            for (int i = 3; i >= 0; i--)
            {
                log = Utf8StrFmt ("%s/VBox.log.%d", logFolder.raw(), i);
                RTFileDelete (log);
                log = Utf8StrFmt ("%s/VBox.png.%d", logFolder.raw(), i);
                RTFileDelete (log);
            }

            RTDirRemove (logFolder);
        }

        /* delete the Snapshots folder, nothing important should be left
         * there (we don't check for errors because the user might have
         * some private files there that we don't want to delete) */
        Utf8Str snapshotFolder = mUserData->mSnapshotFolderFull;
        Assert (!snapshotFolder.isEmpty());
        if (RTDirExists (snapshotFolder))
            RTDirRemove (snapshotFolder);

        /* delete the directory that contains the settings file, but only
         * if it matches the VM name (i.e. a structure created by default in
         * prepareSaveSettings()) */
        {
            Utf8Str settingsDir;
            if (isInOwnDir (&settingsDir))
                RTDirRemove (settingsDir);
        }
    }

    return S_OK;
}

STDMETHODIMP Machine::GetSnapshot (INPTR GUIDPARAM aId, ISnapshot **aSnapshot)
{
    if (!aSnapshot)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Guid id = aId;
    ComObjPtr <Snapshot> snapshot;

    HRESULT rc = findSnapshot (id, snapshot, true /* aSetError */);
    snapshot.queryInterfaceTo (aSnapshot);

    return rc;
}

STDMETHODIMP Machine::FindSnapshot (INPTR BSTR aName, ISnapshot **aSnapshot)
{
    if (!aName)
        return E_INVALIDARG;
    if (!aSnapshot)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    ComObjPtr <Snapshot> snapshot;

    HRESULT rc = findSnapshot (aName, snapshot, true /* aSetError */);
    snapshot.queryInterfaceTo (aSnapshot);

    return rc;
}

STDMETHODIMP Machine::SetCurrentSnapshot (INPTR GUIDPARAM aId)
{
    /// @todo (dmik) don't forget to set
    //  mData->mCurrentStateModified to FALSE

    return setError (E_NOTIMPL, "Not implemented");
}

STDMETHODIMP
Machine::CreateSharedFolder (INPTR BSTR aName, INPTR BSTR aHostPath, BOOL aWritable)
{
    if (!aName || !aHostPath)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    ComObjPtr <SharedFolder> sharedFolder;
    rc = findSharedFolder (aName, sharedFolder, false /* aSetError */);
    if (SUCCEEDED (rc))
        return setError (E_FAIL,
            tr ("Shared folder named '%ls' already exists"), aName);

    sharedFolder.createObject();
    rc = sharedFolder->init (machine(), aName, aHostPath, aWritable);
    CheckComRCReturnRC (rc);

    BOOL accessible = FALSE;
    rc = sharedFolder->COMGETTER(Accessible) (&accessible);
    CheckComRCReturnRC (rc);

    if (!accessible)
        return setError (E_FAIL,
            tr ("Shared folder host path '%ls' is not accessible"), aHostPath);

    mHWData.backup();
    mHWData->mSharedFolders.push_back (sharedFolder);

    /* inform the direct session if any */
    alock.leave();
    onSharedFolderChange();

    return S_OK;
}

STDMETHODIMP Machine::RemoveSharedFolder (INPTR BSTR aName)
{
    if (!aName)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    ComObjPtr <SharedFolder> sharedFolder;
    rc = findSharedFolder (aName, sharedFolder, true /* aSetError */);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mSharedFolders.remove (sharedFolder);

    /* inform the direct session if any */
    alock.leave();
    onSharedFolderChange();

    return S_OK;
}

STDMETHODIMP Machine::CanShowConsoleWindow (BOOL *aCanShow)
{
    if (!aCanShow)
        return E_POINTER;

    /* start with No */
    *aCanShow = FALSE;

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);

        if (mData->mSession.mState != SessionState_Open)
            return setError (E_FAIL,
                tr ("Machine session is not open (session state: %d)"),
                mData->mSession.mState);

        directControl = mData->mSession.mDirectControl;
    }

    /* ignore calls made after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    ULONG64 dummy;
    return directControl->OnShowWindow (TRUE /* aCheck */, aCanShow, &dummy);
}

STDMETHODIMP Machine::ShowConsoleWindow (ULONG64 *aWinId)
{
    if (!aWinId)
        return E_POINTER;

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);

        if (mData->mSession.mState != SessionState_Open)
            return setError (E_FAIL,
                tr ("Machine session is not open (session state: %d)"),
                mData->mSession.mState);

        directControl = mData->mSession.mDirectControl;
    }

    /* ignore calls made after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    BOOL dummy;
    return directControl->OnShowWindow (FALSE /* aCheck */, &dummy, aWinId);
}

// public methods for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  Returns the session machine object associated with the this machine.
 *  The returned session machine is null if no direct session is currently open.
 *
 *  @note locks this object for reading.
 */
ComObjPtr <SessionMachine> Machine::sessionMachine()
{
    ComObjPtr <SessionMachine> sm;

    AutoLimitedCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), sm);

    /* return null for inaccessible machines */
    if (autoCaller.state() != Ready)
        return sm;

    AutoReaderLock alock (this);

    sm = mData->mSession.mMachine;
    Assert (!sm.isNull() ||
            mData->mSession.mState != SessionState_Open);

    return  sm;
}

/**
 *  Saves the registry entry of this machine to the given configuration node.
 *
 *  @param aEntryNode Node to save the registry entry to.
 *
 *  @note locks this object for reading.
 */
HRESULT Machine::saveRegistryEntry (settings::Key &aEntryNode)
{
    AssertReturn (!aEntryNode.isNull(), E_FAIL);

    AutoLimitedCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    /* UUID */
    aEntryNode.setValue <Guid> ("uuid", mData->mUuid);
    /* settings file name (possibly, relative) */
    aEntryNode.setValue <Bstr> ("src", mData->mConfigFile);

    return S_OK;
}

/**
 *  Calculates the absolute path of the given path taking the directory of
 *  the machine settings file as the current directory.
 *
 *  @param  aPath   path to calculate the absolute path for
 *  @param  aResult where to put the result (used only on success,
 *          so can be the same Utf8Str instance as passed as \a aPath)
 *  @return VirtualBox result
 *
 *  @note Locks this object for reading.
 */
int Machine::calculateFullPath (const char *aPath, Utf8Str &aResult)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReaderLock alock (this);

    AssertReturn (!mData->mConfigFileFull.isNull(), VERR_GENERAL_FAILURE);

    Utf8Str settingsDir = mData->mConfigFileFull;

    RTPathStripFilename (settingsDir.mutableRaw());
    char folder [RTPATH_MAX];
    int vrc = RTPathAbsEx (settingsDir, aPath,
                           folder, sizeof (folder));
    if (VBOX_SUCCESS (vrc))
        aResult = folder;

    return vrc;
}

/**
 *  Tries to calculate the relative path of the given absolute path using the
 *  directory of the machine settings file as the base directory.
 *
 *  @param  aPath   absolute path to calculate the relative path for
 *  @param  aResult where to put the result (used only when it's possible to
 *                  make a relative path from the given absolute path;
 *                  otherwise left untouched)
 *
 *  @note Locks this object for reading.
 */
void Machine::calculateRelativePath (const char *aPath, Utf8Str &aResult)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    AutoReaderLock alock (this);

    AssertReturnVoid (!mData->mConfigFileFull.isNull());

    Utf8Str settingsDir = mData->mConfigFileFull;

    RTPathStripFilename (settingsDir.mutableRaw());
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

/**
 *  Returns the full path to the machine's log folder in the
 *  \a aLogFolder argument.
 */
void Machine::getLogFolder (Utf8Str &aLogFolder)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoReaderLock alock (this);

    Utf8Str settingsDir;
    if (isInOwnDir (&settingsDir))
    {
        /* Log folder is <Machines>/<VM_Name>/Logs */
        aLogFolder = Utf8StrFmt ("%s%cLogs", settingsDir.raw(), RTPATH_DELIMITER);
    }
    else
    {
        /* Log folder is <Machines>/<VM_SnapshotFolder>/Logs */
        Assert (!mUserData->mSnapshotFolderFull.isEmpty());
        aLogFolder = Utf8StrFmt ("%ls%cLogs", mUserData->mSnapshotFolderFull.raw(),
                                 RTPATH_DELIMITER);
    }
}

/**
 *  Returns @c true if the given DVD image is attached to this machine either
 *  in the current state or in any of the snapshots.
 *
 *  @param aId          Image ID to check.
 *  @param aUsage       Type of the check.
 *
 *  @note Locks this object + DVD object for reading.
 */
bool Machine::isDVDImageUsed (const Guid &aId, ResourceUsage_T aUsage)
{
    AutoLimitedCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    /* answer 'not attached' if the VM is limited */
    if (autoCaller.state() == Limited)
        return false;

    AutoReaderLock alock (this);

    Machine *m = this;

    /* take the session machine when appropriate */
    if (!mData->mSession.mMachine.isNull())
        m = mData->mSession.mMachine;

    /* first, check the current state */
    {
        const ComObjPtr <DVDDrive> &dvd = m->mDVDDrive;
        AssertReturn (!dvd.isNull(), false);

        AutoReaderLock dvdLock (dvd);

        /* loop over the backed up (permanent) and current (temporary) DVD data */
        DVDDrive::Data *d [2];
        if (dvd->data().isBackedUp())
        {
            d [0] = dvd->data().backedUpData();
            d [1] = dvd->data().data();
        }
        else
        {
            d [0] = dvd->data().data();
            d [1] = NULL;
        }

        if (!(aUsage & ResourceUsage_Permanent))
            d [0] = NULL;
        if (!(aUsage & ResourceUsage_Temporary))
            d [1] = NULL;

        for (unsigned i = 0; i < ELEMENTS (d); ++ i)
        {
            if (d [i] &&
                d [i]->mDriveState == DriveState_ImageMounted)
            {
                Guid id;
                HRESULT rc = d [i]->mDVDImage->COMGETTER(Id) (id.asOutParam());
                AssertComRC (rc);
                if (id == aId)
                    return true;
            }
        }
    }

    /* then, check snapshots if any */
    if (aUsage & ResourceUsage_Permanent)
    {
        if (!mData->mFirstSnapshot.isNull() &&
            mData->mFirstSnapshot->isDVDImageUsed (aId))
            return true;
    }

    return false;
}

/**
 *  Returns @c true if the given Floppy image is attached to this machine either
 *  in the current state or in any of the snapshots.
 *
 *  @param aId          Image ID to check.
 *  @param aUsage       Type of the check.
 *
 *  @note Locks this object + Floppy object for reading.
 */
bool Machine::isFloppyImageUsed (const Guid &aId, ResourceUsage_T aUsage)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    /* answer 'not attached' if the VM is limited */
    if (autoCaller.state() == Limited)
        return false;

    AutoReaderLock alock (this);

    Machine *m = this;

    /* take the session machine when appropriate */
    if (!mData->mSession.mMachine.isNull())
        m = mData->mSession.mMachine;

    /* first, check the current state */
    {
        const ComObjPtr <FloppyDrive> &floppy = m->mFloppyDrive;
        AssertReturn (!floppy.isNull(), false);

        AutoReaderLock floppyLock (floppy);

        /* loop over the backed up (permanent) and current (temporary) Floppy data */
        FloppyDrive::Data *d [2];
        if (floppy->data().isBackedUp())
        {
            d [0] = floppy->data().backedUpData();
            d [1] = floppy->data().data();
        }
        else
        {
            d [0] = floppy->data().data();
            d [1] = NULL;
        }

        if (!(aUsage & ResourceUsage_Permanent))
            d [0] = NULL;
        if (!(aUsage & ResourceUsage_Temporary))
            d [1] = NULL;

        for (unsigned i = 0; i < ELEMENTS (d); ++ i)
        {
            if (d [i] &&
                d [i]->mDriveState == DriveState_ImageMounted)
            {
                Guid id;
                HRESULT rc = d [i]->mFloppyImage->COMGETTER(Id) (id.asOutParam());
                AssertComRC (rc);
                if (id == aId)
                    return true;
            }
        }
    }

    /* then, check snapshots if any */
    if (aUsage & ResourceUsage_Permanent)
    {
        if (!mData->mFirstSnapshot.isNull() &&
            mData->mFirstSnapshot->isFloppyImageUsed (aId))
            return true;
    }

    return false;
}

/**
 *  @note Locks mParent and this object for writing,
 *        calls the client process (outside the lock).
 */
HRESULT Machine::openSession (IInternalSessionControl *aControl)
{
    LogFlowThisFuncEnter();

    AssertReturn (aControl, E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* We need VirtualBox lock because of Progress::notifyComplete() */
    AutoMultiWriteLock2 alock (mParent, this);

    if (!mData->mRegistered)
        return setError (E_UNEXPECTED,
            tr ("The machine '%ls' is not registered"), mUserData->mName.raw());

    LogFlowThisFunc (("mSession.mState=%d\n", mData->mSession.mState));

    if (mData->mSession.mState == SessionState_Open ||
        mData->mSession.mState == SessionState_Closing)
        return setError (E_ACCESSDENIED,
            tr ("A session for the machine '%ls' is currently open "
                "(or being closed)"),
            mUserData->mName.raw());

    /* may not be Running */
    AssertReturn (mData->mMachineState < MachineState_Running, E_FAIL);

    /* get the sesion PID */
    RTPROCESS pid = NIL_RTPROCESS;
    AssertCompile (sizeof (ULONG) == sizeof (RTPROCESS));
    aControl->GetPID ((ULONG *) &pid);
    Assert (pid != NIL_RTPROCESS);

    if (mData->mSession.mState == SessionState_Spawning)
    {
        /* This machine is awaiting for a spawning session to be opened, so
         * reject any other open attempts from processes other than one
         * started by #openRemoteSession(). */

        LogFlowThisFunc (("mSession.mPid=%d(0x%x)\n",
                          mData->mSession.mPid, mData->mSession.mPid));
        LogFlowThisFunc (("session.pid=%d(0x%x)\n", pid, pid));

        if (mData->mSession.mPid != pid)
            return setError (E_ACCESSDENIED,
                tr ("An unexpected process (PID=0x%08X) has tried to open a direct "
                    "session with the machine named '%ls', while only a process "
                    "started by OpenRemoteSession (PID=0x%08X) is allowed"),
                pid, mUserData->mName.raw(), mData->mSession.mPid);
    }

    /* create a SessionMachine object */
    ComObjPtr <SessionMachine> sessionMachine;
    sessionMachine.createObject();
    HRESULT rc = sessionMachine->init (this);
    AssertComRC (rc);

    if (SUCCEEDED (rc))
    {
        /*
         *  Set the session state to Spawning to protect against subsequent
         *  attempts to open a session and to unregister the machine after
         *  we leave the lock.
         */
        SessionState_T origState = mData->mSession.mState;
        mData->mSession.mState = SessionState_Spawning;

        /*
         *  Leave the lock before calling the client process -- it will call
         *  Machine/SessionMachine methods. Leaving the lock here is quite safe
         *  because the state is Spawning, so that openRemotesession() and
         *  openExistingSession() calls will fail. This method, called before we
         *  enter the lock again, will fail because of the wrong PID.
         *
         *  Note that mData->mSession.mRemoteControls accessed outside
         *  the lock may not be modified when state is Spawning, so it's safe.
         */
        alock.leave();

        LogFlowThisFunc (("Calling AssignMachine()...\n"));
        rc = aControl->AssignMachine (sessionMachine);
        LogFlowThisFunc (("AssignMachine() returned %08X\n", rc));

        /* The failure may w/o any error info (from RPC), so provide one */
        if (FAILED (rc))
            setError (rc,
                tr ("Failed to assign the machine to the session"));

        if (SUCCEEDED (rc) && origState == SessionState_Spawning)
        {
            /* complete the remote session initialization */

            /* get the console from the direct session */
            ComPtr <IConsole> console;
            rc = aControl->GetRemoteConsole (console.asOutParam());
            ComAssertComRC (rc);

            if (SUCCEEDED (rc) && !console)
            {
                ComAssert (!!console);
                rc = E_FAIL;
            }

            /* assign machine & console to the remote sesion */
            if (SUCCEEDED (rc))
            {
                /*
                 *  after openRemoteSession(), the first and the only
                 *  entry in remoteControls is that remote session
                 */
                LogFlowThisFunc (("Calling AssignRemoteMachine()...\n"));
                rc = mData->mSession.mRemoteControls.front()->
                    AssignRemoteMachine (sessionMachine, console);
                LogFlowThisFunc (("AssignRemoteMachine() returned %08X\n", rc));

                /* The failure may w/o any error info (from RPC), so provide one */
                if (FAILED (rc))
                    setError (rc,
                        tr ("Failed to assign the machine to the remote session"));
            }

            if (FAILED (rc))
                aControl->Uninitialize();
        }

        /* enter the lock again */
        alock.enter();

        /* Restore the session state */
        mData->mSession.mState = origState;
    }

    /* finalize spawning amyway (this is why we don't return on errors above) */
    if (mData->mSession.mState == SessionState_Spawning)
    {
        /* Note that the progress object is finalized later */

        /* We don't reset mSession.mPid and mType here because both are
         * necessary for SessionMachine::uninit() to reap the child process
         * later. */

        if (FAILED (rc))
        {
            /* Remove the remote control from the list on failure
             * and reset session state to Closed. */
            mData->mSession.mRemoteControls.clear();
            mData->mSession.mState = SessionState_Closed;
        }
    }
    else
    {
        /* memorize PID of the directly opened session */
        if (SUCCEEDED (rc))
            mData->mSession.mPid = pid;
    }

    if (SUCCEEDED (rc))
    {
        /* memorize the direct session control and cache IUnknown for it */
        mData->mSession.mDirectControl = aControl;
        mData->mSession.mState = SessionState_Open;
        /* associate the SessionMachine with this Machine */
        mData->mSession.mMachine = sessionMachine;

        /* request an IUnknown pointer early from the remote party for later
         * identity checks (it will be internally cached within mDirectControl
         * at least on XPCOM) */
        ComPtr <IUnknown> unk = mData->mSession.mDirectControl;
        NOREF (unk);
    }

    if (mData->mSession.mProgress)
    {
        /* finalize the progress after setting the state, for consistency */
        mData->mSession.mProgress->notifyComplete (rc);
        mData->mSession.mProgress.setNull();
    }

    /* uninitialize the created session machine on failure */
    if (FAILED (rc))
        sessionMachine->uninit();

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  @note Locks this object for writing, calls the client process
 *        (inside the lock).
 */
HRESULT Machine::openRemoteSession (IInternalSessionControl *aControl,
                                    INPTR BSTR aType, INPTR BSTR aEnvironment,
                                    Progress *aProgress)
{
    LogFlowThisFuncEnter();

    AssertReturn (aControl, E_FAIL);
    AssertReturn (aProgress, E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (!mData->mRegistered)
        return setError (E_UNEXPECTED,
            tr ("The machine '%ls' is not registered"), mUserData->mName.raw());

    LogFlowThisFunc (("mSession.mState=%d\n", mData->mSession.mState));

    if (mData->mSession.mState == SessionState_Open ||
        mData->mSession.mState == SessionState_Spawning ||
        mData->mSession.mState == SessionState_Closing)
        return setError (E_ACCESSDENIED,
            tr ("A session for the machine '%ls' is currently open "
                "(or being opened or closed)"),
            mUserData->mName.raw());

    /* may not be Running */
    AssertReturn (mData->mMachineState < MachineState_Running, E_FAIL);

    /* get the path to the executable */
    char path [RTPATH_MAX];
    RTPathAppPrivateArch (path, RTPATH_MAX);
    size_t sz = strlen (path);
    path [sz++] = RTPATH_DELIMITER;
    path [sz] = 0;
    char *cmd = path + sz;
    sz = RTPATH_MAX - sz;

    int vrc = VINF_SUCCESS;
    RTPROCESS pid = NIL_RTPROCESS;

    RTENV env = RTENV_DEFAULT;

    if (aEnvironment)
    {
        char *newEnvStr = NULL;

        do
        {
            /* clone the current environment */
            int vrc2 = RTEnvClone (&env, RTENV_DEFAULT);
            AssertRCBreak (vrc2, vrc = vrc2);

            newEnvStr = RTStrDup(Utf8Str (aEnvironment));
            AssertPtrBreak (newEnvStr, vrc = vrc2);

            /* put new variables to the environment
             * (ignore empty variable names here since RTEnv API
             * intentionally doesn't do that) */
            char *var = newEnvStr;
            for (char *p = newEnvStr; *p; ++ p)
            {
                if (*p == '\n' && (p == newEnvStr || *(p - 1) != '\\'))
                {
                    *p = '\0';
                    if (*var)
                    {
                        char *val = strchr (var, '=');
                        if (val)
                        {
                            *val++ = '\0';
                            vrc2 = RTEnvSetEx (env, var, val);
                        }
                        else
                            vrc2 = RTEnvUnsetEx (env, var);
                        if (VBOX_FAILURE (vrc2))
                            break;
                    }
                    var = p + 1;
                }
            }
            if (VBOX_SUCCESS (vrc2) && *var)
                vrc2 = RTEnvPutEx (env, var);

            AssertRCBreak (vrc2, vrc = vrc2);
        }
        while (0);

        if (newEnvStr != NULL)
            RTStrFree(newEnvStr);
    }

    Bstr type (aType);
    if (type == "gui" || type == "GUI/Qt3")
    {
#ifdef RT_OS_DARWIN /* Avoid Lanuch Services confusing this with the selector by using a helper app. */
        const char VirtualBox_exe[] = "../Resources/VirtualBoxVM.app/Contents/MacOS/VirtualBoxVM";
#else
        const char VirtualBox_exe[] = "VirtualBox" HOSTSUFF_EXE;
#endif
        Assert (sz >= sizeof (VirtualBox_exe));
        strcpy (cmd, VirtualBox_exe);

        Utf8Str idStr = mData->mUuid.toString();
#ifdef RT_OS_WINDOWS /** @todo drop this once the RTProcCreate bug has been fixed */
        const char * args[] = {path, "-startvm", idStr, 0 };
#else
        Utf8Str name = mUserData->mName;
        const char * args[] = {path, "-comment", name, "-startvm", idStr, 0 };
#endif
        vrc = RTProcCreate (path, args, env, 0, &pid);
    }
    else
    if (type == "GUI/Qt4")
    {
#ifdef RT_OS_DARWIN /* Avoid Lanuch Services confusing this with the selector by using a helper app. */
        const char VirtualBox_exe[] = "../Resources/VirtualBoxVM.app/Contents/MacOS/VirtualBoxVM4";
#else
        const char VirtualBox_exe[] = "VirtualBox4" HOSTSUFF_EXE;
#endif
        Assert (sz >= sizeof (VirtualBox_exe));
        strcpy (cmd, VirtualBox_exe);

        Utf8Str idStr = mData->mUuid.toString();
#ifdef RT_OS_WINDOWS /** @todo drop this once the RTProcCreate bug has been fixed */
        const char * args[] = {path, "-startvm", idStr, 0 };
#else
        Utf8Str name = mUserData->mName;
        const char * args[] = {path, "-comment", name, "-startvm", idStr, 0 };
#endif
        vrc = RTProcCreate (path, args, env, 0, &pid);
    }
    else
#ifdef VBOX_VRDP
    if (type == "vrdp")
    {
        const char VBoxVRDP_exe[] = "VBoxHeadless" HOSTSUFF_EXE;
        Assert (sz >= sizeof (VBoxVRDP_exe));
        strcpy (cmd, VBoxVRDP_exe);

        Utf8Str idStr = mData->mUuid.toString();
#ifdef RT_OS_WINDOWS
        const char * args[] = {path, "-startvm", idStr, 0 };
#else
        Utf8Str name = mUserData->mName;
        const char * args[] = {path, "-comment", name, "-startvm", idStr, 0 };
#endif
        vrc = RTProcCreate (path, args, env, 0, &pid);
    }
    else
#endif /* VBOX_VRDP */
    if (type == "capture")
    {
        const char VBoxVRDP_exe[] = "VBoxHeadless" HOSTSUFF_EXE;
        Assert (sz >= sizeof (VBoxVRDP_exe));
        strcpy (cmd, VBoxVRDP_exe);

        Utf8Str idStr = mData->mUuid.toString();
#ifdef RT_OS_WINDOWS
        const char * args[] = {path, "-startvm", idStr, "-capture", 0 };
#else
        Utf8Str name = mUserData->mName;
        const char * args[] = {path, "-comment", name, "-startvm", idStr, "-capture", 0 };
#endif
        vrc = RTProcCreate (path, args, env, 0, &pid);
    }
    else
    {
        RTEnvDestroy (env);
        return setError (E_INVALIDARG,
            tr ("Invalid session type: '%ls'"), aType);
    }

    RTEnvDestroy (env);

    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Could not launch a process for the machine '%ls' (%Vrc)"),
            mUserData->mName.raw(), vrc);

    LogFlowThisFunc (("launched.pid=%d(0x%x)\n", pid, pid));

    /*
     *  Note that we don't leave the lock here before calling the client,
     *  because it doesn't need to call us back if called with a NULL argument.
     *  Leaving the lock herer is dangerous because we didn't prepare the
     *  launch data yet, but the client we've just started may happen to be
     *  too fast and call openSession() that will fail (because of PID, etc.),
     *  so that the Machine will never get out of the Spawning session state.
     */

    /* inform the session that it will be a remote one */
    LogFlowThisFunc (("Calling AssignMachine (NULL)...\n"));
    HRESULT rc = aControl->AssignMachine (NULL);
    LogFlowThisFunc (("AssignMachine (NULL) returned %08X\n", rc));

    if (FAILED (rc))
    {
        /* restore the session state */
        mData->mSession.mState = SessionState_Closed;
        /* The failure may w/o any error info (from RPC), so provide one */
        return setError (rc,
            tr ("Failed to assign the machine to the session"));
    }

    /* attach launch data to the machine */
    Assert (mData->mSession.mPid == NIL_RTPROCESS);
    mData->mSession.mRemoteControls.push_back (aControl);
    mData->mSession.mProgress = aProgress;
    mData->mSession.mPid = pid;
    mData->mSession.mState = SessionState_Spawning;
    mData->mSession.mType = type;

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  @note Locks this object for writing, calls the client process
 *        (outside the lock).
 */
HRESULT Machine::openExistingSession (IInternalSessionControl *aControl)
{
    LogFlowThisFuncEnter();

    AssertReturn (aControl, E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (!mData->mRegistered)
        return setError (E_UNEXPECTED,
            tr ("The machine '%ls' is not registered"), mUserData->mName.raw());

    LogFlowThisFunc (("mSession.state=%d\n", mData->mSession.mState));

    if (mData->mSession.mState != SessionState_Open)
        return setError (E_ACCESSDENIED,
            tr ("The machine '%ls' does not have an open session"),
            mUserData->mName.raw());

    ComAssertRet (!mData->mSession.mDirectControl.isNull(), E_FAIL);

    /*
     *  Get the console from the direct session (note that we don't leave the
     *  lock here because GetRemoteConsole must not call us back).
     */
    ComPtr <IConsole> console;
    HRESULT rc = mData->mSession.mDirectControl->
                     GetRemoteConsole (console.asOutParam());
    if (FAILED (rc))
    {
        /* The failure may w/o any error info (from RPC), so provide one */
        return setError (rc,
            tr ("Failed to get a console object from the direct session"));
    }

    ComAssertRet (!console.isNull(), E_FAIL);

    ComObjPtr <SessionMachine> sessionMachine = mData->mSession.mMachine;
    AssertReturn (!sessionMachine.isNull(), E_FAIL);

    /*
     *  Leave the lock before calling the client process. It's safe here
     *  since the only thing to do after we get the lock again is to add
     *  the remote control to the list (which doesn't directly influence
     *  anything).
     */
    alock.leave();

    /* attach the remote session to the machine */
    LogFlowThisFunc (("Calling AssignRemoteMachine()...\n"));
    rc = aControl->AssignRemoteMachine (sessionMachine, console);
    LogFlowThisFunc (("AssignRemoteMachine() returned %08X\n", rc));

    /* The failure may w/o any error info (from RPC), so provide one */
    if (FAILED (rc))
        return setError (rc,
            tr ("Failed to assign the machine to the session"));

    alock.enter();

    /* need to revalidate the state after entering the lock again */
    if (mData->mSession.mState != SessionState_Open)
    {
        aControl->Uninitialize();

        return setError (E_ACCESSDENIED,
            tr ("The machine '%ls' does not have an open session"),
            mUserData->mName.raw());
    }

    /* store the control in the list */
    mData->mSession.mRemoteControls.push_back (aControl);

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Checks that the registered flag of the machine can be set according to
 *  the argument and sets it. On success, commits and saves all settings.
 *
 *  @note When this machine is inaccessible, the only valid value for \a
 *  aRegistered is FALSE (i.e. unregister the machine) because unregistered
 *  inaccessible machines are not currently supported. Note that unregistering
 *  an inaccessible machine will \b uninitialize this machine object. Therefore,
 *  the caller must make sure there are no active Machine::addCaller() calls
 *  on the current thread because this will block Machine::uninit().
 *
 *  @note Must be called from mParent's write lock. Locks this object and
 *  children for writing.
 */
HRESULT Machine::trySetRegistered (BOOL aRegistered)
{
    AssertReturn (mParent->isLockedOnCurrentThread(), E_FAIL);

    AutoLimitedCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    /* wait for state dependants to drop to zero */
    ensureNoStateDependencies (alock);

    ComAssertRet (mData->mRegistered != aRegistered, E_FAIL);

    if (!mData->mAccessible)
    {
        /* A special case: the machine is not accessible. */

        /* inaccessible machines can only be unregistered */
        AssertReturn (!aRegistered, E_FAIL);

        /* Uninitialize ourselves here because currently there may be no
         * unregistered that are inaccessible (this state combination is not
         * supported). Note releasing the caller and leaving the lock before
         * calling uninit() */

        alock.leave();
        autoCaller.release();

        uninit();

        return S_OK;
    }

    AssertReturn (autoCaller.state() == Ready, E_FAIL);

    if (aRegistered)
    {
        if (mData->mRegistered)
            return setError (E_FAIL,
                tr ("The machine '%ls' with UUID {%s} is already registered"),
                mUserData->mName.raw(),
                mData->mUuid.toString().raw());
    }
    else
    {
        if (mData->mMachineState == MachineState_Saved)
            return setError (E_FAIL,
                tr ("Cannot unregister the machine '%ls' because it "
                    "is in the Saved state"),
                mUserData->mName.raw());

        size_t snapshotCount = 0;
        if (mData->mFirstSnapshot)
            snapshotCount = mData->mFirstSnapshot->descendantCount() + 1;
        if (snapshotCount)
            return setError (E_FAIL,
                tr ("Cannot unregister the machine '%ls' because it "
                    "has %d snapshots"),
                mUserData->mName.raw(), snapshotCount);

        if (mData->mSession.mState != SessionState_Closed)
            return setError (E_FAIL,
                tr ("Cannot unregister the machine '%ls' because it has an "
                    "open session"),
                mUserData->mName.raw());

        if (mHDData->mHDAttachments.size() != 0)
            return setError (E_FAIL,
                tr ("Cannot unregister the machine '%ls' because it "
                    "has %d hard disks attached"),
                mUserData->mName.raw(), mHDData->mHDAttachments.size());
    }

    /* Ensure the settings are saved. If we are going to be registered and
     * isConfigLocked() is FALSE then it means that no config file exists yet,
     * so create it. */
    if (isModified() || (aRegistered && !isConfigLocked()))
    {
        HRESULT rc = saveSettings();
        CheckComRCReturnRC (rc);
    }

    mData->mRegistered = aRegistered;

    /* inform the USB proxy about all attached/detached USB filters */
    mUSBController->onMachineRegistered (aRegistered);

    return S_OK;
}

/**
 * Increases the number of objects dependent on the machine state or on the
 * registered state. Guarantees that these two states will not change at least
 * until #releaseStateDependency() is called.
 *
 * Depending on the @a aDepType value, additional state checks may be made.
 * These checks will set extended error info on failure. See
 * #checkStateDependency() for more info.
 *
 * If this method returns a failure, the dependency is not added and the caller
 * is not allowed to rely on any particular machine state or registration state
 * value and may return the failed result code to the upper level.
 *
 * @param aDepType      Dependency type to add.
 * @param aState        Current machine state (NULL if not interested).
 * @param aRegistered   Current registered state (NULL if not interested).
 *
 * @note Locks this object for reading.
 */
HRESULT Machine::addStateDependency (StateDependency aDepType /* = AnyStateDep */,
                                     MachineState_T *aState /* = NULL */,
                                     BOOL *aRegistered /* = NULL */)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    HRESULT rc = checkStateDependency (aDepType);
    CheckComRCReturnRC (rc);

    {
        AutoLock stateLock (stateLockHandle());

        if (mData->mMachineStateChangePending != 0)
        {
            /* ensureNoStateDependencies() is waiting for state dependencies to
             * drop to zero so don't add more. It may make sense to wait a bit
             * and retry before reporting an error (since the pending state
             * transition should be really quick) but let's just assert for
             * now to see if it ever happens on practice. */

            AssertFailed();

            return setError (E_ACCESSDENIED,
                tr ("Machine state change is in progress. "
                    "Please retry the operation later."));
        }

        ++ mData->mMachineStateDeps;
        Assert (mData->mMachineStateDeps != 0 /* overflow */);
    }

    if (aState)
        *aState = mData->mMachineState;
    if (aRegistered)
        *aRegistered = mData->mRegistered;

    return S_OK;
}

/**
 * Decreases the number of objects dependent on the machine state.
 * Must always complete the #addStateDependency() call after the state
 * dependency is no more necessary.
 */
void Machine::releaseStateDependency()
{
    /* stateLockHandle() is the same handle that is used by AutoCaller
     * so lock it in advance to avoid two mutex requests in a raw */
    AutoLock stateLock (stateLockHandle());

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AssertReturnVoid (mData->mMachineStateDeps != 0
                      /* releaseStateDependency() w/o addStateDependency()? */);
    -- mData->mMachineStateDeps;

    if (mData->mMachineStateDeps == 0)
    {
        /* inform ensureNoStateDependencies() that there are no more deps */
        if (mData->mMachineStateChangePending != 0)
        {
            Assert (mData->mMachineStateDepsSem != NIL_RTSEMEVENTMULTI);
            RTSemEventMultiSignal (mData->mMachineStateDepsSem);
        }
    }
}

// protected methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  Performs machine state checks based on the @a aDepType value. If a check
 *  fails, this method will set extended error info, otherwise it will return
 *  S_OK. It is supposed, that on failure, the caller will immedieately return
 *  the return value of this method to the upper level.
 *
 *  When @a aDepType is AnyStateDep, this method always returns S_OK.
 *
 *  When @a aDepType is MutableStateDep, this method returns S_OK only if the
 *  current state of this machine object allows to change settings of the
 *  machine (i.e. the machine is not registered, or registered but not running
 *  and not saved). It is useful to call this method from Machine setters
 *  before performing any change.
 *
 *  When @a aDepType is MutableOrSavedStateDep, this method behaves the same
 *  as for MutableStateDep except that if the machine is saved, S_OK is also
 *  returned. This is useful in setters which allow changing machine
 *  properties when it is in the saved state.
 *
 *  @param aDepType     Dependency type to check.
 *
 *  @note Non Machine based classes should use #addStateDependency() and
 *  #releaseStateDependency() methods or the smart AutoStateDependency
 *  template.
 *
 *  @note This method must be called from under this object's read or write
 *        lock.
 */
HRESULT Machine::checkStateDependency (StateDependency aDepType)
{
    switch (aDepType)
    {
        case AnyStateDep:
        {
            break;
        }
        case MutableStateDep:
        {
            if (mData->mRegistered &&
                (mType != IsSessionMachine ||
                 mData->mMachineState > MachineState_Paused ||
                 mData->mMachineState == MachineState_Saved))
                return setError (E_ACCESSDENIED,
                    tr ("The machine is not mutable (state is %d)"),
                    mData->mMachineState);
            break;
        }
        case MutableOrSavedStateDep:
        {
            if (mData->mRegistered &&
                (mType != IsSessionMachine ||
                 mData->mMachineState > MachineState_Paused))
                return setError (E_ACCESSDENIED,
                    tr ("The machine is not mutable (state is %d)"),
                    mData->mMachineState);
            break;
        }
    }

    return S_OK;
}

/**
 *  Helper to initialize all associated child objects
 *  and allocate data structures.
 *
 *  This method must be called as a part of the object's initialization
 *  procedure (usually done in the #init() method).
 *
 *  @note Must be called only from #init() or from #registeredInit().
 */
HRESULT Machine::initDataAndChildObjects()
{
    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());
    AssertComRCReturn (autoCaller.state() == InInit ||
                       autoCaller.state() == Limited, E_FAIL);

    /* allocate data structures */
    mSSData.allocate();
    mUserData.allocate();
    mHWData.allocate();
    mHDData.allocate();

    /* initialize mOSTypeId */
    mUserData->mOSTypeId = mParent->getUnknownOSType()->id();

    /* create associated BIOS settings object */
    unconst (mBIOSSettings).createObject();
    mBIOSSettings->init (this);

#ifdef VBOX_VRDP
    /* create an associated VRDPServer object (default is disabled) */
    unconst (mVRDPServer).createObject();
    mVRDPServer->init (this);
#endif

    /* create an associated DVD drive object */
    unconst (mDVDDrive).createObject();
    mDVDDrive->init (this);

    /* create an associated floppy drive object */
    unconst (mFloppyDrive).createObject();
    mFloppyDrive->init (this);

    /* create associated serial port objects */
    for (ULONG slot = 0; slot < ELEMENTS (mSerialPorts); slot ++)
    {
        unconst (mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->init (this, slot);
    }

    /* create associated parallel port objects */
    for (ULONG slot = 0; slot < ELEMENTS (mParallelPorts); slot ++)
    {
        unconst (mParallelPorts [slot]).createObject();
        mParallelPorts [slot]->init (this, slot);
    }

    /* create the audio adapter object (always present, default is disabled) */
    unconst (mAudioAdapter).createObject();
    mAudioAdapter->init (this);

    /* create the USB controller object (always present, default is disabled) */
    unconst (mUSBController).createObject();
    mUSBController->init (this);

    /* create the SATA controller object (always present, default is disabled) */
    unconst (mSATAController).createObject();
    mSATAController->init (this);

    /* create associated network adapter objects */
    for (ULONG slot = 0; slot < ELEMENTS (mNetworkAdapters); slot ++)
    {
        unconst (mNetworkAdapters [slot]).createObject();
        mNetworkAdapters [slot]->init (this, slot);
    }

    return S_OK;
}

/**
 *  Helper to uninitialize all associated child objects
 *  and to free all data structures.
 *
 *  This method must be called as a part of the object's uninitialization
 *  procedure (usually done in the #uninit() method).
 *
 *  @note Must be called only from #uninit() or from #registeredInit().
 */
void Machine::uninitDataAndChildObjects()
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());
    AssertComRCReturnVoid (autoCaller.state() == InUninit ||
                           autoCaller.state() == Limited);

    /* uninit all children using addDependentChild()/removeDependentChild()
     * in their init()/uninit() methods */
    uninitDependentChildren();

    /* tell all our other child objects we've been uninitialized */

    for (ULONG slot = 0; slot < ELEMENTS (mNetworkAdapters); slot ++)
    {
        if (mNetworkAdapters [slot])
        {
            mNetworkAdapters [slot]->uninit();
            unconst (mNetworkAdapters [slot]).setNull();
        }
    }

    if (mUSBController)
    {
        mUSBController->uninit();
        unconst (mUSBController).setNull();
    }

    if (mSATAController)
    {
        mSATAController->uninit();
        unconst (mSATAController).setNull();
    }

    if (mAudioAdapter)
    {
        mAudioAdapter->uninit();
        unconst (mAudioAdapter).setNull();
    }

    for (ULONG slot = 0; slot < ELEMENTS (mParallelPorts); slot ++)
    {
        if (mParallelPorts [slot])
        {
            mParallelPorts [slot]->uninit();
            unconst (mParallelPorts [slot]).setNull();
        }
    }

    for (ULONG slot = 0; slot < ELEMENTS (mSerialPorts); slot ++)
    {
        if (mSerialPorts [slot])
        {
            mSerialPorts [slot]->uninit();
            unconst (mSerialPorts [slot]).setNull();
        }
    }

    if (mFloppyDrive)
    {
        mFloppyDrive->uninit();
        unconst (mFloppyDrive).setNull();
    }

    if (mDVDDrive)
    {
        mDVDDrive->uninit();
        unconst (mDVDDrive).setNull();
    }

#ifdef VBOX_VRDP
    if (mVRDPServer)
    {
        mVRDPServer->uninit();
        unconst (mVRDPServer).setNull();
    }
#endif

    if (mBIOSSettings)
    {
        mBIOSSettings->uninit();
        unconst (mBIOSSettings).setNull();
    }

    /* Deassociate hard disks (only when a real Machine or a SnapshotMachine
     * instance is uninitialized; SessionMachine instances refer to real
     * Machine hard disks). This is necessary for a clean re-initialization of
     * the VM after successfully re-checking the accessibility state. Note
     * that in case of normal Machine or SnapshotMachine uninitialization (as
     * a result of unregistering or discarding the snapshot), outdated hard
     * disk attachments will already be uninitialized and deleted, so this
     * code will not affect them. */
    if (!!mHDData && (mType == IsMachine || mType == IsSnapshotMachine))
    {
        for (HDData::HDAttachmentList::const_iterator it =
                 mHDData->mHDAttachments.begin();
             it != mHDData->mHDAttachments.end();
             ++ it)
        {
            (*it)->hardDisk()->setMachineId (Guid());
        }
    }

    if (mType == IsMachine)
    {
        /* reset some important fields of mData */
        mData->mCurrentSnapshot.setNull();
        mData->mFirstSnapshot.setNull();
    }

    /* free data structures (the essential mData structure is not freed here
     * since it may be still in use) */
    mHDData.free();
    mHWData.free();
    mUserData.free();
    mSSData.free();
}

/**
 * Makes sure that there are no machine state dependants. If necessary, waits
 * for the number of dependants to drop to zero. Must be called from under this
 * object's write lock which will be released while waiting.
 *
 * @param aLock This object's write lock.
 *
 * @warning To be used only in methods that change the machine state!
 */
void Machine::ensureNoStateDependencies (AutoLock &aLock)
{
    AssertReturnVoid (aLock.belongsTo (this));
    AssertReturnVoid (aLock.isLockedOnCurrentThread());

    AutoLock stateLock (stateLockHandle());

    /* Wait for all state dependants if necessary */
    if (mData->mMachineStateDeps != 0)
    {
        /* lazy semaphore creation */
        if (mData->mMachineStateDepsSem == NIL_RTSEMEVENTMULTI)
            RTSemEventMultiCreate (&mData->mMachineStateDepsSem);

        LogFlowThisFunc (("Waiting for state deps (%d) to drop to zero...\n",
                          mData->mMachineStateDeps));

        ++ mData->mMachineStateChangePending;

        /* reset the semaphore before waiting, the last dependant will signal
         * it */
        RTSemEventMultiReset (mData->mMachineStateDepsSem);

        stateLock.leave();
        aLock.leave();

        RTSemEventMultiWait (mData->mMachineStateDepsSem, RT_INDEFINITE_WAIT);

        aLock.enter();
        stateLock.enter();

        -- mData->mMachineStateChangePending;
    }
}

/**
 *  Helper to change the machine state.
 *
 *  @note Locks this object for writing.
 */
HRESULT Machine::setMachineState (MachineState_T aMachineState)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aMachineState=%d\n", aMachineState));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoLock alock (this);

    /* wait for state dependants to drop to zero */
    ensureNoStateDependencies (alock);

    if (mData->mMachineState != aMachineState)
    {
        mData->mMachineState = aMachineState;

        RTTimeNow (&mData->mLastStateChange);

        mParent->onMachineStateChange (mData->mUuid, aMachineState);
    }

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Searches for a shared folder with the given logical name
 *  in the collection of shared folders.
 *
 *  @param aName            logical name of the shared folder
 *  @param aSharedFolder    where to return the found object
 *  @param aSetError        whether to set the error info if the folder is
 *                          not found
 *  @return
 *      S_OK when found or E_INVALIDARG when not found
 *
 *  @note
 *      must be called from under the object's lock!
 */
HRESULT Machine::findSharedFolder (const BSTR aName,
                                   ComObjPtr <SharedFolder> &aSharedFolder,
                                   bool aSetError /* = false */)
{
    bool found = false;
    for (HWData::SharedFolderList::const_iterator it = mHWData->mSharedFolders.begin();
        !found && it != mHWData->mSharedFolders.end();
        ++ it)
    {
        AutoLock alock (*it);
        found = (*it)->name() == aName;
        if (found)
            aSharedFolder = *it;
    }

    HRESULT rc = found ? S_OK : E_INVALIDARG;

    if (aSetError && !found)
        setError (rc, tr ("Could not find a shared folder named '%ls'"), aName);

    return rc;
}

/**
 *  Loads all the VM settings by walking down the <Machine> node.
 *
 *  @param aRegistered  true when the machine is being loaded on VirtualBox
 *                      startup
 *
 *  @note This method is intended to be called only from init(), so it assumes
 *  all machine data fields have appropriate default values when it is called.
 *
 *  @note Doesn't lock any objects.
 */
HRESULT Machine::loadSettings (bool aRegistered)
{
    LogFlowThisFuncEnter();
    AssertReturn (mType == IsMachine, E_FAIL);

    AutoCaller autoCaller (this);
    AssertReturn (autoCaller.state() == InInit, E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        using namespace settings;

#if 0
        /// @todo disabled until made thread-safe by using handle duplicates
        File file (File::Read, mData->mHandleCfgFile,
                   Utf8Str (mData->mConfigFileFull));
#else
        File file (File::Read, Utf8Str (mData->mConfigFileFull));
#endif
        XmlTreeBackend tree;

        rc = VirtualBox::loadSettingsTree_FirstTime (tree, file,
                                                     mData->mSettingsFileVersion);
        CheckComRCThrowRC (rc);

        Key machineNode = tree.rootKey().key ("Machine");

        /* uuid (required) */
        Guid id = machineNode.value <Guid> ("uuid");

        /* If the stored UUID is not empty, it means the registered machine
         * is being loaded. Compare the loaded UUID with the stored one taken
         * from the global registry. */
        if (!mData->mUuid.isEmpty())
        {
            if (mData->mUuid != id)
            {
                throw setError (E_FAIL,
                    tr ("Machine UUID {%Vuuid} in '%ls' doesn't match its "
                        "UUID {%s} in the registry file '%ls'"),
                    id.raw(), mData->mConfigFileFull.raw(),
                    mData->mUuid.toString().raw(),
                    mParent->settingsFileName().raw());
            }
        }
        else
            unconst (mData->mUuid) = id;

        /* name (required) */
        mUserData->mName = machineNode.stringValue ("name");

        /* nameSync (optional, default is true) */
        mUserData->mNameSync = machineNode.value <bool> ("nameSync");

        /* Description (optional, default is null) */
        {
            Key descNode = machineNode.findKey ("Description");
            if (!descNode.isNull())
                mUserData->mDescription = descNode.keyStringValue();
            else
                mUserData->mDescription.setNull();
        }

        /* OSType (required) */
        {
            mUserData->mOSTypeId = machineNode.stringValue ("OSType");

            /* look up the object by Id to check it is valid */
            ComPtr <IGuestOSType> guestOSType;
            rc = mParent->GetGuestOSType (mUserData->mOSTypeId,
                                          guestOSType.asOutParam());
            CheckComRCThrowRC (rc);
        }

        /* stateFile (optional) */
        {
            Bstr stateFilePath = machineNode.stringValue ("stateFile");
            if (stateFilePath)
            {
                Utf8Str stateFilePathFull = stateFilePath;
                int vrc = calculateFullPath (stateFilePathFull, stateFilePathFull);
                if (VBOX_FAILURE (vrc))
                {
                    throw setError (E_FAIL,
                        tr ("Invalid saved state file path: '%ls' (%Vrc)"),
                        stateFilePath.raw(), vrc);
                }
                mSSData->mStateFilePath = stateFilePathFull;
            }
            else
                mSSData->mStateFilePath.setNull();
        }

        /*
         *  currentSnapshot ID (optional)
         *
         *  Note that due to XML Schema constaraints, this attribute, when
         *  present, will guaranteedly refer to an existing snapshot
         *  definition in XML
         */
        Guid currentSnapshotId = machineNode.valueOr <Guid> ("currentSnapshot",
                                                             Guid());

        /* snapshotFolder (optional) */
        {
            Bstr folder = machineNode.stringValue ("snapshotFolder");
            rc = COMSETTER(SnapshotFolder) (folder);
            CheckComRCThrowRC (rc);
        }

        /* currentStateModified (optional, default is true) */
        mData->mCurrentStateModified = machineNode.value <bool> ("currentStateModified");

        /* lastStateChange (optional, defaults to now) */
        {
            RTTIMESPEC now;
            RTTimeNow (&now);
            mData->mLastStateChange =
                machineNode.valueOr <RTTIMESPEC> ("lastStateChange", now);
        }

        /* aborted (optional, default is false) */
        bool aborted = machineNode.value <bool> ("aborted");

        /*
         *  note: all mUserData members must be assigned prior this point because
         *  we need to commit changes in order to let mUserData be shared by all
         *  snapshot machine instances.
         */
        mUserData.commitCopy();

        /* Snapshot node (optional) */
        {
            Key snapshotNode = machineNode.findKey ("Snapshot");
            if (!snapshotNode.isNull())
            {
                /* read all snapshots recursively */
                rc = loadSnapshot (snapshotNode, currentSnapshotId, NULL);
                CheckComRCThrowRC (rc);
            }
        }

        /* Hardware node (required) */
        rc = loadHardware (machineNode.key ("Hardware"));
        CheckComRCThrowRC (rc);

        /* HardDiskAttachments node (required) */
        rc = loadHardDisks (machineNode.key ("HardDiskAttachments"), aRegistered);
        CheckComRCThrowRC (rc);

        /*
         *  NOTE: the assignment below must be the last thing to do,
         *  otherwise it will be not possible to change the settings
         *  somewehere in the code above because all setters will be
         *  blocked by checkStateDependency (MutableStateDep).
         */

        /* set the machine state to Aborted or Saved when appropriate */
        if (aborted)
        {
            Assert (!mSSData->mStateFilePath);
            mSSData->mStateFilePath.setNull();

            /* no need to use setMachineState() during init() */
            mData->mMachineState = MachineState_Aborted;
        }
        else if (mSSData->mStateFilePath)
        {
            /* no need to use setMachineState() during init() */
            mData->mMachineState = MachineState_Saved;
        }
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

    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  Recursively loads all snapshots starting from the given.
 *
 *  @param aNode            <Snapshot> node.
 *  @param aCurSnapshotId   Current snapshot ID from the settings file.
 *  @param aParentSnapshot  Parent snapshot.
 */
HRESULT Machine::loadSnapshot (const settings::Key &aNode,
                               const Guid &aCurSnapshotId,
                               Snapshot *aParentSnapshot)
{
    using namespace settings;

    AssertReturn (!aNode.isNull(), E_INVALIDARG);
    AssertReturn (mType == IsMachine, E_FAIL);

    /* create a snapshot machine object */
    ComObjPtr <SnapshotMachine> snapshotMachine;
    snapshotMachine.createObject();

    HRESULT rc = S_OK;

    /* required */
    Guid uuid = aNode.value <Guid> ("uuid");

    {
        /* optional */
        Bstr stateFilePath = aNode.stringValue ("stateFile");
        if (stateFilePath)
        {
            Utf8Str stateFilePathFull = stateFilePath;
            int vrc = calculateFullPath (stateFilePathFull, stateFilePathFull);
            if (VBOX_FAILURE (vrc))
                return setError (E_FAIL,
                                 tr ("Invalid saved state file path: '%ls' (%Vrc)"),
                                 stateFilePath.raw(), vrc);

            stateFilePath = stateFilePathFull;
        }

        /* Hardware node (required) */
        Key hardwareNode = aNode.key ("Hardware");

        /* HardDiskAttachments node (required) */
        Key hdasNode = aNode.key ("HardDiskAttachments");

        /* initialize the snapshot machine */
        rc = snapshotMachine->init (this, hardwareNode, hdasNode,
                                    uuid, stateFilePath);
        CheckComRCReturnRC (rc);
    }

    /* create a snapshot object */
    ComObjPtr <Snapshot> snapshot;
    snapshot.createObject();

    {
        /* required */
        Bstr name = aNode.stringValue ("name");

        /* required */
        RTTIMESPEC timeStamp = aNode.value <RTTIMESPEC> ("timeStamp");

        /* optional */
        Bstr description;
        {
            Key descNode = aNode.findKey ("Description");
            if (!descNode.isNull())
                description = descNode.keyStringValue();
        }

        /* initialize the snapshot */
        rc = snapshot->init (uuid, name, description, timeStamp,
                             snapshotMachine, aParentSnapshot);
        CheckComRCReturnRC (rc);
    }

    /* memorize the first snapshot if necessary */
    if (!mData->mFirstSnapshot)
        mData->mFirstSnapshot = snapshot;

    /* memorize the current snapshot when appropriate */
    if (!mData->mCurrentSnapshot && snapshot->data().mId == aCurSnapshotId)
        mData->mCurrentSnapshot = snapshot;

    /* Snapshots node (optional) */
    {
        Key snapshotsNode = aNode.findKey ("Snapshots");
        if (!snapshotsNode.isNull())
        {
            Key::List children = snapshotsNode.keys ("Snapshot");
            for (Key::List::const_iterator it = children.begin();
                 it != children.end(); ++ it)
            {
                rc = loadSnapshot ((*it), aCurSnapshotId, snapshot);
                CheckComRCBreakRC (rc);
            }
        }
    }

    return rc;
}

/**
 *  @param aNode    <Hardware> node.
 */
HRESULT Machine::loadHardware (const settings::Key &aNode)
{
    using namespace settings;

    AssertReturn (!aNode.isNull(), E_INVALIDARG);
    AssertReturn (mType == IsMachine || mType == IsSnapshotMachine, E_FAIL);

    HRESULT rc = S_OK;

    /* CPU node (currently not required) */
    {
        /* default value in case the node is not there */
        mHWData->mHWVirtExEnabled = TSBool_Default;
        mHWData->mPAEEnabled      = false;

        Key cpuNode = aNode.findKey ("CPU");
        if (!cpuNode.isNull())
        {
            Key hwVirtExNode = cpuNode.key ("HardwareVirtEx");
            if (!hwVirtExNode.isNull())
            {
                const char *enabled = hwVirtExNode.stringValue ("enabled");
                if      (strcmp (enabled, "false") == 0)
                    mHWData->mHWVirtExEnabled = TSBool_False;
                else if (strcmp (enabled, "true") == 0)
                    mHWData->mHWVirtExEnabled = TSBool_True;
                else
                    mHWData->mHWVirtExEnabled = TSBool_Default;
            }
            /* PAE (optional, default is false) */
            Key PAENode = cpuNode.findKey ("PAE");
            if (!PAENode.isNull())
            {
                mHWData->mPAEEnabled = PAENode.value <bool> ("enabled");
            }
        }
    }

    /* Memory node (required) */
    {
        Key memoryNode = aNode.key ("Memory");

        mHWData->mMemorySize = memoryNode.value <ULONG> ("RAMSize");
    }

    /* Boot node (required) */
    {
        /* reset all boot order positions to NoDevice */
        for (size_t i = 0; i < ELEMENTS (mHWData->mBootOrder); i++)
            mHWData->mBootOrder [i] = DeviceType_Null;

        Key bootNode = aNode.key ("Boot");

        Key::List orderNodes = bootNode.keys ("Order");
        for (Key::List::const_iterator it = orderNodes.begin();
             it != orderNodes.end(); ++ it)
        {
            /* position (required) */
            /* position unicity is guaranteed by XML Schema */
            uint32_t position = (*it).value <uint32_t> ("position");
            -- position;
            Assert (position < ELEMENTS (mHWData->mBootOrder));

            /* device (required) */
            const char *device = (*it).stringValue ("device");
            if      (strcmp (device, "None") == 0)
                mHWData->mBootOrder [position] = DeviceType_Null;
            else if (strcmp (device, "Floppy") == 0)
                mHWData->mBootOrder [position] = DeviceType_Floppy;
            else if (strcmp (device, "DVD") == 0)
                mHWData->mBootOrder [position] = DeviceType_DVD;
            else if (strcmp (device, "HardDisk") == 0)
                mHWData->mBootOrder [position] = DeviceType_HardDisk;
            else if (strcmp (device, "Network") == 0)
                mHWData->mBootOrder [position] = DeviceType_Network;
            else
                ComAssertMsgFailed (("Invalid device: %s\n", device));
        }
    }

    /* Display node (required) */
    {
        Key displayNode = aNode.key ("Display");

        mHWData->mVRAMSize = displayNode.value <ULONG> ("VRAMSize");
        mHWData->mMonitorCount = displayNode.value <ULONG> ("MonitorCount");
    }

#ifdef VBOX_VRDP
    /* RemoteDisplay */
    rc = mVRDPServer->loadSettings (aNode);
    CheckComRCReturnRC (rc);
#endif

    /* BIOS */
    rc = mBIOSSettings->loadSettings (aNode);
    CheckComRCReturnRC (rc);

    /* DVD drive */
    rc = mDVDDrive->loadSettings (aNode);
    CheckComRCReturnRC (rc);

    /* Floppy drive */
    rc = mFloppyDrive->loadSettings (aNode);
    CheckComRCReturnRC (rc);

    /* USB Controller */
    rc = mUSBController->loadSettings (aNode);
    CheckComRCReturnRC (rc);

    /* SATA Controller */
    rc = mSATAController->loadSettings (aNode);
    CheckComRCReturnRC (rc);

    /* Network node (required) */
    {
        /* we assume that all network adapters are initially disabled
         * and detached */

        Key networkNode = aNode.key ("Network");

        rc = S_OK;

        Key::List adapters = networkNode.keys ("Adapter");
        for (Key::List::const_iterator it = adapters.begin();
             it != adapters.end(); ++ it)
        {
            /* slot number (required) */
            /* slot unicity is guaranteed by XML Schema */
            uint32_t slot = (*it).value <uint32_t> ("slot");
            AssertBreakVoid (slot < ELEMENTS (mNetworkAdapters));

            rc = mNetworkAdapters [slot]->loadSettings (*it);
            CheckComRCReturnRC (rc);
        }
    }

    /* Serial node (required) */
    {
        Key serialNode = aNode.key ("UART");

        rc = S_OK;

        Key::List ports = serialNode.keys ("Port");
        for (Key::List::const_iterator it = ports.begin();
             it != ports.end(); ++ it)
        {
            /* slot number (required) */
            /* slot unicity is guaranteed by XML Schema */
            uint32_t slot = (*it).value <uint32_t> ("slot");
            AssertBreakVoid (slot < ELEMENTS (mSerialPorts));

            rc = mSerialPorts [slot]->loadSettings (*it);
            CheckComRCReturnRC (rc);
        }
    }

    /* Parallel node (optional) */
    {
        Key parallelNode = aNode.key ("LPT");

        rc = S_OK;

        Key::List ports = parallelNode.keys ("Port");
        for (Key::List::const_iterator it = ports.begin();
             it != ports.end(); ++ it)
        {
            /* slot number (required) */
            /* slot unicity is guaranteed by XML Schema */
            uint32_t slot = (*it).value <uint32_t> ("slot");
            AssertBreakVoid (slot < ELEMENTS (mSerialPorts));

            rc = mParallelPorts [slot]->loadSettings (*it);
            CheckComRCReturnRC (rc);
        }
    }

    /* AudioAdapter */
    rc = mAudioAdapter->loadSettings (aNode);
    CheckComRCReturnRC (rc);

    /* Shared folders (required) */
    {
        Key sharedFoldersNode = aNode.key ("SharedFolders");

        rc = S_OK;

        Key::List folders = sharedFoldersNode.keys ("SharedFolder");
        for (Key::List::const_iterator it = folders.begin();
             it != folders.end(); ++ it)
        {
            /* folder logical name (required) */
            Bstr name = (*it).stringValue ("name");
            /* folder host path (required) */
            Bstr hostPath = (*it).stringValue ("hostPath");

            bool writable = (*it).value <bool> ("writable");

            rc = CreateSharedFolder (name, hostPath, writable);
            CheckComRCReturnRC (rc);
        }
    }

    /* Clipboard node (required) */
    {
        Key clipNode = aNode.key ("Clipboard");

        const char *mode = clipNode.stringValue ("mode");
        if      (strcmp (mode, "Disabled") == 0)
            mHWData->mClipboardMode = ClipboardMode_Disabled;
        else if (strcmp (mode, "HostToGuest") == 0)
            mHWData->mClipboardMode = ClipboardMode_HostToGuest;
        else if (strcmp (mode, "GuestToHost") == 0)
            mHWData->mClipboardMode = ClipboardMode_GuestToHost;
        else if (strcmp (mode, "Bidirectional") == 0)
            mHWData->mClipboardMode = ClipboardMode_Bidirectional;
        else
            AssertMsgFailed (("Invalid clipboard mode '%s'\n", mode));
    }

    /* Guest node (required) */
    {
        Key guestNode = aNode.key ("Guest");

        /* optional, defaults to 0 */
        mHWData->mMemoryBalloonSize =
            guestNode.value <ULONG> ("memoryBalloonSize");
        /* optional, defaults to 0 */
        mHWData->mStatisticsUpdateInterval =
            guestNode.value <ULONG> ("statisticsUpdateInterval");
    }

    AssertComRC (rc);
    return rc;
}

/**
 *  @param aNode        <HardDiskAttachments> node.
 *  @param aRegistered  true when the machine is being loaded on VirtualBox
 *                      startup, or when a snapshot is being loaded (wchich
 *                      currently can happen on startup only)
 *  @param aSnapshotId  pointer to the snapshot ID if this is a snapshot machine
 */
HRESULT Machine::loadHardDisks (const settings::Key &aNode, bool aRegistered,
                                const Guid *aSnapshotId /* = NULL */)
{
    using namespace settings;

    AssertReturn (!aNode.isNull(), E_INVALIDARG);
    AssertReturn ((mType == IsMachine && aSnapshotId == NULL) ||
                  (mType == IsSnapshotMachine && aSnapshotId != NULL), E_FAIL);

    HRESULT rc = S_OK;

    Key::List children = aNode.keys ("HardDiskAttachment");

    if (!aRegistered && children.size() > 0)
    {
        /* when the machine is being loaded (opened) from a file, it cannot
         * have hard disks attached (this should not happen normally,
         * because we don't allow to attach hard disks to an unregistered
         * VM at all */
        return setError (E_FAIL,
            tr ("Unregistered machine '%ls' cannot have hard disks attached "
                "(found %d hard disk attachments)"),
            mUserData->mName.raw(), children.size());
    }


    for (Key::List::const_iterator it = children.begin();
         it != children.end(); ++ it)
    {
        /* hardDisk uuid (required) */
        Guid uuid = (*it).value <Guid> ("hardDisk");
        /* bus (controller) type (required) */
        const char *busStr = (*it).stringValue ("bus");
        /* channel (required) */
        LONG channel = (*it).value <LONG> ("channel");
        /* device (required) */
        LONG device = (*it).value <LONG> ("device");

        /* find a hard disk by UUID */
        ComObjPtr <HardDisk> hd;
        rc = mParent->getHardDisk (uuid, hd);
        CheckComRCReturnRC (rc);

        AutoLock hdLock (hd);

        if (!hd->machineId().isEmpty())
        {
            return setError (E_FAIL,
                tr ("Hard disk '%ls' with UUID {%s} is already "
                    "attached to a machine with UUID {%s} (see '%ls')"),
                hd->toString().raw(), uuid.toString().raw(),
                hd->machineId().toString().raw(),
                mData->mConfigFileFull.raw());
        }

        if (hd->type() == HardDiskType_Immutable)
        {
            return setError (E_FAIL,
                tr ("Immutable hard disk '%ls' with UUID {%s} cannot be "
                    "directly attached to a machine (see '%ls')"),
                hd->toString().raw(), uuid.toString().raw(),
                mData->mConfigFileFull.raw());
        }

        /* attach the device */
        StorageBus_T bus = StorageBus_Null;

        if (strcmp (busStr, "IDE") == 0)
        {
            bus = StorageBus_IDE;
        }
        else if (strcmp (busStr, "SATA") == 0)
        {
            bus = StorageBus_SATA;
        }
        else
            ComAssertMsgFailedRet (("Invalid bus '%s'\n", bus),
                                   E_FAIL);

        ComObjPtr <HardDiskAttachment> attachment;
        attachment.createObject();
        rc = attachment->init (hd, bus, channel, device, false /* aDirty */);
        CheckComRCBreakRC (rc);

        /* associate the hard disk with this machine */
        hd->setMachineId (mData->mUuid);

        /* associate the hard disk with the given snapshot ID */
        if (mType == IsSnapshotMachine)
            hd->setSnapshotId (*aSnapshotId);

        mHDData->mHDAttachments.push_back (attachment);
    }

    return rc;
}

/**
 *  Searches for a <Snapshot> node for the given snapshot.
 *  If the search is successful, \a aSnapshotNode will contain the found node.
 *  In this case, \a aSnapshotsNode can be NULL meaning the found node is a
 *  direct child of \a aMachineNode.
 *
 *  If the search fails, a failure is returned and both \a aSnapshotsNode and
 *  \a aSnapshotNode are set to 0.
 *
 *  @param aSnapshot        Snapshot to search for.
 *  @param aMachineNode     <Machine> node to start from.
 *  @param aSnapshotsNode   <Snapshots> node containing the found <Snapshot> node
 *                          (may be NULL if the caller is not interested).
 *  @param aSnapshotNode    Found <Snapshot> node.
 */
HRESULT Machine::findSnapshotNode (Snapshot *aSnapshot, settings::Key &aMachineNode,
                                   settings::Key *aSnapshotsNode,
                                   settings::Key *aSnapshotNode)
{
    using namespace settings;

    AssertReturn (aSnapshot && !aMachineNode.isNull()
                  && aSnapshotNode != NULL, E_FAIL);

    if (aSnapshotsNode)
        aSnapshotsNode->setNull();
    aSnapshotNode->setNull();

    // build the full uuid path (from the top parent to the given snapshot)
    std::list <Guid> path;
    {
        ComObjPtr <Snapshot> parent = aSnapshot;
        while (parent)
        {
            path.push_front (parent->data().mId);
            parent = parent->parent();
        }
    }

    Key snapshotsNode = aMachineNode;
    Key snapshotNode;

    for (std::list <Guid>::const_iterator it = path.begin();
         it != path.end();
         ++ it)
    {
        if (!snapshotNode.isNull())
        {
            /* proceed to the nested <Snapshots> node */
            snapshotsNode = snapshotNode.key ("Snapshots");
            snapshotNode.setNull();
        }

        AssertReturn (!snapshotsNode.isNull(), E_FAIL);

        Key::List children = snapshotsNode.keys ("Snapshot");
        for (Key::List::const_iterator ch = children.begin();
             ch != children.end();
             ++ ch)
        {
            Guid id = (*ch).value <Guid> ("uuid");
            if (id == (*it))
            {
                /* pass over to the outer loop */
                snapshotNode = *ch;
                break;
            }
        }

        if (!snapshotNode.isNull())
            continue;

        /* the next uuid is not found, no need to continue... */
        AssertFailedBreakVoid();
    }

    // we must always succesfully find the node
    AssertReturn (!snapshotNode.isNull(), E_FAIL);
    AssertReturn (!snapshotsNode.isNull(), E_FAIL);

    if (aSnapshotsNode && (snapshotsNode != aMachineNode))
        *aSnapshotsNode = snapshotsNode;
    *aSnapshotNode = snapshotNode;

    return S_OK;
}

/**
 *  Returns the snapshot with the given UUID or fails of no such snapshot.
 *
 *  @param aId          snapshot UUID to find (empty UUID refers the first snapshot)
 *  @param aSnapshot    where to return the found snapshot
 *  @param aSetError    true to set extended error info on failure
 */
HRESULT Machine::findSnapshot (const Guid &aId, ComObjPtr <Snapshot> &aSnapshot,
                               bool aSetError /* = false */)
{
    if (!mData->mFirstSnapshot)
    {
        if (aSetError)
            return setError (E_FAIL,
                tr ("This machine does not have any snapshots"));
        return E_FAIL;
    }

    if (aId.isEmpty())
        aSnapshot = mData->mFirstSnapshot;
    else
        aSnapshot = mData->mFirstSnapshot->findChildOrSelf (aId);

    if (!aSnapshot)
    {
        if (aSetError)
            return setError (E_FAIL,
                tr ("Could not find a snapshot with UUID {%s}"),
                aId.toString().raw());
        return E_FAIL;
    }

    return S_OK;
}

/**
 *  Returns the snapshot with the given name or fails of no such snapshot.
 *
 *  @param aName        snapshot name to find
 *  @param aSnapshot    where to return the found snapshot
 *  @param aSetError    true to set extended error info on failure
 */
HRESULT Machine::findSnapshot (const BSTR aName, ComObjPtr <Snapshot> &aSnapshot,
                               bool aSetError /* = false */)
{
    AssertReturn (aName, E_INVALIDARG);

    if (!mData->mFirstSnapshot)
    {
        if (aSetError)
            return setError (E_FAIL,
                tr ("This machine does not have any snapshots"));
        return E_FAIL;
    }

    aSnapshot = mData->mFirstSnapshot->findChildOrSelf (aName);

    if (!aSnapshot)
    {
        if (aSetError)
            return setError (E_FAIL,
                tr ("Could not find a snapshot named '%ls'"), aName);
        return E_FAIL;
    }

    return S_OK;
}

/**
 *  Searches for an attachment that contains the given hard disk.
 *  The hard disk must be associated with some VM and can be optionally
 *  associated with some snapshot. If the attachment is stored in the snapshot
 *  (i.e. the hard disk is associated with some snapshot), @a aSnapshot
 *  will point to a non-null object on output.
 *
 *  @param aHd          hard disk to search an attachment for
 *  @param aMachine     where to store the hard disk's machine (can be NULL)
 *  @param aSnapshot    where to store the hard disk's snapshot (can be NULL)
 *  @param aHda         where to store the hard disk's attachment (can be NULL)
 *
 *
 *  @note
 *      It is assumed that the machine where the attachment is found,
 *      is already placed to the Discarding state, when this method is called.
 *  @note
 *      The object returned in @a aHda is the attachment from the snapshot
 *      machine if the hard disk is associated with the snapshot, not from the
 *      primary machine object returned returned in @a aMachine.
 */
HRESULT Machine::findHardDiskAttachment (const ComObjPtr <HardDisk> &aHd,
                                         ComObjPtr <Machine> *aMachine,
                                         ComObjPtr <Snapshot> *aSnapshot,
                                         ComObjPtr <HardDiskAttachment> *aHda)
{
    AssertReturn (!aHd.isNull(), E_INVALIDARG);

    Guid mid = aHd->machineId();
    Guid sid = aHd->snapshotId();

    AssertReturn (!mid.isEmpty(), E_INVALIDARG);

    ComObjPtr <Machine> m;
    mParent->getMachine (mid, m);
    ComAssertRet (!m.isNull(), E_FAIL);

    HDData::HDAttachmentList *attachments = &m->mHDData->mHDAttachments;

    ComObjPtr <Snapshot> s;
    if (!sid.isEmpty())
    {
        m->findSnapshot (sid, s);
        ComAssertRet (!s.isNull(), E_FAIL);
        attachments = &s->data().mMachine->mHDData->mHDAttachments;
    }

    AssertReturn (attachments, E_FAIL);

    for (HDData::HDAttachmentList::const_iterator it = attachments->begin();
         it != attachments->end();
         ++ it)
    {
        if ((*it)->hardDisk() == aHd)
        {
            if (aMachine) *aMachine = m;
            if (aSnapshot) *aSnapshot = s;
            if (aHda) *aHda = (*it);
            return S_OK;
        }
    }

    ComAssertFailed();
    return E_FAIL;
}

/**
 *  Helper for #saveSettings. Cares about renaming the settings directory and
 *  file if the machine name was changed and about creating a new settings file
 *  if this is a new machine.
 *
 *  @note Must be never called directly but only from #saveSettings().
 *
 *  @param aRenamed receives |true| if the name was changed and the settings
 *                  file was renamed as a result, or |false| otherwise. The
 *                  value makes sense only on success.
 *  @param aNew     receives |true| if a virgin settings file was created.
 */
HRESULT Machine::prepareSaveSettings (bool &aRenamed, bool &aNew)
{
    /* Note: tecnhically, mParent needs to be locked only when the machine is
     * registered (see prepareSaveSettings() for details) but we don't
     * currently differentiate it in callers of saveSettings() so we don't
     * make difference here too.  */
    AssertReturn (mParent->isLockedOnCurrentThread(), E_FAIL);
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

    HRESULT rc = S_OK;

    aRenamed = false;

    /* if we're ready and isConfigLocked() is FALSE then it means
     * that no config file exists yet (we will create a virgin one) */
    aNew = !isConfigLocked();

    /* attempt to rename the settings file if machine name is changed */
    if (mUserData->mNameSync &&
        mUserData.isBackedUp() &&
        mUserData.backedUpData()->mName != mUserData->mName)
    {
        aRenamed = true;

        if (!aNew)
        {
            /* unlock the old config file */
            rc = unlockConfig();
            CheckComRCReturnRC (rc);
        }

        bool dirRenamed = false;
        bool fileRenamed = false;

        Utf8Str configFile, newConfigFile;
        Utf8Str configDir, newConfigDir;

        do
        {
            int vrc = VINF_SUCCESS;

            Utf8Str name = mUserData.backedUpData()->mName;
            Utf8Str newName = mUserData->mName;

            configFile = mData->mConfigFileFull;

            /* first, rename the directory if it matches the machine name */
            configDir = configFile;
            RTPathStripFilename (configDir.mutableRaw());
            newConfigDir = configDir;
            if (RTPathFilename (configDir) == name)
            {
                RTPathStripFilename (newConfigDir.mutableRaw());
                newConfigDir = Utf8StrFmt ("%s%c%s",
                    newConfigDir.raw(), RTPATH_DELIMITER, newName.raw());
                /* new dir and old dir cannot be equal here because of 'if'
                 * above and because name != newName */
                Assert (configDir != newConfigDir);
                if (!aNew)
                {
                    /* perform real rename only if the machine is not new */
                    vrc = RTPathRename (configDir.raw(), newConfigDir.raw(), 0);
                    if (VBOX_FAILURE (vrc))
                    {
                        rc = setError (E_FAIL,
                            tr ("Could not rename the directory '%s' to '%s' "
                                "to save the settings file (%Vrc)"),
                            configDir.raw(), newConfigDir.raw(), vrc);
                        break;
                    }
                    dirRenamed = true;
                }
            }

            newConfigFile = Utf8StrFmt ("%s%c%s.xml",
                newConfigDir.raw(), RTPATH_DELIMITER, newName.raw());

            /* then try to rename the settings file itself */
            if (newConfigFile != configFile)
            {
                /* get the path to old settings file in renamed directory */
                configFile = Utf8StrFmt ("%s%c%s",
                        newConfigDir.raw(), RTPATH_DELIMITER,
                        RTPathFilename (configFile));
                if (!aNew)
                {
                    /* perform real rename only if the machine is not new */
                    vrc = RTFileRename (configFile.raw(), newConfigFile.raw(), 0);
                    if (VBOX_FAILURE (vrc))
                    {
                        rc = setError (E_FAIL,
                            tr ("Could not rename the settings file '%s' to '%s' "
                                "(%Vrc)"),
                            configFile.raw(), newConfigFile.raw(), vrc);
                        break;
                    }
                    fileRenamed = true;
                }
            }

            /* update mConfigFileFull amd mConfigFile */
            Bstr oldConfigFileFull = mData->mConfigFileFull;
            Bstr oldConfigFile = mData->mConfigFile;
            mData->mConfigFileFull = newConfigFile;
            /* try to get the relative path for mConfigFile */
            Utf8Str path = newConfigFile;
            mParent->calculateRelativePath (path, path);
            mData->mConfigFile = path;

            /* last, try to update the global settings with the new path */
            if (mData->mRegistered)
            {
                rc = mParent->updateSettings (configDir, newConfigDir);
                if (FAILED (rc))
                {
                    /* revert to old values */
                    mData->mConfigFileFull = oldConfigFileFull;
                    mData->mConfigFile = oldConfigFile;
                    break;
                }
            }

            /* update the snapshot folder */
            path = mUserData->mSnapshotFolderFull;
            if (RTPathStartsWith (path, configDir))
            {
                path = Utf8StrFmt ("%s%s", newConfigDir.raw(),
                                   path.raw() + configDir.length());
                mUserData->mSnapshotFolderFull = path;
                calculateRelativePath (path, path);
                mUserData->mSnapshotFolder = path;
            }

            /* update the saved state file path */
            path = mSSData->mStateFilePath;
            if (RTPathStartsWith (path, configDir))
            {
                path = Utf8StrFmt ("%s%s", newConfigDir.raw(),
                                   path.raw() + configDir.length());
                mSSData->mStateFilePath = path;
            }

            /* Update saved state file paths of all online snapshots.
             * Note that saveSettings() will recognize name change
             * and will save all snapshots in this case. */
            if (mData->mFirstSnapshot)
                mData->mFirstSnapshot->updateSavedStatePaths (configDir,
                                                              newConfigDir);
        }
        while (0);

        if (FAILED (rc))
        {
            /* silently try to rename everything back */
            if (fileRenamed)
                RTFileRename (newConfigFile.raw(), configFile.raw(), 0);
            if (dirRenamed)
                RTPathRename (newConfigDir.raw(), configDir.raw(), 0);
        }

        if (!aNew)
        {
            /* lock the config again */
            HRESULT rc2 = lockConfig();
            if (SUCCEEDED (rc))
                rc = rc2;
        }

        CheckComRCReturnRC (rc);
    }

    if (aNew)
    {
        /* create a virgin config file */
        int vrc = VINF_SUCCESS;

        /* ensure the settings directory exists */
        Utf8Str path = mData->mConfigFileFull;
        RTPathStripFilename (path.mutableRaw());
        if (!RTDirExists (path))
        {
            vrc = RTDirCreateFullPath (path, 0777);
            if (VBOX_FAILURE (vrc))
            {
                return setError (E_FAIL,
                    tr ("Could not create a directory '%s' "
                        "to save the settings file (%Vrc)"),
                    path.raw(), vrc);
            }
        }

        /* Note: open flags must correlate with RTFileOpen() in lockConfig() */
        path = Utf8Str (mData->mConfigFileFull);
        vrc = RTFileOpen (&mData->mHandleCfgFile, path,
                          RTFILE_O_READWRITE | RTFILE_O_CREATE |
                          RTFILE_O_DENY_WRITE);
        if (VBOX_SUCCESS (vrc))
        {
            vrc = RTFileWrite (mData->mHandleCfgFile,
                               (void *) DefaultMachineConfig,
                               sizeof (DefaultMachineConfig), NULL);
        }
        if (VBOX_FAILURE (vrc))
        {
            mData->mHandleCfgFile = NIL_RTFILE;
            return setError (E_FAIL,
                tr ("Could not create the settings file '%s' (%Vrc)"),
                path.raw(), vrc);
        }
        /* we do not close the file to simulate lockConfig() */
    }

    return rc;
}

/**
 *  Saves machine data, user data and hardware data.
 *
 *  @param aMarkCurStateAsModified
 *      If true (default), mData->mCurrentStateModified will be set to
 *      what #isReallyModified() returns prior to saving settings to a file,
 *      otherwise the current value of mData->mCurrentStateModified will be
 *      saved.
 *  @param aInformCallbacksAnyway
 *      If true, callbacks will be informed even if #isReallyModified()
 *      returns false. This is necessary for cases when we change machine data
 *      diectly, not through the backup()/commit() mechanism.
 *
 *  @note Must be called from under mParent write lock (sometimes needed by
 *  #prepareSaveSettings()) and this object's write lock. Locks children for
 *  writing. There is one exception when mParent is unused and therefore may
 *  be left unlocked: if this machine is an unregistered one.
 */
HRESULT Machine::saveSettings (bool aMarkCurStateAsModified /* = true */,
                               bool aInformCallbacksAnyway /* = false */)
{
    LogFlowThisFuncEnter();

    /* Note: tecnhically, mParent needs to be locked only when the machine is
     * registered (see prepareSaveSettings() for details) but we don't
     * currently differentiate it in callers of saveSettings() so we don't
     * make difference here too.  */
    AssertReturn (mParent->isLockedOnCurrentThread(), E_FAIL);
    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

    /// @todo (dmik) I guess we should lock all our child objects here
    //  (such as mVRDPServer etc.) to ensure they are not changed
    //  until completely saved to disk and committed

    /// @todo (dmik) also, we need to delegate saving child objects' settings
    //  to objects themselves to ensure operations 'commit + save changes'
    //  are atomic (amd done from the object's lock so that nobody can change
    //  settings again until completely saved).

    AssertReturn (mType == IsMachine || mType == IsSessionMachine, E_FAIL);

    bool wasModified;

    if (aMarkCurStateAsModified)
    {
        /*
         *  We ignore changes to user data when setting mCurrentStateModified
         *  because the current state will not differ from the current snapshot
         *  if only user data has been changed (user data is shared by all
         *  snapshots).
         */
        mData->mCurrentStateModified = isReallyModified (true /* aIgnoreUserData */);
        wasModified = mUserData.hasActualChanges() || mData->mCurrentStateModified;
    }
    else
    {
        wasModified = isReallyModified();
    }

    HRESULT rc = S_OK;

    /* First, prepare to save settings. It will will care about renaming the
     * settings directory and file if the machine name was changed and about
     * creating a new settings file if this is a new machine. */
    bool isRenamed = false;
    bool isNew = false;
    rc = prepareSaveSettings (isRenamed, isNew);
    CheckComRCReturnRC (rc);

    try
    {
        using namespace settings;

#if 0
        /// @todo disabled until made thread-safe by using handle duplicates
        File file (File::ReadWrite, mData->mHandleCfgFile,
                   Utf8Str (mData->mConfigFileFull));
#else
        File file (File::ReadWrite, Utf8Str (mData->mConfigFileFull));
#endif
        XmlTreeBackend tree;

        /* The newly created settings file is incomplete therefore we turn off
         * validation. The rest is like in loadSettingsTree_ForUpdate().*/
        rc = VirtualBox::loadSettingsTree (tree, file,
                                           !isNew /* aValidate */,
                                           false /* aCatchLoadErrors */,
                                           false /* aAddDefaults */);
        CheckComRCThrowRC (rc);


        /* ask to save all snapshots when the machine name was changed since
         * it may affect saved state file paths for online snapshots (see
         * #openConfigLoader() for details) */
        bool updateAllSnapshots = isRenamed;

        /* commit before saving, since it may change settings
         * (for example, perform fixup of lazy hard disk changes) */
        rc = commit();
        CheckComRCReturnRC (rc);

        /* include hard disk changes to the modified flag */
        wasModified |= mHDData->mHDAttachmentsChanged;
        if (aMarkCurStateAsModified)
            mData->mCurrentStateModified |= BOOL (mHDData->mHDAttachmentsChanged);

        Key machineNode = tree.rootKey().createKey ("Machine");

        /* uuid (required) */
        Assert (!mData->mUuid.isEmpty());
        machineNode.setValue <Guid> ("uuid", mData->mUuid);

        /* name (required) */
        Assert (!mUserData->mName.isEmpty());
        machineNode.setValue <Bstr> ("name", mUserData->mName);

        /* nameSync (optional, default is true) */
        machineNode.setValueOr <bool> ("nameSync", !!mUserData->mNameSync, true);

        /* Description node (optional) */
        if (!mUserData->mDescription.isNull())
        {
            Key descNode = machineNode.createKey ("Description");
            descNode.setKeyValue <Bstr> (mUserData->mDescription);
        }
        else
        {
            Key descNode = machineNode.findKey ("Description");
            if (!descNode.isNull())
                descNode.zap();
        }

        /* OSType (required) */
        machineNode.setValue <Bstr> ("OSType", mUserData->mOSTypeId);

        /* stateFile (optional) */
        if (mData->mMachineState == MachineState_Saved)
        {
            Assert (!mSSData->mStateFilePath.isEmpty());
            /* try to make the file name relative to the settings file dir */
            Utf8Str stateFilePath = mSSData->mStateFilePath;
            calculateRelativePath (stateFilePath, stateFilePath);
            machineNode.setStringValue ("stateFile", stateFilePath);
        }
        else
        {
            Assert (mSSData->mStateFilePath.isNull());
            machineNode.zapValue ("stateFile");
        }

        /* currentSnapshot ID (optional) */
        if (!mData->mCurrentSnapshot.isNull())
        {
            Assert (!mData->mFirstSnapshot.isNull());
            machineNode.setValue <Guid> ("currentSnapshot",
                                         mData->mCurrentSnapshot->data().mId);
        }
        else
        {
            Assert (mData->mFirstSnapshot.isNull());
            machineNode.zapValue ("currentSnapshot");
        }

        /* snapshotFolder (optional) */
        /// @todo use the Bstr::NullOrEmpty constant and setValueOr
        if (!mUserData->mSnapshotFolder.isEmpty())
            machineNode.setValue <Bstr> ("snapshotFolder", mUserData->mSnapshotFolder);
        else
            machineNode.zapValue ("snapshotFolder");

        /* currentStateModified (optional, default is true) */
        machineNode.setValueOr <bool> ("currentStateModified",
                                       !!mData->mCurrentStateModified, true);

        /* lastStateChange */
        machineNode.setValue <RTTIMESPEC> ("lastStateChange",
                                           mData->mLastStateChange);

        /* set the aborted attribute when appropriate, defaults to false */
        machineNode.setValueOr <bool> ("aborted",
                                       mData->mMachineState == MachineState_Aborted,
                                       false);

        /* Hardware node (required) */
        {
            /* first, delete the entire node if exists */
            Key hwNode = machineNode.findKey ("Hardware");
            if (!hwNode.isNull())
                hwNode.zap();
            /* then recreate it */
            hwNode = machineNode.createKey ("Hardware");

            rc = saveHardware (hwNode);
            CheckComRCThrowRC (rc);
        }

        /* HardDiskAttachments node (required) */
        {
            /* first, delete the entire node if exists */
            Key hdaNode = machineNode.findKey ("HardDiskAttachments");
            if (!hdaNode.isNull())
                hdaNode.zap();
            /* then recreate it */
            hdaNode = machineNode.createKey ("HardDiskAttachments");

            rc = saveHardDisks (hdaNode);
            CheckComRCThrowRC (rc);
        }

        /* update all snapshots if requested */
        if (updateAllSnapshots)
        {
            rc = saveSnapshotSettingsWorker (machineNode, NULL,
                                             SaveSS_UpdateAllOp);
            CheckComRCThrowRC (rc);
        }

        /* save the settings on success */
        rc = VirtualBox::saveSettingsTree (tree, file,
                                          mData->mSettingsFileVersion);
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

    if (FAILED (rc))
    {
        /* backup arbitrary data item to cause #isModified() to still return
         * true in case of any error */
        mHWData.backup();
    }

    if (wasModified || aInformCallbacksAnyway)
    {
        /* Fire the data change event, even on failure (since we've already
         * committed all data). This is done only for SessionMachines because
         * mutable Machine instances are always not registered (i.e. private
         * to the client process that creates them) and thus don't need to
         * inform callbacks. */
        if (mType == IsSessionMachine)
            mParent->onMachineDataChange (mData->mUuid);
    }

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  Wrapper for #saveSnapshotSettingsWorker() that opens the settings file
 *  and locates the <Machine> node in there. See #saveSnapshotSettingsWorker()
 *  for more details.
 *
 *  @param aSnapshot    Snapshot to operate on
 *  @param aOpFlags     Operation to perform, one of SaveSS_NoOp, SaveSS_AddOp
 *                      or SaveSS_UpdateAttrsOp possibly combined with
 *                      SaveSS_UpdateCurrentId.
 *
 *  @note Locks this object for writing + other child objects.
 */
HRESULT Machine::saveSnapshotSettings (Snapshot *aSnapshot, int aOpFlags)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AssertReturn (mType == IsMachine || mType == IsSessionMachine, E_FAIL);

    AutoLock alock (this);

    AssertReturn (isConfigLocked(), E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        using namespace settings;

        /* load the config file */
#if 0
        /// @todo disabled until made thread-safe by using handle duplicates
        File file (File::ReadWrite, mData->mHandleCfgFile,
                   Utf8Str (mData->mConfigFileFull));
#else
        File file (File::ReadWrite, Utf8Str (mData->mConfigFileFull));
#endif
        XmlTreeBackend tree;

        rc = VirtualBox::loadSettingsTree_ForUpdate (tree, file);
        CheckComRCReturnRC (rc);

        Key machineNode = tree.rootKey().key ("Machine");

        rc = saveSnapshotSettingsWorker (machineNode, aSnapshot, aOpFlags);
        CheckComRCReturnRC (rc);

        /* save settings on success */
        rc = VirtualBox::saveSettingsTree (tree, file,
                                           mData->mSettingsFileVersion);
        CheckComRCReturnRC (rc);
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions (RT_SRC_POS);
    }

    return rc;
}

/**
 *  Performs the specified operation on the given snapshot
 *  in the settings file represented by \a aMachineNode.
 *
 *  If \a aOpFlags = SaveSS_UpdateAllOp, \a aSnapshot can be NULL to indicate
 *  that the whole tree of the snapshots should be updated in <Machine>.
 *  One particular case is when the last (and the only) snapshot should be
 *  removed (it is so when both mCurrentSnapshot and mFirstSnapshot are NULL).
 *
 *  \a aOp may be just SaveSS_UpdateCurrentId if only the currentSnapshot
 *  attribute of <Machine> needs to be updated.
 *
 *  @param aMachineNode <Machine> node in the opened settings file.
 *  @param aSnapshot    Snapshot to operate on.
 *  @param aOpFlags     Operation to perform, one of SaveSS_NoOp, SaveSS_AddOp
 *                      or SaveSS_UpdateAttrsOp possibly combined with
 *                      SaveSS_UpdateCurrentId.
 *
 *  @note Must be called with this object locked for writing.
 *        Locks child objects.
 */
HRESULT Machine::saveSnapshotSettingsWorker (settings::Key &aMachineNode,
                                             Snapshot *aSnapshot, int aOpFlags)
{
    using namespace settings;

    AssertReturn (!aMachineNode.isNull(), E_FAIL);

    AssertReturn (isLockedOnCurrentThread(), E_FAIL);

    int op = aOpFlags & SaveSS_OpMask;
    AssertReturn (
        (aSnapshot && (op == SaveSS_AddOp || op == SaveSS_UpdateAttrsOp ||
                       op == SaveSS_UpdateAllOp)) ||
        (!aSnapshot && ((op == SaveSS_NoOp && (aOpFlags & SaveSS_UpdateCurrentId)) ||
                        op == SaveSS_UpdateAllOp)),
        E_FAIL);

    HRESULT rc = S_OK;

    bool recreateWholeTree = false;

    do
    {
        if (op == SaveSS_NoOp)
            break;

        /* quick path: recreate the whole tree of the snapshots */
        if (op == SaveSS_UpdateAllOp && !aSnapshot)
        {
            /* first, delete the entire root snapshot node if it exists */
            Key snapshotNode = aMachineNode.findKey ("Snapshot");
            if (!snapshotNode.isNull())
                snapshotNode.zap();

            /* second, if we have any snapshots left, substitute aSnapshot
             * with the first snapshot to recreate the whole tree, otherwise
             * break */
            if (mData->mFirstSnapshot)
            {
                aSnapshot = mData->mFirstSnapshot;
                recreateWholeTree = true;
            }
            else
                break;
        }

        Assert (!!aSnapshot);
        ComObjPtr <Snapshot> parent = aSnapshot->parent();

        if (op == SaveSS_AddOp)
        {
            Key parentNode;

            if (parent)
            {
                rc = findSnapshotNode (parent, aMachineNode, NULL, &parentNode);
                CheckComRCBreakRC (rc);

                ComAssertBreak (!parentNode.isNull(), rc = E_FAIL);
            }

            do
            {
                Key snapshotsNode;

                if (!parentNode.isNull())
                    snapshotsNode = parentNode.createKey ("Snapshots");
                else
                    snapshotsNode = aMachineNode;
                do
                {
                    Key snapshotNode = snapshotsNode.appendKey ("Snapshot");
                    rc = saveSnapshot (snapshotNode, aSnapshot, false /* aAttrsOnly */);
                    CheckComRCBreakRC (rc);

                    /* when a new snapshot is added, this means diffs were created
                     * for every normal/immutable hard disk of the VM, so we need to
                     * save the current hard disk attachments */

                    Key hdaNode = aMachineNode.findKey ("HardDiskAttachments");
                    if (!hdaNode.isNull())
                        hdaNode.zap();
                    hdaNode = aMachineNode.createKey ("HardDiskAttachments");

                    rc = saveHardDisks (hdaNode);
                    CheckComRCBreakRC (rc);

                    if (mHDData->mHDAttachments.size() != 0)
                    {
                        /* If we have one or more attachments then we definitely
                         * created diffs for them and associated new diffs with
                         * current settngs. So, since we don't use saveSettings(),
                         * we need to inform callbacks manually. */
                        if (mType == IsSessionMachine)
                            mParent->onMachineDataChange (mData->mUuid);
                    }
                }
                while (0);
            }
            while (0);

            break;
        }

        Assert ((op == SaveSS_UpdateAttrsOp && !recreateWholeTree) ||
                op == SaveSS_UpdateAllOp);

        Key snapshotsNode;
        Key snapshotNode;

        if (!recreateWholeTree)
        {
            rc = findSnapshotNode (aSnapshot, aMachineNode,
                                   &snapshotsNode, &snapshotNode);
            CheckComRCBreakRC (rc);
        }

        if (snapshotsNode.isNull())
            snapshotsNode = aMachineNode;

        if (op == SaveSS_UpdateAttrsOp)
            rc = saveSnapshot (snapshotNode, aSnapshot, true /* aAttrsOnly */);
        else
        {
            if (!snapshotNode.isNull())
                snapshotNode.zap();

            snapshotNode = snapshotsNode.appendKey ("Snapshot");
            rc = saveSnapshot (snapshotNode, aSnapshot, false /* aAttrsOnly */);
            CheckComRCBreakRC (rc);
        }
    }
    while (0);

    if (SUCCEEDED (rc))
    {
        /* update currentSnapshot when appropriate */
        if (aOpFlags & SaveSS_UpdateCurrentId)
        {
            if (!mData->mCurrentSnapshot.isNull())
                aMachineNode.setValue <Guid> ("currentSnapshot",
                                              mData->mCurrentSnapshot->data().mId);
            else
                aMachineNode.zapValue ("currentSnapshot");
        }
        if (aOpFlags & SaveSS_UpdateCurStateModified)
        {
            aMachineNode.setValue <bool> ("currentStateModified", true);
        }
    }

    return rc;
}

/**
 *  Saves the given snapshot and all its children (unless \a aAttrsOnly is true).
 *  It is assumed that the given node is empty (unless \a aAttrsOnly is true).
 *
 *  @param aNode        <Snapshot> node to save the snapshot to.
 *  @param aSnapshot    Snapshot to save.
 *  @param aAttrsOnly   If true, only updatge user-changeable attrs.
 */
HRESULT Machine::saveSnapshot (settings::Key &aNode, Snapshot *aSnapshot, bool aAttrsOnly)
{
    using namespace settings;

    AssertReturn (!aNode.isNull() && aSnapshot, E_INVALIDARG);
    AssertReturn (mType == IsMachine || mType == IsSessionMachine, E_FAIL);

    /* uuid (required) */
    if (!aAttrsOnly)
        aNode.setValue <Guid> ("uuid", aSnapshot->data().mId);

    /* name (required) */
    aNode.setValue <Bstr> ("name", aSnapshot->data().mName);

    /* timeStamp (required) */
    aNode.setValue <RTTIMESPEC> ("timeStamp", aSnapshot->data().mTimeStamp);

    /* Description node (optional) */
    if (!aSnapshot->data().mDescription.isNull())
    {
        Key descNode = aNode.createKey ("Description");
        descNode.setKeyValue <Bstr> (aSnapshot->data().mDescription);
    }
    else
    {
        Key descNode = aNode.findKey ("Description");
        if (!descNode.isNull())
            descNode.zap();
    }

    if (aAttrsOnly)
        return S_OK;

    /* stateFile (optional) */
    if (aSnapshot->stateFilePath())
    {
        /* try to make the file name relative to the settings file dir */
        Utf8Str stateFilePath = aSnapshot->stateFilePath();
        calculateRelativePath (stateFilePath, stateFilePath);
        aNode.setStringValue ("stateFile", stateFilePath);
    }

    {
        ComObjPtr <SnapshotMachine> snapshotMachine = aSnapshot->data().mMachine;
        ComAssertRet (!snapshotMachine.isNull(), E_FAIL);

        /* save hardware */
        {
            Key hwNode = aNode.createKey ("Hardware");
            HRESULT rc = snapshotMachine->saveHardware (hwNode);
            CheckComRCReturnRC (rc);
        }

        /* save hard disks */
        {
            Key hdasNode = aNode.createKey ("HardDiskAttachments");
            HRESULT rc = snapshotMachine->saveHardDisks (hdasNode);
            CheckComRCReturnRC (rc);
        }
    }

    /* save children */
    {
        AutoLock listLock (aSnapshot->childrenLock());

        if (aSnapshot->children().size())
        {
            Key snapshotsNode = aNode.createKey ("Snapshots");

            HRESULT rc = S_OK;

            for (Snapshot::SnapshotList::const_iterator it = aSnapshot->children().begin();
                 it != aSnapshot->children().end();
                 ++ it)
            {
                Key snapshotNode = snapshotsNode.createKey ("Snapshot");
                rc = saveSnapshot (snapshotNode, (*it), aAttrsOnly);
                CheckComRCReturnRC (rc);
            }
        }
    }

    return S_OK;
}

/**
 *  Saves the VM hardware configuration. It is assumed that the
 *  given node is empty.
 *
 *  @param aNode    <Hardware> node to save the VM hardware confguration to.
 */
HRESULT Machine::saveHardware (settings::Key &aNode)
{
    using namespace settings;

    AssertReturn (!aNode.isNull(), E_INVALIDARG);

    HRESULT rc = S_OK;

    /* CPU (optional) */
    {
        Key cpuNode = aNode.createKey ("CPU");
        Key hwVirtExNode = cpuNode.createKey ("HardwareVirtEx");
        const char *value = NULL;
        switch (mHWData->mHWVirtExEnabled)
        {
            case TSBool_False:
                value = "false";
                break;
            case TSBool_True:
                value = "true";
                break;
            case TSBool_Default:
                value = "default";
                break;
        }
        hwVirtExNode.setStringValue ("enabled", value);

        /* PAE (optional, default is false) */
        Key PAENode = cpuNode.createKey ("PAE");
        PAENode.setValue <bool> ("enabled", mHWData->mPAEEnabled);
    }

    /* memory (required) */
    {
        Key memoryNode = aNode.createKey ("Memory");
        memoryNode.setValue <ULONG> ("RAMSize", mHWData->mMemorySize);
    }

    /* boot (required) */
    {
        Key bootNode = aNode.createKey ("Boot");

        for (ULONG pos = 0; pos < ELEMENTS (mHWData->mBootOrder); ++ pos)
        {
            const char *device = NULL;
            switch (mHWData->mBootOrder [pos])
            {
                case DeviceType_Null:
                    /* skip, this is allowed for <Order> nodes
                     * when loading, the default value NoDevice will remain */
                    continue;
                case DeviceType_Floppy:         device = "Floppy"; break;
                case DeviceType_DVD:            device = "DVD"; break;
                case DeviceType_HardDisk:       device = "HardDisk"; break;
                case DeviceType_Network:        device = "Network"; break;
                default:
                {
                    ComAssertMsgFailedRet (("Invalid boot device: %d\n",
                                            mHWData->mBootOrder [pos]),
                                            E_FAIL);
                }
            }

            Key orderNode = bootNode.appendKey ("Order");
            orderNode.setValue <ULONG> ("position", pos + 1);
            orderNode.setStringValue ("device", device);
        }
    }

    /* display (required) */
    {
        Key displayNode = aNode.createKey ("Display");
        displayNode.setValue <ULONG> ("VRAMSize", mHWData->mVRAMSize);
        displayNode.setValue <ULONG> ("MonitorCount", mHWData->mMonitorCount);
    }

#ifdef VBOX_VRDP
    /* VRDP settings (optional) */
    rc = mVRDPServer->saveSettings (aNode);
    CheckComRCReturnRC (rc);
#endif

    /* BIOS (required) */
    rc = mBIOSSettings->saveSettings (aNode);
    CheckComRCReturnRC (rc);

    /* DVD drive (required) */
    rc = mDVDDrive->saveSettings (aNode);
    CheckComRCReturnRC (rc);

    /* Flooppy drive (required) */
    rc = mFloppyDrive->saveSettings (aNode);
    CheckComRCReturnRC (rc);

    /* USB Controller (required) */
    rc = mUSBController->saveSettings (aNode);
    CheckComRCReturnRC (rc);

    /* SATA Controller (required) */
    rc = mSATAController->saveSettings (aNode);
    CheckComRCReturnRC (rc);

    /* Network adapters (required) */
    {
        Key nwNode = aNode.createKey ("Network");

        for (ULONG slot = 0; slot < ELEMENTS (mNetworkAdapters); ++ slot)
        {
            Key adapterNode = nwNode.appendKey ("Adapter");

            adapterNode.setValue <ULONG> ("slot", slot);

            rc = mNetworkAdapters [slot]->saveSettings (adapterNode);
            CheckComRCReturnRC (rc);
        }
    }

    /* Serial ports */
    {
        Key serialNode = aNode.createKey ("UART");

        for (ULONG slot = 0; slot < ELEMENTS (mSerialPorts); ++ slot)
        {
            Key portNode = serialNode.appendKey ("Port");

            portNode.setValue <ULONG> ("slot", slot);

            rc = mSerialPorts [slot]->saveSettings (portNode);
            CheckComRCReturnRC (rc);
        }
    }

    /* Parallel ports */
    {
        Key parallelNode = aNode.createKey ("LPT");

        for (ULONG slot = 0; slot < ELEMENTS (mParallelPorts); ++ slot)
        {
            Key portNode = parallelNode.appendKey ("Port");

            portNode.setValue <ULONG> ("slot", slot);

            rc = mParallelPorts [slot]->saveSettings (portNode);
            CheckComRCReturnRC (rc);
        }
    }

    /* Audio adapter */
    rc = mAudioAdapter->saveSettings (aNode);
    CheckComRCReturnRC (rc);

    /* Shared folders */
    {
        Key sharedFoldersNode = aNode.createKey ("SharedFolders");

        for (HWData::SharedFolderList::const_iterator it = mHWData->mSharedFolders.begin();
             it != mHWData->mSharedFolders.end();
             ++ it)
        {
            ComObjPtr <SharedFolder> folder = *it;

            Key folderNode = sharedFoldersNode.appendKey ("SharedFolder");

            /* all are mandatory */
            folderNode.setValue <Bstr> ("name", folder->name());
            folderNode.setValue <Bstr> ("hostPath", folder->hostPath());
            folderNode.setValue <bool> ("writable", !!folder->writable());
        }
    }

    /* Clipboard */
    {
        Key clipNode = aNode.createKey ("Clipboard");

        const char *modeStr = "Disabled";
        switch (mHWData->mClipboardMode)
        {
            case ClipboardMode_Disabled:
                /* already assigned */
                break;
            case ClipboardMode_HostToGuest:
                modeStr = "HostToGuest";
                break;
            case ClipboardMode_GuestToHost:
                modeStr = "GuestToHost";
                break;
            case ClipboardMode_Bidirectional:
                modeStr = "Bidirectional";
                break;
            default:
                ComAssertMsgFailedRet (("Clipboard mode %d is invalid",
                                        mHWData->mClipboardMode),
                                       E_FAIL);
        }
        clipNode.setStringValue ("mode", modeStr);
    }

    /* Guest */
    {
        Key guestNode = aNode.createKey ("Guest");

        guestNode.setValue <ULONG> ("memoryBalloonSize",
                                    mHWData->mMemoryBalloonSize);
        guestNode.setValue <ULONG> ("statisticsUpdateInterval",
                                    mHWData->mStatisticsUpdateInterval);
    }

    AssertComRC (rc);
    return rc;
}

/**
 *  Saves the hard disk confguration.
 *  It is assumed that the given node is empty.
 *
 *  @param aNode    <HardDiskAttachments> node to save the hard disk confguration to.
 */
HRESULT Machine::saveHardDisks (settings::Key &aNode)
{
    using namespace settings;

    AssertReturn (!aNode.isNull(), E_INVALIDARG);

    for (HDData::HDAttachmentList::const_iterator it = mHDData->mHDAttachments.begin();
         it != mHDData->mHDAttachments.end();
         ++ it)
    {
        ComObjPtr <HardDiskAttachment> att = *it;

        Key hdNode = aNode.appendKey ("HardDiskAttachment");

        {
            const char *bus = NULL;
            switch (att->bus())
            {
                case StorageBus_IDE:  bus = "IDE"; break;
                case StorageBus_SATA: bus = "SATA"; break;
                default:
                    ComAssertFailedRet (E_FAIL);
            }

            hdNode.setValue <Guid> ("hardDisk", att->hardDisk()->id());
            hdNode.setStringValue ("bus", bus);
            hdNode.setValue <LONG> ("channel", att->channel());
            hdNode.setValue <LONG> ("device", att->device());
        }
    }

    return S_OK;
}

/**
 *  Saves machine state settings as defined by aFlags
 *  (SaveSTS_* values).
 *
 *  @param aFlags   Combination of SaveSTS_* flags.
 *
 *  @note Locks objects for writing.
 */
HRESULT Machine::saveStateSettings (int aFlags)
{
    if (aFlags == 0)
        return S_OK;

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoLock alock (this);

    AssertReturn (isConfigLocked(), E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        using namespace settings;

        /* load the config file */
#if 0
        /// @todo disabled until made thread-safe by using handle duplicates
        File file (File::ReadWrite, mData->mHandleCfgFile,
                   Utf8Str (mData->mConfigFileFull));
#else
        File file (File::ReadWrite, Utf8Str (mData->mConfigFileFull));
#endif
        XmlTreeBackend tree;

        rc = VirtualBox::loadSettingsTree_ForUpdate (tree, file);
        CheckComRCReturnRC (rc);

        Key machineNode = tree.rootKey().key ("Machine");

        if (aFlags & SaveSTS_CurStateModified)
        {
            /* defaults to true */
            machineNode.setValueOr <bool> ("currentStateModified",
                                           !!mData->mCurrentStateModified, true);
        }

        if (aFlags & SaveSTS_StateFilePath)
        {
            if (mSSData->mStateFilePath)
            {
                /* try to make the file name relative to the settings file dir */
                Utf8Str stateFilePath = mSSData->mStateFilePath;
                calculateRelativePath (stateFilePath, stateFilePath);
                machineNode.setStringValue ("stateFile", stateFilePath);
            }
            else
                machineNode.zapValue ("stateFile");
        }

        if (aFlags & SaveSTS_StateTimeStamp)
        {
            Assert (mData->mMachineState != MachineState_Aborted ||
                    mSSData->mStateFilePath.isNull());

            machineNode.setValue <RTTIMESPEC> ("lastStateChange",
                                               mData->mLastStateChange);

            /* set the aborted attribute when appropriate, defaults to false */
            machineNode.setValueOr <bool> ("aborted",
                                           mData->mMachineState == MachineState_Aborted,
                                           false);
        }

        /* save settings on success */
        rc = VirtualBox::saveSettingsTree (tree, file,
                                           mData->mSettingsFileVersion);
        CheckComRCReturnRC (rc);
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions (RT_SRC_POS);
    }

    return rc;
}

/**
 *  Cleans up all differencing hard disks based on immutable hard disks.
 *
 *  @note Locks objects!
 */
HRESULT Machine::wipeOutImmutableDiffs()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReaderLock alock (this);

    AssertReturn (mData->mMachineState == MachineState_PoweredOff ||
                  mData->mMachineState == MachineState_Aborted, E_FAIL);

    for (HDData::HDAttachmentList::const_iterator it = mHDData->mHDAttachments.begin();
         it != mHDData->mHDAttachments.end();
         ++ it)
    {
        ComObjPtr <HardDisk> hd = (*it)->hardDisk();
        AutoLock hdLock (hd);

        if(hd->isParentImmutable())
        {
            /// @todo (dmik) no error handling for now
            //  (need async error reporting for this)
            hd->asVDI()->wipeOutImage();
        }
    }

    return S_OK;
}

/**
 *  Fixes up lazy hard disk attachments by creating or deleting differencing
 *  hard disks when machine settings are being committed.
 *  Must be called only from #commit().
 *
 *  @note Locks objects!
 */
HRESULT Machine::fixupHardDisks (bool aCommit)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoLock alock (this);

    /* no attac/detach operations -- nothing to do */
    if (!mHDData.isBackedUp())
    {
        mHDData->mHDAttachmentsChanged = false;
        return S_OK;
    }

    AssertReturn (mData->mRegistered, E_FAIL);

    if (aCommit)
    {
        /*
         *  changes are being committed,
         *  perform actual diff image creation, deletion etc.
         */

        /* take a copy of backed up attachments (will modify it) */
        HDData::HDAttachmentList backedUp = mHDData.backedUpData()->mHDAttachments;
        /* list of new diffs created */
        std::list <ComObjPtr <HardDisk> > newDiffs;

        HRESULT rc = S_OK;

        /* go through current attachments */
        for (HDData::HDAttachmentList::const_iterator
             it = mHDData->mHDAttachments.begin();
             it != mHDData->mHDAttachments.end();
             ++ it)
        {
            ComObjPtr <HardDiskAttachment> hda = *it;
            ComObjPtr <HardDisk> hd = hda->hardDisk();
            AutoLock hdLock (hd);

            if (!hda->isDirty())
            {
                /*
                 *  not dirty, therefore was either attached before backing up
                 *  or doesn't need any fixup (already fixed up); try to locate
                 *  this hard disk among backed up attachments and remove from
                 *  there to prevent it from being deassociated/deleted
                 */
                HDData::HDAttachmentList::iterator oldIt;
                for (oldIt = backedUp.begin(); oldIt != backedUp.end(); ++ oldIt)
                    if ((*oldIt)->hardDisk().equalsTo (hd))
                        break;
                if (oldIt != backedUp.end())
                {
                    /* remove from there */
                    backedUp.erase (oldIt);
                    Log3 (("FC: %ls found in old\n", hd->toString().raw()));
                }
            }
            else
            {
                /* dirty, determine what to do */

                bool needDiff = false;
                bool searchAmongSnapshots = false;

                switch (hd->type())
                {
                    case HardDiskType_Immutable:
                    {
                        /* decrease readers increased in AttachHardDisk() */
                        hd->releaseReader();
                        Log3 (("FC: %ls released\n", hd->toString().raw()));
                        /* indicate we need a diff (indirect attachment) */
                        needDiff = true;
                        break;
                    }
                    case HardDiskType_Writethrough:
                    {
                        /* reset the dirty flag */
                        hda->updateHardDisk (hd, false /* aDirty */);
                        Log3 (("FC: %ls updated\n", hd->toString().raw()));
                        break;
                    }
                    case HardDiskType_Normal:
                    {
                        if (hd->snapshotId().isEmpty())
                        {
                            /* reset the dirty flag */
                            hda->updateHardDisk (hd, false /* aDirty */);
                            Log3 (("FC: %ls updated\n", hd->toString().raw()));
                        }
                        else
                        {
                            /* decrease readers increased in AttachHardDisk() */
                            hd->releaseReader();
                            Log3 (("FC: %ls released\n", hd->toString().raw()));
                            /* indicate we need a diff (indirect attachment) */
                            needDiff = true;
                            /* search for the most recent base among snapshots */
                            searchAmongSnapshots = true;
                        }
                        break;
                    }
                }

                if (!needDiff)
                    continue;

                bool createDiff = false;

                /*
                 *  see whether any previously attached hard disk has the
                 *  the currently attached one (Normal or Independent) as
                 *  the root
                 */

                HDData::HDAttachmentList::iterator foundIt = backedUp.end();

                for (HDData::HDAttachmentList::iterator it = backedUp.begin();
                     it != backedUp.end();
                     ++ it)
                {
                    if ((*it)->hardDisk()->root().equalsTo (hd))
                    {
                        /*
                         *  matched dev and ctl (i.e. attached to the same place)
                         *  will win and immediately stop the search; otherwise
                         *  the first attachment that matched the hd only will
                         *  be used
                         */
                        if ((*it)->device() == hda->device() &&
                            (*it)->channel() == hda->channel() &&
                            (*it)->bus() == hda->bus())
                        {
                            foundIt = it;
                            break;
                        }
                        else
                        if (foundIt == backedUp.end())
                        {
                            /*
                             *  not an exact match; ensure there is no exact match
                             *  among other current attachments referring the same
                             *  root (to prevent this attachmend from reusing the
                             *  hard disk of the other attachment that will later
                             *  give the exact match or already gave it before)
                             */
                            bool canReuse = true;
                            for (HDData::HDAttachmentList::const_iterator
                                 it2 = mHDData->mHDAttachments.begin();
                                 it2 != mHDData->mHDAttachments.end();
                                 ++ it2)
                            {
                                if ((*it2)->device() == (*it)->device() &&
                                    (*it2)->channel() == (*it)->channel() &&
                                    (*it2)->bus() == (*it)->bus() &&
                                    (*it2)->hardDisk()->root().equalsTo (hd))
                                {
                                    /*
                                     *  the exact match, either non-dirty or dirty
                                     *  one refers the same root: in both cases
                                     *  we cannot reuse the hard disk, so break
                                     */
                                    canReuse = false;
                                    break;
                                }
                            }

                            if (canReuse)
                                foundIt = it;
                        }
                    }
                }

                if (foundIt != backedUp.end())
                {
                    /* found either one or another, reuse the diff */
                    hda->updateHardDisk ((*foundIt)->hardDisk(),
                                         false /* aDirty */);
                    Log3 (("FC: %ls reused as %ls\n", hd->toString().raw(),
                           (*foundIt)->hardDisk()->toString().raw()));
                    /* remove from there */
                    backedUp.erase (foundIt);
                }
                else
                {
                    /* was not attached, need a diff */
                    createDiff = true;
                }

                if (!createDiff)
                    continue;

                ComObjPtr <HardDisk> baseHd = hd;

                if (searchAmongSnapshots)
                {
                    /*
                     *  find the most recent diff based on the currently
                     *  attached root (Normal hard disk) among snapshots
                     */

                    ComObjPtr <Snapshot> snap = mData->mCurrentSnapshot;

                    while (snap)
                    {
                        AutoLock snapLock (snap);

                        const HDData::HDAttachmentList &snapAtts =
                            snap->data().mMachine->mHDData->mHDAttachments;

                        HDData::HDAttachmentList::const_iterator foundIt = snapAtts.end();

                        for (HDData::HDAttachmentList::const_iterator
                             it = snapAtts.begin(); it != snapAtts.end(); ++ it)
                        {
                            if ((*it)->hardDisk()->root().equalsTo (hd))
                            {
                                /*
                                 *  matched dev and ctl (i.e. attached to the same place)
                                 *  will win and immediately stop the search; otherwise
                                 *  the first attachment that matched the hd only will
                                 *  be used
                                 */
                                if ((*it)->device() == hda->device() &&
                                    (*it)->channel() == hda->channel() &&
                                    (*it)->bus() == hda->bus())
                                {
                                    foundIt = it;
                                    break;
                                }
                                else
                                if (foundIt == snapAtts.end())
                                    foundIt = it;
                            }
                        }

                        if (foundIt != snapAtts.end())
                        {
                            /* the most recent diff has been found, use as a base */
                            baseHd = (*foundIt)->hardDisk();
                            Log3 (("FC: %ls: recent found %ls\n",
                                   hd->toString().raw(), baseHd->toString().raw()));
                            break;
                        }

                        snap = snap->parent();
                    }
                }

                /* create a new diff for the hard disk being indirectly attached */

                AutoLock baseHdLock (baseHd);
                baseHd->addReader();

                ComObjPtr <HVirtualDiskImage> vdi;
                rc = baseHd->createDiffHardDisk (mUserData->mSnapshotFolderFull,
                                                 mData->mUuid, vdi, NULL);
                baseHd->releaseReader();
                CheckComRCBreakRC (rc);

                newDiffs.push_back (ComObjPtr <HardDisk> (vdi));

                /* update the attachment and reset the dirty flag */
                hda->updateHardDisk (ComObjPtr <HardDisk> (vdi),
                                     false /* aDirty */);
                Log3 (("FC: %ls: diff created %ls\n",
                       baseHd->toString().raw(), vdi->toString().raw()));
            }
        }

        if (FAILED (rc))
        {
            /* delete diffs we created */
            for (std::list <ComObjPtr <HardDisk> >::const_iterator
                 it = newDiffs.begin(); it != newDiffs.end(); ++ it)
            {
                /*
                 *  unregisterDiffHardDisk() is supposed to delete and uninit
                 *  the differencing hard disk
                 */
                mParent->unregisterDiffHardDisk (*it);
                /* too bad if we fail here, but nothing to do, just continue */
            }

            /* the best is to rollback the changes... */
            mHDData.rollback();
            mHDData->mHDAttachmentsChanged = false;
            Log3 (("FC: ROLLED BACK\n"));
            return rc;
        }

        /*
         *  go through the rest of old attachments and delete diffs
         *  or deassociate hard disks from machines (they will become detached)
         */
        for (HDData::HDAttachmentList::iterator
             it = backedUp.begin(); it != backedUp.end(); ++ it)
        {
            ComObjPtr <HardDiskAttachment> hda = *it;
            ComObjPtr <HardDisk> hd = hda->hardDisk();
            AutoLock hdLock (hd);

            if (hd->isDifferencing())
            {
                /*
                 *  unregisterDiffHardDisk() is supposed to delete and uninit
                 *  the differencing hard disk
                 */
                Log3 (("FC: %ls diff deleted\n", hd->toString().raw()));
                rc = mParent->unregisterDiffHardDisk (hd);
                /*
                 *  too bad if we fail here, but nothing to do, just continue
                 *  (the last rc will be returned to the caller though)
                 */
            }
            else
            {
                /* deassociate from this machine */
                Log3 (("FC: %ls deassociated\n", hd->toString().raw()));
                hd->setMachineId (Guid());
            }
        }

        /* commit all the changes */
        mHDData->mHDAttachmentsChanged = mHDData.hasActualChanges();
        mHDData.commit();
        Log3 (("FC: COMMITTED\n"));

        return rc;
    }

    /*
     *  changes are being rolled back,
     *  go trhough all current attachments and fix up dirty ones
     *  the way it is done in DetachHardDisk()
     */

    for (HDData::HDAttachmentList::iterator it = mHDData->mHDAttachments.begin();
         it != mHDData->mHDAttachments.end();
         ++ it)
    {
        ComObjPtr <HardDiskAttachment> hda = *it;
        ComObjPtr <HardDisk> hd = hda->hardDisk();
        AutoLock hdLock (hd);

        if (hda->isDirty())
        {
            switch (hd->type())
            {
                case HardDiskType_Immutable:
                {
                    /* decrease readers increased in AttachHardDisk() */
                    hd->releaseReader();
                    Log3 (("FR: %ls released\n", hd->toString().raw()));
                    break;
                }
                case HardDiskType_Writethrough:
                {
                    /* deassociate from this machine */
                    hd->setMachineId (Guid());
                    Log3 (("FR: %ls deassociated\n", hd->toString().raw()));
                    break;
                }
                case HardDiskType_Normal:
                {
                    if (hd->snapshotId().isEmpty())
                    {
                        /* deassociate from this machine */
                        hd->setMachineId (Guid());
                        Log3 (("FR: %ls deassociated\n", hd->toString().raw()));
                    }
                    else
                    {
                        /* decrease readers increased in AttachHardDisk() */
                        hd->releaseReader();
                        Log3 (("FR: %ls released\n", hd->toString().raw()));
                    }

                    break;
                }
            }
        }
    }

    /* rollback all the changes */
    mHDData.rollback();
    Log3 (("FR: ROLLED BACK\n"));

    return S_OK;
}

/**
 *  Creates differencing hard disks for all normal hard disks
 *  and replaces attachments to refer to created disks.
 *  Used when taking a snapshot or when discarding the current state.
 *
 *  @param aSnapshotId      ID of the snapshot being taken
 *                          or NULL if the current state is being discarded
 *  @param aFolder          folder where to create diff. hard disks
 *  @param aProgress        progress object to run (must contain at least as
 *                          many operations left as the number of VDIs attached)
 *  @param aOnline          whether the machine is online (i.e., when the EMT
 *                          thread is paused, OR when current hard disks are
 *                          marked as busy for some other reason)
 *
 *  @note
 *      The progress object is not marked as completed, neither on success
 *      nor on failure. This is a responsibility of the caller.
 *
 *  @note Locks mParent + this object for writing
 */
HRESULT Machine::createSnapshotDiffs (const Guid *aSnapshotId,
                                      const Bstr &aFolder,
                                      const ComObjPtr <Progress> &aProgress,
                                      bool aOnline)
{
    AssertReturn (!aFolder.isEmpty(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* accessing mParent methods below needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = S_OK;

    // first pass: check accessibility before performing changes
    if (!aOnline)
    {
        for (HDData::HDAttachmentList::const_iterator it = mHDData->mHDAttachments.begin();
             it != mHDData->mHDAttachments.end();
             ++ it)
        {
            ComObjPtr <HardDiskAttachment> hda = *it;
            ComObjPtr <HardDisk> hd = hda->hardDisk();
            AutoLock hdLock (hd);

            ComAssertMsgBreak (hd->type() == HardDiskType_Normal,
                               ("Invalid hard disk type %d\n", hd->type()),
                               rc = E_FAIL);

            ComAssertMsgBreak (!hd->isParentImmutable() ||
                               hd->storageType() == HardDiskStorageType_VirtualDiskImage,
                               ("Invalid hard disk storage type %d\n", hd->storageType()),
                               rc = E_FAIL);

            Bstr accessError;
            rc = hd->getAccessible (accessError);
            CheckComRCBreakRC (rc);

            if (!accessError.isNull())
            {
                rc = setError (E_FAIL,
                    tr ("Hard disk '%ls' is not accessible (%ls)"),
                    hd->toString().raw(), accessError.raw());
                break;
            }
        }
        CheckComRCReturnRC (rc);
    }

    HDData::HDAttachmentList attachments;

    // second pass: perform changes
    for (HDData::HDAttachmentList::const_iterator it = mHDData->mHDAttachments.begin();
         it != mHDData->mHDAttachments.end();
         ++ it)
    {
        ComObjPtr <HardDiskAttachment> hda = *it;
        ComObjPtr <HardDisk> hd = hda->hardDisk();
        AutoLock hdLock (hd);

        ComObjPtr <HardDisk> parent = hd->parent();
        AutoLock parentHdLock (parent);

        ComObjPtr <HardDisk> newHd;

        // clear busy flag if the VM is online
        if (aOnline)
            hd->clearBusy();
        // increase readers
        hd->addReader();

        if (hd->isParentImmutable())
        {
            aProgress->advanceOperation (Bstr (Utf8StrFmt (
                tr ("Preserving immutable hard disk '%ls'"),
                parent->toString (true /* aShort */).raw())));

            parentHdLock.unlock();
            alock.leave();

            // create a copy of the independent diff
            ComObjPtr <HVirtualDiskImage> vdi;
            rc = hd->asVDI()->cloneDiffImage (aFolder, mData->mUuid, vdi,
                                              aProgress);
            newHd = vdi;

            alock.enter();
            parentHdLock.lock();

            // decrease readers (hd is no more used for reading in any case)
            hd->releaseReader();
        }
        else
        {
            // checked in the first pass
            Assert (hd->type() == HardDiskType_Normal);

            aProgress->advanceOperation (Bstr (Utf8StrFmt (
                tr ("Creating a differencing hard disk for '%ls'"),
                hd->root()->toString (true /* aShort */).raw())));

            parentHdLock.unlock();
            alock.leave();

            // create a new diff for the image being attached
            ComObjPtr <HVirtualDiskImage> vdi;
            rc = hd->createDiffHardDisk (aFolder, mData->mUuid, vdi, aProgress);
            newHd = vdi;

            alock.enter();
            parentHdLock.lock();

            if (SUCCEEDED (rc))
            {
                // if online, hd must keep a reader referece
                if (!aOnline)
                    hd->releaseReader();
            }
            else
            {
                // decrease readers
                hd->releaseReader();
            }
        }

        if (SUCCEEDED (rc))
        {
            ComObjPtr <HardDiskAttachment> newHda;
            newHda.createObject();
            rc = newHda->init (newHd, hda->bus(), hda->channel(), hda->device(),
                               false /* aDirty */);

            if (SUCCEEDED (rc))
            {
                // associate the snapshot id with the old hard disk
                if (hd->type() != HardDiskType_Writethrough && aSnapshotId)
                    hd->setSnapshotId (*aSnapshotId);

                // add the new attachment
                attachments.push_back (newHda);

                // if online, newHd must be marked as busy
                if (aOnline)
                    newHd->setBusy();
            }
        }

        if (FAILED (rc))
        {
            // set busy flag back if the VM is online
            if (aOnline)
                hd->setBusy();
            break;
        }
    }

    if (SUCCEEDED (rc))
    {
        // replace the whole list of attachments with the new one
        mHDData->mHDAttachments = attachments;
    }
    else
    {
        // delete those diffs we've just created
        for (HDData::HDAttachmentList::const_iterator it = attachments.begin();
             it != attachments.end();
             ++ it)
        {
            ComObjPtr <HardDisk> hd = (*it)->hardDisk();
            AutoLock hdLock (hd);
            Assert (hd->children().size() == 0);
            Assert (hd->isDifferencing());
            // unregisterDiffHardDisk() is supposed to delete and uninit
            // the differencing hard disk
            mParent->unregisterDiffHardDisk (hd);
        }
    }

    return rc;
}

/**
 *  Deletes differencing hard disks created by createSnapshotDiffs() in case
 *  if snapshot creation was failed.
 *
 *  @param aSnapshot        failed snapshot
 *
 *  @note Locks mParent + this object for writing.
 */
HRESULT Machine::deleteSnapshotDiffs (const ComObjPtr <Snapshot> &aSnapshot)
{
    AssertReturn (!aSnapshot.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* accessing mParent methods below needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    /* short cut: check whether attachments are all the same */
    if (mHDData->mHDAttachments == aSnapshot->data().mMachine->mHDData->mHDAttachments)
        return S_OK;

    HRESULT rc = S_OK;

    for (HDData::HDAttachmentList::const_iterator it = mHDData->mHDAttachments.begin();
         it != mHDData->mHDAttachments.end();
         ++ it)
    {
        ComObjPtr <HardDiskAttachment> hda = *it;
        ComObjPtr <HardDisk> hd = hda->hardDisk();
        AutoLock hdLock (hd);

        ComObjPtr <HardDisk> parent = hd->parent();
        AutoLock parentHdLock (parent);

        if (!parent || parent->snapshotId() != aSnapshot->data().mId)
            continue;

        /* must not have children */
        ComAssertRet (hd->children().size() == 0, E_FAIL);

        /* deassociate the old hard disk from the given snapshot's ID */
        parent->setSnapshotId (Guid());

        /* unregisterDiffHardDisk() is supposed to delete and uninit
         * the differencing hard disk */
        rc = mParent->unregisterDiffHardDisk (hd);
        /* continue on error */
    }

    /* restore the whole list of attachments from the failed snapshot */
    mHDData->mHDAttachments = aSnapshot->data().mMachine->mHDData->mHDAttachments;

    return rc;
}

/**
 *  Helper to lock the machine configuration for write access.
 *
 *  @return S_OK or E_FAIL and sets error info on failure
 *
 *  @note Doesn't lock anything (must be called from this object's lock)
 */
HRESULT Machine::lockConfig()
{
    HRESULT rc = S_OK;

    if (!isConfigLocked())
    {
        /* open the associated config file */
        int vrc = RTFileOpen (&mData->mHandleCfgFile,
                              Utf8Str (mData->mConfigFileFull),
                              RTFILE_O_READWRITE | RTFILE_O_OPEN |
                              RTFILE_O_DENY_WRITE);
        if (VBOX_FAILURE (vrc))
            mData->mHandleCfgFile = NIL_RTFILE;
    }

    LogFlowThisFunc (("mConfigFile={%ls}, mHandleCfgFile=%d, rc=%08X\n",
                      mData->mConfigFileFull.raw(), mData->mHandleCfgFile, rc));
    return rc;
}

/**
 *  Helper to unlock the machine configuration from write access
 *
 *  @return S_OK
 *
 *  @note Doesn't lock anything.
 *  @note Not thread safe (must be called from this object's lock).
 */
HRESULT Machine::unlockConfig()
{
    HRESULT rc = S_OK;

    if (isConfigLocked())
    {
        RTFileFlush(mData->mHandleCfgFile);
        RTFileClose(mData->mHandleCfgFile);
        /** @todo flush the directory. */
        mData->mHandleCfgFile = NIL_RTFILE;
    }

    LogFlowThisFunc (("\n"));

    return rc;
}

/**
 *  Returns true if the settings file is located in the directory named exactly
 *  as the machine. This will be true if the machine settings structure was
 *  created by default in #openConfigLoader().
 *
 *  @param aSettingsDir if not NULL, the full machine settings file directory
 *                      name will be assigned there.
 *
 *  @note Doesn't lock anything.
 *  @note Not thread safe (must be called from this object's lock).
 */
bool Machine::isInOwnDir (Utf8Str *aSettingsDir /* = NULL */)
{
    Utf8Str settingsDir = mData->mConfigFileFull;
    RTPathStripFilename (settingsDir.mutableRaw());
    char *dirName = RTPathFilename (settingsDir);

    AssertReturn (dirName, false);

    /* if we don't rename anything on name change, return false shorlty */
    if (!mUserData->mNameSync)
        return false;

    if (aSettingsDir)
        *aSettingsDir = settingsDir;

    return Bstr (dirName) == mUserData->mName;
}

/**
 *  @note Locks objects for reading!
 */
bool Machine::isModified()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReaderLock alock (this);

    for (ULONG slot = 0; slot < ELEMENTS (mNetworkAdapters); slot ++)
        if (mNetworkAdapters [slot] && mNetworkAdapters [slot]->isModified())
            return true;

    for (ULONG slot = 0; slot < ELEMENTS (mSerialPorts); slot ++)
        if (mSerialPorts [slot] && mSerialPorts [slot]->isModified())
            return true;

    for (ULONG slot = 0; slot < ELEMENTS (mParallelPorts); slot ++)
        if (mParallelPorts [slot] && mParallelPorts [slot]->isModified())
            return true;

    return
        mUserData.isBackedUp() ||
        mHWData.isBackedUp() ||
        mHDData.isBackedUp() ||
#ifdef VBOX_VRDP
        (mVRDPServer && mVRDPServer->isModified()) ||
#endif
        (mDVDDrive && mDVDDrive->isModified()) ||
        (mFloppyDrive && mFloppyDrive->isModified()) ||
        (mAudioAdapter && mAudioAdapter->isModified()) ||
        (mUSBController && mUSBController->isModified()) ||
        (mSATAController && mSATAController->isModified()) ||
        (mBIOSSettings && mBIOSSettings->isModified());
}

/**
 *  @note This method doesn't check (ignores) actual changes to mHDData.
 *  Use mHDData.mHDAttachmentsChanged right after #commit() instead.
 *
 *  @param aIgnoreUserData      |true| to ignore changes to mUserData
 *
 *  @note Locks objects for reading!
 */
bool Machine::isReallyModified (bool aIgnoreUserData /* = false */)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReaderLock alock (this);

    for (ULONG slot = 0; slot < ELEMENTS (mNetworkAdapters); slot ++)
        if (mNetworkAdapters [slot] && mNetworkAdapters [slot]->isReallyModified())
            return true;

    for (ULONG slot = 0; slot < ELEMENTS (mSerialPorts); slot ++)
        if (mSerialPorts [slot] && mSerialPorts [slot]->isReallyModified())
            return true;

    for (ULONG slot = 0; slot < ELEMENTS (mParallelPorts); slot ++)
        if (mParallelPorts [slot] && mParallelPorts [slot]->isReallyModified())
            return true;

    return
        (!aIgnoreUserData && mUserData.hasActualChanges()) ||
        mHWData.hasActualChanges() ||
        /* ignore mHDData */
        //mHDData.hasActualChanges() ||
#ifdef VBOX_VRDP
        (mVRDPServer && mVRDPServer->isReallyModified()) ||
#endif
        (mDVDDrive && mDVDDrive->isReallyModified()) ||
        (mFloppyDrive && mFloppyDrive->isReallyModified()) ||
        (mAudioAdapter && mAudioAdapter->isReallyModified()) ||
        (mUSBController && mUSBController->isReallyModified()) ||
        (mSATAController && mSATAController->isReallyModified()) ||
        (mBIOSSettings && mBIOSSettings->isReallyModified());
}

/**
 *  Discards all changes to machine settings.
 *
 *  @param  aNotify whether to notify the direct session about changes or not
 *
 *  @note Locks objects!
 */
void Machine::rollback (bool aNotify)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    AutoLock alock (this);

    /* check for changes in own data */

    bool sharedFoldersChanged = false;

    if (aNotify && mHWData.isBackedUp())
    {
        if (mHWData->mSharedFolders.size() !=
            mHWData.backedUpData()->mSharedFolders.size())
            sharedFoldersChanged = true;
        else
        {
            for (HWData::SharedFolderList::iterator rit =
                     mHWData->mSharedFolders.begin();
                 rit != mHWData->mSharedFolders.end() && !sharedFoldersChanged;
                 ++ rit)
            {
                for (HWData::SharedFolderList::iterator cit =
                         mHWData.backedUpData()->mSharedFolders.begin();
                     cit != mHWData.backedUpData()->mSharedFolders.end();
                     ++ cit)
                {
                    if ((*cit)->name() != (*rit)->name() ||
                        (*cit)->hostPath() != (*rit)->hostPath())
                    {
                        sharedFoldersChanged = true;
                        break;
                    }
                }
            }
        }
    }

    mUserData.rollback();

    mHWData.rollback();

    if (mHDData.isBackedUp())
        fixupHardDisks (false /* aCommit */);

    /* check for changes in child objects */

    bool vrdpChanged = false, dvdChanged = false, floppyChanged = false,
         usbChanged = false, sataChanged = false;

    ComPtr <INetworkAdapter> networkAdapters [ELEMENTS (mNetworkAdapters)];
    ComPtr <ISerialPort> serialPorts [ELEMENTS (mSerialPorts)];
    ComPtr <IParallelPort> parallelPorts [ELEMENTS (mParallelPorts)];

    if (mBIOSSettings)
        mBIOSSettings->rollback();

#ifdef VBOX_VRDP
    if (mVRDPServer)
        vrdpChanged = mVRDPServer->rollback();
#endif

    if (mDVDDrive)
        dvdChanged = mDVDDrive->rollback();

    if (mFloppyDrive)
        floppyChanged = mFloppyDrive->rollback();

    if (mAudioAdapter)
        mAudioAdapter->rollback();

    if (mUSBController)
        usbChanged = mUSBController->rollback();

    if (mSATAController)
        sataChanged = mSATAController->rollback();

    for (ULONG slot = 0; slot < ELEMENTS (mNetworkAdapters); slot ++)
        if (mNetworkAdapters [slot])
            if (mNetworkAdapters [slot]->rollback())
                networkAdapters [slot] = mNetworkAdapters [slot];

    for (ULONG slot = 0; slot < ELEMENTS (mSerialPorts); slot ++)
        if (mSerialPorts [slot])
            if (mSerialPorts [slot]->rollback())
                serialPorts [slot] = mSerialPorts [slot];

    for (ULONG slot = 0; slot < ELEMENTS (mParallelPorts); slot ++)
        if (mParallelPorts [slot])
            if (mParallelPorts [slot]->rollback())
                parallelPorts [slot] = mParallelPorts [slot];

    if (aNotify)
    {
        /* inform the direct session about changes */

        ComObjPtr <Machine> that = this;
        alock.leave();

        if (sharedFoldersChanged)
            that->onSharedFolderChange();

        if (vrdpChanged)
            that->onVRDPServerChange();
        if (dvdChanged)
            that->onDVDDriveChange();
        if (floppyChanged)
            that->onFloppyDriveChange();
        if (usbChanged)
            that->onUSBControllerChange();
        if (sataChanged)
            that->onSATAControllerChange();

        for (ULONG slot = 0; slot < ELEMENTS (networkAdapters); slot ++)
            if (networkAdapters [slot])
                that->onNetworkAdapterChange (networkAdapters [slot]);
        for (ULONG slot = 0; slot < ELEMENTS (serialPorts); slot ++)
            if (serialPorts [slot])
                that->onSerialPortChange (serialPorts [slot]);
        for (ULONG slot = 0; slot < ELEMENTS (parallelPorts); slot ++)
            if (parallelPorts [slot])
                that->onParallelPortChange (parallelPorts [slot]);
    }
}

/**
 *  Commits all the changes to machine settings.
 *
 *  Note that when committing fails at some stage, it still continues
 *  until the end. So, all data will either be actually committed or rolled
 *  back (for failed cases) and the returned result code will describe the
 *  first failure encountered. However, #isModified() will still return true
 *  in case of failure, to indicade that settings in memory and on disk are
 *  out of sync.
 *
 *  @note Locks objects!
 */
HRESULT Machine::commit()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoLock alock (this);

    HRESULT rc = S_OK;

    /*
     *  use safe commit to ensure Snapshot machines (that share mUserData)
     *  will still refer to a valid memory location
     */
    mUserData.commitCopy();

    mHWData.commit();

    if (mHDData.isBackedUp())
        rc = fixupHardDisks (true /* aCommit */);

    mBIOSSettings->commit();
#ifdef VBOX_VRDP
    mVRDPServer->commit();
#endif
    mDVDDrive->commit();
    mFloppyDrive->commit();
    mAudioAdapter->commit();
    mUSBController->commit();
    mSATAController->commit();

    for (ULONG slot = 0; slot < ELEMENTS (mNetworkAdapters); slot ++)
        mNetworkAdapters [slot]->commit();
    for (ULONG slot = 0; slot < ELEMENTS (mSerialPorts); slot ++)
        mSerialPorts [slot]->commit();
    for (ULONG slot = 0; slot < ELEMENTS (mParallelPorts); slot ++)
        mParallelPorts [slot]->commit();

    if (mType == IsSessionMachine)
    {
        /* attach new data to the primary machine and reshare it */
        mPeer->mUserData.attach (mUserData);
        mPeer->mHWData.attach (mHWData);
        mPeer->mHDData.attach (mHDData);
    }

    if (FAILED (rc))
    {
        /*
         *  backup arbitrary data item to cause #isModified() to still return
         *  true in case of any error
         */
        mHWData.backup();
    }

    return rc;
}

/**
 *  Copies all the hardware data from the given machine.
 *
 *  @note
 *      This method must be called from under this object's lock.
 *  @note
 *      This method doesn't call #commit(), so all data remains backed up
 *      and unsaved.
 */
void Machine::copyFrom (Machine *aThat)
{
    AssertReturn (mType == IsMachine || mType == IsSessionMachine, (void) 0);
    AssertReturn (aThat->mType == IsSnapshotMachine, (void) 0);

    mHWData.assignCopy (aThat->mHWData);

    // create copies of all shared folders (mHWData after attiching a copy
    // contains just references to original objects)
    for (HWData::SharedFolderList::iterator it = mHWData->mSharedFolders.begin();
         it != mHWData->mSharedFolders.end();
         ++ it)
    {
        ComObjPtr <SharedFolder> folder;
        folder.createObject();
        HRESULT rc = folder->initCopy (machine(), *it);
        AssertComRC (rc);
        *it = folder;
    }

    mBIOSSettings->copyFrom (aThat->mBIOSSettings);
#ifdef VBOX_VRDP
    mVRDPServer->copyFrom (aThat->mVRDPServer);
#endif
    mDVDDrive->copyFrom (aThat->mDVDDrive);
    mFloppyDrive->copyFrom (aThat->mFloppyDrive);
    mAudioAdapter->copyFrom (aThat->mAudioAdapter);
    mUSBController->copyFrom (aThat->mUSBController);
    mSATAController->copyFrom (aThat->mSATAController);

    for (ULONG slot = 0; slot < ELEMENTS (mNetworkAdapters); slot ++)
        mNetworkAdapters [slot]->copyFrom (aThat->mNetworkAdapters [slot]);
    for (ULONG slot = 0; slot < ELEMENTS (mSerialPorts); slot ++)
        mSerialPorts [slot]->copyFrom (aThat->mSerialPorts [slot]);
    for (ULONG slot = 0; slot < ELEMENTS (mParallelPorts); slot ++)
        mParallelPorts [slot]->copyFrom (aThat->mParallelPorts [slot]);
}

/////////////////////////////////////////////////////////////////////////////
// SessionMachine class
/////////////////////////////////////////////////////////////////////////////

/** Task structure for asynchronous VM operations */
struct SessionMachine::Task
{
    Task (SessionMachine *m, Progress *p)
        : machine (m), progress (p)
        , state (m->mData->mMachineState) // save the current machine state
        , subTask (false), settingsChanged (false)
    {}

    void modifyLastState (MachineState_T s)
    {
        *const_cast <MachineState_T *> (&state) = s;
    }

    virtual void handler() = 0;

    const ComObjPtr <SessionMachine> machine;
    const ComObjPtr <Progress> progress;
    const MachineState_T state;

    bool subTask : 1;
    bool settingsChanged : 1;
};

/** Take snapshot task */
struct SessionMachine::TakeSnapshotTask : public SessionMachine::Task
{
    TakeSnapshotTask (SessionMachine *m)
        : Task (m, NULL) {}

    void handler() { machine->takeSnapshotHandler (*this); }
};

/** Discard snapshot task */
struct SessionMachine::DiscardSnapshotTask : public SessionMachine::Task
{
    DiscardSnapshotTask (SessionMachine *m, Progress *p, Snapshot *s)
        : Task (m, p)
        , snapshot (s) {}

    DiscardSnapshotTask (const Task &task, Snapshot *s)
        : Task (task)
        , snapshot (s) {}

    void handler() { machine->discardSnapshotHandler (*this); }

    const ComObjPtr <Snapshot> snapshot;
};

/** Discard current state task */
struct SessionMachine::DiscardCurrentStateTask : public SessionMachine::Task
{
    DiscardCurrentStateTask (SessionMachine *m, Progress *p,
                             bool discardCurSnapshot)
        : Task (m, p), discardCurrentSnapshot (discardCurSnapshot) {}

    void handler() { machine->discardCurrentStateHandler (*this); }

    const bool discardCurrentSnapshot;
};

////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (SessionMachine)

HRESULT SessionMachine::FinalConstruct()
{
    LogFlowThisFunc (("\n"));

    /* set the proper type to indicate we're the SessionMachine instance */
    unconst (mType) = IsSessionMachine;

#if defined(RT_OS_WINDOWS)
    mIPCSem = NULL;
#elif defined(RT_OS_OS2)
    mIPCSem = NULLHANDLE;
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    mIPCSem = -1;
#else
# error "Port me!"
#endif

    return S_OK;
}

void SessionMachine::FinalRelease()
{
    LogFlowThisFunc (("\n"));

    uninit (Uninit::Unexpected);
}

/**
 *  @note Must be called only by Machine::openSession() from its own write lock.
 */
HRESULT SessionMachine::init (Machine *aMachine)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mName={%ls}\n", aMachine->mUserData->mName.raw()));

    AssertReturn (aMachine, E_INVALIDARG);

    AssertReturn (aMachine->lockHandle()->isLockedOnCurrentThread(), E_FAIL);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    /* create the interprocess semaphore */
#if defined(RT_OS_WINDOWS)
    mIPCSemName = aMachine->mData->mConfigFileFull;
    for (size_t i = 0; i < mIPCSemName.length(); i++)
        if (mIPCSemName[i] == '\\')
            mIPCSemName[i] = '/';
    mIPCSem = ::CreateMutex (NULL, FALSE, mIPCSemName);
    ComAssertMsgRet (mIPCSem,
                     ("Cannot create IPC mutex '%ls', err=%d\n",
                      mIPCSemName.raw(), ::GetLastError()),
                     E_FAIL);
#elif defined(RT_OS_OS2)
    Utf8Str ipcSem = Utf8StrFmt ("\\SEM32\\VBOX\\VM\\{%Vuuid}",
                                 aMachine->mData->mUuid.raw());
    mIPCSemName = ipcSem;
    APIRET arc = ::DosCreateMutexSem ((PSZ) ipcSem.raw(), &mIPCSem, 0, FALSE);
    ComAssertMsgRet (arc == NO_ERROR,
                     ("Cannot create IPC mutex '%s', arc=%ld\n",
                      ipcSem.raw(), arc),
                     E_FAIL);
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    Utf8Str configFile = aMachine->mData->mConfigFileFull;
    char *configFileCP = NULL;
    int error;
    RTStrUtf8ToCurrentCP (&configFileCP, configFile);
    key_t key = ::ftok (configFileCP, 0);
    RTStrFree (configFileCP);
    mIPCSem = ::semget (key, 1, S_IRWXU | S_IRWXG | S_IRWXO | IPC_CREAT);
    error = errno;
    if (mIPCSem < 0 && error == ENOSYS)
    {
        setError(E_FAIL,
                tr ("Cannot create IPC semaphore. Most likely your host kernel lacks "
                     "support for SysV IPC. Check the host kernel configuration for "
                     "CONFIG_SYSVIPC=y"));
        return E_FAIL;
    }
    ComAssertMsgRet (mIPCSem >= 0, ("Cannot create IPC semaphore, errno=%d", error),
                     E_FAIL);
    /* set the initial value to 1 */
    int rv = ::semctl (mIPCSem, 0, SETVAL, 1);
    ComAssertMsgRet (rv == 0, ("Cannot init IPC semaphore, errno=%d", errno),
                     E_FAIL);
#else
# error "Port me!"
#endif

    /* memorize the peer Machine */
    unconst (mPeer) = aMachine;
    /* share the parent pointer */
    unconst (mParent) = aMachine->mParent;

    /* take the pointers to data to share */
    mData.share (aMachine->mData);
    mSSData.share (aMachine->mSSData);

    mUserData.share (aMachine->mUserData);
    mHWData.share (aMachine->mHWData);
    mHDData.share (aMachine->mHDData);

    unconst (mBIOSSettings).createObject();
    mBIOSSettings->init (this, aMachine->mBIOSSettings);
#ifdef VBOX_VRDP
    /* create another VRDPServer object that will be mutable */
    unconst (mVRDPServer).createObject();
    mVRDPServer->init (this, aMachine->mVRDPServer);
#endif
    /* create another DVD drive object that will be mutable */
    unconst (mDVDDrive).createObject();
    mDVDDrive->init (this, aMachine->mDVDDrive);
    /* create another floppy drive object that will be mutable */
    unconst (mFloppyDrive).createObject();
    mFloppyDrive->init (this, aMachine->mFloppyDrive);
    /* create another audio adapter object that will be mutable */
    unconst (mAudioAdapter).createObject();
    mAudioAdapter->init (this, aMachine->mAudioAdapter);
    /* create a list of serial ports that will be mutable */
    for (ULONG slot = 0; slot < ELEMENTS (mSerialPorts); slot ++)
    {
        unconst (mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->init (this, aMachine->mSerialPorts [slot]);
    }
    /* create a list of parallel ports that will be mutable */
    for (ULONG slot = 0; slot < ELEMENTS (mParallelPorts); slot ++)
    {
        unconst (mParallelPorts [slot]).createObject();
        mParallelPorts [slot]->init (this, aMachine->mParallelPorts [slot]);
    }
    /* create another USB controller object that will be mutable */
    unconst (mUSBController).createObject();
    mUSBController->init (this, aMachine->mUSBController);
    /* create another SATA controller object that will be mutable */
    unconst (mSATAController).createObject();
    mSATAController->init (this, aMachine->mSATAController);
    /* create a list of network adapters that will be mutable */
    for (ULONG slot = 0; slot < ELEMENTS (mNetworkAdapters); slot ++)
    {
        unconst (mNetworkAdapters [slot]).createObject();
        mNetworkAdapters [slot]->init (this, aMachine->mNetworkAdapters [slot]);
    }

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Uninitializes this session object. If the reason is other than
 *  Uninit::Unexpected, then this method MUST be called from #checkForDeath().
 *
 *  @param aReason          uninitialization reason
 *
 *  @note Locks mParent + this object for writing.
 */
void SessionMachine::uninit (Uninit::Reason aReason)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("reason=%d\n", aReason));

    /*
     *  Strongly reference ourselves to prevent this object deletion after
     *  mData->mSession.mMachine.setNull() below (which can release the last
     *  reference and call the destructor). Important: this must be done before
     *  accessing any members (and before AutoUninitSpan that does it as well).
     *  This self reference will be released as the very last step on return.
     */
    ComObjPtr <SessionMachine> selfRef = this;

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
    {
        LogFlowThisFunc (("Already uninitialized\n"));
        LogFlowThisFuncLeave();
        return;
    }

    if (autoUninitSpan.initFailed())
    {
        /* We've been called by init() because it's failed. It's not really
         * necessary (nor it's safe) to perform the regular uninit sequense
         * below, the following is enough.
         */
        LogFlowThisFunc (("Initialization failed.\n"));
#if defined(RT_OS_WINDOWS)
        if (mIPCSem)
            ::CloseHandle (mIPCSem);
        mIPCSem = NULL;
#elif defined(RT_OS_OS2)
        if (mIPCSem != NULLHANDLE)
            ::DosCloseMutexSem (mIPCSem);
        mIPCSem = NULLHANDLE;
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
        if (mIPCSem >= 0)
            ::semctl (mIPCSem, 0, IPC_RMID);
        mIPCSem = -1;
#else
# error "Port me!"
#endif
        uninitDataAndChildObjects();
        mData.free();
        unconst (mParent).setNull();
        unconst (mPeer).setNull();
        LogFlowThisFuncLeave();
        return;
    }

    /* We need to lock this object in uninit() because the lock is shared
     * with mPeer (as well as data we modify below). mParent->addProcessToReap()
     * and others need mParent lock. */
    AutoMultiWriteLock2 alock (mParent, this);

    MachineState_T lastState = mData->mMachineState;

    if (aReason == Uninit::Abnormal)
    {
        LogWarningThisFunc (("ABNORMAL client termination! (wasRunning=%d)\n",
                             lastState >= MachineState_Running));

        /* reset the state to Aborted */
        if (mData->mMachineState != MachineState_Aborted)
            setMachineState (MachineState_Aborted);
    }

    if (isModified())
    {
        LogWarningThisFunc (("Discarding unsaved settings changes!\n"));
        rollback (false /* aNotify */);
    }

    Assert (!mSnapshotData.mStateFilePath || !mSnapshotData.mSnapshot);
    if (mSnapshotData.mStateFilePath)
    {
        LogWarningThisFunc (("canceling failed save state request!\n"));
        endSavingState (FALSE /* aSuccess  */);
    }
    else if (!!mSnapshotData.mSnapshot)
    {
        LogWarningThisFunc (("canceling untaken snapshot!\n"));
        endTakingSnapshot (FALSE /* aSuccess  */);
    }

    /* release all captured USB devices */
    if (aReason == Uninit::Abnormal && lastState >= MachineState_Running)
    {
        /* Console::captureUSBDevices() is called in the VM process only after
         * setting the machine state to Starting or Restoring.
         * Console::detachAllUSBDevices() will be called upon successful
         * termination. So, we need to release USB devices only if there was
         * an abnormal termination of a running VM. */
        DetachAllUSBDevices (TRUE /* aDone */);
    }

    if (!mData->mSession.mType.isNull())
    {
        /* mType is not null when this machine's process has been started by
         * VirtualBox::OpenRemoteSession(), therefore it is our child.  We
         * need to queue the PID to reap the process (and avoid zombies on
         * Linux). */
        Assert (mData->mSession.mPid != NIL_RTPROCESS);
        mParent->addProcessToReap (mData->mSession.mPid);
    }

    mData->mSession.mPid = NIL_RTPROCESS;

    if (aReason == Uninit::Unexpected)
    {
        /* Uninitialization didn't come from #checkForDeath(), so tell the
         * client watcher thread to update the set of machines that have open
         * sessions. */
        mParent->updateClientWatcher();
    }

    /* uninitialize all remote controls */
    if (mData->mSession.mRemoteControls.size())
    {
        LogFlowThisFunc (("Closing remote sessions (%d):\n",
                          mData->mSession.mRemoteControls.size()));

        Data::Session::RemoteControlList::iterator it =
            mData->mSession.mRemoteControls.begin();
        while (it != mData->mSession.mRemoteControls.end())
        {
            LogFlowThisFunc (("  Calling remoteControl->Uninitialize()...\n"));
            HRESULT rc = (*it)->Uninitialize();
            LogFlowThisFunc (("  remoteControl->Uninitialize() returned %08X\n", rc));
            if (FAILED (rc))
                LogWarningThisFunc (("Forgot to close the remote session?\n"));
            ++ it;
        }
        mData->mSession.mRemoteControls.clear();
    }

    /*
     *  An expected uninitialization can come only from #checkForDeath().
     *  Otherwise it means that something's got really wrong (for examlple,
     *  the Session implementation has released the VirtualBox reference
     *  before it triggered #OnSessionEnd(), or before releasing IPC semaphore,
     *  etc). However, it's also possible, that the client releases the IPC
     *  semaphore correctly (i.e. before it releases the VirtualBox reference),
     *  but but the VirtualBox release event comes first to the server process.
     *  This case is practically possible, so we should not assert on an
     *  unexpected uninit, just log a warning.
     */

    if ((aReason == Uninit::Unexpected))
        LogWarningThisFunc (("Unexpected SessionMachine uninitialization!\n"));

    if (aReason != Uninit::Normal)
    {
        mData->mSession.mDirectControl.setNull();
    }
    else
    {
        /* this must be null here (see #OnSessionEnd()) */
        Assert (mData->mSession.mDirectControl.isNull());
        Assert (mData->mSession.mState == SessionState_Closing);
        Assert (!mData->mSession.mProgress.isNull());

        mData->mSession.mProgress->notifyComplete (S_OK);
        mData->mSession.mProgress.setNull();
    }

    /* remove the association between the peer machine and this session machine */
    Assert (mData->mSession.mMachine == this ||
            aReason == Uninit::Unexpected);

    /* reset the rest of session data */
    mData->mSession.mMachine.setNull();
    mData->mSession.mState = SessionState_Closed;
    mData->mSession.mType.setNull();

    /* close the interprocess semaphore before leaving the shared lock */
#if defined(RT_OS_WINDOWS)
    if (mIPCSem)
        ::CloseHandle (mIPCSem);
    mIPCSem = NULL;
#elif defined(RT_OS_OS2)
    if (mIPCSem != NULLHANDLE)
        ::DosCloseMutexSem (mIPCSem);
    mIPCSem = NULLHANDLE;
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    if (mIPCSem >= 0)
        ::semctl (mIPCSem, 0, IPC_RMID);
    mIPCSem = -1;
#else
# error "Port me!"
#endif

    /* fire an event */
    mParent->onSessionStateChange (mData->mUuid, SessionState_Closed);

    uninitDataAndChildObjects();

    /* free the essential data structure last */
    mData.free();

    /* leave the shared lock before setting the below two to NULL */
    alock.leave();

    unconst (mParent).setNull();
    unconst (mPeer).setNull();

    LogFlowThisFuncLeave();
}

// AutoLock::Lockable interface
////////////////////////////////////////////////////////////////////////////////

/**
 *  Overrides VirtualBoxBase::lockHandle() in order to share the lock handle
 *  with the primary Machine instance (mPeer).
 */
RWLockHandle *SessionMachine::lockHandle() const
{
    AssertReturn (!mPeer.isNull(), NULL);
    return mPeer->lockHandle();
}

// IInternalMachineControl methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  @note Locks the same as #setMachineState() does.
 */
STDMETHODIMP SessionMachine::UpdateState (MachineState_T machineState)
{
    return setMachineState (machineState);
}

/**
 *  @note Locks this object for reading.
 */
STDMETHODIMP SessionMachine::GetIPCId (BSTR *id)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReaderLock alock (this);

#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    mIPCSemName.cloneTo (id);
    return S_OK;
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    mData->mConfigFileFull.cloneTo (id);
    return S_OK;
#else
# error "Port me!"
#endif
}

/**
 *  Goes through the USB filters of the given machine to see if the given
 *  device matches any filter or not.
 *
 *  @note Locks the same as USBController::hasMatchingFilter() does.
 */
STDMETHODIMP SessionMachine::RunUSBDeviceFilters (IUSBDevice *aUSBDevice,
                                                  BOOL *aMatched,
                                                  ULONG *aMaskedIfs)
{
    LogFlowThisFunc (("\n"));

    if (!aUSBDevice)
        return E_INVALIDARG;
    if (!aMatched)
        return E_POINTER;

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    *aMatched = mUSBController->hasMatchingFilter (aUSBDevice, aMaskedIfs);
#else
    *aMatched = FALSE;
#endif

    return S_OK;
}

/**
 *  @note Locks the same as Host::captureUSBDevice() does.
 */
STDMETHODIMP SessionMachine::CaptureUSBDevice (INPTR GUIDPARAM aId)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

#ifdef VBOX_WITH_USB
    /* if cautureUSBDevice() fails, it must have set extended error info */
    return mParent->host()->captureUSBDevice (this, aId);
#else
    return E_FAIL;
#endif
}

/**
 *  @note Locks the same as Host::detachUSBDevice() does.
 */
STDMETHODIMP SessionMachine::DetachUSBDevice (INPTR GUIDPARAM aId, BOOL aDone)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    return mParent->host()->detachUSBDevice (this, aId, aDone);
#else
    return E_FAIL;
#endif
}

/**
 *  Inserts all machine filters to the USB proxy service and then calls
 *  Host::autoCaptureUSBDevices().
 *
 *  Called by Console from the VM process upon VM startup.
 *
 *  @note Locks what called methods lock.
 */
STDMETHODIMP SessionMachine::AutoCaptureUSBDevices()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    HRESULT rc = mUSBController->notifyProxy (true /* aInsertFilters */);
    AssertComRC (rc);
    NOREF (rc);

    return mParent->host()->autoCaptureUSBDevices (this);
#else
    return S_OK;
#endif
}

/**
 *  Removes all machine filters from the USB proxy service and then calls
 *  Host::detachAllUSBDevices().
 *
 *  Called by Console from the VM process upon normal VM termination or by
 *  SessionMachine::uninit() upon abnormal VM termination (from under the
 *  Machine/SessionMachine lock).
 *
 *  @note Locks what called methods lock.
 */
STDMETHODIMP SessionMachine::DetachAllUSBDevices(BOOL aDone)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    HRESULT rc = mUSBController->notifyProxy (false /* aInsertFilters */);
    AssertComRC (rc);
    NOREF (rc);

    return mParent->host()->detachAllUSBDevices (this, aDone);
#else
    return S_OK;
#endif
}

/**
 *  @note Locks mParent + this object for writing.
 */
STDMETHODIMP SessionMachine::OnSessionEnd (ISession *aSession,
                                           IProgress **aProgress)
{
    LogFlowThisFuncEnter();

    AssertReturn (aSession, E_INVALIDARG);
    AssertReturn (aProgress, E_INVALIDARG);

    AutoCaller autoCaller (this);

    LogFlowThisFunc (("state=%d\n", autoCaller.state()));
    /*
     *  We don't assert below because it might happen that a non-direct session
     *  informs us it is closed right after we've been uninitialized -- it's ok.
     */
    CheckComRCReturnRC (autoCaller.rc());

    /* get IInternalSessionControl interface */
    ComPtr <IInternalSessionControl> control (aSession);

    ComAssertRet (!control.isNull(), E_INVALIDARG);

    /* Progress::init() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    if (control.equalsTo (mData->mSession.mDirectControl))
    {
        ComAssertRet (aProgress, E_POINTER);

        /* The direct session is being normally closed by the client process
         * ----------------------------------------------------------------- */

        /* go to the closing state (essential for all open*Session() calls and
         * for #checkForDeath()) */
        Assert (mData->mSession.mState == SessionState_Open);
        mData->mSession.mState = SessionState_Closing;

        /* set direct control to NULL to release the remote instance */
        mData->mSession.mDirectControl.setNull();
        LogFlowThisFunc (("Direct control is set to NULL\n"));

        /*
         *  Create the progress object the client will use to wait until
         *  #checkForDeath() is called to uninitialize this session object
         *  after it releases the IPC semaphore.
         */
        ComObjPtr <Progress> progress;
        progress.createObject();
        progress->init (mParent, static_cast <IMachine *> (mPeer),
                        Bstr (tr ("Closing session")), FALSE /* aCancelable */);
        progress.queryInterfaceTo (aProgress);
        mData->mSession.mProgress = progress;
    }
    else
    {
        /* the remote session is being normally closed */
        Data::Session::RemoteControlList::iterator it =
            mData->mSession.mRemoteControls.begin();
        while (it != mData->mSession.mRemoteControls.end())
        {
            if (control.equalsTo (*it))
                break;
            ++it;
        }
        BOOL found = it != mData->mSession.mRemoteControls.end();
        ComAssertMsgRet (found, ("The session is not found in the session list!"),
                         E_INVALIDARG);
        mData->mSession.mRemoteControls.remove (*it);
    }

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  @note Locks mParent + this object for writing.
 */
STDMETHODIMP SessionMachine::BeginSavingState (IProgress *aProgress, BSTR *aStateFilePath)
{
    LogFlowThisFuncEnter();

    AssertReturn (aProgress, E_INVALIDARG);
    AssertReturn (aStateFilePath, E_POINTER);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* mParent->addProgress() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    AssertReturn (mData->mMachineState == MachineState_Paused &&
                  mSnapshotData.mLastState == MachineState_Null &&
                  mSnapshotData.mProgressId.isEmpty() &&
                  mSnapshotData.mStateFilePath.isNull(),
                  E_FAIL);

    /* memorize the progress ID and add it to the global collection */
    Guid progressId;
    HRESULT rc = aProgress->COMGETTER(Id) (progressId.asOutParam());
    AssertComRCReturn (rc, rc);
    rc = mParent->addProgress (aProgress);
    AssertComRCReturn (rc, rc);

    Bstr stateFilePath;
    /* stateFilePath is null when the machine is not running */
    if (mData->mMachineState == MachineState_Paused)
    {
        stateFilePath = Utf8StrFmt ("%ls%c{%Vuuid}.sav",
                                    mUserData->mSnapshotFolderFull.raw(),
                                    RTPATH_DELIMITER, mData->mUuid.raw());
    }

    /* fill in the snapshot data */
    mSnapshotData.mLastState = mData->mMachineState;
    mSnapshotData.mProgressId = progressId;
    mSnapshotData.mStateFilePath = stateFilePath;

    /* set the state to Saving (this is expected by Console::SaveState()) */
    setMachineState (MachineState_Saving);

    stateFilePath.cloneTo (aStateFilePath);

    return S_OK;
}

/**
 *  @note Locks mParent + this objects for writing.
 */
STDMETHODIMP SessionMachine::EndSavingState (BOOL aSuccess)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* endSavingState() need mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    AssertReturn (mData->mMachineState == MachineState_Saving &&
                  mSnapshotData.mLastState != MachineState_Null &&
                  !mSnapshotData.mProgressId.isEmpty() &&
                  !mSnapshotData.mStateFilePath.isNull(),
                  E_FAIL);

    /*
     *  on success, set the state to Saved;
     *  on failure, set the state to the state we had when BeginSavingState() was
     *  called (this is expected by Console::SaveState() and
     *  Console::saveStateThread())
     */
    if (aSuccess)
        setMachineState (MachineState_Saved);
    else
        setMachineState (mSnapshotData.mLastState);

    return endSavingState (aSuccess);
}

/**
 *  @note Locks this objects for writing.
 */
STDMETHODIMP SessionMachine::AdoptSavedState (INPTR BSTR aSavedStateFile)
{
    LogFlowThisFunc (("\n"));

    AssertReturn (aSavedStateFile, E_INVALIDARG);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoLock alock (this);

    AssertReturn (mData->mMachineState == MachineState_PoweredOff ||
                  mData->mMachineState == MachineState_Aborted,
                  E_FAIL);

    Utf8Str stateFilePathFull = aSavedStateFile;
    int vrc = calculateFullPath (stateFilePathFull, stateFilePathFull);
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Invalid saved state file path: '%ls' (%Vrc)"),
                aSavedStateFile, vrc);

    mSSData->mStateFilePath = stateFilePathFull;

    /* The below setMachineState() will detect the state transition and will
     * update the settings file */

    return setMachineState (MachineState_Saved);
}

/**
 *  @note Locks mParent + this objects for writing.
 */
STDMETHODIMP SessionMachine::BeginTakingSnapshot (
    IConsole *aInitiator, INPTR BSTR aName, INPTR BSTR aDescription,
    IProgress *aProgress, BSTR *aStateFilePath,
    IProgress **aServerProgress)
{
    LogFlowThisFuncEnter();

    AssertReturn (aInitiator && aName, E_INVALIDARG);
    AssertReturn (aStateFilePath && aServerProgress, E_POINTER);

    LogFlowThisFunc (("aName='%ls'\n", aName));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* Progress::init() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    AssertReturn ((mData->mMachineState < MachineState_Running ||
                   mData->mMachineState == MachineState_Paused) &&
                  mSnapshotData.mLastState == MachineState_Null &&
                  mSnapshotData.mSnapshot.isNull() &&
                  mSnapshotData.mServerProgress.isNull() &&
                  mSnapshotData.mCombinedProgress.isNull(),
                  E_FAIL);

    bool takingSnapshotOnline = mData->mMachineState == MachineState_Paused;

    if (!takingSnapshotOnline && mData->mMachineState != MachineState_Saved)
    {
        /*
         *  save all current settings to ensure current changes are committed
         *  and hard disks are fixed up
         */
        HRESULT rc = saveSettings();
        CheckComRCReturnRC (rc);
    }

    /* check that there are no Writethrough hard disks attached */
    for (HDData::HDAttachmentList::const_iterator
         it = mHDData->mHDAttachments.begin();
         it != mHDData->mHDAttachments.end();
         ++ it)
    {
        ComObjPtr <HardDisk> hd = (*it)->hardDisk();
        AutoLock hdLock (hd);
        if (hd->type() == HardDiskType_Writethrough)
            return setError (E_FAIL,
                tr ("Cannot take a snapshot when there is a Writethrough hard "
                    " disk attached ('%ls')"), hd->toString().raw());
    }

    AssertReturn (aProgress || !takingSnapshotOnline, E_FAIL);

    /* create an ID for the snapshot */
    Guid snapshotId;
    snapshotId.create();

    Bstr stateFilePath;
    /* stateFilePath is null when the machine is not online nor saved */
    if (takingSnapshotOnline || mData->mMachineState == MachineState_Saved)
        stateFilePath = Utf8StrFmt ("%ls%c{%Vuuid}.sav",
                                    mUserData->mSnapshotFolderFull.raw(),
                                    RTPATH_DELIMITER,
                                    snapshotId.ptr());

    /* ensure the directory for the saved state file exists */
    if (stateFilePath)
    {
        Utf8Str dir = stateFilePath;
        RTPathStripFilename (dir.mutableRaw());
        if (!RTDirExists (dir))
        {
            int vrc = RTDirCreateFullPath (dir, 0777);
            if (VBOX_FAILURE (vrc))
                return setError (E_FAIL,
                    tr ("Could not create a directory '%s' to save the "
                        "VM state to (%Vrc)"),
                    dir.raw(), vrc);
        }
    }

    /* create a snapshot machine object */
    ComObjPtr <SnapshotMachine> snapshotMachine;
    snapshotMachine.createObject();
    HRESULT rc = snapshotMachine->init (this, snapshotId, stateFilePath);
    AssertComRCReturn (rc, rc);

    Bstr progressDesc = Bstr (tr ("Taking snapshot of virtual machine"));
    Bstr firstOpDesc = Bstr (tr ("Preparing to take snapshot"));

    /*
     *  create a server-side progress object (it will be descriptionless
     *  when we need to combine it with the VM-side progress, i.e. when we're
     *  taking a snapshot online). The number of operations is:
     *  1 (preparing) + # of VDIs + 1 (if the state is saved so we need to copy it)
     */
    ComObjPtr <Progress> serverProgress;
    {
        ULONG opCount = 1 + mHDData->mHDAttachments.size();
        if (mData->mMachineState == MachineState_Saved)
            opCount ++;
        serverProgress.createObject();
        if (takingSnapshotOnline)
            rc = serverProgress->init (FALSE, opCount, firstOpDesc);
        else
            rc = serverProgress->init (mParent, aInitiator, progressDesc, FALSE,
                                       opCount, firstOpDesc);
        AssertComRCReturn (rc, rc);
    }

    /* create a combined server-side progress object when necessary */
    ComObjPtr <CombinedProgress> combinedProgress;
    if (takingSnapshotOnline)
    {
        combinedProgress.createObject();
        rc = combinedProgress->init (mParent, aInitiator, progressDesc,
                                     serverProgress, aProgress);
        AssertComRCReturn (rc, rc);
    }

    /* create a snapshot object */
    RTTIMESPEC time;
    ComObjPtr <Snapshot> snapshot;
    snapshot.createObject();
    rc = snapshot->init (snapshotId, aName, aDescription,
                         *RTTimeNow (&time), snapshotMachine,
                         mData->mCurrentSnapshot);
    AssertComRCReturnRC (rc);

    /*
     *  create and start the task on a separate thread
     *  (note that it will not start working until we release alock)
     */
    TakeSnapshotTask *task = new TakeSnapshotTask (this);
    int vrc = RTThreadCreate (NULL, taskHandler,
                              (void *) task,
                              0, RTTHREADTYPE_MAIN_WORKER, 0, "TakeSnapshot");
    if (VBOX_FAILURE (vrc))
    {
        snapshot->uninit();
        delete task;
        ComAssertFailedRet (E_FAIL);
    }

    /* fill in the snapshot data */
    mSnapshotData.mLastState = mData->mMachineState;
    mSnapshotData.mSnapshot = snapshot;
    mSnapshotData.mServerProgress = serverProgress;
    mSnapshotData.mCombinedProgress = combinedProgress;

    /* set the state to Saving (this is expected by Console::TakeSnapshot()) */
    setMachineState (MachineState_Saving);

    if (takingSnapshotOnline)
        stateFilePath.cloneTo (aStateFilePath);
    else
        *aStateFilePath = NULL;

    serverProgress.queryInterfaceTo (aServerProgress);

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  @note Locks mParent + this objects for writing.
 */
STDMETHODIMP SessionMachine::EndTakingSnapshot (BOOL aSuccess)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* Lock mParent because of endTakingSnapshot() */
    AutoMultiWriteLock2 alock (mParent, this);

    AssertReturn (!aSuccess ||
                  (mData->mMachineState == MachineState_Saving &&
                   mSnapshotData.mLastState != MachineState_Null &&
                   !mSnapshotData.mSnapshot.isNull() &&
                   !mSnapshotData.mServerProgress.isNull() &&
                   !mSnapshotData.mCombinedProgress.isNull()),
                  E_FAIL);

    /*
     *  set the state to the state we had when BeginTakingSnapshot() was called
     *  (this is expected by Console::TakeSnapshot() and
     *  Console::saveStateThread())
     */
    setMachineState (mSnapshotData.mLastState);

    return endTakingSnapshot (aSuccess);
}

/**
 *  @note Locks mParent + this + children objects for writing!
 */
STDMETHODIMP SessionMachine::DiscardSnapshot (
    IConsole *aInitiator, INPTR GUIDPARAM aId,
    MachineState_T *aMachineState, IProgress **aProgress)
{
    LogFlowThisFunc (("\n"));

    Guid id = aId;
    AssertReturn (aInitiator && !id.isEmpty(), E_INVALIDARG);
    AssertReturn (aMachineState && aProgress, E_POINTER);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* Progress::init() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    ComAssertRet (mData->mMachineState < MachineState_Running, E_FAIL);

    ComObjPtr <Snapshot> snapshot;
    HRESULT rc = findSnapshot (id, snapshot, true /* aSetError */);
    CheckComRCReturnRC (rc);

    AutoLock snapshotLock (snapshot);
    if (snapshot == mData->mFirstSnapshot)
    {
        AutoLock chLock (mData->mFirstSnapshot->childrenLock());
        size_t childrenCount = mData->mFirstSnapshot->children().size();
        if (childrenCount > 1)
            return setError (E_FAIL,
                tr ("Cannot discard the snapshot '%ls' because it is the first "
                    "snapshot of the machine '%ls' and it has more than one "
                    "child snapshot (%d)"),
                snapshot->data().mName.raw(), mUserData->mName.raw(),
                childrenCount);
    }

    /*
     *  If the snapshot being discarded is the current one, ensure current
     *  settings are committed and saved.
     */
    if (snapshot == mData->mCurrentSnapshot)
    {
        if (isModified())
        {
            rc = saveSettings();
            CheckComRCReturnRC (rc);
        }
    }

    /*
     *  create a progress object. The number of operations is:
     *    1 (preparing) + # of VDIs
     */
    ComObjPtr <Progress> progress;
    progress.createObject();
    rc = progress->init (mParent, aInitiator,
                         Bstr (Utf8StrFmt (tr ("Discarding snapshot '%ls'"),
                             snapshot->data().mName.raw())),
                         FALSE /* aCancelable */,
                         1 + snapshot->data().mMachine->mHDData->mHDAttachments.size(),
                         Bstr (tr ("Preparing to discard snapshot")));
    AssertComRCReturn (rc, rc);

    /* create and start the task on a separate thread */
    DiscardSnapshotTask *task = new DiscardSnapshotTask (this, progress, snapshot);
    int vrc = RTThreadCreate (NULL, taskHandler,
                              (void *) task,
                              0, RTTHREADTYPE_MAIN_WORKER, 0, "DiscardSnapshot");
    if (VBOX_FAILURE (vrc))
        delete task;
    ComAssertRCRet (vrc, E_FAIL);

    /* set the proper machine state (note: after creating a Task instance) */
    setMachineState (MachineState_Discarding);

    /* return the progress to the caller */
    progress.queryInterfaceTo (aProgress);

    /* return the new state to the caller */
    *aMachineState = mData->mMachineState;

    return S_OK;
}

/**
 *  @note Locks mParent + this + children objects for writing!
 */
STDMETHODIMP SessionMachine::DiscardCurrentState (
    IConsole *aInitiator, MachineState_T *aMachineState, IProgress **aProgress)
{
    LogFlowThisFunc (("\n"));

    AssertReturn (aInitiator, E_INVALIDARG);
    AssertReturn (aMachineState && aProgress, E_POINTER);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* Progress::init() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    ComAssertRet (mData->mMachineState < MachineState_Running, E_FAIL);

    if (mData->mCurrentSnapshot.isNull())
        return setError (E_FAIL,
            tr ("Could not discard the current state of the machine '%ls' "
                "because it doesn't have any snapshots"),
            mUserData->mName.raw());

    /*
     *  create a progress object. The number of operations is:
     *    1 (preparing) + # of VDIs + 1 (if we need to copy the saved state file)
     */
    ComObjPtr <Progress> progress;
    progress.createObject();
    {
        ULONG opCount = 1 + mData->mCurrentSnapshot->data()
                                .mMachine->mHDData->mHDAttachments.size();
        if (mData->mCurrentSnapshot->stateFilePath())
            ++ opCount;
        progress->init (mParent, aInitiator,
                        Bstr (tr ("Discarding current machine state")),
                        FALSE /* aCancelable */, opCount,
                        Bstr (tr ("Preparing to discard current state")));
    }

    /* create and start the task on a separate thread */
    DiscardCurrentStateTask *task =
        new DiscardCurrentStateTask (this, progress, false /* discardCurSnapshot */);
    int vrc = RTThreadCreate (NULL, taskHandler,
                              (void *) task,
                              0, RTTHREADTYPE_MAIN_WORKER, 0, "DiscardCurState");
    if (VBOX_FAILURE (vrc))
        delete task;
    ComAssertRCRet (vrc, E_FAIL);

    /* set the proper machine state (note: after creating a Task instance) */
    setMachineState (MachineState_Discarding);

    /* return the progress to the caller */
    progress.queryInterfaceTo (aProgress);

    /* return the new state to the caller */
    *aMachineState = mData->mMachineState;

    return S_OK;
}

/**
 *  @note Locks mParent + other objects for writing!
 */
STDMETHODIMP SessionMachine::DiscardCurrentSnapshotAndState (
    IConsole *aInitiator, MachineState_T *aMachineState, IProgress **aProgress)
{
    LogFlowThisFunc (("\n"));

    AssertReturn (aInitiator, E_INVALIDARG);
    AssertReturn (aMachineState && aProgress, E_POINTER);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* Progress::init() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    ComAssertRet (mData->mMachineState < MachineState_Running, E_FAIL);

    if (mData->mCurrentSnapshot.isNull())
        return setError (E_FAIL,
            tr ("Could not discard the current state of the machine '%ls' "
                "because it doesn't have any snapshots"),
            mUserData->mName.raw());

    /*
     *  create a progress object. The number of operations is:
     *    1 (preparing) + # of VDIs in the current snapshot +
     *    # of VDIs in the previous snapshot +
     *    1 (if we need to copy the saved state file of the previous snapshot)
     *  or (if there is no previous snapshot):
     *    1 (preparing) + # of VDIs in the current snapshot * 2 +
     *    1 (if we need to copy the saved state file of the current snapshot)
     */
    ComObjPtr <Progress> progress;
    progress.createObject();
    {
        ComObjPtr <Snapshot> curSnapshot = mData->mCurrentSnapshot;
        ComObjPtr <Snapshot> prevSnapshot = mData->mCurrentSnapshot->parent();

        ULONG opCount = 1;
        if (prevSnapshot)
        {
            opCount += curSnapshot->data().mMachine->mHDData->mHDAttachments.size();
            opCount += prevSnapshot->data().mMachine->mHDData->mHDAttachments.size();
            if (prevSnapshot->stateFilePath())
                ++ opCount;
        }
        else
        {
            opCount += curSnapshot->data().mMachine->mHDData->mHDAttachments.size() * 2;
            if (curSnapshot->stateFilePath())
                ++ opCount;
        }

        progress->init (mParent, aInitiator,
                        Bstr (tr ("Discarding current machine snapshot and state")),
                        FALSE /* aCancelable */, opCount,
                        Bstr (tr ("Preparing to discard current snapshot and state")));
    }

    /* create and start the task on a separate thread */
    DiscardCurrentStateTask *task =
        new DiscardCurrentStateTask (this, progress, true /* discardCurSnapshot */);
    int vrc = RTThreadCreate (NULL, taskHandler,
                              (void *) task,
                              0, RTTHREADTYPE_MAIN_WORKER, 0, "DiscardCurState");
    if (VBOX_FAILURE (vrc))
        delete task;
    ComAssertRCRet (vrc, E_FAIL);

    /* set the proper machine state (note: after creating a Task instance) */
    setMachineState (MachineState_Discarding);

    /* return the progress to the caller */
    progress.queryInterfaceTo (aProgress);

    /* return the new state to the caller */
    *aMachineState = mData->mMachineState;

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  Called from the client watcher thread to check for unexpected client
 *  process death.
 *
 *  @note On Win32 and on OS/2, this method is called only when we've got the
 *  mutex (i.e. the client has either died or terminated normally). This
 *  method always returns true.
 *
 *  @note On Linux, the method returns true if the client process has
 *  terminated abnormally (and/or the session has been uninitialized) and
 *  false if it is still alive.
 *
 *  @note Locks this object for writing.
 */
bool SessionMachine::checkForDeath()
{
    Uninit::Reason reason;
    bool doUninit = false;
    bool ret = false;

    /*
     *  Enclose autoCaller with a block because calling uninit()
     *  from under it will deadlock.
     */
    {
        AutoCaller autoCaller (this);
        if (!autoCaller.isOk())
        {
            /*
             *  return true if not ready, to cause the client watcher to exclude
             *  the corresponding session from watching
             */
            LogFlowThisFunc (("Already uninitialized!"));
            return true;
        }

        AutoLock alock (this);

        /*
         *  Determine the reason of death: if the session state is Closing here,
         *  everything is fine. Otherwise it means that the client did not call
         *  OnSessionEnd() before it released the IPC semaphore.
         *  This may happen either because the client process has abnormally
         *  terminated, or because it simply forgot to call ISession::Close()
         *  before exiting. We threat the latter also as an abnormal termination
         *  (see Session::uninit() for details).
         */
        reason = mData->mSession.mState == SessionState_Closing ?
                 Uninit::Normal :
                 Uninit::Abnormal;

#if defined(RT_OS_WINDOWS)

        AssertMsg (mIPCSem, ("semaphore must be created"));

        /* release the IPC mutex */
        ::ReleaseMutex (mIPCSem);

        doUninit = true;

        ret = true;

#elif defined(RT_OS_OS2)

        AssertMsg (mIPCSem, ("semaphore must be created"));

        /* release the IPC mutex */
        ::DosReleaseMutexSem (mIPCSem);

        doUninit = true;

        ret = true;

#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)

        AssertMsg (mIPCSem >= 0, ("semaphore must be created"));

        int val = ::semctl (mIPCSem, 0, GETVAL);
        if (val > 0)
        {
            /* the semaphore is signaled, meaning the session is terminated */
            doUninit = true;
        }

        ret = val > 0;

#else
# error "Port me!"
#endif

    } /* AutoCaller block */

    if (doUninit)
        uninit (reason);

    return ret;
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onDVDDriveChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnDVDDriveChange();
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onFloppyDriveChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnFloppyDriveChange();
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onNetworkAdapterChange(INetworkAdapter *networkAdapter)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnNetworkAdapterChange(networkAdapter);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onSerialPortChange(ISerialPort *serialPort)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnSerialPortChange(serialPort);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onParallelPortChange(IParallelPort *parallelPort)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnParallelPortChange(parallelPort);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onVRDPServerChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnVRDPServerChange();
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onUSBControllerChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnUSBControllerChange();
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onSharedFolderChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnSharedFolderChange (FALSE /* aGlobal */);
}

/**
 *  Returns @c true if this machine's USB controller reports it has a matching
 *  filter for the given USB device and @c false otherwise.
 *
 *  @note Locks this object for reading.
 */
bool SessionMachine::hasMatchingUSBFilter (const ComObjPtr <HostUSBDevice> &aDevice, ULONG *aMaskedIfs)
{
    AutoCaller autoCaller (this);
    /* silently return if not ready -- this method may be called after the
     * direct machine session has been called */
    if (!autoCaller.isOk())
        return false;

    AutoReaderLock alock (this);

#ifdef VBOX_WITH_USB
    return mUSBController->hasMatchingFilter (aDevice, aMaskedIfs);
#else
    return false;
#endif
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onUSBDeviceAttach (IUSBDevice *aDevice,
                                           IVirtualBoxErrorInfo *aError,
                                           ULONG aMaskedIfs)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);

    /* This notification may happen after the machine object has been
     * uninitialized (the session was closed), so don't assert. */
    CheckComRCReturnRC (autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* fail on notifications sent after #OnSessionEnd() is called, it is
     * expected by the caller */
    if (!directControl)
        return E_FAIL;

    return directControl->OnUSBDeviceAttach (aDevice, aError, aMaskedIfs);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onUSBDeviceDetach (INPTR GUIDPARAM aId,
                                           IVirtualBoxErrorInfo *aError)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);

    /* This notification may happen after the machine object has been
     * uninitialized (the session was closed), so don't assert. */
    CheckComRCReturnRC (autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* fail on notifications sent after #OnSessionEnd() is called, it is
     * expected by the caller */
    if (!directControl)
        return E_FAIL;

    return directControl->OnUSBDeviceDetach (aId, aError);
}

// protected methods
/////////////////////////////////////////////////////////////////////////////

/**
 *  Helper method to finalize saving the state.
 *
 *  @note Must be called from under this object's lock.
 *
 *  @param aSuccess TRUE if the snapshot has been taken successfully
 *
 *  @note Locks mParent + this objects for writing.
 */
HRESULT SessionMachine::endSavingState (BOOL aSuccess)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* mParent->removeProgress() and saveSettings() need mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = S_OK;

    if (aSuccess)
    {
        mSSData->mStateFilePath = mSnapshotData.mStateFilePath;

        /* save all VM settings */
        rc = saveSettings();
    }
    else
    {
        /* delete the saved state file (it might have been already created) */
        RTFileDelete (Utf8Str (mSnapshotData.mStateFilePath));
    }

    /* remove the completed progress object */
    mParent->removeProgress (mSnapshotData.mProgressId);

    /* clear out the temporary saved state data */
    mSnapshotData.mLastState = MachineState_Null;
    mSnapshotData.mProgressId.clear();
    mSnapshotData.mStateFilePath.setNull();

    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  Helper method to finalize taking a snapshot.
 *  Gets called only from #EndTakingSnapshot() that is expected to
 *  be called by the VM process when it finishes *all* the tasks related to
 *  taking a snapshot, either scucessfully or unsuccessfilly.
 *
 *  @param aSuccess TRUE if the snapshot has been taken successfully
 *
 *  @note Locks mParent + this objects for writing.
 */
HRESULT SessionMachine::endTakingSnapshot (BOOL aSuccess)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* Progress object uninitialization needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = S_OK;

    if (aSuccess)
    {
        /* the server progress must be completed on success */
        Assert (mSnapshotData.mServerProgress->completed());

        mData->mCurrentSnapshot = mSnapshotData.mSnapshot;
        /* memorize the first snapshot if necessary */
        if (!mData->mFirstSnapshot)
            mData->mFirstSnapshot = mData->mCurrentSnapshot;

        int opFlags = SaveSS_AddOp | SaveSS_UpdateCurrentId;
        if (mSnapshotData.mLastState != MachineState_Paused && !isModified())
        {
            /*
             *  the machine was powered off or saved when taking a snapshot,
             *  so reset the mCurrentStateModified flag
             */
            mData->mCurrentStateModified = FALSE;
            opFlags |= SaveSS_UpdateCurStateModified;
        }

        rc = saveSnapshotSettings (mSnapshotData.mSnapshot, opFlags);
    }

    if (!aSuccess || FAILED (rc))
    {
        if (mSnapshotData.mSnapshot)
        {
            /* wait for the completion of the server progress (diff VDI creation) */
            /// @todo (dmik) later, we will definitely want to cancel it instead
            // (when the cancel function is implemented)
            mSnapshotData.mServerProgress->WaitForCompletion (-1);

            /*
             *  delete all differencing VDIs created
             *  (this will attach their parents back)
             */
            rc = deleteSnapshotDiffs (mSnapshotData.mSnapshot);
            /* continue cleanup on error */

            /* delete the saved state file (it might have been already created) */
            if (mSnapshotData.mSnapshot->stateFilePath())
                RTFileDelete (Utf8Str (mSnapshotData.mSnapshot->stateFilePath()));

            mSnapshotData.mSnapshot->uninit();
        }
    }

    /* inform callbacks */
    if (aSuccess && SUCCEEDED (rc))
        mParent->onSnapshotTaken (mData->mUuid, mSnapshotData.mSnapshot->data().mId);

    /* clear out the snapshot data */
    mSnapshotData.mLastState = MachineState_Null;
    mSnapshotData.mSnapshot.setNull();
    mSnapshotData.mServerProgress.setNull();
    /* uninitialize the combined progress (to remove it from the VBox collection) */
    if (!mSnapshotData.mCombinedProgress.isNull())
    {
        mSnapshotData.mCombinedProgress->uninit();
        mSnapshotData.mCombinedProgress.setNull();
    }

    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  Take snapshot task handler.
 *  Must be called only by TakeSnapshotTask::handler()!
 *
 *  The sole purpose of this task is to asynchronously create differencing VDIs
 *  and copy the saved state file (when necessary). The VM process will wait
 *  for this task to complete using the mSnapshotData.mServerProgress
 *  returned to it.
 *
 *  @note Locks mParent + this objects for writing.
 */
void SessionMachine::takeSnapshotHandler (TakeSnapshotTask &aTask)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);

    LogFlowThisFunc (("state=%d\n", autoCaller.state()));
    if (!autoCaller.isOk())
    {
        /*
         *  we might have been uninitialized because the session was
         *  accidentally closed by the client, so don't assert
         */
        LogFlowThisFuncLeave();
        return;
    }

    /* endTakingSnapshot() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = S_OK;

    LogFlowThisFunc (("Creating differencing VDIs...\n"));

    /* create new differencing hard disks and attach them to this machine */
    rc = createSnapshotDiffs (&mSnapshotData.mSnapshot->data().mId,
                              mUserData->mSnapshotFolderFull,
                              mSnapshotData.mServerProgress,
                              true /* aOnline */);

    if (SUCCEEDED (rc) && mSnapshotData.mLastState == MachineState_Saved)
    {
        Utf8Str stateFrom = mSSData->mStateFilePath;
        Utf8Str stateTo = mSnapshotData.mSnapshot->stateFilePath();

        LogFlowThisFunc (("Copying the execution state from '%s' to '%s'...\n",
                          stateFrom.raw(), stateTo.raw()));

        mSnapshotData.mServerProgress->advanceOperation (
            Bstr (tr ("Copying the execution state")));

        /*
         *  We can safely leave the lock here:
         *  mMachineState is MachineState_Saving here
         */
        alock.leave();

        /* copy the state file */
        int vrc = RTFileCopyEx (stateFrom, stateTo, 0, progressCallback,
                                static_cast <Progress *> (mSnapshotData.mServerProgress));

        alock.enter();

        if (VBOX_FAILURE (vrc))
            rc = setError (E_FAIL,
                tr ("Could not copy the state file '%ls' to '%ls' (%Vrc)"),
                    stateFrom.raw(), stateTo.raw());
    }

    /*
     *  we have to call endTakingSnapshot() here if the snapshot was taken
     *  offline, because the VM process will not do it in this case
     */
    if (mSnapshotData.mLastState != MachineState_Paused)
    {
        LogFlowThisFunc (("Finalizing the taken snapshot (rc=%08X)...\n", rc));

        setMachineState (mSnapshotData.mLastState);
        updateMachineStateOnClient();

        /* finalize the progress after setting the state, for consistency */
        mSnapshotData.mServerProgress->notifyComplete (rc);

        endTakingSnapshot (SUCCEEDED (rc));
    }
    else
    {
        mSnapshotData.mServerProgress->notifyComplete (rc);
    }

    LogFlowThisFuncLeave();
}

/**
 *  Discard snapshot task handler.
 *  Must be called only by DiscardSnapshotTask::handler()!
 *
 *  When aTask.subTask is true, the associated progress object is left
 *  uncompleted on success. On failure, the progress is marked as completed
 *  regardless of this parameter.
 *
 *  @note Locks mParent + this + child objects for writing!
 */
void SessionMachine::discardSnapshotHandler (DiscardSnapshotTask &aTask)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);

    LogFlowThisFunc (("state=%d\n", autoCaller.state()));
    if (!autoCaller.isOk())
    {
        /*
         *  we might have been uninitialized because the session was
         *  accidentally closed by the client, so don't assert
         */
        aTask.progress->notifyComplete (
            E_FAIL, COM_IIDOF (IMachine), getComponentName(),
            tr ("The session has been accidentally closed"));

        LogFlowThisFuncLeave();
        return;
    }

    /* Progress::notifyComplete() et al., saveSettings() need mParent lock.
     * Also safely lock the snapshot stuff in the direction parent->child */
    AutoMultiWriteLock4 alock (mParent->lockHandle(), this->lockHandle(),
                               aTask.snapshot->lockHandle(),
                               aTask.snapshot->childrenLock());

    ComObjPtr <SnapshotMachine> sm = aTask.snapshot->data().mMachine;
    /* no need to lock the snapshot machine since it is const by definiton */

    HRESULT rc = S_OK;

    /* save the snapshot ID (for callbacks) */
    Guid snapshotId = aTask.snapshot->data().mId;

    do
    {
        /* first pass: */
        LogFlowThisFunc (("Check hard disk accessibility and affected machines...\n"));

        HDData::HDAttachmentList::const_iterator it;
        for (it = sm->mHDData->mHDAttachments.begin();
             it != sm->mHDData->mHDAttachments.end();
             ++ it)
        {
            ComObjPtr <HardDiskAttachment> hda = *it;
            ComObjPtr <HardDisk> hd = hda->hardDisk();
            ComObjPtr <HardDisk> parent = hd->parent();

            AutoLock hdLock (hd);

            if (hd->hasForeignChildren())
            {
                rc = setError (E_FAIL,
                    tr ("One or more hard disks belonging to other machines are "
                        "based on the hard disk '%ls' stored in the snapshot '%ls'"),
                    hd->toString().raw(), aTask.snapshot->data().mName.raw());
                break;
            }

            if (hd->type() == HardDiskType_Normal)
            {
                AutoLock hdChildrenLock (hd->childrenLock());
                size_t childrenCount = hd->children().size();
                if (childrenCount > 1)
                {
                    rc = setError (E_FAIL,
                        tr ("Normal hard disk '%ls' stored in the snapshot '%ls' "
                            "has more than one child hard disk (%d)"),
                        hd->toString().raw(), aTask.snapshot->data().mName.raw(),
                        childrenCount);
                    break;
                }
            }
            else
            {
                ComAssertMsgFailedBreak (("Invalid hard disk type %d\n", hd->type()),
                                         rc = E_FAIL);
            }

            Bstr accessError;
            rc = hd->getAccessibleWithChildren (accessError);
            CheckComRCBreakRC (rc);

            if (!accessError.isNull())
            {
                rc = setError (E_FAIL,
                    tr ("Hard disk '%ls' stored in the snapshot '%ls' is not "
                        "accessible (%ls)"),
                    hd->toString().raw(), aTask.snapshot->data().mName.raw(),
                    accessError.raw());
                break;
            }

            rc = hd->setBusyWithChildren();
            if (FAILED (rc))
            {
                /* reset the busy flag of all previous hard disks */
                while (it != sm->mHDData->mHDAttachments.begin())
                    (*(-- it))->hardDisk()->clearBusyWithChildren();
                break;
            }
        }

        CheckComRCBreakRC (rc);

        /* second pass: */
        LogFlowThisFunc (("Performing actual vdi merging...\n"));

        for (it = sm->mHDData->mHDAttachments.begin();
             it != sm->mHDData->mHDAttachments.end();
             ++ it)
        {
            ComObjPtr <HardDiskAttachment> hda = *it;
            ComObjPtr <HardDisk> hd = hda->hardDisk();
            ComObjPtr <HardDisk> parent = hd->parent();

            AutoLock hdLock (hd);

            Bstr hdRootString = hd->root()->toString (true /* aShort */);

            if (parent)
            {
                if (hd->isParentImmutable())
                {
                    aTask.progress->advanceOperation (Bstr (Utf8StrFmt (
                        tr ("Discarding changes to immutable hard disk '%ls'"),
                        hdRootString.raw())));

                    /* clear the busy flag before unregistering */
                    hd->clearBusy();

                    /*
                     *  unregisterDiffHardDisk() is supposed to delete and uninit
                     *  the differencing hard disk
                     */
                    rc = mParent->unregisterDiffHardDisk (hd);
                    CheckComRCBreakRC (rc);
                    continue;
                }
                else
                {
                    /*
                     *  differencing VDI:
                     *  merge this image to all its children
                     */

                    aTask.progress->advanceOperation (Bstr (Utf8StrFmt (
                        tr ("Merging changes to normal hard disk '%ls' to children"),
                        hdRootString.raw())));

                    alock.leave();

                    rc = hd->asVDI()->mergeImageToChildren (aTask.progress);

                    alock.enter();

                    // debug code
                    // if (it != sm->mHDData->mHDAttachments.begin())
                    // {
                    //    rc = setError (E_FAIL, "Simulated failure");
                    //    break;
                    //}

                    if (SUCCEEDED (rc))
                        rc = mParent->unregisterDiffHardDisk (hd);
                    else
                        hd->clearBusyWithChildren();

                    CheckComRCBreakRC (rc);
                }
            }
            else if (hd->type() == HardDiskType_Normal)
            {
                /*
                 *  normal vdi has the only child or none
                 *  (checked in the first pass)
                 */

                ComObjPtr <HardDisk> child;
                {
                    AutoLock hdChildrenLock (hd->childrenLock());
                    if (hd->children().size())
                        child = hd->children().front();
                }

                if (child.isNull())
                {
                    aTask.progress->advanceOperation (Bstr (Utf8StrFmt (
                        tr ("Detaching normal hard disk '%ls'"),
                        hdRootString.raw())));

                    /* just deassociate the normal image from this machine */
                    hd->setMachineId (Guid());
                    hd->setSnapshotId (Guid());

                    /* clear the busy flag */
                    hd->clearBusy();
                }
                else
                {
                    AutoLock childLock (child);

                    aTask.progress->advanceOperation (Bstr (Utf8StrFmt (
                        tr ("Preserving changes to normal hard disk '%ls'"),
                        hdRootString.raw())));

                    ComObjPtr <Machine> cm;
                    ComObjPtr <Snapshot> cs;
                    ComObjPtr <HardDiskAttachment> childHda;
                    rc = findHardDiskAttachment (child, &cm, &cs, &childHda);
                    CheckComRCBreakRC (rc);
                    /* must be the same machine (checked in the first pass) */
                    ComAssertBreak (cm->mData->mUuid == mData->mUuid, rc = E_FAIL);

                    /* merge the child to this basic image */

                    alock.leave();

                    rc = child->asVDI()->mergeImageToParent (aTask.progress);

                    alock.enter();

                    if (SUCCEEDED (rc))
                        rc = mParent->unregisterDiffHardDisk (child);
                    else
                        hd->clearBusyWithChildren();

                    CheckComRCBreakRC (rc);

                    /* reset the snapshot Id  */
                    hd->setSnapshotId (Guid());

                    /* replace the child image in the appropriate place */
                    childHda->updateHardDisk (hd, FALSE /* aDirty */);

                    if (!cs)
                    {
                        aTask.settingsChanged = true;
                    }
                    else
                    {
                        rc = cm->saveSnapshotSettings (cs, SaveSS_UpdateAllOp);
                        CheckComRCBreakRC (rc);
                    }
                }
            }
            else
            {
                ComAssertMsgFailedBreak (("Invalid hard disk type %d\n", hd->type()),
                                         rc = E_FAIL);
            }
        }

        /* preserve existing error info */
        ErrorInfoKeeper mergeEik;
        HRESULT mergeRc = rc;

        if (FAILED (rc))
        {
            /* clear the busy flag on the rest of hard disks */
            for (++ it; it != sm->mHDData->mHDAttachments.end(); ++ it)
                (*it)->hardDisk()->clearBusyWithChildren();
        }

        /*
         *  we have to try to discard the snapshot even if merging failed
         *  because some images might have been already merged (and deleted)
         */

        do
        {
            LogFlowThisFunc (("Discarding the snapshot (reparenting children)...\n"));

            /* It is important to uninitialize and delete all snapshot's hard
             * disk attachments as they are no longer valid -- otherwise the
             * code in Machine::uninitDataAndChildObjects() will mistakenly
             * perform hard disk deassociation. */
            for (HDData::HDAttachmentList::iterator it = sm->mHDData->mHDAttachments.begin();
                 it != sm->mHDData->mHDAttachments.end();)
            {
                (*it)->uninit();
                it = sm->mHDData->mHDAttachments.erase (it);
            }

            ComObjPtr <Snapshot> parentSnapshot = aTask.snapshot->parent();

            /// @todo (dmik):
            //  when we introduce clones later, discarding the snapshot
            //  will affect the current and first snapshots of clones, if they are
            //  direct children of this snapshot. So we will need to lock machines
            //  associated with child snapshots as well and update mCurrentSnapshot
            //  and/or mFirstSnapshot fields.

            if (aTask.snapshot == mData->mCurrentSnapshot)
            {
                /* currently, the parent snapshot must refer to the same machine */
                ComAssertBreak (
                    !parentSnapshot ||
                    parentSnapshot->data().mMachine->mData->mUuid == mData->mUuid,
                    rc = E_FAIL);
                mData->mCurrentSnapshot = parentSnapshot;
                /* mark the current state as modified */
                mData->mCurrentStateModified = TRUE;
            }

            if (aTask.snapshot == mData->mFirstSnapshot)
            {
                /*
                 *  the first snapshot must have only one child when discarded,
                 *  or no children at all
                 */
                ComAssertBreak (aTask.snapshot->children().size() <= 1, rc = E_FAIL);

                if (aTask.snapshot->children().size() == 1)
                {
                    ComObjPtr <Snapshot> childSnapshot = aTask.snapshot->children().front();
                    ComAssertBreak (
                        childSnapshot->data().mMachine->mData->mUuid == mData->mUuid,
                        rc = E_FAIL);
                    mData->mFirstSnapshot = childSnapshot;
                }
                else
                    mData->mFirstSnapshot.setNull();
            }

            /// @todo (dmik)
            //  if we implement some warning mechanism later, we'll have
            //  to return a warning if the state file path cannot be deleted
            Bstr stateFilePath = aTask.snapshot->stateFilePath();
            if (stateFilePath)
                RTFileDelete (Utf8Str (stateFilePath));

            aTask.snapshot->discard();

            rc = saveSnapshotSettings (parentSnapshot,
                                       SaveSS_UpdateAllOp | SaveSS_UpdateCurrentId);
        }
        while (0);

        /* restore the merge error if any (ErrorInfo will be restored
         * automatically) */
        if (FAILED (mergeRc))
            rc = mergeRc;
    }
    while (0);

    if (!aTask.subTask || FAILED (rc))
    {
        if (!aTask.subTask)
        {
            /* preserve existing error info */
            ErrorInfoKeeper eik;

            /* restore the machine state */
            setMachineState (aTask.state);
            updateMachineStateOnClient();

            /*
             *  save settings anyway, since we've already changed the current
             *  machine configuration
             */
            if (aTask.settingsChanged)
            {
                saveSettings (true /* aMarkCurStateAsModified */,
                              true /* aInformCallbacksAnyway */);
            }
        }

        /* set the result (this will try to fetch current error info on failure) */
        aTask.progress->notifyComplete (rc);
    }

    if (SUCCEEDED (rc))
        mParent->onSnapshotDiscarded (mData->mUuid, snapshotId);

    LogFlowThisFunc (("Done discarding snapshot (rc=%08X)\n", rc));
    LogFlowThisFuncLeave();
}

/**
 *  Discard current state task handler.
 *  Must be called only by DiscardCurrentStateTask::handler()!
 *
 *  @note Locks mParent + this object for writing.
 */
void SessionMachine::discardCurrentStateHandler (DiscardCurrentStateTask &aTask)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);

    LogFlowThisFunc (("state=%d\n", autoCaller.state()));
    if (!autoCaller.isOk())
    {
        /*
         *  we might have been uninitialized because the session was
         *  accidentally closed by the client, so don't assert
         */
        aTask.progress->notifyComplete (
            E_FAIL, COM_IIDOF (IMachine), getComponentName(),
            tr ("The session has been accidentally closed"));

        LogFlowThisFuncLeave();
        return;
    }

    /* Progress::notifyComplete() et al., saveSettings() need mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    /*
     *  discard all current changes to mUserData (name, OSType etc.)
     *  (note that the machine is powered off, so there is no need
     *  to inform the direct session)
     */
    if (isModified())
        rollback (false /* aNotify */);

    HRESULT rc = S_OK;

    bool errorInSubtask = false;
    bool stateRestored = false;

    const bool isLastSnapshot = mData->mCurrentSnapshot->parent().isNull();

    do
    {
        /*
         *  discard the saved state file if the machine was Saved prior
         *  to this operation
         */
        if (aTask.state == MachineState_Saved)
        {
            Assert (!mSSData->mStateFilePath.isEmpty());
            RTFileDelete (Utf8Str (mSSData->mStateFilePath));
            mSSData->mStateFilePath.setNull();
            aTask.modifyLastState (MachineState_PoweredOff);
            rc = saveStateSettings (SaveSTS_StateFilePath);
            CheckComRCBreakRC (rc);
        }

        if (aTask.discardCurrentSnapshot && !isLastSnapshot)
        {
            /*
             *  the "discard current snapshot and state" task is in action,
             *  the current snapshot is not the last one.
             *  Discard the current snapshot first.
             */

            DiscardSnapshotTask subTask (aTask, mData->mCurrentSnapshot);
            subTask.subTask = true;
            discardSnapshotHandler (subTask);
            aTask.settingsChanged = subTask.settingsChanged;
            if (aTask.progress->completed())
            {
                /*
                 *  the progress can be completed by a subtask only if there was
                 *  a failure
                 */
                Assert (FAILED (aTask.progress->resultCode()));
                errorInSubtask = true;
                rc = aTask.progress->resultCode();
                break;
            }
        }

        RTTIMESPEC snapshotTimeStamp;
        RTTimeSpecSetMilli (&snapshotTimeStamp, 0);

        {
            ComObjPtr <Snapshot> curSnapshot = mData->mCurrentSnapshot;
            AutoLock snapshotLock (curSnapshot);

            /* remember the timestamp of the snapshot we're restoring from */
            snapshotTimeStamp = curSnapshot->data().mTimeStamp;

            /* copy all hardware data from the current snapshot */
            copyFrom (curSnapshot->data().mMachine);

            LogFlowThisFunc (("Restoring VDIs from the snapshot...\n"));

            /* restore the attachmends from the snapshot */
            mHDData.backup();
            mHDData->mHDAttachments =
                curSnapshot->data().mMachine->mHDData->mHDAttachments;

            snapshotLock.leave();
            alock.leave();
            rc = createSnapshotDiffs (NULL, mUserData->mSnapshotFolderFull,
                                      aTask.progress,
                                      false /* aOnline */);
            alock.enter();
            snapshotLock.enter();

            if (FAILED (rc))
            {
                /* here we can still safely rollback, so do it */
                /* preserve existing error info */
                ErrorInfoKeeper eik;
                /* undo all changes */
                rollback (false /* aNotify */);
                break;
            }

            /*
             *  note: old VDIs will be deassociated/deleted on #commit() called
             *  either from #saveSettings() or directly at the end
             */

            /* should not have a saved state file associated at this point */
            Assert (mSSData->mStateFilePath.isNull());

            if (curSnapshot->stateFilePath())
            {
                Utf8Str snapStateFilePath = curSnapshot->stateFilePath();

                Utf8Str stateFilePath = Utf8StrFmt ("%ls%c{%Vuuid}.sav",
                    mUserData->mSnapshotFolderFull.raw(),
                    RTPATH_DELIMITER, mData->mUuid.raw());

                LogFlowThisFunc (("Copying saved state file from '%s' to '%s'...\n",
                                  snapStateFilePath.raw(), stateFilePath.raw()));

                aTask.progress->advanceOperation (
                    Bstr (tr ("Restoring the execution state")));

                /* copy the state file */
                snapshotLock.leave();
                alock.leave();
                int vrc = RTFileCopyEx (snapStateFilePath, stateFilePath,
                                        0, progressCallback, aTask.progress);
                alock.enter();
                snapshotLock.enter();

                if (VBOX_SUCCESS (vrc))
                {
                    mSSData->mStateFilePath = stateFilePath;
                }
                else
                {
                    rc = setError (E_FAIL,
                        tr ("Could not copy the state file '%s' to '%s' (%Vrc)"),
                        snapStateFilePath.raw(), stateFilePath.raw(), vrc);
                    break;
                }
            }
        }

        bool informCallbacks = false;

        if (aTask.discardCurrentSnapshot && isLastSnapshot)
        {
            /*
             *  discard the current snapshot and state task is in action,
             *  the current snapshot is the last one.
             *  Discard the current snapshot after discarding the current state.
             */

            /* commit changes to fixup hard disks before discarding */
            rc = commit();
            if (SUCCEEDED (rc))
            {
                DiscardSnapshotTask subTask (aTask, mData->mCurrentSnapshot);
                subTask.subTask = true;
                discardSnapshotHandler (subTask);
                aTask.settingsChanged = subTask.settingsChanged;
                if (aTask.progress->completed())
                {
                    /*
                     *  the progress can be completed by a subtask only if there
                     *  was a failure
                     */
                    Assert (FAILED (aTask.progress->resultCode()));
                    errorInSubtask = true;
                    rc = aTask.progress->resultCode();
                }
            }

            /*
             *  we've committed already, so inform callbacks anyway to ensure
             *  they don't miss some change
             */
            informCallbacks = true;
        }

        /*
         *  we have already discarded the current state, so set the
         *  execution state accordingly no matter of the discard snapshot result
         */
        if (mSSData->mStateFilePath)
            setMachineState (MachineState_Saved);
        else
            setMachineState (MachineState_PoweredOff);

        updateMachineStateOnClient();
        stateRestored = true;

        if (errorInSubtask)
            break;

        /* assign the timestamp from the snapshot */
        Assert (RTTimeSpecGetMilli (&snapshotTimeStamp) != 0);
        mData->mLastStateChange = snapshotTimeStamp;

        /* mark the current state as not modified */
        mData->mCurrentStateModified = FALSE;

        /* save all settings and commit */
        rc = saveSettings (false /* aMarkCurStateAsModified */,
                           informCallbacks);
        aTask.settingsChanged = false;
    }
    while (0);

    if (FAILED (rc))
    {
        /* preserve existing error info */
        ErrorInfoKeeper eik;

        if (!stateRestored)
        {
            /* restore the machine state */
            setMachineState (aTask.state);
            updateMachineStateOnClient();
        }

        /*
         *  save all settings and commit if still modified (there is no way to
         *  rollback properly). Note that isModified() will return true after
         *  copyFrom(). Also save the settings if requested by the subtask.
         */
        if (isModified() || aTask.settingsChanged)
        {
            if (aTask.settingsChanged)
                saveSettings (true /* aMarkCurStateAsModified */,
                              true /* aInformCallbacksAnyway */);
            else
                saveSettings();
        }
    }

    if (!errorInSubtask)
    {
        /* set the result (this will try to fetch current error info on failure) */
        aTask.progress->notifyComplete (rc);
    }

    if (SUCCEEDED (rc))
        mParent->onSnapshotDiscarded (mData->mUuid, Guid());

    LogFlowThisFunc (("Done discarding current state (rc=%08X)\n", rc));

    LogFlowThisFuncLeave();
}

/**
 *  Helper to change the machine state (reimplementation).
 *
 *  @note Locks this object for writing.
 */
HRESULT SessionMachine::setMachineState (MachineState_T aMachineState)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aMachineState=%d\n", aMachineState));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoLock alock (this);

    MachineState_T oldMachineState = mData->mMachineState;

    AssertMsgReturn (oldMachineState != aMachineState,
                     ("oldMachineState=%d, aMachineState=%d\n",
                      oldMachineState, aMachineState), E_FAIL);

    HRESULT rc = S_OK;

    int stsFlags = 0;
    bool deleteSavedState = false;

    /* detect some state transitions */

    if (oldMachineState < MachineState_Running &&
        aMachineState >= MachineState_Running &&
        aMachineState != MachineState_Discarding)
    {
        /*
         *  the EMT thread is about to start, so mark attached HDDs as busy
         *  and all its ancestors as being in use
         */
        for (HDData::HDAttachmentList::const_iterator it =
                 mHDData->mHDAttachments.begin();
             it != mHDData->mHDAttachments.end();
             ++ it)
        {
            ComObjPtr <HardDisk> hd = (*it)->hardDisk();
            AutoLock hdLock (hd);
            hd->setBusy();
            hd->addReaderOnAncestors();
        }
    }
    else
    if (oldMachineState >= MachineState_Running &&
        oldMachineState != MachineState_Discarding &&
        aMachineState < MachineState_Running)
    {
        /*
         *  the EMT thread stopped, so mark attached HDDs as no more busy
         *  and remove the in-use flag from all its ancestors
         */
        for (HDData::HDAttachmentList::const_iterator it =
                 mHDData->mHDAttachments.begin();
             it != mHDData->mHDAttachments.end();
             ++ it)
        {
            ComObjPtr <HardDisk> hd = (*it)->hardDisk();
            AutoLock hdLock (hd);
            hd->releaseReaderOnAncestors();
            hd->clearBusy();
        }
    }

    if (oldMachineState == MachineState_Restoring)
    {
        if (aMachineState != MachineState_Saved)
        {
            /*
             *  delete the saved state file once the machine has finished
             *  restoring from it (note that Console sets the state from
             *  Restoring to Saved if the VM couldn't restore successfully,
             *  to give the user an ability to fix an error and retry --
             *  we keep the saved state file in this case)
             */
            deleteSavedState = true;
        }
    }
    else
    if (oldMachineState == MachineState_Saved &&
        (aMachineState == MachineState_PoweredOff ||
         aMachineState == MachineState_Aborted))
    {
        /*
         *  delete the saved state after Console::DiscardSavedState() is called
         *  or if the VM process (owning a direct VM session) crashed while the
         *  VM was Saved
         */

        /// @todo (dmik)
        //      Not sure that deleting the saved state file just because of the
        //      client death before it attempted to restore the VM is a good
        //      thing. But when it crashes we need to go to the Aborted state
        //      which cannot have the saved state file associated... The only
        //      way to fix this is to make the Aborted condition not a VM state
        //      but a bool flag: i.e., when a crash occurs, set it to true and
        //      change the state to PoweredOff or Saved depending on the
        //      saved state presence.

        deleteSavedState = true;
        mData->mCurrentStateModified = TRUE;
        stsFlags |= SaveSTS_CurStateModified;
    }

    if (aMachineState == MachineState_Starting ||
        aMachineState == MachineState_Restoring)
    {
        /*
         *  set the current state modified flag to indicate that the
         *  current state is no more identical to the state in the
         *  current snapshot
         */
        if (!mData->mCurrentSnapshot.isNull())
        {
            mData->mCurrentStateModified = TRUE;
            stsFlags |= SaveSTS_CurStateModified;
        }
    }

    if (deleteSavedState == true)
    {
        Assert (!mSSData->mStateFilePath.isEmpty());
        RTFileDelete (Utf8Str (mSSData->mStateFilePath));
        mSSData->mStateFilePath.setNull();
        stsFlags |= SaveSTS_StateFilePath;
    }

    /* redirect to the underlying peer machine */
    mPeer->setMachineState (aMachineState);

    if (aMachineState == MachineState_PoweredOff ||
        aMachineState == MachineState_Aborted ||
        aMachineState == MachineState_Saved)
    {
        /* the machine has stopped execution
         * (or the saved state file was adopted) */
        stsFlags |= SaveSTS_StateTimeStamp;
    }

    if ((oldMachineState == MachineState_PoweredOff ||
         oldMachineState == MachineState_Aborted) &&
        aMachineState == MachineState_Saved)
    {
        /* the saved state file was adopted */
        Assert (!mSSData->mStateFilePath.isNull());
        stsFlags |= SaveSTS_StateFilePath;
    }

    rc = saveStateSettings (stsFlags);

    if ((oldMachineState != MachineState_PoweredOff &&
         oldMachineState != MachineState_Aborted) &&
        (aMachineState == MachineState_PoweredOff ||
         aMachineState == MachineState_Aborted))
    {
        /*
         *  clear differencing hard disks based on immutable hard disks
         *  once we've been shut down for any reason
         */
        rc = wipeOutImmutableDiffs();
    }

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  Sends the current machine state value to the VM process.
 *
 *  @note Locks this object for reading, then calls a client process.
 */
HRESULT SessionMachine::updateMachineStateOnClient()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReaderLock alock (this);
        AssertReturn (!!mData, E_FAIL);
        directControl = mData->mSession.mDirectControl;

        /* directControl may be already set to NULL here in #OnSessionEnd()
         * called too early by the direct session process while there is still
         * some operation (like discarding the snapshot) in progress. The client
         * process in this case is waiting inside Session::close() for the
         * "end session" process object to complete, while #uninit() called by
         * #checkForDeath() on the Watcher thread is waiting for the pending
         * operation to complete. For now, we accept this inconsitent behavior
         * and simply do nothing here. */

        if (mData->mSession.mState == SessionState_Closing)
            return S_OK;

        AssertReturn (!directControl.isNull(), E_FAIL);
    }

    return directControl->UpdateMachineState (mData->mMachineState);
}

/* static */
DECLCALLBACK(int) SessionMachine::taskHandler (RTTHREAD thread, void *pvUser)
{
    AssertReturn (pvUser, VERR_INVALID_POINTER);

    Task *task = static_cast <Task *> (pvUser);
    task->handler();

    // it's our responsibility to delete the task
    delete task;

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// SnapshotMachine class
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (SnapshotMachine)

HRESULT SnapshotMachine::FinalConstruct()
{
    LogFlowThisFunc (("\n"));

    /* set the proper type to indicate we're the SnapshotMachine instance */
    unconst (mType) = IsSnapshotMachine;

    return S_OK;
}

void SnapshotMachine::FinalRelease()
{
    LogFlowThisFunc (("\n"));

    uninit();
}

/**
 *  Initializes the SnapshotMachine object when taking a snapshot.
 *
 *  @param aSessionMachine  machine to take a snapshot from
 *  @param aSnapshotId      snapshot ID of this snapshot machine
 *  @param aStateFilePath   file where the execution state will be later saved
 *                          (or NULL for the offline snapshot)
 *
 *  @note The aSessionMachine must be locked for writing.
 */
HRESULT SnapshotMachine::init (SessionMachine *aSessionMachine,
                               INPTR GUIDPARAM aSnapshotId,
                               INPTR BSTR aStateFilePath)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mName={%ls}\n", aSessionMachine->mUserData->mName.raw()));

    AssertReturn (aSessionMachine && !Guid (aSnapshotId).isEmpty(), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    AssertReturn (aSessionMachine->isLockedOnCurrentThread(), E_FAIL);

    mSnapshotId = aSnapshotId;

    /* memorize the primary Machine instance (i.e. not SessionMachine!) */
    unconst (mPeer) = aSessionMachine->mPeer;
    /* share the parent pointer */
    unconst (mParent) = mPeer->mParent;

    /* take the pointer to Data to share */
    mData.share (mPeer->mData);
    /*
     *  take the pointer to UserData to share
     *  (our UserData must always be the same as Machine's data)
     */
    mUserData.share (mPeer->mUserData);
    /* make a private copy of all other data (recent changes from SessionMachine) */
    mHWData.attachCopy (aSessionMachine->mHWData);
    mHDData.attachCopy (aSessionMachine->mHDData);

    /* SSData is always unique for SnapshotMachine */
    mSSData.allocate();
    mSSData->mStateFilePath = aStateFilePath;

    /*
     *  create copies of all shared folders (mHWData after attiching a copy
     *  contains just references to original objects)
     */
    for (HWData::SharedFolderList::iterator it = mHWData->mSharedFolders.begin();
         it != mHWData->mSharedFolders.end();
         ++ it)
    {
        ComObjPtr <SharedFolder> folder;
        folder.createObject();
        HRESULT rc = folder->initCopy (this, *it);
        CheckComRCReturnRC (rc);
        *it = folder;
    }

    /* create all other child objects that will be immutable private copies */

    unconst (mBIOSSettings).createObject();
    mBIOSSettings->initCopy (this, mPeer->mBIOSSettings);

#ifdef VBOX_VRDP
    unconst (mVRDPServer).createObject();
    mVRDPServer->initCopy (this, mPeer->mVRDPServer);
#endif

    unconst (mDVDDrive).createObject();
    mDVDDrive->initCopy (this, mPeer->mDVDDrive);

    unconst (mFloppyDrive).createObject();
    mFloppyDrive->initCopy (this, mPeer->mFloppyDrive);

    unconst (mAudioAdapter).createObject();
    mAudioAdapter->initCopy (this, mPeer->mAudioAdapter);

    unconst (mUSBController).createObject();
    mUSBController->initCopy (this, mPeer->mUSBController);

    unconst (mSATAController).createObject();
    mSATAController->initCopy (this, mPeer->mSATAController);

    for (ULONG slot = 0; slot < ELEMENTS (mNetworkAdapters); slot ++)
    {
        unconst (mNetworkAdapters [slot]).createObject();
        mNetworkAdapters [slot]->initCopy (this, mPeer->mNetworkAdapters [slot]);
    }

    for (ULONG slot = 0; slot < ELEMENTS (mSerialPorts); slot ++)
    {
        unconst (mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->initCopy (this, mPeer->mSerialPorts [slot]);
    }

    for (ULONG slot = 0; slot < ELEMENTS (mParallelPorts); slot ++)
    {
        unconst (mParallelPorts [slot]).createObject();
        mParallelPorts [slot]->initCopy (this, mPeer->mParallelPorts [slot]);
    }

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Initializes the SnapshotMachine object when loading from the settings file.
 *
 *  @param aMachine machine the snapshot belngs to
 *  @param aHWNode          <Hardware> node
 *  @param aHDAsNode        <HardDiskAttachments> node
 *  @param aSnapshotId      snapshot ID of this snapshot machine
 *  @param aStateFilePath   file where the execution state is saved
 *                          (or NULL for the offline snapshot)
 *
 *  @note Doesn't lock anything.
 */
HRESULT SnapshotMachine::init (Machine *aMachine,
                               const settings::Key &aHWNode,
                               const settings::Key &aHDAsNode,
                               INPTR GUIDPARAM aSnapshotId, INPTR BSTR aStateFilePath)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mName={%ls}\n", aMachine->mUserData->mName.raw()));

    AssertReturn (aMachine && !aHWNode.isNull() && !aHDAsNode.isNull() &&
                  !Guid (aSnapshotId).isEmpty(),
                  E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    /* Don't need to lock aMachine when VirtualBox is starting up */

    mSnapshotId = aSnapshotId;

    /* memorize the primary Machine instance */
    unconst (mPeer) = aMachine;
    /* share the parent pointer */
    unconst (mParent) = mPeer->mParent;

    /* take the pointer to Data to share */
    mData.share (mPeer->mData);
    /*
     *  take the pointer to UserData to share
     *  (our UserData must always be the same as Machine's data)
     */
    mUserData.share (mPeer->mUserData);
    /* allocate private copies of all other data (will be loaded from settings) */
    mHWData.allocate();
    mHDData.allocate();

    /* SSData is always unique for SnapshotMachine */
    mSSData.allocate();
    mSSData->mStateFilePath = aStateFilePath;

    /* create all other child objects that will be immutable private copies */

    unconst (mBIOSSettings).createObject();
    mBIOSSettings->init (this);

#ifdef VBOX_VRDP
    unconst (mVRDPServer).createObject();
    mVRDPServer->init (this);
#endif

    unconst (mDVDDrive).createObject();
    mDVDDrive->init (this);

    unconst (mFloppyDrive).createObject();
    mFloppyDrive->init (this);

    unconst (mAudioAdapter).createObject();
    mAudioAdapter->init (this);

    unconst (mUSBController).createObject();
    mUSBController->init (this);

    unconst (mSATAController).createObject();
    mSATAController->init (this);

    for (ULONG slot = 0; slot < ELEMENTS (mNetworkAdapters); slot ++)
    {
        unconst (mNetworkAdapters [slot]).createObject();
        mNetworkAdapters [slot]->init (this, slot);
    }

    for (ULONG slot = 0; slot < ELEMENTS (mSerialPorts); slot ++)
    {
        unconst (mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->init (this, slot);
    }

    for (ULONG slot = 0; slot < ELEMENTS (mParallelPorts); slot ++)
    {
        unconst (mParallelPorts [slot]).createObject();
        mParallelPorts [slot]->init (this, slot);
    }

    /* load hardware and harddisk settings */

    HRESULT rc = loadHardware (aHWNode);
    if (SUCCEEDED (rc))
        rc = loadHardDisks (aHDAsNode, true /* aRegistered */, &mSnapshotId);

    if (SUCCEEDED (rc))
    {
        /* commit all changes made during the initialization */
        commit();
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  Uninitializes this SnapshotMachine object.
 */
void SnapshotMachine::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    uninitDataAndChildObjects();

    /* free the essential data structure last */
    mData.free();

    unconst (mParent).setNull();
    unconst (mPeer).setNull();

    LogFlowThisFuncLeave();
}

// AutoLock::Lockable interface
////////////////////////////////////////////////////////////////////////////////

/**
 *  Overrides VirtualBoxBase::lockHandle() in order to share the lock handle
 *  with the primary Machine instance (mPeer).
 */
RWLockHandle *SnapshotMachine::lockHandle() const
{
    AssertReturn (!mPeer.isNull(), NULL);
    return mPeer->lockHandle();
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/**
 *  Called by the snapshot object associated with this SnapshotMachine when
 *  snapshot data such as name or description is changed.
 *
 *  @note Locks this object for writing.
 */
HRESULT SnapshotMachine::onSnapshotChange (Snapshot *aSnapshot)
{
    AutoLock alock (this);

    mPeer->saveSnapshotSettings (aSnapshot, SaveSS_UpdateAttrsOp);

    /* inform callbacks */
    mParent->onSnapshotChange (mData->mUuid, aSnapshot->data().mId);

    return S_OK;
}
