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
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "AudioAdapterImpl.h"
#include "MachineImpl.h"
#include "Logging.h"

#include <iprt/cpputils.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (AudioAdapter)

HRESULT AudioAdapter::FinalConstruct()
{
    return S_OK;
}

void AudioAdapter::FinalRelease()
{
    uninit ();
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
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    mData.allocate();

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
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    unconst (mPeer) = aThat;

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReaderLock thatLock (aThat);
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
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReaderLock thatLock (aThat);
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
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mData.free();

    unconst (mPeer).setNull();
    unconst (mParent).setNull();
}

// IAudioAdapter properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP AudioAdapter::COMGETTER(Enabled)(BOOL *aEnabled)
{
    if (!aEnabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aEnabled = mData->mEnabled;

    return S_OK;
}

STDMETHODIMP AudioAdapter::COMSETTER(Enabled)(BOOL aEnabled)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mEnabled != aEnabled)
    {
        mData.backup();
        mData->mEnabled = aEnabled;
    }

    return S_OK;
}

STDMETHODIMP AudioAdapter::COMGETTER(AudioDriver)(AudioDriverType_T *aAudioDriver)
{
    if (!aAudioDriver)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aAudioDriver = mData->mAudioDriver;

    return S_OK;
}

STDMETHODIMP AudioAdapter::COMSETTER(AudioDriver)(AudioDriverType_T aAudioDriver)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    HRESULT rc = S_OK;

    if (mData->mAudioDriver != aAudioDriver)
    {
        /*
         * which audio driver type are we supposed to use?
         */
        switch (aAudioDriver)
        {
            case AudioDriverType_Null:
#ifdef RT_OS_WINDOWS
# ifdef VBOX_WITH_WINMM
            case AudioDriverType_WinMM:
# endif
            case AudioDriverType_DirectSound:
#endif /* RT_OS_WINDOWS */
#ifdef RT_OS_SOLARIS
            case AudioDriverType_SolAudio:
#endif
#ifdef RT_OS_LINUX
            case AudioDriverType_OSS:
# ifdef VBOX_WITH_ALSA
            case AudioDriverType_ALSA:
# endif
# ifdef VBOX_WITH_PULSE
            case AudioDriverType_Pulse:
# endif
#endif /* RT_OS_LINUX */
#ifdef RT_OS_DARWIN
            case AudioDriverType_CoreAudio:
#endif
#ifdef RT_OS_OS2
            case AudioDriverType_MMPM:
#endif
            {
                mData.backup();
                mData->mAudioDriver = aAudioDriver;
                break;
            }

            default:
            {
                AssertMsgFailed (("Wrong audio driver type %d\n",
                                  aAudioDriver));
                rc = E_FAIL;
            }
        }
    }

    return rc;
}

STDMETHODIMP AudioAdapter::COMGETTER(AudioController)(AudioControllerType_T *aAudioController)
{
    if (!aAudioController)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aAudioController = mData->mAudioController;

    return S_OK;
}

