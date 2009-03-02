/* $Id$ */
/** @file
 * VBoxNetAdp - Virtual Network Adapter Driver (Host), Darwin Specific Code.
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
/*
 * Deal with conflicts first.
 * PVM - BSD mess, that FreeBSD has correct a long time ago.
 * iprt/types.h before sys/param.h - prevents UINT32_C and friends.
 */
#include <iprt/types.h>
#include <sys/param.h>
#undef PVM

#define LOG_GROUP LOG_GROUP_NET_ADP_DRV
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/version.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/alloca.h>

#include <sys/systm.h>
__BEGIN_DECLS /* Buggy 10.4 headers, fixed in 10.5. */
#include <sys/kpi_mbuf.h>
__END_DECLS

#include <net/ethernet.h>
#include <net/if_ether.h>
#include <net/if_types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <sys/errno.h>
#include <sys/param.h>

#define VBOXNETADP_OS_SPECFIC 1
#include "../VBoxNetAdpInternal.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The maximum number of SG segments.
 * Used to prevent stack overflow and similar bad stuff. */
#define VBOXNETADP_DARWIN_MAX_SEGS       32
#define VBOXNETADP_DARWIN_MAX_FAMILIES   4
#define VBOXNETADP_DARWIN_NAME           "vboxnet"
#define VBOXNETADP_DARWIN_MTU            1500
#define VBOXNETADP_DARWIN_DETACH_TIMEOUT 500

#define VBOXNETADP_FROM_IFACE(iface) ((PVBOXNETADP) ifnet_softc(iface))

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
static kern_return_t    VBoxNetAdpDarwinStart(struct kmod_info *pKModInfo, void *pvData);
static kern_return_t    VBoxNetAdpDarwinStop(struct kmod_info *pKModInfo, void *pvData);
__END_DECLS

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Declare the module stuff.
 */
__BEGIN_DECLS
extern kern_return_t _start(struct kmod_info *pKModInfo, void *pvData);
extern kern_return_t _stop(struct kmod_info *pKModInfo, void *pvData);

KMOD_EXPLICIT_DECL(VBoxNetAdp, VBOX_VERSION_STRING, _start, _stop)
DECLHIDDEN(kmod_start_func_t *) _realmain = VBoxNetAdpDarwinStart;
DECLHIDDEN(kmod_stop_func_t  *) _antimain = VBoxNetAdpDarwinStop;
DECLHIDDEN(int)                 _kext_apple_cc = __APPLE_CC__;
__END_DECLS

/**
 * The (common) global data.
 */
#ifdef VBOXANETADP_DO_NOT_USE_NETFLT
static VBOXNETADPGLOBALS g_VBoxNetAdpGlobals;

#else /* !VBOXANETADP_DO_NOT_USE_NETFLT */

/**
 * Generate a suitable MAC address.
 *
 * @param   pThis       The instance.
 * @param   pMac        Where to return the MAC address.
 */
DECLHIDDEN(void) vboxNetAdpComposeMACAddress(PVBOXNETADP pThis, PRTMAC pMac)
{
#if 0 /* Use a locally administered version of the OUI we use for the guest NICs. */
    pMac->au8[0] = 0x08 | 2;
    pMac->au8[1] = 0x00;
    pMac->au8[2] = 0x27;
#else /* this is what \0vb comes down to. It seems to be unassigned atm. */
    pMac->au8[0] = 0;
    pMac->au8[1] = 0x76;
    pMac->au8[2] = 0x62;
#endif

    pMac->au8[3] = 0; /* pThis->uUnit >> 16; */
    pMac->au8[4] = 0; /* pThis->uUnit >> 8; */
    pMac->au8[5] = pThis->uUnit;
}
#endif /* !VBOXANETADP_DO_NOT_USE_NETFLT */




