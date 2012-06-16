/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Linux Specific Code.
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
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
#define VBOXNETFLT_LINUX_NO_XMIT_QUEUE
#include "the-linux-kernel.h"
#include "version-generated.h"
#include "product-generated.h"
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/miscdevice.h>
#include <linux/ip.h>
#include <linux/if_vlan.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/intnetinline.h>
#include <VBox/vmm/pdmnetinline.h>
#include <VBox/param.h>
#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/semaphore.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <iprt/mem.h>
#include <iprt/net.h>
#include <iprt/log.h>
#include <iprt/mp.h>
#include <iprt/mem.h>
#include <iprt/time.h>

#define VBOXNETFLT_OS_SPECFIC 1
#include "../VBoxNetFltInternal.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define VBOX_FLT_NB_TO_INST(pNB)    RT_FROM_MEMBER(pNB, VBOXNETFLTINS, u.s.Notifier)
#define VBOX_FLT_PT_TO_INST(pPT)    RT_FROM_MEMBER(pPT, VBOXNETFLTINS, u.s.PacketType)
#ifndef VBOXNETFLT_LINUX_NO_XMIT_QUEUE
# define VBOX_FLT_XT_TO_INST(pXT)   RT_FROM_MEMBER(pXT, VBOXNETFLTINS, u.s.XmitTask)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
# define VBOX_SKB_RESET_NETWORK_HDR(skb)    skb_reset_network_header(skb)
# define VBOX_SKB_RESET_MAC_HDR(skb)        skb_reset_mac_header(skb)
#else
# define VBOX_SKB_RESET_NETWORK_HDR(skb)    skb->nh.raw = skb->data
# define VBOX_SKB_RESET_MAC_HDR(skb)        skb->mac.raw = skb->data
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
# define VBOX_SKB_CHECKSUM_HELP(skb)        skb_checksum_help(skb)
#else
# define CHECKSUM_PARTIAL                   CHECKSUM_HW
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
#  define VBOX_SKB_CHECKSUM_HELP(skb)       skb_checksum_help(skb, 0)
# else
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 7)
#   define VBOX_SKB_CHECKSUM_HELP(skb)      skb_checksum_help(&skb, 0)
#  else
#   define VBOX_SKB_CHECKSUM_HELP(skb)      (!skb_checksum_help(skb))
#  endif
/* Versions prior 2.6.10 use stats for both bstats and qstats */
#  define bstats stats
#  define qstats stats
# endif
#endif

#ifndef NET_IP_ALIGN
# define NET_IP_ALIGN 2
#endif

#if 0
/** Create scatter / gather segments for fragments. When not used, we will
 *  linearize the socket buffer before creating the internal networking SG. */
# define VBOXNETFLT_SG_SUPPORT 1
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
/** Indicates that the linux kernel may send us GSO frames. */
# define VBOXNETFLT_WITH_GSO                1

/** This enables or disables the transmitting of GSO frame from the internal
 *  network and to the host.  */
# define VBOXNETFLT_WITH_GSO_XMIT_HOST      1

# if 0 /** @todo This is currently disable because it causes performance loss of 5-10%.  */
/** This enables or disables the transmitting of GSO frame from the internal
 *  network and to the wire. */
#  define VBOXNETFLT_WITH_GSO_XMIT_WIRE     1
# endif

/** This enables or disables the forwarding/flooding of GSO frame from the host
 *  to the internal network.  */
# define VBOXNETFLT_WITH_GSO_RECV           1

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
/** This enables or disables handling of GSO frames coming from the wire (GRO). */
# define VBOXNETFLT_WITH_GRO                1
#endif
/*
 * GRO support was backported to RHEL 5.4
 */
#ifdef RHEL_RELEASE_CODE
# if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(5, 4)
#  define VBOXNETFLT_WITH_GRO               1
# endif
#endif

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int      VBoxNetFltLinuxInit(void);
static void     VBoxNetFltLinuxUnload(void);
static void     vboxNetFltLinuxForwardToIntNet(PVBOXNETFLTINS pThis, struct sk_buff *pBuf);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * The (common) global data.
 */
static VBOXNETFLTGLOBALS g_VBoxNetFltGlobals;

module_init(VBoxNetFltLinuxInit);
module_exit(VBoxNetFltLinuxUnload);

MODULE_AUTHOR(VBOX_VENDOR);
MODULE_DESCRIPTION(VBOX_PRODUCT " Network Filter Driver");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING " (" RT_XSTR(INTNETTRUNKIFPORT_VERSION) ")");
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12) && defined(LOG_ENABLED)
unsigned dev_get_flags(const struct net_device *dev)
{
    unsigned flags;

    flags = (dev->flags & ~(IFF_PROMISC |
                            IFF_ALLMULTI |
                            IFF_RUNNING)) |
            (dev->gflags & (IFF_PROMISC |
                            IFF_ALLMULTI));

    if (netif_running(dev) && netif_carrier_ok(dev))
        flags |= IFF_RUNNING;

    return flags;
}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12) */


/**
 * Initialize module.
 *
 * @returns appropriate status code.
 */
static int __init VBoxNetFltLinuxInit(void)
{
    int rc;
    /*
     * Initialize IPRT.
     */
    rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        Log(("VBoxNetFltLinuxInit\n"));

        /*
         * Initialize the globals and connect to the support driver.
         *
         * This will call back vboxNetFltOsOpenSupDrv (and maybe vboxNetFltOsCloseSupDrv)
         * for establishing the connect to the support driver.
         */
        memset(&g_VBoxNetFltGlobals, 0, sizeof(g_VBoxNetFltGlobals));
        rc = vboxNetFltInitGlobalsAndIdc(&g_VBoxNetFltGlobals);
        if (RT_SUCCESS(rc))
        {
            LogRel(("VBoxNetFlt: Successfully started.\n"));
            return 0;
        }

        LogRel(("VBoxNetFlt: failed to initialize device extension (rc=%d)\n", rc));
        RTR0Term();
    }
    else
        LogRel(("VBoxNetFlt: failed to initialize IPRT (rc=%d)\n", rc));

    memset(&g_VBoxNetFltGlobals, 0, sizeof(g_VBoxNetFltGlobals));
    return -RTErrConvertToErrno(rc);
}


/**
 * Unload the module.
 *
 * @todo We have to prevent this if we're busy!
 */
static void __exit VBoxNetFltLinuxUnload(void)
{
    int rc;
    Log(("VBoxNetFltLinuxUnload\n"));
    Assert(vboxNetFltCanUnload(&g_VBoxNetFltGlobals));

    /*
     * Undo the work done during start (in reverse order).
     */
    rc = vboxNetFltTryDeleteIdcAndGlobals(&g_VBoxNetFltGlobals);
    AssertRC(rc); NOREF(rc);

    RTR0Term();

    memset(&g_VBoxNetFltGlobals, 0, sizeof(g_VBoxNetFltGlobals));

    Log(("VBoxNetFltLinuxUnload - done\n"));
}


/**
 * We filter traffic from the host to the internal network
 * before it reaches the NIC driver.
 *
 * The current code uses a very ugly hack overriding hard_start_xmit
 * callback in the device structure, but it has been shown to give us a
 * performance boost of 60-100% though. Eventually we have to find some
 * less hacky way of getting this job done.
 */
#define VBOXNETFLT_WITH_HOST2WIRE_FILTER

#ifdef VBOXNETFLT_WITH_HOST2WIRE_FILTER

# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)

# include <linux/ethtool.h>

typedef struct ethtool_ops OVR_OPSTYPE;
# define OVR_OPS  ethtool_ops
# define OVR_XMIT pfnStartXmit

# else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29) */

typedef struct net_device_ops OVR_OPSTYPE;
# define OVR_OPS  netdev_ops
# define OVR_XMIT pOrgOps->ndo_start_xmit

# endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29) */

/**
 * The overridden net_device_ops of the device we're attached to.
 *
 * As there is no net_device_ops structure in pre-2.6.29 kernels we override
 * ethtool_ops instead along with hard_start_xmit callback in net_device
 * structure.
 *
 * This is a very dirty hack that was created to explore how much we can improve
 * the host to guest transfers by not CC'ing the NIC. It turns out to be
 * the only way to filter outgoing packets for devices without TX queue.
 */
typedef struct VBoxNetDeviceOpsOverride
{
    /** Our overridden ops. */
    OVR_OPSTYPE                     Ops;
    /** Magic word. */
    uint32_t                        u32Magic;
    /** Pointer to the original ops. */
    OVR_OPSTYPE const              *pOrgOps;
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
    /** Pointer to the original hard_start_xmit function. */
    int (*pfnStartXmit)(struct sk_buff *pSkb, struct net_device *pDev);
# endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29) */
    /** Pointer to the net filter instance. */
    PVBOXNETFLTINS                  pVBoxNetFlt;
    /** The number of filtered packages. */
    uint64_t                        cFiltered;
    /** The total number of packets */
    uint64_t                        cTotal;
} VBOXNETDEVICEOPSOVERRIDE, *PVBOXNETDEVICEOPSOVERRIDE;
/** VBOXNETDEVICEOPSOVERRIDE::u32Magic value. */
#define VBOXNETDEVICEOPSOVERRIDE_MAGIC  UINT32_C(0x00c0ffee)

/**
 * ndo_start_xmit wrapper that drops packets that shouldn't go to the wire
 * because they belong on the internal network.
 *
 * @returns NETDEV_TX_XXX.
 * @param   pSkb                The socket buffer to transmit.
 * @param   pDev                The net device.
 */
