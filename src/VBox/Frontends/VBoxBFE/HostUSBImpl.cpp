/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Implementation of HostUSB
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

/* r=michael: I have removed almost all functionality from the Main
 *            version of this file, but left the structure as similar
 *            as I could in case we do need some of it some day. */

#include <string>

#ifdef RT_OS_LINUX
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <mntent.h>
/* bird: This is a hack to work around conflicts between these linux kernel headers
 *       and the GLIBC tcpip headers. They have different declarations of the 4
 *       standard byte order functions. */
#define _LINUX_BYTEORDER_GENERIC_H
#include <linux/cdrom.h>
#include <errno.h>
#endif /* RT_OS_LINUX */

#include "HostUSBImpl.h"
#include "HostUSBDeviceImpl.h"
#include "USBProxyService.h"
#include "Logging.h"

#include <VBox/pdm.h>
#include <VBox/vusb.h>
#include <VBox/usb.h>
#include <VBox/err.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <stdio.h>

#include <algorithm>

// destructor
/////////////////////////////////////////////////////////////////////////////

HostUSB::~HostUSB()
{
    if (isReady())
        uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the host USB object.
 *
 * @returns COM result indicator
 * @param parent handle of our parent object
 */
HRESULT HostUSB::init(PVM pVM)
{
    LogFlowMember(("HostUSB::init(): isReady=%d\n", isReady()));

    ComAssertRet (!isReady(), E_UNEXPECTED);

    /* Save pointer to the VM */
    mpVM = pVM;

/*
#ifdef RT_OS_LINUX
    mUSBProxyService = new USBProxyServiceLinux (this);
#elif defined RT_OS_WINDOWS
    mUSBProxyService = new USBProxyServiceWin32 (this);
*/
#ifdef RT_OS_L4
    mUSBProxyService = new USBProxyServiceLinux (this);
#else
    mUSBProxyService = new USBProxyService (this);
#endif
    /** @todo handle !mUSBProxySerivce->isActive() and mUSBProxyService->getLastError()
     * and somehow report or whatever that the proxy failed to startup.
     * Also, there might be init order issues... */

    setReady(true);
    return S_OK;
}

/**
 *  Uninitializes the host object and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void HostUSB::uninit()
{
    LogFlowMember(("HostUSB::uninit(): isReady=%d\n", isReady()));

    AssertReturn (isReady(), (void) 0);

    delete mUSBProxyService;
    mUSBProxyService = NULL;

    mUSBDevices.clear();

    setReady (FALSE);
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Called by USB proxy service when a new device is physically attached
 *  to the host.
 *
 *  @param      aDevice     Pointer to the device which has been attached.
 */
void HostUSB::onUSBDeviceAttached (HostUSBDevice *aDevice)
{
    LogFlowMember (("HostUSB::onUSBDeviceAttached: aDevice=%p\n",
                    aDevice));
    HostUSB::AutoLock alock (this);

    // add to the collecion
    mUSBDevices.push_back (aDevice);

    // apply all filters (no need to lock the device, nobody can access it yet)
    HRESULT rc = AttachUSBDevice(aDevice);
    AssertComRC (rc);
}

/**
 *  Called by USB proxy service (?) when the device is physically detached
 *  from the host.
 *
 *  @param      aDevice     Pointer to the device which has been detached.
 */
void HostUSB::onUSBDeviceDetached (HostUSBDevice *aDevice)
{
    LogFlowMember (("HostUSB::onUSBDeviceDetached: aDevice=%p\n",
                    &aDevice));
    HostUSB::AutoLock alock (this);

    RTUUID id = aDevice->id();

    HostUSBDevice *device = 0;
    HostUSB::USBDeviceList::iterator it = mUSBDevices.begin();
    while (it != mUSBDevices.end())
    {
        if (RTUuidCompare(&(*it)->id(), &id) == 0)
        {
            device = (*it);
            break;
        }
        ++ it;
    }

    AssertReturn (!!device, (void) 0);

    // remove from the collecion
    mUSBDevices.erase (it);

    if (device->isCaptured())
    {
        // the device is captured, release it
        alock.unlock();
        HRESULT rc = DetachUSBDevice (device);
        AssertComRC (rc);
    }
}

/**
 *  Called by USB proxy service  when the state of the host-driven device
 *  has changed because of non proxy interaction.
 *
 *  @param  aDevice     The device in question.
 */
void HostUSB::onUSBDeviceStateChanged (HostUSBDevice *aDevice)
{
    LogFlowMember (("HostUSB::onUSBDeviceStateChanged: \n"));
    HostUSB::AutoLock alock (this);

    /** @todo dmik, is there anything we should do here? For instance if the device now is available? */
}

STDMETHODIMP HostUSB::AttachUSBDevice (HostUSBDevice *hostDevice)
{
    AutoLock alock (this);
    /* This originally checked that the console object was ready.
     * Unfortunately, this method now belongs to HostUSB, and can get
     * called during construction - so before we are "ready". */
//    CHECK_READY();

    /*
     * Don't proceed unless we've found the usb controller.
     */
    if (!mpVM)
        return setError (E_FAIL, tr ("VM is not powered up"));
    PPDMIBASE pBase;
    int vrc = PDMR3QueryLun (mpVM, "usb-ohci", 0, 0, &pBase);
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL, tr ("VM doesn't have a USB controller"));
    /*
     * Make sure that the device is in a captureable state
     */
    USBDeviceState_T eState = hostDevice->state();
    if (eState != USBDeviceState_Busy &&
        eState != USBDeviceState_Available &&
        eState != USBDeviceState_Held)
        return setError (E_FAIL,
                         tr ("Device is not in a capturable state"));
    PVUSBIRHCONFIG pRhConfig = (PVUSBIRHCONFIG)pBase->pfnQueryInterface (pBase, PDMINTERFACE_VUSB_RH_CONFIG);
    AssertReturn (pRhConfig, E_FAIL);

    /*
     * Get the address and the Uuid, and call the pfnCreateProxyDevice roothub method in EMT.
     */
    std::string Address;
    STDMETHODIMP hrc = hostDevice->COMGETTER (Address) (&Address);
    AssertComRC (hrc);

    RTUUID Uuid;
    hrc = hostDevice->COMGETTER (Id) (Uuid);
    AssertComRC (hrc);

    /* No remote devices for now */
    BOOL fRemote = FALSE;
    void *pvRemote = NULL;

    LogFlowMember (("Console::AttachUSBDevice: Proxying USB device '%s' %Vuuid...\n", Address.c_str(), &Uuid));
    PVMREQ pReq;
    vrc = VMR3ReqCall (mpVM, &pReq, RT_INDEFINITE_WAIT,
                       (PFNRT)pRhConfig->pfnCreateProxyDevice,
                       5, pRhConfig, &Uuid, fRemote,
                       Address.c_str(), pvRemote);
    if (VBOX_SUCCESS (vrc))
        vrc = pReq->iStatus;
    VMR3ReqFree (pReq);
    if (VBOX_SUCCESS (vrc))
        hostDevice->setCaptured();
    else
    {
        Log (("Console::AttachUSBDevice: Failed to create proxy device for '%s' %Vuuid, vrc=%Vrc\n", Address.c_str(),
        &Uuid, vrc));
        AssertRC (vrc);
    /* michael: I presume this is not needed. */
/*        hrc = mControl->ReleaseUSBDevice (Uuid);
        AssertComRC (hrc); */
        switch (vrc)
        {
            case VERR_VUSB_NO_PORTS:
                hrc = setError (E_FAIL, tr ("No available ports on the USB controller"));
                break;
            case VERR_VUSB_USBFS_PERMISSION:
                hrc = setError (E_FAIL, tr ("Not permitted to open the USB device, check usbfs options"));
                break;
            default:
                hrc = setError (E_FAIL, tr ("Failed to create USB proxy device: %Vrc"), vrc);
                break;
        }
        return hrc;
    }

    return S_OK;
}

