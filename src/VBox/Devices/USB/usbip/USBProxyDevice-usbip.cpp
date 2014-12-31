/* $Id$ */
/** @file
 * USB device proxy - USB/IP backend.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_USBPROXY

#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/vmm/pdm.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/socket.h>
#include <iprt/poll.h>
#include <iprt/tcp.h>
#include <iprt/pipe.h>
#include <iprt/list.h>
#include <iprt/semaphore.h>

#include "../USBProxyDevice.h"


/*******************************************************************************
*   Constants And Macros, Structures and Typedefs                              *
*******************************************************************************/

/** The USB version number used for the protocol. */
#define USBIP_VERSION         UINT16_C(0x0100)
/** Request indicator in the command code. */
#define USBIP_INDICATOR_REQ   RT_BIT(15)

/** Command/Reply code for OP_REQ/RET_DEVLIST. */
#define USBIP_REQ_RET_DEVLIST UINT16_C(5)
/** Command/Reply code for OP_REQ/REP_IMPORT. */
#define USBIP_REQ_RET_IMPORT  UINT16_C(3)
/** USB submit command identifier. */
#define USBIP_CMD_SUBMIT      UINT32_C(1)
/** USB submit status identifier. */
#define USBIP_RET_SUBMIT      UINT32_C(2)
/** URB unlink (cancel) command identifier. */
#define USBIP_CMD_UNLINK      UINT32_C(3)
/** URB unlink (cancel) reply identifier. */
#define USBIP_RET_UNLINK      UINT32_C(4)

/** Short read is not okay for the specified URB. */
#define USBIP_XFER_FLAGS_SHORT_NOT_OK        RT_BIT_32(0)
/** Queue the isochronous URB as soon as possible. */
#define USBIP_XFER_FLAGS_ISO_ASAP            RT_BIT_32(1)
/** Don't use DMA mappings for this URB. */
#define USBIP_XFER_FLAGS_NO_TRANSFER_DMA_MAP RT_BIT_32(2)
/** Explain - only applies to UHCI. */
#define USBIP_XFER_FLAGS_FSBR                RT_BIT_32(4)

/** URB direction - input. */
#define USBIP_DIR_IN                         UINT32_C(0)
/** URB direction - output. */
#define USBIP_DIR_OUT                        UINT32_C(1)

/**
 * Exported device entry in the OP_RET_DEVLIST reply.
 */
#pragma pack(1)
typedef struct UsbIpExportedDevice
{
    /** Path of the device, zero terminated string. */
    char    szPath[256];
    /** Bus ID of the exported device, zero terminated string. */
    char    szBusId[32];
    /** Bus number. */
    uint32_t u32BusNum;
    /** Device number. */
    uint32_t u32DevNum;
    /** Speed indicator of the device. */
    uint32_t u32Speed;
    /** Vendor ID of the device. */
    uint16_t u16VendorId;
    /** Product ID of the device. */
    uint16_t u16ProductId;
    /** Device release number. */
    uint16_t u16BcdDevice;
    /** Device class. */
    uint8_t  bDeviceClass;
    /** Device Subclass. */
    uint8_t  bDeviceSubClass;
    /** Device protocol. */
    uint8_t  bDeviceProtocol;
    /** Current configuration value of the device. */
    uint8_t  bNumConfigurations;
    /** Number of interfaces for the device. */
    uint8_t  bNumInterfaces;
} UsbIpExportedDevice;
/** Pointer to a exported device entry. */
typedef UsbIpExportedDevice *PUsbIpExportedDevice;
#pragma pack()

/**
 * Interface descriptor entry for an exported device.
 */
#pragma pack(1)
typedef struct UsbIpDeviceInterface
{
    /** Intefrace class. */
    uint8_t  bInterfaceClass;
    /** Interface sub class. */
    uint8_t  bInterfaceSubClass;
    /** Interface protocol identifier. */
    uint8_t  bInterfaceProtocol;
    /** Padding byte for alignment. */
    uint8_t  bPadding;
} UsbIpDeviceInterface;
/** Pointer to an interface descriptor entry. */
typedef UsbIpDeviceInterface *PUsbIpDeviceInterface;
#pragma pack()

/**
 * USB/IP Import request.
 */
#pragma pack(1)
typedef struct UsbIpReqImport
{
    /** Protocol version number. */
    uint16_t     u16Version;
    /** Command code. */
    uint16_t     u16Cmd;
    /** Status field, unused. */
    uint32_t     u32Status;
    /** Bus Id of the device as zero terminated string. */
    char         aszBusId[32];
} UsbIpReqImport;
/** Pointer to a import request. */
typedef UsbIpReqImport *PUsbIpReqImport;
#pragma pack()

/**
 * USB/IP Import reply.
 *
 * This is only the header, for successful
 * imports the device details are sent to as
 * defined in UsbIpExportedDevice.
 */
#pragma pack(1)
typedef struct UsbIpRetImport
{
    /** Protocol version number. */
    uint16_t     u16Version;
    /** Command code. */
    uint16_t     u16Cmd;
    /** Status field, unused. */
    uint32_t     u32Status;
} UsbIpRetImport;
/** Pointer to a import reply. */
typedef UsbIpRetImport *PUsbIpRetImport;
#pragma pack()

/**
 * Command/Reply header common to the submit and unlink commands
 * replies.
 */
#pragma pack(1)
typedef struct UsbIpReqRetHdr
{
    /** Request/Return code. */
    uint32_t     u32ReqRet;
    /** Sequence number to identify the URB. */
    uint32_t     u32SeqNum;
    /** Device id. */
    uint32_t     u32DevId;
    /** Direction of the endpoint (host->device, device->host). */
    uint32_t     u32Direction;
    /** Endpoint number. */
    uint32_t     u32Endpoint;
} UsbIpReqRetHdr;
/** Pointer to a request/reply header. */
typedef UsbIpReqRetHdr *PUsbIpReqRetHdr;
#pragma pack()

