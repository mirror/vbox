/* $Id$ */
/** @file
 * VirtualBox Linux host USB device enumeration.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___USBGetDevices_h
# define ___USBGetDevices_h

#include <VBox/usb.h>
#include <iprt/mem.h>
#include <iprt/string.h>

/**
 * Free all the members of a USB device created by the Linux enumeration code.
 * @note this duplicates a USBProxyService method which we needed access too
 *       without pulling in the rest of the proxy service code.
 *
 * @param   pDevice     Pointer to the device.
 */
static inline void deviceFreeMembers(PUSBDEVICE pDevice)
{
    RTStrFree((char *)pDevice->pszManufacturer);
    pDevice->pszManufacturer = NULL;
    RTStrFree((char *)pDevice->pszProduct);
    pDevice->pszProduct = NULL;
    RTStrFree((char *)pDevice->pszSerialNumber);
    pDevice->pszSerialNumber = NULL;

    RTStrFree((char *)pDevice->pszAddress);
    pDevice->pszAddress = NULL;
}

/**
 * Free one USB device created by the Linux enumeration code.
 * @note this duplicates a USBProxyService method which we needed access too
 *       without pulling in the rest of the proxy service code.
 *
 * @param   pDevice     Pointer to the device.
 */
static inline void deviceFree(PUSBDEVICE pDevice)
{
    deviceFreeMembers(pDevice);
    RTMemFree(pDevice);
}

/**
 * Free a linked list of USB devices created by the Linux enumeration code.
 * @param  pHead  Pointer to the first device in the linked list
 */
static inline void deviceListFree(PUSBDEVICE *ppHead)
{
    PUSBDEVICE pHead, pNext;
    pHead = *ppHead;
    while (pHead)
    {
        pNext = pHead->pNext;
        deviceFree(pHead);
        pHead = pNext;
    }
    *ppHead = NULL;
}

RT_C_DECLS_BEGIN

/**
 * Check whether a USB device tree root is usable
 * @param pcszRoot        the path to the root of the device tree
 * @param fIsDeviceNodes  whether this is a device node (or usbfs) tree
 * @note  returns a pointer into a static array so it will stay valid
 */
extern bool USBProxyLinuxCheckDeviceRoot(const char *pcszRoot,
                                         bool fIsDeviceNodes);

#ifdef UNIT_TEST
void TestUSBSetupInit(const char *pcszUsbfsRoot, bool fUsbfsAccessible,
                      const char *pcszDevicesRoot, bool fDevicesAccessible,
                      int rcMethodInitResult);
void TestUSBSetEnv(const char *pcszEnvUsb, const char *pcszEnvUsbRoot);
#endif

/**
 * Selects the access method that will be used to access USB devices based on
 * what is available on the host and what if anything the user has specified
 * in the environment.
 * @returns iprt status value
 * @param  pfUsingUsbfsDevices  on success this will be set to true if 
 *                              the prefered access method is USBFS-like and to
 *                              false if it is sysfs/device node-like
 * @param  ppcszDevicesRoot     on success the root of the tree of USBFS-like
 *                              device nodes will be stored here
 */
extern int USBProxyLinuxChooseMethod(bool *pfUsingUsbfsDevices,
                                     const char **ppcszDevicesRoot);
#ifdef UNIT_TEST
/**
 * Specify the list of devices that will appear to be available through
 * usbfs during unit testing (of USBProxyLinuxGetDevices)
 * @param  pacszDeviceAddresses  NULL terminated array of usbfs device addresses
 */
extern void TestUSBSetAvailableUsbfsDevices(const char **pacszDeviceAddresses);
/**
 * Specify the list of files that access will report as accessible (at present
 * we only do accessible or not accessible) during unit testing (of
 * USBProxyLinuxGetDevices)
 * @param  pacszAccessibleFiles  NULL terminated array of file paths to be
 *                               reported accessible
 */
extern void TestUSBSetAccessibleFiles(const char **pacszAccessibleFiles);
#endif

/**
 * Get the list of USB devices supported by the system.  Should be freed using
 * @a deviceFree or something equivalent.
 * @param pcszDevicesRoot  the path to the root of the device tree
 * @param fUseSysfs        whether to use sysfs (or usbfs) for enumeration
 */
extern PUSBDEVICE USBProxyLinuxGetDevices(const char *pcszDevicesRoot,
                                          bool fUseSysfs);

RT_C_DECLS_END

#endif /* ___USBGetDevices_h */
