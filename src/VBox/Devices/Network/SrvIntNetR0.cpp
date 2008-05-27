/* $Id$ */
/** @file
 * Internal networking - The ring 0 service.
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
#define LOG_GROUP LOG_GROUP_SRV_INTNET
#include <VBox/intnet.h>
#include <VBox/sup.h>
#include <VBox/pdm.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/alloc.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/thread.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * A network interface.
 */
typedef struct INTNETIF
{
    /** Pointer to the next interface. */
    struct INTNETIF        *pNext;
    /** The current MAC address for the interface. */
    PDMMAC                  Mac;
    /** Set if the INTNET::Mac member is valid. */
    bool                    fMacSet;
    /** Set if the interface is in promiscuous mode.
     * In promiscuous mode the interface will receive all packages except the one it's sending. */
    bool                    fPromiscuous;
    /** Number of yields done to try make the interface read pending data.
     * We will stop yeilding when this reaches a threshold assuming that the VM is paused or
     * that it simply isn't worth all the delay. It is cleared when a successful send has been done.
     */
    uint32_t                cYields;
    /** Pointer to the current exchange buffer (ring-0). */
    PINTNETBUF              pIntBuf;
    /** Pointer to ring-3 mapping of the current exchange buffer. */
    R3PTRTYPE(PINTNETBUF)   pIntBufR3;
    /** Pointer to the default exchange buffer for the interface. */
    PINTNETBUF              pIntBufDefault;
    /** Pointer to ring-3 mapping of the default exchange buffer. */
    R3PTRTYPE(PINTNETBUF)   pIntBufDefaultR3;
    /** Event semaphore which a receiver thread will sleep on while waiting for data to arrive. */
    RTSEMEVENT              Event;
    /** Number of threads sleeping on the Event semaphore. */
    uint32_t                cSleepers;
    /** The interface handle.
     * When this is INTNET_HANDLE_INVALID a sleeper which is waking up
     * should return with the appropriate error condition. */
    INTNETIFHANDLE          hIf;
    /** Pointer to the network this interface is connected to. */
    struct INTNETNETWORK   *pNetwork;
    /** The session this interface is associated with. */
    PSUPDRVSESSION          pSession;
    /** The SUPR0 object id. */
    void                   *pvObj;
} INTNETIF, *PINTNETIF;


/**
 * Internal representation of a network.
 */
typedef struct INTNETNETWORK
{
    /** The Next network in the chain.
     * This is protected by the INTNET::Spinlock. */
    struct INTNETNETWORK   *pNext;
    /** The network mutex.
     * It protects everything dealing with this network. */
    RTSEMFASTMUTEX          FastMutex;
    /** List of interfaces attached to the network. */
    PINTNETIF               pIFs;
    /** Pointer to the instance data. */
    struct INTNET          *pIntNet;
    /** The SUPR0 object id. */
    void                   *pvObj;
    /**  Access restricted? */
    bool                    fRestrictAccess;
    /** The length of the network name. */
    uint8_t                 cchName;
    /** The network name. */
    char                    szName[INTNET_MAX_NETWORK_NAME];
} INTNETNETWORK, *PINTNETNETWORK;


/**
 * Handle table entry.
 */
typedef union INTNETHTE
{
    /** Pointer to the object we're a handle for. */
    PINTNETIF       pIF;
    /** Index to the next free entry. */
    uintptr_t       iNext;
} INTNETHTE, *PINTNETHTE;


/**
 * Handle table.
 */
typedef struct INTNETHT
{
    /** Pointer to the handle table. */
    PINTNETHTE          paEntries;
    /** The number of allocated handles. */
    uint32_t            cAllocated;
    /** The index of the first free handle entry.
     * ~0U means empty list. */
    uint32_t volatile   iHead;
    /** The index of the last free handle entry.
     * ~0U means empty list. */
    uint32_t volatile   iTail;
} INTNETHT, *PINTNETHT;


/**
 * Internal networking instance.
 */
typedef struct INTNET
{
    /** Mutex protecting the network creation. */
    RTSEMFASTMUTEX          FastMutex;
    /** Spinlock protecting the linked list of networks and the interface handle translation table. */
    RTSPINLOCK              Spinlock;
    /** List of networks. Protected by INTNET::Spinlock. */
    PINTNETNETWORK volatile pNetworks;
    /** Handle table for the interfaces. */
    INTNETHT                IfHandles;
} INTNET;




/**
 * Validates and translates an interface handle to a interface pointer.
 *
 * @returns Pointer to interface.
 * @returns NULL if the handle is invalid.
 * @param   pIntNet     Pointer to the instance data.
 * @param   hIF         The interface handle to validate and translate.
 */
DECLINLINE(PINTNETIF) INTNETHandle2IFPtr(PINTNET pIntNet, INTNETIFHANDLE hIF)
{
    Assert(pIntNet);
    if ((hIF & INTNET_HANDLE_MAGIC) != INTNET_HANDLE_MAGIC)
        return NULL;

    PINTNETHT       pHT = &pIntNet->IfHandles;
    const uint32_t  i   = hIF & INTNET_HANDLE_INDEX_MASK;
    PINTNETIF       pIF = NULL;
    RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquire(pIntNet->Spinlock, &Tmp);

    if (    i < pHT->cAllocated
        &&  pHT->paEntries[i].iNext >= INTNET_HANDLE_MAX
        &&  pHT->paEntries[i].iNext != ~0U)
        pIF = pHT->paEntries[i].pIF;

    RTSpinlockRelease(pIntNet->Spinlock, &Tmp);

    return pIF;
}


/**
 * Allocates a handle for an interface.
 *
 * @returns Handle on success.
 * @returns Invalid handle on failure.
 * @param   pIntNet     Pointer to the instance data.
 * @param   pIF         The interface which we're allocating a handle for.
 */
static INTNETIFHANDLE INTNETHandleAllocate(PINTNET pIntNet, PINTNETIF pIF)
{
    Assert(pIF);
    Assert(pIntNet);
    unsigned    cTries = 10;
    PINTNETHT   pHT = &pIntNet->IfHandles;
    PINTNETHTE  paNew = NULL;

    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquire(pIntNet->Spinlock, &Tmp);
    for (;;)
    {
        /*
         * Check the free list.
         */
        uint32_t i = pHT->iHead;
        if (i != ~0U)
        {
            pHT->iHead = pHT->paEntries[i].iNext;
            if (pHT->iHead == ~0U)
                pHT->iTail = ~0U;

            pHT->paEntries[i].pIF = pIF;
            RTSpinlockRelease(pIntNet->Spinlock, &Tmp);
            if (paNew)
                RTMemFree(paNew);
            return i | INTNET_HANDLE_MAGIC;
        }

        /*
         * Leave the spinlock and allocate a new array.
         */
        const unsigned cNew = pHT->cAllocated + 128;
        RTSpinlockRelease(pIntNet->Spinlock, &Tmp);
        if (--cTries <= 0)
        {
            AssertMsgFailed(("Giving up!\n"));
            break;
        }
        paNew = (PINTNETHTE)RTMemAlloc(sizeof(*paNew) * cNew);
        if (!paNew)
            break;

        /*
         * Acquire the spinlock and check if someone raced us.
         */
        RTSpinlockAcquire(pIntNet->Spinlock, &Tmp);
        if (pHT->cAllocated < cNew)
        {
            /* copy the current table. */
            memcpy(paNew, pHT->paEntries, pHT->cAllocated * sizeof(*paNew));

            /* link the new entries into the free chain. */
            i = pHT->cAllocated;
            uint32_t iTail = pHT->iTail;
            if (iTail == ~0U)
                pHT->iHead = iTail = i++;
            while (i < cNew)
            {
                paNew[iTail].iNext = i;
                iTail = i++;
            }
            paNew[iTail].iNext = ~0U;
            pHT->iTail = iTail;

            /* update the handle table. */
            pHT->cAllocated = cNew;
            paNew = (PINTNETHTE)ASMAtomicXchgPtr((void * volatile *)&pHT->paEntries, paNew);
        }
    }

    if (paNew)
        RTMemFree(paNew);
    return INTNET_HANDLE_INVALID;
}


