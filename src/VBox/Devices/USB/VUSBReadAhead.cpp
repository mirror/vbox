/* $Id$ */
/** @file
 * Virtual USB - Read-ahead buffering for periodic endpoints.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_VUSB
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/critsect.h>
#include "VUSBInternal.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * VUSB Readahead instance data.
 */
typedef struct VUSBREADAHEADINT
{
    /** Pointer to the device which the thread is for. */
    PVUSBDEV            pDev;
    /** Pointer to the pipe which the thread is servicing. */
    PVUSBPIPE           pPipe;
    /** A flag indicating a high-speed (vs. low/full-speed) endpoint. */
    bool                fHighSpeed;
    /** A flag telling the thread to terminate. */
    volatile bool       fTerminate;
    /** Maximum number of URBs to submit. */
    uint32_t            cUrbsMax;
    /** The periodic read-ahead buffer thread. */
    RTTHREAD            hReadAheadThread;
    /** Pointer to the first buffered URB. */
    PVUSBURB            pBuffUrbHead;
    /** Pointer to the last buffered URB. */
    PVUSBURB            pBuffUrbTail;
    /** Count of URBs in read-ahead buffer. */
    uint32_t            cBuffered;
    /** Count of URBs submitted for read-ahead but not yet reaped. */
    uint32_t            cSubmitted;
    /** Critical section to serialize access the buffered URB list. */
    RTCRITSECT          CritSectBuffUrbList;
} VUSBREADAHEADINT, *PVUSBREADAHEADINT;


/*******************************************************************************
*   Implementation                                                             *
*******************************************************************************/

static PVUSBURB vusbDevNewIsocUrb(PVUSBDEV pDev, unsigned uEndPt, unsigned uInterval, unsigned uPktSize)
{
    PVUSBURB    pUrb;
    unsigned    cPackets = 0;
    uint32_t    cbTotal = 0;
    unsigned    uNextIndex = 0;

    Assert(pDev);
    Assert(uEndPt);
    Assert(uInterval);
    Assert(uPktSize);

    /* Calculate the amount of data needed, taking the endpoint's bInterval into account */
    for (unsigned i = 0; i < 8; ++i)
    {
        if (i == uNextIndex)
        {
            cbTotal += uPktSize;
            cPackets++;
            uNextIndex += uInterval;
        }
    }
    Assert(cbTotal <= 24576);

    // @todo: What do we do if cPackets is 0?

    /*
     * Allocate and initialize the URB.
     */
    Assert(pDev->u8Address != VUSB_INVALID_ADDRESS);

    PVUSBROOTHUB pRh = vusbDevGetRh(pDev);
    if (!pRh)
        /* can happen during disconnect */
        return NULL;

    pUrb = vusbRhNewUrb(pRh, pDev->u8Address, cbTotal, 1);
    if (!pUrb)
        /* not much we can do here... */
        return NULL;

    pUrb->enmType               = VUSBXFERTYPE_ISOC;
    pUrb->EndPt                 = uEndPt;
    pUrb->enmDir                = VUSBDIRECTION_IN;
    pUrb->fShortNotOk           = false;
    pUrb->enmStatus             = VUSBSTATUS_OK;
    pUrb->Hci.EdAddr            = 0;
    pUrb->Hci.fUnlinked         = false;
// @todo: fill in the rest? The Hci member is not relevant
#ifdef LOG_ENABLED
    static unsigned s_iSerial = 0;
    s_iSerial = (s_iSerial + 1) % 10000;
    RTStrAPrintf(&pUrb->pszDesc, "URB %p prab<%04d", pUrb, s_iSerial);  // prab = Periodic Read-Ahead Buffer
#endif

    /* Set up the individual packets, again with bInterval in mind */
    pUrb->cIsocPkts = 8;
    unsigned off = 0;
    uNextIndex = 0;
    for (unsigned i = 0; i < 8; i++)
    {
        pUrb->aIsocPkts[i].enmStatus = VUSBSTATUS_NOT_ACCESSED;
        pUrb->aIsocPkts[i].off = off;
        if (i == uNextIndex)    // skip unused packets
        {
            pUrb->aIsocPkts[i].cb = uPktSize;
            off += uPktSize;
            uNextIndex += uInterval;
        }
        else
            pUrb->aIsocPkts[i].cb = 0;
    }
    Assert(off == cbTotal);
    return pUrb;
}

