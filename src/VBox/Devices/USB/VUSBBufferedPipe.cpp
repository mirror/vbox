/* $Id$ */
/** @file
 * Virtual USB - Buffering for isochronous in/outpipes.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_VUSB
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/path.h>
#include <iprt/critsect.h>
#include <iprt/circbuf.h>
#include "VUSBInternal.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Copy of the isoc packet descriptors.
 */
typedef struct VUSBISOCDESC
{
    /** Total number of bytes described by the packets. */
    size_t              cbTotal;
    /** The number of isochronous packets described in aIsocPkts. */
    uint32_t            cIsocPkts;
    /** The iso packets. */
    VUSBURBISOCPKT      aIsocPkts[8];
} VUSBISOCDESC;
/** Pointer to a isoc packets descriptor. */
typedef VUSBISOCDESC *PVUSBISOCDESC;

/**
 * Buffered pipe state.
 */
typedef enum VUSBBUFFEREDPIPESTATE
{
    /** Invalid state. */
    VUSBBUFFEREDPIPESTATE_INVALID = 0,
    /** The buffer is created. */
    VUSBBUFFEREDPIPESTATE_CREATING,
    /** The buffer is destroyed. */
    VUSBBUFFEREDPIPESTATE_DESTROYING,
    /** The buffer is filling with data. */
    VUSBBUFFEREDPIPESTATE_FILLING,
    /** The buffer is streaming data to the guest/device based on the direction. */
    VUSBBUFFEREDPIPESTATE_STREAMING,
    /** 32bit hack. */
    VUSBBUFFEREDPIPESTATE_32BIT_HACK = 0x7fffffff
} VUSBBUFFEREDPIPESTATE;
/** Pointer to a buffered pipe state. */
typedef VUSBBUFFEREDPIPESTATE *PVUSBBUFFEREDPIPESTATE;

/**
 * VUSB buffered pipe instance data.
 */
typedef struct VUSBBUFFEREDPIPEINT
{
    /** Pointer to the device which the thread is for. */
    PVUSBDEV              pDev;
    /** Pointer to the pipe which the thread is servicing. */
    PVUSBPIPE             pPipe;
    /** USB speed to serve. */
    VUSBSPEED             enmSpeed;
    /** The buffered pipe state. */
    VUSBBUFFEREDPIPESTATE enmState;
    /** Maximum latency the buffer should cause. */
    uint32_t              cLatencyMs;
    /** Interval of the endpoint in frames (Low/Full-speed 1ms per frame, High-speed 125us). */
    unsigned              uInterval;
    /** Packet size. */
    size_t                cbPktSize;
    /** Endpoint. */
    unsigned              uEndPt;
    /** The direction of the buffering. */
    VUSBDIRECTION         enmDirection;
    /** Size of the ring buffer to keep all data for buffering. */
    size_t                cbRingBufData;
    /** The circular buffer keeping the data. */
    PRTCIRCBUF            pRingBufData;
    /** Number of URBs in flight on the device. */
    unsigned              cUrbsInFlight;
    /** Critical section protecting the ring buffer and. */
    RTCRITSECT            CritSectBuffer;
    /** Number of isoc descriptors for buffering. */
    unsigned              cIsocDesc;
    /** Current index into the isoc descriptor array for reading. */
    unsigned              idxIsocDescRead;
    /** Current index of the isoc descriptor for writing. */
    unsigned              idxIsocDescWrite;
    /** Array of isoc descriptors for pre buffering. */
    PVUSBISOCDESC         paIsocDesc;
    /** Our own URB pool. */
    VUSBURBPOOL           UrbPool;
#ifdef DEBUG
    /** Lock contention counter. */
    volatile uint32_t     cLockContention;
#endif
#ifdef LOG_ENABLED
    /** Serial number tag for logging. */
    uint32_t              iSerial;
#endif
} VUSBBUFFEREDPIPEINT, *PVUSBBUFFEREDPIPEINT;


/*********************************************************************************************************************************
*   Implementation                                                                                                               *
*********************************************************************************************************************************/

/**
 * Callback for freeing an URB.
 * @param   pUrb    The URB to free.
 */
static DECLCALLBACK(void) vusbBufferedPipeFreeUrb(PVUSBURB pUrb)
{
    /*
     * Assert sanity.
     */
    vusbUrbAssert(pUrb);
    PVUSBBUFFEREDPIPEINT pThis = (PVUSBBUFFEREDPIPEINT)pUrb->pVUsb->pvFreeCtx;
    AssertPtr(pThis);

    /*
     * Free the URB description (logging builds only).
     */
    if (pUrb->pszDesc)
    {
        RTStrFree(pUrb->pszDesc);
        pUrb->pszDesc = NULL;
    }

    vusbUrbPoolFree(&pThis->UrbPool, pUrb);
}


