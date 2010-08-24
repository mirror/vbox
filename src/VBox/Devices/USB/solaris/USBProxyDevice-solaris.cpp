/* $Id$ */
/** @file
 * USB device proxy - the Solaris backend.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
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

#include <limits.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/usb/usba.h>
#include <sys/usb/clients/ugen/usb_ugen.h>
#include <sys/asynch.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/pdm.h>

#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/critsect.h>
#include <iprt/time.h>
#include <iprt/file.h>
#include "../USBProxyDevice.h"
#include <VBox/usblib.h>

/* WORK-IN-PROGRESS */
/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Log Prefix. */
#define USBR3PROXY              "USBProxyDev"
/** Maximum endpoints supported. */
#define USBSOL_MAXENDPOINTS     32
/** Maximum interfaces supported. */
#define USBSOL_MAXINTERFACES    256
/** EndPoint address mask (bEndPointAddress) */
#define USBSOL_EPADDR_NUM_MASK  0x0F
#define USBSOL_EPADDR_DIR_MASK  0x80
#define USBSOL_EPADDR_IN        0x80
#define USBSOL_EPADDR_OUT       0x00
#define USBSOL_CTRLREQ_SIZE     0x08
/** This really needs to be defined in vusb.h! */
#ifndef VUSB_DIR_TO_DEV
# define VUSB_DIR_TO_DEV        0x00
#endif

/** Ugen isoc support toggle, disable this for S10. */
//#define VBOX_WITH_SOLARIS_USB_ISOC

/** -XXX- Remove this hackery eventually */
#if 0
#ifdef DEBUG_ramshankar
# undef LogFlow
# define LogFlow                 LogRel
# undef Log
# define Log                     LogRel
#endif
#endif

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct USBPROXYURBSOL;

/**
 * Asynchronous URB status.
 */
typedef struct URBSTATUSSOL
{
    /** The result of the transfer. */
    aio_result_t        Result;
    /** The URB associated with this transfer. */
    USBPROXYURBSOL     *pUrbSol;
} URBSTATUSSOL, *PURBSTATUSSOL;

/**
 * Wrapper around the solaris urb request structure.
 * This is required to track in-flight and landed URBs.
 */
typedef struct USBPROXYURBSOL
{
    /** Pointer to the VUSB URB (set to NULL if canceled). */
    PVUSBURB                pVUsbUrb;
    /** The millisecond timestamp when this URB was submitted. */
    uint64_t                u64SubmitTS;
    /** Pointer to the Solaris device. */
    struct USBPROXYDEVSOL  *pDevSol;
    /** Status of this URB. */
    URBSTATUSSOL            Status;
#ifdef VBOX_WITH_SOLARIS_USB_ISOC
    /** Isoc. OUT buffer. */
    caddr_t                 pIsocBufOut;
    /** Size of the isoc. OUT buffer */
    size_t                  cbIsocBufOut;
    /** Isoc. IN buffer. */
    caddr_t                 pIsocBufIn;
    /** Size of the isoc. IN buffer. */
    size_t                  cbIsocBufIn;
#endif

    /** Pointer to the next solaris URB. */
    struct USBPROXYURBSOL  *pNext;
    /** Pointer to the previous solaris URB. */
    struct USBPROXYURBSOL  *pPrev;
} USBPROXYURBSOL, *PUSBPROXYURBSOL;

/**
 * Data for the solaris usb proxy backend.
 */
typedef struct USBPROXYDEVSOL
{
    /** Path of the USB device in the dev tree (driver bound). */
    char               *pszDevPath;
    /** Path of the USB device in the devices tree (persistent). */
    char               *pszDevicePath;
    /** Status endpoint. */
    RTFILE              StatFile;
    /** Pointer to the proxy device instance. */
    PUSBPROXYDEV        pProxyDev;
    /** Array of all endpoints. */
    RTFILE              aEpFile[USBSOL_MAXENDPOINTS];
    /** Array of all status endpoints.*/
    RTFILE              aEpStatFile[USBSOL_MAXENDPOINTS];

    /** Critical section protecting the two lists. */
    RTCRITSECT          CritSect;
    /** The list of free solaris URBs. Singly linked. */
    PUSBPROXYURBSOL     pFreeHead;
    /** The list of active solaris URBs. Doubly linked.
     * We must maintain this so we can properly reap URBs of a detached device.
     * Only the split head will appear in this list. */
    PUSBPROXYURBSOL     pInFlightHead;
    /** The list of landed solaris URBs. Doubly linked.
     * Only the split head will appear in this list. */
    PUSBPROXYURBSOL     pTaxingHead;
    /** The tail of the landed solaris URBs. */
    PUSBPROXYURBSOL     pTaxingTail;
} USBPROXYDEVSOL, *PUSBPROXYDEVSOL;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void usbProxySolarisAsyncNotify(bool fActive);

static int usbProxySolarisSendReq(RTFILE File, RTFILE StatFile, PVUSBSETUP pReq, caddr_t pData, uint16_t cbData, int *pStatus);
static int usbProxySolarisSendReqAsync(RTFILE File, PVUSBSETUP pReq, caddr_t pData, uint16_t cbData, PURBSTATUSSOL pUrbStatus);
static int usbProxySolarisCtrlReq(RTFILE File, RTFILE StatFile, PVUSBSETUP pReq, caddr_t pData, uint16_t cbData, int *pStatus);
static int usbProxySolarisCtrlReqAsync(RTFILE File, PVUSBSETUP pReq, caddr_t pData, uint16_t cbData, PURBSTATUSSOL pStatus);
static int usbProxySolarisIO(RTFILE File, RTFILE StatFile, caddr_t pData, size_t cbData, int fOpenMode, int *pStatus);
static int usbProxySolarisIOAsync(RTFILE File, caddr_t pData, size_t cbData, int fOpenMode, PURBSTATUSSOL pUrbStatus);

static void usbProxySolarisInitAllPipes(PUSBPROXYDEVSOL pDevSol);
static int usbProxySolarisOpenDevPipes(PUSBPROXYDEVSOL pDevSol);
static void usbProxySolarisCloseDevPipes(PUSBPROXYDEVSOL pDevSol);
static void usbProxySolarisCloseEndPoints(PUSBPROXYDEVSOL pDevSol);
static void usbProxySolarisCloseDevice(PUSBPROXYDEVSOL pDevSol);

static int usbProxySolarisGetStatus(RTFILE StatFile, int *pStatus, bool fDevStat);
static int usbProxySolarisGetActiveCfg(PUSBPROXYDEVSOL pDevSol, int *pConfigValue);

static int usbProxySolarisConfigIndex(PUSBPROXYDEVSOL pDevSol, uint8_t bCfgValue);
static int usbProxySolarisSeizeAllInterfaces(PUSBPROXYDEVSOL pDevSol, uint8_t bCfgValue);
static VUSBXFERTYPE usbProxySolarisXferType(uint8_t bmAttribute);
static uint32_t usbProxySolarisOpenMode(uint8_t bmEndPointAddress, VUSBXFERTYPE XferType);
static int usbProxySolarisOpenEndPoint(PUSBPROXYDEVSOL pDevSol, int EndPoint, VUSBXFERTYPE XferType);
static int usbProxySolarisOpenNode(PUSBPROXYDEVSOL pDevSol, uint8_t bCfgValue, int iInterface, int iAlternate, int EndPoint,
                    VUSBXFERTYPE XferType);

static void usbProxySolarisAsyncComplete(int Sig);


/**
 * Toggle asynchronous notification signals for asynchronous USB IO.
 *
 * @param fNotify   Whether notification is active.
 */
static void usbProxySolarisAsyncNotify(bool fNotify)
{
    if (fNotify)
        sigset(SIGIO, usbProxySolarisAsyncComplete);
    else
        sigrelse(SIGIO);
}


/**
 * Translate the USB ugen status code to a VUSB status.
 *
 * @returns VUSB URB status code.
 * @param   irc     ugen status code.
 */
static VUSBSTATUS vusbProxySolarisStatusToVUsbStatus(int irc)
{
    LogFlow((USBR3PROXY ":vusbProxySolarisStatusToVUsbStatus irc=%d\n", irc));

    switch (irc)
    {
        case USB_LC_STAT_NOERROR:             return VUSBSTATUS_OK;
        case USB_LC_STAT_CRC:                 return VUSBSTATUS_CRC;
        case USB_LC_STAT_STALL:               return VUSBSTATUS_STALL;
        case USB_LC_STAT_DEV_NOT_RESP:        return VUSBSTATUS_DNR;
        case USB_LC_STAT_DATA_OVERRUN:        return VUSBSTATUS_DATA_OVERRUN;
        case USB_LC_STAT_DATA_UNDERRUN:       return VUSBSTATUS_DATA_UNDERRUN;
        case USB_LC_STAT_TIMEOUT:             return VUSBSTATUS_DNR;
        case USB_LC_STAT_NOT_ACCESSED:        return VUSBSTATUS_NOT_ACCESSED;
        //case USB_LC_STAT_BITSTUFFING:
        //case USB_LC_STAT_DATA_TOGGLE_MM:
        //case USB_LC_STAT_PID_CHECKFAILURE:
        //case USB_LC_STAT_UNEXP_PID:
        //case USB_LC_STAT_BUFFER_OVERRUN:
        //case USB_LC_STAT_BUFFER_UNDERRUN:
        //case USB_LC_STAT_UNSPECIFIED_ERR:
        //case USB_LC_STAT_NO_BANDWIDTH:
        //case USB_LC_STAT_HW_ERR:
        //case USB_LC_STAT_SUSPENDED:
        //case USB_LC_STAT_DISCONNECTED:
        //case USB_LC_STAT_INTR_BUF_FULL:
        //case USB_LC_STAT_INVALID_REQ:
        //case USB_LC_STAT_INTERRUPTED:
        //case USB_LC_STAT_NO_RESOURCES:
        //case USB_LC_STAT_INTR_POLLING_FAILED:
        default:
            LogRel((USBR3PROXY ":vusbProxySolarisStatusToVUsbStatus irc=%#x!!\n", irc));
            return VUSBSTATUS_STALL;
    }
}


