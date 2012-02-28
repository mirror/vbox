/* $Id$ */
/** @file
 * UsbKbd - USB Human Interface Device Emulation, Keyboard.
 */

/*
 * Copyright (C) 2007-2010 Oracle Corporation
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
#define LOG_GROUP   LOG_GROUP_USB_KBD
#include <VBox/vmm/pdmusb.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include "VBoxDD.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @name USB HID string IDs
 * @{ */
#define USBHID_STR_ID_MANUFACTURER  1
#define USBHID_STR_ID_PRODUCT       2
/** @} */

/** @name USB HID specific descriptor types
 * @{ */
#define DT_IF_HID_DESCRIPTOR        0x21
#define DT_IF_HID_REPORT            0x22
/** @} */

/** @name USB HID vendor and product IDs
 * @{ */
#define VBOX_USB_VENDOR             0x80EE
#define USBHID_PID_KEYBOARD         0x0010
/** @} */

/** @name USB HID class specific requests
 * @{ */
#define HID_REQ_GET_REPORT          0x01
#define HID_REQ_GET_IDLE            0x02
#define HID_REQ_SET_REPORT          0x09
#define HID_REQ_SET_IDLE            0x0A
/** @} */

/** @name USB HID additional constants
 * @{ */
/** The highest USB usage code reported by the VBox emulated keyboard */
#define VBOX_USB_MAX_USAGE_CODE     0xE7
/** The size of an array needed to store all USB usage codes */
#define VBOX_USB_USAGE_ARRAY_SIZE   (VBOX_USB_MAX_USAGE_CODE + 1)
#define USBHID_USAGE_ROLL_OVER      1
/** @} */

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * The USB HID request state.
 */
typedef enum USBHIDREQSTATE
{
    /** Invalid status. */
    USBHIDREQSTATE_INVALID = 0,
    /** Ready to receive a new read request. */
    USBHIDREQSTATE_READY,
    /** Have (more) data for the host. */
    USBHIDREQSTATE_DATA_TO_HOST,
    /** Waiting to supply status information to the host. */
    USBHIDREQSTATE_STATUS,
    /** The end of the valid states. */
    USBHIDREQSTATE_END
} USBHIDREQSTATE;


/**
 * Endpoint status data.
 */
typedef struct USBHIDEP
{
    bool                fHalted;
} USBHIDEP;
/** Pointer to the endpoint status. */
typedef USBHIDEP *PUSBHIDEP;


/**
 * A URB queue.
 */
typedef struct USBHIDURBQUEUE
{
    /** The head pointer. */
    PVUSBURB            pHead;
    /** Where to insert the next entry. */
    PVUSBURB           *ppTail;
} USBHIDURBQUEUE;
/** Pointer to a URB queue. */
typedef USBHIDURBQUEUE *PUSBHIDURBQUEUE;
/** Pointer to a const URB queue. */
typedef USBHIDURBQUEUE const *PCUSBHIDURBQUEUE;


/**
 * The USB HID report structure for regular keys.
 */
typedef struct USBHIDK_REPORT
{
    uint8_t     ShiftState;     /**< Modifier keys bitfield */
    uint8_t     Reserved;       /**< Currently unused */
    uint8_t     aKeys[6];       /**< Normal keys */
} USBHIDK_REPORT, *PUSBHIDK_REPORT;

/** Scancode translator state.  */
typedef enum {
    SS_IDLE,    /**< Starting state. */
    SS_EXT,     /**< E0 byte was received. */
    SS_EXT1     /**< E1 byte was received. */
} scan_state_t;

/**
 * The USB HID instance data.
 */
typedef struct USBHID
{
    /** Pointer back to the PDM USB Device instance structure. */
    PPDMUSBINS          pUsbIns;
    /** Critical section protecting the device state. */
    RTCRITSECT          CritSect;

    /** The current configuration.
     * (0 - default, 1 - the one supported configuration, i.e configured.) */
    uint8_t             bConfigurationValue;
    /** USB HID Idle value..
     * (0 - only report state change, !=0 - report in bIdle * 4ms intervals.) */
    uint8_t             bIdle;
    /** Endpoint 0 is the default control pipe, 1 is the dev->host interrupt one. */
    USBHIDEP            aEps[2];
    /** The state of the HID (state machine).*/
    USBHIDREQSTATE      enmState;

    /** State of the scancode translation. */
    scan_state_t        XlatState;

    /** Pending to-host queue.
     * The URBs waiting here are waiting for data to become available.
     */
    USBHIDURBQUEUE      ToHostQueue;

    /** Done queue
     * The URBs stashed here are waiting to be reaped. */
    USBHIDURBQUEUE      DoneQueue;
    /** Signalled when adding an URB to the done queue and fHaveDoneQueueWaiter
     *  is set. */
    RTSEMEVENT          hEvtDoneQueue;
    /** Someone is waiting on the done queue. */
    bool                fHaveDoneQueueWaiter;
    /** If device has pending changes. */
    bool                fHasPendingChanges;
    /** Keypresses which have not yet been reported.  A workaround for the
     * problem of keys being released before the keypress could be reported. */
    uint8_t             abUnreportedKeys[VBOX_USB_USAGE_ARRAY_SIZE];
    /** Currently depressed keys */
    uint8_t             abDepressedKeys[VBOX_USB_USAGE_ARRAY_SIZE];

    /**
     * Keyboard port - LUN#0.
     *
     * @implements  PDMIBASE
     * @implements  PDMIKEYBOARDPORT
     */
    struct
    {
        /** The base interface for the keyboard port. */
        PDMIBASE                            IBase;
        /** The keyboard port base interface. */
        PDMIKEYBOARDPORT                    IPort;

        /** The base interface of the attached keyboard driver. */
        R3PTRTYPE(PPDMIBASE)                pDrvBase;
        /** The keyboard interface of the attached keyboard driver. */
        R3PTRTYPE(PPDMIKEYBOARDCONNECTOR)   pDrv;
    } Lun0;
} USBHID;
/** Pointer to the USB HID instance data. */
typedef USBHID *PUSBHID;

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static const PDMUSBDESCCACHESTRING g_aUsbHidStrings_en_US[] =
{
    { USBHID_STR_ID_MANUFACTURER,   "VirtualBox"    },
    { USBHID_STR_ID_PRODUCT,        "USB Keyboard"  },
};

static const PDMUSBDESCCACHELANG g_aUsbHidLanguages[] =
{
    { 0x0409, RT_ELEMENTS(g_aUsbHidStrings_en_US), g_aUsbHidStrings_en_US }
};

static const VUSBDESCENDPOINTEX g_aUsbHidEndpointDescs[] =
{
    {
        {
            /* .bLength = */            sizeof(VUSBDESCENDPOINT),
            /* .bDescriptorType = */    VUSB_DT_ENDPOINT,
            /* .bEndpointAddress = */   0x81 /* ep=1, in */,
            /* .bmAttributes = */       3 /* interrupt */,
            /* .wMaxPacketSize = */     8,
            /* .bInterval = */          10,
        },
        /* .pvMore = */     NULL,
        /* .pvClass = */    NULL,
        /* .cbClass = */    0
    },
};