/**
 * Frees a handle.
 *
 * @returns Handle on success.
 * @returns Invalid handle on failure.
 * @param   pIntNet     Pointer to the instance data.
 * @param   h           The handle we're freeing.
 */
static void INTNETHandleFree(PINTNET pIntNet, INTNETIFHANDLE h)
{
    Assert(INTNETHandle2IFPtr(pIntNet, h));
    PINTNETHT       pHT = &pIntNet->IfHandles;
    const uint32_t  i   = h & INTNET_HANDLE_INDEX_MASK;

    RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquire(pIntNet->Spinlock, &Tmp);

    if (i < pHT->cAllocated)
    {
        /*
         * Insert at the end of the free list.
         */
        pHT->paEntries[i].iNext = ~0U;
        const uint32_t iTail = pHT->iTail;
        if (iTail != ~0U)
            pHT->paEntries[iTail].iNext = i;
        else
            pHT->iHead = i;
        pHT->iTail = i;
    }
    else
        AssertMsgFailed(("%d >= %d\n", i, pHT->cAllocated));

    RTSpinlockRelease(pIntNet->Spinlock, &Tmp);
}


#ifdef IN_INTNET_TESTCASE
/**
 * Reads the next frame in the buffer.
 * The caller is responsible for ensuring that there is a valid frame in the buffer.
 *
 * @returns Size of the frame in bytes.
 * @param   pBuf        The buffer.
 * @param   pRingBuff   The ring buffer to read from.
 * @param   pvFrame     Where to put the frame. The caller is responsible for
 *                      ensuring that there is sufficient space for the frame.
 */
static unsigned INTNETRingReadFrame(PINTNETBUF pBuf, PINTNETRINGBUF pRingBuf, void *pvFrame)
{
    Assert(pRingBuf->offRead < pBuf->cbBuf);
    Assert(pRingBuf->offRead >= pRingBuf->offStart);
    Assert(pRingBuf->offRead < pRingBuf->offEnd);
    uint32_t    offRead   = pRingBuf->offRead;
    PINTNETHDR  pHdr      = (PINTNETHDR)((uint8_t *)pBuf + offRead);
    const void *pvFrameIn = INTNETHdrGetFramePtr(pHdr, pBuf);
    unsigned    cb        = pHdr->cbFrame;
    memcpy(pvFrame, pvFrameIn, cb);

    /* skip the frame */
    offRead += pHdr->offFrame + cb;
    offRead = RT_ALIGN_32(offRead, sizeof(INTNETHDR));
    Assert(offRead <= pRingBuf->offEnd && offRead >= pRingBuf->offStart);
    if (offRead >= pRingBuf->offEnd)
        offRead = pRingBuf->offStart;
    ASMAtomicXchgU32(&pRingBuf->offRead, offRead);
    return cb;
}
#endif


/**
 * Writes a frame packet to the buffer.
 *
 * @returns VBox status code.
 * @param   pBuf        The buffer.
 * @param   pRingBuf    The ring buffer to read from.
 * @param   pvFrame     The frame to write.
 * @param   cbFrame     The size of the frame.
 */
static int INTNETRingWriteFrame(PINTNETBUF pBuf, PINTNETRINGBUF pRingBuf, const void *pvFrame, uint32_t cbFrame)
{
    /*
     * Validate input.
     */
    Assert(pBuf);
    Assert(pRingBuf);
    Assert(pvFrame);
    Assert(cbFrame >= sizeof(PDMMAC) * 2);
    uint32_t offWrite = pRingBuf->offWrite;
    Assert(offWrite == RT_ALIGN_32(offWrite, sizeof(INTNETHDR)));
    uint32_t offRead = pRingBuf->offRead;
    Assert(offRead == RT_ALIGN_32(offRead, sizeof(INTNETHDR)));

    const uint32_t cb = RT_ALIGN_32(cbFrame, sizeof(INTNETHDR));
    if (offRead <= offWrite)
    {
        /*
         * Try fit it all before the end of the buffer.
         */
        if (pRingBuf->offEnd - offWrite >= cb + sizeof(INTNETHDR))
        {
            PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = cbFrame;
            pHdr->offFrame = sizeof(INTNETHDR);

            memcpy(pHdr + 1, pvFrame, cbFrame);

            offWrite += cb + sizeof(INTNETHDR);
            Assert(offWrite <= pRingBuf->offEnd && offWrite >= pRingBuf->offStart);
            if (offWrite >= pRingBuf->offEnd)
                offWrite = pRingBuf->offStart;
            Log2(("WriteFrame: offWrite: %#x -> %#x (1)\n", pRingBuf->offWrite, offWrite));
            ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
            return VINF_SUCCESS;
        }

        /*
         * Try fit the frame at the start of the buffer.
         * (The header fits before the end of the buffer because of alignment.)
         */
        AssertMsg(pRingBuf->offEnd - offWrite >= sizeof(INTNETHDR), ("offEnd=%x offWrite=%x\n", pRingBuf->offEnd, offWrite));
        if (offRead - pRingBuf->offStart > cb) /* not >= ! */
        {
            PINTNETHDR  pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
            void       *pvFrameOut = (PINTNETHDR)((uint8_t *)pBuf + pRingBuf->offStart);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = cbFrame;
            pHdr->offFrame = (intptr_t)pvFrameOut - (intptr_t)pHdr;

            memcpy(pvFrameOut, pvFrame, cbFrame);

            offWrite = pRingBuf->offStart + cb;
            ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
            Log2(("WriteFrame: offWrite: %#x -> %#x (2)\n", pRingBuf->offWrite, offWrite));
            return VINF_SUCCESS;
        }
    }
    /*
     * The reader is ahead of the writer, try fit it into that space.
     */
    else if (offRead - offWrite > cb + sizeof(INTNETHDR)) /* not >= ! */
    {
        PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
        pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
        pHdr->cbFrame  = cbFrame;
        pHdr->offFrame = sizeof(INTNETHDR);

        memcpy(pHdr + 1, pvFrame, cbFrame);

        offWrite += cb + sizeof(INTNETHDR);
        ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
        Log2(("WriteFrame: offWrite: %#x -> %#x (3)\n", pRingBuf->offWrite, offWrite));
        return VINF_SUCCESS;
    }

    /* (it didn't fit) */
    /** @todo stats */
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Ethernet header.
 */
#pragma pack(1)
typedef struct INTNETETHERHDR
{
    PDMMAC  MacDst;
    PDMMAC  MacSrc;
} INTNETETHERHDR, *PINTNETETHERHDR;
#pragma pack()


/**
 * Sends a frame to a specific interface.
 *
 * @param   pIf         The interface.
 * @param   pvFrame     The frame data.
 * @param   cbFrame     The size of the frame.
 */
static void intnetIfSend(PINTNETIF pIf, const void *pvFrame, unsigned cbFrame)
{
    LogFlow(("intnetIfSend: pIf=%p:{.hIf=%RX32}\n", pIf, pIf->hIf));
    int rc = INTNETRingWriteFrame(pIf->pIntBuf, &pIf->pIntBuf->Recv, pvFrame, cbFrame);
    if (VBOX_SUCCESS(rc))
    {
        pIf->cYields = 0;
        STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatRecvs);
        STAM_REL_COUNTER_ADD(&pIf->pIntBuf->cbStatRecv, cbFrame);
        RTSemEventSignal(pIf->Event);
        return;
    }

    /*
     * Retry a few times, yielding the CPU in between.
     * But don't let a unresponsive VM harm performance, so give up after a short while.
     */
    if (pIf->cYields < 100)
    {
        unsigned cYields = 10;
        do
        {
            RTSemEventSignal(pIf->Event);
            RTThreadYield();
            rc = INTNETRingWriteFrame(pIf->pIntBuf, &pIf->pIntBuf->Recv, pvFrame, cbFrame);
            if (VBOX_SUCCESS(rc))
            {
                STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatYieldsOk);
                STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatRecvs);
                STAM_REL_COUNTER_ADD(&pIf->pIntBuf->cbStatRecv, cbFrame);
                RTSemEventSignal(pIf->Event);
                return;
            }
            pIf->cYields++;
        } while (--cYields > 0);
        STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatYieldsNok);
    }

    /* ok, the frame is lost. */
    STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatLost);
    RTSemEventSignal(pIf->Event);
}


