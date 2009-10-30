/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Windows Specific Code. Integration with IntNet/NetFlt
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
/*
 * Based in part on Microsoft DDK sample code for Ndis Intermediate Miniport passthru driver sample.
 * Copyright (c) 1993-1999, Microsoft Corporation
 */

#include "VBoxNetFltCommon-win.h"
#include <iprt/thread.h>

/** represents the job element of the job queue
 * see comments for JOB_QUEUE */
typedef struct _JOB
{
    /** link in the job queue */
    LIST_ENTRY ListEntry;
    /** job function to be executed */
    JOB_ROUTINE pRoutine;
    /** parameter to be passed to the job function */
    PVOID pContext;
    /** event that will be fired on job completion */
    KEVENT CompletionEvent;
    /** true if the job manager should use the completion even for completion indication, false-otherwise*/
    bool bUseCompletionEvent;
} JOB, *PJOB;

/**
 * represents the queue of jobs processed by the worker thred
 *
 * we use the thread to process tasks which are required to be done at passive level
 * our callbacks may be called at APC level by IntNet, there are some tasks that we can not create at APC,
 * e.g. thread creation. This is why we schedule such jobs to the worker thread working at passive level
 */
typedef struct _JOB_QUEUE
{
    /* jobs */
    LIST_ENTRY Jobs;
    /* we are using ExInterlocked..List functions to access the jobs list */
    KSPIN_LOCK Lock;
    /** this event is used to initiate a job worker thread kill */
    KEVENT KillEvent;
    /** this event is used to notify a worker thread that jobs are added to the queue */
    KEVENT NotifyEvent;
    /** worker thread */
    PKTHREAD pThread;
} JOB_QUEUE, *PJOB_QUEUE;

typedef struct _CREATE_INSTANCE_CONTEXT
{
#ifndef VBOXNETADP
    PNDIS_STRING pOurName;
    PNDIS_STRING pBindToName;
#else
    NDIS_HANDLE hMiniportAdapter;
    NDIS_HANDLE hWrapperConfigurationContext;
#endif
    NDIS_STATUS  Status;
}CREATE_INSTANCE_CONTEXT, *PCREATE_INSTANCE_CONTEXT;

/*contexts used for our jobs */
/* Attach context */
typedef struct _ATTACH_INFO
{
    PVBOXNETFLTINS pNetFltIf;
    PCREATE_INSTANCE_CONTEXT pCreateContext;
    bool fRediscovery;
    int Status;
}ATTACH_INFO, *PATTACH_INFO;

/* general worker context */
typedef struct _WORKER_INFO
{
    PVBOXNETFLTINS pNetFltIf;
    int Status;
}WORKER_INFO, *PWORKER_INFO;

/* idc initialization */
typedef struct _INIT_IDC_INFO
{
    JOB     Job;
    bool    bInitialized;
    volatile bool    bStop;
    volatile int rc;
    KEVENT hCompletionEvent;
}INIT_IDC_INFO, *PINIT_IDC_INFO;


/** globals */

/** global lock */
NDIS_SPIN_LOCK     g_GlobalLock;
/** global job queue. some operations are required to be done at passive level, e.g. thread creation, adapter bind/unbind initiation,
 * while IntNet typically calls us APC_LEVEL, so we just create a system thread in our DriverEntry and enqueue the jobs to that thread */
static JOB_QUEUE g_JobQueue;
/**
 * The (common) global data.
 */
static VBOXNETFLTGLOBALS g_VBoxNetFltGlobals;
volatile static bool g_bIdcInitialized;
INIT_IDC_INFO g_InitIdcInfo;

#ifdef VBOX_LOOPBACK_USEFLAGS
UINT g_fPacketDontLoopBack;
UINT g_fPacketIsLoopedBack;
#endif

#define LIST_ENTRY_2_JOB(pListEntry) \
    ( (PJOB)((uint8_t *)(pListEntry) - RT_OFFSETOF(JOB, ListEntry)) )

static int vboxNetFltWinAttachToInterface(PVBOXNETFLTINS pThis, void * pContext, bool fRediscovery);
static int vboxNetFltWinConnectIt(PVBOXNETFLTINS pThis);
static int vboxNetFltWinTryFiniIdc();
static void vboxNetFltWinFiniNetFltBase();
static int vboxNetFltWinInitNetFltBase();
static int vboxNetFltWinFiniNetFlt();
static int vboxNetFltWinStartInitIdcProbing();
static int vboxNetFltWinStopInitIdcProbing();

/** makes the current thread to sleep for the given number of miliseconds */
DECLHIDDEN(void) vboxNetFltWinSleep(ULONG milis)
{
    RTThreadSleep(milis);
}

/** wait for the given device to be dereferenced */
DECLHIDDEN(void) vboxNetFltWinWaitDereference(PADAPT_DEVICE pState)
{
#ifdef DEBUG
    uint64_t StartNanoTS = RTTimeSystemNanoTS();
    uint64_t CurNanoTS;
#endif
    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);

    while (ASMAtomicUoReadU32((volatile uint32_t *)&pState->cReferences))
    {
        vboxNetFltWinSleep(2);
#ifdef DEBUG
        CurNanoTS = RTTimeSystemNanoTS();
        if(CurNanoTS - StartNanoTS > 20000000)
        {
            DBGPRINT(("device not idle"));
            Assert(0);
//            break;
        }
#endif
    }
}

/**
 * mem functions
 */
/* allocates and zeroes the nonpaged memory of a given size */
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMemAlloc(PVOID* ppMemBuf, UINT cbLength)
{
    NDIS_STATUS fStatus = NdisAllocateMemoryWithTag(ppMemBuf, cbLength, MEM_TAG);
    if(fStatus == NDIS_STATUS_SUCCESS)
    {
        NdisZeroMemory(*ppMemBuf, cbLength);
    }
    return fStatus;
}

/* frees memory allocated with vboxNetFltWinMemAlloc */
DECLHIDDEN(void) vboxNetFltWinMemFree(PVOID pMemBuf)
{
    NdisFreeMemory(pMemBuf, 0, 0);
}

/* frees ndis buffers used on send/receive */
static VOID vboxNetFltWinFiniBuffers(PADAPT pAdapt)
{
    /* NOTE: don't check for NULL since NULL is a valid handle */
#ifndef VBOXNETADP
    NdisFreeBufferPool(pAdapt->hSendBufferPoolHandle);
#endif
#ifndef VBOX_NETFLT_ONDEMAND_BIND
    NdisFreeBufferPool(pAdapt->hRecvBufferPoolHandle);
#endif
}

/* initializes ndis buffers used on send/receive */
static NDIS_STATUS vboxNetFltWinInitBuffers(PADAPT pAdapt)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    do
    {
        /* NOTE: NULL is a valid handle !!! */
#ifndef VBOXNETADP
        NdisAllocateBufferPool(&Status,
                &pAdapt->hSendBufferPoolHandle,
                TX_BUFFER_POOL_SIZE);
        Assert(Status == NDIS_STATUS_SUCCESS);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            break;
        }
#endif

#ifndef VBOX_NETFLT_ONDEMAND_BIND
        /* NOTE: NULL is a valid handle !!! */
        NdisAllocateBufferPool(&Status,
                &pAdapt->hRecvBufferPoolHandle,
                RX_BUFFER_POOL_SIZE);
        Assert(Status == NDIS_STATUS_SUCCESS);
        if (Status != NDIS_STATUS_SUCCESS)
        {
#ifndef VBOXNETADP
            NdisFreeBufferPool(pAdapt->hSendBufferPoolHandle);
#endif
            break;
        }
#endif
    } while (FALSE);

    return Status;
}

/* initializes packet info pool and allocates the cSize packet infos for the pool */
static NDIS_STATUS vboxNetFltWinPpAllocatePacketInfoPool(PPACKET_INFO_POOL pPool, UINT cSize)
{
    UINT cbBufSize = sizeof(PACKET_INFO)*cSize;
    PACKET_INFO * pPacketInfos;
    NDIS_STATUS fStatus;
    UINT i;

    Assert(cSize > 0);

    INIT_INTERLOCKED_PACKET_QUEUE(&pPool->Queue);

    fStatus = vboxNetFltWinMemAlloc((PVOID*)&pPacketInfos, cbBufSize);

    if(fStatus == NDIS_STATUS_SUCCESS)
    {
        PPACKET_INFO pInfo;
        pPool->pBuffer = pPacketInfos;

        for(i = 0; i < cSize; i++)
        {
            pInfo = &pPacketInfos[i];
            vboxNetFltWinQuEnqueueTail(&pPool->Queue.Queue, pInfo);
            pInfo->pPool = pPool;
        }
    }
    else
    {
        Assert(0);
    }

    return fStatus;
}

/* frees the packet info pool */
VOID vboxNetFltWinPpFreePacketInfoPool(PPACKET_INFO_POOL pPool)
{
    vboxNetFltWinMemFree(pPool->pBuffer);

    FINI_INTERLOCKED_PACKET_QUEUE(&pPool->Queue)
}

/**
 * copies one string to another. in case the destination string size is not enough to hold the complete source string
 * does nothing and returns NDIS_STATUS_RESOURCES .
 */
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinCopyString(PNDIS_STRING pDst, PNDIS_STRING pSrc)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    if(pDst != pSrc)
    {
        if(pDst->MaximumLength < pSrc->Length)
        {
            Assert(0);
            Status = NDIS_STATUS_RESOURCES;
        }
        else
        {
            pDst->Length = pSrc->Length;

            if(pDst->Buffer != pSrc->Buffer)
            {
                NdisMoveMemory(pDst->Buffer, pSrc->Buffer, pSrc->Length);
            }
        }
    }
    return Status;
}

/************************************************************************************
 * PINTNETSG pSG manipulation functions
 ************************************************************************************/
static void vboxNetFltWinReinitSG(PINTNETSG pSG)
{
    pSG->pvOwnerData = NULL;
    pSG->pvUserData = NULL;
    pSG->pvUserData2 = NULL;
    pSG->cUsers = 1;
    pSG->fFlags = INTNETSG_FLAGS_TEMP;
    pSG->cSegsUsed = 0;
    pSG->cbTotal = 0;
}

static void vboxNetFltWinInitSG(PINTNETSG pSG, UINT cBufferCount)
{
    pSG->pvOwnerData = NULL;
    pSG->pvUserData = NULL;
    pSG->pvUserData2 = NULL;
    pSG->cUsers = 1;
    pSG->fFlags = INTNETSG_FLAGS_TEMP;
    pSG->cSegsAlloc = cBufferCount;
    pSG->cSegsUsed = 0;
    pSG->cbTotal = 0;
}

/* moves the contents of the given NDIS_BUFFER and all other buffers chained to it to the PINTNETSG
 * the PINTNETSG is expected to contain one segment whose bugger is large enough to maintain
 * the contents of the given NDIS_BUFFER and all other buffers chained to it */
static NDIS_STATUS vboxNetFltWinNdisBufferMoveToSG0(PNDIS_BUFFER pBuffer, PINTNETSG pSG)
{
    UINT cbSeg = 0;
    PINTNETSEG paSeg;
    uint8_t * ptr;
    PVOID pVirtualAddress;
    UINT cbCurrentLength;
    NDIS_STATUS fStatus = NDIS_STATUS_SUCCESS;

    Assert(pSG->cSegsAlloc == 1);

    paSeg = pSG->aSegs;
    ptr = (uint8_t*)paSeg->pv;
    paSeg->cb = 0;
    paSeg->Phys = NIL_RTHCPHYS;
    pSG->cbTotal = 0;

    Assert(paSeg->pv);

    while(pBuffer)
    {
        NdisQueryBufferSafe(
            pBuffer,
            &pVirtualAddress,
            &cbCurrentLength,
            NormalPagePriority);

        if(!pVirtualAddress)
        {
            fStatus = NDIS_STATUS_FAILURE;
            break;
        }

        pSG->cbTotal += cbCurrentLength;
        paSeg->cb += cbCurrentLength;
        NdisMoveMemory(ptr, pVirtualAddress, cbCurrentLength);
        ptr += cbCurrentLength;

        NdisGetNextBuffer(
                pBuffer,
                &pBuffer);
    }

    if(fStatus == NDIS_STATUS_SUCCESS)
    {
        pSG->cSegsUsed = 1;
        Assert(pSG->cbTotal ==  paSeg->cb);
    }
    return fStatus;
}

/* converts the PNDIS_BUFFER to PINTNETSG by making the PINTNETSG segments to point to the memory buffers the
 * ndis buffer(s) point to (as opposed to vboxNetFltWinNdisBufferMoveToSG0 which copies the memory from ndis buffers(s) to PINTNETSG) */
static NDIS_STATUS vboxNetFltWinNdisBuffersToSG(PNDIS_BUFFER pBuffer, PINTNETSG pSG)
{
    UINT cbSeg = 0;
    NDIS_STATUS fStatus = NDIS_STATUS_SUCCESS;
    PVOID pVirtualAddress;
    UINT cbCurrentLength;

    while(pBuffer)
    {
        NdisQueryBufferSafe(
                pBuffer,
                &pVirtualAddress,
                &cbCurrentLength,
                NormalPagePriority);

        if(!pVirtualAddress) {
                fStatus = NDIS_STATUS_FAILURE;
                break;
        }

        pSG->cbTotal += cbCurrentLength;
        pSG->aSegs[cbSeg].cb = cbCurrentLength;
        pSG->aSegs[cbSeg].pv = pVirtualAddress;
        pSG->aSegs[cbSeg].Phys = NIL_RTHCPHYS;
        cbSeg++;

        NdisGetNextBuffer(
                pBuffer,
                &pBuffer);
    }

    AssertFatal(cbSeg <= pSG->cSegsAlloc);

    if(fStatus == NDIS_STATUS_SUCCESS)
    {
        pSG->cSegsUsed = cbSeg;
    }

    return fStatus;
}

static void vboxNetFltWinDeleteSG(PINTNETSG pSG)
{
    vboxNetFltWinMemFree(pSG);
}

static PINTNETSG vboxNetFltWinCreateSG(uint32_t cSize)
{
    PINTNETSG pSG;
    NTSTATUS Status = vboxNetFltWinMemAlloc((PVOID*)&pSG, RT_OFFSETOF(INTNETSG, aSegs[cSize]));
    if(Status == STATUS_SUCCESS)
    {
        vboxNetFltWinInitSG(pSG, cSize);
        return pSG;
    }

    return NULL;
}

/************************************************************************************
 * packet queue functions
 ************************************************************************************/

