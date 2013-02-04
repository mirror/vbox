/* $Id$ */
/** @file
 * Implementation of VirtualBox COM components: USBDeviceFilter and HostUSBDeviceFilter
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

#include "USBDeviceFilterImpl.h"
#include "USBControllerImpl.h"
#include "MachineImpl.h"
#include "HostImpl.h"

#include <iprt/cpp/utils.h>
#include <VBox/settings.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Logging.h"

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
static void usbFilterFieldToString(PCUSBFILTER aFilter, USBFILTERIDX aIdx, Utf8Str &out)
{
    const USBFILTERMATCH matchingMethod = USBFilterGetMatchingMethod(aFilter, aIdx);
    Assert(matchingMethod != USBFILTERMATCH_INVALID);

    if (USBFilterIsMethodNumeric(matchingMethod))
    {
        int value = USBFilterGetNum(aFilter, aIdx);
        Assert(value >= 0 && value <= 0xffff);

        out = Utf8StrFmt("%04RX16", (uint16_t)value);
    }
    else if (USBFilterIsMethodString(matchingMethod))
    {
        out = USBFilterGetString(aFilter, aIdx);
    }
    else
        out.setNull();
}

/*static*/
const char* USBDeviceFilter::describeUSBFilterIdx(USBFILTERIDX aIdx)
{
    switch (aIdx)
    {
        case USBFILTERIDX_VENDOR_ID: return tr("Vendor ID");
        case USBFILTERIDX_PRODUCT_ID: return tr("Product ID");
        case USBFILTERIDX_DEVICE: return tr("Revision");
        case USBFILTERIDX_MANUFACTURER_STR: return tr("Manufacturer");
        case USBFILTERIDX_PRODUCT_STR: return tr("Product");
        case USBFILTERIDX_SERIAL_NUMBER_STR: return tr("Serial number");
        case USBFILTERIDX_PORT: return tr("Port number");
        default: return "";
    }
    return "";
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
/*static*/ HRESULT USBDeviceFilter::usbFilterFieldFromString(PUSBFILTER aFilter,
                                                             USBFILTERIDX aIdx,
                                                             const Utf8Str &aValue,
                                                             Utf8Str &aErrStr)
{
    int vrc;
//     Utf8Str str (aStr);
    if (aValue.isEmpty())
        vrc = USBFilterSetIgnore(aFilter, aIdx);
    else
    {
        const char *pcszValue = aValue.c_str();
        if (USBFilterIsNumericField(aIdx))
        {
            /* Is it a lonely number? */
            char *pszNext;
            uint64_t u64;
            vrc = RTStrToUInt64Ex(pcszValue, &pszNext, 16, &u64);
            if (RT_SUCCESS(vrc))
                pszNext = RTStrStripL (pszNext);
            if (    vrc == VINF_SUCCESS
                &&  !*pszNext)
            {
                if (u64 > 0xffff)
                {
                    // there was a bug writing out "-1" values in earlier versions, which got
                    // written as "FFFFFFFF"; make sure we don't fail on those
                    if (u64 == 0xffffffff)
                        u64 = 0xffff;
                    else
                    {
                        aErrStr = Utf8StrFmt(tr("The %s value '%s' is too big (max 0xFFFF)"), describeUSBFilterIdx(aIdx), pcszValue);
                        return E_INVALIDARG;
                    }
                }

                vrc = USBFilterSetNumExact(aFilter, aIdx, (uint16_t)u64, true /* fMustBePresent */);
            }
            else
                vrc = USBFilterSetNumExpression(aFilter, aIdx, pcszValue, true /* fMustBePresent */);
        }
        else
        {
            /* Any wildcard in the string? */
            Assert(USBFilterIsStringField(aIdx));
            if (    strchr(pcszValue, '*')
                ||  strchr(pcszValue, '?')
                /* || strchr (psz, '[') - later */
                )
                vrc = USBFilterSetStringPattern(aFilter, aIdx, pcszValue, true /* fMustBePresent */);
            else
                vrc = USBFilterSetStringExact(aFilter, aIdx, pcszValue, true /* fMustBePresent */);
        }
    }

    if (RT_FAILURE(vrc))
    {
        if (vrc == VERR_INVALID_PARAMETER)
        {
            aErrStr = Utf8StrFmt(tr("The %s filter expression '%s' is not valid"), describeUSBFilterIdx(aIdx), aValue.c_str());
            return E_INVALIDARG;
        }
        if (vrc == VERR_BUFFER_OVERFLOW)
        {
            aErrStr = Utf8StrFmt(tr("Insufficient expression space for the '%s' filter expression '%s'"), describeUSBFilterIdx(aIdx), aValue.c_str());
            return E_FAIL;
        }
        AssertRC(vrc);
        aErrStr = Utf8StrFmt(tr("Encountered unexpected status %Rrc when setting '%s' to '%s'"), vrc, describeUSBFilterIdx(aIdx), aValue.c_str());
        return E_FAIL;
    }

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////
// USBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

USBDeviceFilter::USBDeviceFilter()
    : mParent(NULL),
      mPeer(NULL)
{
}

USBDeviceFilter::~USBDeviceFilter()
{
}

HRESULT USBDeviceFilter::FinalConstruct()
{
    return BaseFinalConstruct();
}

void USBDeviceFilter::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the USB device filter object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT USBDeviceFilter::init(USBController *aParent,
                              const settings::USBDeviceFilter &data)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent && !data.strName.isEmpty(), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    /* mPeer is left null */

    m_fModified = false;

    mData.allocate();
    mData->mName = data.strName;
    mData->mActive = data.fActive;
    mData->mMaskedIfs = 0;

    /* initialize all filters to any match using null string */
    USBFilterInit(&mData->mUSBFilter, USBFILTERTYPE_CAPTURE);
    mData->mRemote = NULL;

    mInList = false;

    /* use setters for the attributes below to reuse parsing errors
     * handling */

    HRESULT rc = S_OK;
    do
    {
        rc = usbFilterFieldSetter(USBFILTERIDX_VENDOR_ID, data.strVendorId);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_PRODUCT_ID, data.strProductId);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_DEVICE, data.strRevision);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_MANUFACTURER_STR, data.strManufacturer);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_PRODUCT_STR, data.strProduct);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_SERIAL_NUMBER_STR, data.strSerialNumber);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_PORT, data.strPort);
        if (FAILED(rc)) break;

        rc = COMSETTER(Remote)(Bstr(data.strRemote).raw());
        if (FAILED(rc)) break;

        rc = COMSETTER(MaskedInterfaces)(data.ulMaskedInterfaces);
        if (FAILED(rc)) break;
    }
    while (0);

    /* Confirm successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 *  Initializes the USB device filter object (short version).
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT USBDeviceFilter::init(USBController *aParent, IN_BSTR aName)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent && aName && *aName, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    /* mPeer is left null */

    m_fModified = false;

    mData.allocate();

    mData->mName = aName;
    mData->mActive = FALSE;
    mData->mMaskedIfs = 0;

    /* initialize all filters to any match using null string */
    USBFilterInit (&mData->mUSBFilter, USBFILTERTYPE_CAPTURE);
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
    LogFlowThisFunc(("aParent=%p, aThat=%p, aReshare=%RTbool\n",
                      aParent, aThat, aReshare));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    m_fModified = false;

    /* sanity */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    if (aReshare)
    {
        AutoWriteLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);

        unconst(aThat->mPeer) = this;
        mData.attach (aThat->mData);
    }
    else
    {
        unconst(mPeer) = aThat;

        AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
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
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    /* mPeer is left null */

    m_fModified = false;

    /* sanity */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
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
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    mInList = false;

    mData.free();

    unconst(mPeer) = NULL;
    unconst(mParent) = NULL;
}


