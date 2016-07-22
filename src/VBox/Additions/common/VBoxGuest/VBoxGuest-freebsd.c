/* $Id$ */
/** @file
 * VirtualBox Guest Additions Driver for FreeBSD.
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @todo r=bird: This must merge with SUPDrv-freebsd.c before long. The two
 * source files should only differ on prefixes and the extra bits wrt to the
 * pci device. I.e. it should be diffable so that fixes to one can easily be
 * applied to the other. */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <sys/param.h>
#undef PVM
#include <sys/types.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/bus.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/lockmgr.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "VBoxGuestInternal.h"
#include <VBox/version.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <iprt/mem.h>
#include <iprt/asm.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The module name. */
#define DEVICE_NAME  "vboxguest"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
struct VBoxGuestDeviceState
{
    /** Resource ID of the I/O port */
    int                iIOPortResId;
    /** Pointer to the I/O port resource. */
    struct resource   *pIOPortRes;
    /** Start address of the IO Port. */
    uint16_t           uIOPortBase;
    /** Resource ID of the MMIO area */
    int                iVMMDevMemResId;
    /** Pointer to the MMIO resource. */
    struct resource   *pVMMDevMemRes;
    /** Handle of the MMIO resource. */
    bus_space_handle_t VMMDevMemHandle;
    /** Size of the memory area. */
    bus_size_t         VMMDevMemSize;
    /** Mapping of the register space */
    void              *pMMIOBase;
    /** IRQ number */
    int                iIrqResId;
    /** IRQ resource handle. */
    struct resource   *pIrqRes;
    /** Pointer to the IRQ handler. */
    void              *pfnIrqHandler;
    /** VMMDev version */
    uint32_t           u32Version;
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
/*
 * Character device file handlers.
 */
static d_fdopen_t vgdrvFreeBSDOpen;
static d_close_t  vgdrvFreeBSDClose;
static d_ioctl_t  vgdrvFreeBSDIOCtl;
static d_write_t  vgdrvFreeBSDWrite;
static d_read_t   vgdrvFreeBSDRead;
static d_poll_t   vgdrvFreeBSDPoll;

/*
 * IRQ related functions.
 */
static void vgdrvFreeBSDRemoveIRQ(device_t pDevice, void *pvState);
static int  vgdrvFreeBSDAddIRQ(device_t pDevice, void *pvState);
static int  vgdrvFreeBSDISR(void *pvState);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static MALLOC_DEFINE(M_VBOXGUEST, "vboxguest", "VirtualBox Guest Device Driver");

#ifndef D_NEEDMINOR
# define D_NEEDMINOR 0
#endif

/*
 * The /dev/vboxguest character device entry points.
 */
static struct cdevsw    g_vgdrvFreeBSDChrDevSW =
{
    .d_version =        D_VERSION,
    .d_flags =          D_TRACKCLOSE | D_NEEDMINOR,
    .d_fdopen =         vgdrvFreeBSDOpen,
    .d_close =          vgdrvFreeBSDClose,
    .d_ioctl =          vgdrvFreeBSDIOCtl,
    .d_read =           vgdrvFreeBSDRead,
    .d_write =          vgdrvFreeBSDWrite,
    .d_poll =           vgdrvFreeBSDPoll,
    .d_name =           "vboxguest"
};

/** Device extention & session data association structure. */
static VBOXGUESTDEVEXT      g_DevExt;

/** List of cloned device. Managed by the kernel. */
static struct clonedevs    *g_pvgdrvFreeBSDClones;
/** The dev_clone event handler tag. */
static eventhandler_tag     g_vgdrvFreeBSDEHTag;
/** Reference counter */
static volatile uint32_t    cUsers;
/** selinfo structure used for polling. */
static struct selinfo       g_SelInfo;

/**
 * DEVFS event handler.
 */
static void vgdrvFreeBSDClone(void *pvArg, struct ucred *pCred, char *pszName, int cchName, struct cdev **ppDev)
{
    int iUnit;
    int rc;

    Log(("vgdrvFreeBSDClone: pszName=%s ppDev=%p\n", pszName, ppDev));

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
        Log(("vgdrvFreeBSDClone: iUnit=%d >= 256 - rejected\n", iUnit));
        return;
    }

