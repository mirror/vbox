/* $Id$ */
/** @file
 * Declaration of USBDeviceFilter and HostUSBDeviceFilter.
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

#ifndef ____H_USBDEVICEFILTERIMPL
#define ____H_USBDEVICEFILTERIMPL

#include "VirtualBoxBase.h"

#include "Matching.h"
#include <VBox/usbfilter.h>

class USBController;
class Host;
namespace settings
{
    struct USBDeviceFilter;
}

// USBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE USBDeviceFilter :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IUSBDeviceFilter)
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

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(USBDeviceFilter, IUSBDeviceFilter)

    DECLARE_NOT_AGGREGATABLE(USBDeviceFilter)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(USBDeviceFilter)
        VBOX_DEFAULT_INTERFACE_ENTRIES  (IUSBDeviceFilter)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (USBDeviceFilter)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(USBController *aParent,
                 const settings::USBDeviceFilter &data);
    HRESULT init(USBController *aParent, IN_BSTR aName);
    HRESULT init(USBController *aParent, USBDeviceFilter *aThat,
                 bool aReshare = false);
    HRESULT initCopy(USBController *aParent, USBDeviceFilter *aThat);
    void uninit();

    // IUSBDeviceFilter properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMSETTER(Name)) (IN_BSTR aName);
    STDMETHOD(COMGETTER(Active)) (BOOL *aActive);
    STDMETHOD(COMSETTER(Active)) (BOOL aActive);
    STDMETHOD(COMGETTER(VendorId)) (BSTR *aVendorId);
    STDMETHOD(COMSETTER(VendorId)) (IN_BSTR aVendorId);
    STDMETHOD(COMGETTER(ProductId)) (BSTR *aProductId);
    STDMETHOD(COMSETTER(ProductId)) (IN_BSTR aProductId);
    STDMETHOD(COMGETTER(Revision)) (BSTR *aRevision);
    STDMETHOD(COMSETTER(Revision)) (IN_BSTR aRevision);
    STDMETHOD(COMGETTER(Manufacturer)) (BSTR *aManufacturer);
    STDMETHOD(COMSETTER(Manufacturer)) (IN_BSTR aManufacturer);
    STDMETHOD(COMGETTER(Product)) (BSTR *aProduct);
    STDMETHOD(COMSETTER(Product)) (IN_BSTR aProduct);
    STDMETHOD(COMGETTER(SerialNumber)) (BSTR *aSerialNumber);
    STDMETHOD(COMSETTER(SerialNumber)) (IN_BSTR aSerialNumber);
    STDMETHOD(COMGETTER(Port)) (BSTR *aPort);
    STDMETHOD(COMSETTER(Port)) (IN_BSTR aPort);
    STDMETHOD(COMGETTER(Remote)) (BSTR *aRemote);
    STDMETHOD(COMSETTER(Remote)) (IN_BSTR aRemote);
    STDMETHOD(COMGETTER(MaskedInterfaces)) (ULONG *aMaskedIfs);
    STDMETHOD(COMSETTER(MaskedInterfaces)) (ULONG aMaskedIfs);

    // public methods only for internal purposes
    bool isModified();
    void rollback();
    void commit();

    void unshare();

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    void *& getId() { return mData.data()->mId; }

    const Data& getData() { return *mData.data(); }
    ComObjPtr<USBDeviceFilter> peer() { return mPeer; }

    // tr() wants to belong to a class it seems, thus this one here.
    static HRESULT usbFilterFieldFromString(PUSBFILTER aFilter,
                                            USBFILTERIDX aIdx,
                                            const Utf8Str &aValue,
                                            Utf8Str &aErrStr);

    static const char* describeUSBFilterIdx(USBFILTERIDX aIdx);

private:
    HRESULT usbFilterFieldGetter(USBFILTERIDX aIdx, BSTR *aStr);
    HRESULT usbFilterFieldSetter(USBFILTERIDX aIdx, IN_BSTR aStr);
    HRESULT usbFilterFieldSetter(USBFILTERIDX aIdx, const Utf8Str &strNew);

    USBController * const       mParent;
    USBDeviceFilter * const     mPeer;

    Backupable<Data> mData;

    bool m_fModified;

    /** Used externally to indicate this filter is in the list
        (not touched by the class itself except that in init()/uninit()) */
    bool mInList;

    friend class USBController;
};

// HostUSBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE HostUSBDeviceFilter :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IHostUSBDeviceFilter)
{
public:

    struct Data : public USBDeviceFilter::Data
    {
        Data() {}
    };

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(HostUSBDeviceFilter, IHostUSBDeviceFilter)

    DECLARE_NOT_AGGREGATABLE(HostUSBDeviceFilter)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HostUSBDeviceFilter)
        COM_INTERFACE_ENTRY(IUSBDeviceFilter)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IHostUSBDeviceFilter)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (HostUSBDeviceFilter)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Host *aParent,
                 const settings::USBDeviceFilter &data);
    HRESULT init(Host *aParent, IN_BSTR aName);
    void uninit();

    // IUSBDeviceFilter properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMSETTER(Name)) (IN_BSTR aName);
    STDMETHOD(COMGETTER(Active)) (BOOL *aActive);
    STDMETHOD(COMSETTER(Active)) (BOOL aActive);
    STDMETHOD(COMGETTER(VendorId)) (BSTR *aVendorId);
    STDMETHOD(COMSETTER(VendorId)) (IN_BSTR aVendorId);
    STDMETHOD(COMGETTER(ProductId)) (BSTR *aProductId);
    STDMETHOD(COMSETTER(ProductId)) (IN_BSTR aProductId);
    STDMETHOD(COMGETTER(Revision)) (BSTR *aRevision);
    STDMETHOD(COMSETTER(Revision)) (IN_BSTR aRevision);
    STDMETHOD(COMGETTER(Manufacturer)) (BSTR *aManufacturer);
    STDMETHOD(COMSETTER(Manufacturer)) (IN_BSTR aManufacturer);
    STDMETHOD(COMGETTER(Product)) (BSTR *aProduct);
    STDMETHOD(COMSETTER(Product)) (IN_BSTR aProduct);
    STDMETHOD(COMGETTER(SerialNumber)) (BSTR *aSerialNumber);
    STDMETHOD(COMSETTER(SerialNumber)) (IN_BSTR aSerialNumber);
    STDMETHOD(COMGETTER(Port)) (BSTR *aPort);
    STDMETHOD(COMSETTER(Port)) (IN_BSTR aPort);
    STDMETHOD(COMGETTER(Remote)) (BSTR *aRemote);
    STDMETHOD(COMSETTER(Remote)) (IN_BSTR aRemote);
    STDMETHOD(COMGETTER(MaskedInterfaces)) (ULONG *aMaskedIfs);
    STDMETHOD(COMSETTER(MaskedInterfaces)) (ULONG aMaskedIfs);

    // IHostUSBDeviceFilter properties
    STDMETHOD(COMGETTER(Action)) (USBDeviceFilterAction_T *aAction);
    STDMETHOD(COMSETTER(Action)) (USBDeviceFilterAction_T aAction);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)
    void saveSettings(settings::USBDeviceFilter &data);

    void*& getId() { return mData.data()->mId; }

    const Data& getData() { return *mData.data(); }

    // util::Lockable interface
    RWLockHandle *lockHandle() const;

private:
    HRESULT usbFilterFieldGetter(USBFILTERIDX aIdx, BSTR *aStr);
    HRESULT usbFilterFieldSetter(USBFILTERIDX aIdx, Bstr aStr);

    Host * const        mParent;

    Backupable<Data>    mData;

    /** Used externally to indicate this filter is in the list
        (not touched by the class itself except that in init()/uninit()) */
    bool mInList;

    friend class Host;
};

#endif // ____H_USBDEVICEFILTERIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