/**
 * Thread function for performing read-ahead buffering of periodic input.
 *
 * This thread keeps a buffer (queue) filled with data read from a periodic
 * input endpoint.
 *
 * The problem: In the EHCI emulation, there is a very short period between the
 * time when the guest can schedule a request and the time when it expects the results.
 * This leads to many dropped URBs because by the time we get the data from the host,
 * the guest already gave up and moved on.
 *
 * The solution: For periodic transfers, we know the worst-case bandwidth. We can
 * read ahead and buffer a few milliseconds worth of data. That way data is available
 * by the time the guest asks for it and we can deliver it immediately.
 *
 * @returns success indicator.
 * @param   Thread      This thread.
 * @param   pvUser      Pointer to a VUSBREADAHEADARGS structure.
 */
static DECLCALLBACK(int) vusbDevReadAheadThread(RTTHREAD Thread, void *pvUser)
{
    PVUSBREADAHEADINT       pThis = (PVUSBREADAHEADINT)pvUser;
    PVUSBPIPE               pPipe;
    PCVUSBDESCENDPOINT      pDesc;
    PVUSBURB                pUrb;
    int                     rc = VINF_SUCCESS;
    unsigned                max_pkt_size, mult, interval;

    LogFlow(("vusb: periodic read-ahead buffer thread started\n"));
    Assert(pThis);
    Assert(pThis->pPipe && pThis->pDev);

    pPipe = pThis->pPipe;
    pDesc = &pPipe->in->Core;
    Assert(pDesc);

    Assert(!pThis->cSubmitted && !pThis->cBuffered);

    /* Figure out the maximum bandwidth we might need */
    if (pThis->fHighSpeed)
    {
        /* High-speed endpoint */
        Assert((pDesc->wMaxPacketSize & 0x1fff) == pDesc->wMaxPacketSize);
        Assert(pDesc->bInterval <= 16);
        interval     = pDesc->bInterval ? 1 << (pDesc->bInterval - 1) : 1;
        max_pkt_size = pDesc->wMaxPacketSize & 0x7ff;
        mult         = ((pDesc->wMaxPacketSize & 0x1800) >> 11) + 1;
    }
    else
    {
        /* Full- or low-speed endpoint */
        Assert((pDesc->wMaxPacketSize & 0x7ff) == pDesc->wMaxPacketSize);
        interval     = pDesc->bInterval;
        max_pkt_size = pDesc->wMaxPacketSize;
        mult         = 1;
    }
    Log(("vusb: interval=%u, max pkt size=%u, multiplier=%u\n", interval, max_pkt_size, mult));

    /*
     * Submit new URBs in a loop unless the buffer is too full (paused VM etc.). Note that we only
     * queue the URBs here, they are reaped on a different thread.
     */
    while (!pThis->fTerminate)
    {
        while (pThis->cSubmitted < pThis->cUrbsMax && pThis->cBuffered < pThis->cUrbsMax)
        {
            pUrb = vusbDevNewIsocUrb(pThis->pDev, pDesc->bEndpointAddress & 0xF, interval, max_pkt_size * mult);
            if (!pUrb) {
                /* Happens if device was unplugged. */
                Log(("vusb: read-ahead thread failed to allocate new URB; exiting\n"));
                vusbReadAheadStop(pThis);
                break;
            }

            Assert(pUrb->enmState == VUSBURBSTATE_ALLOCATED);

            // @todo: at the moment we abuse the Hci.pNext member (which is otherwise entirely unused!)
            pUrb->Hci.pNext = (PVUSBURB)pvUser;

            pUrb->enmState = VUSBURBSTATE_IN_FLIGHT;
            rc = vusbUrbQueueAsyncRh(pUrb);
            if (RT_FAILURE(rc))
            {
                /* Happens if device was unplugged. */
                Log(("vusb: read-ahead thread failed to queue URB with %Rrc; exiting\n", rc));
                pThis->cUrbsMax = pThis->cSubmitted;
                pUrb->VUsb.pfnFree(pUrb);
                break;
            }
            else
                ASMAtomicIncU32(&pThis->cSubmitted);
        }
        RTThreadSleep(1);
    }
    LogFlow(("vusb: periodic read-ahead buffer thread exiting\n"));

    /* wait until there are no more submitted packets */
    while (pThis->cSubmitted > 0)
    {
        Log2(("vusbDevReadAheadThread: still %u packets submitted, waiting before terminating...\n", pThis->cSubmitted));
        RTThreadSleep(1);
    }

    RTCritSectEnter(&pThis->CritSectBuffUrbList);
    /*
     * Free all still buffered URBs because another endpoint with a different packet size
     * and complete different data formats might be served later.
     */
    while (pThis->pBuffUrbHead)
    {
        PVUSBURB pBufferedUrb = pThis->pBuffUrbHead;

        pThis->pBuffUrbHead = pBufferedUrb->Hci.pNext;
        pBufferedUrb->VUsb.pfnFree(pBufferedUrb);
    }

    RTCritSectLeave(&pThis->CritSectBuffUrbList);
    RTCritSectDelete(&pThis->CritSectBuffUrbList);
    RTMemTmpFree(pThis);

    return rc;
}

