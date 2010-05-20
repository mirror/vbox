/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Solaris Specific Code.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_NET_FLT_DRV
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/intnetinline.h>
#include <VBox/version.h>
#include <iprt/initterm.h>
#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/rand.h>
#include <iprt/net.h>
#include <iprt/spinlock.h>
#include <iprt/mem.h>

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

#include <sys/vnic_mgmt.h>
#include <sys/mac_client.h>
#include <sys/mac_provider.h>
#include <sys/dls.h>

#if 0
#include "include/mac_provider.h"       /* dependency for other headers */
#include "include/mac_client.h"         /* for mac_* */
#include "include/mac_client_priv.h"    /* for mac_info, mac_capab_get etc. */
#if 1
#include "include/dls.h"                /* for dls_mgmt_* */
#include "include/dld_ioc.h"            /* required by vnic.h */
#include "include/vnic.h"               /* for vnic_ioc_diag_t */
#include "include/vnic_impl.h"          /* for vnic_dev_create */
#endif
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
# define LOCAL                          static
/** VBOXNETFLTVNIC::u32Magic */
# define VBOXNETFLTVNIC_MAGIC           0x0ddfaced

#if defined(DEBUG_ramshankar)
# undef Log
# define Log        LogRel
# undef LogFlow
# define LogFlow    LogRel
# undef LOCAL
# define LOCAL
#endif

/** VLAN tag masking, should probably be in IPRT? */
#define VLAN_ID(vlan)          (((vlan) >>  0) & 0x0fffu)
#define VLAN_CFI(vlan)         (((vlan) >> 12) & 0x0001u)
#define VLAN_PRI(vlan)         (((vlan) >> 13) & 0x0007u)
#define VLAN_TAG(pri,cfi,vid)  (((pri) << 13) | ((cfi) << 12) | ((vid) << 0))

typedef struct VLANHEADER
{
    uint16_t Type;
    uint16_t Data;
} VLANHEADER;
typedef struct VLANHEADER *PVLANHEADER;


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

/**
 * VBOXNETFLTVNIC: Per-VNIC instance data.
 */