static void vboxNetAdpDarwinComposeUUID(PVBOXNETADP pThis, PRTUUID pUuid)
{
    /* Generate UUID from name and MAC address. */
    RTUuidClear(pUuid);
    memcpy(pUuid->au8, "vboxnet", 7);
    pUuid->Gen.u8ClockSeqHiAndReserved = (pUuid->Gen.u8ClockSeqHiAndReserved & 0x3f) | 0x80;
    pUuid->Gen.u16TimeHiAndVersion = (pUuid->Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000;
    pUuid->Gen.u8ClockSeqLow = pThis->uUnit;
    vboxNetAdpComposeMACAddress(pThis, (PRTMAC)pUuid->Gen.au8Node);
}

#ifdef VBOXANETADP_DO_NOT_USE_NETFLT
/**
 * Reads and retains the host interface handle.
 *
 * @returns The handle, NULL if detached.
 * @param   pThis
 */
DECLINLINE(ifnet_t) vboxNetAdpDarwinRetainIfNet(PVBOXNETADP pThis)
{
    if (pThis->u.s.pIface)
        ifnet_reference(pThis->u.s.pIface);

    return pThis->u.s.pIface;
}


/**
 * Release the host interface handle previously retained
 * by vboxNetAdpDarwinRetainIfNet.
 *
 * @param   pThis           The instance.
 * @param   pIfNet          The vboxNetAdpDarwinRetainIfNet return value, NULL is fine.
 */
DECLINLINE(void) vboxNetAdpDarwinReleaseIfNet(PVBOXNETADP pThis, ifnet_t pIfNet)
{
    NOREF(pThis);
    if (pIfNet)
        ifnet_release(pIfNet);
}

/**
 * Internal worker that create a darwin mbuf for a (scatter/)gather list.
 *
 * Taken from VBoxNetAdp-darwin.cpp.
 *
 * @returns Pointer to the mbuf.
 * @param   pThis           The instance.
 * @param   pSG             The (scatter/)gather list.
 */
static mbuf_t vboxNetAdpDarwinMBufFromSG(PVBOXNETADP pThis, PINTNETSG pSG)
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
            return pPkt;

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
 * Taken from VBoxNetAdp-darwin.cpp.
 *
 * @returns Number of segments.
 * @param   pThis               The instance.
 * @param   pMBuf               The mbuf.
 * @param   pvFrame             The frame pointer, optional.
 */
DECLINLINE(unsigned) vboxNetAdpDarwinMBufCalcSGSegs(PVBOXNETADP pThis, mbuf_t pMBuf, void *pvFrame)
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
 * Taken from VBoxNetAdp-darwin.cpp.
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
DECLINLINE(void) vboxNetAdpDarwinMBufToSG(PVBOXNETADP pThis, mbuf_t pMBuf, void *pvFrame, PINTNETSG pSG, unsigned cSegs, uint32_t fSrc)
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

#endif /* VBOXANETADP_DO_NOT_USE_NETFLT */
static errno_t vboxNetAdpDarwinOutput(ifnet_t pIface, mbuf_t pMBuf)
{
#ifdef VBOXANETADP_DO_NOT_USE_NETFLT
    PVBOXNETADP pThis = VBOXNETADP_FROM_IFACE(pIface);
    Assert(pThis);
    if (vboxNetAdpPrepareToReceive(pThis))
    {
        unsigned cSegs = vboxNetAdpDarwinMBufCalcSGSegs(pThis, pMBuf, NULL);
        if (cSegs < VBOXNETADP_DARWIN_MAX_SEGS)
        {
            PINTNETSG pSG = (PINTNETSG)alloca(RT_OFFSETOF(INTNETSG, aSegs[cSegs]));
            vboxNetAdpDarwinMBufToSG(pThis, pMBuf, NULL, pSG, cSegs, INTNETTRUNKDIR_HOST);
            vboxNetAdpReceive(pThis, pSG);
        }
        else
            vboxNetAdpCancelReceive(pThis);
    }
#endif /* VBOXANETADP_DO_NOT_USE_NETFLT */
    mbuf_freem_list(pMBuf);
    return 0;
}

static void vboxNetAdpDarwinAttachFamily(PVBOXNETADP pThis, protocol_family_t Family)
{
    u_int32_t i;
    for (i = 0; i < VBOXNETADP_MAX_FAMILIES; i++)
        if (pThis->u.s.aAttachedFamilies[i] == 0)
        {
            pThis->u.s.aAttachedFamilies[i] = Family;
            break;
        }
}

static void vboxNetAdpDarwinDetachFamily(PVBOXNETADP pThis, protocol_family_t Family)
{
    u_int32_t i;
    for (i = 0; i < VBOXNETADP_MAX_FAMILIES; i++)
        if (pThis->u.s.aAttachedFamilies[i] == Family)
            pThis->u.s.aAttachedFamilies[i] = 0;
}

static errno_t vboxNetAdpDarwinAddProto(ifnet_t pIface, protocol_family_t Family, const struct ifnet_demux_desc *pDemuxDesc, u_int32_t nDesc)
{
    PVBOXNETADP pThis = VBOXNETADP_FROM_IFACE(pIface);
    Assert(pThis);
    vboxNetAdpDarwinAttachFamily(pThis, Family);
    LogFlow(("vboxNetAdpAddProto: Family=%d.\n", Family));
    return ether_add_proto(pIface, Family, pDemuxDesc, nDesc);
}

static errno_t vboxNetAdpDarwinDelProto(ifnet_t pIface, protocol_family_t Family)
{
    PVBOXNETADP pThis = VBOXNETADP_FROM_IFACE(pIface);
    Assert(pThis);
    LogFlow(("vboxNetAdpDelProto: Family=%d.\n", Family));
    vboxNetAdpDarwinDetachFamily(pThis, Family);
    return ether_del_proto(pIface, Family);
}

static void vboxNetAdpDarwinDetach(ifnet_t pIface)
{
    PVBOXNETADP pThis = VBOXNETADP_FROM_IFACE(pIface);
    Assert(pThis);
    Log2(("vboxNetAdpDarwinDetach: Signaling detach to vboxNetAdpUnregisterDevice.\n"));
    /* Let vboxNetAdpDarwinUnregisterDevice know that the interface has been detached. */
    RTSemEventSignal(pThis->u.s.hEvtDetached);
}


#ifdef VBOXANETADP_DO_NOT_USE_NETFLT

int  vboxNetAdpPortOsXmit(PVBOXNETADP pThis, PINTNETSG pSG, uint32_t fDst)
{
    int rc = VINF_SUCCESS;
    ifnet_t pIfNet = vboxNetAdpDarwinRetainIfNet(pThis); // Really need a wrapper?
    if (pIfNet)
    {
        /*
         * Create a mbuf for the gather list and push it onto the host stack.
         */
        mbuf_t pMBuf = vboxNetAdpDarwinMBufFromSG(pThis, pSG);
        if (pMBuf)
        {
            /* This is what IONetworkInterface::inputPacket does. */
            unsigned const cbEthHdr = 14;
            mbuf_pkthdr_setheader(pMBuf, mbuf_data(pMBuf));
            mbuf_pkthdr_setlen(pMBuf, mbuf_pkthdr_len(pMBuf) - cbEthHdr);
            mbuf_setdata(pMBuf, (uint8_t *)mbuf_data(pMBuf) + cbEthHdr, mbuf_len(pMBuf) - cbEthHdr);
            mbuf_pkthdr_setrcvif(pMBuf, pIfNet); /* will crash without this. */

            Log(("vboxNetAdpPortOsXmit: calling ifnet_input()\n"));
            errno_t err = ifnet_input(pIfNet, pMBuf, NULL);
            if (err)
                rc = RTErrConvertFromErrno(err);
        }
        else
        {
            Log(("vboxNetAdpPortOsXmit: failed to convert SG to mbuf.\n"));
            rc = VERR_NO_MEMORY;
        }

        vboxNetAdpDarwinReleaseIfNet(pThis, pIfNet);
    }
    else
        Log(("vboxNetAdpPortOsXmit: failed to retain the interface.\n"));

    return rc;
}

bool vboxNetAdpPortOsIsPromiscuous(PVBOXNETADP pThis)
{
    uint16_t fIf = 0;
    ifnet_t pIfNet = vboxNetAdpDarwinRetainIfNet(pThis);
    if (pIfNet)
    {
        /* gather the data */
        fIf = ifnet_flags(pIfNet);
        vboxNetAdpDarwinReleaseIfNet(pThis, pIfNet);
    }
    return fIf & IFF_PROMISC;
}


void vboxNetAdpPortOsGetMacAddress(PVBOXNETADP pThis, PRTMAC pMac)
{
    *pMac = pThis->u.s.Mac;
}


bool vboxNetAdpPortOsIsHostMac(PVBOXNETADP pThis, PCRTMAC pMac)
{
    /* ASSUMES that the MAC address never changes. */
    return pThis->u.s.Mac.au16[0] == pMac->au16[0]
        && pThis->u.s.Mac.au16[1] == pMac->au16[1]
        && pThis->u.s.Mac.au16[2] == pMac->au16[2];
}

int vboxNetAdpOsDisconnectIt(PVBOXNETADP pThis)
{
    /* Nothing to do here. */
    return VINF_SUCCESS;
}


int  vboxNetAdpOsConnectIt(PVBOXNETADP pThis)
{
    /* Nothing to do here. */
    return VINF_SUCCESS;
}
#else /* !VBOXANETADP_DO_NOT_USE_NETFLT */
VBOXNETADP g_vboxnet0;

#endif /* !VBOXANETADP_DO_NOT_USE_NETFLT */


int vboxNetAdpOsCreate(PVBOXNETADP pThis, PCRTMAC pMACAddress)
{
    int rc;
    struct ifnet_init_params Params;
    RTUUID uuid;
    struct sockaddr_dl mac;

    pThis->u.s.hEvtDetached = NIL_RTSEMEVENT;
    rc = RTSemEventCreate(&pThis->u.s.hEvtDetached);
    if (RT_FAILURE(rc))
        return rc;

    mac.sdl_len = sizeof(mac);
    mac.sdl_family = AF_LINK;
    mac.sdl_alen = ETHER_ADDR_LEN;
    mac.sdl_nlen = 0;
    mac.sdl_slen = 0;
    memcpy(LLADDR(&mac), pMACAddress->au8, mac.sdl_alen);

    RTStrPrintf(pThis->szName, VBOXNETADP_MAX_NAME_LEN, "%s%d", VBOXNETADP_NAME, pThis->uUnit);
    vboxNetAdpDarwinComposeUUID(pThis, &uuid);
    Params.uniqueid = uuid.au8;
    Params.uniqueid_len = sizeof(uuid);
    Params.name = VBOXNETADP_NAME;
    Params.unit = pThis->uUnit;
    Params.family = IFNET_FAMILY_ETHERNET;
    Params.type = IFT_ETHER;
    Params.output = vboxNetAdpDarwinOutput;
    Params.demux = ether_demux;
    Params.add_proto = vboxNetAdpDarwinAddProto;
    Params.del_proto = vboxNetAdpDarwinDelProto;
    Params.check_multi = ether_check_multi;
    Params.framer = ether_frameout;
    Params.softc = pThis;
    Params.ioctl = (ifnet_ioctl_func)ether_ioctl;
    Params.set_bpf_tap = NULL;
    Params.detach = vboxNetAdpDarwinDetach;
    Params.event = NULL;
    Params.broadcast_addr = "\xFF\xFF\xFF\xFF\xFF\xFF";
    Params.broadcast_len = ETHER_ADDR_LEN;

    errno_t err = ifnet_allocate(&Params, &pThis->u.s.pIface);
    if (!err)
    {
        err = ifnet_attach(pThis->u.s.pIface, &mac);
        if (!err)
        {
            err = ifnet_set_flags(pThis->u.s.pIface, IFF_RUNNING | IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST, 0xFFFF);
            if (!err)
            {
                ifnet_set_mtu(pThis->u.s.pIface, VBOXNETADP_MTU);
                return VINF_SUCCESS;
            }
            else
                Log(("vboxNetAdpDarwinRegisterDevice: Failed to set flags (err=%d).\n", err));
            ifnet_detach(pThis->u.s.pIface);
        }
        else
            Log(("vboxNetAdpDarwinRegisterDevice: Failed to attach to interface (err=%d).\n", err));
        ifnet_release(pThis->u.s.pIface);
    }
    else
        Log(("vboxNetAdpDarwinRegisterDevice: Failed to allocate interface (err=%d).\n", err));

    return RTErrConvertFromErrno(err);
}

void vboxNetAdpOsDestroy(PVBOXNETADP pThis)
{
    u_int32_t i;
    /* Bring down the interface */
    int rc = VINF_SUCCESS;
    errno_t err;

    AssertPtr(pThis->u.s.pIface);
    Assert(pThis->u.s.hEvtDetached != NIL_RTSEMEVENT);

    err = ifnet_set_flags(pThis->u.s.pIface, 0, IFF_UP | IFF_RUNNING);
    if (err)
        Log(("vboxNetAdpDarwinUnregisterDevice: Failed to bring down interface "
             "(err=%d).\n", err));
    /* Detach all protocols. */
    for (i = 0; i < VBOXNETADP_MAX_FAMILIES; i++)
        if (pThis->u.s.aAttachedFamilies[i])
            ifnet_detach_protocol(pThis->u.s.pIface, pThis->u.s.aAttachedFamilies[i]);
    err = ifnet_detach(pThis->u.s.pIface);
    if (err)
        Log(("vboxNetAdpDarwinUnregisterDevice: Failed to detach interface "
             "(err=%d).\n", err));
    Log2(("vboxNetAdpDarwinUnregisterDevice: Waiting for 'detached' event...\n"));
    /* Wait until we get a signal from detach callback. */
    rc = RTSemEventWait(pThis->u.s.hEvtDetached, VBOXNETADP_DETACH_TIMEOUT);
    if (rc == VERR_TIMEOUT)
        LogRel(("VBoxAdpDrv: Failed to detach interface %s%d\n.",
                VBOXNETADP_NAME, pThis->uUnit));
    err = ifnet_release(pThis->u.s.pIface);
    if (err)
        Log(("vboxNetAdpUnregisterDevice: Failed to release interface (err=%d).\n", err));

    RTSemEventDestroy(pThis->u.s.hEvtDetached);
    pThis->u.s.hEvtDetached = NIL_RTSEMEVENT;
}

int  vboxNetAdpOsInit(PVBOXNETADP pThis)
{
    /*
     * Init the darwin specific members.
     */
    pThis->u.s.pIface = NULL;
    pThis->u.s.hEvtDetached = NIL_RTSEMEVENT;
    memset(pThis->u.s.aAttachedFamilies, 0, sizeof(pThis->u.s.aAttachedFamilies));

    return VINF_SUCCESS;
}

/**
 * Start the kernel module.
 */
static kern_return_t    VBoxNetAdpDarwinStart(struct kmod_info *pKModInfo, void *pvData)
{
    int rc;

    /*
     * Initialize IPRT and find our module tag id.
     * (IPRT is shared with VBoxDrv, it creates the loggers.)
     */
    rc = RTR0Init(0);
    if (RT_SUCCESS(rc))
    {
        Log(("VBoxNetAdpDarwinStart\n"));
        /*
         * Initialize the globals and connect to the support driver.
         *
         * This will call back vboxNetAdpOsOpenSupDrv (and maybe vboxNetAdpOsCloseSupDrv)
         * for establishing the connect to the support driver.
         */
#ifdef VBOXANETADP_DO_NOT_USE_NETFLT
        memset(&g_VBoxNetAdpGlobals, 0, sizeof(g_VBoxNetAdpGlobals));
        rc = vboxNetAdpInitGlobals(&g_VBoxNetAdpGlobals);
#else /* !VBOXANETADP_DO_NOT_USE_NETFLT */
        RTMAC Mac;
        vboxNetAdpOsInit(&g_vboxnet0);
        vboxNetAdpComposeMACAddress(&g_vboxnet0, &Mac);
        rc = vboxNetAdpOsCreate(&g_vboxnet0, &Mac);
#endif /* !VBOXANETADP_DO_NOT_USE_NETFLT */
        if (RT_SUCCESS(rc))
        {
            LogRel(("VBoxAdpDrv: version " VBOX_VERSION_STRING " r%d\n", VBOX_SVN_REV));
            return KMOD_RETURN_SUCCESS;
        }

        LogRel(("VBoxAdpDrv: failed to initialize device extension (rc=%d)\n", rc));
        RTR0Term();
    }
    else
        printf("VBoxAdpDrv: failed to initialize IPRT (rc=%d)\n", rc);

#ifdef VBOXANETADP_DO_NOT_USE_NETFLT
    memset(&g_VBoxNetAdpGlobals, 0, sizeof(g_VBoxNetAdpGlobals));
#endif /* VBOXANETADP_DO_NOT_USE_NETFLT */
    return KMOD_RETURN_FAILURE;
}


/**
 * Stop the kernel module.
 */
static kern_return_t VBoxNetAdpDarwinStop(struct kmod_info *pKModInfo, void *pvData)
{
    Log(("VBoxNetAdpDarwinStop\n"));

    /*
     * Refuse to unload if anyone is currently using the filter driver.
     * This is important as I/O kit / xnu will to be able to do usage
     * tracking for us!
     */
#ifdef VBOXANETADP_DO_NOT_USE_NETFLT
    int rc = vboxNetAdpTryDeleteGlobals(&g_VBoxNetAdpGlobals);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxNetAdpDarwinStop - failed, busy.\n"));
        return KMOD_RETURN_FAILURE;
    }

    /*
     * Undo the work done during start (in reverse order).
     */
    memset(&g_VBoxNetAdpGlobals, 0, sizeof(g_VBoxNetAdpGlobals));
#else /* !VBOXANETADP_DO_NOT_USE_NETFLT */
    vboxNetAdpOsDestroy(&g_vboxnet0);
#endif /* !VBOXANETADP_DO_NOT_USE_NETFLT */

    RTR0Term();

    return KMOD_RETURN_SUCCESS;
}
