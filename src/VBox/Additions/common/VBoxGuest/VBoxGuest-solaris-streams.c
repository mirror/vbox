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
/** The maximum number of open device nodes we support. */
#define MAX_OPEN_NODES           4096


/******************************************************************************
*   Internal functions used in global structures                              *
******************************************************************************/

static int vbgr0SolOpen(queue_t *pReadQueue, dev_t *pDev, int fFlag,
                                int fMode, cred_t *pCred);
static int vbgr0SolClose(queue_t *pReadQueue, int fFlag, cred_t *pCred);
static int vbgr0SolWPut(queue_t *pWriteQueue, mblk_t *pMBlk);

static int vbgr0SolGetInfo(dev_info_t *pDip, ddi_info_cmd_t enmCmd, void *pArg, void **ppResult);
static int vbgr0SolAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd);
static int vbgr0SolDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd);


/******************************************************************************
*   Driver global structures                                                  *
******************************************************************************/

#ifndef TESTCASE  /* I see no value in including these in the test. */

/*
 * mod_info: STREAMS module information.
 */
static struct module_info g_vbgr0SolModInfo =
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
static struct qinit g_vbgr0SolRInit =
{
    NULL,                /* put */
    NULL,                /* service thread procedure */
    vbgr0SolOpen,
    vbgr0SolClose,
    NULL,                /* reserved */
    &g_vbgr0SolModInfo,
    NULL                 /* module statistics structure */
};

/* 
 * winit: write queue structure for handling messages coming from above.  Above
 * means user space applications: either Guest Additions user space tools or
 * applications reading pointer input.  Messages from the last most likely pass
 * through at least the "consms" console mouse streams module which multiplexes
 * hardware pointer drivers to a single virtual pointer.
 */
static struct qinit g_vbgr0SolWInit =
{
    vbgr0SolWPut,
    NULL,                   /* service thread procedure */
    NULL,                   /* open */
    NULL,                   /* close */
    NULL,                   /* reserved */
    &g_vbgr0SolModInfo,
    NULL                    /* module statistics structure */
};

/**
 * streamtab: for drivers that support char/block entry points.
 */
static struct streamtab g_vbgr0SolStreamTab =
{
    &g_vbgr0SolRInit,
    &g_vbgr0SolWInit,
    NULL,                   /* MUX rinit */
    NULL                    /* MUX winit */
};

/**
 * cb_ops: for drivers that support char/block entry points.
 */
static struct cb_ops g_vbgr0SolCbOps =
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
    g_vbgr0SolStreamTab,
    D_NEW | D_MP,           /* compat. flag */
};

/**
 * dev_ops: for driver device operations.
 */
static struct dev_ops g_vbgr0SolDevOps =
{
    DEVO_REV,               /* driver build revision */
    0,                      /* ref count */
    vbgr0SolGetInfo,
    nulldev,                /* identify */
    nulldev,                /* probe */
    vbgr0SolAttach,
    vbgr0SolDetach,
    nodev,                  /* reset */
    &g_vbgr0SolCbOps,
    (struct bus_ops *)0,
    nodev                   /* power */
};

/**
 * modldrv: export driver specifics to the kernel.
 */
static struct modldrv g_vbgr0SolModule =
{
    &mod_driverops,         /* extern from kernel */
    DEVICE_DESC " " VBOX_VERSION_STRING "r" RT_XSTR(VBOX_SVN_REV),
    &g_vbgr0SolDevOps
};

/**
 * modlinkage: export install/remove/info to the kernel.
 */
static struct modlinkage g_vbgr0SolModLinkage =
{
    MODREV_1,               /* loadable module system revision */
    &g_vbgr0SolModule,
    NULL                    /* terminate array of linkage structures */
};

#else  /* TESTCASE */
static void *g_vbgr0SolModLinkage;
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
    /* The current greatest horizontal pixel offset on the screen, used for
     * absolute mouse position reporting.
     */
    int                cMaxScreenX;
    /* The current greatest vertical pixel offset on the screen, used for
     * absolute mouse position reporting.
     */
    int                cMaxScreenY;
} VBGR0STATE, *PVBGR0STATE;


/******************************************************************************
*   Global Variables                                                          *
******************************************************************************/

/** Device handle (we support only one instance). */
static dev_info_t          *g_pDip = NULL;
/** Array of state structures for open device nodes.  I don't care about
 * wasting a few K of memory. */
static VBGR0STATE           g_aOpenNodeStates[MAX_OPEN_NODES] /* = { 0 } */;
/** Mutex to protect the queue pointers in the node states from being unset
 * during an IRQ. */
