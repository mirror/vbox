/** @file
 *
 * VirtualBox IHostUSBDevice COM interface implementation
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

#ifndef ____H_HOSTUSBDEVICEIMPL
#define ____H_HOSTUSBDEVICEIMPL

#include "VirtualBoxBase.h"
#include "USBDeviceFilterImpl.h"
/* #include "USBProxyService.h" circular on Host/HostUSBDevice, the includer must include this. */
#include "Collection.h"

#include <VBox/usb.h>

class SessionMachine;
class USBProxyService;

/**
 * Object class used for the Host USBDevices property.
 */
class ATL_NO_VTABLE HostUSBDevice :
    public VirtualBoxSupportErrorInfoImpl <HostUSBDevice, IHostUSBDevice>,
    public VirtualBoxSupportTranslation <HostUSBDevice>,
    public VirtualBoxBase,
    public IHostUSBDevice
{
public:

    HostUSBDevice();
    virtual ~HostUSBDevice();

    DECLARE_NOT_AGGREGATABLE(HostUSBDevice)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HostUSBDevice)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHostUSBDevice)
        COM_INTERFACE_ENTRY(IUSBDevice)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    // public initializer/uninitializer for internal purposes only
    HRESULT init(PUSBDEVICE aUsb, USBProxyService *aUSBProxyService);

    // IUSBDevice properties
    STDMETHOD(COMGETTER(Id))(GUIDPARAMOUT aId);
    STDMETHOD(COMGETTER(VendorId))(USHORT *aVendorId);
    STDMETHOD(COMGETTER(ProductId))(USHORT *aProductId);
    STDMETHOD(COMGETTER(Revision))(USHORT *aRevision);
    STDMETHOD(COMGETTER(Manufacturer))(BSTR *aManufacturer);
    STDMETHOD(COMGETTER(Product))(BSTR *aProduct);
    STDMETHOD(COMGETTER(SerialNumber))(BSTR *aSerialNumber);
    STDMETHOD(COMGETTER(Address))(BSTR *aAddress);
    STDMETHOD(COMGETTER(Port))(USHORT *aPort);
    STDMETHOD(COMGETTER(Remote))(BOOL *aRemote);

    // IHostUSBDevice properties
    STDMETHOD(COMGETTER(State))(USBDeviceState_T *aState);

    // public methods only for internal purposes

    const Guid &id() { return mId; }
    USBDeviceState_T state() { return mState; }
    ComObjPtr <SessionMachine, ComWeakRef> &machine() { return mMachine; }
    bool isIgnored() { return mIgnored; }

    Utf8Str name();

    void setIgnored();
    bool setCaptured (SessionMachine *aMachine);
    int  setHostDriven();
    int  reset();

    void setHostState (USBDeviceState_T aState);

    bool isMatch (const USBDeviceFilter::Data &aData);

    int compare (PCUSBDEVICE pDev2);
    static int compare (PCUSBDEVICE pDev1, PCUSBDEVICE pDev2);

    bool updateState (PCUSBDEVICE aDev);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostUSBDevice"; }

private:

    Guid mId;
    USBDeviceState_T mState;
    ComObjPtr <SessionMachine, ComWeakRef> mMachine;
    bool mIgnored;
    /** Pointer to the USB Proxy Service instance. */
    USBProxyService *mUSBProxyService;

    /** Pointer to the USB Device structure owned by this device.
     * Only used for host devices. */
    PUSBDEVICE m_pUsb;
};


COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN (HostUSBDevice)

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
            return setError (E_INVALIDARG, HostUSBDeviceCollection::tr (
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
            return setError (E_INVALIDARG, HostUSBDeviceCollection::tr (
                "Could not find a USB device with address '%ls'"),
                aAddress);

        return found.queryInterfaceTo (aDevice);
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_END (HostUSBDevice)


#endif // ____H_HOSTUSBDEVICEIMPL
