/** @file
 *
 * Implementation of VirtualBox COM components:
 * USBDeviceFilter and HostUSBDeviceFilter
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

#include "USBDeviceFilterImpl.h"
#include "USBControllerImpl.h"
#include "MachineImpl.h"
#include "HostImpl.h"
#include "Logging.h"

#include <iprt/cpputils.h>

#ifdef VBOX_WITH_USBFILTER
////////////////////////////////////////////////////////////////////////////////
// Internal Helpers
////////////////////////////////////////////////////////////////////////////////

/**
 *  Converts a USBFilter field into a string.
 *
 *  (This function is also used by HostUSBDeviceFilter.)
 *
 *  @param  aFilter     The filter.
 *  @param  aIdx        The field index.
 *  @param  aStr        The output string.
 */
static void usbFilterFieldToString (PCUSBFILTER aFilter, USBFILTERIDX aIdx, BSTR *aStr)
{
    const USBFILTERMATCH matchingMethod = USBFilterGetMatchingMethod (aFilter, aIdx);
    Assert (matchingMethod != USBFILTERMATCH_INVALID);

    if (USBFilterIsMethodNumeric (matchingMethod))
    {
        int value = USBFilterGetNum (aFilter, aIdx);
        Assert(value >= 0 && value <= 0xffff);

        char str[16];
        RTStrPrintf (str, sizeof(str), "%x", value);
        Bstr ((const char *)str).cloneTo (aStr);
    }
    else if (USBFilterIsMethodString (matchingMethod))
        Bstr (USBFilterGetString (aFilter, aIdx)).cloneTo (aStr);
    else
        Bstr ().cloneTo (aStr);
}

/**
 *  Interprets a string and assigns it to a USBFilter field.
 *
 *  (This function is also used by HostUSBDeviceFilter.)
 *
 *  @param  aFilter     The filter.
 *  @param  aIdx        The field index.
 *  @param  aStr        The input string.
 *  @param  aName       The field name for use in the error string.
 *  @param  aErrStr     Where to return the error string on failure.
 *
 *  @return COM status code.
 *  @remark The idea was to have this as a static function, but tr() doesn't wanna work without a class :-/
 */
/*static*/ HRESULT
USBDeviceFilter::usbFilterFieldFromString (PUSBFILTER aFilter, USBFILTERIDX aIdx, INPTR BSTR aStr, const char *aName, Utf8Str &aErrStr)
{
    int vrc;
    Utf8Str str (aStr);
    if (str.isNull())
        vrc = USBFilterSetIgnore (aFilter, aIdx);
    else
    {
        const char *psz = str;
        if (USBFilterIsNumericField (aIdx))
        {
            /* Is it a lonely number? */
            char *pszNext;
            uint64_t u64;
            vrc = RTStrToUInt64Ex (psz, &pszNext, 16, &u64);
            if (RT_SUCCESS(vrc))
                pszNext = RTStrStripL (pszNext);
            if (    vrc == VINF_SUCCESS
                &&  !*pszNext)
            {
                if (u64 > 0xffff)
                {
                    aErrStr = Utf8StrFmt (tr ("The %s value '%s' is too big (max 0xFFFF)"), aName, str.raw());
                    return E_INVALIDARG;
                }

                vrc = USBFilterSetNumExact (aFilter, aIdx, (uint16_t)u64, true /* fMustBePresent */);
            }
            else
                vrc = USBFilterSetNumExpression (aFilter, aIdx, psz, true /* fMustBePresent */);
        }
        else
        {
            /* Any wildcard in the string? */
            Assert (USBFilterIsStringField (aIdx));
            if (    strchr (psz, '*')
                ||  strchr (psz, '?')
                /* || strchr (psz, '[') - later */
                )
                vrc = USBFilterSetStringPattern (aFilter, aIdx, psz, true /* fMustBePresent */);
            else
                vrc = USBFilterSetStringExact (aFilter, aIdx, psz, true /* fMustBePresent */);
        }
    }

    if (RT_FAILURE(vrc))
    {
        if (vrc == VERR_INVALID_PARAMETER)
        {
            aErrStr = Utf8StrFmt (tr ("The %s filter expression '%s' is not valid"), aName, str.raw());
            return E_INVALIDARG;
        }
        if (vrc == VERR_BUFFER_OVERFLOW)
        {
            aErrStr = Utf8StrFmt (tr ("Insufficient expression space for the '%s' filter expression '%s'"), aName, str.raw());
            return E_FAIL;
        }
        AssertRC (vrc);
        aErrStr = Utf8StrFmt (tr ("Encountered unexpected status %Vrc when setting '%s' to '%s'"), vrc, aName, str.raw());
        return E_FAIL;
    }

    return S_OK;
}
#endif /* VBOX_WITH_USBFILTER */


