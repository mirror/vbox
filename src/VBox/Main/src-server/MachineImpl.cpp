/* $Id$ */
/** @file
 * Implementation of IMachine in VBoxSVC.
 */

/*
 * Copyright (C) 2004-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
#include "ProgressProxyImpl.h"
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
#include "DisplayImpl.h"
#include "DisplayUtils.h"
#include "BandwidthControlImpl.h"
#include "MachineImplCloneVM.h"
#include "AutostartDb.h"

// generated header
#include "VBoxEvents.h"

#ifdef VBOX_WITH_USB
# include "USBProxyService.h"
#endif

#include "AutoCaller.h"
#include "HashedPw.h"
#include "Performance.h"

#include <iprt/asm.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/lockvalidator.h>
#include <iprt/process.h>
#include <iprt/cpp/utils.h>
#include <iprt/cpp/xml.h>               /* xml::XmlFileWriter::s_psz*Suff. */
#include <iprt/sha.h>
#include <iprt/string.h>

#include <VBox/com/array.h>
#include <VBox/com/list.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/settings.h>
#include <VBox/vmm/ssm.h>

#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/HostServices/GuestPropertySvc.h>
# include <VBox/com/array.h>
#endif

#include "VBox/com/MultiResult.h"

#include <algorithm>

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
    mRegistered                = FALSE;
    pMachineConfigFile         = NULL;
    /* Contains hints on what has changed when the user is using the VM (config
     * changes, running the VM, ...). This is used to decide if a config needs
     * to be written to disk. */
    flModifications            = 0;
    /* VM modification usually also trigger setting the current state to
     * "Modified". Although this is not always the case. An e.g. is the VM
     * initialization phase or when snapshot related data is changed. The
     * actually behavior is controlled by the following flag. */
    m_fAllowStateModification  = false;
    mAccessible                = FALSE;
    /* mUuid is initialized in Machine::init() */

    mMachineState              = MachineState_PoweredOff;
    RTTimeNow(&mLastStateChange);

    mMachineStateDeps          = 0;
    mMachineStateDepsSem       = NIL_RTSEMEVENTMULTI;
    mMachineStateChangePending = 0;

    mCurrentStateModified      = TRUE;
    mGuestPropertiesModified   = FALSE;

    mSession.mPID              = NIL_RTPROCESS;
    mSession.mState            = SessionState_Unlocked;
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
    mPageFusionEnabled = false;
    mVRAMSize = 8;
    mAccelerate3DEnabled = false;
    mAccelerate2DVideoEnabled = false;
    mMonitorCount = 1;
    mVideoCaptureFile = "Test.webm";
    mVideoCaptureWidth = 640;
    mVideoCaptureHeight = 480;
    mVideoCaptureEnabled = true;

    mHWVirtExEnabled = true;
    mHWVirtExNestedPagingEnabled = true;
#if HC_ARCH_BITS == 64 && !defined(RT_OS_LINUX)
    mHWVirtExLargePagesEnabled = true;
#else
    /* Not supported on 32 bits hosts. */
    mHWVirtExLargePagesEnabled = false;
#endif
    mHWVirtExVPIDEnabled = true;
    mHWVirtExForceEnabled = false;
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
    mHPETEnabled = false;

    /* default boot order: floppy - DVD - HDD */
    mBootOrder[0] = DeviceType_Floppy;
    mBootOrder[1] = DeviceType_DVD;
    mBootOrder[2] = DeviceType_HardDisk;
    for (size_t i = 3; i < RT_ELEMENTS(mBootOrder); ++i)
        mBootOrder[i] = DeviceType_Null;

    mClipboardMode = ClipboardMode_Disabled;
    mDragAndDropMode = DragAndDropMode_Disabled;
    mGuestPropertyNotificationPatterns = "";

    mFirmwareType = FirmwareType_BIOS;
    mKeyboardHIDType = KeyboardHIDType_PS2Keyboard;
    mPointingHIDType = PointingHIDType_PS2Mouse;
    mChipsetType = ChipsetType_PIIX3;
    mEmulatedUSBCardReaderEnabled = FALSE;

    for (size_t i = 0; i < RT_ELEMENTS(mCPUAttached); i++)
        mCPUAttached[i] = false;

    mIOCacheEnabled = true;
    mIOCacheSize    = 5; /* 5MB */

    /* Maximum CPU execution cap by default. */
    mCpuExecutionCap = 100;
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
    : mCollectorGuest(NULL),
      mPeer(NULL),
      mParent(NULL),
      mSerialPorts(),
      mParallelPorts(),
      uRegistryNeedsSaving(0)
{}

Machine::~Machine()
{}

HRESULT Machine::FinalConstruct()
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void Machine::FinalRelease()
{
    LogFlowThisFunc(("\n"));
    uninit();
    BaseFinalRelease();
}

/**
 *  Initializes a new machine instance; this init() variant creates a new, empty machine.
 *  This gets called from VirtualBox::CreateMachine().
 *
 *  @param aParent      Associated parent object
 *  @param strConfigFile  Local file system path to the VM settings file (can
 *                      be relative to the VirtualBox config directory).
 *  @param strName      name for the machine
 *  @param llGroups     list of groups for the machine
 *  @param aOsType      OS Type of this machine or NULL.
 *  @param aId          UUID for the new machine.
 *  @param fForceOverwrite Whether to overwrite an existing machine settings file.
 *
 *  @return  Success indicator. if not S_OK, the machine object is invalid
 */
HRESULT Machine::init(VirtualBox *aParent,
                      const Utf8Str &strConfigFile,
                      const Utf8Str &strName,
                      const StringsList &llGroups,
                      GuestOSType *aOsType,
                      const Guid &aId,
                      bool fForceOverwrite)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("(Init_New) aConfigFile='%s'\n", strConfigFile.c_str()));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = initImpl(aParent, strConfigFile);
    if (FAILED(rc)) return rc;

    rc = tryCreateMachineConfigFile(fForceOverwrite);
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

        mUserData->s.strName = strName;

        mUserData->s.llGroups = llGroups;

        // the "name sync" flag determines whether the machine directory gets renamed along
        // with the machine file; say so if the settings file name is the same as the
        // settings file parent directory (machine directory)
        mUserData->s.fNameSync = isInOwnDir();

        // initialize the default snapshots folder
        rc = COMSETTER(SnapshotFolder)(NULL);
        AssertComRC(rc);

        if (aOsType)
        {
            /* Store OS type */
            mUserData->s.strOsType = aOsType->id();

            /* Apply BIOS defaults */
            mBIOSSettings->applyDefaults(aOsType);

            /* Apply network adapters defaults */
            for (ULONG slot = 0; slot < mNetworkAdapters.size(); ++slot)
                mNetworkAdapters[slot]->applyDefaults(aOsType);

            /* Apply serial port defaults */
            for (ULONG slot = 0; slot < RT_ELEMENTS(mSerialPorts); ++slot)
                mSerialPorts[slot]->applyDefaults(aOsType);
        }

        /* At this point the changing of the current state modification
         * flag is allowed. */
        allowStateModification();

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

    LogFlowThisFunc(("mName='%s', mRegistered=%RTbool, mAccessible=%RTbool, rc=%08X\n",
                     !!mUserData ? mUserData->s.strName.c_str() : "NULL",
                     mData->mRegistered,
                     mData->mAccessible,
                     rc));

    LogFlowThisFuncLeave();

    return rc;
}

/**
 *  Initializes a new instance with data from machine XML (formerly Init_Registered).
 *  Gets called in two modes:
 *
 *      -- from VirtualBox::initMachines() during VirtualBox startup; in that case, the
 *         UUID is specified and we mark the machine as "registered";
 *
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
    LogFlowThisFunc(("(Init_Registered) aConfigFile='%s\n", strConfigFile.c_str()));

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

                // reject VM UUID duplicates, they can happen if someone
                // tries to register an already known VM config again
                if (aParent->findMachine(mData->pMachineConfigFile->uuid,
                                         true /* fPermitInaccessible */,
                                         false /* aDoSetError */,
                                         NULL) != VBOX_E_OBJECT_NOT_FOUND)
                {
                    throw setError(E_FAIL,
                                   tr("Trying to open a VM config '%s' which has the same UUID as an existing virtual machine"),
                                   mData->m_strConfigFile.c_str());
                }

                // use UUID from machine config
                unconst(mData->mUuid) = mData->pMachineConfigFile->uuid;

                rc = loadMachineDataFromSettings(*mData->pMachineConfigFile,
                                                 NULL /* puuidRegistry */);
                if (FAILED(rc)) throw rc;

                /* At this point the changing of the current state modification
                 * flag is allowed. */
                allowStateModification();

                commit();
            }
            catch (HRESULT err)
            {
                /* we assume that error info is set by the thrower */
                rc = err;
            }
            catch (...)
            {
                rc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
            }
        }
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
    {
        if (mData->mAccessible)
            autoInitSpan.setSucceeded();
        else
        {
            autoInitSpan.setLimited();

            // uninit media from this machine's media registry, or else
            // reloading the settings will fail
            mParent->unregisterMachineMedia(getId());
        }
    }

    LogFlowThisFunc(("mName='%s', mRegistered=%RTbool, mAccessible=%RTbool "
                      "rc=%08X\n",
                      !!mUserData ? mUserData->s.strName.c_str() : "NULL",
                      mData->mRegistered, mData->mAccessible, rc));

    LogFlowThisFuncLeave();

    return rc;
}

