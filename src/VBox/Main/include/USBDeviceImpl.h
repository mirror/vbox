/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include "Collection.h"
#include "Logging.h"


/**
 * Object class used for maintaining devices attached to a USB controller.
 * Generally this contains much less information.
 */
class ATL_NO_VTABLE OUSBDevice :
    public VirtualBoxSupportErrorInfoImpl<OUSBDevice, IUSBDevice>,
    public VirtualBoxSupportTranslation<OUSBDevice>,
    public VirtualBoxBase,
    public IUSBDevice
{
public:

    OUSBDevice();
    virtual ~OUSBDevice();

    DECLARE_NOT_AGGREGATABLE(OUSBDevice)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(OUSBDevice)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IUSBDevice)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    // public initializer/uninitializer for internal purposes only
    HRESULT init(IUSBDevice *a_pUSBDevice);

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

    // public methods only for internal purposes
    const Guid &id() { return mId; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"USBDevice"; }

private:
    /** The UUID of this device. */
    Guid mId;

    /** The vendor id of this USB device. */
    USHORT mVendorId;
    /** The product id of this USB device. */
    USHORT mProductId;
    /** The product revision number of this USB device.
     * (high byte = integer; low byte = decimal) */
    USHORT mRevision;
    /** The Manufacturer string. (Quite possibly NULL.) */
    Bstr mManufacturer;
    /** The Product string. (Quite possibly NULL.) */
    Bstr mProduct;
    /** The SerialNumber string. (Quite possibly NULL.) */
    Bstr mSerialNumber;
    /** The host specific address of the device. */
    Bstr mAddress;
    /** The host port number. */
    USHORT mPort;
    /** The major USB version number of the device. */
    USHORT mVersion;
    /** The major USB version number of the port the device is attached to. */
    USHORT mPortVersion;
    /** Remote (VRDP) or local device. */
    BOOL mRemote;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN (ComObjPtr <OUSBDevice>, IUSBDevice, OUSBDevice)

    STDMETHOD(FindById) (INPTR GUIDPARAM aId, IUSBDevice **aDevice)
    {
        Guid idToFind = aId;
        if (idToFind.isEmpty())
            return E_INVALIDARG;
        if (!aDevice)
            return E_POINTER;
RTLogPrintf("%Rfn: id=%RTuuid\n", __PRETTY_FUNCTION__, idToFind.raw());

        *aDevice = NULL;
        Vector::value_type found;
        Vector::iterator it = vec.begin();
        while (!found && it != vec.end())
        {
            Guid id;
            (*it)->COMGETTER(Id) (id.asOutParam());
RTLogPrintf("%Rfn: it=%RTuuid\n", __PRETTY_FUNCTION__, id.raw());
            if (id == idToFind)
                found = *it;
            ++ it;
        }

        if (!found)
        {
RTLogPrintf("%Rfn: not found\n", __PRETTY_FUNCTION__);
            return setError (E_INVALIDARG, OUSBDeviceCollection::tr (
                "Could not find a USB device with UUID {%s}"),
                idToFind.toString().raw());
        }

        return found.queryInterfaceTo (aDevice);
    }

    STDMETHOD(FindByAddress) (INPTR BSTR aAddress, IUSBDevice **aDevice)
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
            return setError (E_INVALIDARG, OUSBDeviceCollection::tr (
                "Could not find a USB device with address '%ls'"),
                aAddress);

        return found.queryInterfaceTo (aDevice);
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_END (ComObjPtr <OUSBDevice>, IUSBDevice, OUSBDevice)

#endif // ____H_USBDEVICEIMPL
