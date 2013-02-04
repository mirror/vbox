/* $Id$ */
/** @file
 * USB device proxy - the VRDP backend, calls the RemoteUSBBackend methods.
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

#define LOG_GROUP LOG_GROUP_DRV_USBPROXY

#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/vrdpusb.h>
#include <VBox/vmm/pdm.h>

#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/string.h>

#include "../USBProxyDevice.h"

/**
 * Backend data for the VRDP USB Proxy device backend.
 */
typedef struct USBPROXYDEVVRDP
{
    REMOTEUSBCALLBACK  *pCallback;
    PREMOTEUSBDEVICE    pDevice;
} USBPROXYDEVVRDP, *PUSBPROXYDEVVRDP;


/*
 * The USB proxy device functions.
 */

static int usbProxyVrdpOpen(PUSBPROXYDEV pProxyDev, const char *pszAddress, void *pvBackend)
{
    LogFlow(("usbProxyVrdpOpen: pProxyDev=%p pszAddress=%s, pvBackend=%p\n", pProxyDev, pszAddress, pvBackend));

    int rc = VINF_SUCCESS;

    if (strncmp (pszAddress, REMOTE_USB_BACKEND_PREFIX_S, REMOTE_USB_BACKEND_PREFIX_LEN) == 0)
    {
        REMOTEUSBCALLBACK *pCallback = (REMOTEUSBCALLBACK *)pvBackend;
        PREMOTEUSBDEVICE pDevice = NULL;

        rc = pCallback->pfnOpen (pCallback->pInstance, pszAddress, strlen (pszAddress) + 1, &pDevice);

        if (RT_SUCCESS (rc))
        {
            PUSBPROXYDEVVRDP pDevVrdp = (PUSBPROXYDEVVRDP)RTMemAlloc (sizeof(*pDevVrdp));
            if (pDevVrdp)
            {
                pDevVrdp->pCallback = pCallback;
                pDevVrdp->pDevice = pDevice;
                pProxyDev->Backend.pv = pDevVrdp;
                pProxyDev->iActiveCfg = 1; /** @todo that may not be always true. */
                pProxyDev->cIgnoreSetConfigs = 1;
                return VINF_SUCCESS;
            }

            pCallback->pfnClose (pDevice);
            rc = VERR_NO_MEMORY;
        }
    }
    else
    {
        AssertFailed();
        rc = VERR_INVALID_PARAMETER;
    }

    return rc;
}

static void usbProxyVrdpClose(PUSBPROXYDEV pProxyDev)
{
    LogFlow(("usbProxyVrdpClose: pProxyDev = %p\n", pProxyDev));

    PUSBPROXYDEVVRDP pDevVrdp = (PUSBPROXYDEVVRDP)pProxyDev->Backend.pv;

    pDevVrdp->pCallback->pfnClose (pDevVrdp->pDevice);
}

static int usbProxyVrdpReset(PUSBPROXYDEV pProxyDev, bool fResetOnLinux)
{
    LogFlow(("usbProxyVrdpReset: pProxyDev = %p\n", pProxyDev));

    PUSBPROXYDEVVRDP pDevVrdp = (PUSBPROXYDEVVRDP)pProxyDev->Backend.pv;

    int rc = pDevVrdp->pCallback->pfnReset (pDevVrdp->pDevice);

    if (rc == VERR_VUSB_DEVICE_NOT_ATTACHED)
    {
        Log(("usb-vrdp: remote device %p unplugged!!\n", pDevVrdp->pDevice));
        pProxyDev->fDetached = true;
    }

    pProxyDev->iActiveCfg = -1;
    pProxyDev->cIgnoreSetConfigs = 2;

    return rc;
}

