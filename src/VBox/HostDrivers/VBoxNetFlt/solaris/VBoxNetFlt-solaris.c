/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Solaris Specific Code.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#if defined(DEBUG_ramshankar) && !defined(LOG_ENABLED)
# define LOG_ENABLED
#endif
#define LOG_GROUP LOG_GROUP_NET_FLT_DRV
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/cdefs.h>
#include <iprt/string.h>
#include <iprt/initterm.h>
#include <iprt/assert.h>
#include <iprt/alloca.h>
#include <iprt/net.h>
#include <iprt/mem.h>
#include <iprt/thread.h>

#include <inet/ip.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/kstr.h>
#include <sys/file.h>
#include <sys/sockio.h>
#include <sys/strsubr.h>
#include <sys/pathname.h>
#include <sys/t_kuser.h>

#include <sys/types.h>
#include <sys/dlpi.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ethernet.h>
#include <sys/stat.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strsun.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunldi.h>

// Workaround for very strange define in sys/user.h
// #define u       (curproc->p_user)       /* user is now part of proc structure */
#ifdef u
#undef u
#endif

#define VBOXNETFLT_OS_SPECFIC 1
#include "../VBoxNetFltInternal.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The module name. */
#define DEVICE_NAME              "vboxflt"
/** The module descriptions as seen in 'modinfo'. */
#define DEVICE_DESC_DRV          "VirtualBox NetFilter Driver"
#define DEVICE_DESC_MOD          "VirtualBox NetFilter Module"

/** @todo Remove the below hackery once done! */
#if defined(DEBUG_ramshankar) && defined(LOG_ENABLED)
# undef Log
# define Log        LogRel
# undef LogFlow
# define LogFlow    LogRel
#endif

/** Dynamic module binding specific oddities. */
#define VBOXNETFLT_IFNAME_LEN           LIFNAMSIZ + 1
#define VBOXNETFLT_MODE_REQ_MAGIC       0xacce55ed


/*******************************************************************************
*   Global Functions                                                           *
*******************************************************************************/
/**
 * Stream Driver hooks.
 */
static int VBoxNetFltSolarisGetInfo(dev_info_t *pDip, ddi_info_cmd_t enmCmd, void *pArg, void **ppResult);
static int VBoxNetFltSolarisAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd);
static int VBoxNetFltSolarisDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd);

/**
 * Stream Module hooks.
 */
static int VBoxNetFltSolarisModOpen(queue_t *pQueue, dev_t *pDev, int fFile, int fStream, cred_t *pCred);
static int VBoxNetFltSolarisModClose(queue_t *pQueue, int fFile, cred_t *pCred);
static int VBoxNetFltSolarisModReadPut(queue_t *pQueue, mblk_t *pMsg);
static int VBoxNetFltSolarisModWritePut(queue_t *pQueue, mblk_t *pMsg);
static int VBoxNetFltSolarisModWriteService(queue_t *pQueue);

/**
 * OS specific hooks invoked from common VBoxNetFlt ring-0.
 */
bool vboxNetFltPortOsIsPromiscuous(PVBOXNETFLTINS pThis);
void vboxNetFltPortOsGetMacAddress(PVBOXNETFLTINS pThis, PRTMAC pMac);
bool vboxNetFltPortOsIsHostMac(PVBOXNETFLTINS pThis, PCRTMAC pMac);
void vboxNetFltPortOsSetActive(PVBOXNETFLTINS pThis, bool fActive);
int vboxNetFltOsDisconnectIt(PVBOXNETFLTINS pThis);
int vboxNetFltOsConnectIt(PVBOXNETFLTINS pThis);
void vboxNetFltOsDeleteInstance(PVBOXNETFLTINS pThis);
int vboxNetFltOsInitInstance(PVBOXNETFLTINS pThis);
int vboxNetFltOsPreInitInstance(PVBOXNETFLTINS pThis);


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Streams: module info.
 */
static struct module_info g_VBoxNetFltSolarisModInfo =
{
    0xbad,                          /* module id */
    DEVICE_NAME,
    0,                              /* min. packet size */
    INFPSZ,                         /* max. packet size */
    0,                              /* hi-water mask */
    0                               /* lo-water mask */
};

/**
 * Streams: read queue hooks.
 */
static struct qinit g_VBoxNetFltSolarisReadQ =
{
    VBoxNetFltSolarisModReadPut,
    NULL,                           /* service */
    VBoxNetFltSolarisModOpen,
    VBoxNetFltSolarisModClose,
    NULL,                           /* admin (reserved) */
    &g_VBoxNetFltSolarisModInfo,
    NULL                            /* module stats */
};

/**
 * Streams: write queue hooks.
 */
static struct qinit g_VBoxNetFltSolarisWriteQ =
{
    VBoxNetFltSolarisModWritePut,
    VBoxNetFltSolarisModWriteService,
    NULL,                           /* open */
    NULL,                           /* close */
    NULL,                           /* admin (reserved) */
    &g_VBoxNetFltSolarisModInfo,
    NULL                            /* module stats */
};

/**
 * Streams: IO stream tab.
 */
static struct streamtab g_VBoxNetFltSolarisStreamTab =
{
    &g_VBoxNetFltSolarisReadQ,
    &g_VBoxNetFltSolarisWriteQ,
    NULL,                           /* muxread init */
    NULL                            /* muxwrite init */
};

/**
 * cb_ops: driver char/block entry points
 */
static struct cb_ops g_VBoxNetFltSolarisCbOps =
{
    nulldev,                    /* cb open */
    nulldev,                    /* cb close */
    nodev,                      /* b strategy */
    nodev,                      /* b dump */
    nodev,                      /* b print */
    nodev,                      /* cb read */
    nodev,                      /* cb write */
    nodev,                      /* cb ioctl */
    nodev,                      /* c devmap */
    nodev,                      /* c mmap */
    nodev,                      /* c segmap */
    nochpoll,                   /* c poll */
    ddi_prop_op,                /* property ops */
    &g_VBoxNetFltSolarisStreamTab,
    D_NEW | D_MP | D_MTPERMOD,  /* compat. flag */
    CB_REV                      /* revision */
};

/**
 * dev_ops: driver entry/exit and other ops.
 */
static struct dev_ops g_VBoxNetFltSolarisDevOps =
{
    DEVO_REV,                   /* driver build revision */
    0,                          /* ref count */
    VBoxNetFltSolarisGetInfo,
    nulldev,                    /* identify */
    nulldev,                    /* probe */
    VBoxNetFltSolarisAttach,
    VBoxNetFltSolarisDetach,
    nodev,                      /* reset */
    &g_VBoxNetFltSolarisCbOps,
    (struct bus_ops *)0,
    nodev                       /* power */
};

/**
 * modldrv: export driver specifics to kernel
 */
static struct modldrv g_VBoxNetFltSolarisDriver =
{
    &mod_driverops,             /* extern from kernel */
    DEVICE_DESC_DRV,
    &g_VBoxNetFltSolarisDevOps
};

/**
 * fmodsw: streams module ops
 */
static struct fmodsw g_VBoxNetFltSolarisModOps =
{
    DEVICE_NAME,
    &g_VBoxNetFltSolarisStreamTab,
    D_NEW | D_MP | D_MTPERMOD
};

/**
 * modlstrmod: streams module specifics to kernel
 */
static struct modlstrmod g_VBoxNetFltSolarisModule =
{
    &mod_strmodops,             /* extern from kernel */
    DEVICE_DESC_MOD,
    &g_VBoxNetFltSolarisModOps
};

/**
 * modlinkage: export install/remove/info to the kernel
 */
static struct modlinkage g_VBoxNetFltSolarisModLinkage =
{
    MODREV_1,                      /* loadable module system revision */
    &g_VBoxNetFltSolarisDriver,    /* streams driver framework */
    &g_VBoxNetFltSolarisModule,    /* streams module framework */
    NULL                           /* terminate array of linkage structures */
};

struct vboxnetflt_state_t;

/**
 * vboxnetflt_dladdr_t: DL SAP address format
 */
typedef struct vboxnetflt_dladdr_t
{
    ether_addr_t Mac;
    uint16_t SAP;
} vboxnetflt_dladdr_t;

#define VBOXNETFLT_DLADDRL        sizeof(vboxnetflt_dladdr_t)

/**
 * which stream is this?
 */
typedef enum VBOXNETFLTSTREAMTYPE
{
    kUndefined = 0,
    kIpStream = 0x1b,
    kArpStream = 0xab
} VBOXNETFLTSTREAMTYPE;

/**
 * vboxnetflt_stream_t: per-stream data
 */
typedef struct vboxnetflt_stream_t
{
    int DevMinor;                         /* minor device no. (for clone) */
    queue_t *pReadQueue;                  /* read side queue */
    queue_t *pArpReadQueue;               /* ARP read queue */
    struct vboxnetflt_state_t *pState;    /* state associated with this queue */
    struct vboxnetflt_stream_t *pNext;    /* next stream in list */
    bool fPromisc;                        /* cached promiscous value */
    bool fRawMode;                        /* whether raw mode request was successful */
    uint32_t ModeReqId;                   /* track MIOCTLs for swallowing our fake request acknowledgements */
    t_uscalar_t UnAckPrim;                /* unacknowledged primitive sent by this stream, again fake DL request tracking stuff  */
    PVBOXNETFLTINS pThis;                 /* the backend instance */
    VBOXNETFLTSTREAMTYPE Type;            /* the type of the stream Ip/Arp */
} vboxnetflt_stream_t;

/**
 * vboxnetflt_state_t: per-driver data
 */
typedef struct vboxnetflt_state_t
{
    /** Global device info handle. */
    dev_info_t *pDip;
    /** The list of all opened streams. */
    vboxnetflt_stream_t *pOpenedStreams;
    /**
     * pCurInstance is the currently VBox instance to be associated with the stream being created
     * in ModOpen. This is just shared global data between the dynamic attach and the ModOpen procedure.
     */
    PVBOXNETFLTINS pCurInstance;
    /** Goes along with pCurInstance to determine type of stream being opened/created. */
    VBOXNETFLTSTREAMTYPE CurType;
} vboxnetflt_state_t;


/*******************************************************************************
*   Internal Functions                                                           *
*******************************************************************************/
static int vboxNetFltSolarisSetRawMode(queue_t *pQueue);
static int vboxNetFltSolarisSetFastMode(queue_t *pQueue);

static int vboxNetFltSolarisPhysAddrReq(queue_t *pQueue);
static void vboxNetFltSolarisCachePhysAddr(PVBOXNETFLTINS pThis, mblk_t *pPhysAddrAckMsg);

static int vboxNetFltSolarisUnitDataToRaw(PVBOXNETFLTINS pThis, mblk_t *pMsg, mblk_t **ppRawMsg);

static mblk_t *vboxNetFltSolarisMBlkFromSG(PVBOXNETFLTINS pThis, PINTNETSG pSG, uint32_t fDst);
static unsigned vboxNetFltSolarisMBlkCalcSGSegs(PVBOXNETFLTINS pThis, mblk_t *pMsg);
static int vboxNetFltSolarisMBlkToSG(PVBOXNETFLTINS pThis, mblk_t *pMsg, PINTNETSG pSG, unsigned cSegs, uint32_t fSrc);
static int vboxNetFltSolarisRecv(PVBOXNETFLTINS pThis, vboxnetflt_stream_t *pStream, queue_t *pQueue, mblk_t *pMsg);
static PVBOXNETFLTINS vboxNetFltSolarisFindInstance(vboxnetflt_stream_t *pStream);
static mblk_t *vboxNetFltSolarisFixChecksums(mblk_t *pMsg);
static void vboxNetFltSolarisAnalyzeMBlk(mblk_t *pMsg);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The (common) global data. */
static VBOXNETFLTGLOBALS g_VBoxNetFltSolarisGlobals;
/** Global state. */
static vboxnetflt_state_t g_VBoxNetFltSolarisState;
/** GCC C++ hack. */
unsigned __gxx_personality_v0 = 0xdecea5ed;
/** Global Mutex protecting global state. */
RTSEMFASTMUTEX g_VBoxNetFltSolarisMtx = NIL_RTSEMFASTMUTEX;


