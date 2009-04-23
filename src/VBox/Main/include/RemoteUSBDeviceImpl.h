/* $Id$ */

/** @file
 *
 * VirtualBox IHostUSBDevice COM interface implementation
 * for remote (VRDP) USB devices
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ____H_REMOTEUSBDEVICEIMPL
#define ____H_REMOTEUSBDEVICEIMPL

#include "VirtualBoxBase.h"

struct _VRDPUSBDEVICEDESC;
typedef _VRDPUSBDEVICEDESC VRDPUSBDEVICEDESC;

class ATL_NO_VTABLE RemoteUSBDevice :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <RemoteUSBDevice, IHostUSBDevice>,
    public VirtualBoxSupportTranslation <RemoteUSBDevice>,
    VBOX_SCRIPTABLE_IMPL(IHostUSBDevice)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (OUSBDevice)

    DECLARE_NOT_AGGREGATABLE (RemoteUSBDevice)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (RemoteUSBDevice)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IHostUSBDevice)
        COM_INTERFACE_ENTRY (IUSBDevice)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (RemoteUSBDevice)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(uint32_t u32ClientId, VRDPUSBDEVICEDESC *pDevDesc);
    void uninit();

    // IUSBDevice properties
    STDMETHOD(COMGETTER(Id)) (OUT_GUID aId);
    STDMETHOD(COMGETTER(VendorId)) (USHORT *aVendorId);
    STDMETHOD(COMGETTER(ProductId)) (USHORT *aProductId);
    STDMETHOD(COMGETTER(Revision)) (USHORT *aRevision);
    STDMETHOD(COMGETTER(Manufacturer)) (BSTR *aManufacturer);
    STDMETHOD(COMGETTER(Product)) (BSTR *aProduct);
    STDMETHOD(COMGETTER(SerialNumber)) (BSTR *aSerialNumber);
    STDMETHOD(COMGETTER(Address)) (BSTR *aAddress);
    STDMETHOD(COMGETTER(Port)) (USHORT *aPort);
    STDMETHOD(COMGETTER(Version)) (USHORT *aVersion);
    STDMETHOD(COMGETTER(PortVersion)) (USHORT *aPortVersion);
    STDMETHOD(COMGETTER(Remote)) (BOOL *aRemote);

    // IHostUSBDevice properties
    STDMETHOD(COMGETTER(State)) (USBDeviceState_T *aState);

    // public methods only for internal purposes
    bool dirty (void) const { return mData.dirty; }
    void dirty (bool aDirty) { mData.dirty = aDirty; }

    uint16_t devId (void) const { return mData.devId; }
    uint32_t clientId (void) { return mData.clientId; }

    bool captured (void) const { return mData.state == USBDeviceState_Captured; }
    void captured (bool aCaptured)
    {
        if (aCaptured)
        {
            Assert(mData.state == USBDeviceState_Available);
            mData.state = USBDeviceState_Captured;
        }
        else
        {
            Assert(mData.state == USBDeviceState_Captured);
            mData.state = USBDeviceState_Available;
        }
    }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"RemoteUSBDevice"; }

private:

    struct Data
    {
        Data() : vendorId (0), productId (0), revision (0), port (0), version (1),
                 portVersion (1), dirty (FALSE), devId (0), clientId (0) {}

        const Guid id;

        const uint16_t vendorId;
        const uint16_t productId;
        const uint16_t revision;

        const Bstr manufacturer;
        const Bstr product;
        const Bstr serialNumber;

        const Bstr address;

        const uint16_t port;
        const uint16_t version;
        const uint16_t portVersion;

        USBDeviceState_T state;
        bool dirty;

        const uint16_t devId;
        const uint32_t clientId;
    };

    Data mData;
};

#endif // ____H_REMOTEUSBDEVICEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