/**
 * Completes a read-ahead URB. This function does *not* free the URB but puts
 * it on a queue instead. The URB is only freed when the guest asks for the data
 * (by reading on the buffered pipe) or when the pipe/device is torn down.
 */
void vusbUrbCompletionReadAhead(PVUSBURB pUrb)
{
    Assert(pUrb);
    Assert(pUrb->Hci.pNext);
    PVUSBREADAHEADINT pThis = (PVUSBREADAHEADINT)pUrb->Hci.pNext;
    PVUSBPIPE         pPipe = pThis->pPipe;
    Assert(pPipe);

    RTCritSectEnter(&pThis->CritSectBuffUrbList);
    pUrb->Hci.pNext = NULL; // @todo: use a more suitable field
    if (pThis->pBuffUrbHead == NULL)
    {
        // The queue is empty, this is easy
        Assert(!pThis->pBuffUrbTail);
        pThis->pBuffUrbTail = pThis->pBuffUrbHead = pUrb;
    }
    else
    {
        // Some URBs are queued already
        Assert(pThis->pBuffUrbTail);
        Assert(!pThis->pBuffUrbTail->Hci.pNext);
        pThis->pBuffUrbTail = pThis->pBuffUrbTail->Hci.pNext = pUrb;
    }
    ASMAtomicDecU32(&pThis->cSubmitted);
    ++pThis->cBuffered;
    RTCritSectLeave(&pThis->CritSectBuffUrbList);
}

/**
 * Process a submit of an input URB on a pipe with read-ahead buffering. Instead
 * of passing the URB to the proxy, we use previously read data stored in the
 * read-ahead buffer, immediately complete the input URB and free the buffered URB.
 *
 * @param pUrb      The URB submitted by HC
 * @param hReadAhead The read-ahead buffering instance
 *
 * @return int      Status code
 */
