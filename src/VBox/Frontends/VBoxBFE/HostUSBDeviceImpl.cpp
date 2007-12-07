/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Implementation of HostUSBDevice
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

#include "HostUSBDeviceImpl.h"
#include "USBProxyService.h"
#include "Logging.h"

#include <VBox/err.h>


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HostUSBDevice::HostUSBDevice()
    : mUSBProxyService (NULL), m_pUsb (NULL)
{
}

HostUSBDevice::~HostUSBDevice()
{
    if (m_pUsb)
    {
        USBProxyService::freeDevice (m_pUsb);
        m_pUsb = NULL;
    }
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the USB device object.
 *
 * @returns COM result indicator
 * @param   aUsb                Pointer to the usb device structure for which the object is to be a wrapper.
 *                              This structure is now fully owned by the HostUSBDevice object and will be
 *                              freed when it is destructed.
 * @param   aUSBProxyService    Pointer to the USB Proxy Service object.
 */
HRESULT HostUSBDevice::init(PUSBDEVICE aUsb, USBProxyService *aUSBProxyService)
{
    ComAssertRet (aUsb, E_INVALIDARG);

    AutoLock alock (this);

    /*
     * We need a unique ID for this VBoxSVC session.
     * The UUID isn't stored anywhere.
     */
    RTUuidCreate(&mId);

    /*
     * Convert from USBDEVICESTATE to USBDeviceState.
     *
     * Note that not all proxy backend can detect the HELD_BY_PROXY
     * and USED_BY_GUEST states. But that shouldn't matter much.
     */
    switch (aUsb->enmState)
    {
        default:
            AssertMsgFailed(("aUsb->enmState=%d\n", aUsb->enmState));
        case USBDEVICESTATE_UNSUPPORTED:
            mState = USBDeviceState_USBDeviceNotSupported;
            break;
        case USBDEVICESTATE_USED_BY_HOST:
            mState = USBDeviceState_USBDeviceUnavailable;
            break;
        case USBDEVICESTATE_USED_BY_HOST_CAPTURABLE:
            mState = USBDeviceState_USBDeviceBusy;
            break;
        case USBDEVICESTATE_UNUSED:
            mState = USBDeviceState_USBDeviceAvailable;
            break;
        case USBDEVICESTATE_HELD_BY_PROXY:
            mState = USBDeviceState_USBDeviceHeld;
            break;
        case USBDEVICESTATE_USED_BY_GUEST:
            mState = USBDeviceState_USBDeviceCaptured;
            break;
    }

    /*
     * Other data members.
     */
    mIgnored = false;
    mUSBProxyService = aUSBProxyService;
    m_pUsb = aUsb;

    setReady (true);
    return S_OK;
}

// IUSBDevice properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the GUID.
 *
 * @returns COM status code
 * @param   aId     Address of result variable.
 */
STDMETHODIMP HostUSBDevice::COMGETTER(Id)(RTUUID &aId)
{
/*     AutoLock alock (this); */
    CHECK_READY();

    aId = mId;
    return S_OK;
}


/**
 * Returns the vendor Id.
 *
 * @returns COM status code
 * @param   aVendorId   Where to store the vendor id.
 */
STDMETHODIMP HostUSBDevice::COMGETTER(VendorId)(USHORT *aVendorId)
{
    if (!aVendorId)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aVendorId = m_pUsb->idVendor;
    return S_OK;
}


/**
 * Returns the product Id.
 *
 * @returns COM status code
 * @param   aProductId      Where to store the product id.
 */
STDMETHODIMP HostUSBDevice::COMGETTER(ProductId)(USHORT *aProductId)
{
    if (!aProductId)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aProductId = m_pUsb->idProduct;
    return S_OK;
}


/**
 * Returns the revision BCD.
 *
 * @returns COM status code
 * @param   aRevision       Where to store the revision BCD.
 */
STDMETHODIMP HostUSBDevice::COMGETTER(Revision)(USHORT *aRevision)
{
    if (!aRevision)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aRevision = m_pUsb->bcdDevice;
    return S_OK;
}

/**
 * Returns the manufacturer string.
 *
 * @returns COM status code
 * @param   aManufacturer       Where to put the return string.
 */
STDMETHODIMP HostUSBDevice::COMGETTER(Manufacturer)
                                (std::string *aManufacturer)
{
    if (!aManufacturer)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aManufacturer = m_pUsb->pszManufacturer;
    return S_OK;
}


/**
 * Returns the product string.
 *
 * @returns COM status code
 * @param   aProduct            Where to put the return string.
 */
STDMETHODIMP HostUSBDevice::COMGETTER(Product)(std::string *aProduct)
{
    if (!aProduct)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aProduct = m_pUsb->pszProduct;
    return S_OK;
}


/**
 * Returns the serial number string.
 *
 * @returns COM status code
 * @param   aSerialNumber     Where to put the return string.
 */
STDMETHODIMP HostUSBDevice::COMGETTER(SerialNumber)
                                (std::string *aSerialNumber)
{
    if (!aSerialNumber)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aSerialNumber = m_pUsb->pszSerialNumber;
    return S_OK;
}

/**
 * Returns the device address string.
 *
 * @returns COM status code
 * @param   aAddress            Where to put the returned string.
 */
STDMETHODIMP HostUSBDevice::COMGETTER(Address)(std::string *aAddress)
{
    if (!aAddress)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aAddress = m_pUsb->pszAddress;
    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(Port)(USHORT *aPort)
{
    if (!aPort)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    ///@todo implement
    aPort = 0;
    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(Remote)(BOOL *aRemote)
{
    if (!aRemote)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    *aRemote = FALSE;
    return S_OK;
}

// IHostUSBDevice properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HostUSBDevice::COMGETTER(State) (USBDeviceState_T *aState)
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

/** Sets the ignored flag and returns the device to the host */
void HostUSBDevice::setIgnored()
{
    AutoLock alock (this);
    AssertReturn (isReady(), (void) 0);

    AssertReturn (!mIgnored, (void) 0);

    mIgnored = false;
    setHostDriven();
}

/** Requests the capture */
void HostUSBDevice::setCaptured ()
{
    AutoLock alock (this);
    Assert (isReady());

    Assert (
        mState == USBDeviceState_USBDeviceBusy ||
        mState == USBDeviceState_USBDeviceAvailable ||
        mState == USBDeviceState_USBDeviceHeld);

    mUSBProxyService->captureDevice (this);

    mState = USBDeviceState_USBDeviceCaptured;
}

/**
 * Returns the device back to the host
 *
 * @returns VBox status code.
 */
int HostUSBDevice::setHostDriven()
{
    AutoLock alock (this);
    AssertReturn (isReady(), VERR_INVALID_PARAMETER);

    AssertReturn (mState == USBDeviceState_USBDeviceHeld, VERR_INVALID_PARAMETER);

    mState = USBDeviceState_USBDeviceAvailable;

    return mUSBProxyService->releaseDevice (this);
}

/**
 * Resets the device as if it were just attached to the host
 *
 * @returns VBox status code.
 */
int HostUSBDevice::reset()
{
    AutoLock alock (this);
    AssertReturn (isReady(), VERR_INVALID_PARAMETER);

    mState = USBDeviceState_USBDeviceHeld;
    mIgnored = false;

    /** @todo this operation might fail and cause the device to the reattached with a different address and all that. */
    return mUSBProxyService->resetDevice (this);
}

/**
 *  Sets the state of the device, as it was reported by the host.
 *  This method applicable only for devices currently controlled by the host.
 *
 *  @param  aState      new state
 */
void HostUSBDevice::setHostState (USBDeviceState_T aState)
{
    AssertReturn (
        aState == USBDeviceState_USBDeviceUnavailable ||
        aState == USBDeviceState_USBDeviceBusy ||
        aState == USBDeviceState_USBDeviceAvailable,
        (void) 0);

    AssertReturn (
        mState == USBDeviceState_USBDeviceNotSupported || /* initial state */
        mState == USBDeviceState_USBDeviceUnavailable ||
        mState == USBDeviceState_USBDeviceBusy ||
        mState == USBDeviceState_USBDeviceAvailable,
        (void) 0);

    if (mState != aState)
    {
        mState = aState;
    }
}


/**
 * Compares this device with a USBDEVICE and decides which comes first.
 *
 * @returns < 0 if this should come before pDev2.
 * @returns   0 if this and pDev2 are equal.
 * @returns > 0 if this should come after pDev2.
 *
 * @param   pDev2   Device 2.
 */
int HostUSBDevice::compare (PCUSBDEVICE pDev2)
{
    return compare (m_pUsb, pDev2);
}


/**
 * Compares two USB devices and decides which comes first.
 *
 * @returns < 0 if pDev1 should come before pDev2.
 * @returns   0 if pDev1 and pDev2 are equal.
 * @returns > 0 if pDev1 should come after pDev2.
 *
 * @param   pDev1   Device 1.
 * @param   pDev2   Device 2.
 */
/*static*/ int HostUSBDevice::compare (PCUSBDEVICE pDev1, PCUSBDEVICE pDev2)
{
    int iDiff = pDev1->idVendor - pDev2->idVendor;
    if (iDiff)
        return iDiff;

    iDiff = pDev1->idProduct - pDev2->idProduct;
    if (iDiff)
        return iDiff;

    /** @todo Sander, will this work on windows as well? Linux won't reuse an address for quite a while. */
    return strcmp(pDev1->pszAddress, pDev2->pszAddress);
}

/**
 * Updates the state of the device.
 *
 * @returns true if the state has actually changed.
 * @returns false if the stat didn't change, or the change might have been cause by VBox.
 *
 * @param   aDev    The current device state as seen by the proxy backend.
 */
bool HostUSBDevice::updateState (PCUSBDEVICE aDev)
{
    AutoLock alock (this);

    /*
     * We have to be pretty conservative here because the proxy backend
     * doesn't necessarily know everything that's going on. So, rather
     * be overly careful than changing the state once when we shouldn't!
     */
    switch (aDev->enmState)
    {
        default:
            AssertMsgFailed (("aDev->enmState=%d\n", aDev->enmState));
        case USBDEVICESTATE_UNSUPPORTED:
            Assert (mState == USBDeviceState_USBDeviceNotSupported);
            return false;

        case USBDEVICESTATE_USED_BY_HOST:
            switch (mState)
            {
                case USBDeviceState_USBDeviceUnavailable:
                /* the proxy may confuse following states with unavailable */
                case USBDeviceState_USBDeviceHeld:
                case USBDeviceState_USBDeviceCaptured:
                    return false;
                default:
                    LogFlowMember ((" HostUSBDevice::updateState: %d -> %d\n",
                                    mState, USBDeviceState_USBDeviceUnavailable));
                    mState = USBDeviceState_USBDeviceUnavailable;
                    return true;
            }
            break;

        case USBDEVICESTATE_USED_BY_HOST_CAPTURABLE:
            switch (mState)
            {
                case USBDeviceState_USBDeviceBusy:
                /* the proxy may confuse following states with capturable */
                case USBDeviceState_USBDeviceHeld:
                case USBDeviceState_USBDeviceCaptured:
                    return false;
                default:
                    LogFlowMember ((" HostUSBDevice::updateState: %d -> %d\n",
                                    mState, USBDeviceState_USBDeviceBusy));
                    mState = USBDeviceState_USBDeviceBusy;
                    return true;
            }
            break;

        case USBDEVICESTATE_UNUSED:
            switch (mState)
            {
                case USBDeviceState_USBDeviceAvailable:
                /* the proxy may confuse following state(s) with available */
                case USBDeviceState_USBDeviceHeld:
                case USBDeviceState_USBDeviceCaptured:
                    return false;
                default:
                    LogFlowMember ((" HostUSBDevice::updateState: %d -> %d\n",
                                    mState, USBDeviceState_USBDeviceAvailable));
                    mState = USBDeviceState_USBDeviceAvailable;
                    return true;
            }
            break;

        case USBDEVICESTATE_HELD_BY_PROXY:
            switch (mState)
            {
                case USBDeviceState_USBDeviceHeld:
                /* the proxy may confuse following state(s) with held */
                case USBDeviceState_USBDeviceAvailable:
                case USBDeviceState_USBDeviceBusy:
                case USBDeviceState_USBDeviceCaptured:
                    return false;
                default:
                    LogFlowMember ((" HostUSBDevice::updateState: %d -> %d\n",
                                    mState, USBDeviceState_USBDeviceHeld));
                    mState = USBDeviceState_USBDeviceHeld;
                    return true;
            }
            break;

        case USBDEVICESTATE_USED_BY_GUEST:
            switch (mState)
            {
                case USBDeviceState_USBDeviceCaptured:
                /* the proxy may confuse following state(s) with captured */
                case USBDeviceState_USBDeviceHeld:
                case USBDeviceState_USBDeviceAvailable:
                case USBDeviceState_USBDeviceBusy:
                    return false;
                default:
                    LogFlowMember ((" HostUSBDevice::updateState: %d -> %d\n",
                                    mState, USBDeviceState_USBDeviceHeld));
                    mState = USBDeviceState_USBDeviceHeld;
                    return true;
            }
            break;
    }
}

