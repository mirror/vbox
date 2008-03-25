/* $Id:$ */
/** @file
 * VirtualBox Guest Additions Driver for FreeBSD.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
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

/** The module name. */
#define DEVICE_NAME              "vboxguest"

struct VBoxGuestDeviceState
{
    /** file node minor code */
    unsigned          minor;
    /** first IO port */
    int                io_port_resid;
    struct resource   *io_port;
    bus_space_tag_t    io_port_tag;
    bus_space_handle_t io_port_handle;
    /** IO Port. */
    uint16_t           uIOPortBase;
    /** device memory resources */
    int                vmmdevmem_resid;
    struct resource   *vmmdevmem;
    bus_space_tag_t    vmmdevmem_tag;
    bus_space_handle_t vmmdevmem_handle;
    bus_size_t         vmmdevmem_size;
    /** physical address of adapter memory */
    uint32_t           vmmdevmem_addr;
    /** Mapping of the register space */
    void              *pMMIOBase;
    /** IRQ number */
    int                irq_resid;
    struct resource   *irq;
    void              *irq_handler;
    /** VMMDev version */
    uint32_t           u32Version;
};

static MALLOC_DEFINE(M_VBOXDEV, "vboxdev_pci", "VirtualBox Guest driver PCI");

#if FreeBSD >= 700
static int VBoxGuestFreeBSDOpen(struct cdev *dev, int flags, int fmt, struct thread *td, struct file *pFd);
#else
static int VBoxGuestFreeBSDOpen(struct cdev *dev, int flags, int fmt, struct thread *td);
#endif
static int VBoxGuestFreeBSDClose(struct cdev *dev, int flags, int fmt, struct thread *td);
static int VBoxGuestFreeBSDIOCtl(struct cdev *dev, u_long cmd, caddr_t data,
                                 int fflag, struct thread *td);
static ssize_t VBoxGuestFreeBSDWrite (struct cdev *dev, struct uio *uio, int ioflag);
static ssize_t VBoxGuestFreeBSDRead (struct cdev *dev, struct uio *uio, int ioflag);
static void VBoxGuestFreeBSDRemoveIRQ(device_t self, void *pvState);
static int VBoxGuestFreeBSDAddIRQ(device_t self, void *pvState);
int VBoxGuestFreeBSDISR(void *pvState);
DECLVBGL(int) VBoxGuestFreeBSDServiceCall(void *pvSession, unsigned iCmd, void *pvData, size_t cbData, size_t *pcbDataReturned);
DECLVBGL(void *) VBoxGuestFreeBSDServiceOpen(uint32_t *pu32Version);
DECLVBGL(int) VBoxGuestFreeBSDServiceClose(void *pvSession);

/*
 * Device node entry points.
 */
static struct cdevsw    g_VBoxAddFreeBSDChrDevSW =
{
    .d_version =        D_VERSION,
    .d_flags =          D_TRACKCLOSE,
    .d_open =           VBoxGuestFreeBSDOpen,
    .d_close =          VBoxGuestFreeBSDClose,
    .d_ioctl =          VBoxGuestFreeBSDIOCtl,
    .d_read =           VBoxGuestFreeBSDRead,
    .d_write =          VBoxGuestFreeBSDWrite,
    .d_name =           DEVICE_NAME
};

/** The make_dev result. */
static struct cdev         *g_pVBoxAddFreeBSDChrDev;
/** Device extention & session data association structure. */
static VBOXGUESTDEVEXT      g_DevExt;
/** Spinlock protecting g_apSessionHashTab. */
static RTSPINLOCK           g_Spinlock = NIL_RTSPINLOCK;
/** Hash table */
static PVBOXGUESTSESSION    g_apSessionHashTab[19];
/** Calculates the index into g_apSessionHashTab.*/
#define SESSION_HASH(sfn) ((sfn) % RT_ELEMENTS(g_apSessionHashTab))

/** @todo r=bird: fork() and session hash table didn't work well for solaris, so I doubt it works for
 * FreeBSD... A different solution is needed unless the current can be improved. */

/**
 * VBoxGuest Common ioctl wrapper from VBoxGuestLib.
 *
 * @returns VBox error code.
 * @param   pvSession           Opaque pointer to the session.
 * @param   iCmd                Requested function.
 * @param   pvData              IO data buffer.
 * @param   cbData              Size of the data buffer.
 * @param   pcbDataReturned     Where to store the amount of returned data.
 */