#if !defined(VBOX_NETFLT_ONDEMAND_BIND) && !defined(VBOXNETADP)
static NDIS_STATUS vboxNetFltWinQuPostPacket(PADAPT pAdapt, PNDIS_PACKET pPacket, PINTNETSG pSG, uint32_t fFlags
# ifdef DEBUG_NETFLT_PACKETS
        , PNDIS_PACKET pTmpPacket
# endif
        )
{
    NDIS_STATUS fStatus;
    PNDIS_PACKET pMyPacket;
    bool bSrcHost = fFlags & PACKET_SRC_HOST;

    LogFlow(("posting packet back to driver stack..\n"));

    if(!pPacket)
    {
        /* INTNETSG was in the packet queue, create a new NdisPacket from INTNETSG*/
        pMyPacket = vboxNetFltWinNdisPacketFromSG(pAdapt, /* PADAPT */
                pSG, /* PINTNETSG */
                pSG, /* PVOID pBufToFree */
                bSrcHost, /* bool bToWire */
                false); /* bool bCopyMemory */

        Assert(pMyPacket);

        NDIS_SET_PACKET_STATUS(pMyPacket, NDIS_STATUS_SUCCESS);

        DBG_CHECK_PACKET_AND_SG(pMyPacket, pSG);

#ifdef DEBUG_NETFLT_PACKETS
        Assert(pTmpPacket);

        DBG_CHECK_PACKET_AND_SG(pTmpPacket, pSG);

        DBG_CHECK_PACKETS(pTmpPacket, pMyPacket);
#endif

        LogFlow(("non-ndis packet info, packet created (%p)\n", pMyPacket));
    }
    else
    {
        /* NDIS_PACKET was in the packet queue */
        DBG_CHECK_PACKET_AND_SG(pPacket, pSG);

        if(!(fFlags & PACKET_MINE))
        {
            /* the packet is the one that was passed to us in send/receive callback
             * According to the DDK, we can not post it further,
             * instead we should allocate our own packet.
             * So, allocate our own packet (pMyPacket) and copy the packet info there */
            if(bSrcHost)
            {
                fStatus = vboxNetFltWinPrepareSendPacket(pAdapt, pPacket, &pMyPacket/*, true*/);
                LogFlow(("packet from wire, packet created (%p)\n", pMyPacket));
            }
            else
            {
                fStatus = vboxNetFltWinPrepareRecvPacket(pAdapt, pPacket, &pMyPacket, false);
                LogFlow(("packet from wire, packet created (%p)\n", pMyPacket));
            }
        }
        else
        {
            /* the packet enqueued is ours, simply assign pMyPacket and zero pPacket */
            pMyPacket = pPacket;
            pPacket = NULL;
        }
        Assert(pMyPacket);
    }

    if (pMyPacket)
    {
        /* we have successfuly initialized our packet, post it to the host or to the wire */
        if(bSrcHost)
        {
#if defined(DEBUG_NETFLT_PACKETS) || !defined(VBOX_LOOPBACK_USEFLAGS)
            vboxNetFltWinLbPutSendPacket(pAdapt, pMyPacket, false /* bFromIntNet */);
#endif
            NdisSend(&fStatus, pAdapt->hBindingHandle, pMyPacket);

            if (fStatus != NDIS_STATUS_PENDING)
            {
#if defined(DEBUG_NETFLT_PACKETS) || !defined(VBOX_LOOPBACK_USEFLAGS)
                /* the status is NOT pending, complete the packet */
                bool bTmp = vboxNetFltWinLbRemoveSendPacket(pAdapt, pMyPacket);
                Assert(bTmp);
#endif
                if(pPacket)
                {
                    LogFlow(("status is not pending, completing packet (%p)\n", pPacket));
#ifndef WIN9X
                    NdisIMCopySendCompletePerPacketInfo (pPacket, pMyPacket);
#endif
                    NdisFreePacket(pMyPacket);
                }
                else
                {
                    /* should never be here since the PINTNETSG is stored only when the underlying miniport
                     * indicates NDIS_STATUS_RESOURCES, we should never have this when processing
                     * the "from-host" packets */
                    Assert(0);
                    LogFlow(("status is not pending, freeing myPacket (%p)\n", pMyPacket));
                    vboxNetFltWinFreeSGNdisPacket(pMyPacket, false);
                }
            }
        }
        else
        {
            NdisMIndicateReceivePacket(pAdapt->hMiniportHandle, &pMyPacket, 1);

            fStatus = NDIS_STATUS_PENDING;
            /* the packet receive completion is always indicated via MiniportReturnPacket */
        }
    }
    else
    {
        /*we failed to create our packet */
        Assert(0);
        fStatus = NDIS_STATUS_FAILURE;
    }

    return fStatus;
}
#endif

static bool vboxNetFltWinQuProcessInfo(PVBOXNETFLTINS pNetFltIf, PPACKET_QUEUE_WORKER pWorker, PPACKET_INFO pInfo)
{
    PNDIS_PACKET pPacket = NULL;
    PINTNETSG pSG = NULL;
    PADAPT pAdapt = PVBOXNETFLTINS_2_PADAPT(pNetFltIf);
    UINT fFlags;
    NDIS_STATUS Status;
#ifndef VBOXNETADP
    bool bSrcHost;
    bool bPending;
    bool bDropIt;
#endif
#ifdef DEBUG_NETFLT_PACKETS
    /* packet used for matching */
    PNDIS_PACKET pTmpPacket = NULL;
#endif

    fFlags = GET_FLAGS_FROM_INFO(pInfo);
#ifndef VBOXNETADP
    bSrcHost = (fFlags & PACKET_SRC_HOST) != 0;
#endif

    /* we first need to obtain the INTNETSG to be passed to intnet */

    /* the queue may contain two "types" of packets:
     * the NDIS_PACKET and the INTNETSG.
     * I.e. on send/receive we typically enqueue the NDIS_PACKET passed to us by ndis,
     * however in case our ProtocolReceive is called or the packet's status is set to NDIS_STSTUS_RESOURCES
     * in ProtocolReceivePacket, we must return the packet immediately on ProtocolReceive*** exit
     * In this case we allocate the INTNETSG, copy the ndis packet data there and enqueue it.
     * In this case the packet info flags has the PACKET_SG fag set
     *
     * Besides that the NDIS_PACKET contained in the queue could be either the one passed to us in our send/receive callback
     * or the one created by us. The latter is possible in case our ProtocolReceive callback is called and we call NdisTransferData
     * in this case we need to allocate the packet the data to be transfered to.
     * If the enqueued packet is the one allocated by us the PACKET_MINE flag is set
     * */
    if((fFlags & PACKET_SG) == 0)
    {
        /* we have NDIS_PACKET enqueued, we need to convert it to INTNETSG to be passed to intnet */
        PNDIS_BUFFER   pCurrentBuffer = NULL;
        UINT           cBufferCount;
        UINT           uBytesCopied = 0;
        UINT           cbPacketLength;

        pPacket = (PNDIS_PACKET)GET_PACKET_FROM_INFO(pInfo);
        LogFlow(("ndis packet info, packet (%p)\n", pPacket));

        LogFlow(("preparing pSG"));
        NdisQueryPacket(pPacket,
                                NULL,
                                &cBufferCount,
                                &pCurrentBuffer,
                                &cbPacketLength);


        Assert(cBufferCount);

        /* we can not allocate the INTNETSG on stack since in this case we may get stack overflow
         * somewhere outside of our driver (3 pages of system thread stack does not seem to be enough)
         *
         * since we have a "serialized" packet processing, i.e. all packets are being processed and passed
         * to intnet by this thread, we just use one previously allocated INTNETSG which is stored in PADAPT */
        pSG = pWorker->pSG;

        if(cBufferCount > pSG->cSegsAlloc)
        {
            pSG = vboxNetFltWinCreateSG(cBufferCount + 2);
            if(pSG)
            {
                vboxNetFltWinDeleteSG(pWorker->pSG);
                pWorker->pSG = pSG;
            }
            else
            {
                LogRel(("Failed to reallocate the pSG\n"));
            }
        }

        if(pSG)
        {
            /* reinitialize */
            vboxNetFltWinReinitSG(pSG);

            /* convert the ndis buffers to INTNETSG */
            Status = vboxNetFltWinNdisBuffersToSG(pCurrentBuffer, pSG);
            if(Status != NDIS_STATUS_SUCCESS)
            {
                pSG = NULL;
            }
            else
            {
                DBG_CHECK_PACKET_AND_SG(pPacket, pSG);
            }
        }
    }
    else
    {
        /* we have the INTNETSG enqueued. (see the above comment explaining why/when this may happen)
         * just use the INTNETSG to pass it to intnet */
#ifndef VBOX_NETFLT_ONDEMAND_BIND
 #ifndef VBOXNETADP
        /* the PINTNETSG is stored only when the underlying miniport
         * indicates NDIS_STATUS_RESOURCES, we should never have this when processing
         * the "from-host" packedts */
        Assert(!bSrcHost);
 #endif
#else
        /* we have both host and wire in ProtocolReceive */
#endif
        pSG = (PINTNETSG)GET_PACKET_FROM_INFO(pInfo);

        LogFlow(("not ndis packet info, pSG (%p)\n", pSG));
    }

#ifdef DEBUG_NETFLT_PACKETS
    if(!pPacket && !pTmpPacket)
    {
        /* create tmp packet that woud be used for matching */
        pTmpPacket = vboxNetFltWinNdisPacketFromSG(pAdapt, /* PADAPT */
                    pSG, /* PINTNETSG */
                    pSG, /* PVOID pBufToFree */
                    bSrcHost, /* bool bToWire */
                    true); /* bool bCopyMemory */

        NDIS_SET_PACKET_STATUS(pTmpPacket, NDIS_STATUS_SUCCESS);

        DBG_CHECK_PACKET_AND_SG(pTmpPacket, pSG);

        Assert(pTmpPacket);
    }
#endif
    do
    {
#ifndef VBOXNETADP
        /* the pSG was successfully initialized, post it to the netFlt*/
#ifndef VBOX_NETFLT_ONDEMAND_BIND
        bDropIt =
#endif
        pSG ? pNetFltIf->pSwitchPort->pfnRecv(pNetFltIf->pSwitchPort, pSG,
                    bSrcHost ? INTNETTRUNKDIR_HOST : INTNETTRUNKDIR_WIRE
                            )
              : false;
#else
        if(pSG)
        {
            pNetFltIf->pSwitchPort->pfnRecv(pNetFltIf->pSwitchPort, pSG, INTNETTRUNKDIR_HOST);
            STATISTIC_INCREASE(pAdapt->cTxSuccess);
        }
        else
        {
            STATISTIC_INCREASE(pAdapt->cTxError);
        }
#endif

#if !defined(VBOX_NETFLT_ONDEMAND_BIND) && !defined(VBOXNETADP)
        if(!bDropIt)
        {
            Status = vboxNetFltWinQuPostPacket(pAdapt, pPacket, pSG, fFlags
# ifdef DEBUG_NETFLT_PACKETS
                               , pTmpPacket
# endif
            );

            if(Status == NDIS_STATUS_PENDING)
            {
                /* we will process packet completion in the completion routine */
                bPending = true;
                break;
            }
        }
        else
#endif
        {
            Status = NDIS_STATUS_SUCCESS;
        }

        /* drop it */
        if(pPacket)
        {
            if(!(fFlags & PACKET_MINE))
            {
#if !defined(VBOX_NETFLT_ONDEMAND_BIND) && !defined(VBOXNETADP)
                /* complete the packets */
                if(fFlags & PACKET_SRC_HOST)
                {
#endif
#ifndef VBOX_NETFLT_ONDEMAND_BIND
/*                    NDIS_SET_PACKET_STATUS(pPacket, Status); */
                    NdisMSendComplete(pAdapt->hMiniportHandle, pPacket, Status);
#endif
#if !defined(VBOX_NETFLT_ONDEMAND_BIND) && !defined(VBOXNETADP)
                }
                else
                {
#endif
#ifndef VBOXNETADP
                    NdisReturnPackets(&pPacket, 1);
#endif
#if !defined(VBOX_NETFLT_ONDEMAND_BIND) && !defined(VBOXNETADP)
                }
#endif
            }
            else
            {
#ifndef VBOX_NETFLT_ONDEMAND_BIND
                Assert(!(fFlags & PACKET_SRC_HOST));
#endif
                vboxNetFltWinFreeSGNdisPacket(pPacket, true);
            }
        }
        else
        {
            Assert(pSG);
            vboxNetFltWinMemFree(pSG);
        }
#ifndef VBOXNETADP
        bPending = false;
#endif
    } while(0);

#ifdef DEBUG_NETFLT_PACKETS
    if(pTmpPacket)
    {
        vboxNetFltWinFreeSGNdisPacket(pTmpPacket, true);
    }
#endif

#ifndef VBOXNETADP
    return bPending;
#else
    return false;
#endif
}
/*
 * thread start function for the thread which processes the packets enqueued in our send and receive callbacks called by ndis
 *
 * ndis calls us at DISPATCH_LEVEL, while IntNet is using kernel functions which require Irql<DISPATCH_LEVEL
 * this is why we can not immediately post packets to IntNet from our sen/receive callbacks
 * instead we put the incoming packets to the queue and maintain the system thread running at passive level
 * which processes the queue and posts the packets to IntNet, and further to the host or to the wire.
 */
static VOID vboxNetFltWinQuPacketQueueWorkerThreadProc(PVBOXNETFLTINS pNetFltIf)
{
    bool fResume = true;
    NTSTATUS fStatus;
    PADAPT pAdapt = PVBOXNETFLTINS_2_PADAPT(pNetFltIf);
    PPACKET_QUEUE_WORKER pWorker = &pNetFltIf->u.s.PacketQueueWorker;

    /* two events we're waiting is "kill" and "notify" events
     * the former is used for the thread termination
     * the latter gets fired each time the packet is added to the queue */
    PVOID pEvents[] = {
      (PVOID) &pWorker->KillEvent,
      (PVOID) &pWorker->NotifyEvent,
      };

    while(fResume)
    {
        uint32_t cNumProcessed;
        uint32_t cNumPostedToHostWire;

        fStatus = KeWaitForMultipleObjects(2, pEvents, WaitAny, Executive, KernelMode, FALSE,
          NULL, NULL);
        if(!NT_SUCCESS(fStatus) || fStatus == STATUS_WAIT_0)
        {
            /* "kill" event was set
             * will process queued packets and exit */
            fResume = false;
        }

        LogFlow(("==> processing vboxNetFltWinQuPacketQueueWorkerThreadProc\n"));

        cNumProcessed = 0;
        cNumPostedToHostWire = 0;

        do
        {
            PPACKET_INFO pInfo;

#ifdef DEBUG_NETFLT_PACKETS
            /* packet used for matching */
            PNDIS_PACKET pTmpPacket = NULL;
#endif

            /*TODO: FIXME: !!! the better approach for performance would be to dequeue all packets at once
             * and then go through all dequeued packets
             * the same should be done for enqueue !!! */
            pInfo = vboxNetFltWinQuInterlockedDequeueHead(&pWorker->PacketQueue);

            if(!pInfo)
            {
                break;
            }

            LogFlow(("==> found info (%p)\n", pInfo));

            if(vboxNetFltWinQuProcessInfo(pNetFltIf, pWorker, pInfo))
            {
                cNumPostedToHostWire++;
            }

            vboxNetFltWinPpFreePacketInfo(pInfo);

            cNumProcessed++;
        } while(TRUE);

        if(cNumProcessed)
        {
            vboxNetFltWinDecReferenceNetFlt(pNetFltIf, cNumProcessed);

            Assert(cNumProcessed >= cNumPostedToHostWire);

            if(cNumProcessed != cNumPostedToHostWire)
            {
                vboxNetFltWinDecReferenceAdapt(pAdapt, cNumProcessed - cNumPostedToHostWire);
            }
        }
    }

    PsTerminateSystemThread(STATUS_SUCCESS);
}

/**
 * thread start function for the job processing thread
 *
 * see comments for PJOB_QUEUE
 */
static VOID vboxNetFltWinJobWorkerThreadProc(PJOB_QUEUE pQueue)
{
    bool fResume = true;
    NTSTATUS Status;

    PVOID pEvents[] = {
      (PVOID) &pQueue->KillEvent,
      (PVOID) &pQueue->NotifyEvent,
      };

    do
    {
        Status = KeWaitForMultipleObjects(2, pEvents, WaitAny, Executive, KernelMode, FALSE,
          NULL, NULL);
        Assert(NT_SUCCESS(Status));
        if(!NT_SUCCESS(Status) || Status == STATUS_WAIT_0)
        {
            /* will process queued jobs and exit */
            Assert(Status == STATUS_WAIT_0);
            fResume = false;
        }

        do
        {
            PLIST_ENTRY pJobEntry = ExInterlockedRemoveHeadList(&pQueue->Jobs, &pQueue->Lock);
            PJOB pJob;

            if(!pJobEntry)
                break;

            pJob = LIST_ENTRY_2_JOB(pJobEntry);

            Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
            pJob->pRoutine(pJob->pContext);
            Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

            if(pJob->bUseCompletionEvent)
            {
                KeSetEvent(&pJob->CompletionEvent, 1, FALSE);
            }
        } while(TRUE);
    } while(fResume);

    Assert(Status == STATUS_WAIT_0);

    PsTerminateSystemThread(STATUS_SUCCESS);
}

