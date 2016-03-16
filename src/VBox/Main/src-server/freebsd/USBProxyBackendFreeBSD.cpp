/* $Id$ */
/** @file
 * VirtualBox USB Proxy Service, FreeBSD Specialization.
 */

/*
 * Copyright (C) 2005-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "USBProxyBackend.h"
#include "Logging.h"

#include <VBox/usb.h>
#include <VBox/usblib.h>
#include <VBox/err.h>

#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

/**
 * Initialize data members.
 */
USBProxyBackendFreeBSD::USBProxyBackendFreeBSD(USBProxyService *aUsbProxyService)
    : USBProxyBackend(aHost)
{
    LogFlowThisFunc(("aUsbProxyService=%p\n", aUsbProxyService));
}


/**
 * Initializes the object (called right after construction).
 *
 * @returns S_OK on success and non-fatal failures, some COM error otherwise.
 */
int USBProxyBackendFreeBSD::init(void)
{
    /*
     * Create semaphore.
     */
    int rc = RTSemEventCreate(&mNotifyEventSem);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Start the poller thread.
     */
    start();
    return VINF_SUCCESS;
}


/**
 * Stop all service threads and free the device chain.
 */
USBProxyBackendFreeBSD::~USBProxyBackendFreeBSD()
{
    LogFlowThisFunc(("\n"));

    /*
     * Stop the service.
     */
    if (isActive())
        stop();

    RTSemEventDestroy(mNotifyEventSem);
    mNotifyEventSem = NULL;
}


int USBProxyBackendFreeBSD::captureDevice(HostUSBDevice *aDevice)
{
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    AutoReadLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFunc(("aDevice=%s\n", aDevice->i_getName().c_str()));

    /*
     * Don't think we need to do anything when the device is held... fake it.
     */
    Assert(aDevice->i_getUnistate() == kHostUSBDeviceState_Capturing);
    devLock.release();
    interruptWait();

    return VINF_SUCCESS;
}


int USBProxyBackendFreeBSD::releaseDevice(HostUSBDevice *aDevice)
{
    AssertReturn(aDevice, VERR_GENERAL_FAILURE);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), VERR_GENERAL_FAILURE);

    AutoReadLock devLock(aDevice COMMA_LOCKVAL_SRC_POS);
    LogFlowThisFunc(("aDevice=%s\n", aDevice->i_getName().c_str()));

    /*
     * We're not really holding it atm., just fake it.
     */
    Assert(aDevice->i_getUnistate() == kHostUSBDeviceState_ReleasingToHost);
    devLock.release();
    interruptWait();

    return VINF_SUCCESS;
}


bool USBProxyBackendFreeBSD::updateDeviceState(HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice, bool *aRunFilters,
                                               SessionMachine **aIgnoreMachine)
{
    AssertReturn(aDevice, false);
    AssertReturn(!aDevice->isWriteLockOnCurrentThread(), false);

    return updateDeviceStateFake(aDevice, aUSBDevice, aRunFilters, aIgnoreMachine);
}


/**
 * A device was added
 *
 * See USBProxyService::deviceAdded for details.
 */
void USBProxyBackendFreeBSD::deviceAdded(ComObjPtr<HostUSBDevice> &aDevice, SessionMachinesList &llOpenedMachines,
                                         PUSBDEVICE aUSBDevice)
{
    USBProxyBackend::deviceAdded(aDevice, llOpenedMachines, aUSBDevice);
}

int USBProxyBackendFreeBSD::wait(RTMSINTERVAL aMillies)
{
    return RTSemEventWait(mNotifyEventSem, aMillies < 1000 ? 1000 : 5000);
}


int USBProxyBackendFreeBSD::interruptWait(void)
{
    return RTSemEventSignal(mNotifyEventSem);
}


/**
 * Dumps a USBDEVICE structure to the log using LogLevel 3.
 * @param   pDev        The structure to log.
 * @todo    This is really common code.
 */
DECLINLINE(void) usbLogDevice(PUSBDEVICE pDev)
{
    NOREF(pDev);

    Log3(("USB device:\n"));
    Log3(("Product: %s (%x)\n", pDev->pszProduct, pDev->idProduct));
    Log3(("Manufacturer: %s (Vendor ID %x)\n", pDev->pszManufacturer, pDev->idVendor));
    Log3(("Serial number: %s (%llx)\n", pDev->pszSerialNumber, pDev->u64SerialHash));
    Log3(("Device revision: %d\n", pDev->bcdDevice));
    Log3(("Device class: %x\n", pDev->bDeviceClass));
    Log3(("Device subclass: %x\n", pDev->bDeviceSubClass));
    Log3(("Device protocol: %x\n", pDev->bDeviceProtocol));
    Log3(("USB version number: %d\n", pDev->bcdUSB));
    Log3(("Device speed: %s\n",
            pDev->enmSpeed == USBDEVICESPEED_UNKNOWN  ? "unknown"
          : pDev->enmSpeed == USBDEVICESPEED_LOW      ? "1.5 MBit/s"
          : pDev->enmSpeed == USBDEVICESPEED_FULL     ? "12 MBit/s"
          : pDev->enmSpeed == USBDEVICESPEED_HIGH     ? "480 MBit/s"
          : pDev->enmSpeed == USBDEVICESPEED_VARIABLE ? "variable"
          :                                             "invalid"));
    Log3(("Number of configurations: %d\n", pDev->bNumConfigurations));
    Log3(("Bus number: %d\n", pDev->bBus));
    Log3(("Port number: %d\n", pDev->bPort));
    Log3(("Device number: %d\n", pDev->bDevNum));
    Log3(("Device state: %s\n",
            pDev->enmState == USBDEVICESTATE_UNSUPPORTED   ? "unsupported"
          : pDev->enmState == USBDEVICESTATE_USED_BY_HOST  ? "in use by host"
          : pDev->enmState == USBDEVICESTATE_USED_BY_HOST_CAPTURABLE ? "in use by host, possibly capturable"
          : pDev->enmState == USBDEVICESTATE_UNUSED        ? "not in use"
          : pDev->enmState == USBDEVICESTATE_HELD_BY_PROXY ? "held by proxy"
          : pDev->enmState == USBDEVICESTATE_USED_BY_GUEST ? "used by guest"
          :                                                  "invalid"));
    Log3(("OS device address: %s\n", pDev->pszAddress));
}

