/** @file
 * UsbMouse - USB Human Interface Device Emulation (Mouse).
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
#define LOG_GROUP   LOG_GROUP_USB_MSD
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
#define USBHID_STR_ID_PRODUCT_M     2
#define USBHID_STR_ID_PRODUCT_T     3
/** @} */

/** @name USB HID specific descriptor types
 * @{ */
#define DT_IF_HID_DESCRIPTOR        0x21
#define DT_IF_HID_REPORT            0x22
/** @} */

/** @name USB HID vendor and product IDs
 * @{ */
#define VBOX_USB_VENDOR             0x80EE
#define USBHID_PID_MOUSE            0x0020
#define USBHID_PID_TABLET           0x0021
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
 * Mouse movement accumulator.
 */
typedef struct USBHIDM_ACCUM
{
    uint32_t    btn;
    int32_t     dX;
    int32_t     dY;
    int32_t     dZ;
} USBHIDM_ACCUM, *PUSBHIDM_ACCUM;


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
    /** Endpoint 0 is the default control pipe, 1 is the dev->host interrupt one. */
    USBHIDEP            aEps[2];
    /** The state of the HID (state machine).*/
    USBHIDREQSTATE      enmState;

    /** Pointer movement accumulator. */
    USBHIDM_ACCUM       PtrDelta;

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
    /** Is this an absolute pointing device (tablet)? Relative (mouse) otherwise. */
    bool                isAbsolute;
    /** Tablet coordinate shift factor for old and broken operating systems. */
    uint8_t             u8CoordShift;

    /**
     * Mouse port - LUN#0.
     *
     * @implements  PDMIBASE
     * @implements  PDMIMOUSEPORT
     */
    struct
    {
        /** The base interface for the mouse port. */
        PDMIBASE                            IBase;
        /** The mouse port base interface. */
        PDMIMOUSEPORT                       IPort;

        /** The base interface of the attached mouse driver. */
        R3PTRTYPE(PPDMIBASE)                pDrvBase;
        /** The mouse interface of the attached mouse driver. */
        R3PTRTYPE(PPDMIMOUSECONNECTOR)      pDrv;
    } Lun0;

} USBHID;
/** Pointer to the USB HID instance data. */
typedef USBHID *PUSBHID;

/**
 * The USB HID report structure for relative device.
 */
typedef struct USBHIDM_REPORT
{
    uint8_t     btn;
    int8_t      dx;
    int8_t      dy;
    int8_t      dz;
} USBHIDM_REPORT, *PUSBHIDM_REPORT;

/**
 * The USB HID report structure for absolute device.
 */

typedef struct USBHIDT_REPORT
{
    uint8_t     btn;
    int8_t      dz;
    uint16_t    cx;
    uint16_t    cy;
} USBHIDT_REPORT, *PUSBHIDT_REPORT;

/**
 * The combined USB HID report union for relative and absolute device.
 */
typedef union USBHIDTM_REPORT
{
    USBHIDT_REPORT      t;
    USBHIDM_REPORT      m;
} USBHIDTM_REPORT, *PUSBHIDTM_REPORT;

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static const PDMUSBDESCCACHESTRING g_aUsbHidStrings_en_US[] =
{
    { USBHID_STR_ID_MANUFACTURER,   "VirtualBox"    },
    { USBHID_STR_ID_PRODUCT_M,      "USB Mouse"     },
    { USBHID_STR_ID_PRODUCT_T,      "USB Tablet"    },
};

static const PDMUSBDESCCACHELANG g_aUsbHidLanguages[] =
{
    { 0x0409, RT_ELEMENTS(g_aUsbHidStrings_en_US), g_aUsbHidStrings_en_US }
};

static const VUSBDESCENDPOINTEX g_aUsbHidMEndpointDescs[] =
{
    {
        {
            /* .bLength = */            sizeof(VUSBDESCENDPOINT),
            /* .bDescriptorType = */    VUSB_DT_ENDPOINT,
            /* .bEndpointAddress = */   0x81 /* ep=1, in */,
            /* .bmAttributes = */       3 /* interrupt */,
            /* .wMaxPacketSize = */     4,
            /* .bInterval = */          10,
        },
        /* .pvMore = */     NULL,
        /* .pvClass = */    NULL,
        /* .cbClass = */    0
    },
};