static int vboxNetFltLinuxStartXmitFilter(struct sk_buff *pSkb, struct net_device *pDev)
{
    PVBOXNETDEVICEOPSOVERRIDE   pOverride = (PVBOXNETDEVICEOPSOVERRIDE)pDev->OVR_OPS;
    uint8_t                     abHdrBuf[sizeof(RTNETETHERHDR) + sizeof(uint32_t) + RTNETIPV4_MIN_LEN];
    PCRTNETETHERHDR             pEtherHdr;
    PINTNETTRUNKSWPORT          pSwitchPort;
    uint32_t                    cbHdrs;


    /*
     * Validate the override structure.
     *
     * Note! We're racing vboxNetFltLinuxUnhookDev here.  If this was supposed
     *       to be production quality code, we would have to be much more
     *       careful here and avoid the race.
     */
    if (   !VALID_PTR(pOverride)
        || pOverride->u32Magic != VBOXNETDEVICEOPSOVERRIDE_MAGIC
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
        || !VALID_PTR(pOverride->pOrgOps)
# endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29) */
        )
    {
        printk("vboxNetFltLinuxStartXmitFilter: bad override %p\n", pOverride);
        dev_kfree_skb(pSkb);
        return NETDEV_TX_OK;
    }
    pOverride->cTotal++;

    /*
     * Do the filtering base on the default OUI of our virtual NICs
     *
     * Note! In a real solution, we would ask the switch whether the
     *       destination MAC is 100% to be on the internal network and then
     *       drop it.
     */
    cbHdrs = skb_headlen(pSkb);
    cbHdrs = RT_MIN(cbHdrs, sizeof(abHdrBuf));
    pEtherHdr = (PCRTNETETHERHDR)skb_header_pointer(pSkb, 0, cbHdrs, &abHdrBuf[0]);
    if (   pEtherHdr
        && VALID_PTR(pOverride->pVBoxNetFlt)
        && (pSwitchPort = pOverride->pVBoxNetFlt->pSwitchPort) != NULL
        && VALID_PTR(pSwitchPort)
        && cbHdrs >= 6)
    {
        INTNETSWDECISION enmDecision;

        /** @todo consider reference counting, etc. */
        enmDecision = pSwitchPort->pfnPreRecv(pSwitchPort, pEtherHdr, cbHdrs, INTNETTRUNKDIR_HOST);
        if (enmDecision == INTNETSWDECISION_INTNET)
        {
            dev_kfree_skb(pSkb);
            pOverride->cFiltered++;
            return NETDEV_TX_OK;
        }
    }

    return pOverride->OVR_XMIT(pSkb, pDev);
}

/**
 * Hooks the device ndo_start_xmit operation of the device.
 *
 * @param   pThis               The net filter instance.
 * @param   pDev                The net device.
 */
static void vboxNetFltLinuxHookDev(PVBOXNETFLTINS pThis, struct net_device *pDev)
{
    PVBOXNETDEVICEOPSOVERRIDE   pOverride;

    /* Cancel override if ethtool_ops is missing (host-only case, @bugref{5712}) */
    if (!VALID_PTR(pDev->OVR_OPS))
        return;
    pOverride = RTMemAlloc(sizeof(*pOverride));
    if (!pOverride)
        return;
    pOverride->pOrgOps              = pDev->OVR_OPS;
    pOverride->Ops                  = *pDev->OVR_OPS;
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
    pOverride->pfnStartXmit         = pDev->hard_start_xmit;
# else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29) */
    pOverride->Ops.ndo_start_xmit   = vboxNetFltLinuxStartXmitFilter;
# endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29) */
    pOverride->u32Magic             = VBOXNETDEVICEOPSOVERRIDE_MAGIC;
    pOverride->cTotal               = 0;
    pOverride->cFiltered            = 0;
    pOverride->pVBoxNetFlt          = pThis;

    RTSpinlockAcquire(pThis->hSpinlock); /* (this isn't necessary, but so what) */
    ASMAtomicWritePtr((void * volatile *)&pDev->OVR_OPS, pOverride);
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
    ASMAtomicXchgPtr((void * volatile *)&pDev->hard_start_xmit, vboxNetFltLinuxStartXmitFilter);
# endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29) */
    RTSpinlockReleaseNoInts(pThis->hSpinlock);
}

/**
 * Undos what vboxNetFltLinuxHookDev did.
 *
 * @param   pThis               The net filter instance.
 * @param   pDev                The net device.  Can be NULL, in which case
 *                              we'll try retrieve it from @a pThis.
 */
static void vboxNetFltLinuxUnhookDev(PVBOXNETFLTINS pThis, struct net_device *pDev)
{
    PVBOXNETDEVICEOPSOVERRIDE   pOverride;

    RTSpinlockAcquire(pThis->hSpinlock);
    if (!pDev)
        pDev = ASMAtomicUoReadPtrT(&pThis->u.s.pDev, struct net_device *);
    if (VALID_PTR(pDev))
    {
        pOverride = (PVBOXNETDEVICEOPSOVERRIDE)pDev->OVR_OPS;
        if (    VALID_PTR(pOverride)
            &&  pOverride->u32Magic == VBOXNETDEVICEOPSOVERRIDE_MAGIC
            &&  VALID_PTR(pOverride->pOrgOps)
           )
        {
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
            ASMAtomicWritePtr((void * volatile *)&pDev->hard_start_xmit, pOverride->pfnStartXmit);
# endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29) */
            ASMAtomicWritePtr((void const * volatile *)&pDev->OVR_OPS, pOverride->pOrgOps);
            ASMAtomicWriteU32(&pOverride->u32Magic, 0);
        }
        else
            pOverride = NULL;
    }
    else
        pOverride = NULL;
    RTSpinlockReleaseNoInts(pThis->hSpinlock);

    if (pOverride)
    {
        printk("vboxnetflt: dropped %llu out of %llu packets\n", pOverride->cFiltered, pOverride->cTotal);
        RTMemFree(pOverride);
    }
}

#endif /* VBOXNETFLT_WITH_HOST2WIRE_FILTER */


/**
 * Reads and retains the host interface handle.
 *
 * @returns The handle, NULL if detached.
 * @param   pThis
 */
DECLINLINE(struct net_device *) vboxNetFltLinuxRetainNetDev(PVBOXNETFLTINS pThis)
{
#if 0
    struct net_device *pDev = NULL;

    Log(("vboxNetFltLinuxRetainNetDev\n"));
    /*
     * Be careful here to avoid problems racing the detached callback.
     */
    RTSpinlockAcquire(pThis->hSpinlock);
    if (!ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost))
    {
        pDev = (struct net_device *)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pDev);
        if (pDev)
        {
            dev_hold(pDev);
            Log(("vboxNetFltLinuxRetainNetDev: Device %p(%s) retained. ref=%d\n",
                 pDev, pDev->name,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
                 netdev_refcnt_read(pDev)
#else
                 atomic_read(&pDev->refcnt)
#endif
                 ));
        }
    }
    RTSpinlockRelease(pThis->hSpinlock);

    Log(("vboxNetFltLinuxRetainNetDev - done\n"));
    return pDev;
#else
    return ASMAtomicUoReadPtrT(&pThis->u.s.pDev, struct net_device *);
#endif
}


/**
 * Release the host interface handle previously retained
 * by vboxNetFltLinuxRetainNetDev.
 *
 * @param   pThis           The instance.
 * @param   pDev            The vboxNetFltLinuxRetainNetDev
 *                          return value, NULL is fine.
 */
DECLINLINE(void) vboxNetFltLinuxReleaseNetDev(PVBOXNETFLTINS pThis, struct net_device *pDev)
{
#if 0
    Log(("vboxNetFltLinuxReleaseNetDev\n"));
    NOREF(pThis);
    if (pDev)
    {
        dev_put(pDev);
        Log(("vboxNetFltLinuxReleaseNetDev: Device %p(%s) released. ref=%d\n",
             pDev, pDev->name,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
             netdev_refcnt_read(pDev)
#else
             atomic_read(&pDev->refcnt)
#endif
             ));
    }
    Log(("vboxNetFltLinuxReleaseNetDev - done\n"));
#endif
}

#define VBOXNETFLT_CB_TAG(skb) (0xA1C90000 | (skb->dev->ifindex & 0xFFFF))
#define VBOXNETFLT_SKB_TAG(skb) (*(uint32_t*)&((skb)->cb[sizeof((skb)->cb)-sizeof(uint32_t)]))

/**
 * Checks whether this is an mbuf created by vboxNetFltLinuxMBufFromSG,
 * i.e. a buffer which we're pushing and should be ignored by the filter callbacks.
 *
 * @returns true / false accordingly.
 * @param   pBuf            The sk_buff.
 */
DECLINLINE(bool) vboxNetFltLinuxSkBufIsOur(struct sk_buff *pBuf)
{
    return VBOXNETFLT_SKB_TAG(pBuf) == VBOXNETFLT_CB_TAG(pBuf);
}


/**
 * Internal worker that create a linux sk_buff for a
 * (scatter/)gather list.
 *
 * @returns Pointer to the sk_buff.
 * @param   pThis           The instance.
 * @param   pSG             The (scatter/)gather list.
 * @param   fDstWire        Set if the destination is the wire.
 */
static struct sk_buff *vboxNetFltLinuxSkBufFromSG(PVBOXNETFLTINS pThis, PINTNETSG pSG, bool fDstWire)
{
    struct sk_buff *pPkt;
    struct net_device *pDev;
    unsigned fGsoType = 0;

    if (pSG->cbTotal == 0)
    {
        LogRel(("VBoxNetFlt: Dropped empty packet coming from internal network.\n"));
        return NULL;
    }

    /** @todo We should use fragments mapping the SG buffers with large packets.
     *        256 bytes seems to be the a threshold used a lot for this.  It
     *        requires some nasty work on the intnet side though...  */
    /*
     * Allocate a packet and copy over the data.
     */
    pDev = ASMAtomicUoReadPtrT(&pThis->u.s.pDev, struct net_device *);
    pPkt = dev_alloc_skb(pSG->cbTotal + NET_IP_ALIGN);
    if (RT_UNLIKELY(!pPkt))
    {
        Log(("vboxNetFltLinuxSkBufFromSG: Failed to allocate sk_buff(%u).\n", pSG->cbTotal));
        pSG->pvUserData = NULL;
        return NULL;
    }
    pPkt->dev       = pDev;
    pPkt->ip_summed = CHECKSUM_NONE;

    /* Align IP header on 16-byte boundary: 2 + 14 (ethernet hdr size). */
    skb_reserve(pPkt, NET_IP_ALIGN);