/**
 *  Initializes a new instance from a machine config that is already in memory
 *  (import OVF case). Since we are importing, the UUID in the machine
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

    Utf8Str strConfigFile;
    aParent->getDefaultMachineFolder(strConfigFile);
    strConfigFile.append(RTPATH_DELIMITER);
    strConfigFile.append(strName);
    strConfigFile.append(RTPATH_DELIMITER);
    strConfigFile.append(strName);
    strConfigFile.append(".vbox");

    HRESULT rc = initImpl(aParent, strConfigFile);
    if (FAILED(rc)) return rc;

    rc = tryCreateMachineConfigFile(false /* fForceOverwrite */);
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

        rc = loadMachineDataFromSettings(config,
                                         &mData->mUuid); // puuidRegistry: initialize media with this registry ID

        // override VM name as well, it may be different
        mUserData->s.strName = strName;

        if (SUCCEEDED(rc))
        {
            /* At this point the changing of the current state modification
             * flag is allowed. */
            allowStateModification();

            /* commit all changes made during the initialization */
            commit();
        }
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
    {
        if (mData->mAccessible)
            autoInitSpan.setSucceeded();
        else
        {
            autoInitSpan.setLimited();

            // uninit media from this machine's media registry, or else
            // reloading the settings will fail
            mParent->unregisterMachineMedia(getId());
        }
    }

    LogFlowThisFunc(("mName='%s', mRegistered=%RTbool, mAccessible=%RTbool "
                     "rc=%08X\n",
                      !!mUserData ? mUserData->s.strName.c_str() : "NULL",
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
                        strConfigFile.c_str(),
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
HRESULT Machine::tryCreateMachineConfigFile(bool fForceOverwrite)
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
        if (!fForceOverwrite)
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Machine settings file '%s' already exists"),
                          mData->m_strConfigFileFull.c_str());
        else
        {
            /* try to delete the config file, as otherwise the creation
             * of a new settings file will fail. */
            int vrc2 = RTFileDelete(mData->m_strConfigFileFull.c_str());
            if (RT_FAILURE(vrc2))
                rc = setError(VBOX_E_FILE_ERROR,
                              tr("Could not delete the existing settings file '%s' (%Rrc)"),
                              mData->m_strConfigFileFull.c_str(), vrc2);
        }
    }
    else if (    vrc != VERR_FILE_NOT_FOUND
              && vrc != VERR_PATH_NOT_FOUND
            )
        rc = setError(VBOX_E_FILE_ERROR,
                      tr("Invalid machine settings file name '%s' (%Rrc)"),
                      mData->m_strConfigFileFull.c_str(),
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
    AssertReturn(!isSessionMachine(), E_FAIL);
    AssertReturn(!isSnapshotMachine(), E_FAIL);
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
                               mData->m_strConfigFileFull.c_str(),
                               mData->mUuid.toString().c_str(),
                               mParent->settingsFilePath().c_str());

            rc = loadMachineDataFromSettings(*mData->pMachineConfigFile,
                                             NULL /* const Guid *puuidRegistry */);
            if (FAILED(rc)) throw rc;
        }
        catch (HRESULT err)
        {
            /* we assume that error info is set by the thrower */
            rc = err;
        }
        catch (...)
        {
            rc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
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
        /// @todo r=klaus for some reason the settings loading logic backs up
        // the settings, and therefore a commit is needed. Should probably be changed.
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

        // uninit media from this machine's media registry, or else
        // reloading the settings will fail
        mParent->unregisterMachineMedia(getId());

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

    Assert(!uRegistryNeedsSaving);
    if (uRegistryNeedsSaving)
    {
        AutoCaller autoCaller(this);
        if (SUCCEEDED(autoCaller.rc()))
        {
            AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
            saveSettings(NULL, Machine::SaveS_Force);
        }
    }

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    Assert(!isSnapshotMachine());
    Assert(!isSessionMachine());
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

    // uninit media from this machine's media registry, if they're still there
    Guid uuidMachine(getId());

    /* XXX This will fail with
     *   "cannot be closed because it is still attached to 1 virtual machines"
     * because at this point we did not call uninitDataAndChildObjects() yet
     * and therefore also removeBackReference() for all these mediums was not called! */
    if (!uuidMachine.isEmpty())     // can be empty if we're called from a failure of Machine::init
        mParent->unregisterMachineMedia(uuidMachine);

    /* the lock is no more necessary (SessionMachine is uninitialized) */
    alock.release();

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
                        mData->mAccessError.getInterfaceID().ref(),
                        Utf8Str(mData->mAccessError.getComponent()).c_str(),
                        Utf8Str(mData->mAccessError.getText()));
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

    mUserData->s.strName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Name)(IN_BSTR aName)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // prohibit setting a UUID only as the machine name, or else it can
    // never be found by findMachine()
    Guid test(aName);
    if (test.isNotEmpty())
        return setError(E_INVALIDARG,  tr("A machine cannot have a UUID as its name"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->s.strName = aName;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(Description)(BSTR *aDescription)
{
    CheckComArgOutPointerValid(aDescription);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mUserData->s.strDescription.cloneTo(aDescription);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Description)(IN_BSTR aDescription)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    // this can be done in principle in any state as it doesn't affect the VM
    // significantly, but play safe by not messing around while complex
    // activities are going on
    HRESULT rc = checkStateDependency(MutableOrSavedStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->s.strDescription = aDescription;

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

STDMETHODIMP Machine::COMGETTER(Groups)(ComSafeArrayOut(BSTR, aGroups))
{
    CheckComArgOutSafeArrayPointerValid(aGroups);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    SafeArray<BSTR> groups(mUserData->s.llGroups.size());
    size_t i = 0;
    for (StringsList::const_iterator it = mUserData->s.llGroups.begin();
         it != mUserData->s.llGroups.end();
         ++it, i++)
    {
        Bstr tmp = *it;
        tmp.cloneTo(&groups[i]);
    }
    groups.detachTo(ComSafeArrayOutArg(aGroups));

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(Groups)(ComSafeArrayIn(IN_BSTR, aGroups))
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    StringsList llGroups;
    HRESULT rc = mParent->convertMachineGroups(ComSafeArrayInArg(aGroups), &llGroups);
    if (FAILED(rc))
        return rc;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    // changing machine groups is possible while the VM is offline
    rc = checkStateDependency(OfflineStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->s.llGroups = llGroups;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(OSTypeId)(BSTR *aOSTypeId)
{
    CheckComArgOutPointerValid(aOSTypeId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mUserData->s.strOsType.cloneTo(aOSTypeId);

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
    mUserData->s.strOsType = osTypeId;

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

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mFirmwareType = aFirmwareType;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(KeyboardHIDType)(KeyboardHIDType_T *aKeyboardHIDType)
{
    CheckComArgOutPointerValid(aKeyboardHIDType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aKeyboardHIDType = mHWData->mKeyboardHIDType;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(KeyboardHIDType)(KeyboardHIDType_T  aKeyboardHIDType)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mKeyboardHIDType = aKeyboardHIDType;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(PointingHIDType)(PointingHIDType_T *aPointingHIDType)
{
    CheckComArgOutPointerValid(aPointingHIDType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aPointingHIDType = mHWData->mPointingHIDType;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(PointingHIDType)(PointingHIDType_T  aPointingHIDType)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mPointingHIDType = aPointingHIDType;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(ChipsetType)(ChipsetType_T *aChipsetType)
{
    CheckComArgOutPointerValid(aChipsetType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aChipsetType = mHWData->mChipsetType;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(ChipsetType)(ChipsetType_T aChipsetType)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    if (aChipsetType != mHWData->mChipsetType)
    {
        setModified(IsModified_MachineData);
        mHWData.backup();
        mHWData->mChipsetType = aChipsetType;

        // Resize network adapter array, to be finalized on commit/rollback.
        // We must not throw away entries yet, otherwise settings are lost
        // without a way to roll back.
        uint32_t newCount = Global::getMaxNetworkAdapters(aChipsetType);
        uint32_t oldCount = mNetworkAdapters.size();
        if (newCount > oldCount)
        {
            mNetworkAdapters.resize(newCount);
            for (ULONG slot = oldCount; slot < mNetworkAdapters.size(); slot++)
            {
                unconst(mNetworkAdapters[slot]).createObject();
                mNetworkAdapters[slot]->init(this, slot);
            }
        }
    }

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(HardwareVersion)(BSTR *aHWVersion)
{
    CheckComArgOutPointerValid(aHWVersion);

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
    CheckComArgOutPointerValid(memorySize);

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
    CheckComArgOutPointerValid(CPUCount);

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

    /* We cant go below the current number of CPUs attached if hotplug is enabled*/
    if (mHWData->mCPUHotPlugEnabled)
    {
        for (unsigned idx = CPUCount; idx < SchemaDefs::MaxCPUCount; idx++)
        {
            if (mHWData->mCPUAttached[idx])
                return setError(E_INVALIDARG,
                                tr("There is still a CPU attached to socket %lu."
                                   "Detach the CPU before removing the socket"),
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

STDMETHODIMP Machine::COMGETTER(CPUExecutionCap)(ULONG *aExecutionCap)
{
    CheckComArgOutPointerValid(aExecutionCap);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aExecutionCap = mHWData->mCpuExecutionCap;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(CPUExecutionCap)(ULONG aExecutionCap)
{
    HRESULT rc = S_OK;

    /* check throttle limits */
    if (    aExecutionCap < 1
         || aExecutionCap > 100
       )
        return setError(E_INVALIDARG,
                        tr("Invalid CPU execution cap value: %lu (must be in range [%lu, %lu])"),
                        aExecutionCap, 1, 100);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    alock.release();
    rc = onCPUExecutionCapChange(aExecutionCap);
    alock.acquire();
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mCpuExecutionCap = aExecutionCap;

    /* Save settings if online - todo why is this required?? */
    if (Global::IsOnline(mData->mMachineState))
        saveSettings(NULL);

    return S_OK;
}


STDMETHODIMP Machine::COMGETTER(CPUHotPlugEnabled)(BOOL *enabled)
{
    CheckComArgOutPointerValid(enabled);

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
                                tr("CPU hotplugging can't be disabled because the maximum number of CPUs is not equal to the amount of CPUs attached"));

            setModified(IsModified_MachineData);
            mHWData.backup();
        }
    }

    mHWData->mCPUHotPlugEnabled = enabled;

    return rc;
}

STDMETHODIMP Machine::COMGETTER(EmulatedUSBCardReaderEnabled)(BOOL *enabled)
{
#ifdef VBOX_WITH_USB_CARDREADER
    CheckComArgOutPointerValid(enabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = mHWData->mEmulatedUSBCardReaderEnabled;

    return S_OK;
#else
    NOREF(enabled);
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Machine::COMSETTER(EmulatedUSBCardReaderEnabled)(BOOL enabled)
{
#ifdef VBOX_WITH_USB_CARDREADER
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mEmulatedUSBCardReaderEnabled = enabled;

    return S_OK;
#else
    NOREF(enabled);
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Machine::COMGETTER(EmulatedUSBWebcameraEnabled)(BOOL *enabled)
{
    NOREF(enabled);
    return E_NOTIMPL;
}

STDMETHODIMP Machine::COMSETTER(EmulatedUSBWebcameraEnabled)(BOOL enabled)
{
    NOREF(enabled);
    return E_NOTIMPL;
}

STDMETHODIMP Machine::COMGETTER(HPETEnabled)(BOOL *enabled)
{
    CheckComArgOutPointerValid(enabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = mHWData->mHPETEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(HPETEnabled)(BOOL enabled)
{
    HRESULT rc = S_OK;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();

    mHWData->mHPETEnabled = enabled;

    return rc;
}

STDMETHODIMP Machine::COMGETTER(VideoCaptureEnabled)(BOOL * fEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *fEnabled = mHWData->mVideoCaptureEnabled;
    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(VideoCaptureEnabled)(BOOL  fEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    mHWData->mVideoCaptureEnabled = fEnabled;
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(VideoCaptureFile)(BSTR * apFile)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    mHWData->mVideoCaptureFile.cloneTo(apFile);
    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(VideoCaptureFile)(IN_BSTR aFile)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    mHWData->mVideoCaptureFile = aFile;
    return S_OK;
}


STDMETHODIMP Machine::COMGETTER(VideoCaptureWidth)(ULONG *ulHorzRes)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *ulHorzRes = mHWData->mVideoCaptureWidth;
    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(VideoCaptureWidth)(ULONG ulHorzRes)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc()))
    {
        LogFlow(("Autolocked failed\n"));
        return autoCaller.rc();
    }

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    mHWData->mVideoCaptureWidth = ulHorzRes;
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(VideoCaptureHeight)(ULONG *ulVertRes)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
     *ulVertRes = mHWData->mVideoCaptureHeight;
    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(VideoCaptureHeight)(ULONG ulVertRes)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    mHWData->mVideoCaptureHeight = ulVertRes;
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(VRAMSize)(ULONG *memorySize)
{
    CheckComArgOutPointerValid(memorySize);

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
    CheckComArgOutPointerValid(memoryBalloonSize);

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

STDMETHODIMP Machine::COMGETTER(PageFusionEnabled) (BOOL *enabled)
{
    CheckComArgOutPointerValid(enabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *enabled = mHWData->mPageFusionEnabled;
    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(PageFusionEnabled) (BOOL enabled)
{
#ifdef VBOX_WITH_PAGE_SHARING
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /** @todo must support changes for running vms and keep this in sync with IGuest. */
    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mPageFusionEnabled = enabled;
    return S_OK;
#else
    NOREF(enabled);
    return setError(E_NOTIMPL, tr("Page fusion is only supported on 64-bit hosts"));
#endif
}

STDMETHODIMP Machine::COMGETTER(Accelerate3DEnabled)(BOOL *enabled)
{
    CheckComArgOutPointerValid(enabled);

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
    CheckComArgOutPointerValid(enabled);

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
    CheckComArgOutPointerValid(monitorCount);

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
    CheckComArgOutPointerValid(biosSettings);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mBIOSSettings is constant during life time, no need to lock */
    mBIOSSettings.queryInterfaceTo(biosSettings);

    return S_OK;
}

STDMETHODIMP Machine::GetCPUProperty(CPUPropertyType_T property, BOOL *aVal)
{
    CheckComArgOutPointerValid(aVal);

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
                return E_INVALIDARG;

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
                return E_INVALIDARG;

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
            AssertCompile(RT_ELEMENTS(mHWData->mCpuIdStdLeafs) == 0xB);
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
            AssertCompile(RT_ELEMENTS(mHWData->mCpuIdExtLeafs) == 0xB);
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
            AssertCompile(RT_ELEMENTS(mHWData->mCpuIdStdLeafs) == 0xB);
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
            AssertCompile(RT_ELEMENTS(mHWData->mCpuIdExtLeafs) == 0xB);
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
    CheckComArgOutPointerValid(aVal);

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
#if defined(DEBUG_bird) && defined(RT_OS_LINUX) /* This feature is deadly here */
            *aVal = FALSE;
#endif
            break;

        case HWVirtExPropertyType_Force:
            *aVal = mHWData->mHWVirtExForceEnabled;
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

        case HWVirtExPropertyType_Force:
            setModified(IsModified_MachineData);
            mHWData.backup();
            mHWData->mHWVirtExForceEnabled = !!aVal;
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

    Utf8Str strFullSnapshotFolder;
    calculateFullPath(mUserData->s.strSnapshotFolder, strFullSnapshotFolder);
    strFullSnapshotFolder.cloneTo(aSnapshotFolder);

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

    Utf8Str strSnapshotFolder0(aSnapshotFolder);       // keep original

    Utf8Str strSnapshotFolder(strSnapshotFolder0);
    if (strSnapshotFolder.isEmpty())
        strSnapshotFolder = "Snapshots";
    int vrc = calculateFullPath(strSnapshotFolder,
                                strSnapshotFolder);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL,
                        tr("Invalid snapshot folder '%ls' (%Rrc)"),
                        aSnapshotFolder, vrc);

    setModified(IsModified_MachineData);
    mUserData.backup();

    copyPathRelativeToMachine(strSnapshotFolder, mUserData->s.strSnapshotFolder);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(MediumAttachments)(ComSafeArrayOut(IMediumAttachment*, aAttachments))
{
    CheckComArgOutSafeArrayPointerValid(aAttachments);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IMediumAttachment> attachments(mMediaData->mAttachments);
    attachments.detachTo(ComSafeArrayOutArg(aAttachments));

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(VRDEServer)(IVRDEServer **vrdeServer)
{
    CheckComArgOutPointerValid(vrdeServer);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(!!mVRDEServer);
    mVRDEServer.queryInterfaceTo(vrdeServer);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(AudioAdapter)(IAudioAdapter **audioAdapter)
{
    CheckComArgOutPointerValid(audioAdapter);

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

    clearError();
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
     * (w/o treating it as a failure), for example, as in OSE */
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

STDMETHODIMP Machine::COMGETTER(SessionPID)(ULONG *aSessionPID)
{
    CheckComArgOutPointerValid(aSessionPID);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSessionPID = mData->mSession.mPID;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(State)(MachineState_T *machineState)
{
    CheckComArgOutPointerValid(machineState);

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

    mSSData->strStateFilePath.cloneTo(aStateFilePath);

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
    logFolder.cloneTo(aLogFolder);

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
    HRESULT rc = S_OK;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    alock.release();
    rc = onClipboardModeChange(aClipboardMode);
    alock.acquire();
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mClipboardMode = aClipboardMode;

    /* Save settings if online - todo why is this required?? */
    if (Global::IsOnline(mData->mMachineState))
        saveSettings(NULL);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(DragAndDropMode)(DragAndDropMode_T *aDragAndDropMode)
{
    CheckComArgOutPointerValid(aDragAndDropMode);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aDragAndDropMode = mHWData->mDragAndDropMode;

    return S_OK;
}

STDMETHODIMP
Machine::COMSETTER(DragAndDropMode)(DragAndDropMode_T aDragAndDropMode)
{
    HRESULT rc = S_OK;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    alock.release();
    rc = onDragAndDropModeChange(aDragAndDropMode);
    alock.acquire();
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mDragAndDropMode = aDragAndDropMode;

    /* Save settings if online - todo why is this required?? */
    if (Global::IsOnline(mData->mMachineState))
        saveSettings(NULL);

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
        return VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
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

    *aEnabled = mUserData->s.fTeleporterEnabled;

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
        &&  (   !isSessionMachine()
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
    mUserData->s.fTeleporterEnabled = !!aEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(TeleporterPort)(ULONG *aPort)
{
    CheckComArgOutPointerValid(aPort);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aPort = (ULONG)mUserData->s.uTeleporterPort;

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
    mUserData->s.uTeleporterPort = (uint32_t)aPort;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(TeleporterAddress)(BSTR *aAddress)
{
    CheckComArgOutPointerValid(aAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mUserData->s.strTeleporterAddress.cloneTo(aAddress);

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
    mUserData->s.strTeleporterAddress = aAddress;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(TeleporterPassword)(BSTR *aPassword)
{
    CheckComArgOutPointerValid(aPassword);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        mUserData->s.strTeleporterPassword.cloneTo(aPassword);
    }

    return hrc;
}

STDMETHODIMP Machine::COMSETTER(TeleporterPassword)(IN_BSTR aPassword)
{
    /*
     * Hash the password first.
     */
    Utf8Str strPassword(aPassword);
    if (!strPassword.isEmpty())
    {
        if (VBoxIsPasswordHashed(&strPassword))
            return setError(E_INVALIDARG, tr("Cannot set an already hashed password, only plain text password please"));
        VBoxHashPassword(&strPassword);
    }

    /*
     * Do the update.
     */
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        hrc = checkStateDependency(MutableStateDep);
        if (SUCCEEDED(hrc))
        {
            setModified(IsModified_MachineData);
            mUserData.backup();
            mUserData->s.strTeleporterPassword = strPassword;
        }
    }

    return hrc;
}

STDMETHODIMP Machine::COMGETTER(FaultToleranceState)(FaultToleranceState_T *aState)
{
    CheckComArgOutPointerValid(aState);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aState = mUserData->s.enmFaultToleranceState;
    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(FaultToleranceState)(FaultToleranceState_T aState)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* @todo deal with running state change. */
    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->s.enmFaultToleranceState = aState;
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(FaultToleranceAddress)(BSTR *aAddress)
{
    CheckComArgOutPointerValid(aAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mUserData->s.strFaultToleranceAddress.cloneTo(aAddress);
    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(FaultToleranceAddress)(IN_BSTR aAddress)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* @todo deal with running state change. */
    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->s.strFaultToleranceAddress = aAddress;
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(FaultTolerancePort)(ULONG *aPort)
{
    CheckComArgOutPointerValid(aPort);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aPort = mUserData->s.uFaultTolerancePort;
    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(FaultTolerancePort)(ULONG aPort)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* @todo deal with running state change. */
    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->s.uFaultTolerancePort = aPort;
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(FaultTolerancePassword)(BSTR *aPassword)
{
    CheckComArgOutPointerValid(aPassword);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mUserData->s.strFaultTolerancePassword.cloneTo(aPassword);

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(FaultTolerancePassword)(IN_BSTR aPassword)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* @todo deal with running state change. */
    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->s.strFaultTolerancePassword = aPassword;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(FaultToleranceSyncInterval)(ULONG *aInterval)
{
    CheckComArgOutPointerValid(aInterval);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aInterval = mUserData->s.uFaultToleranceInterval;
    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(FaultToleranceSyncInterval)(ULONG aInterval)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* @todo deal with running state change. */
    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mUserData.backup();
    mUserData->s.uFaultToleranceInterval = aInterval;
    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(RTCUseUTC)(BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = mUserData->s.fRTCUseUTC;

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
        &&  (   !isSessionMachine()
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
    mUserData->s.fRTCUseUTC = !!aEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(IOCacheEnabled)(BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = mHWData->mIOCacheEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(IOCacheEnabled)(BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mIOCacheEnabled = aEnabled;

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(IOCacheSize)(ULONG *aIOCacheSize)
{
    CheckComArgOutPointerValid(aIOCacheSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aIOCacheSize = mHWData->mIOCacheSize;

    return S_OK;
}

STDMETHODIMP Machine::COMSETTER(IOCacheSize)(ULONG  aIOCacheSize)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mIOCacheSize = aIOCacheSize;

    return S_OK;
}


/**
 *  @note Locks objects!
 */
STDMETHODIMP Machine::LockMachine(ISession *aSession,
                                  LockType_T lockType)
{
    CheckComArgNotNull(aSession);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* check the session state */
    SessionState_T state;
    HRESULT rc = aSession->COMGETTER(State)(&state);
    if (FAILED(rc)) return rc;

    if (state != SessionState_Unlocked)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("The given session is busy"));

    // get the client's IInternalSessionControl interface
    ComPtr<IInternalSessionControl> pSessionControl = aSession;
    ComAssertMsgRet(!!pSessionControl, ("No IInternalSessionControl interface"),
                    E_INVALIDARG);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mData->mRegistered)
        return setError(E_UNEXPECTED,
                        tr("The machine '%s' is not registered"),
                        mUserData->s.strName.c_str());

    LogFlowThisFunc(("mSession.mState=%s\n", Global::stringifySessionState(mData->mSession.mState)));

    SessionState_T oldState = mData->mSession.mState;
    /* Hack: in case the session is closing and there is a progress object
     * which allows waiting for the session to be closed, take the opportunity
     * and do a limited wait (max. 1 second). This helps a lot when the system
     * is busy and thus session closing can take a little while. */
    if (    mData->mSession.mState == SessionState_Unlocking
        &&  mData->mSession.mProgress)
    {
        alock.release();
        mData->mSession.mProgress->WaitForCompletion(1000);
        alock.acquire();
        LogFlowThisFunc(("after waiting: mSession.mState=%s\n", Global::stringifySessionState(mData->mSession.mState)));
    }

    // try again now
    if (    (mData->mSession.mState == SessionState_Locked)         // machine is write-locked already (i.e. session machine exists)
         && (lockType == LockType_Shared)                           // caller wants a shared link to the existing session that holds the write lock:
       )
    {
        // OK, share the session... we are now dealing with three processes:
        // 1) VBoxSVC (where this code runs);
        // 2) process C: the caller's client process (who wants a shared session);
        // 3) process W: the process which already holds the write lock on the machine (write-locking session)

        // copy pointers to W (the write-locking session) before leaving lock (these must not be NULL)
        ComPtr<IInternalSessionControl> pSessionW = mData->mSession.mDirectControl;
        ComAssertRet(!pSessionW.isNull(), E_FAIL);
        ComObjPtr<SessionMachine> pSessionMachine = mData->mSession.mMachine;
        AssertReturn(!pSessionMachine.isNull(), E_FAIL);

        /*
         *  Release the lock before calling the client process. It's safe here
         *  since the only thing to do after we get the lock again is to add
         *  the remote control to the list (which doesn't directly influence
         *  anything).
         */
        alock.release();

        // get the console of the session holding the write lock (this is a remote call)
        ComPtr<IConsole> pConsoleW;
        LogFlowThisFunc(("Calling GetRemoteConsole()...\n"));
        rc = pSessionW->GetRemoteConsole(pConsoleW.asOutParam());
        LogFlowThisFunc(("GetRemoteConsole() returned %08X\n", rc));
        if (FAILED(rc))
            // the failure may occur w/o any error info (from RPC), so provide one
            return setError(VBOX_E_VM_ERROR,
                            tr("Failed to get a console object from the direct session (%Rrc)"), rc);

        ComAssertRet(!pConsoleW.isNull(), E_FAIL);

        // share the session machine and W's console with the caller's session
        LogFlowThisFunc(("Calling AssignRemoteMachine()...\n"));
        rc = pSessionControl->AssignRemoteMachine(pSessionMachine, pConsoleW);
        LogFlowThisFunc(("AssignRemoteMachine() returned %08X\n", rc));

        if (FAILED(rc))
            // the failure may occur w/o any error info (from RPC), so provide one
            return setError(VBOX_E_VM_ERROR,
                            tr("Failed to assign the machine to the session (%Rrc)"), rc);
        alock.acquire();

        // need to revalidate the state after acquiring the lock again
        if (mData->mSession.mState != SessionState_Locked)
        {
            pSessionControl->Uninitialize();
            return setError(VBOX_E_INVALID_SESSION_STATE,
                            tr("The machine '%s' was unlocked unexpectedly while attempting to share its session"),
                               mUserData->s.strName.c_str());
        }

        // add the caller's session to the list
        mData->mSession.mRemoteControls.push_back(pSessionControl);
    }
    else if (    mData->mSession.mState == SessionState_Locked
              || mData->mSession.mState == SessionState_Unlocking
            )
    {
        // sharing not permitted, or machine still unlocking:
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("The machine '%s' is already locked for a session (or being unlocked)"),
                        mUserData->s.strName.c_str());
    }
    else
    {
        // machine is not locked: then write-lock the machine (create the session machine)

        // must not be busy
        AssertReturn(!Global::IsOnlineOrTransient(mData->mMachineState), E_FAIL);

        // get the caller's session PID
        RTPROCESS pid = NIL_RTPROCESS;
        AssertCompile(sizeof(ULONG) == sizeof(RTPROCESS));
        pSessionControl->GetPID((ULONG*)&pid);
        Assert(pid != NIL_RTPROCESS);

        bool fLaunchingVMProcess = (mData->mSession.mState == SessionState_Spawning);

        if (fLaunchingVMProcess)
        {
            // this machine is awaiting for a spawning session to be opened:
            // then the calling process must be the one that got started by
            // LaunchVMProcess()

            LogFlowThisFunc(("mSession.mPID=%d(0x%x)\n", mData->mSession.mPID, mData->mSession.mPID));
            LogFlowThisFunc(("session.pid=%d(0x%x)\n", pid, pid));

            if (mData->mSession.mPID != pid)
                return setError(E_ACCESSDENIED,
                                tr("An unexpected process (PID=0x%08X) has tried to lock the "
                                   "machine '%s', while only the process started by LaunchVMProcess (PID=0x%08X) is allowed"),
                                pid, mUserData->s.strName.c_str(), mData->mSession.mPID);
        }

        // create the mutable SessionMachine from the current machine
        ComObjPtr<SessionMachine> sessionMachine;
        sessionMachine.createObject();
        rc = sessionMachine->init(this);
        AssertComRC(rc);

        /* NOTE: doing return from this function after this point but
         * before the end is forbidden since it may call SessionMachine::uninit()
         * (through the ComObjPtr's destructor) which requests the VirtualBox write
         * lock while still holding the Machine lock in alock so that a deadlock
         * is possible due to the wrong lock order. */

        if (SUCCEEDED(rc))
        {
            /*
             *  Set the session state to Spawning to protect against subsequent
             *  attempts to open a session and to unregister the machine after
             *  we release the lock.
             */
            SessionState_T origState = mData->mSession.mState;
            mData->mSession.mState = SessionState_Spawning;

            /*
             *  Release the lock before calling the client process -- it will call
             *  Machine/SessionMachine methods. Releasing the lock here is quite safe
             *  because the state is Spawning, so that LaunchVMProcess() and
             *  LockMachine() calls will fail. This method, called before we
             *  acquire the lock again, will fail because of the wrong PID.
             *
             *  Note that mData->mSession.mRemoteControls accessed outside
             *  the lock may not be modified when state is Spawning, so it's safe.
             */
            alock.release();

            LogFlowThisFunc(("Calling AssignMachine()...\n"));
            rc = pSessionControl->AssignMachine(sessionMachine, lockType);
            LogFlowThisFunc(("AssignMachine() returned %08X\n", rc));

            /* The failure may occur w/o any error info (from RPC), so provide one */
            if (FAILED(rc))
                setError(VBOX_E_VM_ERROR,
                         tr("Failed to assign the machine to the session (%Rrc)"), rc);

            if (    SUCCEEDED(rc)
                 && fLaunchingVMProcess
               )
            {
                /* complete the remote session initialization */

                /* get the console from the direct session */
                ComPtr<IConsole> console;
                rc = pSessionControl->GetRemoteConsole(console.asOutParam());
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
                     *  after LaunchVMProcess(), the first and the only
                     *  entry in remoteControls is that remote session
                     */
                    LogFlowThisFunc(("Calling AssignRemoteMachine()...\n"));
                    rc = mData->mSession.mRemoteControls.front()->AssignRemoteMachine(sessionMachine, console);
                    LogFlowThisFunc(("AssignRemoteMachine() returned %08X\n", rc));

                    /* The failure may occur w/o any error info (from RPC), so provide one */
                    if (FAILED(rc))
                        setError(VBOX_E_VM_ERROR,
                                 tr("Failed to assign the machine to the remote session (%Rrc)"), rc);
                }

                if (FAILED(rc))
                    pSessionControl->Uninitialize();
            }

            /* acquire the lock again */
            alock.acquire();

            /* Restore the session state */
            mData->mSession.mState = origState;
        }

        // finalize spawning anyway (this is why we don't return on errors above)
        if (fLaunchingVMProcess)
        {
            /* Note that the progress object is finalized later */
            /** @todo Consider checking mData->mSession.mProgress for cancellation
             *        around here.  */

            /* We don't reset mSession.mPID here because it is necessary for
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
                mData->mSession.mState = SessionState_Unlocked;
            }
        }
        else
        {
            /* memorize PID of the directly opened session */
            if (SUCCEEDED(rc))
                mData->mSession.mPID = pid;
        }

        if (SUCCEEDED(rc))
        {
            /* memorize the direct session control and cache IUnknown for it */
            mData->mSession.mDirectControl = pSessionControl;
            mData->mSession.mState = SessionState_Locked;
            /* associate the SessionMachine with this Machine */
            mData->mSession.mMachine = sessionMachine;

            /* request an IUnknown pointer early from the remote party for later
             * identity checks (it will be internally cached within mDirectControl
             * at least on XPCOM) */
            ComPtr<IUnknown> unk = mData->mSession.mDirectControl;
            NOREF(unk);
        }

        /* Release the lock since SessionMachine::uninit() locks VirtualBox which
         * would break the lock order */
        alock.release();

        /* uninitialize the created session machine on failure */
        if (FAILED(rc))
            sessionMachine->uninit();

    }

    if (SUCCEEDED(rc))
    {
        /*
         *  tell the client watcher thread to update the set of
         *  machines that have open sessions
         */
        mParent->updateClientWatcher();

        if (oldState != SessionState_Locked)
            /* fire an event */
            mParent->onSessionStateChange(getId(), SessionState_Locked);
    }

    return rc;
}

/**
 *  @note Locks objects!
 */
STDMETHODIMP Machine::LaunchVMProcess(ISession *aSession,
                                      IN_BSTR aType,
                                      IN_BSTR aEnvironment,
                                      IProgress **aProgress)
{
    CheckComArgStrNotEmptyOrNull(aType);
    Utf8Str strType(aType);
    Utf8Str strEnvironment(aEnvironment);
    /* "emergencystop" doesn't need the session, so skip the checks/interface
     * retrieval. This code doesn't quite fit in here, but introducing a
     * special API method would be even more effort, and would require explicit
     * support by every API client. It's better to hide the feature a bit. */
    if (strType != "emergencystop")
        CheckComArgNotNull(aSession);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComPtr<IInternalSessionControl> control;
    HRESULT rc = S_OK;

    if (strType != "emergencystop")
    {
        /* check the session state */
        SessionState_T state;
        rc = aSession->COMGETTER(State)(&state);
        if (FAILED(rc))
            return rc;

        if (state != SessionState_Unlocked)
            return setError(VBOX_E_INVALID_OBJECT_STATE,
                            tr("The given session is busy"));

        /* get the IInternalSessionControl interface */
        control = aSession;
        ComAssertMsgRet(!control.isNull(),
                        ("No IInternalSessionControl interface"),
                        E_INVALIDARG);
    }

    /* get the teleporter enable state for the progress object init. */
    BOOL fTeleporterEnabled;
    rc = COMGETTER(TeleporterEnabled)(&fTeleporterEnabled);
    if (FAILED(rc))
        return rc;

    /* create a progress object */
    if (strType != "emergencystop")
    {
        ComObjPtr<ProgressProxy> progress;
        progress.createObject();
        rc = progress->init(mParent,
                            static_cast<IMachine*>(this),
                            Bstr(tr("Starting VM")).raw(),
                            TRUE /* aCancelable */,
                            fTeleporterEnabled ? 20 : 10 /* uTotalOperationsWeight */,
                            BstrFmt(tr("Creating process for virtual machine \"%s\" (%s)"), mUserData->s.strName.c_str(), strType.c_str()).raw(),
                            2 /* uFirstOperationWeight */,
                            fTeleporterEnabled ? 3 : 1 /* cOtherProgressObjectOperations */);

        if (SUCCEEDED(rc))
        {
            rc = launchVMProcess(control, strType, strEnvironment, progress);
            if (SUCCEEDED(rc))
            {
                progress.queryInterfaceTo(aProgress);

                /* signal the client watcher thread */
                mParent->updateClientWatcher();

                /* fire an event */
                mParent->onSessionStateChange(getId(), SessionState_Spawning);
            }
        }
    }
    else
    {
        /* no progress object - either instant success or failure */
        *aProgress = NULL;

        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (mData->mSession.mState != SessionState_Locked)
            return setError(VBOX_E_INVALID_OBJECT_STATE,
                            tr("The machine '%s' is not locked by a session"),
                            mUserData->s.strName.c_str());

        /* must have a VM process associated - do not kill normal API clients
         * with an open session */
        if (!Global::IsOnline(mData->mMachineState))
            return setError(VBOX_E_INVALID_OBJECT_STATE,
                            tr("The machine '%s' does not have a VM process"),
                            mUserData->s.strName.c_str());

        /* forcibly terminate the VM process */
        if (mData->mSession.mPID != NIL_RTPROCESS)
            RTProcTerminate(mData->mSession.mPID);

        /* signal the client watcher thread, as most likely the client has
         * been terminated */
        mParent->updateClientWatcher();
    }

    return rc;
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
                                   IMedium *aMedium)
{
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d aType=%d aMedium=%p\n",
                     aControllerName, aControllerPort, aDevice, aType, aMedium));

    CheckComArgStrNotEmptyOrNull(aControllerName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // request the host lock first, since might be calling Host methods for getting host drives;
    // next, protect the media tree all the while we're in here, as well as our member variables
    AutoMultiWriteLock2 alock(mParent->host(), this COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock treeLock(&mParent->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    /// @todo NEWMEDIA implicit machine registration
    if (!mData->mRegistered)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("Cannot attach storage devices to an unregistered machine"));

    AssertReturn(mData->mMachineState != MachineState_Saved, E_FAIL);

    /* Check for an existing controller. */
    ComObjPtr<StorageController> ctl;
    rc = getStorageControllerByName(aControllerName, ctl, true /* aSetError */);
    if (FAILED(rc)) return rc;

    StorageControllerType_T ctrlType;
    rc = ctl->COMGETTER(ControllerType)(&ctrlType);
    if (FAILED(rc))
        return setError(E_FAIL,
                        tr("Could not get type of controller '%ls'"),
                        aControllerName);

    /* Check that the controller can do hotplugging if we detach the device while the VM is running. */
    bool fHotplug = false;
    if (Global::IsOnlineOrTransient(mData->mMachineState))
        fHotplug = true;

    if (fHotplug && !isControllerHotplugCapable(ctrlType))
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Controller '%ls' does not support hotplugging"),
                        aControllerName);

    // check that the port and device are not out of range
    rc = ctl->checkPortAndDeviceValid(aControllerPort, aDevice);
    if (FAILED(rc)) return rc;

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
                            tr("Medium '%s' is already attached to port %d, device %d of controller '%ls' of this virtual machine"),
                            pMedium->getLocationFull().c_str(),
                            aControllerPort,
                            aDevice,
                            aControllerName);
        }
        else
            return setError(VBOX_E_OBJECT_IN_USE,
                            tr("Device is already attached to port %d, device %d of controller '%ls' of this virtual machine"),
                            aControllerPort, aDevice, aControllerName);
    }

    ComObjPtr<Medium> medium = static_cast<Medium*>(aMedium);
    if (aMedium && medium.isNull())
        return setError(E_INVALIDARG, "The given medium pointer is invalid");

    AutoCaller mediumCaller(medium);
    if (FAILED(mediumCaller.rc())) return mediumCaller.rc();

    AutoWriteLock mediumLock(medium COMMA_LOCKVAL_SRC_POS);

    if (    (pAttachTemp = findAttachment(mMediaData->mAttachments, medium))
         && !medium.isNull()
       )
        return setError(VBOX_E_OBJECT_IN_USE,
                        tr("Medium '%s' is already attached to this virtual machine"),
                        medium->getLocationFull().c_str());

    if (!medium.isNull())
    {
        MediumType_T mtype = medium->getType();
        // MediumType_Readonly is also new, but only applies to DVDs and floppies.
        // For DVDs it's not written to the config file, so needs no global config
        // version bump. For floppies it's a new attribute "type", which is ignored
        // by older VirtualBox version, so needs no global config version bump either.
        // For hard disks this type is not accepted.
        if (mtype == MediumType_MultiAttach)
        {
            // This type is new with VirtualBox 4.0 and therefore requires settings
            // version 1.11 in the settings backend. Unfortunately it is not enough to do
            // the usual routine in MachineConfigFile::bumpSettingsVersionIfNeeded() for
            // two reasons: The medium type is a property of the media registry tree, which
            // can reside in the global config file (for pre-4.0 media); we would therefore
            // possibly need to bump the global config version. We don't want to do that though
            // because that might make downgrading to pre-4.0 impossible.
            // As a result, we can only use these two new types if the medium is NOT in the
            // global registry:
            const Guid &uuidGlobalRegistry = mParent->getGlobalRegistryId();
            if (    medium->isInRegistry(uuidGlobalRegistry)
                 || !mData->pMachineConfigFile->canHaveOwnMediaRegistry()
               )
                return setError(VBOX_E_INVALID_OBJECT_STATE,
                                tr("Cannot attach medium '%s': the media type 'MultiAttach' can only be attached "
                                   "to machines that were created with VirtualBox 4.0 or later"),
                                medium->getLocationFull().c_str());
        }
    }

    bool fIndirect = false;
    if (!medium.isNull())
        fIndirect = medium->isReadOnly();
    bool associate = true;

    do
    {
        if (    aType == DeviceType_HardDisk
             && mMediaData.isBackedUp())
        {
            const MediaData::AttachmentList &oldAtts = mMediaData.backedUpData()->mAttachments;

            /* check if the medium was attached to the VM before we started
             * changing attachments in which case the attachment just needs to
             * be restored */
            if ((pAttachTemp = findAttachment(oldAtts, medium)))
            {
                AssertReturn(!fIndirect, E_FAIL);

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
        if (!fIndirect)
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

                    if (pMedium->getBase(&level) == medium)
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
                    fIndirect = false;
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

                MediumAttachment *pAttachFound = NULL;
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
                    if (pMedium->getBase(&level) == medium)
                    {
                        /* matched device, channel and bus (i.e. attached to the
                         * same place) will win and immediately stop the search;
                         * otherwise the attachment that has the youngest
                         * descendant of medium will be used
                         */
                        if (    pAttach->getDevice() == aDevice
                             && pAttach->getPort() == aControllerPort
                             && pAttach->getControllerName() == aControllerName
                           )
                        {
                            pAttachFound = pAttach;
                            break;
                        }
                        else if (    !pAttachFound
                                  || level > foundLevel /* prefer younger */
                                )
                        {
                            pAttachFound = pAttach;
                            foundLevel = level;
                        }
                    }
                }

                if (pAttachFound)
                {
                    base = pAttachFound->getMedium();
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

        Utf8Str strFullSnapshotFolder;
        calculateFullPath(mUserData->s.strSnapshotFolder, strFullSnapshotFolder);

        ComObjPtr<Medium> diff;
        diff.createObject();
        // store this diff in the same registry as the parent
        Guid uuidRegistryParent;
        if (!medium->getFirstRegistryMachineId(uuidRegistryParent))
        {
            // parent image has no registry: this can happen if we're attaching a new immutable
            // image that has not yet been attached (medium then points to the base and we're
            // creating the diff image for the immutable, and the parent is not yet registered);
            // put the parent in the machine registry then
            mediumLock.release();
            treeLock.release();
            alock.release();
            addMediumToRegistry(medium);
            alock.acquire();
            treeLock.acquire();
            mediumLock.acquire();
            medium->getFirstRegistryMachineId(uuidRegistryParent);
        }
        rc = diff->init(mParent,
                        medium->getPreferredDiffFormat(),
                        strFullSnapshotFolder.append(RTPATH_SLASH_STR),
                        uuidRegistryParent);
        if (FAILED(rc)) return rc;

        /* Apply the normal locking logic to the entire chain. */
        MediumLockList *pMediumLockList(new MediumLockList());
        mediumLock.release();
        treeLock.release();
        rc = diff->createMediumLockList(true /* fFailIfInaccessible */,
                                        true /* fMediumLockWrite */,
                                        medium,
                                        *pMediumLockList);
        treeLock.acquire();
        mediumLock.acquire();
        if (SUCCEEDED(rc))
        {
            mediumLock.release();
            treeLock.release();
            rc = pMediumLockList->Lock();
            treeLock.acquire();
            mediumLock.acquire();
            if (FAILED(rc))
                setError(rc,
                         tr("Could not lock medium when creating diff '%s'"),
                         diff->getLocationFull().c_str());
            else
            {
                /* will release the lock before the potentially lengthy
                 * operation, so protect with the special state */
                MachineState_T oldState = mData->mMachineState;
                setMachineState(MachineState_SettingUp);

                mediumLock.release();
                treeLock.release();
                alock.release();

                rc = medium->createDiffStorage(diff,
                                               MediumVariant_Standard,
                                               pMediumLockList,
                                               NULL /* aProgress */,
                                               true /* aWait */);

                alock.acquire();
                treeLock.acquire();
                mediumLock.acquire();

                setMachineState(oldState);
            }
        }

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
    rc = attachment->init(this,
                          medium,
                          aControllerName,
                          aControllerPort,
                          aDevice,
                          aType,
                          fIndirect,
                          false /* fPassthrough */,
                          false /* fTempEject */,
                          false /* fNonRotational */,
                          false /* fDiscard */,
                          Utf8Str::Empty);
    if (FAILED(rc)) return rc;

    if (associate && !medium.isNull())
    {
        // as the last step, associate the medium to the VM
        rc = medium->addBackReference(mData->mUuid);
        // here we can fail because of Deleting, or being in process of creating a Diff
        if (FAILED(rc)) return rc;

        mediumLock.release();
        treeLock.release();
        alock.release();
        addMediumToRegistry(medium);
        alock.acquire();
        treeLock.acquire();
        mediumLock.acquire();
    }

    /* success: finally remember the attachment */
    setModified(IsModified_Storage);
    mMediaData.backup();
    mMediaData->mAttachments.push_back(attachment);

    mediumLock.release();
    treeLock.release();
    alock.release();

    if (fHotplug)
        rc = onStorageDeviceChange(attachment, FALSE /* aRemove */);

    mParent->saveModifiedRegistries();

    return rc;
}

STDMETHODIMP Machine::DetachDevice(IN_BSTR aControllerName, LONG aControllerPort,
                                   LONG aDevice)
{
    CheckComArgStrNotEmptyOrNull(aControllerName);

    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d\n",
                     aControllerName, aControllerPort, aDevice));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    AssertReturn(mData->mMachineState != MachineState_Saved, E_FAIL);

    /* Check for an existing controller. */
    ComObjPtr<StorageController> ctl;
    rc = getStorageControllerByName(aControllerName, ctl, true /* aSetError */);
    if (FAILED(rc)) return rc;

    StorageControllerType_T ctrlType;
    rc = ctl->COMGETTER(ControllerType)(&ctrlType);
    if (FAILED(rc))
        return setError(E_FAIL,
                        tr("Could not get type of controller '%ls'"),
                        aControllerName);

    /* Check that the controller can do hotplugging if we detach the device while the VM is running. */
    bool fHotplug = false;
    if (Global::IsOnlineOrTransient(mData->mMachineState))
        fHotplug = true;

    if (fHotplug && !isControllerHotplugCapable(ctrlType))
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Controller '%ls' does not support hotplugging"),
                        aControllerName);

    MediumAttachment *pAttach = findAttachment(mMediaData->mAttachments,
                                               aControllerName,
                                               aControllerPort,
                                               aDevice);
    if (!pAttach)
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("No storage device attached to device slot %d on port %d of controller '%ls'"),
                        aDevice, aControllerPort, aControllerName);

    /*
     * The VM has to detach the device before we delete any implicit diffs.
     * If this fails we can roll back without loosing data.
     */
    if (fHotplug)
    {
        alock.release();
        rc = onStorageDeviceChange(pAttach, TRUE /* aRemove */);
        alock.acquire();
    }
    if (FAILED(rc)) return rc;

    /* If we are here everything went well and we can delete the implicit now. */
    rc = detachDevice(pAttach, alock, NULL /* pSnapshot */);

    alock.release();

    mParent->saveModifiedRegistries();

    return rc;
}

STDMETHODIMP Machine::PassthroughDevice(IN_BSTR aControllerName, LONG aControllerPort,
                                        LONG aDevice, BOOL aPassthrough)
{
    CheckComArgStrNotEmptyOrNull(aControllerName);

    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d aPassthrough=%d\n",
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

STDMETHODIMP Machine::TemporaryEjectDevice(IN_BSTR aControllerName, LONG aControllerPort,
                                           LONG aDevice, BOOL aTemporaryEject)
{
    CheckComArgStrNotEmptyOrNull(aControllerName);

    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d aTemporaryEject=%d\n",
                     aControllerName, aControllerPort, aDevice, aTemporaryEject));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

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
                        tr("Setting temporary eject flag rejected as the device attached to device slot %d on port %d of controller '%ls' is not a DVD"),
                        aDevice, aControllerPort, aControllerName);
    pAttach->updateTempEject(!!aTemporaryEject);

    return S_OK;
}

STDMETHODIMP Machine::NonRotationalDevice(IN_BSTR aControllerName, LONG aControllerPort,
                                          LONG aDevice, BOOL aNonRotational)
{
    CheckComArgStrNotEmptyOrNull(aControllerName);

    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d aNonRotational=%d\n",
                     aControllerName, aControllerPort, aDevice, aNonRotational));

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

    if (pAttach->getType() != DeviceType_HardDisk)
        return setError(E_INVALIDARG,
                        tr("Setting the non-rotational medium flag rejected as the device attached to device slot %d on port %d of controller '%ls' is not a hard disk"),
                        aDevice, aControllerPort, aControllerName);
    pAttach->updateNonRotational(!!aNonRotational);

    return S_OK;
}

STDMETHODIMP Machine::SetAutoDiscardForDevice(IN_BSTR aControllerName, LONG aControllerPort,
                                              LONG aDevice, BOOL aDiscard)
{
    CheckComArgStrNotEmptyOrNull(aControllerName);

    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d aDiscard=%d\n",
                     aControllerName, aControllerPort, aDevice, aDiscard));

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

    if (pAttach->getType() != DeviceType_HardDisk)
        return setError(E_INVALIDARG,
                        tr("Setting the discard medium flag rejected as the device attached to device slot %d on port %d of controller '%ls' is not a hard disk"),
                        aDevice, aControllerPort, aControllerName);
    pAttach->updateDiscard(!!aDiscard);

    return S_OK;
}

STDMETHODIMP Machine::SetNoBandwidthGroupForDevice(IN_BSTR aControllerName, LONG aControllerPort,
                                                   LONG aDevice)
{
    int rc = S_OK;
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d\n",
                     aControllerName, aControllerPort, aDevice));

    rc = SetBandwidthGroupForDevice(aControllerName, aControllerPort, aDevice, NULL);

    return rc;
}

STDMETHODIMP Machine::SetBandwidthGroupForDevice(IN_BSTR aControllerName, LONG aControllerPort,
                                                 LONG aDevice, IBandwidthGroup *aBandwidthGroup)
{
    CheckComArgStrNotEmptyOrNull(aControllerName);

    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d\n",
                     aControllerName, aControllerPort, aDevice));

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

    ComObjPtr<BandwidthGroup> group = static_cast<BandwidthGroup*>(aBandwidthGroup);
    if (aBandwidthGroup && group.isNull())
        return setError(E_INVALIDARG, "The given bandwidth group pointer is invalid");

    AutoWriteLock attLock(pAttach COMMA_LOCKVAL_SRC_POS);

    const Utf8Str strBandwidthGroupOld = pAttach->getBandwidthGroup();
    if (strBandwidthGroupOld.isNotEmpty())
    {
        /* Get the bandwidth group object and release it - this must not fail. */
        ComObjPtr<BandwidthGroup> pBandwidthGroupOld;
        rc = getBandwidthGroup(strBandwidthGroupOld, pBandwidthGroupOld, false);
        Assert(SUCCEEDED(rc));

        pBandwidthGroupOld->release();
        pAttach->updateBandwidthGroup(Utf8Str::Empty);
    }

    if (!group.isNull())
    {
        group->reference();
        pAttach->updateBandwidthGroup(group->getName());
    }

    return S_OK;
}

STDMETHODIMP Machine::AttachDeviceWithoutMedium(IN_BSTR aControllerName,
                                                LONG    aControllerPort,
                                                LONG    aDevice,
                                                DeviceType_T aType)
{
     HRESULT rc = S_OK;

     LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d aType=%d aMedium=%p\n",
                      aControllerName, aControllerPort, aDevice, aType));

     rc = AttachDevice(aControllerName, aControllerPort, aDevice, aType, NULL);

     return rc;
}



STDMETHODIMP Machine::UnmountMedium(IN_BSTR aControllerName,
                                    LONG    aControllerPort,
                                    LONG    aDevice,
                                    BOOL    aForce)
{
     int rc = S_OK;
     LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d",
                      aControllerName, aControllerPort, aForce));

     rc = MountMedium(aControllerName, aControllerPort, aDevice, NULL, aForce);

     return rc;
}

STDMETHODIMP Machine::MountMedium(IN_BSTR aControllerName,
                                  LONG aControllerPort,
                                  LONG aDevice,
                                  IMedium *aMedium,
                                  BOOL aForce)
{
    int rc = S_OK;
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d aForce=%d\n",
                     aControllerName, aControllerPort, aDevice, aForce));

    CheckComArgStrNotEmptyOrNull(aControllerName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // request the host lock first, since might be calling Host methods for getting host drives;
    // next, protect the media tree all the while we're in here, as well as our member variables
    AutoMultiWriteLock3 multiLock(mParent->host()->lockHandle(),
                                  this->lockHandle(),
                                  &mParent->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

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

    ComObjPtr<Medium> pMedium = static_cast<Medium*>(aMedium);
    if (aMedium && pMedium.isNull())
        return setError(E_INVALIDARG, "The given medium pointer is invalid");

    AutoCaller mediumCaller(pMedium);
    if (FAILED(mediumCaller.rc())) return mediumCaller.rc();

    AutoWriteLock mediumLock(pMedium COMMA_LOCKVAL_SRC_POS);
    if (pMedium)
    {
        DeviceType_T mediumType = pAttach->getType();
        switch (mediumType)
        {
            case DeviceType_DVD:
            case DeviceType_Floppy:
            break;

            default:
                return setError(VBOX_E_INVALID_OBJECT_STATE,
                                tr("The device at port %d, device %d of controller '%ls' of this virtual machine is not removeable"),
                                aControllerPort,
                                aDevice,
                                aControllerName);
        }
    }

    setModified(IsModified_Storage);
    mMediaData.backup();

    {
        // The backup operation makes the pAttach reference point to the
        // old settings. Re-get the correct reference.
        pAttach = findAttachment(mMediaData->mAttachments,
                                 aControllerName,
                                 aControllerPort,
                                 aDevice);
        if (!oldmedium.isNull())
            oldmedium->removeBackReference(mData->mUuid);
        if (!pMedium.isNull())
        {
            pMedium->addBackReference(mData->mUuid);

            mediumLock.release();
            multiLock.release();
            addMediumToRegistry(pMedium);
            multiLock.acquire();
            mediumLock.acquire();
        }

        AutoWriteLock attLock(pAttach COMMA_LOCKVAL_SRC_POS);
        pAttach->updateMedium(pMedium);
    }

    setModified(IsModified_Storage);

    mediumLock.release();
    multiLock.release();
    rc = onMediumChange(pAttach, aForce);
    multiLock.acquire();
    mediumLock.acquire();

    /* On error roll back this change only. */
    if (FAILED(rc))
    {
        if (!pMedium.isNull())
            pMedium->removeBackReference(mData->mUuid);
        pAttach = findAttachment(mMediaData->mAttachments,
                                 aControllerName,
                                 aControllerPort,
                                 aDevice);
        /* If the attachment is gone in the meantime, bail out. */
        if (pAttach.isNull())
            return rc;
        AutoWriteLock attLock(pAttach COMMA_LOCKVAL_SRC_POS);
        if (!oldmedium.isNull())
            oldmedium->addBackReference(mData->mUuid);
        pAttach->updateMedium(oldmedium);
    }

    mediumLock.release();
    multiLock.release();

    mParent->saveModifiedRegistries();

    return rc;
}

STDMETHODIMP Machine::GetMedium(IN_BSTR aControllerName,
                                LONG aControllerPort,
                                LONG aDevice,
                                IMedium **aMedium)
{
    LogFlowThisFunc(("aControllerName=\"%ls\" aControllerPort=%d aDevice=%d\n",
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
    CheckComArgExpr(slot, slot < mNetworkAdapters.size());

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mNetworkAdapters[slot].queryInterfaceTo(adapter);

    return S_OK;
}

STDMETHODIMP Machine::GetExtraDataKeys(ComSafeArrayOut(BSTR, aKeys))
{
    CheckComArgOutSafeArrayPointerValid(aKeys);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<BSTR> saKeys(mData->pMachineConfigFile->mapExtraDataItems.size());
    int i = 0;
    for (settings::StringsMap::const_iterator it = mData->pMachineConfigFile->mapExtraDataItems.begin();
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

    settings::StringsMap::const_iterator it = mData->pMachineConfigFile->mapExtraDataItems.find(Utf8Str(aKey));
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

    // look up the old value first; if nothing has changed then we need not do anything
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS); // hold read lock only while looking up
        settings::StringsMap::const_iterator it = mData->pMachineConfigFile->mapExtraDataItems.find(strKey);
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

        if (!mParent->onExtraDataCanChange(mData->mUuid, aKey, bstrValue.raw(), error))
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

        if (isSnapshotMachine())
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
            // save the global settings; for that we should hold only the VirtualBox lock
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
    HRESULT rc = checkStateDependency(MutableOrSavedStateDep);
    if (FAILED(rc)) return rc;

    /* the settings file path may never be null */
    ComAssertRet(!mData->m_strConfigFileFull.isEmpty(), E_FAIL);

    /* save all VM data excluding snapshots */
    bool fNeedsGlobalSaveSettings = false;
    rc = saveSettings(&fNeedsGlobalSaveSettings);
    mlock.release();

    if (SUCCEEDED(rc) && fNeedsGlobalSaveSettings)
    {
        // save the global settings; for that we should hold only the VirtualBox lock
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

/** @note Locks objects! */
STDMETHODIMP Machine::Unregister(CleanupMode_T cleanupMode,
                                 ComSafeArrayOut(IMedium*, aMedia))
{
    // use AutoLimitedCaller because this call is valid on inaccessible machines as well
    AutoLimitedCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    Guid id(getId());

    if (mData->mSession.mState != SessionState_Unlocked)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("Cannot unregister the machine '%s' while it is locked"),
                        mUserData->s.strName.c_str());

    // wait for state dependents to drop to zero
    ensureNoStateDependencies();

    if (!mData->mAccessible)
    {
        // inaccessible maschines can only be unregistered; uninitialize ourselves
        // here because currently there may be no unregistered that are inaccessible
        // (this state combination is not supported). Note releasing the caller and
        // leaving the lock before calling uninit()
        alock.release();
        autoCaller.release();

        uninit();

        mParent->unregisterMachine(this, id);
            // calls VirtualBox::saveSettings()

        return S_OK;
    }

    HRESULT rc = S_OK;

    // discard saved state
    if (mData->mMachineState == MachineState_Saved)
    {
        // add the saved state file to the list of files the caller should delete
        Assert(!mSSData->strStateFilePath.isEmpty());
        mData->llFilesToDelete.push_back(mSSData->strStateFilePath);

        mSSData->strStateFilePath.setNull();

        // unconditionally set the machine state to powered off, we now
        // know no session has locked the machine
        mData->mMachineState = MachineState_PoweredOff;
    }

    size_t cSnapshots = 0;
    if (mData->mFirstSnapshot)
        cSnapshots = mData->mFirstSnapshot->getAllChildrenCount() + 1;
    if (cSnapshots && cleanupMode == CleanupMode_UnregisterOnly)
        // fail now before we start detaching media
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("Cannot unregister the machine '%s' because it has %d snapshots"),
                           mUserData->s.strName.c_str(), cSnapshots);

    // This list collects the medium objects from all medium attachments
    // which we will detach from the machine and its snapshots, in a specific
    // order which allows for closing all media without getting "media in use"
    // errors, simply by going through the list from the front to the back:
    // 1) first media from machine attachments (these have the "leaf" attachments with snapshots
    //    and must be closed before the parent media from the snapshots, or closing the parents
    //    will fail because they still have children);
    // 2) media from the youngest snapshots followed by those from the parent snapshots until
    //    the root ("first") snapshot of the machine.
    MediaList llMedia;

    if (    !mMediaData.isNull()      // can be NULL if machine is inaccessible
         && mMediaData->mAttachments.size()
       )
    {
        // we have media attachments: detach them all and add the Medium objects to our list
        if (cleanupMode != CleanupMode_UnregisterOnly)
            detachAllMedia(alock, NULL /* pSnapshot */, cleanupMode, llMedia);
        else
            return setError(VBOX_E_INVALID_OBJECT_STATE,
                            tr("Cannot unregister the machine '%s' because it has %d media attachments"),
                            mUserData->s.strName.c_str(), mMediaData->mAttachments.size());
    }

    if (cSnapshots)
    {
        // autoCleanup must be true here, or we would have failed above

        // add the media from the medium attachments of the snapshots to llMedia
        // as well, after the "main" machine media; Snapshot::uninitRecursively()
        // calls Machine::detachAllMedia() for the snapshot machine, recursing
        // into the children first

        // Snapshot::beginDeletingSnapshot() asserts if the machine state is not this
        MachineState_T oldState = mData->mMachineState;
        mData->mMachineState = MachineState_DeletingSnapshot;

        // make a copy of the first snapshot so the refcount does not drop to 0
        // in beginDeletingSnapshot, which sets pFirstSnapshot to 0 (that hangs
        // because of the AutoCaller voodoo)
        ComObjPtr<Snapshot> pFirstSnapshot = mData->mFirstSnapshot;

        // GO!
        pFirstSnapshot->uninitRecursively(alock, cleanupMode, llMedia, mData->llFilesToDelete);

        mData->mMachineState = oldState;
    }

    if (FAILED(rc))
    {
        rollbackMedia();
        return rc;
    }

    // commit all the media changes made above
    commitMedia();

    mData->mRegistered = false;

    // machine lock no longer needed
    alock.release();

    // return media to caller
    SafeIfaceArray<IMedium> sfaMedia(llMedia);
    sfaMedia.detachTo(ComSafeArrayOutArg(aMedia));

    mParent->unregisterMachine(this, id);
            // calls VirtualBox::saveSettings() and VirtualBox::saveModifiedRegistries()

    return S_OK;
}

struct Machine::DeleteTask
{
    ComObjPtr<Machine>          pMachine;
    RTCList<ComPtr<IMedium> >   llMediums;
    StringsList                 llFilesToDelete;
    ComObjPtr<Progress>         pProgress;
};

STDMETHODIMP Machine::Delete(ComSafeArrayIn(IMedium*, aMedia), IProgress **aProgress)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    if (mData->mRegistered)
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Cannot delete settings of a registered machine"));

    DeleteTask *pTask = new DeleteTask;
    pTask->pMachine = this;
    com::SafeIfaceArray<IMedium> sfaMedia(ComSafeArrayInArg(aMedia));

    // collect files to delete
    pTask->llFilesToDelete = mData->llFilesToDelete;            // saved states pushed here by Unregister()

    for (size_t i = 0; i < sfaMedia.size(); ++i)
    {
        IMedium *pIMedium(sfaMedia[i]);
        ComObjPtr<Medium> pMedium = static_cast<Medium*>(pIMedium);
        if (pMedium.isNull())
            return setError(E_INVALIDARG, "The given medium pointer with index %d is invalid", i);
        SafeArray<BSTR> ids;
        rc = pMedium->COMGETTER(MachineIds)(ComSafeArrayAsOutParam(ids));
        if (FAILED(rc)) return rc;
        /* At this point the medium should not have any back references
         * anymore. If it has it is attached to another VM and *must* not
         * deleted. */
        if (ids.size() < 1)
            pTask->llMediums.append(pMedium);
    }
    if (mData->pMachineConfigFile->fileExists())
        pTask->llFilesToDelete.push_back(mData->m_strConfigFileFull);

    pTask->pProgress.createObject();
    pTask->pProgress->init(getVirtualBox(),
                           static_cast<IMachine*>(this) /* aInitiator */,
                           Bstr(tr("Deleting files")).raw(),
                           true /* fCancellable */,
                           pTask->llFilesToDelete.size() + pTask->llMediums.size() + 1,   // cOperations
                           BstrFmt(tr("Deleting '%s'"), pTask->llFilesToDelete.front().c_str()).raw());

    int vrc = RTThreadCreate(NULL,
                             Machine::deleteThread,
                             (void*)pTask,
                             0,
                             RTTHREADTYPE_MAIN_WORKER,
                             0,
                             "MachineDelete");

    pTask->pProgress.queryInterfaceTo(aProgress);

    if (RT_FAILURE(vrc))
    {
        delete pTask;
        return setError(E_FAIL, "Could not create MachineDelete thread (%Rrc)", vrc);
    }

    LogFlowFuncLeave();

    return S_OK;
}

/**
 * Static task wrapper passed to RTThreadCreate() in Machine::Delete() which then
 * calls Machine::deleteTaskWorker() on the actual machine object.
 * @param Thread
 * @param pvUser
 * @return
 */
/*static*/
DECLCALLBACK(int) Machine::deleteThread(RTTHREAD Thread, void *pvUser)
{
    LogFlowFuncEnter();

    DeleteTask *pTask = (DeleteTask*)pvUser;
    Assert(pTask);
    Assert(pTask->pMachine);
    Assert(pTask->pProgress);

    HRESULT rc = pTask->pMachine->deleteTaskWorker(*pTask);
    pTask->pProgress->notifyComplete(rc);

    delete pTask;

    LogFlowFuncLeave();

    NOREF(Thread);

    return VINF_SUCCESS;
}

/**
 * Task thread implementation for Machine::Delete(), called from Machine::deleteThread().
 * @param task
 * @return
 */
HRESULT Machine::deleteTaskWorker(DeleteTask &task)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    try
    {
        ULONG uLogHistoryCount = 3;
        ComPtr<ISystemProperties> systemProperties;
        rc = mParent->COMGETTER(SystemProperties)(systemProperties.asOutParam());
        if (FAILED(rc)) throw rc;

        if (!systemProperties.isNull())
        {
            rc = systemProperties->COMGETTER(LogHistoryCount)(&uLogHistoryCount);
            if (FAILED(rc)) throw rc;
        }

        MachineState_T oldState = mData->mMachineState;
        setMachineState(MachineState_SettingUp);
        alock.release();
        for (size_t i = 0; i < task.llMediums.size(); ++i)
        {
            ComObjPtr<Medium> pMedium = (Medium*)(IMedium*)task.llMediums.at(i);
            {
                AutoCaller mac(pMedium);
                if (FAILED(mac.rc())) throw mac.rc();
                Utf8Str strLocation = pMedium->getLocationFull();
                rc = task.pProgress->SetNextOperation(BstrFmt(tr("Deleting '%s'"), strLocation.c_str()).raw(), 1);
                if (FAILED(rc)) throw rc;
                LogFunc(("Deleting file %s\n", strLocation.c_str()));
            }
            ComPtr<IProgress> pProgress2;
            rc = pMedium->DeleteStorage(pProgress2.asOutParam());
            if (FAILED(rc)) throw rc;
            rc = task.pProgress->WaitForAsyncProgressCompletion(pProgress2);
            if (FAILED(rc)) throw rc;
            /* Check the result of the asynchrony process. */
            LONG iRc;
            rc = pProgress2->COMGETTER(ResultCode)(&iRc);
            if (FAILED(rc)) throw rc;
            /* If the thread of the progress object has an error, then
             * retrieve the error info from there, or it'll be lost. */
            if (FAILED(iRc))
                throw setError(ProgressErrorInfo(pProgress2));
        }
        setMachineState(oldState);
        alock.acquire();

        // delete the files pushed on the task list by Machine::Delete()
        // (this includes saved states of the machine and snapshots and
        // medium storage files from the IMedium list passed in, and the
        // machine XML file)
        StringsList::const_iterator it = task.llFilesToDelete.begin();
        while (it != task.llFilesToDelete.end())
        {
            const Utf8Str &strFile = *it;
            LogFunc(("Deleting file %s\n", strFile.c_str()));
            int vrc = RTFileDelete(strFile.c_str());
            if (RT_FAILURE(vrc))
                throw setError(VBOX_E_IPRT_ERROR,
                               tr("Could not delete file '%s' (%Rrc)"), strFile.c_str(), vrc);

            ++it;
            if (it == task.llFilesToDelete.end())
            {
                rc = task.pProgress->SetNextOperation(Bstr(tr("Cleaning up machine directory")).raw(), 1);
                if (FAILED(rc)) throw rc;
                break;
            }

            rc = task.pProgress->SetNextOperation(BstrFmt(tr("Deleting '%s'"), it->c_str()).raw(), 1);
            if (FAILED(rc)) throw rc;
        }

        /* delete the settings only when the file actually exists */
        if (mData->pMachineConfigFile->fileExists())
        {
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
                                         logFolder.c_str(), RTPATH_DELIMITER);
                RTFileDelete(log.c_str());
                log = Utf8StrFmt("%s%cVBox.png",
                                 logFolder.c_str(), RTPATH_DELIMITER);
                RTFileDelete(log.c_str());
                for (int i = uLogHistoryCount; i > 0; i--)
                {
                    log = Utf8StrFmt("%s%cVBox.log.%d",
                                     logFolder.c_str(), RTPATH_DELIMITER, i);
                    RTFileDelete(log.c_str());
                    log = Utf8StrFmt("%s%cVBox.png.%d",
                                     logFolder.c_str(), RTPATH_DELIMITER, i);
                    RTFileDelete(log.c_str());
                }

                RTDirRemove(logFolder.c_str());
            }

            /* delete the Snapshots folder, nothing important should be left
             * there (we don't check for errors because the user might have
             * some private files there that we don't want to delete) */
            Utf8Str strFullSnapshotFolder;
            calculateFullPath(mUserData->s.strSnapshotFolder, strFullSnapshotFolder);
            Assert(!strFullSnapshotFolder.isEmpty());
            if (RTDirExists(strFullSnapshotFolder.c_str()))
                RTDirRemove(strFullSnapshotFolder.c_str());

            // delete the directory that contains the settings file, but only
            // if it matches the VM name
            Utf8Str settingsDir;
            if (isInOwnDir(&settingsDir))
                RTDirRemove(settingsDir.c_str());
        }

        alock.release();

        mParent->saveModifiedRegistries();
    }
    catch (HRESULT aRC) { rc = aRC; }

    return rc;
}

STDMETHODIMP Machine::FindSnapshot(IN_BSTR aNameOrId, ISnapshot **aSnapshot)
{
    CheckComArgOutPointerValid(aSnapshot);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<Snapshot> pSnapshot;
    HRESULT rc;

    if (!aNameOrId || !*aNameOrId)
        // null case (caller wants root snapshot): findSnapshotById() handles this
        rc = findSnapshotById(Guid(), pSnapshot, true /* aSetError */);
    else
    {
        Guid uuid(aNameOrId);
        if (!uuid.isEmpty())
            rc = findSnapshotById(uuid, pSnapshot, true /* aSetError */);
        else
            rc = findSnapshotByName(Utf8Str(aNameOrId), pSnapshot, true /* aSetError */);
    }
    pSnapshot.queryInterfaceTo(aSnapshot);

    return rc;
}

STDMETHODIMP Machine::CreateSharedFolder(IN_BSTR aName, IN_BSTR aHostPath, BOOL aWritable, BOOL aAutoMount)
{
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgStrNotEmptyOrNull(aHostPath);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    Utf8Str strName(aName);

    ComObjPtr<SharedFolder> sharedFolder;
    rc = findSharedFolder(strName, sharedFolder, false /* aSetError */);
    if (SUCCEEDED(rc))
        return setError(VBOX_E_OBJECT_IN_USE,
                        tr("Shared folder named '%s' already exists"),
                        strName.c_str());

    sharedFolder.createObject();
    rc = sharedFolder->init(getMachine(),
                            strName,
                            aHostPath,
                            !!aWritable,
                            !!aAutoMount,
                            true /* fFailOnError */);
    if (FAILED(rc)) return rc;

    setModified(IsModified_SharedFolders);
    mHWData.backup();
    mHWData->mSharedFolders.push_back(sharedFolder);

    /* inform the direct session if any */
    alock.release();
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
    alock.release();
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

        if (mData->mSession.mState != SessionState_Locked)
            return setError(VBOX_E_INVALID_VM_STATE,
                            tr("Machine is not locked for session (session state: %s)"),
                            Global::stringifySessionState(mData->mSession.mState));

        directControl = mData->mSession.mDirectControl;
    }

    /* ignore calls made after #OnSessionEnd() is called */
    if (!directControl)
        return S_OK;

    LONG64 dummy;
    return directControl->OnShowWindow(TRUE /* aCheck */, aCanShow, &dummy);
}

STDMETHODIMP Machine::ShowConsoleWindow(LONG64 *aWinId)
{
    CheckComArgOutPointerValid(aWinId);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    ComPtr<IInternalSessionControl> directControl;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (mData->mSession.mState != SessionState_Locked)
            return setError(E_FAIL,
                            tr("Machine is not locked for session (session state: %s)"),
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
                                             LONG64 *aTimestamp,
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
                                        LONG64 *aTimestamp,
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
                                       LONG64 *aTimestamp,
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
    LONG64 dummyTimestamp;
    Bstr dummyFlags;
    return GetGuestProperty(aName, aValue, &dummyTimestamp, dummyFlags.asOutParam());
}

STDMETHODIMP Machine::GetGuestPropertyTimestamp(IN_BSTR aName, LONG64 *aTimestamp)
{
    Bstr dummyValue;
    Bstr dummyFlags;
    return GetGuestProperty(aName, dummyValue.asOutParam(), aTimestamp, dummyFlags.asOutParam());
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
             && RT_FAILURE(validateFlags(utf8Flags.c_str(), &fFlags))
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
            if (aValue)
            {
                RTTIMESPEC time;
                property.strValue = aValue;
                property.mTimestamp = RTTimeSpecGetNano(RTTimeNow(&time));
                if (aFlags != NULL)
                    property.mFlags = fFlags;
                mHWData->mGuestProperties.push_back(property);
            }
        }
        else if (SUCCEEDED(rc) && aValue)
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
                || RTStrSimplePatternMultiMatch(mHWData->mGuestPropertyNotificationPatterns.c_str(),
                                                RTSTR_MAX,
                                                utf8Name.c_str(),
                                                RTSTR_MAX,
                                                NULL)
               )
           )
        {
            /** @todo r=bird: Why aren't we leaving the lock here?  The
             *                same code in PushGuestProperty does... */
            mParent->onGuestPropertyChange(mData->mUuid, aName,
                                           aValue ? aValue : Bstr("").raw(),
                                           aFlags ? aFlags : Bstr("").raw());
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

    try
    {
        ComPtr<IInternalSessionControl> directControl = mData->mSession.mDirectControl;

        BSTR dummy = NULL; /* will not be changed (setter) */
        LONG64 dummy64;
        if (!directControl)
            rc = E_ACCESSDENIED;
        else
            /** @todo Fix when adding DeleteGuestProperty(),
                         see defect. */
            rc = directControl->AccessGuestProperty(aName, aValue, aFlags,
                                                    true /* isSetter */,
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
    CheckComArgMaybeNull(aFlags);
    CheckComArgMaybeNull(aValue);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc()))
        return autoCaller.rc();

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

STDMETHODIMP Machine::DeleteGuestProperty(IN_BSTR aName)
{
    return SetGuestProperty(aName, NULL, NULL);
}

#ifdef VBOX_WITH_GUEST_PROPS
/**
 * Enumerate the guest properties in VBoxSVC's internal structures.
 */
HRESULT Machine::enumerateGuestPropertiesInService
                (IN_BSTR aPatterns, ComSafeArrayOut(BSTR, aNames),
                 ComSafeArrayOut(BSTR, aValues),
                 ComSafeArrayOut(LONG64, aTimestamps),
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
            || RTStrSimplePatternMultiMatch(strPatterns.c_str(),
                                            RTSTR_MAX,
                                            it->strName.c_str(),
                                            RTSTR_MAX,
                                            NULL)
           )
            propList.push_back(*it);

    /*
     * And build up the arrays for returning the property information.
     */
    size_t cEntries = propList.size();
    SafeArray<BSTR> names(cEntries);
    SafeArray<BSTR> values(cEntries);
    SafeArray<LONG64> timestamps(cEntries);
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
                 ComSafeArrayOut(LONG64, aTimestamps),
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
                                               ComSafeArrayOut(LONG64, aTimestamps),
                                               ComSafeArrayOut(BSTR, aFlags))
{
#ifndef VBOX_WITH_GUEST_PROPS
    ReturnComNotImplemented();
#else // VBOX_WITH_GUEST_PROPS
    CheckComArgMaybeNull(aPatterns);
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
    bool fBootable = true;
    for (StorageControllerList::const_iterator it = mStorageControllers->begin();
         it != mStorageControllers->end();
         ++it)
    {
        if ((*it)->getStorageBus() == aConnectionType)
        {
            ULONG ulCurInst = (*it)->getInstance();

            if (ulCurInst >= ulInstance)
                ulInstance = ulCurInst + 1;

            /* Only one controller of each type can be marked as bootable. */
            if ((*it)->getBootable())
                fBootable = false;
        }
    }

    rc = ctrl->init(this, aName, aConnectionType, ulInstance, fBootable);
    if (FAILED(rc)) return rc;

    setModified(IsModified_Storage);
    mStorageControllers.backup();
    mStorageControllers->push_back(ctrl);

    ctrl.queryInterfaceTo(controller);

    /* inform the direct session if any */
    alock.release();
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

STDMETHODIMP Machine::SetStorageControllerBootable(IN_BSTR aName, BOOL fBootable)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = checkStateDependency(MutableStateDep);
    if (FAILED(rc)) return rc;

    ComObjPtr<StorageController> ctrl;

    rc = getStorageControllerByName(aName, ctrl, true /* aSetError */);
    if (SUCCEEDED(rc))
    {
        /* Ensure that only one controller of each type is marked as bootable. */
        if (fBootable == TRUE)
        {
            for (StorageControllerList::const_iterator it = mStorageControllers->begin();
                 it != mStorageControllers->end();
                 ++it)
            {
                ComObjPtr<StorageController> aCtrl = (*it);

                if (   (aCtrl->getName() != Utf8Str(aName))
                    && aCtrl->getBootable() == TRUE
                    && aCtrl->getStorageBus() == ctrl->getStorageBus()
                    && aCtrl->getControllerType() == ctrl->getControllerType())
                {
                    aCtrl->setBootable(FALSE);
                    break;
                }
            }
        }

        if (SUCCEEDED(rc))
        {
            ctrl->setBootable(fBootable);
            setModified(IsModified_Storage);
        }
    }

    if (SUCCEEDED(rc))
    {
        /* inform the direct session if any */
        alock.release();
        onStorageControllerChange();
    }

    return rc;
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

    {
        /* find all attached devices to the appropriate storage controller and detach them all */
        // make a temporary list because detachDevice invalidates iterators into
        // mMediaData->mAttachments
        MediaData::AttachmentList llAttachments2 = mMediaData->mAttachments;

        for (MediaData::AttachmentList::iterator it = llAttachments2.begin();
             it != llAttachments2.end();
             ++it)
        {
            MediumAttachment *pAttachTemp = *it;

            AutoCaller localAutoCaller(pAttachTemp);
            if (FAILED(localAutoCaller.rc())) return localAutoCaller.rc();

            AutoReadLock local_alock(pAttachTemp COMMA_LOCKVAL_SRC_POS);

            if (pAttachTemp->getControllerName() == aName)
            {
                rc = detachDevice(pAttachTemp, alock, NULL);
                if (FAILED(rc)) return rc;
            }
        }
    }

    /* We can remove it now. */
    setModified(IsModified_Storage);
    mStorageControllers.backup();

    ctrl->unshare();

    mStorageControllers->remove(ctrl);

    /* inform the direct session if any */
    alock.release();
    onStorageControllerChange();

    return S_OK;
}

STDMETHODIMP Machine::QuerySavedGuestScreenInfo(ULONG uScreenId,
                                                ULONG *puOriginX,
                                                ULONG *puOriginY,
                                                ULONG *puWidth,
                                                ULONG *puHeight,
                                                BOOL *pfEnabled)
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(puOriginX);
    CheckComArgNotNull(puOriginY);
    CheckComArgNotNull(puWidth);
    CheckComArgNotNull(puHeight);
    CheckComArgNotNull(pfEnabled);

    uint32_t u32OriginX= 0;
    uint32_t u32OriginY= 0;
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;
    uint16_t u16Flags = 0;

    int vrc = readSavedGuestScreenInfo(mSSData->strStateFilePath, uScreenId,
                                       &u32OriginX, &u32OriginY, &u32Width, &u32Height, &u16Flags);
    if (RT_FAILURE(vrc))
    {
#ifdef RT_OS_WINDOWS
        /* HACK: GUI sets *pfEnabled to 'true' and expects it to stay so if the API fails.
         * This works with XPCOM. But Windows COM sets all output parameters to zero.
         * So just assign fEnable to TRUE again.
         * The right fix would be to change GUI API wrappers to make sure that parameters
         * are changed only if API succeeds.
         */
        *pfEnabled = TRUE;
#endif
        return setError(VBOX_E_IPRT_ERROR,
                        tr("Saved guest size is not available (%Rrc)"),
                        vrc);
    }

    *puOriginX = u32OriginX;
    *puOriginY = u32OriginY;
    *puWidth = u32Width;
    *puHeight = u32Height;
    *pfEnabled = (u16Flags & VBVA_SCREEN_F_DISABLED) == 0;

    return S_OK;
}

STDMETHODIMP Machine::QuerySavedThumbnailSize(ULONG aScreenId, ULONG *aSize, ULONG *aWidth, ULONG *aHeight)
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(aSize);
    CheckComArgNotNull(aWidth);
    CheckComArgNotNull(aHeight);

    if (aScreenId != 0)
        return E_NOTIMPL;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint8_t *pu8Data = NULL;
    uint32_t cbData = 0;
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;

    int vrc = readSavedDisplayScreenshot(mSSData->strStateFilePath, 0 /* u32Type */, &pu8Data, &cbData, &u32Width, &u32Height);

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

STDMETHODIMP Machine::ReadSavedThumbnailToArray(ULONG aScreenId, BOOL aBGR, ULONG *aWidth, ULONG *aHeight, ComSafeArrayOut(BYTE, aData))
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(aWidth);
    CheckComArgNotNull(aHeight);
    CheckComArgOutSafeArrayPointerValid(aData);

    if (aScreenId != 0)
        return E_NOTIMPL;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint8_t *pu8Data = NULL;
    uint32_t cbData = 0;
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;

    int vrc = readSavedDisplayScreenshot(mSSData->strStateFilePath, 0 /* u32Type */, &pu8Data, &cbData, &u32Width, &u32Height);

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


STDMETHODIMP Machine::ReadSavedThumbnailPNGToArray(ULONG aScreenId, ULONG *aWidth, ULONG *aHeight, ComSafeArrayOut(BYTE, aData))
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(aWidth);
    CheckComArgNotNull(aHeight);
    CheckComArgOutSafeArrayPointerValid(aData);

    if (aScreenId != 0)
        return E_NOTIMPL;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint8_t *pu8Data = NULL;
    uint32_t cbData = 0;
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;

    int vrc = readSavedDisplayScreenshot(mSSData->strStateFilePath, 0 /* u32Type */, &pu8Data, &cbData, &u32Width, &u32Height);

    if (RT_FAILURE(vrc))
        return setError(VBOX_E_IPRT_ERROR,
                        tr("Saved screenshot data is not available (%Rrc)"),
                        vrc);

    *aWidth = u32Width;
    *aHeight = u32Height;

    HRESULT rc = S_OK;
    uint8_t *pu8PNG = NULL;
    uint32_t cbPNG = 0;
    uint32_t cxPNG = 0;
    uint32_t cyPNG = 0;

    vrc = DisplayMakePNG(pu8Data, u32Width, u32Height, &pu8PNG, &cbPNG, &cxPNG, &cyPNG, 0);

    if (RT_SUCCESS(vrc))
    {
        com::SafeArray<BYTE> screenData(cbPNG);
        screenData.initFrom(pu8PNG, cbPNG);
        if (pu8PNG)
            RTMemFree(pu8PNG);
        screenData.detachTo(ComSafeArrayOutArg(aData));
    }
    else
    {
        if (pu8PNG)
            RTMemFree(pu8PNG);
        return setError(VBOX_E_IPRT_ERROR,
                        tr("Could not convert screenshot to PNG (%Rrc)"),
                        vrc);
    }

    freeSavedDisplayScreenshot(pu8Data);

    return rc;
}

STDMETHODIMP Machine::QuerySavedScreenshotPNGSize(ULONG aScreenId, ULONG *aSize, ULONG *aWidth, ULONG *aHeight)
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(aSize);
    CheckComArgNotNull(aWidth);
    CheckComArgNotNull(aHeight);

    if (aScreenId != 0)
        return E_NOTIMPL;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint8_t *pu8Data = NULL;
    uint32_t cbData = 0;
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;

    int vrc = readSavedDisplayScreenshot(mSSData->strStateFilePath, 1 /* u32Type */, &pu8Data, &cbData, &u32Width, &u32Height);

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

STDMETHODIMP Machine::ReadSavedScreenshotPNGToArray(ULONG aScreenId, ULONG *aWidth, ULONG *aHeight, ComSafeArrayOut(BYTE, aData))
{
    LogFlowThisFunc(("\n"));

    CheckComArgNotNull(aWidth);
    CheckComArgNotNull(aHeight);
    CheckComArgOutSafeArrayPointerValid(aData);

    if (aScreenId != 0)
        return E_NOTIMPL;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint8_t *pu8Data = NULL;
    uint32_t cbData = 0;
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;

    int vrc = readSavedDisplayScreenshot(mSSData->strStateFilePath, 1 /* u32Type */, &pu8Data, &cbData, &u32Width, &u32Height);

    if (RT_FAILURE(vrc))
        return setError(VBOX_E_IPRT_ERROR,
                        tr("Saved screenshot thumbnail data is not available (%Rrc)"),
                        vrc);

    *aWidth = u32Width;
    *aHeight = u32Height;

    com::SafeArray<BYTE> png(cbData);
    png.initFrom(pu8Data, cbData);
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

    alock.release();
    rc = onCPUChange(aCpu, false);
    alock.acquire();
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mCPUAttached[aCpu] = true;

    /* Save settings if online */
    if (Global::IsOnline(mData->mMachineState))
        saveSettings(NULL);

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

    alock.release();
    rc = onCPUChange(aCpu, true);
    alock.acquire();
    if (FAILED(rc)) return rc;

    setModified(IsModified_MachineData);
    mHWData.backup();
    mHWData->mCPUAttached[aCpu] = false;

    /* Save settings if online */
    if (Global::IsOnline(mData->mMachineState))
        saveSettings(NULL);

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
    if (!RTFileExists(log.c_str()))
        log.setNull();
    log.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP Machine::ReadLog(ULONG aIdx, LONG64 aOffset, LONG64 aSize, ComSafeArrayOut(BYTE, aData))
{
    LogFlowThisFunc(("\n"));
    CheckComArgOutSafeArrayPointerValid(aData);
    if (aSize < 0)
        return setError(E_INVALIDARG, tr("The size argument (%lld) is negative"), aSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    Utf8Str log = queryLogFilename(aIdx);

    /* do not unnecessarily hold the lock while doing something which does
     * not need the lock and potentially takes a long time. */
    alock.release();

    /* Limit the chunk size to 32K for now, as that gives better performance
     * over (XP)COM, and keeps the SOAP reply size under 1M for the webservice.
     * One byte expands to approx. 25 bytes of breathtaking XML. */
    size_t cbData = (size_t)RT_MIN(aSize, 32768);
    com::SafeArray<BYTE> logData(cbData);

    RTFILE LogFile;
    int vrc = RTFileOpen(&LogFile, log.c_str(),
                         RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(vrc))
    {
        vrc = RTFileReadAt(LogFile, aOffset, logData.raw(), cbData, &cbData);
        if (RT_SUCCESS(vrc))
            logData.resize(cbData);
        else
            rc = setError(VBOX_E_IPRT_ERROR,
                          tr("Could not read log file '%s' (%Rrc)"),
                          log.c_str(), vrc);
        RTFileClose(LogFile);
    }
    else
        rc = setError(VBOX_E_IPRT_ERROR,
                      tr("Could not open log file '%s' (%Rrc)"),
                      log.c_str(), vrc);

    if (FAILED(rc))
        logData.resize(0);
    logData.detachTo(ComSafeArrayOutArg(aData));

    return rc;
}


/**
 * Currently this method doesn't attach device to the running VM,
 * just makes sure it's plugged on next VM start.
 */
STDMETHODIMP Machine::AttachHostPCIDevice(LONG hostAddress, LONG desiredGuestAddress, BOOL /*tryToUnbind*/)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // lock scope
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        HRESULT rc = checkStateDependency(MutableStateDep);
        if (FAILED(rc)) return rc;

        ChipsetType_T aChipset = ChipsetType_PIIX3;
        COMGETTER(ChipsetType)(&aChipset);

        if (aChipset != ChipsetType_ICH9)
        {
            return setError(E_INVALIDARG,
                            tr("Host PCI attachment only supported with ICH9 chipset"));
        }

        // check if device with this host PCI address already attached
        for (HWData::PCIDeviceAssignmentList::iterator it =  mHWData->mPCIDeviceAssignments.begin();
             it !=  mHWData->mPCIDeviceAssignments.end();
             ++it)
        {
            LONG iHostAddress = -1;
            ComPtr<PCIDeviceAttachment> pAttach;
            pAttach = *it;
            pAttach->COMGETTER(HostAddress)(&iHostAddress);
            if (iHostAddress == hostAddress)
                return setError(E_INVALIDARG,
                                tr("Device with host PCI address already attached to this VM"));
        }

        ComObjPtr<PCIDeviceAttachment> pda;
        char name[32];

        RTStrPrintf(name, sizeof(name), "host%02x:%02x.%x", (hostAddress>>8) & 0xff, (hostAddress & 0xf8) >> 3, hostAddress & 7);
        Bstr bname(name);
        pda.createObject();
        pda->init(this, bname,  hostAddress, desiredGuestAddress, TRUE);
        setModified(IsModified_MachineData);
        mHWData.backup();
        mHWData->mPCIDeviceAssignments.push_back(pda);
    }

    return S_OK;
}

/**
 * Currently this method doesn't detach device from the running VM,
 * just makes sure it's not plugged on next VM start.
 */
STDMETHODIMP Machine::DetachHostPCIDevice(LONG hostAddress)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr<PCIDeviceAttachment> pAttach;
    bool fRemoved = false;
    HRESULT rc;

    // lock scope
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        rc = checkStateDependency(MutableStateDep);
        if (FAILED(rc)) return rc;

        for (HWData::PCIDeviceAssignmentList::iterator it =  mHWData->mPCIDeviceAssignments.begin();
             it !=  mHWData->mPCIDeviceAssignments.end();
             ++it)
        {
            LONG iHostAddress = -1;
            pAttach = *it;
            pAttach->COMGETTER(HostAddress)(&iHostAddress);
            if (iHostAddress  != -1  && iHostAddress == hostAddress)
            {
                setModified(IsModified_MachineData);
                mHWData.backup();
                mHWData->mPCIDeviceAssignments.remove(pAttach);
                fRemoved = true;
                break;
            }
        }
    }


    /* Fire event outside of the lock */
    if (fRemoved)
    {
        Assert(!pAttach.isNull());
        ComPtr<IEventSource> es;
        rc = mParent->COMGETTER(EventSource)(es.asOutParam());
        Assert(SUCCEEDED(rc));
        Bstr mid;
        rc = this->COMGETTER(Id)(mid.asOutParam());
        Assert(SUCCEEDED(rc));
        fireHostPCIDevicePlugEvent(es, mid.raw(), false /* unplugged */, true /* success */, pAttach, NULL);
    }

    return fRemoved ? S_OK : setError(VBOX_E_OBJECT_NOT_FOUND,
                                      tr("No host PCI device %08x attached"),
                                      hostAddress
                                      );
}

STDMETHODIMP Machine::COMGETTER(PCIDeviceAssignments)(ComSafeArrayOut(IPCIDeviceAttachment *, aAssignments))
{
    CheckComArgOutSafeArrayPointerValid(aAssignments);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IPCIDeviceAttachment> assignments(mHWData->mPCIDeviceAssignments);
    assignments.detachTo(ComSafeArrayOutArg(aAssignments));

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(BandwidthControl)(IBandwidthControl **aBandwidthControl)
{
    CheckComArgOutPointerValid(aBandwidthControl);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    mBandwidthControl.queryInterfaceTo(aBandwidthControl);

    return S_OK;
}

STDMETHODIMP Machine::COMGETTER(TracingEnabled)(BOOL *pfEnabled)
{
    CheckComArgOutPointerValid(pfEnabled);
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        *pfEnabled = mHWData->mDebugging.fTracingEnabled;
    }
    return hrc;
}

STDMETHODIMP Machine::COMSETTER(TracingEnabled)(BOOL fEnabled)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        hrc = checkStateDependency(MutableStateDep);
        if (SUCCEEDED(hrc))
        {
            hrc = mHWData.backupEx();
            if (SUCCEEDED(hrc))
            {
                setModified(IsModified_MachineData);
                mHWData->mDebugging.fTracingEnabled = fEnabled != FALSE;
            }
        }
    }
    return hrc;
}

STDMETHODIMP Machine::COMGETTER(TracingConfig)(BSTR *pbstrConfig)
{
    CheckComArgOutPointerValid(pbstrConfig);
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        hrc = mHWData->mDebugging.strTracingConfig.cloneToEx(pbstrConfig);
    }
    return hrc;
}

STDMETHODIMP Machine::COMSETTER(TracingConfig)(IN_BSTR bstrConfig)
{
    CheckComArgStr(bstrConfig);
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        hrc = checkStateDependency(MutableStateDep);
        if (SUCCEEDED(hrc))
        {
            hrc = mHWData.backupEx();
            if (SUCCEEDED(hrc))
            {
                hrc = mHWData->mDebugging.strTracingConfig.cloneEx(bstrConfig);
                if (SUCCEEDED(hrc))
                    setModified(IsModified_MachineData);
            }
        }
    }
    return hrc;

}

STDMETHODIMP Machine::COMGETTER(AllowTracingToAccessVM)(BOOL *pfAllow)
{
    CheckComArgOutPointerValid(pfAllow);
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        *pfAllow = mHWData->mDebugging.fAllowTracingToAccessVM;
    }
    return hrc;
}

STDMETHODIMP Machine::COMSETTER(AllowTracingToAccessVM)(BOOL fAllow)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        hrc = checkStateDependency(MutableStateDep);
        if (SUCCEEDED(hrc))
        {
            hrc = mHWData.backupEx();
            if (SUCCEEDED(hrc))
            {
                setModified(IsModified_MachineData);
                mHWData->mDebugging.fAllowTracingToAccessVM = fAllow != FALSE;
            }
        }
    }
    return hrc;
}

STDMETHODIMP Machine::COMGETTER(AutostartEnabled)(BOOL *pfEnabled)
{
    CheckComArgOutPointerValid(pfEnabled);
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        *pfEnabled = mHWData->mAutostart.fAutostartEnabled;
    }
    return hrc;
}

STDMETHODIMP Machine::COMSETTER(AutostartEnabled)(BOOL fEnabled)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        hrc = checkStateDependency(MutableStateDep);
        if (   SUCCEEDED(hrc)
            && mHWData->mAutostart.fAutostartEnabled != !!fEnabled)
        {
            AutostartDb *autostartDb = mParent->getAutostartDb();
            int vrc;

            if (fEnabled)
                vrc = autostartDb->addAutostartVM(mUserData->s.strName.c_str());
            else
                vrc = autostartDb->removeAutostartVM(mUserData->s.strName.c_str());

            if (RT_SUCCESS(vrc))
            {
                hrc = mHWData.backupEx();
                if (SUCCEEDED(hrc))
                {
                    setModified(IsModified_MachineData);
                    mHWData->mAutostart.fAutostartEnabled = fEnabled != FALSE;
                }
            }
            else if (vrc == VERR_NOT_SUPPORTED)
                hrc = setError(VBOX_E_NOT_SUPPORTED,
                               tr("The VM autostart feature is not supported on this platform"));
            else if (vrc == VERR_PATH_NOT_FOUND)
                hrc = setError(E_FAIL,
                               tr("The path to the autostart database is not set"));
            else
                hrc = setError(E_UNEXPECTED,
                               tr("%s machine '%s' to the autostart database failed with %Rrc"),
                               fEnabled ? "Adding" : "Removing",
                               mUserData->s.strName.c_str(), vrc);
        }
    }
    return hrc;
}