/** HID report descriptor. */
static const uint8_t g_UsbHidReportDesc[] =
{
    /* Usage Page */                0x05, 0x01,     /* Generic Desktop */
    /* Usage */                     0x09, 0x06,     /* Keyboard */
    /* Collection */                0xA1, 0x01,     /* Application */
    /* Usage Page */                0x05, 0x07,     /* Keyboard */
    /* Usage Minimum */             0x19, 0xE0,     /* Left Ctrl Key */
    /* Usage Maximum */             0x29, 0xE7,     /* Right GUI Key */
    /* Logical Minimum */           0x15, 0x00,     /* 0 */
    /* Logical Maximum */           0x25, 0x01,     /* 1 */
    /* Report Count */              0x95, 0x08,     /* 8 */
    /* Report Size */               0x75, 0x01,     /* 1 */
    /* Input */                     0x81, 0x02,     /* Data, Value, Absolute, Bit field */
    /* Report Count */              0x95, 0x01,     /* 1 */
    /* Report Size */               0x75, 0x08,     /* 8 (padding bits) */
    /* Input */                     0x81, 0x01,     /* Constant, Array, Absolute, Bit field */
    /* Report Count */              0x95, 0x05,     /* 5 */
    /* Report Size */               0x75, 0x01,     /* 1 */
    /* Usage Page */                0x05, 0x08,     /* LEDs */
    /* Usage Minimum */             0x19, 0x01,     /* Num Lock */
    /* Usage Maximum */             0x29, 0x05,     /* Kana */
    /* Output */                    0x91, 0x02,     /* Data, Value, Absolute, Non-volatile,Bit field */
    /* Report Count */              0x95, 0x01,     /* 1 */
    /* Report Size */               0x75, 0x03,     /* 3 */
    /* Output */                    0x91, 0x01,     /* Constant, Value, Absolute, Non-volatile, Bit field */
    /* Report Count */              0x95, 0x06,     /* 6 */
    /* Report Size */               0x75, 0x08,     /* 8 */
    /* Logical Minimum */           0x15, 0x00,     /* 0 */
    /* Logical Maximum */           0x26, 0xFF,0x00,/* 255 */
    /* Usage Page */                0x05, 0x07,     /* Keyboard */
    /* Usage Minimum */             0x19, 0x00,     /* 0 */
    /* Usage Maximum */             0x29, 0xFF,     /* 255 */
    /* Input */                     0x81, 0x00,     /* Data, Array, Absolute, Bit field */
    /* End Collection */            0xC0,
};

/** Additional HID class interface descriptor. */
static const uint8_t g_UsbHidIfHidDesc[] =
{
    /* .bLength = */                0x09,
    /* .bDescriptorType = */        0x21,       /* HID */
    /* .bcdHID = */                 0x10, 0x01, /* 1.1 */
    /* .bCountryCode = */           0x0D,       /* International (ISO) */
    /* .bNumDescriptors = */        1,
    /* .bDescriptorType = */        0x22,       /* Report */
    /* .wDescriptorLength = */      sizeof(g_UsbHidReportDesc), 0x00
};

static const VUSBDESCINTERFACEEX g_UsbHidInterfaceDesc =
{
    {
        /* .bLength = */                sizeof(VUSBDESCINTERFACE),
        /* .bDescriptorType = */        VUSB_DT_INTERFACE,
        /* .bInterfaceNumber = */       0,
        /* .bAlternateSetting = */      0,
        /* .bNumEndpoints = */          1,
        /* .bInterfaceClass = */        3 /* HID */,
        /* .bInterfaceSubClass = */     1 /* Boot Interface */,
        /* .bInterfaceProtocol = */     1 /* Keyboard */,
        /* .iInterface = */             0
    },
    /* .pvMore = */     NULL,
    /* .pvClass = */    &g_UsbHidIfHidDesc,
    /* .cbClass = */    sizeof(g_UsbHidIfHidDesc),
    &g_aUsbHidEndpointDescs[0]
};

static const VUSBINTERFACE g_aUsbHidInterfaces[] =
{
    { &g_UsbHidInterfaceDesc, /* .cSettings = */ 1 },
};

static const VUSBDESCCONFIGEX g_UsbHidConfigDesc =
{
    {
        /* .bLength = */            sizeof(VUSBDESCCONFIG),
        /* .bDescriptorType = */    VUSB_DT_CONFIG,
        /* .wTotalLength = */       0 /* recalculated on read */,
        /* .bNumInterfaces = */     RT_ELEMENTS(g_aUsbHidInterfaces),
        /* .bConfigurationValue =*/ 1,
        /* .iConfiguration = */     0,
        /* .bmAttributes = */       RT_BIT(7),
        /* .MaxPower = */           50 /* 100mA */
    },
    NULL,                           /* pvMore */
    &g_aUsbHidInterfaces[0],
    NULL                            /* pvOriginal */
};

static const VUSBDESCDEVICE g_UsbHidDeviceDesc =
{
    /* .bLength = */                sizeof(g_UsbHidDeviceDesc),
    /* .bDescriptorType = */        VUSB_DT_DEVICE,
    /* .bcdUsb = */                 0x110,  /* 1.1 */
    /* .bDeviceClass = */           0 /* Class specified in the interface desc. */,
    /* .bDeviceSubClass = */        0 /* Subclass specified in the interface desc. */,
    /* .bDeviceProtocol = */        0 /* Protocol specified in the interface desc. */,
    /* .bMaxPacketSize0 = */        8,
    /* .idVendor = */               VBOX_USB_VENDOR,
    /* .idProduct = */              USBHID_PID_KEYBOARD,
    /* .bcdDevice = */              0x0100, /* 1.0 */
    /* .iManufacturer = */          USBHID_STR_ID_MANUFACTURER,
    /* .iProduct = */               USBHID_STR_ID_PRODUCT,
    /* .iSerialNumber = */          0,
    /* .bNumConfigurations = */     1
};

static const PDMUSBDESCCACHE g_UsbHidDescCache =
{
    /* .pDevice = */                &g_UsbHidDeviceDesc,
    /* .paConfigs = */              &g_UsbHidConfigDesc,
    /* .paLanguages = */            g_aUsbHidLanguages,
    /* .cLanguages = */             RT_ELEMENTS(g_aUsbHidLanguages),
    /* .fUseCachedDescriptors = */  true,
    /* .fUseCachedStringsDescriptors = */ true
};


/*
 * Because of historical reasons and poor design, VirtualBox internally uses BIOS
 * PC/XT style scan codes to represent keyboard events. Each key press and release is
 * represented as a stream of bytes, typically only one byte but up to four-byte
 * sequences are possible. In the typical case, the GUI front end generates the stream
 * of scan codes which we need to translate back to a single up/down event.
 *
 * This function could possibly live somewhere else.
 */

/** Lookup table for converting PC/XT scan codes to USB HID usage codes. */
static uint8_t aScancode2Hid[] =
{
    0x00, 0x29, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, /* 00-07 */
    0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x2b, /* 08-1F */
    0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, /* 10-17 */
    0x12, 0x13, 0x2f, 0x30, 0x28, 0xe0, 0x04, 0x16, /* 18-1F */
    0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, /* 20-27 */
    0x34, 0x35, 0xe1, 0x31, 0x1d, 0x1b, 0x06, 0x19, /* 28-2F */
    0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0xe5, 0x55, /* 30-37 */
    0xe2, 0x2c, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, /* 38-3F */
    0x3f, 0x40, 0x41, 0x42, 0x43, 0x53, 0x47, 0x5f, /* 40-47 */
    0x60, 0x61, 0x56, 0x5c, 0x5d, 0x5e, 0x57, 0x59, /* 48-4F */
    0x5a, 0x5b, 0x62, 0x63, 0x00, 0x00, 0x64, 0x44, /* 50-57 */
    0x45, 0x67, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00, /* 58-5F */
    0x00, 0x00, 0x00, 0x00, 0x68, 0x69, 0x6a, 0x6b, /* 60-67 */
    0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x00, /* 68-6F */
    0x88, 0x91, 0x90, 0x87, 0x00, 0x00, 0x00, 0x00, /* 70-77 */
    0x00, 0x8a, 0x00, 0x8b, 0x00, 0x89, 0x85, 0x00  /* 78-7F */
};

