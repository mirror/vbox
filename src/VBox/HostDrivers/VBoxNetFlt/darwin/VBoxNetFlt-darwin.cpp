/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Darwin Specific Code.
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
/*
 * Deal with conflicts first.
 * PVM - BSD mess, that FreeBSD has correct a long time ago.
 * iprt/types.h before sys/param.h - prevents UINT32_C and friends.
 */
#include <iprt/types.h>
#include <sys/param.h>
#undef PVM

#include <IOKit/IOLib.h> /* Assert as function */

#define LOG_GROUP LOG_GROUP_NET_FLT_DRV
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/version.h>
#include <iprt/initterm.h>
#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/semaphore.h>
#include <iprt/process.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/time.h>
#include <iprt/net.h>

#include <mach/kmod.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/kern_event.h>
#include <net/kpi_interface.h>
__BEGIN_DECLS /* Buggy 10.4 headers, fixed in 10.5. */
#include <sys/kpi_mbuf.h>
#include <net/kpi_interfacefilter.h>
__END_DECLS
#include <net/if.h>

#define VBOXNETFLT_OS_SPECFIC 1
#include "../VBoxNetFltInternal.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The maximum number of SG segments.
 * Used to prevent stack overflow and similar bad stuff. */
#define VBOXNETFLT_DARWIN_MAX_SEGS      32

#if 0
/** For testing extremely segmented frames. */
#define VBOXNETFLT_DARWIN_TEST_SEG_SIZE 14
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
static kern_return_t    VBoxNetFltDarwinStart(struct kmod_info *pKModInfo, void *pvData);
static kern_return_t    VBoxNetFltDarwinStop(struct kmod_info *pKModInfo, void *pvData);
__END_DECLS


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The mbuf tag data.
 *
 * We have to associate the ethernet header with each packet we're sending
 * because things like icmp will inherit the tag it self so the tag along
 * isn't sufficent to identify our mbufs. For the icmp scenario the ethernet
 * header naturarlly changes before the packet is send pack, so let check it.
 */
typedef struct VBOXNETFLTTAG
{
    /** The ethernet header of the outgoing frame. */
    RTNETETHERHDR EthHdr;
} VBOXNETFLTTAG;
/** Pointer to a VBoxNetFlt mbuf tag. */
typedef VBOXNETFLTTAG *PVBOXNETFLTTAG;
/** Pointer to a const VBoxNetFlt mbuf tag. */
typedef VBOXNETFLTTAG const *PCVBOXNETFLTTAG;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Declare the module stuff.
 */
__BEGIN_DECLS
extern kern_return_t _start(struct kmod_info *pKModInfo, void *pvData);
extern kern_return_t _stop(struct kmod_info *pKModInfo, void *pvData);

KMOD_EXPLICIT_DECL(VBoxNetFlt, VBOX_VERSION_STRING, _start, _stop)
DECLHIDDEN(kmod_start_func_t *) _realmain = VBoxNetFltDarwinStart;
DECLHIDDEN(kmod_stop_func_t  *) _antimain = VBoxNetFltDarwinStop;
DECLHIDDEN(int)                 _kext_apple_cc = __APPLE_CC__;
__END_DECLS


/**
 * The (common) global data.
 */
static VBOXNETFLTGLOBALS g_VBoxNetFltGlobals;

/** The unique tag id for this module.
 * This is basically a unique string hash that lives on untill reboot.
 * It is used for tagging mbufs. */
static mbuf_tag_id_t g_idTag;

/** the offset of the struct ifnet::if_pcount variable. */
static unsigned g_offIfNetPCount = sizeof(void *) * (1 /*if_softc*/ + 1 /*if_name*/ + 2 /*if_link*/ + 2 /*if_addrhead*/ + 1 /*if_check_multi*/)
                                 + sizeof(u_long) /*if_refcnt*/;
/** Macro for accessing ifnet::if_pcount. */
#define VBOX_GET_PCOUNT(pIfNet) ( *(int *)((uintptr_t)pIfNet + g_offIfNetPCount) )


/**
 * Start the kernel module.
 */
static kern_return_t    VBoxNetFltDarwinStart(struct kmod_info *pKModInfo, void *pvData)
{
    int rc;

    /*
     * Initialize IPRT and find our module tag id.
     * (IPRT is shared with VBoxDrv, it creates the loggers.)
     */
    rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        Log(("VBoxNetFltDarwinStart\n"));
        errno_t err = mbuf_tag_id_find("org.VirtualBox.kext.VBoxFltDrv", &g_idTag);
        if (!err)
        {
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
                LogRel(("VBoxFltDrv: version " VBOX_VERSION_STRING " r%d\n", VBOX_SVN_REV));
                return KMOD_RETURN_SUCCESS;
            }

            LogRel(("VBoxFltDrv: failed to initialize device extension (rc=%d)\n", rc));
        }
        else
            LogRel(("VBoxFltDrv: mbuf_tag_id_find failed, err=%d\n", err));
        RTR0Term();
    }
    else
        printf("VBoxFltDrv: failed to initialize IPRT (rc=%d)\n", rc);

    memset(&g_VBoxNetFltGlobals, 0, sizeof(g_VBoxNetFltGlobals));
    return KMOD_RETURN_FAILURE;
}


/**
 * Stop the kernel module.
 */
static kern_return_t VBoxNetFltDarwinStop(struct kmod_info *pKModInfo, void *pvData)
{
    Log(("VBoxNetFltDarwinStop\n"));

    /*
     * Refuse to unload if anyone is currently using the filter driver.
     * This is important as I/O kit / xnu will to be able to do usage
     * tracking for us!
     */
    int rc = vboxNetFltTryDeleteIdcAndGlobals(&g_VBoxNetFltGlobals);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxNetFltDarwinStop - failed, busy.\n"));
        return KMOD_RETURN_FAILURE;
    }

    /*
     * Undo the work done during start (in reverse order).
     */
    memset(&g_VBoxNetFltGlobals, 0, sizeof(g_VBoxNetFltGlobals));

    RTR0Term();

    return KMOD_RETURN_SUCCESS;
}


