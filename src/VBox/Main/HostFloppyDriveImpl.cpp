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

#include "HostFloppyDriveImpl.h"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HostFloppyDrive::HostFloppyDrive()
{
}

HostFloppyDrive::~HostFloppyDrive()
{
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the host floppy drive object.
 *
 * @returns COM result indicator
 * @param driveName name of the drive
 */
HRESULT HostFloppyDrive::init (INPTR BSTR driveName)
{
    ComAssertRet (driveName, E_INVALIDARG);

    AutoLock lock(this);
    mDriveName = driveName;
    setReady(true);
    return S_OK;
}

// IHostFloppyDrive properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the name of the host drive
 *
 * @returns COM status code
 * @param driveName address of result pointer
 */
STDMETHODIMP HostFloppyDrive::COMGETTER(Name) (BSTR *driveName)
{
    if (!driveName)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();
    mDriveName.cloneTo(driveName);
    return S_OK;
}
