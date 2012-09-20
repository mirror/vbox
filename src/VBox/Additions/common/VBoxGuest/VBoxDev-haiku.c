/* $Id$ */
/** @file
 * VBoxGuest kernel driver, Haiku Guest Additions, implementation.
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

/*
 * This code is based on:
 *
 * VirtualBox Guest Additions for Haiku.
 * Copyright (c) 2011 Mike Smith <mike@scgtrp.net>
 *                    François Revol <revol@free.fr>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


#define LOG_GROUP LOG_GROUP_SUP_DRV
//#undef LOG_DISABLED
//#define LOG_ENABLED
//#define LOG_ENABLE_FLOW
//#define DO_LOG
#include <sys/param.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <OS.h>
#include <Drivers.h>
#include <KernelExport.h>
#include <PCI.h>

#include "VBoxGuest-haiku.h"
#include "VBoxGuestInternal.h"
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <iprt/mem.h>
#include <iprt/asm.h>

#define DRIVER_NAME "vboxdev"
#define DEVICE_NAME "misc/vboxguest"
#define MODULE_NAME "generic/vboxguest"

static status_t VBoxGuestHaikuOpen(const char *name, uint32 flags, void **cookie);
static status_t VBoxGuestHaikuClose(void *cookie);
static status_t VBoxGuestHaikuFree(void *cookie);
static status_t VBoxGuestHaikuIOCtl(void *cookie, uint32 op, void *data, size_t len);
static status_t VBoxGuestHaikuSelect(void *cookie, uint8 event, uint32 ref, selectsync *sync);
static status_t VBoxGuestHaikuDeselect(void *cookie, uint8 event, selectsync *sync);
static status_t VBoxGuestHaikuWrite(void *cookie, off_t position, const void *data, size_t *numBytes);
static status_t VBoxGuestHaikuRead(void *cookie, off_t position, void *data, size_t *numBytes);

static device_hooks g_VBoxGuestHaikuDeviceHooks =
{
    VBoxGuestHaikuOpen,
    VBoxGuestHaikuClose,
    VBoxGuestHaikuFree,
    VBoxGuestHaikuIOCtl,
    VBoxGuestHaikuRead,
    VBoxGuestHaikuWrite,
    VBoxGuestHaikuSelect,
    VBoxGuestHaikuDeselect,
};


/**
 * File open handler
 *
 */
static status_t VBoxGuestHaikuOpen(const char *name, uint32 flags, void **cookie)
{
    int rc;
    PVBOXGUESTSESSION pSession;

    LogFlow((DRIVER_NAME ":VBoxGuestHaikuOpen\n"));

    /*
     * Create a new session.
     */
    rc = VBoxGuestCreateUserSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        Log((DRIVER_NAME ":VBoxGuestHaikuOpen success: g_DevExt=%p pSession=%p rc=%d pid=%d\n",&g_DevExt, pSession, rc,(int)RTProcSelf()));
        ASMAtomicIncU32(&cUsers);
        *cookie = pSession;
        return 0;
    }

    LogRel((DRIVER_NAME ":VBoxGuestHaikuOpen: failed. rc=%d\n", rc));
    return RTErrConvertToErrno(rc);
}


/**
 * File close handler
 *
 */
static status_t VBoxGuestHaikuClose(void *cookie)
{
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)cookie;
    Log(("VBoxGuestHaikuClose: pSession=%p\n", pSession));

    /** @todo r=ramshankar: should we really be using the session spinlock here? */
    RTSpinlockAcquire(g_DevExt.SessionSpinlock);

    //XXX: we don't know if it belongs to this session !
    if (sState.selectSync)
    {
        //dprintf(DRIVER_NAME "close: unblocking select %p %x\n", sState.selectSync, sState.selectEvent);
        notify_select_event(sState.selectSync, sState.selectEvent);
        sState.selectEvent = (uint8_t)0;
        sState.selectRef = (uint32_t)0;
        sState.selectSync = (void *)NULL;
    }

    RTSpinlockRelease(g_DevExt.SessionSpinlock);

    return 0;
}


/**
 * File free handler
 *
 */
static status_t VBoxGuestHaikuFree(void *cookie)
{
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)cookie;
    Log(("VBoxGuestHaikuFree: pSession=%p\n", pSession));

    /*
     * Close the session if it's still hanging on to the device...
     */
    if (VALID_PTR(pSession))
    {
        VBoxGuestCloseSession(&g_DevExt, pSession);
        ASMAtomicDecU32(&cUsers);
    }
    else
        Log(("VBoxGuestHaikuFree: si_drv1=%p!\n", pSession));
    return 0;
}


