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
 */

#include "BIOSSettingsImpl.h"
#include "MachineImpl.h"
#include "Logging.h"
#include <iprt/cpputils.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT BIOSSettings::FinalConstruct()
{
    return S_OK;
}

void BIOSSettings::FinalRelease()
{
    uninit ();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the audio adapter object.
 *
 * @returns COM result indicator
 */
HRESULT BIOSSettings::init (Machine *aParent)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aParent: %p\n", aParent));

    ComAssertRet (aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    /* share the parent weakly */
    unconst (mParent) = aParent;

    mData.allocate();

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
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
HRESULT BIOSSettings::init (Machine *aParent, BIOSSettings *that)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aParent: %p, that: %p\n", aParent, that));

    ComAssertRet (aParent && that, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    mParent = aParent;
    mPeer = that;

    AutoLock thatlock (that);
    mData.share (that->mData);

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT BIOSSettings::initCopy (Machine *aParent, BIOSSettings *that)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aParent: %p, that: %p\n", aParent, that));

    ComAssertRet (aParent && that, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    mParent = aParent;
    // mPeer is left null

    AutoLock thatlock (that);
    mData.attachCopy (that->mData);

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void BIOSSettings::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mData.free();

    mPeer.setNull();
    mParent.setNull();

    LogFlowThisFuncLeave();
}

// IBIOSSettings properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP BIOSSettings::COMGETTER(LogoFadeIn)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *enabled = mData->mLogoFadeIn;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoFadeIn)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    mData.backup();
    mData->mLogoFadeIn = enable;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(LogoFadeOut)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *enabled = mData->mLogoFadeOut;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoFadeOut)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    mData.backup();
    mData->mLogoFadeOut = enable;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(LogoDisplayTime)(ULONG *displayTime)
{
    if (!displayTime)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *displayTime = mData->mLogoDisplayTime;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoDisplayTime)(ULONG displayTime)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    mData.backup();
    mData->mLogoDisplayTime = displayTime;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(LogoImagePath)(BSTR *imagePath)
{
    if (!imagePath)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mLogoImagePath.cloneTo(imagePath);
    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(LogoImagePath)(INPTR BSTR imagePath)
{
    /* empty strings are not allowed as path names */
    if (imagePath && !(*imagePath))
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    mData.backup();
    mData->mLogoImagePath = imagePath;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(BootMenuMode)(BIOSBootMenuMode_T *bootMenuMode)
{
    if (!bootMenuMode)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *bootMenuMode = mData->mBootMenuMode;
    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(BootMenuMode)(BIOSBootMenuMode_T bootMenuMode)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    mData.backup();
    mData->mBootMenuMode = bootMenuMode;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(ACPIEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *enabled = mData->mACPIEnabled;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(ACPIEnabled)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    mData.backup();
    mData->mACPIEnabled = enable;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(IOAPICEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *enabled = mData->mIOAPICEnabled;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(IOAPICEnabled)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    mData.backup();
    mData->mIOAPICEnabled = enable;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(PXEDebugEnabled)(BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *enabled = mData->mPXEDebugEnabled;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(PXEDebugEnabled)(BOOL enable)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    mData.backup();
    mData->mPXEDebugEnabled = enable;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMGETTER(TimeOffset)(LONG64 *offset)
{
    if (!offset)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *offset = mData->mTimeOffset;

    return S_OK;
}

STDMETHODIMP BIOSSettings::COMSETTER(TimeOffset)(LONG64 offset)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    mData.backup();
    mData->mTimeOffset = offset;

    return S_OK;
}


// IBIOSSettings methods
/////////////////////////////////////////////////////////////////////////////

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

void BIOSSettings::commit()
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

void BIOSSettings::copyFrom (BIOSSettings *aThat)
{
    AutoLock alock (this);

    // this will back up current data
    mData.assignCopy (aThat->mData);
}

