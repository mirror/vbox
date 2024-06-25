/* $Id$ */
/** @file
 * DrvNATlibslirp - NATlibslirp network transport driver.
 */

/*
 * Copyright (C) 2022-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_NAT
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

#include "DrvNATlibslirp.h"


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{FNPDMTHREADDRV}
 * 
 * Queues guest process received packet. Triggered by drvNATRecvWakeup.
 */
static DECLCALLBACK(int) drvNATRecv(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        RTReqQueueProcess(pThis->hRecvReqQueue, 0);
        if (ASMAtomicReadU32(&pThis->cPkts) == 0)
            RTSemEventWait(pThis->EventRecv, RT_INDEFINITE_WAIT);
    }
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNPDMTHREADWAKEUPDRV}
 */
static DECLCALLBACK(int) drvNATRecvWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    RT_NOREF(pThread);
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    int rc;
    rc = RTSemEventSignal(pThis->EventRecv);

    STAM_COUNTER_INC(&pThis->StatNATRecvWakeups);
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNPDMTHREADDRV}
 */
static DECLCALLBACK(int) drvNATUrgRecv(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        RTReqQueueProcess(pThis->hUrgRecvReqQueue, 0);
        if (ASMAtomicReadU32(&pThis->cUrgPkts) == 0)
        {
            int rc = RTSemEventWait(pThis->EventUrgRecv, RT_INDEFINITE_WAIT);
            AssertRC(rc);
        }
    }
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNPDMTHREADWAKEUPDRV}
 */
static DECLCALLBACK(int) drvNATUrgRecvWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    RT_NOREF(pThread);
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    int rc = RTSemEventSignal(pThis->EventUrgRecv);
    AssertRC(rc);

    return VINF_SUCCESS;
}

/**
 * @brief Processes incoming packet (to guest).
 * 
 * @param   pThis   Pointer to DRVNAT state for current context.
 * @param   pBuf    Pointer to packet buffer.
 * @param   cb      Size of packet in buffer.
 * 
 * @thread  NAT
 */
static DECLCALLBACK(void) drvNATRecvWorker(PDRVNAT pThis, void *pBuf, int cb)
{
    int rc;
    STAM_PROFILE_START(&pThis->StatNATRecv, a);

    while (ASMAtomicReadU32(&pThis->cUrgPkts) != 0)
    {
        rc = RTSemEventWait(pThis->EventRecv, RT_INDEFINITE_WAIT);
        if (   RT_FAILURE(rc)
            && (   rc == VERR_TIMEOUT
                || rc == VERR_INTERRUPTED))
            goto done_unlocked;
    }

    rc = RTCritSectEnter(&pThis->DevAccessLock);
    AssertRC(rc);

    STAM_PROFILE_START(&pThis->StatNATRecvWait, b);
    rc = pThis->pIAboveNet->pfnWaitReceiveAvail(pThis->pIAboveNet, RT_INDEFINITE_WAIT);
    STAM_PROFILE_STOP(&pThis->StatNATRecvWait, b);

    if (RT_SUCCESS(rc))
    {
        rc = pThis->pIAboveNet->pfnReceive(pThis->pIAboveNet, pBuf, cb);
        AssertRC(rc);
        RTMemFree(pBuf);
        pBuf = NULL;
    }
    else if (   rc != VERR_TIMEOUT
             && rc != VERR_INTERRUPTED)
    {
        AssertRC(rc);
    }

    rc = RTCritSectLeave(&pThis->DevAccessLock);
    AssertRC(rc);

done_unlocked:
    ASMAtomicDecU32(&pThis->cPkts);

    drvNATNotifyNATThread(pThis, "drvNATRecvWorker");

    RTMemFree(pBuf);
    pBuf = NULL;

    STAM_PROFILE_STOP(&pThis->StatNATRecv, a);
}

/**
 * Frees a S/G buffer allocated by drvNATNetworkUp_AllocBuf.
 *
 * @param   pThis               Pointer to the NAT instance.
 * @param   pSgBuf              The S/G buffer to free.
 * 
 * @thread  NAT
 */
static void drvNATFreeSgBuf(PDRVNAT pThis, PPDMSCATTERGATHER pSgBuf)
{
    RT_NOREF(pThis);
    Assert((pSgBuf->fFlags & PDMSCATTERGATHER_FLAGS_MAGIC_MASK) == PDMSCATTERGATHER_FLAGS_MAGIC);
    pSgBuf->fFlags = 0;
    if (pSgBuf->pvAllocator)
    {
        Assert(!pSgBuf->pvUser);

        RTMemFree(pSgBuf->aSegs[0].pvSeg);
        RTMemFree(pSgBuf->pvAllocator);

        pSgBuf->pvAllocator = NULL;
    }
    else if (pSgBuf->pvUser)
    {
        RTMemFree(pSgBuf->aSegs[0].pvSeg);
        pSgBuf->aSegs[0].pvSeg = NULL;
        RTMemFree(pSgBuf->pvUser);
        pSgBuf->pvUser = NULL;
    }
    RTMemFree(pSgBuf);
}

/**
 * Worker function for drvNATSend().
 *
 * @param   pThis               Pointer to the NAT instance.
 * @param   pSgBuf              The scatter/gather buffer.
 * @thread  NAT
 */
static DECLCALLBACK(void) drvNATSendWorker(PDRVNAT pThis, PPDMSCATTERGATHER pSgBuf)
{
    LogFlowFunc(("pThis=%p pSgBuf=%p\n", pThis, pSgBuf));

    if (pThis->enmLinkState == PDMNETWORKLINKSTATE_UP)
    {
        const uint8_t *m = static_cast<const uint8_t*>(pSgBuf->pvAllocator);
        if (m)
        {
            /*
             * A normal frame.
             */
            LogFlowFunc(("m=%p\n", m));
            slirp_input(pThis->pNATState->pSlirp, (uint8_t const *)pSgBuf->pvAllocator, (int)pSgBuf->cbUsed);
        }
        else
        {
            /*
             * M_EXT buf, need to segment it.
             */

            uint8_t const  *pbFrame = (uint8_t const *)pSgBuf->aSegs[0].pvSeg;
            PCPDMNETWORKGSO pGso    = (PCPDMNETWORKGSO)pSgBuf->pvUser;
            /* Do not attempt to segment frames with invalid GSO parameters. */
            if (PDMNetGsoIsValid((const PDMNETWORKGSO *)pGso, sizeof(*pGso), pSgBuf->cbUsed))
            {
                uint32_t const cSegs = PDMNetGsoCalcSegmentCount(pGso, pSgBuf->cbUsed);
                Assert(cSegs > 1);
                for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
                {
                    void  *pvSeg;

                    /** @todo r=jack: is this fine leaving as a constant instead of dynamic? */
                    pvSeg = RTMemAlloc(DRVNAT_MAXFRAMESIZE);

                    uint32_t cbPayload, cbHdrs;
                    uint32_t offPayload = PDMNetGsoCarveSegment(pGso, pbFrame, pSgBuf->cbUsed,
                                                                iSeg, cSegs, (uint8_t *)pvSeg, &cbHdrs, &cbPayload);
                    memcpy((uint8_t *)pvSeg + cbHdrs, pbFrame + offPayload, cbPayload);

                    slirp_input(pThis->pNATState->pSlirp, (uint8_t const *)pvSeg, cbPayload + cbHdrs);
                    RTMemFree(pvSeg);
                }
            }
        }
    }

    LogFlowFunc(("leave\n"));
    drvNATFreeSgBuf(pThis, pSgBuf);
}

