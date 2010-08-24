/* $Id$ */
/** @file
 * USB device proxy - the FreeBSD backend.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#ifdef VBOX
# include <iprt/stdint.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_ioctl.h>

#include <VBox/pdm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/vusb.h>
#include <iprt/assert.h>
#include <iprt/stream.h>
#include <iprt/alloc.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/file.h>
#include "../USBProxyDevice.h"

/** Maximum endpoints supported. */
#define USBFBSD_MAXENDPOINTS       32
#define USBFBSD_EPADDR_NUM_MASK  0x0F
#define USBFBSD_EPADDR_DIR_MASK  0x80
#define USBPROXY_FREEBSD_NO_ENTRY_FREE ((unsigned)~0)
/** This really needs to be defined in vusb.h! */
#ifndef VUSB_DIR_TO_DEV
# define VUSB_DIR_TO_DEV        0x00
#endif

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct VUSBURBFBSD
{
    /** Pointer to the URB. */
    PVUSBURB    pUrb;
    /** Buffer pointers. */
    void       *apvData[2];
    /** Buffer lengths. */
    uint32_t    acbData[2];
} VUSBURBFBSD, *PVUSBURBFBSD;

typedef struct USBENDPOINTFBSD
{
    /** Flag whether it is opened. */
    bool        fOpen;
    /** Index in the endpoint list. */
    unsigned    iEndpoint;
    /** Associated endpoint. */
    struct usb_fs_endpoint *pXferEndpoint;
} USBENDPOINTFBSD, *PUSBENDPOINTFBSD;

/**
 * Data for the FreeBSD usb proxy backend.
 */
typedef struct USBPROXYDEVFBSD
{
    /** The open file. */
    RTFILE                  File;
    /** Critical section protecting the two lists. */
    RTCRITSECT              CritSect;
    /** Pointer to the array of USB endpoints. */
    struct usb_fs_endpoint *paXferEndpoints;
    /** Pointer to the array of URB structures.
     * They entries must be in sync with the above array. */
    PVUSBURBFBSD            paUrbs;
    /** Number of entries in both arrays. */
    unsigned                cXferEndpoints;
    /** Pointer to the Fifo containing the indexes for free Xfer
     * endpoints. */
    unsigned               *paXferFree;
    /** Index of the next free entry to write to. */
    unsigned                iXferFreeNextWrite;
    /** Index of the next entry to read from. */
    unsigned                iXferFreeNextRead;
    /** Status of opened endpoints. */
    USBENDPOINTFBSD         aEpOpened[USBFBSD_MAXENDPOINTS];
    /** The list of landed FreeBSD URBs. Doubly linked.
     * Only the split head will appear in this list. */
    PVUSBURB                pTaxingHead;
    /** The tail of the landed FreeBSD URBs. */
    PVUSBURB                pTaxingTail;
} USBPROXYDEVFBSD, *PUSBPROXYDEVFBSD;

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int usbProxyFreeBSDDoIoCtl(PUSBPROXYDEV pProxyDev, unsigned long iCmd, void *pvArg, bool fHandleNoDev, uint32_t cTries);
static void usbProxFreeBSDUrbUnplugged(PUSBPROXYDEV pProxyDev);
static PUSBENDPOINTFBSD usbProxyFreeBSDEndpointOpen(PUSBPROXYDEV pProxyDev, int Endpoint);
static int usbProxyFreeBSDEndpointClose(PUSBPROXYDEV pProxyDev, int Endpoint);

/**
 * Wrapper for the ioctl call.
 *
 * This wrapper will repeate the call if we get an EINTR or EAGAIN. It can also
 * handle ENODEV (detached device) errors.
 *
 * @returns whatever ioctl returns.
 * @param   pProxyDev       The proxy device.
 * @param   iCmd            The ioctl command / function.
 * @param   pvArg           The ioctl argument / data.
 * @param   fHandleNoDev    Whether to handle ENXIO.
 * @param   cTries          The number of retries. Use UINT32_MAX for (kind of) indefinite retries.
 * @internal
 */
static int usbProxyFreeBSDDoIoCtl(PUSBPROXYDEV pProxyDev, unsigned long iCmd, void *pvArg, bool fHandleNoDev, uint32_t cTries)
{
    int rc = VINF_SUCCESS;

    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;
    do
    {
        do
        {
            rc = ioctl(pDevFBSD->File, iCmd, pvArg);
            if (rc >= 0)
                return rc;
        } while (errno == EINTR);

        if (errno == ENXIO && fHandleNoDev)
        {
            usbProxFreeBSDUrbUnplugged(pProxyDev);
            Log(("usb-freebsd: ENXIO -> unplugged. pProxyDev=%s\n", pProxyDev->pUsbIns->pszName));
            errno = ENODEV;
            break;
        }
        if (errno != EAGAIN)
        {
            LogFlow(("usbProxyFreeBSDDoIoCtl returned %Rrc\n", RTErrConvertFromErrno(errno)));
            break;
        }
    } while (cTries-- > 0);

    return rc;
}

