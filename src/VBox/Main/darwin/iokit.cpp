/** $Id: $ */
/** @file
 * Main - Darwin IOKit Routines.
 *
 * Because IOKit makes use of COM like interfaces, it does not mix very
 * well with COM/XPCOM and must therefore be isolated from it using a
 * simpler C interface.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#ifdef VBOX_WITH_USB
# include <IOKit/usb/IOUSBLib.h>
# include <IOKit/IOCFPlugIn.h>
#endif

#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/assert.h>

#include "iokit.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** An attempt at catching reference leaks. */
#define MY_CHECK_CREFS(cRefs)   do { AssertMsg(cRefs < 25, ("%ld\n", cRefs)); NOREF(cRefs); } while (0)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The IO Master Port. */
static mach_port_t g_MasterPort = NULL;


/**
 * Lazily opens the master port.
 *
 * @returns true if the port is open, false on failure (very unlikely).
 */
static bool darwinOpenMasterPort(void)
{
    if (!g_MasterPort)
    {
        kern_return_t krc = IOMasterPort(MACH_PORT_NULL, &g_MasterPort);
        AssertReturn(krc == KERN_SUCCESS, false);
    }
    return true;
}


#ifdef VBOX_WITH_USB

/**
 * Gets an unsigned 8-bit integer value.
 *
 * @returns Success indicator (true/false).
 * @param   DictRef     The dictionary.
 * @param   KeyStrRef   The key name.
 * @param   pu8         Where to store the key value.
 */
static bool darwinDictGetU8(CFMutableDictionaryRef DictRef, CFStringRef KeyStrRef, uint8_t *pu8)
{
    CFTypeRef ValRef = CFDictionaryGetValue(DictRef, KeyStrRef);
    if (ValRef)
    {
        if (CFNumberGetValue((CFNumberRef)ValRef, kCFNumberSInt8Type, pu8))
            return true;
    }
    *pu8 = 0;
    return false;
}


/**
 * Gets an unsigned 16-bit integer value.
 *
 * @returns Success indicator (true/false).
 * @param   DictRef     The dictionary.
 * @param   KeyStrRef   The key name.
 * @param   pu16        Where to store the key value.
 */
static bool darwinDictGetU16(CFMutableDictionaryRef DictRef, CFStringRef KeyStrRef, uint16_t *pu16)
{
    CFTypeRef ValRef = CFDictionaryGetValue(DictRef, KeyStrRef);
    if (ValRef)
    {
        if (CFNumberGetValue((CFNumberRef)ValRef, kCFNumberSInt16Type, pu16))
            return true;
    }
    *pu16 = 0;
    return false;
}


/**
 * Gets an unsigned 32-bit integer value.
 *
 * @returns Success indicator (true/false).
 * @param   DictRef     The dictionary.
 * @param   KeyStrRef   The key name.
 * @param   pu32        Where to store the key value.
 */
static bool darwinDictGetU32(CFMutableDictionaryRef DictRef, CFStringRef KeyStrRef, uint32_t *pu32)
{
    CFTypeRef ValRef = CFDictionaryGetValue(DictRef, KeyStrRef);
    if (ValRef)
    {
        if (CFNumberGetValue((CFNumberRef)ValRef, kCFNumberSInt32Type, pu32))
            return true;
    }
    *pu32 = 0;
    return false;
}


/**
 * Gets an unsigned 64-bit integer value.
 *
 * @returns Success indicator (true/false).
 * @param   DictRef     The dictionary.
 * @param   KeyStrRef   The key name.
 * @param   pu64        Where to store the key value.
 */
static bool darwinDictGetU64(CFMutableDictionaryRef DictRef, CFStringRef KeyStrRef, uint64_t *pu64)
{
    CFTypeRef ValRef = CFDictionaryGetValue(DictRef, KeyStrRef);
    if (ValRef)
    {
        if (CFNumberGetValue((CFNumberRef)ValRef, kCFNumberSInt64Type, pu64))
            return true;
    }
    *pu64 = 0;
    return false;
}


/**
 * Gets string value, converted to UTF-8 and put in a IPRT string buffer.
 *
 * @returns Success indicator (true/false).
 * @param   DictRef     The dictionary.
 * @param   KeyStrRef   The key name.
 * @param   ppsz        Where to store the key value. Free with RTStrFree. Set to NULL on failure.
 */
