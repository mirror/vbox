/** @file
 *
 * VirtualBox IHostUSBDevice COM interface implementation
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
    mId.create();

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
STDMETHODIMP HostUSBDevice::COMGETTER(Id)(GUIDPARAMOUT aId)
{
    if (!aId)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    mId.cloneTo (aId);
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
STDMETHODIMP HostUSBDevice::COMGETTER(Manufacturer)(BSTR *aManufacturer)
{
    if (!aManufacturer)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    Bstr (m_pUsb->pszManufacturer).cloneTo (aManufacturer);
    return S_OK;
}


/**
 * Returns the product string.
 *
 * @returns COM status code
 * @param   aProduct            Where to put the return string.
 */
STDMETHODIMP HostUSBDevice::COMGETTER(Product)(BSTR *aProduct)
{
    if (!aProduct)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    Bstr (m_pUsb->pszProduct).cloneTo (aProduct);
    return S_OK;
}


/**
 * Returns the serial number string.
 *
 * @returns COM status code
 * @param   aSerialNumber     Where to put the return string.
 */
STDMETHODIMP HostUSBDevice::COMGETTER(SerialNumber)(BSTR *aSerialNumber)
{
    if (!aSerialNumber)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    Bstr (m_pUsb->pszSerialNumber).cloneTo (aSerialNumber);
    return S_OK;
}

/**
 * Returns the device address string.
 *
 * @returns COM status code
 * @param   aAddress            Where to put the returned string.
 */
STDMETHODIMP HostUSBDevice::COMGETTER(Address)(BSTR *aAddress)
{
    if (!aAddress)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    Bstr (m_pUsb->pszAddress).cloneTo (aAddress);
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

Utf8Str HostUSBDevice::name()
{
    Utf8Str name;

    AutoLock alock (this);
    AssertReturn (isReady(), name);

    bool haveManufacturer = m_pUsb->pszManufacturer && *m_pUsb->pszManufacturer;
    bool haveProduct = m_pUsb->pszProduct && *m_pUsb->pszProduct;
    if (haveManufacturer && haveProduct)
        name = Utf8StrFmt ("%s %s", m_pUsb->pszManufacturer,
                                     m_pUsb->pszProduct);
    else if(haveManufacturer)
        name = Utf8StrFmt ("%s", m_pUsb->pszManufacturer);
    else if(haveProduct)
        name = Utf8StrFmt ("%s", m_pUsb->pszManufacturer);
    else
        name = "<unknown>";

    return name;
}

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
bool HostUSBDevice::setCaptured (SessionMachine *aMachine)
{
    AssertReturn (aMachine, false);

    AutoLock alock (this);
    AssertReturn (isReady(), false);

    AssertReturn (
        mState == USBDeviceState_USBDeviceBusy ||
        mState == USBDeviceState_USBDeviceAvailable ||
        mState == USBDeviceState_USBDeviceHeld,
        false);

    mUSBProxyService->captureDevice (this);

    mState = USBDeviceState_USBDeviceCaptured;
    mMachine = aMachine;

    return true;
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
    mMachine.setNull();
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
 *  Returns true if this device matches the given filter data.
 *
 *  @note It is assumed, that the filter data owner is appropriately
 *        locked before calling this method.
 *
 *  @note
 *      This method MUST correlate with
 *      USBController::hasMatchingFilter (IUSBDevice *)
 *      in the sense of the device matching logic.
 */
bool HostUSBDevice::isMatch (const USBDeviceFilter::Data &aData)
{
    AutoLock alock (this);
    AssertReturn (isReady(), false);

    if (!aData.mActive)
        return false;

    if (!aData.mVendorId.isMatch (m_pUsb->idVendor))
    {
        LogFlowMember (("HostUSBDevice::isMatch: vendor not match %04X\n",
                        m_pUsb->idVendor));
        return false;
    }
    if (!aData.mProductId.isMatch (m_pUsb->idProduct))
    {
        LogFlowMember (("HostUSBDevice::isMatch: product id not match %04X\n",
                        m_pUsb->idProduct));
        return false;
    }
    if (!aData.mRevision.isMatch (m_pUsb->bcdDevice))
    {
        LogFlowMember (("HostUSBDevice::isMatch: rev not match %04X\n",
                        m_pUsb->bcdDevice));
        return false;
    }

#if !defined (__WIN__)
    // these filters are temporarily ignored on Win32
    if (!aData.mManufacturer.isMatch (Bstr (m_pUsb->pszManufacturer)))
        return false;
    if (!aData.mProduct.isMatch (Bstr (m_pUsb->pszProduct)))
        return false;
    if (!aData.mSerialNumber.isMatch (Bstr (m_pUsb->pszSerialNumber)))
        return false;
    /// @todo (dmik) pusPort is yet absent
//    if (!aData.mPort.isMatch (Bstr (m_pUsb->pusPort)))
//        return false;
#endif

    // Host USB devices are local, so remote is always FALSE
    if (!aData.mRemote.isMatch (FALSE))
    {
        LogFlowMember (("Host::HostUSBDevice: remote not match FALSE\n"));
        return false;
    }

    /// @todo (dmik): bird, I assumed isMatch() is called only for devices
    //  that are suitable for holding/capturing (also assuming that when the device
    //  is just attached it first goes to our filter driver, and only after applying
    //  filters goes back to the system when appropriate). So the below
    //  doesn't look too correct; moreover, currently there is no determinable
    //  "any match" state for intervalic filters, and it will be not so easy
    //  to determine this state for an arbitrary regexp expression...
    //  For now, I just check that the string filter is empty (which doesn't
    //  actually reflect all possible "any match" filters).
    //
    // bird: This method was called for any device some weeks back, and it most certainly
    // should be called for 'busy' devices still. However, we do *not* want 'busy' devices
    // to match empty filters (because that will for instance capture all USB keyboards & mice).
    // You assumption about a filter driver is not correct on linux. We're racing with
    // everyone else in the system there - see your problem with usbfs access.
    //
    // The customer *requires* a way of matching all devices which the host isn't using,
    // if that is now difficult or the below method opens holes in the matching, this *must*
    // be addresses immediately.

    /*
     * If all the criteria is empty, devices which are used by the host will not match.
     */
    if (   m_pUsb->enmState == USBDEVICESTATE_USED_BY_HOST_CAPTURABLE
        && aData.mVendorId.string().isEmpty()
        && aData.mProductId.string().isEmpty()
        && aData.mRevision.string().isEmpty()
        && aData.mManufacturer.string().isEmpty()
        && aData.mProduct.string().isEmpty()
        && aData.mSerialNumber.string().isEmpty())
        return false;

    LogFlowMember (("HostUSBDevice::isMatch: returns true\n"));
    return true;
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
 * @return true if the state has actually changed.
 * @return false if the state didn't change, or the change might have been caused by VBox.
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

    return false;
}