/**
 * Setup a USB request packet.
 */
static void usbProxyFreeBSDSetupReq(struct usb_device_request *pSetupData, uint8_t bmRequestType, uint8_t bRequest,
                        uint16_t wValue, uint16_t wIndex, uint16_t wLength)
{
    LogFlow(("usbProxyFreeBSDSetupReq: pSetupData=%p bmRequestType=%#x bRequest=%#x wValue=%#x wIndex=%#x wLength=%#x\n",
                pSetupData, bmRequestType, bRequest, wValue, wIndex, wLength));

    pSetupData->bmRequestType = bmRequestType;
    pSetupData->bRequest      = bRequest;

    /* Handle endianess here. Currently no swapping is needed. */
    pSetupData->wValue[0]  = wValue & 0xff;
    pSetupData->wValue[1]  = (wValue >> 8) & 0xff;
    pSetupData->wIndex[0]  = wIndex & 0xff;
    pSetupData->wIndex[1]  = (wIndex >> 8) & 0xff;
    pSetupData->wLength[0]  = wLength & 0xff;
    pSetupData->wLength[1]  = (wLength >> 8) & 0xff;
//    pSetupData->wIndex  = wIndex;
//    pSetupData->wLength = wLength;
}

/**
 * The device has been unplugged.
 * Cancel all in-flight URBs and put them up for reaping.
 */
static void usbProxFreeBSDUrbUnplugged(PUSBPROXYDEV pProxyDev)
{
    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;

    /*
     * Shoot down all flying URBs.
     */
    RTCritSectEnter(&pDevFBSD->CritSect);
    pProxyDev->fDetached = true;

#if 0 /** @todo */
    PUSBPROXYURBFBSD pUrbTaxing = NULL;
    PUSBPROXYURBFBSD pUrbFBSD = pDevLnx->pInFlightHead;
    pDevFBSD->pInFlightHead = NULL;
    while (pUrbFBSD)
    {
        PUSBPROXYURBFBSD pCur = pUrbFBSD;
        pUrbFBSD = pUrbFBSD->pNext;

        ioctl(pDevFBSD->File, USBDEVFS_DISCARDURB, &pCur->KUrb);
        if (!pCur->KUrb.status)
            pCur->KUrb.status = -ENODEV;

        /* insert into the taxing list. */
        pCur->pPrev = NULL;
        if (    !pCur->pSplitHead
            ||  pCur == pCur->pSplitHead)
        {
            pCur->pNext = pUrbTaxing;
            if (pUrbTaxing)
                pUrbTaxing->pPrev = pCur;
            pUrbTaxing = pCur;
        }
        else
            pCur->pNext = NULL;
    }

    /* Append the URBs we shot down to the taxing queue. */
    if (pUrbTaxing)
    {
        pUrbTaxing->pPrev = pDevFBSD->pTaxingTail;
        if (pUrbTaxing->pPrev)
            pUrbTaxing->pPrev->pNext = pUrbTaxing;
        else
            pDevFBSD->pTaxingTail = pDevFBSD->pTaxingHead = pUrbTaxing;
    }
#endif
    RTCritSectLeave(&pDevFBSD->CritSect);
}

DECLINLINE(void) usbProxyFreeBSDSetEntryFree(PUSBPROXYDEVFBSD pProxyDev, unsigned iEntry)
{
    pProxyDev->paXferFree[pProxyDev->iXferFreeNextWrite] = iEntry;
    pProxyDev->iXferFreeNextWrite++;
    pProxyDev->iXferFreeNextWrite %= (pProxyDev->cXferEndpoints+1);
}

DECLINLINE(unsigned) usbProxyFreeBSDGetEntryFree(PUSBPROXYDEVFBSD pProxyDev)
{
    unsigned iEntry;

    if (pProxyDev->iXferFreeNextWrite != pProxyDev->iXferFreeNextRead)
    {
        iEntry = pProxyDev->paXferFree[pProxyDev->iXferFreeNextRead];
        pProxyDev->iXferFreeNextRead++;
        pProxyDev->iXferFreeNextRead %= (pProxyDev->cXferEndpoints+1);
    }
    else
        iEntry = USBPROXY_FREEBSD_NO_ENTRY_FREE;

    return iEntry;
}

