/** $Id: $ */
/** @file
 * Main - Darwin IOKit Routines.
 *
 * Because IOKit makes use of COM like interfaces, it does not mix very
 * well with COM/XPCOM and must therefore be isolated from it using a
 * simpler C interface.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <mach/mach.h>
#include <Carbon/Carbon.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/scsi-commands/SCSITaskLib.h>
#include <mach/mach_error.h>

#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/assert.h>

#include "iokit.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The IO Master Port. */
static mach_port_t g_MasterPort = NULL;



/**
 * Enumerate the DVD drives returning a FIFO of device name strings.
 *
 * @returns Pointer to the head.
 *          The caller is responsible for calling RTMemFree() on each of the nodes.
 */
PDARWINDVD DarwinGetDVDDrives(void)
{
    PDARWINDVD pHead = NULL;
    PDARWINDVD pTail = NULL;

    /*
     * Open the master port on the first invocation.
     */
    if (!g_MasterPort)
    {
        kern_return_t krc = IOMasterPort(MACH_PORT_NULL, &g_MasterPort);
        AssertReturn(krc == KERN_SUCCESS, NULL);
    }

    /*
     * Create a matching dictionary for searching for DVD services in the IOKit.
     *
     * [If I understand this correctly, plain CDROMs doesn't show up as
     * IODVDServices. Too keep things simple, we will only support DVDs
     * until somebody complains about it and we get hardware to test it on.
     * (Unless I'm much mistaken, there aren't any (orignal) intel macs with
     * plain cdroms.)]
     */
    CFMutableDictionaryRef RefMatchingDict = IOServiceMatching("IODVDServices");
    AssertReturn(RefMatchingDict, NULL);

    /*
     * do the search and get a collection of keyboards.
     */
    io_iterator_t DVDServices = NULL;
    IOReturn rc = IOServiceGetMatchingServices(g_MasterPort, RefMatchingDict, &DVDServices);
    AssertMsgReturn(rc == kIOReturnSuccess, ("rc=%d\n", rc), NULL);
    RefMatchingDict = NULL; /* the reference is consumed by IOServiceGetMatchingServices. */

    /*
     * Enumerate the DVD drives (services).
     * (This enumeration must be identical to the one performed in DrvHostBase.cpp.)
     */
    unsigned i = 0;
    io_object_t DVDService;
    while ((DVDService = IOIteratorNext(DVDServices)) != 0)
    {
        /*
         * Get the properties we use to identify the DVD drive.
         *
         * While there is a (weird 12 byte) GUID, it isn't persistent
         * accross boots. So, we have to use a combination of the
         * vendor name and product name properties with an optional
         * sequence number for identification.
         */
        CFMutableDictionaryRef PropsRef = 0;
        kern_return_t krc = IORegistryEntryCreateCFProperties(DVDService, &PropsRef, kCFAllocatorDefault, kNilOptions);
        if (krc == KERN_SUCCESS)
        {
            /* Get the Device Characteristics dictionary. */
            CFDictionaryRef DevCharRef = (CFDictionaryRef)CFDictionaryGetValue(PropsRef, CFSTR(kIOPropertyDeviceCharacteristicsKey));
            if (DevCharRef)
            {
                /* The vendor name. */
                char szVendor[128];
                char *pszVendor = &szVendor[0];
                CFTypeRef ValueRef = CFDictionaryGetValue(DevCharRef, CFSTR(kIOPropertyVendorNameKey));
                if (    ValueRef
                    &&  CFGetTypeID(ValueRef) == CFStringGetTypeID()
                    &&  CFStringGetCString((CFStringRef)ValueRef, szVendor, sizeof(szVendor), kCFStringEncodingUTF8))
                    pszVendor = RTStrStrip(szVendor);
                else
                    *pszVendor = '\0';

                /* The product name. */
                char szProduct[128];
                char *pszProduct = &szProduct[0];
                ValueRef = CFDictionaryGetValue(DevCharRef, CFSTR(kIOPropertyProductNameKey));
                if (    ValueRef
                    &&  CFGetTypeID(ValueRef) == CFStringGetTypeID()
                    &&  CFStringGetCString((CFStringRef)ValueRef, szProduct, sizeof(szProduct), kCFStringEncodingUTF8))
                    pszProduct = RTStrStrip(szProduct);
                else
                    *pszProduct = '\0';

                /* Construct the name and check for duplicates. */
                char szName[256 + 32];
                if (*pszVendor || *pszProduct)
                {
                    if (*pszVendor && *pszProduct)
                        RTStrPrintf(szName, sizeof(szName), "%s %s", pszVendor, pszProduct);
                    else
                        strcpy(szName, *pszVendor ? pszVendor : pszProduct);

                    for (PDARWINDVD pCur = pHead; pCur; pCur = pCur->pNext)
                    {
                        if (!strcmp(szName, pCur->szName))
                        {
                            if (*pszVendor && *pszProduct)
                                RTStrPrintf(szName, sizeof(szName), "%s %s (#%u)", pszVendor, pszProduct, i);
                            else
                                RTStrPrintf(szName, sizeof(szName), "%s %s (#%u)", *pszVendor ? pszVendor : pszProduct, i);
                            break;
                        }
                    }
                }
                else
                    RTStrPrintf(szName, sizeof(szName), "(#%u)", i);

                /* Create the device. */
                size_t cbName = strlen(szName) + 1;
                PDARWINDVD pNew = (PDARWINDVD)RTMemAlloc(RT_OFFSETOF(DARWINDVD, szName[cbName]));
                if (pNew)
                {
                    pNew->pNext = NULL;
                    memcpy(pNew->szName, szName, cbName);
                    if (pTail)
                        pTail = pTail->pNext = pNew;
                    else
                        pTail = pHead = pNew;
                }
            }
            CFRelease(PropsRef);
        }
        else
            AssertMsgFailed(("krc=%#x\n", krc));

        IOObjectRelease(DVDService);
        i++;
    }

    IOObjectRelease(DVDServices);

    return pHead;
}
