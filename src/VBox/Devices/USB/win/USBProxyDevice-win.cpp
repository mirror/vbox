/* $Id$ */
/** @file
 * USBPROXY - USB proxy, Win32 backend
 *
 * NOTE: This code assumes only one thread will use it at a time!!
 * bird: usbProxyWinReset() will be called in a separate thread because it
 *       will usually take >=10ms. So, the assumption is broken.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#include <windows.h>

#include <VBox/vmm/pdm.h>
#include <VBox/err.h>
#include <VBox/usb.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/asm.h>
#include "../USBProxyDevice.h"
#include <VBox/usblib.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct _QUEUED_URB
{
    PVUSBURB         urb;

    USBSUP_URB       urbwin;
    OVERLAPPED       overlapped;
    DWORD            cbReturned;
    bool             fCancelled;
} QUEUED_URB, *PQUEUED_URB;

typedef struct
{
    /* Critical section to protect this structure. */
    RTCRITSECT      CritSect;
    HANDLE          hDev;
    uint8_t         bInterfaceNumber;
    bool            fClaimed;
    /** The allocated size of paHandles and paQueuedUrbs. */
    unsigned        cAllocatedUrbs;
    /** The number of URBs in the array. */
    unsigned        cQueuedUrbs;
    /** Array of pointers to the in-flight URB structures. */
    PQUEUED_URB    *paQueuedUrbs;
    /** Array of handles, this is parallel to paQueuedUrbs. */
    PHANDLE         paHandles;
    /** The number of pending URBs. */
    unsigned        cPendingUrbs;
    /** Array of pointers to the pending URB structures. */
    PQUEUED_URB     aPendingUrbs[64];
    /* Thread handle for async io handling. */
    RTTHREAD        hThreadAsyncIo;
    /* Event semaphore for signalling the arrival of a new URB */
    HANDLE          hEventAsyncIo;
    /* Event semaphore for signalling thread termination */
    HANDLE          hEventAsyncTerm;
    /* Set when the async io thread is started. */
    bool            fThreadAsyncIoActive;
} PRIV_USBW32, *PPRIV_USBW32;

/* All functions are returning 1 on success, 0 on error */

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int usbProxyWinSetInterface(PUSBPROXYDEV p, int ifnum, int setting);
static DECLCALLBACK(int) usbProxyWinAsyncIoThread(RTTHREAD ThreadSelf, void *lpParameter);

/**
 * Open a USB device and create a backend instance for it.
 *
 * @returns VBox status code.
 */
