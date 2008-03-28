/* $Id$ */
/** @file
 * VirtualBox Guest Additions Driver for Solaris.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/mutex.h>
#include <sys/pci.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/ddi_intr.h>
#include <sys/sunddi.h>
#undef u /* /usr/include/sys/user.h:249:1 is where this is defined to (curproc->p_user). very cool. */

#if defined(DEBUG_ramshankar) && !defined(LOG_ENABLED)
#define LOG_ENABLED
#endif
#include "VBoxGuestInternal.h"
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <iprt/mem.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The module name. */
#define DEVICE_NAME              "vboxguest"
/** The module description as seen in 'modinfo'. */
#define DEVICE_DESC              "VirtualBox Guest Driver"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int VBoxGuestSolarisOpen(dev_t *pDev, int fFlag, int fType, cred_t *pCred);
static int VBoxGuestSolarisClose(dev_t Dev, int fFlag, int fType, cred_t *pCred);
static int VBoxGuestSolarisRead(dev_t Dev, struct uio *pUio, cred_t *pCred);
static int VBoxGuestSolarisWrite(dev_t Dev, struct uio *pUio, cred_t *pCred);
static int VBoxGuestSolarisIOCtl(dev_t Dev, int Cmd, intptr_t pArg, int Mode, cred_t *pCred, int *pVal);

static int VBoxGuestSolarisGetInfo(dev_info_t *pDip, ddi_info_cmd_t enmCmd, void *pArg, void **ppResult);
static int VBoxGuestSolarisAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd);
static int VBoxGuestSolarisDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd);

static int VBoxGuestSolarisAddIRQ(dev_info_t *pDip, void *pvState);
static void VBoxGuestSolarisRemoveIRQ(dev_info_t *pDip, void *pvState);
static uint_t VBoxGuestSolarisISR(caddr_t Arg);

DECLVBGL(int) VBoxGuestSolarisServiceCall(void *pvSession, unsigned iCmd, void *pvData, size_t cbData, size_t *pcbDataReturned);
DECLVBGL(void *) VBoxGuestSolarisServiceOpen(uint32_t *pu32Version);
DECLVBGL(int) VBoxGuestSolarisServiceClose(void *pvSession);


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * cb_ops: for drivers that support char/block entry points
 */
static struct cb_ops g_VBoxGuestSolarisCbOps =
{
    VBoxGuestSolarisOpen,
    VBoxGuestSolarisClose,
    nodev,                  /* b strategy */
    nodev,                  /* b dump */
    nodev,                  /* b print */
    VBoxGuestSolarisRead,
    VBoxGuestSolarisWrite,
    VBoxGuestSolarisIOCtl,
    nodev,                  /* c devmap */
    nodev,                  /* c mmap */
    nodev,                  /* c segmap */
    nochpoll,               /* c poll */
    ddi_prop_op,            /* property ops */
    NULL,                   /* streamtab  */
    D_NEW | D_MP,           /* compat. flag */
    CB_REV                  /* revision */
};

/**
 * dev_ops: for driver device operations
 */
static struct dev_ops g_VBoxGuestSolarisDevOps =
{
    DEVO_REV,               /* driver build revision */
    0,                      /* ref count */
    VBoxGuestSolarisGetInfo,
    nulldev,                /* identify */
    nulldev,                /* probe */
    VBoxGuestSolarisAttach,
    VBoxGuestSolarisDetach,
    nodev,                  /* reset */
    &g_VBoxGuestSolarisCbOps,
    (struct bus_ops *)0,
    nodev                   /* power */
};

/**
 * modldrv: export driver specifics to the kernel
 */
static struct modldrv g_VBoxGuestSolarisModule =
{
    &mod_driverops,         /* extern from kernel */
    DEVICE_DESC,
    &g_VBoxGuestSolarisDevOps
};

/**
 * modlinkage: export install/remove/info to the kernel
 */
static struct modlinkage g_VBoxGuestSolarisModLinkage =
{
    MODREV_1,               /* loadable module system revision */
    &g_VBoxGuestSolarisModule,
    NULL                    /* terminate array of linkage structures */
};

/**
 * State info for each open file handle.
 */