/** Lookup table for extended scancodes (arrow keys etc.). */
static uint8_t aExtScan2Hid[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 00-07 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 08-1F */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 10-17 */
    0x00, 0x00, 0x00, 0x00, 0x58, 0xe4, 0x00, 0x00, /* 18-1F */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 20-27 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 28-2F */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x00, 0x46, /* 30-37 */
                   /* Sun-specific keys.  Most of the XT codes are made up */
    0xe6, 0x00, 0x00, 0x75, 0x76, 0x77, 0xA3, 0x78, /* 38-3F */
    0x80, 0x81, 0x82, 0x79, 0x00, 0x48, 0x00, 0x4a, /* 40-47 */
    0x52, 0x4b, 0x00, 0x50, 0x00, 0x4f, 0x00, 0x4d, /* 48-4F */
    0x51, 0x4e, 0x49, 0x4c, 0x00, 0x00, 0x00, 0x00, /* 50-57 */
    0x00, 0x00, 0x00, 0xe3, 0xe7, 0x65, 0x66, 0x00, /* 58-5F */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 60-67 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 68-6F */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 70-77 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* 78-7F */
};

/**
 * Convert a PC scan code to a USB HID usage byte.
 *
 * @param state         Current state of the translator (scan_state_t).
 * @param scanCode      Incoming scan code.
 * @param pUsage        Pointer to usage; high bit set for key up events. The
 *                      contents are only valid if returned state is SS_IDLE.
 *
 * @return scan_state_t New state of the translator.
 */
static scan_state_t ScancodeToHidUsage(scan_state_t state, uint8_t scanCode, uint32_t *pUsage)
{
    uint32_t    keyUp;
    uint8_t     usage;

    Assert(pUsage);

    /* Isolate the scan code and key break flag. */
    keyUp = (scanCode & 0x80) << 24;

    switch (state) {
    case SS_IDLE:
        if (scanCode == 0xE0) {
            state = SS_EXT;
        } else if (scanCode == 0xE1) {
            state = SS_EXT1;
        } else {
            usage = aScancode2Hid[scanCode & 0x7F];
            *pUsage = usage | keyUp;
            /* Remain in SS_IDLE state. */
        }
        break;
    case SS_EXT:
        usage = aExtScan2Hid[scanCode & 0x7F];
        *pUsage = usage | keyUp;
        state = SS_IDLE;
        break;
    case SS_EXT1:
        Assert(0);  //@todo - sort out the Pause key
        *pUsage = 0;
        state = SS_IDLE;
        break;
    }
    return state;
}

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


/**
 * Initializes an URB queue.
 *
 * @param   pQueue              The URB queue.
 */
static void usbHidQueueInit(PUSBHIDURBQUEUE pQueue)
{
    pQueue->pHead = NULL;
    pQueue->ppTail = &pQueue->pHead;
}

/**
 * Inserts an URB at the end of the queue.
 *
 * @param   pQueue              The URB queue.
 * @param   pUrb                The URB to insert.
 */
DECLINLINE(void) usbHidQueueAddTail(PUSBHIDURBQUEUE pQueue, PVUSBURB pUrb)
{
    pUrb->Dev.pNext = NULL;
    *pQueue->ppTail = pUrb;
    pQueue->ppTail  = &pUrb->Dev.pNext;
}


/**
 * Unlinks the head of the queue and returns it.
 *
 * @returns The head entry.
 * @param   pQueue              The URB queue.
 */
DECLINLINE(PVUSBURB) usbHidQueueRemoveHead(PUSBHIDURBQUEUE pQueue)
{
    PVUSBURB pUrb = pQueue->pHead;
    if (pUrb)
    {
        PVUSBURB pNext = pUrb->Dev.pNext;
        pQueue->pHead = pNext;
        if (!pNext)
            pQueue->ppTail = &pQueue->pHead;
        else
            pUrb->Dev.pNext = NULL;
    }
    return pUrb;
}


/**
 * Removes an URB from anywhere in the queue.
 *
 * @returns true if found, false if not.
 * @param   pQueue              The URB queue.
 * @param   pUrb                The URB to remove.
 */
DECLINLINE(bool) usbHidQueueRemove(PUSBHIDURBQUEUE pQueue, PVUSBURB pUrb)
{
    PVUSBURB pCur = pQueue->pHead;
    if (pCur == pUrb)
        pQueue->pHead = pUrb->Dev.pNext;
    else
    {
        while (pCur)
        {
            if (pCur->Dev.pNext == pUrb)
            {
                pCur->Dev.pNext = pUrb->Dev.pNext;
                break;
            }
            pCur = pCur->Dev.pNext;
        }
        if (!pCur)
            return false;
    }
    if (!pUrb->Dev.pNext)
        pQueue->ppTail = &pQueue->pHead;
    return true;
}


/**
 * Checks if the queue is empty or not.
 *
 * @returns true if it is, false if it isn't.
 * @param   pQueue              The URB queue.
 */
DECLINLINE(bool) usbHidQueueIsEmpty(PCUSBHIDURBQUEUE pQueue)
{
    return pQueue->pHead == NULL;
}


/**
 * Links an URB into the done queue.
 *
 * @param   pThis               The HID instance.
 * @param   pUrb                The URB.
 */
static void usbHidLinkDone(PUSBHID pThis, PVUSBURB pUrb)
{
    usbHidQueueAddTail(&pThis->DoneQueue, pUrb);

    if (pThis->fHaveDoneQueueWaiter)
    {
        int rc = RTSemEventSignal(pThis->hEvtDoneQueue);
        AssertRC(rc);
    }
}



/**
 * Completes the URB with a stalled state, halting the pipe.
 */
static int usbHidCompleteStall(PUSBHID pThis, PUSBHIDEP pEp, PVUSBURB pUrb, const char *pszWhy)
{
    Log(("usbHidCompleteStall/#%u: pUrb=%p:%s: %s\n", pThis->pUsbIns->iInstance, pUrb, pUrb->pszDesc, pszWhy));

    pUrb->enmStatus = VUSBSTATUS_STALL;

    /** @todo figure out if the stall is global or pipe-specific or both. */
    if (pEp)
        pEp->fHalted = true;
    else
    {
        pThis->aEps[0].fHalted = true;
        pThis->aEps[1].fHalted = true;
    }

    usbHidLinkDone(pThis, pUrb);
    return VINF_SUCCESS;
}


/**
 * Completes the URB with a OK state.
 */
static int usbHidCompleteOk(PUSBHID pThis, PVUSBURB pUrb, size_t cbData)
{
    Log(("usbHidCompleteOk/#%u: pUrb=%p:%s cbData=%#zx\n", pThis->pUsbIns->iInstance, pUrb, pUrb->pszDesc, cbData));

    pUrb->enmStatus = VUSBSTATUS_OK;
    pUrb->cbData    = (uint32_t)cbData;

    usbHidLinkDone(pThis, pUrb);
    return VINF_SUCCESS;
}


/**
 * Reset worker for usbHidUsbReset, usbHidUsbSetConfiguration and
 * usbHidHandleDefaultPipe.
 *
 * @returns VBox status code.
 * @param   pThis               The HID instance.
 * @param   pUrb                Set when usbHidHandleDefaultPipe is the
 *                              caller.
 * @param   fSetConfig          Set when usbHidUsbSetConfiguration is the
 *                              caller.
 */
