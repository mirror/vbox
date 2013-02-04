/* $Id$ */
/** @file
 * Virtual USB - URBs.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_VUSB
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/err.h>
#include <iprt/alloc.h>
#include <VBox/log.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/env.h>
#include "VUSBInternal.h"



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Strings for the CTLSTAGE enum values. */
const char * const g_apszCtlStates[4] =
{
    "SETUP",
    "DATA",
    "STATUS",
    "N/A"
};


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static PVUSBCTRLEXTRA vusbMsgAllocExtraData(PVUSBURB pUrb);


#ifdef LOG_ENABLED
DECLINLINE(const char *) vusbUrbStatusName(VUSBSTATUS enmStatus)
{
    /** Strings for the URB statuses. */
    static const char * const s_apszNames[] =
    {
        "OK",
        "STALL",
        "ERR_DNR",
        "ERR_CRC",
        "DATA_UNDERRUN",
        "DATA_OVERRUN",
        "NOT_ACCESSED",
        "7", "8", "9", "10", "11", "12", "13", "14", "15"
    };

    return enmStatus < (int)RT_ELEMENTS(s_apszNames)
        ? s_apszNames[enmStatus]
        : enmStatus == VUSBSTATUS_INVALID
            ? "INVALID"
            : "??";
}

DECLINLINE(const char *) vusbUrbDirName(VUSBDIRECTION enmDir)
{
    /** Strings for the URB directions. */
    static const char * const s_apszNames[] =
    {
        "setup",
        "in",
        "out"
    };

    return enmDir < (int)RT_ELEMENTS(s_apszNames)
        ? s_apszNames[enmDir]
        : "??";
}

DECLINLINE(const char *) vusbUrbTypeName(VUSBXFERTYPE enmType)
{
    /** Strings for the URB types. */
    static const char * const s_apszName[] =
    {
        "control-part",
        "isochronous",
        "bulk",
        "interrupt",
        "control"
    };

    return enmType < (int)RT_ELEMENTS(s_apszName)
        ? s_apszName[enmType]
        : "??";
}

DECLINLINE(const char *) GetScsiErrCd(uint8_t ScsiErr)
{
    switch (ScsiErr)
    {
        case 0:     return "?";
    }
    return "?";
}

DECLINLINE(const char *) GetScsiKCQ(uint8_t Key, uint8_t ASC, uint8_t ASCQ)
{
    switch (Key)
    {
        case 0:
            switch (RT_MAKE_U16(ASC, ASCQ))
            {
                case RT_MAKE_U16(0x00, 0x00):  return "No error";
            }
            break;

        case 1:
            return "Soft Error";

        case 2:
            return "Not Ready";

        case 3:
            return "Medium Error";

        case 4:
            return "Hard Error";

        case 5:
            return "Illegal Request";

        case 6:
            return "Unit Attention";

        case 7:
            return "Write Protected";

        case 0xb:
            return "Aborted Command";
    }
    return "?";
}


/**
 * Logs an URB.
 *
 * Note that pUrb->pUsbIns, pUrb->VUsb.pDev and pUrb->VUsb.pDev->pUsbIns can all be NULL.
 */
