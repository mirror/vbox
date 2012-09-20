/* $Id$ */
/** @file
 * VBoxGuest kernel module, Haiku Guest Additions, implementation.
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

#define IN_VBOXGUEST
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
#include <iprt/memobj.h>
#include <iprt/asm.h>
#include <iprt/timer.h>
#include <iprt/heap.h>

#define MODULE_NAME VBOXGUEST_MODULE_NAME

/*
 * IRQ related functions.
 */
static void  VBoxGuestHaikuRemoveIRQ(void *pvState);
static int   VBoxGuestHaikuAddIRQ(void *pvState);
static int32 VBoxGuestHaikuISR(void *pvState);

/*
 * Available functions for kernel drivers.
 */
DECLVBGL(int)    VBoxGuestHaikuServiceCall(void *pvSession, unsigned uCmd, void *pvData, size_t cbData, size_t *pcbDataReturned);
DECLVBGL(void *) VBoxGuestHaikuServiceOpen(uint32_t *pu32Version);
DECLVBGL(int)    VBoxGuestHaikuServiceClose(void *pvSession);
DECLVBGL(void *) VBoxGuestIDCOpen(uint32_t *pu32Version);
DECLVBGL(int)    VBoxGuestIDCClose(void *pvSession);
DECLVBGL(int)    VBoxGuestIDCCall(void *pvSession, unsigned iCmd, void *pvData, size_t cbData, size_t *pcbDataReturned);

static status_t std_ops(int32 op, ...);

static RTSPINLOCK g_Spinlock = NIL_RTSPINLOCK;

int32    api_version = B_CUR_DRIVER_API_VERSION;

/** List of cloned device. Managed by the kernel. */
//static struct clonedevs    *g_pVBoxGuestHaikuClones;
/** The dev_clone event handler tag. */
//static eventhandler_tag     g_VBoxGuestHaikuEHTag;
/** selinfo structure used for polling. */
//static struct selinfo       g_SelInfo;
/** PCI Bus Manager Module */
static pci_module_info *gPCI;

static struct vboxguest_module_info g_VBoxGuest = {
    {
        MODULE_NAME,
        0,
        std_ops
    },
    {0},
    {0},
    0,
    RTLogBackdoorPrintf,
    RTLogBackdoorPrintfV,
    RTLogSetDefaultInstanceThread,
    RTMemAllocExTag,
    RTMemContAlloc,
    RTMemContFree,
    RTMemFreeEx,
    RTMpIsCpuPossible,
    RTMpNotificationDeregister,
    RTMpNotificationRegister,
    RTMpOnAll,
    RTMpOnOthers,
    RTMpOnSpecific,
    RTPowerNotificationDeregister,
    RTPowerNotificationRegister,
    RTPowerSignalEvent,
    RTR0AssertPanicSystem,
    RTR0Init,
    RTR0MemObjAddress,
    RTR0MemObjAddressR3,
    RTR0MemObjAllocContTag,
    RTR0MemObjAllocLowTag,
    RTR0MemObjAllocPageTag,
    RTR0MemObjAllocPhysExTag,
    RTR0MemObjAllocPhysNCTag,
    RTR0MemObjAllocPhysTag,
    RTR0MemObjEnterPhysTag,
    RTR0MemObjFree,
    RTR0MemObjGetPagePhysAddr,
    RTR0MemObjIsMapping,
    RTR0MemObjLockKernelTag,
    RTR0MemObjLockUserTag,
    RTR0MemObjMapKernelExTag,
    RTR0MemObjMapKernelTag,
    RTR0MemObjMapUserTag,
    RTR0MemObjProtect,
    RTR0MemObjReserveKernelTag,
    RTR0MemObjReserveUserTag,
    RTR0MemObjSize,
    RTR0ProcHandleSelf,
    RTR0Term,
    RTR0TermForced,
    RTProcSelf,
    RTSemEventGetResolution,
    RTSemEventMultiGetResolution,
    RTSemEventMultiWaitEx,
    RTSemEventMultiWaitExDebug,
    RTSemEventWaitEx,
    RTSemEventWaitExDebug,
    RTThreadIsInInterrupt,
    RTThreadPreemptDisable,
    RTThreadPreemptIsEnabled,
    RTThreadPreemptIsPending,
    RTThreadPreemptIsPendingTrusty,
    RTThreadPreemptIsPossible,
    RTThreadPreemptRestore,
    RTTimerGetSystemGranularity,
    RTTimerReleaseSystemGranularity,
    RTTimerRequestSystemGranularity,
    RTSpinlockAcquire,
    RTSpinlockRelease,
    RTSpinlockReleaseNoInts,
    RTMemTmpAllocTag,
    RTMemTmpFree,
    RTLogDefaultInstance,
    RTLogRelDefaultInstance,
    RTErrConvertToErrno,
    VBoxGuestCommonIOCtl,
    VBoxGuestCreateUserSession,
    VBoxGuestCloseSession,
    VBoxGuestIDCOpen,
    VBoxGuestIDCClose,
    VBoxGuestIDCCall,
    RTAssertMsg1Weak,
    RTAssertMsg2Weak,
    RTAssertMsg2WeakV,
    RTAssertShouldPanic,
    RTSemFastMutexCreate,
    RTSemFastMutexDestroy,
    RTSemFastMutexRelease,
    RTSemFastMutexRequest,
    RTSemMutexCreate,
    RTSemMutexDestroy,
    RTSemMutexRelease,
    RTSemMutexRequest,
    RTHeapSimpleRelocate,
    RTHeapOffsetInit,
    RTHeapSimpleInit,
    RTHeapOffsetAlloc,
    RTHeapSimpleAlloc,
    RTHeapOffsetFree,
    RTHeapSimpleFree
};