static int usbProxyVrdpSetConfig(PUSBPROXYDEV pProxyDev, int cfg)
{
    LogFlow(("usbProxyVrdpSetConfig: pProxyDev=%s cfg=%#x\n", pProxyDev->pUsbIns->pszName, cfg));

    PUSBPROXYDEVVRDP pDevVrdp = (PUSBPROXYDEVVRDP)pProxyDev->Backend.pv;

    int rc = pDevVrdp->pCallback->pfnSetConfig (pDevVrdp->pDevice, (uint8_t)cfg);

    if (rc == VERR_VUSB_DEVICE_NOT_ATTACHED)
    {
        Log(("usb-vrdp: remote device %p unplugged!!\n", pDevVrdp->pDevice));
        pProxyDev->fDetached = true;
    }

    return RT_SUCCESS(rc);
}

static int usbProxyVrdpClaimInterface(PUSBPROXYDEV pProxyDev, int ifnum)
{
    LogFlow(("usbProxyVrdpClaimInterface: pProxyDev=%s ifnum=%#x\n", pProxyDev->pUsbIns->pszName, ifnum));

    PUSBPROXYDEVVRDP pDevVrdp = (PUSBPROXYDEVVRDP)pProxyDev->Backend.pv;

    int rc = pDevVrdp->pCallback->pfnClaimInterface (pDevVrdp->pDevice, (uint8_t)ifnum);

    if (rc == VERR_VUSB_DEVICE_NOT_ATTACHED)
    {
        Log(("usb-vrdp: remote device %p unplugged!!\n", pDevVrdp->pDevice));
        pProxyDev->fDetached = true;
    }

    return RT_SUCCESS(rc);
}

static int usbProxyVrdpReleaseInterface(PUSBPROXYDEV pProxyDev, int ifnum)
{
    LogFlow(("usbProxyVrdpReleaseInterface: pProxyDev=%s ifnum=%#x\n", pProxyDev->pUsbIns->pszName, ifnum));

    PUSBPROXYDEVVRDP pDevVrdp = (PUSBPROXYDEVVRDP)pProxyDev->Backend.pv;

    int rc = pDevVrdp->pCallback->pfnReleaseInterface (pDevVrdp->pDevice, (uint8_t)ifnum);

    if (rc == VERR_VUSB_DEVICE_NOT_ATTACHED)
    {
        Log(("usb-vrdp: remote device %p unplugged!!\n", pDevVrdp->pDevice));
        pProxyDev->fDetached = true;
    }

    return RT_SUCCESS(rc);
}

static int usbProxyVrdpSetInterface(PUSBPROXYDEV pProxyDev, int ifnum, int setting)
{
    LogFlow(("usbProxyVrdpSetInterface: pProxyDev=%p ifnum=%#x setting=%#x\n", pProxyDev, ifnum, setting));

    PUSBPROXYDEVVRDP pDevVrdp = (PUSBPROXYDEVVRDP)pProxyDev->Backend.pv;

    int rc = pDevVrdp->pCallback->pfnInterfaceSetting (pDevVrdp->pDevice, (uint8_t)ifnum, (uint8_t)setting);

    if (rc == VERR_VUSB_DEVICE_NOT_ATTACHED)
    {
        Log(("usb-vrdp: remote device %p unplugged!!\n", pDevVrdp->pDevice));
        pProxyDev->fDetached = true;
    }

    return RT_SUCCESS(rc);
}

static bool usbProxyVrdpClearHaltedEp(PUSBPROXYDEV pProxyDev, unsigned int ep)
{
    LogFlow(("usbProxyVrdpClearHaltedEp: pProxyDev=%s ep=%u\n", pProxyDev->pUsbIns->pszName, ep));

    PUSBPROXYDEVVRDP pDevVrdp = (PUSBPROXYDEVVRDP)pProxyDev->Backend.pv;

    int rc = pDevVrdp->pCallback->pfnClearHaltedEP (pDevVrdp->pDevice, (uint8_t)ep);

    if (rc == VERR_VUSB_DEVICE_NOT_ATTACHED)
    {
        Log(("usb-vrdp: remote device %p unplugged!!\n", pDevVrdp->pDevice));
        pProxyDev->fDetached = true;
    }

    return RT_SUCCESS(rc);
}

