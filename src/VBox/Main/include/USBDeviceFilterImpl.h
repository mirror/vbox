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
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ____H_USBDEVICEFILTERIMPL
#define ____H_USBDEVICEFILTERIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

#include "Matching.h"

class USBController;
class Host;

// USBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE USBDeviceFilter :
    public VirtualBoxSupportErrorInfoImpl <USBDeviceFilter, IUSBDeviceFilter>,
    public VirtualBoxSupportTranslation <USBDeviceFilter>,
    public VirtualBoxBase,
    public IUSBDeviceFilter
{
public:

    struct Data
    {
        struct ConvForRegexp
        {
            inline static Bstr toBstr (const USHORT &aValue)
            {
                return Bstr (Utf8StrFmt ("%04X", aValue));
            }

            inline static const Bstr &toBstr (const Bstr &aValue)
            {
                return aValue;
            }
        };

        typedef matching::Matchable
            <matching::TwoParsedFilters <matching::ParsedIntervalFilter <USHORT>,
                                         matching::ParsedRegexpFilter <ConvForRegexp, true, 4, 4> > >
            USHORTFilter;

        typedef matching::Matchable
            <matching::ParsedRegexpFilter <ConvForRegexp, false> > BstrFilter;

        typedef matching::Matchable <matching::ParsedBoolFilter> BOOLFilter;

        Data() : mActive (FALSE), mId (NULL) {}

        bool operator== (const Data &that) const
        {
            return this == &that ||
                   (mName == that.mName &&
                    mActive == that.mActive &&
                    mVendorId.string() == that. mVendorId.string() &&
                    mProductId.string() == that. mProductId.string() &&
                    mRevision.string() == that. mRevision.string() &&
                    mManufacturer.string() == that. mManufacturer.string() &&
                    mProduct.string() == that. mProduct.string() &&
                    mSerialNumber.string() == that. mSerialNumber.string() &&
                    mPort.string() == that. mPort.string() &&
                    mRemote.string() == that. mRemote.string());
        }

        Bstr mName;
        BOOL mActive;

        USHORTFilter mVendorId;
        USHORTFilter mProductId;
        USHORTFilter mRevision;
        BstrFilter mManufacturer;
        BstrFilter mProduct;
        BstrFilter mSerialNumber;
        USHORTFilter mPort;
        BOOLFilter mRemote;

        /** Arbitrary ID field (not used by the class itself) */
        void *mId;
    };

    DECLARE_NOT_AGGREGATABLE(USBDeviceFilter)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(USBDeviceFilter)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IUSBDeviceFilter)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (USBController *aParent,
                  INPTR BSTR aName, BOOL aActive,
                  INPTR BSTR aVendorId, INPTR BSTR aProductId,
                  INPTR BSTR aRevision,
                  INPTR BSTR aManufacturer, INPTR BSTR aProduct,
                  INPTR BSTR aSerialNumber,
                  INPTR BSTR aPort, INPTR BSTR aRemote);
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

    // public methods only for internal purposes

    void *& id() { return mData.data()->mId; }

    const Data &data() { return *mData.data(); }
    ComObjPtr <USBDeviceFilter> peer() { return mPeer; }

    bool isModified() { AutoLock alock (this); return mData.isBackedUp(); }
    bool isReallyModified() { AutoLock alock (this); return mData.hasActualChanges(); }
    void rollback() { AutoLock alock (this); mData.rollback(); }
    void commit();

    void unshare();

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"USBDeviceFilter"; }

private:

    ComObjPtr <USBController, ComWeakRef> mParent;
    ComObjPtr <USBDeviceFilter> mPeer;
    Backupable <Data> mData;

    /** Used externally to indicate this filter is in the list
        (not touched by the class itslef except that in init()/uninit()) */
    bool mInList;

    friend class USBController;
};

// HostUSBDeviceFilter
////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE HostUSBDeviceFilter :
    public VirtualBoxSupportErrorInfoImpl <HostUSBDeviceFilter, IHostUSBDeviceFilter>,
    public VirtualBoxSupportTranslation <HostUSBDeviceFilter>,
    public VirtualBoxBase,
    public IHostUSBDeviceFilter
{
public:

    struct Data : public USBDeviceFilter::Data
    {
        Data() : mAction (USBDeviceFilterAction_USBDeviceFilterIgnore) {}

        USBDeviceFilterAction_T mAction;
    };

    DECLARE_NOT_AGGREGATABLE(HostUSBDeviceFilter)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HostUSBDeviceFilter)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IUSBDeviceFilter)
        COM_INTERFACE_ENTRY(IHostUSBDeviceFilter)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

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

    // IHostUSBDeviceFilter properties
    STDMETHOD(COMGETTER(Action)) (USBDeviceFilterAction_T *aAction);
    STDMETHOD(COMSETTER(Action)) (USBDeviceFilterAction_T aAction);

    // public methods only for internal purposes

    void *& id() { return mData.data()->mId; }

    const Data &data() { return *mData.data(); }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostUSBDeviceFilter"; }

private:

    ComObjPtr <Host, ComWeakRef> mParent;
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
        // internal collection, no need to implement
        return E_NOTIMPL;
    }

    STDMETHOD(FindByAddress) (INPTR BSTR aAddress, IUSBDevice **aDevice)
    {
        // internal collection, no need to implement
        return E_NOTIMPL;
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END (IfaceUSBDevice, IUSBDevice)

#endif // ____H_USBDEVICEFILTERIMPL