/**
 * Setup a USB request packet.
 */
static void usbProxySolarisSetupReq(PVUSBSETUP pSetupData, uint8_t bmRequestType, uint8_t bRequest,
                        uint16_t wValue, uint16_t wIndex, uint16_t wLength)
{
    AssertCompile(sizeof(VUSBSETUP) == USBSOL_CTRLREQ_SIZE);
    LogFlow((USBR3PROXY ":usbProxySolarisSetupReq pSetupData=%p bmRequestType=%#x bRequest=%#x wValue=%#x wIndex=%#x wLength=%#x\n",
                pSetupData, bmRequestType, bRequest, wValue, wIndex, wLength));

    pSetupData->bmRequestType = bmRequestType;
    pSetupData->bRequest      = bRequest;

    /* Handle endianess here. Currently no swapping is needed. */
    pSetupData->wValue  = wValue;
    pSetupData->wIndex  = wIndex;
    pSetupData->wLength = wLength;
}


/**
 * Helper for sending messages through an endpoint, Synchronous version.
 * Note: pData is assumed to be (pReq + 1) i.e. following the request! Caller's responsibility.
 *
 * @returns Number of bytes transferred.
 * @param   pStatus     Where to store the VUSB status (Optional).
 */
static int usbProxySolarisSendReq(RTFILE File, RTFILE StatFile, PVUSBSETUP pReq, caddr_t pData, uint16_t cbData, int *pStatus)
{
    /* Handle read request */
    if (pReq->bmRequestType & VUSB_DIR_TO_HOST)
        return usbProxySolarisIO(File, StatFile, (caddr_t)pReq, sizeof(VUSBSETUP), RTFILE_O_WRITE, pStatus);

    /* Handle write request */
    return usbProxySolarisIO(File, StatFile, (caddr_t)pReq, sizeof(VUSBSETUP) + cbData, RTFILE_O_WRITE, pStatus);
}


/**
 * Helper for sending messages through an endpoint, Asynchronous version.
 * Note: pData is assumed to (pReq + 1) i.e. following the request! Caller's responsibility.
 *
 * @returns VBox status code
 * @param   pUrbStatus     Where to store the status.
 */
static int usbProxySolarisSendReqAsync(RTFILE File, PVUSBSETUP pReq, caddr_t pData, uint16_t cbData, PURBSTATUSSOL pUrbStatus)
{
    /* Handle read request */
    if (pReq->bmRequestType & VUSB_DIR_TO_HOST)
        return usbProxySolarisIOAsync(File, (caddr_t)pReq, sizeof(VUSBSETUP), RTFILE_O_WRITE, pUrbStatus);

    /* Handle write request */
    return usbProxySolarisIOAsync(File, (caddr_t)pReq, sizeof(VUSBSETUP) + cbData, RTFILE_O_WRITE, pUrbStatus);
}


/**
 * Helper for sending control messages, Synchronous version.
 *
 * @returns Number of bytes transferred or -1 for errors.
 * @param   pStatus     Where to store the VUSB status (Optional).
 */
static int usbProxySolarisCtrlReq(RTFILE File, RTFILE StatFile, PVUSBSETUP pReq, caddr_t pData, uint16_t cbData, int *pStatus)
{
    int cbIO = usbProxySolarisSendReq(File, StatFile, pReq, pData, cbData, pStatus);
    if (cbIO < USBSOL_CTRLREQ_SIZE)
        return cbIO;

    /* Handle read requests, synchronous. */
    cbIO -= USBSOL_CTRLREQ_SIZE;
    if (  (pReq->bmRequestType & VUSB_DIR_TO_HOST)
        && cbData > 0)
    {
        cbIO = usbProxySolarisIO(File, StatFile, pData, cbData, RTFILE_O_READ, pStatus);
    }
    return cbIO;
}


/**
 * Helper for sending control messages, Asynchronous version.
 *
 * @returns VBox status code.
 * @param   pStatus     Where to store the VUSB status (Optional).
 */
static int usbProxySolarisCtrlReqAsync(RTFILE File, PVUSBSETUP pReq, caddr_t pData, uint16_t cbData, PURBSTATUSSOL pStatus)
{
    int cbIO = usbProxySolarisSendReq(File, NIL_RTFILE, pReq, pData, cbData, NULL);
    if (cbIO < USBSOL_CTRLREQ_SIZE)
        return VERR_BUFFER_OVERFLOW;

    /* Handle read requests, asynchronous. */
    int rc = VINF_SUCCESS;
    if (  (pReq->bmRequestType & VUSB_DIR_TO_HOST)
        && cbData > 0)
    {
        rc = usbProxySolarisIOAsync(File, pData, cbData, RTFILE_O_READ, pStatus);
    }
    return rc;
}


#ifdef VBOX_WITH_SOLARIS_USB_ISOC
/**
 * Helper for sending isochronous requests, Asynchronous version.
 *
 * @returns VBox status code.
 * @param   pStatus     Where to store the VUSB status (Optional).
 */
static int usbProxySolarisIsocReqAsync(RTFILE File, PUSBPROXYURBSOL pUrbSol, PURBSTATUSSOL pStatus)
{
    PVUSBURB pUrb = pUrbSol->pVUsbUrb;
    if (pUrb->enmDir == VUSBDIRECTION_OUT)
        return usbProxySolarisIOAsync(File, pUrbSol->pIsocBufOut, pUrbSol->cbIsocBufOut, RTFILE_O_WRITE, pStatus);

    /* Handle read request, write request info synchronous, read asynchronous */
    int cbIO = usbProxySolarisIO(File, NIL_RTFILE, pUrbSol->pIsocBufOut, pUrbSol->cbIsocBufOut, RTFILE_O_WRITE, NULL);

    /* Free OUT buffer for the read request. */
    RTMemFree(pUrbSol->pIsocBufOut);
    pUrbSol->pIsocBufOut = NULL;
    pUrbSol->cbIsocBufOut = 0;

    if (cbIO < 0)
        return VERR_GENERAL_FAILURE;    /* @todo find proper error code here */

    return usbProxySolarisIOAsync(File, pUrbSol->pIsocBufIn, pUrbSol->cbIsocBufIn, RTFILE_O_READ, pStatus);
}
#endif  /* VBOX_WITH_SOLARIS_USB_ISOC */


/**
 * Basic USB IO function for transferring data to/from endpoint nodes, Synchronous version.
 *
 * @returns Number of bytes transferred or -1 on errors.
 * @param   File        The endpoint node
 * @param   StatFile    The associated status endpoint node (Optional, can be NIL_RTFILE).
 * @param   pData       Pre-allocated buffer to read/write into.
 * @param   cbData      Number of bytes to transfer (should be size of buffer).
 * @param   fOpenMode   Open mode for the endpoint determining a Read/Write operation.
 * @param   pError      Where to store VUSB result of operation (Optional, can be NULL).
 */
static int usbProxySolarisIO(RTFILE File, RTFILE StatFile, caddr_t pData, size_t cbData, int fOpenMode, int *pStatus)
{
    AssertReturn(cbData > 0, VERR_IO_BAD_LENGTH);
    int rc;
    size_t cbIO;

    if (fOpenMode == RTFILE_O_READ)
        rc = RTFileRead(File, pData, cbData, &cbIO);
    else
        rc = RTFileWrite(File, pData, cbData, &cbIO);

    if (   pStatus
        && StatFile != NIL_RTFILE)
    {
        int Status;
        usbProxySolarisGetStatus(StatFile, &Status, false);
        *pStatus = vusbProxySolarisStatusToVUsbStatus(Status);
    }
    return cbIO;
}


/**
 * Basic USB IO function for transferring data to/from endpoint nodes, Asynchronous version.
 *
 * @returns VBox status code.
 * @param   File        The endpoint node
 * @param   pData       Pre-allocated buffer to read/write into.
 * @param   cbData      Number of bytes to transfer (should be size of buffer).
 * @param   fMode       RT IO mode for the endpoint (RTFILE_O_READ/RTFILE_O_WRITE).
 * @param   pUrbStatus  Where to store the result (allocated by caller).
 */
static int usbProxySolarisIOAsync(RTFILE File, caddr_t pData, size_t cbData, int fMode, PURBSTATUSSOL pUrbStatus)
{
    AssertReturn(cbData > 0, VERR_IO_BAD_LENGTH);
    int rc;

    /* Turn on asynchronous notifications. */
    usbProxySolarisAsyncNotify(true);

    if (fMode == RTFILE_O_READ)
        rc = aioread((int)File, pData, cbData, 0, SEEK_CUR, (aio_result_t *)pUrbStatus);
    else
        rc = aiowrite((int)File, pData, cbData, 0, SEEK_CUR, (aio_result_t *)pUrbStatus);

    if (!rc)
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(rc);
}


/**
 * Initializes all pipes (including the default pipe) to their default state.
 *
 * @param   pDevSol     The Solaris USB device.
 */
static void usbProxySolarisInitAllPipes(PUSBPROXYDEVSOL pDevSol)
{
    for (int i = 0; i < USBSOL_MAXENDPOINTS; i++)
    {
        pDevSol->aEpFile[i] = NIL_RTFILE;
        pDevSol->aEpStatFile[i] = NIL_RTFILE;
    }
}


/**
 * Opens the default device pipe and it's corresponding status node.
 * Also opens the device status node in non-blocking mode.
 *
 * @returns VBox status code.
 * @param   pDevSol     The Solaris USB device.
 */
