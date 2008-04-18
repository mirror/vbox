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

#include "HostFloppyDriveImpl.h"
#include <iprt/cpputils.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (HostFloppyDrive)

HRESULT HostFloppyDrive::FinalConstruct()
{
    return S_OK;
}

void HostFloppyDrive::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the host floppy drive object.
 *
 * @param aName         Name of the drive.
 * @param aUdi          Universal device identifier (currently may be NULL).
 * @param aDescription  Human-readable drive description (may be NULL).
 *
 * @return COM result indicator.
 */
HRESULT HostFloppyDrive::init (INPTR BSTR aName,
                               INPTR BSTR aUdi /* = NULL */,
                               INPTR BSTR aDescription /* = NULL */)
{
    ComAssertRet (aName, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mName) = aName;
    unconst (mUdi) = aUdi;
    unconst (mDescription) = aDescription;

    /* Confirm the successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void HostFloppyDrive::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst (mName).setNull();
}

// IHostFloppyDrive properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HostFloppyDrive::COMGETTER(Name) (BSTR *aName)
{
    if (!aName)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mName is constant during life time, no need to lock */

    mName.cloneTo (aName);

    return S_OK;
}

STDMETHODIMP HostFloppyDrive::COMGETTER(Description) (BSTR *aDescription)
{
    if (!aDescription)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mDescription is constant during life time, no need to lock */

    mDescription.cloneTo (aDescription);

    return S_OK;
}

STDMETHODIMP HostFloppyDrive::COMGETTER(Udi) (BSTR *aUdi)
{
    if (!aUdi)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mUdi is constant during life time, no need to lock */

    mUdi.cloneTo (aUdi);

    return S_OK;
}
