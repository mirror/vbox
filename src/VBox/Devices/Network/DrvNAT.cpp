/* $Id$ */
/** @file
 * DrvNAT - NAT network transport driver.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
#define LOG_GROUP LOG_GROUP_DRV_NAT
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include "slirp/libslirp.h"
#include "slirp/ctl.h"
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/critsect.h>
#include <iprt/cidr.h>
#include <iprt/stream.h>

#include "Builtins.h"

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
 */
typedef struct DRVNAT
{
    /** The network interface. */
    PDMINETWORKCONNECTOR    INetworkConnector;
    /** The port we're attached to. */
    PPDMINETWORKPORT        pPort;
    /** The network config of the port we're attached to. */
    PPDMINETWORKCONFIG      pConfig;
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
    /* polling thread */
    PPDMTHREAD              pSlirpThread;
    /** Queue for NAT-thread-external events. */
    PRTREQQUEUE             pSlirpReqQueue;

#ifdef VBOX_WITH_SLIRP_MT
    PPDMTHREAD              pGuestThread;
#endif
#ifndef RT_OS_WINDOWS
    /** The write end of the control pipe. */
    RTFILE                  PipeWrite;
    /** The read end of the control pipe. */
    RTFILE                  PipeRead;
# if HC_ARCH_BITS == 32
    /** Alignment padding. */
    uint32_t                u32Alignment;
# endif
#else
    /** for external notification */
    HANDLE                  hWakeupEvent;
#endif

#define DRV_PROFILE_COUNTER(name, dsc)     STAMPROFILE Stat ## name
#define DRV_COUNTING_COUNTER(name, dsc)    STAMCOUNTER Stat ## name
#include "counters.h"
    /** thread delivering packets for receiving by the guest */
    PPDMTHREAD              pRecvThread;
    /** event to wakeup the guest receive thread */
    RTSEMEVENT              EventRecv;
    /** Receive Req queue (deliver packets to the guest) */
    PRTREQQUEUE             pRecvReqQueue;
} DRVNAT;
AssertCompileMemberAlignment(DRVNAT, StatNATRecvWakeups, 8);
/** Pointer the NAT driver instance data. */
typedef DRVNAT *PDRVNAT;

/**
 * NAT queue item.
 */
typedef struct DRVNATQUEUITEM
{
    /** The core part owned by the queue manager. */
    PDMQUEUEITEMCORE    Core;
    /** The buffer for output to guest. */
    const uint8_t       *pu8Buf;
    /* size of buffer */
    size_t              cb;
    void                *mbuf;
} DRVNATQUEUITEM;
/** Pointer to a NAT queue item. */
typedef DRVNATQUEUITEM *PDRVNATQUEUITEM;


static void drvNATNotifyNATThread(PDRVNAT pThis);


/** Converts a pointer to NAT::INetworkConnector to a PRDVNAT. */
#define PDMINETWORKCONNECTOR_2_DRVNAT(pInterface)   ( (PDRVNAT)((uintptr_t)pInterface - RT_OFFSETOF(DRVNAT, INetworkConnector)) )



static DECLCALLBACK(int) drvNATRecv(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
 {
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        RTReqProcess(pThis->pRecvReqQueue, 0);
        RTSemEventWait(pThis->EventRecv, RT_INDEFINITE_WAIT);
    }
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) drvNATRecvWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    int rc = RTSemEventSignal(pThis->EventRecv);

    STAM_COUNTER_INC(&pThis->StatNATRecvWakeups);
    AssertReleaseRC(rc);
    return VINF_SUCCESS;
}


static DECLCALLBACK(void) drvNATRecvWorker(PDRVNAT pThis, uint8_t *pu8Buf, int cb)
{
    STAM_PROFILE_START(&pThis->StatNATRecv, a);

    STAM_PROFILE_START(&pThis->StatNATRecvWait, b);
    int rc = pThis->pPort->pfnWaitReceiveAvail(pThis->pPort, RT_INDEFINITE_WAIT);
    AssertMsgRC(rc, ("NAT: No RX available even on indefinite wait; rc=%Rrc", rc));

    STAM_PROFILE_STOP(&pThis->StatNATRecvWait, b);

    rc = pThis->pPort->pfnReceive(pThis->pPort, pu8Buf, cb);
    AssertRC(rc);
    RTMemFree(pu8Buf);

    STAM_PROFILE_STOP(&pThis->StatNATRecv, a);
}