#if 0
/**
 * DEVFS event handler.
 */
static void VBoxGuestHaikuClone(void *pvArg, struct ucred *pCred, char *pszName, int cchName, struct cdev **ppDev)
{
    int iUnit;
    int rc;

    Log(("VBoxGuestHaikuClone: pszName=%s ppDev=%p\n", pszName, ppDev));

    /*
     * One device node per user, si_drv1 points to the session.
     * /dev/vboxguest<N> where N = {0...255}.
     */
    if (!ppDev)
        return;
    if (strcmp(pszName, "vboxguest") == 0)
        iUnit =  -1;
    else if (dev_stdclone(pszName, NULL, "vboxguest", &iUnit) != 1)
        return;
    if (iUnit >= 256)
    {
        Log(("VBoxGuestHaikuClone: iUnit=%d >= 256 - rejected\n", iUnit));
        return;
    }

    Log(("VBoxGuestHaikuClone: pszName=%s iUnit=%d\n", pszName, iUnit));

    rc = clone_create(&g_pVBoxGuestHaikuClones, &g_VBoxGuestHaikuDeviceHooks, &iUnit, ppDev, 0);
    Log(("VBoxGuestHaikuClone: clone_create -> %d; iUnit=%d\n", rc, iUnit));
    if (rc)
    {
        *ppDev = make_dev(&g_VBoxGuestHaikuDeviceHooks,
                          iUnit,
                          UID_ROOT,
                          GID_WHEEL,
                          0644,
                          "vboxguest%d", iUnit);
        if (*ppDev)
        {
            dev_ref(*ppDev);
            (*ppDev)->si_flags |= SI_CHEAPCLONE;
            Log(("VBoxGuestHaikuClone: Created *ppDev=%p iUnit=%d si_drv1=%p si_drv2=%p\n",
                     *ppDev, iUnit, (*ppDev)->si_drv1, (*ppDev)->si_drv2));
            (*ppDev)->si_drv1 = (*ppDev)->si_drv2 = NULL;
        }
        else
            Log(("VBoxGuestHaikuClone: make_dev iUnit=%d failed\n", iUnit));
    }
    else
        Log(("VBoxGuestHaikuClone: Existing *ppDev=%p iUnit=%d si_drv1=%p si_drv2=%p\n",
             *ppDev, iUnit, (*ppDev)->si_drv1, (*ppDev)->si_drv2));
}
#endif