/**
 * Kernel entry points
 */
int _init(void)
{
    LogFlow((DEVICE_NAME ":_init\n"));

    /*
     * Prevent module autounloading.
     */
    modctl_t *pModCtl = mod_getctl(&g_VBoxNetFltSolarisModLinkage);
    if (pModCtl)
        pModCtl->mod_loadflags |= MOD_NOAUTOUNLOAD;
    else
        LogRel((DEVICE_NAME ":failed to disable autounloading!\n"));

    /*
     * Initialize IPRT.
     */
    int rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize Solaris specific globals here.
         */
        g_VBoxNetFltSolarisState.pOpenedStreams = NULL;
        g_VBoxNetFltSolarisState.pCurInstance = NULL;
        rc = RTSemFastMutexCreate(&g_VBoxNetFltSolarisMtx);
        if (RT_SUCCESS(rc))
        {
            /*
             * Initialize the globals and connect to the support driver.
             *
             * This will call back vboxNetFltOsOpenSupDrv (and maybe vboxNetFltOsCloseSupDrv)
             * for establishing the connect to the support driver.
             */
            memset(&g_VBoxNetFltSolarisGlobals, 0, sizeof(g_VBoxNetFltSolarisGlobals));
            rc = vboxNetFltInitGlobals(&g_VBoxNetFltSolarisGlobals);
            if (RT_SUCCESS(rc))
            {
                rc = mod_install(&g_VBoxNetFltSolarisModLinkage);
                if (!rc)
                    return rc;

                LogRel((DEVICE_NAME ":mod_install failed. rc=%d\n", rc));
                vboxNetFltTryDeleteGlobals(&g_VBoxNetFltSolarisGlobals);                
            }
            else
                LogRel((DEVICE_NAME ":failed to initialize globals.\n"));

            RTSemFastMutexDestroy(g_VBoxNetFltSolarisMtx);
            g_VBoxNetFltSolarisMtx = NIL_RTSEMFASTMUTEX;
        }
        else
            LogRel((DEVICE_NAME ":failed to create mutex.\n"));

        RTR0Term();
    }
    else
        LogRel((DEVICE_NAME ":failed to initialize IPRT (rc=%d)\n", rc));

    memset(&g_VBoxNetFltSolarisGlobals, 0, sizeof(g_VBoxNetFltSolarisGlobals));
    return -1;
}


int _fini(void)
{
    int rc;
    LogFlow((DEVICE_NAME ":_fini\n"));

    /*
     * Undo the work done during start (in reverse order).
     */
    rc = vboxNetFltTryDeleteGlobals(&g_VBoxNetFltSolarisGlobals);
    if (RT_FAILURE(rc))
    {
        LogRel((DEVICE_NAME ":_fini - busy!\n"));
        return EBUSY;
    }

    if (g_VBoxNetFltSolarisMtx != NIL_RTSEMFASTMUTEX)
    {
        RTSemFastMutexDestroy(g_VBoxNetFltSolarisMtx);
        g_VBoxNetFltSolarisMtx = NIL_RTSEMFASTMUTEX;
    }

    RTR0Term();

    return mod_remove(&g_VBoxNetFltSolarisModLinkage);
}


int _info(struct modinfo *pModInfo)
{
    LogFlow((DEVICE_NAME ":_info\n"));

    int rc = mod_info(&g_VBoxNetFltSolarisModLinkage, pModInfo);

    LogFlow((DEVICE_NAME ":_info returns %d\n", rc));
    return rc;
}


/**
 * Attach entry point, to attach a device to the system or resume it.
 *
 * @param   pDip            The module structure instance.
 * @param   enmCmd          Operation type (attach/resume).
 *
 * @returns corresponding solaris error code.
 */
static int VBoxNetFltSolarisAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd)
{
    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisAttach pDip=%p enmCmd=%d\n", pDip, enmCmd));

    switch (enmCmd)
    {
        case DDI_ATTACH:
        {
            int instance = ddi_get_instance(pDip);
            int rc = ddi_create_minor_node(pDip, DEVICE_NAME, S_IFCHR, instance, DDI_PSEUDO, CLONE_DEV);
            if (rc == DDI_SUCCESS)
            {
                g_VBoxNetFltSolarisState.pDip = pDip;
                ddi_report_dev(pDip);
                return DDI_SUCCESS;
            }
            else
                LogRel((DEVICE_NAME ":VBoxNetFltSolarisAttach failed to create minor node. rc%d\n", rc));
            return DDI_FAILURE;
        }

        case DDI_RESUME:
        {
            /* Nothing to do here... */
            return DDI_SUCCESS;
        }
    }
    return DDI_FAILURE;
}


/**
 * Detach entry point, to detach a device to the system or suspend it.
 *
 * @param   pDip            The module structure instance.
 * @param   enmCmd          Operation type (detach/suspend).
 *
 * @returns corresponding solaris error code.
 */
static int VBoxNetFltSolarisDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd)
{
    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisDetach pDip=%p enmCmd=%d\n", pDip, enmCmd));

    switch (enmCmd)
    {
        case DDI_DETACH:
        {
            int instance = ddi_get_instance(pDip);
            ddi_remove_minor_node(pDip, NULL);
            return DDI_SUCCESS;
        }

        case DDI_RESUME:
        {
            /* Nothing to do here... */
            return DDI_SUCCESS;
        }
    }
    return DDI_FAILURE;
}


/**
 * Info entry point, called by solaris kernel for obtaining driver info.
 *
 * @param   pDip            The module structure instance (do not use).
 * @param   enmCmd          Information request type.
 * @param   pvArg           Type specific argument.
 * @param   ppvResult       Where to store the requested info.
 *
 * @returns  corresponding solaris error code.
 */
static int VBoxNetFltSolarisGetInfo(dev_info_t *pDip, ddi_info_cmd_t enmCmd, void *pvArg, void **ppResult)
{
    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisGetInfo pDip=%p enmCmd=%d pArg=%p instance=%d\n", pDip, enmCmd,
                getminor((dev_t)pvArg)));

    switch (enmCmd)
    {
        case DDI_INFO_DEVT2DEVINFO:
        {
            *ppResult = g_VBoxNetFltSolarisState.pDip;
            return DDI_SUCCESS;
        }

        case DDI_INFO_DEVT2INSTANCE:
        {
            int instance = getminor((dev_t)pvArg);
            *ppResult = (void *)(uintptr_t)instance;
            return DDI_SUCCESS;
        }
    }

    return DDI_FAILURE;
}


/**
 * Stream module open entry point, initializes the queue and allows streams processing.
 *
 * @param   pQueue          Pointer to the queue (cannot be NULL).
 * @param   pDev            Pointer to the dev_t associated with the driver at the end of the stream.
 * @param   fOpenMode       Open mode (always 0 for streams driver, thus ignored).
 * @param   fStreamMode     Stream open mode.
 * @param   pCred           Pointer to user credentials.
 *
 * @returns corresponding solaris error code.
 */
static int VBoxNetFltSolarisModOpen(queue_t *pQueue, dev_t *pDev, int fOpenMode, int fStreamMode, cred_t *pCred)
{
    Assert(pQueue);

    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModOpen pQueue=%p pDev=%p fOpenMode=%d fStreamMode=%d\n", pQueue, pDev,
            fOpenMode, fStreamMode));

    /*
     * Already open?
     */
    if (pQueue->q_ptr)
    {
        LogRel((DEVICE_NAME ":VBoxNetFltSolarisModOpen invalid open.\n"));
        return ENOENT;
    }

    /*
     * Check for the VirtualBox instance.
     */
    vboxnetflt_state_t *pState = &g_VBoxNetFltSolarisState;
    if (RT_UNLIKELY(!pState->pCurInstance))
    {
        LogRel((DEVICE_NAME ":VBoxNetFltSolarisModOpen failed to get VirtualBox instance.\n"));
        return ENOENT;
    }

    /*
     * Check VirtualBox stream type.
     */
    if (pState->CurType == kUndefined)
    {
        LogRel((DEVICE_NAME ":VBoxNetFltSolarisModOpen failed due to undefined VirtualBox open mode.\n"));
        return ENOENT;
    }

    /*
     * Get minor number. For clone opens provide a new dev_t.
     */
    minor_t DevMinor = 0;
    vboxnetflt_stream_t *pStream = NULL;
    vboxnetflt_stream_t **ppPrevStream = &pState->pOpenedStreams;
    if (fStreamMode == CLONEOPEN)
    {
        for (; (pStream = *ppPrevStream) != NULL; ppPrevStream = &pStream->pNext)
        {
            if (DevMinor < pStream->DevMinor)
                break;
            DevMinor++;
        }
        *pDev = makedevice(getmajor(*pDev), DevMinor);
    }
    else
        DevMinor = getminor(*pDev);

    /*
     * Allocate & initialize per-stream data. Hook it into the (read and write) queue's module specific data.
     */
    pStream = RTMemAlloc(sizeof(*pStream));
    if (RT_UNLIKELY(!pStream))
    {
        LogRel((DEVICE_NAME ":VBoxNetFltSolarisModOpen failed to allocate stream data.\n"));
        return ENOMEM;
    }
    pStream->DevMinor = DevMinor;
    pStream->pReadQueue = pQueue;
    pStream->pArpReadQueue = NULL;
    pStream->pState = pState;
    pStream->fPromisc = false;
    pStream->fRawMode = false;

    /*
     * Pick up the current global VBOXNETFLTINS instance as
     * the one that we will associate this stream with.
     */
    pStream->pThis = pState->pCurInstance;
    pStream->Type = pState->CurType;
    if (pState->CurType == kIpStream)
        pState->pCurInstance->u.s.pvStream = pStream;
    else
        pState->pCurInstance->u.s.pvArpStream = pStream;

    pStream->ModeReqId = 0;
    pStream->UnAckPrim = 0;
    pQueue->q_ptr = pStream;
    WR(pQueue)->q_ptr = pStream;

    /*
     * Link it to the list of streams.
     */
    pStream->pNext = *ppPrevStream;
    *ppPrevStream = pStream;

    qprocson(pQueue);

    /*
     * Request the physical address (we cache the acknowledgement).
     */
    if (pStream->Type == kIpStream)
        vboxNetFltSolarisPhysAddrReq(pStream->pReadQueue);

    /*
     * Enable raw mode.
     */
    if (pStream->Type == kIpStream)
        vboxNetFltSolarisSetRawMode(pStream->pReadQueue);

    pStream->pThis->fDisconnectedFromHost = false;

    NOREF(fOpenMode);
    NOREF(pCred);

    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModOpen returns 0, DevMinor=%d pQueue=%p\n", DevMinor, pStream->pReadQueue));

    return 0;
}


/**
 * Stream module close entry point, undoes the work done on open and closes the stream.
 *
 * @param   pQueue          Pointer to the queue (cannot be NULL).
 * @param   fOpenMode       Open mode (always 0 for streams driver, thus ignored).
 * @param   pCred           Pointer to user credentials.
 *
 * @returns  corresponding solaris error code.
 */