////////////////////////////////////////////////////////////////////////////////
// USBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (USBDeviceFilter)

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
 *  Initializes the USB device filter object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT USBDeviceFilter::init (USBController *aParent,
                               INPTR BSTR aName, BOOL aActive,
                               INPTR BSTR aVendorId, INPTR BSTR aProductId,
                               INPTR BSTR aRevision,
                               INPTR BSTR aManufacturer, INPTR BSTR aProduct,
                               INPTR BSTR aSerialNumber,
                               INPTR BSTR aPort, INPTR BSTR aRemote,
                               ULONG aMaskedIfs)
{
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aParent && aName && *aName, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    mData.allocate();
    mData->mName = aName;
    mData->mActive = aActive;
    mData->mMaskedIfs = 0;

    /* initialize all filters to any match using null string */
#ifndef VBOX_WITH_USBFILTER
    mData->mVendorId = NULL;
    mData->mProductId = NULL;
    mData->mRevision = NULL;
    mData->mManufacturer = NULL;
    mData->mProduct = NULL;
    mData->mSerialNumber = NULL;
    mData->mPort = NULL;
#else  /* VBOX_WITH_USBFILTER */
    USBFilterInit(&mData->mUSBFilter, USBFILTERTYPE_CAPTURE);
#endif /* VBOX_WITH_USBFILTER */
    mData->mRemote = NULL;

    mInList = false;

    /* use setters for the attributes below to reuse parsing errors
     * handling */

    HRESULT rc = S_OK;
    do
    {
        rc = COMSETTER(VendorId) (aVendorId);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(ProductId) (aProductId);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(Revision) (aRevision);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(Manufacturer) (aManufacturer);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(Product) (aProduct);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(SerialNumber) (aSerialNumber);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(Port) (aPort);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(Remote) (aRemote);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(MaskedInterfaces) (aMaskedIfs);
        CheckComRCBreakRC (rc);
    }
    while (0);

    /* Confirm successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 *  Initializes the USB device filter object (short version).
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT USBDeviceFilter::init (USBController *aParent, INPTR BSTR aName)
{
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aParent && aName && *aName, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    mData.allocate();

    mData->mName = aName;
    mData->mActive = FALSE;
    mData->mMaskedIfs = 0;

    /* initialize all filters to any match using null string */
#ifndef VBOX_WITH_USBFILTER
    mData->mVendorId = NULL;
    mData->mProductId = NULL;
    mData->mRevision = NULL;
    mData->mManufacturer = NULL;
    mData->mProduct = NULL;
    mData->mSerialNumber = NULL;
    mData->mPort = NULL;
#else  /* VBOX_WITH_USBFILTER */
    USBFilterInit (&mData->mUSBFilter, USBFILTERTYPE_CAPTURE);
#endif /* VBOX_WITH_USBFILTER */
    mData->mRemote = NULL;

    mInList = false;

    /* Confirm successful initialization */
    autoInitSpan.setSucceeded();

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
 *
 *  @note Locks @a aThat object for writing if @a aReshare is @c true, or for
 *  reading if @a aReshare is false.
 */
HRESULT USBDeviceFilter::init (USBController *aParent, USBDeviceFilter *aThat,
                               bool aReshare /* = false */)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p, aReshare=%RTbool\n",
                      aParent, aThat, aReshare));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    /* sanity */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    if (aReshare)
    {
        AutoLock thatLock (aThat);

        unconst (aThat->mPeer) = this;
        mData.attach (aThat->mData);
    }
    else
    {
        unconst (mPeer) = aThat;

        AutoReaderLock thatLock (aThat);
        mData.share (aThat->mData);
    }

    /* the arbitrary ID field is not reset because
     * the copy is a shadow of the original */

    mInList = aThat->mInList;

    /* Confirm successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT USBDeviceFilter::initCopy (USBController *aParent, USBDeviceFilter *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    /* sanity */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReaderLock thatLock (aThat);
    mData.attachCopy (aThat->mData);

    /* reset the arbitrary ID field
     * (this field is something unique that two distinct objects, even if they
     * are deep copies of each other, should not share) */
    mData->mId = NULL;

    mInList = aThat->mInList;

    /* Confirm successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void USBDeviceFilter::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mInList = false;

    mData.free();

    mParent->removeDependentChild (this);

    unconst (mPeer).setNull();
    unconst (mParent).setNull();
}

// IUSBDeviceFilter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP USBDeviceFilter::COMGETTER(Name) (BSTR *aName)
{
    if (!aName)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mName.cloneTo (aName);

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Name) (INPTR BSTR aName)
{
    if (!aName || *aName == 0)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mName != aName)
    {
        mData.backup();
        mData->mName = aName;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Active) (BOOL *aActive)
{
    if (!aActive)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aActive = mData->mActive;

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Active) (BOOL aActive)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mActive != aActive)
    {
        mData.backup();
        mData->mActive = aActive;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this, TRUE /* aActiveChanged */);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(VendorId) (BSTR *aVendorId)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aVendorId)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mVendorId.string().cloneTo (aVendorId);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_VENDOR_ID, aVendorId);
