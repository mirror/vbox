/* $Id$ */
/** @file
 * PDM Queue - Transport data and tasks to EMT and R3.
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
#define LOG_GROUP LOG_GROUP_PDM_QUEUE
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/rem.h>
#include <VBox/vm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static inline void pdmR3QueueFree(PPDMQUEUE pQueue, PPDMQUEUEITEMCORE pItem);
static bool pdmR3QueueFlush(PPDMQUEUE pQueue);
static DECLCALLBACK(void) pdmR3QueueTimer(PVM pVM, PTMTIMER pTimer, void *pvUser);



/**
 * Internal worker for the queue creation apis.
 *
 * @returns VBox status.
 * @param   pVM                 VM handle.
 * @param   cbItem              Item size.
 * @param   cItems              Number of items.
 * @param   cMilliesInterval    Number of milliseconds between polling the queue.
 *                              If 0 then the emulation thread will be notified whenever an item arrives.
 * @param   fGCEnabled          Set if the queue will be used from GC and need to be allocated from the Hyper heap.
 * @param   ppQueue             Where to store the queue handle.
 */
static int pdmR3QueueCreate(PVM pVM, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval, bool fGCEnabled, PPDMQUEUE *ppQueue)
{
    /*
     * Validate input.
     */
    if (cbItem < sizeof(PDMQUEUEITEMCORE))
    {
        AssertMsgFailed(("cbItem=%d\n", cbItem));
        return VERR_INVALID_PARAMETER;
    }
    if (cItems < 1 || cItems >= 0x10000)
    {
        AssertMsgFailed(("cItems=%d valid:[1-65535]\n", cItems));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Align the item size and calculate the structure size.
     */
    cbItem = RT_ALIGN(cbItem, sizeof(RTUINTPTR));
    unsigned cb = cbItem * cItems + RT_ALIGN_Z(RT_OFFSETOF(PDMQUEUE, aFreeItems[cItems + PDMQUEUE_FREE_SLACK]), 16);
    PPDMQUEUE pQueue;
    int rc;
    if (fGCEnabled)
        rc = MMHyperAlloc(pVM, cb, 0, MM_TAG_PDM_QUEUE, (void **)&pQueue );
    else
        rc = MMR3HeapAllocZEx(pVM, MM_TAG_PDM_QUEUE, cb, (void **)&pQueue);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Initialize the data fields.
     */
    pQueue->pVMHC = pVM;
    pQueue->pVMGC = fGCEnabled ? pVM->pVMGC : 0;
    pQueue->cMilliesInterval = cMilliesInterval;
    //pQueue->pTimer = NULL;
    pQueue->cbItem = cbItem;
    pQueue->cItems = cItems;
    //pQueue->pPendingHC = NULL;
    //pQueue->pPendingGC = NULL;
    pQueue->iFreeHead = cItems;
    //pQueue->iFreeTail = 0;
    PPDMQUEUEITEMCORE pItem = (PPDMQUEUEITEMCORE)((char *)pQueue + RT_ALIGN_Z(RT_OFFSETOF(PDMQUEUE, aFreeItems[cItems + PDMQUEUE_FREE_SLACK]), 16));
    for (unsigned i = 0; i < cItems; i++, pItem = (PPDMQUEUEITEMCORE)((char *)pItem + cbItem))
    {
        pQueue->aFreeItems[i].pItemHC = pItem;
        if (fGCEnabled)
            pQueue->aFreeItems[i].pItemGC = MMHyperHC2GC(pVM, pItem);
    }

    /*
     * Create timer?
     */
    if (cMilliesInterval)
    {
        int rc = TMR3TimerCreateInternal(pVM, TMCLOCK_REAL, pdmR3QueueTimer, pQueue, "Queue timer", &pQueue->pTimer);
        if (VBOX_SUCCESS(rc))
        {
            rc = TMTimerSetMillies(pQueue->pTimer, cMilliesInterval);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("TMTimerSetMillies failed rc=%Vrc\n", rc));
                int rc2 = TMTimerDestroy(pQueue->pTimer); AssertRC(rc2);
            }
        }
        else
            AssertMsgFailed(("TMR3TimerCreateInternal failed rc=%Vrc\n", rc));
        if (VBOX_FAILURE(rc))
        {
            if (fGCEnabled)
                MMHyperFree(pVM, pQueue);
            else
                MMR3HeapFree(pQueue);
            return rc;
        }

        /*
         * Insert into the queue list for timer driven queues.
         */
        pQueue->pNext = pVM->pdm.s.pQueuesTimer;
        pVM->pdm.s.pQueuesTimer = pQueue;
    }
    else
    {
        /*
         * Insert into the queue list for forced action driven queues.
         * This is a FIFO, so insert at the end.
         */
        /** @todo we should add a priority priority to the queues so we don't have to rely on
         * the initialization order to deal with problems like #1605 (pgm/pcnet deadlock
         * caused by the critsect queue to be last in the chain).
         * Update, the critical sections are no longer using queues.
         */
        if (!pVM->pdm.s.pQueuesForced)
            pVM->pdm.s.pQueuesForced = pQueue;
        else
        {
            PPDMQUEUE pPrev = pVM->pdm.s.pQueuesForced;
            while (pPrev->pNext)
                pPrev = pPrev->pNext;
            pPrev->pNext = pQueue;
        }
    }

    *ppQueue = pQueue;
    return VINF_SUCCESS;
}