static int VBoxNetFltSolarisModClose(queue_t *pQueue, int fOpenMode, cred_t *pCred)
{
    Assert(pQueue);

    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModClose pQueue=%p fOpenMode=%d\n", pQueue, fOpenMode));

    vboxnetflt_stream_t *pStream = NULL;
    vboxnetflt_stream_t **ppPrevStream = NULL;
    vboxnetflt_state_t *pState = NULL;

    /*
     * Get instance data.
     */
    pStream = (vboxnetflt_stream_t *)pQueue->q_ptr;
    if (RT_UNLIKELY(!pStream))
    {
        LogRel((DEVICE_NAME ":VBoxNetFltSolarisModClose failed to get stream.\n"));
        return ENXIO;
    }
    pState = pStream->pState;
    Assert(pState);

    /*
     * Enable native mode.
     */
    if (pStream->Type == kIpStream)
        vboxNetFltSolarisSetFastMode(pStream->pReadQueue);

    pStream->pThis->fDisconnectedFromHost = true;
    qprocsoff(pQueue);

    /*
     * Unlink it from the list of streams.
     */
    for (ppPrevStream = &pState->pOpenedStreams; (pStream = *ppPrevStream) != NULL; ppPrevStream = &pStream->pNext)
        if (pStream == (vboxnetflt_stream_t *)pQueue->q_ptr)
            break;
    *ppPrevStream = pStream->pNext;

    /*
     * Delete the stream.
     */
    if (pStream->Type == kIpStream)
        pStream->pThis->u.s.pvStream = NULL;
    else
        pStream->pThis->u.s.pvArpStream = NULL;
    RTMemFree(pStream);
    pQueue->q_ptr = NULL;
    WR(pQueue)->q_ptr = NULL;

    NOREF(fOpenMode);
    NOREF(pCred);

    return 0;
}


/**
 * Read side put procedure for processing messages in the read queue.
 *
 * @param   pQueue      Pointer to the queue.
 * @param   pMsg        Pointer to the message.
 *
 * @returns corresponding solaris error code.
 */
static int VBoxNetFltSolarisModReadPut(queue_t *pQueue, mblk_t *pMsg)
{
    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModReadPut pQueue=%p pMsg=%p\n"));

    bool fSendUpstream = true;
    vboxnetflt_stream_t *pStream = pQueue->q_ptr;

    /*
     * In the unlikely case where VirtualBox crashed and this filter
     * is somehow still in the host stream we must try not to panic the host.
     */
    if (   pStream
        && pStream->Type == kIpStream
        && pMsg)
    {
        PVBOXNETFLTINS pThis = vboxNetFltSolarisFindInstance(pStream);
        if (RT_LIKELY(pThis))
        {
            switch (DB_TYPE(pMsg))
            {
                case M_DATA:
                {
                    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModReadPut M_DATA\n"));

                    vboxNetFltSolarisRecv(pThis, pStream, pQueue, pMsg);
                    fSendUpstream = false;          /* vboxNetFltSolarisRecv would send it if required, do nothing more here. */
                    break;
                }

                case M_PROTO:
                case M_PCPROTO:
                {
                    union DL_primitives *pPrim = (union DL_primitives *)pMsg->b_rptr;
                    t_uscalar_t Prim = pPrim->dl_primitive;

                    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModReadPut: M_PCPROTO %d\n", Prim));
                    switch (Prim)
                    {
                        case DL_UNITDATA_IND:
                        {
                            /*
                             * I do not think control would come here... We convert all outgoing fast mode requests
                             * to raw mode; so I don't think we should really receive any fast mode replies.
                             */
                            LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModReadPut: DL_UNITDATA_IND\n"));

                            vboxNetFltSolarisRecv(pThis, pStream, pQueue, pMsg);
                            fSendUpstream = false;      /* vboxNetFltSolarisRecv would send it if required, do nothing more here. */
                            break;
                        }

                        case DL_PHYS_ADDR_ACK:
                        {
                            vboxNetFltSolarisCachePhysAddr(pThis, pMsg);

                            /*
                             * Swallow our fake physical address request acknowledgement.
                             */
                            if (pStream->UnAckPrim == DL_PHYS_ADDR_REQ)
                            {
                                freemsg(pMsg);
                                fSendUpstream = false;
                            }
                            break;
                        }

                        case DL_OK_ACK:
                        {
                            dl_ok_ack_t *pOkAck = (dl_ok_ack_t *)pMsg->b_rptr;
                            if (pOkAck->dl_correct_primitive == DL_PROMISCON_REQ)
                            {
                                LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModReadPut: M_PCPROTO: DL_OK_ACK: fPromisc is ON.\n"));
                                pStream->fPromisc = true;
                            }
                            else if (pOkAck->dl_correct_primitive == DL_PROMISCOFF_REQ)
                            {
                                LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModReadPut: M_PCPROTO: DL_OK_ACK: fPromisc is OFF.\n"));
                                pStream->fPromisc = false;
                            }

                            /*
                             * Swallow our fake promiscous request acknowledgement.
                             */
                            if (pStream->UnAckPrim == pOkAck->dl_correct_primitive)
                            {
                                freemsg(pMsg);
                                fSendUpstream = false;
                            }
                            break;
                        }
                    }
                    break;
                }

                case M_IOCACK:
                {
                    /*
                     * Swallow our fake raw/fast path mode request acknowledgement.
                     */
                    struct iocblk *pIOC = (struct iocblk *)pMsg->b_rptr;
                    if (pIOC->ioc_id == pStream->ModeReqId)
                    {
                        pStream->ModeReqId = VBOXNETFLT_MODE_REQ_MAGIC;
                        pStream->fRawMode = !pStream->fRawMode;

                        /*
                         * Somehow raw mode is turned off?? This should never really happen...
                         */
                        if (pStream->fRawMode == false)
                        {
                            LogFlow((DEVICE_NAME ":re-requesting raw mode!\n"));
                            vboxNetFltSolarisSetRawMode(pQueue);
                        }

                        freemsg(pMsg);
                        fSendUpstream = false;
                    }
                    break;
                }

                case M_FLUSH:
                {
                    /*
                     * We must support flushing queues.
                     */
                    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModReadPut: M_FLUSH\n"));
                    if (*pMsg->b_rptr & FLUSHR)
                        flushq(pQueue, FLUSHALL);
                    break;
                }
            }
        }
        else
            LogRel((DEVICE_NAME ":VBoxNetFltSolarisModReadPut: Could not find VirtualBox instance!!\n"));
    }

    if (   fSendUpstream
        && pMsg)
    {
        /*
         * Pass foward high priority messages or when there's no flow control
         * on an empty queue, otherwise queue them.
         */
        if (   queclass(pMsg) == QPCTL
            || (pQueue->q_first == NULL && canputnext(pQueue)))
        {
            putnext(pQueue, pMsg);
        }
        else
            putq(pQueue, pMsg);
    }

    return 0;
}


/**
 * Write side put procedure for processing messages in the write queue.
 *
 * @param   pQueue      Pointer to the queue.
 * @param   pMsg        Pointer to the message.
 *
 * @returns corresponding solaris error code.
 */
static int VBoxNetFltSolarisModWritePut(queue_t *pQueue, mblk_t *pMsg)
{
    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModWritePut pQueue=%p pMsg=%p\n", pQueue, pMsg));

    /*
     * Check for the VirtualBox connection.
     */
    bool fSendDownstream = true;
    vboxnetflt_stream_t *pStream = pQueue->q_ptr;

    /*
     * In the unlikely case where VirtualBox crashed and this filter
     * is somehow still in the host stream we must try not to panic the host.
     */
    if (   pStream
        && pStream->Type == kIpStream
        && pMsg)
    {
        PVBOXNETFLTINS pThis = vboxNetFltSolarisFindInstance(pStream);
        if (RT_LIKELY(pThis))
        {
            switch (DB_TYPE(pMsg))
            {
                case M_DATA:
                {
                    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModWritePut M_DATA\n"));
                    break;
                }

                case M_PROTO:
                case M_PCPROTO:
                {
                    /*
                     * Queue up other primitives to the service routine.
                     */
                    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModWritePut M_PROTO/M_PCPROTO\n"));

                    union DL_primitives *pPrim = (union DL_primitives *)pMsg->b_rptr;
                    t_uscalar_t Prim = pPrim->dl_primitive;
                    switch (Prim)
                    {
                        case DL_UNITDATA_REQ:
                        {
                            LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModWritePut DL_UNITDATA_REQ\n"));
                            mblk_t *pRawMsg;
                            int rc = vboxNetFltSolarisUnitDataToRaw(pThis, pMsg, &pRawMsg);
                            if (VBOX_SUCCESS(rc))
                                pMsg = pRawMsg;
                            else
                                fSendDownstream = false;
                            break;
                        }

                        default:
                        {
                            /*
                             * Enqueue other DLPI primitives and service them later.
                             */
                            fSendDownstream = false;
                            putq(pQueue, pMsg);
                            break;
                        }
                    }
                    break;
                }

                case M_IOCTL:
                {
                    struct iocblk *pIOC = (struct iocblk *)pMsg->b_rptr;
                    if (pIOC->ioc_cmd == DL_IOC_HDR_INFO)
                    {
                        if (pThis->fActive)
                        {
                            /*
                             * Somebody is wanting fast path when we need raw mode.
                             * Since we are evil, let's acknowledge the request ourselves!
                             */
                            miocack(pQueue, pMsg, 0, EINVAL);
                            fSendDownstream = false;
                            LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModWritePut: Fast path request when we need raw mode!\n"));
                        }
                    }
                    break;
                }

                case M_FLUSH:
                {
                    /*
                     * Canonical flush courtesy man qreply(9F) while we have a service routine.
                     */
                    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModWritePut M_FLUSH\n"));
                    if (*pMsg->b_rptr & FLUSHW)
                    {
                        /*
                         * Flush and mark as serviced.
                         */
                        flushq(pQueue, FLUSHALL);
                        *pMsg->b_rptr &= ~FLUSHW;
                    }

                    if (*pMsg->b_rptr & FLUSHR)
                    {
                        /*
                         * Send the request upstream.
                         */
                        flushq(RD(pQueue), FLUSHALL);
                        qreply(pQueue, pMsg);
                    }
                    else
                        freemsg(pMsg);

                    break;
                }
            }
        }
        else
            LogRel((DEVICE_NAME ":VBoxNetFltSolarisModWritePut: Could not find VirtualBox instance!!\n"));
    }

    if (   fSendDownstream
        && pMsg)
    {
        /*
         * Pass foward high priority messages or when there's no flow control
         * on an empty queue, otherwise queue them.
         */
        if (   queclass(pMsg) == QPCTL
            || (pQueue->q_first == NULL && canputnext(pQueue)))
        {
            putnext(pQueue, pMsg);
        }
        else
            putq(pQueue, pMsg);
    }

    return 0;
}


/**
 * Write side service procedure for deferred message processing on the write queue.
 *
 * @param   pQueue      Pointer to the queue.
 *
 * @returns corresponding solaris error code.
 */
static int VBoxNetFltSolarisModWriteService(queue_t *pQueue)
{
    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisModWriteService pQueue=%p\n", pQueue));

    /*
     * Implment just the flow controlled service draining of the queue.
     * Nothing else to do here, we handle all the important stuff in the Put procedure.
     */
    mblk_t *pMsg;
    while (pMsg = getq(pQueue))
    {
        if (canputnext(pQueue))
            putnext(pQueue, pMsg);
        else
        {
            putbq(pQueue, pMsg);
            break;
        }
    }

    return 0;
}


/**
 * Put the stream in raw mode.
 *
 * @returns VBox status code.
 * @param   pQueue      Pointer to the queue.
 */