#ifdef DEBUG
/**
 * Locks the buffered pipe for exclusive access.
 *
 * @returns nothing.
 * @param   pThis    The buffered pipe instance.
 */
DECLINLINE(void) vusbBufferedPipeLock(PVUSBBUFFEREDPIPEINT pThis)
{
    int rc = RTCritSectTryEnter(&pThis->CritSectBuffer);
    if (rc == VERR_SEM_BUSY)
    {
        ASMAtomicIncU32(&pThis->cLockContention);
        RTCritSectEnter(&pThis->CritSectBuffer);
    }
}
#else
# define vusbBufferedPipeLock(a_pThis) RTCritSectEnter(&(a_pThis)->CritSectBuffer)
#endif


/**
 * Create a new isochronous URB.
 *
 * @returns Pointer to the new URB or NULL on failure.
 * @param   pThis     The buffered pipe instance.
 * @param   pIsocDesc The isochronous packet descriptor saved from the HC submitted URB.
 */
static PVUSBURB vusbBufferedPipeNewIsocUrb(PVUSBBUFFEREDPIPEINT pThis, PVUSBISOCDESC pIsocDesc)
{
    PVUSBURB pUrb;

    /*
     * Allocate and initialize the URB.
     */
    Assert(pThis->pDev->u8Address != VUSB_INVALID_ADDRESS);

    pUrb = vusbUrbPoolAlloc(&pThis->UrbPool, VUSBXFERTYPE_ISOC, pThis->enmDirection, pIsocDesc->cbTotal,
                            0, 0, 0);
    if (!pUrb)
        /* not much we can do here... */
        return NULL;

    pUrb->EndPt             = pThis->uEndPt;
    pUrb->fShortNotOk       = false;
    pUrb->enmStatus         = VUSBSTATUS_OK;
    pUrb->pVUsb->pvBuffered = pThis;
    pUrb->pVUsb->pvFreeCtx  = pThis;
    pUrb->pVUsb->pfnFree    = vusbBufferedPipeFreeUrb;
    pUrb->DstAddress        = pThis->pDev->u8Address;
    pUrb->pVUsb->pDev       = pThis->pDev;

#ifdef LOG_ENABLED
    pThis->iSerial = (pThis->iSerial + 1) % 10000;
    RTStrAPrintf(&pUrb->pszDesc, "URB %p isoc%c%04d (buffered)", pUrb,
                 (pUrb->enmDir == VUSBDIRECTION_IN) ? '<' : '>',
                 pThis->iSerial);
#endif

    /* Copy data over. */
    void *pv = NULL;
    size_t cb = 0;
    RTCircBufAcquireReadBlock(pThis->pRingBufData, pIsocDesc->cbTotal, &pv, &cb);
    memcpy(&pUrb->abData[0], pv, cb);
    RTCircBufReleaseReadBlock(pThis->pRingBufData, cb);
    /* Take possible wraparound in the ring buffer into account. */
    if (cb < pIsocDesc->cbTotal)
    {
        size_t cb2 = 0;
        RTCircBufAcquireReadBlock(pThis->pRingBufData, pIsocDesc->cbTotal - cb, &pv, &cb2);
        memcpy(&pUrb->abData[cb], pv, cb2);
        RTCircBufReleaseReadBlock(pThis->pRingBufData, cb2);
        Assert(pIsocDesc->cbTotal == cb + cb2);
    }

    /* Set up the individual packets. */
    pUrb->cIsocPkts = pIsocDesc->cIsocPkts;
    for (unsigned i = 0; i < pUrb->cIsocPkts; i++)
    {
        pUrb->aIsocPkts[i].enmStatus = VUSBSTATUS_NOT_ACCESSED;
        pUrb->aIsocPkts[i].off       = pIsocDesc->aIsocPkts[i].off;
        pUrb->aIsocPkts[i].cb        = pIsocDesc->aIsocPkts[i].cb;
    }

    return pUrb;
}


/**
 * Stream waiting data to the device.
 *
 * @returns VBox status code.
 * @param   pThis     The buffered pipe instance.
 */
