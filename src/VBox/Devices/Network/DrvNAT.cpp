/** @file
 *
 * VBox network devices:
 * NAT network transport driver
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include "Network/slirp/libslirp.h"
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/critsect.h>
#include <iprt/cidr.h>
#include <iprt/stream.h>

#include "Builtins.h"

#ifdef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
# ifndef RT_OS_WINDOWS
#  include <unistd.h>
#  include <fcntl.h>
#  include <poll.h>
# endif
# include <errno.h>
# include <iprt/semaphore.h>
# include <iprt/req.h>
#endif

/**
 * @todo: This is a bad hack to prevent freezing the guest during high network
 *        activity. This needs to be fixed properly.
 */
#define VBOX_NAT_DELAY_HACK


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
#ifndef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
    /** Slirp critical section. */
    RTCRITSECT              CritSect;
#endif
    /** Link state */
    PDMNETWORKLINKSTATE     enmLinkState;
    /** NAT state for this instance. */
    PNATState               pNATState;
    /** TFTP directory prefix. */
    char                    *pszTFTPPrefix;
    /** Boot file name to provide in the DHCP server response. */
    char                    *pszBootFile;
    /** tftp server name to provide in the DHCP server response. */
    char                    *pszNextServer;
#ifdef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
    /* polling thread */
    PPDMTHREAD              pThread;
    /** Queue for NAT-thread-external events. */
    PRTREQQUEUE             pReqQueue;
    /* Send queue */
    PPDMQUEUE               pSendQueue;
# ifdef VBOX_WITH_SLIRP_MT
    PPDMTHREAD              pGuestThread;
# endif
# ifndef RT_OS_WINDOWS
    /** The write end of the control pipe. */
    RTFILE                  PipeWrite;
    /** The read end of the control pipe. */
    RTFILE                  PipeRead;
# else
    /** for external notification */
    HANDLE                  hWakeupEvent;
# endif
#endif
} DRVNAT, *PDRVNAT;

typedef struct DRVNATQUEUITEM
{
    /** The core part owned by the queue manager. */
    PDMQUEUEITEMCORE    Core;
    /** The buffer for output to guest. */
    const uint8_t       *pu8Buf;
    /* size of buffer */
    size_t              cb;
    void                *mbuf;
} DRVNATQUEUITEM, *PDRVNATQUEUITEM;

/** Converts a pointer to NAT::INetworkConnector to a PRDVNAT. */
#define PDMINETWORKCONNECTOR_2_DRVNAT(pInterface)   ( (PDRVNAT)((uintptr_t)pInterface - RT_OFFSETOF(DRVNAT, INetworkConnector)) )


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
 * Send data to the network.
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

#ifdef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC

    PRTREQ pReq = NULL;
    int rc;
    void *buf;
    /* don't queue new requests when the NAT thread is about to stop */
    if (pThis->pThread->enmState != PDMTHREADSTATE_RUNNING)
        return VINF_SUCCESS;
# ifndef VBOX_WITH_SLIRP_MT
    rc = RTReqAlloc(pThis->pReqQueue, &pReq, RTREQTYPE_INTERNAL);
# else
    rc = RTReqAlloc((PRTREQQUEUE)slirp_get_queue(pThis->pNATState), &pReq, RTREQTYPE_INTERNAL);
# endif
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
# ifndef RT_OS_WINDOWS
    /* kick select() */
    rc = RTFileWrite(pThis->PipeWrite, "", 1, NULL);
    AssertRC(rc);
# else
    /* kick WSAWaitForMultipleEvents */
    rc = WSASetEvent(pThis->hWakeupEvent);
    AssertRelease(rc == TRUE);
# endif

#else /* !VBOX_WITH_SIMPLIFIED_SLIRP_SYNC */

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertReleaseRC(rc);

    drvNATSendWorker(pThis, pvBuf, cb);

    RTCritSectLeave(&pThis->CritSect);

#endif /* !VBOX_WITH_SIMPLIFIED_SLIRP_SYNC */

    LogFlow(("drvNATSend: end\n"));
    return VINF_SUCCESS;
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