typedef struct
{
    /** IO port handle. */
    ddi_acc_handle_t        PciIOHandle;
    /** MMIO handle. */
    ddi_acc_handle_t        PciMMIOHandle;
#if 0
    /** Interrupt block cookie. */
    ddi_iblock_cookie_t     BlockCookie;
#endif
    /** Driver Mutex. */
    kmutex_t                Mtx;
    /** IO Port. */
    uint16_t                uIOPortBase;
    /** Address of the MMIO region.*/
    caddr_t                 pMMIOBase;
    /** Size of the MMIO region. */
    off_t                   cbMMIO;
    /** VMMDev Version. */
    uint32_t                u32Version;
    /** Pointer to the interrupt handle vector */
    ddi_intr_handle_t       *pIntr;
    /** Number of actually allocated interrupt handles */
    size_t                  cIntrAllocated;
#ifndef USE_SESSION_HASH
    /** Pointer to the session handle. */
    PVBOXGUESTSESSION       pSession;
#endif
} vboxguest_state_t;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Device handle (we support only one instance). */
static dev_info_t *g_pDip;

/** Opaque pointer to state */
static void *g_pVBoxGuestSolarisState;

/** Device extention & session data association structure. */
static VBOXGUESTDEVEXT      g_DevExt;
/** Spinlock protecting g_apSessionHashTab. */
static RTSPINLOCK           g_Spinlock = NIL_RTSPINLOCK;
#ifdef USE_SESSION_HASH
/** Hash table */
static PVBOXGUESTSESSION    g_apSessionHashTab[19];
/** Calculates the index into g_apSessionHashTab.*/
#define SESSION_HASH(sfn) ((sfn) % RT_ELEMENTS(g_apSessionHashTab))
#endif /* USE_SESSION_HASH */

/** GCC C++ hack. */
unsigned __gxx_personality_v0 = 0xdecea5ed;

/**
 * Kernel entry points
 */
int _init(void)
{
    LogFlow((DEVICE_NAME ":_init\n"));
    int rc = ddi_soft_state_init(&g_pVBoxGuestSolarisState, sizeof(vboxguest_state_t), 1);
    if (!rc)
    {
        rc = mod_install(&g_VBoxGuestSolarisModLinkage);
        if (rc)
            ddi_soft_state_fini(&g_pVBoxGuestSolarisState);
    }
    return rc;
}


int _fini(void)
{
    LogFlow((DEVICE_NAME ":_fini\n"));
    int rc = mod_remove(&g_VBoxGuestSolarisModLinkage);
    if (!rc)
        ddi_soft_state_fini(&g_pVBoxGuestSolarisState);
    return rc;
}


int _info(struct modinfo *pModInfo)
{
    LogFlow((DEVICE_NAME ":_info\n"));
    return mod_info(&g_VBoxGuestSolarisModLinkage, pModInfo);
}


/**
 * Attach entry point, to attach a device to the system or resume it.
 *
 * @param   pDip            The module structure instance.
 * @param   enmCmd          Attach type (ddi_attach_cmd_t)
 *
 * @return  corresponding solaris error code.
 */