/**
 * IOCTL handler
 *
 */
static status_t VBoxGuestHaikuIOCtl(void *cookie, uint32 op, void *data, size_t len)
{
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)cookie;
    //Log(("VBoxGuestHaikuFree: pSession=%p\n", pSession));
    //LogFlow((DRIVER_NAME ":VBoxGuestHaikuIOCtl(, 0x%08x, %p, %d)\n", op, data, len));
    Log((DRIVER_NAME ":VBoxGuestHaikuIOCtl(, 0x%08x, %p, %d)\n", op, data, len));

    int rc = 0;

    /*
     * Validate the input.
     */
    if (RT_UNLIKELY(!VALID_PTR(pSession)))
        return EINVAL;

    /*
     * Validate the request wrapper.
     */
#if 0
    if (IOCPARM_LEN(ulCmd) != sizeof(VBGLBIGREQ))
    {
        Log((DRIVER_NAME ": VBoxGuestHaikuIOCtl: bad request %lu size=%lu expected=%d\n", ulCmd, IOCPARM_LEN(ulCmd), sizeof(VBGLBIGREQ)));
        return ENOTTY;
    }
#endif

    if (RT_UNLIKELY(len > _1M * 16))
    {
        dprintf(DRIVER_NAME ": VBoxGuestHaikuIOCtl: bad size %#x; pArg=%p Cmd=%lu.\n", (unsigned)len, data, op);
        return EINVAL;
    }

    /*
     * Read the request.
     */
    void *pvBuf = NULL;
    if (RT_LIKELY(len > 0))
    {
        pvBuf = RTMemTmpAlloc(len);
        if (RT_UNLIKELY(!pvBuf))
        {
            LogRel((DRIVER_NAME ":VBoxGuestHaikuIOCtl: RTMemTmpAlloc failed to alloc %d bytes.\n", len));
            return ENOMEM;
        }

        rc = user_memcpy(pvBuf, data, len);
        if (RT_UNLIKELY(rc < 0))
        {
            RTMemTmpFree(pvBuf);
            LogRel((DRIVER_NAME ":VBoxGuestHaikuIOCtl: user_memcpy failed; pvBuf=%p data=%p op=%d. rc=%d\n", pvBuf, data, op, rc));
            return EFAULT;
        }
        if (RT_UNLIKELY(!VALID_PTR(pvBuf)))
        {
            RTMemTmpFree(pvBuf);
            LogRel((DRIVER_NAME ":VBoxGuestHaikuIOCtl: pvBuf invalid pointer %p\n", pvBuf));
            return EINVAL;
        }
    }
    Log((DRIVER_NAME ":VBoxGuestHaikuIOCtl: pSession=%p pid=%d.\n", pSession,(int)RTProcSelf()));

    /*
     * Process the IOCtl.
     */
    size_t cbDataReturned;
    rc = VBoxGuestCommonIOCtl(op, &g_DevExt, pSession, pvBuf, len, &cbDataReturned);
    if (RT_SUCCESS(rc))
    {
        rc = 0;
        if (RT_UNLIKELY(cbDataReturned > len))
        {
            Log((DRIVER_NAME ":VBoxGuestHaikuIOCtl: too much output data %d expected %d\n", cbDataReturned, len));
            cbDataReturned = len;
        }
        if (cbDataReturned > 0)
        {
            rc = user_memcpy(data, pvBuf, cbDataReturned);
            if (RT_UNLIKELY(rc < 0))
            {
                Log((DRIVER_NAME ":VBoxGuestHaikuIOCtl: user_memcpy failed; pvBuf=%p pArg=%p Cmd=%lu. rc=%d\n", pvBuf, data, op, rc));
                rc = EFAULT;
            }
        }
    }
    else
    {
        Log((DRIVER_NAME ":VBoxGuestHaikuIOCtl: VBoxGuestCommonIOCtl failed. rc=%d\n", rc));
        rc = EFAULT;
    }
    RTMemTmpFree(pvBuf);
    return rc;
#if 0
#endif
}