/**
 * Create a queue with a device owner.
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle.
 * @param   pDevIns             Device instance.
 * @param   cbItem              Size a queue item.
 * @param   cItems              Number of items in the queue.
 * @param   cMilliesInterval    Number of milliseconds between polling the queue.
 *                              If 0 then the emulation thread will be notified whenever an item arrives.
 * @param   pfnCallback         The consumer function.
 * @param   fGCEnabled          Set if the queue must be usable from GC.
 * @param   ppQueue             Where to store the queue handle on success.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueCreateDevice(PVM pVM, PPDMDEVINS pDevIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                      PFNPDMQUEUEDEV pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue)
{
    LogFlow(("PDMR3QueueCreateDevice: pDevIns=%p cbItem=%d cItems=%d cMilliesInterval=%d pfnCallback=%p fGCEnabled=%RTbool\n",
             pDevIns, cbItem, cItems, cMilliesInterval, pfnCallback, fGCEnabled));

    /*
     * Validate input.
     */
    VM_ASSERT_EMT(pVM);
    if (!pfnCallback)
    {
        AssertMsgFailed(("No consumer callback!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the queue.
     */
    PPDMQUEUE pQueue;
    int rc = pdmR3QueueCreate(pVM, cbItem, cItems, cMilliesInterval, fGCEnabled, &pQueue);
    if (VBOX_SUCCESS(rc))
    {
        pQueue->enmType = PDMQUEUETYPE_DEV;
        pQueue->u.Dev.pDevIns = pDevIns;
        pQueue->u.Dev.pfnCallback = pfnCallback;

        *ppQueue = pQueue;
        Log(("PDM: Created device queue %p; cbItem=%d cItems=%d cMillies=%d pfnCallback=%p pDevIns=%p\n",
             cbItem, cItems, cMilliesInterval, pfnCallback, pDevIns));
    }
    return rc;
}


/**
 * Create a queue with a driver owner.
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle.
 * @param   pDrvIns             Driver instance.
 * @param   cbItem              Size a queue item.
 * @param   cItems              Number of items in the queue.
 * @param   cMilliesInterval    Number of milliseconds between polling the queue.
 *                              If 0 then the emulation thread will be notified whenever an item arrives.
 * @param   pfnCallback         The consumer function.
 * @param   ppQueue             Where to store the queue handle on success.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                      PFNPDMQUEUEDRV pfnCallback, PPDMQUEUE *ppQueue)
{
    LogFlow(("PDMR3QueueCreateDriver: pDrvIns=%p cbItem=%d cItems=%d cMilliesInterval=%d pfnCallback=%p\n",
             pDrvIns, cbItem, cItems, cMilliesInterval, pfnCallback));

    /*
     * Validate input.
     */
    VM_ASSERT_EMT(pVM);
    if (!pfnCallback)
    {
        AssertMsgFailed(("No consumer callback!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the queue.
     */
    PPDMQUEUE pQueue;
    int rc = pdmR3QueueCreate(pVM, cbItem, cItems, cMilliesInterval, false, &pQueue);
    if (VBOX_SUCCESS(rc))
    {
        pQueue->enmType = PDMQUEUETYPE_DRV;
        pQueue->u.Drv.pDrvIns = pDrvIns;
        pQueue->u.Drv.pfnCallback = pfnCallback;

        *ppQueue = pQueue;
        Log(("PDM: Created driver queue %p; cbItem=%d cItems=%d cMillies=%d pfnCallback=%p pDrvIns=%p\n",
             cbItem, cItems, cMilliesInterval, pfnCallback, pDrvIns));
    }
    return rc;
}


/**
 * Create a queue with an internal owner.
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle.
 * @param   cbItem              Size a queue item.
 * @param   cItems              Number of items in the queue.
 * @param   cMilliesInterval    Number of milliseconds between polling the queue.
 *                              If 0 then the emulation thread will be notified whenever an item arrives.
 * @param   pfnCallback         The consumer function.
 * @param   fGCEnabled          Set if the queue must be usable from GC.
 * @param   ppQueue             Where to store the queue handle on success.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueCreateInternal(PVM pVM, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                        PFNPDMQUEUEINT pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue)
{
    LogFlow(("PDMR3QueueCreateInternal: cbItem=%d cItems=%d cMilliesInterval=%d pfnCallback=%p fGCEnabled=%RTbool\n",
             cbItem, cItems, cMilliesInterval, pfnCallback, fGCEnabled));

    /*
     * Validate input.
     */
    VM_ASSERT_EMT(pVM);
    if (!pfnCallback)
    {
        AssertMsgFailed(("No consumer callback!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the queue.
     */
    PPDMQUEUE pQueue;
    int rc = pdmR3QueueCreate(pVM, cbItem, cItems, cMilliesInterval, fGCEnabled, &pQueue);
    if (VBOX_SUCCESS(rc))
    {
        pQueue->enmType = PDMQUEUETYPE_INTERNAL;
        pQueue->u.Int.pfnCallback = pfnCallback;

        *ppQueue = pQueue;
        Log(("PDM: Created internal queue %p; cbItem=%d cItems=%d cMillies=%d pfnCallback=%p\n",
             cbItem, cItems, cMilliesInterval, pfnCallback));
    }
    return rc;
}


/**
 * Create a queue with an external owner.
 *
 * @returns VBox status code.
 * @param   pVM                 VM handle.
 * @param   cbItem              Size a queue item.
 * @param   cItems              Number of items in the queue.
 * @param   cMilliesInterval    Number of milliseconds between polling the queue.
 *                              If 0 then the emulation thread will be notified whenever an item arrives.
 * @param   pfnCallback         The consumer function.
 * @param   pvUser              The user argument to the consumer function.
 * @param   ppQueue             Where to store the queue handle on success.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueCreateExternal(PVM pVM, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval, PFNPDMQUEUEEXT pfnCallback, void *pvUser, PPDMQUEUE *ppQueue)
{
    LogFlow(("PDMR3QueueCreateExternal: cbItem=%d cItems=%d cMilliesInterval=%d pfnCallback=%p\n", cbItem, cItems, cMilliesInterval, pfnCallback));

    /*
     * Validate input.
     */
    VM_ASSERT_EMT(pVM);
    if (!pfnCallback)
    {
        AssertMsgFailed(("No consumer callback!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the queue.
     */
    PPDMQUEUE pQueue;
    int rc = pdmR3QueueCreate(pVM, cbItem, cItems, cMilliesInterval, false, &pQueue);
    if (VBOX_SUCCESS(rc))
    {
        pQueue->enmType = PDMQUEUETYPE_EXTERNAL;
        pQueue->u.Ext.pvUser = pvUser;
        pQueue->u.Ext.pfnCallback = pfnCallback;

        *ppQueue = pQueue;
        Log(("PDM: Created external queue %p; cbItem=%d cItems=%d cMillies=%d pfnCallback=%p pvUser=%p\n",
             cbItem, cItems, cMilliesInterval, pfnCallback, pvUser));
    }
    return rc;
}


/**
 * Destroy a queue.
 *
 * @returns VBox status code.
 * @param   pQueue      Queue to destroy.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueDestroy(PPDMQUEUE pQueue)
{
    LogFlow(("PDMR3QueueDestroy: pQueue=%p\n", pQueue));

    /*
     * Validate input.
     */
    if (!pQueue)
        return VERR_INVALID_PARAMETER;
    Assert(pQueue && pQueue->pVMHC);
    PVM pVM = pQueue->pVMHC;
    VM_ASSERT_EMT(pVM);

    /*
     * Unlink it.
     */
    if (pQueue->pTimer)
    {
        if (pVM->pdm.s.pQueuesTimer != pQueue)
        {
            PPDMQUEUE pCur = pVM->pdm.s.pQueuesTimer;
            while (pCur)
            {
                if (pCur->pNext == pQueue)
                {
                    pCur->pNext = pQueue->pNext;
                    break;
                }
                pCur = pCur->pNext;
            }
            AssertMsg(pCur, ("Didn't find the queue!\n"));
        }
        else
            pVM->pdm.s.pQueuesTimer = pQueue->pNext;
    }
    else
    {
        if (pVM->pdm.s.pQueuesForced != pQueue)
        {
            PPDMQUEUE pCur = pVM->pdm.s.pQueuesForced;
            while (pCur)
            {
                if (pCur->pNext == pQueue)
                {
                    pCur->pNext = pQueue->pNext;
                    break;
                }
                pCur = pCur->pNext;
            }
            AssertMsg(pCur, ("Didn't find the queue!\n"));
        }
        else
            pVM->pdm.s.pQueuesForced = pQueue->pNext;
    }
    pQueue->pNext = NULL;
    pQueue->pVMHC = NULL;

    /*
     * Destroy the timer and free it.
     */
    if (pQueue->pTimer)
    {
        TMTimerDestroy(pQueue->pTimer);
        pQueue->pTimer = NULL;
    }
    if (pQueue->pVMGC)
    {
        pQueue->pVMGC = 0;
        MMHyperFree(pVM, pQueue);
    }
    else
        MMR3HeapFree(pQueue);

    return VINF_SUCCESS;
}


/**
 * Destroy a all queues owned by the specified device.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pDevIns     Device instance.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueDestroyDevice(PVM pVM, PPDMDEVINS pDevIns)
{
    LogFlow(("PDMR3QueueDestroyDevice: pDevIns=%p\n", pDevIns));

    /*
     * Validate input.
     */
    if (!pDevIns)
        return VERR_INVALID_PARAMETER;
    VM_ASSERT_EMT(pVM);

    /*
     * Unlink it.
     */
    PPDMQUEUE pQueueNext = pVM->pdm.s.pQueuesTimer;
    PPDMQUEUE pQueue = pVM->pdm.s.pQueuesForced;
    do
    {
        while (pQueue)
        {
            if (    pQueue->enmType == PDMQUEUETYPE_DEV
                &&  pQueue->u.Dev.pDevIns == pDevIns)
            {
                PPDMQUEUE pQueueDestroy = pQueue;
                pQueue = pQueue->pNext;
                int rc = PDMR3QueueDestroy(pQueueDestroy);
                AssertRC(rc);
            }
            else
                pQueue = pQueue->pNext;
        }

        /* next queue list */
        pQueue = pQueueNext;
        pQueueNext = NULL;
    } while (pQueue);

    return VINF_SUCCESS;
}


/**
 * Destroy a all queues owned by the specified driver.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pDrvIns     Driver instance.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns)
{
    LogFlow(("PDMR3QueueDestroyDriver: pDrvIns=%p\n", pDrvIns));

    /*
     * Validate input.
     */
    if (!pDrvIns)
        return VERR_INVALID_PARAMETER;
    VM_ASSERT_EMT(pVM);

    /*
     * Unlink it.
     */
    PPDMQUEUE pQueueNext = pVM->pdm.s.pQueuesTimer;
    PPDMQUEUE pQueue = pVM->pdm.s.pQueuesForced;
    do
    {
        while (pQueue)
        {
            if (    pQueue->enmType == PDMQUEUETYPE_DRV
                &&  pQueue->u.Drv.pDrvIns == pDrvIns)
            {
                PPDMQUEUE pQueueDestroy = pQueue;
                pQueue = pQueue->pNext;
                int rc = PDMR3QueueDestroy(pQueueDestroy);
                AssertRC(rc);
            }
            else
                pQueue = pQueue->pNext;
        }

        /* next queue list */
        pQueue = pQueueNext;
        pQueueNext = NULL;
    } while (pQueue);

    return VINF_SUCCESS;
}


/**
 * Relocate the queues.
 *
 * @param   pVM             The VM handle.
 * @param   offDelta        The relocation delta.
 */
void pdmR3QueueRelocate(PVM pVM, RTGCINTPTR offDelta)
{
    /*
     * Process the queues.
     */
    PPDMQUEUE pQueueNext = pVM->pdm.s.pQueuesTimer;
    PPDMQUEUE pQueue = pVM->pdm.s.pQueuesForced;
    do
    {
        while (pQueue)
        {
            if (pQueue->pVMGC)
            {
                pQueue->pVMGC = pVM->pVMGC;

                /* Pending GC items. */
                if (pQueue->pPendingGC)
                {
                    pQueue->pPendingGC += offDelta;
                    PPDMQUEUEITEMCORE pCur = (PPDMQUEUEITEMCORE)MMHyperGC2HC(pVM, pQueue->pPendingGC);
                    while (pCur->pNextGC)
                    {
                        pCur->pNextGC += offDelta;
                        pCur = (PPDMQUEUEITEMCORE)MMHyperGC2HC(pVM, pCur->pNextGC);
                    }
                }

                /* The free items. */
                uint32_t i = pQueue->iFreeTail;
                while (i != pQueue->iFreeHead)
                {
                    pQueue->aFreeItems[i].pItemGC = MMHyperHC2GC(pVM, pQueue->aFreeItems[i].pItemHC);
                    i = (i + 1) % (pQueue->cItems + PDMQUEUE_FREE_SLACK);
                }
            }

            /* next queue */
            pQueue = pQueue->pNext;
        }

        /* next queue list */
        pQueue = pQueueNext;
        pQueueNext = NULL;
    } while (pQueue);
}


/**
 * Flush pending queues.
 * This is a forced action callback.
 *
 * @param   pVM     VM handle.
 * @thread  Emulation thread only.
 */
PDMR3DECL(void) PDMR3QueueFlushAll(PVM pVM)
{
    VM_ASSERT_EMT(pVM);
    LogFlow(("PDMR3QueuesFlush:\n"));

    VM_FF_CLEAR(pVM, VM_FF_PDM_QUEUES);
    for (PPDMQUEUE pCur = pVM->pdm.s.pQueuesForced; pCur; pCur = pCur->pNext)
    {
        if (    pCur->pPendingHC
            ||  pCur->pPendingGC)
        {
            if (    pdmR3QueueFlush(pCur)
                &&  pCur->pPendingHC)
                /* new items arrived while flushing. */
                pdmR3QueueFlush(pCur);
        }
    }
}


/**
 * Process pending items in one queue.
 *
 * @returns Success indicator.
 *          If false the item the consumer said "enough!".
 * @param   pQueue  The queue.
 */
static bool pdmR3QueueFlush(PPDMQUEUE pQueue)
{
    PPDMQUEUEITEMCORE pItems = (PPDMQUEUEITEMCORE)ASMAtomicXchgPtr((void * volatile *)&pQueue->pPendingHC, NULL);
    AssertMsg(pItems || pQueue->pPendingGC, ("ERROR: can't be NULL now!\n"));

    /*
     * Reverse the list (it's inserted in LIFO order to avoid semaphores, remember).
     */
    PPDMQUEUEITEMCORE pCur = pItems;
    pItems = NULL;
    while (pCur)
    {
        PPDMQUEUEITEMCORE pInsert = pCur;
        pCur = pCur->pNextHC;
        pInsert->pNextHC = pItems;
        pItems = pInsert;
    }

    /*
     * Do the same for any pending GC items.
     */
    while (pQueue->pPendingGC)
    {
        PPDMQUEUEITEMCORE pInsert = (PPDMQUEUEITEMCORE)MMHyperGC2HC(pQueue->pVMHC, pQueue->pPendingGC);
        pQueue->pPendingGC = pInsert->pNextGC;
        pInsert->pNextGC = 0;
        pInsert->pNextHC = pItems;
        pItems = pInsert;
    }

    /*
     * Feed the items to the consumer function.
     */
    Log2(("pdmR3QueueFlush: pQueue=%p enmType=%d pItems=%p\n", pQueue, pQueue->enmType, pItems));
    switch (pQueue->enmType)
    {
        case PDMQUEUETYPE_DEV:
            while (pItems)
            {
                pCur = pItems;
                pItems = pItems->pNextHC;
                if (!pQueue->u.Dev.pfnCallback(pQueue->u.Dev.pDevIns, pCur))
                    break;
                pdmR3QueueFree(pQueue, pCur);
            }
            break;

        case PDMQUEUETYPE_DRV:
            while (pItems)
            {
                pCur = pItems;
                pItems = pItems->pNextHC;
                if (!pQueue->u.Drv.pfnCallback(pQueue->u.Drv.pDrvIns, pCur))
                    break;
                pdmR3QueueFree(pQueue, pCur);
            }
            break;

        case PDMQUEUETYPE_INTERNAL:
            while (pItems)
            {
                pCur = pItems;
                pItems = pItems->pNextHC;
                if (!pQueue->u.Int.pfnCallback(pQueue->pVMHC, pCur))
                    break;
                pdmR3QueueFree(pQueue, pCur);
            }
            break;

        case PDMQUEUETYPE_EXTERNAL:
            while (pItems)
            {
                pCur = pItems;
                pItems = pItems->pNextHC;
                if (!pQueue->u.Ext.pfnCallback(pQueue->u.Ext.pvUser, pCur))
                    break;
                pdmR3QueueFree(pQueue, pCur);
            }
            break;

        default:
            AssertMsgFailed(("Invalid queue type %d\n", pQueue->enmType));
            break;
    }

    /*
     * Success?
     */
    if (pItems)
    {
        /*
         * Shit, no!
         *      1. Insert pCur.
         *      2. Reverse the list.
         *      3. Insert the LIFO at the tail of the pending list.
         */
        pCur->pNextHC = pItems;
        pItems = pCur;

        //pCur = pItems;
        pItems = NULL;
        while (pCur)
        {
            PPDMQUEUEITEMCORE pInsert = pCur;
            pCur = pCur->pNextHC;
            pInsert->pNextHC = pItems;
            pItems = pInsert;
        }

        if (    pQueue->pPendingHC
            ||  !ASMAtomicCmpXchgPtr((void * volatile *)&pQueue->pPendingHC, pItems, NULL))
        {
            pCur = pQueue->pPendingHC;
            while (pCur->pNextHC)
                pCur = pCur->pNextHC;
            pCur->pNextHC = pItems;
        }
        return false;
    }

    return true;
}


/**
 * This is a worker function used by PDMQueueFlush to perform the
 * flush in ring-3.
 *
 * The queue which should be flushed is pointed to by either pQueueFlushGC,
 * pQueueFlushHC, or pQueueue. This function will flush that queue and
 * recalc the queue FF.
 *
 * @param   pVM     The VM handle.
 * @param   pQueue  The queue to flush. Only used in Ring-3.
 */
PDMR3DECL(void) PDMR3QueueFlushWorker(PVM pVM, PPDMQUEUE pQueue)
{
    Assert(pVM->pdm.s.pQueueFlushHC || pVM->pdm.s.pQueueFlushGC || pQueue);
    VM_ASSERT_EMT(pVM);

    /*
     * Flush the queue.
     */
    if (!pQueue)
        pQueue = pVM->pdm.s.pQueueFlushHC;
    if (!pQueue)
        pQueue = (PPDMQUEUE)MMHyperGC2HC(pVM, pVM->pdm.s.pQueueFlushGC);
    pVM->pdm.s.pQueueFlushHC = NULL;
    pVM->pdm.s.pQueueFlushGC = 0;
    if (    !pQueue
        ||  pdmR3QueueFlush(pQueue))
    {
        /*
         * Recalc the FF.
         */
        VM_FF_CLEAR(pVM, VM_FF_PDM_QUEUES);
        for (pQueue = pVM->pdm.s.pQueuesForced; pQueue; pQueue = pQueue->pNext)
            if (    pQueue->pPendingGC
                ||  pQueue->pPendingHC)
            {
                VM_FF_SET(pVM, VM_FF_PDM_QUEUES);
                break;
            }
    }
}


/**
 * Free an item.
 *
 * @param   pQueue  The queue.
 * @param   pItem   The item.
 */
static inline void pdmR3QueueFree(PPDMQUEUE pQueue, PPDMQUEUEITEMCORE pItem)
{
    VM_ASSERT_EMT(pQueue->pVMHC);
    int i = pQueue->iFreeHead;
    int iNext = (i + 1) % (pQueue->cItems + PDMQUEUE_FREE_SLACK);
    pQueue->aFreeItems[i].pItemHC = pItem;
    if (pQueue->pVMGC)
        pQueue->aFreeItems[i].pItemGC = MMHyperHC2GC(pQueue->pVMHC, pItem);
    if (!ASMAtomicCmpXchgU32(&pQueue->iFreeHead, iNext, i))
        AssertMsgFailed(("huh? i=%d iNext=%d iFreeHead=%d iFreeTail=%d\n", i, iNext, pQueue->iFreeHead, pQueue->iFreeTail));
}


/**
 * Timer handler for PDM queues.
 * This is called by for a single queue.
 *
 * @param   pVM     VM handle.
 * @param   pTimer  Pointer to timer.
 * @param   pvUser  Pointer to the queue.
 */
static DECLCALLBACK(void) pdmR3QueueTimer(PVM pVM, PTMTIMER pTimer, void *pvUser)
{
    PPDMQUEUE pQueue = (PPDMQUEUE)pvUser;
    Assert(pTimer == pQueue->pTimer); NOREF(pTimer);

    if (    pQueue->pPendingHC
        ||  pQueue->pPendingGC)
        pdmR3QueueFlush(pQueue);
    int rc = TMTimerSetMillies(pQueue->pTimer, pQueue->cMilliesInterval);
    AssertRC(rc);
}