/**
 * Reads and retains the host interface handle.
 *
 * @returns The handle, NULL if detached.
 * @param   pThis
 */
DECLINLINE(ifnet_t) vboxNetFltDarwinRetainIfNet(PVBOXNETFLTINS pThis)
{
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    ifnet_t pIfNet = NULL;

    /*
     * Be careful here to avoid problems racing the detached callback.
     */
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    if (!ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost))
    {
        pIfNet = (ifnet_t)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pIfNet);
        if (pIfNet)
            ifnet_reference(pIfNet);
    }
    RTSpinlockRelease(pThis->hSpinlock, &Tmp);

    return pIfNet;
}


/**
 * Release the host interface handle previously retained
 * by vboxNetFltDarwinRetainIfNet.
 *
 * @param   pThis           The instance.
 * @param   pIfNet          The vboxNetFltDarwinRetainIfNet return value, NULL is fine.
 */
DECLINLINE(void) vboxNetFltDarwinReleaseIfNet(PVBOXNETFLTINS pThis, ifnet_t pIfNet)
{
    NOREF(pThis);
    if (pIfNet)
        ifnet_release(pIfNet);
}


/**
 * Checks whether this is an mbuf created by vboxNetFltDarwinMBufFromSG,
 * i.e. a buffer which we're pushing and should be ignored by the filter callbacks.
 *
 * @returns true / false accordingly.
 * @param   pThis           The instance.
 * @param   pMBuf           The mbuf.
 * @param   pvFrame         The frame pointer, optional.
 */
DECLINLINE(bool) vboxNetFltDarwinMBufIsOur(PVBOXNETFLTINS pThis, mbuf_t pMBuf, void *pvFrame)
{
    NOREF(pThis);

    /*
     * Lookup the tag set by vboxNetFltDarwinMBufFromSG.
     */
    PCVBOXNETFLTTAG pTagData;
    size_t cbTagData;
    errno_t err = mbuf_tag_find(pMBuf, g_idTag, 0 /* type */, &cbTagData, (void **)&pTagData);
    if (err)
        return false;
    AssertReturn(cbTagData == sizeof(*pTagData), false);

    /*
     * Dig out the ethernet header from the mbuf.
     */
    PCRTNETETHERHDR pEthHdr = (PCRTNETETHERHDR)pvFrame;
    if (!pEthHdr)
        pEthHdr = (PCRTNETETHERHDR)mbuf_pkthdr_header(pMBuf);
    if (!pEthHdr)
        pEthHdr = (PCRTNETETHERHDR)mbuf_data(pMBuf);
    /* ASSUMING that there is enough data to work on! */
    if (    pEthHdr->DstMac.au8[0] != pTagData->EthHdr.DstMac.au8[0]
        ||  pEthHdr->DstMac.au8[1] != pTagData->EthHdr.DstMac.au8[1]
        ||  pEthHdr->DstMac.au8[2] != pTagData->EthHdr.DstMac.au8[2]
        ||  pEthHdr->DstMac.au8[3] != pTagData->EthHdr.DstMac.au8[3]
        ||  pEthHdr->DstMac.au8[4] != pTagData->EthHdr.DstMac.au8[4]
        ||  pEthHdr->DstMac.au8[5] != pTagData->EthHdr.DstMac.au8[5]
        ||  pEthHdr->SrcMac.au8[0] != pTagData->EthHdr.SrcMac.au8[0]
        ||  pEthHdr->SrcMac.au8[1] != pTagData->EthHdr.SrcMac.au8[1]
        ||  pEthHdr->SrcMac.au8[2] != pTagData->EthHdr.SrcMac.au8[2]
        ||  pEthHdr->SrcMac.au8[3] != pTagData->EthHdr.SrcMac.au8[3]
        ||  pEthHdr->SrcMac.au8[4] != pTagData->EthHdr.SrcMac.au8[4]
        ||  pEthHdr->SrcMac.au8[5] != pTagData->EthHdr.SrcMac.au8[5]
        ||  pEthHdr->EtherType     != pTagData->EthHdr.EtherType)
    {
        Log3(("tagged, but the ethernet header has changed\n"));
        return false;
    }

    return true;
}


/**
 * Internal worker that create a darwin mbuf for a (scatter/)gather list.
 *
 * @returns Pointer to the mbuf.
 * @param   pThis           The instance.
 * @param   pSG             The (scatter/)gather list.
 */
