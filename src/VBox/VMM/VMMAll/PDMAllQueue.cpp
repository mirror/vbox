/* $Id$ */
/** @file
 * PDM Queue - Transport data and tasks to EMT and R3.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP LOG_GROUP_PDM_QUEUE
#include "PDMInternal.h"
#include <VBox/pdm.h>
#ifndef IN_GC
# include <VBox/rem.h>
# include <VBox/mm.h>
#endif
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>


/**
 * Allocate an item from a queue.
 * The allocated item must be handed on to PDMR3QueueInsert() after the
 * data have been filled in.
 *
 * @returns Pointer to allocated queue item.
 * @returns NULL on failure. The queue is exhausted.
 * @param   pQueue      The queue handle.
 * @thread  Any thread.
 */
PDMDECL(PPDMQUEUEITEMCORE) PDMQueueAlloc(PPDMQUEUE pQueue)
{
    Assert(VALID_PTR(pQueue) && pQueue->CTXSUFF(pVM));
    PPDMQUEUEITEMCORE pNew;
    uint32_t iNext;
    uint32_t i;
    do
    {
        i = pQueue->iFreeTail;
        if (i == pQueue->iFreeHead)
            return NULL;
        pNew = pQueue->aFreeItems[i].CTXSUFF(pItem);
        iNext = (i + 1) % (pQueue->cItems + PDMQUEUE_FREE_SLACK);
    } while (!ASMAtomicCmpXchgU32(&pQueue->iFreeTail, iNext, i));
    return pNew;
}


/**
 * Queue an item.
 * The item must have been obtained using PDMQueueAlloc(). Once the item
 * have been passed to this function it must not be touched!
 *
 * @param   pQueue      The queue handle.
 * @param   pItem       The item to insert.
 * @thread  Any thread.
 */
PDMDECL(void) PDMQueueInsert(PPDMQUEUE pQueue, PPDMQUEUEITEMCORE pItem)
{
    Assert(VALID_PTR(pQueue) && pQueue->CTXSUFF(pVM));
    Assert(VALID_PTR(pItem));

    PPDMQUEUEITEMCORE pNext;
    do
    {
        pNext = pQueue->CTXSUFF(pPending);
        pItem->CTXSUFF(pNext) = pNext;
    } while (!ASMAtomicCmpXchgPtr((void * volatile *)&pQueue->CTXSUFF(pPending), pItem, pNext));

    if (!pQueue->pTimer)
    {
        PVM pVM = pQueue->CTXSUFF(pVM);
        Log2(("PDMQueueInsert: VM_FF_PDM_QUEUES %d -> 1\n", VM_FF_ISSET(pVM, VM_FF_PDM_QUEUES)));
        VM_FF_SET(pVM, VM_FF_PDM_QUEUES);
#ifdef IN_RING3
        REMR3NotifyQueuePending(pVM); /** @todo r=bird: we can remove REMR3NotifyQueuePending and let VMR3NotifyFF do the work. */
        VMR3NotifyFF(pVM, true);
#endif
    }
}


/**
 * Queue an item.
 * The item must have been obtained using PDMQueueAlloc(). Once the item
 * have been passed to this function it must not be touched!
 *
 * @param   pQueue          The queue handle.
 * @param   pItem           The item to insert.
 * @param   NanoMaxDelay    The maximum delay before processing the queue, in nanoseconds.
 *                          This applies only to GC.
 * @thread  Any thread.
 */
PDMDECL(void) PDMQueueInsertEx(PPDMQUEUE pQueue, PPDMQUEUEITEMCORE pItem, uint64_t NanoMaxDelay)
{
    PDMQueueInsert(pQueue, pItem);
#ifdef IN_GC
    PVM pVM = pQueue->CTXSUFF(pVM);
    /** @todo figure out where to put this, the next bit should go there too.
    if (NanoMaxDelay)
    {

    }
    else */
    {
        VM_FF_SET(pVM, VM_FF_TO_R3);
        Log2(("PDMQueueInsertEx: Setting VM_FF_TO_R3\n"));
    }
#endif
}



/**
 * Gets the GC pointer for the specified queue.
 *
 * @returns The GC address of the queue.
 * @returns NULL if pQueue is invalid.
 * @param   pQueue          The queue handle.
 */
PDMDECL(GCPTRTYPE(PPDMQUEUE)) PDMQueueGCPtr(PPDMQUEUE pQueue)
{
    Assert(VALID_PTR(pQueue));
    Assert(pQueue->pVMHC && pQueue->pVMGC);
#ifdef IN_GC
    return pQueue;
#else
    return MMHyperHC2GC(pQueue->pVMHC, pQueue);
#endif
}


/**
 * Flushes a PDM queue.
 *
 * @param   pQueue          The queue handle.
 */
PDMDECL(void) PDMQueueFlush(PPDMQUEUE pQueue)
{
    Assert(VALID_PTR(pQueue));
    Assert(pQueue->pVMHC && pQueue->pVMGC);
    PVM pVM = CTXSUFF(pQueue->pVM);
#ifdef IN_GC
    pVM->pdm.s.CTXSUFF(pQueueFlush) = pQueue;
    VMMGCCallHost(pVM, VMMCALLHOST_PDM_QUEUE_FLUSH, (uintptr_t)pQueue);
#elif defined(IN_RING0)
    pVM->pdm.s.CTXSUFF(pQueueFlush) = pQueue;
    VMMR0CallHost(pVM, VMMCALLHOST_PDM_QUEUE_FLUSH, (uintptr_t)pQueue);
#else /* IN_RING3: */
    PVMREQ pReq;
    VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)PDMR3QueueFlushWorker, 2, pVM, pQueue);
    VMR3ReqFree(pReq);
#endif
}
