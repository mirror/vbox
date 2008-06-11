/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Ring-0 Driver, Event Notifications.
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
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/mp.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/spinlock.h>
#include <iprt/string.h>
#include "r0drv/mp-r0drv.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Notification registration record tracking
 * RTMpRegisterNotification() calls.
 */
typedef struct RTMPNOTIFYREG
{
    /** Pointer to the next record. */
    struct RTMPNOTIFYREG * volatile pNext;
    /** The callback. */
    PFNRTMPNOTIFICATION pfnCallback;
    /** The user argument. */
    void *pvUser;
    /** Bit mask indicating whether we've done this callback or not. */
    uint8_t bmDone[sizeof(void *)];
} RTMPNOTIFYREG;
/** Pointer to a registration record. */
typedef RTMPNOTIFYREG *PRTMPNOTIFYREG;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The spinlock protecting the list. */
static RTSPINLOCK volatile g_hRTMpNotifySpinLock = NIL_RTSPINLOCK;
/** List of callbacks, in registration order. */
static PRTMPNOTIFYREG volatile g_pRTMpCallbackHead = NULL;
/** The current done bit. */
static uint32_t volatile g_iRTMpDoneBit;
/** The list generation.
 * This is increased whenever the list has been modified. The callback routine
 * make use of this to avoid having restart at the list head after each callback. */
static uint32_t volatile g_iRTMpGeneration;
/** The number of RTMpNotification users.
 * This is incremented on init and decremented on termination. */
static uint32_t volatile g_cRTMpUsers = 0;




/**
 * This is called by the native code.
 *
 * @param   idCpu           The CPU id the event applies to.
 * @param   enmEvent        The event.
 */
void rtMpNotificationDoCallbacks(RTMPEVENT enmEvent, RTCPUID idCpu)
{
    PRTMPNOTIFYREG pCur;
    RTSPINLOCKTMP Tmp;
    RTSPINLOCK hSpinlock;

    /*
     * This is a little bit tricky as we cannot be holding the spinlock
     * while calling the callback. This means that the list might change
     * while we're walking it, and that multiple events might be running
     * concurrently (depending on the OS).
     *
     * So, the first measure is to employ a 32-bitmask for each
     * record where we'll use a bit that rotates for each call to
     * this function to indicate which records that has been
     * processed. This will take care of both changes to the list
     * and a reasonable amount of concurrent events.
     *
     * In order to avoid having to restart the list walks for every
     * callback we make, we'll make use a list generation number that is
     * incremented everytime the list is changed. So, if it remains
     * unchanged over a callback we can safely continue the iteration.
     */
    uint32_t iDone = ASMAtomicIncU32(&g_iRTMpDoneBit);
    iDone %= RT_SIZEOFMEMB(RTMPNOTIFYREG, bmDone) * 8;

    hSpinlock = g_hRTMpNotifySpinLock;
    if (hSpinlock == NIL_RTSPINLOCK)
        return;
    RTSpinlockAcquire(hSpinlock, &Tmp);

    /* Clear the bit. */
    for (pCur = g_pRTMpCallbackHead; pCur; pCur = pCur->pNext)
        ASMAtomicBitClear(&pCur->bmDone[0], iDone);

    /* Iterate the records and perform the callbacks. */
    do
    {
        uint32_t const iGeneration = ASMAtomicUoReadU32(&g_iRTMpGeneration);

        pCur = g_pRTMpCallbackHead;
        while (pCur)
        {
            if (!ASMAtomicBitTestAndSet(&pCur->bmDone[0], iDone))
            {
                PFNRTMPNOTIFICATION pfnCallback = pCur->pfnCallback;
                void *pvUser = pCur->pvUser;
                pCur = pCur->pNext;
                RTSpinlockRelease(g_hRTMpNotifySpinLock, &Tmp);

                pfnCallback(enmEvent, idCpu, pvUser);

                /* carefully require the lock here, see RTR0MpNotificationTerm(). */
                hSpinlock = g_hRTMpNotifySpinLock;
                if (hSpinlock == NIL_RTSPINLOCK)
                    return;
                RTSpinlockAcquire(hSpinlock, &Tmp);
                if (ASMAtomicUoReadU32(&g_iRTMpGeneration) != iGeneration)
                    break;
            }
            else
                pCur = pCur->pNext;
        }
    } while (pCur);

    RTSpinlockRelease(hSpinlock, &Tmp);
}



