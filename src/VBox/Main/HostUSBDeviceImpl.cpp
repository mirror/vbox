/** @file
 *
 * VirtualBox IHostUSBDevice COM interface implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

DEFINE_EMPTY_CTOR_DTOR (HostUSBDevice)

HRESULT HostUSBDevice::FinalConstruct()
{
    mUSBProxyService = NULL;
    mUsb = NULL;

    return S_OK;
}

void HostUSBDevice::FinalRelease()
{
    uninit();
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

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    /*
     * We need a unique ID for this VBoxSVC session.
     * The UUID isn't stored anywhere.
     */
    unconst (mId).create();

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

    mPendingState = mState;

    /* Other data members */
    mIsStatePending = false;
/// @todo remove
#if 0
    mIgnored = false;
#endif
    mUSBProxyService = aUSBProxyService;
    mUsb = aUsb;

    /* Confirm the successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void HostUSBDevice::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    if (mUsb != NULL)
    {
        USBProxyService::freeDevice (mUsb);
        mUsb = NULL;
    }

    mUSBProxyService = NULL;
}

// IUSBDevice properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HostUSBDevice::COMGETTER(Id)(GUIDPARAMOUT aId)
{
    if (!aId)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mId is constant during life time, no need to lock */
    mId.cloneTo (aId);

    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(VendorId)(USHORT *aVendorId)
{
    if (!aVendorId)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aVendorId = mUsb->idVendor;

    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(ProductId)(USHORT *aProductId)
{
    if (!aProductId)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aProductId = mUsb->idProduct;

    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(Revision)(USHORT *aRevision)
{
    if (!aRevision)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aRevision = mUsb->bcdDevice;

    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(Manufacturer)(BSTR *aManufacturer)
{
    if (!aManufacturer)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Bstr (mUsb->pszManufacturer).cloneTo (aManufacturer);

    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(Product)(BSTR *aProduct)
{
    if (!aProduct)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Bstr (mUsb->pszProduct).cloneTo (aProduct);

    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(SerialNumber)(BSTR *aSerialNumber)
{
    if (!aSerialNumber)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Bstr (mUsb->pszSerialNumber).cloneTo (aSerialNumber);

    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(Address)(BSTR *aAddress)
{
    if (!aAddress)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    Bstr (mUsb->pszAddress).cloneTo (aAddress);

    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(Port)(USHORT *aPort)
{
    if (!aPort)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    ///@todo implement
    aPort = 0;

    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(Remote)(BOOL *aRemote)
{
    if (!aRemote)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aRemote = FALSE;

    return S_OK;
}

// IHostUSBDevice properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HostUSBDevice::COMGETTER(State) (USBDeviceState_T *aState)
{
    if (!aState)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aState = mState;

    return S_OK;
}


// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/** 
 * @note Locks this object for reading.
 */
Utf8Str HostUSBDevice::name()
{
    Utf8Str name;

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), name);

    AutoReaderLock alock (this);

    bool haveManufacturer = mUsb->pszManufacturer && *mUsb->pszManufacturer;
    bool haveProduct = mUsb->pszProduct && *mUsb->pszProduct;
    if (haveManufacturer && haveProduct)
        name = Utf8StrFmt ("%s %s", mUsb->pszManufacturer,
                                     mUsb->pszProduct);
    else if(haveManufacturer)
        name = Utf8StrFmt ("%s", mUsb->pszManufacturer);
    else if(haveProduct)
        name = Utf8StrFmt ("%s", mUsb->pszManufacturer);
    else
        name = "<unknown>";

    return name;
}

/// @todo remove
#if 0
/** 
 * Sets the ignored flag and returns the device to the host.
 *
 * @note Locks this object for writing.
 */
void HostUSBDevice::setIgnored()
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoLock alock (this);

    AssertReturnVoid (!mIgnored);

    mIgnored = true;

    setHostDriven();
}
#endif

/** 
 * @note Locks this object for writing.
 */
bool HostUSBDevice::setCaptured (SessionMachine *aMachine)
{
    AssertReturn (aMachine, false);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoLock alock (this);

    AssertReturn (
        mState == USBDeviceState_USBDeviceBusy ||
        mState == USBDeviceState_USBDeviceAvailable ||
        mState == USBDeviceState_USBDeviceHeld,
        false);

    mState = USBDeviceState_USBDeviceCaptured;
    mMachine = aMachine;

    mUSBProxyService->captureDevice (this);

    return true;
}

/**
 * Returns the device back to the host
 *
 * @returns VBox status code.
 *
 * @note Locks this object for writing.
 */
int HostUSBDevice::setHostDriven()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), VERR_INVALID_PARAMETER);

    AutoLock alock (this);

    AssertReturn (mState == USBDeviceState_USBDeviceHeld, VERR_INVALID_PARAMETER);

    mState = USBDeviceState_USBDeviceAvailable;

    return mUSBProxyService->releaseDevice (this);
}

/**
 * Resets the device as if it were just attached to the host
 *
 * @returns VBox status code.
 *
 * @note Locks this object for writing.
 */
int HostUSBDevice::reset()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), VERR_INVALID_PARAMETER);

    AutoLock alock (this);

    mState = USBDeviceState_USBDeviceHeld;
    mMachine.setNull();
/// @todo remove
#if 0
    mIgnored = false;
#endif

    /** @todo this operation might fail and cause the device to the reattached with a different address and all that. */
    return mUSBProxyService->resetDevice (this);
}

/// @todo remove
#if 0
/**
 *  Sets the state of the device, as it was reported by the host.
 *  This method applicable only for devices currently controlled by the host.
 *
 *  @param  aState      new state
 *
 *  @note Locks this object for writing.
 */
void HostUSBDevice::setHostState (USBDeviceState_T aState)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoLock alock (this);

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
#endif

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
 *
 *  @note Locks this object for reading.
 */
bool HostUSBDevice::isMatch (const USBDeviceFilter::Data &aData)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReaderLock alock (this);

    if (!aData.mActive)
        return false;

    if (!aData.mVendorId.isMatch (mUsb->idVendor))
    {
        LogFlowMember (("HostUSBDevice::isMatch: vendor not match %04X\n",
                        mUsb->idVendor));
        return false;
    }
    if (!aData.mProductId.isMatch (mUsb->idProduct))
    {
        LogFlowMember (("HostUSBDevice::isMatch: product id not match %04X\n",
                        mUsb->idProduct));
        return false;
    }
    if (!aData.mRevision.isMatch (mUsb->bcdDevice))
    {
        LogFlowMember (("HostUSBDevice::isMatch: rev not match %04X\n",
                        mUsb->bcdDevice));
        return false;
    }

#if !defined (__WIN__)
    // these filters are temporarily ignored on Win32
    if (!aData.mManufacturer.isMatch (Bstr (mUsb->pszManufacturer)))
        return false;
    if (!aData.mProduct.isMatch (Bstr (mUsb->pszProduct)))
        return false;
    if (!aData.mSerialNumber.isMatch (Bstr (mUsb->pszSerialNumber)))
        return false;
    /// @todo (dmik) pusPort is yet absent
//    if (!aData.mPort.isMatch (Bstr (mUsb->pusPort)))
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
    if (   mUsb->enmState == USBDEVICESTATE_USED_BY_HOST_CAPTURABLE
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
    return compare (mUsb, pDev2);
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
 *
 * @note Locks this object for writing.
 */
bool HostUSBDevice::updateState (PCUSBDEVICE aDev)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

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