static int vusbBufferedPipeStream(PVUSBBUFFEREDPIPEINT pThis)
{
    int rc = VINF_SUCCESS;

    while (   pThis->idxIsocDescRead != pThis->idxIsocDescWrite
           && RT_SUCCESS(rc))
    {
        PVUSBURB pUrb = vusbBufferedPipeNewIsocUrb(pThis, &pThis->paIsocDesc[pThis->idxIsocDescRead]);
        if (pUrb)
        {
            pUrb->enmState = VUSBURBSTATE_IN_FLIGHT;
            rc = vusbUrbQueueAsyncRh(pUrb);
            if (RT_SUCCESS(rc))
                pThis->cUrbsInFlight++;
            else
                pUrb->pVUsb->pfnFree(pUrb);
        }
        else
            rc = VERR_NO_MEMORY;

        pThis->idxIsocDescRead = (pThis->idxIsocDescRead + 1) % pThis->cIsocDesc;
    }

    return rc;
}


/**
 * Set parameters for the buffered pipe like packet size from the given endpoint.
 *
 * @returns VBox status code.
 * @param   pThis     The buffered pipe instance.
 * @param   pDesc     The endpoint descriptor to take the data from.
 */
static int vusbBufferedPipeSetParamsFromDescriptor(PVUSBBUFFEREDPIPEINT pThis, PCVUSBDESCENDPOINT pDesc)
{
    unsigned cbPktMax;
    unsigned uInterval;
    unsigned uMult;

    if (pThis->enmSpeed == VUSB_SPEED_HIGH)
    {
        /* High-speed endpoint */
        Assert((pDesc->wMaxPacketSize & 0x1fff) == pDesc->wMaxPacketSize);
        Assert(pDesc->bInterval <= 16);
        uInterval = pDesc->bInterval ? 1 << (pDesc->bInterval - 1) : 1;
        cbPktMax  = pDesc->wMaxPacketSize & 0x7ff;
        uMult     = ((pDesc->wMaxPacketSize & 0x1800) >> 11) + 1;
    }
    else if (pThis->enmSpeed == VUSB_SPEED_FULL || pThis->enmSpeed == VUSB_SPEED_LOW)
    {
        /* Full- or low-speed endpoint */
        Assert((pDesc->wMaxPacketSize & 0x7ff) == pDesc->wMaxPacketSize);
        uInterval = pDesc->bInterval;
        cbPktMax  = pDesc->wMaxPacketSize;
        uMult     = 1;
    }
    else
    {
        /** @todo Implement for super speed and up if it turns out to be required, at the moment it looks
         * like we don't need it. */
        return VERR_NOT_SUPPORTED;
    }

    pThis->uInterval = uInterval;
    pThis->cbPktSize = cbPktMax * uMult;
    pThis->uEndPt    = pDesc->bEndpointAddress & 0xf;

    unsigned cPackets = pThis->cLatencyMs / pThis->uInterval;
    cPackets = RT_MAX(cPackets, 1); /* At least one packet. */
    pThis->cbRingBufData = pThis->cbPktSize * cPackets;
    pThis->cIsocDesc     = cPackets / 8 + ((cPackets % 8) ? 1 : 0);

    return VINF_SUCCESS;
}


/**
 * Completes an URB issued by the pipe buffer.
 *
 * @returns nothing.
 * @param   pUrb    The completed URB.
 */
DECLHIDDEN(void) vusbBufferedPipeCompleteUrb(PVUSBURB pUrb)
{
    Assert(pUrb);
    Assert(pUrb->pVUsb->pvBuffered);
    PVUSBBUFFEREDPIPEINT pThis = (PVUSBBUFFEREDPIPEINT)pUrb->pVUsb->pvBuffered;

    vusbBufferedPipeLock(pThis);

#if defined(LOG_ENABLED) || defined(RT_STRICT)
    unsigned cbXfer = 0;
    for (unsigned i = 0; i < pUrb->cIsocPkts; i++)
    {
        LogFlowFunc(("packet %u: cb=%u enmStatus=%u\n", i, pUrb->aIsocPkts[i].cb, pUrb->aIsocPkts[i].enmStatus));
        cbXfer += pUrb->aIsocPkts[i].cb;
    }
    Assert(cbXfer == pUrb->cbData);
#endif
    pUrb->pVUsb->pfnFree(pUrb);
    pThis->cUrbsInFlight--;

    /* Stream more data if available.*/
    if (pThis->enmState == VUSBBUFFEREDPIPESTATE_STREAMING)
        vusbBufferedPipeStream(pThis);
    RTCritSectLeave(&pThis->CritSectBuffer);
}


/**
 * Submit and process the given URB, for outgoing endpoints we will buffer the content
 * until we reached a threshold and start sending the data to the device.
 * For incoming endpoints prefetched data is used to complete the URB immediately.
 *
 * @returns VBox status code.
 * @param   hBuffer   The buffered pipe instance.
 * @param   pUrb      The URB submitted by HC
 */