    Log(("vgdrvFreeBSDClone: pszName=%s iUnit=%d\n", pszName, iUnit));

    rc = clone_create(&g_pvgdrvFreeBSDClones, &g_vgdrvFreeBSDChrDevSW, &iUnit, ppDev, 0);
    Log(("vgdrvFreeBSDClone: clone_create -> %d; iUnit=%d\n", rc, iUnit));
    if (rc)
    {
        *ppDev = make_dev(&g_vgdrvFreeBSDChrDevSW,
                          iUnit,
                          UID_ROOT,
                          GID_WHEEL,
                          0664,
                          "vboxguest%d", iUnit);
        if (*ppDev)
        {
            dev_ref(*ppDev);
            (*ppDev)->si_flags |= SI_CHEAPCLONE;
            Log(("vgdrvFreeBSDClone: Created *ppDev=%p iUnit=%d si_drv1=%p si_drv2=%p\n",
                     *ppDev, iUnit, (*ppDev)->si_drv1, (*ppDev)->si_drv2));
            (*ppDev)->si_drv1 = (*ppDev)->si_drv2 = NULL;
        }
        else
            Log(("vgdrvFreeBSDClone: make_dev iUnit=%d failed\n", iUnit));
    }
    else
        Log(("vgdrvFreeBSDClone: Existing *ppDev=%p iUnit=%d si_drv1=%p si_drv2=%p\n",
             *ppDev, iUnit, (*ppDev)->si_drv1, (*ppDev)->si_drv2));
}

/**
 * File open handler
 *
 */
#if __FreeBSD_version >= 700000
static int vgdrvFreeBSDOpen(struct cdev *pDev, int fOpen, struct thread *pTd, struct file *pFd)
#else
static int vgdrvFreeBSDOpen(struct cdev *pDev, int fOpen, struct thread *pTd)
#endif
{
    int                 rc;
    PVBOXGUESTSESSION   pSession;

    LogFlow(("vgdrvFreeBSDOpen:\n"));

    /*
     * Try grab it (we don't grab the giant, remember).
     */
    if (!ASMAtomicCmpXchgPtr(&pDev->si_drv1, (void *)0x42, NULL))
        return EBUSY;

    /*
     * Create a new session.
     */
    rc = VGDrvCommonCreateUserSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        if (ASMAtomicCmpXchgPtr(&pDev->si_drv1, pSession, (void *)0x42))
        {
            Log(("vgdrvFreeBSDOpen: success - g_DevExt=%p pSession=%p rc=%d pid=%d\n", &g_DevExt, pSession, rc, (int)RTProcSelf()));
            ASMAtomicIncU32(&cUsers);
            return 0;
        }

        VGDrvCommonCloseSession(&g_DevExt, pSession);
    }

    LogRel(("vgdrvFreeBSDOpen: failed. rc=%d\n", rc));
    return RTErrConvertToErrno(rc);
}

/**
 * File close handler
 *
 */
static int vgdrvFreeBSDClose(struct cdev *pDev, int fFile, int DevType, struct thread *pTd)
{
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pDev->si_drv1;
    Log(("vgdrvFreeBSDClose: fFile=%#x pSession=%p\n", fFile, pSession));

    /*
     * Close the session if it's still hanging on to the device...
     */
    if (VALID_PTR(pSession))
    {
        VGDrvCommonCloseSession(&g_DevExt, pSession);
        if (!ASMAtomicCmpXchgPtr(&pDev->si_drv1, NULL, pSession))
            Log(("vgdrvFreeBSDClose: si_drv1=%p expected %p!\n", pDev->si_drv1, pSession));
        ASMAtomicDecU32(&cUsers);
        /* Don't use destroy_dev here because it may sleep resulting in a hanging user process. */
        destroy_dev_sched(pDev);
    }
    else
        Log(("vgdrvFreeBSDClose: si_drv1=%p!\n", pSession));
    return 0;
}

/**
 * IOCTL handler
 *
 */
