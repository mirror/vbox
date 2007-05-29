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

#include "HostDVDDriveImpl.h"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HostDVDDrive::HostDVDDrive()
{
}

HostDVDDrive::~HostDVDDrive()
{
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the host object.
 *
 * @returns COM result indicator
 * @param driveName name of the drive
 */
HRESULT HostDVDDrive::init (INPTR BSTR driveName)
{
    ComAssertRet (driveName, E_INVALIDARG);

    AutoLock lock(this);
    mDriveName = driveName;
    setReady(true);
    return S_OK;
}

/**
 * Initializes the host object.
 *
 * @returns COM result indicator
 * @param driveName        name of the drive
 * @param driveDescription human readable description of the drive
 */
HRESULT HostDVDDrive::init (INPTR BSTR driveName, INPTR BSTR driveDescription)
{
    ComAssertRet (driveName, E_INVALIDARG);
    ComAssertRet (driveDescription, E_INVALIDARG);

    AutoLock lock(this);
    mDriveName = driveName;
    mDriveDescription = driveDescription;
    setReady(true);
    return S_OK;
}

// IHostDVDDrive properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the name of the host drive
 *
 * @returns COM status code
 * @param driveName address of result pointer
 */
STDMETHODIMP HostDVDDrive::COMGETTER(Name) (BSTR *driveName)
{
    if (!driveName)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();
    mDriveName.cloneTo(driveName);
    return S_OK;
}

/**
 * Returns the description of the host drive
 *
 * @returns COM status code
 * @param driveDescription address of result pointer
 */
STDMETHODIMP HostDVDDrive::COMGETTER(Description) (BSTR *driveDescription)
{
    if (!driveDescription)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();
    mDriveDescription.cloneTo(driveDescription);
    return S_OK;
}