/**
 * USB/IP Submit request.
 */
#pragma pack(1)
typedef struct UsbIpReqSubmit
{
    /** The request header. */
    UsbIpReqRetHdr Hdr;
    /** Transfer flags for the URB. */
    uint32_t       u32XferFlags;
    /** Transfer buffer length. */
    uint32_t       u32TransferBufferLength;
    /** Frame to transmit an ISO frame. */
    uint32_t       u32StartFrame;
    /** Number of isochronous packets. */
    uint32_t       u32NumIsocPkts;
    /** Maximum time for the request on the server side host controller. */
    uint32_t       u32Interval;
    /** Setup data for a control URB. */
    VUSBSETUP      Setup;
} UsbIpReqSubmit;
/** Pointer to a submit request. */
typedef UsbIpReqSubmit *PUsbIpReqSubmit;
#pragma pack()

/**
 * USB/IP Submit reply.
 */
#pragma pack(1)
typedef struct UsbIpRetSubmit
{
    /** The reply header. */
    UsbIpReqRetHdr Hdr;
    /** Status code. */
    uint32_t       u32Status;
    /** Actual length of the reply buffer. */
    uint32_t       u32ActualLength;
    /** The actual selected frame for a isochronous transmit. */
    uint32_t       u32StartFrame;
    /** Number of isochronous packets. */
    uint32_t       u32NumIsocPkts;
    /** Number of failed isochronous packets. */
    uint32_t       u32ErrorCount;
    /** Setup data for a control URB. */
    VUSBSETUP      Setup;
} UsbIpRetSubmit;
/** Pointer to a submit reply. */
typedef UsbIpRetSubmit *PUsbIpRetSubmit;
#pragma pack()

/**
 * Unlink URB request.
 */
#pragma pack(1)
typedef struct UsbIpReqUnlink
{
    /** The request header. */
    UsbIpReqRetHdr Hdr;
    /** The sequence number to unlink. */
    uint32_t       u32SeqNum;
} UsbIpReqUnlink;
/** Pointer to a URB unlink request. */
typedef UsbIpReqUnlink *PUsbIpReqUnlink;
#pragma pack()

/**
 * Unlink URB reply.
 */
#pragma pack(1)
typedef struct UsbIpRetUnlink
{
    /** The reply header. */
    UsbIpReqRetHdr Hdr;
    /** Status of the request. */
    uint32_t       u32Status;
} UsbIpRetUnlink;
/** Pointer to a URB unlink request. */
typedef UsbIpRetUnlink *PUsbIpRetUnlink;
#pragma pack()

/**
 * Union of possible replies from the server during normal operation.
 */
#pragma pack(1)
typedef union UsbIpRet
{
    /** The header. */
    UsbIpReqRetHdr Hdr;
    /** Submit reply. */
    UsbIpRetSubmit RetSubmit;
    /** Unlink reply. */
    UsbIpRetUnlink RetUnlink;
    /** Byte view. */
    uint8_t        abReply[1];
} UsbIpRet;
/** Pointer to a reply union. */
typedef UsbIpRet *PUsbIpRet;
#pragma pack()

/**
 * USB/IP backend specific data for one URB.
 * Required for tracking in flight and landed URBs.
 */
typedef struct USBPROXYURBUSBIP
{
    /** List node for the in flight or landed URB list. */
    RTLISTNODE         NodeList;
    /** Sequence number the assigned URB is identified by. */
    uint32_t           u32SeqNumUrb;
    /** Pointer to the VUSB URB. */
    PVUSBURB           pVUsbUrb;
} USBPROXYURBUSBIP;
/** Pointer to a USB/IP URB. */
typedef USBPROXYURBUSBIP *PUSBPROXYURBUSBIP;

/**
 * Backend data for the USB/IP USB Proxy device backend.
 */
typedef struct USBPROXYDEVUSBIP
{
    /** IPRT socket handle. */
    RTSOCKET          hSocket;
    /** Pollset with the wakeup pipe and socket. */
    RTPOLLSET         hPollSet;
    /** Pipe endpoint - read (in the pollset). */
    RTPIPE            hPipeR;
    /** Pipe endpoint - write. */
    RTPIPE            hPipeW;
    /** Flag whether the reaper thread was woken up. */
    volatile bool     fWokenUp;
    /** Next sequence number to use for identifying submitted URBs. */
    volatile uint32_t u32SeqNumNext;
    /** Fast mutex protecting the lists below against concurrent access. */
    RTSEMFASTMUTEX    hMtxLists;
    /** List of in flight URBs. */
    RTLISTANCHOR      ListUrbsInFlight;
    /** List of landed URBs. */
    RTLISTANCHOR      ListUrbsLanded;
    /** Port of the USB/IP host to connect to. */
    uint32_t          uPort;
    /** USB/IP host address. */
    char             *pszHost;
    /** USB Bus ID of the device to capture. */
    char             *pszBusId;
    /** The device ID to use to identify the device. */
    uint32_t          u32DevId;
} USBPROXYDEVUSBIP, *PUSBPROXYDEVUSBIP;

/** Pollset id of the socket. */
#define USBIP_POLL_ID_SOCKET 0
/** Pollset id of the pipe. */
#define USBIP_POLL_ID_PIPE   1

/**
 * Converts a request/reply header from network to host endianness.
 *
 * @returns nothing.
 * @param   pHdr    The header to convert.
 */
DECLINLINE(void) usbProxyUsbIpReqRetHdrN2H(PUsbIpReqRetHdr pHdr)
{
    pHdr->u32ReqRet    = RT_H2N_U32(pHdr->u32ReqRet);
    pHdr->u32SeqNum    = RT_H2N_U32(pHdr->u32SeqNum);
    pHdr->u32DevId     = RT_H2N_U32(pHdr->u32DevId);
    pHdr->u32Direction = RT_H2N_U32(pHdr->u32Direction);
    pHdr->u32Endpoint  = RT_H2N_U32(pHdr->u32Endpoint);
}