static PUSBENDPOINTFBSD usbProxyFreeBSDEndpointOpen(PUSBPROXYDEV pProxyDev, int Endpoint)
{
    LogFlow(("usbProxyFreeBSDEndpointOpen: pProxyDev=%p Endpoint=%d\n", pProxyDev, Endpoint));

    int EndPtIndex = (Endpoint & USBFBSD_EPADDR_NUM_MASK) + ((Endpoint & USBFBSD_EPADDR_DIR_MASK) ? USBFBSD_MAXENDPOINTS / 2 : 0);
    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;
    PUSBENDPOINTFBSD pEndpointFBSD = &pDevFBSD->aEpOpened[EndPtIndex];
    struct usb_fs_endpoint *pXferEndpoint;

    AssertMsg(EndPtIndex < USBFBSD_MAXENDPOINTS, ("Endpoint index exceeds limit %d\n", EndPtIndex));

    if (!pEndpointFBSD->fOpen)
    {
        struct usb_fs_open UsbFsOpen;

        pEndpointFBSD->iEndpoint = usbProxyFreeBSDGetEntryFree(pDevFBSD);
        if (pEndpointFBSD->iEndpoint == USBPROXY_FREEBSD_NO_ENTRY_FREE)
            return NULL;

        LogFlow(("usbProxyFreeBSDEndpointOpen: ep_index=%d\n", pEndpointFBSD->iEndpoint));

        UsbFsOpen.ep_index    = pEndpointFBSD->iEndpoint;
        UsbFsOpen.ep_no       = Endpoint;
        UsbFsOpen.max_bufsize = 256 * _1K; /* Hardcoded assumption about the URBs we get. */
        UsbFsOpen.max_frames  = 2;

        int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_OPEN, &UsbFsOpen, true, UINT32_MAX);
        if (rc)
            return NULL;

        pEndpointFBSD->fOpen = true;
        pEndpointFBSD->pXferEndpoint = &pDevFBSD->paXferEndpoints[pEndpointFBSD->iEndpoint];
    }
    else
    {
        AssertMsgReturn(!pDevFBSD->paUrbs[pEndpointFBSD->iEndpoint].pUrb, ("Endpoint is busy"), NULL);
        pEndpointFBSD->pXferEndpoint = &pDevFBSD->paXferEndpoints[pEndpointFBSD->iEndpoint];
    }

    return pEndpointFBSD;
}

static int usbProxyFreeBSDEndpointClose(PUSBPROXYDEV pProxyDev, int Endpoint)
{
    LogFlow(("usbProxyFreeBSDEndpointClose: pProxyDev=%p Endpoint=%d\n", pProxyDev, Endpoint));

    AssertMsg(Endpoint < USBFBSD_MAXENDPOINTS, ("Endpoint index exceeds limit %d\n", Endpoint));

    int EndPtIndex = (Endpoint & USBFBSD_EPADDR_NUM_MASK) + ((Endpoint & USBFBSD_EPADDR_DIR_MASK) ? USBFBSD_MAXENDPOINTS / 2 : 0);
    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;
    PUSBENDPOINTFBSD pEndpointFBSD = &pDevFBSD->aEpOpened[EndPtIndex];

    if (pEndpointFBSD->fOpen)
    {
        struct usb_fs_close UsbFsClose;

        AssertMsgReturn(!pDevFBSD->paUrbs[pEndpointFBSD->iEndpoint].pUrb, ("Endpoint is busy"), NULL);

        UsbFsClose.ep_index = pEndpointFBSD->iEndpoint;

        int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_CLOSE, &UsbFsClose, true, UINT32_MAX);
        if (rc)
        {
            LogFlow(("usbProxyFreeBSDEndpointClose: failed rc=%d errno=%Rrc\n", rc, RTErrConvertFromErrno(errno)));
            return RTErrConvertFromErrno(errno);
        }

        usbProxyFreeBSDSetEntryFree(pDevFBSD, pEndpointFBSD->iEndpoint);
        pEndpointFBSD->fOpen = false;
    }

    return VINF_SUCCESS;
}

/**
 * Opens the device file.
 *
 * @returns VBox status code.
 * @param   pProxyDev       The device instance.
 * @param   pszAddress      If we are using usbfs, this is the path to the
 *                          device.  If we are using sysfs, this is a string of
 *                          the form "sysfs:<sysfs path>//device:<device node>".
 *                          In the second case, the two paths are guaranteed
 *                          not to contain the substring "//".
 * @param   pvBackend       Backend specific pointer, unused for the linux backend.
 */