static int VBoxGuestSolarisAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisAttach\n"));
    switch (enmCmd)
    {
        case DDI_ATTACH:
        {
            int rc;
            int instance;
            vboxguest_state_t *pState;

            instance = ddi_get_instance(pDip);
#ifdef USE_SESSION_HASH
            rc = ddi_soft_state_zalloc(g_pVBoxGuestSolarisState, instance);
            if (rc != DDI_SUCCESS)
            {
                Log((DEVICE_NAME ":ddi_soft_state_zalloc failed.\n"));
                return DDI_FAILURE;
            }

            pState = ddi_get_soft_state(g_pVBoxGuestSolarisState, instance);
            if (!pState)
            {
                ddi_soft_state_free(g_pVBoxGuestSolarisState, instance);
                Log((DEVICE_NAME ":ddi_get_soft_state for instance %d failed\n", instance));
                return DDI_FAILURE;
            }
#else
            pState = RTMemAllocZ(sizeof(vboxguest_state_t));
            if (!pState)
            {
                Log((DEVICE_NAME ":RTMemAllocZ failed to allocate %d bytes\n", sizeof(vboxguest_state_t)));
                return DDI_FAILURE;
            }
#endif

            /*
             * Initialize IPRT R0 driver, which internally calls OS-specific r0 init.
             */
            rc = RTR0Init(0);
            if (RT_FAILURE(rc))
            {
                Log((DEVICE_NAME ":RTR0Init failed.\n"));
                return DDI_FAILURE;
            }

            /*
             * Initialize the session hash table.
             */
            rc = RTSpinlockCreate(&g_Spinlock);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Enable resources for PCI access.
                 */
                ddi_acc_handle_t PciHandle;
                rc = pci_config_setup(pDip, &PciHandle);
                if (rc == DDI_SUCCESS)
                {
                    /*
                     * Check vendor and device ID.
                     */
                    uint16_t uVendorID = pci_config_get16(PciHandle, PCI_CONF_VENID);
                    uint16_t uDeviceID = pci_config_get16(PciHandle, PCI_CONF_DEVID);
                    if (   uVendorID == VMMDEV_VENDORID
                        && uDeviceID == VMMDEV_DEVICEID)
                    {
                        /*
                         * Verify PCI class of the device (a bit paranoid).
                         */
                        uint8_t uClass = pci_config_get8(PciHandle, PCI_CONF_BASCLASS);
                        uint8_t uSubClass = pci_config_get8(PciHandle, PCI_CONF_SUBCLASS);
                        if (   uClass == PCI_CLASS_PERIPH
                            && uSubClass == PCI_PERIPH_OTHER)
                        {
                            /*
                             * Map the register address space.
                             */
                            caddr_t baseAddr;
                            ddi_device_acc_attr_t deviceAttr;
                            deviceAttr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
                            deviceAttr.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;
                            deviceAttr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
                            deviceAttr.devacc_attr_access = DDI_DEFAULT_ACC;
                            rc = ddi_regs_map_setup(pDip, 1, &baseAddr, 0, 0, &deviceAttr, &pState->PciIOHandle);
                            if (rc == DDI_SUCCESS)
                            {
                                /*
                                 * Read size of the MMIO region.
                                 */
                                pState->uIOPortBase = (uintptr_t)baseAddr;
                                rc = ddi_dev_regsize(pDip, 2, &pState->cbMMIO);
                                if (rc == DDI_SUCCESS)
                                {
                                    rc = ddi_regs_map_setup(pDip, 2, &pState->pMMIOBase, 0, pState->cbMMIO, &deviceAttr,
                                                &pState->PciMMIOHandle);
                                    if (rc == DDI_SUCCESS)
                                    {
                                        /*
                                         * Add IRQ of VMMDev.
                                         */
                                        rc = VBoxGuestSolarisAddIRQ(pDip, pState);
                                        if (rc == DDI_SUCCESS)
                                        {
                                            /*
                                             * Call the common device extension initializer.
                                             */
                                            rc = VBoxGuestInitDevExt(&g_DevExt, pState->uIOPortBase, pState->pMMIOBase,
                                                        pState->cbMMIO, VBOXOSTYPE_Solaris);
                                            if (RT_SUCCESS(rc))
                                            {
                                                rc = ddi_create_minor_node(pDip, DEVICE_NAME, S_IFCHR, instance, DDI_PSEUDO, 0);
                                                if (rc == DDI_SUCCESS)
                                                {
                                                    g_pDip = pDip;
                                                    ddi_set_driver_private(pDip, pState);
                                                    pci_config_teardown(&PciHandle);
                                                    ddi_report_dev(pDip);
                                                    return DDI_SUCCESS;
                                                }

                                                LogRel((DEVICE_NAME ":ddi_create_minor_node failed.\n"));
                                            }
                                            else
                                                LogRel((DEVICE_NAME ":VBoxGuestInitDevExt failed.\n"));
                                            VBoxGuestSolarisRemoveIRQ(pDip, pState);
                                        }
                                        else
                                            LogRel((DEVICE_NAME ":VBoxGuestSolarisAddIRQ failed.\n"));
                                        ddi_regs_map_free(&pState->PciMMIOHandle);
                                    }
                                    else
                                        LogRel((DEVICE_NAME ":ddi_regs_map_setup for MMIO region failed.\n"));
                                }
                                else
                                    LogRel((DEVICE_NAME ":ddi_dev_regsize for MMIO region failed.\n"));
                                ddi_regs_map_free(&pState->PciIOHandle);
                            }
                            else
                                LogRel((DEVICE_NAME ":ddi_regs_map_setup for IOport failed.\n"));
                        }
                        else
                            LogRel((DEVICE_NAME ":PCI class/sub-class does not match.\n"));
                    }
                    else
                        LogRel((DEVICE_NAME ":PCI vendorID, deviceID does not match.\n"));
                    pci_config_teardown(&PciHandle);
                }
                else
                    LogRel((DEVICE_NAME ":pci_config_setup failed rc=%d.\n", rc));
                RTSpinlockDestroy(g_Spinlock);
                g_Spinlock = NIL_RTSPINLOCK;
            }
            else
                LogRel((DEVICE_NAME ":RTSpinlockCreate failed.\n"));

            RTR0Term();
            return DDI_FAILURE;
        }

        case DDI_RESUME:
        {
            /** @todo implement resume for guest driver. */
            return DDI_SUCCESS;
        }

        default:
            return DDI_FAILURE;
    }
}


