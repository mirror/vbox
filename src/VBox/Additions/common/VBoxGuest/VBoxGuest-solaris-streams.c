/* $Id$ */
/** @file
 * VirtualBox Guest Additions Driver for Solaris.
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


/******************************************************************************
*   Header Files                                                              *
******************************************************************************/

#ifndef TESTCASE
# include <sys/conf.h>
# include <sys/modctl.h>
# include <sys/mutex.h>
# include <sys/pci.h>
# include <sys/stat.h>
# include <sys/ddi.h>
# include <sys/ddi_intr.h>
# include <sys/sunddi.h>
# include <sys/open.h>
# include <sys/sunldi.h>
# include <sys/file.h>
#undef u /* /usr/include/sys/user.h:249:1 is where this is defined to (curproc->p_user). very cool. */
#else  /* TESTCASE */
# undef IN_RING3
# define IN_RING0
#endif  /* TESTCASE */

#include "VBoxGuestInternal.h"
#include <VBox/log.h>
#include <VBox/version.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <iprt/mem.h>
#include <iprt/cdefs.h>
#include <iprt/asm.h>

#ifdef TESTCASE  /* Include this last as we . */
# include "testcase/solaris.h"
# include <iprt/test.h>
#endif  /* TESTCASE */


/******************************************************************************
*   Defined Constants And Macros                                              *
******************************************************************************/

/** The module name. */
#define DEVICE_NAME              "vboxguest"
/** The module description as seen in 'modinfo'. */
#define DEVICE_DESC              "VirtualBox GstDrv"


/******************************************************************************
*   Internal functions used in global structures                              *
******************************************************************************/

static int vboxGuestSolarisOpen(queue_t *pReadQueue, dev_t *pDev, int fFlag,
                                int fMode, cred_t *pCred);
static int vboxGuestSolarisClose(queue_t *pReadQueue, int fFlag, cred_t *pCred);
static int vboxGuestSolarisWPut(queue_t *pWriteQueue, mblk_t *pMBlk);

static int vboxGuestSolarisGetInfo(dev_info_t *pDip, ddi_info_cmd_t enmCmd, void *pArg, void **ppResult);
static int vboxGuestSolarisAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd);
static int vboxGuestSolarisDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd);


/******************************************************************************
*   Driver global structures                                                  *
******************************************************************************/

#ifndef TESTCASE  /* I see no value in including these. */

/*
 * mod_info: STREAMS module information.
 */
static struct module_info g_VBoxGuestSolarisModInfo =
{
    0x0ffff,                /* module id number */
    "vboxguest",
    0,                      /* minimum packet size */
    INFPSZ,                 /* maximum packet size accepted */
    512,                    /* high water mark for data flow control */
    128                     /* low water mark */
};

/*
 * rinit: read queue structure for handling messages coming from below.  In
 * our case this means the host and the virtual hardware, so we do not need
 * the put and service procedures.
 */
static struct qinit g_VBoxGuestSolarisRInit =
{
    NULL,                /* put */
    NULL,                /* service thread procedure */
    vboxGuestSolarisOpen,
    vboxGuestSolarisClose,
    NULL,                /* reserved */
    &g_VBoxGuestSolarisModInfo,
    NULL                 /* module statistics structure */
};

/* 
 * winit: write queue structure for handling messages coming from above.  Above
 * means user space applications: either Guest Additions user space tools or
 * applications reading pointer input.  Messages from the last most likely pass
 * through at least the "consms" console mouse streams module which multiplexes
 * hardware pointer drivers to a single virtual pointer.
 */
static struct qinit g_VBoxGuestSolarisWInit =
{
    vboxGuestSolarisWPut,
    NULL,                   /* service thread procedure */
    NULL,                   /* open */
    NULL,                   /* close */
    NULL,                   /* reserved */
    &g_VBoxGuestSolarisModInfo,
    NULL                    /* module statistics structure */
};

/**
 * streamtab: for drivers that support char/block entry points.
 */
static struct streamtab g_VBoxGuestSolarisStreamTab =
{
    &g_VBoxGuestSolarisRInit,
    &g_VBoxGuestSolarisWInit,
    NULL,                   /* MUX rinit */
    NULL                    /* MUX winit */
};

/**
 * cb_ops: for drivers that support char/block entry points.
 */
static struct cb_ops g_VBoxGuestSolarisCbOps =
{
    nulldev,                /* open */
    nulldev,                /* close */
    nulldev,                /* b strategy */
    nulldev,                /* b dump */
    nulldev,                /* b print */
    nulldev,                /* c read */
    nulldev,                /* c write */
    nulldev,                /* c ioctl */
    nulldev,                /* c devmap */
    nulldev,                /* c mmap */
    nulldev,                /* c segmap */
    nochpoll,               /* c poll */
    ddi_prop_op,            /* property ops */
    g_VBoxGuestSolarisStreamTab,
    D_NEW | D_MP,           /* compat. flag */
};

/**
 * dev_ops: for driver device operations.
 */
static struct dev_ops g_VBoxGuestSolarisDevOps =
{
    DEVO_REV,               /* driver build revision */
    0,                      /* ref count */
    vboxGuestSolarisGetInfo,
    nulldev,                /* identify */
    nulldev,                /* probe */
    vboxGuestSolarisAttach,
    vboxGuestSolarisDetach,
    nodev,                  /* reset */
    &g_VBoxGuestSolarisCbOps,
    (struct bus_ops *)0,
    nodev                   /* power */
};

/**
 * modldrv: export driver specifics to the kernel.
 */
static struct modldrv g_VBoxGuestSolarisModule =
{
    &mod_driverops,         /* extern from kernel */
    DEVICE_DESC " " VBOX_VERSION_STRING "r" RT_XSTR(VBOX_SVN_REV),
    &g_VBoxGuestSolarisDevOps
};

/**
 * modlinkage: export install/remove/info to the kernel.
 */
