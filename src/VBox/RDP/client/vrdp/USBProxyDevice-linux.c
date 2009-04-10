/** @file
 *
 * VUSB Device - USB Device Proxy, the Linux backend.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Define NO_PORT_RESET to skip the slow and broken linux port reset.
 * Resetting will break PalmOne. */
#define NO_PORT_RESET
/** Define NO_LOGICAL_RECONNECT to skip the broken logical reconnect handling. */
#define NO_LOGICAL_RECONNECT


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_USBPROXY
#if defined(VBOX) && !defined(RDESKTOP)
# include <iprt/stdint.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
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
#include <ctype.h>
#ifdef VBOX_WITH_LINUX_COMPILER_H
# include <linux/compiler.h>
#endif
#include <linux/usbdevice_fs.h>
/*
 * Backlevel 2.4 headers doesn't have these two defines.
 * They were added some time between 2.4.21 and 2.4.26, probably in 2.4.23.
 */
#ifndef USBDEVFS_DISCONNECT
# define USBDEVFS_DISCONNECT        _IO('U', 22)
# define USBDEVFS_CONNECT           _IO('U', 23)
#endif

#ifndef USBDEVFS_URB_SHORT_NOT_OK
# define USBDEVFS_URB_SHORT_NOT_OK  0 /* rhel3 doesn't have this. darn! */
#endif


/* FedoraCore 4 does not have the bit defined by default. */
#ifndef POLLWRNORM
# define POLLWRNORM 0x0100
#endif

#if defined(VBOX) && !defined(RDESKTOP)
# include <VBox/pdm.h>
# include <VBox/err.h>
# include <VBox/log.h>
# include <iprt/assert.h>
# include <iprt/stream.h>
# include <iprt/alloc.h>
# include <iprt/thread.h>
# include <iprt/time.h>
# include <iprt/asm.h>
# include <iprt/string.h>
# include <iprt/file.h>
# include "../USBProxyDevice.h"
#else

# include "../rdesktop.h"
# include "vrdpusb.h"
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Wrapper around the linux urb request structure.
 * This is required to track in-flight and landed URBs.
 */
typedef struct USBPROXYURBLNX
{
    /** The kernel URB data */
    struct usbdevfs_urb     KUrb;
    /** The millisecond timestamp when this URB was submitted. */
    uint64_t                u64SubmitTS;
    /** Pointer to the next linux URB. */
    struct USBPROXYURBLNX  *pNext;
    /** Pointer to the previous linux URB. */
    struct USBPROXYURBLNX  *pPrev;
    /** If we've split the VUSBURB up into multiple linux URBs, this is points to the head. */
    struct USBPROXYURBLNX  *pSplitHead;
    /** The next linux URB if split up. */
    struct USBPROXYURBLNX  *pSplitNext;
    /** Whether it has timed out and should be shot down on the next failing reap call. */
    bool                    fTimedOut;
    /** Indicates that this URB has been canceled by timeout and should return an CRC error. */
    bool                    fCanceledByTimedOut;
    /** Don't report these back. */
    bool                    fCanceledBySubmit;
    /** This split element is reaped. */
    bool                    fSplitElementReaped;
} USBPROXYURBLNX, *PUSBPROXYURBLNX;

/**
 * Data for the linux usb proxy backend.
 */
typedef struct USBPROXYDEVLNX
{
    /** The open file. */
    RTFILE              File;
    /** Critical section protecting the two lists. */
    RTCRITSECT          CritSect;
    /** The list of free linux URBs. Singly linked. */
    PUSBPROXYURBLNX     pFreeHead;
    /** The list of active linux URBs. Doubly linked.
     * We must maintain this so we can properly reap URBs of a detached device.
     * Only the split head will appear in this list. */
    PUSBPROXYURBLNX     pInFlightHead;
    /** The list of landed linux URBs. Doubly linked.
     * Only the split head will appear in this list. */
    PUSBPROXYURBLNX     pTaxingHead;
    /** The tail of the landed linux URBs. */
    PUSBPROXYURBLNX     pTaxingTail;
} USBPROXYDEVLNX, *PUSBPROXYDEVLNX;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int usbProxyLinuxDoIoCtl(PUSBPROXYDEV pProxyDev, int iCmd, void *pvArg, bool fHandleNoDev, uint32_t cTries);
static void usbProxLinuxUrbUnplugged(PUSBPROXYDEV pProxyDev);
static void usbProxyLinuxSetConnected(PUSBPROXYDEV pProyxDev, int iIf, bool fConnect, bool fQuiet);
static PUSBPROXYURBLNX usbProxyLinuxUrbAlloc(PUSBPROXYDEV pProxyDev, PUSBPROXYURBLNX pSplitHead);
static void usbProxyLinuxUrbFree(PUSBPROXYDEV pProxyDev, PUSBPROXYURBLNX pUrbLnx);
static void usbProxyLinuxUrbFreeSplitList(PUSBPROXYDEV pProxyDev, PUSBPROXYURBLNX pUrbLnx);
static int usbProxyLinuxFindActiveConfig(PUSBPROXYDEV pProxyDev, const char *pszAddress, int *iFirstCfg);



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
 * @param   fHandleNoDev    Whether to handle ENODEV.
 * @param   cTries          The number of retries. Use UINT32_MAX for (kind of) indefinite retries.
 * @internal
 */
static int usbProxyLinuxDoIoCtl(PUSBPROXYDEV pProxyDev, int iCmd, void *pvArg, bool fHandleNoDev, uint32_t cTries)
{
    int rc;
    PUSBPROXYDEVLNX pDevLnx = (PUSBPROXYDEVLNX)pProxyDev->Backend.pv;
    do
    {
        do
        {
            rc = ioctl(pDevLnx->File, iCmd, pvArg);
            if (rc >= 0)
                return rc;
        } while (errno == EINTR);

        if (errno == ENODEV && fHandleNoDev)
        {
            usbProxLinuxUrbUnplugged(pProxyDev);
            Log(("usb-linux: ENODEV -> unplugged. pProxyDev=%p[%s]\n", pProxyDev, pProxyDev->Dev.pszName));
            errno = ENODEV;
            break;
        }
        if (errno != EAGAIN)
            break;
    } while (cTries-- > 0);

    return rc;
}


/**
 * The device has been unplugged.
 * Cancel all in-flight URBs and put them up for reaping.
 */
static void usbProxLinuxUrbUnplugged(PUSBPROXYDEV pProxyDev)
{
    PUSBPROXYDEVLNX pDevLnx = (PUSBPROXYDEVLNX)pProxyDev->Backend.pv;

    /*
     * Shoot down all flying URBs.
     */
    RTCritSectEnter(&pDevLnx->CritSect);

    PUSBPROXYURBLNX pUrbTaxing = NULL;
    PUSBPROXYURBLNX pUrbLnx = pDevLnx->pInFlightHead;
    pDevLnx->pInFlightHead = NULL;
    while (pUrbLnx)
    {
        PUSBPROXYURBLNX pCur = pUrbLnx;
        pUrbLnx = pUrbLnx->pNext;

        ioctl(pDevLnx->File, USBDEVFS_DISCARDURB, &pCur->KUrb); /* not sure if this is required.. */
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
        pUrbTaxing->pPrev = pDevLnx->pTaxingTail;
        if (pUrbTaxing->pPrev)
            pUrbTaxing->pPrev->pNext = pUrbTaxing;
        else
            pDevLnx->pTaxingTail = pDevLnx->pTaxingHead = pUrbTaxing;
    }

    RTCritSectLeave(&pDevLnx->CritSect);

    vusbDevUnplugged(&pProxyDev->Dev);
}


/**
 * Set the connect state seen by kernel drivers
 * @internal
 */
