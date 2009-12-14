/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Solaris Specific Code.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
#define LOG_GROUP LOG_GROUP_NET_FLT_DRV
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/cdefs.h>
#include <VBox/version.h>
#include <iprt/initterm.h>
#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/net.h>
#include <iprt/spinlock.h>

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/gld.h>
#include <sys/sunddi.h>
#include <sys/strsubr.h>
#include <sys/dlpi.h>
#include <sys/dls_mgmt.h>
#include <sys/mac.h>
#include <sys/strsun.h>
#include <sys/sunddi.h>

#include "include/mac_provider.h"       /* dependency for other headers */
#include "include/mac_client.h"         /* for mac_* */
#include "include/mac_client_priv.h"    /* for mac_info, mac_capab_get etc. */
#if 0
#include "include/dls.h"                /* for dls_mgmt_* */
#include "include/dld_ioc.h"            /* required by vnic.h */
#include "include/vnic.h"               /* for vnic_ioc_diag_t */
#include "include/vnic_impl.h"          /* for vnic_dev_create */
#endif

#define VBOXNETFLT_OS_SPECFIC 1
#include "../VBoxNetFltInternal.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The module name. */
#define DEVICE_NAME                     "vboxflt"
/** The module descriptions as seen in 'modinfo'. */
#define DEVICE_DESC_DRV                 "VirtualBox NetBow"
/** The dynamically created VNIC name */
#define VBOXFLT_VNIC_NAME               "vboxvnic"
/** Debugging switch for using symbols in kmdb */
# define LOCAL      static

#if defined(DEBUG_ramshankar)
# undef Log
# define Log        LogRel
# undef LogFlow
# define LogFlow    LogRel
# undef LOCAL
# define LOCAL
#endif


/*******************************************************************************
*   Kernel Entry Hooks                                                         *
*******************************************************************************/
LOCAL int VBoxNetFltSolarisAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd);
LOCAL int VBoxNetFltSolarisDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd);
LOCAL int VBoxNetFltSolarisGetInfo(dev_info_t *pDip, ddi_info_cmd_t enmCmd, void *pArg, void **ppResult);


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * cb_ops: for drivers that support char/block entry points
 */
static struct cb_ops g_VBoxNetFltSolarisCbOps =
{
    nulldev,                    /* c open */
    nulldev,                    /* c close */
    nodev,                      /* b strategy */
    nodev,                      /* b dump */
    nodev,                      /* b print */
    nodev,                      /* c read */
    nodev,                      /* c write*/
    nodev,                      /* c ioctl*/
    nodev,                      /* c devmap */
    nodev,                      /* c mmap */
    nodev,                      /* c segmap */
    nochpoll,                   /* c poll */
    ddi_prop_op,                /* property ops */
    NULL,                       /* streamtab  */
    D_NEW | D_MP,               /* compat. flag */
    CB_REV,                     /* revision */
    nodev,                      /* c aread */
    nodev                       /* c awrite */
};

/**
 * dev_ops: for driver device operations
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
    NULL,                       /* bus ops */
    nodev,                      /* power */
    ddi_quiesce_not_needed
};

/**
 * modldrv: export driver specifics to the kernel
 */
static struct modldrv g_VBoxNetFltSolarisModule =
{
    &mod_driverops,             /* extern from kernel */
    DEVICE_DESC_DRV " " VBOX_VERSION_STRING "r" RT_XSTR(VBOX_SVN_REV),
    &g_VBoxNetFltSolarisDevOps
};

/**
 * modlinkage: export install/remove/info to the kernel
 */
static struct modlinkage g_VBoxNetFltSolarisModLinkage =
{
    MODREV_1,
    {
        &g_VBoxNetFltSolarisModule,
        NULL,
    }
};


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Global Device handle we only support one instance. */
static dev_info_t *g_pVBoxNetFltSolarisDip = NULL;
/** Global Mutex (actually an rw lock). */
static RTSEMFASTMUTEX g_VBoxNetFltSolarisMtx = NIL_RTSEMFASTMUTEX;
/** The (common) global data. */
static VBOXNETFLTGLOBALS g_VBoxNetFltSolarisGlobals;


