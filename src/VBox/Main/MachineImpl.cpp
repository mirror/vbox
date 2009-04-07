/* $Id$ */

/** @file
 * Implementation of IMachine in VBoxSVC.
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

/* Make sure all the stdint.h macros are included - must come first! */
#ifndef __STDC_LIMIT_MACROS
# define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
# define __STDC_CONSTANT_MACROS
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
#include "ProgressImpl.h"
#include "HardDiskAttachmentImpl.h"
#include "USBControllerImpl.h"
#include "HostImpl.h"
#include "SystemPropertiesImpl.h"
#include "SharedFolderImpl.h"
#include "GuestOSTypeImpl.h"
#include "VirtualBoxErrorInfoImpl.h"
#include "GuestImpl.h"
#include "StorageControllerImpl.h"

#ifdef VBOX_WITH_USB
# include "USBProxyService.h"
#endif

#include "VirtualBoxXMLUtil.h"

#include "Logging.h"
#include "Performance.h"

#include <stdio.h>
#include <stdlib.h>

#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/asm.h>
#include <iprt/process.h>
#include <iprt/cpputils.h>
#include <iprt/env.h>
#include <iprt/string.h>

#include <VBox/com/array.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/settings.h>

#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/HostServices/GuestPropertySvc.h>
# include <VBox/com/array.h>
#endif

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
static const char gDefaultMachineConfig[] =
{
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" RTFILE_LINEFEED
    "<!-- Sun VirtualBox Machine Configuration -->" RTFILE_LINEFEED
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
    Progress *progress = static_cast<Progress*>(pvUser);

    /* update the progress object */
    if (progress)
        progress->setCurrentOperationProgress(uPercentage);

    return VINF_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////
// Machine::Data structure
/////////////////////////////////////////////////////////////////////////////

Machine::Data::Data()
{
    mRegistered = FALSE;
    mAccessible = FALSE;
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
    mHWVersion = "2"; /** @todo get the default from the schema if that is possible. */
    mMemorySize = 128;
    mCPUCount = 1;
    mMemoryBalloonSize = 0;
    mStatisticsUpdateInterval = 0;
    mVRAMSize = 8;
    mAccelerate3DEnabled = false;
    mMonitorCount = 1;
    mHWVirtExEnabled = TSBool_False;
    mHWVirtExNestedPagingEnabled = false;
    mHWVirtExVPIDEnabled = false;
    mPAEEnabled = false;
    mPropertyServiceActive = false;

    /* default boot order: floppy - DVD - HDD */
    mBootOrder [0] = DeviceType_Floppy;
    mBootOrder [1] = DeviceType_DVD;
    mBootOrder [2] = DeviceType_HardDisk;
    for (size_t i = 3; i < RT_ELEMENTS (mBootOrder); i++)
        mBootOrder [i] = DeviceType_Null;

    mClipboardMode = ClipboardMode_Bidirectional;
    mGuestPropertyNotificationPatterns = "";
}

Machine::HWData::~HWData()
{
}

bool Machine::HWData::operator== (const HWData &that) const
{
    if (this == &that)
        return true;

    if (mHWVersion != that.mHWVersion ||
        mMemorySize != that.mMemorySize ||
        mMemoryBalloonSize != that.mMemoryBalloonSize ||
        mStatisticsUpdateInterval != that.mStatisticsUpdateInterval ||
        mVRAMSize != that.mVRAMSize ||
        mAccelerate3DEnabled != that.mAccelerate3DEnabled ||
        mMonitorCount != that.mMonitorCount ||
        mHWVirtExEnabled != that.mHWVirtExEnabled ||
        mHWVirtExNestedPagingEnabled != that.mHWVirtExNestedPagingEnabled ||
        mHWVirtExVPIDEnabled != that.mHWVirtExVPIDEnabled ||
        mPAEEnabled != that.mPAEEnabled ||
        mCPUCount != that.mCPUCount ||
        mClipboardMode != that.mClipboardMode)
        return false;

    for (size_t i = 0; i < RT_ELEMENTS (mBootOrder); ++ i)
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
}

Machine::HDData::~HDData()
{
}

bool Machine::HDData::operator== (const HDData &that) const
{
    if (this == &that)
        return true;

    if (mAttachments.size() != that.mAttachments.size())
        return false;

    if (mAttachments.size() == 0)
        return true;

    /* Make copies to speed up comparison */
    AttachmentList atts = mAttachments;
    AttachmentList thatAtts = that.mAttachments;

    AttachmentList::iterator it = atts.begin();
    while (it != atts.end())
    {
        bool found = false;
        AttachmentList::iterator thatIt = thatAtts.begin();
        while (thatIt != thatAtts.end())
        {
            if ((*it)->controller() == (*thatIt)->controller() &&
                (*it)->port() == (*thatIt)->port() &&
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
 *  @param aOsType      OS Type of this machine
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
HRESULT Machine::init (VirtualBox *aParent, CBSTR aConfigFile,
                       InitMode aMode, CBSTR aName /* = NULL */,
                       GuestOSType *aOsType /* = NULL */,
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
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    /* share the parent weakly */
    unconst (mParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    /* allocate the essential machine data structure (the rest will be
     * allocated later by initDataAndChildObjects() */
    mData.allocate();

    /* memorize the config file name (as provided) */
    mData->mConfigFile = aConfigFile;

    /* get the full file name */
    Utf8Str configFileFull;
    int vrc = mParent->calculateFullPath (Utf8Str (aConfigFile), configFileFull);
    if (RT_FAILURE (vrc))
        return setError (VBOX_E_FILE_ERROR,
            tr ("Invalid machine settings file name '%ls' (%Rrc)"),
            aConfigFile, vrc);

    mData->mConfigFileFull = configFileFull;

    if (aMode == Init_Registered)
    {
        mData->mRegistered = TRUE;

        /* store the supplied UUID (will be used to check for UUID consistency
         * in loadSettings() */
        unconst (mData->mUuid) = *aId;
        rc = registeredInit();
    }
    else
    {
        if (aMode == Init_Existing)
        {
            /* lock the settings file */
            rc = lockConfig();
        }
        else if (aMode == Init_New)
        {
            /* check for the file existence */
            RTFILE f = NIL_RTFILE;
            int vrc = RTFileOpen (&f, configFileFull, RTFILE_O_READ);
            if (RT_SUCCESS (vrc) || vrc == VERR_SHARING_VIOLATION)
            {
                rc = setError (VBOX_E_FILE_ERROR,
                    tr ("Machine settings file '%s' already exists"),
                    configFileFull.raw());
                if (RT_SUCCESS (vrc))
                    RTFileClose (f);
            }
            else
            {
                if (vrc != VERR_FILE_NOT_FOUND && vrc != VERR_PATH_NOT_FOUND)
                    rc = setError (VBOX_E_FILE_ERROR,
                        tr ("Invalid machine settings file name '%ls' (%Rrc)"),
                        mData->mConfigFileFull.raw(), vrc);
            }
        }
        else
            AssertFailed();

        if (SUCCEEDED (rc))
            rc = initDataAndChildObjects();

        if (SUCCEEDED (rc))
        {
            /* set to true now to cause uninit() to call
             * uninitDataAndChildObjects() on failure */
            mData->mAccessible = TRUE;

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

                if (aOsType)
                {
                    /* Store OS type */
                    mUserData->mOSTypeId = aOsType->id();

                    /* Apply HWVirtEx default; always true (used to rely on aOsType->recommendedVirtEx())  */
                    mHWData->mHWVirtExEnabled = TSBool_True;

                    /* Apply BIOS defaults */
                    mBIOSSettings->applyDefaults (aOsType);

                    /* Apply network adapters defaults */
                    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); ++ slot)
                        mNetworkAdapters [slot]->applyDefaults (aOsType);
                }

                /* The default is that the VM has at least one IDE controller
                 * which can't be disabled (because of the DVD stuff which is
                 * not in the StorageDevice implementation at the moment)
                 */
                ComPtr<IStorageController> pController;
                rc = AddStorageController(Bstr("IDE"), StorageBus_IDE, pController.asOutParam());
                CheckComRCReturnRC(rc);
                ComObjPtr<StorageController> ctl;
                rc = getStorageControllerByName(Bstr("IDE"), ctl, true);
                CheckComRCReturnRC(rc);
                ctl->COMSETTER(ControllerType)(StorageControllerType_PIIX4);
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
    AssertReturn (!mData->mAccessible, E_FAIL);

    HRESULT rc = lockConfig();

    if (SUCCEEDED (rc))
        rc = initDataAndChildObjects();

    if (SUCCEEDED (rc))
    {
        /* Temporarily reset the registered flag in order to let setters
         * potentially called from loadSettings() succeed (isMutable() used in
         * all setters will return FALSE for a Machine instance if mRegistered
         * is TRUE). */
        mData->mRegistered = FALSE;

        rc = loadSettings (true /* aRegistered */);

        /* Restore the registered flag (even on failure) */
        mData->mRegistered = TRUE;

        if (FAILED (rc))
            unlockConfig();
    }

    if (SUCCEEDED (rc))
    {
        /* Set mAccessible to TRUE only if we successfully locked and loaded
         * the settings file */
        mData->mAccessible = TRUE;

        /* commit all changes made during loading the settings file */
        commit();
    }
    else
    {
        /* If the machine is registered, then, instead of returning a
         * failure, we mark it as inaccessible and set the result to
         * success to give it a try later */

        /* fetch the current error info */
        mData->mAccessError = com::ErrorInfo();
        LogWarning (("Machine {%RTuuid} is inaccessible! [%ls]\n",
                     mData->mUuid.raw(),
                     mData->mAccessError.getText().raw()));

        /* rollback all changes */
        rollback (false /* aNotify */);

        /* uninitialize the common part to make sure all data is reset to
         * default (null) values */
        uninitDataAndChildObjects();

        rc = S_OK;
    }

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
 *  threads are done but preventing them from doing so by holding a lock.
 */
void Machine::uninit()
{
    LogFlowThisFuncEnter();

    Assert (!isWriteLockOnCurrentThread());

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

        if (Global::IsOnlineOrTransient (mData->mMachineState))
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
    CheckComArgOutPointerValid (aParent);

    AutoLimitedCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mParent is constant during life time, no need to lock */
    mParent.queryInterfaceTo (aParent);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Accessible) (BOOL *aAccessible)
{
    CheckComArgOutPointerValid (aAccessible);

    AutoLimitedCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = S_OK;

    if (!mData->mAccessible)
    {
        /* try to initialize the VM once more if not accessible */

        AutoReinitSpan autoReinitSpan (this);
        AssertReturn (autoReinitSpan.isOk(), E_FAIL);

        rc = registeredInit();

        if (SUCCEEDED (rc) && mData->mAccessible)
        {
            autoReinitSpan.setSucceeded();

            /* make sure interesting parties will notice the accessibility
             * state change */
            mParent->onMachineStateChange (mData->mUuid, mData->mMachineState);
            mParent->onMachineDataChange (mData->mUuid);
        }
    }

    if (SUCCEEDED (rc))
        *aAccessible = mData->mAccessible;

    return rc;
}

STDMETHODIMP Machine::COMGETTER(AccessError) (IVirtualBoxErrorInfo **aAccessError)
{
    CheckComArgOutPointerValid (aAccessError);

    AutoLimitedCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

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
    CheckComArgOutPointerValid (aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mUserData->mName.cloneTo (aName);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Name) (IN_BSTR aName)
{
    CheckComArgNotNull (aName);

    if (!*aName)
        return setError (E_INVALIDARG,
            tr ("Machine name cannot be empty"));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mUserData.backup();
    mUserData->mName = aName;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Description) (BSTR *aDescription)
{
    CheckComArgOutPointerValid (aDescription);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mUserData->mDescription.cloneTo (aDescription);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Description) (IN_BSTR aDescription)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mUserData.backup();
    mUserData->mDescription = aDescription;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Id) (OUT_GUID aId)
{
    CheckComArgOutPointerValid (aId);

    AutoLimitedCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData->mUuid.cloneTo (aId);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(OSTypeId) (BSTR *aOSTypeId)
{
    CheckComArgOutPointerValid (aOSTypeId);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mUserData->mOSTypeId.cloneTo (aOSTypeId);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(OSTypeId) (IN_BSTR aOSTypeId)
{
    CheckComArgNotNull (aOSTypeId);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* look up the object by Id to check it is valid */
    ComPtr <IGuestOSType> guestOSType;
    HRESULT rc = mParent->GetGuestOSType (aOSTypeId,
                                          guestOSType.asOutParam());
    CheckComRCReturnRC (rc);

    /* when setting, always use the "etalon" value for consistency -- lookup
     * by ID is case-insensitive and the input value may have different case */
    Bstr osTypeId;
    rc = guestOSType->COMGETTER(Id) (osTypeId.asOutParam());
    CheckComRCReturnRC (rc);

    AutoWriteLock alock (this);

    rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mUserData.backup();
    mUserData->mOSTypeId = osTypeId;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(HardwareVersion) (BSTR *aHWVersion)
{
    if (!aHWVersion)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mHWData->mHWVersion.cloneTo (aHWVersion);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(HardwareVersion) (IN_BSTR aHWVersion)
{
    /* check known version */
    Utf8Str hwVersion = aHWVersion;
    if (    hwVersion.compare ("1") != 0
        &&  hwVersion.compare ("2") != 0)
        return setError (E_INVALIDARG,
            tr ("Invalid hardware version: %ls\n"), aHWVersion);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mHWVersion = hwVersion;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(MemorySize) (ULONG *memorySize)
{
    if (!memorySize)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *memorySize = mHWData->mMemorySize;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(MemorySize) (ULONG memorySize)
{
    /* check RAM limits */
    if (memorySize < MM_RAM_MIN_IN_MB ||
        memorySize > MM_RAM_MAX_IN_MB)
        return setError (E_INVALIDARG,
            tr ("Invalid RAM size: %lu MB (must be in range [%lu, %lu] MB)"),
                memorySize, MM_RAM_MIN_IN_MB, MM_RAM_MAX_IN_MB);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mMemorySize = memorySize;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CPUCount) (ULONG *CPUCount)
{
    if (!CPUCount)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *CPUCount = mHWData->mCPUCount;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(CPUCount) (ULONG CPUCount)
{
    /* check RAM limits */
    if (CPUCount < SchemaDefs::MinCPUCount ||
        CPUCount > SchemaDefs::MaxCPUCount)
        return setError (E_INVALIDARG,
            tr ("Invalid virtual CPU count: %lu (must be in range [%lu, %lu])"),
                CPUCount, SchemaDefs::MinCPUCount, SchemaDefs::MaxCPUCount);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mCPUCount = CPUCount;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(VRAMSize) (ULONG *memorySize)
{
    if (!memorySize)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

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

    AutoWriteLock alock (this);

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

    AutoReadLock alock (this);

    *memoryBalloonSize = mHWData->mMemoryBalloonSize;

    return S_OK;
}

/** @todo this method should not be public */
STDMETHODIMP Machine::COMSETTER(MemoryBalloonSize) (ULONG memoryBalloonSize)
{
    /* check limits */
    if (memoryBalloonSize >= VMMDEV_MAX_MEMORY_BALLOON (mHWData->mMemorySize))
        return setError (E_INVALIDARG,
            tr ("Invalid memory balloon size: %lu MB (must be in range [%lu, %lu] MB)"),
                memoryBalloonSize, 0, VMMDEV_MAX_MEMORY_BALLOON (mHWData->mMemorySize));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

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

    AutoReadLock alock (this);

    *statisticsUpdateInterval = mHWData->mStatisticsUpdateInterval;

    return S_OK;
}

/** @todo this method should not be public */
STDMETHODIMP Machine::COMSETTER(StatisticsUpdateInterval) (ULONG statisticsUpdateInterval)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mStatisticsUpdateInterval = statisticsUpdateInterval;

    return S_OK;
}


STDMETHODIMP Machine::COMGETTER(Accelerate3DEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *enabled = mHWData->mAccelerate3DEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Accelerate3DEnabled)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /** @todo check validity! */

    mHWData.backup();
    mHWData->mAccelerate3DEnabled = enable;

    return S_OK;
}


STDMETHODIMP Machine::COMGETTER(MonitorCount) (ULONG *monitorCount)
{
    if (!monitorCount)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

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

    AutoWriteLock alock (this);

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

    AutoReadLock alock (this);

    *enabled = mHWData->mHWVirtExEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(HWVirtExEnabled)(TSBool_T enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /** @todo check validity! */

    mHWData.backup();
    mHWData->mHWVirtExEnabled = enable;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(HWVirtExNestedPagingEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *enabled = mHWData->mHWVirtExNestedPagingEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(HWVirtExNestedPagingEnabled)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /** @todo check validity! */

    mHWData.backup();
    mHWData->mHWVirtExNestedPagingEnabled = enable;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(HWVirtExVPIDEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *enabled = mHWData->mHWVirtExVPIDEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(HWVirtExVPIDEnabled)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /** @todo check validity! */

    mHWData.backup();
    mHWData->mHWVirtExVPIDEnabled = enable;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(PAEEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *enabled = mHWData->mPAEEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(PAEEnabled)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /** @todo check validity! */

    mHWData.backup();
    mHWData->mPAEEnabled = enable;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SnapshotFolder) (BSTR *aSnapshotFolder)
{
    CheckComArgOutPointerValid (aSnapshotFolder);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mUserData->mSnapshotFolderFull.cloneTo (aSnapshotFolder);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(SnapshotFolder) (IN_BSTR aSnapshotFolder)
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

    AutoWriteLock alock (this);

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
            snapshotFolder = Utf8StrFmt ("{%RTuuid}", mData->mUuid.raw());
        }
    }

    int vrc = calculateFullPath (snapshotFolder, snapshotFolder);
    if (RT_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Invalid snapshot folder '%ls' (%Rrc)"),
                aSnapshotFolder, vrc);

    mUserData.backup();
    mUserData->mSnapshotFolder = aSnapshotFolder;
    mUserData->mSnapshotFolderFull = snapshotFolder;

    return S_OK;
}

STDMETHODIMP Machine::
COMGETTER(HardDiskAttachments) (ComSafeArrayOut(IHardDiskAttachment *, aAttachments))
{
    if (ComSafeArrayOutIsNull (aAttachments))
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    SafeIfaceArray<IHardDiskAttachment> attachments (mHDData->mAttachments);
    attachments.detachTo (ComSafeArrayOutArg (aAttachments));

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(VRDPServer)(IVRDPServer **vrdpServer)
{
#ifdef VBOX_WITH_VRDP
    if (!vrdpServer)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Assert (!!mVRDPServer);
    mVRDPServer.queryInterfaceTo (vrdpServer);

    return S_OK;
#else
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP Machine::COMGETTER(DVDDrive) (IDVDDrive **dvdDrive)
{
    if (!dvdDrive)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

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

    AutoReadLock alock (this);

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

    AutoReadLock alock (this);

    mAudioAdapter.queryInterfaceTo (audioAdapter);
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(USBController) (IUSBController **aUSBController)
{
#ifdef VBOX_WITH_USB
    CheckComArgOutPointerValid (aUSBController);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    MultiResult rc = mParent->host()->checkUSBProxyService();
    CheckComRCReturnRC (rc);

    AutoReadLock alock (this);

    return rc = mUSBController.queryInterfaceTo (aUSBController);
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treting it as a failure), for example, as in OSE */
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP Machine::COMGETTER(SettingsFilePath) (BSTR *aFilePath)
{
    CheckComArgOutPointerValid (aFilePath);

    AutoLimitedCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData->mConfigFileFull.cloneTo (aFilePath);
    return S_OK;
}

STDMETHODIMP Machine::
COMGETTER(SettingsFileVersion) (BSTR *aSettingsFileVersion)
{
    CheckComArgOutPointerValid (aSettingsFileVersion);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData->mSettingsFileVersion.cloneTo (aSettingsFileVersion);
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SettingsModified) (BOOL *aModified)
{
    CheckComArgOutPointerValid (aModified);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

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
    CheckComArgOutPointerValid (aSessionState);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aSessionState = mData->mSession.mState;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SessionType) (BSTR *aSessionType)
{
    CheckComArgOutPointerValid (aSessionType);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData->mSession.mType.cloneTo (aSessionType);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SessionPid) (ULONG *aSessionPid)
{
    CheckComArgOutPointerValid (aSessionPid);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aSessionPid = mData->mSession.mPid;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(State) (MachineState_T *machineState)
{
    if (!machineState)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *machineState = mData->mMachineState;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(LastStateChange) (LONG64 *aLastStateChange)
{
    CheckComArgOutPointerValid (aLastStateChange);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aLastStateChange = RTTimeSpecGetMilli (&mData->mLastStateChange);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(StateFilePath) (BSTR *aStateFilePath)
{
    CheckComArgOutPointerValid (aStateFilePath);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mSSData->mStateFilePath.cloneTo (aStateFilePath);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(LogFolder) (BSTR *aLogFolder)
{
    CheckComArgOutPointerValid (aLogFolder);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Utf8Str logFolder;
    getLogFolder (logFolder);

    Bstr (logFolder).cloneTo (aLogFolder);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CurrentSnapshot) (ISnapshot **aCurrentSnapshot)
{
    CheckComArgOutPointerValid (aCurrentSnapshot);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData->mCurrentSnapshot.queryInterfaceTo (aCurrentSnapshot);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SnapshotCount) (ULONG *aSnapshotCount)
{
    CheckComArgOutPointerValid (aSnapshotCount);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aSnapshotCount = !mData->mFirstSnapshot ? 0 :
                      mData->mFirstSnapshot->descendantCount() + 1 /* self */;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CurrentStateModified) (BOOL *aCurrentStateModified)
{
    CheckComArgOutPointerValid (aCurrentStateModified);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    /* Note: for machines with no snapshots, we always return FALSE
     * (mData->mCurrentStateModified will be TRUE in this case, for historical
     * reasons :) */

    *aCurrentStateModified = !mData->mFirstSnapshot ? FALSE :
                             mData->mCurrentStateModified;

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(SharedFolders) (ComSafeArrayOut (ISharedFolder *, aSharedFolders))
{
    CheckComArgOutSafeArrayPointerValid (aSharedFolders);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    SafeIfaceArray <ISharedFolder> folders (mHWData->mSharedFolders);
    folders.detachTo (ComSafeArrayOutArg(aSharedFolders));

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(ClipboardMode) (ClipboardMode_T *aClipboardMode)
{
    CheckComArgOutPointerValid (aClipboardMode);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aClipboardMode = mHWData->mClipboardMode;

    return S_OK;
}

STDMETHODIMP
Machine::COMSETTER(ClipboardMode) (ClipboardMode_T aClipboardMode)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mClipboardMode = aClipboardMode;

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(GuestPropertyNotificationPatterns) (BSTR *aPatterns)
{
    CheckComArgOutPointerValid (aPatterns);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mHWData->mGuestPropertyNotificationPatterns.cloneTo (aPatterns);

    return RT_LIKELY (aPatterns != NULL) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
Machine::COMSETTER(GuestPropertyNotificationPatterns) (IN_BSTR aPatterns)
{
    AssertLogRelReturn (VALID_PTR (aPatterns), E_POINTER);
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mGuestPropertyNotificationPatterns = aPatterns;

    return RT_LIKELY (!mHWData->mGuestPropertyNotificationPatterns.isNull())
               ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
Machine::COMGETTER(StorageControllers) (ComSafeArrayOut(IStorageController *, aStorageControllers))
{
    CheckComArgOutSafeArrayPointerValid (aStorageControllers);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    SafeIfaceArray <IStorageController> ctrls (*mStorageControllers.data());
    ctrls.detachTo (ComSafeArrayOutArg(aStorageControllers));

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
        return setError (E_NOTIMPL,
            tr ("Booting from USB device is currently not supported"));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

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

    AutoReadLock alock (this);

    *aDevice = mHWData->mBootOrder [aPosition - 1];

    return S_OK;
}

STDMETHODIMP Machine::AttachHardDisk(IN_GUID aId,
                                     IN_BSTR aControllerName, LONG aControllerPort,
                                     LONG aDevice)
{
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%ld aDevice=%ld\n",
                     aControllerName, aControllerPort, aDevice));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::findHardDisk() need read lock; also we want to make sure the
     * hard disk object we pick up doesn't get unregistered before we finish. */
    AutoReadLock vboxLock (mParent);
    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /// @todo NEWMEDIA implicit machine registration
    if (!mData->mRegistered)
        return setError (VBOX_E_INVALID_OBJECT_STATE,
            tr ("Cannot attach hard disks to an unregistered machine"));

    AssertReturn (mData->mMachineState != MachineState_Saved, E_FAIL);

    if (Global::IsOnlineOrTransient (mData->mMachineState))
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Invalid machine state: %d"), mData->mMachineState);

    /* Check for an existing controller. */
    ComObjPtr<StorageController> ctl;
    rc = getStorageControllerByName(aControllerName, ctl, true /* aSetError */);
    CheckComRCReturnRC(rc);

    /* check that the port and device are not out of range. */
    ULONG portCount;
    ULONG devicesPerPort;
    rc = ctl->COMGETTER(PortCount)(&portCount);
    CheckComRCReturnRC(rc);
    rc = ctl->COMGETTER(MaxDevicesPerPortCount)(&devicesPerPort);
    CheckComRCReturnRC(rc);

    if (   (aControllerPort < 0)
        || (aControllerPort >= (LONG)portCount)
        || (aDevice < 0)
        || (aDevice >= (LONG)devicesPerPort)
       )
        return setError (E_INVALIDARG,
            tr ("The port and/or count parameter are out of range [%lu:%lu]"),
                portCount, devicesPerPort);

    /* check if the device slot is already busy */
    HDData::AttachmentList::const_iterator it =
        std::find_if (mHDData->mAttachments.begin(),
                      mHDData->mAttachments.end(),
                      HardDiskAttachment::EqualsTo (aControllerName, aControllerPort, aDevice));

    if (it != mHDData->mAttachments.end())
    {
        ComObjPtr<HardDisk> hd = (*it)->hardDisk();
        AutoReadLock hdLock (hd);
        return setError (VBOX_E_OBJECT_IN_USE,
            tr ("Hard disk '%ls' is already attached to device slot %d on "
                "port %d of controller '%ls' of this virtual machine"),
            hd->locationFull().raw(), aDevice, aControllerPort, aControllerName);
    }

    Guid id = aId;

    /* find a hard disk by UUID */
    ComObjPtr<HardDisk> hd;
    rc = mParent->findHardDisk(&id, NULL, true /* aSetError */, &hd);
    CheckComRCReturnRC (rc);

    AutoCaller hdCaller (hd);
    CheckComRCReturnRC (hdCaller.rc());

    AutoWriteLock hdLock (hd);

    if (std::find_if (mHDData->mAttachments.begin(),
                      mHDData->mAttachments.end(),
                      HardDiskAttachment::RefersTo (hd)) !=
            mHDData->mAttachments.end())
    {
        return setError (VBOX_E_OBJECT_IN_USE,
            tr ("Hard disk '%ls' is already attached to this virtual machine"),
            hd->locationFull().raw());
    }

    bool indirect = hd->isReadOnly();
    bool associate = true;

    do
    {
        if (mHDData.isBackedUp())
        {
            const HDData::AttachmentList &oldAtts =
                mHDData.backedUpData()->mAttachments;

            /* check if the hard disk was attached to the VM before we started
             * changing attachemnts in which case the attachment just needs to
             * be restored */
            HDData::AttachmentList::const_iterator it =
                std::find_if (oldAtts.begin(), oldAtts.end(),
                              HardDiskAttachment::RefersTo (hd));
            if (it != oldAtts.end())
            {
                AssertReturn (!indirect, E_FAIL);

                /* see if it's the same bus/channel/device */
                if ((*it)->device() == aDevice &&
                    (*it)->port() == aControllerPort &&
                    (*it)->controller() == aControllerName)
                {
                    /* the simplest case: restore the whole attachment
                     * and return, nothing else to do */
                    mHDData->mAttachments.push_back (*it);
                    return S_OK;
                }

                /* bus/channel/device differ; we need a new attachment object,
                 * but don't try to associate it again */
                associate = false;
                break;
            }
        }

        /* go further only if the attachment is to be indirect */
        if (!indirect)
            break;

        /* perform the so called smart attachment logic for indirect
         * attachments. Note that smart attachment is only applicable to base
         * hard disks. */

        if (hd->parent().isNull())
        {
            /* first, investigate the backup copy of the current hard disk
             * attachments to make it possible to re-attach existing diffs to
             * another device slot w/o losing their contents */
            if (mHDData.isBackedUp())
            {
                const HDData::AttachmentList &oldAtts =
                    mHDData.backedUpData()->mAttachments;

                HDData::AttachmentList::const_iterator foundIt = oldAtts.end();
                uint32_t foundLevel = 0;

                for (HDData::AttachmentList::const_iterator
                     it = oldAtts.begin(); it != oldAtts.end(); ++ it)
                {
                    uint32_t level = 0;
                    if ((*it)->hardDisk()->root (&level).equalsTo (hd))
                    {
                        /* skip the hard disk if its currently attached (we
                         * cannot attach the same hard disk twice) */
                        if (std::find_if (mHDData->mAttachments.begin(),
                                          mHDData->mAttachments.end(),
                                          HardDiskAttachment::RefersTo (
                                              (*it)->hardDisk())) !=
                                mHDData->mAttachments.end())
                            continue;

                        /* matched device, channel and bus (i.e. attached to the
                         * same place) will win and immediately stop the search;
                         * otherwise the attachment that has the youngest
                         * descendant of hd will be used
                         */
                        if ((*it)->device() == aDevice &&
                            (*it)->port() == aControllerPort &&
                            (*it)->controller() == aControllerName)
                        {
                            /* the simplest case: restore the whole attachment
                             * and return, nothing else to do */
                            mHDData->mAttachments.push_back (*it);
                            return S_OK;
                        }
                        else
                        if (foundIt == oldAtts.end() ||
                            level > foundLevel /* prefer younger */)
                        {
                            foundIt = it;
                            foundLevel = level;
                        }
                    }
                }

                if (foundIt != oldAtts.end())
                {
                    /* use the previously attached hard disk */
                    hd = (*foundIt)->hardDisk();
                    hdCaller.attach (hd);
                    CheckComRCReturnRC (hdCaller.rc());
                    hdLock.attach (hd);
                    /* not implicit, doesn't require association with this VM */
                    indirect = false;
                    associate = false;
                    /* go right to the HardDiskAttachment creation */
                    break;
                }
            }

            /* then, search through snapshots for the best diff in the given
             * hard disk's chain to base the new diff on */

            ComObjPtr<HardDisk> base;
            ComObjPtr<Snapshot> snap = mData->mCurrentSnapshot;
            while (snap)
            {
                AutoReadLock snapLock (snap);

                const HDData::AttachmentList &snapAtts =
                    snap->data().mMachine->mHDData->mAttachments;

                HDData::AttachmentList::const_iterator foundIt = snapAtts.end();
                uint32_t foundLevel = 0;

                for (HDData::AttachmentList::const_iterator
                     it = snapAtts.begin(); it != snapAtts.end(); ++ it)
                {
                    uint32_t level = 0;
                    if ((*it)->hardDisk()->root (&level).equalsTo (hd))
                    {
                        /* matched device, channel and bus (i.e. attached to the
                         * same place) will win and immediately stop the search;
                         * otherwise the attachment that has the youngest
                         * descendant of hd will be used
                         */
                        if ((*it)->device() == aDevice &&
                            (*it)->port() == aControllerPort &&
                            (*it)->controller() == aControllerName)
                        {
                            foundIt = it;
                            break;
                        }
                        else
                        if (foundIt == snapAtts.end() ||
                            level > foundLevel /* prefer younger */)
                        {
                            foundIt = it;
                            foundLevel = level;
                        }
                    }
                }

                if (foundIt != snapAtts.end())
                {
                    base = (*foundIt)->hardDisk();
                    break;
                }

                snap = snap->parent();
            }

            /* found a suitable diff, use it as a base */
            if (!base.isNull())
            {
                hd = base;
                hdCaller.attach (hd);
                CheckComRCReturnRC (hdCaller.rc());
                hdLock.attach (hd);
            }
        }

        ComObjPtr<HardDisk> diff;
        diff.createObject();
        rc = diff->init(mParent,
                        hd->preferredDiffFormat(),
                        BstrFmt ("%ls"RTPATH_SLASH_STR,
                                 mUserData->mSnapshotFolderFull.raw()));
        CheckComRCReturnRC (rc);

        /* make sure the hard disk is not modified before createDiffStorage() */
        rc = hd->LockRead (NULL);
        CheckComRCReturnRC (rc);

        /* will leave the lock before the potentially lengthy operation, so
         * protect with the special state */
        MachineState_T oldState = mData->mMachineState;
        setMachineState (MachineState_SettingUp);

        hdLock.leave();
        alock.leave();
        vboxLock.unlock();

        rc = hd->createDiffStorageAndWait (diff, HardDiskVariant_Standard);

        alock.enter();
        hdLock.enter();

        setMachineState (oldState);

        hd->UnlockRead (NULL);

        CheckComRCReturnRC (rc);

        /* use the created diff for the actual attachment */
        hd = diff;
        hdCaller.attach (hd);
        CheckComRCReturnRC (hdCaller.rc());
        hdLock.attach (hd);
    }
    while (0);

    ComObjPtr<HardDiskAttachment> attachment;
    attachment.createObject();
    rc = attachment->init (hd, aControllerName, aControllerPort, aDevice, indirect);
    CheckComRCReturnRC (rc);

    if (associate)
    {
        /* as the last step, associate the hard disk to the VM */
        rc = hd->attachTo (mData->mUuid);
        /* here we can fail because of Deleting, or being in process of
         * creating a Diff */
        CheckComRCReturnRC (rc);
    }

    /* sucsess: finally remember the attachment */
    mHDData.backup();
    mHDData->mAttachments.push_back (attachment);

    return rc;
}

STDMETHODIMP Machine::GetHardDisk(IN_BSTR aControllerName, LONG aControllerPort,
                                  LONG aDevice, IHardDisk **aHardDisk)
{
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%ld aDevice=%ld\n",
                     aControllerName, aControllerPort, aDevice));

    CheckComArgNotNull (aControllerName);
    CheckComArgOutPointerValid (aHardDisk);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aHardDisk = NULL;

    HDData::AttachmentList::const_iterator it =
        std::find_if (mHDData->mAttachments.begin(),
                      mHDData->mAttachments.end(),
                      HardDiskAttachment::EqualsTo (aControllerName, aControllerPort, aDevice));

    if (it == mHDData->mAttachments.end())
        return setError (VBOX_E_OBJECT_NOT_FOUND,
            tr ("No hard disk attached to device slot %d on port %d of controller '%ls'"),
            aDevice, aControllerPort, aControllerName);

    (*it)->hardDisk().queryInterfaceTo (aHardDisk);

    return S_OK;
}

STDMETHODIMP Machine::DetachHardDisk(IN_BSTR aControllerName, LONG aControllerPort,
                                     LONG aDevice)
{
    CheckComArgNotNull (aControllerName);

    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%ld aDevice=%ld\n",
                     aControllerName, aControllerPort, aDevice));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    AssertReturn (mData->mMachineState != MachineState_Saved, E_FAIL);

    if (Global::IsOnlineOrTransient (mData->mMachineState))
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Invalid machine state: %d"), mData->mMachineState);

    HDData::AttachmentList::const_iterator it =
        std::find_if (mHDData->mAttachments.begin(),
                      mHDData->mAttachments.end(),
                      HardDiskAttachment::EqualsTo (aControllerName, aControllerPort, aDevice));

    if (it == mHDData->mAttachments.end())
        return setError (VBOX_E_OBJECT_NOT_FOUND,
            tr ("No hard disk attached to device slot %d on port %d of controller '%ls'"),
            aDevice, aControllerPort, aControllerName);

    ComObjPtr<HardDiskAttachment> hda = *it;
    ComObjPtr<HardDisk> hd = hda->hardDisk();

    if (hda->isImplicit())
    {
        /* attempt to implicitly delete the implicitly created diff */

        /// @todo move the implicit flag from HardDiskAttachment to HardDisk
        /// and forbid any hard disk operation when it is implicit. Or maybe
        /// a special media state for it to make it even more simple.

        Assert (mHDData.isBackedUp());

        /* will leave the lock before the potentially lengthy operation, so
         * protect with the special state */
        MachineState_T oldState = mData->mMachineState;
        setMachineState (MachineState_SettingUp);

        alock.leave();

        rc = hd->deleteStorageAndWait();

        alock.enter();

        setMachineState (oldState);

        CheckComRCReturnRC (rc);
    }

    mHDData.backup();

    /* we cannot use erase (it) below because backup() above will create
     * a copy of the list and make this copy active, but the iterator
     * still refers to the original and is not valid for the copy */
    mHDData->mAttachments.remove (hda);

    return S_OK;
}

STDMETHODIMP Machine::GetSerialPort (ULONG slot, ISerialPort **port)
{
    CheckComArgOutPointerValid (port);
    CheckComArgExpr (slot, slot < RT_ELEMENTS (mSerialPorts));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mSerialPorts [slot].queryInterfaceTo (port);

    return S_OK;
}

STDMETHODIMP Machine::GetParallelPort (ULONG slot, IParallelPort **port)
{
    CheckComArgOutPointerValid (port);
    CheckComArgExpr (slot, slot < RT_ELEMENTS (mParallelPorts));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mParallelPorts [slot].queryInterfaceTo (port);

    return S_OK;
}

STDMETHODIMP Machine::GetNetworkAdapter (ULONG slot, INetworkAdapter **adapter)
{
    CheckComArgOutPointerValid (adapter);
    CheckComArgExpr (slot, slot < RT_ELEMENTS (mNetworkAdapters));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mNetworkAdapters [slot].queryInterfaceTo (adapter);

    return S_OK;
}

/**
 *  @note Locks this object for reading.
 */
STDMETHODIMP Machine::GetNextExtraDataKey (IN_BSTR aKey, BSTR *aNextKey, BSTR *aNextValue)
{
    CheckComArgOutPointerValid (aNextKey);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* serialize file access (prevent writes) */
    AutoReadLock alock (this);

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
        using namespace xml;

        /* load the settings file (we don't reuse the existing handle but
         * request a new one to allow for concurrent multithreaded reads) */
        File file (File::Mode_Read, Utf8Str (mData->mConfigFileFull));
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
         * but none of them equals the requested non-NULL key. b) is an
         * error as well as a) if the key is non-NULL. When the key is NULL
         * (which is the case only when there are no items), we just fall
         * through to return NULLs and S_OK. */

        if (aKey != NULL)
            return setError (VBOX_E_OBJECT_NOT_FOUND,
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
STDMETHODIMP Machine::GetExtraData (IN_BSTR aKey, BSTR *aValue)
{
    CheckComArgNotNull (aKey);
    CheckComArgOutPointerValid (aValue);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* serialize file access (prevent writes) */
    AutoReadLock alock (this);

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
        using namespace xml;

        /* load the settings file (we don't reuse the existing handle but
         * request a new one to allow for concurrent multithreaded reads) */
        File file (File::Mode_Read, Utf8Str (mData->mConfigFileFull));
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
STDMETHODIMP Machine::SetExtraData (IN_BSTR aKey, IN_BSTR aValue)
{
    CheckComArgNotNull (aKey);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::onExtraDataCanChange() and saveSettings() need mParent
     * lock (saveSettings() needs a write one). This object's write lock is
     * also necessary to serialize file access (prevent concurrent reads and
     * writes). */
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
        rc = saveSettings();
        CheckComRCReturnRC (rc);
    }

    try
    {
        using namespace settings;
        using namespace xml;

        /* load the settings file */
        File file (mData->mHandleCfgFile, Utf8Str (mData->mConfigFileFull));
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
                CBSTR err = error.isNull() ? (CBSTR) L"" : error.raw();
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

    /* when there was auto-conversion, we want to save the file even if
     * the VM is saved */
    StateDependency dep = mData->mSettingsFileVersion != VBOX_XML_VERSION_FULL ?
        MutableOrSavedStateDep : MutableStateDep;

    HRESULT rc = checkStateDependency (dep);
    CheckComRCReturnRC (rc);

    /* the settings file path may never be null */
    ComAssertRet (mData->mConfigFileFull, E_FAIL);

    /* save all VM data excluding snapshots */
    return saveSettings();
}

STDMETHODIMP Machine::SaveSettingsWithBackup (BSTR *aBakFileName)
{
    CheckComArgOutPointerValid (aBakFileName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* saveSettings() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    /* when there was auto-conversion, we want to save the file even if
     * the VM is saved */
    StateDependency dep = mData->mSettingsFileVersion != VBOX_XML_VERSION_FULL ?
        MutableOrSavedStateDep : MutableStateDep;

    HRESULT rc = checkStateDependency (dep);
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

    AutoWriteLock alock (this);

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

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    if (mData->mRegistered)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Cannot delete settings of a registered machine"));

    /* delete the settings only when the file actually exists */
    lockConfig();
    if (isConfigLocked())
    {
        unlockConfig();
        int vrc = RTFileDelete (Utf8Str (mData->mConfigFileFull));
        if (RT_FAILURE (vrc))
            return setError (VBOX_E_IPRT_ERROR,
                tr ("Could not delete the settings file '%ls' (%Rrc)"),
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

STDMETHODIMP Machine::GetSnapshot (IN_GUID aId, ISnapshot **aSnapshot)
{
    CheckComArgOutPointerValid (aSnapshot);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Guid id = aId;
    ComObjPtr <Snapshot> snapshot;

    HRESULT rc = findSnapshot (id, snapshot, true /* aSetError */);
    snapshot.queryInterfaceTo (aSnapshot);

    return rc;
}

STDMETHODIMP Machine::FindSnapshot (IN_BSTR aName, ISnapshot **aSnapshot)
{
    CheckComArgNotNull (aName);
    CheckComArgOutPointerValid (aSnapshot);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    ComObjPtr <Snapshot> snapshot;

    HRESULT rc = findSnapshot (aName, snapshot, true /* aSetError */);
    snapshot.queryInterfaceTo (aSnapshot);

    return rc;
}

STDMETHODIMP Machine::SetCurrentSnapshot (IN_GUID /* aId */)
{
    /// @todo (dmik) don't forget to set
    //  mData->mCurrentStateModified to FALSE

    return setError (E_NOTIMPL, "Not implemented");
}

STDMETHODIMP
Machine::CreateSharedFolder (IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable)
{
    CheckComArgNotNull (aName);
    CheckComArgNotNull (aHostPath);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    ComObjPtr <SharedFolder> sharedFolder;
    rc = findSharedFolder (aName, sharedFolder, false /* aSetError */);
    if (SUCCEEDED (rc))
        return setError (VBOX_E_OBJECT_IN_USE,
            tr ("Shared folder named '%ls' already exists"), aName);

    sharedFolder.createObject();
    rc = sharedFolder->init (machine(), aName, aHostPath, aWritable);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    mHWData->mSharedFolders.push_back (sharedFolder);

    /* inform the direct session if any */
    alock.leave();
    onSharedFolderChange();

    return S_OK;
}

STDMETHODIMP Machine::RemoveSharedFolder (IN_BSTR aName)
{
    CheckComArgNotNull (aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

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
    CheckComArgOutPointerValid (aCanShow);

    /* start with No */
    *aCanShow = FALSE;

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReadLock alock (this);

        if (mData->mSession.mState != SessionState_Open)
            return setError (VBOX_E_INVALID_VM_STATE,
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
    CheckComArgOutPointerValid (aWinId);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReadLock alock (this);

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

STDMETHODIMP Machine::GetGuestProperty (IN_BSTR aName, BSTR *aValue, ULONG64 *aTimestamp, BSTR *aFlags)
{
#if !defined (VBOX_WITH_GUEST_PROPS)
    ReturnComNotImplemented();
#else
    CheckComArgNotNull (aName);
    CheckComArgOutPointerValid (aValue);
    CheckComArgOutPointerValid (aTimestamp);
    CheckComArgOutPointerValid (aFlags);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    using namespace guestProp;
    HRESULT rc = E_FAIL;

    if (!mHWData->mPropertyServiceActive)
    {
        bool found = false;
        for (HWData::GuestPropertyList::const_iterator it = mHWData->mGuestProperties.begin();
             (it != mHWData->mGuestProperties.end()) && !found; ++it)
        {
            if (it->mName == aName)
            {
                char szFlags[MAX_FLAGS_LEN + 1];
                it->mValue.cloneTo (aValue);
                *aTimestamp = it->mTimestamp;
                writeFlags (it->mFlags, szFlags);
                Bstr (szFlags).cloneTo (aFlags);
                found = true;
            }
        }
        rc = S_OK;
    }
    else
    {
        ComPtr <IInternalSessionControl> directControl =
            mData->mSession.mDirectControl;

        /* just be on the safe side when calling another process */
        alock.unlock();

        /* fail if we were called after #OnSessionEnd() is called.  This is a
         * silly race condition. */

        if (!directControl)
            rc = E_FAIL;
        else
            rc = directControl->AccessGuestProperty (aName, NULL, NULL,
                                                     false /* isSetter */,
                                                     aValue, aTimestamp, aFlags);
    }
    return rc;
#endif /* else !defined (VBOX_WITH_GUEST_PROPS) */
}

STDMETHODIMP Machine::GetGuestPropertyValue (IN_BSTR aName, BSTR *aValue)
{
    ULONG64 dummyTimestamp;
    BSTR dummyFlags;
    return GetGuestProperty (aName, aValue, &dummyTimestamp, &dummyFlags);
}

STDMETHODIMP Machine::GetGuestPropertyTimestamp (IN_BSTR aName, ULONG64 *aTimestamp)
{
    BSTR dummyValue;
    BSTR dummyFlags;
    return GetGuestProperty (aName, &dummyValue, aTimestamp, &dummyFlags);
}

STDMETHODIMP Machine::SetGuestProperty (IN_BSTR aName, IN_BSTR aValue, IN_BSTR aFlags)
{
#if !defined (VBOX_WITH_GUEST_PROPS)
    ReturnComNotImplemented();
#else
    using namespace guestProp;

    CheckComArgNotNull (aName);
    if ((aValue != NULL) && !VALID_PTR (aValue))
        return E_INVALIDARG;
    if ((aFlags != NULL) && !VALID_PTR (aFlags))
        return E_INVALIDARG;

    Utf8Str utf8Name (aName);
    Utf8Str utf8Flags (aFlags);
    Utf8Str utf8Patterns (mHWData->mGuestPropertyNotificationPatterns);
    if (   utf8Name.isNull()
        || ((aFlags != NULL) && utf8Flags.isNull())
        || utf8Patterns.isNull()
       )
        return E_OUTOFMEMORY;
    bool matchAll = false;
    if (0 == utf8Patterns.length())
        matchAll = true;

    uint32_t fFlags = NILFLAG;
    if ((aFlags != NULL) && RT_FAILURE (validateFlags (utf8Flags.raw(), &fFlags))
       )
        return setError (E_INVALIDARG, tr ("Invalid flag values: '%ls'"),
                aFlags);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    rc = S_OK;

    if (!mHWData->mPropertyServiceActive)
    {
        bool found = false;
        HWData::GuestProperty property;
        property.mFlags = NILFLAG;
        if (fFlags & TRANSIENT)
            rc = setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Cannot set a transient property when the "
                    "machine is not running"));
        if (SUCCEEDED (rc))
        {
            for (HWData::GuestPropertyList::iterator it =
                    mHWData->mGuestProperties.begin();
                 it != mHWData->mGuestProperties.end(); ++ it)
                if (it->mName == aName)
                {
                    property = *it;
                    if (it->mFlags & (RDONLYHOST))
                        rc = setError (E_ACCESSDENIED,
                            tr ("The property '%ls' cannot be changed by the host"),
                            aName);
                    else
                    {
                        mHWData.backup();
                        /* The backup() operation invalidates our iterator, so
                         * get a new one. */
                        for (it = mHWData->mGuestProperties.begin();
                            it->mName != aName; ++ it)
                            ;
                        mHWData->mGuestProperties.erase (it);
                    }
                    found = true;
                    break;
                }
        }
        if (found && SUCCEEDED (rc))
        {
            if (aValue != NULL)
            {
                RTTIMESPEC time;
                property.mValue = aValue;
                property.mTimestamp = RTTimeSpecGetNano (RTTimeNow (&time));
                if (aFlags != NULL)
                    property.mFlags = fFlags;
                mHWData->mGuestProperties.push_back (property);
            }
        }
        else if (SUCCEEDED (rc) && (aValue != NULL))
        {
            RTTIMESPEC time;
            mHWData.backup();
            property.mName = aName;
            property.mValue = aValue;
            property.mTimestamp = RTTimeSpecGetNano (RTTimeNow (&time));
            property.mFlags = fFlags;
            mHWData->mGuestProperties.push_back (property);
        }
        if (   SUCCEEDED (rc)
            && (   matchAll
                || RTStrSimplePatternMultiMatch (utf8Patterns.raw(), RTSTR_MAX,
                                                utf8Name.raw(), RTSTR_MAX, NULL)
              )
          )
            mParent->onGuestPropertyChange (mData->mUuid, aName, aValue, aFlags);
    }
    else
    {
        ComPtr <IInternalSessionControl> directControl =
            mData->mSession.mDirectControl;

        /* just be on the safe side when calling another process */
        alock.leave();

        BSTR dummy = NULL;
        ULONG64 dummy64;
        if (!directControl)
            rc = E_FAIL;
        else
            rc = directControl->AccessGuestProperty (aName, aValue, aFlags,
                                                     true /* isSetter */,
                                                     &dummy, &dummy64, &dummy);
    }
    return rc;
#endif /* else !defined (VBOX_WITH_GUEST_PROPS) */
}

STDMETHODIMP Machine::SetGuestPropertyValue (IN_BSTR aName, IN_BSTR aValue)
{
    return SetGuestProperty (aName, aValue, NULL);
}

STDMETHODIMP Machine::
EnumerateGuestProperties (IN_BSTR aPatterns, ComSafeArrayOut (BSTR, aNames),
                          ComSafeArrayOut (BSTR, aValues),
                          ComSafeArrayOut (ULONG64, aTimestamps),
                          ComSafeArrayOut (BSTR, aFlags))
{
#if !defined (VBOX_WITH_GUEST_PROPS)
    ReturnComNotImplemented();
#else
    if (!VALID_PTR (aPatterns) && (aPatterns != NULL))
        return E_POINTER;

    CheckComArgOutSafeArrayPointerValid (aNames);
    CheckComArgOutSafeArrayPointerValid (aValues);
    CheckComArgOutSafeArrayPointerValid (aTimestamps);
    CheckComArgOutSafeArrayPointerValid (aFlags);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    using namespace guestProp;
    HRESULT rc = E_FAIL;

    bool matchAll = false;
    if ((NULL == aPatterns) || (0 == aPatterns[0]))
        matchAll = true;
    if (!mHWData->mPropertyServiceActive)
    {

/*
 * Look for matching patterns and build up a list.
 */
        HWData::GuestPropertyList propList;
        for (HWData::GuestPropertyList::iterator it = mHWData->mGuestProperties.begin();
             it != mHWData->mGuestProperties.end(); ++it)
            if (   matchAll
                || RTStrSimplePatternMultiMatch (Utf8Str (aPatterns).raw(),
                                                 RTSTR_MAX,
                                                 Utf8Str (it->mName).raw(),
                                                 RTSTR_MAX, NULL)
               )
                propList.push_back (*it);

/*
 * And build up the arrays for returning the property information.
 */
        size_t cEntries = propList.size();
        SafeArray <BSTR> names (cEntries);
        SafeArray <BSTR> values (cEntries);
        SafeArray <ULONG64> timestamps (cEntries);
        SafeArray <BSTR> flags (cEntries);
        size_t iProp = 0;
        for (HWData::GuestPropertyList::iterator it = propList.begin();
             it != propList.end(); ++it)
        {
             char szFlags[MAX_FLAGS_LEN + 1];
             it->mName.cloneTo (&names[iProp]);
             it->mValue.cloneTo (&values[iProp]);
             timestamps[iProp] = it->mTimestamp;
             writeFlags (it->mFlags, szFlags);
             Bstr (szFlags).cloneTo (&flags[iProp]);
             ++iProp;
        }
        names.detachTo (ComSafeArrayOutArg (aNames));
        values.detachTo (ComSafeArrayOutArg (aValues));
        timestamps.detachTo (ComSafeArrayOutArg (aTimestamps));
        flags.detachTo (ComSafeArrayOutArg (aFlags));
        rc = S_OK;
    }
    else
    {
        ComPtr <IInternalSessionControl> directControl =
            mData->mSession.mDirectControl;

        /* just be on the safe side when calling another process */
        alock.unlock();

        if (!directControl)
            rc = E_FAIL;
        else
            rc = directControl->EnumerateGuestProperties (aPatterns,
                                                          ComSafeArrayOutArg (aNames),
                                                          ComSafeArrayOutArg (aValues),
                                                          ComSafeArrayOutArg (aTimestamps),
                                                          ComSafeArrayOutArg (aFlags));
    }
    return rc;
#endif /* else !defined (VBOX_WITH_GUEST_PROPS) */
}

STDMETHODIMP Machine::
GetHardDiskAttachmentsOfController(IN_BSTR aName, ComSafeArrayOut (IHardDiskAttachment *, aAttachments))
{
    HDData::AttachmentList atts;

    HRESULT rc = getHardDiskAttachmentsOfController(aName, atts);
    CheckComRCReturnRC(rc);

    SafeIfaceArray<IHardDiskAttachment> attachments (atts);
    attachments.detachTo (ComSafeArrayOutArg (aAttachments));

    return S_OK;
}

STDMETHODIMP Machine::
AddStorageController(IN_BSTR aName,
                     StorageBus_T aConnectionType,
                     IStorageController **controller)
{
    CheckComArgStrNotEmptyOrNull(aName);

    if (   (aConnectionType <= StorageBus_Null)
        || (aConnectionType >  StorageBus_SCSI))
        return setError (E_INVALIDARG,
            tr ("Invalid connection type: %d"),
                aConnectionType);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    /* try to find one with the name first. */
    ComObjPtr<StorageController> ctrl;

    rc = getStorageControllerByName (aName, ctrl, false /* aSetError */);
    if (SUCCEEDED (rc))
        return setError (VBOX_E_OBJECT_IN_USE,
            tr ("Storage controller named '%ls' already exists"), aName);

    ctrl.createObject();
    rc = ctrl->init (this, aName, aConnectionType);
    CheckComRCReturnRC (rc);

    mStorageControllers.backup();
    mStorageControllers->push_back (ctrl);

    ctrl.queryInterfaceTo(controller);

    /* inform the direct session if any */
    alock.leave();
    onStorageControllerChange();

    return S_OK;
}

STDMETHODIMP Machine::
GetStorageControllerByName(IN_BSTR aName, IStorageController **aStorageController)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    ComObjPtr <StorageController> ctrl;

    HRESULT rc = getStorageControllerByName (aName, ctrl, true /* aSetError */);
    if (SUCCEEDED (rc))
        ctrl.queryInterfaceTo (aStorageController);

    return rc;
}

STDMETHODIMP Machine::
RemoveStorageController(IN_BSTR aName)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    ComObjPtr <StorageController> ctrl;
    rc = getStorageControllerByName (aName, ctrl, true /* aSetError */);
    CheckComRCReturnRC (rc);

    /* We can remove the controller only if there is no device attached. */
    /* check if the device slot is already busy */
    for (HDData::AttachmentList::const_iterator
         it = mHDData->mAttachments.begin();
         it != mHDData->mAttachments.end();
         ++ it)
    {
        if (it != mHDData->mAttachments.end())
        {
            if ((*it)->controller() == aName)
                return setError (VBOX_E_OBJECT_IN_USE,
                    tr ("Storage controller named '%ls' has still devices attached"), aName);
        }
    }

    /* We can remove it now. */
    mStorageControllers.backup();

    ctrl->unshare();

    mStorageControllers->remove (ctrl);

    /* inform the direct session if any */
    alock.leave();
    onStorageControllerChange();

    return S_OK;
}

// public methods for internal purposes
/////////////////////////////////////////////////////////////////////////////

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

    AutoReadLock alock (this);

    /* UUID */
    aEntryNode.setValue <Guid> ("uuid", mData->mUuid);
    /* settings file name (possibly, relative) */
    aEntryNode.setValue <Bstr> ("src", mData->mConfigFile);

    return S_OK;
}

/**
 * Calculates the absolute path of the given path taking the directory of the
 * machine settings file as the current directory.
 *
 * @param  aPath    Path to calculate the absolute path for.
 * @param  aResult  Where to put the result (used only on success, can be the
 *                  same Utf8Str instance as passed in @a aPath).
 * @return IPRT result.
 *
 * @note Locks this object for reading.
 */
int Machine::calculateFullPath (const char *aPath, Utf8Str &aResult)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);

    AssertReturn (!mData->mConfigFileFull.isNull(), VERR_GENERAL_FAILURE);

    Utf8Str settingsDir = mData->mConfigFileFull;

    RTPathStripFilename (settingsDir.mutableRaw());
    char folder [RTPATH_MAX];
    int vrc = RTPathAbsEx (settingsDir, aPath, folder, sizeof (folder));
    if (RT_SUCCESS (vrc))
        aResult = folder;

    return vrc;
}

/**
 * Tries to calculate the relative path of the given absolute path using the
 * directory of the machine settings file as the base directory.
 *
 * @param  aPath    Absolute path to calculate the relative path for.
 * @param  aResult  Where to put the result (used only when it's possible to
 *                  make a relative path from the given absolute path; otherwise
 *                  left untouched).
 *
 * @note Locks this object for reading.
 */
void Machine::calculateRelativePath (const char *aPath, Utf8Str &aResult)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    AutoReadLock alock (this);

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

    AutoReadLock alock (this);

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
 *  @note Locks this object for writing, calls the client process (outside the
 *        lock).
 */
HRESULT Machine::openSession (IInternalSessionControl *aControl)
{
    LogFlowThisFuncEnter();

    AssertReturn (aControl, E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (!mData->mRegistered)
        return setError (E_UNEXPECTED,
            tr ("The machine '%ls' is not registered"), mUserData->mName.raw());

    LogFlowThisFunc (("mSession.mState=%d\n", mData->mSession.mState));

    if (mData->mSession.mState == SessionState_Open ||
        mData->mSession.mState == SessionState_Closing)
        return setError (VBOX_E_INVALID_OBJECT_STATE,
            tr ("A session for the machine '%ls' is currently open "
                "(or being closed)"),
            mUserData->mName.raw());

    /* may not be busy */
    AssertReturn (!Global::IsOnlineOrTransient (mData->mMachineState), E_FAIL);

    /* get the session PID */
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

    /* NOTE: doing return from this function after this point but
     * before the end is forbidden since it may call SessionMachine::uninit()
     * (through the ComObjPtr's destructor) which requests the VirtualBox write
     * lock while still holding the Machine lock in alock so that a deadlock
     * is possible due to the wrong lock order. */

    if (SUCCEEDED (rc))
    {
#ifdef VBOX_WITH_RESOURCE_USAGE_API
        registerMetrics (mParent->performanceCollector(), this, pid);
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

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

        /* The failure may occur w/o any error info (from RPC), so provide one */
        if (FAILED (rc))
            setError (VBOX_E_VM_ERROR,
                tr ("Failed to assign the machine to the session (%Rrc)"), rc);

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

            /* assign machine & console to the remote session */
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

                /* The failure may occur w/o any error info (from RPC), so provide one */
                if (FAILED (rc))
                    setError (VBOX_E_VM_ERROR,
                        tr ("Failed to assign the machine to the remote session (%Rrc)"), rc);
            }

            if (FAILED (rc))
                aControl->Uninitialize();
        }

        /* enter the lock again */
        alock.enter();

        /* Restore the session state */
        mData->mSession.mState = origState;
    }

    /* finalize spawning anyway (this is why we don't return on errors above) */
    if (mData->mSession.mState == SessionState_Spawning)
    {
        /* Note that the progress object is finalized later */

        /* We don't reset mSession.mPid here because it is necessary for
         * SessionMachine::uninit() to reap the child process later. */

        if (FAILED (rc))
        {
            /* Close the remote session, remove the remote control from the list
             * and reset session state to Closed (@note keep the code in sync
             * with the relevant part in openSession()). */

            Assert (mData->mSession.mRemoteControls.size() == 1);
            if (mData->mSession.mRemoteControls.size() == 1)
            {
                ErrorInfoKeeper eik;
                mData->mSession.mRemoteControls.front()->Uninitialize();
            }

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

    /* Leave the lock since SessionMachine::uninit() locks VirtualBox which
     * would break the lock order */
    alock.leave();

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
                                    IN_BSTR aType, IN_BSTR aEnvironment,
                                    Progress *aProgress)
{
    LogFlowThisFuncEnter();

    AssertReturn (aControl, E_FAIL);
    AssertReturn (aProgress, E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (!mData->mRegistered)
        return setError (E_UNEXPECTED,
            tr ("The machine '%ls' is not registered"), mUserData->mName.raw());

    LogFlowThisFunc (("mSession.mState=%d\n", mData->mSession.mState));

    if (mData->mSession.mState == SessionState_Open ||
        mData->mSession.mState == SessionState_Spawning ||
        mData->mSession.mState == SessionState_Closing)
        return setError (VBOX_E_INVALID_OBJECT_STATE,
            tr ("A session for the machine '%ls' is currently open "
                "(or being opened or closed)"),
            mUserData->mName.raw());

    /* may not be busy */
    AssertReturn (!Global::IsOnlineOrTransient (mData->mMachineState), E_FAIL);

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
            AssertRCBreakStmt (vrc2, vrc = vrc2);

            newEnvStr = RTStrDup (Utf8Str (aEnvironment));
            AssertPtrBreakStmt (newEnvStr, vrc = vrc2);

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
                        if (RT_FAILURE (vrc2))
                            break;
                    }
                    var = p + 1;
                }
            }
            if (RT_SUCCESS (vrc2) && *var)
                vrc2 = RTEnvPutEx (env, var);

            AssertRCBreakStmt (vrc2, vrc = vrc2);
        }
        while (0);

        if (newEnvStr != NULL)
            RTStrFree (newEnvStr);
    }

    Bstr type (aType);

    /* Qt is default */
#ifdef VBOX_WITH_QTGUI
    if (type == "gui" || type == "GUI/Qt")
    {
# ifdef RT_OS_DARWIN /* Avoid Launch Services confusing this with the selector by using a helper app. */
        const char VirtualBox_exe[] = "../Resources/VirtualBoxVM.app/Contents/MacOS/VirtualBoxVM";
# else
        const char VirtualBox_exe[] = "VirtualBox" HOSTSUFF_EXE;
# endif
        Assert (sz >= sizeof (VirtualBox_exe));
        strcpy (cmd, VirtualBox_exe);

        Utf8Str idStr = mData->mUuid.toString();
# ifdef RT_OS_WINDOWS /** @todo drop this once the RTProcCreate bug has been fixed */
        const char * args[] = {path, "--startvm", idStr, 0 };
# else
        Utf8Str name = mUserData->mName;
        const char * args[] = {path, "--comment", name, "--startvm", idStr, 0 };
# endif
        vrc = RTProcCreate (path, args, env, 0, &pid);
    }
#else /* !VBOX_WITH_QTGUI */
    if (0)
        ;
#endif /* VBOX_WITH_QTGUI */

    else

#ifdef VBOX_WITH_VBOXSDL
    if (type == "sdl" || type == "GUI/SDL")
    {
        const char VBoxSDL_exe[] = "VBoxSDL" HOSTSUFF_EXE;
        Assert (sz >= sizeof (VBoxSDL_exe));
        strcpy (cmd, VBoxSDL_exe);

        Utf8Str idStr = mData->mUuid.toString();
# ifdef RT_OS_WINDOWS
        const char * args[] = {path, "--startvm", idStr, 0 };
# else
        Utf8Str name = mUserData->mName;
        const char * args[] = {path, "--comment", name, "--startvm", idStr, 0 };
# endif
        vrc = RTProcCreate (path, args, env, 0, &pid);
    }
#else /* !VBOX_WITH_VBOXSDL */
    if (0)
        ;
#endif /* !VBOX_WITH_VBOXSDL */

    else

#ifdef VBOX_WITH_VRDP
    if (type == "vrdp")
    {
        const char VBoxVRDP_exe[] = "VBoxHeadless" HOSTSUFF_EXE;
        Assert (sz >= sizeof (VBoxVRDP_exe));
        strcpy (cmd, VBoxVRDP_exe);

        Utf8Str idStr = mData->mUuid.toString();
# ifdef RT_OS_WINDOWS
        const char * args[] = {path, "--startvm", idStr, 0 };
# else
        Utf8Str name = mUserData->mName;
        const char * args[] = {path, "--comment", name, "--startvm", idStr, 0 };
# endif
        vrc = RTProcCreate (path, args, env, 0, &pid);
    }
#else /* !VBOX_WITH_VRDP */
    if (0)
        ;
#endif /* !VBOX_WITH_VRDP */

    else

#ifdef VBOX_WITH_HEADLESS
    if (type == "capture")
    {
        const char VBoxVRDP_exe[] = "VBoxHeadless" HOSTSUFF_EXE;
        Assert (sz >= sizeof (VBoxVRDP_exe));
        strcpy (cmd, VBoxVRDP_exe);

        Utf8Str idStr = mData->mUuid.toString();
# ifdef RT_OS_WINDOWS
        const char * args[] = {path, "--startvm", idStr, "--capture", 0 };
# else
        Utf8Str name = mUserData->mName;
        const char * args[] = {path, "--comment", name, "--startvm", idStr, "--capture", 0 };
# endif
        vrc = RTProcCreate (path, args, env, 0, &pid);
    }
#else /* !VBOX_WITH_HEADLESS */
    if (0)
        ;
#endif /* !VBOX_WITH_HEADLESS */
    else
    {
        RTEnvDestroy (env);
        return setError (E_INVALIDARG,
            tr ("Invalid session type: '%ls'"), aType);
    }

    RTEnvDestroy (env);

    if (RT_FAILURE (vrc))
        return setError (VBOX_E_IPRT_ERROR,
            tr ("Could not launch a process for the machine '%ls' (%Rrc)"),
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
        /* The failure may occur w/o any error info (from RPC), so provide one */
        return setError (VBOX_E_VM_ERROR,
            tr ("Failed to assign the machine to the session (%Rrc)"), rc);
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

    AutoWriteLock alock (this);

    if (!mData->mRegistered)
        return setError (E_UNEXPECTED,
            tr ("The machine '%ls' is not registered"), mUserData->mName.raw());

    LogFlowThisFunc (("mSession.state=%d\n", mData->mSession.mState));

    if (mData->mSession.mState != SessionState_Open)
        return setError (VBOX_E_INVALID_SESSION_STATE,
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
        /* The failure may occur w/o any error info (from RPC), so provide one */
        return setError (VBOX_E_VM_ERROR,
            tr ("Failed to get a console object from the direct session (%Rrc)"), rc);
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

    /* The failure may occur w/o any error info (from RPC), so provide one */
    if (FAILED (rc))
        return setError (VBOX_E_VM_ERROR,
            tr ("Failed to assign the machine to the session (%Rrc)"), rc);

    alock.enter();

    /* need to revalidate the state after entering the lock again */
    if (mData->mSession.mState != SessionState_Open)
    {
        aControl->Uninitialize();

        return setError (VBOX_E_INVALID_SESSION_STATE,
            tr ("The machine '%ls' does not have an open session"),
            mUserData->mName.raw());
    }

    /* store the control in the list */
    mData->mSession.mRemoteControls.push_back (aControl);

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 * Returns @c true if the given machine has an open direct session and returns
 * the session machine instance and additional session data (on some platforms)
 * if so.
 *
 * Note that when the method returns @c false, the arguments remain unchanged.
 *
 * @param aMachine  Session machine object.
 * @param aControl  Direct session control object (optional).
 * @param aIPCSem   Mutex IPC semaphore handle for this machine (optional).
 *
 * @note locks this object for reading.
 */
#if defined (RT_OS_WINDOWS)
bool Machine::isSessionOpen (ComObjPtr <SessionMachine> &aMachine,
                             ComPtr <IInternalSessionControl> *aControl /*= NULL*/,
                             HANDLE *aIPCSem /*= NULL*/,
                             bool aAllowClosing /*= false*/)
#elif defined (RT_OS_OS2)
bool Machine::isSessionOpen (ComObjPtr <SessionMachine> &aMachine,
                             ComPtr <IInternalSessionControl> *aControl /*= NULL*/,
                             HMTX *aIPCSem /*= NULL*/,
                             bool aAllowClosing /*= false*/)
#else
bool Machine::isSessionOpen (ComObjPtr <SessionMachine> &aMachine,
                             ComPtr <IInternalSessionControl> *aControl /*= NULL*/,
                             bool aAllowClosing /*= false*/)
#endif
{
    AutoLimitedCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    /* just return false for inaccessible machines */
    if (autoCaller.state() != Ready)
        return false;

    AutoReadLock alock (this);

    if (mData->mSession.mState == SessionState_Open ||
        (aAllowClosing && mData->mSession.mState == SessionState_Closing))
    {
        AssertReturn (!mData->mSession.mMachine.isNull(), false);

        aMachine = mData->mSession.mMachine;

        if (aControl != NULL)
            *aControl = mData->mSession.mDirectControl;

#if defined (RT_OS_WINDOWS) || defined (RT_OS_OS2)
        /* Additional session data */
        if (aIPCSem != NULL)
            *aIPCSem = aMachine->mIPCSem;
#endif
        return true;
    }

    return false;
}

/**
 * Returns @c true if the given machine has an spawning direct session and
 * returns and additional session data (on some platforms) if so.
 *
 * Note that when the method returns @c false, the arguments remain unchanged.
 *
 * @param aPID  PID of the spawned direct session process.
 *
 * @note locks this object for reading.
 */
#if defined (RT_OS_WINDOWS) || defined (RT_OS_OS2)
bool Machine::isSessionSpawning (RTPROCESS *aPID /*= NULL*/)
#else
bool Machine::isSessionSpawning()
#endif
{
    AutoLimitedCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    /* just return false for inaccessible machines */
    if (autoCaller.state() != Ready)
        return false;

    AutoReadLock alock (this);

    if (mData->mSession.mState == SessionState_Spawning)
    {
#if defined (RT_OS_WINDOWS) || defined (RT_OS_OS2)
        /* Additional session data */
        if (aPID != NULL)
        {
            AssertReturn (mData->mSession.mPid != NIL_RTPROCESS, false);
            *aPID = mData->mSession.mPid;
        }
#endif
        return true;
    }

    return false;
}

/**
 * Called from the client watcher thread to check for unexpected client process
 * death during Session_Spawning state (e.g. before it successfully opened a
 * direct session).
 *
 * On Win32 and on OS/2, this method is called only when we've got the
 * direct client's process termination notification, so it always returns @c
 * true.
 *
 * On other platforms, this method returns @c true if the client process is
 * terminated and @c false if it's still alive.
 *
 * @note Locks this object for writing.
 */
bool Machine::checkForSpawnFailure()
{
    AutoCaller autoCaller (this);
    if (!autoCaller.isOk())
    {
        /* nothing to do */
        LogFlowThisFunc (("Already uninitialized!"));
        return true;
    }

    /* VirtualBox::addProcessToReap() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    if (mData->mSession.mState != SessionState_Spawning)
    {
        /* nothing to do */
        LogFlowThisFunc (("Not spawning any more!"));
        return true;
    }

    HRESULT rc = S_OK;

#if defined (RT_OS_WINDOWS) || defined (RT_OS_OS2)

    /* the process was already unexpectedly terminated, we just need to set an
     * error and finalize session spawning */
    rc = setError (E_FAIL,
                   tr ("Virtual machine '%ls' has terminated unexpectedly "
                       "during startup"),
                   name().raw());
#else

    RTPROCSTATUS status;
    int vrc = ::RTProcWait (mData->mSession.mPid, RTPROCWAIT_FLAGS_NOBLOCK,
                            &status);

    if (vrc != VERR_PROCESS_RUNNING)
        rc = setError (E_FAIL,
                       tr ("Virtual machine '%ls' has terminated unexpectedly "
                           "during startup"),
                       name().raw());
#endif

    if (FAILED (rc))
    {
        /* Close the remote session, remove the remote control from the list
         * and reset session state to Closed (@note keep the code in sync with
         * the relevant part in checkForSpawnFailure()). */

        Assert (mData->mSession.mRemoteControls.size() == 1);
        if (mData->mSession.mRemoteControls.size() == 1)
        {
            ErrorInfoKeeper eik;
            mData->mSession.mRemoteControls.front()->Uninitialize();
        }

        mData->mSession.mRemoteControls.clear();
        mData->mSession.mState = SessionState_Closed;

        /* finalize the progress after setting the state, for consistency */
        mData->mSession.mProgress->notifyComplete (rc);
        mData->mSession.mProgress.setNull();

        mParent->addProcessToReap (mData->mSession.mPid);
        mData->mSession.mPid = NIL_RTPROCESS;

        mParent->onSessionStateChange (mData->mUuid, SessionState_Closed);
        return true;
    }

    return false;
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
    AssertReturn (mParent->isWriteLockOnCurrentThread(), E_FAIL);

    AutoLimitedCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* wait for state dependants to drop to zero */
    ensureNoStateDependencies();

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

    /* we will probably modify these and want to prevent concurrent
     * modifications until we finish */
    AutoWriteLock dvdLock (mDVDDrive);
    AutoWriteLock floppyLock (mFloppyDrive);

    if (aRegistered)
    {
        if (mData->mRegistered)
            return setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("The machine '%ls' with UUID {%s} is already registered"),
                mUserData->mName.raw(),
                mData->mUuid.toString().raw());
    }
    else
    {
        if (mData->mMachineState == MachineState_Saved)
            return setError (VBOX_E_INVALID_VM_STATE,
                tr ("Cannot unregister the machine '%ls' because it "
                    "is in the Saved state"),
                mUserData->mName.raw());

        size_t snapshotCount = 0;
        if (mData->mFirstSnapshot)
            snapshotCount = mData->mFirstSnapshot->descendantCount() + 1;
        if (snapshotCount)
            return setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Cannot unregister the machine '%ls' because it "
                    "has %d snapshots"),
                mUserData->mName.raw(), snapshotCount);

        if (mData->mSession.mState != SessionState_Closed)
            return setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Cannot unregister the machine '%ls' because it has an "
                    "open session"),
                mUserData->mName.raw());

        if (mHDData->mAttachments.size() != 0)
            return setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Cannot unregister the machine '%ls' because it "
                    "has %d hard disks attached"),
                mUserData->mName.raw(), mHDData->mAttachments.size());

        /* Note that we do not prevent unregistration of a DVD or Floppy image
         * is attached: as opposed to hard disks detaching such an image
         * implicitly in this method (which we will do below) won't have any
         * side effects (like detached orphan base and diff hard disks etc).*/
    }

    HRESULT rc = S_OK;

    /* Ensure the settings are saved. If we are going to be registered and
     * isConfigLocked() is FALSE then it means that no config file exists yet,
     * so create it by calling saveSettings() too. */
    if (isModified() || (aRegistered && !isConfigLocked()))
    {
        rc = saveSettings();
        CheckComRCReturnRC (rc);
    }

    /* Implicitly detach DVD/Floppy */
    rc = mDVDDrive->unmount();
    if (SUCCEEDED (rc))
        rc = mFloppyDrive->unmount();

    if (SUCCEEDED (rc))
    {
        /* we may have had implicit modifications we want to fix on success */
        commit();

        mData->mRegistered = aRegistered;
    }
    else
    {
        /* we may have had implicit modifications we want to cancel on failure*/
        rollback (false /* aNotify */);
    }

    return rc;
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
 * @note Locks this object for writing.
 */
HRESULT Machine::addStateDependency (StateDependency aDepType /* = AnyStateDep */,
                                     MachineState_T *aState /* = NULL */,
                                     BOOL *aRegistered /* = NULL */)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (aDepType);
    CheckComRCReturnRC (rc);

    {
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
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock (this);

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
                return setError (VBOX_E_INVALID_VM_STATE,
                    tr ("The machine is not mutable (state is %d)"),
                    mData->mMachineState);
            break;
        }
        case MutableOrSavedStateDep:
        {
            if (mData->mRegistered &&
                (mType != IsSessionMachine ||
                 mData->mMachineState > MachineState_Paused))
                return setError (VBOX_E_INVALID_VM_STATE,
                    tr ("The machine is not mutable (state is %d)"),
                    mData->mMachineState);
            break;
        }
    }

    return S_OK;
}

/**
 * Helper to initialize all associated child objects and allocate data
 * structures.
 *
 * This method must be called as a part of the object's initialization procedure
 * (usually done in the #init() method).
 *
 * @note Must be called only from #init() or from #registeredInit().
 */
HRESULT Machine::initDataAndChildObjects()
{
    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());
    AssertComRCReturn (autoCaller.state() == InInit ||
                       autoCaller.state() == Limited, E_FAIL);

    AssertReturn (!mData->mAccessible, E_FAIL);

    /* allocate data structures */
    mSSData.allocate();
    mUserData.allocate();
    mHWData.allocate();
    mHDData.allocate();
    mStorageControllers.allocate();

    /* initialize mOSTypeId */
    mUserData->mOSTypeId = mParent->getUnknownOSType()->id();

    /* create associated BIOS settings object */
    unconst (mBIOSSettings).createObject();
    mBIOSSettings->init (this);

#ifdef VBOX_WITH_VRDP
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
    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
    {
        unconst (mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->init (this, slot);
    }

    /* create associated parallel port objects */
    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
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

    /* create associated network adapter objects */
    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
    {
        unconst (mNetworkAdapters [slot]).createObject();
        mNetworkAdapters [slot]->init (this, slot);
    }

    return S_OK;
}

/**
 * Helper to uninitialize all associated child objects and to free all data
 * structures.
 *
 * This method must be called as a part of the object's uninitialization
 * procedure (usually done in the #uninit() method).
 *
 * @note Must be called only from #uninit() or from #registeredInit().
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

    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
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

    if (mAudioAdapter)
    {
        mAudioAdapter->uninit();
        unconst (mAudioAdapter).setNull();
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
    {
        if (mParallelPorts [slot])
        {
            mParallelPorts [slot]->uninit();
            unconst (mParallelPorts [slot]).setNull();
        }
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
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

#ifdef VBOX_WITH_VRDP
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
        for (HDData::AttachmentList::const_iterator it =
                 mHDData->mAttachments.begin();
             it != mHDData->mAttachments.end();
             ++ it)
        {
            HRESULT rc = (*it)->hardDisk()->detachFrom (mData->mUuid,
                                                        snapshotId());
            AssertComRC (rc);
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
    mStorageControllers.free();
    mHWData.free();
    mUserData.free();
    mSSData.free();
}

/**
 * Makes sure that there are no machine state dependants. If necessary, waits
 * for the number of dependants to drop to zero.
 *
 * Make sure this method is called from under this object's write lock to
 * guarantee that no new dependants may be added when this method returns
 * control to the caller.
 *
 * @note Locks this object for writing. The lock will be released while waiting
 *       (if necessary).
 *
 * @warning To be used only in methods that change the machine state!
 */
void Machine::ensureNoStateDependencies()
{
    AssertReturnVoid (isWriteLockOnCurrentThread());

    AutoWriteLock alock (this);

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

        alock.leave();

        RTSemEventMultiWait (mData->mMachineStateDepsSem, RT_INDEFINITE_WAIT);

        alock.enter();

        -- mData->mMachineStateChangePending;
    }
}

/**
 * Changes the machine state and informs callbacks.
 *
 * This method is not intended to fail so it either returns S_OK or asserts (and
 * returns a failure).
 *
 * @note Locks this object for writing.
 */
HRESULT Machine::setMachineState (MachineState_T aMachineState)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aMachineState=%d\n", aMachineState));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    /* wait for state dependants to drop to zero */
    ensureNoStateDependencies();

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
 *      S_OK when found or VBOX_E_OBJECT_NOT_FOUND when not found
 *
 *  @note
 *      must be called from under the object's lock!
 */
HRESULT Machine::findSharedFolder (CBSTR aName,
                                   ComObjPtr <SharedFolder> &aSharedFolder,
                                   bool aSetError /* = false */)
{
    bool found = false;
    for (HWData::SharedFolderList::const_iterator it = mHWData->mSharedFolders.begin();
        !found && it != mHWData->mSharedFolders.end();
        ++ it)
    {
        AutoWriteLock alock (*it);
        found = (*it)->name() == aName;
        if (found)
            aSharedFolder = *it;
    }

    HRESULT rc = found ? S_OK : VBOX_E_OBJECT_NOT_FOUND;

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
        using namespace xml;

        /* no concurrent file access is possible in init() so open by handle */
        File file (mData->mHandleCfgFile, Utf8Str (mData->mConfigFileFull));
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
                    tr ("Machine UUID {%RTuuid} in '%ls' doesn't match its "
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
                if (RT_FAILURE (vrc))
                {
                    throw setError (E_FAIL,
                        tr ("Invalid saved state file path '%ls' (%Rrc)"),
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
            rc = COMSETTER (SnapshotFolder) (folder);
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

        Key hardwareNode = machineNode.key("Hardware");

        /* Hardware node (required) */
        rc = loadHardware (hardwareNode);
        CheckComRCThrowRC (rc);

        /* Load storage controllers */
        rc = loadStorageControllers (machineNode.key ("StorageControllers"), aRegistered);
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
            if (RT_FAILURE (vrc))
                return setError (E_FAIL,
                                 tr ("Invalid saved state file path '%ls' (%Rrc)"),
                                 stateFilePath.raw(), vrc);

            stateFilePath = stateFilePathFull;
        }

        /* Hardware node (required) */
        Key hardwareNode = aNode.key ("Hardware");

        /* StorageControllers node (required) */
        Key storageNode = aNode.key ("StorageControllers");

        /* initialize the snapshot machine */
        rc = snapshotMachine->init (this, hardwareNode, storageNode,
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

    /* The hardware version attribute (optional). */
    mHWData->mHWVersion = aNode.stringValue ("version");

    /* CPU node (currently not required) */
    {
        /* default value in case the node is not there */
        mHWData->mHWVirtExEnabled             = TSBool_Default;
        mHWData->mHWVirtExNestedPagingEnabled = false;
        mHWData->mHWVirtExVPIDEnabled         = false;
        mHWData->mPAEEnabled                  = false;

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
            /* HardwareVirtExNestedPaging (optional, default is false) */
            Key HWVirtExNestedPagingNode = cpuNode.findKey ("HardwareVirtExNestedPaging");
            if (!HWVirtExNestedPagingNode.isNull())
            {
                mHWData->mHWVirtExNestedPagingEnabled = HWVirtExNestedPagingNode.value <bool> ("enabled");
            }

            /* HardwareVirtExVPID (optional, default is false) */
            Key HWVirtExVPIDNode = cpuNode.findKey ("HardwareVirtExVPID");
            if (!HWVirtExVPIDNode.isNull())
            {
                mHWData->mHWVirtExVPIDEnabled = HWVirtExVPIDNode.value <bool> ("enabled");
            }

            /* PAE (optional, default is false) */
            Key PAENode = cpuNode.findKey ("PAE");
            if (!PAENode.isNull())
            {
                mHWData->mPAEEnabled = PAENode.value <bool> ("enabled");
            }

            /* CPUCount (optional, default is 1) */
            mHWData->mCPUCount = cpuNode.value <ULONG> ("count");
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
        for (size_t i = 0; i < RT_ELEMENTS (mHWData->mBootOrder); i++)
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
            Assert (position < RT_ELEMENTS (mHWData->mBootOrder));

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
                ComAssertMsgFailed (("Invalid device: %s", device));
        }
    }

    /* Display node (required) */
    {
        Key displayNode = aNode.key ("Display");

        mHWData->mVRAMSize      = displayNode.value <ULONG> ("VRAMSize");
        mHWData->mMonitorCount  = displayNode.value <ULONG> ("monitorCount");
        mHWData->mAccelerate3DEnabled = displayNode.value <bool> ("accelerate3D");
    }

#ifdef VBOX_WITH_VRDP
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
            AssertBreak (slot < RT_ELEMENTS (mNetworkAdapters));

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
            AssertBreak (slot < RT_ELEMENTS (mSerialPorts));

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
            AssertBreak (slot < RT_ELEMENTS (mSerialPorts));

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

#ifdef VBOX_WITH_GUEST_PROPS
    /* Guest properties (optional) */
    {
        using namespace guestProp;

        Key guestPropertiesNode = aNode.findKey ("GuestProperties");
        Bstr notificationPatterns ("");  /* We catch allocation failure below. */
        if (!guestPropertiesNode.isNull())
        {
            Key::List properties = guestPropertiesNode.keys ("GuestProperty");
            for (Key::List::const_iterator it = properties.begin();
                 it != properties.end(); ++ it)
            {
                uint32_t fFlags = NILFLAG;

                /* property name (required) */
                Bstr name = (*it).stringValue ("name");
                /* property value (required) */
                Bstr value = (*it).stringValue ("value");
                /* property timestamp (optional, defaults to 0) */
                ULONG64 timestamp = (*it).value<ULONG64> ("timestamp");
                /* property flags (optional, defaults to empty) */
                Bstr flags = (*it).stringValue ("flags");
                Utf8Str utf8Flags (flags);
                if (utf8Flags.isNull ())
                    return E_OUTOFMEMORY;
                validateFlags (utf8Flags.raw(), &fFlags);
                HWData::GuestProperty property = { name, value, timestamp, fFlags };
                mHWData->mGuestProperties.push_back (property);
                /* This is just sanity, as the push_back() will probably have thrown
                 * an exception if we are out of memory.  Note that if we run out
                 * allocating the Bstrs above, this will be caught here as well. */
                if (   mHWData->mGuestProperties.back().mName.isNull ()
                    || mHWData->mGuestProperties.back().mValue.isNull ()
                   )
                    return E_OUTOFMEMORY;
            }
            notificationPatterns = guestPropertiesNode.stringValue ("notificationPatterns");
        }
        mHWData->mPropertyServiceActive = false;
        mHWData->mGuestPropertyNotificationPatterns = notificationPatterns;
        if (mHWData->mGuestPropertyNotificationPatterns.isNull ())
            return E_OUTOFMEMORY;
    }
#endif /* VBOX_WITH_GUEST_PROPS defined */

    AssertComRC (rc);
    return rc;
}

/**
 *  @param aNode    <StorageControllers> node.
 */
HRESULT Machine::loadStorageControllers (const settings::Key &aNode, bool aRegistered,
                                         const Guid *aSnapshotId /* = NULL */)
{
    using namespace settings;

    AssertReturn (!aNode.isNull(), E_INVALIDARG);
    AssertReturn (mType == IsMachine || mType == IsSnapshotMachine, E_FAIL);

    HRESULT rc = S_OK;

    Key::List children = aNode.keys ("StorageController");

    /* Make sure the attached hard disks don't get unregistered until we
     * associate them with tis machine (important for VMs loaded (opened) after
     * VirtualBox startup) */
    AutoReadLock vboxLock (mParent);

    for (Key::List::const_iterator it = children.begin();
         it != children.end(); ++ it)
    {
        Bstr controllerName = (*it).stringValue ("name");
        const char *controllerType = (*it).stringValue ("type");
        ULONG portCount = (*it).value <ULONG> ("PortCount");
        StorageControllerType_T controller;
        StorageBus_T connection;

        if (strcmp (controllerType, "AHCI") == 0)
        {
            connection = StorageBus_SATA;
            controller = StorageControllerType_IntelAhci;
        }
        else if (strcmp (controllerType, "LsiLogic") == 0)
        {
            connection = StorageBus_SCSI;
            controller = StorageControllerType_LsiLogic;
        }
        else if (strcmp (controllerType, "BusLogic") == 0)
        {
            connection = StorageBus_SCSI;
            controller = StorageControllerType_BusLogic;
        }
        else if (strcmp (controllerType, "PIIX3") == 0)
        {
            connection = StorageBus_IDE;
            controller = StorageControllerType_PIIX3;
        }
        else if (strcmp (controllerType, "PIIX4") == 0)
        {
            connection = StorageBus_IDE;
            controller = StorageControllerType_PIIX4;
        }
        else if (strcmp (controllerType, "ICH6") == 0)
        {
            connection = StorageBus_IDE;
            controller = StorageControllerType_ICH6;
        }
        else
            AssertFailedReturn (E_FAIL);

        ComObjPtr<StorageController> ctl;
        /* Try to find one with the name first. */
        rc = getStorageControllerByName (controllerName, ctl, false /* aSetError */);
        if (SUCCEEDED (rc))
            return setError (VBOX_E_OBJECT_IN_USE,
                tr ("Storage controller named '%ls' already exists"), controllerName.raw());

        ctl.createObject();
        rc = ctl->init (this, controllerName, connection);
        CheckComRCReturnRC (rc);

        mStorageControllers->push_back (ctl);

        rc = ctl->COMSETTER(ControllerType)(controller);
        CheckComRCReturnRC(rc);

        rc = ctl->COMSETTER(PortCount)(portCount);
        CheckComRCReturnRC(rc);

        /* Set IDE emulation settings (only for AHCI controller). */
        if (controller == StorageControllerType_IntelAhci)
        {
            ULONG val;

            /* ide emulation settings (optional, default to 0,1,2,3 respectively) */
            val = (*it).valueOr <ULONG> ("IDE0MasterEmulationPort", 0);
            rc = ctl->SetIDEEmulationPort(0, val);
            CheckComRCReturnRC(rc);
            val = (*it).valueOr <ULONG> ("IDE0SlaveEmulationPort", 1);
            rc = ctl->SetIDEEmulationPort(1, val);
            CheckComRCReturnRC(rc);
            val = (*it).valueOr <ULONG> ("IDE1MasterEmulationPort", 2);
            rc = ctl->SetIDEEmulationPort(2, val);
            CheckComRCReturnRC(rc);
            val = (*it).valueOr <ULONG> ("IDE1SlaveEmulationPort", 3);
            rc = ctl->SetIDEEmulationPort(3, val);
            CheckComRCReturnRC(rc);
        }

        /* Load the attached devices now. */
        rc = loadStorageDevices(ctl, (*it),
                                aRegistered, aSnapshotId);
        CheckComRCReturnRC(rc);
    }

    return S_OK;
}

/**
 * @param aNode        <HardDiskAttachments> node.
 * @param aRegistered  true when the machine is being loaded on VirtualBox
 *                      startup, or when a snapshot is being loaded (wchich
 *                      currently can happen on startup only)
 * @param aSnapshotId  pointer to the snapshot ID if this is a snapshot machine
 *
 * @note Lock mParent for reading and hard disks for writing before calling.
 */
HRESULT Machine::loadStorageDevices (ComObjPtr<StorageController> aStorageController,
                                     const settings::Key &aNode, bool aRegistered,
                                     const Guid *aSnapshotId /* = NULL */)
{
    using namespace settings;

    AssertReturn (!aNode.isNull(), E_INVALIDARG);
    AssertReturn ((mType == IsMachine && aSnapshotId == NULL) ||
                  (mType == IsSnapshotMachine && aSnapshotId != NULL), E_FAIL);

    HRESULT rc = S_OK;

    Key::List children = aNode.keys ("AttachedDevice");

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
        Key idKey = (*it).key ("Image");
        /* hard disk uuid (required) */
        Guid uuid = idKey.value <Guid> ("uuid");
        /* device type (required) */
        const char *deviceType = (*it).stringValue ("type");
        /* channel (required) */
        LONG port = (*it).value <LONG> ("port");
        /* device (required) */
        LONG device = (*it).value <LONG> ("device");

        /* We support only hard disk types at the moment.
         * @todo: Implement support for CD/DVD drives.
         */
        if (strcmp(deviceType, "HardDisk") != 0)
            return setError (E_FAIL,
                tr ("Device at position %lu:%lu is not a hard disk: %s"),
                port, device, deviceType);

        /* find a hard disk by UUID */
        ComObjPtr<HardDisk> hd;
        rc = mParent->findHardDisk(&uuid, NULL, true /* aDoSetError */, &hd);
        CheckComRCReturnRC (rc);

        AutoWriteLock hdLock (hd);

        if (hd->type() == HardDiskType_Immutable)
        {
            if (mType == IsSnapshotMachine)
                return setError (E_FAIL,
                    tr ("Immutable hard disk '%ls' with UUID {%RTuuid} cannot be "
                        "directly attached to snapshot with UUID {%RTuuid} "
                        "of the virtual machine '%ls' ('%ls')"),
                    hd->locationFull().raw(), uuid.raw(),
                    aSnapshotId->raw(),
                    mUserData->mName.raw(), mData->mConfigFileFull.raw());

            return setError (E_FAIL,
                tr ("Immutable hard disk '%ls' with UUID {%RTuuid} cannot be "
                    "directly attached to the virtual machine '%ls' ('%ls')"),
                hd->locationFull().raw(), uuid.raw(),
                mUserData->mName.raw(), mData->mConfigFileFull.raw());
        }

        if (mType != IsSnapshotMachine && hd->children().size() != 0)
            return setError (E_FAIL,
                tr ("Hard disk '%ls' with UUID {%RTuuid} cannot be directly "
                    "attached to the virtual machine '%ls' ('%ls') "
                    "because it has %d differencing child hard disks"),
                hd->locationFull().raw(), uuid.raw(),
                mUserData->mName.raw(), mData->mConfigFileFull.raw(),
                hd->children().size());

        if (std::find_if (mHDData->mAttachments.begin(),
                          mHDData->mAttachments.end(),
                          HardDiskAttachment::RefersTo (hd)) !=
                mHDData->mAttachments.end())
        {
            return setError (E_FAIL,
                tr ("Hard disk '%ls' with UUID {%RTuuid} is already attached "
                    "to the virtual machine '%ls' ('%ls')"),
                hd->locationFull().raw(), uuid.raw(),
                mUserData->mName.raw(), mData->mConfigFileFull.raw());
        }

        const Bstr controllerName = aStorageController->name();
        ComObjPtr<HardDiskAttachment> attachment;
        attachment.createObject();
        rc = attachment->init (hd, controllerName, port, device);
        CheckComRCBreakRC (rc);

        /* associate the hard disk with this machine and snapshot */
        if (mType == IsSnapshotMachine)
            rc = hd->attachTo (mData->mUuid, *aSnapshotId);
        else
            rc = hd->attachTo (mData->mUuid);

        AssertComRCBreakRC (rc);

        /* backup mHDData to let registeredInit() properly rollback on failure
         * (= limited accessibility) */

        mHDData.backup();
        mHDData->mAttachments.push_back (attachment);
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
        AssertFailedBreak();
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
HRESULT Machine::findSnapshot (IN_BSTR aName, ComObjPtr <Snapshot> &aSnapshot,
                               bool aSetError /* = false */)
{
    AssertReturn (aName, E_INVALIDARG);

    if (!mData->mFirstSnapshot)
    {
        if (aSetError)
            return setError (VBOX_E_OBJECT_NOT_FOUND,
                tr ("This machine does not have any snapshots"));
        return VBOX_E_OBJECT_NOT_FOUND;
    }

    aSnapshot = mData->mFirstSnapshot->findChildOrSelf (aName);

    if (!aSnapshot)
    {
        if (aSetError)
            return setError (VBOX_E_OBJECT_NOT_FOUND,
                tr ("Could not find a snapshot named '%ls'"), aName);
        return VBOX_E_OBJECT_NOT_FOUND;
    }

    return S_OK;
}

/**
 * Returns a storage controller object with the given name.
 *
 *  @param aName                 storage controller name to find
 *  @param aStorageController    where to return the found storage controller
 *  @param aSetError             true to set extended error info on failure
 */
HRESULT Machine::getStorageControllerByName(CBSTR aName,
                                            ComObjPtr <StorageController> &aStorageController,
                                            bool aSetError /* = false */)
{
    AssertReturn (aName, E_INVALIDARG);

    for (StorageControllerList::const_iterator it =
                mStorageControllers->begin();
            it != mStorageControllers->end();
            ++ it)
    {
        if ((*it)->name() == aName)
        {
            aStorageController = (*it);
            return S_OK;
        }
    }

    if (aSetError)
        return setError (VBOX_E_OBJECT_NOT_FOUND,
            tr ("Could not find a storage controller named '%ls'"), aName);
    return VBOX_E_OBJECT_NOT_FOUND;
}

HRESULT Machine::getHardDiskAttachmentsOfController(CBSTR aName,
                                                    HDData::AttachmentList &atts)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    for (HDData::AttachmentList::iterator it = mHDData->mAttachments.begin();
         it != mHDData->mAttachments.end(); ++it)
    {
        if ((*it)->controller() == aName)
            atts.push_back(*it);
    }

    return S_OK;
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
    AssertReturn (mParent->isWriteLockOnCurrentThread(), E_FAIL);
    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

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
                    if (RT_FAILURE (vrc))
                    {
                        rc = setError (E_FAIL,
                            tr ("Could not rename the directory '%s' to '%s' "
                                "to save the settings file (%Rrc)"),
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
                    if (RT_FAILURE (vrc))
                    {
                        rc = setError (E_FAIL,
                            tr ("Could not rename the settings file '%s' to '%s' "
                                "(%Rrc)"),
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
            if (RT_FAILURE (vrc))
            {
                return setError (E_FAIL,
                    tr ("Could not create a directory '%s' "
                        "to save the settings file (%Rrc)"),
                    path.raw(), vrc);
            }
        }

        /* Note: open flags must correlate with RTFileOpen() in lockConfig() */
        path = Utf8Str (mData->mConfigFileFull);
        vrc = RTFileOpen (&mData->mHandleCfgFile, path,
                          RTFILE_O_READWRITE | RTFILE_O_CREATE |
                          RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS (vrc))
        {
            vrc = RTFileWrite (mData->mHandleCfgFile,
                               (void *) gDefaultMachineConfig,
                               strlen (gDefaultMachineConfig), NULL);
        }
        if (RT_FAILURE (vrc))
        {
            mData->mHandleCfgFile = NIL_RTFILE;
            return setError (E_FAIL,
                tr ("Could not create the settings file '%s' (%Rrc)"),
                path.raw(), vrc);
        }
        /* we do not close the file to simulate lockConfig() */
    }

    return rc;
}

/**
 * Saves and commits machine data, user data and hardware data.
 *
 * Note that on failure, the data remains uncommitted.
 *
 * @a aFlags may combine the following flags:
 *
 *  - SaveS_ResetCurStateModified: Resets mData->mCurrentStateModified to FALSE.
 *    Used when saving settings after an operation that makes them 100%
 *    correspond to the settings from the current snapshot.
 *  - SaveS_InformCallbacksAnyway: Callbacks will be informed even if
 *    #isReallyModified() returns false. This is necessary for cases when we
 *    change machine data diectly, not through the backup()/commit() mechanism.
 *
 * @note Must be called from under mParent write lock (sometimes needed by
 * #prepareSaveSettings()) and this object's write lock. Locks children for
 * writing. There is one exception when mParent is unused and therefore may be
 * left unlocked: if this machine is an unregistered one.
 */
HRESULT Machine::saveSettings (int aFlags /*= 0*/)
{
    LogFlowThisFuncEnter();

    /* Note: tecnhically, mParent needs to be locked only when the machine is
     * registered (see prepareSaveSettings() for details) but we don't
     * currently differentiate it in callers of saveSettings() so we don't
     * make difference here too.  */
    AssertReturn (mParent->isWriteLockOnCurrentThread(), E_FAIL);
    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

    /* make sure child objects are unable to modify the settings while we are
     * saving them */
    ensureNoStateDependencies();

    AssertReturn (mType == IsMachine || mType == IsSessionMachine, E_FAIL);

    BOOL currentStateModified = mData->mCurrentStateModified;
    bool settingsModified;

    if (!(aFlags & SaveS_ResetCurStateModified) && !currentStateModified)
    {
        /* We ignore changes to user data when setting mCurrentStateModified
         * because the current state will not differ from the current snapshot
         * if only user data has been changed (user data is shared by all
         * snapshots). */
        currentStateModified = isReallyModified (true /* aIgnoreUserData */);
        settingsModified = mUserData.hasActualChanges() || currentStateModified;
    }
    else
    {
        if (aFlags & SaveS_ResetCurStateModified)
            currentStateModified = FALSE;
        settingsModified = isReallyModified();
    }

    HRESULT rc = S_OK;

    /* First, prepare to save settings. It will care about renaming the
     * settings directory and file if the machine name was changed and about
     * creating a new settings file if this is a new machine. */
    bool isRenamed = false;
    bool isNew = false;
    rc = prepareSaveSettings (isRenamed, isNew);
    CheckComRCReturnRC (rc);

    try
    {
        using namespace settings;
        using namespace xml;

        /* this object is locked for writing to prevent concurrent reads and writes */
        File file (mData->mHandleCfgFile, Utf8Str (mData->mConfigFileFull));
        XmlTreeBackend tree;

        /* The newly created settings file is incomplete therefore we turn off
         * validation. The rest is like in loadSettingsTree_ForUpdate().*/
        rc = VirtualBox::loadSettingsTree (tree, file,
                                           !isNew /* aValidate */,
                                           false /* aCatchLoadErrors */,
                                           false /* aAddDefaults */);
        CheckComRCThrowRC (rc);

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
        /// @todo The reason for MachineState_Restoring below:
        /// PushGuestProperties() is always called from Console::powerDown()
        /// (including the case when restoring from the saved state fails) and
        /// calls SaveSettings() to save guest properties. Since the saved state
        /// file is still present there (and should be kept), we must save it
        /// while in Restoring state too. However, calling SaveSettings() from
        /// PushGuestProperties() is wrong in the first place. A proper way is
        /// to only save guest properties node and not involve the whole save
        /// process.
        if (mData->mMachineState == MachineState_Saved ||
            mData->mMachineState == MachineState_Restoring)
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
                                       !!currentStateModified, true);

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

        /* StorageControllers node (required) */
        {
            /* first, delete the entire node if exists */
            Key storageNode = machineNode.findKey ("StorageControllers");
            if (!storageNode.isNull())
                storageNode.zap();
            /* then recreate it */
            storageNode = machineNode.createKey ("StorageControllers");

            rc = saveStorageControllers (storageNode);
            CheckComRCThrowRC (rc);
        }

        /* ask to save all snapshots when the machine name was changed since
         * it may affect saved state file paths for online snapshots (see
         * #openConfigLoader() for details) */
        if (isRenamed)
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

    if (SUCCEEDED (rc))
    {
        commit();

        /* memorize the new modified state */
        mData->mCurrentStateModified = currentStateModified;
    }

    if (settingsModified || (aFlags & SaveS_InformCallbacksAnyway))
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

    /* This object's write lock is also necessary to serialize file access
     * (prevent concurrent reads and writes) */
    AutoWriteLock alock (this);

    AssertReturn (isConfigLocked(), E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        using namespace settings;
        using namespace xml;

        /* load the settings file */
        File file (mData->mHandleCfgFile, Utf8Str (mData->mConfigFileFull));
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

    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

    int op = aOpFlags & SaveSS_OpMask;
    AssertReturn (
        (aSnapshot && (op == SaveSS_AddOp || op == SaveSS_UpdateAttrsOp ||
                       op == SaveSS_UpdateAllOp)) ||
        (!aSnapshot && ((op == SaveSS_NoOp && (aOpFlags & SaveSS_CurrentId)) ||
                        op == SaveSS_UpdateAllOp)),
        E_FAIL);

    HRESULT rc = S_OK;

    bool recreateWholeTree = false;

    do
    {
        if (op == SaveSS_NoOp)
            break;

        /* quick path: recreate the whole tree of the snapshots */
        if (op == SaveSS_UpdateAllOp && aSnapshot == NULL)
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

                    Key storageNode = aMachineNode.findKey ("StorageControllers");
                    if (!storageNode.isNull())
                        storageNode.zap();
                    storageNode = aMachineNode.createKey ("StorageControllers");

                    rc = saveStorageControllers (storageNode);
                    CheckComRCBreakRC (rc);

                    if (mHDData->mAttachments.size() != 0)
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
        if (aOpFlags & SaveSS_CurrentId)
        {
            if (!mData->mCurrentSnapshot.isNull())
                aMachineNode.setValue <Guid> ("currentSnapshot",
                                              mData->mCurrentSnapshot->data().mId);
            else
                aMachineNode.zapValue ("currentSnapshot");
        }
        if (aOpFlags & SaveSS_CurStateModified)
        {
            /* defaults to true */
            aMachineNode.setValueOr <bool> ("currentStateModified",
                                            !!mData->mCurrentStateModified, true);
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

        /* save hard disks. */
        {
            Key storageNode = aNode.createKey ("StorageControllers");
            HRESULT rc = snapshotMachine->saveStorageControllers (storageNode);
            CheckComRCReturnRC (rc);
        }
    }

    /* save children */
    {
        AutoWriteLock listLock (aSnapshot->childrenLock ());

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

    /* The hardware version attribute (optional).
       Automatically upgrade from 1 to 2 when there is no saved state. (ugly!) */
    {
        Utf8Str hwVersion = mHWData->mHWVersion;
        if (   hwVersion.compare ("1") == 0
            && mSSData->mStateFilePath.isEmpty())
            mHWData->mHWVersion = hwVersion = "2";  /** @todo Is this safe, to update mHWVersion here? If not some other point needs to be found where this can be done. */
        if (hwVersion.compare ("2") == 0)           /** @todo get the default from the schema if possible. */
            aNode.zapValue ("version");
        else
            aNode.setStringValue ("version", hwVersion.raw());
    }

    /* CPU (optional, but always created atm) */
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

        /* Nested paging (optional, default is false) */
        if (mHWData->mHWVirtExNestedPagingEnabled)
        {
            Key HWVirtExNestedPagingNode = cpuNode.createKey ("HardwareVirtExNestedPaging");
            HWVirtExNestedPagingNode.setValue <bool> ("enabled", true);
        }

        /* VPID (optional, default is false) */
        if (mHWData->mHWVirtExVPIDEnabled)
        {
            Key HWVirtExVPIDNode = cpuNode.createKey ("HardwareVirtExVPID");
            HWVirtExVPIDNode.setValue <bool> ("enabled", true);
        }

        /* PAE (optional, default is false) */
        if (mHWData->mPAEEnabled)
        {
            Key PAENode = cpuNode.createKey ("PAE");
            PAENode.setValue <bool> ("enabled", true);
        }

        /* CPU count */
        cpuNode.setValue <ULONG> ("count", mHWData->mCPUCount);
    }

    /* memory (required) */
    {
        Key memoryNode = aNode.createKey ("Memory");
        memoryNode.setValue <ULONG> ("RAMSize", mHWData->mMemorySize);
    }

    /* boot (required) */
    {
        Key bootNode = aNode.createKey ("Boot");

        for (ULONG pos = 0; pos < RT_ELEMENTS (mHWData->mBootOrder); ++ pos)
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
                    ComAssertMsgFailedRet (("Invalid boot device: %d",
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
        displayNode.setValue <ULONG> ("monitorCount", mHWData->mMonitorCount);
        displayNode.setValue <bool> ("accelerate3D", !!mHWData->mAccelerate3DEnabled);
    }

#ifdef VBOX_WITH_VRDP
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

    /* Network adapters (required) */
    {
        Key nwNode = aNode.createKey ("Network");

        for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); ++ slot)
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

        for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); ++ slot)
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

        for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); ++ slot)
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

#ifdef VBOX_WITH_GUEST_PROPS
    /* Guest properties */
    try
    {
        using namespace guestProp;

        Key guestPropertiesNode = aNode.createKey ("GuestProperties");

        for (HWData::GuestPropertyList::const_iterator it = mHWData->mGuestProperties.begin();
             it != mHWData->mGuestProperties.end(); ++it)
        {
            HWData::GuestProperty property = *it;

            Key propertyNode = guestPropertiesNode.appendKey ("GuestProperty");
            char szFlags[MAX_FLAGS_LEN + 1];

            propertyNode.setValue <Bstr> ("name", property.mName);
            propertyNode.setValue <Bstr> ("value", property.mValue);
            propertyNode.setValue <ULONG64> ("timestamp", property.mTimestamp);
            writeFlags (property.mFlags, szFlags);
            Bstr flags (szFlags);
            if (flags.isNull())
                return E_OUTOFMEMORY;
            propertyNode.setValue <Bstr> ("flags", flags);
        }
        Bstr emptyStr ("");
        if (emptyStr.isNull())
            return E_OUTOFMEMORY;
        guestPropertiesNode.setValueOr <Bstr> ("notificationPatterns",
                                               mHWData->mGuestPropertyNotificationPatterns,
                                               emptyStr);
    }
    catch (xml::ENoMemory e)
    {
        return E_OUTOFMEMORY;
    }
#endif /* VBOX_WITH_GUEST_PROPS defined */

    AssertComRC (rc);
    return rc;
}

/**
 *  Saves the storage controller configuration.
 *
 *  @param aNode    <StorageControllers> node to save the VM hardware confguration to.
 */
HRESULT Machine::saveStorageControllers (settings::Key &aNode)
{
    using namespace settings;

    AssertReturn (!aNode.isNull(), E_INVALIDARG);

    for (StorageControllerList::const_iterator
         it = mStorageControllers->begin();
         it != mStorageControllers->end();
         ++ it)
    {
        HRESULT rc;
        const char *type = NULL;
        ComObjPtr <StorageController> ctl = *it;

        Key ctlNode = aNode.appendKey ("StorageController");

        ctlNode.setValue <Bstr> ("name", ctl->name());

        switch (ctl->controllerType())
        {
            case StorageControllerType_IntelAhci: type = "AHCI"; break;
            case StorageControllerType_LsiLogic: type = "LsiLogic"; break;
            case StorageControllerType_BusLogic: type = "BusLogic"; break;
            case StorageControllerType_PIIX3: type = "PIIX3"; break;
            case StorageControllerType_PIIX4: type = "PIIX4"; break;
            case StorageControllerType_ICH6: type = "ICH6"; break;
            default:
                ComAssertFailedRet (E_FAIL);
        }

        ctlNode.setStringValue ("type", type);

        /* Save the port count. */
        ULONG portCount;
        rc = ctl->COMGETTER(PortCount)(&portCount);
        ComAssertRCRet(rc, rc);
        ctlNode.setValue <ULONG> ("PortCount", portCount);

        /* Save IDE emulation settings. */
        if (ctl->controllerType() == StorageControllerType_IntelAhci)
        {
            LONG uVal;

            rc = ctl->GetIDEEmulationPort(0, &uVal);
            ComAssertRCRet(rc, rc);
            ctlNode.setValue <LONG> ("IDE0MasterEmulationPort", uVal);

            rc = ctl->GetIDEEmulationPort(1, &uVal);
            ComAssertRCRet(rc, rc);
            ctlNode.setValue <LONG> ("IDE0SlaveEmulationPort", uVal);

            rc = ctl->GetIDEEmulationPort(2, &uVal);
            ComAssertRCRet(rc, rc);
            ctlNode.setValue <LONG> ("IDE1MasterEmulationPort", uVal);

            rc = ctl->GetIDEEmulationPort(3, &uVal);
            ComAssertRCRet(rc, rc);
            ctlNode.setValue <LONG> ("IDE1SlaveEmulationPort", uVal);
        }

        /* save the devices now. */
        rc = saveStorageDevices(ctl, ctlNode);
        ComAssertRCRet(rc, rc);
    }

    return S_OK;
}

/**
 *  Saves the hard disk confguration.
 *  It is assumed that the given node is empty.
 *
 *  @param aNode    <HardDiskAttachments> node to save the hard disk confguration to.
 */
HRESULT Machine::saveStorageDevices (ComObjPtr<StorageController> aStorageController,
                                     settings::Key &aNode)
{
    using namespace settings;

    AssertReturn (!aNode.isNull(), E_INVALIDARG);

    HDData::AttachmentList atts;

    HRESULT rc = getHardDiskAttachmentsOfController(aStorageController->name(), atts);
    CheckComRCReturnRC(rc);

    for (HDData::AttachmentList::const_iterator
         it = atts.begin();
         it != atts.end();
         ++ it)
    {
        Key hdNode = aNode.appendKey ("AttachedDevice");

        {
            /* device type. Only hard disk allowed atm. */
            hdNode.setStringValue ("type", "HardDisk");
            /* channel (required) */
            hdNode.setValue <LONG> ("port", (*it)->port());
            /* device (required) */
            hdNode.setValue <LONG> ("device", (*it)->device());
            /* ID of the image. */
            Key idNode = hdNode.appendKey ("Image");
            idNode.setValue <Guid> ("uuid", (*it)->hardDisk()->id());
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

    /* This object's write lock is also necessary to serialize file access
     * (prevent concurrent reads and writes) */
    AutoWriteLock alock (this);

    AssertReturn (isConfigLocked(), E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        using namespace settings;
        using namespace xml;

        /* load the settings file */
        File file (mData->mHandleCfgFile, Utf8Str (mData->mConfigFileFull));
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
 * Creates differencing hard disks for all normal hard disks attached to this
 * machine and a new set of attachments to refer to created disks.
 *
 * Used when taking a snapshot or when discarding the current state.
 *
 * This method assumes that mHDData contains the original hard disk attachments
 * it needs to create diffs for. On success, these attachments will be replaced
 * with the created diffs. On failure, #deleteImplicitDiffs() is implicitly
 * called to delete created diffs which will also rollback mHDData and restore
 * whatever was backed up before calling this method.
 *
 * Attachments with non-normal hard disks are left as is.
 *
 * If @a aOnline is @c false then the original hard disks that require implicit
 * diffs will be locked for reading. Otherwise it is assumed that they are
 * already locked for writing (when the VM was started). Note that in the latter
 * case it is responsibility of the caller to lock the newly created diffs for
 * writing if this method succeeds.
 *
 * @param aFolder           Folder where to create diff hard disks.
 * @param aProgress         Progress object to run (must contain at least as
 *                          many operations left as the number of hard disks
 *                          attached).
 * @param aOnline           Whether the VM was online prior to this operation.
 *
 * @note The progress object is not marked as completed, neither on success nor
 *       on failure. This is a responsibility of the caller.
 *
 * @note Locks this object for writing.
 */
HRESULT Machine::createImplicitDiffs (const Bstr &aFolder,
                                      ComObjPtr <Progress> &aProgress,
                                      bool aOnline)
{
    AssertReturn (!aFolder.isEmpty(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    /* must be in a protective state because we leave the lock below */
    AssertReturn (mData->mMachineState == MachineState_Saving ||
                  mData->mMachineState == MachineState_Discarding, E_FAIL);

    HRESULT rc = S_OK;

    typedef std::list< ComObjPtr<HardDisk> > LockedMedia;
    LockedMedia lockedMedia;

    try
    {
        if (!aOnline)
        {
            /* lock all attached hard disks early to detect "in use"
             * situations before creating actual diffs */
            for (HDData::AttachmentList::const_iterator
                 it = mHDData->mAttachments.begin();
                 it != mHDData->mAttachments.end();
                 ++ it)
            {
                ComObjPtr<HardDiskAttachment> hda = *it;
                ComObjPtr<HardDisk> hd = hda->hardDisk();

                rc = hd->LockRead (NULL);
                CheckComRCThrowRC (rc);

                lockedMedia.push_back (hd);
            }
        }

        /* remember the current list (note that we don't use backup() since
         * mHDData may be already backed up) */
        HDData::AttachmentList atts = mHDData->mAttachments;

        /* start from scratch */
        mHDData->mAttachments.clear();

        /* go through remembered attachments and create diffs for normal hard
         * disks and attach them */

        for (HDData::AttachmentList::const_iterator
             it = atts.begin(); it != atts.end(); ++ it)
        {
            ComObjPtr<HardDiskAttachment> hda = *it;
            ComObjPtr<HardDisk> hd = hda->hardDisk();

            /* type cannot be changed while attached => no need to lock */
            if (hd->type() != HardDiskType_Normal)
            {
                /* copy the attachment as is */

                Assert (hd->type() == HardDiskType_Writethrough);

                rc = aProgress->setNextOperation(BstrFmt(tr("Skipping writethrough hard disk '%s'"),
                                                         hd->root()->name().raw()),
                                                 1);        // weight
                CheckComRCThrowRC (rc);

                mHDData->mAttachments.push_back (hda);
                continue;
            }

            /* need a diff */

            rc = aProgress->setNextOperation(BstrFmt(tr("Creating differencing hard disk for '%s'"),
                                                     hd->root()->name().raw()),
                                             1);        // weight
            CheckComRCThrowRC (rc);

            ComObjPtr<HardDisk> diff;
            diff.createObject();
            rc = diff->init (mParent, hd->preferredDiffFormat(),
                             BstrFmt ("%ls"RTPATH_SLASH_STR,
                                      mUserData->mSnapshotFolderFull.raw()));
            CheckComRCThrowRC (rc);

            /* leave the lock before the potentially lengthy operation */
            alock.leave();

            rc = hd->createDiffStorageAndWait (diff, HardDiskVariant_Standard,
                                               &aProgress);

            alock.enter();

            CheckComRCThrowRC (rc);

            rc = diff->attachTo (mData->mUuid);
            AssertComRCThrowRC (rc);

            /* add a new attachment */
            ComObjPtr<HardDiskAttachment> attachment;
            attachment.createObject();
            rc = attachment->init (diff, hda->controller(), hda->port(),
                                   hda->device(), true /* aImplicit */);
            CheckComRCThrowRC (rc);

            mHDData->mAttachments.push_back (attachment);
        }
    }
    catch (HRESULT aRC) { rc = aRC; }

    /* unlock all hard disks we locked */
    if (!aOnline)
    {
        ErrorInfoKeeper eik;

        for (LockedMedia::const_iterator it = lockedMedia.begin();
             it != lockedMedia.end(); ++ it)
        {
            HRESULT rc2 = (*it)->UnlockRead (NULL);
            AssertComRC (rc2);
        }
    }

    if (FAILED (rc))
    {
        MultiResultRef mrc (rc);

        mrc = deleteImplicitDiffs();
    }

    return rc;
}

/**
 * Deletes implicit differencing hard disks created either by
 * #createImplicitDiffs() or by #AttachHardDisk() and rolls back mHDData.
 *
 * Note that to delete hard disks created by #AttachHardDisk() this method is
 * called from #fixupHardDisks() when the changes are rolled back.
 *
 * @note Locks this object for writing.
 */
HRESULT Machine::deleteImplicitDiffs()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    AssertReturn (mHDData.isBackedUp(), E_FAIL);

    HRESULT rc = S_OK;

    HDData::AttachmentList implicitAtts;

    const HDData::AttachmentList &oldAtts =
        mHDData.backedUpData()->mAttachments;

    /* enumerate new attachments */
    for (HDData::AttachmentList::const_iterator
            it = mHDData->mAttachments.begin();
         it != mHDData->mAttachments.end(); ++ it)
    {
        ComObjPtr<HardDisk> hd = (*it)->hardDisk();

        if ((*it)->isImplicit())
        {
            /* deassociate and mark for deletion */
            rc = hd->detachFrom (mData->mUuid);
            AssertComRC (rc);
            implicitAtts.push_back (*it);
            continue;
        }

        /* was this hard disk attached before? */
        HDData::AttachmentList::const_iterator oldIt =
            std::find_if(oldAtts.begin(), oldAtts.end(),
                         HardDiskAttachment::RefersTo (hd));
        if (oldIt == oldAtts.end())
        {
            /* no: de-associate */
            rc = hd->detachFrom (mData->mUuid);
            AssertComRC (rc);
            continue;
        }
    }

    /* rollback hard disk changes */
    mHDData.rollback();

    MultiResult mrc (S_OK);

    /* delete unused implicit diffs */
    if (implicitAtts.size() != 0)
    {
        /* will leave the lock before the potentially lengthy
         * operation, so protect with the special state (unless already
         * protected) */
        MachineState_T oldState = mData->mMachineState;
        if (oldState != MachineState_Saving &&
            oldState != MachineState_Discarding)
        {
            setMachineState (MachineState_SettingUp);
        }

        alock.leave();

        for (HDData::AttachmentList::const_iterator
                it = implicitAtts.begin();
             it != implicitAtts.end(); ++ it)
        {
            ComObjPtr<HardDisk> hd = (*it)->hardDisk();

            mrc = hd->deleteStorageAndWait();
        }

        alock.enter();

        if (mData->mMachineState == MachineState_SettingUp)
        {
            setMachineState (oldState);
        }
    }

    return mrc;
}

/**
 * Perform deferred hard disk detachments on success and deletion of implicitly
 * created diffs on failure.
 *
 * Does nothing if the hard disk attachment data (mHDData) is not changed (not
 * backed up).
 *
 * When the data is backed up, this method will commit mHDData if @a aCommit is
 * @c true and rollback it otherwise before returning.
 *
 * If @a aOnline is @c true then this method called with @a aCommit = @c true
 * will also unlock the old hard disks for which the new implicit diffs were
 * created and will lock these new diffs for writing. When @a aCommit is @c
 * false, this argument is ignored.
 *
 * @param aCommit       @c true if called on success.
 * @param aOnline       Whether the VM was online prior to this operation.
 *
 * @note Locks this object for writing!
 */
void Machine::fixupHardDisks(bool aCommit, bool aOnline /*= false*/)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock (this);

    /* no attach/detach operations -- nothing to do */
    if (!mHDData.isBackedUp())
        return;

    HRESULT rc = S_OK;

    if (aCommit)
    {
        HDData::AttachmentList &oldAtts =
            mHDData.backedUpData()->mAttachments;

        /* enumerate new attachments */
        for (HDData::AttachmentList::const_iterator
             it = mHDData->mAttachments.begin();
             it != mHDData->mAttachments.end(); ++ it)
        {
            ComObjPtr<HardDisk> hd = (*it)->hardDisk();

            if ((*it)->isImplicit())
            {
                /* convert implicit attachment to normal */
                (*it)->setImplicit (false);

                if (aOnline)
                {
                    rc = hd->LockWrite (NULL);
                    AssertComRC (rc);

                    mData->mSession.mLockedMedia.push_back (
                        Data::Session::LockedMedia::value_type (
                            ComPtr <IHardDisk> (hd), true));

                    /* also, relock the old hard disk which is a base for the
                     * new diff for reading if the VM is online */

                    ComObjPtr<HardDisk> parent = hd->parent();
                    /* make the relock atomic */
                    AutoWriteLock parentLock (parent);
                    rc = parent->UnlockWrite (NULL);
                    AssertComRC (rc);
                    rc = parent->LockRead (NULL);
                    AssertComRC (rc);

                    /* XXX actually we should replace the old entry in that
                     * vector (write lock => read lock) but this would take
                     * some effort. So lets just ignore the error code in
                     * SessionMachine::unlockMedia(). */
                    mData->mSession.mLockedMedia.push_back (
                        Data::Session::LockedMedia::value_type (
                            ComPtr <IHardDisk> (parent), false));
                }

                continue;
            }

            /* was this hard disk attached before? */
            HDData::AttachmentList::iterator oldIt =
                std::find_if (oldAtts.begin(), oldAtts.end(),
                              HardDiskAttachment::RefersTo (hd));
            if (oldIt != oldAtts.end())
            {
                /* yes: remove from old to avoid de-association */
                oldAtts.erase (oldIt);
            }
        }

        /* enumerate remaining old attachments and de-associate from the
         * current machine state */
        for (HDData::AttachmentList::const_iterator it = oldAtts.begin();
             it != oldAtts.end(); ++ it)
        {
            ComObjPtr<HardDisk> hd = (*it)->hardDisk();

            /* now de-associate from the current machine state */
            rc = hd->detachFrom (mData->mUuid);
            AssertComRC (rc);

            if (aOnline)
            {
                /* unlock since not used anymore */
                MediaState_T state;
                rc = hd->UnlockWrite (&state);
                /* the disk may be alredy relocked for reading above */
                Assert (SUCCEEDED (rc) || state == MediaState_LockedRead);
            }
        }

        /* commit the hard disk changes */
        mHDData.commit();

        if (mType == IsSessionMachine)
        {
            /* attach new data to the primary machine and reshare it */
            mPeer->mHDData.attach (mHDData);
        }
    }
    else
    {
        deleteImplicitDiffs();
    }

    return;
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
        if (RT_FAILURE (vrc))
        {
            mData->mHandleCfgFile = NIL_RTFILE;

            rc = setError (VBOX_E_FILE_ERROR,
                tr ("Could not lock the settings file '%ls' (%Rrc)"),
                mData->mConfigFileFull.raw(), vrc);
        }
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
        RTFileFlush (mData->mHandleCfgFile);
        RTFileClose (mData->mHandleCfgFile);
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

    AutoReadLock alock (this);

    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
        if (mNetworkAdapters [slot] && mNetworkAdapters [slot]->isModified())
            return true;

    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
        if (mSerialPorts [slot] && mSerialPorts [slot]->isModified())
            return true;

    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
        if (mParallelPorts [slot] && mParallelPorts [slot]->isModified())
            return true;

    if (!mStorageControllers.isNull())
    {
        for (StorageControllerList::const_iterator it =
                    mStorageControllers->begin();
                it != mStorageControllers->end();
                ++ it)
        {
            if ((*it)->isModified())
                return true;
        }
    }

    return
        mUserData.isBackedUp() ||
        mHWData.isBackedUp() ||
        mHDData.isBackedUp() ||
        mStorageControllers.isBackedUp() ||
#ifdef VBOX_WITH_VRDP
        (mVRDPServer && mVRDPServer->isModified()) ||
#endif
        (mDVDDrive && mDVDDrive->isModified()) ||
        (mFloppyDrive && mFloppyDrive->isModified()) ||
        (mAudioAdapter && mAudioAdapter->isModified()) ||
        (mUSBController && mUSBController->isModified()) ||
        (mBIOSSettings && mBIOSSettings->isModified());
}

/**
 * Returns the logical OR of data.hasActualChanges() of this and all child
 * objects.
 *
 * @param aIgnoreUserData       @c true to ignore changes to mUserData
 *
 * @note Locks objects for reading!
 */
bool Machine::isReallyModified (bool aIgnoreUserData /* = false */)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock (this);

    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
        if (mNetworkAdapters [slot] && mNetworkAdapters [slot]->isReallyModified())
            return true;

    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
        if (mSerialPorts [slot] && mSerialPorts [slot]->isReallyModified())
            return true;

    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
        if (mParallelPorts [slot] && mParallelPorts [slot]->isReallyModified())
            return true;

    if (!mStorageControllers.isBackedUp())
    {
        /* see whether any of the devices has changed its data */
        for (StorageControllerList::const_iterator
             it = mStorageControllers->begin();
             it != mStorageControllers->end();
             ++ it)
        {
            if ((*it)->isReallyModified())
                return true;
        }
    }
    else
    {
        if (mStorageControllers->size() != mStorageControllers.backedUpData()->size())
            return true;
    }

    return
        (!aIgnoreUserData && mUserData.hasActualChanges()) ||
        mHWData.hasActualChanges() ||
        mHDData.hasActualChanges() ||
        mStorageControllers.hasActualChanges() ||
#ifdef VBOX_WITH_VRDP
        (mVRDPServer && mVRDPServer->isReallyModified()) ||
#endif
        (mDVDDrive && mDVDDrive->isReallyModified()) ||
        (mFloppyDrive && mFloppyDrive->isReallyModified()) ||
        (mAudioAdapter && mAudioAdapter->isReallyModified()) ||
        (mUSBController && mUSBController->isReallyModified()) ||
        (mBIOSSettings && mBIOSSettings->isReallyModified());
}

/**
 * Discards all changes to machine settings.
 *
 * @param aNotify   Whether to notify the direct session about changes or not.
 *
 * @note Locks objects for writing!
 */
void Machine::rollback (bool aNotify)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    AutoWriteLock alock (this);

    /* check for changes in own data */

    bool sharedFoldersChanged = false, storageChanged = false;

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

    if (!mStorageControllers.isNull())
    {
        if (mStorageControllers.isBackedUp())
        {
            /* unitialize all new devices (absent in the backed up list). */
            StorageControllerList::const_iterator it = mStorageControllers->begin();
            StorageControllerList *backedList = mStorageControllers.backedUpData();
            while (it != mStorageControllers->end())
            {
                if (std::find (backedList->begin(), backedList->end(), *it ) ==
                    backedList->end())
                {
                    (*it)->uninit();
                }
                ++ it;
            }

            /* restore the list */
            mStorageControllers.rollback();
        }

        /* rollback any changes to devices after restoring the list */
        StorageControllerList::const_iterator it = mStorageControllers->begin();
        while (it != mStorageControllers->end())
        {
            if ((*it)->isModified())
                (*it)->rollback();

            ++ it;
        }
    }

    mUserData.rollback();

    mHWData.rollback();

    if (mHDData.isBackedUp())
        fixupHardDisks(false /* aCommit */);

    /* check for changes in child objects */

    bool vrdpChanged = false, dvdChanged = false, floppyChanged = false,
         usbChanged = false;

    ComPtr <INetworkAdapter> networkAdapters [RT_ELEMENTS (mNetworkAdapters)];
    ComPtr <ISerialPort> serialPorts [RT_ELEMENTS (mSerialPorts)];
    ComPtr <IParallelPort> parallelPorts [RT_ELEMENTS (mParallelPorts)];

    if (mBIOSSettings)
        mBIOSSettings->rollback();

#ifdef VBOX_WITH_VRDP
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

    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
        if (mNetworkAdapters [slot])
            if (mNetworkAdapters [slot]->rollback())
                networkAdapters [slot] = mNetworkAdapters [slot];

    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
        if (mSerialPorts [slot])
            if (mSerialPorts [slot]->rollback())
                serialPorts [slot] = mSerialPorts [slot];

    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
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

        for (ULONG slot = 0; slot < RT_ELEMENTS (networkAdapters); slot ++)
            if (networkAdapters [slot])
                that->onNetworkAdapterChange (networkAdapters [slot]);
        for (ULONG slot = 0; slot < RT_ELEMENTS (serialPorts); slot ++)
            if (serialPorts [slot])
                that->onSerialPortChange (serialPorts [slot]);
        for (ULONG slot = 0; slot < RT_ELEMENTS (parallelPorts); slot ++)
            if (parallelPorts [slot])
                that->onParallelPortChange (parallelPorts [slot]);

        if (storageChanged)
            that->onStorageControllerChange();
    }
}

/**
 * Commits all the changes to machine settings.
 *
 * Note that this operation is supposed to never fail.
 *
 * @note Locks this object and children for writing.
 */
void Machine::commit()
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    AutoMultiWriteLock2 alock (mPeer, this);

    /*
     *  use safe commit to ensure Snapshot machines (that share mUserData)
     *  will still refer to a valid memory location
     */
    mUserData.commitCopy();

    mHWData.commit();

    if (mHDData.isBackedUp())
        fixupHardDisks(true /* aCommit */);

    mBIOSSettings->commit();
#ifdef VBOX_WITH_VRDP
    mVRDPServer->commit();
#endif
    mDVDDrive->commit();
    mFloppyDrive->commit();
    mAudioAdapter->commit();
    mUSBController->commit();

    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
        mNetworkAdapters [slot]->commit();
    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
        mSerialPorts [slot]->commit();
    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
        mParallelPorts [slot]->commit();

    bool commitStorageControllers = false;

    if (mStorageControllers.isBackedUp())
    {
        mStorageControllers.commit();

        if (mPeer)
        {
            AutoWriteLock peerlock (mPeer);

            /* Commit all changes to new controllers (this will reshare data with
             * peers for thos who have peers) */
            StorageControllerList *newList = new StorageControllerList();
            StorageControllerList::const_iterator it = mStorageControllers->begin();
            while (it != mStorageControllers->end())
            {
                (*it)->commit();

                /* look if this controller has a peer device */
                ComObjPtr<StorageController> peer = (*it)->peer();
                if (!peer)
                {
                    /* no peer means the device is a newly created one;
                     * create a peer owning data this device share it with */
                    peer.createObject();
                    peer->init (mPeer, *it, true /* aReshare */);
                }
                else
                {
                    /* remove peer from the old list */
                    mPeer->mStorageControllers->remove (peer);
                }
                /* and add it to the new list */
                newList->push_back(peer);

                ++ it;
            }

            /* uninit old peer's controllers that are left */
            it = mPeer->mStorageControllers->begin();
            while (it != mPeer->mStorageControllers->end())
            {
                (*it)->uninit();
                ++ it;
            }

            /* attach new list of controllers to our peer */
            mPeer->mStorageControllers.attach (newList);
        }
        else
        {
            /* we have no peer (our parent is the newly created machine);
             * just commit changes to devices */
            commitStorageControllers = true;
        }
    }
    else
    {
        /* the list of controllers itself is not changed,
         * just commit changes to controllers themselves */
        commitStorageControllers = true;
    }

    if (commitStorageControllers)
    {
        StorageControllerList::const_iterator it = mStorageControllers->begin();
        while (it != mStorageControllers->end())
        {
            (*it)->commit();
            ++ it;
        }
    }

    if (mType == IsSessionMachine)
    {
        /* attach new data to the primary machine and reshare it */
        mPeer->mUserData.attach (mUserData);
        mPeer->mHWData.attach (mHWData);
        /* mHDData is reshared by fixupHardDisks */
        // mPeer->mHDData.attach (mHDData);
        Assert (mPeer->mHDData.data() == mHDData.data());
    }
}

/**
 * Copies all the hardware data from the given machine.
 *
 * Currently, only called when the VM is being restored from a snapshot. In
 * particular, this implies that the VM is not running during this method's
 * call.
 *
 * @note This method must be called from under this object's lock.
 *
 * @note This method doesn't call #commit(), so all data remains backed up and
 *       unsaved.
 */
void Machine::copyFrom (Machine *aThat)
{
    AssertReturnVoid (mType == IsMachine || mType == IsSessionMachine);
    AssertReturnVoid (aThat->mType == IsSnapshotMachine);

    AssertReturnVoid (!Global::IsOnline (mData->mMachineState));

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
#ifdef VBOX_WITH_VRDP
    mVRDPServer->copyFrom (aThat->mVRDPServer);
#endif
    mDVDDrive->copyFrom (aThat->mDVDDrive);
    mFloppyDrive->copyFrom (aThat->mFloppyDrive);
    mAudioAdapter->copyFrom (aThat->mAudioAdapter);
    mUSBController->copyFrom (aThat->mUSBController);

    /* create private copies of all controllers */
    mStorageControllers.backup();
    mStorageControllers->clear();
    for (StorageControllerList::iterator it = aThat->mStorageControllers->begin();
         it != aThat->mStorageControllers->end();
         ++ it)
    {
        ComObjPtr <StorageController> ctrl;
        ctrl.createObject();
        ctrl->initCopy (this, *it);
        mStorageControllers->push_back(ctrl);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
        mNetworkAdapters [slot]->copyFrom (aThat->mNetworkAdapters [slot]);
    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
        mSerialPorts [slot]->copyFrom (aThat->mSerialPorts [slot]);
    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
        mParallelPorts [slot]->copyFrom (aThat->mParallelPorts [slot]);
}

#ifdef VBOX_WITH_RESOURCE_USAGE_API
void Machine::registerMetrics (PerformanceCollector *aCollector, Machine *aMachine, RTPROCESS pid)
{
    pm::CollectorHAL *hal = aCollector->getHAL();
    /* Create sub metrics */
    pm::SubMetric *cpuLoadUser = new pm::SubMetric ("CPU/Load/User",
        "Percentage of processor time spent in user mode by VM process.");
    pm::SubMetric *cpuLoadKernel = new pm::SubMetric ("CPU/Load/Kernel",
        "Percentage of processor time spent in kernel mode by VM process.");
    pm::SubMetric *ramUsageUsed  = new pm::SubMetric ("RAM/Usage/Used",
        "Size of resident portion of VM process in memory.");
    /* Create and register base metrics */
    IUnknown *objptr;

    ComObjPtr<Machine> tmp = aMachine;
    tmp.queryInterfaceTo (&objptr);
    pm::BaseMetric *cpuLoad = new pm::MachineCpuLoadRaw (hal, objptr, pid,
                                             cpuLoadUser, cpuLoadKernel);
    aCollector->registerBaseMetric (cpuLoad);
    pm::BaseMetric *ramUsage = new pm::MachineRamUsage (hal, objptr, pid,
                                                           ramUsageUsed);
    aCollector->registerBaseMetric (ramUsage);

    aCollector->registerMetric (new pm::Metric (cpuLoad, cpuLoadUser, 0));
    aCollector->registerMetric (new pm::Metric (cpuLoad, cpuLoadUser,
                                                new pm::AggregateAvg()));
    aCollector->registerMetric (new pm::Metric (cpuLoad, cpuLoadUser,
                                                new pm::AggregateMin()));
    aCollector->registerMetric (new pm::Metric (cpuLoad, cpuLoadUser,
                                                new pm::AggregateMax()));
    aCollector->registerMetric (new pm::Metric (cpuLoad, cpuLoadKernel, 0));
    aCollector->registerMetric (new pm::Metric (cpuLoad, cpuLoadKernel,
                                                new pm::AggregateAvg()));
    aCollector->registerMetric (new pm::Metric (cpuLoad, cpuLoadKernel,
                                                new pm::AggregateMin()));
    aCollector->registerMetric (new pm::Metric (cpuLoad, cpuLoadKernel,
                                                new pm::AggregateMax()));

    aCollector->registerMetric (new pm::Metric (ramUsage, ramUsageUsed, 0));
    aCollector->registerMetric (new pm::Metric (ramUsage, ramUsageUsed,
                                                new pm::AggregateAvg()));
    aCollector->registerMetric (new pm::Metric (ramUsage, ramUsageUsed,
                                                new pm::AggregateMin()));
    aCollector->registerMetric (new pm::Metric (ramUsage, ramUsageUsed,
                                                new pm::AggregateMax()));
};

void Machine::unregisterMetrics (PerformanceCollector *aCollector, Machine *aMachine)
{
    aCollector->unregisterMetricsFor (aMachine);
    aCollector->unregisterBaseMetricsFor (aMachine);
};
#endif /* VBOX_WITH_RESOURCE_USAGE_API */


/////////////////////////////////////////////////////////////////////////////
// SessionMachine class
/////////////////////////////////////////////////////////////////////////////

/** Task structure for asynchronous VM operations */
struct SessionMachine::Task
{
    Task (SessionMachine *m, Progress *p)
        : machine (m), progress (p)
        , state (m->mData->mMachineState) // save the current machine state
        , subTask (false)
    {}

    void modifyLastState (MachineState_T s)
    {
        *const_cast <MachineState_T *> (&state) = s;
    }

    virtual void handler() = 0;

    ComObjPtr <SessionMachine> machine;
    ComObjPtr <Progress> progress;
    const MachineState_T state;

    bool subTask : 1;
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

    ComObjPtr <Snapshot> snapshot;
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

    AssertReturn (aMachine->lockHandle()->isWriteLockOnCurrentThread(), E_FAIL);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    /* create the interprocess semaphore */
#if defined(RT_OS_WINDOWS)
    mIPCSemName = aMachine->mData->mConfigFileFull;
    for (size_t i = 0; i < mIPCSemName.length(); i++)
        if (mIPCSemName[i] == '\\')
            mIPCSemName[i] = '/';
    mIPCSem = ::CreateMutex (NULL, FALSE, mIPCSemName);
    ComAssertMsgRet (mIPCSem,
                     ("Cannot create IPC mutex '%ls', err=%d",
                      mIPCSemName.raw(), ::GetLastError()),
                     E_FAIL);
#elif defined(RT_OS_OS2)
    Utf8Str ipcSem = Utf8StrFmt ("\\SEM32\\VBOX\\VM\\{%RTuuid}",
                                 aMachine->mData->mUuid.raw());
    mIPCSemName = ipcSem;
    APIRET arc = ::DosCreateMutexSem ((PSZ) ipcSem.raw(), &mIPCSem, 0, FALSE);
    ComAssertMsgRet (arc == NO_ERROR,
                     ("Cannot create IPC mutex '%s', arc=%ld",
                      ipcSem.raw(), arc),
                     E_FAIL);
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
# ifdef VBOX_WITH_NEW_SYS_V_KEYGEN
    AssertCompileSize(key_t, 4);
    key_t key;
    mIPCSem = -1;
    mIPCKey = "0";
    for (uint32_t i = 0; i < 1 << 24; i++)
    {
        key = ((uint32_t)'V' << 24) | i;
        int sem = ::semget (key, 1, S_IRUSR | S_IWUSR | IPC_CREAT | IPC_EXCL);
        if (sem >= 0 || (errno != EEXIST && errno != EACCES))
        {
            mIPCSem = sem;
            if (sem >= 0)
                mIPCKey = BstrFmt ("%u", key);
            break;
        }
    }
# else /* !VBOX_WITH_NEW_SYS_V_KEYGEN */
    Utf8Str semName = aMachine->mData->mConfigFileFull;
    char *pszSemName = NULL;
    RTStrUtf8ToCurrentCP (&pszSemName, semName);
    key_t key = ::ftok (pszSemName, 'V');
    RTStrFree (pszSemName);

    mIPCSem = ::semget (key, 1, S_IRWXU | S_IRWXG | S_IRWXO | IPC_CREAT);
# endif /* !VBOX_WITH_NEW_SYS_V_KEYGEN */

    int errnoSave = errno;
    if (mIPCSem < 0 && errnoSave == ENOSYS)
    {
        setError (E_FAIL,
                  tr ("Cannot create IPC semaphore. Most likely your host kernel lacks "
                      "support for SysV IPC. Check the host kernel configuration for "
                      "CONFIG_SYSVIPC=y"));
        return E_FAIL;
    }
    ComAssertMsgRet (mIPCSem >= 0, ("Cannot create IPC semaphore, errno=%d", errnoSave),
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

    mStorageControllers.allocate();
    StorageControllerList::const_iterator it = aMachine->mStorageControllers->begin();
    while (it != aMachine->mStorageControllers->end())
    {
        ComObjPtr <StorageController> ctl;
        ctl.createObject();
        ctl->init(this, *it);
        mStorageControllers->push_back (ctl);
        ++ it;
    }

    unconst (mBIOSSettings).createObject();
    mBIOSSettings->init (this, aMachine->mBIOSSettings);
#ifdef VBOX_WITH_VRDP
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
    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
    {
        unconst (mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->init (this, aMachine->mSerialPorts [slot]);
    }
    /* create a list of parallel ports that will be mutable */
    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
    {
        unconst (mParallelPorts [slot]).createObject();
        mParallelPorts [slot]->init (this, aMachine->mParallelPorts [slot]);
    }
    /* create another USB controller object that will be mutable */
    unconst (mUSBController).createObject();
    mUSBController->init (this, aMachine->mUSBController);

    /* create a list of network adapters that will be mutable */
    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
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
# ifdef VBOX_WITH_NEW_SYS_V_KEYGEN
        mIPCKey = "0";
# endif /* VBOX_WITH_NEW_SYS_V_KEYGEN */
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

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    unregisterMetrics (mParent->performanceCollector(), mPeer);
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    MachineState_T lastState = mData->mMachineState;

    if (aReason == Uninit::Abnormal)
    {
        LogWarningThisFunc (("ABNORMAL client termination! (wasBusy=%d)\n",
                             Global::IsOnlineOrTransient (lastState)));

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
    else if (!mSnapshotData.mSnapshot.isNull())
    {
        LogWarningThisFunc (("canceling untaken snapshot!\n"));
        endTakingSnapshot (FALSE /* aSuccess  */);
    }

#ifdef VBOX_WITH_USB
    /* release all captured USB devices */
    if (aReason == Uninit::Abnormal && Global::IsOnline (lastState))
    {
        /* Console::captureUSBDevices() is called in the VM process only after
         * setting the machine state to Starting or Restoring.
         * Console::detachAllUSBDevices() will be called upon successful
         * termination. So, we need to release USB devices only if there was
         * an abnormal termination of a running VM.
         *
         * This is identical to SessionMachine::DetachAllUSBDevices except
         * for the aAbnormal argument. */
        HRESULT rc = mUSBController->notifyProxy (false /* aInsertFilters */);
        AssertComRC (rc);
        NOREF (rc);

        USBProxyService *service = mParent->host()->usbProxyService();
        if (service)
            service->detachAllDevicesFromVM (this, true /* aDone */, true /* aAbnormal */);
    }
#endif /* VBOX_WITH_USB */

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
     *  but the VirtualBox release event comes first to the server process.
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
# ifdef VBOX_WITH_NEW_SYS_V_KEYGEN
    mIPCKey = "0";
# endif /* VBOX_WITH_NEW_SYS_V_KEYGEN */
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

// util::Lockable interface
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
STDMETHODIMP SessionMachine::UpdateState (MachineState_T aMachineState)
{
    return setMachineState (aMachineState);
}

/**
 *  @note Locks this object for reading.
 */
STDMETHODIMP SessionMachine::GetIPCId (BSTR *aId)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);

#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    mIPCSemName.cloneTo (aId);
    return S_OK;
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
# ifdef VBOX_WITH_NEW_SYS_V_KEYGEN
    mIPCKey.cloneTo (aId);
# else /* !VBOX_WITH_NEW_SYS_V_KEYGEN */
    mData->mConfigFileFull.cloneTo (aId);
# endif /* !VBOX_WITH_NEW_SYS_V_KEYGEN */
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

    CheckComArgNotNull (aUSBDevice);
    CheckComArgOutPointerValid (aMatched);

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
STDMETHODIMP SessionMachine::CaptureUSBDevice (IN_GUID aId)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

#ifdef VBOX_WITH_USB
    /* if captureDeviceForVM() fails, it must have set extended error info */
    MultiResult rc = mParent->host()->checkUSBProxyService();
    CheckComRCReturnRC (rc);

    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn (service, E_FAIL);
    return service->captureDeviceForVM (this, aId);
#else
    return E_NOTIMPL;
#endif
}

/**
 *  @note Locks the same as Host::detachUSBDevice() does.
 */
STDMETHODIMP SessionMachine::DetachUSBDevice (IN_GUID aId, BOOL aDone)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn (service, E_FAIL);
    return service->detachDeviceFromVM (this, aId, !!aDone);
#else
    return E_NOTIMPL;
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

    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn (service, E_FAIL);
    return service->autoCaptureDevicesForVM (this);
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
STDMETHODIMP SessionMachine::DetachAllUSBDevices (BOOL aDone)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    HRESULT rc = mUSBController->notifyProxy (false /* aInsertFilters */);
    AssertComRC (rc);
    NOREF (rc);

    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn (service, E_FAIL);
    return service->detachAllDevicesFromVM (this, !!aDone, false /* aAbnormal */);
#else
    return S_OK;
#endif
}

/**
 *  @note Locks this object for writing.
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

    AutoWriteLock alock (this);

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

        /*  Create the progress object the client will use to wait until
         * #checkForDeath() is called to uninitialize this session object after
         * it releases the IPC semaphore. */
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
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::BeginSavingState (IProgress *aProgress, BSTR *aStateFilePath)
{
    LogFlowThisFuncEnter();

    AssertReturn (aProgress, E_INVALIDARG);
    AssertReturn (aStateFilePath, E_POINTER);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

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
        stateFilePath = Utf8StrFmt ("%ls%c{%RTuuid}.sav",
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
 *  @note Locks mParent + this object for writing.
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
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::AdoptSavedState (IN_BSTR aSavedStateFile)
{
    LogFlowThisFunc (("\n"));

    AssertReturn (aSavedStateFile, E_INVALIDARG);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    AssertReturn (mData->mMachineState == MachineState_PoweredOff ||
                  mData->mMachineState == MachineState_Aborted,
                  E_FAIL);

    Utf8Str stateFilePathFull = aSavedStateFile;
    int vrc = calculateFullPath (stateFilePathFull, stateFilePathFull);
    if (RT_FAILURE (vrc))
        return setError (VBOX_E_FILE_ERROR,
            tr ("Invalid saved state file path '%ls' (%Rrc)"),
                aSavedStateFile, vrc);

    mSSData->mStateFilePath = stateFilePathFull;

    /* The below setMachineState() will detect the state transition and will
     * update the settings file */

    return setMachineState (MachineState_Saved);
}

/**
 *  @note Locks mParent + this object for writing.
 */
STDMETHODIMP SessionMachine::BeginTakingSnapshot (
    IConsole *aInitiator, IN_BSTR aName, IN_BSTR aDescription,
    IProgress *aProgress, BSTR *aStateFilePath,
    IProgress **aServerProgress)
{
    LogFlowThisFuncEnter();

    AssertReturn (aInitiator && aName, E_INVALIDARG);
    AssertReturn (aStateFilePath && aServerProgress, E_POINTER);

    LogFlowThisFunc (("aName='%ls'\n", aName));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* saveSettings() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    AssertReturn ((!Global::IsOnlineOrTransient (mData->mMachineState) ||
                   mData->mMachineState == MachineState_Paused) &&
                  mSnapshotData.mLastState == MachineState_Null &&
                  mSnapshotData.mSnapshot.isNull() &&
                  mSnapshotData.mServerProgress.isNull() &&
                  mSnapshotData.mCombinedProgress.isNull(),
                  E_FAIL);

    bool takingSnapshotOnline = mData->mMachineState == MachineState_Paused;

    if (!takingSnapshotOnline && mData->mMachineState != MachineState_Saved)
    {
        /* save all current settings to ensure current changes are committed and
         * hard disks are fixed up */
        HRESULT rc = saveSettings();
        CheckComRCReturnRC (rc);
    }

    /// @todo NEWMEDIA so far, we decided to allow for Writhethrough hard disks
    /// when taking snapshots putting all the responsibility to the user...
#if 0
    /* check that there are no Writethrough hard disks attached */
    for (HDData::AttachmentList::const_iterator
         it = mHDData->mAttachments.begin();
         it != mHDData->mAttachments.end();
         ++ it)
    {
        ComObjPtr <HardDisk> hd = (*it)->hardDisk();
        AutoReadLock hdLock (hd);
        if (hd->type() == HardDiskType_Writethrough)
            return setError (E_FAIL,
                tr ("Cannot take a snapshot because the Writethrough hard disk "
                    "'%ls' is attached to this virtual machine"),
                hd->locationFull().raw());
    }
#endif

    AssertReturn (aProgress || !takingSnapshotOnline, E_FAIL);

    /* create an ID for the snapshot */
    Guid snapshotId;
    snapshotId.create();

    Bstr stateFilePath;
    /* stateFilePath is null when the machine is not online nor saved */
    if (takingSnapshotOnline || mData->mMachineState == MachineState_Saved)
        stateFilePath = Utf8StrFmt ("%ls%c{%RTuuid}.sav",
                                    mUserData->mSnapshotFolderFull.raw(),
                                    RTPATH_DELIMITER,
                                    snapshotId.ptr());

    /* ensure the directory for the saved state file exists */
    if (stateFilePath)
    {
        HRESULT rc = VirtualBox::ensureFilePathExists (Utf8Str (stateFilePath));
        CheckComRCReturnRC (rc);
    }

    /* create a snapshot machine object */
    ComObjPtr <SnapshotMachine> snapshotMachine;
    snapshotMachine.createObject();
    HRESULT rc = snapshotMachine->init (this, snapshotId, stateFilePath);
    AssertComRCReturn (rc, rc);

    Bstr progressDesc = BstrFmt (tr ("Taking snapshot of virtual machine '%ls'"),
                                 mUserData->mName.raw());
    Bstr firstOpDesc = Bstr (tr ("Preparing to take snapshot"));

    /* create a server-side progress object (it will be descriptionless when we
     * need to combine it with the VM-side progress, i.e. when we're taking a
     * snapshot online). The number of operations is: 1 (preparing) + # of
     * hard disks + 1 (if the state is saved so we need to copy it)
     */
    ComObjPtr <Progress> serverProgress;
    serverProgress.createObject();
    {
        ULONG opCount = 1 + (ULONG)mHDData->mAttachments.size();
        if (mData->mMachineState == MachineState_Saved)
            opCount ++;
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

    /* create and start the task on a separate thread (note that it will not
     * start working until we release alock) */
    TakeSnapshotTask *task = new TakeSnapshotTask (this);
    int vrc = RTThreadCreate (NULL, taskHandler,
                              (void *) task,
                              0, RTTHREADTYPE_MAIN_WORKER, 0, "TakeSnapshot");
    if (RT_FAILURE (vrc))
    {
        snapshot->uninit();
        delete task;
        ComAssertRCRet (vrc, E_FAIL);
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
 * @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::EndTakingSnapshot (BOOL aSuccess)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    AssertReturn (!aSuccess ||
                  (mData->mMachineState == MachineState_Saving &&
                   mSnapshotData.mLastState != MachineState_Null &&
                   !mSnapshotData.mSnapshot.isNull() &&
                   !mSnapshotData.mServerProgress.isNull() &&
                   !mSnapshotData.mCombinedProgress.isNull()),
                  E_FAIL);

    /* set the state to the state we had when BeginTakingSnapshot() was called
     * (this is expected by Console::TakeSnapshot() and
     * Console::saveStateThread()) */
    setMachineState (mSnapshotData.mLastState);

    return endTakingSnapshot (aSuccess);
}

/**
 *  @note Locks mParent + this + children objects for writing!
 */
STDMETHODIMP SessionMachine::DiscardSnapshot (
    IConsole *aInitiator, IN_GUID aId,
    MachineState_T *aMachineState, IProgress **aProgress)
{
    LogFlowThisFunc (("\n"));

    Guid id = aId;
    AssertReturn (aInitiator && !id.isEmpty(), E_INVALIDARG);
    AssertReturn (aMachineState && aProgress, E_POINTER);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* saveSettings() needs mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    ComAssertRet (!Global::IsOnlineOrTransient (mData->mMachineState), E_FAIL);

    ComObjPtr <Snapshot> snapshot;
    HRESULT rc = findSnapshot (id, snapshot, true /* aSetError */);
    CheckComRCReturnRC (rc);

    AutoWriteLock snapshotLock (snapshot);

    {
        AutoWriteLock chLock (snapshot->childrenLock());
        size_t childrenCount = snapshot->children().size();
        if (childrenCount > 1)
            return setError (VBOX_E_INVALID_OBJECT_STATE,
                tr ("Snapshot '%ls' of the machine '%ls' has more than one "
                    "child snapshot (%d)"),
                snapshot->data().mName.raw(), mUserData->mName.raw(),
                childrenCount);
    }

    /* If the snapshot being discarded is the current one, ensure current
     * settings are committed and saved.
     */
    if (snapshot == mData->mCurrentSnapshot)
    {
        if (isModified())
        {
            rc = saveSettings();
            CheckComRCReturnRC (rc);
        }
    }

    /* create a progress object. The number of operations is:
     *   1 (preparing) + # of hard disks + 1 if the snapshot is online
     */
    ComObjPtr <Progress> progress;
    progress.createObject();
    rc = progress->init (mParent, aInitiator,
                         Bstr (Utf8StrFmt (tr ("Discarding snapshot '%ls'"),
                             snapshot->data().mName.raw())),
                         FALSE /* aCancelable */,
                         1 + (ULONG)snapshot->data().mMachine->mHDData->mAttachments.size() +
                         (snapshot->stateFilePath().isNull() ? 0 : 1),
                         Bstr (tr ("Preparing to discard snapshot")));
    AssertComRCReturn (rc, rc);

    /* create and start the task on a separate thread */
    DiscardSnapshotTask *task = new DiscardSnapshotTask (this, progress, snapshot);
    int vrc = RTThreadCreate (NULL, taskHandler,
                              (void *) task,
                              0, RTTHREADTYPE_MAIN_WORKER, 0, "DiscardSnapshot");
    if (RT_FAILURE (vrc))
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
 *  @note Locks this + children objects for writing!
 */
STDMETHODIMP SessionMachine::DiscardCurrentState (
    IConsole *aInitiator, MachineState_T *aMachineState, IProgress **aProgress)
{
    LogFlowThisFunc (("\n"));

    AssertReturn (aInitiator, E_INVALIDARG);
    AssertReturn (aMachineState && aProgress, E_POINTER);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    ComAssertRet (!Global::IsOnlineOrTransient (mData->mMachineState), E_FAIL);

    if (mData->mCurrentSnapshot.isNull())
        return setError (VBOX_E_INVALID_OBJECT_STATE,
            tr ("Could not discard the current state of the machine '%ls' "
                "because it doesn't have any snapshots"),
            mUserData->mName.raw());

    /* create a progress object. The number of operations is: 1 (preparing) + #
     * of hard disks + 1 (if we need to copy the saved state file) */
    ComObjPtr <Progress> progress;
    progress.createObject();
    {
        ULONG opCount = 1 + (ULONG)mData->mCurrentSnapshot->data()
                                       .mMachine->mHDData->mAttachments.size();
        if (mData->mCurrentSnapshot->stateFilePath())
            ++ opCount;
        progress->init (mParent, aInitiator,
                        Bstr (tr ("Discarding current machine state")),
                        FALSE /* aCancelable */, opCount,
                        Bstr (tr ("Preparing to discard current state")));
    }

    /* create and start the task on a separate thread (note that it will not
     * start working until we release alock) */
    DiscardCurrentStateTask *task =
        new DiscardCurrentStateTask (this, progress, false /* discardCurSnapshot */);
    int vrc = RTThreadCreate (NULL, taskHandler,
                              (void *) task,
                              0, RTTHREADTYPE_MAIN_WORKER, 0, "DiscardCurState");
    if (RT_FAILURE (vrc))
    {
        delete task;
        ComAssertRCRet (vrc, E_FAIL);
    }

    /* set the proper machine state (note: after creating a Task instance) */
    setMachineState (MachineState_Discarding);

    /* return the progress to the caller */
    progress.queryInterfaceTo (aProgress);

    /* return the new state to the caller */
    *aMachineState = mData->mMachineState;

    return S_OK;
}

/**
 *  @note Locks thos object for writing!
 */
STDMETHODIMP SessionMachine::DiscardCurrentSnapshotAndState (
    IConsole *aInitiator, MachineState_T *aMachineState, IProgress **aProgress)
{
    LogFlowThisFunc (("\n"));

    AssertReturn (aInitiator, E_INVALIDARG);
    AssertReturn (aMachineState && aProgress, E_POINTER);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    ComAssertRet (!Global::IsOnlineOrTransient (mData->mMachineState), E_FAIL);

    if (mData->mCurrentSnapshot.isNull())
        return setError (VBOX_E_INVALID_OBJECT_STATE,
            tr ("Could not discard the current state of the machine '%ls' "
                "because it doesn't have any snapshots"),
            mUserData->mName.raw());

    /* create a progress object. The number of operations is:
     *   1 (preparing) + # of hard disks in the current snapshot +
     *   # of hard disks in the previous snapshot +
     *   1 if we need to copy the saved state file of the previous snapshot +
     *   1 if the current snapshot is online
     * or (if there is no previous snapshot):
     *   1 (preparing) + # of hard disks in the current snapshot * 2 +
     *   1 if we need to copy the saved state file of the current snapshot * 2
     */
    ComObjPtr <Progress> progress;
    progress.createObject();
    {
        ComObjPtr <Snapshot> curSnapshot = mData->mCurrentSnapshot;
        ComObjPtr <Snapshot> prevSnapshot = mData->mCurrentSnapshot->parent();

        ULONG opCount = 1;
        if (prevSnapshot)
        {
            opCount += (ULONG)curSnapshot->data().mMachine->mHDData->mAttachments.size();
            opCount += (ULONG)prevSnapshot->data().mMachine->mHDData->mAttachments.size();
            if (prevSnapshot->stateFilePath())
                ++ opCount;
            if (curSnapshot->stateFilePath())
                ++ opCount;
        }
        else
        {
            opCount +=
                (ULONG)curSnapshot->data().mMachine->mHDData->mAttachments.size() * 2;
            if (curSnapshot->stateFilePath())
                opCount += 2;
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
                              0, RTTHREADTYPE_MAIN_WORKER, 0, "DiscardCurStSnp");
    if (RT_FAILURE (vrc))
    {
        delete task;
        ComAssertRCRet (vrc, E_FAIL);
    }

    /* set the proper machine state (note: after creating a Task instance) */
    setMachineState (MachineState_Discarding);

    /* return the progress to the caller */
    progress.queryInterfaceTo (aProgress);

    /* return the new state to the caller */
    *aMachineState = mData->mMachineState;

    return S_OK;
}

STDMETHODIMP SessionMachine::
PullGuestProperties (ComSafeArrayOut (BSTR, aNames),
                     ComSafeArrayOut (BSTR, aValues),
                     ComSafeArrayOut (ULONG64, aTimestamps),
                     ComSafeArrayOut (BSTR, aFlags))
{
    LogFlowThisFunc (("\n"));

#ifdef VBOX_WITH_GUEST_PROPS
    using namespace guestProp;

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);

    AssertReturn (!ComSafeArrayOutIsNull (aNames), E_POINTER);
    AssertReturn (!ComSafeArrayOutIsNull (aValues), E_POINTER);
    AssertReturn (!ComSafeArrayOutIsNull (aTimestamps), E_POINTER);
    AssertReturn (!ComSafeArrayOutIsNull (aFlags), E_POINTER);

    size_t cEntries = mHWData->mGuestProperties.size();
    com::SafeArray <BSTR> names (cEntries);
    com::SafeArray <BSTR> values (cEntries);
    com::SafeArray <ULONG64> timestamps (cEntries);
    com::SafeArray <BSTR> flags (cEntries);
    unsigned i = 0;
    for (HWData::GuestPropertyList::iterator it = mHWData->mGuestProperties.begin();
         it != mHWData->mGuestProperties.end(); ++it)
    {
        char szFlags[MAX_FLAGS_LEN + 1];
        it->mName.cloneTo (&names[i]);
        it->mValue.cloneTo (&values[i]);
        timestamps[i] = it->mTimestamp;
        writeFlags (it->mFlags, szFlags);
        Bstr (szFlags).cloneTo (&flags[i]);
        ++i;
    }
    names.detachTo (ComSafeArrayOutArg (aNames));
    values.detachTo (ComSafeArrayOutArg (aValues));
    timestamps.detachTo (ComSafeArrayOutArg (aTimestamps));
    flags.detachTo (ComSafeArrayOutArg (aFlags));
    mHWData->mPropertyServiceActive = true;
    return S_OK;
#else
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP SessionMachine::PushGuestProperties (ComSafeArrayIn (IN_BSTR, aNames),
                                                  ComSafeArrayIn (IN_BSTR, aValues),
                                                  ComSafeArrayIn (ULONG64, aTimestamps),
                                                  ComSafeArrayIn (IN_BSTR, aFlags))
{
    LogFlowThisFunc (("\n"));

#ifdef VBOX_WITH_GUEST_PROPS
    using namespace guestProp;

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    /* Temporarily reset the registered flag, so that our machine state
     * changes (i.e. mHWData.backup()) succeed.  (isMutable() used in
     * all setters will return FALSE for a Machine instance if mRegistered
     * is TRUE).  This is copied from registeredInit(), and may or may not be
     * the right way to handle this. */
    mData->mRegistered = FALSE;
    HRESULT rc = checkStateDependency (MutableStateDep);
    LogRel (("checkStateDependency (MutableStateDep) returned 0x%x\n", rc));
    CheckComRCReturnRC (rc);

    // ComAssertRet (mData->mMachineState < MachineState_Running, E_FAIL);

    AssertReturn (!ComSafeArrayInIsNull (aNames), E_POINTER);
    AssertReturn (!ComSafeArrayInIsNull (aValues), E_POINTER);
    AssertReturn (!ComSafeArrayInIsNull (aTimestamps), E_POINTER);
    AssertReturn (!ComSafeArrayInIsNull (aFlags), E_POINTER);

    com::SafeArray <IN_BSTR> names (ComSafeArrayInArg (aNames));
    com::SafeArray <IN_BSTR> values (ComSafeArrayInArg (aValues));
    com::SafeArray <ULONG64> timestamps (ComSafeArrayInArg (aTimestamps));
    com::SafeArray <IN_BSTR> flags (ComSafeArrayInArg (aFlags));
    DiscardSettings();
    mHWData.backup();
    mHWData->mGuestProperties.erase (mHWData->mGuestProperties.begin(),
                                    mHWData->mGuestProperties.end());
    for (unsigned i = 0; i < names.size(); ++i)
    {
        uint32_t fFlags = NILFLAG;
        validateFlags (Utf8Str (flags[i]).raw(), &fFlags);
        HWData::GuestProperty property = { names[i], values[i], timestamps[i], fFlags };
        mHWData->mGuestProperties.push_back (property);
    }
    mHWData->mPropertyServiceActive = false;
    alock.unlock();
    SaveSettings();
    /* Restore the mRegistered flag. */
    mData->mRegistered = TRUE;
    return S_OK;
#else
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP SessionMachine::PushGuestProperty (IN_BSTR aName, IN_BSTR aValue,
                                                ULONG64 aTimestamp, IN_BSTR aFlags)
{
    LogFlowThisFunc (("\n"));

#ifdef VBOX_WITH_GUEST_PROPS
    using namespace guestProp;

    CheckComArgNotNull (aName);
    if ((aValue != NULL) && (!VALID_PTR (aValue) || !VALID_PTR (aFlags)))
        return E_POINTER;  /* aValue can be NULL to indicate deletion */

    Utf8Str utf8Name (aName);
    Utf8Str utf8Flags (aFlags);
    Utf8Str utf8Patterns (mHWData->mGuestPropertyNotificationPatterns);
    if (   utf8Name.isNull()
        || ((aFlags != NULL) && utf8Flags.isNull())
        || utf8Patterns.isNull()
       )
        return E_OUTOFMEMORY;

    uint32_t fFlags = NILFLAG;
    if ((aFlags != NULL) && RT_FAILURE (validateFlags (utf8Flags.raw(), &fFlags)))
        return E_INVALIDARG;

    bool matchAll = false;
    if (utf8Patterns.length() == 0)
        matchAll = true;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = checkStateDependency (MutableStateDep);
    CheckComRCReturnRC (rc);

    mHWData.backup();
    for (HWData::GuestPropertyList::iterator iter = mHWData->mGuestProperties.begin();
         iter != mHWData->mGuestProperties.end(); ++iter)
        if (aName == iter->mName)
        {
            mHWData->mGuestProperties.erase (iter);
            break;
        }
    if (aValue != NULL)
    {
        HWData::GuestProperty property = { aName, aValue, aTimestamp, fFlags };
        mHWData->mGuestProperties.push_back (property);
    }

    /* send a callback notification if appropriate */
    alock.leave();
    if (   matchAll
        || RTStrSimplePatternMultiMatch (utf8Patterns.raw(), RTSTR_MAX,
                                         utf8Name.raw(), RTSTR_MAX, NULL)
       )
        mParent->onGuestPropertyChange (mData->mUuid, aName, aValue, aFlags);

    return S_OK;
#else
    ReturnComNotImplemented();
#endif
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Called from the client watcher thread to check for expected or unexpected
 * death of the client process that has a direct session to this machine.
 *
 * On Win32 and on OS/2, this method is called only when we've got the
 * mutex (i.e. the client has either died or terminated normally) so it always
 * returns @c true (the client is terminated, the session machine is
 * uninitialized).
 *
 * On other platforms, the method returns @c true if the client process has
 * terminated normally or abnormally and the session machine was uninitialized,
 * and @c false if the client process is still alive.
 *
 * @note Locks this object for writing.
 */
bool SessionMachine::checkForDeath()
{
    Uninit::Reason reason;
    bool terminated = false;

    /* Enclose autoCaller with a block because calling uninit() from under it
     * will deadlock. */
    {
        AutoCaller autoCaller (this);
        if (!autoCaller.isOk())
        {
            /* return true if not ready, to cause the client watcher to exclude
             * the corresponding session from watching */
            LogFlowThisFunc (("Already uninitialized!"));
            return true;
        }

        AutoWriteLock alock (this);

        /* Determine the reason of death: if the session state is Closing here,
         * everything is fine. Otherwise it means that the client did not call
         * OnSessionEnd() before it released the IPC semaphore. This may happen
         * either because the client process has abnormally terminated, or
         * because it simply forgot to call ISession::Close() before exiting. We
         * threat the latter also as an abnormal termination (see
         * Session::uninit() for details). */
        reason = mData->mSession.mState == SessionState_Closing ?
                 Uninit::Normal :
                 Uninit::Abnormal;

#if defined(RT_OS_WINDOWS)

        AssertMsg (mIPCSem, ("semaphore must be created"));

        /* release the IPC mutex */
        ::ReleaseMutex (mIPCSem);

        terminated = true;

#elif defined(RT_OS_OS2)

        AssertMsg (mIPCSem, ("semaphore must be created"));

        /* release the IPC mutex */
        ::DosReleaseMutexSem (mIPCSem);

        terminated = true;

#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)

        AssertMsg (mIPCSem >= 0, ("semaphore must be created"));

        int val = ::semctl (mIPCSem, 0, GETVAL);
        if (val > 0)
        {
            /* the semaphore is signaled, meaning the session is terminated */
            terminated = true;
        }

#else
# error "Port me!"
#endif

    } /* AutoCaller block */

    if (terminated)
        uninit (reason);

    return terminated;
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
        AutoReadLock alock (this);
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
        AutoReadLock alock (this);
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
HRESULT SessionMachine::onNetworkAdapterChange (INetworkAdapter *networkAdapter)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReadLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnNetworkAdapterChange (networkAdapter);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onSerialPortChange (ISerialPort *serialPort)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReadLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnSerialPortChange (serialPort);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onParallelPortChange (IParallelPort *parallelPort)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReadLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnParallelPortChange (parallelPort);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onStorageControllerChange ()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReadLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnStorageControllerChange ();
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
        AutoReadLock alock (this);
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
        AutoReadLock alock (this);
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
        AutoReadLock alock (this);
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

    AutoReadLock alock (this);

#ifdef VBOX_WITH_USB
    switch (mData->mMachineState)
    {
        case MachineState_Starting:
        case MachineState_Restoring:
        case MachineState_Paused:
        case MachineState_Running:
            return mUSBController->hasMatchingFilter (aDevice, aMaskedIfs);
        default: break;
    }
#endif
    return false;
}

/**
 *  @note The calls shall hold no locks. Will temporarily lock this object for reading.
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
        AutoReadLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* fail on notifications sent after #OnSessionEnd() is called, it is
     * expected by the caller */
    if (!directControl)
        return E_FAIL;

    /* No locks should be held at this point. */
    AssertMsg (RTThreadGetWriteLockCount (RTThreadSelf()) == 0, ("%d\n", RTThreadGetWriteLockCount (RTThreadSelf())));
    AssertMsg (RTThreadGetReadLockCount (RTThreadSelf()) == 0, ("%d\n", RTThreadGetReadLockCount (RTThreadSelf())));

    return directControl->OnUSBDeviceAttach (aDevice, aError, aMaskedIfs);
}

/**
 *  @note The calls shall hold no locks. Will temporarily lock this object for reading.
 */
HRESULT SessionMachine::onUSBDeviceDetach (IN_GUID aId,
                                           IVirtualBoxErrorInfo *aError)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);

    /* This notification may happen after the machine object has been
     * uninitialized (the session was closed), so don't assert. */
    CheckComRCReturnRC (autoCaller.rc());

    ComPtr <IInternalSessionControl> directControl;
    {
        AutoReadLock alock (this);
        directControl = mData->mSession.mDirectControl;
    }

    /* fail on notifications sent after #OnSessionEnd() is called, it is
     * expected by the caller */
    if (!directControl)
        return E_FAIL;

    /* No locks should be held at this point. */
    AssertMsg (RTThreadGetWriteLockCount (RTThreadSelf()) == 0, ("%d\n", RTThreadGetWriteLockCount (RTThreadSelf())));
    AssertMsg (RTThreadGetReadLockCount (RTThreadSelf()) == 0, ("%d\n", RTThreadGetReadLockCount (RTThreadSelf())));

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

    /* saveSettings() needs mParent lock */
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
 * Helper method to finalize taking a snapshot. Gets called to finalize the
 * "take snapshot" procedure.
 *
 * Expected to be called after completing *all* the tasks related to taking the
 * snapshot, either successfully or unsuccessfilly.
 *
 * @param aSuccess  TRUE if the snapshot has been taken successfully.
 *
 * @note Locks this objects for writing.
 */
HRESULT SessionMachine::endTakingSnapshot (BOOL aSuccess)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    AssertReturn (!mSnapshotData.mSnapshot.isNull(), E_FAIL);

    MultiResult rc (S_OK);

    if (aSuccess)
    {
        /* the server progress must be completed on success */
        Assert (mSnapshotData.mServerProgress->completed());

        mData->mCurrentSnapshot = mSnapshotData.mSnapshot;

        /* memorize the first snapshot if necessary */
        if (!mData->mFirstSnapshot)
            mData->mFirstSnapshot = mData->mCurrentSnapshot;

        int opFlags = SaveSS_AddOp | SaveSS_CurrentId;
        if (!Global::IsOnline (mSnapshotData.mLastState))
        {
            /* the machine was powered off or saved when taking a snapshot, so
             * reset the mCurrentStateModified flag */
            mData->mCurrentStateModified = FALSE;
            opFlags |= SaveSS_CurStateModified;
        }

        rc = saveSnapshotSettings (mSnapshotData.mSnapshot, opFlags);
    }

    if (aSuccess && SUCCEEDED (rc))
    {
        bool online = Global::IsOnline (mSnapshotData.mLastState);

        /* associate old hard disks with the snapshot and do locking/unlocking*/
        fixupHardDisks(true /* aCommit */, online);

        /* inform callbacks */
        mParent->onSnapshotTaken (mData->mUuid,
                                  mSnapshotData.mSnapshot->data().mId);
    }
    else
    {
        /* wait for the completion of the server progress (diff VDI creation) */
        /// @todo (dmik) later, we will definitely want to cancel it instead
        // (when the cancel function is implemented)
        mSnapshotData.mServerProgress->WaitForCompletion (-1);

        /* delete all differencing hard disks created (this will also attach
         * their parents back by rolling back mHDData) */
        fixupHardDisks(false /* aCommit */);

        /* delete the saved state file (it might have been already created) */
        if (mSnapshotData.mSnapshot->stateFilePath())
            RTFileDelete (Utf8Str (mSnapshotData.mSnapshot->stateFilePath()));

        mSnapshotData.mSnapshot->uninit();
    }

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
 * Take snapshot task handler. Must be called only by
 * TakeSnapshotTask::handler()!
 *
 * The sole purpose of this task is to asynchronously create differencing VDIs
 * and copy the saved state file (when necessary). The VM process will wait for
 * this task to complete using the mSnapshotData.mServerProgress returned to it.
 *
 * @note Locks this object for writing.
 */
void SessionMachine::takeSnapshotHandler (TakeSnapshotTask & /* aTask */)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);

    LogFlowThisFunc (("state=%d\n", autoCaller.state()));
    if (!autoCaller.isOk())
    {
        /* we might have been uninitialized because the session was accidentally
         * closed by the client, so don't assert */
        LogFlowThisFuncLeave();
        return;
    }

    AutoWriteLock alock (this);

    HRESULT rc = S_OK;

    bool online = Global::IsOnline (mSnapshotData.mLastState);

    LogFlowThisFunc (("Creating differencing hard disks (online=%d)...\n",
                      online));

    mHDData.backup();

    /* create new differencing hard disks and attach them to this machine */
    rc = createImplicitDiffs (mUserData->mSnapshotFolderFull,
                              mSnapshotData.mServerProgress,
                              online);

    if (SUCCEEDED (rc) && mSnapshotData.mLastState == MachineState_Saved)
    {
        Utf8Str stateFrom = mSSData->mStateFilePath;
        Utf8Str stateTo = mSnapshotData.mSnapshot->stateFilePath();

        LogFlowThisFunc (("Copying the execution state from '%s' to '%s'...\n",
                          stateFrom.raw(), stateTo.raw()));

        mSnapshotData.mServerProgress->setNextOperation(Bstr(tr("Copying the execution state")),
                                                        1);        // weight

        /* Leave the lock before a lengthy operation (mMachineState is
         * MachineState_Saving here) */

        alock.leave();

        /* copy the state file */
        int vrc = RTFileCopyEx (stateFrom, stateTo, 0, progressCallback,
                                static_cast <Progress *> (mSnapshotData.mServerProgress));

        alock.enter();

        if (RT_FAILURE (vrc))
            rc = setError (E_FAIL,
                tr ("Could not copy the state file '%s' to '%s' (%Rrc)"),
                stateFrom.raw(), stateTo.raw(), vrc);
    }

    /* we have to call endTakingSnapshot() ourselves if the snapshot was taken
     * offline because the VM process will not do it in this case
     */
    if (!online)
    {
        LogFlowThisFunc (("Finalizing the taken snapshot (rc=%Rhrc)...\n", rc));

        {
            ErrorInfoKeeper eik;

            setMachineState (mSnapshotData.mLastState);
            updateMachineStateOnClient();
        }

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
 * Helper struct for SessionMachine::discardSnapshotHandler().
 */
struct HardDiskDiscardRec
{
    HardDiskDiscardRec() : chain (NULL) {}

    HardDiskDiscardRec (const ComObjPtr<HardDisk> &aHd,
                HardDisk::MergeChain *aChain = NULL)
        : hd (aHd), chain (aChain) {}

    HardDiskDiscardRec (const ComObjPtr<HardDisk> &aHd,
                        HardDisk::MergeChain *aChain,
                        const ComObjPtr<HardDisk> &aReplaceHd,
                        const ComObjPtr<HardDiskAttachment> &aReplaceHda,
                        const Guid &aSnapshotId)
        : hd (aHd), chain (aChain)
        , replaceHd (aReplaceHd), replaceHda (aReplaceHda)
        , snapshotId (aSnapshotId) {}

    ComObjPtr<HardDisk> hd;
    HardDisk::MergeChain *chain;
    /* these are for the replace hard disk case: */
    ComObjPtr<HardDisk> replaceHd;
    ComObjPtr<HardDiskAttachment> replaceHda;
    Guid snapshotId;
};

typedef std::list <HardDiskDiscardRec> HardDiskDiscardRecList;

/**
 * Discard snapshot task handler. Must be called only by
 * DiscardSnapshotTask::handler()!
 *
 * When aTask.subTask is true, the associated progress object is left
 * uncompleted on success. On failure, the progress is marked as completed
 * regardless of this parameter.
 *
 * @note Locks mParent + this + child objects for writing!
 */
void SessionMachine::discardSnapshotHandler (DiscardSnapshotTask &aTask)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);

    LogFlowThisFunc (("state=%d\n", autoCaller.state()));
    if (!autoCaller.isOk())
    {
        /* we might have been uninitialized because the session was accidentally
         * closed by the client, so don't assert */
        aTask.progress->notifyComplete (
            E_FAIL, COM_IIDOF (IMachine), getComponentName(),
            tr ("The session has been accidentally closed"));

        LogFlowThisFuncLeave();
        return;
    }

    /* saveSettings() needs mParent lock */
    AutoWriteLock vboxLock (mParent);

    /* @todo We don't need mParent lock so far so unlock() it. Better is to
     * provide an AutoWriteLock argument that lets create a non-locking
     * instance */
    vboxLock.unlock();

    /* Preseve the {parent, child} lock order for this and snapshot stuff */
    AutoMultiWriteLock3 alock (this->lockHandle(),
                               aTask.snapshot->lockHandle(),
                               aTask.snapshot->childrenLock());

    ComObjPtr <SnapshotMachine> sm = aTask.snapshot->data().mMachine;
    /* no need to lock the snapshot machine since it is const by definiton */

    HRESULT rc = S_OK;

    /* save the snapshot ID (for callbacks) */
    Guid snapshotId = aTask.snapshot->data().mId;

    HardDiskDiscardRecList toDiscard;

    bool settingsChanged = false;

    try
    {
        /* first pass: */
        LogFlowThisFunc (("1: Checking hard disk merge prerequisites...\n"));

        for (HDData::AttachmentList::const_iterator it =
             sm->mHDData->mAttachments.begin();
             it != sm->mHDData->mAttachments.end();
             ++ it)
        {
            ComObjPtr<HardDiskAttachment> hda = *it;
            ComObjPtr<HardDisk> hd = hda->hardDisk();

            /* HardDisk::prepareDiscard() reqiuires a write lock */
            AutoWriteLock hdLock (hd);

            if (hd->type() != HardDiskType_Normal)
            {
                /* skip writethrough hard disks */

                Assert (hd->type() == HardDiskType_Writethrough);

                rc = aTask.progress->setNextOperation(BstrFmt(tr("Skipping writethrough hard disk '%s'"),
                                                              hd->root()->name().raw()),
                                                      1); // weight
                CheckComRCThrowRC (rc);

                continue;
            }

            HardDisk::MergeChain *chain = NULL;

            /* needs to be discarded (merged with the child if any), check
             * prerequisites */
            rc = hd->prepareDiscard (chain);
            CheckComRCThrowRC (rc);

            if (hd->parent().isNull() && chain != NULL)
            {
                /* it's a base hard disk so it will be a backward merge of its
                 * only child to it (prepareDiscard() does necessary checks). We
                 * need then to update the attachment that refers to the child
                 * to refer to the parent insead. Don't forget to detach the
                 * child (otherwise mergeTo() called by discard() will assert
                 * because it will be going to delete the child) */

                /* The below assert would be nice but I don't want to move
                 * HardDisk::MergeChain to the header just for that
                 * Assert (!chain->isForward()); */

                Assert (hd->children().size() == 1);

                ComObjPtr<HardDisk> replaceHd = hd->children().front();

                Assert (replaceHd->backRefs().front().machineId == mData->mUuid);
                Assert (replaceHd->backRefs().front().snapshotIds.size() <= 1);

                Guid snapshotId;
                if (replaceHd->backRefs().front().snapshotIds.size() == 1)
                    snapshotId = replaceHd->backRefs().front().snapshotIds.front();

                HRESULT rc2 = S_OK;

                /* adjust back references */
                rc2 = replaceHd->detachFrom (mData->mUuid, snapshotId);
                AssertComRC (rc2);

                rc2 = hd->attachTo (mData->mUuid, snapshotId);
                AssertComRC (rc2);

                /* replace the hard disk in the attachment object */
                HDData::AttachmentList::iterator it;
                if (snapshotId.isEmpty())
                {
                    /* in current state */
                    it = std::find_if (mHDData->mAttachments.begin(),
                                       mHDData->mAttachments.end(),
                                       HardDiskAttachment::RefersTo (replaceHd));
                    AssertBreak (it != mHDData->mAttachments.end());
                }
                else
                {
                    /* in snapshot */
                    ComObjPtr <Snapshot> snapshot;
                    rc2 = findSnapshot (snapshotId, snapshot);
                    AssertComRC (rc2);

                    /* don't lock the snapshot; cannot be modified outside */
                    HDData::AttachmentList &snapAtts =
                        snapshot->data().mMachine->mHDData->mAttachments;
                    it = std::find_if (snapAtts.begin(),
                                       snapAtts.end(),
                                       HardDiskAttachment::RefersTo (replaceHd));
                    AssertBreak (it != snapAtts.end());
                }

                AutoWriteLock attLock (*it);
                (*it)->updateHardDisk (hd, false /* aImplicit */);

                toDiscard.push_back (HardDiskDiscardRec (hd, chain, replaceHd,
                                                         *it, snapshotId));
                continue;
            }

            toDiscard.push_back (HardDiskDiscardRec (hd, chain));
        }

        /* Now we checked that we can successfully merge all normal hard disks
         * (unless a runtime error like end-of-disc happens). Prior to
         * performing the actual merge, we want to discard the snapshot itself
         * and remove it from the XML file to make sure that a possible merge
         * ruintime error will not make this snapshot inconsistent because of
         * the partially merged or corrupted hard disks */

        /* second pass: */
        LogFlowThisFunc (("2: Discarding snapshot...\n"));

        {
            /* for now, the snapshot must have only one child when discarded,
             * or no children at all */
            ComAssertThrow (aTask.snapshot->children().size() <= 1, E_FAIL);

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
                /// @todo NEWMEDIA not really clear why
                ComAssertThrow (
                    !parentSnapshot ||
                    parentSnapshot->data().mMachine->mData->mUuid == mData->mUuid,
                    E_FAIL);
                mData->mCurrentSnapshot = parentSnapshot;

                /* we've changed the base of the current state so mark it as
                 * modified as it no longer guaranteed to be its copy */
                mData->mCurrentStateModified = TRUE;
            }

            if (aTask.snapshot == mData->mFirstSnapshot)
            {
                if (aTask.snapshot->children().size() == 1)
                {
                    ComObjPtr <Snapshot> childSnapshot =
                        aTask.snapshot->children().front();
                    ComAssertThrow (
                        childSnapshot->data().mMachine->mData->mUuid == mData->mUuid,
                        E_FAIL);
                    mData->mFirstSnapshot = childSnapshot;
                }
                else
                    mData->mFirstSnapshot.setNull();
            }

            Bstr stateFilePath = aTask.snapshot->stateFilePath();

            /* Note that discarding the snapshot will deassociate it from the
             * hard disks which will allow the merge+delete operation for them*/
            aTask.snapshot->discard();

            rc = saveSnapshotSettings (parentSnapshot, SaveSS_UpdateAllOp |
                                                       SaveSS_CurrentId |
                                                       SaveSS_CurStateModified);
            CheckComRCThrowRC (rc);

            /// @todo (dmik)
            //  if we implement some warning mechanism later, we'll have
            //  to return a warning if the state file path cannot be deleted
            if (stateFilePath)
            {
                aTask.progress->setNextOperation(Bstr(tr("Discarding the execution state")),
                                                 1);        // weight

                RTFileDelete (Utf8Str (stateFilePath));
            }

            /// @todo NEWMEDIA to provide a good level of fauilt tolerance, we
            /// should restore the shapshot in the snapshot tree if
            /// saveSnapshotSettings fails. Actually, we may call
            /// #saveSnapshotSettings() with a special flag that will tell it to
            /// skip the given snapshot as if it would have been discarded and
            /// only actually discard it if the save operation succeeds.
        }

        /* here we come when we've irrevesibly discarded the snapshot which
         * means that the VM settigns (our relevant changes to mData) need to be
         * saved too */
        /// @todo NEWMEDIA maybe save everything in one operation in place of
        ///  saveSnapshotSettings() above
        settingsChanged = true;

        /* third pass: */
        LogFlowThisFunc (("3: Performing actual hard disk merging...\n"));

        /* leave the locks before the potentially lengthy operation */
        alock.leave();

        /// @todo NEWMEDIA turn the following errors into warnings because the
        /// snapshot itself has been already deleted (and interpret these
        /// warnings properly on the GUI side)

        for (HardDiskDiscardRecList::iterator it = toDiscard.begin();
             it != toDiscard.end();)
        {
            rc = it->hd->discard (aTask.progress, it->chain);
            CheckComRCBreakRC (rc);

            /* prevent from calling cancelDiscard() */
            it = toDiscard.erase (it);
        }

        alock.enter();

        CheckComRCThrowRC (rc);
    }
    catch (HRESULT aRC) { rc = aRC; }

    if FAILED (rc)
    {
        HRESULT rc2 = S_OK;

        /* un-prepare the remaining hard disks */
        for (HardDiskDiscardRecList::const_iterator it = toDiscard.begin();
             it != toDiscard.end(); ++ it)
        {
            it->hd->cancelDiscard (it->chain);

            if (!it->replaceHd.isNull())
            {
                /* undo hard disk replacement */

                rc2 = it->replaceHd->attachTo (mData->mUuid, it->snapshotId);
                AssertComRC (rc2);

                rc2 = it->hd->detachFrom (mData->mUuid, it->snapshotId);
                AssertComRC (rc2);

                AutoWriteLock attLock (it->replaceHda);
                it->replaceHda->updateHardDisk (it->replaceHd, false /* aImplicit */);
            }
        }
    }

    if (!aTask.subTask || FAILED (rc))
    {
        if (!aTask.subTask)
        {
            /* saveSettings() below needs a VirtualBox write lock and we need to
             * leave this object's lock to do this to follow the {parent-child}
             * locking rule. This is the last chance to do that while we are
             * still in a protective state which allows us to temporarily leave
             * the lock */
            alock.unlock();
            vboxLock.lock();
            alock.lock();

            /* preserve existing error info */
            ErrorInfoKeeper eik;

            /* restore the machine state */
            setMachineState (aTask.state);
            updateMachineStateOnClient();

            if (settingsChanged)
                saveSettings (SaveS_InformCallbacksAnyway);
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
 * Discard current state task handler. Must be called only by
 * DiscardCurrentStateTask::handler()!
 *
 * @note Locks mParent + this object for writing.
 */
void SessionMachine::discardCurrentStateHandler (DiscardCurrentStateTask &aTask)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);

    LogFlowThisFunc (("state=%d\n", autoCaller.state()));
    if (!autoCaller.isOk())
    {
        /* we might have been uninitialized because the session was accidentally
         * closed by the client, so don't assert */
        aTask.progress->notifyComplete (
            E_FAIL, COM_IIDOF (IMachine), getComponentName(),
            tr ("The session has been accidentally closed"));

        LogFlowThisFuncLeave();
        return;
    }

    /* saveSettings() needs mParent lock */
    AutoWriteLock vboxLock (mParent);

    /* @todo We don't need mParent lock so far so unlock() it. Better is to
     * provide an AutoWriteLock argument that lets create a non-locking
     * instance */
    vboxLock.unlock();

    AutoWriteLock alock (this);

    /* discard all current changes to mUserData (name, OSType etc.) (note that
     * the machine is powered off, so there is no need to inform the direct
     * session) */
    if (isModified())
        rollback (false /* aNotify */);

    HRESULT rc = S_OK;

    bool errorInSubtask = false;
    bool stateRestored = false;

    const bool isLastSnapshot = mData->mCurrentSnapshot->parent().isNull();

    try
    {
        /* discard the saved state file if the machine was Saved prior to this
         * operation */
        if (aTask.state == MachineState_Saved)
        {
            Assert (!mSSData->mStateFilePath.isEmpty());
            RTFileDelete (Utf8Str (mSSData->mStateFilePath));
            mSSData->mStateFilePath.setNull();
            aTask.modifyLastState (MachineState_PoweredOff);
            rc = saveStateSettings (SaveSTS_StateFilePath);
            CheckComRCThrowRC (rc);
        }

        if (aTask.discardCurrentSnapshot && !isLastSnapshot)
        {
            /* the "discard current snapshot and state" task is in action, the
             * current snapshot is not the last one. Discard the current
             * snapshot first */

            DiscardSnapshotTask subTask (aTask, mData->mCurrentSnapshot);
            subTask.subTask = true;
            discardSnapshotHandler (subTask);

            AutoCaller progressCaller (aTask.progress);
            AutoReadLock progressLock (aTask.progress);
            if (aTask.progress->completed())
            {
                /* the progress can be completed by a subtask only if there was
                 * a failure */
                rc = aTask.progress->resultCode();
                Assert (FAILED (rc));
                errorInSubtask = true;
                throw rc;
            }
        }

        RTTIMESPEC snapshotTimeStamp;
        RTTimeSpecSetMilli (&snapshotTimeStamp, 0);

        {
            ComObjPtr <Snapshot> curSnapshot = mData->mCurrentSnapshot;
            AutoReadLock snapshotLock (curSnapshot);

            /* remember the timestamp of the snapshot we're restoring from */
            snapshotTimeStamp = curSnapshot->data().mTimeStamp;

            /* copy all hardware data from the current snapshot */
            copyFrom (curSnapshot->data().mMachine);

            LogFlowThisFunc (("Restoring hard disks from the snapshot...\n"));

            /* restore the attachmends from the snapshot */
            mHDData.backup();
            mHDData->mAttachments =
                curSnapshot->data().mMachine->mHDData->mAttachments;

            /* leave the locks before the potentially lengthy operation */
            snapshotLock.unlock();
            alock.leave();

            rc = createImplicitDiffs (mUserData->mSnapshotFolderFull,
                                      aTask.progress,
                                      false /* aOnline */);

            alock.enter();
            snapshotLock.lock();

            CheckComRCThrowRC (rc);

            /* Note: on success, current (old) hard disks will be
             * deassociated/deleted on #commit() called from #saveSettings() at
             * the end. On failure, newly created implicit diffs will be
             * deleted by #rollback() at the end. */

            /* should not have a saved state file associated at this point */
            Assert (mSSData->mStateFilePath.isNull());

            if (curSnapshot->stateFilePath())
            {
                Utf8Str snapStateFilePath = curSnapshot->stateFilePath();

                Utf8Str stateFilePath = Utf8StrFmt ("%ls%c{%RTuuid}.sav",
                    mUserData->mSnapshotFolderFull.raw(),
                    RTPATH_DELIMITER, mData->mUuid.raw());

                LogFlowThisFunc (("Copying saved state file from '%s' to '%s'...\n",
                                  snapStateFilePath.raw(), stateFilePath.raw()));

                aTask.progress->setNextOperation(Bstr(tr("Restoring the execution state")),
                                                 1);        // weight

                /* leave the lock before the potentially lengthy operation */
                snapshotLock.unlock();
                alock.leave();

                /* copy the state file */
                int vrc = RTFileCopyEx (snapStateFilePath, stateFilePath,
                                        0, progressCallback, aTask.progress);

                alock.enter();
                snapshotLock.lock();

                if (RT_SUCCESS (vrc))
                {
                    mSSData->mStateFilePath = stateFilePath;
                }
                else
                {
                    throw setError (E_FAIL,
                        tr ("Could not copy the state file '%s' to '%s' (%Rrc)"),
                        snapStateFilePath.raw(), stateFilePath.raw(), vrc);
                }
            }
        }

        /* grab differencing hard disks from the old attachments that will
         * become unused and need to be auto-deleted */

        std::list< ComObjPtr<HardDisk> > diffs;

        for (HDData::AttachmentList::const_iterator
             it = mHDData.backedUpData()->mAttachments.begin();
             it != mHDData.backedUpData()->mAttachments.end(); ++ it)
        {
            ComObjPtr<HardDisk> hd = (*it)->hardDisk();

            /* while the hard disk is attached, the number of children or the
             * parent cannot change, so no lock */
            if (!hd->parent().isNull() && hd->children().size() == 0)
                diffs.push_back (hd);
        }

        int saveFlags = 0;

        if (aTask.discardCurrentSnapshot && isLastSnapshot)
        {
            /* commit changes to have unused diffs deassociated from this
             * machine before deletion (see below) */
            commit();

            /* delete the unused diffs now (and uninit them) because discard
             * may fail otherwise (too many children of the hard disk to be
             * discarded) */
            for (std::list< ComObjPtr<HardDisk> >::const_iterator
                 it = diffs.begin(); it != diffs.end(); ++ it)
            {
                /// @todo for now, we ignore errors since we've already
                /// and therefore cannot fail. Later, we may want to report a
                /// warning through the Progress object
                HRESULT rc2 = (*it)->deleteStorageAndWait();
                if (SUCCEEDED (rc2))
                    (*it)->uninit();
            }

            /* prevent further deletion */
            diffs.clear();

            /* discard the current snapshot and state task is in action, the
             * current snapshot is the last one. Discard the current snapshot
             * after discarding the current state. */

            DiscardSnapshotTask subTask (aTask, mData->mCurrentSnapshot);
            subTask.subTask = true;
            discardSnapshotHandler (subTask);

            AutoCaller progressCaller (aTask.progress);
            AutoReadLock progressLock (aTask.progress);
            if (aTask.progress->completed())
            {
                /* the progress can be completed by a subtask only if there
                 * was a failure */
                rc = aTask.progress->resultCode();
                Assert (FAILED (rc));
                errorInSubtask = true;
            }

            /* we've committed already, so inform callbacks anyway to ensure
             * they don't miss some change */
            /// @todo NEWMEDIA check if we need this informCallbacks at all
            /// after updating discardCurrentSnapshot functionality
            saveFlags |= SaveS_InformCallbacksAnyway;
        }

        /* @todo saveSettings() below needs a VirtualBox write lock and we need
         * to leave this object's lock to do this to follow the {parent-child}
         * locking rule. This is the last chance to do that while we are still
         * in a protective state which allows us to temporarily leave the lock*/
        alock.unlock();
        vboxLock.lock();
        alock.lock();

        /* we have already discarded the current state, so set the execution
         * state accordingly no matter of the discard snapshot result */
        if (mSSData->mStateFilePath)
            setMachineState (MachineState_Saved);
        else
            setMachineState (MachineState_PoweredOff);

        updateMachineStateOnClient();
        stateRestored = true;

        /* assign the timestamp from the snapshot */
        Assert (RTTimeSpecGetMilli (&snapshotTimeStamp) != 0);
        mData->mLastStateChange = snapshotTimeStamp;

        /* save all settings, reset the modified flag and commit. Note that we
         * do so even if the subtask failed (errorInSubtask=true) because we've
         * already committed machine data and deleted old diffs before
         * discarding the current snapshot so there is no way to rollback */
        HRESULT rc2 = saveSettings (SaveS_ResetCurStateModified | saveFlags);

        /// @todo NEWMEDIA return multiple errors
        if (errorInSubtask)
            throw rc;

        rc = rc2;

        if (SUCCEEDED (rc))
        {
            /* now, delete the unused diffs (only on success!) and uninit them*/
            for (std::list< ComObjPtr<HardDisk> >::const_iterator
                 it = diffs.begin(); it != diffs.end(); ++ it)
            {
                /// @todo for now, we ignore errors since we've already
                /// discarded and therefore cannot fail. Later, we may want to
                /// report a warning through the Progress object
                HRESULT rc2 = (*it)->deleteStorageAndWait();
                if (SUCCEEDED (rc2))
                    (*it)->uninit();
            }
        }
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (FAILED (rc))
    {
        /* preserve existing error info */
        ErrorInfoKeeper eik;

        if (!errorInSubtask)
        {
            /* undo all changes on failure unless the subtask has done so */
            rollback (false /* aNotify */);
        }

        if (!stateRestored)
        {
            /* restore the machine state */
            setMachineState (aTask.state);
            updateMachineStateOnClient();
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
 * Locks the attached media.
 *
 * All attached hard disks and DVD/floppy are locked for writing. Parents of
 * attached hard disks (if any) are locked for reading.
 *
 * This method also performs accessibility check of all media it locks: if some
 * media is inaccessible, the method will return a failure and a bunch of
 * extended error info objects per each inaccessible medium.
 *
 * Note that this method is atomic: if it returns a success, all media are
 * locked as described above; on failure no media is locked at all (all
 * succeeded individual locks will be undone).
 *
 * This method is intended to be called when the machine is in Starting or
 * Restoring state and asserts otherwise.
 *
 * The locks made by this method must be undone by calling #unlockMedia() when
 * no more needed.
 */
HRESULT SessionMachine::lockMedia()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    AssertReturn (mData->mMachineState == MachineState_Starting ||
                  mData->mMachineState == MachineState_Restoring, E_FAIL);

    typedef std::list <ComPtr <IMedium> > MediaList;
    MediaList mediaToCheck;
    MediaState_T mediaState;

    try
    {
        HRESULT rc = S_OK;

        /* lock hard disks */
        for (HDData::AttachmentList::const_iterator it =
                 mHDData->mAttachments.begin();
             it != mHDData->mAttachments.end(); ++ it)
        {
            ComObjPtr<HardDisk> hd = (*it)->hardDisk();

            bool first = true;

            /** @todo split out the media locking, and put it into
             * HardDiskImpl.cpp, as it needs this functionality too. */
            while (!hd.isNull())
            {
                if (first)
                {
                    rc = hd->LockWrite (&mediaState);
                    CheckComRCThrowRC (rc);

                    mData->mSession.mLockedMedia.push_back (
                        Data::Session::LockedMedia::value_type (
                            ComPtr <IHardDisk> (hd), true));

                    first = false;
                }
                else
                {
                    rc = hd->LockRead (&mediaState);
                    CheckComRCThrowRC (rc);

                    mData->mSession.mLockedMedia.push_back (
                        Data::Session::LockedMedia::value_type (
                            ComPtr <IHardDisk> (hd), false));
                }

                if (mediaState == MediaState_Inaccessible)
                    mediaToCheck.push_back (ComPtr <IHardDisk> (hd));

                /* no locks or callers here since there should be no way to
                 * change the hard disk parent at this point (as it is still
                 * attached to the machine) */
                hd = hd->parent();
            }
        }

        /* lock the DVD image for reading if mounted */
        {
            AutoReadLock driveLock (mDVDDrive);
            if (mDVDDrive->data()->state == DriveState_ImageMounted)
            {
                ComObjPtr <DVDImage> image = mDVDDrive->data()->image;

                rc = image->LockRead (&mediaState);
                CheckComRCThrowRC (rc);

                mData->mSession.mLockedMedia.push_back (
                    Data::Session::LockedMedia::value_type (
                        ComPtr <IDVDImage> (image), false));

                if (mediaState == MediaState_Inaccessible)
                    mediaToCheck.push_back (ComPtr <IDVDImage> (image));
            }
        }

        /* lock the floppy image for reading if mounted */
        {
            AutoReadLock driveLock (mFloppyDrive);
            if (mFloppyDrive->data()->state == DriveState_ImageMounted)
            {
                ComObjPtr <FloppyImage> image = mFloppyDrive->data()->image;

                rc = image->LockRead (&mediaState);
                CheckComRCThrowRC (rc);

                mData->mSession.mLockedMedia.push_back (
                    Data::Session::LockedMedia::value_type (
                        ComPtr <IFloppyImage> (image), false));

                if (mediaState == MediaState_Inaccessible)
                    mediaToCheck.push_back (ComPtr <IFloppyImage> (image));
            }
        }

        /* SUCCEEDED locking all media, now check accessibility */

        ErrorInfoKeeper eik (true /* aIsNull */);
        MultiResult mrc (S_OK);

        /* perform a check of inaccessible media deferred above */
        for (MediaList::const_iterator
             it = mediaToCheck.begin();
             it != mediaToCheck.end(); ++ it)
        {
            MediaState_T mediaState;
            rc = (*it)->COMGETTER(State) (&mediaState);
            CheckComRCThrowRC (rc);

            Assert (mediaState == MediaState_LockedRead ||
                    mediaState == MediaState_LockedWrite);

            /* Note that we locked the medium already, so use the error
             * value to see if there was an accessibility failure */

            Bstr error;
            rc = (*it)->COMGETTER(LastAccessError) (error.asOutParam());
            CheckComRCThrowRC (rc);

            if (!error.isNull())
            {
                Bstr loc;
                rc = (*it)->COMGETTER(Location) (loc.asOutParam());
                CheckComRCThrowRC (rc);

                /* collect multiple errors */
                eik.restore();

                /* be in sync with MediumBase::setStateError() */
                Assert (!error.isEmpty());
                mrc = setError (E_FAIL,
                    tr ("Medium '%ls' is not accessible. %ls"),
                    loc.raw(), error.raw());

                eik.fetch();
            }
        }

        eik.restore();
        CheckComRCThrowRC ((HRESULT) mrc);
    }
    catch (HRESULT aRC)
    {
        /* Unlock all locked media on failure */
        unlockMedia();
        return aRC;
    }

    return S_OK;
}

/**
 * Undoes the locks made by by #lockMedia().
 */
void SessionMachine::unlockMedia()
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock (this);

    /* we may be holding important error info on the current thread;
     * preserve it */
    ErrorInfoKeeper eik;

    HRESULT rc = S_OK;

    for (Data::Session::LockedMedia::const_iterator
         it = mData->mSession.mLockedMedia.begin();
         it != mData->mSession.mLockedMedia.end(); ++ it)
    {
        MediaState_T state;
        if (it->second)
            rc = it->first->UnlockWrite (&state);
        else
            rc = it->first->UnlockRead (&state);

        /* the latter can happen if an object was re-locked in
         * Machine::fixupHardDisks() */
        Assert (SUCCEEDED (rc) || state == MediaState_LockedRead);
    }

    mData->mSession.mLockedMedia.clear();
}

/**
 * Helper to change the machine state (reimplementation).
 *
 * @note Locks this object for writing.
 */
HRESULT SessionMachine::setMachineState (MachineState_T aMachineState)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aMachineState=%d\n", aMachineState));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    MachineState_T oldMachineState = mData->mMachineState;

    AssertMsgReturn (oldMachineState != aMachineState,
                     ("oldMachineState=%d, aMachineState=%d\n",
                      oldMachineState, aMachineState), E_FAIL);

    HRESULT rc = S_OK;

    int stsFlags = 0;
    bool deleteSavedState = false;

    /* detect some state transitions */

    if ((oldMachineState == MachineState_Saved &&
           aMachineState == MachineState_Restoring) ||
        (oldMachineState < MachineState_Running /* any other OFF state */ &&
           aMachineState == MachineState_Starting))
    {
        /* The EMT thread is about to start */

        /* Nothing to do here for now... */

        /// @todo NEWMEDIA don't let mDVDDrive and other children
        /// change anything when in the Starting/Restoring state
    }
    else
    if (oldMachineState >= MachineState_Running &&
        oldMachineState != MachineState_Discarding &&
        oldMachineState != MachineState_SettingUp &&
        aMachineState < MachineState_Running &&
        /* ignore PoweredOff->Saving->PoweredOff transition when taking a
         * snapshot */
        (mSnapshotData.mSnapshot.isNull() ||
         mSnapshotData.mLastState >= MachineState_Running))
    {
        /* The EMT thread has just stopped, unlock attached media. Note that as
         * opposed to locking that is done from Console, we do unlocking here
         * because the VM process may have aborted before having a chance to
         * properly unlock all media it locked. */

        unlockMedia();
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
        /* set the current state modified flag to indicate that the current
         * state is no more identical to the state in the
         * current snapshot */
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
        /* we've been shut down for any reason */
        /* no special action so far */
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
        AutoReadLock alock (this);
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
DECLCALLBACK(int) SessionMachine::taskHandler (RTTHREAD /* thread */, void *pvUser)
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
                               IN_GUID aSnapshotId,
                               IN_BSTR aStateFilePath)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mName={%ls}\n", aSessionMachine->mUserData->mName.raw()));

    AssertReturn (aSessionMachine && !Guid (aSnapshotId).isEmpty(), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    AssertReturn (aSessionMachine->isWriteLockOnCurrentThread(), E_FAIL);

    mSnapshotId = aSnapshotId;

    /* memorize the primary Machine instance (i.e. not SessionMachine!) */
    unconst (mPeer) = aSessionMachine->mPeer;
    /* share the parent pointer */
    unconst (mParent) = mPeer->mParent;

    /* take the pointer to Data to share */
    mData.share (mPeer->mData);

    /* take the pointer to UserData to share (our UserData must always be the
     * same as Machine's data) */
    mUserData.share (mPeer->mUserData);
    /* make a private copy of all other data (recent changes from SessionMachine) */
    mHWData.attachCopy (aSessionMachine->mHWData);
    mHDData.attachCopy (aSessionMachine->mHDData);

    /* SSData is always unique for SnapshotMachine */
    mSSData.allocate();
    mSSData->mStateFilePath = aStateFilePath;

    HRESULT rc = S_OK;

    /* create copies of all shared folders (mHWData after attiching a copy
     * contains just references to original objects) */
    for (HWData::SharedFolderList::iterator
         it = mHWData->mSharedFolders.begin();
         it != mHWData->mSharedFolders.end();
         ++ it)
    {
        ComObjPtr <SharedFolder> folder;
        folder.createObject();
        rc = folder->initCopy (this, *it);
        CheckComRCReturnRC (rc);
        *it = folder;
    }

    /* associate hard disks with the snapshot
     * (Machine::uninitDataAndChildObjects() will deassociate at destruction) */
    for (HDData::AttachmentList::const_iterator
         it = mHDData->mAttachments.begin();
         it != mHDData->mAttachments.end();
         ++ it)
    {
        rc = (*it)->hardDisk()->attachTo (mData->mUuid, mSnapshotId);
        AssertComRC (rc);
    }

    /* create copies of all storage controllers (mStorageControllerData
     * after attaching a copy contains just references to original objects) */
    mStorageControllers.allocate();
    for (StorageControllerList::const_iterator
         it = aSessionMachine->mStorageControllers->begin();
         it != aSessionMachine->mStorageControllers->end();
         ++ it)
    {
        ComObjPtr <StorageController> ctrl;
        ctrl.createObject();
        ctrl->initCopy (this, *it);
        mStorageControllers->push_back(ctrl);
    }

    /* create all other child objects that will be immutable private copies */

    unconst (mBIOSSettings).createObject();
    mBIOSSettings->initCopy (this, mPeer->mBIOSSettings);

#ifdef VBOX_WITH_VRDP
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

    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
    {
        unconst (mNetworkAdapters [slot]).createObject();
        mNetworkAdapters [slot]->initCopy (this, mPeer->mNetworkAdapters [slot]);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
    {
        unconst (mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->initCopy (this, mPeer->mSerialPorts [slot]);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
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
                               IN_GUID aSnapshotId, IN_BSTR aStateFilePath)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("mName={%ls}\n", aMachine->mUserData->mName.raw()));

    AssertReturn (aMachine && !aHWNode.isNull() && !aHDAsNode.isNull() &&
                  !Guid (aSnapshotId).isEmpty(),
                  E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

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
    mStorageControllers.allocate();

    /* SSData is always unique for SnapshotMachine */
    mSSData.allocate();
    mSSData->mStateFilePath = aStateFilePath;

    /* create all other child objects that will be immutable private copies */

    unconst (mBIOSSettings).createObject();
    mBIOSSettings->init (this);

#ifdef VBOX_WITH_VRDP
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

    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
    {
        unconst (mNetworkAdapters [slot]).createObject();
        mNetworkAdapters [slot]->init (this, slot);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
    {
        unconst (mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->init (this, slot);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
    {
        unconst (mParallelPorts [slot]).createObject();
        mParallelPorts [slot]->init (this, slot);
    }

    /* load hardware and harddisk settings */

    HRESULT rc = loadHardware (aHWNode);
    if (SUCCEEDED (rc))
        rc = loadStorageControllers (aHDAsNode, true /* aRegistered */, &mSnapshotId);

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

// util::Lockable interface
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
    AutoWriteLock alock (this);

    mPeer->saveSnapshotSettings (aSnapshot, SaveSS_UpdateAttrsOp);

    /* inform callbacks */
    mParent->onSnapshotChange (mData->mUuid, aSnapshot->data().mId);

    return S_OK;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