static void usbProxyLinuxSetConnected(PUSBPROXYDEV pProxyDev, int iIf, bool fConnect, bool fQuiet)
{
    struct usbdevfs_ioctl IoCtl;
    if (!fQuiet)
        LogFlow(("usbProxyLinuxSetConnected: pProxyDev=%p[%s] iIf=%#x fConnect=%d\n",
                 pProxyDev, pProxyDev->Dev.pszName, iIf, fConnect));

    IoCtl.ifno = iIf;
    IoCtl.ioctl_code = fConnect ? USBDEVFS_CONNECT : USBDEVFS_DISCONNECT;
    IoCtl.data = NULL;
    if (    usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_IOCTL, &IoCtl, true, UINT32_MAX)
        &&  !fQuiet)
        Log(("usbProxyLinuxSetConnected: failure, errno=%d. pProxyDev=%p[%s]\n",
             errno, pProxyDev, pProxyDev->Dev.pszName));
}


/**
 * Allocates a linux URB request structure.
 * @returns Pointer to an active URB request.
 * @returns NULL on failure.
 * @param   pProxyDev       The proxy device instance.
 * @param   pSplitHead      The split list head if allocating for a split list.
 */
static PUSBPROXYURBLNX usbProxyLinuxUrbAlloc(PUSBPROXYDEV pProxyDev, PUSBPROXYURBLNX pSplitHead)
{
    PUSBPROXYDEVLNX pDevLnx = (PUSBPROXYDEVLNX)pProxyDev->Backend.pv;
    PUSBPROXYURBLNX pUrbLnx;

    RTCritSectEnter(&pDevLnx->CritSect);

    /*
     * Try remove a linux URB from the free list, if none there allocate a new one.
     */
    pUrbLnx = pDevLnx->pFreeHead;
    if (pUrbLnx)
        pDevLnx->pFreeHead = pUrbLnx->pNext;
    else
    {
        RTCritSectLeave(&pDevLnx->CritSect);
        pUrbLnx = (PUSBPROXYURBLNX)RTMemAlloc(sizeof(*pUrbLnx));
        if (!pUrbLnx)
            return NULL;
        RTCritSectEnter(&pDevLnx->CritSect);
    }
    pUrbLnx->pSplitHead = pSplitHead;
    pUrbLnx->pSplitNext = NULL;
    pUrbLnx->fTimedOut = false;
    pUrbLnx->fCanceledByTimedOut = false;
    pUrbLnx->fCanceledBySubmit = false;
    pUrbLnx->fSplitElementReaped = false;

    /*
     * Link it into the active list
     */
    if (!pSplitHead)
    {
        pUrbLnx->pPrev = NULL;
        pUrbLnx->pNext = pDevLnx->pInFlightHead;
        if (pUrbLnx->pNext)
            pUrbLnx->pNext->pPrev = pUrbLnx;
        pDevLnx->pInFlightHead = pUrbLnx;
    }
    else
        pUrbLnx->pPrev = pUrbLnx->pNext = (PUSBPROXYURBLNX)0xdead;

    RTCritSectLeave(&pDevLnx->CritSect);
    return pUrbLnx;
}


/**
 * Frees a linux URB request structure.
 *
 * @param   pProxyDev       The proxy device instance.
 * @param   pUrbLnx         The linux URB to free.
 */
static void usbProxyLinuxUrbFree(PUSBPROXYDEV pProxyDev, PUSBPROXYURBLNX pUrbLnx)
{
    PUSBPROXYDEVLNX pDevLnx = (PUSBPROXYDEVLNX)pProxyDev->Backend.pv;

    RTCritSectEnter(&pDevLnx->CritSect);

    /*
     * Remove from the active list.
     */
    if (    !pUrbLnx->pSplitHead
        ||  pUrbLnx->pSplitHead == pUrbLnx)
    {
        if (pUrbLnx->pNext)
            pUrbLnx->pNext->pPrev = pUrbLnx->pPrev;
        if (pUrbLnx->pPrev)
            pUrbLnx->pPrev->pNext = pUrbLnx->pNext;
        else
            pDevLnx->pInFlightHead  = pUrbLnx->pNext;
    }
    pUrbLnx->pSplitHead = pUrbLnx->pSplitNext = NULL;

    /*
     * Link it into the free list.
     */
    pUrbLnx->pPrev = NULL;
    pUrbLnx->pNext = pDevLnx->pFreeHead;
    pDevLnx->pFreeHead = pUrbLnx;

    RTCritSectLeave(&pDevLnx->CritSect);
}


/**
 * Frees split list of a linux URB request structure.
 *
 * @param   pProxyDev       The proxy device instance.
 * @param   pUrbLnx         A linux URB to in the split list to be freed.
 */
static void usbProxyLinuxUrbFreeSplitList(PUSBPROXYDEV pProxyDev, PUSBPROXYURBLNX pUrbLnx)
{
    PUSBPROXYDEVLNX pDevLnx = (PUSBPROXYDEVLNX)pProxyDev->Backend.pv;

    RTCritSectEnter(&pDevLnx->CritSect);

    pUrbLnx = pUrbLnx->pSplitHead;
    Assert(pUrbLnx);
    while (pUrbLnx)
    {
        PUSBPROXYURBLNX pFree = pUrbLnx;
        pUrbLnx = pUrbLnx->pSplitNext;
        Assert(pFree->pSplitHead);
        usbProxyLinuxUrbFree(pProxyDev, pFree);
    }

    RTCritSectLeave(&pDevLnx->CritSect);
}


/**
 * This finds the device in the /proc/bus/usb/bus/addr file and finds
 * the config with an asterix.
 *
 * @returns The Cfg#.
 * @returns -1 if no active config.
 * @param   pszAddress      The path to the device. We infere the location of the
 *                          devices file, which bus and device number we're looking for.
 * @param   iFirstCfg       The first configuration. (optional)
 * @internal
 */
static int usbProxyLinuxFindActiveConfig(PUSBPROXYDEV pProxyDev, const char *pszAddress, int *piFirstCfg)
{
    /*
     * Set return defaults.
     */
    int iActiveCfg = -1;
    if (piFirstCfg)
        *piFirstCfg = 1;

    /*
     * Interpret the address and turn it into a path to the devices file.
     */
    size_t cchAddress = strlen(pszAddress);
    char *pszDevices = (char *)RTMemDupEx(pszAddress, cchAddress, sizeof("devices"));
    AssertReturn(pszDevices, iActiveCfg);

    /* the device number */
    char *psz = pszDevices + cchAddress;
    while (*psz != '/')
        psz--;
    Assert(pszDevices < psz);
    uint32_t uDev;
    int rc = RTStrToUInt32Ex(psz + 1, NULL, 10, &uDev);
    if (RT_SUCCESS(rc))
    {
        /* the bus number */
        *psz-- = '\0';
        while (*psz != '/')
            psz--;
        Assert(pszDevices < psz);
        uint32_t uBus;
        rc = RTStrToUInt32Ex(psz + 1, NULL, 10, &uBus);
        if (RT_SUCCESS(rc))
        {
            strcpy(psz + 1, "devices");

            /*
             * Open and scan the devices file.
             * We're ASSUMING that each device starts off with a 'T:' line.
             */
            PRTSTREAM pFile;
            rc = RTStrmOpen(pszDevices, "r", &pFile);
            if (RT_SUCCESS(rc))
            {
                char szLine[1024];
                while (RT_SUCCESS(RTStrmGetLine(pFile, szLine, sizeof(szLine))))
                {
                    /* we're only interested in 'T:' lines. */
                    psz = RTStrStripL(szLine);
                    if (psz[0] != 'T' || psz[1] != ':')
                        continue;

                    /* Skip ahead to 'Bus' and compare */
                    psz = RTStrStripL(psz + 2); Assert(!strncmp(psz, "Bus=", 4));
                    psz = RTStrStripL(psz + 4);
                    char *pszNext;
                    uint32_t u;
                    rc = RTStrToUInt32Ex(psz, &pszNext, 10, &u); AssertRC(rc);
                    if (RT_FAILURE(rc))
                        continue;
                    if (u != uBus)
                        continue;

                    /* Skip ahead to 'Dev#' and compare */
                    psz = strstr(psz, "Dev#="); Assert(psz);
                    if (!psz)
                        continue;
                    psz = RTStrStripL(psz + 5);
                    rc = RTStrToUInt32Ex(psz, &pszNext, 10, &u); AssertRC(rc);
                    if (RT_FAILURE(rc))
                        continue;
                    if (u != uDev)
                        continue;

                    /*
                     * Ok, we've found the device.
                     * Scan until we find a selected configuration, the next device, or EOF.
                     */
                    while (RT_SUCCESS(RTStrmGetLine(pFile, szLine, sizeof(szLine))))
                    {
                        psz = RTStrStripL(szLine);
                        if (psz[0] == 'T')
                            break;
                        if (psz[0] != 'C' || psz[1] != ':')
                            continue;
                        const bool fActive = psz[2] == '*';
                        if (!fActive && !piFirstCfg)
                            continue;

                        /* Get the 'Cfg#' value. */
                        psz = strstr(psz, "Cfg#="); Assert(psz);
                        if (psz)
                        {
                            psz = RTStrStripL(psz + 5);
                            rc = RTStrToUInt32Ex(psz, &pszNext, 10, &u); AssertRC(rc);
                            if (RT_SUCCESS(rc))
                            {
                                if (piFirstCfg)
                                {
                                    *piFirstCfg = u;
                                    piFirstCfg = NULL;
                                }
                                if (fActive)
                                    iActiveCfg = u;
                            }
                        }
                        if (fActive)
                            break;
                    }
                    break;
                }
                RTStrmClose(pFile);
            }
        }
    }
    RTMemFree(pszDevices);

    return iActiveCfg;
}

