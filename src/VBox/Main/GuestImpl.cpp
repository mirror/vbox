/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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

HRESULT Guest::FinalConstruct()
{
    return S_OK;
}

void Guest::FinalRelease()
{
    if (isReady())
        uninit ();
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the guest object.
 */
HRESULT Guest::init (Console *aParent)
{
    LogFlowMember (("Guest::init (%p)\n", aParent));

    ComAssertRet (aParent, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = aParent;

    mData.allocate();
    // mData.mAdditionsActive is FALSE

    setReady (true);
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Guest::uninit()
{
    LogFlowMember (("Guest::uninit()\n"));

    AutoLock alock (this);
    AssertReturn (isReady(), (void) 0);

    mData.free();

    mParent.setNull();

    setReady (false);
}

// IGuest properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the assigned guest OS type
 *
 * @returns COM status code
 * @param guestOSType address of result variable
 */
STDMETHODIMP Guest::COMGETTER(OSType) (IGuestOSType **aOSType)
{
    if (!aOSType)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    // redirect the call to IMachine if no additions are installed
    if (mData->mAdditionsVersion.isNull())
        return mParent->machine()->COMGETTER(OSType) (aOSType);

    mData->mGuestOSType.queryInterfaceTo (aOSType);
    return S_OK;
}

/**
 * Returns whether the Guest Additions are currently active.
 *
 * @returns COM status code
 * @param   active address of result variable
 */
STDMETHODIMP Guest::COMGETTER(AdditionsActive) (BOOL *aAdditionsActive)
{
    if (!aAdditionsActive)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aAdditionsActive = mData->mAdditionsActive;
    return S_OK;
}

/**
 * Returns the reported Guest Additions version. Empty
 * string if they are not installed.
 *
 * @returns COM status code
 * @param   version address of result variable
 */
STDMETHODIMP Guest::COMGETTER(AdditionsVersion) (BSTR *aAdditionsVersion)
{
    if (!aAdditionsVersion)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mAdditionsVersion.cloneTo (aAdditionsVersion);
    return S_OK;
}

/**
 * Set guest logon credentials. They can be retrieved by the
 * guest Additions. It's up to the guest what it does with them.
 * Note that this has been placed here to make it transient
 * information.
 */
STDMETHODIMP Guest::SetCredentials(INPTR BSTR aUserName, INPTR BSTR aPassword,
                                   INPTR BSTR aDomain, BOOL aAllowInteractiveLogon)
{
    if (!aUserName || !aPassword || !aDomain)
        return E_INVALIDARG;

    AutoLock alock(this);
    CHECK_READY();

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
    return setError(E_FAIL, tr("VMM device not available, VM not running"));
}


// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

void Guest::setAdditionsVersion(Bstr version)
{
    if (!version)
        return;
    AutoLock alock(this);
    mData->mAdditionsVersion = version;
    // this implies that Additions are active
    mData->mAdditionsActive = true;
}