static kmutex_t             g_StateMutex;
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
static kmutex_t             g_IrqMutex;


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
        pModCtl = mod_getctl(&g_vbgr0SolModLinkage);
        if (pModCtl)
            pModCtl->mod_loadflags |= MOD_NOAUTOUNLOAD;
        else
            LogRel((DEVICE_NAME ":failed to disable autounloading!\n"));
        /* Initialise the node state mutex.  This will be taken in the ISR. */
        mutex_init(&g_StateMutex, NULL, MUTEX_DRIVER,
                   DDI_INTR_PRI(uIntrPriority));
        rc = mod_install(&g_vbgr0SolModLinkage);
    }
    else
    {
        cmn_err(CE_NOTE, "_init: RTR0Init failed. rc=%d\n", rc);
        return EINVAL;
    }

    return rc;
}


#ifdef TESTCASE
/** Simple test of the flow through _init. */
static void test_init(RTTEST hTest)
{
    RTTestSub(hTest, "Testing _init");
    RTTEST_CHECK(hTest, _init() == 0);
}
#endif


/** Driver cleanup. */
int _fini(void)
{
    int rc;

    LogFlow((DEVICE_NAME ":_fini\n"));
    rc = mod_remove(&g_vbgr0SolModLinkage);
    mutex_destroy(&g_StateMutex);

    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogDestroy(RTLogSetDefaultInstance(NULL));

    RTR0Term();
    return rc;
}


/** Driver identification. */
int _info(struct modinfo *pModInfo)
{
    LogFlow((DEVICE_NAME ":_info\n"));
    return mod_info(&g_vbgr0SolModLinkage, pModInfo);
}


/******************************************************************************
*   Helper routines                                                           *
******************************************************************************/

/** Calls the kernel IOCtl to report mouse status to the host on behalf of
 * an open session. */
static int vbgr0SolSetMouseStatus(PVBOXGUESTSESSION pSession, uint32_t fStatus)
{
    return VBoxGuestCommonIOCtl(VBOXGUEST_IOCTL_SET_MOUSE_STATUS, &g_DevExt,
                                pSession, &fStatus, sizeof(fStatus), NULL);
}

/** Resets (zeroes) a member in our open node state array in an IRQ-safe way.
 */
static void vbgr0SolResetSoftState(PVBGR0STATE pState)
{
    mutex_enter(&g_StateMutex);
    RT_ZERO(*pState);
    mutex_exit(&g_StateMutex);
}

/******************************************************************************
*   Main code                                                                 *
******************************************************************************/

/**
 * Open callback for the read queue, which we use as a generic device open
 * handler.
 */
int vbgr0SolOpen(queue_t *pReadQueue, dev_t *pDev, int fFlag, int fMode,
                 cred_t *pCred)
{
    int                 rc;
    PVBOXGUESTSESSION   pSession = NULL;
    PVBGR0STATE pState = NULL;
    unsigned cInstance;

    NOREF(fFlag);
    NOREF(pCred);
    LogFlow((DEVICE_NAME "::Open\n"));

    /*
     * Sanity check on the mode parameter - only open as a driver, not a
     * module, and we do cloning ourselves.  Note that we start at 1, as minor
     * zero was allocated to the file system device node in vbgr0SolAttach
     * (see https://blogs.oracle.com/timatworkhomeandinbetween/entry/using_makedevice_in_a_drivers).
     */
    if (fMode)
        return EINVAL;

    for (cInstance = 1; cInstance < MAX_OPEN_NODES; cInstance++)
    {
        if (ASMAtomicCmpXchgPtr(&g_aOpenNodeStates[cInstance].pWriteQueue,
                                WR(pReadQueue), NULL))
        {
            pState = &g_aOpenNodeStates[cInstance];
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
        qprocson(pState->pWriteQueue);
        Log((DEVICE_NAME "::Open: pSession=%p pState=%p pid=%d\n", pSession, pState, (int)RTProcSelf()));
        return 0;
    }

    /* Failed, clean up. */
    vbgr0SolResetSoftState(pState);

    LogRel((DEVICE_NAME "::Open: VBoxGuestCreateUserSession failed. rc=%d\n", rc));
    return EFAULT;
}


/**
 * Close callback for the read queue, which we use as a generic device close
 * handler.
 */
int vbgr0SolClose(queue_t *pReadQueue, int fFlag, cred_t *pCred)
{
    PVBOXGUESTSESSION pSession = NULL;
    PVBGR0STATE pState = (PVBGR0STATE)pReadQueue->q_ptr;

    LogFlow((DEVICE_NAME "::Close pid=%d\n", (int)RTProcSelf()));
    NOREF(fFlag);
    NOREF(pCred);

    if (!pState)
    {
        Log((DEVICE_NAME "::Close: failed to get pState.\n"));
        return EFAULT;
    }
    qprocsoff(pState->pWriteQueue);
    pSession = pState->pSession;
    vbgr0SolResetSoftState(pState);
    pReadQueue->q_ptr = NULL;

    Log((DEVICE_NAME "::Close: pSession=%p pState=%p\n", pSession, pState));
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


#ifdef TESTCASE
/** Simple test of vbgr0SolOpen and vbgr0SolClose. */
static void testOpenClose(RTTEST hTest)
{
    queue_t aQueues[4];
    dev_t device = 0;
    int rc;

    RTTestSub(hTest, "Testing vbgr0SolOpen and vbgr0SolClose");
    RT_ZERO(g_aOpenNodeStates);
    RT_ZERO(aQueues);
    doInitQueues(&aQueues[0]);
    doInitQueues(&aQueues[2]);
    rc = vbgr0SolOpen(RD(&aQueues[0]), &device, 0, 0, NULL);
    RTTEST_CHECK(hTest, rc == 0);
    RTTEST_CHECK(hTest, g_aOpenNodeStates[1].pWriteQueue == WR(&aQueues[0]));
    rc = vbgr0SolOpen(RD(&aQueues[2]), &device, 0, 0, NULL);
    RTTEST_CHECK(hTest, rc == 0);
    RTTEST_CHECK(hTest, g_aOpenNodeStates[2].pWriteQueue == WR(&aQueues[2]));
    vbgr0SolClose(RD(&aQueues[0]), 0, NULL);
    vbgr0SolClose(RD(&aQueues[1]), 0, NULL);
}
#endif


/* Helper for vbgr0SolWPut. */
static int vbgr0SolDispatchIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk);

/**
 * Handler for messages sent from above (user-space and upper modules) which
 * land in our write queue.
 */
int vbgr0SolWPut(queue_t *pWriteQueue, mblk_t *pMBlk)
{
    PVBGR0STATE pState = (PVBGR0STATE)pWriteQueue->q_ptr;
    
    LogFlowFunc((DEVICE_NAME "::\n"));
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
            int err = vbgr0SolDispatchIOCtl(pWriteQueue, pMBlk);
            if (!err)
                qreply(pWriteQueue, pMBlk);
            else
                miocnak(pWriteQueue, pMBlk, 0, err);
            break;
        }
    }
    return 0;
}


