/* $Id$ */
/** @file
 * VBoxNetAdapter - Network Adapter Driver (Host), Solaris Specific Code.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#if defined(DEBUG_ramshankar) && !defined(LOG_ENABLED)
# define LOG_ENABLED
#endif

#define LOG_GROUP LOG_GROUP_NET_ADP_DRV
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/version.h>
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/initterm.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/rand.h>

#include <sys/types.h>
#include <sys/dlpi.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strsun.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunldi.h>
#include <sys/gld.h>

#include "../VBoxNetAdpInternal.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define VBOXSOLQUOTE2(x)         #x
#define VBOXSOLQUOTE(x)          VBOXSOLQUOTE2(x)
#define DEVICE_NAME              "vboxnet"
/** The module descriptions as seen in 'modinfo'. */
#define DEVICE_DESC_DRV          "VirtualBox NetAdp"
#define VBOXNETADP_MTU           1500

static int VBoxNetAdpSolarisAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd);
static int VBoxNetAdpSolarisDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd);

/**
 * Streams: module info.
 */
static struct module_info g_VBoxNetAdpSolarisModInfo =
{
    0x0dd,                            /* module id */
    DEVICE_NAME,
    0,                                /* min. packet size */
    INFPSZ,                           /* max. packet size */
    0,                                /* hi-water mark */
    0                                 /* lo-water mark */
};

/**
 * Streams: read queue hooks.
 */
static struct qinit g_VBoxNetAdpSolarisReadQ =
{
    NULL,                             /* read */
    gld_rsrv,
    gld_open,
    gld_close,
    NULL,                             /* admin (reserved) */
    &g_VBoxNetAdpSolarisModInfo,
    NULL                              /* module stats */
};

/**
 * Streams: write queue hooks.
 */
static struct qinit g_VBoxNetAdpSolarisWriteQ =
{
    gld_wput,
    gld_wsrv,
    NULL,                             /* open */
    NULL,                             /* close */
    NULL,                             /* admin (reserved) */
    &g_VBoxNetAdpSolarisModInfo,
    NULL                              /* module stats */
};

/**
 * Streams: IO stream tab.
 */
static struct streamtab g_VBoxNetAdpSolarisStreamTab =
{
    &g_VBoxNetAdpSolarisReadQ,
    &g_VBoxNetAdpSolarisWriteQ,
    NULL,                           /* muxread init */
    NULL                            /* muxwrite init */
};

/**
 * cb_ops: driver char/block entry points
 */
static struct cb_ops g_VBoxNetAdpSolarisCbOps =
{
    nulldev,                        /* cb open */
    nulldev,                        /* cb close */
    nodev,                          /* b strategy */
    nodev,                          /* b dump */
    nodev,                          /* b print */
    nodev,                          /* cb read */
    nodev,                          /* cb write */
    nodev,                          /* cb ioctl */
    nodev,                          /* c devmap */
    nodev,                          /* c mmap */
    nodev,                          /* c segmap */
    nochpoll,                       /* c poll */
    ddi_prop_op,                    /* property ops */
    &g_VBoxNetAdpSolarisStreamTab,
    D_MP,                           /* compat. flag */
    CB_REV                          /* revision */
};

/**
 * dev_ops: driver entry/exit and other ops.
 */
static struct dev_ops g_VBoxNetAdpSolarisDevOps =
{
    DEVO_REV,                       /* driver build revision */
    0,                              /* ref count */
    gld_getinfo,
    nulldev,                        /* identify */
    nulldev,                        /* probe */
    VBoxNetAdpSolarisAttach,
    VBoxNetAdpSolarisDetach,
    nodev,                          /* reset */
    &g_VBoxNetAdpSolarisCbOps,
    (struct bus_ops *)0,
    nodev                           /* power */
};

/**
 * modldrv: export driver specifics to kernel
 */
static struct modldrv g_VBoxNetAdpSolarisDriver =
{
    &mod_driverops,                 /* extern from kernel */
    DEVICE_DESC_DRV " " VBOX_VERSION_STRING "r" VBOXSOLQUOTE(VBOX_SVN_REV),
    &g_VBoxNetAdpSolarisDevOps
};

/**
 * modlinkage: export install/remove/info to the kernel
 */