/**
 * Worker function for drvNATSend().
 * @thread "NAT" thread.
 */
static void drvNATSendWorker(PDRVNAT pThis, const void *pvBuf, size_t cb)
{
    Assert(pThis->enmLinkState == PDMNETWORKLINKSTATE_UP);
    if (pThis->enmLinkState == PDMNETWORKLINKSTATE_UP)
        slirp_input(pThis->pNATState, (uint8_t *)pvBuf, cb);
}


/**
 * Called by the guest to send data to the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           Data to send.
 * @param   cb              Number of bytes to send.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvNATSend(PPDMINETWORKCONNECTOR pInterface, const void *pvBuf, size_t cb)
{
    PDRVNAT pThis = PDMINETWORKCONNECTOR_2_DRVNAT(pInterface);

    LogFlow(("drvNATSend: pvBuf=%p cb=%#x\n", pvBuf, cb));
    Log2(("drvNATSend: pvBuf=%p cb=%#x\n%.*Rhxd\n", pvBuf, cb, cb, pvBuf));

    PRTREQ pReq = NULL;
    int rc;
    void *buf;

    /* don't queue new requests when the NAT thread is about to stop */
    if (pThis->pSlirpThread->enmState != PDMTHREADSTATE_RUNNING)
        return VINF_SUCCESS;

#ifndef VBOX_WITH_SLIRP_MT
    rc = RTReqAlloc(pThis->pSlirpReqQueue, &pReq, RTREQTYPE_INTERNAL);
#else
    rc = RTReqAlloc((PRTREQQUEUE)slirp_get_queue(pThis->pNATState), &pReq, RTREQTYPE_INTERNAL);
#endif
    AssertReleaseRC(rc);

    /* @todo: Here we should get mbuf instead temporal buffer */
    buf = RTMemAlloc(cb);
    if (buf == NULL)
    {
        LogRel(("NAT: Can't allocate send buffer\n"));
        return VERR_NO_MEMORY;
    }
    memcpy(buf, pvBuf, cb);

    pReq->u.Internal.pfn      = (PFNRT)drvNATSendWorker;
    pReq->u.Internal.cArgs    = 3;
    pReq->u.Internal.aArgs[0] = (uintptr_t)pThis;
    pReq->u.Internal.aArgs[1] = (uintptr_t)buf;
    pReq->u.Internal.aArgs[2] = (uintptr_t)cb;
    pReq->fFlags              = RTREQFLAGS_VOID|RTREQFLAGS_NO_WAIT;

    rc = RTReqQueue(pReq, 0); /* don't wait, we have to wakeup the NAT thread fist */
    AssertReleaseRC(rc);
    drvNATNotifyNATThread(pThis);
    LogFlow(("drvNATSend: end\n"));
    return VINF_SUCCESS;
}


/**
 * Get the NAT thread out of poll/WSAWaitForMultipleEvents
 */
static void drvNATNotifyNATThread(PDRVNAT pThis)
{
    int rc;
#ifndef RT_OS_WINDOWS
    /* kick select() */
    rc = RTFileWrite(pThis->PipeWrite, "", 1, NULL);
#else
    /* kick WSAWaitForMultipleEvents */
    rc = WSASetEvent(pThis->hWakeupEvent);
#endif
    AssertReleaseRC(rc);
}


/**
 * Set promiscuous mode.
 *
 * This is called when the promiscuous mode is set. This means that there doesn't have
 * to be a mode change when it's called.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   fPromiscuous    Set if the adaptor is now in promiscuous mode. Clear if it is not.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvNATSetPromiscuousMode(PPDMINETWORKCONNECTOR pInterface, bool fPromiscuous)
{
    LogFlow(("drvNATSetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    /* nothing to do */
}

/**
 * Worker function for drvNATNotifyLinkChanged().
 * @thread "NAT" thread.
 */
static void drvNATNotifyLinkChangedWorker(PDRVNAT pThis, PDMNETWORKLINKSTATE enmLinkState)
{
    pThis->enmLinkState = enmLinkState;

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
            AssertMsgFailed(("drvNATNotifyLinkChanged: unexpected link state %d\n", enmLinkState));
    }
}