/**
 * Converts a request/reply header from host to network endianness.
 *
 * @returns nothing.
 * @param   pHdr              The header to convert.
 */
DECLINLINE(void) usbProxyUsbIpReqRetHdrH2N(PUsbIpReqRetHdr pHdr)
{
    pHdr->u32ReqRet    = RT_N2H_U32(pHdr->u32ReqRet);
    pHdr->u32SeqNum    = RT_N2H_U32(pHdr->u32SeqNum);
    pHdr->u32DevId     = RT_N2H_U32(pHdr->u32DevId);
    pHdr->u32Direction = RT_N2H_U32(pHdr->u32Direction);
    pHdr->u32Endpoint  = RT_N2H_U32(pHdr->u32Endpoint);
}

/**
 * Converts a submit request from host to network endianness.
 *
 * @returns nothing.
 * @param   pReqSubmit        The submit request to convert.
 */
DECLINLINE(void) usbProxyUsbIpReqSubmitH2N(PUsbIpReqSubmit pReqSubmit)
{
    usbProxyUsbIpReqRetHdrH2N(&pReqSubmit->Hdr);
    pReqSubmit->u32XferFlags            = RT_H2N_U32(pReqSubmit->u32XferFlags);
    pReqSubmit->u32TransferBufferLength = RT_H2N_U32(pReqSubmit->u32TransferBufferLength);
    pReqSubmit->u32StartFrame           = RT_H2N_U32(pReqSubmit->u32StartFrame);
    pReqSubmit->u32NumIsocPkts          = RT_H2N_U32(pReqSubmit->u32NumIsocPkts);
    pReqSubmit->u32Interval             = RT_H2N_U32(pReqSubmit->u32Interval);
}

/**
 * Converts a submit reply from network to host endianness.
 *
 * @returns nothing.
 * @param   pReqSubmit        The submit reply to convert.
 */
DECLINLINE(void) usbProxyUsbIpRetSubmitN2H(PUsbIpRetSubmit pRetSubmit)
{
    usbProxyUsbIpReqRetHdrN2H(&pRetSubmit->Hdr);
    pRetSubmit->u32Status       = RT_N2H_U32(pRetSubmit->u32Status);
    pRetSubmit->u32ActualLength = RT_N2H_U32(pRetSubmit->u32ActualLength);
    pRetSubmit->u32StartFrame   = RT_N2H_U32(pRetSubmit->u32StartFrame);
    pRetSubmit->u32NumIsocPkts  = RT_N2H_U32(pRetSubmit->u32NumIsocPkts);
    pRetSubmit->u32ErrorCount   = RT_N2H_U32(pRetSubmit->u32ErrorCount);
}

/**
 * Converts a unlink request from host to network endianness.
 *
 * @returns nothing.
 * @param   pReqUnlink        The unlink request to convert.
 */
DECLINLINE(void) usbProxyUsbIpReqUnlinkH2N(PUsbIpReqUnlink pReqUnlink)
{
    usbProxyUsbIpReqRetHdrH2N(&pReqUnlink->Hdr);
    pReqUnlink->u32SeqNum = RT_H2N_U32(pReqUnlink->u32SeqNum);
}

/**
 * Converts a unlink reply from network to host endianness.
 *
 * @returns nothing.
 * @param   pRetUnlink        The unlink reply to convert.
 */
DECLINLINE(void) usbProxyUsbIpRetUnlinkN2H(PUsbIpRetUnlink pRetUnlink)
{
    usbProxyUsbIpReqRetHdrN2H(&pRetUnlink->Hdr);
    pRetUnlink->u32Status = RT_N2H_U32(pRetUnlink->u32Status);
}

/**
 * Convert the given exported device structure from host to network byte order.
 *
 * @returns nothing.
 * @param   pDevice           The device structure to convert.
 */
DECLINLINE(void) usbProxyUsbIpExportedDeviceN2H(PUsbIpExportedDevice pDevice)
{
    pDevice->u32BusNum    = RT_N2H_U32(pDevice->u32BusNum);
    pDevice->u32DevNum    = RT_N2H_U32(pDevice->u32DevNum);
    pDevice->u32Speed     = RT_N2H_U16(pDevice->u32Speed);
    pDevice->u16VendorId  = RT_N2H_U16(pDevice->u16VendorId);
    pDevice->u16ProductId = RT_N2H_U16(pDevice->u16ProductId);
    pDevice->u16BcdDevice = RT_N2H_U16(pDevice->u16BcdDevice);
}

/**
 * Converts a USB/IP status code to VBox status code.
 *
 * @returns VBox status code.
 * @param   u32Status    The USB/IP status code from the reply.
 */
DECLINLINE(int) usbProxyUsbIpErrConvertFromStatus(uint32_t u32Status)
{
    if (RT_LIKELY(u32Status == 0))
        return VINF_SUCCESS;

    return VERR_NOT_IMPLEMENTED;
}

/**
 * Gets the next free sequence number.
 *
 * @returns Next free sequence number.
 * @param   pProxyDevUsbIp    The USB/IP proxy device data.
 */
DECLINLINE(uint32_t) usbProxyUsbIpSeqNumGet(PUSBPROXYDEVUSBIP pProxyDevUsbIp)
{
    return ASMAtomicIncU32(&pProxyDevUsbIp->u32SeqNumNext);
}

/**
 * Links a given URB into the given list.
 *
 * @returns nothing.
 * @param   pProxyDevUsbIp    The USB/IP proxy device data.
 * @param   pList             The list to link the URB into.
 * @param   pUrbUsbIp         The URB to link.
 */