static struct modlinkage g_VBoxGuestSolarisModLinkage =
{
    MODREV_1,               /* loadable module system revision */
    &g_VBoxGuestSolarisModule,
    NULL                    /* terminate array of linkage structures */
};

#else  /* TESTCASE */
static void *g_VBoxGuestSolarisModLinkage;
#endif  /* TESTCASE */

/**
 * State info for each open file handle.
 */
typedef struct
{
    /** Pointer to the session handle. */
    PVBOXGUESTSESSION  pSession;
    /** The STREAMS write queue which we need for sending messages up to
     * user-space. */
    queue_t           *pWriteQueue;
    /** Our minor number. */
    unsigned           cMinor;
    /* The current greatest horizontal pixel offset on the screen, used for
     * absolute mouse position reporting.
     */
    unsigned           cMaxScreenX;
    /* The current greatest vertical pixel offset on the screen, used for
     * absolute mouse position reporting.
     */
    unsigned           cMaxScreenY;
} vboxguest_state_t;


/******************************************************************************
*   Global Variables                                                          *
******************************************************************************/

/** Device handle (we support only one instance). */
static dev_info_t          *g_pDip = NULL;
/** Opaque pointer to file-descriptor states */
static void                *g_pVBoxGuestSolarisState = NULL;
/** Device extention & session data association structure. */
static VBOXGUESTDEVEXT      g_DevExt;
/** IO port handle. */
static ddi_acc_handle_t     g_PciIOHandle;
/** MMIO handle. */
static ddi_acc_handle_t     g_PciMMIOHandle;
/** IO Port. */
static uint16_t             g_uIOPortBase;
/** Address of the MMIO region.*/
static char                *g_pMMIOBase;  /* Actually caddr_t. */
/** Size of the MMIO region. */
static off_t                g_cbMMIO;
/** Pointer to the interrupt handle vector */
static ddi_intr_handle_t   *g_pIntr;
/** Number of actually allocated interrupt handles */
static size_t               g_cIntrAllocated;
/** The IRQ Mutex */
static kmutex_t             g_IrqMtx;
/** Our global state.
 * @todo Make this into an opaque pointer in the device extension structure.
 * @todo Can't we make do without all these globals anyway?
 */
static vboxguest_state_t   *g_pState;


/******************************************************************************
*   Kernel entry points                                                       *
******************************************************************************/

/** Driver initialisation. */
int _init(void)
{
    /*
     * Initialize IPRT R0 driver, which internally calls OS-specific r0 init.
     */
    int rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        PRTLOGGER pRelLogger;
        static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
        modctl_t *pModCtl;

        rc = RTLogCreate(&pRelLogger, 0 /* fFlags */, "all",
                         "VBOX_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups,
                         RTLOGDEST_STDOUT | RTLOGDEST_DEBUGGER, NULL);
        if (RT_SUCCESS(rc))
            RTLogRelSetDefaultInstance(pRelLogger);
        else
            cmn_err(CE_NOTE, "failed to initialize driver logging rc=%d!\n", rc);

        /*
         * Prevent module autounloading.
         */
        pModCtl = mod_getctl(&g_VBoxGuestSolarisModLinkage);
        if (pModCtl)
            pModCtl->mod_loadflags |= MOD_NOAUTOUNLOAD;
        else
            LogRel((DEVICE_NAME ":failed to disable autounloading!\n"));

        rc = ddi_soft_state_init(&g_pVBoxGuestSolarisState, sizeof(vboxguest_state_t), 1);
        if (!rc)
        {
            rc = mod_install(&g_VBoxGuestSolarisModLinkage);
            if (rc)
                ddi_soft_state_fini(&g_pVBoxGuestSolarisState);
        }
    }
    else
    {
        cmn_err(CE_NOTE, "_init: RTR0Init failed. rc=%d\n", rc);
        return EINVAL;
    }

    return rc;
}


#ifdef TESTCASE
/* Nothing in these three really worth testing, plus we would have to stub
 * around the IPRT log functions. */
#endif


/** Driver cleanup. */
int _fini(void)
{
    int rc;

    LogFlow((DEVICE_NAME ":_fini\n"));
    rc = mod_remove(&g_VBoxGuestSolarisModLinkage);
    if (!rc)
        ddi_soft_state_fini(&g_pVBoxGuestSolarisState);

    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogDestroy(RTLogSetDefaultInstance(NULL));

    RTR0Term();
    return rc;
}


/** Driver identification. */
int _info(struct modinfo *pModInfo)
{
    LogFlow((DEVICE_NAME ":_info\n"));
    return mod_info(&g_VBoxGuestSolarisModLinkage, pModInfo);
}


/******************************************************************************
*   Main code                                                                 *
******************************************************************************/

/**
 * Open callback for the read queue, which we use as a generic device open
 * handler.
 */
int vboxGuestSolarisOpen(queue_t *pReadQueue, dev_t *pDev, int fFlag, int fMode,
                         cred_t *pCred)
{
    int                 rc;
    PVBOXGUESTSESSION   pSession = NULL;
    vboxguest_state_t *pState = NULL;
    unsigned cInstance;

    NOREF(fFlag);
    NOREF(pCred);
    LogFlow((DEVICE_NAME "::Open\n"));

    /*
     * Sanity check on the mode parameter.
     */
    if (fMode)
        return EINVAL;

    for (cInstance = 0; cInstance < 4096; cInstance++)
    {
        if (    !ddi_get_soft_state(g_pVBoxGuestSolarisState, cInstance) /* faster */
            &&  ddi_soft_state_zalloc(g_pVBoxGuestSolarisState, cInstance) == DDI_SUCCESS)
        {
            pState = ddi_get_soft_state(g_pVBoxGuestSolarisState, cInstance);
            break;
        }
    }
    if (!pState)
    {
        Log((DEVICE_NAME "::Open: too many open instances."));
        return ENXIO;
    }

    /*
     * Create a new session.
     */
    rc = VBoxGuestCreateUserSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        pState->pSession = pSession;
        *pDev = makedevice(getmajor(*pDev), cInstance);
        /* Initialise user data for the queues to our state and vice-versa. */
        WR(pReadQueue)->q_ptr = (char *)pState;
        pReadQueue->q_ptr = (char *)pState;
        pState->pWriteQueue = WR(pReadQueue);
        pState->cMinor = cInstance;
        g_pState = pState;
        qprocson(pState->pWriteQueue);
        Log((DEVICE_NAME "::Open: pSession=%p pState=%p pid=%d\n", pSession, pState, (int)RTProcSelf()));
        return 0;
    }

    /* Failed, clean up. */
    ddi_soft_state_free(g_pVBoxGuestSolarisState, cInstance);

    LogRel((DEVICE_NAME "::Open: VBoxGuestCreateUserSession failed. rc=%d\n", rc));
    return EFAULT;
}