static int usbProxySolarisOpenDevPipes(PUSBPROXYDEVSOL pDevSol)
{
    LogFlow((USBR3PROXY ":usbProxySolarisOpenDevPipes pDevSol=%p\n", pDevSol));

    AssertReturn(pDevSol->pszDevPath, VERR_INVALID_NAME);

    char achBuf[PATH_MAX];
    RTStrPrintf(achBuf, sizeof(achBuf), "%s/devstat", pDevSol->pszDevPath);

    /*
     * Open status endpoint as read-only.
     */
    int rc = RTFileOpen(&pDevSol->StatFile, achBuf, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_NON_BLOCK);
    if (RT_FAILURE(rc))
    {
        LogRel((USBR3PROXY ":failed to open status end-point. achBuf=%s rc=%Rrc\n", achBuf, rc));
        return rc;
    }

    /*
     * Open default control endpoint as read-write.
     */
    RTStrPrintf(achBuf, sizeof(achBuf), "%s/cntrl0", pDevSol->pszDevPath);
    rc = RTFileOpen(&pDevSol->aEpFile[0], achBuf, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        LogRel((USBR3PROXY ":failed to open control end-point achBuf=%s. rc=%Rrc\n", achBuf, rc));

    /*
     * Open default control status endpoint as read-only.
     */
    RTStrPrintf(achBuf, sizeof(achBuf), "%s/cntrl0stat", pDevSol->pszDevPath);
    rc = RTFileOpen(&pDevSol->aEpStatFile[0], achBuf, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        LogRel((USBR3PROXY ":failed to open control status end-point achBuf=%s. rc=%Rrc\n", achBuf, rc));

    return rc;
}


/**
 * Closes the default pipe, status node and device status node.
 *
 * @param   pDevSol     The Solaris USB device.
 */
static void usbProxySolarisCloseDevPipes(PUSBPROXYDEVSOL pDevSol)
{
    LogFlow((USBR3PROXY ":usbProxySolarisCloseDevPipes pDevSol=%p\n", pDevSol));

    int rc;
#define CLOSEFILE_AND_RESET(servicefile) \
    do { \
        if (servicefile != NIL_RTFILE) \
        { \
            rc = RTFileClose(servicefile); \
            AssertRC(rc); \
            servicefile = NIL_RTFILE; \
        } \
    } while(0)

    CLOSEFILE_AND_RESET(pDevSol->StatFile);
    CLOSEFILE_AND_RESET(pDevSol->aEpFile[0]);
    CLOSEFILE_AND_RESET(pDevSol->aEpStatFile[0]);
}


/**
 * Close all endpoints and reset endpoint descriptor arrays.
 *
 * @param   pDevSol     The Solaris USB device.
 */
static void usbProxySolarisCloseEndPoints(PUSBPROXYDEVSOL pDevSol)
{
    /* Close endpoint and endpoint status descriptors. */
    for (int i = 1; i < USBSOL_MAXENDPOINTS; i++)
    {
        if (pDevSol->aEpFile[i] != NIL_RTFILE)
        {
            RTFileClose(pDevSol->aEpFile[i]);
            pDevSol->aEpFile[i] = NIL_RTFILE;
        }

        if (pDevSol->aEpStatFile[i] != NIL_RTFILE)
        {
            RTFileClose(pDevSol->aEpStatFile[i]);
            pDevSol->aEpStatFile[i] = NIL_RTFILE;
        }
    }
}


/**
 * Helper/Wrapper for closing all the device nodes and control pipes.
 *
 * @param   pDevSol     The Solaris USB device.
 */
static void usbProxySolarisCloseDevice(PUSBPROXYDEVSOL pDevSol)
{
    usbProxySolarisCloseEndPoints(pDevSol);
    usbProxySolarisCloseDevPipes(pDevSol);
}


/**
 * Get status information from a status node.
 *
 * @param   StatFile        The opened status node,
 * @param   pStatus         Where to store the status (optional).
 * @param   fDevStat        Whether the passed StatFile is the device status node.
 */
static int usbProxySolarisGetStatus(RTFILE StatFile, int *pStatus, bool fDevStat)
{
    LogFlow((USBR3PROXY ":usbProxySolarisGetStatus pStatus=%p fDevStat=%d\n", pStatus, fDevStat));

    int Status;
    size_t cbRead;
    int rc = RTFileRead(StatFile, &Status, sizeof(Status), &cbRead);
    if (cbRead != sizeof(Status))
    {
        rc = VERR_DEV_IO_ERROR;
        LogRel((USBR3PROXY "usbProxySolarisGetStatus: failed to read. rc=%Rrc\n", rc));
    }
    else
    {
        /*
         * Ugen supplied error codes.
         */
        rc = VINF_SUCCESS;
        if (!fDevStat)
        {
            switch (Status)
            {
                case USB_LC_STAT_NOERROR:             Log((USBR3PROXY    ":usbProxySolarisGetStatus: No Error\n"));                           break;
                case USB_LC_STAT_CRC:                 LogRel((USBR3PROXY ":usbProxySolarisGetStatus: CRC Timeout Detected\n"));               break;
                case USB_LC_STAT_BITSTUFFING:         LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Bit Stuffing Violation\n"));             break;
                case USB_LC_STAT_DATA_TOGGLE_MM:      LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Data Toggle Mismatch\n"));               break;
                case USB_LC_STAT_STALL:               LogRel((USBR3PROXY ":usbProxySolarisGetStatus: End Point Stalled\n"));                  break;
                case USB_LC_STAT_DEV_NOT_RESP:        LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Device Not Responding\n"));              break;
                case USB_LC_STAT_PID_CHECKFAILURE:    LogRel((USBR3PROXY ":usbProxySolarisGetStatus: PID Check Failure\n"));                  break;
                case USB_LC_STAT_UNEXP_PID:           LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Unexpected PID\n"));                     break;
                case USB_LC_STAT_DATA_OVERRUN:        LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Data Exceeded Size\n"));                 break;
                case USB_LC_STAT_DATA_UNDERRUN:       LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Less Data Received\n"));                 break;
                case USB_LC_STAT_BUFFER_OVERRUN:      LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Buffer Size Exceeded\n"));               break;
                case USB_LC_STAT_BUFFER_UNDERRUN:     LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Buffer Underrun\n"));                    break;
                case USB_LC_STAT_TIMEOUT:             LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Command has timed out\n"));              break;
                case USB_LC_STAT_NOT_ACCESSED:        LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Not Accessed by Hardware\n"));           break;
                case USB_LC_STAT_UNSPECIFIED_ERR:     LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Unspecified Error\n"));                  break;
                case USB_LC_STAT_NO_BANDWIDTH:        LogRel((USBR3PROXY ":usbProxySolarisGetStatus: No Bandwidth\n"));                       break;
                case USB_LC_STAT_HW_ERR:              LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Host Controller Hardware Error\n"));     break;
                case USB_LC_STAT_SUSPENDED:           LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Device Suspended\n"));                   break;
                case USB_LC_STAT_DISCONNECTED:        LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Device Disconnected\n"));                break;
                case USB_LC_STAT_INTR_BUF_FULL:       LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Interrupt Buffer full\n"));              break;
                case USB_LC_STAT_INVALID_REQ:         LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Request Invalid\n"));                    break;
                case USB_LC_STAT_INTERRUPTED:         LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Request Interrupted\n"));                break;
                case USB_LC_STAT_NO_RESOURCES:        LogRel((USBR3PROXY ":usbProxySolarisGetStatus: No Resources Available for Request\n")); break;
                case USB_LC_STAT_INTR_POLLING_FAILED: LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Failed to Restart Poll"));               break;
                default:                              LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Error Not Determined %d\n", Status));    break;
            }
        }
        else
        {
            switch (Status)
            {
                case USB_DEV_STAT_ONLINE:             LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Device Online\n"));                      break;
                case USB_DEV_STAT_DISCONNECTED:       LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Device Disconnected\n"));                break;
                case USB_DEV_STAT_RESUMED:            LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Device Resumed\n"));                     break;
                case USB_DEV_STAT_UNAVAILABLE:        LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Device Status Unavailable\n"));          break;
                default:                              LogRel((USBR3PROXY ":usbProxySolarisGetStatus: Unknown error %d!\n", Status));          break;
            }
        }
    }
    if (pStatus)
        *pStatus = Status;
    return rc;
}


/**
 * Get currently active configuration.
 *
 * @param   pDevSol         The Solaris USB Device.
 * @param   pConfigValue    Where to store the configuration value.
 * @returns VBox status code.
 */
static int usbProxySolarisGetActiveCfg(PUSBPROXYDEVSOL pDevSol, int *pConfigValue)
{
    LogFlow((USBR3PROXY ":usbProxySolarisGetActiveCfg pDevSol=%p pConfigValue=%p\n", pDevSol, pConfigValue));

    AssertReturn(pConfigValue, VERR_INVALID_POINTER);

    /* USB 2.0(Sec. 9.4.2): bRequest=GET_CONFIGURATION(0x08) wValue=0 wIndex=0 wLength=1 */
    VUSBSETUP SetupData;
    usbProxySolarisSetupReq(&SetupData, VUSB_DIR_TO_HOST, VUSB_REQ_GET_CONFIGURATION, 0, 0, 1);

    int iConfig = -1;
    int rc = RTFileWrite(pDevSol->aEpFile[0], &SetupData, sizeof(SetupData), NULL);
    if (RT_SUCCESS(rc))
    {
        size_t cbRead;
        rc = RTFileRead(pDevSol->aEpFile[0], &iConfig, sizeof(iConfig), &cbRead);
        if (   cbRead == sizeof(iConfig)
            && RT_SUCCESS(rc))
        {
            LogRel((USBR3PROXY ":usbProxySolarisGetActiveCfg success. cfg=%d\n", iConfig));
            *pConfigValue = iConfig;
            return rc;
        }
        rc = VERR_DEV_IO_ERROR;
        LogRel((USBR3PROXY ":usbProxySolarisGetActiveCfg failed to read configuration. cbRead=%d rc=%Rrc iConfig=%d\n", cbRead, rc, iConfig));
    }
    else
        LogRel((USBR3PROXY ":usbProxySolarisGetActiveCfg failed to write configuration request!\n"));
    return rc;
}


/**
 * Finds the interface and alternate setting for a given Endpoint address.
 *
 * @returns VBox status code.
 * @param   pDevSol     The Solaris USB device.
 * @param   EndPoint    The endpoint address as seen in the descriptor.
 * @param   piInterface Where to store the interface number.
 * @param   piAlternate Where to store the alternate number.
 */
static int usbProxySolarisGetInterfaceForEndpoint(PUSBPROXYDEVSOL pDevSol, int EndPoint, int *piInterface, int *piAlternate)
{
    LogFlow((USBR3PROXY ":usbProxySolarisGetInterfaceForEndpoint pDevSol=%p Endpoint=%#x\n", pDevSol, EndPoint));

    /* Find the interface for this endpoint. */
    PUSBPROXYDEV pProxyDev = pDevSol->pProxyDev;
    PCPDMUSBDESCCACHE pDescCache = &pProxyDev->DescCache;
    AssertReturn(pDescCache, VERR_INVALID_HANDLE);

    /* Find the configuration index for this configuration value. */
    int iCfg = usbProxySolarisConfigIndex(pDevSol, pProxyDev->iActiveCfg >= 1 ? pProxyDev->iActiveCfg : 1);
    if (iCfg < 0)
    {
        LogRel((USBR3PROXY ":usbProxySolarisGetInterfaceForEndpoint failed to obtain configuration index\n"));
        return VERR_GENERAL_FAILURE;
    }

    PCVUSBDESCCONFIGEX pCfg = &pProxyDev->paCfgDescs[iCfg];
    AssertReturn(pCfg, VERR_INVALID_POINTER);

    /*
     * Loop through all interfaces for this configuration.
     */
    for (int i = 0; i < pCfg->Core.bNumInterfaces; i++)
    {
        PCVUSBINTERFACE pInterface = &pCfg->paIfs[i];
        AssertReturn(pInterface, VERR_INVALID_POINTER);

        /*
         * Loop through all alternate settings of this interface.
         */
        for (uint32_t k = 0; k < pInterface->cSettings; k++)
        {
            PCVUSBDESCINTERFACEEX pInterfaceEx = &pInterface->setting[k];
            AssertReturn(pInterfaceEx, VERR_INVALID_POINTER);

            /*
             * Loop through all the endpoints of this interface.
             */
            for (unsigned j = 0; j < pInterfaceEx->Core.bNumEndpoints; j++)
            {
                PCVUSBDESCENDPOINTEX pEndPoint = &pInterfaceEx->paEndpoints[j];
                AssertReturn(pEndPoint, VERR_INVALID_POINTER);

                if (pEndPoint->Core.bEndpointAddress == EndPoint)
                {
                    if (piInterface)
                        *piInterface = pInterfaceEx->Core.bInterfaceNumber;
                    if (piAlternate)
                        *piAlternate = pInterfaceEx->Core.bAlternateSetting;
                    return VINF_SUCCESS;
                }
            }
        }
    }
    LogRel((USBR3PROXY ":usbProxySolarisGetInterfaceForEndpoint failed for EndPt=%#x\n", EndPoint));
    return VERR_GENERAL_FAILURE;
}


/**
 * Finds configuration index for the configuration value.
 *
 * @returns Index of configuration with value 'bCfgValue' or -1 if not found.
 */
static int usbProxySolarisConfigIndex(PUSBPROXYDEVSOL pDevSol, uint8_t bCfgValue)
{
    LogFlow((USBR3PROXY ":usbProxySolarisConfigIndex pDevSol=%p bCfgValue=%d\n", bCfgValue));

    /* Find the configuration index corresponding to this configuration value. */
    PUSBPROXYDEV pProxyDev = pDevSol->pProxyDev;
    PCPDMUSBDESCCACHE pDescCache = &pProxyDev->DescCache;
    AssertReturn(pDescCache, VERR_INVALID_HANDLE);

    int cCfg = pDescCache->pDevice->bNumConfigurations;
    for (int i = 0; i < cCfg; i++)
    {
        PCVUSBDESCCONFIGEX pCfg = &pProxyDev->paCfgDescs[i];
        AssertReturn(pCfg, -1);
        if (pCfg->Core.bConfigurationValue == bCfgValue)
        {
            LogFlow((USBR3PROXY ":usbProxySolarisConfigIndex found bCfgValue=%d at index %d\n", bCfgValue, i));
            return i;
        }
    }
    LogRel((USBR3PROXY ":usbProxySolarisConfigIndex bCfgValue=%d not found!\n", bCfgValue));
    return -1;
}


/**
 * Claims all interfaces for a given configuration.
 *
 * @returns VBox status code.
 * @param   pDevSol     The Solaris USB device.
 * @param   bCfgValue   The configuration value.
 */
static int usbProxySolarisSeizeAllInterfaces(PUSBPROXYDEVSOL pDevSol, uint8_t bCfgValue)
{
    LogFlow((USBR3PROXY ":usbProxySolarisSeizeAllInterfaces pDevSol=%p bCfgValue=%d\n", pDevSol, bCfgValue));

    AssertReturn(pDevSol, VERR_INVALID_POINTER);

    /*
     * ugen doesn't have a seize all interface scheme, but we can just open all the end-points of the interface
     * in exclusive mode to have a similar effect. This should mostly be called from Init.
     */
    PCPDMUSBDESCCACHE pDescCache = &pDevSol->pProxyDev->DescCache;
    AssertReturn(pDescCache, VERR_INVALID_HANDLE);

    int rc = VERR_GENERAL_FAILURE;
    int cCfg = pDescCache->pDevice->bNumConfigurations;

    /* Find the configuration index for this configuration value. */
    int iCfg = usbProxySolarisConfigIndex(pDevSol, bCfgValue);
    if (iCfg < 0)
    {
        LogRel((USBR3PROXY ":usbProxySolarisSeizeAllInterfaces failed to find configuration index for configuration value %d\n", bCfgValue));
        return VERR_GENERAL_FAILURE;    /* @todo find a better code? */
    }

    if (iCfg < cCfg)
    {
        PCVUSBDESCCONFIGEX pCfg = &pDevSol->pProxyDev->paCfgDescs[iCfg];

        /*
         * Loop through all interfaces for this configuration.
         */
        for (int iInterface = 0; iInterface < pCfg->Core.bNumInterfaces; iInterface++)
        {
            PCVUSBINTERFACE pInterface = &pCfg->paIfs[iInterface];
            AssertReturn(pInterface, VERR_INVALID_POINTER);

            /*
             * Loop through all alternate settings of this interface.
             */
            for (uint32_t k = 0; k < pInterface->num_settings; k++)
            {
                PCVUSBDESCINTERFACEEX pInterfaceEx = &pInterface->setting[k];
                AssertReturn(pInterfaceEx, VERR_INVALID_POINTER);

                int iAlternate = pInterfaceEx->Core.bAlternateSetting;

                /*
                 * Loop through all endpoints for this interface.
                 */
                for (unsigned j = 0; j < pInterfaceEx->Core.bNumEndpoints; j++)
                {
                    PCVUSBDESCENDPOINTEX pEndPoint = &pInterfaceEx->paEndpoints[j];
                    AssertReturn(pEndPoint, VERR_INVALID_POINTER);

                    rc = usbProxySolarisOpenNode(pDevSol,
                                                 bCfgValue,                                              /* Configuration value */
                                                 iInterface,                                             /* Interface number */
                                                 iAlternate,                                             /* Alternate setting */
                                                 pEndPoint->Core.bEndpointAddress,                       /* Endpoint Address */
                                                 usbProxySolarisXferType(pEndPoint->Core.bmAttributes)); /* Xfer Type */
                    if (RT_FAILURE(rc))
                    {
                        LogRel((USBR3PROXY ":usbProxySolarisSeizeAllInterfaces failed to open Cfg%d If%d Alt%d Ep%d(Addr %#x)\n", bCfgValue,
                                iInterface, iAlternate, k, pEndPoint->Core.bEndpointAddress));
                        break;
                    }
                }
            }
            if (RT_FAILURE(rc))
                break;
        }
    }
    else
        LogRel((USBR3PROXY ":usbProxySolarisSeizeAllInterfaces failed! iCfg %d > cCfg %d\n", iCfg, cCfg));

    LogFlow((USBR3PROXY ":usbProxySolarisSeizeAllInterfaces returned %d\n", rc));
    return rc;
}


/**
 * Determine the XferType from the endpoint attribute.
 *
 * @returns XferType for the given endpoint 'bmAttribute'
 */
static VUSBXFERTYPE usbProxySolarisXferType(uint8_t bmAttribute)
{
    LogFlow((USBR3PROXY ":usbProxySolarisXferType bmAttribute=%#x\n", bmAttribute));

    /*
     * USB 2.0(Sec. 9.6.6): bmAttribute transfer type.
     */
    return (VUSBXFERTYPE)(bmAttribute & 0x03);
}


/**
 * Determines the ugen node's open mode based on the XferType and direction.
 *
 * @returns     The file open mode, or UINT32_MAX upon errors.
 * @param       bmEndpointAddress       Address of endpoint as seen in the descriptor.
 * @param       XferType                The VUSB XferType.
 */
DECLINLINE(uint32_t) usbProxySolarisOpenMode(uint8_t bmEndpointAddress, VUSBXFERTYPE XferType)
{
    LogFlow((USBR3PROXY ":usbProxySolarisOpenMode bmEndpointAddress=%#x XferType=%d\n", bmEndpointAddress, XferType));

    uint32_t fMode = RTFILE_O_OPEN | RTFILE_O_DENY_NONE;
    int fIn = bmEndpointAddress & USBSOL_EPADDR_IN;
    switch (XferType)
    {
        case VUSBXFERTYPE_INTR: fMode |= (fIn ? RTFILE_O_READ : RTFILE_O_WRITE | RTFILE_O_NON_BLOCK);    break;
        case VUSBXFERTYPE_BULK: fMode |= (fIn ? RTFILE_O_READ : RTFILE_O_WRITE);                         break;
        case VUSBXFERTYPE_ISOC: fMode |= RTFILE_O_READWRITE | RTFILE_O_NON_BLOCK;                        break;
        case VUSBXFERTYPE_CTRL: fMode |= RTFILE_O_READWRITE;                                             break;
        case VUSBXFERTYPE_MSG : fMode |= RTFILE_O_READWRITE | RTFILE_O_NON_BLOCK;                        break;
        case VUSBXFERTYPE_INVALID: fMode = UINT32_MAX; LogRel((USBR3PROXY ":invalid XferType %d\n", XferType)); break;
    }
    return fMode;
}


/**
 * Open and endpoint given the XferType and optional OpenMode.
 *
 * @returns VBox status code.
 * @param   pDevSol     The Solaris USB device.
 * @param   EndPoint    The endpoint address as in the descriptor.
 * @param   XferType    Endpoint xfer type.
 */
static int usbProxySolarisOpenEndPoint(PUSBPROXYDEVSOL pDevSol, int EndPoint, VUSBXFERTYPE XferType)
{
    LogFlow((USBR3PROXY ":usbProxySolarisOpenEndPoint pDevSol=%p EndPoint=%#x XferType=%d\n", EndPoint, XferType));

    /* Endpoint zero (the default control pipe) would already be open. */
    if (EndPoint == 0)
        return VINF_SUCCESS;

    PUSBPROXYDEV pProxyDev = pDevSol->pProxyDev;

    /* @todo Fix This! I'm not yet sure what's the right thing to do here regarding configuration... */
    /* Get current configuration index. */
    int bCfgValue = pProxyDev->iActiveCfg;
    if (bCfgValue == -1)
    {
        /* @todo Fix or remove this. GET_CONFIGURATION requests don't work... */
        int rc = usbProxySolarisGetActiveCfg(pDevSol, &bCfgValue);
        if (RT_FAILURE(rc))
            bCfgValue = 1;
    }

    /* Get interface for endpoint. */
    int iInterface;
    int iAlternate;
    int rc = usbProxySolarisGetInterfaceForEndpoint(pDevSol, EndPoint, &iInterface, &iAlternate);
    if (RT_SUCCESS(rc))
        return usbProxySolarisOpenNode(pDevSol, bCfgValue, iInterface, iAlternate, EndPoint, XferType);
    else
        LogRel((USBR3PROXY ":usbProxySolarisOpenEndPoint usbProxySolarisGetInterfaceForEndpoint failed. rc=%Rrc\n", rc));

    return VERR_GENERAL_FAILURE;
}


/**
 * Open an endpoint node and stores it among the list of open endpoints.
 *
 * @returns VBox status code.
 * @param   pDevSol     The Solaris USB device.
 * @param   bCfgValue   The configuration value.
 * @param   iInterface  The interface number.
 * @param   iAlternate  The alternate setting.
 * @param   EndPoint    The endpoint address as stored in the descriptor.
 * @param   XferType    Endpoint xfer type.
 * @param   fOpenMode   Open mode for the endpoint (< 0 auto-detects mode based on xfer type and direction).
 */
static int usbProxySolarisOpenNode(PUSBPROXYDEVSOL pDevSol, uint8_t bCfgValue, int iInterface, int iAlternate, int EndPoint,
                                   VUSBXFERTYPE XferType)
{
    LogFlow((USBR3PROXY ":usbProxySolarisOpenNode pDevSol=%p bCfgValue=%d iInterface=%d iAlternate=%d EndPoint=%#x XferType=%d\n",
            pDevSol, bCfgValue, iInterface, iAlternate, EndPoint, XferType));

    int EndPtIndex = (EndPoint & USBSOL_EPADDR_NUM_MASK) + ((EndPoint & USBSOL_EPADDR_DIR_MASK) ? USBSOL_MAXENDPOINTS / 2 : 0);
    if (EndPtIndex < 0 ||  EndPtIndex > USBSOL_MAXENDPOINTS - 1)
        return VERR_INVALID_HANDLE;

    /* Check if already open; ep0 should already be open as well. */
    if (   EndPtIndex == 0
        || pDevSol->aEpFile[EndPtIndex] != NIL_RTFILE)
    {
        return VINF_SUCCESS;
    }

    /*
     * Construct the node name and open the endpoint.
     */
    char szEndPoint[PATH_MAX + 1];
    char szConfig[12];
    char szAlternate[12];

    memset(szConfig, 0, sizeof(szConfig));
    memset(szAlternate, 0, sizeof(szAlternate));

    int iCfg = usbProxySolarisConfigIndex(pDevSol, bCfgValue);
    if (iCfg > 0)
        RTStrPrintf(szConfig, sizeof(szConfig), "cfg%d", bCfgValue);

    if (iAlternate > 0)
        RTStrPrintf(szAlternate, sizeof(szAlternate), ".%d", iAlternate);

    RTStrPrintf(szEndPoint, sizeof(szEndPoint), "%s/%sif%d%s%s%d", pDevSol->pszDevPath, szConfig, iInterface,
            szAlternate, (EndPoint & USBSOL_EPADDR_DIR_MASK) ? "in" : "out", (EndPoint & USBSOL_EPADDR_NUM_MASK));

    /*
     * Ugen requires Interrupt IN endpoints to be polled before opening.
     */
    char szEndPointStat[PATH_MAX + 1];
    RTStrPrintf(szEndPointStat, sizeof(szEndPointStat), "%sstat", szEndPoint);

    /*
     * Open the endpoint status node.
     */
    if (   XferType == VUSBXFERTYPE_INTR
        && (EndPoint & USBSOL_EPADDR_IN))
    {
        int rc = RTFileOpen(&pDevSol->aEpStatFile[EndPtIndex], szEndPointStat, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (RT_FAILURE(rc))
        {
            LogRel((USBR3PROXY ":usbProxySolarisOpenNode failed to poll Intr IN endpoint stat %s\n", szEndPointStat));
            pDevSol->aEpStatFile[EndPtIndex] = NIL_RTFILE;
            return rc;
        }

        /*
         * Unbuffered Interrupt IN transfers requires us to open the status endpoint and transfer
         * USB_EP_INTR_ONE_XFER once. This will put it in unbuffered transfer mode until it's closed.
         */
        char cMsg = USB_EP_INTR_ONE_XFER;
        rc = RTFileWrite(pDevSol->aEpStatFile[EndPtIndex], &cMsg, sizeof(cMsg), NULL);
        if (RT_FAILURE(rc))
        {
            LogRel((USBR3PROXY ":usbProxySolarisOpenNode failed to poll Intr IN endpoint %s\n", szEndPointStat));
            RTFileClose(pDevSol->aEpStatFile[EndPtIndex]);
            pDevSol->aEpStatFile[EndPtIndex] = NIL_RTFILE;
            return rc;
        }
    }
    else
    {
        int rc = RTFileOpen(&pDevSol->aEpStatFile[EndPtIndex], szEndPointStat, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (RT_FAILURE(rc))
        {
            LogRel((USBR3PROXY ":usbProxySolarisOpenNode failed to open endpoint stat %s in read only.\n", szEndPointStat));
            return rc;
        }
    }

    uint32_t fOpenMode = usbProxySolarisOpenMode(EndPoint, XferType);
    if (fOpenMode == UINT32_MAX)
    {
        LogRel((USBR3PROXY ":usbProxySolarisOpenNode invalid file mode for XferType %d\n", XferType));
        return VERR_INVALID_FMODE;
    }

    /*
     * Open the Endpoint.
     */
    int rc = RTFileOpen(&pDevSol->aEpFile[EndPtIndex], szEndPoint, fOpenMode);
    if (RT_FAILURE(rc))
    {
        LogRel((USBR3PROXY ":usbProxySolarisOpenNode failed to open endpoint %s in mode %d\n", szEndPoint, fOpenMode));
        RTFileClose(pDevSol->aEpStatFile[EndPtIndex]);
        pDevSol->aEpStatFile[EndPtIndex] = NIL_RTFILE;
    }
    else
        Log((USBR3PROXY ":usbProxySolarisOpenNode Success! szEndPoint=%s Endpoint=%#x EndPtIndex=%#x\n", szEndPoint, EndPoint, EndPtIndex));
    return rc;
}


/**
 * Completion callback worker.
 *
 * @param   pTimeout    Pointer to a timeval struct specifying the timeout.
 */
static void usbProxySolarisUrbComplete(struct timeval *pTimeout)
{
    /*
     * Deque the completed URB.
     */
    aio_result_t *pResult = aiowait(pTimeout);
    if (!pResult)                           /* Timeout */
    {
        Log(("usbProxySolarisUrbComplete: timed out\n"));
        return;
    }
    else if ((intptr_t)pResult == -1)       /* aiowait returns -1 instead of a pointer for errors. crap. */
    {
        if (errno == EINVAL)
            Log(("usbProxySolarisUrbComplete: No pending requests.\n"));
        else
            Log(("usbProxySolarisUrbComplete: aiowait failed errno=%d\n", errno));
        return;
    }

    /*
     * Update the URB status.
     */
    PURBSTATUSSOL   pStatus = (PURBSTATUSSOL)pResult;
    PUSBPROXYURBSOL pUrbSol = pStatus->pUrbSol;
    PUSBPROXYDEVSOL pDevSol = pUrbSol->pDevSol;
    PVUSBURB        pUrb    = pUrbSol->pVUsbUrb;

    RTCritSectEnter(&pDevSol->CritSect);

#ifdef VBOX_WITH_SOLARIS_USB_ISOC
    if (pUrb->enmType == VUSBXFERTYPE_ISOC)
    {
        if (pUrb->enmDir == VUSBDIRECTION_IN)
        {
            void *pvPayload = pUrbSol->pIsocBufIn + pUrb->cIsocPkts * sizeof(ugen_isoc_pkt_descr_t);
            memcpy(pUrb->abData, pvPayload, pUrb->cbData);

            RTMemFree(pUrbSol->pIsocBufIn);
            pUrbSol->pIsocBufIn = NULL;
            pUrbSol->cbIsocBufIn = 0;
        }
        else
        {
            RTMemFree(pUrbSol->pIsocBufOut);
            pUrbSol->pIsocBufOut = NULL;
            pUrbSol->cbIsocBufOut = 0;
        }
    }
    else
#endif  /* !VBOX_WITH_SOLARIS_USB_ISOC */
    {
        pUrb->cbData = pResult->aio_return;
        pUrb->enmStatus = pResult->aio_errno ? VUSBSTATUS_STALL : VUSBSTATUS_OK;    /* @todo find a better way */
        if (pUrb->enmType == VUSBXFERTYPE_MSG)
            pUrb->cbData += sizeof(VUSBSETUP);
    }

    /*
     * Remove from the active list.
     */
    if (pUrbSol->pNext)
        pUrbSol->pNext->pPrev = pUrbSol->pPrev;
    if (pUrbSol->pPrev)
        pUrbSol->pPrev->pNext = pUrbSol->pNext;
    else
    {
        Assert(pDevSol->pInFlightHead == pUrbSol);
        pDevSol->pInFlightHead = pUrbSol->pNext;
    }

    /*
     * Link it into the taxing list.
     */
    pUrbSol->pNext = NULL;
    pUrbSol->pPrev = pDevSol->pTaxingTail;
    if (pDevSol->pTaxingTail)
        pDevSol->pTaxingTail->pNext = pUrbSol;
    else
        pDevSol->pTaxingHead = pUrbSol;
    pDevSol->pTaxingTail = pUrbSol;

    RTCritSectLeave(&pDevSol->CritSect);

    Log(("%s: usbProxySolarisUrbComplete: cb=%d EndPt=%#x enmStatus=%s\n",
             pUrb->pszDesc, pUrb->cbData, pUrb->EndPt, pUrb->enmStatus == VUSBSTATUS_OK ? "VUSBSTATUS_OK" : "NotSuccessful"));
}


/**
 * Completion callback for asynchronous URBs.
 *
 */
static void usbProxySolarisAsyncComplete(int Sig)
{
    LogFlow((USBR3PROXY ":usbProxySolarisAsyncComplete Sig=%s %d\n", Sig == SIGIO ? "SIGIO" : "Irrelevant Sig", Sig));

    if (Sig == SIGIO)
        return usbProxySolarisUrbComplete(NULL);
}


/**
 * Opens the USB device.
 *
 * @returns VBox status code.
 * @param   pProxyDev       The device instance.
 * @param   pszAddress      The path to the device in the dev and devices tree,
 *                          The format of this string is "/dev/path|/devices/path".
 * @param   pvBackend       Backend specific pointer, unused for the solaris backend.
 */
static int usbProxySolarisOpen(PUSBPROXYDEV pProxyDev, const char *pszAddress, void *pvBackend)
{
    LogFlow((USBR3PROXY ":usbProxySolarisOpen pProxyDev=%p pszAddress=%s pvBackend=%p\n", pProxyDev, pszAddress, pvBackend));

    /*
     * Initialize our USB R3 lib.
     */
    int rc = USBLibInit();
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate and initialize the solaris backend data.
         */
        PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)RTMemAllocZ(sizeof(*pDevSol));
        if (pDevSol)
        {
            /*
             * Parse out 2 paths (dev and devices tree).
             */
            char szDevBuf[PATH_MAX];
            char szDevicesBuf[PATH_MAX];
            int cFields = sscanf(pszAddress, "%[^'|']|%s", szDevBuf, szDevicesBuf);
            if (cFields == 2)
            {
                szDevBuf[strlen(szDevBuf)] = 0;
                szDevicesBuf[strlen(szDevicesBuf)] = 0;
                RTStrAPrintf(&pDevSol->pszDevPath, "%s", szDevBuf);
                RTStrAPrintf(&pDevSol->pszDevicePath, "%s", szDevicesBuf);
                rc = RTCritSectInit(&pDevSol->CritSect);
                if (RT_SUCCESS(rc))
                {
                    pProxyDev->Backend.pv = pDevSol;

                    /*
                     * Initialize all pipes.
                     */
                    usbProxySolarisInitAllPipes(pDevSol);

                    /*
                     * Open the default control and control status endpoint.
                     */
                    rc = usbProxySolarisOpenDevPipes(pDevSol);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * @todo Fix this: Try to get the active configuration.
                         */
                        //int iActiveCfg = -1;
                        //rc = usbProxySolarisGetActiveCfg(pDevSol, &iActiveCfg);
                        if (RT_SUCCESS(rc))
                        {
                            //pProxyDev->iActiveCfg = usbProxySolarisConfigIndex(pDevSol, iActiveCfg) > 0 ? iActiveCfg : -1;
                            pProxyDev->iActiveCfg = -1;
                            pProxyDev->cIgnoreSetConfigs = 1;
                        }
                        else
                            LogRel((USBR3PROXY ":usbProxySolarisGetActiveCfg failed! rc=%Rrc\n", rc));

                        pDevSol->pProxyDev = pProxyDev;
                        return VINF_SUCCESS;
                    }
                    else
                        LogRel((USBR3PROXY ":usbProxySolarisDevPipes failed. rc=%Rrc\n", rc));
                    usbProxySolarisCloseDevPipes(pDevSol);   /* Close here to handle partial open failures. */
                    RTCritSectDelete(&pDevSol->CritSect);
                }
                else
                    LogRel((USBR3PROXY ":RTCritSectInit failed. rc=%Rrc\n", rc));
                RTStrFree(pDevSol->pszDevPath);
                RTStrFree(pDevSol->pszDevicePath);
            }
            else
            {
                rc = VERR_GENERAL_FAILURE;
                LogRel((USBR3PROXY ":Failed to parse address of USB device pszAddress=%s\n", pszAddress));
            }
            RTMemFree(pDevSol);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        LogRel((USBR3PROXY ":USBLibInit failed. rc=%Rrc\n", rc));

    USBLibTerm();
    pProxyDev->Backend.pv = NULL;
    return rc;
}


/**
 * Post-open initialization of the USB device.
 *
 * @returns VBox status code.
 * @param   pProxyDev     The device instance.
 */
static int usbProxySolarisInit(PUSBPROXYDEV pProxyDev)
{
    LogFlow((USBR3PROXY ":usbProxySolarisInit: pProxyDev=%p\n", pProxyDev));

    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;
    AssertReturn(pDevSol, VERR_INVALID_POINTER);

    /* Use the first configuration as the active configure. */
    PCVUSBDESCCONFIGEX pCfg = &pProxyDev->paCfgDescs[0];
    if (pCfg)
    {
        /* Now seize all the interfaces. */
        return usbProxySolarisSeizeAllInterfaces(pDevSol, pCfg->Core.bConfigurationValue);
    }

    LogRel((USBR3PROXY ":usbProxySolarisInit invalid device descriptors.\n"));
    return VERR_INVALID_HANDLE;
}


/**
 * Close the USB device.
 *
 * @param   pProxyDev   The device instance.
 */
static void usbProxySolarisClose(PUSBPROXYDEV pProxyDev)
{
    LogFlow((USBR3PROXY ":usbProxySolarisClose: pProxyDev=%p\n", pProxyDev));

    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;

    /* Close the device. */
    usbProxySolarisCloseDevice(pDevSol);

    /* Reset the device without reattaching a new driver.  */
    USBLibResetDevice(pDevSol->pszDevicePath, false);

    /*
     * Now we can close it and free all the resources.
     */
    RTCritSectDelete(&pDevSol->CritSect);

    PUSBPROXYURBSOL pUrbSol;
    while ((pUrbSol = pDevSol->pInFlightHead) != NULL)
    {
        pDevSol->pInFlightHead = pUrbSol->pNext;
        RTMemFree(pUrbSol);
    }

    while ((pUrbSol = pDevSol->pFreeHead) != NULL)
    {
        pDevSol->pFreeHead = pUrbSol->pNext;
        RTMemFree(pUrbSol);
    }

    usbProxySolarisAsyncNotify(false);

    RTStrFree(pDevSol->pszDevPath);
    pDevSol->pszDevPath = NULL;
    RTStrFree(pDevSol->pszDevicePath);
    pDevSol->pszDevicePath = NULL;
    RTMemFree(pDevSol);
    pProxyDev->Backend.pv = NULL;

    USBLibTerm();
}


/**
 * Reset the device.
 *
 * @returns VBox status code.
 * @param   pProxyDev    The device to reset.
 */
static int usbProxySolarisReset(PUSBPROXYDEV pProxyDev, bool fResetOnSolaris)
{
    LogFlow((USBR3PROXY ":usbProxySolarisReset pProxyDev=%s\n", pProxyDev->pUsbIns->pszName));

    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;

    /* Close all endpoints and endpoint status nodes. */
    usbProxySolarisCloseEndPoints(pDevSol);

    /* Stop notifications */
    usbProxySolarisAsyncNotify(false);

    /*
     * Specific device resets are implicitly handled by ugen upon closing/reopening the device.
     * Root hub resets that affects all devices are executed.
     */
    if (fResetOnSolaris)
    {
        /* Reset the device without reattaching a new driver */
        int rc = USBLibResetDevice(pDevSol->pszDevicePath, false);
        if (RT_SUCCESS(rc))
        {
            pProxyDev->cIgnoreSetConfigs = 0;
            pProxyDev->iActiveCfg = -1;
        }
        else
            LogRel(("usbProxySolarisReset: failed! rc=%Rrc\n", rc));
        return rc;
    }
    else
        return VINF_SUCCESS;
}


/**
 * Set the active configuration.
 *
 * The caller makes sure that it's not called first time after open or reset
 * with the active interface.
 *
 * @returns success indicator.
 * @param   pProxyDev       The device instance data.
 * @param   iCfg            The configuration value to set.
 */
static int usbProxySolarisSetConfig(PUSBPROXYDEV pProxyDev, int iCfg)
{
    LogFlow((USBR3PROXY ":usbProxySolarisSetConfig: pProxyDev=%p iCfg=%d\n", pProxyDev, iCfg));

    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;

    /*
     * Solaris ugen ignores any SET_CONFIGURATION requests. It does an implicitly config change while
     * opening the appropriate nodes of the configuration. So what we need to do is make sure all the
     * nodes of previous configuration is closed before re-opening/claiming the new configuration.
     */
    if (pProxyDev->iActiveCfg == iCfg)
        return true;

    usbProxySolarisCloseEndPoints(pDevSol);
    int rc = usbProxySolarisSeizeAllInterfaces(pDevSol, (uint8_t)iCfg);
    if (RT_SUCCESS(rc))
    {
        if (iCfg > 0)
            pProxyDev->iActiveCfg = iCfg;
        else
            LogRel((USBR3PROXY ":usbProxySolarisSetConfig invalid iCfg %d\n", iCfg));
        return true;
    }

    LogRel((USBR3PROXY ":usbProxySolarisSetConfig failed to seize all interfaces for iCfg %d\n", iCfg));
    return false;
}


/**
 * Claims an interface.
 *
 * This is a stub on Solaris since we release/claim all interfaces at
 * open/reset/setconfig time.
 *
 * @returns success indicator (always true).
 */
static int usbProxySolarisClaimInterface(PUSBPROXYDEV pProxyDev, int iIf)
{
    return true;
}


/**
 * Releases an interface.
 *
 * This is a stub on Solaris since we release/claim all interfaces at
 * open/reset/setconfig time.
 *
 * @returns success indicator.
 */
static int usbProxySolarisReleaseInterface(PUSBPROXYDEV pProxyDev, int iIf)
{
    return true;
}


/**
 * Specify an alternate setting for the specified interface of the current configuration.
 *
 * @returns success indicator.
 */
static int usbProxySolarisSetInterface(PUSBPROXYDEV pProxyDev, int iIf, int iAlt)
{
    /*
     * We already open all alternate settings for all interfaces. Don't think we need to do
     * anything special here. As talking with the specified alternate setting would already be
     * possible as it's already open.
     */
    return true;
}


/**
 * Clears the halted endpoint 'EndPt'.
 */
static bool usbProxySolarisClearHaltedEp(PUSBPROXYDEV pProxyDev, unsigned int EndPt)
{
    LogFlow((USBR3PROXY ":usbProxySolarisClearHaltedEp pProxyDev=%p EndPt=%u", pProxyDev, EndPt));

    /*
     * Clearing the zero control pipe doesn't make sense and isn't
     * supported by the USBA. Just ignore it.
     */
    if (EndPt == 0)
        return true;

    int Status;
    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;

    VUSBSETUP Req;
    usbProxySolarisSetupReq(&Req, VUSB_DIR_TO_DEV | VUSB_TO_ENDPOINT, VUSB_REQ_CLEAR_FEATURE, 0, EndPt, 0);

    usbProxySolarisCtrlReq(pDevSol->aEpFile[0], pDevSol->aEpStatFile[0], &Req, NULL, 0, &Status);
    if (Status == VUSBSTATUS_OK)
        return true;

    LogRel((USBR3PROXY ":usbProxySolarisClearHaltedEp failed! EndPt=%u\n", EndPt));
    return false;
}


/**
 * Allocates a Solaris URB request structure.
 *
 * @returns Pointer to an active URB request.
 * @returns NULL on failure.
 *
 * @param   pDevSol         The solaris USB device.
 */
static PUSBPROXYURBSOL usbProxySolarisUrbAlloc(PUSBPROXYDEVSOL pDevSol)
{
    PUSBPROXYURBSOL pUrbSol;

    RTCritSectEnter(&pDevSol->CritSect);

    /*
     * Try remove a Solaris URB from the free list, if none there allocate a new one.
     */
    pUrbSol = pDevSol->pFreeHead;
    if (pUrbSol)
        pDevSol->pFreeHead = pUrbSol->pNext;
    else
    {
        RTCritSectLeave(&pDevSol->CritSect);
        pUrbSol = (PUSBPROXYURBSOL)RTMemAllocZ(sizeof(*pUrbSol));
        if (!pUrbSol)
            return NULL;

        pUrbSol->Status.pUrbSol = pUrbSol;

        RTCritSectEnter(&pDevSol->CritSect);
    }
    pUrbSol->pVUsbUrb = NULL;
    pUrbSol->pDevSol = pDevSol;

    /*
     * Link it into the active list
     */
    pUrbSol->pPrev = NULL;
    pUrbSol->pNext = pDevSol->pInFlightHead;
    if (pUrbSol->pNext)
        pUrbSol->pNext->pPrev = pUrbSol;
    pDevSol->pInFlightHead = pUrbSol;

    RTCritSectLeave(&pDevSol->CritSect);
    return pUrbSol;
}


#ifdef VBOX_WITH_SOLARIS_USB_ISOC
/**
 * Allocates an isochronous buffer.
 *
 * @returns VBox status code.
 * @param   pUrbSol     Pointer to the solaris URB associated with the transfer.
 */
static int usbProxySolarisUrbAllocIsocBuf(PUSBPROXYURBSOL pUrbSol)
{
    AssertReturn(pUrbSol, VERR_INVALID_POINTER);

    LogFlow((USBR3PROXY ":usbProxySolarisUrbAllocIsocBuf pUrbSol=%p\n", pUrbSol));

    PVUSBURB pUrb = pUrbSol->pVUsbUrb;
    bool fOut = (pUrb->enmDir == VUSBDIRECTION_OUT);
    uint32_t cIsocPkts = pUrb->cIsocPkts;
    size_t cbIsocPkt = pUrb->aIsocPkts[0].cb;           /* I hope this is okay. */
    size_t cbBufOut = sizeof(int) + sizeof(ugen_isoc_pkt_descr_t) * cIsocPkts;
    size_t cbBufIn = cbBufOut - sizeof(int);
    cbBufOut += cbIsocPkt * cIsocPkts;
    caddr_t pBuf = (caddr_t)RTMemAlloc(fOut ? cbBufOut : cbBufIn);
    if (pBuf)
    {
        ugen_isoc_req_head_t *pIsocReq = (ugen_isoc_req_head_t *)pBuf;
        ugen_isoc_pkt_descr_t *pIsocPkt = (ugen_isoc_pkt_descr_t *)pIsocReq->req_isoc_pkt_descrs;

        pIsocReq->req_isoc_pkts_count = cIsocPkts;
        for (ushort_t i = 0; i < cIsocPkts; i++)
        {
            pIsocPkt[i].dsc_isoc_pkt_len = pUrb->aIsocPkts[i].cb;
            pIsocPkt[i].dsc_isoc_pkt_actual_len = 0;
            pIsocPkt[i].dsc_isoc_pkt_status = 0;
        }

        /*
         * Copy the isoc. Out buffers.
         */
        if (fOut)
        {
            caddr_t pPayload = pBuf + sizeof(int) + sizeof(*pIsocPkt) * cIsocPkts;
            memcpy(pPayload, pUrb->abData, pUrb->cbData);
        }

        /*
         * Associate the buffer with the solaris URB to free it later as
         * we perform async. transfers.
         */
        pUrbSol->pIsocBufOut = pBuf;
        pUrbSol->cbIsocBufOut = cbBufOut;
        if (fOut)
        {
            pUrbSol->pIsocBufIn = NULL;
            pUrbSol->cbIsocBufIn = 0;
        }
        else
        {
            /*
             * Allocate space for read buffer. While perform isoc. IN request we will make
             * use of both the pvIsocIn and pvIsocOut buffers (see usbProxySolarisIsocReqAsync).
             */
            caddr_t pBufIn = (caddr_t)RTMemAlloc(cbBufIn);
            if (pBufIn)
            {
                pUrbSol->pIsocBufIn = pBufIn;
                pUrbSol->cbIsocBufIn = cbBufIn;
            }
            else
            {
                RTMemFree(pUrbSol->pIsocBufOut);
                return VERR_NO_MEMORY;
            }
        }

        LogFlow((USBR3PROXY ":usbProxySolarisUrbAllocIsocBuf success! cbBufIn=%ld cbBufOut=%ld\n", cbBufIn, cbBufOut));
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}
#endif


/**
 * Frees a Solaris URB request structure.
 *
 * @param   pDevSol         The Solaris USB device.
 * @param   pUrbSol         The Solaris URB to free.
 */
static void usbProxySolarisUrbFree(PUSBPROXYDEVSOL pDevSol, PUSBPROXYURBSOL pUrbSol)
{
    RTCritSectEnter(&pDevSol->CritSect);

    /*
     * Remove from the active or taxing list.
     */
    if (pUrbSol->pNext)
        pUrbSol->pNext->pPrev   = pUrbSol->pPrev;
    else if (pDevSol->pTaxingTail == pUrbSol)
        pDevSol->pTaxingTail    = pUrbSol->pPrev;

    if (pUrbSol->pPrev)
        pUrbSol->pPrev->pNext   = pUrbSol->pNext;
    else if (pDevSol->pTaxingHead == pUrbSol)
        pDevSol->pTaxingHead    = pUrbSol->pNext;
    else if (pDevSol->pInFlightHead == pUrbSol)
        pDevSol->pInFlightHead  = pUrbSol->pNext;
    else
        AssertFailed();

    /*
     * Link it into the free list.
     */
    pUrbSol->pPrev = NULL;
    pUrbSol->pNext = pDevSol->pFreeHead;
    pDevSol->pFreeHead = pUrbSol;

    pUrbSol->pVUsbUrb = NULL;
    pUrbSol->pDevSol = NULL;

    RTCritSectLeave(&pDevSol->CritSect);
}


/**
 * @copydoc USBPROXYBACK::pfnUrbQueue
 */
static int usbProxySolarisUrbQueue(PVUSBURB pUrb)
{
    PUSBPROXYDEV    pProxyDev = PDMINS_2_DATA(pUrb->pUsbIns, PUSBPROXYDEV);
    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;

    LogFlow((USBR3PROXY ":usbProxySolarisUrbQueue pUrb=%p pProxyDev=%p pDevSol=%p EndPt=%#x cbData=%d\n", pUrb, pProxyDev, pDevSol,
            pUrb->EndPt, pUrb->cbData));

    /*
     * Allocate a solaris urb.
     */
    PUSBPROXYURBSOL pUrbSol = usbProxySolarisUrbAlloc(pDevSol);
    if (!pUrbSol)
        return false;

    pUrbSol->u64SubmitTS = RTTimeMilliTS();
    pUrbSol->pVUsbUrb = pUrb;
    pUrbSol->pDevSol = pDevSol;
    PVUSBSETUP pSetup = (PVUSBSETUP)&pUrb->abData[0];

    uint8_t EndPt = pUrb->EndPt;
    if (pUrb->EndPt)
        EndPt = pUrb->EndPt | (pUrb->enmDir == VUSBDIRECTION_IN ? 0x80 : 0);

    int EndPtIndex = (EndPt & USBSOL_EPADDR_NUM_MASK) + ((EndPt & USBSOL_EPADDR_DIR_MASK) ? USBSOL_MAXENDPOINTS / 2 : 0);
    if (EndPtIndex < 0 ||  EndPtIndex > USBSOL_MAXENDPOINTS - 1)
    {
        LogRel((USBR3PROXY ":usbProxySolarisUrbQueue invalid endpoint address=%#x!\n", pUrb->EndPt));
        return VERR_INVALID_HANDLE;
    }

    Log((USBR3PROXY ":usbProxySolarisUrbQueue EndPoint=%#x EndPtIndex=%d XferType=%d Direction=%s\n", pUrb->EndPt, EndPtIndex, pUrb->enmType,
                    pUrb->enmDir == VUSBDIRECTION_IN ? "read" : "write"));

    /*
     * Open the endpoint.
     */
    int irc = VERR_GENERAL_FAILURE;
    int rc = usbProxySolarisOpenEndPoint(pDevSol, EndPt, pUrb->enmType);
    if (RT_SUCCESS(rc))
    {
        /*
         * Perform the transfer.
         */
        switch (pUrb->enmType)
        {
            case VUSBXFERTYPE_MSG:
            {
                AssertMsgBreak(pUrb->cbData >= sizeof(VUSBSETUP), ("cbData=%d\n", pUrb->cbData));
                if (EndPtIndex == 0)
                {
                    irc = usbProxySolarisCtrlReqAsync(pDevSol->aEpFile[EndPtIndex], pSetup, (caddr_t)(pSetup + 1), pSetup->wLength,
                            &pUrbSol->Status);
                }
                else
                {
                    irc = usbProxySolarisSendReqAsync(pDevSol->aEpFile[EndPtIndex], pSetup, (caddr_t)(pSetup + 1), pSetup->wLength,
                            &pUrbSol->Status);
                }
                break;
            }

            case VUSBXFERTYPE_INTR:
            case VUSBXFERTYPE_BULK:
            {
                AssertMsgBreak(pUrb->enmDir == VUSBDIRECTION_IN || pUrb->enmDir == VUSBDIRECTION_OUT, ("invalid enmDir=%d\n", pUrb->enmDir));
                irc = usbProxySolarisIOAsync(pDevSol->aEpFile[EndPtIndex], (caddr_t)(pUrb->abData), pUrb->cbData,
                            pUrb->enmDir == VUSBDIRECTION_IN ? RTFILE_O_READ : RTFILE_O_WRITE, &pUrbSol->Status);
                Log((USBR3PROXY ":usbProxySolarisUrbQueue enmType=%d\n", pUrb->enmType));
                break;
            }

#ifdef VBOX_WITH_SOLARIS_USB_ISOC
            case VUSBXFERTYPE_ISOC:
            {
                /*
                 * Allocate the isoc buffer and submit the request.
                 */
                irc = usbProxySolarisUrbAllocIsocBuf(pUrbSol);
                if (RT_SUCCESS(irc))
                    irc = usbProxySolarisIsocReqAsync(pDevSol->aEpFile[EndPtIndex], pUrbSol, &pUrbSol->Status);
                break;
            }
#endif

            default:
            {
                AssertMsgFailed(("%s: enmType=%#x\n", pUrb->pszDesc, pUrb->enmType));
                break;
            }
        }
    }
    if (   RT_SUCCESS(rc)
        && RT_SUCCESS(irc))
    {
        LogFlow((USBR3PROXY ":successfully queued USB %p Endpoint=%d EndPtIndex=%d enmType=%d cbData=%d\n", pUrb, pUrb->EndPt, EndPtIndex,
                pUrb->enmType, pUrb->cbData));
        pUrb->Dev.pvPrivate = pUrbSol;
        return true;
    }

    usbProxySolarisUrbFree(pDevSol, pUrbSol);
    LogRel((USBR3PROXY ":failed to transfer URB %p Endpoint=%d enmType=%d cbData=%d\n", pUrb, pUrb->EndPt, pUrb->enmType, pUrb->cbData));
    return false;
}


/**
 * Cancels the URB.
 * The URB requires reaping, so we don't change its state.
 */
static void usbProxySolarisUrbCancel(PVUSBURB pUrb)
{
    PUSBPROXYURBSOL pUrbSol = (PUSBPROXYURBSOL)pUrb->Dev.pvPrivate;

    LogFlow((USBR3PROXY ":usbProxySolarisUrbCancel pUrb=%p pUrbSol=%p pDevSol=%p", pUrb, pUrbSol, pUrbSol->pDevSol));

    int rc = aiocancel(&pUrbSol->Status.Result);
    Log((USBR3PROXY ":usbProxySolarisUrbCancel aiocancel returned to %d.\n", rc));
    NOREF(rc);
}


/**
 * Reap URBs in-flight on a device.
 *
 * @returns Pointer to a completed URB.
 * @returns NULL if no URB was completed.
 * @param   pProxyDev   The device.
 * @param   cMillies    Number of milliseconds to wait. Use 0 to not wait at all.
 */
static PVUSBURB usbProxySolarisUrbReap(PUSBPROXYDEV pProxyDev, RTMSINTERVAL cMillies)
{
    LogFlow((USBR3PROXY ":usbProxySolarisUrbReap pProxyDev=%p cMillies=%u\n", pProxyDev, cMillies));

    /*
     * Deque URBs inflight or those landed.
     */
    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;
    if (   pDevSol->pInFlightHead
        || pDevSol->pTaxingHead)
    {
        struct timeval *pTimeout = NULL;
        if (cMillies > 0)
        {
            struct timeval Timeout;
            Timeout.tv_sec  = cMillies >= 1000 ? cMillies / 1000L : 0;
            Timeout.tv_usec = cMillies >= 1000 ? cMillies % 1000L : cMillies;
            pTimeout = &Timeout;
        }
        usbProxySolarisUrbComplete(pTimeout);
    }

    /*
     * Any URBs pending delivery?
     */
    PVUSBURB pUrb = NULL;
    while (     pDevSol->pTaxingHead
           &&   !pUrb)
    {
        RTCritSectEnter(&pDevSol->CritSect);

        PUSBPROXYURBSOL pUrbSol = pDevSol->pTaxingHead;
        if (pUrbSol)
        {
            pUrb = pUrbSol->pVUsbUrb;
            if (pUrb)
            {
                pUrb->Dev.pvPrivate = NULL;
                usbProxySolarisUrbFree(pDevSol, pUrbSol);
            }
        }
        RTCritSectLeave(&pDevSol->CritSect);
    }

    if (pUrb)
        Log(("%s: usbProxySolarisUrbReap: pProxyDev=%s returns %p enmStatus=%d\n", pUrb->pszDesc, pProxyDev->pUsbIns->pszName, pUrb, pUrb->enmStatus));
    return pUrb;
}

/**
 * The Solaris USB Proxy Backend.
 */
extern const USBPROXYBACK g_USBProxyDeviceHost =
{
    "host",
    usbProxySolarisOpen,
    usbProxySolarisInit,
    usbProxySolarisClose,
    usbProxySolarisReset,
    usbProxySolarisSetConfig,
    usbProxySolarisClaimInterface,
    usbProxySolarisReleaseInterface,
    usbProxySolarisSetInterface,
    usbProxySolarisClearHaltedEp,
    usbProxySolarisUrbQueue,
    usbProxySolarisUrbCancel,
    usbProxySolarisUrbReap,
    NULL
};