static status_t VBoxGuestHaikuDetach(void)
{
    struct VBoxGuestDeviceState *pState = &sState;

    if (cUsers > 0)
        return EBUSY;

    /*
     * Reverse what we did in VBoxGuestHaikuAttach.
     */

    VBoxGuestHaikuRemoveIRQ(pState);

    if (pState->iVMMDevMemAreaId)
        delete_area(pState->iVMMDevMemAreaId);

    VBoxGuestDeleteDevExt(&g_DevExt);

#ifdef DO_LOG
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogSetDefaultInstance(NULL);
//    RTLogDestroy(RTLogSetDefaultInstance(NULL));
#endif

    RTSpinlockDestroy(g_Spinlock);
    g_Spinlock = NIL_RTSPINLOCK;

    RTR0Term();

    return B_OK;
}

/**
 * Interrupt service routine.
 *
 * @returns Whether the interrupt was from VMMDev.
 * @param   pvState Opaque pointer to the device state.
 */
static int32 VBoxGuestHaikuISR(void *pvState)
{
    LogFlow((MODULE_NAME ":VBoxGuestHaikuISR pvState=%p\n", pvState));

    bool fOurIRQ = VBoxGuestCommonISR(&g_DevExt);

    return fOurIRQ ? B_HANDLED_INTERRUPT : B_UNHANDLED_INTERRUPT;
}

void VBoxGuestNativeISRMousePollEvent(PVBOXGUESTDEVEXT pDevExt)
{
    LogFlow((MODULE_NAME "::NativeISRMousePollEvent:\n"));

    status_t err = B_OK;
    //dprintf(MODULE_NAME ": isr mouse\n");

    /*
     * Wake up poll waiters.
     */
    //selwakeup(&g_SelInfo);
    //XXX:notify_select_event();
    RTSpinlockAcquire(g_Spinlock);

    if (sState.selectSync) {
        //dprintf(MODULE_NAME ": isr mouse: notify\n");
        notify_select_event(sState.selectSync, sState.selectEvent);
        sState.selectEvent = (uint8_t)0;
        sState.selectRef = (uint32_t)0;
        sState.selectSync = NULL;
    } else
        err = B_ERROR;

    RTSpinlockRelease(g_Spinlock);
}

/**
 * Sets IRQ for VMMDev.
 *
 * @returns Haiku error code.
 * @param   pvState  Pointer to the state info structure.
 */
static int VBoxGuestHaikuAddIRQ(void *pvState)
{
    status_t status;
    struct VBoxGuestDeviceState *pState = (struct VBoxGuestDeviceState *)pvState;

    status = install_io_interrupt_handler(pState->iIrqResId, VBoxGuestHaikuISR, pState,  0);

    if (status != B_OK)
    {
        return VERR_DEV_IO_ERROR;
    }

    return VINF_SUCCESS;
}

/**
 * Removes IRQ for VMMDev.
 *
 * @param   pvState  Opaque pointer to the state info structure.
 */
static void VBoxGuestHaikuRemoveIRQ(void *pvState)
{
    struct VBoxGuestDeviceState *pState = (struct VBoxGuestDeviceState *)pvState;

    remove_io_interrupt_handler(pState->iIrqResId, VBoxGuestHaikuISR, pState);
}

