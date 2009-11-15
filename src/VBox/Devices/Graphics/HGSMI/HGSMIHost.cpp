/** @file
 *
 * VBox Host Guest Shared Memory Interface (HGSMI).
 * Host part:
 *  - virtual hardware IO handlers;
 *  - channel management;
 *  - low level interface for buffer transfer.
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


/*
 * Async host->guest calls. Completion by an IO write from the guest or a timer timeout.
 *
 * Sync guest->host calls. Initiated by an IO write from the guest.
 *
 * Guest->Host
 * ___________
 *
 * Synchronous for the guest, an async result can be also reported later by a host->guest call:
 *
 * G: Alloc shared memory, fill the structure, issue an IO write (HGSMI_IO_GUEST) with the memory offset.
 * H: Verify the shared memory and call the handler.
 * G: Continue after the IO completion.
 *
 *
 * Host->Guest
 * __________
 *
 * H:      Alloc shared memory, fill in the info.
 *         Register in the FIFO with a callback, issue IRQ (on EMT).
 *         Wait on a sem with timeout if necessary.
 * G:      Read FIFO from HGSMI_IO_HOST_COMMAND.
 * H(EMT): Get the shared memory offset from FIFO to return to the guest.
 * G:      Get offset, process command, issue IO write to HGSMI_IO_HOST_COMMAND.
 * H(EMT): Find registered shared mem, run callback, which could post the sem.
 * H:      Get results and free shared mem (could be freed automatically on EMT too).
 *
 *
 * Implementation notes:
 *
 * Host->Guest
 *
 * * Shared memory allocation using a critsect.
 * * FIFO manipulation with a critsect.
 *
 */

#include <iprt/alloc.h>
#include <iprt/critsect.h>
#include <iprt/heap.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>

#include <VBox/err.h>
#define LOG_GROUP LOG_GROUP_DEV_VGA
#include <VBox/log.h>
#include <VBox/ssm.h>

#include "HGSMIHost.h"
#include "VBox/HGSMI/HGSMIChannels.h"
#include "VBox/HGSMI/HGSMIChSetup.h"

#include "HGSMIHostHlp.h"

#ifdef DEBUG_sunlover
#define HGSMI_STRICT 1
#endif /* !DEBUG_sunlover */

#ifdef DEBUG_misha
# define VBOXHGSMI_STATE_DEBUG
#endif

#ifdef VBOXHGSMI_STATE_DEBUG
#define VBOXHGSMI_STATE_START_MAGIC 0x12345678
#define VBOXHGSMI_STATE_STOP_MAGIC  0x87654321
#define VBOXHGSMI_STATE_FIFOSTART_MAGIC 0x9abcdef1
#define VBOXHGSMI_STATE_FIFOSTOP_MAGIC 0x1fedcba9

#define VBOXHGSMI_SAVE_START(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VBOXHGSMI_STATE_START_MAGIC); AssertRC(rc);}while(0)
#define VBOXHGSMI_SAVE_STOP(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VBOXHGSMI_STATE_STOP_MAGIC); AssertRC(rc);}while(0)
#define VBOXHGSMI_SAVE_FIFOSTART(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VBOXHGSMI_STATE_FIFOSTART_MAGIC); AssertRC(rc);}while(0)
#define VBOXHGSMI_SAVE_FIFOSTOP(_pSSM) do{ int rc = SSMR3PutU32(_pSSM, VBOXHGSMI_STATE_FIFOSTOP_MAGIC); AssertRC(rc);}while(0)

#define VBOXHGSMI_LOAD_CHECK(_pSSM, _v) \
    do{ \
        uint32_t u32; \
        int rc = SSMR3GetU32(_pSSM, &u32); AssertRC(rc); \
        Assert(u32 == (_v)); \
    }while(0)

#define VBOXHGSMI_LOAD_START(_pSSM) VBOXHGSMI_LOAD_CHECK(_pSSM, VBOXHGSMI_STATE_START_MAGIC)
#define VBOXHGSMI_LOAD_FIFOSTART(_pSSM) VBOXHGSMI_LOAD_CHECK(_pSSM, VBOXHGSMI_STATE_FIFOSTART_MAGIC)
#define VBOXHGSMI_LOAD_FIFOSTOP(_pSSM) VBOXHGSMI_LOAD_CHECK(_pSSM, VBOXHGSMI_STATE_FIFOSTOP_MAGIC)
#define VBOXHGSMI_LOAD_STOP(_pSSM) VBOXHGSMI_LOAD_CHECK(_pSSM, VBOXHGSMI_STATE_STOP_MAGIC)
#else
#define VBOXHGSMI_SAVE_START(_pSSM) do{ }while(0)
#define VBOXHGSMI_SAVE_STOP(_pSSM) do{ }while(0)
#define VBOXHGSMI_SAVE_FIFOSTART(_pSSM) do{ }while(0)
#define VBOXHGSMI_SAVE_FIFOSTOP(_pSSM) do{ }while(0)


#define VBOXHGSMI_LOAD_START(_pSSM) do{ }while(0)
#define VBOXHGSMI_LOAD_FIFOSTART(_pSSM) do{ }while(0)
#define VBOXHGSMI_LOAD_FIFOSTOP(_pSSM) do{ }while(0)
#define VBOXHGSMI_LOAD_STOP(_pSSM) do{ }while(0)

#endif

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


typedef struct _HGSMIINSTANCE
{
    PVM pVM;                           /* The VM. */

    const char *pszName;               /* A name for the instance. Mostyl used in the log. */

    RTCRITSECT   instanceCritSect;     /* For updating the instance data: FIFO's, channels. */

    HGSMIAREA area; /* The shared memory description. */
    HGSMIHEAP hostHeap;                /* Host heap instance. */
    RTCRITSECT    hostHeapCritSect;    /* Heap serialization lock. */

    HGSMILIST hostFIFO;                /* Pending host buffers. */
    HGSMILIST hostFIFORead;            /* Host buffers readed by the guest. */
    HGSMILIST hostFIFOProcessed;       /* Processed by the guest. */
    HGSMILIST hostFIFOFree;            /* Buffers for reuse. */
    RTCRITSECT    hostFIFOCritSect;    /* FIFO serialization lock. */

    PFNHGSMINOTIFYGUEST pfnNotifyGuest; /* Guest notification callback. */
    void *pvNotifyGuest;               /* Guest notification callback context. */

    volatile HGSMIHOSTFLAGS * pHGFlags;

    HGSMICHANNELINFO channelInfo;      /* Channel handlers indexed by the channel id.
                                                      * The array is accessed under the instance lock.
                                                      */
 }  HGSMIINSTANCE;