/**
 * Close callback for the read queue, which we use as a generic device close
 * handler.
 */
int vboxGuestSolarisClose(queue_t *pReadQueue, int fFlag, cred_t *pCred)
{
    PVBOXGUESTSESSION pSession = NULL;
    vboxguest_state_t *pState = (vboxguest_state_t *)pReadQueue->q_ptr;

    LogFlow((DEVICE_NAME "::Close pid=%d\n", (int)RTProcSelf()));
    NOREF(fFlag);
    NOREF(pCred);

    if (!pState)
    {
        Log((DEVICE_NAME "::Close: failed to get pState.\n"));
        return EFAULT;
    }
    qprocsoff(pState->pWriteQueue);
    pState->pWriteQueue = NULL;
    g_pState = NULL;
    pReadQueue->q_ptr = NULL;

    pSession = pState->pSession;
    pState->pSession = NULL;
    Log((DEVICE_NAME "::Close: pSession=%p pState=%p\n", pSession, pState));
    ddi_soft_state_free(g_pVBoxGuestSolarisState, pState->cMinor);
    if (!pSession)
    {
        Log((DEVICE_NAME "::Close: failed to get pSession.\n"));
        return EFAULT;
    }

    /*
     * Close the session.
     */
    VBoxGuestCloseSession(&g_DevExt, pSession);
    return 0;
}


/* Helper for vboxGuestSolarisWPut. */
static int vboxGuestSolarisDispatchIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk);

/**
 * Handler for messages sent from above (user-space and upper modules) which
 * land in our write queue.
 */
int vboxGuestSolarisWPut(queue_t *pWriteQueue, mblk_t *pMBlk)
{
    vboxguest_state_t *pState = (vboxguest_state_t *)pWriteQueue->q_ptr;
    
    LogFlowFunc(("\n"));
    switch (pMBlk->b_datap->db_type)
    {
        case M_FLUSH:
            /* Flush the write queue if so requested. */
            if (*pMBlk->b_rptr & FLUSHW)
                flushq(pWriteQueue, FLUSHDATA);

            /* Flush the read queue if so requested. */
            if (*pMBlk->b_rptr & FLUSHR)
                flushq(RD(pWriteQueue), FLUSHDATA);

            /* We have no one below us to pass the message on to. */
            return 0;
        /* M_IOCDATA is additional data attached to (at least) transparent
         * IOCtls.  We handle the two together here and separate them further
         * down. */
        case M_IOCTL:
        case M_IOCDATA:
        {
            int err = vboxGuestSolarisDispatchIOCtl(pWriteQueue, pMBlk);
            if (!err)
                qreply(pWriteQueue, pMBlk);
            else
                miocnak(pWriteQueue, pMBlk, 0, err);
            break;
        }
    }
    return 0;
}


/** Data transfer direction of an IOCtl.  This is used for describing
 * transparent IOCtls, and @a UNSPECIFIED is not a valid value for them. */
enum IOCTLDIRECTION
{
    /** This IOCtl transfers no data. */
    NONE,
    /** This IOCtl only transfers data from user to kernel. */
    IN,
    /** This IOCtl only transfers data from kernel to user. */
    OUT,
    /** This IOCtl transfers data from user to kernel and back. */
    BOTH,
    /** We aren't saying anything about how the IOCtl transfers data. */
    UNSPECIFIED
};

/**
 * IOCtl handler function.
 * @returns 0 on success, error code on failure.
 * @param cCmd      The IOCtl command number.
 * @param pvData    Buffer for the user data.
 * @param cbBuffer  Size of the buffer in @a pvData or zero.
 * @param pcbData   Where to set the size of the data returned.  Required for
 *                  handlers which return data.
 * @param prc       Where to store the return code.  Default is zero.  Only
 *                  used for IOCtls without data for convenience of
 *                  implemention.
 */
typedef int FNVBOXGUESTSOLARISIOCTL(vboxguest_state_t *pState, int cCmd,
                                    void *pvData, size_t cbBuffer,
                                    size_t *pcbData, int *prc);
typedef FNVBOXGUESTSOLARISIOCTL *PFNVBOXGUESTSOLARISIOCTL;

/* Helpers for vboxGuestSolarisDispatchIOCtl. */
static int vboxGuestSolarisHandleIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk,
                                       PFNVBOXGUESTSOLARISIOCTL pfnHandler,
                                       int cCmd, size_t cbTransparent,
                                       enum IOCTLDIRECTION enmDirection);
static int vboxGuestSolarisVUIDIOCtl(vboxguest_state_t *pState, int cCmd,
                                     void *pvData, size_t cbBuffer,
                                     size_t *pcbData, int *prc);
static int vboxGuestSolarisGuestIOCtl(vboxguest_state_t *pState, int cCmd,
                                      void *pvData, size_t cbBuffer,
                                      size_t *pcbData, int *prc);