static int vboxNetFltSolarisSetRawMode(queue_t *pQueue)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisSetRawMode pQueue=%p\n", pQueue));

    mblk_t *pRawMsg = NULL;
    pRawMsg = mkiocb(DLIOCRAW);
    if (RT_UNLIKELY(!pRawMsg))
        return VERR_NO_MEMORY;

    vboxnetflt_stream_t *pStream = pQueue->q_ptr;
    struct iocblk *pIOC = (struct iocblk *)pRawMsg->b_rptr;
    pStream->ModeReqId = pIOC->ioc_id;
    pIOC->ioc_count = 0;

    qreply(pQueue, pRawMsg);
    return VINF_SUCCESS;
}


/**
 * Put the stream back in fast path mode.
 *
 * @returns VBox status code.
 * @param   pQueue      Pointer to the queue.
 */
static int vboxNetFltSolarisSetFastMode(queue_t *pQueue)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisSetFastMode pQueue=%p\n", pQueue));

    mblk_t *pFastMsg = mkiocb(DL_IOC_HDR_INFO);
    if (RT_UNLIKELY(!pFastMsg))
        return VERR_NO_MEMORY;

    vboxnetflt_stream_t *pStream = pQueue->q_ptr;
    struct iocblk *pIOC = (struct iocblk *)pFastMsg->b_rptr;
    pStream->ModeReqId = pIOC->ioc_id;

    size_t cbReq = sizeof(dl_unitdata_req_t) + sizeof(vboxnetflt_dladdr_t);
    mblk_t *pDataReqMsg = allocb(cbReq, BPRI_MED);
    if (RT_UNLIKELY(!pDataReqMsg))
        return VERR_NO_MEMORY;

    DB_TYPE(pDataReqMsg) = M_PROTO;
    dl_unitdata_req_t *pDataReq = (dl_unitdata_req_t *)pDataReqMsg->b_rptr;
    pDataReq->dl_primitive = DL_UNITDATA_REQ;
    pDataReq->dl_dest_addr_length = sizeof(vboxnetflt_dladdr_t);
    pDataReq->dl_dest_addr_offset = sizeof(dl_unitdata_req_t);
    pDataReq->dl_priority.dl_min = 0;
    pDataReq->dl_priority.dl_max = 0;

    bzero(pDataReqMsg->b_rptr + sizeof(dl_unitdata_req_t), sizeof(vboxnetflt_dladdr_t));
    pDataReqMsg->b_wptr = pDataReqMsg->b_rptr + cbReq;

    /*
     * Link the data format request message into the header ioctl message.
     */
    pFastMsg->b_cont = pDataReqMsg;
    pIOC->ioc_count = msgdsize(pDataReqMsg);

    qreply(pQueue, pFastMsg);
    return VINF_SUCCESS;
}


/**
 * Send fake promiscous mode requests downstream.
 *
 * @param   pQueue          Pointer to the queue.
 * @param   fPromisc        Whether to enable promiscous mode or not.
 * @param   PromiscLevel    Promiscous level; DL_PROMISC_PHYS/SAP/MULTI.
 *
 * @returns VBox error code.
 */
static int vboxNetFltSolarisPromiscReq(queue_t *pQueue, bool fPromisc)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisPromiscReq pQueue=%p fPromisc=%d\n", pQueue, fPromisc));

    t_uscalar_t Cmd;
    size_t cbReq = 0;
    if (fPromisc)
    {
        Cmd = DL_PROMISCON_REQ;
        cbReq = DL_PROMISCON_REQ_SIZE;
    }
    else
    {
        Cmd = DL_PROMISCOFF_REQ;
        cbReq = DL_PROMISCOFF_REQ_SIZE;
    }

    mblk_t *pPromiscPhysMsg = mexchange(NULL, NULL, cbReq, M_PROTO, Cmd);
    if (RT_UNLIKELY(!pPromiscPhysMsg))
        return VERR_NO_MEMORY;

    mblk_t *pPromiscSapMsg = mexchange(NULL, NULL, cbReq, M_PROTO, Cmd);
    if (RT_UNLIKELY(!pPromiscSapMsg))
    {
        freemsg(pPromiscPhysMsg);
        return VERR_NO_MEMORY;
    }

    vboxnetflt_stream_t *pStream = pQueue->q_ptr;
    pStream->UnAckPrim = Cmd;

    if (fPromisc)
    {
        ((dl_promiscon_req_t *)pPromiscPhysMsg->b_rptr)->dl_level = DL_PROMISC_PHYS;
        ((dl_promiscon_req_t *)pPromiscSapMsg->b_rptr)->dl_level = DL_PROMISC_SAP;
    }
    else
    {
        ((dl_promiscoff_req_t *)pPromiscPhysMsg->b_rptr)->dl_level = DL_PROMISC_PHYS;
        ((dl_promiscoff_req_t *)pPromiscSapMsg->b_rptr)->dl_level = DL_PROMISC_SAP;
    }

    qreply(pQueue, pPromiscPhysMsg);
    qreply(pQueue, pPromiscSapMsg);

    return VINF_SUCCESS;
}


/**
 * Send a fake physical address request downstream.
 *
 * @returns VBox status code.
 * @param   pQueue      Pointer to the queue.
 * @param   pMsg        Pointer to the request message.
 */
static int vboxNetFltSolarisPhysAddrReq(queue_t *pQueue)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisPhysAddrReq pQueue=%p\n", pQueue));

    t_uscalar_t Cmd = DL_PHYS_ADDR_REQ;
    size_t cbReq = DL_PHYS_ADDR_REQ_SIZE;
    mblk_t *pPhysAddrMsg = mexchange(NULL, NULL, cbReq, M_PROTO, Cmd);
    if (RT_UNLIKELY(!pPhysAddrMsg))
        return VERR_NO_MEMORY;

    vboxnetflt_stream_t *pStream = pQueue->q_ptr;
    pStream->UnAckPrim = Cmd;

    dl_phys_addr_req_t *pPhysAddrReq = (dl_phys_addr_req_t *)pPhysAddrMsg->b_rptr;
    pPhysAddrReq->dl_addr_type = DL_CURR_PHYS_ADDR;

    qreply(pQueue, pPhysAddrMsg);
    return VERR_GENERAL_FAILURE;
}


/**
 * Cache the MAC address into the VirtualBox instance given a physical
 * address acknowledgement message.
 *
 * @param   pThis       The instance.
 * @param   pMsg        Pointer to the physical address acknowledgement message.
 */
static void vboxNetFltSolarisCachePhysAddr(PVBOXNETFLTINS pThis, mblk_t *pMsg)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisCachePhysAddr pThis=%p pMsg=%p\n", pThis, pMsg));

    AssertCompile(sizeof(RTMAC) == ETHERADDRL);
    dl_phys_addr_ack_t *pPhysAddrAck = (dl_phys_addr_ack_t *)pMsg->b_rptr;
    if (pPhysAddrAck->dl_addr_length == sizeof(pThis->u.s.Mac))
    {
        bcopy(pMsg->b_rptr + pPhysAddrAck->dl_addr_offset, &pThis->u.s.Mac, sizeof(pThis->u.s.Mac));

        LogFlow((DEVICE_NAME ":vboxNetFltSolarisCachePhysAddr: DL_PHYS_ADDR_ACK: Mac=%.*Rhxs\n", sizeof(pThis->u.s.Mac),
                    &pThis->u.s.Mac));
    }
}


/**
 * Opens the required device and returns the vnode_t associated with it.
 * We require this for the funny attach/detach routine.
 *
 * @returns VBox status code.
 * @param   pszDev          The device path.
 * @param   ppVNode         Where to store the vnode_t pointer associated with the opened device.
 * @param   ppVNodeHeld     Where to store the vnode_t required during closing of the device.
 * @param   ppUser          Open handle required while closing the device.
 */
static int vboxNetFltSolarisOpenDev(char *pszDev, vnode_t **ppVNode, vnode_t **ppVNodeHeld, TIUSER **ppUser)
{
    int rc;
    vnode_t *pVNodeHeld = NULL;
    rc = lookupname(pszDev, UIO_SYSSPACE, FOLLOW, NULLVPP, &pVNodeHeld);
    if (!rc)
    {
        TIUSER *pUser;
        rc = t_kopen((file_t *)NULL, pVNodeHeld->v_rdev, FREAD | FWRITE, &pUser, kcred);
        if (!rc)
        {
            *ppVNode = pUser->fp->f_vnode;
            *ppVNodeHeld = pVNodeHeld;
            *ppUser = pUser;
            return VINF_SUCCESS;
        }
        VN_RELE(pVNodeHeld);
    }
    return VERR_PATH_NOT_FOUND;
}


/**
 * Close the device opened using vboxNetFltSolarisOpenDev.
 *
 * @param   pVNodeHeld      Pointer to the held vnode of the device.
 * @param   pUser           Pointer to the file handle.
 */
static void vboxNetFltSolarisCloseDev(vnode_t *pVNodeHeld, TIUSER *pUser)
{
    t_kclose(pUser, 0);
    VN_RELE(pVNodeHeld);
}


/**
 * Get the logical interface flags from the stream.
 *
 * @returns VBox status code.
 * @param   hDevice        Layered device handle.
 * @param   pInterface     Pointer to the interface.
 */
static int vboxNetFltSolarisGetIfFlags(ldi_handle_t hDevice, struct lifreq *pInterface)
{
    struct strioctl IOCReq;
    int rc;
    int ret;
    IOCReq.ic_cmd = SIOCGLIFFLAGS;
    IOCReq.ic_timout = 40;
    IOCReq.ic_len = sizeof(struct lifreq);
    IOCReq.ic_dp = (caddr_t)pInterface;
    rc = ldi_ioctl(hDevice, I_STR, (intptr_t)&IOCReq, FKIOCTL, kcred, &ret);
    if (!rc)
        return VINF_SUCCESS;

    return RTErrConvertFromErrno(rc);
}


/**
 * Sets the multiplexor ID from the interface.
 *
 * @returns VBox status code.
 * @param   pVNode      Pointer to the device vnode.
 * @param   pInterface  Pointer to the interface.
 */
static int vboxNetFltSolarisSetMuxId(vnode_t *pVNode, struct lifreq *pInterface)
{
    struct strioctl IOCReq;
    int rc;
    int ret;
    IOCReq.ic_cmd = SIOCSLIFMUXID;
    IOCReq.ic_timout = 40;
    IOCReq.ic_len = sizeof(struct lifreq);
    IOCReq.ic_dp = (caddr_t)pInterface;

    rc = strioctl(pVNode, I_STR, (intptr_t)&IOCReq, 0, K_TO_K, kcred, &ret);
    if (!rc)
        return VINF_SUCCESS;

    return RTErrConvertFromErrno(rc);
}


/**
 * Get the multiplexor file descriptor of the lower stream.
 *
 * @returns VBox status code.
 * @param   MuxId   The multiplexor ID.
 * @param   pFd     Where to store the lower stream file descriptor.
 */
static int vboxNetFltSolarisMuxIdToFd(vnode_t *pVNode, int MuxId, int *pFd)
{
    int ret;
    int rc = strioctl(pVNode, _I_MUXID2FD, (intptr_t)MuxId, 0, K_TO_K, kcred, &ret);
    if (!rc)
    {
        *pFd = ret;
        return VINF_SUCCESS;
    }

    return RTErrConvertFromErrno(rc);
}


/**
 * Relinks the lower and the upper stream.
 *
 * @returns VBox status code.
 * @param   pVNode      Pointer to the device vnode.
 * @param   pInterface  Pointer to the interface.
 * @param   IpMuxFd     The IP multiplexor ID.
 * @param   ArpMuxFd    The ARP multiplexor ID.
 */
