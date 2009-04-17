/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Linux Specific Code.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
#include "the-linux-kernel.h"
#include "version-generated.h"
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/miscdevice.h>

#define LOG_GROUP LOG_GROUP_NET_FLT_DRV
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/semaphore.h>
#include <iprt/initterm.h>
#include <iprt/process.h>
#include <iprt/mem.h>
#include <iprt/log.h>
#include <iprt/mp.h>
#include <iprt/mem.h>
#include <iprt/time.h>

#define VBOXNETFLT_OS_SPECFIC 1
#include "../VBoxNetFltInternal.h"

#define VBOX_FLT_NB_TO_INST(pNB) ((PVBOXNETFLTINS)((uint8_t *)pNB - \
                                  RT_OFFSETOF(VBOXNETFLTINS, u.s.Notifier)))
#define VBOX_FLT_PT_TO_INST(pPT) ((PVBOXNETFLTINS)((uint8_t *)pPT - \
                                  RT_OFFSETOF(VBOXNETFLTINS, u.s.PacketType)))
#define VBOX_FLT_XT_TO_INST(pXT) ((PVBOXNETFLTINS)((uint8_t *)pXT - \
                                  RT_OFFSETOF(VBOXNETFLTINS, u.s.XmitTask)))

#define VBOX_GET_PCOUNT(pDev) (pDev->promiscuity)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
# define VBOX_SKB_RESET_NETWORK_HDR(skb) skb_reset_network_header(skb)
# define VBOX_SKB_RESET_MAC_HDR(skb) skb_reset_mac_header(skb)
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22) */
# define VBOX_SKB_RESET_NETWORK_HDR(skb) skb->nh.raw = skb->data
# define VBOX_SKB_RESET_MAC_HDR(skb) skb->mac.raw = skb->data
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
# define VBOX_SKB_CHECKSUM_HELP(skb) skb_checksum_help(skb)
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19) */
# define CHECKSUM_PARTIAL CHECKSUM_HW
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
#  define VBOX_SKB_CHECKSUM_HELP(skb) skb_checksum_help(skb, 0)
# else /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 10) */
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 7)
#   define VBOX_SKB_CHECKSUM_HELP(skb) skb_checksum_help(&skb, 0)
#  else /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 7) */
#   define VBOX_SKB_CHECKSUM_HELP(skb) (!skb_checksum_help(skb))
#  endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 7) */
# endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 10) */
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
# define VBOX_SKB_IS_GSO(skb) skb_is_gso(skb)
                                        /* No features, very dumb device */
# define VBOX_SKB_GSO_SEGMENT(skb) skb_gso_segment(skb, 0)
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18) */
# define VBOX_SKB_IS_GSO(skb) false
# define VBOX_SKB_GSO_SEGMENT(skb) NULL
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18) */

#ifndef NET_IP_ALIGN
# define NET_IP_ALIGN 2
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
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

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int      VBoxNetFltLinuxInit(void);
static void     VBoxNetFltLinuxUnload(void);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * The (common) global data.
 */
#ifdef RT_ARCH_AMD64
/**
 * Memory for the executable memory heap (in IPRT).
 */
extern uint8_t g_abExecMemory[4096]; /* cannot donate less than one page */
__asm__(".section execmemory, \"awx\", @progbits\n\t"
        ".align 32\n\t"
        ".globl g_abExecMemory\n"
        "g_abExecMemory:\n\t"
        ".zero 4096\n\t"
        ".type g_abExecMemory, @object\n\t"
        ".size g_abExecMemory, 4096\n\t"
        ".text\n\t");
#endif

static VBOXNETFLTGLOBALS g_VBoxNetFltGlobals;

module_init(VBoxNetFltLinuxInit);
module_exit(VBoxNetFltLinuxUnload);

MODULE_AUTHOR("Sun Microsystems, Inc.");
MODULE_DESCRIPTION("VirtualBox Network Filter Driver");
MODULE_LICENSE("GPL");
#ifdef MODULE_VERSION
# define xstr(s) str(s)
# define str(s)  #s
MODULE_VERSION(VBOX_VERSION_STRING " (" xstr(INTNETTRUNKIFPORT_VERSION) ")");
#endif

/**
 * The (common) global data.
 */