static int usbProxyFreeBSDOpen(PUSBPROXYDEV pProxyDev, const char *pszAddress, void *pvBackend)
{
    LogFlow(("usbProxyFreeBSDOpen: pProxyDev=%p pszAddress=%s\n", pProxyDev, pszAddress));

    /*
     * Try open the device node.
     */
    RTFILE File;
    int rc = RTFileOpen(&File, pszAddress, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate and initialize the linux backend data.
         */
        PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)RTMemAllocZ(sizeof(USBPROXYDEVFBSD));
        if (pDevFBSD)
        {
            pDevFBSD->File = File;
            rc = RTCritSectInit(&pDevFBSD->CritSect);
            if (RT_SUCCESS(rc))
            {
                unsigned cTransfersMax = 127; /* Maximum in the kernel atm. */

                /* Allocate arrays for data transfers. */
                pDevFBSD->paXferEndpoints = (struct usb_fs_endpoint *)RTMemAllocZ(cTransfersMax * sizeof(struct usb_fs_endpoint));
                pDevFBSD->paUrbs          = (PVUSBURBFBSD)RTMemAllocZ(cTransfersMax * sizeof(VUSBURBFBSD));
                pDevFBSD->paXferFree      = (unsigned *)RTMemAllocZ((cTransfersMax + 1) * sizeof(unsigned));
                pDevFBSD->cXferEndpoints  = cTransfersMax;

                if (pDevFBSD->paXferEndpoints && pDevFBSD->paUrbs && pDevFBSD->paXferFree)
                {
                    /* Initialize the kernel side. */
                    struct usb_fs_init UsbFsInit;

                    UsbFsInit.pEndpoints   = pDevFBSD->paXferEndpoints;
                    UsbFsInit.ep_index_max = cTransfersMax;
                    rc = ioctl(File, USB_FS_INIT, &UsbFsInit);
                    if (!rc)
                    {
                        for (unsigned i = 0; i < cTransfersMax; i++)
                            usbProxyFreeBSDSetEntryFree(pDevFBSD, i);

                        for (unsigned i= 0; i < USBFBSD_MAXENDPOINTS; i++)
                            pDevFBSD->aEpOpened[i].fOpen = false;

                        pProxyDev->Backend.pv = pDevFBSD;

                        LogFlow(("usbProxyFreeBSDOpen(%p, %s): returns successfully File=%d iActiveCfg=%d\n",
                                 pProxyDev, pszAddress, pDevFBSD->File, pProxyDev->iActiveCfg));

                        return VINF_SUCCESS;
                    }
                    else
                        rc = RTErrConvertFromErrno(errno);
                }
                else
                    rc = VERR_NO_MEMORY;

                if (pDevFBSD->paXferEndpoints)
                    RTMemFree(pDevFBSD->paXferEndpoints);
                if (pDevFBSD->paUrbs)
                    RTMemFree(pDevFBSD->paUrbs);
                if (pDevFBSD->paXferFree)
                    RTMemFree(pDevFBSD->paXferFree);
            }

            RTMemFree(pDevFBSD);
        }
        else
            rc = VERR_NO_MEMORY;
        RTFileClose(File);
    }
    else if (rc == VERR_ACCESS_DENIED)
        rc = VERR_VUSB_USBFS_PERMISSION;

    Log(("usbProxyFreeBSDOpen(%p, %s) failed, rc=%Rrc!\n", pProxyDev, pszAddress, rc));
    pProxyDev->Backend.pv = NULL;

    NOREF(pvBackend);
    return rc;

    return VINF_SUCCESS;
}


/**
 * Claims all the interfaces and figures out the
 * current configuration.
 *
 * @returns VINF_SUCCESS.
 * @param   pProxyDev       The proxy device.
 */
static int usbProxyFreeBSDInit(PUSBPROXYDEV pProxyDev)
{
    LogFlow(("usbProxyFreeBSDInit: pProxyDev=%s\n", pProxyDev->pUsbIns->pszName));
    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;

    /* Retrieve current active configuration. */
    int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_GET_CONFIG, &pProxyDev->iActiveCfg, true, UINT32_MAX);
    if (RT_FAILURE(rc))
    {
        pProxyDev->iActiveCfg = -1;
        return rc;
    }

    Log(("usbProxyFreeBSDInit: iActiveCfg=%d\n", pProxyDev->iActiveCfg));
    pProxyDev->cIgnoreSetConfigs = 1;
    pProxyDev->iActiveCfg++;

    return VINF_SUCCESS;
}


/**
 * Closes the proxy device.
 */