static int vboxNetFltSolarisRelink(vnode_t *pVNode, struct lifreq *pInterface, int IpMuxFd, int ArpMuxFd)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisRelink: pVNode=%p pInterface=%p IpMuxFd=%d ArpMuxFd=%d\n", pVNode,
            pInterface, IpMuxFd, ArpMuxFd));

    int NewIpMuxId;
    int NewArpMuxId;
    int rc = strioctl(pVNode, I_PLINK, (intptr_t)IpMuxFd, 0, K_TO_K, kcred, &NewIpMuxId);
    int rc2 = strioctl(pVNode, I_PLINK, (intptr_t)ArpMuxFd, 0, K_TO_K, kcred, &NewArpMuxId);
    if (   !rc
        && !rc2)
    {
        pInterface->lifr_ip_muxid = NewIpMuxId;
        pInterface->lifr_arp_muxid = NewArpMuxId;
        rc = vboxNetFltSolarisSetMuxId(pVNode, pInterface);
        if (VBOX_SUCCESS(rc))
            return VINF_SUCCESS;

        LogRel((DEVICE_NAME ":vboxNetFltSolarisRelink: failed to set new Mux Id.\n"));
    }
    else
        LogRel((DEVICE_NAME ":vboxNetFltSolarisRelink: failed to link.\n"));

    return VERR_GENERAL_FAILURE;
}


/**
 * Dynamically find the position on the host stack where to attach/detach ourselves.
 *
 * @returns VBox status code.
 * @param   pVNode      Pointer to the lower stream vnode.
 * @param   pModPos     Where to store the module position.
 */
static int vboxNetFltSolarisDetermineModPos(bool fAttach, vnode_t *pVNode, int *pModPos)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisDetermineModPos: fAttach=%d pVNode=%p pModPos=%p\n", fAttach, pVNode, pModPos));

    int cMod;
    int rc = strioctl(pVNode, I_LIST, (intptr_t)NULL, 0, K_TO_K, kcred, &cMod);
    if (!rc)
    {
        if (cMod < 1)
        {
            LogRel((DEVICE_NAME ":vboxNetFltSolarisDetermineModPos: too few modules on host interface. cMod=%d\n"));
            return VERR_OUT_OF_RANGE;
        }

        /*
         * While attaching we make sure we are at the bottom most of the stack, excepting
         * the host driver.
         */
        LogFlow((DEVICE_NAME ":vboxNetFltSolarisDetermineModPos: cMod=%d\n", cMod));
        if (fAttach)
        {
            *pModPos = cMod - 1;
            return VINF_SUCCESS;
        }

        /*
         * Detaching is a bit more complicated; since user could have altered the stack positions
         * we take the safe approach by finding our position.
         */
        struct str_list StrList;
        StrList.sl_nmods = cMod;
        StrList.sl_modlist = RTMemAllocZ(cMod * sizeof(struct str_list));
        if (RT_UNLIKELY(!StrList.sl_modlist))
        {
            LogFlow((DEVICE_NAME ":vboxNetFltSolarisDetermineModPos: failed to alloc memory for StrList.\n"));
            return VERR_NO_MEMORY;
        }

        /*
         * Get the list of all modules on the stack.
         */
        int ret;
        rc = strioctl(pVNode, I_LIST, (intptr_t)&StrList, 0, K_TO_K, kcred, &ret);
        if (!rc)
        {
            /*
             * Find our filter.
             */
            for (int i = 0; i < StrList.sl_nmods; i++)
            {
                if (!strcmp(DEVICE_NAME, StrList.sl_modlist[i].l_name))
                {
                    LogFlow((DEVICE_NAME ":vboxNetFltSolarisDetermineModPos: Success! Found %s at %d.\n", DEVICE_NAME, i));
                    *pModPos = i;
                    return VINF_SUCCESS;
                }
            }

            LogRel((DEVICE_NAME ":vboxNetFltSolarisDetermineModPos: failed to find %s in the host stack.\n"));
        }
        else
            LogRel((DEVICE_NAME ":vboxNetFltSolarisDetermineModPos: failed to get module information. rc=%d\n"));
    }
    else
        LogRel((DEVICE_NAME ":vboxNetFltSolarisDetermineModPos: failed to get list of modules on host interface. rc=%d\n", rc));
    return VERR_GENERAL_FAILURE;
}


/**
 * Dynamically attaches this streams module on to the host stack.
 * As a side-effect, this streams also gets opened/closed during
 * the actual injection/ejection phase.
 *
 * @returns VBox status code.
 * @param   pThis       The instance.
 * @param   fAttach     Is this an attach or detach.
 */
static int vboxNetFltSolarisModSetup(PVBOXNETFLTINS pThis, bool fAttach)
{
    LogFlow(("vboxNetFltSolarisModSetup: pThis=%p (%s) fAttach=%s\n", pThis, pThis->szName, fAttach ? "true" : "false"));

    /*
     * Statuatory Warning: Hackish code ahead.
     */
    if (strlen(pThis->szName) > VBOXNETFLT_IFNAME_LEN - 1)
    {
        LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: interface name too long %s\n", pThis->szName));
        return VERR_INTNET_FLT_IF_NOT_FOUND;
    }

    char *pszModName = DEVICE_NAME;

    struct lifreq Interface;
    bzero(&Interface, sizeof(Interface));
    Interface.lifr_addr.ss_family = AF_INET;
    strncpy(Interface.lifr_name, pThis->szName, sizeof(Interface.lifr_name));

    struct strmodconf StrMod;
    StrMod.mod_name = pszModName;
    StrMod.pos = -1;        /* this is filled in later. */

    struct strmodconf ArpStrMod;
    bcopy(&StrMod, &ArpStrMod, sizeof(StrMod));

    int rc;
    int rc2;
    int ret;
    ldi_ident_t IPDevId = ldi_ident_from_anon();
    ldi_ident_t ARPDevId = ldi_ident_from_anon();
    ldi_handle_t IPDevHandle;
    ldi_handle_t UDPDevHandle;
    ldi_handle_t ARPDevHandle;

    /*
     * Open the IP and ARP streams as layered devices.
     */
    rc = ldi_open_by_name(IP_DEV_NAME, FREAD | FWRITE, kcred, &IPDevHandle, IPDevId);
    ldi_ident_release(IPDevId);
    if (rc)
    {
        LogRel((DEVICE_NAME ":failed to open the IP stream on '%s'.\n", pThis->szName));
        return VERR_INTNET_FLT_IF_FAILED;
    }

    rc = ldi_open_by_name("/dev/arp", FREAD | FWRITE, kcred, &ARPDevHandle, ARPDevId);
    ldi_ident_release(ARPDevId);
    if (rc)
    {
        LogRel((DEVICE_NAME ":failed to open the ARP stream on '%s'.\n", pThis->szName));
        ldi_close(IPDevHandle, FREAD | FWRITE, kcred);
        return VERR_INTNET_FLT_IF_FAILED;
    }

    /*
     * Obtain the interface flags from IP.
     */
    rc = vboxNetFltSolarisGetIfFlags(IPDevHandle, &Interface);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Open the UDP stream. We sort of cheat here and obtain the vnode so that we can perform
         * things that are not possible from the layered interface.
         */
        vnode_t *pVNodeUDP = NULL;
        vnode_t *pVNodeUDPHeld = NULL;
        TIUSER *pUserUDP = NULL;
        rc = vboxNetFltSolarisOpenDev(UDP_DEV_NAME, &pVNodeUDP, &pVNodeUDPHeld, &pUserUDP);
        if (VBOX_SUCCESS(rc))
        {
            /*
             * Get the multiplexor IDs.
             */
            rc = ldi_ioctl(IPDevHandle, SIOCGLIFMUXID, (intptr_t)&Interface, FKIOCTL, kcred, &ret);
            if (!rc)
            {
                /*
                 * Get the multiplex file descriptor to the lower streams. Generally this is lost
                 * once a module is I_PLINK, we need to reobtain it for inserting/removing ourselves from the stack.
                 */
                int IpMuxFd;
                int ArpMuxFd;
                rc = vboxNetFltSolarisMuxIdToFd(pVNodeUDP, Interface.lifr_ip_muxid, &IpMuxFd);
                rc2 = vboxNetFltSolarisMuxIdToFd(pVNodeUDP, Interface.lifr_arp_muxid, &ArpMuxFd);
                if (   VBOX_SUCCESS(rc)
                    && VBOX_SUCCESS(rc2))
                {
                    /*
                     * We need to I_PUNLINK on these multiplexor IDs before we can start
                     * operating on the lower stream as insertions are direct operations on the lower stream.
                     */
                    int ret;
                    rc = strioctl(pVNodeUDP, I_PUNLINK, (intptr_t)Interface.lifr_ip_muxid, 0, K_TO_K, kcred, &ret);
                    rc2 = strioctl(pVNodeUDP, I_PUNLINK, (intptr_t)Interface.lifr_arp_muxid, 0, K_TO_K, kcred, &ret);
                    if (   !rc
                        && !rc2)
                    {
                        /*
                         * Obtain the vnode from the useless userland file descriptor.
                         */
                        file_t *pIpFile = getf(IpMuxFd);
                        file_t *pArpFile = getf(ArpMuxFd);
                        if (   pIpFile
                            && pArpFile
                            && pArpFile->f_vnode
                            && pIpFile->f_vnode)
                        {
                            vnode_t *pVNodeIp = pIpFile->f_vnode;
                            vnode_t *pVNodeArp = pArpFile->f_vnode;

                            /*
                             * Find the position on the host stack for attaching/detaching ourselves.
                             */
                            rc = vboxNetFltSolarisDetermineModPos(fAttach, pVNodeIp, &StrMod.pos);
                            rc2 = vboxNetFltSolarisDetermineModPos(fAttach, pVNodeArp, &ArpStrMod.pos);
                            if (   VBOX_SUCCESS(rc)
                                && VBOX_SUCCESS(rc2))
                            {
                                /*
                                 * Set global data which will be grabbed by ModOpen.
                                 * There is a known (though very unlikely) race here because
                                 * of the inability to pass user data while inserting.
                                 */
                                g_VBoxNetFltSolarisState.pCurInstance = pThis;
                                g_VBoxNetFltSolarisState.CurType = kIpStream;

                                /*
                                 * Inject/Eject from the host IP stack.
                                 */
                                rc = strioctl(pVNodeIp, fAttach ? _I_INSERT : _I_REMOVE, (intptr_t)&StrMod, 0, K_TO_K,
                                            kcred, &ret);
                                if (!rc)
                                {
                                    /*
                                     * Inject/Eject from the host ARP stack.
                                     */
                                    g_VBoxNetFltSolarisState.CurType = kArpStream;
                                    rc = strioctl(pVNodeArp, fAttach ? _I_INSERT : _I_REMOVE, (intptr_t)&ArpStrMod, 0, K_TO_K,
                                                kcred, &ret);
                                    if (!rc)
                                    {
                                        g_VBoxNetFltSolarisState.pCurInstance = NULL;
                                        g_VBoxNetFltSolarisState.CurType = kUndefined;

                                        /*
                                         * Our job's not yet over; we need to relink the upper and lower streams
                                         * otherwise we've pretty much screwed up the host interface.
                                         */
                                        rc = vboxNetFltSolarisRelink(pVNodeUDP, &Interface, IpMuxFd, ArpMuxFd);
                                        if (VBOX_SUCCESS(rc))
                                        {
                                            bool fRawModeOk = !fAttach;   /* Raw mode check is always ok during the detach case */
                                            if (fAttach)
                                            {
                                                /*
                                                 * Check if our raw mode request was successful (only in the IP stream).
                                                 */
                                                vboxnetflt_stream_t *pStream = pThis->u.s.pvStream;
                                                if (RT_LIKELY(pStream))
                                                {
                                                    if (   pStream->fRawMode == true
                                                        && pStream->ModeReqId == VBOXNETFLT_MODE_REQ_MAGIC)
                                                    {
                                                        pStream->ModeReqId = 0;
                                                        fRawModeOk = true;
                                                    }
                                                }
                                            }

                                            if (fRawModeOk)
                                            {
                                                /*
                                                 * Close the devices ONLY during the return from function case; otherwise
                                                 * we end up close twice which is an instant kernel panic.
                                                 */
                                                vboxNetFltSolarisCloseDev(pVNodeUDPHeld, pUserUDP);
                                                ldi_close(ARPDevHandle, FREAD | FWRITE, kcred);
                                                ldi_close(IPDevHandle, FREAD | FWRITE, kcred);

                                                LogFlow((DEVICE_NAME ":vboxNetFltSolarisModSetup: Success! %s %s@(Ip:%d Arp:%d) "
                                                        "%s interface %s\n", fAttach ? "Injected" : "Ejected", StrMod.mod_name,
                                                        StrMod.pos, ArpStrMod.pos, fAttach ? "to" : "from", pThis->szName));
                                                return VINF_SUCCESS;
                                            }
                                            else
                                                LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: Raw mode request failed.\n"));
                                        }
                                        else
                                        {
                                            LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: Relinking failed. Mode=%s rc=%d.\n",
                                                    fAttach ? "inject" : "eject", rc));
                                        }

                                        /*
                                         * Try failing gracefully during attach.
                                         */
                                        if (fAttach)
                                            strioctl(pVNodeIp, _I_REMOVE, (intptr_t)&StrMod, 0, K_TO_K, kcred, &ret);
                                    }
                                    else
                                    {
                                        LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: failed to %s the ARP stack. rc=%d\n",
                                                fAttach ? "inject into" : "eject from", rc));
                                    }

                                    if (fAttach)
                                        strioctl(pVNodeIp, _I_REMOVE, (intptr_t)&StrMod, 0, K_TO_K, kcred, &ret);

                                    vboxNetFltSolarisRelink(pVNodeUDP, &Interface, IpMuxFd, ArpMuxFd);
                                }
                                else
                                {
                                    LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: failed to %s the IP stack. rc=%d\n",
                                            fAttach ? "inject into" : "eject from", rc));
                                }
                            }
                            else
                                LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: failed to find position. rc=%d rc2=%d\n", rc, rc2));
                        }
                        else
                            LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: failed to get vnode from MuxFd.\n"));
                    }
                    else
                        LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: failed to unlink upper stream rc=%d rc2=%d.\n", rc, rc2));
                }
                else
                    LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: failed to get MuxFd from MuxId. rc=%d rc2=%d\n"));
            }
            else
                LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: failed to get Mux Ids.\n"));
            vboxNetFltSolarisCloseDev(pVNodeUDPHeld, pUserUDP);
        }
        else
            LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: failed to open UDP.\n"));
    }
    else
        LogRel((DEVICE_NAME ":vboxNetFltSolarisModSetup: invalid interface '%s'.\n", pThis->szName));

    ldi_close(ARPDevHandle, FREAD | FWRITE, kcred);
    ldi_close(IPDevHandle, FREAD | FWRITE, kcred);

    return VERR_INTNET_FLT_IF_FAILED;
}