/*******************************************************************************
*   Internal Function                                                          *
*******************************************************************************/
LOCAL mblk_t *vboxNetFltSolarisMBlkFromSG(PVBOXNETFLTINS pThis, PINTNETSG pSG, uint32_t fDst);
LOCAL unsigned vboxNetFltSolarisMBlkCalcSGSegs(PVBOXNETFLTINS pThis, mblk_t *pMsg);
LOCAL int vboxNetFltSolarisMBlkToSG(PVBOXNETFLTINS pThis, mblk_t *pMsg, PINTNETSG pSG, unsigned cSegs, uint32_t fSrc);

LOCAL int vboxNetFltSolarisAttachToInterface(PVBOXNETFLTINS pThis, bool fRediscovery);
LOCAL int vboxNetFltSolarisDetachFromInterface(PVBOXNETFLTINS pThis);
LOCAL void vboxNetFltSolarisRecv(void *pvData, mac_resource_handle_t hResource, mblk_t *pMsg, boolean_t fLoopback);


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
            rc = vboxNetFltInitGlobalsAndIdc(&g_VBoxNetFltSolarisGlobals);
            if (RT_SUCCESS(rc))
            {
                rc = mod_install(&g_VBoxNetFltSolarisModLinkage);
                if (!rc)
                    return rc;

                LogRel((DEVICE_NAME ":mod_install failed. rc=%d\n", rc));
                vboxNetFltTryDeleteIdcAndGlobals(&g_VBoxNetFltSolarisGlobals);
            }
            else
                LogRel((DEVICE_NAME ":failed to initialize globals.\n"));

            RTSemFastMutexDestroy(g_VBoxNetFltSolarisMtx);
            g_VBoxNetFltSolarisMtx = NIL_RTSEMFASTMUTEX;
        }

        RTR0Term();
    }
    else
        LogRel((DEVICE_NAME ":failed to initialize IPRT (rc=%d)\n", rc));

    memset(&g_VBoxNetFltSolarisGlobals, 0, sizeof(g_VBoxNetFltSolarisGlobals));
    return RTErrConvertToErrno(rc);
}