static const VUSBDESCENDPOINTEX g_aUsbHidTEndpointDescs[] =
{
    {
        {
            /* .bLength = */            sizeof(VUSBDESCENDPOINT),
            /* .bDescriptorType = */    VUSB_DT_ENDPOINT,
            /* .bEndpointAddress = */   0x81 /* ep=1, in */,
            /* .bmAttributes = */       3 /* interrupt */,
            /* .wMaxPacketSize = */     6,
            /* .bInterval = */          10,
        },
        /* .pvMore = */     NULL,
        /* .pvClass = */    NULL,
        /* .cbClass = */    0
    },
};

/* HID report descriptor (mouse). */
static const uint8_t g_UsbHidMReportDesc[] =
{
    /* Usage Page */                0x05, 0x01,     /* Generic Desktop */
    /* Usage */                     0x09, 0x02,     /* Mouse */
    /* Collection */                0xA1, 0x01,     /* Application */
    /* Usage */                     0x09, 0x01,     /* Pointer */
    /* Collection */                0xA1, 0x00,     /* Physical */
    /* Usage Page */                0x05, 0x09,     /* Button */
    /* Usage Minimum */             0x19, 0x01,     /* Button 1 */
    /* Usage Maximum */             0x29, 0x05,     /* Button 5 */
    /* Logical Minimum */           0x15, 0x00,     /* 0 */
    /* Logical Maximum */           0x25, 0x01,     /* 1 */
    /* Report Count */              0x95, 0x05,     /* 5 */
    /* Report Size */               0x75, 0x01,     /* 1 */
    /* Input */                     0x81, 0x02,     /* Data, Value, Absolute, Bit field */
    /* Report Count */              0x95, 0x01,     /* 1 */
    /* Report Size */               0x75, 0x03,     /* 3 (padding bits) */
    /* Input */                     0x81, 0x03,     /* Constant, Value, Absolute, Bit field */
    /* Usage Page */                0x05, 0x01,     /* Generic Desktop */
    /* Usage */                     0x09, 0x30,     /* X */
    /* Usage */                     0x09, 0x31,     /* Y */
    /* Usage */                     0x09, 0x38,     /* Z (wheel) */
    /* Logical Minimum */           0x15, 0x81,     /* -127 */
    /* Logical Maximum */           0x25, 0x7F,     /* +127 */
    /* Report Size */               0x75, 0x08,     /* 8 */
    /* Report Count */              0x95, 0x03,     /* 3 */
    /* Input */                     0x81, 0x06,     /* Data, Value, Relative, Bit field */
    /* End Collection */            0xC0,
    /* End Collection */            0xC0,
};

/* HID report descriptor (tablet). */
/* NB: The layout is far from random. Having the buttons and Z axis grouped
 * together avoids alignment issues. Also, if X/Y is reported first, followed
 * by buttons/Z, Windows gets phantom Z movement. That is likely a bug in Windows
 * as OS X shows no such problem. When X/Y is reported last, Windows behaves
 * properly.
 */
