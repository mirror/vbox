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
#include "MediumAttachmentImpl.h"
#include "MediumImpl.h"
#include "USBControllerImpl.h"
#include "HostImpl.h"
#include "SharedFolderImpl.h"
#include "GuestOSTypeImpl.h"
#include "VirtualBoxErrorInfoImpl.h"
#include "GuestImpl.h"
#include "StorageControllerImpl.h"

#ifdef VBOX_WITH_USB
# include "USBProxyService.h"
#endif

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
 *  Progress callback handler for lengthy operations
 *  (corresponds to the FNRTPROGRESS typedef).
 *
 *  @param uPercentage  Completetion precentage (0-100).
 *  @param pvUser       Pointer to the Progress instance.
 */
static DECLCALLBACK(int) progressCallback(unsigned uPercentage, void *pvUser)
{
    Progress *progress = static_cast<Progress*>(pvUser);

    /* update the progress object */
    if (progress)
        progress->SetCurrentOperationProgress(uPercentage);

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
    RTTimeNow(&mLastStateChange);

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
        RTSemEventMultiDestroy(mMachineStateDepsSem);
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
    mTeleporterEnabled = FALSE;
    mTeleporterPort = 0;

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
    mAccelerate2DVideoEnabled = false;
    mMonitorCount = 1;
    mHWVirtExEnabled = true;
    mHWVirtExNestedPagingEnabled = false;
    mHWVirtExVPIDEnabled = false;
    mHWVirtExExclusive = true;
    mPAEEnabled = false;
    mSyntheticCpu = false;
    mPropertyServiceActive = false;

    /* default boot order: floppy - DVD - HDD */
    mBootOrder [0] = DeviceType_Floppy;
    mBootOrder [1] = DeviceType_DVD;
    mBootOrder [2] = DeviceType_HardDisk;
    for (size_t i = 3; i < RT_ELEMENTS (mBootOrder); ++i)
        mBootOrder [i] = DeviceType_Null;

    mClipboardMode = ClipboardMode_Bidirectional;
    mGuestPropertyNotificationPatterns = "";

    mFirmwareType = FirmwareType_BIOS;
}

Machine::HWData::~HWData()
{
}

bool Machine::HWData::operator==(const HWData &that) const
{
    if (this == &that)
        return true;

    if (mHWVersion != that.mHWVersion ||
        mHardwareUUID != that.mHardwareUUID ||
        mMemorySize != that.mMemorySize ||
        mMemoryBalloonSize != that.mMemoryBalloonSize ||
        mStatisticsUpdateInterval != that.mStatisticsUpdateInterval ||
        mVRAMSize != that.mVRAMSize ||
        mFirmwareType != that.mFirmwareType ||
        mAccelerate3DEnabled != that.mAccelerate3DEnabled ||
        mAccelerate2DVideoEnabled != that.mAccelerate2DVideoEnabled ||
        mMonitorCount != that.mMonitorCount ||
        mHWVirtExEnabled != that.mHWVirtExEnabled ||
        mHWVirtExNestedPagingEnabled != that.mHWVirtExNestedPagingEnabled ||
        mHWVirtExVPIDEnabled != that.mHWVirtExVPIDEnabled ||
        mHWVirtExExclusive != that.mHWVirtExExclusive ||
        mPAEEnabled != that.mPAEEnabled ||
        mSyntheticCpu != that.mSyntheticCpu ||
        mCPUCount != that.mCPUCount ||
        mClipboardMode != that.mClipboardMode)
        return false;

    for (size_t i = 0; i < RT_ELEMENTS (mBootOrder); ++i)
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
            if (    (*it)->name() == (*thatIt)->name()
                 && RTPathCompare(Utf8Str((*it)->hostPath()).c_str(),
                                  Utf8Str((*thatIt)->hostPath()).c_str()
                                 ) == 0)
            {
                thatFolders.erase (thatIt);
                found = true;
                break;
            }
            else
                ++thatIt;
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

Machine::MediaData::MediaData()
{
}

Machine::MediaData::~MediaData()
{
}

bool Machine::MediaData::operator== (const MediaData &that) const
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
            if ((*it)->matches((*thatIt)->controllerName(),
                               (*thatIt)->port(),
                               (*thatIt)->device()) &&
                (*it)->passthrough() == (*thatIt)->passthrough() &&
                (*it)->medium().equalsTo ((*thatIt)->medium()))
            {
                thatAtts.erase (thatIt);
                found = true;
                break;
            }
            else
                ++thatIt;
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
    LogFlowThisFunc(("\n"));
    return S_OK;
}

