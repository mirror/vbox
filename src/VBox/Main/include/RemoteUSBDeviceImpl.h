/** @file
 *
 * VirtualBox IHostUSBDevice COM interface implementation
 * for remote (VRDP) USB devices
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

#ifndef ____H_REMOTEUSBDEVICEIMPL
#define ____H_REMOTEUSBDEVICEIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"
#include <VBox/vrdpapi.h>

class ATL_NO_VTABLE RemoteUSBDevice :
    public VirtualBoxSupportErrorInfoImpl <RemoteUSBDevice, IHostUSBDevice>,
    public VirtualBoxSupportTranslation <RemoteUSBDevice>,
    public VirtualBoxBase,
    public IHostUSBDevice
{
public:

    DECLARE_NOT_AGGREGATABLE(RemoteUSBDevice)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(RemoteUSBDevice)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHostUSBDevice)
        COM_INTERFACE_ENTRY(IUSBDevice)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (RemoteUSBDevice)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
#ifdef VRDP_MC
    HRESULT init(uint32_t u32ClientId, VRDPUSBDEVICEDESC *pDevDesc);
#else
    HRESULT init(VRDPUSBDEVICEDESC *pDevDesc);
#endif /* VRDP_MC */
    void uninit();

    // IUSBDevice properties
    STDMETHOD(COMGETTER(Id)) (GUIDPARAMOUT aId);
    STDMETHOD(COMGETTER(VendorId)) (USHORT *aVendorId);
    STDMETHOD(COMGETTER(ProductId)) (USHORT *aProductId);
    STDMETHOD(COMGETTER(Revision)) (USHORT *aRevision);
    STDMETHOD(COMGETTER(Manufacturer)) (BSTR *aManufacturer);
    STDMETHOD(COMGETTER(Product)) (BSTR *aProduct);
    STDMETHOD(COMGETTER(SerialNumber)) (BSTR *aSerialNumber);
    STDMETHOD(COMGETTER(Address)) (BSTR *aAddress);
    STDMETHOD(COMGETTER(Port)) (USHORT *aPort);
    STDMETHOD(COMGETTER(Remote)) (BOOL *aRemote);

    // IHostUSBDevice properties
    STDMETHOD(COMGETTER(State)) (USBDeviceState_T *aState);

    // public methods only for internal purposes
    bool dirty (void) { return mDirty; }
    void dirty (bool aDirty) { mDirty = aDirty; }

    uint16_t devId (void) { return mDevId; }
#ifdef VRDP_MC
    uint32_t clientId (void) { return mClientId; }
#endif /* VRDP_MC */

    bool captured (void) { return mState == USBDeviceState_USBDeviceCaptured; }
    void captured (bool aCaptured)
    {
        if (aCaptured)
        {
            Assert(mState == USBDeviceState_USBDeviceAvailable);
            mState = USBDeviceState_USBDeviceCaptured;
        }
        else
        {
            Assert(mState == USBDeviceState_USBDeviceCaptured);
            mState = USBDeviceState_USBDeviceAvailable;
        }
    }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"RemoteUSBDevice"; }

private:

    Guid mId;

    uint16_t mVendorId;
    uint16_t mProductId;
    uint16_t mRevision;

    Bstr mManufacturer;
    Bstr mProduct;
    Bstr mSerialNumber;

    Bstr mAddress;

    uint16_t mPort;

    USBDeviceState_T mState;

    bool mDirty;
    uint16_t mDevId;
#ifdef VRDP_MC
    uint32_t mClientId;
#endif /* VRDP_MC */
};


/// @todo (dmik) give a less stupid name and move to Collection.h
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_FOR_BEGIN(c, iface) \
    class c##Enumerator \
        : public IfaceVectorEnumerator \
            <iface##Enumerator, iface, ComObjPtr <c>, c##Enumerator> \
        , public VirtualBoxSupportTranslation <c##Enumerator> \
    { \
        NS_DECL_ISUPPORTS \
        public: static const wchar_t *getComponentName() { \
            return WSTR_LITERAL (c) L"Enumerator"; \
        } \
    }; \
    class c##Collection \
        : public ReadonlyIfaceVector \
            <iface##Collection, iface, iface##Enumerator, ComObjPtr <c>, c##Enumerator, \
         c##Collection> \
        , public VirtualBoxSupportTranslation <c##Collection> \
    { \
        NS_DECL_ISUPPORTS \
        public: static const wchar_t *getComponentName() { \
            return WSTR_LITERAL (c) L"Collection"; \
        }

#define COM_DECL_READONLY_ENUM_AND_COLLECTION_FOR_END(c, iface) \
    };

#ifdef __WIN__

#define COM_IMPL_READONLY_ENUM_AND_COLLECTION_FOR(c, iface)

#else // !__WIN__

#define COM_IMPL_READONLY_ENUM_AND_COLLECTION_FOR(c, iface) \
    NS_DECL_CLASSINFO(c##Collection) \
    NS_IMPL_THREADSAFE_ISUPPORTS1_CI(c##Collection, iface##Collection) \
    NS_DECL_CLASSINFO(c##Enumerator) \
    NS_IMPL_THREADSAFE_ISUPPORTS1_CI(c##Enumerator, iface##Enumerator)

#endif

COM_DECL_READONLY_ENUM_AND_COLLECTION_FOR_BEGIN (RemoteUSBDevice, IHostUSBDevice)

    STDMETHOD(FindById) (INPTR GUIDPARAM aId, IHostUSBDevice **aDevice)
    {
        Guid idToFind = aId;
        if (idToFind.isEmpty())
            return E_INVALIDARG;
        if (!aDevice)
            return E_POINTER;

        *aDevice = NULL;
        Vector::value_type found;
        Vector::iterator it = vec.begin();
        while (!found && it != vec.end())
        {
            Guid id;
            (*it)->COMGETTER(Id) (id.asOutParam());
            if (id == idToFind)
                found = *it;
            ++ it;
        }

        if (!found)
            return setError (E_INVALIDARG, RemoteUSBDeviceCollection::tr (
                "Could not find a USB device with UUID {%s}"),
                idToFind.toString().raw());

        return found.queryInterfaceTo (aDevice);
    }

    STDMETHOD(FindByAddress) (INPTR BSTR aAddress, IHostUSBDevice **aDevice)
    {
        if (!aAddress)
            return E_INVALIDARG;
        if (!aDevice)
            return E_POINTER;

        *aDevice = NULL;
        Vector::value_type found;
        Vector::iterator it = vec.begin();
        while (!found && it != vec.end())
        {
            Bstr address;
            (*it)->COMGETTER(Address) (address.asOutParam());
            if (address == aAddress)
                found = *it;
            ++ it;
        }

        if (!found)
            return setError (E_INVALIDARG, RemoteUSBDeviceCollection::tr (
                "Could not find a USB device with address '%ls'"),
                aAddress);

        return found.queryInterfaceTo (aDevice);
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_FOR_END (RemoteUSBDevice, IHostUSBDevice)


#endif // ____H_REMOTEUSBDEVICEIMPL