#ifdef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC

    PRTREQ pReq = NULL;
    /* don't queue new requests when the NAT thread is about to stop */
    if (pThis->pThread->enmState != PDMTHREADSTATE_RUNNING)
        return;
    int rc = RTReqAlloc(pThis->pReqQueue, &pReq, RTREQTYPE_INTERNAL);
    AssertReleaseRC(rc);
    pReq->u.Internal.pfn      = (PFNRT)drvNATNotifyLinkChangedWorker;
    pReq->u.Internal.cArgs    = 2;
    pReq->u.Internal.aArgs[0] = (uintptr_t)pThis;
    pReq->u.Internal.aArgs[1] = (uintptr_t)enmLinkState;
    pReq->fFlags              = RTREQFLAGS_VOID;
    rc = RTReqQueue(pReq, 0); /* don't wait, we have to wakeup the NAT thread fist */
    if (RT_LIKELY(rc == VERR_TIMEOUT))
    {
# ifndef RT_OS_WINDOWS
        /* kick select() */
        rc = RTFileWrite(pThis->PipeWrite, "", 1, NULL);
        AssertRC(rc);
# else
        /* kick WSAWaitForMultipleEvents() */
        rc = WSASetEvent(pThis->hWakeupEvent);
        AssertRelease(rc == TRUE);
# endif
        rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);
        AssertReleaseRC(rc);
    }
    else
        AssertReleaseRC(rc);
    RTReqFree(pReq);

#else /* !VBOX_WITH_SIMPLIFIED_SLIRP_SYNC */

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertReleaseRC(rc);
    drvNATNotifyLinkChangedWorker(pThis, enmLinkState);
    RTCritSectLeave(&pThis->CritSect);

#endif /* VBOX_WITH_SIMPLIFIED_SLIRP_SYNC */
}


#ifndef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC

/**
 * Poller callback.
 */
static DECLCALLBACK(void) drvNATPoller(PPDMDRVINS pDrvIns)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    fd_set  ReadFDs;
    fd_set  WriteFDs;
    fd_set  XcptFDs;
    int     nFDs = -1;
    FD_ZERO(&ReadFDs);
    FD_ZERO(&WriteFDs);
    FD_ZERO(&XcptFDs);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertReleaseRC(rc);

    slirp_select_fill(pThis->pNATState, &nFDs, &ReadFDs, &WriteFDs, &XcptFDs);

    struct timeval tv = {0, 0}; /* no wait */
    int cChangedFDs = select(nFDs + 1, &ReadFDs, &WriteFDs, &XcptFDs, &tv);
    if (cChangedFDs >= 0)
        slirp_select_poll(pThis->pNATState, &nFDs, &ReadFDs, &WriteFDs, &XcptFDs);

    RTCritSectLeave(&pThis->CritSect);
}

#else /* VBOX_WITH_SIMPLIFIED_SLIRP_SYNC */

static DECLCALLBACK(int) drvNATAsyncIoThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    int     nFDs = -1;
    unsigned int ms;
# ifdef RT_OS_WINDOWS
    DWORD   event;
    HANDLE  *phEvents;
    unsigned int cBreak = 0;
# else /* RT_OS_WINDOWS */
    struct pollfd *polls = NULL;
    unsigned int cPollNegRet;
# endif /* !RT_OS_WINDOWS */

    LogFlow(("drvNATAsyncIoThread: pThis=%p\n", pThis));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

# ifdef RT_OS_WINDOWS
    phEvents = slirp_get_events(pThis->pNATState);
# endif /* RT_OS_WINDOWS */

    /*
     * Polling loop.
     */
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        nFDs = -1;

        /*
         * To prevent concurent execution of sending/receving threads
         */
# ifndef RT_OS_WINDOWS
        nFDs = slirp_get_nsock(pThis->pNATState);
        polls = NULL;
        polls = (struct pollfd *)RTMemAlloc((1 + nFDs) * sizeof(struct pollfd) + sizeof(uint32_t)); /* allocation for all sockets + Management pipe*/
        if (polls == NULL)
            return VERR_NO_MEMORY;

        slirp_select_fill(pThis->pNATState, &nFDs, &polls[1]); /*don't bother Slirp with knowelege about managemant pipe*/
        ms = slirp_get_timeout_ms(pThis->pNATState);

        polls[0].fd = pThis->PipeRead;
        polls[0].events = POLLRDNORM|POLLPRI|POLLRDBAND; /* POLLRDBAND usually doesn't used on Linux but seems used on Solaris */
        polls[0].revents = 0;

        int cChangedFDs = poll(polls, nFDs + 1, ms ? ms : -1);
        /* 2.6.23 + gdb -> hitting all the time. probably a bug in poll/ptrace/whatever. */
        if (cChangedFDs < 0) 
        {
            if (cPollNegRet++ > 128)
            {
                cPollNegRet = 0;
                LogRel(("Poll returns (%s) suppressed %d\n", strerror(errno), cPollNegRet));
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
            /* process _all_ outstanding requests but don't wait */
            RTReqProcess(pThis->pReqQueue, 0);
        }
        RTMemFree(polls);
# else /* RT_OS_WINDOWS */
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
            Log2(("%s: timeout\n", __FUNCTION__));
            continue;
        }

        /* poll the sockets in any case */
        Log2(("%s: poll\n", __FUNCTION__));
        slirp_select_poll(pThis->pNATState, /* fTimeout=*/false, /* fIcmp=*/(event == WSA_WAIT_EVENT_0));
        /* process _all_ outstanding requests but don't wait */
        RTReqProcess(pThis->pReqQueue, 0);