void Machine::FinalRelease()
{
    LogFlowThisFunc(("\n"));
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
HRESULT Machine::init(VirtualBox *aParent,
                      const Utf8Str &strConfigFile,
                      InitMode aMode,
                      CBSTR aName /* = NULL */,
                      GuestOSType *aOsType /* = NULL */,
                      BOOL aNameSync /* = TRUE */,
                      const Guid *aId /* = NULL */)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aConfigFile='%s', aMode=%d\n", strConfigFile.raw(), aMode));

    AssertReturn (aParent, E_INVALIDARG);
    AssertReturn (!strConfigFile.isEmpty(), E_INVALIDARG);
    AssertReturn(aMode != Init_New || (aName != NULL && *aName != '\0'),
                  E_INVALIDARG);
    AssertReturn(aMode != Init_Registered || aId != NULL, E_FAIL);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    /* share the parent weakly */
    unconst(mParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    /* allocate the essential machine data structure (the rest will be
     * allocated later by initDataAndChildObjects() */
    mData.allocate();

    mData->m_pMachineConfigFile = NULL;

    /* memorize the config file name (as provided) */
    mData->m_strConfigFile = strConfigFile;

    /* get the full file name */
    int vrc = mParent->calculateFullPath(strConfigFile, mData->m_strConfigFileFull);
    if (RT_FAILURE(vrc))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Invalid machine settings file name '%s' (%Rrc)"),
                        strConfigFile.raw(),
                        vrc);

    if (aMode == Init_Registered)
    {
        mData->mRegistered = TRUE;

        /* store the supplied UUID (will be used to check for UUID consistency
         * in loadSettings() */
        unconst(mData->mUuid) = *aId;

        // now load the settings from XML:
        rc = registeredInit();
    }
    else
    {
        if (aMode == Init_Import)
        {
            // we're reading the settings file below
        }
        else if (aMode == Init_New)
        {
            /* check for the file existence */
            RTFILE f = NIL_RTFILE;
            int vrc = RTFileOpen(&f, mData->m_strConfigFileFull.c_str(), RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
            if (    RT_SUCCESS(vrc)
                 || vrc == VERR_SHARING_VIOLATION
               )
            {
                rc = setError(VBOX_E_FILE_ERROR,
                              tr("Machine settings file '%s' already exists"),
                              mData->m_strConfigFileFull.raw());
                if (RT_SUCCESS(vrc))
                    RTFileClose(f);
            }
            else
            {
                if (     vrc != VERR_FILE_NOT_FOUND
                      && vrc != VERR_PATH_NOT_FOUND
                   )
                    rc = setError(VBOX_E_FILE_ERROR,
                                  tr("Invalid machine settings file name '%s' (%Rrc)"),
                                  mData->m_strConfigFileFull.raw(),
                                  vrc);
            }

            // create an empty machine config
            mData->m_pMachineConfigFile = new settings::MachineConfigFile(NULL);
        }
        else
            AssertFailed();

        if (SUCCEEDED(rc))
            rc = initDataAndChildObjects();

        if (SUCCEEDED(rc))
        {
            /* set to true now to cause uninit() to call
             * uninitDataAndChildObjects() on failure */
            mData->mAccessible = TRUE;

            if (aMode != Init_New)
            {
                rc = loadSettings(false /* aRegistered */);
            }
            else
            {
                /* create the machine UUID */
                if (aId)
                    unconst(mData->mUuid) = *aId;
                else
                    unconst(mData->mUuid).create();

                /* memorize the provided new machine's name */
                mUserData->mName = aName;
                mUserData->mNameSync = aNameSync;

                /* initialize the default snapshots folder
                 * (note: depends on the name value set above!) */
                rc = COMSETTER(SnapshotFolder)(NULL);
                AssertComRC(rc);

                if (aOsType)
                {
                    /* Store OS type */
                    mUserData->mOSTypeId = aOsType->id();

                    /* Apply BIOS defaults */
                    mBIOSSettings->applyDefaults (aOsType);

                    /* Apply network adapters defaults */
                    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); ++slot)
                        mNetworkAdapters [slot]->applyDefaults (aOsType);

                    /* Apply serial port defaults */
                    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); ++slot)
                        mSerialPorts [slot]->applyDefaults (aOsType);
                }
            }

            /* commit all changes made during the initialization */
            if (SUCCEEDED(rc))
                commit();
        }
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
    {
        if (mData->mAccessible)
            autoInitSpan.setSucceeded();
        else
            autoInitSpan.setLimited();
    }

    LogFlowThisFunc(("mName='%ls', mRegistered=%RTbool, mAccessible=%RTbool "
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
    AssertReturn(mType == IsMachine, E_FAIL);
    AssertReturn(!mData->mUuid.isEmpty(), E_FAIL);
    AssertReturn(!mData->mAccessible, E_FAIL);

    HRESULT rc = initDataAndChildObjects();

    if (SUCCEEDED(rc))
    {
        /* Temporarily reset the registered flag in order to let setters
         * potentially called from loadSettings() succeed (isMutable() used in
         * all setters will return FALSE for a Machine instance if mRegistered
         * is TRUE). */
        mData->mRegistered = FALSE;

        rc = loadSettings(true /* aRegistered */);

        /* Restore the registered flag (even on failure) */
        mData->mRegistered = TRUE;
    }

    if (SUCCEEDED(rc))
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
        LogWarning(("Machine {%RTuuid} is inaccessible! [%ls]\n",
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
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    Assert (mType == IsMachine);
    Assert (!!mData);

    LogFlowThisFunc(("initFailed()=%d\n", autoUninitSpan.initFailed()));
    LogFlowThisFunc(("mRegistered=%d\n", mData->mRegistered));

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
        LogWarningThisFunc(("Session machine is not NULL (%p), "
                             "the direct session is still open!\n",
                             (SessionMachine *) mData->mSession.mMachine));

        if (Global::IsOnlineOrTransient (mData->mMachineState))
        {
            LogWarningThisFunc(("Setting state to Aborted!\n"));
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

    if (isModified())
    {
        LogWarningThisFunc(("Discarding unsaved settings changes!\n"));
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
    CheckComArgOutPointerValid(aParent);

    AutoLimitedCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mParent is constant during life time, no need to lock */
    mParent.queryInterfaceTo(aParent);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Accessible) (BOOL *aAccessible)
{
    CheckComArgOutPointerValid(aAccessible);

    AutoLimitedCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = S_OK;

    if (!mData->mAccessible)
    {
        /* try to initialize the VM once more if not accessible */

        AutoReinitSpan autoReinitSpan(this);
        AssertReturn(autoReinitSpan.isOk(), E_FAIL);

        if (mData->m_pMachineConfigFile)
        {
            // @todo why are we parsing this several times?
            // this is hugely inefficient
            delete mData->m_pMachineConfigFile;
            mData->m_pMachineConfigFile = NULL;
        }

        rc = registeredInit();

        if (SUCCEEDED(rc) && mData->mAccessible)
        {
            autoReinitSpan.setSucceeded();

            /* make sure interesting parties will notice the accessibility
             * state change */
            mParent->onMachineStateChange(mData->mUuid, mData->mMachineState);
            mParent->onMachineDataChange(mData->mUuid);
        }
    }

    if (SUCCEEDED(rc))
        *aAccessible = mData->mAccessible;

    return rc;
}

STDMETHODIMP Machine::COMGETTER(AccessError) (IVirtualBoxErrorInfo **aAccessError)
{
    CheckComArgOutPointerValid(aAccessError);

    AutoLimitedCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    if (mData->mAccessible || !mData->mAccessError.isBasicAvailable())
    {
        /* return shortly */
        aAccessError = NULL;
        return S_OK;
    }

    HRESULT rc = S_OK;

    ComObjPtr<VirtualBoxErrorInfo> errorInfo;
    rc = errorInfo.createObject();
    if (SUCCEEDED(rc))
    {
        errorInfo->init (mData->mAccessError.getResultCode(),
                         mData->mAccessError.getInterfaceID(),
                         mData->mAccessError.getComponent(),
                         mData->mAccessError.getText());
        rc = errorInfo.queryInterfaceTo(aAccessError);
    }

    return rc;
}

STDMETHODIMP Machine::COMGETTER(Name) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mUserData->mName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Name) (IN_BSTR aName)
{
    CheckComArgNotNull (aName);

    if (!*aName)
        return setError(E_INVALIDARG,
                        tr("Machine name cannot be empty"));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mUserData.backup();
    mUserData->mName = aName;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Description) (BSTR *aDescription)
{
    CheckComArgOutPointerValid(aDescription);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mUserData->mDescription.cloneTo(aDescription);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Description) (IN_BSTR aDescription)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mUserData.backup();
    mUserData->mDescription = aDescription;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Id) (BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoLimitedCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mData->mUuid.toUtf16().cloneTo(aId);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(OSTypeId) (BSTR *aOSTypeId)
{
    CheckComArgOutPointerValid(aOSTypeId);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mUserData->mOSTypeId.cloneTo(aOSTypeId);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(OSTypeId) (IN_BSTR aOSTypeId)
{
    CheckComArgNotNull (aOSTypeId);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* look up the object by Id to check it is valid */
    ComPtr<IGuestOSType> guestOSType;
    HRESULT rc = mParent->GetGuestOSType (aOSTypeId,
                                          guestOSType.asOutParam());
    CheckComRCReturnRC(rc);

    /* when setting, always use the "etalon" value for consistency -- lookup
     * by ID is case-insensitive and the input value may have different case */
    Bstr osTypeId;
    rc = guestOSType->COMGETTER(Id) (osTypeId.asOutParam());
    CheckComRCReturnRC(rc);

    AutoWriteLock alock(this);

    rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mUserData.backup();
    mUserData->mOSTypeId = osTypeId;

    return S_OK;
}


STDMETHODIMP Machine::COMGETTER(FirmwareType) (FirmwareType_T *aFirmwareType)
{
    CheckComArgOutPointerValid(aFirmwareType);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aFirmwareType = mHWData->mFirmwareType;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(FirmwareType) (FirmwareType_T aFirmwareType)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());
    AutoWriteLock alock(this);

    int rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mFirmwareType = aFirmwareType;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(HardwareVersion) (BSTR *aHWVersion)
{
    if (!aHWVersion)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mHWData->mHWVersion.cloneTo(aHWVersion);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(HardwareVersion) (IN_BSTR aHWVersion)
{
    /* check known version */
    Utf8Str hwVersion = aHWVersion;
    if (    hwVersion.compare ("1") != 0
        &&  hwVersion.compare ("2") != 0)
        return setError(E_INVALIDARG,
                        tr("Invalid hardware version: %ls\n"), aHWVersion);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mHWVersion = hwVersion;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(HardwareUUID)(BSTR *aUUID)
{
    CheckComArgOutPointerValid(aUUID);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    if (!mHWData->mHardwareUUID.isEmpty())
        mHWData->mHardwareUUID.toUtf16().cloneTo(aUUID);
    else
        mData->mUuid.toUtf16().cloneTo(aUUID);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(HardwareUUID) (IN_BSTR aUUID)
{
    Guid hardwareUUID(aUUID);
    if (hardwareUUID.isEmpty())
        return E_INVALIDARG;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    if (hardwareUUID == mData->mUuid)
        mHWData->mHardwareUUID.clear();
    else
        mHWData->mHardwareUUID = hardwareUUID;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(MemorySize) (ULONG *memorySize)
{
    if (!memorySize)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *memorySize = mHWData->mMemorySize;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(MemorySize) (ULONG memorySize)
{
    /* check RAM limits */
    if (    memorySize < MM_RAM_MIN_IN_MB
         || memorySize > MM_RAM_MAX_IN_MB
       )
        return setError(E_INVALIDARG,
                        tr("Invalid RAM size: %lu MB (must be in range [%lu, %lu] MB)"),
                        memorySize, MM_RAM_MIN_IN_MB, MM_RAM_MAX_IN_MB);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mMemorySize = memorySize;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CPUCount) (ULONG *CPUCount)
{
    if (!CPUCount)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *CPUCount = mHWData->mCPUCount;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(CPUCount) (ULONG CPUCount)
{
    /* check RAM limits */
    if (    CPUCount < SchemaDefs::MinCPUCount
         || CPUCount > SchemaDefs::MaxCPUCount
       )
        return setError(E_INVALIDARG,
                        tr("Invalid virtual CPU count: %lu (must be in range [%lu, %lu])"),
                        CPUCount, SchemaDefs::MinCPUCount, SchemaDefs::MaxCPUCount);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mCPUCount = CPUCount;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(VRAMSize) (ULONG *memorySize)
{
    if (!memorySize)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *memorySize = mHWData->mVRAMSize;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(VRAMSize) (ULONG memorySize)
{
    /* check VRAM limits */
    if (memorySize < SchemaDefs::MinGuestVRAM ||
        memorySize > SchemaDefs::MaxGuestVRAM)
        return setError(E_INVALIDARG,
                        tr("Invalid VRAM size: %lu MB (must be in range [%lu, %lu] MB)"),
                        memorySize, SchemaDefs::MinGuestVRAM, SchemaDefs::MaxGuestVRAM);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mVRAMSize = memorySize;

    return S_OK;
}

/** @todo this method should not be public */
STDMETHODIMP Machine::COMGETTER(MemoryBalloonSize) (ULONG *memoryBalloonSize)
{
    if (!memoryBalloonSize)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *memoryBalloonSize = mHWData->mMemoryBalloonSize;

    return S_OK;
}

/** @todo this method should not be public */
STDMETHODIMP Machine::COMSETTER(MemoryBalloonSize) (ULONG memoryBalloonSize)
{
    /* check limits */
    if (memoryBalloonSize >= VMMDEV_MAX_MEMORY_BALLOON (mHWData->mMemorySize))
        return setError(E_INVALIDARG,
                        tr("Invalid memory balloon size: %lu MB (must be in range [%lu, %lu] MB)"),
                        memoryBalloonSize, 0, VMMDEV_MAX_MEMORY_BALLOON (mHWData->mMemorySize));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mMemoryBalloonSize = memoryBalloonSize;

    return S_OK;
}

/** @todo this method should not be public */
STDMETHODIMP Machine::COMGETTER(StatisticsUpdateInterval) (ULONG *statisticsUpdateInterval)
{
    if (!statisticsUpdateInterval)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *statisticsUpdateInterval = mHWData->mStatisticsUpdateInterval;

    return S_OK;
}

/** @todo this method should not be public */
STDMETHODIMP Machine::COMSETTER(StatisticsUpdateInterval) (ULONG statisticsUpdateInterval)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mStatisticsUpdateInterval = statisticsUpdateInterval;

    return S_OK;
}


STDMETHODIMP Machine::COMGETTER(Accelerate3DEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *enabled = mHWData->mAccelerate3DEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Accelerate3DEnabled)(BOOL enable)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    /** @todo check validity! */

    mHWData.backup();
    mHWData->mAccelerate3DEnabled = enable;

    return S_OK;
}


STDMETHODIMP Machine::COMGETTER(Accelerate2DVideoEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *enabled = mHWData->mAccelerate2DVideoEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Accelerate2DVideoEnabled)(BOOL enable)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    /** @todo check validity! */

    mHWData.backup();
    mHWData->mAccelerate2DVideoEnabled = enable;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(MonitorCount) (ULONG *monitorCount)
{
    if (!monitorCount)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *monitorCount = mHWData->mMonitorCount;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(MonitorCount) (ULONG monitorCount)
{
    /* make sure monitor count is a sensible number */
    if (monitorCount < 1 || monitorCount > SchemaDefs::MaxGuestMonitors)
        return setError(E_INVALIDARG,
                        tr("Invalid monitor count: %lu (must be in range [%lu, %lu])"),
                        monitorCount, 1, SchemaDefs::MaxGuestMonitors);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mMonitorCount = monitorCount;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(BIOSSettings)(IBIOSSettings **biosSettings)
{
    if (!biosSettings)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* mBIOSSettings is constant during life time, no need to lock */
    mBIOSSettings.queryInterfaceTo(biosSettings);

    return S_OK;
}

STDMETHODIMP Machine::GetCpuProperty(CpuPropertyType_T property, BOOL *aVal)
{
    if (!aVal)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    switch(property)
    {
    case CpuPropertyType_PAE:
        *aVal = mHWData->mPAEEnabled;
        break;

    case CpuPropertyType_Synthetic:
        *aVal = mHWData->mSyntheticCpu;
        break;

    default:
        return E_INVALIDARG;
    }
    return S_OK;
}

STDMETHODIMP Machine::SetCpuProperty(CpuPropertyType_T property, BOOL aVal)
{
    if (!aVal)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    switch(property)
    {
    case CpuPropertyType_PAE:
        mHWData->mPAEEnabled = !!aVal;
        break;

    case CpuPropertyType_Synthetic:
        mHWData->mSyntheticCpu = !!aVal;
        break;

    default:
        return E_INVALIDARG;
    }
    return S_OK;
}

STDMETHODIMP Machine::GetHWVirtExProperty(HWVirtExPropertyType_T property, BOOL *aVal)
{
    if (!aVal)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    switch(property)
    {
    case HWVirtExPropertyType_Enabled:
        *aVal = mHWData->mHWVirtExEnabled;
        break;

    case HWVirtExPropertyType_Exclusive:
        *aVal = mHWData->mHWVirtExExclusive;
        break;

    case HWVirtExPropertyType_VPID:
        *aVal = mHWData->mHWVirtExVPIDEnabled;
        break;

    case HWVirtExPropertyType_NestedPaging:
        *aVal = mHWData->mHWVirtExNestedPagingEnabled;
        break;

    default:
        return E_INVALIDARG;
    }
    return S_OK;
}

STDMETHODIMP Machine::SetHWVirtExProperty(HWVirtExPropertyType_T property, BOOL aVal)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    switch(property)
    {
    case HWVirtExPropertyType_Enabled:
        mHWData.backup();
        mHWData->mHWVirtExEnabled = !!aVal;
        break;

    case HWVirtExPropertyType_Exclusive:
        mHWData.backup();
        mHWData->mHWVirtExExclusive = !!aVal;
        break;

    case HWVirtExPropertyType_VPID:
        mHWData.backup();
        mHWData->mHWVirtExVPIDEnabled = !!aVal;
        break;

    case HWVirtExPropertyType_NestedPaging:
        mHWData.backup();
        mHWData->mHWVirtExNestedPagingEnabled = !!aVal;
        break;

    default:
        return E_INVALIDARG;
    }
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SnapshotFolder) (BSTR *aSnapshotFolder)
{
    CheckComArgOutPointerValid(aSnapshotFolder);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mUserData->mSnapshotFolderFull.cloneTo(aSnapshotFolder);

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

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    if (!mData->mCurrentSnapshot.isNull())
        return setError(E_FAIL,
                        tr("The snapshot folder of a machine with snapshots cannot be changed (please discard all snapshots first)"));

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

    int vrc = calculateFullPath(snapshotFolder, snapshotFolder);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL,
                        tr("Invalid snapshot folder '%ls' (%Rrc)"),
                        aSnapshotFolder, vrc);

    mUserData.backup();
    mUserData->mSnapshotFolder = aSnapshotFolder;
    mUserData->mSnapshotFolderFull = snapshotFolder;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(MediumAttachments)(ComSafeArrayOut(IMediumAttachment*, aAttachments))
{
    if (ComSafeArrayOutIsNull(aAttachments))
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IMediumAttachment> attachments(mMediaData->mAttachments);
    attachments.detachTo(ComSafeArrayOutArg(aAttachments));

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(VRDPServer)(IVRDPServer **vrdpServer)
{
#ifdef VBOX_WITH_VRDP
    if (!vrdpServer)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    Assert (!!mVRDPServer);
    mVRDPServer.queryInterfaceTo(vrdpServer);

    return S_OK;
#else
    NOREF(vrdpServer);
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP Machine::COMGETTER(AudioAdapter)(IAudioAdapter **audioAdapter)
{
    if (!audioAdapter)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mAudioAdapter.queryInterfaceTo(audioAdapter);
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(USBController) (IUSBController **aUSBController)
{
#ifdef VBOX_WITH_USB
    CheckComArgOutPointerValid(aUSBController);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    MultiResult rc = mParent->host()->checkUSBProxyService();
    CheckComRCReturnRC(rc);

    AutoReadLock alock(this);

    return rc = mUSBController.queryInterfaceTo(aUSBController);
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treting it as a failure), for example, as in OSE */
    NOREF(aUSBController);
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP Machine::COMGETTER(SettingsFilePath) (BSTR *aFilePath)
{
    CheckComArgOutPointerValid(aFilePath);

    AutoLimitedCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mData->m_strConfigFileFull.cloneTo(aFilePath);
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SettingsModified) (BOOL *aModified)
{
    CheckComArgOutPointerValid(aModified);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    if (mData->mInitMode == Init_New)
          /*
           *  if this is a new machine then no config file exists yet, so always return TRUE
           */
        *aModified = TRUE;
    else
        *aModified = isModified();

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SessionState) (SessionState_T *aSessionState)
{
    CheckComArgOutPointerValid(aSessionState);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aSessionState = mData->mSession.mState;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SessionType) (BSTR *aSessionType)
{
    CheckComArgOutPointerValid(aSessionType);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    if (mData->mSession.mType.isNull())
        Bstr("").cloneTo(aSessionType);
    else
        mData->mSession.mType.cloneTo(aSessionType);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SessionPid) (ULONG *aSessionPid)
{
    CheckComArgOutPointerValid(aSessionPid);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aSessionPid = mData->mSession.mPid;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(State) (MachineState_T *machineState)
{
    if (!machineState)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *machineState = mData->mMachineState;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(LastStateChange) (LONG64 *aLastStateChange)
{
    CheckComArgOutPointerValid(aLastStateChange);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aLastStateChange = RTTimeSpecGetMilli (&mData->mLastStateChange);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(StateFilePath) (BSTR *aStateFilePath)
{
    CheckComArgOutPointerValid(aStateFilePath);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    if (mSSData->mStateFilePath.isEmpty())
        Bstr("").cloneTo(aStateFilePath);
    else
        mSSData->mStateFilePath.cloneTo(aStateFilePath);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(LogFolder) (BSTR *aLogFolder)
{
    CheckComArgOutPointerValid(aLogFolder);

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    Utf8Str logFolder;
    getLogFolder (logFolder);

    Bstr (logFolder).cloneTo(aLogFolder);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CurrentSnapshot) (ISnapshot **aCurrentSnapshot)
{
    CheckComArgOutPointerValid(aCurrentSnapshot);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mData->mCurrentSnapshot.queryInterfaceTo(aCurrentSnapshot);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SnapshotCount)(ULONG *aSnapshotCount)
{
    CheckComArgOutPointerValid(aSnapshotCount);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aSnapshotCount = mData->mFirstSnapshot.isNull()
                          ? 0
                          : mData->mFirstSnapshot->getAllChildrenCount() + 1;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CurrentStateModified) (BOOL *aCurrentStateModified)
{
    CheckComArgOutPointerValid(aCurrentStateModified);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    /* Note: for machines with no snapshots, we always return FALSE
     * (mData->mCurrentStateModified will be TRUE in this case, for historical
     * reasons :) */

    *aCurrentStateModified = mData->mFirstSnapshot.isNull()
                            ? FALSE
                            : mData->mCurrentStateModified;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SharedFolders) (ComSafeArrayOut(ISharedFolder *, aSharedFolders))
{
    CheckComArgOutSafeArrayPointerValid(aSharedFolders);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<ISharedFolder> folders(mHWData->mSharedFolders);
    folders.detachTo(ComSafeArrayOutArg(aSharedFolders));

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(ClipboardMode) (ClipboardMode_T *aClipboardMode)
{
    CheckComArgOutPointerValid(aClipboardMode);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aClipboardMode = mHWData->mClipboardMode;

    return S_OK;
}

STDMETHODIMP
Machine::COMSETTER(ClipboardMode) (ClipboardMode_T aClipboardMode)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mClipboardMode = aClipboardMode;

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(GuestPropertyNotificationPatterns) (BSTR *aPatterns)
{
    CheckComArgOutPointerValid(aPatterns);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mHWData->mGuestPropertyNotificationPatterns.cloneTo(aPatterns);

    return RT_LIKELY (aPatterns != NULL) ? S_OK : E_OUTOFMEMORY; /** @todo r=bird: this is wrong... :-) */
}

STDMETHODIMP
Machine::COMSETTER(GuestPropertyNotificationPatterns) (IN_BSTR aPatterns)
{
    AssertLogRelReturn (VALID_PTR (aPatterns), E_POINTER);
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mGuestPropertyNotificationPatterns = aPatterns;

    return RT_LIKELY (!mHWData->mGuestPropertyNotificationPatterns.isNull())
               ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
Machine::COMGETTER(StorageControllers) (ComSafeArrayOut(IStorageController *, aStorageControllers))
{
    CheckComArgOutSafeArrayPointerValid(aStorageControllers);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IStorageController> ctrls (*mStorageControllers.data());
    ctrls.detachTo(ComSafeArrayOutArg(aStorageControllers));

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(TeleporterEnabled)(BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aEnabled = mUserData->mTeleporterEnabled;

    return S_OK;
}

STDMETHODIMP
Machine::COMSETTER(TeleporterEnabled)(BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    /* Only allow it to be set to true when PoweredOff or Aborted.
       (Clearing it is always permitted.) */
    if (    aEnabled
        &&  mData->mRegistered
        &&  (   mType != IsSessionMachine
             || (   mData->mMachineState != MachineState_PoweredOff
                 && mData->mMachineState != MachineState_Aborted)
            )
       )
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("The machine is not powered off (state is %s)"),
                        Global::stringifyMachineState(mData->mMachineState));

    mUserData.backup();
    mUserData->mTeleporterEnabled = aEnabled;

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(TeleporterPort)(ULONG *aPort)
{
    CheckComArgOutPointerValid(aPort);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aPort = mUserData->mTeleporterPort;

    return S_OK;
}

STDMETHODIMP
Machine::COMSETTER(TeleporterPort)(ULONG aPort)
{
    if (aPort >= _64K)
        return setError(E_INVALIDARG, tr("Invalid port number %d"), aPort);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mUserData.backup();
    mUserData->mTeleporterPort = aPort;

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(TeleporterAddress)(BSTR *aAddress)
{
    CheckComArgOutPointerValid(aAddress);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mUserData->mTeleporterAddress.cloneTo(aAddress);

    return S_OK;
}

STDMETHODIMP
Machine::COMSETTER(TeleporterAddress)(IN_BSTR aAddress)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mUserData.backup();
    mUserData->mTeleporterAddress = aAddress;

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(TeleporterPassword)(BSTR *aPassword)
{
    CheckComArgOutPointerValid(aPassword);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mUserData->mTeleporterPassword.cloneTo(aPassword);

    return S_OK;
}

STDMETHODIMP
Machine::COMSETTER(TeleporterPassword)(IN_BSTR aPassword)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mUserData.backup();
    mUserData->mTeleporterPassword = aPassword;

    return S_OK;
}


// IMachine methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Machine::SetBootOrder (ULONG aPosition, DeviceType_T aDevice)
{
    if (aPosition < 1 || aPosition > SchemaDefs::MaxBootPosition)
        return setError(E_INVALIDARG,
                        tr ("Invalid boot position: %lu (must be in range [1, %lu])"),
                        aPosition, SchemaDefs::MaxBootPosition);

    if (aDevice == DeviceType_USB)
        return setError(E_NOTIMPL,
                        tr("Booting from USB device is currently not supported"));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mBootOrder [aPosition - 1] = aDevice;

    return S_OK;
}

STDMETHODIMP Machine::GetBootOrder (ULONG aPosition, DeviceType_T *aDevice)
{
    if (aPosition < 1 || aPosition > SchemaDefs::MaxBootPosition)
        return setError(E_INVALIDARG,
                       tr("Invalid boot position: %lu (must be in range [1, %lu])"),
                       aPosition, SchemaDefs::MaxBootPosition);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aDevice = mHWData->mBootOrder [aPosition - 1];

    return S_OK;
}

STDMETHODIMP Machine::AttachDevice(IN_BSTR aControllerName,
                                   LONG aControllerPort,
                                   LONG aDevice,
                                   DeviceType_T aType,
                                   IN_BSTR aId)
{
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d aType=%d aId=\"%ls\"\n",
                     aControllerName, aControllerPort, aDevice, aType, aId));

    CheckComArgNotNull(aControllerName);
    CheckComArgNotNull(aId);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* VirtualBox::findHardDisk() and the corresponding other methods for
     * DVD and floppy media need *write* lock (for getting rid of unneeded
     * host drives which got enumerated); also we want to make sure the
     * media object we pick up doesn't get unregistered before we finish. */
    AutoMultiWriteLock2 alock(mParent, this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    /// @todo NEWMEDIA implicit machine registration
    if (!mData->mRegistered)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("Cannot attach storage devices to an unregistered machine"));

    AssertReturn(mData->mMachineState != MachineState_Saved, E_FAIL);

    if (Global::IsOnlineOrTransient(mData->mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Invalid machine state: %s"),
                        Global::stringifyMachineState(mData->mMachineState));

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
        return setError(E_INVALIDARG,
                        tr("The port and/or count parameter are out of range [%lu:%lu]"),
                        portCount,
                        devicesPerPort);

    /* check if the device slot is already busy */
    MediumAttachment *pAttachTemp;
    if ((pAttachTemp = findAttachment(mMediaData->mAttachments,
                                      aControllerName,
                                      aControllerPort,
                                      aDevice)))
    {
        Medium *pMedium = pAttachTemp->medium();
        if (pMedium)
        {
            AutoReadLock mediumLock(pMedium);
            return setError(VBOX_E_OBJECT_IN_USE,
                            tr("Medium '%ls' is already attached to device slot %d on port %d of controller '%ls' of this virtual machine"),
                            pMedium->locationFull().raw(), aDevice, aControllerPort, aControllerName);
        }
        else
            return setError(VBOX_E_OBJECT_IN_USE,
                            tr("Device is already attached to slot %d on port %d of controller '%ls' of this virtual machine"),
                            aDevice, aControllerPort, aControllerName);
    }

    Guid id(aId);

    ComObjPtr<Medium> medium;
    switch (aType)
    {
        case DeviceType_HardDisk:
            /* find a hard disk by UUID */
            rc = mParent->findHardDisk(&id, NULL, true /* aSetError */, &medium);
            CheckComRCReturnRC(rc);
            break;

        case DeviceType_DVD:
            if (!id.isEmpty())
            {
                /* first search for host drive */
                SafeIfaceArray<IMedium> drivevec;
                rc = mParent->host()->COMGETTER(DVDDrives)(ComSafeArrayAsOutParam(drivevec));
                if (SUCCEEDED(rc))
                {
                    for (size_t i = 0; i < drivevec.size(); ++i)
                    {
                        /// @todo eliminate this conversion
                        ComObjPtr<Medium> med = (Medium *)drivevec[i];
                        if (med->id() == id)
                        {
                            medium = med;
                            break;
                        }
                    }
                }

                if (medium.isNull())
                {
                    /* find a DVD image by UUID */
                    rc = mParent->findDVDImage(&id, NULL, true /* aSetError */, &medium);
                    CheckComRCReturnRC(rc);
                }
            }
            else
            {
                /* null UUID means null medium, which needs no code */
            }
            break;

        case DeviceType_Floppy:
            if (!id.isEmpty())
            {
                /* first search for host drive */
                SafeIfaceArray<IMedium> drivevec;
                rc = mParent->host()->COMGETTER(FloppyDrives)(ComSafeArrayAsOutParam(drivevec));
                if (SUCCEEDED(rc))
                {
                    for (size_t i = 0; i < drivevec.size(); ++i)
                    {
                        /// @todo eliminate this conversion
                        ComObjPtr<Medium> med = (Medium *)drivevec[i];
                        if (med->id() == id)
                        {
                            medium = med;
                            break;
                        }
                    }
                }

                if (medium.isNull())
                {
                    /* find a floppy image by UUID */
                    rc = mParent->findFloppyImage(&id, NULL, true /* aSetError */, &medium);
                    CheckComRCReturnRC(rc);
                }
            }
            else
            {
                /* null UUID means null medium, which needs no code */
            }
            break;

        default:
            return setError(E_INVALIDARG,
                            tr("The device type %d is not recognized"),
                            (int)aType);
    }

    AutoCaller mediumCaller(medium);
    CheckComRCReturnRC(mediumCaller.rc());

    AutoWriteLock mediumLock(medium);

    if (   (pAttachTemp = findAttachment(mMediaData->mAttachments, medium))
        && !medium.isNull())
    {
        return setError(VBOX_E_OBJECT_IN_USE,
                        tr("Medium '%ls' is already attached to this virtual machine"),
                        medium->locationFull().raw());
    }

    bool indirect = false;
    if (!medium.isNull())
        indirect = medium->isReadOnly();
    bool associate = true;

    do
    {
        if (mMediaData.isBackedUp())
        {
            const MediaData::AttachmentList &oldAtts = mMediaData.backedUpData()->mAttachments;

            /* check if the medium was attached to the VM before we started
             * changing attachments in which case the attachment just needs to
             * be restored */
            if ((pAttachTemp = findAttachment(oldAtts, medium)))
            {
                AssertReturn(!indirect, E_FAIL);

                /* see if it's the same bus/channel/device */
                if (pAttachTemp->matches(aControllerName, aControllerPort, aDevice))
                {
                    /* the simplest case: restore the whole attachment
                    * and return, nothing else to do */
                    mMediaData->mAttachments.push_back(pAttachTemp);
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

        if (medium->parent().isNull())
        {
            /* first, investigate the backup copy of the current hard disk
             * attachments to make it possible to re-attach existing diffs to
             * another device slot w/o losing their contents */
            if (mMediaData.isBackedUp())
            {
                const MediaData::AttachmentList &oldAtts = mMediaData.backedUpData()->mAttachments;

                MediaData::AttachmentList::const_iterator foundIt = oldAtts.end();
                uint32_t foundLevel = 0;

                for (MediaData::AttachmentList::const_iterator it = oldAtts.begin();
                     it != oldAtts.end();
                     ++it)
                {
                    uint32_t level = 0;
                    MediumAttachment *pAttach = *it;
                    ComObjPtr<Medium> pMedium = pAttach->medium();
                    Assert(!pMedium.isNull() || pAttach->type() != DeviceType_HardDisk);
                    if (pMedium.isNull())
                        continue;

                    if (pMedium->base(&level).equalsTo(medium))
                    {
                        /* skip the hard disk if its currently attached (we
                         * cannot attach the same hard disk twice) */
                        if (findAttachment(mMediaData->mAttachments,
                                           pMedium))
                            continue;

                        /* matched device, channel and bus (i.e. attached to the
                         * same place) will win and immediately stop the search;
                         * otherwise the attachment that has the youngest
                         * descendant of medium will be used
                         */
                        if (pAttach->matches(aControllerName, aControllerPort, aDevice))
                        {
                            /* the simplest case: restore the whole attachment
                             * and return, nothing else to do */
                            mMediaData->mAttachments.push_back(*it);
                            return S_OK;
                        }
                        else if (    foundIt == oldAtts.end()
                                  || level > foundLevel /* prefer younger */
                                )
                        {
                            foundIt = it;
                            foundLevel = level;
                        }
                    }
                }

                if (foundIt != oldAtts.end())
                {
                    /* use the previously attached hard disk */
                    medium = (*foundIt)->medium();
                    mediumCaller.attach(medium);
                    CheckComRCReturnRC(mediumCaller.rc());
                    mediumLock.attach(medium);
                    /* not implicit, doesn't require association with this VM */
                    indirect = false;
                    associate = false;
                    /* go right to the MediumAttachment creation */
                    break;
                }
            }

            /* then, search through snapshots for the best diff in the given
             * hard disk's chain to base the new diff on */

            ComObjPtr<Medium> base;
            ComObjPtr<Snapshot> snap = mData->mCurrentSnapshot;
            while (snap)
            {
                AutoReadLock snapLock(snap);

                const MediaData::AttachmentList &snapAtts = snap->getSnapshotMachine()->mMediaData->mAttachments;

                MediaData::AttachmentList::const_iterator foundIt = snapAtts.end();
                uint32_t foundLevel = 0;

                for (MediaData::AttachmentList::const_iterator it = snapAtts.begin();
                     it != snapAtts.end();
                     ++it)
                {
                    MediumAttachment *pAttach = *it;
                    ComObjPtr<Medium> pMedium = pAttach->medium();
                    Assert(!pMedium.isNull() || pAttach->type() != DeviceType_HardDisk);
                    if (pMedium.isNull())
                        continue;

                    uint32_t level = 0;
                    if (pMedium->base(&level).equalsTo(medium))
                    {
                        /* matched device, channel and bus (i.e. attached to the
                         * same place) will win and immediately stop the search;
                         * otherwise the attachment that has the youngest
                         * descendant of medium will be used
                         */
                        if (pAttach->matches(aControllerName, aControllerPort, aDevice))
                        {
                            foundIt = it;
                            break;
                        }
                        else if (    foundIt == snapAtts.end()
                                  || level > foundLevel /* prefer younger */
                                )
                        {
                            foundIt = it;
                            foundLevel = level;
                        }
                    }
                }

                if (foundIt != snapAtts.end())
                {
                    base = (*foundIt)->medium();
                    break;
                }

                snap = snap->parent();
            }

            /* found a suitable diff, use it as a base */
            if (!base.isNull())
            {
                medium = base;
                mediumCaller.attach(medium);
                CheckComRCReturnRC(mediumCaller.rc());
                mediumLock.attach(medium);
            }
        }

        ComObjPtr<Medium> diff;
        diff.createObject();
        rc = diff->init(mParent,
                        medium->preferredDiffFormat().raw(),
                        BstrFmt("%ls"RTPATH_SLASH_STR,
                                 mUserData->mSnapshotFolderFull.raw()).raw());
        CheckComRCReturnRC(rc);

        /* make sure the hard disk is not modified before createDiffStorage() */
        rc = medium->LockRead(NULL);
        CheckComRCReturnRC(rc);

        /* will leave the lock before the potentially lengthy operation, so
         * protect with the special state */
        MachineState_T oldState = mData->mMachineState;
        setMachineState(MachineState_SettingUp);

        mediumLock.leave();
        alock.leave();

        rc = medium->createDiffStorageAndWait(diff, MediumVariant_Standard);

        alock.enter();
        mediumLock.enter();

        setMachineState(oldState);

        medium->UnlockRead(NULL);

        CheckComRCReturnRC(rc);

        /* use the created diff for the actual attachment */
        medium = diff;
        mediumCaller.attach(medium);
        CheckComRCReturnRC(mediumCaller.rc());
        mediumLock.attach(medium);
    }
    while (0);

    ComObjPtr<MediumAttachment> attachment;
    attachment.createObject();
    rc = attachment->init(this, medium, aControllerName, aControllerPort, aDevice, aType, indirect);
    CheckComRCReturnRC(rc);

    if (associate && !medium.isNull())
    {
        /* as the last step, associate the medium to the VM */
        rc = medium->attachTo(mData->mUuid);
        /* here we can fail because of Deleting, or being in process of
         * creating a Diff */
        CheckComRCReturnRC(rc);
    }

    /* success: finally remember the attachment */
    mMediaData.backup();
    mMediaData->mAttachments.push_back(attachment);

    return rc;
}

STDMETHODIMP Machine::DetachDevice(IN_BSTR aControllerName, LONG aControllerPort,
                                   LONG aDevice)
{
    CheckComArgNotNull(aControllerName);

    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%ld aDevice=%ld\n",
                     aControllerName, aControllerPort, aDevice));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    AssertReturn(mData->mMachineState != MachineState_Saved, E_FAIL);

    if (Global::IsOnlineOrTransient(mData->mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Invalid machine state: %s"),
                        Global::stringifyMachineState(mData->mMachineState));

    MediumAttachment *pAttach = findAttachment(mMediaData->mAttachments,
                                               aControllerName,
                                               aControllerPort,
                                               aDevice);
    if (!pAttach)
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("No storage device attached to device slot %d on port %d of controller '%ls'"),
                        aDevice, aControllerPort, aControllerName);


    if (pAttach->isImplicit())
    {
        /* attempt to implicitly delete the implicitly created diff */

        /// @todo move the implicit flag from MediumAttachment to Medium
        /// and forbid any hard disk operation when it is implicit. Or maybe
        /// a special media state for it to make it even more simple.

        Assert(mMediaData.isBackedUp());

        /* will leave the lock before the potentially lengthy operation, so
         * protect with the special state */
        MachineState_T oldState = mData->mMachineState;
        setMachineState(MachineState_SettingUp);

        alock.leave();

        ComObjPtr<Medium> hd = pAttach->medium();
        rc = hd->deleteStorageAndWait();

        alock.enter();

        setMachineState(oldState);

        CheckComRCReturnRC(rc);
    }

    mMediaData.backup();

    /* we cannot use erase (it) below because backup() above will create
     * a copy of the list and make this copy active, but the iterator
     * still refers to the original and is not valid for the copy */
    mMediaData->mAttachments.remove(pAttach);

    return S_OK;
}

STDMETHODIMP Machine::PassthroughDevice(IN_BSTR aControllerName, LONG aControllerPort,
                                        LONG aDevice, BOOL aPassthrough)
{
    CheckComArgNotNull(aControllerName);

    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%ld aDevice=%ld aPassthrough=%d\n",
                     aControllerName, aControllerPort, aDevice, aPassthrough));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    AssertReturn(mData->mMachineState != MachineState_Saved, E_FAIL);

    if (Global::IsOnlineOrTransient(mData->mMachineState))
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Invalid machine state: %s"),
                        Global::stringifyMachineState(mData->mMachineState));

    MediumAttachment *pAttach = findAttachment(mMediaData->mAttachments,
                                               aControllerName,
                                               aControllerPort,
                                               aDevice);
    if (!pAttach)
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("No storage device attached to device slot %d on port %d of controller '%ls'"),
                        aDevice, aControllerPort, aControllerName);


    mMediaData.backup();

    AutoWriteLock attLock(pAttach);

    if (pAttach->type() != DeviceType_DVD)
        return setError(E_INVALIDARG,
                        tr("Setting passthrough rejected as the device attached to device slot %d on port %d of controller '%ls' is not a DVD"),
                        aDevice, aControllerPort, aControllerName);
    pAttach->updatePassthrough(!!aPassthrough);

    return S_OK;
}

STDMETHODIMP Machine::MountMedium(IN_BSTR aControllerName,
                                  LONG aControllerPort,
                                  LONG aDevice,
                                  IN_BSTR aId)
{
    int rc = S_OK;
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%ld aDevice=%ld\n",
                     aControllerName, aControllerPort, aDevice));

    CheckComArgNotNull(aControllerName);
    CheckComArgNotNull(aId);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    ComObjPtr<MediumAttachment> pAttach = findAttachment(mMediaData->mAttachments,
                                                         aControllerName,
                                                         aControllerPort,
                                                         aDevice);
    if (pAttach.isNull())
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("No drive attached to device slot %d on port %d of controller '%ls'"),
                        aDevice, aControllerPort, aControllerName);

    Guid id(aId);
    ComObjPtr<Medium> medium;
    switch (pAttach->type())
    {
        case DeviceType_DVD:
            if (!id.isEmpty())
            {
                /* find a DVD by host device UUID */
                SafeIfaceArray<IMedium> drivevec;
                rc = mParent->host()->COMGETTER(DVDDrives)(ComSafeArrayAsOutParam(drivevec));
                if (SUCCEEDED(rc))
                {
                    for (size_t i = 0; i < drivevec.size(); ++i)
                    {
                        /// @todo eliminate this conversion
                        ComObjPtr<Medium> med = (Medium *)drivevec[i];
                        if (id == med->id())
                        {
                            medium = med;
                            break;
                        }
                    }
                }
                /* find a DVD by UUID */
                if (medium.isNull())
                    rc = mParent->findDVDImage(&id, NULL, true /* aDoSetError */, &medium);
            }
            CheckComRCReturnRC(rc);
            break;
        case DeviceType_Floppy:
            if (!id.isEmpty())
            {
                /* find a Floppy by host device UUID */
                SafeIfaceArray<IMedium> drivevec;
                rc = mParent->host()->COMGETTER(FloppyDrives)(ComSafeArrayAsOutParam(drivevec));
                if (SUCCEEDED(rc))
                {
                    for (size_t i = 0; i < drivevec.size(); ++i)
                    {
                        /// @todo eliminate this conversion
                        ComObjPtr<Medium> med = (Medium *)drivevec[i];
                        if (id == med->id())
                        {
                            medium = med;
                            break;
                        }
                    }
                }
                /* find a Floppy by UUID */
                if (medium.isNull())
                    rc = mParent->findFloppyImage(&id, NULL, true /* aDoSetError */, &medium);
            }
            CheckComRCReturnRC(rc);
            break;
        default:
            return setError(VBOX_E_INVALID_OBJECT_STATE,
                            tr("Cannot change medium attached to device slot %d on port %d of controller '%ls'"),
                            aDevice, aControllerPort, aControllerName);
    }

    if (SUCCEEDED(rc))
    {

        mMediaData.backup();
        /* The backup operation makes the pAttach reference point to the
         * old settings. Re-get the correct reference. */
        pAttach = findAttachment(mMediaData->mAttachments,
                                 aControllerName,
                                 aControllerPort,
                                 aDevice);
        AutoWriteLock attLock(pAttach);
        if (!medium.isNull())
            medium->attachTo(mData->mUuid);
        pAttach->updateMedium(medium, false /* aImplicit */);
    }

    alock.unlock();
    onMediumChange(pAttach);

    return rc;
}

STDMETHODIMP Machine::GetMedium(IN_BSTR aControllerName,
                                LONG aControllerPort,
                                LONG aDevice,
                                IMedium **aMedium)
{
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%ld aDevice=%ld\n",
                     aControllerName, aControllerPort, aDevice));

    CheckComArgNotNull(aControllerName);
    CheckComArgOutPointerValid(aMedium);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aMedium = NULL;

    ComObjPtr<MediumAttachment> pAttach = findAttachment(mMediaData->mAttachments,
                                                         aControllerName,
                                                         aControllerPort,
                                                         aDevice);
    if (pAttach.isNull())
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("No storage device attached to device slot %d on port %d of controller '%ls'"),
                        aDevice, aControllerPort, aControllerName);

    pAttach->medium().queryInterfaceTo(aMedium);

    return S_OK;
}

STDMETHODIMP Machine::GetSerialPort (ULONG slot, ISerialPort **port)
{
    CheckComArgOutPointerValid(port);
    CheckComArgExpr (slot, slot < RT_ELEMENTS (mSerialPorts));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mSerialPorts [slot].queryInterfaceTo(port);

    return S_OK;
}

STDMETHODIMP Machine::GetParallelPort (ULONG slot, IParallelPort **port)
{
    CheckComArgOutPointerValid(port);
    CheckComArgExpr (slot, slot < RT_ELEMENTS (mParallelPorts));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mParallelPorts [slot].queryInterfaceTo(port);

    return S_OK;
}

STDMETHODIMP Machine::GetNetworkAdapter (ULONG slot, INetworkAdapter **adapter)
{
    CheckComArgOutPointerValid(adapter);
    CheckComArgExpr (slot, slot < RT_ELEMENTS (mNetworkAdapters));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    mNetworkAdapters[slot].queryInterfaceTo(adapter);

    return S_OK;
}

STDMETHODIMP Machine::GetExtraDataKeys(ComSafeArrayOut(BSTR, aKeys))
{
    if (ComSafeArrayOutIsNull(aKeys))
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    com::SafeArray<BSTR> saKeys(mData->m_pMachineConfigFile->mapExtraDataItems.size());
    int i = 0;
    for (settings::ExtraDataItemsMap::const_iterator it = mData->m_pMachineConfigFile->mapExtraDataItems.begin();
         it != mData->m_pMachineConfigFile->mapExtraDataItems.end();
         ++it, ++i)
    {
        const Utf8Str &strKey = it->first;
        strKey.cloneTo(&saKeys[i]);
    }
    saKeys.detachTo(ComSafeArrayOutArg(aKeys));

    return S_OK;
  }

  /**
   *  @note Locks this object for reading.
   */
STDMETHODIMP Machine::GetExtraData(IN_BSTR aKey,
                                   BSTR *aValue)
{
    CheckComArgNotNull(aKey);
    CheckComArgOutPointerValid(aValue);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* start with nothing found */
    Bstr bstrResult("");

    AutoReadLock alock (this);

    settings::ExtraDataItemsMap::const_iterator it = mData->m_pMachineConfigFile->mapExtraDataItems.find(Utf8Str(aKey));
    if (it != mData->m_pMachineConfigFile->mapExtraDataItems.end())
        // found:
        bstrResult = it->second; // source is a Utf8Str

    /* return the result to caller (may be empty) */
    bstrResult.cloneTo(aValue);

    return S_OK;
}

  /**
   *  @note Locks mParent for writing + this object for writing.
   */
STDMETHODIMP Machine::SetExtraData(IN_BSTR aKey, IN_BSTR aValue)
{
    CheckComArgNotNull(aKey);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC (autoCaller.rc());

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
        settings::ExtraDataItemsMap::const_iterator it = mData->m_pMachineConfigFile->mapExtraDataItems.find(strKey);
        if (it != mData->m_pMachineConfigFile->mapExtraDataItems.end())
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

        if (!mParent->onExtraDataCanChange(mData->mUuid, aKey, bstrValue, error))
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

        // data is changing and change not vetoed: then write it out under the locks

        // saveSettings() needs VirtualBox write lock
        AutoMultiWriteLock2 alock(mParent, this);

        if (mType == IsSnapshotMachine)
        {
            HRESULT rc = checkStateDependency(MutableStateDep);
            CheckComRCReturnRC (rc);
        }

        if (strValue.isEmpty())
            mData->m_pMachineConfigFile->mapExtraDataItems.erase(strKey);
        else
            mData->m_pMachineConfigFile->mapExtraDataItems[strKey] = strValue;
                // creates a new key if needed

        /* save settings on success */
        HRESULT rc = saveSettings();
        CheckComRCReturnRC (rc);
    }

    // fire notification outside the lock
    if (fChanged)
        mParent->onExtraDataChange(mData->mUuid, aKey, aValue);

    return S_OK;
}

STDMETHODIMP Machine::SaveSettings()
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* saveSettings() needs mParent lock */
    AutoMultiWriteLock2 alock(mParent, this);

    /* when there was auto-conversion, we want to save the file even if
     * the VM is saved */
    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    /* the settings file path may never be null */
    ComAssertRet(!mData->m_strConfigFileFull.isEmpty(), E_FAIL);

    /* save all VM data excluding snapshots */
    return saveSettings();
}

STDMETHODIMP Machine::DiscardSettings()
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    /*
     *  during this rollback, the session will be notified if data has
     *  been actually changed
     */
    rollback (true /* aNotify */);

    return S_OK;
}

STDMETHODIMP Machine::DeleteSettings()
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    if (mData->mRegistered)
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Cannot delete settings of a registered machine"));

    /* delete the settings only when the file actually exists */
    if (mData->m_pMachineConfigFile->fileExists())
    {
        int vrc = RTFileDelete(mData->m_strConfigFileFull.c_str());
        if (RT_FAILURE(vrc))
            return setError(VBOX_E_IPRT_ERROR,
                            tr("Could not delete the settings file '%s' (%Rrc)"),
                            mData->m_strConfigFileFull.raw(),
                            vrc);

        /* delete the Logs folder, nothing important should be left
         * there (we don't check for errors because the user might have
         * some private files there that we don't want to delete) */
        Utf8Str logFolder;
        getLogFolder(logFolder);
        Assert(logFolder.length());
        if (RTDirExists(logFolder.c_str()))
        {
            /* Delete all VBox.log[.N] files from the Logs folder
             * (this must be in sync with the rotation logic in
             * Console::powerUpThread()). Also, delete the VBox.png[.N]
             * files that may have been created by the GUI. */
            Utf8Str log = Utf8StrFmt("%s/VBox.log", logFolder.raw());
            RTFileDelete(log.c_str());
            log = Utf8StrFmt("%s/VBox.png", logFolder.raw());
            RTFileDelete(log.c_str());
            for (int i = 3; i >= 0; i--)
            {
                log = Utf8StrFmt("%s/VBox.log.%d", logFolder.raw(), i);
                RTFileDelete(log.c_str());
                log = Utf8StrFmt("%s/VBox.png.%d", logFolder.raw(), i);
                RTFileDelete(log.c_str());
            }

            RTDirRemove(logFolder.c_str());
        }

        /* delete the Snapshots folder, nothing important should be left
         * there (we don't check for errors because the user might have
         * some private files there that we don't want to delete) */
        Utf8Str snapshotFolder(mUserData->mSnapshotFolderFull);
        Assert(snapshotFolder.length());
        if (RTDirExists(snapshotFolder.c_str()))
            RTDirRemove(snapshotFolder.c_str());

        /* delete the directory that contains the settings file, but only
         * if it matches the VM name (i.e. a structure created by default in
         * prepareSaveSettings()) */
        {
            Utf8Str settingsDir;
            if (isInOwnDir(&settingsDir))
                RTDirRemove(settingsDir.c_str());
        }
    }

    return S_OK;
}

STDMETHODIMP Machine::GetSnapshot (IN_BSTR aId, ISnapshot **aSnapshot)
{
    CheckComArgOutPointerValid(aSnapshot);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    Guid id(aId);
    ComObjPtr<Snapshot> snapshot;

    HRESULT rc = findSnapshot (id, snapshot, true /* aSetError */);
    snapshot.queryInterfaceTo(aSnapshot);

    return rc;
}

STDMETHODIMP Machine::FindSnapshot (IN_BSTR aName, ISnapshot **aSnapshot)
{
    CheckComArgNotNull (aName);
    CheckComArgOutPointerValid(aSnapshot);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    ComObjPtr<Snapshot> snapshot;

    HRESULT rc = findSnapshot (aName, snapshot, true /* aSetError */);
    snapshot.queryInterfaceTo(aSnapshot);

    return rc;
}

STDMETHODIMP Machine::SetCurrentSnapshot (IN_BSTR /* aId */)
{
    /// @todo (dmik) don't forget to set
    //  mData->mCurrentStateModified to FALSE

    return setError (E_NOTIMPL, "Not implemented");
}

STDMETHODIMP Machine::CreateSharedFolder (IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable)
{
    CheckComArgNotNull(aName);
    CheckComArgNotNull(aHostPath);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    ComObjPtr<SharedFolder> sharedFolder;
    rc = findSharedFolder (aName, sharedFolder, false /* aSetError */);
    if (SUCCEEDED(rc))
        return setError(VBOX_E_OBJECT_IN_USE,
                        tr("Shared folder named '%ls' already exists"),
                        aName);

    sharedFolder.createObject();
    rc = sharedFolder->init (machine(), aName, aHostPath, aWritable);
    CheckComRCReturnRC(rc);

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

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    ComObjPtr<SharedFolder> sharedFolder;
    rc = findSharedFolder (aName, sharedFolder, true /* aSetError */);
    CheckComRCReturnRC(rc);

    mHWData.backup();
    mHWData->mSharedFolders.remove (sharedFolder);

    /* inform the direct session if any */
    alock.leave();
    onSharedFolderChange();

    return S_OK;
}

STDMETHODIMP Machine::CanShowConsoleWindow (BOOL *aCanShow)
{
    CheckComArgOutPointerValid(aCanShow);

    /* start with No */
    *aCanShow = FALSE;

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);

        if (mData->mSession.mState != SessionState_Open)
            return setError(VBOX_E_INVALID_VM_STATE,
                            tr("Machine session is not open (session state: %s)"),
                            Global::stringifySessionState(mData->mSession.mState));

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
    CheckComArgOutPointerValid(aWinId);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);

        if (mData->mSession.mState != SessionState_Open)
            return setError(E_FAIL,
                            tr("Machine session is not open (session state: %s)"),
                            Global::stringifySessionState(mData->mSession.mState));

        directControl = mData->mSession.mDirectControl;
    }

    /* ignore calls made after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    BOOL dummy;
    return directControl->OnShowWindow (FALSE /* aCheck */, &dummy, aWinId);
}

STDMETHODIMP Machine::GetGuestProperty(IN_BSTR aName,
                                       BSTR *aValue,
                                       ULONG64 *aTimestamp,
                                       BSTR *aFlags)
{
#if !defined (VBOX_WITH_GUEST_PROPS)
    ReturnComNotImplemented();
#else
    CheckComArgNotNull(aName);
    CheckComArgOutPointerValid(aValue);
    CheckComArgOutPointerValid(aTimestamp);
    CheckComArgOutPointerValid(aFlags);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    using namespace guestProp;
    HRESULT rc = E_FAIL;

    Utf8Str strName(aName);

    if (!mHWData->mPropertyServiceActive)
    {
        bool found = false;
        for (HWData::GuestPropertyList::const_iterator it = mHWData->mGuestProperties.begin();
             (it != mHWData->mGuestProperties.end()) && !found;
             ++it)
        {
            if (it->strName == strName)
            {
                char szFlags[MAX_FLAGS_LEN + 1];
                it->strValue.cloneTo(aValue);
                *aTimestamp = it->mTimestamp;
                writeFlags(it->mFlags, szFlags);
                Bstr(szFlags).cloneTo(aFlags);
                found = true;
            }
        }
        rc = S_OK;
    }
    else
    {
        ComPtr<IInternalSessionControl> directControl =
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

STDMETHODIMP Machine::SetGuestProperty(IN_BSTR aName,
                                       IN_BSTR aValue,
                                       IN_BSTR aFlags)
{
#if !defined (VBOX_WITH_GUEST_PROPS)
    ReturnComNotImplemented();
#else
    using namespace guestProp;

    CheckComArgNotNull(aName);
    CheckComArgNotNull(aValue);
    if ((aFlags != NULL) && !VALID_PTR (aFlags))
        return E_INVALIDARG;

    HRESULT rc = S_OK;

    try
    {
        Utf8Str utf8Name(aName);
        Utf8Str utf8Flags(aFlags);
        Utf8Str utf8Patterns(mHWData->mGuestPropertyNotificationPatterns);

        bool matchAll = false;
        if (utf8Patterns.isEmpty())
            matchAll = true;

        uint32_t fFlags = NILFLAG;
        if (    (aFlags != NULL)
             && RT_FAILURE(validateFlags (utf8Flags.raw(), &fFlags))
           )
            return setError(E_INVALIDARG,
                            tr("Invalid flag values: '%ls'"),
                            aFlags);

        AutoCaller autoCaller(this);
        CheckComRCReturnRC(autoCaller.rc());

        AutoWriteLock alock(this);

        rc = checkStateDependency(MutableStateDep);
        CheckComRCReturnRC(rc);

        rc = S_OK;

        if (!mHWData->mPropertyServiceActive)
        {
            bool found = false;
            HWData::GuestProperty property;
            property.mFlags = NILFLAG;

            if (SUCCEEDED(rc))
            {
                for (HWData::GuestPropertyList::iterator it = mHWData->mGuestProperties.begin();
                     it != mHWData->mGuestProperties.end();
                     ++it)
                    if (it->strName == utf8Name)
                    {
                        property = *it;
                        if (it->mFlags & (RDONLYHOST))
                            rc = setError(E_ACCESSDENIED,
                                          tr("The property '%ls' cannot be changed by the host"),
                                          aName);
                        else
                        {
                            mHWData.backup();
                            /* The backup() operation invalidates our iterator, so
                            * get a new one. */
                            for (it = mHWData->mGuestProperties.begin();
                                 it->strName != utf8Name;
                                 ++it)
                                ;
                            mHWData->mGuestProperties.erase (it);
                        }
                        found = true;
                        break;
                    }
            }
            if (found && SUCCEEDED(rc))
            {
                if (*aValue)
                {
                    RTTIMESPEC time;
                    property.strValue = aValue;
                    property.mTimestamp = RTTimeSpecGetNano(RTTimeNow(&time));
                    if (aFlags != NULL)
                        property.mFlags = fFlags;
                    mHWData->mGuestProperties.push_back (property);
                }
            }
            else if (SUCCEEDED(rc) && *aValue)
            {
                RTTIMESPEC time;
                mHWData.backup();
                property.strName = aName;
                property.strValue = aValue;
                property.mTimestamp = RTTimeSpecGetNano(RTTimeNow(&time));
                property.mFlags = fFlags;
                mHWData->mGuestProperties.push_back (property);
            }
            if (   SUCCEEDED(rc)
                && (   matchAll
                    || RTStrSimplePatternMultiMatch (utf8Patterns.raw(), RTSTR_MAX,
                                                    utf8Name.raw(), RTSTR_MAX, NULL)
                )
            )
                mParent->onGuestPropertyChange (mData->mUuid, aName, aValue, aFlags);
        }
        else
        {
            ComPtr<IInternalSessionControl> directControl =
                mData->mSession.mDirectControl;

            /* just be on the safe side when calling another process */
            alock.leave();

            BSTR dummy = NULL;
            ULONG64 dummy64;
            if (!directControl)
                rc = E_FAIL;
            else
                rc = directControl->AccessGuestProperty
                             (aName, *aValue ? aValue : NULL, aFlags,
                              true /* isSetter */, &dummy, &dummy64, &dummy);
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }

    return rc;
#endif /* else !defined (VBOX_WITH_GUEST_PROPS) */
}

STDMETHODIMP Machine::SetGuestPropertyValue (IN_BSTR aName, IN_BSTR aValue)
{
    return SetGuestProperty (aName, aValue, NULL);
}

STDMETHODIMP Machine::EnumerateGuestProperties(IN_BSTR aPatterns,
                                               ComSafeArrayOut(BSTR, aNames),
                                               ComSafeArrayOut(BSTR, aValues),
                                               ComSafeArrayOut(ULONG64, aTimestamps),
                                               ComSafeArrayOut(BSTR, aFlags))
{
#if !defined (VBOX_WITH_GUEST_PROPS)
    ReturnComNotImplemented();
#else
    if (!VALID_PTR (aPatterns) && (aPatterns != NULL))
        return E_POINTER;

    CheckComArgOutSafeArrayPointerValid(aNames);
    CheckComArgOutSafeArrayPointerValid(aValues);
    CheckComArgOutSafeArrayPointerValid(aTimestamps);
    CheckComArgOutSafeArrayPointerValid(aFlags);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    using namespace guestProp;
    HRESULT rc = E_FAIL;

    Utf8Str strPatterns(aPatterns);

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
             it != mHWData->mGuestProperties.end();
             ++it)
            if (   matchAll
                || RTStrSimplePatternMultiMatch(strPatterns.raw(),
                                                RTSTR_MAX,
                                                it->strName.raw(),
                                                RTSTR_MAX, NULL)
               )
                propList.push_back(*it);

        /*
         * And build up the arrays for returning the property information.
         */
        size_t cEntries = propList.size();
        SafeArray<BSTR> names (cEntries);
        SafeArray<BSTR> values (cEntries);
        SafeArray<ULONG64> timestamps (cEntries);
        SafeArray<BSTR> flags (cEntries);
        size_t iProp = 0;
        for (HWData::GuestPropertyList::iterator it = propList.begin();
             it != propList.end();
             ++it)
        {
             char szFlags[MAX_FLAGS_LEN + 1];
             it->strName.cloneTo(&names[iProp]);
             it->strValue.cloneTo(&values[iProp]);
             timestamps[iProp] = it->mTimestamp;
             writeFlags(it->mFlags, szFlags);
             Bstr(szFlags).cloneTo(&flags[iProp]);
             ++iProp;
        }
        names.detachTo(ComSafeArrayOutArg(aNames));
        values.detachTo(ComSafeArrayOutArg(aValues));
        timestamps.detachTo(ComSafeArrayOutArg(aTimestamps));
        flags.detachTo(ComSafeArrayOutArg(aFlags));
        rc = S_OK;
    }
    else
    {
        ComPtr<IInternalSessionControl> directControl = mData->mSession.mDirectControl;

        /* just be on the safe side when calling another process */
        alock.unlock();

        if (!directControl)
            rc = E_FAIL;
        else
            rc = directControl->EnumerateGuestProperties(aPatterns,
                                                         ComSafeArrayOutArg(aNames),
                                                         ComSafeArrayOutArg(aValues),
                                                         ComSafeArrayOutArg(aTimestamps),
                                                         ComSafeArrayOutArg(aFlags));
    }
    return rc;
#endif /* else !defined (VBOX_WITH_GUEST_PROPS) */
}

STDMETHODIMP Machine::GetMediumAttachmentsOfController(IN_BSTR aName,
                                                       ComSafeArrayOut(IMediumAttachment*, aAttachments))
{
    MediaData::AttachmentList atts;

    HRESULT rc = getMediumAttachmentsOfController(aName, atts);
    CheckComRCReturnRC(rc);

    SafeIfaceArray<IMediumAttachment> attachments(atts);
    attachments.detachTo(ComSafeArrayOutArg(aAttachments));

    return S_OK;
}

STDMETHODIMP Machine::GetMediumAttachment(IN_BSTR aControllerName,
                                          LONG aControllerPort,
                                          LONG aDevice,
                                          IMediumAttachment **aAttachment)
{
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d\n",
                     aControllerName, aControllerPort, aDevice));

    CheckComArgNotNull(aControllerName);
    CheckComArgOutPointerValid(aAttachment);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aAttachment = NULL;

    ComObjPtr<MediumAttachment> pAttach = findAttachment(mMediaData->mAttachments,
                                                         aControllerName,
                                                         aControllerPort,
                                                         aDevice);
    if (pAttach.isNull())
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("No storage device attached to device slot %d on port %d of controller '%ls'"),
                        aDevice, aControllerPort, aControllerName);

    pAttach.queryInterfaceTo(aAttachment);

    return S_OK;
}

STDMETHODIMP Machine::AddStorageController(IN_BSTR aName,
                                           StorageBus_T aConnectionType,
                                           IStorageController **controller)
{
    CheckComArgStrNotEmptyOrNull(aName);

    if (   (aConnectionType <= StorageBus_Null)
        || (aConnectionType >  StorageBus_Floppy))
        return setError (E_INVALIDARG,
            tr ("Invalid connection type: %d"),
                aConnectionType);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    /* try to find one with the name first. */
    ComObjPtr<StorageController> ctrl;

    rc = getStorageControllerByName (aName, ctrl, false /* aSetError */);
    if (SUCCEEDED(rc))
        return setError (VBOX_E_OBJECT_IN_USE,
            tr ("Storage controller named '%ls' already exists"), aName);

    ctrl.createObject();
    rc = ctrl->init (this, aName, aConnectionType);
    CheckComRCReturnRC(rc);

    mStorageControllers.backup();
    mStorageControllers->push_back (ctrl);

    ctrl.queryInterfaceTo(controller);

    /* inform the direct session if any */
    alock.leave();
    onStorageControllerChange();

    return S_OK;
}

STDMETHODIMP Machine::GetStorageControllerByName(IN_BSTR aName,
                                                 IStorageController **aStorageController)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    ComObjPtr<StorageController> ctrl;

    HRESULT rc = getStorageControllerByName (aName, ctrl, true /* aSetError */);
    if (SUCCEEDED(rc))
        ctrl.queryInterfaceTo(aStorageController);

    return rc;
}

STDMETHODIMP Machine::RemoveStorageController(IN_BSTR aName)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(MutableStateDep);
    CheckComRCReturnRC(rc);

    ComObjPtr<StorageController> ctrl;
    rc = getStorageControllerByName (aName, ctrl, true /* aSetError */);
    CheckComRCReturnRC(rc);

    /* We can remove the controller only if there is no device attached. */
    /* check if the device slot is already busy */
    for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
         it != mMediaData->mAttachments.end();
         ++it)
    {
        if (Bstr((*it)->controllerName()) == aName)
            return setError(VBOX_E_OBJECT_IN_USE,
                            tr("Storage controller named '%ls' has still devices attached"),
                            aName);
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
HRESULT Machine::saveRegistryEntry(settings::MachineRegistryEntry &data)
{
    AutoLimitedCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    data.uuid = mData->mUuid;
    data.strSettingsFile = mData->m_strConfigFile;

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
int Machine::calculateFullPath(const Utf8Str &strPath, Utf8Str &aResult)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this);

    AssertReturn (!mData->m_strConfigFileFull.isEmpty(), VERR_GENERAL_FAILURE);

    Utf8Str strSettingsDir = mData->m_strConfigFileFull;

    strSettingsDir.stripFilename();
    char folder[RTPATH_MAX];
    int vrc = RTPathAbsEx(strSettingsDir.c_str(), strPath.c_str(), folder, sizeof(folder));
    if (RT_SUCCESS(vrc))
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
void Machine::calculateRelativePath(const Utf8Str &strPath, Utf8Str &aResult)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    AutoReadLock alock(this);

    AssertReturnVoid (!mData->m_strConfigFileFull.isEmpty());

    Utf8Str settingsDir = mData->m_strConfigFileFull;

    settingsDir.stripFilename();
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

/**
 *  Returns the full path to the machine's log folder in the
 *  \a aLogFolder argument.
 */
void Machine::getLogFolder (Utf8Str &aLogFolder)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoReadLock alock(this);

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
HRESULT Machine::openSession(IInternalSessionControl *aControl)
{
    LogFlowThisFuncEnter();

    AssertReturn(aControl, E_FAIL);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (!mData->mRegistered)
        return setError(E_UNEXPECTED,
                        tr("The machine '%ls' is not registered"),
                        mUserData->mName.raw());

    LogFlowThisFunc(("mSession.mState=%s\n", Global::stringifySessionState(mData->mSession.mState)));

    /* Hack: in case the session is closing and there is a progress object
     * which allows waiting for the session to be closed, take the opportunity
     * and do a limited wait (max. 1 second). This helps a lot when the system
     * is busy and thus session closing can take a little while. */
    if (    mData->mSession.mState == SessionState_Closing
        &&  mData->mSession.mProgress)
    {
        alock.leave();
        mData->mSession.mProgress->WaitForCompletion(1000);
        alock.enter();
        LogFlowThisFunc(("after waiting: mSession.mState=%s\n", Global::stringifySessionState(mData->mSession.mState)));
    }

    if (mData->mSession.mState == SessionState_Open ||
        mData->mSession.mState == SessionState_Closing)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("A session for the machine '%ls' is currently open (or being closed)"),
                        mUserData->mName.raw());

    /* may not be busy */
    AssertReturn(!Global::IsOnlineOrTransient(mData->mMachineState), E_FAIL);

    /* get the session PID */
    RTPROCESS pid = NIL_RTPROCESS;
    AssertCompile(sizeof(ULONG) == sizeof(RTPROCESS));
    aControl->GetPID((ULONG *) &pid);
    Assert(pid != NIL_RTPROCESS);

    if (mData->mSession.mState == SessionState_Spawning)
    {
        /* This machine is awaiting for a spawning session to be opened, so
         * reject any other open attempts from processes other than one
         * started by #openRemoteSession(). */

        LogFlowThisFunc(("mSession.mPid=%d(0x%x)\n",
                          mData->mSession.mPid, mData->mSession.mPid));
        LogFlowThisFunc(("session.pid=%d(0x%x)\n", pid, pid));

        if (mData->mSession.mPid != pid)
            return setError(E_ACCESSDENIED,
                            tr("An unexpected process (PID=0x%08X) has tried to open a direct "
                               "session with the machine named '%ls', while only a process "
                               "started by OpenRemoteSession (PID=0x%08X) is allowed"),
                            pid, mUserData->mName.raw(), mData->mSession.mPid);
    }

    /* create a SessionMachine object */
    ComObjPtr<SessionMachine> sessionMachine;
    sessionMachine.createObject();
    HRESULT rc = sessionMachine->init(this);
    AssertComRC(rc);

    /* NOTE: doing return from this function after this point but
     * before the end is forbidden since it may call SessionMachine::uninit()
     * (through the ComObjPtr's destructor) which requests the VirtualBox write
     * lock while still holding the Machine lock in alock so that a deadlock
     * is possible due to the wrong lock order. */

    if (SUCCEEDED(rc))
    {
#ifdef VBOX_WITH_RESOURCE_USAGE_API
        registerMetrics(mParent->performanceCollector(), this, pid);
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

        LogFlowThisFunc(("Calling AssignMachine()...\n"));
        rc = aControl->AssignMachine(sessionMachine);
        LogFlowThisFunc(("AssignMachine() returned %08X\n", rc));

        /* The failure may occur w/o any error info (from RPC), so provide one */
        if (FAILED(rc))
            setError(VBOX_E_VM_ERROR,
                tr("Failed to assign the machine to the session (%Rrc)"), rc);

        if (SUCCEEDED(rc) && origState == SessionState_Spawning)
        {
            /* complete the remote session initialization */

            /* get the console from the direct session */
            ComPtr<IConsole> console;
            rc = aControl->GetRemoteConsole(console.asOutParam());
            ComAssertComRC(rc);

            if (SUCCEEDED(rc) && !console)
            {
                ComAssert(!!console);
                rc = E_FAIL;
            }

            /* assign machine & console to the remote session */
            if (SUCCEEDED(rc))
            {
                /*
                 *  after openRemoteSession(), the first and the only
                 *  entry in remoteControls is that remote session
                 */
                LogFlowThisFunc(("Calling AssignRemoteMachine()...\n"));
                rc = mData->mSession.mRemoteControls.front()->
                    AssignRemoteMachine(sessionMachine, console);
                LogFlowThisFunc(("AssignRemoteMachine() returned %08X\n", rc));

                /* The failure may occur w/o any error info (from RPC), so provide one */
                if (FAILED(rc))
                    setError(VBOX_E_VM_ERROR,
                             tr("Failed to assign the machine to the remote session (%Rrc)"), rc);
            }

            if (FAILED(rc))
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

        if (FAILED(rc))
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
        if (SUCCEEDED(rc))
            mData->mSession.mPid = pid;
    }

    if (SUCCEEDED(rc))
    {
        /* memorize the direct session control and cache IUnknown for it */
        mData->mSession.mDirectControl = aControl;
        mData->mSession.mState = SessionState_Open;
        /* associate the SessionMachine with this Machine */
        mData->mSession.mMachine = sessionMachine;

        /* request an IUnknown pointer early from the remote party for later
         * identity checks (it will be internally cached within mDirectControl
         * at least on XPCOM) */
        ComPtr<IUnknown> unk = mData->mSession.mDirectControl;
        NOREF(unk);
    }

    if (mData->mSession.mProgress)
    {
        /* finalize the progress after setting the state, for consistency */
        mData->mSession.mProgress->notifyComplete(rc);
        mData->mSession.mProgress.setNull();
    }

    /* Leave the lock since SessionMachine::uninit() locks VirtualBox which
     * would break the lock order */
    alock.leave();

    /* uninitialize the created session machine on failure */
    if (FAILED(rc))
        sessionMachine->uninit();

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

/**
 *  @note Locks this object for writing, calls the client process
 *        (inside the lock).
 */
HRESULT Machine::openRemoteSession(IInternalSessionControl *aControl,
                                   IN_BSTR aType,
                                   IN_BSTR aEnvironment,
                                   Progress *aProgress)
{
    LogFlowThisFuncEnter();

    AssertReturn(aControl, E_FAIL);
    AssertReturn(aProgress, E_FAIL);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (!mData->mRegistered)
        return setError(E_UNEXPECTED,
                        tr("The machine '%ls' is not registered"),
                        mUserData->mName.raw());

    LogFlowThisFunc(("mSession.mState=%s\n", Global::stringifySessionState(mData->mSession.mState)));

    if (mData->mSession.mState == SessionState_Open ||
        mData->mSession.mState == SessionState_Spawning ||
        mData->mSession.mState == SessionState_Closing)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("A session for the machine '%ls' is currently open (or being opened or closed)"),
                        mUserData->mName.raw());

    /* may not be busy */
    AssertReturn(!Global::IsOnlineOrTransient (mData->mMachineState), E_FAIL);

    /* get the path to the executable */
    char szPath[RTPATH_MAX];
    RTPathAppPrivateArch(szPath, RTPATH_MAX);
    size_t sz = strlen(szPath);
    szPath[sz++] = RTPATH_DELIMITER;
    szPath[sz] = 0;
    char *cmd = szPath + sz;
    sz = RTPATH_MAX - sz;

    int vrc = VINF_SUCCESS;
    RTPROCESS pid = NIL_RTPROCESS;

    RTENV env = RTENV_DEFAULT;

    if (aEnvironment != NULL && *aEnvironment)
    {
        char *newEnvStr = NULL;

        do
        {
            /* clone the current environment */
            int vrc2 = RTEnvClone(&env, RTENV_DEFAULT);
            AssertRCBreakStmt(vrc2, vrc = vrc2);

            newEnvStr = RTStrDup(Utf8Str(aEnvironment).c_str());
            AssertPtrBreakStmt(newEnvStr, vrc = vrc2);

            /* put new variables to the environment
             * (ignore empty variable names here since RTEnv API
             * intentionally doesn't do that) */
            char *var = newEnvStr;
            for (char *p = newEnvStr; *p; ++p)
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
                        if (RT_FAILURE(vrc2))
                            break;
                    }
                    var = p + 1;
                }
            }
            if (RT_SUCCESS(vrc2) && *var)
                vrc2 = RTEnvPutEx (env, var);

            AssertRCBreakStmt (vrc2, vrc = vrc2);
        }
        while (0);

        if (newEnvStr != NULL)
            RTStrFree(newEnvStr);
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
        const char * args[] = {szPath, "--startvm", idStr.c_str(), 0 };