// IUSBDeviceFilter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP USBDeviceFilter::COMGETTER(Name) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Name) (IN_BSTR aName)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent->getMachine());
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mName != aName)
    {
        m_fModified = true;
        ComObjPtr<Machine> pMachine = mParent->getMachine();

        mData.backup();
        mData->mName = aName;

        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(pMachine COMMA_LOCKVAL_SRC_POS);
        pMachine->setModified(Machine::IsModified_USB);
        mlock.release();

        return mParent->onDeviceFilterChange(this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Active) (BOOL *aActive)
{
    CheckComArgOutPointerValid(aActive);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aActive = mData->mActive;

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Active) (BOOL aActive)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent->getMachine());
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mActive != aActive)
    {
        m_fModified = true;
        ComObjPtr<Machine> pMachine = mParent->getMachine();

        mData.backup();
        mData->mActive = aActive;

        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(pMachine COMMA_LOCKVAL_SRC_POS);
        pMachine->setModified(Machine::IsModified_USB);
        mlock.release();

        return mParent->onDeviceFilterChange(this, TRUE /* aActiveChanged */);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(VendorId) (BSTR *aVendorId)
{
    return usbFilterFieldGetter(USBFILTERIDX_VENDOR_ID, aVendorId);
}

STDMETHODIMP USBDeviceFilter::COMSETTER(VendorId) (IN_BSTR aVendorId)
{
    return usbFilterFieldSetter(USBFILTERIDX_VENDOR_ID, aVendorId);
}

STDMETHODIMP USBDeviceFilter::COMGETTER(ProductId) (BSTR *aProductId)
{
    return usbFilterFieldGetter(USBFILTERIDX_PRODUCT_ID, aProductId);
}

STDMETHODIMP USBDeviceFilter::COMSETTER(ProductId) (IN_BSTR aProductId)
{
    return usbFilterFieldSetter(USBFILTERIDX_PRODUCT_ID, aProductId);
 }

STDMETHODIMP USBDeviceFilter::COMGETTER(Revision) (BSTR *aRevision)
{
    return usbFilterFieldGetter(USBFILTERIDX_DEVICE, aRevision);
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Revision) (IN_BSTR aRevision)
{
    return usbFilterFieldSetter(USBFILTERIDX_DEVICE, aRevision);
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Manufacturer) (BSTR *aManufacturer)
{
    return usbFilterFieldGetter(USBFILTERIDX_MANUFACTURER_STR, aManufacturer);
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Manufacturer) (IN_BSTR aManufacturer)
{
    return usbFilterFieldSetter(USBFILTERIDX_MANUFACTURER_STR, aManufacturer);
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Product) (BSTR *aProduct)
{
    return usbFilterFieldGetter(USBFILTERIDX_PRODUCT_STR, aProduct);
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Product) (IN_BSTR aProduct)
{
    return usbFilterFieldSetter(USBFILTERIDX_PRODUCT_STR, aProduct);
}

STDMETHODIMP USBDeviceFilter::COMGETTER(SerialNumber) (BSTR *aSerialNumber)
{
    return usbFilterFieldGetter(USBFILTERIDX_SERIAL_NUMBER_STR, aSerialNumber);
}

STDMETHODIMP USBDeviceFilter::COMSETTER(SerialNumber) (IN_BSTR aSerialNumber)
{
    return usbFilterFieldSetter(USBFILTERIDX_SERIAL_NUMBER_STR, aSerialNumber);
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Port) (BSTR *aPort)
{
    return usbFilterFieldGetter(USBFILTERIDX_PORT, aPort);
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Port) (IN_BSTR aPort)
{
    return usbFilterFieldSetter(USBFILTERIDX_PORT, aPort);
}

STDMETHODIMP USBDeviceFilter::COMGETTER(Remote) (BSTR *aRemote)
{
    CheckComArgOutPointerValid(aRemote);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mRemote.string().cloneTo(aRemote);

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(Remote) (IN_BSTR aRemote)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent->getMachine());
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mRemote.string() != aRemote)
    {
        Data::BOOLFilter flt = aRemote;
        ComAssertRet(!flt.isNull(), E_FAIL);
        if (!flt.isValid())
            return setError(E_INVALIDARG,
                            tr("Remote state filter string '%ls' is not valid (error at position %d)"),
                            aRemote, flt.errorPosition() + 1);

        m_fModified = true;
        ComObjPtr<Machine> pMachine = mParent->getMachine();

        mData.backup();
        mData->mRemote = flt;

        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(pMachine COMMA_LOCKVAL_SRC_POS);
        pMachine->setModified(Machine::IsModified_USB);
        mlock.release();

        return mParent->onDeviceFilterChange(this);
    }

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMGETTER(MaskedInterfaces) (ULONG *aMaskedIfs)
{
    CheckComArgOutPointerValid(aMaskedIfs);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMaskedIfs = mData->mMaskedIfs;

    return S_OK;
}