static struct modlinkage g_VBoxNetAdpSolarisModLinkage =
{
    MODREV_1,                       /* loadable module system revision */
    &g_VBoxNetAdpSolarisDriver,     /* adapter streams driver framework */
    NULL                            /* terminate array of linkage structures */
};


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The default ethernet broadcast address */
static uchar_t achBroadcastAddr[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/**
 * vboxnetadp_state_t: per-instance data
 */
typedef struct vboxnetadp_state_t
{
    dev_info_t   *pDip;           /* device info. */
    RTMAC         FactoryMac;     /* default 'factory' MAC address */
    RTMAC         CurrentMac;     /* current MAC address */
} vboxnetadp_state_t;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int vboxNetAdpSolarisGenerateMac(PRTMAC pMac);
static int vboxNetAdpSolarisSetMacAddress(gld_mac_info_t *pMacInfo, unsigned char *pszMacAddr);
static int vboxNetAdpSolarisSend(gld_mac_info_t *pMacInfo, mblk_t *pMsg);
static int vboxNetAdpSolarisStub(gld_mac_info_t *pMacInfo);
static int vboxNetAdpSolarisSetPromisc(gld_mac_info_t *pMacInfo, int fPromisc);
static int vboxNetAdpSolarisSetMulticast(gld_mac_info_t *pMacInfo, unsigned char *pMulticastAddr, int fMulticast);


/**
 * Kernel entry points
 */
int _init(void)
{
    LogFlow((DEVICE_NAME ":_init\n"));

    /*
     * Prevent module autounloading.
     */
    modctl_t *pModCtl = mod_getctl(&g_VBoxNetAdpSolarisModLinkage);
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
        rc = mod_install(&g_VBoxNetAdpSolarisModLinkage);
        if (!rc)
            return rc;

        LogRel((DEVICE_NAME ":mod_install failed. rc=%d\n", rc));
        RTR0Term();
    }
    else
        LogRel((DEVICE_NAME ":failed to initialize IPRT (rc=%d)\n", rc));

    return RTErrConvertToErrno(rc);
}


int _fini(void)
{
    int rc;
    LogFlow((DEVICE_NAME ":_fini\n"));

    /*
     * Undo the work done during start (in reverse order).
     */
    RTR0Term();

    return mod_remove(&g_VBoxNetAdpSolarisModLinkage);
}