    /* Copy the segments. */
    skb_put(pPkt, pSG->cbTotal);
    IntNetSgRead(pSG, pPkt->data);

#if defined(VBOXNETFLT_WITH_GSO_XMIT_WIRE) || defined(VBOXNETFLT_WITH_GSO_XMIT_HOST)
    /*
     * Setup GSO if used by this packet.
     */
    switch ((PDMNETWORKGSOTYPE)pSG->GsoCtx.u8Type)
    {
        default:
            AssertMsgFailed(("%u (%s)\n", pSG->GsoCtx.u8Type, PDMNetGsoTypeName((PDMNETWORKGSOTYPE)pSG->GsoCtx.u8Type) ));
            /* fall thru */
        case PDMNETWORKGSOTYPE_INVALID:
            fGsoType = 0;
            break;
        case PDMNETWORKGSOTYPE_IPV4_TCP:
            fGsoType = SKB_GSO_TCPV4;
            break;
        case PDMNETWORKGSOTYPE_IPV4_UDP:
            fGsoType = SKB_GSO_UDP;
            break;
        case PDMNETWORKGSOTYPE_IPV6_TCP:
            fGsoType = SKB_GSO_TCPV6;
            break;
    }
    if (fGsoType)
    {
        struct skb_shared_info *pShInfo = skb_shinfo(pPkt);

        pShInfo->gso_type = fGsoType | SKB_GSO_DODGY;
        pShInfo->gso_size = pSG->GsoCtx.cbMaxSeg;
        pShInfo->gso_segs = PDMNetGsoCalcSegmentCount(&pSG->GsoCtx, pSG->cbTotal);

        /*
         * We need to set checksum fields even if the packet goes to the host
         * directly as it may be immediately forwarded by IP layer @bugref{5020}.
         */
        Assert(skb_headlen(pPkt) >= pSG->GsoCtx.cbHdrsTotal);
        pPkt->ip_summed  = CHECKSUM_PARTIAL;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
        pPkt->csum_start = skb_headroom(pPkt) + pSG->GsoCtx.offHdr2;
        if (fGsoType & (SKB_GSO_TCPV4 | SKB_GSO_TCPV6))
            pPkt->csum_offset = RT_OFFSETOF(RTNETTCP, th_sum);
        else
            pPkt->csum_offset = RT_OFFSETOF(RTNETUDP, uh_sum);
# else
        pPkt->h.raw = pPkt->data + pSG->GsoCtx.offHdr2;
        if (fGsoType & (SKB_GSO_TCPV4 | SKB_GSO_TCPV6))
            pPkt->csum = RT_OFFSETOF(RTNETTCP, th_sum);
        else
            pPkt->csum = RT_OFFSETOF(RTNETUDP, uh_sum);
# endif
        if (!fDstWire)
            PDMNetGsoPrepForDirectUse(&pSG->GsoCtx, pPkt->data, pSG->cbTotal, PDMNETCSUMTYPE_PSEUDO);
    }
#endif /* VBOXNETFLT_WITH_GSO_XMIT_WIRE || VBOXNETFLT_WITH_GSO_XMIT_HOST */

    /*
     * Finish up the socket buffer.
     */
    pPkt->protocol = eth_type_trans(pPkt, pDev);
    if (fDstWire)
    {
        VBOX_SKB_RESET_NETWORK_HDR(pPkt);

        /* Restore ethernet header back. */
        skb_push(pPkt, ETH_HLEN); /** @todo VLAN: +4 if VLAN? */
        VBOX_SKB_RESET_MAC_HDR(pPkt);
    }
    VBOXNETFLT_SKB_TAG(pPkt) = VBOXNETFLT_CB_TAG(pPkt);

    return pPkt;
}


/**
 * Initializes a SG list from an sk_buff.
 *
 * @returns Number of segments.
 * @param   pThis               The instance.
 * @param   pBuf                The sk_buff.
 * @param   pSG                 The SG.
 * @param   pvFrame             The frame pointer, optional.
 * @param   cSegs               The number of segments allocated for the SG.
 *                              This should match the number in the mbuf exactly!
 * @param   fSrc                The source of the frame.
 * @param   pGso                Pointer to the GSO context if it's a GSO
 *                              internal network frame.  NULL if regular frame.
 */
DECLINLINE(void) vboxNetFltLinuxSkBufToSG(PVBOXNETFLTINS pThis, struct sk_buff *pBuf, PINTNETSG pSG,
                                          unsigned cSegs, uint32_t fSrc, PCPDMNETWORKGSO pGsoCtx)
{
    int i;
    NOREF(pThis);

    Assert(!skb_shinfo(pBuf)->frag_list);

    if (!pGsoCtx)
        IntNetSgInitTempSegs(pSG, pBuf->len, cSegs, 0 /*cSegsUsed*/);
    else
        IntNetSgInitTempSegsGso(pSG, pBuf->len, cSegs, 0 /*cSegsUsed*/, pGsoCtx);

#ifdef VBOXNETFLT_SG_SUPPORT
    pSG->aSegs[0].cb = skb_headlen(pBuf);
    pSG->aSegs[0].pv = pBuf->data;
    pSG->aSegs[0].Phys = NIL_RTHCPHYS;

    for (i = 0; i < skb_shinfo(pBuf)->nr_frags; i++)
    {
        skb_frag_t *pFrag = &skb_shinfo(pBuf)->frags[i];
        pSG->aSegs[i+1].cb = pFrag->size;
        pSG->aSegs[i+1].pv = kmap(pFrag->page);
        printk("%p = kmap()\n", pSG->aSegs[i+1].pv);
        pSG->aSegs[i+1].Phys = NIL_RTHCPHYS;
    }
    ++i;

#else
    pSG->aSegs[0].cb = pBuf->len;
    pSG->aSegs[0].pv = pBuf->data;
    pSG->aSegs[0].Phys = NIL_RTHCPHYS;
    i = 1;
#endif

    pSG->cSegsUsed = i;

#ifdef PADD_RUNT_FRAMES_FROM_HOST
    /*
     * Add a trailer if the frame is too small.
     *
     * Since we're getting to the packet before it is framed, it has not
     * yet been padded. The current solution is to add a segment pointing
     * to a buffer containing all zeros and pray that works for all frames...
     */
    if (pSG->cbTotal < 60 && (fSrc & INTNETTRUNKDIR_HOST))
    {
        static uint8_t const s_abZero[128] = {0};

        AssertReturnVoid(i < cSegs);

        pSG->aSegs[i].Phys = NIL_RTHCPHYS;
        pSG->aSegs[i].pv = (void *)&s_abZero[0];
        pSG->aSegs[i].cb = 60 - pSG->cbTotal;
        pSG->cbTotal = 60;
        pSG->cSegsUsed++;
        Assert(i + 1 <= pSG->cSegsAlloc)
    }
#endif

    Log4(("vboxNetFltLinuxSkBufToSG: allocated=%d, segments=%d frags=%d next=%p frag_list=%p pkt_type=%x fSrc=%x\n",
          pSG->cSegsAlloc, pSG->cSegsUsed, skb_shinfo(pBuf)->nr_frags, pBuf->next, skb_shinfo(pBuf)->frag_list, pBuf->pkt_type, fSrc));
    for (i = 0; i < pSG->cSegsUsed; i++)
        Log4(("vboxNetFltLinuxSkBufToSG:   #%d: cb=%d pv=%p\n",
              i, pSG->aSegs[i].cb, pSG->aSegs[i].pv));
}

/**
 * Packet handler,
 *
 * @returns 0 or EJUSTRETURN.
 * @param   pThis           The instance.
 * @param   pMBuf           The mbuf.
 * @param   pvFrame         The start of the frame, optional.
 * @param   fSrc            Where the packet (allegedly) comes from, one INTNETTRUNKDIR_* value.
 * @param   eProtocol       The protocol.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
static int vboxNetFltLinuxPacketHandler(struct sk_buff *pBuf,
                                        struct net_device *pSkbDev,
                                        struct packet_type *pPacketType,
                                        struct net_device *pOrigDev)
#else
static int vboxNetFltLinuxPacketHandler(struct sk_buff *pBuf,
                                        struct net_device *pSkbDev,
                                        struct packet_type *pPacketType)
#endif
{
    PVBOXNETFLTINS pThis;
    struct net_device *pDev;
    LogFlow(("vboxNetFltLinuxPacketHandler: pBuf=%p pSkbDev=%p pPacketType=%p\n",
             pBuf, pSkbDev, pPacketType));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
    Log3(("vboxNetFltLinuxPacketHandler: skb len=%u data_len=%u truesize=%u next=%p nr_frags=%u gso_size=%u gso_seqs=%u gso_type=%x frag_list=%p pkt_type=%x\n",
          pBuf->len, pBuf->data_len, pBuf->truesize, pBuf->next, skb_shinfo(pBuf)->nr_frags, skb_shinfo(pBuf)->gso_size, skb_shinfo(pBuf)->gso_segs, skb_shinfo(pBuf)->gso_type, skb_shinfo(pBuf)->frag_list, pBuf->pkt_type));
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
    Log4(("vboxNetFltLinuxPacketHandler: packet dump follows:\n%.*Rhxd\n", pBuf->len-pBuf->data_len, skb_mac_header(pBuf)));
# endif
#else
    Log3(("vboxNetFltLinuxPacketHandler: skb len=%u data_len=%u truesize=%u next=%p nr_frags=%u tso_size=%u tso_seqs=%u frag_list=%p pkt_type=%x\n",
          pBuf->len, pBuf->data_len, pBuf->truesize, pBuf->next, skb_shinfo(pBuf)->nr_frags, skb_shinfo(pBuf)->tso_size, skb_shinfo(pBuf)->tso_segs, skb_shinfo(pBuf)->frag_list, pBuf->pkt_type));
#endif
    /*
     * Drop it immediately?
     */
    if (!pBuf)
        return 0;

    if (pBuf->pkt_type == PACKET_LOOPBACK)
    {
        /*
         * We are not interested in loopbacked packets as they will always have
         * another copy going to the wire.
         */
        Log2(("vboxNetFltLinuxPacketHandler: dropped loopback packet (cb=%u)\n", pBuf->len));
        return 0;
    }

    pThis = VBOX_FLT_PT_TO_INST(pPacketType);
    pDev = ASMAtomicUoReadPtrT(&pThis->u.s.pDev, struct net_device *);
    if (pDev != pSkbDev)
    {
        Log(("vboxNetFltLinuxPacketHandler: Devices do not match, pThis may be wrong! pThis=%p\n", pThis));
        return 0;
    }

    Log4(("vboxNetFltLinuxPacketHandler: pBuf->cb dump:\n%.*Rhxd\n", sizeof(pBuf->cb), pBuf->cb));
    if (vboxNetFltLinuxSkBufIsOur(pBuf))
    {
        Log2(("vboxNetFltLinuxPacketHandler: got our own sk_buff, drop it.\n"));
        dev_kfree_skb(pBuf);
        return 0;
    }

