/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "AudioAdapterImpl.h"
#include "MachineImpl.h"
#include "Logging.h"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT AudioAdapter::FinalConstruct()
{
    return S_OK;
}

void AudioAdapter::FinalRelease()
{
    if (isReady())
        uninit ();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the audio adapter object.
 *
 * @returns COM result indicator
 */
HRESULT AudioAdapter::init (Machine *parent)
{
    LogFlowMember (("AudioAdapter::init (%p)\n", parent));

    ComAssertRet (parent, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;
    // mPeer is left null

    mData.allocate();

    setReady (true);
    return S_OK;
}

/**
 *  Initializes the audio adapter object given another audio adapter object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 */
HRESULT AudioAdapter::init (Machine *parent, AudioAdapter *that)
{
    LogFlowMember (("AudioAdapter::init (%p, %p)\n", parent, that));

    ComAssertRet (parent && that, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;
    mPeer = that;

    AutoLock thatlock (that);
    mData.share (that->mData);

    setReady (true);
    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT AudioAdapter::initCopy (Machine *parent, AudioAdapter *that)
{
    LogFlowMember (("AudioAdapter::initCopy (%p, %p)\n", parent, that));

    ComAssertRet (parent && that, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;
    // mPeer is left null

    AutoLock thatlock (that);
    mData.attachCopy (that->mData);

    setReady (true);
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void AudioAdapter::uninit()
{
    LogFlowMember (("AudioAdapter::uninit()\n"));

    AutoLock alock (this);
    AssertReturn (isReady(), (void) 0);

    mData.free();

    mPeer.setNull();
    mParent.setNull();

    setReady(false);
}

// IAudioAdapter properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the enabled status
 *
 * @returns COM status code
 * @param enabled address of result variable
 */
STDMETHODIMP AudioAdapter::COMGETTER(Enabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    *enabled = mData->mEnabled;
    return S_OK;
}

/**
 * Sets the enabled state
 *
 * @returns COM status code
 * @param enabled address of result variable
 */
STDMETHODIMP AudioAdapter::COMSETTER(Enabled)(BOOL enabled)
{
    AutoLock lock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    if (mData->mEnabled != enabled)
    {
        mData.backup();
        mData->mEnabled = enabled;
    }

    return S_OK;
}

/**
 * Returns the current audio driver type
 *
 * @returns COM status code
 * @param audioDriver address of result variable
 */
STDMETHODIMP AudioAdapter::COMGETTER(AudioDriver)(AudioDriverType_T *audioDriver)
{
    if (!audioDriver)
        return E_POINTER;

    AutoLock lock(this);
    CHECK_READY();

    *audioDriver = mData->mAudioDriver;
    return S_OK;
}

/**
 * Sets the audio driver type
 *
 * @returns COM status code
 * @param audioDriver audio driver type to use
 */
STDMETHODIMP AudioAdapter::COMSETTER(AudioDriver)(AudioDriverType_T audioDriver)
{
    AutoLock lock(this);
    CHECK_READY();

    CHECK_MACHINE_MUTABILITY (mParent);

    HRESULT rc = S_OK;

    if (mData->mAudioDriver != audioDriver)
    {
        /*
         * which audio driver type are we supposed to use?
         */
        switch (audioDriver)
        {
            case AudioDriverType_NullAudioDriver:
#ifdef __WIN__
#ifdef VBOX_WITH_WINMM
            case AudioDriverType_WINMMAudioDriver:
#endif
            case AudioDriverType_DSOUNDAudioDriver:
#endif /* __WIN__ */
#ifdef __LINUX__
            case AudioDriverType_OSSAudioDriver:
#ifdef VBOX_WITH_ALSA
            case AudioDriverType_ALSAAudioDriver:
#endif
#endif /* __LINUX__ */
#ifdef __DARWIN__
            case AudioDriverType_CoreAudioDriver:
#endif 
#ifdef __OS2__
            case AudioDriverType_MMPMAudioDriver:
#endif 
            {
                mData.backup();
                mData->mAudioDriver = audioDriver;
                break;
            }

            default:
            {
                Log(("wrong audio driver type specified!\n"));
                rc = E_FAIL;
            }
        }
    }

    return rc;
}

// IAudioAdapter methods
/////////////////////////////////////////////////////////////////////////////

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

void AudioAdapter::commit()
{
    AutoLock alock (this);
    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            // attach new data to the peer and reshare it
            AutoLock peerlock (mPeer);
            mPeer->mData.attach (mData);
        }
    }
}

void AudioAdapter::copyFrom (AudioAdapter *aThat)
{
    AutoLock alock (this);

    // this will back up current data
    mData.assignCopy (aThat->mData);
}