STDMETHODIMP Machine::COMGETTER(AutostartDelay)(ULONG *puDelay)
{
    CheckComArgOutPointerValid(puDelay);
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        *puDelay = mHWData->mAutostart.uAutostartDelay;
    }
    return hrc;
}

STDMETHODIMP Machine::COMSETTER(AutostartDelay)(ULONG uDelay)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        hrc = checkStateDependency(MutableStateDep);
        if (SUCCEEDED(hrc))
        {
            hrc = mHWData.backupEx();
            if (SUCCEEDED(hrc))
            {
                setModified(IsModified_MachineData);
                mHWData->mAutostart.uAutostartDelay = uDelay;
            }
        }
    }
    return hrc;
}

STDMETHODIMP Machine::COMGETTER(AutostopType)(AutostopType_T *penmAutostopType)
{
    CheckComArgOutPointerValid(penmAutostopType);
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        *penmAutostopType = mHWData->mAutostart.enmAutostopType;
    }
    return hrc;
}

STDMETHODIMP Machine::COMSETTER(AutostopType)(AutostopType_T enmAutostopType)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        hrc = checkStateDependency(MutableStateDep);
        if (   SUCCEEDED(hrc)
            && mHWData->mAutostart.enmAutostopType != enmAutostopType)
        {
            AutostartDb *autostartDb = mParent->getAutostartDb();
            int vrc;

            if (enmAutostopType != AutostopType_Disabled)
                vrc = autostartDb->addAutostopVM(mUserData->s.strName.c_str());
            else
                vrc = autostartDb->removeAutostopVM(mUserData->s.strName.c_str());

            if (RT_SUCCESS(vrc))
            {
                hrc = mHWData.backupEx();
                if (SUCCEEDED(hrc))
                {
                    setModified(IsModified_MachineData);
                    mHWData->mAutostart.enmAutostopType = enmAutostopType;
                }
            }
            else if (vrc == VERR_NOT_SUPPORTED)
                hrc = setError(VBOX_E_NOT_SUPPORTED,
                               tr("The VM autostop feature is not supported on this platform"));
            else if (vrc == VERR_PATH_NOT_FOUND)
                hrc = setError(E_FAIL,
                               tr("The path to the autostart database is not set"));
            else
                hrc = setError(E_UNEXPECTED,
                               tr("%s machine '%s' to the autostop database failed with %Rrc"),
                               enmAutostopType != AutostopType_Disabled ? "Adding" : "Removing",
                               mUserData->s.strName.c_str(), vrc);
        }
    }
    return hrc;
}