static bool darwinDictGetString(CFMutableDictionaryRef DictRef, CFStringRef KeyStrRef, char **ppsz)
{
    CFTypeRef ValRef = CFDictionaryGetValue(DictRef, KeyStrRef);
    if (ValRef)
    {
        char szBuf[512];
        if (CFStringGetCString((CFStringRef)ValRef, szBuf, sizeof(szBuf), kCFStringEncodingUTF8))
        {
            *ppsz = RTStrDup(RTStrStrip(szBuf));
            if (*ppsz)
                return true;
        }
    }
    *ppsz = NULL;
    return false;
}


/**
 * Notification data created by DarwinSubscribeUSBNotifications, used by
 * the callbacks and finally freed by DarwinUnsubscribeUSBNotifications.
 */
typedef struct DARWINUSBNOTIFY
{
    /** The notification port.
     * It's shared between the notification callbacks. */
    IONotificationPortRef NotifyPort;
    /** The run loop source for NotifyPort. */
    CFRunLoopSourceRef NotifyRLSrc;
    /** The attach notification iterator. */
    io_iterator_t AttachIterator;
    /** The 2nd attach notification iterator. */
    io_iterator_t AttachIterator2;
    /** The detach notificaiton iterator. */
    io_iterator_t DetachIterator;
} DARWINUSBNOTIFY, *PDARWINUSBNOTIFY;


/**
 * Run thru an interrator.
 *
 * The docs says this is necessary to start getting notifications,
 * so this function is called in the callbacks and right after
 * registering the notification.
 *
 * @param   pIterator   The iterator reference.
 */
static void darwinDrainIterator(io_iterator_t pIterator)
{
    io_object_t Object;
    while ((Object = IOIteratorNext(pIterator)))
        IOObjectRelease(Object);
}


/**
 * Callback for the two attach notifications.
 *
 * @param   pvNotify        Our data.
 * @param   NotifyIterator  The notification iterator.
 */
static void darwinUSBAttachNotification(void *pvNotify, io_iterator_t NotifyIterator)
{
    NOREF(pvNotify); //PDARWINUSBNOTIFY pNotify = (PDARWINUSBNOTIFY)pvNotify;
    darwinDrainIterator(NotifyIterator);
}


/**
 * Callback for the detach notifications.
 *
 * @param   pvNotify        Our data.
 * @param   NotifyIterator  The notification iterator.
 */
static void darwinUSBDetachNotification(void *pvNotify, io_iterator_t NotifyIterator)
{
    NOREF(pvNotify); //PDARWINUSBNOTIFY pNotify = (PDARWINUSBNOTIFY)pvNotify;
    darwinDrainIterator(NotifyIterator);
}


/**
 * Subscribes the run loop to USB notification events relevant to
 * device attach/detach.
 *
 * The source mode for these events is defined as VBOX_IOKIT_MODE_STRING
 * so that the caller can listen to events from this mode only and
 * re-evalutate the list of attached devices whenever an event arrives.
 *
 * @returns opaque for passing to the unsubscribe function. If NULL
 *          something unexpectedly failed during subscription.
 */