DECLINLINE(void) usbProxyUsbIpLinkUrb(PUSBPROXYDEVUSBIP pProxyDevUsbIp, PRTLISTANCHOR pList, PUSBPROXYURBUSBIP pUrbUsbIp)
{
    int rc = RTSemFastMutexRequest(pProxyDevUsbIp->hMtxLists);
    AssertRC(rc);
    RTListAppend(pList, &pUrbUsbIp->NodeList);
    RTSemFastMutexRelease(pProxyDevUsbIp->hMtxLists);
}

/**
 * Unlinks a given URB from the current assigned list.
 *
 * @returns nothing.
 * @param   pProxyDevUsbIp    The USB/IP proxy device data.
 * @param   pUrbUsbIp         The URB to unlink.
 */
DECLINLINE(void) usbProxyUsbIpUnlinkUrb(PUSBPROXYDEVUSBIP pProxyDevUsbIp, PUSBPROXYURBUSBIP pUrbUsbIp)
{
    int rc = RTSemFastMutexRequest(pProxyDevUsbIp->hMtxLists);
    AssertRC(rc);
    RTListNodeRemove(&pUrbUsbIp->NodeList);
    RTSemFastMutexRelease(pProxyDevUsbIp->hMtxLists);
}

/**
 * Allocates a USB/IP proxy specific URB state.
 *
 * @returns Pointer to the USB/IP specific URB data or NULL on failure.
 * @param   pProxyDevUsbIp    The USB/IP proxy device data.
 */
static PUSBPROXYURBUSBIP usbProxyUsbIpUrbAlloc(PUSBPROXYDEVUSBIP pProxyDevUsbIp)
{
    NOREF(pProxyDevUsbIp);
    return (PUSBPROXYURBUSBIP)RTMemAllocZ(sizeof(USBPROXYURBUSBIP));
}

/**
 * Frees the given USB/IP URB state.
 *
 * @returns nothing.
 * @param   pProxyDevUsbIp    The USB/IP proxy device data.
 * @param   pUrbUsbIp         The USB/IP speciic URB data.
 */
static void usbProxyUsbIpUrbFree(PUSBPROXYDEVUSBIP pProxyDevUsbIp, PUSBPROXYURBUSBIP pUrbUsbIp)
{
    NOREF(pProxyDevUsbIp);
    RTMemFree(pUrbUsbIp);
}

/**
 * Parse the string representation of the host address.
 *
 * @returns VBox status code.
 * @param   pProxyDevUsbIp    The USB/IP proxy device data to parse the address for.
 * @param   pszAddress        The address string to parse.
 */
