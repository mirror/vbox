/* $Id$ */
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
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_MAIN

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

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/process.h>
#include <iprt/assert.h>
#include <iprt/thread.h>

#include "iokit.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** An attempt at catching reference leaks. */
#define MY_CHECK_CREFS(cRefs)   do { AssertMsg(cRefs < 25, ("%ld\n", cRefs)); NOREF(cRefs); } while (0)

/** Contains the pid of the current client. If 0, the kernel is the current client. */
#define VBOXUSB_CLIENT_KEY  "VBoxUSB-Client"
/** Contains the pid of the filter owner (i.e. the VBoxSVC pid). */
#define VBOXUSB_OWNER_KEY   "VBoxUSB-Owner"
/** The VBoxUSBDevice class name. */
#define VBOXUSBDEVICE_CLASS_NAME "org_virtualbox_VBoxUSBDevice"


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
 * Gets a RTPROCESS value.
 *
 * @returns Success indicator (true/false).
 * @param   DictRef     The dictionary.
 * @param   KeyStrRef   The key name.
 * @param   pProcess    Where to store the key value.
 */
static bool darwinDictGetProccess(CFMutableDictionaryRef DictRef, CFStringRef KeyStrRef, PRTPROCESS pProcess)
{
    switch (sizeof(*pProcess))
    {
        case sizeof(uint16_t):  return darwinDictGetU16(DictRef, KeyStrRef, (uint16_t *)pProcess);
        case sizeof(uint32_t):  return darwinDictGetU32(DictRef, KeyStrRef, (uint32_t *)pProcess);
        case sizeof(uint64_t):  return darwinDictGetU64(DictRef, KeyStrRef, (uint64_t *)pProcess);
        default:
            AssertMsgFailedReturn(("%d\n", sizeof(*pProcess)), false);
    }
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


#if 1 /* dumping disabled */
# define DARWIN_IOKIT_LOG(a)         Log(a)
# define DARWIN_IOKIT_LOG_FLUSH()    do {} while (0)
# define DARWIN_IOKIT_DUMP_OBJ(o)    do {} while (0)
#else
# if 0
#  include <iprt/stream.h>
#  define DARWIN_IOKIT_LOG(a) RTPrintf a
#  define DARWIN_IOKIT_LOG_FLUSH() RTStrmFlush(g_pStdOut)
# else
#  define DARWIN_IOKIT_LOG(a) RTLogPrintf a
#  define DARWIN_IOKIT_LOG(a) RTLogFlush()
# endif
# define DARWIN_IOKIT_DUMP_OBJ(o)    darwinDumpObj(o)

/**
 * Callback for dumping a dictionary key.
 *
 * @param   pvKey       The key name.
 * @param   pvValue     The key value
 * @param   pvUser      The recursion depth.
 */
static void darwinDumpDictCallback(const void *pvKey, const void *pvValue, void *pvUser)
{
    /* display the key name. */
    char *pszKey = (char *)RTMemTmpAlloc(1024);
    if (!CFStringGetCString((CFStringRef)pvKey, pszKey, 1024, kCFStringEncodingUTF8))
        strcpy(pszKey, "CFStringGetCString failure");
    DARWIN_IOKIT_LOG(("%+*s%s", (int)(uintptr_t)pvUser, "", pszKey));
    RTMemTmpFree(pszKey);

    /* display the value type */
    CFTypeID Type = CFGetTypeID(pvValue);
    DARWIN_IOKIT_LOG((" [%d-", Type));

    /* display the value */
    if (Type == CFDictionaryGetTypeID())
    {
        DARWIN_IOKIT_LOG(("dictionary] =\n"
                     "%-*s{\n", (int)(uintptr_t)pvUser, ""));
        CFDictionaryApplyFunction((CFDictionaryRef)pvValue, darwinDumpDictCallback, (void *)((uintptr_t)pvUser + 4));
        DARWIN_IOKIT_LOG(("%-*s}\n", (int)(uintptr_t)pvUser, ""));
    }
    else if (Type == CFNumberGetTypeID())
    {
        union
        {
            SInt8 s8;
            SInt16 s16;
            SInt32 s32;
            SInt64 s64;
            Float32 rf32;
            Float64 rd64;
            char ch;
            short s;
            int i;
            long l;
            long long ll;
            float rf;
            double rd;
            CFIndex iCF;
        } u;
        memset(&u, 0, sizeof(u));
        CFNumberType NumType = CFNumberGetType((CFNumberRef)pvValue);
        if (CFNumberGetValue((CFNumberRef)pvValue, NumType, &u))
        {
            switch (CFNumberGetType((CFNumberRef)pvValue))
            {
                case kCFNumberSInt8Type:    DARWIN_IOKIT_LOG(("SInt8] = %RI8 (%#RX8)\n", NumType, u.s8, u.s8)); break;
                case kCFNumberSInt16Type:   DARWIN_IOKIT_LOG(("SInt16] = %RI16 (%#RX16)\n", NumType, u.s16, u.s16)); break;
                case kCFNumberSInt32Type:   DARWIN_IOKIT_LOG(("SInt32] = %RI32 (%#RX32)\n", NumType, u.s32, u.s32)); break;
                case kCFNumberSInt64Type:   DARWIN_IOKIT_LOG(("SInt64] = %RI64 (%#RX64)\n", NumType, u.s64, u.s64)); break;
                case kCFNumberFloat32Type:  DARWIN_IOKIT_LOG(("float32] = %#lx\n", NumType, u.l)); break;
                case kCFNumberFloat64Type:  DARWIN_IOKIT_LOG(("float64] = %#llx\n", NumType, u.ll)); break;
                case kCFNumberFloatType:    DARWIN_IOKIT_LOG(("float] = %#lx\n", NumType, u.l)); break;
                case kCFNumberDoubleType:   DARWIN_IOKIT_LOG(("double] = %#llx\n", NumType, u.ll)); break;
                case kCFNumberCharType:     DARWIN_IOKIT_LOG(("char] = %hhd (%hhx)\n", NumType, u.ch, u.ch)); break;
                case kCFNumberShortType:    DARWIN_IOKIT_LOG(("short] = %hd (%hx)\n", NumType, u.s, u.s)); break;
                case kCFNumberIntType:      DARWIN_IOKIT_LOG(("int] = %d (%#x)\n", NumType, u.i, u.i)); break;
                case kCFNumberLongType:     DARWIN_IOKIT_LOG(("long] = %ld (%#lx)\n", NumType, u.l, u.l)); break;
                case kCFNumberLongLongType: DARWIN_IOKIT_LOG(("long long] = %lld (%#llx)\n", NumType, u.ll, u.ll)); break;
                case kCFNumberCFIndexType:  DARWIN_IOKIT_LOG(("CFIndex] = %lld (%#llx)\n", NumType, (long long)u.iCF, (long long)u.iCF)); break;
                    break;
                default:                    DARWIN_IOKIT_LOG(("%d?] = %lld (%llx)\n", NumType, u.ll, u.ll)); break;
            }
        }
        else
            DARWIN_IOKIT_LOG(("number] = CFNumberGetValue failed\n"));
    }
    else if (Type == CFBooleanGetTypeID())
        DARWIN_IOKIT_LOG(("boolean] = %RTbool\n", CFBooleanGetValue((CFBooleanRef)pvValue)));
    else if (Type == CFStringGetTypeID())
    {
        DARWIN_IOKIT_LOG(("string] = "));
        char *pszValue = (char *)RTMemTmpAlloc(16*_1K);
        if (!CFStringGetCString((CFStringRef)pvValue, pszValue, 16*_1K, kCFStringEncodingUTF8))
            strcpy(pszValue, "CFStringGetCString failure");
        DARWIN_IOKIT_LOG(("\"%s\"\n", pszValue));
        RTMemTmpFree(pszValue);
    }
    else
        DARWIN_IOKIT_LOG(("??] = %p\n", pvValue));
}


/**
 * Dumps a dictionary to the log.
 *
 * @param   DictRef     The dictionary to dump.
 */
static void darwinDumpDict(CFMutableDictionaryRef DictRef, unsigned cIndents)
{
    CFDictionaryApplyFunction(DictRef, darwinDumpDictCallback, (void *)(uintptr_t)cIndents);
    DARWIN_IOKIT_LOG_FLUSH();
}


/**
 * Dumps an I/O kit registry object and all it children.
 * @param   Object      The object to dump.
 * @param   cIndents    The number of indents to use.
 */
static void darwinDumpObjInt(io_object_t Object, unsigned cIndents)
{
    static io_string_t s_szPath;
    kern_return_t krc = IORegistryEntryGetPath(Object, kIOServicePlane, s_szPath);
    if (krc != KERN_SUCCESS)
        strcpy(s_szPath, "IORegistryEntryGetPath failed");
    DARWIN_IOKIT_LOG(("Dumping %p - %s:\n", (const void *)Object, s_szPath));

    CFMutableDictionaryRef PropsRef = 0;
    krc = IORegistryEntryCreateCFProperties(Object, &PropsRef, kCFAllocatorDefault, kNilOptions);
    if (krc == KERN_SUCCESS)
    {
        darwinDumpDict(PropsRef, cIndents + 4);
        CFRelease(PropsRef);
    }

    /*
     * Children.
     */
    io_iterator_t Children;
    krc = IORegistryEntryGetChildIterator(Object, kIOServicePlane, &Children);
    if (krc == KERN_SUCCESS)
    {
        io_object_t Child;
        while ((Child = IOIteratorNext(Children)))
        {
            darwinDumpObjInt(Child, cIndents + 4);
            IOObjectRelease(Child);
        }
        IOObjectRelease(Children);
    }
    else
        DARWIN_IOKIT_LOG(("IORegistryEntryGetChildIterator -> %#x\n", krc));
}

/**
 * Dumps an I/O kit registry object and all it children.
 * @param   Object      The object to dump.
 */
static void darwinDumpObj(io_object_t Object)
{
    darwinDumpObjInt(Object, 0);
}

#endif


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
    {
        DARWIN_IOKIT_DUMP_OBJ(Object);
        IOObjectRelease(Object);
    }
}