void *DarwinSubscribeUSBNotifications(void)
{
    AssertReturn(darwinOpenMasterPort(), NULL);

    PDARWINUSBNOTIFY pNotify = (PDARWINUSBNOTIFY)RTMemAllocZ(sizeof(*pNotify));
    AssertReturn(pNotify, NULL);

    /*
     * Create the notification port, bake it into a runloop source which we
     * then add to our run loop.
     */
    pNotify->NotifyPort = IONotificationPortCreate(g_MasterPort);
    Assert(pNotify->NotifyPort);
    if (pNotify->NotifyPort)
    {
        pNotify->NotifyRLSrc = IONotificationPortGetRunLoopSource(pNotify->NotifyPort);
        Assert(pNotify->NotifyRLSrc);
        if (pNotify->NotifyRLSrc)
        {
            CFRunLoopAddSource(CFRunLoopGetCurrent(), pNotify->NotifyRLSrc, CFSTR(VBOX_IOKIT_MODE_STRING));

            /*
             * Create the notifcation callbacks.
             */
            kern_return_t rc = IOServiceAddMatchingNotification(pNotify->NotifyPort,
                                                                kIOPublishNotification,
                                                                IOServiceMatching(kIOUSBDeviceClassName),
                                                                darwinUSBAttachNotification,
                                                                pNotify,
                                                                &pNotify->AttachIterator);
            if (rc == KERN_SUCCESS)
            {
                darwinDrainIterator(pNotify->AttachIterator);
                rc = IOServiceAddMatchingNotification(pNotify->NotifyPort,
                                                      kIOMatchedNotification,
                                                      IOServiceMatching(kIOUSBDeviceClassName),
                                                      darwinUSBAttachNotification,
                                                      pNotify,
                                                      &pNotify->AttachIterator2);
                if (rc == KERN_SUCCESS)
                {
                    darwinDrainIterator(pNotify->AttachIterator2);
                    rc = IOServiceAddMatchingNotification(pNotify->NotifyPort,
                                                          kIOTerminatedNotification,
                                                          IOServiceMatching(kIOUSBDeviceClassName),
                                                          darwinUSBDetachNotification,
                                                          pNotify,
                                                          &pNotify->DetachIterator);
                    {
                        darwinDrainIterator(pNotify->DetachIterator);
                        return pNotify;
                    }
                    IOObjectRelease(pNotify->AttachIterator2);
                }
                IOObjectRelease(pNotify->AttachIterator);
            }
            CFRunLoopRemoveSource(CFRunLoopGetCurrent(), pNotify->NotifyRLSrc, CFSTR(VBOX_IOKIT_MODE_STRING));
        }
        IONotificationPortDestroy(pNotify->NotifyPort);
    }

    RTMemFree(pNotify);
    return NULL;
}


/**
 * Unsubscribe the run loop from USB notification subscribed to
 * by DarwinSubscribeUSBNotifications.
 *
 * @param   pvOpaque    The return value from DarwinSubscribeUSBNotifications.
 */
void DarwinUnsubscribeUSBNotifications(void *pvOpaque)
{
    PDARWINUSBNOTIFY pNotify = (PDARWINUSBNOTIFY)pvOpaque;
    if (!pNotify)
        return;

    IOObjectRelease(pNotify->AttachIterator);
    pNotify->AttachIterator = NULL;
    IOObjectRelease(pNotify->AttachIterator2);
    pNotify->AttachIterator2 = NULL;
    IOObjectRelease(pNotify->DetachIterator);
    pNotify->DetachIterator = NULL;

    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), pNotify->NotifyRLSrc, CFSTR(VBOX_IOKIT_MODE_STRING));
    IONotificationPortDestroy(pNotify->NotifyPort);
    pNotify->NotifyRLSrc = NULL;
    pNotify->NotifyPort = NULL;

    RTMemFree(pNotify);
}


/**
 * Enumerate the USB devices returning a FIFO of them.
 *
 * @returns Pointer to the head.
 *          USBProxyService::freeDevice is expected to free each of the list elements.
 */