#ifndef VBOXNETFLT_SG_SUPPORT
    {
        /*
         * Get rid of fragmented packets, they cause too much trouble.
         */
        unsigned int uMacLen = pBuf->mac_len;
        struct sk_buff *pCopy = skb_copy(pBuf, GFP_ATOMIC);
        kfree_skb(pBuf);
        if (!pCopy)
        {
            LogRel(("VBoxNetFlt: Failed to allocate packet buffer, dropping the packet.\n"));
            return 0;
        }
        pBuf = pCopy;
        /* Somehow skb_copy ignores mac_len */
        pBuf->mac_len = uMacLen;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
        /* Restore VLAN tag stripped by host hardware */
        if (vlan_tx_tag_present(pBuf) && skb_headroom(pBuf) >= VLAN_ETH_HLEN)
        {
            uint8_t *pMac = (uint8_t*)skb_mac_header(pBuf);
            struct vlan_ethhdr *pVHdr = (struct vlan_ethhdr *)(pMac - VLAN_HLEN);
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
            memmove(pVHdr, pMac, ETH_ALEN * 2);
#  else
            memmove(pVHdr, pMac, VLAN_ETH_ALEN * 2);
#  endif
            pVHdr->h_vlan_proto = RT_H2N_U16(ETH_P_8021Q);
            pVHdr->h_vlan_TCI   = RT_H2N_U16(vlan_tx_tag_get(pBuf));
            pBuf->mac_header   -= VLAN_HLEN;
            pBuf->mac_len      += VLAN_HLEN;
        }
# endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) */

# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
        Log3(("vboxNetFltLinuxPacketHandler: skb copy len=%u data_len=%u truesize=%u next=%p nr_frags=%u gso_size=%u gso_seqs=%u gso_type=%x frag_list=%p pkt_type=%x\n",
              pBuf->len, pBuf->data_len, pBuf->truesize, pBuf->next, skb_shinfo(pBuf)->nr_frags, skb_shinfo(pBuf)->gso_size, skb_shinfo(pBuf)->gso_segs, skb_shinfo(pBuf)->gso_type, skb_shinfo(pBuf)->frag_list, pBuf->pkt_type));
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
        Log4(("vboxNetFltLinuxPacketHandler: packet dump follows:\n%.*Rhxd\n", pBuf->len-pBuf->data_len, skb_mac_header(pBuf)));
#  endif
# else
        Log3(("vboxNetFltLinuxPacketHandler: skb copy len=%u data_len=%u truesize=%u next=%p nr_frags=%u tso_size=%u tso_seqs=%u frag_list=%p pkt_type=%x\n",
              pBuf->len, pBuf->data_len, pBuf->truesize, pBuf->next, skb_shinfo(pBuf)->nr_frags, skb_shinfo(pBuf)->tso_size, skb_shinfo(pBuf)->tso_segs, skb_shinfo(pBuf)->frag_list, pBuf->pkt_type));
# endif
    }
#endif

#ifdef VBOXNETFLT_LINUX_NO_XMIT_QUEUE
    /* Forward it to the internal network. */
    vboxNetFltLinuxForwardToIntNet(pThis, pBuf);
#else
    /* Add the packet to transmit queue and schedule the bottom half. */
    skb_queue_tail(&pThis->u.s.XmitQueue, pBuf);
    schedule_work(&pThis->u.s.XmitTask);
    Log4(("vboxNetFltLinuxPacketHandler: scheduled work %p for sk_buff %p\n",
          &pThis->u.s.XmitTask, pBuf));
#endif

    /* It does not really matter what we return, it is ignored by the kernel. */
    return 0;
}

/**
 * Calculate the number of INTNETSEG segments the socket buffer will need.
 *
 * @returns Segment count.
 * @param   pBuf                The socket buffer.
 */
DECLINLINE(unsigned) vboxNetFltLinuxCalcSGSegments(struct sk_buff *pBuf)
{
#ifdef VBOXNETFLT_SG_SUPPORT
    unsigned cSegs = 1 + skb_shinfo(pBuf)->nr_frags;
#else
    unsigned cSegs = 1;
#endif
#ifdef PADD_RUNT_FRAMES_FROM_HOST
    /* vboxNetFltLinuxSkBufToSG adds a padding segment if it's a runt. */
    if (pBuf->len < 60)
        cSegs++;
#endif
    return cSegs;
}

/**
 * Destroy the intnet scatter / gather buffer created by
 * vboxNetFltLinuxSkBufToSG.
 */
static void vboxNetFltLinuxDestroySG(PINTNETSG pSG)
{
#ifdef VBOXNETFLT_SG_SUPPORT
    int i;

    for (i = 0; i < skb_shinfo(pBuf)->nr_frags; i++)
    {
        printk("kunmap(%p)\n", pSG->aSegs[i+1].pv);
        kunmap(pSG->aSegs[i+1].pv);
    }
#endif
    NOREF(pSG);
}

#ifdef LOG_ENABLED
/**
 * Logging helper.
 */
static void vboxNetFltDumpPacket(PINTNETSG pSG, bool fEgress, const char *pszWhere, int iIncrement)
{
    int i, offSeg;
    uint8_t *pInt, *pExt;
    static int iPacketNo = 1;
    iPacketNo += iIncrement;
    if (fEgress)
    {
        pExt = pSG->aSegs[0].pv;
        pInt = pExt + 6;
    }
    else
    {
        pInt = pSG->aSegs[0].pv;
        pExt = pInt + 6;
    }
    Log(("VBoxNetFlt: (int)%02x:%02x:%02x:%02x:%02x:%02x"
         " %s (%s)%02x:%02x:%02x:%02x:%02x:%02x (%u bytes) packet #%u\n",
         pInt[0], pInt[1], pInt[2], pInt[3], pInt[4], pInt[5],
         fEgress ? "-->" : "<--", pszWhere,
         pExt[0], pExt[1], pExt[2], pExt[3], pExt[4], pExt[5],
         pSG->cbTotal, iPacketNo));
    if (pSG->cSegsUsed == 1)
    {
        Log3(("%.*Rhxd\n", pSG->aSegs[0].cb, pSG->aSegs[0].pv));
    }
    else
    {
        for (i = 0, offSeg = 0; i < pSG->cSegsUsed; i++)
        {
            Log3(("-- segment %d at 0x%x (%d bytes) --\n%.*Rhxd\n",
                  i, offSeg, pSG->aSegs[i].cb, pSG->aSegs[i].cb, pSG->aSegs[i].pv));
            offSeg += pSG->aSegs[i].cb;
        }
    }

}
#else
# define vboxNetFltDumpPacket(a, b, c, d) do {} while (0)
#endif

#ifdef VBOXNETFLT_WITH_GSO_RECV

/**
 * Worker for vboxNetFltLinuxForwardToIntNet that checks if we can forwards a
 * GSO socket buffer without having to segment it.
 *
 * @returns true on success, false if needs segmenting.
 * @param   pThis               The net filter instance.
 * @param   pSkb                The GSO socket buffer.
 * @param   fSrc                The source.
 * @param   pGsoCtx             Where to return the GSO context on success.
 */