/**
 * Notification on link status changes.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmLinkState    The new link state.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvNATNotifyLinkChanged(PPDMINETWORKCONNECTOR pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    PDRVNAT pThis = PDMINETWORKCONNECTOR_2_DRVNAT(pInterface);

    LogFlow(("drvNATNotifyLinkChanged: enmLinkState=%d\n", enmLinkState));

    PRTREQ pReq = NULL;

    /* don't queue new requests when the NAT thread is about to stop */
    if (pThis->pSlirpThread->enmState != PDMTHREADSTATE_RUNNING)
        return;

    int rc = RTReqAlloc(pThis->pSlirpReqQueue, &pReq, RTREQTYPE_INTERNAL);
    AssertReleaseRC(rc);
    pReq->u.Internal.pfn      = (PFNRT)drvNATNotifyLinkChangedWorker;
    pReq->u.Internal.cArgs    = 2;
    pReq->u.Internal.aArgs[0] = (uintptr_t)pThis;
    pReq->u.Internal.aArgs[1] = (uintptr_t)enmLinkState;
    pReq->fFlags              = RTREQFLAGS_VOID;
    rc = RTReqQueue(pReq, 0); /* don't wait, we have to wakeup the NAT thread fist */
    if (RT_LIKELY(rc == VERR_TIMEOUT))
    {
        drvNATNotifyNATThread(pThis);
        rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertReleaseRC(rc);
    }
    else
        AssertReleaseRC(rc);
    RTReqFree(pReq);
}

/**
 * NAT thread handling the slirp stuff. The slirp implementation is single-threaded
 * so we execute this enginre in a dedicated thread. We take care that this thread
 * does not become the bottleneck: If the guest wants to send, a request is enqueued
 * into the pSlirpReqQueue and handled asynchronously by this thread. If this thread
 * wants to deliver packets to the guest, it enqueues a request into pRecvReqQueue
 * which is later handled by the Recv thread.
 */
static DECLCALLBACK(int) drvNATAsyncIoThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    int     nFDs = -1;
    unsigned int ms;
#ifdef RT_OS_WINDOWS
    DWORD   event;
    HANDLE  *phEvents;
    unsigned int cBreak = 0;
#else /* RT_OS_WINDOWS */
    struct pollfd *polls = NULL;
    unsigned int cPollNegRet = 0;
#endif /* !RT_OS_WINDOWS */

    LogFlow(("drvNATAsyncIoThread: pThis=%p\n", pThis));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

#ifdef RT_OS_WINDOWS
    phEvents = slirp_get_events(pThis->pNATState);
#endif /* RT_OS_WINDOWS */

    /*
     * Polling loop.
     */
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        nFDs = -1;
        /*
         * To prevent concurent execution of sending/receving threads
         */
#ifndef RT_OS_WINDOWS
        nFDs = slirp_get_nsock(pThis->pNATState);
        polls = NULL;
        /* allocation for all sockets + Management pipe */
        polls = (struct pollfd *)RTMemAlloc((1 + nFDs) * sizeof(struct pollfd) + sizeof(uint32_t));
        if (polls == NULL)
            return VERR_NO_MEMORY;

        /* don't pass the managemant pipe */
        slirp_select_fill(pThis->pNATState, &nFDs, &polls[1]);
        ms = slirp_get_timeout_ms(pThis->pNATState);

        polls[0].fd = pThis->PipeRead;
        /* POLLRDBAND usually doesn't used on Linux but seems used on Solaris */
        polls[0].events = POLLRDNORM|POLLPRI|POLLRDBAND;
        polls[0].revents = 0;

        int cChangedFDs = poll(polls, nFDs + 1, ms ? ms : -1);
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
                /* drain the pipe */
                char ch[1];
                size_t cbRead;
                int counter = 0;
                /*
                 * drvNATSend decoupled so we don't know how many times
                 * device's thread sends before we've entered multiplex,
                 * so to avoid false alarm drain pipe here to the very end
                 *
                 * @todo: Probably we should counter drvNATSend to count how
                 * deep pipe has been filed before drain.
                 *
                 * XXX:Make it reading exactly we need to drain the pipe.
                 */
                RTFileRead(pThis->PipeRead, &ch, 1, &cbRead);
            }
        }
        /* process _all_ outstanding requests but don't wait */
        RTReqProcess(pThis->pSlirpReqQueue, 0);
        RTMemFree(polls);