#  ifdef VBOX_NAT_DELAY_HACK
        if (cBreak++ > 128)
        {
            cBreak = 0;
            RTThreadSleep(2);
        }
#  endif
# endif /* RT_OS_WINDOWS */
    }

    return VINF_SUCCESS;
}

 /**
 *  Unblock the send thread so it can respond to a state change.
 *
 *  @returns VBox status code.
 *  @param   pDevIns     The pcnet device instance.
 *  @param   pThread     The send thread.
 */
static DECLCALLBACK(int) drvNATAsyncIoWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

# ifndef RT_OS_WINDOWS
    /* kick select() */
    int rc = RTFileWrite(pThis->PipeWrite, "", 1, NULL);
    AssertRC(rc);
# else /* !RT_OS_WINDOWS */
    /* kick WSAWaitForMultipleEvents() */
    WSASetEvent(pThis->hWakeupEvent);
# endif /* RT_OS_WINDOWS */

    return VINF_SUCCESS;
}

# ifdef VBOX_WITH_SLIRP_MT
static DECLCALLBACK(int) drvNATAsyncIoGuest(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
	{
        slirp_process_queue(pThis->pNATState);
	}
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvNATAsyncIoGuestWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    return VINF_SUCCESS;
}
# endif /* VBOX_WITH_SLIRP_MT */

#endif /* VBOX_WITH_SIMPLIFIED_SLIRP_SYNC */


/**
 * Function called by slirp to check if it's possible to feed incoming data to the network port.
 * @returns 1 if possible.
 * @returns 0 if not possible.
 */
int slirp_can_output(void *pvUser)
{
    PDRVNAT pThis = (PDRVNAT)pvUser;

    Assert(pThis);

#ifndef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
    /** Happens during termination */
    if (!RTCritSectIsOwner(&pThis->CritSect))
        return 0;

    int rc =  pThis->pPort->pfnWaitReceiveAvail(pThis->pPort, 0);
    return RT_SUCCESS(rc);
#else
    return 1;
#endif
}


/**
 * Function called by slirp to feed incoming data to the network port.
 */
#ifdef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
void slirp_output(void *pvUser, void *pvArg, const uint8_t *pu8Buf, int cb)
#else
void slirp_output(void *pvUser, const uint8_t *pu8Buf, int cb)
#endif
{
    PDRVNAT pThis = (PDRVNAT)pvUser;

    LogFlow(("slirp_output BEGIN %x %d\n", pu8Buf, cb));
    Log2(("slirp_output: pu8Buf=%p cb=%#x (pThis=%p)\n%.*Rhxd\n", pu8Buf, cb, pThis, cb, pu8Buf));

    Assert(pThis);

#ifndef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
    /** Happens during termination */
    if (!RTCritSectIsOwner(&pThis->CritSect))
        return;

    int rc = pThis->pPort->pfnReceive(pThis->pPort, pu8Buf, cb);
    AssertRC(rc);
    LogFlow(("slirp_output END %x %d\n", pu8Buf, cb));
#else

    PDRVNATQUEUITEM pItem = (PDRVNATQUEUITEM)PDMQueueAlloc(pThis->pSendQueue);
    if (pItem)
    {
        pItem->pu8Buf = pu8Buf;
        pItem->cb = cb;
        pItem->mbuf = pvArg;
        Log2(("pItem:%p %.Rhxd\n", pItem, pItem->pu8Buf));
        PDMQueueInsert(pThis->pSendQueue, &pItem->Core);
        return;
    }
    static unsigned cDroppedPackets;
    if (cDroppedPackets < 64)
    {
        cDroppedPackets++;
    }
    else
    {
        LogRel(("NAT: %d messages suppressed about dropping package (couldn't allocate queue item)\n", cDroppedPackets));
        cDroppedPackets = 0;
    }
    RTMemFree((void *)pu8Buf);
#endif
}