/** Table of supported VUID IOCtls. */
struct
{
    /** The IOCtl number. */
    int cCmd;
    /** The size of the buffer which needs to be copied between user and kernel
     * space, or zero if unknown (must be known for tranparent IOCtls). */
    size_t cbBuffer;
    /** The direction the buffer data needs to be copied.  This must be
     * specified for transparent IOCtls. */
    enum IOCTLDIRECTION enmDirection;
} s_aVUIDIOCtlDescriptions[] =
{
   { VUIDGFORMAT,     sizeof(int),                  OUT         },
   { VUIDSFORMAT,     0,                            NONE        },
   { VUIDGADDR,       0,                            UNSPECIFIED },
   { MSIOGETPARMS,    sizeof(Ms_parms),             OUT         },
   { MSIOSETPARMS,    0,                            NONE        },
   { MSIOSRESOLUTION, sizeof(Ms_screen_resolution), IN          },
   { MSIOBUTTONS,     sizeof(int),                  OUT         },
   { VUIDGWHEELCOUNT, sizeof(int),                  OUT         },
   { VUIDGWHEELINFO,  0,                            UNSPECIFIED },
   { VUIDGWHEELSTATE, 0,                            UNSPECIFIED },
   { VUIDSWHEELSTATE, 0,                            UNSPECIFIED }
};

/**
 * Handle a STREAMS IOCtl message for our driver on the write stream.  This
 * function takes care of the IOCtl logic only and does not call qreply() or
 * miocnak() at all - the caller must call these on success or failure
 * respectively.
 * @returns  0 on success or the IOCtl error code on failure.
 * @param  pWriteQueue  pointer to the STREAMS write queue structure.
 * @param  pMBlk        pointer to the STREAMS message block structure.
 */
static int vboxGuestSolarisDispatchIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk)
{
    struct iocblk *pIOCBlk = (struct iocblk *)pMBlk->b_rptr;
    int cCmd = pIOCBlk->ioc_cmd, cCmdType = (cCmd >> 8) & ~0xff;
    size_t cbBuffer;
    enum IOCTLDIRECTION enmDirection;

    LogFlowFunc(("cCmdType=%c, cCmd=%d\n", cCmdType, cCmd));
    switch (cCmdType)
    {
        case MSIOC:
        case VUIOC:
        {
            unsigned i;
            
            for (i = 0; i < RT_ELEMENTS(s_aVUIDIOCtlDescriptions); ++i)
                if (s_aVUIDIOCtlDescriptions[i].cCmd == cCmd)
                {
                    cbBuffer     = s_aVUIDIOCtlDescriptions[i].cbBuffer;
                    enmDirection = s_aVUIDIOCtlDescriptions[i].enmDirection;
                    return vboxGuestSolarisHandleIOCtl(pWriteQueue, pMBlk,
                                                     vboxGuestSolarisVUIDIOCtl,
                                                       cCmd, cbBuffer,
                                                       enmDirection);
                }
            return EINVAL;
        }
        case 'V':
            return vboxGuestSolarisHandleIOCtl(pWriteQueue, pMBlk,
                                               vboxGuestSolarisGuestIOCtl,
                                               cCmd, 0, UNSPECIFIED);
        default:
            return ENOTTY;
    }
}


/* Helpers for vboxGuestSolarisHandleIOCtl. */
static int vboxGuestSolarisHandleIOCtlData
               (queue_t *pWriteQueue, mblk_t *pMBlk,
                PFNVBOXGUESTSOLARISIOCTL pfnHandler, int cCmd,
                size_t cbTransparent, enum IOCTLDIRECTION enmDirection);

static int vboxGuestSolarisHandleTransparentIOCtl
                (queue_t *pWriteQueue, mblk_t *pMBlk,
                 PFNVBOXGUESTSOLARISIOCTL pfnHandler, int cCmd,
                size_t cbTransparent, enum IOCTLDIRECTION enmDirection);

static int vboxGuestSolarisHandleIStrIOCtl
                (queue_t *pWriteQueue, mblk_t *pMBlk,
                 PFNVBOXGUESTSOLARISIOCTL pfnHandler, int cCmd);

/**
 * Generic code for handling STREAMS-specific IOCtl logic and boilerplate.  It
 * calls the IOCtl handler passed to it without the handler having to be aware
 * of STREAMS structures, or whether this is a transparent (traditional) or an
 * I_STR (using a STREAMS structure to describe the data) IOCtl.  With the
 * caveat that we only support transparent IOCtls which pass all data in a
 * single buffer of a fixed size (I_STR IOCtls are restricted to a single
 * buffer anyway, but the caller can choose the buffer size).
 * @returns  0 on success or the IOCtl error code on failure.
 * @param  pWriteQueue    pointer to the STREAMS write queue structure.
 * @param  pMBlk          pointer to the STREAMS message block structure.
 * @param  pfnHandler     pointer to the right IOCtl handler function for this
 *                        IOCtl number.
 * @param  cCmd           IOCtl command number.
 * @param  cbTransparent  size of the user space buffer for this IOCtl number,
 *                        used for processing transparent IOCtls.  Pass zero
 *                        for IOCtls with no maximum buffer size (which will
 *                        not be able to be handled as transparent) or with
 *                        no argument.
 * @param  enmDirection   data transfer direction of the IOCtl.
 */
static int vboxGuestSolarisHandleIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk,
                                       PFNVBOXGUESTSOLARISIOCTL pfnHandler,
                                       int cCmd, size_t cbTransparent,
                                       enum IOCTLDIRECTION enmDirection)
{
    struct iocblk *pIOCBlk = (struct iocblk *)pMBlk->b_rptr;
    vboxguest_state_t *pState = (vboxguest_state_t *)pWriteQueue->q_ptr;