/**
 * Wrapper for attaching ourselves to the interface.
 *
 * @returns VBox status code.
 * @param   pThis           The instance.
 * @remarks Owns the globals mutex, so re-requesting it anytime during this phase
 *          would panic the system e.g. in vboxNetFltSolarisFindInstance).
 */
static int vboxNetFltSolarisAttachToInterface(PVBOXNETFLTINS pThis)
{
    int rc = RTSemFastMutexRequest(g_VBoxNetFltSolarisMtx);
    AssertRC(rc);

    rc = vboxNetFltSolarisModSetup(pThis, true);

    RTSemFastMutexRelease(g_VBoxNetFltSolarisMtx);
    return rc;
}


/**
 * Wrapper for detaching ourselves from the interface.
 *
 * @returns VBox status code.
 * @param   pThis           The instance.
 * @remarks Owns the globals mutex, so re-requesting it anytime during this phase
 *          would panic the system (e.g. in vboxNetFltSolarisFindInstance).
 */
static int vboxNetFltSolarisDetachFromInterface(PVBOXNETFLTINS pThis)
{
    int rc = RTSemFastMutexRequest(g_VBoxNetFltSolarisMtx);
    AssertRC(rc);

    rc = vboxNetFltSolarisModSetup(pThis, false);

    RTSemFastMutexRelease(g_VBoxNetFltSolarisMtx);
    return rc;
}


/**
 * Create a solaris message block from the SG list.
 *
 * @returns Solaris message block.
 * @param   pThis           The instance.
 * @param   pSG             Pointer to the scatter-gather list.
 */
static mblk_t *vboxNetFltSolarisMBlkFromSG(PVBOXNETFLTINS pThis, PINTNETSG pSG, uint32_t fDst)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisMBlkFromSG pThis=%p pSG=%p\n"));

    mblk_t *pMsg = allocb(pSG->cbTotal, BPRI_MED);
    if (RT_UNLIKELY(!pMsg))
    {
        LogRel((DEVICE_NAME ":vboxNetFltSolarisMBlkFromSG failed to alloc %d bytes for mblk_t.\n", pSG->cbTotal));
        return NULL;
    }

    /*
     * Single buffer copy. Maybe later explore the
     * need/possibility for using a mblk_t chain rather.
     */
    for (unsigned i = 0; i < pSG->cSegsUsed; i++)
    {
        if (pSG->aSegs[i].pv)
        {
            bcopy(pSG->aSegs[i].pv, pMsg->b_wptr, pSG->aSegs[i].cb);
            pMsg->b_wptr += pSG->aSegs[i].cb;
        }
    }
    DB_TYPE(pMsg) = M_DATA;
    return pMsg;
}


/**
 * Calculate the number of segments required for this message block.
 *
 * @returns Number of segments.
 * @param   pThis   The instance
 * @param   pMsg    Pointer to the data message.
 */
static unsigned vboxNetFltSolarisMBlkCalcSGSegs(PVBOXNETFLTINS pThis, mblk_t *pMsg)
{
    unsigned cSegs = 0;
    for (mblk_t *pCur = pMsg; pCur; pCur = pCur->b_cont)
        if (MBLKL(pCur))
            cSegs++;

#ifdef PADD_RUNT_FRAMES_FROM_HOST
    if (msgdsize(pMsg) < 60)
        cSegs++;
#endif

    NOREF(pThis);
    return RT_MAX(cSegs, 1);
}


/**
 * Initializes an SG list from the given message block.
 *
 * @returns VBox status code.
 * @param   pThis   The instance.
 * @param   pMsg    Pointer to the data message.
                    The caller must ensure it's not a control message block.
 * @param   pSG     Pointer to the SG.
 * @param   cSegs   Number of segments in the SG.
 *                  This should match the number in the message block exactly!
 * @param   fSrc    The source of the message.
 */
static int vboxNetFltSolarisMBlkToSG(PVBOXNETFLTINS pThis, mblk_t *pMsg, PINTNETSG pSG, unsigned cSegs, uint32_t fSrc)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisMBlkToSG pThis=%p pMsg=%p pSG=%p cSegs=%d\n", pThis, pMsg, pSG, cSegs));

    pSG->pvOwnerData = NULL;
    pSG->pvUserData = NULL;
    pSG->pvUserData2 = NULL;
    pSG->cUsers = 1;
    pSG->cbTotal = 0;
    pSG->fFlags = INTNETSG_FLAGS_TEMP;
    pSG->cSegsAlloc = cSegs;

    /*
     * Convert the message block to segments.
     */
    mblk_t *pCur = pMsg;
    unsigned iSeg = 0;
    while (pCur)
    {
        size_t cbSeg = MBLKL(pCur);
        if (cbSeg)
        {
            void *pvSeg = pCur->b_rptr;
            pSG->aSegs[iSeg].pv = pvSeg;
            pSG->aSegs[iSeg].cb = cbSeg;
            pSG->aSegs[iSeg].Phys = NIL_RTHCPHYS;
            pSG->cbTotal += cbSeg;
            iSeg++;
        }
        pCur = pCur->b_cont;
    }
    pSG->cSegsUsed = iSeg;

#ifdef PADD_RUNT_FRAMES_FROM_HOST
    if (pSG->cbTotal < 60 && (fSrc & INTNETTRUNKDIR_HOST))
    {
        LogFlow((DEVICE_NAME ":vboxNetFltSolarisMBlkToSG pulling up to length.\n"));

        static uint8_t const s_abZero[128] = {0};
        pSG->aSegs[iSeg].Phys = NIL_RTHCPHYS;
        pSG->aSegs[iSeg].pv = (void *)&s_abZero[0];
        pSG->aSegs[iSeg].cb = 60 - pSG->cbTotal;
        pSG->cbTotal = 60;
        pSG->cSegsUsed++;
    }
#endif

    LogFlow((DEVICE_NAME ":vboxNetFltSolarisMBlkToSG iSeg=%d pSG->cbTotal=%d msgdsize=%d\n", iSeg, pSG->cbTotal, msgdsize(pMsg)));
    return VINF_SUCCESS;
}


/**
 * Converts raw mode M_DATA messages to M_PROTO DL_UNITDATA_IND format.
 *
 * @returns VBox status code.
 * @param   pMsg        Pointer to the raw message.
 * @param   pDlpiMsg    Where to store the M_PROTO message.
 *
 * @remarks The original raw message would be no longer valid and will be
 *          linked as part of the new DLPI message. Callers must take care
 *          not to use the raw message if this routine is successful.
 */
static int vboxNetFltSolarisRawToUnitData(mblk_t *pMsg, mblk_t **ppDlpiMsg)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisRawToUnitData pMsg=%p\n", pMsg));

    if (DB_TYPE(pMsg) != M_DATA)
        return VERR_NO_MEMORY;

    size_t cbMsg = sizeof(dl_unitdata_ind_t) + 2 * sizeof(vboxnetflt_dladdr_t);
    mblk_t *pDlpiMsg = allocb(cbMsg, BPRI_MED);
    if (RT_UNLIKELY(!pDlpiMsg))
        return VERR_NO_MEMORY;

    DB_TYPE(pDlpiMsg) = M_PROTO;
    dl_unitdata_ind_t *pDlpiData = (dl_unitdata_ind_t *)pDlpiMsg->b_rptr;
    pDlpiData->dl_primitive = DL_UNITDATA_IND;
    pDlpiData->dl_dest_addr_length = VBOXNETFLT_DLADDRL;
    pDlpiData->dl_dest_addr_offset = sizeof(dl_unitdata_ind_t);
    pDlpiData->dl_src_addr_length = VBOXNETFLT_DLADDRL;
    pDlpiData->dl_src_addr_offset = VBOXNETFLT_DLADDRL + sizeof(dl_unitdata_ind_t);

    PRTNETETHERHDR pEthHdr = (PRTNETETHERHDR)pMsg->b_rptr;

    vboxnetflt_dladdr_t *pDlAddr = (vboxnetflt_dladdr_t *)(pDlpiMsg->b_rptr + pDlpiData->dl_dest_addr_offset);
    pDlAddr->SAP = RT_BE2H_U16(pEthHdr->EtherType);
    bcopy(&pEthHdr->DstMac, &pDlAddr->Mac, sizeof(RTMAC));

    pDlAddr = (vboxnetflt_dladdr_t *)(pDlpiMsg->b_rptr + pDlpiData->dl_src_addr_offset);
    pDlAddr->SAP = RT_BE2H_U16(pEthHdr->EtherType);
    bcopy(&pEthHdr->SrcMac, &pDlAddr->Mac, sizeof(RTMAC));

    pDlpiMsg->b_wptr = pDlpiMsg->b_rptr + cbMsg;

    /* Make the message point to the protocol header */
    pMsg->b_rptr += sizeof(RTNETETHERHDR);

    pDlpiMsg->b_cont = pMsg;
    *ppDlpiMsg = pDlpiMsg;
    return VINF_SUCCESS;
}