static status_t VBoxGuestHaikuSelect(void *cookie, uint8 event, uint32 ref, selectsync *sync)
{
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)cookie;
    status_t err = B_OK;
    //dprintf(DRIVER_NAME "select(,%d,%p)\n", event, sync);


    switch (event)
    {
        case B_SELECT_READ:
            //case B_SELECT_ERROR:
            break;
        default:
            return EINVAL;
    }

    RTSpinlockAcquire(g_DevExt.SessionSpinlock);

    uint32_t u32CurSeq = ASMAtomicUoReadU32(&g_DevExt.u32MousePosChangedSeq);
    if (pSession->u32MousePosChangedSeq != u32CurSeq)
    {
        //dprintf(DRIVER_NAME "select: notifying now: %p %x\n", sync, event);
        pSession->u32MousePosChangedSeq = u32CurSeq;
        notify_select_event(sync, event);
    }
    else if (sState.selectSync == NULL)
    {
        //dprintf(DRIVER_NAME "select: caching: %p %x\n", sync, event);
        sState.selectEvent = (uint8_t)event;
        sState.selectRef = (uint32_t)ref;
        sState.selectSync = (void *)sync;
    }
    else
    {
        //dprintf(DRIVER_NAME "select: dropping: %p %x\n", sync, event);
        err = B_WOULD_BLOCK;
    }

    RTSpinlockRelease(g_DevExt.SessionSpinlock);

    return err;
#if 0
    int fEventsProcessed;

    LogFlow((DRIVER_NAME "::Poll: fEvents=%d\n", fEvents));

    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pDev->si_drv1;
    if (RT_UNLIKELY(!VALID_PTR(pSession)))
    {
        Log((DRIVER_NAME "::Poll: no state data for %s\n", devtoname(pDev)));
        return (fEvents & (POLLHUP|POLLIN|POLLRDNORM|POLLOUT|POLLWRNORM));
    }

    uint32_t u32CurSeq = ASMAtomicUoReadU32(&g_DevExt.u32MousePosChangedSeq);
    if (pSession->u32MousePosChangedSeq != u32CurSeq)
    {
        fEventsProcessed = fEvents & (POLLIN | POLLRDNORM);
        pSession->u32MousePosChangedSeq = u32CurSeq;
    }
    else
    {
        fEventsProcessed = 0;

        selrecord(td, &g_SelInfo);
    }

    return fEventsProcessed;
#endif
}


static status_t VBoxGuestHaikuDeselect(void *cookie, uint8 event, selectsync *sync)
{
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)cookie;
    status_t err = B_OK;
    //dprintf(DRIVER_NAME "deselect(,%d,%p)\n", event, sync);

    RTSpinlockAcquire(g_DevExt.SessionSpinlock);

    if (sState.selectSync == sync)
    {
        //dprintf(DRIVER_NAME "deselect: dropping: %p %x\n", sState.selectSync, sState.selectEvent);
        sState.selectEvent = (uint8_t)0;
        sState.selectRef = (uint32_t)0;
        sState.selectSync = NULL;
    }
    else
        err = B_OK;

    RTSpinlockRelease(g_DevExt.SessionSpinlock);
    return err;
}


static status_t VBoxGuestHaikuWrite(void *cookie, off_t position, const void *data, size_t *numBytes)
{
    *numBytes = 0;
    return 0;
}


static status_t VBoxGuestHaikuRead(void *cookie, off_t position, void *data, size_t *numBytes)
{
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)cookie;

    //dprintf(DRIVER_NAME "read(,,%d)\n", *numBytes);

    if (*numBytes == 0)
        return 0;

    uint32_t u32CurSeq = ASMAtomicUoReadU32(&g_DevExt.u32MousePosChangedSeq);
    if (pSession->u32MousePosChangedSeq != u32CurSeq)
    {
        pSession->u32MousePosChangedSeq = u32CurSeq;
        //dprintf(DRIVER_NAME "read: giving 1 byte\n");
        *numBytes = 1;
        return 0;
    }

    *numBytes = 0;
    return 0;
}


int32 api_version = B_CUR_DRIVER_API_VERSION;

status_t init_hardware()
{
    return get_module(MODULE_NAME, (module_info **)&g_VBoxGuest);
}

status_t init_driver()
{
    return B_OK;
}

device_hooks* find_device(const char *name)
{
    static device_hooks g_VBoxGuestHaikuDeviceHooks =
    {
        VBoxGuestHaikuOpen,
        VBoxGuestHaikuClose,
        VBoxGuestHaikuFree,
        VBoxGuestHaikuIOCtl,
        VBoxGuestHaikuRead,
        VBoxGuestHaikuWrite,
        VBoxGuestHaikuSelect,
        VBoxGuestHaikuDeselect
    };
    return &g_VBoxGuestHaikuDeviceHooks;
}

const char** publish_devices()
{
    static const char *devices[] = { DEVICE_NAME, NULL };
    return devices;
}

void uninit_driver()
{
    put_module(MODULE_NAME);
}