static int vgdrvFreeBSDIOCtl(struct cdev *pDev, u_long ulCmd, caddr_t pvData, int fFile, struct thread *pTd)
{
    LogFlow(("vgdrvFreeBSDIOCtl\n"));

    int rc = 0;

    /*
     * Validate the input.
     */
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pDev->si_drv1;
    if (RT_UNLIKELY(!VALID_PTR(pSession)))
        return EINVAL;

    /*
     * Validate the request wrapper.
     */
    if (IOCPARM_LEN(ulCmd) != sizeof(VBGLBIGREQ))
    {
        Log(("vgdrvFreeBSDIOCtl: bad request %lu size=%lu expected=%d\n", ulCmd, IOCPARM_LEN(ulCmd), sizeof(VBGLBIGREQ)));
        return ENOTTY;
    }

    PVBGLBIGREQ ReqWrap = (PVBGLBIGREQ)pvData;
    if (ReqWrap->u32Magic != VBGLBIGREQ_MAGIC)
    {
        Log(("vgdrvFreeBSDIOCtl: bad magic %#x; pArg=%p Cmd=%lu.\n", ReqWrap->u32Magic, pvData, ulCmd));
        return EINVAL;
    }
    if (RT_UNLIKELY(   ReqWrap->cbData == 0
                    || ReqWrap->cbData > _1M*16))
    {
        printf("vgdrvFreeBSDIOCtl: bad size %#x; pArg=%p Cmd=%lu.\n", ReqWrap->cbData, pvData, ulCmd);
        return EINVAL;
    }

    /*
     * Read the request.
     */
    void *pvBuf = RTMemTmpAlloc(ReqWrap->cbData);
    if (RT_UNLIKELY(!pvBuf))
    {
        Log(("vgdrvFreeBSDIOCtl: RTMemTmpAlloc failed to alloc %d bytes.\n", ReqWrap->cbData));
        return ENOMEM;
    }

    rc = copyin((void *)(uintptr_t)ReqWrap->pvDataR3, pvBuf, ReqWrap->cbData);
    if (RT_UNLIKELY(rc))
    {
        RTMemTmpFree(pvBuf);
        Log(("vgdrvFreeBSDIOCtl: copyin failed; pvBuf=%p pArg=%p Cmd=%lu. rc=%d\n", pvBuf, pvData, ulCmd, rc));
        return EFAULT;
    }
    if (RT_UNLIKELY(   ReqWrap->cbData != 0
                    && !VALID_PTR(pvBuf)))
    {
        RTMemTmpFree(pvBuf);
        Log(("vgdrvFreeBSDIOCtl: pvBuf invalid pointer %p\n", pvBuf));
        return EINVAL;
    }
    Log(("vgdrvFreeBSDIOCtl: pSession=%p pid=%d.\n", pSession, (int)RTProcSelf()));

    /*
     * Process the IOCtl.
     */
    size_t cbDataReturned;
    rc = VGDrvCommonIoCtl(ulCmd, &g_DevExt, pSession, pvBuf, ReqWrap->cbData, &cbDataReturned);
    if (RT_SUCCESS(rc))
    {
        rc = 0;
        if (RT_UNLIKELY(cbDataReturned > ReqWrap->cbData))
        {
            Log(("vgdrvFreeBSDIOCtl: too much output data %d expected %d\n", cbDataReturned, ReqWrap->cbData));
            cbDataReturned = ReqWrap->cbData;
        }
        if (cbDataReturned > 0)
        {
            rc = copyout(pvBuf, (void *)(uintptr_t)ReqWrap->pvDataR3, cbDataReturned);
            if (RT_UNLIKELY(rc))
            {
                Log(("vgdrvFreeBSDIOCtl: copyout failed; pvBuf=%p pArg=%p Cmd=%lu. rc=%d\n", pvBuf, pvData, ulCmd, rc));
                rc = EFAULT;
            }
        }
    }
    else
    {
        Log(("vgdrvFreeBSDIOCtl: VGDrvCommonIoCtl failed. rc=%d\n", rc));
        rc = EFAULT;
    }
    RTMemTmpFree(pvBuf);
    return rc;
}