STDMETHODIMP Machine::CloneTo(IMachine *pTarget, CloneMode_T mode, ComSafeArrayIn(CloneOptions_T, options), IProgress **pProgress)
{
    LogFlowFuncEnter();

    CheckComArgNotNull(pTarget);
    CheckComArgOutPointerValid(pProgress);

    /* Convert the options. */
    RTCList<CloneOptions_T> optList;
    if (options != NULL)
        optList = com::SafeArray<CloneOptions_T>(ComSafeArrayInArg(options)).toList();

    if (optList.contains(CloneOptions_Link))
    {
        if (!isSnapshotMachine())
            return setError(E_INVALIDARG,
                            tr("Linked clone can only be created from a snapshot"));
        if (mode != CloneMode_MachineState)
            return setError(E_INVALIDARG,
                            tr("Linked clone can only be created for a single machine state"));
    }
    AssertReturn(!(optList.contains(CloneOptions_KeepAllMACs) && optList.contains(CloneOptions_KeepNATMACs)), E_INVALIDARG);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();


    MachineCloneVM *pWorker = new MachineCloneVM(this, static_cast<Machine*>(pTarget), mode, optList);

    HRESULT rc = pWorker->start(pProgress);

    LogFlowFuncLeave();

    return rc;
}

// public methods for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Adds the given IsModified_* flag to the dirty flags of the machine.
 * This must be called either during loadSettings or under the machine write lock.
 * @param fl
 */