/**
 * enqueues the job to the job queue to be processed by the job worker thread
 * see comments for PJOB_QUEUE
 */
static VOID vboxNetFltWinJobEnqueueJob(PJOB_QUEUE pQueue, PJOB pJob, bool bEnqueueHead)
{
    if(bEnqueueHead)
    {
        ExInterlockedInsertHeadList(&pQueue->Jobs, &pJob->ListEntry, &pQueue->Lock);
    }
    else
    {
        ExInterlockedInsertTailList(&pQueue->Jobs, &pJob->ListEntry, &pQueue->Lock);
    }

    KeSetEvent(&pQueue->NotifyEvent, 1, FALSE);
}

DECLINLINE(VOID) vboxNetFltWinJobInit(PJOB pJob, JOB_ROUTINE pRoutine, PVOID pContext, bool bUseEvent)
{
    pJob->pRoutine = pRoutine;
    pJob->pContext = pContext;
    pJob->bUseCompletionEvent = bUseEvent;
    if(bUseEvent)
        KeInitializeEvent(&pJob->CompletionEvent, NotificationEvent, FALSE);
}

/**
 * enqueues the job to the job queue to be processed by the job worker thread and
 * blocks until the job is done
 * see comments for PJOB_QUEUE
 */
static VOID vboxNetFltWinJobSynchExec(PJOB_QUEUE pQueue, JOB_ROUTINE pRoutine, PVOID pContext)
{
    JOB Job;

    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);

    vboxNetFltWinJobInit(&Job, pRoutine, pContext, true);

    vboxNetFltWinJobEnqueueJob(pQueue, &Job, false);

    KeWaitForSingleObject(&Job.CompletionEvent, Executive, KernelMode, FALSE, NULL);
}

/**
 * enqueues the job to be processed by the job worker thread at passive level and
 * blocks until the job is done
 */
DECLHIDDEN(VOID) vboxNetFltWinJobSynchExecAtPassive(JOB_ROUTINE pRoutine, PVOID pContext)
{
    vboxNetFltWinJobSynchExec(&g_JobQueue, pRoutine, pContext);
}

/**
 * helper function used for system thread creation
 */
static NTSTATUS vboxNetFltWinQuCreateSystemThread(PKTHREAD * ppThread, PKSTART_ROUTINE  pStartRoutine, PVOID  pStartContext)
{
    NTSTATUS fStatus;
    HANDLE hThread;
    OBJECT_ATTRIBUTES fObjectAttributes;

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    InitializeObjectAttributes(&fObjectAttributes, NULL, OBJ_KERNEL_HANDLE,
                        NULL, NULL);

    fStatus = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS,
                        &fObjectAttributes, NULL, NULL,
                        (PKSTART_ROUTINE) pStartRoutine, pStartContext);
    if (!NT_SUCCESS(fStatus))
      return fStatus;

    ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, NULL,
                        KernelMode, (PVOID*) ppThread, NULL);
    ZwClose(hThread);
    return STATUS_SUCCESS;
}

/**
 * initialize the job queue
 * see comments for PJOB_QUEUE
 */
static NTSTATUS vboxNetFltWinJobInitQueue(PJOB_QUEUE pQueue)
{
    NTSTATUS fStatus;

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    NdisZeroMemory(pQueue, sizeof(JOB_QUEUE));

    KeInitializeEvent(&pQueue->KillEvent, NotificationEvent, FALSE);

    KeInitializeEvent(&pQueue->NotifyEvent, SynchronizationEvent, FALSE);

    InitializeListHead(&pQueue->Jobs);

    fStatus = vboxNetFltWinQuCreateSystemThread(&pQueue->pThread, (PKSTART_ROUTINE)vboxNetFltWinJobWorkerThreadProc, pQueue);
    if(fStatus != STATUS_SUCCESS)
    {
        pQueue->pThread = NULL;
    }
    else
    {
        Assert(pQueue->pThread);
    }

    return fStatus;
}

/**
 * deinitialize the job queue
 * see comments for PJOB_QUEUE
 */
static void vboxNetFltWinJobFiniQueue(PJOB_QUEUE pQueue)
{
    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    if(pQueue->pThread)
    {
        KeSetEvent(&pQueue->KillEvent, 0, FALSE);

        KeWaitForSingleObject(pQueue->pThread, Executive,
                            KernelMode, FALSE, NULL);
    }
}

/**
 * initializes the packet queue
 * */
DECLHIDDEN(NTSTATUS) vboxNetFltWinQuInitPacketQueue(PVBOXNETFLTINS pInstance)
{
    NTSTATUS Status;
    PPACKET_QUEUE_WORKER pWorker = &pInstance->u.s.PacketQueueWorker;

    AssertFatal(!pWorker->pSG);

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    KeInitializeEvent(&pWorker->KillEvent, NotificationEvent, FALSE);

    KeInitializeEvent(&pWorker->NotifyEvent, SynchronizationEvent, FALSE);

    INIT_INTERLOCKED_PACKET_QUEUE(&pWorker->PacketQueue);

    do
    {
    Status = vboxNetFltWinPpAllocatePacketInfoPool(&pWorker->PacketInfoPool, PACKET_INFO_POOL_SIZE);

    if(Status == NDIS_STATUS_SUCCESS)
    {
        pWorker->pSG = vboxNetFltWinCreateSG(PACKET_QUEUE_SG_SEGS_ALLOC);
        if(!pWorker->pSG)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        Status = vboxNetFltWinQuCreateSystemThread(&pWorker->pThread, (PKSTART_ROUTINE)vboxNetFltWinQuPacketQueueWorkerThreadProc, pInstance);
        if(Status != STATUS_SUCCESS)
        {
            vboxNetFltWinPpFreePacketInfoPool(&pWorker->PacketInfoPool);
            vboxNetFltWinMemFree(pWorker->pSG);
            pWorker->pSG = NULL;
            break;
        }
    }

    } while(0);

    return Status;
}

/*
 * deletes the packet queue
 */
DECLHIDDEN(void) vboxNetFltWinQuFiniPacketQueue(PVBOXNETFLTINS pInstance)
{
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    PINTNETSG pSG;
    PPACKET_QUEUE_WORKER pWorker = &pInstance->u.s.PacketQueueWorker;
    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);

//    Assert(pAdapt->pPacketQueueSG);

    /* using the pPacketQueueSG as an indicator that the packet queue is initialized */
    RTSpinlockAcquire((pInstance)->hSpinlock, &Tmp);
    if(pWorker->pSG)
    {
        pSG = pWorker->pSG;
        pWorker->pSG = NULL;
        RTSpinlockRelease((pInstance)->hSpinlock, &Tmp);
        KeSetEvent(&pWorker->KillEvent, 0, FALSE);

        KeWaitForSingleObject(pWorker->pThread, Executive,
                            KernelMode, FALSE, NULL);

        vboxNetFltWinPpFreePacketInfoPool(&pWorker->PacketInfoPool);

        vboxNetFltWinDeleteSG(pSG);

        FINI_INTERLOCKED_PACKET_QUEUE(&pWorker->PacketQueue);
    }
    else
    {
        RTSpinlockRelease((pInstance)->hSpinlock, &Tmp);
    }
}

/*
 * creates the INTNETSG containing one segment pointing to the buffer of size cbBufSize
 * the INTNETSG created should be cleaned with vboxNetFltWinMemFree
 */
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinAllocSG(UINT cbBufSize, PINTNETSG *ppSG)
{
    UINT cbBufferOffset;
    UINT cbMemSize;
    NDIS_STATUS Status;
    PINTNETSG pSG;

    /* allocation:
     * 1. SG_PACKET - with one aSegs pointing to
     * 2. buffer of cbPacketLength containing the entire packet */
    cbBufferOffset = sizeof(INTNETSG);
    /* make sure the buffer is aligned */
    if((cbBufferOffset & (sizeof(PVOID) - 1)) != 0)
    {
        cbBufferOffset += sizeof(PVOID);
        cbBufferOffset &= ~(sizeof(PVOID) - 1);
    }

    cbMemSize = cbBufferOffset + cbBufSize;
    Status = vboxNetFltWinMemAlloc((PVOID*)&pSG, cbMemSize);
    if(Status == NDIS_STATUS_SUCCESS)
    {
        vboxNetFltWinInitSG(pSG, 1);
        pSG->aSegs[0].pv = (uint8_t *)pSG + cbBufferOffset;
        pSG->aSegs[0].Phys = NIL_RTHCPHYS;
        pSG->aSegs[0].cb = cbBufSize;
        pSG->cbTotal = cbBufSize;
        pSG->cSegsUsed = 1;

        LogFlow(("pSG created  (%p)\n", pSG));

        *ppSG = pSG;
    }
    return Status;
}

/**
 * put the packet info to the queue
 */
DECLINLINE(void) vboxNetFltWinQuEnqueueInfo(PPACKET_QUEUE_WORKER pWorker, PPACKET_INFO pInfo)
{
    vboxNetFltWinQuInterlockedEnqueueTail(&pWorker->PacketQueue, pInfo);

    KeSetEvent(&pWorker->NotifyEvent, IO_NETWORK_INCREMENT, FALSE);
}


/**
 * puts the packet to the queue
 *
 * @return NDIST_STATUS_SUCCESS iff the packet was enqueued successfully
 * and error status otherwise.
 * NOTE: that the success status does NOT mean that the packet processing is completed, but only that it was enqueued successfully
 * the packet can be returned to the caller protocol/moniport only in case the bReleasePacket was set to true (in this case the copy of the packet was enqueued)
 * or if vboxNetFltWinQuEnqueuePacket failed, i.e. the packet was NOT enqueued
 */
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinQuEnqueuePacket(PVBOXNETFLTINS pInstance, PVOID pPacket, const UINT fPacketFlags)
{
    PPACKET_INFO pInfo;
    PPACKET_QUEUE_WORKER pWorker = &pInstance->u.s.PacketQueueWorker;
    NDIS_STATUS fStatus = NDIS_STATUS_SUCCESS;

    do
    {
        if(fPacketFlags & PACKET_COPY)
        {
            PNDIS_BUFFER   pBuffer = NULL;
            UINT       cBufferCount;
            UINT       uBytesCopied = 0;
            UINT       cbPacketLength;
            PINTNETSG pSG;

            /* the packet is Ndis packet */
            Assert(!(fPacketFlags & PACKET_SG));
            Assert(!(fPacketFlags & PACKET_MINE));

            NdisQueryPacket((PNDIS_PACKET)pPacket,
                    NULL,
                    &cBufferCount,
                    &pBuffer,
                    &cbPacketLength);


            Assert(cBufferCount);

            fStatus = vboxNetFltWinAllocSG(cbPacketLength, &pSG);
            if(fStatus != NDIS_STATUS_SUCCESS)
            {
                Assert(0);
                break;
            }

            pInfo = vboxNetFltWinPpAllocPacketInfo(&pWorker->PacketInfoPool);

            if(!pInfo)
            {
                Assert(0);
                /* TODO: what status to set? */
                fStatus = NDIS_STATUS_FAILURE;
                vboxNetFltWinMemFree(pSG);
                break;
            }

            Assert(pInfo->pPool);

            /* the packet we are queueing is SG, add PACKET_SG to flags */
            SET_FLAGS_TO_INFO(pInfo, fPacketFlags | PACKET_SG);
            SET_PACKET_TO_INFO(pInfo, pSG);

            fStatus = vboxNetFltWinNdisBufferMoveToSG0(pBuffer, pSG);
            if(fStatus != NDIS_STATUS_SUCCESS)
            {
                Assert(0);
                vboxNetFltWinPpFreePacketInfo(pInfo);
                vboxNetFltWinMemFree(pSG);
                break;
            }

            DBG_CHECK_PACKET_AND_SG((PNDIS_PACKET)pPacket, pSG);
        }
        else
        {
            pInfo = vboxNetFltWinPpAllocPacketInfo(&pWorker->PacketInfoPool);

            if(!pInfo)
            {
                Assert(0);
                /* TODO: what status to set? */
                fStatus = NDIS_STATUS_FAILURE;
                break;
            }

            Assert(pInfo->pPool);

            SET_FLAGS_TO_INFO(pInfo, fPacketFlags);
            SET_PACKET_TO_INFO(pInfo, pPacket);
        }

        vboxNetFltWinQuEnqueueInfo(pWorker, pInfo);

    } while(0);

    return fStatus;
}

#ifndef VBOX_NETFLT_ONDEMAND_BIND
/*
 * ioctl i/f
 */
static NTSTATUS
vboxNetFltWinPtDispatchIoctl(
    IN PDEVICE_OBJECT    pDeviceObject,
    IN PIO_STACK_LOCATION  pIrpStack)
{
#if 0
    ULONG CtlCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;
    NTSTATUS Status = STATUS_SUCCESS;
    int rc;

    switch(CtlCode)
    {

        case VBOXNETFLT_WIN_IOCTL_INIT:
            rc = vboxNetFltWinInitIdc();
            if(!RT_SUCCESS(rc))
            {
                Status = STATUS_UNSUCCESSFUL;
            }
            break;
        case VBOXNETFLT_WIN_IOCTL_FINI:
            /* we are finalizing during unload */
            /* TODO: FIXME: need to prevent driver unload that can occur in case IntNet is connected to us,
             * but we are  not bound to any adapters */
/*            rc = vboxNetFltWinTryFiniIdc();
            if(!RT_SUCCESS(rc))
            {
                Status = STATUS_UNSUCCESSFUL;
            }
            */
            break;

        default:
            Status = STATUS_NOT_SUPPORTED;
            break;
    }

    return Status;
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/*
Routine Description:

    Process IRPs sent to this device.

Arguments:

    DeviceObject - pointer to a device object
    Irp      - pointer to an I/O Request Packet

Return Value:

    NTSTATUS - STATUS_SUCCESS always - change this when adding
    real code to handle ioctls.

*/
DECLHIDDEN(NTSTATUS)
vboxNetFltWinPtDispatch(
    IN PDEVICE_OBJECT    pDeviceObject,
    IN PIRP              pIrp
    )
{
    PIO_STACK_LOCATION  pIrpStack;
    NTSTATUS            Status = STATUS_SUCCESS;

    LogFlow(("==>Pt Dispatch\n"));
    pIrpStack = IoGetCurrentIrpStackLocation(pIrp);


    switch (pIrpStack->MajorFunction)
    {
        case IRP_MJ_CREATE:
            break;

        case IRP_MJ_CLEANUP:
            break;

        case IRP_MJ_CLOSE:
            break;

        case IRP_MJ_DEVICE_CONTROL:
            Status = vboxNetFltWinPtDispatchIoctl(pDeviceObject, pIrpStack);
            break;
        default:
            break;
    }

    pIrp->IoStatus.Status = Status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    LogFlow(("<== Pt Dispatch\n"));

    return Status;

}
#endif

/*
 * netflt
 */
#ifndef VBOXNETADP
/*
 * NOTE! the routine is NOT re-enterable for the given pAdapt
 * the serialization is not implemented for performance reasons
 * since we are assuming the caller serializes the requests as IntNet does
 */
static NDIS_STATUS vboxNetFltWinSynchNdisRequest(PADAPT pAdapt, PNDIS_REQUEST pRequest)
{
    int rc;

    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);

    /* 1. serialize */
    rc = RTSemFastMutexRequest(pAdapt->hSynchRequestMutex); AssertRC(rc);
    if(RT_SUCCESS(rc))
    {
        NDIS_STATUS fRequestStatus = NDIS_STATUS_SUCCESS;

        /* 2. set pAdapt->pSynchRequest */
        Assert(!pAdapt->pSynchRequest);
        pAdapt->pSynchRequest = pRequest;

        /* 3. call NdisRequest */
        NdisRequest(&fRequestStatus, pAdapt->hBindingHandle, pRequest);

        if(fRequestStatus == NDIS_STATUS_PENDING)
        {
        /* 3.1 if pending wait and assign the resulting status */
            KeWaitForSingleObject(&pAdapt->hSynchCompletionEvent, Executive,
                            KernelMode, FALSE, NULL);

            fRequestStatus = pAdapt->fSynchCompletionStatus;
        }

        /* 4. clear the pAdapt->pSynchRequest */
        pAdapt->pSynchRequest = NULL;

        RTSemFastMutexRelease(pAdapt->hSynchRequestMutex); AssertRC(rc);
        return fRequestStatus;
    }
    return NDIS_STATUS_FAILURE;
}


