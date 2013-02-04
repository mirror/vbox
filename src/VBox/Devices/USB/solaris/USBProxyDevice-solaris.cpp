/* $Id$ */
/** @file
 * USB device proxy - the Solaris backend.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#ifdef DEBUG_ramshankar
# define LOG_INSTANCE       RTLogRelDefaultInstance()
#endif
#include <sys/poll.h>
#include <errno.h>
#include <strings.h>
#include <limits.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/vmm/pdm.h>

#include <iprt/string.h>
#include <iprt/critsect.h>
#include <iprt/time.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include "../USBProxyDevice.h"
#include <VBox/usblib.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Log Prefix. */
#define USBPROXY              "USBProxy"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Wrapper around the solaris urb request structure.
 * This is required to track in-flight and landed URBs.
 */
typedef struct USBPROXYURBSOL
{
    /** Pointer to the Solaris device. */
    struct USBPROXYDEVSOL         *pDevSol;
    /** Pointer to the VUSB URB (set to NULL if canceled). */
    PVUSBURB                       pVUsbUrb;
    /** Pointer to the next solaris URB. */
    struct USBPROXYURBSOL         *pNext;
    /** Pointer to the previous solaris URB. */
    struct USBPROXYURBSOL         *pPrev;
} USBPROXYURBSOL, *PUSBPROXYURBSOL;

/**
 * Data for the solaris usb proxy backend.
 */
typedef struct USBPROXYDEVSOL
{
    /** Path of the USB device in the devices tree (persistent). */
    char                          *pszDevicePath;
    /** The connection to the client driver. */
    RTFILE                         hFile;
    /** Pointer to the proxy device instance. */
    PUSBPROXYDEV                   pProxyDev;
    /** Critical section protecting the two lists. */
    RTCRITSECT                     CritSect;
    /** The list of free solaris URBs. Singly linked. */
    PUSBPROXYURBSOL                pFreeHead;
    /** The list of active solaris URBs. Doubly linked.
     * We must maintain this so we can properly reap URBs of a detached device.
     * Only the split head will appear in this list. */
    PUSBPROXYURBSOL                pInFlightHead;
    /** The list of landed solaris URBs. Doubly linked.
     * Only the split head will appear in this list. */
    PUSBPROXYURBSOL                pTaxingHead;
    /** The tail of the landed solaris URBs. */
    PUSBPROXYURBSOL                pTaxingTail;
} USBPROXYDEVSOL, *PUSBPROXYDEVSOL;

PVUSBURB usbProxySolarisUrbComplete(PUSBPROXYDEVSOL pDevSol);


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
        pUrbSol = (PUSBPROXYURBSOL)RTMemAlloc(sizeof(*pUrbSol));
        if (!pUrbSol)
            return NULL;
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


/*
 * Close the connection to the USB client driver.
 *
 * This is required because our userland enumeration relies on drivers/device trees
 * to recognize active devices, and hence if this device is unplugged we should no
 * longer keep the client driver loaded.
 */
static void usbProxySolarisCloseFile(PUSBPROXYDEVSOL pDevSol)
{
    RTFileClose(pDevSol->hFile);
    pDevSol->hFile = NIL_RTFILE;
}


/**
 * The client driver IOCtl Wrapper function.
 *
 * @returns VBox status code.
 * @param   pDevSol         The Solaris device instance.
 * @param   Function        The Function.
 * @param   pvData          Opaque pointer to the data.
 * @param   cbData          Size of the data pointed to by pvData.
 */
static int usbProxySolarisIOCtl(PUSBPROXYDEVSOL pDevSol, unsigned Function, void *pvData, size_t cbData)
{
    if (RT_UNLIKELY(pDevSol->hFile == NIL_RTFILE))
    {
        LogFlow((USBPROXY ":usbProxySolarisIOCtl connection to driver gone!\n"));
        return VERR_VUSB_DEVICE_NOT_ATTACHED;
    }

    VBOXUSBREQ Req;
    Req.u32Magic = VBOXUSB_MAGIC;
    Req.rc = -1;
    Req.cbData = cbData;
    Req.pvDataR3 = pvData;

    int Ret = -1;
    int rc = RTFileIoCtl(pDevSol->hFile, Function, &Req, sizeof(Req), &Ret);
    if (RT_SUCCESS(rc))
    {
        if (RT_FAILURE(Req.rc))
        {
            if (Req.rc == VERR_VUSB_DEVICE_NOT_ATTACHED)
            {
                pDevSol->pProxyDev->fDetached = true;
                usbProxySolarisCloseFile(pDevSol);
                LogRel((USBPROXY ":Command %#x failed, USB Device '%s' disconnected!\n", Function, pDevSol->pProxyDev->pUsbIns->pszName));
            }
            else
                LogRel((USBPROXY ":Command %#x failed. Req.rc=%Rrc\n", Function, Req.rc));
        }

        return Req.rc;
    }

    LogRel((USBPROXY ":Function %#x failed. rc=%Rrc\n", Function, rc));
    return rc;
}