static void usbProxyFreeBSDClose(PUSBPROXYDEV pProxyDev)
{
    LogFlow(("usbProxyFreeBSDClose: pProxyDev=%s\n", pProxyDev->pUsbIns->pszName));
    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;
    Assert(pDevFBSD);
    if (!pDevFBSD)
        return;

    RTCritSectDelete(&pDevFBSD->CritSect);

    struct usb_fs_uninit UsbFsUninit;
    UsbFsUninit.dummy = 0;

    int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_UNINIT, &UsbFsUninit, false, 1);
    AssertMsg(!rc, ("Freeing kernel ressources failed rc=%Rrc\n", RTErrConvertFromErrno(errno)));

    if (pDevFBSD->paXferEndpoints)
        RTMemFree(pDevFBSD->paXferEndpoints);
    if (pDevFBSD->paUrbs)
        RTMemFree(pDevFBSD->paUrbs);
    if (pDevFBSD->paXferFree)
        RTMemFree(pDevFBSD->paXferFree);

    RTFileClose(pDevFBSD->File);
    pDevFBSD->File = NIL_RTFILE;

    RTMemFree(pDevFBSD);
    pProxyDev->Backend.pv = NULL;

    LogFlow(("usbProxyFreeBSDClose: returns\n"));
}


/**
 * Reset a device.
 *
 * @returns VBox status code.
 * @param   pDev    The device to reset.
 */
static int usbProxyFreeBSDReset(PUSBPROXYDEV pProxyDev, bool fResetOnFreeBSD)
{
    LogFlow(("usbProxyFreeBSDReset: pProxyDev=%s\n", pProxyDev->pUsbIns->pszName));
    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;

    /* Close any open endpoints. */
    for (unsigned i = 0; i < USBFBSD_MAXENDPOINTS; i++)
        usbProxyFreeBSDEndpointClose(pProxyDev, i);

    /* We need to release kernel ressources first. */
    struct usb_fs_uninit UsbFsUninit;
    UsbFsUninit.dummy = 0;

    int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_UNINIT, &UsbFsUninit, false, 1);
    AssertMsg(!rc, ("Freeing kernel ressources failed rc=%Rrc\n", RTErrConvertFromErrno(errno)));

    /* Resetting is not possible from a normal user account */
#if 0
    int iUnused = 0;
    rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_DEVICEENUMERATE, &iUnused, true, UINT32_MAX);
    if (rc)
        return RTErrConvertFromErrno(errno);
#endif

    /* Allocate kernel ressources again. */
    struct usb_fs_init UsbFsInit;

    UsbFsInit.pEndpoints   = pDevFBSD->paXferEndpoints;
    UsbFsInit.ep_index_max = pDevFBSD->cXferEndpoints;
    rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_INIT, &UsbFsInit, true, UINT32_MAX);
    if (!rc)
    {
        /* Retrieve current active configuration. */
        rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_GET_CONFIG, &pProxyDev->iActiveCfg, true, UINT32_MAX);
        if (rc)
        {
            pProxyDev->iActiveCfg = -1;
            rc = RTErrConvertFromErrno(errno);
        }
        else
        {
            pProxyDev->cIgnoreSetConfigs = 2;
            pProxyDev->iActiveCfg++;
        }
    }
    else
        rc = RTErrConvertFromErrno(errno);

    Log(("usbProxyFreeBSDReset: iActiveCfg=%d\n", pProxyDev->iActiveCfg));

    return rc;
}


/**
 * SET_CONFIGURATION.
 *
 * The caller makes sure that it's not called first time after open or reset
 * with the active interface.
 *
 * @returns success indicator.
 * @param   pProxyDev       The device instance data.
 * @param   iCfg            The configuration to set.
 */
static int usbProxyFreeBSDSetConfig(PUSBPROXYDEV pProxyDev, int iCfg)
{
    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;

    LogFlow(("usbProxyFreeBSDSetConfig: pProxyDev=%s cfg=%#x\n",
             pProxyDev->pUsbIns->pszName, iCfg));

    /* Close any open endpoints. */
    for (unsigned i = 0; i < USBFBSD_MAXENDPOINTS; i++)
        usbProxyFreeBSDEndpointClose(pProxyDev, i);

    /* We need to release kernel ressources first. */
    struct usb_fs_uninit UsbFsUninit;
    UsbFsUninit.dummy = 0;

    int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_UNINIT, &UsbFsUninit, false, 1);
    AssertMsg(!rc, ("Freeing kernel ressources failed rc=%Rrc\n", RTErrConvertFromErrno(errno)));

    int iCfgIndex = 0;

    /* Get theconfiguration index matching the value. */
    for (iCfgIndex = 0; iCfgIndex < pProxyDev->DevDesc.bNumConfigurations; iCfgIndex++)
    {
        if (pProxyDev->paCfgDescs[iCfgIndex].Core.bConfigurationValue == iCfg)
            break;
    }

    if (RT_UNLIKELY(iCfgIndex == pProxyDev->DevDesc.bNumConfigurations))
    {
        LogFlow(("usbProxyFreeBSDSetConfig: configuration %d not found\n", iCfg));
        return false;
    }

    /* Set the config */
    rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_SET_CONFIG, &iCfgIndex, true, UINT32_MAX);
    if (RT_FAILURE(rc))
    {
        LogFlow(("usbProxyFreeBSDSetConfig: setting config index %d failed rc=%d errno=%Rrc\n", iCfgIndex, rc, RTErrConvertFromErrno(errno)));
        return false;
    }

    /* Allocate kernel ressources again. */
    struct usb_fs_init UsbFsInit;

    UsbFsInit.pEndpoints   = pDevFBSD->paXferEndpoints;
    UsbFsInit.ep_index_max = pDevFBSD->cXferEndpoints;
    rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_INIT, &UsbFsInit, true, UINT32_MAX);


    LogFlow(("usbProxyFreeBSDSetConfig: rc=%d errno=%Rrc\n", rc, RTErrConvertFromErrno(errno)));

    if (!rc)
        return true;
    else
        return false;
}