typedef DECLCALLBACK(void) FNHGSMIHOSTFIFOCALLBACK(void *pvCallback);
typedef FNHGSMIHOSTFIFOCALLBACK *PFNHGSMIHOSTFIFOCALLBACK;

typedef struct _HGSMIHOSTFIFOENTRY
{
    /* The list field. Must be the first field. */
    HGSMILISTENTRY entry;

    /* Backlink to the HGSMI instance. */
    HGSMIINSTANCE *pIns;

#if 0
    /* removed to allow saved state handling */
    /* The event which is signalled when the command has been processed by the host. */
    RTSEMEVENTMULTI hEvent;
#endif
    /* Status flags of the entry. */
    volatile uint32_t fl;

    /* Offset in the memory region of the entry data. */
    HGSMIOFFSET offBuffer;

#if 0
    /* removed to allow saved state handling */
    /* The command completion callback. */
    PFNHGSMIHOSTFIFOCALLBACK pfnCallback;
    void *pvCallback;
#endif

} HGSMIHOSTFIFOENTRY;

#define HGSMILISTENTRY_2_FIFOENTRY(_pe) \
    ( (HGSMIHOSTFIFOENTRY*)((uint8_t *)(_pe) - RT_OFFSETOF(HGSMIHOSTFIFOENTRY, entry)) )
//AssertCompile(RT_OFFSETOF(HGSMIHOSTFIFOENTRY, entry) == 0);


#define HGSMI_F_HOST_FIFO_ALLOCATED 0x0001
#define HGSMI_F_HOST_FIFO_QUEUED    0x0002
#define HGSMI_F_HOST_FIFO_READ      0x0004
#define HGSMI_F_HOST_FIFO_PROCESSED 0x0008
#define HGSMI_F_HOST_FIFO_FREE      0x0010
#define HGSMI_F_HOST_FIFO_CANCELED  0x0020

static DECLCALLBACK(void) hgsmiHostCommandFreeCallback (void *pvCallback);

static int hgsmiLock (HGSMIINSTANCE *pIns)
{
    int rc = RTCritSectEnter (&pIns->instanceCritSect);
    AssertRC (rc);
    return rc;
}

static void hgsmiUnlock (HGSMIINSTANCE *pIns)
{
    int rc = RTCritSectLeave (&pIns->instanceCritSect);
    AssertRC (rc);
}

static int hgsmiFIFOLock (HGSMIINSTANCE *pIns)
{
    int rc = RTCritSectEnter (&pIns->hostFIFOCritSect);
    AssertRC (rc);
    return rc;
}

static void hgsmiFIFOUnlock (HGSMIINSTANCE *pIns)
{
    int rc = RTCritSectLeave (&pIns->hostFIFOCritSect);
    AssertRC (rc);
}

//static HGSMICHANNEL *hgsmiChannelFindById (PHGSMIINSTANCE pIns,
//                                           uint8_t u8Channel)
//{
//    HGSMICHANNEL *pChannel = &pIns->Channels[u8Channel];
//
//    if (pChannel->u8Flags & HGSMI_CH_F_REGISTERED)
//    {
//        return pChannel;
//    }
//
//    return NULL;
//}

#if 0
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
        }
    }
    else
    {
        LogFunc(("invalid data size 0x%x, maximum is 0x%x!!!\n", pHeader->u32DataSize, pArea->offLast - offBuffer));
    }

    LogFlowFunc(("returning NULL\n"));
    HGSMI_STRICT_ASSERT_FAILED();
    return NULL;
}

/*
 * Process a guest buffer.
 * @thread EMT
 */
static int hgsmiGuestBufferProcess (HGSMIINSTANCE *pIns,
                                    const HGSMICHANNEL *pChannel,
                                    const HGSMIBUFFERHEADER *pHeader)
{
    LogFlowFunc(("pIns %p, pChannel %p, pHeader %p\n", pIns, pChannel, pHeader));

    int rc = HGSMIChannelHandlerCall (pIns,
                                      &pChannel->handler,
                                      pHeader);

    return rc;
}
#endif
/*
 * Virtual hardware IO handlers.
 */

/* The guest submits a new buffer to the host.
 * Called from the HGSMI_IO_GUEST write handler.
 * @thread EMT
 */
void HGSMIGuestWrite (PHGSMIINSTANCE pIns,
                      HGSMIOFFSET offBuffer)
{
    HGSMIBufferProcess (&pIns->area, &pIns->channelInfo, offBuffer);
}

/* Called from HGSMI_IO_GUEST read handler. */
HGSMIOFFSET HGSMIGuestRead (PHGSMIINSTANCE pIns)
{
    LogFlowFunc(("pIns %p\n", pIns));

    AssertPtr(pIns);

    VM_ASSERT_EMT(pIns->pVM);

    /* Currently there is no functionality here. */
    NOREF(pIns);

    return HGSMIOFFSET_VOID;
}

static bool hgsmiProcessHostCmdCompletion (HGSMIINSTANCE *pIns,
                     HGSMIOFFSET offBuffer,
                     bool bCompleteFirst)
{
    VM_ASSERT_EMT(pIns->pVM);

    int rc = hgsmiFIFOLock(pIns);
    if(RT_SUCCESS(rc))
    {
        /* Search the Read list for the given buffer offset. Also find the previous entry. */
        HGSMIHOSTFIFOENTRY *pEntry = HGSMILISTENTRY_2_FIFOENTRY(pIns->hostFIFORead.pHead);
        HGSMIHOSTFIFOENTRY *pPrev = NULL;

        while (pEntry)
        {
            Assert(pEntry->fl == (HGSMI_F_HOST_FIFO_ALLOCATED | HGSMI_F_HOST_FIFO_READ));

            if (bCompleteFirst || pEntry->offBuffer == offBuffer)
            {
                break;
            }

#ifdef DEBUGVHWASTRICT
            /* guest usually completes commands in the order it receives it
             * if we're here this would typically means there is some cmd loss */
            Assert(0);
#endif

            pPrev = pEntry;
            pEntry = HGSMILISTENTRY_2_FIFOENTRY(pEntry->entry.pNext);
        }

        LogFlowFunc(("read list entry: %p.\n", pEntry));

        Assert(pEntry || bCompleteFirst);

        if (pEntry)
        {
            /* Exclude from the Read list. */
            hgsmiListRemove (&pIns->hostFIFORead, &pEntry->entry, pPrev? &pPrev->entry: NULL);

            pEntry->fl &= ~HGSMI_F_HOST_FIFO_READ;
            pEntry->fl |= HGSMI_F_HOST_FIFO_PROCESSED;

            /* Save in the Processed list. */
            hgsmiListAppend (&pIns->hostFIFOProcessed, &pEntry->entry);

            hgsmiFIFOUnlock(pIns);
#if 0
            /* Inform the submitter. */
            if (pEntry->pfnCallback)
            {
                pEntry->pfnCallback (pEntry->pvCallback);
            }
#else
            hgsmiHostCommandFreeCallback(pEntry);
#endif
            return true;
        }

        hgsmiFIFOUnlock(pIns);
        if(!bCompleteFirst)
            LogRel(("HGSMI[%s]: ignored invalid write to the host FIFO: 0x%08X!!!\n", pIns->pszName, offBuffer));
    }
    return false;
}

