/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include "USBDeviceImpl.h"


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (OUSBDevice)

HRESULT OUSBDevice::FinalConstruct()
{
    return S_OK;
}

void OUSBDevice::FinalRelease()
{
    uninit ();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the USB device object.
 *
 * @returns COM result indicator
 * @param   aUSBDevice    The USB device (interface) to clone.
 */
HRESULT OUSBDevice::init(IUSBDevice *aUSBDevice)
{
    LogFlowThisFunc (("aUSBDevice=%p\n", aUSBDevice));

    ComAssertRet (aUSBDevice, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    HRESULT hrc = aUSBDevice->COMGETTER(VendorId)(&unconst (mData.vendorId));
    ComAssertComRCRet (hrc, hrc);
    ComAssertRet (mData.vendorId, E_INVALIDARG);

    hrc = aUSBDevice->COMGETTER(ProductId)(&unconst (mData.productId));
    ComAssertComRCRet (hrc, hrc);
    ComAssertRet (mData.productId, E_INVALIDARG);

    hrc = aUSBDevice->COMGETTER(Revision)(&unconst (mData.revision));
    ComAssertComRCRet (hrc, hrc);

    hrc = aUSBDevice->COMGETTER(Manufacturer)(unconst (mData.manufacturer).asOutParam());
    ComAssertComRCRet (hrc, hrc);

    hrc = aUSBDevice->COMGETTER(Product)(unconst (mData.product).asOutParam());
    ComAssertComRCRet (hrc, hrc);

    hrc = aUSBDevice->COMGETTER(SerialNumber)(unconst (mData.serialNumber).asOutParam());
    ComAssertComRCRet (hrc, hrc);

    hrc = aUSBDevice->COMGETTER(Address)(unconst (mData.address).asOutParam());
    ComAssertComRCRet (hrc, hrc);

    hrc = aUSBDevice->COMGETTER(Port)(&unconst (mData.port));
    ComAssertComRCRet (hrc, hrc);

    hrc = aUSBDevice->COMGETTER(Port)(&unconst (mData.version));
    ComAssertComRCRet (hrc, hrc);

    hrc = aUSBDevice->COMGETTER(Port)(&unconst (mData.portVersion));
    ComAssertComRCRet (hrc, hrc);

    hrc = aUSBDevice->COMGETTER(Remote)(&unconst (mData.remote));
    ComAssertComRCRet (hrc, hrc);

    Bstr id;
    hrc = aUSBDevice->COMGETTER(Id)(id.asOutParam());
    ComAssertComRCRet (hrc, hrc);
    unconst(mData.id) = Guid(id);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void OUSBDevice::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst (mData.id).clear();

    unconst (mData.vendorId) = 0;
    unconst (mData.productId) = 0;
    unconst (mData.revision) = 0;

    unconst (mData.manufacturer).setNull();
    unconst (mData.product).setNull();
    unconst (mData.serialNumber).setNull();

    unconst (mData.address).setNull();

    unconst (mData.port) = 0;
    unconst (mData.version) = 1;
    unconst (mData.portVersion) = 1;

    unconst (mData.remote) = FALSE;
}

// IUSBDevice properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the GUID.
 *
 * @returns COM status code
 * @param   aId   Address of result variable.
 */
STDMETHODIMP OUSBDevice::COMGETTER(Id)(BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    Guid(mData.id).toString().cloneTo (aId);

    return S_OK;
}


/**
 * Returns the vendor Id.
 *
 * @returns COM status code
 * @param   aVendorId   Where to store the vendor id.
 */
STDMETHODIMP OUSBDevice::COMGETTER(VendorId)(USHORT *aVendorId)
{
    CheckComArgOutPointerValid(aVendorId);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    *aVendorId = mData.vendorId;

    return S_OK;
}


/**
 * Returns the product Id.
 *
 * @returns COM status code
 * @param   aProductId  Where to store the product id.
 */
STDMETHODIMP OUSBDevice::COMGETTER(ProductId)(USHORT *aProductId)
{
    CheckComArgOutPointerValid(aProductId);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    *aProductId = mData.productId;

    return S_OK;
}


/**
 * Returns the revision BCD.
 *
 * @returns COM status code
 * @param   aRevision  Where to store the revision BCD.
 */
STDMETHODIMP OUSBDevice::COMGETTER(Revision)(USHORT *aRevision)
{
    CheckComArgOutPointerValid(aRevision);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    *aRevision = mData.revision;

    return S_OK;
}

/**
 * Returns the manufacturer string.
 *
 * @returns COM status code
 * @param   aManufacturer     Where to put the return string.
 */
STDMETHODIMP OUSBDevice::COMGETTER(Manufacturer)(BSTR *aManufacturer)
{
    CheckComArgOutPointerValid(aManufacturer);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    mData.manufacturer.cloneTo (aManufacturer);

    return S_OK;
}


/**
 * Returns the product string.
 *
 * @returns COM status code
 * @param   aProduct          Where to put the return string.
 */
STDMETHODIMP OUSBDevice::COMGETTER(Product)(BSTR *aProduct)
{
    CheckComArgOutPointerValid(aProduct);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    mData.product.cloneTo (aProduct);

    return S_OK;
}


/**
 * Returns the serial number string.
 *
 * @returns COM status code
 * @param   aSerialNumber     Where to put the return string.
 */
STDMETHODIMP OUSBDevice::COMGETTER(SerialNumber)(BSTR *aSerialNumber)
{
    CheckComArgOutPointerValid(aSerialNumber);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    mData.serialNumber.cloneTo (aSerialNumber);

    return S_OK;
}


/**
 * Returns the host specific device address.
 *
 * @returns COM status code
 * @param   aAddress          Where to put the return string.
 */
STDMETHODIMP OUSBDevice::COMGETTER(Address)(BSTR *aAddress)
{
    CheckComArgOutPointerValid(aAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    mData.address.cloneTo (aAddress);

    return S_OK;
}

STDMETHODIMP OUSBDevice::COMGETTER(Port)(USHORT *aPort)
{
    CheckComArgOutPointerValid(aPort);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    *aPort = mData.port;

    return S_OK;
}

STDMETHODIMP OUSBDevice::COMGETTER(Version)(USHORT *aVersion)
{
    CheckComArgOutPointerValid(aVersion);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    *aVersion = mData.version;

    return S_OK;
}

STDMETHODIMP OUSBDevice::COMGETTER(PortVersion)(USHORT *aPortVersion)
{
    CheckComArgOutPointerValid(aPortVersion);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    *aPortVersion = mData.portVersion;

    return S_OK;
}

STDMETHODIMP OUSBDevice::COMGETTER(Remote)(BOOL *aRemote)
{
    CheckComArgOutPointerValid(aRemote);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    *aRemote = mData.remote;

    return S_OK;
}

// private methods
/////////////////////////////////////////////////////////////////////////////
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
