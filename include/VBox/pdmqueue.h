/** @file
 * PDM - Pluggable Device Manager, Queues.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
    /** Pointer to the next item in the pending list - R3 Pointer. */
    R3PTRTYPE(PPDMQUEUEITEMCORE)    pNextR3;
    /** Pointer to the next item in the pending list - R0 Pointer. */
    R0PTRTYPE(PPDMQUEUEITEMCORE)    pNextR0;
    /** Pointer to the next item in the pending list - RC Pointer. */
    RCPTRTYPE(PPDMQUEUEITEMCORE)    pNextRC;
#if HC_ARCH_BITS == 64
    RTRCPTR                         Alignment0;
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

PDMR3DECL(int)  PDMR3QueueCreateDevice(PVM pVM, PPDMDEVINS pDevIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                       PFNPDMQUEUEDEV pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue);
PDMR3DECL(int)  PDMR3QueueCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                       PFNPDMQUEUEDRV pfnCallback, PPDMQUEUE *ppQueue);
PDMR3DECL(int)  PDMR3QueueCreateInternal(PVM pVM, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                         PFNPDMQUEUEINT pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue);
PDMR3DECL(int)  PDMR3QueueCreateExternal(PVM pVM, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                         PFNPDMQUEUEEXT pfnCallback, void *pvUser, PPDMQUEUE *ppQueue);
PDMR3DECL(int)  PDMR3QueueDestroy(PPDMQUEUE pQueue);
PDMR3DECL(int)  PDMR3QueueDestroyDevice(PVM pVM, PPDMDEVINS pDevIns);
PDMR3DECL(int)  PDMR3QueueDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns);
PDMR3DECL(void) PDMR3QueueFlushAll(PVM pVM);
PDMR3DECL(void) PDMR3QueueFlushWorker(PVM pVM, PPDMQUEUE pQueue);

PDMDECL(void)                 PDMQueueFlush(PPDMQUEUE pQueue);
PDMDECL(PPDMQUEUEITEMCORE)    PDMQueueAlloc(PPDMQUEUE pQueue);
PDMDECL(void)                 PDMQueueInsert(PPDMQUEUE pQueue, PPDMQUEUEITEMCORE pItem);
PDMDECL(void)                 PDMQueueInsertEx(PPDMQUEUE pQueue, PPDMQUEUEITEMCORE pItem, uint64_t NanoMaxDelay);
PDMDECL(RCPTRTYPE(PPDMQUEUE)) PDMQueueRCPtr(PPDMQUEUE pQueue);
PDMDECL(R0PTRTYPE(PPDMQUEUE)) PDMQueueR0Ptr(PPDMQUEUE pQueue);

/** @} */

__END_DECLS

#endif


