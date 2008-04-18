/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of HostUSBDevice
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

#ifndef ____H_HOSTUSBDEVICEIMPL
#define ____H_HOSTUSBDEVICEIMPL

#ifndef VBOXBFE_WITH_USB
# error "misconfiguration VBOXBFE_WITH_USB isn't defined and HostUSBDeviceImpl.h was included."
#endif
#include <string>

#include "VirtualBoxBase.h"
// #include "USBDeviceFilterImpl.h"
/* #include "USBProxyService.h" circular on Host/HostUSBDevice, the includer must include this. */
// #include "Collection.h"

#include <VBox/usb.h>
#include <iprt/uuid.h>

class USBProxyService;

/**
 * The state of a given USB device in the host and in the guest.
 * Originally part of the COM interface.
 */
typedef enum {
    /** Not supported by the VirtualBox server, not available to
        guests. */
    USBDeviceState_NotSupported,
    /** Being used by the host computer exclusively, not available
        to guests. */
    USBDeviceState_Unavailable,
    /** Being used by the host computer, potentially available to
       guests. */
    USBDeviceState_Busy,
    /** Not used by the host computer, available to guests. The
        host computer can also start using the device at any time. */
    USBDeviceState_Available,
    /** Held by the VirtualBox server (ignored by the host computer),
        available to guests. */
    USBDeviceState_Held,
    /** Captured by one of the guest computers, not available to
        anybody else. */
    USBDeviceState_Captured
} USBDeviceState_T;

/**
 * Object class used for the Host USBDevices property.
 */
class HostUSBDevice : public VirtualBoxBase
{
public:

    HostUSBDevice();
    virtual ~HostUSBDevice();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(PUSBDEVICE aUsb, USBProxyService *aUSBProxyService);

    // IUSBDevice properties
    STDMETHOD(COMGETTER(Id))(RTUUID &aId);
    STDMETHOD(COMGETTER(VendorId))(USHORT *aVendorId);
    STDMETHOD(COMGETTER(ProductId))(USHORT *aProductId);
    STDMETHOD(COMGETTER(Revision))(USHORT *aRevision);
    STDMETHOD(COMGETTER(Manufacturer))(std::string *aManufacturer);
    STDMETHOD(COMGETTER(Product))(std::string *aProduct);
    STDMETHOD(COMGETTER(SerialNumber))(std::string *aSerialNumber);
    STDMETHOD(COMGETTER(Address))(std::string *aAddress);
    STDMETHOD(COMGETTER(Port))(USHORT *aPort);
    STDMETHOD(COMGETTER(Remote))(BOOL *aRemote);

    // IHostUSBDevice properties
    STDMETHOD(COMGETTER(State))(USBDeviceState_T *aState);

    // public methods only for internal purposes

    const RTUUID &id() { return mId; }
    USBDeviceState_T state() { return mState; }
    bool isIgnored() { return mIgnored; }

    void setIgnored();
    void setCaptured ();
    bool isCaptured()
        { return mState == USBDeviceState_Captured; }
    int  setHostDriven();
    int  reset();

    void setHostState (USBDeviceState_T aState);

    int compare (PCUSBDEVICE pDev2);
    static int compare (PCUSBDEVICE pDev1, PCUSBDEVICE pDev2);

    bool updateState (PCUSBDEVICE aDev);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostUSBDevice"; }

private:

    RTUUID mId;
    USBDeviceState_T mState;
    bool mIgnored;
    /** Pointer to the USB Proxy Service instance. */
    USBProxyService *mUSBProxyService;

    /** Pointer to the USB Device structure owned by this device.
     * Only used for host devices. */
    PUSBDEVICE m_pUsb;
};

#endif // ____H_HOSTUSBDEVICEIMPL