/**
 * Get the active configuration from the device. The first time this is called
 * our client driver would returned the cached configuration since the device is first plugged in.
 * Subsequent get configuration requests are passed on to the device.
 *
 * @returns VBox status code.
 * @param   pDevSol         The Solaris device instance.
 *
 */
static inline int usbProxySolarisGetActiveConfig(PUSBPROXYDEVSOL pDevSol)
{
    VBOXUSBREQ_GET_CONFIG GetConfigReq;
    bzero(&GetConfigReq, sizeof(GetConfigReq));
    int rc = usbProxySolarisIOCtl(pDevSol, VBOXUSB_IOCTL_GET_CONFIG, &GetConfigReq, sizeof(GetConfigReq));
    if (RT_SUCCESS(rc))
    {
        pDevSol->pProxyDev->iActiveCfg = GetConfigReq.bConfigValue;
        pDevSol->pProxyDev->cIgnoreSetConfigs = 0;
    }
    else
    {
        if (rc != VERR_VUSB_DEVICE_NOT_ATTACHED)
            LogRel((USBPROXY ":Failed to get configuration. rc=%Rrc\n", rc));

        pDevSol->pProxyDev->iActiveCfg = -1;
        pDevSol->pProxyDev->cIgnoreSetConfigs = 0;
    }
    return rc;
}


/**
 * Opens the USB device.
 *
 * @returns VBox status code.
 * @param   pProxyDev       The device instance.
 * @param   pszAddress      The unique device identifier.
 *                          The format of this string is "VendorId:ProducIt:Release:StaticPath".
 * @param   pvBackend       Backend specific pointer, unused for the solaris backend.
 */