static mbuf_t vboxNetFltDarwinMBufFromSG(PVBOXNETFLTINS pThis, PINTNETSG pSG)
{
    /// @todo future? mbuf_how_t How = preemtion enabled ? MBUF_DONTWAIT : MBUF_WAITOK;
    mbuf_how_t How = MBUF_WAITOK;

    /*
     * We can't make use of the physical addresses on darwin because the way the
     * mbuf / cluster stuffe works (see mbuf_data_to_physical and mcl_to_paddr).
     * So, because we're lazy, we will ASSUME that all SGs coming from INTNET
     * will only contain one single segment.
     */
    Assert(pSG->cSegsUsed == 1);
    Assert(pSG->cbTotal == pSG->aSegs[0].cb);
    Assert(pSG->cbTotal > 0);

    /*
     * We need some way of getting back to our instance data when
     * the mbuf is freed, so use pvUserData for this.
     *  -- this is not relevant anylonger! --
     */
    Assert(!pSG->pvUserData || pSG->pvUserData == pThis);
    Assert(!pSG->pvUserData2);
    pSG->pvUserData = pThis;

    /*
     * Allocate a packet and copy over the data.
     *
     * Using mbuf_attachcluster() here would've been nice but there are two
     * issues with it: (1) it's 10.5.x only, and (2) the documentation indicates
     * that it's not supposed to be used for really external buffers. The 2nd
     * point might be argued against considering that the only m_clattach user
     * is mallocs memory for the ext mbuf and not doing what's stated in the docs.
     * However, it's hard to tell if these m_clattach buffers actually makes it
     * to the NICs or not, and even if they did, the NIC would need the physical
     * addresses for the pages they contain and might end up copying the data
     * to a new mbuf anyway.
     *
     * So, in the end it's better to just do it the simple way that will work
     * 100%, even if it involes some extra work (alloc + copy) we really wished
     * to avoid.
     */
    mbuf_t pPkt = NULL;
    errno_t err = mbuf_allocpacket(How, pSG->cbTotal, NULL, &pPkt);
    if (!err)
    {
        /* Skip zero sized memory buffers (paranoia). */
        mbuf_t pCur = pPkt;
        while (pCur && !mbuf_maxlen(pCur))
            pCur = mbuf_next(pCur);
        Assert(pCur);

        /* Set the required packet header attributes. */
        mbuf_pkthdr_setlen(pPkt, pSG->cbTotal);
        mbuf_pkthdr_setheader(pPkt, mbuf_data(pCur));

        /* Special case the single buffer copy. */
        if (    mbuf_next(pCur)
            &&  mbuf_maxlen(pCur) >= pSG->cbTotal)
        {
            mbuf_setlen(pCur, pSG->cbTotal);
            memcpy(mbuf_data(pCur), pSG->aSegs[0].pv, pSG->cbTotal);
        }
        else
        {
            /* Multi buffer copying. */
            size_t         cbSrc = pSG->cbTotal;
            uint8_t const *pbSrc = (uint8_t const *)pSG->aSegs[0].pv;
            while (cbSrc > 0 && pCur)
            {
                size_t cb = mbuf_maxlen(pCur);
                if (cbSrc < cb)
                    cb = cbSrc;
                mbuf_setlen(pCur, cb);
                memcpy(mbuf_data(pCur), pbSrc, cb);

                /* advance */
                pbSrc += cb;
                cbSrc -= cb;
                pCur = mbuf_next(pCur);
            }
        }
        if (!err)
        {
            /*
             * Tag the packet and return successfully.
             */
            PVBOXNETFLTTAG pTagData;
            err = mbuf_tag_allocate(pPkt, g_idTag, 0 /* type */, sizeof(VBOXNETFLTTAG) /* tag len */, How, (void **)&pTagData);
            if (!err)
            {
                Assert(pSG->aSegs[0].cb >= sizeof(pTagData->EthHdr));
                memcpy(&pTagData->EthHdr, pSG->aSegs[0].pv, sizeof(pTagData->EthHdr));
                return pPkt;
            }

            /* bailout: */
            AssertMsg(err == ENOMEM || err == EWOULDBLOCK, ("err=%d\n", err));
        }

        mbuf_freem(pPkt);
    }
    else
        AssertMsg(err == ENOMEM || err == EWOULDBLOCK, ("err=%d\n", err));
    pSG->pvUserData = NULL;

    return NULL;
}


/**
 * Calculates the number of segments required to represent the mbuf.
 *
 * @returns Number of segments.
 * @param   pThis               The instance.
 * @param   pMBuf               The mbuf.
 * @param   pvFrame             The frame pointer, optional.
 */
DECLINLINE(unsigned) vboxNetFltDarwinMBufCalcSGSegs(PVBOXNETFLTINS pThis, mbuf_t pMBuf, void *pvFrame)
{
    NOREF(pThis);

    /*
     * Count the buffers in the chain.
     */
    unsigned cSegs = 0;
    for (mbuf_t pCur = pMBuf; pCur; pCur = mbuf_next(pCur))
        if (mbuf_len(pCur))
            cSegs++;
        else if (   !cSegs
                 && pvFrame
                 && (uintptr_t)pvFrame - (uintptr_t)mbuf_datastart(pMBuf) < mbuf_maxlen(pMBuf))
            cSegs++;

#ifdef PADD_RUNT_FRAMES_FROM_HOST
    /*
     * Add one buffer if the total is less than the ethernet minimum 60 bytes.
     * This may allocate a segment too much if the ethernet header is separated,
     * but that shouldn't harm us much.
     */
    if (mbuf_pkthdr_len(pMBuf) < 60)
        cSegs++;
#endif

#ifdef VBOXNETFLT_DARWIN_TEST_SEG_SIZE
    /* maximize the number of segments. */
    cSegs = RT_MAX(VBOXNETFLT_DARWIN_MAX_SEGS - 1, cSegs);
#endif

    return cSegs ? cSegs : 1;
}


/**
 * Initializes a SG list from an mbuf.
 *
 * @returns Number of segments.
 * @param   pThis               The instance.
 * @param   pMBuf               The mbuf.
 * @param   pSG                 The SG.
 * @param   pvFrame             The frame pointer, optional.
 * @param   cSegs               The number of segments allocated for the SG.
 *                              This should match the number in the mbuf exactly!
 * @param   fSrc                The source of the frame.
 */
