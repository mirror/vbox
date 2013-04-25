/** @file
 * IPRT - Generic Work Queue with concurrent access.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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

#ifndef ___iprt_workqueue_h
#define ___iprt_workqueue_h

#include <iprt/types.h>
#include <iprt/asm.h>

/** @defgroup grp_rt_list    RTWorkQueue - Generic Work Queue
 * @ingroup grp_rt
 *
 * Implementation of a lockless work queue for threaded environments.
 * @{
 */

RT_C_DECLS_BEGIN

/**
 * A work item
 */
typedef struct RTWORKITEM
{
    /** Pointer to the next work item in the list. */
    struct RTWORKITEM * volatile pNext;
} RTWORKITEM;
/** Pointer to a work item. */
typedef RTWORKITEM *PRTWORKITEM;
/** Pointer to a work item pointer. */
typedef PRTWORKITEM *PPRTWORKITEM;

/**
 * Work queue.
 */
typedef struct RTWORKQUEUE
{
    /* Head of the work queue. */
    volatile PRTWORKITEM        pHead;
} RTWORKQUEUE;
/** Pointer to a work queue. */
typedef RTWORKQUEUE *PRTWORKQUEUE;

/**
 * Initialize a work queue.
 *
 * @param   pWorkQueue          Pointer to an unitialised work queue.
 */
DECLINLINE(void) RTWorkQueueInit(PRTWORKQUEUE pWorkQueue)
{
    ASMAtomicWriteNullPtr(&pWorkQueue->pHead);
}

/**
 * Insert a new item into the work queue.
 *
 * @param   pWorkQueue          The work queue to insert into.
 * @param   pItem               The item to insert.
 */
DECLINLINE(void) RTWorkQueueInsert(PRTWORKQUEUE pWorkQueue, PRTWORKITEM pItem)
{
    PRTWORKITEM pNext = ASMAtomicUoReadPtrT(&pWorkQueue->pHead, PRTWORKITEM);
    PRTWORKITEM pHeadOld;
    pItem->pNext = pNext;
    while (!ASMAtomicCmpXchgExPtr(&pWorkQueue->pHead, pItem, pNext, &pHeadOld))
    {
        pNext = pHeadOld;
        Assert(pNext != pItem);
        pItem->pNext = pNext;
        ASMNopPause();
    }
}

/**
 * Remove all items from the given work queue and return them in the inserted order.
 *
 * @returns Pointer to the first item.
 * @param   pWorkQueue          The work queue.
 */
DECLINLINE(PRTWORKITEM) RTWorkQueueRemoveAll(PRTWORKQUEUE pWorkQueue)
{
    PRTWORKITEM pHead = ASMAtomicXchgPtrT(&pWorkQueue->pHead, NULL, PRTWORKITEM);

    /* Reverse it. */
    PRTWORKITEM pCur = pHead;
    pHead = NULL;
    while (pCur)
    {
        PRTWORKITEM pInsert = pCur;
        pCur = pCur->pNext;
        pInsert->pNext = pHead;
        pHead = pInsert;
    }

    return pHead;
}

RT_C_DECLS_END

/** @} */

#endif