/**
 * Claims an interface.
 * @returns success indicator.
 */
static int usbProxyFreeBSDClaimInterface(PUSBPROXYDEV pProxyDev, int iIf)
{
    LogFlow(("usbProxyFreeBSDClaimInterface: pProxyDev=%s ifnum=%#x\n", pProxyDev->pUsbIns->pszName, iIf));

    int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_CLAIM_INTERFACE, &iIf, true, UINT32_MAX);
    if (RT_FAILURE(rc))
        return false;

    return true;
}


/**
 * Releases an interface.
 * @returns success indicator.
 */
static int usbProxyFreeBSDReleaseInterface(PUSBPROXYDEV pProxyDev, int iIf)
{
    LogFlow(("usbProxyFreeBSDReleaseInterface: pProxyDev=%s ifnum=%#x\n", pProxyDev->pUsbIns->pszName, iIf));

    int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_RELEASE_INTERFACE, &iIf, true, UINT32_MAX);
    if (RT_FAILURE(rc))
        return false;

    return true;
}


/**
 * SET_INTERFACE.
 *
 * @returns success indicator.
 */
static int usbProxyFreeBSDSetInterface(PUSBPROXYDEV pProxyDev, int iIf, int iAlt)
{
    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;

    LogFlow(("usbProxyFreeBSDSetInterface: pProxyDev=%p iIf=%#x iAlt=%#x\n", pProxyDev, iIf, iAlt));

    /* Close any open endpoints. */
    for (unsigned i = 0; i < USBFBSD_MAXENDPOINTS; i++)
        usbProxyFreeBSDEndpointClose(pProxyDev, i);

    /* We need to release kernel ressources first. */
    struct usb_fs_uninit UsbFsUninit;
    UsbFsUninit.dummy = 0;

    int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_UNINIT, &UsbFsUninit, false, 1);
    AssertMsg(!rc, ("Freeing kernel ressources failed rc=%Rrc\n", RTErrConvertFromErrno(errno)));

    struct usb_alt_interface UsbIntAlt;
    UsbIntAlt.uai_interface_index = iIf;
    UsbIntAlt.uai_alt_index = iAlt;
    rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_SET_ALTINTERFACE, &UsbIntAlt, true, UINT32_MAX);
    if (rc)
    {
        LogFlow(("usbProxyFreeBSDSetInterface: Setting interface %d %d failed rc=%d errno=%Rrc\n", iIf, iAlt, rc,RTErrConvertFromErrno(errno)));
        return false;
    }

    /* Allocate kernel ressources again. */
    struct usb_fs_init UsbFsInit;

    UsbFsInit.pEndpoints   = pDevFBSD->paXferEndpoints;
    UsbFsInit.ep_index_max = pDevFBSD->cXferEndpoints;
    rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_INIT, &UsbFsInit, true, UINT32_MAX);

    LogFlow(("usbProxyFreeBSDSetInterface: rc=%d errno=%Rrc\n", rc, RTErrConvertFromErrno(errno)));

    if (!rc)
        return true;
    else
        return false;
}


/**
 * Clears the halted endpoint 'EndPt'.
 */
