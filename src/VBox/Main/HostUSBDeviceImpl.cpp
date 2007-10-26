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
 */

#include <iprt/types.h> /* for UINT64_C */

#include "HostUSBDeviceImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxErrorInfoImpl.h"
#include "USBProxyService.h"

#include "Logging.h"

#include <VBox/err.h>
#include <iprt/cpputils.h>

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
            /* @todo USBDEVICESTATE_USED_BY_GUEST seems not to be used
             * anywhere in the proxy code; it's quite logical because the
             * proxy doesn't know anything about guest VMs. */
            AssertFailedReturn (E_FAIL);
            break;
    }

    mPendingState = mState;
    mPendingStateEx = kNothingPending;

    /* Other data members */
    mIsStatePending = false;
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

STDMETHODIMP HostUSBDevice::COMGETTER(Version)(USHORT *aVersion)
{
    if (!aVersion)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aVersion = mUsb->bcdUSB >> 8;

    return S_OK;
}

STDMETHODIMP HostUSBDevice::COMGETTER(PortVersion)(USHORT *aPortVersion)
{
    if (!aPortVersion)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    /** @todo implement this correctly or things just won't work right, see the vista defect. */
    *aPortVersion = mUsb->bcdUSB >> 8;

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
    else if (haveManufacturer)
        name = Utf8StrFmt ("%s", mUsb->pszManufacturer);
    else if (haveProduct)
        name = Utf8StrFmt ("%s", mUsb->pszProduct);
    else
        name = "<unknown>";

    return name;
}

/**
 *  Requests the USB proxy service to capture the device and sets the pending
 *  state to Captured.
 *
 *  If the state change may be performed immediately (for example, Hold ->
 *  Captured), then the machine is informed before this method returns.
 *
 *  @param aMachine     Machine that will capture this device on success.
 *  @return             @c false if the device could be immediately captured
 *                      but the VM process refused to grab it;
 *                      @c true otherwise.
 *
 *  @note Must be called from under the object write lock.
 *
 *  @note May lock the given machine object for reading.
 */
bool HostUSBDevice::requestCapture (SessionMachine *aMachine)
{
    LogFlowThisFunc (("\n"));

    AssertReturn (aMachine, false);

    AssertReturn (isLockedOnCurrentThread(), false);

    AssertReturn (mIsStatePending == false, false);

    AssertReturn (
        mState == USBDeviceState_USBDeviceBusy ||
        mState == USBDeviceState_USBDeviceAvailable ||
        mState == USBDeviceState_USBDeviceHeld,
        false);

    if (mState == USBDeviceState_USBDeviceHeld)
    {
        /* can perform immediate capture, inform the VM process */

        ComPtr <IUSBDevice> d = this;

        mIsStatePending = true;
        mPendingSince = 0;

        /* the VM process will query the object, so leave the lock */
        AutoLock alock (this);
        alock.leave();

        LogFlowThisFunc (("Calling machine->onUSBDeviceAttach()...\n"));

        HRESULT rc = aMachine->onUSBDeviceAttach (d, NULL);

        /* The VM process has a legal reason to fail (for example, incorrect
         * usbfs permissions or out of virtual USB ports). More over, the VM
         * process might have been accidentially crashed and not accessible
         * any more (so that calling an uninitialized SessionMachine returns a
         * failure). So don't assert. */

        LogFlowThisFunc (("Done machine->onUSBDeviceAttach()=%08X\n", rc));

        alock.enter();

        mIsStatePending = false;
        mPendingStateEx = kNothingPending;

        if (SUCCEEDED (rc))
        {
            mState = mPendingState = USBDeviceState_USBDeviceCaptured;
            mMachine = aMachine;
            return true;
        }

        return false;
    }

    mIsStatePending = true;
    mPendingState = USBDeviceState_USBDeviceCaptured;
    mPendingStateEx = kNothingPending;
    mPendingSince = RTTimeNanoTS();
    mMachine = aMachine;

    mUSBProxyService->captureDevice (this);

    return true;
}

/**
 *  Requests the USB proxy service to release the device and sets the pending
 *  state to Available.
 *
 *  If the state change may be performed immediately (for example, the current
 *  state is Busy), this method does nothing.
 *
 *  @note Must be called from under the object write lock.
 */
