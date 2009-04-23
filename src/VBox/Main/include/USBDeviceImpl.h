/* $Id$ */

/** @file
 * Header file for the OUSBDevice (IUSBDevice) class, VBoxC.
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

#ifndef ____H_USBDEVICEIMPL
#define ____H_USBDEVICEIMPL

#include "VirtualBoxBase.h"
#include "Logging.h"


/**
 * Object class used for maintaining devices attached to a USB controller.
 * Generally this contains much less information.
 */
class ATL_NO_VTABLE OUSBDevice :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl<OUSBDevice, IUSBDevice>,
    public VirtualBoxSupportTranslation<OUSBDevice>,
    VBOX_SCRIPTABLE_IMPL(IUSBDevice)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (OUSBDevice)

    DECLARE_NOT_AGGREGATABLE(OUSBDevice)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(OUSBDevice)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IUSBDevice)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (OUSBDevice)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (IUSBDevice *a_pUSBDevice);
    void uninit();

    // IUSBDevice properties
    STDMETHOD(COMGETTER(Id))(OUT_GUID aId);
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

    // public methods only for internal purposes
    const Guid &id() const { return mData.id; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"USBDevice"; }

private:

    struct Data
    {
        Data() : vendorId (0), productId (0), revision (0), port (0),
                 version (1), portVersion (1), remote (FALSE) {}

        /** The UUID of this device. */
        const Guid id;

        /** The vendor id of this USB device. */
        const USHORT vendorId;
        /** The product id of this USB device. */
        const USHORT productId;
        /** The product revision number of this USB device.
         * (high byte = integer; low byte = decimal) */
        const USHORT revision;
        /** The Manufacturer string. (Quite possibly NULL.) */
        const Bstr manufacturer;
        /** The Product string. (Quite possibly NULL.) */
        const Bstr product;
        /** The SerialNumber string. (Quite possibly NULL.) */
        const Bstr serialNumber;
        /** The host specific address of the device. */
        const Bstr address;
        /** The host port number. */
        const USHORT port;
        /** The major USB version number of the device. */
        const USHORT version;
        /** The major USB version number of the port the device is attached to. */
        const USHORT portVersion;
        /** Remote (VRDP) or local device. */
        const BOOL remote;
    };

    Data mData;
};

#endif // ____H_USBDEVICEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