# else
        Utf8Str name = mUserData->mName;
        const char * args[] = {szPath, "--comment", name.c_str(), "--startvm", idStr.c_str(), 0 };
# endif
        vrc = RTProcCreate(szPath, args, env, 0, &pid);
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
        const char * args[] = {szPath, "--startvm", idStr.c_str(), 0 };
# else
        Utf8Str name = mUserData->mName;
        const char * args[] = {szPath, "--comment", name.c_str(), "--startvm", idStr.c_str(), 0 };
# endif
        vrc = RTProcCreate(szPath, args, env, 0, &pid);
    }
#else /* !VBOX_WITH_VBOXSDL */
    if (0)
        ;
#endif /* !VBOX_WITH_VBOXSDL */

    else

#ifdef VBOX_WITH_HEADLESS
    if (   type == "headless"
        || type == "capture"
#ifdef VBOX_WITH_VRDP
        || type == "vrdp"
#endif
       )
    {
        const char VBoxHeadless_exe[] = "VBoxHeadless" HOSTSUFF_EXE;
        Assert (sz >= sizeof (VBoxHeadless_exe));
        strcpy (cmd, VBoxHeadless_exe);

        Utf8Str idStr = mData->mUuid.toString();
        /* Leave space for 2 args, as "headless" needs --vrdp off on non-OSE. */
# ifdef RT_OS_WINDOWS
        const char * args[] = {szPath, "--startvm", idStr.c_str(), 0, 0, 0 };
# else
        Utf8Str name = mUserData->mName;
        const char * args[] ={szPath, "--comment", name.c_str(), "--startvm", idStr.c_str(), 0, 0, 0 };
# endif
#ifdef VBOX_WITH_VRDP
        if (type == "headless")
        {
            unsigned pos = RT_ELEMENTS(args) - 3;
            args[pos++] = "--vrdp";
            args[pos] = "off";
        }
#endif
        if (type == "capture")
        {
            unsigned pos = RT_ELEMENTS(args) - 3;
            args[pos] = "--capture";
        }
        vrc = RTProcCreate(szPath, args, env, 0, &pid);
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

    if (RT_FAILURE(vrc))
        return setError (VBOX_E_IPRT_ERROR,
            tr ("Could not launch a process for the machine '%ls' (%Rrc)"),
            mUserData->mName.raw(), vrc);

    LogFlowThisFunc(("launched.pid=%d(0x%x)\n", pid, pid));

    /*
     *  Note that we don't leave the lock here before calling the client,
     *  because it doesn't need to call us back if called with a NULL argument.
     *  Leaving the lock herer is dangerous because we didn't prepare the
     *  launch data yet, but the client we've just started may happen to be
     *  too fast and call openSession() that will fail (because of PID, etc.),
     *  so that the Machine will never get out of the Spawning session state.
     */

    /* inform the session that it will be a remote one */
    LogFlowThisFunc(("Calling AssignMachine (NULL)...\n"));
    HRESULT rc = aControl->AssignMachine (NULL);
    LogFlowThisFunc(("AssignMachine (NULL) returned %08X\n", rc));

    if (FAILED(rc))
    {
        /* restore the session state */
        mData->mSession.mState = SessionState_Closed;
        /* The failure may occur w/o any error info (from RPC), so provide one */
        return setError(VBOX_E_VM_ERROR,
                        tr("Failed to assign the machine to the session (%Rrc)"), rc);
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

    AssertReturn(aControl, E_FAIL);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (!mData->mRegistered)
        return setError (E_UNEXPECTED,
            tr ("The machine '%ls' is not registered"), mUserData->mName.raw());

    LogFlowThisFunc(("mSession.state=%s\n", Global::stringifySessionState(mData->mSession.mState)));

    if (mData->mSession.mState != SessionState_Open)
        return setError (VBOX_E_INVALID_SESSION_STATE,
            tr ("The machine '%ls' does not have an open session"),
            mUserData->mName.raw());

    ComAssertRet (!mData->mSession.mDirectControl.isNull(), E_FAIL);

    /*
     *  Get the console from the direct session (note that we don't leave the
     *  lock here because GetRemoteConsole must not call us back).
     */
    ComPtr<IConsole> console;
    HRESULT rc = mData->mSession.mDirectControl->
                     GetRemoteConsole (console.asOutParam());
    if (FAILED (rc))
    {
        /* The failure may occur w/o any error info (from RPC), so provide one */
        return setError (VBOX_E_VM_ERROR,
            tr ("Failed to get a console object from the direct session (%Rrc)"), rc);
    }

    ComAssertRet (!console.isNull(), E_FAIL);

    ComObjPtr<SessionMachine> sessionMachine = mData->mSession.mMachine;
    AssertReturn(!sessionMachine.isNull(), E_FAIL);

    /*
     *  Leave the lock before calling the client process. It's safe here
     *  since the only thing to do after we get the lock again is to add
     *  the remote control to the list (which doesn't directly influence
     *  anything).
     */
    alock.leave();

    /* attach the remote session to the machine */
    LogFlowThisFunc(("Calling AssignRemoteMachine()...\n"));
    rc = aControl->AssignRemoteMachine (sessionMachine, console);
    LogFlowThisFunc(("AssignRemoteMachine() returned %08X\n", rc));

    /* The failure may occur w/o any error info (from RPC), so provide one */
    if (FAILED(rc))
        return setError(VBOX_E_VM_ERROR,
                        tr("Failed to assign the machine to the session (%Rrc)"),
                        rc);

    alock.enter();

    /* need to revalidate the state after entering the lock again */
    if (mData->mSession.mState != SessionState_Open)
    {
        aControl->Uninitialize();

        return setError(VBOX_E_INVALID_SESSION_STATE,
                        tr("The machine '%ls' does not have an open session"),
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
bool Machine::isSessionOpen (ComObjPtr<SessionMachine> &aMachine,
                             ComPtr<IInternalSessionControl> *aControl /*= NULL*/,
                             HANDLE *aIPCSem /*= NULL*/,
                             bool aAllowClosing /*= false*/)
#elif defined (RT_OS_OS2)
bool Machine::isSessionOpen (ComObjPtr<SessionMachine> &aMachine,
                             ComPtr<IInternalSessionControl> *aControl /*= NULL*/,
                             HMTX *aIPCSem /*= NULL*/,
                             bool aAllowClosing /*= false*/)
#else
bool Machine::isSessionOpen (ComObjPtr<SessionMachine> &aMachine,
                             ComPtr<IInternalSessionControl> *aControl /*= NULL*/,
                             bool aAllowClosing /*= false*/)
#endif
{
    AutoLimitedCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), false);

    /* just return false for inaccessible machines */
    if (autoCaller.state() != Ready)
        return false;

    AutoReadLock alock(this);

    if (mData->mSession.mState == SessionState_Open ||
        (aAllowClosing && mData->mSession.mState == SessionState_Closing))
    {
        AssertReturn(!mData->mSession.mMachine.isNull(), false);

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
    AutoLimitedCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), false);

    /* just return false for inaccessible machines */
    if (autoCaller.state() != Ready)
        return false;

    AutoReadLock alock(this);

    if (mData->mSession.mState == SessionState_Spawning)
    {
#if defined (RT_OS_WINDOWS) || defined (RT_OS_OS2)
        /* Additional session data */
        if (aPID != NULL)
        {
            AssertReturn(mData->mSession.mPid != NIL_RTPROCESS, false);
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
    AutoCaller autoCaller(this);
    if (!autoCaller.isOk())
    {
        /* nothing to do */
        LogFlowThisFunc(("Already uninitialized!\n"));
        return true;
    }

    /* VirtualBox::addProcessToReap() needs a write lock */
    AutoMultiWriteLock2 alock(mParent, this);

    if (mData->mSession.mState != SessionState_Spawning)
    {
        /* nothing to do */
        LogFlowThisFunc(("Not spawning any more!\n"));
        return true;
    }

    HRESULT rc = S_OK;

#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)

    /* the process was already unexpectedly terminated, we just need to set an
     * error and finalize session spawning */
    rc = setError(E_FAIL,
                  tr("Virtual machine '%ls' has terminated unexpectedly during startup"),
                  name().raw());
#else

    /* PID not yet initialized, skip check. */
    if (mData->mSession.mPid == NIL_RTPROCESS)
        return false;

    RTPROCSTATUS status;
    int vrc = ::RTProcWait(mData->mSession.mPid, RTPROCWAIT_FLAGS_NOBLOCK,
                           &status);

    if (vrc != VERR_PROCESS_RUNNING)
        rc = setError(E_FAIL,
                      tr("Virtual machine '%ls' has terminated unexpectedly during startup"),
                      name().raw());
#endif

    if (FAILED(rc))
    {
        /* Close the remote session, remove the remote control from the list
         * and reset session state to Closed (@note keep the code in sync with
         * the relevant part in checkForSpawnFailure()). */

        Assert(mData->mSession.mRemoteControls.size() == 1);
        if (mData->mSession.mRemoteControls.size() == 1)
        {
            ErrorInfoKeeper eik;
            mData->mSession.mRemoteControls.front()->Uninitialize();
        }

        mData->mSession.mRemoteControls.clear();
        mData->mSession.mState = SessionState_Closed;

        /* finalize the progress after setting the state, for consistency */
        if (!mData->mSession.mProgress.isNull())
        {
            mData->mSession.mProgress->notifyComplete(rc);
            mData->mSession.mProgress.setNull();
        }

        mParent->addProcessToReap(mData->mSession.mPid);
        mData->mSession.mPid = NIL_RTPROCESS;

        mParent->onSessionStateChange(mData->mUuid, SessionState_Closed);
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
    AssertReturn(mParent->isWriteLockOnCurrentThread(), E_FAIL);

    AutoLimitedCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    /* wait for state dependants to drop to zero */
    ensureNoStateDependencies();

    ComAssertRet (mData->mRegistered != aRegistered, E_FAIL);

    if (!mData->mAccessible)
    {
        /* A special case: the machine is not accessible. */

        /* inaccessible machines can only be unregistered */
        AssertReturn(!aRegistered, E_FAIL);

        /* Uninitialize ourselves here because currently there may be no
         * unregistered that are inaccessible (this state combination is not
         * supported). Note releasing the caller and leaving the lock before
         * calling uninit() */

        alock.leave();
        autoCaller.release();

        uninit();

        return S_OK;
    }

    AssertReturn(autoCaller.state() == Ready, E_FAIL);

    if (aRegistered)
    {
        if (mData->mRegistered)
            return setError(VBOX_E_INVALID_OBJECT_STATE,
                            tr("The machine '%ls' with UUID {%s} is already registered"),
                            mUserData->mName.raw(),
                            mData->mUuid.toString().raw());
    }
    else
    {
        if (mData->mMachineState == MachineState_Saved)
            return setError(VBOX_E_INVALID_VM_STATE,
                            tr("Cannot unregister the machine '%ls' because it is in the Saved state"),
                            mUserData->mName.raw());

        size_t snapshotCount = 0;
        if (mData->mFirstSnapshot)
            snapshotCount = mData->mFirstSnapshot->getAllChildrenCount() + 1;
        if (snapshotCount)
            return setError(VBOX_E_INVALID_OBJECT_STATE,
                            tr("Cannot unregister the machine '%ls' because it has %d snapshots"),
                            mUserData->mName.raw(), snapshotCount);

        if (mData->mSession.mState != SessionState_Closed)
            return setError(VBOX_E_INVALID_OBJECT_STATE,
                            tr("Cannot unregister the machine '%ls' because it has an open session"),
                            mUserData->mName.raw());

        if (mMediaData->mAttachments.size() != 0)
            return setError(VBOX_E_INVALID_OBJECT_STATE,
                            tr("Cannot unregister the machine '%ls' because it has %d medium attachments"),
                            mUserData->mName.raw(),
                            mMediaData->mAttachments.size());

        /* Note that we do not prevent unregistration of a DVD or Floppy image
         * is attached: as opposed to hard disks detaching such an image
         * implicitly in this method (which we will do below) won't have any
         * side effects (like detached orphan base and diff hard disks etc).*/
    }

    HRESULT rc = S_OK;

    /* Ensure the settings are saved. If we are going to be registered and
     * isConfigLocked() is FALSE then it means that no config file exists yet,
     * so create it by calling saveSettings() too. */
    if (    isModified()
         || (aRegistered && !mData->m_pMachineConfigFile->fileExists())
       )
    {
        rc = saveSettings();
        CheckComRCReturnRC(rc);
    }

    /* more config checking goes here */

    if (SUCCEEDED(rc))
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
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = checkStateDependency(aDepType);
    CheckComRCReturnRC(rc);

    {
        if (mData->mMachineStateChangePending != 0)
        {
            /* ensureNoStateDependencies() is waiting for state dependencies to
             * drop to zero so don't add more. It may make sense to wait a bit
             * and retry before reporting an error (since the pending state
             * transition should be really quick) but let's just assert for
             * now to see if it ever happens on practice. */

            AssertFailed();

            return setError(E_ACCESSDENIED,
                            tr("Machine state change is in progress. Please retry the operation later."));
        }

        ++mData->mMachineStateDeps;
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
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this);

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
HRESULT Machine::checkStateDependency(StateDependency aDepType)
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
                return setError(VBOX_E_INVALID_VM_STATE,
                                tr("The machine is not mutable (state is %s)"),
                                Global::stringifyMachineState(mData->mMachineState));
            break;
        }
        case MutableOrSavedStateDep:
        {
            if (mData->mRegistered &&
                (mType != IsSessionMachine ||
                 mData->mMachineState > MachineState_Paused))
                return setError(VBOX_E_INVALID_VM_STATE,
                                tr("The machine is not mutable (state is %s)"),
                                Global::stringifyMachineState(mData->mMachineState));
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
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());
    AssertComRCReturn (autoCaller.state() == InInit ||
                       autoCaller.state() == Limited, E_FAIL);

    AssertReturn(!mData->mAccessible, E_FAIL);

    /* allocate data structures */
    mSSData.allocate();
    mUserData.allocate();
    mHWData.allocate();
    mMediaData.allocate();
    mStorageControllers.allocate();

    /* initialize mOSTypeId */
    mUserData->mOSTypeId = mParent->getUnknownOSType()->id();

    /* create associated BIOS settings object */
    unconst(mBIOSSettings).createObject();
    mBIOSSettings->init (this);

#ifdef VBOX_WITH_VRDP
    /* create an associated VRDPServer object (default is disabled) */
    unconst(mVRDPServer).createObject();
    mVRDPServer->init (this);
#endif

    /* create associated serial port objects */
    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
    {
        unconst(mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->init (this, slot);
    }

    /* create associated parallel port objects */
    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
    {
        unconst(mParallelPorts [slot]).createObject();
        mParallelPorts [slot]->init (this, slot);
    }

    /* create the audio adapter object (always present, default is disabled) */
    unconst(mAudioAdapter).createObject();
    mAudioAdapter->init (this);

    /* create the USB controller object (always present, default is disabled) */
    unconst(mUSBController).createObject();
    mUSBController->init (this);

    /* create associated network adapter objects */
    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
    {
        unconst(mNetworkAdapters [slot]).createObject();
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
    AutoCaller autoCaller(this);
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
            unconst(mNetworkAdapters [slot]).setNull();
        }
    }

    if (mUSBController)
    {
        mUSBController->uninit();
        unconst(mUSBController).setNull();
    }

    if (mAudioAdapter)
    {
        mAudioAdapter->uninit();
        unconst(mAudioAdapter).setNull();
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
    {
        if (mParallelPorts [slot])
        {
            mParallelPorts [slot]->uninit();
            unconst(mParallelPorts [slot]).setNull();
        }
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
    {
        if (mSerialPorts [slot])
        {
            mSerialPorts [slot]->uninit();
            unconst(mSerialPorts [slot]).setNull();
        }
    }

#ifdef VBOX_WITH_VRDP
    if (mVRDPServer)
    {
        mVRDPServer->uninit();
        unconst(mVRDPServer).setNull();
    }
#endif

    if (mBIOSSettings)
    {
        mBIOSSettings->uninit();
        unconst(mBIOSSettings).setNull();
    }

    /* Deassociate hard disks (only when a real Machine or a SnapshotMachine
     * instance is uninitialized; SessionMachine instances refer to real
     * Machine hard disks). This is necessary for a clean re-initialization of
     * the VM after successfully re-checking the accessibility state. Note
     * that in case of normal Machine or SnapshotMachine uninitialization (as
     * a result of unregistering or discarding the snapshot), outdated hard
     * disk attachments will already be uninitialized and deleted, so this
     * code will not affect them. */
    if (!!mMediaData && (mType == IsMachine || mType == IsSnapshotMachine))
    {
        for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
             it != mMediaData->mAttachments.end();
             ++it)
        {
            ComObjPtr<Medium> hd = (*it)->medium();
            if (hd.isNull() || (*it)->type() != DeviceType_HardDisk)
                continue;
            HRESULT rc = hd->detachFrom(mData->mUuid, snapshotId());
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
    mMediaData.free();
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

    AutoWriteLock alock(this);

    /* Wait for all state dependants if necessary */
    if (mData->mMachineStateDeps != 0)
    {
        /* lazy semaphore creation */
        if (mData->mMachineStateDepsSem == NIL_RTSEMEVENTMULTI)
            RTSemEventMultiCreate (&mData->mMachineStateDepsSem);

        LogFlowThisFunc(("Waiting for state deps (%d) to drop to zero...\n",
                          mData->mMachineStateDeps));

        ++mData->mMachineStateChangePending;

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
    LogFlowThisFunc(("aMachineState=%d\n", aMachineState));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    /* wait for state dependants to drop to zero */
    ensureNoStateDependencies();

    if (mData->mMachineState != aMachineState)
    {
        mData->mMachineState = aMachineState;

        RTTimeNow (&mData->mLastStateChange);

        mParent->onMachineStateChange(mData->mUuid, aMachineState);
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
                                   ComObjPtr<SharedFolder> &aSharedFolder,
                                   bool aSetError /* = false */)
{
    bool found = false;
    for (HWData::SharedFolderList::const_iterator it = mHWData->mSharedFolders.begin();
        !found && it != mHWData->mSharedFolders.end();
        ++it)
    {
        AutoWriteLock alock(*it);
        found = (*it)->name() == aName;
        if (found)
            aSharedFolder = *it;
    }

    HRESULT rc = found ? S_OK : VBOX_E_OBJECT_NOT_FOUND;

    if (aSetError && !found)
        setError(rc, tr("Could not find a shared folder named '%ls'"), aName);

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
HRESULT Machine::loadSettings(bool aRegistered)
{
    LogFlowThisFuncEnter();
    AssertReturn(mType == IsMachine, E_FAIL);

    AutoCaller autoCaller(this);
    AssertReturn(autoCaller.state() == InInit, E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        Assert(mData->m_pMachineConfigFile == NULL);

        // load and parse machine XML; this will throw on XML or logic errors
        mData->m_pMachineConfigFile = new settings::MachineConfigFile(&mData->m_strConfigFileFull);

        /* If the stored UUID is not empty, it means the registered machine
         * is being loaded. Compare the loaded UUID with the stored one taken
         * from the global registry. */
        if (!mData->mUuid.isEmpty())
        {
            if (mData->mUuid != mData->m_pMachineConfigFile->uuid)
            {
                throw setError(E_FAIL,
                               tr("Machine UUID {%RTuuid} in '%s' doesn't match its UUID {%s} in the registry file '%s'"),
                               mData->m_pMachineConfigFile->uuid.raw(),
                               mData->m_strConfigFileFull.raw(),
                               mData->mUuid.toString().raw(),
                               mParent->settingsFilePath().raw());
            }
        }
        else
            unconst (mData->mUuid) = mData->m_pMachineConfigFile->uuid;

        /* name (required) */
        mUserData->mName = mData->m_pMachineConfigFile->strName;

        /* nameSync (optional, default is true) */
        mUserData->mNameSync = mData->m_pMachineConfigFile->fNameSync;

        mUserData->mDescription = mData->m_pMachineConfigFile->strDescription;

        // guest OS type
        mUserData->mOSTypeId = mData->m_pMachineConfigFile->strOsType;
        /* look up the object by Id to check it is valid */
        ComPtr<IGuestOSType> guestOSType;
        rc = mParent->GetGuestOSType(mUserData->mOSTypeId,
                                     guestOSType.asOutParam());
        CheckComRCThrowRC(rc);

        // stateFile (optional)
        if (mData->m_pMachineConfigFile->strStateFile.isEmpty())
            mSSData->mStateFilePath.setNull();
        else
        {
            Utf8Str stateFilePathFull(mData->m_pMachineConfigFile->strStateFile);
            int vrc = calculateFullPath(stateFilePathFull, stateFilePathFull);
            if (RT_FAILURE(vrc))
                throw setError(E_FAIL,
                                tr("Invalid saved state file path '%s' (%Rrc)"),
                                mData->m_pMachineConfigFile->strStateFile.raw(),
                                vrc);
            mSSData->mStateFilePath = stateFilePathFull;
        }

        /* snapshotFolder (optional) */
        rc = COMSETTER(SnapshotFolder)(Bstr(mData->m_pMachineConfigFile->strSnapshotFolder));
        CheckComRCThrowRC(rc);

        /* currentStateModified (optional, default is true) */
        mData->mCurrentStateModified = mData->m_pMachineConfigFile->fCurrentStateModified;

        mData->mLastStateChange = mData->m_pMachineConfigFile->timeLastStateChange;

        /* teleportation */
        mUserData->mTeleporterEnabled  = mData->m_pMachineConfigFile->fTeleporterEnabled;
        mUserData->mTeleporterPort     = mData->m_pMachineConfigFile->uTeleporterPort;
        mUserData->mTeleporterAddress  = mData->m_pMachineConfigFile->strTeleporterAddress;
        mUserData->mTeleporterPassword = mData->m_pMachineConfigFile->strTeleporterPassword;

        /*
         *  note: all mUserData members must be assigned prior this point because
         *  we need to commit changes in order to let mUserData be shared by all
         *  snapshot machine instances.
         */
        mUserData.commitCopy();

        /* Snapshot node (optional) */
        if (mData->m_pMachineConfigFile->llFirstSnapshot.size())
        {
            // there can only be one root snapshot
            Assert(mData->m_pMachineConfigFile->llFirstSnapshot.size() == 1);

            settings::Snapshot &snap = mData->m_pMachineConfigFile->llFirstSnapshot.front();

            rc = loadSnapshot(snap,
                              mData->m_pMachineConfigFile->uuidCurrentSnapshot,
                              NULL);        // no parent == first snapshot
            CheckComRCThrowRC(rc);
        }

        /* Hardware node (required) */
        rc = loadHardware(mData->m_pMachineConfigFile->hardwareMachine);
        CheckComRCThrowRC(rc);

        /* Load storage controllers */
        rc = loadStorageControllers(mData->m_pMachineConfigFile->storageMachine, aRegistered);
        CheckComRCThrowRC(rc);

        /*
         *  NOTE: the assignment below must be the last thing to do,
         *  otherwise it will be not possible to change the settings
         *  somewehere in the code above because all setters will be
         *  blocked by checkStateDependency(MutableStateDep).
         */

        /* set the machine state to Aborted or Saved when appropriate */
        if (mData->m_pMachineConfigFile->fAborted)
        {
            Assert(!mSSData->mStateFilePath);
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
HRESULT Machine::loadSnapshot(const settings::Snapshot &data,
                              const Guid &aCurSnapshotId,
                              Snapshot *aParentSnapshot)
{
    AssertReturn (mType == IsMachine, E_FAIL);

    HRESULT rc = S_OK;

    Utf8Str strStateFile;
    if (!data.strStateFile.isEmpty())
    {
        /* optional */
        strStateFile = data.strStateFile;
        int vrc = calculateFullPath(strStateFile, strStateFile);
        if (RT_FAILURE(vrc))
            return setError(E_FAIL,
                            tr("Invalid saved state file path '%s' (%Rrc)"),
                            strStateFile.raw(),
                            vrc);
    }

    /* create a snapshot machine object */
    ComObjPtr<SnapshotMachine> pSnapshotMachine;
    pSnapshotMachine.createObject();
    rc = pSnapshotMachine->init(this,
                                data.hardware,
                                data.storage,
                                data.uuid,
                                strStateFile);
    CheckComRCReturnRC (rc);

    /* create a snapshot object */
    ComObjPtr<Snapshot> pSnapshot;
    pSnapshot.createObject();
    /* initialize the snapshot */
    rc = pSnapshot->init(mParent, // VirtualBox object
                         data.uuid,
                         data.strName,
                         data.strDescription,
                         data.timestamp,
                         pSnapshotMachine,
                         aParentSnapshot);
    CheckComRCReturnRC (rc);

    /* memorize the first snapshot if necessary */
    if (!mData->mFirstSnapshot)
        mData->mFirstSnapshot = pSnapshot;

    /* memorize the current snapshot when appropriate */
    if (    !mData->mCurrentSnapshot
         && pSnapshot->getId() == aCurSnapshotId
       )
        mData->mCurrentSnapshot = pSnapshot;

    // now create the children
    for (settings::SnapshotsList::const_iterator it = data.llChildSnapshots.begin();
         it != data.llChildSnapshots.end();
         ++it)
    {
        const settings::Snapshot &childData = *it;
        // recurse
        rc = loadSnapshot(childData,
                          aCurSnapshotId,
                          pSnapshot);       // parent = the one we created above
        CheckComRCBreakRC(rc);
    }

    return rc;
}

/**
 *  @param aNode    <Hardware> node.
 */
HRESULT Machine::loadHardware(const settings::Hardware &data)
{
    AssertReturn(mType == IsMachine || mType == IsSnapshotMachine, E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        /* The hardware version attribute (optional). */
        mHWData->mHWVersion = data.strVersion;
        mHWData->mHardwareUUID = data.uuid;

        mHWData->mHWVirtExEnabled             = data.fHardwareVirt;
        mHWData->mHWVirtExExclusive           = data.fHardwareVirtExclusive;
        mHWData->mHWVirtExNestedPagingEnabled = data.fNestedPaging;
        mHWData->mHWVirtExVPIDEnabled         = data.fVPID;
        mHWData->mPAEEnabled                  = data.fPAE;
        mHWData->mSyntheticCpu                = data.fSyntheticCpu;

        mHWData->mCPUCount = data.cCPUs;

        mHWData->mMemorySize = data.ulMemorySizeMB;

        // boot order
        for (size_t i = 0;
             i < RT_ELEMENTS(mHWData->mBootOrder);
             i++)
        {
            settings::BootOrderMap::const_iterator it = data.mapBootOrder.find(i);
            if (it == data.mapBootOrder.end())
                mHWData->mBootOrder[i] = DeviceType_Null;
            else
                mHWData->mBootOrder[i] = it->second;
        }

        mHWData->mVRAMSize      = data.ulVRAMSizeMB;
        mHWData->mMonitorCount  = data.cMonitors;
        mHWData->mAccelerate3DEnabled = data.fAccelerate3D;
        mHWData->mAccelerate2DVideoEnabled = data.fAccelerate2DVideo;
        mHWData->mFirmwareType = data.firmwareType;

#ifdef VBOX_WITH_VRDP
        /* RemoteDisplay */
        rc = mVRDPServer->loadSettings(data.vrdpSettings);
        CheckComRCReturnRC (rc);
#endif

        /* BIOS */
        rc = mBIOSSettings->loadSettings(data.biosSettings);
        CheckComRCReturnRC (rc);

        /* USB Controller */
        rc = mUSBController->loadSettings(data.usbController);
        CheckComRCReturnRC (rc);

        // network adapters
        for (settings::NetworkAdaptersList::const_iterator it = data.llNetworkAdapters.begin();
            it != data.llNetworkAdapters.end();
            ++it)
        {
            const settings::NetworkAdapter &nic = *it;

            /* slot unicity is guaranteed by XML Schema */
            AssertBreak(nic.ulSlot < RT_ELEMENTS(mNetworkAdapters));
            rc = mNetworkAdapters[nic.ulSlot]->loadSettings(nic);
            CheckComRCReturnRC (rc);
        }

        // serial ports
        for (settings::SerialPortsList::const_iterator it = data.llSerialPorts.begin();
            it != data.llSerialPorts.end();
            ++it)
        {
            const settings::SerialPort &s = *it;

            AssertBreak(s.ulSlot < RT_ELEMENTS(mSerialPorts));
            rc = mSerialPorts[s.ulSlot]->loadSettings(s);
            CheckComRCReturnRC (rc);
        }

        // parallel ports (optional)
        for (settings::ParallelPortsList::const_iterator it = data.llParallelPorts.begin();
            it != data.llParallelPorts.end();
            ++it)
        {
            const settings::ParallelPort &p = *it;

            AssertBreak(p.ulSlot < RT_ELEMENTS(mParallelPorts));
            rc = mParallelPorts[p.ulSlot]->loadSettings(p);
            CheckComRCReturnRC (rc);
        }

        /* AudioAdapter */
        rc = mAudioAdapter->loadSettings(data.audioAdapter);
        CheckComRCReturnRC (rc);

        for (settings::SharedFoldersList::const_iterator it = data.llSharedFolders.begin();
             it != data.llSharedFolders.end();
             ++it)
        {
            const settings::SharedFolder &sf = *it;
            rc = CreateSharedFolder(Bstr(sf.strName), Bstr(sf.strHostPath), sf.fWritable);
            CheckComRCReturnRC (rc);
        }

        // Clipboard
        mHWData->mClipboardMode = data.clipboardMode;

        // guest settings
        mHWData->mMemoryBalloonSize = data.ulMemoryBalloonSize;
        mHWData->mStatisticsUpdateInterval = data.ulStatisticsUpdateInterval;

#ifdef VBOX_WITH_GUEST_PROPS
        /* Guest properties (optional) */
        for (settings::GuestPropertiesList::const_iterator it = data.llGuestProperties.begin();
            it != data.llGuestProperties.end();
            ++it)
        {
            const settings::GuestProperty &prop = *it;
            uint32_t fFlags = guestProp::NILFLAG;
            guestProp::validateFlags(prop.strFlags.c_str(), &fFlags);
            HWData::GuestProperty property = { prop.strName, prop.strValue, prop.timestamp, fFlags };
            mHWData->mGuestProperties.push_back(property);
        }

        mHWData->mPropertyServiceActive = false;
        mHWData->mGuestPropertyNotificationPatterns = data.strNotificationPatterns;
#endif /* VBOX_WITH_GUEST_PROPS defined */
    }
    catch(std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }

    AssertComRC(rc);
    return rc;
}

  /**
   *  @param aNode    <StorageControllers> node.
   */
HRESULT Machine::loadStorageControllers(const settings::Storage &data,
                                        bool aRegistered,
                                        const Guid *aSnapshotId /* = NULL */)
{
    AssertReturn (mType == IsMachine || mType == IsSnapshotMachine, E_FAIL);

    HRESULT rc = S_OK;

    /* Make sure the attached hard disks don't get unregistered until we
     * associate them with tis machine (important for VMs loaded (opened) after
     * VirtualBox startup) */
    AutoReadLock vboxLock(mParent);

    for (settings::StorageControllersList::const_iterator it = data.llStorageControllers.begin();
         it != data.llStorageControllers.end();
         ++it)
    {
        const settings::StorageController &ctlData = *it;

        ComObjPtr<StorageController> pCtl;
        /* Try to find one with the name first. */
        rc = getStorageControllerByName(ctlData.strName, pCtl, false /* aSetError */);
        if (SUCCEEDED(rc))
            return setError(VBOX_E_OBJECT_IN_USE,
                            tr("Storage controller named '%s' already exists"),
                            ctlData.strName.raw());

        pCtl.createObject();
        rc = pCtl->init(this,
                        ctlData.strName,
                        ctlData.storageBus);
        CheckComRCReturnRC (rc);

        mStorageControllers->push_back(pCtl);

        rc = pCtl->COMSETTER(ControllerType)(ctlData.controllerType);
        CheckComRCReturnRC (rc);

        rc = pCtl->COMSETTER(PortCount)(ctlData.ulPortCount);
        CheckComRCReturnRC (rc);

        /* Set IDE emulation settings (only for AHCI controller). */
        if (ctlData.controllerType == StorageControllerType_IntelAhci)
        {
            if (    (FAILED(rc = pCtl->SetIDEEmulationPort(0, ctlData.lIDE0MasterEmulationPort)))
                 || (FAILED(rc = pCtl->SetIDEEmulationPort(1, ctlData.lIDE0SlaveEmulationPort)))
                 || (FAILED(rc = pCtl->SetIDEEmulationPort(2, ctlData.lIDE1MasterEmulationPort)))
                 || (FAILED(rc = pCtl->SetIDEEmulationPort(3, ctlData.lIDE1SlaveEmulationPort)))
               )
                return rc;
        }

        /* Load the attached devices now. */
        rc = loadStorageDevices(pCtl,
                                ctlData,
                                aRegistered,
                                aSnapshotId);
        CheckComRCReturnRC (rc);
    }

    return S_OK;
}

/**
 * @param aNode        <HardDiskAttachments> node.
 * @param aRegistered  true when the machine is being loaded on VirtualBox
 *                     startup, or when a snapshot is being loaded (wchich
 *                     currently can happen on startup only)
 * @param aSnapshotId  pointer to the snapshot ID if this is a snapshot machine
 *
 * @note Lock mParent  for reading and hard disks for writing before calling.
 */
HRESULT Machine::loadStorageDevices(StorageController *aStorageController,
                                    const settings::StorageController &data,
                                    bool aRegistered,
                                    const Guid *aSnapshotId /*= NULL*/)
{
    AssertReturn ((mType == IsMachine && aSnapshotId == NULL) ||
                  (mType == IsSnapshotMachine && aSnapshotId != NULL), E_FAIL);

    HRESULT rc = S_OK;

    if (!aRegistered && data.llAttachedDevices.size() > 0)
        /* when the machine is being loaded (opened) from a file, it cannot
         * have hard disks attached (this should not happen normally,
         * because we don't allow to attach hard disks to an unregistered
         * VM at all */
        return setError(E_FAIL,
                        tr("Unregistered machine '%ls' cannot have storage devices attached (found %d attachments)"),
                        mUserData->mName.raw(),
                        data.llAttachedDevices.size());

    /* paranoia: detect duplicate attachments */
    for (settings::AttachedDevicesList::const_iterator it = data.llAttachedDevices.begin();
         it != data.llAttachedDevices.end();
         ++it)
    {
        for (settings::AttachedDevicesList::const_iterator it2 = it;
             it2 != data.llAttachedDevices.end();
             ++it2)
        {
            if (it == it2)
                continue;

            if (   (*it).lPort == (*it2).lPort
                && (*it).lDevice == (*it2).lDevice)
            {
                return setError(E_FAIL,
                                tr("Duplicate attachments for storage controller '%s', port %d, device %d of the virtual machine '%ls'"),
                                aStorageController->name().raw(), (*it).lPort, (*it).lDevice, mUserData->mName.raw());
            }
        }
    }

    for (settings::AttachedDevicesList::const_iterator it = data.llAttachedDevices.begin();
         it != data.llAttachedDevices.end();
         ++it)
    {
        const settings::AttachedDevice &dev = *it;
        ComObjPtr<Medium> medium;

        switch (dev.deviceType)
        {
            case DeviceType_Floppy:
                /* find a floppy by UUID */
                if (!dev.uuid.isEmpty())
                    rc = mParent->findFloppyImage(&dev.uuid, NULL, true /* aDoSetError */, &medium);
                /* find a floppy by host device name */
                else if (!dev.strHostDriveSrc.isEmpty())
                {
                    SafeIfaceArray<IMedium> drivevec;
                    rc = mParent->host()->COMGETTER(FloppyDrives)(ComSafeArrayAsOutParam(drivevec));
                    if (SUCCEEDED(rc))
                    {
                        for (size_t i = 0; i < drivevec.size(); ++i)
                        {
                            Bstr hostDriveSrc(dev.strHostDriveSrc);
                            /// @todo eliminate this conversion
                            ComObjPtr<Medium> med = (Medium *)drivevec[i];
                            if (    hostDriveSrc == med->name()
                                ||  hostDriveSrc == med->location())
                            {
                                medium = med;
                                break;
                            }
                        }
                    }
                }
                break;

            case DeviceType_DVD:
                /* find a DVD by UUID */
                if (!dev.uuid.isEmpty())
                    rc = mParent->findDVDImage(&dev.uuid, NULL, true /* aDoSetError */, &medium);
                /* find a DVD by host device name */
                else if (!dev.strHostDriveSrc.isEmpty())
                {
                    SafeIfaceArray<IMedium> drivevec;
                    rc = mParent->host()->COMGETTER(DVDDrives)(ComSafeArrayAsOutParam(drivevec));
                    if (SUCCEEDED(rc))
                    {
                        for (size_t i = 0; i < drivevec.size(); ++i)
                        {
                            Bstr hostDriveSrc(dev.strHostDriveSrc);
                            /// @todo eliminate this conversion
                            ComObjPtr<Medium> med = (Medium *)drivevec[i];
                            if (    hostDriveSrc == med->name()
                                ||  hostDriveSrc == med->location())
                            {
                                medium = med;
                                break;
                            }
                        }
                    }
                }
                break;

            case DeviceType_HardDisk:
            {
                /* find a hard disk by UUID */
                rc = mParent->findHardDisk(&dev.uuid, NULL, true /* aDoSetError */, &medium);
                CheckComRCReturnRC(rc);

                AutoWriteLock hdLock(medium);

                if (medium->type() == MediumType_Immutable)
                {
                    if (mType == IsSnapshotMachine)
                        return setError(E_FAIL,
                                        tr("Immutable hard disk '%ls' with UUID {%RTuuid} cannot be directly attached to snapshot with UUID {%RTuuid} "
                                           "of the virtual machine '%ls' ('%s')"),
                                        medium->locationFull().raw(),
                                        dev.uuid.raw(),
                                        aSnapshotId->raw(),
                                        mUserData->mName.raw(),
                                        mData->m_strConfigFileFull.raw());

                    return setError(E_FAIL,
                                    tr("Immutable hard disk '%ls' with UUID {%RTuuid} cannot be directly attached to the virtual machine '%ls' ('%s')"),
                                    medium->locationFull().raw(),
                                    dev.uuid.raw(),
                                    mUserData->mName.raw(),
                                    mData->m_strConfigFileFull.raw());
                }

                if (    mType != IsSnapshotMachine
                     && medium->children().size() != 0
                   )
                    return setError(E_FAIL,
                                    tr("Hard disk '%ls' with UUID {%RTuuid} cannot be directly attached to the virtual machine '%ls' ('%s') "
                                       "because it has %d differencing child hard disks"),
                                    medium->locationFull().raw(),
                                    dev.uuid.raw(),
                                    mUserData->mName.raw(),
                                    mData->m_strConfigFileFull.raw(),
                                    medium->children().size());

                if (findAttachment(mMediaData->mAttachments,
                                   medium))
                    return setError(E_FAIL,
                                    tr("Hard disk '%ls' with UUID {%RTuuid} is already attached to the virtual machine '%ls' ('%s')"),
                                    medium->locationFull().raw(),
                                    dev.uuid.raw(),
                                    mUserData->mName.raw(),
                                    mData->m_strConfigFileFull.raw());

                break;
            }

            default:
                return setError(E_FAIL,
                                tr("Device with unknown type is attached to the virtual machine '%ls' ('%s')"),
                                medium->locationFull().raw(),
                                mUserData->mName.raw(),
                                mData->m_strConfigFileFull.raw());
        }

        if (rc)
            break;

        ComObjPtr<MediumAttachment> pAttachment;
        pAttachment.createObject();
        rc = pAttachment->init(this,
                               medium,
                               aStorageController->name(),
                               dev.lPort,
                               dev.lDevice,
                               dev.deviceType);
        CheckComRCBreakRC(rc);

        /* associate the medium with this machine and snapshot */
        if (!medium.isNull())
        {
            if (mType == IsSnapshotMachine)
                rc = medium->attachTo(mData->mUuid, *aSnapshotId);
            else
                rc = medium->attachTo(mData->mUuid);
        }
        AssertComRCBreakRC (rc);

        /* backup mMediaData to let registeredInit() properly rollback on failure
         * (= limited accessibility) */

        mMediaData.backup();
        mMediaData->mAttachments.push_back(pAttachment);
    }

    return rc;
}

/**
 *  Returns the snapshot with the given UUID or fails of no such snapshot exists.
 *
 *  @param aId          snapshot UUID to find (empty UUID refers the first snapshot)
 *  @param aSnapshot    where to return the found snapshot
 *  @param aSetError    true to set extended error info on failure
 */
HRESULT Machine::findSnapshot(const Guid &aId,
                              ComObjPtr<Snapshot> &aSnapshot,
                              bool aSetError /* = false */)
{
    AutoReadLock chlock(snapshotsTreeLockHandle());

    if (!mData->mFirstSnapshot)
    {
        if (aSetError)
            return setError(E_FAIL,
                            tr("This machine does not have any snapshots"));
        return E_FAIL;
    }

    if (aId.isEmpty())
        aSnapshot = mData->mFirstSnapshot;
    else
        aSnapshot = mData->mFirstSnapshot->findChildOrSelf(aId);

    if (!aSnapshot)
    {
        if (aSetError)
            return setError(E_FAIL,
                            tr("Could not find a snapshot with UUID {%s}"),
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
HRESULT Machine::findSnapshot(IN_BSTR aName,
                              ComObjPtr<Snapshot> &aSnapshot,
                              bool aSetError /* = false */)
{
    AssertReturn(aName, E_INVALIDARG);

    AutoReadLock chlock(snapshotsTreeLockHandle());

    if (!mData->mFirstSnapshot)
    {
        if (aSetError)
            return setError(VBOX_E_OBJECT_NOT_FOUND,
                            tr("This machine does not have any snapshots"));
        return VBOX_E_OBJECT_NOT_FOUND;
    }

    aSnapshot = mData->mFirstSnapshot->findChildOrSelf (aName);

    if (!aSnapshot)
    {
        if (aSetError)
            return setError(VBOX_E_OBJECT_NOT_FOUND,
                            tr("Could not find a snapshot named '%ls'"), aName);
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
HRESULT Machine::getStorageControllerByName(const Utf8Str &aName,
                                            ComObjPtr<StorageController> &aStorageController,
                                            bool aSetError /* = false */)
{
    AssertReturn (!aName.isEmpty(), E_INVALIDARG);

    for (StorageControllerList::const_iterator it = mStorageControllers->begin();
         it != mStorageControllers->end();
         ++it)
    {
        if ((*it)->name() == aName)
        {
            aStorageController = (*it);
            return S_OK;
        }
    }

    if (aSetError)
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("Could not find a storage controller named '%s'"),
                        aName.raw());
    return VBOX_E_OBJECT_NOT_FOUND;
}

HRESULT Machine::getMediumAttachmentsOfController(CBSTR aName,
                                                  MediaData::AttachmentList &atts)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    for (MediaData::AttachmentList::iterator it = mMediaData->mAttachments.begin();
         it != mMediaData->mAttachments.end();
         ++it)
    {
        if (Bstr((*it)->controllerName()) == aName)
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
HRESULT Machine::prepareSaveSettings(bool &aRenamed,
                                     bool &aNew)
{
    /* Note: tecnhically, mParent needs to be locked only when the machine is
     * registered (see prepareSaveSettings() for details) but we don't
     * currently differentiate it in callers of saveSettings() so we don't
     * make difference here too.  */
    AssertReturn(mParent->isWriteLockOnCurrentThread(), E_FAIL);
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    HRESULT rc = S_OK;

    aRenamed = false;

    /* if we're ready and isConfigLocked() is FALSE then it means
     * that no config file exists yet (we will create a virgin one) */
    aNew = !mData->m_pMachineConfigFile->fileExists();

    /* attempt to rename the settings file if machine name is changed */
    if (    mUserData->mNameSync
         && mUserData.isBackedUp()
         && mUserData.backedUpData()->mName != mUserData->mName
       )
    {
        aRenamed = true;

        bool dirRenamed = false;
        bool fileRenamed = false;

        Utf8Str configFile, newConfigFile;
        Utf8Str configDir, newConfigDir;

        do
        {
            int vrc = VINF_SUCCESS;

            Utf8Str name = mUserData.backedUpData()->mName;
            Utf8Str newName = mUserData->mName;

            configFile = mData->m_strConfigFileFull;

            /* first, rename the directory if it matches the machine name */
            configDir = configFile;
            configDir.stripFilename();
            newConfigDir = configDir;
            if (!strcmp(RTPathFilename(configDir.c_str()), name.c_str()))
            {
                newConfigDir.stripFilename();
                newConfigDir = Utf8StrFmt ("%s%c%s",
                    newConfigDir.raw(), RTPATH_DELIMITER, newName.raw());
                /* new dir and old dir cannot be equal here because of 'if'
                 * above and because name != newName */
                Assert (configDir != newConfigDir);
                if (!aNew)
                {
                    /* perform real rename only if the machine is not new */
                    vrc = RTPathRename (configDir.raw(), newConfigDir.raw(), 0);
                    if (RT_FAILURE(vrc))
                    {
                        rc = setError(E_FAIL,
                                      tr("Could not rename the directory '%s' to '%s' to save the settings file (%Rrc)"),
                                      configDir.raw(),
                                      newConfigDir.raw(),
                                      vrc);
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
                configFile = Utf8StrFmt("%s%c%s",
                                        newConfigDir.raw(),
                                        RTPATH_DELIMITER,
                                        RTPathFilename(configFile.c_str()));
                if (!aNew)
                {
                    /* perform real rename only if the machine is not new */
                    vrc = RTFileRename (configFile.raw(), newConfigFile.raw(), 0);
                    if (RT_FAILURE(vrc))
                    {
                        rc = setError(E_FAIL,
                                      tr("Could not rename the settings file '%s' to '%s' (%Rrc)"),
                                      configFile.raw(),
                                      newConfigFile.raw(),
                                      vrc);
                        break;
                    }
                    fileRenamed = true;
                }
            }

            /* update m_strConfigFileFull amd mConfigFile */
            Utf8Str oldConfigFileFull = mData->m_strConfigFileFull;
            Utf8Str oldConfigFile = mData->m_strConfigFile;
            mData->m_strConfigFileFull = newConfigFile;
            /* try to get the relative path for mConfigFile */
            Utf8Str path = newConfigFile;
            mParent->calculateRelativePath (path, path);
            mData->m_strConfigFile = path;

            /* last, try to update the global settings with the new path */
            if (mData->mRegistered)
            {
                rc = mParent->updateSettings(configDir.c_str(), newConfigDir.c_str());
                if (FAILED(rc))
                {
                    /* revert to old values */
                    mData->m_strConfigFileFull = oldConfigFileFull;
                    mData->m_strConfigFile = oldConfigFile;
                    break;
                }
            }

            /* update the snapshot folder */
            path = mUserData->mSnapshotFolderFull;
            if (RTPathStartsWith(path.c_str(), configDir.c_str()))
            {
                path = Utf8StrFmt("%s%s", newConfigDir.raw(),
                                  path.raw() + configDir.length());
                mUserData->mSnapshotFolderFull = path;
                calculateRelativePath (path, path);
                mUserData->mSnapshotFolder = path;
            }

            /* update the saved state file path */
            path = mSSData->mStateFilePath;
            if (RTPathStartsWith(path.c_str(), configDir.c_str()))
            {
                path = Utf8StrFmt("%s%s", newConfigDir.raw(),
                                  path.raw() + configDir.length());
                mSSData->mStateFilePath = path;
            }

            /* Update saved state file paths of all online snapshots.
             * Note that saveSettings() will recognize name change
             * and will save all snapshots in this case. */
            if (mData->mFirstSnapshot)
                mData->mFirstSnapshot->updateSavedStatePaths(configDir.c_str(),
                                                             newConfigDir.c_str());
        }
        while (0);

        if (FAILED(rc))
        {
            /* silently try to rename everything back */
            if (fileRenamed)
                RTFileRename(newConfigFile.raw(), configFile.raw(), 0);
            if (dirRenamed)
                RTPathRename(newConfigDir.raw(), configDir.raw(), 0);
        }

        CheckComRCReturnRC(rc);
    }

    if (aNew)
    {
        /* create a virgin config file */
        int vrc = VINF_SUCCESS;

        /* ensure the settings directory exists */
        Utf8Str path(mData->m_strConfigFileFull);
        path.stripFilename();
        if (!RTDirExists(path.c_str()))
        {
            vrc = RTDirCreateFullPath(path.c_str(), 0777);
            if (RT_FAILURE(vrc))
            {
                return setError(E_FAIL,
                                tr("Could not create a directory '%s' to save the settings file (%Rrc)"),
                                path.raw(),
                                vrc);
            }
        }

        /* Note: open flags must correlate with RTFileOpen() in lockConfig() */
        path = Utf8Str(mData->m_strConfigFileFull);
        vrc = RTFileOpen(&mData->mHandleCfgFile, path.c_str(),
                         RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_WRITE);
        if (RT_FAILURE(vrc))
        {
            mData->mHandleCfgFile = NIL_RTFILE;
            return setError(E_FAIL,
                            tr("Could not create the settings file '%s' (%Rrc)"),
                            path.raw(),
                            vrc);
        }
        RTFileClose(mData->mHandleCfgFile);
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
HRESULT Machine::saveSettings(int aFlags /*= 0*/)
{
    LogFlowThisFuncEnter();

    /* Note: tecnhically, mParent needs to be locked only when the machine is
     * registered (see prepareSaveSettings() for details) but we don't
     * currently differentiate it in callers of saveSettings() so we don't
     * make difference here too.  */
    AssertReturn(mParent->isWriteLockOnCurrentThread(), E_FAIL);
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    /* make sure child objects are unable to modify the settings while we are
     * saving them */
    ensureNoStateDependencies();

    AssertReturn(mType == IsMachine || mType == IsSessionMachine, E_FAIL);

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
    rc = prepareSaveSettings(isRenamed, isNew);
    CheckComRCReturnRC(rc);

    try
    {
        mData->m_pMachineConfigFile->uuid = mData->mUuid;
        mData->m_pMachineConfigFile->strName = mUserData->mName;
        mData->m_pMachineConfigFile->fNameSync = !!mUserData->mNameSync;
        mData->m_pMachineConfigFile->strDescription = mUserData->mDescription;
        mData->m_pMachineConfigFile->strOsType = mUserData->mOSTypeId;

        if (    mData->mMachineState == MachineState_Saved
             || mData->mMachineState == MachineState_Restoring
           )
        {
            Assert(!mSSData->mStateFilePath.isEmpty());
            /* try to make the file name relative to the settings file dir */
            Utf8Str stateFilePath = mSSData->mStateFilePath;
            calculateRelativePath(stateFilePath, stateFilePath);

            mData->m_pMachineConfigFile->strStateFile = stateFilePath;
        }
        else
        {
            Assert(mSSData->mStateFilePath.isNull());
            mData->m_pMachineConfigFile->strStateFile.setNull();
        }

        if (mData->mCurrentSnapshot)
            mData->m_pMachineConfigFile->uuidCurrentSnapshot = mData->mCurrentSnapshot->getId();
        else
            mData->m_pMachineConfigFile->uuidCurrentSnapshot.clear();

        mData->m_pMachineConfigFile->strSnapshotFolder = mUserData->mSnapshotFolder;
        mData->m_pMachineConfigFile->fCurrentStateModified = !!currentStateModified;
        mData->m_pMachineConfigFile->timeLastStateChange = mData->mLastStateChange;
        mData->m_pMachineConfigFile->fAborted = (mData->mMachineState == MachineState_Aborted);

        mData->m_pMachineConfigFile->fTeleporterEnabled    = !!mUserData->mTeleporterEnabled;
        mData->m_pMachineConfigFile->uTeleporterPort       = mUserData->mTeleporterPort;
        mData->m_pMachineConfigFile->strTeleporterAddress  = mUserData->mTeleporterAddress;
        mData->m_pMachineConfigFile->strTeleporterPassword = mUserData->mTeleporterPassword;

        rc = saveHardware(mData->m_pMachineConfigFile->hardwareMachine);
        CheckComRCThrowRC(rc);

        rc = saveStorageControllers(mData->m_pMachineConfigFile->storageMachine);
        CheckComRCThrowRC(rc);

        // save snapshots
        rc = saveAllSnapshots();
        CheckComRCThrowRC(rc);

        // now spit it all out
        mData->m_pMachineConfigFile->write(mData->m_strConfigFileFull);
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

    if (SUCCEEDED(rc))
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
            mParent->onMachineDataChange(mData->mUuid);
    }

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

HRESULT Machine::saveAllSnapshots()
{
    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        mData->m_pMachineConfigFile->llFirstSnapshot.clear();

        if (mData->mFirstSnapshot)
        {
            settings::Snapshot snapNew;
            mData->m_pMachineConfigFile->llFirstSnapshot.push_back(snapNew);

            // get reference to the fresh copy of the snapshot on the list and
            // work on that copy directly to avoid excessive copying later
            settings::Snapshot &snap = mData->m_pMachineConfigFile->llFirstSnapshot.front();

            rc = mData->mFirstSnapshot->saveSnapshot(snap, false /*aAttrsOnly*/);
            CheckComRCThrowRC(rc);
        }

//         if (mType == IsSessionMachine)
//             mParent->onMachineDataChange(mData->mUuid);          @todo is this necessary?

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
 *  Saves the VM hardware configuration. It is assumed that the
 *  given node is empty.
 *
 *  @param aNode    <Hardware> node to save the VM hardware confguration to.
 */
HRESULT Machine::saveHardware(settings::Hardware &data)
{
    HRESULT rc = S_OK;

    try
    {
        /* The hardware version attribute (optional).
            Automatically upgrade from 1 to 2 when there is no saved state. (ugly!) */
        if (    mHWData->mHWVersion == "1"
             && mSSData->mStateFilePath.isEmpty()
           )
            mHWData->mHWVersion = "2";  /** @todo Is this safe, to update mHWVersion here? If not some other point needs to be found where this can be done. */

        data.strVersion = mHWData->mHWVersion;
        data.uuid = mHWData->mHardwareUUID;

        // CPU
        data.fHardwareVirt          = !!mHWData->mHWVirtExEnabled;
        data.fHardwareVirtExclusive = !!mHWData->mHWVirtExExclusive;
        data.fNestedPaging          = !!mHWData->mHWVirtExNestedPagingEnabled;
        data.fVPID                  = !!mHWData->mHWVirtExVPIDEnabled;
        data.fPAE                   = !!mHWData->mPAEEnabled;
        data.fSyntheticCpu          = !!mHWData->mSyntheticCpu;

        data.cCPUs = mHWData->mCPUCount;

        // memory
        data.ulMemorySizeMB = mHWData->mMemorySize;

        // firmware
        data.firmwareType = mHWData->mFirmwareType;

        // boot order
        data.mapBootOrder.clear();
        for (size_t i = 0;
             i < RT_ELEMENTS(mHWData->mBootOrder);
             ++i)
            data.mapBootOrder[i] = mHWData->mBootOrder[i];

        // display
        data.ulVRAMSizeMB = mHWData->mVRAMSize;
        data.cMonitors = mHWData->mMonitorCount;
        data.fAccelerate3D = !!mHWData->mAccelerate3DEnabled;
        data.fAccelerate2DVideo = !!mHWData->mAccelerate2DVideoEnabled;

#ifdef VBOX_WITH_VRDP
        /* VRDP settings (optional) */
        rc = mVRDPServer->saveSettings(data.vrdpSettings);
        CheckComRCThrowRC(rc);
#endif

        /* BIOS (required) */
        rc = mBIOSSettings->saveSettings(data.biosSettings);
        CheckComRCThrowRC(rc);

        /* USB Controller (required) */
        rc = mUSBController->saveSettings(data.usbController);
        CheckComRCThrowRC(rc);

        /* Network adapters (required) */
        data.llNetworkAdapters.clear();
        for (ULONG slot = 0;
             slot < RT_ELEMENTS(mNetworkAdapters);
             ++slot)
        {
            settings::NetworkAdapter nic;
            nic.ulSlot = slot;
            rc = mNetworkAdapters[slot]->saveSettings(nic);
            CheckComRCThrowRC(rc);

            data.llNetworkAdapters.push_back(nic);
        }

        /* Serial ports */
        data.llSerialPorts.clear();
        for (ULONG slot = 0;
             slot < RT_ELEMENTS(mSerialPorts);
             ++slot)
        {
            settings::SerialPort s;
            s.ulSlot = slot;
            rc = mSerialPorts[slot]->saveSettings(s);
            CheckComRCReturnRC (rc);

            data.llSerialPorts.push_back(s);
        }

        /* Parallel ports */
        data.llParallelPorts.clear();
        for (ULONG slot = 0;
             slot < RT_ELEMENTS(mParallelPorts);
             ++slot)
        {
            settings::ParallelPort p;
            p.ulSlot = slot;
            rc = mParallelPorts[slot]->saveSettings(p);
            CheckComRCReturnRC (rc);

            data.llParallelPorts.push_back(p);
        }

        /* Audio adapter */
        rc = mAudioAdapter->saveSettings(data.audioAdapter);
        CheckComRCReturnRC (rc);

        /* Shared folders */
        data.llSharedFolders.clear();
        for (HWData::SharedFolderList::const_iterator it = mHWData->mSharedFolders.begin();
            it != mHWData->mSharedFolders.end();
            ++it)
        {
            ComObjPtr<SharedFolder> pFolder = *it;
            settings::SharedFolder sf;
            sf.strName = pFolder->name();
            sf.strHostPath = pFolder->hostPath();
            sf.fWritable = !!pFolder->writable();

            data.llSharedFolders.push_back(sf);
        }

        // clipboard
        data.clipboardMode = mHWData->mClipboardMode;

        /* Guest */
        data.ulMemoryBalloonSize = mHWData->mMemoryBalloonSize;
        data.ulStatisticsUpdateInterval = mHWData->mStatisticsUpdateInterval;

        // guest properties
        data.llGuestProperties.clear();
#ifdef VBOX_WITH_GUEST_PROPS
        for (HWData::GuestPropertyList::const_iterator it = mHWData->mGuestProperties.begin();
             it != mHWData->mGuestProperties.end();
             ++it)
        {
            HWData::GuestProperty property = *it;

            settings::GuestProperty prop;
            prop.strName = property.strName;
            prop.strValue = property.strValue;
            prop.timestamp = property.mTimestamp;
            char szFlags[guestProp::MAX_FLAGS_LEN + 1];
            guestProp::writeFlags(property.mFlags, szFlags);
            prop.strFlags = szFlags;

            data.llGuestProperties.push_back(prop);
        }

        data.strNotificationPatterns = mHWData->mGuestPropertyNotificationPatterns;
#endif /* VBOX_WITH_GUEST_PROPS defined */
    }
    catch(std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }

    AssertComRC(rc);
    return rc;
}

/**
 *  Saves the storage controller configuration.
 *
 *  @param aNode    <StorageControllers> node to save the VM hardware confguration to.
 */
HRESULT Machine::saveStorageControllers(settings::Storage &data)
{
    data.llStorageControllers.clear();

    for (StorageControllerList::const_iterator it = mStorageControllers->begin();
         it != mStorageControllers->end();
         ++it)
    {
        HRESULT rc;
        ComObjPtr<StorageController> pCtl = *it;

        settings::StorageController ctl;
        ctl.strName = pCtl->name();
        ctl.controllerType = pCtl->controllerType();
        ctl.storageBus = pCtl->storageBus();

        /* Save the port count. */
        ULONG portCount;
        rc = pCtl->COMGETTER(PortCount)(&portCount);
        ComAssertComRCRet(rc, rc);
        ctl.ulPortCount = portCount;

        /* Save IDE emulation settings. */
        if (ctl.controllerType == StorageControllerType_IntelAhci)
        {
            if (    (FAILED(rc = pCtl->GetIDEEmulationPort(0, (LONG*)&ctl.lIDE0MasterEmulationPort)))
                 || (FAILED(rc = pCtl->GetIDEEmulationPort(1, (LONG*)&ctl.lIDE0SlaveEmulationPort)))
                 || (FAILED(rc = pCtl->GetIDEEmulationPort(2, (LONG*)&ctl.lIDE1MasterEmulationPort)))
                 || (FAILED(rc = pCtl->GetIDEEmulationPort(3, (LONG*)&ctl.lIDE1SlaveEmulationPort)))
               )
                ComAssertComRCRet(rc, rc);
        }

        /* save the devices now. */
        rc = saveStorageDevices(pCtl, ctl);
        ComAssertComRCRet(rc, rc);

        data.llStorageControllers.push_back(ctl);
    }

    return S_OK;
}

/**
 *  Saves the hard disk confguration.
 */
HRESULT Machine::saveStorageDevices(ComObjPtr<StorageController> aStorageController,
                                    settings::StorageController &data)
{
    using namespace settings;

    MediaData::AttachmentList atts;

    HRESULT rc = getMediumAttachmentsOfController(Bstr(aStorageController->name()), atts);
    CheckComRCReturnRC (rc);

    data.llAttachedDevices.clear();
    for (MediaData::AttachmentList::const_iterator it = atts.begin();
         it != atts.end();
         ++it)
    {
        settings::AttachedDevice dev;

        dev.deviceType = (*it)->type();
        dev.lPort = (*it)->port();
        dev.lDevice = (*it)->device();
        if (!(*it)->medium().isNull())
        {
            BOOL fHostDrive = false;
            rc = (*it)->medium()->COMGETTER(HostDrive)(&fHostDrive);
            if (FAILED(rc))
                return rc;
            if (fHostDrive)
                dev.strHostDriveSrc = (*it)->medium()->location();
            else
                dev.uuid = (*it)->medium()->id();
            dev.fPassThrough = (*it)->passthrough();
        }

        data.llAttachedDevices.push_back(dev);
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
HRESULT Machine::saveStateSettings(int aFlags)
{
    if (aFlags == 0)
        return S_OK;

    AutoCaller autoCaller (this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    /* This object's write lock is also necessary to serialize file access
     * (prevent concurrent reads and writes) */
    AutoWriteLock alock(this);

    HRESULT rc = S_OK;

    Assert(mData->m_pMachineConfigFile);

    try
    {
        if (aFlags & SaveSTS_CurStateModified)
            mData->m_pMachineConfigFile->fCurrentStateModified = true;

        if (aFlags & SaveSTS_StateFilePath)
        {
            if (mSSData->mStateFilePath)
            {
                /* try to make the file name relative to the settings file dir */
                Utf8Str stateFilePath = mSSData->mStateFilePath;
                calculateRelativePath(stateFilePath, stateFilePath);
                mData->m_pMachineConfigFile->strStateFile = stateFilePath;
            }
            else
                mData->m_pMachineConfigFile->strStateFile.setNull();
        }

        if (aFlags & SaveSTS_StateTimeStamp)
        {
            Assert(    mData->mMachineState != MachineState_Aborted
                    || mSSData->mStateFilePath.isNull());

            mData->m_pMachineConfigFile->timeLastStateChange = mData->mLastStateChange;

            mData->m_pMachineConfigFile->fAborted = (mData->mMachineState == MachineState_Aborted);
        }

        mData->m_pMachineConfigFile->write(mData->m_strConfigFileFull);
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
 * This method assumes that mMediaData contains the original hard disk attachments
 * it needs to create diffs for. On success, these attachments will be replaced
 * with the created diffs. On failure, #deleteImplicitDiffs() is implicitly
 * called to delete created diffs which will also rollback mMediaData and restore
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
HRESULT Machine::createImplicitDiffs(const Bstr &aFolder,
                                     IProgress *aProgress,
                                     ULONG aWeight,
                                     bool aOnline)
{
    AssertReturn(!aFolder.isEmpty(), E_FAIL);

    LogFlowThisFunc(("aFolder='%ls', aOnline=%d\n", aFolder.raw(), aOnline));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    /* must be in a protective state because we leave the lock below */
    AssertReturn(    mData->mMachineState == MachineState_Saving
                  || mData->mMachineState == MachineState_RestoringSnapshot
                  || mData->mMachineState == MachineState_DeletingSnapshot,
                 E_FAIL);

    HRESULT rc = S_OK;

    typedef std::list< ComObjPtr<Medium> > LockedMedia;
    LockedMedia lockedMedia;

    try
    {
        if (!aOnline)
        {
            /* lock all attached hard disks early to detect "in use"
             * situations before creating actual diffs */
            for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
                 it != mMediaData->mAttachments.end();
                 ++it)
            {
                MediumAttachment* pAtt = *it;
                if (pAtt->type() == DeviceType_HardDisk)
                {
                    Medium* pHD = pAtt->medium();
                    Assert(pHD);
                    rc = pHD->LockRead (NULL);
                    CheckComRCThrowRC(rc);
                    lockedMedia.push_back(pHD);
                }
            }
        }

        /* remember the current list (note that we don't use backup() since
         * mMediaData may be already backed up) */
        MediaData::AttachmentList atts = mMediaData->mAttachments;

        /* start from scratch */
        mMediaData->mAttachments.clear();

        /* go through remembered attachments and create diffs for normal hard
         * disks and attach them */
        for (MediaData::AttachmentList::const_iterator it = atts.begin();
             it != atts.end();
             ++it)
        {
            MediumAttachment* pAtt = *it;

            DeviceType_T devType = pAtt->type();
            Medium* medium = pAtt->medium();

            if (   devType != DeviceType_HardDisk
                || medium == NULL
                || medium->type() != MediumType_Normal)
            {
                /* copy the attachment as is */

                /** @todo the progress object created in Console::TakeSnaphot
                 * only expects operations for hard disks. Later other
                 * device types need to show up in the progress as well. */
                if (devType == DeviceType_HardDisk)
                {
                    if (medium == NULL)
                        aProgress->SetNextOperation(Bstr(tr("Skipping attachment without medium")),
                                                    aWeight);        // weight
                    else
                        aProgress->SetNextOperation(BstrFmt(tr("Skipping medium '%s'"),
                                                            medium->base()->name().raw()),
                                                    aWeight);        // weight
                }

                mMediaData->mAttachments.push_back(pAtt);
                continue;
            }

            /* need a diff */
            aProgress->SetNextOperation(BstrFmt(tr("Creating differencing hard disk for '%s'"),
                                                medium->base()->name().raw()),
                                        aWeight);        // weight

            ComObjPtr<Medium> diff;
            diff.createObject();
            rc = diff->init(mParent,
                            medium->preferredDiffFormat().raw(),
                            BstrFmt("%ls"RTPATH_SLASH_STR,
                                    mUserData->mSnapshotFolderFull.raw()).raw());
            CheckComRCThrowRC(rc);

            /* leave the lock before the potentially lengthy operation */
            alock.leave();

            rc = medium->createDiffStorageAndWait(diff,
                                                  MediumVariant_Standard,
                                                  NULL);

            // at this point, the old image is still locked for writing, but instead
            // we need the new diff image locked for writing and lock the previously
            // current one for reading only
            if (aOnline)
            {
                diff->LockWrite(NULL);
                mData->mSession.mLockedMedia.push_back(Data::Session::LockedMedia::value_type(ComPtr<IMedium>(diff), true));
                medium->UnlockWrite(NULL);
                medium->LockRead(NULL);
                mData->mSession.mLockedMedia.push_back(Data::Session::LockedMedia::value_type(ComPtr<IMedium>(medium), false));
            }

            alock.enter();

            CheckComRCThrowRC(rc);

            rc = diff->attachTo(mData->mUuid);
            AssertComRCThrowRC(rc);

            /* add a new attachment */
            ComObjPtr<MediumAttachment> attachment;
            attachment.createObject();
            rc = attachment->init(this,
                                  diff,
                                  pAtt->controllerName(),
                                  pAtt->port(),
                                  pAtt->device(),
                                  DeviceType_HardDisk,
                                  true /* aImplicit */);
            CheckComRCThrowRC(rc);

            mMediaData->mAttachments.push_back(attachment);
        }
    }
    catch (HRESULT aRC) { rc = aRC; }

    /* unlock all hard disks we locked */
    if (!aOnline)
    {
        ErrorInfoKeeper eik;

        for (LockedMedia::const_iterator it = lockedMedia.begin();
             it != lockedMedia.end();
             ++it)
        {
            HRESULT rc2 = (*it)->UnlockRead(NULL);
            AssertComRC(rc2);
        }
    }

    if (FAILED(rc))
    {
        MultiResultRef mrc (rc);

        mrc = deleteImplicitDiffs();
    }

    return rc;
}

/**
 * Deletes implicit differencing hard disks created either by
 * #createImplicitDiffs() or by #AttachMedium() and rolls back mMediaData.
 *
 * Note that to delete hard disks created by #AttachMedium() this method is
 * called from #fixupMedia() when the changes are rolled back.
 *
 * @note Locks this object for writing.
 */
HRESULT Machine::deleteImplicitDiffs()
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AssertReturn(mMediaData.isBackedUp(), E_FAIL);

    HRESULT rc = S_OK;

    MediaData::AttachmentList implicitAtts;

    const MediaData::AttachmentList &oldAtts = mMediaData.backedUpData()->mAttachments;

    /* enumerate new attachments */
    for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
         it != mMediaData->mAttachments.end();
         ++it)
    {
        ComObjPtr<Medium> hd = (*it)->medium();
        if (hd.isNull())
            continue;

        if ((*it)->isImplicit())
        {
            /* deassociate and mark for deletion */
            rc = hd->detachFrom(mData->mUuid);
            AssertComRC(rc);
            implicitAtts.push_back (*it);
            continue;
        }

        /* was this hard disk attached before? */
        if (!findAttachment(oldAtts, hd))
        {
            /* no: de-associate */
            rc = hd->detachFrom(mData->mUuid);
            AssertComRC(rc);
            continue;
        }
    }

    /* rollback hard disk changes */
    mMediaData.rollback();

    MultiResult mrc (S_OK);

    /* delete unused implicit diffs */
    if (implicitAtts.size() != 0)
    {
        /* will leave the lock before the potentially lengthy
         * operation, so protect with the special state (unless already
         * protected) */
        MachineState_T oldState = mData->mMachineState;
        if (    oldState != MachineState_Saving
             && oldState != MachineState_RestoringSnapshot
             && oldState != MachineState_DeletingSnapshot
           )
            setMachineState (MachineState_SettingUp);

        alock.leave();

        for (MediaData::AttachmentList::const_iterator it = implicitAtts.begin();
             it != implicitAtts.end();
             ++it)
        {
            ComObjPtr<Medium> hd = (*it)->medium();
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
 * Looks through the given list of media attachments for one with the given parameters
 * and returns it, or NULL if not found. The list is a parameter so that backup lists
 * can be searched as well if needed.
 *
 * @param list
 * @param aControllerName
 * @param aControllerPort
 * @param aDevice
 * @return
 */
MediumAttachment* Machine::findAttachment(const MediaData::AttachmentList &ll,
                                          IN_BSTR aControllerName,
                                          LONG aControllerPort,
                                          LONG aDevice)
{
   for (MediaData::AttachmentList::const_iterator it = ll.begin();
         it != ll.end();
         ++it)
    {
        MediumAttachment *pAttach = *it;
        if (pAttach->matches(aControllerName, aControllerPort, aDevice))
            return pAttach;
    }

    return NULL;
}

/**
 * Looks through the given list of media attachments for one with the given parameters
 * and returns it, or NULL if not found. The list is a parameter so that backup lists
 * can be searched as well if needed.
 *
 * @param list
 * @param aControllerName
 * @param aControllerPort
 * @param aDevice
 * @return
 */
MediumAttachment* Machine::findAttachment(const MediaData::AttachmentList &ll,
                                          ComObjPtr<Medium> pMedium)
{
   for (MediaData::AttachmentList::const_iterator it = ll.begin();
         it != ll.end();
         ++it)
    {
        MediumAttachment *pAttach = *it;
        ComObjPtr<Medium> pMediumThis = pAttach->medium();
        if (pMediumThis.equalsTo(pMedium))
            return pAttach;
    }

    return NULL;
}

/**
 * Looks through the given list of media attachments for one with the given parameters
 * and returns it, or NULL if not found. The list is a parameter so that backup lists
 * can be searched as well if needed.
 *
 * @param list
 * @param aControllerName
 * @param aControllerPort
 * @param aDevice
 * @return
 */
MediumAttachment* Machine::findAttachment(const MediaData::AttachmentList &ll,
                                          Guid &id)
{
   for (MediaData::AttachmentList::const_iterator it = ll.begin();
         it != ll.end();
         ++it)
    {
        MediumAttachment *pAttach = *it;
        ComObjPtr<Medium> pMediumThis = pAttach->medium();
        if (pMediumThis->id() == id)
            return pAttach;
    }

    return NULL;
}

/**
 * Perform deferred hard disk detachments on success and deletion of implicitly
 * created diffs on failure.
 *
 * Does nothing if the hard disk attachment data (mMediaData) is not changed (not
 * backed up).
 *
 * When the data is backed up, this method will commit mMediaData if @a aCommit is
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
void Machine::fixupMedia(bool aCommit, bool aOnline /*= false*/)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = S_OK;

    /* no attach/detach operations -- nothing to do */
    if (!mMediaData.isBackedUp())
        return;

    if (aCommit)
    {
        MediaData::AttachmentList &oldAtts = mMediaData.backedUpData()->mAttachments;

        /* enumerate new attachments */
        for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
             it != mMediaData->mAttachments.end();
             ++it)
        {
            MediumAttachment *pAttach = *it;

            if (pAttach->type() == DeviceType_HardDisk)
            {
                pAttach->commit();

                Medium* pMedium = pAttach->medium();

                /** @todo convert all this Machine-based voodoo to MediumAttachment
                * based commit logic. */
                if (pAttach->isImplicit())
                {
                    /* convert implicit attachment to normal */
                    pAttach->setImplicit(false);

                    if (aOnline)
                    {
                        rc = pMedium->LockWrite(NULL);
                        AssertComRC(rc);

                        mData->mSession.mLockedMedia.push_back(
                            Data::Session::LockedMedia::value_type(
                                ComPtr<IMedium>(pMedium), true));

                        /* also, relock the old hard disk which is a base for the
                        * new diff for reading if the VM is online */

                        ComObjPtr<Medium> parent = pMedium->parent();
                        /* make the relock atomic */
                        AutoWriteLock parentLock (parent);
                        rc = parent->UnlockWrite(NULL);
                        AssertComRC(rc);
                        rc = parent->LockRead(NULL);
                        AssertComRC(rc);

                        /* XXX actually we should replace the old entry in that
                        * vector (write lock => read lock) but this would take
                        * some effort. So lets just ignore the error code in
                        * SessionMachine::unlockMedia(). */
                        mData->mSession.mLockedMedia.push_back(
                            Data::Session::LockedMedia::value_type (
                                ComPtr<IMedium>(parent), false));
                    }

                    continue;
                }

                if (pMedium)
                {
                    /* was this hard disk attached before? */
                    for (MediaData::AttachmentList::iterator oldIt = oldAtts.begin();
                         oldIt != oldAtts.end();
                         ++oldIt)
                    {
                        MediumAttachment *pOldAttach = *it;
                        if (pOldAttach->medium().equalsTo(pMedium))
                        {
                            /* yes: remove from old to avoid de-association */
                            oldAtts.erase(oldIt);
                            break;
                        }
                    }
                }
            }
        }

        /* enumerate remaining old attachments and de-associate from the
         * current machine state */
        for (MediaData::AttachmentList::const_iterator it = oldAtts.begin();
             it != oldAtts.end();
             ++it)
        {
            MediumAttachment *pAttach = *it;

            if (pAttach->type() == DeviceType_HardDisk)
            {
                Medium* pMedium = pAttach->medium();

                if (pMedium)
                {
                    /* now de-associate from the current machine state */
                    rc = pMedium->detachFrom(mData->mUuid);
                    AssertComRC(rc);

                    if (aOnline)
                    {
                        /* unlock since not used anymore */
                        MediumState_T state;
                        rc = pMedium->UnlockWrite(&state);
                        /* the disk may be alredy relocked for reading above */
                        Assert (SUCCEEDED(rc) || state == MediumState_LockedRead);
                    }
                }
            }
        }

        /* commit the hard disk changes */
        mMediaData.commit();

        if (mType == IsSessionMachine)
        {
            /* attach new data to the primary machine and reshare it */
            mPeer->mMediaData.attach(mMediaData);
        }
    }
    else
    {
        /* enumerate new attachments */
        for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
             it != mMediaData->mAttachments.end();
             ++it)
        {
            (*it)->rollback();
        }

        /** @todo convert all this Machine-based voodoo to MediumAttachment
         * based rollback logic. */
        // @todo r=dj the below totally fails if this gets called from Machine::rollback(),
        // which gets called if Machine::registeredInit() fails...
        deleteImplicitDiffs();
    }

    return;
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
bool Machine::isInOwnDir(Utf8Str *aSettingsDir /* = NULL */)
{
    Utf8Str settingsDir = mData->m_strConfigFileFull;
    settingsDir.stripFilename();
    char *dirName = RTPathFilename(settingsDir.c_str());

    AssertReturn(dirName, false);

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
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock(this);

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
                ++it)
        {
            if ((*it)->isModified())
                return true;
        }
    }

    return
        mUserData.isBackedUp() ||
        mHWData.isBackedUp() ||
        mMediaData.isBackedUp() ||
        mStorageControllers.isBackedUp() ||
#ifdef VBOX_WITH_VRDP
        (mVRDPServer && mVRDPServer->isModified()) ||
#endif
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
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock(this);

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
             ++it)
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
        mMediaData.hasActualChanges() ||
        mStorageControllers.hasActualChanges() ||
#ifdef VBOX_WITH_VRDP
        (mVRDPServer && mVRDPServer->isReallyModified()) ||
#endif
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
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), (void) 0);

    AutoWriteLock alock(this);

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
                 ++rit)
            {
                for (HWData::SharedFolderList::iterator cit =
                         mHWData.backedUpData()->mSharedFolders.begin();
                     cit != mHWData.backedUpData()->mSharedFolders.end();
                     ++cit)
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
                ++it;
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

            ++it;
        }
    }

    mUserData.rollback();

    mHWData.rollback();

    if (mMediaData.isBackedUp())
        fixupMedia(false /* aCommit */);

    /* check for changes in child objects */

    bool vrdpChanged = false, usbChanged = false;

    ComPtr<INetworkAdapter> networkAdapters [RT_ELEMENTS (mNetworkAdapters)];
    ComPtr<ISerialPort> serialPorts [RT_ELEMENTS (mSerialPorts)];
    ComPtr<IParallelPort> parallelPorts [RT_ELEMENTS (mParallelPorts)];

    if (mBIOSSettings)
        mBIOSSettings->rollback();

#ifdef VBOX_WITH_VRDP
    if (mVRDPServer)
        vrdpChanged = mVRDPServer->rollback();
#endif

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

        ComObjPtr<Machine> that = this;
        alock.leave();

        if (sharedFoldersChanged)
            that->onSharedFolderChange();

        if (vrdpChanged)
            that->onVRDPServerChange();
        if (usbChanged)
            that->onUSBControllerChange();

        for (ULONG slot = 0; slot < RT_ELEMENTS (networkAdapters); slot ++)
            if (networkAdapters [slot])
                that->onNetworkAdapterChange (networkAdapters [slot], FALSE);
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
    AutoCaller autoCaller(this);
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

    if (mMediaData.isBackedUp())
        fixupMedia(true /* aCommit */);

    mBIOSSettings->commit();
#ifdef VBOX_WITH_VRDP
    mVRDPServer->commit();
#endif
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

                ++it;
            }

            /* uninit old peer's controllers that are left */
            it = mPeer->mStorageControllers->begin();
            while (it != mPeer->mStorageControllers->end())
            {
                (*it)->uninit();
                ++it;
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
            ++it;
        }
    }

    if (mType == IsSessionMachine)
    {
        /* attach new data to the primary machine and reshare it */
        mPeer->mUserData.attach (mUserData);
        mPeer->mHWData.attach (mHWData);
        /* mMediaData is reshared by fixupMedia */
        // mPeer->mMediaData.attach(mMediaData);
        Assert(mPeer->mMediaData.data() == mMediaData.data());
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
         ++it)
    {
        ComObjPtr<SharedFolder> folder;
        folder.createObject();
        HRESULT rc = folder->initCopy (machine(), *it);
        AssertComRC (rc);
        *it = folder;
    }

    mBIOSSettings->copyFrom (aThat->mBIOSSettings);
#ifdef VBOX_WITH_VRDP
    mVRDPServer->copyFrom (aThat->mVRDPServer);
#endif
    mAudioAdapter->copyFrom (aThat->mAudioAdapter);
    mUSBController->copyFrom (aThat->mUSBController);

    /* create private copies of all controllers */
    mStorageControllers.backup();
    mStorageControllers->clear();
    for (StorageControllerList::iterator it = aThat->mStorageControllers->begin();
         it != aThat->mStorageControllers->end();
         ++it)
    {
        ComObjPtr<StorageController> ctrl;
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
    pm::BaseMetric *cpuLoad = new pm::MachineCpuLoadRaw (hal, aMachine, pid,
                                                         cpuLoadUser, cpuLoadKernel);
    aCollector->registerBaseMetric (cpuLoad);
    pm::BaseMetric *ramUsage = new pm::MachineRamUsage (hal, aMachine, pid,
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

    ComObjPtr<SessionMachine> machine;
    ComObjPtr<Progress> progress;
    const MachineState_T state;

    bool subTask : 1;
};

/** Discard snapshot task */
struct SessionMachine::DeleteSnapshotTask
    : public SessionMachine::Task
{
    DeleteSnapshotTask(SessionMachine *m, Progress *p, Snapshot *s)
        : Task(m, p),
          snapshot(s)
    {}

    DeleteSnapshotTask (const Task &task, Snapshot *s)
        : Task(task)
        , snapshot(s)
    {}

    void handler()
    {
        machine->deleteSnapshotHandler(*this);
    }

    ComObjPtr<Snapshot> snapshot;
};

/** Restore snapshot state task */
struct SessionMachine::RestoreSnapshotTask
    : public SessionMachine::Task
{
    RestoreSnapshotTask(SessionMachine *m, ComObjPtr<Snapshot> &aSnapshot, Progress *p)
        : Task(m, p),
          pSnapshot(aSnapshot)
    {}

    void handler()
    {
        machine->restoreSnapshotHandler(*this);
    }

    ComObjPtr<Snapshot> pSnapshot;
};

////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(SessionMachine)

HRESULT SessionMachine::FinalConstruct()
{
    LogFlowThisFunc(("\n"));

    /* set the proper type to indicate we're the SessionMachine instance */
    unconst(mType) = IsSessionMachine;

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
    LogFlowThisFunc(("\n"));

    uninit (Uninit::Unexpected);
}

/**
 *  @note Must be called only by Machine::openSession() from its own write lock.
 */
HRESULT SessionMachine::init (Machine *aMachine)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mName={%ls}\n", aMachine->mUserData->mName.raw()));

    AssertReturn(aMachine, E_INVALIDARG);

    AssertReturn(aMachine->lockHandle()->isWriteLockOnCurrentThread(), E_FAIL);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* create the interprocess semaphore */
#if defined(RT_OS_WINDOWS)
    mIPCSemName = aMachine->mData->m_strConfigFileFull;
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
#  if defined(RT_OS_FREEBSD) && (HC_ARCH_BITS == 64)
    /** @todo Check that this still works correctly. */
    AssertCompileSize(key_t, 8);
#  else
    AssertCompileSize(key_t, 4);
#  endif
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
    Utf8Str semName = aMachine->mData->m_strConfigFileFull;
    char *pszSemName = NULL;
    RTStrUtf8ToCurrentCP (&pszSemName, semName);
    key_t key = ::ftok (pszSemName, 'V');
    RTStrFree (pszSemName);

    mIPCSem = ::semget (key, 1, S_IRWXU | S_IRWXG | S_IRWXO | IPC_CREAT);
# endif /* !VBOX_WITH_NEW_SYS_V_KEYGEN */

    int errnoSave = errno;
    if (mIPCSem < 0 && errnoSave == ENOSYS)
    {
        setError(E_FAIL,
                 tr("Cannot create IPC semaphore. Most likely your host kernel lacks "
                    "support for SysV IPC. Check the host kernel configuration for "
                    "CONFIG_SYSVIPC=y"));
        return E_FAIL;
    }
    /* ENOSPC can also be the result of VBoxSVC crashes without properly freeing
     * the IPC semaphores */
    if (mIPCSem < 0 && errnoSave == ENOSPC)
    {
#ifdef RT_OS_LINUX
        setError(E_FAIL,
                 tr("Cannot create IPC semaphore because the system limit for the "
                    "maximum number of semaphore sets (SEMMNI), or the system wide "
                    "maximum number of sempahores (SEMMNS) would be exceeded. The "
                    "current set of SysV IPC semaphores can be determined from "
                    "the file /proc/sysvipc/sem"));
#else
        setError(E_FAIL,
                 tr("Cannot create IPC semaphore because the system-imposed limit "
                    "on the maximum number of allowed  semaphores or semaphore "
                    "identifiers system-wide would be exceeded"));
#endif
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
    unconst(mPeer) = aMachine;
    /* share the parent pointer */
    unconst(mParent) = aMachine->mParent;

    /* take the pointers to data to share */
    mData.share (aMachine->mData);
    mSSData.share (aMachine->mSSData);

    mUserData.share (aMachine->mUserData);
    mHWData.share (aMachine->mHWData);
    mMediaData.share(aMachine->mMediaData);

    mStorageControllers.allocate();
    for (StorageControllerList::const_iterator it = aMachine->mStorageControllers->begin();
         it != aMachine->mStorageControllers->end();
         ++it)
    {
        ComObjPtr<StorageController> ctl;
        ctl.createObject();
        ctl->init(this, *it);
        mStorageControllers->push_back (ctl);
    }

    unconst(mBIOSSettings).createObject();
    mBIOSSettings->init (this, aMachine->mBIOSSettings);
#ifdef VBOX_WITH_VRDP
    /* create another VRDPServer object that will be mutable */
    unconst(mVRDPServer).createObject();
    mVRDPServer->init (this, aMachine->mVRDPServer);
#endif
    /* create another audio adapter object that will be mutable */
    unconst(mAudioAdapter).createObject();
    mAudioAdapter->init (this, aMachine->mAudioAdapter);
    /* create a list of serial ports that will be mutable */
    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
    {
        unconst(mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->init (this, aMachine->mSerialPorts [slot]);
    }
    /* create a list of parallel ports that will be mutable */
    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
    {
        unconst(mParallelPorts [slot]).createObject();
        mParallelPorts [slot]->init (this, aMachine->mParallelPorts [slot]);
    }
    /* create another USB controller object that will be mutable */
    unconst(mUSBController).createObject();
    mUSBController->init (this, aMachine->mUSBController);

    /* create a list of network adapters that will be mutable */
    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
    {
        unconst(mNetworkAdapters [slot]).createObject();
        mNetworkAdapters [slot]->init (this, aMachine->mNetworkAdapters [slot]);
    }

    /* default is to delete saved state on Saved -> PoweredOff transition */
    mRemoveSavedState = true;

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
    LogFlowThisFunc(("reason=%d\n", aReason));

    /*
     *  Strongly reference ourselves to prevent this object deletion after
     *  mData->mSession.mMachine.setNull() below (which can release the last
     *  reference and call the destructor). Important: this must be done before
     *  accessing any members (and before AutoUninitSpan that does it as well).
     *  This self reference will be released as the very last step on return.
     */
    ComObjPtr<SessionMachine> selfRef = this;

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
    {
        LogFlowThisFunc(("Already uninitialized\n"));
        LogFlowThisFuncLeave();
        return;
    }

    if (autoUninitSpan.initFailed())
    {
        /* We've been called by init() because it's failed. It's not really
         * necessary (nor it's safe) to perform the regular uninit sequense
         * below, the following is enough.
         */
        LogFlowThisFunc(("Initialization failed.\n"));
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
        unconst(mParent).setNull();
        unconst(mPeer).setNull();
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
    NOREF(lastState);

    if (aReason == Uninit::Abnormal)
    {
        LogWarningThisFunc(("ABNORMAL client termination! (wasBusy=%d)\n",
                             Global::IsOnlineOrTransient (lastState)));

        /* reset the state to Aborted */
        if (mData->mMachineState != MachineState_Aborted)
            setMachineState (MachineState_Aborted);
    }

    if (isModified())
    {
        LogWarningThisFunc(("Discarding unsaved settings changes!\n"));
        rollback (false /* aNotify */);
    }

    Assert (!mSnapshotData.mStateFilePath || !mSnapshotData.mSnapshot);
    if (mSnapshotData.mStateFilePath)
    {
        LogWarningThisFunc(("canceling failed save state request!\n"));
        endSavingState (FALSE /* aSuccess  */);
    }
    else if (!mSnapshotData.mSnapshot.isNull())
    {
        LogWarningThisFunc(("canceling untaken snapshot!\n"));
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
        AssertComRC(rc);
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
        LogFlowThisFunc(("Closing remote sessions (%d):\n",
                          mData->mSession.mRemoteControls.size()));

        Data::Session::RemoteControlList::iterator it =
            mData->mSession.mRemoteControls.begin();
        while (it != mData->mSession.mRemoteControls.end())
        {
            LogFlowThisFunc(("  Calling remoteControl->Uninitialize()...\n"));
            HRESULT rc = (*it)->Uninitialize();
            LogFlowThisFunc(("  remoteControl->Uninitialize() returned %08X\n", rc));
            if (FAILED (rc))
                LogWarningThisFunc(("Forgot to close the remote session?\n"));
            ++it;
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
        LogWarningThisFunc(("Unexpected SessionMachine uninitialization!\n"));

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

    /* close the interprocess semaphore before leaving the exclusive lock */
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

    /* leave the exclusive lock before setting the below two to NULL */
    alock.leave();

    unconst(mParent).setNull();
    unconst(mPeer).setNull();

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
    AssertReturn(!mPeer.isNull(), NULL);
    return mPeer->lockHandle();
}

// IInternalMachineControl methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::SetRemoveSavedState(BOOL aRemove)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    mRemoveSavedState = aRemove;

    return S_OK;
}

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
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this);

#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    mIPCSemName.cloneTo(aId);
    return S_OK;
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
# ifdef VBOX_WITH_NEW_SYS_V_KEYGEN
    mIPCKey.cloneTo(aId);
# else /* !VBOX_WITH_NEW_SYS_V_KEYGEN */
    mData->m_strConfigFileFull.cloneTo(aId);
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
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull (aUSBDevice);
    CheckComArgOutPointerValid(aMatched);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    *aMatched = mUSBController->hasMatchingFilter (aUSBDevice, aMaskedIfs);
#else
    NOREF(aUSBDevice);
    NOREF(aMaskedIfs);
    *aMatched = FALSE;
#endif

    return S_OK;
}

/**
 *  @note Locks the same as Host::captureUSBDevice() does.
 */
STDMETHODIMP SessionMachine::CaptureUSBDevice (IN_BSTR aId)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

#ifdef VBOX_WITH_USB
    /* if captureDeviceForVM() fails, it must have set extended error info */
    MultiResult rc = mParent->host()->checkUSBProxyService();
    CheckComRCReturnRC(rc);

    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn(service, E_FAIL);
    return service->captureDeviceForVM (this, Guid(aId));
#else
    NOREF(aId);
    return E_NOTIMPL;
#endif
}

/**
 *  @note Locks the same as Host::detachUSBDevice() does.
 */
STDMETHODIMP SessionMachine::DetachUSBDevice (IN_BSTR aId, BOOL aDone)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn(service, E_FAIL);
    return service->detachDeviceFromVM (this, Guid(aId), !!aDone);
#else
    NOREF(aId);
    NOREF(aDone);
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
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    HRESULT rc = mUSBController->notifyProxy (true /* aInsertFilters */);
    AssertComRC(rc);
    NOREF (rc);

    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn(service, E_FAIL);
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
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    HRESULT rc = mUSBController->notifyProxy (false /* aInsertFilters */);
    AssertComRC(rc);
    NOREF (rc);

    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn(service, E_FAIL);
    return service->detachAllDevicesFromVM (this, !!aDone, false /* aAbnormal */);
#else
    NOREF(aDone);
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

    AssertReturn(aSession, E_INVALIDARG);
    AssertReturn(aProgress, E_INVALIDARG);

    AutoCaller autoCaller(this);

    LogFlowThisFunc(("callerstate=%d\n", autoCaller.state()));
    /*
     *  We don't assert below because it might happen that a non-direct session
     *  informs us it is closed right after we've been uninitialized -- it's ok.
     */
    CheckComRCReturnRC(autoCaller.rc());

    /* get IInternalSessionControl interface */
    ComPtr<IInternalSessionControl> control (aSession);

    ComAssertRet (!control.isNull(), E_INVALIDARG);

    AutoWriteLock alock(this);

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
        LogFlowThisFunc(("Direct control is set to NULL\n"));

        /*  Create the progress object the client will use to wait until
         * #checkForDeath() is called to uninitialize this session object after
         * it releases the IPC semaphore. */
        ComObjPtr<Progress> progress;
        progress.createObject();
        progress->init (mParent, static_cast <IMachine *> (mPeer),
                        Bstr (tr ("Closing session")), FALSE /* aCancelable */);
        progress.queryInterfaceTo(aProgress);
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

    AssertReturn(aProgress, E_INVALIDARG);
    AssertReturn(aStateFilePath, E_POINTER);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AssertReturn(mData->mMachineState == MachineState_Paused &&
                  mSnapshotData.mLastState == MachineState_Null &&
                  mSnapshotData.mProgressId.isEmpty() &&
                  mSnapshotData.mStateFilePath.isNull(),
                  E_FAIL);

    /* memorize the progress ID and add it to the global collection */
    Bstr progressId;
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
    mSnapshotData.mProgressId = Guid(progressId);
    mSnapshotData.mStateFilePath = stateFilePath;

    /* set the state to Saving (this is expected by Console::SaveState()) */
    setMachineState (MachineState_Saving);

    stateFilePath.cloneTo(aStateFilePath);

    return S_OK;
}

/**
 *  @note Locks mParent + this object for writing.
 */
STDMETHODIMP SessionMachine::EndSavingState (BOOL aSuccess)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* endSavingState() need mParent lock */
    AutoMultiWriteLock2 alock (mParent, this);

    AssertReturn(mData->mMachineState == MachineState_Saving &&
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
    LogFlowThisFunc(("\n"));

    AssertReturn(aSavedStateFile, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AssertReturn(mData->mMachineState == MachineState_PoweredOff ||
                  mData->mMachineState == MachineState_Aborted,
                  E_FAIL);

    Utf8Str stateFilePathFull = aSavedStateFile;
    int vrc = calculateFullPath(stateFilePathFull, stateFilePathFull);
    if (RT_FAILURE(vrc))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Invalid saved state file path '%ls' (%Rrc)"),
                        aSavedStateFile,
                        vrc);

    mSSData->mStateFilePath = stateFilePathFull;

    /* The below setMachineState() will detect the state transition and will
     * update the settings file */

    return setMachineState (MachineState_Saved);
}

/**
 *  @note Locks mParent + this object for writing.
 */
STDMETHODIMP SessionMachine::BeginTakingSnapshot(IConsole *aInitiator,
                                                 IN_BSTR aName,
                                                 IN_BSTR aDescription,
                                                 IProgress *aConsoleProgress,
                                                 BOOL fTakingSnapshotOnline,
                                                 BSTR *aStateFilePath)
{
    LogFlowThisFuncEnter();

    AssertReturn(aInitiator && aName, E_INVALIDARG);
    AssertReturn(aStateFilePath, E_POINTER);

    LogFlowThisFunc(("aName='%ls'\n", aName));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    /* saveSettings() needs mParent lock */
    AutoMultiWriteLock2 alock(mParent, this);

    AssertReturn(    !Global::IsOnlineOrTransient(mData->mMachineState)
                  || mData->mMachineState == MachineState_Running
                  || mData->mMachineState == MachineState_Paused, E_FAIL);
    AssertReturn(mSnapshotData.mLastState == MachineState_Null, E_FAIL);
    AssertReturn(mSnapshotData.mSnapshot.isNull(), E_FAIL);

    if (    !fTakingSnapshotOnline
         && mData->mMachineState != MachineState_Saved
       )
    {
        /* save all current settings to ensure current changes are committed and
         * hard disks are fixed up */
        HRESULT rc = saveSettings();
        CheckComRCReturnRC(rc);
    }

    /* create an ID for the snapshot */
    Guid snapshotId;
    snapshotId.create();

    Utf8Str strStateFilePath;
    /* stateFilePath is null when the machine is not online nor saved */
    if (    fTakingSnapshotOnline
         || mData->mMachineState == MachineState_Saved)
    {
        strStateFilePath = Utf8StrFmt("%ls%c{%RTuuid}.sav",
                                      mUserData->mSnapshotFolderFull.raw(),
                                      RTPATH_DELIMITER,
                                      snapshotId.ptr());
        /* ensure the directory for the saved state file exists */
        HRESULT rc = VirtualBox::ensureFilePathExists(strStateFilePath);
        CheckComRCReturnRC(rc);
    }

    /* create a snapshot machine object */
    ComObjPtr<SnapshotMachine> snapshotMachine;
    snapshotMachine.createObject();
    HRESULT rc = snapshotMachine->init(this, snapshotId, strStateFilePath);
    AssertComRCReturn(rc, rc);

    /* create a snapshot object */
    RTTIMESPEC time;
    ComObjPtr<Snapshot> pSnapshot;
    pSnapshot.createObject();
    rc = pSnapshot->init(mParent,
                         snapshotId,
                         aName,
                         aDescription,
                         *RTTimeNow(&time),
                         snapshotMachine,
                         mData->mCurrentSnapshot);
    AssertComRCReturnRC(rc);

    /* fill in the snapshot data */
    mSnapshotData.mLastState = mData->mMachineState;
    mSnapshotData.mSnapshot = pSnapshot;

    try
    {
        LogFlowThisFunc(("Creating differencing hard disks (online=%d)...\n",
                        fTakingSnapshotOnline));

        // backup the media data so we can recover if things goes wrong along the day;
        // the matching commit() is in fixupMedia() during endSnapshot()
        mMediaData.backup();

        /* set the state to Saving (this is expected by Console::TakeSnapshot()) */
        setMachineState(MachineState_Saving);

        /* create new differencing hard disks and attach them to this machine */
        rc = createImplicitDiffs(mUserData->mSnapshotFolderFull,
                                 aConsoleProgress,
                                 1,            // operation weight; must be the same as in Console::TakeSnapshot()
                                 !!fTakingSnapshotOnline);

        if (SUCCEEDED(rc) && mSnapshotData.mLastState == MachineState_Saved)
        {
            Utf8Str stateFrom = mSSData->mStateFilePath;
            Utf8Str stateTo = mSnapshotData.mSnapshot->stateFilePath();

            LogFlowThisFunc(("Copying the execution state from '%s' to '%s'...\n",
                            stateFrom.raw(), stateTo.raw()));

            aConsoleProgress->SetNextOperation(Bstr(tr("Copying the execution state")),
                                               1);        // weight

            /* Leave the lock before a lengthy operation (mMachineState is
            * MachineState_Saving here) */
            alock.leave();

            /* copy the state file */
            int vrc = RTFileCopyEx(stateFrom.c_str(),
                                   stateTo.c_str(),
                                   0,
                                   progressCallback,
                                   aConsoleProgress);
            alock.enter();

            if (RT_FAILURE(vrc))
                throw setError(E_FAIL,
                               tr("Could not copy the state file '%s' to '%s' (%Rrc)"),
                               stateFrom.raw(),
                               stateTo.raw(),
                               vrc);
        }
    }
    catch (HRESULT hrc)
    {
        pSnapshot->uninit();
        pSnapshot.setNull();
        rc = hrc;
    }

    if (fTakingSnapshotOnline)
        strStateFilePath.cloneTo(aStateFilePath);
    else
        *aStateFilePath = NULL;

    LogFlowThisFuncLeave();
    return rc;
}

/**
 * @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::EndTakingSnapshot(BOOL aSuccess)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AssertReturn(!aSuccess ||
                  (mData->mMachineState == MachineState_Saving &&
                   mSnapshotData.mLastState != MachineState_Null &&
                   !mSnapshotData.mSnapshot.isNull()),
                  E_FAIL);

    /*
     * Restore the state we had when BeginTakingSnapshot() was called,
     * Console::fntTakeSnapshotWorker restores its local copy when we return.
     * If the state was Running, then let Console::fntTakeSnapshotWorker it
     * all via Console::Resume().
     */
    if (   mData->mMachineState != mSnapshotData.mLastState
        && mSnapshotData.mLastState != MachineState_Running)
        setMachineState(mSnapshotData.mLastState);

    return endTakingSnapshot(aSuccess);
}

/**
 *  @note Locks mParent + this + children objects for writing!
 */
STDMETHODIMP SessionMachine::DeleteSnapshot(IConsole *aInitiator,
                                            IN_BSTR aId,
                                            MachineState_T *aMachineState,
                                            IProgress **aProgress)
{
    LogFlowThisFunc(("\n"));

    Guid id(aId);
    AssertReturn(aInitiator && !id.isEmpty(), E_INVALIDARG);
    AssertReturn(aMachineState && aProgress, E_POINTER);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    /* saveSettings() needs mParent lock */
    AutoMultiWriteLock2 alock(mParent, this);

    ComAssertRet (!Global::IsOnlineOrTransient (mData->mMachineState), E_FAIL);

    AutoWriteLock treeLock(snapshotsTreeLockHandle());

    ComObjPtr<Snapshot> snapshot;
    HRESULT rc = findSnapshot(id, snapshot, true /* aSetError */);
    CheckComRCReturnRC(rc);

    AutoWriteLock snapshotLock(snapshot);

    size_t childrenCount = snapshot->getChildrenCount();
    if (childrenCount > 1)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("Snapshot '%s' of the machine '%ls' has more than one child snapshot (%d)"),
                        snapshot->getName().c_str(),
                        mUserData->mName.raw(),
                        childrenCount);

    /* If the snapshot being discarded is the current one, ensure current
     * settings are committed and saved.
     */
    if (snapshot == mData->mCurrentSnapshot)
    {
        if (isModified())
        {
            rc = saveSettings();
            CheckComRCReturnRC(rc);
        }
    }

    /* create a progress object. The number of operations is:
     *   1 (preparing) + # of hard disks + 1 if the snapshot is online
     */
    ComObjPtr<Progress> progress;
    progress.createObject();
    rc = progress->init(mParent, aInitiator,
                        BstrFmt(tr("Discarding snapshot '%s'"),
                                snapshot->getName().c_str()),
                        FALSE /* aCancelable */,
                        1 + (ULONG)snapshot->getSnapshotMachine()->mMediaData->mAttachments.size() +
                        (snapshot->stateFilePath().isNull() ? 0 : 1),
                        Bstr (tr ("Preparing to discard snapshot")));
    AssertComRCReturn(rc, rc);

    /* create and start the task on a separate thread */
    DeleteSnapshotTask *task = new DeleteSnapshotTask(this, progress, snapshot);
    int vrc = RTThreadCreate(NULL,
                             taskHandler,
                             (void*)task,
                             0,
                             RTTHREADTYPE_MAIN_WORKER,
                             0,
                             "DeleteSnapshot");
    if (RT_FAILURE(vrc))
        delete task;
    ComAssertRCRet (vrc, E_FAIL);

    /* set the proper machine state (note: after creating a Task instance) */
    setMachineState(MachineState_DeletingSnapshot);

    /* return the progress to the caller */
    progress.queryInterfaceTo(aProgress);

    /* return the new state to the caller */
    *aMachineState = mData->mMachineState;

    return S_OK;
}

/**
 *  @note Locks this + children objects for writing!
 */
STDMETHODIMP SessionMachine::RestoreSnapshot(IConsole *aInitiator,
                                             ISnapshot *aSnapshot,
                                             MachineState_T *aMachineState,
                                             IProgress **aProgress)
{
    LogFlowThisFunc(("\n"));

    AssertReturn(aInitiator, E_INVALIDARG);
    AssertReturn(aSnapshot && aMachineState && aProgress, E_POINTER);

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    ComAssertRet(!Global::IsOnlineOrTransient(mData->mMachineState),
                 E_FAIL);

    ComObjPtr<Snapshot> pSnapshot(static_cast<Snapshot*>(aSnapshot));

    /* create a progress object. The number of operations is: 1 (preparing) + #
     * of hard disks + 1 (if we need to copy the saved state file) */
    ComObjPtr<Progress> progress;
    progress.createObject();
    {
        ULONG opCount =   1     // preparing
                        + (ULONG)pSnapshot->getSnapshotMachine()->mMediaData->mAttachments.size();  // one for each attachment @todo only for HDs!
        if (pSnapshot->stateFilePath())
            ++opCount;      // one for the saved state
        progress->init(mParent, aInitiator,
                       Bstr(tr("Restoring snapshot")),
                       FALSE /* aCancelable */, opCount,
                       Bstr(tr("Preparing to restore machine state from snapshot")));
    }

    /* create and start the task on a separate thread (note that it will not
     * start working until we release alock) */
    RestoreSnapshotTask *task = new RestoreSnapshotTask(this, pSnapshot, progress);
    int vrc = RTThreadCreate(NULL,
                             taskHandler,
                             (void*)task,
                             0,
                             RTTHREADTYPE_MAIN_WORKER,
                             0,
                             "DiscardCurState");
    if (RT_FAILURE(vrc))
    {
        delete task;
        ComAssertRCRet(vrc, E_FAIL);
    }

    /* set the proper machine state (note: after creating a Task instance) */
    setMachineState(MachineState_RestoringSnapshot);

    /* return the progress to the caller */
    progress.queryInterfaceTo(aProgress);

    /* return the new state to the caller */
    *aMachineState = mData->mMachineState;

    return S_OK;
}

STDMETHODIMP SessionMachine::PullGuestProperties(ComSafeArrayOut(BSTR, aNames),
                                                 ComSafeArrayOut(BSTR, aValues),
                                                 ComSafeArrayOut(ULONG64, aTimestamps),
                                                 ComSafeArrayOut(BSTR, aFlags))
{
    LogFlowThisFunc(("\n"));

#ifdef VBOX_WITH_GUEST_PROPS
    using namespace guestProp;

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this);

    AssertReturn(!ComSafeArrayOutIsNull(aNames), E_POINTER);
    AssertReturn(!ComSafeArrayOutIsNull(aValues), E_POINTER);
    AssertReturn(!ComSafeArrayOutIsNull(aTimestamps), E_POINTER);
    AssertReturn(!ComSafeArrayOutIsNull(aFlags), E_POINTER);

    size_t cEntries = mHWData->mGuestProperties.size();
    com::SafeArray<BSTR> names (cEntries);
    com::SafeArray<BSTR> values (cEntries);
    com::SafeArray<ULONG64> timestamps (cEntries);
    com::SafeArray<BSTR> flags (cEntries);
    unsigned i = 0;
    for (HWData::GuestPropertyList::iterator it = mHWData->mGuestProperties.begin();
         it != mHWData->mGuestProperties.end();
         ++it)
    {
        char szFlags[MAX_FLAGS_LEN + 1];
        it->strName.cloneTo(&names[i]);
        it->strValue.cloneTo(&values[i]);
        timestamps[i] = it->mTimestamp;
        /* If it is NULL, keep it NULL. */
        if (it->mFlags)
        {
            writeFlags(it->mFlags, szFlags);
            Bstr(szFlags).cloneTo(&flags[i]);
        }
        else
            flags[i] = NULL;
        ++i;
    }
    names.detachTo(ComSafeArrayOutArg(aNames));
    values.detachTo(ComSafeArrayOutArg(aValues));
    timestamps.detachTo(ComSafeArrayOutArg(aTimestamps));
    flags.detachTo(ComSafeArrayOutArg(aFlags));
    mHWData->mPropertyServiceActive = true;
    return S_OK;
#else
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP SessionMachine::PushGuestProperties(ComSafeArrayIn (IN_BSTR, aNames),
                                                 ComSafeArrayIn (IN_BSTR, aValues),
                                                 ComSafeArrayIn (ULONG64, aTimestamps),
                                                 ComSafeArrayIn (IN_BSTR, aFlags))
{
    LogFlowThisFunc(("\n"));

#ifdef VBOX_WITH_GUEST_PROPS
    using namespace guestProp;

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    /* Temporarily reset the registered flag, so that our machine state
     * changes (i.e. mHWData.backup()) succeed.  (isMutable() used in
     * all setters will return FALSE for a Machine instance if mRegistered
     * is TRUE).  This is copied from registeredInit(), and may or may not be
     * the right way to handle this. */
    mData->mRegistered = FALSE;
    HRESULT rc = checkStateDependency(MutableStateDep);
    LogRel (("checkStateDependency(MutableStateDep) returned 0x%x\n", rc));
    CheckComRCReturnRC(rc);

    // ComAssertRet (mData->mMachineState < MachineState_Running, E_FAIL);

    AssertReturn(!ComSafeArrayInIsNull (aNames), E_POINTER);
    AssertReturn(!ComSafeArrayInIsNull (aValues), E_POINTER);
    AssertReturn(!ComSafeArrayInIsNull (aTimestamps), E_POINTER);
    AssertReturn(!ComSafeArrayInIsNull (aFlags), E_POINTER);

    com::SafeArray<IN_BSTR> names (ComSafeArrayInArg (aNames));
    com::SafeArray<IN_BSTR> values (ComSafeArrayInArg (aValues));
    com::SafeArray<ULONG64> timestamps (ComSafeArrayInArg (aTimestamps));
    com::SafeArray<IN_BSTR> flags (ComSafeArrayInArg (aFlags));
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

STDMETHODIMP SessionMachine::PushGuestProperty(IN_BSTR aName,
                                               IN_BSTR aValue,
                                               ULONG64 aTimestamp,
                                               IN_BSTR aFlags)
{
    LogFlowThisFunc(("\n"));

#ifdef VBOX_WITH_GUEST_PROPS
    using namespace guestProp;

    CheckComArgNotNull (aName);
    if ((aValue != NULL) && (!VALID_PTR (aValue) || !VALID_PTR (aFlags)))
        return E_POINTER;  /* aValue can be NULL to indicate deletion */

    try
    {
        Utf8Str utf8Name(aName);
        Utf8Str utf8Flags(aFlags);
        Utf8Str utf8Patterns(mHWData->mGuestPropertyNotificationPatterns);

        uint32_t fFlags = NILFLAG;
        if ((aFlags != NULL) && RT_FAILURE(validateFlags (utf8Flags.raw(), &fFlags)))
            return E_INVALIDARG;

        bool matchAll = false;
        if (utf8Patterns.isEmpty())
            matchAll = true;

        AutoCaller autoCaller(this);
        CheckComRCReturnRC(autoCaller.rc());

        AutoWriteLock alock(this);

        HRESULT rc = checkStateDependency(MutableStateDep);
        CheckComRCReturnRC(rc);

        mHWData.backup();
        for (HWData::GuestPropertyList::iterator iter = mHWData->mGuestProperties.begin();
             iter != mHWData->mGuestProperties.end();
             ++iter)
            if (utf8Name == iter->strName)
            {
                mHWData->mGuestProperties.erase(iter);
                break;
            }
        if (aValue != NULL)
        {
            HWData::GuestProperty property = { aName, aValue, aTimestamp, fFlags };
            mHWData->mGuestProperties.push_back (property);
        }

        /* send a callback notification if appropriate */
        alock.leave();
        if (    matchAll
             || RTStrSimplePatternMultiMatch(utf8Patterns.raw(),
                                             RTSTR_MAX,
                                             utf8Name.raw(),
                                             RTSTR_MAX, NULL)
        )
            mParent->onGuestPropertyChange(mData->mUuid,
                                           aName,
                                           aValue,
                                           aFlags);
    }
    catch(std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }
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
        AutoCaller autoCaller(this);
        if (!autoCaller.isOk())
        {
            /* return true if not ready, to cause the client watcher to exclude
             * the corresponding session from watching */
            LogFlowThisFunc(("Already uninitialized!\n"));
            return true;
        }

        AutoWriteLock alock(this);

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
HRESULT SessionMachine::onNetworkAdapterChange (INetworkAdapter *networkAdapter, BOOL changeAdapter)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnNetworkAdapterChange (networkAdapter, changeAdapter);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onSerialPortChange (ISerialPort *serialPort)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);
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
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);
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
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);
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
HRESULT SessionMachine::onMediumChange(IMediumAttachment *aAttachment)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnMediumChange(aAttachment);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onVRDPServerChange()
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);
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
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);
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
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);
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
bool SessionMachine::hasMatchingUSBFilter (const ComObjPtr<HostUSBDevice> &aDevice, ULONG *aMaskedIfs)
{
    AutoCaller autoCaller(this);
    /* silently return if not ready -- this method may be called after the
     * direct machine session has been called */
    if (!autoCaller.isOk())
        return false;

    AutoReadLock alock(this);

#ifdef VBOX_WITH_USB
    switch (mData->mMachineState)
    {
        case MachineState_Starting:
        case MachineState_Restoring:
        case MachineState_TeleportingFrom:
        case MachineState_Paused:
        case MachineState_Running:
            return mUSBController->hasMatchingFilter (aDevice, aMaskedIfs);
        default: break;
    }
#else
    NOREF(aDevice);
    NOREF(aMaskedIfs);
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
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);

    /* This notification may happen after the machine object has been
     * uninitialized (the session was closed), so don't assert. */
    CheckComRCReturnRC(autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);
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
HRESULT SessionMachine::onUSBDeviceDetach (IN_BSTR aId,
                                           IVirtualBoxErrorInfo *aError)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);

    /* This notification may happen after the machine object has been
     * uninitialized (the session was closed), so don't assert. */
    CheckComRCReturnRC(autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);
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

    AutoCaller autoCaller(this);
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
        RTFileDelete(Utf8Str(mSnapshotData.mStateFilePath).c_str());
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
 * "take snapshot" procedure, either from the public SessionMachine::EndTakingSnapshot()
 * if taking the snapshot failed/was aborted or from the takeSnapshotHandler thread
 * when taking the snapshot succeeded.
 *
 * Expected to be called after completing *all* the tasks related to taking the
 * snapshot, either successfully or unsuccessfilly.
 *
 * @param aSuccess  TRUE if the snapshot has been taken successfully.
 *
 * @note Locks this objects for writing.
 */
HRESULT SessionMachine::endTakingSnapshot(BOOL aSuccess)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoMultiWriteLock2 alock(mParent, this);
            // saveSettings needs VirtualBox lock

    AssertReturn(!mSnapshotData.mSnapshot.isNull(), E_FAIL);

    MultiResult rc(S_OK);

    Snapshot *pOldFirstSnap = mData->mFirstSnapshot;
    Snapshot *pOldCurrentSnap = mData->mCurrentSnapshot;

    bool fOnline = Global::IsOnline(mSnapshotData.mLastState);

    if (aSuccess)
    {
        mData->mCurrentSnapshot = mSnapshotData.mSnapshot;

        /* memorize the first snapshot if necessary */
        if (!mData->mFirstSnapshot)
            mData->mFirstSnapshot = mData->mCurrentSnapshot;

        if (!fOnline)
            /* the machine was powered off or saved when taking a snapshot, so
             * reset the mCurrentStateModified flag */
            mData->mCurrentStateModified = FALSE;

        rc = saveSettings();
    }

    if (aSuccess && SUCCEEDED(rc))
    {
        /* associate old hard disks with the snapshot and do locking/unlocking*/
        fixupMedia(true /* aCommit */, fOnline);

        /* inform callbacks */
        mParent->onSnapshotTaken(mData->mUuid,
                                 mSnapshotData.mSnapshot->getId());
    }
    else
    {
        /* delete all differencing hard disks created (this will also attach
         * their parents back by rolling back mMediaData) */
        fixupMedia(false /* aCommit */);

        mData->mFirstSnapshot = pOldFirstSnap;      // might have been changed above
        mData->mCurrentSnapshot = pOldCurrentSnap;      // might have been changed above

        /* delete the saved state file (it might have been already created) */
        if (mSnapshotData.mSnapshot->stateFilePath())
            RTFileDelete(Utf8Str(mSnapshotData.mSnapshot->stateFilePath()).c_str());

        mSnapshotData.mSnapshot->uninit();
    }

    /* clear out the snapshot data */
    mSnapshotData.mLastState = MachineState_Null;
    mSnapshotData.mSnapshot.setNull();

    LogFlowThisFuncLeave();
    return rc;
}

/**
 * Helper struct for SessionMachine::deleteSnapshotHandler().
 */
struct MediumDiscardRec
{
    MediumDiscardRec() : chain (NULL) {}

    MediumDiscardRec (const ComObjPtr<Medium> &aHd,
                      Medium::MergeChain *aChain = NULL)
        : hd (aHd), chain (aChain) {}

    MediumDiscardRec (const ComObjPtr<Medium> &aHd,
                      Medium::MergeChain *aChain,
                      const ComObjPtr<Medium> &aReplaceHd,
                      const ComObjPtr<MediumAttachment> &aReplaceHda,
                      const Guid &aSnapshotId)
        : hd (aHd), chain (aChain)
        , replaceHd (aReplaceHd), replaceHda (aReplaceHda)
        , snapshotId (aSnapshotId) {}

    ComObjPtr<Medium> hd;
    Medium::MergeChain *chain;
    /* these are for the replace hard disk case: */
    ComObjPtr<Medium> replaceHd;
    ComObjPtr<MediumAttachment> replaceHda;
    Guid snapshotId;
};

typedef std::list <MediumDiscardRec> MediumDiscardRecList;

/**
 * Discard snapshot task handler. Must be called only by
 * DeleteSnapshotTask::handler()!
 *
 * When aTask.subTask is true, the associated progress object is left
 * uncompleted on success. On failure, the progress is marked as completed
 * regardless of this parameter.
 *
 * @note Locks mParent + this + child objects for writing!
 */
void SessionMachine::deleteSnapshotHandler(DeleteSnapshotTask &aTask)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);

    LogFlowThisFunc(("state=%d\n", autoCaller.state()));
    if (!autoCaller.isOk())
    {
        /* we might have been uninitialized because the session was accidentally
         * closed by the client, so don't assert */
        aTask.progress->notifyComplete(E_FAIL,
                                       COM_IIDOF(IMachine),
                                       getComponentName(),
                                       tr("The session has been accidentally closed"));
        LogFlowThisFuncLeave();
        return;
    }

    /* Locking order:  */
    AutoMultiWriteLock3 alock(this->lockHandle(),
                              this->snapshotsTreeLockHandle(),
                              aTask.snapshot->lockHandle());

    ComPtr<SnapshotMachine> sm = aTask.snapshot->getSnapshotMachine();
    /* no need to lock the snapshot machine since it is const by definiton */

    HRESULT rc = S_OK;

    /* save the snapshot ID (for callbacks) */
    Guid snapshotId = aTask.snapshot->getId();

    MediumDiscardRecList toDiscard;

    bool settingsChanged = false;

    try
    {
        /* first pass: */
        LogFlowThisFunc(("1: Checking hard disk merge prerequisites...\n"));

        for (MediaData::AttachmentList::const_iterator it = sm->mMediaData->mAttachments.begin();
             it != sm->mMediaData->mAttachments.end();
             ++it)
        {
            ComObjPtr<MediumAttachment> hda = *it;
            ComObjPtr<Medium> hd = hda->medium();

            // medium can be NULL only for non-hard-disk types
            Assert(    !hd.isNull()
                    || hda->type() != DeviceType_HardDisk);
            if (hd.isNull())
                continue;

            /* Medium::prepareDiscard() reqiuires a write lock */
            AutoWriteLock hdLock(hd);

            if (hd->type() != MediumType_Normal)
            {
                /* skip writethrough hard disks */
                Assert(hd->type() == MediumType_Writethrough);
                rc = aTask.progress->SetNextOperation(BstrFmt(tr("Skipping writethrough hard disk '%s'"),
                                                              hd->base()->name().raw()),
                                                      1); // weight
                CheckComRCThrowRC(rc);
                continue;
            }

            Medium::MergeChain *chain = NULL;

            /* needs to be discarded (merged with the child if any), check
             * prerequisites */
            rc = hd->prepareDiscard(chain);
            CheckComRCThrowRC(rc);

            if (hd->parent().isNull() && chain != NULL)
            {
                /* it's a base hard disk so it will be a backward merge of its
                 * only child to it (prepareDiscard() does necessary checks). We
                 * need then to update the attachment that refers to the child
                 * to refer to the parent instead. Don't forget to detach the
                 * child (otherwise mergeTo() called by discard() will assert
                 * because it will be going to delete the child) */

                /* The below assert would be nice but I don't want to move
                 * Medium::MergeChain to the header just for that
                 * Assert (!chain->isForward()); */

                Assert(hd->children().size() == 1);

                ComObjPtr<Medium> replaceHd = hd->children().front();

                const Guid *pReplaceMachineId = replaceHd->getFirstMachineBackrefId();
                Assert(pReplaceMachineId  && *pReplaceMachineId == mData->mUuid);

                Guid snapshotId;
                const Guid *pSnapshotId = replaceHd->getFirstMachineBackrefSnapshotId();
                if (pSnapshotId)
                    snapshotId = *pSnapshotId;

                HRESULT rc2 = S_OK;

                /* adjust back references */
                rc2 = replaceHd->detachFrom (mData->mUuid, snapshotId);
                AssertComRC(rc2);

                rc2 = hd->attachTo (mData->mUuid, snapshotId);
                AssertComRC(rc2);

                /* replace the hard disk in the attachment object */
                if (snapshotId.isEmpty())
                {
                    /* in current state */
                    AssertBreak(hda = findAttachment(mMediaData->mAttachments, replaceHd));
                }
                else
                {
                    /* in snapshot */
                    ComObjPtr<Snapshot> snapshot;
                    rc2 = findSnapshot(snapshotId, snapshot);
                    AssertComRC(rc2);

                    /* don't lock the snapshot; cannot be modified outside */
                    MediaData::AttachmentList &snapAtts = snapshot->getSnapshotMachine()->mMediaData->mAttachments;
                    AssertBreak(hda = findAttachment(snapAtts, replaceHd));
                }

                AutoWriteLock attLock(hda);
                hda->updateMedium(hd, false /* aImplicit */);

                toDiscard.push_back(MediumDiscardRec(hd,
                                                     chain,
                                                     replaceHd,
                                                     hda,
                                                     snapshotId));
                continue;
            }

            toDiscard.push_back(MediumDiscardRec(hd, chain));
        }

        /* Now we checked that we can successfully merge all normal hard disks
         * (unless a runtime error like end-of-disc happens). Prior to
         * performing the actual merge, we want to discard the snapshot itself
         * and remove it from the XML file to make sure that a possible merge
         * ruintime error will not make this snapshot inconsistent because of
         * the partially merged or corrupted hard disks */

        /* second pass: */
        LogFlowThisFunc(("2: Discarding snapshot...\n"));

        {
            ComObjPtr<Snapshot> parentSnapshot = aTask.snapshot->parent();
            Bstr stateFilePath = aTask.snapshot->stateFilePath();

            /* Note that discarding the snapshot will deassociate it from the
             * hard disks which will allow the merge+delete operation for them*/
            aTask.snapshot->beginDiscard();
            aTask.snapshot->uninit();

            rc = saveAllSnapshots();
            CheckComRCThrowRC(rc);

            /// @todo (dmik)
            //  if we implement some warning mechanism later, we'll have
            //  to return a warning if the state file path cannot be deleted
            if (stateFilePath)
            {
                aTask.progress->SetNextOperation(Bstr(tr("Discarding the execution state")),
                                                 1);        // weight

                RTFileDelete(Utf8Str(stateFilePath).c_str());
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
        LogFlowThisFunc(("3: Performing actual hard disk merging...\n"));

        /* leave the locks before the potentially lengthy operation */
        alock.leave();

        /// @todo NEWMEDIA turn the following errors into warnings because the
        /// snapshot itself has been already deleted (and interpret these
        /// warnings properly on the GUI side)

        for (MediumDiscardRecList::iterator it = toDiscard.begin();
             it != toDiscard.end();)
        {
            rc = it->hd->discard (aTask.progress, it->chain);
            CheckComRCBreakRC(rc);

            /* prevent from calling cancelDiscard() */
            it = toDiscard.erase (it);
        }

        alock.enter();

        CheckComRCThrowRC(rc);
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (FAILED(rc))
    {
        HRESULT rc2 = S_OK;

        /* un-prepare the remaining hard disks */
        for (MediumDiscardRecList::const_iterator it = toDiscard.begin();
             it != toDiscard.end(); ++it)
        {
            it->hd->cancelDiscard (it->chain);

            if (!it->replaceHd.isNull())
            {
                /* undo hard disk replacement */

                rc2 = it->replaceHd->attachTo (mData->mUuid, it->snapshotId);
                AssertComRC(rc2);

                rc2 = it->hd->detachFrom (mData->mUuid, it->snapshotId);
                AssertComRC(rc2);

                AutoWriteLock attLock (it->replaceHda);
                it->replaceHda->updateMedium(it->replaceHd, false /* aImplicit */);
            }
        }
    }

    if (!aTask.subTask || FAILED(rc))
    {
        if (!aTask.subTask)
        {
            /* saveSettings() below needs a VirtualBox write lock and we need to
             * leave this object's lock to do this to follow the {parent-child}
             * locking rule. This is the last chance to do that while we are
             * still in a protective state which allows us to temporarily leave
             * the lock */
            alock.unlock();
            AutoWriteLock vboxLock(mParent);
            alock.lock();

            /* preserve existing error info */
            ErrorInfoKeeper eik;

            /* restore the machine state */
            setMachineState(aTask.state);
            updateMachineStateOnClient();

            if (settingsChanged)
                saveSettings(SaveS_InformCallbacksAnyway);
        }

        /* set the result (this will try to fetch current error info on failure) */
        aTask.progress->notifyComplete (rc);
    }

    if (SUCCEEDED(rc))
        mParent->onSnapshotDiscarded (mData->mUuid, snapshotId);

    LogFlowThisFunc(("Done discarding snapshot (rc=%08X)\n", rc));
    LogFlowThisFuncLeave();
}

/**
 * Restore snapshot state task handler. Must be called only by
 * RestoreSnapshotTask::handler()!
 *
 * @note Locks mParent + this object for writing.
 */
void SessionMachine::restoreSnapshotHandler(RestoreSnapshotTask &aTask)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);

    LogFlowThisFunc(("state=%d\n", autoCaller.state()));
    if (!autoCaller.isOk())
    {
        /* we might have been uninitialized because the session was accidentally
         * closed by the client, so don't assert */
        aTask.progress->notifyComplete(E_FAIL,
                                       COM_IIDOF(IMachine),
                                       getComponentName(),
                                       tr("The session has been accidentally closed"));

        LogFlowThisFuncLeave();
        return;
    }

    /* saveSettings() needs mParent lock */
    AutoWriteLock vboxLock(mParent);

    /* @todo We don't need mParent lock so far so unlock() it. Better is to
     * provide an AutoWriteLock argument that lets create a non-locking
     * instance */
    vboxLock.unlock();

    AutoWriteLock alock(this);

    /* discard all current changes to mUserData (name, OSType etc.) (note that
     * the machine is powered off, so there is no need to inform the direct
     * session) */
    if (isModified())
        rollback(false /* aNotify */);

    HRESULT rc = S_OK;

    bool errorInSubtask = false;
    bool stateRestored = false;

    try
    {
        /* discard the saved state file if the machine was Saved prior to this
         * operation */
        if (aTask.state == MachineState_Saved)
        {
            Assert(!mSSData->mStateFilePath.isEmpty());
            RTFileDelete(Utf8Str(mSSData->mStateFilePath).c_str());
            mSSData->mStateFilePath.setNull();
            aTask.modifyLastState(MachineState_PoweredOff);
            rc = saveStateSettings(SaveSTS_StateFilePath);
            CheckComRCThrowRC(rc);
        }

        RTTIMESPEC snapshotTimeStamp;
        RTTimeSpecSetMilli(&snapshotTimeStamp, 0);

        {
            AutoReadLock snapshotLock(aTask.pSnapshot);

            /* remember the timestamp of the snapshot we're restoring from */
            snapshotTimeStamp = aTask.pSnapshot->getTimeStamp();

            ComPtr<SnapshotMachine> pSnapshotMachine(aTask.pSnapshot->getSnapshotMachine());

            /* copy all hardware data from the snapshot */
            copyFrom(pSnapshotMachine);

            LogFlowThisFunc(("Restoring hard disks from the snapshot...\n"));

            /* restore the attachments from the snapshot */
            mMediaData.backup();
            mMediaData->mAttachments = pSnapshotMachine->mMediaData->mAttachments;

            /* leave the locks before the potentially lengthy operation */
            snapshotLock.unlock();
            alock.leave();

            rc = createImplicitDiffs(mUserData->mSnapshotFolderFull,
                                     aTask.progress,
                                     1,
                                     false /* aOnline */);

            alock.enter();
            snapshotLock.lock();

            CheckComRCThrowRC(rc);

            /* Note: on success, current (old) hard disks will be
             * deassociated/deleted on #commit() called from #saveSettings() at
             * the end. On failure, newly created implicit diffs will be
             * deleted by #rollback() at the end. */

            /* should not have a saved state file associated at this point */
            Assert(mSSData->mStateFilePath.isNull());

            if (aTask.pSnapshot->stateFilePath())
            {
                Utf8Str snapStateFilePath = aTask.pSnapshot->stateFilePath();

                Utf8Str stateFilePath = Utf8StrFmt("%ls%c{%RTuuid}.sav",
                                                   mUserData->mSnapshotFolderFull.raw(),
                                                   RTPATH_DELIMITER,
                                                   mData->mUuid.raw());

                LogFlowThisFunc(("Copying saved state file from '%s' to '%s'...\n",
                                  snapStateFilePath.raw(), stateFilePath.raw()));

                aTask.progress->SetNextOperation(Bstr(tr("Restoring the execution state")),
                                                 1);        // weight

                /* leave the lock before the potentially lengthy operation */
                snapshotLock.unlock();
                alock.leave();

                /* copy the state file */
                int vrc = RTFileCopyEx(snapStateFilePath.c_str(),
                                       stateFilePath.c_str(),
                                       0,
                                       progressCallback,
                                       aTask.progress);

                alock.enter();
                snapshotLock.lock();

                if (RT_SUCCESS(vrc))
                {
                    mSSData->mStateFilePath = stateFilePath;

                    /* make the snapshot we restored from the current snapshot */
                    mData->mCurrentSnapshot = aTask.pSnapshot;
                }
                else
                {
                    throw setError(E_FAIL,
                                   tr("Could not copy the state file '%s' to '%s' (%Rrc)"),
                                   snapStateFilePath.raw(),
                                   stateFilePath.raw(),
                                   vrc);
                }
            }
        }

        /* grab differencing hard disks from the old attachments that will
         * become unused and need to be auto-deleted */

        std::list< ComObjPtr<Medium> > diffs;

        for (MediaData::AttachmentList::const_iterator it = mMediaData.backedUpData()->mAttachments.begin();
             it != mMediaData.backedUpData()->mAttachments.end();
             ++it)
        {
            ComObjPtr<Medium> hd = (*it)->medium();

            /* while the hard disk is attached, the number of children or the
             * parent cannot change, so no lock */
            if (!hd.isNull() && !hd->parent().isNull() && hd->children().size() == 0)
                diffs.push_back (hd);
        }

        int saveFlags = 0;

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
            setMachineState(MachineState_Saved);
        else
            setMachineState(MachineState_PoweredOff);

        updateMachineStateOnClient();
        stateRestored = true;

        /* assign the timestamp from the snapshot */
        Assert(RTTimeSpecGetMilli (&snapshotTimeStamp) != 0);
        mData->mLastStateChange = snapshotTimeStamp;

        /* save all settings, reset the modified flag and commit. Note that we
         * do so even if the subtask failed (errorInSubtask=true) because we've
         * already committed machine data and deleted old diffs before
         * discarding the snapshot so there is no way to rollback */
        HRESULT rc2 = saveSettings(SaveS_ResetCurStateModified | saveFlags);

        /// @todo NEWMEDIA return multiple errors
        if (errorInSubtask)
            throw rc;

        rc = rc2;

        if (SUCCEEDED(rc))
        {
            /* now, delete the unused diffs (only on success!) and uninit them*/
            for (std::list< ComObjPtr<Medium> >::const_iterator it = diffs.begin();
                 it != diffs.end();
                 ++it)
            {
                /// @todo for now, we ignore errors since we've already
                /// discarded and therefore cannot fail. Later, we may want to
                /// report a warning through the Progress object
                HRESULT rc2 = (*it)->deleteStorageAndWait();
                if (SUCCEEDED(rc2))
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
            setMachineState(aTask.state);
            updateMachineStateOnClient();
        }
    }

    if (!errorInSubtask)
    {
        /* set the result (this will try to fetch current error info on failure) */
        aTask.progress->notifyComplete (rc);
    }

    if (SUCCEEDED(rc))
        mParent->onSnapshotDiscarded(mData->mUuid, Guid());

    LogFlowThisFunc(("Done restoring snapshot (rc=%08X)\n", rc));

    LogFlowThisFuncLeave();
}

/**
 * Locks the attached media.
 *
 * All attached hard disks are locked for writing and DVD/floppy are locked for
 * reading. Parents of attached hard disks (if any) are locked for reading.
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
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    AssertReturn(   mData->mMachineState == MachineState_Starting
                 || mData->mMachineState == MachineState_Restoring
                 || mData->mMachineState == MachineState_TeleportingFrom, E_FAIL);

    typedef std::list <ComPtr<IMedium> > MediaList;
    MediaList mediaToCheck;
    MediumState_T mediaState;

    try
    {
        HRESULT rc = S_OK;

        /* lock all medium objects attached to the VM */
        for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
             it != mMediaData->mAttachments.end();
             ++it)
        {
            DeviceType_T devType = (*it)->type();
            ComObjPtr<Medium> hd = (*it)->medium();

            bool first = true;

            /** @todo split out the media locking, and put it into
             * MediumImpl.cpp, as it needs this functionality too. */
            while (!hd.isNull())
            {
                if (first)
                {
                    if (devType != DeviceType_DVD)
                    {
                        /* HardDisk and Floppy medium must be locked for writing */
                        rc = hd->LockWrite(&mediaState);
                        CheckComRCThrowRC(rc);
                    }
                    else
                    {
                        /* DVD medium must be locked for reading */
                        rc = hd->LockRead(&mediaState);
                        CheckComRCThrowRC(rc);
                    }

                    mData->mSession.mLockedMedia.push_back (
                        Data::Session::LockedMedia::value_type (
                            ComPtr<IMedium> (hd), true));

                    first = false;
                }
                else
                {
                    rc = hd->LockRead (&mediaState);
                    CheckComRCThrowRC(rc);

                    mData->mSession.mLockedMedia.push_back (
                        Data::Session::LockedMedia::value_type (
                            ComPtr<IMedium> (hd), false));
                }

                if (mediaState == MediumState_Inaccessible)
                    mediaToCheck.push_back (ComPtr<IMedium> (hd));

                /* no locks or callers here since there should be no way to
                 * change the hard disk parent at this point (as it is still
                 * attached to the machine) */
                hd = hd->parent();
            }
        }

        /* SUCCEEDED locking all media, now check accessibility */

        ErrorInfoKeeper eik (true /* aIsNull */);
        MultiResult mrc (S_OK);

        /* perform a check of inaccessible media deferred above */
        for (MediaList::const_iterator
             it = mediaToCheck.begin();
             it != mediaToCheck.end(); ++it)
        {
            MediumState_T mediaState;
            rc = (*it)->COMGETTER(State) (&mediaState);
            CheckComRCThrowRC(rc);

            Assert (mediaState == MediumState_LockedRead ||
                    mediaState == MediumState_LockedWrite);

            /* Note that we locked the medium already, so use the error
             * value to see if there was an accessibility failure */

            Bstr error;
            rc = (*it)->COMGETTER(LastAccessError) (error.asOutParam());
            CheckComRCThrowRC(rc);

            if (!error.isEmpty())
            {
                Bstr loc;
                rc = (*it)->COMGETTER(Location) (loc.asOutParam());
                CheckComRCThrowRC(rc);

                /* collect multiple errors */
                eik.restore();

                /* be in sync with MediumBase::setStateError() */
                Assert (!error.isEmpty());
                mrc = setError(E_FAIL,
                               tr("Medium '%ls' is not accessible. %ls"),
                               loc.raw(),
                               error.raw());

                eik.fetch();
            }
        }

        eik.restore();
        CheckComRCThrowRC((HRESULT) mrc);
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
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this);

    /* we may be holding important error info on the current thread;
     * preserve it */
    ErrorInfoKeeper eik;

    HRESULT rc = S_OK;

    for (Data::Session::LockedMedia::const_iterator
         it = mData->mSession.mLockedMedia.begin();
         it != mData->mSession.mLockedMedia.end(); ++it)
    {
        MediumState_T state;
        if (it->second)
            rc = it->first->UnlockWrite (&state);
        else
            rc = it->first->UnlockRead (&state);

        /* The second can happen if an object was re-locked in
         * Machine::fixupMedia(). The last can happen when e.g a DVD/Floppy
         * image was unmounted at runtime. */
        Assert (SUCCEEDED(rc) || state == MediumState_LockedRead || state == MediumState_Created);
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
    LogFlowThisFunc(("aMachineState=%d\n", aMachineState));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this);

    MachineState_T oldMachineState = mData->mMachineState;

    AssertMsgReturn (oldMachineState != aMachineState,
                     ("oldMachineState=%d, aMachineState=%d\n",
                      oldMachineState, aMachineState), E_FAIL);

    HRESULT rc = S_OK;

    int stsFlags = 0;
    bool deleteSavedState = false;

    /* detect some state transitions */

    if (   (   oldMachineState == MachineState_Saved
            && aMachineState   == MachineState_Restoring)
        || (   oldMachineState == MachineState_PoweredOff
            && aMachineState   == MachineState_TeleportingFrom)
        || (   oldMachineState <  MachineState_Running /* any other OFF state */
            && aMachineState   == MachineState_Starting)
       )
    {
        /* The EMT thread is about to start */

        /* Nothing to do here for now... */

        /// @todo NEWMEDIA don't let mDVDDrive and other children
        /// change anything when in the Starting/Restoring state
    }
    else if (   oldMachineState >= MachineState_Running
             && oldMachineState != MachineState_RestoringSnapshot
             && oldMachineState != MachineState_DeletingSnapshot
             && oldMachineState != MachineState_SettingUp
             && aMachineState < MachineState_Running
             /* ignore PoweredOff->Saving->PoweredOff transition when taking a
              * snapshot */
             && (   mSnapshotData.mSnapshot.isNull()
                 || mSnapshotData.mLastState >= MachineState_Running)
            )
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
    else if (   oldMachineState == MachineState_Saved
             && (   aMachineState == MachineState_PoweredOff
                 || aMachineState == MachineState_Aborted)
            )
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

    if (   aMachineState == MachineState_Starting
        || aMachineState == MachineState_Restoring
        || aMachineState == MachineState_TeleportingFrom
       )
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

    if (deleteSavedState)
    {
        if (mRemoveSavedState)
        {
            Assert (!mSSData->mStateFilePath.isEmpty());
            RTFileDelete(Utf8Str(mSSData->mStateFilePath).c_str());
        }
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

    LogFlowThisFunc(("rc=%08X\n", rc));
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
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this);
        AssertReturn(!!mData, E_FAIL);
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

        AssertReturn(!directControl.isNull(), E_FAIL);
    }

    return directControl->UpdateMachineState (mData->mMachineState);
}

/* static */
DECLCALLBACK(int) SessionMachine::taskHandler (RTTHREAD /* thread */, void *pvUser)
{
    AssertReturn(pvUser, VERR_INVALID_POINTER);

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
    LogFlowThisFunc(("\n"));

    /* set the proper type to indicate we're the SnapshotMachine instance */
    unconst(mType) = IsSnapshotMachine;

    return S_OK;
}

void SnapshotMachine::FinalRelease()
{
    LogFlowThisFunc(("\n"));

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
HRESULT SnapshotMachine::init(SessionMachine *aSessionMachine,
                              IN_GUID aSnapshotId,
                              const Utf8Str &aStateFilePath)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mName={%ls}\n", aSessionMachine->mUserData->mName.raw()));

    AssertReturn(aSessionMachine && !Guid (aSnapshotId).isEmpty(), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    AssertReturn(aSessionMachine->isWriteLockOnCurrentThread(), E_FAIL);

    mSnapshotId = aSnapshotId;

    /* memorize the primary Machine instance (i.e. not SessionMachine!) */
    unconst(mPeer) = aSessionMachine->mPeer;
    /* share the parent pointer */
    unconst(mParent) = mPeer->mParent;

    /* take the pointer to Data to share */
    mData.share (mPeer->mData);

    /* take the pointer to UserData to share (our UserData must always be the
     * same as Machine's data) */
    mUserData.share (mPeer->mUserData);
    /* make a private copy of all other data (recent changes from SessionMachine) */
    mHWData.attachCopy (aSessionMachine->mHWData);
    mMediaData.attachCopy(aSessionMachine->mMediaData);

    /* SSData is always unique for SnapshotMachine */
    mSSData.allocate();
    mSSData->mStateFilePath = aStateFilePath;

    HRESULT rc = S_OK;

    /* create copies of all shared folders (mHWData after attiching a copy
     * contains just references to original objects) */
    for (HWData::SharedFolderList::iterator
         it = mHWData->mSharedFolders.begin();
         it != mHWData->mSharedFolders.end();
         ++it)
    {
        ComObjPtr<SharedFolder> folder;
        folder.createObject();
        rc = folder->initCopy (this, *it);
        CheckComRCReturnRC(rc);
        *it = folder;
    }

    /* associate hard disks with the snapshot
     * (Machine::uninitDataAndChildObjects() will deassociate at destruction) */
    for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
         it != mMediaData->mAttachments.end();
         ++it)
    {
        MediumAttachment *pAtt = *it;
        Medium *pMedium = pAtt->medium();
        if (pMedium) // can be NULL for non-harddisk
        {
            rc = pMedium->attachTo(mData->mUuid, mSnapshotId);
            AssertComRC(rc);
        }
    }

    /* create copies of all storage controllers (mStorageControllerData
     * after attaching a copy contains just references to original objects) */
    mStorageControllers.allocate();
    for (StorageControllerList::const_iterator
         it = aSessionMachine->mStorageControllers->begin();
         it != aSessionMachine->mStorageControllers->end();
         ++it)
    {
        ComObjPtr<StorageController> ctrl;
        ctrl.createObject();
        ctrl->initCopy (this, *it);
        mStorageControllers->push_back(ctrl);
    }

    /* create all other child objects that will be immutable private copies */

    unconst(mBIOSSettings).createObject();
    mBIOSSettings->initCopy (this, mPeer->mBIOSSettings);

#ifdef VBOX_WITH_VRDP
    unconst(mVRDPServer).createObject();
    mVRDPServer->initCopy (this, mPeer->mVRDPServer);
#endif

    unconst(mAudioAdapter).createObject();
    mAudioAdapter->initCopy (this, mPeer->mAudioAdapter);

    unconst(mUSBController).createObject();
    mUSBController->initCopy (this, mPeer->mUSBController);

    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
    {
        unconst(mNetworkAdapters [slot]).createObject();
        mNetworkAdapters [slot]->initCopy (this, mPeer->mNetworkAdapters [slot]);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
    {
        unconst(mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->initCopy (this, mPeer->mSerialPorts [slot]);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
    {
        unconst(mParallelPorts [slot]).createObject();
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
HRESULT SnapshotMachine::init(Machine *aMachine,
                              const settings::Hardware &hardware,
                              const settings::Storage &storage,
                              IN_GUID aSnapshotId,
                              const Utf8Str &aStateFilePath)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mName={%ls}\n", aMachine->mUserData->mName.raw()));

    AssertReturn(aMachine &&  !Guid(aSnapshotId).isEmpty(), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Don't need to lock aMachine when VirtualBox is starting up */

    mSnapshotId = aSnapshotId;

    /* memorize the primary Machine instance */
    unconst(mPeer) = aMachine;
    /* share the parent pointer */
    unconst(mParent) = mPeer->mParent;

    /* take the pointer to Data to share */
    mData.share (mPeer->mData);
    /*
     *  take the pointer to UserData to share
     *  (our UserData must always be the same as Machine's data)
     */
    mUserData.share (mPeer->mUserData);
    /* allocate private copies of all other data (will be loaded from settings) */
    mHWData.allocate();
    mMediaData.allocate();
    mStorageControllers.allocate();

    /* SSData is always unique for SnapshotMachine */
    mSSData.allocate();
    mSSData->mStateFilePath = aStateFilePath;

    /* create all other child objects that will be immutable private copies */

    unconst(mBIOSSettings).createObject();
    mBIOSSettings->init (this);

#ifdef VBOX_WITH_VRDP
    unconst(mVRDPServer).createObject();
    mVRDPServer->init (this);
#endif

    unconst(mAudioAdapter).createObject();
    mAudioAdapter->init (this);

    unconst(mUSBController).createObject();
    mUSBController->init (this);

    for (ULONG slot = 0; slot < RT_ELEMENTS (mNetworkAdapters); slot ++)
    {
        unconst(mNetworkAdapters [slot]).createObject();
        mNetworkAdapters [slot]->init (this, slot);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mSerialPorts); slot ++)
    {
        unconst(mSerialPorts [slot]).createObject();
        mSerialPorts [slot]->init (this, slot);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS (mParallelPorts); slot ++)
    {
        unconst(mParallelPorts [slot]).createObject();
        mParallelPorts [slot]->init (this, slot);
    }

    /* load hardware and harddisk settings */

    HRESULT rc = loadHardware(hardware);
    if (SUCCEEDED(rc))
        rc = loadStorageControllers(storage, true /* aRegistered */, &mSnapshotId);

    if (SUCCEEDED(rc))
        /* commit all changes made during the initialization */
        commit();

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
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
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    uninitDataAndChildObjects();

    /* free the essential data structure last */
    mData.free();

    unconst(mParent).setNull();
    unconst(mPeer).setNull();

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
    AssertReturn(!mPeer.isNull(), NULL);
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
    AutoWriteLock alock(this);

    //     mPeer->saveAllSnapshots();  @todo

    /* inform callbacks */
    mParent->onSnapshotChange(mData->mUuid, aSnapshot->getId());

    return S_OK;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