/* The the guest has finished processing of a buffer previously submitted by the host.
 * Called from HGSMI_IO_HOST write handler.
 * @thread EMT
 */
void HGSMIHostWrite (HGSMIINSTANCE *pIns,
                     HGSMIOFFSET offBuffer)
{
    LogFlowFunc(("pIns %p offBuffer 0x%x\n", pIns, offBuffer));

    hgsmiProcessHostCmdCompletion (pIns, offBuffer, false);
}

/* The guest reads a new host buffer to be processed.
 * Called from the HGSMI_IO_HOST read handler.
 * @thread EMT
 */
HGSMIOFFSET HGSMIHostRead (HGSMIINSTANCE *pIns)
{
    LogFlowFunc(("pIns %p\n", pIns));

    VM_ASSERT_EMT(pIns->pVM);

    int rc = hgsmiFIFOLock(pIns);
    Assert(RT_SUCCESS(rc));
    if(RT_SUCCESS(rc))
    {
        /* Get the host FIFO head entry. */
        HGSMIHOSTFIFOENTRY *pEntry = HGSMILISTENTRY_2_FIFOENTRY(pIns->hostFIFO.pHead);

        LogFlowFunc(("host FIFO head %p.\n", pEntry));

        if (pEntry != NULL)
        {
            Assert(pEntry->fl == (HGSMI_F_HOST_FIFO_ALLOCATED | HGSMI_F_HOST_FIFO_QUEUED));

            /* Exclude from the FIFO. */
            hgsmiListRemove (&pIns->hostFIFO, &pEntry->entry, NULL);

            if(!pIns->hostFIFO.pHead)
            {
                pIns->pHGFlags->u32HostFlags &= (~HGSMIHOSTFLAGS_COMMANDS_PENDING);
            }

            pEntry->fl &= ~HGSMI_F_HOST_FIFO_QUEUED;
            pEntry->fl |= HGSMI_F_HOST_FIFO_READ;

            /* Save in the Read list. */
            hgsmiListAppend (&pIns->hostFIFORead, &pEntry->entry);

            hgsmiFIFOUnlock(pIns);
            Assert(pEntry->offBuffer != HGSMIOFFSET_VOID);
            /* Return the buffer offset of the host FIFO head. */
            return pEntry->offBuffer;
        }
        hgsmiFIFOUnlock(pIns);
    }
    /* Special value that means there is no host buffers to be processed. */
    return HGSMIOFFSET_VOID;
}


/* Tells the guest that a new buffer to be processed is available from the host. */
static void hgsmiNotifyGuest (HGSMIINSTANCE *pIns)
{
    if (pIns->pfnNotifyGuest)
    {
//        pIns->pHGFlags->u32HostFlags |= HGSMIHOSTFLAGS_IRQ;
        pIns->pfnNotifyGuest (pIns->pvNotifyGuest);
    }
}

void HGSMISetHostGuestFlags(HGSMIINSTANCE *pIns, uint32_t flags)
{
    pIns->pHGFlags->u32HostFlags |= flags;
}

void HGSMIClearHostGuestFlags(HGSMIINSTANCE *pIns, uint32_t flags)
{
    pIns->pHGFlags->u32HostFlags &= (~flags);
}

#if 0
static void hgsmiRaiseEvent (const HGSMIHOSTFIFOENTRY *pEntry)
{
    int rc = RTSemEventMultiSignal (pEntry->hEvent);
    AssertRC(rc);
}

static int hgsmiWaitEvent (const HGSMIHOSTFIFOENTRY *pEntry)
{
    int rc = RTSemEventMultiWait (pEntry->hEvent, RT_INDEFINITE_WAIT);
    AssertRC(rc);
    return rc;
}
#endif

#if 0
DECLINLINE(HGSMIOFFSET) hgsmiMemoryOffset (const HGSMIINSTANCE *pIns, const void *pv)
{
    Assert((uint8_t *)pv >= pIns->area.pu8Base);
    Assert((uint8_t *)pv < pIns->area.pu8Base + pIns->area.cbArea);
    return (HGSMIOFFSET)((uint8_t *)pv - pIns->area.pu8Base);
}
#endif
/*
 * The host heap.
 *
 * Uses the RTHeap implementation.
 *
 */
static int hgsmiHostHeapLock (HGSMIINSTANCE *pIns)
{
    int rc = RTCritSectEnter (&pIns->hostHeapCritSect);
    AssertRC (rc);
    return rc;
}

static void hgsmiHostHeapUnlock (HGSMIINSTANCE *pIns)
{
    int rc = RTCritSectLeave (&pIns->hostHeapCritSect);
    AssertRC (rc);
}

#if 0
static int hgsmiHostHeapAlloc (HGSMIINSTANCE *pIns, void **ppvMem, uint32_t cb)
{
    int rc = hgsmiHostHeapLock (pIns);

    if (RT_SUCCESS (rc))
    {
        if (pIns->hostHeap == NIL_RTHEAPSIMPLE)
        {
            rc = VERR_NOT_SUPPORTED;
        }
        else
        {
            /* A block structure: [header][data][tail].
             * 'header' and 'tail' is used to verify memory blocks.
             */
            uint32_t cbAlloc = HGSMIBufferRequiredSize (cb);

            void *pv = RTHeapSimpleAlloc (pIns->hostHeap, cbAlloc, 0);

            if (pv)
            {
                HGSMIBUFFERHEADER *pHdr = (HGSMIBUFFERHEADER *)pv;

                /* Store some information which will help to verify memory pointers. */
                pHdr->u32Signature   = HGSMI_MEM_SIGNATURE;
                pHdr->cb             = cb;
                pHdr->off            = hgsmiMemoryOffset (pIns, pv);
                pHdr->u32HdrVerifyer = HGSMI_MEM_VERIFYER_HDR (pHdr);

                /* Setup the tail. */
                HGSMIBUFFERTAIL *pTail = HGSMIBufferTail (pHdr);

                pTail->u32TailVerifyer = HGSMI_MEM_VERIFYER_TAIL (pHdr);

                *ppvMem = pv;
            }
            else
            {
                rc = VERR_NO_MEMORY;
            }
        }

        hgsmiHostHeapUnlock (pIns);
    }

    return rc;
}


