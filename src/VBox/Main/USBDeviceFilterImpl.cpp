/** @file
 *
 * Implementation of VirtualBox COM components:
 * USBDeviceFilter and HostUSBDeviceFilter
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

#include "USBDeviceFilterImpl.h"
#include "USBControllerImpl.h"
#include "MachineImpl.h"
#include "HostImpl.h"
#include "Logging.h"

////////////////////////////////////////////////////////////////////////////////
// USBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

HRESULT USBDeviceFilter::FinalConstruct()
{
    return S_OK;
}

void USBDeviceFilter::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the USB device filter object.
 */
HRESULT USBDeviceFilter::init (USBController *aParent,
                               INPTR BSTR aName, BOOL aActive,
                               INPTR BSTR aVendorId, INPTR BSTR aProductId,
                               INPTR BSTR aRevision,
                               INPTR BSTR aManufacturer, INPTR BSTR aProduct,
                               INPTR BSTR aSerialNumber,
                               INPTR BSTR aPort, INPTR BSTR aRemote)
{
    LogFlowMember (("USBDeviceFilter::init (%p)\n", aParent));

    ComAssertRet (aParent && aName && *aName, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = aParent;

    mData.allocate();
    mData->mName = aName;
    mData->mActive = aActive;

    // initialize all filters to any match using null string
    mData->mVendorId = NULL;
    mData->mProductId = NULL;
    mData->mRevision = NULL;
    mData->mManufacturer = NULL;
    mData->mProduct = NULL;
    mData->mSerialNumber = NULL;
    mData->mPort = NULL;
    mData->mRemote = NULL;

    mInList = false;

    // use setters for the attributes below to reuse parsing errors handling
    setReady (true);
    HRESULT rc = S_OK;
    do
    {
        rc = COMSETTER(VendorId) (aVendorId);
        if (FAILED (rc))
            break;
        rc = COMSETTER(ProductId) (aProductId);
        if (FAILED (rc))
            break;
        rc = COMSETTER(Revision) (aRevision);
        if (FAILED (rc))
            break;
        rc = COMSETTER(Manufacturer) (aManufacturer);
        if (FAILED (rc))
            break;
        rc = COMSETTER(Product) (aProduct);
        if (FAILED (rc))
            break;
        rc = COMSETTER(SerialNumber) (aSerialNumber);
        if (FAILED (rc))
            break;
        rc = COMSETTER(Port) (aPort);
        if (FAILED (rc))
            break;
        rc = COMSETTER(Remote) (aRemote);
        if (FAILED (rc))
            break;
    }
    while (0);

    if (SUCCEEDED (rc))
        mParent->addDependentChild (this);
    else
        setReady (false);

    return rc;
}

/**
 * Initializes the USB device filter object (short version).
 */
HRESULT USBDeviceFilter::init (USBController *aParent, INPTR BSTR aName)
{
    LogFlowMember (("USBDeviceFilter::init (%p) [short]\n", aParent));

    ComAssertRet (aParent && aName && *aName, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = aParent;
    // mPeer is left null

    mData.allocate();

    mData->mName = aName;
    mData->mActive = FALSE;

    // initialize all filters to any match using null string
    mData->mVendorId = NULL;
    mData->mProductId = NULL;
    mData->mRevision = NULL;
    mData->mManufacturer = NULL;
    mData->mProduct = NULL;
    mData->mSerialNumber = NULL;
    mData->mPort = NULL;
    mData->mRemote = NULL;

    mInList = false;

    mParent->addDependentChild (this);

    setReady (true);
    return S_OK;
}

/**
 *  Initializes the object given another object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @param  aReshare
 *      When false, the original object will remain a data owner.
 *      Otherwise, data ownership will be transferred from the original
 *      object to this one.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 */
HRESULT USBDeviceFilter::init (USBController *aParent, USBDeviceFilter *aThat,
                               bool aReshare /* = false */)
{
    LogFlowMember (("USBDeviceFilter::init (%p, %p): aReshare=%d\n",
                    aParent, aThat, aReshare));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = aParent;

    AutoLock thatlock (aThat);
    if (aReshare)
    {
        aThat->mPeer = this;
        mData.attach (aThat->mData);
    }
    else
    {
        mPeer = aThat;
        mData.share (aThat->mData);
    }

    // the arbitrary ID field is not reset because
    // the copy is a shadow of the original

    mInList = aThat->mInList;

    mParent->addDependentChild (this);

    setReady (true);
    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT USBDeviceFilter::initCopy (USBController *aParent, USBDeviceFilter *aThat)
{
    LogFlowMember (("USBDeviceFilter::initCopy (%p, %p)\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = aParent;
    // mPeer is left null

    AutoLock thatlock (aThat);
    mData.attachCopy (aThat->mData);

    // reset the arbitrary ID field
    // (this field is something unique that two distinct objects, even if they
    // are deep copies of each other, should not share)
    mData->mId = NULL;

    mInList = aThat->mInList;

    mParent->addDependentChild (this);

    setReady (true);
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void USBDeviceFilter::uninit()
{
    LogFlowMember (("USBDeviceFilter::uninit()\n"));

    AutoLock alock (this);

    LogFlowMember (("USBDeviceFilter::uninit(): isReady=%d\n", isReady()));

    if (!isReady())
        return;

    mInList = false;

    mData.free();

    setReady (false);

    alock.leave();
    mParent->removeDependentChild (this);
    alock.enter();

    mPeer.setNull();
    mParent.setNull();
}

// IUSBDeviceFilter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP USBDeviceFilter::COMGETTER(Name) (BSTR *aName)
{
    if (!aName)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mName.cloneTo (aName);
    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Name) (INPTR BSTR aName)
{
    if (!aName || *aName == 0)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();
    CHECK_MACHINE_MUTABILITY (mParent->parent());

    if (mData->mName != aName)
    {
        mData.backup();
        mData->mName = aName;

        // notify parent
        alock.unlock();
        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Active) (BOOL *aActive)
{
    if (!aActive)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aActive = mData->mActive;
    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Active) (BOOL aActive)
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_MACHINE_MUTABILITY (mParent->parent());

    if (mData->mActive != aActive)
    {
        mData.backup();
        mData->mActive = aActive;

        // notify parent
        alock.unlock();
        return mParent->onDeviceFilterChange (this, TRUE /* aActiveChanged */);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(VendorId) (BSTR *aVendorId)
{
    if (!aVendorId)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mVendorId.string().cloneTo (aVendorId);
    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(VendorId) (INPTR BSTR aVendorId)
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_MACHINE_MUTABILITY (mParent->parent());

    if (mData->mVendorId.string() != aVendorId)
    {
        Data::USHORTFilter flt = aVendorId;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Vendor ID filter string '%ls' is not valid (error at position %d)"),
                aVendorId, flt.errorPosition() + 1);
#if defined (__WIN__)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aVendorId);
#endif

        mData.backup();
        mData->mVendorId = flt;

        // notify parent
        alock.unlock();
        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(ProductId) (BSTR *aProductId)
{
    if (!aProductId)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mProductId.string().cloneTo (aProductId);
    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(ProductId) (INPTR BSTR aProductId)
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_MACHINE_MUTABILITY (mParent->parent());

    if (mData->mProductId.string() != aProductId)
    {
        Data::USHORTFilter flt = aProductId;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Product ID filter string '%ls' is not valid (error at position %d)"),
                aProductId, flt.errorPosition() + 1);
#if defined (__WIN__)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aProductId);
#endif

        mData.backup();
        mData->mProductId = flt;

        // notify parent
        alock.unlock();
        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Revision) (BSTR *aRevision)
{
    if (!aRevision)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mRevision.string().cloneTo (aRevision);
    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Revision) (INPTR BSTR aRevision)
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_MACHINE_MUTABILITY (mParent->parent());

    if (mData->mRevision.string() != aRevision)
    {
        Data::USHORTFilter flt = aRevision;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Revision filter string '%ls' is not valid (error at position %d)"),
                aRevision, flt.errorPosition() + 1);
#if defined (__WIN__)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aRevision);
#endif

        mData.backup();
        mData->mRevision = flt;

        // notify parent
        alock.unlock();
        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Manufacturer) (BSTR *aManufacturer)
{
    if (!aManufacturer)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mManufacturer.string().cloneTo (aManufacturer);
    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Manufacturer) (INPTR BSTR aManufacturer)
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_MACHINE_MUTABILITY (mParent->parent());

    if (mData->mManufacturer.string() != aManufacturer)
    {
        Data::USHORTFilter flt = aManufacturer;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Manufacturer filter string '%ls' is not valid (error at position %d)"),
                aManufacturer, flt.errorPosition() + 1);

        mData.backup();
        mData->mManufacturer = flt;

        // notify parent
        alock.unlock();
        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Product) (BSTR *aProduct)
{
    if (!aProduct)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mProduct.string().cloneTo (aProduct);
    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Product) (INPTR BSTR aProduct)
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_MACHINE_MUTABILITY (mParent->parent());

    if (mData->mProduct.string() != aProduct)
    {
        Data::USHORTFilter flt = aProduct;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Product filter string '%ls' is not valid (error at position %d)"),
                aProduct, flt.errorPosition() + 1);

        mData.backup();
        mData->mProduct = flt;

        // notify parent
        alock.unlock();
        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(SerialNumber) (BSTR *aSerialNumber)
{
    if (!aSerialNumber)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mSerialNumber.string().cloneTo (aSerialNumber);
    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(SerialNumber) (INPTR BSTR aSerialNumber)
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_MACHINE_MUTABILITY (mParent->parent());

    if (mData->mSerialNumber.string() != aSerialNumber)
    {
        Data::USHORTFilter flt = aSerialNumber;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Serial number filter string '%ls' is not valid (error at position %d)"),
                aSerialNumber, flt.errorPosition() + 1);

        mData.backup();
        mData->mSerialNumber = flt;

        // notify parent
        alock.unlock();
        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Port) (BSTR *aPort)
{
    if (!aPort)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mPort.string().cloneTo (aPort);
    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Port) (INPTR BSTR aPort)
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_MACHINE_MUTABILITY (mParent->parent());

    if (mData->mPort.string() != aPort)
    {
        Data::USHORTFilter flt = aPort;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Port number filter string '%ls' is not valid (error at position %d)"),
                aPort, flt.errorPosition() + 1);
#if defined (__WIN__)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aPort);
#endif

        mData.backup();
        mData->mPort = flt;

        // notify parent
        alock.unlock();
        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Remote) (BSTR *aRemote)
{
    if (!aRemote)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mRemote.string().cloneTo (aRemote);
    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Remote) (INPTR BSTR aRemote)
{
    AutoLock alock (this);
    CHECK_READY();
    CHECK_MACHINE_MUTABILITY (mParent->parent());

    if (mData->mRemote.string() != aRemote)
    {
        Data::BOOLFilter flt = aRemote;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Remote state filter string '%ls' is not valid (error at position %d)"),
                aRemote, flt.errorPosition() + 1);

        mData.backup();
        mData->mRemote = flt;

        // notify parent
        alock.unlock();
        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

void USBDeviceFilter::commit()
{
    AutoLock alock (this);
    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            // attach new data to the peer and reshare it
            AutoLock peerlock (mPeer);
            mPeer->mData.attach (mData);
        }
    }
}

/**
 *  Cancels sharing (if any) by making an independent copy of data.
 *  This operation also resets this object's peer to NULL.
 */
void USBDeviceFilter::unshare()
{
    AutoLock alock (this);
    if (mData.isShared())
    {
        if (!mData.isBackedUp())
            mData.backup();
        mData.commit();
    }
    mPeer.setNull();
}

////////////////////////////////////////////////////////////////////////////////
// HostUSBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

HRESULT HostUSBDeviceFilter::FinalConstruct()
{
    return S_OK;
}

void HostUSBDeviceFilter::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the USB device filter object.
 */
HRESULT HostUSBDeviceFilter::init (Host *aParent,
                                   INPTR BSTR aName, BOOL aActive,
                                   INPTR BSTR aVendorId, INPTR BSTR aProductId,
                                   INPTR BSTR aRevision,
                                   INPTR BSTR aManufacturer, INPTR BSTR aProduct,
                                   INPTR BSTR aSerialNumber,
                                   INPTR BSTR aPort,
                                   USBDeviceFilterAction_T aAction)
{
    LogFlowMember (("HostUSBDeviceFilter::init(): isReady=%d\n", isReady()));

    ComAssertRet (aParent && aName && *aName, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = aParent;

    mData.allocate();
    mData->mName = aName;
    mData->mActive = aActive;
    mData->mAction = aAction;

    // initialize all filters to any match using null string
    mData->mVendorId = NULL;
    mData->mProductId = NULL;
    mData->mRevision = NULL;
    mData->mManufacturer = NULL;
    mData->mProduct = NULL;
    mData->mSerialNumber = NULL;
    mData->mPort = NULL;
    mData->mRemote = NULL;

    mInList = false;

    // use setters for the attributes below to reuse parsing errors handling
    setReady (true);
    HRESULT rc;
    do
    {
        rc = COMSETTER(VendorId) (aVendorId);
        if (FAILED (rc))
            break;
        rc = COMSETTER(ProductId) (aProductId);
        if (FAILED (rc))
            break;
        rc = COMSETTER(Revision) (aRevision);
        if (FAILED (rc))
            break;
        rc = COMSETTER(Manufacturer) (aManufacturer);
        if (FAILED (rc))
            break;
        rc = COMSETTER(Product) (aProduct);
        if (FAILED (rc))
            break;
        rc = COMSETTER(SerialNumber) (aSerialNumber);
        if (FAILED (rc))
            break;
        rc = COMSETTER(Port) (aPort);
        if (FAILED (rc))
            break;
    }
    while (0);

    if (SUCCEEDED (rc))
        mParent->addDependentChild (this);
    else
        setReady (false);

    return rc;
}

/**
 * Initializes the USB device filter object (short version).
 */
HRESULT HostUSBDeviceFilter::init (Host *aParent, INPTR BSTR aName)
{
    LogFlowMember (("HostUSBDeviceFilter::init(): isReady=%d\n", isReady()));

    ComAssertRet (aParent && aName && *aName, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = aParent;

    mData.allocate();
    mData->mName = aName;
    mData->mActive = FALSE;
    mData->mAction = USBDeviceFilterAction_USBDeviceFilterIgnore;

    mInList = false;

    // initialize all filters using null string (any match)
    mData->mVendorId = NULL;
    mData->mProductId = NULL;
    mData->mRevision = NULL;
    mData->mManufacturer = NULL;
    mData->mProduct = NULL;
    mData->mSerialNumber = NULL;
    mData->mPort = NULL;
    mData->mRemote = NULL;

    mParent->addDependentChild (this);

    setReady (true);
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void HostUSBDeviceFilter::uninit()
{
    LogFlowMember (("HostUSBDeviceFilter::uninit()\n"));

    AutoLock alock (this);

    LogFlowMember (("HostUSBDeviceFilter::uninit(): isReady=%d\n", isReady()));

    if (!isReady())
        return;

    mInList = false;

    mData.free();

    setReady (false);

    alock.leave();
    mParent->removeDependentChild (this);
    alock.enter();

    mParent.setNull();
}

// IUSBDeviceFilter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Name) (BSTR *aName)
{
    if (!aName)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mName.cloneTo (aName);
    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Name) (INPTR BSTR aName)
{
    if (!aName || *aName == 0)
        return E_INVALIDARG;

    AutoLock alock (this);
    CHECK_READY();

    if (mData->mName != aName)
    {
        mData->mName = aName;

        // notify parent
        alock.unlock();
        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Active) (BOOL *aActive)
{
    if (!aActive)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aActive = mData->mActive;
    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Active) (BOOL aActive)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mData->mActive != aActive)
    {
        mData->mActive = aActive;

        // notify parent
        alock.unlock();
        return mParent->onUSBDeviceFilterChange (this, TRUE /* aActiveChanged  */);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(VendorId) (BSTR *aVendorId)
{
    if (!aVendorId)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mVendorId.string().cloneTo (aVendorId);
    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(VendorId) (INPTR BSTR aVendorId)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mData->mVendorId.string() != aVendorId)
    {
        Data::USHORTFilter flt = aVendorId;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Vendor ID filter string '%ls' is not valid (error at position %d)"),
                aVendorId, flt.errorPosition() + 1);
#if defined (__WIN__)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aVendorId);
#endif

        mData->mVendorId = flt;

        // notify parent
        alock.unlock();
        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(ProductId) (BSTR *aProductId)
{
    if (!aProductId)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mProductId.string().cloneTo (aProductId);
    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(ProductId) (INPTR BSTR aProductId)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mData->mProductId.string() != aProductId)
    {
        Data::USHORTFilter flt = aProductId;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Product ID filter string '%ls' is not valid (error at position %d)"),
                aProductId, flt.errorPosition() + 1);
#if defined (__WIN__)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aProductId);
#endif

        mData->mProductId = flt;

        // notify parent
        alock.unlock();
        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Revision) (BSTR *aRevision)
{
    if (!aRevision)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mRevision.string().cloneTo (aRevision);
    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Revision) (INPTR BSTR aRevision)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mData->mRevision.string() != aRevision)
    {
        Data::USHORTFilter flt = aRevision;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Revision filter string '%ls' is not valid (error at position %d)"),
                aRevision, flt.errorPosition() + 1);
#if defined (__WIN__)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aRevision);
#endif

        mData->mRevision = flt;

        // notify parent
        alock.unlock();
        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Manufacturer) (BSTR *aManufacturer)
{
    if (!aManufacturer)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mManufacturer.string().cloneTo (aManufacturer);
    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Manufacturer) (INPTR BSTR aManufacturer)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mData->mManufacturer.string() != aManufacturer)
    {
        Data::USHORTFilter flt = aManufacturer;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Manufacturer filter string '%ls' is not valid (error at position %d)"),
                aManufacturer, flt.errorPosition() + 1);

        mData->mManufacturer = flt;

        // notify parent
        alock.unlock();
        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Product) (BSTR *aProduct)
{
    if (!aProduct)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mProduct.string().cloneTo (aProduct);
    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Product) (INPTR BSTR aProduct)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mData->mProduct.string() != aProduct)
    {
        Data::USHORTFilter flt = aProduct;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Product filter string '%ls' is not valid (error at position %d)"),
                aProduct, flt.errorPosition() + 1);

        mData->mProduct = flt;

        // notify parent
        alock.unlock();
        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(SerialNumber) (BSTR *aSerialNumber)
{
    if (!aSerialNumber)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mSerialNumber.string().cloneTo (aSerialNumber);
    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(SerialNumber) (INPTR BSTR aSerialNumber)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mData->mSerialNumber.string() != aSerialNumber)
    {
        Data::USHORTFilter flt = aSerialNumber;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Serial number filter string '%ls' is not valid (error at position %d)"),
                aSerialNumber, flt.errorPosition() + 1);

        mData->mSerialNumber = flt;

        // notify parent
        alock.unlock();
        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Port) (BSTR *aPort)
{
    if (!aPort)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mPort.string().cloneTo (aPort);
    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Port) (INPTR BSTR aPort)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mData->mPort.string() != aPort)
    {
        Data::USHORTFilter flt = aPort;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Port number filter string '%ls' is not valid (error at position %d)"),
                aPort, flt.errorPosition() + 1);
#if defined (__WIN__)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aPort);
#endif

        mData->mPort = flt;

        // notify parent
        alock.unlock();
        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Remote) (BSTR *aRemote)
{
    if (!aRemote)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mData->mRemote.string().cloneTo (aRemote);
    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Remote) (INPTR BSTR aRemote)
{
    return setError (E_NOTIMPL,
        tr ("The remote state filter is not supported by "
            "IHostUSBDeviceFilter objects"));
}

// IHostUSBDeviceFilter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Action) (USBDeviceFilterAction_T *aAction)
{
    if (!aAction)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    *aAction = mData->mAction;
    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Action) (USBDeviceFilterAction_T aAction)
{
    AutoLock alock (this);
    CHECK_READY();

    if (mData->mAction != aAction)
    {
        mData->mAction = aAction;

        // notify parent
        alock.unlock();
        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