static const uint8_t g_UsbHidTReportDesc[] =
{
    /* Usage Page */                0x05, 0x01,     /* Generic Desktop */
    /* Usage */                     0x09, 0x02,     /* Mouse */
    /* Collection */                0xA1, 0x01,     /* Application */
    /* Usage */                     0x09, 0x01,     /* Pointer */
    /* Collection */                0xA1, 0x00,     /* Physical */
    /* Usage Page */                0x05, 0x09,     /* Button */
    /* Usage Minimum */             0x19, 0x01,     /* Button 1 */
    /* Usage Maximum */             0x29, 0x05,     /* Button 5 */
    /* Logical Minimum */           0x15, 0x00,     /* 0 */
    /* Logical Maximum */           0x25, 0x01,     /* 1 */
    /* Report Count */              0x95, 0x05,     /* 5 */
    /* Report Size */               0x75, 0x01,     /* 1 */
    /* Input */                     0x81, 0x02,     /* Data, Value, Absolute, Bit field */
    /* Report Count */              0x95, 0x01,     /* 1 */
    /* Report Size */               0x75, 0x03,     /* 3 (padding bits) */
    /* Input */                     0x81, 0x03,     /* Constant, Value, Absolute, Bit field */
    /* Usage Page */                0x05, 0x01,     /* Generic Desktop */
    /* Usage */                     0x09, 0x38,     /* Z (wheel) */
    /* Logical Minimum */           0x15, 0x81,     /* -127 */
    /* Logical Maximum */           0x25, 0x7F,     /* +127 */
    /* Report Size */               0x75, 0x08,     /* 8 */
    /* Report Count */              0x95, 0x01,     /* 1 */
    /* Input */                     0x81, 0x06,     /* Data, Value, Relative, Bit field */
    /* Usage Page */                0x05, 0x01,     /* Generic Desktop */
    /* Usage */                     0x09, 0x30,     /* X */
    /* Usage */                     0x09, 0x31,     /* Y */
    /* Logical Minimum */           0x15, 0x00,     /* 0 */
    /* Logical Maximum */           0x26, 0xFF,0x7F,/* 0x7fff */
    /* Physical Minimum */          0x35, 0x00,     /* 0 */
    /* Physical Maximum */          0x46, 0xFF,0x7F,/* 0x7fff */
    /* Report Size */               0x75, 0x10,     /* 16 */
    /* Report Count */              0x95, 0x02,     /* 2 */
    /* Input */                     0x81, 0x02,     /* Data, Value, Absolute, Bit field */
    /* End Collection */            0xC0,
    /* End Collection */            0xC0,
};

/* Additional HID class interface descriptor. */
static const uint8_t g_UsbHidMIfHidDesc[] =
{
    /* .bLength = */                0x09,
    /* .bDescriptorType = */        0x21,       /* HID */
    /* .bcdHID = */                 0x10, 0x01, /* 1.1 */
    /* .bCountryCode = */           0,
    /* .bNumDescriptors = */        1,
    /* .bDescriptorType = */        0x22,       /* Report */
    /* .wDescriptorLength = */      sizeof(g_UsbHidMReportDesc), 0x00
};

/* Additional HID class interface descriptor. */
static const uint8_t g_UsbHidTIfHidDesc[] =
{
    /* .bLength = */                0x09,
    /* .bDescriptorType = */        0x21,       /* HID */
    /* .bcdHID = */                 0x10, 0x01, /* 1.1 */
    /* .bCountryCode = */           0,
    /* .bNumDescriptors = */        1,
    /* .bDescriptorType = */        0x22,       /* Report */
    /* .wDescriptorLength = */      sizeof(g_UsbHidTReportDesc), 0x00
};

static const VUSBDESCINTERFACEEX g_UsbHidMInterfaceDesc =
{
    {
        /* .bLength = */                sizeof(VUSBDESCINTERFACE),
        /* .bDescriptorType = */        VUSB_DT_INTERFACE,
        /* .bInterfaceNumber = */       0,
        /* .bAlternateSetting = */      0,
        /* .bNumEndpoints = */          1,
        /* .bInterfaceClass = */        3 /* HID */,
        /* .bInterfaceSubClass = */     1 /* Boot Interface */,
        /* .bInterfaceProtocol = */     2 /* Mouse */,
        /* .iInterface = */             0
    },
    /* .pvMore = */     NULL,
    /* .pvClass = */    &g_UsbHidMIfHidDesc,
    /* .cbClass = */    sizeof(g_UsbHidMIfHidDesc),
    &g_aUsbHidMEndpointDescs[0]
};

static const VUSBDESCINTERFACEEX g_UsbHidTInterfaceDesc =
{
    {
        /* .bLength = */                sizeof(VUSBDESCINTERFACE),
        /* .bDescriptorType = */        VUSB_DT_INTERFACE,
        /* .bInterfaceNumber = */       0,
        /* .bAlternateSetting = */      0,
        /* .bNumEndpoints = */          1,
        /* .bInterfaceClass = */        3 /* HID */,
        /* .bInterfaceSubClass = */     0 /* No subclass - no boot interface. */,
        /* .bInterfaceProtocol = */     0 /* No protocol - no boot interface. */,
        /* .iInterface = */             0
    },
    /* .pvMore = */     NULL,
    /* .pvClass = */    &g_UsbHidTIfHidDesc,
    /* .cbClass = */    sizeof(g_UsbHidTIfHidDesc),
    &g_aUsbHidTEndpointDescs[0]
};

