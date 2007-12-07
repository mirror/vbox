/** @file
 * USB - Universal Serial Bus.
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

#ifndef ___VBox_usb_h
#define ___VBox_usb_h

#include <VBox/types.h>

__BEGIN_DECLS

/**
 * USB device interface endpoint.
 */
typedef struct USBENDPOINT
{
    /** The address of the endpoint on the USB device described by this descriptor. */
    uint8_t     bEndpointAddress;
    /** This field describes the endpoint's attributes when it is configured using the bConfigurationValue. */
    uint8_t     bmAttributes;
    /** Maximum packet size this endpoint is capable of sending or receiving when this configuration is selected. */
    uint16_t    wMaxPacketSize;
    /** Interval for polling endpoint for data transfers. Expressed in milliseconds.
     * This is interpreted the bInterval value. */
    uint16_t    u16Interval;
} USBENDPOINT;
/** Pointer to a USB device interface endpoint. */
typedef USBENDPOINT *PUSBENDPOINT;
/** Pointer to a const USB device interface endpoint. */
typedef const USBENDPOINT *PCUSBENDPOINT;

/** USBENDPOINT::bmAttributes values.
 * @{ */
#define USB_EP_ATTR_CONTROL         0
#define USB_EP_ATTR_ISOCHRONOUS     1
#define USB_EP_ATTR_BULK            2
#define USB_EP_ATTR_INTERRUPT       3
/** @} */


/**
 * USB device interface.
 */
typedef struct USBINTERFACE
{
    /** Number of interface. */
    uint8_t     bInterfaceNumber;
    /** Value used to select alternate setting for the interface identified in the prior field. */
    uint8_t     bAlternateSetting;
    /** Number of endpoints used by this interface (excluding endpoint zero). */
    uint8_t     bNumEndpoints;
    /** Pointer to an array of endpoints. */
    PUSBENDPOINT paEndpoints;
    /** Interface class. */
    uint8_t     bInterfaceClass;
    /** Interface subclass. */
    uint8_t     bInterfaceSubClass;
    /** Protocol code. */
    uint8_t     bInterfaceProtocol;
    /** Number of alternate settings. */
    uint8_t     cAlts;
    /** Pointer to an array of alternate interface settings. */
    struct USBINTERFACE *paAlts;
    /** String describing this interface. */
    const char *pszInterface;
    /** String containing the driver name.
     * This is a NULL pointer if the interface is not in use. */
    const char *pszDriver;
} USBINTERFACE;
/** Pointer to a USB device interface description. */
typedef USBINTERFACE *PUSBINTERFACE;
/** Pointer to a const USB device interface description. */
typedef const USBINTERFACE *PCUSBINTERFACE;

/**
 * Device configuration.
 */
typedef struct USBCONFIG
{
    /** Set if this is the active configuration. */
    bool        fActive;
    /** Number of interfaces. */
    uint8_t     bNumInterfaces;
    /** Pointer to an array of interfaces. */
    PUSBINTERFACE paInterfaces;
    /** Configuration number. (For SetConfiguration().) */
    uint8_t     bConfigurationValue;
    /** Configuration description string. */
    const char *pszConfiguration;
    /** Configuration characteristics. */
    uint8_t     bmAttributes;
    /** Maximum power consumption of the USB device in this config.
     * (This field does NOT need shifting like in the USB config descriptor.)  */
    uint16_t    u16MaxPower;
} USBCONFIG;
/** Pointer to a USB configuration. */
typedef USBCONFIG *PUSBCONFIG;
/** Pointer to a const USB configuration. */
typedef const USBCONFIG *PCUSBCONFIG;


/**
 * The USB host device state.
 */
typedef enum USBDEVICESTATE
{
    /** The device is unsupported. */
    USBDEVICESTATE_UNSUPPORTED = 1,
    /** The device is in use by the host. */
    USBDEVICESTATE_USED_BY_HOST,
    /** The device is in use by the host but could perhaps be captured even so. */
    USBDEVICESTATE_USED_BY_HOST_CAPTURABLE,
    /** The device is not used by the host or any guest. */
    USBDEVICESTATE_UNUSED,
    /** The device is held by the proxy for later guest usage. */
    USBDEVICESTATE_HELD_BY_PROXY,
    /** The device in use by a guest. */
    USBDEVICESTATE_USED_BY_GUEST,
    /** The usual 32-bit hack. */
    USBDEVICESTATE_32BIT_HACK = 0x7fffffff
} USBDEVICESTATE;


/**
 * USB host device description.
 * Used for enumeration of USB devices.
 */