int _fini(void)
{
    int rc;
    LogFlow((DEVICE_NAME ":_fini\n"));

    /*
     * Undo the work done during start (in reverse order).
     */
    rc = vboxNetFltTryDeleteIdcAndGlobals(&g_VBoxNetFltSolarisGlobals);
    if (RT_FAILURE(rc))
    {
        LogRel((DEVICE_NAME ":_fini - busy!\n"));
        return EBUSY;
    }

    rc = mod_remove(&g_VBoxNetFltSolarisModLinkage);
    if (!rc)
    {
        if (g_VBoxNetFltSolarisMtx != NIL_RTSEMFASTMUTEX)
        {
            RTSemFastMutexDestroy(g_VBoxNetFltSolarisMtx);
            g_VBoxNetFltSolarisMtx = NIL_RTSEMFASTMUTEX;
        }

        RTR0Term();
    }

    return rc;
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
LOCAL int VBoxNetFltSolarisAttach(dev_info_t *pDip, ddi_attach_cmd_t enmCmd)
{
    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisAttach pDip=%p enmCmd=%d\n", pDip, enmCmd));

    switch (enmCmd)
    {
        case DDI_ATTACH:
        {
            int instance = ddi_get_instance(pDip);
            int rc = ddi_create_priv_minor_node(pDip, DEVICE_NAME, S_IFCHR, instance, DDI_PSEUDO, 0, "none", "none", 0666);
            if (rc == DDI_SUCCESS)
            {
                g_pVBoxNetFltSolarisDip = pDip;
                ddi_report_dev(pDip);
                return DDI_SUCCESS;
            }
            else
                LogRel((DEVICE_NAME ":VBoxNetFltSolarisAttach failed to create minor node. rc=%d\n", rc));
            return DDI_FAILURE;
        }

        case DDI_RESUME:
        {
            /* Nothing to do here... */
            return DDI_SUCCESS;
        }

        /* case DDI_PM_RESUME: */
        default:
            return DDI_FAILURE;
    }
}


/**
 * Detach entry point, to detach a device to the system or suspend it.
 *
 * @param   pDip            The module structure instance.
 * @param   enmCmd          Operation type (detach/suspend).
 *
 * @returns corresponding solaris error code.
 */
LOCAL int VBoxNetFltSolarisDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd)
{
    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisDetach pDip=%p enmCmd=%d\n", pDip, enmCmd));

    switch (enmCmd)
    {
        case DDI_DETACH:
        {
            ddi_remove_minor_node(pDip, NULL);
            return DDI_SUCCESS;
        }

        case DDI_RESUME:
        {
            /* Nothing to do here... */
            return DDI_SUCCESS;
        }

        /* case DDI_PM_SUSPEND: */
        /* case DDI_HOT_PLUG_DETACH: */
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
 * @returns corresponding solaris error code.
 */
LOCAL int VBoxNetFltSolarisGetInfo(dev_info_t *pDip, ddi_info_cmd_t enmCmd, void *pvArg, void **ppResult)
{
    LogFlow((DEVICE_NAME ":VBoxNetFltSolarisGetInfo pDip=%p enmCmd=%d pArg=%p instance=%d\n", pDip, enmCmd, getminor((dev_t)pvArg)));

    switch (enmCmd)
    {
        case DDI_INFO_DEVT2DEVINFO:
        {
            *ppResult = g_pVBoxNetFltSolarisDip;
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
 * Create a solaris message block from the SG list.
 *
 * @param   pThis           The instance.
 * @param   pSG             Pointer to the scatter-gather list.
 *
 * @returns Solaris message block.
 */
LOCAL mblk_t *vboxNetFltSolarisMBlkFromSG(PVBOXNETFLTINS pThis, PINTNETSG pSG, uint32_t fDst)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisMBlkFromSG pThis=%p pSG=%p\n", pThis, pSG));

    mblk_t *pMsg = allocb(pSG->cbTotal, BPRI_HI);
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
 * @param   pThis   The instance
 * @param   pMsg    Pointer to the data message.
 *
 * @returns Number of segments.
 */
LOCAL unsigned vboxNetFltSolarisMBlkCalcSGSegs(PVBOXNETFLTINS pThis, mblk_t *pMsg)
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
 * @param   pThis   The instance.
 * @param   pMsg    Pointer to the data message.
                    The caller must ensure it's not a control message block.
 * @param   pSG     Pointer to the SG.
 * @param   cSegs   Number of segments in the SG.
 *                  This should match the number in the message block exactly!
 * @param   fSrc    The source of the message.
 *
 * @returns VBox status code.
 */
LOCAL int vboxNetFltSolarisMBlkToSG(PVBOXNETFLTINS pThis, mblk_t *pMsg, PINTNETSG pSG, unsigned cSegs, uint32_t fSrc)
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
 * Simple packet dump, used for internal debugging.
 *
 * @param   pMsg    Pointer to the message to analyze and dump.
 */
static void vboxNetFltSolarisAnalyzeMBlk(mblk_t *pMsg)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisAnalyzeMBlk pMsg=%p\n", pMsg));

    PCRTNETETHERHDR pEthHdr = (PCRTNETETHERHDR)pMsg->b_rptr;
    uint8_t *pb = pMsg->b_rptr;
    if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPV4))
    {
        PRTNETIPV4 pIpHdr = (PRTNETIPV4)(pEthHdr + 1);
        if (!pMsg->b_cont)
        {
            if (pIpHdr->ip_p == RTNETIPV4_PROT_ICMP)
                LogFlow((DEVICE_NAME ":ICMP D=%.6Rhxs  S=%.6Rhxs  T=%04x\n", pb, pb + 6, RT_BE2H_U16(*(uint16_t *)(pb + 12))));
            else if (pIpHdr->ip_p == RTNETIPV4_PROT_TCP)
                LogFlow((DEVICE_NAME ":TCP D=%.6Rhxs  S=%.6Rhxs\n", pb, pb + 6));
            else if (pIpHdr->ip_p == RTNETIPV4_PROT_UDP)
            {
                PCRTNETUDP pUdpHdr = (PCRTNETUDP)((uint32_t *)pIpHdr + pIpHdr->ip_hl);
                if (   RT_BE2H_U16(pUdpHdr->uh_sport) == 67
                    && RT_BE2H_U16(pUdpHdr->uh_dport) == 68)
                {
                    LogRel((DEVICE_NAME ":UDP bootp ack D=%.6Rhxs S=%.6Rhxs UDP_CheckSum=%04x Computex=%04x\n", pb, pb + 6,
                                RT_BE2H_U16(pUdpHdr->uh_sum), RT_BE2H_U16(RTNetIPv4UDPChecksum(pIpHdr, pUdpHdr, pUdpHdr + 1))));
                }
            }
        }
        else
        {
            LogFlow((DEVICE_NAME ":Chained IP packet. Skipping validity check.\n"));
        }
    }
    else if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_VLAN))
    {
        typedef struct VLANHEADER
        {
            int Pcp:3;
            int Cfi:1;
            int Vid:12;
        } VLANHEADER;

        VLANHEADER *pVlanHdr = (VLANHEADER *)(pMsg->b_rptr + sizeof(RTNETETHERHDR));
        LogFlow((DEVICE_NAME ":VLAN Pcp=%d Cfi=%d Id=%d\n", pVlanHdr->Pcp, pVlanHdr->Cfi, pVlanHdr->Vid >> 4));
        LogFlow((DEVICE_NAME "%.*Rhxd\n", MBLKL(pMsg), pMsg->b_rptr));
    }
    else if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_ARP))
    {
        PRTNETARPHDR pArpHdr = (PRTNETARPHDR)(pEthHdr + 1);
        LogFlow((DEVICE_NAME ":ARP Op=%d\n", pArpHdr->ar_oper));
    }
    else if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPV6))
    {
        LogFlow((DEVICE_NAME ":IPv6 D=%.6Rhxs S=%.6Rhxs\n", pb, pb + 6));
    }
    else if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPX_1)
             || pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPX_2)
             || pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPX_3))
    {
        LogFlow((DEVICE_NAME ":IPX packet.\n"));
    }
    else
    {
        LogFlow((DEVICE_NAME ":Unknown EtherType=%x D=%.6Rhxs S=%.6Rhxs\n", RT_H2BE_U16(pEthHdr->EtherType), &pEthHdr->DstMac,
                    &pEthHdr->SrcMac));
        /* LogFlow((DEVICE_NAME ":%.*Rhxd\n", MBLKL(pMsg), pMsg->b_rptr)); */
    }
}


