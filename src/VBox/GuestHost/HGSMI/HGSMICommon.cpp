/* $Id$ */
/** @file
 * VBox Host Guest Shared Memory Interface (HGSMI) - Functions common to both host and guest.
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

#define LOG_DISABLED /* Maybe we can enabled it all the time now? */
#define LOG_GROUP LOG_GROUP_HGSMI
#include <iprt/heap.h>
#include <iprt/string.h>

#include <VBox/HGSMI/HGSMI.h>
#include <VBox/log.h>


/* Channel flags. */
#define HGSMI_CH_F_REGISTERED 0x01

/* Assertions for situations which could happen and normally must be processed properly
 * but must be investigated during development: guest misbehaving, etc.
 */
#ifdef HGSMI_STRICT
#define HGSMI_STRICT_ASSERT_FAILED() AssertFailed()
#define HGSMI_STRICT_ASSERT(expr) Assert(expr)
#else
#define HGSMI_STRICT_ASSERT_FAILED() do {} while (0)
#define HGSMI_STRICT_ASSERT(expr) do {} while (0)
#endif /* !HGSMI_STRICT */

/* One-at-a-Time Hash from
 * http://www.burtleburtle.net/bob/hash/doobs.html
 *
 * ub4 one_at_a_time(char *key, ub4 len)
 * {
 *   ub4   hash, i;
 *   for (hash=0, i=0; i<len; ++i)
 *   {
 *     hash += key[i];
 *     hash += (hash << 10);
 *     hash ^= (hash >> 6);
 *   }
 *   hash += (hash << 3);
 *   hash ^= (hash >> 11);
 *   hash += (hash << 15);
 *   return hash;
 * }
 */

static uint32_t hgsmiHashBegin (void)
{
    return 0;
}

