/* $Id$ */

/** @file
 * Implementation of IMachine in VBoxSVC.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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
# include <errno.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/ipc.h>
# include <sys/sem.h>
#endif

#include "Logging.h"
#include "VirtualBoxImpl.h"
#include "MachineImpl.h"
#include "ProgressImpl.h"
#include "MediumAttachmentImpl.h"
#include "MediumImpl.h"
#include "MediumLock.h"
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

#include "AutoCaller.h"
#include "Performance.h"

#include <iprt/asm.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/lockvalidator.h>
#include <iprt/process.h>
#include <iprt/cpp/utils.h>
#include <iprt/cpp/xml.h>               /* xml::XmlFileWriter::s_psz*Suff. */
#include <iprt/string.h>

#include <VBox/com/array.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/settings.h>
#include <VBox/ssm.h>

#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/HostServices/GuestPropertySvc.h>
# include <VBox/com/array.h>
#endif

#include <algorithm>

#include <typeinfo>

#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
# define HOSTSUFF_EXE ".exe"
#else /* !RT_OS_WINDOWS */
# define HOSTSUFF_EXE ""
#endif /* !RT_OS_WINDOWS */

// defines / prototypes
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Machine::Data structure
/////////////////////////////////////////////////////////////////////////////

Machine::Data::Data()
{
    mRegistered = FALSE;
    pMachineConfigFile = NULL;
    flModifications = 0;
    mAccessible = FALSE;
    /* mUuid is initialized in Machine::init() */

    mMachineState = MachineState_PoweredOff;
    RTTimeNow(&mLastStateChange);

    mMachineStateDeps = 0;
    mMachineStateDepsSem = NIL_RTSEMEVENTMULTI;
    mMachineStateChangePending = 0;

    mCurrentStateModified = TRUE;
    mGuestPropertiesModified = FALSE;

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
    if (pMachineConfigFile)
    {
        delete pMachineConfigFile;
        pMachineConfigFile = NULL;
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
    mRTCUseUTC = FALSE;

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
    mCPUHotPlugEnabled = false;
    mMemoryBalloonSize = 0;
    mVRAMSize = 8;
    mAccelerate3DEnabled = false;
    mAccelerate2DVideoEnabled = false;
    mMonitorCount = 1;
    mHWVirtExEnabled = true;
    mHWVirtExNestedPagingEnabled = true;
#if HC_ARCH_BITS == 64
    /* Default value decision pending. */
    mHWVirtExLargePagesEnabled = false;
#else
    /* Not supported on 32 bits hosts. */
    mHWVirtExLargePagesEnabled = false;
#endif
    mHWVirtExVPIDEnabled = true;
#if defined(RT_OS_DARWIN) || defined(RT_OS_WINDOWS)
    mHWVirtExExclusive = false;
#else
    mHWVirtExExclusive = true;
#endif
#if HC_ARCH_BITS == 64 || defined(RT_OS_WINDOWS) || defined(RT_OS_DARWIN)
    mPAEEnabled = true;
#else
    mPAEEnabled = false;
#endif
    mSyntheticCpu = false;
    mHpetEnabled = false;

    /* default boot order: floppy - DVD - HDD */
    mBootOrder[0] = DeviceType_Floppy;
    mBootOrder[1] = DeviceType_DVD;
    mBootOrder[2] = DeviceType_HardDisk;
    for (size_t i = 3; i < RT_ELEMENTS(mBootOrder); ++i)
        mBootOrder[i] = DeviceType_Null;

    mClipboardMode = ClipboardMode_Bidirectional;
    mGuestPropertyNotificationPatterns = "";

    mFirmwareType = FirmwareType_BIOS;
    mKeyboardHidType = KeyboardHidType_PS2Keyboard;
    mPointingHidType = PointingHidType_PS2Mouse;

    for (size_t i = 0; i < RT_ELEMENTS(mCPUAttached); i++)
        mCPUAttached[i] = false;

    mIoMgrType     = IoMgrType_Async;
    mIoBackendType = IoBackendType_Unbuffered;
    mIoCacheEnabled = true;
    mIoCacheSize    = 5; /* 5MB */
    mIoBandwidthMax = 0; /* Unlimited */
}

Machine::HWData::~HWData()
{
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

/////////////////////////////////////////////////////////////////////////////
// Machine class
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

Machine::Machine()
    : mGuestHAL(NULL),
      mPeer(NULL),
      mParent(NULL)
{}

Machine::~Machine()
{}

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
 *  Initializes a new machine instance; this init() variant creates a new, empty machine.
 *  This gets called from VirtualBox::CreateMachine() or VirtualBox::CreateLegacyMachine().
 *
 *  @param aParent      Associated parent object
 *  @param strConfigFile  Local file system path to the VM settings file (can
 *                      be relative to the VirtualBox config directory).
 *  @param strName      name for the machine
 *  @param aId          UUID for the new machine.
 *  @param aOsType      Optional OS Type of this machine.
 *  @param aOverride    |TRUE| to override VM config file existence checks.
 *                      |FALSE| refuses to overwrite existing VM configs.
 *  @param aNameSync    |TRUE| to automatically sync settings dir and file
 *                      name with the machine name. |FALSE| is used for legacy
 *                      machines where the file name is specified by the
 *                      user and should never change.
 *
 *  @return  Success indicator. if not S_OK, the machine object is invalid
 */
HRESULT Machine::init(VirtualBox *aParent,
                      const Utf8Str &strConfigFile,
                      const Utf8Str &strName,
                      const Guid &aId,
                      GuestOSType *aOsType /* = NULL */,
                      BOOL aOverride /* = FALSE */,
                      BOOL aNameSync /* = TRUE */)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("(Init_New) aConfigFile='%s'\n", strConfigFile.raw()));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = initImpl(aParent, strConfigFile);
    if (FAILED(rc)) return rc;

    rc = tryCreateMachineConfigFile(aOverride);
    if (FAILED(rc)) return rc;

    if (SUCCEEDED(rc))
    {
        // create an empty machine config
        mData->pMachineConfigFile = new settings::MachineConfigFile(NULL);

        rc = initDataAndChildObjects();
    }

    if (SUCCEEDED(rc))
    {
        // set to true now to cause uninit() to call uninitDataAndChildObjects() on failure
        mData->mAccessible = TRUE;

        unconst(mData->mUuid) = aId;

        mUserData->mName = strName;
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
            mBIOSSettings->applyDefaults(aOsType);

            /* Apply network adapters defaults */
            for (ULONG slot = 0; slot < RT_ELEMENTS(mNetworkAdapters); ++slot)
                mNetworkAdapters[slot]->applyDefaults(aOsType);

            /* Apply serial port defaults */
            for (ULONG slot = 0; slot < RT_ELEMENTS(mSerialPorts); ++slot)
                mSerialPorts[slot]->applyDefaults(aOsType);
        }

        /* commit all changes made during the initialization */
        commit();
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
    {
        if (mData->mAccessible)
            autoInitSpan.setSucceeded();
        else
            autoInitSpan.setLimited();
    }

    LogFlowThisFunc(("mName='%ls', mRegistered=%RTbool, mAccessible=%RTbool, rc=%08X\n",
                     !!mUserData ? mUserData->mName.raw() : NULL,
                     mData->mRegistered,
                     mData->mAccessible,
                     rc));

    LogFlowThisFuncLeave();

    return rc;
}

/**
 *  Initializes a new instance with data from machine XML (formerly Init_Registered).
 *  Gets called in two modes:
 *      -- from VirtualBox::initMachines() during VirtualBox startup; in that case, the
 *         UUID is specified and we mark the machine as "registered";
 *      -- from the public VirtualBox::OpenMachine() API, in which case the UUID is NULL
 *         and the machine remains unregistered until RegisterMachine() is called.
 *
 *  @param aParent      Associated parent object
 *  @param aConfigFile  Local file system path to the VM settings file (can
 *                      be relative to the VirtualBox config directory).
 *  @param aId          UUID of the machine or NULL (see above).
 *
 *  @return  Success indicator. if not S_OK, the machine object is invalid
 */