STDMETHODIMP USBDeviceFilter::COMSETTER(MaskedInterfaces) (ULONG aMaskedIfs)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent->getMachine());
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mMaskedIfs != aMaskedIfs)
    {
        m_fModified = true;
        ComObjPtr<Machine> pMachine = mParent->getMachine();

        mData.backup();
        mData->mMaskedIfs = aMaskedIfs;

        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(pMachine COMMA_LOCKVAL_SRC_POS);
        pMachine->setModified(Machine::IsModified_USB);
        mlock.release();

        return mParent->onDeviceFilterChange(this);
    }

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

bool USBDeviceFilter::isModified()
{
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    return m_fModified;
}

/**
 *  @note Locks this object for writing.
 */
void USBDeviceFilter::rollback()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.rollback();
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void USBDeviceFilter::commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(mPeer, this COMMA_LOCKVAL_SRC_POS);

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
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* peer is not modified, lock it for reading (mPeer is "master" so locked
     * first) */
    AutoReadLock rl(mPeer COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    if (mData.isShared())
    {
        if (!mData.isBackedUp())
            mData.backup();

        mData.commit();
    }

    unconst(mPeer) = NULL;
}

/**
 *  Generic USB filter field getter; converts the field value to UTF-16.
 *
 *  @param  aIdx    The field index.
 *  @param  aStr    Where to store the value.
 *
 *  @return COM status.
 */