DECLHIDDEN(NDIS_STATUS) vboxNetFltWinGetMacAddress(PADAPT pAdapt, PRTMAC pMac)
{
    NDIS_REQUEST request;
    NDIS_STATUS status;
    request.RequestType = NdisRequestQueryInformation;
    request.DATA.QUERY_INFORMATION.InformationBuffer = pMac;
    request.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(RTMAC);
    request.DATA.QUERY_INFORMATION.Oid = OID_802_3_CURRENT_ADDRESS;
    status = vboxNetFltWinSynchNdisRequest(pAdapt, &request);
    if(status != NDIS_STATUS_SUCCESS)
    {
        /* TODO */
        Assert(0);
    }

    return status;

}

DECLHIDDEN(NDIS_STATUS) vboxNetFltWinQueryPhysicalMedium(PADAPT pAdapt, NDIS_PHYSICAL_MEDIUM * pMedium)
{
    NDIS_REQUEST Request;
    NDIS_STATUS Status;
    Request.RequestType = NdisRequestQueryInformation;
    Request.DATA.QUERY_INFORMATION.InformationBuffer = pMedium;
    Request.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(NDIS_PHYSICAL_MEDIUM);
    Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_PHYSICAL_MEDIUM;
    Status = vboxNetFltWinSynchNdisRequest(pAdapt, &Request);
    if(Status != NDIS_STATUS_SUCCESS)
    {
        if(Status == NDIS_STATUS_NOT_SUPPORTED || Status == NDIS_STATUS_NOT_RECOGNIZED || Status == NDIS_STATUS_INVALID_OID)
        {
            Status = NDIS_STATUS_NOT_SUPPORTED;
        }
        else
        {
            LogRel(("OID_GEN_PHYSICAL_MEDIUM failed: Status (0x%x)", Status));
            Assert(0);
        }
    }
    return Status;
}

DECLHIDDEN(bool) vboxNetFltWinIsPromiscuous(PADAPT pAdapt)
{
    /** @todo r=bird: This is too slow and is probably returning the wrong
     *        information. What we're interested in is whether someone besides us
     *        has put the interface into promiscuous mode. */
    NDIS_REQUEST request;
    NDIS_STATUS status;
    ULONG filter;
    Assert(VBOXNETFLT_PROMISCUOUS_SUPPORTED(pAdapt));
    request.RequestType = NdisRequestQueryInformation;
    request.DATA.QUERY_INFORMATION.InformationBuffer = &filter;
    request.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(filter);
    request.DATA.QUERY_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    status = vboxNetFltWinSynchNdisRequest(pAdapt, &request);
    if(status != NDIS_STATUS_SUCCESS)
    {
        /* TODO */
        Assert(0);
        return false;
    }
    return (filter & NDIS_PACKET_TYPE_PROMISCUOUS) != 0;
}

DECLHIDDEN(NDIS_STATUS) vboxNetFltWinSetPromiscuous(PADAPT pAdapt, bool bYes)
{
    Assert(VBOXNETFLT_PROMISCUOUS_SUPPORTED(pAdapt));
    if(VBOXNETFLT_PROMISCUOUS_SUPPORTED(pAdapt))
    {
        NDIS_REQUEST Request;
        NDIS_STATUS fStatus;
        ULONG fFilter;
        ULONG fExpectedFilter;
        ULONG fOurFilter;
        Request.RequestType = NdisRequestQueryInformation;
        Request.DATA.QUERY_INFORMATION.InformationBuffer = &fFilter;
        Request.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(fFilter);
        Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
        fStatus = vboxNetFltWinSynchNdisRequest(pAdapt, &Request);
        if(fStatus != NDIS_STATUS_SUCCESS)
        {
            /* TODO: */
            Assert(0);
            return fStatus;
        }

        if(!pAdapt->bUpperProtSetFilterInitialized)
        {
            /* the cache was not initialized yet, initiate it with the current filter value */
            pAdapt->fUpperProtocolSetFilter = fFilter;
            pAdapt->bUpperProtSetFilterInitialized = true;
        }


        if(bYes)
        {
            fExpectedFilter = NDIS_PACKET_TYPE_PROMISCUOUS;
            fOurFilter = NDIS_PACKET_TYPE_PROMISCUOUS;
        }
        else
        {
            fExpectedFilter = pAdapt->fUpperProtocolSetFilter;
            fOurFilter = 0;
        }

        if(fExpectedFilter != fFilter)
        {
            Request.RequestType = NdisRequestSetInformation;
            Request.DATA.SET_INFORMATION.InformationBuffer = &fExpectedFilter;
            Request.DATA.SET_INFORMATION.InformationBufferLength = sizeof(fExpectedFilter);
            Request.DATA.SET_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
            fStatus = vboxNetFltWinSynchNdisRequest(pAdapt, &Request);
            if(fStatus != NDIS_STATUS_SUCCESS)
            {
                /* TODO */
                Assert(0);
                return fStatus;
            }
        }
        pAdapt->fOurSetFilter = fOurFilter;
        return fStatus;
    }
    return NDIS_STATUS_NOT_SUPPORTED;
}
#else /* if defined VBOXNETADP */

/**
 *  Generates a new unique MAC address based on our vendor ID
 */
DECLHIDDEN(void) vboxNetFltWinGenerateMACAddress(RTMAC *pMac)
{
    /* temporary use a time info */
    uint64_t NanoTS = RTTimeSystemNanoTS();
    pMac->au8[0] = (uint8_t)((VBOXNETADP_VENDOR_ID >> 16) & 0xff);
    pMac->au8[1] = (uint8_t)((VBOXNETADP_VENDOR_ID >> 8) & 0xff);
    pMac->au8[2] = (uint8_t)(VBOXNETADP_VENDOR_ID & 0xff);
    pMac->au8[3] = (uint8_t)(NanoTS & 0xff0000);
    pMac->au16[2] = (uint16_t)(NanoTS & 0xffff);
}

DECLHIDDEN(int) vboxNetFltWinMAC2NdisString(RTMAC *pMac, PNDIS_STRING pNdisString)
{
    static const char s_achDigits[17] = "0123456789abcdef";
    uint8_t u8;
    int i;
    PWSTR pString;

    /* validate parameters */
    AssertPtrReturn(pMac, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pNdisString, VERR_INVALID_PARAMETER);
    AssertReturn(pNdisString->MaximumLength >= 13*sizeof(pNdisString->Buffer[0]), VERR_INVALID_PARAMETER);

    pString = pNdisString->Buffer;

    for( i = 0; i < 6; i++)
    {
        u8 = pMac->au8[i];
        pString[ 0] = s_achDigits[(u8 >>  4) & 0xf];
        pString[ 1] = s_achDigits[(u8/*>>0*/)& 0xf];
        pString += 2;
    }

    pNdisString->Length = 12*sizeof(pNdisString->Buffer[0]);

    *pString = L'\0';

    return VINF_SUCCESS;
}

static int vboxNetFltWinWchar2Int(WCHAR c, uint8_t * pv)
{
    if(c >= L'A' && c <= L'F')
    {
        *pv = (c - L'A') + 10;
    }
    else if(c >= L'a' && c <= L'f')
    {
        *pv = (c - L'a') + 10;
    }
    else if(c >= L'0' && c <= L'9')
    {
        *pv = (c - L'0');
    }
    else
    {
        return VERR_INVALID_PARAMETER;
    }
    return VINF_SUCCESS;
}

DECLHIDDEN(int) vboxNetFltWinMACFromNdisString(RTMAC *pMac, PNDIS_STRING pNdisString)
{
    int i, rc;
    PWSTR pString;

    /* validate parameters */
    AssertPtrReturn(pMac, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pNdisString, VERR_INVALID_PARAMETER);
    AssertReturn(pNdisString->Length >= 12*sizeof(pNdisString->Buffer[0]), VERR_INVALID_PARAMETER);

    pString = pNdisString->Buffer;

    for(i = 0; i < 6; i++)
    {
        uint8_t v1, v2;
        rc = vboxNetFltWinWchar2Int(pString[0], &v1);
        if(RT_FAILURE(rc))
        {
            break;
        }

        rc = vboxNetFltWinWchar2Int(pString[1], &v2);
        if(RT_FAILURE(rc))
        {
            break;
        }

        pMac->au8[i] = (v1 << 4) | v2;

        pString += 2;
    }

    return rc;
}

#endif
/**
 * creates a NDIS_PACKET from the PINTNETSG
 */
#ifdef VBOX_NETFLT_ONDEMAND_BIND
/* TODO: the bToWire parameter seems to be unneeded here, remove them*/
#endif
DECLHIDDEN(PNDIS_PACKET) vboxNetFltWinNdisPacketFromSG(PADAPT pAdapt, PINTNETSG pSG, PVOID pBufToFree, bool bToWire, bool bCopyMemory)
{
    NDIS_STATUS fStatus;
    PNDIS_PACKET pPacket;

    Assert(pSG->cSegsUsed == 1);
    Assert(pSG->cbTotal == pSG->aSegs[0].cb);
    Assert(pSG->aSegs[0].pv);
    Assert(pSG->cbTotal >= sizeof(ETH_HEADER_SIZE));

#ifdef VBOX_NETFLT_ONDEMAND_BIND
    NdisAllocatePacket(&fStatus, &pPacket, pAdapt->hSendPacketPoolHandle);
#elif defined(VBOXNETADP)
    NdisAllocatePacket(&fStatus, &pPacket, pAdapt->hRecvPacketPoolHandle);
#else
    NdisAllocatePacket(&fStatus, &pPacket, bToWire ? pAdapt->hSendPacketPoolHandle : pAdapt->hRecvPacketPoolHandle);
#endif
    if(fStatus == NDIS_STATUS_SUCCESS)
    {
        PNDIS_BUFFER pBuffer;
        PVOID pMemBuf;

        /* @todo: generally we do not always need to zero-initialize the complete OOB data here, reinitialize only when/what we need,
         * however we DO need to reset the status for the packets we indicate via NdisMIndicateReceivePacket to avoid packet loss
         * in case the status contains NDIS_STATUS_RESOURCES */
        VBOXNETFLT_OOB_INIT(pPacket);

        if(bCopyMemory)
        {
            fStatus = vboxNetFltWinMemAlloc(&pMemBuf, pSG->cbTotal);
            if(fStatus == NDIS_STATUS_SUCCESS)
            {
                NdisMoveMemory(pMemBuf, pSG->aSegs[0].pv, pSG->cbTotal);
            }
            else
            {
                Assert(0);
                NdisFreePacket(pPacket);
                pPacket = NULL;
            }
        }
        else
        {
            pMemBuf = pSG->aSegs[0].pv;
        }
        if(fStatus == NDIS_STATUS_SUCCESS)
        {
#ifdef VBOX_NETFLT_ONDEMAND_BIND
            NdisAllocateBuffer(&fStatus, &pBuffer,
                    pAdapt->hSendBufferPoolHandle,
                    pMemBuf,
                    pSG->cbTotal);
#elif defined(VBOXNETADP)
            NdisAllocateBuffer(&fStatus, &pBuffer,
                    pAdapt->hRecvBufferPoolHandle,
                    pMemBuf,
                    pSG->cbTotal);
#else
            NdisAllocateBuffer(&fStatus, &pBuffer,
                    bToWire ? pAdapt->hSendBufferPoolHandle : pAdapt->hRecvBufferPoolHandle,
                    pMemBuf,
                    pSG->cbTotal);
#endif

            if(fStatus == NDIS_STATUS_SUCCESS)
            {
                NdisChainBufferAtBack(pPacket, pBuffer);

#ifndef VBOX_NETFLT_ONDEMAND_BIND
                if(bToWire)
#endif
                {
                    PSEND_RSVD        pSendRsvd;
                    pSendRsvd = (PSEND_RSVD)(pPacket->ProtocolReserved);
                    pSendRsvd->pOriginalPkt = NULL;
                    pSendRsvd->pBufToFree = pBufToFree;
#ifdef VBOX_LOOPBACK_USEFLAGS
                    /* set "don't loopback" flags */
                    NdisSetPacketFlags(pPacket, g_fPacketDontLoopBack);
#else
                    NdisSetPacketFlags(pPacket, 0);
#endif
                }
#ifndef VBOX_NETFLT_ONDEMAND_BIND
                else
                {
                    PRECV_RSVD      pRecvRsvd;
                    pRecvRsvd = (PRECV_RSVD)(pPacket->MiniportReserved);
                    pRecvRsvd->pOriginalPkt = NULL;
                    pRecvRsvd->pBufToFree = pBufToFree;

                    /* me must set the header size on receive */
                    NDIS_SET_PACKET_HEADER_SIZE(pPacket, ETH_HEADER_SIZE);
                    /* NdisAllocatePacket zero-initializes the OOB data,
                     * but keeps the packet flags, clean them here */
                    NdisSetPacketFlags(pPacket, 0);
                }
#endif
                /* TODO: set out of bound data */
            }
            else
            {
                Assert(0);
                if(bCopyMemory)
                {
                    vboxNetFltWinMemFree(pMemBuf);
                }
                NdisFreePacket(pPacket);
                pPacket = NULL;
            }
        }
        else
        {
            Assert(0);
            NdisFreePacket(pPacket);
            pPacket = NULL;
        }
    }
    else
    {
        pPacket = NULL;
    }

    DBG_CHECK_PACKET_AND_SG(pPacket, pSG);

    return pPacket;
}

/*
 * frees NDIS_PACKET creaed with vboxNetFltWinNdisPacketFromSG
 */
DECLHIDDEN(void) vboxNetFltWinFreeSGNdisPacket(PNDIS_PACKET pPacket, bool bFreeMem)
{
    UINT cBufCount;
    PNDIS_BUFFER pFirstBuffer;
    UINT uTotalPacketLength;
    PNDIS_BUFFER pBuffer;

    NdisQueryPacket(pPacket, NULL, &cBufCount, &pFirstBuffer, &uTotalPacketLength);

    Assert(cBufCount == 1);

    do
    {
        NdisUnchainBufferAtBack(pPacket, &pBuffer);
        if(pBuffer != NULL)
        {
            PVOID pMemBuf;
            UINT cbLength;

            NdisQueryBufferSafe(pBuffer, &pMemBuf, &cbLength, NormalPagePriority);
            NdisFreeBuffer(pBuffer);
            if(bFreeMem)
            {
                vboxNetFltWinMemFree(pMemBuf);
            }
        }
        else
        {
            break;
        }
    } while(true);

    NdisFreePacket(pPacket);
}