static const VUSBINTERFACE g_aUsbHidMInterfaces[] =
{
    { &g_UsbHidMInterfaceDesc, /* .cSettings = */ 1 },
};

static const VUSBINTERFACE g_aUsbHidTInterfaces[] =
{
    { &g_UsbHidTInterfaceDesc, /* .cSettings = */ 1 },
};

static const VUSBDESCCONFIGEX g_UsbHidMConfigDesc =
{
    {
        /* .bLength = */            sizeof(VUSBDESCCONFIG),
        /* .bDescriptorType = */    VUSB_DT_CONFIG,
        /* .wTotalLength = */       0 /* recalculated on read */,
        /* .bNumInterfaces = */     RT_ELEMENTS(g_aUsbHidMInterfaces),
        /* .bConfigurationValue =*/ 1,
        /* .iConfiguration = */     0,
        /* .bmAttributes = */       RT_BIT(7),
        /* .MaxPower = */           50 /* 100mA */
    },
    NULL,                           /* pvMore */
    &g_aUsbHidMInterfaces[0],
    NULL                            /* pvOriginal */
};

static const VUSBDESCCONFIGEX g_UsbHidTConfigDesc =
{
    {
        /* .bLength = */            sizeof(VUSBDESCCONFIG),
        /* .bDescriptorType = */    VUSB_DT_CONFIG,
        /* .wTotalLength = */       0 /* recalculated on read */,
        /* .bNumInterfaces = */     RT_ELEMENTS(g_aUsbHidTInterfaces),
        /* .bConfigurationValue =*/ 1,
        /* .iConfiguration = */     0,
        /* .bmAttributes = */       RT_BIT(7),
        /* .MaxPower = */           50 /* 100mA */
    },
    NULL,                           /* pvMore */
    &g_aUsbHidTInterfaces[0],
    NULL                            /* pvOriginal */
};

static const VUSBDESCDEVICE g_UsbHidMDeviceDesc =
{
    /* .bLength = */                sizeof(g_UsbHidMDeviceDesc),
    /* .bDescriptorType = */        VUSB_DT_DEVICE,
    /* .bcdUsb = */                 0x110,  /* 1.1 */
    /* .bDeviceClass = */           0 /* Class specified in the interface desc. */,
    /* .bDeviceSubClass = */        0 /* Subclass specified in the interface desc. */,
    /* .bDeviceProtocol = */        0 /* Protocol specified in the interface desc. */,
    /* .bMaxPacketSize0 = */        8,
    /* .idVendor = */               VBOX_USB_VENDOR,
    /* .idProduct = */              USBHID_PID_MOUSE,
    /* .bcdDevice = */              0x0100, /* 1.0 */
    /* .iManufacturer = */          USBHID_STR_ID_MANUFACTURER,
    /* .iProduct = */               USBHID_STR_ID_PRODUCT_M,
    /* .iSerialNumber = */          0,
    /* .bNumConfigurations = */     1
};

static const VUSBDESCDEVICE g_UsbHidTDeviceDesc =
{
    /* .bLength = */                sizeof(g_UsbHidTDeviceDesc),
    /* .bDescriptorType = */        VUSB_DT_DEVICE,
    /* .bcdUsb = */                 0x110,  /* 1.1 */
    /* .bDeviceClass = */           0 /* Class specified in the interface desc. */,
    /* .bDeviceSubClass = */        0 /* Subclass specified in the interface desc. */,
    /* .bDeviceProtocol = */        0 /* Protocol specified in the interface desc. */,
    /* .bMaxPacketSize0 = */        8,
    /* .idVendor = */               VBOX_USB_VENDOR,
    /* .idProduct = */              USBHID_PID_TABLET,
    /* .bcdDevice = */              0x0100, /* 1.0 */
    /* .iManufacturer = */          USBHID_STR_ID_MANUFACTURER,
    /* .iProduct = */               USBHID_STR_ID_PRODUCT_T,
    /* .iSerialNumber = */          0,
    /* .bNumConfigurations = */     1
};

