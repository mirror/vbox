/* $Id$ */

/** @file
 *
 * VirtualBox IHostUSBDevice COM interface implementation
 * for remote (VRDP) USB devices
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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

#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/cpp/utils.h>

#include <VBox/err.h>

#include <VBox/RemoteDesktop/VRDE.h>
#include <VBox/vrdpusb.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (RemoteUSBDevice)

HRESULT RemoteUSBDevice::FinalConstruct()
{
    return BaseFinalConstruct();
}

void RemoteUSBDevice::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/** @todo (sunlover) REMOTE_USB Device states. */

/**
 * Initializes the remote USB device object.
 */
HRESULT RemoteUSBDevice::init (uint32_t u32ClientId, VRDEUSBDEVICEDESC *pDevDesc, bool fDescExt)
{
    LogFlowThisFunc(("u32ClientId=%d,pDevDesc=%p\n", u32ClientId, pDevDesc));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mData.id).create();

    unconst(mData.vendorId)     = pDevDesc->idVendor;
    unconst(mData.productId)    = pDevDesc->idProduct;
    unconst(mData.revision)     = pDevDesc->bcdRev;

    unconst(mData.manufacturer) = pDevDesc->oManufacturer? (char *)pDevDesc + pDevDesc->oManufacturer: "";
    unconst(mData.product)      = pDevDesc->oProduct? (char *)pDevDesc + pDevDesc->oProduct: "";
    unconst(mData.serialNumber) = pDevDesc->oSerialNumber? (char *)pDevDesc + pDevDesc->oSerialNumber: "";

    char id[64];
    RTStrPrintf(id, sizeof (id), REMOTE_USB_BACKEND_PREFIX_S "0x%08X&0x%08X", pDevDesc->id, u32ClientId);
    unconst(mData.address)      = id;

    unconst(mData.port)         = pDevDesc->idPort;
    unconst(mData.version)      = pDevDesc->bcdUSB >> 8;
    if (fDescExt)
    {
        VRDEUSBDEVICEDESCEXT *pDevDescExt = (VRDEUSBDEVICEDESCEXT *)pDevDesc;
        switch (pDevDescExt->u16DeviceSpeed)
        {
            default:
            case VRDE_USBDEVICESPEED_UNKNOWN:
            case VRDE_USBDEVICESPEED_LOW:
            case VRDE_USBDEVICESPEED_FULL:
                unconst(mData.portVersion) = 1;
                break;

            case VRDE_USBDEVICESPEED_HIGH:
            case VRDE_USBDEVICESPEED_VARIABLE:
            case VRDE_USBDEVICESPEED_SUPERSPEED:
                unconst(mData.portVersion) = 2;
                break;
        }
    }
    else
    {
        unconst(mData.portVersion)  = mData.version;
    }

    mData.state                  = USBDeviceState_Available;

    mData.dirty                  = false;
    unconst(mData.devId)        = pDevDesc->id;

    unconst(mData.clientId)     = u32ClientId;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void RemoteUSBDevice::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(mData.id).clear();

    unconst(mData.vendorId) = 0;
    unconst(mData.productId) = 0;
    unconst(mData.revision) = 0;

    unconst(mData.manufacturer).setNull();
    unconst(mData.product).setNull();
    unconst(mData.serialNumber).setNull();

    unconst(mData.address).setNull();

    unconst(mData.port) = 0;
    unconst(mData.version) = 1;
    unconst(mData.portVersion) = 1;

    unconst(mData.dirty) = FALSE;

    unconst(mData.devId) = 0;
    unconst(mData.clientId) = 0;
}

// IUSBDevice properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP RemoteUSBDevice::COMGETTER(Id) (BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    mData.id.toUtf16().detachTo(aId);

    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(VendorId) (USHORT *aVendorId)
{
    CheckComArgOutPointerValid(aVendorId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    *aVendorId = mData.vendorId;

    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(ProductId) (USHORT *aProductId)
{
    CheckComArgOutPointerValid(aProductId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    *aProductId = mData.productId;

    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Revision) (USHORT *aRevision)
{
    CheckComArgOutPointerValid(aRevision);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    *aRevision = mData.revision;

    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Manufacturer) (BSTR *aManufacturer)
{
    CheckComArgOutPointerValid(aManufacturer);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    mData.manufacturer.cloneTo(aManufacturer);

    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Product) (BSTR *aProduct)
{
    CheckComArgOutPointerValid(aProduct);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    mData.product.cloneTo(aProduct);

    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(SerialNumber) (BSTR *aSerialNumber)
{
    CheckComArgOutPointerValid(aSerialNumber);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    mData.serialNumber.cloneTo(aSerialNumber);

    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Address) (BSTR *aAddress)
{
    CheckComArgOutPointerValid(aAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    mData.address.cloneTo(aAddress);

    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Port) (USHORT *aPort)
{
    CheckComArgOutPointerValid(aPort);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    *aPort = mData.port;

    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Version) (USHORT *aVersion)
{
    CheckComArgOutPointerValid(aVersion);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    *aVersion = mData.version;

    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(PortVersion) (USHORT *aPortVersion)
{
    CheckComArgOutPointerValid(aPortVersion);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    *aPortVersion = mData.portVersion;

    return S_OK;
}

STDMETHODIMP RemoteUSBDevice::COMGETTER(Remote) (BOOL *aRemote)
{
    CheckComArgOutPointerValid(aRemote);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* RemoteUSBDevice is always remote. */
    /* this is const, no need to lock */
    *aRemote = TRUE;

    return S_OK;
}

// IHostUSBDevice properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP RemoteUSBDevice::COMGETTER(State) (USBDeviceState_T *aState)
{
    CheckComArgOutPointerValid(aState);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aState = mData.state;

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