/**
 * Detach entry point, to detach a device to the system or suspend it.
 *
 * @param   pDip            The module structure instance.
 * @param   enmCmd          Attach type (ddi_attach_cmd_t)
 *
 * @return  corresponding solaris error code.
 */
static int VBoxGuestSolarisDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisDetach\n"));
    switch (enmCmd)
    {
        case DDI_DETACH:
        {
            int rc;
            int instance = ddi_get_instance(pDip);
#ifdef USE_SESSION_HASH
            vboxguest_state_t *pState = ddi_get_soft_state(g_pVBoxGuestSolarisState, instance);
#else
            vboxguest_state_t *pState = ddi_get_driver_private(g_pDip);
#endif
            if (pState)
            {
                VBoxGuestSolarisRemoveIRQ(pDip, pState);
                ddi_regs_map_free(&pState->PciIOHandle);
                ddi_regs_map_free(&pState->PciMMIOHandle);
                ddi_remove_minor_node(pDip, NULL);
#ifdef USE_SESSION_HASH
                ddi_soft_state_free(g_pVBoxGuestSolarisState, instance);
#else
                RTMemFree(pState);
#endif

                rc = RTSpinlockDestroy(g_Spinlock);
                AssertRC(rc);
                g_Spinlock = NIL_RTSPINLOCK;

                RTR0Term();
                return DDI_SUCCESS;
            }
            Log((DEVICE_NAME ":ddi_get_soft_state failed. Cannot detach instance %d\n", instance));
            return DDI_FAILURE;
        }

        case DDI_SUSPEND:
        {
            /** @todo implement suspend for guest driver. */
            return DDI_SUCCESS;
        }

        default:
            return DDI_FAILURE;
    }
}


/**
 * Info entry point, called by solaris kernel for obtaining driver info.
 *
 * @param   pDip            The module structure instance (do not use).
 * @param   enmCmd          Information request type.
 * @param   pvArg           Type specific argument.
 * @param   ppvResult       Where to store the requested info.
 *
 * @return  corresponding solaris error code.
 */
static int VBoxGuestSolarisGetInfo(dev_info_t *pDip, ddi_info_cmd_t enmCmd, void *pvArg, void **ppvResult)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisGetInfo\n"));

    int rc = DDI_SUCCESS;
    switch (enmCmd)
    {
        case DDI_INFO_DEVT2DEVINFO:
            *ppvResult = (void *)g_pDip;
            break;

        case DDI_INFO_DEVT2INSTANCE:
            *ppvResult = (void *)(uintptr_t)ddi_get_instance(g_pDip);
            break;

        default:
            rc = DDI_FAILURE;
            break;
    }

    NOREF(pvArg);
    return rc;
}


/**
 * User context entry points
 */
static int VBoxGuestSolarisOpen(dev_t *pDev, int fFlag, int fType, cred_t *pCred)
{
    int                 rc;
    PVBOXGUESTSESSION   pSession;

    LogFlow((DEVICE_NAME ":VBoxGuestSolarisOpen\n"));

    /*
     * Verify we are being opened as a character device
     */
    if (fType != OTYP_CHR)
        return EINVAL;

#ifndef USE_SESSION_HASH
    vboxguest_state_t *pState = NULL;
    unsigned iOpenInstance;
    for (iOpenInstance = 0; iOpenInstance < 4096; iOpenInstance++)
    {
        if (    !ddi_get_soft_state(g_pVBoxGuestSolarisState, iOpenInstance) /* faster */
            &&  ddi_soft_state_zalloc(g_pVBoxGuestSolarisState, iOpenInstance) == DDI_SUCCESS)
        {
            pState = ddi_get_soft_state(g_pVBoxGuestSolarisState, iOpenInstance);
            break;
        }
    }
    if (!pState)
    {
        Log((DEVICE_NAME ":VBoxGuestSolarisOpen: too many open instances."));
        return ENXIO;
    }

    /*
     * Create a new session.
     */
    rc = VBoxGuestCreateUserSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        pState->pSession = pSession;
        *pDev = makedevice(getmajor(*pDev), iOpenInstance);
        Log((DEVICE_NAME "VBoxGuestSolarisOpen: pSession=%p pState=%p pid=%d\n", pSession, pState, (int)RTProcSelf()));
        return 0;
    }

    /* Failed, clean up. */
    ddi_soft_state_free(g_pVBoxGuestSolarisState, iOpenInstance);