HRESULT Machine::init(VirtualBox *aParent,
                      const Utf8Str &strConfigFile,
                      const Guid *aId)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("(Init_Registered) aConfigFile='%s\n", strConfigFile.raw()));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = initImpl(aParent, strConfigFile);
    if (FAILED(rc)) return rc;

    if (aId)
    {
        // loading a registered VM:
        unconst(mData->mUuid) = *aId;
        mData->mRegistered = TRUE;
        // now load the settings from XML:
        rc = registeredInit();
            // this calls initDataAndChildObjects() and loadSettings()
    }
    else
    {
        // opening an unregistered VM (VirtualBox::OpenMachine()):
        rc = initDataAndChildObjects();

        if (SUCCEEDED(rc))
        {
            // set to true now to cause uninit() to call uninitDataAndChildObjects() on failure
            mData->mAccessible = TRUE;

            try
            {
                // load and parse machine XML; this will throw on XML or logic errors
                mData->pMachineConfigFile = new settings::MachineConfigFile(&mData->m_strConfigFileFull);

                // use UUID from machine config
                unconst(mData->mUuid) = mData->pMachineConfigFile->uuid;

                rc = loadMachineDataFromSettings(*mData->pMachineConfigFile);
                if (FAILED(rc)) throw rc;

                commit();
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
 *  Initializes a new instance from a machine config that is already in memory
 *  (import OVF import case). Since we are importing, the UUID in the machine
 *  config is ignored and we always generate a fresh one.
 *
 *  @param strName  Name for the new machine; this overrides what is specified in config and is used
 *                  for the settings file as well.
 *  @param config   Machine configuration loaded and parsed from XML.
 *
 *  @return  Success indicator. if not S_OK, the machine object is invalid
 */
HRESULT Machine::init(VirtualBox *aParent,
                      const Utf8Str &strName,
                      const settings::MachineConfigFile &config)
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    Utf8Str strConfigFile(aParent->getDefaultMachineFolder());
    strConfigFile.append(Utf8StrFmt("%c%s%c%s.xml",
                                    RTPATH_DELIMITER,
                                    strName.c_str(),
                                    RTPATH_DELIMITER,
                                    strName.c_str()));

    HRESULT rc = initImpl(aParent, strConfigFile);
    if (FAILED(rc)) return rc;

    rc = tryCreateMachineConfigFile(FALSE /* aOverride */);
    if (FAILED(rc)) return rc;

    rc = initDataAndChildObjects();

    if (SUCCEEDED(rc))
    {
        // set to true now to cause uninit() to call uninitDataAndChildObjects() on failure
        mData->mAccessible = TRUE;

        // create empty machine config for instance data
        mData->pMachineConfigFile = new settings::MachineConfigFile(NULL);

        // generate fresh UUID, ignore machine config
        unconst(mData->mUuid).create();

        rc = loadMachineDataFromSettings(config);

        // override VM name as well, it may be different
        mUserData->mName = strName;

        /* commit all changes made during the initialization */
        if (SUCCEEDED(rc))
            commit();
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
 * Shared code between the various init() implementations.
 * @param aParent
 * @return
 */
HRESULT Machine::initImpl(VirtualBox *aParent,
                          const Utf8Str &strConfigFile)
{
    LogFlowThisFuncEnter();

    AssertReturn(aParent, E_INVALIDARG);
    AssertReturn(!strConfigFile.isEmpty(), E_INVALIDARG);

    HRESULT rc = S_OK;

    /* share the parent weakly */
    unconst(mParent) = aParent;

    /* allocate the essential machine data structure (the rest will be
     * allocated later by initDataAndChildObjects() */
    mData.allocate();

    /* memorize the config file name (as provided) */
    mData->m_strConfigFile = strConfigFile;

    /* get the full file name */
    int vrc1 = mParent->calculateFullPath(strConfigFile, mData->m_strConfigFileFull);
    if (RT_FAILURE(vrc1))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Invalid machine settings file name '%s' (%Rrc)"),
                        strConfigFile.raw(),
                        vrc1);

    LogFlowThisFuncLeave();

    return rc;
}

/**
 * Tries to create a machine settings file in the path stored in the machine
 * instance data. Used when a new machine is created to fail gracefully if
 * the settings file could not be written (e.g. because machine dir is read-only).
 * @return
 */
HRESULT Machine::tryCreateMachineConfigFile(BOOL aOverride)
{
    HRESULT rc = S_OK;

    // when we create a new machine, we must be able to create the settings file
    RTFILE f = NIL_RTFILE;
    int vrc = RTFileOpen(&f, mData->m_strConfigFileFull.c_str(), RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (    RT_SUCCESS(vrc)
         || vrc == VERR_SHARING_VIOLATION
       )
    {
        if (RT_SUCCESS(vrc))
            RTFileClose(f);
        if (!aOverride)
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Machine settings file '%s' already exists"),
                          mData->m_strConfigFileFull.raw());
        else
        {
            /* try to delete the config file, as otherwise the creation
                * of a new settings file will fail. */
            int vrc2 = RTFileDelete(mData->m_strConfigFileFull.c_str());
            if (RT_FAILURE(vrc2))
                rc = setError(VBOX_E_FILE_ERROR,
                              tr("Could not delete the existing settings file '%s' (%Rrc)"),
                              mData->m_strConfigFileFull.raw(), vrc2);
        }
    }
    else if (    vrc != VERR_FILE_NOT_FOUND
              && vrc != VERR_PATH_NOT_FOUND
            )
        rc = setError(VBOX_E_FILE_ERROR,
                      tr("Invalid machine settings file name '%s' (%Rrc)"),
                      mData->m_strConfigFileFull.raw(),
                      vrc);
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
    AssertReturn(getClassID() == clsidMachine, E_FAIL);
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

        try
        {
            // load and parse machine XML; this will throw on XML or logic errors
            mData->pMachineConfigFile = new settings::MachineConfigFile(&mData->m_strConfigFileFull);

            if (mData->mUuid != mData->pMachineConfigFile->uuid)
                throw setError(E_FAIL,
                               tr("Machine UUID {%RTuuid} in '%s' doesn't match its UUID {%s} in the registry file '%s'"),
                               mData->pMachineConfigFile->uuid.raw(),
                               mData->m_strConfigFileFull.raw(),
                               mData->mUuid.toString().raw(),
                               mParent->settingsFilePath().raw());

            rc = loadMachineDataFromSettings(*mData->pMachineConfigFile);
            if (FAILED(rc)) throw rc;
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

        /* Restore the registered flag (even on failure) */
        mData->mRegistered = TRUE;
    }

    if (SUCCEEDED(rc))
    {
        /* Set mAccessible to TRUE only if we successfully locked and loaded
         * the settings file */
        mData->mAccessible = TRUE;

        /* commit all changes made during loading the settings file */
        commit(); // @todo r=dj why do we need a commit during init?!? this is very expensive
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
        rollback(false /* aNotify */);

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

    Assert(!isWriteLockOnCurrentThread());

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    Assert(getClassID() == clsidMachine);
    Assert(!!mData);

    LogFlowThisFunc(("initFailed()=%d\n", autoUninitSpan.initFailed()));
    LogFlowThisFunc(("mRegistered=%d\n", mData->mRegistered));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

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
        LogWarningThisFunc(("Session machine is not NULL (%p), the direct session is still open!\n",
                            (SessionMachine*)mData->mSession.mMachine));

        if (Global::IsOnlineOrTransient(mData->mMachineState))
        {
            LogWarningThisFunc(("Setting state to Aborted!\n"));
            /* set machine state using SessionMachine reimplementation */
            static_cast<Machine*>(mData->mSession.mMachine)->setMachineState(MachineState_Aborted);
        }

        /*
         *  Uninitialize SessionMachine using public uninit() to indicate
         *  an unexpected uninitialization.
         */
        mData->mSession.mMachine->uninit();
        /* SessionMachine::uninit() must set mSession.mMachine to null */
        Assert(mData->mSession.mMachine.isNull());
    }

    /* the lock is no more necessary (SessionMachine is uninitialized) */
    alock.leave();

    // has machine been modified?
    if (mData->flModifications)
    {
        LogWarningThisFunc(("Discarding unsaved settings changes!\n"));
        rollback(false /* aNotify */);
    }

    if (mData->mAccessible)
        uninitDataAndChildObjects();

    /* free the essential data structure last */
    mData.free();

    LogFlowThisFuncLeave();
}

// IMachine properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Machine::COMGETTER(Parent)(IVirtualBox **aParent)
{
    CheckComArgOutPointerValid(aParent);

    AutoLimitedCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mParent is constant during life time, no need to lock */
    ComObjPtr<VirtualBox> pVirtualBox(mParent);
    pVirtualBox.queryInterfaceTo(aParent);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Accessible)(BOOL *aAccessible)
{
    CheckComArgOutPointerValid(aAccessible);

    AutoLimitedCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    LogFlowThisFunc(("ENTER\n"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (!mData->mAccessible)
    {
        /* try to initialize the VM once more if not accessible */

        AutoReinitSpan autoReinitSpan(this);
        AssertReturn(autoReinitSpan.isOk(), E_FAIL);

#ifdef DEBUG
        LogFlowThisFunc(("Dumping media backreferences\n"));
        mParent->dumpAllBackRefs();
#endif

        if (mData->pMachineConfigFile)
        {
            // reset the XML file to force loadSettings() (called from registeredInit())
            // to parse it again; the file might have changed
            delete mData->pMachineConfigFile;
            mData->pMachineConfigFile = NULL;
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

    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP Machine::COMGETTER(AccessError)(IVirtualBoxErrorInfo **aAccessError)
{
    CheckComArgOutPointerValid(aAccessError);

    AutoLimitedCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

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
        errorInfo->init(mData->mAccessError.getResultCode(),
                        mData->mAccessError.getInterfaceID(),
                        mData->mAccessError.getComponent(),
                        mData->mAccessError.getText());
        rc = errorInfo.queryInterfaceTo(aAccessError);
    }

    return rc;
}

STDMETHODIMP Machine::COMGETTER(Name)(BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mUserData->mName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Name)(IN_BSTR aName)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->mName = aName;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Description)(BSTR *aDescription)
{
    CheckComArgOutPointerValid(aDescription);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mUserData->mDescription.cloneTo(aDescription);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Description)(IN_BSTR aDescription)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->mDescription = aDescription;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Id)(BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoLimitedCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mUuid.toUtf16().cloneTo(aId);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(OSTypeId)(BSTR *aOSTypeId)
{
    CheckComArgOutPointerValid(aOSTypeId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mUserData->mOSTypeId.cloneTo(aOSTypeId);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(OSTypeId)(IN_BSTR aOSTypeId)
{
    CheckComArgStrNotEmptyOrNull(aOSTypeId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* look up the object by Id to check it is valid */
    ComPtr<IGuestOSType> guestOSType;
    HRESULT rc = mParent->GetGuestOSType(aOSTypeId, guestOSType.asOutParam());
    if (FAILED(rc)) return rc;

    /* when setting, always use the "etalon" value for consistency -- lookup
     * by ID is case-insensitive and the input value may have different case */
    Bstr osTypeId;
    rc = guestOSType->COMGETTER(Id)(osTypeId.asOutParam());
    if (FAILED(rc)) return rc;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->mOSTypeId = osTypeId;

    return S_OK;
}


STDMETHODIMP Machine::COMGETTER(FirmwareType)(FirmwareType_T *aFirmwareType)
{
    CheckComArgOutPointerValid(aFirmwareType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aFirmwareType = mHWData->mFirmwareType;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(FirmwareType)(FirmwareType_T aFirmwareType)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mFirmwareType = aFirmwareType;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(KeyboardHidType)(KeyboardHidType_T *aKeyboardHidType)
{
    CheckComArgOutPointerValid(aKeyboardHidType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aKeyboardHidType = mHWData->mKeyboardHidType;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(KeyboardHidType)(KeyboardHidType_T  aKeyboardHidType)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mKeyboardHidType = aKeyboardHidType;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(PointingHidType)(PointingHidType_T *aPointingHidType)
{
    CheckComArgOutPointerValid(aPointingHidType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aPointingHidType = mHWData->mPointingHidType;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(PointingHidType)(PointingHidType_T  aPointingHidType)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mPointingHidType = aPointingHidType;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(HardwareVersion)(BSTR *aHWVersion)
{
    if (!aHWVersion)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mHWData->mHWVersion.cloneTo(aHWVersion);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(HardwareVersion)(IN_BSTR aHWVersion)
{
    /* check known version */
    Utf8Str hwVersion = aHWVersion;
    if (    hwVersion.compare("1") != 0
        &&  hwVersion.compare("2") != 0)
        return setError(E_INVALIDARG,
                        tr("Invalid hardware version: %ls\n"), aHWVersion);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mHWVersion = hwVersion;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(HardwareUUID)(BSTR *aUUID)
{
    CheckComArgOutPointerValid(aUUID);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mHWData->mHardwareUUID.isEmpty())
        mHWData->mHardwareUUID.toUtf16().cloneTo(aUUID);
    else
        mData->mUuid.toUtf16().cloneTo(aUUID);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(HardwareUUID)(IN_BSTR aUUID)
{
    Guid hardwareUUID(aUUID);
    if (hardwareUUID.isEmpty())
        return E_INVALIDARG;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    if (hardwareUUID == mData->mUuid)
        mHWData->mHardwareUUID.clear();
    else
        mHWData->mHardwareUUID = hardwareUUID;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(MemorySize)(ULONG *memorySize)
{
    if (!memorySize)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *memorySize = mHWData->mMemorySize;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(MemorySize)(ULONG memorySize)
{
    /* check RAM limits */
    if (    memorySize < MM_RAM_MIN_IN_MB
         || memorySize > MM_RAM_MAX_IN_MB
       )
        return setError(E_INVALIDARG,
                        tr("Invalid RAM size: %lu MB (must be in range [%lu, %lu] MB)"),
                        memorySize, MM_RAM_MIN_IN_MB, MM_RAM_MAX_IN_MB);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mMemorySize = memorySize;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CPUCount)(ULONG *CPUCount)
{
    if (!CPUCount)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *CPUCount = mHWData->mCPUCount;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(CPUCount)(ULONG CPUCount)
{
    /* check CPU limits */
    if (    CPUCount < SchemaDefs::MinCPUCount
         || CPUCount > SchemaDefs::MaxCPUCount
       )
        return setError(E_INVALIDARG,
                        tr("Invalid virtual CPU count: %lu (must be in range [%lu, %lu])"),
                        CPUCount, SchemaDefs::MinCPUCount, SchemaDefs::MaxCPUCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* We cant go below the current number of CPUs if hotplug is enabled*/
    if (mHWData->mCPUHotPlugEnabled)
    {
        for (unsigned idx = CPUCount; idx < SchemaDefs::MaxCPUCount; idx++)
        {
            if (mHWData->mCPUAttached[idx])
                return setError(E_INVALIDARG,
                                tr(": %lu (must be higher than or equal to %lu)"),
                                CPUCount, idx+1);
        }
    }

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mCPUCount = CPUCount;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CPUHotPlugEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = mHWData->mCPUHotPlugEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(CPUHotPlugEnabled)(BOOL enabled)
{
    HRESULT rc = S_OK;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    if (mHWData->mCPUHotPlugEnabled != enabled)
    {
        if (enabled)
        {
            setModified(IsModified_MachineData);
            mHWData.backup();

            /* Add the amount of CPUs currently attached */
            for (unsigned i = 0; i < mHWData->mCPUCount; i++)
            {
                mHWData->mCPUAttached[i] = true;
            }
        }
        else
        {
            /*
             * We can disable hotplug only if the amount of maximum CPUs is equal
             * to the amount of attached CPUs
             */
            unsigned cCpusAttached = 0;
            unsigned iHighestId = 0;

            for (unsigned i = 0; i < SchemaDefs::MaxCPUCount; i++)
            {
                if (mHWData->mCPUAttached[i])
                {
                    cCpusAttached++;
                    iHighestId = i;
                }
            }

            if (   (cCpusAttached != mHWData->mCPUCount)
                || (iHighestId >= mHWData->mCPUCount))
                return setError(E_INVALIDARG,
                                tr("CPU hotplugging can't be disabled because the maximum number of CPUs is not equal to the amount of CPUs attached\n"));

            setModified(IsModified_MachineData);
            mHWData.backup();
        }
    }

    mHWData->mCPUHotPlugEnabled = enabled;

    return rc;
}

STDMETHODIMP Machine::COMGETTER(HpetEnabled)(BOOL *enabled)
{
    CheckComArgOutPointerValid(enabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = mHWData->mHpetEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(HpetEnabled)(BOOL enabled)
{
    HRESULT rc = S_OK;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();

    mHWData->mHpetEnabled = enabled;

    return rc;
}

STDMETHODIMP Machine::COMGETTER(VRAMSize)(ULONG *memorySize)
{
    if (!memorySize)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *memorySize = mHWData->mVRAMSize;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(VRAMSize)(ULONG memorySize)
{
    /* check VRAM limits */
    if (memorySize < SchemaDefs::MinGuestVRAM ||
        memorySize > SchemaDefs::MaxGuestVRAM)
        return setError(E_INVALIDARG,
                        tr("Invalid VRAM size: %lu MB (must be in range [%lu, %lu] MB)"),
                        memorySize, SchemaDefs::MinGuestVRAM, SchemaDefs::MaxGuestVRAM);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mVRAMSize = memorySize;

    return S_OK;
}

/** @todo this method should not be public */
STDMETHODIMP Machine::COMGETTER(MemoryBalloonSize)(ULONG *memoryBalloonSize)
{
    if (!memoryBalloonSize)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *memoryBalloonSize = mHWData->mMemoryBalloonSize;

    return S_OK;
}

/**
 * Set the memory balloon size.
 *
 * This method is also called from IGuest::COMSETTER(MemoryBalloonSize) so
 * we have to make sure that we never call IGuest from here.
 */
STDMETHODIMP Machine::COMSETTER(MemoryBalloonSize)(ULONG memoryBalloonSize)
{
    /* This must match GMMR0Init; currently we only support memory ballooning on all 64-bit hosts except Mac OS X */
#if HC_ARCH_BITS == 64 && (defined(RT_OS_WINDOWS) || defined(RT_OS_SOLARIS) || defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD))
    /* check limits */
    if (memoryBalloonSize >= VMMDEV_MAX_MEMORY_BALLOON(mHWData->mMemorySize))
        return setError(E_INVALIDARG,
                        tr("Invalid memory balloon size: %lu MB (must be in range [%lu, %lu] MB)"),
                        memoryBalloonSize, 0, VMMDEV_MAX_MEMORY_BALLOON(mHWData->mMemorySize));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mMemoryBalloonSize = memoryBalloonSize;

    return S_OK;
#else
    NOREF(memoryBalloonSize);
    return setError(E_NOTIMPL, tr("Memory ballooning is only supported on 64-bit hosts"));
#endif
}

STDMETHODIMP Machine::COMGETTER(Accelerate3DEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = mHWData->mAccelerate3DEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Accelerate3DEnabled)(BOOL enable)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    /** @todo check validity! */

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mAccelerate3DEnabled = enable;

    return S_OK;
}


STDMETHODIMP Machine::COMGETTER(Accelerate2DVideoEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = mHWData->mAccelerate2DVideoEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Accelerate2DVideoEnabled)(BOOL enable)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    /** @todo check validity! */

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mAccelerate2DVideoEnabled = enable;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(MonitorCount)(ULONG *monitorCount)
{
    if (!monitorCount)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *monitorCount = mHWData->mMonitorCount;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(MonitorCount)(ULONG monitorCount)
{
    /* make sure monitor count is a sensible number */
    if (monitorCount < 1 || monitorCount > SchemaDefs::MaxGuestMonitors)
        return setError(E_INVALIDARG,
                        tr("Invalid monitor count: %lu (must be in range [%lu, %lu])"),
                        monitorCount, 1, SchemaDefs::MaxGuestMonitors);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mMonitorCount = monitorCount;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(BIOSSettings)(IBIOSSettings **biosSettings)
{
    if (!biosSettings)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mBIOSSettings is constant during life time, no need to lock */
    mBIOSSettings.queryInterfaceTo(biosSettings);

    return S_OK;
}

STDMETHODIMP Machine::GetCPUProperty(CPUPropertyType_T property, BOOL *aVal)
{
    if (!aVal)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    switch(property)
    {
    case CPUPropertyType_PAE:
        *aVal = mHWData->mPAEEnabled;
        break;

    case CPUPropertyType_Synthetic:
        *aVal = mHWData->mSyntheticCpu;
        break;

    default:
        return E_INVALIDARG;
    }
    return S_OK;
}

STDMETHODIMP Machine::SetCPUProperty(CPUPropertyType_T property, BOOL aVal)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    switch(property)
    {
    case CPUPropertyType_PAE:
        setModified(IsModified_MachineData);
        mHWData.backup();
        mHWData->mPAEEnabled = !!aVal;
        break;

    case CPUPropertyType_Synthetic:
        setModified(IsModified_MachineData);
        mHWData.backup();
        mHWData->mSyntheticCpu = !!aVal;
        break;

    default:
        return E_INVALIDARG;
    }
    return S_OK;
}

STDMETHODIMP Machine::GetCPUIDLeaf(ULONG aId, ULONG *aValEax, ULONG *aValEbx, ULONG *aValEcx, ULONG *aValEdx)
{
    CheckComArgOutPointerValid(aValEax);
    CheckComArgOutPointerValid(aValEbx);
    CheckComArgOutPointerValid(aValEcx);
    CheckComArgOutPointerValid(aValEdx);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    switch(aId)
    {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        case 0x8:
        case 0x9:
        case 0xA:
            if (mHWData->mCpuIdStdLeafs[aId].ulId != aId)
                return setError(E_INVALIDARG, tr("CpuId override leaf %#x is not set"), aId);

            *aValEax = mHWData->mCpuIdStdLeafs[aId].ulEax;
            *aValEbx = mHWData->mCpuIdStdLeafs[aId].ulEbx;
            *aValEcx = mHWData->mCpuIdStdLeafs[aId].ulEcx;
            *aValEdx = mHWData->mCpuIdStdLeafs[aId].ulEdx;
            break;

        case 0x80000000:
        case 0x80000001:
        case 0x80000002:
        case 0x80000003:
        case 0x80000004:
        case 0x80000005:
        case 0x80000006:
        case 0x80000007:
        case 0x80000008:
        case 0x80000009:
        case 0x8000000A:
            if (mHWData->mCpuIdExtLeafs[aId - 0x80000000].ulId != aId)
                return setError(E_INVALIDARG, tr("CpuId override leaf %#x is not set"), aId);

            *aValEax = mHWData->mCpuIdExtLeafs[aId - 0x80000000].ulEax;
            *aValEbx = mHWData->mCpuIdExtLeafs[aId - 0x80000000].ulEbx;
            *aValEcx = mHWData->mCpuIdExtLeafs[aId - 0x80000000].ulEcx;
            *aValEdx = mHWData->mCpuIdExtLeafs[aId - 0x80000000].ulEdx;
            break;

        default:
            return setError(E_INVALIDARG, tr("CpuId override leaf %#x is out of range"), aId);
    }
    return S_OK;
}

STDMETHODIMP Machine::SetCPUIDLeaf(ULONG aId, ULONG aValEax, ULONG aValEbx, ULONG aValEcx, ULONG aValEdx)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    switch(aId)
    {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        case 0x8:
        case 0x9:
        case 0xA:
            AssertCompile(RT_ELEMENTS(mHWData->mCpuIdStdLeafs) == 0xA);
            AssertRelease(aId < RT_ELEMENTS(mHWData->mCpuIdStdLeafs));
            setModified(IsModified_MachineData);
            mHWData.backup();
            mHWData->mCpuIdStdLeafs[aId].ulId  = aId;
            mHWData->mCpuIdStdLeafs[aId].ulEax = aValEax;
            mHWData->mCpuIdStdLeafs[aId].ulEbx = aValEbx;
            mHWData->mCpuIdStdLeafs[aId].ulEcx = aValEcx;
            mHWData->mCpuIdStdLeafs[aId].ulEdx = aValEdx;
            break;

        case 0x80000000:
        case 0x80000001:
        case 0x80000002:
        case 0x80000003:
        case 0x80000004:
        case 0x80000005:
        case 0x80000006:
        case 0x80000007:
        case 0x80000008:
        case 0x80000009:
        case 0x8000000A:
            AssertCompile(RT_ELEMENTS(mHWData->mCpuIdExtLeafs) == 0xA);
            AssertRelease(aId - 0x80000000 < RT_ELEMENTS(mHWData->mCpuIdExtLeafs));
            setModified(IsModified_MachineData);
            mHWData.backup();
            mHWData->mCpuIdExtLeafs[aId - 0x80000000].ulId  = aId;
            mHWData->mCpuIdExtLeafs[aId - 0x80000000].ulEax = aValEax;
            mHWData->mCpuIdExtLeafs[aId - 0x80000000].ulEbx = aValEbx;
            mHWData->mCpuIdExtLeafs[aId - 0x80000000].ulEcx = aValEcx;
            mHWData->mCpuIdExtLeafs[aId - 0x80000000].ulEdx = aValEdx;
            break;

        default:
            return setError(E_INVALIDARG, tr("CpuId override leaf %#x is out of range"), aId);
    }
    return S_OK;
}

STDMETHODIMP Machine::RemoveCPUIDLeaf(ULONG aId)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    switch(aId)
    {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        case 0x8:
        case 0x9:
        case 0xA:
            AssertCompile(RT_ELEMENTS(mHWData->mCpuIdStdLeafs) == 0xA);
            AssertRelease(aId < RT_ELEMENTS(mHWData->mCpuIdStdLeafs));
            setModified(IsModified_MachineData);
            mHWData.backup();
            /* Invalidate leaf. */
            mHWData->mCpuIdStdLeafs[aId].ulId = UINT32_MAX;
            break;

        case 0x80000000:
        case 0x80000001:
        case 0x80000002:
        case 0x80000003:
        case 0x80000004:
        case 0x80000005:
        case 0x80000006:
        case 0x80000007:
        case 0x80000008:
        case 0x80000009:
        case 0x8000000A:
            AssertCompile(RT_ELEMENTS(mHWData->mCpuIdExtLeafs) == 0xA);
            AssertRelease(aId - 0x80000000 < RT_ELEMENTS(mHWData->mCpuIdExtLeafs));
            setModified(IsModified_MachineData);
            mHWData.backup();
            /* Invalidate leaf. */
            mHWData->mCpuIdExtLeafs[aId - 0x80000000].ulId = UINT32_MAX;
            break;

        default:
            return setError(E_INVALIDARG, tr("CpuId override leaf %#x is out of range"), aId);
    }
    return S_OK;
}

STDMETHODIMP Machine::RemoveAllCPUIDLeaves()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();

    /* Invalidate all standard leafs. */
    for (unsigned i = 0; i < RT_ELEMENTS(mHWData->mCpuIdStdLeafs); i++)
        mHWData->mCpuIdStdLeafs[i].ulId = UINT32_MAX;

    /* Invalidate all extended leafs. */
    for (unsigned i = 0; i < RT_ELEMENTS(mHWData->mCpuIdExtLeafs); i++)
        mHWData->mCpuIdExtLeafs[i].ulId = UINT32_MAX;

    return S_OK;
}

STDMETHODIMP Machine::GetHWVirtExProperty(HWVirtExPropertyType_T property, BOOL *aVal)
{
    if (!aVal)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

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

        case HWVirtExPropertyType_LargePages:
            *aVal = mHWData->mHWVirtExLargePagesEnabled;
            break;

        default:
            return E_INVALIDARG;
    }
    return S_OK;
}

STDMETHODIMP Machine::SetHWVirtExProperty(HWVirtExPropertyType_T property, BOOL aVal)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    switch(property)
    {
        case HWVirtExPropertyType_Enabled:
            setModified(IsModified_MachineData);
            mHWData.backup();
            mHWData->mHWVirtExEnabled = !!aVal;
            break;

        case HWVirtExPropertyType_Exclusive:
            setModified(IsModified_MachineData);
            mHWData.backup();
            mHWData->mHWVirtExExclusive = !!aVal;
            break;

        case HWVirtExPropertyType_VPID:
            setModified(IsModified_MachineData);
            mHWData.backup();
            mHWData->mHWVirtExVPIDEnabled = !!aVal;
            break;

        case HWVirtExPropertyType_NestedPaging:
            setModified(IsModified_MachineData);
            mHWData.backup();
            mHWData->mHWVirtExNestedPagingEnabled = !!aVal;
            break;

        case HWVirtExPropertyType_LargePages:
            setModified(IsModified_MachineData);
            mHWData.backup();
            mHWData->mHWVirtExLargePagesEnabled = !!aVal;
            break;

        default:
            return E_INVALIDARG;
    }

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SnapshotFolder)(BSTR *aSnapshotFolder)
{
    CheckComArgOutPointerValid(aSnapshotFolder);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mUserData->mSnapshotFolderFull.cloneTo(aSnapshotFolder);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(SnapshotFolder)(IN_BSTR aSnapshotFolder)
{
    /* @todo (r=dmik):
     *  1. Allow to change the name of the snapshot folder containing snapshots
     *  2. Rename the folder on disk instead of just changing the property
     *     value (to be smart and not to leave garbage). Note that it cannot be
     *     done here because the change may be rolled back. Thus, the right
     *     place is #saveSettings().
     */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    if (!mData->mCurrentSnapshot.isNull())
        return setError(E_FAIL,
                        tr("The snapshot folder of a machine with snapshots cannot be changed (please delete all snapshots first)"));

    Utf8Str snapshotFolder = aSnapshotFolder;

    if (snapshotFolder.isEmpty())
    {
        if (isInOwnDir())
        {
            /* the default snapshots folder is 'Snapshots' in the machine dir */
            snapshotFolder = "Snapshots";
        }
        else
        {
            /* the default snapshots folder is {UUID}, for backwards
             * compatibility and to resolve conflicts */
            snapshotFolder = Utf8StrFmt("{%RTuuid}", mData->mUuid.raw());
        }
    }

    int vrc = calculateFullPath(snapshotFolder, snapshotFolder);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL,
                        tr("Invalid snapshot folder '%ls' (%Rrc)"),
                        aSnapshotFolder, vrc);

    setModified(IsModified_MachineData);
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
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

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
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(!!mVRDPServer);
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
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mAudioAdapter.queryInterfaceTo(audioAdapter);
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(USBController)(IUSBController **aUSBController)
{
#ifdef VBOX_WITH_VUSB
    CheckComArgOutPointerValid(aUSBController);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    MultiResult rc(S_OK);

# ifdef VBOX_WITH_USB
    rc = mParent->host()->checkUSBProxyService();
    if (FAILED(rc)) return rc;
# endif

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return rc = mUSBController.queryInterfaceTo(aUSBController);
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treting it as a failure), for example, as in OSE */
    NOREF(aUSBController);
    ReturnComNotImplemented();
#endif /* VBOX_WITH_VUSB */
}

STDMETHODIMP Machine::COMGETTER(SettingsFilePath)(BSTR *aFilePath)
{
    CheckComArgOutPointerValid(aFilePath);

    AutoLimitedCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m_strConfigFileFull.cloneTo(aFilePath);
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SettingsModified)(BOOL *aModified)
{
    CheckComArgOutPointerValid(aModified);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    if (!mData->pMachineConfigFile->fileExists())
        // this is a new machine, and no config file exists yet:
        *aModified = TRUE;
    else
        *aModified = (mData->flModifications != 0);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SessionState)(SessionState_T *aSessionState)
{
    CheckComArgOutPointerValid(aSessionState);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSessionState = mData->mSession.mState;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SessionType)(BSTR *aSessionType)
{
    CheckComArgOutPointerValid(aSessionType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mSession.mType.cloneTo(aSessionType);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SessionPid)(ULONG *aSessionPid)
{
    CheckComArgOutPointerValid(aSessionPid);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSessionPid = mData->mSession.mPid;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(State)(MachineState_T *machineState)
{
    if (!machineState)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *machineState = mData->mMachineState;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(LastStateChange)(LONG64 *aLastStateChange)
{
    CheckComArgOutPointerValid(aLastStateChange);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aLastStateChange = RTTimeSpecGetMilli(&mData->mLastStateChange);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(StateFilePath)(BSTR *aStateFilePath)
{
    CheckComArgOutPointerValid(aStateFilePath);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mSSData->mStateFilePath.cloneTo(aStateFilePath);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(LogFolder)(BSTR *aLogFolder)
{
    CheckComArgOutPointerValid(aLogFolder);

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str logFolder;
    getLogFolder(logFolder);

    Bstr (logFolder).cloneTo(aLogFolder);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CurrentSnapshot) (ISnapshot **aCurrentSnapshot)
{
    CheckComArgOutPointerValid(aCurrentSnapshot);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mCurrentSnapshot.queryInterfaceTo(aCurrentSnapshot);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SnapshotCount)(ULONG *aSnapshotCount)
{
    CheckComArgOutPointerValid(aSnapshotCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSnapshotCount = mData->mFirstSnapshot.isNull()
                          ? 0
                          : mData->mFirstSnapshot->getAllChildrenCount() + 1;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(CurrentStateModified)(BOOL *aCurrentStateModified)
{
    CheckComArgOutPointerValid(aCurrentStateModified);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Note: for machines with no snapshots, we always return FALSE
     * (mData->mCurrentStateModified will be TRUE in this case, for historical
     * reasons :) */

    *aCurrentStateModified = mData->mFirstSnapshot.isNull()
                            ? FALSE
                            : mData->mCurrentStateModified;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(SharedFolders)(ComSafeArrayOut(ISharedFolder *, aSharedFolders))
{
    CheckComArgOutSafeArrayPointerValid(aSharedFolders);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<ISharedFolder> folders(mHWData->mSharedFolders);
    folders.detachTo(ComSafeArrayOutArg(aSharedFolders));

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(ClipboardMode)(ClipboardMode_T *aClipboardMode)
{
    CheckComArgOutPointerValid(aClipboardMode);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aClipboardMode = mHWData->mClipboardMode;

    return S_OK;
}

STDMETHODIMP
Machine::COMSETTER(ClipboardMode)(ClipboardMode_T aClipboardMode)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mClipboardMode = aClipboardMode;

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(GuestPropertyNotificationPatterns)(BSTR *aPatterns)
{
    CheckComArgOutPointerValid(aPatterns);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    try
    {
        mHWData->mGuestPropertyNotificationPatterns.cloneTo(aPatterns);
    }
    catch (...)
    {
        return VirtualBox::handleUnexpectedExceptions(RT_SRC_POS);
    }

    return S_OK;
}

STDMETHODIMP
Machine::COMSETTER(GuestPropertyNotificationPatterns)(IN_BSTR aPatterns)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mGuestPropertyNotificationPatterns = aPatterns;
    return rc;
}

STDMETHODIMP
Machine::COMGETTER(StorageControllers)(ComSafeArrayOut(IStorageController *, aStorageControllers))
{
    CheckComArgOutSafeArrayPointerValid(aStorageControllers);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IStorageController> ctrls(*mStorageControllers.data());
    ctrls.detachTo(ComSafeArrayOutArg(aStorageControllers));

    return S_OK;
}

STDMETHODIMP
Machine::COMGETTER(TeleporterEnabled)(BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = mUserData->mTeleporterEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(TeleporterEnabled)(BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Only allow it to be set to true when PoweredOff or Aborted.
       (Clearing it is always permitted.) */
    if (    aEnabled
        &&  mData->mRegistered
        &&  (   getClassID() != clsidSessionMachine
             || (   mData->mMachineState != MachineState_PoweredOff
                 && mData->mMachineState != MachineState_Teleported
                 && mData->mMachineState != MachineState_Aborted
                )
            )
       )
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("The machine is not powered off (state is %s)"),
                        Global::stringifyMachineState(mData->mMachineState));

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->mTeleporterEnabled = aEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(TeleporterPort)(ULONG *aPort)
{
    CheckComArgOutPointerValid(aPort);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aPort = mUserData->mTeleporterPort;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(TeleporterPort)(ULONG aPort)
{
    if (aPort >= _64K)
        return setError(E_INVALIDARG, tr("Invalid port number %d"), aPort);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->mTeleporterPort = aPort;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(TeleporterAddress)(BSTR *aAddress)
{
    CheckComArgOutPointerValid(aAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mUserData->mTeleporterAddress.cloneTo(aAddress);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(TeleporterAddress)(IN_BSTR aAddress)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->mTeleporterAddress = aAddress;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(TeleporterPassword)(BSTR *aPassword)
{
    CheckComArgOutPointerValid(aPassword);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mUserData->mTeleporterPassword.cloneTo(aPassword);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(TeleporterPassword)(IN_BSTR aPassword)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->mTeleporterPassword = aPassword;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(RTCUseUTC)(BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = mUserData->mRTCUseUTC;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(RTCUseUTC)(BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Only allow it to be set to true when PoweredOff or Aborted.
       (Clearing it is always permitted.) */
    if (    aEnabled
        &&  mData->mRegistered
        &&  (   getClassID() != clsidSessionMachine
             || (   mData->mMachineState != MachineState_PoweredOff
                 && mData->mMachineState != MachineState_Teleported
                 && mData->mMachineState != MachineState_Aborted
                )
            )
       )
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("The machine is not powered off (state is %s)"),
                        Global::stringifyMachineState(mData->mMachineState));

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->mRTCUseUTC = aEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(IoMgr)(IoMgrType_T *aIoMgrType)
{
    CheckComArgOutPointerValid(aIoMgrType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aIoMgrType = mHWData->mIoMgrType;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(IoMgr)(IoMgrType_T aIoMgrType)
{
    if (   aIoMgrType != IoMgrType_Async
        && aIoMgrType != IoMgrType_Simple)
        return E_INVALIDARG;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mIoMgrType = aIoMgrType;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(IoBackend)(IoBackendType_T *aIoBackendType)
{
    CheckComArgOutPointerValid(aIoBackendType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aIoBackendType = mHWData->mIoBackendType;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(IoBackend)(IoBackendType_T aIoBackendType)
{
    if (   aIoBackendType != IoBackendType_Buffered
        && aIoBackendType != IoBackendType_Unbuffered)
        return E_INVALIDARG;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mIoBackendType = aIoBackendType;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(IoCacheEnabled)(BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = mHWData->mIoCacheEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(IoCacheEnabled)(BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mIoCacheEnabled = aEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(IoCacheSize)(ULONG *aIoCacheSize)
{
    CheckComArgOutPointerValid(aIoCacheSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aIoCacheSize = mHWData->mIoCacheSize;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(IoCacheSize)(ULONG  aIoCacheSize)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mIoCacheSize = aIoCacheSize;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(IoBandwidthMax)(ULONG *aIoBandwidthMax)
{
    CheckComArgOutPointerValid(aIoBandwidthMax);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aIoBandwidthMax = mHWData->mIoBandwidthMax;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(IoBandwidthMax)(ULONG  aIoBandwidthMax)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mIoBandwidthMax = aIoBandwidthMax;

    return S_OK;
}

STDMETHODIMP Machine::SetBootOrder(ULONG aPosition, DeviceType_T aDevice)
{
    if (aPosition < 1 || aPosition > SchemaDefs::MaxBootPosition)
        return setError(E_INVALIDARG,
                        tr("Invalid boot position: %lu (must be in range [1, %lu])"),
                        aPosition, SchemaDefs::MaxBootPosition);

    if (aDevice == DeviceType_USB)
        return setError(E_NOTIMPL,
                        tr("Booting from USB device is currently not supported"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mBootOrder[aPosition - 1] = aDevice;

    return S_OK;
}

STDMETHODIMP Machine::GetBootOrder(ULONG aPosition, DeviceType_T *aDevice)
{
    if (aPosition < 1 || aPosition > SchemaDefs::MaxBootPosition)
        return setError(E_INVALIDARG,
                       tr("Invalid boot position: %lu (must be in range [1, %lu])"),
                       aPosition, SchemaDefs::MaxBootPosition);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aDevice = mHWData->mBootOrder[aPosition - 1];

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

    CheckComArgStrNotEmptyOrNull(aControllerName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // if this becomes true then we need to call saveSettings in the end
    // @todo r=dj there is no error handling so far...
    bool fNeedsSaveSettings = false;

    // request the host lock first, since might be calling Host methods for getting host drives;
    // next, protect the media tree all the while we're in here, as well as our member variables
    AutoMultiWriteLock2 alock(mParent->host()->lockHandle(),
                              this->lockHandle() COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock treeLock(&mParent->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

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
    if (FAILED(rc)) return rc;

    /* check that the port and device are not out of range. */
    ULONG portCount;
    ULONG devicesPerPort;
    rc = ctl->COMGETTER(PortCount)(&portCount);
    if (FAILED(rc)) return rc;
    rc = ctl->COMGETTER(MaxDevicesPerPortCount)(&devicesPerPort);
    if (FAILED(rc)) return rc;

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
        Medium *pMedium = pAttachTemp->getMedium();
        if (pMedium)
        {
            AutoReadLock mediumLock(pMedium COMMA_LOCKVAL_SRC_POS);
            return setError(VBOX_E_OBJECT_IN_USE,
                            tr("Medium '%s' is already attached to device slot %d on port %d of controller '%ls' of this virtual machine"),
                            pMedium->getLocationFull().raw(),
                            aDevice,
                            aControllerPort,
                            aControllerName);
        }
        else
            return setError(VBOX_E_OBJECT_IN_USE,
                            tr("Device is already attached to slot %d on port %d of controller '%ls' of this virtual machine"),
                            aDevice, aControllerPort, aControllerName);
    }

    Guid uuid(aId);

    ComObjPtr<Medium> medium;
    switch (aType)
    {
        case DeviceType_HardDisk:
            /* find a hard disk by UUID */
            rc = mParent->findHardDisk(&uuid, NULL, true /* aSetError */, &medium);
            if (FAILED(rc)) return rc;
            break;

        case DeviceType_DVD: // @todo r=dj eliminate this, replace with findDVDImage
            if (!uuid.isEmpty())
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
                        if (med->getId() == uuid)
                        {
                            medium = med;
                            break;
                        }
                    }
                }

                if (medium.isNull())
                {
                    /* find a DVD image by UUID */
                    rc = mParent->findDVDImage(&uuid, NULL, true /* aSetError */, &medium);
                    if (FAILED(rc)) return rc;
                }
            }
            else
            {
                /* null UUID means null medium, which needs no code */
            }
            break;

        case DeviceType_Floppy: // @todo r=dj eliminate this, replace with findFloppyImage
            if (!uuid.isEmpty())
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
                        if (med->getId() == uuid)
                        {
                            medium = med;
                            break;
                        }
                    }
                }

                if (medium.isNull())
                {
                    /* find a floppy image by UUID */
                    rc = mParent->findFloppyImage(&uuid, NULL, true /* aSetError */, &medium);
                    if (FAILED(rc)) return rc;
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
    if (FAILED(mediumCaller.rc())) return mediumCaller.rc();

    AutoWriteLock mediumLock(medium COMMA_LOCKVAL_SRC_POS);

    if (    (pAttachTemp = findAttachment(mMediaData->mAttachments, medium))
         && !medium.isNull()
       )
        return setError(VBOX_E_OBJECT_IN_USE,
                        tr("Medium '%s' is already attached to this virtual machine"),
                        medium->getLocationFull().raw());

    bool indirect = false;
    if (!medium.isNull())
        indirect = medium->isReadOnly();
    bool associate = true;

    do
    {
        if (aType == DeviceType_HardDisk && mMediaData.isBackedUp())
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

        if (medium->getParent().isNull())
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
                    ComObjPtr<Medium> pMedium = pAttach->getMedium();
                    Assert(!pMedium.isNull() || pAttach->getType() != DeviceType_HardDisk);
                    if (pMedium.isNull())
                        continue;

                    if (pMedium->getBase(&level).equalsTo(medium))
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
                    medium = (*foundIt)->getMedium();
                    mediumCaller.attach(medium);
                    if (FAILED(mediumCaller.rc())) return mediumCaller.rc();
                    mediumLock.attach(medium);
                    /* not implicit, doesn't require association with this VM */
                    indirect = false;
                    associate = false;
                    /* go right to the MediumAttachment creation */
                    break;
                }
            }

            /* must give up the medium lock and medium tree lock as below we
             * go over snapshots, which needs a lock with higher lock order. */
            mediumLock.release();
            treeLock.release();

            /* then, search through snapshots for the best diff in the given
             * hard disk's chain to base the new diff on */

            ComObjPtr<Medium> base;
            ComObjPtr<Snapshot> snap = mData->mCurrentSnapshot;
            while (snap)
            {
                AutoReadLock snapLock(snap COMMA_LOCKVAL_SRC_POS);

                const MediaData::AttachmentList &snapAtts = snap->getSnapshotMachine()->mMediaData->mAttachments;

                MediaData::AttachmentList::const_iterator foundIt = snapAtts.end();
                uint32_t foundLevel = 0;

                for (MediaData::AttachmentList::const_iterator it = snapAtts.begin();
                     it != snapAtts.end();
                     ++it)
                {
                    MediumAttachment *pAttach = *it;
                    ComObjPtr<Medium> pMedium = pAttach->getMedium();
                    Assert(!pMedium.isNull() || pAttach->getType() != DeviceType_HardDisk);
                    if (pMedium.isNull())
                        continue;

                    uint32_t level = 0;
                    if (pMedium->getBase(&level).equalsTo(medium))
                    {
                        /* matched device, channel and bus (i.e. attached to the
                         * same place) will win and immediately stop the search;
                         * otherwise the attachment that has the youngest
                         * descendant of medium will be used
                         */
                        if (    (*it)->getDevice() == aDevice
                             && (*it)->getPort() == aControllerPort
                             && (*it)->getControllerName() == aControllerName
                           )
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
                    base = (*foundIt)->getMedium();
                    break;
                }

                snap = snap->getParent();
            }

            /* re-lock medium tree and the medium, as we need it below */
            treeLock.acquire();
            mediumLock.acquire();

            /* found a suitable diff, use it as a base */
            if (!base.isNull())
            {
                medium = base;
                mediumCaller.attach(medium);
                if (FAILED(mediumCaller.rc())) return mediumCaller.rc();
                mediumLock.attach(medium);
            }
        }

        ComObjPtr<Medium> diff;
        diff.createObject();
        rc = diff->init(mParent,
                        medium->preferredDiffFormat().raw(),
                        BstrFmt("%ls"RTPATH_SLASH_STR,
                                 mUserData->mSnapshotFolderFull.raw()).raw(),
                        &fNeedsSaveSettings);
        if (FAILED(rc)) return rc;

        /* Apply the normal locking logic to the entire chain. */
        MediumLockList *pMediumLockList(new MediumLockList());
        rc = diff->createMediumLockList(true, medium, *pMediumLockList);
        if (FAILED(rc)) return rc;
        rc = pMediumLockList->Lock();
        if (FAILED(rc))
            return setError(rc,
                            tr("Could not lock medium when creating diff '%s'"),
                            diff->getLocationFull().c_str());

        /* will leave the lock before the potentially lengthy operation, so
         * protect with the special state */
        MachineState_T oldState = mData->mMachineState;
        setMachineState(MachineState_SettingUp);

        mediumLock.leave();
        treeLock.leave();
        alock.leave();

        rc = medium->createDiffStorage(diff, MediumVariant_Standard,
                                       pMediumLockList, NULL /* aProgress */,
                                       true /* aWait */, &fNeedsSaveSettings);

        alock.enter();
        treeLock.enter();
        mediumLock.enter();

        setMachineState(oldState);

        /* Unlock the media and free the associated memory. */
        delete pMediumLockList;

        if (FAILED(rc)) return rc;

        /* use the created diff for the actual attachment */
        medium = diff;
        mediumCaller.attach(medium);
        if (FAILED(mediumCaller.rc())) return mediumCaller.rc();
        mediumLock.attach(medium);
    }
    while (0);

    ComObjPtr<MediumAttachment> attachment;
    attachment.createObject();
    rc = attachment->init(this, medium, aControllerName, aControllerPort, aDevice, aType, indirect);
    if (FAILED(rc)) return rc;

    if (associate && !medium.isNull())
    {
        /* as the last step, associate the medium to the VM */
        rc = medium->attachTo(mData->mUuid);
        /* here we can fail because of Deleting, or being in process of
         * creating a Diff */
        if (FAILED(rc)) return rc;
    }

    /* success: finally remember the attachment */
    setModified(IsModified_Storage);
    mMediaData.backup();
    mMediaData->mAttachments.push_back(attachment);

    if (fNeedsSaveSettings)
    {
        mediumLock.release();
        treeLock.leave();
        alock.release();

        AutoWriteLock vboxLock(mParent COMMA_LOCKVAL_SRC_POS);
        mParent->saveSettings();
    }

    return rc;
}

STDMETHODIMP Machine::DetachDevice(IN_BSTR aControllerName, LONG aControllerPort,
                                   LONG aDevice)
{
    CheckComArgStrNotEmptyOrNull(aControllerName);

    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%ld aDevice=%ld\n",
                     aControllerName, aControllerPort, aDevice));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    bool fNeedsSaveSettings = false;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

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

    ComObjPtr<Medium> oldmedium = pAttach->getMedium();
    DeviceType_T mediumType = pAttach->getType();

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

        rc = oldmedium->deleteStorage(NULL /*aProgress*/, true /*aWait*/,
                                      &fNeedsSaveSettings);

        alock.enter();

        setMachineState(oldState);

        if (FAILED(rc)) return rc;
    }

    setModified(IsModified_Storage);
    mMediaData.backup();

    /* we cannot use erase (it) below because backup() above will create
     * a copy of the list and make this copy active, but the iterator
     * still refers to the original and is not valid for the copy */
    mMediaData->mAttachments.remove(pAttach);

    /* For non-hard disk media, detach straight away. */
    if (mediumType != DeviceType_HardDisk && !oldmedium.isNull())
        oldmedium->detachFrom(mData->mUuid);

    if (fNeedsSaveSettings)
    {
        bool fNeedsGlobalSaveSettings = false;
        saveSettings(&fNeedsGlobalSaveSettings);

        if (fNeedsGlobalSaveSettings)
        {
            alock.release();
            AutoWriteLock vboxlock(this COMMA_LOCKVAL_SRC_POS);
            mParent->saveSettings();
        }
    }

    return S_OK;
}

STDMETHODIMP Machine::PassthroughDevice(IN_BSTR aControllerName, LONG aControllerPort,
                                        LONG aDevice, BOOL aPassthrough)
{
    CheckComArgStrNotEmptyOrNull(aControllerName);

    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%ld aDevice=%ld aPassthrough=%d\n",
                     aControllerName, aControllerPort, aDevice, aPassthrough));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

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


    setModified(IsModified_Storage);
    mMediaData.backup();

    AutoWriteLock attLock(pAttach COMMA_LOCKVAL_SRC_POS);

    if (pAttach->getType() != DeviceType_DVD)
        return setError(E_INVALIDARG,
                        tr("Setting passthrough rejected as the device attached to device slot %d on port %d of controller '%ls' is not a DVD"),
                        aDevice, aControllerPort, aControllerName);
    pAttach->updatePassthrough(!!aPassthrough);

    return S_OK;
}

STDMETHODIMP Machine::MountMedium(IN_BSTR aControllerName,
                                  LONG aControllerPort,
                                  LONG aDevice,
                                  IN_BSTR aId,
                                  BOOL aForce)
{
    int rc = S_OK;
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%ld aDevice=%ld aForce=%d\n",
                     aControllerName, aControllerPort, aDevice, aForce));

    CheckComArgStrNotEmptyOrNull(aControllerName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // we're calling host methods for getting DVD and floppy drives so lock host first
    AutoMultiWriteLock2 alock(mParent->host(), this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<MediumAttachment> pAttach = findAttachment(mMediaData->mAttachments,
                                                         aControllerName,
                                                         aControllerPort,
                                                         aDevice);
    if (pAttach.isNull())
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("No drive attached to device slot %d on port %d of controller '%ls'"),
                        aDevice, aControllerPort, aControllerName);

    /* Remember previously mounted medium. The medium before taking the
     * backup is not necessarily the same thing. */
    ComObjPtr<Medium> oldmedium;
    oldmedium = pAttach->getMedium();

    Guid uuid(aId);
    ComObjPtr<Medium> medium;
    DeviceType_T mediumType = pAttach->getType();
    switch (mediumType)
    {
        case DeviceType_DVD:
            if (!uuid.isEmpty())
            {
                /* find a DVD by host device UUID */
                MediaList llHostDVDDrives;
                rc = mParent->host()->getDVDDrives(llHostDVDDrives);
                if (SUCCEEDED(rc))
                {
                    for (MediaList::iterator it = llHostDVDDrives.begin();
                         it != llHostDVDDrives.end();
                         ++it)
                    {
                        ComObjPtr<Medium> &p = *it;
                        if (uuid == p->getId())
                        {
                            medium = p;
                            break;
                        }
                    }
                }
                /* find a DVD by UUID */
                if (medium.isNull())
                    rc = mParent->findDVDImage(&uuid, NULL, true /* aDoSetError */, &medium);
            }
            if (FAILED(rc)) return rc;
            break;
        case DeviceType_Floppy:
            if (!uuid.isEmpty())
            {
                /* find a Floppy by host device UUID */
                MediaList llHostFloppyDrives;
                rc = mParent->host()->getFloppyDrives(llHostFloppyDrives);
                if (SUCCEEDED(rc))
                {
                    for (MediaList::iterator it = llHostFloppyDrives.begin();
                         it != llHostFloppyDrives.end();
                         ++it)
                    {
                        ComObjPtr<Medium> &p = *it;
                        if (uuid == p->getId())
                        {
                            medium = p;
                            break;
                        }
                    }
                }
                /* find a Floppy by UUID */
                if (medium.isNull())
                    rc = mParent->findFloppyImage(&uuid, NULL, true /* aDoSetError */, &medium);
            }
            if (FAILED(rc)) return rc;
            break;
        default:
            return setError(VBOX_E_INVALID_OBJECT_STATE,
                            tr("Cannot change medium attached to device slot %d on port %d of controller '%ls'"),
                            aDevice, aControllerPort, aControllerName);
    }

    if (SUCCEEDED(rc))
    {
        setModified(IsModified_Storage);
        mMediaData.backup();

        /* The backup operation makes the pAttach reference point to the
         * old settings. Re-get the correct reference. */
        pAttach = findAttachment(mMediaData->mAttachments,
                                 aControllerName,
                                 aControllerPort,
                                 aDevice);
        AutoWriteLock attLock(pAttach COMMA_LOCKVAL_SRC_POS);
        /* For non-hard disk media, detach straight away. */
        if (mediumType != DeviceType_HardDisk && !oldmedium.isNull())
            oldmedium->detachFrom(mData->mUuid);
        if (!medium.isNull())
            medium->attachTo(mData->mUuid);
        pAttach->updateMedium(medium, false /* aImplicit */);
        setModified(IsModified_Storage);
    }

    alock.leave();
    rc = onMediumChange(pAttach, aForce);
    alock.enter();

    /* On error roll back this change only. */
    if (FAILED(rc))
    {
        if (!medium.isNull())
            medium->detachFrom(mData->mUuid);
        pAttach = findAttachment(mMediaData->mAttachments,
                                 aControllerName,
                                 aControllerPort,
                                 aDevice);
        /* If the attachment is gone in the mean time, bail out. */
        if (pAttach.isNull())
            return rc;
        AutoWriteLock attLock(pAttach COMMA_LOCKVAL_SRC_POS);
        /* For non-hard disk media, re-attach straight away. */
        if (mediumType != DeviceType_HardDisk && !oldmedium.isNull())
            oldmedium->attachTo(mData->mUuid);
        pAttach->updateMedium(oldmedium, false /* aImplicit */);
    }

    return rc;
}

STDMETHODIMP Machine::GetMedium(IN_BSTR aControllerName,
                                LONG aControllerPort,
                                LONG aDevice,
                                IMedium **aMedium)
{
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%ld aDevice=%ld\n",
                     aControllerName, aControllerPort, aDevice));

    CheckComArgStrNotEmptyOrNull(aControllerName);
    CheckComArgOutPointerValid(aMedium);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMedium = NULL;

    ComObjPtr<MediumAttachment> pAttach = findAttachment(mMediaData->mAttachments,
                                                         aControllerName,
                                                         aControllerPort,
                                                         aDevice);
    if (pAttach.isNull())
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("No storage device attached to device slot %d on port %d of controller '%ls'"),
                        aDevice, aControllerPort, aControllerName);

    pAttach->getMedium().queryInterfaceTo(aMedium);

    return S_OK;
}

STDMETHODIMP Machine::GetSerialPort(ULONG slot, ISerialPort **port)
{
    CheckComArgOutPointerValid(port);
    CheckComArgExpr(slot, slot < RT_ELEMENTS(mSerialPorts));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mSerialPorts[slot].queryInterfaceTo(port);

    return S_OK;
}

STDMETHODIMP Machine::GetParallelPort(ULONG slot, IParallelPort **port)
{
    CheckComArgOutPointerValid(port);
    CheckComArgExpr(slot, slot < RT_ELEMENTS(mParallelPorts));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mParallelPorts[slot].queryInterfaceTo(port);

    return S_OK;
}

STDMETHODIMP Machine::GetNetworkAdapter(ULONG slot, INetworkAdapter **adapter)
{
    CheckComArgOutPointerValid(adapter);
    CheckComArgExpr(slot, slot < RT_ELEMENTS(mNetworkAdapters));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mNetworkAdapters[slot].queryInterfaceTo(adapter);

    return S_OK;
}

STDMETHODIMP Machine::GetExtraDataKeys(ComSafeArrayOut(BSTR, aKeys))
{
    if (ComSafeArrayOutIsNull(aKeys))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<BSTR> saKeys(mData->pMachineConfigFile->mapExtraDataItems.size());
    int i = 0;
    for (settings::ExtraDataItemsMap::const_iterator it = mData->pMachineConfigFile->mapExtraDataItems.begin();
         it != mData->pMachineConfigFile->mapExtraDataItems.end();
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
    CheckComArgStrNotEmptyOrNull(aKey);
    CheckComArgOutPointerValid(aValue);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* start with nothing found */
    Bstr bstrResult("");

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    settings::ExtraDataItemsMap::const_iterator it = mData->pMachineConfigFile->mapExtraDataItems.find(Utf8Str(aKey));
    if (it != mData->pMachineConfigFile->mapExtraDataItems.end())
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
    CheckComArgStrNotEmptyOrNull(aKey);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

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
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS); // hold read lock only while looking up
        settings::ExtraDataItemsMap::const_iterator it = mData->pMachineConfigFile->mapExtraDataItems.find(strKey);
        if (it != mData->pMachineConfigFile->mapExtraDataItems.end())
            strOldValue = it->second;
    }

    bool fChanged;
    if ((fChanged = (strOldValue != strValue)))
    {
        // ask for permission from all listeners outside the locks;
        // onExtraDataCanChange() only briefly requests the VirtualBox
        // lock to copy the list of callbacks to invoke
        Bstr error;
        Bstr bstrValue(aValue);

        if (!mParent->onExtraDataCanChange(mData->mUuid, aKey, bstrValue, error))
        {
            const char *sep = error.isEmpty() ? "" : ": ";
            CBSTR err = error.raw();
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
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (getClassID() == clsidSnapshotMachine)
        {
            HRESULT rc = checkStateDependency(MutableStateDep);
            if (FAILED(rc)) return rc;
        }

        if (strValue.isEmpty())
            mData->pMachineConfigFile->mapExtraDataItems.erase(strKey);
        else
            mData->pMachineConfigFile->mapExtraDataItems[strKey] = strValue;
                // creates a new key if needed

        bool fNeedsGlobalSaveSettings = false;
        saveSettings(&fNeedsGlobalSaveSettings);

        if (fNeedsGlobalSaveSettings)
        {
            alock.release();
            AutoWriteLock vboxlock(mParent COMMA_LOCKVAL_SRC_POS);
            mParent->saveSettings();
        }
    }

    // fire notification outside the lock
    if (fChanged)
        mParent->onExtraDataChange(mData->mUuid, aKey, aValue);

    return S_OK;
}

STDMETHODIMP Machine::SaveSettings()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock mlock(this COMMA_LOCKVAL_SRC_POS);

    /* when there was auto-conversion, we want to save the file even if
     * the VM is saved */
    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    /* the settings file path may never be null */
    ComAssertRet(!mData->m_strConfigFileFull.isEmpty(), E_FAIL);

    /* save all VM data excluding snapshots */
    bool fNeedsGlobalSaveSettings = false;
    rc = saveSettings(&fNeedsGlobalSaveSettings);
    mlock.release();

    if (SUCCEEDED(rc) && fNeedsGlobalSaveSettings)
    {
        AutoWriteLock vlock(mParent COMMA_LOCKVAL_SRC_POS);
        rc = mParent->saveSettings();
    }

    return rc;
}

STDMETHODIMP Machine::DiscardSettings()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    /*
     *  during this rollback, the session will be notified if data has
     *  been actually changed
     */
    rollback(true /* aNotify */);

    return S_OK;
}

STDMETHODIMP Machine::DeleteSettings()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    if (mData->mRegistered)
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Cannot delete settings of a registered machine"));

    ULONG uLogHistoryCount = 3;
    ComPtr<ISystemProperties> systemProperties;
    mParent->COMGETTER(SystemProperties)(systemProperties.asOutParam());
    if (!systemProperties.isNull())
        systemProperties->COMGETTER(LogHistoryCount)(&uLogHistoryCount);

    /* delete the settings only when the file actually exists */
    if (mData->pMachineConfigFile->fileExists())
    {
        int vrc = RTFileDelete(mData->m_strConfigFileFull.c_str());
        if (RT_FAILURE(vrc))
            return setError(VBOX_E_IPRT_ERROR,
                            tr("Could not delete the settings file '%s' (%Rrc)"),
                            mData->m_strConfigFileFull.raw(),
                            vrc);

        /* Delete any backup or uncommitted XML files. Ignore failures.
           See the fSafe parameter of xml::XmlFileWriter::write for details. */
        /** @todo Find a way to avoid referring directly to iprt/xml.h here. */
        Utf8Str otherXml = Utf8StrFmt("%s%s", mData->m_strConfigFileFull.c_str(), xml::XmlFileWriter::s_pszTmpSuff);
        RTFileDelete(otherXml.c_str());
        otherXml = Utf8StrFmt("%s%s", mData->m_strConfigFileFull.c_str(), xml::XmlFileWriter::s_pszPrevSuff);
        RTFileDelete(otherXml.c_str());

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
            Utf8Str log = Utf8StrFmt("%s%cVBox.log",
                                     logFolder.raw(), RTPATH_DELIMITER);
            RTFileDelete(log.c_str());
            log = Utf8StrFmt("%s%cVBox.png",
                             logFolder.raw(), RTPATH_DELIMITER);
            RTFileDelete(log.c_str());
            for (int i = uLogHistoryCount; i > 0; i--)
            {
                log = Utf8StrFmt("%s%cVBox.log.%d",
                                 logFolder.raw(), RTPATH_DELIMITER, i);
                RTFileDelete(log.c_str());
                log = Utf8StrFmt("%s%cVBox.png.%d",
                                 logFolder.raw(), RTPATH_DELIMITER, i);
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

STDMETHODIMP Machine::GetSnapshot(IN_BSTR aId, ISnapshot **aSnapshot)
{
    CheckComArgOutPointerValid(aSnapshot);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Guid uuid(aId);
    /* Todo: fix this properly by perhaps introducing an isValid method for the Guid class */
    if (    (aId)
        &&  (*aId != '\0')      // an empty Bstr means "get root snapshot", so don't fail on that
        &&  (uuid.isEmpty()))
    {
        RTUUID uuidTemp;
        /* Either it's a null UUID or the conversion failed. (null uuid has a special meaning in findSnapshot) */
        if (RT_FAILURE(RTUuidFromUtf16(&uuidTemp, aId)))
            return setError(E_FAIL,
                            tr("Could not find a snapshot with UUID {%ls}"),
                            aId);
    }

    ComObjPtr<Snapshot> snapshot;

    HRESULT rc = findSnapshot(uuid, snapshot, true /* aSetError */);
    snapshot.queryInterfaceTo(aSnapshot);

    return rc;
}

STDMETHODIMP Machine::FindSnapshot(IN_BSTR aName, ISnapshot **aSnapshot)
{
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgOutPointerValid(aSnapshot);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<Snapshot> snapshot;

    HRESULT rc = findSnapshot(aName, snapshot, true /* aSetError */);
    snapshot.queryInterfaceTo(aSnapshot);

    return rc;
}

STDMETHODIMP Machine::SetCurrentSnapshot(IN_BSTR /* aId */)
{
    /// @todo (dmik) don't forget to set
    //  mData->mCurrentStateModified to FALSE

    return setError(E_NOTIMPL, "Not implemented");
}

STDMETHODIMP Machine::CreateSharedFolder(IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable)
{
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgStrNotEmptyOrNull(aHostPath);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    ComObjPtr<SharedFolder> sharedFolder;
    rc = findSharedFolder(aName, sharedFolder, false /* aSetError */);
    if (SUCCEEDED(rc))
        return setError(VBOX_E_OBJECT_IN_USE,
                        tr("Shared folder named '%ls' already exists"),
                        aName);

    sharedFolder.createObject();
    rc = sharedFolder->init(getMachine(), aName, aHostPath, aWritable);
    if (FAILED(rc)) return rc;

    setModified(IsModified_SharedFolders);
    mHWData.backup();
    mHWData->mSharedFolders.push_back(sharedFolder);

    /* inform the direct session if any */
    alock.leave();
    onSharedFolderChange();

    return S_OK;
}

STDMETHODIMP Machine::RemoveSharedFolder(IN_BSTR aName)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    ComObjPtr<SharedFolder> sharedFolder;
    rc = findSharedFolder(aName, sharedFolder, true /* aSetError */);
    if (FAILED(rc)) return rc;

    setModified(IsModified_SharedFolders);
    mHWData.backup();
    mHWData->mSharedFolders.remove(sharedFolder);

    /* inform the direct session if any */
    alock.leave();
    onSharedFolderChange();

    return S_OK;
}

STDMETHODIMP Machine::CanShowConsoleWindow(BOOL *aCanShow)
{
    CheckComArgOutPointerValid(aCanShow);

    /* start with No */
    *aCanShow = FALSE;

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

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
    return directControl->OnShowWindow(TRUE /* aCheck */, aCanShow, &dummy);
}

STDMETHODIMP Machine::ShowConsoleWindow(ULONG64 *aWinId)
{
    CheckComArgOutPointerValid(aWinId);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

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
    return directControl->OnShowWindow(FALSE /* aCheck */, &dummy, aWinId);
}

#ifdef VBOX_WITH_GUEST_PROPS
/**
 * Look up a guest property in VBoxSVC's internal structures.
 */
HRESULT Machine::getGuestPropertyFromService(IN_BSTR aName,
                                             BSTR *aValue,
                                             ULONG64 *aTimestamp,
                                             BSTR *aFlags) const
{
    using namespace guestProp;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    Utf8Str strName(aName);
    HWData::GuestPropertyList::const_iterator it;

    for (it = mHWData->mGuestProperties.begin();
         it != mHWData->mGuestProperties.end(); ++it)
    {
        if (it->strName == strName)
        {
            char szFlags[MAX_FLAGS_LEN + 1];
            it->strValue.cloneTo(aValue);
            *aTimestamp = it->mTimestamp;
            writeFlags(it->mFlags, szFlags);
            Bstr(szFlags).cloneTo(aFlags);
            break;
        }
    }
    return S_OK;
}

/**
 * Query the VM that a guest property belongs to for the property.
 * @returns E_ACCESSDENIED if the VM process is not available or not
 *          currently handling queries and the lookup should then be done in
 *          VBoxSVC.
 */
HRESULT Machine::getGuestPropertyFromVM(IN_BSTR aName,
                                        BSTR *aValue,
                                        ULONG64 *aTimestamp,
                                        BSTR *aFlags) const
{
    HRESULT rc;
    ComPtr<IInternalSessionControl> directControl;
    directControl = mData->mSession.mDirectControl;

    /* fail if we were called after #OnSessionEnd() is called.  This is a
     * silly race condition. */

    if (!directControl)
        rc = E_ACCESSDENIED;
    else
        rc = directControl->AccessGuestProperty(aName, NULL, NULL,
                                                false /* isSetter */,
                                                aValue, aTimestamp, aFlags);
    return rc;
}
#endif // VBOX_WITH_GUEST_PROPS

STDMETHODIMP Machine::GetGuestProperty(IN_BSTR aName,
                                       BSTR *aValue,
                                       ULONG64 *aTimestamp,
                                       BSTR *aFlags)
{
#ifndef VBOX_WITH_GUEST_PROPS
    ReturnComNotImplemented();
#else // VBOX_WITH_GUEST_PROPS
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgOutPointerValid(aValue);
    CheckComArgOutPointerValid(aTimestamp);
    CheckComArgOutPointerValid(aFlags);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = getGuestPropertyFromVM(aName, aValue, aTimestamp, aFlags);
    if (rc == E_ACCESSDENIED)
        /* The VM is not running or the service is not (yet) accessible */
        rc = getGuestPropertyFromService(aName, aValue, aTimestamp, aFlags);
    return rc;
#endif // VBOX_WITH_GUEST_PROPS
}

STDMETHODIMP Machine::GetGuestPropertyValue(IN_BSTR aName, BSTR *aValue)
{
    ULONG64 dummyTimestamp;
    BSTR dummyFlags;
    return GetGuestProperty(aName, aValue, &dummyTimestamp, &dummyFlags);
}

STDMETHODIMP Machine::GetGuestPropertyTimestamp(IN_BSTR aName, ULONG64 *aTimestamp)
{
    BSTR dummyValue;
    BSTR dummyFlags;
    return GetGuestProperty(aName, &dummyValue, aTimestamp, &dummyFlags);
}

#ifdef VBOX_WITH_GUEST_PROPS
/**
 * Set a guest property in VBoxSVC's internal structures.
 */
HRESULT Machine::setGuestPropertyToService(IN_BSTR aName, IN_BSTR aValue,
                                           IN_BSTR aFlags)
{
    using namespace guestProp;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = S_OK;
    HWData::GuestProperty property;
    property.mFlags = NILFLAG;
    bool found = false;

    rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    try
    {
        Utf8Str utf8Name(aName);
        Utf8Str utf8Flags(aFlags);
        uint32_t fFlags = NILFLAG;
        if (    (aFlags != NULL)
             && RT_FAILURE(validateFlags(utf8Flags.raw(), &fFlags))
           )
            return setError(E_INVALIDARG,
                            tr("Invalid flag values: '%ls'"),
                            aFlags);

        /** @todo r=bird: see efficiency rant in PushGuestProperty. (Yeah, I
         *                know, this is simple and do an OK job atm.) */
        HWData::GuestPropertyList::iterator it;
        for (it = mHWData->mGuestProperties.begin();
             it != mHWData->mGuestProperties.end(); ++it)
            if (it->strName == utf8Name)
            {
                property = *it;
                if (it->mFlags & (RDONLYHOST))
                    rc = setError(E_ACCESSDENIED,
                                  tr("The property '%ls' cannot be changed by the host"),
                                  aName);
                else
                {
                    setModified(IsModified_MachineData);
                    mHWData.backup();           // @todo r=dj backup in a loop?!?

                    /* The backup() operation invalidates our iterator, so
                    * get a new one. */
                    for (it = mHWData->mGuestProperties.begin();
                         it->strName != utf8Name;
                         ++it)
                        ;
                    mHWData->mGuestProperties.erase(it);
                }
                found = true;
                break;
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
                mHWData->mGuestProperties.push_back(property);
            }
        }
        else if (SUCCEEDED(rc) && *aValue)
        {
            RTTIMESPEC time;
            setModified(IsModified_MachineData);
            mHWData.backup();
            property.strName = aName;
            property.strValue = aValue;
            property.mTimestamp = RTTimeSpecGetNano(RTTimeNow(&time));
            property.mFlags = fFlags;
            mHWData->mGuestProperties.push_back(property);
        }
        if (   SUCCEEDED(rc)
            && (   mHWData->mGuestPropertyNotificationPatterns.isEmpty()
                || RTStrSimplePatternMultiMatch(mHWData->mGuestPropertyNotificationPatterns.raw(), RTSTR_MAX,
                                                utf8Name.raw(), RTSTR_MAX, NULL) )
           )
        {
            /** @todo r=bird: Why aren't we leaving the lock here?  The
             *                same code in PushGuestProperty does... */
            mParent->onGuestPropertyChange(mData->mUuid, aName, aValue, aFlags);
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }

    return rc;
}

/**
 * Set a property on the VM that that property belongs to.
 * @returns E_ACCESSDENIED if the VM process is not available or not
 *          currently handling queries and the setting should then be done in
 *          VBoxSVC.
 */
HRESULT Machine::setGuestPropertyToVM(IN_BSTR aName, IN_BSTR aValue,
                                      IN_BSTR aFlags)
{
    HRESULT rc;

    try {
        ComPtr<IInternalSessionControl> directControl =
            mData->mSession.mDirectControl;

        BSTR dummy = NULL;
        ULONG64 dummy64;
        if (!directControl)
            rc = E_ACCESSDENIED;
        else
            rc = directControl->AccessGuestProperty
                     (aName,
                      /** @todo Fix when adding DeleteGuestProperty(),
                                   see defect. */
                      *aValue ? aValue : NULL, aFlags, true /* isSetter */,
                      &dummy, &dummy64, &dummy);
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }

    return rc;
}
#endif // VBOX_WITH_GUEST_PROPS

STDMETHODIMP Machine::SetGuestProperty(IN_BSTR aName, IN_BSTR aValue,
                                       IN_BSTR aFlags)
{
#ifndef VBOX_WITH_GUEST_PROPS
    ReturnComNotImplemented();
#else // VBOX_WITH_GUEST_PROPS
    CheckComArgStrNotEmptyOrNull(aName);
    if ((aFlags != NULL) && !VALID_PTR(aFlags))
        return E_INVALIDARG;
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = setGuestPropertyToVM(aName, aValue, aFlags);
    if (rc == E_ACCESSDENIED)
        /* The VM is not running or the service is not (yet) accessible */
        rc = setGuestPropertyToService(aName, aValue, aFlags);
    return rc;
#endif // VBOX_WITH_GUEST_PROPS
}

STDMETHODIMP Machine::SetGuestPropertyValue(IN_BSTR aName, IN_BSTR aValue)
{
    return SetGuestProperty(aName, aValue, NULL);
}

#ifdef VBOX_WITH_GUEST_PROPS
/**
 * Enumerate the guest properties in VBoxSVC's internal structures.
 */
HRESULT Machine::enumerateGuestPropertiesInService
                (IN_BSTR aPatterns, ComSafeArrayOut(BSTR, aNames),
                 ComSafeArrayOut(BSTR, aValues),
                 ComSafeArrayOut(ULONG64, aTimestamps),
                 ComSafeArrayOut(BSTR, aFlags))
{
    using namespace guestProp;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    Utf8Str strPatterns(aPatterns);

    /*
     * Look for matching patterns and build up a list.
     */
    HWData::GuestPropertyList propList;
    for (HWData::GuestPropertyList::iterator it = mHWData->mGuestProperties.begin();
         it != mHWData->mGuestProperties.end();
         ++it)
        if (   strPatterns.isEmpty()
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
    SafeArray<BSTR> names(cEntries);
    SafeArray<BSTR> values(cEntries);
    SafeArray<ULONG64> timestamps(cEntries);
    SafeArray<BSTR> flags(cEntries);
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
    return S_OK;
}

/**
 * Enumerate the properties managed by a VM.
 * @returns E_ACCESSDENIED if the VM process is not available or not
 *          currently handling queries and the setting should then be done in
 *          VBoxSVC.
 */
HRESULT Machine::enumerateGuestPropertiesOnVM
                (IN_BSTR aPatterns, ComSafeArrayOut(BSTR, aNames),
                 ComSafeArrayOut(BSTR, aValues),
                 ComSafeArrayOut(ULONG64, aTimestamps),
                 ComSafeArrayOut(BSTR, aFlags))
{
    HRESULT rc;
    ComPtr<IInternalSessionControl> directControl;
    directControl = mData->mSession.mDirectControl;

    if (!directControl)
        rc = E_ACCESSDENIED;
    else
        rc = directControl->EnumerateGuestProperties
                     (aPatterns, ComSafeArrayOutArg(aNames),
                      ComSafeArrayOutArg(aValues),
                      ComSafeArrayOutArg(aTimestamps),
                      ComSafeArrayOutArg(aFlags));
    return rc;
}
#endif // VBOX_WITH_GUEST_PROPS

STDMETHODIMP Machine::EnumerateGuestProperties(IN_BSTR aPatterns,
                                               ComSafeArrayOut(BSTR, aNames),
                                               ComSafeArrayOut(BSTR, aValues),
                                               ComSafeArrayOut(ULONG64, aTimestamps),
                                               ComSafeArrayOut(BSTR, aFlags))
{
#ifndef VBOX_WITH_GUEST_PROPS
    ReturnComNotImplemented();
#else // VBOX_WITH_GUEST_PROPS
    if (!VALID_PTR(aPatterns) && (aPatterns != NULL))
        return E_POINTER;

    CheckComArgOutSafeArrayPointerValid(aNames);
    CheckComArgOutSafeArrayPointerValid(aValues);
    CheckComArgOutSafeArrayPointerValid(aTimestamps);
    CheckComArgOutSafeArrayPointerValid(aFlags);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = enumerateGuestPropertiesOnVM
                     (aPatterns, ComSafeArrayOutArg(aNames),
                      ComSafeArrayOutArg(aValues),
                      ComSafeArrayOutArg(aTimestamps),
                      ComSafeArrayOutArg(aFlags));
    if (rc == E_ACCESSDENIED)
        /* The VM is not running or the service is not (yet) accessible */
        rc = enumerateGuestPropertiesInService
                     (aPatterns, ComSafeArrayOutArg(aNames),
                      ComSafeArrayOutArg(aValues),
                      ComSafeArrayOutArg(aTimestamps),
                      ComSafeArrayOutArg(aFlags));
    return rc;
#endif // VBOX_WITH_GUEST_PROPS
}

STDMETHODIMP Machine::GetMediumAttachmentsOfController(IN_BSTR aName,
                                                       ComSafeArrayOut(IMediumAttachment*, aAttachments))
{
    MediaData::AttachmentList atts;

    HRESULT rc = getMediumAttachmentsOfController(aName, atts);
    if (FAILED(rc)) return rc;

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

    CheckComArgStrNotEmptyOrNull(aControllerName);
    CheckComArgOutPointerValid(aAttachment);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

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
        || (aConnectionType >  StorageBus_SAS))
        return setError(E_INVALIDARG,
                        tr("Invalid connection type: %d"),
                        aConnectionType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    /* try to find one with the name first. */
    ComObjPtr<StorageController> ctrl;

    rc = getStorageControllerByName(aName, ctrl, false /* aSetError */);
    if (SUCCEEDED(rc))
        return setError(VBOX_E_OBJECT_IN_USE,
                        tr("Storage controller named '%ls' already exists"),
                        aName);

    ctrl.createObject();

    /* get a new instance number for the storage controller */
    ULONG ulInstance = 0;
    for (StorageControllerList::const_iterator it = mStorageControllers->begin();
         it != mStorageControllers->end();
         ++it)
    {
        if ((*it)->getStorageBus() == aConnectionType)
        {
            ULONG ulCurInst = (*it)->getInstance();

            if (ulCurInst >= ulInstance)
                ulInstance = ulCurInst + 1;
        }
    }

    rc = ctrl->init(this, aName, aConnectionType, ulInstance);
    if (FAILED(rc)) return rc;

    setModified(IsModified_Storage);
    mStorageControllers.backup();
    mStorageControllers->push_back(ctrl);

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
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<StorageController> ctrl;

    HRESULT rc = getStorageControllerByName(aName, ctrl, true /* aSetError */);
    if (SUCCEEDED(rc))
        ctrl.queryInterfaceTo(aStorageController);

    return rc;
}

STDMETHODIMP Machine::GetStorageControllerByInstance(ULONG aInstance,
                                                     IStorageController **aStorageController)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    for (StorageControllerList::const_iterator it = mStorageControllers->begin();
         it != mStorageControllers->end();
         ++it)
    {
        if ((*it)->getInstance() == aInstance)
        {
            (*it).queryInterfaceTo(aStorageController);
            return S_OK;
        }
    }

    return setError(VBOX_E_OBJECT_NOT_FOUND,
                    tr("Could not find a storage controller with instance number '%lu'"),
                    aInstance);
}

STDMETHODIMP Machine::RemoveStorageController(IN_BSTR aName)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    ComObjPtr<StorageController> ctrl;
    rc = getStorageControllerByName(aName, ctrl, true /* aSetError */);
    if (FAILED(rc)) return rc;

    /* We can remove the controller only if there is no device attached. */
    /* check if the device slot is already busy */
    for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
         it != mMediaData->mAttachments.end();
         ++it)
    {
        if ((*it)->getControllerName() == aName)
            return setError(VBOX_E_OBJECT_IN_USE,
                            tr("Storage controller named '%ls' has still devices attached"),
                            aName);
    }

    /* We can remove it now. */
    setModified(IsModified_Storage);
    mStorageControllers.backup();

    ctrl->unshare();

    mStorageControllers->remove(ctrl);

    /* inform the direct session if any */
    alock.leave();
    onStorageControllerChange();

    return S_OK;
}

/* @todo where is the right place for this? */
#define sSSMDisplayScreenshotVer 0x00010001

static int readSavedDisplayScreenshot(Utf8Str *pStateFilePath, uint32_t u32Type, uint8_t **ppu8Data, uint32_t *pcbData, uint32_t *pu32Width, uint32_t *pu32Height)
{
    LogFlowFunc(("u32Type = %d [%s]\n", u32Type, pStateFilePath->raw()));

    /* @todo cache read data */
    if (pStateFilePath->isEmpty())
    {
        /* No saved state data. */
        return VERR_NOT_SUPPORTED;
    }

    uint8_t *pu8Data = NULL;
    uint32_t cbData = 0;
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;

    PSSMHANDLE pSSM;
    int vrc = SSMR3Open(pStateFilePath->raw(), 0 /*fFlags*/, &pSSM);
    if (RT_SUCCESS(vrc))
    {
        uint32_t uVersion;
        vrc = SSMR3Seek(pSSM, "DisplayScreenshot", 1100 /*iInstance*/, &uVersion);
        if (RT_SUCCESS(vrc))
        {
            if (uVersion == sSSMDisplayScreenshotVer)
            {
                uint32_t cBlocks;
                vrc = SSMR3GetU32(pSSM, &cBlocks);
                AssertRCReturn(vrc, vrc);

                for (uint32_t i = 0; i < cBlocks; i++)
                {
                    uint32_t cbBlock;
                    vrc = SSMR3GetU32(pSSM, &cbBlock);
                    AssertRCBreak(vrc);

                    uint32_t typeOfBlock;
                    vrc = SSMR3GetU32(pSSM, &typeOfBlock);
                    AssertRCBreak(vrc);

                    LogFlowFunc(("[%d] type %d, size %d bytes\n", i, typeOfBlock, cbBlock));

                    if (typeOfBlock == u32Type)
                    {
                        if (cbBlock > 2 * sizeof(uint32_t))
                        {
                            cbData = cbBlock - 2 * sizeof(uint32_t);
                            pu8Data = (uint8_t *)RTMemAlloc(cbData);
                            if (pu8Data == NULL)
                            {
                                vrc = VERR_NO_MEMORY;
                                break;
                            }

                            vrc = SSMR3GetU32(pSSM, &u32Width);
                            AssertRCBreak(vrc);
                            vrc = SSMR3GetU32(pSSM, &u32Height);
                            AssertRCBreak(vrc);
                            vrc = SSMR3GetMem(pSSM, pu8Data, cbData);
                            AssertRCBreak(vrc);
                        }
                        else
                        {
                            /* No saved state data. */
                            vrc = VERR_NOT_SUPPORTED;
                        }

                        break;
                    }
                    else
                    {
                        /* displaySSMSaveScreenshot did not write any data, if
                         * cbBlock was == 2 * sizeof (uint32_t).
                         */
                        if (cbBlock > 2 * sizeof (uint32_t))
                        {
                            vrc = SSMR3Skip(pSSM, cbBlock);
                            AssertRCBreak(vrc);
                        }
                    }
                }
            }
            else
            {
                vrc = VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
            }
        }

        SSMR3Close(pSSM);
    }

    if (RT_SUCCESS(vrc))
    {
        if (u32Type == 0 && cbData % 4 != 0)
        {
            /* Bitmap is 32bpp, so data is invalid. */
            vrc = VERR_SSM_UNEXPECTED_DATA;
        }
    }

    if (RT_SUCCESS(vrc))
    {
        *ppu8Data = pu8Data;
        *pcbData = cbData;
        *pu32Width = u32Width;
        *pu32Height = u32Height;
        LogFlowFunc(("cbData %d, u32Width %d, u32Height %d\n", cbData, u32Width, u32Height));
    }

    LogFlowFunc(("vrc %Rrc\n", vrc));
    return vrc;
}

static void freeSavedDisplayScreenshot(uint8_t *pu8Data)
{
    /* @todo not necessary when caching is implemented. */
    RTMemFree(pu8Data);
}

STDMETHODIMP Machine::QuerySavedThumbnailSize(ULONG *aSize, ULONG *aWidth, ULONG *aHeight)
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(aSize);
    CheckComArgNotNull(aWidth);
    CheckComArgNotNull(aHeight);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint8_t *pu8Data = NULL;
    uint32_t cbData = 0;
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;

    int vrc = readSavedDisplayScreenshot(&mSSData->mStateFilePath, 0 /* u32Type */, &pu8Data, &cbData, &u32Width, &u32Height);

    if (RT_FAILURE(vrc))
        return setError(VBOX_E_IPRT_ERROR,
                        tr("Saved screenshot data is not available (%Rrc)"),
                        vrc);

    *aSize = cbData;
    *aWidth = u32Width;
    *aHeight = u32Height;

    freeSavedDisplayScreenshot(pu8Data);

    return S_OK;
}

STDMETHODIMP Machine::ReadSavedThumbnailToArray(BOOL aBGR, ULONG *aWidth, ULONG *aHeight, ComSafeArrayOut(BYTE, aData))
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(aWidth);
    CheckComArgNotNull(aHeight);
    CheckComArgOutSafeArrayPointerValid(aData);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint8_t *pu8Data = NULL;
    uint32_t cbData = 0;
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;

    int vrc = readSavedDisplayScreenshot(&mSSData->mStateFilePath, 0 /* u32Type */, &pu8Data, &cbData, &u32Width, &u32Height);

    if (RT_FAILURE(vrc))
        return setError(VBOX_E_IPRT_ERROR,
                        tr("Saved screenshot data is not available (%Rrc)"),
                        vrc);

    *aWidth = u32Width;
    *aHeight = u32Height;

    com::SafeArray<BYTE> bitmap(cbData);
    /* Convert pixels to format expected by the API caller. */
    if (aBGR)
    {
        /* [0] B, [1] G, [2] R, [3] A. */
        for (unsigned i = 0; i < cbData; i += 4)
        {
            bitmap[i]     = pu8Data[i];
            bitmap[i + 1] = pu8Data[i + 1];
            bitmap[i + 2] = pu8Data[i + 2];
            bitmap[i + 3] = 0xff;
        }
    }
    else
    {
        /* [0] R, [1] G, [2] B, [3] A. */
        for (unsigned i = 0; i < cbData; i += 4)
        {
            bitmap[i]     = pu8Data[i + 2];
            bitmap[i + 1] = pu8Data[i + 1];
            bitmap[i + 2] = pu8Data[i];
            bitmap[i + 3] = 0xff;
        }
    }
    bitmap.detachTo(ComSafeArrayOutArg(aData));

    freeSavedDisplayScreenshot(pu8Data);

    return S_OK;
}

STDMETHODIMP Machine::QuerySavedScreenshotPNGSize(ULONG *aSize, ULONG *aWidth, ULONG *aHeight)
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(aSize);
    CheckComArgNotNull(aWidth);
    CheckComArgNotNull(aHeight);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint8_t *pu8Data = NULL;
    uint32_t cbData = 0;
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;

    int vrc = readSavedDisplayScreenshot(&mSSData->mStateFilePath, 1 /* u32Type */, &pu8Data, &cbData, &u32Width, &u32Height);

    if (RT_FAILURE(vrc))
        return setError(VBOX_E_IPRT_ERROR,
                        tr("Saved screenshot data is not available (%Rrc)"),
                        vrc);

    *aSize = cbData;
    *aWidth = u32Width;
    *aHeight = u32Height;

    freeSavedDisplayScreenshot(pu8Data);

    return S_OK;
}

STDMETHODIMP Machine::ReadSavedScreenshotPNGToArray(ULONG *aWidth, ULONG *aHeight, ComSafeArrayOut(BYTE, aData))
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(aWidth);
    CheckComArgNotNull(aHeight);
    CheckComArgOutSafeArrayPointerValid(aData);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint8_t *pu8Data = NULL;
    uint32_t cbData = 0;
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;

    int vrc = readSavedDisplayScreenshot(&mSSData->mStateFilePath, 1 /* u32Type */, &pu8Data, &cbData, &u32Width, &u32Height);

    if (RT_FAILURE(vrc))
        return setError(VBOX_E_IPRT_ERROR,
                        tr("Saved screenshot data is not available (%Rrc)"),
                        vrc);

    *aWidth = u32Width;
    *aHeight = u32Height;

    com::SafeArray<BYTE> png(cbData);
    for (unsigned i = 0; i < cbData; i++)
        png[i] = pu8Data[i];
    png.detachTo(ComSafeArrayOutArg(aData));

    freeSavedDisplayScreenshot(pu8Data);

    return S_OK;
}

STDMETHODIMP Machine::HotPlugCPU(ULONG aCpu)
{
    HRESULT rc = S_OK;
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mHWData->mCPUHotPlugEnabled)
        return setError(E_INVALIDARG, tr("CPU hotplug is not enabled"));

    if (aCpu >= mHWData->mCPUCount)
        return setError(E_INVALIDARG, tr("CPU id exceeds number of possible CPUs [0:%lu]"), mHWData->mCPUCount-1);

    if (mHWData->mCPUAttached[aCpu])
        return setError(VBOX_E_OBJECT_IN_USE, tr("CPU %lu is already attached"), aCpu);

    alock.leave();
    rc = onCPUChange(aCpu, false);
    alock.enter();
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mCPUAttached[aCpu] = true;

    /* Save settings if online */
    if (Global::IsOnline(mData->mMachineState))
        SaveSettings();

    return S_OK;
}

STDMETHODIMP Machine::HotUnplugCPU(ULONG aCpu)
{
    HRESULT rc = S_OK;
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mHWData->mCPUHotPlugEnabled)
        return setError(E_INVALIDARG, tr("CPU hotplug is not enabled"));

    if (aCpu >= SchemaDefs::MaxCPUCount)
        return setError(E_INVALIDARG,
                        tr("CPU index exceeds maximum CPU count (must be in range [0:%lu])"),
                        SchemaDefs::MaxCPUCount);

    if (!mHWData->mCPUAttached[aCpu])
        return setError(VBOX_E_OBJECT_NOT_FOUND, tr("CPU %lu is not attached"), aCpu);

    /* CPU 0 can't be detached */
    if (aCpu == 0)
        return setError(E_INVALIDARG, tr("It is not possible to detach CPU 0"));

    alock.leave();
    rc = onCPUChange(aCpu, true);
    alock.enter();
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mCPUAttached[aCpu] = false;

    /* Save settings if online */
    if (Global::IsOnline(mData->mMachineState))
        SaveSettings();

    return S_OK;
}

STDMETHODIMP Machine::GetCPUStatus(ULONG aCpu, BOOL *aCpuAttached)
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(aCpuAttached);

    *aCpuAttached = false;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* If hotplug is enabled the CPU is always enabled. */
    if (!mHWData->mCPUHotPlugEnabled)
    {
        if (aCpu < mHWData->mCPUCount)
            *aCpuAttached = true;
    }
    else
    {
        if (aCpu < SchemaDefs::MaxCPUCount)
            *aCpuAttached = mHWData->mCPUAttached[aCpu];
    }

    return S_OK;
}

STDMETHODIMP Machine::QueryLogFilename(ULONG aIdx, BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str log = queryLogFilename(aIdx);
    if (RTFileExists(log.c_str()))
        log.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP Machine::ReadLog(ULONG aIdx, ULONG64 aOffset, ULONG64 aSize, ComSafeArrayOut(BYTE, aData))
{
    LogFlowThisFunc(("\n"));
    CheckComArgOutSafeArrayPointerValid(aData);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    Utf8Str log = queryLogFilename(aIdx);

    /* do not unnecessarily hold the lock while doing something which does
     * not need the lock and potentially takes a long time. */
    alock.release();

    size_t cbData = (size_t)RT_MIN(aSize, 2048);
    com::SafeArray<BYTE> logData(cbData);

    RTFILE LogFile;
    int vrc = RTFileOpen(&LogFile, log.raw(),
                         RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(vrc))
    {
        vrc = RTFileReadAt(LogFile, aOffset, logData.raw(), cbData, &cbData);
        if (RT_SUCCESS(vrc))
            logData.resize(cbData);
        else
            rc = setError(VBOX_E_IPRT_ERROR,
                          tr("Could not read log file '%s' (%Rrc)"),
                          log.raw(), vrc);
    }
    else
        rc = setError(VBOX_E_IPRT_ERROR,
                      tr("Could not open log file '%s' (%Rrc)"),
                      log.raw(), vrc);

    if (FAILED(rc))
        logData.resize(0);
    logData.detachTo(ComSafeArrayOutArg(aData));

    return rc;
}


// public methods for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Adds the given IsModified_* flag to the dirty flags of the machine.
 * This must be called either during loadSettings or under the machine write lock.
 * @param fl
 */
void Machine::setModified(uint32_t fl)
{
    mData->flModifications |= fl;
}

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

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

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
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(!mData->m_strConfigFileFull.isEmpty(), VERR_GENERAL_FAILURE);

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
    AssertComRCReturn(autoCaller.rc(), (void)0);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturnVoid(!mData->m_strConfigFileFull.isEmpty());

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
void Machine::getLogFolder(Utf8Str &aLogFolder)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str settingsDir;
    if (isInOwnDir(&settingsDir))
    {
        /* Log folder is <Machines>/<VM_Name>/Logs */
        aLogFolder = Utf8StrFmt("%s%cLogs", settingsDir.raw(), RTPATH_DELIMITER);
    }
    else
    {
        /* Log folder is <Machines>/<VM_SnapshotFolder>/Logs */
        Assert(!mUserData->mSnapshotFolderFull.isEmpty());
        aLogFolder = Utf8StrFmt ("%ls%cLogs", mUserData->mSnapshotFolderFull.raw(),
                                 RTPATH_DELIMITER);
    }
}

/**
 *  Returns the full path to the machine's log file for an given index.
 */
Utf8Str Machine::queryLogFilename(ULONG idx)
{
    Utf8Str logFolder;
    getLogFolder(logFolder);
    Assert(logFolder.length());
    Utf8Str log;
    if (idx == 0)
        log = Utf8StrFmt("%s%cVBox.log",
                         logFolder.raw(), RTPATH_DELIMITER);
    else
        log = Utf8StrFmt("%s%cVBox.log.%d",
                         logFolder.raw(), RTPATH_DELIMITER, idx);
    return log;
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
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

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

            Assert(mData->mSession.mRemoteControls.size() == 1);
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
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

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
    AssertReturn(!Global::IsOnlineOrTransient(mData->mMachineState), E_FAIL);

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
                        char *val = strchr(var, '=');
                        if (val)
                        {
                            *val++ = '\0';
                            vrc2 = RTEnvSetEx(env, var, val);
                        }
                        else
                            vrc2 = RTEnvUnsetEx(env, var);
                        if (RT_FAILURE(vrc2))
                            break;
                    }
                    var = p + 1;
                }
            }
            if (RT_SUCCESS(vrc2) && *var)
                vrc2 = RTEnvPutEx(env, var);

            AssertRCBreakStmt(vrc2, vrc = vrc2);
        }
        while (0);

        if (newEnvStr != NULL)
            RTStrFree(newEnvStr);
    }

    Utf8Str strType(aType);

    /* Qt is default */
#ifdef VBOX_WITH_QTGUI
    if (strType == "gui" || strType == "GUI/Qt")
    {
# ifdef RT_OS_DARWIN /* Avoid Launch Services confusing this with the selector by using a helper app. */
        const char VirtualBox_exe[] = "../Resources/VirtualBoxVM.app/Contents/MacOS/VirtualBoxVM";
# else
        const char VirtualBox_exe[] = "VirtualBox" HOSTSUFF_EXE;
# endif
        Assert(sz >= sizeof(VirtualBox_exe));
        strcpy(cmd, VirtualBox_exe);

        Utf8Str idStr = mData->mUuid.toString();
        Utf8Str strName = mUserData->mName;
        const char * args[] = {szPath, "--comment", strName.c_str(), "--startvm", idStr.c_str(), "--no-startvm-errormsgbox", 0 };
        vrc = RTProcCreate(szPath, args, env, 0, &pid);
    }
#else /* !VBOX_WITH_QTGUI */
    if (0)
        ;
#endif /* VBOX_WITH_QTGUI */

    else

#ifdef VBOX_WITH_VBOXSDL
    if (strType == "sdl" || strType == "GUI/SDL")
    {
        const char VBoxSDL_exe[] = "VBoxSDL" HOSTSUFF_EXE;
        Assert(sz >= sizeof(VBoxSDL_exe));
        strcpy(cmd, VBoxSDL_exe);

        Utf8Str idStr = mData->mUuid.toString();
        Utf8Str strName = mUserData->mName;
        const char * args[] = {szPath, "--comment", strName.c_str(), "--startvm", idStr.c_str(), 0 };
        vrc = RTProcCreate(szPath, args, env, 0, &pid);
    }
#else /* !VBOX_WITH_VBOXSDL */
    if (0)
        ;
#endif /* !VBOX_WITH_VBOXSDL */

    else

#ifdef VBOX_WITH_HEADLESS
    if (   strType == "headless"
        || strType == "capture"
#ifdef VBOX_WITH_VRDP
        || strType == "vrdp"
#endif
       )
    {
        const char VBoxHeadless_exe[] = "VBoxHeadless" HOSTSUFF_EXE;
        Assert(sz >= sizeof(VBoxHeadless_exe));
        strcpy(cmd, VBoxHeadless_exe);

        Utf8Str idStr = mData->mUuid.toString();
        /* Leave space for 2 args, as "headless" needs --vrdp off on non-OSE. */
        Utf8Str strName = mUserData->mName;
        const char * args[] = {szPath, "--comment", strName.c_str(), "--startvm", idStr.c_str(), 0, 0, 0 };
#ifdef VBOX_WITH_VRDP
        if (strType == "headless")
        {
            unsigned pos = RT_ELEMENTS(args) - 3;
            args[pos++] = "--vrdp";
            args[pos] = "off";
        }
#endif
        if (strType == "capture")
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
        RTEnvDestroy(env);
        return setError(E_INVALIDARG,
                        tr("Invalid session type: '%s'"),
                        strType.c_str());
    }

    RTEnvDestroy(env);

    if (RT_FAILURE(vrc))
        return setError(VBOX_E_IPRT_ERROR,
                        tr("Could not launch a process for the machine '%ls' (%Rrc)"),
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
    HRESULT rc = aControl->AssignMachine(NULL);
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
    Assert(mData->mSession.mPid == NIL_RTPROCESS);
    mData->mSession.mRemoteControls.push_back (aControl);
    mData->mSession.mProgress = aProgress;
    mData->mSession.mPid = pid;
    mData->mSession.mState = SessionState_Spawning;
    mData->mSession.mType = strType;

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  @note Locks this object for writing, calls the client process
 *        (outside the lock).
 */
HRESULT Machine::openExistingSession(IInternalSessionControl *aControl)
{
    LogFlowThisFuncEnter();

    AssertReturn(aControl, E_FAIL);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mData->mRegistered)
        return setError(E_UNEXPECTED,
                        tr("The machine '%ls' is not registered"),
                        mUserData->mName.raw());

    LogFlowThisFunc(("mSession.state=%s\n", Global::stringifySessionState(mData->mSession.mState)));

    if (mData->mSession.mState != SessionState_Open)
        return setError(VBOX_E_INVALID_SESSION_STATE,
                        tr("The machine '%ls' does not have an open session"),
                        mUserData->mName.raw());

    ComAssertRet(!mData->mSession.mDirectControl.isNull(), E_FAIL);

    // copy member variables before leaving lock
    ComPtr<IInternalSessionControl> pDirectControl = mData->mSession.mDirectControl;
    ComObjPtr<SessionMachine> pSessionMachine = mData->mSession.mMachine;
    AssertReturn(!pSessionMachine.isNull(), E_FAIL);

    /*
     *  Leave the lock before calling the client process. It's safe here
     *  since the only thing to do after we get the lock again is to add
     *  the remote control to the list (which doesn't directly influence
     *  anything).
     */
    alock.leave();

    // get the console from the direct session (this is a remote call)
    ComPtr<IConsole> pConsole;
    LogFlowThisFunc(("Calling GetRemoteConsole()...\n"));
    HRESULT rc = pDirectControl->GetRemoteConsole(pConsole.asOutParam());
    LogFlowThisFunc(("GetRemoteConsole() returned %08X\n", rc));
    if (FAILED (rc))
        /* The failure may occur w/o any error info (from RPC), so provide one */
        return setError(VBOX_E_VM_ERROR,
            tr("Failed to get a console object from the direct session (%Rrc)"), rc);

    ComAssertRet(!pConsole.isNull(), E_FAIL);

    /* attach the remote session to the machine */
    LogFlowThisFunc(("Calling AssignRemoteMachine()...\n"));
    rc = aControl->AssignRemoteMachine(pSessionMachine, pConsole);
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
    mData->mSession.mRemoteControls.push_back(aControl);

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
#if defined(RT_OS_WINDOWS)
bool Machine::isSessionOpen(ComObjPtr<SessionMachine> &aMachine,
                            ComPtr<IInternalSessionControl> *aControl /*= NULL*/,
                            HANDLE *aIPCSem /*= NULL*/,
                            bool aAllowClosing /*= false*/)
#elif defined(RT_OS_OS2)
bool Machine::isSessionOpen(ComObjPtr<SessionMachine> &aMachine,
                            ComPtr<IInternalSessionControl> *aControl /*= NULL*/,
                            HMTX *aIPCSem /*= NULL*/,
                            bool aAllowClosing /*= false*/)
#else
bool Machine::isSessionOpen(ComObjPtr<SessionMachine> &aMachine,
                            ComPtr<IInternalSessionControl> *aControl /*= NULL*/,
                            bool aAllowClosing /*= false*/)
#endif
{
    AutoLimitedCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), false);

    /* just return false for inaccessible machines */
    if (autoCaller.state() != Ready)
        return false;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mSession.mState == SessionState_Open ||
        (aAllowClosing && mData->mSession.mState == SessionState_Closing))
    {
        AssertReturn(!mData->mSession.mMachine.isNull(), false);

        aMachine = mData->mSession.mMachine;

        if (aControl != NULL)
            *aControl = mData->mSession.mDirectControl;

#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
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
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
bool Machine::isSessionSpawning(RTPROCESS *aPID /*= NULL*/)
#else
bool Machine::isSessionSpawning()
#endif
{
    AutoLimitedCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), false);

    /* just return false for inaccessible machines */
    if (autoCaller.state() != Ready)
        return false;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mSession.mState == SessionState_Spawning)
    {
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
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
    AutoMultiWriteLock2 alock(mParent, this COMMA_LOCKVAL_SRC_POS);

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
                  getName().raw());
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
                      getName().raw());
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

        /* finalize the progress after setting the state */
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
HRESULT Machine::trySetRegistered(BOOL argNewRegistered)
{
    AssertReturn(mParent->isWriteLockOnCurrentThread(), E_FAIL);

    AutoLimitedCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* wait for state dependants to drop to zero */
    ensureNoStateDependencies();

    ComAssertRet(mData->mRegistered != argNewRegistered, E_FAIL);

    if (!mData->mAccessible)
    {
        /* A special case: the machine is not accessible. */

        /* inaccessible machines can only be unregistered */
        AssertReturn(!argNewRegistered, E_FAIL);

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

    if (argNewRegistered)
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

    // Ensure the settings are saved. If we are going to be registered and
    // no config file exists yet, create it by calling saveSettings() too.
    if (    (mData->flModifications)
         || (argNewRegistered && !mData->pMachineConfigFile->fileExists())
       )
    {
        rc = saveSettings(NULL);
                // no need to check whether VirtualBox.xml needs saving too since
                // we can't have a machine XML file rename pending
        if (FAILED(rc)) return rc;
    }

    /* more config checking goes here */

    if (SUCCEEDED(rc))
    {
        /* we may have had implicit modifications we want to fix on success */
        commit();

        mData->mRegistered = argNewRegistered;
    }
    else
    {
        /* we may have had implicit modifications we want to cancel on failure*/
        rollback(false /* aNotify */);
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
HRESULT Machine::addStateDependency(StateDependency aDepType /* = AnyStateDep */,
                                    MachineState_T *aState /* = NULL */,
                                    BOOL *aRegistered /* = NULL */)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(aDepType);
    if (FAILED(rc)) return rc;

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
        Assert(mData->mMachineStateDeps != 0 /* overflow */);
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
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* releaseStateDependency() w/o addStateDependency()? */
    AssertReturnVoid(mData->mMachineStateDeps != 0);
    -- mData->mMachineStateDeps;

    if (mData->mMachineStateDeps == 0)
    {
        /* inform ensureNoStateDependencies() that there are no more deps */
        if (mData->mMachineStateChangePending != 0)
        {
            Assert(mData->mMachineStateDepsSem != NIL_RTSEMEVENTMULTI);
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
            if (   mData->mRegistered
                && (   getClassID() != clsidSessionMachine  /** @todo This was just convered raw; Check if Running and Paused should actually be included here... (Live Migration) */
                    || (   mData->mMachineState != MachineState_Paused
                        && mData->mMachineState != MachineState_Running
                        && mData->mMachineState != MachineState_Aborted
                        && mData->mMachineState != MachineState_Teleported
                        && mData->mMachineState != MachineState_PoweredOff
                       )
                   )
               )
                return setError(VBOX_E_INVALID_VM_STATE,
                                tr("The machine is not mutable (state is %s)"),
                                Global::stringifyMachineState(mData->mMachineState));
            break;
        }
        case MutableOrSavedStateDep:
        {
            if (   mData->mRegistered
                && (   getClassID() != clsidSessionMachine  /** @todo This was just convered raw; Check if Running and Paused should actually be included here... (Live Migration) */
                    || (   mData->mMachineState != MachineState_Paused
                        && mData->mMachineState != MachineState_Running
                        && mData->mMachineState != MachineState_Aborted
                        && mData->mMachineState != MachineState_Teleported
                        && mData->mMachineState != MachineState_Saved
                        && mData->mMachineState != MachineState_PoweredOff
                       )
                   )
               )
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
    AssertComRCReturn(autoCaller.state() == InInit ||
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
    mBIOSSettings->init(this);

#ifdef VBOX_WITH_VRDP
    /* create an associated VRDPServer object (default is disabled) */
    unconst(mVRDPServer).createObject();
    mVRDPServer->init(this);
#endif

    /* create associated serial port objects */
    for (ULONG slot = 0; slot < RT_ELEMENTS(mSerialPorts); slot++)
    {
        unconst(mSerialPorts[slot]).createObject();
        mSerialPorts[slot]->init(this, slot);
    }

    /* create associated parallel port objects */
    for (ULONG slot = 0; slot < RT_ELEMENTS(mParallelPorts); slot++)
    {
        unconst(mParallelPorts[slot]).createObject();
        mParallelPorts[slot]->init(this, slot);
    }

    /* create the audio adapter object (always present, default is disabled) */
    unconst(mAudioAdapter).createObject();
    mAudioAdapter->init(this);

    /* create the USB controller object (always present, default is disabled) */
    unconst(mUSBController).createObject();
    mUSBController->init(this);

    /* create associated network adapter objects */
    for (ULONG slot = 0; slot < RT_ELEMENTS(mNetworkAdapters); slot ++)
    {
        unconst(mNetworkAdapters[slot]).createObject();
        mNetworkAdapters[slot]->init(this, slot);
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
    AssertComRCReturnVoid(autoCaller.rc());
    AssertComRCReturnVoid(    autoCaller.state() == InUninit
                           || autoCaller.state() == Limited);

    /* uninit all children using addDependentChild()/removeDependentChild()
     * in their init()/uninit() methods */
    uninitDependentChildren();

    /* tell all our other child objects we've been uninitialized */

    for (ULONG slot = 0; slot < RT_ELEMENTS(mNetworkAdapters); slot++)
    {
        if (mNetworkAdapters[slot])
        {
            mNetworkAdapters[slot]->uninit();
            unconst(mNetworkAdapters[slot]).setNull();
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

    for (ULONG slot = 0; slot < RT_ELEMENTS(mParallelPorts); slot++)
    {
        if (mParallelPorts[slot])
        {
            mParallelPorts[slot]->uninit();
            unconst(mParallelPorts[slot]).setNull();
        }
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS(mSerialPorts); slot++)
    {
        if (mSerialPorts[slot])
        {
            mSerialPorts[slot]->uninit();
            unconst(mSerialPorts[slot]).setNull();
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
     * a result of unregistering or deleting the snapshot), outdated hard
     * disk attachments will already be uninitialized and deleted, so this
     * code will not affect them. */
    VBoxClsID clsid = getClassID();
    if (    !!mMediaData
         && (clsid == clsidMachine || clsid == clsidSnapshotMachine)
       )
    {
        for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
             it != mMediaData->mAttachments.end();
             ++it)
        {
            ComObjPtr<Medium> hd = (*it)->getMedium();
            if (hd.isNull())
                continue;
            HRESULT rc = hd->detachFrom(mData->mUuid, getSnapshotId());
            AssertComRC(rc);
        }
    }

    if (getClassID() == clsidMachine)
    {
        // clean up the snapshots list (Snapshot::uninit() will handle the snapshot's children recursively)
        if (mData->mFirstSnapshot)
        {
            // snapshots tree is protected by media write lock; strictly
            // this isn't necessary here since we're deleting the entire
            // machine, but otherwise we assert in Snapshot::uninit()
            AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
            mData->mFirstSnapshot->uninit();
            mData->mFirstSnapshot.setNull();
        }

        mData->mCurrentSnapshot.setNull();
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
 *  Returns a pointer to the Machine object for this machine that acts like a
 *  parent for complex machine data objects such as shared folders, etc.
 *
 *  For primary Machine objects and for SnapshotMachine objects, returns this
 *  object's pointer itself. For SessoinMachine objects, returns the peer
 *  (primary) machine pointer.
 */
Machine* Machine::getMachine()
{
    if (getClassID() == clsidSessionMachine)
        return (Machine*)mPeer;
    return this;
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
    AssertReturnVoid(isWriteLockOnCurrentThread());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Wait for all state dependants if necessary */
    if (mData->mMachineStateDeps != 0)
    {
        /* lazy semaphore creation */
        if (mData->mMachineStateDepsSem == NIL_RTSEMEVENTMULTI)
            RTSemEventMultiCreate(&mData->mMachineStateDepsSem);

        LogFlowThisFunc(("Waiting for state deps (%d) to drop to zero...\n",
                          mData->mMachineStateDeps));

        ++mData->mMachineStateChangePending;

        /* reset the semaphore before waiting, the last dependant will signal
         * it */
        RTSemEventMultiReset(mData->mMachineStateDepsSem);

        alock.leave();

        RTSemEventMultiWait(mData->mMachineStateDepsSem, RT_INDEFINITE_WAIT);

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
HRESULT Machine::setMachineState(MachineState_T aMachineState)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aMachineState=%s\n", Global::stringifyMachineState(aMachineState) ));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* wait for state dependants to drop to zero */
    ensureNoStateDependencies();

    if (mData->mMachineState != aMachineState)
    {
        mData->mMachineState = aMachineState;

        RTTimeNow(&mData->mLastStateChange);

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
HRESULT Machine::findSharedFolder(CBSTR aName,
                                  ComObjPtr<SharedFolder> &aSharedFolder,
                                  bool aSetError /* = false */)
{
    bool found = false;
    for (HWData::SharedFolderList::const_iterator it = mHWData->mSharedFolders.begin();
        !found && it != mHWData->mSharedFolders.end();
        ++it)
    {
        AutoWriteLock alock(*it COMMA_LOCKVAL_SRC_POS);
        found = (*it)->getName() == aName;
        if (found)
            aSharedFolder = *it;
    }

    HRESULT rc = found ? S_OK : VBOX_E_OBJECT_NOT_FOUND;

    if (aSetError && !found)
        setError(rc, tr("Could not find a shared folder named '%ls'"), aName);

    return rc;
}

/**
 * Initializes all machine instance data from the given settings structures
 * from XML. The exception is the machine UUID which needs special handling
 * depending on the caller's use case, so the caller needs to set that herself.
 *
 * @param config
 * @param fAllowStorage
 */
HRESULT Machine::loadMachineDataFromSettings(const settings::MachineConfigFile &config)
{
    /* name (required) */
    mUserData->mName = config.strName;

    /* nameSync (optional, default is true) */
    mUserData->mNameSync = config.fNameSync;

    mUserData->mDescription = config.strDescription;

    // guest OS type
    mUserData->mOSTypeId = config.strOsType;
    /* look up the object by Id to check it is valid */
    ComPtr<IGuestOSType> guestOSType;
    HRESULT rc = mParent->GetGuestOSType(mUserData->mOSTypeId,
                                         guestOSType.asOutParam());
    if (FAILED(rc)) return rc;

    // stateFile (optional)
    if (config.strStateFile.isEmpty())
        mSSData->mStateFilePath.setNull();
    else
    {
        Utf8Str stateFilePathFull(config.strStateFile);
        int vrc = calculateFullPath(stateFilePathFull, stateFilePathFull);
        if (RT_FAILURE(vrc))
            return setError(E_FAIL,
                            tr("Invalid saved state file path '%s' (%Rrc)"),
                            config.strStateFile.raw(),
                            vrc);
        mSSData->mStateFilePath = stateFilePathFull;
    }

    /* snapshotFolder (optional) */
    rc = COMSETTER(SnapshotFolder)(Bstr(config.strSnapshotFolder));
    if (FAILED(rc)) return rc;

    /* currentStateModified (optional, default is true) */
    mData->mCurrentStateModified = config.fCurrentStateModified;

    mData->mLastStateChange = config.timeLastStateChange;

    /* teleportation */
    mUserData->mTeleporterEnabled  = config.fTeleporterEnabled;
    mUserData->mTeleporterPort     = config.uTeleporterPort;
    mUserData->mTeleporterAddress  = config.strTeleporterAddress;
    mUserData->mTeleporterPassword = config.strTeleporterPassword;

    /* RTC */
    mUserData->mRTCUseUTC = config.fRTCUseUTC;

    /*
     *  note: all mUserData members must be assigned prior this point because
     *  we need to commit changes in order to let mUserData be shared by all
     *  snapshot machine instances.
     */
    mUserData.commitCopy();

    /* Snapshot node (optional) */
    size_t cRootSnapshots;
    if ((cRootSnapshots = config.llFirstSnapshot.size()))
    {
        // there must be only one root snapshot
        Assert(cRootSnapshots == 1);

        const settings::Snapshot &snap = config.llFirstSnapshot.front();

        rc = loadSnapshot(snap,
                          config.uuidCurrentSnapshot,
                          NULL);        // no parent == first snapshot
        if (FAILED(rc)) return rc;
    }

    /* Hardware node (required) */
    rc = loadHardware(config.hardwareMachine);
    if (FAILED(rc)) return rc;

    /* Load storage controllers */
    rc = loadStorageControllers(config.storageMachine);
    if (FAILED(rc)) return rc;

    /*
        *  NOTE: the assignment below must be the last thing to do,
        *  otherwise it will be not possible to change the settings
        *  somewehere in the code above because all setters will be
        *  blocked by checkStateDependency(MutableStateDep).
        */

    /* set the machine state to Aborted or Saved when appropriate */
    if (config.fAborted)
    {
        Assert(!mSSData->mStateFilePath.isEmpty());
        mSSData->mStateFilePath.setNull();

        /* no need to use setMachineState() during init() */
        mData->mMachineState = MachineState_Aborted;
    }
    else if (!mSSData->mStateFilePath.isEmpty())
    {
        /* no need to use setMachineState() during init() */
        mData->mMachineState = MachineState_Saved;
    }

    // after loading settings, we are no longer different from the XML on disk
    mData->flModifications = 0;

    return S_OK;
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
    AssertReturn(getClassID() == clsidMachine, E_FAIL);

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
    if (FAILED(rc)) return rc;

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
    if (FAILED(rc)) return rc;

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
        if (FAILED(rc)) return rc;
    }

    return rc;
}

/**
 *  @param aNode    <Hardware> node.
 */
HRESULT Machine::loadHardware(const settings::Hardware &data)
{
    AssertReturn(getClassID() == clsidMachine || getClassID() == clsidSnapshotMachine, E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        /* The hardware version attribute (optional). */
        mHWData->mHWVersion = data.strVersion;
        mHWData->mHardwareUUID = data.uuid;

        mHWData->mHWVirtExEnabled             = data.fHardwareVirt;
        mHWData->mHWVirtExExclusive           = data.fHardwareVirtExclusive;
        mHWData->mHWVirtExNestedPagingEnabled = data.fNestedPaging;
        mHWData->mHWVirtExLargePagesEnabled   = data.fLargePages;
        mHWData->mHWVirtExVPIDEnabled         = data.fVPID;
        mHWData->mPAEEnabled                  = data.fPAE;
        mHWData->mSyntheticCpu                = data.fSyntheticCpu;

        mHWData->mCPUCount          = data.cCPUs;
        mHWData->mCPUHotPlugEnabled = data.fCpuHotPlug;

        // cpu
        if (mHWData->mCPUHotPlugEnabled)
        {
            for (settings::CpuList::const_iterator it = data.llCpus.begin();
                it != data.llCpus.end();
                ++it)
            {
                const settings::Cpu &cpu = *it;

                mHWData->mCPUAttached[cpu.ulId] = true;
            }
        }

        // cpuid leafs
        for (settings::CpuIdLeafsList::const_iterator it = data.llCpuIdLeafs.begin();
            it != data.llCpuIdLeafs.end();
            ++it)
        {
            const settings::CpuIdLeaf &leaf = *it;

            switch (leaf.ulId)
            {
            case 0x0:
            case 0x1:
            case 0x2:
            case 0x3:
            case 0x4:
            case 0x5:
            case 0x6:
            case 0x7:
            case 0x8:
            case 0x9:
            case 0xA:
                mHWData->mCpuIdStdLeafs[leaf.ulId] = leaf;
                break;

            case 0x80000000:
            case 0x80000001:
            case 0x80000002:
            case 0x80000003:
            case 0x80000004:
            case 0x80000005:
            case 0x80000006:
            case 0x80000007:
            case 0x80000008:
            case 0x80000009:
            case 0x8000000A:
                mHWData->mCpuIdExtLeafs[leaf.ulId - 0x80000000] = leaf;
                break;

            default:
                /* just ignore */
                break;
            }
        }

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
        mHWData->mPointingHidType = data.pointingHidType;
        mHWData->mKeyboardHidType = data.keyboardHidType;
        mHWData->mHpetEnabled = data.fHpetEnabled;

#ifdef VBOX_WITH_VRDP
        /* RemoteDisplay */
        rc = mVRDPServer->loadSettings(data.vrdpSettings);
        if (FAILED(rc)) return rc;
#endif

        /* BIOS */
        rc = mBIOSSettings->loadSettings(data.biosSettings);
        if (FAILED(rc)) return rc;

        /* USB Controller */
        rc = mUSBController->loadSettings(data.usbController);
        if (FAILED(rc)) return rc;

        // network adapters
        for (settings::NetworkAdaptersList::const_iterator it = data.llNetworkAdapters.begin();
            it != data.llNetworkAdapters.end();
            ++it)
        {
            const settings::NetworkAdapter &nic = *it;

            /* slot unicity is guaranteed by XML Schema */
            AssertBreak(nic.ulSlot < RT_ELEMENTS(mNetworkAdapters));
            rc = mNetworkAdapters[nic.ulSlot]->loadSettings(nic);
            if (FAILED(rc)) return rc;
        }

        // serial ports
        for (settings::SerialPortsList::const_iterator it = data.llSerialPorts.begin();
            it != data.llSerialPorts.end();
            ++it)
        {
            const settings::SerialPort &s = *it;

            AssertBreak(s.ulSlot < RT_ELEMENTS(mSerialPorts));
            rc = mSerialPorts[s.ulSlot]->loadSettings(s);
            if (FAILED(rc)) return rc;
        }

        // parallel ports (optional)
        for (settings::ParallelPortsList::const_iterator it = data.llParallelPorts.begin();
            it != data.llParallelPorts.end();
            ++it)
        {
            const settings::ParallelPort &p = *it;

            AssertBreak(p.ulSlot < RT_ELEMENTS(mParallelPorts));
            rc = mParallelPorts[p.ulSlot]->loadSettings(p);
            if (FAILED(rc)) return rc;
        }

        /* AudioAdapter */
        rc = mAudioAdapter->loadSettings(data.audioAdapter);
        if (FAILED(rc)) return rc;

        for (settings::SharedFoldersList::const_iterator it = data.llSharedFolders.begin();
             it != data.llSharedFolders.end();
             ++it)
        {
            const settings::SharedFolder &sf = *it;
            rc = CreateSharedFolder(Bstr(sf.strName), Bstr(sf.strHostPath), sf.fWritable);
            if (FAILED(rc)) return rc;
        }

        // Clipboard
        mHWData->mClipboardMode = data.clipboardMode;

        // guest settings
        mHWData->mMemoryBalloonSize = data.ulMemoryBalloonSize;

        // IO settings
        mHWData->mIoMgrType = data.ioSettings.ioMgrType;
        mHWData->mIoBackendType = data.ioSettings.ioBackendType;
        mHWData->mIoCacheEnabled = data.ioSettings.fIoCacheEnabled;
        mHWData->mIoCacheSize = data.ioSettings.ulIoCacheSize;
        mHWData->mIoBandwidthMax = data.ioSettings.ulIoBandwidthMax;

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
                                        const Guid *aSnapshotId /* = NULL */)
{
    AssertReturn(getClassID() == clsidMachine || getClassID() == clsidSnapshotMachine, E_FAIL);

    HRESULT rc = S_OK;

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
                        ctlData.storageBus,
                        ctlData.ulInstance);
        if (FAILED(rc)) return rc;

        mStorageControllers->push_back(pCtl);

        rc = pCtl->COMSETTER(ControllerType)(ctlData.controllerType);
        if (FAILED(rc)) return rc;

        rc = pCtl->COMSETTER(PortCount)(ctlData.ulPortCount);
        if (FAILED(rc)) return rc;

        rc = pCtl->COMSETTER(IoBackend)(ctlData.ioBackendType);
        if (FAILED(rc)) return rc;

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
                                aSnapshotId);
        if (FAILED(rc)) return rc;
    }

    return S_OK;
}

/**
 * @param aNode        <HardDiskAttachments> node.
 * @param fAllowStorage if false, we produce an error if the config requests media attachments
 *                      (used with importing unregistered machines which cannot have media attachments)
 * @param aSnapshotId  pointer to the snapshot ID if this is a snapshot machine
 *
 * @note Lock mParent  for reading and hard disks for writing before calling.
 */
HRESULT Machine::loadStorageDevices(StorageController *aStorageController,
                                    const settings::StorageController &data,
                                    const Guid *aSnapshotId /*= NULL*/)
{
    AssertReturn(   (getClassID() == clsidMachine && aSnapshotId == NULL)
                 || (getClassID() == clsidSnapshotMachine && aSnapshotId != NULL),
                 E_FAIL);

    HRESULT rc = S_OK;

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
                                aStorageController->getName().raw(), (*it).lPort, (*it).lDevice, mUserData->mName.raw());
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
                            /// @todo eliminate this conversion
                            ComObjPtr<Medium> med = (Medium *)drivevec[i];
                            if (    dev.strHostDriveSrc == med->getName()
                                ||  dev.strHostDriveSrc == med->getLocation())
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
                            if (    hostDriveSrc == med->getName()
                                ||  hostDriveSrc == med->getLocation())
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
                if (FAILED(rc))
                {
                    VBoxClsID clsid = getClassID();
                    if (clsid == clsidSnapshotMachine)
                    {
                        // wrap another error message around the "cannot find hard disk" set by findHardDisk
                        // so the user knows that the bad disk is in a snapshot somewhere
                        com::ErrorInfo info;
                        return setError(E_FAIL,
                                        tr("A differencing image of snapshot {%RTuuid} could not be found. %ls"),
                                        aSnapshotId->raw(),
                                        info.getText().raw());
                    }
                    else
                        return rc;
                }

                AutoWriteLock hdLock(medium COMMA_LOCKVAL_SRC_POS);

                if (medium->getType() == MediumType_Immutable)
                {
                    if (getClassID() == clsidSnapshotMachine)
                        return setError(E_FAIL,
                                        tr("Immutable hard disk '%s' with UUID {%RTuuid} cannot be directly attached to snapshot with UUID {%RTuuid} "
                                           "of the virtual machine '%ls' ('%s')"),
                                        medium->getLocationFull().raw(),
                                        dev.uuid.raw(),
                                        aSnapshotId->raw(),
                                        mUserData->mName.raw(),
                                        mData->m_strConfigFileFull.raw());

                    return setError(E_FAIL,
                                    tr("Immutable hard disk '%s' with UUID {%RTuuid} cannot be directly attached to the virtual machine '%ls' ('%s')"),
                                    medium->getLocationFull().raw(),
                                    dev.uuid.raw(),
                                    mUserData->mName.raw(),
                                    mData->m_strConfigFileFull.raw());
                }

                if (    getClassID() != clsidSnapshotMachine
                     && medium->getChildren().size() != 0
                   )
                    return setError(E_FAIL,
                                    tr("Hard disk '%s' with UUID {%RTuuid} cannot be directly attached to the virtual machine '%ls' ('%s') "
                                       "because it has %d differencing child hard disks"),
                                    medium->getLocationFull().raw(),
                                    dev.uuid.raw(),
                                    mUserData->mName.raw(),
                                    mData->m_strConfigFileFull.raw(),
                                    medium->getChildren().size());

                if (findAttachment(mMediaData->mAttachments,
                                   medium))
                    return setError(E_FAIL,
                                    tr("Hard disk '%s' with UUID {%RTuuid} is already attached to the virtual machine '%ls' ('%s')"),
                                    medium->getLocationFull().raw(),
                                    dev.uuid.raw(),
                                    mUserData->mName.raw(),
                                    mData->m_strConfigFileFull.raw());

                break;
            }

            default:
                return setError(E_FAIL,
                                tr("Device with unknown type is attached to the virtual machine '%s' ('%s')"),
                                medium->getLocationFull().raw(),
                                mUserData->mName.raw(),
                                mData->m_strConfigFileFull.raw());
        }

        if (FAILED(rc))
            break;

        const Bstr controllerName = aStorageController->getName();
        ComObjPtr<MediumAttachment> pAttachment;
        pAttachment.createObject();
        rc = pAttachment->init(this,
                               medium,
                               controllerName,
                               dev.lPort,
                               dev.lDevice,
                               dev.deviceType,
                               dev.fPassThrough);
        if (FAILED(rc)) break;

        /* associate the medium with this machine and snapshot */
        if (!medium.isNull())
        {
            if (getClassID() == clsidSnapshotMachine)
                rc = medium->attachTo(mData->mUuid, *aSnapshotId);
            else
                rc = medium->attachTo(mData->mUuid);
        }

        if (FAILED(rc))
            break;

        /* back up mMediaData to let registeredInit() properly rollback on failure
         * (= limited accessibility) */
        setModified(IsModified_Storage);
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
    AutoReadLock chlock(this COMMA_LOCKVAL_SRC_POS);

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

    AutoReadLock chlock(this COMMA_LOCKVAL_SRC_POS);

    if (!mData->mFirstSnapshot)
    {
        if (aSetError)
            return setError(VBOX_E_OBJECT_NOT_FOUND,
                            tr("This machine does not have any snapshots"));
        return VBOX_E_OBJECT_NOT_FOUND;
    }

    aSnapshot = mData->mFirstSnapshot->findChildOrSelf(aName);

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
    AssertReturn(!aName.isEmpty(), E_INVALIDARG);

    for (StorageControllerList::const_iterator it = mStorageControllers->begin();
         it != mStorageControllers->end();
         ++it)
    {
        if ((*it)->getName() == aName)
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
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    for (MediaData::AttachmentList::iterator it = mMediaData->mAttachments.begin();
         it != mMediaData->mAttachments.end();
         ++it)
    {
        if ((*it)->getControllerName() == aName)
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
 */
HRESULT Machine::prepareSaveSettings(bool *pfNeedsGlobalSaveSettings)
{
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    HRESULT rc = S_OK;

    bool fSettingsFileIsNew = !mData->pMachineConfigFile->fileExists();

    /* attempt to rename the settings file if machine name is changed */
    if (    mUserData->mNameSync
         && mUserData.isBackedUp()
         && mUserData.backedUpData()->mName != mUserData->mName
       )
    {
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
                newConfigDir = Utf8StrFmt("%s%c%s",
                    newConfigDir.raw(), RTPATH_DELIMITER, newName.raw());
                /* new dir and old dir cannot be equal here because of 'if'
                 * above and because name != newName */
                Assert(configDir != newConfigDir);
                if (!fSettingsFileIsNew)
                {
                    /* perform real rename only if the machine is not new */
                    vrc = RTPathRename(configDir.raw(), newConfigDir.raw(), 0);
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

            newConfigFile = Utf8StrFmt("%s%c%s.xml",
                newConfigDir.raw(), RTPATH_DELIMITER, newName.raw());

            /* then try to rename the settings file itself */
            if (newConfigFile != configFile)
            {
                /* get the path to old settings file in renamed directory */
                configFile = Utf8StrFmt("%s%c%s",
                                        newConfigDir.raw(),
                                        RTPATH_DELIMITER,
                                        RTPathFilename(configFile.c_str()));
                if (!fSettingsFileIsNew)
                {
                    /* perform real rename only if the machine is not new */
                    vrc = RTFileRename(configFile.raw(), newConfigFile.raw(), 0);
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
            mData->m_strConfigFileFull = newConfigFile;

            // compute the relative path too
            Utf8Str path = newConfigFile;
            mParent->calculateRelativePath(path, path);
            mData->m_strConfigFile = path;

            // store the old and new so that VirtualBox::saveSettings() can update
            // the media registry
            if (    mData->mRegistered
                 && configDir != newConfigDir)
            {
                mParent->rememberMachineNameChangeForMedia(configDir, newConfigDir);

                if (pfNeedsGlobalSaveSettings)
                    *pfNeedsGlobalSaveSettings = true;
            }

            /* update the snapshot folder */
            path = mUserData->mSnapshotFolderFull;
            if (RTPathStartsWith(path.c_str(), configDir.c_str()))
            {
                path = Utf8StrFmt("%s%s", newConfigDir.raw(),
                                  path.raw() + configDir.length());
                mUserData->mSnapshotFolderFull = path;
                calculateRelativePath(path, path);
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

        if (FAILED(rc)) return rc;
    }

    if (fSettingsFileIsNew)
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
        RTFILE f = NIL_RTFILE;
        vrc = RTFileOpen(&f, path.c_str(),
                         RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_WRITE);
        if (RT_FAILURE(vrc))
            return setError(E_FAIL,
                            tr("Could not create the settings file '%s' (%Rrc)"),
                            path.raw(),
                            vrc);
        RTFileClose(f);
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
 *    change machine data directly, not through the backup()/commit() mechanism.
 *  - SaveS_Force: settings will be saved without doing a deep compare of the
 *    settings structures. This is used when this is called because snapshots
 *    have changed to avoid the overhead of the deep compare.
 *
 * @note Must be called from under this object's write lock. Locks children for
 * writing.
 *
 * @param pfNeedsGlobalSaveSettings Optional pointer to a bool that must have been
 *          initialized to false and that will be set to true by this function if
 *          the caller must invoke VirtualBox::saveSettings() because the global
 *          settings have changed. This will happen if a machine rename has been
 *          saved and the global machine and media registries will therefore need
 *          updating.
 */
HRESULT Machine::saveSettings(bool *pfNeedsGlobalSaveSettings,
                              int aFlags /*= 0*/)
{
    LogFlowThisFuncEnter();

    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    /* make sure child objects are unable to modify the settings while we are
     * saving them */
    ensureNoStateDependencies();

    AssertReturn(    getClassID() == clsidMachine
                  || getClassID() == clsidSessionMachine,
                 E_FAIL);

    HRESULT rc = S_OK;
    bool fNeedsWrite = false;

    /* First, prepare to save settings. It will care about renaming the
     * settings directory and file if the machine name was changed and about
     * creating a new settings file if this is a new machine. */
    rc = prepareSaveSettings(pfNeedsGlobalSaveSettings);
    if (FAILED(rc)) return rc;

    // keep a pointer to the current settings structures
    settings::MachineConfigFile *pOldConfig = mData->pMachineConfigFile;
    settings::MachineConfigFile *pNewConfig = NULL;

    try
    {
        // make a fresh one to have everyone write stuff into
        pNewConfig = new settings::MachineConfigFile(NULL);
        pNewConfig->copyBaseFrom(*mData->pMachineConfigFile);

        // now go and copy all the settings data from COM to the settings structures
        // (this calles saveSettings() on all the COM objects in the machine)
        copyMachineDataToSettings(*pNewConfig);

        if (aFlags & SaveS_ResetCurStateModified)
        {
            // this gets set by takeSnapshot() (if offline snapshot) and restoreSnapshot()
            mData->mCurrentStateModified = FALSE;
            fNeedsWrite = true;     // always, no need to compare
        }
        else if (aFlags & SaveS_Force)
        {
            fNeedsWrite = true;     // always, no need to compare
        }
        else
        {
            if (!mData->mCurrentStateModified)
            {
                // do a deep compare of the settings that we just saved with the settings
                // previously stored in the config file; this invokes MachineConfigFile::operator==
                // which does a deep compare of all the settings, which is expensive but less expensive
                // than writing out XML in vain
                bool fAnySettingsChanged = (*pNewConfig == *pOldConfig);

                // could still be modified if any settings changed
                mData->mCurrentStateModified = fAnySettingsChanged;

                fNeedsWrite = fAnySettingsChanged;
            }
            else
                fNeedsWrite = true;
        }

        pNewConfig->fCurrentStateModified = !!mData->mCurrentStateModified;

        if (fNeedsWrite)
            // now spit it all out!
            pNewConfig->write(mData->m_strConfigFileFull);

        mData->pMachineConfigFile = pNewConfig;
        delete pOldConfig;
        commit();

        // after saving settings, we are no longer different from the XML on disk
        mData->flModifications = 0;
    }
    catch (HRESULT err)
    {
        // we assume that error info is set by the thrower
        rc = err;

        // restore old config
        delete pNewConfig;
        mData->pMachineConfigFile = pOldConfig;
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions(RT_SRC_POS);
    }

    if (fNeedsWrite || (aFlags & SaveS_InformCallbacksAnyway))
    {
        /* Fire the data change event, even on failure (since we've already
         * committed all data). This is done only for SessionMachines because
         * mutable Machine instances are always not registered (i.e. private
         * to the client process that creates them) and thus don't need to
         * inform callbacks. */
        if (getClassID() == clsidSessionMachine)
            mParent->onMachineDataChange(mData->mUuid);
    }

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();
    return rc;
}

/**
 * Implementation for saving the machine settings into the given
 * settings::MachineConfigFile instance. This copies machine extradata
 * from the previous machine config file in the instance data, if any.
 *
 * This gets called from two locations:
 *
 *  --  Machine::saveSettings(), during the regular XML writing;
 *
 *  --  Appliance::buildXMLForOneVirtualSystem(), when a machine gets
 *      exported to OVF and we write the VirtualBox proprietary XML
 *      into a <vbox:Machine> tag.
 *
 * This routine fills all the fields in there, including snapshots, *except*
 * for the following:
 *
 * -- fCurrentStateModified. There is some special logic associated with that.
 *
 * The caller can then call MachineConfigFile::write() or do something else
 * with it.
 *
 * Caller must hold the machine lock!
 *
 * This throws XML errors and HRESULT, so the caller must have a catch block!
 */
void Machine::copyMachineDataToSettings(settings::MachineConfigFile &config)
{
    // deep copy extradata
    config.mapExtraDataItems = mData->pMachineConfigFile->mapExtraDataItems;

    config.uuid = mData->mUuid;
    config.strName = mUserData->mName;
    config.fNameSync = !!mUserData->mNameSync;
    config.strDescription = mUserData->mDescription;
    config.strOsType = mUserData->mOSTypeId;

    if (    mData->mMachineState == MachineState_Saved
         || mData->mMachineState == MachineState_Restoring
            // when deleting a snapshot we may or may not have a saved state in the current state,
            // so let's not assert here please
         || (    (mData->mMachineState == MachineState_DeletingSnapshot)
              && (!mSSData->mStateFilePath.isEmpty())
            )
        )
    {
        Assert(!mSSData->mStateFilePath.isEmpty());
        /* try to make the file name relative to the settings file dir */
        calculateRelativePath(mSSData->mStateFilePath, config.strStateFile);
    }
    else
    {
        Assert(mSSData->mStateFilePath.isEmpty());
        config.strStateFile.setNull();
    }

    if (mData->mCurrentSnapshot)
        config.uuidCurrentSnapshot = mData->mCurrentSnapshot->getId();
    else
        config.uuidCurrentSnapshot.clear();

    config.strSnapshotFolder = mUserData->mSnapshotFolder;
    // config.fCurrentStateModified is special, see below
    config.timeLastStateChange = mData->mLastStateChange;
    config.fAborted = (mData->mMachineState == MachineState_Aborted);
    /// @todo Live Migration:        config.fTeleported = (mData->mMachineState == MachineState_Teleported);

    config.fTeleporterEnabled    = !!mUserData->mTeleporterEnabled;
    config.uTeleporterPort       = mUserData->mTeleporterPort;
    config.strTeleporterAddress  = mUserData->mTeleporterAddress;
    config.strTeleporterPassword = mUserData->mTeleporterPassword;

    config.fRTCUseUTC = !!mUserData->mRTCUseUTC;

    HRESULT rc = saveHardware(config.hardwareMachine);
    if (FAILED(rc)) throw rc;

    rc = saveStorageControllers(config.storageMachine);
    if (FAILED(rc)) throw rc;

    // save snapshots
    rc = saveAllSnapshots(config);
    if (FAILED(rc)) throw rc;
}

/**
 * Saves all snapshots of the machine into the given machine config file. Called
 * from Machine::buildMachineXML() and SessionMachine::deleteSnapshotHandler().
 * @param config
 * @return
 */
HRESULT Machine::saveAllSnapshots(settings::MachineConfigFile &config)
{
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    HRESULT rc = S_OK;

    try
    {
        config.llFirstSnapshot.clear();

        if (mData->mFirstSnapshot)
        {
            settings::Snapshot snapNew;
            config.llFirstSnapshot.push_back(snapNew);

            // get reference to the fresh copy of the snapshot on the list and
            // work on that copy directly to avoid excessive copying later
            settings::Snapshot &snap = config.llFirstSnapshot.front();

            rc = mData->mFirstSnapshot->saveSnapshot(snap, false /*aAttrsOnly*/);
            if (FAILED(rc)) throw rc;
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
        rc = VirtualBox::handleUnexpectedExceptions(RT_SRC_POS);
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
        data.fLargePages            = !!mHWData->mHWVirtExLargePagesEnabled;
        data.fVPID                  = !!mHWData->mHWVirtExVPIDEnabled;
        data.fPAE                   = !!mHWData->mPAEEnabled;
        data.fSyntheticCpu          = !!mHWData->mSyntheticCpu;

        /* Standard and Extended CPUID leafs. */
        data.llCpuIdLeafs.clear();
        for (unsigned idx = 0; idx < RT_ELEMENTS(mHWData->mCpuIdStdLeafs); idx++)
        {
            if (mHWData->mCpuIdStdLeafs[idx].ulId != UINT32_MAX)
                data.llCpuIdLeafs.push_back(mHWData->mCpuIdStdLeafs[idx]);
        }
        for (unsigned idx = 0; idx < RT_ELEMENTS(mHWData->mCpuIdExtLeafs); idx++)
        {
            if (mHWData->mCpuIdExtLeafs[idx].ulId != UINT32_MAX)
                data.llCpuIdLeafs.push_back(mHWData->mCpuIdExtLeafs[idx]);
        }

        data.cCPUs       = mHWData->mCPUCount;
        data.fCpuHotPlug = !!mHWData->mCPUHotPlugEnabled;

        data.llCpus.clear();
        if (data.fCpuHotPlug)
        {
            for (unsigned idx = 0; idx < data.cCPUs; idx++)
            {
                if (mHWData->mCPUAttached[idx])
                {
                    settings::Cpu cpu;
                    cpu.ulId = idx;
                    data.llCpus.push_back(cpu);
                }
            }
        }

        // memory
        data.ulMemorySizeMB = mHWData->mMemorySize;

        // firmware
        data.firmwareType = mHWData->mFirmwareType;

        // HID
        data.pointingHidType = mHWData->mPointingHidType;
        data.keyboardHidType = mHWData->mKeyboardHidType;

        // HPET
        data.fHpetEnabled = !!mHWData->mHpetEnabled;

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
        if (FAILED(rc)) throw rc;
#endif

        /* BIOS (required) */
        rc = mBIOSSettings->saveSettings(data.biosSettings);
        if (FAILED(rc)) throw rc;

        /* USB Controller (required) */
        rc = mUSBController->saveSettings(data.usbController);
        if (FAILED(rc)) throw rc;

        /* Network adapters (required) */
        data.llNetworkAdapters.clear();
        for (ULONG slot = 0;
             slot < RT_ELEMENTS(mNetworkAdapters);
             ++slot)
        {
            settings::NetworkAdapter nic;
            nic.ulSlot = slot;
            rc = mNetworkAdapters[slot]->saveSettings(nic);
            if (FAILED(rc)) throw rc;

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
            if (FAILED(rc)) return rc;

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
            if (FAILED(rc)) return rc;

            data.llParallelPorts.push_back(p);
        }

        /* Audio adapter */
        rc = mAudioAdapter->saveSettings(data.audioAdapter);
        if (FAILED(rc)) return rc;

        /* Shared folders */
        data.llSharedFolders.clear();
        for (HWData::SharedFolderList::const_iterator it = mHWData->mSharedFolders.begin();
            it != mHWData->mSharedFolders.end();
            ++it)
        {
            ComObjPtr<SharedFolder> pFolder = *it;
            settings::SharedFolder sf;
            sf.strName = pFolder->getName();
            sf.strHostPath = pFolder->getHostPath();
            sf.fWritable = !!pFolder->isWritable();

            data.llSharedFolders.push_back(sf);
        }

        // clipboard
        data.clipboardMode = mHWData->mClipboardMode;

        /* Guest */
        data.ulMemoryBalloonSize = mHWData->mMemoryBalloonSize;

        // IO settings
        data.ioSettings.ioMgrType = mHWData->mIoMgrType;
        data.ioSettings.ioBackendType = mHWData->mIoBackendType;
        data.ioSettings.fIoCacheEnabled = !!mHWData->mIoCacheEnabled;
        data.ioSettings.ulIoCacheSize = mHWData->mIoCacheSize;
        data.ioSettings.ulIoBandwidthMax = mHWData->mIoBandwidthMax;

        // guest properties
        data.llGuestProperties.clear();
#ifdef VBOX_WITH_GUEST_PROPS
        for (HWData::GuestPropertyList::const_iterator it = mHWData->mGuestProperties.begin();
             it != mHWData->mGuestProperties.end();
             ++it)
        {
            HWData::GuestProperty property = *it;

            /* Remove transient guest properties at shutdown unless we
             * are saving state */
            if (   (   mData->mMachineState == MachineState_PoweredOff
                    || mData->mMachineState == MachineState_Aborted
                    || mData->mMachineState == MachineState_Teleported)
                && property.mFlags & guestProp::TRANSIENT)
                continue;
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
        /* I presume this doesn't require a backup(). */
        mData->mGuestPropertiesModified = FALSE;
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
        ctl.strName = pCtl->getName();
        ctl.controllerType = pCtl->getControllerType();
        ctl.storageBus = pCtl->getStorageBus();
        ctl.ulInstance = pCtl->getInstance();

        /* Save the port count. */
        ULONG portCount;
        rc = pCtl->COMGETTER(PortCount)(&portCount);
        ComAssertComRCRet(rc, rc);
        ctl.ulPortCount = portCount;

        /* Save I/O backend */
        IoBackendType_T ioBackendType;
        rc = pCtl->COMGETTER(IoBackend)(&ioBackendType);
        ComAssertComRCRet(rc, rc);
        ctl.ioBackendType = ioBackendType;

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
    MediaData::AttachmentList atts;

    HRESULT rc = getMediumAttachmentsOfController(Bstr(aStorageController->getName()), atts);
    if (FAILED(rc)) return rc;

    data.llAttachedDevices.clear();
    for (MediaData::AttachmentList::const_iterator it = atts.begin();
         it != atts.end();
         ++it)
    {
        settings::AttachedDevice dev;

        MediumAttachment *pAttach = *it;
        Medium *pMedium = pAttach->getMedium();

        dev.deviceType = pAttach->getType();
        dev.lPort = pAttach->getPort();
        dev.lDevice = pAttach->getDevice();
        if (pMedium)
        {
            BOOL fHostDrive = FALSE;
            rc = pMedium->COMGETTER(HostDrive)(&fHostDrive);
            if (FAILED(rc))
                return rc;
            if (fHostDrive)
                dev.strHostDriveSrc = pMedium->getLocation();
            else
                dev.uuid = pMedium->getId();
            dev.fPassThrough = pAttach->getPassthrough();
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

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    /* This object's write lock is also necessary to serialize file access
     * (prevent concurrent reads and writes) */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    Assert(mData->pMachineConfigFile);

    try
    {
        if (aFlags & SaveSTS_CurStateModified)
            mData->pMachineConfigFile->fCurrentStateModified = true;

        if (aFlags & SaveSTS_StateFilePath)
        {
            if (!mSSData->mStateFilePath.isEmpty())
                /* try to make the file name relative to the settings file dir */
                calculateRelativePath(mSSData->mStateFilePath, mData->pMachineConfigFile->strStateFile);
            else
                mData->pMachineConfigFile->strStateFile.setNull();
        }

        if (aFlags & SaveSTS_StateTimeStamp)
        {
            Assert(    mData->mMachineState != MachineState_Aborted
                    || mSSData->mStateFilePath.isEmpty());

            mData->pMachineConfigFile->timeLastStateChange = mData->mLastStateChange;

            mData->pMachineConfigFile->fAborted = (mData->mMachineState == MachineState_Aborted);
//@todo live migration             mData->pMachineConfigFile->fTeleported = (mData->mMachineState == MachineState_Teleported);
        }

        mData->pMachineConfigFile->write(mData->m_strConfigFileFull);
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions(RT_SRC_POS);
    }

    return rc;
}

/**
 * Creates differencing hard disks for all normal hard disks attached to this
 * machine and a new set of attachments to refer to created disks.
 *
 * Used when taking a snapshot or when deleting the current state.
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
 * @param pfNeedsSaveSettings Optional pointer to a bool that must have been initialized to false and that will be set to true
 *                by this function if the caller should invoke VirtualBox::saveSettings() because the global settings have changed.
 *
 * @note The progress object is not marked as completed, neither on success nor
 *       on failure. This is a responsibility of the caller.
 *
 * @note Locks this object for writing.
 */
HRESULT Machine::createImplicitDiffs(const Bstr &aFolder,
                                     IProgress *aProgress,
                                     ULONG aWeight,
                                     bool aOnline,
                                     bool *pfNeedsSaveSettings)
{
    AssertReturn(!aFolder.isEmpty(), E_FAIL);

    LogFlowThisFunc(("aFolder='%ls', aOnline=%d\n", aFolder.raw(), aOnline));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* must be in a protective state because we leave the lock below */
    AssertReturn(   mData->mMachineState == MachineState_Saving
                 || mData->mMachineState == MachineState_LiveSnapshotting
                 || mData->mMachineState == MachineState_RestoringSnapshot
                 || mData->mMachineState == MachineState_DeletingSnapshot
                 , E_FAIL);

    HRESULT rc = S_OK;

    MediumLockListMap lockedMediaOffline;
    MediumLockListMap *lockedMediaMap;
    if (aOnline)
        lockedMediaMap = &mData->mSession.mLockedMedia;
    else
        lockedMediaMap = &lockedMediaOffline;

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
                if (pAtt->getType() == DeviceType_HardDisk)
                {
                    Medium* pMedium = pAtt->getMedium();
                    Assert(pMedium);

                    MediumLockList *pMediumLockList(new MediumLockList());
                    rc = pMedium->createMediumLockList(false, NULL,
                                                       *pMediumLockList);
                    if (FAILED(rc))
                    {
                        delete pMediumLockList;
                        throw rc;
                    }
                    rc = lockedMediaMap->Insert(pAtt, pMediumLockList);
                    if (FAILED(rc))
                    {
                        throw setError(rc,
                                       tr("Collecting locking information for all attached media failed"));
                    }
                }
            }

            /* Now lock all media. If this fails, nothing is locked. */
            rc = lockedMediaMap->Lock();
            if (FAILED(rc))
            {
                throw setError(rc,
                               tr("Locking of attached media failed"));
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

            DeviceType_T devType = pAtt->getType();
            Medium* pMedium = pAtt->getMedium();

            if (   devType != DeviceType_HardDisk
                || pMedium == NULL
                || pMedium->getType() != MediumType_Normal)
            {
                /* copy the attachment as is */

                /** @todo the progress object created in Console::TakeSnaphot
                 * only expects operations for hard disks. Later other
                 * device types need to show up in the progress as well. */
                if (devType == DeviceType_HardDisk)
                {
                    if (pMedium == NULL)
                        aProgress->SetNextOperation(Bstr(tr("Skipping attachment without medium")),
                                                    aWeight);        // weight
                    else
                        aProgress->SetNextOperation(BstrFmt(tr("Skipping medium '%s'"),
                                                            pMedium->getBase()->getName().raw()),
                                                    aWeight);        // weight
                }

                mMediaData->mAttachments.push_back(pAtt);
                continue;
            }

            /* need a diff */
            aProgress->SetNextOperation(BstrFmt(tr("Creating differencing hard disk for '%s'"),
                                                pMedium->getBase()->getName().raw()),
                                        aWeight);        // weight

            ComObjPtr<Medium> diff;
            diff.createObject();
            rc = diff->init(mParent,
                            pMedium->preferredDiffFormat().raw(),
                            BstrFmt("%ls"RTPATH_SLASH_STR,
                                    mUserData->mSnapshotFolderFull.raw()).raw(),
                            pfNeedsSaveSettings);
            if (FAILED(rc)) throw rc;

            /** @todo r=bird: How is the locking and diff image cleaned up if we fail before
             *        the push_back?  Looks like we're going to leave medium with the
             *        wrong kind of lock (general issue with if we fail anywhere at all)
             *        and an orphaned VDI in the snapshots folder. */

            /* update the appropriate lock list */
            MediumLockList *pMediumLockList;
            rc = lockedMediaMap->Get(pAtt, pMediumLockList);
            AssertComRCThrowRC(rc);
            if (aOnline)
            {
                rc = pMediumLockList->Update(pMedium, false);
                AssertComRCThrowRC(rc);
            }

            /* leave the lock before the potentially lengthy operation */
            alock.leave();
            rc = pMedium->createDiffStorage(diff, MediumVariant_Standard,
                                            pMediumLockList,
                                            NULL /* aProgress */,
                                            true /* aWait */,
                                            pfNeedsSaveSettings);
            alock.enter();
            if (FAILED(rc)) throw rc;

            rc = lockedMediaMap->Unlock();
            AssertComRCThrowRC(rc);
            rc = pMediumLockList->Append(diff, true);
            AssertComRCThrowRC(rc);
            rc = lockedMediaMap->Lock();
            AssertComRCThrowRC(rc);

            rc = diff->attachTo(mData->mUuid);
            AssertComRCThrowRC(rc);

            /* add a new attachment */
            ComObjPtr<MediumAttachment> attachment;
            attachment.createObject();
            rc = attachment->init(this,
                                  diff,
                                  pAtt->getControllerName(),
                                  pAtt->getPort(),
                                  pAtt->getDevice(),
                                  DeviceType_HardDisk,
                                  true /* aImplicit */);
            if (FAILED(rc)) throw rc;

            rc = lockedMediaMap->ReplaceKey(pAtt, attachment);
            AssertComRCThrowRC(rc);
            mMediaData->mAttachments.push_back(attachment);
        }
    }
    catch (HRESULT aRC) { rc = aRC; }

    /* unlock all hard disks we locked */
    if (!aOnline)
    {
        ErrorInfoKeeper eik;

        rc = lockedMediaMap->Clear();
        AssertComRC(rc);
    }

    if (FAILED(rc))
    {
        MultiResultRef mrc(rc);

        mrc = deleteImplicitDiffs(pfNeedsSaveSettings);
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
 * @param pfNeedsSaveSettings Optional pointer to a bool that must have been initialized to false and that will be set to true
 *                by this function if the caller should invoke VirtualBox::saveSettings() because the global settings have changed.
 *
 * @note Locks this object for writing.
 */
HRESULT Machine::deleteImplicitDiffs(bool *pfNeedsSaveSettings)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFuncEnter();

    AssertReturn(mMediaData.isBackedUp(), E_FAIL);

    HRESULT rc = S_OK;

    MediaData::AttachmentList implicitAtts;

    const MediaData::AttachmentList &oldAtts = mMediaData.backedUpData()->mAttachments;

    /* enumerate new attachments */
    for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
         it != mMediaData->mAttachments.end();
         ++it)
    {
        ComObjPtr<Medium> hd = (*it)->getMedium();
        if (hd.isNull())
            continue;

        if ((*it)->isImplicit())
        {
            /* deassociate and mark for deletion */
            LogFlowThisFunc(("Detaching '%s', pending deletion\n", (*it)->getLogName()));
            rc = hd->detachFrom(mData->mUuid);
            AssertComRC(rc);
            implicitAtts.push_back(*it);
            continue;
        }

        /* was this hard disk attached before? */
        if (!findAttachment(oldAtts, hd))
        {
            /* no: de-associate */
            LogFlowThisFunc(("Detaching '%s', no deletion\n", (*it)->getLogName()));
            rc = hd->detachFrom(mData->mUuid);
            AssertComRC(rc);
            continue;
        }
        LogFlowThisFunc(("Not detaching '%s'\n", (*it)->getLogName()));
    }

    /* rollback hard disk changes */
    mMediaData.rollback();

    MultiResult mrc(S_OK);

    /* delete unused implicit diffs */
    if (implicitAtts.size() != 0)
    {
        /* will leave the lock before the potentially lengthy
         * operation, so protect with the special state (unless already
         * protected) */
        MachineState_T oldState = mData->mMachineState;
        if (    oldState != MachineState_Saving
             && oldState != MachineState_LiveSnapshotting
             && oldState != MachineState_RestoringSnapshot
             && oldState != MachineState_DeletingSnapshot
           )
            setMachineState(MachineState_SettingUp);

        alock.leave();

        for (MediaData::AttachmentList::const_iterator it = implicitAtts.begin();
             it != implicitAtts.end();
             ++it)
        {
            LogFlowThisFunc(("Deleting '%s'\n", (*it)->getLogName()));
            ComObjPtr<Medium> hd = (*it)->getMedium();

            rc = hd->deleteStorage(NULL /*aProgress*/, true /*aWait*/,
                                   pfNeedsSaveSettings);
            AssertMsg(SUCCEEDED(rc), ("rc=%Rhrc it=%s hd=%s\n", rc, (*it)->getLogName(), hd->getLocationFull().c_str() ));
            mrc = rc;
        }

        alock.enter();

        if (mData->mMachineState == MachineState_SettingUp)
        {
            setMachineState(oldState);
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
        ComObjPtr<Medium> pMediumThis = pAttach->getMedium();
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
        ComObjPtr<Medium> pMediumThis = pAttach->getMedium();
        if (pMediumThis->getId() == id)
            return pAttach;
    }

    return NULL;
}

/**
 * Perform deferred hard disk detachments.
 *
 * Does nothing if the hard disk attachment data (mMediaData) is not changed (not
 * backed up).
 *
 * If @a aOnline is @c true then this method will also unlock the old hard disks
 * for which the new implicit diffs were created and will lock these new diffs for
 * writing.
 *
 * @param aOnline       Whether the VM was online prior to this operation.
 *
 * @note Locks this object for writing!
 */
void Machine::commitMedia(bool aOnline /*= false*/)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowThisFunc(("Entering, aOnline=%d\n", aOnline));

    HRESULT rc = S_OK;

    /* no attach/detach operations -- nothing to do */
    if (!mMediaData.isBackedUp())
        return;

    MediaData::AttachmentList &oldAtts = mMediaData.backedUpData()->mAttachments;
    bool fMediaNeedsLocking = false;

    /* enumerate new attachments */
    for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
            it != mMediaData->mAttachments.end();
            ++it)
    {
        MediumAttachment *pAttach = *it;

        pAttach->commit();

        Medium* pMedium = pAttach->getMedium();
        bool fImplicit = pAttach->isImplicit();

        LogFlowThisFunc(("Examining current medium '%s' (implicit: %d)\n",
                            (pMedium) ? pMedium->getName().raw() : "NULL",
                            fImplicit));

        /** @todo convert all this Machine-based voodoo to MediumAttachment
         * based commit logic. */
        if (fImplicit)
        {
            /* convert implicit attachment to normal */
            pAttach->setImplicit(false);

            if (    aOnline
                    && pMedium
                    && pAttach->getType() == DeviceType_HardDisk
                )
            {
                ComObjPtr<Medium> parent = pMedium->getParent();
                AutoWriteLock parentLock(parent COMMA_LOCKVAL_SRC_POS);

                /* update the appropriate lock list */
                MediumLockList *pMediumLockList;
                rc = mData->mSession.mLockedMedia.Get(pAttach, pMediumLockList);
                AssertComRC(rc);
                if (pMediumLockList)
                {
                    /* unlock if there's a need to change the locking */
                    if (!fMediaNeedsLocking)
                    {
                        rc = mData->mSession.mLockedMedia.Unlock();
                        AssertComRC(rc);
                        fMediaNeedsLocking = true;
                    }
                    rc = pMediumLockList->Update(parent, false);
                    AssertComRC(rc);
                    rc = pMediumLockList->Append(pMedium, true);
                    AssertComRC(rc);
                }
            }

            continue;
        }

        if (pMedium)
        {
            /* was this medium attached before? */
            for (MediaData::AttachmentList::iterator oldIt = oldAtts.begin();
                    oldIt != oldAtts.end();
                    ++oldIt)
            {
                MediumAttachment *pOldAttach = *oldIt;
                if (pOldAttach->getMedium().equalsTo(pMedium))
                {
                    LogFlowThisFunc(("--> medium '%s' was attached before, will not remove\n", pMedium->getName().raw()));

                    /* yes: remove from old to avoid de-association */
                    oldAtts.erase(oldIt);
                    break;
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
        Medium* pMedium = pAttach->getMedium();

        /* Detach only hard disks, since DVD/floppy media is detached
         * instantly in MountMedium. */
        if (pAttach->getType() == DeviceType_HardDisk && pMedium)
        {
            LogFlowThisFunc(("detaching medium '%s' from machine\n", pMedium->getName().raw()));

            /* now de-associate from the current machine state */
            rc = pMedium->detachFrom(mData->mUuid);
            AssertComRC(rc);

            if (aOnline)
            {
                /* unlock since medium is not used anymore */
                MediumLockList *pMediumLockList;
                rc = mData->mSession.mLockedMedia.Get(pAttach, pMediumLockList);
                AssertComRC(rc);
                if (pMediumLockList)
                {
                    rc = mData->mSession.mLockedMedia.Remove(pAttach);
                    AssertComRC(rc);
                }
            }
        }
    }

    /* take media locks again so that the locking state is consistent */
    if (fMediaNeedsLocking)
    {
        Assert(aOnline);
        rc = mData->mSession.mLockedMedia.Lock();
        AssertComRC(rc);
    }

    /* commit the hard disk changes */
    mMediaData.commit();

    if (getClassID() == clsidSessionMachine)
    {
        /* attach new data to the primary machine and reshare it */
        mPeer->mMediaData.attach(mMediaData);
    }

    return;
}

/**
 * Perform deferred deletion of implicitly created diffs.
 *
 * Does nothing if the hard disk attachment data (mMediaData) is not changed (not
 * backed up).
 *
 * @param pfNeedsSaveSettings Optional pointer to a bool that must have been initialized to false and that will be set to true
 *                by this function if the caller should invoke VirtualBox::saveSettings() because the global settings have changed.
 *
 * @note Locks this object for writing!
 */
void Machine::rollbackMedia()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowThisFunc(("Entering\n"));

    HRESULT rc = S_OK;

    /* no attach/detach operations -- nothing to do */
    if (!mMediaData.isBackedUp())
        return;

    /* enumerate new attachments */
    for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
            it != mMediaData->mAttachments.end();
            ++it)
    {
        MediumAttachment *pAttach = *it;
        /* Fix up the backrefs for DVD/floppy media. */
        if (pAttach->getType() != DeviceType_HardDisk)
        {
            Medium* pMedium = pAttach->getMedium();
            if (pMedium)
            {
                rc = pMedium->detachFrom(mData->mUuid);
                AssertComRC(rc);
            }
        }

        (*it)->rollback();

        pAttach = *it;
        /* Fix up the backrefs for DVD/floppy media. */
        if (pAttach->getType() != DeviceType_HardDisk)
        {
            Medium* pMedium = pAttach->getMedium();
            if (pMedium)
            {
                rc = pMedium->attachTo(mData->mUuid);
                AssertComRC(rc);
            }
        }
    }

    /** @todo convert all this Machine-based voodoo to MediumAttachment
     * based rollback logic. */
    // @todo r=dj the below totally fails if this gets called from Machine::rollback(),
    // which gets called if Machine::registeredInit() fails...
    deleteImplicitDiffs(NULL /*pfNeedsSaveSettings*/);

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
bool Machine::isInOwnDir(Utf8Str *aSettingsDir /* = NULL */) const
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

    return Bstr(dirName) == mUserData->mName;
}

/**
 * Discards all changes to machine settings.
 *
 * @param aNotify   Whether to notify the direct session about changes or not.
 *
 * @note Locks objects for writing!
 */
void Machine::rollback(bool aNotify)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), (void)0);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mStorageControllers.isNull())
    {
        if (mStorageControllers.isBackedUp())
        {
            /* unitialize all new devices (absent in the backed up list). */
            StorageControllerList::const_iterator it = mStorageControllers->begin();
            StorageControllerList *backedList = mStorageControllers.backedUpData();
            while (it != mStorageControllers->end())
            {
                if (   std::find(backedList->begin(), backedList->end(), *it)
                    == backedList->end()
                   )
                {
                    (*it)->uninit();
                }
                ++it;
            }

            /* restore the list */
            mStorageControllers.rollback();
        }

        /* rollback any changes to devices after restoring the list */
        if (mData->flModifications & IsModified_Storage)
        {
            StorageControllerList::const_iterator it = mStorageControllers->begin();
            while (it != mStorageControllers->end())
            {
                (*it)->rollback();
                ++it;
            }
        }
    }

    mUserData.rollback();

    mHWData.rollback();

    if (mData->flModifications & IsModified_Storage)
        rollbackMedia();

    if (mBIOSSettings)
        mBIOSSettings->rollback();

#ifdef VBOX_WITH_VRDP
    if (mVRDPServer && (mData->flModifications & IsModified_VRDPServer))
        mVRDPServer->rollback();
#endif

    if (mAudioAdapter)
        mAudioAdapter->rollback();

    if (mUSBController && (mData->flModifications & IsModified_USB))
        mUSBController->rollback();

    ComPtr<INetworkAdapter> networkAdapters[RT_ELEMENTS(mNetworkAdapters)];
    ComPtr<ISerialPort> serialPorts[RT_ELEMENTS(mSerialPorts)];
    ComPtr<IParallelPort> parallelPorts[RT_ELEMENTS(mParallelPorts)];

    if (mData->flModifications & IsModified_NetworkAdapters)
        for (ULONG slot = 0; slot < RT_ELEMENTS(mNetworkAdapters); slot++)
            if (    mNetworkAdapters[slot]
                 && mNetworkAdapters[slot]->isModified())
            {
                mNetworkAdapters[slot]->rollback();
                networkAdapters[slot] = mNetworkAdapters[slot];
            }

    if (mData->flModifications & IsModified_SerialPorts)
        for (ULONG slot = 0; slot < RT_ELEMENTS(mSerialPorts); slot++)
            if (    mSerialPorts[slot]
                 && mSerialPorts[slot]->isModified())
            {
                mSerialPorts[slot]->rollback();
                serialPorts[slot] = mSerialPorts[slot];
            }

    if (mData->flModifications & IsModified_ParallelPorts)
        for (ULONG slot = 0; slot < RT_ELEMENTS(mParallelPorts); slot++)
            if (    mParallelPorts[slot]
                 && mParallelPorts[slot]->isModified())
            {
                mParallelPorts[slot]->rollback();
                parallelPorts[slot] = mParallelPorts[slot];
            }

    if (aNotify)
    {
        /* inform the direct session about changes */

        ComObjPtr<Machine> that = this;
        uint32_t flModifications = mData->flModifications;
        alock.leave();

        if (flModifications & IsModified_SharedFolders)
            that->onSharedFolderChange();

        if (flModifications & IsModified_VRDPServer)
            that->onVRDPServerChange();
        if (flModifications & IsModified_USB)
            that->onUSBControllerChange();

        for (ULONG slot = 0; slot < RT_ELEMENTS(networkAdapters); slot ++)
            if (networkAdapters[slot])
                that->onNetworkAdapterChange(networkAdapters[slot], FALSE);
        for (ULONG slot = 0; slot < RT_ELEMENTS(serialPorts); slot ++)
            if (serialPorts[slot])
                that->onSerialPortChange(serialPorts[slot]);
        for (ULONG slot = 0; slot < RT_ELEMENTS(parallelPorts); slot ++)
            if (parallelPorts[slot])
                that->onParallelPortChange(parallelPorts[slot]);

        if (flModifications & IsModified_Storage)
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
    AssertComRCReturnVoid(autoCaller.rc());

    AutoCaller peerCaller(mPeer);
    AssertComRCReturnVoid(peerCaller.rc());

    AutoMultiWriteLock2 alock(mPeer, this COMMA_LOCKVAL_SRC_POS);

    /*
     *  use safe commit to ensure Snapshot machines (that share mUserData)
     *  will still refer to a valid memory location
     */
    mUserData.commitCopy();

    mHWData.commit();

    if (mMediaData.isBackedUp())
        commitMedia();

    mBIOSSettings->commit();
#ifdef VBOX_WITH_VRDP
    mVRDPServer->commit();
#endif
    mAudioAdapter->commit();
    mUSBController->commit();

    for (ULONG slot = 0; slot < RT_ELEMENTS(mNetworkAdapters); slot++)
        mNetworkAdapters[slot]->commit();
    for (ULONG slot = 0; slot < RT_ELEMENTS(mSerialPorts); slot++)
        mSerialPorts[slot]->commit();
    for (ULONG slot = 0; slot < RT_ELEMENTS(mParallelPorts); slot++)
        mParallelPorts[slot]->commit();

    bool commitStorageControllers = false;

    if (mStorageControllers.isBackedUp())
    {
        mStorageControllers.commit();

        if (mPeer)
        {
            AutoWriteLock peerlock(mPeer COMMA_LOCKVAL_SRC_POS);

            /* Commit all changes to new controllers (this will reshare data with
             * peers for thos who have peers) */
            StorageControllerList *newList = new StorageControllerList();
            StorageControllerList::const_iterator it = mStorageControllers->begin();
            while (it != mStorageControllers->end())
            {
                (*it)->commit();

                /* look if this controller has a peer device */
                ComObjPtr<StorageController> peer = (*it)->getPeer();
                if (!peer)
                {
                    /* no peer means the device is a newly created one;
                     * create a peer owning data this device share it with */
                    peer.createObject();
                    peer->init(mPeer, *it, true /* aReshare */);
                }
                else
                {
                    /* remove peer from the old list */
                    mPeer->mStorageControllers->remove(peer);
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
            mPeer->mStorageControllers.attach(newList);
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

    if (getClassID() == clsidSessionMachine)
    {
        /* attach new data to the primary machine and reshare it */
        mPeer->mUserData.attach(mUserData);
        mPeer->mHWData.attach(mHWData);
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
void Machine::copyFrom(Machine *aThat)
{
    AssertReturnVoid(getClassID() == clsidMachine || getClassID() == clsidSessionMachine);
    AssertReturnVoid(aThat->getClassID() == clsidSnapshotMachine);

    AssertReturnVoid(!Global::IsOnline(mData->mMachineState));

    mHWData.assignCopy(aThat->mHWData);

    // create copies of all shared folders (mHWData after attiching a copy
    // contains just references to original objects)
    for (HWData::SharedFolderList::iterator it = mHWData->mSharedFolders.begin();
         it != mHWData->mSharedFolders.end();
         ++it)
    {
        ComObjPtr<SharedFolder> folder;
        folder.createObject();
        HRESULT rc = folder->initCopy(getMachine(), *it);
        AssertComRC(rc);
        *it = folder;
    }

    mBIOSSettings->copyFrom(aThat->mBIOSSettings);
#ifdef VBOX_WITH_VRDP
    mVRDPServer->copyFrom(aThat->mVRDPServer);
#endif
    mAudioAdapter->copyFrom(aThat->mAudioAdapter);
    mUSBController->copyFrom(aThat->mUSBController);

    /* create private copies of all controllers */
    mStorageControllers.backup();
    mStorageControllers->clear();
    for (StorageControllerList::iterator it = aThat->mStorageControllers->begin();
         it != aThat->mStorageControllers->end();
         ++it)
    {
        ComObjPtr<StorageController> ctrl;
        ctrl.createObject();
        ctrl->initCopy(this, *it);
        mStorageControllers->push_back(ctrl);
    }

    for (ULONG slot = 0; slot < RT_ELEMENTS(mNetworkAdapters); slot++)
        mNetworkAdapters[slot]->copyFrom(aThat->mNetworkAdapters[slot]);
    for (ULONG slot = 0; slot < RT_ELEMENTS(mSerialPorts); slot++)
        mSerialPorts[slot]->copyFrom(aThat->mSerialPorts[slot]);
    for (ULONG slot = 0; slot < RT_ELEMENTS(mParallelPorts); slot++)
        mParallelPorts[slot]->copyFrom(aThat->mParallelPorts[slot]);
}

#ifdef VBOX_WITH_RESOURCE_USAGE_API
void Machine::registerMetrics(PerformanceCollector *aCollector, Machine *aMachine, RTPROCESS pid)
{
    pm::CollectorHAL *hal = aCollector->getHAL();
    /* Create sub metrics */
    pm::SubMetric *cpuLoadUser = new pm::SubMetric("CPU/Load/User",
        "Percentage of processor time spent in user mode by the VM process.");
    pm::SubMetric *cpuLoadKernel = new pm::SubMetric("CPU/Load/Kernel",
        "Percentage of processor time spent in kernel mode by the VM process.");
    pm::SubMetric *ramUsageUsed  = new pm::SubMetric("RAM/Usage/Used",
        "Size of resident portion of VM process in memory.");
    /* Create and register base metrics */
    pm::BaseMetric *cpuLoad = new pm::MachineCpuLoadRaw(hal, aMachine, pid,
                                                        cpuLoadUser, cpuLoadKernel);
    aCollector->registerBaseMetric(cpuLoad);
    pm::BaseMetric *ramUsage = new pm::MachineRamUsage(hal, aMachine, pid,
                                                       ramUsageUsed);
    aCollector->registerBaseMetric(ramUsage);

    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadUser, 0));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadUser,
                                                new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadUser,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadUser,
                                              new pm::AggregateMax()));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadKernel, 0));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadKernel,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadKernel,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(cpuLoad, cpuLoadKernel,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageUsed, 0));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageUsed,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageUsed,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(ramUsage, ramUsageUsed,
                                              new pm::AggregateMax()));


    /* Guest metrics */
    mGuestHAL = new pm::CollectorGuestHAL(this, hal);

    /* Create sub metrics */
    pm::SubMetric *guestLoadUser = new pm::SubMetric("Guest/CPU/Load/User",
        "Percentage of processor time spent in user mode as seen by the guest.");
    pm::SubMetric *guestLoadKernel = new pm::SubMetric("Guest/CPU/Load/Kernel",
        "Percentage of processor time spent in kernel mode as seen by the guest.");
    pm::SubMetric *guestLoadIdle = new pm::SubMetric("Guest/CPU/Load/Idle",
        "Percentage of processor time spent idling as seen by the guest.");

    /* The total amount of physical ram is fixed now, but we'll support dynamic guest ram configurations in the future. */
    pm::SubMetric *guestMemTotal = new pm::SubMetric("Guest/RAM/Usage/Total",      "Total amount of physical guest RAM.");
    pm::SubMetric *guestMemFree = new pm::SubMetric("Guest/RAM/Usage/Free",        "Free amount of physical guest RAM.");
    pm::SubMetric *guestMemBalloon = new pm::SubMetric("Guest/RAM/Usage/Balloon",  "Amount of ballooned physical guest RAM.");
    pm::SubMetric *guestMemCache = new pm::SubMetric("Guest/RAM/Usage/Cache",        "Total amount of guest (disk) cache memory.");

    pm::SubMetric *guestPagedTotal = new pm::SubMetric("Guest/Pagefile/Usage/Total",    "Total amount of space in the page file.");

    /* Create and register base metrics */
    pm::BaseMetric *guestCpuLoad = new pm::GuestCpuLoad(mGuestHAL, aMachine, guestLoadUser, guestLoadKernel, guestLoadIdle);
    aCollector->registerBaseMetric(guestCpuLoad);

    pm::BaseMetric *guestCpuMem = new pm::GuestRamUsage(mGuestHAL, aMachine, guestMemTotal, guestMemFree, guestMemBalloon,
                                                        guestMemCache, guestPagedTotal);
    aCollector->registerBaseMetric(guestCpuMem);

    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadUser, 0));
    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadUser, new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadUser, new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadUser, new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadKernel, 0));
    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadKernel, new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadKernel, new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadKernel, new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadIdle, 0));
    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadIdle, new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadIdle, new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(guestCpuLoad, guestLoadIdle, new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemTotal, 0));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemTotal, new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemTotal, new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemTotal, new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemFree, 0));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemFree, new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemFree, new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemFree, new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemBalloon, 0));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemBalloon, new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemBalloon, new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemBalloon, new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemCache, 0));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemCache, new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemCache, new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemCache, new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestPagedTotal, 0));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestPagedTotal, new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestPagedTotal, new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestPagedTotal, new pm::AggregateMax()));
};

void Machine::unregisterMetrics(PerformanceCollector *aCollector, Machine *aMachine)
{
    aCollector->unregisterMetricsFor(aMachine);
    aCollector->unregisterBaseMetricsFor(aMachine);

    if (mGuestHAL)
        delete mGuestHAL;
};
#endif /* VBOX_WITH_RESOURCE_USAGE_API */


////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(SessionMachine)

HRESULT SessionMachine::FinalConstruct()
{
    LogFlowThisFunc(("\n"));

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

    uninit(Uninit::Unexpected);
}

/**
 *  @note Must be called only by Machine::openSession() from its own write lock.
 */
HRESULT SessionMachine::init(Machine *aMachine)
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
    mIPCSem = ::CreateMutex(NULL, FALSE, mIPCSemName);
    ComAssertMsgRet(mIPCSem,
                    ("Cannot create IPC mutex '%ls', err=%d",
                     mIPCSemName.raw(), ::GetLastError()),
                    E_FAIL);
#elif defined(RT_OS_OS2)
    Utf8Str ipcSem = Utf8StrFmt("\\SEM32\\VBOX\\VM\\{%RTuuid}",
                                aMachine->mData->mUuid.raw());
    mIPCSemName = ipcSem;
    APIRET arc = ::DosCreateMutexSem((PSZ)ipcSem.raw(), &mIPCSem, 0, FALSE);
    ComAssertMsgRet(arc == NO_ERROR,
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
        int sem = ::semget(key, 1, S_IRUSR | S_IWUSR | IPC_CREAT | IPC_EXCL);
        if (sem >= 0 || (errno != EEXIST && errno != EACCES))
        {
            mIPCSem = sem;
            if (sem >= 0)
                mIPCKey = BstrFmt("%u", key);
            break;
        }
    }
# else /* !VBOX_WITH_NEW_SYS_V_KEYGEN */
    Utf8Str semName = aMachine->mData->m_strConfigFileFull;
    char *pszSemName = NULL;
    RTStrUtf8ToCurrentCP(&pszSemName, semName);
    key_t key = ::ftok(pszSemName, 'V');
    RTStrFree(pszSemName);

    mIPCSem = ::semget(key, 1, S_IRWXU | S_IRWXG | S_IRWXO | IPC_CREAT);
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
    ComAssertMsgRet(mIPCSem >= 0, ("Cannot create IPC semaphore, errno=%d", errnoSave),
                    E_FAIL);
    /* set the initial value to 1 */
    int rv = ::semctl(mIPCSem, 0, SETVAL, 1);
    ComAssertMsgRet(rv == 0, ("Cannot init IPC semaphore, errno=%d", errno),
                    E_FAIL);
#else
# error "Port me!"
#endif

    /* memorize the peer Machine */
    unconst(mPeer) = aMachine;
    /* share the parent pointer */
    unconst(mParent) = aMachine->mParent;

    /* take the pointers to data to share */
    mData.share(aMachine->mData);
    mSSData.share(aMachine->mSSData);

    mUserData.share(aMachine->mUserData);
    mHWData.share(aMachine->mHWData);
    mMediaData.share(aMachine->mMediaData);

    mStorageControllers.allocate();
    for (StorageControllerList::const_iterator it = aMachine->mStorageControllers->begin();
         it != aMachine->mStorageControllers->end();
         ++it)
    {
        ComObjPtr<StorageController> ctl;
        ctl.createObject();
        ctl->init(this, *it);
        mStorageControllers->push_back(ctl);
    }

    unconst(mBIOSSettings).createObject();
    mBIOSSettings->init(this, aMachine->mBIOSSettings);
#ifdef VBOX_WITH_VRDP
    /* create another VRDPServer object that will be mutable */
    unconst(mVRDPServer).createObject();
    mVRDPServer->init(this, aMachine->mVRDPServer);
#endif
    /* create another audio adapter object that will be mutable */
    unconst(mAudioAdapter).createObject();
    mAudioAdapter->init(this, aMachine->mAudioAdapter);
    /* create a list of serial ports that will be mutable */
    for (ULONG slot = 0; slot < RT_ELEMENTS(mSerialPorts); slot++)
    {
        unconst(mSerialPorts[slot]).createObject();
        mSerialPorts[slot]->init(this, aMachine->mSerialPorts[slot]);
    }
    /* create a list of parallel ports that will be mutable */
    for (ULONG slot = 0; slot < RT_ELEMENTS(mParallelPorts); slot++)
    {
        unconst(mParallelPorts[slot]).createObject();
        mParallelPorts[slot]->init(this, aMachine->mParallelPorts[slot]);
    }
    /* create another USB controller object that will be mutable */
    unconst(mUSBController).createObject();
    mUSBController->init(this, aMachine->mUSBController);

    /* create a list of network adapters that will be mutable */
    for (ULONG slot = 0; slot < RT_ELEMENTS(mNetworkAdapters); slot++)
    {
        unconst(mNetworkAdapters[slot]).createObject();
        mNetworkAdapters[slot]->init(this, aMachine->mNetworkAdapters[slot]);
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
void SessionMachine::uninit(Uninit::Reason aReason)
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
            ::CloseHandle(mIPCSem);
        mIPCSem = NULL;
#elif defined(RT_OS_OS2)
        if (mIPCSem != NULLHANDLE)
            ::DosCloseMutexSem(mIPCSem);
        mIPCSem = NULLHANDLE;
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
        if (mIPCSem >= 0)
            ::semctl(mIPCSem, 0, IPC_RMID);
        mIPCSem = -1;
# ifdef VBOX_WITH_NEW_SYS_V_KEYGEN
        mIPCKey = "0";
# endif /* VBOX_WITH_NEW_SYS_V_KEYGEN */
#else
# error "Port me!"
#endif
        uninitDataAndChildObjects();
        mData.free();
        unconst(mParent) = NULL;
        unconst(mPeer) = NULL;
        LogFlowThisFuncLeave();
        return;
    }

    MachineState_T lastState;
    {
        AutoReadLock tempLock(this COMMA_LOCKVAL_SRC_POS);
        lastState = mData->mMachineState;
    }
    NOREF(lastState);

#ifdef VBOX_WITH_USB
    // release all captured USB devices, but do this before requesting the locks below
    if (aReason == Uninit::Abnormal && Global::IsOnline(lastState))
    {
        /* Console::captureUSBDevices() is called in the VM process only after
         * setting the machine state to Starting or Restoring.
         * Console::detachAllUSBDevices() will be called upon successful
         * termination. So, we need to release USB devices only if there was
         * an abnormal termination of a running VM.
         *
         * This is identical to SessionMachine::DetachAllUSBDevices except
         * for the aAbnormal argument. */
        HRESULT rc = mUSBController->notifyProxy(false /* aInsertFilters */);
        AssertComRC(rc);
        NOREF(rc);

        USBProxyService *service = mParent->host()->usbProxyService();
        if (service)
            service->detachAllDevicesFromVM(this, true /* aDone */, true /* aAbnormal */);
    }
#endif /* VBOX_WITH_USB */

    // we need to lock this object in uninit() because the lock is shared
    // with mPeer (as well as data we modify below). mParent->addProcessToReap()
    // and others need mParent lock, and USB needs host lock.
    AutoMultiWriteLock3 multilock(mParent, mParent->host(), this COMMA_LOCKVAL_SRC_POS);

#ifdef VBOX_WITH_RESOURCE_USAGE_API
    unregisterMetrics(mParent->performanceCollector(), mPeer);
#endif /* VBOX_WITH_RESOURCE_USAGE_API */

    if (aReason == Uninit::Abnormal)
    {
        LogWarningThisFunc(("ABNORMAL client termination! (wasBusy=%d)\n",
                             Global::IsOnlineOrTransient(lastState)));

        /* reset the state to Aborted */
        if (mData->mMachineState != MachineState_Aborted)
            setMachineState(MachineState_Aborted);
    }

    // any machine settings modified?
    if (mData->flModifications)
    {
        LogWarningThisFunc(("Discarding unsaved settings changes!\n"));
        rollback(false /* aNotify */);
    }

    Assert(mSnapshotData.mStateFilePath.isEmpty() || !mSnapshotData.mSnapshot);
    if (!mSnapshotData.mStateFilePath.isEmpty())
    {
        LogWarningThisFunc(("canceling failed save state request!\n"));
        endSavingState(FALSE /* aSuccess  */);
    }
    else if (!mSnapshotData.mSnapshot.isNull())
    {
        LogWarningThisFunc(("canceling untaken snapshot!\n"));

        /* delete all differencing hard disks created (this will also attach
         * their parents back by rolling back mMediaData) */
        rollbackMedia();
        /* delete the saved state file (it might have been already created) */
        if (mSnapshotData.mSnapshot->stateFilePath().length())
            RTFileDelete(mSnapshotData.mSnapshot->stateFilePath().c_str());

        mSnapshotData.mSnapshot->uninit();
    }

    if (!mData->mSession.mType.isEmpty())
    {
        /* mType is not null when this machine's process has been started by
         * VirtualBox::OpenRemoteSession(), therefore it is our child.  We
         * need to queue the PID to reap the process (and avoid zombies on
         * Linux). */
        Assert(mData->mSession.mPid != NIL_RTPROCESS);
        mParent->addProcessToReap(mData->mSession.mPid);
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
            if (FAILED(rc))
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
        Assert(mData->mSession.mDirectControl.isNull());
        Assert(mData->mSession.mState == SessionState_Closing);
        Assert(!mData->mSession.mProgress.isNull());
    }
    if (mData->mSession.mProgress)
    {
        if (aReason == Uninit::Normal)
            mData->mSession.mProgress->notifyComplete(S_OK);
        else
            mData->mSession.mProgress->notifyComplete(E_FAIL,
                                                      COM_IIDOF(ISession),
                                                      getComponentName(),
                                                      tr("The VM session was aborted"));
        mData->mSession.mProgress.setNull();
    }

    /* remove the association between the peer machine and this session machine */
    Assert(mData->mSession.mMachine == this ||
           aReason == Uninit::Unexpected);

    /* reset the rest of session data */
    mData->mSession.mMachine.setNull();
    mData->mSession.mState = SessionState_Closed;
    mData->mSession.mType.setNull();

    /* close the interprocess semaphore before leaving the exclusive lock */
#if defined(RT_OS_WINDOWS)
    if (mIPCSem)
        ::CloseHandle(mIPCSem);
    mIPCSem = NULL;
#elif defined(RT_OS_OS2)
    if (mIPCSem != NULLHANDLE)
        ::DosCloseMutexSem(mIPCSem);
    mIPCSem = NULLHANDLE;
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    if (mIPCSem >= 0)
        ::semctl(mIPCSem, 0, IPC_RMID);
    mIPCSem = -1;
# ifdef VBOX_WITH_NEW_SYS_V_KEYGEN
    mIPCKey = "0";
# endif /* VBOX_WITH_NEW_SYS_V_KEYGEN */
#else
# error "Port me!"
#endif

    /* fire an event */
    mParent->onSessionStateChange(mData->mUuid, SessionState_Closed);

    uninitDataAndChildObjects();

    /* free the essential data structure last */
    mData.free();

    /* leave the exclusive lock before setting the below two to NULL */
    multilock.leave();

    unconst(mParent) = NULL;
    unconst(mPeer) = NULL;

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
    AssertReturn(mPeer != NULL, NULL);
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
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mRemoveSavedState = aRemove;

    return S_OK;
}

/**
 *  @note Locks the same as #setMachineState() does.
 */
STDMETHODIMP SessionMachine::UpdateState(MachineState_T aMachineState)
{
    return setMachineState(aMachineState);
}

/**
 *  @note Locks this object for reading.
 */
STDMETHODIMP SessionMachine::GetIPCId(BSTR *aId)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

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
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::SetPowerUpInfo(IVirtualBoxErrorInfo *aError)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (   mData->mSession.mState == SessionState_Open
        && mData->mSession.mProgress)
    {
        /* Finalize the progress, since the remote session has completed
         * power on (successful or not). */
        if (aError)
        {
            /* Transfer error information immediately, as the
             * IVirtualBoxErrorInfo object is most likely transient. */
            HRESULT rc;
            LONG rRc = S_OK;
            rc = aError->COMGETTER(ResultCode)(&rRc);
            AssertComRCReturnRC(rc);
            Bstr rIID;
            rc = aError->COMGETTER(InterfaceID)(rIID.asOutParam());
            AssertComRCReturnRC(rc);
            Bstr rComponent;
            rc = aError->COMGETTER(Component)(rComponent.asOutParam());
            AssertComRCReturnRC(rc);
            Bstr rText;
            rc = aError->COMGETTER(Text)(rText.asOutParam());
            AssertComRCReturnRC(rc);
            mData->mSession.mProgress->notifyComplete(rRc, Guid(rIID), rComponent, Utf8Str(rText).raw());
        }
        else
            mData->mSession.mProgress->notifyComplete(S_OK);
        mData->mSession.mProgress.setNull();

        return S_OK;
    }
    else
        return VBOX_E_INVALID_OBJECT_STATE;
}

/**
 *  Goes through the USB filters of the given machine to see if the given
 *  device matches any filter or not.
 *
 *  @note Locks the same as USBController::hasMatchingFilter() does.
 */
STDMETHODIMP SessionMachine::RunUSBDeviceFilters(IUSBDevice *aUSBDevice,
                                                 BOOL *aMatched,
                                                 ULONG *aMaskedIfs)
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(aUSBDevice);
    CheckComArgOutPointerValid(aMatched);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    *aMatched = mUSBController->hasMatchingFilter(aUSBDevice, aMaskedIfs);
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
STDMETHODIMP SessionMachine::CaptureUSBDevice(IN_BSTR aId)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

#ifdef VBOX_WITH_USB
    /* if captureDeviceForVM() fails, it must have set extended error info */
    MultiResult rc = mParent->host()->checkUSBProxyService();
    if (FAILED(rc)) return rc;

    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn(service, E_FAIL);
    return service->captureDeviceForVM(this, Guid(aId));
#else
    NOREF(aId);
    return E_NOTIMPL;
#endif
}

/**
 *  @note Locks the same as Host::detachUSBDevice() does.
 */
STDMETHODIMP SessionMachine::DetachUSBDevice(IN_BSTR aId, BOOL aDone)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn(service, E_FAIL);
    return service->detachDeviceFromVM(this, Guid(aId), !!aDone);
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
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    HRESULT rc = mUSBController->notifyProxy(true /* aInsertFilters */);
    AssertComRC(rc);
    NOREF(rc);

    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn(service, E_FAIL);
    return service->autoCaptureDevicesForVM(this);
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
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

#ifdef VBOX_WITH_USB
    HRESULT rc = mUSBController->notifyProxy(false /* aInsertFilters */);
    AssertComRC(rc);
    NOREF(rc);

    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn(service, E_FAIL);
    return service->detachAllDevicesFromVM(this, !!aDone, false /* aAbnormal */);
#else
    NOREF(aDone);
    return S_OK;
#endif
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::OnSessionEnd(ISession *aSession,
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
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* get IInternalSessionControl interface */
    ComPtr<IInternalSessionControl> control(aSession);

    ComAssertRet(!control.isNull(), E_INVALIDARG);

    /* Creating a Progress object requires the VirtualBox lock, and
     * thus locking it here is required by the lock order rules. */
    AutoMultiWriteLock2 alock(mParent->lockHandle(), this->lockHandle() COMMA_LOCKVAL_SRC_POS);

    if (control.equalsTo(mData->mSession.mDirectControl))
    {
        ComAssertRet(aProgress, E_POINTER);

        /* The direct session is being normally closed by the client process
         * ----------------------------------------------------------------- */

        /* go to the closing state (essential for all open*Session() calls and
         * for #checkForDeath()) */
        Assert(mData->mSession.mState == SessionState_Open);
        mData->mSession.mState = SessionState_Closing;

        /* set direct control to NULL to release the remote instance */
        mData->mSession.mDirectControl.setNull();
        LogFlowThisFunc(("Direct control is set to NULL\n"));

        if (mData->mSession.mProgress)
        {
            /* finalize the progress, someone might wait if a frontend
             * closes the session before powering on the VM. */
            mData->mSession.mProgress->notifyComplete(E_FAIL,
                                                      COM_IIDOF(ISession),
                                                      getComponentName(),
                                                      tr("The VM session was closed before any attempt to power it on"));
            mData->mSession.mProgress.setNull();
        }

        /*  Create the progress object the client will use to wait until
         * #checkForDeath() is called to uninitialize this session object after
         * it releases the IPC semaphore. */
        Assert(mData->mSession.mProgress.isNull());
        ComObjPtr<Progress> progress;
        progress.createObject();
        ComPtr<IUnknown> pPeer(mPeer);
        progress->init(mParent, pPeer,
                       Bstr(tr("Closing session")), FALSE /* aCancelable */);
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
            if (control.equalsTo(*it))
                break;
            ++it;
        }
        BOOL found = it != mData->mSession.mRemoteControls.end();
        ComAssertMsgRet(found, ("The session is not found in the session list!"),
                         E_INVALIDARG);
        mData->mSession.mRemoteControls.remove(*it);
    }

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::BeginSavingState(IProgress *aProgress, BSTR *aStateFilePath)
{
    LogFlowThisFuncEnter();

    AssertReturn(aProgress, E_INVALIDARG);
    AssertReturn(aStateFilePath, E_POINTER);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(    mData->mMachineState == MachineState_Paused
                  && mSnapshotData.mLastState == MachineState_Null
                  && mSnapshotData.mProgressId.isEmpty()
                  && mSnapshotData.mStateFilePath.isEmpty(),
                 E_FAIL);

    /* memorize the progress ID and add it to the global collection */
    Bstr progressId;
    HRESULT rc = aProgress->COMGETTER(Id)(progressId.asOutParam());
    AssertComRCReturn(rc, rc);
    rc = mParent->addProgress(aProgress);
    AssertComRCReturn(rc, rc);

    Bstr stateFilePath;
    /* stateFilePath is null when the machine is not running */
    if (mData->mMachineState == MachineState_Paused)
    {
        stateFilePath = Utf8StrFmt("%ls%c{%RTuuid}.sav",
                                   mUserData->mSnapshotFolderFull.raw(),
                                   RTPATH_DELIMITER, mData->mUuid.raw());
    }

    /* fill in the snapshot data */
    mSnapshotData.mLastState = mData->mMachineState;
    mSnapshotData.mProgressId = Guid(progressId);
    mSnapshotData.mStateFilePath = stateFilePath;

    /* set the state to Saving (this is expected by Console::SaveState()) */
    setMachineState(MachineState_Saving);

    stateFilePath.cloneTo(aStateFilePath);

    return S_OK;
}

/**
 *  @note Locks mParent + this object for writing.
 */
STDMETHODIMP SessionMachine::EndSavingState(BOOL aSuccess)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    /* endSavingState() need mParent lock */
    AutoMultiWriteLock2 alock(mParent, this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(    mData->mMachineState == MachineState_Saving
                  && mSnapshotData.mLastState != MachineState_Null
                  && !mSnapshotData.mProgressId.isEmpty()
                  && !mSnapshotData.mStateFilePath.isEmpty(),
                 E_FAIL);

    /*
     *  on success, set the state to Saved;
     *  on failure, set the state to the state we had when BeginSavingState() was
     *  called (this is expected by Console::SaveState() and
     *  Console::saveStateThread())
     */
    if (aSuccess)
        setMachineState(MachineState_Saved);
    else
        setMachineState(mSnapshotData.mLastState);

    return endSavingState(aSuccess);
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::AdoptSavedState(IN_BSTR aSavedStateFile)
{
    LogFlowThisFunc(("\n"));

    CheckComArgStrNotEmptyOrNull(aSavedStateFile);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(   mData->mMachineState == MachineState_PoweredOff
                 || mData->mMachineState == MachineState_Teleported
                 || mData->mMachineState == MachineState_Aborted
                 , E_FAIL); /** @todo setError. */

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

    return setMachineState(MachineState_Saved);
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
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(!ComSafeArrayOutIsNull(aNames), E_POINTER);
    AssertReturn(!ComSafeArrayOutIsNull(aValues), E_POINTER);
    AssertReturn(!ComSafeArrayOutIsNull(aTimestamps), E_POINTER);
    AssertReturn(!ComSafeArrayOutIsNull(aFlags), E_POINTER);

    size_t cEntries = mHWData->mGuestProperties.size();
    com::SafeArray<BSTR> names(cEntries);
    com::SafeArray<BSTR> values(cEntries);
    com::SafeArray<ULONG64> timestamps(cEntries);
    com::SafeArray<BSTR> flags(cEntries);
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

    CheckComArgStrNotEmptyOrNull(aName);
    if (aValue != NULL && (!VALID_PTR(aValue) || !VALID_PTR(aFlags)))
        return E_POINTER;  /* aValue can be NULL to indicate deletion */

    try
    {
        /*
         * Convert input up front.
         */
        Utf8Str     utf8Name(aName);
        uint32_t    fFlags = NILFLAG;
        if (aFlags)
        {
            Utf8Str utf8Flags(aFlags);
            int vrc = validateFlags(utf8Flags.raw(), &fFlags);
            AssertRCReturn(vrc, E_INVALIDARG);
        }

        /*
         * Now grab the object lock, validate the state and do the update.
         */
        AutoCaller autoCaller(this);
        if (FAILED(autoCaller.rc())) return autoCaller.rc();

        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        switch (mData->mMachineState)
        {
            case MachineState_Paused:
            case MachineState_Running:
            case MachineState_Teleporting:
            case MachineState_TeleportingPausedVM:
            case MachineState_LiveSnapshotting:
            case MachineState_Saving:
                break;

            default:
                AssertMsgFailedReturn(("%s\n", Global::stringifyMachineState(mData->mMachineState)),
                                      VBOX_E_INVALID_VM_STATE);
        }

        setModified(IsModified_MachineData);
        mHWData.backup();

        /** @todo r=bird: The careful memory handling doesn't work out here because
         *  the catch block won't undo any damange we've done.  So, if push_back throws
         *  bad_alloc then you've lost the value.
         *
         *  Another thing. Doing a linear search here isn't extremely efficient, esp.
         *  since values that changes actually bubbles to the end of the list.  Using
         *  something that has an efficient lookup and can tollerate a bit of updates
         *  would be nice.  RTStrSpace is one suggestion (it's not perfect).  Some
         *  combination of RTStrCache (for sharing names and getting uniqueness into
         *  the bargain) and hash/tree is another. */
        for (HWData::GuestPropertyList::iterator iter = mHWData->mGuestProperties.begin();
             iter != mHWData->mGuestProperties.end();
             ++iter)
            if (utf8Name == iter->strName)
            {
                mHWData->mGuestProperties.erase(iter);
                mData->mGuestPropertiesModified = TRUE;
                break;
            }
        if (aValue != NULL)
        {
            HWData::GuestProperty property = { aName, aValue, aTimestamp, fFlags };
            mHWData->mGuestProperties.push_back(property);
            mData->mGuestPropertiesModified = TRUE;
        }

        /*
         * Send a callback notification if appropriate
         */
        if (    mHWData->mGuestPropertyNotificationPatterns.isEmpty()
             || RTStrSimplePatternMultiMatch(mHWData->mGuestPropertyNotificationPatterns.raw(),
                                             RTSTR_MAX,
                                             utf8Name.raw(),
                                             RTSTR_MAX, NULL)
           )
        {
            alock.leave();

            mParent->onGuestPropertyChange(mData->mUuid,
                                           aName,
                                           aValue,
                                           aFlags);
        }
    }
    catch (...)
    {
        return VirtualBox::handleUnexpectedExceptions(RT_SRC_POS);
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

        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

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

        AssertMsg(mIPCSem, ("semaphore must be created"));

        /* release the IPC mutex */
        ::ReleaseMutex(mIPCSem);

        terminated = true;

#elif defined(RT_OS_OS2)

        AssertMsg(mIPCSem, ("semaphore must be created"));

        /* release the IPC mutex */
        ::DosReleaseMutexSem(mIPCSem);

        terminated = true;

#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)

        AssertMsg(mIPCSem >= 0, ("semaphore must be created"));

        int val = ::semctl(mIPCSem, 0, GETVAL);
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
        uninit(reason);

    return terminated;
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onNetworkAdapterChange(INetworkAdapter *networkAdapter, BOOL changeAdapter)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnNetworkAdapterChange(networkAdapter, changeAdapter);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onSerialPortChange(ISerialPort *serialPort)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
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
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
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
HRESULT SessionMachine::onStorageControllerChange()
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnStorageControllerChange();
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onMediumChange(IMediumAttachment *aAttachment, BOOL aForce)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnMediumChange(aAttachment, aForce);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onCPUChange(ULONG aCPU, BOOL aRemove)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnCPUChange(aCPU, aRemove);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onVRDPServerChange()
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
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
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
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
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        directControl = mData->mSession.mDirectControl;
    }

    /* ignore notifications sent after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    return directControl->OnSharedFolderChange(FALSE /* aGlobal */);
}

/**
 *  Returns @c true if this machine's USB controller reports it has a matching
 *  filter for the given USB device and @c false otherwise.
 *
 *  @note Caller must have requested machine read lock.
 */
bool SessionMachine::hasMatchingUSBFilter(const ComObjPtr<HostUSBDevice> &aDevice, ULONG *aMaskedIfs)
{
    AutoCaller autoCaller(this);
    /* silently return if not ready -- this method may be called after the
     * direct machine session has been called */
    if (!autoCaller.isOk())
        return false;


#ifdef VBOX_WITH_USB
    switch (mData->mMachineState)
    {
        case MachineState_Starting:
        case MachineState_Restoring:
        case MachineState_TeleportingIn:
        case MachineState_Paused:
        case MachineState_Running:
        /** @todo Live Migration: snapshoting & teleporting. Need to fend things of
         *        elsewhere... */
            return mUSBController->hasMatchingFilter(aDevice, aMaskedIfs);
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
HRESULT SessionMachine::onUSBDeviceAttach(IUSBDevice *aDevice,
                                          IVirtualBoxErrorInfo *aError,
                                          ULONG aMaskedIfs)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);

    /* This notification may happen after the machine object has been
     * uninitialized (the session was closed), so don't assert. */
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        directControl = mData->mSession.mDirectControl;
    }

    /* fail on notifications sent after #OnSessionEnd() is called, it is
     * expected by the caller */
    if (!directControl)
        return E_FAIL;

    /* No locks should be held at this point. */
    AssertMsg(RTLockValidatorWriteLockGetCount(RTThreadSelf()) == 0, ("%d\n", RTLockValidatorWriteLockGetCount(RTThreadSelf())));
    AssertMsg(RTLockValidatorReadLockGetCount(RTThreadSelf()) == 0, ("%d\n", RTLockValidatorReadLockGetCount(RTThreadSelf())));

    return directControl->OnUSBDeviceAttach(aDevice, aError, aMaskedIfs);
}

/**
 *  @note The calls shall hold no locks. Will temporarily lock this object for reading.
 */
HRESULT SessionMachine::onUSBDeviceDetach(IN_BSTR aId,
                                          IVirtualBoxErrorInfo *aError)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);

    /* This notification may happen after the machine object has been
     * uninitialized (the session was closed), so don't assert. */
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        directControl = mData->mSession.mDirectControl;
    }

    /* fail on notifications sent after #OnSessionEnd() is called, it is
     * expected by the caller */
    if (!directControl)
        return E_FAIL;

    /* No locks should be held at this point. */
    AssertMsg(RTLockValidatorWriteLockGetCount(RTThreadSelf()) == 0, ("%d\n", RTLockValidatorWriteLockGetCount(RTThreadSelf())));
    AssertMsg(RTLockValidatorReadLockGetCount(RTThreadSelf()) == 0, ("%d\n", RTLockValidatorReadLockGetCount(RTThreadSelf())));

    return directControl->OnUSBDeviceDetach(aId, aError);
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
HRESULT SessionMachine::endSavingState(BOOL aSuccess)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    /* saveSettings() needs mParent lock */
    AutoMultiWriteLock2 alock(mParent, this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (aSuccess)
    {
        mSSData->mStateFilePath = mSnapshotData.mStateFilePath;

        /* save all VM settings */
        rc = saveSettings(NULL);
                // no need to check whether VirtualBox.xml needs saving also since
                // we can't have a name change pending at this point
    }
    else
    {
        /* delete the saved state file (it might have been already created) */
        RTFileDelete(mSnapshotData.mStateFilePath.c_str());
    }

    /* remove the completed progress object */
    mParent->removeProgress(mSnapshotData.mProgressId);

    /* clear out the temporary saved state data */
    mSnapshotData.mLastState = MachineState_Null;
    mSnapshotData.mProgressId.clear();
    mSnapshotData.mStateFilePath.setNull();

    LogFlowThisFuncLeave();
    return rc;
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
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(   mData->mMachineState == MachineState_Starting
                 || mData->mMachineState == MachineState_Restoring
                 || mData->mMachineState == MachineState_TeleportingIn, E_FAIL);
    /* bail out if trying to lock things with already set up locking */
    AssertReturn(mData->mSession.mLockedMedia.IsEmpty(), E_FAIL);

    MultiResult mrc(S_OK);

    /* Collect locking information for all medium objects attached to the VM. */
    for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
         it != mMediaData->mAttachments.end();
         ++it)
    {
        MediumAttachment* pAtt = *it;
        DeviceType_T devType = pAtt->getType();
        Medium *pMedium = pAtt->getMedium();

        MediumLockList *pMediumLockList(new MediumLockList());
        // There can be attachments without a medium (floppy/dvd), and thus
        // it's impossible to create a medium lock list. It still makes sense
        // to have the empty medium lock list in the map in case a medium is
        // attached later.
        if (pMedium != NULL)
        {
            mrc = pMedium->createMediumLockList(devType != DeviceType_DVD,
                                                NULL, *pMediumLockList);
            if (FAILED(mrc))
            {
                delete pMediumLockList;
                mData->mSession.mLockedMedia.Clear();
                break;
            }
        }

        HRESULT rc = mData->mSession.mLockedMedia.Insert(pAtt, pMediumLockList);
        if (FAILED(rc))
        {
            mData->mSession.mLockedMedia.Clear();
            mrc = setError(rc,
                           tr("Collecting locking information for all attached media failed"));
            break;
        }
    }

    if (SUCCEEDED(mrc))
    {
        /* Now lock all media. If this fails, nothing is locked. */
        HRESULT rc = mData->mSession.mLockedMedia.Lock();
        if (FAILED(rc))
        {
            mrc = setError(rc,
                           tr("Locking of attached media failed"));
        }
    }

    return mrc;
}

/**
 * Undoes the locks made by by #lockMedia().
 */
void SessionMachine::unlockMedia()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* we may be holding important error info on the current thread;
     * preserve it */
    ErrorInfoKeeper eik;

    HRESULT rc = mData->mSession.mLockedMedia.Clear();
    AssertComRC(rc);
}

/**
 * Helper to change the machine state (reimplementation).
 *
 * @note Locks this object for writing.
 */
HRESULT SessionMachine::setMachineState(MachineState_T aMachineState)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aMachineState=%s\n", Global::stringifyMachineState(aMachineState) ));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    MachineState_T oldMachineState = mData->mMachineState;

    AssertMsgReturn(oldMachineState != aMachineState,
                    ("oldMachineState=%s, aMachineState=%s\n",
                     Global::stringifyMachineState(oldMachineState), Global::stringifyMachineState(aMachineState)),
                    E_FAIL);

    HRESULT rc = S_OK;

    int stsFlags = 0;
    bool deleteSavedState = false;

    /* detect some state transitions */

    if (   (   oldMachineState == MachineState_Saved
            && aMachineState   == MachineState_Restoring)
        || (   (   oldMachineState == MachineState_PoweredOff
                || oldMachineState == MachineState_Teleported
                || oldMachineState == MachineState_Aborted
               )
            && (   aMachineState   == MachineState_TeleportingIn
                || aMachineState   == MachineState_Starting
               )
           )
       )
    {
        /* The EMT thread is about to start */

        /* Nothing to do here for now... */

        /// @todo NEWMEDIA don't let mDVDDrive and other children
        /// change anything when in the Starting/Restoring state
    }
    else if (   (   oldMachineState == MachineState_Running
                 || oldMachineState == MachineState_Paused
                 || oldMachineState == MachineState_Teleporting
                 || oldMachineState == MachineState_LiveSnapshotting
                 || oldMachineState == MachineState_Stuck
                 || oldMachineState == MachineState_Starting
                 || oldMachineState == MachineState_Stopping
                 || oldMachineState == MachineState_Saving
                 || oldMachineState == MachineState_Restoring
                 || oldMachineState == MachineState_TeleportingPausedVM
                 || oldMachineState == MachineState_TeleportingIn
                 )
             && (   aMachineState == MachineState_PoweredOff
                 || aMachineState == MachineState_Saved
                 || aMachineState == MachineState_Teleported
                 || aMachineState == MachineState_Aborted
                )
             /* ignore PoweredOff->Saving->PoweredOff transition when taking a
              * snapshot */
             && (   mSnapshotData.mSnapshot.isNull()
                 || mSnapshotData.mLastState >= MachineState_Running /** @todo Live Migration: clean up (lazy bird) */
                )
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
                 || aMachineState == MachineState_Aborted
                 || aMachineState == MachineState_Teleported
                )
            )
    {
        /*
         *  delete the saved state after Console::ForgetSavedState() is called
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
        || aMachineState == MachineState_TeleportingIn
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
            Assert(!mSSData->mStateFilePath.isEmpty());
            RTFileDelete(mSSData->mStateFilePath.c_str());
        }
        mSSData->mStateFilePath.setNull();
        stsFlags |= SaveSTS_StateFilePath;
    }

    /* redirect to the underlying peer machine */
    mPeer->setMachineState(aMachineState);

    if (   aMachineState == MachineState_PoweredOff
        || aMachineState == MachineState_Teleported
        || aMachineState == MachineState_Aborted
        || aMachineState == MachineState_Saved)
    {
        /* the machine has stopped execution
         * (or the saved state file was adopted) */
        stsFlags |= SaveSTS_StateTimeStamp;
    }

    if (   (   oldMachineState == MachineState_PoweredOff
            || oldMachineState == MachineState_Aborted
            || oldMachineState == MachineState_Teleported
           )
        && aMachineState == MachineState_Saved)
    {
        /* the saved state file was adopted */
        Assert(!mSSData->mStateFilePath.isEmpty());
        stsFlags |= SaveSTS_StateFilePath;
    }

    if (   aMachineState == MachineState_PoweredOff
        || aMachineState == MachineState_Aborted
        || aMachineState == MachineState_Teleported)
    {
        /* Make sure any transient guest properties get removed from the
         * property store on shutdown. */

        HWData::GuestPropertyList::iterator it;
        BOOL fNeedsSaving = mData->mGuestPropertiesModified;
        if (!fNeedsSaving)
            for (it = mHWData->mGuestProperties.begin();
                 it != mHWData->mGuestProperties.end(); ++it)
                if (it->mFlags & guestProp::TRANSIENT)
                {
                    fNeedsSaving = true;
                    break;
                }
        if (fNeedsSaving)
        {
            mData->mCurrentStateModified = TRUE;
            stsFlags |= SaveSTS_CurStateModified;
            SaveSettings();
        }
    }

    rc = saveStateSettings(stsFlags);

    if (   (   oldMachineState != MachineState_PoweredOff
            && oldMachineState != MachineState_Aborted
            && oldMachineState != MachineState_Teleported
           )
        && (   aMachineState == MachineState_PoweredOff
            || aMachineState == MachineState_Aborted
            || aMachineState == MachineState_Teleported
           )
       )
    {
        /* we've been shut down for any reason */
        /* no special action so far */
    }

    LogFlowThisFunc(("rc=%Rhrc [%s]\n", rc, Global::stringifyMachineState(mData->mMachineState) ));
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
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        AssertReturn(!!mData, E_FAIL);
        directControl = mData->mSession.mDirectControl;

        /* directControl may be already set to NULL here in #OnSessionEnd()
         * called too early by the direct session process while there is still
         * some operation (like deleting the snapshot) in progress. The client
         * process in this case is waiting inside Session::close() for the
         * "end session" process object to complete, while #uninit() called by
         * #checkForDeath() on the Watcher thread is waiting for the pending
         * operation to complete. For now, we accept this inconsitent behavior
         * and simply do nothing here. */

        if (mData->mSession.mState == SessionState_Closing)
            return S_OK;

        AssertReturn(!directControl.isNull(), E_FAIL);
    }

    return directControl->UpdateMachineState(mData->mMachineState);
}