#ifdef TESTCASE
/* Constants, definitions and test functions for testWPut. */
static const int g_ccTestWPutFirmEvent = VUID_FIRM_EVENT;
# define PVGFORMAT (&g_ccTestWPutFirmEvent)
# define CBGFORMAT (sizeof(g_ccTestWPutFirmEvent))
static const Ms_screen_resolution g_TestResolution = { 640, 480 };
# define PMSIOSRES (&g_TestResolution)
# define CBMSIOSRES (sizeof(g_TestResolution))

static inline void testSetResolution(RTTEST hTest, queue_t *pWriteQueue,
                                     struct msgb *pMBlk)
{
    PVBGR0STATE pState = (PVBGR0STATE)pWriteQueue->q_ptr;
    RTTEST_CHECK_MSG(hTest,    pState->cMaxScreenX
                            == g_TestResolution.width - 1,
                     (hTest, "pState->cMaxScreenX=%d\n", pState->cMaxScreenX));
    RTTEST_CHECK_MSG(hTest,    pState->cMaxScreenY
                            == g_TestResolution.height - 1,
                     (hTest, "pState->cMaxScreenY=%d\n", pState->cMaxScreenY));
}

/** Data table for testWPut. */
static struct
{
    int iIOCCmd;
    size_t cbData;
    const void *pvDataIn;
    size_t cbDataIn;
    const void *pvDataOut;
    size_t cbDataOut;
    int rcExp;
    void (*pfnExtra)(RTTEST hTest, queue_t *pWriteQueue, struct msgb *pMBlk);
    bool fCanTransparent;
} g_asTestWPut[] =
{
   /* iIOCCmd          cbData           pvDataIn   cbDataIn
      pvDataOut   cbDataOut rcExp   pfnExtra           fCanTransparent */
    { VUIDGFORMAT,     sizeof(int),     NULL,      0,
      PVGFORMAT, CBGFORMAT, 0,      NULL,              false },
    { VUIDGFORMAT,     sizeof(int) - 1, NULL,      0,
      NULL,       0,        EINVAL, NULL,              false },
    { VUIDGFORMAT,     sizeof(int) + 1, NULL,      0,
      PVGFORMAT, CBGFORMAT, 0,      NULL,              false },
    { VUIDSFORMAT,     sizeof(int),     PVGFORMAT, CBGFORMAT,
      NULL,       0,        0,      NULL,              false },
    { MSIOSRESOLUTION, CBMSIOSRES,      PMSIOSRES, CBMSIOSRES,
      NULL,       0,        0,      testSetResolution, false },
    { VUIDGWHEELINFO,  0,               NULL,      0,
      NULL,       0,        EINVAL, NULL,              true }
};