typedef struct USBDEVICE
{
    /** USB version number. */
    uint16_t        bcdUSB;
    /** Device class. */
    uint8_t         bDeviceClass;
    /** Device subclass. */
    uint8_t         bDeviceSubClass;
    /** Device protocol */
    uint8_t         bDeviceProtocol;
    /** Vendor ID. */
    uint16_t        idVendor;
    /** Product ID. */
    uint16_t        idProduct;
    /** Revision, integer part. */
    uint16_t        bcdDevice;
    /** Manufacturer string. */
    const char     *pszManufacturer;
    /** Product string. */
    const char     *pszProduct;
    /** Serial number string. */
    const char     *pszSerialNumber;
    /** Serial hash. */
    uint64_t        u64SerialHash;
    /** Number of configurations. */
    uint8_t         bNumConfigurations;
    /** Pointer to an array of configurations. */
    PUSBCONFIG      paConfigurations;
    /** The device state. */
    USBDEVICESTATE  enmState;
    /** The address of the device. */
    const char     *pszAddress;

    /** The USB Bus number. */
    uint8_t         bBus;
    /** The level in topologly for this bus. */
    uint8_t         bLevel;
    /** Device number. */
    uint8_t         bDevNum;
    /** Parent device number. */
    uint8_t         bDevNumParent;
    /** The port number. */
    uint8_t         bPort;
    /** Number of devices on this level. */
    uint8_t         bNumDevices;
    /** Maximum number of children. */
    uint8_t         bMaxChildren;

    /** If linked, this is the pointer to the next device in the list. */
    struct USBDEVICE *pNext;
    /** If linked doubly, this is the pointer to the prev device in the list. */
    struct USBDEVICE *pPrev;
} USBDEVICE;
/** Pointer to a USB device. */
typedef USBDEVICE *PUSBDEVICE;
/** Pointer to a const USB device. */
typedef USBDEVICE *PCUSBDEVICE;


#ifdef VBOX_USB_H_INCL_DESCRIPTORS /* for the time being, since this may easily conflict with system headers */

/**
 * USB device descriptor.
 */
#pragma pack(1)
typedef struct USBDESCHDR
{
    /** The descriptor length. */
    uint8_t         bLength;
    /** The descriptor type. */
    uint8_t         bDescriptorType;
} USBDESCHDR;
#pragma pack()
/** Pointer to an USB descriptor header. */
typedef USBDESCHDR *PUSBDESCHDR;

/** @name Descriptor Type values (bDescriptorType)
 * {@ */
#if !defined(USB_DT_DEVICE) && !defined(USB_DT_ENDPOINT)
# define USB_DT_DEVICE              0x01
# define USB_DT_CONFIG              0x02
# define USB_DT_STRING              0x03
# define USB_DT_INTERFACE           0x04
# define USB_DT_ENDPOINT            0x05

# define USB_DT_HID                 0x21
# define USB_DT_REPORT              0x22
# define USB_DT_PHYSICAL            0x23
# define USB_DT_HUB                 0x29
#endif 
/** @} */


/**
 * USB device descriptor.
 */
#pragma pack(1)
typedef struct USBDEVICEDESC
{
    /** The descriptor length. (Usually sizeof(USBDEVICEDESC).) */
    uint8_t         bLength;
    /** The descriptor type. (USB_DT_DEVICE) */
    uint8_t         bDescriptorType;
    /** USB version number. */
    uint16_t        bcdUSB;
    /** Device class. */
    uint8_t         bDeviceClass;
    /** Device subclass. */
    uint8_t         bDeviceSubClass;
    /** Device protocol */
    uint8_t         bDeviceProtocol;
    /** The max packet size of the default control pipe. */
    uint8_t         bMaxPacketSize0;
    /** Vendor ID. */
    uint16_t        idVendor;
    /** Product ID. */
    uint16_t        idProduct;
    /** Revision, integer part. */
    uint16_t        bcdDevice;
    /** Manufacturer string index. */
    uint8_t         iManufacturer;
    /** Product string index. */
    uint8_t         iProduct;
    /** Serial number string index. */
    uint8_t         iSerialNumber;
    /** Number of configurations. */
    uint8_t         bNumConfigurations;
} USBDEVICEDESC;
#pragma pack()
/** Pointer to an USB device descriptor. */
typedef USBDEVICEDESC *PUSBDEVICEDESC;

/** @name Class codes (bDeviceClass)
 * @{ */
#ifndef USB_HUB_CLASSCODE
# define USB_HUB_CLASSCODE  0x09
#endif
/** @} */

/**
 * USB configuration descriptor.
 */
#pragma pack(1)
typedef struct USBCONFIGDESC
{
    /** The descriptor length. (Usually sizeof(USBCONFIGDESC).) */
    uint8_t         bLength;
    /** The descriptor type. (USB_DT_CONFIG) */
    uint8_t         bDescriptorType;
    /** The length of the configuration descriptor plus all associated descriptors. */
    uint16_t        wTotalLength;
    /** Number of interfaces. */
    uint8_t         bNumInterfaces;
    /** Configuration number. (For SetConfiguration().) */
    uint8_t         bConfigurationValue;
    /** Configuration description string. */
    uint8_t         iConfiguration;
    /** Configuration characteristics. */
    uint8_t         bmAttributes;
    /** Maximum power consumption of the USB device in this config. */
    uint8_t         MaxPower;
} USBCONFIGDESC;
#pragma pack()
/** Pointer to an USB configuration descriptor. */
typedef USBCONFIGDESC *PUSBCONFIGDESC;

#endif /* VBOX_USB_H_INCL_DESCRIPTORS */

__END_DECLS

#endif

