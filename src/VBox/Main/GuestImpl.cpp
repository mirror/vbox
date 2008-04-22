/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#include "GuestImpl.h"
#include "ConsoleImpl.h"
#include "VMMDev.h"

#include "Logging.h"

#include <VBox/VBoxDev.h>
#include <iprt/cpputils.h>

// defines
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (Guest)

HRESULT Guest::FinalConstruct()
{
    return S_OK;
}

void Guest::FinalRelease()
{
    uninit ();
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the guest object.
 */
HRESULT Guest::init (Console *aParent)
{
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;

    /* mData.mAdditionsActive is FALSE */

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    ULONG aMemoryBalloonSize;
    HRESULT ret = mParent->machine()->COMGETTER(MemoryBalloonSize)(&aMemoryBalloonSize);
    if (ret == S_OK)
        mMemoryBalloonSize = aMemoryBalloonSize;
    else
        mMemoryBalloonSize = 0;                     /* Default is no ballooning */

    ULONG aStatUpdateInterval;
    ret = mParent->machine()->COMGETTER(StatisticsUpdateInterval)(&aStatUpdateInterval);
    if (ret == S_OK)
        mStatUpdateInterval = aStatUpdateInterval;
    else
        mStatUpdateInterval = 0;                    /* Default is not to report guest statistics at all */

    /* invalidate all stats */
    for (int i=0;i<GuestStatisticType_MaxVal;i++)
        mCurrentGuestStat[i] = GUEST_STAT_INVALID;

    /* start with sample 0 */
    mCurrentGuestStat[GuestStatisticType_SampleNumber] = 0;
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Guest::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst (mParent).setNull();
}

// IGuest properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Guest::COMGETTER(OSTypeId) (BSTR *aOSTypeId)
{
    if (!aOSTypeId)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    // redirect the call to IMachine if no additions are installed
    if (mData.mAdditionsVersion.isNull())
        return mParent->machine()->COMGETTER(OSTypeId) (aOSTypeId);

    mData.mOSTypeId.cloneTo (aOSTypeId);

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(AdditionsActive) (BOOL *aAdditionsActive)
{
    if (!aAdditionsActive)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aAdditionsActive = mData.mAdditionsActive;

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(AdditionsVersion) (BSTR *aAdditionsVersion)
{
    if (!aAdditionsVersion)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mData.mAdditionsVersion.cloneTo (aAdditionsVersion);

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(SupportsSeamless) (BOOL *aSupportsSeamless)
{
    if (!aSupportsSeamless)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aSupportsSeamless = mData.mSupportsSeamless;

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(SupportsGraphics) (BOOL *aSupportsGraphics)
{
    if (!aSupportsGraphics)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aSupportsGraphics = mData.mSupportsGraphics;

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(MemoryBalloonSize) (ULONG *aMemoryBalloonSize)
{
    if (!aMemoryBalloonSize)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aMemoryBalloonSize = mMemoryBalloonSize;

    return S_OK;
}

STDMETHODIMP Guest::COMSETTER(MemoryBalloonSize) (ULONG aMemoryBalloonSize)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT ret = mParent->machine()->COMSETTER(MemoryBalloonSize)(aMemoryBalloonSize);
    if (ret == S_OK)
    {
        mMemoryBalloonSize = aMemoryBalloonSize;
        /* forward the information to the VMM device */
        VMMDev *vmmDev = mParent->getVMMDev();
        if (vmmDev)
            vmmDev->getVMMDevPort()->pfnSetMemoryBalloon(vmmDev->getVMMDevPort(), aMemoryBalloonSize);
    }

    return ret;
}

STDMETHODIMP Guest::COMGETTER(StatisticsUpdateInterval) (ULONG *aUpdateInterval)
{
    if (!aUpdateInterval)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aUpdateInterval = mStatUpdateInterval;

    return S_OK;
}

STDMETHODIMP Guest::COMSETTER(StatisticsUpdateInterval) (ULONG aUpdateInterval)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT ret = mParent->machine()->COMSETTER(StatisticsUpdateInterval)(aUpdateInterval);
    if (ret == S_OK)
    {
        mStatUpdateInterval = aUpdateInterval;
        /* forward the information to the VMM device */
        VMMDev *vmmDev = mParent->getVMMDev();
        if (vmmDev)
            vmmDev->getVMMDevPort()->pfnSetStatisticsInterval(vmmDev->getVMMDevPort(), aUpdateInterval);
    }

    return ret;
}

STDMETHODIMP Guest::SetCredentials(INPTR BSTR aUserName, INPTR BSTR aPassword,
                                   INPTR BSTR aDomain, BOOL aAllowInteractiveLogon)
{
    if (!aUserName || !aPassword || !aDomain)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* forward the information to the VMM device */
    VMMDev *vmmDev = mParent->getVMMDev();
    if (vmmDev)
    {
        uint32_t u32Flags = VMMDEV_SETCREDENTIALS_GUESTLOGON;
        if (!aAllowInteractiveLogon)
            u32Flags = VMMDEV_SETCREDENTIALS_NOLOCALLOGON;

        vmmDev->getVMMDevPort()->pfnSetCredentials(vmmDev->getVMMDevPort(),
                                                   Utf8Str(aUserName).raw(), Utf8Str(aPassword).raw(),
                                                   Utf8Str(aDomain).raw(), u32Flags);
        return S_OK;
    }

    return setError (E_FAIL,
        tr ("VMM device is not available (is the VM running?)"));
}

STDMETHODIMP Guest::GetStatistic(ULONG aCpuId, GuestStatisticType_T aStatistic, ULONG *aStatVal)
{
    if (!aStatVal)
        return E_INVALIDARG;

    if (aCpuId != 0)
        return E_INVALIDARG;

    if (aStatistic >= GuestStatisticType_MaxVal)
        return E_INVALIDARG;

    /* not available or not yet reported? */
    if (mCurrentGuestStat[aStatistic] == GUEST_STAT_INVALID)
        return E_INVALIDARG;

    *aStatVal = mCurrentGuestStat[aStatistic];
    return S_OK;
}

STDMETHODIMP Guest::SetStatistic(ULONG aCpuId, GuestStatisticType_T aStatistic, ULONG aStatVal)
{
    if (aCpuId != 0)
        return E_INVALIDARG;

    if (aStatistic >= GuestStatisticType_MaxVal)
        return E_INVALIDARG;

    /* internal method assumes that the caller known what he's doing (no boundary checks) */
    mCurrentGuestStat[aStatistic] = aStatVal;
    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

void Guest::setAdditionsVersion (Bstr aVersion)
{
    AssertReturnVoid (!aVersion.isEmpty());

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock (this);

    mData.mAdditionsVersion = aVersion;
    /* this implies that Additions are active */
    mData.mAdditionsActive = TRUE;
}

void Guest::setMaxGuestResolution (ULONG aMaxWidth, ULONG aMaxHeight)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock (this);

    mData.mMaxWidth  = aMaxWidth;
    mData.mMaxHeight = aMaxHeight;
}

void Guest::setSupportsSeamless (BOOL aSupportsSeamless)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock (this);

    mData.mSupportsSeamless = aSupportsSeamless;
}

void Guest::setSupportsGraphics (BOOL aSupportsGraphics)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock (this);

    mData.mSupportsGraphics = aSupportsGraphics;
}