/*
 * Free all packet pools on the specified adapter.
 * @param pAdapt    - pointer to ADAPT structure
 */
static VOID
vboxNetFltWinPtFreeAllPacketPools(
    IN PADAPT                    pAdapt
    )
{
#ifndef VBOX_NETFLT_ONDEMAND_BIND
    if (pAdapt->hRecvPacketPoolHandle != NULL)
    {
        /*
         * Free the packet pool that is used to indicate receives
         */
        NdisFreePacketPool(pAdapt->hRecvPacketPoolHandle);

        pAdapt->hRecvPacketPoolHandle = NULL;
    }
#endif /* #ifndef VBOX_NETFLT_ONDEMAND_BIND*/
#ifndef VBOXNETADP
    if (pAdapt->hSendPacketPoolHandle != NULL)
    {

        /*
         *  Free the packet pool that is used to send packets below
         */

        NdisFreePacketPool(pAdapt->hSendPacketPoolHandle);

        pAdapt->hSendPacketPoolHandle = NULL;

    }
#endif
}
#if !defined(VBOX_NETFLT_ONDEMAND_BIND) && !defined(VBOXNETADP)
static void vboxNetFltWinAssociateMiniportProtocol()
{
    NdisIMAssociateMiniport(vboxNetFltWinMpGetHandle(), vboxNetFltWinPtGetHandle());
}
#endif

/*
 * NetFlt driver unload function
 */
DECLHIDDEN(VOID)
vboxNetFltWinUnload(
    IN PDRIVER_OBJECT        DriverObject
    )
{
    int rc;
    UNREFERENCED_PARAMETER(DriverObject);

    LogFlow(("vboxNetFltWinUnload: entered\n"));

    rc = vboxNetFltWinTryFiniIdc();
    if (RT_FAILURE(rc))
    {
        /* TODO: we can not prevent driver unload here */
        Assert(0);

        Log(("vboxNetFltWinTryFiniIdc - failed, busy.\n"));
    }

    vboxNetFltWinJobFiniQueue(&g_JobQueue);
#ifndef VBOXNETADP
    vboxNetFltWinPtDeregister();
#endif
#ifndef VBOX_NETFLT_ONDEMAND_BIND
    vboxNetFltWinMpDeregister();
#endif

    vboxNetFltWinFiniNetFltBase();
    /* don't use logging or any RT after de-init */

    NdisFreeSpinLock(&g_GlobalLock);
}

RT_C_DECLS_BEGIN

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT        DriverObject,
    IN PUNICODE_STRING       RegistryPath
    );

RT_C_DECLS_END
/*
 * First entry point to be called, when this driver is loaded.
 * Register with NDIS as an intermediate driver.
 * @return  STATUS_SUCCESS if all initialization is successful, STATUS_XXX
 *   error code if not.
 */
NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT        DriverObject,
    IN PUNICODE_STRING       RegistryPath
    )
{
    NDIS_STATUS                        Status = NDIS_STATUS_SUCCESS;
    int rc;

    NdisAllocateSpinLock(&g_GlobalLock);

    do
    {
#ifdef VBOX_LOOPBACK_USEFLAGS
        ULONG MjVersion;
        ULONG MnVersion;
#endif

#ifdef VBOX_NETFLT_ONDEMAND_BIND
        /* we are registering in the DriverEntry only when we are working as a protocol
         * since in this case our driver is loaded after the VBoxDrv*/
        rc = vboxNetFltWinInitNetFlt();
#else
        /* the idc registration is initiated via IOCTL since our driver
         * can be loaded when the VBoxDrv is not in case we are a Ndis IM driver */
        rc = vboxNetFltWinInitNetFltBase();
#endif
        if(!RT_SUCCESS(rc))
        {
            Status = NDIS_STATUS_FAILURE;
            break;
        }
#ifdef VBOX_LOOPBACK_USEFLAGS
        PsGetVersion(&MjVersion, &MnVersion,
          NULL, /* PULONG  BuildNumber  OPTIONAL */
          NULL /* PUNICODE_STRING  CSDVersion  OPTIONAL */
          );

        g_fPacketDontLoopBack = NDIS_FLAGS_DONT_LOOPBACK;

        if(MjVersion == 5 && MnVersion == 0)
        {
            /* this is Win2k*/
            g_fPacketDontLoopBack |= NDIS_FLAGS_SKIP_LOOPBACK_W2K;
        }

        g_fPacketIsLoopedBack = NDIS_FLAGS_IS_LOOPBACK_PACKET;
#endif

#ifndef VBOX_NETFLT_ONDEMAND_BIND
        Status = vboxNetFltWinMpRegister(DriverObject, RegistryPath);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            vboxNetFltWinFiniNetFlt();
            break;
        }
#endif
#ifndef VBOXNETADP
        Status = vboxNetFltWinPtRegister(DriverObject, RegistryPath);
        if (Status != NDIS_STATUS_SUCCESS)
        {
#ifndef VBOX_NETFLT_ONDEMAND_BIND
            vboxNetFltWinMpDeregister();
#endif
            vboxNetFltWinFiniNetFlt();
            break;
        }
#endif

        Status = vboxNetFltWinJobInitQueue(&g_JobQueue);
        if(Status != STATUS_SUCCESS)
        {
#ifndef VBOXNETADP
            vboxNetFltWinPtDeregister();
#endif
#ifndef VBOX_NETFLT_ONDEMAND_BIND
            vboxNetFltWinMpDeregister();
#endif
            vboxNetFltWinFiniNetFlt();
            break;
        }

        /* note: we do it after we initialize the Job Queue */
        vboxNetFltWinStartInitIdcProbing();

#if !defined(VBOX_NETFLT_ONDEMAND_BIND) && !defined(VBOXNETADP)
        vboxNetFltWinAssociateMiniportProtocol();
#endif
    } while (FALSE);

    return(Status);
}

#if !defined(VBOX_NETFLT_ONDEMAND_BIND) && !defined(VBOXNETADP)
/**
 * creates and initializes the packet to be sent to the underlying miniport given a packet posted to our miniport edge
 * according to DDK docs we must create our own packet rather than posting the one passed to us
 */
DECLHIDDEN(NDIS_STATUS)
vboxNetFltWinPrepareSendPacket(
    IN PADAPT             pAdapt,
    IN PNDIS_PACKET           pPacket,
    OUT PNDIS_PACKET         *ppMyPacket
    /*,    IN bool bNetFltActive */
    )
{
    NDIS_STATUS fStatus;

    NdisAllocatePacket(&fStatus,
                       ppMyPacket,
                       pAdapt->hSendPacketPoolHandle);

    if (fStatus == NDIS_STATUS_SUCCESS)
    {
        PSEND_RSVD        pSendRsvd;

        pSendRsvd = (PSEND_RSVD)((*ppMyPacket)->ProtocolReserved);
        pSendRsvd->pOriginalPkt = pPacket;
        pSendRsvd->pBufToFree = NULL;

        NDIS_PACKET_FIRST_NDIS_BUFFER(*ppMyPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(pPacket);
        NDIS_PACKET_LAST_NDIS_BUFFER(*ppMyPacket) = NDIS_PACKET_LAST_NDIS_BUFFER(pPacket);

        vboxNetFltWinCopyPacketInfoOnSend(*ppMyPacket, pPacket);

#ifdef VBOX_LOOPBACK_USEFLAGS
        NdisGetPacketFlags(*ppMyPacket) |= g_fPacketDontLoopBack;
#endif
    }
    else
    {
        *ppMyPacket = NULL;
    }

    return fStatus;
}

/**
 * creates and initializes the packet to be sent to the upperlying protocol given a packet indicated to our protocol edge
 * according to DDK docs we must create our own packet rather than posting the one passed to us
 */
DECLHIDDEN(NDIS_STATUS)
vboxNetFltWinPrepareRecvPacket(
    IN PADAPT            pAdapt,
    IN PNDIS_PACKET           pPacket,
    OUT PNDIS_PACKET        *ppMyPacket,
    IN bool bDpr
    )
{
    NDIS_STATUS         fStatus;

    /*
     * Get a packet off the pool and indicate that up
     */
    if(bDpr)
    {
        Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

        NdisDprAllocatePacket(&fStatus,
                               ppMyPacket,
                               pAdapt->hRecvPacketPoolHandle);
    }
    else
    {
        NdisAllocatePacket(&fStatus,
                               ppMyPacket,
                               pAdapt->hRecvPacketPoolHandle);
    }

    if (fStatus == NDIS_STATUS_SUCCESS)
    {
        PRECV_RSVD            pRecvRsvd;

        pRecvRsvd = (PRECV_RSVD)((*ppMyPacket)->MiniportReserved);
        pRecvRsvd->pOriginalPkt = pPacket;
        pRecvRsvd->pBufToFree = NULL;

        NDIS_PACKET_FIRST_NDIS_BUFFER(*ppMyPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(pPacket);
        NDIS_PACKET_LAST_NDIS_BUFFER(*ppMyPacket) = NDIS_PACKET_LAST_NDIS_BUFFER(pPacket);

        fStatus = vboxNetFltWinCopyPacketInfoOnRecv(*ppMyPacket, pPacket);
    }
    else
    {
        *ppMyPacket = NULL;
    }
    return fStatus;
}
#endif

/**
 * initializes the ADAPT (our context structure) and binds to the given adapter
 */
#if defined(VBOX_NETFLT_ONDEMAND_BIND)
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtInitBind(PADAPT pAdapt)
#elif defined(VBOXNETADP)
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtInitBind(PADAPT *ppAdapt, NDIS_HANDLE hMiniportAdapter, PNDIS_STRING pBindToMiniportName /* actually this is our miniport name*/, NDIS_HANDLE hWrapperConfigurationContext)
#else
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtInitBind(PADAPT *ppAdapt, PNDIS_STRING pOurMiniportName, PNDIS_STRING pBindToMiniportName)
#endif
{
    NDIS_STATUS Status;
    do
    {
#ifndef VBOX_NETFLT_ONDEMAND_BIND
        ANSI_STRING AnsiString;
        int rc;
        PVBOXNETFLTINS pInstance;
        USHORT cbAnsiName = pBindToMiniportName->Length;/* the lenght is is bytes ; *2 ;RtlUnicodeStringToAnsiSize(pBindToMiniportName)*/
        CREATE_INSTANCE_CONTEXT Context;
        RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;

# ifndef VBOXNETADP
        Context.pOurName = pOurMiniportName;
        Context.pBindToName = pBindToMiniportName;
# else
        Context.hMiniportAdapter = hMiniportAdapter;
        Context.hWrapperConfigurationContext = hWrapperConfigurationContext;
# endif
        Context.Status = NDIS_STATUS_SUCCESS;

        AnsiString.Buffer = 0; /* will be allocated by RtlUnicodeStringToAnsiString */
        AnsiString.Length = 0;
        AnsiString.MaximumLength = cbAnsiName;

        Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

        Status = RtlUnicodeStringToAnsiString(&AnsiString, pBindToMiniportName, true);

        if(Status != STATUS_SUCCESS)
        {
            break;
        }

        rc = vboxNetFltSearchCreateInstance(&g_VBoxNetFltGlobals, AnsiString.Buffer, &pInstance, &Context);
        RtlFreeAnsiString(&AnsiString);
        if(RT_FAILURE(rc))
        {
        	Assert(0);
      		Status = Context.Status != NDIS_STATUS_SUCCESS ? Context.Status : NDIS_STATUS_FAILURE;
            break;
        }

        Assert(pInstance);

        if(rc == VINF_ALREADY_INITIALIZED)
        {
            PADAPT pAdapt = PVBOXNETFLTINS_2_PADAPT(pInstance);
            /* the case when our adapter was unbound while IntNet was connected to it */
            /* the instance remains valid until intNet disconnects from it, we simply search and re-use it*/

            /* re-initialize PADAPT */
            rc = vboxNetFltWinAttachToInterface(pInstance, &Context, true);
            if(RT_FAILURE(rc))
            {
                Assert(0);
          		Status = Context.Status != NDIS_STATUS_SUCCESS ? Context.Status : NDIS_STATUS_FAILURE;
                /* release netflt */
                vboxNetFltRelease(pInstance, false);

                break;
            }
        }

        *ppAdapt = PVBOXNETFLTINS_2_PADAPT(pInstance);
#else
        Status = vboxNetFltWinPtAllocInitPADAPT(pAdapt);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            break;
        }

        Status = vboxNetFltWinPtDoBinding(pAdapt);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            vboxNetFltWinPtFiniPADAPT(pAdapt);
            break;
        }
#endif
    }while(FALSE);

    return Status;
}

/**
 * initializes the ADAPT
 */
#ifdef VBOX_NETFLT_ONDEMAND_BIND
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtAllocInitPADAPT(PADAPT pAdapt)
{
    NDIS_STATUS Status;

    do
    {
        Status = vboxNetFltWinPtInitPADAPT(pAdapt);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            break;
        }
        Status = NDIS_STATUS_SUCCESS;
    } while(0);

    return Status;
}

/**
 * unbinds from the adapter we are bound to and deinitializes the ADAPT
 */
static NDIS_STATUS vboxNetFltWinPtFiniUnbind(PADAPT pAdapt)
{
    NDIS_STATUS Status;

    LogFlow(("==> vboxNetFltWinPtFiniUnbind: Adapt %p\n", pAdapt));

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    do
    {
        Status = vboxNetFltWinPtDoUnbinding(pAdapt, true);
        if(Status != NDIS_STATUS_SUCCESS)
        {
            Assert(0);
            /* TODO: should we break ? */
            /* break; */
        }

        vboxNetFltWinPtFiniPADAPT(pAdapt);
    } while(0);
    LogFlow(("<== vboxNetFltWinPtFiniUnbind: Adapt %p\n", pAdapt));

    return Status;
}
#endif

/*
 * deinitializes the ADAPT
 */
DECLHIDDEN(VOID) vboxNetFltWinPtFiniPADAPT(PADAPT pAdapt)
{
#ifndef VBOXNETADP
    int rc;
#endif

    LogFlow(("<== vboxNetFltWinPtFiniPADAPT : pAdapt %p\n", pAdapt));

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
#ifndef VBOXNETADP
    if(pAdapt->DeviceName.Buffer)
    {
        vboxNetFltWinMemFree(pAdapt->DeviceName.Buffer);
    }


    FINI_INTERLOCKED_SINGLE_LIST(&pAdapt->TransferDataList);
# if defined(DEBUG_NETFLT_LOOPBACK) || !defined(VBOX_LOOPBACK_USEFLAGS)
    FINI_INTERLOCKED_SINGLE_LIST(&pAdapt->SendPacketQueue);
# endif
#endif

#ifndef VBOX_NETFLT_ONDEMAND_BIND
    /* moved to vboxNetFltWinDetachFromInterfaceWorker */
#else
    vboxNetFltWinQuFiniPacketQueue(pAdapt);
#endif

    vboxNetFltWinFiniBuffers(pAdapt);

    /*
     *    Free the memory here, if was not released earlier(by calling the HaltHandler)
     */
    vboxNetFltWinPtFreeAllPacketPools (pAdapt);
#ifndef VBOXNETADP
    rc = RTSemFastMutexDestroy(pAdapt->hSynchRequestMutex);  AssertRC(rc);
#endif

    LogFlow(("<== vboxNetFltWinPtFiniPADAPT : pAdapt %p\n", pAdapt));
}