#else
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

        int instance;
        for (instance = 0; instance < 4096; instance++)
        {
            vboxguest_state_t *pState = ddi_get_soft_state(g_pVBoxGuestSolarisState, instance);
            if (pState)
                break;
        }
        if (instance >= 4096)
        {
            Log((DEVICE_NAME ":VBoxGuestSolarisOpen: All instances exhausted\n"));
            return ENXIO;
        }
        *pDev = makedevice(getmajor(*pDev), instance);
        Log((DEVICE_NAME ":VBoxGuestSolarisOpen success: g_DevExt=%p pSession=%p rc=%d pid=%d\n", &g_DevExt, pSession, rc, (int)RTProcSelf()));
        return 0;
    }
#endif
    LogRel((DEVICE_NAME ":VBoxGuestSolarisOpen: VBoxGuestCreateUserSession failed. rc=%d\n", rc));
    return EFAULT;
}


static int VBoxGuestSolarisClose(dev_t Dev, int flag, int fType, cred_t *pCred)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisClose pid=%d\n", (int)RTProcSelf()));

#ifndef USE_SESSION_HASH
    PVBOXGUESTSESSION pSession;
    vboxguest_state_t *pState = ddi_get_soft_state(g_pVBoxGuestSolarisState, getminor(Dev));
    if (!pState)
    {
        Log((DEVICE_NAME ":VBoxGuestSolarisClose: failed to get pState.\n"));
        return EFAULT;
    }

    pSession = pState->pSession;
    pState->pSession = NULL;
    Log((DEVICE_NAME ":VBoxGuestSolarisClose: pSession=%p pState=%p\n", pSession, pState));
    ddi_soft_state_free(g_pVBoxGuestSolarisState, getminor(Dev));
    if (!pSession)
    {
        Log((DEVICE_NAME ":VBoxGuestSolarisClose: failed to get pSession.\n"));
        return EFAULT;
    }

#else /* USE_SESSION_HASH */
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
        Log((DEVICE_NAME ":VBoxGuestSolarisClose: WHUT?!? pSession == NULL! This must be a mistake... pid=%d", (int)Process));
        return EFAULT;
    }
    Log((DEVICE_NAME ":VBoxGuestSolarisClose: pid=%d\n", (int)Process));
#endif /* USE_SESSION_HASH */

    /*
     * Close the session.
     */
    VBoxGuestCloseSession(&g_DevExt, pSession);
    return 0;
}


static int VBoxGuestSolarisRead(dev_t Dev, struct uio *pUio, cred_t *pCred)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisRead\n"));
    return 0;
}


static int VBoxGuestSolarisWrite(dev_t Dev, struct uio *pUio, cred_t *pCred)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisWrite\n"));
    return 0;
}


/** @def IOCPARM_LEN
 * Gets the length from the ioctl number.
 * This is normally defined by sys/ioccom.h on BSD systems...
 */
#ifndef IOCPARM_LEN
# define IOCPARM_LEN(Code)                      (((Code) >> 16) & IOCPARM_MASK)
#endif


/**
 * Driver ioctl, an alternate entry point for this character driver.
 *
 * @param   Dev             Device number
 * @param   Cmd             Operation identifier
 * @param   pArg            Arguments from user to driver
 * @param   Mode            Information bitfield (read/write, address space etc.)
 * @param   pCred           User credentials
 * @param   pVal            Return value for calling process.
 *
 * @return  corresponding solaris error code.
 */
static int VBoxGuestSolarisIOCtl(dev_t Dev, int Cmd, intptr_t pArg, int Mode, cred_t *pCred, int *pVal)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisIOCtl\n"));

#ifndef USE_SESSION_HASH
    /*
     * Get the session from the soft state item.
     */
    vboxguest_state_t *pState = ddi_get_soft_state(g_pVBoxGuestSolarisState, getminor(Dev));
    if (!pState)
    {
        Log((DEVICE_NAME ":VBoxGuestSolarisIOCtl: no state data for %d\n", getminor(Dev)));
        return EINVAL;
    }

    PVBOXGUESTSESSION pSession = pState->pSession;
    if (!pSession)
    {
        Log((DEVICE_NAME ":VBoxGuestSolarisIOCtl: no session data for %d\n", getminor(Dev)));
        return EINVAL;
    }