static const PDMUSBDESCCACHE g_UsbHidMDescCache =
{
    /* .pDevice = */                &g_UsbHidMDeviceDesc,
    /* .paConfigs = */              &g_UsbHidMConfigDesc,
    /* .paLanguages = */            g_aUsbHidLanguages,
    /* .cLanguages = */             RT_ELEMENTS(g_aUsbHidLanguages),
    /* .fUseCachedDescriptors = */  true,
    /* .fUseCachedStringsDescriptors = */ true
};

static const PDMUSBDESCCACHE g_UsbHidTDescCache =
{
    /* .pDevice = */                &g_UsbHidTDeviceDesc,
    /* .paConfigs = */              &g_UsbHidTConfigDesc,
    /* .paLanguages = */            g_aUsbHidLanguages,
    /* .cLanguages = */             RT_ELEMENTS(g_aUsbHidLanguages),
    /* .fUseCachedDescriptors = */  true,
    /* .fUseCachedStringsDescriptors = */ true
};


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
     * Wait for the any command currently executing to complete before
     * resetting.  (We cannot cancel its execution.)  How we do this depends
     * on the reset method.
     */

    /*
     * Reset the device state.
     */
    pThis->enmState = USBHIDREQSTATE_READY;
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

static int8_t clamp_i8(int32_t val)
{
    if (val > 127) {
        val = 127;
    } else if (val < -127) {
        val = -127;
    }
    return val;
}

/**
 * Create a USB HID report report based on the currently accumulated data.
 */
static size_t usbHidFillReport(PUSBHIDTM_REPORT pReport, PUSBHIDM_ACCUM pAccumulated, bool isAbsolute)
{
    size_t          cbCopy;

    if (isAbsolute)
    {
        pReport->t.btn = pAccumulated->btn;
        pReport->t.cx  = pAccumulated->dX;
        pReport->t.cy  = pAccumulated->dY;
        pReport->t.dz  = clamp_i8(pAccumulated->dZ);

        cbCopy = sizeof(pReport->t);
//        LogRel(("Abs movement, X=%d, Y=%d, dZ=%d, btn=%02x, report size %d\n", pReport->t.cx, pReport->t.cy, pReport->t.dz, pReport->t.btn, cbCopy));
    }
    else
    {
        pReport->m.btn = pAccumulated->btn;
        pReport->m.dx  = clamp_i8(pAccumulated->dX);
        pReport->m.dy  = clamp_i8(pAccumulated->dY);
        pReport->m.dz  = clamp_i8(pAccumulated->dZ);
    
        cbCopy = sizeof(pReport->m);
//        LogRel(("Rel movement, dX=%d, dY=%d, dZ=%d, btn=%02x, report size %d\n", pReport->m.dx, pReport->m.dy, pReport->m.dz, pReport->m.btn, cbCopy));
    }

    /* Clear the accumulated movement. */
    RT_ZERO(*pAccumulated);

    return cbCopy;
}

/**
 * Sends a state report to the host if there is a pending URB.
 */
static int usbHidSendReport(PUSBHID pThis)
{
    PVUSBURB    pUrb = usbHidQueueRemoveHead(&pThis->ToHostQueue);

    if (pUrb)
    {
        PUSBHIDTM_REPORT    pReport = (PUSBHIDTM_REPORT)&pUrb->abData[0];
        size_t              cbCopy;

        cbCopy = usbHidFillReport(pReport, &pThis->PtrDelta, pThis->isAbsolute);
        pThis->fHasPendingChanges = false;
        return usbHidCompleteOk(pThis, pUrb, cbCopy);
    }
    else
    {
        Log2(("No available URB for USB mouse\n"));
        pThis->fHasPendingChanges = true;
    }
    return VINF_EOF;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) usbHidMouseQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PUSBHID pThis = RT_FROM_MEMBER(pInterface, USBHID, Lun0.IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->Lun0.IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMOUSEPORT, &pThis->Lun0.IPort);
    return NULL;
}

/**
 * Relative mouse event handler.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the mouse port interface (KBDState::Mouse.iPort).
 * @param   i32DeltaX       The X delta.
 * @param   i32DeltaY       The Y delta.
 * @param   i32DeltaZ       The Z delta.
 * @param   i32DeltaW       The W delta.
 * @param   fButtonStates   The button states.
 */