DECLINLINE(void) vboxNetFltDarwinMBufToSG(PVBOXNETFLTINS pThis, mbuf_t pMBuf, void *pvFrame, PINTNETSG pSG, unsigned cSegs, uint32_t fSrc)
{
    NOREF(pThis);

    pSG->pvOwnerData = NULL;
    pSG->pvUserData = NULL;
    pSG->pvUserData2 = NULL;
    pSG->cUsers = 1;
    pSG->fFlags = INTNETSG_FLAGS_TEMP;
    pSG->cSegsAlloc = cSegs;

    /*
     * Walk the chain and convert the buffers to segments.
     */
    unsigned iSeg = 0;
    pSG->cbTotal = 0;
    for (mbuf_t pCur = pMBuf; pCur; pCur = mbuf_next(pCur))
    {
        size_t cbSeg = mbuf_len(pCur);
        if (cbSeg)
        {
            void *pvSeg = mbuf_data(pCur);

            /* deal with pvFrame */
            if (!iSeg && pvFrame && pvFrame != pvSeg)
            {
                void     *pvStart   = mbuf_datastart(pMBuf);
                uintptr_t offSeg    = (uintptr_t)pvSeg - (uintptr_t)pvStart;
                uintptr_t offSegEnd = offSeg + cbSeg;
                Assert(pvStart && pvSeg && offSeg < mbuf_maxlen(pMBuf) && offSegEnd <= mbuf_maxlen(pMBuf)); NOREF(offSegEnd);
                uintptr_t offFrame  = (uintptr_t)pvFrame - (uintptr_t)pvStart;
                if (RT_LIKELY(offFrame < offSeg))
                {
                    pvSeg = pvFrame;
                    cbSeg += offSeg - offFrame;
                }
                else
                    AssertMsgFailed(("pvFrame=%p pvStart=%p pvSeg=%p offSeg=%p cbSeg=%#zx offSegEnd=%p offFrame=%p maxlen=%#zx\n",
                                     pvFrame, pvStart, pvSeg, offSeg, cbSeg, offSegEnd, offFrame, mbuf_maxlen(pMBuf)));
                pvFrame = NULL;
            }

            AssertBreak(iSeg < cSegs);
            pSG->cbTotal += cbSeg;
            pSG->aSegs[iSeg].cb = cbSeg;
            pSG->aSegs[iSeg].pv = pvSeg;
            pSG->aSegs[iSeg].Phys = NIL_RTHCPHYS;
            iSeg++;
        }
        /* The pvFrame might be in a now empty buffer. */
        else if (   !iSeg
                 && pvFrame
                 && (uintptr_t)pvFrame - (uintptr_t)mbuf_datastart(pMBuf) < mbuf_maxlen(pMBuf))
        {
            cbSeg = (uintptr_t)mbuf_datastart(pMBuf) + mbuf_maxlen(pMBuf) - (uintptr_t)pvFrame;
            pSG->cbTotal += cbSeg;
            pSG->aSegs[iSeg].cb = cbSeg;
            pSG->aSegs[iSeg].pv = pvFrame;
            pSG->aSegs[iSeg].Phys = NIL_RTHCPHYS;
            iSeg++;
            pvFrame = NULL;
        }
    }

    Assert(iSeg && iSeg <= cSegs);
    pSG->cSegsUsed = iSeg;

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
        AssertReturnVoid(iSeg < cSegs);

        static uint8_t const s_abZero[128] = {0};
        pSG->aSegs[iSeg].Phys = NIL_RTHCPHYS;
        pSG->aSegs[iSeg].pv = (void *)&s_abZero[0];
        pSG->aSegs[iSeg].cb = 60 - pSG->cbTotal;
        pSG->cbTotal = 60;
        pSG->cSegsUsed++;
    }
#endif

#ifdef VBOXNETFLT_DARWIN_TEST_SEG_SIZE
    /*
     * Redistribute the segments.
     */
    if (pSG->cSegsUsed < pSG->cSegsAlloc)
    {
        /* copy the segments to the end. */
        int iSrc = pSG->cSegsUsed;
        int iDst = pSG->cSegsAlloc;
        while (iSrc > 0)
        {
            iDst--;
            iSrc--;
            pSG->aSegs[iDst] = pSG->aSegs[iSrc];
        }

        /* create small segments from the start. */
        pSG->cSegsUsed = pSG->cSegsAlloc;
        iSrc = iDst;
        iDst = 0;
        while (     iDst < iSrc
               &&   iDst < pSG->cSegsAlloc)
        {
            pSG->aSegs[iDst].Phys = NIL_RTHCPHYS;
            pSG->aSegs[iDst].pv = pSG->aSegs[iSrc].pv;
            pSG->aSegs[iDst].cb = RT_MIN(pSG->aSegs[iSrc].cb, VBOXNETFLT_DARWIN_TEST_SEG_SIZE);
            if (pSG->aSegs[iDst].cb != pSG->aSegs[iSrc].cb)
            {
                pSG->aSegs[iSrc].cb -= pSG->aSegs[iDst].cb;
                pSG->aSegs[iSrc].pv = (uint8_t *)pSG->aSegs[iSrc].pv + pSG->aSegs[iDst].cb;
            }
            else if (++iSrc >= pSG->cSegsAlloc)
            {
                pSG->cSegsUsed = iDst + 1;
                break;
            }
            iDst++;
        }
    }
#endif

    AssertMsg(!pvFrame, ("pvFrame=%p pMBuf=%p iSeg=%d\n", pvFrame, pMBuf, iSeg));
}


/**
 *
 * @see iff_detached_func in the darwin kpi.
 */