static VBOXNETFLTGLOBALS g_VBoxNetFltGlobals;

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
#ifdef RT_ARCH_AMD64
        rc = RTR0MemExecDonate(&g_abExecMemory[0], sizeof(g_abExecMemory));
        printk("VBoxNetFlt: dbg - g_abExecMemory=%p\n", (void *)&g_abExecMemory[0]);
        if (RT_FAILURE(rc))
        {
            printk("VBoxNetFlt: failed to donate exec memory, no logging will be available.\n");
        }
#endif
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
        else
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
 * Reads and retains the host interface handle.
 *
 * @returns The handle, NULL if detached.
 * @param   pThis
 */
DECLINLINE(struct net_device *) vboxNetFltLinuxRetainNetDev(PVBOXNETFLTINS pThis)
{
#if 0
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    struct net_device *pDev = NULL;

    Log(("vboxNetFltLinuxRetainNetDev\n"));
    /*
     * Be careful here to avoid problems racing the detached callback.
     */
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    if (!ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost))
    {
        pDev = (struct net_device *)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pDev);
        if (pDev)
        {
            dev_hold(pDev);
            Log(("vboxNetFltLinuxRetainNetDev: Device %p(%s) retained. ref=%d\n", pDev, pDev->name, atomic_read(&pDev->refcnt)));
        }
    }
    RTSpinlockRelease(pThis->hSpinlock, &Tmp);

    Log(("vboxNetFltLinuxRetainNetDev - done\n"));
    return pDev;
#else
    return (struct net_device *)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pDev);
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
        Log(("vboxNetFltLinuxReleaseNetDev: Device %p(%s) released. ref=%d\n", pDev, pDev->name, atomic_read(&pDev->refcnt)));
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
 */
static struct sk_buff *vboxNetFltLinuxSkBufFromSG(PVBOXNETFLTINS pThis, PINTNETSG pSG, bool fDstWire)
{
    struct sk_buff *pPkt;
    struct net_device *pDev;
    /*
     * Because we're lazy, we will ASSUME that all SGs coming from INTNET
     * will only contain one single segment.
     */
    if (pSG->cSegsUsed != 1 || pSG->cbTotal != pSG->aSegs[0].cb)
    {
        LogRel(("VBoxNetFlt: Dropped multi-segment(%d) packet coming from internal network.\n", pSG->cSegsUsed));
        return NULL;
    }
    if (pSG->cbTotal == 0)
    {
        LogRel(("VBoxNetFlt: Dropped empty packet coming from internal network.\n"));
        return NULL;
    }

    /*
     * Allocate a packet and copy over the data.
     *
     */
    pDev = (struct net_device *)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pDev);
    pPkt = dev_alloc_skb(pSG->cbTotal + NET_IP_ALIGN);
    if (pPkt)
    {
        pPkt->dev = pDev;
        /* Align IP header on 16-byte boundary: 2 + 14 (ethernet hdr size). */
        skb_reserve(pPkt, NET_IP_ALIGN);
        skb_put(pPkt, pSG->cbTotal);
        memcpy(pPkt->data, pSG->aSegs[0].pv, pSG->cbTotal);
        /* Set protocol and packet_type fields. */
        pPkt->protocol = eth_type_trans(pPkt, pDev);
        pPkt->ip_summed = CHECKSUM_NONE;
        if (fDstWire)
        {
            VBOX_SKB_RESET_NETWORK_HDR(pPkt);
            /* Restore ethernet header back. */
            skb_push(pPkt, ETH_HLEN);
            VBOX_SKB_RESET_MAC_HDR(pPkt);
        }
        VBOXNETFLT_SKB_TAG(pPkt) = VBOXNETFLT_CB_TAG(pPkt);

        return pPkt;
    }
    else
        Log(("vboxNetFltLinuxSkBufFromSG: Failed to allocate sk_buff(%u).\n", pSG->cbTotal));
    pSG->pvUserData = NULL;

    return NULL;
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
 */
