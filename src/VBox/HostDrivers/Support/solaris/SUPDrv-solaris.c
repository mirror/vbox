/** @file
 * VBox host drivers - Ring-0 support drivers - Solaris host:
 * Solaris driver C code
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include "SUPDRV.h"
#include <iprt/spinlock.h>
#include <iprt/process.h>
#include <iprt/initterm.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The module name. */
#define DEVICE_NAME    "vboxdrv"
#define DEVICE_DESC    "VirtualBox Driver"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int VBoxDrvSolarisOpen(dev_t* pDev, int fFlag, int fType, cred_t* pCred);
static int VBoxDrvSolarisClose(dev_t Dev, int fFlag, int fType, cred_t* pCred);
static int VBoxDrvSolarisRead(dev_t Dev, struct uio* pUio, cred_t* pCred);
static int VBoxDrvSolarisWrite(dev_t Dev, struct uio* pUio, cred_t* pCred);
static int VBoxDrvSolarisIoctl (dev_t Dev, int Cmd, intptr_t Arg, int mode,cred_t* pCred, int* pVal);

static int VBoxDrvSolarisAttach(dev_info_t* pDip, ddi_attach_cmd_t Cmd);
static int VBoxDrvSolarisDetach(dev_info_t* pDip, ddi_detach_cmd_t Cmd);

static int VBoxDrvSolarisErr2Native(int rc);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * cb_ops: for drivers that support char/block entry points
 */
static struct cb_ops g_VBoxDrvSolarisCbOps =
{
    VBoxDrvSolarisOpen,
    VBoxDrvSolarisClose,
    nodev,                  /* b strategy */
    nodev,                  /* b dump */
    nodev,                  /* b print */
    VBoxDrvSolarisRead,
    VBoxDrvSolarisWrite,
    VBoxDrvSolarisIoctl,
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
static struct dev_ops g_VBoxDrvSolarisDevOps =
{
    DEVO_REV,               /* driver build revision */
    0,                      /* ref count */
    nulldev,                /* get info */
    nulldev,                /* identify */
    nulldev,                /* probe */
    VBoxDrvSolarisAttach,
    VBoxDrvSolarisDetach,
    nodev,          /* reset */
    &g_VBoxDrvSolarisCbOps,
    (struct bus_ops *)0,
    nodev                   /* power */
};

/**
 * modldrv: export driver specifics to the kernel
 */
static struct modldrv g_VBoxDrvSolarisModule =
{
    &mod_driverops,         /* extern from kernel */
    DEVICE_DESC,
    &g_VBoxDrvSolarisDevOps
};

/**
 * modlinkage: export install/remove/info to the kernel
 */
static struct modlinkage g_VBoxDrvSolarisModLinkage =
{
    MODREV_1,               /* loadable module system revision */
    &g_VBoxDrvSolarisModule,
    NULL                    /* terminate array of linkage structures */
};

/** Track module instances */
dev_info_t* g_pVBoxDrvSolarisDip;

/** Opaque pointer to state */
static void* g_pVBoxDrvSolarisState;

/** Device extention & session data association structure */
static SUPDRVDEVEXT         g_DevExt;

/* GCC C++ hack. */
unsigned __gxx_personality_v0 = 0xcccccccc;

/** Hash table */
static PSUPDRVSESSION       g_apSessionHashTab[19];
/** Spinlock protecting g_apSessionHashTab. */
static RTSPINLOCK           g_Spinlock = NIL_RTSPINLOCK;
/** Calculates bucket index into g_apSessionHashTab.*/
#define SESSION_HASH(sfn) ((sfn) % RT_ELEMENTS(g_apSessionHashTab))

/**
 * Kernel entry points
 */
int _init (void)
{
    cmn_err(CE_CONT, "VBoxDrvSolaris _init");

    int e = ddi_soft_state_init(&g_pVBoxDrvSolarisState, sizeof (g_pVBoxDrvSolarisDip), 1);
    if (e != 0)
        return e;

    e = mod_install(&g_VBoxDrvSolarisModLinkage);
    if (e != 0)
        ddi_soft_state_fini(&g_pVBoxDrvSolarisState);

    return e;
}

int _fini (void)
{
    cmn_err(CE_CONT, "VBoxDrvSolaris _fini");

    int e = mod_remove(&g_VBoxDrvSolarisModLinkage);
    if (e != 0)
        return e;

    ddi_soft_state_fini(&g_pVBoxDrvSolarisState);
    return e;
}

int _info (struct modinfo* pModInfo)
{
    cmn_err(CE_CONT, "VBoxDrvSolaris _info");
    return mod_info (&g_VBoxDrvSolarisModLinkage, pModInfo);
}

/**
 * User context entry points
 */
static int VBoxDrvSolarisOpen(dev_t *pDev, int fFlag, int fType, cred_t* pCred)
{
    cmn_err(CE_CONT, "VBoxDrvSolarisOpen");

    int                 rc;
    PSUPDRVSESSION      pSession;

    /*
     * Create a new session.
     */
#if 1
    rc = VINF_SUCCESS;
#else
    rc = supdrvCreateSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;
        unsigned        iHash;

        pSession->Uid       = crgetuid(pCred);
        pSession->Gid       = crgetgid(pCred);
        pSession->Process   = RTProcSelf();
        pSession->R0Process = RTR0ProcHandleSelf();

        /*
         * Insert it into the hash table.
         */
        iHash = SESSION_HASH(pSession->Process);
        RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
        pSession->pNextHash = g_apSessionHashTab[iHash];
        g_apSessionHashTab[iHash] = pSession;
        RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    }
#endif
    return VBoxDrvSolarisErr2Native(rc);
}