DECLHIDDEN(VOID) vboxNetFltWinPtFiniPADAPT(PADAPT pAdapt);
#ifndef VBOXNETADP
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtInitPADAPT(IN  PADAPT pAdapt, IN PNDIS_STRING pOurDeviceName)
#else
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtInitPADAPT(IN  PADAPT pAdapt)
#endif
{
    NDIS_STATUS                     Status;
#ifndef VBOXNETADP
    int rc;
#endif
    BOOLEAN                         bCallFiniOnFail = FALSE;

    LogFlow(("==> vboxNetFltWinPtInitPADAPT : pAdapt %p\n", pAdapt));

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    do
    {
        NdisZeroMemory(pAdapt, sizeof(ADAPT));
#ifndef VBOXNETADP
        NdisInitializeEvent(&pAdapt->hEvent);

        KeInitializeEvent(&pAdapt->hSynchCompletionEvent, SynchronizationEvent, FALSE);

        /*
         * Allocate a packet pool for sends. We need this to pass sends down.
         * We cannot use the same packet descriptor that came down to our send
         * handler (see also NDIS 5.1 packet stacking).
         */
        NdisAllocatePacketPoolEx(&Status,
                                   &pAdapt->hSendPacketPoolHandle,
                                   MIN_PACKET_POOL_SIZE,
                                   MAX_PACKET_POOL_SIZE - MIN_PACKET_POOL_SIZE,
                                   sizeof(PT_RSVD));

        if (Status != NDIS_STATUS_SUCCESS)
        {
            pAdapt->hSendPacketPoolHandle = NULL;
            break;
        }
#else
#endif

        Status = vboxNetFltWinInitBuffers(pAdapt);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            break;
        }

        bCallFiniOnFail = TRUE;
#ifndef VBOXNETADP
        rc = RTSemFastMutexCreate(&pAdapt->hSynchRequestMutex);
        if(RT_FAILURE(rc))
        {
            Status = NDIS_STATUS_FAILURE;
            break;
        }
#endif

#ifndef VBOX_NETFLT_ONDEMAND_BIND
# ifndef VBOXNETADP
        Status = vboxNetFltWinMemAlloc((PVOID*)&pAdapt->DeviceName.Buffer, pOurDeviceName->Length);
        if(Status != NDIS_STATUS_SUCCESS)
        {
            Assert(0);
            pAdapt->DeviceName.Buffer = NULL;
            break;
        }
        pAdapt->DeviceName.MaximumLength = pOurDeviceName->Length;
        pAdapt->DeviceName.Length = 0;
        Status = vboxNetFltWinCopyString(&pAdapt->DeviceName, pOurDeviceName);
        if(Status != NDIS_STATUS_SUCCESS)
        {
            Assert(0);
            break;
        }
# endif

        /*
         * Allocate a packet pool for receives. We need this to indicate receives.
         * Same consideration as sends (see also NDIS 5.1 packet stacking).
         */
        NdisAllocatePacketPoolEx(&Status,
                                   &pAdapt->hRecvPacketPoolHandle,
                                   MIN_PACKET_POOL_SIZE,
                                   MAX_PACKET_POOL_SIZE - MIN_PACKET_POOL_SIZE,
                                   PROTOCOL_RESERVED_SIZE_IN_PACKET);

        if (Status != NDIS_STATUS_SUCCESS)
        {
            pAdapt->hRecvPacketPoolHandle = NULL;
            break;
        }
#ifndef VBOXNETADP
        NdisInitializeEvent(&pAdapt->MiniportInitEvent);
#endif
#endif
#ifndef VBOXNETADP
        pAdapt->PTState.PowerState = NdisDeviceStateD3;
        vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Deinitialized);

        INIT_INTERLOCKED_SINGLE_LIST(&pAdapt->TransferDataList);

# if defined(DEBUG_NETFLT_LOOPBACK) || !defined(VBOX_LOOPBACK_USEFLAGS)
        INIT_INTERLOCKED_SINGLE_LIST(&pAdapt->SendPacketQueue);
# endif
#endif
        /* TODO: do we need it here ?? */
        pAdapt->MPState.PowerState = NdisDeviceStateD3;
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);

#ifdef VBOX_NETFLT_ONDEMAND_BIND
        {
            PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
            rc = vboxNetFltWinConnectIt(pNetFlt);
            if(RT_FAILURE(rc))
            {
                Assert(0);
                Status = NDIS_STATUS_FAILURE;
                break;
            }
        }
#endif

        /* moved to vboxNetFltOsInitInstance */
    } while(0);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        if(bCallFiniOnFail)
        {
            vboxNetFltWinPtFiniPADAPT(pAdapt);
        }
    }

    LogFlow(("<== vboxNetFltWinPtInitPADAPT : pAdapt %p, Status %x\n", pAdapt, Status));

    return Status;
}

/**
 * match packets
 */
#define NEXT_LIST_ENTRY(_Entry) ((_Entry)->Flink)
#define PREV_LIST_ENTRY(_Entry) ((_Entry)->Blink)
#define FIRST_LIST_ENTRY NEXT_LIST_ENTRY
#define LAST_LIST_ENTRY PREV_LIST_ENTRY

#define MIN(_a, _b) ((_a) < (_b) ? (_a) : (_b))

#ifndef VBOXNETADP

#ifdef DEBUG_misha

RTMAC g_vboxNetFltWinVerifyMACBroadcast = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
RTMAC g_vboxNetFltWinVerifyMACGuest = {0x08, 0x00, 0x27, 0x01, 0x02, 0x03};

DECLHIDDEN(PRTNETETHERHDR) vboxNetFltWinGetEthHdr(PNDIS_PACKET pPacket)
{
    UINT cBufCount1;
    PNDIS_BUFFER pBuffer1;
    UINT uTotalPacketLength1;
    RTNETETHERHDR* pEth;
    UINT cbLength1 = 0;
    UINT i = 0;

    NdisQueryPacket(pPacket, NULL, &cBufCount1, &pBuffer1, &uTotalPacketLength1);

    Assert(pBuffer1);
    Assert(uTotalPacketLength1 >= ETH_HEADER_SIZE);
    if(uTotalPacketLength1 < ETH_HEADER_SIZE)
        return NULL;

    NdisQueryBufferSafe(pBuffer1, &pEth, &cbLength1, NormalPagePriority);
    Assert(cbLength1 >= ETH_HEADER_SIZE);
    if(cbLength1 < ETH_HEADER_SIZE)
        return NULL;

    return pEth;
}

DECLHIDDEN(PRTNETETHERHDR) vboxNetFltWinGetEthHdrSG(PINTNETSG pSG)
{
    Assert(pSG->cSegsUsed);
    Assert(pSG->cSegsAlloc >= pSG->cSegsUsed);
    Assert(pSG->aSegs[0].cb >= ETH_HEADER_SIZE);

    if(!pSG->cSegsUsed)
        return NULL;

    if(pSG->aSegs[0].cb < ETH_HEADER_SIZE)
        return NULL;

    return (PRTNETETHERHDR)pSG->aSegs[0].pv;
}

DECLHIDDEN(bool) vboxNetFltWinCheckMACs(PNDIS_PACKET pPacket, PRTMAC pDst, PRTMAC pSrc)
{
    PRTNETETHERHDR pHdr = vboxNetFltWinGetEthHdr(pPacket);
    Assert(pHdr);

    if(!pHdr)
        return false;

    if(pDst && memcmp(pDst, &pHdr->DstMac, sizeof(RTMAC)))
        return false;

    if(pSrc && memcmp(pSrc, &pHdr->SrcMac, sizeof(RTMAC)))
        return false;

    return true;
}

DECLHIDDEN(bool) vboxNetFltWinCheckMACsSG(PINTNETSG pSG, PRTMAC pDst, PRTMAC pSrc)
{
    PRTNETETHERHDR pHdr = vboxNetFltWinGetEthHdrSG(pSG);
    Assert(pHdr);

    if(!pHdr)
        return false;

    if(pDst && memcmp(pDst, &pHdr->DstMac, sizeof(RTMAC)))
        return false;

    if(pSrc && memcmp(pSrc, &pHdr->SrcMac, sizeof(RTMAC)))
        return false;

    return true;
}
#endif

# if !defined(VBOX_LOOPBACK_USEFLAGS) || defined(DEBUG_NETFLT_PACKETS)
/*
 * answers whether the two given packets match based on the packet length and the first cbMatch bytes of the packets
 * if cbMatch < 0 matches complete packets.
 */
DECLHIDDEN(bool) vboxNetFltWinMatchPackets(PNDIS_PACKET pPacket1, PNDIS_PACKET pPacket2, const INT cbMatch)
{
    UINT cBufCount1;
    PNDIS_BUFFER pBuffer1;
    UINT uTotalPacketLength1;
    uint8_t* pMemBuf1;
    UINT cbLength1 = 0;

    UINT cBufCount2;
    PNDIS_BUFFER pBuffer2;
    UINT uTotalPacketLength2;
    uint8_t* pMemBuf2;
    UINT cbLength2 = 0;
    bool bMatch = true;

#ifdef DEBUG_NETFLT_PACKETS
    bool bCompleteMatch = false;
#endif

    NdisQueryPacket(pPacket1, NULL, &cBufCount1, &pBuffer1, &uTotalPacketLength1);
    NdisQueryPacket(pPacket2, NULL, &cBufCount2, &pBuffer2, &uTotalPacketLength2);

    Assert(pBuffer1);
    Assert(pBuffer2);

    if(uTotalPacketLength1 != uTotalPacketLength2)
    {
        bMatch = false;
    }
    else
    {
        UINT ucbLength2Match = 0;
        UINT ucbMatch;
        if(cbMatch < 0 || (UINT)cbMatch > uTotalPacketLength1)
        {
            /* NOTE: assuming uTotalPacketLength1 == uTotalPacketLength2*/
            ucbMatch = uTotalPacketLength1;
#ifdef DEBUG_NETFLT_PACKETS
            bCompleteMatch = true;
#endif
        }
        else
        {
            ucbMatch = (UINT)cbMatch;
        }

        for(;;)
        {
            if(!cbLength1)
            {
                NdisQueryBufferSafe(pBuffer1, &pMemBuf1, &cbLength1, NormalPagePriority);
                NdisGetNextBuffer(pBuffer1, &pBuffer1);
            }
            else
            {
                Assert(pMemBuf1);
                Assert(ucbLength2Match);
                pMemBuf1 += ucbLength2Match;
            }

            if(!cbLength2)
            {
                NdisQueryBufferSafe(pBuffer2, &pMemBuf2, &cbLength2, NormalPagePriority);
                NdisGetNextBuffer(pBuffer2, &pBuffer2);
            }
            else
            {
                Assert(pMemBuf2);
                Assert(ucbLength2Match);
                pMemBuf2 += ucbLength2Match;
            }

            ucbLength2Match = MIN(ucbMatch, cbLength1);
            ucbLength2Match = MIN(ucbMatch, cbLength2);

            if(memcmp((PVOID*)pMemBuf1, (PVOID*)pMemBuf2, ucbLength2Match))
            {
                bMatch = false;
                break;
            }

            ucbMatch -= ucbLength2Match;
            if(!ucbMatch)
                break;

            cbLength1 -= ucbLength2Match;
            cbLength2 -= ucbLength2Match;
        }
    }

#ifdef DEBUG_NETFLT_PACKETS
    if(bMatch && !bCompleteMatch)
    {
        /* check that the packets fully match */
        DBG_CHECK_PACKETS(pPacket1, pPacket2);
    }
#endif

    return bMatch;
}

/*
 * answers whether the ndis packet and PINTNETSG match based on the packet length and the first cbMatch bytes of the packet and PINTNETSG
 * if cbMatch < 0 matches complete packets.
 */
DECLHIDDEN(bool) vboxNetFltWinMatchPacketAndSG(PNDIS_PACKET pPacket, PINTNETSG pSG, const INT cbMatch)
{
    UINT cBufCount1;
    PNDIS_BUFFER pBuffer1;
    UINT uTotalPacketLength1;
    uint8_t* pMemBuf1;
    UINT cbLength1 = 0;
    UINT uTotalPacketLength2 = pSG->cbTotal;
    uint8_t* pMemBuf2;
    UINT cbLength2 = 0;
    bool bMatch = true;
    bool bCompleteMatch = false;
    UINT i = 0;

    NdisQueryPacket(pPacket, NULL, &cBufCount1, &pBuffer1, &uTotalPacketLength1);

    Assert(pBuffer1);
    Assert(pSG->cSegsUsed);
    Assert(pSG->cSegsAlloc >= pSG->cSegsUsed);

    if(uTotalPacketLength1 != uTotalPacketLength2)
    {
        Assert(0);
        bMatch = false;
    }
    else
    {
        UINT ucbLength2Match = 0;
        UINT ucbMatch;

        if(cbMatch < 0 || (UINT)cbMatch > uTotalPacketLength1)
        {
            /* NOTE: assuming uTotalPacketLength1 == uTotalPacketLength2*/
            ucbMatch = uTotalPacketLength1;
            bCompleteMatch = true;
        }
        else
        {
            ucbMatch = (UINT)cbMatch;
        }

        for(;;)
        {
            if(!cbLength1)
            {
                NdisQueryBufferSafe(pBuffer1, &pMemBuf1, &cbLength1, NormalPagePriority);
                NdisGetNextBuffer(pBuffer1, &pBuffer1);
            }
            else
            {
                Assert(pMemBuf1);
                Assert(ucbLength2Match);
                pMemBuf1 += ucbLength2Match;
            }

            if(!cbLength2)
            {
                Assert(i < pSG->cSegsUsed);
                pMemBuf2 = (uint8_t*)pSG->aSegs[i].pv;
                cbLength2 = pSG->aSegs[i].cb;
                i++;
            }
            else
            {
                Assert(pMemBuf2);
                Assert(ucbLength2Match);
                pMemBuf2 += ucbLength2Match;
            }

            ucbLength2Match = MIN(ucbMatch, cbLength1);
            ucbLength2Match = MIN(ucbMatch, cbLength2);

            if(memcmp((PVOID*)pMemBuf1, (PVOID*)pMemBuf2, ucbLength2Match))
            {
                bMatch = false;
                Assert(0);
                break;
            }

            ucbMatch -= ucbLength2Match;
            if(!ucbMatch)
                break;

            cbLength1 -= ucbLength2Match;
            cbLength2 -= ucbLength2Match;
        }
    }

    if(bMatch && !bCompleteMatch)
    {
        /* check that the packets fully match */
        DBG_CHECK_PACKET_AND_SG(pPacket, pSG);
    }
    return bMatch;
}

#  if 0
/*
 * answers whether the two PINTNETSGs match based on the packet length and the first cbMatch bytes of the PINTNETSG
 * if cbMatch < 0 matches complete packets.
 */
static bool vboxNetFltWinMatchSGs(PINTNETSG pSG1, PINTNETSG pSG2, const INT cbMatch)
{
    UINT uTotalPacketLength1 = pSG1->cbTotal;
    PVOID pMemBuf1;
    UINT cbLength1 = 0;
    UINT i1 = 0;
    UINT uTotalPacketLength2 = pSG2->cbTotal;
    PVOID pMemBuf2;
    UINT cbLength2 = 0;

    bool bMatch = true;
    bool bCompleteMatch = false;
    UINT i2 = 0;

    Assert(pSG1->cSegsUsed);
    Assert(pSG2->cSegsUsed);
    Assert(pSG1->cSegsAlloc >= pSG1->cSegsUsed);
    Assert(pSG2->cSegsAlloc >= pSG2->cSegsUsed);

    if(uTotalPacketLength1 != uTotalPacketLength2)
    {
        Assert(0);
        bMatch = false;
    }
    else
    {
        UINT ucbMatch;
        if(cbMatch < 0 || (UINT)cbMatch > uTotalPacketLength1)
        {
            /* NOTE: assuming uTotalPacketLength1 == uTotalPacketLength2*/
            ucbMatch = uTotalPacketLength1;
            bCompleteMatch = true;
        }
        else
        {
            ucbMatch = (UINT)cbMatch;
        }

        do
        {
            UINT ucbLength2Match;
            if(!cbLength1)
            {
                Assert(i1 < pSG1->cSegsUsed);
                pMemBuf1 = pSG1->aSegs[i1].pv;
                cbLength1 = pSG1->aSegs[i1].cb;
                i1++;
            }

            if(!cbLength2)
            {
                Assert(i2 < pSG2->cSegsUsed);
                pMemBuf2 = pSG2->aSegs[i2].pv;
                cbLength2 = pSG2->aSegs[i2].cb;
                i2++;
            }

            ucbLength2Match = MIN(ucbMatch, cbLength1);
            ucbLength2Match = MIN(ucbMatch, cbLength2);

            if(memcmp(pMemBuf1, pMemBuf2, ucbLength2Match))
            {
                bMatch = false;
                Assert(0);
                break;
            }
            ucbMatch -= ucbLength2Match;
            cbLength1 -= ucbLength2Match;
            cbLength2 -= ucbLength2Match;
        } while (ucbMatch);
    }

    if(bMatch && !bCompleteMatch)
    {
        /* check that the packets fully match */
        DBG_CHECK_SGS(pSG1, pSG2);
    }
    return bMatch;
}
#  endif
# endif
#endif