DECLINLINE(void) vboxNetFltLinuxSkBufToSG(PVBOXNETFLTINS pThis, struct sk_buff *pBuf, PINTNETSG pSG, unsigned cSegs, uint32_t fSrc)
{
    int i;
    NOREF(pThis);

    Assert(!skb_shinfo(pBuf)->frag_list);
    pSG->pvOwnerData = NULL;
    pSG->pvUserData = NULL;
    pSG->pvUserData2 = NULL;
    pSG->cUsers = 1;
    pSG->fFlags = INTNETSG_FLAGS_TEMP;
    pSG->cSegsAlloc = cSegs;

   if (fSrc & INTNETTRUNKDIR_WIRE)
    {
        /*
         * The packet came from wire, ethernet header was removed by device driver.
         * Restore it.
         */
        skb_push(pBuf, ETH_HLEN);
    }
    pSG->cbTotal = pBuf->len;
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
    pSG->cSegsUsed = ++i;
#else
    pSG->aSegs[0].cb = pBuf->len;
    pSG->aSegs[0].pv = pBuf->data;
    pSG->aSegs[0].Phys = NIL_RTHCPHYS;
    pSG->cSegsUsed = i = 1;
#endif


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
    /*
     * Drop it immediately?
     */
    if (!pBuf)
        return 0;

    pThis = VBOX_FLT_PT_TO_INST(pPacketType);
    pDev = (struct net_device *)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pDev);
    if (pThis->u.s.pDev != pSkbDev)
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
        struct sk_buff *pCopy = skb_copy(pBuf, GFP_ATOMIC);
        kfree_skb(pBuf);
        if (!pCopy)
        {
            LogRel(("VBoxNetFlt: Failed to allocate packet buffer, dropping the packet.\n"));
            return 0;
        }
        pBuf = pCopy;
    }
#endif

    /* Add the packet to transmit queue and schedule the bottom half. */
    skb_queue_tail(&pThis->u.s.XmitQueue, pBuf);
    schedule_work(&pThis->u.s.XmitTask);
    Log4(("vboxNetFltLinuxPacketHandler: scheduled work %p for sk_buff %p\n",
          &pThis->u.s.XmitTask, pBuf));
    /* It does not really matter what we return, it is ignored by the kernel. */
    return 0;
}

static unsigned vboxNetFltLinuxSGSegments(PVBOXNETFLTINS pThis, struct sk_buff *pBuf)
{
#ifdef VBOXNETFLT_SG_SUPPORT
    unsigned cSegs = 1 + skb_shinfo(pBuf)->nr_frags;
#else
    unsigned cSegs = 1;
#endif
#ifdef PADD_RUNT_FRAMES_FROM_HOST
    /*
     * Add a trailer if the frame is too small.
     */
    if (pBuf->len < 60)
        cSegs++;
#endif
    return cSegs;
}

/* WARNING! This function should only be called after vboxNetFltLinuxSkBufToSG()! */
static void  vboxNetFltLinuxFreeSkBuff(struct sk_buff *pBuf, PINTNETSG pSG)
{
#ifdef VBOXNETFLT_SG_SUPPORT
    int i;

    for (i = 0; i < skb_shinfo(pBuf)->nr_frags; i++)
    {
        printk("kunmap(%p)\n", pSG->aSegs[i+1].pv);
        kunmap(pSG->aSegs[i+1].pv);
    }
#endif

    dev_kfree_skb(pBuf);
}

#ifndef LOG_ENABLED
#define vboxNetFltDumpPacket(a, b, c, d)
#else
static void vboxNetFltDumpPacket(PINTNETSG pSG, bool fEgress, const char *pszWhere, int iIncrement)
{
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
    Log3(("%.*Rhxd\n", pSG->aSegs[0].cb, pSG->aSegs[0].pv));
}
#endif

static int vboxNetFltLinuxForwardSegment(PVBOXNETFLTINS pThis, struct sk_buff *pBuf, uint32_t fSrc)
{
    unsigned cSegs = vboxNetFltLinuxSGSegments(pThis, pBuf);
    if (cSegs < MAX_SKB_FRAGS)
    {
        uint8_t *pTmp;
        PINTNETSG pSG = (PINTNETSG)alloca(RT_OFFSETOF(INTNETSG, aSegs[cSegs]));
        if (!pSG)
        {
            Log(("VBoxNetFlt: Failed to allocate SG buffer.\n"));
            return VERR_NO_MEMORY;
        }
        vboxNetFltLinuxSkBufToSG(pThis, pBuf, pSG, cSegs, fSrc);

        pTmp = pSG->aSegs[0].pv;
        vboxNetFltDumpPacket(pSG, false, (fSrc & INTNETTRUNKDIR_HOST) ? "host" : "wire", 1);
        pThis->pSwitchPort->pfnRecv(pThis->pSwitchPort, pSG, fSrc);
        Log4(("VBoxNetFlt: Dropping the sk_buff.\n"));
        vboxNetFltLinuxFreeSkBuff(pBuf, pSG);
    }

    return VINF_SUCCESS;
}

