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

#include "GuestOSTypeImpl.h"
#include "Logging.h"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

GuestOSType::GuestOSType()
{
    mOSType = OSTypeUnknown;
}

GuestOSType::~GuestOSType()
{
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the guest OS type object.
 *
 * @returns COM result indicator
 * @param id          os short name string
 * @param description os name string
 * @param osType      global OS type ID
 * @param ramSize     recommended RAM size in megabytes
 * @param vramSize    recommended video memory size in megabytes
 * @param hddSize     recommended HDD size in megabytes
 */
HRESULT GuestOSType::init (const char *id, const char *description, OSType osType,
                           uint32_t ramSize, uint32_t vramSize, uint32_t hddSize)
{
    ComAssertRet (id && description, E_INVALIDARG);

    AutoLock lock(this);
    mID = id;
    mDescription = description;
    mOSType = osType;
    mRAMSize = ramSize;
    mVRAMSize = vramSize;
    mHDDSize = hddSize;

    LogFlowMember (("GuestOSType::init(): id='%ls', desc='%ls', type=%d, "
                    "ram=%d, vram=%d, hdd=%d\n",
                    mID.raw(), mDescription.raw(), mOSType,
                    mRAMSize, mVRAMSize, mHDDSize));

    setReady(true);
    return S_OK;
}

// IGuestOSType properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the ID string
 *
 * @returns COM status code
 * @param id address of result variable
 */
STDMETHODIMP GuestOSType::COMGETTER(Id) (BSTR *id)
{
    if (!id)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();

    mID.cloneTo(id);
    return S_OK;
}

/**
 * Returns the description string
 *
 * @returns COM status code
 * @param description address of result variable
 */
STDMETHODIMP GuestOSType::COMGETTER(Description) (BSTR *description)
{
    if (!description)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();

    mDescription.cloneTo(description);
    return S_OK;
}

/**
 * Returns the recommended RAM size in megabytes
 *
 * @returns COM status code
 * @param description address of result variable
 */
STDMETHODIMP GuestOSType::COMGETTER(RecommendedRAM) (ULONG *ramSize)
{
    if (!ramSize)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();

    *ramSize = mRAMSize;
    return S_OK;
}

/**
 * Returns the recommended video RAM size in megabytes
 *
 * @returns COM status code
 * @param description address of result variable
 */
STDMETHODIMP GuestOSType::COMGETTER(RecommendedVRAM) (ULONG *vramSize)
{
    if (!vramSize)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();

    *vramSize = mVRAMSize;
    return S_OK;
}

/**
 * Returns the recommended HDD size in megabytes
 *
 * @returns COM status code
 * @param description address of result variable
 */
STDMETHODIMP GuestOSType::COMGETTER(RecommendedHDD) (ULONG *hddSize)
{
    if (!hddSize)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();

    *hddSize = mHDDSize;
    return S_OK;
}