/**
 * Receive (rx) entry point.
 *
 * @param   pvData          Private data.
 * @param   hResource       The resource handle.
 * @param   pMsg            The packet.
 * @param   fLoopback       Whether this is a loopback packet or not.
 */
LOCAL void vboxNetFltSolarisRecv(void *pvData, mac_resource_handle_t hResource, mblk_t *pMsg, boolean_t fLoopback)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisRecv pvData=%p pMsg=%p fLoopback=%d cbData=%d\n", pvData, pMsg, fLoopback, pMsg ? MBLKL(pMsg) : 0));

    PVBOXNETFLTINS pThis = (PVBOXNETFLTINS)pvData;
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pMsg);

    /*
     * Active? Retain the instance and increment the busy counter.
     */
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    const bool fActive = ASMAtomicUoReadBool(&pThis->fActive);
    if (fActive)
        vboxNetFltRetain(pThis, true /* fBusy */);
    RTSpinlockRelease(pThis->hSpinlock, &Tmp);
    if (!fActive)
        return;

    uint32_t fSrc = INTNETTRUNKDIR_WIRE;
    PRTNETETHERHDR pEthHdr = (PRTNETETHERHDR)pMsg->b_rptr;
    if (   MBLKL(pMsg) >= sizeof(RTNETETHERHDR)
        && vboxNetFltPortOsIsHostMac(pThis, &pEthHdr->SrcMac))
        fSrc = INTNETTRUNKDIR_HOST;

    /*
     * Route all received packets into the internal network.
     */
    unsigned cSegs = vboxNetFltSolarisMBlkCalcSGSegs(pThis, pMsg);
    PINTNETSG pSG = (PINTNETSG)alloca(RT_OFFSETOF(INTNETSG, aSegs[cSegs]));
    int rc = vboxNetFltSolarisMBlkToSG(pThis, pMsg, pSG, cSegs, fSrc);
    if (RT_SUCCESS(rc))
        pThis->pSwitchPort->pfnRecv(pThis->pSwitchPort, pSG, fSrc);
    else
        LogRel((DEVICE_NAME ":vboxNetFltSolarisMBlkToSG failed. rc=%d\n", rc));

    vboxNetFltRelease(pThis, true /* fBusy */);
    freemsgchain(pMsg);

    NOREF(hResource);
}


/**
 * Create a VNIC dynamically over the given interface.
 *
 * @param   pThis           The VM connection instance.
 *
 * @returns corresponding VBox error code.
 */
