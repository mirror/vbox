/* $Id$ */
/** @file
 *
 * Test executable for quickly excercising/debugging the Linux host hardware
 * bits.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
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

#include <errno.h>
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

void printDevices(PUSBDEVICE pDevices, const char *pcszAccess)
{
    PUSBDEVICE pDevice = pDevices;

    RTPrintf("Enumerating usb devices using %s\n", pcszAccess);
    while (pDevice)
    {
        RTPrintf("  Manufacturer: %s, product: %s, serial number string: %s\n",
                    pDevice->pszManufacturer, pDevice->pszProduct,
                    pDevice->pszSerialNumber);
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
#ifdef VBOX_USB_WITH_SYSFS
    PUSBDEVICE pDevice = USBProxyLinuxGetDevices(NULL);
    printDevices(pDevice, "sysfs");
    freeDevices(pDevice);
    if (USBProxyLinuxCheckForUsbfs("/proc/bus/usb/devices"))
    {
        pDevice = USBProxyLinuxGetDevices("/proc/bus/usb");
        printDevices(pDevice, "/proc/bus/usb");
        freeDevices(pDevice);
    }
    if (USBProxyLinuxCheckForUsbfs("/dev/bus/usb/devices"))
    {
        pDevice = USBProxyLinuxGetDevices("/dev/bus/usb");
        printDevices(pDevice, "/dev/bus/usb");
        freeDevices(pDevice);
    }
    VBoxMainHotplugWaiter waiter;
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