/**
 * @interface_method_impl{PDMINETWORKUP,pfnBeginXmit}
 */
static DECLCALLBACK(int) drvNATNetworkUp_BeginXmit(PPDMINETWORKUP pInterface, bool fOnWorkerThread)
{
    RT_NOREF(fOnWorkerThread);
    PDRVNAT pThis = RT_FROM_MEMBER(pInterface, DRVNAT, INetworkUp);
    int rc = RTCritSectTryEnter(&pThis->XmitLock);
    if (RT_FAILURE(rc))
    {
        /** @todo Kick the worker thread when we have one... */
        rc = VERR_TRY_AGAIN;
    }
    LogFlowFunc(("Beginning xmit...\n"));
    return rc;
}

/**
 * @interface_method_impl{PDMINETWORKUP,pfnAllocBuf}
 */
static DECLCALLBACK(int) drvNATNetworkUp_AllocBuf(PPDMINETWORKUP pInterface, size_t cbMin,
                                                  PCPDMNETWORKGSO pGso, PPPDMSCATTERGATHER ppSgBuf)
{
    PDRVNAT pThis = RT_FROM_MEMBER(pInterface, DRVNAT, INetworkUp);
    Assert(RTCritSectIsOwner(&pThis->XmitLock));

    LogFlowFunc(("enter\n"));

    /*
     * Drop the incoming frame if the NAT thread isn't running.
     */
    if (pThis->pSlirpThread->enmState != PDMTHREADSTATE_RUNNING)
    {
        Log(("drvNATNetowrkUp_AllocBuf: returns VERR_NET_NO_NETWORK\n"));
        return VERR_NET_NO_NETWORK;
    }

    /*
     * Allocate a scatter/gather buffer and an mbuf.
     */
    PPDMSCATTERGATHER pSgBuf = (PPDMSCATTERGATHER)RTMemAllocZ(sizeof(PDMSCATTERGATHER));
    if (!pSgBuf)
        return VERR_NO_MEMORY;
    if (!pGso)
    {
        /*
         * Drop the frame if it is too big.
         */
        if (cbMin >= DRVNAT_MAXFRAMESIZE)
        {
            Log(("drvNATNetowrkUp_AllocBuf: drops over-sized frame (%u bytes), returns VERR_INVALID_PARAMETER\n",
                 cbMin));
            RTMemFree(pSgBuf);
            return VERR_INVALID_PARAMETER;
        }

        pSgBuf->pvUser      = NULL;
        pSgBuf->aSegs[0].cbSeg = RT_ALIGN_Z(cbMin, 128);
        pSgBuf->aSegs[0].pvSeg = RTMemAlloc(pSgBuf->aSegs[0].cbSeg);
        pSgBuf->pvAllocator = pSgBuf->aSegs[0].pvSeg;

        if (!pSgBuf->pvAllocator)
        {
            RTMemFree(pSgBuf);
            return VERR_TRY_AGAIN;
        }
    }
    else
    {
        /*
         * Drop the frame if its segment is too big.
         */
        if (pGso->cbHdrsTotal + pGso->cbMaxSeg >= DRVNAT_MAXFRAMESIZE)
        {
            Log(("drvNATNetowrkUp_AllocBuf: drops over-sized frame (%u bytes), returns VERR_INVALID_PARAMETER\n",
                 pGso->cbHdrsTotal + pGso->cbMaxSeg));
            RTMemFree(pSgBuf);
            return VERR_INVALID_PARAMETER;
        }

        pSgBuf->pvUser      = RTMemDup(pGso, sizeof(*pGso));
        pSgBuf->pvAllocator = NULL;

        /** @todo r=jack: figure out why need *2 */
        pSgBuf->aSegs[0].cbSeg = RT_ALIGN_Z(cbMin*2, 128);
        pSgBuf->aSegs[0].pvSeg = RTMemAlloc(pSgBuf->aSegs[0].cbSeg);
        if (!pSgBuf->pvUser || !pSgBuf->aSegs[0].pvSeg)
        {
            RTMemFree(pSgBuf->aSegs[0].pvSeg);
            RTMemFree(pSgBuf->pvUser);
            RTMemFree(pSgBuf);
            return VERR_TRY_AGAIN;
        }
    }

    /*
     * Initialize the S/G buffer and return.
     */
    pSgBuf->fFlags      = PDMSCATTERGATHER_FLAGS_MAGIC | PDMSCATTERGATHER_FLAGS_OWNER_1;
    pSgBuf->cbUsed      = 0;
    pSgBuf->cbAvailable = pSgBuf->aSegs[0].cbSeg;
    pSgBuf->cSegs       = 1;

    *ppSgBuf = pSgBuf;
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMINETWORKUP,pfnFreeBuf}
 */
static DECLCALLBACK(int) drvNATNetworkUp_FreeBuf(PPDMINETWORKUP pInterface, PPDMSCATTERGATHER pSgBuf)
{
    PDRVNAT pThis = RT_FROM_MEMBER(pInterface, DRVNAT, INetworkUp);
    Assert(RTCritSectIsOwner(&pThis->XmitLock));
    drvNATFreeSgBuf(pThis, pSgBuf);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMINETWORKUP,pfnSendBuf}
 */
static DECLCALLBACK(int) drvNATNetworkUp_SendBuf(PPDMINETWORKUP pInterface, PPDMSCATTERGATHER pSgBuf, bool fOnWorkerThread)
{
    RT_NOREF(fOnWorkerThread);
    PDRVNAT pThis = RT_FROM_MEMBER(pInterface, DRVNAT, INetworkUp);
    Assert((pSgBuf->fFlags & PDMSCATTERGATHER_FLAGS_OWNER_MASK) == PDMSCATTERGATHER_FLAGS_OWNER_1);
    Assert(RTCritSectIsOwner(&pThis->XmitLock));

    LogFlowFunc(("enter\n"));

    int rc;
    if (pThis->pSlirpThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        rc = RTReqQueueCallEx(pThis->hSlirpReqQueue, NULL /*ppReq*/, 0 /*cMillies*/,
                              RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                              (PFNRT)drvNATSendWorker, 2, pThis, pSgBuf);
        if (RT_SUCCESS(rc))
        {
            drvNATNotifyNATThread(pThis, "drvNATNetworkUp_SendBuf");
            LogFlowFunc(("leave success\n"));
            return VINF_SUCCESS;
        }

        rc = VERR_NET_NO_BUFFER_SPACE;
    }
    else
        rc = VERR_NET_DOWN;
    drvNATFreeSgBuf(pThis, pSgBuf);
    LogFlowFunc(("leave rc=%Rrc\n", rc));
    return rc;
}