static int usbProxyVrdpUrbQueue(PVUSBURB pUrb)
{
    LogFlow(("usbProxyVrdpUrbQueue: pUrb=%p\n", pUrb));

    /** @todo implement isochronous transfers for USB over VRDP. */
    if (pUrb->enmType == VUSBXFERTYPE_ISOC)
    {
        Log(("usbproxy: isochronous transfers aren't implemented yet.\n"));
        return false;
    }

    PUSBPROXYDEV pProxyDev = PDMINS_2_DATA(pUrb->pUsbIns, PUSBPROXYDEV);
    PUSBPROXYDEVVRDP pDevVrdp = (PUSBPROXYDEVVRDP)pProxyDev->Backend.pv;

    int rc = pDevVrdp->pCallback->pfnQueueURB (pDevVrdp->pDevice, pUrb->enmType, pUrb->EndPt, pUrb->enmDir, pUrb->cbData,
                                               pUrb->abData, pUrb, (PREMOTEUSBQURB *)&pUrb->Dev.pvPrivate);

    if (rc == VERR_VUSB_DEVICE_NOT_ATTACHED)
    {
        Log(("usb-vrdp: remote device %p unplugged!!\n", pDevVrdp->pDevice));
        pProxyDev->fDetached = true;
    }

    return RT_SUCCESS(rc);
}

static PVUSBURB usbProxyVrdpUrbReap(PUSBPROXYDEV pProxyDev, RTMSINTERVAL cMillies)
{
    LogFlow(("usbProxyVrdpUrbReap: pProxyDev=%s\n", pProxyDev->pUsbIns->pszName));

    PUSBPROXYDEVVRDP pDevVrdp = (PUSBPROXYDEVVRDP)pProxyDev->Backend.pv;

    PVUSBURB pUrb = NULL;
    uint32_t cbData = 0;
    uint32_t u32Err = VUSBSTATUS_OK;

    int rc = pDevVrdp->pCallback->pfnReapURB (pDevVrdp->pDevice, cMillies, (void **)&pUrb, &cbData, &u32Err);

    LogFlow(("usbProxyVrdpUrbReap: rc = %Rrc, pUrb = %p\n", rc, pUrb));

    if (RT_SUCCESS(rc) && pUrb)
    {
        pUrb->enmStatus = (VUSBSTATUS)u32Err;
        pUrb->cbData = cbData;
        pUrb->Dev.pvPrivate = NULL;
    }

    if (rc == VERR_VUSB_DEVICE_NOT_ATTACHED)
    {
        Log(("usb-vrdp: remote device %p unplugged!!\n", pDevVrdp->pDevice));
        pProxyDev->fDetached = true;
    }

    return pUrb;
}

static void usbProxyVrdpUrbCancel(PVUSBURB pUrb)
{
    LogFlow(("usbProxyVrdpUrbCancel: pUrb=%p\n", pUrb));

    PUSBPROXYDEV pProxyDev = PDMINS_2_DATA(pUrb->pUsbIns, PUSBPROXYDEV);
    PUSBPROXYDEVVRDP pDevVrdp = (PUSBPROXYDEVVRDP)pProxyDev->Backend.pv;

    pDevVrdp->pCallback->pfnCancelURB (pDevVrdp->pDevice, (PREMOTEUSBQURB)pUrb->Dev.pvPrivate);
}

/**
 * The VRDP USB Proxy Backend operations.
 */
extern const USBPROXYBACK g_USBProxyDeviceVRDP =
{
    "vrdp",
    usbProxyVrdpOpen,
    NULL,
    usbProxyVrdpClose,
    usbProxyVrdpReset,
    usbProxyVrdpSetConfig,
    usbProxyVrdpClaimInterface,
    usbProxyVrdpReleaseInterface,
    usbProxyVrdpSetInterface,
    usbProxyVrdpClearHaltedEp,
    usbProxyVrdpUrbQueue,
    usbProxyVrdpUrbCancel,
    usbProxyVrdpUrbReap,
    0
};