static void vboxNetFltLinuxForwardToIntNet(PVBOXNETFLTINS pThis, struct sk_buff *pBuf)
{
    uint32_t fSrc = pBuf->pkt_type == PACKET_OUTGOING ? INTNETTRUNKDIR_HOST : INTNETTRUNKDIR_WIRE;

    if (VBOX_SKB_IS_GSO(pBuf))
    {
        /* Need to segment the packet */
        struct sk_buff *pNext, *pSegment;
        //Log2(("vboxNetFltLinuxForwardToIntNet: cb=%u gso_size=%u gso_segs=%u gso_type=%u\n",
        //      pBuf->len, skb_shinfo(pBuf)->gso_size, skb_shinfo(pBuf)->gso_segs, skb_shinfo(pBuf)->gso_type));

        for (pSegment = VBOX_SKB_GSO_SEGMENT(pBuf); pSegment; pSegment = pNext)
        {
            pNext = pSegment->next;
            pSegment->next = 0;
            vboxNetFltLinuxForwardSegment(pThis, pSegment, fSrc);
        }
        dev_kfree_skb(pBuf);
    }
    else
    {
        if (pBuf->ip_summed == CHECKSUM_PARTIAL)
            if (VBOX_SKB_CHECKSUM_HELP(pBuf))
            {
                LogRel(("VBoxNetFlt: Failed to compute checksum, dropping the packet.\n"));
                dev_kfree_skb(pBuf);
                return;
            }
        vboxNetFltLinuxForwardSegment(pThis, pBuf, fSrc);
    }
    /*
     * Create a (scatter/)gather list for the sk_buff and feed it to the internal network.
     */
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
static void vboxNetFltLinuxXmitTask(struct work_struct *pWork)
#else
static void vboxNetFltLinuxXmitTask(void *pWork)
#endif
{
    struct sk_buff *pBuf;
    bool fActive;
    PVBOXNETFLTINS pThis;
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;

    Log4(("vboxNetFltLinuxXmitTask: Got work %p.\n", pWork));
    pThis = VBOX_FLT_XT_TO_INST(pWork);
    /*
     * Active? Retain the instance and increment the busy counter.
     */
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    fActive = ASMAtomicUoReadBool(&pThis->fActive);
    if (fActive)
        vboxNetFltRetain(pThis, true /* fBusy */);
    RTSpinlockRelease(pThis->hSpinlock, &Tmp);
    if (!fActive)
        return;

    while ((pBuf = skb_dequeue(&pThis->u.s.XmitQueue)) != 0)
        vboxNetFltLinuxForwardToIntNet(pThis, pBuf);

    vboxNetFltRelease(pThis, true /* fBusy */);
}

/**
 * Internal worker for vboxNetFltOsInitInstance and vboxNetFltOsMaybeRediscovered.
 *
 * @returns VBox status code.
 * @param   pThis           The instance.
 * @param   fRediscovery    If set we're doing a rediscovery attempt, so, don't
 *                          flood the release log.
 */
static int vboxNetFltLinuxAttachToInterface(PVBOXNETFLTINS pThis, struct net_device *pDev)
{
    struct packet_type *pt;
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;

    LogFlow(("vboxNetFltLinuxAttachToInterface: pThis=%p (%s)\n", pThis, pThis->szName));

    if (!pDev)
    {
        Log(("VBoxNetFlt: failed to find device '%s'\n", pThis->szName));
        return VERR_INTNET_FLT_IF_NOT_FOUND;
    }

    dev_hold(pDev);
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    ASMAtomicUoWritePtr((void * volatile *)&pThis->u.s.pDev, pDev);
    RTSpinlockRelease(pThis->hSpinlock, &Tmp);

    Log(("vboxNetFltLinuxAttachToInterface: Device %p(%s) retained. ref=%d\n", pDev, pDev->name, atomic_read(&pDev->refcnt)));
    Log(("vboxNetFltLinuxAttachToInterface: Got pDev=%p pThis=%p pThis->u.s.pDev=%p\n", pDev, pThis, ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pDev)));
    /*
     * Get the mac address while we still have a valid ifnet reference.
     */
    memcpy(&pThis->u.s.Mac, pDev->dev_addr, sizeof(pThis->u.s.Mac));

    pt = &pThis->u.s.PacketType;
    pt->type = __constant_htons(ETH_P_ALL);
    pt->dev  = pDev;
    pt->func = vboxNetFltLinuxPacketHandler;
    dev_add_pack(pt);
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    pDev = (struct net_device *)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pDev);
    if (pDev)
    {
        ASMAtomicUoWriteBool(&pThis->fDisconnectedFromHost, false);
        ASMAtomicUoWriteBool(&pThis->u.s.fRegistered, true);
        pDev = NULL; /* don't dereference it */
    }
    RTSpinlockRelease(pThis->hSpinlock, &Tmp);
    Log(("vboxNetFltLinuxAttachToInterface: this=%p: Packet handler installed.\n", pThis));

    /* Release the interface on failure. */
    if (pDev)
    {
        RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
        ASMAtomicUoWritePtr((void * volatile *)&pThis->u.s.pDev, NULL);
        RTSpinlockRelease(pThis->hSpinlock, &Tmp);
        dev_put(pDev);
        Log(("vboxNetFltLinuxAttachToInterface: Device %p(%s) released. ref=%d\n", pDev, pDev->name, atomic_read(&pDev->refcnt)));
    }

    LogRel(("VBoxNetFlt: attached to '%s' / %.*Rhxs\n", pThis->szName, sizeof(pThis->u.s.Mac), &pThis->u.s.Mac));
    return VINF_SUCCESS;
}