#endif
}

STDMETHODIMP USBDeviceFilter::COMSETTER(VendorId) (INPTR BSTR aVendorId)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mVendorId.string() != aVendorId)
    {
        Data::USHORTFilter flt = aVendorId;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Vendor ID filter string '%ls' is not valid (error at position %d)"),
                aVendorId, flt.errorPosition() + 1);
#if defined (RT_OS_WINDOWS)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aVendorId);
#endif

        mData.backup();
        mData->mVendorId = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_VENDOR_ID, aVendorId, tr ("Vendor ID"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP USBDeviceFilter::COMGETTER(ProductId) (BSTR *aProductId)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aProductId)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mProductId.string().cloneTo (aProductId);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_PRODUCT_ID, aProductId);
#endif
}

STDMETHODIMP USBDeviceFilter::COMSETTER(ProductId) (INPTR BSTR aProductId)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mProductId.string() != aProductId)
    {
        Data::USHORTFilter flt = aProductId;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Product ID filter string '%ls' is not valid (error at position %d)"),
                aProductId, flt.errorPosition() + 1);
#if defined (RT_OS_WINDOWS)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aProductId);
#endif

        mData.backup();
        mData->mProductId = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_PRODUCT_ID, aProductId, tr ("Product ID"));
#endif /* VBOX_WITH_USBFILTER */
 }

STDMETHODIMP USBDeviceFilter::COMGETTER(Revision) (BSTR *aRevision)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aRevision)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mRevision.string().cloneTo (aRevision);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_DEVICE, aRevision);
#endif
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Revision) (INPTR BSTR aRevision)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mRevision.string() != aRevision)
    {
        Data::USHORTFilter flt = aRevision;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Revision filter string '%ls' is not valid (error at position %d)"),
                aRevision, flt.errorPosition() + 1);
#if defined (RT_OS_WINDOWS)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aRevision);
#endif

        mData.backup();
        mData->mRevision = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_DEVICE, aRevision, tr ("Revision"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Manufacturer) (BSTR *aManufacturer)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aManufacturer)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mManufacturer.string().cloneTo (aManufacturer);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_MANUFACTURER_STR, aManufacturer);
#endif
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Manufacturer) (INPTR BSTR aManufacturer)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mManufacturer.string() != aManufacturer)
    {
        Data::BstrFilter flt = aManufacturer;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Manufacturer filter string '%ls' is not valid (error at position %d)"),
                aManufacturer, flt.errorPosition() + 1);

        mData.backup();
        mData->mManufacturer = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_MANUFACTURER_STR, aManufacturer, tr ("Manufacturer"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Product) (BSTR *aProduct)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aProduct)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mProduct.string().cloneTo (aProduct);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_PRODUCT_STR, aProduct);
