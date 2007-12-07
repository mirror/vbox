/** @file
 * USBLIB - USB Support Library:
 * This module implements the basic low-level OS interfaces for Windows hosts.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_usblib_win_h
#define ___VBox_usblib_win_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/usb.h>

#ifdef RT_OS_WINDOWS

#include <initguid.h>
// {6068EB61-98E7-4c98-9E20-1F068295909A}
DEFINE_GUID(GUID_CLASS_VBOXUSB, 0x873fdf, 0xCAFE, 0x80EE, 0xaa, 0x5e, 0x0, 0xc0, 0x4f, 0xb1, 0x72, 0xb);

#define USBFLT_SERVICE_NAME              "\\\\.\\VBoxUSBFlt"
#define USBFLT_NTDEVICE_NAME_STRING      L"\\Device\\VBoxUSBFlt"
#define USBFLT_SYMBOLIC_NAME_STRING      L"\\DosDevices\\VBoxUSBFlt"

#define USBMON_SERVICE_NAME              "VBoxUSBMon"
#define USBMON_DEVICE_NAME               "\\\\.\\VBoxUSBMon"
#define USBMON_DEVICE_NAME_NT            L"\\Device\\VBoxUSBMon"
#define USBMON_DEVICE_NAME_DOS           L"\\DosDevices\\VBoxUSBMon"

/*
 * IOCtl numbers.
 * We're using the Win32 type of numbers here, thus the macros below.
 */