int dev2fd (PUSBPROXYDEV pProxyDev)
{
    if (pProxyDev)
    {
        PUSBPROXYDEVLNX pDevLnx = (PUSBPROXYDEVLNX)pProxyDev->Backend.pv;

	if (pDevLnx)
	{
	    return pDevLnx->File;
	}
    }
    return -1;
}

/**
 * Opens the /proc/bus/usb/bus/addr file.
 *
 * @returns VBox status code.
 * @param   pProxyDev       The device instance.
 * @param   pszAddress      The path to the device.
 * @param   pvBackend       Backend specific pointer, unused for the linux backend.
 */
static int usbProxyLinuxOpen(PUSBPROXYDEV pProxyDev, const char *pszAddress, void *pvBackend)
{
    LogFlow(("usbProxyLinuxOpen: pProxyDev=%p pszAddress=%s\n", pProxyDev, pszAddress));
    RTFILE File;
    int rc = RTFileOpen(&File, pszAddress, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate and initialize the linux backend data.
         */
        PUSBPROXYDEVLNX pDevLnx = (PUSBPROXYDEVLNX)RTMemAllocZ(sizeof(*pDevLnx));
        if (pDevLnx)
        {
            pDevLnx->File = File;
            rc = RTCritSectInit(&pDevLnx->CritSect);
            if (RT_SUCCESS(rc))
            {
                pProxyDev->Backend.pv = pDevLnx;

                /* brute force rulez */
		unsigned iIf;
                for (iIf = 0; iIf < 256; iIf++)
                    usbProxyLinuxSetConnected(pProxyDev, iIf, false, true);

                /*
                 * Determin the active configuration.
                 *
                 * If there isn't any active configuration, we will get EHOSTUNREACH (113) errors
                 * when trying to read the device descriptors in usbProxyDevCreate. So, we'll make
                 * the first one active (usually 1) then.
                 */
                pProxyDev->cIgnoreSetConfigs = 1;
                int iFirstCfg;
                pProxyDev->iActiveCfg = usbProxyLinuxFindActiveConfig(pProxyDev, pszAddress, &iFirstCfg);
                if (pProxyDev->iActiveCfg == -1)
                {
                    usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_SETCONFIGURATION, &iFirstCfg, false, UINT32_MAX);
                    pProxyDev->iActiveCfg = usbProxyLinuxFindActiveConfig(pProxyDev, pszAddress, NULL);
                    Log(("usbProxyLinuxOpen: No active config! Tried to set %d: iActiveCfg=%d\n", iFirstCfg, pProxyDev->iActiveCfg));
                }

                LogFlow(("usbProxyLinuxOpen(%p, %s): returns successfully File=%d iActiveCfg=%d\n",
                         pProxyDev, pszAddress, pDevLnx->File, pProxyDev->iActiveCfg));

                return VINF_SUCCESS;
            }

            RTMemFree(pDevLnx);
        }
        else
            rc = VERR_NO_MEMORY;
        RTFileClose(File);
    }
    else if (rc == VERR_ACCESS_DENIED)
        rc = VERR_VUSB_USBFS_PERMISSION;

    Log(("usbProxyLinuxOpen(%p, %s) failed, rc=%d!\n", pProxyDev, pszAddress, rc));
    pProxyDev->Backend.pv = NULL;

    NOREF(pvBackend);
    return rc;
}


/**
 * Closes the proxy device.
 */
static void usbProxyLinuxClose(PUSBPROXYDEV pProxyDev)
{
    LogFlow(("usbProxyLinuxClose: pProxyDev=%p[%s]\n", pProxyDev, pProxyDev->Dev.pszName));
    PUSBPROXYDEVLNX pDevLnx = (PUSBPROXYDEVLNX)pProxyDev->Backend.pv;
    Assert(pDevLnx);
    if (!pDevLnx)
        return;

    /*
     * Try put the device in a state which linux can cope with before we release it.
     * Resetting it would be a nice start, although we must remember
     * that it might have been disconnected...
     */
    /* ASSUMES: thread == EMT */
    if (!usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_RESET, NULL, false, 10))
    {
        /* Connect drivers. */
	unsigned iIf;
        for (iIf = 0; iIf < 256; iIf++)
            usbProxyLinuxSetConnected(pProxyDev, iIf, true, true);
    }
    else
        Log(("usbProxyLinuxClose: Reset failed, errno=%d.\n", errno));

    /*
     * Now we can close it and free all the resources.
     */
    RTFileClose(pDevLnx->File);
    pDevLnx->File = NIL_RTFILE;

    RTCritSectDelete(&pDevLnx->CritSect);

    PUSBPROXYURBLNX pUrbLnx;
    while ((pUrbLnx = pDevLnx->pInFlightHead) != NULL)
    {
        pDevLnx->pInFlightHead = pUrbLnx->pNext;
        if (    usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_DISCARDURB, &pUrbLnx->KUrb, false, UINT32_MAX)
            &&  errno != ENODEV
            &&  errno != ENOENT)
            AssertMsgFailed(("errno=%d\n", errno));
        if (pUrbLnx->pSplitHead)
        {
            PUSBPROXYURBLNX pCur = pUrbLnx->pSplitNext;
            while (pCur)
            {
                PUSBPROXYURBLNX pFree = pCur;
                pCur = pFree->pSplitNext;
                if (    !pFree->fSplitElementReaped
                    &&  usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_DISCARDURB, &pFree->KUrb, false, UINT32_MAX)
                    &&  errno != ENODEV
                    &&  errno != ENOENT)
                    AssertMsgFailed(("errno=%d\n", errno));
                RTMemFree(pFree);
            }
        }
        else
            Assert(!pUrbLnx->pSplitNext);
        RTMemFree(pUrbLnx);
    }

    while ((pUrbLnx = pDevLnx->pFreeHead) != NULL)
    {
        pDevLnx->pFreeHead = pUrbLnx->pNext;
        RTMemFree(pUrbLnx);
    }

    RTMemFree(pDevLnx);
    pProxyDev->Backend.pv = NULL;
    LogFlow(("usbProxyLinuxClose: returns\n"));
}


