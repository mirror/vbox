/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of HostUSB
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

#ifndef ____H_HOSTUSBIMPL
#define ____H_HOSTUSBIMPL

#ifndef VBOXBFE_WITH_USB
# error "misconfiguration VBOXBFE_WITH_USB isn't defined and HostUSBImpl.h was included."
#endif

#include "VirtualBoxBase.h"
#include "HostUSBDeviceImpl.h"

/* We do not support loadable configurations here */
// #include <VBox/cfgldr.h>

#include <list>

class HostUSB :
    public VirtualBoxBase
{
public:
    ~HostUSB();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (PVM pVM);
    void uninit();

    // public methods only for internal purposes

    void onUSBDeviceAttached (HostUSBDevice *aDevice);
    void onUSBDeviceDetached (HostUSBDevice *aDevice);
    void onUSBDeviceStateChanged (HostUSBDevice *aDevice);

    USBProxyService *usbProxyService() { return mUSBProxyService; }

private:

    typedef std::list <HostUSBDevice *> USBDeviceList;
    USBDeviceList mUSBDevices;

    /** Pointer to the VM */
    PVM mpVM;

    /** Pointer to the USBProxyService object. */
    USBProxyService *mUSBProxyService;

    STDMETHODIMP AttachUSBDevice (HostUSBDevice *hostDevice);
    STDMETHODIMP DetachUSBDevice (HostUSBDevice *aDevice);
};

#endif // ____H_HOSTUSBIMPL