static DECLCALLBACK(int) usbHidMousePutEvent(PPDMIMOUSEPORT pInterface, int32_t i32DeltaX, int32_t i32DeltaY, int32_t i32DeltaZ, int32_t i32DeltaW, uint32_t fButtonStates)
{
    PUSBHID pThis = RT_FROM_MEMBER(pInterface, USBHID, Lun0.IPort);
    RTCritSectEnter(&pThis->CritSect);

    /* Accumulate movement - the events from the front end may arrive
     * at a much higher rate than USB can handle.
     */
    pThis->PtrDelta.btn = fButtonStates;
    pThis->PtrDelta.dX += i32DeltaX;
    pThis->PtrDelta.dY += i32DeltaY;
    pThis->PtrDelta.dZ -= i32DeltaZ;    /* Inverted! */

    /* Send a report if possible. */
    usbHidSendReport(pThis);

    RTCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * Absolute mouse event handler.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the mouse port interface (KBDState::Mouse.iPort).
 * @param   u32X            The X coordinate.
 * @param   u32Y            The Y coordinate.
 * @param   i32DeltaZ       The Z delta.
 * @param   i32DeltaW       The W delta.
 * @param   fButtonStates   The button states.
 */
static DECLCALLBACK(int) usbHidMousePutEventAbs(PPDMIMOUSEPORT pInterface, uint32_t u32X, uint32_t u32Y, int32_t i32DeltaZ, int32_t i32DeltaW, uint32_t fButtonStates)
{
    PUSBHID pThis = RT_FROM_MEMBER(pInterface, USBHID, Lun0.IPort);
    RTCritSectEnter(&pThis->CritSect);

    Assert(pThis->isAbsolute);

    /* Accumulate movement - the events from the front end may arrive
     * at a much higher rate than USB can handle. Probably not a real issue
     * when only the Z axis is relative (X/Y movement isn't technically
     * accumulated and only the last value is used).
     */
    pThis->PtrDelta.btn = fButtonStates;
    pThis->PtrDelta.dX  = u32X >> pThis->u8CoordShift;
    pThis->PtrDelta.dY  = u32Y >> pThis->u8CoordShift;
    pThis->PtrDelta.dZ -= i32DeltaZ;    /* Inverted! */

    /* Send a report if possible. */
    usbHidSendReport(pThis);

    RTCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}

/**
 * @copydoc PDMUSBREG::pfnUrbReap
 */
static DECLCALLBACK(PVUSBURB) usbHidUrbReap(PPDMUSBINS pUsbIns, RTMSINTERVAL cMillies)
{
    PUSBHID pThis = PDMINS_2_DATA(pUsbIns, PUSBHID);
    LogFlow(("usbHidUrbReap/#%u: cMillies=%u\n", pUsbIns->iInstance, cMillies));

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
            /* If a report is pending, send it right away. */
            if (pThis->fHasPendingChanges)
                usbHidSendReport(pThis);
            LogFlow(("usbHidHandleIntrDevToHost: Added %p:%s to the queue\n", pUrb, pUrb->pszDesc));
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
                            uint32_t        cbCopy;
                            uint32_t        cbDesc;
                            const uint8_t   *pDesc;

                            case DT_IF_HID_DESCRIPTOR:
                            {
                                if (pThis->isAbsolute)
                                {
                                    cbDesc = sizeof(g_UsbHidTIfHidDesc);
                                    pDesc = (const uint8_t *)&g_UsbHidTIfHidDesc;
                                }
                                else
                                {
                                    cbDesc = sizeof(g_UsbHidMIfHidDesc);
                                    pDesc = (const uint8_t *)&g_UsbHidMIfHidDesc;
                                }
                                /* Returned data is written after the setup message. */
                                cbCopy = pUrb->cbData - sizeof(*pSetup);
                                cbCopy = RT_MIN(cbCopy, cbDesc);
                                Log(("usbHidMouse: GET_DESCRIPTOR DT_IF_HID_DESCRIPTOR wValue=%#x wIndex=%#x cbCopy=%#x\n", pSetup->wValue, pSetup->wIndex, cbCopy));
                                memcpy(&pUrb->abData[sizeof(*pSetup)], pDesc, cbCopy);
                                return usbHidCompleteOk(pThis, pUrb, cbCopy + sizeof(*pSetup));
                            }

                            case DT_IF_HID_REPORT:
                            {
                                if (pThis->isAbsolute)
                                {
                                    cbDesc = sizeof(g_UsbHidTReportDesc);
                                    pDesc = (const uint8_t *)&g_UsbHidTReportDesc;
                                }
                                else
                                {
                                    cbDesc = sizeof(g_UsbHidMReportDesc);
                                    pDesc = (const uint8_t *)&g_UsbHidMReportDesc;
                                }
                                /* Returned data is written after the setup message. */
                                cbCopy = pUrb->cbData - sizeof(*pSetup);
                                cbCopy = RT_MIN(cbCopy, cbDesc);
                                Log(("usbHid: GET_DESCRIPTOR DT_IF_HID_REPORT wValue=%#x wIndex=%#x cbCopy=%#x\n", pSetup->wValue, pSetup->wIndex, cbCopy));
                                memcpy(&pUrb->abData[sizeof(*pSetup)], pDesc, cbCopy);
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
    /* 3.1 Bulk-Only Mass Storage Reset */
    else if (    pSetup->bmRequestType == (VUSB_REQ_CLASS | VUSB_TO_INTERFACE)
             &&  pSetup->bRequest == 0xff
             &&  !pSetup->wValue
             &&  !pSetup->wLength
             &&  pSetup->wIndex == 0)
    {
        Log(("usbHidHandleDefaultPipe: Bulk-Only Mass Storage Reset\n"));
        return usbHidResetWorker(pThis, pUrb, false /*fSetConfig*/);
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
     * Set received event type to absolute or relative.
     */
    pThis->Lun0.pDrv->pfnReportModes(pThis->Lun0.pDrv, !pThis->isAbsolute,
                                     pThis->isAbsolute);

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
    if (pThis->isAbsolute) {
        return &g_UsbHidTDescCache;
    } else {
        return &g_UsbHidMDescCache;
    }
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
    usbHidQueueInit(&pThis->ToHostQueue);
    usbHidQueueInit(&pThis->DoneQueue);

    int rc = RTCritSectInit(&pThis->CritSect);
    AssertRCReturn(rc, rc);

    rc = RTSemEventCreate(&pThis->hEvtDoneQueue);
    AssertRCReturn(rc, rc);

    /*
     * Validate and read the configuration.
     */
    rc = CFGMR3ValidateConfig(pCfg, "/", "Absolute|CoordShift", "Config", "UsbHid", iInstance);
    if (RT_FAILURE(rc))
        return rc;
    rc = CFGMR3QueryBoolDef(pCfg, "Absolute", &pThis->isAbsolute, false);
    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("HID failed to query settings"));

    pThis->Lun0.IBase.pfnQueryInterface = usbHidMouseQueryInterface;
    pThis->Lun0.IPort.pfnPutEvent       = usbHidMousePutEvent;
    pThis->Lun0.IPort.pfnPutEventAbs    = usbHidMousePutEventAbs;

    /*
     * Attach the mouse driver.
     */
    rc = PDMUsbHlpDriverAttach(pUsbIns, 0 /*iLun*/, &pThis->Lun0.IBase, &pThis->Lun0.pDrvBase, "Mouse Port");
    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("HID failed to attach mouse driver"));

    pThis->Lun0.pDrv = PDMIBASE_QUERY_INTERFACE(pThis->Lun0.pDrvBase, PDMIMOUSECONNECTOR);
    if (!pThis->Lun0.pDrv)
        return PDMUsbHlpVMSetError(pUsbIns, VERR_PDM_MISSING_INTERFACE, RT_SRC_POS, N_("HID failed to query mouse interface"));

    rc = CFGMR3QueryU8Def(pCfg, "CoordShift", &pThis->u8CoordShift, 1);
    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("HID failed to query shift factor"));

    return VINF_SUCCESS;
}


/**
 * The USB Human Interface Device (HID) Mouse registration record.
 */
const PDMUSBREG g_UsbHidMou =
{
    /* u32Version */
    PDM_USBREG_VERSION,
    /* szName */
    "HidMouse",
    /* pszDescription */
    "USB HID Mouse.",
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