static int hgsmiHostHeapCheckBlock (HGSMIINSTANCE *pIns, void *pvMem)
{
    int rc = hgsmiHostHeapLock (pIns);

    if (RT_SUCCESS (rc))
    {
        rc = hgsmiVerifyBuffer (pIns, pvMem);

        hgsmiHostHeapUnlock (pIns);
    }

    return rc;
}

static int hgsmiHostHeapFree (HGSMIINSTANCE *pIns, void *pvMem)
{
    int rc = hgsmiHostHeapLock (pIns);

    if (RT_SUCCESS (rc))
    {
        RTHeapSimpleFree (pIns->hostHeap, pvMem);

        hgsmiHostHeapUnlock (pIns);
    }

    return rc;
}

static int hgsmiCheckMemPtr (HGSMIINSTANCE *pIns, void *pvMem, HGSMIOFFSET *poffMem)
{
    int rc = hgsmiHostHeapCheckBlock (pIns, pvMem);

    if (RT_SUCCESS (rc))
    {
        *poffMem = hgsmiMemoryOffset (pIns, pvMem);
    }

    return rc;
}
#endif

static int hgsmiHostFIFOAlloc (HGSMIINSTANCE *pIns, HGSMIHOSTFIFOENTRY **ppEntry)
{
    int rc = VINF_SUCCESS;

    NOREF (pIns);

    HGSMIHOSTFIFOENTRY *pEntry = (HGSMIHOSTFIFOENTRY *)RTMemAllocZ (sizeof (HGSMIHOSTFIFOENTRY));

    if (pEntry)
    {
        pEntry->fl = HGSMI_F_HOST_FIFO_ALLOCATED;
#if 0
        rc = RTSemEventMultiCreate (&pEntry->hEvent);

        if (RT_FAILURE (rc))
        {
            RTMemFree (pEntry);
        }
#endif
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS (rc))
    {
        *ppEntry = pEntry;
    }

    return rc;
}

static void hgsmiHostFIFOFree (HGSMIINSTANCE *pIns, HGSMIHOSTFIFOENTRY *pEntry)
{
    NOREF (pIns);
#if 0
    if (pEntry->hEvent)
    {
        RTSemEventMultiDestroy (pEntry->hEvent);
    }
#endif
    RTMemFree (pEntry);
}

static int hgsmiHostCommandFreeByEntry (HGSMIHOSTFIFOENTRY *pEntry)
{
    HGSMIINSTANCE *pIns = pEntry->pIns;
    int rc = hgsmiFIFOLock (pIns);
    if(RT_SUCCESS(rc))
    {
        hgsmiListRemove (&pIns->hostFIFOProcessed, &pEntry->entry, NULL);
        hgsmiFIFOUnlock (pIns);

        void *pvMem = HGSMIBufferDataFromOffset(&pIns->area, pEntry->offBuffer);

        rc = hgsmiHostHeapLock (pIns);
        if(RT_SUCCESS(rc))
        {
            /* Deallocate the host heap memory. */
            HGSMIHeapFree (&pIns->hostHeap, pvMem);

            hgsmiHostHeapUnlock(pIns);
        }

        hgsmiHostFIFOFree (pIns, pEntry);
    }
    return rc;
}

static int hgsmiHostCommandFree (HGSMIINSTANCE *pIns,
                                                void *pvMem)
{
    HGSMIOFFSET offMem = HGSMIHeapBufferOffset (&pIns->hostHeap, pvMem);
    int rc = VINF_SUCCESS;
    if (offMem != HGSMIOFFSET_VOID)
    {
        rc = hgsmiFIFOLock (pIns);
        if(RT_SUCCESS(rc))
        {
            /* Search the Processed list for the given offMem. Also find the previous entry. */
            HGSMIHOSTFIFOENTRY *pEntry = HGSMILISTENTRY_2_FIFOENTRY(pIns->hostFIFOProcessed.pHead);
            HGSMIHOSTFIFOENTRY *pPrev = NULL;

            while (pEntry)
            {
                Assert(pEntry->fl == (HGSMI_F_HOST_FIFO_ALLOCATED | HGSMI_F_HOST_FIFO_PROCESSED));

                if (pEntry->offBuffer == offMem)
                {
                    break;
                }

                pPrev = pEntry;
                pEntry = HGSMILISTENTRY_2_FIFOENTRY(pEntry->entry.pNext);
            }

            if (pEntry)
            {
                /* Exclude from the Processed list. */
                hgsmiListRemove (&pIns->hostFIFOProcessed, &pEntry->entry, &pPrev->entry);
            }
            else
            {
                LogRel(("HGSMI[%s]: the host frees unprocessed FIFO entry: 0x%08X\n", pIns->pszName, offMem));
                AssertFailed ();
            }


            hgsmiFIFOUnlock (pIns);

            rc = hgsmiHostHeapLock (pIns);
            if(RT_SUCCESS(rc))
            {
                /* Deallocate the host heap memory. */
                HGSMIHeapFree (&pIns->hostHeap, pvMem);

                hgsmiHostHeapUnlock(pIns);
            }

            if(pEntry)
            {
                /* Deallocate the entry. */
                hgsmiHostFIFOFree (pIns, pEntry);
            }
        }

    }
    else
    {
        rc = VERR_INVALID_POINTER;
        LogRel(("HGSMI[%s]: the host frees invalid FIFO entry: %p\n", pIns->pszName, pvMem));
        AssertFailed ();
    }
    return rc;
}

#define HGSMI_SET_COMMAND_PROCESSED_STATE(_pe) \
{ \
    Assert((_pe)->entry.pNext == NULL); \
    Assert((_pe)->fl == (HGSMI_F_HOST_FIFO_ALLOCATED | HGSMI_F_HOST_FIFO_PROCESSED)); \
}

#if 0
static DECLCALLBACK(void) hgsmiHostCommandRaiseEventCallback (void *pvCallback)
{
    /* Guest has processed the command. */
    HGSMIHOSTFIFOENTRY *pEntry = (HGSMIHOSTFIFOENTRY *)pvCallback;

    HGSMI_SET_COMMAND_PROCESSED_STATE(pEntry);

    /* This is a simple callback, just signal the event. */
    hgsmiRaiseEvent (pEntry);
}
#endif

static DECLCALLBACK(void) hgsmiHostCommandFreeCallback (void *pvCallback)
{
    /* Guest has processed the command. */
    HGSMIHOSTFIFOENTRY *pEntry = (HGSMIHOSTFIFOENTRY *)pvCallback;

    HGSMI_SET_COMMAND_PROCESSED_STATE(pEntry);

    /* This is a simple callback, just signal the event. */
    hgsmiHostCommandFreeByEntry (pEntry);
}