/**
 * Callback for the 1st attach notification.
 *
 * @param   pvNotify        Our data.
 * @param   NotifyIterator  The notification iterator.
 */
static void darwinUSBAttachNotification1(void *pvNotify, io_iterator_t NotifyIterator)
{
    DARWIN_IOKIT_LOG(("USB Attach Notification1\n"));
    NOREF(pvNotify); //PDARWINUSBNOTIFY pNotify = (PDARWINUSBNOTIFY)pvNotify;
    darwinDrainIterator(NotifyIterator);
}


/**
 * Callback for the 2nd attach notification.
 *
 * @param   pvNotify        Our data.
 * @param   NotifyIterator  The notification iterator.
 */
static void darwinUSBAttachNotification2(void *pvNotify, io_iterator_t NotifyIterator)
{
    DARWIN_IOKIT_LOG(("USB Attach Notification2\n"));
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
    DARWIN_IOKIT_LOG(("USB Detach Notification\n"));
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
                                                                darwinUSBAttachNotification1,
                                                                pNotify,
                                                                &pNotify->AttachIterator);
            if (rc == KERN_SUCCESS)
            {
                darwinDrainIterator(pNotify->AttachIterator);
                rc = IOServiceAddMatchingNotification(pNotify->NotifyPort,
                                                      kIOMatchedNotification,
                                                      IOServiceMatching(kIOUSBDeviceClassName),
                                                      darwinUSBAttachNotification2,
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
 * Decends recursivly into a IORegistry tree locating the first object of a given class.
 *
 * The search is performed depth first.
 *
 * @returns Object reference if found, NULL if not.
 * @param   Object      The current tree root.
 * @param   pszClass    The name of the class we're looking for.
 * @param   pszNameBuf  A scratch buffer for query the class name in to avoid
 *                      wasting 128 bytes on an io_name_t object for every recursion.
 */
static io_object_t darwinFindObjectByClass(io_object_t Object, const char *pszClass, io_name_t pszNameBuf)
{
    io_iterator_t Children;
    kern_return_t krc = IORegistryEntryGetChildIterator(Object, kIOServicePlane, &Children);
    if (krc != KERN_SUCCESS)
        return NULL;
    io_object_t Child;
    while ((Child = IOIteratorNext(Children)))
    {
        krc = IOObjectGetClass(Child, pszNameBuf);
        if (    krc == KERN_SUCCESS
            &&  !strcmp(pszNameBuf, pszClass))
            break;

        io_object_t GrandChild = darwinFindObjectByClass(Child, pszClass, pszNameBuf);
        IOObjectRelease(Child);
        if (GrandChild)
        {
            Child = GrandChild;
            break;
        }
    }
    IOObjectRelease(Children);
    return Child;
}


/**
 * Decends recursivly into IOUSBMassStorageClass tree to check whether
 * the MSD is mounted or not.
 *
 * The current heuristic is to look for the IOMedia class.
 *
 * @returns true if mounted, false if not.
 * @param   MSDObj      The IOUSBMassStorageClass object.
 * @param   pszNameBuf  A scratch buffer for query the class name in to avoid
 *                      wasting 128 bytes on an io_name_t object for every recursion.
 */
static bool darwinIsMassStorageInterfaceInUse(io_object_t MSDObj, io_name_t pszNameBuf)
{
    io_object_t MediaObj = darwinFindObjectByClass(MSDObj, "IOMedia", pszNameBuf);
    if (MediaObj)
    {
        /* more checks? */
        IOObjectRelease(MediaObj);
        return true;
    }
    return false;
}


/**
 * Worker function for DarwinGetUSBDevices() that tries to figure out
 * what state the device is in.
 *
 * This is mostly a matter of distinguishing between devices that nobody
 * uses, devices that can be seized and devices that cannot be grabbed.
 *
 * @param   pCur        The USB device data.
 * @param   USBDevice   The USB device object.
 * @param   PropsRef    The USB device properties.
 */
static void darwinDeterminUSBDeviceState(PUSBDEVICE pCur, io_object_t USBDevice, CFMutableDictionaryRef PropsRef)
{
    /*
     * Iterate the interfaces (among the children of the IOUSBDevice object).
     */
    io_iterator_t Interfaces;
    kern_return_t krc = IORegistryEntryGetChildIterator(USBDevice, kIOServicePlane, &Interfaces);
    if (krc != KERN_SUCCESS)
        return;

    RTPROCESS Owner = NIL_RTPROCESS;
    RTPROCESS Client = NIL_RTPROCESS;
    bool fUserClientOnly = true;
    bool fConfigured = false;
    bool fInUse = false;
    bool fSeizable = true;
    io_object_t Interface;
    while ((Interface = IOIteratorNext(Interfaces)))
    {
        io_name_t szName;
        krc = IOObjectGetClass(Interface, szName);
        if (    krc == KERN_SUCCESS
            &&  !strcmp(szName, "IOUSBInterface"))
        {
            fConfigured = true;

            /*
             * Iterate the interface children looking for stuff other than
             * IOUSBUserClientInit objects.
             */
            io_iterator_t Children1;
            krc = IORegistryEntryGetChildIterator(Interface, kIOServicePlane, &Children1);
            if (krc == KERN_SUCCESS)
            {
                io_object_t Child1;
                while ((Child1 = IOIteratorNext(Children1)))
                {
                    krc = IOObjectGetClass(Child1, szName);
                    if (    krc == KERN_SUCCESS
                        &&  strcmp(szName, "IOUSBUserClientInit"))
                    {
                        fUserClientOnly = false;

                        if (!strcmp(szName, "IOUSBMassStorageClass"))
                        {
                            /* Only permit capturing MSDs that aren't mounted, at least
                               until the GUI starts poping up warnings about data loss
                               and such when capturing a busy device. */
                            fSeizable = false;
                            fInUse |= darwinIsMassStorageInterfaceInUse(Child1, szName);
                        }
                        else if (!strcmp(szName, "IOUSBHIDDriver")
                              || !strcmp(szName, "AppleHIDMouse")
                              /** @todo more? */)
                        {
                            /* For now, just assume that all HID devices are inaccessible
                               because of the greedy HID service. */
                            fSeizable = false;
                            fInUse = true;
                        }
                        else
                            fInUse = true;
                    }
                    IOObjectRelease(Child1);
                }
                IOObjectRelease(Children1);
            }
        }
        /*
         * Not an interface, could it be VBoxUSBDevice?
         * If it is, get the owner and client properties.
         */
        else if (    krc == KERN_SUCCESS
                 &&  !strcmp(szName, VBOXUSBDEVICE_CLASS_NAME))
        {
            CFMutableDictionaryRef PropsRef = 0;
            krc = IORegistryEntryCreateCFProperties(Interface, &PropsRef, kCFAllocatorDefault, kNilOptions);
            if (krc == KERN_SUCCESS)
            {
                darwinDictGetProccess(PropsRef, CFSTR(VBOXUSB_OWNER_KEY), &Owner);
                darwinDictGetProccess(PropsRef, CFSTR(VBOXUSB_CLIENT_KEY), &Client);
                CFRelease(PropsRef);
            }
        }

        IOObjectRelease(Interface);
    }
    IOObjectRelease(Interfaces);

    /*
     * Calc the status.
     */
    if (    Owner != NIL_RTPROCESS
        &&  !Owner)
    {
        if (Owner == RTProcSelf())
            pCur->enmState = Client == NIL_RTPROCESS || !Client
                           ? USBDEVICESTATE_HELD_BY_PROXY
                           : USBDEVICESTATE_USED_BY_GUEST;
        else
            pCur->enmState = USBDEVICESTATE_USED_BY_HOST;
    }
    else if (fUserClientOnly)
        /** @todo how to detect other user client?!? - Look for IOUSBUserClient! */
        pCur->enmState = !fConfigured
                       ? USBDEVICESTATE_UNUSED
                       : USBDEVICESTATE_USED_BY_HOST_CAPTURABLE;
    else if (!fInUse)
        pCur->enmState = USBDEVICESTATE_UNUSED;
    else
        pCur->enmState = fSeizable
                       ? USBDEVICESTATE_USED_BY_HOST_CAPTURABLE
                       : USBDEVICESTATE_USED_BY_HOST;
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
    //DARWIN_IOKIT_LOG(("DarwinGetUSBDevices\n"));

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
        //DARWIN_IOKIT_DUMP_OBJ(USBDevice);

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
                pCur->enmState = USBDEVICESTATE_USED_BY_HOST_CAPTURABLE;    /* just a default, we'll try harder in a bit. */

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
                uint8_t bSpeed;
                AssertBreak(darwinDictGetU8(PropsRef,  CFSTR(kUSBDevicePropertySpeed),  &bSpeed),);
                Assert(bSpeed <= 2);
                pCur->enmSpeed = bSpeed == 2 ? USBDEVICESPEED_HIGH
                               : bSpeed == 1 ? USBDEVICESPEED_FULL
                               : bSpeed == 0 ? USBDEVICESPEED_LOW
                                             : USBDEVICESPEED_UNKNOWN;

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
                 * Try determin the state.
                 */
                darwinDeterminUSBDeviceState(pCur, USBDevice, PropsRef);

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
                DarwinFreeUSBDeviceFromIOKit(pCur);

            CFRelease(PropsRef);
        }
        else
            AssertMsgFailed(("krc=%#x\n", krc));

        IOObjectRelease(USBDevice);
        i++;
    }

    IOObjectRelease(USBDevices);
    //DARWIN_IOKIT_LOG_FLUSH();

    /*
     * Some post processing. There are a couple of things we have to
     * make 100% sure about, and that is that the (Apple) keyboard
     * and mouse most likely to be in use by the user aren't available
     * for capturing. If there is no Apple mouse or keyboard we'll
     * take the first one from another vendor.
     */
    /* As it turns out, the HID service will take all keyboards and mice
       and we're not currently able to seize them. */
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


/**
 * Triggers re-enumeration of a device.
 *
 * @returns VBox status code.
 * @param   pCur    The USBDEVICE structure for the device.
 */
int DarwinReEnumerateUSBDevice(PCUSBDEVICE pCur)
{
    int vrc;
    const char *pszAddress = pCur->pszAddress;
    AssertPtrReturn(pszAddress, VERR_INVALID_POINTER);
    AssertReturn(darwinOpenMasterPort(), VERR_GENERAL_FAILURE);

    /*
     * This code is a short version of the Open method in USBProxyDevice-darwin.cpp stuff.
     * Fixes made to this code probably applies there too!
     */

    CFMutableDictionaryRef RefMatchingDict = IOServiceMatching(kIOUSBDeviceClassName);
    AssertReturn(RefMatchingDict, NULL);

    uint64_t u64SessionId = 0;
    uint32_t u32LocationId = 0;
    const char *psz = pszAddress;
    do
    {
        const char chValue = *psz;
        AssertReleaseReturn(psz[1] == '=', VERR_INTERNAL_ERROR);
        uint64_t u64Value;
        int rc = RTStrToUInt64Ex(psz + 2, (char **)&psz, 0, &u64Value);
        AssertReleaseRCReturn(rc, rc);
        AssertReleaseReturn(!*psz || *psz == ';', rc);
        switch (chValue)
        {
            case 'l':
                u32LocationId = (uint32_t)u64Value;
                break;
            case 's':
                u64SessionId = u64Value;
                break;
            case 'p':
            case 'v':
            {
#if 0 /* Guess what, this doesn't 'ing work either! */
                SInt32 i32 = (int16_t)u64Value;
                CFNumberRef Num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i32);
                AssertBreak(Num,);
                CFDictionarySetValue(RefMatchingDict, chValue == 'p' ? CFSTR(kUSBProductID) : CFSTR(kUSBVendorID), Num);
                CFRelease(Num);
#endif
                break;
            }
            default:
                AssertReleaseMsgFailedReturn(("chValue=%#x\n", chValue), VERR_INTERNAL_ERROR);
        }
        if (*psz == ';')
            psz++;
    } while (*psz);

    io_iterator_t USBDevices = NULL;
    IOReturn irc = IOServiceGetMatchingServices(g_MasterPort, RefMatchingDict, &USBDevices);
    AssertMsgReturn(irc == kIOReturnSuccess, ("irc=%#x\n", irc), NULL);
    RefMatchingDict = NULL; /* the reference is consumed by IOServiceGetMatchingServices. */

    unsigned cMatches = 0;
    io_object_t USBDevice;
    while ((USBDevice = IOIteratorNext(USBDevices)))
    {
        cMatches++;
        CFMutableDictionaryRef PropsRef = 0;
        kern_return_t krc = IORegistryEntryCreateCFProperties(USBDevice, &PropsRef, kCFAllocatorDefault, kNilOptions);
        if (krc == KERN_SUCCESS)
        {
            uint64_t u64CurSessionId;
            uint32_t u32CurLocationId;
            if (    (    !u64SessionId
                     || (   darwinDictGetU64(PropsRef, CFSTR("sessionID"), &u64CurSessionId)
                         && u64CurSessionId == u64SessionId))
                &&  (   !u32LocationId
                     || (   darwinDictGetU32(PropsRef, CFSTR(kUSBDevicePropertyLocationID), &u32CurLocationId)
                         && u32CurLocationId == u32LocationId))
                )
            {
                CFRelease(PropsRef);
                break;
            }
            CFRelease(PropsRef);
        }
        IOObjectRelease(USBDevice);
    }
    IOObjectRelease(USBDevices);
    USBDevices = NULL;
    if (!USBDevice)
    {
        LogRel(("USB: Device '%s' not found (%d pid+vid matches)\n", pszAddress, cMatches));
        IOObjectRelease(USBDevices);
        return VERR_VUSB_DEVICE_NAME_NOT_FOUND;
    }

    /*
     * Create a plugin interface for the device and query its IOUSBDeviceInterface.
     */
    SInt32 Score = 0;
    IOCFPlugInInterface **ppPlugInInterface = NULL;
    irc = IOCreatePlugInInterfaceForService(USBDevice, kIOUSBDeviceUserClientTypeID,
                                            kIOCFPlugInInterfaceID, &ppPlugInInterface, &Score);
    if (irc == kIOReturnSuccess)
    {
        IOUSBDeviceInterface245 **ppDevI = NULL;
        HRESULT hrc = (*ppPlugInInterface)->QueryInterface(ppPlugInInterface,
                                                           CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245),
                                                           (LPVOID *)&ppDevI);
        irc = IODestroyPlugInInterface(ppPlugInInterface); Assert(irc == kIOReturnSuccess);
        ppPlugInInterface = NULL;
        if (hrc == S_OK)
        {
            /*
             * Try open the device for exclusive access.
             */
            irc = (*ppDevI)->USBDeviceOpenSeize(ppDevI);
            if (irc == kIOReturnExclusiveAccess)
            {
                RTThreadSleep(20);
                irc = (*ppDevI)->USBDeviceOpenSeize(ppDevI);
            }
            if (irc == kIOReturnSuccess)
            {
                /*
                 * Re-enumerate the device and bail out.
                 */
                irc = (*ppDevI)->USBDeviceReEnumerate(ppDevI, 0);
                if (irc != kIOReturnSuccess)
                {
                    LogRel(("USB: Failed to open device '%s', plug-in creation failed with irc=%#x.\n", pszAddress, irc));
                    vrc = RTErrConvertFromDarwinIO(irc);
                }

                (*ppDevI)->USBDeviceClose(ppDevI);
            }
            else if (irc == kIOReturnExclusiveAccess)
            {
                LogRel(("USB: Device '%s' is being used by another process\n", pszAddress));
                vrc = VERR_SHARING_VIOLATION;
            }
            else
            {
                LogRel(("USB: Failed to open device '%s', irc=%#x.\n", pszAddress, irc));
                vrc = VERR_OPEN_FAILED;
            }
        }
        else
        {
            LogRel(("USB: Failed to create plugin interface for device '%s', hrc=%#x.\n", pszAddress, hrc));
            vrc = VERR_OPEN_FAILED;
        }

        (*ppDevI)->Release(ppDevI);
    }
    else
    {
        LogRel(("USB: Failed to open device '%s', plug-in creation failed with irc=%#x.\n", pszAddress, irc));
        vrc = RTErrConvertFromDarwinIO(irc);
    }

    return vrc;
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