    if (pMBlk->b_datap->db_type == M_IOCDATA)
        return vboxGuestSolarisHandleIOCtlData(pWriteQueue, pMBlk, pfnHandler,
                                               cCmd, cbTransparent,
                                               enmDirection);
    else if (pIOCBlk->ioc_count == TRANSPARENT)
        return vboxGuestSolarisHandleTransparentIOCtl(pWriteQueue, pMBlk,
                                                      pfnHandler, cCmd,
                                                      cbTransparent,
                                                      enmDirection);
    else
        return vboxGuestSolarisHandleIStrIOCtl(pWriteQueue, pMBlk, pfnHandler,
                                               cCmd);
}


/**
 * Helper for vboxGuestSolarisHandleIOCtl.  This rather complicated-looking
 * code is basically the standard boilerplate for handling any streams IOCtl
 * additional data, which we currently only use for transparent IOCtls.
 * @copydoc vboxGuestSolarisHandleIOCtl
 */
static int vboxGuestSolarisHandleIOCtlData(queue_t *pWriteQueue, mblk_t *pMBlk,
                                           PFNVBOXGUESTSOLARISIOCTL pfnHandler,
                                           int cCmd, size_t cbTransparent,
                                           enum IOCTLDIRECTION enmDirection)
{
    struct copyresp *pCopyResp = (struct copyresp *)pMBlk->b_rptr;
    struct iocblk   *pIOCBlk   = (struct iocblk *)pMBlk->b_rptr;
    vboxguest_state_t *pState = (vboxguest_state_t *)pWriteQueue->q_ptr;

    if (pCopyResp->cp_rval)
    {
        freemsg(pMBlk);
        return EAGAIN;  /* cp_rval is a pointer but should be the error. */
    }
    if ((pCopyResp->cp_private && enmDirection == BOTH) || enmDirection == IN)
    {
        size_t cbBuffer = pIOCBlk->ioc_count, cbData = 0;
        void *pvData = NULL;
        int err;

        if (cbData < cbTransparent)
            return EINVAL;
        if (!pMBlk->b_cont)
            return EINVAL;
        if (enmDirection == BOTH && !pCopyResp->cp_private)
            return EINVAL;
        pvData = pMBlk->b_cont->b_rptr;
        err = pfnHandler(pState, cCmd, pvData, cbBuffer, &cbData, NULL);
        if (!err && enmDirection == BOTH)
            mcopyout(pMBlk, NULL, cbData, pCopyResp->cp_private, NULL);
        else if (!err && enmDirection == IN)
            miocack(pWriteQueue, pMBlk, 0, 0);
        return err;
    }
    else
    {
        AssertReturn(enmDirection == OUT || enmDirection == BOTH, EINVAL);
        miocack(pWriteQueue, pMBlk, 0, 0);
        return 0;
    }
}

/**
 * Helper for vboxGuestSolarisHandleIOCtl.  This rather complicated-looking
 * code is basically the standard boilerplate for handling transparent IOCtls,
 * that is, IOCtls which are not re-packed inside STREAMS IOCtls.
 * @copydoc vboxGuestSolarisHandleIOCtl
 */
int vboxGuestSolarisHandleTransparentIOCtl
                (queue_t *pWriteQueue, mblk_t *pMBlk,
                 PFNVBOXGUESTSOLARISIOCTL pfnHandler, int cCmd,
                 size_t cbTransparent, enum IOCTLDIRECTION enmDirection)
{
    int err = 0, rc = 0;
    size_t cbData = 0;
    vboxguest_state_t *pState = (vboxguest_state_t *)pWriteQueue->q_ptr;

    if (   (enmDirection != NONE && !pMBlk->b_cont)
        || enmDirection == UNSPECIFIED)
        return EINVAL;
    if (enmDirection == IN || enmDirection == BOTH)
    {
        void *pUserAddr = NULL;
        /* We only need state data if there is something to copy back. */
        if (enmDirection == BOTH)
            pUserAddr = *(void **)pMBlk->b_cont->b_rptr;
	    mcopyin(pMBlk, pUserAddr /* state data */, cbTransparent, NULL);
	}
	else if (enmDirection == OUT)
    {
        mblk_t *pMBlkOut = allocb(cbOut, BPRI_MED);
        void *pvData;

        if (!pMBlkOut)
            return EAGAIN;
        pvData = pMBlkOut->b_rptr;
        err = pfnHandler(pState, cCmd, pvData, cbTransparent, &cbData, NULL);
        if (!err)
            mcopyout(pMBlk, NULL, cbData, NULL, pMBlkOut);
        else
            freemsg(pMBlkOut);
    }
    else
    {
        AssertReturn(enmDirection == NONE, EINVAL);
        err = pfnHandler(pState, cCmd, NULL, 0, NULL, &rc);
        if (!err)
            miocack(pWriteQueue, pMBlk, 0, rc);
    }
    return err;
}
                 
/**
 * Helper for vboxGuestSolarisHandleIOCtl.  This rather complicated-looking
 * code is basically the standard boilerplate for handling any streams IOCtl.
 * @copydoc vboxGuestSolarisHandleIOCtl
 */
static int vboxGuestSolarisHandleIStrIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk,
                                           PFNVBOXGUESTSOLARISIOCTL pfnHandler,
                                           int cCmd)
{
    struct iocblk *pIOCBlk = (struct iocblk *)pMBlk->b_rptr;
    vboxguest_state_t *pState = (vboxguest_state_t *)pWriteQueue->q_ptr;
    uint_t cbBuffer = pIOCBlk->ioc_count;
    void *pvData = NULL;
    int err, rc = 0;
    size_t cbData = 0;
    
    if (cbBuffer && !pMBlk->b_cont)
        return EINVAL;
    /* Repack the whole buffer into a single message block if needed. */
    if (cbBuffer)
    {
        err = miocpullup(pMBlk, cbBuffer);
        if (err)
            return err;
        pvData = pMBlk->b_cont->b_rptr;
    }
    err = pfnHandler(pState, cCmd, pvData, cbBuffer, &cbData, &rc);
    if (!err)
        miocack(pWriteQueue, pMBlk, cbData, rc);
    return err;
}