static int usbProxySolarisOpen(PUSBPROXYDEV pProxyDev, const char *pszAddress, void *pvBackend)
{
    LogFlowFunc((USBPROXY ":usbProxySolarisOpen pProxyDev=%p pszAddress=%s pvBackend=%p\n", pProxyDev, pszAddress, pvBackend));

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
        if (RT_LIKELY(pDevSol))
        {
            AssertCompile(PATH_MAX >= MAXPATHLEN);
            char szDeviceIdent[PATH_MAX+48];
            rc = RTStrPrintf(szDeviceIdent, sizeof(szDeviceIdent), "%s", pszAddress);
            if (RT_SUCCESS(rc))
            {
                rc = RTCritSectInit(&pDevSol->CritSect);
                if (RT_SUCCESS(rc))
                {
                    pProxyDev->Backend.pv = pDevSol;

                    int Instance;
                    char *pszDevicePath = NULL;
                    rc = USBLibGetClientInfo(szDeviceIdent, &pszDevicePath, &Instance);
                    if (RT_SUCCESS(rc))
                    {
                        pDevSol->pszDevicePath = pszDevicePath;

                        /*
                         * Open the client driver.
                         */
                        RTFILE hFile;
                        rc = RTFileOpen(&hFile, pDevSol->pszDevicePath, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
                        if (RT_SUCCESS(rc))
                        {
                            pDevSol->hFile = hFile;
                            pDevSol->pProxyDev = pProxyDev;

                            /*
                             * Verify client driver version.
                             */
                            VBOXUSBREQ_GET_VERSION GetVersionReq;
                            bzero(&GetVersionReq, sizeof(GetVersionReq));
                            rc = usbProxySolarisIOCtl(pDevSol, VBOXUSB_IOCTL_GET_VERSION, &GetVersionReq, sizeof(GetVersionReq));
                            if (RT_SUCCESS(rc))
                            {
                                if (   GetVersionReq.u32Major == VBOXUSB_VERSION_MAJOR
                                    && GetVersionReq.u32Minor >= VBOXUSB_VERSION_MINOR)
                                {
                                    /*
                                     * Try & get the current cached config from Solaris.
                                     */
                                    usbProxySolarisGetActiveConfig(pDevSol);
                                    return VINF_SUCCESS;
                                }
                                else
                                {
                                    LogRel((USBPROXY ":version mismatch! driver v%d.%d expecting ~v%d.%d\n", GetVersionReq.u32Major,
                                            GetVersionReq.u32Minor, VBOXUSB_VERSION_MAJOR, VBOXUSB_VERSION_MINOR));
                                    rc = VERR_VERSION_MISMATCH;
                                }
                            }
                            else
                            {
                                LogRel((USBPROXY ":failed to query driver version. rc=%Rrc\n", rc));
                            }

                            RTFileClose(pDevSol->hFile);
                            pDevSol->hFile = NIL_RTFILE;
                            pDevSol->pProxyDev = NULL;
                        }
                        else
                            LogRel((USBPROXY ":failed to open device. rc=%Rrc pszDevicePath=%s\n", rc, pDevSol->pszDevicePath));

                        RTStrFree(pDevSol->pszDevicePath);
                        pDevSol->pszDevicePath = NULL;
                    }
                    else
                    {
                        LogRel((USBPROXY ":failed to get client info. rc=%Rrc pszDevicePath=%s\n", rc, pDevSol->pszDevicePath));
                        if (rc == VERR_NOT_FOUND)
                            rc = VERR_OPEN_FAILED;
                    }

                    RTCritSectDelete(&pDevSol->CritSect);
                }
                else
                    LogRel((USBPROXY ":RTCritSectInit failed. rc=%Rrc pszAddress=%s\n", rc, pszAddress));
            }
            else
                LogRel((USBPROXY ":RTStrAPrintf failed. rc=%Rrc pszAddress=%s\n", rc, pszAddress));

            RTMemFree(pDevSol);
            pDevSol = NULL;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        LogRel((USBPROXY ":USBLibInit failed. rc=%Rrc\n", rc));

    USBLibTerm();
    pProxyDev->Backend.pv = NULL;
    return rc;
}


/**
 * Close the USB device.
 *
 * @param   pProxyDev   The device instance.
 */
static void usbProxySolarisClose(PUSBPROXYDEV pProxyDev)
{
    LogFlow((USBPROXY ":usbProxySolarisClose: pProxyDev=%p\n", pProxyDev));

    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;

    /* Close the device (do not re-enumerate). */
    VBOXUSBREQ_CLOSE_DEVICE CloseReq;
    CloseReq.ResetLevel = VBOXUSB_RESET_LEVEL_NONE;
    usbProxySolarisIOCtl(pDevSol, VBOXUSB_IOCTL_CLOSE_DEVICE, &CloseReq, sizeof(CloseReq));

    pProxyDev->fDetached = true;
    usbProxySolarisCloseFile(pDevSol);

    /*
     * Now we can close it and free all the resources.
     */
    RTCritSectDelete(&pDevSol->CritSect);

    PUSBPROXYURBSOL pUrbSol = NULL;
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
 * @param   pProxyDev           The device to reset.
 * @param   fRootHubReset       Is this a root hub reset or device specific reset request.
 */
static int usbProxySolarisReset(PUSBPROXYDEV pProxyDev, bool fRootHubReset)
{
    LogFlowFunc((USBPROXY ":usbProxySolarisReset pProxyDev=%s fRootHubReset=%d\n", pProxyDev->pUsbIns->pszName, fRootHubReset));

    /** Pass all resets to the device. The Trekstor USB (1.1) stick requires this to work. */
    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;

    /* Soft reset the device. */
    VBOXUSBREQ_CLOSE_DEVICE CloseReq;
    CloseReq.ResetLevel = VBOXUSB_RESET_LEVEL_SOFT;
    int rc = usbProxySolarisIOCtl(pDevSol, VBOXUSB_IOCTL_CLOSE_DEVICE, &CloseReq, sizeof(CloseReq));
    if (RT_SUCCESS(rc))
    {
        /* Get the active config. Solaris USBA sets a default config. */
        usbProxySolarisGetActiveConfig(pDevSol);
    }
    else if (rc != VERR_VUSB_DEVICE_NOT_ATTACHED)
        LogRel((USBPROXY ":usbProxySolarisReset failed. rc=%d\n", rc));

    return rc;
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
    LogFlowFunc((USBPROXY ":usbProxySolarisSetConfig: pProxyDev=%p iCfg=%#x\n", pProxyDev, iCfg));

    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;
    AssertPtrReturn(pDevSol, VERR_INVALID_POINTER);

    VBOXUSBREQ_SET_CONFIG SetConfigReq;
    SetConfigReq.bConfigValue = iCfg;
    int rc = usbProxySolarisIOCtl(pDevSol, VBOXUSB_IOCTL_SET_CONFIG, &SetConfigReq, sizeof(SetConfigReq));
    if (RT_SUCCESS(rc))
        return true;

    if (rc != VERR_VUSB_DEVICE_NOT_ATTACHED)
        LogRel((USBPROXY ":usbProxySolarisSetConfig failed to switch configuration. rc=%Rrc\n", rc));

    return false;
}


/**
 * Claims an interface.
 *
 * This is a stub on Solaris since we release/claim all interfaces at
 * as and when required with endpoint opens.
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
 * as and when required with endpoint opens.
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
    LogFlowFunc((USBPROXY ":usbProxySolarisSetInterface: pProxyDev=%p iIf=%d iAlt=%d\n", pProxyDev, iIf, iAlt));

    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;
    AssertPtrReturn(pDevSol, VERR_INVALID_POINTER);

    VBOXUSBREQ_SET_INTERFACE SetInterfaceReq;
    SetInterfaceReq.bInterface = iIf;
    SetInterfaceReq.bAlternate = iAlt;
    int rc = usbProxySolarisIOCtl(pDevSol, VBOXUSB_IOCTL_SET_INTERFACE, &SetInterfaceReq, sizeof(SetInterfaceReq));
    if (RT_SUCCESS(rc))
        return true;

    if (rc != VERR_VUSB_DEVICE_NOT_ATTACHED)
        LogRel((USBPROXY ":usbProxySolarisSetInterface failed to set interface. rc=%Rrc\n", rc));

    return false;
}


/**
 * Clears the halted endpoint 'EndPt'.
 */
static bool usbProxySolarisClearHaltedEp(PUSBPROXYDEV pProxyDev, unsigned int EndPt)
{
    LogFlowFunc((USBPROXY ":usbProxySolarisClearHaltedEp pProxyDev=%p EndPt=%#x\n", pProxyDev, EndPt));

    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;
    AssertPtrReturn(pDevSol, VERR_INVALID_POINTER);

    VBOXUSBREQ_CLEAR_EP ClearEpReq;
    ClearEpReq.bEndpoint = EndPt;
    int rc = usbProxySolarisIOCtl(pDevSol, VBOXUSB_IOCTL_CLEAR_EP, &ClearEpReq, sizeof(ClearEpReq));
    if (RT_SUCCESS(rc))
        return true;

    if (rc != VERR_VUSB_DEVICE_NOT_ATTACHED)
        LogRel((USBPROXY ":usbProxySolarisClearHaltedEp failed! rc=%Rrc\n", rc));

    return false;
}


/**
 * @copydoc USBPROXYBACK::pfnUrbQueue
 */
static int usbProxySolarisUrbQueue(PVUSBURB pUrb)
{
    PUSBPROXYDEV    pProxyDev = PDMINS_2_DATA(pUrb->pUsbIns, PUSBPROXYDEV);
    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;

    LogFlowFunc((USBPROXY ": usbProxySolarisUrbQueue: pProxyDev=%s pUrb=%p EndPt=%#x enmDir=%d cbData=%d pvData=%p\n",
             pProxyDev->pUsbIns->pszName, pUrb, pUrb->EndPt, pUrb->enmDir, pUrb->cbData, pUrb->abData));

    PUSBPROXYURBSOL pUrbSol = usbProxySolarisUrbAlloc(pDevSol);
    if (RT_UNLIKELY(!pUrbSol))
    {
        LogRel((USBPROXY ":usbProxySolarisUrbQueue: Failed to allocate URB.\n"));
        return false;
    }

    pUrbSol->pVUsbUrb = pUrb;
    pUrbSol->pDevSol = pDevSol;

    uint8_t EndPt = pUrb->EndPt;
    if (EndPt)
        EndPt |= pUrb->enmDir == VUSBDIRECTION_IN ? VUSB_DIR_TO_HOST : VUSB_DIR_TO_DEVICE;

    VBOXUSBREQ_URB UrbReq;
    UrbReq.pvUrbR3      = pUrbSol;
    UrbReq.bEndpoint    = EndPt;
    UrbReq.enmType      = pUrb->enmType;
    UrbReq.enmDir       = pUrb->enmDir;
    UrbReq.enmStatus    = pUrb->enmStatus;
    UrbReq.fShortOk     = !pUrb->fShortNotOk;
    UrbReq.cbData       = pUrb->cbData;
    UrbReq.pvData       = pUrb->abData;
    if (pUrb->enmType == VUSBXFERTYPE_ISOC)
    {
        UrbReq.cIsocPkts = pUrb->cIsocPkts;
        for (unsigned i = 0; i < pUrb->cIsocPkts; i++)
        {
            UrbReq.aIsocPkts[i].cbPkt = pUrb->aIsocPkts[i].cb;
            UrbReq.aIsocPkts[i].cbActPkt = 0;
            UrbReq.aIsocPkts[i].enmStatus = VUSBSTATUS_INVALID;
        }
    }

    int rc = usbProxySolarisIOCtl(pDevSol, VBOXUSB_IOCTL_SEND_URB, &UrbReq, sizeof(UrbReq));
    if (RT_SUCCESS(rc))
    {
        if (pUrb->enmType == VUSBXFERTYPE_ISOC)
            LogFlow((USBPROXY ":usbProxySolarisUrbQueue success cbData=%d.\n", pUrb->cbData));
        pUrb->Dev.pvPrivate = pUrbSol;
        return true;
    }

    if (rc != VERR_VUSB_DEVICE_NOT_ATTACHED)
        LogRel((USBPROXY ":usbProxySolarisUrbQueue Failed!! pProxyDev=%s pUrb=%p EndPt=%#x bEndpoint=%#x enmType=%d enmDir=%d cbData=%u rc=%Rrc\n",
             pProxyDev->pUsbIns->pszName, pUrb, pUrb->EndPt, UrbReq.bEndpoint, pUrb->enmType, pUrb->enmDir, pUrb->cbData, rc));

    return false;
}


/**
 * Cancels a URB.
 *
 * The URB requires reaping, so we don't change its state.
 * @remark  There isn't any way to cancel a specific asynchronous request
 *          on Solaris. So we just abort pending URBs on the pipe.
 */
static void usbProxySolarisUrbCancel(PVUSBURB pUrb)
{
    PUSBPROXYURBSOL pUrbSol = (PUSBPROXYURBSOL)pUrb->Dev.pvPrivate;

    PUSBPROXYDEV pProxyDev = PDMINS_2_DATA(pUrb->pUsbIns, PUSBPROXYDEV);
    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;
    AssertPtrReturnVoid(pDevSol);

    LogFlowFunc((USBPROXY ":usbProxySolarisUrbCancel pUrb=%p pUrbSol=%p pDevSol=%p\n", pUrb, pUrbSol, pUrbSol->pDevSol));

    /* Aborting the control pipe isn't supported, pretend success. */
    if (!pUrb->EndPt)
        return;

    VBOXUSBREQ_ABORT_PIPE AbortPipeReq;
    AbortPipeReq.bEndpoint = pUrb->EndPt | (pUrb->enmDir == VUSBDIRECTION_IN ? VUSB_DIR_TO_HOST : VUSB_DIR_TO_DEVICE);
    int rc = usbProxySolarisIOCtl(pDevSol, VBOXUSB_IOCTL_ABORT_PIPE, &AbortPipeReq, sizeof(AbortPipeReq));
    if (RT_FAILURE(rc))
    {
        if (rc != VERR_VUSB_DEVICE_NOT_ATTACHED)
            LogRel((USBPROXY ":usbProxySolarisUrbCancel failed to abort pipe. rc=%Rrc\n", rc));
        return;
    }

    LogFlow((USBPROXY ":usbProxySolarisUrbCancel success.\n", rc));
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
    //LogFlowFunc((USBPROXY ":usbProxySolarisUrbReap pProxyDev=%p cMillies=%u\n", pProxyDev, cMillies));

    PUSBPROXYDEVSOL pDevSol = (PUSBPROXYDEVSOL)pProxyDev->Backend.pv;

    /*
     * Don't block if nothing is in the air.
     */
    if (!pDevSol->pInFlightHead)
        return NULL;

    /*
     * Deque URBs inflight or those landed.
     */
    if (cMillies > 0)
    {
        for (;;)
        {
            struct pollfd pfd;
            pfd.fd = RTFileToNative(pDevSol->hFile);
            pfd.events = POLLIN;
            pfd.revents = 0;
            int rc = poll(&pfd, 1, cMillies);
            if (rc > 0)
            {
                if (pfd.revents & POLLHUP)
                {
                    LogRel((USBPROXY ":Reaping failed, USB Device '%s' disconnected!\n", pDevSol->pProxyDev->pUsbIns->pszName));
                    pProxyDev->fDetached = true;
                    usbProxySolarisCloseFile(pDevSol);
                }
                break;
            }

            if (rc == 0)
            {
                //LogFlow((USBPROXY ":usbProxySolarisUrbReap: Timed out\n"));
                return NULL;
            }
            else if (rc != EAGAIN)
            {
                LogFlow((USBPROXY ":usbProxySolarisUrbReap Poll rc=%d errno=%d\n", rc, errno));
                return NULL;
            }
        }
    }

    usbProxySolarisUrbComplete(pDevSol);

    /*
     * Any URBs pending delivery?
     */
    PVUSBURB pUrb = NULL;
    while (pDevSol->pTaxingHead)
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

    return pUrb;
}


/**
 * Reads a completed/error'd URB from the client driver (no waiting).
 *
 * @param   pDevSol         The Solaris device instance.
 */
PVUSBURB usbProxySolarisUrbComplete(PUSBPROXYDEVSOL pDevSol)
{
    LogFlowFunc((USBPROXY ":usbProxySolarisUrbComplete pDevSol=%p\n", pDevSol));

    VBOXUSBREQ_URB UrbReq;
    bzero(&UrbReq, sizeof(UrbReq));

    int rc = usbProxySolarisIOCtl(pDevSol, VBOXUSB_IOCTL_REAP_URB, &UrbReq, sizeof(UrbReq));
    if (RT_SUCCESS(rc))
    {
        if (UrbReq.pvUrbR3)
        {
            PUSBPROXYURBSOL pUrbSol = (PUSBPROXYURBSOL)UrbReq.pvUrbR3;
            PVUSBURB pUrb           = pUrbSol->pVUsbUrb;
            if (RT_LIKELY(pUrb))
            {
                Assert(pUrb->u32Magic == VUSBURB_MAGIC);

                /*
                 * Update the URB.
                 */
                if (   pUrb->enmType == VUSBXFERTYPE_ISOC
                    && pUrb->enmDir == VUSBDIRECTION_IN)
                {
                    size_t cbData = 0;
                    for (unsigned i = 0; i < UrbReq.cIsocPkts; i++)
                    {
                        pUrb->aIsocPkts[i].cb = UrbReq.aIsocPkts[i].cbActPkt;
                        cbData += UrbReq.aIsocPkts[i].cbActPkt;
                        pUrb->aIsocPkts[i].enmStatus = UrbReq.aIsocPkts[i].enmStatus;
                    }

                    LogFlow((USBPROXY ":usbProxySolarisUrbComplete ISOC cbData=%d cbActPktSum=%d\n", pUrb->cbData, cbData));
                    pUrb->cbData = cbData;
                    pUrb->enmStatus = UrbReq.enmStatus;
                }
                else
                {
                    pUrb->cbData    = UrbReq.cbData;
                    pUrb->enmStatus = UrbReq.enmStatus;
                }

                RTCritSectEnter(&pDevSol->CritSect);

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

                LogFlow((USBPROXY "usbProxySolarisUrbComplete: cb=%d EndPt=%#x enmDir=%d enmStatus=%s (%d) \n",
                         pUrb->cbData, pUrb->EndPt, pUrb->enmDir, pUrb->enmStatus == VUSBSTATUS_OK ? "OK" : "** Failed **", pUrb->enmStatus));
//                if (pUrb->cbData < 2049)
//                    LogFlow((USBPROXY "%.*Rhxd\n", pUrb->cbData, pUrb->abData));
                return pUrb;
            }
        }
    }
    else
    {
        if (rc != VERR_VUSB_DEVICE_NOT_ATTACHED)
            LogRel((USBPROXY ":Reaping URB failed. rc=%Rrc\n", rc));
    }

    return NULL;
}



/**
 * The Solaris USB Proxy Backend.
 */
extern const USBPROXYBACK g_USBProxyDeviceHost =
{
    "host",
    usbProxySolarisOpen,
    NULL,
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