/**
 * Sends a frame.
 *
 * This function will distribute the frame to the interfaces it is addressed to.
 * It will also update the MAC address of the sender.
 *
 * The caller must own the network mutex.
 *
 * @param   pNetwork    The network the frame is being sent to.
 * @param   pIfSender   The interface sending the frame.
 * @param   pvFrame     The frame data.
 * @param   cbFrame     The size of the frame.
 */
static void intnetNetworkSend(PINTNETNETWORK pNetwork, PINTNETIF pIfSender, const void *pvFrame, unsigned cbFrame)
{
    /*
     * Assert reality.
     */
    Assert(pNetwork);
    Assert(pIfSender);
    Assert(pNetwork == pIfSender->pNetwork);
    Assert(pvFrame);
    if (cbFrame < sizeof(PDMMAC) * 2)
        return;

    /*
     * Send statistics.
     */
    STAM_REL_COUNTER_INC(&pIfSender->pIntBuf->cStatSends);
    STAM_REL_COUNTER_ADD(&pIfSender->pIntBuf->cbStatSend, cbFrame);

    /*
     * Inspect the header updating the mac address of the sender in the process.
     */
    PINTNETETHERHDR pEthHdr = (PINTNETETHERHDR)pvFrame;
    if (memcmp(&pEthHdr->MacSrc, &pIfSender->Mac, sizeof(pIfSender->Mac)))
    {
        /** @todo stats */
        Log2(("IF MAC: %.6Rhxs -> %.6Rhxs\n", &pIfSender->Mac, &pEthHdr->MacSrc));
        pIfSender->Mac = pEthHdr->MacSrc;
        pIfSender->fMacSet = true;
    }

    if (    (pEthHdr->MacDst.au8[0] & 1) /* multicast address */
        ||  (   pEthHdr->MacDst.au16[0] == 0xffff /* broadcast address. s*/
             && pEthHdr->MacDst.au16[1] == 0xffff
             && pEthHdr->MacDst.au16[2] == 0xffff)
        )
    {
        /*
         * This is a broadcast or multicast address. For the present we treat those
         * two as the same - investigating multicast is left for later.
         *
         * Write the packet to all the interfaces and signal them.
         */
        Log2(("Broadcast\n"));
        for (PINTNETIF pIf = pNetwork->pIFs; pIf; pIf = pIf->pNext)
            if (pIf != pIfSender)
                intnetIfSend(pIf, pvFrame, cbFrame);
    }
    else
    {
        /*
         * Only send to the interfaces with matching a MAC address.
         */
        Log2(("Dst=%.6Rhxs\n", &pEthHdr->MacDst));
        for (PINTNETIF pIf = pNetwork->pIFs; pIf; pIf = pIf->pNext)
        {
            Log2(("Dst=%.6Rhxs ?==? %.6Rhxs\n", &pEthHdr->MacDst, &pIf->Mac));
            if (    (   !pIf->fMacSet
                     || !memcmp(&pIf->Mac, &pEthHdr->MacDst, sizeof(pIf->Mac)))
                ||  (   pIf->fPromiscuous
                     && pIf != pIfSender /* promiscuous mode: omit the sender */))
                intnetIfSend(pIf, pvFrame, cbFrame);
        }
    }
}


/**
 * Sends one or more frames.
 *
 * The function will first the frame which is passed as the optional
 * arguments pvFrame and cbFrame. These are optional since it also
 * possible to chain together one or more frames in the send buffer
 * which the function will process after considering it's arguments.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance data.
 * @param   hIf         The interface handle.
 * @param   pvFrame     Pointer to the frame.
 * @param   cbFrame     Size of the frame.
 */
INTNETR0DECL(int) INTNETR0IfSend(PINTNET pIntNet, INTNETIFHANDLE hIf, const void *pvFrame, unsigned cbFrame)
{
    LogFlow(("INTNETR0IfSend: pIntNet=%p hIf=%RX32 pvFrame=%p cbFrame=%u\n", pIntNet, hIf, pvFrame, cbFrame));

    /*
     * Validate input.
     */
    AssertReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = INTNETHandle2IFPtr(pIntNet, hIf);
    if (!pIf)
        return VERR_INVALID_HANDLE;
    if (pvFrame && cbFrame)
    {
        AssertReturn(cbFrame < 0x8000, VERR_INVALID_PARAMETER);
        AssertReturn(VALID_PTR(pvFrame), VERR_INVALID_PARAMETER);
        AssertReturn(VALID_PTR((uint8_t *)pvFrame + cbFrame - 1), VERR_INVALID_PARAMETER);

        /* This is the better place to crash, probe the buffer. */
        ASMProbeReadBuffer(pvFrame, cbFrame);
    }

    int rc = RTSemFastMutexRequest(pIf->pNetwork->FastMutex);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Process the argument.
     */
    if (pvFrame && cbFrame)
        intnetNetworkSend(pIf->pNetwork, pIf, pvFrame, cbFrame);

    /*
     * Process the send buffer.
     */
    while (pIf->pIntBuf->Send.offRead != pIf->pIntBuf->Send.offWrite)
    {
        /* Send the frame if the type is sane. */
        PINTNETHDR pHdr = (PINTNETHDR)((uintptr_t)pIf->pIntBuf + pIf->pIntBuf->Send.offRead);
        if (pHdr->u16Type == INTNETHDR_TYPE_FRAME)
        {
            void *pvCurFrame = INTNETHdrGetFramePtr(pHdr, pIf->pIntBuf);
            if (pvCurFrame)
                intnetNetworkSend(pIf->pNetwork, pIf, pvCurFrame, pHdr->cbFrame);
        }
        /* else: ignore the frame */

        /* Skip to the next frame. */
        INTNETRingSkipFrame(pIf->pIntBuf, &pIf->pIntBuf->Send);
    }

    return RTSemFastMutexRelease(pIf->pNetwork->FastMutex);
}