static int VBoxDrvSolarisClose(dev_t pDev, int flag, int otyp, cred_t* cred)
{
    cmn_err(CE_CONT, "VBoxDrvSolarisClose");
    return DDI_SUCCESS;
}

static int VBoxDrvSolarisRead(dev_t dev, struct uio* pUio, cred_t* credp)
{
    cmn_err(CE_CONT, "VBoxDrvSolarisRead");
    return DDI_SUCCESS;
}

static int VBoxDrvSolarisWrite(dev_t dev, struct uio* pUio, cred_t* credp)
{
    cmn_err(CE_CONT, "VBoxDrvSolarisWrite");
    return DDI_SUCCESS;
}

/**
 * Attach entry point, to attach a device to the system or resume it.
 *
 * @param   pDip            The module structure instance.
 * @param   enmCmd          Attach type (ddi_attach_cmd_t)
 *
 * @return  corresponding solaris error code.
 */
static int VBoxDrvSolarisAttach(dev_info_t* pDip, ddi_attach_cmd_t enmCmd)
{
    cmn_err(CE_CONT, "VBoxDrvSolarisAttach");
    int rc = VINF_SUCCESS;
    int instance = 0;

    switch (enmCmd)
    {
        case DDI_ATTACH:
        {
            instance = ddi_get_instance (pDip);
            g_pVBoxDrvSolarisDip = pDip;

            if (ddi_soft_state_zalloc(g_pVBoxDrvSolarisState, instance) != DDI_SUCCESS)
            {
                cmn_err(CE_NOTE, "VBoxDrvSolarisAttach: state alloc failed");
                return DDI_FAILURE;
            }

            /*
             * Initialize IPRT R0 driver, which internally calls OS-specific r0 init.
             */
            rc = RTR0Init(0);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Initialize the device extension
                 */
                rc = supdrvInitDevExt(&g_DevExt);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Initialize the session hash table.
                     */
                    //memset(g_apSessionHashTab, 0, sizeof(g_apSessionHashTab));
                    //rc = RTSpinlockCreate(&g_Spinlock);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Register ourselves as a character device, pseudo-driver
                         */
                        if (ddi_create_minor_node(g_pVBoxDrvSolarisDip, "0", S_IFCHR,
                                instance, DDI_PSEUDO, 0) == DDI_SUCCESS)
                        {
                            cmn_err(CE_CONT, "VBoxDrvSolarisAttach: successful.");
                            return DDI_SUCCESS;
                        }

                        /* Is this really necessary? */
                        ddi_remove_minor_node(g_pVBoxDrvSolarisDip, NULL);
                        cmn_err(CE_NOTE,"VBoxDrvSolarisAttach: ddi_create_minor_node failed.");

                        //RTSpinlockDestroy(g_Spinlock);
                        //g_Spinlock = NIL_RTSPINLOCK;
                    }
                    else
                        cmn_err(CE_NOTE, "VBoxDrvSolarisAttach: RTSpinlockCreate failed");
                    //supdrvDeleteDevExt(&g_DevExt);
                }
                else
                    cmn_err(CE_NOTE, "VBoxDrvSolarisAttach: supdrvInitDevExt failed");
                RTR0Term ();
            }
            else
                cmn_err(CE_NOTE, "VBoxDrvSolarisAttach: failed to init R0Drv");
            //memset(&g_DevExt, 0, sizeof(g_DevExt));
            break;
        }

        default:
            return DDI_FAILURE;
    }

    return DDI_FAILURE;
}