#endif
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Product) (INPTR BSTR aProduct)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mProduct.string() != aProduct)
    {
        Data::BstrFilter flt = aProduct;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Product filter string '%ls' is not valid (error at position %d)"),
                aProduct, flt.errorPosition() + 1);

        mData.backup();
        mData->mProduct = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_PRODUCT_STR, aProduct, tr ("Product"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP USBDeviceFilter::COMGETTER(SerialNumber) (BSTR *aSerialNumber)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aSerialNumber)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mSerialNumber.string().cloneTo (aSerialNumber);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_SERIAL_NUMBER_STR, aSerialNumber);
#endif
}

STDMETHODIMP USBDeviceFilter::COMSETTER(SerialNumber) (INPTR BSTR aSerialNumber)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mSerialNumber.string() != aSerialNumber)
    {
        Data::BstrFilter flt = aSerialNumber;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Serial number filter string '%ls' is not valid (error at position %d)"),
                aSerialNumber, flt.errorPosition() + 1);

        mData.backup();
        mData->mSerialNumber = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_SERIAL_NUMBER_STR, aSerialNumber, tr ("Serial number"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Port) (BSTR *aPort)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aPort)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mPort.string().cloneTo (aPort);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_PORT, aPort);
#endif
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Port) (INPTR BSTR aPort)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mPort.string() != aPort)
    {
        Data::USHORTFilter flt = aPort;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Port number filter string '%ls' is not valid (error at position %d)"),
                aPort, flt.errorPosition() + 1);
#if defined (RT_OS_WINDOWS)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aPort);
#endif

        mData.backup();
        mData->mPort = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_PORT, aPort, tr ("Port number"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Remote) (BSTR *aRemote)
{
    if (!aRemote)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mRemote.string().cloneTo (aRemote);

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Remote) (INPTR BSTR aRemote)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

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

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(MaskedInterfaces) (ULONG *aMaskedIfs)
{
    if (!aMaskedIfs)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aMaskedIfs = mData->mMaskedIfs;

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(MaskedInterfaces) (ULONG aMaskedIfs)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mMaskedIfs != aMaskedIfs)
    {
        mData.backup();
        mData->mMaskedIfs = aMaskedIfs;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/**
 *  @note Locks this object for writing.
 */
bool USBDeviceFilter::rollback()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoLock alock (this);

    bool changed = false;

    if (mData.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        changed = mData.hasActualChanges();
        mData.rollback();
    }

    return changed;
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void USBDeviceFilter::commit()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (mPeer);
    AssertComRCReturnVoid (thatCaller.rc());

    /* lock both for writing since we modify both */
    AutoMultiLock <2> alock (this->wlock(), AutoLock::maybeWlock (mPeer));

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->mData.attach (mData);
        }
    }
}

/**
 *  Cancels sharing (if any) by making an independent copy of data.
 *  This operation also resets this object's peer to NULL.
 *
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void USBDeviceFilter::unshare()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (mPeer);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading */
    AutoMultiLock <2> alock (this->wlock(), AutoLock::maybeRlock (mPeer));

    if (mData.isShared())
    {

        if (!mData.isBackedUp())
            mData.backup();

        mData.commit();
    }

    unconst (mPeer).setNull();
}

#ifdef VBOX_WITH_USBFILTER
/**
 *  Generic USB filter field getter.
 *
 *  @param  aIdx    The field index.
 *  @param  aStr    Where to store the value.
 *
 *  @return COM status.
 */
HRESULT USBDeviceFilter::usbFilterFieldGetter (USBFILTERIDX aIdx, BSTR *aStr)
{
    if (!aStr)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    usbFilterFieldToString (&mData->mUSBFilter, aIdx, aStr);

    return S_OK;
}

/**
 *  Generic USB filter field setter.
 *
 *  @param  aIdx    The field index.
 *  @param  aStr    The new value.
 *  @param  aName   The translated field name (for error messages).
 *
 *  @return COM status.
 */
HRESULT USBDeviceFilter::usbFilterFieldSetter (USBFILTERIDX aIdx, Bstr aStr, Utf8Str aName)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent->parent());
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    Bstr old;
    usbFilterFieldToString (&mData->mUSBFilter, aIdx, old.asOutParam());
    if (old != aStr)
    {
        mData.backup();

        Utf8Str errStr;
        HRESULT rc = usbFilterFieldFromString (&mData->mUSBFilter, aIdx, aStr, aName, errStr);
        if (FAILED (rc))
        {
            mData.rollback();
            return setError (rc, "%s", errStr.raw());
        }

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onDeviceFilterChange (this);
    }

    return S_OK;
}
#endif /* VBOX_WITH_USBFILTER */