/**
 * Converts DLPI M_PROTO messages to the raw mode M_DATA format.
 *
 * @returns VBox status code.
 * @param   pMsg        Pointer to the M_PROTO message.
 * @param   ppRawMsg    Where to store the converted message.
 *
 * @remarks If successful, the original pMsg is no longer valid, it will be deleted.
 *          Callers must take care not to continue to use pMsg after a successful
 *          call to this conversion routine.
 */
static int vboxNetFltSolarisUnitDataToRaw(PVBOXNETFLTINS pThis, mblk_t *pMsg, mblk_t **ppRawMsg)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisUnitDataToRaw pMsg=%p\n", pMsg));

    if (   !pMsg->b_cont
        || DB_TYPE(pMsg) != M_PROTO)
    {
        LogRel((DEVICE_NAME ":vboxNetFltSolarisUnitDataToRaw invalid input message.\n"));
        return VERR_NET_PROTOCOL_ERROR;
    }

    /*
     * Upstream consumers send/receive packets in the fast path mode.
     * We of course need to convert them into raw ethernet frames.
     */
    RTNETETHERHDR EthHdr;
    union DL_primitives *pPrim = (union DL_primitives *)pMsg->b_rptr;
    switch (pPrim->dl_primitive)
    {
        case DL_UNITDATA_IND:
        {
            /*
             * Receive side.
             */
            dl_unitdata_ind_t *pDlpiMsg = (dl_unitdata_ind_t *)pMsg->b_rptr;
            bcopy(pMsg->b_rptr + pDlpiMsg->dl_dest_addr_offset, &EthHdr.DstMac, sizeof(EthHdr.DstMac));
            bcopy(pMsg->b_rptr + pDlpiMsg->dl_src_addr_offset, &EthHdr.SrcMac, sizeof(EthHdr.SrcMac));

            vboxnetflt_dladdr_t *pDLSapAddr = (vboxnetflt_dladdr_t *)(pMsg->b_rptr + pDlpiMsg->dl_dest_addr_offset);
            EthHdr.EtherType = RT_H2BE_U16(pDLSapAddr->SAP);

            break;
        }

        case DL_UNITDATA_REQ:
        {
            /*
             * Send side.
             */
            dl_unitdata_req_t *pDlpiMsg = (dl_unitdata_req_t *)pMsg->b_rptr;

            bcopy(pMsg->b_rptr + pDlpiMsg->dl_dest_addr_offset, &EthHdr.DstMac, sizeof(EthHdr.DstMac));
            bcopy(&pThis->u.s.Mac, &EthHdr.SrcMac, sizeof(EthHdr.SrcMac));

            vboxnetflt_dladdr_t *pDLSapAddr = (vboxnetflt_dladdr_t *)(pMsg->b_rptr + pDlpiMsg->dl_dest_addr_offset);
            EthHdr.EtherType = RT_H2BE_U16(pDLSapAddr->SAP);

            break;
        }

        default:
        {
            LogRel((DEVICE_NAME ":vboxNetFltSolarisUnitDataToRaw Unknown M_PROTO. This shouldn't be happening!!"));
            return VERR_NET_PROTOCOL_ERROR;
        }
    }

    /*
     * Let us just link it as a mblk_t chain rather than re-copy the entire message.
     * The vboxNetFltSolarisMBlkToSG function will handle chained mblk_t's.
     */
    size_t cbLen = sizeof(EthHdr);
    mblk_t *pEtherMsg = allocb(cbLen, BPRI_MED);
    if (RT_UNLIKELY(!pEtherMsg))
        return VERR_NO_MEMORY;

    DB_TYPE(pEtherMsg) = M_DATA;
    bcopy(&EthHdr, pEtherMsg->b_wptr, sizeof(EthHdr));
    pEtherMsg->b_wptr += cbLen;

    pEtherMsg->b_cont = pMsg->b_cont;

    /*
     * Change the chained blocks to type M_DATA.
     */
    for (mblk_t *pTmp = pEtherMsg->b_cont; pTmp; pTmp = pTmp->b_cont)
        DB_TYPE(pTmp) = M_DATA;

    pMsg->b_cont = NULL;
    freemsg(pMsg);

    *ppRawMsg = pEtherMsg;
    return VINF_SUCCESS;
}


/**
 * Worker for routing messages from the wire or from the host.
 *
 * @returns VBox status code.
 * @param   pThis       The instance.
 * @param   pStream     Pointer to the stream.
 * @param   pQueue      Pointer to the queue.
 * @param   pOrigMsg    Pointer to the message.
 */
static int vboxNetFltSolarisRecv(PVBOXNETFLTINS pThis, vboxnetflt_stream_t *pStream, queue_t *pQueue, mblk_t *pOrigMsg)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisRecv pThis=%p pOrigMsg=%p\n", pThis, pOrigMsg));

    AssertCompile(sizeof(struct ether_header) == sizeof(RTNETETHERHDR));
    Assert(pStream->Type == kIpStream);

    /*
     * Make a copy as we will alter pMsg.
     */
    mblk_t *pMsg = copymsg(pOrigMsg);

    /*
     * Sort out M_PROTO and M_DATA.
     */
    bool fOriginalIsRaw = true;
    if (DB_TYPE(pMsg) == M_PROTO)
    {
        fOriginalIsRaw = false;
        mblk_t *pRawMsg;
        int rc = vboxNetFltSolarisUnitDataToRaw(pThis, pMsg, &pRawMsg);
        if (VBOX_SUCCESS(rc))
            pMsg = pRawMsg;
        else
        {
            freemsg(pMsg);
            LogRel((DEVICE_NAME ":vboxNetFltSolarisRecv invalid message!\n"));
            return VERR_GENERAL_FAILURE;
        }
    }

    /*
     * Figure out the source of the packet based on the source Mac address.
     */
    /** @todo Is there a more fool-proof way to determine if the packet was indeed sent from the host?? */
    uint32_t fSrc = INTNETTRUNKDIR_WIRE;
    PRTNETETHERHDR pEthHdr = (PRTNETETHERHDR)pMsg->b_rptr;
    if (vboxNetFltPortOsIsHostMac(pThis, &pEthHdr->SrcMac))
        fSrc = INTNETTRUNKDIR_HOST;

    bool fChecksumAdjusted = false;
#if 1
    if (fSrc & INTNETTRUNKDIR_HOST)
    {
        mblk_t *pCorrectedMsg = vboxNetFltSolarisFixChecksums(pMsg);
        if (pCorrectedMsg)
        {
            fChecksumAdjusted = true;
            pMsg = pCorrectedMsg;
        }
    }
#endif

    /*
     * Route all received packets into the internal network.
     */
    unsigned cSegs = vboxNetFltSolarisMBlkCalcSGSegs(pThis, pMsg);
    PINTNETSG pSG = (PINTNETSG)alloca(RT_OFFSETOF(INTNETSG, aSegs[cSegs]));
    int rc = vboxNetFltSolarisMBlkToSG(pThis, pMsg, pSG, cSegs, fSrc);
    if (VBOX_FAILURE(rc))
    {
        LogRel((DEVICE_NAME ":vboxNetFltSolarisMBlkToSG failed. rc=%d\n", rc));
        return rc;
    }

    bool fDropIt = pThis->pSwitchPort->pfnRecv(pThis->pSwitchPort, pSG, fSrc);

    /*
     * Drop messages consumed by the internal network.
     */
    if (fDropIt)
    {
        LogFlow((DEVICE_NAME ":vboxNetFltSolarisRecv Packet consumed by internal network.\n"));
        freemsg(pOrigMsg);
        freemsg(pMsg);
    }
    else
    {
        /*
         * Packets from the host, push them up in raw mode.
         * Packets from the wire, we must push them in their original form
         * as upstream consumers expect this format.
         */
        if (pStream->fRawMode)
        {
            if (fSrc & INTNETTRUNKDIR_HOST)
            {
                /* Raw packets with correct checksums, pass-through the original */
                if (    fOriginalIsRaw
                    && !fChecksumAdjusted)
                {
                    putnext(pQueue, pOrigMsg);
                }
                else    /* For M_PROTO packets or checksum corrected raw packets, pass-through the raw */
                {
                    putnext(pQueue, pMsg);
                    pMsg = pOrigMsg;        /* for the freemsg that follows */
                }
            }
            else    /* INTNETTRUNKDIR_WIRE */
            {
                if (fOriginalIsRaw)
                    pOrigMsg->b_rptr += sizeof(RTNETETHERHDR);

                putnext(pQueue, pOrigMsg);
            }
        }
        else
            putnext(pQueue, pOrigMsg);

        freemsg(pMsg);
    }

    return VINF_SUCCESS;
}


/**
 * Find the PVBOXNETFLTINS associated with a stream.
 *
 * @returns PVBOXNETFLTINS instance, or NULL if there's none.
 * @param   pStream     Pointer to the stream to search for.
 */
static PVBOXNETFLTINS vboxNetFltSolarisFindInstance(vboxnetflt_stream_t *pStream)
{
    vboxnetflt_stream_t *pCur = g_VBoxNetFltSolarisState.pOpenedStreams;
    for (; pCur; pCur = pCur->pNext)
        if (pCur == pStream)
            return pCur->pThis;

    return NULL;
}


/**
 * Finalize the message to be fed into the internal network.
 * Verifies and tries to fix checksums for TCP, UDP and IP.
 *
 * @returns Corrected message or NULL if no change was required.
 * @param   pMsg    Pointer to the message block.
 *                  This must not be DLPI linked messages, must be M_DATA.
 *
 * @remarks If this function returns a checksum adjusted message, the
 *          passed in input message has been freed and should not be
 *          referenced anymore by the caller.
 */