static status_t VBoxGuestHaikuAttach(const pci_info *pDevice)
{
    status_t status;
    int rc = VINF_SUCCESS;
    int iResId = 0;
    struct VBoxGuestDeviceState *pState = &sState;
    static const char * const   s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    PRTLOGGER                   pRelLogger;

    cUsers = 0;

    /*
     * Initialize IPRT R0 driver, which internally calls OS-specific r0 init.
     */
    rc = RTR0Init(0);
    if (RT_FAILURE(rc))
    {
    	/** @todo r=ramshankar: use dprintf here */
        LogFunc(("RTR0Init failed.\n"));
        return ENXIO;
    }

    rc = RTSpinlockCreate(&g_Spinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VBoxGuestHaiku");
    if (RT_FAILURE(rc))
    {
    	LogRel(("VBoxGuestHaikuAttach: RTSpinlock create failed. rc=%Rrc\n", rc));
    	return ENXIO;
    }

#ifdef DO_LOG
    /*
     * Create the release log.
     * (We do that here instead of common code because we want to log
     * early failures using the LogRel macro.)
     */
    rc = RTLogCreate(&pRelLogger, 0|RTLOGFLAGS_PREFIX_THREAD /* fFlags */, "all",
                     "VBOX_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups,
                     RTLOGDEST_STDOUT | RTLOGDEST_DEBUGGER | RTLOGDEST_USER, NULL);
dprintf(MODULE_NAME ": RTLogCreate: %d\n", rc);
    if (RT_SUCCESS(rc))
    {
        //RTLogGroupSettings(pRelLogger, g_szLogGrp);
        //RTLogFlags(pRelLogger, g_szLogFlags);
        //RTLogDestinations(pRelLogger, "/var/log/vboxguest.log");
        RTLogRelSetDefaultInstance(pRelLogger);
        RTLogSetDefaultInstance(pRelLogger);//XXX
    }
#endif
    Log((MODULE_NAME ": plip!\n"));
    LogAlways((MODULE_NAME ": plop!\n"));


    /*
     * Allocate I/O port resource.
     */
    pState->uIOPortBase = pDevice->u.h0.base_registers[0];
    //XXX check flags for IO ?
    if (pState->uIOPortBase)
    {
        /*
         * Map the MMIO region.
         */
        uint32 phys = pDevice->u.h0.base_registers[1];
        //XXX check flags for mem ?
        pState->VMMDevMemSize    = pDevice->u.h0.base_register_sizes[1];
        pState->iVMMDevMemAreaId = map_physical_memory("VirtualBox Guest MMIO",
            phys, pState->VMMDevMemSize, B_ANY_KERNEL_BLOCK_ADDRESS,
            B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA, &pState->pMMIOBase);

        if (pState->iVMMDevMemAreaId > 0 && pState->pMMIOBase)
        {
            /*
             * Call the common device extension initializer.
             */
            rc = VBoxGuestInitDevExt(&g_DevExt, pState->uIOPortBase,
                                     pState->pMMIOBase, pState->VMMDevMemSize,
#if ARCH_BITS == 64
                                     VBOXOSTYPE_Haiku_x64,
#else
                                     VBOXOSTYPE_Haiku,
#endif
                                     VMMDEV_EVENT_MOUSE_POSITION_CHANGED);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Add IRQ of VMMDev.
                 */
                pState->iIrqResId = pDevice->u.h0.interrupt_line;
                rc = VBoxGuestHaikuAddIRQ(pState);
                if (RT_SUCCESS(rc))
                {
                    dprintf(MODULE_NAME ": loaded successfully\n");
                    return 0;
                }
                else
                    dprintf((MODULE_NAME ":VBoxGuestInitDevExt failed.\n"));
                VBoxGuestDeleteDevExt(&g_DevExt);
            }
            else
                dprintf((MODULE_NAME ":VBoxGuestHaikuAddIRQ failed.\n"));
        }
        else
            dprintf((MODULE_NAME ":MMIO region setup failed.\n"));
    }
    else
        dprintf((MODULE_NAME ":IOport setup failed.\n"));

    RTR0Term();
    return ENXIO;
}

static status_t VBoxGuestHaikuProbe(pci_info *pDevice)
{
    if ((pDevice->vendor_id == VMMDEV_VENDORID) && (pDevice->device_id == VMMDEV_DEVICEID))
        return 0;

    return ENXIO;
}

status_t init_module(void)
{
    status_t status = B_ENTRY_NOT_FOUND;
    pci_info info;
    int ix = 0;

    if (get_module(B_PCI_MODULE_NAME, (module_info **)&gPCI))
        return ENOSYS;

    while ((*gPCI->get_nth_pci_info)(ix++, &info) == B_OK) {
        if (VBoxGuestHaikuProbe(&info) == 0) {
            // we found it
            status = VBoxGuestHaikuAttach(&info);
            break;
        }
    }

    return status;
}

void uninit_module(void)
{
    VBoxGuestHaikuDetach();

    put_module(B_PCI_MODULE_NAME);
}

static status_t std_ops(int32 op, ...) {
    switch(op) {
    case B_MODULE_INIT:
        dprintf(MODULE_NAME ": B_MODULE_INIT\n");
        return init_module();
    case B_MODULE_UNINIT:
        dprintf(MODULE_NAME ": B_MODULE_UNINIT\n");
        uninit_module();
        return B_OK;
    default:
        return B_ERROR;
    }
}

_EXPORT module_info *modules[] = {
    (module_info*) &g_VBoxGuest,
    NULL
};

/* Common code that depend on g_DevExt. */
#include "VBoxGuestIDC-unix.c.h"