DECLHIDDEN(int) vusbBufferedPipeSubmitUrb(VUSBBUFFEREDPIPE hBuffer, PVUSBURB pUrb)
{
    int rc = VINF_SUCCESS;
    PVUSBBUFFEREDPIPEINT pThis = hBuffer;

    AssertReturn(pThis->enmDirection == pUrb->enmDir, VERR_INTERNAL_ERROR);
    AssertReturn(pUrb->enmType == VUSBXFERTYPE_ISOC, VERR_INTERNAL_ERROR);

    vusbBufferedPipeLock(pThis);

    if (pThis->enmDirection == VUSBDIRECTION_OUT)
    {
        void *pv = NULL;
        size_t cb = 0;

        /* Copy the data of the URB into our internal ring buffer. */
        RTCircBufAcquireWriteBlock(pThis->pRingBufData, pUrb->cbData, &pv, &cb);
        memcpy(pv, &pUrb->abData[0], cb);
        RTCircBufReleaseWriteBlock(pThis->pRingBufData, cb);
        /* Take possible wraparound in the ring buffer into account. */
        if (cb < pUrb->cbData)
        {
            size_t cb2 = 0;
            RTCircBufAcquireWriteBlock(pThis->pRingBufData, pUrb->cbData - cb, &pv, &cb2);
            memcpy(pv, &pUrb->abData[cb], cb2);
            RTCircBufReleaseWriteBlock(pThis->pRingBufData, cb2);
            Assert(pUrb->cbData == cb + cb2);
        }

        /*
         * Copy the isoc packet descriptors over stuffing as much as possible into one.
         * We recombine URBs into one if possible maximizing the number of frames
         * one URB covers when we send it to the device.
         */
        unsigned idxIsocPkt = 0;
        for (unsigned iTry = 0; iTry < 2; iTry++)
        {
            PVUSBISOCDESC pIsocDesc = &pThis->paIsocDesc[pThis->idxIsocDescWrite];
            for (unsigned i = idxIsocPkt; i < pUrb->cIsocPkts && pIsocDesc->cIsocPkts < RT_ELEMENTS(pIsocDesc->aIsocPkts); i++)
            {
                pIsocDesc->aIsocPkts[pIsocDesc->cIsocPkts].enmStatus = VUSBSTATUS_NOT_ACCESSED;
                pIsocDesc->aIsocPkts[pIsocDesc->cIsocPkts].off = (uint16_t)pIsocDesc->cbTotal;
                Assert(pIsocDesc->aIsocPkts[pIsocDesc->cIsocPkts].off == pIsocDesc->cbTotal);
                pIsocDesc->aIsocPkts[pIsocDesc->cIsocPkts].cb  = pUrb->aIsocPkts[i].cb;
                pIsocDesc->cbTotal += pUrb->aIsocPkts[i].cb;
                pIsocDesc->cIsocPkts++;
                idxIsocPkt++;
                pUrb->aIsocPkts[i].enmStatus = VUSBSTATUS_OK;
            }

            if (pIsocDesc->cIsocPkts == RT_ELEMENTS(pIsocDesc->aIsocPkts))
            {
                /* Advance to the next isoc descriptor. */
                pThis->idxIsocDescWrite = (pThis->idxIsocDescWrite + 1) % pThis->cIsocDesc;
                pThis->paIsocDesc[pThis->idxIsocDescWrite].cbTotal   = 0;
                pThis->paIsocDesc[pThis->idxIsocDescWrite].cIsocPkts = 0;
                /* On the first wraparound start streaming because our buffer is full. */
                if (   pThis->enmState == VUSBBUFFEREDPIPESTATE_FILLING
                    && pThis->idxIsocDescWrite == 0)
                    pThis->enmState = VUSBBUFFEREDPIPESTATE_STREAMING;
            }
        }

        if (pThis->enmState == VUSBBUFFEREDPIPESTATE_STREAMING)
        {
            /* Stream anything we have. */
            rc = vusbBufferedPipeStream(pThis);
        }

        /* Complete the URB submitted by the HC. */
        pUrb->enmState = VUSBURBSTATE_REAPED;
        pUrb->enmStatus = VUSBSTATUS_OK;
        vusbUrbCompletionRh(pUrb);
    }
    else
    {
        AssertMsgFailed(("TODO"));
    }
    RTCritSectLeave(&pThis->CritSectBuffer);
    return rc;
}