# undef PVGFORMAT
# undef CBGFORMAT
# undef PMSIOSRES
# undef CBMSIOSRES

/* Helpers for testWPut. */
static void testWPutStreams(RTTEST hTest, unsigned i);
static void testWPutTransparent(RTTEST hTest, unsigned i);

/** Test WPut's handling of different IOCtls, which is bulk of the logic in
 * this file. */
static void testWPut(RTTEST hTest)
{
    unsigned i;

    RTTestSub(hTest, "Testing vbgr0WPut");
    for (i = 0; i < RT_ELEMENTS(g_asTestWPut); ++i)
    {
        testWPutStreams(hTest, i);
        if (g_asTestWPut[i].fCanTransparent)
            testWPutTransparent(hTest, i);
    }
}

/** Simulate sending a streams IOCtl to WPut with the parameters from table
 * line @a i. */
void testWPutStreams(RTTEST hTest, unsigned i)
{
    queue_t aQueues[2];
    dev_t device = 0;
    struct msgb MBlk, MBlkCont;
    struct datab DBlk;
    struct iocblk IOCBlk;
    int rc, cFormat = 0;
    unsigned char acData[1024];

    RT_ZERO(aQueues);
    doInitQueues(&aQueues[0]);
    rc = vbgr0SolOpen(RD(&aQueues[0]), &device, 0, 0, NULL);
    RTTEST_CHECK_MSG(hTest, rc == 0, (hTest, "i=%u, rc=%d\n", i, rc));
    RTTEST_CHECK_MSG(hTest,    g_aOpenNodeStates[1].pWriteQueue
                            == WR(&aQueues[0]), (hTest, "i=%u\n", i));
    RT_ZERO(MBlk);
    RT_ZERO(DBlk);
    RT_ZERO(IOCBlk);
    RT_ZERO(MBlkCont);
    RT_ZERO(acData);
    DBlk.db_type = M_IOCTL;
    IOCBlk.ioc_cmd = g_asTestWPut[i].iIOCCmd;
    IOCBlk.ioc_count = g_asTestWPut[i].cbData;
    AssertReturnVoid(g_asTestWPut[i].cbData <= sizeof(acData));
    AssertReturnVoid(g_asTestWPut[i].cbDataIn <= g_asTestWPut[i].cbData);
    AssertReturnVoid(g_asTestWPut[i].cbDataOut <= g_asTestWPut[i].cbData);
    memcpy(acData, g_asTestWPut[i].pvDataIn, g_asTestWPut[i].cbDataIn);
    MBlkCont.b_rptr = acData;
    MBlkCont.b_wptr = acData + g_asTestWPut[i].cbData;
    MBlk.b_cont = &MBlkCont;
    MBlk.b_rptr = (unsigned char *)&IOCBlk;
    MBlk.b_wptr = (unsigned char *)&IOCBlk + sizeof(IOCBlk);
    MBlk.b_datap = &DBlk;
    rc = vbgr0SolWPut(WR(&aQueues[0]), &MBlk);
    RTTEST_CHECK_MSG(hTest, IOCBlk.ioc_error == g_asTestWPut[i].rcExp,
                     (hTest, "i=%u, IOCBlk.ioc_error=%d\n", i,
                      IOCBlk.ioc_error));
    RTTEST_CHECK_MSG(hTest, IOCBlk.ioc_count == g_asTestWPut[i].cbDataOut,
                     (hTest, "i=%u, ioc_count=%u\n", i, IOCBlk.ioc_count));
    RTTEST_CHECK_MSG(hTest, !memcmp(acData, g_asTestWPut[i].pvDataOut,
                                    g_asTestWPut[i].cbDataOut),
                     (hTest, "i=%u\n", i));
    /* Hack to ensure that miocpullup() gets called when needed. */
    if (g_asTestWPut[i].cbData > 0)
        RTTEST_CHECK_MSG(hTest, MBlk.b_flag == 1, (hTest, "i=%u\n", i));
    if (g_asTestWPut[i].pfnExtra)
        g_asTestWPut[i].pfnExtra(hTest, WR(&aQueues[0]), &MBlk);
    vbgr0SolClose(RD(&aQueues[1]), 0, NULL);
}


/** Simulate sending a transparent IOCtl to WPut with the parameters from table
 * line @a i. */