static void vboxNetFltWinFiniNetFltBase()
{
    do
    {
        vboxNetFltDeleteGlobals(&g_VBoxNetFltGlobals);

        /*
         * Undo the work done during start (in reverse order).
         */
        memset(&g_VBoxNetFltGlobals, 0, sizeof(g_VBoxNetFltGlobals));

        RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
        RTLogDestroy(RTLogSetDefaultInstance(NULL));

        RTR0Term();
    } while (0);
}

static int vboxNetFltWinTryFiniIdc()
{
    int rc;

    vboxNetFltWinStopInitIdcProbing();

    if(g_bIdcInitialized)
    {
        rc = vboxNetFltTryDeleteIdc(&g_VBoxNetFltGlobals);
        if(RT_SUCCESS(rc))
        {
            g_bIdcInitialized = false;
        }
    }
    else
    {
        rc = VINF_SUCCESS;
    }
    return rc;

}

static int vboxNetFltWinFiniNetFlt()
{
    int rc = vboxNetFltWinTryFiniIdc();
    if(RT_SUCCESS(rc))
    {
        vboxNetFltWinFiniNetFltBase();
    }
    return rc;
}

/**
 * base netflt initialization
 */
static int vboxNetFltWinInitNetFltBase()
{
    int rc;

    do
    {
        Assert(!g_bIdcInitialized);

        rc = RTR0Init(0);
        if (!RT_SUCCESS(rc))
        {
            break;
        }

        memset(&g_VBoxNetFltGlobals, 0, sizeof(g_VBoxNetFltGlobals));
        rc = vboxNetFltInitGlobals(&g_VBoxNetFltGlobals);
        if (!RT_SUCCESS(rc))
        {
            RTR0Term();
            break;
        }
    }while(0);

    return rc;
}

/**
 * initialize IDC
 */
static int vboxNetFltWinInitIdc()
{
    int rc;

    do
    {
        if(g_bIdcInitialized)
        {
#ifdef VBOX_NETFLT_ONDEMAND_BIND
            Assert(0);
#endif
            rc = VINF_ALREADY_INITIALIZED;
            break;
        }

        /*
         * connect to the support driver.
         *
         * This will call back vboxNetFltOsOpenSupDrv (and maybe vboxNetFltOsCloseSupDrv)
         * for establishing the connect to the support driver.
         */
        rc = vboxNetFltInitIdc(&g_VBoxNetFltGlobals);
        if (!RT_SUCCESS(rc))
        {
            break;
        }

        g_bIdcInitialized = true;
    } while (0);

    return rc;
}

static void vboxNetFltWinInitIdcProbingWorker(PINIT_IDC_INFO pInitIdcInfo)
{
    int rc = vboxNetFltWinInitIdc();
    if(RT_FAILURE(rc))
    {
        bool bInterupted = ASMAtomicUoReadBool(&pInitIdcInfo->bStop);
        if(!bInterupted)
        {
            RTThreadSleep(1000); /* 1 s */
            bInterupted = ASMAtomicUoReadBool(&pInitIdcInfo->bStop);
            if(!bInterupted)
            {
                vboxNetFltWinJobEnqueueJob(&g_JobQueue, &pInitIdcInfo->Job, false);
                return;
            }
        }

        /* it's interupted */
        rc = VERR_INTERRUPTED;
    }

    ASMAtomicUoWriteU32(&pInitIdcInfo->rc, rc);
    KeSetEvent(&pInitIdcInfo->hCompletionEvent, 0, FALSE);
}

static int vboxNetFltWinStopInitIdcProbing()
{
    if(!g_InitIdcInfo.bInitialized)
        return VERR_INVALID_STATE;

    ASMAtomicUoWriteBool(&g_InitIdcInfo.bStop, true);
    KeWaitForSingleObject(&g_InitIdcInfo.hCompletionEvent, Executive, KernelMode, FALSE, NULL);

    return g_InitIdcInfo.rc;
}

static int vboxNetFltWinStartInitIdcProbing()
{
    Assert(!g_bIdcInitialized);
    KeInitializeEvent(&g_InitIdcInfo.hCompletionEvent, NotificationEvent, FALSE);
    g_InitIdcInfo.bStop = false;
    g_InitIdcInfo.bInitialized = true;
    vboxNetFltWinJobInit(&g_InitIdcInfo.Job, vboxNetFltWinInitIdcProbingWorker, &g_InitIdcInfo, false);
    vboxNetFltWinJobEnqueueJob(&g_JobQueue, &g_InitIdcInfo.Job, false);
    return VINF_SUCCESS;
}

static int vboxNetFltWinInitNetFlt()
{
    int rc;

    do
    {
        rc = vboxNetFltWinInitNetFltBase();
        if(RT_FAILURE(rc))
        {
            Assert(0);
            break;
        }

        /*
         * connect to the support driver.
         *
         * This will call back vboxNetFltOsOpenSupDrv (and maybe vboxNetFltOsCloseSupDrv)
         * for establishing the connect to the support driver.
         */
        rc = vboxNetFltWinInitIdc();
        if (RT_FAILURE(rc))
        {
            Assert(0);
            vboxNetFltWinFiniNetFltBase();
            break;
        }
    } while (0);

    return rc;
}

/* detach*/
static int vboxNetFltWinDeleteInstance(PVBOXNETFLTINS pThis)
{
    PADAPT pAdapt = PVBOXNETFLTINS_2_PADAPT(pThis);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    NDIS_STATUS Status;
    LogFlow(("vboxNetFltWinDeleteInstance: pThis=%p \n", pThis));

    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
    Assert(pAdapt);
    Assert(pThis);
    Assert(pThis->fDisconnectedFromHost);
    Assert(!pThis->fRediscoveryPending);
    Assert(!pThis->fActive);
#ifndef VBOXNETADP
    Assert(pAdapt->PTState.OpState == kVBoxNetDevOpState_Deinitialized);
    Assert(!pAdapt->hBindingHandle);
#endif
    Assert(pAdapt->MPState.OpState == kVBoxNetDevOpState_Deinitialized);
    Assert(!pThis->u.s.PacketQueueWorker.pSG);
//    Assert(!pAdapt->hMiniportHandle);

#ifndef VBOX_NETFLT_ONDEMAND_BIND
    Status = vboxNetFltWinMpDereferenceControlDevice();
    Assert(Status == NDIS_STATUS_SUCCESS);
#else
    Status = vboxNetFltWinPtFiniUnbind(pAdapt);
    if(Status != NDIS_STATUS_SUCCESS)
    {
        Assert(0);
        /* pDetachInfo->Status = VERR_GENERAL_FAILURE; */
    }
#endif

    RTSemMutexDestroy(pThis->u.s.hAdaptMutex);

    return VINF_SUCCESS;
}

static NDIS_STATUS vboxNetFltWinDisconnectIt(PVBOXNETFLTINS pInstance)
{
    vboxNetFltWinQuFiniPacketQueue(pInstance);
    return NDIS_STATUS_SUCCESS;
}

/* detach*/
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinDetachFromInterface(PADAPT pAdapt, bool bOnUnbind)
{
    PVBOXNETFLTINS pThis = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    NDIS_STATUS Status;
    int rc;
    LogFlow(("vboxNetFltWinDetachFromInterface: pThis=%p\n", pThis));

    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
    Assert(pAdapt);
    Assert(pThis);
/*    Assert(!pThis->fActive); */

    /* paranoya to ensyre the instance is not removed while we're waiting on the mutex
     * in case ndis does something unpredictable, e.g. calls our miniport halt independently
     * from protocol unbind and concurrently with it*/
    vboxNetFltRetain(pThis, false);

    rc = RTSemMutexRequest(pThis->u.s.hAdaptMutex, RT_INDEFINITE_WAIT);
    if(RT_SUCCESS(rc))
    {
#ifndef VBOX_NETFLT_ONDEMAND_BIND
        Assert(vboxNetFltWinGetAdaptState(pAdapt) == kVBoxAdaptState_Connected);
        Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initialized);
#ifndef VBOXNETADP
        Assert(vboxNetFltWinGetOpState(&pAdapt->PTState) == kVBoxNetDevOpState_Initialized);
#endif
//        if(
//#ifdef VBOXNETADP
//                vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initialized
//#else
//                vboxNetFltWinGetOpState(&pAdapt->PTState) == kVBoxNetDevOpState_Initialized
////                    && vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initialized
//#endif
//                )
        if(vboxNetFltWinGetAdaptState(pAdapt) == kVBoxAdaptState_Connected)
        {
            vboxNetFltWinSetAdaptState(pAdapt, kVBoxAdaptState_Disconnecting);
#ifndef VBOXNETADP
            Status = vboxNetFltWinPtDoUnbinding(pAdapt, bOnUnbind);
#else
            Status = vboxNetFltWinMpDoDeinitialization(pAdapt);
#endif
            Assert(Status == NDIS_STATUS_SUCCESS);

            vboxNetFltWinSetAdaptState(pAdapt, kVBoxAdaptState_Disconnected);
            Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized);
#ifndef VBOXNETADP
            Assert(vboxNetFltWinGetOpState(&pAdapt->PTState) == kVBoxNetDevOpState_Deinitialized);
#endif
//            /* paranoya */
//            vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
//#ifndef VBOXNETADP
//            vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Deinitialized);
//#endif

            vboxNetFltWinPtFiniPADAPT(pAdapt);

            /* we're unbinding, make an unbind-related release */
            vboxNetFltRelease(pThis, false);
#else
            Status = vboxNetFltWinPtFiniUnbind(pAdapt);
            if(Status != NDIS_STATUS_SUCCESS)
            {
                Assert(0);
                /* pDetachInfo->Status = VERR_GENERAL_FAILURE; */
            }
#endif
        }
        else
        {
            AssertBreakpoint();
#ifndef VBOXNETADP
            pAdapt->Status = NDIS_STATUS_FAILURE;
#endif
            if(!bOnUnbind)
            {
                vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
            }
            Status = NDIS_STATUS_FAILURE;
        }
        RTSemMutexRelease(pThis->u.s.hAdaptMutex);
    }
    else
    {
        AssertBreakpoint();
        Status = NDIS_STATUS_FAILURE;
    }

    /* release for the retain we made before waining on the mutex */
    vboxNetFltRelease(pThis, false);

    return Status;
}

/**
 * Worker for vboxNetFltWinAttachToInterface.
 *
 * @param   pAttachInfo     Structure for communicating with
 *                          vboxNetFltWinAttachToInterface.
 */
static void vboxNetFltWinAttachToInterfaceWorker(PATTACH_INFO pAttachInfo)
{
    PVBOXNETFLTINS pThis = pAttachInfo->pNetFltIf;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    int rc;
    PADAPT pAdapt = PVBOXNETFLTINS_2_PADAPT(pThis);

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    /* to ensure we're not removed while we're here */
    vboxNetFltRetain(pThis, false);

    rc = RTSemMutexRequest(pThis->u.s.hAdaptMutex, RT_INDEFINITE_WAIT);
    if(RT_SUCCESS(rc))
    {
        RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
        Assert(vboxNetFltWinGetAdaptState(pAdapt) == kVBoxAdaptState_Disconnected);
        Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized);
#ifndef VBOXNETADP
        Assert(vboxNetFltWinGetOpState(&pAdapt->PTState) == kVBoxNetDevOpState_Deinitialized);
#endif
//        if(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized
//#ifndef VBOXNETADP
//                && vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized
//#endif
//                )
        if(vboxNetFltWinGetAdaptState(pAdapt) == kVBoxAdaptState_Disconnected)
        {

#ifndef VBOX_NETFLT_ONDEMAND_BIND
            if(pAttachInfo->fRediscovery)
            {
                /* rediscovery means adaptor bind is performed while intnet is already using it
                 * i.e. adaptor was unbound while being used by intnet and now being bound back again */
                Assert(((VBOXNETFTLINSSTATE)ASMAtomicUoReadU32((uint32_t volatile *)&pThis->enmState)) == kVBoxNetFltInsState_Connected);
            }
#ifndef VBOXNETADP
            Status = vboxNetFltWinPtInitPADAPT(pAdapt, pAttachInfo->pCreateContext->pOurName);
#else
            Status = vboxNetFltWinPtInitPADAPT(pAdapt);
#endif
            if(Status == NDIS_STATUS_SUCCESS)
            {
                vboxNetFltWinSetAdaptState(pAdapt, kVBoxAdaptState_Connecting);

#ifndef VBOXNETADP
                Status = vboxNetFltWinPtDoBinding(pAdapt, pAttachInfo->pCreateContext->pOurName, pAttachInfo->pCreateContext->pBindToName);
#else
                Status = vboxNetFltWinMpDoInitialization(pAdapt, pAttachInfo->pCreateContext->hMiniportAdapter, pAttachInfo->pCreateContext->hWrapperConfigurationContext);
#endif
                if (Status == NDIS_STATUS_SUCCESS)
                {
                    if(pAttachInfo->fRediscovery || (Status = vboxNetFltWinMpReferenceControlDevice()) == NDIS_STATUS_SUCCESS)
                    {
#ifndef VBOXNETADP
                        if(pAdapt->Status == NDIS_STATUS_SUCCESS)
#endif
                        {
                            vboxNetFltWinSetAdaptState(pAdapt, kVBoxAdaptState_Connected);
//                            Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Initialized);
#ifndef VBOXNETADP
                            Assert(vboxNetFltWinGetOpState(&pAdapt->PTState) == kVBoxNetDevOpState_Initialized);
#endif
//                            /* paranoya */
////                            vboxNetFltWinSetAdaptState(&pAdapt->MPState, kVBoxNetDevOpState_Initialized);
//#ifndef VBOXNETADP
//                            vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Initialized);
//#endif

                            RTSpinlockAcquire(pThis->hSpinlock, &Tmp);

                            /* 4. mark as connected */
                            ASMAtomicUoWriteBool(&pThis->fDisconnectedFromHost, false);

                            RTSpinlockRelease(pThis->hSpinlock, &Tmp);

                            pAttachInfo->Status = VINF_SUCCESS;
                            pAttachInfo->pCreateContext->Status = NDIS_STATUS_SUCCESS;

                            RTSemMutexRelease(pThis->u.s.hAdaptMutex);

                            vboxNetFltRelease(pThis, false);

                            return;
                        }
                        AssertBreakpoint();

                        if(!pAttachInfo->fRediscovery)
                        {
                            vboxNetFltWinMpDereferenceControlDevice();
                        }
                    }
                    AssertBreakpoint();
#ifndef VBOXNETADP
                    vboxNetFltWinPtDoUnbinding(pAdapt, true);
#else
                    vboxNetFltWinMpDoDeinitialization(pAdapt);
#endif
                }
                AssertBreakpoint();
                vboxNetFltWinPtFiniPADAPT(pAdapt);
            }
            AssertBreakpoint();
            vboxNetFltWinSetAdaptState(pAdapt, kVBoxAdaptState_Disconnected);
            Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized);
#ifndef VBOXNETADP
            Assert(vboxNetFltWinGetOpState(&pAdapt->PTState) == kVBoxNetDevOpState_Deinitialized);
#endif
//            /* paranoya */
//            vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
//#ifndef VBOXNETADP
//            vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Deinitialized);
//#endif
        }
        AssertBreakpoint();