typedef struct VBOXNETFLTVNIC
{
    uint32_t             u32Magic;        /* Magic number (VBOXNETFLTVNIC_MAGIC) */
    bool                 fCreated;        /* Whether we created the VNIC or not */
    void                *pvIf;            /* The VirtualBox interface */
    mac_handle_t         hInterface;      /* The lower MAC handle */
    datalink_id_t        hLinkId;         /* The link ID */
    mac_client_handle_t  hClient;         /* Client handle */
    mac_unicast_handle_t hUnicast;        /* Unicast address handle  */
    mac_promisc_handle_t hPromiscuous;    /* Promiscuous handle */
    char                 szName[128];     /* The VNIC name */
    list_node_t          hNode;           /* Handle to the next VNIC in the list */
} VBOXNETFLTVNIC;
typedef struct VBOXNETFLTVNIC *PVBOXNETFLTVNIC;


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
*   Internal Functions                                                         *
*******************************************************************************/
LOCAL mblk_t *vboxNetFltSolarisMBlkFromSG(PVBOXNETFLTINS pThis, PINTNETSG pSG, uint32_t fDst);
LOCAL unsigned vboxNetFltSolarisMBlkCalcSGSegs(PVBOXNETFLTINS pThis, mblk_t *pMsg);
LOCAL int vboxNetFltSolarisMBlkToSG(PVBOXNETFLTINS pThis, mblk_t *pMsg, PINTNETSG pSG, unsigned cSegs, uint32_t fSrc);
LOCAL void vboxNetFltSolarisRecv(void *pvData, mac_resource_handle_t hResource, mblk_t *pMsg, boolean_t fLoopback);
LOCAL void vboxNetFltSolarisAnalyzeMBlk(mblk_t *pMsg);
LOCAL void vboxNetFltSolarisReportInfo(PVBOXNETFLTINS pThis);
LOCAL int vboxNetFltSolarisInitVNIC(PVBOXNETFLTINS pThis, PVBOXNETFLTVNIC pVNIC);
LOCAL PVBOXNETFLTVNIC vboxNetFltSolarisAllocVNIC(void);
LOCAL void vboxNetFltSolarisFreeVNIC(PVBOXNETFLTVNIC pVNIC);
LOCAL void vboxNetFltSolarisDestroyVNIC(PVBOXNETFLTVNIC pVNIC);
LOCAL int vboxNetFltSolarisCreateVNIC(PVBOXNETFLTINS pThis, PVBOXNETFLTVNIC *ppVNIC);


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

    /*
     * Convert the message block to segments. Works cbTotal and sets cSegsUsed.
     */
    IntNetSgInitTempSegs(pSG, 0 /*cbTotal*/, cSegs, 0 /*cSegsUsed*/);
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
        Assert(iSeg + 1 < cSegs);
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
LOCAL void vboxNetFltSolarisAnalyzeMBlk(mblk_t *pMsg)
{
    LogFlowFunc((DEVICE_NAME ":vboxNetFltSolarisAnalyzeMBlk pMsg=%p\n", pMsg));

    PCRTNETETHERHDR pEthHdr = (PCRTNETETHERHDR)pMsg->b_rptr;
    uint8_t *pb = pMsg->b_rptr;
    if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPV4))
    {
        PRTNETIPV4 pIpHdr = (PRTNETIPV4)(pEthHdr + 1);
        if (!pMsg->b_cont)
        {
            if (pIpHdr->ip_p == RTNETIPV4_PROT_ICMP)
                LogRel((DEVICE_NAME ":ICMP D=%.6Rhxs  S=%.6Rhxs  T=%04x\n", pb, pb + 6, RT_BE2H_U16(*(uint16_t *)(pb + 12))));
            else if (pIpHdr->ip_p == RTNETIPV4_PROT_TCP)
                LogRel((DEVICE_NAME ":TCP D=%.6Rhxs  S=%.6Rhxs\n", pb, pb + 6));
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
        PVLANHEADER pVlanHdr = (PVLANHEADER)(pMsg->b_rptr + sizeof(RTNETETHERHDR) - sizeof(pEthHdr->EtherType));
        LogRel((DEVICE_NAME ":VLAN Pcp=%u Cfi=%u Id=%u\n", VLAN_PRI(RT_BE2H_U16(pVlanHdr->Data)), VLAN_CFI(RT_BE2H_U16(pVlanHdr->Data)), VLAN_ID(RT_BE2H_U16(pVlanHdr->Data))));
        LogRel((DEVICE_NAME "%.*Rhxd\n", sizeof(VLANHEADER), pVlanHdr));
    }
    else if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_ARP))
    {
        PRTNETARPHDR pArpHdr = (PRTNETARPHDR)(pEthHdr + 1);
        LogRel((DEVICE_NAME ":ARP Op=%d\n", pArpHdr->ar_oper));
    }
    else if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPV6))
    {
        LogRel((DEVICE_NAME ":IPv6 D=%.6Rhxs S=%.6Rhxs\n", pb, pb + 6));
    }
    else if (pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPX_1)
             || pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPX_2)
             || pEthHdr->EtherType == RT_H2BE_U16(RTNET_ETHERTYPE_IPX_3))
    {
        LogRel((DEVICE_NAME ":IPX packet.\n"));
    }
    else
    {
        LogRel((DEVICE_NAME ":Unknown EtherType=%x D=%.6Rhxs S=%.6Rhxs\n", RT_H2BE_U16(pEthHdr->EtherType), &pEthHdr->DstMac,
                    &pEthHdr->SrcMac));
        /* LogFlow((DEVICE_NAME ":%.*Rhxd\n", MBLKL(pMsg), pMsg->b_rptr)); */
    }
}


/**
 * Helper.
 */
