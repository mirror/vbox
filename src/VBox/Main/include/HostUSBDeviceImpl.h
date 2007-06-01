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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ____H_HOSTUSBDEVICEIMPL
#define ____H_HOSTUSBDEVICEIMPL

#include "VirtualBoxBase.h"
#include "USBDeviceFilterImpl.h"
/* #include "USBProxyService.h" circular on Host/HostUSBDevice, the includer
 * must include this. */
#include "Collection.h"

#include <VBox/usb.h>

class SessionMachine;
class USBProxyService;

/**
 * Object class used to hold Host USB Device properties.
 */
class ATL_NO_VTABLE HostUSBDevice :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <HostUSBDevice, IHostUSBDevice>,
    public VirtualBoxSupportTranslation <HostUSBDevice>,
    public IHostUSBDevice
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (HostUSBDevice)

    DECLARE_NOT_AGGREGATABLE(HostUSBDevice)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HostUSBDevice)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHostUSBDevice)
        COM_INTERFACE_ENTRY(IUSBDevice)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (HostUSBDevice)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(PUSBDEVICE aUsb, USBProxyService *aUSBProxyService);
    void uninit();

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

    /* @note Must be called from under the object read lock. */
    const Guid &id() const { return mId; }

    /* @note Must be called from under the object read lock. */
    USBDeviceState_T state() const { return mState; }

    /** Same as state() except you don't need to lock any thing. */
    USBDeviceState_T stateUnlocked() const
    {
        AutoReaderLock (this);
        return state();
    }

    /* @note Must be called from under the object read lock. */
    USBDeviceState_T pendingState() const { return mPendingState; }

    /** Same as pendingState() except you don't need to lock any thing. */
    USBDeviceState_T pendingStateUnlocked() const
    {
        AutoReaderLock (this);
        return pendingState();
    }

    /* @note Must be called from under the object read lock. */
    ComObjPtr <SessionMachine, ComWeakRef> &machine() { return mMachine; }

/// @todo remove
#if 0
    /* @note Must be called from under the object read lock. */
    bool isIgnored() { return mIgnored; }
#endif

    /* @note Must be called from under the object read lock. */
    bool isStatePending() const { return mIsStatePending; }

    /** Same as isStatePending() except you don't need to lock any thing. */
    bool isStatePendingUnlocked() const
    {
        AutoReaderLock (this);
        return isStatePending();
    }

    /* @note Must be called from under the object read lock. */
    PCUSBDEVICE usbData() const { return mUsb; }

    Utf8Str name();

/// @todo remove
#if 0
    void setIgnored();
#endif

    bool setCaptured (SessionMachine *aMachine);
    int  setHostDriven();
    int  reset();

/// @todo remove
#if 0
    void setHostState (USBDeviceState_T aState);
#endif

    bool isMatch (const USBDeviceFilter::Data &aData);

    int compare (PCUSBDEVICE pDev2);
    static int compare (PCUSBDEVICE pDev1, PCUSBDEVICE pDev2);

    bool updateState (PCUSBDEVICE aDev);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostUSBDevice"; }

private:

    const Guid mId;
    USBDeviceState_T mState;
    USBDeviceState_T mPendingState;
    ComObjPtr <SessionMachine, ComWeakRef> mMachine;
    bool mIsStatePending : 1;
/// @todo remove
#if 0
    bool mIgnored : 1;
#endif

    /** Pointer to the USB Proxy Service instance. */
    USBProxyService *mUSBProxyService;

    /** Pointer to the USB Device structure owned by this device.
     * Only used for host devices. */
    PUSBDEVICE mUsb;
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