static int vboxNetFltLinuxUnregisterDevice(PVBOXNETFLTINS pThis, struct net_device *pDev)
{
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;

    Assert(!pThis->fDisconnectedFromHost);
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    ASMAtomicWriteBool(&pThis->u.s.fRegistered, false);
    ASMAtomicWriteBool(&pThis->fDisconnectedFromHost, true);
    ASMAtomicUoWritePtr((void * volatile *)&pThis->u.s.pDev, NULL);
    RTSpinlockRelease(pThis->hSpinlock, &Tmp);

    dev_remove_pack(&pThis->u.s.PacketType);
    skb_queue_purge(&pThis->u.s.XmitQueue);
    Log(("vboxNetFltLinuxUnregisterDevice: this=%p: Packet handler removed, xmit queue purged.\n", pThis));
    Log(("vboxNetFltLinuxUnregisterDevice: Device %p(%s) released. ref=%d\n", pDev, pDev->name, atomic_read(&pDev->refcnt)));
    dev_put(pDev);

    return NOTIFY_OK;
}

static int vboxNetFltLinuxDeviceIsUp(PVBOXNETFLTINS pThis, struct net_device *pDev)
{
    /* Check if we are not suspended and promiscuous mode has not been set. */
    if (ASMAtomicUoReadBool(&pThis->fActive) && !ASMAtomicUoReadBool(&pThis->u.s.fPromiscuousSet))
    {
        /* Note that there is no need for locking as the kernel got hold of the lock already. */
        dev_set_promiscuity(pDev, 1);
        ASMAtomicWriteBool(&pThis->u.s.fPromiscuousSet, true);
        Log(("vboxNetFltLinuxDeviceIsUp: enabled promiscuous mode on %s (%d)\n", pThis->szName, VBOX_GET_PCOUNT(pDev)));
    }
    else
        Log(("vboxNetFltLinuxDeviceIsUp: no need to enable promiscuous mode on %s (%d)\n", pThis->szName, VBOX_GET_PCOUNT(pDev)));
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
        Log(("vboxNetFltLinuxDeviceGoingDown: disabled promiscuous mode on %s (%d)\n", pThis->szName, VBOX_GET_PCOUNT(pDev)));
    }
    else
        Log(("vboxNetFltLinuxDeviceGoingDown: no need to disable promiscuous mode on %s (%d)\n", pThis->szName, VBOX_GET_PCOUNT(pDev)));
    return NOTIFY_OK;
}

static int vboxNetFltLinuxNotifierCallback(struct notifier_block *self, unsigned long ulEventType, void *ptr)

{
    int rc = NOTIFY_OK;
#ifdef DEBUG
    char *pszEvent = "<unknown>";
#endif
    struct net_device *pDev = (struct net_device *)ptr;
    PVBOXNETFLTINS pThis = VBOX_FLT_NB_TO_INST(self);

#ifdef DEBUG
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
    }
    Log(("VBoxNetFlt: got event %s(0x%lx) on %s, pDev=%p pThis=%p pThis->u.s.pDev=%p\n",
         pszEvent, ulEventType, pDev->name, pDev, pThis, ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pDev)));
