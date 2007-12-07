/** @file
 *
 * VirtualBox IHostUSBDevice COM interface implementation
 * for remote (VRDP) USB devices
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

#include "RemoteUSBDeviceImpl.h"
#include "Logging.h"

#include <VBox/err.h>

#include <VBox/vrdpusb.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (RemoteUSBDevice)

HRESULT RemoteUSBDevice::FinalConstruct()
{
    return S_OK;
}

void RemoteUSBDevice::FinalRelease()
{
    if (isReady())
        uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/** @todo (sunlover) REMOTE_USB Device states. */

/**
 * Initializes the remote USB device object.
 */
HRESULT RemoteUSBDevice::init (uint32_t u32ClientId, VRDPUSBDEVICEDESC *pDevDesc)
{
    LogFlowMember (("RemoteUSBDevice::init()\n"));

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mId.create();

    mVendorId     = pDevDesc->idVendor;
    mProductId    = pDevDesc->idProduct;
    mRevision     = pDevDesc->bcdRev;

    mManufacturer = pDevDesc->oManufacturer? (char *)pDevDesc + pDevDesc->oManufacturer: "";
    mProduct      = pDevDesc->oProduct? (char *)pDevDesc + pDevDesc->oProduct: "";
    mSerialNumber = pDevDesc->oSerialNumber? (char *)pDevDesc + pDevDesc->oSerialNumber: "";

    char id[64];
    RTStrPrintf(id, sizeof (id), REMOTE_USB_BACKEND_PREFIX_S "0x%08X&0x%08X", pDevDesc->id, u32ClientId);
    mAddress      = id;

    mPort         = pDevDesc->idPort;
    mVersion      = pDevDesc->bcdUSB >> 8;
    mPortVersion  = mVersion; /** @todo fix this */

    mState        = USBDeviceState_USBDeviceAvailable;

    mDirty        = false;
    mDevId        = pDevDesc->id;

    mClientId     = u32ClientId;

    setReady (true);
    return S_OK;
}


/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void RemoteUSBDevice::uninit()
{
    LogFlowMember (("RemoteUSBDevice::uninit()\n"));

    AutoLock alock (this);
    AssertReturn (isReady(), (void) 0);

    setReady (false);
}

// IUSBDevice properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP RemoteUSBDevice::COMGETTER(Id) (GUIDPARAMOUT aId)
{
    if (!aId)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    mId.cloneTo (aId);
    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(VendorId) (USHORT *aVendorId)
{
    if (!aVendorId)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aVendorId = mVendorId;
    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(ProductId) (USHORT *aProductId)
{
    if (!aProductId)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aProductId = mProductId;
    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Revision) (USHORT *aRevision)
{
    if (!aRevision)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aRevision = mRevision;
    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Manufacturer) (BSTR *aManufacturer)
{
    if (!aManufacturer)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    mManufacturer.cloneTo (aManufacturer);
    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Product) (BSTR *aProduct)
{
    if (!aProduct)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    mProduct.cloneTo (aProduct);
    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(SerialNumber) (BSTR *aSerialNumber)
{
    if (!aSerialNumber)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    mSerialNumber.cloneTo (aSerialNumber);
    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Address) (BSTR *aAddress)
{
    if (!aAddress)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    mAddress.cloneTo (aAddress);
    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Port) (USHORT *aPort)
{
    if (!aPort)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aPort = mPort;
    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Version) (USHORT *aVersion)
{
    if (!aVersion)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aVersion = mVersion;
    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(PortVersion) (USHORT *aPortVersion)
{
    if (!aPortVersion)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aPortVersion = mPortVersion;
    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Remote) (BOOL *aRemote)
{
    if (!aRemote)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    /* RemoteUSBDevice is always remote. */
    *aRemote = TRUE;
    return S_OK;
}

// IHostUSBDevice properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP RemoteUSBDevice::COMGETTER(State) (USBDeviceState_T *aState)
{
    if (!aState)
        return E_POINTER;

    AutoLock lock (this);
    CHECK_READY();

    *aState = mState;
    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////
