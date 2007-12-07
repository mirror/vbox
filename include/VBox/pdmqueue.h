/** @file
 * PDM - Pluggable Device Manager, Queues.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_pdmqueue_h
#define ___VBox_pdmqueue_h

#include <VBox/types.h>

__BEGIN_DECLS

/** @defgroup grp_pdm_queue     Queues
 * @ingroup grp_pdm
 * @{
 */

/** Pointer to a PDM queue. Also called PDM queue handle. */
typedef struct PDMQUEUE *PPDMQUEUE;

/** Pointer to a PDM queue item core. */
typedef struct PDMQUEUEITEMCORE *PPDMQUEUEITEMCORE;

/**
 * PDM queue item core.
 */
typedef struct PDMQUEUEITEMCORE
{
    /** Pointer to the next item in the pending list - HC Pointer. */
    R3R0PTRTYPE(PPDMQUEUEITEMCORE)  pNextHC;
    /** Pointer to the next item in the pending list - GC Pointer. */
    GCPTRTYPE(PPDMQUEUEITEMCORE)    pNextGC;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32
    uint32_t                        Alignment0;
#endif
} PDMQUEUEITEMCORE;


/**
 * Queue consumer callback for devices.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pDevIns     The device instance.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
typedef DECLCALLBACK(bool) FNPDMQUEUEDEV(PPDMDEVINS pDevIns, PPDMQUEUEITEMCORE pItem);
/** Pointer to a FNPDMQUEUEDEV(). */
typedef FNPDMQUEUEDEV *PFNPDMQUEUEDEV;

/**
 * Queue consumer callback for drivers.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pDrvIns     The driver instance.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
typedef DECLCALLBACK(bool) FNPDMQUEUEDRV(PPDMDRVINS pDrvIns, PPDMQUEUEITEMCORE pItem);
/** Pointer to a FNPDMQUEUEDRV(). */
typedef FNPDMQUEUEDRV *PFNPDMQUEUEDRV;

/**
 * Queue consumer callback for internal component.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pVM         The VM handle.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
typedef DECLCALLBACK(bool) FNPDMQUEUEINT(PVM pVM, PPDMQUEUEITEMCORE pItem);
/** Pointer to a FNPDMQUEUEINT(). */
typedef FNPDMQUEUEINT *PFNPDMQUEUEINT;

/**
 * Queue consumer callback for external component.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pvUser      User argument.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
typedef DECLCALLBACK(bool) FNPDMQUEUEEXT(void *pvUser, PPDMQUEUEITEMCORE pItem);
/** Pointer to a FNPDMQUEUEEXT(). */
typedef FNPDMQUEUEEXT *PFNPDMQUEUEEXT;

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
                                      PFNPDMQUEUEDEV pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue);

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
 * @thread  The emulation thread.
 */
PDMR3DECL(int) PDMR3QueueCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                      PFNPDMQUEUEDRV pfnCallback, PPDMQUEUE *ppQueue);

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
                                        PFNPDMQUEUEINT pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue);

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
 * @thread  The emulation thread.
 */
PDMR3DECL(int) PDMR3QueueCreateExternal(PVM pVM, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                        PFNPDMQUEUEEXT pfnCallback, void *pvUser, PPDMQUEUE *ppQueue);

/**
 * Destroy a queue.
 *
 * @returns VBox status code.
 * @param   pQueue      Queue to destroy.
 * @thread  The emulation thread.
 */
PDMR3DECL(int) PDMR3QueueDestroy(PPDMQUEUE pQueue);

/**
 * Destroy a all queues owned by the specified device.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pDevIns     Device instance.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueDestroyDevice(PVM pVM, PPDMDEVINS pDevIns);

/**
 * Destroy a all queues owned by the specified driver.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pDrvIns     Driver instance.
 * @thread  Emulation thread only.
 */
PDMR3DECL(int) PDMR3QueueDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns);

/**
 * Flushes pending queues.
 * This is a forced action callback.
 *
 * @param   pVM     VM handle.
 * @thread  The emulation thread.
 */
PDMR3DECL(void) PDMR3QueueFlushAll(PVM pVM);

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
PDMR3DECL(void) PDMR3QueueFlushWorker(PVM pVM, PPDMQUEUE pQueue);

/**
 * Flushes a PDM queue.
 *
 * @param   pQueue          The queue handle.
 */
PDMDECL(void) PDMQueueFlush(PPDMQUEUE pQueue);

/**
 * Allocate an item from a queue.
 * The allocated item must be handed on to PDMQueueInsert() after the
 * data has been filled in.
 *
 * @returns Pointer to allocated queue item.
 * @returns NULL on failure. The queue is exhausted.
 * @param   pQueue      The queue handle.
 * @thread  Any thread.
 */
PDMDECL(PPDMQUEUEITEMCORE) PDMQueueAlloc(PPDMQUEUE pQueue);

/**
 * Queue an item.
 * The item must have been obtained using PDMQueueAlloc(). Once the item
 * has been passed to this function it must not be touched!
 *
 * @param   pQueue      The queue handle.
 * @param   pItem       The item to insert.
 * @thread  Any thread.
 */
PDMDECL(void) PDMQueueInsert(PPDMQUEUE pQueue, PPDMQUEUEITEMCORE pItem);

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
PDMDECL(void) PDMQueueInsertEx(PPDMQUEUE pQueue, PPDMQUEUEITEMCORE pItem, uint64_t NanoMaxDelay);


/**
 * Gets the GC pointer for the specified queue.
 *
 * @returns The GC address of the queue.
 * @returns NULL if pQueue is invalid.
 * @param   pQueue          The queue handle.
 */
PDMDECL(GCPTRTYPE(PPDMQUEUE)) PDMQueueGCPtr(PPDMQUEUE pQueue);

/** @} */

__END_DECLS

#endif