static bool vboxNetFltLinuxCanForwardAsGso(PVBOXNETFLTINS pThis, struct sk_buff *pSkb, uint32_t fSrc,
                                           PPDMNETWORKGSO pGsoCtx)
{
    PDMNETWORKGSOTYPE   enmGsoType;
    uint16_t            uEtherType;
    unsigned int        cbTransport;
    unsigned int        offTransport;
    unsigned int        cbTransportHdr;
    unsigned            uProtocol;
    union
    {
        RTNETIPV4           IPv4;
        RTNETIPV6           IPv6;
        RTNETTCP            Tcp;
        uint8_t             ab[40];
        uint16_t            au16[40/2];
        uint32_t            au32[40/4];
    }                   Buf;

    /*
     * Check the GSO properties of the socket buffer and make sure it fits.
     */
    /** @todo Figure out how to handle SKB_GSO_TCP_ECN! */
    if (RT_UNLIKELY( skb_shinfo(pSkb)->gso_type & ~(SKB_GSO_UDP | SKB_GSO_DODGY | SKB_GSO_TCPV6 | SKB_GSO_TCPV4) ))
    {
        Log5(("vboxNetFltLinuxCanForwardAsGso: gso_type=%#x\n", skb_shinfo(pSkb)->gso_type));
        return false;
    }
    if (RT_UNLIKELY(   skb_shinfo(pSkb)->gso_size < 1
                    || pSkb->len > VBOX_MAX_GSO_SIZE ))
    {
        Log5(("vboxNetFltLinuxCanForwardAsGso: gso_size=%#x skb_len=%#x (max=%#x)\n", skb_shinfo(pSkb)->gso_size, pSkb->len, VBOX_MAX_GSO_SIZE));
        return false;
    }
    /*
     * It is possible to receive GSO packets from wire if GRO is enabled.
     */
    if (RT_UNLIKELY(fSrc & INTNETTRUNKDIR_WIRE))
    {
        Log5(("vboxNetFltLinuxCanForwardAsGso: fSrc=wire\n"));
#ifdef VBOXNETFLT_WITH_GRO
        /*
         * The packet came from the wire and the driver has already consumed
         * mac header. We need to restore it back.
         */
        pSkb->mac_len = skb_network_header(pSkb) - skb_mac_header(pSkb);
        skb_push(pSkb, pSkb->mac_len);
        Log5(("vboxNetFltLinuxCanForwardAsGso: mac_len=%d data=%p mac_header=%p network_header=%p\n",
              pSkb->mac_len, pSkb->data, skb_mac_header(pSkb), skb_network_header(pSkb)));
#else /* !VBOXNETFLT_WITH_GRO */
        /* Older kernels didn't have GRO. */
        return false;
#endif /* !VBOXNETFLT_WITH_GRO */
    }
    else
    {
        /*
         * skb_gso_segment does the following. Do we need to do it as well?
         */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
        skb_reset_mac_header(pSkb);
        pSkb->mac_len = pSkb->network_header - pSkb->mac_header;
#else
        pSkb->mac.raw = pSkb->data;
        pSkb->mac_len = pSkb->nh.raw - pSkb->data;
#endif
    }

    /*
     * Switch on the ethertype.
     */
    uEtherType = pSkb->protocol;
    if (   uEtherType    == RT_H2N_U16_C(RTNET_ETHERTYPE_VLAN)
        && pSkb->mac_len == sizeof(RTNETETHERHDR) + sizeof(uint32_t))
    {
        uint16_t const *puEtherType = skb_header_pointer(pSkb, sizeof(RTNETETHERHDR) + sizeof(uint16_t), sizeof(uint16_t), &Buf);
        if (puEtherType)
            uEtherType = *puEtherType;
    }
    switch (uEtherType)
    {
        case RT_H2N_U16_C(RTNET_ETHERTYPE_IPV4):
        {
            unsigned int cbHdr;
            PCRTNETIPV4  pIPv4 = (PCRTNETIPV4)skb_header_pointer(pSkb, pSkb->mac_len, sizeof(Buf.IPv4), &Buf);
            if (RT_UNLIKELY(!pIPv4))
            {
                Log5(("vboxNetFltLinuxCanForwardAsGso: failed to access IPv4 hdr\n"));
                return false;
            }

            cbHdr       = pIPv4->ip_hl * 4;
            cbTransport = RT_N2H_U16(pIPv4->ip_len);
            if (RT_UNLIKELY(   cbHdr < RTNETIPV4_MIN_LEN
                            || cbHdr > cbTransport ))
            {
                Log5(("vboxNetFltLinuxCanForwardAsGso: invalid IPv4 lengths: ip_hl=%u ip_len=%u\n", pIPv4->ip_hl, RT_N2H_U16(pIPv4->ip_len)));
                return false;
            }
            cbTransport -= cbHdr;
            offTransport = pSkb->mac_len + cbHdr;
            uProtocol    = pIPv4->ip_p;
            if (uProtocol == RTNETIPV4_PROT_TCP)
                enmGsoType = PDMNETWORKGSOTYPE_IPV4_TCP;
            else if (uProtocol == RTNETIPV4_PROT_UDP)
                enmGsoType = PDMNETWORKGSOTYPE_IPV4_UDP;
            else /** @todo IPv6: 4to6 tunneling */
                enmGsoType = PDMNETWORKGSOTYPE_INVALID;
            break;
        }

        case RT_H2N_U16_C(RTNET_ETHERTYPE_IPV6):
        {
            PCRTNETIPV6 pIPv6 = (PCRTNETIPV6)skb_header_pointer(pSkb, pSkb->mac_len, sizeof(Buf.IPv6), &Buf);
            if (RT_UNLIKELY(!pIPv6))
            {
                Log5(("vboxNetFltLinuxCanForwardAsGso: failed to access IPv6 hdr\n"));
                return false;
            }

            cbTransport  = RT_N2H_U16(pIPv6->ip6_plen);
            offTransport = pSkb->mac_len + sizeof(RTNETIPV6);
            uProtocol    = pIPv6->ip6_nxt;
            /** @todo IPv6: Dig our way out of the other headers. */
            if (uProtocol == RTNETIPV4_PROT_TCP)
                enmGsoType = PDMNETWORKGSOTYPE_IPV6_TCP;
            else if (uProtocol == RTNETIPV4_PROT_UDP)
                enmGsoType = PDMNETWORKGSOTYPE_IPV4_UDP;
            else
                enmGsoType = PDMNETWORKGSOTYPE_INVALID;
            break;
        }

        default:
            Log5(("vboxNetFltLinuxCanForwardAsGso: uEtherType=%#x\n", RT_H2N_U16(uEtherType)));
            return false;
    }

    if (enmGsoType == PDMNETWORKGSOTYPE_INVALID)
    {
        Log5(("vboxNetFltLinuxCanForwardAsGso: Unsupported protocol %d\n", uProtocol));
        return false;
    }

    if (RT_UNLIKELY(   offTransport + cbTransport <= offTransport
                    || offTransport + cbTransport > pSkb->len
                    || cbTransport < (uProtocol == RTNETIPV4_PROT_TCP ? RTNETTCP_MIN_LEN : RTNETUDP_MIN_LEN)) )
    {
        Log5(("vboxNetFltLinuxCanForwardAsGso: Bad transport length; off=%#x + cb=%#x => %#x; skb_len=%#x (%s)\n",
              offTransport, cbTransport, offTransport + cbTransport, pSkb->len, PDMNetGsoTypeName(enmGsoType) ));
        return false;
    }

    /*
     * Check the TCP/UDP bits.
     */
    if (uProtocol == RTNETIPV4_PROT_TCP)
    {
        PCRTNETTCP pTcp = (PCRTNETTCP)skb_header_pointer(pSkb, offTransport, sizeof(Buf.Tcp), &Buf);
        if (RT_UNLIKELY(!pTcp))
        {
            Log5(("vboxNetFltLinuxCanForwardAsGso: failed to access TCP hdr\n"));
            return false;
        }

        cbTransportHdr = pTcp->th_off * 4;
        pGsoCtx->cbHdrsSeg = offTransport + cbTransportHdr;
        if (RT_UNLIKELY(   cbTransportHdr < RTNETTCP_MIN_LEN
                        || cbTransportHdr > cbTransport
                        || offTransport + cbTransportHdr >= UINT8_MAX
                        || offTransport + cbTransportHdr >= pSkb->len ))
        {
            Log5(("vboxNetFltLinuxCanForwardAsGso: No space for TCP header; off=%#x cb=%#x skb_len=%#x\n", offTransport, cbTransportHdr, pSkb->len));
            return false;
        }

    }
    else
    {
        Assert(uProtocol == RTNETIPV4_PROT_UDP);
        cbTransportHdr = sizeof(RTNETUDP);
        pGsoCtx->cbHdrsSeg = offTransport; /* Exclude UDP header */
        if (RT_UNLIKELY(   offTransport + cbTransportHdr >= UINT8_MAX
                        || offTransport + cbTransportHdr >= pSkb->len ))
        {
            Log5(("vboxNetFltLinuxCanForwardAsGso: No space for UDP header; off=%#x skb_len=%#x\n", offTransport, pSkb->len));
            return false;
        }
    }

    /*
     * We're good, init the GSO context.
     */
    pGsoCtx->u8Type       = enmGsoType;
    pGsoCtx->cbHdrsTotal  = offTransport + cbTransportHdr;
    pGsoCtx->cbMaxSeg     = skb_shinfo(pSkb)->gso_size;
    pGsoCtx->offHdr1      = pSkb->mac_len;
    pGsoCtx->offHdr2      = offTransport;
    pGsoCtx->u8Unused     = 0;

    return true;
}

/**
 * Forward the socket buffer as a GSO internal network frame.
 *
 * @returns IPRT status code.
 * @param   pThis               The net filter instance.
 * @param   pSkb                The GSO socket buffer.
 * @param   fSrc                The source.
 * @param   pGsoCtx             Where to return the GSO context on success.
 */
static int vboxNetFltLinuxForwardAsGso(PVBOXNETFLTINS pThis, struct sk_buff *pSkb, uint32_t fSrc, PCPDMNETWORKGSO pGsoCtx)
{
    int         rc;
    unsigned    cSegs = vboxNetFltLinuxCalcSGSegments(pSkb);
    if (RT_LIKELY(cSegs <= MAX_SKB_FRAGS + 1))
    {
        PINTNETSG pSG = (PINTNETSG)alloca(RT_OFFSETOF(INTNETSG, aSegs[cSegs]));
        if (RT_LIKELY(pSG))
        {
            vboxNetFltLinuxSkBufToSG(pThis, pSkb, pSG, cSegs, fSrc, pGsoCtx);

            vboxNetFltDumpPacket(pSG, false, (fSrc & INTNETTRUNKDIR_HOST) ? "host" : "wire", 1);
            pThis->pSwitchPort->pfnRecv(pThis->pSwitchPort, NULL /* pvIf */, pSG, fSrc);

            vboxNetFltLinuxDestroySG(pSG);
            rc = VINF_SUCCESS;
        }
        else
        {
            Log(("VBoxNetFlt: Dropping the sk_buff (failure case).\n"));
            rc = VERR_NO_MEMORY;
        }
    }
    else
    {
        Log(("VBoxNetFlt: Bad sk_buff? cSegs=%#x.\n", cSegs));
        rc = VERR_INTERNAL_ERROR_3;
    }

    Log4(("VBoxNetFlt: Dropping the sk_buff.\n"));
    dev_kfree_skb(pSkb);
    return rc;
}

#endif /* VBOXNETFLT_WITH_GSO_RECV */

/**
 * Worker for vboxNetFltLinuxForwardToIntNet.
 *
 * @returns VINF_SUCCESS or VERR_NO_MEMORY.
 * @param   pThis               The net filter instance.
 * @param   pBuf                The socket buffer.
 * @param   fSrc                The source.
 */
static int vboxNetFltLinuxForwardSegment(PVBOXNETFLTINS pThis, struct sk_buff *pBuf, uint32_t fSrc)
{
    int         rc;
    unsigned    cSegs = vboxNetFltLinuxCalcSGSegments(pBuf);
    if (cSegs <= MAX_SKB_FRAGS + 1)
    {
        PINTNETSG pSG = (PINTNETSG)alloca(RT_OFFSETOF(INTNETSG, aSegs[cSegs]));
        if (RT_LIKELY(pSG))
        {
            if (fSrc & INTNETTRUNKDIR_WIRE)
            {
                /*
                 * The packet came from wire, ethernet header was removed by device driver.
                 * Restore it using mac_len field. This takes into account VLAN headers too.
                 */
                skb_push(pBuf, pBuf->mac_len);
            }

            vboxNetFltLinuxSkBufToSG(pThis, pBuf, pSG, cSegs, fSrc, NULL /*pGsoCtx*/);

            vboxNetFltDumpPacket(pSG, false, (fSrc & INTNETTRUNKDIR_HOST) ? "host" : "wire", 1);
            pThis->pSwitchPort->pfnRecv(pThis->pSwitchPort, NULL /* pvIf */, pSG, fSrc);

            vboxNetFltLinuxDestroySG(pSG);
            rc = VINF_SUCCESS;
        }
        else
        {
            Log(("VBoxNetFlt: Failed to allocate SG buffer.\n"));
            rc = VERR_NO_MEMORY;
        }
    }
    else
    {
        Log(("VBoxNetFlt: Bad sk_buff? cSegs=%#x.\n", cSegs));
        rc = VERR_INTERNAL_ERROR_3;
    }

    Log4(("VBoxNetFlt: Dropping the sk_buff.\n"));
    dev_kfree_skb(pBuf);
    return rc;
}

/**
 *
 * @param   pBuf        The socket buffer.  This is consumed by this function.
 */