static int hgsmiHostCommandWrite (HGSMIINSTANCE *pIns, HGSMIOFFSET offMem
#if 0
        , PFNHGSMIHOSTFIFOCALLBACK pfnCallback, void **ppvContext
#endif
        )
{
    HGSMIHOSTFIFOENTRY *pEntry;

    int rc = hgsmiHostFIFOAlloc (pIns, &pEntry);

    if (RT_SUCCESS (rc))
    {
        /* Initialize the new entry and add it to the FIFO. */
        pEntry->fl |= HGSMI_F_HOST_FIFO_QUEUED;

        pEntry->pIns = pIns;
        pEntry->offBuffer = offMem;
#if 0
        pEntry->pfnCallback = pfnCallback;
        pEntry->pvCallback = pEntry;
#endif

        rc = hgsmiFIFOLock(pIns);
        if (RT_SUCCESS (rc))
        {
            hgsmiListAppend (&pIns->hostFIFO, &pEntry->entry);
            pIns->pHGFlags->u32HostFlags |= HGSMIHOSTFLAGS_COMMANDS_PENDING;

            hgsmiFIFOUnlock(pIns);
#if 0
            *ppvContext = pEntry;
#endif
        }
        else
        {
            hgsmiHostFIFOFree(pIns, pEntry);
        }
    }

    return rc;
}


/**
 * Append the shared memory block to the FIFO, inform the guest.
 *
 * @param pIns       Pointer to HGSMI instance,
 * @param pv         The HC memory pointer to the information.
 * @param ppvContext Where to store a pointer, which will allow the caller
 *                   to wait for the command completion.
 * @param            bDoIrq specifies whether the guest interrupt should be generated,
 * i.e. in case the command is not urgent(e.g. some guest command completion notification that does not require post-processing)
 * the command could be posted without raising an irq.
 *
 * @thread EMT
 */
static int hgsmiHostCommandProcess (HGSMIINSTANCE *pIns, HGSMIOFFSET offBuffer,
#if 0
        PFNHGSMIHOSTFIFOCALLBACK pfnCallback, void **ppvContext,
#endif
        bool bDoIrq)
{
//    HGSMIOFFSET offMem;
//
//    int rc = hgsmiCheckMemPtr (pIns, pvMem, &offMem);
//
//    if (RT_SUCCESS (rc))
//    {
        /* Append the command to FIFO. */
        int rc = hgsmiHostCommandWrite (pIns, offBuffer
#if 0
                , pfnCallback, ppvContext
#endif
                );

        if (RT_SUCCESS (rc))
        {
            if(bDoIrq)
            {
                /* Now guest can read the FIFO, the notification is informational. */
                hgsmiNotifyGuest (pIns);
            }
        }
//    }
//    else
//    {
//        AssertFailed ();
//    }

    return rc;
}
#if 0
static void hgsmiWait (void *pvContext)
{
    HGSMIHOSTFIFOENTRY *pEntry = (HGSMIHOSTFIFOENTRY *)pvContext;

    for (;;)
    {
        hgsmiWaitEvent (pEntry);

        if (pEntry->fl & (HGSMI_F_HOST_FIFO_PROCESSED | HGSMI_F_HOST_FIFO_CANCELED))
        {
            return;
        }
    }
}
#endif
/**
 * Allocate a shared memory block. The host can write command/data to the memory.
 *
 * @param pIns   Pointer to HGSMI instance,
 * @param ppvMem Where to store the allocated memory pointer to data.
 * @param cbMem  How many bytes of data to allocate.
 */
int HGSMIHostCommandAlloc (HGSMIINSTANCE *pIns,
                           void **ppvMem,
                           HGSMISIZE cbMem,
                           uint8_t u8Channel,
                           uint16_t u16ChannelInfo)
{
    LogFlowFunc (("pIns = %p, cbMem = 0x%08X(%d)\n", pIns, cbMem, cbMem));

    int rc = hgsmiHostHeapLock (pIns);
    if(RT_SUCCESS(rc))
    {
        void *pvMem = HGSMIHeapAlloc (&pIns->hostHeap,
                                  cbMem,
                                  u8Channel,
                                  u16ChannelInfo);
        hgsmiHostHeapUnlock(pIns);

        if (pvMem)
        {
            *ppvMem = pvMem;
        }
        else
        {
            LogRel((0, "HGSMIHeapAlloc: HGSMIHeapAlloc failed\n"));
            rc = VERR_GENERAL_FAILURE;
        }
    }

    LogFlowFunc (("rc = %Rrc, pvMem = %p\n", rc, *ppvMem));

    return rc;
}

/**
 * Convenience function that allows posting the host command asynchronously
 * and make it freed on completion.
 * The caller does not get notified in any way on command complation,
 * on success return the pvMem buffer can not be used after being passed to this function
 *
 * @param pIns  Pointer to HGSMI instance,
 * @param pvMem The pointer returned by 'HGSMIHostCommandAlloc'.
 * @param bDoIrq specifies whether the guest interrupt should be generated,
 * i.e. in case the command is not urgent(e.g. some guest command completion notification that does not require post-processing)
 * the command could be posted without raising an irq.
 */
int HGSMIHostCommandProcessAndFreeAsynch (PHGSMIINSTANCE pIns,
                             void *pvMem,
                             bool bDoIrq)
{
    LogFlowFunc(("pIns = %p, pvMem = %p\n", pIns, pvMem));

    VM_ASSERT_OTHER_THREAD(pIns->pVM);

#if 0
    void *pvContext = NULL;
#endif

    HGSMIOFFSET offBuffer = HGSMIHeapBufferOffset (&pIns->hostHeap, pvMem);

//    /* Have to forward to EMT because FIFO processing is there. */
//    int rc = VMR3ReqCallVoid (pIns->pVM, &pReq, RT_INDEFINITE_WAIT,
//                              (PFNRT) hgsmiHostCommandProcess,
//                              3, pIns, offBuffer, &pvContext);

    int rc = hgsmiHostCommandProcess (pIns, offBuffer,
#if 0
            hgsmiHostCommandFreeCallback, &pvContext,
#endif
            bDoIrq);
    AssertRC (rc);

    LogFlowFunc(("rc = %Rrc\n", rc));

    return rc;
}
#if 0
/**
 * Submit the shared memory block to the guest.
 *
 * @param pIns  Pointer to HGSMI instance,
 * @param pvMem The pointer returned by 'HGSMIHostCommandAlloc'.
 */