DECLINLINE(bool) vboxNetFltPortSolarisIsHostMac(PVBOXNETFLTINS pThis, PCRTMAC pMac)
{
    return pThis->u.s.MacAddr.au16[0] == pMac->au16[0]
        && pThis->u.s.MacAddr.au16[1] == pMac->au16[1]
        && pThis->u.s.MacAddr.au16[2] == pMac->au16[2];
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
    if (!vboxNetFltTryRetainBusyActive(pThis))
        return;

    uint32_t fSrc = INTNETTRUNKDIR_WIRE;
    PRTNETETHERHDR pEthHdr = (PRTNETETHERHDR)pMsg->b_rptr;
    if (   MBLKL(pMsg) >= sizeof(RTNETETHERHDR)
        && vboxNetFltPortSolarisIsHostMac(pThis, &pEthHdr->SrcMac))
        fSrc = INTNETTRUNKDIR_HOST;

    /*
     * Route all received packets into the internal network.
     */
    uint16_t cFailed = 0;
    for (mblk_t *pCurMsg = pMsg; pCurMsg != NULL; pCurMsg = pCurMsg->b_next)
    {
        unsigned cSegs = vboxNetFltSolarisMBlkCalcSGSegs(pThis, pCurMsg);
        PINTNETSG pSG = (PINTNETSG)alloca(RT_OFFSETOF(INTNETSG, aSegs[cSegs]));
        int rc = vboxNetFltSolarisMBlkToSG(pThis, pMsg, pSG, cSegs, fSrc);
        if (RT_SUCCESS(rc))
            pThis->pSwitchPort->pfnRecv(pThis->pSwitchPort, NULL, pSG, fSrc);
        else
            cFailed++;
    }
    vboxNetFltRelease(pThis, true /* fBusy */);

    if (RT_UNLIKELY(cFailed))
        LogRel((DEVICE_NAME ":vboxNetFltSolarisMBlkToSG failed for %u packets.\n", cFailed));

    freemsgchain(pMsg);

    NOREF(hResource);
}


/**
 * Report capabilities and MAC address to IntNet.
 *
 * @param   pThis           The instance.
 * @remarks Retains the instance while doing it's job.
 */
LOCAL void vboxNetFltSolarisReportInfo(PVBOXNETFLTINS pThis)
{
    if (!pThis->u.s.fReportedInfo)
    {
        if (vboxNetFltTryRetainBusyNotDisconnected(pThis))
        {
            Assert(pThis->pSwitchPort);
            pThis->pSwitchPort->pfnReportMacAddress(pThis->pSwitchPort, &pThis->u.s.MacAddr);
            pThis->pSwitchPort->pfnReportPromiscuousMode(pThis->pSwitchPort, false); /** @todo Promisc */
            pThis->pSwitchPort->pfnReportGsoCapabilities(pThis->pSwitchPort, 0, INTNETTRUNKDIR_WIRE | INTNETTRUNKDIR_HOST);
            pThis->pSwitchPort->pfnReportNoPreemptDsts(pThis->pSwitchPort, 0 /* none */);
            vboxNetFltRelease(pThis, true /*fBusy*/);
            pThis->u.s.fReportedInfo = true;
        }
    }
}


/**
 * Initialize a VNIC.
 *
 * @param   pThis           The instance.
 * @param   pVNIC           Pointer to the VNIC.
 *
 * @returns Solaris error code (errno).
 */