static int usbProxyWinOpen(PUSBPROXYDEV pProxyDev, const char *pszAddress, void *pvBackend)
{
    /* Here you just need to use pProxyDev->priv to store whatever per-device
     * data is needed
     */
    /*
     * Allocate private device instance data and use USBPROXYDEV::Backend::pv to point to it.
     */
    PPRIV_USBW32 pPriv = (PPRIV_USBW32)RTMemAllocZ(sizeof(PRIV_USBW32));
    if (!pPriv)
        return VERR_NO_MEMORY;
    pProxyDev->Backend.pv = pPriv;

    int rc = VINF_SUCCESS;
    pPriv->cAllocatedUrbs = 32;
    pPriv->paHandles    = (PHANDLE)RTMemAllocZ(sizeof(pPriv->paHandles[0]) * pPriv->cAllocatedUrbs);
    pPriv->paQueuedUrbs = (PQUEUED_URB *)RTMemAllocZ(sizeof(pPriv->paQueuedUrbs[0]) * pPriv->cAllocatedUrbs);
    if (    pPriv->paQueuedUrbs
        &&  pPriv->paHandles)
    {
        /*
         * Open the device.
         */
        pPriv->hDev = CreateFile(pszAddress,
                                 GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_WRITE | FILE_SHARE_READ,
                                 NULL, // no SECURITY_ATTRIBUTES structure
                                 OPEN_EXISTING, // No special create flags
                                 FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, // overlapped IO
                                 NULL); // No template file
        if (pPriv->hDev != INVALID_HANDLE_VALUE)
        {
            Log(("usbProxyWinOpen: hDev=%p\n", pPriv->hDev));

            /*
             * Check the version
             */
            USBSUP_VERSION  version = {0};
            DWORD           cbReturned = 0;
            if (DeviceIoControl(pPriv->hDev, SUPUSB_IOCTL_GET_VERSION, NULL, 0, &version, sizeof(version), &cbReturned, NULL))
            {
                if (!(    version.u32Major != USBDRV_MAJOR_VERSION
                      ||  version.u32Minor <  USBDRV_MINOR_VERSION))
                {
                    USBSUP_CLAIMDEV in;
                    in.bInterfaceNumber = 0;

                    cbReturned = 0;
                    if (DeviceIoControl(pPriv->hDev, SUPUSB_IOCTL_USB_CLAIM_DEVICE, &in, sizeof(in), &in, sizeof(in), &cbReturned, NULL))
                    {
                        if (in.fClaimed)
                        {
                            pPriv->fClaimed = true;
#if 0 /** @todo this needs to be enabled if windows chooses a default config. Test with the TrekStor GO Stick. */
                            pProxyDev->iActiveCfg = 1;
                            pProxyDev->cIgnoreSetConfigs = 1;
#endif

                            rc = RTCritSectInit(&pPriv->CritSect);
                            AssertRC(rc);
                            pPriv->hEventAsyncIo    = CreateEvent(NULL, FALSE, FALSE, NULL);
                            Assert(pPriv->hEventAsyncIo);
                            pPriv->hEventAsyncTerm  = CreateEvent(NULL, FALSE, FALSE, NULL);
                            Assert(pPriv->hEventAsyncTerm);
                            rc = RTThreadCreate(&pPriv->hThreadAsyncIo, usbProxyWinAsyncIoThread, pPriv, 128 * _1K, RTTHREADTYPE_IO, 0, "USBAsyncIo");
                            Assert(pPriv->hThreadAsyncIo);

                            return VINF_SUCCESS;
                        }

                        rc = VERR_GENERAL_FAILURE;
                        Log(("usbproxy: unable to claim device %x (%s)!!\n", pPriv->hDev, pszAddress));
                    }
                }
                else
                {
                    rc = VERR_VERSION_MISMATCH;
                    Log(("usbproxy: Version mismatch: %d.%d != %d.%d (cur)\n",
                         version.u32Major, version.u32Minor, USBDRV_MAJOR_VERSION, USBDRV_MINOR_VERSION));
                }
            }

            /* Convert last error if necessary */
            if (RT_SUCCESS(rc))
            {
                DWORD dwErr = GetLastError();
                Log(("usbproxy: last error %d\n", dwErr));
                rc = RTErrConvertFromWin32(dwErr);
            }

            CloseHandle(pPriv->hDev);
            pPriv->hDev = INVALID_HANDLE_VALUE;
        }
        else
        {
            Log(("usbproxy: FAILED to open '%s'! last error %d\n", pszAddress, GetLastError()));
            rc = VERR_FILE_NOT_FOUND;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    RTMemFree(pPriv->paQueuedUrbs);
    RTMemFree(pPriv->paHandles);
    RTMemFree(pPriv);
    pProxyDev->Backend.pv = NULL;
    return rc;
}

/**
 * Copy the device and free resources associated with the backend.
 */
static void usbProxyWinClose(PUSBPROXYDEV pProxyDev)
{
    /* Here we just close the device and free up p->priv
     * there is no need to do anything like cancel outstanding requests
     * that will have been done already
     */
    PPRIV_USBW32 pPriv = (PPRIV_USBW32)pProxyDev->Backend.pv;
    Assert(pPriv);
    if (!pPriv)
        return;
    Log(("usbProxyWinClose: %p\n", pPriv->hDev));

    if (pPriv->hDev != INVALID_HANDLE_VALUE)
    {
        Assert(pPriv->fClaimed);

        USBSUP_RELEASEDEV in;
        DWORD cbReturned = 0;
        in.bInterfaceNumber = pPriv->bInterfaceNumber;
        if (!DeviceIoControl(pPriv->hDev, SUPUSB_IOCTL_USB_RELEASE_DEVICE, &in, sizeof(in), NULL, 0, &cbReturned, NULL))
        {
            Log(("usbproxy: usbProxyWinClose: DeviceIoControl %#x failed with %#x!!\n", pPriv->hDev, GetLastError()));
        }
        if (!CloseHandle(pPriv->hDev))
            AssertLogRelMsgFailed(("usbproxy: usbProxyWinClose: CloseHandle %#x failed with %#x!!\n", pPriv->hDev, GetLastError()));
        pPriv->hDev = INVALID_HANDLE_VALUE;
    }

    /* Terminate async thread (which will clean up hEventAsyncTerm) */
    SetEvent(pPriv->hEventAsyncTerm);
    CloseHandle(pPriv->hEventAsyncIo);
    RTCritSectDelete(&pPriv->CritSect);

    RTMemFree(pPriv->paQueuedUrbs);
    RTMemFree(pPriv->paHandles);
    RTMemFree(pPriv);
    pProxyDev->Backend.pv = NULL;
}


static int usbProxyWinReset(PUSBPROXYDEV pProxyDev, bool fResetOnLinux)
{
    PPRIV_USBW32 pPriv = (PPRIV_USBW32)pProxyDev->Backend.pv;
    DWORD cbReturned;
    int  rc;

    Assert(pPriv);

    Log(("usbproxy: Reset %x\n", pPriv->hDev));

    /* Here we just need to assert reset signalling on the USB device */
    cbReturned = 0;
    if (DeviceIoControl(pPriv->hDev, SUPUSB_IOCTL_USB_RESET, NULL, 0, NULL, 0, &cbReturned, NULL))
    {
#if 0 /** @todo this needs to be enabled if windows chooses a default config. Test with the TrekStor GO Stick. */
        pProxyDev->iActiveCfg = 1;
        pProxyDev->cIgnoreSetConfigs = 2;
#else
        pProxyDev->iActiveCfg = -1;
        pProxyDev->cIgnoreSetConfigs = 0;
#endif
        return VINF_SUCCESS;
    }

    rc = GetLastError();
    if (rc == ERROR_DEVICE_REMOVED)
    {
        Log(("usbproxy: device %p unplugged!!\n", pPriv->hDev));
        pProxyDev->fDetached = true;
    }
    return RTErrConvertFromWin32(rc);
}

static int usbProxyWinSetConfig(PUSBPROXYDEV pProxyDev, int cfg)
{
    /* Send a SET_CONFIGURATION command to the device. We don't do this
     * as a normal control message, because the OS might not want to
     * be left out of the loop on such a thing.
     *
     * It would be OK to send a SET_CONFIGURATION control URB at this
     * point but it has to be synchronous.
    */
    PPRIV_USBW32 pPriv = (PPRIV_USBW32)pProxyDev->Backend.pv;
    USBSUP_SET_CONFIG in;
    DWORD cbReturned;

    Assert(pPriv);

    Log(("usbproxy: Set config of %p to %d\n", pPriv->hDev, cfg));
    in.bConfigurationValue = cfg;

    /* Here we just need to assert reset signalling on the USB device */
    cbReturned = 0;
    if (DeviceIoControl(pPriv->hDev, SUPUSB_IOCTL_USB_SET_CONFIG, &in, sizeof(in), NULL, 0, &cbReturned, NULL))
        return 1;

    if (   GetLastError() == ERROR_INVALID_HANDLE_STATE
        || GetLastError() == ERROR_BAD_COMMAND)
    {
        Log(("usbproxy: device %p unplugged!!\n", pPriv->hDev));
        pProxyDev->fDetached = true;
    }
    else
        AssertMsgFailed(("lasterr=%u\n", GetLastError()));

    return 0;
}

static int usbProxyWinClaimInterface(PUSBPROXYDEV p, int ifnum)
{
    /* Called just before we use an interface. Needed on Linux to claim
     * the interface from the OS, since even when proxying the host OS
     * might want to allow other programs to use the unused interfaces.
     * Not relevant for Windows.
     */
    PPRIV_USBW32 pPriv = (PPRIV_USBW32)p->Backend.pv;

    pPriv->bInterfaceNumber = ifnum;

    Assert(pPriv);
    return true;
}

static int usbProxyWinReleaseInterface(PUSBPROXYDEV p, int ifnum)
{
    /* The opposite of claim_interface. */
    PPRIV_USBW32 pPriv = (PPRIV_USBW32)p->Backend.pv;

    Assert(pPriv);
    return true;
}

static int usbProxyWinSetInterface(PUSBPROXYDEV pProxyDev, int ifnum, int setting)
{
    /* Select an alternate setting for an interface, the same applies
     * here as for set_config, you may convert this in to a control
     * message if you want but it must be synchronous
     */
    PPRIV_USBW32 pPriv = (PPRIV_USBW32)pProxyDev->Backend.pv;
    USBSUP_SELECT_INTERFACE in;
    DWORD cbReturned;

    Assert(pPriv);

    Log(("usbproxy: Select interface of %x to %d/%d\n", pPriv->hDev, ifnum, setting));
    in.bInterfaceNumber  = ifnum;
    in.bAlternateSetting = setting;

    /* Here we just need to assert reset signalling on the USB device */
    cbReturned = 0;
    if (DeviceIoControl(pPriv->hDev, SUPUSB_IOCTL_USB_SELECT_INTERFACE, &in, sizeof(in), NULL, 0, &cbReturned, NULL))
        return true;

    if (    GetLastError() == ERROR_INVALID_HANDLE_STATE
        ||  GetLastError() == ERROR_BAD_COMMAND)
    {
        Log(("usbproxy: device %x unplugged!!\n", pPriv->hDev));
        pProxyDev->fDetached = true;
    }
    else
        AssertMsgFailed(("lasterr=%d\n", GetLastError()));
    return 0;
}

/**
 * Clears the halted endpoint 'ep'.
 */
static bool usbProxyWinClearHaltedEndPt(PUSBPROXYDEV pProxyDev, unsigned int ep)
{
    PPRIV_USBW32 pPriv = (PPRIV_USBW32)pProxyDev->Backend.pv;
    USBSUP_CLEAR_ENDPOINT in;
    DWORD cbReturned;

    Assert(pPriv);

    Log(("usbproxy: Clear endpoint %d of %x\n", ep, pPriv->hDev));
    in.bEndpoint = ep;

    cbReturned = 0;
    if (DeviceIoControl(pPriv->hDev, SUPUSB_IOCTL_USB_CLEAR_ENDPOINT, &in, sizeof(in), NULL, 0, &cbReturned, NULL))
        return true;

    if (    GetLastError() == ERROR_INVALID_HANDLE_STATE
        ||  GetLastError() == ERROR_BAD_COMMAND)
    {
        Log(("usbproxy: device %x unplugged!!\n", pPriv->hDev));
        pProxyDev->fDetached = true;
    }
    else
        AssertMsgFailed(("lasterr=%d\n", GetLastError()));
    return 0;
}

/**
 * Aborts a pipe/endpoint (cancels all outstanding URBs on the endpoint).
 */
static int usbProxyWinAbortEndPt(PUSBPROXYDEV pProxyDev, unsigned int ep)
{
    PPRIV_USBW32 pPriv = (PPRIV_USBW32)pProxyDev->Backend.pv;
    USBSUP_CLEAR_ENDPOINT in;
    DWORD cbReturned;
    int  rc;

    Assert(pPriv);

    Log(("usbproxy: Abort endpoint %d of %x\n", ep, pPriv->hDev));
    in.bEndpoint = ep;

    cbReturned = 0;
    if (DeviceIoControl(pPriv->hDev, SUPUSB_IOCTL_USB_ABORT_ENDPOINT, &in, sizeof(in), NULL, 0, &cbReturned, NULL))
        return VINF_SUCCESS;

    rc = GetLastError();
    if (    rc == ERROR_INVALID_HANDLE_STATE
        ||  rc == ERROR_BAD_COMMAND)
    {
        Log(("usbproxy: device %x unplugged!!\n", pPriv->hDev));
        pProxyDev->fDetached = true;
    }
    else
        AssertMsgFailed(("lasterr=%d\n", rc));
    return RTErrConvertFromWin32(rc);
}

/**
 * @copydoc USBPROXYBACK::pfnUrbQueue
 */
static int usbProxyWinUrbQueue(PVUSBURB pUrb)
{
    PUSBPROXYDEV    pProxyDev = PDMINS_2_DATA(pUrb->pUsbIns, PUSBPROXYDEV);
    PPRIV_USBW32    pPriv = (PPRIV_USBW32)pProxyDev->Backend.pv;
    Assert(pPriv);

    /*
     * Allocate and initialize a URB queue structure.
     */
    /** @todo pool these */
    PQUEUED_URB pQUrbWin = (PQUEUED_URB)RTMemAllocZ(sizeof(QUEUED_URB));
    if (!pQUrbWin)
        return false;

    switch (pUrb->enmType)
    {
        case VUSBXFERTYPE_CTRL: pQUrbWin->urbwin.type = USBSUP_TRANSFER_TYPE_CTRL; break; /* you won't ever see these */
        case VUSBXFERTYPE_ISOC: pQUrbWin->urbwin.type = USBSUP_TRANSFER_TYPE_ISOC;
            pQUrbWin->urbwin.numIsoPkts = pUrb->cIsocPkts;
            for (unsigned i = 0; i < pUrb->cIsocPkts; ++i)
            {
                pQUrbWin->urbwin.aIsoPkts[i].cb   = pUrb->aIsocPkts[i].cb;
                pQUrbWin->urbwin.aIsoPkts[i].off  = pUrb->aIsocPkts[i].off;
                pQUrbWin->urbwin.aIsoPkts[i].stat = USBSUP_XFER_OK;
            }
            break;
        case VUSBXFERTYPE_BULK: pQUrbWin->urbwin.type = USBSUP_TRANSFER_TYPE_BULK; break;
        case VUSBXFERTYPE_INTR: pQUrbWin->urbwin.type = USBSUP_TRANSFER_TYPE_INTR; break;
        case VUSBXFERTYPE_MSG:  pQUrbWin->urbwin.type = USBSUP_TRANSFER_TYPE_MSG; break;
        default:
            AssertMsgFailed(("Invalid type %d\n", pUrb->enmType));
            return false;
    }

    switch (pUrb->enmDir)
    {
        case VUSBDIRECTION_SETUP:
            AssertFailed();
            pQUrbWin->urbwin.dir = USBSUP_DIRECTION_SETUP;
            break;
        case VUSBDIRECTION_IN:
            pQUrbWin->urbwin.dir = USBSUP_DIRECTION_IN;
            break;
        case VUSBDIRECTION_OUT:
            pQUrbWin->urbwin.dir = USBSUP_DIRECTION_OUT;
            break;
        default:
            AssertMsgFailed(("Invalid direction %d\n", pUrb->enmDir));
            return false;
    }

    Log(("usbproxy: Queue URB %p ep=%d cbData=%d abData=%p cIsocPkts=%d\n", pUrb, pUrb->EndPt, pUrb->cbData, pUrb->abData, pUrb->cIsocPkts));

    pQUrbWin->urb           = pUrb;
    pQUrbWin->urbwin.ep     = pUrb->EndPt;
    pQUrbWin->urbwin.len    = pUrb->cbData;
    pQUrbWin->urbwin.buf    = pUrb->abData;
    pQUrbWin->urbwin.error  = USBSUP_XFER_OK;
    pQUrbWin->urbwin.flags  = USBSUP_FLAG_NONE;
    if (pUrb->enmDir == VUSBDIRECTION_IN && !pUrb->fShortNotOk)
        pQUrbWin->urbwin.flags = USBSUP_FLAG_SHORT_OK;

    pQUrbWin->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (    pQUrbWin->overlapped.hEvent != INVALID_HANDLE_VALUE
        &&  pPriv->cPendingUrbs < RT_ELEMENTS(pPriv->aPendingUrbs))
    {
        while (!pPriv->fThreadAsyncIoActive)
            RTThreadSleep(1);

        RTCritSectEnter(&pPriv->CritSect);
        do
        {
            /* Ensure we've got sufficient space in the arrays.
             * Do it inside the lock to ensure we do not concur
             * with the usbProxyWinAsyncIoThread */
            if (pPriv->cQueuedUrbs + 1 > pPriv->cAllocatedUrbs)
            {
                unsigned cNewMax = pPriv->cAllocatedUrbs + 32;
                void *pv = RTMemRealloc(pPriv->paHandles, sizeof(pPriv->paHandles[0]) * cNewMax);
                if (!pv)
                {
                    AssertMsgFailed(("RTMemRealloc failed for paHandles[%d]", cNewMax));
                    break;
                }
                pPriv->paHandles = (PHANDLE)pv;

                pv = RTMemRealloc(pPriv->paQueuedUrbs, sizeof(pPriv->paQueuedUrbs[0]) * cNewMax);
                if (!pv)
                {
                    AssertMsgFailed(("RTMemRealloc failed for paQueuedUrbs[%d]", cNewMax));
                    break;
                }
                pPriv->paQueuedUrbs = (PQUEUED_URB *)pv;
                pPriv->cAllocatedUrbs = cNewMax;
            }

            pUrb->Dev.pvPrivate = pQUrbWin;
            pPriv->aPendingUrbs[pPriv->cPendingUrbs] = pQUrbWin;
            pPriv->cPendingUrbs++;
            RTCritSectLeave(&pPriv->CritSect);
            SetEvent(pPriv->hEventAsyncIo);
            return true;
        } while (0);

        RTCritSectLeave(&pPriv->CritSect);
    }
#ifdef DEBUG_misha
    else
    {
        AssertMsgFailed(("FAILED!!, hEvent(0x%p), cPendingUrbs(%d)\n", pQUrbWin->overlapped.hEvent, pPriv->cPendingUrbs));
    }
#endif

    Assert(pQUrbWin->overlapped.hEvent == INVALID_HANDLE_VALUE);
    RTMemFree(pQUrbWin);
    return false;
}

/**
 * Convert Windows proxy URB status to VUSB status.
 *
 * @returns VUSB status constant.
 * @param   win_status  Windows USB proxy status constant.
 */
static VUSBSTATUS usbProxyWinStatusToVUsbStatus(USBSUP_ERROR win_status)
{
    VUSBSTATUS      vusb_status;

    switch (win_status)
    {
        case USBSUP_XFER_OK:        vusb_status = VUSBSTATUS_OK; break;
        case USBSUP_XFER_STALL:     vusb_status = VUSBSTATUS_STALL; break;
        case USBSUP_XFER_DNR:       vusb_status = VUSBSTATUS_DNR; break;
        case USBSUP_XFER_CRC:       vusb_status = VUSBSTATUS_CRC; break;
        case USBSUP_XFER_NAC:       vusb_status = VUSBSTATUS_NOT_ACCESSED; break;
        case USBSUP_XFER_UNDERRUN:  vusb_status = VUSBSTATUS_DATA_UNDERRUN; break;
        case USBSUP_XFER_OVERRUN:   vusb_status = VUSBSTATUS_DATA_OVERRUN; break;
        default:
            AssertMsgFailed(("USB: Invalid error %d\n", win_status));
            vusb_status = VUSBSTATUS_DNR;
            break;
    }
    return vusb_status;
}

/**
 * Reap URBs in-flight on a device.
 *
 * @returns Pointer to a completed URB.
 * @returns NULL if no URB was completed.
 * @param   pProxyDev   The device.
 * @param   cMillies    Number of milliseconds to wait. Use 0 to not
 *                      wait at all.
 */
static PVUSBURB usbProxyWinUrbReap(PUSBPROXYDEV pProxyDev, RTMSINTERVAL cMillies)
{
    PPRIV_USBW32      pPriv = (PPRIV_USBW32)pProxyDev->Backend.pv;
    AssertReturn(pPriv, NULL);

    /*
     * There are some unnecessary calls, just return immediately or
     * WaitForMultipleObjects will fail.
     */
    if (pPriv->cQueuedUrbs <= 0)
    {
        if (cMillies != 0)
        {
            /* Wait for the URBs to be queued if there are some pending */
            while (pPriv->cPendingUrbs)
                RTThreadSleep(1);
            if (pPriv->cQueuedUrbs <= 0)
                return NULL;
        }
        else
            return NULL;
    }

    /*
     * Wait/poll.
     *
     * ASSUMPTIONS:
     *   1. The usbProxyWinUrbReap can not be run concurrently with each other
     *      so racing the cQueuedUrbs access/modification can not occur.
     *   2. The usbProxyWinUrbReap can not be run concurrently with
     *      usbProxyWinUrbQueue so they can not race the pPriv->paHandles
     *      access/realloc.
     */
    unsigned cQueuedUrbs = ASMAtomicReadU32((volatile uint32_t *)&pPriv->cQueuedUrbs);
    PVUSBURB pUrb = NULL;
    DWORD rc = WaitForMultipleObjects(cQueuedUrbs, pPriv->paHandles, FALSE, cMillies);
    if (rc >= WAIT_OBJECT_0 && rc < WAIT_OBJECT_0 + cQueuedUrbs)
    {
        RTCritSectEnter(&pPriv->CritSect);
        unsigned iUrb = rc - WAIT_OBJECT_0;
        PQUEUED_URB pQUrbWin = pPriv->paQueuedUrbs[iUrb];
        pUrb = pQUrbWin->urb;

        /*
         * Remove it from the arrays.
         */
        cQueuedUrbs = --pPriv->cQueuedUrbs;
        if (cQueuedUrbs != iUrb)
        {
            /* Move the array forward */
            for (unsigned i=iUrb;i<cQueuedUrbs;i++)
            {
                pPriv->paHandles[i]    = pPriv->paHandles[i+1];
                pPriv->paQueuedUrbs[i] = pPriv->paQueuedUrbs[i+1];
            }
        }
        pPriv->paHandles[cQueuedUrbs] = INVALID_HANDLE_VALUE;
        pPriv->paQueuedUrbs[cQueuedUrbs] = NULL;
        RTCritSectLeave(&pPriv->CritSect);
        Assert(cQueuedUrbs == pPriv->cQueuedUrbs);

        /*
         * Update the urb.
         */
        pUrb->enmStatus = usbProxyWinStatusToVUsbStatus(pQUrbWin->urbwin.error);
        pUrb->cbData = (uint32_t)pQUrbWin->urbwin.len;
        if (pUrb->enmType == VUSBXFERTYPE_ISOC)
        {
            for (unsigned i = 0; i < pUrb->cIsocPkts; ++i)
            {
                /* NB: Windows won't change the packet offsets, but the packets may
                 * be only partially filled or completely empty.
                 */
                pUrb->aIsocPkts[i].enmStatus = usbProxyWinStatusToVUsbStatus(pQUrbWin->urbwin.aIsoPkts[i].stat);
                pUrb->aIsocPkts[i].cb = pQUrbWin->urbwin.aIsoPkts[i].cb;
            }
        }
        Log(("usbproxy: pUrb=%p (#%d) ep=%d cbData=%d status=%d cIsocPkts=%d ready\n",
             pUrb, rc - WAIT_OBJECT_0, pQUrbWin->urb->EndPt, pQUrbWin->urb->cbData, pUrb->enmStatus, pUrb->cIsocPkts));

        /* free the urb queuing structure */
        if (pQUrbWin->overlapped.hEvent != INVALID_HANDLE_VALUE)
        {
            CloseHandle(pQUrbWin->overlapped.hEvent);
            pQUrbWin->overlapped.hEvent = INVALID_HANDLE_VALUE;
        }
        RTMemFree(pQUrbWin);
    }
    else if (   rc == WAIT_FAILED
             || (rc >= WAIT_ABANDONED_0 && rc < WAIT_ABANDONED_0 + cQueuedUrbs))
        AssertMsgFailed(("USB: WaitForMultipleObjects %d objects failed with rc=%d and last error %d\n", cQueuedUrbs, rc, GetLastError()));

    return pUrb;
}

/* Thread to handle async URB queueing. */
static DECLCALLBACK(int) usbProxyWinAsyncIoThread(RTTHREAD ThreadSelf, void *lpParameter)
{
    PPRIV_USBW32    pPriv = (PPRIV_USBW32)lpParameter;
    HANDLE          hEventWait[2];

    hEventWait[0] = pPriv->hEventAsyncIo;
    hEventWait[1] = pPriv->hEventAsyncTerm;

    Log(("usbProxyWinAsyncIoThread: start\n"));
    pPriv->fThreadAsyncIoActive = true;

    while (true)
    {
        DWORD ret = WaitForMultipleObjects(2, hEventWait, FALSE /* wait for any */, INFINITE);

        if (ret == WAIT_OBJECT_0)
        {
            /*
             * Submit pending URBs.
             */
            RTCritSectEnter(&pPriv->CritSect);

            for (unsigned i = 0; i < pPriv->cPendingUrbs; i++)
            {
                PQUEUED_URB pQUrbWin = pPriv->aPendingUrbs[i];

                Assert(pQUrbWin);
                if (pQUrbWin)
                {
                    if (   DeviceIoControl(pPriv->hDev, SUPUSB_IOCTL_SEND_URB,
                                           &pQUrbWin->urbwin, sizeof(pQUrbWin->urbwin),
                                           &pQUrbWin->urbwin, sizeof(pQUrbWin->urbwin),
                                           &pQUrbWin->cbReturned, &pQUrbWin->overlapped)
                        || GetLastError() == ERROR_IO_PENDING)
                    {
                        /* insert into the queue */
                        unsigned j = pPriv->cQueuedUrbs;
                        pPriv->paQueuedUrbs[j] = pQUrbWin;
                        pPriv->paHandles[j] = pQUrbWin->overlapped.hEvent;
                        /* do an atomic increment to allow usbProxyWinUrbReap thread get it outside a lock,
                         * being sure that pPriv->paHandles contains cQueuedUrbs valid handles */
                        ASMAtomicIncU32((uint32_t volatile *)&pPriv->cQueuedUrbs);
                    }
                    else
                    {
                        DWORD dwErr = GetLastError();
                        if (   dwErr == ERROR_INVALID_HANDLE_STATE
                            || dwErr == ERROR_BAD_COMMAND)
                        {
                            PUSBPROXYDEV pProxyDev = PDMINS_2_DATA(pQUrbWin->urb->pUsbIns, PUSBPROXYDEV);
                            Log(("usbproxy: device %p unplugged!!\n", pPriv->hDev));
                            pProxyDev->fDetached = true;
                        }
                        else
                            AssertMsgFailed(("dwErr=%X urbwin.error=%d (submit urb)\n", dwErr, pQUrbWin->urbwin.error));
                        CloseHandle(pQUrbWin->overlapped.hEvent);
                        pQUrbWin->overlapped.hEvent = INVALID_HANDLE_VALUE;
                    }
                }
                pPriv->aPendingUrbs[i] = 0;
            }
            pPriv->cPendingUrbs = 0;
            RTCritSectLeave(&pPriv->CritSect);
        }
        else
        if (ret == WAIT_OBJECT_0 + 1)
        {
            Log(("usbProxyWinAsyncIoThread: terminating\n"));
            CloseHandle(hEventWait[1]);
            break;
        }
        else
        {
            Log(("usbProxyWinAsyncIoThread: unexpected return code %x\n", ret));
            break;
        }
    }
    return 0;
}

/**
 * Cancels an in-flight URB.
 *
 * The URB requires reaping, so we don't change its state.
 *
 * @remark  There isn't a way to cancel a specific URB on Windows.
 *          on darwin. The interface only supports the aborting of
 *          all URBs pending on an endpoint. Luckily that is usually
 *          exactly what the guest wants to do.
 */
static void usbProxyWinUrbCancel(PVUSBURB pUrb)
{
    PUSBPROXYDEV      pProxyDev = PDMINS_2_DATA(pUrb->pUsbIns, PUSBPROXYDEV);
    PPRIV_USBW32      pPriv     = (PPRIV_USBW32)pProxyDev->Backend.pv;
    PQUEUED_URB       pQUrbWin  = (PQUEUED_URB)pUrb->Dev.pvPrivate;
    int                     rc;
    USBSUP_CLEAR_ENDPOINT   in;
    DWORD                   cbReturned;
    Assert(pQUrbWin);

    in.bEndpoint = pUrb->EndPt | (pUrb->enmDir == VUSBDIRECTION_IN ? 0x80 : 0);
    Log(("Cancel urb %p, endpoint %x\n", pUrb, in.bEndpoint));

    cbReturned = 0;
    if (DeviceIoControl(pPriv->hDev, SUPUSB_IOCTL_USB_ABORT_ENDPOINT, &in, sizeof(in), NULL, 0, &cbReturned, NULL))
        return;

    rc = GetLastError();
    if (    rc == ERROR_INVALID_HANDLE_STATE
        ||  rc == ERROR_BAD_COMMAND)
    {
        Log(("usbproxy: device %x unplugged!!\n", pPriv->hDev));
        pProxyDev->fDetached = true;
    }
    else
        AssertMsgFailed(("lasterr=%d\n", rc));
}


/**
 * The Win32 USB Proxy Backend.
 */
extern const USBPROXYBACK g_USBProxyDeviceHost =
{
    "host",
    usbProxyWinOpen,
    NULL,
    usbProxyWinClose,
    usbProxyWinReset,
    usbProxyWinSetConfig,
    usbProxyWinClaimInterface,
    usbProxyWinReleaseInterface,
    usbProxyWinSetInterface,
    usbProxyWinClearHaltedEndPt,
    usbProxyWinUrbQueue,
    usbProxyWinUrbCancel,
    usbProxyWinUrbReap,
    0
};

