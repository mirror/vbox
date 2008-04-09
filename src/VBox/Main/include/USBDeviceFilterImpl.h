/** @file
 *
 * Declaration of VirtualBox COM components:
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

#ifndef ____H_USBDEVICEFILTERIMPL
#define ____H_USBDEVICEFILTERIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

#include "Matching.h"
#include <VBox/usbfilter.h>

class USBController;
class Host;

// USBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE USBDeviceFilter :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <USBDeviceFilter, IUSBDeviceFilter>,
    public VirtualBoxSupportTranslation <USBDeviceFilter>,
    public IUSBDeviceFilter
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

        bool operator== (const Data &that) const
        {
            return this == &that
                || (    mName == that.mName
                    &&  mActive == that.mActive
                    &&  mMaskedIfs == that.mMaskedIfs
                    &&  USBFilterIsIdentical (&mUSBFilter, &that.mUSBFilter));
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

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (USBDeviceFilter)

    DECLARE_NOT_AGGREGATABLE(USBDeviceFilter)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(USBDeviceFilter)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IUSBDeviceFilter)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (USBDeviceFilter)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (USBController *aParent,
                  INPTR BSTR aName, BOOL aActive,
                  INPTR BSTR aVendorId, INPTR BSTR aProductId,
                  INPTR BSTR aRevision,
                  INPTR BSTR aManufacturer, INPTR BSTR aProduct,
                  INPTR BSTR aSerialNumber,
                  INPTR BSTR aPort, INPTR BSTR aRemote,
                  ULONG aMaskedIfs);
    HRESULT init (USBController *aParent, INPTR BSTR aName);
    HRESULT init (USBController *aParent, USBDeviceFilter *aThat,
                  bool aReshare = false);
    HRESULT initCopy (USBController *aParent, USBDeviceFilter *aThat);
    void uninit();

    // IUSBDeviceFilter properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMSETTER(Name)) (INPTR BSTR aName);
    STDMETHOD(COMGETTER(Active)) (BOOL *aActive);
    STDMETHOD(COMSETTER(Active)) (BOOL aActive);
    STDMETHOD(COMGETTER(VendorId)) (BSTR *aVendorId);
    STDMETHOD(COMSETTER(VendorId)) (INPTR BSTR aVendorId);
    STDMETHOD(COMGETTER(ProductId)) (BSTR *aProductId);
    STDMETHOD(COMSETTER(ProductId)) (INPTR BSTR aProductId);
    STDMETHOD(COMGETTER(Revision)) (BSTR *aRevision);
    STDMETHOD(COMSETTER(Revision)) (INPTR BSTR aRevision);
    STDMETHOD(COMGETTER(Manufacturer)) (BSTR *aManufacturer);
    STDMETHOD(COMSETTER(Manufacturer)) (INPTR BSTR aManufacturer);
    STDMETHOD(COMGETTER(Product)) (BSTR *aProduct);
    STDMETHOD(COMSETTER(Product)) (INPTR BSTR aProduct);
    STDMETHOD(COMGETTER(SerialNumber)) (BSTR *aSerialNumber);
    STDMETHOD(COMSETTER(SerialNumber)) (INPTR BSTR aSerialNumber);
    STDMETHOD(COMGETTER(Port)) (BSTR *aPort);
    STDMETHOD(COMSETTER(Port)) (INPTR BSTR aPort);
    STDMETHOD(COMGETTER(Remote)) (BSTR *aRemote);
    STDMETHOD(COMSETTER(Remote)) (INPTR BSTR aRemote);
    STDMETHOD(COMGETTER(MaskedInterfaces)) (ULONG *aMaskedIfs);
    STDMETHOD(COMSETTER(MaskedInterfaces)) (ULONG aMaskedIfs);

    // public methods only for internal purposes

    bool isModified() { AutoLock alock (this); return mData.isBackedUp(); }
    bool isReallyModified() { AutoLock alock (this); return mData.hasActualChanges(); }
    bool rollback();
    void commit();

    void unshare();

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    void *& id() { return mData.data()->mId; }

    const Data &data() { return *mData.data(); }
    ComObjPtr <USBDeviceFilter> peer() { return mPeer; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"USBDeviceFilter"; }

    // tr() wants to belong to a class it seems, thus this one here.
    static HRESULT usbFilterFieldFromString (PUSBFILTER aFilter, USBFILTERIDX aIdx, INPTR BSTR aStr, const char *aName, Utf8Str &aErrStr);

private:
    HRESULT usbFilterFieldGetter (USBFILTERIDX aIdx, BSTR *aStr);
    HRESULT usbFilterFieldSetter (USBFILTERIDX aIdx, Bstr aStr, Utf8Str aName);

    const ComObjPtr <USBController, ComWeakRef> mParent;
    const ComObjPtr <USBDeviceFilter> mPeer;

    Backupable <Data> mData;

    /** Used externally to indicate this filter is in the list
        (not touched by the class itslef except that in init()/uninit()) */
    bool mInList;

    friend class USBController;
};

// HostUSBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE HostUSBDeviceFilter :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <HostUSBDeviceFilter, IHostUSBDeviceFilter>,
    public VirtualBoxSupportTranslation <HostUSBDeviceFilter>,
    public IHostUSBDeviceFilter
{
public:

    struct Data : public USBDeviceFilter::Data
    {
        Data() {}
    };

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (HostUSBDeviceFilter)

    DECLARE_NOT_AGGREGATABLE(HostUSBDeviceFilter)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HostUSBDeviceFilter)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IUSBDeviceFilter)
        COM_INTERFACE_ENTRY(IHostUSBDeviceFilter)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (HostUSBDeviceFilter)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Host *aParent,
                  INPTR BSTR aName, BOOL aActive,
                  INPTR BSTR aVendorId, INPTR BSTR aProductId,
                  INPTR BSTR aRevision,
                  INPTR BSTR aManufacturer, INPTR BSTR aProduct,
                  INPTR BSTR aSerialNumber,
                  INPTR BSTR aPort,
                  USBDeviceFilterAction_T aAction);
    HRESULT init (Host *aParent, INPTR BSTR aName);
    void uninit();

    // IUSBDeviceFilter properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMSETTER(Name)) (INPTR BSTR aName);
    STDMETHOD(COMGETTER(Active)) (BOOL *aActive);
    STDMETHOD(COMSETTER(Active)) (BOOL aActive);
    STDMETHOD(COMGETTER(VendorId)) (BSTR *aVendorId);
    STDMETHOD(COMSETTER(VendorId)) (INPTR BSTR aVendorId);
    STDMETHOD(COMGETTER(ProductId)) (BSTR *aProductId);
    STDMETHOD(COMSETTER(ProductId)) (INPTR BSTR aProductId);
    STDMETHOD(COMGETTER(Revision)) (BSTR *aRevision);
    STDMETHOD(COMSETTER(Revision)) (INPTR BSTR aRevision);
    STDMETHOD(COMGETTER(Manufacturer)) (BSTR *aManufacturer);
    STDMETHOD(COMSETTER(Manufacturer)) (INPTR BSTR aManufacturer);
    STDMETHOD(COMGETTER(Product)) (BSTR *aProduct);
    STDMETHOD(COMSETTER(Product)) (INPTR BSTR aProduct);
    STDMETHOD(COMGETTER(SerialNumber)) (BSTR *aSerialNumber);
    STDMETHOD(COMSETTER(SerialNumber)) (INPTR BSTR aSerialNumber);
    STDMETHOD(COMGETTER(Port)) (BSTR *aPort);
    STDMETHOD(COMSETTER(Port)) (INPTR BSTR aPort);
    STDMETHOD(COMGETTER(Remote)) (BSTR *aRemote);
    STDMETHOD(COMSETTER(Remote)) (INPTR BSTR aRemote);
    STDMETHOD(COMGETTER(MaskedInterfaces)) (ULONG *aMaskedIfs);
    STDMETHOD(COMSETTER(MaskedInterfaces)) (ULONG aMaskedIfs);

    // IHostUSBDeviceFilter properties
    STDMETHOD(COMGETTER(Action)) (USBDeviceFilterAction_T *aAction);
    STDMETHOD(COMSETTER(Action)) (USBDeviceFilterAction_T aAction);

    // public methods only for internal purposes

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    void *& id() { return mData.data()->mId; }

    const Data &data() { return *mData.data(); }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostUSBDeviceFilter"; }

private:
    HRESULT usbFilterFieldGetter (USBFILTERIDX aIdx, BSTR *aStr);
    HRESULT usbFilterFieldSetter (USBFILTERIDX aIdx, Bstr aStr, Utf8Str aName);

    const ComObjPtr <Host, ComWeakRef> mParent;

    Backupable <Data> mData;

    /** Used externally to indicate this filter is in the list
        (not touched by the class itslef except that in init()/uninit()) */
    bool mInList;

    friend class Host;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION (USBDeviceFilter)
COM_DECL_READONLY_ENUM_AND_COLLECTION (HostUSBDeviceFilter)

/**
 *  Separate IUSBDeviceCollection implementation that is constructed from a list of
 *  IUSBDevice instances (as opposed to a collection defined in IUSBDeviceImpl.h
 *  constructed from OUSBDevice lists).
 */
COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_BEGIN (IfaceUSBDevice, IUSBDevice)

    STDMETHOD(FindById) (INPTR GUIDPARAM aId, IUSBDevice **aDevice)
    {
        /* internal collection, no need to implement */
        return E_NOTIMPL;
    }

    STDMETHOD(FindByAddress) (INPTR BSTR aAddress, IUSBDevice **aDevice)
    {
        /* internal collection, no need to implement */
        return E_NOTIMPL;
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END (IfaceUSBDevice, IUSBDevice)

#endif // ____H_USBDEVICEFILTERIMPL