#if defined(NO_PORT_RESET) && !defined(NO_LOGICAL_RECONNECT)
/**
 * Look for the logically reconnected device.
 * After 5 seconds we'll give up.
 *
 * @returns VBox status code.
 * @thread  Reset thread or EMT.
 */
static int usb_reset_logical_reconnect(PUSBPROXYDEV pDev)
{
    FILE *          pFile;
    uint64_t        u64StartTS = RTTimeMilliTS();

    Log2(("usb_reset_logical_reconnect: pDev=%p:{.bBus=%#x, .bDevNum=%#x, .idVendor=%#x, .idProduct=%#x, .bcdDevice=%#x, .u64SerialHash=%#llx .bDevNumParent=%#x .bPort=%#x .bLevel=%#x}\n",
          pDev, pDev->Info.bBus, pDev->Info.bDevNum, pDev->Info.idVendor, pDev->Info.idProduct, pDev->Info.bcdDevice,
          pDev->Info.u64SerialHash, pDev->Info.bDevNumParent, pDev->Info.bPort, pDev->Info.bLevel));

    /* First, let hubd get a chance to logically reconnect the device. */
    if (!RTThreadYield())
        RTThreadSleep(1);

    /*
     * Search for the new device address.
     */
    pFile = get_devices_file();
    if (!pFile)
        return VERR_FILE_NOT_FOUND;

    /*
     * Loop until found or 5seconds have elapsed.
     */
    for (;;) {
        struct pollfd   pfd;
        uint8_t     tmp;
        int         rc;
        char        buf[512];
        uint64_t    u64Elapsed;
        int         got = 0;
        struct usb_dev_entry id = {0};

        /*
         * Since this is kernel ABI we don't need to be too fussy about
         * the parsing.
         */
        while (fgets(buf, sizeof(buf), pFile)) {
            char *psz = strchr(buf, '\n');
            if ( psz == NULL ) {
                AssertMsgFailed(("usb_reset_logical_reconnect: Line to long!!\n"));
                break;
            }
            *psz = '\0';

            switch ( buf[0] ) {
            case 'T': /* topology */
                /* Check if we've got enough for a device. */
                if (got >= 2) {
                    Log2(("usb_reset_logical_reconnect: {.bBus=%#x, .bDevNum=%#x, .idVendor=%#x, .idProduct=%#x, .bcdDevice=%#x, .u64SerialHash=%#llx, .bDevNumParent=%#x, .bPort=%#x, .bLevel=%#x}\n",
                          id.bBus, id.bDevNum, id.idVendor, id.idProduct, id.bcdDevice, id.u64SerialHash, id.bDevNumParent, id.bPort, id.bLevel));
                    if (    id.bDevNumParent == pDev->Info.bDevNumParent
                        &&  id.idVendor == pDev->Info.idVendor
                        &&  id.idProduct == pDev->Info.idProduct
                        &&  id.bcdDevice == pDev->Info.bcdDevice
                        &&  id.u64SerialHash == pDev->Info.u64SerialHash
                        &&  id.bBus == pDev->Info.bBus
                        &&  id.bPort == pDev->Info.bPort
                        &&  id.bLevel == pDev->Info.bLevel) {
                        goto l_found;
                    }
                }

                /* restart */
                got = 0;
                memset(&id, 0, sizeof(id));

                /*T:  Bus=04 Lev=02 Prnt=02 Port=00 Cnt=01 Dev#=  3 Spd=1.5 MxCh= 0*/
                Log2(("usb_reset_logical_reconnect: %s\n", buf));
                buf[10] = '\0';
                if ( !get_u8(buf + 8, &id.bBus) )
                    break;
                buf[49] = '\0';
                psz = buf + 46;
                while ( *psz == ' ' )
                    psz++;
                if ( !get_u8(psz, &id.bDevNum) )
                    break;

                buf[17] = '\0';
                if ( !get_u8(buf + 15, &id.bLevel) )
                    break;
                buf[25] = '\0';
                if ( !get_u8(buf + 23, &id.bDevNumParent) )
                    break;
                buf[33] = '\0';
                if ( !get_u8(buf + 31, &id.bPort) )
                    break;
                got++;
                break;

            case 'P': /* product */
                Log2(("usb_reset_logical_reconnect: %s\n", buf));
                buf[15] = '\0';
                if ( !get_x16(buf + 11, &id.idVendor) )
                    break;
                buf[27] = '\0';
                if ( !get_x16(buf + 23, &id.idProduct) )
                    break;
                buf[34] = '\0';
                if ( buf[32] == ' ' )
                    buf[32] = '0';
                id.bcdDevice = 0;
                if ( !get_x8(buf + 32, &tmp) )
                    break;
                id.bcdDevice = tmp << 8;
                if ( !get_x8(buf + 35, &tmp) )
                    break;
                id.bcdDevice |= tmp;
                got++;
                break;

            case 'S': /* String descriptor */
                /* Skip past "S:" and then the whitespace */
                for(psz = buf + 2; *psz != '\0'; psz++)
                    if ( !isspace(*psz) )
                        break;

                /* If it is a serial number string, skip past
                 * "SerialNumber="
                 */
                if ( strncmp(psz, "SerialNumber=", sizeof("SerialNumber=") - 1) )
                    break;

                Log2(("usb_reset_logical_reconnect: %s\n", buf));
                psz += sizeof("SerialNumber=") - 1;

                usb_serial_hash(psz, &id.u64SerialHash);
                break;
            }
        }

        /*
         * Check last.
         */
        if (    got >= 2
            &&  id.bDevNumParent == pDev->Info.bDevNumParent
            &&  id.idVendor == pDev->Info.idVendor
            &&  id.idProduct == pDev->Info.idProduct
            &&  id.bcdDevice == pDev->Info.bcdDevice
            &&  id.u64SerialHash == pDev->Info.u64SerialHash
            &&  id.bBus == pDev->Info.bBus
            &&  id.bPort == pDev->Info.bPort
            &&  id.bLevel == pDev->Info.bLevel) {
        l_found:
            /* close the existing file descriptor. */
            RTFileClose(pDevLnx->File);
            pDevLnx->File = NIL_RTFILE;

            /* open stuff at the new address. */
            pDev->Info = id;
            if (usbProxyLinuxOpen(pDev, &id))
                return VINF_SUCCESS;
            break;
        }

        /*
         * Wait for a while and then check the file again.
         */
        u64Elapsed = RTTimeMilliTS() - u64StartTS;
        if (u64Elapsed >= 5000/*ms*/)
            break; /* done */

        pfd.fd = fileno(pFile);
        pfd.events = POLLIN;
        rc = poll(&pfd, 1, 5000 - u64Elapsed);
        if (rc < 0) {
            AssertMsg(errno == EINTR, ("errno=%d\n", errno));
            RTThreadSleep(32); /* paranoia: don't eat cpu on failure */
        }

        rewind(pFile);
    } /* for loop */

    return VERR_GENERAL_FAILURE;
}
#endif /* !NO_PORT_RESET && !NO_LOGICAL_RECONNECT */


/**
 * Reset a device.
 *
 * @returns VBox status code.
 * @param   pDev    The device to reset.
 */
