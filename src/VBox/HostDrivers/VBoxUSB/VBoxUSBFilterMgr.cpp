/* $Id$ */
/** @file
 * VirtualBox Ring-0 USB Filter Manager.
 */

/*
 * Copyright (C) 2007 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/usbfilter.h>
#include "VBoxUSBFilterMgr.h"

#include <iprt/mem.h>
#ifdef VBOXUSBFILTERMGR_USB_SPINLOCK
# include <iprt/spinlock.h>
#else
# include <iprt/semaphore.h>
#endif
#include <iprt/string.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** @def VBOXUSBFILTERMGR_LOCK
 * Locks the filter list. Careful with scoping since this may
 * create a temporary variable. Don't call twice in the same function.
 */

/** @def VBOXUSBFILTERMGR_UNLOCK
 * Unlocks the filter list.
 */
#ifdef VBOXUSBFILTERMGR_USB_SPINLOCK

# define VBOXUSBFILTERMGR_LOCK() \
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER; \
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp)

# define VBOXUSBFILTERMGR_UNLOCK() \
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp)

#else

# define VBOXUSBFILTERMGR_LOCK() \
    do { int rc2 = RTSemFastMutexRequest(g_Mtx); AssertRC(rc2); } while (0)

# define VBOXUSBFILTERMGR_UNLOCK() \
    do { int rc2 = RTSemFastMutexRelease(g_Mtx); AssertRC(rc2); } while (0)

#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Pointer to an VBoxUSB filter. */
typedef struct VBOXUSBFILTER *PVBOXUSBFILTER;
/** Pointer to PVBOXUSBFILTER. */
typedef PVBOXUSBFILTER *PPVBOXUSBFILTER;

/**
 * VBoxUSB internal filter represenation.
 */
typedef struct VBOXUSBFILTER
{
    /** The core filter. */
    USBFILTER       Core;
    /** The filter owner. */
    RTPROCESS       Owner;
    /** The filter Id. */
    uintptr_t       uId;
    /** Pointer to the next filter in the list. */
    PVBOXUSBFILTER  pNext;
} VBOXUSBFILTER;

/**
 * VBoxUSB filter list.
 */
typedef struct VBOXUSBFILTERLIST
{
    /** The head pointer. */
    PVBOXUSBFILTER      pHead;
    /** The tail pointer. */
    PVBOXUSBFILTER      pTail;
} VBOXUSBFILTERLIST;
/** Pointer to a VBOXUSBFILTERLIST. */
typedef VBOXUSBFILTERLIST *PVBOXUSBFILTERLIST;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef VBOXUSBFILTERMGR_USB_SPINLOCK
/** Spinlock protecting the filter lists. */
static RTSPINLOCK           g_Spinlock = NIL_RTSPINLOCK;
#else
/** Mutex protecting the filter lists. */
static RTSEMFASTMUTEX       g_Mtx = NIL_RTSEMFASTMUTEX;
#endif
/** The per-type filter lists.
 * @remark The first entry is empty (USBFILTERTYPE_INVALID). */
static VBOXUSBFILTERLIST    g_aLists[USBFILTERTYPE_END];



/**
 * Initalizes the VBoxUSB filter manager.
 *
 * @returns IPRT status code.
 */
int VBoxUSBFilterInit(void)
{
#ifdef VBOXUSBFILTERMGR_USB_SPINLOCK
    int rc = RTSpinlockCreate(&g_Spinlock);
#else
    int rc = RTSemFastMutexCreate(&g_Mtx);
#endif
    if (RT_SUCCESS(rc))
    {
        /* not really required, but anyway... */
        for (unsigned i = USBFILTERTYPE_FIRST; i < RT_ELEMENTS(g_aLists); i++)
            g_aLists[i].pHead = g_aLists[i].pTail = NULL;
    }
    return rc;
}


/**
 * Internal worker that frees a filter.
 *
 * @param   pFilter     The filter to free.
 */
static void vboxUSBFilterFree(PVBOXUSBFILTER pFilter)
{
    USBFilterDelete(&pFilter->Core);
    pFilter->Owner = NIL_RTPROCESS;
    pFilter->pNext = NULL;
    RTMemFree(pFilter);
}