#else /* USE_SESSION_HASH */
    RTSPINLOCKTMP       Tmp = RTSPINLOCKTMP_INITIALIZER;
    const RTPROCESS     Process = RTProcSelf();
    const unsigned      iHash = SESSION_HASH(Process);
    PVBOXGUESTSESSION   pSession;

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
        Log((DEVICE_NAME ":VBoxGuestSolarisIOCtl: WHAT?!? pSession == NULL! This must be a mistake... pid=%d iCmd=%#x\n", (int)Process, Cmd));
        return EINVAL;
    }
#endif /* USE_SESSION_HASH */

    /*
     * Read and validate the request wrapper.
     */
    VBGLBIGREQ ReqWrap;
    if (IOCPARM_LEN(Cmd) != sizeof(ReqWrap))
    {
        Log((DEVICE_NAME ": VBoxGuestSolarisIOCtl: bad request %#x size=%d expected=%d\n", Cmd, IOCPARM_LEN(Cmd), sizeof(ReqWrap)));
        return ENOTTY;
    }

    int rc = ddi_copyin((void *)pArg, &ReqWrap, sizeof(ReqWrap), Mode);
    if (RT_UNLIKELY(rc))
    {
        Log((DEVICE_NAME ": VBoxGuestSolarisIOCtl: ddi_copyin failed to read header pArg=%p Cmd=%d. rc=%d.\n", pArg, Cmd, rc));
        return EINVAL;
    }

    if (ReqWrap.u32Magic != VBGLBIGREQ_MAGIC)
    {
        Log((DEVICE_NAME ": VBoxGuestSolarisIOCtl: bad magic %#x; pArg=%p Cmd=%d.\n", ReqWrap.u32Magic, pArg, Cmd));
        return EINVAL;
    }
    if (RT_UNLIKELY(   ReqWrap.cbData == 0
                    || ReqWrap.cbData > _1M*16))
    {
        Log((DEVICE_NAME ": VBoxGuestSolarisIOCtl: bad size %#x; pArg=%p Cmd=%d.\n", ReqWrap.cbData, pArg, Cmd));
        return EINVAL;
    }

    /*
     * Read the request.
     */
    void *pvBuf = RTMemTmpAlloc(ReqWrap.cbData);
    if (RT_UNLIKELY(!pvBuf))
    {
        Log((DEVICE_NAME ":VBoxGuestSolarisIOCtl: RTMemTmpAlloc failed to alloc %d bytes.\n", ReqWrap.cbData));
        return ENOMEM;
    }

    rc = ddi_copyin((void *)(uintptr_t)ReqWrap.pvDataR3, pvBuf, ReqWrap.cbData, Mode);
    if (RT_UNLIKELY(rc))
    {
        RTMemTmpFree(pvBuf);
        Log((DEVICE_NAME ":VBoxGuestSolarisIOCtl: ddi_copyin failed; pvBuf=%p pArg=%p Cmd=%d. rc=%d\n", pvBuf, pArg, Cmd, rc));
        return EFAULT;
    }
    if (RT_UNLIKELY(   ReqWrap.cbData != 0
                    && !VALID_PTR(pvBuf)))
    {
        RTMemTmpFree(pvBuf);
        Log((DEVICE_NAME ":VBoxGuestSolarisIOCtl: pvBuf invalid pointer %p\n", pvBuf));
        return EINVAL;
    }
    Log((DEVICE_NAME ":VBoxGuestSolarisIOCtl: pSession=%p pid=%d.\n", pSession, (int)RTProcSelf()));

    /*
     * Process the IOCtl.
     */
    size_t cbDataReturned;
    rc = VBoxGuestCommonIOCtl(Cmd, &g_DevExt, pSession, pvBuf, ReqWrap.cbData, &cbDataReturned);
    if (RT_SUCCESS(rc))
    {
        rc = 0;
        if (RT_UNLIKELY(cbDataReturned > ReqWrap.cbData))
        {
            Log((DEVICE_NAME ":VBoxGuestSolarisIOCtl: too much output data %d expected %d\n", cbDataReturned, ReqWrap.cbData));
            cbDataReturned = ReqWrap.cbData;
        }
        if (cbDataReturned > 0)
        {
            rc = ddi_copyout(pvBuf, (void *)(uintptr_t)ReqWrap.pvDataR3, cbDataReturned, Mode);
            if (RT_UNLIKELY(rc))
            {
                Log((DEVICE_NAME ":VBoxGuestSolarisIOCtl: ddi_copyout failed; pvBuf=%p pArg=%p Cmd=%d. rc=%d\n", pvBuf, pArg, Cmd, rc));
                rc = EFAULT;
            }
        }
    }
    else
    {
        LogRel((DEVICE_NAME ":VBoxGuestSolarisIOCtl: VBoxGuestCommonIOCtl failed. rc=%d\n", rc));
        rc = EFAULT;
    }
    *pVal = rc;
    RTMemTmpFree(pvBuf);
    return rc;
}