PUSBDEVICE USBProxyBackendFreeBSD::getDevices(void)
{
    PUSBDEVICE pDevices = NULL;
    int FileUsb = 0;
    int iBus  = 0;
    int iAddr = 1;
    int rc = VINF_SUCCESS;
    char *pszDevicePath = NULL;
    uint32_t PlugTime = 0;

    for (;;)
    {
        rc = RTStrAPrintf(&pszDevicePath, "/dev/%s%d.%d", USB_GENERIC_NAME, iBus, iAddr);
        if (RT_FAILURE(rc))
        {
            mLastError = rc;
            break;
        }

        LogFlowFunc((": Opening %s\n", pszDevicePath));

        FileUsb = open(pszDevicePath, O_RDONLY);
        if (FileUsb < 0)
        {
            if (   (errno != ENOENT)
                && (errno != EACCES))
                mLastError = RTErrConvertFromErrno(errno);

            RTStrFree(pszDevicePath);

            if ((errno == ENOENT) && (iAddr > 1))
            {
                iAddr = 1;
                iBus++;
                continue;
            }
            else if (errno == EACCES)
            {
                /* Skip devices without the right permission. */
                iAddr++;
                continue;
            }
            else
                break;
        }

        LogFlowFunc((": %s opened successfully\n", pszDevicePath));

        struct usb_device_info UsbDevInfo;
        RT_ZERO(UsbDevInfo);

        rc = ioctl(FileUsb, USB_GET_DEVICEINFO, &UsbDevInfo);
        if (rc < 0)
        {
            LogFlowFunc((": Error querying device info rc=%Rrc\n", RTErrConvertFromErrno(errno)));
            mLastError = RTErrConvertFromErrno(errno);
            close(FileUsb);
            RTStrFree(pszDevicePath);
            break;
        }

        /* Filter out hubs */
        if (UsbDevInfo.udi_class != 0x09)
        {
            PUSBDEVICE pDevice = (PUSBDEVICE)RTMemAllocZ(sizeof(USBDEVICE));
            if (!pDevice)
            {
                mLastError = VERR_NO_MEMORY;
                close(FileUsb);
                RTStrFree(pszDevicePath);
                break;
            }

            pDevice->enmState           = USBDEVICESTATE_UNUSED;
            pDevice->bBus               = UsbDevInfo.udi_bus;
            pDevice->bDeviceClass       = UsbDevInfo.udi_class;
            pDevice->bDeviceSubClass    = UsbDevInfo.udi_subclass;
            pDevice->bDeviceProtocol    = UsbDevInfo.udi_protocol;
            pDevice->bNumConfigurations = UsbDevInfo.udi_config_no;
            pDevice->idVendor           = UsbDevInfo.udi_vendorNo;
            pDevice->idProduct          = UsbDevInfo.udi_productNo;
            pDevice->bDevNum            = UsbDevInfo.udi_index;

            switch (UsbDevInfo.udi_speed)
            {
                case USB_SPEED_LOW:
                    pDevice->enmSpeed = USBDEVICESPEED_LOW;
                    break;
                case USB_SPEED_FULL:
                    pDevice->enmSpeed = USBDEVICESPEED_FULL;
                    break;
                case USB_SPEED_HIGH:
                    pDevice->enmSpeed = USBDEVICESPEED_HIGH;
                    break;
                case USB_SPEED_SUPER:
                case USB_SPEED_VARIABLE:
                default:
                    pDevice->enmSpeed = USBDEVICESPEED_UNKNOWN;
            }

            if (UsbDevInfo.udi_vendor[0] != '\0')
                pDevice->pszManufacturer = RTStrDupN(UsbDevInfo.udi_vendor, sizeof(UsbDevInfo.udi_vendor));

            if (UsbDevInfo.udi_product[0] != '\0')
                pDevice->pszProduct = RTStrDupN(UsbDevInfo.udi_product, sizeof(UsbDevInfo.udi_product));

            if (UsbDevInfo.udi_serial[0] != '\0')
            {
                pDevice->pszSerialNumber = RTStrDupN(UsbDevInfo.udi_serial, sizeof(UsbDevInfo.udi_serial));
                pDevice->u64SerialHash   = USBLibHashSerial(pDevice->pszSerialNumber);
            }
            rc = ioctl(FileUsb, USB_GET_PLUGTIME, &PlugTime);
            if (rc == 0)
                pDevice->u64SerialHash  += PlugTime;

            pDevice->pszAddress = RTStrDup(pszDevicePath);
            pDevice->pszBackend = RTStrDup("host");
            pDevice->enmState   = USBDEVICESTATE_USED_BY_HOST_CAPTURABLE;

            usbLogDevice(pDevice);

            pDevice->pNext = pDevices;
            if (pDevices)
                pDevices->pPrev = pDevice;
            pDevices = pDevice;
        }
        close(FileUsb);
        RTStrFree(pszDevicePath);
        iAddr++;
    }

    return pDevices;
}