#else /* RT_OS_WINDOWS */
        slirp_select_fill(pThis->pNATState, &nFDs);
        ms = slirp_get_timeout_ms(pThis->pNATState);
        struct timeval tv = { 0, ms*1000 };
        event = WSAWaitForMultipleEvents(nFDs, phEvents, FALSE, ms ? ms : WSA_INFINITE, FALSE);
        if (   (event < WSA_WAIT_EVENT_0 || event > WSA_WAIT_EVENT_0 + nFDs - 1)
            && event != WSA_WAIT_TIMEOUT)
        {
            int error = WSAGetLastError();
            LogRel(("NAT: WSAWaitForMultipleEvents returned %d (error %d)\n", event, error));
            RTAssertReleasePanic();
        }

        if (event == WSA_WAIT_TIMEOUT)
        {
            /* only check for slow/fast timers */
            slirp_select_poll(pThis->pNATState, /* fTimeout=*/true, /*fIcmp=*/false);
            continue;
        }
        /* poll the sockets in any case */
        Log2(("%s: poll\n", __FUNCTION__));
        slirp_select_poll(pThis->pNATState, /* fTimeout=*/false, /* fIcmp=*/(event == WSA_WAIT_EVENT_0));
        /* process _all_ outstanding requests but don't wait */
        RTReqProcess(pThis->pSlirpReqQueue, 0);
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

    drvNATNotifyNATThread(pThis);
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

/**
 * Function called by slirp to feed incoming data to the network port.
 */
void slirp_output(void *pvUser, void *pvArg, const uint8_t *pu8Buf, int cb)
{
    PDRVNAT pThis = (PDRVNAT)pvUser;
    Assert(pThis);

    LogFlow(("slirp_output BEGIN %x %d\n", pu8Buf, cb));
    Log2(("slirp_output: pu8Buf=%p cb=%#x (pThis=%p)\n%.*Rhxd\n", pu8Buf, cb, pThis, cb, pu8Buf));

    PRTREQ pReq = NULL;

    /* don't queue new requests when the NAT thread is about to stop */
    if (pThis->pSlirpThread->enmState != PDMTHREADSTATE_RUNNING)
        return;

    int rc = RTReqAlloc(pThis->pRecvReqQueue, &pReq, RTREQTYPE_INTERNAL);
    AssertReleaseRC(rc);
    pReq->u.Internal.pfn      = (PFNRT)drvNATRecvWorker;
    pReq->u.Internal.cArgs    = 3;
    pReq->u.Internal.aArgs[0] = (uintptr_t)pThis;
    pReq->u.Internal.aArgs[1] = (uintptr_t)pu8Buf;
    pReq->u.Internal.aArgs[2] = (uintptr_t)cb;
    pReq->fFlags              = RTREQFLAGS_VOID|RTREQFLAGS_NO_WAIT;
    rc = RTReqQueue(pReq, 0);
    AssertReleaseRC(rc);
    drvNATRecvWakeup(pThis->pDrvIns, pThis->pRecvThread);
    STAM_COUNTER_INC(&pThis->StatQueuePktSent);
}


/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) drvNATQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_NETWORK_CONNECTOR:
            return &pThis->INetworkConnector;
        default:
            return NULL;
    }
}


/**
 * Get the MAC address into the slirp stack.
 *
 * Called by drvNATLoadDone and drvNATPowerOn.
 */