/**
 * Sets IRQ for VMMDev.
 *
 * @returns Solaris error code.
 * @param   pDip     Pointer to the device info structure.
 * @param   pvState  Pointer to the state info structure.
 */
static int VBoxGuestSolarisAddIRQ(dev_info_t *pDip, void *pvState)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisAddIRQ %p\n", pvState));

    vboxguest_state_t *pState = (vboxguest_state_t *)pvState;
#if 0
    /*
     * These calls are supposedly deprecated. But Sun seems to use them all over
     * the place. Anyway, once this works we will switch to the highly elaborate
     * and non-obsolete way of setting up IRQs.
     */
    int rc = ddi_get_iblock_cookie(pDip, 0, &pState->BlockCookie);
    if (rc == DDI_SUCCESS)
    {
        mutex_init(&pState->Mtx, "VBoxGuest Driver Mutex", MUTEX_DRIVER, (void *)pState->BlockCookie);
        rc = ddi_add_intr(pDip, 0, &pState->BlockCookie, NULL, VBoxGuestSolarisISR, (caddr_t)pState);
        if (rc != DDI_SUCCESS)
            Log((DEVICE_NAME ":ddi_add_intr failed. Cannot set IRQ for VMMDev.\n"));
    }
    else
        Log((DEVICE_NAME ":ddi_get_iblock_cookie failed. Cannot set IRQ for VMMDev.\n"));
    return rc;
#else
    int IntrType = 0;
    int rc = ddi_intr_get_supported_types(pDip, &IntrType);
    if (rc == DDI_SUCCESS)
    {
        /* We won't need to bother about MSIs. */
        if (IntrType & DDI_INTR_TYPE_FIXED)
        {
            int IntrCount = 0;
            rc = ddi_intr_get_nintrs(pDip, IntrType, &IntrCount);
            if (   rc == DDI_SUCCESS
                && IntrCount > 0)
            {
                int IntrAvail = 0;
                rc = ddi_intr_get_navail(pDip, IntrType, &IntrAvail);
                if (   rc == DDI_SUCCESS
                    && IntrAvail > 0)
                {
                    /* Allocated kernel memory for the interrupt handles. The allocation size is stored internally. */
                    pState->pIntr = RTMemAlloc(IntrCount * sizeof(ddi_intr_handle_t));
                    if (pState->pIntr)
                    {
                        int IntrAllocated;
                        rc = ddi_intr_alloc(pDip, pState->pIntr, IntrType, 0, IntrCount, &IntrAllocated, DDI_INTR_ALLOC_NORMAL);
                        if (   rc == DDI_SUCCESS
                            && IntrAllocated > 0)
                        {
                            pState->cIntrAllocated = IntrAllocated;
                            uint_t uIntrPriority;
                            rc = ddi_intr_get_pri(pState->pIntr[0], &uIntrPriority);
                            if (rc == DDI_SUCCESS)
                            {
                                /* Initialize the mutex. */
                                mutex_init(&pState->Mtx, "VBoxGuestMtx", MUTEX_DRIVER, DDI_INTR_PRI(uIntrPriority));

                                /* Assign interrupt handler functions and enable interrupts. */
                                for (int i = 0; i < IntrAllocated; i++)
                                {
                                    rc = ddi_intr_add_handler(pState->pIntr[i], (ddi_intr_handler_t *)VBoxGuestSolarisISR,
                                                            (caddr_t)pState, NULL);
                                    if (rc == DDI_SUCCESS)
                                        rc = ddi_intr_enable(pState->pIntr[i]);
                                    if (rc != DDI_SUCCESS)
                                    {
                                        /* Changing local IntrAllocated to hold so-far allocated handles for freeing. */
                                        IntrAllocated = i;
                                        break;
                                    }
                                }
                                if (rc == DDI_SUCCESS)
                                    return rc;

                                /* Remove any assigned handlers */
                                LogRel((DEVICE_NAME ":failed to assign IRQs allocated=%d\n", IntrAllocated));
                                for (int x = 0; x < IntrAllocated; x++)
                                    ddi_intr_remove_handler(pState->pIntr[x]);
                            }
                            else
                                LogRel((DEVICE_NAME ":VBoxGuestSolarisAddIRQ: failed to get priority of interrupt. rc=%d\n", rc));

                            /* Remove allocated IRQs, too bad we can free only one handle at a time. */
                            for (int k = 0; k < pState->cIntrAllocated; k++)
                                ddi_intr_free(pState->pIntr[k]);
                        }
                        else
                            LogRel((DEVICE_NAME ":VBoxGuestSolarisAddIRQ: failed to allocated IRQs. count=%d\n", IntrCount));
                        RTMemFree(pState->pIntr);
                    }
                    else
                        LogRel((DEVICE_NAME ":VBoxGuestSolarisAddIRQ: failed to allocated IRQs. count=%d\n", IntrCount));
                }
                else
                    LogRel((DEVICE_NAME ":VBoxGuestSolarisAddIRQ: failed to get or insufficient available IRQs. rc=%d IntrAvail=%d\n", rc, IntrAvail));
            }
            else
                LogRel((DEVICE_NAME ":VBoxGuestSolarisAddIRQ: failed to get or insufficient number of IRQs. rc=%d IntrCount=%d\n", rc, IntrCount));
        }
        else
            LogRel((DEVICE_NAME ":VBoxGuestSolarisAddIRQ: invalid irq type. IntrType=%#x\n", IntrType));
    }
    else
        LogRel((DEVICE_NAME ":VBoxGuestSolarisAddIRQ: failed to get supported interrupt types\n"));
    return rc;