void Machine::setModified(uint32_t fl, bool fAllowStateModification /* = true */)
{
    mData->flModifications |= fl;
    if (fAllowStateModification && isStateModificationAllowed())
        mData->mCurrentStateModified = true;
}

/**
 * Adds the given IsModified_* flag to the dirty flags of the machine, taking
 * care of the write locking.
 *
 * @param   fModifications      The flag to add.
 */
void Machine::setModifiedLock(uint32_t fModification, bool fAllowStateModification /* = true */)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    setModified(fModification, fAllowStateModification);
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
 * Copies strSource to strTarget, making it relative to the machine folder
 * if it is a subdirectory thereof, or simply copying it otherwise.
 *
 * @param strSource Path to evaluate and copy.
 * @param strTarget Buffer to receive target path.
 *
 * @note Locks this object for reading.
 */
void Machine::copyPathRelativeToMachine(const Utf8Str &strSource,
                                        Utf8Str &strTarget)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), (void)0);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturnVoid(!mData->m_strConfigFileFull.isEmpty());
    // use strTarget as a temporary buffer to hold the machine settings dir
    strTarget = mData->m_strConfigFileFull;
    strTarget.stripFilename();
    if (RTPathStartsWith(strSource.c_str(), strTarget.c_str()))
    {
        // is relative: then append what's left
        strTarget = strSource.substr(strTarget.length() + 1); // skip '/'
        // for empty paths (only possible for subdirs) use "." to avoid
        // triggering default settings for not present config attributes.
        if (strTarget.isEmpty())
            strTarget = ".";
    }
    else
        // is not relative: then overwrite
        strTarget = strSource;
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

    char szTmp[RTPATH_MAX];
    int vrc = RTEnvGetEx(RTENV_DEFAULT, "VBOX_USER_VMLOGDIR", szTmp, sizeof(szTmp), NULL);
    if (RT_SUCCESS(vrc))
    {
        if (szTmp[0] && !mUserData.isNull())
        {
            char szTmp2[RTPATH_MAX];
            vrc = RTPathAbs(szTmp, szTmp2, sizeof(szTmp2));
            if (RT_SUCCESS(vrc))
                aLogFolder = BstrFmt("%s%c%s",
                                     szTmp2,
                                     RTPATH_DELIMITER,
                                     mUserData->s.strName.c_str()); // path/to/logfolder/vmname
        }
        else
            vrc = VERR_PATH_IS_RELATIVE;
    }

    if (RT_FAILURE(vrc))
    {
        // fallback if VBOX_USER_LOGHOME is not set or invalid
        aLogFolder = mData->m_strConfigFileFull;    // path/to/machinesfolder/vmname/vmname.vbox
        aLogFolder.stripFilename();                 // path/to/machinesfolder/vmname
        aLogFolder.append(RTPATH_DELIMITER);
        aLogFolder.append("Logs");                  // path/to/machinesfolder/vmname/Logs
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
                         logFolder.c_str(), RTPATH_DELIMITER);
    else
        log = Utf8StrFmt("%s%cVBox.log.%d",
                         logFolder.c_str(), RTPATH_DELIMITER, idx);
    return log;
}

/**
 * Composes a unique saved state filename based on the current system time. The filename is
 * granular to the second so this will work so long as no more than one snapshot is taken on
 * a machine per second.
 *
 * Before version 4.1, we used this formula for saved state files:
 *      Utf8StrFmt("%s%c{%RTuuid}.sav", strFullSnapshotFolder.c_str(), RTPATH_DELIMITER, mData->mUuid.raw())
 * which no longer works because saved state files can now be shared between the saved state of the
 * "saved" machine and an online snapshot, and the following would cause problems:
 * 1) save machine
 * 2) create online snapshot from that machine state --> reusing saved state file
 * 3) save machine again --> filename would be reused, breaking the online snapshot
 *
 * So instead we now use a timestamp.
 *
 * @param str
 */
void Machine::composeSavedStateFilename(Utf8Str &strStateFilePath)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        calculateFullPath(mUserData->s.strSnapshotFolder, strStateFilePath);
    }

    RTTIMESPEC ts;
    RTTimeNow(&ts);
    RTTIME time;
    RTTimeExplode(&time, &ts);

    strStateFilePath += RTPATH_DELIMITER;
    strStateFilePath += Utf8StrFmt("%04d-%02u-%02uT%02u-%02u-%02u-%09uZ.sav",
                                   time.i32Year, time.u8Month, time.u8MonthDay,
                                   time.u8Hour, time.u8Minute, time.u8Second, time.u32Nanosecond);
}

/**
 *  @note Locks this object for writing, calls the client process
 *        (inside the lock).
 */
HRESULT Machine::launchVMProcess(IInternalSessionControl *aControl,
                                 const Utf8Str &strType,
                                 const Utf8Str &strEnvironment,
                                 ProgressProxy *aProgress)
{
    LogFlowThisFuncEnter();

    AssertReturn(aControl, E_FAIL);
    AssertReturn(aProgress, E_FAIL);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!mData->mRegistered)
        return setError(E_UNEXPECTED,
                        tr("The machine '%s' is not registered"),
                        mUserData->s.strName.c_str());

    LogFlowThisFunc(("mSession.mState=%s\n", Global::stringifySessionState(mData->mSession.mState)));

    if (    mData->mSession.mState == SessionState_Locked
         || mData->mSession.mState == SessionState_Spawning
         || mData->mSession.mState == SessionState_Unlocking)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("The machine '%s' is already locked by a session (or being locked or unlocked)"),
                        mUserData->s.strName.c_str());

    /* may not be busy */
    AssertReturn(!Global::IsOnlineOrTransient(mData->mMachineState), E_FAIL);

    /* get the path to the executable */
    char szPath[RTPATH_MAX];
    RTPathAppPrivateArch(szPath, sizeof(szPath) - 1);
    size_t sz = strlen(szPath);
    szPath[sz++] = RTPATH_DELIMITER;
    szPath[sz] = 0;
    char *cmd = szPath + sz;
    sz = RTPATH_MAX - sz;

    int vrc = VINF_SUCCESS;
    RTPROCESS pid = NIL_RTPROCESS;

    RTENV env = RTENV_DEFAULT;

    if (!strEnvironment.isEmpty())
    {
        char *newEnvStr = NULL;

        do
        {
            /* clone the current environment */
            int vrc2 = RTEnvClone(&env, RTENV_DEFAULT);
            AssertRCBreakStmt(vrc2, vrc = vrc2);

            newEnvStr = RTStrDup(strEnvironment.c_str());
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
        const char * args[] = {szPath, "--comment", mUserData->s.strName.c_str(), "--startvm", idStr.c_str(), "--no-startvm-errormsgbox", 0 };
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
        const char * args[] = {szPath, "--comment", mUserData->s.strName.c_str(), "--startvm", idStr.c_str(), 0 };
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
        || strType == "vrdp" /* Deprecated. Same as headless. */
       )
    {
        /* On pre-4.0 the "headless" type was used for passing "--vrdp off" to VBoxHeadless to let it work in OSE,
         * which did not contain VRDP server. In VBox 4.0 the remote desktop server (VRDE) is optional,
         * and a VM works even if the server has not been installed.
         * So in 4.0 the "headless" behavior remains the same for default VBox installations.
         * Only if a VRDE has been installed and the VM enables it, the "headless" will work
         * differently in 4.0 and 3.x.
         */
        const char VBoxHeadless_exe[] = "VBoxHeadless" HOSTSUFF_EXE;
        Assert(sz >= sizeof(VBoxHeadless_exe));
        strcpy(cmd, VBoxHeadless_exe);

        Utf8Str idStr = mData->mUuid.toString();
        /* Leave space for "--capture" arg. */
        const char * args[] = {szPath, "--comment", mUserData->s.strName.c_str(),
                                       "--startvm", idStr.c_str(),
                                       "--vrde", "config",
                                       0, /* For "--capture". */
                                       0 };
        if (strType == "capture")
        {
            unsigned pos = RT_ELEMENTS(args) - 2;
            args[pos] = "--capture";
        }
        vrc = RTProcCreate(szPath, args, env,
#ifdef RT_OS_WINDOWS
                RTPROC_FLAGS_NO_WINDOW
#else
                0
#endif
                , &pid);
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
                        tr("Could not launch a process for the machine '%s' (%Rrc)"),
                        mUserData->s.strName.c_str(), vrc);

    LogFlowThisFunc(("launched.pid=%d(0x%x)\n", pid, pid));

    /*
     *  Note that we don't release the lock here before calling the client,
     *  because it doesn't need to call us back if called with a NULL argument.
     *  Releasing the lock here is dangerous because we didn't prepare the
     *  launch data yet, but the client we've just started may happen to be
     *  too fast and call openSession() that will fail (because of PID, etc.),
     *  so that the Machine will never get out of the Spawning session state.
     */

    /* inform the session that it will be a remote one */
    LogFlowThisFunc(("Calling AssignMachine (NULL)...\n"));
    HRESULT rc = aControl->AssignMachine(NULL, LockType_Write);
    LogFlowThisFunc(("AssignMachine (NULL) returned %08X\n", rc));

    if (FAILED(rc))
    {
        /* restore the session state */
        mData->mSession.mState = SessionState_Unlocked;
        /* The failure may occur w/o any error info (from RPC), so provide one */
        return setError(VBOX_E_VM_ERROR,
                        tr("Failed to assign the machine to the session (%Rrc)"), rc);
    }

    /* attach launch data to the machine */
    Assert(mData->mSession.mPID == NIL_RTPROCESS);
    mData->mSession.mRemoteControls.push_back(aControl);
    mData->mSession.mProgress = aProgress;
    mData->mSession.mPID = pid;
    mData->mSession.mState = SessionState_Spawning;
    mData->mSession.mType = strType;

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

    if (    mData->mSession.mState == SessionState_Locked
         || (aAllowClosing && mData->mSession.mState == SessionState_Unlocking)
       )
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
            AssertReturn(mData->mSession.mPID != NIL_RTPROCESS, false);
            *aPID = mData->mSession.mPID;
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
                  tr("The virtual machine '%s' has terminated unexpectedly during startup"),
                  getName().c_str());
#else

    /* PID not yet initialized, skip check. */
    if (mData->mSession.mPID == NIL_RTPROCESS)
        return false;

    RTPROCSTATUS status;
    int vrc = ::RTProcWait(mData->mSession.mPID, RTPROCWAIT_FLAGS_NOBLOCK,
                           &status);

    if (vrc != VERR_PROCESS_RUNNING)
    {
        if (RT_SUCCESS(vrc) && status.enmReason == RTPROCEXITREASON_NORMAL)
            rc = setError(E_FAIL,
                          tr("The virtual machine '%s' has terminated unexpectedly during startup with exit code %d"),
                          getName().c_str(), status.iStatus);
        else if (RT_SUCCESS(vrc) && status.enmReason == RTPROCEXITREASON_SIGNAL)
            rc = setError(E_FAIL,
                          tr("The virtual machine '%s' has terminated unexpectedly during startup because of signal %d"),
                          getName().c_str(), status.iStatus);
        else if (RT_SUCCESS(vrc) && status.enmReason == RTPROCEXITREASON_ABEND)
            rc = setError(E_FAIL,
                          tr("The virtual machine '%s' has terminated abnormally"),
                          getName().c_str(), status.iStatus);
        else
            rc = setError(E_FAIL,
                          tr("The virtual machine '%s' has terminated unexpectedly during startup (%Rrc)"),
                          getName().c_str(), rc);
    }

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
        mData->mSession.mState = SessionState_Unlocked;

        /* finalize the progress after setting the state */
        if (!mData->mSession.mProgress.isNull())
        {
            mData->mSession.mProgress->notifyComplete(rc);
            mData->mSession.mProgress.setNull();
        }

        mParent->addProcessToReap(mData->mSession.mPID);
        mData->mSession.mPID = NIL_RTPROCESS;

        mParent->onSessionStateChange(mData->mUuid, SessionState_Unlocked);
        return true;
    }

    return false;
}

/**
 *  Checks whether the machine can be registered. If so, commits and saves
 *  all settings.
 *
 *  @note Must be called from mParent's write lock. Locks this object and
 *  children for writing.
 */