int vusbUrbSubmitBufferedRead(PVUSBURB pUrb, VUSBREADAHEAD hReadAhead)
{
    PVUSBREADAHEADINT pThis = hReadAhead;
    PVUSBURB pBufferedUrb;
    Assert(pUrb && pThis);

    RTCritSectEnter(&pThis->CritSectBuffUrbList);
    pBufferedUrb = pThis->pBuffUrbHead;
    if (pBufferedUrb)
    {
        unsigned    cbTotal;

        // There's a URB available in the read-ahead buffer; use it
        pThis->pBuffUrbHead = pBufferedUrb->Hci.pNext;
        if (pThis->pBuffUrbHead == NULL)
            pThis->pBuffUrbTail = NULL;

        --pThis->cBuffered;
        RTCritSectLeave(&pThis->CritSectBuffUrbList);

        // Make sure the buffered URB is what we expect
        Assert(pUrb->enmType == pBufferedUrb->enmType);
        Assert(pUrb->EndPt == pBufferedUrb->EndPt);
        Assert(pUrb->enmDir == pBufferedUrb->enmDir);

        pUrb->enmState  = VUSBURBSTATE_REAPED;
        pUrb->enmStatus = pBufferedUrb->enmStatus;
        cbTotal = 0;
        // Copy status and data received from the device
        for (unsigned i = 0; i < pUrb->cIsocPkts; ++i)
        {
            unsigned    off, len;

            off = pBufferedUrb->aIsocPkts[i].off;
            len = pBufferedUrb->aIsocPkts[i].cb;
            pUrb->aIsocPkts[i].cb = len;
            pUrb->aIsocPkts[i].off = off;
            pUrb->aIsocPkts[i].enmStatus = pBufferedUrb->aIsocPkts[i].enmStatus;
            cbTotal += len;
            Assert(pUrb->VUsb.cbDataAllocated >= cbTotal);
            memcpy(&pUrb->abData[off], &pBufferedUrb->abData[off], len);
        }
        // Give back the data to the HC right away and then free the buffered URB
        vusbUrbCompletionRh(pUrb);
        // This assertion is wrong as the URB could be re-allocated in the meantime by the EMT (race)
        // Assert(pUrb->enmState == VUSBURBSTATE_FREE);
        Assert(pBufferedUrb->enmState == VUSBURBSTATE_REAPED);
        LogFlow(("%s: vusbUrbSubmitBufferedRead: Freeing buffered URB\n", pBufferedUrb->pszDesc));
        pBufferedUrb->VUsb.pfnFree(pBufferedUrb);
        // This assertion is wrong as the URB could be re-allocated in the meantime by the EMT (race)
        // Assert(pBufferedUrb->enmState == VUSBURBSTATE_FREE);
    }
    else
    {
        RTCritSectLeave(&pThis->CritSectBuffUrbList);
        // No URB on hand. Either we exhausted the buffer (shouldn't happen!) or the guest simply
        // asked for data too soon. Pretend that the device didn't deliver any data.
        pUrb->enmState  = VUSBURBSTATE_REAPED;
        pUrb->enmStatus = VUSBSTATUS_DATA_UNDERRUN;
        for (unsigned i = 0; i < pUrb->cIsocPkts; ++i)
        {
            pUrb->aIsocPkts[i].cb = 0;
            pUrb->aIsocPkts[i].enmStatus = VUSBSTATUS_NOT_ACCESSED;
        }
        vusbUrbCompletionRh(pUrb);
        // This assertion is wrong as the URB could be re-allocated in the meantime by the EMT (race)
        // Assert(pUrb->enmState == VUSBURBSTATE_FREE);
        LogFlow(("vusbUrbSubmitBufferedRead: No buffered URB available!\n"));
    }
    return VINF_SUCCESS;
}

/* Read-ahead start/stop functions, used primarily to keep the PVUSBREADAHEADARGS struct private to this module. */

VUSBREADAHEAD vusbReadAheadStart(PVUSBDEV pDev, PVUSBPIPE pPipe)
{
    int rc;
    PVUSBREADAHEADINT pThis = (PVUSBREADAHEADINT)RTMemTmpAlloc(sizeof(VUSBREADAHEADINT));

    if (pThis)
    {
        PVUSBROOTHUB pRh = vusbDevGetRh(pDev);
        pThis->pDev         = pDev;
        pThis->pPipe        = pPipe;
        pThis->fTerminate   = false;
        pThis->fHighSpeed   = pRh && ((pRh->fHcVersions & VUSB_STDVER_20) != 0);
        pThis->cUrbsMax     = 120;
        pThis->pBuffUrbHead = NULL;
        pThis->pBuffUrbTail = NULL;
        pThis->cBuffered    = 0;
        pThis->cSubmitted   = 0;
        rc = RTCritSectInit(&pThis->CritSectBuffUrbList);
        if (RT_SUCCESS(rc))
        {
            if (pThis->fHighSpeed)
                rc = RTThreadCreate(&pThis->hReadAheadThread, vusbDevReadAheadThread, pThis, 0, RTTHREADTYPE_IO, 0 /* fFlags */, "USBISOC");
            else
                rc = VERR_VUSB_DEVICE_NOT_ATTACHED; // No buffering for low/full-speed devices at the moment, needs testing.
            if (RT_SUCCESS(rc))
            {
                Log(("vusb: created isochronous read-ahead thread\n"));
                return pThis;
            }
            else
                Log(("vusb: isochronous read-ahead thread creation failed, rc=%d\n", rc));

            rc = RTCritSectDelete(&pThis->CritSectBuffUrbList);
            AssertRC(rc);
        }

        RTMemTmpFree(pThis);
    }

    /* If thread creation failed for any reason, simply fall back to standard processing. */
    return NULL;
}

void vusbReadAheadStop(VUSBREADAHEAD hReadAhead)
{
    PVUSBREADAHEADINT pThis = hReadAhead;
    Log(("vusb: terminating read-ahead thread for endpoint\n"));
    ASMAtomicXchgBool(&pThis->fTerminate, true);
}

/*
 * Local Variables:
 *  mode: c
 *  c-file-style: "bsd"
 *  c-basic-offset: 4
 *  tab-width: 4
 *  indent-tabs-mode: s
 * End:
 */