/**
 * Terminates the VBoxUSB filter manager.
 */
void VBoxUSBFilterTerm(void)
{
#ifdef VBOXUSBFILTERMGR_USB_SPINLOCK
    RTSpinlockDestroy(g_Spinlock);
    g_Spinlock = NIL_RTSPINLOCK;
#else
    RTSemFastMutexDestroy(g_Mtx);
    g_Mtx = NIL_RTSEMFASTMUTEX;
#endif

    for (unsigned i = USBFILTERTYPE_FIRST; i < RT_ELEMENTS(g_aLists); i++)
    {
        PVBOXUSBFILTER pCur = g_aLists[i].pHead;
        g_aLists[i].pHead = g_aLists[i].pTail = NULL;
        while (pCur)
        {
            PVBOXUSBFILTER pNext = pCur->pNext;
            vboxUSBFilterFree(pCur);
            pCur = pNext;
        }
    }
}


/**
 * Adds a new filter.
 *
 * The filter will be validate, duplicated and added.
 *
 * @returns IPRT status code.
 * @param   pFilter     The filter.
 * @param   Owner       The filter owner. Must be non-zero.
 * @param   puId        Where to store the filter ID.
 */
int VBoxUSBFilterAdd(PCUSBFILTER pFilter, RTPROCESS Owner, uintptr_t *puId)
{
    /*
     * Validate input.
     */
    int rc = USBFilterValidate(pFilter);
    if (RT_FAILURE(rc))
        return rc;
    if (!Owner || Owner == NIL_RTPROCESS)
        return VERR_INVALID_PARAMETER;
    if (!VALID_PTR(puId))
        return VERR_INVALID_POINTER;

    /*
     * Allocate a new filter.
     */
    PVBOXUSBFILTER pNew = (PVBOXUSBFILTER)RTMemAlloc(sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;
    memcpy(&pNew->Core, pFilter, sizeof(pNew->Core));
    pNew->Owner = Owner;
    pNew->uId   = (uintptr_t)pNew;
    pNew->pNext = NULL;

    *puId = pNew->uId;

    /*
     * Insert it.
     */
    PVBOXUSBFILTERLIST pList = &g_aLists[pFilter->enmType];

    VBOXUSBFILTERMGR_LOCK();

    if (pList->pTail)
        pList->pTail->pNext = pNew;
    else
        pList->pHead = pNew;
    pList->pTail = pNew;

    VBOXUSBFILTERMGR_UNLOCK();

    return VINF_SUCCESS;
}


/**
 * Removes an existing filter.
 *
 * The filter will be validate, duplicated and added.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS if successfully removed.
 * @retval  VERR_FILE_NOT_FOUND if the specified filter/owner cannot be found.
 *
 * @param   Owner       The filter owner.
 * @param   uId         The ID of the filter that's to be removed.
 *                      Returned by VBoxUSBFilterAdd().
 */
int VBoxUSBFilterRemove(RTPROCESS Owner, uintptr_t uId)
{
    /*
     * Validate input.
     */
    if (!uId)
        return VERR_INVALID_PARAMETER;
    if (!Owner || Owner == NIL_RTPROCESS)
        return VERR_INVALID_PARAMETER;

    /*
     * Locate and unlink it.
     */
    PVBOXUSBFILTER pCur = NULL;

    VBOXUSBFILTERMGR_LOCK();

    for (unsigned i = USBFILTERTYPE_FIRST; !pCur && i < RT_ELEMENTS(g_aLists); i++)
    {
        PVBOXUSBFILTER pPrev = NULL;
        pCur = g_aLists[i].pHead;
        while (pCur)
        {
            if (    pCur->uId == uId
                &&  pCur->Owner == Owner)
            {
                PVBOXUSBFILTER pNext = pCur->pNext;
                if (pPrev)
                    pPrev->pNext = pNext;
                else
                    g_aLists[i].pHead = pNext;
                if (!pNext)
                    g_aLists[i].pTail = pPrev;
                break;
            }

            pPrev = pCur;
            pCur = pCur->pNext;
        }
    }

    VBOXUSBFILTERMGR_UNLOCK();

    /*
     * Free it (if found).
     */
    if (pCur)
    {
        vboxUSBFilterFree(pCur);
        return VINF_SUCCESS;
    }

    return VERR_FILE_NOT_FOUND;
}


/**
 * Removes all filters belonging to the specified owner.
 *
 * This is typically called when an owner disconnects or
 * terminates unexpectedly.
 *
 * @param   Owner       The owner
 */
void VBoxUSBFilterRemoveOwner(RTPROCESS Owner)
{
    /*
     * Collect the filters that should be freed.
     */
    PVBOXUSBFILTER pToFree = NULL;

    VBOXUSBFILTERMGR_LOCK();

    for (unsigned i = USBFILTERTYPE_FIRST; i < RT_ELEMENTS(g_aLists); i++)
    {
        PVBOXUSBFILTER pPrev = NULL;
        PVBOXUSBFILTER pCur = g_aLists[i].pHead;
        while (pCur)
        {
            if (pCur->Owner == Owner)
            {
                PVBOXUSBFILTER pNext = pCur->pNext;
                if (pPrev)
                    pPrev->pNext = pNext;
                else
                    g_aLists[i].pHead = pNext;
                if (!pNext)
                    g_aLists[i].pTail = pPrev;

                pCur->pNext = pToFree;
                pToFree = pCur;

                pCur = pNext;
            }
            else
            {
                pPrev = pCur;
                pCur = pCur->pNext;
            }
        }
    }

    VBOXUSBFILTERMGR_UNLOCK();

    /*
     * Free any filters we've found.
     */
    while (pToFree)
    {
        PVBOXUSBFILTER pNext = pToFree->pNext;
        vboxUSBFilterFree(pToFree);
        pToFree = pNext;
    }
}


/**
 * Match the specified device against the filters.
 *
 * @returns Owner on if matched, NIL_RTPROCESS it not matched.
 * @param   pDevice     The device data as a filter structure.
 *                      See USBFilterMatch for how to construct this.
 * @param   puId        Where to store the filter id (optional).
 */
RTPROCESS VBoxUSBFilterMatch(PCUSBFILTER pDevice, uintptr_t *puId)
{
    /*
     * Validate input.
     */
    int rc = USBFilterValidate(pDevice);
    AssertRCReturn(rc, NIL_RTPROCESS);

    /*
     * Search the lists for a match.
     * (The lists are ordered by priority.)
     */
    VBOXUSBFILTERMGR_LOCK();

    for (unsigned i = USBFILTERTYPE_FIRST; i < RT_ELEMENTS(g_aLists); i++)
    {
        PVBOXUSBFILTER pPrev = NULL;
        PVBOXUSBFILTER pCur = g_aLists[i].pHead;
        while (pCur)
        {
            if (USBFilterMatch(&pCur->Core, pDevice))
            {
                /*
                 * Take list specific actions and return.
                 *
                 * The code does NOT implement the case where there are two or more
                 * filter clients, and one of them is releaseing a device that's
                 * requested by some of the others. It's just too much work for a
                 * situation that noone will encounter.
                 */
                if (puId)
                    *puId = pCur->uId;
                RTPROCESS Owner = i != USBFILTERTYPE_IGNORE
                               && i != USBFILTERTYPE_ONESHOT_IGNORE
                                ? pCur->Owner
                                : NIL_RTPROCESS;

                if (    i == USBFILTERTYPE_ONESHOT_IGNORE
                    ||  i == USBFILTERTYPE_ONESHOT_CAPTURE)
                {
                    /* unlink */
                    PVBOXUSBFILTER pNext = pCur->pNext;
                    if (pPrev)
                        pPrev->pNext = pNext;
                    else
                        g_aLists[i].pHead = pNext;
                    if (!pNext)
                        g_aLists[i].pTail = pPrev;
                }

                VBOXUSBFILTERMGR_UNLOCK();

                if (    i == USBFILTERTYPE_ONESHOT_IGNORE
                    ||  i == USBFILTERTYPE_ONESHOT_CAPTURE)
                    vboxUSBFilterFree(pCur);
                return Owner;
            }

            pPrev = pCur;
            pCur = pCur->pNext;
        }
    }

    VBOXUSBFILTERMGR_UNLOCK();
    return NIL_RTPROCESS;
}

