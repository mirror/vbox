/* $Id$ */
/** @file
 * VirtualBox COM class implementation - Audio settings for a VM.
 */

/*
 * Copyright (C) 2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#define LOG_GROUP LOG_GROUP_MAIN_AUDIOSETTINGS
#include "AudioSettingsImpl.h"
#include "MachineImpl.h"

#include <iprt/cpp/utils.h>

#include <VBox/settings.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "LoggingNew.h"


////////////////////////////////////////////////////////////////////////////////
//
// AudioSettings private data definition
//
////////////////////////////////////////////////////////////////////////////////

struct AudioSettings::Data
{
    Data()
        : pParent(NULL)
    { }

    Machine * const                pParent;
    const ComObjPtr<AudioAdapter>  pAdapter;
    const ComObjPtr<AudioSettings> pPeer;
};

DEFINE_EMPTY_CTOR_DTOR(AudioSettings)

HRESULT AudioSettings::FinalConstruct()
{
    return BaseFinalConstruct();
}

void AudioSettings::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}


// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the audio settings object.
 *
 * @param aParent  Handle of the parent object.
 */
HRESULT AudioSettings::init(Machine *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    /* share the parent weakly */
    unconst(m->pParent) = aParent;

    /* create the audio adapter object (always present, default is disabled) */
    unconst(m->pAdapter).createObject();
    m->pAdapter->init(this);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 * Initializes the audio settings object given another audio settings object
 * (a kind of copy constructor). This object shares data with
 * the object passed as an argument.
 *
 * @note This object must be destroyed before the original object
 * it shares data with is destroyed.
 *
 * @note Locks @a aThat object for reading.
 */
HRESULT AudioSettings::init(Machine *aParent, AudioSettings *aThat)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p, aThat: %p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pParent) = aParent;
    unconst(m->pPeer)   = aThat;

    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatlock(aThat COMMA_LOCKVAL_SRC_POS);

    unconst(m->pAdapter) = aThat->m->pAdapter;

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 * Initializes the guest object given another guest object
 * (a kind of copy constructor). This object makes a private copy of data
 * of the original object passed as an argument.
 *
 * @note Locks @a aThat object for reading.
 */
HRESULT AudioSettings::initCopy(Machine *aParent, AudioSettings *aThat)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p, aThat: %p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pParent) = aParent;
    // pPeer is left null

    AutoReadLock thatlock(aThat COMMA_LOCKVAL_SRC_POS);

    HRESULT hrc = unconst(m->pAdapter).createObject();
    ComAssertComRCRet(hrc, hrc);
    hrc = m->pAdapter->init(this);
    ComAssertComRCRet(hrc, hrc);
    m->pAdapter->i_copyFrom(aThat->m->pAdapter);

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void AudioSettings::uninit(void)
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    delete m;
    m = NULL;
}


// IAudioSettings properties
////////////////////////////////////////////////////////////////////////////////

HRESULT AudioSettings::getAdapter(ComPtr<IAudioAdapter> &aAdapter)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aAdapter = m->pAdapter;

    return S_OK;
}


// IAudioSettings methods
////////////////////////////////////////////////////////////////////////////////

HRESULT AudioSettings::getHostAudioDevice(AudioDirection_T aUsage, ComPtr<IHostAudioDevice> &aDevice)
{
    RT_NOREF(aUsage, aDevice);
    ReturnComNotImplemented();
}

HRESULT AudioSettings::setHostAudioDevice(const ComPtr<IHostAudioDevice> &aDevice, AudioDirection_T aUsage)
{
    RT_NOREF(aDevice, aUsage);
    ReturnComNotImplemented();
}


// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/**
 * Returns the parent object as a weak pointer.
 */
Machine* AudioSettings::i_getParent(void)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), NULL);

    AssertPtrReturn(m, NULL);
    return m->pParent;
}

/**
 * Determines whether the audio settings currently can be changed or not.
 *
 * @returns \c true if the settings can be changed, \c false if not.
 */
bool AudioSettings::i_canChangeSettings(void)
{
    AutoAnyStateDependency adep(m->pParent);
    if (FAILED(adep.rc()))
        return false;

    /** @todo Do some more checks here? */
    return true;
}

/**
 * Gets called when the machine object needs to know that audio adapter settings
 * have been changed.
 */