/**
 * VMMR0 request wrapper for INTNETR0IfSend.
 *
 * @returns see INTNETR0IfSend.
 * @param   pIntNet         The internal networking instance.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfSendReq(PINTNET pIntNet, PINTNETIFSENDREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfSend(pIntNet, pReq->hIf, NULL, 0);
}


/**
 * Maps the default buffer into ring 3.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance data.
 * @param   hIf         The interface handle.
 * @param   ppRing3Buf  Where to store the address of the ring-3 mapping.
 */
INTNETR0DECL(int) INTNETR0IfGetRing3Buffer(PINTNET pIntNet, INTNETIFHANDLE hIf, R3PTRTYPE(PINTNETBUF) *ppRing3Buf)
{
    LogFlow(("INTNETR0IfGetRing3Buffer: pIntNet=%p hIf=%RX32 ppRing3Buf=%p\n", pIntNet, hIf, ppRing3Buf));

    /*
     * Validate input.
     */
    AssertReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = INTNETHandle2IFPtr(pIntNet, hIf);
    if (!pIf)
        return VERR_INVALID_HANDLE;
    AssertReturn(VALID_PTR(ppRing3Buf), VERR_INVALID_PARAMETER);

    /*
     * ASSUMES that only the process that created an interface can use it.
     * ASSUMES that we created the ring-3 mapping when selecting or
     * allocating the buffer.
     */
    int rc = RTSemFastMutexRequest(pIf->pNetwork->FastMutex);
    if (VBOX_FAILURE(rc))
        return rc;

    *ppRing3Buf = pIf->pIntBufR3;

    rc = RTSemFastMutexRelease(pIf->pNetwork->FastMutex);
    LogFlow(("INTNETR0IfGetRing3Buffer: returns %Vrc *ppRing3Buf=%p\n", rc, *ppRing3Buf));
    return rc;
}


/**
 * VMMR0 request wrapper for INTNETR0IfGetRing3Buffer.
 *
 * @returns see INTNETR0IfGetRing3Buffer.
 * @param   pIntNet         The internal networking instance.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfGetRing3BufferReq(PINTNET pIntNet, PINTNETIFGETRING3BUFFERREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfGetRing3Buffer(pIntNet, pReq->hIf, &pReq->pRing3Buf);
}


/**
 * Gets the ring-0 address of the current buffer.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance data.
 * @param   hIf         The interface handle.
 * @param   ppRing0Buf  Where to store the address of the ring-3 mapping.
 */
INTNETR0DECL(int) INTNETR0IfGetRing0Buffer(PINTNET pIntNet, INTNETIFHANDLE hIf, PINTNETBUF *ppRing0Buf)
{
    LogFlow(("INTNETR0IfGetRing0Buffer: pIntNet=%p hIf=%RX32 ppRing0Buf=%p\n", pIntNet, hIf, ppRing0Buf));

    /*
     * Validate input.
     */
    AssertReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = INTNETHandle2IFPtr(pIntNet, hIf);
    if (!pIf)
        return VERR_INVALID_HANDLE;
    AssertReturn(VALID_PTR(ppRing0Buf), VERR_INVALID_PARAMETER);

    /*
     * Assuming that we're in Ring-0, this should be rather simple :-)
     */
    int rc = RTSemFastMutexRequest(pIf->pNetwork->FastMutex);
    if (VBOX_FAILURE(rc))
        return rc;

    *ppRing0Buf = pIf->pIntBuf;

    rc = RTSemFastMutexRelease(pIf->pNetwork->FastMutex);
    LogFlow(("INTNETR0IfGetRing0Buffer: returns %Vrc *ppRing0Buf=%p\n", rc, *ppRing0Buf));
    return rc;
}


#if 0
/**
 * Gets the physical addresses of the default interface buffer.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance data.
 * @param   hIF         The interface handle.
 * @param   paPages     Where to store the addresses. (The reserved fields will be set to zero.)
 * @param   cPages
 */
INTNETR0DECL(int) INTNETR0IfGetPhysBuffer(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPPAGE paPages, unsigned cPages)
{
    /*
     * Validate input.
     */
    AssertReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = INTNETHandle2IFPtr(pIntNet, hIf);
    if (!pIf)
        return VERR_INVALID_HANDLE;
    AssertReturn(VALID_PTR(paPages), VERR_INVALID_PARAMETER);
    AssertReturn(VALID_PTR((uint8_t *)&paPages[cPages] - 1), VERR_INVALID_PARAMETER);

    /*
     * Assuming that we're in Ring-0, this should be rather simple :-)
     */
    int rc = RTSemFastMutexRequest(pIf->pNetwork->FastMutex);
    if (VBOX_FAILURE(rc))
        return rc;

    /** @todo make a SUPR0 api for obtaining the array. SUPR0 is keeping track of everything, there
     * is no need for any extra bookkeeping here.. */
    //*ppRing0Buf = pIf->pIntBuf;

    //return RTSemFastMutexRelease(pIf->pNetwork->FastMutex);
    RTSemFastMutexRelease(pIf->pNetwork->FastMutex);
    return VERR_NOT_IMPLEMENTED;
}
#endif


/**
 * Sets the promiscuous mode property of an interface.
 *
 * @returns VBox status code.
 * @param   pIntNet         The instance handle.
 * @param   hIf             The interface handle.
 * @param   fPromiscuous    Set if the interface should be in promiscuous mode, clear if not.
 */
INTNETR0DECL(int) INTNETR0IfSetPromiscuousMode(PINTNET pIntNet, INTNETIFHANDLE hIf, bool fPromiscuous)
{
    LogFlow(("INTNETR0IfSetPromiscuousMode: pIntNet=%p hIf=%RX32 fPromiscuous=%d\n", pIntNet, hIf, fPromiscuous));

    /*
     * Get and validate essential handles.
     */
    AssertReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = INTNETHandle2IFPtr(pIntNet, hIf);
    if (!pIf)
    {
        LogFlow(("INTNETR0IfSetPromiscuousMode: returns VERR_INVALID_HANDLE\n"));
        return VERR_INVALID_HANDLE;
    }
    if (pIf->fPromiscuous != fPromiscuous)
    {
        Log(("INTNETR0IfSetPromiscuousMode: hIf=%RX32: Changed from %d -> %d\n",
             hIf, !fPromiscuous, !!fPromiscuous));
        ASMAtomicXchgSize(&pIf->fPromiscuous, !!fPromiscuous);
    }
    return VINF_SUCCESS;
}