LOCAL int vboxNetFltSolarisCreateVNIC(PVBOXNETFLTINS pThis)
{
#if 0
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC pThis=%p\n", pThis));

    datalink_id_t InterfaceLinkId;
    int rc = dls_mgmt_get_linkid(pThis->szName, &InterfaceLinkId);
    if (!rc)
    {
        dev_t DeviceNum = makedevice(ddi_driver_major(g_pVBoxNetFltSolarisDip), pThis->u.s.uInstance);
        char szVNICName[sizeof(VBOXFLT_VNIC_NAME) + 16];
        RTStrPrintf(szVNICName, sizeof(szVNICName), "%s%d", VBOXFLT_VNIC_NAME, pThis->u.s.uInstance);
        LogRel((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC Creating VNIC '%s' over '%s'\n", szVNICName, pThis->szName));
        rc = dls_mgmt_create(szVNICName, DeviceNum, DATALINK_CLASS_VNIC, DL_ETHER, B_FALSE /* Persist */, &pThis->u.s.VNICLinkId);
        if (!rc)
        {
            /* -XXX- @todo remove hardcoded Guest MAC address, should be sent from guest later. */
            RTMAC GuestMac;
            GuestMac.au8[0] = 0x08;
            GuestMac.au8[1] = 0x00;
            GuestMac.au8[2] = 0x27;
            GuestMac.au8[3] = 0xFE;
            GuestMac.au8[4] = 0x21;
            GuestMac.au8[5] = 0x03;

            AssertCompile(sizeof(RTMAC) <= MAXMACADDRLEN);
            uchar_t MacAddr[MAXMACADDRLEN];
            bzero(MacAddr, sizeof(MacAddr));
            bcopy(GuestMac.au8, MacAddr, RT_MIN(sizeof(GuestMac), sizeof(MacAddr)));

            int MacSlot = 0;
            vnic_ioc_diag_t Diag = VNIC_IOC_DIAG_NONE;
            vnic_mac_addr_type_t MacAddrType = VNIC_MAC_ADDR_TYPE_FIXED;
            int cbMac = sizeof(RTMAC);
            int fFlags = VNIC_IOC_CREATE_NODUPCHECK | VNIC_IOC_CREATE_REQ_HWRINGS;
            rc = vnic_dev_create(pThis->u.s.VNICLinkId, InterfaceLinkId, &MacAddrType, &cbMac, MacAddr,
                                &MacSlot, 0 /* Mac Prefix */, 0 /* VLAN Id */, NULL /* Resources Caps */,
                                fFlags, &Diag, kcred);
            if (rc)
            {
                if (Diag == VNIC_IOC_DIAG_NO_HWRINGS)
                {
                    /*
                     * No hardware rings available, retry without requesting hardware ring.
                     */
                    LogRel((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC No hardware rings available for VNIC over '%s'. Recreating VNIC '%s'\n", pThis->szName,
                                    szVNICName));

                    fFlags = VNIC_IOC_CREATE_NODUPCHECK;
                    rc = vnic_dev_create(pThis->u.s.VNICLinkId, InterfaceLinkId, &MacAddrType, &cbMac, MacAddr,
                                        &MacSlot, 0 /* Mac Prefix */, 0 /* VLAN Id */, NULL /* Resources Caps */,
                                        fFlags, &Diag, kcred);
                }
            }

            if (!rc)
            {
                pThis->u.s.fCreatedVNIC = true;
                pThis->u.s.uInstance++;
                LogFlow((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC successfully created VNIC '%s' over '%s'\n", "vboxvnic", pThis->szName));
                return VINF_SUCCESS;
            }
            else
            {
                LogRel((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC failed to create VNIC over '%s'. rc=%d Diagnosis=%d\n", pThis->szName,
                            rc, Diag));
                rc = VERR_INTNET_FLT_VNIC_CREATE_FAILED;
            }

            dls_mgmt_destroy(pThis->u.s.VNICLinkId, B_FALSE /* Persist */);
        }
        else
        {
            LogRel((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC failed to create a link id for a VNIC over '%s' rc=%d\n", pThis->szName, rc));
            rc = VERR_INTNET_FLT_VNIC_CREATE_FAILED;
        }
    }
    else
    {
        LogRel((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC failed to find interface '%s' rc=%d\n", pThis->szName, rc));
        rc = VERR_INTNET_FLT_IF_NOT_FOUND;
    }

    return rc;
#endif
}


/**
 * Attach to the network interface.
 *
 * @param   pThis           The VM connection instance.
 * @param   fRediscovery    Whether this is a rediscovery attempt after disconnect.
 *
 * @returns corresponding VBox error code.
 */
LOCAL int vboxNetFltSolarisAttachToInterface(PVBOXNETFLTINS pThis, bool fRediscovery)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisAttachToInterface pThis=%p fRediscovery=%d\n", pThis, fRediscovery));

    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    /*
     * Open the underlying interface (lower MAC) and get it's handle.
     */
    int rc = mac_open_by_linkname(pThis->szName, &pThis->u.s.hInterface);
    if (RT_LIKELY(!rc))
    {
        /*
         * If this is not a VNIC, create a VNIC and open it instead.
         */
        rc = mac_is_vnic(pThis->u.s.hInterface);
        if (!rc)
        {
            LogRel((DEVICE_NAME ":vboxNetFltSolarisAttachToInterface '%s' is not a VNIC. Creating one.\n", pThis->szName));

            mac_close(pThis->u.s.hInterface);
            pThis->u.s.hInterface = NULL;
            rc = vboxNetFltSolarisCreateVNIC(pThis);
            if (RT_SUCCESS(rc))
                rc = mac_open_by_linkid(pThis->u.s.VNICLinkId, &pThis->u.s.hInterface);
            else
            {
                pThis->u.s.hInterface = NULL;
                LogRel((DEVICE_NAME ":vboxNetFltSolarisAttachToInterface failed to create VNIC. rc=%Rrc\n", rc));
            }
        }
        else
            rc = VINF_SUCCESS;

        /*
         * At this point "hInterface" should be a handle to a VNIC, we no longer would deal with physical interface
         * if it has been passed by the user.
         */
        if (RT_SUCCESS(rc))
        {
            const mac_info_t *pMacInfo = mac_info(pThis->u.s.hInterface);
            if (RT_LIKELY(pMacInfo))
            {
                if (   pMacInfo->mi_media == DL_ETHER
                    && pMacInfo->mi_nativemedia == DL_ETHER)
                {
                    /*
                     * Obtain the MAC address of the interface.
                     */
                    AssertCompile(sizeof(RTMAC) == ETHERADDRL);
                    mac_unicast_primary_get(pThis->u.s.hInterface, (uint8_t *)&pThis->u.s.Mac.au8);

                    LogFlow((DEVICE_NAME ":vboxNetFltSolarisAttachToInterface MAC address of %s is %.*Rhxs\n", pThis->szName,
                                    sizeof(pThis->u.s.Mac), &pThis->u.s.Mac));

                    /** @todo Obtain the MTU size using mac_sdu_get() */
                    /** @todo Obtain capabilities (hardware checksum etc.) using mac_capab_get() */

                    /*
                     * Open a client connection to the lower MAC interface.
                     */
                    rc = mac_client_open(pThis->u.s.hInterface, &pThis->u.s.hClient,
                                         NULL                                   /* name of this client */,
                                         MAC_OPEN_FLAGS_USE_DATALINK_NAME |     /* client name same as underlying NIC */
                                         MAC_OPEN_FLAGS_MULTI_PRIMARY           /* allow multiple primary unicasts */
                                         );
                    if (RT_LIKELY(!rc))
                    {
                        /** @todo -XXX- if possible we must move unicast_add and rx_set to when the Guest advertises it's MAC to us.  */

                        /*
                         * Set a unicast address for this client and the packet receive callback.
                         * We want to use the primary unicast address of the underlying interface (VNIC) hence we pass NULL.
                         * Also we don't really set the RX function here, this is done when we activate promiscuous mode.
                         */
                        mac_diag_t MacDiag;
                        rc = mac_unicast_add(pThis->u.s.hClient, NULL /* MAC Address */,
                                            MAC_UNICAST_PRIMARY | MAC_UNICAST_STRIP_DISABLE | 
                                            MAC_UNICAST_DISABLE_TX_VID_CHECK | MAC_UNICAST_NODUPCHECK | MAC_UNICAST_HW,
                                            &pThis->u.s.hUnicast, 0 /* VLAN id */, &MacDiag);
                        if (!rc)
                        {
                            LogFlow((DEVICE_NAME ":vboxNetFltSolarisAttachToInterface successfully attached over '%s'\n", pThis->szName));
                            return VINF_SUCCESS;
                        }
                        else
                            LogRel((DEVICE_NAME ":vboxNetFltSolarisAttachToInterface failed to set client MAC address over link '%s' rc=%d\n",
                                    pThis->szName, rc));

                        mac_client_close(pThis->u.s.hClient, 0 /* fFlags */);
                        pThis->u.s.hClient = NULL;
                    }
                    else
                    {
                        LogRel((DEVICE_NAME ":vboxNetFltSolarisAttachToInterface failed to create client over link '%s' rc=%d\n",
                                pThis->szName, rc));
                    }
                }
                else
                {
                    LogRel((DEVICE_NAME ":vboxNetFltSolarisAttachToInterface media type incompatible: %d %d\n", pMacInfo->mi_media,
                            pMacInfo->mi_nativemedia));
                    rc = EPROTO;
                }
            }
            else
            {
                LogRel((DEVICE_NAME ":vboxNetFltSolarisAttachToInterface failed to obtain info for link '%s'\n", pThis->szName));
                rc = ENXIO;
            }
        }
        else
        {
            LogFlow((DEVICE_NAME ":vboxNetFltSolarisAttachToInterface VNIC creation failed over '%s'\n", pThis->szName));
            rc = ENXIO;
        }

        if (pThis->u.s.hInterface)
        {
            mac_close(pThis->u.s.hInterface);
            pThis->u.s.hInterface = NULL;
        }
    }
    else
        LogRel((DEVICE_NAME ":vboxNetFltSolarisAttachToInterface failed to open link '%s' rc=%d\n", pThis->szName, rc));

    return RTErrConvertFromErrno(rc);
}



/**
 * Detach from the network interface.
 *
 * @param   pThis           The VM connection instance.
 * @returns corresponding VBox error code.
 */
LOCAL int vboxNetFltSolarisDetachFromInterface(PVBOXNETFLTINS pThis)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (pThis->u.s.hPromiscuous)
    {
        mac_promisc_remove(pThis->u.s.hPromiscuous);
        pThis->u.s.hPromiscuous = NULL;
    }

    if (pThis->u.s.hClient)
    {
        if (pThis->u.s.hUnicast)
        {
            mac_unicast_remove(pThis->u.s.hClient, pThis->u.s.hUnicast);
            pThis->u.s.hUnicast = NULL;
        }

        mac_rx_clear(pThis->u.s.hClient);

        mac_client_close(pThis->u.s.hClient, 0 /* fFlags */);
        pThis->u.s.hClient = NULL;
    }

    if (pThis->u.s.hInterface)
    {
        mac_close(pThis->u.s.hInterface);
        pThis->u.s.hInterface = NULL;
    }

    if (pThis->u.s.fCreatedVNIC)
    {
#if 0
        vnic_dev_delete(pThis->u.s.VNICLinkId, 0 /* Flags -- ununsed in snv_127 */, kcred);
        dls_mgmt_destroy(pThis->u.s.VNICLinkId, B_FALSE /* Persist */);
        pThis->u.s.VNICLinkId = DATALINK_INVALID_LINKID;
        pThis->u.s.fCreatedVNIC = false;
#endif
    }
}


/* -=-=-=-=-=- Common Hooks -=-=-=-=-=- */
bool vboxNetFltPortOsIsPromiscuous(PVBOXNETFLTINS pThis)
{
    LogFlow((DEVICE_NAME ":vboxNetFltPortOsIsPromiscuous pThis=%p\n", pThis));
    return false;
}


void vboxNetFltPortOsGetMacAddress(PVBOXNETFLTINS pThis, PRTMAC pMac)
{
    LogFlow((DEVICE_NAME ":vboxNetFltPortOsGetMacAddress pThis=%p\n", pThis));
    *pMac = pThis->u.s.Mac;
    return;
}


bool vboxNetFltPortOsIsHostMac(PVBOXNETFLTINS pThis, PCRTMAC pMac)
{
    /** @todo What happens when the MAC address of the underlying NIC changes?? */
    return pThis->u.s.Mac.au16[0] == pMac->au16[0]
        && pThis->u.s.Mac.au16[1] == pMac->au16[1]
        && pThis->u.s.Mac.au16[2] == pMac->au16[2];
}


void vboxNetFltPortOsSetActive(PVBOXNETFLTINS pThis, bool fActive)
{
    LogFlow((DEVICE_NAME ":vboxNetFltPortOsSetActive pThis=%p fActive=%d\n", pThis, fActive));

    if (fActive)
    {
        /*
         * Activate promiscuous mode.
         */
        if (!pThis->u.s.hPromiscuous)
        {
            int rc = mac_promisc_add(pThis->u.s.hClient, MAC_CLIENT_PROMISC_ALL, vboxNetFltSolarisRecv, pThis, &pThis->u.s.hPromiscuous,
                                    MAC_PROMISC_FLAGS_NO_TX_LOOP);
            if (rc)
                LogRel((DEVICE_NAME ":vboxNetFltPortOsSetActive cannot enable promiscuous mode for '%s' rc=%d\n", pThis->szName, rc));
        }
    }
    else
    {
        /*
         * Deactivate promiscuous mode.
         */
        if (pThis->u.s.hPromiscuous)
        {
            mac_promisc_remove(pThis->u.s.hPromiscuous);
            pThis->u.s.hPromiscuous = NULL;
        }
    }
}


int vboxNetFltOsDisconnectIt(PVBOXNETFLTINS pThis)
{
    LogFlow((DEVICE_NAME ":vboxNetFltOsDisconnectIt pThis=%p\n", pThis));
    return vboxNetFltSolarisDetachFromInterface(pThis);
}


int  vboxNetFltOsConnectIt(PVBOXNETFLTINS pThis)
{
    LogFlow((DEVICE_NAME ":vboxNetFltOsConnectIt pThis=%p\n", pThis));
    return vboxNetFltSolarisAttachToInterface(pThis, false /* fRediscovery */);
}


void vboxNetFltOsDeleteInstance(PVBOXNETFLTINS pThis)
{
    LogFlow((DEVICE_NAME ":vboxNetFltOsDeleteInstance pThis=%p\n", pThis));
}


int vboxNetFltOsInitInstance(PVBOXNETFLTINS pThis, void *pvContext)
{
    LogFlow((DEVICE_NAME ":vboxNetFltOsInitInstance pThis=%p pvContext=%p\n", pThis, pvContext));

    return VINF_SUCCESS;
}


int vboxNetFltOsPreInitInstance(PVBOXNETFLTINS pThis)
{
    /*
     * Init. the solaris specific data.
     */
    pThis->u.s.VNICLinkId = DATALINK_INVALID_LINKID;
    pThis->u.s.uInstance = 0;
    pThis->u.s.fCreatedVNIC = false;
    pThis->u.s.hInterface = NULL;
    pThis->u.s.hClient = NULL;
    pThis->u.s.hUnicast = NULL;
    pThis->u.s.hPromiscuous = NULL;
    bzero(&pThis->u.s.Mac, sizeof(pThis->u.s.Mac));
    return VINF_SUCCESS;
}


bool vboxNetFltOsMaybeRediscovered(PVBOXNETFLTINS pThis)
{
    /*
     * @todo Think about this.
     */
    return false;
}


int vboxNetFltPortOsXmit(PVBOXNETFLTINS pThis, PINTNETSG pSG, uint32_t fDst)
{
    /*
     * Xmit the packet down the interface (tx)
     */
    int rc = VERR_INVALID_HANDLE;
    if (RT_LIKELY(pThis->u.s.hClient))
    {
        mblk_t *pMsg = vboxNetFltSolarisMBlkFromSG(pThis, pSG, fDst);
        if (RT_LIKELY(pMsg))
        {
            LogFlow((DEVICE_NAME ":vboxNetFltPortOsXmit pThis=%p cbData=%d\n", pThis, MBLKL(pMsg)));

            mac_tx_cookie_t pXmitCookie = mac_tx(pThis->u.s.hClient, pMsg, 0 /* Hint */, MAC_DROP_ON_NO_DESC, NULL /* return message */);
            if (RT_LIKELY(!pXmitCookie))
                return VINF_SUCCESS;

            pMsg = NULL;
            rc = VERR_NET_IO_ERROR;
            LogFlow((DEVICE_NAME ":vboxNetFltPortOsXmit Xmit failed.\n"));
        }
        else
        {
            LogRel((DEVICE_NAME ":vboxNetFltPortOsXmit no memory for allocating Xmit packet.\n"));
            rc = VERR_NO_MEMORY;
        }
    }
    else
        LogFlow((DEVICE_NAME ":vboxNetFltPortOsXmit missing client.\n"));

    return rc;
}