#ifdef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
/**
 * Queue callback for processing a queued item.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pDrvIns         The driver instance.
 * @param   pItemCore       Pointer to the queue item to process.
 */
static DECLCALLBACK(bool) drvNATQueueConsumer(PPDMDRVINS pDrvIns, PPDMQUEUEITEMCORE pItemCore)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    PDRVNATQUEUITEM pItem = (PDRVNATQUEUITEM)pItemCore;
    PRTREQ pReq = NULL;
    Log(("drvNATQueueConsumer(pItem:%p, pu8Buf:%p, cb:%d)\n", pItem, pItem->pu8Buf, pItem->cb));
    Log2(("drvNATQueueConsumer: pu8Buf:\n%.Rhxd\n", pItem->pu8Buf));
    int rc = pThis->pPort->pfnWaitReceiveAvail(pThis->pPort, 0);
    if (RT_FAILURE(rc))
        return false;
    rc = pThis->pPort->pfnReceive(pThis->pPort, pItem->pu8Buf, pItem->cb);

#if 0
    rc = RTReqAlloc(pThis->pReqQueue, &pReq, RTREQTYPE_INTERNAL);
    AssertReleaseRC(rc);
    pReq->u.Internal.pfn      = (PFNRT)slirp_post_sent;
    pReq->u.Internal.cArgs    = 2;
    pReq->u.Internal.aArgs[0] = (uintptr_t)pThis->pNATState;
    pReq->u.Internal.aArgs[1] = (uintptr_t)pItem->mbuf;
    pReq->fFlags              = RTREQFLAGS_VOID;
    AssertRC(rc);
#else
    /*Copy buffer again, till seeking good way of syncronization with slirp mbuf management code*/
    AssertRelease(pItem->mbuf == NULL);
    RTMemFree((void *)pItem->pu8Buf);
#endif
    return RT_SUCCESS(rc);
}
#endif

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

#ifndef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertReleaseRC(rc);
#endif
    slirp_term(pThis->pNATState);
    pThis->pNATState = NULL;
#ifndef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
    RTCritSectLeave(&pThis->CritSect);
    RTCritSectDelete(&pThis->CritSect);
#endif
}


/**
 * Sets up the redirectors.
 *
 * @returns VBox status code.
 * @param   pCfgHandle      The drivers configuration handle.
 */