void testWPutTransparent(RTTEST hTest, unsigned i)
{
    queue_t aQueues[2];
    dev_t device = 0;
    struct msgb MBlk, MBlkCont;
    struct datab DBlk;
    struct iocblk IOCBlk;
    int rc, cFormat = 0;
    unsigned char acData[1024];

    RT_ZERO(aQueues);
    doInitQueues(&aQueues[0]);
    rc = vbgr0SolOpen(RD(&aQueues[0]), &device, 0, 0, NULL);
    RTTEST_CHECK_MSG(hTest, rc == 0, (hTest, "i=%u, rc=%d\n", i, rc));
    RTTEST_CHECK_MSG(hTest,    g_aOpenNodeStates[1].pWriteQueue
                            == WR(&aQueues[0]), (hTest, "i=%u\n", i));
    RT_ZERO(MBlk);
    RT_ZERO(DBlk);
    RT_ZERO(IOCBlk);
    RT_ZERO(MBlkCont);
    RT_ZERO(acData);
    DBlk.db_type = M_IOCTL;
    IOCBlk.ioc_cmd = g_asTestWPut[i].iIOCCmd;
    IOCBlk.ioc_count = TRANSPARENT;
    AssertReturnVoid(g_asTestWPut[i].cbData <= sizeof(acData));
    AssertReturnVoid(g_asTestWPut[i].cbDataIn <= g_asTestWPut[i].cbData);
    AssertReturnVoid(g_asTestWPut[i].cbDataOut <= g_asTestWPut[i].cbData);
    memcpy(acData, g_asTestWPut[i].pvDataIn, g_asTestWPut[i].cbDataIn);
    MBlkCont.b_rptr = acData;
    MBlkCont.b_wptr = acData + g_asTestWPut[i].cbData;
    MBlk.b_cont = &MBlkCont;
    MBlk.b_rptr = (unsigned char *)&IOCBlk;
    MBlk.b_wptr = (unsigned char *)&IOCBlk + sizeof(IOCBlk);
    MBlk.b_datap = &DBlk;
    rc = vbgr0SolWPut(WR(&aQueues[0]), &MBlk);
    RTTEST_CHECK_MSG(hTest, IOCBlk.ioc_error == g_asTestWPut[i].rcExp,
                     (hTest, "i=%u, IOCBlk.ioc_error=%d\n", i,
                      IOCBlk.ioc_error));
    RTTEST_CHECK_MSG(hTest, IOCBlk.ioc_count == g_asTestWPut[i].cbDataOut,
                     (hTest, "i=%u, ioc_count=%u\n", i, IOCBlk.ioc_count));
    RTTEST_CHECK_MSG(hTest, !memcmp(acData, g_asTestWPut[i].pvDataOut,
                                    g_asTestWPut[i].cbDataOut),
                     (hTest, "i=%u\n", i));
    /* Hack to ensure that miocpullup() gets called when needed. */
    if (g_asTestWPut[i].cbData > 0)
        RTTEST_CHECK_MSG(hTest, MBlk.b_flag == 1, (hTest, "i=%u\n", i));
    if (g_asTestWPut[i].pfnExtra)
        g_asTestWPut[i].pfnExtra(hTest, WR(&aQueues[0]), &MBlk);
    vbgr0SolClose(RD(&aQueues[1]), 0, NULL);
}
#endif


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
 * @param iCmd      The IOCtl command number.
 * @param pvData    Buffer for the user data.
 * @param cbBuffer  Size of the buffer in @a pvData or zero.
 * @param pcbData   Where to set the size of the data returned.  Required for
 *                  handlers which return data.
 * @param prc       Where to store the return code.  Default is zero.  Only
 *                  used for IOCtls without data for convenience of
 *                  implemention.
 */
typedef int FNVBGR0SOLIOCTL(PVBGR0STATE pState, int iCmd, void *pvData,
                            size_t cbBuffer, size_t *pcbData, int *prc);
typedef FNVBGR0SOLIOCTL *PFNVBGR0SOLIOCTL;

/* Helpers for vbgr0SolDispatchIOCtl. */
static int vbgr0SolHandleIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk,
                               PFNVBGR0SOLIOCTL pfnHandler,
                               int iCmd, size_t cbTransparent,
                               enum IOCTLDIRECTION enmDirection);
static int vbgr0SolVUIDIOCtl(PVBGR0STATE pState, int iCmd, void *pvData,
                             size_t cbBuffer, size_t *pcbData, int *prc);
static int vbgr0SolGuestIOCtl(PVBGR0STATE pState, int iCmd, void *pvData,
                              size_t cbBuffer, size_t *pcbData, int *prc);