LOCAL int vboxNetFltSolarisInitVNIC(PVBOXNETFLTINS pThis, PVBOXNETFLTVNIC pVNIC)
{
    /*
     * Some paranoia.
     */
    AssertReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pVNIC, VERR_INVALID_PARAMETER);
    AssertReturn(pVNIC->hInterface, VERR_INVALID_POINTER);
    AssertReturn(pVNIC->hLinkId, VERR_INVALID_POINTER);
    AssertReturn(!pVNIC->hClient, VERR_INVALID_HANDLE);

    int rc = mac_client_open(pVNIC->hInterface, &pVNIC->hClient,
                         NULL,                                   /* name of this client */
                         MAC_OPEN_FLAGS_USE_DATALINK_NAME |      /* client name same as underlying NIC */
                         MAC_OPEN_FLAGS_MULTI_PRIMARY            /* allow multiple primary unicasts */
                         );
    if (RT_LIKELY(!rc))
    {
        /*
         * Set the RX callback.
         */
        mac_diag_t Diag = MAC_DIAG_NONE;
        rc = mac_unicast_add_set_rx(pVNIC->hClient,
                                        NULL                        /* MAC address, use existing VNIC address */,
                                        MAC_UNICAST_PRIMARY |       /* Use Primary address of the VNIC */
                                            MAC_UNICAST_NODUPCHECK, /* Don't fail for conflicting MAC/VLAN-id combinations */
                                        &pVNIC->hUnicast,
                                        0                           /* VLAN-id */,
                                        &Diag,
                                        vboxNetFltSolarisRecv,      /* RX callback */
                                        pThis                       /* callback private data */
                                        );
        if (RT_LIKELY(!rc))
        {
            if (!pThis->u.s.fReportedInfo)
            {
                /*
                 * Obtain the MAC address of the underlying physical interface.
                 */
                mac_handle_t hLowerMac = mac_get_lower_mac_handle(pVNIC->hInterface);
                if (RT_LIKELY(hLowerMac))
                {
                    mac_unicast_primary_get(hLowerMac, (uint8_t *)pThis->u.s.MacAddr.au8);
                    vboxNetFltSolarisReportInfo(pThis);
                }
                else
                {
                    LogRel((DEVICE_NAME ":vboxNetFltOsInitInstance failed to get lower MAC handle for '%s'\n", pThis->szName));
                    rc = ENODEV;
                }
            }

            if (!rc)
            {
                Assert(pVNIC->hClient);
                Assert(pVNIC->hInterface);
                Assert(pVNIC->hLinkId);
                LogFlow((DEVICE_NAME ":vboxNetFltOsInitInstance successfully initialized VNIC '%s'\n", pVNIC->szName));
                return 0;
            }

            mac_unicast_remove(pVNIC->hClient, pVNIC->hUnicast);
            mac_rx_clear(pVNIC->hClient);
            pVNIC->hUnicast = NULL;
        }
        else
            LogRel((DEVICE_NAME ":vboxNetFltOsInitInstance failed to set RX callback. rc=%d Diag=%d\n", rc, Diag));

        mac_client_close(pVNIC->hClient, 0 /* flags */);
        pVNIC->hClient = NULL;
    }
    else
        LogRel((DEVICE_NAME ":vboxNetFltOsInitInstance failed to open mac client for '%s' rc=%d\n", pThis->szName, rc));

    return RTErrConvertFromErrno(rc);
}


/**
 * Allocate a VNIC structure.
 *
 * @returns An allocated VNIC structure or NULL in case of errors.
 */
LOCAL PVBOXNETFLTVNIC vboxNetFltSolarisAllocVNIC(void)
{
    PVBOXNETFLTVNIC pVNIC = RTMemAlloc(sizeof(VBOXNETFLTVNIC));
    if (RT_UNLIKELY(!pVNIC))
        return NULL;

    pVNIC->u32Magic = VBOXNETFLTVNIC_MAGIC;
    pVNIC->fCreated = false;
    pVNIC->pvIf = NULL;
    pVNIC->hInterface = NULL;
    pVNIC->hLinkId = DATALINK_INVALID_LINKID;
    pVNIC->hClient = NULL;
    pVNIC->hUnicast = NULL;
    pVNIC->hPromiscuous = NULL;
    RT_ZERO(pVNIC->szName);
    list_link_init(&pVNIC->hNode);
    return pVNIC;
}


/**
 * Frees an allocated VNIC.
 *
 * @param   pVNIC           Pointer to the VNIC.
 */
LOCAL void vboxNetFltSolarisFreeVNIC(PVBOXNETFLTVNIC pVNIC)
{
    if (pVNIC)
        RTMemFree(pVNIC);
}


/**
 * Destroy a created VNIC if it was created by us, or just
 * de-initializes the VNIC freeing up resources handles.
 *
 * @param   pVNIC           Pointer to the VNIC.
 */
LOCAL void vboxNetFltSolarisDestroyVNIC(PVBOXNETFLTVNIC pVNIC)
{
    if (pVNIC)
    {
        if (pVNIC->hPromiscuous)
        {
            mac_promisc_remove(pVNIC->hPromiscuous);
            pVNIC->hPromiscuous = NULL;
        }

        if (pVNIC->hClient)
        {
            if (pVNIC->hUnicast)
            {
                mac_unicast_remove(pVNIC->hClient, pVNIC->hUnicast);
                pVNIC->hUnicast = NULL;
            }

            mac_rx_clear(pVNIC->hClient);

            mac_client_close(pVNIC->hClient, 0 /* fFlags */);
            pVNIC->hClient = NULL;
        }

        if (pVNIC->hInterface)
        {
            mac_close(pVNIC->hInterface);
            pVNIC->hInterface = NULL;
        }

        if (pVNIC->fCreated)
        {
            vnic_delete(pVNIC->hLinkId, 0 /* Flags */);
            pVNIC->hLinkId = DATALINK_INVALID_LINKID;
            pVNIC->fCreated = false;
        }
    }
}