static int usbProxyLinuxReset(PUSBPROXYDEV pProxyDev)
{
#ifdef NO_PORT_RESET
    LogFlow(("usbProxyLinuxReset: pProxyDev=%p[%s] - NO_PORT_RESET\n", pProxyDev, pProxyDev->Dev.pszName));
    pProxyDev->cIgnoreSetConfigs = 2;

#else /* !NO_PORT_RESET */
    LogFlow(("usbProxyLinuxReset: pProxyDev=%p[%s]\n", pProxyDev, pProxyDev->Dev.pszName));
# ifndef NO_LOGICAL_RECONNECT
    ASMAtomicIncU32(&g_cResetActive);
# endif

    if (usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_RESET, NULL, false, 10))
    {
        int rc = errno;
# ifndef NO_LOGICAL_RECONNECT
        if (rc == ENODEV)
        {
            /*
             * This usually happens because of a 'logical disconnection'.
             * So, we're in for a real treat from our excellent OS now...
             */
            rc2 = usb_reset_logical_reconnect(pProxyDev);
            if (RT_FAILURE(rc2))
                usbProxLinuxUrbUnplugged(pProxyDev);
            if (RT_SUCCESS(rc2))
            {
                ASMAtomicDecU32(&g_cResetActive);
                LogFlow(("usbProxyLinuxReset: returns success (after recovering disconnected device!)\n"));
                return VINF_SUCCESS;
            }
        }
        ASMAtomicDecU32(&g_cResetActive);
# endif /* NO_LOGICAL_RECONNECT */

        Log(("usb-linux: Reset failed, rc=%Rrc errno=%d.\n", RTErrConvertFromErrno(rc), rc));
        pProxyDev->iActiveCfg = -1;
        return RTErrConvertFromErrno(rc);
    }

# ifndef NO_LOGICAL_RECONNECT
    ASMAtomicDecU32(&g_cResetActive);
# endif

    pProxyDev->cIgnoreSetConfigs = 2;
    LogFlow(("usbProxyLinuxReset: returns success\n"));
#endif /* !NO_PORT_RESET */
    return VINF_SUCCESS;
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
static int usbProxyLinuxSetConfig(PUSBPROXYDEV pProxyDev, int iCfg)
{
    LogFlow(("usbProxyLinuxSetConfig: pProxyDev=%p[%s] cfg=%#x\n",
             pProxyDev, pProxyDev->Dev.pszName, iCfg));

    if (usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_SETCONFIGURATION, &iCfg, true, UINT32_MAX))
    {
        Log(("usb-linux: Set configuration. errno=%d\n", errno));
        return false;
    }
    return true;
}


/**
 * Claims an interface.
 * @returns success indicator.
 */
static int usbProxyLinuxClaimInterface(PUSBPROXYDEV pProxyDev, int iIf)
{
    LogFlow(("usbProxyLinuxClaimInterface: pProxyDev=%p[%s] ifnum=%#x\n", pProxyDev, pProxyDev->Dev.pszName, iIf));
    usbProxyLinuxSetConnected(pProxyDev, iIf, false, false);

    if (usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_CLAIMINTERFACE, &iIf, true, UINT32_MAX))
    {
        Log(("usb-linux: Claim interface. errno=%d pProxyDev=%p[%s]\n", errno, pProxyDev, pProxyDev->Dev.pszName));
        return false;
    }
    return true;
}


/**
 * Releases an interface.
 * @returns success indicator.
 */
static int usbProxyLinuxReleaseInterface(PUSBPROXYDEV pProxyDev, int iIf)
{
    LogFlow(("usbProxyLinuxReleaseInterface: pProxyDev=%p[%s] ifnum=%#x\n", pProxyDev, pProxyDev->Dev.pszName, iIf));

    if (usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_RELEASEINTERFACE, &iIf, true, UINT32_MAX))
    {
        Log(("usb-linux: Release interface, errno=%d. pProxyDev=%p[%s]\n", errno, pProxyDev, pProxyDev->Dev.pszName));
        return false;
    }
    return true;
}


/**
 * SET_INTERFACE.
 *
 * @returns success indicator.
 */
static int usbProxyLinuxSetInterface(PUSBPROXYDEV pProxyDev, int iIf, int iAlt)
{
    struct usbdevfs_setinterface SetIf;
    LogFlow(("usbProxyLinuxSetInterface: pProxyDev=%p iIf=%#x iAlt=%#x\n", pProxyDev, iIf, iAlt));

    SetIf.interface  = iIf;
    SetIf.altsetting = iAlt;
    if (usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_SETINTERFACE, &SetIf, true, UINT32_MAX))
    {
        Log(("usb-linux: Set interface, errno=%d. pProxyDev=%p[%s]\n", errno, pProxyDev, pProxyDev->Dev.pszName));
        return false;
    }
    return true;
}


/**
 * Clears the halted endpoint 'EndPt'.
 */
static bool usbProxyLinuxClearHaltedEp(PUSBPROXYDEV pProxyDev, unsigned int EndPt)
{
    LogFlow(("usbProxyLinuxClearHaltedEp: pProxyDev=%p[%s] EndPt=%u\n", pProxyDev, pProxyDev->Dev.pszName, EndPt));

    if (usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_CLEAR_HALT, &EndPt, true, UINT32_MAX))
    {
        /*
         * Unfortunately this doesn't work on control pipes.
         * Windows doing this on the default endpoint and possibly other pipes too,
         * so we'll feign success for ENOENT errors.
         */
        if (errno == ENOENT)
        {
            Log(("usb-linux: clear_halted_ep failed errno=%d. pProxyDev=%p[%s] ep=%d - IGNORED\n",
                 errno, pProxyDev, pProxyDev->Dev.pszName, EndPt));
            return true;
        }
        Log(("usb-linux: clear_halted_ep failed errno=%d. pProxyDev=%p[%s] ep=%d\n",
             errno, pProxyDev, pProxyDev->Dev.pszName, EndPt));
        return false;
    }
    return true;
}


/**
 * Setup packet byte-swapping routines.
 */
static void usbProxyLinuxUrbSwapSetup(PVUSBSETUP pSetup)
{
    pSetup->wValue = RT_LE2H_U16(pSetup->wValue);
    pSetup->wIndex = RT_LE2H_U16(pSetup->wIndex);
    pSetup->wLength = RT_LE2H_U16(pSetup->wLength);
}


/** The split size. */
#define SPLIT_SIZE 0x2000

/**
 * Try split up a VUSB URB into smaller URBs which the
 * linux kernel can deal with.
 *
 * @returns true / false.
 * @param   pProxyDev   The proxy device.
 * @param   pUrbLnx     The linux URB which was rejected because of being too big.
 * @param   pUrb        The VUSB URB.
 */
