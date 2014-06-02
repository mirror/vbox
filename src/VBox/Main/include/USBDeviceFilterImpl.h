/* $Id$ */
/** @file
 * Declaration of USBDeviceFilter and HostUSBDeviceFilter.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_USBDEVICEFILTERIMPL
#define ____H_USBDEVICEFILTERIMPL

#include "Matching.h"
#include <VBox/usbfilter.h>
#include "USBDeviceFilterWrap.h"

class USBDeviceFilters;
class Host;
namespace settings
{
    struct USBDeviceFilter;
}

// USBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE USBDeviceFilter :
    public USBDeviceFilterWrap
{
public:

    struct Data
    {
        typedef matching::Matchable <matching::ParsedBoolFilter> BOOLFilter;

        Data() : mActive (FALSE), mMaskedIfs (0), mId (NULL) {}
        Data (const Data &aThat) : mName (aThat.mName), mActive (aThat.mActive),
            mRemote (aThat.mRemote), mMaskedIfs (aThat.mMaskedIfs) , mId (aThat.mId)
        {
            USBFilterClone (&mUSBFilter, &aThat.mUSBFilter);
        }

        /** The filter name. */
        Bstr mName;
        /** Indicates whether the filter is active or not. */
        BOOL mActive;
        /** Remote or local matching criterion. */
        BOOLFilter mRemote;
        /** The filter data blob. */
        USBFILTER mUSBFilter;

        /** Interface masking bit mask that should be applied to matching devices. */
        ULONG mMaskedIfs;

        /** Arbitrary ID field (not used by the class itself) */
        void *mId;
    };

    DECLARE_EMPTY_CTOR_DTOR(USBDeviceFilter)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(USBDeviceFilters *aParent,
                 const settings::USBDeviceFilter &data);
    HRESULT init(USBDeviceFilters *aParent, IN_BSTR aName);
    HRESULT init(USBDeviceFilters *aParent, USBDeviceFilter *aThat,
                 bool aReshare = false);
    HRESULT initCopy(USBDeviceFilters *aParent, USBDeviceFilter *aThat);
    void uninit();

    // public methods only for internal purposes
    bool i_isModified();
    void i_rollback();
    void i_commit();

    void unshare();

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)
    void *& i_getId() { return mData.data()->mId; }
    const Data& i_getData() { return *mData.data(); }
    ComObjPtr<USBDeviceFilter> i_peer() { return mPeer; }

    // tr() wants to belong to a class it seems, thus this one here.
    static HRESULT i_usbFilterFieldFromString(PUSBFILTER aFilter,
                                              USBFILTERIDX aIdx,
                                              const Utf8Str &aValue,
                                              Utf8Str &aErrStr);

    static const char* i_describeUSBFilterIdx(USBFILTERIDX aIdx);

private:

    // wrapped IUSBDeviceFilter properties
    HRESULT getName(com::Utf8Str &aName);
    HRESULT setName(const com::Utf8Str &aName);
    HRESULT getActive(BOOL *aActive);
    HRESULT setActive(BOOL aActive);
    HRESULT getVendorId(com::Utf8Str &aVendorId);
    HRESULT setVendorId(const com::Utf8Str &aVendorId);
    HRESULT getProductId(com::Utf8Str &aProductId);
    HRESULT setProductId(const com::Utf8Str &aProductId);
    HRESULT getRevision(com::Utf8Str &aRevision);
    HRESULT setRevision(const com::Utf8Str &aRevision);
    HRESULT getManufacturer(com::Utf8Str &aManufacturer);
    HRESULT setManufacturer(const com::Utf8Str &aManufacturer);
    HRESULT getProduct(com::Utf8Str &aProduct);
    HRESULT setProduct(const com::Utf8Str &aProduct);
    HRESULT getSerialNumber(com::Utf8Str &aSerialNumber);
    HRESULT setSerialNumber(const com::Utf8Str &aSerialNumber);
    HRESULT getPort(com::Utf8Str &aPort);
    HRESULT setPort(const com::Utf8Str &aPort);
    HRESULT getRemote(com::Utf8Str &aRemote);
    HRESULT setRemote(const com::Utf8Str &aRemote);
    HRESULT getMaskedInterfaces(ULONG *aMaskedInterfaces);
    HRESULT setMaskedInterfaces(ULONG aMaskedInterfaces);

    // wrapped IUSBDeviceFilter methods
    HRESULT i_usbFilterFieldGetter(USBFILTERIDX aIdx, com::Utf8Str &aStr);
    HRESULT i_usbFilterFieldSetter(USBFILTERIDX aIdx, const com::Utf8Str &strNew);

    USBDeviceFilters * const     mParent;
    USBDeviceFilter  * const     mPeer;

    Backupable<Data> mData;

    bool m_fModified;

    /** Used externally to indicate this filter is in the list
        (not touched by the class itself except that in init()/uninit()) */
    bool mInList;

    friend class USBDeviceFilters;
};
#include "HostUSBDeviceFilterWrap.h"

// HostUSBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE HostUSBDeviceFilter :
    public HostUSBDeviceFilterWrap
{
public:

    struct Data : public USBDeviceFilter::Data
    {
        Data() {}
    };

    DECLARE_EMPTY_CTOR_DTOR (HostUSBDeviceFilter)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Host *aParent,
                 const settings::USBDeviceFilter &data);
    HRESULT init(Host *aParent, IN_BSTR aName);
    void uninit();

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)
    void i_saveSettings(settings::USBDeviceFilter &data);

    void*& i_getId() { return mData.data()->mId; }

    const Data& i_getData() { return *mData.data(); }

    // util::Lockable interface
    RWLockHandle *lockHandle() const;

private:

    // wrapped IHostUSBDeviceFilter properties
    HRESULT getName(com::Utf8Str &aName);
    HRESULT setName(const com::Utf8Str &aName);
    HRESULT getActive(BOOL *aActive);
    HRESULT setActive(BOOL aActive);
    HRESULT getVendorId(com::Utf8Str &aVendorId);
    HRESULT setVendorId(const com::Utf8Str &aVendorId);
    HRESULT getProductId(com::Utf8Str &aProductId);
    HRESULT setProductId(const com::Utf8Str &aProductId);
    HRESULT getRevision(com::Utf8Str &aRevision);
    HRESULT setRevision(const com::Utf8Str &aRevision);
    HRESULT getManufacturer(com::Utf8Str &aManufacturer);
    HRESULT setManufacturer(const com::Utf8Str &aManufacturer);
    HRESULT getProduct(com::Utf8Str &aProduct);
    HRESULT setProduct(const com::Utf8Str &aProduct);
    HRESULT getSerialNumber(com::Utf8Str &aSerialNumber);
    HRESULT setSerialNumber(const com::Utf8Str &aSerialNumber);
    HRESULT getPort(com::Utf8Str &aPort);
    HRESULT setPort(const com::Utf8Str &aPort);
    HRESULT getRemote(com::Utf8Str &aRemote);
    HRESULT setRemote(const com::Utf8Str &aRemote);
    HRESULT getMaskedInterfaces(ULONG *aMaskedInterfaces);
    HRESULT setMaskedInterfaces(ULONG aMaskedInterfaces);

    // wrapped IHostUSBDeviceFilter properties
    HRESULT getAction(USBDeviceFilterAction_T *aAction);
    HRESULT setAction(USBDeviceFilterAction_T aAction);

    HRESULT i_usbFilterFieldGetter(USBFILTERIDX aIdx, com::Utf8Str &aStr);
    HRESULT i_usbFilterFieldSetter(USBFILTERIDX aIdx, const com::Utf8Str &aStr);

    Host * const        mParent;

    Backupable<Data>    mData;

    /** Used externally to indicate this filter is in the list
        (not touched by the class itself except that in init()/uninit()) */
    bool mInList;

    friend class Host;
};

#endif // ____H_USBDEVICEFILTERIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
