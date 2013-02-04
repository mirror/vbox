/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

AudioAdapter::AudioAdapter()
    : mParent(NULL)
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

    mData.allocate();
    mData->mAudioDriver = defaultAudioDriver;

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
    mData.share (aThat->mData);

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
    mData.attachCopy (aThat->mData);

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

    mData.free();

    unconst(mPeer) = NULL;
    unconst(mParent) = NULL;
}

// IAudioAdapter properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP AudioAdapter::COMGETTER(Enabled)(BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = mData->mEnabled;

    return S_OK;
}

STDMETHODIMP AudioAdapter::COMSETTER(Enabled)(BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mEnabled != aEnabled)
    {
        mData.backup();
        mData->mEnabled = aEnabled;

        alock.release();
        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
        mParent->setModified(Machine::IsModified_AudioAdapter);
    }

    return S_OK;
}

STDMETHODIMP AudioAdapter::COMGETTER(AudioDriver)(AudioDriverType_T *aAudioDriver)
{
    CheckComArgOutPointerValid(aAudioDriver);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aAudioDriver = mData->mAudioDriver;

    return S_OK;
}

STDMETHODIMP AudioAdapter::COMSETTER(AudioDriver)(AudioDriverType_T aAudioDriver)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (mData->mAudioDriver != aAudioDriver)
    {
        if (settings::MachineConfigFile::isAudioDriverAllowedOnThisHost(aAudioDriver))
        {
            mData.backup();
            mData->mAudioDriver = aAudioDriver;

            alock.release();
            AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
            mParent->setModified(Machine::IsModified_AudioAdapter);
        }
        else
        {
            AssertMsgFailed(("Wrong audio driver type %d\n", aAudioDriver));
            rc = E_FAIL;
        }
    }

    return rc;
}

STDMETHODIMP AudioAdapter::COMGETTER(AudioController)(AudioControllerType_T *aAudioController)
{
    CheckComArgOutPointerValid(aAudioController);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aAudioController = mData->mAudioController;

    return S_OK;
}

STDMETHODIMP AudioAdapter::COMSETTER(AudioController)(AudioControllerType_T aAudioController)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (mData->mAudioController != aAudioController)
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
                mData.backup();
                mData->mAudioController = aAudioController;

                alock.release();
                AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);  // mParent is const, needs no locking
                mParent->setModified(Machine::IsModified_AudioAdapter);
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

AudioAdapter::Data::Data()
{
    /* Generic defaults */
    mEnabled = false;
    mAudioController = AudioControllerType_AC97;
    /* Driver defaults to the null audio driver */
    mAudioDriver = AudioDriverType_Null;
}

/**
 *  Loads settings from the given machine node.
 *  May be called once right after this object creation.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for writing.
 */
HRESULT AudioAdapter::loadSettings(const settings::AudioAdapter &data)
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

    mData->mEnabled = data.fEnabled;
    mData->mAudioController = data.controllerType;
    mData->mAudioDriver = data.driverType;

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT AudioAdapter::saveSettings(settings::AudioAdapter &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.fEnabled = !!mData->mEnabled;
    data.controllerType = mData->mAudioController;
    data.driverType = mData->mAudioDriver;
    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
void AudioAdapter::rollback()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.rollback();
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void AudioAdapter::commit()
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

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->mData.attach (mData);
        }
    }
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void AudioAdapter::copyFrom(AudioAdapter *aThat)
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
    mData.assignCopy(aThat->mData);
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