int _info(struct modinfo *pModInfo)
{
    LogFlow((DEVICE_NAME ":_info\n"));

    int rc = mod_info(&g_VBoxNetAdpSolarisModLinkage, pModInfo);

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
static int VBoxNetAdpSolarisAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd)
{
    LogFlow((DEVICE_NAME ":VBoxNetAdpSolarisAttach pDip=%p enmCmd=%d\n", pDip, enmCmd));

    int rc = -1;
    switch (enmCmd)
    {
        case DDI_ATTACH:
        {
            gld_mac_info_t *pMacInfo = gld_mac_alloc(pDip);
            if (pMacInfo)
            {
                vboxnetadp_state_t *pState = RTMemAllocZ(sizeof(vboxnetadp_state_t));
                if (pState)
                {
                    pState->pDip = pDip;

                    /*
                     * Setup GLD MAC layer registeration info.
                     */
                    pMacInfo->gldm_reset = vboxNetAdpSolarisStub;
                    pMacInfo->gldm_start = vboxNetAdpSolarisStub;
                    pMacInfo->gldm_stop = vboxNetAdpSolarisStub;
                    pMacInfo->gldm_set_mac_addr = vboxNetAdpSolarisSetMacAddress;
                    pMacInfo->gldm_set_multicast = vboxNetAdpSolarisSetMulticast;
                    pMacInfo->gldm_set_promiscuous = vboxNetAdpSolarisSetPromisc;
                    pMacInfo->gldm_send = vboxNetAdpSolarisSend;
                    pMacInfo->gldm_intr = NULL;
                    pMacInfo->gldm_get_stats = NULL;
                    pMacInfo->gldm_ioctl = NULL;
                    pMacInfo->gldm_ident = DEVICE_NAME;
                    pMacInfo->gldm_type = DL_ETHER;
                    pMacInfo->gldm_minpkt = 0;
                    pMacInfo->gldm_maxpkt = VBOXNETADP_MTU;

                    AssertCompile(sizeof(RTMAC) == ETHERADDRL);

                    pMacInfo->gldm_addrlen = ETHERADDRL;
                    pMacInfo->gldm_saplen = -2;
                    pMacInfo->gldm_broadcast_addr = achBroadcastAddr;
                    pMacInfo->gldm_ppa = ddi_get_instance(pState->pDip);
                    pMacInfo->gldm_devinfo = pState->pDip;
                    pMacInfo->gldm_private = (caddr_t)pState;

                    /*
                     * We use a semi-random MAC addresses similar to a guest NIC's MAC address
                     * as the default factory address of the interface.
                     */
                    rc = vboxNetAdpSolarisGenerateMac(&pState->FactoryMac);
                    if (RT_SUCCESS(rc))
                    {
                        bcopy(&pState->FactoryMac, &pState->CurrentMac, sizeof(RTMAC));
                        pMacInfo->gldm_vendor_addr = (unsigned char *)&pState->FactoryMac;

                        /*
                         * Now try registering our GLD with the MAC layer.
                         * Registeration can fail on some S10 versions when the MTU size is more than 1500.
                         * When we implement jumbo frames we should probably retry with MTU 1500 for S10.
                         */
                        rc = gld_register(pDip, (char *)ddi_driver_name(pDip), pMacInfo);
                        if (rc == DDI_SUCCESS)
                        {
                            ddi_report_dev(pDip);
                            return DDI_SUCCESS;
                        }
                        else
                            LogRel((DEVICE_NAME ":VBoxNetAdpSolarisAttach failed to register GLD. rc=%d\n", rc));
                    }
                    else
                        LogRel((DEVICE_NAME ":VBoxNetAdpSolarisAttach failed to generate mac address.rc=%d\n"));

                    RTMemFree(pState);
                }
                else
                    LogRel((DEVICE_NAME ":VBoxNetAdpSolarisAttach failed to alloc state.\n"));

                gld_mac_free(pMacInfo);
            }
            else
                LogRel((DEVICE_NAME ":VBoxNetAdpSolarisAttach failed to alloc mac structure.\n"));
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
static int VBoxNetAdpSolarisDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd)
{
    LogFlow((DEVICE_NAME ":VBoxNetAdpSolarisDetach pDip=%p enmCmd=%d\n", pDip, enmCmd));

    switch (enmCmd)
    {
        case DDI_DETACH:
        {
            /*
             * Unregister and clean up.
             */
            gld_mac_info_t *pMacInfo = ddi_get_driver_private(pDip);
            if (pMacInfo)
            {
                vboxnetadp_state_t *pState = (vboxnetadp_state_t *)pMacInfo->gldm_private;
                if (pState)
                {
                    int rc = gld_unregister(pMacInfo);
                    if (rc == DDI_SUCCESS)
                    {
                        gld_mac_free(pMacInfo);
                        RTMemFree(pState);
                        return DDI_SUCCESS;
                    }
                    else
                        LogRel((DEVICE_NAME ":VBoxNetAdpSolarisDetach failed to unregister GLD from MAC layer.rc=%d\n", rc));
                }
                else
                    LogRel((DEVICE_NAME ":VBoxNetAdpSolarisDetach failed to get internal state.\n"));
            }
            else
                LogRel((DEVICE_NAME ":VBoxNetAdpSolarisDetach failed to get driver private GLD data.\n"));

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


static int vboxNetAdpSolarisGenerateMac(PRTMAC pMac)
{
    pMac->au8[0] = 0x00;
    pMac->au8[1] = 0x08;
    pMac->au8[2] = 0x27;
    RTRandBytes(&pMac->au8[3], 3);
    LogFlow((DEVICE_NAME ":VBoxNetAdpSolarisGenerateMac Generated %.*Rhxs\n", sizeof(RTMAC), &pMac));
    return VINF_SUCCESS;
}


static int vboxNetAdpSolarisSetMacAddress(gld_mac_info_t *pMacInfo, unsigned char *pszMacAddr)
{
    vboxnetadp_state_t *pState = (vboxnetadp_state_t *)pMacInfo->gldm_private;
    if (pState)
    {
        bcopy(pszMacAddr, &pState->CurrentMac, sizeof(RTMAC));
        LogFlow((DEVICE_NAME ":vboxNetAdpSolarisSetMacAddress updated MAC %.*Rhxs\n", sizeof(RTMAC), &pState->CurrentMac));
        return GLD_SUCCESS;
    }
    else
        LogRel((DEVICE_NAME ":vboxNetAdpSolarisSetMacAddress failed to get internal state.\n"));
    return GLD_FAILURE;
}


static int vboxNetAdpSolarisSend(gld_mac_info_t *pMacInfo, mblk_t *pMsg)
{
    return GLD_SUCCESS;
}


static int vboxNetAdpSolarisStub(gld_mac_info_t *pMacInfo)
{
    return GLD_SUCCESS;
}


static int vboxNetAdpSolarisSetMulticast(gld_mac_info_t *pMacInfo, unsigned char *pMulticastAddr, int fMulticast)
{
    return GLD_SUCCESS;
}


static int vboxNetAdpSolarisSetPromisc(gld_mac_info_t *pMacInfo, int fPromisc)
{
    /* Host requesting promiscuous intnet connection... */
    return GLD_SUCCESS;
}