DECLVBGL(int) VBoxGuestFreeBSDServiceCall(void *pvSession, unsigned iCmd, void *pvData, size_t cbData, size_t *pcbDataReturned)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisServiceCall %pvSesssion=%p Cmd=%u pvData=%p cbData=%d\n", pvSession, iCmd, pvData, cbData));

    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pvSession;
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertMsgReturn(pSession->pDevExt == &g_DevExt,
                    ("SC: %p != %p\n", pSession->pDevExt, &g_DevExt), VERR_INVALID_HANDLE);

    return VBoxGuestCommonIOCtl(iCmd, &g_DevExt, pSession, pvData, cbData, pcbDataReturned);
}


/**
 * FreeBSD Guest service open.
 *
 * @returns Opaque pointer to session object.
 * @param   pu32Version         Where to store VMMDev version.
 */
DECLVBGL(void *) VBoxGuestFreeBSDServiceOpen(uint32_t *pu32Version)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisServiceOpen\n"));

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
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisServiceClose\n"));

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
 * File open handler
 *
 */
#if FreeBSD >= 700
static int VBoxGuestFreeBSDOpen(struct cdev *dev, int flags, int fmt, struct thread *td, struct file *pFd)
#else
static int VBoxGuestFreeBSDOpen(struct cdev *dev, int flags, int fmt, struct thread *td)
#endif
{
    int                 rc;
    PVBOXGUESTSESSION   pSession;

    LogFlow((DEVICE_NAME ":VBoxGuestFreeBSDOpen\n"));

    /*
     * Create a new session.
     */
    rc = VBoxGuestCreateUserSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        /*
         * Insert it into the hash table.
         */
        unsigned iHash = SESSION_HASH(pSession->Process);
        RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
        RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
        pSession->pNextHash = g_apSessionHashTab[iHash];
        g_apSessionHashTab[iHash] = pSession;
        RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);

        Log((DEVICE_NAME ":VBoxGuestFreeBSDOpen success: g_DevExt=%p pSession=%p rc=%d pid=%d\n", &g_DevExt, pSession, rc, (int)RTProcSelf()));
        return 0;
    }

    LogRel((DEVICE_NAME ":VBoxGuestFreeBSDOpen: VBoxGuestCreateUserSession failed. rc=%d\n", rc));
    return EFAULT;
}

/**
 * File close handler
 *
 */
static int VBoxGuestFreeBSDClose(struct cdev *dev, int flags, int fmt, struct thread *td)
{
    LogFlow((DEVICE_NAME ":VBoxGuestFreeBSDClose pid=%d\n", (int)RTProcSelf()));

    /*
     * Remove from the hash table.
     */
    PVBOXGUESTSESSION   pSession;
    const RTPROCESS     Process = RTProcSelf();
    const unsigned      iHash = SESSION_HASH(Process);
    RTSPINLOCKTMP       Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);

    pSession = g_apSessionHashTab[iHash];
    if (pSession)
    {
        if (pSession->Process == Process)
        {
            g_apSessionHashTab[iHash] = pSession->pNextHash;
            pSession->pNextHash = NULL;
        }
        else
        {
            PVBOXGUESTSESSION pPrev = pSession;
            pSession = pSession->pNextHash;
            while (pSession)
            {
                if (pSession->Process == Process)
                {
                    pPrev->pNextHash = pSession->pNextHash;
                    pSession->pNextHash = NULL;
                    break;
                }

                /* next */
                pPrev = pSession;
                pSession = pSession->pNextHash;
            }
        }
    }
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    if (!pSession)
    {
        Log((DEVICE_NAME ":VBoxGuestFreeBSDClose: WHUT?!? pSession == NULL! This must be a mistake... pid=%d", (int)Process));
        return EFAULT;
    }
    Log((DEVICE_NAME ":VBoxGuestFreeBSDClose: pid=%d\n", (int)Process));

    /*
     * Close the session.
     */
    VBoxGuestCloseSession(&g_DevExt, pSession);
    return 0;
}

/**
 * IOCTL handler
 *
 */
