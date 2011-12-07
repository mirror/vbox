/* $Id$ */
/** @file
 * DrvNAT - NAT network transport driver.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_NAT
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include "slirp/libslirp.h"
#include "slirp/ctl.h"
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmnetifs.h>
#include <VBox/vmm/pdmnetinline.h>

#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/cidr.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/pipe.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"

#ifndef RT_OS_WINDOWS
# include <unistd.h>
# include <fcntl.h>
# include <poll.h>
# include <errno.h>
#endif
#ifdef RT_OS_FREEBSD
# include <netinet/in.h>
#endif
#include <iprt/semaphore.h>
#include <iprt/req.h>

#define COUNTERS_INIT
#include "counters.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/**
 * @todo: This is a bad hack to prevent freezing the guest during high network
 *        activity. Windows host only. This needs to be fixed properly.
 */
#define VBOX_NAT_DELAY_HACK

#define GET_EXTRADATA(pthis, node, name, rc, type, type_name, var)                                  \
do {                                                                                                \
    (rc) = CFGMR3Query ## type((node), name, &(var));                                               \
    if (RT_FAILURE((rc)) && (rc) != VERR_CFGM_VALUE_NOT_FOUND)                                      \
        return PDMDrvHlpVMSetError((pthis)->pDrvIns, (rc), RT_SRC_POS, N_("NAT#%d: configuration query for \""name"\" " #type_name " failed"), \
                                   (pthis)->pDrvIns->iInstance);                                    \
} while (0)

#define GET_ED_STRICT(pthis, node, name, rc, type, type_name, var)                                  \
do {                                                                                                \
    (rc) = CFGMR3Query ## type((node), name, &(var));                                               \
    if (RT_FAILURE((rc)))                                                                           \
        return PDMDrvHlpVMSetError((pthis)->pDrvIns, (rc), RT_SRC_POS, N_("NAT#%d: configuration query for \""name"\" " #type_name " failed"), \
                                  (pthis)->pDrvIns->iInstance);                                     \
} while (0)

#define GET_EXTRADATA_N(pthis, node, name, rc, type, type_name, var, var_size)                      \
do {                                                                                                \
    (rc) = CFGMR3Query ## type((node), name, &(var), var_size);                                     \
    if (RT_FAILURE((rc)) && (rc) != VERR_CFGM_VALUE_NOT_FOUND)                                      \
        return PDMDrvHlpVMSetError((pthis)->pDrvIns, (rc), RT_SRC_POS, N_("NAT#%d: configuration query for \""name"\" " #type_name " failed"), \
                                  (pthis)->pDrvIns->iInstance);                                     \
} while (0)

#define GET_BOOL(rc, pthis, node, name, var) \
    GET_EXTRADATA(pthis, node, name, (rc), Bool, bolean, (var))
#define GET_STRING(rc, pthis, node, name, var, var_size) \
    GET_EXTRADATA_N(pthis, node, name, (rc), String, string, (var), (var_size))
#define GET_STRING_ALLOC(rc, pthis, node, name, var) \
    GET_EXTRADATA(pthis, node, name, (rc), StringAlloc, string, (var))
#define GET_S32(rc, pthis, node, name, var) \
    GET_EXTRADATA(pthis, node, name, (rc), S32, int, (var))
#define GET_S32_STRICT(rc, pthis, node, name, var) \
    GET_ED_STRICT(pthis, node, name, (rc), S32, int, (var))



#define DO_GET_IP(rc, node, instance, status, x)                                \
do {                                                                            \
    char    sz##x[32];                                                          \
    GET_STRING((rc), (node), (instance), #x, sz ## x[0],  sizeof(sz ## x));     \
    if (rc != VERR_CFGM_VALUE_NOT_FOUND)                                        \
        (status) = inet_aton(sz ## x, &x);                                      \
} while (0)

#define GETIP_DEF(rc, node, instance, x, def)           \
do                                                      \
{                                                       \
    int status = 0;                                     \
    DO_GET_IP((rc), (node), (instance),  status, x);    \
    if (status == 0 || rc == VERR_CFGM_VALUE_NOT_FOUND) \
        x.s_addr = def;                                 \
} while (0)

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * NAT network transport driver instance data.
 *
 * @implements  PDMINETWORKUP
 */
typedef struct DRVNAT
{
    /** The network interface. */
    PDMINETWORKUP           INetworkUp;
    /** The network NAT Engine configureation. */
    PDMINETWORKNATCONFIG    INetworkNATCfg;
    /** The port we're attached to. */
    PPDMINETWORKDOWN        pIAboveNet;
    /** The network config of the port we're attached to. */
    PPDMINETWORKCONFIG      pIAboveConfig;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Link state */
    PDMNETWORKLINKSTATE     enmLinkState;
    /** NAT state for this instance. */
    PNATState               pNATState;
    /** TFTP directory prefix. */
    char                   *pszTFTPPrefix;
    /** Boot file name to provide in the DHCP server response. */
    char                   *pszBootFile;
    /** tftp server name to provide in the DHCP server response. */
    char                   *pszNextServer;
    /** Polling thread. */
    PPDMTHREAD              pSlirpThread;
    /** Queue for NAT-thread-external events. */
    RTREQQUEUE              hSlirpReqQueue;
    /** The guest IP for port-forwarding. */
    uint32_t                GuestIP;
    /** Link state set when the VM is suspended. */
    PDMNETWORKLINKSTATE     enmLinkStateWant;

#ifdef VBOX_WITH_SLIRP_MT
    PPDMTHREAD              pGuestThread;
#endif
#ifndef RT_OS_WINDOWS
    /** The write end of the control pipe. */
    RTPIPE                  hPipeWrite;
    /** The read end of the control pipe. */
    RTPIPE                  hPipeRead;
#else
    /** for external notification */
    HANDLE                  hWakeupEvent;
#endif

#define DRV_PROFILE_COUNTER(name, dsc)     STAMPROFILE Stat ## name
#define DRV_COUNTING_COUNTER(name, dsc)    STAMCOUNTER Stat ## name
#include "counters.h"
    /** thread delivering packets for receiving by the guest */
    PPDMTHREAD              pRecvThread;
    /** thread delivering urg packets for receiving by the guest */
    PPDMTHREAD              pUrgRecvThread;
    /** event to wakeup the guest receive thread */
    RTSEMEVENT              EventRecv;
    /** event to wakeup the guest urgent receive thread */
    RTSEMEVENT              EventUrgRecv;
    /** Receive Req queue (deliver packets to the guest) */
    RTREQQUEUE              hRecvReqQueue;
    /** Receive Urgent Req queue (deliver packets to the guest). */
    RTREQQUEUE              hUrgRecvReqQueue;

    /** makes access to device func RecvAvail and Recv atomical. */
    RTCRITSECT              DevAccessLock;
    /** Number of in-flight urgent packets. */
    volatile uint32_t       cUrgPkts;
    /** Number of in-flight regular packets. */
    volatile uint32_t       cPkts;

    /** Transmit lock taken by BeginXmit and released by EndXmit. */
    RTCRITSECT              XmitLock;
} DRVNAT;
AssertCompileMemberAlignment(DRVNAT, StatNATRecvWakeups, 8);
/** Pointer the NAT driver instance data. */
typedef DRVNAT *PDRVNAT;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void drvNATNotifyNATThread(PDRVNAT pThis, const char *pszWho);


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


static DECLCALLBACK(int) drvNATRecvWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    int rc;
    rc = RTSemEventSignal(pThis->EventRecv);

    STAM_COUNTER_INC(&pThis->StatNATRecvWakeups);
    return VINF_SUCCESS;
}

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

static DECLCALLBACK(int) drvNATUrgRecvWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    int rc = RTSemEventSignal(pThis->EventUrgRecv);
    AssertRC(rc);

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) drvNATUrgRecvWorker(PDRVNAT pThis, uint8_t *pu8Buf, int cb, struct mbuf *m)
{
    int rc = RTCritSectEnter(&pThis->DevAccessLock);
    AssertRC(rc);
    rc = pThis->pIAboveNet->pfnWaitReceiveAvail(pThis->pIAboveNet, RT_INDEFINITE_WAIT);
    if (RT_SUCCESS(rc))
    {
        rc = pThis->pIAboveNet->pfnReceive(pThis->pIAboveNet, pu8Buf, cb);
        AssertRC(rc);
    }
    else if (   rc != VERR_TIMEOUT
             && rc != VERR_INTERRUPTED)
    {
        AssertRC(rc);
    }

    rc = RTCritSectLeave(&pThis->DevAccessLock);
    AssertRC(rc);

    slirp_ext_m_free(pThis->pNATState, m, pu8Buf);
    if (ASMAtomicDecU32(&pThis->cUrgPkts) == 0)
    {
        drvNATRecvWakeup(pThis->pDrvIns, pThis->pRecvThread);
        drvNATNotifyNATThread(pThis, "drvNATUrgRecvWorker");
    }
}


static DECLCALLBACK(void) drvNATRecvWorker(PDRVNAT pThis, uint8_t *pu8Buf, int cb, struct mbuf *m)
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
        rc = pThis->pIAboveNet->pfnReceive(pThis->pIAboveNet, pu8Buf, cb);
        AssertRC(rc);
    }
    else if (   rc != VERR_TIMEOUT
             && rc != VERR_INTERRUPTED)
    {
        AssertRC(rc);
    }

    rc = RTCritSectLeave(&pThis->DevAccessLock);
    AssertRC(rc);

done_unlocked:
    slirp_ext_m_free(pThis->pNATState, m, pu8Buf);
    ASMAtomicDecU32(&pThis->cPkts);

    drvNATNotifyNATThread(pThis, "drvNATRecvWorker");

    STAM_PROFILE_STOP(&pThis->StatNATRecv, a);
}

/**
 * Frees a S/G buffer allocated by drvNATNetworkUp_AllocBuf.
 *
 * @param   pThis               Pointer to the NAT instance.
 * @param   pSgBuf              The S/G buffer to free.
 */
static void drvNATFreeSgBuf(PDRVNAT pThis, PPDMSCATTERGATHER pSgBuf)
{
    Assert((pSgBuf->fFlags & PDMSCATTERGATHER_FLAGS_MAGIC_MASK) == PDMSCATTERGATHER_FLAGS_MAGIC);
    pSgBuf->fFlags = 0;
    if (pSgBuf->pvAllocator)
    {
        Assert(!pSgBuf->pvUser);
        slirp_ext_m_free(pThis->pNATState, (struct mbuf *)pSgBuf->pvAllocator, NULL);
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
static void drvNATSendWorker(PDRVNAT pThis, PPDMSCATTERGATHER pSgBuf)
{
    Assert(pThis->enmLinkState == PDMNETWORKLINKSTATE_UP);
    if (pThis->enmLinkState == PDMNETWORKLINKSTATE_UP)
    {
        struct mbuf *m = (struct mbuf *)pSgBuf->pvAllocator;
        if (m)
        {
            /*
             * A normal frame.
             */
            pSgBuf->pvAllocator = NULL;
            slirp_input(pThis->pNATState, m, pSgBuf->cbUsed);
        }
        else
        {
            /*
             * GSO frame, need to segment it.
             */
            /** @todo Make the NAT engine grok large frames?  Could be more efficient... */
#if 0 /* this is for testing PDMNetGsoCarveSegmentQD. */
            uint8_t         abHdrScratch[256];
#endif
            uint8_t const  *pbFrame = (uint8_t const *)pSgBuf->aSegs[0].pvSeg;
            PCPDMNETWORKGSO pGso    = (PCPDMNETWORKGSO)pSgBuf->pvUser;
            uint32_t const  cSegs   = PDMNetGsoCalcSegmentCount(pGso, pSgBuf->cbUsed);  Assert(cSegs > 1);
            for (size_t iSeg = 0; iSeg < cSegs; iSeg++)
            {
                size_t cbSeg;
                void  *pvSeg;
                m = slirp_ext_m_get(pThis->pNATState, pGso->cbHdrsTotal + pGso->cbMaxSeg, &pvSeg, &cbSeg);
                if (!m)
                    break;

#if 1
                uint32_t cbPayload, cbHdrs;
                uint32_t offPayload = PDMNetGsoCarveSegment(pGso, pbFrame, pSgBuf->cbUsed,
                                                            iSeg, cSegs, (uint8_t *)pvSeg, &cbHdrs, &cbPayload);
                memcpy((uint8_t *)pvSeg + cbHdrs, pbFrame + offPayload, cbPayload);

                slirp_input(pThis->pNATState, m, cbPayload + cbHdrs);
#else
                uint32_t cbSegFrame;
                void *pvSegFrame = PDMNetGsoCarveSegmentQD(pGso, (uint8_t *)pbFrame, pSgBuf->cbUsed, abHdrScratch,
                                                           iSeg, cSegs, &cbSegFrame);
                memcpy((uint8_t *)pvSeg, pvSegFrame, cbSegFrame);

                slirp_input(pThis->pNATState, m, cbSegFrame);
#endif
            }
        }
    }
    drvNATFreeSgBuf(pThis, pSgBuf);

    /** @todo Implement the VERR_TRY_AGAIN drvNATNetworkUp_AllocBuf semantics. */
}

/**
 * @interface_method_impl{PDMINETWORKUP,pfnBeginXmit}
 */
static DECLCALLBACK(int) drvNATNetworkUp_BeginXmit(PPDMINETWORKUP pInterface, bool fOnWorkerThread)
{
    PDRVNAT pThis = RT_FROM_MEMBER(pInterface, DRVNAT, INetworkUp);
    int rc = RTCritSectTryEnter(&pThis->XmitLock);
    if (RT_FAILURE(rc))
    {
        /** @todo Kick the worker thread when we have one... */
        rc = VERR_TRY_AGAIN;
    }
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
    PPDMSCATTERGATHER pSgBuf = (PPDMSCATTERGATHER)RTMemAlloc(sizeof(*pSgBuf));
    if (!pSgBuf)
        return VERR_NO_MEMORY;
    if (!pGso)
    {
        pSgBuf->pvUser      = NULL;
        pSgBuf->pvAllocator = slirp_ext_m_get(pThis->pNATState, cbMin,
                                              &pSgBuf->aSegs[0].pvSeg, &pSgBuf->aSegs[0].cbSeg);
        if (!pSgBuf->pvAllocator)
        {
            RTMemFree(pSgBuf);
            return VERR_TRY_AGAIN;
        }
    }
    else
    {
        pSgBuf->pvUser      = RTMemDup(pGso, sizeof(*pGso));
        pSgBuf->pvAllocator = NULL;
        pSgBuf->aSegs[0].cbSeg = RT_ALIGN_Z(cbMin, 16);
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

#if 0 /* poison */
    memset(pSgBuf->aSegs[0].pvSeg, 'F', pSgBuf->aSegs[0].cbSeg);
#endif
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
    PDRVNAT pThis = RT_FROM_MEMBER(pInterface, DRVNAT, INetworkUp);
    Assert((pSgBuf->fFlags & PDMSCATTERGATHER_FLAGS_OWNER_MASK) == PDMSCATTERGATHER_FLAGS_OWNER_1);
    Assert(RTCritSectIsOwner(&pThis->XmitLock));

    int rc;
    if (pThis->pSlirpThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        /* Set an FTM checkpoint as this operation changes the state permanently. */
        PDMDrvHlpFTSetCheckpoint(pThis->pDrvIns, FTMCHECKPOINTTYPE_NETWORK);

#ifdef VBOX_WITH_SLIRP_MT
        RTREQQUEUE hQueue = (RTREQQUEUE)slirp_get_queue(pThis->pNATState);
#else
        RTREQQUEUE hQueue = pThis->hSlirpReqQueue;
#endif
        rc = RTReqQueueCallEx(hQueue, NULL /*ppReq*/, 0 /*cMillies*/, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                              (PFNRT)drvNATSendWorker, 2, pThis, pSgBuf);
        if (RT_SUCCESS(rc))
        {
            drvNATNotifyNATThread(pThis, "drvNATNetworkUp_SendBuf");
            return VINF_SUCCESS;
        }

        rc = VERR_NET_NO_BUFFER_SPACE;
    }
    else
        rc = VERR_NET_DOWN;
    drvNATFreeSgBuf(pThis, pSgBuf);
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
    LogFlow(("drvNATNetworkUp_SetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    /* nothing to do */
}

/**
 * Worker function for drvNATNetworkUp_NotifyLinkChanged().
 * @thread "NAT" thread.
 */
static void drvNATNotifyLinkChangedWorker(PDRVNAT pThis, PDMNETWORKLINKSTATE enmLinkState)
{
    pThis->enmLinkState = pThis->enmLinkStateWant = enmLinkState;
    switch (enmLinkState)
    {
        case PDMNETWORKLINKSTATE_UP:
            LogRel(("NAT: link up\n"));
            slirp_link_up(pThis->pNATState);
            break;

        case PDMNETWORKLINKSTATE_DOWN:
        case PDMNETWORKLINKSTATE_DOWN_RESUME:
            LogRel(("NAT: link down\n"));
            slirp_link_down(pThis->pNATState);
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

    /* Don't queue new requests when the NAT thread is about to stop.
     * But the VM could also be paused. So memorize the desired state. */
    if (pThis->pSlirpThread->enmState != PDMTHREADSTATE_RUNNING)
    {
        pThis->enmLinkStateWant = enmLinkState;
        return;
    }

    PRTREQ pReq;
    int rc = RTReqQueueCallEx(pThis->hSlirpReqQueue, &pReq, 0 /*cMillies*/, RTREQFLAGS_VOID,
                              (PFNRT)drvNATNotifyLinkChangedWorker, 2, pThis, enmLinkState);
    if (RT_LIKELY(rc == VERR_TIMEOUT))
    {
        drvNATNotifyNATThread(pThis, "drvNATNetworkUp_NotifyLinkChanged");
        rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertRC(rc);
    }
    else
        AssertRC(rc);
    RTReqRelease(pReq);
}

static void drvNATNotifyApplyPortForwardCommand(PDRVNAT pThis, bool fRemove,
                                                bool fUdp, const char *pHostIp,
                                                uint16_t u16HostPort, const char *pGuestIp, uint16_t u16GuestPort)
{
    RTMAC Mac;
    RT_ZERO(Mac); /* can't get MAC here */
    if (pThis->pIAboveConfig)
        pThis->pIAboveConfig->pfnGetMac(pThis->pIAboveConfig, &Mac);

    struct in_addr guestIp, hostIp;

    if (   pHostIp == NULL
        || inet_aton(pHostIp, &hostIp) == 0)
        hostIp.s_addr = INADDR_ANY;

    if (   pGuestIp == NULL
        || inet_aton(pGuestIp, &guestIp) == 0)
        guestIp.s_addr = pThis->GuestIP;

    if (fRemove)
        slirp_remove_redirect(pThis->pNATState, fUdp, hostIp, u16HostPort, guestIp, u16GuestPort);
    else
        slirp_add_redirect(pThis->pNATState, fUdp, hostIp, u16HostPort, guestIp, u16GuestPort, Mac.au8);
}

DECLCALLBACK(int) drvNATNetworkNatConfig_RedirectRuleCommand(PPDMINETWORKNATCONFIG pInterface, bool fRemove,
                                                             bool fUdp, const char *pHostIp,
                                                             uint16_t u16HostPort, const char *pGuestIp, uint16_t u16GuestPort)
{
    LogFlowFunc(("fRemove=%d, fUdp=%d, pHostIp=%s, u16HostPort=%u, pGuestIp=%s, u16GuestPort=%u\n",
                 RT_BOOL(fRemove), RT_BOOL(fUdp), pHostIp, u16HostPort, pGuestIp,
                 u16GuestPort));
    PDRVNAT pThis = RT_FROM_MEMBER(pInterface, DRVNAT, INetworkNATCfg);
    PRTREQ pReq;
    int rc = RTReqQueueCallEx(pThis->hSlirpReqQueue, &pReq, 0 /*cMillies*/, RTREQFLAGS_VOID,
                              (PFNRT)drvNATNotifyApplyPortForwardCommand, 7, pThis, fRemove,
                              fUdp, pHostIp, u16HostPort, pGuestIp, u16GuestPort);
    if (RT_LIKELY(rc == VERR_TIMEOUT))
    {
        drvNATNotifyNATThread(pThis, "drvNATNetworkNatConfig_RedirectRuleCommand");
        rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertRC(rc);
    }
    else
        AssertRC(rc);

    RTReqRelease(pReq);
    port_forwarding_done:
    return rc;
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
    int     nFDs = -1;
#ifdef RT_OS_WINDOWS
    HANDLE  *phEvents = slirp_get_events(pThis->pNATState);
    unsigned int cBreak = 0;
#else /* RT_OS_WINDOWS */
    unsigned int cPollNegRet = 0;
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
        nFDs = slirp_get_nsock(pThis->pNATState);
        /* allocation for all sockets + Management pipe */
        struct pollfd *polls = (struct pollfd *)RTMemAlloc((1 + nFDs) * sizeof(struct pollfd) + sizeof(uint32_t));
        if (polls == NULL)
            return VERR_NO_MEMORY;

        /* don't pass the management pipe */
        slirp_select_fill(pThis->pNATState, &nFDs, &polls[1]);

        polls[0].fd = RTPipeToNative(pThis->hPipeRead);
        /* POLLRDBAND usually doesn't used on Linux but seems used on Solaris */
        polls[0].events = POLLRDNORM | POLLPRI | POLLRDBAND;
        polls[0].revents = 0;

        int cChangedFDs = poll(polls, nFDs + 1, slirp_get_timeout_ms(pThis->pNATState));
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
                LogRel(("NAT:Poll returns (%s) suppressed %d\n", strerror(errno), cPollNegRet));
                cPollNegRet = 0;
            }
        }

        if (cChangedFDs >= 0)
        {
            slirp_select_poll(pThis->pNATState, &polls[1], nFDs);
            if (polls[0].revents & (POLLRDNORM|POLLPRI|POLLRDBAND))
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
        }
        /* process _all_ outstanding requests but don't wait */
        RTReqQueueProcess(pThis->hSlirpReqQueue, 0);
        RTMemFree(polls);

#else /* RT_OS_WINDOWS */
        nFDs = -1;
        slirp_select_fill(pThis->pNATState, &nFDs);
        DWORD dwEvent = WSAWaitForMultipleEvents(nFDs, phEvents, FALSE,
                                                 slirp_get_timeout_ms(pThis->pNATState),
                                                 FALSE);
        if (   (dwEvent < WSA_WAIT_EVENT_0 || dwEvent > WSA_WAIT_EVENT_0 + nFDs - 1)
            && dwEvent != WSA_WAIT_TIMEOUT)
        {
            int error = WSAGetLastError();
            LogRel(("NAT: WSAWaitForMultipleEvents returned %d (error %d)\n", dwEvent, error));
            RTAssertPanic();
        }

        if (dwEvent == WSA_WAIT_TIMEOUT)
        {
            /* only check for slow/fast timers */
            slirp_select_poll(pThis->pNATState, /* fTimeout=*/true, /*fIcmp=*/false);
            continue;
        }
        /* poll the sockets in any case */
        Log2(("%s: poll\n", __FUNCTION__));
        slirp_select_poll(pThis->pNATState, /* fTimeout=*/false, /* fIcmp=*/(dwEvent == WSA_WAIT_EVENT_0));
        /* process _all_ outstanding requests but don't wait */
        RTReqQueueProcess(pThis->hSlirpReqQueue, 0);
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
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    drvNATNotifyNATThread(pThis, "drvNATAsyncIoWakeup");
    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_SLIRP_MT

static DECLCALLBACK(int) drvNATAsyncIoGuest(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
        slirp_process_queue(pThis->pNATState);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) drvNATAsyncIoGuestWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    return VINF_SUCCESS;
}

#endif /* VBOX_WITH_SLIRP_MT */

/**
 * Function called by slirp to check if it's possible to feed incoming data to the network port.
 * @returns 1 if possible.
 * @returns 0 if not possible.
 */
int slirp_can_output(void *pvUser)
{
    return 1;
}

void slirp_push_recv_thread(void *pvUser)
{
    PDRVNAT pThis = (PDRVNAT)pvUser;
    Assert(pThis);
    drvNATUrgRecvWakeup(pThis->pDrvIns, pThis->pUrgRecvThread);
}

void slirp_urg_output(void *pvUser, struct mbuf *m, const uint8_t *pu8Buf, int cb)
{
    PDRVNAT pThis = (PDRVNAT)pvUser;
    Assert(pThis);

    PRTREQ pReq = NULL;

    /* don't queue new requests when the NAT thread is about to stop */
    if (pThis->pSlirpThread->enmState != PDMTHREADSTATE_RUNNING)
        return;

    ASMAtomicIncU32(&pThis->cUrgPkts);
    int rc = RTReqQueueCallEx(pThis->hUrgRecvReqQueue, NULL /*ppReq*/, 0 /*cMillies*/, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                              (PFNRT)drvNATUrgRecvWorker, 4, pThis, pu8Buf, cb, m);
    AssertRC(rc);
    drvNATUrgRecvWakeup(pThis->pDrvIns, pThis->pUrgRecvThread);
}

/**
 * Function called by slirp to wake up device after VERR_TRY_AGAIN
 */
void slirp_output_pending(void *pvUser)
{
    PDRVNAT pThis = (PDRVNAT)pvUser;
    Assert(pThis);
    pThis->pIAboveNet->pfnXmitPending(pThis->pIAboveNet);
}

/**
 * Function called by slirp to feed incoming data to the NIC.
 */
void slirp_output(void *pvUser, struct mbuf *m, const uint8_t *pu8Buf, int cb)
{
    PDRVNAT pThis = (PDRVNAT)pvUser;
    Assert(pThis);

    LogFlow(("slirp_output BEGIN %x %d\n", pu8Buf, cb));
    Log2(("slirp_output: pu8Buf=%p cb=%#x (pThis=%p)\n%.*Rhxd\n", pu8Buf, cb, pThis, cb, pu8Buf));

    PRTREQ pReq = NULL;

    /* don't queue new requests when the NAT thread is about to stop */
    if (pThis->pSlirpThread->enmState != PDMTHREADSTATE_RUNNING)
        return;

    ASMAtomicIncU32(&pThis->cPkts);
    int rc = RTReqQueueCallEx(pThis->hRecvReqQueue, NULL /*ppReq*/, 0 /*cMillies*/, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                              (PFNRT)drvNATRecvWorker, 4, pThis, pu8Buf, cb, m);
    AssertRC(rc);
    drvNATRecvWakeup(pThis->pDrvIns, pThis->pRecvThread);
    STAM_COUNTER_INC(&pThis->StatQueuePktSent);
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
 * Get the MAC address into the slirp stack.
 *
 * Called by drvNATLoadDone and drvNATPowerOn.
 */
static void drvNATSetMac(PDRVNAT pThis)
{
    if (pThis->pIAboveConfig)
    {
        RTMAC Mac;
        pThis->pIAboveConfig->pfnGetMac(pThis->pIAboveConfig, &Mac);
        /* Re-activate the port forwarding. If  */
        slirp_set_ethaddr_and_activate_port_forwarding(pThis->pNATState, Mac.au8, pThis->GuestIP);
    }
}


/**
 * After loading we have to pass the MAC address of the ethernet device to the slirp stack.
 * Otherwise the guest is not reachable until it performs a DHCP request or an ARP request
 * (usually done during guest boot).
 */
static DECLCALLBACK(int) drvNATLoadDone(PPDMDRVINS pDrvIns, PSSMHANDLE pSSMHandle)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    drvNATSetMac(pThis);
    return VINF_SUCCESS;
}


/**
 * Some guests might not use DHCP to retrieve an IP but use a static IP.
 */
static DECLCALLBACK(void) drvNATPowerOn(PPDMDRVINS pDrvIns)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    drvNATSetMac(pThis);
}


/**
 * Info handler.
 */
static DECLCALLBACK(void) drvNATInfo(PPDMDRVINS pDrvIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    slirp_info(pThis->pNATState, pHlp, pszArgs);
}


/**
 * Sets up the redirectors.
 *
 * @returns VBox status code.
 * @param   pCfg            The configuration handle.
 */
static int drvNATConstructRedir(unsigned iInstance, PDRVNAT pThis, PCFGMNODE pCfg, RTIPV4ADDR Network)
{
    RTMAC Mac;
    RT_ZERO(Mac); /* can't get MAC here */

    /*
     * Enumerate redirections.
     */
    for (PCFGMNODE pNode = CFGMR3GetFirstChild(pCfg); pNode; pNode = CFGMR3GetNextChild(pNode))
    {
        /*
         * Validate the port forwarding config.
         */
        if (!CFGMR3AreValuesValid(pNode, "Protocol\0UDP\0HostPort\0GuestPort\0GuestIP\0BindIP\0"))
            return PDMDRV_SET_ERROR(pThis->pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES,
                                    N_("Unknown configuration in port forwarding"));

        /* protocol type */
        bool fUDP;
        char szProtocol[32];
        int rc;
        GET_STRING(rc, pThis, pNode, "Protocol", szProtocol[0], sizeof(szProtocol));
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        {
            fUDP = false;
            GET_BOOL(rc, pThis, pNode, "UDP", fUDP);
        }
        else if (RT_SUCCESS(rc))
        {
            if (!RTStrICmp(szProtocol, "TCP"))
                fUDP = false;
            else if (!RTStrICmp(szProtocol, "UDP"))
                fUDP = true;
            else
                return PDMDrvHlpVMSetError(pThis->pDrvIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                    N_("NAT#%d: Invalid configuration value for \"Protocol\": \"%s\""),
                    iInstance, szProtocol);
        }
        else
            return PDMDrvHlpVMSetError(pThis->pDrvIns, rc, RT_SRC_POS,
                                       N_("NAT#%d: configuration query for \"Protocol\" failed"),
                                       iInstance);
        /* host port */
        int32_t iHostPort;
        GET_S32_STRICT(rc, pThis, pNode, "HostPort", iHostPort);

        /* guest port */
        int32_t iGuestPort;
        GET_S32_STRICT(rc, pThis, pNode, "GuestPort", iGuestPort);

        /* guest address */
        struct in_addr GuestIP;
        /* @todo (vvl) use CTL_* */
        GETIP_DEF(rc, pThis, pNode, GuestIP, htonl(Network | CTL_GUEST));

        /* Store the guest IP for re-establishing the port-forwarding rules. Note that GuestIP
         * is not documented. Without */
        if (pThis->GuestIP == INADDR_ANY)
            pThis->GuestIP = GuestIP.s_addr;

        /*
         * Call slirp about it.
         */
        struct in_addr BindIP;
        GETIP_DEF(rc, pThis, pNode, BindIP, INADDR_ANY);
        if (slirp_add_redirect(pThis->pNATState, fUDP, BindIP, iHostPort, GuestIP, iGuestPort, Mac.au8) < 0)
            return PDMDrvHlpVMSetError(pThis->pDrvIns, VERR_NAT_REDIR_SETUP, RT_SRC_POS,
                                       N_("NAT#%d: configuration error: failed to set up "
                                       "redirection of %d to %d. Probably a conflict with "
                                       "existing services or other rules"), iInstance, iHostPort,
                                       iGuestPort);
    } /* for each redir rule */

    return VINF_SUCCESS;
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
        slirp_term(pThis->pNATState);
        slirp_deregister_statistics(pThis->pNATState, pDrvIns);
#ifdef VBOX_WITH_STATISTICS
# define DRV_PROFILE_COUNTER(name, dsc)     DEREGISTER_COUNTER(name, pThis)
# define DRV_COUNTING_COUNTER(name, dsc)    DEREGISTER_COUNTER(name, pThis)
# include "counters.h"
#endif
        pThis->pNATState = NULL;
    }

    RTReqQueueDestroy(pThis->hSlirpReqQueue);
    pThis->hSlirpReqQueue = NIL_RTREQQUEUE;

    RTReqQueueDestroy(pThis->hUrgRecvReqQueue);
    pThis->hUrgRecvReqQueue = NIL_RTREQQUEUE;

    RTSemEventDestroy(pThis->EventRecv);
    pThis->EventRecv = NIL_RTSEMEVENT;

    RTSemEventDestroy(pThis->EventUrgRecv);
    pThis->EventUrgRecv = NIL_RTSEMEVENT;

    if (RTCritSectIsInitialized(&pThis->DevAccessLock))
        RTCritSectDelete(&pThis->DevAccessLock);

    if (RTCritSectIsInitialized(&pThis->XmitLock))
        RTCritSectDelete(&pThis->XmitLock);
}


/**
 * Construct a NAT network transport driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvNATConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    LogFlow(("drvNATConstruct:\n"));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "PassDomain\0TFTPPrefix\0BootFile\0Network"
                              "\0NextServer\0DNSProxy\0BindIP\0UseHostResolver\0"
                              "SlirpMTU\0AliasMode\0"
                              "SockRcv\0SockSnd\0TcpRcv\0TcpSnd\0"
                              "ICMPCacheLimit\0"
                              "SoMaxConnection\0"))
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES,
                                N_("Unknown NAT configuration option, only supports PassDomain,"
                                " TFTPPrefix, BootFile and Network"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->pNATState                    = NULL;
    pThis->pszTFTPPrefix                = NULL;
    pThis->pszBootFile                  = NULL;
    pThis->pszNextServer                = NULL;
    pThis->hSlirpReqQueue               = NIL_RTREQQUEUE;
    pThis->hUrgRecvReqQueue             = NIL_RTREQQUEUE;
    pThis->EventRecv                    = NIL_RTSEMEVENT;
    pThis->EventUrgRecv                 = NIL_RTSEMEVENT;

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
    pThis->INetworkNATCfg.pfnRedirectRuleCommand = drvNATNetworkNatConfig_RedirectRuleCommand;

    /*
     * Get the configuration settings.
     */
    int rc;
    bool fPassDomain = true;
    GET_BOOL(rc, pThis, pCfg, "PassDomain", fPassDomain);

    GET_STRING_ALLOC(rc, pThis, pCfg, "TFTPPrefix", pThis->pszTFTPPrefix);
    GET_STRING_ALLOC(rc, pThis, pCfg, "BootFile", pThis->pszBootFile);
    GET_STRING_ALLOC(rc, pThis, pCfg, "NextServer", pThis->pszNextServer);

    int fDNSProxy = 0;
    GET_S32(rc, pThis, pCfg, "DNSProxy", fDNSProxy);
    int fUseHostResolver = 0;
    GET_S32(rc, pThis, pCfg, "UseHostResolver", fUseHostResolver);
    int MTU = 1500;
    GET_S32(rc, pThis, pCfg, "SlirpMTU", MTU);
    int i32AliasMode = 0;
    int i32MainAliasMode = 0;
    GET_S32(rc, pThis, pCfg, "AliasMode", i32MainAliasMode);
    int iIcmpCacheLimit = 100;
    GET_S32(rc, pThis, pCfg, "ICMPCacheLimit", iIcmpCacheLimit);

    i32AliasMode |= (i32MainAliasMode & 0x1 ? 0x1 : 0);
    i32AliasMode |= (i32MainAliasMode & 0x2 ? 0x40 : 0);
    i32AliasMode |= (i32MainAliasMode & 0x4 ? 0x4 : 0);
    int i32SoMaxConn = 10;
    GET_S32(rc, pThis, pCfg, "SoMaxConnection", i32SoMaxConn);
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
    GET_STRING(rc, pThis, pCfg, "Network", szNetwork[0], sizeof(szNetwork));
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT%d: Configuration error: "
                                   "missing network"),
                                   pDrvIns->iInstance, szNetwork);

    RTIPV4ADDR Network;
    RTIPV4ADDR Netmask;
    rc = RTCidrStrToIPv4(szNetwork, &Network, &Netmask);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: Configuration error: "
                                   "network '%s' describes not a valid IPv4 network"),
                                   pDrvIns->iInstance, szNetwork);

    /*
     * Initialize slirp.
     */
    rc = slirp_init(&pThis->pNATState, RT_H2N_U32(Network), Netmask,
                    fPassDomain, !!fUseHostResolver, i32AliasMode,
                    iIcmpCacheLimit, pThis);
    if (RT_SUCCESS(rc))
    {
        slirp_set_dhcp_TFTP_prefix(pThis->pNATState, pThis->pszTFTPPrefix);
        slirp_set_dhcp_TFTP_bootfile(pThis->pNATState, pThis->pszBootFile);
        slirp_set_dhcp_next_server(pThis->pNATState, pThis->pszNextServer);
        slirp_set_dhcp_dns_proxy(pThis->pNATState, !!fDNSProxy);
        slirp_set_mtu(pThis->pNATState, MTU);
        slirp_set_somaxconn(pThis->pNATState, i32SoMaxConn);
        char *pszBindIP = NULL;
        GET_STRING_ALLOC(rc, pThis, pCfg, "BindIP", pszBindIP);
        rc = slirp_set_binding_address(pThis->pNATState, pszBindIP);
        if (rc != 0 && pszBindIP && *pszBindIP)
            LogRel(("NAT: value of BindIP has been ignored\n"));

        if(pszBindIP != NULL)
            MMR3HeapFree(pszBindIP);
#define SLIRP_SET_TUNING_VALUE(name, setter)                    \
            do                                                  \
            {                                                   \
                int len = 0;                                    \
                rc = CFGMR3QueryS32(pCfg, name, &len);    \
                if (RT_SUCCESS(rc))                             \
                    setter(pThis->pNATState, len);              \
            } while(0)

        SLIRP_SET_TUNING_VALUE("SockRcv", slirp_set_rcvbuf);
        SLIRP_SET_TUNING_VALUE("SockSnd", slirp_set_sndbuf);
        SLIRP_SET_TUNING_VALUE("TcpRcv", slirp_set_tcp_rcvspace);
        SLIRP_SET_TUNING_VALUE("TcpSnd", slirp_set_tcp_sndspace);

        slirp_register_statistics(pThis->pNATState, pDrvIns);
#ifdef VBOX_WITH_STATISTICS
# define DRV_PROFILE_COUNTER(name, dsc)     REGISTER_COUNTER(name, pThis, STAMTYPE_PROFILE, STAMUNIT_TICKS_PER_CALL, dsc)
# define DRV_COUNTING_COUNTER(name, dsc)    REGISTER_COUNTER(name, pThis, STAMTYPE_COUNTER, STAMUNIT_COUNT,          dsc)
# include "counters.h"
#endif

        rc = drvNATConstructRedir(pDrvIns->iInstance, pThis, pCfg, Network);
        if (RT_SUCCESS(rc))
        {
            /*
             * Register a load done notification to get the MAC address into the slirp
             * engine after we loaded a guest state.
             */
            rc = PDMDrvHlpSSMRegisterLoadDone(pDrvIns, drvNATLoadDone);
            AssertLogRelRCReturn(rc, rc);

            rc = RTReqQueueCreate(&pThis->hSlirpReqQueue);
            AssertLogRelRCReturn(rc, rc);

            rc = RTReqQueueCreate(&pThis->hRecvReqQueue);
            AssertLogRelRCReturn(rc, rc);

            rc = RTReqQueueCreate(&pThis->hUrgRecvReqQueue);
            AssertLogRelRCReturn(rc, rc);

            rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pRecvThread, pThis, drvNATRecv,
                                       drvNATRecvWakeup, 128 * _1K, RTTHREADTYPE_IO, "NATRX");
            AssertRCReturn(rc, rc);

            rc = RTSemEventCreate(&pThis->EventRecv);
            AssertRCReturn(rc, rc);

            rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pUrgRecvThread, pThis, drvNATUrgRecv,
                                       drvNATUrgRecvWakeup, 128 * _1K, RTTHREADTYPE_IO, "NATURGRX");
            AssertRCReturn(rc, rc);

            rc = RTSemEventCreate(&pThis->EventRecv);
            AssertRCReturn(rc, rc);

            rc = RTSemEventCreate(&pThis->EventUrgRecv);
            AssertRCReturn(rc, rc);

            rc = RTCritSectInit(&pThis->DevAccessLock);
            AssertRCReturn(rc, rc);

            rc = RTCritSectInit(&pThis->XmitLock);
            AssertRCReturn(rc, rc);

            char szTmp[128];
            RTStrPrintf(szTmp, sizeof(szTmp), "nat%d", pDrvIns->iInstance);
            PDMDrvHlpDBGFInfoRegister(pDrvIns, szTmp, "NAT info.", drvNATInfo);