static int drvNATConstructRedir(unsigned iInstance, PDRVNAT pThis, PCFGMNODE pCfgHandle, RTIPV4ADDR Network)
{
    /*
     * Enumerate redirections.
     */
    for (PCFGMNODE pNode = CFGMR3GetFirstChild(pCfgHandle); pNode; pNode = CFGMR3GetNextChild(pNode))
    {
        /*
         * Validate the port forwarding config.
         */
        if (!CFGMR3AreValuesValid(pNode, "Protocol\0UDP\0HostPort\0GuestPort\0GuestIP\0"))
            return PDMDRV_SET_ERROR(pThis->pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES, N_("Unknown configuration in port forwarding"));

        /* protocol type */
        bool fUDP;
        char szProtocol[32];
        int rc = CFGMR3QueryString(pNode, "Protocol", &szProtocol[0], sizeof(szProtocol));
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        {
            rc = CFGMR3QueryBool(pNode, "UDP", &fUDP);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                fUDP = false;
            else if (RT_FAILURE(rc))
                return PDMDrvHlpVMSetError(pThis->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"UDP\" boolean failed"), iInstance);
        }
        else if (RT_SUCCESS(rc))
        {
            if (!RTStrICmp(szProtocol, "TCP"))
                fUDP = false;
            else if (!RTStrICmp(szProtocol, "UDP"))
                fUDP = true;
            else
                return PDMDrvHlpVMSetError(pThis->pDrvIns, VERR_INVALID_PARAMETER, RT_SRC_POS, N_("NAT#%d: Invalid configuration value for \"Protocol\": \"%s\""), iInstance, szProtocol);
        }
        else
            return PDMDrvHlpVMSetError(pThis->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"Protocol\" string failed"), iInstance);

        /* host port */
        int32_t iHostPort;
        rc = CFGMR3QueryS32(pNode, "HostPort", &iHostPort);
        if (RT_FAILURE(rc))
            return PDMDrvHlpVMSetError(pThis->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"HostPort\" integer failed"), iInstance);

        /* guest port */
        int32_t iGuestPort;
        rc = CFGMR3QueryS32(pNode, "GuestPort", &iGuestPort);
        if (RT_FAILURE(rc))
            return PDMDrvHlpVMSetError(pThis->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"GuestPort\" integer failed"), iInstance);

        /* guest address */
        char    szGuestIP[32];
        rc = CFGMR3QueryString(pNode, "GuestIP", &szGuestIP[0], sizeof(szGuestIP));
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
            RTStrPrintf(szGuestIP, sizeof(szGuestIP), "%d.%d.%d.%d",
                        (Network & 0xFF000000) >> 24, (Network & 0xFF0000) >> 16, (Network & 0xFF00) >> 8, (Network & 0xE0) | 15);
        else if (RT_FAILURE(rc))
            return PDMDrvHlpVMSetError(pThis->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"GuestIP\" string failed"), iInstance);
        struct in_addr GuestIP;
        if (!inet_aton(szGuestIP, &GuestIP))
            return PDMDrvHlpVMSetError(pThis->pDrvIns, VERR_NAT_REDIR_GUEST_IP, RT_SRC_POS,
                                       N_("NAT#%d: configuration error: invalid \"GuestIP\"=\"%s\", inet_aton failed"), iInstance, szGuestIP);

        /*
         * Call slirp about it.
         */
        Log(("drvNATConstruct: Redir %d -> %s:%d\n", iHostPort, szGuestIP, iGuestPort));
        if (slirp_redir(pThis->pNATState, fUDP, iHostPort, GuestIP, iGuestPort) < 0)
            return PDMDrvHlpVMSetError(pThis->pDrvIns, VERR_NAT_REDIR_SETUP, RT_SRC_POS,
                                       N_("NAT#%d: configuration error: failed to set up redirection of %d to %s:%d. Probably a conflict with existing services or other rules"), iInstance, iHostPort, szGuestIP, iGuestPort);
    } /* for each redir rule */

    return VINF_SUCCESS;
}

/**
 * Get the MAC address into the slirp stack.
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
 * Construct a NAT network transport driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvNATConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    char szNetAddr[16];
    char szNetwork[32]; /* xxx.xxx.xxx.xxx/yy */
    LogFlow(("drvNATConstruct:\n"));

    /*
     * Validate the config.
     */
#ifndef VBOX_WITH_SLIRP_DNS_PROXY
    if (!CFGMR3AreValuesValid(pCfgHandle, "PassDomain\0TFTPPrefix\0BootFile\0Network\0NextServer\0"))
#else
    if (!CFGMR3AreValuesValid(pCfgHandle, "PassDomain\0TFTPPrefix\0BootFile\0Network\0NextServer\0DNSProxy\0"))
#endif
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES, N_("Unknown NAT configuration option, only supports PassDomain, TFTPPrefix, BootFile and Network"));

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
    bool fPassDomain = true;
    int rc = CFGMR3QueryBool(pCfgHandle, "PassDomain", &fPassDomain);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fPassDomain = true;
    else if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"PassDomain\" boolean failed"), pDrvIns->iInstance);

    rc = CFGMR3QueryStringAlloc(pCfgHandle, "TFTPPrefix", &pThis->pszTFTPPrefix);
    if (RT_FAILURE(rc) && rc != VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"TFTPPrefix\" string failed"), pDrvIns->iInstance);
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "BootFile", &pThis->pszBootFile);
    if (RT_FAILURE(rc) && rc != VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"BootFile\" string failed"), pDrvIns->iInstance);
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "NextServer", &pThis->pszNextServer);
    if (RT_FAILURE(rc) && rc != VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"NextServer\" string failed"), pDrvIns->iInstance);
#ifdef VBOX_WITH_SLIRP_DNS_PROXY
    int fDNSProxy;
    rc = CFGMR3QueryS32(pCfgHandle, "DNSProxy", &fDNSProxy);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fDNSProxy = 0;
