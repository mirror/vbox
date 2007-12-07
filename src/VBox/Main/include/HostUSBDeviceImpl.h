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
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
    STDMETHOD(COMGETTER(Version))(USHORT *aVersion);
    STDMETHOD(COMGETTER(PortVersion))(USHORT *aPortVersion);
    STDMETHOD(COMGETTER(Remote))(BOOL *aRemote);

    // IHostUSBDevice properties
    STDMETHOD(COMGETTER(State))(USBDeviceState_T *aState);

    /** Additional internal states.
     * The async detach stuff for Darwin is a two stage journey with a variation
     * (filters) depending on who won the race to lock the Host object.
     *
     * @remark Trying out mac os x style enum naming convention here. nice or what?
     */
    typedef enum
    {
        /** Nothing is pending here, check mPendingState. */
        kNothingPending,
        /** 1st stage of the detch, waiting for the logical detach notification. */
        kDetachingPendingDetach,
        /** 1st stage of the detch, waiting for the logical detach notification - re-run filters.
         * Prev: kDetachingPendingDetach */
        kDetachingPendingDetachFilters,
        /** 2nd stage of the detach, waiting for the logical attach notification.
         * Prev: kDetachingPendingDetach */
        kDetachingPendingAttach,
        /** 2nd stage of the detach, waiting for the logical attach notification - re-run filters.
         * Prev: kDetachingPendingDetachFilters */
        kDetachingPendingAttachFilters
    } InternalState;

    // public methods only for internal purposes

    /** @note Must be called from under the object read lock. */
    const Guid &id() const { return mId; }

    /** @note Must be called from under the object read lock. */
    USBDeviceState_T state() const { return mState; }

    /** @note Must be called from under the object read lock. */
    USBDeviceState_T pendingState() const { return mPendingState; }

    /** @note Must be called from under the object read lock. */
    InternalState pendingStateEx() const { return mPendingStateEx; }

    /** @note Must be called from under the object read lock. */
    ComObjPtr <SessionMachine> &machine() { return mMachine; }

    /** @note Must be called from under the object read lock. */
    bool isStatePending() const { return mIsStatePending; }

    /** @note Must be called from under the object read lock. */
    PCUSBDEVICE usbData() const { return mUsb; }

    Utf8Str name();

    bool requestCapture (SessionMachine *aMachine, ULONG aMaskedIfs = 0);
    void requestRelease();
    void requestHold();

    void setHeld();
    void onDetachedPhys();

    void handlePendingStateChange();
    void cancelPendingState(bool aTimeout = false);

    bool isMatch (const USBDeviceFilter::Data &aData);

    int compare (PCUSBDEVICE aDev2);
    static int compare (PCUSBDEVICE aDev1, PCUSBDEVICE aDev2,
                        bool aIsStrict = true);

    bool updateState (PCUSBDEVICE aDev);

    void checkForAsyncTimeout();

    bool setLogicalReconnect (InternalState aStage);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostUSBDevice"; }

private:

    const Guid mId;
    USBDeviceState_T mState;
    USBDeviceState_T mPendingState;
    /** Same as mPendingState but for the internal states. */
    InternalState mPendingStateEx;
    /** RTTimeNanoTS() of when mIsStatePending was set or mDetaching changed
     * from kNotDetaching. For operations that cannot be cancelled it's 0. */
    uint64_t mPendingSince;
    ComObjPtr <SessionMachine> mMachine;
    bool mIsStatePending : 1;

    /** Pointer to the USB Proxy Service instance. */
    USBProxyService *mUSBProxyService;

    /** Pointer to the USB Device structure owned by this device.
     * Only used for host devices. */
    PUSBDEVICE mUsb;

    /** The interface mask to be use in the pending capture. */
    ULONG mMaskedIfs;

    friend class USBProxyService;
#ifdef RT_OS_LINUX
    friend class USBProxyServiceLinux;
#endif
#ifdef RT_OS_DARWIN
    /** One-shot filter id. */
    void *mOneShotId;

    friend class USBProxyServiceDarwin;
#endif
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