DECLHIDDEN(int) vusbBufferedPipeCreate(PVUSBDEV pDev, PVUSBPIPE pPipe, VUSBDIRECTION enmDirection,
                                       VUSBSPEED enmSpeed, uint32_t cLatencyMs,
                                       PVUSBBUFFEREDPIPE phBuffer)
{
    int rc;
    PVUSBBUFFEREDPIPEINT pThis = (PVUSBBUFFEREDPIPEINT)RTMemAllocZ(sizeof(VUSBBUFFEREDPIPEINT));

    AssertReturn(enmDirection == VUSBDIRECTION_IN || enmDirection == VUSBDIRECTION_OUT,
                 VERR_INVALID_PARAMETER);
    AssertPtrReturn(pDev, VERR_INVALID_POINTER);
    AssertPtrReturn(pPipe, VERR_INVALID_POINTER);
    AssertPtrReturn(phBuffer, VERR_INVALID_POINTER);

    if (!cLatencyMs)
    {
        *phBuffer = NULL;
        return VINF_SUCCESS;
    }

    if (pThis)
    {
        PCVUSBDESCENDPOINT pDesc;

        pThis->pDev             = pDev;
        pThis->pPipe            = pPipe;
        pThis->enmSpeed         = enmSpeed;
        pThis->cLatencyMs       = cLatencyMs;
        pThis->enmDirection     = enmDirection;
        pThis->enmState         = VUSBBUFFEREDPIPESTATE_CREATING;
        pThis->cUrbsInFlight    = 0;
        pThis->idxIsocDescRead  = 0;
        pThis->idxIsocDescWrite = 0;
#ifdef DEBUG
        pThis->cLockContention  = 0;
#endif
#ifdef LOG_ENABLED
        pThis->iSerial          = 0;
#endif

        if (enmDirection == VUSBDIRECTION_IN)
            pDesc = &pPipe->in->Core;
        else
            pDesc = &pPipe->out->Core;
        Assert(pDesc);

        rc = vusbBufferedPipeSetParamsFromDescriptor(pThis, pDesc);
        if (RT_SUCCESS(rc))
        {
            pThis->paIsocDesc = (PVUSBISOCDESC)RTMemAllocZ(pThis->cIsocDesc * sizeof(VUSBISOCDESC));
            if (RT_LIKELY(pThis->paIsocDesc))
            {
                rc = vusbUrbPoolInit(&pThis->UrbPool);
                if (RT_SUCCESS(rc))
                {
                    rc = RTCritSectInit(&pThis->CritSectBuffer);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Create a ring buffer which can hold twice the amount of data
                         * for the required latency so we can fill the buffer with new data
                         * while the old one is still being used
                         */
                        rc = RTCircBufCreate(&pThis->pRingBufData, 2 * pThis->cbRingBufData);
                        if (RT_SUCCESS(rc))
                        {
                            /*
                             * For an input pipe start filling the buffer for an output endpoint
                             * we have to wait until the buffer is filled by the guest before
                             * starting to stream it to the device.
                             */
                            if (enmDirection == VUSBDIRECTION_IN)
                            {
                                /** @todo */
                            }
                            pThis->enmState = VUSBBUFFEREDPIPESTATE_FILLING;
                            *phBuffer = pThis;
                            return VINF_SUCCESS;
                        }

                        RTCritSectDelete(&pThis->CritSectBuffer);
                    }

                    RTMemFree(pThis->paIsocDesc);
                }
            }
            else
                rc = VERR_NO_MEMORY;
        }

        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


DECLHIDDEN(void) vusbBufferedPipeDestroy(VUSBBUFFEREDPIPE hBuffer)
{
    PVUSBBUFFEREDPIPEINT pThis = hBuffer;

    pThis->enmState = VUSBBUFFEREDPIPESTATE_DESTROYING;

    /* Cancel all outstanding URBs. */
    vusbDevCancelAllUrbs(pThis->pDev, false /* fDetaching */);

    vusbBufferedPipeLock(pThis);
    pThis->cUrbsInFlight = 0;

    /* Stream the last data. */
    vusbBufferedPipeStream(pThis);

    /* Wait for any in flight URBs to complete. */
    while (pThis->cUrbsInFlight)
    {
        RTCritSectLeave(&pThis->CritSectBuffer);
        RTThreadSleep(1);
        RTCritSectEnter(&pThis->CritSectBuffer);
    }

    RTCircBufDestroy(pThis->pRingBufData);
    vusbUrbPoolDestroy(&pThis->UrbPool);
    RTCritSectLeave(&pThis->CritSectBuffer);
#ifdef DEBUG
    Log(("VUSB: Destroyed buffered pipe with lock contention counter %u\n", pThis->cLockContention));
#endif
    RTMemFree(pThis->paIsocDesc);
    RTMemFree(pThis);
}