void HostUSBDevice::requestRelease()
{
    LogFlowThisFunc (("\n"));

    AssertReturnVoid (isLockedOnCurrentThread());

    AssertReturnVoid (mIsStatePending == false);

    AssertReturnVoid (
        mState == USBDeviceState_USBDeviceBusy ||
        mState == USBDeviceState_USBDeviceAvailable ||
        mState == USBDeviceState_USBDeviceHeld);

    if (mState != USBDeviceState_USBDeviceHeld)
        return;

    mIsStatePending = true;
    mPendingState = USBDeviceState_USBDeviceAvailable;
    mPendingStateEx = kNothingPending;
    mPendingSince = RTTimeNanoTS();

    mUSBProxyService->releaseDevice (this);
}

/**
 *  Requests the USB proxy service to release the device, sets the pending
 *  state to Held and removes the machine association if any.
 *
 *  If the state change may be performed immediately (for example, the current
 *  state is already Held), this method does nothing but removes the machine
 *  association.
 *
 *  @note Must be called from under the object write lock.
 */
void HostUSBDevice::requestHold()
{
    LogFlowThisFunc (("\n"));

    AssertReturnVoid (isLockedOnCurrentThread());

    AssertReturnVoid (mIsStatePending == false);

    AssertReturnVoid (
        mState == USBDeviceState_USBDeviceBusy ||
        mState == USBDeviceState_USBDeviceAvailable ||
        mState == USBDeviceState_USBDeviceHeld);

    mMachine.setNull();

    if (mState == USBDeviceState_USBDeviceHeld)
        return;

    mIsStatePending = true;
    mPendingState = USBDeviceState_USBDeviceHeld;
    mPendingStateEx = kNothingPending;
    mPendingSince = RTTimeNanoTS();

    mUSBProxyService->captureDevice (this);
}

/**
 *  Sets the device state from Captured to Held and resets the machine
 *  association (if any). Usually called before applying filters.
 *
 *  @note Must be called from under the object write lock.
 */
void HostUSBDevice::setHeld()
{
    LogFlowThisFunc (("\n"));

    AssertReturnVoid (isLockedOnCurrentThread());

    AssertReturnVoid (mState == USBDeviceState_USBDeviceCaptured);
    AssertReturnVoid (mPendingState == USBDeviceState_USBDeviceCaptured);
    AssertReturnVoid (mIsStatePending == false);

    mState = USBDeviceState_USBDeviceHeld;
    mMachine.setNull();
}

/**
 *  Resets all device data and informs the machine (if any) about the
 *  detachment. Must be called when this device is physically detached from
 *  the host.
 *
 *  @note Must be called from under the object write lock.
 */
void HostUSBDevice::onDetachedPhys()
{
    LogFlowThisFunc (("\n"));

    AssertReturnVoid (isLockedOnCurrentThread());

    if (!mMachine.isNull() && mState == USBDeviceState_USBDeviceCaptured)
    {
        /* the device is captured by a machine, instruct it to release */

        mIsStatePending = true;
        mPendingSince = 0;

        /* the VM process will query the object, so leave the lock */
        AutoLock alock (this);
        alock.leave();

        LogFlowThisFunc (("Calling machine->onUSBDeviceDetach()...\n"));

        HRESULT rc = mMachine->onUSBDeviceDetach (mId, NULL);
        NOREF(rc);

        /* This call may expectedly fail with rc = NS_ERROR_FAILURE (on XPCOM)
         * if the VM process requests device release right before termination
         * and then terminates before onUSBDeviceDetach() reached
         * it. Therefore, we don't assert here. On MS COM, there should be
         * something similar (with the different error code). More over, the
         * VM process might have been accidentially crashed and not accessible
         * any more (so that calling an uninitialized SessionMachine returns a
         * failure). So don't assert. */

        LogFlowThisFunc (("Done machine->onUSBDeviceDetach()=%08X\n", rc));

        alock.enter();

        /* Reset all fields. The object should have been
         * uninitialized after this method returns, so it doesn't really
         * matter what state we put it in. */
        mIsStatePending = false;
        mState = mPendingState = USBDeviceState_USBDeviceNotSupported;
        mPendingStateEx = kNothingPending;
        mMachine.setNull();
    }
}