////////////////////////////////////////////////////////////////////////////////
// HostUSBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (HostUSBDeviceFilter)

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
 *  Initializes the USB device filter object.
 *
 *  @param aParent  Handle of the parent object.
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
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aParent && aName && *aName, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    mData.allocate();
    mData->mName = aName;
    mData->mActive = aActive;
#ifndef VBOX_WITH_USBFILTER
    mData->mAction = aAction;

    /* initialize all filters to any match using null string */
    mData->mVendorId = NULL;
    mData->mProductId = NULL;
    mData->mRevision = NULL;
    mData->mManufacturer = NULL;
    mData->mProduct = NULL;
    mData->mSerialNumber = NULL;
    mData->mPort = NULL;
#else  /* VBOX_WITH_USBFILTER */
    USBFilterInit (&mData->mUSBFilter, USBFILTERTYPE_IGNORE);
#endif /* VBOX_WITH_USBFILTER */
    mData->mRemote = NULL;
    mData->mMaskedIfs = 0;

    mInList = false;

    /* use setters for the attributes below to reuse parsing errors
     * handling */

    HRESULT rc = S_OK;
    do
    {
#ifndef VBOX_WITH_USBFILTER
        rc = COMSETTER(Action) (aAction);
        CheckComRCBreakRC (rc);
#endif /* VBOX_WITH_USBFILTER */
        rc = COMSETTER(VendorId) (aVendorId);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(ProductId) (aProductId);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(Revision) (aRevision);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(Manufacturer) (aManufacturer);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(Product) (aProduct);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(SerialNumber) (aSerialNumber);
        CheckComRCBreakRC (rc);
        rc = COMSETTER(Port) (aPort);
        CheckComRCBreakRC (rc);
    }
    while (0);

    /* Confirm successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 *  Initializes the USB device filter object (short version).
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT HostUSBDeviceFilter::init (Host *aParent, INPTR BSTR aName)
{
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aParent && aName && *aName, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    mData.allocate();

    mData->mName = aName;
    mData->mActive = FALSE;
#ifndef VBOX_WITH_USBFILTER
    mData->mAction = USBDeviceFilterAction_USBDeviceFilterIgnore;
#endif /* !VBOX_WITH_USBFILTER */

    mInList = false;

#ifndef VBOX_WITH_USBFILTER
    /* initialize all filters to any match using null string */
    mData->mVendorId = NULL;
    mData->mProductId = NULL;
    mData->mRevision = NULL;
    mData->mManufacturer = NULL;
    mData->mProduct = NULL;
    mData->mSerialNumber = NULL;
    mData->mPort = NULL;
#else  /* VBOX_WITH_USBFILTER */
    USBFilterInit (&mData->mUSBFilter, USBFILTERTYPE_IGNORE);
#endif /* VBOX_WITH_USBFILTER */
    mData->mRemote = NULL;
    mData->mMaskedIfs = 0;

    /* Confirm successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void HostUSBDeviceFilter::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mInList = false;

    mData.free();

    mParent->removeDependentChild (this);

    unconst (mParent).setNull();
}

// IUSBDeviceFilter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Name) (BSTR *aName)
{
    if (!aName)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mName.cloneTo (aName);

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Name) (INPTR BSTR aName)
{
    if (!aName || *aName == 0)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mData->mName != aName)
    {
        mData->mName = aName;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Active) (BOOL *aActive)
{
    if (!aActive)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aActive = mData->mActive;

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Active) (BOOL aActive)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mData->mActive != aActive)
    {
        mData->mActive = aActive;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this, TRUE /* aActiveChanged  */);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(VendorId) (BSTR *aVendorId)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aVendorId)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mVendorId.string().cloneTo (aVendorId);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_VENDOR_ID, aVendorId);
#endif
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(VendorId) (INPTR BSTR aVendorId)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mData->mVendorId.string() != aVendorId)
    {
        Data::USHORTFilter flt = aVendorId;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Vendor ID filter string '%ls' is not valid (error at position %d)"),
                aVendorId, flt.errorPosition() + 1);