#endif
}


/**
 * Removes IRQ for VMMDev.
 *
 * @param   pDip     Pointer to the device info structure.
 * @param   pvState  Opaque pointer to the state info structure.
 */
static void VBoxGuestSolarisRemoveIRQ(dev_info_t *pDip, void *pvState)
{
    vboxguest_state_t *pState = (vboxguest_state_t *)pvState;
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisRemoveIRQ pvState=%p\n"));

#if 0
    ddi_remove_intr(pDip, 0, pState->BlockCookie);
    mutex_destroy(&pState->Mtx);
#else
    for (int i = 0; i < pState->cIntrAllocated; i++)
    {
        int rc = ddi_intr_disable(pState->pIntr[i]);
        if (rc == DDI_SUCCESS)
        {
            rc = ddi_intr_remove_handler(pState->pIntr[i]);
            if (rc == DDI_SUCCESS)
                ddi_intr_free(pState->pIntr[i]);
        }
    }
    RTMemFree(pState->pIntr);
    mutex_destroy(&pState->Mtx);
#endif
}


/**
 * Interrupt Service Routine for VMMDev.
 *
 * @returns DDI_INTR_CLAIMED if it's our interrupt, DDI_INTR_UNCLAIMED if it isn't.
 */
static uint_t VBoxGuestSolarisISR(caddr_t Arg)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisISR Arg=%p\n", Arg));

    vboxguest_state_t *pState = (vboxguest_state_t *)Arg;
    mutex_enter(&pState->Mtx);
    bool fOurIRQ = VBoxGuestCommonISR(&g_DevExt);
    mutex_exit(&pState->Mtx);

    return fOurIRQ ? DDI_INTR_CLAIMED : DDI_INTR_UNCLAIMED;
}


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
DECLVBGL(int) VBoxGuestSolarisServiceCall(void *pvSession, unsigned iCmd, void *pvData, size_t cbData, size_t *pcbDataReturned)
{
    LogFlow((DEVICE_NAME ":VBoxGuestSolarisServiceCall %pvSesssion=%p Cmd=%u pvData=%p cbData=%d\n", pvSession, iCmd, pvData, cbData));

    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pvSession;
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertMsgReturn(pSession->pDevExt == &g_DevExt,
                    ("SC: %p != %p\n", pSession->pDevExt, &g_DevExt), VERR_INVALID_HANDLE);

    return VBoxGuestCommonIOCtl(iCmd, &g_DevExt, pSession, pvData, cbData, pcbDataReturned);
}


/**
 * Solaris Guest service open.
 *
 * @returns Opaque pointer to session object.
 * @param   pu32Version         Where to store VMMDev version.
 */
DECLVBGL(void *) VBoxGuestSolarisServiceOpen(uint32_t *pu32Version)
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
 * Solaris Guest service close.
 *
 * @returns VBox error code.
 * @param   pvState             Opaque pointer to the session object.
 */
DECLVBGL(int) VBoxGuestSolarisServiceClose(void *pvSession)
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