static int usbProxyLinuxUrbQueueSplit(PUSBPROXYDEV pProxyDev, PUSBPROXYURBLNX pUrbLnx, PVUSBURB pUrb)
{
    unsigned        cTries;
    PUSBPROXYDEVLNX pDevLnx = (PUSBPROXYDEVLNX)pProxyDev->Backend.pv;

    /*
     * Split it up into SPLIT_SIZE sized blocks.
     */
    const unsigned cKUrbs = (pUrb->cbData + SPLIT_SIZE - 1) / SPLIT_SIZE;
    LogFlow(("usbProxyLinuxUrbQueueSplit: pUrb=%p cKUrbs=%d cbData=%d\n", pUrb, cKUrbs, pUrb->cbData));

    uint32_t cbLeft = pUrb->cbData;
    uint8_t *pb = &pUrb->abData[0];

    /* the first one (already allocated) */
    switch (pUrb->enmType)
    {
        default: /* shut up gcc */
        case VUSBXFERTYPE_BULK: pUrbLnx->KUrb.type = USBDEVFS_URB_TYPE_BULK; break;
        case VUSBXFERTYPE_INTR: pUrbLnx->KUrb.type = USBDEVFS_URB_TYPE_INTERRUPT; break;
        case VUSBXFERTYPE_ISOC: pUrbLnx->KUrb.type = USBDEVFS_URB_TYPE_ISO; break;
        case VUSBXFERTYPE_MSG:  pUrbLnx->KUrb.type = USBDEVFS_URB_TYPE_CONTROL; break;
    }
    pUrbLnx->KUrb.endpoint          = pUrb->EndPt;
    if (pUrb->enmDir == VUSBDIRECTION_IN)
        pUrbLnx->KUrb.endpoint |= 0x80;
    pUrbLnx->KUrb.status            = 0;
    pUrbLnx->KUrb.flags             = pUrb->fShortNotOk ? USBDEVFS_URB_SHORT_NOT_OK : 0; /* ISO_ASAP too? */
    pUrbLnx->KUrb.buffer            = pb;
    pUrbLnx->KUrb.buffer_length     = MIN(cbLeft, SPLIT_SIZE);
    pUrbLnx->KUrb.actual_length     = 0;
    pUrbLnx->KUrb.start_frame       = 0;
    pUrbLnx->KUrb.number_of_packets = 0;
    pUrbLnx->KUrb.error_count       = 0;
    pUrbLnx->KUrb.signr             = 0;
    pUrbLnx->KUrb.usercontext       = pUrb;
    pUrbLnx->pSplitHead = pUrbLnx;
    pUrbLnx->pSplitNext = NULL;

    pb += pUrbLnx->KUrb.buffer_length;
    cbLeft -= pUrbLnx->KUrb.buffer_length;

    /* the rest. */
    unsigned i;
    PUSBPROXYURBLNX pCur = pUrbLnx;
    for (i = 1; i < cKUrbs; i++)
    {
        Assert(cbLeft != 0);
        pCur = pCur->pSplitNext = usbProxyLinuxUrbAlloc(pProxyDev, pUrbLnx);
        if (!pCur)
        {
            usbProxyLinuxUrbFreeSplitList(pProxyDev, pUrbLnx);
            return false;
        }
        Assert(pUrbLnx->pNext != pCur); Assert(pUrbLnx->pPrev != pCur); Assert(pCur->pNext == pCur->pPrev);
        Assert(pCur->pSplitHead == pUrbLnx);
        Assert(pCur->pSplitNext == NULL);

        pCur->KUrb = pUrbLnx->KUrb;
        pCur->KUrb.buffer = pb;
        pCur->KUrb.buffer_length = MIN(cbLeft, SPLIT_SIZE);
        pCur->KUrb.actual_length = 0;

        pb += pCur->KUrb.buffer_length;
        cbLeft -= pCur->KUrb.buffer_length;
    }
    Assert(cbLeft == 0);

    /*
     * Submit them.
     */
    bool fUnplugged = false;
    bool fFailed = false;
    pCur = pUrbLnx;
    for (i = 0; i < cKUrbs; i++, pCur = pCur->pSplitNext)
    {
        cTries = 0;
        while (ioctl(pDevLnx->File, USBDEVFS_SUBMITURB, &pCur->KUrb))
        {
            if (errno == EINTR)
                continue;
            if (errno == ENODEV)
            {
                Log(("usbProxyLinuxUrbQueueSplit: ENODEV -> unplugged. pProxyDev=%p[%s]\n", pProxyDev, pProxyDev->Dev.pszName));
                fFailed = fUnplugged = true;
                break;
            }

            Log(("usb-linux: Queue URB %p -> %d!!! type=%d ep=%#x buffer_length=%#x cTries=%d\n",
                 pUrb, errno, pCur->KUrb.type, pCur->KUrb.endpoint, pCur->KUrb.buffer_length, cTries));
            if (errno != EBUSY && ++cTries < 3) /* this doesn't work for the floppy :/ */
                continue;
            fFailed = true;
            break;
        }
        if (fFailed)
            break;
        pCur->u64SubmitTS = RTTimeMilliTS();
    }
    if (!fFailed)
    {
        pUrb->Dev.pvProxyUrb = pUrbLnx;
        LogFlow(("usbProxyLinuxUrbQueueSplit: ok\n"));
        return true;
    }

    /*
     * Clean up.
     */
    if (pUrb->enmType == VUSBXFERTYPE_MSG)
        usbProxyLinuxUrbSwapSetup((PVUSBSETUP)pUrb->abData);

    /* discard and reap later (walking with pUrbLnx). */
    if (pUrbLnx != pCur)
    {
        for (;;)
        {
            pUrbLnx->fCanceledBySubmit = true;
            pUrbLnx->KUrb.usercontext = NULL;
            if (usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_DISCARDURB, &pUrbLnx->KUrb, false, UINT32_MAX))
            {
                if (errno == ENODEV)
                    fUnplugged = true;
                else if (errno == ENOENT)
                    pUrbLnx->fSplitElementReaped = true;
                else
                    LogRel(("SUB: Failed to discard %p! errno=%d (pUrb=%p)\n", pUrbLnx->KUrb.usercontext, errno, pUrb)); /* serious! */
            }
            if (pUrbLnx->pSplitNext == pCur)
            {
                pUrbLnx->pSplitNext = NULL;
                break;
            }
            pUrbLnx = pUrbLnx->pSplitNext; Assert(pUrbLnx);
        }
    }

    /* free the unsubmitted ones. */
    while (pCur)
    {
        PUSBPROXYURBLNX pFree = pCur;
        pCur = pCur->pSplitNext;
        usbProxyLinuxUrbFree(pProxyDev, pFree);
    }

    /* send unplug event if we failed with ENODEV originally. */
    if (fUnplugged)
        usbProxLinuxUrbUnplugged(pProxyDev);
    return false;
}


/**
 * @copydoc USBPROXYBACK::pfbUrbQueue
 */
static int usbProxyLinuxUrbQueue(PVUSBURB pUrb)
{
    unsigned        cTries;
    PUSBPROXYDEV    pProxyDev = (PUSBPROXYDEV)pUrb->pDev;
    PUSBPROXYDEVLNX pDevLnx = (PUSBPROXYDEVLNX)pProxyDev->Backend.pv;
    LogFlow(("usbProxyLinuxUrbQueue: pProxyDev=%p[%s] pUrb=%p EndPt=%d cbData=%d\n",
             pProxyDev, pProxyDev->Dev.pszName, pUrb, pUrb->EndPt, pUrb->cbData));

    /*
     * Allocate a linux urb.
     */
    PUSBPROXYURBLNX pUrbLnx = usbProxyLinuxUrbAlloc(pProxyDev, NULL);
    if (!pUrbLnx)
        return false;

    switch (pUrb->enmType)
    {
        case VUSBXFERTYPE_MSG:
            pUrbLnx->KUrb.type = USBDEVFS_URB_TYPE_CONTROL;
            if (pUrb->cbData < sizeof(VUSBSETUP))
            {
                usbProxyLinuxUrbFree(pProxyDev, pUrbLnx);
                return false;
            }
            usbProxyLinuxUrbSwapSetup((PVUSBSETUP)pUrb->abData);
            LogFlow(("usbProxyLinuxUrbQueue: message\n"));
            break;
        case VUSBXFERTYPE_BULK:
            pUrbLnx->KUrb.type = USBDEVFS_URB_TYPE_BULK;
            break;
        case VUSBXFERTYPE_ISOC:
            pUrbLnx->KUrb.type = USBDEVFS_URB_TYPE_ISO;
            break;
        case VUSBXFERTYPE_INTR:
            pUrbLnx->KUrb.type = USBDEVFS_URB_TYPE_INTERRUPT;
            break;
        default:
            goto l_err;
    }
    pUrbLnx->KUrb.endpoint          = pUrb->EndPt;
    pUrbLnx->KUrb.status            = 0;
    pUrbLnx->KUrb.flags             = pUrb->fShortNotOk ? USBDEVFS_URB_SHORT_NOT_OK : 0; /* ISO_ASAP too? */
    pUrbLnx->KUrb.buffer            = pUrb->abData;
    pUrbLnx->KUrb.buffer_length     = pUrb->cbData;
    pUrbLnx->KUrb.actual_length     = 0;
    pUrbLnx->KUrb.start_frame       = 0;
    pUrbLnx->KUrb.number_of_packets = 0;
    pUrbLnx->KUrb.error_count       = 0;
    pUrbLnx->KUrb.signr             = 0;
    pUrbLnx->KUrb.usercontext       = pUrb;
    switch (pUrb->enmDir)
    {
        case VUSBDIRECTION_IN:
            pUrbLnx->KUrb.endpoint |= 0x80;
            break;
        case VUSBDIRECTION_OUT:
            break;
        default:
            AssertMsgFailed(("usbProxyLinuxUrbQueue: Invalid directorion %d\n", pUrb->enmDir));
            goto l_err;
    }

    /*
     * Submit it.
     */
    cTries = 0;
    while (ioctl(pDevLnx->File, USBDEVFS_SUBMITURB, &pUrbLnx->KUrb))
    {
        if (errno == EINTR)
            continue;
        if (errno == ENODEV)
        {
            Log(("usbProxyLinuxUrbQueue: ENODEV -> unplugged. pProxyDev=%p[%s]\n", pProxyDev, pProxyDev->Dev.pszName));
            if (pUrb->enmType == VUSBXFERTYPE_MSG)
                usbProxyLinuxUrbSwapSetup((PVUSBSETUP)pUrb->abData);
            usbProxyLinuxUrbFree(pProxyDev, pUrbLnx);

            usbProxLinuxUrbUnplugged(pProxyDev);
            return false;
        }

        /*
         * usbfs has or used to have a low buffer limit (16KB) in order to prevent
         * processes wasting kmalloc'ed memory. It will return EINVAL if break that
         * limit, and we'll have to split the VUSB URB up into multiple linux URBs.
         *
         * Since this is a limit which is subject to change, we cannot check for it
         * before submitting the URB. We just have to try and fail.
         */
        if (    errno == EINVAL
            &&  pUrb->cbData >= 8*_1K)
            return usbProxyLinuxUrbQueueSplit(pProxyDev, pUrbLnx, pUrb);

        Log(("usb-linux: Queue URB %p -> %d!!! type=%d ep=%#x buffer_length=%#x cTries=%d\n",
             pUrb, errno, pUrbLnx->KUrb.type, pUrbLnx->KUrb.endpoint, pUrbLnx->KUrb.buffer_length, cTries));
        if (errno != EBUSY && ++cTries < 3) /* this doesn't work for the floppy :/ */
            continue;
l_err:
        if (pUrb->enmType == VUSBXFERTYPE_MSG)
            usbProxyLinuxUrbSwapSetup((PVUSBSETUP)pUrb->abData);
        usbProxyLinuxUrbFree(pProxyDev, pUrbLnx);
        return false;
    }
    pUrbLnx->u64SubmitTS = RTTimeMilliTS();

    LogFlow(("usbProxyLinuxUrbQueue: ok\n"));
    pUrb->Dev.pvProxyUrb = pUrbLnx;
    return true;
}