/**
 * @interface_method_impl{PDMINETWORKUP,pfnEndXmit}
 */
static DECLCALLBACK(void) drvNATNetworkUp_EndXmit(PPDMINETWORKUP pInterface)
{
    PDRVNAT pThis = RT_FROM_MEMBER(pInterface, DRVNAT, INetworkUp);
    RTCritSectLeave(&pThis->XmitLock);
}

/**
 * Get the NAT thread out of poll/WSAWaitForMultipleEvents
 */
static void drvNATNotifyNATThread(PDRVNAT pThis, const char *pszWho)
{
    RT_NOREF(pszWho);
    int rc;
#ifndef RT_OS_WINDOWS
    /* kick poll() */
    size_t cbIgnored;
    rc = RTPipeWrite(pThis->hPipeWrite, "", 1, &cbIgnored);
#else
    /* kick WSAWaitForMultipleEvents */
    rc = WSASetEvent(pThis->hWakeupEvent);
#endif
    AssertRC(rc);
}

/**
 * @interface_method_impl{PDMINETWORKUP,pfnSetPromiscuousMode}
 */
static DECLCALLBACK(void) drvNATNetworkUp_SetPromiscuousMode(PPDMINETWORKUP pInterface, bool fPromiscuous)
{
    RT_NOREF(pInterface, fPromiscuous);
    LogFlow(("drvNATNetworkUp_SetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    /* nothing to do */
}

/**
 * Worker function for drvNATNetworkUp_NotifyLinkChanged().
 * @thread "NAT" thread.
 */
static DECLCALLBACK(void) drvNATNotifyLinkChangedWorker(PDRVNAT pThis, PDMNETWORKLINKSTATE enmLinkState)
{
    pThis->enmLinkState = pThis->enmLinkStateWant = enmLinkState;
    switch (enmLinkState)
    {
        case PDMNETWORKLINKSTATE_UP:
            LogRel(("NAT: Link up\n"));
            // slirp_link_up(pThis->pNATState);
            break;

        case PDMNETWORKLINKSTATE_DOWN:
        case PDMNETWORKLINKSTATE_DOWN_RESUME:
            LogRel(("NAT: Link down\n"));
            // slirp_link_down(pThis->pNATState);
            break;

        default:
            AssertMsgFailed(("drvNATNetworkUp_NotifyLinkChanged: unexpected link state %d\n", enmLinkState));
    }
}

/**
 * Notification on link status changes.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmLinkState    The new link state.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvNATNetworkUp_NotifyLinkChanged(PPDMINETWORKUP pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    PDRVNAT pThis = RT_FROM_MEMBER(pInterface, DRVNAT, INetworkUp);

    LogFlow(("drvNATNetworkUp_NotifyLinkChanged: enmLinkState=%d\n", enmLinkState));

    /* Don't queue new requests if the NAT thread is not running (e.g. paused,
     * stopping), otherwise we would deadlock. Memorize the change. */
    if (pThis->pSlirpThread->enmState != PDMTHREADSTATE_RUNNING)
    {
        pThis->enmLinkStateWant = enmLinkState;
        return;
    }

    PRTREQ pReq;
    int rc = RTReqQueueCallEx(pThis->hSlirpReqQueue, &pReq, 0 /*cMillies*/, RTREQFLAGS_VOID,
                              (PFNRT)drvNATNotifyLinkChangedWorker, 2, pThis, enmLinkState);
    if (rc == VERR_TIMEOUT)
    {
        drvNATNotifyNATThread(pThis, "drvNATNetworkUp_NotifyLinkChanged");
        rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertRC(rc);
    }
    else
        AssertRC(rc);
    RTReqRelease(pReq);
}

/** @todo r=jack: move this to the right spot. temp for dev. */
static void register_poll_fd(int fd, void *opaque) {
    RT_NOREF(fd, opaque);
}

static void unregister_poll_fd(int fd, void *opaque) {
    RT_NOREF(fd, opaque);
}

static int slirp_to_poll(int events) {
    int ret = 0;
#ifndef RT_OS_WINDOWS
    if (events & SLIRP_POLL_IN)  ret |= POLLIN;
    if (events & SLIRP_POLL_OUT) ret |= POLLOUT;
    if (events & SLIRP_POLL_PRI) ret |= POLLPRI;
    if (events & SLIRP_POLL_ERR) ret |= POLLERR;
    if (events & SLIRP_POLL_HUP) ret |= POLLHUP;
#else
    if (events & SLIRP_POLL_IN)  ret |= (POLLRDNORM | POLLRDBAND);
    if (events & SLIRP_POLL_OUT) ret |= POLLWRNORM;
    if (events & SLIRP_POLL_PRI) ret |= (POLLIN);
    if (events & SLIRP_POLL_ERR) ret |= 0;
    if (events & SLIRP_POLL_HUP) ret |= 0;
#endif
    return ret;
}

static int poll_to_slirp(int events) {
    int ret = 0;
#ifndef RT_OS_WINDOWS
    if (events & POLLIN)  ret |= SLIRP_POLL_IN;
    if (events & POLLOUT) ret |= SLIRP_POLL_OUT;
    if (events & POLLPRI) ret |= SLIRP_POLL_PRI;
    if (events & POLLERR) ret |= SLIRP_POLL_ERR;
    if (events & POLLHUP) ret |= SLIRP_POLL_HUP;
#else
    if (events & (POLLRDNORM | POLLRDBAND))  ret |= SLIRP_POLL_IN;
    if (events & POLLWRNORM) ret |= SLIRP_POLL_OUT;
    if (events & (POLLPRI)) ret |= SLIRP_POLL_PRI;
    if (events & POLLERR) ret |= SLIRP_POLL_ERR;
    if (events & POLLHUP) ret |= SLIRP_POLL_HUP;
#endif
    return ret;
}

static int add_poll_cb(int fd, int events, void *opaque)
{
    // LogFlow(("add_poll_cb: fd=%d, events=%d\n", fd, events));
    PDRVNAT pThis = (PDRVNAT)opaque;

    if (pThis->pNATState->nsock + 1 >= pThis->pNATState->uPollCap)
    {
        int cbNew = pThis->pNATState->uPollCap * 2 * sizeof(struct pollfd);
        struct pollfd *pvNew = (struct pollfd *)RTMemRealloc(pThis->pNATState->polls, cbNew);
        if(pvNew)
        {
            pThis->pNATState->polls = pvNew;
            pThis->pNATState->uPollCap *= 2;
        }
        else {
            LogFlow(("add_poll_cb return -1"));
            return -1;
        }
    }

    int idx = pThis->pNATState->nsock;
#ifdef RT_OS_WINDOWS
    pThis->pNATState->polls[idx].fd = libslirp_wrap_RTHandleTableLookup(fd);
#else
    pThis->pNATState->polls[idx].fd = fd;
#endif
    pThis->pNATState->polls[idx].events = slirp_to_poll(events);
    pThis->pNATState->polls[idx].revents = 0;
    pThis->pNATState->nsock += 1;
    return idx;
}

static int get_revents_cb(int idx, void *opaque)
{
    PDRVNAT pThis = (PDRVNAT)opaque;
    struct pollfd* polls = pThis->pNATState->polls;
    return poll_to_slirp(polls[idx].revents);
}

/**
 * NAT thread handling the slirp stuff.
 *
 * The slirp implementation is single-threaded so we execute this enginre in a
 * dedicated thread. We take care that this thread does not become the
 * bottleneck: If the guest wants to send, a request is enqueued into the
 * hSlirpReqQueue and handled asynchronously by this thread.  If this thread
 * wants to deliver packets to the guest, it enqueues a request into
 * hRecvReqQueue which is later handled by the Recv thread.
 */
static DECLCALLBACK(int) drvNATAsyncIoThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
#ifdef RT_OS_WINDOWS
    unsigned int cBreak = 0;
#else /* RT_OS_WINDOWS */
    unsigned int cPollNegRet = 0;
    add_poll_cb(RTPipeToNative(pThis->hPipeRead), SLIRP_POLL_IN | SLIRP_POLL_HUP, pThis);
    pThis->pNATState->polls[0].fd = RTPipeToNative(pThis->hPipeRead);
    pThis->pNATState->polls[0].events = POLLRDNORM | POLLPRI | POLLRDBAND;
    pThis->pNATState->polls[0].revents = 0;
#endif /* !RT_OS_WINDOWS */

    LogFlow(("drvNATAsyncIoThread: pThis=%p\n", pThis));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    if (pThis->enmLinkStateWant != pThis->enmLinkState)
        drvNATNotifyLinkChangedWorker(pThis, pThis->enmLinkStateWant);

    /*
     * Polling loop.
     */
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        /*
         * To prevent concurrent execution of sending/receiving threads
         */
#ifndef RT_OS_WINDOWS
        uint32_t uTimeout = 0;
        pThis->pNATState->nsock = 1;

        slirp_pollfds_fill(pThis->pNATState->pSlirp, &uTimeout, add_poll_cb /* SlirpAddPollCb */, pThis /* opaque */);
        slirpUpdateTimeout(&uTimeout, pThis);

        int cChangedFDs = poll(pThis->pNATState->polls, pThis->pNATState->nsock, uTimeout /* timeout */);

        if (cChangedFDs < 0)
        {
            if (errno == EINTR)
            {
                Log2(("NAT: signal was caught while sleep on poll\n"));
                /* No error, just process all outstanding requests but don't wait */
                cChangedFDs = 0;
            }
            else if (cPollNegRet++ > 128)
            {
                LogRel(("NAT: Poll returns (%s) suppressed %d\n", strerror(errno), cPollNegRet));
                cPollNegRet = 0;
            }
        }


        slirp_pollfds_poll(pThis->pNATState->pSlirp, cChangedFDs < 0, get_revents_cb /* SlirpGetREventsCb */, pThis /* opaque */);
        if (pThis->pNATState->polls[0].revents & (POLLRDNORM|POLLPRI|POLLRDBAND))
        {
            /* drain the pipe
             *
             * Note! drvNATSend decoupled so we don't know how many times
             * device's thread sends before we've entered multiplex,
             * so to avoid false alarm drain pipe here to the very end
             *
             * @todo: Probably we should counter drvNATSend to count how
             * deep pipe has been filed before drain.
             *
             */
            /** @todo XXX: Make it reading exactly we need to drain the
             * pipe.*/
            char ch;
            size_t cbRead;
            RTPipeRead(pThis->hPipeRead, &ch, 1, &cbRead);
        }

        /* process _all_ outstanding requests but don't wait */
        RTReqQueueProcess(pThis->hSlirpReqQueue, 0);
        slirpCheckTimeout(pThis);

#else /* RT_OS_WINDOWS */
        uint32_t uTimeout = 0;
        pThis->pNATState->nsock = 0;
        slirp_pollfds_fill(pThis->pNATState->pSlirp, &uTimeout, add_poll_cb /* SlirpAddPollCb */, pThis /* opaque */);
        slirpUpdateTimeout(&uTimeout, pThis);

        int cChangedFDs = WSAPoll(pThis->pNATState->polls, pThis->pNATState->nsock, uTimeout /* timeout */);
        int error = WSAGetLastError();

        if (cChangedFDs < 0)
        {
            LogFlow(("NAT: WSAPoll returned %d (error %d)\n", cChangedFDs, error));
            LogFlow(("NSOCK = %d\n", pThis->pNATState->nsock));

            if (error == 10022)
                RTThreadSleep(100);
        }

        if (cChangedFDs == 0)
        {
            /* only check for slow/fast timers */
            slirp_pollfds_poll(pThis->pNATState->pSlirp, false /*select error*/, get_revents_cb /* SlirpGetREventsCb */, pThis /* opaque */);
            RTReqQueueProcess(pThis->hSlirpReqQueue, 0);
            continue;
        }
        /* poll the sockets in any case */
        Log2(("%s: poll\n", __FUNCTION__));
        slirp_pollfds_poll(pThis->pNATState->pSlirp, cChangedFDs < 0 /*select error*/, get_revents_cb /* SlirpGetREventsCb */, pThis /* opaque */);

        /* process _all_ outstanding requests but don't wait */
        RTReqQueueProcess(pThis->hSlirpReqQueue, 0);
        slirpCheckTimeout(pThis);
# ifdef VBOX_NAT_DELAY_HACK
        if (cBreak++ > 128)
        {
            cBreak = 0;
            RTThreadSleep(2);
        }
# endif
#endif /* RT_OS_WINDOWS */
    }

    return VINF_SUCCESS;
}