/**
 * VMMR0 request wrapper for INTNETR0IfSetPromiscuousMode.
 *
 * @returns see INTNETR0IfSetPromiscuousMode.
 * @param   pIntNet         The internal networking instance.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfSetPromiscuousModeReq(PINTNET pIntNet, PINTNETIFSETPROMISCUOUSMODEREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfSetPromiscuousMode(pIntNet, pReq->hIf, pReq->fPromiscuous);
}


/**
 * Wait for the interface to get signaled.
 * The interface will be signaled when is put into the receive buffer.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance handle.
 * @param   hIf         The interface handle.
 * @param   cMillies    Number of milliseconds to wait. RT_INDEFINITE_WAIT should be
 *                      used if indefinite wait is desired.
 */
INTNETR0DECL(int) INTNETR0IfWait(PINTNET pIntNet, INTNETIFHANDLE hIf, uint32_t cMillies)
{
    LogFlow(("INTNETR0IfWait: pIntNet=%p hIf=%RX32 cMillies=%u\n", pIntNet, hIf, cMillies));

    /*
     * Get and validate essential handles.
     */
    AssertReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = INTNETHandle2IFPtr(pIntNet, hIf);
    if (!pIf)
    {
        LogFlow(("INTNETR0IfWait: returns VERR_INVALID_HANDLE\n"));
        return VERR_INVALID_HANDLE;
    }
    const INTNETIFHANDLE    hIfSelf = pIf->hIf;
    const RTSEMEVENT        Event = pIf->Event;
    if (    hIfSelf != hIf
        &&  Event != NIL_RTSEMEVENT)
    {
        LogFlow(("INTNETR0IfWait: returns VERR_SEM_DESTROYED\n"));
        return VERR_SEM_DESTROYED;
    }

    /*
     * It is tempting to check if there is data to be read here,
     * but the problem with such an approach is that it will cause
     * one unnecessary supervisor->user->supervisor trip. There is
     * already a risk for such, so we don't need to increase this.
     */

    /*
     * Increment the number of waiters before starting the wait.
     * Upon wakeup we must assert reality checking that we're not
     * already destroyed or in the process of being destroyed.
     */
    ASMAtomicIncU32(&pIf->cSleepers);
    int rc = RTSemEventWaitNoResume(Event, cMillies);
    if (pIf->Event == Event)
    {
        ASMAtomicDecU32(&pIf->cSleepers);
        if (pIf->hIf != hIf)
            rc = VERR_SEM_DESTROYED;
    }
    else
        rc = VERR_SEM_DESTROYED;
    LogFlow(("INTNETR0IfWait: returns %Vrc\n", rc));
    return rc;
}


/**
 * VMMR0 request wrapper for INTNETR0IfWait.
 *
 * @returns see INTNETR0IfWait.
 * @param   pIntNet         The internal networking instance.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfWaitReq(PINTNET pIntNet, PINTNETIFWAITREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfWait(pIntNet, pReq->hIf, pReq->cMillies);
}


/**
 * Close an interface.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance handle.
 * @param   hIf         The interface handle.
 */
INTNETR0DECL(int) INTNETR0IfClose(PINTNET pIntNet, INTNETIFHANDLE hIf)
{
    LogFlow(("INTNETR0IfClose: pIntNet=%p hIf=%RX32\n", pIntNet, hIf));

    /*
     * Get and validate essential handles.
     */
    AssertReturn(VALID_PTR(pIntNet), VERR_INVALID_PARAMETER);
    PINTNETIF pIf = INTNETHandle2IFPtr(pIntNet, hIf);
    if (!pIf)
        return VERR_INVALID_HANDLE;

    int rc = SUPR0ObjRelease(pIf->pvObj, pIf->pSession);
    LogFlow(("INTNETR0IfClose: returns %Vrc\n", rc));
    return rc;
}


/**
 * VMMR0 request wrapper for INTNETR0IfCloseReq.
 *
 * @returns see INTNETR0IfClose.
 * @param   pIntNet         The internal networking instance.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfCloseReq(PINTNET pIntNet, PINTNETIFCLOSEREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfClose(pIntNet, pReq->hIf);
}


/**
 * Interface destructor callback.
 * This is called for reference counted objectes when the count reaches 0.
 *
 * @param   pvObj       The object pointer.
 * @param   pvUser1     Pointer to the interface.
 * @param   pvUser2     Pointer to the INTNET instance data.
 */
static DECLCALLBACK(void) INTNETIfDestruct(void *pvObj, void *pvUser1, void *pvUser2)
{
    LogFlow(("INTNETIfDestruct: pvObj=%p pvUser1=%p pvUser2=%p\n", pvObj, pvUser1, pvUser2));
    PINTNETIF pIf = (PINTNETIF)pvUser1;
    PINTNET   pIntNet = (PINTNET)pvUser2;

    /*
     * Delete the interface handle so the object no longer can be opened.
     */
    if (pIf->hIf != INTNET_HANDLE_INVALID)
    {
        INTNETHandleFree(pIntNet, pIf->hIf);
        ASMAtomicXchgSize(&pIf->hIf, INTNET_HANDLE_INVALID);
    }

    /*
     * If we've got a network unlink ourselves from it.
     * Because of cleanup order we might be an orphan now.
     */
    if (pIf->pNetwork)
        SUPR0ObjRelease(pIf->pNetwork->pvObj, pIf->pSession);
    if (pIf->pNetwork)
    {
        PINTNETNETWORK pNetwork = pIf->pNetwork;
        RTSemFastMutexRequest(pNetwork->FastMutex);
        if (pNetwork->pIFs == pIf)
            pNetwork->pIFs = pIf->pNext;
        else
        {
            PINTNETIF pPrev = pNetwork->pIFs;
            while (pPrev)
            {
                if (pPrev->pNext == pIf)
                {
                    pPrev->pNext = pIf->pNext;
                    break;
                }
                pPrev = pPrev->pNext;
            }
            Assert(pPrev);
        }
        RTSemFastMutexRelease(pNetwork->FastMutex);
        pIf->pNext = NULL;
    }

    /*
     * Wakeup anyone waiting on this interface.
     *
     * We *must* make sure they have woken up properly and realized
     * that the interface is no longer valid.
     */
    if (pIf->Event != NIL_RTSEMEVENT)
    {
        RTSEMEVENT Event = pIf->Event;
        ASMAtomicXchgSize(&pIf->Event, NIL_RTSEMEVENT);
        unsigned cMaxWait = 0x1000;
        while (pIf->cSleepers && cMaxWait-- > 0)
        {
            RTSemEventSignal(Event);
            RTThreadYield();
        }
        if (pIf->cSleepers)
        {
            RTThreadSleep(1);

            cMaxWait = pIf->cSleepers;
            while (pIf->cSleepers && cMaxWait-- > 0)
            {
                RTSemEventSignal(Event);
                RTThreadSleep(10);
            }
        }
        RTSemEventDestroy(Event);
    }

    /*
     * Unmap user buffer.
     */
    if (pIf->pIntBuf != pIf->pIntBufDefault)
    {
        /** @todo user buffer */
    }

    /*
     * Unmap and Free the default buffer.
     */
    if (pIf->pIntBufDefault)
    {
        SUPR0MemFree(pIf->pSession, (RTHCUINTPTR)pIf->pIntBufDefault);
        pIf->pIntBufDefault = NULL;
        pIf->pIntBufDefaultR3 = 0;
        pIf->pIntBuf = NULL;
        pIf->pIntBufR3 = 0;
    }

    /*
     * The interface.
     */
    pIf->pvObj = NULL;
    RTMemFree(pIf);
}