static int usbHidResetWorker(PUSBHID pThis, PVUSBURB pUrb, bool fSetConfig)
{
    /*
     * Deactivate the keyboard.
     */
    pThis->Lun0.pDrv->pfnSetActive(pThis->Lun0.pDrv, false);

    /*
     * Reset the device state.
     */
    pThis->enmState = USBHIDREQSTATE_READY;
    pThis->bIdle = 0;
    pThis->fHasPendingChanges = false;

    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aEps); i++)
        pThis->aEps[i].fHalted = false;

    if (!pUrb && !fSetConfig) /* (only device reset) */
        pThis->bConfigurationValue = 0; /* default */

    /*
     * Ditch all pending URBs.
     */
    PVUSBURB pCurUrb;
    while ((pCurUrb = usbHidQueueRemoveHead(&pThis->ToHostQueue)) != NULL)
    {
        pCurUrb->enmStatus = VUSBSTATUS_CRC;
        usbHidLinkDone(pThis, pCurUrb);
    }

    if (pUrb)
        return usbHidCompleteOk(pThis, pUrb, 0);
    return VINF_SUCCESS;
}

#ifdef DEBUG
# define HEX_DIGIT(x) (((x) < 0xa) ? ((x) + '0') : ((x) - 0xa + 'a'))
static void usbHidComputePressed(PUSBHIDK_REPORT pReport, char* pszBuf, unsigned cbBuf)
{
    unsigned offBuf = 0;
    unsigned i;
    for (i = 0; i < RT_ELEMENTS(pReport->aKeys); ++i)
    {
        uint8_t uCode = pReport->aKeys[i];
        if (uCode != 0)
        {
            if (offBuf + 4 >= cbBuf)
                break;
            pszBuf[offBuf++] = HEX_DIGIT(uCode >> 4);
            pszBuf[offBuf++] = HEX_DIGIT(uCode & 0xf);
            pszBuf[offBuf++] = ' ';
        }
    }
    pszBuf[offBuf++] = '\0';
}
# undef HEX_DIGIT
#endif

/**
 * Returns true if the usage code corresponds to a keyboard modifier key
 * (left or right ctrl, shift, alt or GUI).  The usage codes for these keys
 * are the range 0xe0 to 0xe7.
 */
static bool usbHidUsageCodeIsModifier(uint8_t u8Usage)
{
    return u8Usage >= 0xe0 && u8Usage <= 0xe7;
}

/**
 * Convert a USB HID usage code to a keyboard modifier flag.  The arithmetic
 * is simple: the modifier keys have usage codes from 0xe0 to 0xe7, and the
 * lower nibble is the bit number of the flag.
 */
static uint8_t usbHidModifierToFlag(uint8_t u8Usage)
{
    Assert(usbHidUsageCodeIsModifier(u8Usage));
    return RT_BIT(u8Usage & 0xf);
}

/**
 * Create a USB HID keyboard report based on a vector of keys which have been
 * pressed since the last report was created (so that we don't miss keys that
 * are only pressed briefly) and a vector of currently depressed keys.
 * The keys in the report aKeys array are in increasing order (important for
 * the test case).
 */
static int usbHidFillReport(PUSBHIDK_REPORT pReport,
                            uint8_t *pabUnreportedKeys,
                            uint8_t *pabDepressedKeys)
{
    int rc = false;
    unsigned iBuf = 0;
    RT_ZERO(*pReport);
    for (unsigned iKey = 0; iKey < VBOX_USB_USAGE_ARRAY_SIZE; ++iKey)
    {
        AssertReturn(iBuf <= RT_ELEMENTS(pReport->aKeys),
                     VERR_INTERNAL_ERROR);
        if (pabUnreportedKeys[iKey] || pabDepressedKeys[iKey])
        {
            if (usbHidUsageCodeIsModifier(iKey))
                pReport->ShiftState |= usbHidModifierToFlag(iKey);
            else if (iBuf == RT_ELEMENTS(pReport->aKeys))
            {
                /* The USB HID spec says that the entire vector should be
                 * set to ErrorRollOver on overflow.  We don't mind if this
                 * path is taken several times for one report. */
                for (unsigned iBuf2 = 0;
                     iBuf2 < RT_ELEMENTS(pReport->aKeys); ++iBuf2)
                    pReport->aKeys[iBuf2] = USBHID_USAGE_ROLL_OVER;
            }
            else
            {
                pReport->aKeys[iBuf] = iKey;
                ++iBuf;
                /* More Korean keyboard hackery: Give the caller a hint that
                 * a key release event needs reporting.
                 */
                if (iKey == 0x90 || iKey == 0x91)
                    rc = true;
            }
            pabUnreportedKeys[iKey] = 0;
        }
    }
    return rc;
}

#ifdef DEBUG
/** Test data for testing usbHidFillReport().  The format is:
 *   - Unreported keys (zero terminated array)
 *   - Depressed keys (zero terminated array)
 *   - Expected shift state in the report (single byte inside array)
 *   - Expected keys buffer contents (array of six bytes)
 */
static const uint8_t testUsbHidFillReportData[][4][10] = {
    /* Just unreported, no modifiers */
    {{4, 9, 0}, {0}, {0}, {4, 9, 0, 0, 0, 0}},
    /* Just unreported, one modifier */
    {{4, 9, 0xe2, 0}, {0}, {4}, {4, 9, 0, 0, 0, 0}},
    /* Just unreported, two modifiers */
    {{4, 9, 0xe2, 0xe4, 0}, {0}, {20}, {4, 9, 0, 0, 0, 0}},
    /* Just depressed, no modifiers */
    {{0}, {7, 20, 0}, {0}, {7, 20, 0, 0, 0, 0}},
    /* Just depressed, one modifier */
    {{0}, {7, 20, 0xe3, 0}, {8}, {7, 20, 0, 0, 0, 0}},
    /* Just depressed, two modifiers */
    {{0}, {7, 20, 0xe3, 0xe6, 0}, {72}, {7, 20, 0, 0, 0, 0}},
    /* Unreported and depressed, no overlap, no modifiers */
    {{5, 10, 0}, {8, 21, 0}, {0}, {5, 8, 10, 21, 0, 0}},
    /* Unreported and depressed, one overlap, no modifiers */
    {{5, 10, 0}, {8, 10, 21, 0}, {0}, {5, 8, 10, 21, 0, 0}},
    /* Unreported and depressed, no overlap, non-overlapping modifiers */
    {{5, 10, 0xe2, 0xe4, 0}, {8, 21, 0xe3, 0xe6, 0}, {92},
           {5, 8, 10, 21, 0, 0}},
    /* Unreported and depressed, one overlap, non-overlapping modifiers */
    {{5, 10, 21, 0xe2, 0xe4, 0}, {8, 21, 0xe3, 0xe6, 0}, {92},
           {5, 8, 10, 21, 0, 0}},
    /* Unreported and depressed, no overlap, overlapping modifiers */
    {{5, 10, 0xe2, 0xe4, 0}, {8, 21, 0xe3, 0xe4, 0}, {28},
           {5, 8, 10, 21, 0, 0}},
    /* Unreported and depressed, one overlap, overlapping modifiers */
    {{5, 10, 0xe2, 0xe4, 0}, {5, 8, 21, 0xe3, 0xe4, 0}, {28},
           {5, 8, 10, 21, 0, 0}},
    /* Just too many unreported, no modifiers */
    {{4, 9, 11, 12, 16, 18, 20, 0}, {0}, {0}, {1, 1, 1, 1, 1, 1}},
    /* Just too many unreported, two modifiers */
    {{4, 9, 11, 12, 16, 18, 20, 0xe2, 0xe4, 0}, {0}, {20},
           {1, 1, 1, 1, 1, 1}},
    /* Just too many depressed, no modifiers */
    {{0}, {7, 20, 22, 25, 27, 29, 34, 0}, {0}, {1, 1, 1, 1, 1, 1}},
    /* Just too many depressed, two modifiers */
    {{0}, {7, 20, 22, 25, 27, 29, 34, 0xe3, 0xe5, 0}, {40},
           {1, 1, 1, 1, 1, 1}},
    /* Too many unreported and depressed, no overlap, no modifiers */
    {{5, 10, 12, 13, 0}, {8, 9, 21, 0}, {0}, {1, 1, 1, 1, 1, 1}},
    /* Eight unreported and depressed total, one overlap, no modifiers */
    {{5, 10, 12, 13, 0}, {8, 10, 21, 22, 0}, {0}, {1, 1, 1, 1, 1, 1}},
    /* Seven unreported and depressed total, one overlap, no modifiers */
    {{5, 10, 12, 13, 0}, {8, 10, 21, 0}, {0}, {5, 8, 10, 12, 13, 21}},
    /* Too many unreported and depressed, no overlap, two modifiers */
    {{5, 10, 12, 13, 0xe2, 0}, {8, 9, 21, 0xe4, 0}, {20},
           {1, 1, 1, 1, 1, 1}},
    /* Eight unreported and depressed total, one overlap, two modifiers */
    {{5, 10, 12, 13, 0xe1, 0}, {8, 10, 21, 22, 0xe2, 0}, {6},
           {1, 1, 1, 1, 1, 1}},
    /* Seven unreported and depressed total, one overlap, two modifiers */
    {{5, 10, 12, 13, 0xe2, 0}, {8, 10, 21, 0xe3, 0}, {12},
           {5, 8, 10, 12, 13, 21}}
};

