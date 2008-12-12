/* $Id$ */
/** @file
 *
 * Test executable for quickly excercising/debugging the Linux host hardware
 * bits.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <HostHardwareLinux.h>

#include <VBox/err.h>

#include <iprt/initterm.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/linux/sysfs.h>

#include <iprt/cdefs.h>
#include <iprt/types.h>

#include <errno.h>
#include <string.h>

int main()
{
    RTR3Init();
    g_testHostHardwareLinux = true;
    int rc = VINF_SUCCESS;
    VBoxMainDriveInfo driveInfo;
    g_testHostHardwareLinux = true;
    rc = driveInfo.updateFloppies();
    if (RT_SUCCESS (rc))
        rc = driveInfo.updateDVDs();
    if (RT_FAILURE (rc))
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
        if (!it->mUdi.empty())
            RTPrintf (", udi: %s", it->mUdi.c_str());
        if (!it->mDescription.empty())
            RTPrintf (", description: %s", it->mDescription.c_str());
        RTPrintf ("\n");
    }
    RTPrintf ("Listing DVD drives detected:\n");
    for (DriveInfoList::const_iterator it = driveInfo.DVDBegin();
         it != driveInfo.DVDEnd(); ++it)
    {
        RTPrintf ("  device: %s", it->mDevice.c_str());
        if (!it->mUdi.empty())
            RTPrintf (", udi: %s", it->mUdi.c_str());
        if (!it->mDescription.empty())
            RTPrintf (", description: %s", it->mDescription.c_str());
        RTPrintf ("\n");
    }
#ifdef VBOX_USB_WITH_SYSFS
    VBoxMainUSBDeviceInfo deviceInfo;
    rc = deviceInfo.UpdateDevices();
    if (RT_FAILURE (rc))
    {
        RTPrintf ("Failed to update the host USB device information, error %Rrc\n",
                 rc);
        return 1;
    }
    RTPrintf ("Listing USB devices detected:\n");
    for (USBDeviceInfoList::const_iterator it = deviceInfo.DevicesBegin();
         it != deviceInfo.DevicesEnd(); ++it)
    {
        char szProduct[1024];
        if (RTLinuxSysFsReadStrFile(szProduct, sizeof(szProduct),
                                    "%s/product", it->mSysfsPath.c_str()) == -1)
        {
            if (errno != ENOENT)
            {
                RTPrintf ("Failed to get the product name for device %s: error %s\n",
                          it->mDevice.c_str(), strerror(errno));
                return 1;
            }
            else
                szProduct[0] = '\0';
        }
        RTPrintf ("  device: %s (%s), sysfs path: %s\n", szProduct, it->mDevice.c_str(),
                  it->mSysfsPath.c_str());
        RTPrintf ("    interfaces:\n");
        for (USBInterfaceList::const_iterator it2 = it->mInterfaces.begin();
             it2 != it->mInterfaces.end(); ++it2)
        {
            char szDriver[RTPATH_MAX];
            strcpy(szDriver, "none");
            ssize_t size = RTLinuxSysFsGetLinkDest(szDriver, sizeof(szDriver),
                                                   "%s/driver", it2->c_str());
            if (size == -1 && errno != ENOENT)
            {
                RTPrintf ("Failed to get the driver for interface %s of device %s: error %s\n",
                          it2->c_str(), it->mDevice.c_str(), strerror(errno));
                return 1;
            }
            if (RTLinuxSysFsExists("%s/driver", it2->c_str()) != (size != -1))            
            {
                RTPrintf ("RTLinuxSysFsExists did not return the expected value for the driver link of interface %s of device %s.\n",
                          it2->c_str(), it->mDevice.c_str());
                return 1;
            }
            uint64_t u64InterfaceClass;
            u64InterfaceClass = RTLinuxSysFsReadIntFile(16, "%s/bInterfaceClass",
                                                        it2->c_str());
            RTPrintf ("      sysfs path: %s, driver: %s, interface class: 0x%x\n",
                      it2->c_str(), szDriver, u64InterfaceClass);
        }
    }
    RTPrintf ("Waiting for a hotplug event, Ctrl-C to abort...\n");
    VBoxMainHotplugWaiter waiter;
    waiter.Wait();
#endif  /* VBOX_USB_WITH_SYSFS */
    return 0;
}