int HGSMIHostCommandProcess (HGSMIINSTANCE *pIns,
                             void *pvMem)
{
    LogFlowFunc(("pIns = %p, pvMem = %p\n", pIns, pvMem));

    VM_ASSERT_OTHER_THREAD(pIns->pVM);

    void *pvContext = NULL;

    HGSMIOFFSET offBuffer = HGSMIHeapBufferOffset (&pIns->hostHeap, pvMem);

//    /* Have to forward to EMT because FIFO processing is there. */
//    int rc = VMR3ReqCallVoid (pIns->pVM, &pReq, RT_INDEFINITE_WAIT,
//                              (PFNRT) hgsmiHostCommandProcess,
//                              3, pIns, offBuffer, &pvContext);

    int rc = hgsmiHostCommandProcess (pIns, offBuffer,
#if 0
            hgsmiHostCommandRaiseEventCallback, &pvContext,
#endif
            true);
    AssertReleaseRC (rc);

    if (RT_SUCCESS (rc))
    {
        /* Wait for completion. */
        hgsmiWait (pvContext);
    }

    LogFlowFunc(("rc = %Rrc\n", rc));

    return rc;
}
#endif

/**
 * Free the shared memory block.
 *
 * @param pIns  Pointer to HGSMI instance,
 * @param pvMem The pointer returned by 'HGSMIHostCommandAlloc'.
 */
int HGSMIHostCommandFree (HGSMIINSTANCE *pIns,
                          void *pvMem)
{
    LogFlowFunc(("pIns = %p, pvMem = %p\n", pIns, pvMem));

    return hgsmiHostCommandFree (pIns, pvMem);
}

int HGSMISetupHostHeap (PHGSMIINSTANCE pIns,
                        HGSMIOFFSET    offHeap,
                        HGSMISIZE      cbHeap)
{
    LogFlowFunc(("pIns %p, offHeap 0x%08X, cbHeap = 0x%08X\n", pIns, offHeap, cbHeap));

    int rc = VINF_SUCCESS;

    Assert (pIns);

//    if (   offHeap >= pIns->cbMem
//        || cbHeap > pIns->cbMem
//        || offHeap + cbHeap > pIns->cbMem)
//    {
//        rc = VERR_INVALID_PARAMETER;
//    }
//    else
    {
        rc = hgsmiHostHeapLock (pIns);

        if (RT_SUCCESS (rc))
        {
            if (pIns->hostHeap.cRefs)
            {
                Assert(0);
                /* It is possible to change the heap only if there is no pending allocations. */
                rc = VERR_ACCESS_DENIED;
            }
            else
            {
                rc = HGSMIHeapSetup (&pIns->hostHeap,
                                        pIns->area.pu8Base+offHeap,
                                        cbHeap,
                                        offHeap);
            }

            hgsmiHostHeapUnlock (pIns);
        }
    }

    LogFlowFunc(("rc = %Rrc\n", rc));

    return rc;
}

static int hgsmiHostSaveFifoEntryLocked (HGSMIHOSTFIFOENTRY *pEntry, PSSMHANDLE pSSM)
{
    int rc = SSMR3PutU32 (pSSM, pEntry->fl); AssertRC(rc);
    rc = SSMR3PutU32 (pSSM, pEntry->offBuffer); AssertRC(rc);
    return rc;
}

static int hgsmiHostSaveFifoLocked (HGSMILIST * pFifo, PSSMHANDLE pSSM)
{
    VBOXHGSMI_SAVE_FIFOSTART(pSSM);
    uint32_t size = 0;
    for(HGSMILISTENTRY * pEntry = pFifo->pHead; pEntry; pEntry = pEntry->pNext)
    {
        ++size;
    }
    int rc = SSMR3PutU32 (pSSM, size); AssertRC(rc);

    for(HGSMILISTENTRY * pEntry = pFifo->pHead; pEntry; pEntry = pEntry->pNext)
    {
        HGSMIHOSTFIFOENTRY *pFifoEntry = HGSMILISTENTRY_2_FIFOENTRY(pEntry);
        rc = hgsmiHostSaveFifoEntryLocked (pFifoEntry, pSSM);  AssertRC(rc);
    }

    VBOXHGSMI_SAVE_FIFOSTOP(pSSM);

    return rc;
}

static int hgsmiHostLoadFifoEntryLocked (PHGSMIINSTANCE pIns, HGSMIHOSTFIFOENTRY **ppEntry, PSSMHANDLE pSSM)
{
    HGSMIHOSTFIFOENTRY *pEntry;
    int rc = hgsmiHostFIFOAlloc (pIns, &pEntry);  AssertRC(rc);
    if (RT_SUCCESS (rc))
    {
        uint32_t u32;
        pEntry->pIns = pIns;
        rc = SSMR3GetU32 (pSSM, &u32); AssertRC(rc);
        pEntry->fl = u32;
        rc = SSMR3GetU32 (pSSM, &pEntry->offBuffer); AssertRC(rc);
        *ppEntry = pEntry;
    }

    return rc;
}

static int hgsmiHostLoadFifoLocked (PHGSMIINSTANCE pIns, HGSMILIST * pFifo, PSSMHANDLE pSSM)
{
    VBOXHGSMI_LOAD_FIFOSTART(pSSM);

    uint32_t size;
    int rc = SSMR3GetU32 (pSSM, &size); AssertRC(rc);
    if(RT_SUCCESS(rc) && size)
    {
        for(uint32_t i = 0; i < size; ++i)
        {
            HGSMIHOSTFIFOENTRY *pFifoEntry = NULL;  /* initialized to shut up gcc */
            rc = hgsmiHostLoadFifoEntryLocked (pIns, &pFifoEntry, pSSM); AssertRC(rc);
            if (RT_SUCCESS (rc))
            {
                hgsmiListAppend (pFifo, &pFifoEntry->entry);
            }
        }
    }

    VBOXHGSMI_LOAD_FIFOSTOP(pSSM);

    return rc;
}

int HGSMIHostSaveStateExec (PHGSMIINSTANCE pIns, PSSMHANDLE pSSM)
{
    VBOXHGSMI_SAVE_START(pSSM);

    int rc;


    HGSMIOFFSET off = pIns->pHGFlags ? HGSMIPointerToOffset(&pIns->area, (const HGSMIBUFFERHEADER *)pIns->pHGFlags) : HGSMIOFFSET_VOID;
    rc = SSMR3PutU32 (pSSM, off); AssertRC(rc);

    off = HGSMIHeapHandleLocationOffset(&pIns->hostHeap);
    rc = SSMR3PutU32 (pSSM, off); AssertRC(rc);
    if(off != HGSMIOFFSET_VOID)
    {
        rc = SSMR3PutU32 (pSSM, HGSMIHeapOffset(&pIns->hostHeap)); AssertRC(rc);
        rc = SSMR3PutU32(pSSM, HGSMIHeapSize(&pIns->hostHeap)); AssertRC(rc);
        /* need save mem pointer to calculate offset on restore */
        rc = SSMR3PutU64 (pSSM, (uint64_t)pIns->area.pu8Base); AssertRC(rc);
        rc = hgsmiFIFOLock (pIns); AssertRC(rc);
        if(RT_SUCCESS(rc))
        {
            rc = hgsmiHostSaveFifoLocked (&pIns->hostFIFO, pSSM); AssertRC(rc);
            rc = hgsmiHostSaveFifoLocked (&pIns->hostFIFORead, pSSM); AssertRC(rc);
            rc = hgsmiHostSaveFifoLocked (&pIns->hostFIFOProcessed, pSSM); AssertRC(rc);

            hgsmiFIFOUnlock (pIns);
        }
    }

    VBOXHGSMI_SAVE_STOP(pSSM);

    return rc;
}