STDMETHODIMP AudioAdapter::COMSETTER(AudioController)(AudioControllerType_T aAudioController)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

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
                mData.backup();
                mData->mAudioController = aAudioController;
                break;

            default:
            {
                AssertMsgFailed (("Wrong audio controller type %d\n",
                                  aAudioController));
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

/**
 *  Loads settings from the given machine node.
 *  May be called once right after this object creation.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for writing.
 */
HRESULT AudioAdapter::loadSettings (const settings::Key &aMachineNode)
{
    using namespace settings;

    AssertReturn (!aMachineNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    /* Note: we assume that the default values for attributes of optional
     * nodes are assigned in the Data::Data() constructor and don't do it
     * here. It implies that this method may only be called after constructing
     * a new BIOSSettings object while all its data fields are in the default
     * values. Exceptions are fields whose creation time defaults don't match
     * values that should be applied when these fields are not explicitly set
     * in the settings file (for backwards compatibility reasons). This takes
     * place when a setting of a newly created object must default to A while
     * the same setting of an object loaded from the old settings file must
     * default to B. */

    /* AudioAdapter node (required) */
    Key audioAdapterNode = aMachineNode.key ("AudioAdapter");

    /* is the adapter enabled? (required) */
    mData->mEnabled = audioAdapterNode.value <bool> ("enabled");

    /* now check the audio adapter (not required, default is AC97) */
    const char *controller = audioAdapterNode.stringValue ("controller");
    if (strcmp (controller, "SB16") == 0)
        mData->mAudioController = AudioControllerType_SB16;
    else
        mData->mAudioController = AudioControllerType_AC97;

    /* now check the audio driver (required) */
    const char *driver = audioAdapterNode.stringValue ("driver");
    mData->mAudioDriver = AudioDriverType_Null;
    if      (strcmp (driver, "Null") == 0)
        ; /* Null has been set above */
#ifdef RT_OS_WINDOWS
    else if (strcmp (driver, "WinMM") == 0)
#ifdef VBOX_WITH_WINMM
        mData->mAudioDriver = AudioDriverType_WinMM;
#else
        /* fall back to dsound */
        mData->mAudioDriver = AudioDriverType_DirectSound;
#endif
    else if (strcmp (driver, "DirectSound") == 0)
        mData->mAudioDriver = AudioDriverType_DirectSound;
#endif // RT_OS_WINDOWS
#ifdef RT_OS_SOLARIS
    else if (strcmp (driver, "SolAudio") == 0)
        mData->mAudioDriver = AudioDriverType_SolAudio;
#endif // RT_OS_SOLARIS
#ifdef RT_OS_LINUX
    else if (strcmp (driver, "OSS") == 0)
        mData->mAudioDriver = AudioDriverType_OSS;
    else if (strcmp (driver, "ALSA") == 0)
# ifdef VBOX_WITH_ALSA
        mData->mAudioDriver = AudioDriverType_ALSA;
# else
        /* fall back to OSS */
        mData->mAudioDriver = AudioDriverType_OSS;
# endif
    else if (strcmp (driver, "Pulse") == 0)
# ifdef VBOX_WITH_PULSE
        mData->mAudioDriver = AudioDriverType_Pulse;
# else
        /* fall back to OSS */
        mData->mAudioDriver = AudioDriverType_OSS;
# endif
#endif // RT_OS_LINUX
#ifdef RT_OS_DARWIN
    else if (strcmp (driver, "CoreAudio") == 0)
        mData->mAudioDriver = AudioDriverType_CoreAudio;
#endif
#ifdef RT_OS_OS2
    else if (strcmp (driver, "MMPM") == 0)
        mData->mAudioDriver = AudioDriverType_MMPM;
#endif
    else
        AssertMsgFailed (("Invalid driver '%s'\n", driver));

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param aMachineNode <Machine> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT AudioAdapter::saveSettings (settings::Key &aMachineNode)
{
    using namespace settings;

    AssertReturn (!aMachineNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Key node = aMachineNode.createKey ("AudioAdapter");

    const char *controllerStr = NULL;
    switch (mData->mAudioController)
    {
        case AudioControllerType_SB16:
        {
            controllerStr = "SB16";
            break;
        }
        default:
        {
            controllerStr = "AC97";
            break;
        }
    }
    node.setStringValue ("controller", controllerStr);

    const char *driverStr = NULL;
    switch (mData->mAudioDriver)
    {
        case AudioDriverType_Null:
        {
            driverStr = "Null";
            break;
        }
#ifdef RT_OS_WINDOWS
            case AudioDriverType_WinMM:
# ifdef VBOX_WITH_WINMM
            {
                driverStr = "WinMM";
                break;
            }
# endif
            case AudioDriverType_DirectSound:
            {
                driverStr = "DirectSound";
                break;
            }
#endif /* RT_OS_WINDOWS */
#ifdef RT_OS_SOLARIS
            case AudioDriverType_SolAudio:
            {
                driverStr = "SolAudio";
                break;
            }
#endif
#ifdef RT_OS_LINUX
            case AudioDriverType_ALSA:
# ifdef VBOX_WITH_ALSA
            {
                driverStr = "ALSA";
                break;
            }
# endif
            case AudioDriverType_Pulse:
# ifdef VBOX_WITH_PULSE
            {
                driverStr = "Pulse";
                break;
            }
# endif
            case AudioDriverType_OSS:
            {
                driverStr = "OSS";
                break;
            }
#endif /* RT_OS_LINUX */
#ifdef RT_OS_DARWIN
            case AudioDriverType_CoreAudio:
            {
                driverStr = "CoreAudio";
                break;
            }
#endif
#ifdef RT_OS_OS2
            case AudioDriverType_MMPM:
            {
                driverStr = "MMPM";
                break;
            }
#endif
            default:
                ComAssertMsgFailedRet (("Wrong audio driver type! driver = %d\n",
                                        mData->mAudioDriver),
                                       E_FAIL);
    }
    node.setStringValue ("driver", driverStr);

    node.setValue <bool> ("enabled", !!mData->mEnabled);

    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
bool AudioAdapter::rollback()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoLock alock (this);

    bool changed = false;

    if (mData.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        changed = mData.hasActualChanges();
        mData.rollback();
    }

    return changed;
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void AudioAdapter::commit()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (mPeer);
    AssertComRCReturnVoid (thatCaller.rc());

    /* lock both for writing since we modify both */
    AutoMultiLock <2> alock (this->wlock(), AutoLock::maybeWlock (mPeer));

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
void AudioAdapter::copyFrom (AudioAdapter *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (mPeer);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading */
    AutoMultiLock <2> alock (this->wlock(), aThat->rlock());

    /* this will back up current data */
    mData.assignCopy (aThat->mData);
}