#if defined (RT_OS_WINDOWS)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aVendorId);
#endif

        mData->mVendorId = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_VENDOR_ID, aVendorId, tr ("Vendor ID"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(ProductId) (BSTR *aProductId)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aProductId)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mProductId.string().cloneTo (aProductId);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_PRODUCT_ID, aProductId);
#endif
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(ProductId) (INPTR BSTR aProductId)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mData->mProductId.string() != aProductId)
    {
        Data::USHORTFilter flt = aProductId;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Product ID filter string '%ls' is not valid (error at position %d)"),
                aProductId, flt.errorPosition() + 1);
#if defined (RT_OS_WINDOWS)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aProductId);
#endif

        mData->mProductId = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_PRODUCT_ID, aProductId, tr ("Product ID"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Revision) (BSTR *aRevision)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aRevision)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mRevision.string().cloneTo (aRevision);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_DEVICE, aRevision);
#endif
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Revision) (INPTR BSTR aRevision)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mData->mRevision.string() != aRevision)
    {
        Data::USHORTFilter flt = aRevision;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Revision filter string '%ls' is not valid (error at position %d)"),
                aRevision, flt.errorPosition() + 1);
#if defined (RT_OS_WINDOWS)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aRevision);
#endif

        mData->mRevision = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_DEVICE, aRevision, tr ("Revision"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Manufacturer) (BSTR *aManufacturer)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aManufacturer)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mManufacturer.string().cloneTo (aManufacturer);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_MANUFACTURER_STR, aManufacturer);
#endif
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Manufacturer) (INPTR BSTR aManufacturer)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mData->mManufacturer.string() != aManufacturer)
    {
        Data::BstrFilter flt = aManufacturer;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Manufacturer filter string '%ls' is not valid (error at position %d)"),
                aManufacturer, flt.errorPosition() + 1);

        mData->mManufacturer = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_MANUFACTURER_STR, aManufacturer, tr ("Manufacturer"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Product) (BSTR *aProduct)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aProduct)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mProduct.string().cloneTo (aProduct);

    return S_OK;
#else
    return usbFilterFieldGetter (USBFILTERIDX_PRODUCT_STR, aProduct);
#endif
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Product) (INPTR BSTR aProduct)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mData->mProduct.string() != aProduct)
    {
        Data::BstrFilter flt = aProduct;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Product filter string '%ls' is not valid (error at position %d)"),
                aProduct, flt.errorPosition() + 1);

        mData->mProduct = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_PRODUCT_STR, aProduct, tr ("Product"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(SerialNumber) (BSTR *aSerialNumber)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aSerialNumber)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mSerialNumber.string().cloneTo (aSerialNumber);

    return S_OK;
#else   /* VBOX_WITH_USBFILTER */
    return usbFilterFieldGetter (USBFILTERIDX_SERIAL_NUMBER_STR, aSerialNumber);