/**
 * Unblock the send thread so it can respond to a state change.
 *
 * @returns VBox status code.
 * @param   pDevIns     The pcnet device instance.
 * @param   pThread     The send thread.
 */
static DECLCALLBACK(int) drvNATAsyncIoWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    RT_NOREF(pThread);
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    drvNATNotifyNATThread(pThis, "drvNATAsyncIoWakeup");
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvNATHostResThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        RTReqQueueProcess(pThis->hHostResQueue, RT_INDEFINITE_WAIT);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvNATReqQueueInterrupt()
{
    /*
     * RTReqQueueProcess loops until request returns a warning or info
     * status code (other than VINF_SUCCESS).
     */
    return VINF_INTERRUPTED;
}

static DECLCALLBACK(int) drvNATHostResWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    RT_NOREF(pThread);
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    Assert(pThis != NULL);

    int rc;
    rc = RTReqQueueCallEx(pThis->hHostResQueue, NULL /*ppReq*/, 0 /*cMillies*/,
                          RTREQFLAGS_IPRT_STATUS | RTREQFLAGS_NO_WAIT,
                          (PFNRT)drvNATReqQueueInterrupt, 0);
    return rc;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvNATQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVNAT     pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKUP, &pThis->INetworkUp);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINETWORKNATCONFIG, &pThis->INetworkNATCfg);
    return NULL;
}

/**
 * Info handler.
 */