HRESULT USBDeviceFilter::usbFilterFieldGetter(USBFILTERIDX aIdx, BSTR *aStr)
{
    CheckComArgOutPointerValid(aStr);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str str;
    usbFilterFieldToString(&mData->mUSBFilter, aIdx, str);

    str.cloneTo(aStr);

    return S_OK;
}

/**
 *  Generic USB filter field setter, expects UTF-8 input.
 *
 *  @param  aIdx    The field index.
 *  @param  aStr    The new value.
 *
 *  @return COM status.
 */
HRESULT USBDeviceFilter::usbFilterFieldSetter(USBFILTERIDX aIdx,
                                              const Utf8Str &strNew)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent->getMachine());
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str strOld;
    usbFilterFieldToString(&mData->mUSBFilter, aIdx, strOld);
    if (strOld != strNew)
    {
        m_fModified = true;
        ComObjPtr<Machine> pMachine = mParent->getMachine();

        mData.backup();

        Utf8Str errStr;
        HRESULT rc = usbFilterFieldFromString(&mData->mUSBFilter, aIdx, strNew, errStr);
        if (FAILED(rc))
        {
            mData.rollback();
            return setError(rc, "%s", errStr.c_str());
        }

        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(pMachine COMMA_LOCKVAL_SRC_POS);
        pMachine->setModified(Machine::IsModified_USB);
        mlock.release();

        return mParent->onDeviceFilterChange(this);
    }

    return S_OK;
}

/**
 *  Generic USB filter field setter, expects UTF-16 input.
 *
 *  @param  aIdx    The field index.
 *  @param  aStr    The new value.
 *
 *  @return COM status.
 */
HRESULT USBDeviceFilter::usbFilterFieldSetter(USBFILTERIDX aIdx,
                                              IN_BSTR aStr)
{
    return usbFilterFieldSetter(aIdx, Utf8Str(aStr));
}


////////////////////////////////////////////////////////////////////////////////
// HostUSBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

HostUSBDeviceFilter::HostUSBDeviceFilter()
    : mParent(NULL)
{
}

HostUSBDeviceFilter::~HostUSBDeviceFilter()
{
}


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
HRESULT HostUSBDeviceFilter::init(Host *aParent,
                                  const settings::USBDeviceFilter &data)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent && !data.strName.isEmpty(), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addChild(this);

    mData.allocate();
    mData->mName = data.strName;
    mData->mActive = data.fActive;
    USBFilterInit (&mData->mUSBFilter, USBFILTERTYPE_IGNORE);
    mData->mRemote = NULL;
    mData->mMaskedIfs = 0;

    mInList = false;

    /* use setters for the attributes below to reuse parsing errors
     * handling */

    HRESULT rc = S_OK;
    do
    {
        rc = COMSETTER(Action)(data.action);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_VENDOR_ID, data.strVendorId);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_PRODUCT_ID, data.strProductId);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_DEVICE, data.strRevision);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_MANUFACTURER_STR, data.strManufacturer);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_PRODUCT_ID, data.strProduct);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_SERIAL_NUMBER_STR, data.strSerialNumber);
        if (FAILED(rc)) break;

        rc = usbFilterFieldSetter(USBFILTERIDX_PORT, data.strPort);
        if (FAILED(rc)) break;
    }
    while (0);

    /* Confirm successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 *  Initializes the USB device filter object (short version).
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT HostUSBDeviceFilter::init (Host *aParent, IN_BSTR aName)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent && aName && *aName, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addChild(this);

    mData.allocate();

    mData->mName = aName;
    mData->mActive = FALSE;
    mInList = false;
    USBFilterInit (&mData->mUSBFilter, USBFILTERTYPE_IGNORE);
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
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    mInList = false;

    mData.free();

    mParent->removeChild(this);

    unconst(mParent) = NULL;
}

/**
 * Most of the USB bits are protect by one lock to simplify things.
 * This lock is currently the one of the Host object, which happens
 * to be our parent.
 */