static bool usbProxyFreeBSDClearHaltedEp(PUSBPROXYDEV pProxyDev, unsigned int EndPt)
{
    LogFlow(("usbProxyFreeBSDClearHaltedEp: pProxyDev=%s EndPt=%u\n", pProxyDev->pUsbIns->pszName, EndPt));

    /*
     * Clearing the zero control pipe doesn't make sense. Just ignore it.
     */
    if (EndPt == 0)
        return true;

    struct usb_ctl_request Req;

    memset(&Req, 0, sizeof(struct usb_ctl_request));
    usbProxyFreeBSDSetupReq(&Req.ucr_request, VUSB_DIR_TO_DEV | VUSB_TO_ENDPOINT, VUSB_REQ_CLEAR_FEATURE, 0, EndPt, 0);

    int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_DO_REQUEST, &Req, true, 1);
    if (rc)
    {
        LogFlow(("usbProxyFreeBSDClearHaltedEp: failed rc=%d errno=%Rrc\n", rc, RTErrConvertFromErrno(errno)));
        return false;
    }

    LogFlow(("usbProxyFreeBSDClearHaltedEp: succeeded\n"));

    return true;
}


/**
 * @copydoc USBPROXYBACK::pfnUrbQueue
 */
static int usbProxyFreeBSDUrbQueue(PVUSBURB pUrb)
{
    PUSBPROXYDEV pProxyDev = PDMINS_2_DATA(pUrb->pUsbIns, PUSBPROXYDEV);
    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;

    LogFlow(("usbProxyFreeBSDUrbQueue: pUrb=%p\n", pUrb));

    uint8_t EndPt = pUrb->EndPt;
    if (pUrb->EndPt)
        EndPt = pUrb->EndPt | (pUrb->enmDir == VUSBDIRECTION_IN ? 0x80 : 0);

    PUSBENDPOINTFBSD pEndpointFBSD = usbProxyFreeBSDEndpointOpen(pProxyDev, EndPt);
    if (!pEndpointFBSD)
        return false;

    PVUSBURBFBSD pUrbFBSD = &pDevFBSD->paUrbs[pEndpointFBSD->iEndpoint];
    AssertMsg(!pUrbFBSD->pUrb, ("Assigned entry is busy\n"));
    pUrbFBSD->pUrb = pUrb;

    struct usb_fs_start UsbFsStart;
    unsigned cFrames;

    if (pUrb->enmType == VUSBXFERTYPE_MSG)
    {
        PVUSBSETUP pSetup = (PVUSBSETUP)&pUrb->abData[0];

        pUrbFBSD->apvData[0] = pSetup;
        pUrbFBSD->acbData[0] = sizeof(VUSBSETUP);

        if (pSetup->wLength)
        {
            pUrbFBSD->apvData[1] = &pUrb->abData[sizeof(VUSBSETUP)];
            pUrbFBSD->acbData[1] = pSetup->wLength;
            cFrames = 2;
        }
        else
            cFrames = 1;
    }
    else
    {
        pUrbFBSD->apvData[0] = &pUrb->abData[0];
        pUrbFBSD->acbData[0] = pUrb->cbData;
        cFrames = 1;
    }

    struct usb_fs_endpoint *pXferEndpoint = pEndpointFBSD->pXferEndpoint;
    pXferEndpoint->ppBuffer = &pUrbFBSD->apvData[0];
    pXferEndpoint->pLength  = &pUrbFBSD->acbData[0];
    pXferEndpoint->nFrames  = cFrames;
    pXferEndpoint->timeout  = USB_FS_TIMEOUT_NONE; /* Timeout handling will be done during reap. */
    pXferEndpoint->flags    = pUrb->fShortNotOk ? 0 : USB_FS_FLAG_MULTI_SHORT_OK;

    /* Start the transfer */
    UsbFsStart.ep_index = pEndpointFBSD->iEndpoint;
    int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_START, &UsbFsStart, true, UINT32_MAX);

    LogFlow(("usbProxyFreeBSDUrbQueue: USB_FS_START returned rc=%d errno=%Rrc\n", rc, RTErrConvertFromErrno(errno)));
    if (rc)
    {
        return false;
    }

    return true;
}


/**
 * Reap URBs in-flight on a device.
 *
 * @returns Pointer to a completed URB.
 * @returns NULL if no URB was completed.
 * @param   pProxyDev   The device.
 * @param   cMillies    Number of milliseconds to wait. Use 0 to not wait at all.
 */