HRESULT Machine::prepareRegister()
{
    AssertReturn(mParent->isWriteLockOnCurrentThread(), E_FAIL);

    AutoLimitedCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* wait for state dependents to drop to zero */
    ensureNoStateDependencies();

    if (!mData->mAccessible)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("The machine '%s' with UUID {%s} is inaccessible and cannot be registered"),
                        mUserData->s.strName.c_str(),
                        mData->mUuid.toString().c_str());

    AssertReturn(autoCaller.state() == Ready, E_FAIL);

    if (mData->mRegistered)
        return setError(VBOX_E_INVALID_OBJECT_STATE,
                        tr("The machine '%s' with UUID {%s} is already registered"),
                        mUserData->s.strName.c_str(),
                        mData->mUuid.toString().c_str());

    HRESULT rc = S_OK;

    // Ensure the settings are saved. If we are going to be registered and
    // no config file exists yet, create it by calling saveSettings() too.
    if (    (mData->flModifications)
         || (!mData->pMachineConfigFile->fileExists())
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

        mData->mRegistered = true;
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
 *  S_OK. It is supposed, that on failure, the caller will immediately return
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
 *  When @a aDepType is OfflineStateDep, this method returns S_OK if the
 *  state is one of the 4 offline states (PoweredOff, Saved, Teleported,
 *  Aborted).
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
                && (   !isSessionMachine()  /** @todo This was just converted raw; Check if Running and Paused should actually be included here... (Live Migration) */
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
                && (   !isSessionMachine() /** @todo This was just converted raw; Check if Running and Paused should actually be included here... (Live Migration) */
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
        case OfflineStateDep:
        {
            if (   mData->mRegistered
                && (   !isSessionMachine()
                    || (   mData->mMachineState != MachineState_PoweredOff
                        && mData->mMachineState != MachineState_Saved
                        && mData->mMachineState != MachineState_Aborted
                        && mData->mMachineState != MachineState_Teleported
                       )
                   )
               )
                return setError(VBOX_E_INVALID_VM_STATE,
                                tr("The machine is not offline (state is %s)"),
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
    mUserData->s.strOsType = mParent->getUnknownOSType()->id();

    /* create associated BIOS settings object */
    unconst(mBIOSSettings).createObject();
    mBIOSSettings->init(this);

    /* create an associated VRDE object (default is disabled) */
    unconst(mVRDEServer).createObject();
    mVRDEServer->init(this);

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
    mNetworkAdapters.resize(Global::getMaxNetworkAdapters(mHWData->mChipsetType));
    for (ULONG slot = 0; slot < mNetworkAdapters.size(); slot++)
    {
        unconst(mNetworkAdapters[slot]).createObject();
        mNetworkAdapters[slot]->init(this, slot);
    }

    /* create the bandwidth control */
    unconst(mBandwidthControl).createObject();
    mBandwidthControl->init(this);

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

    /* tell all our other child objects we've been uninitialized */
    if (mBandwidthControl)
    {
        mBandwidthControl->uninit();
        unconst(mBandwidthControl).setNull();
    }

    for (ULONG slot = 0; slot < mNetworkAdapters.size(); slot++)
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

    if (mVRDEServer)
    {
        mVRDEServer->uninit();
        unconst(mVRDEServer).setNull();
    }

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
    if (    !!mMediaData
         && (!isSessionMachine())
       )
    {
        for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
             it != mMediaData->mAttachments.end();
             ++it)
        {
            ComObjPtr<Medium> hd = (*it)->getMedium();
            if (hd.isNull())
                continue;
            HRESULT rc = hd->removeBackReference(mData->mUuid, getSnapshotId());
            AssertComRC(rc);
        }
    }

    if (!isSessionMachine() && !isSnapshotMachine())
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
 *  object's pointer itself. For SessionMachine objects, returns the peer
 *  (primary) machine pointer.
 */
Machine* Machine::getMachine()
{
    if (isSessionMachine())
        return (Machine*)mPeer;
    return this;
}

/**
 * Makes sure that there are no machine state dependents. If necessary, waits
 * for the number of dependents to drop to zero.
 *
 * Make sure this method is called from under this object's write lock to
 * guarantee that no new dependents may be added when this method returns
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

    /* Wait for all state dependents if necessary */
    if (mData->mMachineStateDeps != 0)
    {
        /* lazy semaphore creation */
        if (mData->mMachineStateDepsSem == NIL_RTSEMEVENTMULTI)
            RTSemEventMultiCreate(&mData->mMachineStateDepsSem);

        LogFlowThisFunc(("Waiting for state deps (%d) to drop to zero...\n",
                          mData->mMachineStateDeps));

        ++mData->mMachineStateChangePending;

        /* reset the semaphore before waiting, the last dependent will signal
         * it */
        RTSemEventMultiReset(mData->mMachineStateDepsSem);

        alock.release();

        RTSemEventMultiWait(mData->mMachineStateDepsSem, RT_INDEFINITE_WAIT);

        alock.acquire();

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

    /* wait for state dependents to drop to zero */
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
HRESULT Machine::findSharedFolder(const Utf8Str &aName,
                                  ComObjPtr<SharedFolder> &aSharedFolder,
                                  bool aSetError /* = false */)
{
    HRESULT rc = VBOX_E_OBJECT_NOT_FOUND;
    for (HWData::SharedFolderList::const_iterator it = mHWData->mSharedFolders.begin();
        it != mHWData->mSharedFolders.end();
        ++it)
    {
        SharedFolder *pSF = *it;
        AutoCaller autoCaller(pSF);
        if (pSF->getName() == aName)
        {
            aSharedFolder = pSF;
            rc = S_OK;
            break;
        }
    }

    if (aSetError && FAILED(rc))
        setError(rc, tr("Could not find a shared folder named '%s'"), aName.c_str());

    return rc;
}

/**
 * Initializes all machine instance data from the given settings structures
 * from XML. The exception is the machine UUID which needs special handling
 * depending on the caller's use case, so the caller needs to set that herself.
 *
 * This gets called in several contexts during machine initialization:
 *
 * -- When machine XML exists on disk already and needs to be loaded into memory,
 *    for example, from registeredInit() to load all registered machines on
 *    VirtualBox startup. In this case, puuidRegistry is NULL because the media
 *    attached to the machine should be part of some media registry already.
 *
 * -- During OVF import, when a machine config has been constructed from an
 *    OVF file. In this case, puuidRegistry is set to the machine UUID to
 *    ensure that the media listed as attachments in the config (which have
 *    been imported from the OVF) receive the correct registry ID.
 *
 * -- During VM cloning.
 *
 * @param config Machine settings from XML.
 * @param puuidRegistry If != NULL, Medium::setRegistryIdIfFirst() gets called with this registry ID for each attached medium in the config.
 * @return
 */
HRESULT Machine::loadMachineDataFromSettings(const settings::MachineConfigFile &config,
                                             const Guid *puuidRegistry)
{
    // copy name, description, OS type, teleporter, UTC etc.
    mUserData->s = config.machineUserData;

    // look up the object by Id to check it is valid
    ComPtr<IGuestOSType> guestOSType;
    HRESULT rc = mParent->GetGuestOSType(Bstr(mUserData->s.strOsType).raw(),
                                         guestOSType.asOutParam());
    if (FAILED(rc)) return rc;

    // stateFile (optional)
    if (config.strStateFile.isEmpty())
        mSSData->strStateFilePath.setNull();
    else
    {
        Utf8Str stateFilePathFull(config.strStateFile);
        int vrc = calculateFullPath(stateFilePathFull, stateFilePathFull);
        if (RT_FAILURE(vrc))
            return setError(E_FAIL,
                            tr("Invalid saved state file path '%s' (%Rrc)"),
                            config.strStateFile.c_str(),
                            vrc);
        mSSData->strStateFilePath = stateFilePathFull;
    }

    // snapshot folder needs special processing so set it again
    rc = COMSETTER(SnapshotFolder)(Bstr(config.machineUserData.strSnapshotFolder).raw());
    if (FAILED(rc)) return rc;

    /* Copy the extra data items (Not in any case config is already the same as
     * mData->pMachineConfigFile, like when the xml files are read from disk. So
     * make sure the extra data map is copied). */
    mData->pMachineConfigFile->mapExtraDataItems = config.mapExtraDataItems;

    /* currentStateModified (optional, default is true) */
    mData->mCurrentStateModified = config.fCurrentStateModified;

    mData->mLastStateChange = config.timeLastStateChange;

    /*
     *  note: all mUserData members must be assigned prior this point because
     *  we need to commit changes in order to let mUserData be shared by all
     *  snapshot machine instances.
     */
    mUserData.commitCopy();

    // machine registry, if present (must be loaded before snapshots)
    if (config.canHaveOwnMediaRegistry())
    {
        // determine machine folder
        Utf8Str strMachineFolder = getSettingsFileFull();
        strMachineFolder.stripFilename();
        rc = mParent->initMedia(getId(),         // media registry ID == machine UUID
                                config.mediaRegistry,
                                strMachineFolder);
        if (FAILED(rc)) return rc;
    }

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

    // hardware data
    rc = loadHardware(config.hardwareMachine, &config.debugging, &config.autostart);
    if (FAILED(rc)) return rc;

    // load storage controllers
    rc = loadStorageControllers(config.storageMachine,
                                puuidRegistry,
                                NULL /* puuidSnapshot */);
    if (FAILED(rc)) return rc;

    /*
     *  NOTE: the assignment below must be the last thing to do,
     *  otherwise it will be not possible to change the settings
     *  somewhere in the code above because all setters will be
     *  blocked by checkStateDependency(MutableStateDep).
     */

    /* set the machine state to Aborted or Saved when appropriate */
    if (config.fAborted)
    {
        mSSData->strStateFilePath.setNull();

        /* no need to use setMachineState() during init() */
        mData->mMachineState = MachineState_Aborted;
    }
    else if (!mSSData->strStateFilePath.isEmpty())
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
    AssertReturn(!isSnapshotMachine(), E_FAIL);
    AssertReturn(!isSessionMachine(), E_FAIL);

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
                            strStateFile.c_str(),
                            vrc);
    }

    /* create a snapshot machine object */
    ComObjPtr<SnapshotMachine> pSnapshotMachine;
    pSnapshotMachine.createObject();
    rc = pSnapshotMachine->initFromSettings(this,
                                            data.hardware,
                                            &data.debugging,
                                            &data.autostart,
                                            data.storage,
                                            data.uuid.ref(),
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
 *  Loads settings into mHWData.
 *
 *  @param data           Reference to the hardware settings.
 *  @param pDbg           Pointer to the debugging settings.
 *  @param pAutostart     Pointer to the autostart settings.
 */
HRESULT Machine::loadHardware(const settings::Hardware &data, const settings::Debugging *pDbg,
                              const settings::Autostart *pAutostart)
{
    AssertReturn(!isSessionMachine(), E_FAIL);

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
        mHWData->mHWVirtExForceEnabled        = data.fHardwareVirtForce;
        mHWData->mPAEEnabled                  = data.fPAE;
        mHWData->mSyntheticCpu                = data.fSyntheticCpu;

        mHWData->mCPUCount                    = data.cCPUs;
        mHWData->mCPUHotPlugEnabled           = data.fCpuHotPlug;
        mHWData->mCpuExecutionCap             = data.ulCpuExecutionCap;

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
        mHWData->mPageFusionEnabled = data.fPageFusionEnabled;

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
        mHWData->mVideoCaptureWidth = data.ulVideoCaptureHorzRes;
        mHWData->mVideoCaptureHeight = data.ulVideoCaptureVertRes;
        mHWData->mVideoCaptureEnabled = data.fVideoCaptureEnabled;
        mHWData->mVideoCaptureFile = data.strVideoCaptureFile;
        mHWData->mFirmwareType = data.firmwareType;
        mHWData->mPointingHIDType = data.pointingHIDType;
        mHWData->mKeyboardHIDType = data.keyboardHIDType;
        mHWData->mChipsetType = data.chipsetType;
        mHWData->mEmulatedUSBCardReaderEnabled = data.fEmulatedUSBCardReader;
        mHWData->mHPETEnabled = data.fHPETEnabled;

        /* VRDEServer */
        rc = mVRDEServer->loadSettings(data.vrdeSettings);
        if (FAILED(rc)) return rc;

        /* BIOS */
        rc = mBIOSSettings->loadSettings(data.biosSettings);
        if (FAILED(rc)) return rc;

        // Bandwidth control (must come before network adapters)
        rc = mBandwidthControl->loadSettings(data.ioSettings);
        if (FAILED(rc)) return rc;

        /* USB Controller */
        rc = mUSBController->loadSettings(data.usbController);
        if (FAILED(rc)) return rc;

        // network adapters
        uint32_t newCount = Global::getMaxNetworkAdapters(mHWData->mChipsetType);
        uint32_t oldCount = mNetworkAdapters.size();
        if (newCount > oldCount)
        {
            mNetworkAdapters.resize(newCount);
            for (ULONG slot = oldCount; slot < mNetworkAdapters.size(); slot++)
            {
                unconst(mNetworkAdapters[slot]).createObject();
                mNetworkAdapters[slot]->init(this, slot);
            }
        }
        else if (newCount < oldCount)
            mNetworkAdapters.resize(newCount);
        for (settings::NetworkAdaptersList::const_iterator it = data.llNetworkAdapters.begin();
            it != data.llNetworkAdapters.end();
            ++it)
        {
            const settings::NetworkAdapter &nic = *it;

            /* slot unicity is guaranteed by XML Schema */
            AssertBreak(nic.ulSlot < mNetworkAdapters.size());
            rc = mNetworkAdapters[nic.ulSlot]->loadSettings(mBandwidthControl, nic);
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

        /* Shared folders */
        for (settings::SharedFoldersList::const_iterator it = data.llSharedFolders.begin();
             it != data.llSharedFolders.end();
             ++it)
        {
            const settings::SharedFolder &sf = *it;

            ComObjPtr<SharedFolder> sharedFolder;
            /* Check for double entries. Not allowed! */
            rc = findSharedFolder(sf.strName, sharedFolder, false /* aSetError */);
            if (SUCCEEDED(rc))
                return setError(VBOX_E_OBJECT_IN_USE,
                                tr("Shared folder named '%s' already exists"),
                                sf.strName.c_str());

            /* Create the new shared folder. Don't break on error. This will be
             * reported when the machine starts. */
            sharedFolder.createObject();
            rc = sharedFolder->init(getMachine(),
                                    sf.strName,
                                    sf.strHostPath,
                                    RT_BOOL(sf.fWritable),
                                    RT_BOOL(sf.fAutoMount),
                                    false /* fFailOnError */);
            if (FAILED(rc)) return rc;
            mHWData->mSharedFolders.push_back(sharedFolder);
        }

        // Clipboard
        mHWData->mClipboardMode = data.clipboardMode;

        // drag'n'drop
        mHWData->mDragAndDropMode = data.dragAndDropMode;

        // guest settings
        mHWData->mMemoryBalloonSize = data.ulMemoryBalloonSize;

        // IO settings
        mHWData->mIOCacheEnabled = data.ioSettings.fIOCacheEnabled;
        mHWData->mIOCacheSize = data.ioSettings.ulIOCacheSize;

        // Host PCI devices
        for (settings::HostPCIDeviceAttachmentList::const_iterator it = data.pciAttachments.begin();
             it != data.pciAttachments.end();
             ++it)
        {
            const settings::HostPCIDeviceAttachment &hpda = *it;
            ComObjPtr<PCIDeviceAttachment> pda;

            pda.createObject();
            pda->loadSettings(this, hpda);
            mHWData->mPCIDeviceAssignments.push_back(pda);
        }

        /*
         * (The following isn't really real hardware, but it lives in HWData
         * for reasons of convenience.)
         */

#ifdef VBOX_WITH_GUEST_PROPS
        /* Guest properties (optional) */
        for (settings::GuestPropertiesList::const_iterator it = data.llGuestProperties.begin();
            it != data.llGuestProperties.end();
            ++it)
        {
            const settings::GuestProperty &prop = *it;
            uint32_t fFlags = guestProp::NILFLAG;
            guestProp::validateFlags(prop.strFlags.c_str(), &fFlags);
            HWData::GuestProperty property = { prop.strName, prop.strValue, (LONG64) prop.timestamp, fFlags };
            mHWData->mGuestProperties.push_back(property);
        }

        mHWData->mGuestPropertyNotificationPatterns = data.strNotificationPatterns;
#endif /* VBOX_WITH_GUEST_PROPS defined */

        rc = loadDebugging(pDbg);
        if (FAILED(rc))
            return rc;

        mHWData->mAutostart = *pAutostart;
    }
    catch(std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }

    AssertComRC(rc);
    return rc;
}

/**
 * Called from Machine::loadHardware() to load the debugging settings of the
 * machine.
 *
 * @param   pDbg        Pointer to the settings.
 */
HRESULT Machine::loadDebugging(const settings::Debugging *pDbg)
{
    mHWData->mDebugging = *pDbg;
    /* no more processing currently required, this will probably change. */
    return S_OK;
}

/**
 *  Called from loadMachineDataFromSettings() for the storage controller data, including media.
 *
 * @param data
 * @param puuidRegistry media registry ID to set media to or NULL; see Machine::loadMachineDataFromSettings()
 * @param puuidSnapshot
 * @return
 */
HRESULT Machine::loadStorageControllers(const settings::Storage &data,
                                        const Guid *puuidRegistry,
                                        const Guid *puuidSnapshot)
{
    AssertReturn(!isSessionMachine(), E_FAIL);

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
                            ctlData.strName.c_str());

        pCtl.createObject();
        rc = pCtl->init(this,
                        ctlData.strName,
                        ctlData.storageBus,
                        ctlData.ulInstance,
                        ctlData.fBootable);
        if (FAILED(rc)) return rc;

        mStorageControllers->push_back(pCtl);

        rc = pCtl->COMSETTER(ControllerType)(ctlData.controllerType);
        if (FAILED(rc)) return rc;

        rc = pCtl->COMSETTER(PortCount)(ctlData.ulPortCount);
        if (FAILED(rc)) return rc;

        rc = pCtl->COMSETTER(UseHostIOCache)(ctlData.fUseHostIOCache);
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
                                puuidRegistry,
                                puuidSnapshot);
        if (FAILED(rc)) return rc;
    }

    return S_OK;
}

/**
 * Called from loadStorageControllers for a controller's devices.
 *
 * @param aStorageController
 * @param data
 * @param puuidRegistry media registry ID to set media to or NULL; see Machine::loadMachineDataFromSettings()
 * @param aSnapshotId  pointer to the snapshot ID if this is a snapshot machine
 * @return
 */
HRESULT Machine::loadStorageDevices(StorageController *aStorageController,
                                    const settings::StorageController &data,
                                    const Guid *puuidRegistry,
                                    const Guid *puuidSnapshot)
{
    HRESULT rc = S_OK;

    /* paranoia: detect duplicate attachments */
    for (settings::AttachedDevicesList::const_iterator it = data.llAttachedDevices.begin();
         it != data.llAttachedDevices.end();
         ++it)
    {
        const settings::AttachedDevice &ad = *it;

        for (settings::AttachedDevicesList::const_iterator it2 = it;
             it2 != data.llAttachedDevices.end();
             ++it2)
        {
            if (it == it2)
                continue;

            const settings::AttachedDevice &ad2 = *it2;

            if (   ad.lPort == ad2.lPort
                && ad.lDevice == ad2.lDevice)
            {
                return setError(E_FAIL,
                                tr("Duplicate attachments for storage controller '%s', port %d, device %d of the virtual machine '%s'"),
                                aStorageController->getName().c_str(),
                                ad.lPort,
                                ad.lDevice,
                                mUserData->s.strName.c_str());
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
            case DeviceType_DVD:
                if (dev.strHostDriveSrc.isNotEmpty())
                    rc = mParent->host()->findHostDriveByName(dev.deviceType, dev.strHostDriveSrc, false /* fRefresh */, medium);
                else
                    rc = mParent->findRemoveableMedium(dev.deviceType,
                                                       dev.uuid,
                                                       false /* fRefresh */,
                                                       false /* aSetError */,
                                                       medium);
                if (rc == VBOX_E_OBJECT_NOT_FOUND)
                    // This is not an error. The host drive or UUID might have vanished, so just go ahead without this removeable medium attachment
                    rc = S_OK;
            break;

            case DeviceType_HardDisk:
            {
                /* find a hard disk by UUID */
                rc = mParent->findHardDiskById(dev.uuid, true /* aDoSetError */, &medium);
                if (FAILED(rc))
                {
                    if (isSnapshotMachine())
                    {
                        // wrap another error message around the "cannot find hard disk" set by findHardDisk
                        // so the user knows that the bad disk is in a snapshot somewhere
                        com::ErrorInfo info;
                        return setError(E_FAIL,
                                        tr("A differencing image of snapshot {%RTuuid} could not be found. %ls"),
                                        puuidSnapshot->raw(),
                                        info.getText().raw());
                    }
                    else
                        return rc;
                }

                AutoWriteLock hdLock(medium COMMA_LOCKVAL_SRC_POS);

                if (medium->getType() == MediumType_Immutable)
                {
                    if (isSnapshotMachine())
                        return setError(E_FAIL,
                                        tr("Immutable hard disk '%s' with UUID {%RTuuid} cannot be directly attached to snapshot with UUID {%RTuuid} "
                                           "of the virtual machine '%s' ('%s')"),
                                        medium->getLocationFull().c_str(),
                                        dev.uuid.raw(),
                                        puuidSnapshot->raw(),
                                        mUserData->s.strName.c_str(),
                                        mData->m_strConfigFileFull.c_str());

                    return setError(E_FAIL,
                                    tr("Immutable hard disk '%s' with UUID {%RTuuid} cannot be directly attached to the virtual machine '%s' ('%s')"),
                                    medium->getLocationFull().c_str(),
                                    dev.uuid.raw(),
                                    mUserData->s.strName.c_str(),
                                    mData->m_strConfigFileFull.c_str());
                }

                if (medium->getType() == MediumType_MultiAttach)
                {
                    if (isSnapshotMachine())
                        return setError(E_FAIL,
                                        tr("Multi-attach hard disk '%s' with UUID {%RTuuid} cannot be directly attached to snapshot with UUID {%RTuuid} "
                                           "of the virtual machine '%s' ('%s')"),
                                        medium->getLocationFull().c_str(),
                                        dev.uuid.raw(),
                                        puuidSnapshot->raw(),
                                        mUserData->s.strName.c_str(),
                                        mData->m_strConfigFileFull.c_str());

                    return setError(E_FAIL,
                                    tr("Multi-attach hard disk '%s' with UUID {%RTuuid} cannot be directly attached to the virtual machine '%s' ('%s')"),
                                    medium->getLocationFull().c_str(),
                                    dev.uuid.raw(),
                                    mUserData->s.strName.c_str(),
                                    mData->m_strConfigFileFull.c_str());
                }

                if (    !isSnapshotMachine()
                     && medium->getChildren().size() != 0
                   )
                    return setError(E_FAIL,
                                    tr("Hard disk '%s' with UUID {%RTuuid} cannot be directly attached to the virtual machine '%s' ('%s') "
                                       "because it has %d differencing child hard disks"),
                                    medium->getLocationFull().c_str(),
                                    dev.uuid.raw(),
                                    mUserData->s.strName.c_str(),
                                    mData->m_strConfigFileFull.c_str(),
                                    medium->getChildren().size());

                if (findAttachment(mMediaData->mAttachments,
                                   medium))
                    return setError(E_FAIL,
                                    tr("Hard disk '%s' with UUID {%RTuuid} is already attached to the virtual machine '%s' ('%s')"),
                                    medium->getLocationFull().c_str(),
                                    dev.uuid.raw(),
                                    mUserData->s.strName.c_str(),
                                    mData->m_strConfigFileFull.c_str());

                break;
            }

            default:
                return setError(E_FAIL,
                                tr("Device '%s' with unknown type is attached to the virtual machine '%s' ('%s')"),
                                medium->getLocationFull().c_str(),
                                mUserData->s.strName.c_str(),
                                mData->m_strConfigFileFull.c_str());
        }

        if (FAILED(rc))
            break;

        /* Bandwidth groups are loaded at this point. */
        ComObjPtr<BandwidthGroup> pBwGroup;

        if (!dev.strBwGroup.isEmpty())
        {
            rc = mBandwidthControl->getBandwidthGroupByName(dev.strBwGroup, pBwGroup, false /* aSetError */);
            if (FAILED(rc))
                return setError(E_FAIL,
                                tr("Device '%s' with unknown bandwidth group '%s' is attached to the virtual machine '%s' ('%s')"),
                                medium->getLocationFull().c_str(),
                                dev.strBwGroup.c_str(),
                                mUserData->s.strName.c_str(),
                                mData->m_strConfigFileFull.c_str());
            pBwGroup->reference();
        }

        const Bstr controllerName = aStorageController->getName();
        ComObjPtr<MediumAttachment> pAttachment;
        pAttachment.createObject();
        rc = pAttachment->init(this,
                               medium,
                               controllerName,
                               dev.lPort,
                               dev.lDevice,
                               dev.deviceType,
                               false,
                               dev.fPassThrough,
                               dev.fTempEject,
                               dev.fNonRotational,
                               dev.fDiscard,
                               pBwGroup.isNull() ? Utf8Str::Empty : pBwGroup->getName());
        if (FAILED(rc)) break;

        /* associate the medium with this machine and snapshot */
        if (!medium.isNull())
        {
            AutoCaller medCaller(medium);
            if (FAILED(medCaller.rc())) return medCaller.rc();
            AutoWriteLock mlock(medium COMMA_LOCKVAL_SRC_POS);

            if (isSnapshotMachine())
                rc = medium->addBackReference(mData->mUuid, *puuidSnapshot);
            else
                rc = medium->addBackReference(mData->mUuid);
            /* If the medium->addBackReference fails it sets an appropriate
             * error message, so no need to do any guesswork here. */

            if (puuidRegistry)
                // caller wants registry ID to be set on all attached media (OVF import case)
                medium->addRegistry(*puuidRegistry, false /* fRecurse */);
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
HRESULT Machine::findSnapshotById(const Guid &aId,
                                  ComObjPtr<Snapshot> &aSnapshot,
                                  bool aSetError /* = false */)
{
    AutoReadLock chlock(this COMMA_LOCKVAL_SRC_POS);

    if (!mData->mFirstSnapshot)
    {
        if (aSetError)
            return setError(E_FAIL, tr("This machine does not have any snapshots"));
        return E_FAIL;
    }

    if (aId.isEmpty())
        aSnapshot = mData->mFirstSnapshot;
    else
        aSnapshot = mData->mFirstSnapshot->findChildOrSelf(aId.ref());

    if (!aSnapshot)
    {
        if (aSetError)
            return setError(E_FAIL,
                            tr("Could not find a snapshot with UUID {%s}"),
                            aId.toString().c_str());
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
HRESULT Machine::findSnapshotByName(const Utf8Str &strName,
                                    ComObjPtr<Snapshot> &aSnapshot,
                                    bool aSetError /* = false */)
{
    AssertReturn(!strName.isEmpty(), E_INVALIDARG);

    AutoReadLock chlock(this COMMA_LOCKVAL_SRC_POS);

    if (!mData->mFirstSnapshot)
    {
        if (aSetError)
            return setError(VBOX_E_OBJECT_NOT_FOUND,
                            tr("This machine does not have any snapshots"));
        return VBOX_E_OBJECT_NOT_FOUND;
    }

    aSnapshot = mData->mFirstSnapshot->findChildOrSelf(strName);

    if (!aSnapshot)
    {
        if (aSetError)
            return setError(VBOX_E_OBJECT_NOT_FOUND,
                            tr("Could not find a snapshot named '%s'"), strName.c_str());
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
                        aName.c_str());
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
        const ComObjPtr<MediumAttachment> &pAtt = *it;

        // should never happen, but deal with NULL pointers in the list.
        AssertStmt(!pAtt.isNull(), continue);

        // getControllerName() needs caller+read lock
        AutoCaller autoAttCaller(pAtt);
        if (FAILED(autoAttCaller.rc()))
        {
            atts.clear();
            return autoAttCaller.rc();
        }
        AutoReadLock attLock(pAtt COMMA_LOCKVAL_SRC_POS);

        if (pAtt->getControllerName() == aName)
            atts.push_back(pAtt);
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

    /// @todo need to handle primary group change, too

    /* attempt to rename the settings file if machine name is changed */
    if (    mUserData->s.fNameSync
         && mUserData.isBackedUp()
         && (   mUserData.backedUpData()->s.strName != mUserData->s.strName
             || mUserData.backedUpData()->s.llGroups.front() != mUserData->s.llGroups.front())
       )
    {
        bool dirRenamed = false;
        bool fileRenamed = false;

        Utf8Str configFile, newConfigFile;
        Utf8Str configFilePrev, newConfigFilePrev;
        Utf8Str configDir, newConfigDir;

        do
        {
            int vrc = VINF_SUCCESS;

            Utf8Str name = mUserData.backedUpData()->s.strName;
            Utf8Str newName = mUserData->s.strName;
            Utf8Str group = mUserData.backedUpData()->s.llGroups.front();
            if (group == "/")
                group.setNull();
            Utf8Str newGroup = mUserData->s.llGroups.front();
            if (newGroup == "/")
                newGroup.setNull();

            configFile = mData->m_strConfigFileFull;

            /* first, rename the directory if it matches the group and machine name */
            Utf8Str groupPlusName = Utf8StrFmt("%s%c%s",
                group.c_str(), RTPATH_DELIMITER, name.c_str());
            Utf8Str newGroupPlusName = Utf8StrFmt("%s%c%s",
                newGroup.c_str(), RTPATH_DELIMITER, newName.c_str());
            configDir = configFile;
            configDir.stripFilename();
            newConfigDir = configDir;
            if (   configDir.length() >= groupPlusName.length()
                && !RTPathCompare(configDir.substr(configDir.length() - groupPlusName.length(), groupPlusName.length()).c_str(), groupPlusName.c_str()))
            {
                newConfigDir = newConfigDir.substr(0, configDir.length() - groupPlusName.length());
                Utf8Str newConfigBaseDir(newConfigDir);
                newConfigDir.append(newGroupPlusName);
                /* consistency: use \ if appropriate on the platform */
                RTPathChangeToDosSlashes(newConfigDir.mutableRaw(), false);
                /* new dir and old dir cannot be equal here because of 'if'
                 * above and because name != newName */
                Assert(configDir != newConfigDir);
                if (!fSettingsFileIsNew)
                {
                    /* perform real rename only if the machine is not new */
                    vrc = RTPathRename(configDir.c_str(), newConfigDir.c_str(), 0);
                    if (   vrc == VERR_FILE_NOT_FOUND
                        || vrc == VERR_PATH_NOT_FOUND)
                    {
                        /* create the parent directory, then retry renaming */
                        Utf8Str parent(newConfigDir);
                        parent.stripFilename();
                        (void)RTDirCreateFullPath(parent.c_str(), 0700);
                        vrc = RTPathRename(configDir.c_str(), newConfigDir.c_str(), 0);
                    }
                    if (RT_FAILURE(vrc))
                    {
                        rc = setError(E_FAIL,
                                      tr("Could not rename the directory '%s' to '%s' to save the settings file (%Rrc)"),
                                      configDir.c_str(),
                                      newConfigDir.c_str(),
                                      vrc);
                        break;
                    }
                    /* delete subdirectories which are no longer needed */
                    Utf8Str dir(configDir);
                    dir.stripFilename();
                    while (dir != newConfigBaseDir && dir != ".")
                    {
                        vrc = RTDirRemove(dir.c_str());
                        if (RT_FAILURE(vrc))
                            break;
                        dir.stripFilename();
                    }
                    dirRenamed = true;
                }
            }

            newConfigFile = Utf8StrFmt("%s%c%s.vbox",
                newConfigDir.c_str(), RTPATH_DELIMITER, newName.c_str());

            /* then try to rename the settings file itself */
            if (newConfigFile != configFile)
            {
                /* get the path to old settings file in renamed directory */
                configFile = Utf8StrFmt("%s%c%s",
                                        newConfigDir.c_str(),
                                        RTPATH_DELIMITER,
                                        RTPathFilename(configFile.c_str()));
                if (!fSettingsFileIsNew)
                {
                    /* perform real rename only if the machine is not new */
                    vrc = RTFileRename(configFile.c_str(), newConfigFile.c_str(), 0);
                    if (RT_FAILURE(vrc))
                    {
                        rc = setError(E_FAIL,
                                      tr("Could not rename the settings file '%s' to '%s' (%Rrc)"),
                                      configFile.c_str(),
                                      newConfigFile.c_str(),
                                      vrc);
                        break;
                    }
                    fileRenamed = true;
                    configFilePrev = configFile;
                    configFilePrev += "-prev";
                    newConfigFilePrev = newConfigFile;
                    newConfigFilePrev += "-prev";
                    RTFileRename(configFilePrev.c_str(), newConfigFilePrev.c_str(), 0);
                }
            }

            // update m_strConfigFileFull amd mConfigFile
            mData->m_strConfigFileFull = newConfigFile;
            // compute the relative path too
            mParent->copyPathRelativeToConfig(newConfigFile, mData->m_strConfigFile);

            // store the old and new so that VirtualBox::saveSettings() can update
            // the media registry
            if (    mData->mRegistered
                 && configDir != newConfigDir)
            {
                mParent->rememberMachineNameChangeForMedia(configDir, newConfigDir);

                if (pfNeedsGlobalSaveSettings)
                    *pfNeedsGlobalSaveSettings = true;
            }

            // in the saved state file path, replace the old directory with the new directory
            if (RTPathStartsWith(mSSData->strStateFilePath.c_str(), configDir.c_str()))
                mSSData->strStateFilePath = newConfigDir.append(mSSData->strStateFilePath.c_str() + configDir.length());

            // and do the same thing for the saved state file paths of all the online snapshots
            if (mData->mFirstSnapshot)
                mData->mFirstSnapshot->updateSavedStatePaths(configDir.c_str(),
                                                             newConfigDir.c_str());
        }
        while (0);

        if (FAILED(rc))
        {
            /* silently try to rename everything back */
            if (fileRenamed)
            {
                RTFileRename(newConfigFilePrev.c_str(), configFilePrev.c_str(), 0);
                RTFileRename(newConfigFile.c_str(), configFile.c_str(), 0);
            }
            if (dirRenamed)
                RTPathRename(newConfigDir.c_str(), configDir.c_str(), 0);
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
            vrc = RTDirCreateFullPath(path.c_str(), 0700);
            if (RT_FAILURE(vrc))
            {
                return setError(E_FAIL,
                                tr("Could not create a directory '%s' to save the settings file (%Rrc)"),
                                path.c_str(),
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
                            path.c_str(),
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

    AssertReturn(!isSnapshotMachine(),
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
                bool fAnySettingsChanged = !(*pNewConfig == *pOldConfig);

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
        rc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    if (fNeedsWrite || (aFlags & SaveS_InformCallbacksAnyway))
    {
        /* Fire the data change event, even on failure (since we've already
         * committed all data). This is done only for SessionMachines because
         * mutable Machine instances are always not registered (i.e. private
         * to the client process that creates them) and thus don't need to
         * inform callbacks. */
        if (isSessionMachine())
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

    // copy name, description, OS type, teleport, UTC etc.
    config.machineUserData = mUserData->s;

    if (    mData->mMachineState == MachineState_Saved
         || mData->mMachineState == MachineState_Restoring
            // when deleting a snapshot we may or may not have a saved state in the current state,
            // so let's not assert here please
         || (    (   mData->mMachineState == MachineState_DeletingSnapshot
                  || mData->mMachineState == MachineState_DeletingSnapshotOnline
                  || mData->mMachineState == MachineState_DeletingSnapshotPaused)
              && (!mSSData->strStateFilePath.isEmpty())
            )
        )
    {
        Assert(!mSSData->strStateFilePath.isEmpty());
        /* try to make the file name relative to the settings file dir */
        copyPathRelativeToMachine(mSSData->strStateFilePath, config.strStateFile);
    }
    else
    {
        Assert(mSSData->strStateFilePath.isEmpty() || mData->mMachineState == MachineState_Saving);
        config.strStateFile.setNull();
    }

    if (mData->mCurrentSnapshot)
        config.uuidCurrentSnapshot = mData->mCurrentSnapshot->getId();
    else
        config.uuidCurrentSnapshot.clear();

    config.timeLastStateChange = mData->mLastStateChange;
    config.fAborted = (mData->mMachineState == MachineState_Aborted);
    /// @todo Live Migration:        config.fTeleported = (mData->mMachineState == MachineState_Teleported);

    HRESULT rc = saveHardware(config.hardwareMachine, &config.debugging, &config.autostart);
    if (FAILED(rc)) throw rc;

    rc = saveStorageControllers(config.storageMachine);
    if (FAILED(rc)) throw rc;

    // save machine's media registry if this is VirtualBox 4.0 or later
    if (config.canHaveOwnMediaRegistry())
    {
        // determine machine folder
        Utf8Str strMachineFolder = getSettingsFileFull();
        strMachineFolder.stripFilename();
        mParent->saveMediaRegistry(config.mediaRegistry,
                                   getId(),             // only media with registry ID == machine UUID
                                   strMachineFolder);
            // this throws HRESULT
    }

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
        rc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    return rc;
}

/**
 *  Saves the VM hardware configuration. It is assumed that the
 *  given node is empty.
 *
 *  @param data           Reference to the settings object for the hardware config.
 *  @param pDbg           Pointer to the settings object for the debugging config
 *                        which happens to live in mHWData.
 *  @param pAutostart     Pointer to the settings object for the autostart config
 *                        which happens to live in mHWData.
 */
HRESULT Machine::saveHardware(settings::Hardware &data, settings::Debugging *pDbg,
                              settings::Autostart *pAutostart)
{
    HRESULT rc = S_OK;

    try
    {
        /* The hardware version attribute (optional).
            Automatically upgrade from 1 to 2 when there is no saved state. (ugly!) */
        if (    mHWData->mHWVersion == "1"
             && mSSData->strStateFilePath.isEmpty()
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
        data.fHardwareVirtForce     = !!mHWData->mHWVirtExForceEnabled;
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

        data.cCPUs             = mHWData->mCPUCount;
        data.fCpuHotPlug       = !!mHWData->mCPUHotPlugEnabled;
        data.ulCpuExecutionCap = mHWData->mCpuExecutionCap;

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
        data.fPageFusionEnabled = !!mHWData->mPageFusionEnabled;

        // firmware
        data.firmwareType = mHWData->mFirmwareType;

        // HID
        data.pointingHIDType = mHWData->mPointingHIDType;
        data.keyboardHIDType = mHWData->mKeyboardHIDType;

        // chipset
        data.chipsetType = mHWData->mChipsetType;

        data.fEmulatedUSBCardReader = !!mHWData->mEmulatedUSBCardReaderEnabled;

        // HPET
        data.fHPETEnabled = !!mHWData->mHPETEnabled;

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
        data.ulVideoCaptureHorzRes = mHWData->mVideoCaptureWidth;
        data.ulVideoCaptureVertRes = mHWData->mVideoCaptureHeight;
        data.fVideoCaptureEnabled  = !! mHWData->mVideoCaptureEnabled;
        data.strVideoCaptureFile = mHWData->mVideoCaptureFile;

        /* VRDEServer settings (optional) */
        rc = mVRDEServer->saveSettings(data.vrdeSettings);
        if (FAILED(rc)) throw rc;

        /* BIOS (required) */
        rc = mBIOSSettings->saveSettings(data.biosSettings);
        if (FAILED(rc)) throw rc;

        /* USB Controller (required) */
        rc = mUSBController->saveSettings(data.usbController);
        if (FAILED(rc)) throw rc;

        /* Network adapters (required) */
        uint32_t uMaxNICs = RT_MIN(Global::getMaxNetworkAdapters(mHWData->mChipsetType), mNetworkAdapters.size());
        data.llNetworkAdapters.clear();
        /* Write out only the nominal number of network adapters for this
         * chipset type. Since Machine::commit() hasn't been called there
         * may be extra NIC settings in the vector. */
        for (ULONG slot = 0; slot < uMaxNICs; ++slot)
        {
            settings::NetworkAdapter nic;
            nic.ulSlot = slot;
            /* paranoia check... must not be NULL, but must not crash either. */
            if (mNetworkAdapters[slot])
            {
                rc = mNetworkAdapters[slot]->saveSettings(nic);
                if (FAILED(rc)) throw rc;

                data.llNetworkAdapters.push_back(nic);
            }
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
            SharedFolder *pSF = *it;
            AutoCaller sfCaller(pSF);
            AutoReadLock sfLock(pSF COMMA_LOCKVAL_SRC_POS);
            settings::SharedFolder sf;
            sf.strName = pSF->getName();
            sf.strHostPath = pSF->getHostPath();
            sf.fWritable = !!pSF->isWritable();
            sf.fAutoMount = !!pSF->isAutoMounted();

            data.llSharedFolders.push_back(sf);
        }

        // clipboard
        data.clipboardMode = mHWData->mClipboardMode;

        // drag'n'drop
        data.dragAndDropMode = mHWData->mDragAndDropMode;

        /* Guest */
        data.ulMemoryBalloonSize = mHWData->mMemoryBalloonSize;

        // IO settings
        data.ioSettings.fIOCacheEnabled = !!mHWData->mIOCacheEnabled;
        data.ioSettings.ulIOCacheSize = mHWData->mIOCacheSize;

        /* BandwidthControl (required) */
        rc = mBandwidthControl->saveSettings(data.ioSettings);
        if (FAILED(rc)) throw rc;

        /* Host PCI devices */
        for (HWData::PCIDeviceAssignmentList::const_iterator it = mHWData->mPCIDeviceAssignments.begin();
             it != mHWData->mPCIDeviceAssignments.end();
             ++it)
        {
            ComObjPtr<PCIDeviceAttachment> pda = *it;
            settings::HostPCIDeviceAttachment hpda;

            rc = pda->saveSettings(hpda);
            if (FAILED(rc)) throw rc;

            data.pciAttachments.push_back(hpda);
        }


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
                && (   property.mFlags & guestProp::TRANSIENT
                    || property.mFlags & guestProp::TRANSRESET))
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

        *pDbg = mHWData->mDebugging;
        *pAutostart = mHWData->mAutostart;
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
 *  @param aNode    <StorageControllers> node to save the VM hardware configuration to.
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
        ctl.fBootable = pCtl->getBootable();

        /* Save the port count. */
        ULONG portCount;
        rc = pCtl->COMGETTER(PortCount)(&portCount);
        ComAssertComRCRet(rc, rc);
        ctl.ulPortCount = portCount;

        /* Save fUseHostIOCache */
        BOOL fUseHostIOCache;
        rc = pCtl->COMGETTER(UseHostIOCache)(&fUseHostIOCache);
        ComAssertComRCRet(rc, rc);
        ctl.fUseHostIOCache = !!fUseHostIOCache;

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
 *  Saves the hard disk configuration.
 */
HRESULT Machine::saveStorageDevices(ComObjPtr<StorageController> aStorageController,
                                    settings::StorageController &data)
{
    MediaData::AttachmentList atts;

    HRESULT rc = getMediumAttachmentsOfController(Bstr(aStorageController->getName()).raw(), atts);
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
            if (pMedium->isHostDrive())
                dev.strHostDriveSrc = pMedium->getLocationFull();
            else
                dev.uuid = pMedium->getId();
            dev.fPassThrough = pAttach->getPassthrough();
            dev.fTempEject = pAttach->getTempEject();
            dev.fDiscard = pAttach->getDiscard();
        }

        dev.strBwGroup = pAttach->getBandwidthGroup();

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
            if (!mSSData->strStateFilePath.isEmpty())
                /* try to make the file name relative to the settings file dir */
                copyPathRelativeToMachine(mSSData->strStateFilePath, mData->pMachineConfigFile->strStateFile);
            else
                mData->pMachineConfigFile->strStateFile.setNull();
        }

        if (aFlags & SaveSTS_StateTimeStamp)
        {
            Assert(    mData->mMachineState != MachineState_Aborted
                    || mSSData->strStateFilePath.isEmpty());

            mData->pMachineConfigFile->timeLastStateChange = mData->mLastStateChange;

            mData->pMachineConfigFile->fAborted = (mData->mMachineState == MachineState_Aborted);
//@todo live migration             mData->pMachineConfigFile->fTeleported = (mData->mMachineState == MachineState_Teleported);
        }

        mData->pMachineConfigFile->write(mData->m_strConfigFileFull);
    }
    catch (...)
    {
        rc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    return rc;
}

/**
 * Ensures that the given medium is added to a media registry. If this machine
 * was created with 4.0 or later, then the machine registry is used. Otherwise
 * the global VirtualBox media registry is used.
 *
 * Caller must NOT hold machine lock, media tree or any medium locks!
 *
 * @param pMedium
 */
void Machine::addMediumToRegistry(ComObjPtr<Medium> &pMedium)
{
    /* Paranoia checks: do not hold machine or media tree locks. */
    AssertReturnVoid(!isWriteLockOnCurrentThread());
    AssertReturnVoid(!mParent->getMediaTreeLockHandle().isWriteLockOnCurrentThread());

    ComObjPtr<Medium> pBase;
    {
        AutoReadLock treeLock(&mParent->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);
        pBase = pMedium->getBase();
    }

    /* Paranoia checks: do not hold medium locks. */
    AssertReturnVoid(!pMedium->isWriteLockOnCurrentThread());
    AssertReturnVoid(!pBase->isWriteLockOnCurrentThread());

    // decide which medium registry to use now that the medium is attached:
    Guid uuid;
    if (mData->pMachineConfigFile->canHaveOwnMediaRegistry())
        // machine XML is VirtualBox 4.0 or higher:
        uuid = getId();     // machine UUID
    else
        uuid = mParent->getGlobalRegistryId(); // VirtualBox global registry UUID

    if (pMedium->addRegistry(uuid, false /* fRecurse */))
        mParent->markRegistryModified(uuid);

    /* For more complex hard disk structures it can happen that the base
     * medium isn't yet associated with any medium registry. Do that now. */
    if (pMedium != pBase)
    {
        if (pBase->addRegistry(uuid, true /* fRecurse */))
            mParent->markRegistryModified(uuid);
    }
}

/**
 * Creates differencing hard disks for all normal hard disks attached to this
 * machine and a new set of attachments to refer to created disks.
 *
 * Used when taking a snapshot or when deleting the current state. Gets called
 * from SessionMachine::BeginTakingSnapshot() and SessionMachine::restoreSnapshotHandler().
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
HRESULT Machine::createImplicitDiffs(IProgress *aProgress,
                                     ULONG aWeight,
                                     bool aOnline)
{
    LogFlowThisFunc(("aOnline=%d\n", aOnline));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoMultiWriteLock2 alock(this->lockHandle(),
                              &mParent->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    /* must be in a protective state because we release the lock below */
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
                    alock.release();
                    rc = pMedium->createMediumLockList(true /* fFailIfInaccessible */,
                                                       false /* fMediumLockWrite */,
                                                       NULL,
                                                       *pMediumLockList);
                    alock.acquire();
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
            alock.release();
            rc = lockedMediaMap->Lock();
            alock.acquire();
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
                        aProgress->SetNextOperation(Bstr(tr("Skipping attachment without medium")).raw(),
                                                    aWeight);        // weight
                    else
                        aProgress->SetNextOperation(BstrFmt(tr("Skipping medium '%s'"),
                                                            pMedium->getBase()->getName().c_str()).raw(),
                                                    aWeight);        // weight
                }

                mMediaData->mAttachments.push_back(pAtt);
                continue;
            }

            /* need a diff */
            aProgress->SetNextOperation(BstrFmt(tr("Creating differencing hard disk for '%s'"),
                                                pMedium->getBase()->getName().c_str()).raw(),
                                        aWeight);        // weight

            Utf8Str strFullSnapshotFolder;
            calculateFullPath(mUserData->s.strSnapshotFolder, strFullSnapshotFolder);

            ComObjPtr<Medium> diff;
            diff.createObject();
            // store the diff in the same registry as the parent
            // (this cannot fail here because we can't create implicit diffs for
            // unregistered images)
            Guid uuidRegistryParent;
            bool fInRegistry = pMedium->getFirstRegistryMachineId(uuidRegistryParent);
            Assert(fInRegistry); NOREF(fInRegistry);
            rc = diff->init(mParent,
                            pMedium->getPreferredDiffFormat(),
                            strFullSnapshotFolder.append(RTPATH_SLASH_STR),
                            uuidRegistryParent);
            if (FAILED(rc)) throw rc;

            /** @todo r=bird: How is the locking and diff image cleaned up if we fail before
             *        the push_back?  Looks like we're going to release medium with the
             *        wrong kind of lock (general issue with if we fail anywhere at all)
             *        and an orphaned VDI in the snapshots folder. */

            /* update the appropriate lock list */
            MediumLockList *pMediumLockList;
            rc = lockedMediaMap->Get(pAtt, pMediumLockList);
            AssertComRCThrowRC(rc);
            if (aOnline)
            {
                alock.release();
                rc = pMediumLockList->Update(pMedium, false);
                alock.acquire();
                AssertComRCThrowRC(rc);
            }

            /* release the locks before the potentially lengthy operation */
            alock.release();
            rc = pMedium->createDiffStorage(diff, MediumVariant_Standard,
                                            pMediumLockList,
                                            NULL /* aProgress */,
                                            true /* aWait */);
            alock.acquire();
            if (FAILED(rc)) throw rc;

            rc = lockedMediaMap->Unlock();
            AssertComRCThrowRC(rc);
            alock.release();
            rc = pMediumLockList->Append(diff, true);
            alock.acquire();
            AssertComRCThrowRC(rc);
            alock.release();
            rc = lockedMediaMap->Lock();
            alock.acquire();
            AssertComRCThrowRC(rc);

            rc = diff->addBackReference(mData->mUuid);
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
                                  true /* aImplicit */,
                                  false /* aPassthrough */,
                                  false /* aTempEject */,
                                  pAtt->getNonRotational(),
                                  pAtt->getDiscard(),
                                  pAtt->getBandwidthGroup());
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

        HRESULT rc1 = lockedMediaMap->Clear();
        AssertComRC(rc1);
    }

    if (FAILED(rc))
    {
        MultiResult mrc = rc;

        alock.release();
        mrc = deleteImplicitDiffs();
    }

    return rc;
}

/**
 * Deletes implicit differencing hard disks created either by
 * #createImplicitDiffs() or by #AttachDevice() and rolls back mMediaData.
 *
 * Note that to delete hard disks created by #AttachDevice() this method is
 * called from #fixupMedia() when the changes are rolled back.
 *
 * @note Locks this object for writing.
 */
HRESULT Machine::deleteImplicitDiffs()
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
            rc = hd->removeBackReference(mData->mUuid);
            AssertComRC(rc);
            implicitAtts.push_back(*it);
            continue;
        }

        /* was this hard disk attached before? */
        if (!findAttachment(oldAtts, hd))
        {
            /* no: de-associate */
            LogFlowThisFunc(("Detaching '%s', no deletion\n", (*it)->getLogName()));
            rc = hd->removeBackReference(mData->mUuid);
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
        /* will release the lock before the potentially lengthy
         * operation, so protect with the special state (unless already
         * protected) */
        MachineState_T oldState = mData->mMachineState;
        if (    oldState != MachineState_Saving
             && oldState != MachineState_LiveSnapshotting
             && oldState != MachineState_RestoringSnapshot
             && oldState != MachineState_DeletingSnapshot
             && oldState != MachineState_DeletingSnapshotOnline
             && oldState != MachineState_DeletingSnapshotPaused
           )
            setMachineState(MachineState_SettingUp);

        alock.release();

        for (MediaData::AttachmentList::const_iterator it = implicitAtts.begin();
             it != implicitAtts.end();
             ++it)
        {
            LogFlowThisFunc(("Deleting '%s'\n", (*it)->getLogName()));
            ComObjPtr<Medium> hd = (*it)->getMedium();

            rc = hd->deleteStorage(NULL /*aProgress*/, true /*aWait*/);
            AssertMsg(SUCCEEDED(rc), ("rc=%Rhrc it=%s hd=%s\n", rc, (*it)->getLogName(), hd->getLocationFull().c_str() ));
            mrc = rc;
        }

        alock.acquire();

        if (mData->mMachineState == MachineState_SettingUp)
            setMachineState(oldState);
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
        if (pMediumThis == pMedium)
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
 * Main implementation for Machine::DetachDevice. This also gets called
 * from Machine::prepareUnregister() so it has been taken out for simplicity.
 *
 * @param pAttach Medium attachment to detach.
 * @param writeLock Machine write lock which the caller must have locked once. This may be released temporarily in here.
 * @param pSnapshot If NULL, then the detachment is for the current machine. Otherwise this is for a SnapshotMachine, and this must be its snapshot.
 * @return
 */
HRESULT Machine::detachDevice(MediumAttachment *pAttach,
                              AutoWriteLock &writeLock,
                              Snapshot *pSnapshot)
{
    ComObjPtr<Medium> oldmedium = pAttach->getMedium();
    DeviceType_T mediumType = pAttach->getType();

    LogFlowThisFunc(("Entering, medium of attachment is %s\n", oldmedium ? oldmedium->getLocationFull().c_str() : "NULL"));

    if (pAttach->isImplicit())
    {
        /* attempt to implicitly delete the implicitly created diff */

        /// @todo move the implicit flag from MediumAttachment to Medium
        /// and forbid any hard disk operation when it is implicit. Or maybe
        /// a special media state for it to make it even more simple.

        Assert(mMediaData.isBackedUp());

        /* will release the lock before the potentially lengthy operation, so
         * protect with the special state */
        MachineState_T oldState = mData->mMachineState;
        setMachineState(MachineState_SettingUp);

        writeLock.release();

        HRESULT rc = oldmedium->deleteStorage(NULL /*aProgress*/,
                                              true /*aWait*/);

        writeLock.acquire();

        setMachineState(oldState);

        if (FAILED(rc)) return rc;
    }

    setModified(IsModified_Storage);
    mMediaData.backup();
    mMediaData->mAttachments.remove(pAttach);

    if (!oldmedium.isNull())
    {
        // if this is from a snapshot, do not defer detachment to commitMedia()
        if (pSnapshot)
            oldmedium->removeBackReference(mData->mUuid, pSnapshot->getId());
        // else if non-hard disk media, do not defer detachment to commitMedia() either
        else if (mediumType != DeviceType_HardDisk)
            oldmedium->removeBackReference(mData->mUuid);
    }

    return S_OK;
}

/**
 * Goes thru all media of the given list and
 *
 * 1) calls detachDevice() on each of them for this machine and
 * 2) adds all Medium objects found in the process to the given list,
 *    depending on cleanupMode.
 *
 * If cleanupMode is CleanupMode_DetachAllReturnHardDisksOnly, this only
 * adds hard disks to the list. If it is CleanupMode_Full, this adds all
 * media to the list.
 *
 * This gets called from Machine::Unregister, both for the actual Machine and
 * the SnapshotMachine objects that might be found in the snapshots.
 *
 * Requires caller and locking. The machine lock must be passed in because it
 * will be passed on to detachDevice which needs it for temporary unlocking.
 *
 * @param writeLock Machine lock from top-level caller; this gets passed to detachDevice.
 * @param pSnapshot Must be NULL when called for a "real" Machine or a snapshot object if called for a SnapshotMachine.
 * @param cleanupMode If DetachAllReturnHardDisksOnly, only hard disk media get added to llMedia; if Full, then all media get added;
 *          otherwise no media get added.
 * @param llMedia Caller's list to receive Medium objects which got detached so caller can close() them, depending on cleanupMode.
 * @return
 */
HRESULT Machine::detachAllMedia(AutoWriteLock &writeLock,
                                Snapshot *pSnapshot,
                                CleanupMode_T cleanupMode,
                                MediaList &llMedia)
{
    Assert(isWriteLockOnCurrentThread());

    HRESULT rc;

    // make a temporary list because detachDevice invalidates iterators into
    // mMediaData->mAttachments
    MediaData::AttachmentList llAttachments2 = mMediaData->mAttachments;

    for (MediaData::AttachmentList::iterator it = llAttachments2.begin();
         it != llAttachments2.end();
         ++it)
    {
        ComObjPtr<MediumAttachment> &pAttach = *it;
        ComObjPtr<Medium> pMedium = pAttach->getMedium();

        if (!pMedium.isNull())
        {
            AutoCaller mac(pMedium);
            if (FAILED(mac.rc())) return mac.rc();
            AutoReadLock lock(pMedium COMMA_LOCKVAL_SRC_POS);
            DeviceType_T devType = pMedium->getDeviceType();
            if (    (    cleanupMode == CleanupMode_DetachAllReturnHardDisksOnly
                      && devType == DeviceType_HardDisk)
                 || (cleanupMode == CleanupMode_Full)
               )
            {
                llMedia.push_back(pMedium);
                ComObjPtr<Medium> pParent = pMedium->getParent();
                /*
                 * Search for medias which are not attached to any machine, but
                 * in the chain to an attached disk. Mediums are only consided
                 * if they are:
                 * - have only one child
                 * - no references to any machines
                 * - are of normal medium type
                 */
                while (!pParent.isNull())
                {
                    AutoCaller mac1(pParent);
                    if (FAILED(mac1.rc())) return mac1.rc();
                    AutoReadLock lock1(pParent COMMA_LOCKVAL_SRC_POS);
                    if (pParent->getChildren().size() == 1)
                    {
                        if (   pParent->getMachineBackRefCount() == 0
                            && pParent->getType() == MediumType_Normal
                            && find(llMedia.begin(), llMedia.end(), pParent) == llMedia.end())
                            llMedia.push_back(pParent);
                    }
                    else
                        break;
                    pParent = pParent->getParent();
                }
            }
        }

        // real machine: then we need to use the proper method
        rc = detachDevice(pAttach, writeLock, pSnapshot);

        if (FAILED(rc))
            return rc;
    }

    return S_OK;
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
                         (pMedium) ? pMedium->getName().c_str() : "NULL",
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
                if (pOldAttach->getMedium() == pMedium)
                {
                    LogFlowThisFunc(("--> medium '%s' was attached before, will not remove\n", pMedium->getName().c_str()));

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
            LogFlowThisFunc(("detaching medium '%s' from machine\n", pMedium->getName().c_str()));

            /* now de-associate from the current machine state */
            rc = pMedium->removeBackReference(mData->mUuid);
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

    if (isSessionMachine())
    {
        /*
         * Update the parent machine to point to the new owner.
         * This is necessary because the stored parent will point to the
         * session machine otherwise and cause crashes or errors later
         * when the session machine gets invalid.
         */
        /** @todo Change the MediumAttachment class to behave like any other
         *        class in this regard by creating peer MediumAttachment
         *        objects for session machines and share the data with the peer
         *        machine.
         */
        for (MediaData::AttachmentList::const_iterator it = mMediaData->mAttachments.begin();
             it != mMediaData->mAttachments.end();
             ++it)
        {
            (*it)->updateParentMachine(mPeer);
        }

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
                rc = pMedium->removeBackReference(mData->mUuid);
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
                rc = pMedium->addBackReference(mData->mUuid);
                AssertComRC(rc);
            }
        }
    }

    /** @todo convert all this Machine-based voodoo to MediumAttachment
     * based rollback logic. */
    deleteImplicitDiffs();

    return;
}

/**
 *  Returns true if the settings file is located in the directory named exactly
 *  as the machine; this means, among other things, that the machine directory
 *  should be auto-renamed.
 *
 *  @param aSettingsDir if not NULL, the full machine settings file directory
 *                      name will be assigned there.
 *
 *  @note Doesn't lock anything.
 *  @note Not thread safe (must be called from this object's lock).
 */
bool Machine::isInOwnDir(Utf8Str *aSettingsDir /* = NULL */) const
{
    Utf8Str strMachineDirName(mData->m_strConfigFileFull);  // path/to/machinesfolder/vmname/vmname.vbox
    strMachineDirName.stripFilename();                      // path/to/machinesfolder/vmname
    if (aSettingsDir)
        *aSettingsDir = strMachineDirName;
    strMachineDirName.stripPath();                          // vmname
    Utf8Str strConfigFileOnly(mData->m_strConfigFileFull);  // path/to/machinesfolder/vmname/vmname.vbox
    strConfigFileOnly.stripPath()                           // vmname.vbox
                     .stripExt();                           // vmname

    AssertReturn(!strMachineDirName.isEmpty(), false);
    AssertReturn(!strConfigFileOnly.isEmpty(), false);

    return strMachineDirName == strConfigFileOnly;
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

    if (mVRDEServer && (mData->flModifications & IsModified_VRDEServer))
        mVRDEServer->rollback();

    if (mAudioAdapter)
        mAudioAdapter->rollback();

    if (mUSBController && (mData->flModifications & IsModified_USB))
        mUSBController->rollback();

    if (mBandwidthControl && (mData->flModifications & IsModified_BandwidthControl))
        mBandwidthControl->rollback();

    if (!mHWData.isNull())
        mNetworkAdapters.resize(Global::getMaxNetworkAdapters(mHWData->mChipsetType));
    NetworkAdapterVector networkAdapters(mNetworkAdapters.size());
    ComPtr<ISerialPort> serialPorts[RT_ELEMENTS(mSerialPorts)];
    ComPtr<IParallelPort> parallelPorts[RT_ELEMENTS(mParallelPorts)];

    if (mData->flModifications & IsModified_NetworkAdapters)
        for (ULONG slot = 0; slot < mNetworkAdapters.size(); slot++)
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
        alock.release();

        if (flModifications & IsModified_SharedFolders)
            that->onSharedFolderChange();

        if (flModifications & IsModified_VRDEServer)
            that->onVRDEServerChange(/* aRestart */ TRUE);
        if (flModifications & IsModified_USB)
            that->onUSBControllerChange();

        for (ULONG slot = 0; slot < networkAdapters.size(); slot++)
            if (networkAdapters[slot])
                that->onNetworkAdapterChange(networkAdapters[slot], FALSE);
        for (ULONG slot = 0; slot < RT_ELEMENTS(serialPorts); slot++)
            if (serialPorts[slot])
                that->onSerialPortChange(serialPorts[slot]);
        for (ULONG slot = 0; slot < RT_ELEMENTS(parallelPorts); slot++)
            if (parallelPorts[slot])
                that->onParallelPortChange(parallelPorts[slot]);

        if (flModifications & IsModified_Storage)
            that->onStorageControllerChange();

#if 0
        if (flModifications & IsModified_BandwidthControl)
            that->onBandwidthControlChange();
#endif
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
    mVRDEServer->commit();
    mAudioAdapter->commit();
    mUSBController->commit();
    mBandwidthControl->commit();

    /* Since mNetworkAdapters is a list which might have been changed (resized)
     * without using the Backupable<> template we need to handle the copying
     * of the list entries manually, including the creation of peers for the
     * new objects. */
    bool commitNetworkAdapters = false;
    size_t newSize = Global::getMaxNetworkAdapters(mHWData->mChipsetType);
    if (mPeer)
    {
        /* commit everything, even the ones which will go away */
        for (size_t slot = 0; slot < mNetworkAdapters.size(); slot++)
            mNetworkAdapters[slot]->commit();
        /* copy over the new entries, creating a peer and uninit the original */
        mPeer->mNetworkAdapters.resize(RT_MAX(newSize, mPeer->mNetworkAdapters.size()));
        for (size_t slot = 0; slot < newSize; slot++)
        {
            /* look if this adapter has a peer device */
            ComObjPtr<NetworkAdapter> peer = mNetworkAdapters[slot]->getPeer();
            if (!peer)
            {
                /* no peer means the adapter is a newly created one;
                 * create a peer owning data this data share it with */
                peer.createObject();
                peer->init(mPeer, mNetworkAdapters[slot], true /* aReshare */);
            }
            mPeer->mNetworkAdapters[slot] = peer;
        }
        /* uninit any no longer needed network adapters */
        for (size_t slot = newSize; slot < mNetworkAdapters.size(); slot++)
            mNetworkAdapters[slot]->uninit();
        for (size_t slot = newSize; slot < mPeer->mNetworkAdapters.size(); slot++)
        {
            if (mPeer->mNetworkAdapters[slot])
                mPeer->mNetworkAdapters[slot]->uninit();
        }
        /* Keep the original network adapter count until this point, so that
         * discarding a chipset type change will not lose settings. */
        mNetworkAdapters.resize(newSize);
        mPeer->mNetworkAdapters.resize(newSize);
    }
    else
    {
        /* we have no peer (our parent is the newly created machine);
         * just commit changes to the network adapters */
        commitNetworkAdapters = true;
    }
    if (commitNetworkAdapters)
    {
        for (size_t slot = 0; slot < mNetworkAdapters.size(); slot++)
            mNetworkAdapters[slot]->commit();
    }

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
            /* Commit all changes to new controllers (this will reshare data with
             * peers for those who have peers) */
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

    if (isSessionMachine())
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
    AssertReturnVoid(!isSnapshotMachine());
    AssertReturnVoid(aThat->isSnapshotMachine());

    AssertReturnVoid(!Global::IsOnline(mData->mMachineState));

    mHWData.assignCopy(aThat->mHWData);

    // create copies of all shared folders (mHWData after attaching a copy
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
    mVRDEServer->copyFrom(aThat->mVRDEServer);
    mAudioAdapter->copyFrom(aThat->mAudioAdapter);
    mUSBController->copyFrom(aThat->mUSBController);
    mBandwidthControl->copyFrom(aThat->mBandwidthControl);

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

    for (ULONG slot = 0; slot < mNetworkAdapters.size(); slot++)
        mNetworkAdapters[slot]->copyFrom(aThat->mNetworkAdapters[slot]);
    for (ULONG slot = 0; slot < RT_ELEMENTS(mSerialPorts); slot++)
        mSerialPorts[slot]->copyFrom(aThat->mSerialPorts[slot]);
    for (ULONG slot = 0; slot < RT_ELEMENTS(mParallelPorts); slot++)
        mParallelPorts[slot]->copyFrom(aThat->mParallelPorts[slot]);
}

/**
 * Returns whether the given storage controller is hotplug capable.
 *
 * @returns true if the controller supports hotplugging
 *          false otherwise.
 * @param   enmCtrlType    The controller type to check for.
 */
bool Machine::isControllerHotplugCapable(StorageControllerType_T enmCtrlType)
{
    switch (enmCtrlType)
    {
        case StorageControllerType_IntelAhci:
            return true;
        case StorageControllerType_LsiLogic:
        case StorageControllerType_LsiLogicSas:
        case StorageControllerType_BusLogic:
        case StorageControllerType_PIIX3:
        case StorageControllerType_PIIX4:
        case StorageControllerType_ICH6:
        case StorageControllerType_I82078:
        default:
            return false;
    }
}

#ifdef VBOX_WITH_RESOURCE_USAGE_API

void Machine::registerMetrics(PerformanceCollector *aCollector, Machine *aMachine, RTPROCESS pid)
{
    AssertReturnVoid(isWriteLockOnCurrentThread());
    AssertPtrReturnVoid(aCollector);

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


    /* Guest metrics collector */
    mCollectorGuest = new pm::CollectorGuest(aMachine, pid);
    aCollector->registerGuest(mCollectorGuest);
    LogAleksey(("{%p} " LOG_FN_FMT ": mCollectorGuest=%p\n",
                this, __PRETTY_FUNCTION__, mCollectorGuest));

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
    pm::SubMetric *guestMemShared = new pm::SubMetric("Guest/RAM/Usage/Shared",  "Amount of shared physical guest RAM.");
    pm::SubMetric *guestMemCache = new pm::SubMetric("Guest/RAM/Usage/Cache",        "Total amount of guest (disk) cache memory.");

    pm::SubMetric *guestPagedTotal = new pm::SubMetric("Guest/Pagefile/Usage/Total",    "Total amount of space in the page file.");

    /* Create and register base metrics */
    pm::BaseMetric *guestCpuLoad = new pm::GuestCpuLoad(mCollectorGuest, aMachine,
                                                        guestLoadUser, guestLoadKernel, guestLoadIdle);
    aCollector->registerBaseMetric(guestCpuLoad);

    pm::BaseMetric *guestCpuMem = new pm::GuestRamUsage(mCollectorGuest, aMachine,
                                                        guestMemTotal, guestMemFree,
                                                        guestMemBalloon, guestMemShared,
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

    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemShared, 0));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemShared, new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemShared, new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemShared, new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemCache, 0));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemCache, new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemCache, new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestMemCache, new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestPagedTotal, 0));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestPagedTotal, new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestPagedTotal, new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(guestCpuMem, guestPagedTotal, new pm::AggregateMax()));
}

void Machine::unregisterMetrics(PerformanceCollector *aCollector, Machine *aMachine)
{
    AssertReturnVoid(isWriteLockOnCurrentThread());

    if (aCollector)
    {
        aCollector->unregisterMetricsFor(aMachine);
        aCollector->unregisterBaseMetricsFor(aMachine);
    }
}

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

    return BaseFinalConstruct();
}

void SessionMachine::FinalRelease()
{
    LogFlowThisFunc(("\n"));

    uninit(Uninit::Unexpected);

    BaseFinalRelease();
}

/**
 *  @note Must be called only by Machine::openSession() from its own write lock.
 */
HRESULT SessionMachine::init(Machine *aMachine)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("mName={%s}\n", aMachine->mUserData->s.strName.c_str()));

    AssertReturn(aMachine, E_INVALIDARG);

    AssertReturn(aMachine->lockHandle()->isWriteLockOnCurrentThread(), E_FAIL);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* create the interprocess semaphore */