RWLockHandle *HostUSBDeviceFilter::lockHandle() const
{
    return mParent->lockHandle();
}


// IUSBDeviceFilter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Name) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Name) (IN_BSTR aName)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mName != aName)
    {
        mData->mName = aName;

        /* leave the lock before informing callbacks */
        alock.release();

        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Active) (BOOL *aActive)
{
    CheckComArgOutPointerValid(aActive);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aActive = mData->mActive;

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Active) (BOOL aActive)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mActive != aActive)
    {
        mData->mActive = aActive;

        /* leave the lock before informing callbacks */
        alock.release();

        return mParent->onUSBDeviceFilterChange (this, TRUE /* aActiveChanged  */);
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(VendorId) (BSTR *aVendorId)
{
    return usbFilterFieldGetter(USBFILTERIDX_VENDOR_ID, aVendorId);
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(VendorId) (IN_BSTR aVendorId)
{
    return usbFilterFieldSetter(USBFILTERIDX_VENDOR_ID, aVendorId);
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(ProductId) (BSTR *aProductId)
{
    return usbFilterFieldGetter(USBFILTERIDX_PRODUCT_ID, aProductId);
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(ProductId) (IN_BSTR aProductId)
{
    return usbFilterFieldSetter(USBFILTERIDX_PRODUCT_ID, aProductId);
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Revision) (BSTR *aRevision)
{
    return usbFilterFieldGetter(USBFILTERIDX_DEVICE, aRevision);
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Revision) (IN_BSTR aRevision)
{
    return usbFilterFieldSetter(USBFILTERIDX_DEVICE, aRevision);
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Manufacturer) (BSTR *aManufacturer)
{
    return usbFilterFieldGetter(USBFILTERIDX_MANUFACTURER_STR, aManufacturer);
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Manufacturer) (IN_BSTR aManufacturer)
{
    return usbFilterFieldSetter(USBFILTERIDX_MANUFACTURER_STR, aManufacturer);
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Product) (BSTR *aProduct)
{
    return usbFilterFieldGetter(USBFILTERIDX_PRODUCT_STR, aProduct);
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Product) (IN_BSTR aProduct)
{
    return usbFilterFieldSetter(USBFILTERIDX_PRODUCT_STR, aProduct);
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(SerialNumber) (BSTR *aSerialNumber)
{
    return usbFilterFieldGetter(USBFILTERIDX_SERIAL_NUMBER_STR, aSerialNumber);
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(SerialNumber) (IN_BSTR aSerialNumber)
{
    return usbFilterFieldSetter(USBFILTERIDX_SERIAL_NUMBER_STR, aSerialNumber);
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Port) (BSTR *aPort)
{
    return usbFilterFieldGetter(USBFILTERIDX_PORT, aPort);
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Port) (IN_BSTR aPort)
{
    return usbFilterFieldSetter(USBFILTERIDX_PORT, aPort);
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Remote) (BSTR *aRemote)
{
    CheckComArgOutPointerValid(aRemote);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mRemote.string().cloneTo(aRemote);

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Remote) (IN_BSTR /* aRemote */)
{
    return setError(E_NOTIMPL,
                    tr("The remote state filter is not supported by IHostUSBDeviceFilter objects"));
}

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(MaskedInterfaces) (ULONG *aMaskedIfs)
{
    CheckComArgOutPointerValid(aMaskedIfs);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMaskedIfs = mData->mMaskedIfs;

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(MaskedInterfaces) (ULONG /* aMaskedIfs */)
{
    return setError(E_NOTIMPL,
                    tr("The masked interfaces property is not applicable to IHostUSBDeviceFilter objects"));
}

// IHostUSBDeviceFilter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HostUSBDeviceFilter::COMGETTER(Action) (USBDeviceFilterAction_T *aAction)
{
    CheckComArgOutPointerValid(aAction);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    switch (USBFilterGetFilterType (&mData->mUSBFilter))
    {
        case USBFILTERTYPE_IGNORE:   *aAction = USBDeviceFilterAction_Ignore; break;
        case USBFILTERTYPE_CAPTURE:  *aAction = USBDeviceFilterAction_Hold; break;
        default:                     *aAction = USBDeviceFilterAction_Null; break;
    }

    return S_OK;
}

STDMETHODIMP HostUSBDeviceFilter::COMSETTER(Action) (USBDeviceFilterAction_T aAction)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    USBFILTERTYPE filterType;
    switch (aAction)
    {
        case USBDeviceFilterAction_Ignore:   filterType = USBFILTERTYPE_IGNORE; break;
        case USBDeviceFilterAction_Hold:     filterType = USBFILTERTYPE_CAPTURE; break;
        case USBDeviceFilterAction_Null:
            return setError(E_INVALIDARG,
                            tr("Action value InvalidUSBDeviceFilterAction is not permitted"));
        default:
            return setError(E_INVALIDARG,
                            tr("Invalid action %d"),
                            aAction);
    }
    if (USBFilterGetFilterType (&mData->mUSBFilter) != filterType)
    {
        int vrc = USBFilterSetFilterType (&mData->mUSBFilter, filterType);
        if (RT_FAILURE(vrc))
            return setError(E_INVALIDARG,
                            tr("Unexpected error %Rrc"),
                            vrc);

        /* leave the lock before informing callbacks */
        alock.release();

        return mParent->onUSBDeviceFilterChange (this);
    }

    return S_OK;
}

/**
 *  Generic USB filter field getter.
 *
 *  @param  aIdx    The field index.
 *  @param  aStr    Where to store the value.
 *
 *  @return COM status.
 */
HRESULT HostUSBDeviceFilter::usbFilterFieldGetter(USBFILTERIDX aIdx, BSTR *aStr)
{
    CheckComArgOutPointerValid(aStr);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str str;
    usbFilterFieldToString(&mData->mUSBFilter, aIdx, str);

    str.cloneTo(aStr);

    return S_OK;
}

void HostUSBDeviceFilter::saveSettings(settings::USBDeviceFilter &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.strName = mData->mName;
    data.fActive = !!mData->mActive;

    usbFilterFieldToString(&mData->mUSBFilter, USBFILTERIDX_VENDOR_ID, data.strVendorId);
    usbFilterFieldToString(&mData->mUSBFilter, USBFILTERIDX_PRODUCT_ID, data.strProductId);
    usbFilterFieldToString(&mData->mUSBFilter, USBFILTERIDX_DEVICE, data.strRevision);
    usbFilterFieldToString(&mData->mUSBFilter, USBFILTERIDX_MANUFACTURER_STR, data.strManufacturer);
    usbFilterFieldToString(&mData->mUSBFilter, USBFILTERIDX_PRODUCT_STR, data.strProduct);
    usbFilterFieldToString(&mData->mUSBFilter, USBFILTERIDX_SERIAL_NUMBER_STR, data.strSerialNumber);
    usbFilterFieldToString(&mData->mUSBFilter, USBFILTERIDX_PORT, data.strPort);

    COMGETTER(Action)(&data.action);
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
HRESULT HostUSBDeviceFilter::usbFilterFieldSetter(USBFILTERIDX aIdx, Bstr aStr)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str strOld;
    Utf8Str strNew(aStr);
    usbFilterFieldToString(&mData->mUSBFilter, aIdx, strOld);
    if (strOld != strNew)
    {
        //mData.backup();

        Utf8Str errStr;
        HRESULT rc = USBDeviceFilter::usbFilterFieldFromString(&mData->mUSBFilter, aIdx, aStr, errStr);
        if (FAILED(rc))
        {
            //mData.rollback();
            return setError(rc, "%s", errStr.c_str());
        }

        /* leave the lock before informing callbacks */
        alock.release();

        return mParent->onUSBDeviceFilterChange(this);
    }

    return S_OK;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
