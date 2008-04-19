/* $Id$ */
/** @file
 * VirtualBox Guest Additions Driver for FreeBSD.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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

/** @todo r=bird: This must merge with SUPDrv-freebsd.c before long. The two
 * source files should only differ on prefixes and the extra bits wrt to the
 * pci device. I.e. it should be diffable so that fixes to one can easily be
 * applied to the other. */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <sys/lockmgr.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "VBoxGuestInternal.h"
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <iprt/mem.h>
#include <iprt/asm.h>

/** The module name. */
#define DEVICE_NAME    "vboxguest"

struct VBoxGuestDeviceState
{
    /** file node minor code */
    unsigned           uMinor;
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

static MALLOC_DEFINE(M_VBOXDEV, "vboxdev_pci", "VirtualBox Guest driver PCI");

/*
 * Character device file handlers.
 */
static d_fdopen_t VBoxGuestFreeBSDOpen;
static d_close_t  VBoxGuestFreeBSDClose;
static d_ioctl_t  VBoxGuestFreeBSDIOCtl;
static d_write_t  VBoxGuestFreeBSDWrite;
static d_read_t   VBoxGuestFreeBSDRead;

/*
 * IRQ related functions.
 */
static void VBoxGuestFreeBSDRemoveIRQ(device_t pDevice, void *pvState);
static int  VBoxGuestFreeBSDAddIRQ(device_t pDevice, void *pvState);
static int  VBoxGuestFreeBSDISR(void *pvState);

/*
 * Available functions for kernel drivers.
 */
DECLVBGL(int)    VBoxGuestFreeBSDServiceCall(void *pvSession, unsigned uCmd, void *pvData, size_t cbData, size_t *pcbDataReturned);
DECLVBGL(void *) VBoxGuestFreeBSDServiceOpen(uint32_t *pu32Version);
DECLVBGL(int)    VBoxGuestFreeBSDServiceClose(void *pvSession);

/*
 * Device node entry points.
 */
static struct cdevsw    g_VBoxGuestFreeBSDChrDevSW =
{
    .d_version =        D_VERSION,
    .d_flags =          D_TRACKCLOSE,
    .d_fdopen =         VBoxGuestFreeBSDOpen,
    .d_close =          VBoxGuestFreeBSDClose,
    .d_ioctl =          VBoxGuestFreeBSDIOCtl,
    .d_read =           VBoxGuestFreeBSDRead,
    .d_write =          VBoxGuestFreeBSDWrite,
    .d_name =           DEVICE_NAME
};

/** Device extention & session data association structure. */
static VBOXGUESTDEVEXT      g_DevExt;
/** List of cloned device. Managed by the kernel. */
static struct clonedevs    *g_pVBoxGuestFreeBSDClones;
/** The dev_clone event handler tag. */
static eventhandler_tag     g_VBoxGuestFreeBSDEHTag;

/**
 * VBoxGuest Common ioctl wrapper from VBoxGuestLib.
 *
 * @returns VBox error code.
 * @param   pvSession           Opaque pointer to the session.
 * @param   uCmd                Requested function.
 * @param   pvData              IO data buffer.
 * @param   cbData              Size of the data buffer.
 * @param   pcbDataReturned     Where to store the amount of returned data.
 */
DECLVBGL(int) VBoxGuestFreeBSDServiceCall(void *pvSession, unsigned uCmd, void *pvData, size_t cbData, size_t *pcbDataReturned)
{
    LogFlow((DEVICE_NAME ":VBoxGuestFreeBSDServiceCall %pvSesssion=%p uCmd=%u pvData=%p cbData=%d\n", pvSession, uCmd, pvData, cbData));

    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pvSession;
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertMsgReturn(pSession->pDevExt == &g_DevExt,
                    ("SC: %p != %p\n", pSession->pDevExt, &g_DevExt), VERR_INVALID_HANDLE);

    return VBoxGuestCommonIOCtl(uCmd, &g_DevExt, pSession, pvData, cbData, pcbDataReturned);
}


/**
 * FreeBSD Guest service open.
 *
 * @returns Opaque pointer to session object.
 * @param   pu32Version         Where to store VMMDev version.
 */
DECLVBGL(void *) VBoxGuestFreeBSDServiceOpen(uint32_t *pu32Version)
{
    LogFlow((DEVICE_NAME ":VBoxGuestFreeBSDServiceOpen\n"));

    AssertPtrReturn(pu32Version, NULL);
    PVBOXGUESTSESSION pSession;
    int rc = VBoxGuestCreateKernelSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        *pu32Version = VMMDEV_VERSION;
        return pSession;
    }
    LogRel((DEVICE_NAME ":VBoxGuestCreateKernelSession failed. rc=%d\n", rc));
    return NULL;
}