static void vboxNetFltLinuxForwardToIntNet(PVBOXNETFLTINS pThis, struct sk_buff *pBuf)
{
    uint32_t fSrc = pBuf->pkt_type == PACKET_OUTGOING ? INTNETTRUNKDIR_HOST : INTNETTRUNKDIR_WIRE;

#ifdef VBOXNETFLT_WITH_GSO
    if (skb_is_gso(pBuf))
    {
        PDMNETWORKGSO GsoCtx;
        Log3(("vboxNetFltLinuxForwardToIntNet: skb len=%u data_len=%u truesize=%u next=%p nr_frags=%u gso_size=%u gso_seqs=%u gso_type=%x frag_list=%p pkt_type=%x ip_summed=%d\n",
              pBuf->len, pBuf->data_len, pBuf->truesize, pBuf->next, skb_shinfo(pBuf)->nr_frags, skb_shinfo(pBuf)->gso_size, skb_shinfo(pBuf)->gso_segs, skb_shinfo(pBuf)->gso_type, skb_shinfo(pBuf)->frag_list, pBuf->pkt_type, pBuf->ip_summed));
# ifdef VBOXNETFLT_WITH_GSO_RECV
        if (   (skb_shinfo(pBuf)->gso_type & (SKB_GSO_UDP | SKB_GSO_TCPV6 | SKB_GSO_TCPV4))
            && vboxNetFltLinuxCanForwardAsGso(pThis, pBuf, fSrc, &GsoCtx) )
            vboxNetFltLinuxForwardAsGso(pThis, pBuf, fSrc, &GsoCtx);
        else
# endif
        {
            /* Need to segment the packet */
            struct sk_buff *pNext;
            struct sk_buff *pSegment = skb_gso_segment(pBuf, 0 /*supported features*/);
            if (IS_ERR(pSegment))
            {
                dev_kfree_skb(pBuf);
                LogRel(("VBoxNetFlt: Failed to segment a packet (%d).\n", PTR_ERR(pSegment)));
                return;
            }

            for (; pSegment; pSegment = pNext)
            {
                Log3(("vboxNetFltLinuxForwardToIntNet: segment len=%u data_len=%u truesize=%u next=%p nr_frags=%u gso_size=%u gso_seqs=%u gso_type=%x frag_list=%p pkt_type=%x\n",
                      pSegment->len, pSegment->data_len, pSegment->truesize, pSegment->next, skb_shinfo(pSegment)->nr_frags, skb_shinfo(pSegment)->gso_size, skb_shinfo(pSegment)->gso_segs, skb_shinfo(pSegment)->gso_type, skb_shinfo(pSegment)->frag_list, pSegment->pkt_type));
                pNext = pSegment->next;
                pSegment->next = 0;
                vboxNetFltLinuxForwardSegment(pThis, pSegment, fSrc);
            }
            dev_kfree_skb(pBuf);
        }
    }
    else
#endif /* VBOXNETFLT_WITH_GSO */
    {
        if (pBuf->ip_summed == CHECKSUM_PARTIAL && pBuf->pkt_type == PACKET_OUTGOING)
        {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18)
            /*
             * Try to work around the problem with CentOS 4.7 and 5.2 (2.6.9
             * and 2.6.18 kernels), they pass wrong 'h' pointer down. We take IP
             * header length from the header itself and reconstruct 'h' pointer
             * to TCP (or whatever) header.
             */
            unsigned char *tmp = pBuf->h.raw;
            if (pBuf->h.raw == pBuf->nh.raw && pBuf->protocol == htons(ETH_P_IP))
                pBuf->h.raw = pBuf->nh.raw + pBuf->nh.iph->ihl * 4;
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18) */
            if (VBOX_SKB_CHECKSUM_HELP(pBuf))
            {
                LogRel(("VBoxNetFlt: Failed to compute checksum, dropping the packet.\n"));
                dev_kfree_skb(pBuf);
                return;
            }
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18)
            /* Restore the original (wrong) pointer. */
            pBuf->h.raw = tmp;
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18) */
        }
        vboxNetFltLinuxForwardSegment(pThis, pBuf, fSrc);
    }
}

#ifndef VBOXNETFLT_LINUX_NO_XMIT_QUEUE
/**
 * Work queue handler that forwards the socket buffers queued by
 * vboxNetFltLinuxPacketHandler to the internal network.
 *
 * @param   pWork               The work queue.
 */
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
static void vboxNetFltLinuxXmitTask(struct work_struct *pWork)
# else
static void vboxNetFltLinuxXmitTask(void *pWork)
# endif
{
    PVBOXNETFLTINS  pThis   = VBOX_FLT_XT_TO_INST(pWork);
    struct sk_buff *pBuf;

    Log4(("vboxNetFltLinuxXmitTask: Got work %p.\n", pWork));

    /*
     * Active? Retain the instance and increment the busy counter.
     */
    if (vboxNetFltTryRetainBusyActive(pThis))
    {
        while ((pBuf = skb_dequeue(&pThis->u.s.XmitQueue)) != NULL)
            vboxNetFltLinuxForwardToIntNet(pThis, pBuf);

        vboxNetFltRelease(pThis, true /* fBusy */);
    }
    else
    {
        /** @todo Shouldn't we just drop the packets here? There is little point in
         *        making them accumulate when the VM is paused and it'll only waste
         *        kernel memory anyway... Hmm. maybe wait a short while (2-5 secs)
         *        before start draining the packets (goes for the intnet ring buf
         *        too)? */
    }
}
#endif /* !VBOXNETFLT_LINUX_NO_XMIT_QUEUE */

/**
 * Reports the GSO capabilities of the hardware NIC.
 *
 * @param   pThis               The net filter instance.  The caller hold a
 *                              reference to this.
 */
static void vboxNetFltLinuxReportNicGsoCapabilities(PVBOXNETFLTINS pThis)
{
#ifdef VBOXNETFLT_WITH_GSO_XMIT_WIRE
    if (vboxNetFltTryRetainBusyNotDisconnected(pThis))
    {
        struct net_device  *pDev;
        PINTNETTRUNKSWPORT  pSwitchPort;
        unsigned int        fFeatures;

        RTSpinlockAcquire(pThis->hSpinlock);

        pSwitchPort = pThis->pSwitchPort; /* this doesn't need to be here, but it doesn't harm. */
        pDev = ASMAtomicUoReadPtrT(&pThis->u.s.pDev, struct net_device *);
        if (pDev)
            fFeatures = pDev->features;
        else
            fFeatures = 0;

        RTSpinlockReleaseNoInts(pThis->hSpinlock);

        if (pThis->pSwitchPort)
        {
            /* Set/update the GSO capabilities of the NIC. */
            uint32_t fGsoCapabilites = 0;
            if (fFeatures & NETIF_F_TSO)
                fGsoCapabilites |= RT_BIT_32(PDMNETWORKGSOTYPE_IPV4_TCP);
            if (fFeatures & NETIF_F_TSO6)
                fGsoCapabilites |= RT_BIT_32(PDMNETWORKGSOTYPE_IPV6_TCP);
# if 0 /** @todo GSO: Test UDP offloading (UFO) on linux. */
            if (fFeatures & NETIF_F_UFO)
                fGsoCapabilites |= RT_BIT_32(PDMNETWORKGSOTYPE_IPV4_UDP);
            if (fFeatures & NETIF_F_UFO)
                fGsoCapabilites |= RT_BIT_32(PDMNETWORKGSOTYPE_IPV6_UDP);
# endif
            Log3(("vboxNetFltLinuxReportNicGsoCapabilities: reporting wire %s%s%s%s\n",
                  (fGsoCapabilites & RT_BIT_32(PDMNETWORKGSOTYPE_IPV4_TCP)) ? "tso " : "",
                  (fGsoCapabilites & RT_BIT_32(PDMNETWORKGSOTYPE_IPV6_TCP)) ? "tso6 " : "",
                  (fGsoCapabilites & RT_BIT_32(PDMNETWORKGSOTYPE_IPV4_UDP)) ? "ufo " : "",
                  (fGsoCapabilites & RT_BIT_32(PDMNETWORKGSOTYPE_IPV6_UDP)) ? "ufo6 " : ""));
            pThis->pSwitchPort->pfnReportGsoCapabilities(pThis->pSwitchPort, fGsoCapabilites, INTNETTRUNKDIR_WIRE);
        }

        vboxNetFltRelease(pThis, true /*fBusy*/);
    }
#endif /* VBOXNETFLT_WITH_GSO_XMIT_WIRE */
}

/**
 * Helper that determines whether the host (ignoreing us) is operating the
 * interface in promiscuous mode or not.
 */
static bool vboxNetFltLinuxPromiscuous(PVBOXNETFLTINS pThis)
{
    bool                fRc  = false;
    struct net_device * pDev = vboxNetFltLinuxRetainNetDev(pThis);
    if (pDev)
    {
        fRc = !!(pDev->promiscuity - (ASMAtomicUoReadBool(&pThis->u.s.fPromiscuousSet) & 1));
        LogFlow(("vboxNetFltPortOsIsPromiscuous: returns %d, pDev->promiscuity=%d, fPromiscuousSet=%d\n",
                 fRc, pDev->promiscuity, pThis->u.s.fPromiscuousSet));
        vboxNetFltLinuxReleaseNetDev(pThis, pDev);
    }
    return fRc;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
/**
 * Helper for detecting TAP devices.
 */
static bool vboxNetFltIsTapDevice(PVBOXNETFLTINS pThis, struct net_device *pDev)
{
    if (pDev->ethtool_ops && pDev->ethtool_ops->get_drvinfo)
    {
        struct ethtool_drvinfo Info;

        memset(&Info, 0, sizeof(Info));
        Info.cmd = ETHTOOL_GDRVINFO;
        pDev->ethtool_ops->get_drvinfo(pDev, &Info);
        Log3(("vboxNetFltIsTapDevice: driver=%s version=%s bus_info=%s\n",
              Info.driver, Info.version, Info.bus_info));

        return !strncmp(Info.driver,   "tun", 4)
            && !strncmp(Info.bus_info, "tap", 4);
    }

    return false;
}

/**
 * Helper for updating the link state of TAP devices.
 * Only TAP devices are affected.
 */
static void vboxNetFltSetTapLinkState(PVBOXNETFLTINS pThis, struct net_device *pDev, bool fLinkUp)
{
    if (vboxNetFltIsTapDevice(pThis, pDev))
    {
        Log3(("vboxNetFltSetTapLinkState: bringing %s tap device link state\n",
              fLinkUp ? "up" : "down"));
        netif_tx_lock_bh(pDev);
        if (fLinkUp)
            netif_carrier_on(pDev);
        else
            netif_carrier_off(pDev);
        netif_tx_unlock_bh(pDev);
    }
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36) */
DECLINLINE(void) vboxNetFltSetTapLinkState(PVBOXNETFLTINS pThis, struct net_device *pDev, bool fLinkUp)
{
    /* Nothing to do for pre-2.6.36 kernels. */
}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36) */

/**
 * Internal worker for vboxNetFltLinuxNotifierCallback.
 *
 * @returns VBox status code.
 * @param   pThis           The instance.
 * @param   fRediscovery    If set we're doing a rediscovery attempt, so, don't
 *                          flood the release log.
 */