#ifndef RT_OS_WINDOWS
            /*
             * Create the control pipe.
             */
            rc = RTPipeCreate(&pThis->hPipeRead, &pThis->hPipeWrite, 0 /*fFlags*/);
            AssertRCReturn(rc, rc);
#else
            pThis->hWakeupEvent = CreateEvent(NULL, FALSE, FALSE, NULL); /* auto-reset event */
            slirp_register_external_event(pThis->pNATState, pThis->hWakeupEvent,
                                          VBOX_WAKEUP_EVENT_INDEX);
#endif

            rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pSlirpThread, pThis, drvNATAsyncIoThread,
                                       drvNATAsyncIoWakeup, 128 * _1K, RTTHREADTYPE_IO, "NAT");
            AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_SLIRP_MT
            rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pGuestThread, pThis, drvNATAsyncIoGuest,
                                       drvNATAsyncIoGuestWakeup, 128 * _1K, RTTHREADTYPE_IO, "NATGUEST");
            AssertRCReturn(rc, rc);
#endif

            pThis->enmLinkState = pThis->enmLinkStateWant = PDMNETWORKLINKSTATE_UP;

            /* might return VINF_NAT_DNS */
            return rc;
        }

        /* failure path */
        slirp_term(pThis->pNATState);
        pThis->pNATState = NULL;
    }
    else
    {
        PDMDRV_SET_ERROR(pDrvIns, rc, N_("Unknown error during NAT networking setup: "));
        AssertMsgFailed(("Add error message for rc=%d (%Rrc)\n", rc, rc));
    }

    return rc;
}


/**
 * NAT network transport driver registration record.
 */
const PDMDRVREG g_DrvNAT =
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
    "NAT Network Transport Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    ~0,
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
    drvNATPowerOn,
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