static int usbProxyUsbIpParseAddress(PUSBPROXYDEVUSBIP pProxyDevUsbIp, const char *pszAddress)
{
    int rc = VINF_SUCCESS;
    uint32_t uPort;
    char *pszPortStart = RTStrStr(pszAddress, ":");
    char *pszHost = NULL;
    if (pszPortStart)
    {
        size_t cbHost = pszPortStart - pszAddress;
        pszPortStart++;
        rc = RTStrToUInt32Ex(pszPortStart, NULL, 10 /* uBase */, &pProxyDevUsbIp->uPort);
        if (   rc == VINF_SUCCESS
            || cbHost == 0)
        {
            rc = RTStrAllocEx(&pProxyDevUsbIp->pszHost, cbHost + 1);
            if (RT_SUCCESS(rc))
            {
                rc = RTStrCopyEx(pProxyDevUsbIp->pszHost, cbHost + 1, pszAddress, cbHost);
                AssertRC(rc);
                return VINF_SUCCESS;
            }
        }
        else
            rc = VERR_INVALID_PARAMETER;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    return rc;
}

/**
 * Connects to the USB/IP host and claims the device given in the proxy device data.
 *
 * @returns VBox status.
 * @param   pProxyDevUsbIp    The USB/IP proxy device data.
 */
static int usbProxyUsbIpConnect(PUSBPROXYDEVUSBIP pProxyDevUsbIp)
{
    int rc = VINF_SUCCESS;
    rc = RTTcpClientConnect(pProxyDevUsbIp->pszHost, pProxyDevUsbIp->uPort, &pProxyDevUsbIp->hSocket);
    if (RT_SUCCESS(rc))
    {
        /* Disable send coalescing. */
        rc = RTTcpSetSendCoalescing(pProxyDevUsbIp->hSocket, false);
        if (RT_FAILURE(rc))
            LogRel(("UsbIp: Disabling send coalescing failed (rc=%Rrc), continuing nevertheless but expect reduced performance\n", rc));

        /* Import the device, i.e. claim it for our use. */
        UsbIpReqImport ReqImport;
        ReqImport.u16Version = RT_H2N_U16(USBIP_VERSION);
        ReqImport.u16Cmd     = RT_H2N_U16(USBIP_INDICATOR_REQ | USBIP_REQ_RET_IMPORT);
        ReqImport.u32Status  = RT_H2N_U32(0);
        rc = RTStrCopy(&ReqImport.aszBusId[0], sizeof(ReqImport.aszBusId[0]), pProxyDevUsbIp->pszBusId);
        if (rc == VINF_SUCCESS)
        {
            rc = RTTcpWrite(pProxyDevUsbIp->hSocket, &ReqImport, sizeof(ReqImport));
            if (RT_SUCCESS(rc))
            {
                /* Read the reply. */
                UsbIpRetImport RetImport;
                rc = RTTcpRead(pProxyDevUsbIp->hSocket, &RetImport, sizeof(RetImport), NULL);
                if (RT_SUCCESS(rc))
                {
                    RetImport.u16Version = RT_N2H_U16(RetImport.u16Version);
                    RetImport.u16Cmd     = RT_N2H_U16(RetImport.u16Cmd);
                    RetImport.u32Status  = RT_N2H_U16(RetImport.u32Status);
                    if (   RetImport.u16Version == USBIP_VERSION
                        && RetImport.u16Cmd == USBIP_REQ_RET_IMPORT
                        && RetImport.u32Status == 0)
                    {
                        /* Read the device data. */
                        UsbIpExportedDevice Device;
                        rc = RTTcpRead(pProxyDevUsbIp->hSocket, &Device, sizeof(Device), NULL);
                        if (RT_SUCCESS(rc))
                        {
                            usbProxyUsbIpExportedDeviceN2H(&Device);
                            pProxyDevUsbIp->u32DevId = (Device.u32BusNum << 16) | Device.u32DevNum;

                            rc = RTPollSetAddSocket(pProxyDevUsbIp->hPollSet, pProxyDevUsbIp->hSocket,
                                                    RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, USBIP_POLL_ID_SOCKET);
                        }
                    }
                    else
                    {
                        /* Check what went wrong and leave a meaningful error message in the log. */
                        if (RetImport.u16Version != USBIP_VERSION)
                            LogRel(("UsbIp: Unexpected protocol version received from host (%#x vs. %#x)\n",
                                    RetImport.u16Version, USBIP_VERSION));
                        else if (RetImport.u16Cmd != USBIP_REQ_RET_IMPORT)
                            LogRel(("UsbIp: Unexpected reply code received from host (%#x vs. %#x)\n",
                                    RetImport.u16Cmd, USBIP_REQ_RET_IMPORT));
                        else if (RetImport.u32Status != 0)
                            LogRel(("UsbIp: Claiming the device has failed on the host with an unspecified error\n"));
                        else
                            AssertMsgFailed(("Something went wrong with if condition\n"));
                    }
                }
            }
        }
        else
        {
            LogRel(("UsbIp: Given bus ID is exceeds permitted protocol length: %u vs %u\n",
                    strlen(pProxyDevUsbIp->pszBusId) + 1, sizeof(ReqImport.aszBusId[0])));
            rc = VERR_INVALID_PARAMETER;
        }

        if (RT_FAILURE(rc))
            RTTcpClientCloseEx(pProxyDevUsbIp->hSocket, false /*fGracefulShutdown*/);
    }
    if (RT_FAILURE(rc))
        LogRel(("UsbIp: Connecting to the host %s failed with %Rrc\n", pProxyDevUsbIp->pszHost, rc));
    return rc;
}

/**
 * Disconnects from the USB/IP host releasing the device given in the proxy device data.
 *
 * @returns VBox status code.
 * @param   pProxyDevUsbIp    The USB/IP proxy device data.
 */
static int usbProxyUsbIpDisconnect(PUSBPROXYDEVUSBIP pProxyDevUsbIp)
{
    int rc = VINF_SUCCESS;

    rc = RTTcpClientCloseEx(pProxyDevUsbIp->hSocket, false /*fGracefulShutdown*/);
    if (RT_SUCCESS(rc))
        pProxyDevUsbIp->hSocket = NIL_RTSOCKET;
    return rc;
}

/**
 * Synchronously exchange a given control message with the remote device.
 *
 * @eturns VBox status code.
 * @param  pProxyDevUsbIp    The USB/IP proxy device data.
 * @param  pSetup            The setup message.
 *
 * @note This method is only used to implement the *SetConfig, *SetInterface and *ClearHaltedEp
 *       callbacks because the USB/IP protocol lacks dedicated requests for these.
 * @remark It is assumed that this method is never called while usbProxyUsbIpUrbReap is called
 *         on another thread.
 */
static int usbProxyUsbIpCtrlUrbExchangeSync(PUSBPROXYDEVUSBIP pProxyDevUsbIp, PVUSBSETUP pSetup)
{
    int rc = VINF_SUCCESS;

    UsbIpReqSubmit ReqSubmit;
    uint32_t u32SeqNum = usbProxyUsbIpSeqNumGet(pProxyDevUsbIp);
    ReqSubmit.Hdr.u32ReqRet           = USBIP_CMD_SUBMIT;
    ReqSubmit.Hdr.u32SeqNum           = u32SeqNum;
    ReqSubmit.Hdr.u32DevId            = pProxyDevUsbIp->u32DevId;
    ReqSubmit.Hdr.u32Direction        = USBIP_DIR_OUT;
    ReqSubmit.Hdr.u32Endpoint         = 0; /* Only default control endpoint is allowed for these kind of messages. */
    ReqSubmit.u32XferFlags            = 0;
    ReqSubmit.u32TransferBufferLength = 0;
    ReqSubmit.u32StartFrame           = 0;
    ReqSubmit.u32NumIsocPkts          = 0;
    ReqSubmit.u32Interval             = 0;
    memcpy(&ReqSubmit.Setup, pSetup, sizeof(ReqSubmit.Setup));
    usbProxyUsbIpReqSubmitH2N(&ReqSubmit);

    /* Send the command. */
    rc = RTTcpWrite(pProxyDevUsbIp->hSocket, &ReqSubmit, sizeof(ReqSubmit));
    if (RT_SUCCESS(rc))
    {
        /* Wait for the response. */
        /** @todo: Don't wait indefinitely long. */
        UsbIpRetSubmit RetSubmit;
        rc = RTTcpRead(pProxyDevUsbIp->hSocket, &RetSubmit, sizeof(RetSubmit), NULL);
        if (RT_SUCCESS(rc))
        {
            usbProxyUsbIpRetSubmitN2H(&RetSubmit);
            rc = usbProxyUsbIpErrConvertFromStatus(RetSubmit.u32Status);
        }
    }
    return rc;
}

/*
 * The USB proxy device functions.
 */

static DECLCALLBACK(int) usbProxyUsbIpOpen(PUSBPROXYDEV pProxyDev, const char *pszAddress, void *pvBackend)
{
    LogFlowFunc(("pProxyDev=%p pszAddress=%s, pvBackend=%p\n", pProxyDev, pszAddress, pvBackend));

    PUSBPROXYDEVUSBIP pDevUsbIp = USBPROXYDEV_2_DATA(pProxyDev, PUSBPROXYDEVUSBIP);
    int rc = VINF_SUCCESS;

    RTListInit(&pDevUsbIp->ListUrbsInFlight);
    RTListInit(&pDevUsbIp->ListUrbsLanded);
    pDevUsbIp->hSocket       = NIL_RTSOCKET;
    pDevUsbIp->hPollSet      = NIL_RTPOLLSET;
    pDevUsbIp->hPipeW        = NIL_RTPIPE;
    pDevUsbIp->hPipeR        = NIL_RTPIPE;
    pDevUsbIp->fWokenUp      = false;
    pDevUsbIp->u32SeqNumNext = 0;
    pDevUsbIp->pszHost       = NULL;
    pDevUsbIp->pszBusId      = NULL;

    /* Setup wakeup pipe and poll set first. */
    rc = RTPipeCreate(&pDevUsbIp->hPipeR, &pDevUsbIp->hPipeW, 0);
    if (RT_SUCCESS(rc))
    {
        rc = RTPollSetCreate(&pDevUsbIp->hPollSet);
        if (RT_SUCCESS(rc))
        {
            rc = RTPollSetAddPipe(pDevUsbIp->hPollSet, pDevUsbIp->hPipeR,
                                  RTPOLL_EVT_READ, USBIP_POLL_ID_PIPE);
            if (RT_SUCCESS(rc))
            {
                /* Connect to the USB/IP host. */
                rc = usbProxyUsbIpParseAddress(pDevUsbIp, pszAddress);
                if (RT_SUCCESS(rc))
                    rc = usbProxyUsbIpConnect(pDevUsbIp);
            }

            if (RT_FAILURE(rc))
            {
                RTPollSetRemove(pDevUsbIp->hPollSet, USBIP_POLL_ID_PIPE);
                int rc2 = RTPollSetDestroy(pDevUsbIp->hPollSet);
                AssertRC(rc2);
            }
        }

        if (RT_FAILURE(rc))
        {
            int rc2 = RTPipeClose(pDevUsbIp->hPipeR);
            AssertRC(rc2);
            rc2 = RTPipeClose(pDevUsbIp->hPipeW);
            AssertRC(rc2);
        }
    }

    return rc;
}

static DECLCALLBACK(void) usbProxyUsbIpClose(PUSBPROXYDEV pProxyDev)
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("pProxyDev = %p\n", pProxyDev));

    PUSBPROXYDEVUSBIP pDevUsbIp = USBPROXYDEV_2_DATA(pProxyDev, PUSBPROXYDEVUSBIP);
    /* Destroy the pipe and pollset if necessary. */
    if (pDevUsbIp->hPollSet != NIL_RTPOLLSET)
    {
        if (pDevUsbIp->hSocket != NIL_RTSOCKET)
        {
            rc = RTPollSetRemove(pDevUsbIp->hPollSet, USBIP_POLL_ID_SOCKET);
            Assert(RT_SUCCESS(rc) || rc == VERR_POLL_HANDLE_ID_NOT_FOUND);
        }
        rc = RTPollSetRemove(pDevUsbIp->hPollSet, USBIP_POLL_ID_PIPE);
        AssertRC(rc);
        rc = RTPollSetDestroy(pDevUsbIp->hPollSet);
        AssertRC(rc);
        rc = RTPipeClose(pDevUsbIp->hPipeR);
        AssertRC(rc);
        rc = RTPipeClose(pDevUsbIp->hPipeW);
        AssertRC(rc);
    }

    if (pDevUsbIp->hSocket != NIL_RTSOCKET)
        usbProxyUsbIpDisconnect(pDevUsbIp);
    if (pDevUsbIp->pszHost)
        RTStrFree(pDevUsbIp->pszHost);
    if (pDevUsbIp->pszBusId)
        RTStrFree(pDevUsbIp->pszBusId);

    /* Clear the URB lists. */
    rc = RTSemFastMutexRequest(pDevUsbIp->hMtxLists);
    AssertRC(rc);
    PUSBPROXYURBUSBIP pIter = NULL;
    PUSBPROXYURBUSBIP pIterNext = NULL;
    RTListForEachSafe(&pDevUsbIp->ListUrbsInFlight, pIter, pIterNext, USBPROXYURBUSBIP, NodeList)
    {
        RTListNodeRemove(&pIter->NodeList);
        RTMemFree(pIter);
    }

    RTListForEachSafe(&pDevUsbIp->ListUrbsLanded, pIter, pIterNext, USBPROXYURBUSBIP, NodeList)
    {
        RTListNodeRemove(&pIter->NodeList);
        RTMemFree(pIter);
    }
    RTSemFastMutexRelease(pDevUsbIp->hMtxLists);
    RTSemFastMutexDestroy(pDevUsbIp->hMtxLists);
}