/** Table of supported VUID IOCtls. */
struct
{
    /** The IOCtl number. */
    int iCmd;
    /** The size of the buffer which needs to be copied between user and kernel
     * space, or zero if unknown (must be known for tranparent IOCtls). */
    size_t cbBuffer;
    /** The direction the buffer data needs to be copied.  This must be
     * specified for transparent IOCtls. */
    enum IOCTLDIRECTION enmDirection;
} g_aVUIDIOCtlDescriptions[] =
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
static int vbgr0SolDispatchIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk)
{
    struct iocblk *pIOCBlk = (struct iocblk *)pMBlk->b_rptr;
    int iCmd = pIOCBlk->ioc_cmd, iCmdType = iCmd & ~0xff;
    size_t cbBuffer;
    enum IOCTLDIRECTION enmDirection;

    LogFlowFunc((DEVICE_NAME "::iCmdType=%c, iCmd=%d\n", iCmdType, iCmd));
    switch (iCmdType)
    {
        case MSIOC:
        case VUIOC:
        {
            unsigned i;
            
            for (i = 0; i < RT_ELEMENTS(g_aVUIDIOCtlDescriptions); ++i)
                if (g_aVUIDIOCtlDescriptions[i].iCmd == iCmd)
                {
                    cbBuffer     = g_aVUIDIOCtlDescriptions[i].cbBuffer;
                    enmDirection = g_aVUIDIOCtlDescriptions[i].enmDirection;
                    return vbgr0SolHandleIOCtl(pWriteQueue, pMBlk,
                                               vbgr0SolVUIDIOCtl, iCmd,
                                               cbBuffer, enmDirection);
                }
            return EINVAL;
        }
        case 'V' << 8:
            return vbgr0SolHandleIOCtl(pWriteQueue, pMBlk, vbgr0SolGuestIOCtl,
                                       iCmd, 0, UNSPECIFIED);
        default:
            return ENOTTY;
    }
}


/* Helpers for vbgr0SolHandleIOCtl. */
static int vbgr0SolHandleIOCtlData(queue_t *pWriteQueue, mblk_t *pMBlk,
                                   PFNVBGR0SOLIOCTL pfnHandler, int iCmd,
                                   size_t cbTransparent,
                                   enum IOCTLDIRECTION enmDirection);

static int vbgr0SolHandleTransparentIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk,
                                          PFNVBGR0SOLIOCTL pfnHandler,
                                          int iCmd, size_t cbTransparent,
                                          enum IOCTLDIRECTION enmDirection);

static int vbgr0SolHandleIStrIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk,
                                   PFNVBGR0SOLIOCTL pfnHandler, int iCmd);

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
 * @param  iCmd           IOCtl command number.
 * @param  cbTransparent  size of the user space buffer for this IOCtl number,
 *                        used for processing transparent IOCtls.  Pass zero
 *                        for IOCtls with no maximum buffer size (which will
 *                        not be able to be handled as transparent) or with
 *                        no argument.
 * @param  enmDirection   data transfer direction of the IOCtl.
 */
static int vbgr0SolHandleIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk,
                               PFNVBGR0SOLIOCTL pfnHandler, int iCmd,
                               size_t cbTransparent,
                               enum IOCTLDIRECTION enmDirection)
{
    struct iocblk *pIOCBlk = (struct iocblk *)pMBlk->b_rptr;
    PVBGR0STATE pState = (PVBGR0STATE)pWriteQueue->q_ptr;

    if (pMBlk->b_datap->db_type == M_IOCDATA)
        return vbgr0SolHandleIOCtlData(pWriteQueue, pMBlk, pfnHandler, iCmd,
                                       cbTransparent, enmDirection);
    else if (   pMBlk->b_datap->db_type == M_IOCTL
             && pIOCBlk->ioc_count == TRANSPARENT)
        return vbgr0SolHandleTransparentIOCtl(pWriteQueue, pMBlk, pfnHandler,
                                              iCmd, cbTransparent,
                                              enmDirection);
    else if (pMBlk->b_datap->db_type == M_IOCTL)
        return vbgr0SolHandleIStrIOCtl(pWriteQueue, pMBlk, pfnHandler, iCmd);
    return EINVAL;
}


/**
 * Helper for vbgr0SolHandleIOCtl.  This rather complicated-looking
 * code is basically the standard boilerplate for handling any streams IOCtl
 * additional data, which we currently only use for transparent IOCtls.
 * @copydoc vbgr0SolHandleIOCtl
 */
static int vbgr0SolHandleIOCtlData(queue_t *pWriteQueue, mblk_t *pMBlk,
                                   PFNVBGR0SOLIOCTL pfnHandler, int iCmd,
                                   size_t cbTransparent,
                                   enum IOCTLDIRECTION enmDirection)
{
    struct copyresp *pCopyResp = (struct copyresp *)pMBlk->b_rptr;
    PVBGR0STATE pState = (PVBGR0STATE)pWriteQueue->q_ptr;