static void drvNATSetMac(PDRVNAT pThis)
{
    if (pThis->pConfig)
    {
        RTMAC Mac;
        pThis->pConfig->pfnGetMac(pThis->pConfig, &Mac);
        slirp_set_ethaddr(pThis->pNATState, Mac.au8);
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
 * Sets up the redirectors.
 *
 * @returns VBox status code.
 * @param   pCfgHandle      The drivers configuration handle.
 */
static int drvNATConstructRedir(unsigned iInstance, PDRVNAT pThis, PCFGMNODE pCfgHandle, RTIPV4ADDR Network)
{
     RTMAC Mac;
     memset(&Mac, 0, sizeof(RTMAC)); /*can't get MAC here */
    /*
     * Enumerate redirections.
     */
    for (PCFGMNODE pNode = CFGMR3GetFirstChild(pCfgHandle); pNode; pNode = CFGMR3GetNextChild(pNode))
    {
        /*
         * Validate the port forwarding config.
         */
        if (!CFGMR3AreValuesValid(pNode, "Protocol\0UDP\0HostPort\0GuestPort\0GuestIP\0BindIP\0"))
            return PDMDRV_SET_ERROR(pThis->pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES, N_("Unknown configuration in port forwarding"));

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

        /*
         * Call slirp about it.
         */
        struct in_addr BindIP;
        GETIP_DEF(rc, pThis, pNode, BindIP, INADDR_ANY);
        if (slirp_redir(pThis->pNATState, fUDP, BindIP, iHostPort, GuestIP, iGuestPort, Mac.au8) < 0)
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

    slirp_term(pThis->pNATState);
    slirp_deregister_statistics(pThis->pNATState, pDrvIns);
    pThis->pNATState = NULL;
#ifdef VBOX_WITH_STATISTICS
# define DRV_PROFILE_COUNTER(name, dsc)     DEREGISTER_COUNTER(name, pThis)
# define DRV_COUNTING_COUNTER(name, dsc)    DEREGISTER_COUNTER(name, pThis)
# include "counters.h"
#endif
}


/**
 * Construct a NAT network transport driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvNATConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    LogFlow(("drvNATConstruct:\n"));

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle,
                              "PassDomain\0TFTPPrefix\0BootFile\0Network"
                              "\0NextServer\0DNSProxy\0BindIP\0"
                              "SocketRcvBuf\0SocketSndBuf\0TcpRcvSpace\0TcpSndSpace\0"))
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
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvNATQueryInterface;
    /* INetwork */
    pThis->INetworkConnector.pfnSend               = drvNATSend;
    pThis->INetworkConnector.pfnSetPromiscuousMode = drvNATSetPromiscuousMode;
    pThis->INetworkConnector.pfnNotifyLinkChanged  = drvNATNotifyLinkChanged;

    /*
     * Get the configuration settings.
     */
    int rc;
    bool fPassDomain = true;
    GET_BOOL(rc, pThis, pCfgHandle, "PassDomain", fPassDomain);

    GET_STRING_ALLOC(rc, pThis, pCfgHandle, "TFTPPrefix", pThis->pszTFTPPrefix);
    GET_STRING_ALLOC(rc, pThis, pCfgHandle, "BootFile", pThis->pszBootFile);
    GET_STRING_ALLOC(rc, pThis, pCfgHandle, "NextServer", pThis->pszNextServer);

    int fDNSProxy = 0;
    GET_S32(rc, pThis, pCfgHandle, "DNSProxy", fDNSProxy);

    /*
     * Query the network port interface.
     */
    pThis->pPort =
                (PPDMINETWORKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase,
                                                                      PDMINTERFACE_NETWORK_PORT);
    if (!pThis->pPort)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: the above device/driver didn't "
                                "export the network port interface"));
    pThis->pConfig =
                (PPDMINETWORKCONFIG)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase,
                                                                        PDMINTERFACE_NETWORK_CONFIG);
    if (!pThis->pConfig)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: the above device/driver didn't "
                                "export the network config interface"));

    /* Generate a network address for this network card. */
    char szNetwork[32]; /* xxx.xxx.xxx.xxx/yy */
    GET_STRING(rc, pThis, pCfgHandle, "Network", szNetwork[0], sizeof(szNetwork));
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        RTStrPrintf(szNetwork, sizeof(szNetwork), "10.0.%d.0/24", pDrvIns->iInstance + 2);

    RTIPV4ADDR Network;
    RTIPV4ADDR Netmask;
    rc = RTCidrStrToIPv4(szNetwork, &Network, &Netmask);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: Configuration error: "
                                   "network '%s' describes not a valid IPv4 network"),
                                   pDrvIns->iInstance, szNetwork);

    char szNetAddr[16];
    RTStrPrintf(szNetAddr, sizeof(szNetAddr), "%d.%d.%d.%d",
               (Network & 0xFF000000) >> 24, (Network & 0xFF0000) >> 16,
               (Network & 0xFF00) >> 8, Network & 0xFF);

    /*
     * Initialize slirp.
     */
    rc = slirp_init(&pThis->pNATState, &szNetAddr[0], Netmask, fPassDomain, pThis);
    if (RT_SUCCESS(rc))
    {
        slirp_set_dhcp_TFTP_prefix(pThis->pNATState, pThis->pszTFTPPrefix);
        slirp_set_dhcp_TFTP_bootfile(pThis->pNATState, pThis->pszBootFile);
        slirp_set_dhcp_next_server(pThis->pNATState, pThis->pszNextServer);
        slirp_set_dhcp_dns_proxy(pThis->pNATState, !!fDNSProxy);
        char *pszBindIP = NULL;
        GET_STRING_ALLOC(rc, pThis, pCfgHandle, "BindIP", pszBindIP);
        rc = slirp_set_binding_address(pThis->pNATState, pszBindIP);
        if (rc != 0)
            LogRel(("NAT: value of BindIP has been ignored\n"));

        if(pszBindIP != NULL)
            MMR3HeapFree(pszBindIP);
#define SLIRP_SET_TUNING_VALUE(name, setter)                    \
            do                                                  \
            {                                                   \
                int len = 0;                                    \
                rc = CFGMR3QueryS32(pCfgHandle, name, &len);    \
                if (RT_SUCCESS(rc))                             \
                    setter(pThis->pNATState, len);              \
            } while(0)

        SLIRP_SET_TUNING_VALUE("SocketRcvBuf", slirp_set_rcvbuf);
        SLIRP_SET_TUNING_VALUE("SocketSndBuf", slirp_set_sndbuf);
        SLIRP_SET_TUNING_VALUE("TcpRcvSpace", slirp_set_tcp_rcvspace);
        SLIRP_SET_TUNING_VALUE("TcpSndSpace", slirp_set_tcp_sndspace);

        slirp_register_statistics(pThis->pNATState, pDrvIns);
#ifdef VBOX_WITH_STATISTICS
# define DRV_PROFILE_COUNTER(name, dsc)     REGISTER_COUNTER(name, pThis, STAMTYPE_PROFILE, STAMUNIT_TICKS_PER_CALL, dsc)
# define DRV_COUNTING_COUNTER(name, dsc)    REGISTER_COUNTER(name, pThis, STAMTYPE_COUNTER, STAMUNIT_COUNT,          dsc)
# include "counters.h"
#endif

        int rc2 = drvNATConstructRedir(pDrvIns->iInstance, pThis, pCfgHandle, Network);
        if (RT_SUCCESS(rc2))
        {
            /*
             * Register a load done notification to get the MAC address into the slirp
             * engine after we loaded a guest state.
             */
            rc2 = PDMDrvHlpSSMRegisterLoadDone(pDrvIns, drvNATLoadDone);
            AssertRC(rc2);
            rc = RTReqCreateQueue(&pThis->pSlirpReqQueue);
            if (RT_FAILURE(rc))
            {
                LogRel(("NAT: Can't create request queue\n"));
                return rc;
            }


            rc = RTReqCreateQueue(&pThis->pRecvReqQueue);
            if (RT_FAILURE(rc))
            {
                LogRel(("NAT: Can't create request queue\n"));
                return rc;
            }
            rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pThis->pRecvThread, pThis, drvNATRecv,
                                          drvNATRecvWakeup, 128 * _1K, RTTHREADTYPE_IO, "NATRX");
            AssertReleaseRC(rc);
            rc = RTSemEventCreate(&pThis->EventRecv);

#ifndef RT_OS_WINDOWS
            /*
             * Create the control pipe.
             */
            int fds[2];
            if (pipe(&fds[0]) != 0) /** @todo RTPipeCreate() or something... */
            {
                int rc = RTErrConvertFromErrno(errno);
                AssertRC(rc);
                return rc;
            }
            pThis->PipeRead = fds[0];
            pThis->PipeWrite = fds[1];
#else
            pThis->hWakeupEvent = CreateEvent(NULL, FALSE, FALSE, NULL); /* auto-reset event */
            slirp_register_external_event(pThis->pNATState, pThis->hWakeupEvent,
                                          VBOX_WAKEUP_EVENT_INDEX);
#endif

            rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pThis->pSlirpThread, pThis, drvNATAsyncIoThread,
                                          drvNATAsyncIoWakeup, 128 * _1K, RTTHREADTYPE_IO, "NAT");
            AssertReleaseRC(rc);

#ifdef VBOX_WITH_SLIRP_MT
            rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pThis->pGuestThread, pThis, drvNATAsyncIoGuest,
                                          drvNATAsyncIoGuestWakeup, 128 * _1K, RTTHREADTYPE_IO, "NATGUEST");
            AssertReleaseRC(rc);
#endif

            pThis->enmLinkState = PDMNETWORKLINKSTATE_UP;

            /* might return VINF_NAT_DNS */
            return rc;
        }
        /* failure path */
        rc = rc2;
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
    /* szDriverName */
    "NAT",
    /* pszDescription */
    "NAT Network Transport Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    16,
    /* cbInstance */
    sizeof(DRVNAT),
    /* pfnConstruct */
    drvNATConstruct,
    /* pfnDestruct */
    drvNATDestruct,
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