/** Test case for usbHidFillReport() */
class testUsbHidFillReport
{
    USBHIDK_REPORT mReport;
    uint8_t mabUnreportedKeys[VBOX_USB_USAGE_ARRAY_SIZE];
    uint8_t mabDepressedKeys[VBOX_USB_USAGE_ARRAY_SIZE];
    const uint8_t (*mTests)[4][10];

    void doTest(unsigned cTest, const uint8_t *paiUnreportedKeys,
                const uint8_t *paiDepressedKeys, uint8_t aExpShiftState,
                const uint8_t *pabExpKeys)
    {
        RT_ZERO(mReport);
        RT_ZERO(mabUnreportedKeys);
        RT_ZERO(mabDepressedKeys);
        for (unsigned i = 0; paiUnreportedKeys[i] != 0; ++i)
            mabUnreportedKeys[paiUnreportedKeys[i]] = 1;
        for (unsigned i = 0; paiDepressedKeys[i] != 0; ++i)
            mabUnreportedKeys[paiDepressedKeys[i]] = 1;
        int rc = usbHidFillReport(&mReport, mabUnreportedKeys, mabDepressedKeys);
        AssertMsgRC(rc, ("test %u\n", cTest));
        AssertMsg(mReport.ShiftState == aExpShiftState, ("test %u\n", cTest));
        for (unsigned i = 0; i < RT_ELEMENTS(mReport.aKeys); ++i)
            AssertMsg(mReport.aKeys[i] == pabExpKeys[i], ("test %u\n", cTest));
    }

public:
    testUsbHidFillReport(void) : mTests(&testUsbHidFillReportData[0])
    {
        for (unsigned i = 0; i < RT_ELEMENTS(testUsbHidFillReportData); ++i)
            doTest(i, mTests[i][0], mTests[i][1], mTests[i][2][0],
                   mTests[i][3]);
    }
};

static testUsbHidFillReport gsTestUsbHidFillReport;
#endif

/**
 * Sends a state report to the host if there is a pending URB.
 */
static int usbHidSendReport(PUSBHID pThis)
{
    PVUSBURB pUrb = usbHidQueueRemoveHead(&pThis->ToHostQueue);
    if (pUrb)
    {
        PUSBHIDK_REPORT pReport = (PUSBHIDK_REPORT)&pUrb->abData[0];

        int again = usbHidFillReport(pReport, pThis->abUnreportedKeys,
                                      pThis->abDepressedKeys);
        if (again)
            pThis->fHasPendingChanges = true;
        else
            pThis->fHasPendingChanges = false;
        return usbHidCompleteOk(pThis, pUrb, sizeof(*pReport));
    }
    else
    {
        Log2(("No available URB for USB kbd\n"));
        pThis->fHasPendingChanges = true;
    }
    return VINF_EOF;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) usbHidKeyboardQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PUSBHID pThis = RT_FROM_MEMBER(pInterface, USBHID, Lun0.IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->Lun0.IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIKEYBOARDPORT, &pThis->Lun0.IPort);
    return NULL;
}

/**
 * Keyboard event handler.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the keyboard port interface (KBDState::Keyboard.IPort).
 * @param   u8KeyCode       The keycode.
 */
static DECLCALLBACK(int) usbHidKeyboardPutEvent(PPDMIKEYBOARDPORT pInterface, uint8_t u8KeyCode)
{
    PUSBHID pThis = RT_FROM_MEMBER(pInterface, USBHID, Lun0.IPort);
    uint32_t    u32Usage = 0;
    uint8_t     u8HidCode;
    int         fKeyDown;
    bool        fHaveEvent = true;

    RTCritSectEnter(&pThis->CritSect);

    pThis->XlatState = ScancodeToHidUsage(pThis->XlatState, u8KeyCode, &u32Usage);

    if (pThis->XlatState == SS_IDLE)
    {
        /* The usage code is valid. */
        fKeyDown = !(u32Usage & 0x80000000);
        u8HidCode = u32Usage & 0xFF;
        AssertReturn(u8HidCode <= VBOX_USB_MAX_USAGE_CODE, VERR_INTERNAL_ERROR);

        LogFlowFunc(("key %s: 0x%x->0x%x\n",
                        fKeyDown ? "down" : "up", u8KeyCode, u8HidCode));

        if (fKeyDown)
        {
            /* Due to host key repeat, we can get key events for keys which are
             * already depressed. */
            if (!pThis->abDepressedKeys[u8HidCode])
                pThis->abUnreportedKeys[u8HidCode] = 1;
            else
                fHaveEvent = false;
            pThis->abDepressedKeys[u8HidCode] = 1;
        }
        else
        {
            /* For stupid Korean keyboards, we have to fake a key up/down sequence
             * because they only send break codes for Hangul/Hanja keys.
             */
            if (u8HidCode == 0x90 || u8HidCode == 0x91)
                pThis->abUnreportedKeys[u8HidCode] = 1;
            pThis->abDepressedKeys[u8HidCode] = 0;
        }


        /* Send a report if the host is already waiting for it. */
        if (fHaveEvent)
            usbHidSendReport(pThis);
    }

    RTCritSectLeave(&pThis->CritSect);

    return VINF_SUCCESS;
}

/**
 * @copydoc PDMUSBREG::pfnUrbReap
 */