    if (pCopyResp->cp_rval)  /* cp_rval is a pointer used as a boolean. */
    {
        freemsg(pMBlk);
        return EAGAIN;
    }
    if ((pCopyResp->cp_private && enmDirection == BOTH) || enmDirection == IN)
    {
        size_t cbData = 0;
        void *pvData = NULL;
        int err;

        if (cbData < cbTransparent)
            return EINVAL;
        if (!pMBlk->b_cont)
            return EINVAL;
        if (enmDirection == BOTH && !pCopyResp->cp_private)
            return EINVAL;
        pvData = pMBlk->b_cont->b_rptr;
        err = pfnHandler(pState, iCmd, pvData, cbTransparent, &cbData, NULL);
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
 * Helper for vbgr0SolHandleIOCtl.  This rather complicated-looking
 * code is basically the standard boilerplate for handling transparent IOCtls,
 * that is, IOCtls which are not re-packed inside STREAMS IOCtls.
 * @copydoc vbgr0SolHandleIOCtl
 */
int vbgr0SolHandleTransparentIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk,
                                   PFNVBGR0SOLIOCTL pfnHandler, int iCmd,
                                   size_t cbTransparent,
                                   enum IOCTLDIRECTION enmDirection)
{
    int err = 0, rc = 0;
    size_t cbData = 0;
    PVBGR0STATE pState = (PVBGR0STATE)pWriteQueue->q_ptr;

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
        err = pfnHandler(pState, iCmd, pvData, cbTransparent, &cbData, NULL);
        if (!err)
            mcopyout(pMBlk, NULL, cbData, NULL, pMBlkOut);
        else
            freemsg(pMBlkOut);
    }
    else
    {
        AssertReturn(enmDirection == NONE, EINVAL);
        err = pfnHandler(pState, iCmd, NULL, 0, NULL, &rc);
        if (!err)
            miocack(pWriteQueue, pMBlk, 0, rc);
    }
    return err;
}
                 
/**
 * Helper for vbgr0SolHandleIOCtl.  This rather complicated-looking
 * code is basically the standard boilerplate for handling any streams IOCtl.
 * @copydoc vbgr0SolHandleIOCtl
 */
static int vbgr0SolHandleIStrIOCtl(queue_t *pWriteQueue, mblk_t *pMBlk,
                                   PFNVBGR0SOLIOCTL pfnHandler, int iCmd)
{
    struct iocblk *pIOCBlk = (struct iocblk *)pMBlk->b_rptr;
    PVBGR0STATE pState = (PVBGR0STATE)pWriteQueue->q_ptr;
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
    err = pfnHandler(pState, iCmd, pvData, cbBuffer, &cbData, &rc);
    if (!err)
        miocack(pWriteQueue, pMBlk, cbData, rc);
    return err;
}


/**
 * Handle a VUID input device IOCtl.
 * @copydoc FNVBGR0SOLIOCTL
 */