/**
 *  Handles the finished pending state change and informs the VM process if
 *  necessary.
 *
 *  @note Must be called from under the object write lock.
 */
void HostUSBDevice::handlePendingStateChange()
{
    LogFlowThisFunc (("\n"));

    AssertReturnVoid (isLockedOnCurrentThread());

    AssertReturnVoid (mIsStatePending == true);
    AssertReturnVoid (mState != USBDeviceState_USBDeviceCaptured || mPendingStateEx != kNothingPending);

    bool wasCapture = false;

    HRESULT requestRC = S_OK;
    Bstr errorText;

    switch (mPendingStateEx)
    {
        case kNothingPending:
            switch (mPendingState)
            {
                case USBDeviceState_USBDeviceCaptured:
                {
                    if (mState == USBDeviceState_USBDeviceHeld)
                    {
                        if (!mMachine.isNull())
                            wasCapture = true;
                        else
                        {
                            /* it is a canceled capture request. Give the device back
                             * to the host. */
                            mPendingState = USBDeviceState_USBDeviceAvailable;
                            mUSBProxyService->releaseDevice (this);
                        }
                    }
                    else
                    {
                        /* couldn't capture the device, will report an error */
                        wasCapture = true;

                        Assert (!mMachine.isNull());

                        /// @todo more detailed error message depending on the state?
                        //  probably need some error code/string from the USB proxy itself

                        requestRC = E_FAIL;
                        errorText = Utf8StrFmt (
                            tr ("USB device '%s' with UUID {%Vuuid} is being accessed by the host "
                                "computer and cannot be attached to the virtual machine."
                                "Please try later"),
                            name().raw(), id().raw());
                    }
                    break;
                }
                case USBDeviceState_USBDeviceAvailable:
                {
                    Assert (mMachine.isNull());

                    if (mState == USBDeviceState_USBDeviceHeld)
                    {
                        /* couldn't release the device (give it back to the host).
                         * there is nobody to report an error to (the machine has
                         * already been deassociated because VMM has already detached
                         * the device before requesting a release). */
                    }
                    else
                    {
                        /* it is a canceled release request. Leave at the host */
                        /// @todo we may want to re-run all filters in this case
                    }
                    break;
                }
                case USBDeviceState_USBDeviceHeld:
                {
                    if (mState == USBDeviceState_USBDeviceHeld)
                    {
                        /* All right, the device is now held (due to some global
                         * filter). */
                        break;
                    }
                    else
                    {
                        /* couldn't capture the device requested by the global
                         * filter, there is nobody to report an error to. */
                    }
                    break;
                }
                default:
                    AssertFailed();
            }
            break;

        /*
         * The device has reappeared, the caller (Host) will (maybe) reapply filters,
         * since we don't quite know we set the machine to NULL.
         */
        case kDetachingPendingAttachFilters:
            mMachine.setNull();
            break;

        /*
         * The device has reappeared while the detach operation is still in
         * progress, just clear the pending operation and leave the machine as is.
         */
        case kDetachingPendingAttach:
            break;

        case kDetachingPendingDetach:
        case kDetachingPendingDetachFilters:
        default:
            AssertMsgFailed(("%d\n", mPendingStateEx));
            return;
    }

    ComObjPtr <VirtualBoxErrorInfo> error;
    if (FAILED (requestRC))
    {
        LogFlowThisFunc (("Request failed, requestRC=%08X, text='%ls'\n",
                          requestRC, errorText.raw()));

        error.createObject();
        error->init (requestRC, COM_IIDOF (IHostUSBDevice),
                     Bstr (HostUSBDevice::getComponentName()),
                     errorText.raw());
    }

    if (wasCapture)
    {
        /* inform the VM process */

        ComPtr <IUSBDevice> d = this;

        /* the VM process will query the object, so leave the lock */
        AutoLock alock (this);
        alock.leave();

        LogFlowThisFunc (("Calling machine->onUSBDeviceAttach()...\n"));

        HRESULT rc = mMachine->onUSBDeviceAttach (d, error);

        /* The VM process has a legal reason to fail (for example, incorrect
         * usbfs permissions or out of virtual USB ports). More over, the VM
         * process might have been accidentially crashed and not accessible
         * any more (so that calling an uninitialized SessionMachine returns a
         * failure). So don't assert. */

        /// @todo we will probably want to re-run all filters on failure

        LogFlowThisFunc (("Done machine->onUSBDeviceAttach()=%08X\n", rc));

        alock.enter();

        if (SUCCEEDED (requestRC) && SUCCEEDED (rc))
        {
            mIsStatePending = false;
            mState = mPendingState = USBDeviceState_USBDeviceCaptured;
            mPendingStateEx = kNothingPending;
            return;
        }

        /* on failure, either from the proxy or from the VM process,
         * deassociate from the machine */
        mMachine.setNull();
    }

    mIsStatePending = false;
    mPendingState = mState;
    mPendingStateEx = kNothingPending;
}