static mblk_t *vboxNetFltSolarisFixChecksums(mblk_t *pMsg)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisFixChecksums pMsg=%p\n"));

    Assert(DB_TYPE(pMsg) == M_DATA);

    if (MBLKL(pMsg) < sizeof(RTNETETHERHDR))
    {
        LogRel((DEVICE_NAME ":vboxNetFltSolarisFixChecksums Packet shorter than ethernet header size!\n"));
        return NULL;
    }

    PRTNETETHERHDR pEthHdr = (PRTNETETHERHDR)pMsg->b_rptr;
    if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPV4))
    {
        /*
         * Check if we have a complete packet or being fed a chain.
         */
        size_t cbIpPacket = 0;
        mblk_t *pFullMsg = NULL;
        if (pMsg->b_cont)
        {
            LogFlow((DEVICE_NAME ":Chained mblk_t.\n"));

            /*
             * Handle chain by making a packet copy to verify if the IP checksum is correct.
             * Contributions to calculating IP checksums from a chained message block with
             * odd/non-pulled up sizes are welcome.
             */
            size_t cbFullMsg = msgdsize(pMsg);
            mblk_t *pFullMsg = allocb(cbFullMsg, BPRI_MED);
            LogFlow((DEVICE_NAME ":msgdsize returns %d\n", cbFullMsg));
            if (RT_UNLIKELY(!pFullMsg))
            {
                LogRel((DEVICE_NAME ":vboxNetFltSolarisFixChecksums failed to alloc new message of %d bytes.\n", cbFullMsg));
                return false;
            }

            for (mblk_t *pTmp = pMsg; pTmp; pTmp = pTmp->b_cont)
            {
                if (DB_TYPE(pTmp) == M_DATA)
                {
                    bcopy(pTmp->b_rptr, pFullMsg->b_wptr, MBLKL(pTmp));
                    pFullMsg->b_wptr += MBLKL(pTmp);
                }
                else
                    LogFlow((DEVICE_NAME ":Not M_DATA.. this is really bad.\n"));
            }

            DB_TYPE(pFullMsg) = M_DATA;
            pEthHdr = (PRTNETETHERHDR)pFullMsg->b_rptr;
            cbIpPacket = MBLKL(pFullMsg) - sizeof(RTNETETHERHDR);
        }
        else
            cbIpPacket = MBLKL(pMsg) - sizeof(RTNETETHERHDR);

        /*
         * Check if the IP checksum is valid.
         */
        PRTNETIPV4 pIpHdr = (PRTNETIPV4)(pEthHdr + 1);
        if (!RTNetIPv4IsHdrValid(pIpHdr, cbIpPacket, cbIpPacket))
        {
            LogFlow((DEVICE_NAME ":Invalid IP checksum detected. Trying to fix...\n"));

            /*
             * Fix up TCP/UDP and IP checksums if they're incomplete/invalid.
             */
            if (pIpHdr->ip_p == RTNETIPV4_PROT_TCP)
            {
                size_t cbTcpPacket = cbIpPacket - (pIpHdr->ip_hl << 2);

                PRTNETTCP pTcpHdr = (PRTNETTCP)(pIpHdr + 1);
                if (!RTNetIPv4IsTCPValid(pIpHdr, pTcpHdr, cbTcpPacket, NULL, cbTcpPacket))
                {
                    RTNetIPv4TCPChecksum(pIpHdr, pTcpHdr, NULL);
                    LogFlow((DEVICE_NAME ":fixed TCP checkum.\n"));
                }
                else
                    LogFlow((DEVICE_NAME ":valid TCP checksum invalid IP checksum...\n"));
            }
            else if (pIpHdr->ip_p == RTNETIPV4_PROT_UDP)
            {
                size_t cbUdpPacket = cbIpPacket - (pIpHdr->ip_hl << 2);

                PRTNETUDP pUdpHdr = (PRTNETUDP)(pIpHdr + 1);
                RTNetIPv4UDPChecksum(pIpHdr, pUdpHdr, pUdpHdr + 1);

                LogFlow((DEVICE_NAME ":fixed UDP checksum.\n"));
            }
            else
            {
                LogFlow((DEVICE_NAME ":Non TCP/UDP protocol with invalid IP header!\n"));
            }

            RTNetIPv4HdrChecksum(pIpHdr);
            LogFlow((DEVICE_NAME ":fixed IP checkum.\n"));

            /*
             * If we made a copy and the checksum is corrected on the copy,
             * free the original, return the checksum fixed copy.
             */
            if (pFullMsg)
            {
                freemsg(pMsg);
                return pFullMsg;
            }

            return pMsg;
        }

        /*
         * If we made a copy and the checksum is NOT corrected, free the copy,
         * and return NULL.
         */
        if (pFullMsg)
            freemsg(pFullMsg);
    }

    return NULL;
}


/**
 * Simple packet dump, used for internal debugging.
 *
 * @param   pMsg    Pointer to the message to analyze and dump.
 */
static void vboxNetFltSolarisAnalyzeMBlk(mblk_t *pMsg)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisAnalyzeMBlk pMsg=%p\n"));

    PCRTNETETHERHDR pEthHdr = (PCRTNETETHERHDR)pMsg->b_rptr;
    if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPV4))
    {
        PRTNETIPV4 pIpHdr = (PRTNETIPV4)(pEthHdr + 1);
        size_t cbLen = MBLKL(pMsg) - sizeof(*pEthHdr);
        if (RTNetIPv4IsHdrValid(pIpHdr, cbLen, cbLen))
        {
            uint8_t *pb = pMsg->b_rptr;
            if (pIpHdr->ip_p == RTNETIPV4_PROT_ICMP)
                LogFlow((DEVICE_NAME ":ICMP D=%.6Rhxs  S=%.6Rhxs  T=%04x\n", pb, pb + 6, RT_BE2H_U16(*(uint16_t *)(pb + 12))));
            else if (pIpHdr->ip_p == RTNETIPV4_PROT_TCP)
                LogFlow((DEVICE_NAME ":TCP D=%.6Rhxs  S=%.6Rhxs\n", pb, pb + 6));
            else if (pIpHdr->ip_p == RTNETIPV4_PROT_UDP)
                LogFlow((DEVICE_NAME ":UDP D=%.6Rhxs  S=%.6Rhxs\n", pb, pb + 6));
        }
        else
        {
            LogFlow((DEVICE_NAME ":Invalid IP header.\n"));
            return;
        }
    }
    else if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_ARP))
    {
        PRTNETARPHDR pArpHdr = (PRTNETARPHDR)(pEthHdr + 1);
        LogFlow((DEVICE_NAME ":ARP Op=%d\n", pArpHdr->ar_oper));
    }
    else
    {
        LogFlow((DEVICE_NAME ":Unknown EtherType=%x D=%.6Rhxs S=%.6Rhxs\n", RT_H2BE_U16(pEthHdr->EtherType), &pEthHdr->DstMac,
                    &pEthHdr->SrcMac));
        /* LogFlow((DEVICE_NAME ":%.*Vhxd\n", MBLKL(pMsg), pMsg->b_rptr)); */
    }
}


/* -=-=-=-=-=- Common Hooks -=-=-=-=-=- */
bool vboxNetFltPortOsIsPromiscuous(PVBOXNETFLTINS pThis)
{
    vboxnetflt_stream_t *pStream = pThis->u.s.pvStream;
    return pStream->fPromisc;
}


void vboxNetFltPortOsGetMacAddress(PVBOXNETFLTINS pThis, PRTMAC pMac)
{
    LogFlow((DEVICE_NAME ":vboxNetFltPortOsGetMacAddress pThis=%p\n", pThis));

    RTMAC EmptyMac;
    bzero(&EmptyMac, sizeof(EmptyMac));
    if (!bcmp(&pThis->u.s.Mac, &EmptyMac, sizeof(EmptyMac)))    /* This should never happen... */
    {
        /* Bummer, Mac address is not copied yet. */
        LogRel((DEVICE_NAME ":vboxNetFltPortOsGetMacAddress: Mac address not cached yet!\n"));
        return;
    }

    *pMac = pThis->u.s.Mac;
}


bool vboxNetFltPortOsIsHostMac(PVBOXNETFLTINS pThis, PCRTMAC pMac)
{
    /*
     * MAC address change acknowledgements are intercepted on the read side
     * hence theoritically we are always update to date with any changes.
     */
    return pThis->u.s.Mac.au16[0] == pMac->au16[0]
        && pThis->u.s.Mac.au16[1] == pMac->au16[1]
        && pThis->u.s.Mac.au16[2] == pMac->au16[2];
}


void vboxNetFltPortOsSetActive(PVBOXNETFLTINS pThis, bool fActive)
{
    LogFlow((DEVICE_NAME ":vboxNetFltPortOsSetActive pThis=%p fActive=%d\n", pThis, fActive));

    /*
     * Enable/disable promiscuous mode.
     */
    vboxnetflt_stream_t *pStream = pThis->u.s.pvStream;
    Assert(pStream->Type == kIpStream);
    int rc = vboxNetFltSolarisPromiscReq(pStream->pReadQueue, fActive);
    if (VBOX_FAILURE(rc))
        LogRel((DEVICE_NAME ":vboxNetFltPortOsSetActive failed to request promiscuous mode! rc=%d\n", rc));
}


int vboxNetFltOsDisconnectIt(PVBOXNETFLTINS pThis)
{
    /* Nothing to do here. */
    return VINF_SUCCESS;
}


int  vboxNetFltOsConnectIt(PVBOXNETFLTINS pThis)
{
    /* Nothing to do here. */
    return VINF_SUCCESS;
}


void vboxNetFltOsDeleteInstance(PVBOXNETFLTINS pThis)
{
    LogFlow((DEVICE_NAME ":vboxNetFltOsDeleteInstance pThis=%p\n"));
    vboxNetFltSolarisDetachFromInterface(pThis);
}


int vboxNetFltOsInitInstance(PVBOXNETFLTINS pThis)
{
    LogFlow((DEVICE_NAME ":vboxNetFltOsInitInstance pThis=%p\n"));
    return vboxNetFltSolarisAttachToInterface(pThis);
}


int vboxNetFltOsPreInitInstance(PVBOXNETFLTINS pThis)
{
    /*
     * Init. the solaris specific data.
     */
    pThis->u.s.pvStream = NULL;
    pThis->u.s.pvArpStream = NULL;
    bzero(&pThis->u.s.Mac, sizeof(pThis->u.s.Mac));
    return VINF_SUCCESS;
}


bool vboxNetFltOsMaybeRediscovered(PVBOXNETFLTINS pThis)
{
    /*
     * We don't support interface rediscovery on Solaris hosts because the
     * filter is very tightly bound to the stream.
     */
    return false;
}


int vboxNetFltPortOsXmit(PVBOXNETFLTINS pThis, PINTNETSG pSG, uint32_t fDst)
{
    LogFlow((DEVICE_NAME ":vboxNetFltPortOsXmit pThis=%p pSG=%p fDst=%d\n", pThis, pSG, fDst));

    vboxnetflt_stream_t *pStream = pThis->u.s.pvStream;
    queue_t *pReadQueue = pStream->pReadQueue;
    queue_t *pWriteQueue = WR(pReadQueue);

    /*
     * Create a message block and send it down the wire (downstream).
     */
    if (fDst & INTNETTRUNKDIR_WIRE)
    {
        mblk_t *pMsg = vboxNetFltSolarisMBlkFromSG(pThis, pSG, fDst);
        if (RT_UNLIKELY(!pMsg))
        {
            LogRel((DEVICE_NAME ":vboxNetFltPortOsXmit vboxNetFltSolarisMBlkFromSG failed.\n"));
            return VERR_NO_MEMORY;
        }

        LogFlow((DEVICE_NAME ":vboxNetFltPortOsXmit INTNETTRUNKDIR_WIRE\n", pMsg));
        putnext(pWriteQueue, pMsg);
    }

    /*
     * Create a message block and send it up the host stack (upstream).
     */
    if (fDst & INTNETTRUNKDIR_HOST)
    {
        mblk_t *pMsg = vboxNetFltSolarisMBlkFromSG(pThis, pSG, fDst);
        if (RT_UNLIKELY(!pMsg))
        {
            LogRel((DEVICE_NAME ":vboxNetFltPortOsXmit vboxNetFltSolarisMBlkFromSG failed.\n"));
            return VERR_NO_MEMORY;
        }

        PCRTNETETHERHDR pEthHdr = (PCRTNETETHERHDR)pMsg->b_rptr;
        bool fArp = (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_ARP));

        /*
         * Construct a DL_UNITDATA_IND style message.
         */
        mblk_t *pDlpiMsg;
        int rc = vboxNetFltSolarisRawToUnitData(pMsg, &pDlpiMsg);
        if (VBOX_FAILURE(rc))
        {
            LogRel((DEVICE_NAME ":vboxNetFltSolarisRawToUnitData failed! rc=%d\n", rc));
            freemsg(pMsg);
            return VERR_NO_MEMORY;
        }
        pMsg = pDlpiMsg;

        if (fArp)
        {
            LogFlow((DEVICE_NAME ":vboxNetFltPortOsXmit INTNETTRUNKDIR_HOST ARP\n"));

            /*
             * Send message up ARP stream.
             */
            vboxnetflt_stream_t *pArpStream = pThis->u.s.pvArpStream;
            queue_t *pArpReadQueue = pArpStream->pReadQueue;
            putnext(pArpReadQueue, pMsg);
        }
        else
        {
            LogFlow((DEVICE_NAME ":vboxNetFltPortOsXmit INTNETTRUNKDIR_HOST\n"));

            /*
             * Send messages up IP stream.
             */
            putnext(pReadQueue, pMsg);
        }
    }

    return VINF_SUCCESS;
}