STDMETHODIMP HostUSB::DetachUSBDevice (HostUSBDevice *aDevice)
{
    AutoLock alock (this);
    /* This originally checked that the console object was ready.
     * Unfortunately, this method now belongs to HostUSB, and can get
     * called during construction - so before we are "ready". */
//    CHECK_READY();

    /*
     * Detach the device from the VM
     */
    int vrc = VERR_PDM_DEVICE_NOT_FOUND;
    if (mpVM)
    {
        PPDMIBASE pBase;
        vrc = PDMR3QueryLun (mpVM, "usb-ohci", 0, 0, &pBase);
        if (VBOX_SUCCESS (vrc))
        {
            PVUSBIRHCONFIG pRhConfig = (PVUSBIRHCONFIG)pBase->pfnQueryInterface (pBase, PDMINTERFACE_VUSB_RH_CONFIG);
            Assert (pRhConfig);

            RTUUID Uuid = aDevice->id();
            LogFlowMember (("Console::DetachUSBDevice: Detaching USB proxy device %Vuuid...\n", &Uuid));
            PVMREQ pReq;
            vrc = VMR3ReqCall (mpVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)pRhConfig->pfnDestroyProxyDevice,
                               2, pRhConfig, &Uuid);
            if (VBOX_SUCCESS (vrc))
                vrc = pReq->iStatus;
            VMR3ReqFree (pReq);
        }
    }
    if (    vrc == VERR_PDM_DEVICE_NOT_FOUND
        ||  vrc == VERR_PDM_DEVICE_INSTANCE_NOT_FOUND)
    {
        Log (("Console::DetachUSBDevice: USB isn't enabled.\n"));
        vrc = VINF_SUCCESS;
    }
    if (VBOX_SUCCESS (vrc))
        return S_OK;
    Log (("Console::AttachUSBDevice: Failed to detach the device from the USB controller, vrc=%Vrc.\n", vrc));
    return(setError (E_UNEXPECTED, tr ("Failed to destroy the USB proxy device: %Vrc"), vrc));
}