/**
 * Detach entry point, to detach a device to the system or suspend it.
 *
 * @param   pDip            The module structure instance.
 * @param   enmCmd          Attach type (ddi_attach_cmd_t)
 *
 * @return  corresponding solaris error code.
 */
static int VBoxDrvSolarisDetach(dev_info_t* pDip, ddi_detach_cmd_t enmCmd)
{
    int rc = VINF_SUCCESS;
    int instance = ddi_get_instance(pDip);

    cmn_err(CE_CONT, "VBoxDrvSolarisDetach");
    switch (enmCmd)
    {
        case DDI_DETACH:
        {
            g_pVBoxDrvSolarisDip = NULL;
            ddi_get_soft_state(g_pVBoxDrvSolarisState, instance);
            ddi_remove_minor_node(pDip, NULL);
            ddi_soft_state_free(g_pVBoxDrvSolarisState, instance);
            cmn_err(CE_CONT, "VBoxDrvSolarisDetach: Clean Up Done.");
            
            //rc = supdrvDeleteDevExt(&g_DevExt);
            //AssertRC(rc);

            //rc = RTSpinlockDestroy(g_Spinlock);
            //AssertRC(rc);
            //g_Spinlock = NIL_RTSPINLOCK;

            RTR0Term();

            memset(&g_DevExt, 0, sizeof(g_DevExt));

            return DDI_SUCCESS;
        }

        default:
            return DDI_FAILURE;
    }
}

/**
 * Driver ioctl, an alternate entry point for this character driver.
 *
 * @param   Dev             Device number
 * @param   Cmd             Operation identifier
 * @param   Arg             Arguments from user to driver
 * @param   Mode            Information bitfield (read/write, address space etc)
 * @param   pCred           User credentials
 * @param   pVal            Return value for calling process.
 *
 * @return  corresponding solaris error code.
 */
static int VBoxDrvSolarisIoctl(dev_t Dev, int Cmd, intptr_t Arg, int Mode, cred_t* pCred, int* pVal)
{
    cmn_err(CE_CONT, "VBoxDrvSolarisIoctl");
    return DDI_SUCCESS;
}

/**
 * Converts an supdrv error code to a Solaris error code.
 *
 * @returns corresponding Solaris error code.
 * @param   rc  supdrv error code (SUPDRV_ERR_* defines).
 */
static int VBoxDrvSolarisErr2Native(int rc)
{
    switch (rc)
    {
        case 0:                             return 0;
        case SUPDRV_ERR_GENERAL_FAILURE:    return EACCES;
        case SUPDRV_ERR_INVALID_PARAM:      return EINVAL;
        case SUPDRV_ERR_INVALID_MAGIC:      return EILSEQ;
        case SUPDRV_ERR_INVALID_HANDLE:     return ENXIO;
        case SUPDRV_ERR_INVALID_POINTER:    return EFAULT;
        case SUPDRV_ERR_LOCK_FAILED:        return ENOLCK;
        case SUPDRV_ERR_ALREADY_LOADED:     return EEXIST;
        case SUPDRV_ERR_PERMISSION_DENIED:  return EPERM;
        case SUPDRV_ERR_VERSION_MISMATCH:   return ENOSYS;
    }

    return EPERM;
}