static int vgdrvFreeBSDPoll(struct cdev *pDev, int fEvents, struct thread *td)
{
    int fEventsProcessed;

    LogFlow(("vgdrvFreeBSDPoll: fEvents=%d\n", fEvents));

    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pDev->si_drv1;
    if (RT_UNLIKELY(!VALID_PTR(pSession))) {
        Log(("vgdrvFreeBSDPoll: no state data for %s\n", devtoname(pDev)));
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
}

static int vgdrvFreeBSDWrite(struct cdev *pDev, struct uio *pUio, int fIo)
{
    return 0;
}

static int vgdrvFreeBSDRead(struct cdev *pDev, struct uio *pUio, int fIo)
{
    return 0;
}

static int vgdrvFreeBSDDetach(device_t pDevice)
{
    struct VBoxGuestDeviceState *pState = device_get_softc(pDevice);

    if (cUsers > 0)
        return EBUSY;

    /*
     * Reverse what we did in vgdrvFreeBSDAttach.
     */
    if (g_vgdrvFreeBSDEHTag != NULL)
        EVENTHANDLER_DEREGISTER(dev_clone, g_vgdrvFreeBSDEHTag);

    clone_cleanup(&g_pvgdrvFreeBSDClones);

    vgdrvFreeBSDRemoveIRQ(pDevice, pState);

    if (pState->pVMMDevMemRes)
        bus_release_resource(pDevice, SYS_RES_MEMORY, pState->iVMMDevMemResId, pState->pVMMDevMemRes);
    if (pState->pIOPortRes)
        bus_release_resource(pDevice, SYS_RES_IOPORT, pState->iIOPortResId, pState->pIOPortRes);

    VGDrvCommonDeleteDevExt(&g_DevExt);

    RTR0Term();

    return 0;
}

/**
 * Interrupt service routine.
 *
 * @returns Whether the interrupt was from VMMDev.
 * @param   pvState Opaque pointer to the device state.
 */
static int vgdrvFreeBSDISR(void *pvState)
{
    LogFlow(("vgdrvFreeBSDISR: pvState=%p\n", pvState));

    bool fOurIRQ = VGDrvCommonISR(&g_DevExt);

    return fOurIRQ ? 0 : 1;
}

void VGDrvNativeISRMousePollEvent(PVBOXGUESTDEVEXT pDevExt)
{
    LogFlow(("VGDrvNativeISRMousePollEvent:\n"));

    /*
     * Wake up poll waiters.
     */
    selwakeup(&g_SelInfo);
}

/**
 * Sets IRQ for VMMDev.
 *
 * @returns FreeBSD error code.
 * @param   pDevice  Pointer to the device info structure.
 * @param   pvState  Pointer to the state info structure.
 */
static int vgdrvFreeBSDAddIRQ(device_t pDevice, void *pvState)
{
    int iResId = 0;
    int rc = 0;
    struct VBoxGuestDeviceState *pState = (struct VBoxGuestDeviceState *)pvState;

    pState->pIrqRes = bus_alloc_resource_any(pDevice, SYS_RES_IRQ, &iResId, RF_SHAREABLE | RF_ACTIVE);

#if __FreeBSD_version >= 700000
    rc = bus_setup_intr(pDevice, pState->pIrqRes, INTR_TYPE_BIO | INTR_MPSAFE, NULL, (driver_intr_t *)vgdrvFreeBSDISR, pState,
                        &pState->pfnIrqHandler);
#else
    rc = bus_setup_intr(pDevice, pState->pIrqRes, INTR_TYPE_BIO, (driver_intr_t *)vgdrvFreeBSDISR, pState, &pState->pfnIrqHandler);
#endif

    if (rc)
    {
        pState->pfnIrqHandler = NULL;
        return VERR_DEV_IO_ERROR;
    }

    pState->iIrqResId = iResId;

    return VINF_SUCCESS;
}

/**
 * Removes IRQ for VMMDev.
 *
 * @param   pDevice  Pointer to the device info structure.
 * @param   pvState  Opaque pointer to the state info structure.
 */
static void vgdrvFreeBSDRemoveIRQ(device_t pDevice, void *pvState)
{
    struct VBoxGuestDeviceState *pState = (struct VBoxGuestDeviceState *)pvState;

    if (pState->pIrqRes)
    {
        bus_teardown_intr(pDevice, pState->pIrqRes, pState->pfnIrqHandler);
        bus_release_resource(pDevice, SYS_RES_IRQ, 0, pState->pIrqRes);
    }
}

static int vgdrvFreeBSDAttach(device_t pDevice)
{
    int rc;
    int iResId;
    struct VBoxGuestDeviceState *pState;

    cUsers = 0;

    /*
     * Initialize IPRT R0 driver, which internally calls OS-specific r0 init.
     */
    rc = RTR0Init(0);
    if (RT_FAILURE(rc))
    {
        LogFunc(("RTR0Init failed.\n"));
        return ENXIO;
    }

    pState = device_get_softc(pDevice);

    /*
     * Allocate I/O port resource.
     */
    iResId                 = PCIR_BAR(0);
    pState->pIOPortRes     = bus_alloc_resource_any(pDevice, SYS_RES_IOPORT, &iResId, RF_ACTIVE);
    pState->uIOPortBase    = rman_get_start(pState->pIOPortRes);
    pState->iIOPortResId   = iResId;
    if (pState->uIOPortBase)
    {
        /*
         * Map the MMIO region.
         */
        iResId                   = PCIR_BAR(1);
        pState->pVMMDevMemRes    = bus_alloc_resource_any(pDevice, SYS_RES_MEMORY, &iResId, RF_ACTIVE);
        pState->VMMDevMemHandle  = rman_get_bushandle(pState->pVMMDevMemRes);
        pState->VMMDevMemSize    = rman_get_size(pState->pVMMDevMemRes);

        pState->pMMIOBase        = rman_get_virtual(pState->pVMMDevMemRes);
        pState->iVMMDevMemResId  = iResId;
        if (pState->pMMIOBase)
        {
            /*
             * Call the common device extension initializer.
             */
            rc = VGDrvCommonInitDevExt(&g_DevExt, pState->uIOPortBase,
                                       pState->pMMIOBase, pState->VMMDevMemSize,
#if ARCH_BITS == 64
                                       VBOXOSTYPE_FreeBSD_x64,
#else
                                       VBOXOSTYPE_FreeBSD,
#endif
                                       VMMDEV_EVENT_MOUSE_POSITION_CHANGED);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Add IRQ of VMMDev.
                 */
                rc = vgdrvFreeBSDAddIRQ(pDevice, pState);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Configure device cloning.
                     */
                    clone_setup(&g_pvgdrvFreeBSDClones);
                    g_vgdrvFreeBSDEHTag = EVENTHANDLER_REGISTER(dev_clone, vgdrvFreeBSDClone, 0, 1000);
                    if (g_vgdrvFreeBSDEHTag)
                    {
                        printf(DEVICE_NAME ": loaded successfully\n");
                        return 0;
                    }

                    printf(DEVICE_NAME ": EVENTHANDLER_REGISTER(dev_clone,,,) failed\n");
                    clone_cleanup(&g_pvgdrvFreeBSDClones);
                    vgdrvFreeBSDRemoveIRQ(pDevice, pState);
                }
                else
                    printf((DEVICE_NAME ": VGDrvCommonInitDevExt failed.\n"));
                VGDrvCommonDeleteDevExt(&g_DevExt);
            }
            else
                printf((DEVICE_NAME ": vgdrvFreeBSDAddIRQ failed.\n"));
        }
        else
            printf((DEVICE_NAME ": MMIO region setup failed.\n"));
    }
    else
        printf((DEVICE_NAME ": IOport setup failed.\n"));

    RTR0Term();
    return ENXIO;
}

static int vgdrvFreeBSDProbe(device_t pDevice)
{
    if ((pci_get_vendor(pDevice) == VMMDEV_VENDORID) && (pci_get_device(pDevice) == VMMDEV_DEVICEID))
        return 0;

    return ENXIO;
}

static device_method_t vgdrvFreeBSDMethods[] =
{
    /* Device interface. */
    DEVMETHOD(device_probe,  vgdrvFreeBSDProbe),
    DEVMETHOD(device_attach, vgdrvFreeBSDAttach),
    DEVMETHOD(device_detach, vgdrvFreeBSDDetach),
    {0,0}
};

static driver_t vgdrvFreeBSDDriver =
{
    DEVICE_NAME,
    vgdrvFreeBSDMethods,
    sizeof(struct VBoxGuestDeviceState),
};

static devclass_t vgdrvFreeBSDClass;

DRIVER_MODULE(vboxguest, pci, vgdrvFreeBSDDriver, vgdrvFreeBSDClass, 0, 0);
MODULE_VERSION(vboxguest, 1);

/* Common code that depend on g_DevExt. */
#include "VBoxGuestIDC-unix.c.h"