RTDECL(int) RTMpNotificationRegister(PFNRTMPNOTIFICATION pfnCallback, void *pvUser)
{
    PRTMPNOTIFYREG pCur;
    PRTMPNOTIFYREG pNew;
    RTSPINLOCKTMP Tmp;

    /*
     * Validation.
     */
    AssertPtrReturn(pfnCallback, VERR_INVALID_POINTER);
    AssertReturn(g_hRTMpNotifySpinLock != NIL_RTSPINLOCK, VERR_WRONG_ORDER);

    RTSpinlockAcquire(g_hRTMpNotifySpinLock, &Tmp);
    for (pCur = g_pRTMpCallbackHead; pCur; pCur = pCur->pNext)
        if (    pCur->pvUser == pvUser
            &&  pCur->pfnCallback == pfnCallback)
            break;
    RTSpinlockRelease(g_hRTMpNotifySpinLock, &Tmp);
    AssertMsgReturn(!pCur, ("pCur=%p pfnCallback=%p pvUser=%p\n", pCur, pfnCallback, pvUser), VERR_ALREADY_EXISTS);

    /*
     * Allocate a new record and attempt to insert it.
     */
    pNew = (PRTMPNOTIFYREG)RTMemAlloc(sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;

    pNew->pNext = NULL;
    pNew->pfnCallback = pfnCallback;
    pNew->pvUser = pvUser;
    memset(&pNew->bmDone[0], 0xff, sizeof(pNew->bmDone));

    RTSpinlockAcquire(g_hRTMpNotifySpinLock, &Tmp);

    pCur = g_pRTMpCallbackHead;
    if (!pCur)
        g_pRTMpCallbackHead = pNew;
    else
    {
        for (pCur = g_pRTMpCallbackHead; ; pCur = pCur->pNext)
            if (    pCur->pvUser == pvUser
                &&  pCur->pfnCallback == pfnCallback)
                break;
            else if (!pCur->pNext)
            {
                pCur->pNext = pNew;
                pCur = NULL;
                break;
            }
    }

    ASMAtomicIncU32(&g_iRTMpGeneration);

    RTSpinlockRelease(g_hRTMpNotifySpinLock, &Tmp);

    /* duplicate? */
    if (pCur)
    {
        RTMemFree(pCur);
        AssertMsgFailedReturn(("pCur=%p pfnCallback=%p pvUser=%p\n", pCur, pfnCallback, pvUser), VERR_ALREADY_EXISTS);
    }

    return VINF_SUCCESS;
}



RTDECL(int) RTMpNotificationDeregister(PFNRTMPNOTIFICATION pfnCallback, void *pvUser)
{
    PRTMPNOTIFYREG pPrev;
    PRTMPNOTIFYREG pCur;
    RTSPINLOCKTMP Tmp;

    /*
     * Validation.
     */
    AssertPtrReturn(pfnCallback, VERR_INVALID_POINTER);
    AssertReturn(g_hRTMpNotifySpinLock != NIL_RTSPINLOCK, VERR_WRONG_ORDER);

    /*
     * Find and unlink the record from the list.
     */
    RTSpinlockAcquire(g_hRTMpNotifySpinLock, &Tmp);
    pPrev = NULL;
    for (pCur = g_pRTMpCallbackHead; pCur; pCur = pCur->pNext)
    {
        if (    pCur->pvUser == pvUser
            &&  pCur->pfnCallback == pfnCallback)
            break;
        pPrev = pCur;
    }
    if (pCur)
    {
        if (pPrev)
            pPrev->pNext = pCur->pNext;
        else
            g_pRTMpCallbackHead = pCur->pNext;
        ASMAtomicIncU32(&g_iRTMpGeneration);
    }
    RTSpinlockRelease(g_hRTMpNotifySpinLock, &Tmp);

    if (!pCur)
        return VERR_NOT_FOUND;

    /*
     * Invalidate and free the record.
     */
    pCur->pNext = NULL;
    pCur->pfnCallback = NULL;
    RTMemFree(pCur);

    return VINF_SUCCESS;
}


int rtR0MpNotificationInit(void)
{
    int rc = VINF_SUCCESS;

    if (ASMAtomicIncS32(&g_cRTMpUsers) == 1)
    {
        rc = RTSpinlockCreate((PRTSPINLOCK)&g_hRTMpNotifySpinLock);
        if (RT_SUCCESS(rc))
        {
            rc = rtR0MpNotificationNativeInit();
            if (RT_SUCCESS(rc))
                return rc;

            RTSpinlockDestroy(g_hRTMpNotifySpinLock);
            g_hRTMpNotifySpinLock = NIL_RTSPINLOCK;
        }
        ASMAtomicDecS32(&g_cRTMpUsers);
    }
    return rc;
}


void rtR0MpNotificationTerm(void)
{
    RTSPINLOCK hSpinlock = g_hRTMpNotifySpinLock;
    if (hSpinlock != NIL_RTSPINLOCK)
    {
        AssertMsg(g_cRTMpUsers > 0, ("%d\n", g_cRTMpUsers));
        if (ASMAtomicDecS32(&g_cRTMpUsers) == 0)
        {

            PRTMPNOTIFYREG pHead;
            RTSPINLOCKTMP Tmp;

            rtR0MpNotificationNativeTerm();

            /* pick up the list and the spinlock. */
            RTSpinlockAcquire(hSpinlock, &Tmp);
            ASMAtomicWriteSize(&g_hRTMpNotifySpinLock, NIL_RTSPINLOCK);
            pHead = g_pRTMpCallbackHead;
            g_pRTMpCallbackHead = NULL;
            ASMAtomicIncU32(&g_iRTMpGeneration);
            RTSpinlockRelease(hSpinlock, &Tmp);

            /* free the list. */
            while (pHead)
            {
                PRTMPNOTIFYREG pFree = pHead;
                pHead = pHead->pNext;

                pFree->pNext = NULL;
                pFree->pfnCallback = NULL;
                RTMemFree(pFree);
            }

            RTSpinlockDestroy(g_hRTMpNotifySpinLock);
            g_hRTMpNotifySpinLock = NULL;
        }
    }
}


