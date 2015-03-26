/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "AudioAdapterImpl.h"
#include "MachineImpl.h"

#include <iprt/cpp/utils.h>

#include <VBox/settings.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Logging.h"

struct AudioAdapterData
{
    AudioAdapterData() :
        mEnabled(false),
        mAudioDriver(AudioDriverType_Null),
        mAudioController(AudioControllerType_AC97)
    {}

    BOOL mEnabled;
    AudioDriverType_T mAudioDriver;
    AudioControllerType_T mAudioController;
};

struct AudioAdapter::Data
{
    Backupable<AudioAdapterData> m;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

AudioAdapter::AudioAdapter()
    : mParent(NULL),
      mData(NULL)
{
}

AudioAdapter::~AudioAdapter()
{
}

HRESULT AudioAdapter::FinalConstruct()
{
    return BaseFinalConstruct();
}

void AudioAdapter::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the audio adapter object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT AudioAdapter::init (Machine *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Get the default audio driver out of the system properties */
    ComPtr<IVirtualBox> VBox;
    HRESULT rc = aParent->COMGETTER(Parent)(VBox.asOutParam());
    if (FAILED(rc)) return rc;
    ComPtr<ISystemProperties> sysProps;
    rc = VBox->COMGETTER(SystemProperties)(sysProps.asOutParam());
    if (FAILED(rc)) return rc;
    AudioDriverType_T defaultAudioDriver;
    rc = sysProps->COMGETTER(DefaultAudioDriver)(&defaultAudioDriver);
    if (FAILED(rc)) return rc;

    unconst(mParent) = aParent;
    /* mPeer is left null */

    mData = new Data();
    mData->m.allocate();
    mData->m->mAudioDriver = defaultAudioDriver;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the audio adapter object given another audio adapter object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT AudioAdapter::init (Machine *aParent, AudioAdapter *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    unconst(mPeer) = aThat;

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    mData = new Data();
    mData->m.share (aThat->mData->m);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT AudioAdapter::initCopy (Machine *aParent, AudioAdapter *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    /* mPeer is left null */

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    mData = new Data();
    mData->m.attachCopy (aThat->mData->m);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void AudioAdapter::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    mData->m.free();
    delete mData;
    mData = NULL;

    unconst(mPeer) = NULL;
    unconst(mParent) = NULL;
}

// IAudioAdapter properties
/////////////////////////////////////////////////////////////////////////////

HRESULT AudioAdapter::getEnabled(BOOL *aEnabled)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = mData->m->mEnabled;

    return S_OK;
}

HRESULT AudioAdapter::setEnabled(BOOL aEnabled)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->m->mEnabled != aEnabled)
    {
        mData->m.backup();
        mData->m->mEnabled = aEnabled;

        alock.release();
        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
        mParent->i_setModified(Machine::IsModified_AudioAdapter);
    }

    return S_OK;
}

HRESULT AudioAdapter::getAudioDriver(AudioDriverType_T *aAudioDriver)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aAudioDriver = mData->m->mAudioDriver;

    return S_OK;
}

HRESULT AudioAdapter::setAudioDriver(AudioDriverType_T aAudioDriver)
{

    /* the machine needs to be mutable */
    AutoMutableOrSavedStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (mData->m->mAudioDriver != aAudioDriver)
    {
        if (settings::MachineConfigFile::isAudioDriverAllowedOnThisHost(aAudioDriver))
        {
            mData->m.backup();
            mData->m->mAudioDriver = aAudioDriver;
            alock.release();
            AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
            mParent->i_setModified(Machine::IsModified_AudioAdapter);
        }
        else
        {
            AssertMsgFailed(("Wrong audio driver type %d\n", aAudioDriver));
            rc = E_FAIL;
        }
    }

    return rc;
}

HRESULT AudioAdapter::getAudioController(AudioControllerType_T *aAudioController)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aAudioController = mData->m->mAudioController;

    return S_OK;
}

HRESULT AudioAdapter::setAudioController(AudioControllerType_T aAudioController)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (mData->m->mAudioController != aAudioController)
    {
        /*
         * which audio hardware type are we supposed to use?
         */
        switch (aAudioController)
        {
            case AudioControllerType_AC97:
            case AudioControllerType_SB16:
            case AudioControllerType_HDA:
            {
                mData->m.backup();
                mData->m->mAudioController = aAudioController;
                alock.release();
                AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
                mParent->i_setModified(Machine::IsModified_AudioAdapter);
                break;
            }

            default:
                AssertMsgFailed (("Wrong audio controller type %d\n",
                                  aAudioController));
                rc = E_FAIL;
        }
    }

    return rc;
}

// IAudioAdapter methods
/////////////////////////////////////////////////////////////////////////////

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 *  Loads settings from the given machine node.
 *  May be called once right after this object creation.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for writing.
 */
HRESULT AudioAdapter::i_loadSettings(const settings::AudioAdapter &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Note: we assume that the default values for attributes of optional
     * nodes are assigned in the Data::Data() constructor and don't do it
     * here. It implies that this method may only be called after constructing
     * a new AudioAdapter object while all its data fields are in the default
     * values. Exceptions are fields whose creation time defaults don't match
     * values that should be applied when these fields are not explicitly set
     * in the settings file (for backwards compatibility reasons). This takes
     * place when a setting of a newly created object must default to A while
     * the same setting of an object loaded from the old settings file must
     * default to B. */

    mData->m->mEnabled = data.fEnabled;
    mData->m->mAudioController = data.controllerType;
    mData->m->mAudioDriver = data.driverType;

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT AudioAdapter::i_saveSettings(settings::AudioAdapter &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.fEnabled = !!mData->m->mEnabled;
    data.controllerType = mData->m->mAudioController;
    data.driverType = mData->m->mAudioDriver;
    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
void AudioAdapter::i_rollback()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m.rollback();
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void AudioAdapter::i_commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(mPeer, this COMMA_LOCKVAL_SRC_POS);

    if (mData->m.isBackedUp())
    {
        mData->m.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->mData->m.attach (mData->m);
        }
    }
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void AudioAdapter::i_copyFrom(AudioAdapter *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    mData->m.assignCopy(aThat->mData->m);
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