static int vboxNetFltLinuxAttachToInterface(PVBOXNETFLTINS pThis, struct net_device *pDev)
{
    LogFlow(("vboxNetFltLinuxAttachToInterface: pThis=%p (%s)\n", pThis, pThis->szName));

    /*
     * Retain and store the device.
     */
    dev_hold(pDev);

    RTSpinlockAcquire(pThis->hSpinlock);
    ASMAtomicUoWritePtr(&pThis->u.s.pDev, pDev);
    RTSpinlockReleaseNoInts(pThis->hSpinlock);

    Log(("vboxNetFltLinuxAttachToInterface: Device %p(%s) retained. ref=%d\n",
          pDev, pDev->name,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
          netdev_refcnt_read(pDev)
#else
          atomic_read(&pDev->refcnt)
#endif
          ));
    Log(("vboxNetFltLinuxAttachToInterface: Got pDev=%p pThis=%p pThis->u.s.pDev=%p\n",
          pDev, pThis, ASMAtomicUoReadPtrT(&pThis->u.s.pDev, struct net_device *)));

    /* Get the mac address while we still have a valid net_device reference. */
    memcpy(&pThis->u.s.MacAddr, pDev->dev_addr, sizeof(pThis->u.s.MacAddr));

    /*
     * Install a packet filter for this device with a protocol wildcard (ETH_P_ALL).
     */
    pThis->u.s.PacketType.type = __constant_htons(ETH_P_ALL);
    pThis->u.s.PacketType.dev  = pDev;
    pThis->u.s.PacketType.func = vboxNetFltLinuxPacketHandler;
    dev_add_pack(&pThis->u.s.PacketType);
    ASMAtomicUoWriteBool(&pThis->u.s.fPacketHandler, true);
    Log(("vboxNetFltLinuxAttachToInterface: this=%p: Packet handler installed.\n", pThis));

#ifdef VBOXNETFLT_WITH_HOST2WIRE_FILTER
    vboxNetFltLinuxHookDev(pThis, pDev);
#endif

    /*
     * If attaching to TAP interface we need to bring the link state up
     * starting from 2.6.36 kernel.
     */
    vboxNetFltSetTapLinkState(pThis, pDev, true);

    /*
     * Set indicators that require the spinlock. Be abit paranoid about racing
     * the device notification handle.
     */
    RTSpinlockAcquire(pThis->hSpinlock);
    pDev = ASMAtomicUoReadPtrT(&pThis->u.s.pDev, struct net_device *);
    if (pDev)
    {
        ASMAtomicUoWriteBool(&pThis->fDisconnectedFromHost, false);
        ASMAtomicUoWriteBool(&pThis->u.s.fRegistered, true);
        pDev = NULL; /* don't dereference it */
    }
    RTSpinlockReleaseNoInts(pThis->hSpinlock);

    /*
     * If the above succeeded report GSO capabilities,  if not undo and
     * release the device.
     */
    if (!pDev)
    {
        Assert(pThis->pSwitchPort);
        if (vboxNetFltTryRetainBusyNotDisconnected(pThis))
        {
            vboxNetFltLinuxReportNicGsoCapabilities(pThis);
            pThis->pSwitchPort->pfnReportMacAddress(pThis->pSwitchPort, &pThis->u.s.MacAddr);
            pThis->pSwitchPort->pfnReportPromiscuousMode(pThis->pSwitchPort, vboxNetFltLinuxPromiscuous(pThis));
            pThis->pSwitchPort->pfnReportNoPreemptDsts(pThis->pSwitchPort, INTNETTRUNKDIR_WIRE | INTNETTRUNKDIR_HOST);
            vboxNetFltRelease(pThis, true /*fBusy*/);
        }
    }
    else
    {
#ifdef VBOXNETFLT_WITH_HOST2WIRE_FILTER
        vboxNetFltLinuxUnhookDev(pThis, pDev);
#endif
        RTSpinlockAcquire(pThis->hSpinlock);
        ASMAtomicUoWriteNullPtr(&pThis->u.s.pDev);
        RTSpinlockReleaseNoInts(pThis->hSpinlock);
        dev_put(pDev);
        Log(("vboxNetFltLinuxAttachToInterface: Device %p(%s) released. ref=%d\n",
             pDev, pDev->name,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
             netdev_refcnt_read(pDev)
#else
             atomic_read(&pDev->refcnt)
#endif
             ));
    }

    LogRel(("VBoxNetFlt: attached to '%s' / %.*Rhxs\n", pThis->szName, sizeof(pThis->u.s.MacAddr), &pThis->u.s.MacAddr));
    return VINF_SUCCESS;
}


static int vboxNetFltLinuxUnregisterDevice(PVBOXNETFLTINS pThis, struct net_device *pDev)
{
    bool fRegistered;
    Assert(!pThis->fDisconnectedFromHost);

#ifdef VBOXNETFLT_WITH_HOST2WIRE_FILTER
    vboxNetFltLinuxUnhookDev(pThis, pDev);
#endif

    if (ASMAtomicCmpXchgBool(&pThis->u.s.fPacketHandler, false, true))
    {
        dev_remove_pack(&pThis->u.s.PacketType);
        Log(("vboxNetFltLinuxUnregisterDevice: this=%p: packet handler removed.\n", pThis));
    }

    RTSpinlockAcquire(pThis->hSpinlock);
    fRegistered = ASMAtomicXchgBool(&pThis->u.s.fRegistered, false);
    if (fRegistered)
    {
        ASMAtomicWriteBool(&pThis->fDisconnectedFromHost, true);
        ASMAtomicUoWriteNullPtr(&pThis->u.s.pDev);
    }
    RTSpinlockReleaseNoInts(pThis->hSpinlock);

    if (fRegistered)
    {
#ifndef VBOXNETFLT_LINUX_NO_XMIT_QUEUE
        skb_queue_purge(&pThis->u.s.XmitQueue);
#endif
        Log(("vboxNetFltLinuxUnregisterDevice: this=%p: xmit queue purged.\n", pThis));
        Log(("vboxNetFltLinuxUnregisterDevice: Device %p(%s) released. ref=%d\n",
             pDev, pDev->name,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
             netdev_refcnt_read(pDev)
#else
             atomic_read(&pDev->refcnt)
#endif
           ));
        dev_put(pDev);
    }

    return NOTIFY_OK;
}

static int vboxNetFltLinuxDeviceIsUp(PVBOXNETFLTINS pThis, struct net_device *pDev)
{
    /* Check if we are not suspended and promiscuous mode has not been set. */
    if (   pThis->enmTrunkState == INTNETTRUNKIFSTATE_ACTIVE
        && !ASMAtomicUoReadBool(&pThis->u.s.fPromiscuousSet))
    {
        /* Note that there is no need for locking as the kernel got hold of the lock already. */
        dev_set_promiscuity(pDev, 1);
        ASMAtomicWriteBool(&pThis->u.s.fPromiscuousSet, true);
        Log(("vboxNetFltLinuxDeviceIsUp: enabled promiscuous mode on %s (%d)\n", pThis->szName, pDev->promiscuity));
    }
    else
        Log(("vboxNetFltLinuxDeviceIsUp: no need to enable promiscuous mode on %s (%d)\n", pThis->szName, pDev->promiscuity));
    return NOTIFY_OK;
}

static int vboxNetFltLinuxDeviceGoingDown(PVBOXNETFLTINS pThis, struct net_device *pDev)
{
    /* Undo promiscuous mode if we has set it. */
    if (ASMAtomicUoReadBool(&pThis->u.s.fPromiscuousSet))
    {
        /* Note that there is no need for locking as the kernel got hold of the lock already. */
        dev_set_promiscuity(pDev, -1);
        ASMAtomicWriteBool(&pThis->u.s.fPromiscuousSet, false);
        Log(("vboxNetFltLinuxDeviceGoingDown: disabled promiscuous mode on %s (%d)\n", pThis->szName, pDev->promiscuity));
    }
    else
        Log(("vboxNetFltLinuxDeviceGoingDown: no need to disable promiscuous mode on %s (%d)\n", pThis->szName, pDev->promiscuity));
    return NOTIFY_OK;
}

#ifdef LOG_ENABLED
/** Stringify the NETDEV_XXX constants. */
static const char *vboxNetFltLinuxGetNetDevEventName(unsigned long ulEventType)
{
    const char *pszEvent = "NETDRV_<unknown>";
    switch (ulEventType)
    {
        case NETDEV_REGISTER: pszEvent = "NETDEV_REGISTER"; break;
        case NETDEV_UNREGISTER: pszEvent = "NETDEV_UNREGISTER"; break;
        case NETDEV_UP: pszEvent = "NETDEV_UP"; break;
        case NETDEV_DOWN: pszEvent = "NETDEV_DOWN"; break;
        case NETDEV_REBOOT: pszEvent = "NETDEV_REBOOT"; break;
        case NETDEV_CHANGENAME: pszEvent = "NETDEV_CHANGENAME"; break;
        case NETDEV_CHANGE: pszEvent = "NETDEV_CHANGE"; break;
        case NETDEV_CHANGEMTU: pszEvent = "NETDEV_CHANGEMTU"; break;
        case NETDEV_CHANGEADDR: pszEvent = "NETDEV_CHANGEADDR"; break;
        case NETDEV_GOING_DOWN: pszEvent = "NETDEV_GOING_DOWN"; break;
# ifdef NETDEV_FEAT_CHANGE
        case NETDEV_FEAT_CHANGE: pszEvent = "NETDEV_FEAT_CHANGE"; break;
# endif
    }
    return pszEvent;
}
#endif /* LOG_ENABLED */

/**
 * Callback for listening to netdevice events.
 *
 * This works the rediscovery, clean up on unregistration, promiscuity on
 * up/down, and GSO feature changes from ethtool.
 *
 * @returns NOTIFY_OK
 * @param   self                Pointer to our notifier registration block.
 * @param   ulEventType         The event.
 * @param   ptr                 Event specific, but it is usually the device it
 *                              relates to.
 */
static int vboxNetFltLinuxNotifierCallback(struct notifier_block *self, unsigned long ulEventType, void *ptr)

{
    PVBOXNETFLTINS      pThis = VBOX_FLT_NB_TO_INST(self);
    struct net_device  *pDev  = (struct net_device *)ptr;
    int                 rc    = NOTIFY_OK;

    Log(("VBoxNetFlt: got event %s(0x%lx) on %s, pDev=%p pThis=%p pThis->u.s.pDev=%p\n",
         vboxNetFltLinuxGetNetDevEventName(ulEventType), ulEventType, pDev->name, pDev, pThis, ASMAtomicUoReadPtrT(&pThis->u.s.pDev, struct net_device *)));
    if (    ulEventType == NETDEV_REGISTER
        && !strcmp(pDev->name, pThis->szName))
    {
        vboxNetFltLinuxAttachToInterface(pThis, pDev);
    }
    else
    {
        pDev = ASMAtomicUoReadPtrT(&pThis->u.s.pDev, struct net_device *);
        if (pDev == ptr)
        {
            switch (ulEventType)
            {
                case NETDEV_UNREGISTER:
                    rc = vboxNetFltLinuxUnregisterDevice(pThis, pDev);
                    break;
                case NETDEV_UP:
                    rc = vboxNetFltLinuxDeviceIsUp(pThis, pDev);
                    break;
                case NETDEV_GOING_DOWN:
                    rc = vboxNetFltLinuxDeviceGoingDown(pThis, pDev);
                    break;
                case NETDEV_CHANGENAME:
                    break;
#ifdef NETDEV_FEAT_CHANGE
                case NETDEV_FEAT_CHANGE:
                    vboxNetFltLinuxReportNicGsoCapabilities(pThis);
                    break;
#endif
            }
        }
    }

    return rc;
}