/**
 * Check if any or the in-flight URBs are taking too long and should be cancelled.
 *
 * Cancelling is done in three turns, first a URB is marked for timeout if it's
 * exceeding a certain time limit. Then the next time it's encountered it is actually
 * cancelled. The idea now is that it's supposed to be reaped and returned in the next
 * round of calls.
 *
 * @param   pProxyDev   The proxy device.
 * @param   pDevLnx     The linux backend data.
 *
 * @todo    Make the HCI do proper timeout handling! Current timeout is 3 min and 20 seconds
 *          as not to break bloomberg which queues IN packages with 3 min timeouts.
 */
static void vusbProxyLinuxUrbDoTimeouts(PUSBPROXYDEV pProxyDev, PUSBPROXYDEVLNX pDevLnx)
{
    RTCritSectEnter(&pDevLnx->CritSect);
    uint64_t u64MilliTS = RTTimeMilliTS();
    PUSBPROXYURBLNX pCur;
    for (pCur = pDevLnx->pInFlightHead;
         pCur;
         pCur = pCur->pNext)
    {
        if (pCur->fTimedOut)
        {
            if (pCur->pSplitHead)
            {
                /* split */
                Assert(pCur == pCur->pSplitHead);
                unsigned cFailures = 0;
		PUSBPROXYURBLNX pCur2;
                for ( pCur2 = pCur; pCur2; pCur2 = pCur2->pSplitNext)
                {
                    if (pCur2->fSplitElementReaped)
                        continue;

                    if (    !usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_DISCARDURB, &pCur2->KUrb, true, UINT32_MAX)
                        ||  errno == ENOENT)
                        pCur2->fCanceledByTimedOut = true;
                    else if (errno != ENODEV)
                        Log(("vusbProxyLinuxUrbDoTimeouts: pUrb=%p failed errno=%d (!!split!!)\n", pCur2->KUrb.usercontext, errno));
                    else
                        goto l_leave; /* ENODEV means break and everything cancelled elsewhere. */
                }
                LogRel(("USB: Cancelled URB (%p) after %lldms!! (cFailures=%d)\n",
                        pCur->KUrb.usercontext, u64MilliTS - pCur->u64SubmitTS, cFailures));
            }
            else
            {
                /* unsplit */
                if (    !usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_DISCARDURB, &pCur->KUrb, true, UINT32_MAX)
                    ||  errno == -ENOENT)
                {
                    pCur->fCanceledByTimedOut = true;
                    LogRel(("USB: Cancelled URB (%p) after %lldms!!\n", pCur->KUrb.usercontext, u64MilliTS - pCur->u64SubmitTS));
                }
                else if (errno != ENODEV)
                    LogFlow(("vusbProxyLinuxUrbDoTimeouts: pUrb=%p failed errno=%d\n", pCur->KUrb.usercontext, errno));
                else
                    goto l_leave; /* ENODEV means break and everything cancelled elsewhere. */
            }
        }
        else if (u64MilliTS - pCur->u64SubmitTS >= 200*1000 /* 200 sec (180 sec has been observed with XP) */)
            pCur->fTimedOut = true;
    }

l_leave:
    RTCritSectLeave(&pDevLnx->CritSect);
}


/**
 * Get and translates the linux status to a VUSB status.
 */
static VUSBSTATUS vusbProxyLinuxUrbGetStatus(PUSBPROXYURBLNX pUrbLnx)
{
    switch (pUrbLnx->KUrb.status)
    {
        case 0:
            if (!pUrbLnx->fCanceledByTimedOut)
                return VUSBSTATUS_OK;
            /* fall thru */

        case -EILSEQ:
            return VUSBSTATUS_CRC;

        case -EREMOTEIO: /* ehci and ohci uses this for underflow error. */
            return VUSBSTATUS_UNDERFLOW;

        case -ENODEV:
            return VUSBSTATUS_DNR;

        //case -ECOMM:
        //    return VUSBSTATUS_BUFFER_OVERRUN;
        //case -ENOSR:
        //    return VUSBSTATUS_BUFFER_UNDERRUN;

        //case -EPROTO:
        //    return VUSBSTATUS_BIT_STUFFING;

        default:
            Log(("usbProxyLinuxUrbReap: pKUrb status %d!!\n", pUrbLnx->KUrb.status));
            return VUSBSTATUS_STALL;
    }
}


/**
 * Reap URBs in-flight on a device.
 *
 * @returns Pointer to a completed URB.
 * @returns NULL if no URB was completed.
 * @param   pProxyDev   The device.
 * @param   cMillies    Number of milliseconds to wait. Use 0 to not wait at all.
 */