static void vboxNetFltDarwinIffDetached(void *pvThis, ifnet_t pIfNet)
{
    PVBOXNETFLTINS pThis = (PVBOXNETFLTINS)pvThis;
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    uint64_t NanoTS = RTTimeSystemNanoTS();
    LogFlow(("vboxNetFltDarwinIffDetached: pThis=%p NanoTS=%RU64 (%d)\n",
             pThis, NanoTS, VALID_PTR(pIfNet) ? VBOX_GET_PCOUNT(pIfNet) :  -1));

    Assert(!pThis->fDisconnectedFromHost);
    Assert(!pThis->fRediscoveryPending);

    /*
     * If we've put it into promiscuous mode, undo that now. If we don't
     * the if_pcount will go all wrong when it's replugged.
     */
    if (ASMAtomicXchgBool(&pThis->u.s.fSetPromiscuous, false))
        ifnet_set_promiscuous(pIfNet, 0);

    /*
     * We carefully take the spinlock and increase the interface reference
     * behind it in order to avoid problematic races with the detached callback.
     */
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);

    pIfNet = (ifnet_t)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pIfNet);
    int cPromisc = VALID_PTR(pIfNet) ? VBOX_GET_PCOUNT(pIfNet) : - 1;

    ASMAtomicUoWritePtr((void * volatile *)&pThis->u.s.pIfNet, NULL);
    ASMAtomicUoWritePtr((void * volatile *)&pThis->u.s.pIfFilter, NULL);
    ASMAtomicWriteBool(&pThis->u.s.fNeedSetPromiscuous, false);
    pThis->u.s.fSetPromiscuous = false;
    ASMAtomicUoWriteU64(&pThis->NanoTSLastRediscovery, NanoTS);
    ASMAtomicUoWriteBool(&pThis->fRediscoveryPending, false);
    ASMAtomicWriteBool(&pThis->fDisconnectedFromHost, true);

    RTSpinlockRelease(pThis->hSpinlock, &Tmp);

    if (pIfNet)
        ifnet_release(pIfNet);
    LogRel(("VBoxNetFlt: was detached from '%s' (%d)\n", pThis->szName, cPromisc));
}


/**
 *
 * @see iff_ioctl_func in the darwin kpi.
 */
static errno_t vboxNetFltDarwinIffIoCtl(void *pvThis, ifnet_t pIfNet, protocol_family_t eProtocol, u_long uCmd, void *pvArg)
{
    PVBOXNETFLTINS pThis = (PVBOXNETFLTINS)pvThis;
    LogFlow(("vboxNetFltDarwinIffIoCtl: pThis=%p uCmd=%lx\n", pThis, uCmd));

    /*
     * Update fOtherPromiscuous.
     */
    /** @todo we'll have to find the offset of if_pcount to get this right! */
    //if (uCmd == SIOCSIFFLAGS)
    //{
    //
    //}

    /*
     * We didn't handle it, continue processing.
     */
    NOREF(pThis);
    NOREF(eProtocol);
    NOREF(uCmd);
    NOREF(pvArg);
    return EOPNOTSUPP;
}


/**
 *
 * @see iff_event_func in the darwin kpi.
 */
static void vboxNetFltDarwinIffEvent(void *pvThis, ifnet_t pIfNet, protocol_family_t eProtocol, const struct kev_msg *pEvMsg)
{
    PVBOXNETFLTINS pThis = (PVBOXNETFLTINS)pvThis;
    LogFlow(("vboxNetFltDarwinIffEvent: pThis=%p\n", pThis));

    NOREF(pThis);
    NOREF(pIfNet);
    NOREF(eProtocol);
    NOREF(pEvMsg);

    /*
     * Watch out for the interface going online / offline.
     */
    if (    VALID_PTR(pThis)
        &&  VALID_PTR(pEvMsg)
        &&  pEvMsg->vendor_code  == KEV_VENDOR_APPLE
        &&  pEvMsg->kev_class    == KEV_NETWORK_CLASS
        &&  pEvMsg->kev_subclass == KEV_DL_SUBCLASS)
    {
        if (pThis->u.s.pIfNet    == pIfNet)
        {
            if (pEvMsg->event_code == KEV_DL_LINK_ON)
            {
                if (ASMAtomicUoReadBool(&pThis->u.s.fNeedSetPromiscuous))
                {
                    /* failed to bring it online. */
                    errno_t err = ifnet_set_promiscuous(pIfNet, 1);
                    if (!err)
                    {
                        ASMAtomicWriteBool(&pThis->u.s.fSetPromiscuous, true);
                        ASMAtomicWriteBool(&pThis->u.s.fNeedSetPromiscuous, false);
                        Log(("vboxNetFltDarwinIffEvent: enabled promiscuous mode on %s (%d)\n", pThis->szName, VBOX_GET_PCOUNT(pIfNet)));
                    }
                    else
                        Log(("vboxNetFltDarwinIffEvent: ifnet_set_promiscuous failed on %s, err=%d (%d)\n", pThis->szName, err, VBOX_GET_PCOUNT(pIfNet)));
                }
                else if (   ASMAtomicUoReadBool(&pThis->u.s.fSetPromiscuous)
                         && !(ifnet_flags(pIfNet) & IFF_PROMISC))
                {
                    /* Try fix the inconsistency. */
                    errno_t err = ifnet_set_flags(pIfNet, IFF_PROMISC, IFF_PROMISC);
                    if (!err)
                        err = ifnet_ioctl(pIfNet, 0, SIOCSIFFLAGS, NULL);
                    if (!err && (ifnet_flags(pIfNet) & IFF_PROMISC))
                        Log(("vboxNetFltDarwinIffEvent: fixed IFF_PROMISC on %s (%d)\n", pThis->szName, VBOX_GET_PCOUNT(pIfNet)));
                    else
                        Log(("vboxNetFltDarwinIffEvent: failed to fix IFF_PROMISC on %s, err=%d flags=%#x (%d)\n",
                             pThis->szName, err, ifnet_flags(pIfNet), VBOX_GET_PCOUNT(pIfNet)));
                }
                else
                    Log(("vboxNetFltDarwinIffEvent: online, '%s'. flags=%#x (%d)\n", pThis->szName, ifnet_flags(pIfNet), VBOX_GET_PCOUNT(pIfNet)));
            }
            else if (pEvMsg->event_code == KEV_DL_LINK_OFF)
                Log(("vboxNetFltDarwinIffEvent: %s goes down (%d)\n", pThis->szName, VBOX_GET_PCOUNT(pIfNet)));
        }
        else
            Log(("vboxNetFltDarwinIffEvent: pThis->u.s.pIfNet=%p pIfNet=%p (%d)\n", pThis->u.s.pIfNet, pIfNet, VALID_PTR(pIfNet) ? VBOX_GET_PCOUNT(pIfNet) : -1));
    }
    else if (VALID_PTR(pEvMsg))
        Log(("vboxNetFltDarwinIffEvent: vendor_code=%#x kev_class=%#x kev_subclass=%#x event_code=%#x\n",
             pEvMsg->vendor_code, pEvMsg->kev_class, pEvMsg->kev_subclass, pEvMsg->event_code));
}