bool vboxNetFltOsMaybeRediscovered(PVBOXNETFLTINS pThis)
{
    return !ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost);
}

int  vboxNetFltPortOsXmit(PVBOXNETFLTINS pThis, void *pvIfData, PINTNETSG pSG, uint32_t fDst)
{
    struct net_device * pDev;
    int err;
    int rc = VINF_SUCCESS;
    NOREF(pvIfData);

    LogFlow(("vboxNetFltPortOsXmit: pThis=%p (%s)\n", pThis, pThis->szName));

    pDev = vboxNetFltLinuxRetainNetDev(pThis);
    if (pDev)
    {
        /*
         * Create a sk_buff for the gather list and push it onto the wire.
         */
        if (fDst & INTNETTRUNKDIR_WIRE)
        {
            struct sk_buff *pBuf = vboxNetFltLinuxSkBufFromSG(pThis, pSG, true);
            if (pBuf)
            {
                vboxNetFltDumpPacket(pSG, true, "wire", 1);
                Log4(("vboxNetFltPortOsXmit: pBuf->cb dump:\n%.*Rhxd\n", sizeof(pBuf->cb), pBuf->cb));
                Log4(("vboxNetFltPortOsXmit: dev_queue_xmit(%p)\n", pBuf));
                err = dev_queue_xmit(pBuf);
                if (err)
                    rc = RTErrConvertFromErrno(err);
            }
            else
                rc = VERR_NO_MEMORY;
        }

        /*
         * Create a sk_buff for the gather list and push it onto the host stack.
         */
        if (fDst & INTNETTRUNKDIR_HOST)
        {
            struct sk_buff *pBuf = vboxNetFltLinuxSkBufFromSG(pThis, pSG, false);
            if (pBuf)
            {
                vboxNetFltDumpPacket(pSG, true, "host", (fDst & INTNETTRUNKDIR_WIRE) ? 0 : 1);
                Log4(("vboxNetFltPortOsXmit: pBuf->cb dump:\n%.*Rhxd\n", sizeof(pBuf->cb), pBuf->cb));
                Log4(("vboxNetFltPortOsXmit: netif_rx_ni(%p)\n", pBuf));
                err = netif_rx_ni(pBuf);
                if (err)
                    rc = RTErrConvertFromErrno(err);
            }
            else
                rc = VERR_NO_MEMORY;
        }

        vboxNetFltLinuxReleaseNetDev(pThis, pDev);
    }

    return rc;
}


void vboxNetFltPortOsSetActive(PVBOXNETFLTINS pThis, bool fActive)
{
    struct net_device * pDev;

    LogFlow(("vboxNetFltPortOsSetActive: pThis=%p (%s), fActive=%s, fDisablePromiscuous=%s\n",
             pThis, pThis->szName, fActive?"true":"false",
             pThis->fDisablePromiscuous?"true":"false"));

    if (pThis->fDisablePromiscuous)
        return;

    pDev = vboxNetFltLinuxRetainNetDev(pThis);
    if (pDev)
    {
        /*
         * This api is a bit weird, the best reference is the code.
         *
         * Also, we have a bit or race conditions wrt the maintenance of
         * host the interface promiscuity for vboxNetFltPortOsIsPromiscuous.
         */
#ifdef LOG_ENABLED
        u_int16_t fIf;
        unsigned const cPromiscBefore = pDev->promiscuity;
#endif
        if (fActive)
        {
            Assert(!pThis->u.s.fPromiscuousSet);

            rtnl_lock();
            dev_set_promiscuity(pDev, 1);
            rtnl_unlock();
            pThis->u.s.fPromiscuousSet = true;
            Log(("vboxNetFltPortOsSetActive: enabled promiscuous mode on %s (%d)\n", pThis->szName, pDev->promiscuity));
        }
        else
        {
            if (pThis->u.s.fPromiscuousSet)
            {
                rtnl_lock();
                dev_set_promiscuity(pDev, -1);
                rtnl_unlock();
                Log(("vboxNetFltPortOsSetActive: disabled promiscuous mode on %s (%d)\n", pThis->szName, pDev->promiscuity));
            }
            pThis->u.s.fPromiscuousSet = false;

#ifdef LOG_ENABLED
            fIf = dev_get_flags(pDev);
            Log(("VBoxNetFlt: fIf=%#x; %d->%d\n", fIf, cPromiscBefore, pDev->promiscuity));
#endif
        }

        vboxNetFltLinuxReleaseNetDev(pThis, pDev);
    }
}


int vboxNetFltOsDisconnectIt(PVBOXNETFLTINS pThis)
{
    /*
     * Remove packet handler when we get disconnected from internal switch as
     * we don't want the handler to forward packets to disconnected switch.
     */
    if (ASMAtomicCmpXchgBool(&pThis->u.s.fPacketHandler, false, true))
    {
        dev_remove_pack(&pThis->u.s.PacketType);
        Log(("vboxNetFltOsDisconnectIt: this=%p: Packet handler removed.\n", pThis));
    }
    return VINF_SUCCESS;
}


int  vboxNetFltOsConnectIt(PVBOXNETFLTINS pThis)
{
    /*
     * Report the GSO capabilities of the host and device (if connected).
     * Note! No need to mark ourselves busy here.
     */
    /** @todo duplicate work here now? Attach */
#if defined(VBOXNETFLT_WITH_GSO_XMIT_HOST)
    Log3(("vboxNetFltOsConnectIt: reporting host tso tso6 ufo\n"));
    pThis->pSwitchPort->pfnReportGsoCapabilities(pThis->pSwitchPort,
                                                 0
                                                 | RT_BIT_32(PDMNETWORKGSOTYPE_IPV4_TCP)
                                                 | RT_BIT_32(PDMNETWORKGSOTYPE_IPV6_TCP)
                                                 | RT_BIT_32(PDMNETWORKGSOTYPE_IPV4_UDP)
# if 0 /** @todo GSO: Test UDP offloading (UFO) on linux. */
                                                 | RT_BIT_32(PDMNETWORKGSOTYPE_IPV6_UDP)
# endif
                                                 , INTNETTRUNKDIR_HOST);

#endif
    vboxNetFltLinuxReportNicGsoCapabilities(pThis);

    return VINF_SUCCESS;
}


void vboxNetFltOsDeleteInstance(PVBOXNETFLTINS pThis)
{
    struct net_device  *pDev;
    bool                fRegistered;

#ifdef VBOXNETFLT_WITH_HOST2WIRE_FILTER
    vboxNetFltLinuxUnhookDev(pThis, NULL);
#endif

    /** @todo This code may race vboxNetFltLinuxUnregisterDevice (very very
     *        unlikely, but none the less).  Since it doesn't actually update the
     *        state (just reads it), it is likely to panic in some interesting
     *        ways. */

    RTSpinlockAcquire(pThis->hSpinlock);
    pDev = ASMAtomicUoReadPtrT(&pThis->u.s.pDev, struct net_device *);
    fRegistered = ASMAtomicXchgBool(&pThis->u.s.fRegistered, false);
    RTSpinlockReleaseNoInts(pThis->hSpinlock);

    if (fRegistered)
    {
        vboxNetFltSetTapLinkState(pThis, pDev, false);

#ifndef VBOXNETFLT_LINUX_NO_XMIT_QUEUE
        skb_queue_purge(&pThis->u.s.XmitQueue);
#endif
        Log(("vboxNetFltOsDeleteInstance: this=%p: xmit queue purged.\n", pThis));
        Log(("vboxNetFltOsDeleteInstance: Device %p(%s) released. ref=%d\n",
             pDev, pDev->name,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
             netdev_refcnt_read(pDev)
#else
             atomic_read(&pDev->refcnt)
#endif
             ));
        dev_put(pDev);
    }
    Log(("vboxNetFltOsDeleteInstance: this=%p: Notifier removed.\n", pThis));
    unregister_netdevice_notifier(&pThis->u.s.Notifier);
    module_put(THIS_MODULE);
}


int  vboxNetFltOsInitInstance(PVBOXNETFLTINS pThis, void *pvContext)
{
    int err;
    NOREF(pvContext);

    pThis->u.s.Notifier.notifier_call = vboxNetFltLinuxNotifierCallback;
    err = register_netdevice_notifier(&pThis->u.s.Notifier);
    if (err)
        return VERR_INTNET_FLT_IF_FAILED;
    if (!pThis->u.s.fRegistered)
    {
        unregister_netdevice_notifier(&pThis->u.s.Notifier);
        LogRel(("VBoxNetFlt: failed to find %s.\n", pThis->szName));
        return VERR_INTNET_FLT_IF_NOT_FOUND;
    }

    Log(("vboxNetFltOsInitInstance: this=%p: Notifier installed.\n", pThis));
    if (   pThis->fDisconnectedFromHost
        || !try_module_get(THIS_MODULE))
        return VERR_INTNET_FLT_IF_FAILED;

    return VINF_SUCCESS;
}

int  vboxNetFltOsPreInitInstance(PVBOXNETFLTINS pThis)
{
    /*
     * Init the linux specific members.
     */
    ASMAtomicUoWriteNullPtr(&pThis->u.s.pDev);
    pThis->u.s.fRegistered     = false;
    pThis->u.s.fPromiscuousSet = false;
    pThis->u.s.fPacketHandler  = false;
    memset(&pThis->u.s.PacketType, 0, sizeof(pThis->u.s.PacketType));
#ifndef VBOXNETFLT_LINUX_NO_XMIT_QUEUE
    skb_queue_head_init(&pThis->u.s.XmitQueue);
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
    INIT_WORK(&pThis->u.s.XmitTask, vboxNetFltLinuxXmitTask);
# else
    INIT_WORK(&pThis->u.s.XmitTask, vboxNetFltLinuxXmitTask, &pThis->u.s.XmitTask);
# endif
#endif

    return VINF_SUCCESS;
}


void vboxNetFltPortOsNotifyMacAddress(PVBOXNETFLTINS pThis, void *pvIfData, PCRTMAC pMac)
{
    NOREF(pThis); NOREF(pvIfData); NOREF(pMac);
}


int vboxNetFltPortOsConnectInterface(PVBOXNETFLTINS pThis, void *pvIf, void **pvIfData)
{
    /* Nothing to do */
    NOREF(pThis); NOREF(pvIf); NOREF(pvIfData);
    return VINF_SUCCESS;
}


int vboxNetFltPortOsDisconnectInterface(PVBOXNETFLTINS pThis, void *pvIfData)
{
    /* Nothing to do */
    NOREF(pThis); NOREF(pvIfData);
    return VINF_SUCCESS;
}

