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
 * Check whether @a pcszDevices is a valid usbfs devices file for checking
 * whether usbfs is supported or not.
 * @returns VINF_SUCCESS if it is
 * @returns VERR_NOT_FOUND if it isn't
 * @returns iprt status code if an error occurred
 * @todo  test whether USB with sysfs/dev is supported
 */
extern int USBProxyLinuxCheckForUsbfs(const char *pcszDevices);

/**
 * Get the list of USB devices supported by the system.  Should be freed using
 * @a deviceFree or something equivalent.
 * @param pcszUsbfsRoot  the path to usbfs, or NULL to use sysfs
 */
extern PUSBDEVICE USBProxyLinuxGetDevices(const char *pcszUsbfsRoot);

#endif /* ___USBGetDevices_h */