/**
 * Internal worker for  vboxNetFltDarwinIffInput and vboxNetFltDarwinIffOutput,
 *
 * @returns 0 or EJUSTRETURN.
 * @param   pThis           The instance.
 * @param   pMBuf           The mbuf.
 * @param   pvFrame         The start of the frame, optional.
 * @param   fSrc            Where the packet (allegedly) comes from, one INTNETTRUNKDIR_* value.
 * @param   eProtocol       The protocol.
 */
static errno_t vboxNetFltDarwinIffInputOutputWorker(PVBOXNETFLTINS pThis, mbuf_t pMBuf, void *pvFrame,
                                                    uint32_t fSrc, protocol_family_t eProtocol)
{
    /*
     * Drop it immediately?
     */
    Log2(("vboxNetFltDarwinIffInputOutputWorker: pThis=%p pMBuf=%p pvFrame=%p fSrc=%#x cbPkt=%x\n",
          pThis, pMBuf, pvFrame, fSrc, pMBuf ? mbuf_pkthdr_len(pMBuf) : -1));
    if (!pMBuf)
        return 0;
#if 0 /* debugging lost icmp packets */
    if (mbuf_pkthdr_len(pMBuf) > 0x300)
    {
        uint8_t *pb = (uint8_t *)(pvFrame ? pvFrame : mbuf_data(pMBuf));
        Log3(("D=%.6Rhxs  S=%.6Rhxs  T=%04x IFF\n", pb, pb + 6, RT_BE2H_U16(*(uint16_t *)(pb + 12))));
    }
#endif
    if (vboxNetFltDarwinMBufIsOur(pThis, pMBuf, pvFrame))
        return 0;

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
        return 0;

    /*
     * Finalize out-bound packets since the stack puts off finalizing
     * TCP/IP checksums as long as possible.
     * ASSUMES this only applies to outbound IP packets.
     */
    if (    (fSrc & INTNETTRUNKDIR_HOST)
        &&  eProtocol == PF_INET)
    {
        Assert(!pvFrame);
        mbuf_outbound_finalize(pMBuf, eProtocol, sizeof(RTNETETHERHDR));
    }

    /*
     * Create a (scatter/)gather list for the mbuf and feed it to the internal network.
     */
    bool fDropIt = false;
    unsigned cSegs = vboxNetFltDarwinMBufCalcSGSegs(pThis, pMBuf, pvFrame);
    if (cSegs < VBOXNETFLT_DARWIN_MAX_SEGS)
    {
        PINTNETSG pSG = (PINTNETSG)alloca(RT_OFFSETOF(INTNETSG, aSegs[cSegs]));
        vboxNetFltDarwinMBufToSG(pThis, pMBuf, pvFrame, pSG, cSegs, fSrc);

        fDropIt = pThis->pSwitchPort->pfnRecv(pThis->pSwitchPort, pSG, fSrc);
        if (fDropIt)
            mbuf_freem(pMBuf);
    }

    vboxNetFltRelease(pThis, true /* fBusy */);

    return fDropIt ? EJUSTRETURN : 0;
}


/**
 * From the host.
 *
 * @see iff_output_func in the darwin kpi.
 */
static errno_t vboxNetFltDarwinIffOutput(void *pvThis, ifnet_t pIfNet, protocol_family_t eProtocol, mbuf_t *ppMBuf)
{
    /** @todo there was some note about the ethernet header here or something like that... */

    NOREF(eProtocol);
    NOREF(pIfNet);
    return vboxNetFltDarwinIffInputOutputWorker((PVBOXNETFLTINS)pvThis, *ppMBuf, NULL, INTNETTRUNKDIR_HOST, eProtocol);
}


/**
 * From the wire.
 *
 * @see iff_input_func in the darwin kpi.
 */
static errno_t vboxNetFltDarwinIffInput(void *pvThis, ifnet_t pIfNet, protocol_family_t eProtocol, mbuf_t *ppMBuf, char **ppchFrame)
{
    NOREF(eProtocol);
    NOREF(pIfNet);
    return vboxNetFltDarwinIffInputOutputWorker((PVBOXNETFLTINS)pvThis, *ppMBuf, *ppchFrame, INTNETTRUNKDIR_WIRE, eProtocol);
}


/**
 * Internal worker for vboxNetFltOsInitInstance and vboxNetFltOsMaybeRediscovered.
 *
 * @returns VBox status code.
 * @param   pThis           The instance.
 * @param   fRediscovery    If set we're doing a rediscovery attempt, so, don't
 *                          flood the release log.
 */
