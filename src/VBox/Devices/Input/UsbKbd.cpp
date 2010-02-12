/** @file
 * UsbKbd - USB Human Interface Device Emulation (Keyboard).
 */

/*
 * Copyright (C) 2007-2010 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP   LOG_GROUP_USB_MSD
#include <VBox/pdmusb.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include "../Builtins.h"


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
    uint8_t     ShiftState;     /* Modifier keys bitfield */
    uint8_t     Reserved;       /* Currently unused */
    uint8_t     aKeys[6];       /* Normal keys */
} USBHIDK_REPORT, *PUSBHIDK_REPORT;

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

    /** HID report reflecting the current keyboard state. */
    USBHIDK_REPORT      Report;

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

/* HID report descriptor. */
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
    /* Usage Maximum */             0x29, 0xFF,0x00,/* 255 */
    /* Input */                     0x81, 0x00,     /* Data, Array, Absolute, Bit field */
    /* End Collection */            0xC0,
};

/* Additional HID class interface descriptor. */
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
    NULL,
    &g_aUsbHidInterfaces[0]
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
        pThis->aEps[1].fHalted = true;
        pThis->aEps[2].fHalted = true;
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
    pUrb->cbData    = cbData;

    usbHidLinkDone(pThis, pUrb);
    return VINF_SUCCESS;
}


/**
 * Reset worker for usbHidUsbReset, usbHidUsbSetConfiguration and
 * usbHidUrbHandleDefaultPipe.
 *
 * @returns VBox status code.
 * @param   pThis               The HID instance.
 * @param   pUrb                Set when usbHidUrbHandleDefaultPipe is the
 *                              caller.
 * @param   fSetConfig          Set when usbHidUsbSetConfiguration is the
 *                              caller.
 */
static int usbHidResetWorker(PUSBHID pThis, PVUSBURB pUrb, bool fSetConfig)
{
    /*
     * Reset the device state.
     */
    pThis->enmState = USBHIDREQSTATE_READY;
    pThis->bIdle = 0;
    memset(&pThis->Report, 0, sizeof(pThis->Report));

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


/**
 * Sends a state report to the host if there is a pending URB.
 */
static int usbHidSendReport(PUSBHID pThis)
{
    PVUSBURB pUrb = usbHidQueueRemoveHead(&pThis->ToHostQueue);
    if (pUrb)
    {
        PUSBHIDK_REPORT pReport = &pThis->Report;
        size_t          cbCopy;

        cbCopy = sizeof(*pReport);
        memcpy(&pUrb->abData[0], pReport, cbCopy);
//        LogRel(("Sent report: %x : %x %x, size %d\n", pReport->ShiftState, pReport->aKeys[0], pReport->aKeys[1], cbCopy));
        return usbHidCompleteOk(pThis, pUrb, cbCopy);
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

static int8_t clamp_i8(int32_t val)
{
    if (val > 127) {
        val = 127; 
    } else if (val < -127) {
        val = -127;
    }
    return val;
}

static uint8_t aKeycode2Hid[] =
{
    0x00, 0x29, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x2b,
    0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c,
    0x12, 0x13, 0x2f, 0x30, 0x28, 0xe0, 0x04, 0x16,
    0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33,
    0x34, 0x35, 0xe1, 0x31, 0x1d, 0x1b, 0x06, 0x19,
    0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0xe5, 0x55,
    0xe2, 0x2c, 0x32, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e,
    0x3f, 0x40, 0x41, 0x42, 0x43, 0x53, 0x47, 0x5f,
    0x60, 0x61, 0x56, 0x5c, 0x5d, 0x5e, 0x57, 0x59,
    0x5a, 0x5b, 0x62, 0x63, 0x00, 0x00, 0x00, 0x44,
    0x45, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
    0xe8, 0xe9, 0x71, 0x72, 0x73, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x85, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xe3, 0xe7, 0x65,
};

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
    PUSBHIDK_REPORT pReport = &pThis->Report;
    uint8_t u8HidCode;
    int fKeyDown;
    int i;

//    int rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
//    AssertReleaseRC(rc);

    /* Extract the key event. */
    u8HidCode = aKeycode2Hid[u8KeyCode & 0x7f];
    fKeyDown = !(u8KeyCode & 0x80);

    if (fKeyDown) 
    {
        for (i = 0; i < RT_ELEMENTS(pReport->aKeys); ++i)
        {
            if (pReport->aKeys[i] == u8HidCode)
                break;                              /* Skip repeat events. */
            if (pReport->aKeys[i] == 0) {
                pReport->aKeys[i] = u8HidCode;      /* Report key down. */
                break;
            }
        }
        if (i == RT_ELEMENTS(pReport->aKeys))
        {
            /* We ran out of room. Report error. */
            // @todo!!
        }
    }
    else
    {
        for (i = 0; i < RT_ELEMENTS(pReport->aKeys); ++i)
        {
            if (pReport->aKeys[i] == u8HidCode)
            {
                pReport->aKeys[i] = 0;
                break;                              /* Remove key down. */
            }
        }
        if (i == RT_ELEMENTS(pReport->aKeys))
        {
//            AssertMsgFailed(("Key is up but was never down!?"));
        }
    }

    /* Send a report if the host is already waiting for it. */
    usbHidSendReport(pThis);

//    PDMCritSectLeave(&pThis->CritSect);
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
            /* If device was not set idle, sent the current report right away. */
            if (pThis->bIdle != 0)
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
                            case DT_IF_HID_REPORT:
                                uint32_t    cbCopy;

                                /* Returned data is written after the setup message. */
                                cbCopy = pUrb->cbData - sizeof(*pSetup);
                                cbCopy = RT_MIN(cbCopy, sizeof(g_UsbHidReportDesc));
                                Log(("usbHid: GET_DESCRIPTOR DT_IF_HID_REPORT wValue=%#x wIndex=%#x cbCopy=%#x\n", pSetup->wValue, pSetup->wIndex, cbCopy));
                                memcpy(&pUrb->abData[sizeof(*pSetup)], &g_UsbHidReportDesc, cbCopy);
                                return usbHidCompleteOk(pThis, pUrb, cbCopy + sizeof(*pSetup));
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
 * @copydoc PDMUSBREG::pfnQueue
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
    rc = CFGMR3ValidateConfig(pCfg, "/", "", "", "UsbHid", iInstance);
    if (RT_FAILURE(rc))
        return rc;

    pThis->Lun0.IBase.pfnQueryInterface = usbHidKeyboardQueryInterface;
    pThis->Lun0.IPort.pfnPutEvent       = usbHidKeyboardPutEvent;

    /*
     * Attach the keyboard driver.
     */
    rc = pUsbIns->pHlpR3->pfnDriverAttach(pUsbIns, 0 /*iLun*/, &pThis->Lun0.IBase, &pThis->Lun0.pDrvBase, "Keyboard Port");
    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("HID failed to attach keyboard driver"));

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
    ~0,
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
    /* pfnUsbGetCachedDescriptors */
    usbHidUsbGetDescriptorCache,
    /* pfnUsbSetConfiguration */
    usbHidUsbSetConfiguration,
    /* pfnUsbSetInterface */
    usbHidUsbSetInterface,
    /* pfnUsbClearHaltedEndpoint */
    usbHidUsbClearHaltedEndpoint,
    /* pfnUrbNew */
    NULL/*usbHidUrbNew*/,
    /* pfnQueue */
    usbHidQueue,
    /* pfnUrbCancel */
    usbHidUrbCancel,
    /* pfnUrbReap */
    usbHidUrbReap,
    /* u32TheEnd */
    PDM_USBREG_VERSION
};