void AudioSettings::i_onAdapterChanged(IAudioAdapter *pAdapter)
{
    LogFlowThisFuncEnter();

    AssertPtrReturnVoid(pAdapter);

    m->pParent->i_onAudioAdapterChange(pAdapter); // mParent is const, needs no locking

    LogFlowThisFuncLeave();
}

/**
 * Gets called when the machine object needs to know that a host audio device
 * has been changed.
 */
void AudioSettings::i_onHostDeviceChanged(IHostAudioDevice *pDevice,
                                          bool fIsNew, AudioDeviceState_T enmState, IVirtualBoxErrorInfo *pErrInfo)
{
    LogFlowThisFuncEnter();

    AssertPtrReturnVoid(pDevice);

    m->pParent->i_onHostAudioDeviceChange(pDevice, fIsNew, enmState, pErrInfo); // mParent is const, needs no locking

    LogFlowThisFuncLeave();
}

/**
 * Gets called when the machine object needs to know that the audio settings
 * have been changed.
 */
void AudioSettings::i_onSettingsChanged(void)
{
    LogFlowThisFuncEnter();

    AutoWriteLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);
    m->pParent->i_setModified(Machine::IsModified_AudioSettings);
    mlock.release();

    LogFlowThisFuncLeave();
}

/**
 * Loads settings from the given machine node.
 * May be called once right after this object creation.
 *
 * @param data Configuration settings.
 *
 * @note Locks this object for writing.
 */
HRESULT AudioSettings::i_loadSettings(const settings::AudioAdapter &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->pAdapter->i_loadSettings(data);

    /* Note: The host audio device selection is run-time only, e.g. won't be serialized in the settings! */
    return S_OK;
}

/**
 * Saves settings to the given node.
 *
 * @param data Configuration settings.
 *
 * @note Locks this object for reading.
 */
HRESULT AudioSettings::i_saveSettings(settings::AudioAdapter &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->pAdapter->i_saveSettings(data);

    /* Note: The host audio device selection is run-time only, e.g. won't be serialized in the settings! */
    return S_OK;
}

/**
 * @note Locks this object for writing, together with the peer object
 * represented by @a aThat (locked for reading).
 */
void AudioSettings::i_copyFrom(AudioSettings *aThat)
{
    AssertReturnVoid(aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller(aThat);
    AssertComRCReturnVoid(thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    m->pAdapter->i_copyFrom(aThat->m->pAdapter);
}

/**
 * Applies default audio settings, based on the given guest OS type.
 *
 * @returns HRESULT
 * @param   aGuestOsType        Guest OS type to use for basing the default settings on.
 */
HRESULT AudioSettings::i_applyDefaults(ComObjPtr<GuestOSType> &aGuestOsType)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AudioControllerType_T audioController;
    HRESULT rc = aGuestOsType->COMGETTER(RecommendedAudioController)(&audioController);
    if (FAILED(rc)) return rc;

    rc = m->pAdapter->COMSETTER(AudioController)(audioController);
    if (FAILED(rc)) return rc;

    AudioCodecType_T audioCodec;
    rc = aGuestOsType->COMGETTER(RecommendedAudioCodec)(&audioCodec);
    if (FAILED(rc)) return rc;

    rc = m->pAdapter->COMSETTER(AudioCodec)(audioCodec);
    if (FAILED(rc)) return rc;

    rc = m->pAdapter->COMSETTER(Enabled)(true);
    if (FAILED(rc)) return rc;

    rc = m->pAdapter->COMSETTER(EnabledOut)(true);
    if (FAILED(rc)) return rc;

    /* Note: We do NOT enable audio input by default due to security reasons!
     *       This always has to be done by the user manually. */

    /* Note: Does not touch the host audio device selection, as this is a run-time only setting. */
    return S_OK;
}

/**
 * @note Locks this object for writing.
 */
void AudioSettings::i_rollback(void)
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->pAdapter->i_rollback();

    /* Note: Does not touch the host audio device selection, as this is a run-time only setting. */
}

/**
 * @note Locks this object for writing, together with the peer object (also
 * for writing) if there is one.
 */
void AudioSettings::i_commit(void)
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    m->pAdapter->i_commit();

    /* Note: Does not touch the host audio device selection, as this is a run-time only setting. */
}

