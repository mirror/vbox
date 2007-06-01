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

#include "GuestImpl.h"
#include "ConsoleImpl.h"
#include "VMMDev.h"

#include "Logging.h"

#include <VBox/VBoxDev.h>

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

    AutoReaderLock alock (this);

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

    AutoReaderLock alock (this);

    *aAdditionsActive = mData.mAdditionsActive;

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(AdditionsVersion) (BSTR *aAdditionsVersion)
{
    if (!aAdditionsVersion)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData.mAdditionsVersion.cloneTo (aAdditionsVersion);

    return S_OK;
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


// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

void Guest::setAdditionsVersion (Bstr aVersion)
{
    AssertReturnVoid (!aVersion.isEmpty());

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoLock alock (this);

    mData.mAdditionsVersion = aVersion;
    /* this implies that Additions are active */
    mData.mAdditionsActive = TRUE;
}