static PVUSBURB usbProxyLinuxUrbReap(PUSBPROXYDEV pProxyDev, unsigned cMillies)
{
    PUSBPROXYURBLNX pUrbLnx = NULL;
    PUSBPROXYDEVLNX pDevLnx = (PUSBPROXYDEVLNX)pProxyDev->Backend.pv;

    /*
     * Any URBs pending delivery?
     */
    if (pDevLnx->pTaxingHead)
    {
        RTCritSectEnter(&pDevLnx->CritSect);
        pUrbLnx = pDevLnx->pTaxingHead;
        if (pUrbLnx)
        {
            /* unlink from the pending delivery list */
            if (pUrbLnx->pNext)
            {
                pUrbLnx->pNext->pPrev = NULL;
                pDevLnx->pTaxingHead = pUrbLnx->pNext;
            }
            else
                pDevLnx->pTaxingHead = pDevLnx->pTaxingTail = NULL;

            /* temporarily into the active list, so free works right. */
            pUrbLnx->pPrev = NULL;
            pUrbLnx->pNext = pDevLnx->pInFlightHead;
            if (pUrbLnx->pNext)
                pUrbLnx->pNext->pPrev = pUrbLnx;
            pDevLnx->pInFlightHead = pUrbLnx;
        }
        RTCritSectLeave(&pDevLnx->CritSect);
    }
    if (!pUrbLnx)
    {
        /*
         * Don't block if nothing is in the air.
         */
        if (!pDevLnx->pInFlightHead)
            return NULL;

        /*
         * Block for requested period.
         *
         * It seems to me that the path of poll() is shorter and
         * involves less semaphores than ioctl() on usbfs. So, we'll
         * do a poll regardless of whether cMillies == 0 or not.
         */
        if (cMillies)
        {

            for (;;)
            {
                struct pollfd pfd;
                int rc;

                pfd.fd = pDevLnx->File;
                pfd.events = POLLOUT | POLLWRNORM /* completed async */
                           | POLLERR | POLLHUP    /* disconnected */;
                pfd.revents = 0;
                rc = poll(&pfd, 1, cMillies);
                Log(("usbProxyLinuxUrbReap: poll rc = %d\n", rc));
                if (rc >= 1)
                    break;
                if (rc >= 0 /*|| errno == ETIMEOUT*/)
                {
                    vusbProxyLinuxUrbDoTimeouts(pProxyDev, pDevLnx);
                    return NULL;
                }
                if (errno != EAGAIN)
                {
                    Log(("usb-linux: Reap URB - poll -> %d errno=%d pProxyDev=%p[%s]\n", rc, errno, pProxyDev, pProxyDev->Dev.pszName));
                    return NULL;
                }
                Log(("usbProxyLinuxUrbReap: poll again - weird!!!\n"));
            }
        }

        /*
         * Reap URBs, non-blocking.
         */
        for (;;)
        {
            struct usbdevfs_urb *pKUrb;
            while (ioctl(pDevLnx->File, USBDEVFS_REAPURBNDELAY, &pKUrb))
                if (errno != EINTR)
                {
                    if (errno == ENODEV)
                        usbProxLinuxUrbUnplugged(pProxyDev);
                    else if (errno == EAGAIN)
                        vusbProxyLinuxUrbDoTimeouts(pProxyDev, pDevLnx);
                    else
                        Log(("usb-linux: Reap URB. errno=%d pProxyDev=%p[%s]\n", errno, pProxyDev, pProxyDev->Dev.pszName));
                    return NULL;
                }
            pUrbLnx = (PUSBPROXYURBLNX)pKUrb;

            /* split list: Is the entire split list done yet? */
            if (pUrbLnx->pSplitHead)
            {
                pUrbLnx->fSplitElementReaped = true;
		PUSBPROXYURBLNX pCur;
                for (pCur = pUrbLnx->pSplitHead; pCur; pCur = pCur->pSplitNext)
                    if (!pCur->fSplitElementReaped)
                    {
                        pUrbLnx = NULL;
                        break;
                    }
                if (!pUrbLnx)
                    continue;
                pUrbLnx = pUrbLnx->pSplitHead;
            }
            break;
        }
    }

    /*
     * Ok, we got one!
     */
    PVUSBURB pUrb = (PVUSBURB)pUrbLnx->KUrb.usercontext;
    if (    pUrb
        &&  !pUrbLnx->fCanceledBySubmit)
    {
        if (pUrbLnx->pSplitHead)
        {
            /* split - find the end byte and the first error status. */
            Assert(pUrbLnx == pUrbLnx->pSplitHead);
            uint8_t *pbEnd = &pUrb->abData[0];
            pUrb->enmStatus = VUSBSTATUS_OK;
	    PUSBPROXYURBLNX pCur;
            for (pCur = pUrbLnx; pCur; pCur = pCur->pSplitNext)
            {
                if (pCur->KUrb.actual_length)
                    pbEnd = (uint8_t *)pCur->KUrb.buffer + pCur->KUrb.actual_length;
                if (pUrb->enmStatus == VUSBSTATUS_OK)
                    pUrb->enmStatus = vusbProxyLinuxUrbGetStatus(pCur);
            }
            pUrb->cbData = pbEnd - &pUrb->abData[0];
            usbProxyLinuxUrbFreeSplitList(pProxyDev, pUrbLnx);
        }
        else
        {
            /* unsplit. */
            pUrb->enmStatus = vusbProxyLinuxUrbGetStatus(pUrbLnx);
            pUrb->cbData = pUrbLnx->KUrb.actual_length;
            usbProxyLinuxUrbFree(pProxyDev, pUrbLnx);
        }
        pUrb->Dev.pvProxyUrb = NULL;

        /* some adjustments for message transfers. */
        if (pUrb->enmType == VUSBXFERTYPE_MSG)
        {
            pUrb->cbData += sizeof(VUSBSETUP);
            usbProxyLinuxUrbSwapSetup((PVUSBSETUP)pUrb->abData);
        }
    }
    else
    {
        usbProxyLinuxUrbFree(pProxyDev, pUrbLnx);
        pUrb = NULL;
    }

    LogFlow(("usbProxyLinuxUrbReap: dev=%p[%s] returns %p\n", pProxyDev, pProxyDev->Dev.pszName, pUrb));
    return pUrb;
}


/**
 * Cancels the URB.
 * The URB requires reaping, so we don't change its state.
 */
static void usbProxyLinuxUrbCancel(PVUSBURB pUrb)
{
    PUSBPROXYDEV pProxyDev = (PUSBPROXYDEV)pUrb->pDev;
    PUSBPROXYURBLNX pUrbLnx = (PUSBPROXYURBLNX)pUrb->Dev.pvProxyUrb;
    if (pUrbLnx->pSplitHead)
    {
        /* split */
        Assert(pUrbLnx == pUrbLnx->pSplitHead);
	PUSBPROXYURBLNX pCur;
        for (pCur = pUrbLnx; pCur; pCur = pCur->pSplitNext)
        {
            if (pCur->fSplitElementReaped)
                continue;
            if (    !usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_DISCARDURB, &pCur->KUrb, true, UINT32_MAX)
                ||  errno == ENOENT)
                continue;
            if (errno == ENODEV)
                break;
            Log(("usb-linux: Discard URB %p failed, errno=%d. pProxyDev=%p[%s]!!! (split)\n",
                 pUrb, errno, pProxyDev, pProxyDev->Dev.pszName));
        }
    }
    else
    {
        /* unsplit */
        if (    usbProxyLinuxDoIoCtl(pProxyDev, USBDEVFS_DISCARDURB, &pUrbLnx->KUrb, true, UINT32_MAX)
            &&  errno != ENODEV /* deal with elsewhere. */
            &&  errno != ENOENT)
            Log(("usb-linux: Discard URB %p failed, errno=%d. pProxyDev=%p[%s]!!!\n",
                 pUrb, errno, pProxyDev, pProxyDev->Dev.pszName));
    }
}


/**
 * The Linux USB Proxy Backend.
 */
#ifdef __cplusplus
extern
#endif
const USBPROXYBACK g_USBProxyDeviceHost =
{
    "host",
    usbProxyLinuxOpen,
    usbProxyLinuxClose,
    usbProxyLinuxReset,
    usbProxyLinuxSetConfig,
    usbProxyLinuxClaimInterface,
    usbProxyLinuxReleaseInterface,
    usbProxyLinuxSetInterface,
    usbProxyLinuxClearHaltedEp,
    usbProxyLinuxUrbQueue,
    usbProxyLinuxUrbCancel,
    usbProxyLinuxUrbReap,
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