/**
 * FreeBSD Guest service close.
 *
 * @returns VBox error code.
 * @param   pvState             Opaque pointer to the session object.
 */
DECLVBGL(int) VBoxGuestFreeBSDServiceClose(void *pvSession)
{
    LogFlow((DEVICE_NAME ":VBoxGuestFreeBSDServiceClose\n"));

    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pvSession;
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    if (pSession)
    {
        VBoxGuestCloseSession(&g_DevExt, pSession);
        return VINF_SUCCESS;
    }
    LogRel((DEVICE_NAME ":Invalid pSession.\n"));
    return VERR_INVALID_HANDLE;
}

/**
 * DEVFS event handler.
 */
static void VBoxGuestFreeBSDClone(void *pvArg, struct ucred *pCred, char *pszName, int cchName, struct cdev **ppDev)
{
    int iUnit;
    int rc;

    Log(("VBoxGuestFreeBSDClone: pszName=%s ppDev=%p\n", pszName, ppDev));

    /*
     * One device node per user, si_drv1 points to the session.
     * /dev/vboxguest<N> where N = {0...255}.
     */
    if (!ppDev)
        return;
    if (dev_stdclone(pszName, NULL, "vboxguest", &iUnit) != 1)
        return;
    if (iUnit >= 256 || iUnit < 0)
    {
        Log(("VBoxGuestFreeBSDClone: iUnit=%d >= 256 - rejected\n", iUnit));
        return;
    }

    Log(("VBoxGuestFreeBSDClone: pszName=%s iUnit=%d\n", pszName, iUnit));

    rc = clone_create(&g_pVBoxGuestFreeBSDClones, &g_VBoxGuestFreeBSDChrDevSW, &iUnit, ppDev, 0);
    Log(("VBoxGuestFreeBSDClone: clone_create -> %d; iUnit=%d\n", rc, iUnit));
    if (rc)
    {
        *ppDev = make_dev(&g_VBoxGuestFreeBSDChrDevSW,
                          unit2minor(iUnit),
                          UID_ROOT,
                          GID_WHEEL,
                          0644,
                          "vboxguest%d", iUnit);
        if (*ppDev)
        {
            dev_ref(*ppDev);
            (*ppDev)->si_flags |= SI_CHEAPCLONE;
            Log(("VBoxGuestFreeBSDClone: Created *ppDev=%p iUnit=%d si_drv1=%p si_drv2=%p\n",
                     *ppDev, iUnit, (*ppDev)->si_drv1, (*ppDev)->si_drv2));
            (*ppDev)->si_drv1 = (*ppDev)->si_drv2 = NULL;
        }
        else
            Log(("VBoxGuestFreeBSDClone: make_dev iUnit=%d failed\n", iUnit));
    }
    else
        Log(("VBoxGuestFreeBSDClone: Existing *ppDev=%p iUnit=%d si_drv1=%p si_drv2=%p\n",
             *ppDev, iUnit, (*ppDev)->si_drv1, (*ppDev)->si_drv2));
}

/**
 * File open handler
 *
 */
#if __FreeBSD_version >= 700000
static int VBoxGuestFreeBSDOpen(struct cdev *pDev, int fOpen, struct thread *pTd, struct file *pFd)
#else
static int VBoxGuestFreeBSDOpen(struct cdev *pDev, int fOpen, struct thread *pTd)
#endif
{
    int                 rc;
    PVBOXGUESTSESSION   pSession;

    LogFlow((DEVICE_NAME ":VBoxGuestFreeBSDOpen\n"));

    /*
     * Try grab it (we don't grab the giant, remember).
     */
    if (!ASMAtomicCmpXchgPtr(&pDev->si_drv1, (void *)0x42, NULL))
        return EBUSY;

    /*
     * Create a new session.
     */
    rc = VBoxGuestCreateUserSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        if (ASMAtomicCmpXchgPtr(&pDev->si_drv1, pSession, (void *)0x42))
        {
            Log((DEVICE_NAME ":VBoxGuestFreeBSDOpen success: g_DevExt=%p pSession=%p rc=%d pid=%d\n", &g_DevExt, pSession, rc, (int)RTProcSelf()));
            return 0;
        }

        VBoxGuestCloseSession(&g_DevExt, pSession);
    }

    LogRel((DEVICE_NAME ":VBoxGuestFreeBSDOpen: failed. rc=%d\n", rc));
    return RTErrConvertToErrno(rc);
}