static DECLCALLBACK(PVUSBURB) usbHidUrbReap(PPDMUSBINS pUsbIns, RTMSINTERVAL cMillies)
{
    PUSBHID pThis = PDMINS_2_DATA(pUsbIns, PUSBHID);
    //LogFlow(("usbHidUrbReap/#%u: cMillies=%u\n", pUsbIns->iInstance, cMillies));

    RTCritSectEnter(&pThis->CritSect);

    PVUSBURB pUrb = usbHidQueueRemoveHead(&pThis->DoneQueue);
    if (!pUrb && cMillies)
    {
        /* Wait */
        pThis->fHaveDoneQueueWaiter = true;
        RTCritSectLeave(&pThis->CritSect);

        RTSemEventWait(pThis->hEvtDoneQueue, cMillies);

        RTCritSectEnter(&pThis->CritSect);
        pThis->fHaveDoneQueueWaiter = false;

        pUrb = usbHidQueueRemoveHead(&pThis->DoneQueue);
    }

    RTCritSectLeave(&pThis->CritSect);

    if (pUrb)
        Log(("usbHidUrbReap/#%u: pUrb=%p:%s\n", pUsbIns->iInstance, pUrb, pUrb->pszDesc));
    return pUrb;
}


/**
 * @copydoc PDMUSBREG::pfnUrbCancel
 */
static DECLCALLBACK(int) usbHidUrbCancel(PPDMUSBINS pUsbIns, PVUSBURB pUrb)
{
    PUSBHID pThis = PDMINS_2_DATA(pUsbIns, PUSBHID);
    LogFlow(("usbHidUrbCancel/#%u: pUrb=%p:%s\n", pUsbIns->iInstance, pUrb, pUrb->pszDesc));
    RTCritSectEnter(&pThis->CritSect);

    /*
     * Remove the URB from the to-host queue and move it onto the done queue.
     */
    if (usbHidQueueRemove(&pThis->ToHostQueue, pUrb))
        usbHidLinkDone(pThis, pUrb);

    RTCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}


/**
 * Handles request sent to the inbound (device to host) interrupt pipe. This is
 * rather different from bulk requests because an interrupt read URB may complete
 * after arbitrarily long time.
 */
static int usbHidHandleIntrDevToHost(PUSBHID pThis, PUSBHIDEP pEp, PVUSBURB pUrb)
{
    /*
     * Stall the request if the pipe is halted.
     */
    if (RT_UNLIKELY(pEp->fHalted))
        return usbHidCompleteStall(pThis, NULL, pUrb, "Halted pipe");

    /*
     * Deal with the URB according to the state.
     */
    switch (pThis->enmState)
    {
        /*
         * We've data left to transfer to the host.
         */
        case USBHIDREQSTATE_DATA_TO_HOST:
        {
            AssertFailed();
            Log(("usbHidHandleIntrDevToHost: Entering STATUS\n"));
            return usbHidCompleteOk(pThis, pUrb, 0);
        }

        /*
         * Status transfer.
         */
        case USBHIDREQSTATE_STATUS:
        {
            AssertFailed();
            Log(("usbHidHandleIntrDevToHost: Entering READY\n"));
            pThis->enmState = USBHIDREQSTATE_READY;
            return usbHidCompleteOk(pThis, pUrb, 0);
        }

        case USBHIDREQSTATE_READY:
            usbHidQueueAddTail(&pThis->ToHostQueue, pUrb);
            /* If device was not set idle, sent the current report right away. */
            if (pThis->bIdle != 0 || pThis->fHasPendingChanges)
                usbHidSendReport(pThis);
            LogFlow(("usbHidHandleIntrDevToHost: Sent report via %p:%s\n", pUrb, pUrb->pszDesc));
            return VINF_SUCCESS;

        /*
         * Bad states, stall.
         */
        default:
            Log(("usbHidHandleIntrDevToHost: enmState=%d cbData=%#x\n", pThis->enmState, pUrb->cbData));
            return usbHidCompleteStall(pThis, NULL, pUrb, "Really bad state (D2H)!");
    }
}


/**
 * Handles request sent to the default control pipe.
 */