static int vboxNetFltDarwinAttachToInterface(PVBOXNETFLTINS pThis, bool fRediscovery)
{
    LogFlow(("vboxNetFltDarwinAttachToInterface: pThis=%p (%s)\n", pThis, pThis->szName));

    /*
     * Locate the interface first.
     *
     * The pIfNet member is updated before iflt_attach is called and used
     * to deal with the hypothetical case where someone rips out the
     * interface immediately after our iflt_attach call.
     */
    ifnet_t pIfNet = NULL;
    errno_t err = ifnet_find_by_name(pThis->szName, &pIfNet);
    if (err)
    {
        Assert(err == ENXIO);
        if (!fRediscovery)
            LogRel(("VBoxFltDrv: failed to find ifnet '%s' (err=%d)\n", pThis->szName, err));
        else
            Log(("VBoxFltDrv: failed to find ifnet '%s' (err=%d)\n", pThis->szName, err));
        return VERR_INTNET_FLT_IF_NOT_FOUND;
    }

    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    ASMAtomicUoWritePtr((void * volatile *)&pThis->u.s.pIfNet, pIfNet);
    RTSpinlockRelease(pThis->hSpinlock, &Tmp);

    /*
     * Get the mac address while we still have a valid ifnet reference.
     */
    err = ifnet_lladdr_copy_bytes(pIfNet, &pThis->u.s.Mac, sizeof(pThis->u.s.Mac));
    if (!err)
    {
        /*
         * Try attach the filter.
         */
        struct iff_filter RegRec;
        RegRec.iff_cookie   = pThis;
        RegRec.iff_name     = "VBoxNetFlt";
        RegRec.iff_protocol = 0;
        RegRec.iff_input    = vboxNetFltDarwinIffInput;
        RegRec.iff_output   = vboxNetFltDarwinIffOutput;
        RegRec.iff_event    = vboxNetFltDarwinIffEvent;
        RegRec.iff_ioctl    = vboxNetFltDarwinIffIoCtl;
        RegRec.iff_detached = vboxNetFltDarwinIffDetached;
        interface_filter_t pIfFilter = NULL;
        err = iflt_attach(pIfNet, &RegRec, &pIfFilter);
        Assert(err || pIfFilter);

        RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
        pIfNet = (ifnet_t)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pIfNet);
        if (pIfNet && !err)
        {
            ASMAtomicUoWriteBool(&pThis->fDisconnectedFromHost, false);
            ASMAtomicUoWritePtr((void * volatile *)&pThis->u.s.pIfFilter, pIfFilter);
            pIfNet = NULL; /* don't dereference it */
        }
        RTSpinlockRelease(pThis->hSpinlock, &Tmp);
    }

    /* Release the interface on failure. */
    if (pIfNet)
        ifnet_release(pIfNet);

    int rc = RTErrConvertFromErrno(err);
    if (RT_SUCCESS(rc))
        LogRel(("VBoxFltDrv: attached to '%s' / %.*Rhxs\n", pThis->szName, sizeof(pThis->u.s.Mac), &pThis->u.s.Mac));
    else
        LogRel(("VBoxFltDrv: failed to attach to ifnet '%s' (err=%d)\n", pThis->szName, err));
    return rc;
}


bool vboxNetFltOsMaybeRediscovered(PVBOXNETFLTINS pThis)
{
    vboxNetFltDarwinAttachToInterface(pThis, true /* fRediscovery */);
    return !ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost);
}


int  vboxNetFltPortOsXmit(PVBOXNETFLTINS pThis, PINTNETSG pSG, uint32_t fDst)
{
    int rc = VINF_SUCCESS;
    ifnet_t pIfNet = vboxNetFltDarwinRetainIfNet(pThis);
    if (pIfNet)
    {
        /*
         * Create a mbuf for the gather list and push it onto the wire.
         */
        if (fDst & INTNETTRUNKDIR_WIRE)
        {
            mbuf_t pMBuf = vboxNetFltDarwinMBufFromSG(pThis, pSG);
            if (pMBuf)
            {
                errno_t err = ifnet_output_raw(pIfNet, PF_LINK, pMBuf);
                if (err)
                    rc = RTErrConvertFromErrno(err);
            }
            else
                rc = VERR_NO_MEMORY;
        }

        /*
         * Create a mbuf for the gather list and push it onto the host stack.
         */
        if (fDst & INTNETTRUNKDIR_HOST)
        {
            mbuf_t pMBuf = vboxNetFltDarwinMBufFromSG(pThis, pSG);
            if (pMBuf)
            {
                /* This is what IONetworkInterface::inputPacket does. */
                unsigned const cbEthHdr = 14;
                mbuf_pkthdr_setheader(pMBuf, mbuf_data(pMBuf));
                mbuf_pkthdr_setlen(pMBuf, mbuf_pkthdr_len(pMBuf) - cbEthHdr);
                mbuf_setdata(pMBuf, (uint8_t *)mbuf_data(pMBuf) + cbEthHdr, mbuf_len(pMBuf) - cbEthHdr);
                mbuf_pkthdr_setrcvif(pMBuf, pIfNet); /* will crash without this. */

                errno_t err = ifnet_input(pIfNet, pMBuf, NULL);
                if (err)
                    rc = RTErrConvertFromErrno(err);
            }
            else
                rc = VERR_NO_MEMORY;
        }

        vboxNetFltDarwinReleaseIfNet(pThis, pIfNet);
    }

    return rc;
}