static DECLCALLBACK(int) usbProxyUsbIpReset(PUSBPROXYDEV pProxyDev, bool fResetOnLinux)
{
    LogFlowFunc(("pProxyDev = %p\n", pProxyDev));

    PUSBPROXYDEVUSBIP pDev = USBPROXYDEV_2_DATA(pProxyDev, PUSBPROXYDEVUSBIP);
    return VINF_SUCCESS; /* No way to reset the device with the current protocol. */
}

static DECLCALLBACK(int) usbProxyUsbIpSetConfig(PUSBPROXYDEV pProxyDev, int iCfg)
{
    LogFlowFunc(("pProxyDev=%s cfg=%#x\n", pProxyDev->pUsbIns->pszName, iCfg));

    PUSBPROXYDEVUSBIP pDev = USBPROXYDEV_2_DATA(pProxyDev, PUSBPROXYDEVUSBIP);
    VUSBSETUP Setup;

    Setup.bmRequestType = 0;
    Setup.bRequest      = 0x09;
    Setup.wValue        = iCfg;
    Setup.wIndex        = 0;
    Setup.wLength       = 0;
    return usbProxyUsbIpCtrlUrbExchangeSync(pDev, &Setup);
}

static DECLCALLBACK(int) usbProxyUsbIpClaimInterface(PUSBPROXYDEV pProxyDev, int ifnum)
{
    LogFlowFunc(("pProxyDev=%s ifnum=%#x\n", pProxyDev->pUsbIns->pszName, ifnum));
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) usbProxyUsbIpReleaseInterface(PUSBPROXYDEV pProxyDev, int ifnum)
{
    LogFlowFunc(("pProxyDev=%s ifnum=%#x\n", pProxyDev->pUsbIns->pszName, ifnum));
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) usbProxyUsbIpSetInterface(PUSBPROXYDEV pProxyDev, int ifnum, int setting)
{
    LogFlowFunc(("pProxyDev=%p ifnum=%#x setting=%#x\n", pProxyDev, ifnum, setting));

    PUSBPROXYDEVUSBIP pDev = USBPROXYDEV_2_DATA(pProxyDev, PUSBPROXYDEVUSBIP);
    VUSBSETUP Setup;

    Setup.bmRequestType = 0x1;
    Setup.bRequest      = 0x11; /* SET_INTERFACE */
    Setup.wValue        = setting;
    Setup.wIndex        = ifnum;
    Setup.wLength       = 0;
    return usbProxyUsbIpCtrlUrbExchangeSync(pDev, &Setup);
}