/**
 * Creates a new network interface.
 *
 * The call must have opened the network for the new interface
 * and is responsible for closing it on failure. On success
 * it must leave the network opened so the interface destructor
 * can close it.
 *
 * @returns VBox status code.
 * @param   pNetwork    The network.
 * @param   pSession    The session handle.
 * @param   cbSend      The size of the send buffer.
 * @param   cbRecv      The size of the receive buffer.
 * @param   phIf        Where to store the interface handle.
 */
static int INTNETNetworkCreateIf(PINTNETNETWORK pNetwork, PSUPDRVSESSION pSession, unsigned cbSend, unsigned cbRecv, PINTNETIFHANDLE phIf)
{
    LogFlow(("INTNETNetworkCreateIf: pNetwork=%p pSession=%p cbSend=%u cbRecv=%u phIf=%p\n",
             pNetwork, pSession, cbSend, cbRecv, phIf));

    /*
     * Assert input.
     */
    Assert(VALID_PTR(pNetwork));
    Assert(VALID_PTR(phIf));

    /*
     * Allocate and initialize the interface structure.
     */
    PINTNETIF pIf = (PINTNETIF)RTMemAllocZ(sizeof(*pIf));
    if (!pIf)
        return VERR_NO_MEMORY;

    memset(&pIf->Mac, 0xff, sizeof(pIf->Mac)); /* broadcast */
    //pIf->fMacSet = 0;
    int rc = RTSemEventCreate(&pIf->Event);
    if (VBOX_SUCCESS(rc))
    {
        pIf->pSession = pSession;
        pIf->pNetwork = pNetwork;

        /*
         * Create the default buffer.
         */
        cbRecv = RT_ALIGN(RT_MAX(cbRecv, sizeof(INTNETHDR) * 4), sizeof(INTNETHDR));
        cbSend = RT_ALIGN(RT_MAX(cbSend, sizeof(INTNETHDR) * 4), sizeof(INTNETHDR));
        const unsigned cbBuf = RT_ALIGN(sizeof(*pIf->pIntBuf), sizeof(INTNETHDR)) + cbRecv + cbSend;
        rc = SUPR0MemAlloc(pIf->pSession, cbBuf, (PRTR0PTR)&pIf->pIntBufDefault, (PRTR3PTR)&pIf->pIntBufDefaultR3);
        if (VBOX_SUCCESS(rc))
        {
            pIf->pIntBuf = pIf->pIntBufDefault;
            pIf->pIntBufR3 = pIf->pIntBufDefaultR3;
            pIf->pIntBuf->cbBuf = cbBuf;
            pIf->pIntBuf->cbRecv = cbRecv;
            pIf->pIntBuf->cbSend = cbSend;
            /* receive ring buffer. */
            pIf->pIntBuf->Recv.offStart = RT_ALIGN_32(sizeof(*pIf->pIntBuf), sizeof(INTNETHDR));
            pIf->pIntBuf->Recv.offRead  = pIf->pIntBuf->Recv.offStart;
            pIf->pIntBuf->Recv.offWrite = pIf->pIntBuf->Recv.offStart;
            pIf->pIntBuf->Recv.offEnd   = pIf->pIntBuf->Recv.offStart + cbRecv;
            /* send ring buffer. */
            pIf->pIntBuf->Send.offStart = pIf->pIntBuf->Recv.offEnd;
            pIf->pIntBuf->Send.offRead  = pIf->pIntBuf->Send.offStart;
            pIf->pIntBuf->Send.offWrite = pIf->pIntBuf->Send.offStart;
            pIf->pIntBuf->Send.offEnd   = pIf->pIntBuf->Send.offStart + cbSend;

            /*
             * Link the interface to the network.
             */
            rc = RTSemFastMutexRequest(pNetwork->FastMutex);
            if (VBOX_SUCCESS(rc))
            {
                pIf->pNext = pNetwork->pIFs;
                pNetwork->pIFs = pIf;
                RTSemFastMutexRelease(pNetwork->FastMutex);

                /*
                 * Register the interface with the session.
                 */
                pIf->pvObj = SUPR0ObjRegister(pSession, SUPDRVOBJTYPE_INTERNAL_NETWORK_INTERFACE, INTNETIfDestruct, pIf, pNetwork->pIntNet);
                if (pIf->pvObj)
                {
                    pIf->hIf = INTNETHandleAllocate(pNetwork->pIntNet, pIf);
                    if (pIf->hIf != INTNET_HANDLE_INVALID)
                    {
                        *phIf = pIf->hIf;
                        LogFlow(("INTNETNetworkCreateIf: returns VINF_SUCCESS *phIf=%p\n", *phIf));
                        return VINF_SUCCESS;
                    }
                    rc = VERR_NO_MEMORY;

                    SUPR0ObjRelease(pIf->pvObj, pSession);
                    LogFlow(("INTNETNetworkCreateIf: returns %Vrc\n", rc));
                    return rc;
                }
                rc = VERR_NO_MEMORY;
                RTSemFastMutexDestroy(pNetwork->FastMutex);
                pNetwork->FastMutex = NIL_RTSEMFASTMUTEX;
            }
            SUPR0MemFree(pIf->pSession, (RTHCUINTPTR)pIf->pIntBufDefault);
            pIf->pIntBufDefault = NULL;
            pIf->pIntBuf = NULL;
        }

        RTSemEventDestroy(pIf->Event);
        pIf->Event = NIL_RTSEMEVENT;
    }
    RTMemFree(pIf);
    LogFlow(("INTNETNetworkCreateIf: returns %Vrc\n", rc));
    return rc;
}


/**
 * Close a network which was opened/created using INTNETOpenNetwork()/INTNETCreateNetwork().
 *
 * @param   pNetwork    The network to close.
 * @param   pSession    The session handle.
 */
static int INTNETNetworkClose(PINTNETNETWORK pNetwork, PSUPDRVSESSION pSession)
{
    LogFlow(("INTNETNetworkClose: pNetwork=%p pSession=%p\n", pNetwork, pSession));
    AssertReturn(VALID_PTR(pSession), VERR_INVALID_PARAMETER);
    AssertReturn(VALID_PTR(pNetwork), VERR_INVALID_PARAMETER);
    PINTNET         pIntNet = pNetwork->pIntNet;
    RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;

    RTSpinlockAcquire(pIntNet->Spinlock, &Tmp);
    int rc = SUPR0ObjRelease(pNetwork->pvObj, pSession);
    RTSpinlockRelease(pIntNet->Spinlock, &Tmp);
    LogFlow(("INTNETNetworkClose: return %Vrc\n", rc));
    return rc;
}


/**
 * Object destructor callback.
 * This is called for reference counted objectes when the count reaches 0.
 *
 * @param   pvObj       The object pointer.
 * @param   pvUser1     Pointer to the network.
 * @param   pvUser2     Pointer to the INTNET instance data.
 */