static int usbHidHandleDefaultPipe(PUSBHID pThis, PUSBHIDEP pEp, PVUSBURB pUrb)
{
    PVUSBSETUP pSetup = (PVUSBSETUP)&pUrb->abData[0];
    LogFlow(("usbHidHandleDefaultPipe: cbData=%d\n", pUrb->cbData));

    AssertReturn(pUrb->cbData >= sizeof(*pSetup), VERR_VUSB_FAILED_TO_QUEUE_URB);

    if ((pSetup->bmRequestType & VUSB_REQ_MASK) == VUSB_REQ_STANDARD)
    {
        switch (pSetup->bRequest)
        {
            case VUSB_REQ_GET_DESCRIPTOR:
            {
                switch (pSetup->bmRequestType)
                {
                    case VUSB_TO_DEVICE | VUSB_REQ_STANDARD | VUSB_DIR_TO_HOST:
                    {
                        switch (pSetup->wValue >> 8)
                        {
                            case VUSB_DT_STRING:
                                Log(("usbHid: GET_DESCRIPTOR DT_STRING wValue=%#x wIndex=%#x\n", pSetup->wValue, pSetup->wIndex));
                                break;
                            default:
                                Log(("usbHid: GET_DESCRIPTOR, huh? wValue=%#x wIndex=%#x\n", pSetup->wValue, pSetup->wIndex));
                                break;
                        }
                        break;
                    }

                    case VUSB_TO_INTERFACE | VUSB_REQ_STANDARD | VUSB_DIR_TO_HOST:
                    {
                        switch (pSetup->wValue >> 8)
                        {
                            case DT_IF_HID_DESCRIPTOR:
                            {
                                uint32_t    cbCopy;

                                /* Returned data is written after the setup message. */
                                cbCopy = pUrb->cbData - sizeof(*pSetup);
                                cbCopy = RT_MIN(cbCopy, sizeof(g_UsbHidIfHidDesc));
                                Log(("usbHidKbd: GET_DESCRIPTOR DT_IF_HID_DESCRIPTOR wValue=%#x wIndex=%#x cbCopy=%#x\n", pSetup->wValue, pSetup->wIndex, cbCopy));
                                memcpy(&pUrb->abData[sizeof(*pSetup)], &g_UsbHidIfHidDesc, cbCopy);
                                return usbHidCompleteOk(pThis, pUrb, cbCopy + sizeof(*pSetup));
                            }

                            case DT_IF_HID_REPORT:
                            {
                                uint32_t    cbCopy;

                                /* Returned data is written after the setup message. */
                                cbCopy = pUrb->cbData - sizeof(*pSetup);
                                cbCopy = RT_MIN(cbCopy, sizeof(g_UsbHidReportDesc));
                                Log(("usbHid: GET_DESCRIPTOR DT_IF_HID_REPORT wValue=%#x wIndex=%#x cbCopy=%#x\n", pSetup->wValue, pSetup->wIndex, cbCopy));
                                memcpy(&pUrb->abData[sizeof(*pSetup)], &g_UsbHidReportDesc, cbCopy);
                                return usbHidCompleteOk(pThis, pUrb, cbCopy + sizeof(*pSetup));
                            }

                            default:
                                Log(("usbHid: GET_DESCRIPTOR, huh? wValue=%#x wIndex=%#x\n", pSetup->wValue, pSetup->wIndex));
                                break;
                        }
                        break;
                    }

                    default:
                        Log(("usbHid: Bad GET_DESCRIPTOR req: bmRequestType=%#x\n", pSetup->bmRequestType));
                        return usbHidCompleteStall(pThis, pEp, pUrb, "Bad GET_DESCRIPTOR");
                }
                break;
            }

            case VUSB_REQ_GET_STATUS:
            {
                uint16_t    wRet = 0;

                if (pSetup->wLength != 2)
                {
                    Log(("usbHid: Bad GET_STATUS req: wLength=%#x\n", pSetup->wLength));
                    break;
                }
                Assert(pSetup->wValue == 0);
                switch (pSetup->bmRequestType)
                {
                    case VUSB_TO_DEVICE | VUSB_REQ_STANDARD | VUSB_DIR_TO_HOST:
                    {
                        Assert(pSetup->wIndex == 0);
                        Log(("usbHid: GET_STATUS (device)\n"));
                        wRet = 0;   /* Not self-powered, no remote wakeup. */
                        memcpy(&pUrb->abData[sizeof(*pSetup)], &wRet, sizeof(wRet));
                        return usbHidCompleteOk(pThis, pUrb, sizeof(wRet) + sizeof(*pSetup));
                    }

                    case VUSB_TO_INTERFACE | VUSB_REQ_STANDARD | VUSB_DIR_TO_HOST:
                    {
                        if (pSetup->wIndex == 0)
                        {
                            memcpy(&pUrb->abData[sizeof(*pSetup)], &wRet, sizeof(wRet));
                            return usbHidCompleteOk(pThis, pUrb, sizeof(wRet) + sizeof(*pSetup));
                        }
                        else
                        {
                            Log(("usbHid: GET_STATUS (interface) invalid, wIndex=%#x\n", pSetup->wIndex));
                        }
                        break;
                    }

                    case VUSB_TO_ENDPOINT | VUSB_REQ_STANDARD | VUSB_DIR_TO_HOST:
                    {
                        if (pSetup->wIndex < RT_ELEMENTS(pThis->aEps))
                        {
                            wRet = pThis->aEps[pSetup->wIndex].fHalted ? 1 : 0;
                            memcpy(&pUrb->abData[sizeof(*pSetup)], &wRet, sizeof(wRet));
                            return usbHidCompleteOk(pThis, pUrb, sizeof(wRet) + sizeof(*pSetup));
                        }
                        else
                        {
                            Log(("usbHid: GET_STATUS (endpoint) invalid, wIndex=%#x\n", pSetup->wIndex));
                        }
                        break;
                    }

                    default:
                        Log(("usbHid: Bad GET_STATUS req: bmRequestType=%#x\n", pSetup->bmRequestType));
                        return usbHidCompleteStall(pThis, pEp, pUrb, "Bad GET_STATUS");
                }
                break;
            }

            case VUSB_REQ_CLEAR_FEATURE:
                break;
        }

        /** @todo implement this. */
        Log(("usbHid: Implement standard request: bmRequestType=%#x bRequest=%#x wValue=%#x wIndex=%#x wLength=%#x\n",
             pSetup->bmRequestType, pSetup->bRequest, pSetup->wValue, pSetup->wIndex, pSetup->wLength));

        usbHidCompleteStall(pThis, pEp, pUrb, "TODO: standard request stuff");
    }
    else if ((pSetup->bmRequestType & VUSB_REQ_MASK) == VUSB_REQ_CLASS)
    {
        switch (pSetup->bRequest)
        {
            case HID_REQ_SET_IDLE:
            {
                switch (pSetup->bmRequestType)
                {
                    case VUSB_TO_INTERFACE | VUSB_REQ_CLASS | VUSB_DIR_TO_DEVICE:
                    {
                        Log(("usbHid: SET_IDLE wValue=%#x wIndex=%#x\n", pSetup->wValue, pSetup->wIndex));
                        pThis->bIdle = pSetup->wValue >> 8;
                        /* Consider 24ms to mean zero for keyboards (see IOUSBHIDDriver) */
                        if (pThis->bIdle == 6) pThis->bIdle = 0;
                        return usbHidCompleteOk(pThis, pUrb, 0);
                    }
                    break;
                }
                break;
            }
            case HID_REQ_GET_IDLE:
            {
                switch (pSetup->bmRequestType)
                {
                    case VUSB_TO_INTERFACE | VUSB_REQ_CLASS | VUSB_DIR_TO_HOST:
                    {
                        Log(("usbHid: GET_IDLE wValue=%#x wIndex=%#x, returning %#x\n", pSetup->wValue, pSetup->wIndex, pThis->bIdle));
                        pUrb->abData[sizeof(*pSetup)] = pThis->bIdle;
                        return usbHidCompleteOk(pThis, pUrb, 1);
                    }
                    break;
                }
                break;
            }
        }
        Log(("usbHid: Unimplemented class request: bmRequestType=%#x bRequest=%#x wValue=%#x wIndex=%#x wLength=%#x\n",
             pSetup->bmRequestType, pSetup->bRequest, pSetup->wValue, pSetup->wIndex, pSetup->wLength));

        usbHidCompleteStall(pThis, pEp, pUrb, "TODO: class request stuff");
    }
    else
    {
        Log(("usbHid: Unknown control msg: bmRequestType=%#x bRequest=%#x wValue=%#x wIndex=%#x wLength=%#x\n",
             pSetup->bmRequestType, pSetup->bRequest, pSetup->wValue, pSetup->wIndex, pSetup->wLength));
        return usbHidCompleteStall(pThis, pEp, pUrb, "Unknown control msg");
    }

    return VINF_SUCCESS;
}


/**
 * @copydoc PDMUSBREG::pfnUrbQueue
 */
static DECLCALLBACK(int) usbHidQueue(PPDMUSBINS pUsbIns, PVUSBURB pUrb)
{
    PUSBHID pThis = PDMINS_2_DATA(pUsbIns, PUSBHID);
    LogFlow(("usbHidQueue/#%u: pUrb=%p:%s EndPt=%#x\n", pUsbIns->iInstance, pUrb, pUrb->pszDesc, pUrb->EndPt));
    RTCritSectEnter(&pThis->CritSect);

    /*
     * Parse on a per end-point basis.
     */
    int rc;
    switch (pUrb->EndPt)
    {
        case 0:
            rc = usbHidHandleDefaultPipe(pThis, &pThis->aEps[0], pUrb);
            break;

        case 0x81:
            AssertFailed();
        case 0x01:
            rc = usbHidHandleIntrDevToHost(pThis, &pThis->aEps[1], pUrb);
            break;

        default:
            AssertMsgFailed(("EndPt=%d\n", pUrb->EndPt));
            rc = VERR_VUSB_FAILED_TO_QUEUE_URB;
            break;
    }

    RTCritSectLeave(&pThis->CritSect);
    return rc;
}


/**
 * @copydoc PDMUSBREG::pfnUsbClearHaltedEndpoint
 */
static DECLCALLBACK(int) usbHidUsbClearHaltedEndpoint(PPDMUSBINS pUsbIns, unsigned uEndpoint)
{
    PUSBHID pThis = PDMINS_2_DATA(pUsbIns, PUSBHID);
    LogFlow(("usbHidUsbClearHaltedEndpoint/#%u: uEndpoint=%#x\n", pUsbIns->iInstance, uEndpoint));

    if ((uEndpoint & ~0x80) < RT_ELEMENTS(pThis->aEps))
    {
        RTCritSectEnter(&pThis->CritSect);
        pThis->aEps[(uEndpoint & ~0x80)].fHalted = false;
        RTCritSectLeave(&pThis->CritSect);
    }

    return VINF_SUCCESS;
}


/**
 * @copydoc PDMUSBREG::pfnUsbSetInterface
 */