/**
 * Handle a VUID input device IOCtl.
 * @copydoc FNVBOXGUESTSOLARISIOCTL
 */
static int vboxGuestSolarisVUIDIOCtl(vboxguest_state_t *pState, int cCmd,
                                     void *pvData, size_t cbBuffer,
                                     size_t *pcbData, int *prc)
{
    LogFlowFunc((": " /* no '\n' */));
    switch (cCmd)
    {
        case VUIDGFORMAT:
        {
            LogFlowFunc(("VUIDGFORMAT\n"));
            AssertReturn(cbBuffer >= sizeof(int), EINVAL);
            *(int *)pvData = VUID_FIRM_EVENT;
            *pcbData = sizeof(int);
            return 0;
        }
        case VUIDSFORMAT:
            LogFlowFunc(("VUIDSFORMAT\n"));
            /* We define our native format to be VUID_FIRM_EVENT, so there
             * is nothing more to do and we exit here on success or on
             * failure. */
            return 0;
        case VUIDGADDR:
        case VUIDSADDR:
            LogFlowFunc(("VUIDGADDR/VUIDSADDR\n"));
            return ENOTTY;
        case MSIOGETPARMS:
        {
            Ms_parms parms = { 0 };

            LogFlowFunc(("MSIOGETPARMS\n"));
            AssertReturn(cbBuffer >= sizeof(Ms_parms), EINVAL);
            *(Ms_parms *)pvData = parms;
            *pcbData = sizeof(Ms_parms);
            return 0;
        }
        case MSIOSETPARMS:
            LogFlowFunc(("MSIOSETPARMS\n"));
            return 0;
        case MSIOSRESOLUTION:
        {
            Ms_screen_resolution *pResolution = (Ms_screen_resolution *)pvData;
            LogFlowFunc(("MSIOSRESOLUTION\n"));
            AssertReturn(cbBuffer >= sizeof(Ms_screen_resolution), EINVAL);
            pState->cMaxScreenX = pResolution->width  - 1;
            pState->cMaxScreenY = pResolution->height - 1;
            return 0;
        }
        case MSIOBUTTONS:
        {
            LogFlowFunc(("MSIOBUTTONS\n"));
            AssertReturn(cbBuffer >= sizeof(int), EINVAL);
            *(int *)pvData = 0;
            *pcbData = sizeof(int);
            return 0;
        }
        case VUIDGWHEELCOUNT:
        {
            LogFlowFunc(("VUIDGWHEELCOUNT\n"));
            AssertReturn(cbBuffer >= sizeof(int), EINVAL);
            *(int *)pvData = 0;
            *pcbData = sizeof(int);
            return 0;
        }
        case VUIDGWHEELINFO:
        case VUIDGWHEELSTATE:
        case VUIDSWHEELSTATE:
            LogFlowFunc(("VUIDGWHEELINFO/VUIDGWHEELSTATE/VUIDSWHEELSTATE\n"));
            return EINVAL;
        default:
            LogFlowFunc(("Invalid IOCtl command %x\n", cCmd));
            return EINVAL;
    }
}


/**
 * Handle a VBoxGuest IOCtl.
 * @copydoc FNVBOXGUESTSOLARISIOCTL
 */