static DECLCALLBACK(void) INTNETNetworkDestruct(void *pvObj, void *pvUser1, void *pvUser2)
{
    LogFlow(("INTNETNetworkDestruct: pvObj=%p pvUser1=%p pvUser2=%p\n", pvObj, pvUser1, pvUser2));
    RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;
    PINTNETNETWORK  pNetwork = (PINTNETNETWORK)pvUser1;
    PINTNET         pIntNet = (PINTNET)pvUser2;
    Assert(pNetwork->pIntNet == pIntNet);

    /*
     * Unlink the network.s
     */
    RTSpinlockAcquire(pIntNet->Spinlock, &Tmp);
    PINTNETNETWORK pPrev = pIntNet->pNetworks;
    if (pPrev == pNetwork)
        pIntNet->pNetworks = pNetwork->pNext;
    else
    {
        for (; pPrev; pPrev = pPrev->pNext)
            if (pPrev->pNext == pNetwork)
            {
                pPrev->pNext = pNetwork->pNext;
                break;
            }
        Assert(pPrev);
    }
    pNetwork->pNext = NULL;
    pNetwork->pvObj = NULL;
    RTSpinlockRelease(pIntNet->Spinlock, &Tmp);

    /*
     * Because of the undefined order of the per session object dereferencing when closing a session,
     * we have to handle the case where the network is destroyed before the interfaces. We'll
     * deal with this by simply orphaning the interfaces.
     */
    RTSemFastMutexRequest(pNetwork->FastMutex);
    PINTNETIF pCur = pNetwork->pIFs;
    while (pCur)
    {
        PINTNETIF pNext = pCur->pNext;
        pCur->pNext    = NULL;
        pCur->pNetwork = NULL;
        pCur = pNext;
    }
    RTSemFastMutexRelease(pNetwork->FastMutex);

    /*
     * Free resources.
     */
    RTSemFastMutexDestroy(pNetwork->FastMutex);
    pNetwork->FastMutex = NIL_RTSEMFASTMUTEX;
    RTMemFree(pNetwork);
}


/**
 * Opens an existing network.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance data.
 * @param   pSession    The current session.
 * @param   pszNetwork  The network name. This has a valid length.
 * @param   ppNetwork   Where to store the pointer to the network on success.
 */
static int INTNETOpenNetwork(PINTNET pIntNet, PSUPDRVSESSION pSession, const char *pszNetwork, PINTNETNETWORK *ppNetwork)
{
    LogFlow(("INTNETOpenNetwork: pIntNet=%p pSession=%p pszNetwork=%p:{%s} ppNetwork=%p\n",
             pIntNet, pSession, pszNetwork, pszNetwork, ppNetwork));

    Assert(VALID_PTR(pIntNet));
    Assert(VALID_PTR(pSession));
    Assert(VALID_PTR(pszNetwork));
    Assert(VALID_PTR(ppNetwork));
    *ppNetwork = NULL;

    /*
     * Search networks by name.
     */
    PINTNETNETWORK pCur;
    uint8_t cchName = strlen(pszNetwork);
    Assert(cchName && cchName < sizeof(pCur->szName)); /* caller ensures this */

    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquire(pIntNet->Spinlock, &Tmp);
    pCur = pIntNet->pNetworks;
    while (pCur)
    {
        if (    pCur->cchName == cchName
            &&  !memcmp(pCur->szName, pszNetwork, cchName))
        {
            /*
             * Increment the reference and check that the
             * session can access this network.
             */
            int rc = SUPR0ObjAddRef(pCur->pvObj, pSession);
            RTSpinlockRelease(pIntNet->Spinlock, &Tmp);

            if (VBOX_SUCCESS(rc))
            {
                if (pCur->fRestrictAccess)
                    rc = SUPR0ObjVerifyAccess(pCur->pvObj, pSession, pCur->szName);
                if (VBOX_SUCCESS(rc))
                    *ppNetwork = pCur;
                else
                {
                    RTSpinlockAcquire(pIntNet->Spinlock, &Tmp);
                    SUPR0ObjRelease(pCur->pvObj, pSession);
                    RTSpinlockRelease(pIntNet->Spinlock, &Tmp);
                }
            }
            LogFlow(("INTNETOpenNetwork: returns %Vrc *ppNetwork=%p\n", rc, *ppNetwork));
            return rc;
        }
        pCur = pCur->pNext;
    }
    RTSpinlockRelease(pIntNet->Spinlock, &Tmp);

    LogFlow(("INTNETOpenNetwork: returns VERR_FILE_NOT_FOUND\n"));
    return VERR_FILE_NOT_FOUND;
}


/**
 * Creates a new network.
 *
 * The call must own the INTNET::FastMutex and has already
 * attempted opening the network.
 *
 * @returns VBox status code.
 * @param   pIntNet         The instance data.
 * @param   pszNetwork      The name of the network. This must be at least one character long and no longer
 *                          than the INTNETNETWORK::szName.
 * @param   fRestrictAccess Whether new participants should be subjected to access check or not.
 * @param   pSession        The session handle.
 * @param   ppNetwork       Where to store the network.
 */