PUSBDEVICE DarwinGetUSBDevices(void)
{
    AssertReturn(darwinOpenMasterPort(), NULL);

    /*
     * Create a matching dictionary for searching for USB Devices in the IOKit.
     */
    CFMutableDictionaryRef RefMatchingDict = IOServiceMatching(kIOUSBDeviceClassName);
    AssertReturn(RefMatchingDict, NULL);

    /*
     * Perform the search and get a collection of USB Device back.
     */
    io_iterator_t USBDevices = NULL;
    IOReturn rc = IOServiceGetMatchingServices(g_MasterPort, RefMatchingDict, &USBDevices);
    AssertMsgReturn(rc == kIOReturnSuccess, ("rc=%d\n", rc), NULL);
    RefMatchingDict = NULL; /* the reference is consumed by IOServiceGetMatchingServices. */

    /*
     * Enumerate the USB Devices.
     */
    PUSBDEVICE pHead = NULL;
    PUSBDEVICE pTail = NULL;
    unsigned i = 0;
    io_object_t USBDevice;
    while ((USBDevice = IOIteratorNext(USBDevices)) != 0)
    {
        /*
         * Query the device properties from the registry.
         *
         * We could alternatively use the device and such, but that will be
         * slower and we would have to resort to the registry for the three
         * string anyway.
         */
        CFMutableDictionaryRef PropsRef = 0;
        kern_return_t krc = IORegistryEntryCreateCFProperties(USBDevice, &PropsRef, kCFAllocatorDefault, kNilOptions);
        if (krc == KERN_SUCCESS)
        {
            bool fOk = false;
            PUSBDEVICE pCur = (PUSBDEVICE)RTMemAllocZ(sizeof(*pCur));
            do /* loop for breaking out of on failure. */
            {
                AssertBreak(pCur,);

                /*
                 * Mandatory
                 */
                pCur->bcdUSB = 0;                                           /* we've no idea. */
                pCur->enmState = USBDEVICESTATE_USED_BY_HOST_CAPTURABLE;    /* ditto. */

                AssertBreak(darwinDictGetU8(PropsRef,  CFSTR(kUSBDeviceClass),           &pCur->bDeviceClass),);
                /* skip hubs */
                if (pCur->bDeviceClass == 0x09 /* hub, find a define! */)
                    break;
                AssertBreak(darwinDictGetU8(PropsRef,  CFSTR(kUSBDeviceSubClass),       &pCur->bDeviceSubClass),);
                AssertBreak(darwinDictGetU8(PropsRef,  CFSTR(kUSBDeviceProtocol),       &pCur->bDeviceProtocol),);
                AssertBreak(darwinDictGetU16(PropsRef, CFSTR(kUSBVendorID),             &pCur->idVendor),);
                AssertBreak(darwinDictGetU16(PropsRef, CFSTR(kUSBProductID),            &pCur->idProduct),);
                AssertBreak(darwinDictGetU16(PropsRef, CFSTR(kUSBDeviceReleaseNumber),  &pCur->bcdDevice),);
                uint32_t u32LocationId;
                AssertBreak(darwinDictGetU32(PropsRef, CFSTR(kUSBDevicePropertyLocationID), &u32LocationId),);
                uint64_t u64SessionId;
                AssertBreak(darwinDictGetU64(PropsRef, CFSTR("sessionID"), &u64SessionId),);
                char szAddress[64];
                RTStrPrintf(szAddress, sizeof(szAddress), "p=0x%04RX16;v=0x%04RX16;s=0x%016RX64;l=0x%08RX32",
                            pCur->idProduct, pCur->idVendor, u64SessionId, u32LocationId);
                pCur->pszAddress = RTStrDup(szAddress);
                AssertBreak(pCur->pszAddress,);

                /*
                 * Optional.
                 * There are some nameless device in the iMac, apply names to them.
                 */
                darwinDictGetString(PropsRef, CFSTR("USB Vendor Name"),     (char **)&pCur->pszManufacturer);
                if (    !pCur->pszManufacturer
                    &&  pCur->idVendor == kIOUSBVendorIDAppleComputer)
                    pCur->pszManufacturer = RTStrDup("Apple Computer, Inc.");
                darwinDictGetString(PropsRef, CFSTR("USB Product Name"),    (char **)&pCur->pszProduct);
                if (    !pCur->pszProduct
                    &&  pCur->bDeviceClass == 224 /* Wireless */
                    &&  pCur->bDeviceSubClass == 1 /* Radio Frequency */
                    &&  pCur->bDeviceProtocol == 1 /* Bluetooth */)
                    pCur->pszProduct = RTStrDup("Bluetooth");
                darwinDictGetString(PropsRef, CFSTR("USB Serial Number"),   (char **)&pCur->pszSerialNumber);

#if 0           /* leave the remainder as zero for now. */
                /*
                 * Create a plugin interface for the service and query its USB Device interface.
                 */
                SInt32 Score = 0;
                IOCFPlugInInterface **ppPlugInInterface = NULL;
                rc = IOCreatePlugInInterfaceForService(USBDevice, kIOUSBDeviceUserClientTypeID,
                                                       kIOCFPlugInInterfaceID, &ppPlugInInterface, &Score);
                if (rc == kIOReturnSuccess)
                {
                    IOUSBDeviceInterface245 **ppUSBDevI = NULL;
                    HRESULT hrc = (*ppPlugInInterface)->QueryInterface(ppPlugInInterface,
                                                                       CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245),
                                                                       (LPVOID *)&ppUSBDevI);
                    rc = IODestroyPlugInInterface(ppPlugInInterface); Assert(rc == kIOReturnSuccess);
                    ppPlugInInterface = NULL;
                    if (hrc == S_OK)
                    {
                        /** @todo enumerate configurations and interfaces if we actually need them. */
                        //IOReturn (*GetNumberOfConfigurations)(void *self, UInt8 *numConfig);
                        //IOReturn (*GetConfigurationDescriptorPtr)(void *self, UInt8 configIndex, IOUSBConfigurationDescriptorPtr *desc);
                        //IOReturn (*CreateInterfaceIterator)(void *self, IOUSBFindInterfaceRequest *req, io_iterator_t *iter);
                    }
                    long cReft = (*ppUSBDeviceInterface)->Release(ppUSBDeviceInterface); MY_CHECK_CREFS(cRefs);
                }
#endif

                /*
                 * We're good. Link the device.
                 */
                pCur->pPrev = pTail;
                if (pTail)
                    pTail = pTail->pNext = pCur;
                else
                    pTail = pHead = pCur;
                fOk = true;
            } while (0);

            /* cleanup on failure / skipped device. */
            if (!fOk && pCur)
            {
                /** @todo */
            }

            CFRelease(PropsRef);
        }
        else
            AssertMsgFailed(("krc=%#x\n", krc));

        IOObjectRelease(USBDevice);
        i++;
    }

    IOObjectRelease(USBDevices);

    /*
     * Some post processing. There are a couple of things we have to
     * make 100% sure about, and that is that the (Apple) keyboard
     * and mouse most likely to be in use by the user aren't available
     * for capturing. If there is no Apple mouse or keyboard we'll
     * take the first one from another vendor.
     */
    PUSBDEVICE pMouse = NULL;
    PUSBDEVICE pKeyboard = NULL;
    for (PUSBDEVICE pCur = pHead; pCur; pCur = pCur->pNext)
        if (pCur->idVendor == kIOUSBVendorIDAppleComputer)
        {
            /*
             * This test is a bit rough, should check device class/protocol but
             * we don't have interface info yet so that might be a bit tricky.
             */
            if (    (   !pKeyboard
                     || pKeyboard->idVendor != kIOUSBVendorIDAppleComputer)
                &&  pCur->pszProduct
                &&  strstr(pCur->pszProduct, " Keyboard"))
                pKeyboard = pCur;
            else if (    (   !pMouse
                          || pMouse->idVendor != kIOUSBVendorIDAppleComputer)
                     &&  pCur->pszProduct
                     &&  strstr(pCur->pszProduct, " Mouse")
                )
                pMouse = pCur;
        }
        else if (!pKeyboard || !pMouse)
        {
            if (    pCur->bDeviceClass == 3         /* HID */
                &&  pCur->bDeviceProtocol == 1      /* Keyboard */)
                pKeyboard = pCur;
            else if (   pCur->bDeviceClass == 3     /* HID */
                     && pCur->bDeviceProtocol == 2  /* Mouse */)
                pMouse = pCur;
            /** @todo examin interfaces */
        }

    if (pKeyboard)
        pKeyboard->enmState = USBDEVICESTATE_USED_BY_HOST;
    if (pMouse)
        pMouse->enmState = USBDEVICESTATE_USED_BY_HOST;

    return pHead;
}

#endif /* VBOX_WITH_USB */


/**
 * Enumerate the DVD drives returning a FIFO of device name strings.
 *
 * @returns Pointer to the head.
 *          The caller is responsible for calling RTMemFree() on each of the nodes.
 */
PDARWINDVD DarwinGetDVDDrives(void)
{
    AssertReturn(darwinOpenMasterPort(), NULL);

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
     * Perform the search and get a collection of DVD services.
     */
    io_iterator_t DVDServices = NULL;
    IOReturn rc = IOServiceGetMatchingServices(g_MasterPort, RefMatchingDict, &DVDServices);
    AssertMsgReturn(rc == kIOReturnSuccess, ("rc=%d\n", rc), NULL);
    RefMatchingDict = NULL; /* the reference is consumed by IOServiceGetMatchingServices. */

    /*
     * Enumerate the DVD services.
     * (This enumeration must be identical to the one performed in DrvHostBase.cpp.)
     */
    PDARWINDVD pHead = NULL;
    PDARWINDVD pTail = NULL;
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