static DECLCALLBACK(void) drvNATInfo(PPDMDRVINS pDrvIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pszArgs);
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    pHlp->pfnPrintf(pHlp, "libslirp Connection Info:\n");
    pHlp->pfnPrintf(pHlp, slirp_connection_info(pThis->pNATState->pSlirp));
    pHlp->pfnPrintf(pHlp, "libslirp Neighbor Info:\n");
    pHlp->pfnPrintf(pHlp, slirp_neighbor_info(pThis->pNATState->pSlirp));
    pHlp->pfnPrintf(pHlp, "libslirp Version String: %s \n", slirp_version_string());
}

/**
 * Sets up the redirectors.
 *
 * @returns VBox status code.
 * @param   pCfg            The configuration handle.
 */
static int drvNATConstructRedir(unsigned iInstance, PDRVNAT pThis, PCFGMNODE pCfg, PRTNETADDRIPV4 pNetwork)
{
    PPDMDRVINS pDrvIns = pThis->pDrvIns;
    PCPDMDRVHLPR3 pHlp = pDrvIns->pHlpR3;

    RT_NOREF(pNetwork); /** @todo figure why pNetwork isn't used */

    PCFGMNODE pPFTree = pHlp->pfnCFGMGetChild(pCfg, "PortForwarding");
    if (pPFTree == NULL)
        return VINF_SUCCESS;

    /*
     * Enumerate redirections.
     */
    for (PCFGMNODE pNode = pHlp->pfnCFGMGetFirstChild(pPFTree); pNode; pNode = pHlp->pfnCFGMGetNextChild(pNode))
    {
        /*
         * Validate the port forwarding config.
         */
        if (!pHlp->pfnCFGMAreValuesValid(pNode, "Name\0Protocol\0UDP\0HostPort\0GuestPort\0GuestIP\0BindIP\0"))
            return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES,
                                    N_("Unknown configuration in port forwarding"));

        /* protocol type */
        bool fUDP;
        char szProtocol[32];
        int rc;
        GET_STRING(rc, pDrvIns, pNode, "Protocol", szProtocol[0], sizeof(szProtocol));
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        {
            fUDP = false;
            GET_BOOL(rc, pDrvIns, pNode, "UDP", fUDP);
        }
        else if (RT_SUCCESS(rc))
        {
            if (!RTStrICmp(szProtocol, "TCP"))
                fUDP = false;
            else if (!RTStrICmp(szProtocol, "UDP"))
                fUDP = true;
            else
                return PDMDrvHlpVMSetError(pDrvIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                    N_("NAT#%d: Invalid configuration value for \"Protocol\": \"%s\""),
                    iInstance, szProtocol);
        }
        else
            return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                       N_("NAT#%d: configuration query for \"Protocol\" failed"),
                                       iInstance);
        /* host port */
        int32_t iHostPort;
        GET_S32_STRICT(rc, pDrvIns, pNode, "HostPort", iHostPort);

        /* guest port */
        int32_t iGuestPort;
        GET_S32_STRICT(rc, pDrvIns, pNode, "GuestPort", iGuestPort);

        /* host address ("BindIP" name is rather unfortunate given "HostPort" to go with it) */
        struct in_addr BindIP;
        RT_ZERO(BindIP);
        GETIP_DEF(rc, pDrvIns, pNode, BindIP, INADDR_ANY);

        /* guest address */
        struct in_addr GuestIP;
        RT_ZERO(GuestIP);
        GETIP_DEF(rc, pDrvIns, pNode, GuestIP, INADDR_ANY);

        /*
         * Call slirp about it.
         */
        if (slirp_add_hostfwd(pThis->pNATState->pSlirp, fUDP, BindIP,
                              iHostPort, GuestIP, iGuestPort) < 0)
            return PDMDrvHlpVMSetError(pThis->pDrvIns, VERR_NAT_REDIR_SETUP, RT_SRC_POS,
                                       N_("NAT#%d: configuration error: failed to set up "
                                       "redirection of %d to %d. Probably a conflict with "
                                       "existing services or other rules"), iInstance, iHostPort,
                                       iGuestPort);
    } /* for each redir rule */

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) drvNATNotifyApplyPortForwardCommand(PDRVNAT pThis, bool fRemove,
                                                              bool fUdp, const char *pHostIp,
                                                              uint16_t u16HostPort, const char *pGuestIp, uint16_t u16GuestPort)
{
    struct in_addr guestIp, hostIp;

    if (   pHostIp == NULL
        || inet_aton(pHostIp, &hostIp) == 0)
        hostIp.s_addr = INADDR_ANY;

    if (   pGuestIp == NULL
        || inet_aton(pGuestIp, &guestIp) == 0)
        guestIp.s_addr = pThis->GuestIP;

    if (fRemove)
        slirp_remove_hostfwd(pThis->pNATState->pSlirp, fUdp, hostIp, u16HostPort);
    else
        slirp_add_hostfwd(pThis->pNATState->pSlirp, fUdp, hostIp,
                          u16HostPort, guestIp, u16GuestPort);
}

static DECLCALLBACK(int) drvNATNetworkNatConfigRedirect(PPDMINETWORKNATCONFIG pInterface, bool fRemove,
                                                        bool fUdp, const char *pHostIp, uint16_t u16HostPort,
                                                        const char *pGuestIp, uint16_t u16GuestPort)
{
    LogFlowFunc(("fRemove=%d, fUdp=%d, pHostIp=%s, u16HostPort=%u, pGuestIp=%s, u16GuestPort=%u\n",
                 RT_BOOL(fRemove), RT_BOOL(fUdp), pHostIp, u16HostPort, pGuestIp, u16GuestPort));
    PDRVNAT pThis = RT_FROM_MEMBER(pInterface, DRVNAT, INetworkNATCfg);
    /* Execute the command directly if the VM is not running. */
    int rc;
    if (pThis->pSlirpThread->enmState != PDMTHREADSTATE_RUNNING)
    {
        drvNATNotifyApplyPortForwardCommand(pThis, fRemove, fUdp, pHostIp,
                                           u16HostPort, pGuestIp,u16GuestPort);
        rc = VINF_SUCCESS;
    }
    else
    {
        PRTREQ pReq;
        rc = RTReqQueueCallEx(pThis->hSlirpReqQueue, &pReq, 0 /*cMillies*/, RTREQFLAGS_VOID,
                              (PFNRT)drvNATNotifyApplyPortForwardCommand, 7, pThis, fRemove,
                              fUdp, pHostIp, u16HostPort, pGuestIp, u16GuestPort);
        if (rc == VERR_TIMEOUT)
        {
            drvNATNotifyNATThread(pThis, "drvNATNetworkNatConfigRedirect");
            rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);
            AssertRC(rc);
        }
        else
            AssertRC(rc);

        RTReqRelease(pReq);
    }
    return rc;
}