static int vbgr0SolVUIDIOCtl(PVBGR0STATE pState, int iCmd, void *pvData,
                             size_t cbBuffer, size_t *pcbData, int *prc)
{
    LogFlowFunc((DEVICE_NAME ":: " /* no '\n' */));
    switch (iCmd)
    {
        case VUIDGFORMAT:
        {
            LogFlowFunc(("VUIDGFORMAT\n"));
            if (cbBuffer < sizeof(int))
                return EINVAL;
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
            if (cbBuffer < sizeof(Ms_parms))
                return EINVAL;
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
            int rc;

            LogFlowFunc(("MSIOSRESOLUTION\n"));
            if (cbBuffer < sizeof(Ms_screen_resolution))
                return EINVAL;
            pState->cMaxScreenX = pResolution->width  - 1;
            pState->cMaxScreenY = pResolution->height - 1;
            /* Note: we don't disable this again until session close. */
            rc = vbgr0SolSetMouseStatus(pState->pSession,
                                          VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE
                                        | VMMDEV_MOUSE_NEW_PROTOCOL);
            if (RT_SUCCESS(rc))
                return 0;
            pState->cMaxScreenX = 0;
            pState->cMaxScreenY = 0;
            return ENODEV;
        }
        case MSIOBUTTONS:
        {
            LogFlowFunc(("MSIOBUTTONS\n"));
            if (cbBuffer < sizeof(int))
                return EINVAL;
            *(int *)pvData = 0;
            *pcbData = sizeof(int);
            return 0;
        }
        case VUIDGWHEELCOUNT:
        {
            LogFlowFunc(("VUIDGWHEELCOUNT\n"));
            if (cbBuffer < sizeof(int))
                return EINVAL;
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
            LogFlowFunc(("Invalid IOCtl command %x\n", iCmd));
            return EINVAL;
    }
}


/**
 * Handle a VBoxGuest IOCtl.
 * @copydoc FNVBGR0SOLIOCTL
 */
static int vbgr0SolGuestIOCtl(PVBGR0STATE pState, int iCmd, void *pvData,
                              size_t cbBuffer, size_t *pcbData, int *prc)
{
    int rc = VBoxGuestCommonIOCtl(iCmd, &g_DevExt, pState->pSession, pvData, cbBuffer, pcbData);
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
        Log((DEVICE_NAME "::IOCtl: VBoxGuestCommonIOCtl failed. Cmd=%#x rc=%d\n", iCmd, rc));
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
int vbgr0SolGetInfo(dev_info_t *pDip, ddi_info_cmd_t enmCmd, void *pvArg,
                    void **ppvResult)
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


/* Helpers for vbgr0SolAttach and vbgr0SolDetach. */
static int vbgr0SolAddIRQ(dev_info_t *pDip);
static void vbgr0SolRemoveIRQ(dev_info_t *pDip);

/**
 * Attach entry point, to attach a device to the system or resume it.
 *
 * @param   pDip            The module structure instance.
 * @param   enmCmd          Attach type (ddi_attach_cmd_t)
 *
 * @return  corresponding solaris error code.
 */
int vbgr0SolAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd)
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
                            rc = vbgr0SolAddIRQ(pDip);
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
                                vbgr0SolRemoveIRQ(pDip);
                            }
                            else
                                LogRel((DEVICE_NAME "::Attach: vbgr0SolAddIRQ failed.\n"));
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
int vbgr0SolDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd)
{
    LogFlow((DEVICE_NAME "::Detach\n"));
    switch (enmCmd)
    {
        case DDI_DETACH:
        {
            vbgr0SolRemoveIRQ(pDip);
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


/* Interrupt service routine installed by vbgr0SolAddIRQ. */
static uint_t vbgr0SolISR(char *Arg /* Actually caddr_t. */);

/**
 * Sets IRQ for VMMDev.
 *
 * @returns Solaris error code.
 * @param   pDip     Pointer to the device info structure.
 */
static int vbgr0SolAddIRQ(dev_info_t *pDip)
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
                                mutex_init(&g_IrqMutex, NULL, MUTEX_DRIVER, DDI_INTR_PRI(uIntrPriority));

                                /* Assign interrupt handler functions and enable interrupts. */
                                for (i = 0; i < IntrAllocated; i++)
                                {
                                    rc = ddi_intr_add_handler(g_pIntr[i], (ddi_intr_handler_t *)vbgr0SolISR,
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
static void vbgr0SolRemoveIRQ(dev_info_t *pDip)
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
    mutex_destroy(&g_IrqMutex);
}


/**
 * Interrupt Service Routine for VMMDev.
 *
 * @param   Arg     Private data (unused, will be NULL).
 * @returns DDI_INTR_CLAIMED if it's our interrupt, DDI_INTR_UNCLAIMED if it isn't.
 */
static uint_t vbgr0SolISR(char *Arg /* Actually caddr_t. */)
{
    bool fOurIRQ;

    LogFlow((DEVICE_NAME "::ISR:\n"));
    mutex_enter(&g_IrqMutex);
    fOurIRQ = VBoxGuestCommonISR(&g_DevExt);
    mutex_exit(&g_IrqMutex);
    return fOurIRQ ? DDI_INTR_CLAIMED : DDI_INTR_UNCLAIMED;
}


/* Helper for VBoxGuestNativeISRMousePollEvent. */
static void vbgr0SolVUIDPutAbsEvent(PVBGR0STATE pState, ushort_t cEvent,
                                    int cValue);

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
        unsigned i;

        mutex_enter(&g_StateMutex);
        for (i = 1; i < MAX_OPEN_NODES; ++i)
        {
            int cMaxScreenX  = g_aOpenNodeStates[i].cMaxScreenX;
            int cMaxScreenY  = g_aOpenNodeStates[i].cMaxScreenY;

            if (!cMaxScreenX || !cMaxScreenY)
                continue;
            vbgr0SolVUIDPutAbsEvent(&g_aOpenNodeStates[i], LOC_X_ABSOLUTE,
                                       pReq->pointerXPos * cMaxScreenX
                                     / VMMDEV_MOUSE_RANGE_MAX);
            vbgr0SolVUIDPutAbsEvent(&g_aOpenNodeStates[i], LOC_Y_ABSOLUTE,
                                       pReq->pointerYPos * cMaxScreenY
                                     / VMMDEV_MOUSE_RANGE_MAX);
        }
        mutex_exit(&g_StateMutex);
    }
    VbglGRFree(&pReq->header);
}


void vbgr0SolVUIDPutAbsEvent(PVBGR0STATE pState, ushort_t cEvent,
                              int cValue)
{
    queue_t *pReadQueue = RD(pState->pWriteQueue);
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

#ifdef TESTCASE
int main(void)
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstVBoxGuest-solaris", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);
    test_init(hTest);
    testOpenClose(hTest);
    testWPut(hTest);

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}
#endif