/**
 *  Cancels pending state change due to machine termination or timeout.
 *
 *  @note Must be called from under the object write lock.
 *  @param  aTimeout        Whether this is a timeout or not.
 */
void HostUSBDevice::cancelPendingState(bool aTimeout /*= false*/)
{
    LogFlowThisFunc (("\n"));

    AssertReturnVoid (isLockedOnCurrentThread());

    AssertReturnVoid (mIsStatePending == true);
    AssertReturnVoid (aTimeout || !mMachine.isNull());

    switch (mPendingStateEx)
    {
        case kNothingPending:
            switch (mPendingState)
            {
                case USBDeviceState_USBDeviceCaptured:
                    /* reset mMachine to deassociate it from the filter and tell
                     * handlePendingStateChange() what to do */
                    mMachine.setNull();
                    if (!aTimeout)
                        break;
                case USBDeviceState_USBDeviceAvailable:
                case USBDeviceState_USBDeviceHeld:
                    if (aTimeout)
                    {
                        mPendingStateEx = kNothingPending;
                        mIsStatePending = false;
                        break;
                    }
                    /* fall thru */
                default:
                    AssertFailed();
            }
            break;

        case kDetachingPendingDetach:
        case kDetachingPendingDetachFilters:
        case kDetachingPendingAttach:
        case kDetachingPendingAttachFilters:
            LogFlowThisFunc (("cancelling reattach in state %d\n", mPendingStateEx));
            mMachine.setNull();
            mPendingStateEx = kNothingPending;
            mIsStatePending = false;
            break;

        default:
            AssertMsgFailed(("%d\n", mPendingStateEx));
            break;
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

#ifndef VBOX_WITH_USBFILTER
    if (!aData.mVendorId.isMatch (mUsb->idVendor))
    {
        LogFlowThisFunc (("vendor not match %04X\n",
                          mUsb->idVendor));
        return false;
    }
    if (!aData.mProductId.isMatch (mUsb->idProduct))
    {
        LogFlowThisFunc (("product id not match %04X\n",
                          mUsb->idProduct));
        return false;
    }
    if (!aData.mRevision.isMatch (mUsb->bcdDevice))
    {
        LogFlowThisFunc (("rev not match %04X\n",
                          mUsb->bcdDevice));
        return false;
    }

#if !defined (RT_OS_WINDOWS)
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

#else  /* VBOX_WITH_USBFILTER */
    if (USBFilterMatchDevice (&aData.mUSBFilter, mUsb))
    {
        /* Don't match busy devices with a 100% wildcard filter - this will
           later become a filter prop (ring-3 only). */
        if (    mUsb->enmState == USBDEVICESTATE_USED_BY_HOST_CAPTURABLE
            &&  !USBFilterHasAnySubstatialCriteria (&aData.mUSBFilter))
            return false;
    }
#endif /* VBOX_WITH_USBFILTER */

    LogFlowThisFunc (("returns true\n"));
    return true;
}


/**
 *  Compares this device with a USBDEVICE and decides which comes first.
 *
 *  If the device has a pending state request, a non-strict comparison is made
 *  (to properly detect a re-attached device). Otherwise, a strict comparison
 *  is performed.
 *
 *  @param   aDev2   Device 2.
 *
 *  @return < 0 if this should come before aDev2.
 *  @return   0 if this and aDev2 are equal.
 *  @return > 0 if this should come after aDev2.
 *
 *  @note Must be called from under the object write lock.
 */
int HostUSBDevice::compare (PCUSBDEVICE aDev2)
{
    AssertReturn (isLockedOnCurrentThread(), -1);

    return compare (mUsb, aDev2, !isStatePending());
}

/**
 *  Compares two USB devices and decides which comes first.
 *
 *  If @a aIsStrict is @c true then the comparison will indicate a difference
 *  even if the same physical device (represented by @a aDev1) has been just
 *  re-attached to the host computer (represented by @a aDev2) and got a
 *  different address from the host OS, etc.
 *
 *  If @a aIsStrict is @c false, then such a re-attached device will be
 *  considered equal to the previous device definition and this function will
 *  retrun 0.
 *
 *  @param   aDev1      Device 1.
 *  @param   aDev2      Device 2.
 *  @param   aIsStrict  @c true to do a strict check and @c false otherwise.

 *  @return < 0 if aDev1 should come before aDev2.
 *  @return   0 if aDev1 and aDev2 are equal.
 *  @return > 0 if aDev1 should come after aDev2.
 */
/*static*/
int HostUSBDevice::compare (PCUSBDEVICE aDev1, PCUSBDEVICE aDev2,
                            bool aIsStrict /* = true */)
{
    /* The non-strict checks tries as best as it can to distiguish between
       different physical devices of the same product. Unfortunately this
       isn't always possible and we might end up a bit confused in rare cases... */

    int iDiff = aDev1->idVendor - aDev2->idVendor;
    if (iDiff)
        return iDiff;

    iDiff = aDev1->idProduct - aDev2->idProduct;
    if (iDiff)
        return iDiff;

    iDiff = aDev1->bcdDevice - aDev2->bcdDevice;
    if (iDiff)
        return iDiff;

    if (aDev1->u64SerialHash != aDev2->u64SerialHash)
        return aDev1->u64SerialHash < aDev2->u64SerialHash ? -1 : 1;

    if (!aIsStrict)
        return 0;

    /* The rest is considered as a strict check since it includes bits that
       may vary on logical reconnects (or whatever you wish to call it). */
    return strcmp (aDev1->pszAddress, aDev2->pszAddress);
}

/**
 *  Updates the state of the device.
 *
 *  If this method returns @c true, Host::onUSBDeviceStateChanged() will be
 *  called to process the state change (complete the state change request,
 *  inform the VM process etc.).
 *
 *  If this method returns @c false, it is assumed that the given state change
 *  is "minor": it doesn't require any further action other than update the
 *  mState field with the actual state value.
 *
 *  Regardless of the return value, this method always takes ownership of the
 *  new USBDEVICE structure passed in and updates the pNext and pPrev fiends in
 *  it using the values of the old structure.
 *
 *  @param   aDev    The current device state as seen by the proxy backend.
 *
 *  @return Whether the Host object should be bothered with this state change.
 *
 *  @note Locks this object for writing.
 */
bool HostUSBDevice::updateState (PCUSBDEVICE aDev)
{
    LogFlowThisFunc (("\n"));

    AssertReturn (isLockedOnCurrentThread(), false);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoLock alock (this);

    /* Replace the existing structure by the new one */
    if (mUsb != aDev)
    {
        aDev->pNext = mUsb->pNext;
        aDev->pPrev = mUsb->pPrev;
        USBProxyService::freeDevice (mUsb);
        mUsb = aDev;
    }

    bool isImportant = false;

    /*
     * We have to be pretty conservative here because the proxy backend
     * doesn't necessarily know everything that's going on. So, rather
     * be overly careful than changing the state once when we shouldn't!
     *
     * In particular, we treat changing between three states Unavailable, Busy
     * and Available as non-important (because they all mean that the device
     * is owned by the host) and return false in this case. We may want to
     * change it later and, e.g. re-run all USB filters when the device goes from
     * from Busy to Available).
     *
     * 2007-07-04: State transitions from Unavailable to Busy or Available
     *             are now considered important and will cause filters to
     *             be rerun on the device. (See #2030 and #1870.)
     */

    LogFlowThisFunc (("aDev->enmState=%d mState=%d mPendingState=%d mPendingStateEx=%d\n",
                      aDev->enmState, mState, mPendingState, mPendingStateEx));

    switch (aDev->enmState)
    {
        default:
            AssertMsgFailed (("aDev->enmState=%d\n", aDev->enmState));
        case USBDEVICESTATE_UNSUPPORTED:
            Assert (mState == USBDeviceState_USBDeviceNotSupported);
            switch (mState)
            {
                case USBDeviceState_USBDeviceCaptured:
                    isImportant = mIsStatePending;
                    break;
            }
            return isImportant;

        case USBDEVICESTATE_USED_BY_HOST:
            switch (mState)
            {
                case USBDeviceState_USBDeviceUnavailable:
                    return false;
                /* the following state changes don't require any action for now */
                case USBDeviceState_USBDeviceBusy:
                case USBDeviceState_USBDeviceAvailable:
                    isImportant = false;
                    break;
#ifndef RT_OS_WINDOWS /* Only windows really knows whether the device is unavailable or captured. */
                case USBDeviceState_USBDeviceCaptured:
                    if (!mIsStatePending)
                        return false;
                    /* fall thru */
#endif
                default:
                    isImportant = true;
            }
            LogFlowThisFunc (("%d -> %d\n",
                              mState, USBDeviceState_USBDeviceUnavailable));
            mState = USBDeviceState_USBDeviceUnavailable;
            return isImportant;

        case USBDEVICESTATE_USED_BY_HOST_CAPTURABLE:
            switch (mState)
            {
                case USBDeviceState_USBDeviceBusy:
                    return false;
                case USBDeviceState_USBDeviceAvailable:
                    isImportant = false;
                    break;
                case USBDeviceState_USBDeviceCaptured:
#ifndef RT_OS_WINDOWS /* Only Windows really knows whether the device is busy or captured. */
                    if (!mIsStatePending)
                        return false;
#endif
                    /* Remain in the captured state if it's an async detach. */
                    if (mPendingStateEx != kNothingPending)
                    {
                        LogFlowThisFunc (("USBDeviceCaptured - async detach completed (%d)\n", mPendingStateEx));
                        return true;
                    }
                    /* fall thru */
                default:
                    /* USBDeviceState_USBDeviceUnavailable: The device has become capturable, re-run filters. */
                    /* USBDeviceState_USBDeviceHeld:        Pending request. */
                    /* USBDeviceState_USBDeviceCaptured:    Pending request. */
                    /* USBDeviceState_USBDeviceNotSupported: Something is broken. */
                    isImportant = true;
            }
            LogFlowThisFunc (("%d -> %d\n",
                              mState, USBDeviceState_USBDeviceBusy));
            mState = USBDeviceState_USBDeviceBusy;
            return isImportant;

        case USBDEVICESTATE_UNUSED:
            switch (mState)
            {
                case USBDeviceState_USBDeviceAvailable:
                    return false;
#if defined(RT_OS_LINUX)  /* Hack for /proc/bus/usb/devices not necessarily putting up a driver. */ \
 || defined(RT_OS_DARWIN) /* We're a bit clueless as to the exact device state, just like linux. */
                case USBDeviceState_USBDeviceCaptured:
                    if (    !mIsStatePending
                        ||  mPendingStateEx != kNothingPending)
                        return false;
                    isImportant = true;
                    break;
#endif
                /* the following state changes don't require any action for now */
                case USBDeviceState_USBDeviceBusy:
                    isImportant = false;
                    break;
                default:
                /* USBDeviceState_USBDeviceUnavailable: The device has become available, re-run filters. */
                /* USBDeviceState_USBDeviceHeld:        Pending request. */
                /* USBDeviceState_USBDeviceNotSupported: Something is broken. */
                    isImportant = true;
            }
            LogFlowThisFunc (("%d -> %d\n",
                              mState, USBDeviceState_USBDeviceAvailable));
            mState = USBDeviceState_USBDeviceAvailable;
            return isImportant;

        case USBDEVICESTATE_HELD_BY_PROXY:
            switch (mState)
            {
                case USBDeviceState_USBDeviceHeld:
                    return false;
                case USBDeviceState_USBDeviceCaptured:
                    if (!mIsStatePending)
                        return false;
                    /* no break */
                default:
                    LogFlowThisFunc (("%d -> %d\n",
                                      mState, USBDeviceState_USBDeviceHeld));
                    mState = USBDeviceState_USBDeviceHeld;
                    return true;
            }
            break;

        case USBDEVICESTATE_USED_BY_GUEST:
            /** @todo USBDEVICESTATE_USED_BY_GUEST seems not to be used
             * anywhere in the proxy code; it's quite logical because the
             * proxy doesn't know anything about guest VMs. */
            AssertFailed();
#if 0
            switch (mState)
            {
                case USBDeviceState_USBDeviceCaptured:
                /* the proxy may confuse following state(s) with captured */
                case USBDeviceState_USBDeviceHeld:
                case USBDeviceState_USBDeviceAvailable:
                case USBDeviceState_USBDeviceBusy:
                    return false;
                default:
                    LogFlowThisFunc (("%d -> %d\n",
                                      mState, USBDeviceState_USBDeviceHeld));
                    mState = USBDeviceState_USBDeviceHeld;
                    return true;
            }
#endif
            break;
    }

    return false;
}

/**
 *  Checks for timeout of any pending async operation.
 *
 *  The caller must write lock the object prior to calling
 *  this method.
 */
void HostUSBDevice::checkForAsyncTimeout()
{
    AssertReturnVoid (isLockedOnCurrentThread());

#ifndef RT_OS_WINDOWS /* no timeouts on windows yet since I don't have all the details here... */
    if (isStatePending() && mPendingSince)
    {
        uint64_t elapsedNanoseconds = RTTimeNanoTS() - mPendingSince;
        if (elapsedNanoseconds > UINT64_C (60000000000) ) /* 60 seconds */
        {
            LogRel (("USB: Async operation timed out; mPendingState=%d mPendingStateEx=%d idVendor=%04x (%s) idProduct=%04x (%s) bcdDevice=%04x\n",
                     mPendingState, mPendingStateEx, mUsb->idVendor, mUsb->pszManufacturer, mUsb->idProduct,  mUsb->pszProduct, mUsb->bcdDevice));

            cancelPendingState (true);
        }
    }
#endif
}

/**
 *  This method is called by the USB proxy and Host to work the
 *  logical reconnection operation.
 *
 *  @param  aStage      kDeatchingPendingDetach, kDeatchingPendingDetachFilters,
 *                      kDetachingPendingAttach or kDetachingPendingAttachFilters.
 *
 *  @returns Success indicator.
 */
bool HostUSBDevice::setLogicalReconnect (InternalState aStage)
{
    AssertReturn (isLockedOnCurrentThread(), false);

    switch (aStage)
    {
        case kDetachingPendingDetach:
            AssertReturn (!mIsStatePending, false);
            mPendingState = mState;
            mIsStatePending = true;
            mPendingSince = RTTimeNanoTS();
            LogFlowThisFunc (("pending detach\n"));
            break;

        case kDetachingPendingDetachFilters:
            AssertReturn (mIsStatePending, false);
            AssertReturn (mPendingStateEx == kDetachingPendingDetach, false);
            LogFlowThisFunc (("pending detach+filters\n"));
            break;

        case kDetachingPendingAttach:
            AssertReturn (mIsStatePending, false);
            AssertReturn (mPendingStateEx == kDetachingPendingDetach, false);
            LogFlowThisFunc (("pending attach\n"));
            break;

        case kDetachingPendingAttachFilters:
            AssertReturn (mIsStatePending, false);
            AssertReturn (   mPendingStateEx == kDetachingPendingAttach
                          || mPendingStateEx == kDetachingPendingDetachFilters, false);
            LogFlowThisFunc (("pending attach+filters\n"));
            break;

        default:
            AssertFailedReturn (false);
    }
    mPendingStateEx = aStage;
    return true;
}