#if defined(RT_OS_WINDOWS)
    mIPCSemName = aMachine->mData->m_strConfigFileFull;
    for (size_t i = 0; i < mIPCSemName.length(); i++)
        if (mIPCSemName.raw()[i] == '\\')
            mIPCSemName.raw()[i] = '/';
    mIPCSem = ::CreateMutex(NULL, FALSE, mIPCSemName.raw());
    ComAssertMsgRet(mIPCSem,
                    ("Cannot create IPC mutex '%ls', err=%d",
                     mIPCSemName.raw(), ::GetLastError()),
                    E_FAIL);
#elif defined(RT_OS_OS2)
    Utf8Str ipcSem = Utf8StrFmt("\\SEM32\\VBOX\\VM\\{%RTuuid}",
                                aMachine->mData->mUuid.raw());
    mIPCSemName = ipcSem;
    APIRET arc = ::DosCreateMutexSem((PSZ)ipcSem.c_str(), &mIPCSem, 0, FALSE);
    ComAssertMsgRet(arc == NO_ERROR,
                    ("Cannot create IPC mutex '%s', arc=%ld",
                     ipcSem.c_str(), arc),
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
                    "maximum number of semaphores (SEMMNS) would be exceeded. The "
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
    /* create another VRDEServer object that will be mutable */
    unconst(mVRDEServer).createObject();
    mVRDEServer->init(this, aMachine->mVRDEServer);
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
    mNetworkAdapters.resize(aMachine->mNetworkAdapters.size());
    for (ULONG slot = 0; slot < mNetworkAdapters.size(); slot++)
    {
        unconst(mNetworkAdapters[slot]).createObject();
        mNetworkAdapters[slot]->init(this, aMachine->mNetworkAdapters[slot]);
    }

    /* create another bandwidth control object that will be mutable */
    unconst(mBandwidthControl).createObject();
    mBandwidthControl->init(this, aMachine->mBandwidthControl);

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
         * necessary (nor it's safe) to perform the regular uninit sequence
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

#if 0
    // Trigger async cleanup tasks, avoid doing things here which are not
    // vital to be done immediately and maybe need more locks. This calls
    // Machine::unregisterMetrics().
    mParent->onMachineUninit(mPeer);
#else
    /*
     * It is safe to call Machine::unregisterMetrics() here because
     * PerformanceCollector::samplerCallback no longer accesses guest methods
     * holding the lock.
     */
    unregisterMetrics(mParent->performanceCollector(), mPeer);
#endif
    /* The guest must be unregistered after its metrics (@bugref{5949}). */
    LogAleksey(("{%p} " LOG_FN_FMT ": mCollectorGuest=%p\n",
                this, __PRETTY_FUNCTION__, mCollectorGuest));
    if (mCollectorGuest)
    {
        mParent->performanceCollector()->unregisterGuest(mCollectorGuest);
        // delete mCollectorGuest; => CollectorGuestManager::destroyUnregistered()
        mCollectorGuest = NULL;
    }

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

    Assert(    mConsoleTaskData.strStateFilePath.isEmpty()
            || !mConsoleTaskData.mSnapshot);
    if (!mConsoleTaskData.strStateFilePath.isEmpty())
    {
        LogWarningThisFunc(("canceling failed save state request!\n"));
        endSavingState(E_FAIL, tr("Machine terminated with pending save state!"));
    }
    else if (!mConsoleTaskData.mSnapshot.isNull())
    {
        LogWarningThisFunc(("canceling untaken snapshot!\n"));

        /* delete all differencing hard disks created (this will also attach
         * their parents back by rolling back mMediaData) */
        rollbackMedia();

        // delete the saved state file (it might have been already created)
        // AFTER killing the snapshot so that releaseSavedStateFile() won't
        // think it's still in use
        Utf8Str strStateFile = mConsoleTaskData.mSnapshot->getStateFilePath();
        mConsoleTaskData.mSnapshot->uninit();
        releaseSavedStateFile(strStateFile, NULL /* pSnapshotToIgnore */ );
    }

    if (!mData->mSession.mType.isEmpty())
    {
        /* mType is not null when this machine's process has been started by
         * Machine::LaunchVMProcess(), therefore it is our child.  We
         * need to queue the PID to reap the process (and avoid zombies on
         * Linux). */
        Assert(mData->mSession.mPID != NIL_RTPROCESS);
        mParent->addProcessToReap(mData->mSession.mPID);
    }

    mData->mSession.mPID = NIL_RTPROCESS;

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
     *  Otherwise it means that something's gone really wrong (for example,
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
        Assert(mData->mSession.mState == SessionState_Unlocking);
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
    Assert(   (SessionMachine*)mData->mSession.mMachine == this
            || aReason == Uninit::Unexpected);

    /* reset the rest of session data */
    mData->mSession.mMachine.setNull();
    mData->mSession.mState = SessionState_Unlocked;
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
    mParent->onSessionStateChange(mData->mUuid, SessionState_Unlocked);

    uninitDataAndChildObjects();

    /* free the essential data structure last */
    mData.free();

    /* release the exclusive lock before setting the below two to NULL */
    multilock.release();

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
 *  Passes collected guest statistics to performance collector object
 */
STDMETHODIMP SessionMachine::ReportGuestStatistics(ULONG aValidStats, ULONG aCpuUser,
                                                   ULONG aCpuKernel, ULONG aCpuIdle,
                                                   ULONG aMemTotal, ULONG aMemFree,
                                                   ULONG aMemBalloon, ULONG aMemShared,
                                                   ULONG aMemCache, ULONG aPageTotal,
                                                   ULONG aAllocVMM, ULONG aFreeVMM,
                                                   ULONG aBalloonedVMM, ULONG aSharedVMM)
{
    if (mCollectorGuest)
        mCollectorGuest->updateStats(aValidStats, aCpuUser, aCpuKernel, aCpuIdle,
                                     aMemTotal, aMemFree, aMemBalloon, aMemShared,
                                     aMemCache, aPageTotal, aAllocVMM, aFreeVMM,
                                     aBalloonedVMM, aSharedVMM);

    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::SetRemoveSavedStateFile(BOOL aRemove)
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
STDMETHODIMP SessionMachine::BeginPowerUp(IProgress *aProgress)
{
    LogFlowThisFunc(("aProgress=%p\n", aProgress));
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mSession.mState != SessionState_Locked)
        return VBOX_E_INVALID_OBJECT_STATE;

    if (!mData->mSession.mProgress.isNull())
        mData->mSession.mProgress->setOtherProgressObject(aProgress);

    LogFlowThisFunc(("returns S_OK.\n"));
    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::EndPowerUp(LONG iResult)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mSession.mState != SessionState_Locked)
        return VBOX_E_INVALID_OBJECT_STATE;

    /* Finalize the LaunchVMProcess progress object. */
    if (mData->mSession.mProgress)
    {
        mData->mSession.mProgress->notifyComplete((HRESULT)iResult);
        mData->mSession.mProgress.setNull();
    }

    if (SUCCEEDED((HRESULT)iResult))
    {
#ifdef VBOX_WITH_RESOURCE_USAGE_API
        /* The VM has been powered up successfully, so it makes sense
         * now to offer the performance metrics for a running machine
         * object. Doing it earlier wouldn't be safe. */
        registerMetrics(mParent->performanceCollector(), mPeer,
                        mData->mSession.mPID);
#endif /* VBOX_WITH_RESOURCE_USAGE_API */
    }

    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::BeginPoweringDown(IProgress **aProgress)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(mConsoleTaskData.mLastState == MachineState_Null,
                 E_FAIL);

    /* create a progress object to track operation completion */
    ComObjPtr<Progress> pProgress;
    pProgress.createObject();
    pProgress->init(getVirtualBox(),
                    static_cast<IMachine *>(this) /* aInitiator */,
                    Bstr(tr("Stopping the virtual machine")).raw(),
                    FALSE /* aCancelable */);

    /* fill in the console task data */
    mConsoleTaskData.mLastState = mData->mMachineState;
    mConsoleTaskData.mProgress = pProgress;

    /* set the state to Stopping (this is expected by Console::PowerDown()) */
    setMachineState(MachineState_Stopping);

    pProgress.queryInterfaceTo(aProgress);

    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::EndPoweringDown(LONG iResult, IN_BSTR aErrMsg)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(    (   (SUCCEEDED(iResult) && mData->mMachineState == MachineState_PoweredOff)
                      || (FAILED(iResult) && mData->mMachineState == MachineState_Stopping))
                  && mConsoleTaskData.mLastState != MachineState_Null,
                 E_FAIL);

    /*
     * On failure, set the state to the state we had when BeginPoweringDown()
     * was called (this is expected by Console::PowerDown() and the associated
     * task). On success the VM process already changed the state to
     * MachineState_PoweredOff, so no need to do anything.
     */
    if (FAILED(iResult))
        setMachineState(mConsoleTaskData.mLastState);

    /* notify the progress object about operation completion */
    Assert(mConsoleTaskData.mProgress);
    if (SUCCEEDED(iResult))
        mConsoleTaskData.mProgress->notifyComplete(S_OK);
    else
    {
        Utf8Str strErrMsg(aErrMsg);
        if (strErrMsg.length())
            mConsoleTaskData.mProgress->notifyComplete(iResult,
                                                       COM_IIDOF(ISession),
                                                       getComponentName(),
                                                       strErrMsg.c_str());
        else
            mConsoleTaskData.mProgress->notifyComplete(iResult);
    }

    /* clear out the temporary saved state data */
    mConsoleTaskData.mLastState = MachineState_Null;
    mConsoleTaskData.mProgress.setNull();

    LogFlowThisFuncLeave();
    return S_OK;
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
    clearError();
    MultiResult rc = mParent->host()->checkUSBProxyService();
    if (FAILED(rc)) return rc;

    USBProxyService *service = mParent->host()->usbProxyService();
    AssertReturn(service, E_FAIL);
    return service->captureDeviceForVM(this, Guid(aId).ref());
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
    return service->detachDeviceFromVM(this, Guid(aId).ref(), !!aDone);
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
    AutoMultiWriteLock2 alock(mParent, this COMMA_LOCKVAL_SRC_POS);

    if (control == mData->mSession.mDirectControl)
    {
        ComAssertRet(aProgress, E_POINTER);

        /* The direct session is being normally closed by the client process
         * ----------------------------------------------------------------- */

        /* go to the closing state (essential for all open*Session() calls and
         * for #checkForDeath()) */
        Assert(mData->mSession.mState == SessionState_Locked);
        mData->mSession.mState = SessionState_Unlocking;

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
         * it releases the IPC semaphore.
         * Note! Because we're "reusing" mProgress here, this must be a proxy
         *       object just like for LaunchVMProcess. */
        Assert(mData->mSession.mProgress.isNull());
        ComObjPtr<ProgressProxy> progress;
        progress.createObject();
        ComPtr<IUnknown> pPeer(mPeer);
        progress->init(mParent, pPeer,
                       Bstr(tr("Closing session")).raw(),
                       FALSE /* aCancelable */);
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
            if (control == *it)
                break;
            ++it;
        }
        BOOL found = it != mData->mSession.mRemoteControls.end();
        ComAssertMsgRet(found, ("The session is not found in the session list!"),
                         E_INVALIDARG);
        // This MUST be erase(it), not remove(*it) as the latter triggers a
        // very nasty use after free due to the place where the value "lives".
        mData->mSession.mRemoteControls.erase(it);
    }

    /* signal the client watcher thread, because the client is going away */
    mParent->updateClientWatcher();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
STDMETHODIMP SessionMachine::BeginSavingState(IProgress **aProgress, BSTR *aStateFilePath)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aProgress);
    CheckComArgOutPointerValid(aStateFilePath);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(    mData->mMachineState == MachineState_Paused
                  && mConsoleTaskData.mLastState == MachineState_Null
                  && mConsoleTaskData.strStateFilePath.isEmpty(),
                 E_FAIL);

    /* create a progress object to track operation completion */
    ComObjPtr<Progress> pProgress;
    pProgress.createObject();
    pProgress->init(getVirtualBox(),
                    static_cast<IMachine *>(this) /* aInitiator */,
                    Bstr(tr("Saving the execution state of the virtual machine")).raw(),
                    FALSE /* aCancelable */);

    Utf8Str strStateFilePath;
    /* stateFilePath is null when the machine is not running */
    if (mData->mMachineState == MachineState_Paused)
        composeSavedStateFilename(strStateFilePath);

    /* fill in the console task data */
    mConsoleTaskData.mLastState = mData->mMachineState;
    mConsoleTaskData.strStateFilePath = strStateFilePath;
    mConsoleTaskData.mProgress = pProgress;

    /* set the state to Saving (this is expected by Console::SaveState()) */
    setMachineState(MachineState_Saving);

    strStateFilePath.cloneTo(aStateFilePath);
    pProgress.queryInterfaceTo(aProgress);

    return S_OK;
}