static int INTNETCreateNetwork(PINTNET pIntNet, PSUPDRVSESSION pSession, const char *pszNetwork, bool fRestrictAccess, PINTNETNETWORK *ppNetwork)
{
    LogFlow(("INTNETCreateNetwork: pIntNet=%p pSession=%p pszNetwork=%p:{%s} ppNetwork=%p\n",
             pIntNet, pSession, pszNetwork, pszNetwork, ppNetwork));

    Assert(VALID_PTR(pIntNet));
    Assert(VALID_PTR(pSession));
    Assert(VALID_PTR(pszNetwork));
    Assert(VALID_PTR(ppNetwork));
    *ppNetwork = NULL;

    /*
     * Verify that the network doesn't exist.
     */
    const uint8_t cchName = strlen(pszNetwork);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquire(pIntNet->Spinlock, &Tmp);
    for (PINTNETNETWORK pCur = pIntNet->pNetworks; pCur; pCur = pCur->pNext)
        if (    pCur->cchName == cchName
            &&  !memcmp(pCur->szName, pszNetwork, cchName))
        {
            RTSpinlockRelease(pIntNet->Spinlock, &Tmp);
            LogFlow(("INTNETCreateNetwork: returns VERR_ALREADY_EXISTS\n"));
            return VERR_ALREADY_EXISTS;
        }
    RTSpinlockRelease(pIntNet->Spinlock, &Tmp);

    /*
     * Allocate and initialize.
     */
    PINTNETNETWORK pNew = (PINTNETNETWORK)RTMemAllocZ(sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;
    int rc = RTSemFastMutexCreate(&pNew->FastMutex);
    if (VBOX_SUCCESS(rc))
    {
        //pNew->pIFs = NULL;
        pNew->pIntNet = pIntNet;
        pNew->cchName = cchName;
        pNew->fRestrictAccess = fRestrictAccess;
        Assert(cchName && cchName < sizeof(pNew->szName));  /* caller's responsibility. */
        memcpy(pNew->szName, pszNetwork, cchName);          /* '\0' by alloc. */

        /*
         * Register the object in the current session.
         */
        pNew->pvObj = SUPR0ObjRegister(pSession, SUPDRVOBJTYPE_INTERNAL_NETWORK, INTNETNetworkDestruct, pNew, pIntNet);
        if (pNew->pvObj)
        {
            /*
             * Insert the network into the list.
             * This must be done before we attempt any SUPR0ObjRelease call.
             */
            RTSpinlockAcquire(pIntNet->Spinlock, &Tmp);
            pNew->pNext = pIntNet->pNetworks;
            pIntNet->pNetworks = pNew;
            RTSpinlockRelease(pIntNet->Spinlock, &Tmp);

            /*
             * Check if the current session is actually allowed to create and open
             * the network. It is possible to implement network name based policies
             * and these must be checked now. SUPR0ObjRegister does no such checks.
             */
            rc = SUPR0ObjVerifyAccess(pNew->pvObj, pSession, pNew->szName);
            if (VBOX_SUCCESS(rc))
            {
                *ppNetwork = pNew;
                LogFlow(("INTNETCreateNetwork: returns VINF_SUCCESS *ppNetwork=%p\n", pNew));
                return VINF_SUCCESS;
            }

            /* The release will destroy the object. */
            SUPR0ObjRelease(pNew->pvObj, pSession);
            LogFlow(("INTNETCreateNetwork: returns %Vrc\n", rc));
            return rc;
        }
        rc = VERR_NO_MEMORY;

        RTSemFastMutexDestroy(pNew->FastMutex);
        pNew->FastMutex = NIL_RTSEMFASTMUTEX;
    }
    RTMemFree(pNew);
    LogFlow(("INTNETCreateNetwork: returns %Vrc\n", rc));
    return rc;
}


/**
 * Opens a network interface and attaches it to the specified network.
 *
 * @returns VBox status code.
 * @param   pIntNet         The internal network instance.
 * @param   pSession        The session handle.
 * @param   pszNetwork      The network name.
 * @param   cbSend          The send buffer size.
 * @param   cbRecv          The receive buffer size.
 * @param   fRestrictAccess Whether new participants should be subjected to access check or not.
 * @param   phIf            Where to store the handle to the network interface.
 */
INTNETR0DECL(int) INTNETR0Open(PINTNET pIntNet, PSUPDRVSESSION pSession, const char *pszNetwork, unsigned cbSend, unsigned cbRecv, bool fRestrictAccess, PINTNETIFHANDLE phIf)
{
    LogFlow(("INTNETR0Open: pIntNet=%p pSession=%p pszNetwork=%p:{%s} cbSend=%u cbRecv=%u phIf=%p\n",
             pIntNet, pSession, pszNetwork, pszNetwork, cbSend, cbRecv, phIf));

    /*
     * Validate input.
     */
    AssertReturn(VALID_PTR(pIntNet), VERR_INVALID_PARAMETER);
    AssertReturn(VALID_PTR(pszNetwork), VERR_INVALID_PARAMETER);
    const char *pszNetworkEnd = (const char *)memchr(pszNetwork, '\0', INTNET_MAX_NETWORK_NAME);
    AssertReturn(pszNetworkEnd, VERR_INVALID_PARAMETER);
    size_t cchNetwork = pszNetworkEnd - pszNetwork;
    AssertReturn(cchNetwork, VERR_INVALID_PARAMETER);
    AssertReturn(VALID_PTR(phIf), VERR_INVALID_PARAMETER);

    /*
     * Acquire the mutex to serialize open/create.
     */
    int rc = RTSemFastMutexRequest(pIntNet->FastMutex);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Try open/create the network.
     */
    PINTNETNETWORK pNetwork;
    rc = INTNETOpenNetwork(pIntNet, pSession, pszNetwork, &pNetwork);
    if (rc == VERR_FILE_NOT_FOUND)
        rc = INTNETCreateNetwork(pIntNet, pSession, pszNetwork, fRestrictAccess, &pNetwork);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Create a new interface to this network.
         * On failure we close the network. On success it remains open untill the
         * interface is destroyed or the last session is doing cleanup (order problems).
         */
        rc = INTNETNetworkCreateIf(pNetwork, pSession, cbSend, cbRecv, phIf);
        if (VBOX_FAILURE(rc))
            INTNETNetworkClose(pNetwork, pSession);
    }

    RTSemFastMutexRelease(pIntNet->FastMutex);

    LogFlow(("INTNETR0Open: return %Vrc *phIf=%RX32\n", rc, *phIf));
    return rc;
}


/**
 * VMMR0 request wrapper for GMMR0MapUnmapChunk.
 *
 * @returns see GMMR0MapUnmapChunk.
 * @param   pIntNet         The internal networking instance.
 * @param   pSession        The session handle.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0OpenReq(PINTNET pIntNet, PSUPDRVSESSION pSession, PINTNETOPENREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0Open(pIntNet, pSession, &pReq->szNetwork[0], pReq->cbSend, pReq->cbRecv, pReq->fRestrictAccess, &pReq->hIf);
}


/**
 * Destroys an instance of the Ring-0 internal networking service.
 *
 * @param   pIntNet     Pointer to the instance data.
 */
INTNETR0DECL(void) INTNETR0Destroy(PINTNET pIntNet)
{
    LogFlow(("INTNETR0Destroy: pIntNet=%p\n", pIntNet));

    /*
     * Allow NULL pointers.
     */
    if (!pIntNet)
        return;

    /*
     * There is not supposed to be any networks hanging around at this time.
     */
    Assert(pIntNet->pNetworks == NULL);
    if (pIntNet->FastMutex != NIL_RTSEMFASTMUTEX)
    {
        RTSemFastMutexDestroy(pIntNet->FastMutex);
        pIntNet->FastMutex = NIL_RTSEMFASTMUTEX;
    }
    if (pIntNet->Spinlock != NIL_RTSPINLOCK)
    {
        RTSpinlockDestroy(pIntNet->Spinlock);
        pIntNet->Spinlock = NIL_RTSPINLOCK;
    }

    RTMemFree(pIntNet);
}


/**
 * Create an instance of the Ring-0 internal networking service.
 *
 * @returns VBox status code.
 * @param   ppIntNet    Where to store the instance pointer.
 */
INTNETR0DECL(int) INTNETR0Create(PINTNET *ppIntNet)
{
    LogFlow(("INTNETR0Create: ppIntNet=%p\n", ppIntNet));
    int rc = VERR_NO_MEMORY;
    PINTNET pIntNet = (PINTNET)RTMemAllocZ(sizeof(*pIntNet));
    if (pIntNet)
    {
        //pIntNet->pNetworks              = NULL;
        //pIntNet->IfHandles.paEntries    = NULL;
        //pIntNet->IfHandles.cAllocated   = 0;
        pIntNet->IfHandles.iHead        = ~0U;
        pIntNet->IfHandles.iTail        = ~0U;

        rc = RTSemFastMutexCreate(&pIntNet->FastMutex);
        if (VBOX_SUCCESS(rc))
        {
            rc = RTSpinlockCreate(&pIntNet->Spinlock);
            if (VBOX_SUCCESS(rc))
            {
                *ppIntNet = pIntNet;
                LogFlow(("INTNETR0Create: returns VINF_SUCCESS *ppIntNet=%p\n", pIntNet));
                return VINF_SUCCESS;
            }
            RTSemFastMutexDestroy(pIntNet->FastMutex);
        }
        RTMemFree(pIntNet);
    }
    *ppIntNet = NULL;
    LogFlow(("INTNETR0Create: returns %Vrc\n", rc));
    return rc;
}