static void slirpUpdateTimeout(uint32_t *uTimeout, void *opaque)
{
    PDRVNAT pThis = (PDRVNAT)opaque;
    Assert(pThis);

    int64_t currTime = slirpClockGetNsCb(pThis) / (1000 * 1000);
    SlirpTimer *pCurrent = pThis->pNATState->pTimerHead;
    while (pCurrent != NULL)
    {
        if (pCurrent->uTimeExpire != -1)
        {
            int64_t diff = pCurrent->uTimeExpire - currTime;

            if (diff < 0)
                diff = 0;

            if (diff < *uTimeout)
                *uTimeout = diff;
        }

        pCurrent = pCurrent->next;
    }
}

static void slirpCheckTimeout(void *opaque)
{
    PDRVNAT pThis = (PDRVNAT)opaque;
    Assert(pThis);

    int64_t currTime = slirpClockGetNsCb(pThis) / (1000 * 1000);
    SlirpTimer *pCurrent = pThis->pNATState->pTimerHead;
    while (pCurrent != NULL)
    {
        if (pCurrent->uTimeExpire != -1)
        {
            int64_t diff = pCurrent->uTimeExpire - currTime;
            if (diff <= 0)
            {
                pCurrent->uTimeExpire = -1;
                pCurrent->pHandler(pCurrent->opaque);
            }
        }

        pCurrent = pCurrent->next;
    }
}

/**
 * CALLBACKS
 */
static DECLCALLBACK(ssize_t) slirpSendPacketCb(const void *pBuf, size_t cb, void *opaque /* PDRVNAT */)
{
    char *pNewBuf = (char *)RTMemAlloc(cb);
    memcpy(pNewBuf, pBuf, cb);

    PDRVNAT pThis = (PDRVNAT)opaque;
    Assert(pThis);

    LogFlow(("slirp_output BEGIN %p %d\n", pNewBuf, cb));
    Log6(("slirp_output: pu8Buf=%p cb=%#x (pThis=%p)\n%.*Rhxd\n", pNewBuf, cb, pThis));

    /* don't queue new requests when the NAT thread is about to stop */
    if (pThis->pSlirpThread->enmState != PDMTHREADSTATE_RUNNING)
        return -1;

    ASMAtomicIncU32(&pThis->cPkts);
    int rc = RTReqQueueCallEx(pThis->hRecvReqQueue, NULL /*ppReq*/, 0 /*cMillies*/, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                              (PFNRT)drvNATRecvWorker, 3, pThis, pNewBuf, cb);
    AssertRC(rc);
    drvNATRecvWakeup(pThis->pDrvIns, pThis->pRecvThread);
    drvNATNotifyNATThread(pThis, "slirpSendPacketCb");
    STAM_COUNTER_INC(&pThis->StatQueuePktSent);
    LogFlowFuncLeave();
    return cb;
}

static DECLCALLBACK(void) slirpGuestErrorCb(const char *pMsg, void *opaque)
{
    PDRVNAT pThis = (PDRVNAT)opaque;
    Assert(pThis);

    PDMDRV_SET_ERROR(pThis->pDrvIns, VERR_PDM_UNKNOWN_DRVREG_VERSION,
                            N_("Unknown error: "));
    LogRel((pMsg));
}

static DECLCALLBACK(int64_t) slirpClockGetNsCb(void *opaque)
{
    PDRVNAT pThis = (PDRVNAT)opaque;
    Assert(pThis);

    return (int64_t)RTTimeNanoTS();
}

static DECLCALLBACK(void *) slirpTimerNewCb(SlirpTimerCb slirpTimeCb, void *cb_opaque, void *opaque)
{
    PDRVNAT pThis = (PDRVNAT)opaque;
    Assert(pThis);

    SlirpTimer *pNewTimer = (SlirpTimer *)RTMemAlloc(sizeof(SlirpTimer));
    if (!pNewTimer)
        return NULL;

    pNewTimer->next = pThis->pNATState->pTimerHead;
    pNewTimer->uTimeExpire = -1;
    pNewTimer->pHandler = slirpTimeCb;
    pNewTimer->opaque = cb_opaque;
    pThis->pNATState->pTimerHead = pNewTimer;

    return pNewTimer;
}

static DECLCALLBACK(void) slirpTimerFreeCb(void *pTimer, void *opaque)
{
    PDRVNAT pThis = (PDRVNAT)opaque;
    Assert(pThis);
    SlirpTimer *pCurrent = pThis->pNATState->pTimerHead;

    while (pCurrent != NULL)
    {
        if (pCurrent == (SlirpTimer *)pTimer)
        {
            SlirpTimer *pTmp = pCurrent->next;
            RTMemFree(pCurrent);
            pCurrent = pTmp;
        }
        else
            pCurrent = pCurrent->next;
    }
}

static DECLCALLBACK(void) slirpTimerModCb(void *pTimer, int64_t expireTime, void *opaque)
{
    PDRVNAT pThis = (PDRVNAT)opaque;
    Assert(pThis);

    ((SlirpTimer *)pTimer)->uTimeExpire = expireTime;
}

static DECLCALLBACK(void) slirpNotifyCb(void *opaque)
{
    PDRVNAT pThis = (PDRVNAT)opaque;

    drvNATAsyncIoWakeup(pThis->pDrvIns, NULL);
}

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvNATDestruct(PPDMDRVINS pDrvIns)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    LogFlow(("drvNATDestruct:\n"));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    if (pThis->pNATState)
    {
        slirp_cleanup(pThis->pNATState->pSlirp);
#ifdef VBOX_WITH_STATISTICS
# define DRV_PROFILE_COUNTER(name, dsc)     DEREGISTER_COUNTER(name, pThis)
# define DRV_COUNTING_COUNTER(name, dsc)    DEREGISTER_COUNTER(name, pThis)
# include "slirp/counters.h"
#endif
        pThis->pNATState = NULL;
    }

    RTReqQueueDestroy(pThis->hHostResQueue);
    pThis->hHostResQueue = NIL_RTREQQUEUE;

    RTReqQueueDestroy(pThis->hSlirpReqQueue);
    pThis->hSlirpReqQueue = NIL_RTREQQUEUE;

    RTReqQueueDestroy(pThis->hUrgRecvReqQueue);
    pThis->hUrgRecvReqQueue = NIL_RTREQQUEUE;

    RTReqQueueDestroy(pThis->hRecvReqQueue);
    pThis->hRecvReqQueue = NIL_RTREQQUEUE;

    RTSemEventDestroy(pThis->EventRecv);
    pThis->EventRecv = NIL_RTSEMEVENT;

    RTSemEventDestroy(pThis->EventUrgRecv);
    pThis->EventUrgRecv = NIL_RTSEMEVENT;

    if (RTCritSectIsInitialized(&pThis->DevAccessLock))
        RTCritSectDelete(&pThis->DevAccessLock);

    if (RTCritSectIsInitialized(&pThis->XmitLock))
        RTCritSectDelete(&pThis->XmitLock);

#ifndef RT_OS_WINDOWS
    RTPipeClose(pThis->hPipeRead);
    RTPipeClose(pThis->hPipeWrite);
#endif