int HGSMIHostLoadStateExec (PHGSMIINSTANCE pIns, PSSMHANDLE pSSM, uint32_t u32Version)
{
    if(u32Version < 3)
        return VINF_SUCCESS;

    VBOXHGSMI_LOAD_START(pSSM);

    int rc;
    HGSMIOFFSET off;
    rc = SSMR3GetU32(pSSM, &off); AssertRC(rc);
    pIns->pHGFlags = (off != HGSMIOFFSET_VOID) ? (HGSMIHOSTFLAGS*)HGSMIOffsetToPointer (&pIns->area, off) : NULL;

    HGSMIHEAP hHeap = pIns->hostHeap;
    rc = SSMR3GetU32(pSSM, &off); AssertRC(rc);
    if(off != HGSMIOFFSET_VOID)
    {
        HGSMIOFFSET offHeap;
        uint32_t cbHeap;
        rc = SSMR3GetU32(pSSM, &offHeap); AssertRC(rc);
        rc = SSMR3GetU32(pSSM, &cbHeap); AssertRC(rc);
        uint64_t oldMem;
        rc = SSMR3GetU64(pSSM, &oldMem); AssertRC(rc);

        rc = hgsmiHostHeapLock (pIns); AssertRC(rc);
        if (RT_SUCCESS (rc))
        {
            Assert(!pIns->hostHeap.cRefs);
            pIns->hostHeap.cRefs = 0;
            rc = HGSMIHeapRelocate(&pIns->hostHeap,
                                    pIns->area.pu8Base+offHeap,
                                    off,
                                    uintptr_t(pIns->area.pu8Base) - uintptr_t(oldMem),
                                    cbHeap,
                                    offHeap);

            hgsmiHostHeapUnlock (pIns);
        }


        rc = hgsmiFIFOLock (pIns); AssertRC(rc);
        if(RT_SUCCESS(rc))
        {
            rc = hgsmiHostLoadFifoLocked (pIns, &pIns->hostFIFO, pSSM); AssertRC(rc);
            rc = hgsmiHostLoadFifoLocked (pIns, &pIns->hostFIFORead, pSSM); AssertRC(rc);
            rc = hgsmiHostLoadFifoLocked (pIns, &pIns->hostFIFOProcessed, pSSM); AssertRC(rc);

            hgsmiFIFOUnlock (pIns);
        }
    }

    VBOXHGSMI_LOAD_STOP(pSSM);

    return rc;
}

/*
 * Channels management.
 */

static int hgsmiChannelMapCreate (PHGSMIINSTANCE pIns,
                                  const char *pszChannel,
                                  uint8_t *pu8Channel)
{
    /* @todo later */
    return VERR_NOT_SUPPORTED;
}

/* Register a new HGSMI channel by a predefined index.
 */
int HGSMIHostChannelRegister (PHGSMIINSTANCE pIns,
                          uint8_t u8Channel,
                          PFNHGSMICHANNELHANDLER pfnChannelHandler,
                          void *pvChannelHandler,
                          HGSMICHANNELHANDLER *pOldHandler)
{
    LogFlowFunc(("pIns %p, u8Channel %x, pfnChannelHandler %p, pvChannelHandler %p, pOldHandler %p\n",
                  pIns, u8Channel, pfnChannelHandler, pvChannelHandler, pOldHandler));

    AssertReturn(!HGSMI_IS_DYNAMIC_CHANNEL(u8Channel), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pIns, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfnChannelHandler, VERR_INVALID_PARAMETER);

    int rc = hgsmiLock (pIns);

    if (RT_SUCCESS (rc))
    {
        rc = HGSMIChannelRegister (&pIns->channelInfo, u8Channel, NULL, pfnChannelHandler, pvChannelHandler, pOldHandler);

        hgsmiUnlock (pIns);
    }

    LogFlowFunc(("leave rc = %Rrc\n", rc));

    return rc;
}

/* Register a new HGSMI channel by name.
 */
