/* $Id$ */
/** @file
 * IPRT - RTSystemQueryDmiString, darwin ring-3.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/system.h>
#include "internal/iprt.h"

#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include <stdio.h>
#include <mach/mach_port.h>
#include <IOKit/IOKitLib.h>

#define IOCLASS_PLATFORMEXPERTDEVICE         "IOPlatformExpertDevice"
#define PROP_PRODUCT_NAME                    "product-name"
#define PROP_PRODUCT_VERSION                 "version"
#define PROP_PRODUCT_SERIAL                  "IOPlatformSerialNumber"
#define PROP_PRODUCT_UUID                    "IOPlatformUUID"

RTDECL(int) RTSystemQueryDmiString(RTSYSDMISTR enmString, char *pszBuf, size_t cbBuf)
{
    AssertPtrReturn(pszBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf > 0, VERR_INVALID_PARAMETER);
    *pszBuf = '\0';
    AssertReturn(enmString > RTSYSDMISTR_INVALID && enmString < RTSYSDMISTR_END, VERR_INVALID_PARAMETER);

    CFStringRef PropStringRef = NULL;
    switch (enmString)
    {
        case RTSYSDMISTR_PRODUCT_NAME:    PropStringRef = CFSTR(PROP_PRODUCT_NAME);    break;
        case RTSYSDMISTR_PRODUCT_VERSION: PropStringRef = CFSTR(PROP_PRODUCT_VERSION); break;
        case RTSYSDMISTR_PRODUCT_SERIAL:  PropStringRef = CFSTR(PROP_PRODUCT_SERIAL);  break;
        case RTSYSDMISTR_PRODUCT_UUID:    PropStringRef = CFSTR(PROP_PRODUCT_UUID);    break;
        default:
            return VERR_NOT_SUPPORTED;
    }

    mach_port_t MasterPort;
    kern_return_t kr = IOMasterPort(MACH_PORT_NULL, &MasterPort);
    if (kr != kIOReturnSuccess)
    {
        if (kr == KERN_NO_ACCESS)
            return VERR_ACCESS_DENIED;
        return RTErrConvertFromDarwinIO(kr);
    }

    CFDictionaryRef ClassToMatch = IOServiceMatching(IOCLASS_PLATFORMEXPERTDEVICE);
    if (!ClassToMatch)
        return VERR_NOT_SUPPORTED;

    io_iterator_t Iterator;
    kr = IOServiceGetMatchingServices(MasterPort, ClassToMatch, &Iterator);
    if (kr != kIOReturnSuccess)
        return RTErrConvertFromDarwinIO(kr);

    int rc = VERR_NOT_SUPPORTED;
    io_service_t ServiceObject;
    while ((ServiceObject = IOIteratorNext(Iterator)))
    {
        if (   enmString == RTSYSDMISTR_PRODUCT_NAME
            || enmString == RTSYSDMISTR_PRODUCT_VERSION)
        {
            CFDataRef DataRef = (CFDataRef)IORegistryEntryCreateCFProperty(ServiceObject, PropStringRef, kCFAllocatorDefault, kNilOptions);
            if (DataRef)
            {
                size_t cbData = CFDataGetLength(DataRef);
                const uint8_t *pu8Data = CFDataGetBytePtr(DataRef);
                rc = RTStrCopy(pszBuf, RT_MIN(cbData, cbBuf), (const char*)pu8Data);
                break;
            }
        }
        else
        {
            CFStringRef StringRef = (CFStringRef)IORegistryEntryCreateCFProperty(ServiceObject, PropStringRef, kCFAllocatorDefault, kNilOptions);
            if (StringRef)
            {
                Boolean res = CFStringGetCString(StringRef, pszBuf, cbBuf, kCFStringEncodingASCII);
                rc = res == TRUE ? VINF_SUCCESS : VERR_NOT_SUPPORTED;
                break;
            }
        }
    }

    IOObjectRelease(ServiceObject);
    IOObjectRelease(Iterator);
    return rc;
}