#ifndef CTL_CODE
# if defined(RT_OS_WINDOWS)
#  define CTL_CODE(DeviceType, Function, Method, Access) \
    ( ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#else /* unix: */
#  define CTL_CODE(DeviceType, Function, Method_ignored, Access_ignored) \
    ( (3 << 30) | ((DeviceType) << 8) | (Function) | (sizeof(SUPDRVIOCTLDATA) << 16) )
# endif
#endif
#ifndef METHOD_BUFFERED
# define METHOD_BUFFERED        0
#endif
#ifndef FILE_WRITE_ACCESS
# define FILE_WRITE_ACCESS      0x0002
#endif
#ifndef FILE_DEVICE_UNKNOWN
# define FILE_DEVICE_UNKNOWN    0x00000022
#endif

#define USBFLT_MAJOR_VERSION              1
#define USBFLT_MINOR_VERSION              2

#define USBMON_MAJOR_VERSION              1
#define USBMON_MINOR_VERSION              1

#define USBDRV_MAJOR_VERSION              1
#define USBDRV_MINOR_VERSION              2

#define SUPUSB_IOCTL_TEST                 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x601, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSB_IOCTL_GET_DEVICE           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x603, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSB_IOCTL_SEND_URB             CTL_CODE(FILE_DEVICE_UNKNOWN, 0x607, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSB_IOCTL_USB_RESET            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x608, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSB_IOCTL_USB_SELECT_INTERFACE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x609, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSB_IOCTL_USB_SET_CONFIG       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x60A, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSB_IOCTL_USB_CLAIM_DEVICE     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x60B, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSB_IOCTL_USB_RELEASE_DEVICE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x60C, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSB_IOCTL_IS_OPERATIONAL       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x60D, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSB_IOCTL_USB_CLEAR_ENDPOINT   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x60E, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSB_IOCTL_GET_VERSION          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x60F, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define SUPUSBFLT_IOCTL_GET_NUM_DEVICES   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x602, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSBFLT_IOCTL_USB_CHANGE        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x604, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSBFLT_IOCTL_DISABLE_CAPTURE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x605, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSBFLT_IOCTL_ENABLE_CAPTURE    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x606, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSBFLT_IOCTL_IGNORE_DEVICE     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x60F, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSBFLT_IOCTL_GET_VERSION       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x610, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSBFLT_IOCTL_ADD_FILTER        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x611, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSBFLT_IOCTL_REMOVE_FILTER     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x612, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSBFLT_IOCTL_CAPTURE_DEVICE    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x613, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define SUPUSBFLT_IOCTL_RELEASE_DEVICE    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x614, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#pragma pack(4)

#define MAX_FILTER_NAME                 128
#define MAX_USB_SERIAL_STRING           64

typedef struct
{
    uint16_t        vid, did, rev;
    char            serial_hash[MAX_USB_SERIAL_STRING];

    uint8_t         fAttached;
} USBSUP_GETDEV, *PUSBSUP_GETDEV;

typedef struct
{
    uint32_t        u32Major;
    uint32_t        u32Minor;
} USBSUP_VERSION, *PUSBSUP_VERSION;

#endif /* RT_OS_WINDOWS */

#define MAX_VENDOR_NAME    16
#define MAX_PRODUCT_NAME   MAX_VENDOR_NAME
#define MAX_REVISION_NAME  MAX_VENDOR_NAME

typedef struct
{
    char            szVendor[MAX_VENDOR_NAME];
    char            szProduct[MAX_PRODUCT_NAME];
    char            szRevision[MAX_REVISION_NAME];
    uintptr_t       id;
} USBSUP_FILTER, *PUSBSUP_FILTER;


typedef struct
{
    USHORT          usVendorId;
    USHORT          usProductId;
    USHORT          usRevision;
} USBSUP_CAPTURE, *PUSBSUP_CAPTURE;

typedef USBSUP_CAPTURE      USBSUP_RELEASE;
typedef PUSBSUP_CAPTURE     PUSBSUP_RELEASE;

typedef struct
{
    uint8_t         bInterfaceNumber;
    uint8_t         fClaimed;
} USBSUP_CLAIMDEV, *PUSBSUP_CLAIMDEV;

typedef USBSUP_CLAIMDEV  USBSUP_RELEASEDEV;
typedef PUSBSUP_CLAIMDEV PUSBSUP_RELEASEDEV;

typedef struct
{
    uint32_t         cUSBDevices;
} USBSUP_GETNUMDEV, *PUSBSUP_GETNUMDEV;

typedef struct
{
    uint8_t          fUSBChange;
    uint32_t         cUSBStateChange;
} USBSUP_USB_CHANGE, *PUSBSUP_USB_CHANGE;

typedef struct
{
    uint8_t         bConfigurationValue;
} USBSUP_SET_CONFIG, *PUSBSUP_SET_CONFIG;

typedef struct
{
    uint8_t         bInterfaceNumber;
    uint8_t         bAlternateSetting;
} USBSUP_SELECT_INTERFACE, *PUSBSUP_SELECT_INTERFACE;

typedef struct
{
    uint8_t         bEndpoint;
} USBSUP_CLEAR_ENDPOINT, *PUSBSUP_CLEAR_ENDPOINT;

typedef enum
{
    USBSUP_TRANSFER_TYPE_CTRL = 0,
    USBSUP_TRANSFER_TYPE_ISOC = 1,
    USBSUP_TRANSFER_TYPE_BULK = 2,
    USBSUP_TRANSFER_TYPE_INTR = 3,
    USBSUP_TRANSFER_TYPE_MSG  = 4
} USBSUP_TRANSFER_TYPE;

typedef enum
{
    USBSUP_DIRECTION_SETUP = 0,
    USBSUP_DIRECTION_IN    = 1,
    USBSUP_DIRECTION_OUT   = 2
} USBSUP_DIRECTION;


typedef enum
{
    USBSUP_XFER_OK         = 0,
    USBSUP_XFER_STALL      = 1,
    USBSUP_XFER_DNR        = 2,
    USBSUP_XFER_CRC        = 3
} USBSUP_ERROR;

typedef struct
{
    USBSUP_TRANSFER_TYPE type; /* [in] QUSB_TRANSFER_TYPE_XXX */
    uint32_t ep;               /* [in] index to dev->pipe */
    USBSUP_DIRECTION     dir;  /* [in] QUSB_DIRECTION_XXX */
    uint32_t error;            /* [out] QUSB_XFER_XXX */
    size_t len;                /* [in/out] may change */
    void *buf;                 /* [in/out] depends on dir */
} USBSUP_URB, *PUSBSUP_URB;

#pragma pack()                          /* paranoia */


__BEGIN_DECLS

#ifdef IN_RING3

/** @defgroup   grp_usblib_r3     USBLIB Host Context Ring 3 API
 * @{
 */

/**
 * Initialize the USB library
 *
 * @returns VBox status code.
 */
VBOXDDU_DECL(int) usbLibInit();

/**
 * Terminate the USB library
 *
 * @returns VBox status code.
 */
VBOXDDU_DECL(int) usbLibTerm();

/**
 * Add USB device filter
 *
 * @returns VBox status code.
 * @param   pszVendor       Vendor filter string
 * @param   pszProduct      Product filter string
 * @param   pszRevision     Revision filter string
 * @param   ppID            Pointer to filter id
 */
VBOXDDU_DECL(int) usbLibInsertFilter(const char *pszVendor, const char *pszProduct, const char *pszRevision, void **ppID);

/**
 * Remove USB device filter
 *
 * @returns VBox status code.
 * @param   aID             Filter id
 */
VBOXDDU_DECL(int) usbLibRemoveFilter (void *aID);

/**
 * Return all attached USB devices.
 *
 * @returns VBox status code
 * @param ppDevices         Receives pointer to list of devices
 * @param pcbNumDevices     Number of USB devices in the list
 */
VBOXDDU_DECL(int) usbLibGetDevices(PUSBDEVICE *ppDevices,  uint32_t *pcbNumDevices);

/**
 * Check for USB device arrivals or removals
 *
 * @returns boolean
 */
VBOXDDU_DECL(bool) usbLibHasPendingDeviceChanges();


/**
 * Capture specified USB device
 *
 * @returns VBox status code
 * @param usVendorId        Vendor id
 * @param usProductId       Product id
 * @param usRevision        Revision
 */
VBOXDDU_DECL(int) usbLibCaptureDevice(USHORT usVendorId, USHORT usProductId, USHORT usRevision);

/**
 * Release specified USB device to the host.
 *
 * @returns VBox status code
 * @param usVendorId        Vendor id
 * @param usProductId       Product id
 * @param usRevision        Revision
 */
VBOXDDU_DECL(int) usbLibReleaseDevice(USHORT usVendorId, USHORT usProductId, USHORT usRevision);


/** @} */
#endif

/** @} */

__END_DECLS


#endif