static uint32_t hgsmiHashProcess (uint32_t hash,
                                  const void *pvData,
                                  size_t cbData)
{
    const uint8_t *pu8Data = (const uint8_t *)pvData;

    while (cbData--)
    {
        hash += *pu8Data++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    return hash;
}

static uint32_t hgsmiHashEnd (uint32_t hash)
{
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

uint32_t HGSMIChecksum (HGSMIOFFSET offBuffer,
                        const HGSMIBUFFERHEADER *pHeader,
                        const HGSMIBUFFERTAIL *pTail)
{
    uint32_t u32Checksum = hgsmiHashBegin ();

    u32Checksum = hgsmiHashProcess (u32Checksum, &offBuffer, sizeof (offBuffer));
    u32Checksum = hgsmiHashProcess (u32Checksum, pHeader, sizeof (HGSMIBUFFERHEADER));
    u32Checksum = hgsmiHashProcess (u32Checksum, pTail, RT_OFFSETOF(HGSMIBUFFERTAIL, u32Checksum));

    return hgsmiHashEnd (u32Checksum);
}

static HGSMIOFFSET hgsmiBufferInitializeSingle (const HGSMIAREA *pArea,
                                                HGSMIBUFFERHEADER *pHeader,
                                                uint32_t u32DataSize,
                                                uint8_t u8Channel,
                                                uint16_t u16ChannelInfo)
{
    if (   !pArea
        || !pHeader)
    {
        return HGSMIOFFSET_VOID;
    }

    /* Buffer must be within the area:
     *   * header data size do not exceed the maximum data size;
     *   * buffer address is greater than the area base address;
     *   * buffer address is lower than the maximum allowed for the given data size.
     */
    HGSMISIZE cbMaximumDataSize = pArea->offLast - pArea->offBase;

    if (   u32DataSize > cbMaximumDataSize
        || (uint8_t *)pHeader < pArea->pu8Base
        || (uint8_t *)pHeader > pArea->pu8Base + cbMaximumDataSize - u32DataSize)
    {
        return HGSMIOFFSET_VOID;
    }

    HGSMIOFFSET offBuffer = HGSMIPointerToOffset (pArea, pHeader);

    pHeader->u8Flags        = HGSMI_BUFFER_HEADER_F_SEQ_SINGLE;
    pHeader->u32DataSize    = u32DataSize;
    pHeader->u8Channel      = u8Channel;
    pHeader->u16ChannelInfo = u16ChannelInfo;
    memset (pHeader->u.au8Union, 0, sizeof (pHeader->u.au8Union));

    HGSMIBUFFERTAIL *pTail = HGSMIBufferTail (pHeader);

    pTail->u32Reserved = 0;
    pTail->u32Checksum = HGSMIChecksum (offBuffer, pHeader, pTail);

    return offBuffer;
}

int HGSMIAreaInitialize (HGSMIAREA *pArea, void *pvBase, HGSMISIZE cbArea, HGSMIOFFSET offBase)
{
    uint8_t *pu8Base = (uint8_t *)pvBase;

    if (  !pArea                                   /* Check that the area: */
        || cbArea < HGSMIBufferMinimumSize ()      /* Large enough. */
        || pu8Base + cbArea < pu8Base              /* No address space wrap. */
        || offBase > UINT32_C(0xFFFFFFFF) - cbArea /* Area within the 32 bit space: offBase + cbMem <= 0xFFFFFFFF */
       )
    {
        return VERR_INVALID_PARAMETER;
    }

    pArea->pu8Base = pu8Base;
    pArea->offBase = offBase;
    pArea->offLast = cbArea - HGSMIBufferMinimumSize () + offBase;
    pArea->cbArea = cbArea;

    return VINF_SUCCESS;
}

void HGSMIAreaClear (HGSMIAREA *pArea)
{
    if (pArea)
    {
        memset (pArea, 0, sizeof (HGSMIAREA));
    }
}

/* Initialize the memory buffer including its checksum.
 * No changes alloed to the header and the tail after that.
 */
HGSMIOFFSET HGSMIBufferInitializeSingle (const HGSMIAREA *pArea,
                                         HGSMIBUFFERHEADER *pHeader,
                                         HGSMISIZE cbBuffer,
                                         uint8_t u8Channel,
                                         uint16_t u16ChannelInfo)
{
    if (cbBuffer < HGSMIBufferMinimumSize ())
    {
        return HGSMIOFFSET_VOID;
    }

    return hgsmiBufferInitializeSingle (pArea, pHeader, cbBuffer - HGSMIBufferMinimumSize (), u8Channel, u16ChannelInfo);
}

void HGSMIHeapSetupUnitialized (HGSMIHEAP *pHeap)
{
    pHeap->u.hPtr = NIL_RTHEAPSIMPLE;
    pHeap->cRefs = 0;
    pHeap->area.cbArea = 0;
    pHeap->area.offBase = HGSMIOFFSET_VOID;
    pHeap->area.offLast = HGSMIOFFSET_VOID;
    pHeap->area.pu8Base = 0;
    pHeap->fOffsetBased = false;
}

bool HGSMIHeapIsItialized (HGSMIHEAP *pHeap)
{
    return pHeap->u.hPtr != NIL_RTHEAPSIMPLE;
}

int HGSMIHeapRelocate (HGSMIHEAP *pHeap,
                       void *pvBase,
                       uint32_t offHeapHandle,
                       uintptr_t offDelta,
                       HGSMISIZE cbArea,
                       HGSMIOFFSET offBase,
                       bool fOffsetBased
                       )
{
    if (   !pHeap
        || !pvBase)
    {
        return VERR_INVALID_PARAMETER;
    }

    int rc = HGSMIAreaInitialize (&pHeap->area, pvBase, cbArea, offBase);

    if (RT_SUCCESS (rc))
    {
        if (fOffsetBased)
            pHeap->u.hOff = (RTHEAPOFFSET)((uint8_t *)pvBase + offHeapHandle);
        else
        {
            pHeap->u.hPtr = (RTHEAPSIMPLE)((uint8_t *)pvBase + offHeapHandle);
            rc = RTHeapSimpleRelocate (pHeap->u.hPtr, offDelta); AssertRC(rc);
        }
        if (RT_SUCCESS (rc))
        {
            pHeap->cRefs = 0;
            pHeap->fOffsetBased = fOffsetBased;
        }
        else
        {
            HGSMIAreaClear (&pHeap->area);
        }
    }

    return rc;
}

int HGSMIHeapSetup (HGSMIHEAP *pHeap,
                    void *pvBase,
                    HGSMISIZE cbArea,
                    HGSMIOFFSET offBase,
                    bool fOffsetBased)
{
    if (   !pHeap
        || !pvBase)
    {
        return VERR_INVALID_PARAMETER;
    }

    int rc = HGSMIAreaInitialize (&pHeap->area, pvBase, cbArea, offBase);

    if (RT_SUCCESS (rc))
    {
        if (!fOffsetBased)
            rc = RTHeapSimpleInit (&pHeap->u.hPtr, pvBase, cbArea);
        else
            rc = RTHeapOffsetInit (&pHeap->u.hOff, pvBase, cbArea);

        if (RT_SUCCESS (rc))
        {
            pHeap->cRefs = 0;
            pHeap->fOffsetBased = fOffsetBased;
        }
        else
        {
            HGSMIAreaClear (&pHeap->area);
        }
    }

    return rc;
}

void HGSMIHeapDestroy (HGSMIHEAP *pHeap)
{
    if (pHeap)
    {
        Assert(!pHeap->cRefs);
        pHeap->u.hPtr = NIL_RTHEAPSIMPLE;
        HGSMIAreaClear (&pHeap->area);
        pHeap->cRefs = 0;
    }
}

void *HGSMIHeapAlloc (HGSMIHEAP *pHeap,
                      HGSMISIZE cbData,
                      uint8_t u8Channel,
                      uint16_t u16ChannelInfo)
{
    if (pHeap->u.hPtr == NIL_RTHEAPSIMPLE)
    {
        return NULL;
    }

    size_t cbAlloc = HGSMIBufferRequiredSize (cbData);

    HGSMIBUFFERHEADER *pHeader;
    if (!pHeap->fOffsetBased)
        pHeader = (HGSMIBUFFERHEADER *)RTHeapSimpleAlloc (pHeap->u.hPtr, cbAlloc, 0);
    else
        pHeader = (HGSMIBUFFERHEADER *)RTHeapOffsetAlloc (pHeap->u.hOff, cbAlloc, 0);

    if (!pHeader)
    {
        return NULL;
    }

    ++pHeap->cRefs;

    hgsmiBufferInitializeSingle (&pHeap->area, pHeader, cbData, u8Channel, u16ChannelInfo);

    return HGSMIBufferData (pHeader);
}

HGSMIOFFSET HGSMIHeapBufferOffset (HGSMIHEAP *pHeap,
                                   void *pvData)
{
    HGSMIBUFFERHEADER *pHeader = HGSMIBufferHeaderFromData (pvData);

    HGSMIOFFSET offBuffer = HGSMIPointerToOffset (&pHeap->area, pHeader);

    return offBuffer;
}

void HGSMIHeapFree (HGSMIHEAP *pHeap,
                    void *pvData)
{
    if (   pvData
        && pHeap->u.hPtr != NIL_RTHEAPSIMPLE)
    {
        HGSMIBUFFERHEADER *pHeader = HGSMIBufferHeaderFromData (pvData);

        if (!pHeap->fOffsetBased)
            RTHeapSimpleFree (pHeap->u.hPtr, pHeader);
        else
            RTHeapOffsetFree (pHeap->u.hOff, pHeader);

        --pHeap->cRefs;
    }
}

/* Verify that the given offBuffer points to a valid buffer, which is within the area.
 */
static const HGSMIBUFFERHEADER *hgsmiVerifyBuffer (const HGSMIAREA *pArea,
                                                   HGSMIOFFSET offBuffer)
{
    AssertPtr(pArea);

    LogFlowFunc(("buffer 0x%x, area %p %x [0x%x;0x%x]\n", offBuffer, pArea->pu8Base, pArea->cbArea, pArea->offBase, pArea->offLast));

    if (   offBuffer < pArea->offBase
        || offBuffer > pArea->offLast)
    {
        LogFunc(("offset 0x%x is outside the area [0x%x;0x%x]!!!\n", offBuffer, pArea->offBase, pArea->offLast));
        HGSMI_STRICT_ASSERT_FAILED();
        return NULL;
    }

    const HGSMIBUFFERHEADER *pHeader = HGSMIOffsetToPointer (pArea, offBuffer);

    /* Quick check of the data size, it should be less than the maximum
     * data size for the buffer at this offset.
     */
    LogFlowFunc(("datasize check: pHeader->u32DataSize = 0x%x pArea->offLast - offBuffer = 0x%x\n", pHeader->u32DataSize, pArea->offLast - offBuffer));
    if (pHeader->u32DataSize <= pArea->offLast - offBuffer)
    {
        HGSMIBUFFERTAIL *pTail = HGSMIBufferTail (pHeader);

        /* At least both pHeader and pTail structures are in the area. Check the checksum. */
        uint32_t u32Checksum = HGSMIChecksum (offBuffer, pHeader, pTail);

        LogFlowFunc(("checksum check: u32Checksum = 0x%x pTail->u32Checksum = 0x%x\n", u32Checksum, pTail->u32Checksum));
        if (u32Checksum == pTail->u32Checksum)
        {
            LogFlowFunc(("returning %p\n", pHeader));
            return pHeader;
        }
        else
        {
            LogFunc(("invalid checksum 0x%x, expected 0x%x!!!\n", u32Checksum, pTail->u32Checksum));
            HGSMI_STRICT_ASSERT_FAILED();
        }
    }
    else
    {
        LogFunc(("invalid data size 0x%x, maximum is 0x%x!!!\n", pHeader->u32DataSize, pArea->offLast - offBuffer));
        HGSMI_STRICT_ASSERT_FAILED();
    }

    LogFlowFunc(("returning NULL\n"));
    return NULL;
}

/* A wrapper to safely call the handler.
 */
int HGSMIChannelHandlerCall (const HGSMICHANNELHANDLER *pHandler,
                             const HGSMIBUFFERHEADER *pHeader)
{
    LogFlowFunc(("pHandler %p, pHeader %p\n", pHandler, pHeader));

    int rc;

    Assert(pHandler && pHandler->pfnHandler);

    if (   pHandler
        && pHandler->pfnHandler)
    {
        void *pvBuffer = HGSMIBufferData (pHeader);
        HGSMISIZE cbBuffer = pHeader->u32DataSize;

        rc = pHandler->pfnHandler (pHandler->pvHandler, pHeader->u16ChannelInfo, pvBuffer, cbBuffer);
    }
    else
    {
        /* It is a NOOP case here. */
        rc = VINF_SUCCESS;
    }

    LogFlowFunc(("leave rc = %Rrc\n", rc));

    return rc;
}

/*
 * Process a guest buffer.
 * @thread EMT
 */
static int hgsmiBufferProcess (const HGSMICHANNEL *pChannel,
                                    const HGSMIBUFFERHEADER *pHeader)
{
    LogFlowFunc(("pChannel %p, pHeader %p\n", pChannel, pHeader));

    int rc = HGSMIChannelHandlerCall (&pChannel->handler,
                                      pHeader);

    return rc;
}

HGSMICHANNEL *HGSMIChannelFindById (HGSMICHANNELINFO * pChannelInfo,
                                           uint8_t u8Channel)
{
    HGSMICHANNEL *pChannel = &pChannelInfo->Channels[u8Channel];

    if (pChannel->u8Flags & HGSMI_CH_F_REGISTERED)
    {
        return pChannel;
    }

    return NULL;
}

int HGSMIBufferProcess (HGSMIAREA *pArea,
                         HGSMICHANNELINFO * pChannelInfo,
                         HGSMIOFFSET offBuffer)
{
    LogFlowFunc(("pArea %p, offBuffer 0x%x\n", pArea, offBuffer));

    AssertPtr(pArea);
    AssertPtr(pChannelInfo);

    int rc = VERR_GENERAL_FAILURE;

//    VM_ASSERT_EMT(pIns->pVM);

    /* Guest has prepared a command description at 'offBuffer'. */
    const HGSMIBUFFERHEADER *pHeader = hgsmiVerifyBuffer (pArea, offBuffer);
    Assert(pHeader);
    if (pHeader)
    {
        /* Pass the command to the appropriate handler registered with this instance.
         * Start with the handler list head, which is the preallocated HGSMI setup channel.
         */
        HGSMICHANNEL *pChannel = HGSMIChannelFindById (pChannelInfo, pHeader->u8Channel);
        Assert(pChannel);
        if (pChannel)
        {
            hgsmiBufferProcess (pChannel, pHeader);
            HGSMI_STRICT_ASSERT(hgsmiVerifyBuffer (pArea, offBuffer) != NULL);
            rc = VINF_SUCCESS;
        }
        else
        {
            rc = VERR_INVALID_FUNCTION;
        }
    }
    else
    {
        rc = VERR_INVALID_HANDLE;
//        LogRel(("HGSMI[%s]: ignored invalid guest buffer 0x%08X!!!\n", pIns->pszName, offBuffer));
    }
    return rc;
}

/* Register a new VBVA channel by index.
 *
 */
int HGSMIChannelRegister (HGSMICHANNELINFO * pChannelInfo,
                                 uint8_t u8Channel,
                                 const char *pszName,
                                 PFNHGSMICHANNELHANDLER pfnChannelHandler,
                                 void *pvChannelHandler,
                                 HGSMICHANNELHANDLER *pOldHandler)
{
    AssertPtrReturn(pOldHandler, VERR_INVALID_PARAMETER);

    /* Check whether the channel is already registered. */
    HGSMICHANNEL *pChannel = HGSMIChannelFindById (pChannelInfo, u8Channel);

    if (!pChannel)
    {
        /* Channel is not yet registered. */
        pChannel = &pChannelInfo->Channels[u8Channel];

        pChannel->u8Flags = HGSMI_CH_F_REGISTERED;
        pChannel->u8Channel = u8Channel;

        pChannel->handler.pfnHandler = NULL;
        pChannel->handler.pvHandler = NULL;

        pChannel->pszName = pszName;
    }

    *pOldHandler = pChannel->handler;

    pChannel->handler.pfnHandler = pfnChannelHandler;
    pChannel->handler.pvHandler = pvChannelHandler;

    return VINF_SUCCESS;
}