static PVUSBURB usbProxyFreeBSDUrbReap(PUSBPROXYDEV pProxyDev, RTMSINTERVAL cMillies)
{
    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;

    LogFlow(("usbProxyFreeBSDUrbReap: cMillies=%u\n", cMillies));

    /* We will poll for finished urbs because the ioctl doesn't take a timeout parameter. */
    struct pollfd PollFd;
    PVUSBURB pUrb = NULL;

    PollFd.fd      = (int)pDevFBSD->File;
    PollFd.events  = POLLIN | POLLRDNORM | POLLOUT;
    PollFd.revents = 0;

    struct usb_fs_complete UsbFsComplete;

    UsbFsComplete.ep_index = 0;

    int rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_COMPLETE, &UsbFsComplete, true, UINT32_MAX);
    if (!rc)
    {
        struct usb_fs_endpoint *pXferEndpoint = &pDevFBSD->paXferEndpoints[UsbFsComplete.ep_index];
        PVUSBURBFBSD pUrbFBSD = &pDevFBSD->paUrbs[UsbFsComplete.ep_index];

        LogFlow(("Reaped URB %#p\n", pUrbFBSD->pUrb));

        pUrb = pUrbFBSD->pUrb;
        AssertMsg(pUrb, ("No URB handle for the completed entry\n"));
        pUrbFBSD->pUrb = NULL;

        switch (pXferEndpoint->status)
        {
            case USB_ERR_NORMAL_COMPLETION:
                pUrb->enmStatus = VUSBSTATUS_OK;
                break;
            case USB_ERR_STALLED:
                pUrb->enmStatus = VUSBSTATUS_STALL;
                break;
            default:
                AssertMsgFailed(("Unexpected status code %d\n", pXferEndpoint->status));
        }
    }
    else
    {

        rc = poll(&PollFd, 1, cMillies == RT_INDEFINITE_WAIT ? INFTIM : cMillies);
        if (rc == 1)
        {
    //        do
            {
                UsbFsComplete.ep_index = 0;
                rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_COMPLETE, &UsbFsComplete, true, UINT32_MAX);
                if (!rc)
                {
                    struct usb_fs_endpoint *pXferEndpoint = &pDevFBSD->paXferEndpoints[UsbFsComplete.ep_index];
                    PVUSBURBFBSD pUrbFBSD = &pDevFBSD->paUrbs[UsbFsComplete.ep_index];

                    LogFlow(("Reaped URB %#p\n", pUrbFBSD->pUrb));

                    pUrb = pUrbFBSD->pUrb;
                    AssertMsg(pUrb, ("No URB handle for the completed entry\n"));
                    pUrbFBSD->pUrb = NULL;

                    switch (pXferEndpoint->status)
                    {
                        case USB_ERR_NORMAL_COMPLETION:
                            pUrb->enmStatus = VUSBSTATUS_OK;
                            break;
                        case USB_ERR_STALLED:
                            pUrb->enmStatus = VUSBSTATUS_STALL;
                            break;
                        default:
                            AssertMsgFailed(("Unexpected status code %d\n", pXferEndpoint->status));
                    }

                    rc = usbProxyFreeBSDDoIoCtl(pProxyDev, USB_FS_COMPLETE, &UsbFsComplete, true, UINT32_MAX);
                    AssertMsg(((rc == -1) && (errno == EBUSY)), ("Expected return value rc=%d rc=%Rrc\n", rc, RTErrConvertFromErrno(errno)));
                }
                else
                    LogFlow(("couldn't get completed URB rc=%Rrc\n", RTErrConvertFromErrno(errno)));
            }
    //        while (!rc);
        }
        else
            LogFlow(("poll returned rc=%d rcRT=%Rrc\n", rc, rc < 0 ? RTErrConvertFromErrno(errno) : VERR_TIMEOUT));
    }

    return pUrb;
}


/**
 * Cancels the URB.
 * The URB requires reaping, so we don't change its state.
 */
static void usbProxyFreeBSDUrbCancel(PVUSBURB pUrb)
{
    PUSBPROXYDEV pProxyDev = PDMINS_2_DATA(pUrb->pUsbIns, PUSBPROXYDEV);
    PUSBPROXYDEVFBSD pDevFBSD = (PUSBPROXYDEVFBSD)pProxyDev->Backend.pv;


}


/**
 * The FreeBSD USB Proxy Backend.
 */
extern const USBPROXYBACK g_USBProxyDeviceHost =
{
    "host",
    usbProxyFreeBSDOpen,
    usbProxyFreeBSDInit,
    usbProxyFreeBSDClose,
    usbProxyFreeBSDReset,
    usbProxyFreeBSDSetConfig,
    usbProxyFreeBSDClaimInterface,
    usbProxyFreeBSDReleaseInterface,
    usbProxyFreeBSDSetInterface,
    usbProxyFreeBSDClearHaltedEp,
    usbProxyFreeBSDUrbQueue,
    usbProxyFreeBSDUrbCancel,
    usbProxyFreeBSDUrbReap,
    0
};


/*
 * Local Variables:
 *  mode: c
 *  c-file-style: "bsd"
 *  c-basic-offset: 4
 *  tab-width: 4
 *  indent-tabs-mode: s
 * End:
 */