#ifdef RT_OS_DARWIN
    /* Cleanup the DNS watcher. */
    if (pThis->hRunLoopSrcDnsWatcher != NULL)
    {
        CFRunLoopRef hRunLoopMain = CFRunLoopGetMain();
        CFRetain(hRunLoopMain);
        CFRunLoopRemoveSource(hRunLoopMain, pThis->hRunLoopSrcDnsWatcher, kCFRunLoopCommonModes);
        CFRelease(hRunLoopMain);
        CFRelease(pThis->hRunLoopSrcDnsWatcher);
        pThis->hRunLoopSrcDnsWatcher = NULL;
    }
#endif
}

/**
 * Construct a NAT network transport driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvNATConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    int rc = 0;

    /* Construct PDRVNAT */

    RT_NOREF(fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVNAT         pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->pNATState                    = (SlirpState *)RTMemAlloc(sizeof(SlirpState));
    if(pThis->pNATState == NULL)
    {
        return VERR_NO_MEMORY;
    }
    else
    {
        pThis->pNATState->nsock = 0;
        pThis->pNATState->pTimerHead = NULL;
        pThis->pNATState->polls = (struct pollfd *)RTMemAlloc(64 * sizeof(struct pollfd));
        pThis->pNATState->uPollCap = 64;
    }
    pThis->hSlirpReqQueue               = NIL_RTREQQUEUE;
    pThis->hUrgRecvReqQueue             = NIL_RTREQQUEUE;
    pThis->hHostResQueue                = NIL_RTREQQUEUE;
    pThis->EventRecv                    = NIL_RTSEMEVENT;
    pThis->EventUrgRecv                 = NIL_RTSEMEVENT;
#ifdef RT_OS_DARWIN
    pThis->hRunLoopSrcDnsWatcher        = NULL;
#endif

    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvNATQueryInterface;

    /* INetwork */
    pThis->INetworkUp.pfnBeginXmit          = drvNATNetworkUp_BeginXmit;
    pThis->INetworkUp.pfnAllocBuf           = drvNATNetworkUp_AllocBuf;
    pThis->INetworkUp.pfnFreeBuf            = drvNATNetworkUp_FreeBuf;
    pThis->INetworkUp.pfnSendBuf            = drvNATNetworkUp_SendBuf;
    pThis->INetworkUp.pfnEndXmit            = drvNATNetworkUp_EndXmit;
    pThis->INetworkUp.pfnSetPromiscuousMode = drvNATNetworkUp_SetPromiscuousMode;
    pThis->INetworkUp.pfnNotifyLinkChanged  = drvNATNetworkUp_NotifyLinkChanged;

    /* NAT engine configuration */
    pThis->INetworkNATCfg.pfnRedirectRuleCommand = drvNATNetworkNatConfigRedirect;
    pThis->INetworkNATCfg.pfnNotifyDnsChanged = NULL;

    /*
     * Validate the config.
     */
    PDMDRV_VALIDATE_CONFIG_RETURN(pDrvIns,
                                  "PassDomain"
                                  "|TFTPPrefix"
                                  "|BootFile"
                                  "|Network"
                                  "|NextServer"
                                  "|DNSProxy"
                                  "|BindIP"
                                  "|UseHostResolver"
                                  "|SlirpMTU"
                                  "|AliasMode"
                                  "|SockRcv"
                                  "|SockSnd"
                                  "|TcpRcv"
                                  "|TcpSnd"
                                  "|ICMPCacheLimit"
                                  "|SoMaxConnection"
                                  "|LocalhostReachable"
                                  "|HostResolverMappings"
                                  , "PortForwarding");

    /*
     * Get the configuration settings.
     */
    bool fPassDomain = true;
    GET_BOOL(rc, pDrvIns, pCfg, "PassDomain", fPassDomain);

    GET_STRING_ALLOC(rc, pDrvIns, pCfg, "TFTPPrefix", pThis->pszTFTPPrefix);
    GET_STRING_ALLOC(rc, pDrvIns, pCfg, "BootFile", pThis->pszBootFile);
    GET_STRING_ALLOC(rc, pDrvIns, pCfg, "NextServer", pThis->pszNextServer);

    int fDNSProxy = 0;
    GET_S32(rc, pDrvIns, pCfg, "DNSProxy", fDNSProxy);
    int fUseHostResolver = 0;
    GET_S32(rc, pDrvIns, pCfg, "UseHostResolver", fUseHostResolver);
    int MTU = 1500;
    GET_S32(rc, pDrvIns, pCfg, "SlirpMTU", MTU);
    int i32AliasMode = 0;
    int i32MainAliasMode = 0;
    GET_S32(rc, pDrvIns, pCfg, "AliasMode", i32MainAliasMode);
    int iIcmpCacheLimit = 100;
    GET_S32(rc, pDrvIns, pCfg, "ICMPCacheLimit", iIcmpCacheLimit);
    bool fLocalhostReachable = false;
    GET_BOOL(rc, pDrvIns, pCfg, "LocalhostReachable", fLocalhostReachable);

    i32AliasMode |= (i32MainAliasMode & 0x1 ? 0x1 : 0);
    i32AliasMode |= (i32MainAliasMode & 0x2 ? 0x40 : 0);
    i32AliasMode |= (i32MainAliasMode & 0x4 ? 0x4 : 0);
    int i32SoMaxConn = 10;
    GET_S32(rc, pDrvIns, pCfg, "SoMaxConnection", i32SoMaxConn);
    /*
     * Query the network port interface.
     */
    pThis->pIAboveNet = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMINETWORKDOWN);
    if (!pThis->pIAboveNet)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: the above device/driver didn't "
                                "export the network port interface"));
    pThis->pIAboveConfig = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMINETWORKCONFIG);
    if (!pThis->pIAboveConfig)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: the above device/driver didn't "
                                "export the network config interface"));

    /* Generate a network address for this network card. */
    char szNetwork[32]; /* xxx.xxx.xxx.xxx/yy */
    GET_STRING(rc, pDrvIns, pCfg, "Network", szNetwork[0], sizeof(szNetwork));
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT%d: Configuration error: missing network"),
                                   pDrvIns->iInstance);

    RTNETADDRIPV4 Network, Netmask;

    rc = RTCidrStrToIPv4(szNetwork, &Network, &Netmask);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("NAT#%d: Configuration error: network '%s' describes not a valid IPv4 network"),
                                   pDrvIns->iInstance, szNetwork);

    /* Construct Libslirp Config and Initialzie Slirp */

    LogFlow(("Here is what is coming out of the vbox config:\n \
            Network: %lu\n \
            Netmask: %lu\n", Network, Netmask));

#ifndef RT_OS_WINDOWS
    struct in_addr vnetwork = RTNetIPv4AddrHEToInAddr(&Network);
    struct in_addr vnetmask = RTNetIPv4AddrHEToInAddr(&Netmask);
    struct in_addr vhost = RTNetInAddrFromU8(10, 0, 2, 2);
    struct in_addr vdhcp_start = RTNetInAddrFromU8(10, 0, 2, 15);
    struct in_addr vnameserver = RTNetInAddrFromU8(10, 0, 2, 3);