bool vboxNetFltPortOsIsPromiscuous(PVBOXNETFLTINS pThis)
{
    bool fRc = false;
    ifnet_t pIfNet = vboxNetFltDarwinRetainIfNet(pThis);
    if (pIfNet)
    {
        /* gather the data */
        uint16_t fIf = ifnet_flags(pIfNet);
        unsigned cPromisc = VBOX_GET_PCOUNT(pIfNet);
        bool fSetPromiscuous = ASMAtomicUoReadBool(&pThis->u.s.fSetPromiscuous);
        vboxNetFltDarwinReleaseIfNet(pThis, pIfNet);

        /* calc the return. */
        fRc = (fIf & IFF_PROMISC)
           && cPromisc > fSetPromiscuous;
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
    ifnet_t pIfNet = vboxNetFltDarwinRetainIfNet(pThis);
    if (pIfNet)
    {
        if (pThis->fDisablePromiscuous)
        {
            /*
             * Promiscuous mode should not be used (wireless), we just need to
             * make sure the interface is up.
             */
            if (fActive)
            {
                u_int16_t fIf = ifnet_flags(pIfNet);
                if ((fIf & (IFF_UP | IFF_RUNNING)) != (IFF_UP | IFF_RUNNING))
                {
                    ifnet_set_flags(pIfNet, IFF_UP, IFF_UP);
                    ifnet_ioctl(pIfNet, 0, SIOCSIFFLAGS, NULL);
                }
            }
        }
        else
        {
            /*
             * This api is a bit weird, the best reference is the code.
             *
             * Also, we have a bit or race conditions wrt the maintance of
             * host the interface promiscuity for vboxNetFltPortOsIsPromiscuous.
             */
            unsigned const cPromiscBefore = VBOX_GET_PCOUNT(pIfNet);
            u_int16_t fIf;
            if (fActive)
            {
                Assert(!pThis->u.s.fSetPromiscuous);
                errno_t err = ENETDOWN;
                ASMAtomicWriteBool(&pThis->u.s.fNeedSetPromiscuous, true);

                /*
                 * Try bring the interface up and running if it's down.
                 */
                fIf = ifnet_flags(pIfNet);
                if ((fIf & (IFF_UP | IFF_RUNNING)) != (IFF_UP | IFF_RUNNING))
                {
                    err = ifnet_set_flags(pIfNet, IFF_UP, IFF_UP);
                    errno_t err2 = ifnet_ioctl(pIfNet, 0, SIOCSIFFLAGS, NULL);
                    if (!err)
                        err = err2;
                    fIf = ifnet_flags(pIfNet);
                }

                /*
                 * Is it already up?  If it isn't, leave it to the link event or
                 * we'll upset if_pcount (as stated above, ifnet_set_promiscuous is weird).
                 */
                if ((fIf & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
                {
                    err = ifnet_set_promiscuous(pIfNet, 1);
                    pThis->u.s.fSetPromiscuous = err == 0;
                    if (!err)
                    {
                        ASMAtomicWriteBool(&pThis->u.s.fNeedSetPromiscuous, false);

                        /* check if it actually worked, this stuff is not always behaving well. */
                        if (!(ifnet_flags(pIfNet) & IFF_PROMISC))
                        {
                            err = ifnet_set_flags(pIfNet, IFF_PROMISC, IFF_PROMISC);
                            if (!err)
                                err = ifnet_ioctl(pIfNet, 0, SIOCSIFFLAGS, NULL);
                            if (!err)
                                Log(("vboxNetFlt: fixed IFF_PROMISC on %s (%d->%d)\n", pThis->szName, cPromiscBefore, VBOX_GET_PCOUNT(pIfNet)));
                            else
                                Log(("VBoxNetFlt: failed to fix IFF_PROMISC on %s, err=%d (%d->%d)\n",
                                     pThis->szName, err, cPromiscBefore, VBOX_GET_PCOUNT(pIfNet)));
                        }
                    }
                    else
                        Log(("VBoxNetFlt: ifnet_set_promiscuous -> err=%d grr! (%d->%d)\n", err, cPromiscBefore, VBOX_GET_PCOUNT(pIfNet)));
                }
                else if (!err)
                    Log(("VBoxNetFlt: Waiting for the link to come up... (%d->%d)\n", cPromiscBefore, VBOX_GET_PCOUNT(pIfNet)));
                if (err)
                    LogRel(("VBoxNetFlt: Failed to put '%s' into promiscuous mode, err=%d (%d->%d)\n", pThis->szName, err, cPromiscBefore, VBOX_GET_PCOUNT(pIfNet)));
            }
            else
            {
                ASMAtomicWriteBool(&pThis->u.s.fNeedSetPromiscuous, false);
                if (pThis->u.s.fSetPromiscuous)
                {
                    errno_t err = ifnet_set_promiscuous(pIfNet, 0);
                    AssertMsg(!err, ("%d\n", err)); NOREF(err);
                }
                pThis->u.s.fSetPromiscuous = false;

                fIf = ifnet_flags(pIfNet);
                Log(("VBoxNetFlt: fIf=%#x; %d->%d\n", fIf, cPromiscBefore, VBOX_GET_PCOUNT(pIfNet)));
            }
        }

        vboxNetFltDarwinReleaseIfNet(pThis, pIfNet);
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
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    interface_filter_t pIfFilter;

    /*
     * Carefully obtain the interface filter reference and detach it.
     */
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    pIfFilter = (interface_filter_t)ASMAtomicUoReadPtr((void * volatile *)&pThis->u.s.pIfFilter);
    if (pIfFilter)
        ASMAtomicUoWritePtr((void * volatile *)&pThis->u.s.pIfFilter, NULL);
    RTSpinlockRelease(pThis->hSpinlock, &Tmp);

    if (pIfFilter)
        iflt_detach(pIfFilter);
}


int  vboxNetFltOsInitInstance(PVBOXNETFLTINS pThis, void *pvContext)
{
    NOREF(pvContext);
    return vboxNetFltDarwinAttachToInterface(pThis, false /* fRediscovery */);
}


int  vboxNetFltOsPreInitInstance(PVBOXNETFLTINS pThis)
{
    /*
     * Init the darwin specific members.
     */
    pThis->u.s.pIfNet = NULL;
    pThis->u.s.pIfFilter = NULL;
    pThis->u.s.fSetPromiscuous = false;
    pThis->u.s.fNeedSetPromiscuous = false;
    //pThis->u.s.Mac = {0};

    return VINF_SUCCESS;
}