static DECLCALLBACK(int) usbProxyUsbIpClearHaltedEp(PUSBPROXYDEV pProxyDev, unsigned int iEp)
{
    LogFlowFunc(("pProxyDev=%s ep=%u\n", pProxyDev->pUsbIns->pszName, iEp));

    PUSBPROXYDEVUSBIP pDev = USBPROXYDEV_2_DATA(pProxyDev, PUSBPROXYDEVUSBIP);
    VUSBSETUP Setup;

    Setup.bmRequestType = 0x2;
    Setup.bRequest      = 0x01; /* CLEAR_FEATURE */
    Setup.wValue        = 0x00; /* ENDPOINT_HALT */
    Setup.wIndex        = iEp;
    Setup.wLength       = 0;
    return usbProxyUsbIpCtrlUrbExchangeSync(pDev, &Setup);
}

static DECLCALLBACK(int) usbProxyUsbIpUrbQueue(PUSBPROXYDEV pProxyDev, PVUSBURB pUrb)
{
    LogFlowFunc(("pUrb=%p\n", pUrb));

    PUSBPROXYDEVUSBIP pProxyDevUsbIp = USBPROXYDEV_2_DATA(pProxyDev, PUSBPROXYDEVUSBIP);

    /* Allocate a USB/IP Urb. */
    PUSBPROXYURBUSBIP pUrbUsbIp = usbProxyUsbIpUrbAlloc(pProxyDevUsbIp);
    if (!pUrbUsbIp)
        return VERR_NO_MEMORY;

    pUrbUsbIp->u32SeqNumUrb = usbProxyUsbIpSeqNumGet(pProxyDevUsbIp);

    UsbIpReqSubmit ReqSubmit;
    ReqSubmit.Hdr.u32ReqRet           = USBIP_CMD_SUBMIT;
    ReqSubmit.Hdr.u32SeqNum           = pUrbUsbIp->u32SeqNumUrb;
    ReqSubmit.Hdr.u32DevId            = pProxyDevUsbIp->u32DevId;
    ReqSubmit.Hdr.u32Endpoint         = pUrb->EndPt;
    ReqSubmit.Hdr.u32Direction        = pUrb->enmDir == VUSBDIRECTION_IN ? USBIP_DIR_IN : USBIP_DIR_OUT;
    ReqSubmit.u32XferFlags            = 0;
    if (pUrb->enmDir == VUSBDIRECTION_IN && pUrb->fShortNotOk)
        ReqSubmit.u32XferFlags |= USBIP_XFER_FLAGS_SHORT_NOT_OK;

    ReqSubmit.u32TransferBufferLength = pUrb->cbData;
    ReqSubmit.u32StartFrame           = 0;
    ReqSubmit.u32NumIsocPkts          = 0;
    ReqSubmit.u32Interval             = 0;

    switch (pUrb->enmType)
    {
        case VUSBXFERTYPE_MSG:
            memcpy(&ReqSubmit.Setup, &pUrb->abData, sizeof(ReqSubmit.Setup));
            LogFlowFunc(("Message (Control) URB\n"));
            break;
        case VUSBXFERTYPE_ISOC:
            ReqSubmit.u32XferFlags |= USBIP_XFER_FLAGS_ISO_ASAP;
            ReqSubmit.u32NumIsocPkts = pUrb->cIsocPkts;
#if 0
            for (unsigned i = 0; i < pUrb->cIsocPkts; i++)
            {
                pUrbLnx->KUrb.iso_frame_desc[i].length = pUrb->aIsocPkts[i].cb;
                pUrbLnx->KUrb.iso_frame_desc[i].actual_length = 0;
                pUrbLnx->KUrb.iso_frame_desc[i].status = 0x7fff;
            }
#else /** @todo: Implement isochronous support */
            usbProxyUsbIpUrbFree(pProxyDevUsbIp, pUrbUsbIp);
            return VERR_NOT_SUPPORTED;
#endif
            break;
        case VUSBXFERTYPE_BULK:
        case VUSBXFERTYPE_INTR:
            break;
        default:
            usbProxyUsbIpUrbFree(pProxyDevUsbIp, pUrbUsbIp);
            return VERR_INVALID_PARAMETER; /** @todo: better status code. */
    }
    usbProxyUsbIpReqSubmitH2N(&ReqSubmit);

    /* Send the command. */
    RTSGBUF SgBufReq;
    RTSGSEG aSegReq[2];
    aSegReq[0].pvSeg = &ReqSubmit;
    aSegReq[0].cbSeg = sizeof(ReqSubmit);
    aSegReq[1].pvSeg = &pUrb->abData[0];
    aSegReq[1].cbSeg = pUrb->cbData;
    RTSgBufInit(&SgBufReq, &aSegReq[0], RT_ELEMENTS(aSegReq));

    int rc = RTTcpSgWrite(pProxyDevUsbIp->hSocket, &SgBufReq);
    if (RT_SUCCESS(rc))
    {
        /* Link the URB into the list of in flight URBs. */
        pUrb->Dev.pvPrivate = pUrbUsbIp;
        pUrbUsbIp->pVUsbUrb = pUrb;
        usbProxyUsbIpLinkUrb(pProxyDevUsbIp, &pProxyDevUsbIp->ListUrbsInFlight, pUrbUsbIp);
    }
    else
        usbProxyUsbIpUrbFree(pProxyDevUsbIp, pUrbUsbIp);

    return rc;
}