static DECLCALLBACK(int) usbHidUsbSetInterface(PPDMUSBINS pUsbIns, uint8_t bInterfaceNumber, uint8_t bAlternateSetting)
{
    LogFlow(("usbHidUsbSetInterface/#%u: bInterfaceNumber=%u bAlternateSetting=%u\n", pUsbIns->iInstance, bInterfaceNumber, bAlternateSetting));
    Assert(bAlternateSetting == 0);
    return VINF_SUCCESS;
}


/**
 * @copydoc PDMUSBREG::pfnUsbSetConfiguration
 */
static DECLCALLBACK(int) usbHidUsbSetConfiguration(PPDMUSBINS pUsbIns, uint8_t bConfigurationValue,
                                                   const void *pvOldCfgDesc, const void *pvOldIfState, const void *pvNewCfgDesc)
{
    PUSBHID pThis = PDMINS_2_DATA(pUsbIns, PUSBHID);
    LogFlow(("usbHidUsbSetConfiguration/#%u: bConfigurationValue=%u\n", pUsbIns->iInstance, bConfigurationValue));
    Assert(bConfigurationValue == 1);
    RTCritSectEnter(&pThis->CritSect);

    /*
     * If the same config is applied more than once, it's a kind of reset.
     */
    if (pThis->bConfigurationValue == bConfigurationValue)
        usbHidResetWorker(pThis, NULL, true /*fSetConfig*/); /** @todo figure out the exact difference */
    pThis->bConfigurationValue = bConfigurationValue;

    /*
     * Tell the other end that the keyboard is now enabled and wants
     * to receive keystrokes.
     */
    pThis->Lun0.pDrv->pfnSetActive(pThis->Lun0.pDrv, true);

    RTCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}


/**
 * @copydoc PDMUSBREG::pfnUsbGetDescriptorCache
 */
static DECLCALLBACK(PCPDMUSBDESCCACHE) usbHidUsbGetDescriptorCache(PPDMUSBINS pUsbIns)
{
    PUSBHID pThis = PDMINS_2_DATA(pUsbIns, PUSBHID);
    LogFlow(("usbHidUsbGetDescriptorCache/#%u:\n", pUsbIns->iInstance));
    return &g_UsbHidDescCache;
}


/**
 * @copydoc PDMUSBREG::pfnUsbReset
 */
static DECLCALLBACK(int) usbHidUsbReset(PPDMUSBINS pUsbIns, bool fResetOnLinux)
{
    PUSBHID pThis = PDMINS_2_DATA(pUsbIns, PUSBHID);
    LogFlow(("usbHidUsbReset/#%u:\n", pUsbIns->iInstance));
    RTCritSectEnter(&pThis->CritSect);

    int rc = usbHidResetWorker(pThis, NULL, false /*fSetConfig*/);

    RTCritSectLeave(&pThis->CritSect);
    return rc;
}


/**
 * @copydoc PDMUSBREG::pfnDestruct
 */
static void usbHidDestruct(PPDMUSBINS pUsbIns)
{
    PUSBHID pThis = PDMINS_2_DATA(pUsbIns, PUSBHID);
    LogFlow(("usbHidDestruct/#%u:\n", pUsbIns->iInstance));

    if (RTCritSectIsInitialized(&pThis->CritSect))
    {
        /* Let whoever runs in this critical section complete. */
        RTCritSectEnter(&pThis->CritSect);
        RTCritSectLeave(&pThis->CritSect);
        RTCritSectDelete(&pThis->CritSect);
    }

    if (pThis->hEvtDoneQueue != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(pThis->hEvtDoneQueue);
        pThis->hEvtDoneQueue = NIL_RTSEMEVENT;
    }
}


/**
 * @copydoc PDMUSBREG::pfnConstruct
 */
static DECLCALLBACK(int) usbHidConstruct(PPDMUSBINS pUsbIns, int iInstance, PCFGMNODE pCfg, PCFGMNODE pCfgGlobal)
{
    PUSBHID pThis = PDMINS_2_DATA(pUsbIns, PUSBHID);
    Log(("usbHidConstruct/#%u:\n", iInstance));

    /*
     * Perform the basic structure initialization first so the destructor
     * will not misbehave.
     */
    pThis->pUsbIns                                  = pUsbIns;
    pThis->hEvtDoneQueue                            = NIL_RTSEMEVENT;
    pThis->XlatState                                = SS_IDLE;
    usbHidQueueInit(&pThis->ToHostQueue);
    usbHidQueueInit(&pThis->DoneQueue);

    int rc = RTCritSectInit(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    rc = RTSemEventCreate(&pThis->hEvtDoneQueue);
    AssertRCReturn(rc, rc);

    /*
     * Validate and read the configuration.
     */
    rc = CFGMR3ValidateConfig(pCfg, "/", "", "", "UsbHid", iInstance);
    if (RT_FAILURE(rc))
        return rc;

    pThis->Lun0.IBase.pfnQueryInterface = usbHidKeyboardQueryInterface;
    pThis->Lun0.IPort.pfnPutEvent       = usbHidKeyboardPutEvent;

    /*
     * Attach the keyboard driver.
     */
    rc = PDMUsbHlpDriverAttach(pUsbIns, 0 /*iLun*/, &pThis->Lun0.IBase, &pThis->Lun0.pDrvBase, "Keyboard Port");
    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("HID failed to attach keyboard driver"));

    pThis->Lun0.pDrv = PDMIBASE_QUERY_INTERFACE(pThis->Lun0.pDrvBase, PDMIKEYBOARDCONNECTOR);
    if (!pThis->Lun0.pDrv)
        return PDMUsbHlpVMSetError(pUsbIns, VERR_PDM_MISSING_INTERFACE, RT_SRC_POS, N_("HID failed to query keyboard interface"));

    return VINF_SUCCESS;
}


/**
 * The USB Human Interface Device (HID) Keyboard registration record.
 */
const PDMUSBREG g_UsbHidKbd =
{
    /* u32Version */
    PDM_USBREG_VERSION,
    /* szName */
    "HidKeyboard",
    /* pszDescription */
    "USB HID Keyboard.",
    /* fFlags */
    0,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(USBHID),
    /* pfnConstruct */
    usbHidConstruct,
    /* pfnDestruct */
    usbHidDestruct,
    /* pfnVMInitComplete */
    NULL,
    /* pfnVMPowerOn */
    NULL,
    /* pfnVMReset */
    NULL,
    /* pfnVMSuspend */
    NULL,
    /* pfnVMResume */
    NULL,
    /* pfnVMPowerOff */
    NULL,
    /* pfnHotPlugged */
    NULL,
    /* pfnHotUnplugged */
    NULL,
    /* pfnDriverAttach */
    NULL,
    /* pfnDriverDetach */
    NULL,
    /* pfnQueryInterface */
    NULL,
    /* pfnUsbReset */
    usbHidUsbReset,
    /* pfnUsbGetDescriptorCache */
    usbHidUsbGetDescriptorCache,
    /* pfnUsbSetConfiguration */
    usbHidUsbSetConfiguration,
    /* pfnUsbSetInterface */
    usbHidUsbSetInterface,
    /* pfnUsbClearHaltedEndpoint */
    usbHidUsbClearHaltedEndpoint,
    /* pfnUrbNew */
    NULL/*usbHidUrbNew*/,
    /* pfnUrbQueue */
    usbHidQueue,
    /* pfnUrbCancel */
    usbHidUrbCancel,
    /* pfnUrbReap */
    usbHidUrbReap,
    /* u32TheEnd */
    PDM_USBREG_VERSION
};
