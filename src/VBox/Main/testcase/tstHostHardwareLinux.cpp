/* $Id$ */
/** @file
 *
 * Test executable for quickly excercising/debugging the Linux host hardware
 * bits.
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <HostHardwareLinux.h>
#include <USBGetDevices.h>

#include <VBox/err.h>

#include <iprt/initterm.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/thread.h>
#include <iprt/linux/sysfs.h>

#include <iprt/cdefs.h>
#include <iprt/types.h>

#include <string.h>
#include <stdlib.h>

int doHotplugEvent(VBoxMainHotplugWaiter *waiter, RTMSINTERVAL cMillies)
{
    int rc;
    while (true)
    {
        rc = waiter->Wait (cMillies);
        if (rc == VERR_TIMEOUT || rc == VERR_INTERRUPTED)
            break;
        if (RT_FAILURE(rc))
        {
            RTPrintf("Failed!\n");
            exit(1);
        }
        if (RT_SUCCESS(rc))
            break;
    }
    return rc;
}

void printDevices(PUSBDEVICE pDevices,
                  const char *pcszDevices,
                  const char *pcszMethod)
{
    PUSBDEVICE pDevice = pDevices;

    RTPrintf("Enumerating usb devices using %s at %s\n", pcszMethod, pcszDevices);
    while (pDevice)
    {
        const char *pcszState =
              pDevice->enmState == USBDEVICESTATE_UNSUPPORTED   ? "UNSUPPORTED"
            : pDevice->enmState == USBDEVICESTATE_USED_BY_HOST  ? "USED_BY_HOST"
            : pDevice->enmState == USBDEVICESTATE_USED_BY_HOST_CAPTURABLE ? "USED_BY_HOST_CAPTURABLE"
            : pDevice->enmState == USBDEVICESTATE_UNUSED        ? "UNUSED"
            : pDevice->enmState == USBDEVICESTATE_HELD_BY_PROXY ? "held by proxy"
            : pDevice->enmState == USBDEVICESTATE_USED_BY_GUEST ? "used by guest"
            :                                                     "invalid";
        RTPrintf("  Manufacturer: %s, product: %s, serial number string: %s, state: %s\n",
                    pDevice->pszManufacturer, pDevice->pszProduct,
                    pDevice->pszSerialNumber, pcszState);
        RTPrintf("    Device address: %s\n", pDevice->pszAddress);
        pDevice = pDevice->pNext;
    }
}

void freeDevices(PUSBDEVICE pDevices)
{
    PUSBDEVICE pDevice = pDevices, pDeviceNext;

    while (pDevice)
    {
        pDeviceNext = pDevice->pNext;
        deviceFree(pDevice);
        pDevice = pDeviceNext;
    }
}

int main()
{
    RTR3Init();
    int rc = VINF_SUCCESS;
    VBoxMainDriveInfo driveInfo;
    rc = driveInfo.updateFloppies();
    if (RT_SUCCESS(rc))
        rc = driveInfo.updateDVDs();
    if (RT_FAILURE(rc))
    {
        RTPrintf("Failed to update the host drive information, error %Rrc\n",
                 rc);
        return 1;
    }
    RTPrintf ("Listing floppy drives detected:\n");
    for (DriveInfoList::const_iterator it = driveInfo.FloppyBegin();
         it != driveInfo.FloppyEnd(); ++it)
    {
        RTPrintf ("  device: %s", it->mDevice.c_str());
        if (!it->mUdi.isEmpty())
            RTPrintf (", udi: %s", it->mUdi.c_str());
        if (!it->mDescription.isEmpty())
            RTPrintf (", description: %s", it->mDescription.c_str());
        RTPrintf ("\n");
    }
    RTPrintf ("Listing DVD drives detected:\n");
    for (DriveInfoList::const_iterator it = driveInfo.DVDBegin();
         it != driveInfo.DVDEnd(); ++it)
    {
        RTPrintf ("  device: %s", it->mDevice.c_str());
        if (!it->mUdi.isEmpty())
            RTPrintf (", udi: %s", it->mUdi.c_str());
        if (!it->mDescription.isEmpty())
            RTPrintf (", description: %s", it->mDescription.c_str());
        RTPrintf ("\n");
    }
    RTPrintf("NOTE: checking for usbfs at /dev/bus/usb, not /proc/bus/usb!!!\n");
    if (USBProxyLinuxCheckDeviceRoot("/dev/bus/usb", false))
    {
        PUSBDEVICE pDevice = USBProxyLinuxGetDevices("/dev/bus/usb",
                                                     false);
        printDevices(pDevice, "/dev/bus/usb", "usbfs");
        freeDevices(pDevice);
    }
    else
        RTPrintf("-> not found\n");
#ifdef VBOX_USB_WITH_SYSFS
    RTPrintf("Testing for USB devices at /dev/vboxusb\n");
    if (USBProxyLinuxCheckDeviceRoot("/dev/vboxusb", true))
    {
        PUSBDEVICE pDevice = USBProxyLinuxGetDevices("/dev/vboxusb",
                                                     true);
        printDevices(pDevice, "/dev/vboxusb", "sysfs");
        freeDevices(pDevice);
    }
    else
        RTPrintf("-> not found\n");
    VBoxMainHotplugWaiter waiter("/dev/vboxusb");
    RTPrintf ("Waiting for a hotplug event for five seconds...\n");
    doHotplugEvent(&waiter, 5000);
    RTPrintf ("Waiting for a hotplug event, Ctrl-C to abort...\n");
    doHotplugEvent(&waiter, RT_INDEFINITE_WAIT);
    RTPrintf ("Testing interrupting a hotplug event...\n");
    waiter.Interrupt();
    rc = doHotplugEvent(&waiter, 5000);
    RTPrintf ("%s\n", rc == VERR_INTERRUPTED ? "Success!" : "Failed!");
#endif  /* VBOX_USB_WITH_SYSFS */
    return 0;
}