#endif

    /*
     * Query the network port interface.
     */
    pThis->pPort = (PPDMINETWORKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_PORT);
    if (!pThis->pPort)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: the above device/driver didn't export the network port interface"));
    pThis->pConfig = (PPDMINETWORKCONFIG)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_CONFIG);
    if (!pThis->pConfig)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: the above device/driver didn't export the network config interface"));

    /* Generate a network address for this network card. */
    rc = CFGMR3QueryString(pCfgHandle, "Network", szNetwork, sizeof(szNetwork));
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        RTStrPrintf(szNetwork, sizeof(szNetwork), "10.0.%d.0/24", pDrvIns->iInstance + 2);
    else if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"Network\" string failed"), pDrvIns->iInstance);

    RTIPV4ADDR Network;
    RTIPV4ADDR Netmask;
    rc = RTCidrStrToIPv4(szNetwork, &Network, &Netmask);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: Configuration error: network '%s' describes not a valid IPv4 network"), pDrvIns->iInstance, szNetwork);

    RTStrPrintf(szNetAddr, sizeof(szNetAddr), "%d.%d.%d.%d",
               (Network & 0xFF000000) >> 24, (Network & 0xFF0000) >> 16, (Network & 0xFF00) >> 8, Network & 0xFF);

    /*
     * The slirp lock..
     */
#ifndef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
    rc = RTCritSectInit(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;
#endif
    /*
     * Initialize slirp.
     */
    rc = slirp_init(&pThis->pNATState, &szNetAddr[0], Netmask, fPassDomain, pThis);
    if (RT_SUCCESS(rc))
    {
        slirp_set_dhcp_TFTP_prefix(pThis->pNATState, pThis->pszTFTPPrefix);
        slirp_set_dhcp_TFTP_bootfile(pThis->pNATState, pThis->pszBootFile);
        slirp_set_dhcp_next_server(pThis->pNATState, pThis->pszNextServer);
#ifdef VBOX_WITH_SLIRP_DNS_PROXY
        slirp_set_dhcp_dns_proxy(pThis->pNATState, fDNSProxy);
#endif

        slirp_register_timers(pThis->pNATState, pDrvIns);
        int rc2 = drvNATConstructRedir(pDrvIns->iInstance, pThis, pCfgHandle, Network);
        if (RT_SUCCESS(rc2))
        {
            /*
             * Register a load done notification to get the MAC address into the slirp
             * engine after we loaded a guest state.
             */
            rc2 = PDMDrvHlpSSMRegister(pDrvIns, pDrvIns->pDrvReg->szDriverName,
                                       pDrvIns->iInstance, 0, 0,
                                       NULL, NULL, NULL, NULL, NULL, drvNATLoadDone);
            AssertRC(rc2);
#ifndef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
            pDrvIns->pDrvHlp->pfnPDMPollerRegister(pDrvIns, drvNATPoller);
#else
            rc = RTReqCreateQueue(&pThis->pReqQueue);
            if (RT_FAILURE(rc))
            {
                LogRel(("NAT: Can't create request queue\n"));
                return rc;
            }

            rc = PDMDrvHlpPDMQueueCreate(pDrvIns, sizeof(DRVNATQUEUITEM), 50, 0, drvNATQueueConsumer, &pThis->pSendQueue);
            if (RT_FAILURE(rc))
            {
                LogRel(("NAT: Can't create send queue\n"));
                return rc;
            }

# ifndef RT_OS_WINDOWS
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
# else
            pThis->hWakeupEvent = CreateEvent(NULL, FALSE, FALSE, NULL); /* auto-reset event */
            slirp_register_external_event(pThis->pNATState, pThis->hWakeupEvent, VBOX_WAKEUP_EVENT_INDEX);
# endif

            rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pThis->pThread, pThis, drvNATAsyncIoThread, drvNATAsyncIoWakeup, 128 * _1K, RTTHREADTYPE_IO, "NAT");
            AssertReleaseRC(rc);

#ifdef VBOX_WITH_SLIRP_MT
            rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pThis->pGuestThread, pThis, drvNATAsyncIoGuest, drvNATAsyncIoGuestWakeup, 128 * _1K, RTTHREADTYPE_IO, "NATGUEST");
            AssertReleaseRC(rc);
#endif
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


#ifndef VBOX_WITH_SIMPLIFIED_SLIRP_SYNC
    RTCritSectDelete(&pThis->CritSect);
#endif
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
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL
};