/**
 * File close handler
 *
 */
static int VBoxGuestFreeBSDClose(struct cdev *pDev, int fFile, int DevType, struct thread *pTd)
{
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pDev->si_drv1;
    Log(("VBoxGuestFreeBSDClose: fFile=%#x iUnit=%d pSession=%p\n", fFile, minor2unit(minor(pDev)), pSession));

    /*
     * Close the session if it's still hanging on to the device...
     */
    if (VALID_PTR(pSession))
    {
        VBoxGuestCloseSession(&g_DevExt, pSession);
        if (!ASMAtomicCmpXchgPtr(&pDev->si_drv1, NULL, pSession))
            Log(("VBoxGuestFreeBSDClose: si_drv1=%p expected %p!\n", pDev->si_drv1, pSession));
    }
    else
        Log(("VBoxGuestFreeBSDClose: si_drv1=%p!\n", pSession));
    return 0;
}

/**
 * IOCTL handler
 *
 */
static int VBoxGuestFreeBSDIOCtl(struct cdev *pDev, u_long ulCmd, caddr_t pvData, int fFile, struct thread *pTd)
{
    LogFlow((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl\n"));

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
        Log((DEVICE_NAME ": VBoxGuestFreeBSDIOCtl: bad request %lu size=%lu expected=%d\n", ulCmd, IOCPARM_LEN(ulCmd), sizeof(VBGLBIGREQ)));
        return ENOTTY;
    }

    PVBGLBIGREQ ReqWrap = (PVBGLBIGREQ)pvData;
    if (ReqWrap->u32Magic != VBGLBIGREQ_MAGIC)
    {
        Log((DEVICE_NAME ": VBoxGuestFreeBSDIOCtl: bad magic %#x; pArg=%p Cmd=%lu.\n", ReqWrap->u32Magic, pvData, ulCmd));
        return EINVAL;
    }
    if (RT_UNLIKELY(   ReqWrap->cbData == 0
                    || ReqWrap->cbData > _1M*16))
    {
        printf(DEVICE_NAME ": VBoxGuestFreeBSDIOCtl: bad size %#x; pArg=%p Cmd=%lu.\n", ReqWrap->cbData, pvData, ulCmd);
        return EINVAL;
    }

    /*
     * Read the request.
     */
    void *pvBuf = RTMemTmpAlloc(ReqWrap->cbData);
    if (RT_UNLIKELY(!pvBuf))
    {
        Log((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl: RTMemTmpAlloc failed to alloc %d bytes.\n", ReqWrap->cbData));
        return ENOMEM;
    }

    rc = copyin((void *)(uintptr_t)ReqWrap->pvDataR3, pvBuf, ReqWrap->cbData);
    if (RT_UNLIKELY(rc))
    {
        RTMemTmpFree(pvBuf);
        Log((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl: copyin failed; pvBuf=%p pArg=%p Cmd=%lu. rc=%d\n", pvBuf, pvData, ulCmd, rc));
        return EFAULT;
    }
    if (RT_UNLIKELY(   ReqWrap->cbData != 0
                    && !VALID_PTR(pvBuf)))
    {
        RTMemTmpFree(pvBuf);
        Log((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl: pvBuf invalid pointer %p\n", pvBuf));
        return EINVAL;
    }
    Log((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl: pSession=%p pid=%d.\n", pSession, (int)RTProcSelf()));

    /*
     * Process the IOCtl.
     */
    size_t cbDataReturned;
    rc = VBoxGuestCommonIOCtl(ulCmd, &g_DevExt, pSession, pvBuf, ReqWrap->cbData, &cbDataReturned);
    if (RT_SUCCESS(rc))
    {
        rc = 0;
        if (RT_UNLIKELY(cbDataReturned > ReqWrap->cbData))
        {
            Log((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl: too much output data %d expected %d\n", cbDataReturned, ReqWrap->cbData));
            cbDataReturned = ReqWrap->cbData;
        }
        if (cbDataReturned > 0)
        {
            rc = copyout(pvBuf, (void *)(uintptr_t)ReqWrap->pvDataR3, cbDataReturned);
            if (RT_UNLIKELY(rc))
            {
                Log((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl: copyout failed; pvBuf=%p pArg=%p Cmd=%lu. rc=%d\n", pvBuf, pvData, ulCmd, rc));
                rc = EFAULT;
            }
        }
    }
    else
    {
        Log((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl: VBoxGuestCommonIOCtl failed. rc=%d\n", rc));
        rc = EFAULT;
    }
    RTMemTmpFree(pvBuf);
    return rc;
}

static ssize_t VBoxGuestFreeBSDWrite (struct cdev *pDev, struct uio *pUio, int fIo)
{
    return 0;
}

static ssize_t VBoxGuestFreeBSDRead (struct cdev *pDev, struct uio *pUio, int fIo)
{
    return 0;
}

static int VBoxGuestFreeBSDDetach(device_t pDevice)
{
    struct VBoxGuestDeviceState *pState = (struct VBoxGuestDeviceState *)device_get_softc(pDevice);

    /*
     * Reserve what we did in VBoxGuestFreeBSDAttach.
     */
    clone_cleanup(&g_pVBoxGuestFreeBSDClones);

    VBoxGuestFreeBSDRemoveIRQ(pDevice, pState);

    if (pState->pVMMDevMemRes)
        bus_release_resource(pDevice, SYS_RES_MEMORY, pState->iVMMDevMemResId, pState->pVMMDevMemRes);
    if (pState->pIOPortRes)
        bus_release_resource(pDevice, SYS_RES_IOPORT, pState->iIOPortResId, pState->pIOPortRes);

    VBoxGuestDeleteDevExt(&g_DevExt);

    free(pState, M_VBOXDEV);
    RTR0Term();

    return 0;
}

/**
 * Interrupt service routine.
 *
 * @returns Whether the interrupt was from VMMDev.
 * @param   pvState Opaque pointer to the device state.
 */
static int VBoxGuestFreeBSDISR(void *pvState)
{
    LogFlow((DEVICE_NAME ":VBoxGuestFreeBSDISR pvState=%p\n", pvState));

    bool fOurIRQ = VBoxGuestCommonISR(&g_DevExt);

    return fOurIRQ ? 0 : 1;
}

/**
 * Sets IRQ for VMMDev.
 *
 * @returns FreeBSD error code.
 * @param   pDevice  Pointer to the device info structure.
 * @param   pvState  Pointer to the state info structure.
 */
static int VBoxGuestFreeBSDAddIRQ(device_t pDevice, void *pvState)
{
    int iResId = 0;
    int rc = 0;
    struct VBoxGuestDeviceState *pState = (struct VBoxGuestDeviceState *)pvState;

    pState->pIrqRes = bus_alloc_resource_any(pDevice, SYS_RES_IRQ, &iResId, RF_SHAREABLE | RF_ACTIVE);

#if __FreeBSD_version >= 700000
    rc = bus_setup_intr(pDevice, pState->pIrqRes, INTR_TYPE_BIO, NULL, (driver_intr_t *)VBoxGuestFreeBSDISR, pState, &pState->pfnIrqHandler);
#else
    rc = bus_setup_intr(pDevice, pState->pIrqRes, INTR_TYPE_BIO, (driver_intr_t *)VBoxGuestFreeBSDISR, pState, &pState->pfnIrqHandler);
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
static void VBoxGuestFreeBSDRemoveIRQ(device_t pDevice, void *pvState)
{
    struct VBoxGuestDeviceState *pState = (struct VBoxGuestDeviceState *)pvState;

    if (pState->pIrqRes)
    {
        bus_teardown_intr(pDevice, pState->pIrqRes, pState->pfnIrqHandler);
        bus_release_resource(pDevice, SYS_RES_IRQ, 0, pState->pIrqRes);
    }
}

static int VBoxGuestFreeBSDAttach(device_t pDevice)
{
    int rc = VINF_SUCCESS;
    int iResId = 0;
    struct VBoxGuestDeviceState *pState = NULL;

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
    if (!pState)
    {
        pState = malloc(sizeof(struct VBoxGuestDeviceState), M_VBOXDEV, M_NOWAIT | M_ZERO);
        if (!pState)
            return ENOMEM;

        device_set_softc(pDevice, pState);
    }

    /*
     * Allocate I/O port resource.
     */
    iResId                 = PCIR_BAR(0);
    pState->pIOPortRes     = bus_alloc_resource_any(pDevice, SYS_RES_IOPORT, &iResId, RF_ACTIVE);
    pState->uIOPortBase    = bus_get_resource_start(pDevice, SYS_RES_IOPORT, iResId);
    pState->iIOPortResId   = iResId;
    if (pState->uIOPortBase)
    {
        /*
         * Map the MMIO region.
         */
        iResId                   = PCIR_BAR(1);
        pState->pVMMDevMemRes    = bus_alloc_resource_any(pDevice, SYS_RES_MEMORY, &iResId, RF_ACTIVE);
        pState->VMMDevMemHandle  = rman_get_bushandle(pState->pVMMDevMemRes);
        pState->VMMDevMemSize    = bus_get_resource_count(pDevice, SYS_RES_MEMORY, iResId);

        pState->pMMIOBase = (void *)pState->VMMDevMemHandle;
        pState->iVMMDevMemResId = iResId;
        if (pState->pMMIOBase)
        {
            /*
             * Add IRQ of VMMDev.
             */
            rc = VBoxGuestFreeBSDAddIRQ(pDevice, pState);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Call the common device extension initializer.
                 */
                rc = VBoxGuestInitDevExt(&g_DevExt, pState->uIOPortBase, pState->pMMIOBase,
                                            pState->VMMDevMemSize, VBOXOSTYPE_FreeBSD);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Configure device cloning.
                     */
                    clone_setup(&g_pVBoxGuestFreeBSDClones);
                    g_VBoxGuestFreeBSDEHTag = EVENTHANDLER_REGISTER(dev_clone, VBoxGuestFreeBSDClone, 0, 1000);
                    if (g_VBoxGuestFreeBSDEHTag)
                    {
                        printf(DEVICE_NAME ": loaded successfully\n");
                        return 0;
                    }

                    printf("vboxdrv: EVENTHANDLER_REGISTER(dev_clone,,,) failed\n");
                    clone_cleanup(&g_pVBoxGuestFreeBSDClones);
                    VBoxGuestDeleteDevExt(&g_DevExt);
                }
                else
                    printf((DEVICE_NAME ":VBoxGuestInitDevExt failed.\n"));
                VBoxGuestFreeBSDRemoveIRQ(pDevice, pState);
            }
            else
                printf((DEVICE_NAME ":VBoxGuestFreeBSDAddIRQ failed.\n"));
        }
        else
            printf((DEVICE_NAME ":MMIO region setup failed.\n"));
    }
    else
        printf((DEVICE_NAME ":IOport setup failed.\n"));

    RTR0Term();
    return ENXIO;
}

static int VBoxGuestFreeBSDProbe(device_t pDevice)
{
    if ((pci_get_vendor(pDevice) == VMMDEV_VENDORID) && (pci_get_device(pDevice) == VMMDEV_DEVICEID))
        return 0;

    return ENXIO;
}

static device_method_t VBoxGuestFreeBSDMethods[] =
{
    /* Device interface. */
    DEVMETHOD(device_probe,  VBoxGuestFreeBSDProbe),
    DEVMETHOD(device_attach, VBoxGuestFreeBSDAttach),
    DEVMETHOD(device_detach, VBoxGuestFreeBSDDetach),
    {0,0}
};

static driver_t VBoxGuestFreeBSDDriver =
{
    DEVICE_NAME,
    VBoxGuestFreeBSDMethods,
    sizeof(struct VBoxGuestDeviceState),
};

static devclass_t VBoxGuestFreeBSDClass;

DRIVER_MODULE(vboxguest, pci, VBoxGuestFreeBSDDriver, VBoxGuestFreeBSDClass, 0, 0);
MODULE_VERSION(vboxguest, 1);

int __gxx_personality_v0 = 0xdeadbeef;