#endif  /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(SerialNumber) (INPTR BSTR aSerialNumber)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mData->mSerialNumber.string() != aSerialNumber)
    {
        Data::BstrFilter flt = aSerialNumber;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Serial number filter string '%ls' is not valid (error at position %d)"),
                aSerialNumber, flt.errorPosition() + 1);

        mData->mSerialNumber = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_SERIAL_NUMBER_STR, aSerialNumber, tr ("Serial number"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Port) (BSTR *aPort)
{
#ifndef VBOX_WITH_USBFILTER
    if (!aPort)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mPort.string().cloneTo (aPort);

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldGetter (USBFILTERIDX_PORT, aPort);
#endif  /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Port) (INPTR BSTR aPort)
{
#ifndef VBOX_WITH_USBFILTER
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    if (mData->mPort.string() != aPort)
    {
        Data::USHORTFilter flt = aPort;
        ComAssertRet (!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError (E_INVALIDARG,
                tr ("Port number filter string '%ls' is not valid (error at position %d)"),
                aPort, flt.errorPosition() + 1);
#if defined (RT_OS_WINDOWS)
        // intervalic filters are temporarily disabled
        if (!flt.first().isNull() && flt.first().isValid())
            return setError (E_INVALIDARG,
                tr ("'%ls': Intervalic filters are not currently available on this platform"),
                aPort);
#endif

        mData->mPort = flt;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
#else  /* VBOX_WITH_USBFILTER */
    return usbFilterFieldSetter (USBFILTERIDX_PORT, aPort, tr ("Port number"));
#endif /* VBOX_WITH_USBFILTER */
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Remote) (BSTR *aRemote)
{
    if (!aRemote)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mRemote.string().cloneTo (aRemote);

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Remote) (INPTR BSTR aRemote)
{
    return setError (E_NOTIMPL,
        tr ("The remote state filter is not supported by "
            "IHostUSBDeviceFilter objects"));
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(MaskedInterfaces) (ULONG *aMaskedIfs)
{
    if (!aMaskedIfs)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aMaskedIfs = mData->mMaskedIfs;

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(MaskedInterfaces) (INPTR ULONG aMaskedIfs)
{
    return setError (E_NOTIMPL,
        tr ("The masked interfaces property is not applicable to IHostUSBDeviceFilter objects"));
}

// IHostUSBDeviceFilter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Action) (USBDeviceFilterAction_T *aAction)
{
    if (!aAction)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

#ifndef VBOX_WITH_USBFILTER
    *aAction = mData->mAction;
#else   /* VBOX_WITH_USBFILTER */
    switch (USBFilterGetFilterType (&mData->mUSBFilter))
    {
        case USBFILTERTYPE_IGNORE:   *aAction = USBDeviceFilterAction_USBDeviceFilterIgnore; break;
        case USBFILTERTYPE_CAPTURE:  *aAction = USBDeviceFilterAction_USBDeviceFilterHold; break;
        default:                     *aAction = USBDeviceFilterAction_InvalidUSBDeviceFilterAction; break;
    }
#endif  /* VBOX_WITH_USBFILTER */

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Action) (USBDeviceFilterAction_T aAction)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

#ifndef VBOX_WITH_USBFILTER
    if (mData->mAction != aAction)
    {
        mData->mAction = aAction;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this);
    }

#else   /* VBOX_WITH_USBFILTER */
    USBFILTERTYPE filterType;
    switch (aAction)
    {
        case USBDeviceFilterAction_USBDeviceFilterIgnore:   filterType = USBFILTERTYPE_IGNORE; break;
        case USBDeviceFilterAction_USBDeviceFilterHold:     filterType = USBFILTERTYPE_CAPTURE; break;
        case USBDeviceFilterAction_InvalidUSBDeviceFilterAction:
            return setError (E_INVALIDARG,
                tr ("Action value InvalidUSBDeviceFilterAction is not permitted"));
        default:
            return setError (E_INVALIDARG,
                tr ("Invalid action %d"),
                aAction);
    }
    if (USBFilterGetFilterType (&mData->mUSBFilter) != filterType)
    {
        int vrc = USBFilterSetFilterType (&mData->mUSBFilter, filterType);
        if (RT_FAILURE (vrc))
            return setError (E_INVALIDARG,
                tr ("Unexpected error %Vrc"),
                vrc);

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this);
    }
#endif  /* VBOX_WITH_USBFILTER */

    return S_OK;
}


#ifdef VBOX_WITH_USBFILTER
/**
 *  Generic USB filter field getter.
 *
 *  @param  aIdx    The field index.
 *  @param  aStr    Where to store the value.
 *
 *  @return COM status.
 */
HRESULT HostUSBDeviceFilter::usbFilterFieldGetter (USBFILTERIDX aIdx, BSTR *aStr)
{
    if (!aStr)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    usbFilterFieldToString (&mData->mUSBFilter, aIdx, aStr);

    return S_OK;
}


/**
 *  Generic USB filter field setter.
 *
 *  @param  aIdx    The field index.
 *  @param  aStr    The new value.
 *  @param  aName   The translated field name (for error messages).
 *
 *  @return COM status.
 */
HRESULT HostUSBDeviceFilter::usbFilterFieldSetter (USBFILTERIDX aIdx, Bstr aStr, Utf8Str aName)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    Bstr old;
    usbFilterFieldToString (&mData->mUSBFilter, aIdx, old.asOutParam());
    if (old != aStr)
    {
        //mData.backup();

        Utf8Str errStr;
        HRESULT rc = USBDeviceFilter::usbFilterFieldFromString (&mData->mUSBFilter, aIdx, aStr, aName, errStr);
        if (FAILED (rc))
        {
            //mData.rollback();
            return setError (rc, "%s", errStr.raw());
        }

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}
#endif /* VBOX_WITH_USBFILTER */