void vusbUrbTrace(PVUSBURB pUrb, const char *pszMsg, bool fComplete)
{
    PVUSBDEV        pDev   = pUrb->VUsb.pDev; /* Can be NULL when called from usbProxyConstruct and friends. */
    PVUSBPIPE       pPipe  = &pDev->aPipes[pUrb->EndPt];
    const uint8_t  *pbData = pUrb->abData;
    uint32_t        cbData = pUrb->cbData;
    PCVUSBSETUP     pSetup = NULL;
    bool            fDescriptors = false;
    static size_t   s_cchMaxMsg = 10;
    size_t          cchMsg = strlen(pszMsg);
    if (cchMsg > s_cchMaxMsg)
        s_cchMaxMsg = cchMsg;

    Log(("%s: %*s: pDev=%p[%s] rc=%s a=%i e=%u d=%s t=%s cb=%#x(%d) Ed=%08x cTds=%d Td0=%08x ts=%RU64 (%RU64 ns ago) %s\n",
         pUrb->pszDesc, s_cchMaxMsg, pszMsg,
         pDev,
         pUrb->pUsbIns ? pUrb->pUsbIns->pszName : "",
         vusbUrbStatusName(pUrb->enmStatus),
         pDev ? pDev->u8Address : -1,
         pUrb->EndPt,
         vusbUrbDirName(pUrb->enmDir),
         vusbUrbTypeName(pUrb->enmType),
         pUrb->cbData,
         pUrb->cbData,
         pUrb->Hci.EdAddr,
         pUrb->Hci.cTds,
         pUrb->Hci.cTds ? pUrb->Hci.paTds[0].TdAddr : ~(uint32_t)0,
         pUrb->VUsb.u64SubmitTS,
         RTTimeNanoTS() - pUrb->VUsb.u64SubmitTS,
         pUrb->fShortNotOk ? "ShortNotOk" : "ShortOk"));

#ifndef DEBUG_bird
    if (    pUrb->enmType   == VUSBXFERTYPE_CTRL
        &&  pUrb->enmStatus == VUSBSTATUS_OK)
        return;
#endif

    if (    pUrb->enmType == VUSBXFERTYPE_MSG
        ||  (   pUrb->enmDir  == VUSBDIRECTION_SETUP
             && pUrb->enmType == VUSBXFERTYPE_CTRL
             && cbData))
    {
        static const char * const s_apszReqDirs[]       = {"host2dev", "dev2host"};
        static const char * const s_apszReqTypes[]      = {"std", "class", "vendor", "reserved"};
        static const char * const s_apszReqRecipients[] = {"dev", "if", "endpoint", "other"};
        static const char * const s_apszRequests[] =
        {
            "GET_STATUS",        "CLEAR_FEATURE",     "2?",             "SET_FEATURE",
            "4?",                "SET_ADDRESS",       "GET_DESCRIPTOR", "SET_DESCRIPTOR",
            "GET_CONFIGURATION", "SET_CONFIGURATION", "GET_INTERFACE",  "SET_INTERFACE",
            "SYNCH_FRAME"
        };
        pSetup = (PVUSBSETUP)pUrb->abData;
        pbData += sizeof(*pSetup);
        cbData -= sizeof(*pSetup);

        Log(("%s: %*s: CTRL: bmRequestType=0x%.2x (%s %s %s) bRequest=0x%.2x (%s) wValue=0x%.4x wIndex=0x%.4x wLength=0x%.4x\n",
             pUrb->pszDesc, s_cchMaxMsg, pszMsg,
             pSetup->bmRequestType, s_apszReqDirs[pSetup->bmRequestType >> 7], s_apszReqTypes[(pSetup->bmRequestType >> 5) & 0x3],
             (unsigned)(pSetup->bmRequestType & 0xf) < RT_ELEMENTS(s_apszReqRecipients) ? s_apszReqRecipients[pSetup->bmRequestType & 0xf] : "??",
             pSetup->bRequest, pSetup->bRequest < RT_ELEMENTS(s_apszRequests) ? s_apszRequests[pSetup->bRequest] : "??",
             pSetup->wValue, pSetup->wIndex, pSetup->wLength));

        if (    pSetup->bRequest == VUSB_REQ_GET_DESCRIPTOR
            &&  fComplete
            &&  pUrb->enmStatus == VUSBSTATUS_OK
            &&  ((pSetup->bmRequestType >> 5) & 0x3) < 2 /* vendor */)
            fDescriptors = true;
    }
    else if (   fComplete
             && pUrb->enmDir == VUSBDIRECTION_IN
             && pUrb->enmType == VUSBXFERTYPE_CTRL
             && pUrb->enmStatus == VUSBSTATUS_OK
             && pPipe->pCtrl
             && pPipe->pCtrl->enmStage == CTLSTAGE_DATA
             && cbData > 0)
    {
        pSetup = pPipe->pCtrl->pMsg;
        if (pSetup->bRequest == VUSB_REQ_GET_DESCRIPTOR)
            fDescriptors = true;
    }

    /*
     * Dump descriptors.
     */
    if (fDescriptors)
    {
        const uint8_t *pb = pbData;
        const uint8_t *pbEnd = pbData + cbData;
        while (pb + 1 < pbEnd)
        {
            const unsigned  cbLeft = pbEnd - pb;
            const unsigned  cbLength = *pb;
            unsigned        cb = cbLength;
            uint8_t         bDescriptorType = pb[1];

            /* length out of bounds? */
            if (cbLength > cbLeft)
            {
                cb = cbLeft;
                if (cbLength != 0xff) /* ignore this */
                    Log(("URB: %*s: DESC: warning descriptor length goes beyond the end of the URB! cbLength=%d cbLeft=%d\n",
                         s_cchMaxMsg, pszMsg, cbLength, cbLeft));
            }

            if (cb >= 2)
            {
                Log(("URB: %*s: DESC: %04x: %25s = %#04x (%d)\n"
                     "URB: %*s:       %04x: %25s = %#04x (",
                     s_cchMaxMsg, pszMsg, pb - pbData, "bLength", cbLength, cbLength,
                     s_cchMaxMsg, pszMsg, pb - pbData + 1, "bDescriptorType", bDescriptorType));

                #pragma pack(1)
                #define BYTE_FIELD(strct, memb) \
                    if ((unsigned)RT_OFFSETOF(strct, memb) < cb) \
                        Log(("URB: %*s:       %04x: %25s = %#04x\n", s_cchMaxMsg, pszMsg, \
                             pb + RT_OFFSETOF(strct, memb) - pbData, #memb, pb[RT_OFFSETOF(strct, memb)]))
                #define BYTE_FIELD_START(strct, memb) do { \
                    if ((unsigned)RT_OFFSETOF(strct, memb) < cb) \
                    { \
                        Log(("URB: %*s:       %04x: %25s = %#04x", s_cchMaxMsg, pszMsg, \
                             pb + RT_OFFSETOF(strct, memb) - pbData, #memb, pb[RT_OFFSETOF(strct, memb)]))
                #define BYTE_FIELD_END(strct, memb) \
                        Log(("\n")); \
                    } } while (0)
                #define WORD_FIELD(strct, memb) \
                    if ((unsigned)RT_OFFSETOF(strct, memb) + 1 < cb) \
                        Log(("URB: %*s:       %04x: %25s = %#06x\n", s_cchMaxMsg, pszMsg, \
                             pb + RT_OFFSETOF(strct, memb) - pbData, #memb, *(uint16_t *)&pb[RT_OFFSETOF(strct, memb)]))
                #define BCD_FIELD(strct, memb) \
                    if ((unsigned)RT_OFFSETOF(strct, memb) + 1 < cb) \
                        Log(("URB: %*s:       %04x: %25s = %#06x (%02x.%02x)\n", s_cchMaxMsg, pszMsg, \
                             pb + RT_OFFSETOF(strct, memb) - pbData, #memb, *(uint16_t *)&pb[RT_OFFSETOF(strct, memb)], \
                             pb[RT_OFFSETOF(strct, memb) + 1], pb[RT_OFFSETOF(strct, memb)]))
                #define SIZE_CHECK(strct) \
                    if (cb > sizeof(strct)) \
                        Log(("URB: %*s:       %04x: WARNING %d extra byte(s) %.*Rhxs\n", s_cchMaxMsg, pszMsg, \
                             pb + sizeof(strct) - pbData, cb - sizeof(strct), cb - sizeof(strct), pb + sizeof(strct))); \
                    else if (cb < sizeof(strct)) \
                        Log(("URB: %*s:       %04x: WARNING %d missing byte(s)! Expected size %d.\n", s_cchMaxMsg, pszMsg, \
                             pb + cb - pbData, sizeof(strct) - cb, sizeof(strct)))

                /* on type */
                switch (bDescriptorType)
                {
                    case VUSB_DT_DEVICE:
                    {
                        struct dev_desc
                        {
                            uint8_t  bLength;
                            uint8_t  bDescriptorType;
                            uint16_t bcdUSB;
                            uint8_t  bDeviceClass;
                            uint8_t  bDeviceSubClass;
                            uint8_t  bDeviceProtocol;
                            uint8_t  bMaxPacketSize0;
                            uint16_t idVendor;
                            uint16_t idProduct;
                            uint16_t bcdDevice;
                            uint8_t  iManufacturer;
                            uint8_t  iProduct;
                            uint8_t  iSerialNumber;
                            uint8_t  bNumConfigurations;
                        } *pDesc = (struct dev_desc *)pb; NOREF(pDesc);
                        Log(("DEV)\n"));
                        BCD_FIELD( struct dev_desc, bcdUSB);
                        BYTE_FIELD(struct dev_desc, bDeviceClass);
                        BYTE_FIELD(struct dev_desc, bDeviceSubClass);
                        BYTE_FIELD(struct dev_desc, bDeviceProtocol);
                        BYTE_FIELD(struct dev_desc, bMaxPacketSize0);
                        WORD_FIELD(struct dev_desc, idVendor);
                        WORD_FIELD(struct dev_desc, idProduct);
                        BCD_FIELD( struct dev_desc, bcdDevice);
                        BYTE_FIELD(struct dev_desc, iManufacturer);
                        BYTE_FIELD(struct dev_desc, iProduct);
                        BYTE_FIELD(struct dev_desc, iSerialNumber);
                        BYTE_FIELD(struct dev_desc, bNumConfigurations);
                        SIZE_CHECK(struct dev_desc);
                        break;
                    }

                    case VUSB_DT_CONFIG:
                    {
                        struct cfg_desc
                        {
                            uint8_t  bLength;
                            uint8_t  bDescriptorType;
                            uint16_t wTotalLength;
                            uint8_t  bNumInterfaces;
                            uint8_t  bConfigurationValue;
                            uint8_t  iConfiguration;
                            uint8_t  bmAttributes;
                            uint8_t  MaxPower;
                        } *pDesc = (struct cfg_desc *)pb; NOREF(pDesc);
                        Log(("CFG)\n"));
                        WORD_FIELD(struct cfg_desc, wTotalLength);
                        BYTE_FIELD(struct cfg_desc, bNumInterfaces);
                        BYTE_FIELD(struct cfg_desc, bConfigurationValue);
                        BYTE_FIELD(struct cfg_desc, iConfiguration);
                        BYTE_FIELD_START(struct cfg_desc, bmAttributes);
                            static const char * const s_apszTransType[4] = { "Control", "Isochronous", "Bulk", "Interrupt" };
                            static const char * const s_apszSyncType[4]  = { "NoSync", "Asynchronous", "Adaptive", "Synchronous" };
                            static const char * const s_apszUsageType[4] = { "Data ep", "Feedback ep.", "Implicit feedback Data ep.", "Reserved" };
                            Log((" %s - %s - %s", s_apszTransType[(pDesc->bmAttributes & 0x3)],
                                 s_apszSyncType[((pDesc->bmAttributes >> 2) & 0x3)], s_apszUsageType[((pDesc->bmAttributes >> 4) & 0x3)]));
                        BYTE_FIELD_END(struct cfg_desc, bmAttributes);
                        BYTE_FIELD(struct cfg_desc, MaxPower);
                        SIZE_CHECK(struct cfg_desc);
                        break;
                    }

                    case VUSB_DT_STRING:
                        if (!pSetup->wIndex)
                        {
                            /* langid array */
                            uint16_t *pu16 = (uint16_t *)pb + 1;
                            Log(("LANGIDs)\n"));
                            while ((uintptr_t)pu16 + 2 - (uintptr_t)pb <= cb)
                            {
                                Log(("URB: %*s:       %04x: wLANGID[%#x] = %#06x\n",
                                     s_cchMaxMsg, pszMsg, (uint8_t *)pu16 - pbData, pu16 - (uint16_t *)pb, *pu16));
                                pu16++;
                            }
                            if (cb & 1)
                                Log(("URB: %*s:       %04x: WARNING descriptor size is odd! extra byte: %02\n",
                                     s_cchMaxMsg, pszMsg, (uint8_t *)pu16 - pbData, *(uint8_t *)pu16));
                        }
                        else
                        {
                            /** a string. */
                            Log(("STRING)\n"));
                            if (cb > 2)
                                Log(("URB: %*s:       %04x: Length=%d String=%.*ls\n",
                                     s_cchMaxMsg, pszMsg, pb - pbData, cb - 2, cb / 2 - 1, pb + 2));
                            else
                                Log(("URB: %*s:       %04x: Length=0!\n", s_cchMaxMsg, pszMsg, pb - pbData));
                        }
                        break;

                    case VUSB_DT_INTERFACE:
                    {
                        struct if_desc
                        {
                            uint8_t  bLength;
                            uint8_t  bDescriptorType;
                            uint8_t  bInterfaceNumber;
                            uint8_t  bAlternateSetting;
                            uint8_t  bNumEndpoints;
                            uint8_t  bInterfaceClass;
                            uint8_t  bInterfaceSubClass;
                            uint8_t  bInterfaceProtocol;
                            uint8_t  iInterface;
                        } *pDesc = (struct if_desc *)pb; NOREF(pDesc);
                        Log(("IF)\n"));
                        BYTE_FIELD(struct if_desc, bInterfaceNumber);
                        BYTE_FIELD(struct if_desc, bAlternateSetting);
                        BYTE_FIELD(struct if_desc, bNumEndpoints);
                        BYTE_FIELD(struct if_desc, bInterfaceClass);
                        BYTE_FIELD(struct if_desc, bInterfaceSubClass);
                        BYTE_FIELD(struct if_desc, bInterfaceProtocol);
                        BYTE_FIELD(struct if_desc, iInterface);
                        SIZE_CHECK(struct if_desc);
                        break;
                    }

                    case VUSB_DT_ENDPOINT:
                    {
                        struct ep_desc
                        {
                            uint8_t  bLength;
                            uint8_t  bDescriptorType;
                            uint8_t  bEndpointAddress;
                            uint8_t  bmAttributes;
                            uint16_t wMaxPacketSize;
                            uint8_t  bInterval;
                        } *pDesc = (struct ep_desc *)pb; NOREF(pDesc);
                        Log(("EP)\n"));
                        BYTE_FIELD(struct ep_desc, bEndpointAddress);
                        BYTE_FIELD(struct ep_desc, bmAttributes);
                        WORD_FIELD(struct ep_desc, wMaxPacketSize);
                        BYTE_FIELD(struct ep_desc, bInterval);
                        SIZE_CHECK(struct ep_desc);
                        break;
                    }

                    case VUSB_DT_DEVICE_QUALIFIER:
                    {
                        struct dq_desc
                        {
                            uint8_t  bLength;
                            uint8_t  bDescriptorType;
                            uint16_t bcdUSB;
                            uint8_t  bDeviceClass;
                            uint8_t  bDeviceSubClass;
                            uint8_t  bDeviceProtocol;
                            uint8_t  bMaxPacketSize0;
                            uint8_t  bNumConfigurations;
                            uint8_t  bReserved;
                        } *pDQDesc = (struct dq_desc *)pb; NOREF(pDQDesc);
                        Log(("DEVQ)\n"));
                        BCD_FIELD( struct dq_desc, bcdUSB);
                        BYTE_FIELD(struct dq_desc, bDeviceClass);
                        BYTE_FIELD(struct dq_desc, bDeviceSubClass);
                        BYTE_FIELD(struct dq_desc, bDeviceProtocol);
                        BYTE_FIELD(struct dq_desc, bMaxPacketSize0);
                        BYTE_FIELD(struct dq_desc, bNumConfigurations);
                        BYTE_FIELD(struct dq_desc, bReserved);
                        SIZE_CHECK(struct dq_desc);
                        break;
                    }

                    case VUSB_DT_OTHER_SPEED_CFG:
                    {
                        struct oth_cfg_desc
                        {
                            uint8_t  bLength;
                            uint8_t  bDescriptorType;
                            uint16_t wTotalLength;
                            uint8_t  bNumInterfaces;
                            uint8_t  bConfigurationValue;
                            uint8_t  iConfiguration;
                            uint8_t  bmAttributes;
                            uint8_t  MaxPower;
                        } *pDesc = (struct oth_cfg_desc *)pb; NOREF(pDesc);
                        Log(("OCFG)\n"));
                        WORD_FIELD(struct oth_cfg_desc, wTotalLength);
                        BYTE_FIELD(struct oth_cfg_desc, bNumInterfaces);
                        BYTE_FIELD(struct oth_cfg_desc, bConfigurationValue);
                        BYTE_FIELD(struct oth_cfg_desc, iConfiguration);
                        BYTE_FIELD_START(struct oth_cfg_desc, bmAttributes);
                            static const char * const s_apszTransType[4] = { "Control", "Isochronous", "Bulk", "Interrupt" };
                            static const char * const s_apszSyncType[4]  = { "NoSync", "Asynchronous", "Adaptive", "Synchronous" };
                            static const char * const s_apszUsageType[4] = { "Data ep", "Feedback ep.", "Implicit feedback Data ep.", "Reserved" };
                            Log((" %s - %s - %s", s_apszTransType[(pDesc->bmAttributes & 0x3)],
                                 s_apszSyncType[((pDesc->bmAttributes >> 2) & 0x3)], s_apszUsageType[((pDesc->bmAttributes >> 4) & 0x3)]));
                        BYTE_FIELD_END(struct oth_cfg_desc, bmAttributes);
                        BYTE_FIELD(struct oth_cfg_desc, MaxPower);
                        SIZE_CHECK(struct oth_cfg_desc);
                        break;
                    }

                    case 0x21:
                    {
                        struct hid_desc
                        {
                            uint8_t  bLength;
                            uint8_t  bDescriptorType;
                            uint16_t bcdHid;
                            uint8_t  bCountry;
                            uint8_t  bNumDescriptors;
                            uint8_t  bReportType;
                            uint16_t wReportLength;
                        } *pDesc = (struct hid_desc *)pb; NOREF(pDesc);
                        Log(("EP)\n"));
                        BCD_FIELD( struct hid_desc, bcdHid);
                        BYTE_FIELD(struct hid_desc, bCountry);
                        BYTE_FIELD(struct hid_desc, bNumDescriptors);
                        BYTE_FIELD(struct hid_desc, bReportType);
                        WORD_FIELD(struct hid_desc, wReportLength);
                        SIZE_CHECK(struct hid_desc);
                        break;
                    }

                    case 0xff:
                        Log(("UNKNOWN-ignore)\n"));
                        break;

                    default:
                        Log(("UNKNOWN)!!!\n"));
                        break;
                }

                #undef BYTE_FIELD
                #undef WORD_FIELD
                #undef BCD_FIELD
                #undef SIZE_CHECK
                #pragma pack()
            }
            else
            {
                Log(("URB: %*s: DESC: %04x: bLength=%d bDescriptorType=%d - invalid length\n",
                     s_cchMaxMsg, pszMsg, pb - pbData, cb, bDescriptorType));
                break;
            }

            /* next */
            pb += cb;
        }
    }

    /*
     * SCSI
     */
    if (    pUrb->enmType == VUSBXFERTYPE_BULK
        &&  pUrb->enmDir  == VUSBDIRECTION_OUT
        &&  pUrb->cbData >= 12
        &&  !memcmp(pUrb->abData, "USBC", 4))
    {
        const struct usbc
        {
            uint32_t    Signature;
            uint32_t    Tag;
            uint32_t    DataTransferLength;
            uint8_t     Flags;
            uint8_t     Lun;
            uint8_t     Length;
            uint8_t     CDB[13];
        } *pUsbC = (struct usbc *)pUrb->abData;
        Log(("URB: %*s: SCSI: Tag=%#x DataTransferLength=%#x Flags=%#x Lun=%#x Length=%#x CDB=%.*Rhxs\n",
             s_cchMaxMsg, pszMsg, pUsbC->Tag, pUsbC->DataTransferLength, pUsbC->Flags, pUsbC->Lun,
             pUsbC->Length, pUsbC->Length, pUsbC->CDB));
        const uint8_t *pb = &pUsbC->CDB[0];
        switch (pb[0])
        {
            case 0x00: /* test unit read */
                Log(("URB: %*s: SCSI: TEST_UNIT_READY LUN=%d Ctrl=%#RX8\n",
                     s_cchMaxMsg, pszMsg, pb[1] >> 5, pb[5]));
                break;
            case 0x03: /* Request Sense command */
                Log(("URB: %*s: SCSI: REQUEST_SENSE LUN=%d AlcLen=%#RX16 Ctrl=%#RX8\n",
                     s_cchMaxMsg, pszMsg, pb[1] >> 5, pb[4], pb[5]));
                break;
            case 0x12: /* Inquiry command. */
                Log(("URB: %*s: SCSI: INQUIRY EVPD=%d LUN=%d PgCd=%#RX8 AlcLen=%#RX8 Ctrl=%#RX8\n",
                     s_cchMaxMsg, pszMsg, pb[1] & 1, pb[1] >> 5, pb[2], pb[4], pb[5]));
                break;
            case 0x1a: /* Mode Sense(6) command */
                Log(("URB: %*s: SCSI: MODE_SENSE6 LUN=%d DBD=%d PC=%d PgCd=%#RX8 AlcLen=%#RX8 Ctrl=%#RX8\n",
                     s_cchMaxMsg, pszMsg, pb[1] >> 5, !!(pb[1] & RT_BIT(3)), pb[2] >> 6, pb[2] & 0x3f, pb[4], pb[5]));
                break;
            case 0x5a:
                Log(("URB: %*s: SCSI: MODE_SENSE10 LUN=%d DBD=%d PC=%d PgCd=%#RX8 AlcLen=%#RX16 Ctrl=%#RX8\n",
                     s_cchMaxMsg, pszMsg, pb[1] >> 5, !!(pb[1] & RT_BIT(3)), pb[2] >> 6, pb[2] & 0x3f,
                     RT_MAKE_U16(pb[8], pb[7]), pb[9]));
                break;
            case 0x25: /* Read Capacity(6) command. */
                Log(("URB: %*s: SCSI: READ_CAPACITY\n",
                     s_cchMaxMsg, pszMsg));
                break;
            case 0x28: /* Read(10) command. */
                Log(("URB: %*s: SCSI: READ10 RelAdr=%d FUA=%d DPO=%d LUN=%d LBA=%#RX32 Len=%#RX16 Ctrl=%#RX8\n",
                     s_cchMaxMsg, pszMsg,
                     pb[1] & 1, !!(pb[1] & RT_BIT(3)), !!(pb[1] & RT_BIT(4)), pb[1] >> 5,
                     RT_MAKE_U32_FROM_U8(pb[5], pb[4], pb[3], pb[2]),
                     RT_MAKE_U16(pb[8], pb[7]), pb[9]));
                break;
            case 0xa8: /* Read(12) command. */
                Log(("URB: %*s: SCSI: READ12 RelAdr=%d FUA=%d DPO=%d LUN=%d LBA=%#RX32 Len=%#RX32 Ctrl=%#RX8\n",
                     s_cchMaxMsg, pszMsg,
                     pb[1] & 1, !!(pb[1] & RT_BIT(3)), !!(pb[1] & RT_BIT(4)), pb[1] >> 5,
                     RT_MAKE_U32_FROM_U8(pb[5], pb[4], pb[3], pb[2]),
                     RT_MAKE_U32_FROM_U8(pb[9], pb[8], pb[7], pb[6]),
                     pb[11]));
                break;
            case 0x3e: /* Read Long command. */
                Log(("URB: %*s: SCSI: READ LONG RelAdr=%d Correct=%d LUN=%d LBA=%#RX16 ByteLen=%#RX16 Ctrl=%#RX8\n",
                     s_cchMaxMsg, pszMsg,
                     pb[1] & 1, !!(pb[1] & RT_BIT(1)),  pb[1] >> 5,
                     RT_MAKE_U16(pb[3], pb[2]), RT_MAKE_U16(pb[6], pb[5]),
                     pb[11]));
                break;
            case 0x2a: /* Write(10) command. */
                Log(("URB: %*s: SCSI: WRITE10 RelAdr=%d EBP=%d FUA=%d DPO=%d LUN=%d LBA=%#RX32 Len=%#RX16 Ctrl=%#RX8\n",
                     s_cchMaxMsg, pszMsg,
                     pb[1] & 1, !!(pb[1] & RT_BIT(2)), !!(pb[1] & RT_BIT(3)),
                     !!(pb[1] & RT_BIT(4)), pb[1] >> 5,
                     RT_MAKE_U32_FROM_U8(pb[5], pb[4], pb[3], pb[2]),
                     RT_MAKE_U16(pb[8], pb[7]), pb[9]));
                break;
            case 0xaa: /* Write(12) command. */
                Log(("URB: %*s: SCSI: WRITE12 RelAdr=%d EBP=%d FUA=%d DPO=%d LUN=%d LBA=%#RX32 Len=%#RX32 Ctrl=%#RX8\n",
                     s_cchMaxMsg, pszMsg,
                     pb[1] & 1, !!(pb[1] & RT_BIT(3)), !!(pb[1] & RT_BIT(4)),
                     !!(pb[1] & RT_BIT(4)), pb[1] >> 5,
                     RT_MAKE_U32_FROM_U8(pb[5], pb[4], pb[3], pb[2]),
                     RT_MAKE_U32_FROM_U8(pb[9], pb[8], pb[7], pb[6]),
                     pb[11]));
                break;
            case 0x3f: /* Write Long command. */
                Log(("URB: %*s: SCSI: WRITE LONG RelAdr=%d LUN=%d LBA=%#RX16 ByteLen=%#RX16 Ctrl=%#RX8\n",
                     s_cchMaxMsg, pszMsg,
                     pb[1] & 1,  pb[1] >> 5,
                     RT_MAKE_U16(pb[3], pb[2]), RT_MAKE_U16(pb[6], pb[5]),
                     pb[11]));
                break;
            case 0x35: /* Synchronize Cache(10) command. */
                Log(("URB: %*s: SCSI: SYNCHRONIZE_CACHE10\n",
                     s_cchMaxMsg, pszMsg));
                break;
            case 0xa0: /* Report LUNs command. */
                Log(("URB: %*s: SCSI: REPORT_LUNS\n",
                     s_cchMaxMsg, pszMsg));
                break;
            default:
                Log(("URB: %*s: SCSI: cmd=%#x\n",
                     s_cchMaxMsg, pszMsg, pb[0]));
                break;
        }
        if (pDev)
            pDev->Urb.u8ScsiCmd = pb[0];
    }
    else if (   fComplete
             && pUrb->enmType == VUSBXFERTYPE_BULK
             && pUrb->enmDir  == VUSBDIRECTION_IN
             && pUrb->cbData >= 12
             && !memcmp(pUrb->abData, "USBS", 4))
    {
        const struct usbs
        {
            uint32_t    Signature;
            uint32_t    Tag;
            uint32_t    DataResidue;
            uint8_t     Status;
            uint8_t     CDB[3];
        } *pUsbS = (struct usbs *)pUrb->abData;
        static const char * const s_apszStatuses[] = { "PASSED", "FAILED", "PHASE ERROR", "RESERVED" };
        Log(("URB: %*s: SCSI: Tag=%#x DataResidue=%#RX32 Status=%#RX8 %s\n",
             s_cchMaxMsg, pszMsg, pUsbS->Tag, pUsbS->DataResidue, pUsbS->Status,
             s_apszStatuses[pUsbS->Status < RT_ELEMENTS(s_apszStatuses) ? pUsbS->Status : RT_ELEMENTS(s_apszStatuses) - 1]));
        if (pDev)
            pDev->Urb.u8ScsiCmd = 0xff;
    }
    else if (   fComplete
             && pUrb->enmType == VUSBXFERTYPE_BULK
             && pUrb->enmDir  == VUSBDIRECTION_IN
             && pDev
             && pDev->Urb.u8ScsiCmd != 0xff)
    {
        const uint8_t *pb = pUrb->abData;
        switch (pDev->Urb.u8ScsiCmd)
        {
            case 0x03: /* REQUEST_SENSE */
                Log(("URB: %*s: SCSI: RESPONSE: REQUEST_SENSE (%s)\n",
                     s_cchMaxMsg, pszMsg, pb[0] & 7 ? "scsi compliant" : "not scsi compliant"));
                Log(("URB: %*s: SCSI: ErrCd=%#RX8 (%s) Seg=%#RX8 Filemark=%d EOM=%d ILI=%d\n",
                     s_cchMaxMsg, pszMsg, pb[0] & 0x7f, GetScsiErrCd(pb[0] & 0x7f), pb[1],
                     pb[2] >> 7, !!(pb[2] & RT_BIT(6)), !!(pb[2] & RT_BIT(5))));
                Log(("URB: %*s: SCSI: SenseKey=%#x ASC=%#RX8 ASCQ=%#RX8 : %s\n",
                     s_cchMaxMsg, pszMsg, pb[2] & 0xf, pb[12], pb[13],
                     GetScsiKCQ(pb[2] & 0xf, pb[12], pb[13])));
                /** @todo more later */
                break;

            case 0x12: /* INQUIRY. */
            {
                unsigned cb = pb[4] + 5;
                Log(("URB: %*s: SCSI: RESPONSE: INQUIRY\n"
                     "URB: %*s: SCSI: PeripheralQualifier=%d PeripheralType=%#RX8 RMB=%d DevTypeMod=%#RX8\n",
                     s_cchMaxMsg, pszMsg, s_cchMaxMsg, pszMsg,
                     pb[0] >> 5, pb[0] & 0x1f, pb[1] >> 7, pb[1] & 0x7f));
                Log(("URB: %*s: SCSI: ISOVer=%d ECMAVer=%d ANSIVer=%d\n",
                     s_cchMaxMsg, pszMsg, pb[2] >> 6, (pb[2] >> 3) & 7, pb[2] & 7));
                Log(("URB: %*s: SCSI: AENC=%d TrmlOP=%d RespDataFmt=%d (%s) AddLen=%d\n",
                     s_cchMaxMsg, pszMsg, pb[3] >> 7, (pb[3] >> 6) & 1,
                     pb[3] & 0xf, pb[3] & 0xf ? "legacy" : "scsi", pb[4]));
                if (cb < 8)
                    break;
                Log(("URB: %*s: SCSI: RelAdr=%d WBus32=%d WBus16=%d Sync=%d Linked=%d CmdQue=%d SftRe=%d\n",
                     s_cchMaxMsg, pszMsg, pb[7] >> 7, !!(pb[7] >> 6), !!(pb[7] >> 5), !!(pb[7] >> 4),
                     !!(pb[7] >> 3), !!(pb[7] >> 1), pb[7] & 1));
                if (cb < 16)
                    break;
                Log(("URB: %*s: SCSI: VendorId=%.8s\n", s_cchMaxMsg, pszMsg, &pb[8]));
                if (cb < 32)
                    break;
                Log(("URB: %*s: SCSI: ProductId=%.16s\n", s_cchMaxMsg, pszMsg, &pb[16]));
                if (cb < 36)
                    break;
                Log(("URB: %*s: SCSI: ProdRevLvl=%.4s\n", s_cchMaxMsg, pszMsg, &pb[32]));
                if (cb > 36)
                    Log(("URB: %*s: SCSI: VendorSpecific=%.*s\n",
                         s_cchMaxMsg, pszMsg, RT_MIN(cb - 36, 20), &pb[36]));
                if (cb > 96)
                    Log(("URB: %*s: SCSI: VendorParam=%.*Rhxs\n",
                         s_cchMaxMsg, pszMsg, cb - 96, &pb[96]));
                break;
            }

            case 0x25: /* Read Capacity(6) command. */
                Log(("URB: %*s: SCSI: RESPONSE: READ_CAPACITY\n"
                     "URB: %*s: SCSI: LBA=%#RX32 BlockLen=%#RX32\n",
                     s_cchMaxMsg, pszMsg, s_cchMaxMsg, pszMsg,
                     RT_MAKE_U32_FROM_U8(pb[3], pb[2], pb[1], pb[0]),
                     RT_MAKE_U32_FROM_U8(pb[7], pb[6], pb[5], pb[4])));
                break;
        }

        pDev->Urb.u8ScsiCmd = 0xff;
    }

    /*
     * The Quickcam control pipe.
     */
    if (    pSetup
        &&  ((pSetup->bmRequestType >> 5) & 0x3) >= 2 /* vendor */
        &&  (fComplete || !(pSetup->bmRequestType >> 7))
        &&  pDev
        &&  pDev->pDescCache->pDevice
        &&  pDev->pDescCache->pDevice->idVendor == 0x046d
        &&  (   pDev->pDescCache->pDevice->idProduct == 0x8f6
             || pDev->pDescCache->pDevice->idProduct == 0x8f5
             || pDev->pDescCache->pDevice->idProduct == 0x8f0)
       )
    {
        pbData = (const uint8_t *)(pSetup + 1);
        cbData = pUrb->cbData - sizeof(*pSetup);

        if (    pSetup->bRequest == 0x04
            &&  pSetup->wIndex == 0
            &&  (cbData == 1 || cbData == 2))
        {
            /* the value */
            unsigned uVal = pbData[0];
            if (cbData > 1)
                uVal |= (unsigned)pbData[1] << 8;

            const char *pszReg = NULL;
            switch (pSetup->wValue)
            {
                case 0:         pszReg = "i2c init"; break;
                case 0x0423:    pszReg = "STV_REG23"; break;
                case 0x0509:    pszReg = "RED something"; break;
                case 0x050a:    pszReg = "GREEN something"; break;
                case 0x050b:    pszReg = "BLUE something"; break;
                case 0x143f:    pszReg = "COMMIT? INIT DONE?"; break;
                case 0x1440:    pszReg = "STV_ISO_ENABLE"; break;
                case 0x1442:    pszReg = uVal & (RT_BIT(7)|RT_BIT(5)) ? "BUTTON PRESSED" : "BUTTON" ; break;
                case 0x1443:    pszReg = "STV_SCAN_RATE"; break;
                case 0x1445:    pszReg = "LED?"; break;
                case 0x1500:    pszReg = "STV_REG00"; break;
                case 0x1501:    pszReg = "STV_REG01"; break;
                case 0x1502:    pszReg = "STV_REG02"; break;
                case 0x1503:    pszReg = "STV_REG03"; break;
                case 0x1504:    pszReg = "STV_REG04"; break;
                case 0x15c1:    pszReg = "STV_ISO_SIZE"; break;
                case 0x15c3:    pszReg = "STV_Y_CTRL"; break;
                case 0x1680:    pszReg = "STV_X_CTRL"; break;
                case 0xe00a:    pszReg = "ProductId"; break;
                default:        pszReg = "[no clue]";   break;
            }
            if (pszReg)
                Log(("URB: %*s: QUICKCAM: %s %#x (%d) %s '%s' (%#x)\n",
                     s_cchMaxMsg, pszMsg,
                     (pSetup->bmRequestType >> 7) ? "read" : "write", uVal, uVal, (pSetup->bmRequestType >> 7) ? "from" : "to",
                     pszReg, pSetup->wValue));
        }
        else if (cbData)
            Log(("URB: %*s: QUICKCAM: Unknown request: bRequest=%#x bmRequestType=%#x wValue=%#x wIndex=%#x: %.*Rhxs\n", s_cchMaxMsg, pszMsg,
                 pSetup->bRequest, pSetup->bmRequestType, pSetup->wValue, pSetup->wIndex, cbData, pbData));
        else
            Log(("URB: %*s: QUICKCAM: Unknown request: bRequest=%#x bmRequestType=%#x wValue=%#x wIndex=%#x: (no data)\n", s_cchMaxMsg, pszMsg,
                 pSetup->bRequest, pSetup->bmRequestType, pSetup->wValue, pSetup->wIndex));
    }

#if 1
    if (    cbData /** @todo Fix RTStrFormatV to communicate .* so formatter doesn't apply defaults when cbData=0. */
        && (fComplete
            ? pUrb->enmDir != VUSBDIRECTION_OUT
            : pUrb->enmDir == VUSBDIRECTION_OUT))
        Log3(("%16.*Rhxd\n", cbData, pbData));
#endif
    if (pUrb->enmType == VUSBXFERTYPE_MSG && pUrb->VUsb.pCtrlUrb)
        vusbUrbTrace(pUrb->VUsb.pCtrlUrb, "NESTED MSG", fComplete);
}
#endif /* LOG_ENABLED */


/**
 * Complete a SETUP stage URB.
 *
 * This is used both for dev2host and host2dev kind of transfers.
 * It is used by both the sync and async control paths.
 */
static void vusbMsgSetupCompletion(PVUSBURB pUrb)
{
    PVUSBDEV        pDev   = pUrb->VUsb.pDev;
    PVUSBPIPE       pPipe  = &pDev->aPipes[pUrb->EndPt];
    PVUSBCTRLEXTRA  pExtra = pPipe->pCtrl;
    PVUSBSETUP      pSetup = pExtra->pMsg;

    LogFlow(("%s: vusbMsgSetupCompletion: cbData=%d wLength=%#x cbLeft=%d pPipe=%p stage %s->DATA\n",
             pUrb->pszDesc, pUrb->cbData, pSetup->wLength, pExtra->cbLeft, pPipe, g_apszCtlStates[pExtra->enmStage])); NOREF(pSetup);
    pExtra->enmStage = CTLSTAGE_DATA;
    pUrb->enmStatus  = VUSBSTATUS_OK;
}

/**
 * Complete a DATA stage URB.
 *
 * This is used both for dev2host and host2dev kind of transfers.
 * It is used by both the sync and async control paths.
 */
static void vusbMsgDataCompletion(PVUSBURB pUrb)
{
    PVUSBDEV        pDev   = pUrb->VUsb.pDev;
    PVUSBPIPE       pPipe  = &pDev->aPipes[pUrb->EndPt];
    PVUSBCTRLEXTRA  pExtra = pPipe->pCtrl;
    PVUSBSETUP      pSetup = pExtra->pMsg;

    LogFlow(("%s: vusbMsgDataCompletion: cbData=%d wLength=%#x cbLeft=%d pPipe=%p stage DATA\n",
             pUrb->pszDesc, pUrb->cbData, pSetup->wLength, pExtra->cbLeft, pPipe)); NOREF(pSetup);

    pUrb->enmStatus = VUSBSTATUS_OK;
}

/**
 * Complete a STATUS stage URB.
 *
 * This is used both for dev2host and host2dev kind of transfers.
 * It is used by both the sync and async control paths.
 */
static void vusbMsgStatusCompletion(PVUSBURB pUrb)
{
    PVUSBDEV        pDev = pUrb->VUsb.pDev;
    PVUSBPIPE       pPipe = &pDev->aPipes[pUrb->EndPt];
    PVUSBCTRLEXTRA  pExtra = pPipe->pCtrl;

    if (pExtra->fOk)
    {
        /*
         * vusbDevStdReqSetAddress requests are deferred.
         */
        if (pDev->u8NewAddress != VUSB_INVALID_ADDRESS)
        {
            vusbDevSetAddress(pDev, pDev->u8NewAddress);
            pDev->u8NewAddress = VUSB_INVALID_ADDRESS;
        }

        LogFlow(("%s: vusbMsgStatusCompletion: pDev=%p[%s] pPipe=%p err=OK stage %s->SETUP\n",
                 pUrb->pszDesc, pDev, pDev->pUsbIns->pszName, pPipe, g_apszCtlStates[pExtra->enmStage]));
        pUrb->enmStatus = VUSBSTATUS_OK;
    }
    else
    {
        LogFlow(("%s: vusbMsgStatusCompletion: pDev=%p[%s] pPipe=%p err=STALL stage %s->SETUP\n",
                 pUrb->pszDesc, pDev, pDev->pUsbIns->pszName, pPipe, g_apszCtlStates[pExtra->enmStage]));
        pUrb->enmStatus = VUSBSTATUS_STALL;
    }

    /*
     * Done with this message sequence.
     */
    pExtra->pbCur    = NULL;
    pExtra->enmStage = CTLSTAGE_SETUP;
}

/**
 * This is a worker function for vusbMsgCompletion and
 * vusbMsgSubmitSynchronously used to complete the original URB.
 *
 * @param   pUrb    The URB originating from the HCI.
 */
static void vusbCtrlCompletion(PVUSBURB pUrb)
{
    PVUSBDEV        pDev = pUrb->VUsb.pDev;
    PVUSBPIPE       pPipe = &pDev->aPipes[pUrb->EndPt];
    PVUSBCTRLEXTRA  pExtra = pPipe->pCtrl;
    LogFlow(("%s: vusbCtrlCompletion: pDev=%p[%s]\n", pUrb->pszDesc, pDev, pDev->pUsbIns->pszName));

    switch (pExtra->enmStage)
    {
        case CTLSTAGE_SETUP:
            vusbMsgSetupCompletion(pUrb);
            break;
        case CTLSTAGE_DATA:
            vusbMsgDataCompletion(pUrb);
            break;
        case CTLSTAGE_STATUS:
            vusbMsgStatusCompletion(pUrb);
            break;
    }
    vusbUrbCompletionRh(pUrb);
}

/**
 * Called from vusbUrbCompletionRh when it encounters a
 * message type URB.
 *
 * @param   pUrb    The URB within the control pipe extra state data.
 */
static void vusbMsgCompletion(PVUSBURB pUrb)
{
    PVUSBDEV        pDev   = pUrb->VUsb.pDev;
    PVUSBPIPE       pPipe  = &pDev->aPipes[pUrb->EndPt];
    PVUSBCTRLEXTRA  pExtra = pPipe->pCtrl;

#ifdef LOG_ENABLED
    LogFlow(("%s: vusbMsgCompletion: pDev=%p[%s]\n", pUrb->pszDesc, pDev, pDev->pUsbIns->pszName));
    vusbUrbTrace(pUrb, "vusbMsgCompletion", true);
#endif
    Assert(&pExtra->Urb == pUrb);


    if (pUrb->enmStatus == VUSBSTATUS_OK)
        pExtra->fOk = true;
    else
        pExtra->fOk = false;
    pExtra->cbLeft = pUrb->cbData - sizeof(VUSBSETUP);

    /*
     * Complete the original URB.
     */
    PVUSBURB pCtrlUrb = pUrb->VUsb.pCtrlUrb;
    pCtrlUrb->enmState = VUSBURBSTATE_REAPED;
    vusbCtrlCompletion(pCtrlUrb);

    /*
     * 'Free' the message URB, i.e. put it back to the allocated state.
     */
    Assert(   pUrb->enmState == VUSBURBSTATE_REAPED
           || pUrb->enmState == VUSBURBSTATE_CANCELLED);
    if (pUrb->enmState != VUSBURBSTATE_CANCELLED)
        pUrb->enmState = VUSBURBSTATE_ALLOCATED;
}

/**
 * Deal with URB errors, talking thru the RH to the HCI.
 *
 * @returns true if it could be retried.
 * @returns false if it should be completed with failure.
 * @param   pUrb    The URB in question.
 */
static int vusbUrbErrorRh(PVUSBURB pUrb)
{
    PVUSBDEV pDev = pUrb->VUsb.pDev;
    PVUSBROOTHUB pRh = vusbDevGetRh(pDev);
    LogFlow(("%s: vusbUrbErrorRh: pDev=%p[%s] rh=%p\n", pUrb->pszDesc, pDev, pDev->pUsbIns ? pDev->pUsbIns->pszName : "", pRh));
    return pRh->pIRhPort->pfnXferError(pRh->pIRhPort, pUrb);
}

/**
 * Does URB completion on roothub level.
 *
 * @param   pUrb    The URB to complete.
 */
void vusbUrbCompletionRh(PVUSBURB pUrb)
{
    LogFlow(("%s: vusbUrbCompletionRh: type=%s status=%s\n",
             pUrb->pszDesc, vusbUrbTypeName(pUrb->enmType), vusbUrbStatusName(pUrb->enmStatus)));
    AssertMsg(   pUrb->enmState == VUSBURBSTATE_REAPED
              || pUrb->enmState == VUSBURBSTATE_CANCELLED, ("%d\n", pUrb->enmState));


#ifdef VBOX_WITH_STATISTICS
    /*
     * Total and per-type submit statistics.
     */
    PVUSBROOTHUB pRh = vusbDevGetRh(pUrb->VUsb.pDev);
    if (pUrb->enmType != VUSBXFERTYPE_MSG)
    {
        Assert(pUrb->enmType >= 0 && pUrb->enmType < (int)RT_ELEMENTS(pRh->aTypes));

        if (    pUrb->enmStatus == VUSBSTATUS_OK
            ||  pUrb->enmStatus == VUSBSTATUS_DATA_UNDERRUN
            ||  pUrb->enmStatus == VUSBSTATUS_DATA_OVERRUN)
        {
            if (pUrb->enmType == VUSBXFERTYPE_ISOC)
            {
                for (unsigned i = 0; i < pUrb->cIsocPkts; i++)
                {
                    const unsigned cb = pUrb->aIsocPkts[i].cb;
                    if (cb)
                    {
                        STAM_COUNTER_ADD(&pRh->Total.StatActBytes, cb);
                        STAM_COUNTER_ADD(&pRh->aTypes[VUSBXFERTYPE_ISOC].StatActBytes, cb);
                        STAM_COUNTER_ADD(&pRh->aStatIsocDetails[i].Bytes, cb);
                        if (pUrb->enmDir == VUSBDIRECTION_IN)
                        {
                            STAM_COUNTER_ADD(&pRh->Total.StatActReadBytes, cb);
                            STAM_COUNTER_ADD(&pRh->aTypes[VUSBXFERTYPE_ISOC].StatActReadBytes, cb);
                        }
                        else
                        {
                            STAM_COUNTER_ADD(&pRh->Total.StatActWriteBytes, cb);
                            STAM_COUNTER_ADD(&pRh->aTypes[VUSBXFERTYPE_ISOC].StatActWriteBytes, cb);
                        }
                        STAM_COUNTER_INC(&pRh->StatIsocActPkts);
                        STAM_COUNTER_INC(&pRh->StatIsocActReadPkts);
                    }
                    STAM_COUNTER_INC(&pRh->aStatIsocDetails[i].Pkts);
                    switch (pUrb->aIsocPkts[i].enmStatus)
                    {
                        case VUSBSTATUS_OK:
                            if (cb)                     STAM_COUNTER_INC(&pRh->aStatIsocDetails[i].Ok);
                            else                        STAM_COUNTER_INC(&pRh->aStatIsocDetails[i].Ok0); break;
                        case VUSBSTATUS_DATA_UNDERRUN:
                            if (cb)                     STAM_COUNTER_INC(&pRh->aStatIsocDetails[i].DataUnderrun);
                            else                        STAM_COUNTER_INC(&pRh->aStatIsocDetails[i].DataUnderrun0); break;
                        case VUSBSTATUS_DATA_OVERRUN:   STAM_COUNTER_INC(&pRh->aStatIsocDetails[i].DataOverrun); break;
                        case VUSBSTATUS_NOT_ACCESSED:   STAM_COUNTER_INC(&pRh->aStatIsocDetails[i].NotAccessed); break;
                        default:                        STAM_COUNTER_INC(&pRh->aStatIsocDetails[i].Misc); break;
                    }
                }
            }
            else
            {
                STAM_COUNTER_ADD(&pRh->Total.StatActBytes, pUrb->cbData);
                STAM_COUNTER_ADD(&pRh->aTypes[pUrb->enmType].StatActBytes, pUrb->cbData);
                if (pUrb->enmDir == VUSBDIRECTION_IN)
                {
                    STAM_COUNTER_ADD(&pRh->Total.StatActReadBytes, pUrb->cbData);
                    STAM_COUNTER_ADD(&pRh->aTypes[pUrb->enmType].StatActReadBytes, pUrb->cbData);
                }
                else
                {
                    STAM_COUNTER_ADD(&pRh->Total.StatActWriteBytes, pUrb->cbData);
                    STAM_COUNTER_ADD(&pRh->aTypes[pUrb->enmType].StatActWriteBytes, pUrb->cbData);
                }
            }
        }
        else
        {
            /* (Note. this also counts the cancelled packets) */
            STAM_COUNTER_INC(&pRh->Total.StatUrbsFailed);
            STAM_COUNTER_INC(&pRh->aTypes[pUrb->enmType].StatUrbsFailed);
        }
    }
#endif /* VBOX_WITH_STATISTICS */

    /*
     * Msg transfers are special virtual transfers associated with
     * vusb, not the roothub
     */
    switch (pUrb->enmType)
    {
        case VUSBXFERTYPE_MSG:
            vusbMsgCompletion(pUrb);
            return;
        case VUSBXFERTYPE_ISOC:
            /* Don't bother with error callback for isochronous URBs. */
            break;

#if 1   /** @todo r=bird: OHCI say "If the Transfer Descriptor is being
         * retired because of an error, the Host Controller must update
         * the Halt bit of the Endpoint Descriptor."
         *
         * So, I'll subject all transfertypes to the same halt stuff now. It could
         * just happen to fix the logitech disconnect trap in win2k.
         */
        default:
#endif
        case VUSBXFERTYPE_BULK:
            if (pUrb->enmStatus != VUSBSTATUS_OK)
                vusbUrbErrorRh(pUrb);
            break;
    }
#ifdef LOG_ENABLED
    vusbUrbTrace(pUrb, "vusbUrbCompletionRh", true);
#endif
#ifndef VBOX_WITH_STATISTICS
    PVUSBROOTHUB pRh = vusbDevGetRh(pUrb->VUsb.pDev);
#endif

    /** @todo explain why we do this pDev change. */
    PVUSBDEV pTmp = pUrb->VUsb.pDev;
    pUrb->VUsb.pDev = &pRh->Hub.Dev;
    pRh->pIRhPort->pfnXferCompletion(pRh->pIRhPort, pUrb);
    pUrb->VUsb.pDev = pTmp;
    if (pUrb->enmState == VUSBURBSTATE_REAPED)
    {
        LogFlow(("%s: vusbUrbCompletionRh: Freeing URB\n", pUrb->pszDesc));
        pUrb->VUsb.pfnFree(pUrb);
    }
}


/**
 * Certain control requests must not ever be forwarded to the device because
 * they are required by the vusb core in order to maintain the vusb internal
 * data structures.
 */
DECLINLINE(bool) vusbUrbIsRequestSafe(PCVUSBSETUP pSetup, PVUSBURB pUrb)
{
    if ((pSetup->bmRequestType & VUSB_REQ_MASK) != VUSB_REQ_STANDARD)
        return true;

    switch (pSetup->bRequest)
    {
        case VUSB_REQ_CLEAR_FEATURE:
            return  pUrb->EndPt != 0                   /* not default control pipe */
                ||  pSetup->wValue != 0                /* not ENDPOINT_HALT */
                ||  !pUrb->pUsbIns->pReg->pfnUsbClearHaltedEndpoint; /* not special need for backend */
        case VUSB_REQ_SET_ADDRESS:
        case VUSB_REQ_SET_CONFIGURATION:
        case VUSB_REQ_GET_CONFIGURATION:
        case VUSB_REQ_SET_INTERFACE:
        case VUSB_REQ_GET_INTERFACE:
            return false;

        /*
         * If the device wishes it, we'll use the cached device and
         * configuration descriptors.  (We return false when we want to use the
         * cache. Yeah, it's a bit weird to read.)
         */
        case VUSB_REQ_GET_DESCRIPTOR:
            if (    !pUrb->VUsb.pDev->pDescCache->fUseCachedDescriptors
                ||  (pSetup->bmRequestType & VUSB_RECIP_MASK) != VUSB_TO_DEVICE)
                return true;
            switch (pSetup->wValue >> 8)
            {
                case VUSB_DT_DEVICE:
                case VUSB_DT_CONFIG:
                    return false;
                case VUSB_DT_STRING:
                    return !pUrb->VUsb.pDev->pDescCache->fUseCachedStringsDescriptors;
                default:
                    return true;
            }

        default:
            return true;
    }
}


/**
 * Queues an URB for asynchronous transfer.
 * A list of asynchronous URBs is kept by the roothub.
 *
 * @returns VBox status code (from pfnUrbQueue).
 * @param   pUrb    The URB.
 */
int vusbUrbQueueAsyncRh(PVUSBURB pUrb)
{
#ifdef LOG_ENABLED
    vusbUrbTrace(pUrb, "vusbUrbQueueAsyncRh", false);
#endif

    /* Immediately return in case of error.
     * XXX There is still a race: The Rh might vanish after this point! */
    PVUSBDEV pDev = pUrb->VUsb.pDev;
    PVUSBROOTHUB pRh = vusbDevGetRh(pDev);
    if (!pRh)
    {
        Log(("vusbUrbQueueAsyncRh returning VERR_OBJECT_DESTROYED\n"));
        return VERR_OBJECT_DESTROYED;
    }

    int rc = pUrb->pUsbIns->pReg->pfnUrbQueue(pUrb->pUsbIns, pUrb);
    if (RT_FAILURE(rc))
    {
        LogFlow(("%s: vusbUrbQueueAsyncRh: returns %Rrc (queue_urb)\n", pUrb->pszDesc, rc));
        return rc;
    }

    pDev->aPipes[pUrb->EndPt].async++;

    /* Queue the pUrb on the roothub */
    RTCritSectEnter(&pRh->CritSect);
    pUrb->VUsb.pNext = pRh->pAsyncUrbHead;
    if (pRh->pAsyncUrbHead)
        pRh->pAsyncUrbHead->VUsb.ppPrev = &pUrb->VUsb.pNext;
    pRh->pAsyncUrbHead = pUrb;
    pUrb->VUsb.ppPrev = &pRh->pAsyncUrbHead;
    RTCritSectLeave(&pRh->CritSect);

    return rc;
}


/**
 * Send a control message *synchronously*.
 * @return
 */
static void vusbMsgSubmitSynchronously(PVUSBURB pUrb, bool fSafeRequest)
{
    PVUSBDEV        pDev   = pUrb->VUsb.pDev;
    Assert(pDev);
    PVUSBPIPE       pPipe  = &pDev->aPipes[pUrb->EndPt];
    PVUSBCTRLEXTRA  pExtra = pPipe->pCtrl;
    PVUSBSETUP      pSetup = pExtra->pMsg;
    LogFlow(("%s: vusbMsgSubmitSynchronously: pDev=%p[%s]\n", pUrb->pszDesc, pDev, pDev->pUsbIns ? pDev->pUsbIns->pszName : ""));

    uint8_t *pbData = (uint8_t *)pExtra->pMsg + sizeof(*pSetup);
    uint32_t cbData = pSetup->wLength;
    bool    fOk = false;
    if (!fSafeRequest)
        fOk = vusbDevStandardRequest(pDev, pUrb->EndPt, pSetup, pbData, &cbData);
    else
        AssertMsgFailed(("oops\n"));

    pUrb->enmState = VUSBURBSTATE_REAPED;
    if (fOk)
    {
        pSetup->wLength = cbData;
        pUrb->enmStatus = VUSBSTATUS_OK;
        pExtra->fOk = true;
    }
    else
    {
        pUrb->enmStatus = VUSBSTATUS_STALL;
        pExtra->fOk = false;
    }
    pExtra->cbLeft = cbData; /* used by IN only */

    vusbCtrlCompletion(pUrb);

    /*
     * 'Free' the message URB, i.e. put it back to the allocated state.
     */
    pExtra->Urb.enmState = VUSBURBSTATE_ALLOCATED;
}

/**
 * Callback for dealing with device reset.
 */
void vusbMsgResetExtraData(PVUSBCTRLEXTRA pExtra)
{
    if (!pExtra)
        return;
    pExtra->enmStage = CTLSTAGE_SETUP;
    if (pExtra->Urb.enmState != VUSBURBSTATE_CANCELLED)
        pExtra->Urb.enmState = VUSBURBSTATE_ALLOCATED;
}


/**
 * Callback to free a cancelled message URB.
 *
 * This is yet another place we're we have to performance acrobatics to
 * deal with cancelled URBs. sigh.
 *
 * The deal here is that we never free message URBs since they are integrated
 * into the message pipe state. But since cancel can leave URBs unreaped and in
 * a state which require them not to be freed, we'll have to do two things.
 * First, if a new message URB is processed we'll have to get a new message
 * pipe state. Second, we cannot just free the damn state structure because
 * that might lead to heap corruption since it might still be in-flight.
 *
 * The URB embedded into the message pipe control structure will start in an
 * ALLOCATED state. When submitted it will be go to the IN-FLIGHT state. When
 * reaped it will go from REAPED to ALLOCATED. When completed in the CANCELLED
 * state it will remain in that state (as does normal URBs).
 *
 * If a new message urb comes up while it's in the CANCELLED state, we will
 * orphan it and it will be freed here in vusbMsgFreeUrb. We indicate this
 * by setting VUsb.pvFreeCtx to NULL.
 *
 * If we have to free the message state structure because of device destruction,
 * configuration changes, or similar, we will orphan the message pipe state in
 * the same way by setting VUsb.pvFreeCtx to NULL and let this function free it.
 *
 * @param   pUrb
 */
static DECLCALLBACK(void) vusbMsgFreeUrb(PVUSBURB pUrb)
{
    vusbUrbAssert(pUrb);
    PVUSBCTRLEXTRA pExtra = (PVUSBCTRLEXTRA)((uint8_t *)pUrb - RT_OFFSETOF(VUSBCTRLEXTRA, Urb));
    if (    pUrb->enmState == VUSBURBSTATE_CANCELLED
        &&  !pUrb->VUsb.pvFreeCtx)
    {
        LogFlow(("vusbMsgFreeUrb: Freeing orphan: %p (pUrb=%p)\n", pExtra, pUrb));
        RTMemFree(pExtra);
    }
    else
    {
        Assert(pUrb->VUsb.pvFreeCtx == &pExtra->Urb);
        pUrb->enmState = VUSBURBSTATE_ALLOCATED;
    }
}

/**
 * Frees the extra state data associated with a message pipe.
 *
 * @param   pExtra      The data.
 */
void vusbMsgFreeExtraData(PVUSBCTRLEXTRA pExtra)
{
    if (!pExtra)
        return;
    if (pExtra->Urb.enmState != VUSBURBSTATE_CANCELLED)
    {
        pExtra->Urb.u32Magic = 0;
        pExtra->Urb.enmState = VUSBURBSTATE_FREE;
        if (pExtra->Urb.pszDesc)
            RTStrFree(pExtra->Urb.pszDesc);
        RTMemFree(pExtra);
    }
    else
        pExtra->Urb.VUsb.pvFreeCtx = NULL; /* see vusbMsgFreeUrb */
}

/**
 * Allocates the extra state data required for a control pipe.
 *
 * @returns Pointer to the allocated and initialized state data.
 * @returns NULL on out of memory condition.
 * @param   pUrb    A URB we can copy default data from.
 */
static PVUSBCTRLEXTRA vusbMsgAllocExtraData(PVUSBURB pUrb)
{
/** @todo reuse these? */
    PVUSBCTRLEXTRA pExtra;
    const size_t cbMax = sizeof(pExtra->Urb.abData) + sizeof(VUSBSETUP);
    pExtra = (PVUSBCTRLEXTRA)RTMemAllocZ(RT_OFFSETOF(VUSBCTRLEXTRA, Urb.abData[cbMax]));
    if (pExtra)
    {
        pExtra->enmStage = CTLSTAGE_SETUP;
        //pExtra->fOk = false;
        pExtra->pMsg = (PVUSBSETUP)pExtra->Urb.abData;
        pExtra->pbCur = (uint8_t *)(pExtra->pMsg + 1);
        //pExtra->cbLeft = 0;
        pExtra->cbMax = cbMax;

        //pExtra->Urb.Dev.pvProxyUrb = NULL;
        pExtra->Urb.u32Magic = VUSBURB_MAGIC;
        pExtra->Urb.enmState = VUSBURBSTATE_ALLOCATED;
#ifdef LOG_ENABLED
        RTStrAPrintf(&pExtra->Urb.pszDesc, "URB %p msg->%p", &pExtra->Urb, pUrb);
#endif
        //pExtra->Urb.VUsb.pCtrlUrb = NULL;
        //pExtra->Urb.VUsb.pNext = NULL;
        //pExtra->Urb.VUsb.ppPrev = NULL;
        pExtra->Urb.VUsb.pDev = pUrb->VUsb.pDev;
        pExtra->Urb.VUsb.pfnFree = vusbMsgFreeUrb;
        pExtra->Urb.VUsb.pvFreeCtx = &pExtra->Urb;
        //pExtra->Urb.Hci = {0};
        //pExtra->Urb.Dev.pvProxyUrb = NULL;
        pExtra->Urb.pUsbIns = pUrb->pUsbIns;
        pExtra->Urb.DstAddress = pUrb->DstAddress;
        pExtra->Urb.EndPt = pUrb->EndPt;
        pExtra->Urb.enmType = VUSBXFERTYPE_MSG;
        pExtra->Urb.enmDir = VUSBDIRECTION_INVALID;
        //pExtra->Urb.fShortNotOk = false;
        pExtra->Urb.enmStatus = VUSBSTATUS_INVALID;
        //pExtra->Urb.cbData = 0;
        vusbUrbAssert(&pExtra->Urb);
    }
    return pExtra;
}

/**
 * Sets up the message.
 *
 * The message is associated with the pipe, in what's currently called
 * control pipe extra state data (pointed to by pPipe->pCtrl). If this
 * is a OUT message, we will no go on collecting data URB. If it's a
 * IN message, we'll send it and then queue any incoming data for the
 * URBs collecting it.
 *
 * @returns Success indicator.
 */
static bool vusbMsgSetup(PVUSBPIPE pPipe, const void *pvBuf, uint32_t cbBuf)
{
    PVUSBCTRLEXTRA  pExtra = pPipe->pCtrl;
    const VUSBSETUP *pSetupIn = (PVUSBSETUP)pvBuf;

    /*
     * Validate length.
     */
    if (cbBuf < sizeof(VUSBSETUP))
    {
        LogFlow(("vusbMsgSetup: pPipe=%p cbBuf=%u < %u (failure) !!!\n",
                 pPipe, cbBuf, sizeof(VUSBSETUP)));
        return false;
    }

    /*
     * Check if we've got an cancelled message URB. Allocate a new one in that case.
     */
    if (pExtra->Urb.enmState == VUSBURBSTATE_CANCELLED)
    {
        void *pvNew = RTMemDup(pExtra, RT_OFFSETOF(VUSBCTRLEXTRA, Urb.abData[pExtra->cbMax]));
        if (!pvNew)
        {
            Log(("vusbMsgSetup: out of memory!!! cbReq=%u\n", RT_OFFSETOF(VUSBCTRLEXTRA, Urb.abData[pExtra->cbMax])));
            return false;
        }
        pExtra->Urb.VUsb.pvFreeCtx = NULL;
        LogFlow(("vusbMsgSetup: Replacing canceled pExtra=%p with %p.\n", pExtra, pvNew));
        pPipe->pCtrl = pExtra = (PVUSBCTRLEXTRA)pvNew;
        pExtra->pMsg = (PVUSBSETUP)pExtra->Urb.abData;
        pExtra->Urb.enmState = VUSBURBSTATE_ALLOCATED;
    }

    /*
     * Check that we've got sufficient space in the message URB.
     */
    if (pExtra->cbMax < cbBuf + pSetupIn->wLength)
    {
        uint32_t cbReq = RT_ALIGN_32(cbBuf + pSetupIn->wLength, 1024);
        PVUSBCTRLEXTRA pNew = (PVUSBCTRLEXTRA)RTMemRealloc(pExtra, RT_OFFSETOF(VUSBCTRLEXTRA, Urb.abData[cbReq]));
        if (!pNew)
        {
            Log(("vusbMsgSetup: out of memory!!! cbReq=%u %u\n",
                 cbReq, RT_OFFSETOF(VUSBCTRLEXTRA, Urb.abData[cbReq])));
            return false;
        }
        if (pExtra != pNew)
        {
            pNew->pMsg = (PVUSBSETUP)pNew->Urb.abData;
            pExtra = pNew;
        }
        pExtra->cbMax = cbReq;
    }
    Assert(pExtra->Urb.enmState == VUSBURBSTATE_ALLOCATED);

    /*
     * Copy the setup data and prepare for data.
     */
    PVUSBSETUP pSetup = pExtra->pMsg;
    pExtra->fSubmitted      = false;
    pExtra->Urb.enmState    = VUSBURBSTATE_IN_FLIGHT;
    pExtra->pbCur           = (uint8_t *)(pSetup + 1);
    pSetup->bmRequestType   = pSetupIn->bmRequestType;
    pSetup->bRequest        = pSetupIn->bRequest;
    pSetup->wValue          = RT_LE2H_U16(pSetupIn->wValue);
    pSetup->wIndex          = RT_LE2H_U16(pSetupIn->wIndex);
    pSetup->wLength         = RT_LE2H_U16(pSetupIn->wLength);

    LogFlow(("vusbMsgSetup(%p,,%d): bmRequestType=%#04x bRequest=%#04x wValue=%#06x wIndex=%#06x wLength=%d\n",
             pPipe, cbBuf, pSetup->bmRequestType, pSetup->bRequest, pSetup->wValue, pSetup->wIndex, pSetup->wLength));
    return true;
}

/**
 * Build the message URB from the given control URB and accompanying message
 * pipe state which we grab from the device for the URB.
 *
 * @param   pUrb        The URB to submit.
 */
static void vusbMsgDoTransfer(PVUSBURB pUrb, PVUSBSETUP pSetup, PVUSBCTRLEXTRA pExtra, PVUSBPIPE pPipe, PVUSBDEV pDev)
{
    /*
     * Mark this transfer as sent (cleared at setup time).
     */
    Assert(!pExtra->fSubmitted);
    pExtra->fSubmitted = true;

    /*
     * Do we have to do this synchronously?
     */
    bool fSafeRequest = vusbUrbIsRequestSafe(pSetup, pUrb);
    if (!fSafeRequest)
    {
        vusbMsgSubmitSynchronously(pUrb, fSafeRequest);
        return;
    }

    /*
     * Do it asynchronously.
     */
    LogFlow(("%s: vusbMsgDoTransfer: ep=%d pMsgUrb=%p pPipe=%p stage=%s\n",
             pUrb->pszDesc, pUrb->EndPt, &pExtra->Urb, pPipe, g_apszCtlStates[pExtra->enmStage]));
    Assert(pExtra->Urb.enmType == VUSBXFERTYPE_MSG);
    Assert(pExtra->Urb.EndPt == pUrb->EndPt);
    pExtra->Urb.enmDir  = (pSetup->bmRequestType & VUSB_DIR_TO_HOST) ? VUSBDIRECTION_IN : VUSBDIRECTION_OUT;
    pExtra->Urb.cbData  = pSetup->wLength + sizeof(*pSetup);
    pExtra->Urb.VUsb.pCtrlUrb = pUrb;
    int rc = vusbUrbQueueAsyncRh(&pExtra->Urb);
    if (RT_FAILURE(rc))
    {
        /*
         * If we fail submitting it, will not retry but fail immediately.
         *
         * This keeps things simple. The host OS will have retried if
         * it's a proxied device, and if it's a virtual one it really means
         * it if it's failing a control message.
         */
        LogFlow(("%s: vusbMsgDoTransfer: failed submitting urb! failing it with %s (rc=%Rrc)!!!\n",
                 pUrb->pszDesc, rc == VERR_VUSB_DEVICE_NOT_ATTACHED ? "DNR" : "CRC", rc));
        pExtra->Urb.enmStatus = rc == VERR_VUSB_DEVICE_NOT_ATTACHED ? VUSBSTATUS_DNR : VUSBSTATUS_CRC;
        pExtra->Urb.enmState = VUSBURBSTATE_REAPED;
        vusbMsgCompletion(&pExtra->Urb);
    }
}

/**
 * Fails a URB request with a pipe STALL error.
 *
 * @returns VINF_SUCCESS indicating that we've completed the URB.
 * @param   pUrb    The URB in question.
 */
static int vusbMsgStall(PVUSBURB pUrb)
{
    PVUSBPIPE       pPipe = &pUrb->VUsb.pDev->aPipes[pUrb->EndPt];
    PVUSBCTRLEXTRA  pExtra = pPipe->pCtrl;
    LogFlow(("%s: vusbMsgStall: pPipe=%p err=STALL stage %s->SETUP\n",
             pUrb->pszDesc, pPipe, g_apszCtlStates[pExtra->enmStage]));

    pExtra->pbCur    = NULL;
    pExtra->enmStage = CTLSTAGE_SETUP;
    pUrb->enmState = VUSBURBSTATE_REAPED;
    pUrb->enmStatus  = VUSBSTATUS_STALL;
    vusbUrbCompletionRh(pUrb);
    return VINF_SUCCESS;
}

/**
 * Submit a control message.
 *
 * Here we implement the USB defined traffic that occurs in message pipes
 * (aka control endpoints). We want to provide a single function for device
 * drivers so that they don't all have to reimplement the usb logic for
 * themselves. This means we need to keep a little bit of state information
 * because control transfers occur over multiple bus transactions. We may
 * also need to buffer data over multiple data stages.
 *
 * @returns VBox status code.
 * @param   pUrb        The URB to submit.
 */
static int vusbUrbSubmitCtrl(PVUSBURB pUrb)
{
#ifdef LOG_ENABLED
    vusbUrbTrace(pUrb, "vusbUrbSubmitCtrl", false);
#endif
    PVUSBDEV        pDev = pUrb->VUsb.pDev;
    PVUSBPIPE       pPipe = &pDev->aPipes[pUrb->EndPt];
    PVUSBCTRLEXTRA  pExtra = pPipe->pCtrl;
    if (!pExtra && !(pExtra = pPipe->pCtrl = vusbMsgAllocExtraData(pUrb)))
        return VERR_VUSB_NO_URB_MEMORY;
    PVUSBSETUP      pSetup = pExtra->pMsg;

    AssertMsgReturn(!pPipe->async, ("%u\n", pPipe->async), VERR_GENERAL_FAILURE);


    /*
     * A setup packet always resets the transaction and the
     * end of data transmission is signified by change in
     * data direction.
     */
    if (pUrb->enmDir == VUSBDIRECTION_SETUP)
    {
        LogFlow(("%s: vusbUrbSubmitCtrl: pPipe=%p state %s->SETUP\n",
                 pUrb->pszDesc, pPipe, g_apszCtlStates[pExtra->enmStage]));
        pExtra->enmStage = CTLSTAGE_SETUP;
    }
    else if (   pExtra->enmStage == CTLSTAGE_DATA
                /* (the STATUS stage direction goes the other way) */
             && !!(pSetup->bmRequestType & VUSB_DIR_TO_HOST) != (pUrb->enmDir == VUSBDIRECTION_IN))
    {
        LogFlow(("%s: vusbUrbSubmitCtrl: pPipe=%p state %s->STATUS\n",
                 pUrb->pszDesc, pPipe, g_apszCtlStates[pExtra->enmStage]));
        pExtra->enmStage = CTLSTAGE_STATUS;
    }

    /*
     * Act according to the current message stage.
     */
    switch (pExtra->enmStage)
    {
        case CTLSTAGE_SETUP:
            /*
             * When stall handshake is returned, all subsequent packets
             * must generate stall until a setup packet arrives.
             */
            if (pUrb->enmDir != VUSBDIRECTION_SETUP)
            {
                Log(("%s: vusbUrbSubmitCtrl: Stall at setup stage (dir=%#x)!!\n", pUrb->pszDesc, pUrb->enmDir));
                return vusbMsgStall(pUrb);
            }

            /* Store setup details, return DNR if corrupt */
            if (!vusbMsgSetup(pPipe, pUrb->abData, pUrb->cbData))
            {
                pUrb->enmState = VUSBURBSTATE_REAPED;
                pUrb->enmStatus = VUSBSTATUS_DNR;
                vusbUrbCompletionRh(pUrb);
                return VINF_SUCCESS;
            }
            if (pPipe->pCtrl != pExtra)
            {
                pExtra = pPipe->pCtrl;
                pSetup = pExtra->pMsg;
            }

            /* pre-buffer our output if it's device-to-host */
            if (pSetup->bmRequestType & VUSB_DIR_TO_HOST)
                vusbMsgDoTransfer(pUrb, pSetup, pExtra, pPipe, pDev);
            else if (pSetup->wLength)
            {
                LogFlow(("%s: vusbUrbSubmitCtrl: stage=SETUP - to dev: need data\n", pUrb->pszDesc));
                pUrb->enmState = VUSBURBSTATE_REAPED;
                vusbMsgSetupCompletion(pUrb);
                vusbUrbCompletionRh(pUrb);
            }
            /*
             * If there is no DATA stage, we must send it now since there are
             * no requirement of a STATUS stage.
             */
            else
            {
                LogFlow(("%s: vusbUrbSubmitCtrl: stage=SETUP - to dev: sending\n", pUrb->pszDesc));
                vusbMsgDoTransfer(pUrb, pSetup, pExtra, pPipe, pDev);
            }
            break;

        case CTLSTAGE_DATA:
        {
            /*
             * If a data stage exceeds the target buffer indicated in
             * setup return stall, if data stage returns stall there
             * will be no status stage.
             */
            uint8_t *pbData = (uint8_t *)(pExtra->pMsg + 1);
            if (&pExtra->pbCur[pUrb->cbData] > &pbData[pSetup->wLength])
            {
                if (!pSetup->wLength) /* happens during iPhone detection with iTunes (correct?) */
                {
                    Log(("%s: vusbUrbSubmitCtrl: pSetup->wLength == 0!! (iPhone)\n", pUrb->pszDesc));
                    pSetup->wLength = pUrb->cbData;
                }

                /* Variable length data transfers */
                if (    (pSetup->bmRequestType & VUSB_DIR_TO_HOST)
                    ||  pSetup->wLength == 0
                    ||  (pUrb->cbData % pSetup->wLength) == 0)  /* magic which need explaining... */
                {
                    uint8_t *pbEnd = pbData + pSetup->wLength;
                    int cbLeft = pbEnd - pExtra->pbCur;
                    LogFlow(("%s: vusbUrbSubmitCtrl: Var DATA, pUrb->cbData %d -> %d\n", pUrb->pszDesc, pUrb->cbData, cbLeft));
                    pUrb->cbData = cbLeft;
                }
                else
                {
                    Log(("%s: vusbUrbSubmitCtrl: Stall at data stage!!\n", pUrb->pszDesc));
                    return vusbMsgStall(pUrb);
                }
            }

            if (pUrb->enmDir == VUSBDIRECTION_IN)
            {
                /* put data received from the device. */
                const uint32_t cbRead = RT_MIN(pUrb->cbData, pExtra->cbLeft);
                memcpy(pUrb->abData, pExtra->pbCur, cbRead);

                /* advance */
                pExtra->pbCur += cbRead;
                if (pUrb->cbData == cbRead)
                    pExtra->cbLeft -= pUrb->cbData;
                else
                {
                    /* adjust the pUrb->cbData to reflect the number of bytes containing actual data. */
                    LogFlow(("%s: vusbUrbSubmitCtrl: adjusting last DATA pUrb->cbData, %d -> %d\n",
                             pUrb->pszDesc, pUrb->cbData, pExtra->cbLeft));
                    pUrb->cbData = cbRead;
                    pExtra->cbLeft = 0;
                }
            }
            else
            {
                /* get data for sending when completed. */
                memcpy(pExtra->pbCur, pUrb->abData, pUrb->cbData);

                /* advance */
                pExtra->pbCur += pUrb->cbData;

                /*
                 * If we've got the necessary data, we'll send it now since there are
                 * no requirement of a STATUS stage.
                 */
                if (    !pExtra->fSubmitted
                    &&  pExtra->pbCur - pbData >= pSetup->wLength)
                {
                    LogFlow(("%s: vusbUrbSubmitCtrl: stage=DATA - to dev: sending\n", pUrb->pszDesc));
                    vusbMsgDoTransfer(pUrb, pSetup, pExtra, pPipe, pDev);
                    break;
                }
            }

            pUrb->enmState = VUSBURBSTATE_REAPED;
            vusbMsgDataCompletion(pUrb);
            vusbUrbCompletionRh(pUrb);
            break;
        }

        case CTLSTAGE_STATUS:
            if (    (pSetup->bmRequestType & VUSB_DIR_TO_HOST)
                ||  pExtra->fSubmitted)
            {
                Assert(pExtra->fSubmitted);
                pUrb->enmState = VUSBURBSTATE_REAPED;
                vusbMsgStatusCompletion(pUrb);
                vusbUrbCompletionRh(pUrb);
            }
            else
            {
                LogFlow(("%s: vusbUrbSubmitCtrl: stage=STATUS - to dev: sending\n", pUrb->pszDesc));
                vusbMsgDoTransfer(pUrb, pSetup, pExtra, pPipe, pDev);
            }
            break;
    }

    return VINF_SUCCESS;
}


/**
 * Submit a interrupt URB.
 *
 * @returns VBox status code.
 * @param   pUrb        The URB to submit.
 */
static int vusbUrbSubmitInterrupt(PVUSBURB pUrb)
{
    LogFlow(("%s: vusbUrbSubmitInterrupt: (sync)\n", pUrb->pszDesc));
    return vusbUrbQueueAsyncRh(pUrb);
}


/**
 * Submit a bulk URB.
 *
 * @returns VBox status code.
 * @param   pUrb        The URB to submit.
 */
static int vusbUrbSubmitBulk(PVUSBURB pUrb)
{
    LogFlow(("%s: vusbUrbSubmitBulk: (async)\n", pUrb->pszDesc));
    return vusbUrbQueueAsyncRh(pUrb);
}


/**
 * Submit an isochronous URB.
 *
 * @returns VBox status code.
 * @param   pUrb        The URB to submit.
 */
static int vusbUrbSubmitIsochronous(PVUSBURB pUrb)
{
    LogFlow(("%s: vusbUrbSubmitIsochronous: (async)\n", pUrb->pszDesc));
    return vusbUrbQueueAsyncRh(pUrb);
}


/**
 * Fail a URB with a 'hard-error' sort of error.
 *
 * @return VINF_SUCCESS (the Urb status indicates the error).
 * @param   pUrb    The URB.
 */
static int vusbUrbSubmitHardError(PVUSBURB pUrb)
{
    /* FIXME: Find out the correct return code from the spec */
    pUrb->enmState = VUSBURBSTATE_REAPED;
    pUrb->enmStatus = VUSBSTATUS_DNR;
    vusbUrbCompletionRh(pUrb);
    return VINF_SUCCESS;
}


/**
 * Submit a URB.
 */
int vusbUrbSubmit(PVUSBURB pUrb)
{
    vusbUrbAssert(pUrb);
    Assert(pUrb->enmState == VUSBURBSTATE_ALLOCATED);
    PVUSBDEV pDev = pUrb->VUsb.pDev;
    PVUSBPIPE pPipe = NULL;
    Assert(pDev);

    /*
     * Check that the device is in a valid state.
     */
    const VUSBDEVICESTATE enmState = pDev->enmState;
    if (enmState == VUSB_DEVICE_STATE_RESET)
    {
        LogRel(("VUSB: %s: power off ignored, the device is resetting!\n", pDev->pUsbIns->pszName));
        pUrb->enmStatus = VUSBSTATUS_DNR;
        /* This will postpone the TDs until we're done with the resetting. */
        return VERR_VUSB_DEVICE_IS_RESETTING;
    }

#ifdef LOG_ENABLED
    /* stamp it */
    pUrb->VUsb.u64SubmitTS = RTTimeNanoTS();
#endif

    /** @todo Check max packet size here too? */

    /*
     * Validate the pipe.
     */
    if (pUrb->EndPt >= VUSB_PIPE_MAX)
    {
        Log(("%s: pDev=%p[%s]: SUBMIT: ep %i >= %i!!!\n", pUrb->pszDesc, pDev, pDev->pUsbIns->pszName, pUrb->EndPt, VUSB_PIPE_MAX));
        return vusbUrbSubmitHardError(pUrb);
    }
    PCVUSBDESCENDPOINTEX pEndPtDesc;
    switch (pUrb->enmDir)
    {
        case VUSBDIRECTION_IN:
            pEndPtDesc = pDev->aPipes[pUrb->EndPt].in;
            pPipe = &pDev->aPipes[pUrb->EndPt];
            break;
        case VUSBDIRECTION_SETUP:
        case VUSBDIRECTION_OUT:
        default:
            pEndPtDesc = pDev->aPipes[pUrb->EndPt].out;
            break;
    }
    if (!pEndPtDesc)
    {
        Log(("%s: pDev=%p[%s]: SUBMIT: no endpoint!!! dir=%s e=%i\n",
             pUrb->pszDesc, pDev, pDev->pUsbIns->pszName, vusbUrbDirName(pUrb->enmDir), pUrb->EndPt));
        return vusbUrbSubmitHardError(pUrb);
    }

    /*
     * Check for correct transfer types.
     * Our type codes are the same - what a coincidence.
     */
    if ((pEndPtDesc->Core.bmAttributes & 0x3) != pUrb->enmType)
    {
        Log(("%s: pDev=%p[%s]: SUBMIT: %s transfer requested for %#x endpoint on DstAddress=%i ep=%i dir=%s\n",
             pUrb->pszDesc, pDev, pDev->pUsbIns->pszName, vusbUrbTypeName(pUrb->enmType), pEndPtDesc->Core.bmAttributes,
             pUrb->DstAddress, pUrb->EndPt, vusbUrbDirName(pUrb->enmDir)));
        return vusbUrbSubmitHardError(pUrb);
    }

    /*
     * If there's a URB in the read-ahead buffer, use it.
     */
    int rc;

#ifdef VBOX_WITH_USB
    if (pPipe && pPipe->pBuffUrbHead)
    {
        rc = vusbUrbSubmitBufferedRead(pUrb, pPipe);
        return rc;
    }
#endif

    /*
     * Take action based on type.
     */
    pUrb->enmState = VUSBURBSTATE_IN_FLIGHT;
    switch (pUrb->enmType)
    {
        case VUSBXFERTYPE_CTRL:
            rc = vusbUrbSubmitCtrl(pUrb);
            break;
        case VUSBXFERTYPE_BULK:
            rc = vusbUrbSubmitBulk(pUrb);
            break;
        case VUSBXFERTYPE_INTR:
            rc = vusbUrbSubmitInterrupt(pUrb);
            break;
        case VUSBXFERTYPE_ISOC:
            rc = vusbUrbSubmitIsochronous(pUrb);
            break;
        default:
            AssertMsgFailed(("Unexpected pUrb type %d\n", pUrb->enmType));
            return vusbUrbSubmitHardError(pUrb);
    }

    /*
     * The device was detached, so we fail everything.
     * (We should really detach and destroy the device, but we'll have to wait till Main reacts.)
     */
    if (rc == VERR_VUSB_DEVICE_NOT_ATTACHED)
        rc = vusbUrbSubmitHardError(pUrb);
    /*
     * We don't increment error count if async URBs are in flight, in
     * this case we just assume we need to throttle back, this also
     * makes sure we don't halt bulk endpoints at the wrong time.
     */
    else if (   RT_FAILURE(rc)
             && !pDev->aPipes[pUrb->EndPt].async
             /* && pUrb->enmType == VUSBXFERTYPE_BULK ?? */
             && !vusbUrbErrorRh(pUrb))
    {
        /* don't retry it anymore. */
        pUrb->enmState = VUSBURBSTATE_REAPED;
        pUrb->enmStatus = VUSBSTATUS_CRC;
        vusbUrbCompletionRh(pUrb);
        return VINF_SUCCESS;
    }

    return rc;
}


/**
 * Reap in-flight URBs.
 *
 * @param   pHead       Pointer to the head of the URB list.
 * @param   cMillies    Number of milliseconds to block in each reap operation.
 *                      Use 0 to not block at all.
 */
void vusbUrbDoReapAsync(PVUSBURB pHead, RTMSINTERVAL cMillies)
{
    PVUSBURB pUrb = pHead;
    while (pUrb)
    {
        vusbUrbAssert(pUrb);
        PVUSBURB pUrbNext = pUrb->VUsb.pNext;
        PVUSBDEV pDev = pUrb->VUsb.pDev;

        /* Don't touch resetting devices - paranoid safety precaution. */
        if (pDev->enmState != VUSB_DEVICE_STATE_RESET)
        {
            /*
             * Reap most URBs pending on a single device.
             */
            PVUSBURB pRipe;
            while ((pRipe = pDev->pUsbIns->pReg->pfnUrbReap(pDev->pUsbIns, cMillies)) != NULL)
            {
                vusbUrbAssert(pRipe);
                if (pRipe == pUrbNext)
                    pUrbNext = pUrbNext->VUsb.pNext;
                vusbUrbRipe(pRipe);
            }
        }

        /* next */
        pUrb = pUrbNext;
    }
}


/**
 * Completes the URB.
 */
static void vusbUrbCompletion(PVUSBURB pUrb)
{
    Assert(pUrb->VUsb.pDev->aPipes);
    pUrb->VUsb.pDev->aPipes[pUrb->EndPt].async--;

    if (pUrb->enmState == VUSBURBSTATE_REAPED)
        vusbUrbUnlink(pUrb);
#ifdef VBOX_WITH_USB
    // Read-ahead URBs are handled differently
    if (pUrb->Hci.pNext != NULL)
        vusbUrbCompletionReadAhead(pUrb);
    else
#endif
        vusbUrbCompletionRh(pUrb);
}


/**
 * Cancels an URB with CRC failure.
 *
 * Cancelling an URB is a tricky thing. The USBProxy backend can not
 * all cancel it and we must keep the URB around until it's ripe and
 * can be reaped the normal way. However, we must complete the URB
 * now, before leaving this function. This is not nice. sigh.
 *
 * This function will cancel the URB if it's in-flight and complete
 * it. The device will in its pfnCancel method be given the chance to
 * say that the URB doesn't need reaping and should be unlinked.
 *
 * An URB which is in the cancel state after pfnCancel will remain in that
 * state and in the async list until its reaped. When it's finally reaped
 * it will be unlinked and freed without doing any completion.
 *
 * There are different modes of canceling an URB. When devices are being
 * disconnected etc., they will be completed with an error (CRC). However,
 * when the HC needs to temporarily halt communication with a device, the
 * URB/TD must be left alone if possible.
 *
 * @param   pUrb        The URB to cancel.
 * @param   mode        The way the URB should be canceled.
 */
void vusbUrbCancel(PVUSBURB pUrb, CANCELMODE mode)
{
    vusbUrbAssert(pUrb);
#ifdef VBOX_WITH_STATISTICS
    PVUSBROOTHUB pRh = vusbDevGetRh(pUrb->VUsb.pDev);
#endif
    if (pUrb->enmState == VUSBURBSTATE_IN_FLIGHT)
    {
        LogFlow(("%s: vusbUrbCancel: Canceling in-flight\n", pUrb->pszDesc));
        STAM_COUNTER_INC(&pRh->Total.StatUrbsCancelled);
        if (pUrb->enmType != VUSBXFERTYPE_MSG)
        {
            STAM_STATS({Assert(pUrb->enmType >= 0 && pUrb->enmType < (int)RT_ELEMENTS(pRh->aTypes));});
            STAM_COUNTER_INC(&pRh->aTypes[pUrb->enmType].StatUrbsCancelled);
        }

        pUrb->enmState = VUSBURBSTATE_CANCELLED;
        PPDMUSBINS pUsbIns = pUrb->pUsbIns;
        pUsbIns->pReg->pfnUrbCancel(pUsbIns, pUrb);
        Assert(pUrb->enmState == VUSBURBSTATE_CANCELLED || pUrb->enmState == VUSBURBSTATE_REAPED);

        pUrb->enmStatus = VUSBSTATUS_CRC;
        vusbUrbCompletion(pUrb);
    }
    else if (pUrb->enmState == VUSBURBSTATE_REAPED)
    {
        LogFlow(("%s: vusbUrbCancel: Canceling reaped urb\n", pUrb->pszDesc));
        STAM_COUNTER_INC(&pRh->Total.StatUrbsCancelled);
        if (pUrb->enmType != VUSBXFERTYPE_MSG)
        {
            STAM_STATS({Assert(pUrb->enmType >= 0 && pUrb->enmType < (int)RT_ELEMENTS(pRh->aTypes));});
            STAM_COUNTER_INC(&pRh->aTypes[pUrb->enmType].StatUrbsCancelled);
        }

        pUrb->enmStatus = VUSBSTATUS_CRC;
        vusbUrbCompletion(pUrb);
    }
    else
    {
        AssertMsg(pUrb->enmState == VUSBURBSTATE_CANCELLED, ("Invalid state %d, pUrb=%p\n", pUrb->enmState, pUrb));
        switch (mode)
        {
            default:
                AssertMsgFailed(("Invalid cancel mode\n"));
            case CANCELMODE_FAIL:
                pUrb->enmStatus = VUSBSTATUS_CRC;
                break;
            case CANCELMODE_UNDO:
                pUrb->enmStatus = VUSBSTATUS_UNDO;
                break;

        }
    }
}


/**
 * Deals with a ripe URB (i.e. after reaping it).
 *
 * If an URB is in the reaped or in-flight state, we'll
 * complete it. If it's cancelled, we'll simply free it.
 * Any other states should never get here.
 *
 * @param   pUrb    The URB.
 */
void vusbUrbRipe(PVUSBURB pUrb)
{
    if (    pUrb->enmState == VUSBURBSTATE_IN_FLIGHT
        ||  pUrb->enmState == VUSBURBSTATE_REAPED)
    {
        pUrb->enmState = VUSBURBSTATE_REAPED;
        vusbUrbCompletion(pUrb);
    }
    else if (pUrb->enmState == VUSBURBSTATE_CANCELLED)
    {
        vusbUrbUnlink(pUrb);
        LogFlow(("%s: vusbUrbRipe: Freeing cancelled URB\n", pUrb->pszDesc));
        pUrb->VUsb.pfnFree(pUrb);
    }
    else
        AssertMsgFailed(("Invalid URB state %d; %s\n", pUrb->enmState, pUrb->pszDesc));
}


/*
 * Local Variables:
 *  mode: c
 *  c-file-style: "bsd"
 *  c-basic-offset: 4
 *  tab-width: 4
 *  indent-tabs-mode: s
 * End:
 */