static DECLCALLBACK(PVUSBURB) usbProxyUsbIpUrbReap(PUSBPROXYDEV pProxyDev, RTMSINTERVAL cMillies)
{
    LogFlowFunc(("pProxyDev=%s\n", pProxyDev->pUsbIns->pszName));

    PUSBPROXYDEVUSBIP pDev = USBPROXYDEV_2_DATA(pProxyDev, PUSBPROXYDEVUSBIP);
    PUSBPROXYURBUSBIP pUrbUsbIp = NULL;
    PVUSBURB pUrb = NULL;

    /* Any URBs pending delivery? */
    if (!RTListIsEmpty(&pDev->ListUrbsLanded))
    {
        pUrbUsbIp = RTListGetFirst(&pDev->ListUrbsLanded, USBPROXYURBUSBIP, NodeList);
        if (pUrbUsbIp)
        {
            /* unlink from the pending delivery list */
            usbProxyUsbIpUnlinkUrb(pDev, pUrbUsbIp);
        }
    }

    if (!pUrbUsbIp)
    {
        uint32_t uIdReady = 0;
        uint32_t fEventsRecv = 0;

        if (!ASMAtomicXchgBool(&pDev->fWokenUp, false))
        {
            int rc = RTPoll(pDev->hPollSet, cMillies, &fEventsRecv, &uIdReady);
            Assert(RT_SUCCESS(rc) || rc == VERR_TIMEOUT);
        }

        UsbIpRet Reply;
    }

    if (pUrbUsbIp)
    {

    }

    return pUrb;
}

static DECLCALLBACK(int) usbProxyUsbIpUrbCancel(PUSBPROXYDEV pProxyDev, PVUSBURB pUrb)
{
    LogFlowFunc(("pUrb=%p\n", pUrb));

    PUSBPROXYDEVUSBIP pProxyDevUsbIp = USBPROXYDEV_2_DATA(pProxyDev, PUSBPROXYDEVUSBIP);
    PUSBPROXYURBUSBIP pUrbUsbIp = (PUSBPROXYURBUSBIP)pUrb->Dev.pvPrivate;
    UsbIpReqUnlink ReqUnlink;

    uint32_t u32SeqNum = usbProxyUsbIpSeqNumGet(pProxyDevUsbIp);
    ReqUnlink.Hdr.u32ReqRet           = USBIP_CMD_UNLINK;
    ReqUnlink.Hdr.u32SeqNum           = u32SeqNum;
    ReqUnlink.Hdr.u32DevId            = pProxyDevUsbIp->u32DevId;
    ReqUnlink.Hdr.u32Direction        = USBIP_DIR_OUT;
    ReqUnlink.Hdr.u32Endpoint         = pUrb->EndPt;
    ReqUnlink.u32SeqNum               = pUrbUsbIp->u32SeqNumUrb;

    int rc = RTTcpWrite(pProxyDevUsbIp->hSocket, &ReqUnlink, sizeof(ReqUnlink));
    /* Wait for the reply. */
    return rc;
}

static DECLCALLBACK(int) usbProxyUsbIpWakeup(PUSBPROXYDEV pProxyDev)
{
    LogFlowFunc(("pProxyDev=%s\n", pProxyDev->pUsbIns->pszName));

    PUSBPROXYDEVUSBIP pDevUsbIp = USBPROXYDEV_2_DATA(pProxyDev, PUSBPROXYDEVUSBIP);
    int rc = VINF_SUCCESS;

    if (!ASMAtomicXchgBool(&pDevUsbIp->fWokenUp, true))
    {
        size_t cbWritten = 0;

        rc = RTPipeWrite(pDevUsbIp->hPipeW, "", 1, &cbWritten);
        Assert(RT_SUCCESS(rc) || cbWritten == 0);
    }

    return rc;
}

/**
 * The USB/IP USB Proxy Backend operations.
 */
extern const USBPROXYBACK g_USBProxyDeviceUsbIp =
{
    /* pszName */
    "usbip",
    /* cbBackend */
    sizeof(USBPROXYDEVUSBIP),
    usbProxyUsbIpOpen,
    NULL,
    usbProxyUsbIpClose,
    usbProxyUsbIpReset,
    usbProxyUsbIpSetConfig,
    usbProxyUsbIpClaimInterface,
    usbProxyUsbIpReleaseInterface,
    usbProxyUsbIpSetInterface,
    usbProxyUsbIpClearHaltedEp,
    usbProxyUsbIpUrbQueue,
    usbProxyUsbIpUrbCancel,
    usbProxyUsbIpUrbReap,
    usbProxyUsbIpWakeup,
    0
};