int HGSMIChannelRegisterName (PHGSMIINSTANCE pIns,
                              const char *pszChannel,
                              PFNHGSMICHANNELHANDLER pfnChannelHandler,
                              void *pvChannelHandler,
                              uint8_t *pu8Channel,
                              HGSMICHANNELHANDLER *pOldHandler)
{
    LogFlowFunc(("pIns %p, pszChannel %s, pfnChannelHandler %p, pvChannelHandler %p, pu8Channel %p, pOldHandler %p\n",
                  pIns, pszChannel, pfnChannelHandler, pvChannelHandler, pu8Channel, pOldHandler));

    AssertPtrReturn(pIns, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszChannel, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pu8Channel, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfnChannelHandler, VERR_INVALID_PARAMETER);

    int rc;

    /* The pointer to the copy will be saved in the channel description. */
    char *pszName = RTStrDup (pszChannel);

    if (pszName)
    {
        rc = hgsmiLock (pIns);

        if (RT_SUCCESS (rc))
        {
            rc = hgsmiChannelMapCreate (pIns, pszName, pu8Channel);

            if (RT_SUCCESS (rc))
            {
                rc = HGSMIChannelRegister (&pIns->channelInfo, *pu8Channel, pszName, pfnChannelHandler, pvChannelHandler, pOldHandler);
            }

            hgsmiUnlock (pIns);
        }

        if (RT_FAILURE (rc))
        {
            RTStrFree (pszName);
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    LogFlowFunc(("leave rc = %Rrc\n", rc));

    return rc;
}

#if 0
/* A wrapper to safely call the handler.
 */
int HGSMIChannelHandlerCall (PHGSMIINSTANCE pIns,
                             const HGSMICHANNELHANDLER *pHandler,
                             const HGSMIBUFFERHEADER *pHeader)
{
    LogFlowFunc(("pHandler %p, pIns %p, pHeader %p\n", pHandler, pIns, pHeader));

    int rc;

    if (   pHandler
        && pHandler->pfnHandler)
    {
        void *pvBuffer = HGSMIBufferData (pHeader);
        HGSMISIZE cbBuffer = pHeader->u32DataSize;

        rc = pHandler->pfnHandler (pIns, pHandler->pvHandler, pHeader->u16ChannelInfo, pvBuffer, cbBuffer);
    }
    else
    {
        /* It is a NOOP case here. */
        rc = VINF_SUCCESS;
    }

    LogFlowFunc(("leave rc = %Rrc\n", rc));

    return rc;
}

#endif

void *HGSMIOffsetToPointerHost (PHGSMIINSTANCE pIns,
                                HGSMIOFFSET offBuffer)
{
    const HGSMIAREA *pArea = &pIns->area;

    if (   offBuffer < pArea->offBase
        || offBuffer > pArea->offLast)
    {
        LogFunc(("offset 0x%x is outside the area [0x%x;0x%x]!!!\n", offBuffer, pArea->offBase, pArea->offLast));
        return NULL;
    }

    return HGSMIOffsetToPointer (pArea, offBuffer);
}


HGSMIOFFSET HGSMIPointerToOffsetHost (PHGSMIINSTANCE pIns,
                                      const void *pv)
{
    const HGSMIAREA *pArea = &pIns->area;

    uintptr_t pBegin = (uintptr_t)pArea->pu8Base;
    uintptr_t pEnd = (uintptr_t)pArea->pu8Base + (pArea->cbArea - 1);
    uintptr_t p = (uintptr_t)pv;

    if (   p < pBegin
        || p > pEnd)
    {
        LogFunc(("pointer %p is outside the area [%p;%p]!!!\n", pv, pBegin, pEnd));
        return HGSMIOFFSET_VOID;
    }

    return HGSMIPointerToOffset (pArea, (HGSMIBUFFERHEADER *)pv);
}


void *HGSMIContext (PHGSMIINSTANCE pIns)
{
    uint8_t *p = (uint8_t *)pIns;
    return p + sizeof (HGSMIINSTANCE);
}

/* The guest submitted a buffer. */
static DECLCALLBACK(int) hgsmiChannelHandler (void *pvHandler, uint16_t u16ChannelInfo, void *pvBuffer, HGSMISIZE cbBuffer)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pvHandler %p, u16ChannelInfo %d, pvBuffer %p, cbBuffer %u\n",
            pvHandler, u16ChannelInfo, pvBuffer, cbBuffer));

    PHGSMIINSTANCE pIns = (PHGSMIINSTANCE)pvHandler;

    switch (u16ChannelInfo)
    {
        case HGSMI_CC_HOST_FLAGS_LOCATION:
        {
            if (cbBuffer < sizeof (HGSMIBUFFERLOCATION))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            HGSMIBUFFERLOCATION *pLoc = (HGSMIBUFFERLOCATION *)pvBuffer;
            if(pLoc->cbLocation != sizeof(HGSMIHOSTFLAGS))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            pIns->pHGFlags = (HGSMIHOSTFLAGS*)HGSMIOffsetToPointer (&pIns->area, pLoc->offLocation);
        } break;

        default:
            Log(("Unsupported HGSMI guest command %d!!!\n",
                 u16ChannelInfo));
            break;
    }

    return rc;
}

static HGSMICHANNELHANDLER sOldChannelHandler;

int HGSMICreate (PHGSMIINSTANCE *ppIns,
                 PVM             pVM,
                 const char     *pszName,
                 HGSMIOFFSET     offBase,
                 uint8_t        *pu8MemBase,
                 HGSMISIZE       cbMem,
                 PFNHGSMINOTIFYGUEST pfnNotifyGuest,
                 void           *pvNotifyGuest,
                 size_t         cbContext)
{
    LogFlowFunc(("ppIns = %p, pVM = %p, pszName = [%s], pu8MemBase = %p, cbMem = 0x%08X, offMemBase = 0x%08X, "
                 "pfnNotifyGuest = %p, pvNotifyGuest = %p, cbContext = %d\n",
                 ppIns,
                 pVM,
                 pszName,
                 pu8MemBase,
                 cbMem,
                 pfnNotifyGuest,
                 pvNotifyGuest,
                 cbContext
               ));

    AssertPtrReturn(ppIns, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pVM, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pu8MemBase, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    PHGSMIINSTANCE pIns = (PHGSMIINSTANCE)RTMemAllocZ (sizeof (HGSMIINSTANCE) + cbContext);

    if (!pIns)
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS (rc))
    {
        rc = HGSMIAreaInitialize (&pIns->area, pu8MemBase, cbMem, offBase);
    }

    if (RT_SUCCESS (rc))
    {
        rc = RTCritSectInit (&pIns->instanceCritSect);
    }

    if (RT_SUCCESS (rc))
    {
        rc = RTCritSectInit (&pIns->hostHeapCritSect);
    }

    if (RT_SUCCESS (rc))
    {
        rc = RTCritSectInit (&pIns->hostFIFOCritSect);
    }

    if (RT_SUCCESS (rc))
    {
        pIns->pVM            = pVM;

        pIns->pszName        = VALID_PTR(pszName)? pszName: "";

        HGSMIHeapSetupUnitialized (&pIns->hostHeap);

        pIns->pfnNotifyGuest = pfnNotifyGuest;
        pIns->pvNotifyGuest  = pvNotifyGuest;
    }

    rc = HGSMIHostChannelRegister (pIns,
								   HGSMI_CH_HGSMI,
								   hgsmiChannelHandler,
								   pIns,
								   &sOldChannelHandler);

    if (RT_SUCCESS (rc))
    {
        *ppIns = pIns;
    }
    else
    {
        HGSMIDestroy (pIns);
    }

    LogFlowFunc(("leave rc = %Rrc, pIns = %p\n", rc, pIns));

    return rc;
}

void HGSMIReset (PHGSMIINSTANCE pIns)
{
    if(pIns->pHGFlags)
    {
        /* treat the abandoned commands as read.. */
        while(HGSMIHostRead (pIns) != HGSMIOFFSET_VOID)  {}
    }

    /* .. and complete them */
    while(hgsmiProcessHostCmdCompletion (pIns, 0, true)) {}

    HGSMIHeapSetupUnitialized (&pIns->hostHeap);
}

void HGSMIDestroy (PHGSMIINSTANCE pIns)
{
    LogFlowFunc(("pIns = %p\n", pIns));

    if (pIns)
    {
        if (RTCritSectIsInitialized (&pIns->hostHeapCritSect))
        {
            RTCritSectDelete (&pIns->hostHeapCritSect);
        }

        if (RTCritSectIsInitialized (&pIns->instanceCritSect))
        {
            RTCritSectDelete (&pIns->instanceCritSect);
        }

        if (RTCritSectIsInitialized (&pIns->hostFIFOCritSect))
        {
            RTCritSectDelete (&pIns->hostFIFOCritSect);
        }

        memset (pIns, 0, sizeof (HGSMIINSTANCE));

        RTMemFree (pIns);
    }

    LogFlowFunc(("leave\n"));
}