#else  /* VBOX_NETFLT_ONDEMAND_BIND */
            Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

            Status = vboxNetFltWinPtInitBind(pAdapt);
            if (Status != NDIS_STATUS_SUCCESS)
            {
                pAttachInfo->Status = VERR_GENERAL_FAILURE;
                break;
            }

            Status = vboxNetFltWinGetMacAddress(pAdapt, &pThis->u.s.Mac);
            if (Status != NDIS_STATUS_SUCCESS)
            {
                vboxNetFltWinPtFiniUnbind(pAdapt);
                pAttachInfo->Status = VERR_GENERAL_FAILURE;
                break;
            }
#endif /* VBOX_NETFLT_ONDEMAND_BIND */


        pAttachInfo->Status = VERR_GENERAL_FAILURE;
        pAttachInfo->pCreateContext->Status = Status;
        RTSemMutexRelease(pThis->u.s.hAdaptMutex);
    }
    else
    {
        AssertBreakpoint();
        pAttachInfo->Status = rc;
    }

    vboxNetFltRelease(pThis, false);

    return;
}

/**
 * Common code for vboxNetFltOsInitInstance and
 * vboxNetFltOsMaybeRediscovered.
 *
 * @returns IPRT status code.
 * @param   pThis           The instance.
 * @param   fRediscovery    True if vboxNetFltOsMaybeRediscovered is calling,
 *                          false if it's vboxNetFltOsInitInstance.
 */
static int vboxNetFltWinAttachToInterface(PVBOXNETFLTINS pThis, void * pContext, bool fRediscovery)
{
    ATTACH_INFO Info;
    Info.pNetFltIf = pThis;
    Info.fRediscovery = fRediscovery;
    Info.pCreateContext = (PCREATE_INSTANCE_CONTEXT)pContext;


#ifdef VBOX_NETFLT_ONDEMAND_BIND
    /* packet queue worker thread gets created on attach interface, need to do it via job at passive level */
    vboxNetFltWinJobSynchExecAtPassive((JOB_ROUTINE)vboxNetFltWinAttachToInterfaceWorker, &Info);
#else
    vboxNetFltWinAttachToInterfaceWorker(&Info);
#endif
    return Info.Status;
}

/*
 *
 * The OS specific interface definition
 *
 */


bool vboxNetFltOsMaybeRediscovered(PVBOXNETFLTINS pThis)
{
    /* AttachToInterface true if disconnected */
    return !ASMAtomicUoReadBool(&pThis->fDisconnectedFromHost);
}

int vboxNetFltPortOsXmit(PVBOXNETFLTINS pThis, PINTNETSG pSG, uint32_t fDst)
{
    int rc = VINF_SUCCESS;
    uint32_t cRefs = 0;
    PADAPT pAdapt;
#if !defined(VBOXNETADP) && !defined(VBOX_NETFLT_ONDEMAND_BIND)
    if(fDst & INTNETTRUNKDIR_WIRE)
    {
        cRefs++;
    }
    if(fDst & INTNETTRUNKDIR_HOST)
    {
        cRefs++;
    }
#else
    if(fDst & INTNETTRUNKDIR_WIRE || fDst & INTNETTRUNKDIR_HOST)
    {
        cRefs = 1;
    }
#endif

    AssertReturn(cRefs, VINF_SUCCESS);

    pAdapt = PVBOXNETFLTINS_2_PADAPT(pThis);

    if(!vboxNetFltWinIncReferenceAdapt(pAdapt, cRefs))
    {
        return VERR_GENERAL_FAILURE;
    }
#ifndef VBOXNETADP
    if ((fDst & INTNETTRUNKDIR_WIRE)
# ifdef VBOX_NETFLT_ONDEMAND_BIND
         ||   (fDst & INTNETTRUNKDIR_HOST)
# endif
            )
    {
        PNDIS_PACKET pPacket;

        pPacket = vboxNetFltWinNdisPacketFromSG(pAdapt, pSG, NULL /*pBufToFree*/,
                                                true /*fToWire*/, true /*fCopyMemory*/);

        if (pPacket)
        {
            NDIS_STATUS fStatus;

#if defined(DEBUG_NETFLT_PACKETS) || !defined(VBOX_LOOPBACK_USEFLAGS)
            vboxNetFltWinLbPutSendPacket(pAdapt, pPacket, true /* bFromIntNet */);
#endif
            NdisSend(&fStatus, pAdapt->hBindingHandle, pPacket);
            if (fStatus != NDIS_STATUS_PENDING)
            {
#if defined(DEBUG_NETFLT_PACKETS) || !defined(VBOX_LOOPBACK_USEFLAGS)
                /* the status is NOT pending, complete the packet */
                bool bTmp = vboxNetFltWinLbRemoveSendPacket(pAdapt, pPacket);
                Assert(bTmp);
#endif
                if(!NT_SUCCESS(fStatus))
                {
                    /* TODO: convert status to VERR_xxx */
                    rc = VERR_GENERAL_FAILURE;
                }

                vboxNetFltWinFreeSGNdisPacket(pPacket, true);
            }
            else
            {
                /* pending, dereference on packet complete */
                cRefs--;
            }
        }
        else
        {
            Assert(0);
            rc = VERR_NO_MEMORY;
        }
    }
#endif
#ifndef VBOX_NETFLT_ONDEMAND_BIND
#ifndef VBOXNETADP
    if (fDst & INTNETTRUNKDIR_HOST)
#else
    if(cRefs)
#endif
    {
        PNDIS_PACKET pPacket = vboxNetFltWinNdisPacketFromSG(pAdapt, pSG, NULL /*pBufToFree*/,
                                                             false /*fToWire*/, true /*fCopyMemory*/);
        if (pPacket)
        {
            NdisMIndicateReceivePacket(pAdapt->hMiniportHandle, &pPacket, 1);
            cRefs--;
#ifdef VBOXNETADP
            STATISTIC_INCREASE(pAdapt->cRxSuccess);
#endif
        }
        else
        {
            Assert(0);
#ifdef VBOXNETADP
            STATISTIC_INCREASE(pAdapt->cRxError);
#endif
            rc = VERR_NO_MEMORY;
        }
    }

    Assert(cRefs <= 2);

    if(cRefs)
    {
        vboxNetFltWinDecReferenceAdapt(pAdapt, cRefs);
    }
#endif

    return rc;
}

bool vboxNetFltPortOsIsPromiscuous(PVBOXNETFLTINS pThis)
{
#ifndef VBOXNETADP
    PADAPT pAdapt = PVBOXNETFLTINS_2_PADAPT(pThis);
    if(VBOXNETFLT_PROMISCUOUS_SUPPORTED(pAdapt))
    {
        bool bPromiscuous;
        if(!vboxNetFltWinReferenceAdapt(pAdapt))
            return false;

        bPromiscuous = (pAdapt->fUpperProtocolSetFilter & NDIS_PACKET_TYPE_PROMISCUOUS) == NDIS_PACKET_TYPE_PROMISCUOUS;
            /*vboxNetFltWinIsPromiscuous(pAdapt);*/

        vboxNetFltWinDereferenceAdapt(pAdapt);
        return bPromiscuous;
    }
    return false;
#else
    return true;
#endif
}

void vboxNetFltPortOsGetMacAddress(PVBOXNETFLTINS pThis, PRTMAC pMac)
{
    *pMac = pThis->u.s.Mac;
}

bool vboxNetFltPortOsIsHostMac(PVBOXNETFLTINS pThis, PCRTMAC pMac)
{
    /* ASSUMES that the MAC address never changes. */
    return pThis->u.s.Mac.au16[0] == pMac->au16[0]
        && pThis->u.s.Mac.au16[1] == pMac->au16[1]
        && pThis->u.s.Mac.au16[2] == pMac->au16[2];
}

void vboxNetFltPortOsSetActive(PVBOXNETFLTINS pThis, bool fActive)
{
#ifndef VBOXNETADP
    NDIS_STATUS Status;
#endif
    PADAPT pAdapt = PVBOXNETFLTINS_2_PADAPT(pThis);

    /* we first wait for all pending ops to complete
     * this might include all packets queued for processing */
    for(;;)
    {
    	if(fActive)
    	{
    		if(!pThis->u.s.cModePassThruRefs)
    		{
    			break;
    		}
    	}
    	else
    	{
    		if(!pThis->u.s.cModeNetFltRefs)
    		{
    			break;
    		}
    	}
		vboxNetFltWinSleep(2);
    }

    if(!vboxNetFltWinReferenceAdapt(pAdapt))
    	return;
#ifndef VBOXNETADP

    /* the packets put to ReceiveQueue Array are currently not holding the references,
     * simply need to flush them */
    vboxNetFltWinPtFlushReceiveQueue(pAdapt, false /*fReturn*/);

    if(fActive)
    {
#ifdef DEBUG_misha
        NDIS_PHYSICAL_MEDIUM           PhMedium;
        bool bPromiscSupported;

        Status = vboxNetFltWinQueryPhysicalMedium(pAdapt, &PhMedium);
        if(Status != NDIS_STATUS_SUCCESS)
        {

            DBGPRINT(("vboxNetFltWinQueryPhysicalMedium failed, Status (0x%x), setting medium to NdisPhysicalMediumUnspecified\n", Status));
            Assert(Status == NDIS_STATUS_NOT_SUPPORTED);
            if(Status != NDIS_STATUS_NOT_SUPPORTED)
            {
                LogRel(("vboxNetFltWinQueryPhysicalMedium failed, Status (0x%x), setting medium to NdisPhysicalMediumUnspecified\n", Status));
            }
            PhMedium = NdisPhysicalMediumUnspecified;
        }
        else
        {
            DBGPRINT(("(SUCCESS) vboxNetFltWinQueryPhysicalMedium SUCCESS\n"));
        }

        bPromiscSupported =  (!(PhMedium == NdisPhysicalMediumWirelessWan
                        || PhMedium == NdisPhysicalMediumWirelessLan
                        || PhMedium == NdisPhysicalMediumNative802_11
                        || PhMedium == NdisPhysicalMediumBluetooth
                        /*|| PhMedium == NdisPhysicalMediumWiMax */
                        ));

        Assert(bPromiscSupported == VBOXNETFLT_PROMISCUOUS_SUPPORTED(pAdapt));
#endif
    }

    if(VBOXNETFLT_PROMISCUOUS_SUPPORTED(pAdapt))
    {
        Status = vboxNetFltWinSetPromiscuous(pAdapt, fActive);
        if(Status != NDIS_STATUS_SUCCESS)
        {
            DBGPRINT(("vboxNetFltWinSetPromiscuous failed, Status (0x%x), fActive (%d)\n", Status, fActive));
            Assert(0);
            LogRel(("vboxNetFltWinSetPromiscuous failed, Status (0x%x), fActive (%d)\n", Status, fActive));
        }
    }
#else
# ifdef VBOXNETADP_REPORT_DISCONNECTED
    if(fActive)
    {
        NdisMIndicateStatus(pAdapt->hMiniportHandle,
                                 NDIS_STATUS_MEDIA_CONNECT,
                                 (PVOID)NULL,
                                 0);
    }
    else
    {
        NdisMIndicateStatus(pAdapt->hMiniportHandle,
                                 NDIS_STATUS_MEDIA_DISCONNECT,
                                 (PVOID)NULL,
                                 0);
    }
#else
    if(fActive)
    {
        /* indicate status change to make the ip settings be re-picked for dhcp */
        NdisMIndicateStatus(pAdapt->hMiniportHandle,
                                 NDIS_STATUS_MEDIA_DISCONNECT,
                                 (PVOID)NULL,
                                 0);

        NdisMIndicateStatus(pAdapt->hMiniportHandle,
                                 NDIS_STATUS_MEDIA_CONNECT,
                                 (PVOID)NULL,
                                 0);
    }
# endif
#endif
    vboxNetFltWinDereferenceAdapt(pAdapt);

    return;
}

int vboxNetFltOsDisconnectIt(PVBOXNETFLTINS pThis)
{
    NDIS_STATUS Status = vboxNetFltWinDisconnectIt(pThis);
    return Status == NDIS_STATUS_SUCCESS ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

static void vboxNetFltWinConnectItWorker(PWORKER_INFO pInfo)
{
    NDIS_STATUS Status;
    PVBOXNETFLTINS pInstance = pInfo->pNetFltIf;
    PADAPT pAdapt = PVBOXNETFLTINS_2_PADAPT(pInstance);

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    /* this is not a rediscovery, initialize Mac cache */
    if(vboxNetFltWinReferenceAdapt(pAdapt))
    {
#ifndef VBOXNETADP
        Status = vboxNetFltWinGetMacAddress(pAdapt, &pInstance->u.s.Mac);
        if (Status == NDIS_STATUS_SUCCESS)
#endif
        {
            Status = vboxNetFltWinQuInitPacketQueue(pInstance);
            if(Status == NDIS_STATUS_SUCCESS)
            {
                pInfo->Status = VINF_SUCCESS;
            }
            else
            {
                pInfo->Status = VERR_GENERAL_FAILURE;
            }
        }
#ifndef VBOXNETADP
        else
        {
            pInfo->Status = VERR_INTNET_FLT_IF_FAILED;
        }
#endif

        vboxNetFltWinDereferenceAdapt(pAdapt);
    }
    else
    {
        pInfo->Status = VERR_INTNET_FLT_IF_NOT_FOUND;
    }
}

static int vboxNetFltWinConnectIt(PVBOXNETFLTINS pThis)
{
    WORKER_INFO Info;
    Info.pNetFltIf = pThis;

    vboxNetFltWinJobSynchExecAtPassive(vboxNetFltWinConnectItWorker, &Info);

    return Info.Status;
}

int vboxNetFltOsConnectIt(PVBOXNETFLTINS pThis)
{
    return vboxNetFltWinConnectIt(pThis);
}

void vboxNetFltOsDeleteInstance(PVBOXNETFLTINS pThis)
{
    vboxNetFltWinDeleteInstance(pThis);
}

int vboxNetFltOsInitInstance(PVBOXNETFLTINS pThis, void *pvContext)
{
    int rc = RTSemMutexCreate(&pThis->u.s.hAdaptMutex);
    if (RT_SUCCESS(rc))
    {
        rc = vboxNetFltWinAttachToInterface(pThis, pvContext, false /*fRediscovery*/ );
        if (RT_SUCCESS(rc))
        {
            return rc;
        }
        RTSemMutexDestroy(pThis->u.s.hAdaptMutex);
    }
    return rc;
}

int vboxNetFltOsPreInitInstance(PVBOXNETFLTINS pThis)
{
    PADAPT pAdapt = PVBOXNETFLTINS_2_PADAPT(pThis);
    pThis->u.s.cModeNetFltRefs = 0;
    pThis->u.s.cModePassThruRefs = 0;
    vboxNetFltWinSetAdaptState(pAdapt, kVBoxAdaptState_Disconnected);
    vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
#ifndef VBOXNETADP
    vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Deinitialized);
#endif
    return VINF_SUCCESS;
}