#else
    struct in_addr vnetwork;
    vnetwork.S_un.S_addr = RT_BSWAP_U32(Network.u);

    struct in_addr vnetmask;
    vnetmask.S_un.S_addr = RT_BSWAP_U32(Netmask.u);

    struct in_addr vhost;
    vhost.S_un.S_addr = RT_BSWAP_U32(0x0a000202);

    struct in_addr vdhcp_start;
    vdhcp_start.S_un.S_addr = RT_BSWAP_U32(0x0a00020f);

    struct in_addr vnameserver;
    vnameserver.S_un.S_addr = RT_BSWAP_U32(0x0a000203);
#endif

    SlirpConfig *pSlirpCfg = new SlirpConfig { 0 };

    pSlirpCfg->version = 4;
    pSlirpCfg->restricted = false;
    pSlirpCfg->in_enabled = true;
    pSlirpCfg->vnetwork = vnetwork;
    pSlirpCfg->vnetmask = vnetmask;
    pSlirpCfg->vhost = vhost;
    pSlirpCfg->in6_enabled = true;

    inet_pton(AF_INET6, "fd00::", &pSlirpCfg->vprefix_addr6);
    pSlirpCfg->vprefix_len = 64;
    inet_pton(AF_INET6, "fd00::2", &pSlirpCfg->vhost6);

    pSlirpCfg->vhostname = "vbox";
    pSlirpCfg->tftp_server_name = pThis->pszNextServer;
    pSlirpCfg->tftp_path = pThis->pszTFTPPrefix;
    pSlirpCfg->bootfile = pThis->pszBootFile;
    pSlirpCfg->vdhcp_start = vdhcp_start;
    pSlirpCfg->vnameserver = vnameserver;

#ifndef RT_OS_WINDOWS
    inet_pton(AF_INET6, "fd00::3", &pSlirpCfg->vnameserver6);
#else
    inet_pton(23, "fd00::3", &pSlirpCfg->vnameserver6);
#endif

    pSlirpCfg->vdnssearch = NULL;
    pSlirpCfg->vdomainname = NULL;

    SlirpCb *slirpCallbacks = (struct SlirpCb *)RTMemAlloc(sizeof(SlirpCb));

    slirpCallbacks->send_packet = &slirpSendPacketCb;
    slirpCallbacks->guest_error = &slirpGuestErrorCb;
    slirpCallbacks->clock_get_ns = &slirpClockGetNsCb;
    slirpCallbacks->timer_new = &slirpTimerNewCb;
    slirpCallbacks->timer_free = &slirpTimerFreeCb;
    slirpCallbacks->timer_mod = &slirpTimerModCb;
    slirpCallbacks->register_poll_fd = &register_poll_fd;
    slirpCallbacks->unregister_poll_fd = &unregister_poll_fd;
    slirpCallbacks->notify = &slirpNotifyCb;
    slirpCallbacks->init_completed = NULL;
    slirpCallbacks->timer_new_opaque = NULL;

    Slirp *pSlirp = slirp_new(/* cfg */ pSlirpCfg, /* callbacks */ slirpCallbacks, /* opaque */ pThis);

    if (pSlirp == NULL)
        return VERR_INVALID_POINTER;

    pThis->pNATState->pSlirp = pSlirp;

    // pThis->pNATState->polls = NULL;

    rc = drvNATConstructRedir(pDrvIns->iInstance, pThis, pCfg, &Network);
    AssertLogRelRCReturn(rc, rc);

    rc = PDMDrvHlpSSMRegisterLoadDone(pDrvIns, NULL);
    AssertLogRelRCReturn(rc, rc);

    rc = RTReqQueueCreate(&pThis->hSlirpReqQueue);
    AssertLogRelRCReturn(rc, rc);

    rc = RTReqQueueCreate(&pThis->hRecvReqQueue);
    AssertLogRelRCReturn(rc, rc);

    rc = RTReqQueueCreate(&pThis->hUrgRecvReqQueue);
    AssertLogRelRCReturn(rc, rc);

    rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pRecvThread, pThis, drvNATRecv,
                                drvNATRecvWakeup, 256 * _1K, RTTHREADTYPE_IO, "NATRX");
    AssertRCReturn(rc, rc);

    rc = RTSemEventCreate(&pThis->EventRecv);
    AssertRCReturn(rc, rc);

    rc = RTSemEventCreate(&pThis->EventUrgRecv);
    AssertRCReturn(rc, rc);

    rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pUrgRecvThread, pThis, drvNATUrgRecv,
                                drvNATUrgRecvWakeup, 256 * _1K, RTTHREADTYPE_IO, "NATURGRX");
    AssertRCReturn(rc, rc);

    rc = RTReqQueueCreate(&pThis->hHostResQueue);
    AssertRCReturn(rc, rc);

    rc = PDMDrvHlpThreadCreate(pThis->pDrvIns, &pThis->pHostResThread,
                                pThis, drvNATHostResThread, drvNATHostResWakeup,
                                128 * _1K, RTTHREADTYPE_IO, "HOSTRES");
    AssertRCReturn(rc, rc);

    rc = RTCritSectInit(&pThis->DevAccessLock);
    AssertRCReturn(rc, rc);

    rc = RTCritSectInit(&pThis->XmitLock);
    AssertRCReturn(rc, rc);

    char szTmp[128];
    RTStrPrintf(szTmp, sizeof(szTmp), "nat%d", pDrvIns->iInstance);
    PDMDrvHlpDBGFInfoRegister(pDrvIns, szTmp, "NAT info.", drvNATInfo);

#ifdef VBOX_WITH_STATISTICS
# define DRV_PROFILE_COUNTER(name, dsc)     REGISTER_COUNTER(name, pThis, STAMTYPE_PROFILE, STAMUNIT_TICKS_PER_CALL, dsc)
# define DRV_COUNTING_COUNTER(name, dsc)    REGISTER_COUNTER(name, pThis, STAMTYPE_COUNTER, STAMUNIT_COUNT,          dsc)
# include "slirp/counters.h"
#endif

#ifndef RT_OS_WINDOWS
    /*
        * Create the control pipe.
        */
    rc = RTPipeCreate(&pThis->hPipeRead, &pThis->hPipeWrite, 0 /*fFlags*/);
    AssertRCReturn(rc, rc);
#else
    pThis->hWakeupEvent = CreateEvent(NULL, FALSE, FALSE, NULL); /* auto-reset event */
    pThis->pNATState->phEvents[VBOX_WAKEUP_EVENT_INDEX] = pThis->hWakeupEvent;
    pThis->pNATState->phEvents[VBOX_SOCKET_EVENT_INDEX] = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif

    rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pSlirpThread, pThis, drvNATAsyncIoThread,
                                drvNATAsyncIoWakeup, 256 * _1K, RTTHREADTYPE_IO, "NAT");
    AssertRCReturn(rc, rc);

    pThis->enmLinkState = pThis->enmLinkStateWant = PDMNETWORKLINKSTATE_UP;

    return rc;
}

/**
 * NAT network transport driver registration record.
 */
const PDMDRVREG g_DrvNATlibslirp =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "NAT",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "NATlibslrip Network Transport Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVNAT),
    /* pfnConstruct */
    drvNATConstruct,
    /* pfnDestruct */
    drvNATDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};