static int VBoxGuestFreeBSDIOCtl(struct cdev *dev, u_long cmd, caddr_t data,
	                             int fflag, struct thread *td)
{
    LogFlow((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl\n"));

    RTSPINLOCKTMP       Tmp = RTSPINLOCKTMP_INITIALIZER;
    const RTPROCESS     Process = RTProcSelf();
    const unsigned      iHash = SESSION_HASH(Process);
    PVBOXGUESTSESSION   pSession;
    int                 rc = 0;

    /*
     * Find the session.
     */
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    pSession = g_apSessionHashTab[iHash];
    if (pSession && pSession->Process != Process)
    {
        do pSession = pSession->pNextHash;
        while (pSession && pSession->Process != Process);
    }
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    if (!pSession)
    {
        Log((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl: WHAT?!? pSession == NULL! This must be a mistake... pid=%d iCmd=%#lx\n", (int)Process, cmd));
        return EINVAL;
    }

    /*
     * Validate the request wrapper.
     */
    if (IOCPARM_LEN(cmd) != sizeof(VBGLBIGREQ))
    {
        Log((DEVICE_NAME ": VBoxGuestFreeBSDIOCtl: bad request %lu size=%lu expected=%d\n", cmd, IOCPARM_LEN(cmd), sizeof(VBGLBIGREQ)));
        return ENOTTY;
    }

    PVBGLBIGREQ ReqWrap = (PVBGLBIGREQ)data;
    if (ReqWrap->u32Magic != VBGLBIGREQ_MAGIC)
    {
        Log((DEVICE_NAME ": VBoxGuestFreeBSDIOCtl: bad magic %#x; pArg=%p Cmd=%lu.\n", ReqWrap->u32Magic, data, cmd));
        return EINVAL;
    }
    if (RT_UNLIKELY(   ReqWrap->cbData == 0
                    || ReqWrap->cbData > _1M*16))
    {
        printf(DEVICE_NAME ": VBoxGuestFreeBSDIOCtl: bad size %#x; pArg=%p Cmd=%lu.\n", ReqWrap->cbData, data, cmd);
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
        Log((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl: copyin failed; pvBuf=%p pArg=%p Cmd=%lu. rc=%d\n", pvBuf, data, cmd, rc));
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
    rc = VBoxGuestCommonIOCtl(cmd, &g_DevExt, pSession, pvBuf, ReqWrap->cbData, &cbDataReturned);
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
                Log((DEVICE_NAME ":VBoxGuestFreeBSDIOCtl: copyout failed; pvBuf=%p pArg=%p Cmd=%lu. rc=%d\n", pvBuf, data, cmd, rc));
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

static ssize_t VBoxGuestFreeBSDWrite (struct cdev *dev, struct uio *uio, int ioflag)
{
    return 0;
}

static ssize_t VBoxGuestFreeBSDRead (struct cdev *dev, struct uio *uio, int ioflag)
{
    return 0;
}

static int VBoxGuestFreeBSDDetach(device_t self)
{
    struct VBoxGuestDeviceState *pState = (struct VBoxGuestDeviceState *)device_get_softc(self);

    VBoxGuestFreeBSDRemoveIRQ(self, pState);

    if (pState->vmmdevmem)
        bus_release_resource(self, SYS_RES_MEMORY, pState->vmmdevmem_resid, pState->vmmdevmem);
    if (pState->io_port)
        bus_release_resource(self, SYS_RES_IOPORT, pState->io_port_resid, pState->io_port);

    VBoxGuestDeleteDevExt(&g_DevExt);

    RTSpinlockDestroy(g_Spinlock);
    g_Spinlock = NIL_RTSPINLOCK;

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
int VBoxGuestFreeBSDISR(void *pvState)
{
    LogFlow((DEVICE_NAME ":VBoxGuestFreeBSDISR pvState=%p\n", pvState));

    bool fOurIRQ = VBoxGuestCommonISR(&g_DevExt);

    return fOurIRQ ? 0 : 1;
}

/**
 * Sets IRQ for VMMDev.
 *
 * @returns FreeBSD error code.
 * @param   self     Pointer to the device info structure.
 * @param   pvState  Pointer to the state info structure.
 */
static int VBoxGuestFreeBSDAddIRQ(device_t self, void *pvState)
{
    int res_id = 0;
    int rc = 0;
    struct VBoxGuestDeviceState *pState = (struct VBoxGuestDeviceState *)pvState;

    pState->irq = bus_alloc_resource_any(self, SYS_RES_IRQ, &res_id, RF_SHAREABLE | RF_ACTIVE);

#if __FreeBSD_version >= 700000
    rc = bus_setup_intr(self, pState->irq, INTR_TYPE_BIO, NULL, (driver_intr_t *)VBoxGuestFreeBSDISR, pState, &pState->irq_handler);
#else
    rc = bus_setup_intr(self, pState->irq, INTR_TYPE_BIO, (driver_intr_t *)VBoxGuestFreeBSDISR, pState, &pState->irq_handler);
#endif

    if (rc)
    {
        pState->irq_handler = NULL;
        return VERR_DEV_IO_ERROR;
    }

    pState->irq_resid = res_id;

    return VINF_SUCCESS;
}

/**
 * Removes IRQ for VMMDev.
 *
 * @param   self     Pointer to the device info structure.
 * @param   pvState  Opaque pointer to the state info structure.
 */
static void VBoxGuestFreeBSDRemoveIRQ(device_t self, void *pvState)
{
    struct VBoxGuestDeviceState *pState = (struct VBoxGuestDeviceState *)pvState;

    if (pState->irq)
    {
        bus_teardown_intr(self, pState->irq, pState->irq_handler);
        bus_release_resource(self, SYS_RES_IRQ, 0, pState->irq);
    }
}

static int VBoxGuestFreeBSDAttach(device_t self)
{
    int rc = VINF_SUCCESS;
    int res_id = 0;
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

    pState = device_get_softc(self);
    if (!pState)
    {
        pState = malloc(sizeof(struct VBoxGuestDeviceState), M_VBOXDEV, M_NOWAIT | M_ZERO);
        if (!pState)
            return ENOMEM;

        device_set_softc(self, pState);
    }

    /*
     * Initialize the session hash table.
     */
    rc = RTSpinlockCreate(&g_Spinlock);
    if (RT_SUCCESS(rc))
    {
        /*
         * Map the register address space.
         */
        res_id                 =  PCIR_BAR(0);
        pState->io_port        = bus_alloc_resource_any(self, SYS_RES_IOPORT, &res_id, RF_ACTIVE);
        pState->io_port_tag    = rman_get_bustag(pState->io_port);
        pState->io_port_handle = rman_get_bushandle(pState->io_port);
        pState->uIOPortBase    = bus_get_resource_start(self, SYS_RES_IOPORT, res_id);
        pState->io_port_resid  = res_id;
        if (pState->uIOPortBase)
        {
            /*
             * Map the MMIO region.
             */
            res_id = PCIR_BAR(1);
            pState->vmmdevmem        = bus_alloc_resource_any(self, SYS_RES_MEMORY, &res_id, RF_ACTIVE);
            pState->vmmdevmem_tag    = rman_get_bustag(pState->vmmdevmem);
            pState->vmmdevmem_handle = rman_get_bushandle(pState->vmmdevmem);
            pState->vmmdevmem_addr   = bus_get_resource_start(self, SYS_RES_MEMORY, res_id);
            pState->vmmdevmem_size   = bus_get_resource_count(self, SYS_RES_MEMORY, res_id);

            pState->pMMIOBase = (void *)pState->vmmdevmem_handle;
            pState->vmmdevmem_resid = res_id;
            if (pState->pMMIOBase)
            {
                /*
                 * Add IRQ of VMMDev.
                 */
                rc = VBoxGuestFreeBSDAddIRQ(self, pState);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Call the common device extension initializer.
                     */
                    rc = VBoxGuestInitDevExt(&g_DevExt, pState->uIOPortBase, pState->pMMIOBase,
                                             pState->vmmdevmem_size, VBOXOSTYPE_FreeBSD);
                    printf("rc=%d\n", rc);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Create device node.
                         */
                        g_pVBoxAddFreeBSDChrDev = make_dev(&g_VBoxAddFreeBSDChrDevSW,
                                                           0,
                                                           UID_ROOT,
                                                           GID_WHEEL,
                                                           0666,
                                                           DEVICE_NAME);
                        g_pVBoxAddFreeBSDChrDev->si_drv1 = self;
                        return 0;
                    }
                    else
                        printf((DEVICE_NAME ":VBoxGuestInitDevExt failed.\n"));
                    VBoxGuestFreeBSDRemoveIRQ(self, pState);
                }
                else
                    printf((DEVICE_NAME ":VBoxGuestFreeBSDAddIRQ failed.\n"));
            }
            else
                printf((DEVICE_NAME ":MMIO region setup failed.\n"));
        }
        else
            printf((DEVICE_NAME ":IOport setup failed.\n"));
        RTSpinlockDestroy(g_Spinlock);
        g_Spinlock = NIL_RTSPINLOCK;
    }
    else
        printf((DEVICE_NAME ":RTSpinlockCreate failed.\n"));

    RTR0Term();
    return ENXIO;
}

static int VBoxGuestFreeBSDProbe(device_t self)
{
    if ((pci_get_vendor(self) == VMMDEV_VENDORID) && (pci_get_device(self) == VMMDEV_DEVICEID))
    {
        Log(("Found VBox device\n"));
        return 0;
    }

    return ENXIO;
}

static device_method_t VBoxGuestFreeBSDMethods[] =
{
    /* Device interface. */
    DEVMETHOD(device_probe, VBoxGuestFreeBSDProbe),
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