static int vboxGuestSolarisGuestIOCtl(vboxguest_state_t *pState, int cCmd,
                                      void *pvData, size_t cbBuffer,
                                      size_t *pcbData, int *prc)
{
    int rc = VBoxGuestCommonIOCtl(cCmd, &g_DevExt, pState->pSession, pvData, cbBuffer, pcbData);
    if (RT_SUCCESS(rc))
    {
        *prc = rc;
        return 0;
    }
    else
    {
        /*
         * We Log() instead of LogRel() here because VBOXGUEST_IOCTL_WAITEVENT can return VERR_TIMEOUT,
         * VBOXGUEST_IOCTL_CANCEL_ALL_EVENTS can return VERR_INTERRUPTED and possibly more in the future;
         * which are not really failures that require logging.
         */
        Log((DEVICE_NAME "::IOCtl: VBoxGuestCommonIOCtl failed. Cmd=%#x rc=%d\n", cCmd, rc));
        rc = RTErrConvertToErrno(rc);
        return rc;
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
int vboxGuestSolarisGetInfo(dev_info_t *pDip, ddi_info_cmd_t enmCmd,
                            void *pvArg, void **ppvResult)
{
    int rc = DDI_SUCCESS;

    LogFlow((DEVICE_NAME "::GetInfo\n"));
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


/* Helpers for vboxGuestSolarisAttach and vboxGuestSolarisDetach. */
static int vboxGuestSolarisAddIRQ(dev_info_t *pDip);
static void vboxGuestSolarisRemoveIRQ(dev_info_t *pDip);

/**
 * Attach entry point, to attach a device to the system or resume it.
 *
 * @param   pDip            The module structure instance.
 * @param   enmCmd          Attach type (ddi_attach_cmd_t)
 *
 * @return  corresponding solaris error code.
 */
int vboxGuestSolarisAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd)
{
    LogFlow((DEVICE_NAME "::Attach\n"));
    switch (enmCmd)
    {
        case DDI_ATTACH:
        {
            int instance, rc;
            ddi_acc_handle_t PciHandle;

            if (g_pDip)
            {
                LogRel((DEVICE_NAME "::Attach: Only one instance supported.\n"));
                return DDI_FAILURE;
            }
            instance = ddi_get_instance(pDip);

            /*
             * Enable resources for PCI access.
             */
            rc = pci_config_setup(pDip, &PciHandle);
            if (rc == DDI_SUCCESS)
            {
                /*
                 * Map the register address space.
                 */
                char *baseAddr;  /* Actually caddr_t. */
                ddi_device_acc_attr_t deviceAttr;
                deviceAttr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
                deviceAttr.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;
                deviceAttr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
                deviceAttr.devacc_attr_access = DDI_DEFAULT_ACC;
                rc = ddi_regs_map_setup(pDip, 1, &baseAddr, 0, 0, &deviceAttr, &g_PciIOHandle);
                if (rc == DDI_SUCCESS)
                {
                    /*
                     * Read size of the MMIO region.
                     */
                    g_uIOPortBase = (uintptr_t)baseAddr;
                    rc = ddi_dev_regsize(pDip, 2, &g_cbMMIO);
                    if (rc == DDI_SUCCESS)
                    {
                        rc = ddi_regs_map_setup(pDip, 2, &g_pMMIOBase, 0, g_cbMMIO, &deviceAttr,
                                        &g_PciMMIOHandle);
                        if (rc == DDI_SUCCESS)
                        {
                            /*
                             * Add IRQ of VMMDev.
                             */
                            rc = vboxGuestSolarisAddIRQ(pDip);
                            if (rc == DDI_SUCCESS)
                            {
                                /*
                                 * Call the common device extension initializer.
                                 */
#if ARCH_BITS == 64
# define VBOXGUEST_OS_TYPE VBOXOSTYPE_Solaris_x64
#else
# define VBOXGUEST_OS_TYPE VBOXOSTYPE_Solaris
#endif
                                rc = VBoxGuestInitDevExt(&g_DevExt,
                                                         g_uIOPortBase,
                                                         g_pMMIOBase, g_cbMMIO,
                                                         VBOXGUEST_OS_TYPE,
                                          VMMDEV_EVENT_MOUSE_POSITION_CHANGED);
#undef VBOXGUEST_OS_TYPE
                                if (RT_SUCCESS(rc))
                                {
                                    rc = ddi_create_minor_node(pDip, DEVICE_NAME, S_IFCHR, instance, DDI_PSEUDO, 0);
                                    if (rc == DDI_SUCCESS)
                                    {
                                        g_pDip = pDip;
                                        pci_config_teardown(&PciHandle);
                                        return DDI_SUCCESS;
                                    }

                                    LogRel((DEVICE_NAME "::Attach: ddi_create_minor_node failed.\n"));
                                    VBoxGuestDeleteDevExt(&g_DevExt);
                                }
                                else
                                    LogRel((DEVICE_NAME "::Attach: VBoxGuestInitDevExt failed.\n"));
                                vboxGuestSolarisRemoveIRQ(pDip);
                            }
                            else
                                LogRel((DEVICE_NAME "::Attach: vboxGuestSolarisAddIRQ failed.\n"));
                            ddi_regs_map_free(&g_PciMMIOHandle);
                        }
                        else
                            LogRel((DEVICE_NAME "::Attach: ddi_regs_map_setup for MMIO region failed.\n"));
                    }
                    else
                        LogRel((DEVICE_NAME "::Attach: ddi_dev_regsize for MMIO region failed.\n"));
                    ddi_regs_map_free(&g_PciIOHandle);
                }
                else
                    LogRel((DEVICE_NAME "::Attach: ddi_regs_map_setup for IOport failed.\n"));
                pci_config_teardown(&PciHandle);
            }
            else
                LogRel((DEVICE_NAME "::Attach: pci_config_setup failed rc=%d.\n", rc));
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
int vboxGuestSolarisDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd)
{
    LogFlow((DEVICE_NAME "::Detach\n"));
    switch (enmCmd)
    {
        case DDI_DETACH:
        {
            vboxGuestSolarisRemoveIRQ(pDip);
            ddi_regs_map_free(&g_PciIOHandle);
            ddi_regs_map_free(&g_PciMMIOHandle);
            ddi_remove_minor_node(pDip, NULL);
            VBoxGuestDeleteDevExt(&g_DevExt);
            g_pDip = NULL;
            return DDI_SUCCESS;
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


/* Interrupt service routine installed by vboxGuestSolarisAddIRQ. */
static uint_t vboxGuestSolarisISR(char *Arg /* Actually caddr_t. */);

/**
 * Sets IRQ for VMMDev.
 *
 * @returns Solaris error code.
 * @param   pDip     Pointer to the device info structure.
 */
static int vboxGuestSolarisAddIRQ(dev_info_t *pDip)
{
    int IntrType = 0, rc;

    LogFlow((DEVICE_NAME "::AddIRQ: pDip=%p\n", pDip));
    rc = ddi_intr_get_supported_types(pDip, &IntrType);
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
                    g_pIntr = RTMemAlloc(IntrCount * sizeof(ddi_intr_handle_t));
                    if (g_pIntr)
                    {
                        size_t IntrAllocated;
                        unsigned i;
                        rc = ddi_intr_alloc(pDip, g_pIntr, IntrType, 0, IntrCount, &IntrAllocated, DDI_INTR_ALLOC_NORMAL);
                        if (   rc == DDI_SUCCESS
                            && IntrAllocated > 0)
                        {
                            uint_t uIntrPriority;
                            g_cIntrAllocated = IntrAllocated;
                            rc = ddi_intr_get_pri(g_pIntr[0], &uIntrPriority);
                            if (rc == DDI_SUCCESS)
                            {
                                /* Initialize the mutex. */
                                mutex_init(&g_IrqMtx, NULL, MUTEX_DRIVER, DDI_INTR_PRI(uIntrPriority));

                                /* Assign interrupt handler functions and enable interrupts. */
                                for (i = 0; i < IntrAllocated; i++)
                                {
                                    rc = ddi_intr_add_handler(g_pIntr[i], (ddi_intr_handler_t *)vboxGuestSolarisISR,
                                                            NULL /* No Private Data */, NULL);
                                    if (rc == DDI_SUCCESS)
                                        rc = ddi_intr_enable(g_pIntr[i]);
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
                                for (i = 0; i < IntrAllocated; i++)
                                    ddi_intr_remove_handler(g_pIntr[i]);
                            }
                            else
                                LogRel((DEVICE_NAME "::AddIRQ: failed to get priority of interrupt. rc=%d\n", rc));

                            /* Remove allocated IRQs, too bad we can free only one handle at a time. */
                            for (i = 0; i < g_cIntrAllocated; i++)
                                ddi_intr_free(g_pIntr[i]);
                        }
                        else
                            LogRel((DEVICE_NAME "::AddIRQ: failed to allocated IRQs. count=%d\n", IntrCount));
                        RTMemFree(g_pIntr);
                    }
                    else
                        LogRel((DEVICE_NAME "::AddIRQ: failed to allocated IRQs. count=%d\n", IntrCount));
                }
                else
                    LogRel((DEVICE_NAME "::AddIRQ: failed to get or insufficient available IRQs. rc=%d IntrAvail=%d\n", rc, IntrAvail));
            }
            else
                LogRel((DEVICE_NAME "::AddIRQ: failed to get or insufficient number of IRQs. rc=%d IntrCount=%d\n", rc, IntrCount));
        }
        else
            LogRel((DEVICE_NAME "::AddIRQ: invalid irq type. IntrType=%#x\n", IntrType));
    }
    else
        LogRel((DEVICE_NAME "::AddIRQ: failed to get supported interrupt types\n"));
    return rc;
}


/**
 * Removes IRQ for VMMDev.
 *
 * @param   pDip     Pointer to the device info structure.
 */
static void vboxGuestSolarisRemoveIRQ(dev_info_t *pDip)
{
    unsigned i;

    LogFlow((DEVICE_NAME "::RemoveIRQ:\n"));
    for (i = 0; i < g_cIntrAllocated; i++)
    {
        int rc = ddi_intr_disable(g_pIntr[i]);
        if (rc == DDI_SUCCESS)
        {
            rc = ddi_intr_remove_handler(g_pIntr[i]);
            if (rc == DDI_SUCCESS)
                ddi_intr_free(g_pIntr[i]);
        }
    }
    RTMemFree(g_pIntr);
    mutex_destroy(&g_IrqMtx);
}


/**
 * Interrupt Service Routine for VMMDev.
 *
 * @param   Arg     Private data (unused, will be NULL).
 * @returns DDI_INTR_CLAIMED if it's our interrupt, DDI_INTR_UNCLAIMED if it isn't.
 */
static uint_t vboxGuestSolarisISR(char *Arg /* Actually caddr_t. */)
{
    bool fOurIRQ;

    LogFlow((DEVICE_NAME "::ISR:\n"));
    mutex_enter(&g_IrqMtx);
    fOurIRQ = VBoxGuestCommonISR(&g_DevExt);
    mutex_exit(&g_IrqMtx);
    return fOurIRQ ? DDI_INTR_CLAIMED : DDI_INTR_UNCLAIMED;
}


/* Helper for VBoxGuestNativeISRMousePollEvent. */
static void VBoxGuestVUIDPutAbsEvent(ushort_t cEvent, int cValue);

/**
 * Native part of the IRQ service routine, called when the VBoxGuest mouse
 * pointer is moved.  We send a VUID event up to user space.
 */
void VBoxGuestNativeISRMousePollEvent(PVBOXGUESTDEVEXT pDevExt)
{
    VMMDevReqMouseStatus *pReq;
    int rc;
    LogFlow((DEVICE_NAME "::NativeISRMousePollEvent:\n"));

    rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(*pReq),
                     VMMDevReq_GetMouseStatus);
    if (RT_FAILURE(rc))
        return;  /* If kernel memory is short a missed event is acceptable! */
    pReq->mouseFeatures = 0;
    pReq->pointerXPos = 0;
    pReq->pointerYPos = 0;
    rc = VbglGRPerform(&pReq->header);
    if (RT_SUCCESS(rc))
    {
        int cMaxScreenX  = g_pState->cMaxScreenX;
        int cMaxScreenY  = g_pState->cMaxScreenY;

        VBoxGuestVUIDPutAbsEvent(LOC_X_ABSOLUTE,
                                   pReq->pointerXPos * cMaxScreenX
                                 / VMMDEV_MOUSE_RANGE_MAX);
        VBoxGuestVUIDPutAbsEvent(LOC_Y_ABSOLUTE,
                                   pReq->pointerYPos * cMaxScreenY
                                 / VMMDEV_MOUSE_RANGE_MAX);
    }
    VbglGRFree(&pReq->header);
}


void VBoxGuestVUIDPutAbsEvent(ushort_t cEvent, int cValue)
{
    queue_t *pReadQueue = RD(g_pState->pWriteQueue);
    mblk_t *pMBlk = allocb(sizeof(Firm_event, BPRI_HI));
    Firm_event *pEvent;
    AssertReturnVoid(cEvent == LOC_X_ABSOLUTE || cEvent == LOC_Y_ABSOLUTE);
    if (!pMBlk)
        return;  /* If kernel memory is short a missed event is acceptable! */
    pEvent = (Firm_event *)pMBlk->b_wptr;
    pEvent->id        = cEvent;
    pEvent->pair_type = FE_PAIR_DELTA;
    pEvent->pair      = cEvent == LOC_X_ABSOLUTE ? LOC_X_DELTA : LOC_Y_DELTA;
    pEvent->value     = cValue;
    uniqtime32(&pEvent->time);
    pMBlk->b_wptr += sizeof(Firm_event);
    /* Put the message on the queue immediately if it is not blocked. */
    if (canput(pReadQueue->q_next))
        putnext(pReadQueue, pMBlk);
    else
        putbq(pReadQueue, pMBlk);
}


/* Common code that depends on g_DevExt. */
#ifndef TESTCASE
# include "VBoxGuestIDC-unix.c.h"
#endif