#endif
    if (ulEventType == NETDEV_REGISTER && !strcmp(pDev->name, pThis->szName))
    {
        vboxNetFltLinuxAttachToInterface(pThis, pDev);
    }
    else
    {
        pDev = (struct net_device *)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pDev);
        if (pDev != ptr)
            return NOTIFY_OK;
        rc = NOTIFY_OK;
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
        }
    }

    return rc;
}

bool vboxNetFltOsMaybeRediscovered(PVBOXNETFLTINS pThis)
{
    return !ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost);
}

int  vboxNetFltPortOsXmit(PVBOXNETFLTINS pThis, PINTNETSG pSG, uint32_t fDst)
{
    struct net_device * pDev;
    int err;
    int rc = VINF_SUCCESS;

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


bool vboxNetFltPortOsIsPromiscuous(PVBOXNETFLTINS pThis)
{
    bool fRc = false;
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


void vboxNetFltPortOsGetMacAddress(PVBOXNETFLTINS pThis, PRTMAC pMac)
{
    *pMac = pThis->u.s.Mac;
}


bool vboxNetFltPortOsIsHostMac(PVBOXNETFLTINS pThis, PCRTMAC pMac)
{
    /* ASSUMES that the MAC address never changes. */
    return pThis->u.s.Mac.au16[0] == pMac->au16[0]
        && pThis->u.s.Mac.au16[1] == pMac->au16[1]
        && pThis->u.s.Mac.au16[2] == pMac->au16[2];
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
         * Also, we have a bit or race conditions wrt the maintance of
         * host the interface promiscuity for vboxNetFltPortOsIsPromiscuous.
         */
#ifdef LOG_ENABLED
        u_int16_t fIf;
        unsigned const cPromiscBefore = VBOX_GET_PCOUNT(pDev);
#endif
        if (fActive)
        {
            Assert(!pThis->u.s.fPromiscuousSet);

            rtnl_lock();
            dev_set_promiscuity(pDev, 1);
            rtnl_unlock();
            pThis->u.s.fPromiscuousSet = true;
            Log(("vboxNetFltPortOsSetActive: enabled promiscuous mode on %s (%d)\n", pThis->szName, VBOX_GET_PCOUNT(pDev)));
        }
        else
        {
            if (pThis->u.s.fPromiscuousSet)
            {
                rtnl_lock();
                dev_set_promiscuity(pDev, -1);
                rtnl_unlock();
                Log(("vboxNetFltPortOsSetActive: disabled promiscuous mode on %s (%d)\n", pThis->szName, VBOX_GET_PCOUNT(pDev)));
            }
            pThis->u.s.fPromiscuousSet = false;

#ifdef LOG_ENABLED
            fIf = dev_get_flags(pDev);
            Log(("VBoxNetFlt: fIf=%#x; %d->%d\n", fIf, cPromiscBefore, VBOX_GET_PCOUNT(pDev)));
#endif
        }

        vboxNetFltLinuxReleaseNetDev(pThis, pDev);
    }
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
    struct net_device *pDev;
    bool fRegistered;
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;

    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    pDev = (struct net_device *)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pDev);
    fRegistered = ASMAtomicUoReadBool(&pThis->u.s.fRegistered);
    RTSpinlockRelease(pThis->hSpinlock, &Tmp);
    if (fRegistered)
    {
        dev_remove_pack(&pThis->u.s.PacketType);
        skb_queue_purge(&pThis->u.s.XmitQueue);
        Log(("vboxNetFltOsDeleteInstance: this=%p: Packet handler removed, xmit queue purged.\n", pThis));
        Log(("vboxNetFltOsDeleteInstance: Device %p(%s) released. ref=%d\n", pDev, pDev->name, atomic_read(&pDev->refcnt)));
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
    pThis->u.s.pDev = NULL;
    pThis->u.s.fRegistered = false;
    pThis->u.s.fPromiscuousSet = false;
    memset(&pThis->u.s.PacketType, 0, sizeof(pThis->u.s.PacketType));
    skb_queue_head_init(&pThis->u.s.XmitQueue);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
    INIT_WORK(&pThis->u.s.XmitTask, vboxNetFltLinuxXmitTask);
#else
    INIT_WORK(&pThis->u.s.XmitTask, vboxNetFltLinuxXmitTask, &pThis->u.s.XmitTask);
#endif

    return VINF_SUCCESS;
}