/**
 *  @note Locks mParent + this object for writing.
 */
STDMETHODIMP SessionMachine::EndSavingState(LONG iResult, IN_BSTR aErrMsg)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    /* endSavingState() need mParent lock */
    AutoMultiWriteLock2 alock(mParent, this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(    (   (SUCCEEDED(iResult) && mData->mMachineState == MachineState_Saved)
                      || (FAILED(iResult) && mData->mMachineState == MachineState_Saving))
                  && mConsoleTaskData.mLastState != MachineState_Null
                  && !mConsoleTaskData.strStateFilePath.isEmpty(),
                 E_FAIL);

    /*
     * On failure, set the state to the state we had when BeginSavingState()
     * was called (this is expected by Console::SaveState() and the associated
     * task). On success the VM process already changed the state to
     * MachineState_Saved, so no need to do anything.
     */
    if (FAILED(iResult))
        setMachineState(mConsoleTaskData.mLastState);

    return endSavingState(iResult, aErrMsg);
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

    mSSData->strStateFilePath = stateFilePathFull;

    /* The below setMachineState() will detect the state transition and will
     * update the settings file */

    return setMachineState(MachineState_Saved);
}

STDMETHODIMP SessionMachine::PullGuestProperties(ComSafeArrayOut(BSTR, aNames),
                                                 ComSafeArrayOut(BSTR, aValues),
                                                 ComSafeArrayOut(LONG64, aTimestamps),
                                                 ComSafeArrayOut(BSTR, aFlags))
{
    LogFlowThisFunc(("\n"));

#ifdef VBOX_WITH_GUEST_PROPS
    using namespace guestProp;

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CheckComArgOutSafeArrayPointerValid(aNames);
    CheckComArgOutSafeArrayPointerValid(aValues);
    CheckComArgOutSafeArrayPointerValid(aTimestamps);
    CheckComArgOutSafeArrayPointerValid(aFlags);

    size_t cEntries = mHWData->mGuestProperties.size();
    com::SafeArray<BSTR> names(cEntries);
    com::SafeArray<BSTR> values(cEntries);
    com::SafeArray<LONG64> timestamps(cEntries);
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
                                               LONG64 aTimestamp,
                                               IN_BSTR aFlags)
{
    LogFlowThisFunc(("\n"));

#ifdef VBOX_WITH_GUEST_PROPS
    using namespace guestProp;

    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgNotNull(aValue);
    CheckComArgNotNull(aFlags);

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
            int vrc = validateFlags(utf8Flags.c_str(), &fFlags);
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
            case MachineState_DeletingSnapshotOnline:
            case MachineState_DeletingSnapshotPaused:
            case MachineState_Saving:
                break;

            default:
#ifndef DEBUG_sunlover
                AssertMsgFailedReturn(("%s\n", Global::stringifyMachineState(mData->mMachineState)),
                                      VBOX_E_INVALID_VM_STATE);
#else
                return VBOX_E_INVALID_VM_STATE;
#endif
        }

        setModified(IsModified_MachineData);
        mHWData.backup();

        /** @todo r=bird: The careful memory handling doesn't work out here because
         *  the catch block won't undo any damage we've done.  So, if push_back throws
         *  bad_alloc then you've lost the value.
         *
         *  Another thing. Doing a linear search here isn't extremely efficient, esp.
         *  since values that changes actually bubbles to the end of the list.  Using
         *  something that has an efficient lookup and can tolerate a bit of updates
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
             || RTStrSimplePatternMultiMatch(mHWData->mGuestPropertyNotificationPatterns.c_str(),
                                             RTSTR_MAX,
                                             utf8Name.c_str(),
                                             RTSTR_MAX, NULL)
           )
        {
            alock.release();

            mParent->onGuestPropertyChange(mData->mUuid,
                                           aName,
                                           aValue,
                                           aFlags);
        }
    }
    catch (...)
    {
        return VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }
    return S_OK;
#else
    ReturnComNotImplemented();
#endif
}

STDMETHODIMP SessionMachine::EjectMedium(IMediumAttachment *aAttachment,
                                         IMediumAttachment **aNewAttachment)
{
    CheckComArgNotNull(aAttachment);
    CheckComArgOutPointerValid(aNewAttachment);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // request the host lock first, since might be calling Host methods for getting host drives;
    // next, protect the media tree all the while we're in here, as well as our member variables
    AutoMultiWriteLock3 multiLock(mParent->host()->lockHandle(),
                                  this->lockHandle(),
                                  &mParent->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<MediumAttachment> pAttach = static_cast<MediumAttachment *>(aAttachment);

    Bstr ctrlName;
    LONG lPort;
    LONG lDevice;
    bool fTempEject;
    {
        AutoCaller autoAttachCaller(this);
        if (FAILED(autoAttachCaller.rc())) return autoAttachCaller.rc();

        AutoReadLock attLock(pAttach COMMA_LOCKVAL_SRC_POS);

        /* Need to query the details first, as the IMediumAttachment reference
         * might be to the original settings, which we are going to change. */
        ctrlName = pAttach->getControllerName();
        lPort = pAttach->getPort();
        lDevice = pAttach->getDevice();
        fTempEject = pAttach->getTempEject();
    }

    if (!fTempEject)
    {
        /* Remember previously mounted medium. The medium before taking the
         * backup is not necessarily the same thing. */
        ComObjPtr<Medium> oldmedium;
        oldmedium = pAttach->getMedium();

        setModified(IsModified_Storage);
        mMediaData.backup();

        // The backup operation makes the pAttach reference point to the
        // old settings. Re-get the correct reference.
        pAttach = findAttachment(mMediaData->mAttachments,
                                 ctrlName.raw(),
                                 lPort,
                                 lDevice);

        {
            AutoCaller autoAttachCaller(this);
            if (FAILED(autoAttachCaller.rc())) return autoAttachCaller.rc();

            AutoWriteLock attLock(pAttach COMMA_LOCKVAL_SRC_POS);
            if (!oldmedium.isNull())
                oldmedium->removeBackReference(mData->mUuid);

            pAttach->updateMedium(NULL);
            pAttach->updateEjected();
        }

        setModified(IsModified_Storage);
    }
    else
    {
        {
            AutoWriteLock attLock(pAttach COMMA_LOCKVAL_SRC_POS);
            pAttach->updateEjected();
        }
    }

    pAttach.queryInterfaceTo(aNewAttachment);

    return S_OK;
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
        reason = mData->mSession.mState == SessionState_Unlocking ?
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
HRESULT SessionMachine::onNATRedirectRuleChange(ULONG ulSlot, BOOL aNatRuleRemove, IN_BSTR aRuleName,
                                 NATProtocol_T aProto, IN_BSTR aHostIp, LONG aHostPort, IN_BSTR aGuestIp, LONG aGuestPort)
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
    /*
     * instead acting like callback we ask IVirtualBox deliver corresponding event
     */

    mParent->onNatRedirectChange(getId(), ulSlot, RT_BOOL(aNatRuleRemove), aRuleName, aProto, aHostIp, (uint16_t)aHostPort, aGuestIp, (uint16_t)aGuestPort);
    return S_OK;
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

HRESULT SessionMachine::onCPUExecutionCapChange(ULONG aExecutionCap)
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

    return directControl->OnCPUExecutionCapChange(aExecutionCap);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onVRDEServerChange(BOOL aRestart)
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

    return directControl->OnVRDEServerChange(aRestart);
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
 * @note Locks this object for reading.
 */
HRESULT SessionMachine::onClipboardModeChange(ClipboardMode_T aClipboardMode)
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

    return directControl->OnClipboardModeChange(aClipboardMode);
}

/**
 * @note Locks this object for reading.
 */
HRESULT SessionMachine::onDragAndDropModeChange(DragAndDropMode_T aDragAndDropMode)
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

    return directControl->OnDragAndDropModeChange(aDragAndDropMode);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onBandwidthGroupChange(IBandwidthGroup *aBandwidthGroup)
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

    return directControl->OnBandwidthGroupChange(aBandwidthGroup);
}

/**
 *  @note Locks this object for reading.
 */
HRESULT SessionMachine::onStorageDeviceChange(IMediumAttachment *aAttachment, BOOL aRemove)
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

    return directControl->OnStorageDeviceChange(aAttachment, aRemove);
}

/**
 *  Returns @c true if this machine's USB controller reports it has a matching
 *  filter for the given USB device and @c false otherwise.
 *
 *  @note locks this object for reading.
 */
bool SessionMachine::hasMatchingUSBFilter(const ComObjPtr<HostUSBDevice> &aDevice, ULONG *aMaskedIfs)
{
    AutoCaller autoCaller(this);
    /* silently return if not ready -- this method may be called after the
     * direct machine session has been called */
    if (!autoCaller.isOk())
        return false;

#ifdef VBOX_WITH_USB
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    switch (mData->mMachineState)
    {
        case MachineState_Starting:
        case MachineState_Restoring:
        case MachineState_TeleportingIn:
        case MachineState_Paused:
        case MachineState_Running:
        /** @todo Live Migration: snapshoting & teleporting. Need to fend things of
         *        elsewhere... */
            alock.release();
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
 *  @param aRc      S_OK if the snapshot has been taken successfully
 *  @param aErrMsg  human readable error message for failure
 *
 *  @note Locks mParent + this objects for writing.
 */
HRESULT SessionMachine::endSavingState(HRESULT aRc, const Utf8Str &aErrMsg)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (SUCCEEDED(aRc))
    {
        mSSData->strStateFilePath = mConsoleTaskData.strStateFilePath;

        /* save all VM settings */
        rc = saveSettings(NULL);
                // no need to check whether VirtualBox.xml needs saving also since
                // we can't have a name change pending at this point
    }
    else
    {
        // delete the saved state file (it might have been already created);
        // we need not check whether this is shared with a snapshot here because
        // we certainly created this saved state file here anew
        RTFileDelete(mConsoleTaskData.strStateFilePath.c_str());
    }

    /* notify the progress object about operation completion */
    Assert(mConsoleTaskData.mProgress);
    if (SUCCEEDED(aRc))
        mConsoleTaskData.mProgress->notifyComplete(S_OK);
    else
    {
        if (aErrMsg.length())
            mConsoleTaskData.mProgress->notifyComplete(aRc,
                                                       COM_IIDOF(ISession),
                                                       getComponentName(),
                                                       aErrMsg.c_str());
        else
            mConsoleTaskData.mProgress->notifyComplete(aRc);
    }

    /* clear out the temporary saved state data */
    mConsoleTaskData.mLastState = MachineState_Null;
    mConsoleTaskData.strStateFilePath.setNull();
    mConsoleTaskData.mProgress.setNull();

    LogFlowThisFuncLeave();
    return rc;
}

/**
 * Deletes the given file if it is no longer in use by either the current machine state
 * (if the machine is "saved") or any of the machine's snapshots.
 *
 * Note: This checks mSSData->strStateFilePath, which is shared by the Machine and SessionMachine
 * but is different for each SnapshotMachine. When calling this, the order of calling this
 * function on the one hand and changing that variable OR the snapshots tree on the other hand
 * is therefore critical. I know, it's all rather messy.
 *
 * @param strStateFile
 * @param pSnapshotToIgnore  Passed to Snapshot::sharesSavedStateFile(); this snapshot is ignored in the test for whether the saved state file is in use.
 */
void SessionMachine::releaseSavedStateFile(const Utf8Str &strStateFile,
                                           Snapshot *pSnapshotToIgnore)
{
    // it is safe to delete this saved state file if it is not currently in use by the machine ...
    if (    (strStateFile.isNotEmpty())
         && (strStateFile != mSSData->strStateFilePath)     // session machine's saved state
       )
        // ... and it must also not be shared with other snapshots
        if (    !mData->mFirstSnapshot
             || !mData->mFirstSnapshot->sharesSavedStateFile(strStateFile, pSnapshotToIgnore)
                                // this checks the SnapshotMachine's state file paths
           )
            RTFileDelete(strStateFile.c_str());
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

    AutoMultiWriteLock2 alock(this->lockHandle(),
                              &mParent->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    AssertReturn(   mData->mMachineState == MachineState_Starting
                 || mData->mMachineState == MachineState_Restoring
                 || mData->mMachineState == MachineState_TeleportingIn, E_FAIL);
    /* bail out if trying to lock things with already set up locking */
    AssertReturn(mData->mSession.mLockedMedia.IsEmpty(), E_FAIL);

    clearError();
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
            MediumType_T mediumType = pMedium->getType();
            bool fIsReadOnlyLock =    mediumType == MediumType_Readonly
                                   || mediumType == MediumType_Shareable;
            bool fIsVitalImage = (devType == DeviceType_HardDisk);

            alock.release();
            mrc = pMedium->createMediumLockList(fIsVitalImage /* fFailIfInaccessible */,
                                                !fIsReadOnlyLock /* fMediumLockWrite */,
                                                NULL,
                                                *pMediumLockList);
            alock.acquire();
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
        alock.release();
        HRESULT rc = mData->mSession.mLockedMedia.Lock();
        alock.acquire();
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
 * @note This method must not call saveSettings or SaveSettings, otherwise
 *       it can cause crashes in random places due to unexpectedly committing
 *       the current settings. The caller is responsible for that. The call
 *       to saveStateSettings is fine, because this method does not commit.
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
             && (   mConsoleTaskData.mSnapshot.isNull()
                 || mConsoleTaskData.mLastState >= MachineState_Running /** @todo Live Migration: clean up (lazy bird) */
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
            Assert(!mSSData->strStateFilePath.isEmpty());

            // it is safe to delete the saved state file if ...
            if (    !mData->mFirstSnapshot      // ... we have no snapshots or
                 || !mData->mFirstSnapshot->sharesSavedStateFile(mSSData->strStateFilePath, NULL /* pSnapshotToIgnore */)
                                                // ... none of the snapshots share the saved state file
               )
                RTFileDelete(mSSData->strStateFilePath.c_str());
        }

        mSSData->strStateFilePath.setNull();
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
        Assert(!mSSData->strStateFilePath.isEmpty());
        stsFlags |= SaveSTS_StateFilePath;
    }

#ifdef VBOX_WITH_GUEST_PROPS
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
                if (   (it->mFlags & guestProp::TRANSIENT)
                    || (it->mFlags & guestProp::TRANSRESET))
                {
                    fNeedsSaving = true;
                    break;
                }
        if (fNeedsSaving)
        {
            mData->mCurrentStateModified = TRUE;
            stsFlags |= SaveSTS_CurStateModified;
        }
    }
#endif

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
         * operation to complete. For now, we accept this inconsistent behavior
         * and simply do nothing here. */

        if (mData->mSession.mState == SessionState_Unlocking)
            return S_OK;

        AssertReturn(!directControl.isNull(), E_FAIL);
    }

    return directControl->UpdateMachineState(mData->mMachineState);
}
