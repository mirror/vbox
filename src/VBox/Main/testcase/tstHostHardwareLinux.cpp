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
    VBoxMainUSBDeviceInfo deviceInfo;
    AssertReturn(VBoxMainUSBDevInfoInit(&deviceInfo), 1);
    rc = USBDevInfoUpdateDevices(&deviceInfo);
    if (RT_FAILURE(rc))
    {
        RTPrintf ("Failed to update the host USB device information, error %Rrc\n",
                 rc);
        return 1;
    }
    RTPrintf ("Listing USB devices detected:\n");
    USBDeviceInfoList_iterator it;
    USBDeviceInfoList_iter_init(&it, USBDevInfoBegin(&deviceInfo));
    for (; !USBDeviceInfoList_iter_eq(&it, USBDevInfoEnd(&deviceInfo));
           USBDeviceInfoList_iter_incr(&it))
    {
        char szProduct[1024];
        USBDeviceInfo *pInfo = USBDeviceInfoList_iter_target(&it);
        if (RTLinuxSysFsReadStrFile(szProduct, sizeof(szProduct),
                                    "%s/product", pInfo->mSysfsPath) == -1)
        {
            if (errno != ENOENT)
            {
                RTPrintf ("Failed to get the product name for device %s: error %s\n",
                          pInfo->mDevice, strerror(errno));
                return 1;
            }
            else
                szProduct[0] = '\0';
        }
        RTPrintf ("  device: %s (%s), sysfs path: %s\n", szProduct, pInfo->mDevice,
                  pInfo->mSysfsPath);
        RTPrintf ("    interfaces:\n");
        USBInterfaceList_iterator it2;
        USBInterfaceList_iter_init(&it2, USBInterfaceList_begin(&pInfo->mInterfaces));
        for (; !USBInterfaceList_iter_eq(&it2, USBInterfaceList_end(&pInfo->mInterfaces));
               USBInterfaceList_iter_incr(&it2))
        {
            char szDriver[RTPATH_MAX];
            char *pszIf = *USBInterfaceList_iter_target(&it2);
            strcpy(szDriver, "none");
            ssize_t size = RTLinuxSysFsGetLinkDest(szDriver, sizeof(szDriver),
                                                   "%s/driver", pszIf);
            if (size == -1 && errno != ENOENT)
            {
                RTPrintf ("Failed to get the driver for interface %s of device %s: error %s\n",
                          pszIf, pInfo->mDevice, strerror(errno));
                return 1;
            }
            if (RTLinuxSysFsExists("%s/driver", pszIf) != (size != -1))
            {
                RTPrintf ("RTLinuxSysFsExists did not return the expected value for the driver link of interface %s of device %s.\n",
                          pszIf, pInfo->mDevice);
                return 1;
            }
            uint64_t u64InterfaceClass;
            u64InterfaceClass = RTLinuxSysFsReadIntFile(16, "%s/bInterfaceClass",
                                                        pszIf);
            RTPrintf ("      sysfs path: %s, driver: %s, interface class: 0x%x\n",
                      pszIf, szDriver, u64InterfaceClass);
        }
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