/**
 * Create a non-persistent VNIC over the given interface.
 *
 * @param   pThis           The VM connection instance.
 * @param   ppVNIC          Where to store the created VNIC.
 *
 * @returns corresponding VBox error code.
 */
LOCAL int vboxNetFltSolarisCreateVNIC(PVBOXNETFLTINS pThis, PVBOXNETFLTVNIC *ppVNIC)
{
    LogFlow((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC pThis=%p\n", pThis));

    AssertReturn(pThis, VERR_INVALID_POINTER);
    AssertReturn(ppVNIC, VERR_INVALID_POINTER);

    PVBOXNETFLTVNIC pVNIC = vboxNetFltSolarisAllocVNIC();
    if (RT_UNLIKELY(!pVNIC))
        return VERR_NO_MEMORY;

    AssertCompile(sizeof(pVNIC->szName) > sizeof(VBOXFLT_VNIC_NAME) + 64);
    RTStrPrintf(pVNIC->szName, sizeof(pVNIC->szName), "%s%RU64", VBOXFLT_VNIC_NAME, pThis->u.s.uInstance);

    /*
     * Set a random MAC address.
     */
    RTMAC GuestMac;
    GuestMac.au8[0] = 0x08;
    GuestMac.au8[1] = 0x00;
    GuestMac.au8[2] = 0x27;
    RTRandBytes(&GuestMac.au8[3], 3);

    AssertCompile(sizeof(RTMAC) <= MAXMACADDRLEN);

    vnic_mac_addr_type_t AddrType = VNIC_MAC_ADDR_TYPE_FIXED;
    vnic_ioc_diag_t Diag          = VNIC_IOC_DIAG_NONE;
    int MacSlot                   = 0;
    int MacLen                    = sizeof(GuestMac);
    uint32_t fFlags               = 0; /* @todo it should be VNIC_IOC_CREATE_NODUPCHECK */

    int rc = vnic_create(pVNIC->szName, pThis->szName, &AddrType, &MacLen, GuestMac.au8, &MacSlot, 0 /* Mac-Prefix Length */, 0 /* VLAN-ID */,
                        fFlags, &pVNIC->hLinkId, &Diag, NULL /* Reserved */);
    if (!rc)
    {
        pVNIC->fCreated = true;

        /*
         * Now try opening the created VNIC.
         */
        rc = mac_open_by_linkid(pVNIC->hLinkId, &pVNIC->hInterface);
        if (!rc)
        {
            rc = vboxNetFltSolarisInitVNIC(pThis, pVNIC);
            if (RT_LIKELY(!rc))
            {
                pThis->u.s.uInstance++;
                LogFlow((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC successfully created VNIC '%s' over '%s'\n", pVNIC->szName, pThis->szName));
                *ppVNIC = pVNIC;
                return VINF_SUCCESS;
            }
            else
                LogRel((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC vboxNetFltSolarisInitVNIC failed. rc=%d\n", rc));

            mac_close(pVNIC->hInterface);
            pVNIC->hInterface = NULL;
        }
        else
        {
            LogRel((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC failed to open VNIC '%s' over '%s'. rc=%d\n", pVNIC->szName,
                        pThis->szName, rc));
        }

        vboxNetFltSolarisDestroyVNIC(pVNIC);
        rc = VERR_OPEN_FAILED;
    }
    else
    {
        LogRel((DEVICE_NAME ":vboxNetFltSolarisCreateVNIC failed to create VNIC '%s' over '%s' rc=%d Diag=%d\n", pVNIC->szName,
                    pThis->szName, rc, Diag));
        rc = VERR_INTNET_FLT_VNIC_CREATE_FAILED;
    }

    vboxNetFltSolarisFreeVNIC(pVNIC);

    return rc;
}


/* -=-=-=-=-=- Common Hooks -=-=-=-=-=- */


void vboxNetFltPortOsSetActive(PVBOXNETFLTINS pThis, bool fActive)
{
    LogFlow((DEVICE_NAME ":vboxNetFltPortOsSetActive pThis=%p fActive=%d\n", pThis, fActive));
#if 0
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
#endif
}


int vboxNetFltOsDisconnectIt(PVBOXNETFLTINS pThis)
{
    LogFlow((DEVICE_NAME ":vboxNetFltOsDisconnectIt pThis=%p\n", pThis));
    return VINF_SUCCESS;
}


int  vboxNetFltOsConnectIt(PVBOXNETFLTINS pThis)
{
    LogFlow((DEVICE_NAME ":vboxNetFltOsConnectIt pThis=%p\n", pThis));
    return VINF_SUCCESS;
}


void vboxNetFltOsDeleteInstance(PVBOXNETFLTINS pThis)
{
    LogFlow((DEVICE_NAME ":vboxNetFltOsDeleteInstance pThis=%p\n", pThis));

    /*
     * Destroy all managed VNICs. If a VNIC was passed to us, there
     * will be only 1 item in the list, otherwise as many interfaces
     * that were somehow not destroyed using DisconnectInterface() will be
     * present.
     */
    PVBOXNETFLTVNIC pVNIC = NULL;
    while ((pVNIC = list_remove_head(&pThis->u.s.hVNICs)) != NULL)
    {
        vboxNetFltSolarisDestroyVNIC(pVNIC);
        vboxNetFltSolarisFreeVNIC(pVNIC);
    }

    list_destroy(&pThis->u.s.hVNICs);
}


int vboxNetFltOsInitInstance(PVBOXNETFLTINS pThis, void *pvContext)
{
    LogFlow((DEVICE_NAME ":vboxNetFltOsInitInstance pThis=%p pvContext=%p\n", pThis, pvContext));

    /*
     * Figure out if the interface is a VNIC or a physical/etherstub/whatever NIC.
     */
    mac_handle_t hInterface;
    int rc = mac_open_by_linkname(pThis->szName, &hInterface);
    if (RT_LIKELY(!rc))
    {
        rc = mac_is_vnic(hInterface);
        if (!rc)
        {
            /*
             * This is NOT a VNIC. Just pretend success for now.
             * We will create a VNIC per guest interface later (see vboxNetFltPortOsConnectInterface).
             */
            pThis->u.s.fIsVNIC = false;
            mac_unicast_primary_get(hInterface, pThis->u.s.MacAddr.au8);
            vboxNetFltSolarisReportInfo(pThis);
            mac_close(hInterface);
            return VINF_SUCCESS;
        }

        pThis->u.s.fIsVNIC = true;

        PVBOXNETFLTVNIC pVNIC = vboxNetFltSolarisAllocVNIC();
        if (RT_LIKELY(pVNIC))
        {
            pVNIC->fCreated = false;
            pVNIC->hInterface = hInterface;

            /*
             * Obtain the data link ID for this VNIC, it's needed for modifying the MAC address among other things.
             */
            rc = dls_mgmt_get_linkid(pThis->szName, &pVNIC->hLinkId);
            if (RT_LIKELY(!rc))
            {
                /*
                 * Initialize the VNIC and add it to the list of managed VNICs.
                 */
                RTStrPrintf(pVNIC->szName, sizeof(pVNIC->szName), "%s", pThis->szName);
                rc = vboxNetFltSolarisInitVNIC(pThis, pVNIC);
                if (!rc)
                {
                    list_insert_head(&pThis->u.s.hVNICs, pVNIC);
                    return VINF_SUCCESS;
                }
                else
                    LogRel((DEVICE_NAME ":vboxNetFltOsInitInstance vboxNetFltSolarisInitVNIC failed. rc=%d\n", rc));
            }
            else
                LogRel((DEVICE_NAME ":vboxNetFltOsInitInstance failed to get link id for '%s'. rc=%d\n", pThis->szName, rc));

            vboxNetFltSolarisFreeVNIC(pVNIC);
        }
        else
            LogRel((DEVICE_NAME ":vboxNetFltOsInitInstance failed to allocate VNIC private data.\n"));

        mac_close(hInterface);
    }
    else
        LogRel((DEVICE_NAME ":vboxNetFltOsInitInstance failed to open link '%s'! rc=%d\n", pThis->szName));

    return RTErrConvertFromErrno(rc);
}


int vboxNetFltOsPreInitInstance(PVBOXNETFLTINS pThis)
{
    /*
     * Init. the solaris specific data.
     */
    pThis->u.s.fIsVNIC = false;
    list_create(&pThis->u.s.hVNICs, sizeof(VBOXNETFLTVNIC), offsetof(VBOXNETFLTVNIC, hNode));
    pThis->u.s.uInstance = 0;
    pThis->u.s.pvVNIC = NULL;
    bzero(&pThis->u.s.MacAddr, sizeof(pThis->u.s.MacAddr));
    pThis->u.s.fReportedInfo = false;
    return VINF_SUCCESS;
}


bool vboxNetFltOsMaybeRediscovered(PVBOXNETFLTINS pThis)
{
    /*
     * @todo Think about this.
     */
    return false;
}


int vboxNetFltPortOsXmit(PVBOXNETFLTINS pThis, void *pvIfData, PINTNETSG pSG, uint32_t fDst)
{
    /*
     * Validate parameters.
     */
    PVBOXNETFLTVNIC pVNIC = pvIfData;
    AssertMsgReturn(VALID_PTR(pVNIC) && pVNIC->u32Magic == VBOXNETFLTVNIC_MAGIC,
                    ("Invalid pvIfData=%p magic=%#x (expected %#x)\n", pvIfData, pVNIC ? pVNIC->u32Magic : 0, VBOXNETFLTVNIC_MAGIC),
                    VERR_INVALID_PARAMETER);

    /*
     * Xmit the packet down the appropriate VNIC interface.
     */
    int rc = VINF_SUCCESS;
    mblk_t *pMsg = vboxNetFltSolarisMBlkFromSG(pThis, pSG, fDst);
    if (RT_LIKELY(pMsg))
    {
        LogFlow((DEVICE_NAME ":vboxNetFltPortOsXmit pThis=%p cbData=%d\n", pThis, MBLKL(pMsg)));

        mac_tx_cookie_t pXmitCookie = mac_tx(pVNIC->hClient, pMsg, 0 /* Hint */, MAC_DROP_ON_NO_DESC, NULL /* return message */);
        if (RT_LIKELY(!pXmitCookie))
            return VINF_SUCCESS;

        pMsg = NULL;
        rc = VERR_NET_IO_ERROR;
        LogFlow((DEVICE_NAME ":vboxNetFltPortOsXmit Xmit failed pVNIC=%p.\n", pVNIC));
    }
    else
    {
        LogRel((DEVICE_NAME ":vboxNetFltPortOsXmit no memory for allocating Xmit packet.\n"));
        rc = VERR_NO_MEMORY;
    }

    return rc;
}


void vboxNetFltPortOsNotifyMacAddress(PVBOXNETFLTINS pThis, void *pvIfData, PCRTMAC pMac)
{
    LogFlow((DEVICE_NAME ":vboxNetFltPortOSNotifyMacAddress %s %.6Rhxs\n", pThis->szName, pMac));

    /*
     * Validate parameters.
     */
    PVBOXNETFLTVNIC pVNIC = pvIfData;
    AssertMsgReturnVoid(VALID_PTR(pVNIC) && pVNIC->u32Magic == VBOXNETFLTVNIC_MAGIC,
                    ("Invalid pvIfData=%p magic=%#x (expected %#x)\n", pvIfData, pVNIC ? pVNIC->u32Magic : 0, VBOXNETFLTVNIC_MAGIC));
    AssertMsgReturnVoid(pVNIC->hLinkId != DATALINK_INVALID_LINKID,
                    ("Invalid hLinkId pVNIC=%p magic=%#x\n", pVNIC, pVNIC->u32Magic));

    /*
     * Set the MAC address of the VNIC to the one used by the VM interface.
     */
    uchar_t au8GuestMac[MAXMACADDRLEN];
    bcopy(pMac->au8, au8GuestMac, sizeof(RTMAC));

    vnic_mac_addr_type_t AddrType = VNIC_MAC_ADDR_TYPE_FIXED;
    vnic_ioc_diag_t      Diag     = VNIC_IOC_DIAG_NONE;
    int                  MacSlot  = 0;
    int                  MacLen   = sizeof(RTMAC);

    int rc = vnic_modify_addr(pVNIC->hLinkId, &AddrType, &MacLen, au8GuestMac, &MacSlot, 0 /* Mac-Prefix Length */, &Diag);
    if (RT_UNLIKELY(rc))
        LogRel((DEVICE_NAME ":vboxNetFltPortOsNotifyMacAddress failed! rc=%d Diag=%d\n", rc, Diag));
}


int vboxNetFltPortOsConnectInterface(PVBOXNETFLTINS pThis, void *pvIf, void **ppvIfData)
{
    LogFlow((DEVICE_NAME ":vboxNetFltPortOsConnectInterface pThis=%p pvIf=%p\n", pThis, pvIf));

    int rc = VINF_SUCCESS;

    /*
     * If the underlying interface is not a VNIC, we need to create
     * a VNIC per guest NIC.
     */
    if (!pThis->u.s.fIsVNIC)
    {
        PVBOXNETFLTVNIC pVNIC = NULL;
        rc = vboxNetFltSolarisCreateVNIC(pThis, &pVNIC);
        if (RT_SUCCESS(rc))
        {
            /*
             * VM Interface<->VNIC association so that we can Xmit/Recv on the right ones.
             */
            pVNIC->pvIf = pvIf;
            *ppvIfData = pVNIC;

            /*
             * Add the created VNIC to the list of VNICs we manage.
             */
            list_insert_tail(&pThis->u.s.hVNICs, pVNIC);
            LogFlow((DEVICE_NAME ":vboxNetFltPortOsConnectInterface successfully created VNIC '%s'.\n", pVNIC->szName));
            return VINF_SUCCESS;
        }
        else
            LogRel((DEVICE_NAME ":vboxNetFltPortOsConnectInterface failed to create VNIC rc=%d\n", rc));
    }
    else
    {
        PVBOXNETFLTVNIC pVNIC = list_head(&pThis->u.s.hVNICs);
        if (RT_LIKELY(pVNIC))
        {
            *ppvIfData = pVNIC;
            LogFlow((DEVICE_NAME ":vboxNetFltPortOsConnectInterface set VNIC '%s' private data\n", pVNIC->szName));
        }
        else
        {
            LogRel((DEVICE_NAME ":vboxNetFltPortOsConnectInterface huh!? Missing VNIC!\n"));
            return VERR_GENERAL_FAILURE;
        }
    }

    return rc;
}


int vboxNetFltPortOsDisconnectInterface(PVBOXNETFLTINS pThis, void *pvIfData)
{
    LogFlow((DEVICE_NAME ":vboxNetFltPortOsDisconnectInterface pThis=%p\n", pThis));

    PVBOXNETFLTVNIC pVNIC = pvIfData;
    AssertMsgReturn(VALID_PTR(pVNIC) && pVNIC->u32Magic == VBOXNETFLTVNIC_MAGIC,
                    ("Invalid pvIfData=%p magic=%#x (expected %#x)\n", pvIfData, pVNIC ? pVNIC->u32Magic : 0, VBOXNETFLTVNIC_MAGIC),
                    VERR_INVALID_POINTER);

    /*
     * If the underlying interface is not a VNIC, we need to delete the created VNIC.
     */
    if (!pThis->u.s.fIsVNIC)
    {
        /*
         * Remove the VNIC from the list, destroy and free it.
         */
        list_remove(&pThis->u.s.hVNICs, pVNIC);
        LogRel((DEVICE_NAME ":vboxNetFltPortOsDisconnectInterface destroying pVNIC=%p\n", pVNIC));
        vboxNetFltSolarisDestroyVNIC(pVNIC);
        vboxNetFltSolarisFreeVNIC(pVNIC);
    }

    return VINF_SUCCESS;
}

