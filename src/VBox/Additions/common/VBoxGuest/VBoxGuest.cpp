/** $Id: */
/** @file
 * VBoxGuest - Guest Additions Driver.
 */

/*
 * Copyright (C) 2007 innotek GmbH
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
#define LOG_GROUP   LOG_GROUP_DEFAULT
#include "VBoxGuestInternal.h"
#include <VBox/VBoxDev.h> /* for VMMDEV_RAM_SIZE */
#include <VBox/log.h>
#include <iprt/mem.h>
#include <iprt/time.h>
#include <iprt/memobj.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/process.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#ifdef VBOX_HGCM
# include <iprt/thread.h>
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef VBOX_HGCM
static DECLCALLBACK(void) VBoxGuestHGCMAsyncWaitCallback(VMMDevHGCMRequestHeader *pHdrNonVolatile, void *pvUser, uint32_t u32User);
#endif



/**
 * Reserves memory in which the VMM can relocate any guest mappings
 * that are floating around.
 *
 * This operation is a little bit tricky since the VMM might not accept
 * just any address because of address clashes between the three contexts
 * it operates in, so use a small stack to perform this operation.
 *
 * @returns VBox status code (ignored).
 * @param   pDevExt     The device extension.
 */
static int vboxGuestInitFixateGuestMappings(PVBOXGUESTDEVEXT pDevExt)
{
    /** @todo implement this using RTR0MemObjReserveKernel() (it needs to be implemented everywhere too). */
    return VINF_SUCCESS;
}


/**
 * Initializes the interrupt filter mask.
 *
 * This will ASSUME that we're the ones in carge over the mask, so
 * we'll simply clear all bits we don't set.
 *
 * @returns VBox status code (ignored).
 * @param   pDevExt     The device extension.
 * @param   fMask       The new mask.
 */
static int vboxGuestInitFilterMask(PVBOXGUESTDEVEXT pDevExt, uint32_t fMask)
{
    VMMDevCtlGuestFilterMask *pReq;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(*pReq), VMMDevReq_CtlGuestFilterMask);
    if (RT_SUCCESS(rc))
    {
        pReq->u32OrMask = fMask;
        pReq->u32NotMask = ~fMask; /* It's an AND mask. */
        rc = VbglGRPerform(&pReq->header);
        if (    RT_FAILURE(rc)
            ||  RT_FAILURE(pReq->header.rc))
            LogRel(("vboxGuestInitCtlFilterMask: failed with rc=%Rrc and VMMDev rc=%Rrc\n",
                    rc, pReq->header.rc));
        VbglGRFree(&pReq->header);
    }
    return rc;
}


/**
 * Report guest information to the VMMDev.
 *
 * @returns VBox status code.
 * @param   pDevExt     The device extension.
 * @param   enmOSType   The OS type to report.
 */
static int vboxGuestInitReportGuestInfo(PVBOXGUESTDEVEXT pDevExt, VBOXOSTYPE enmOSType)
{
    VMMDevReportGuestInfo *pReq;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(*pReq), VMMDevReq_ReportGuestInfo);
    if (RT_SUCCESS(rc))
    {
        pReq->guestInfo.additionsVersion = VMMDEV_VERSION;
        pReq->guestInfo.osType = enmOSType;
        rc = VbglGRPerform(&pReq->header);
        if (    RT_FAILURE(rc)
            ||  RT_FAILURE(pReq->header.rc))
            LogRel(("vboxGuestInitReportGuestInfo: failed with rc=%Rrc and VMMDev rc=%Rrc\n",
                    rc, pReq->header.rc));
        VbglGRFree(&pReq->header);
    }
    return rc;
}


/**
 * Initializes the VBoxGuest device extension when the
 * device driver is loaded.
 *
 * The native code locates the VMMDev on the PCI bus and retrieve
 * the MMIO and I/O port ranges, this function will take care of
 * mapping the MMIO memory (if present). Upon successful return
 * the native code should set up the interrupt handler.
 *
 * @returns VBox status code.
 *
 * @param   pDevExt         The device extension. Allocated by the native code.
 * @param   IOPortBase      The base of the I/O port range.
 * @param   pvMMIOBase      The base of the MMIO memory mapping.
 *                          This is optional, pass NULL if not present.
 * @param   cbMMIO          The size of the MMIO memory mapping.
 *                          This is optional, pass 0 if not present.
 * @param   enmOSType       The guest OS type to report to the VMMDev.
 */
int VBoxGuestInitDevExt(PVBOXGUESTDEVEXT pDevExt, uint16_t IOPortBase,
                        void *pvMMIOBase, uint32_t cbMMIO, VBOXOSTYPE enmOSType)
{
    int rc, rc2;

    /*
     * Initalize the data.
     */
    pDevExt->IOPortBase = IOPortBase;
    pDevExt->pVMMDevMemory = NULL;
    pDevExt->pIrqAckEvents = NULL;
    pDevExt->WaitList.pHead = NULL;
    pDevExt->WaitList.pTail = NULL;
#ifdef VBOX_HGCM
    pDevExt->HGCMWaitList.pHead = NULL;
    pDevExt->HGCMWaitList.pTail = NULL;
#endif
    pDevExt->FreeList.pHead = NULL;
    pDevExt->FreeList.pTail = NULL;
    pDevExt->f32PendingEvents = 0;
    pDevExt->u32ClipboardClientId = 0;

    /*
     * If there is an MMIO region validate the version and size.
     */
    if (pvMMIOBase)
    {
        Assert(cbMMIO);
        VMMDevMemory *pVMMDev = (VMMDevMemory *)pvMMIOBase;
        if (    pVMMDev->u32Version == VMMDEV_MEMORY_VERSION
            &&  pVMMDev->u32Size >= 32
            &&  pVMMDev->u32Size <= cbMMIO)
        {
            pDevExt->pVMMDevMemory = pVMMDev;
            Log(("VBoxGuestInitDevExt: VMMDevMemory: mapping=%p size=%#RX32 (%#RX32) version=%#RX32\n",
                 pVMMDev, pVMMDev->u32Size, cbMMIO, pVMMDev->u32Version));
        }
        else /* try live without it. */
            LogRel(("VBoxGuestInitDevExt: Bogus VMMDev memory; u32Version=%RX32 (expected %RX32) u32Size=%RX32 (expected <= %RX32)\n",
                    pVMMDev->u32Version, VMMDEV_MEMORY_VERSION, pVMMDev->u32Size, cbMMIO));
    }

    /*
     * Create the wait and seesion spinlocks.
     */
    rc = RTSpinlockCreate(&pDevExt->WaitSpinlock);
    if (RT_SUCCESS(rc))
        rc = RTSpinlockCreate(&pDevExt->SessionSpinlock);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxGuestInitDevExt: failed to spinlock, rc=%d!\n", rc));
        if (pDevExt->WaitSpinlock != NIL_RTSPINLOCK)
            RTSpinlockDestroy(pDevExt->WaitSpinlock);
        return rc;
    }

    /*
     * Initialize the guest library and report the guest info back to VMMDev,
     * set the interrupt control filter mask, and fixate the guest mappings
     * made by the VMM.
     */
    rc = VbglInit(pDevExt->IOPortBase, (VMMDevMemory *)pDevExt->pVMMDevMemory);
    if (RT_SUCCESS(rc))
    {
        rc = VbglGRAlloc((VMMDevRequestHeader **)&pDevExt->pIrqAckEvents, sizeof(VMMDevEvents), VMMDevReq_AcknowledgeEvents);
        if (RT_SUCCESS(rc))
        {
            rc = vboxGuestInitReportGuestInfo(pDevExt, enmOSType);
            if (RT_SUCCESS(rc))
            {
#ifdef VBOX_HGCM
                rc = vboxGuestInitFilterMask(pDevExt, VMMDEV_EVENT_HGCM);
#else
                rc = vboxGuestInitFilterMask(pDevExt, 0);
#endif
                if (RT_SUCCESS(rc))
                {
                    vboxGuestInitFixateGuestMappings(pDevExt);
                    Log(("VBoxGuestInitDevExt: returns success\n"));
                    return VINF_SUCCESS;
                }
            }

            /* failure cleanup */
        }
        else
            Log(("VBoxGuestInitDevExt: VBoxGRAlloc failed, rc=%Rrc\n", rc));

        VbglTerminate();
    }
    else
        Log(("VBoxGuestInitDevExt: VbglInit failed, rc=%Rrc\n", rc));

    rc2 = RTSpinlockDestroy(pDevExt->WaitSpinlock); AssertRC(rc2);
    rc2 = RTSpinlockDestroy(pDevExt->SessionSpinlock); AssertRC(rc2);
    return rc; /* (failed) */
}


/**
 * Deletes all the items in a wait chain.
 * @param   pWait       The head of the chain.
 */
static void VBoxGuestDeleteWaitList(PVBOXGUESTWAITLIST pList)
{
    while (pList->pHead)
    {
        PVBOXGUESTWAIT pWait = pList->pHead;
        pList->pHead = pWait->pNext;

        pWait->pNext = NULL;
        pWait->pPrev = NULL;
        int rc2 = RTSemEventMultiDestroy(pWait->Event); AssertRC(rc2);
        pWait->Event = NIL_RTSEMEVENTMULTI;
        RTMemFree(pWait);
    }
    pList->pHead = NULL;
    pList->pTail = NULL;
}


/**
 * Destroys the VBoxGuest device extension.
 *
 * The native code should call this before the driver is loaded,
 * but don't call this on shutdown.
 *
 * @param   pDevExt         The device extension.
 */
void VBoxGuestDeleteDevExt(PVBOXGUESTDEVEXT pDevExt)
{
    int rc2;
    Log(("VBoxGuestDeleteDevExt:\n"));

    rc2 = RTSpinlockDestroy(pDevExt->WaitSpinlock); AssertRC(rc2);

    VBoxGuestDeleteWaitList(&pDevExt->WaitList);
#ifdef VBOX_HGCM
    VBoxGuestDeleteWaitList(&pDevExt->HGCMWaitList);
#endif
    VBoxGuestDeleteWaitList(&pDevExt->FreeList);

    VbglTerminate();

    pDevExt->pVMMDevMemory = NULL;

    pDevExt->IOPortBase = 0;
    pDevExt->pIrqAckEvents = NULL;
}


/**
 * Creates a VBoxGuest user session.
 *
 * The native code calls this when a ring-3 client opens the device.
 * Use VBoxGuestCreateKernelSession when a ring-0 client connects.
 *
 * @returns VBox status code.
 * @param   pDevExt         The device extension.
 * @param   ppSession       Where to store the session on success.
 */
int VBoxGuestCreateUserSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION *ppSession)
{
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)RTMemAllocZ(sizeof(*pSession));
    if (RT_UNLIKELY(!pSession))
    {
        LogRel(("VBoxGuestCreateUserSession: no memory!\n"));
        return VERR_NO_MEMORY;
    }

    pSession->Process = RTProcSelf();
    pSession->R0Process = RTR0ProcHandleSelf();
    pSession->pDevExt = pDevExt;

    *ppSession = pSession;
    LogFlow(("VBoxGuestCreateUserSession: pSession=%p proc=%RTproc (%d) r0proc=%p\n",
             pSession, pSession->Process, (int)pSession->Process, (uintptr_t)pSession->R0Process)); /** @todo %RTr0proc */
    return VINF_SUCCESS;
}


/**
 * Creates a VBoxGuest kernel session.
 *
 * The native code calls this when a ring-0 client connects to the device.
 * Use VBoxGuestCreateUserSession when a ring-3 client opens the device.
 *
 * @returns VBox status code.
 * @param   pDevExt         The device extension.
 * @param   ppSession       Where to store the session on success.
 */
int VBoxGuestCreateKernelSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION *ppSession)
{
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)RTMemAllocZ(sizeof(*pSession));
    if (RT_UNLIKELY(!pSession))
    {
        LogRel(("VBoxGuestCreateKernelSession: no memory!\n"));
        return VERR_NO_MEMORY;
    }

    pSession->Process = NIL_RTPROCESS;
    pSession->R0Process = NIL_RTR0PROCESS;
    pSession->pDevExt = pDevExt;

    *ppSession = pSession;
    LogFlow(("VBoxGuestCreateKernelSession: pSession=%p proc=%RTproc (%d) r0proc=%p\n",
             pSession, pSession->Process, (int)pSession->Process, (uintptr_t)pSession->R0Process)); /** @todo %RTr0proc */
    return VINF_SUCCESS;
}



/**
 * Closes a VBoxGuest session.
 *
 * @param   pDevExt         The device extension.
 * @param   pSession        The session to close (and free).
 */
void VBoxGuestCloseSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession)
{
    Log(("VBoxGuestCloseSession: pSession=%p proc=%RTproc (%d) r0proc=%p\n",
         pSession, pSession->Process, (int)pSession->Process, (uintptr_t)pSession->R0Process)); /** @todo %RTr0proc */

#ifdef VBOX_HGCM
    for (unsigned i = 0; i < RT_ELEMENTS(pSession->aHGCMClientIds); i++)
        if (pSession->aHGCMClientIds[i])
        {
            VBoxGuestHGCMDisconnectInfo Info;
            Info.result = 0;
            Info.u32ClientID = pSession->aHGCMClientIds[i];
            pSession->aHGCMClientIds[i] = 0;
            Log(("VBoxGuestCloseSession: disconnecting client id %#RX32\n", Info.u32ClientID));
            VbglHGCMDisconnect(&Info, VBoxGuestHGCMAsyncWaitCallback, pDevExt, false /* uninterruptible */);
        }
#endif

    pSession->pDevExt = NULL;
    pSession->Process = NIL_RTPROCESS;
    pSession->R0Process = NIL_RTR0PROCESS;
    RTMemFree(pSession);
}


/**
 * Links the wait-for-event entry into the tail of the given list.
 *
 * @param   pList           The list to link it into.
 * @param   pWait           The wait for event entry to append.
 */
DECLINLINE(void) VBoxGuestWaitAppend(PVBOXGUESTWAITLIST pList, PVBOXGUESTWAIT pWait)
{
    const PVBOXGUESTWAIT pTail = pList->pTail;
    pWait->pNext = NULL;
    pWait->pPrev = pTail;
    if (pTail)
        pTail->pNext = pWait;
    else
        pList->pHead = pWait;
    pList->pTail = pWait;
}


/**
 * Unlinks the wait-for-event entry.
 *
 * @param   pList           The list to unlink it from.
 * @param   pWait           The wait for event entry to unlink.
 */
DECLINLINE(void) VBoxGuestWaitUnlink(PVBOXGUESTWAITLIST pList, PVBOXGUESTWAIT pWait)
{
    const PVBOXGUESTWAIT pPrev = pWait->pPrev;
    const PVBOXGUESTWAIT pNext = pWait->pNext;
    if (pNext)
        pNext->pPrev = pPrev;
    else
        pList->pTail = pPrev;
    if (pPrev)
        pPrev->pNext = pNext;
    else
        pList->pHead = pNext;
}


/**
 * Allocates a wiat-for-event entry.
 *
 * @returns The wait-for-event entry.
 * @param   pDevExt         The device extension.
 */
static PVBOXGUESTWAIT VBoxGuestWaitAlloc(PVBOXGUESTDEVEXT pDevExt)
{
    /*
     * Allocate it one way or the other.
     */
    PVBOXGUESTWAIT pWait = pDevExt->FreeList.pTail;
    if (pWait)
    {
        RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
        RTSpinlockAcquireNoInts(pDevExt->WaitSpinlock, &Tmp);

        pWait = pDevExt->FreeList.pTail;
        if (pWait)
            VBoxGuestWaitUnlink(&pDevExt->FreeList, pWait);

        RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, &Tmp);
    }
    if (!pWait)
    {
        static unsigned s_cErrors = 0;

        pWait = (PVBOXGUESTWAIT)RTMemAlloc(sizeof(*pWait));
        if (!pWait)
        {
            if (s_cErrors++ < 32)
                LogRel(("VBoxGuestWaitAlloc: out-of-memory!\n"));
            return NULL;
        }

        int rc = RTSemEventMultiCreate(&pWait->Event);
        if (RT_FAILURE(rc))
        {
            if (s_cErrors++ < 32)
                LogRel(("VBoxGuestCommonIOCtl: RTSemEventMultiCreate failed with rc=%Rrc!\n", rc));
            RTMemFree(pWait);
            return NULL;
        }
    }

    /*
     * Zero members just as an precaution.
     */
    pWait->pNext = NULL;
    pWait->pPrev = NULL;
    pWait->fReqEvents = 0;
    pWait->fResEvents = 0;
#ifdef VBOX_HGCM
    pWait->pHGCMReq = NULL;
#endif
    RTSemEventMultiReset(pWait->Event);
    return pWait;
}


/**
 * Frees the wait-for-event entry.
 * The caller must own the wait spinlock!
 *
 * @param   pDevExt         The device extension.
 * @param   pWait           The wait-for-event entry to free.
 */
static void VBoxGuestWaitFreeLocked(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTWAIT pWait)
{
    pWait->fReqEvents = 0;
    pWait->fResEvents = 0;
#ifdef VBOX_HGCM
    pWait->pHGCMReq = NULL;
#endif
    VBoxGuestWaitAppend(&pDevExt->FreeList, pWait);
}


/**
 * Frees the wait-for-event entry.
 *
 * @param   pDevExt         The device extension.
 * @param   pWait           The wait-for-event entry to free.
 */
static void VBoxGuestWaitFreeUnlocked(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTWAIT pWait)
{
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(pDevExt->WaitSpinlock, &Tmp);
    VBoxGuestWaitFreeLocked(pDevExt, pWait);
    RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, &Tmp);
}


/**
 * Implements the fast (no input or output) type of IOCtls.
 *
 * This is currently just a placeholder stub inherited from the support driver code.
 *
 * @returns VBox status code.
 * @param   iFunction   The IOCtl function number.
 * @param   pDevExt     The device extension.
 * @param   pSession    The session.
 */
int  VBoxGuestCommonIOCtlFast(unsigned iFunction, PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession)
{
    Log(("VBoxGuestCommonIOCtlFast: iFunction=%#x pDevExt=%p pSession=%p\n", iFunction, pDevExt, pSession));

    return VERR_NOT_SUPPORTED;
}



static int VBoxGuestCommonIOCtl_GetVMMDevPort(PVBOXGUESTDEVEXT pDevExt, VBoxGuestPortInfo *pInfo, size_t *pcbDataReturned)
{
    Log(("VBoxGuestCommonIOCtl: GETVMMDEVPORT\n"));
    pInfo->portAddress = pDevExt->IOPortBase;
    pInfo->pVMMDevMemory = (VMMDevMemory *)pDevExt->pVMMDevMemory;
    if (pcbDataReturned)
        *pcbDataReturned = sizeof(*pInfo);
    return VINF_SUCCESS;
}


/**
 * Worker VBoxGuestCommonIOCtl_WaitEvent.
 * The caller enters the spinlock, we may or may not leave it.
 *
 * @returns VINF_SUCCESS if we've left the spinlock and can return immediately.
 */
DECLINLINE(int) WaitEventCheckCondition(PVBOXGUESTDEVEXT pDevExt, VBoxGuestWaitEventInfo *pInfo,
                                        int iEvent, const uint32_t fReqEvents, PRTSPINLOCKTMP pTmp)
{
    uint32_t fMatches = pDevExt->f32PendingEvents & fReqEvents;
    if (fMatches)
    {
        ASMAtomicAndU32(&pDevExt->f32PendingEvents, ~fMatches);
        RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, pTmp);

        pInfo->u32EventFlagsOut = fMatches;
        pInfo->u32Result = VBOXGUEST_WAITEVENT_OK;
        if (fReqEvents & ~((uint32_t)1 << iEvent))
            Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns %#x\n", pInfo->u32EventFlagsOut));
        else
            Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns %#x/%d\n", pInfo->u32EventFlagsOut, iEvent));
        return VINF_SUCCESS;
    }
    return VERR_TIMEOUT;
}


static int VBoxGuestCommonIOCtl_WaitEvent(PVBOXGUESTDEVEXT pDevExt, VBoxGuestWaitEventInfo *pInfo, size_t *pcbDataReturned,
                                          bool fInterruptible)
{
    pInfo->u32EventFlagsOut = 0;
    pInfo->u32Result = VBOXGUEST_WAITEVENT_ERROR;
    if (pcbDataReturned)
        *pcbDataReturned = sizeof(*pInfo);

    /*
     * Copy and verify the input mask.
     */
    const uint32_t fReqEvents = pInfo->u32EventMaskIn;
    int iEvent = ASMBitFirstSetU32(fReqEvents) - 1;
    if (RT_UNLIKELY(iEvent < 0))
    {
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: Invalid input mask %#x!!\n", fReqEvents));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Check the condition up front, before doing the wait-for-event allocations.
     */
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(pDevExt->WaitSpinlock, &Tmp);
    int rc = WaitEventCheckCondition(pDevExt, pInfo, iEvent, fReqEvents, &Tmp);
    if (rc == VINF_SUCCESS)
        return rc;
    RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, &Tmp);

    if (!pInfo->u32TimeoutIn)
    {
        pInfo->u32Result = VBOXGUEST_WAITEVENT_TIMEOUT;
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns VINF_TIMEOUT\n"));
        return VERR_TIMEOUT;
    }

    PVBOXGUESTWAIT pWait = VBoxGuestWaitAlloc(pDevExt);
    if (!pWait)
        return VERR_NO_MEMORY;
    pWait->fReqEvents = fReqEvents;

    /*
     * We've got the wait entry now, re-enter the spinlock and check for the condition.
     * If the wait condition is met, return.
     * Otherwise enter into the list and go to sleep waiting for the ISR to signal us.
     */
    RTSpinlockAcquireNoInts(pDevExt->WaitSpinlock, &Tmp);
    rc = WaitEventCheckCondition(pDevExt, pInfo, iEvent, fReqEvents, &Tmp);
    if (rc == VINF_SUCCESS)
    {
        VBoxGuestWaitFreeUnlocked(pDevExt, pWait);
        return rc;
    }
    VBoxGuestWaitAppend(&pDevExt->WaitList, pWait);
    RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, &Tmp);

    if (fInterruptible)
        rc = RTSemEventMultiWaitNoResume(pWait->Event,
                                         pInfo->u32TimeoutIn == UINT32_MAX ? RT_INDEFINITE_WAIT : pInfo->u32TimeoutIn);
    else
        rc = RTSemEventMultiWait(pWait->Event,
                                 pInfo->u32TimeoutIn == UINT32_MAX ? RT_INDEFINITE_WAIT : pInfo->u32TimeoutIn);

    /*
     * There is one special case here and that's when the semaphore is
     * destroyed upon device driver unload. This shouldn't happen of course,
     * but in case it does, just get out of here ASAP.
     */
    if (rc == VERR_SEM_DESTROYED)
        return rc;

    /*
     * Unlink the wait item and dispose of it.
     */
    RTSpinlockAcquireNoInts(pDevExt->WaitSpinlock, &Tmp);
    VBoxGuestWaitUnlink(&pDevExt->WaitList, pWait);
    const uint32_t fResEvents = pWait->fResEvents;
    VBoxGuestWaitFreeLocked(pDevExt, pWait);
    RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, &Tmp);

    /*
     * Now deal with the return code.
     */
    if (fResEvents)
    {
        pInfo->u32EventFlagsOut = fResEvents;
        pInfo->u32Result = VBOXGUEST_WAITEVENT_OK;
        if (fReqEvents & ~((uint32_t)1 << iEvent))
            Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns %#x\n", pInfo->u32EventFlagsOut));
        else
            Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns %#x/%d\n", pInfo->u32EventFlagsOut, iEvent));
        rc = VINF_SUCCESS;
    }
    else if (rc == VERR_TIMEOUT)
    {
        pInfo->u32Result = VBOXGUEST_WAITEVENT_TIMEOUT;
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns VINF_TIMEOUT\n"));
    }
    else if (rc == VERR_INTERRUPTED)
    {
        pInfo->u32Result = VBOXGUEST_WAITEVENT_INTERRUPTED;
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns VERR_INTERRUPTED\n"));
    }
    else
    {
        if (RT_SUCCESS(rc))
        {
            static unsigned s_cErrors = 0;
            if (s_cErrors++ < 32)
                LogRel(("VBoxGuestCommonIOCtl: WAITEVENT: returns %Rrc but no events!\n", rc));
            rc = VERR_INTERNAL_ERROR;
        }
        pInfo->u32Result = VBOXGUEST_WAITEVENT_ERROR;
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns %Rrc\n", rc));
    }

    return rc;
}


static int VBoxGuestCommonIOCtl_VMMRequest(PVBOXGUESTDEVEXT pDevExt, VMMDevRequestHeader *pReqHdr,
                                           size_t cbData, size_t *pcbDataReturned)
{
    Log(("VBoxGuestCommonIOCtl: VMMREQUEST type %d\n", pReqHdr->requestType));

    /*
     * Validate the header and request size.
     */
    const uint32_t cbReq = pReqHdr->size;
    const uint32_t cbMinSize = vmmdevGetRequestSize(pReqHdr->requestType);
    if (cbReq < cbMinSize)
    {
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: invalid hdr size %#x, expected >= %#x; type=%#x!!\n",
             cbReq, cbMinSize, pReqHdr->requestType));
        return VERR_INVALID_PARAMETER;
    }
    if (cbReq > cbData)
    {
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: invalid size %#x, expected >= %#x (hdr); type=%#x!!\n",
             cbData, cbReq, pReqHdr->requestType));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Make a copy of the request in the physical memory heap so
     * the VBoxGuestLibrary can more easily deal with the request.
     * (This is really a waste of time since the OS or the OS specific
     * code has already buffered or locked the input/output buffer, but
     * it does makes things a bit simpler wrt to phys address.)
     */
    VMMDevRequestHeader *pReqCopy;
    int rc = VbglGRAlloc(&pReqCopy, cbReq, pReqHdr->requestType);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: failed to allocate %u (%#x) bytes to cache the request. rc=%d!!\n",
             cbReq, cbReq, rc));
        return rc;
    }

    memcpy(pReqCopy, pReqHdr, cbReq);
    rc = VbglGRPerform(pReqCopy);
    if (    RT_SUCCESS(rc)
        &&  RT_SUCCESS(pReqCopy->rc))
    {
        Assert(rc != VINF_HGCM_ASYNC_EXECUTE);
        Assert(pReqCopy->rc != VINF_HGCM_ASYNC_EXECUTE);

        memcpy(pReqHdr, pReqCopy, cbReq);
        if (pcbDataReturned)
            *pcbDataReturned = cbReq;
    }
    else if (RT_FAILURE(rc))
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: VbglGRPerform - rc=%Rrc!\n", rc));
    else
    {
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: request execution failed; VMMDev rc=%Rrc!\n", pReqCopy->rc));
        rc = pReqCopy->rc;
    }

    VbglGRFree(pReqCopy);
    return rc;
}


static int VBoxGuestCommonIOCtl_CtlFilterMask(PVBOXGUESTDEVEXT pDevExt, VBoxGuestFilterMaskInfo *pInfo)
{
    VMMDevCtlGuestFilterMask *pReq;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(*pReq), VMMDevReq_CtlGuestFilterMask);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxGuestCommonIOCtl: CTL_FILTER_MASK: failed to allocate %u (%#x) bytes to cache the request. rc=%d!!\n",
             sizeof(*pReq), sizeof(*pReq), rc));
        return rc;
    }

    pReq->u32OrMask = pInfo->u32OrMask;
    pReq->u32NotMask = pInfo->u32NotMask;

    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
        Log(("VBoxGuestCommonIOCtl: CTL_FILTER_MASK: VbglGRPerform failed, rc=%Rrc!\n", rc));
    else if (RT_FAILURE(pReq->header.rc))
    {
        Log(("VBoxGuestCommonIOCtl: CTL_FILTER_MASK: The request failed; VMMDev rc=%Rrc!\n", pReq->header.rc));
        rc = pReq->header.rc;
    }

    VbglGRFree(&pReq->header);
    return rc;
}


#ifdef VBOX_HGCM

/**
 * This is a callback for dealing with async waits.
 *
 * It operates in a manner similar to VBoxGuestCommonIOCtl_WaitEvent.
 */
static DECLCALLBACK(void)
VBoxGuestHGCMAsyncWaitCallback(VMMDevHGCMRequestHeader *pHdrNonVolatile, void *pvUser, uint32_t u32User)
{
    VMMDevHGCMRequestHeader volatile *pHdr = (VMMDevHGCMRequestHeader volatile *)pHdrNonVolatile;
    const bool fInterruptible = (bool)u32User;
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pvUser;
    Log(("VBoxGuestHGCMAsyncWaitCallback: requestType=%d\n", pHdr->header.requestType));

    /*
     * Check to see if the condition was met by the time we got here.
     *
     * We create a simple poll loop here for dealing with out-of-memory
     * conditions since the caller isn't necessarily able to deal with
     * us returning too early.
     */
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    PVBOXGUESTWAIT pWait;
    for (;;)
    {
        RTSpinlockAcquireNoInts(pDevExt->WaitSpinlock, &Tmp);
        if ((pHdr->fu32Flags & VBOX_HGCM_REQ_DONE) != 0)
        {
            RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, &Tmp);
            return;
        }
        RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, &Tmp);

        pWait = VBoxGuestWaitAlloc(pDevExt);
        if (pWait)
            break;
        if (fInterruptible)
            return;
        RTThreadSleep(1);
    }
    pWait->fReqEvents = VMMDEV_EVENT_HGCM;
    pWait->pHGCMReq = pHdr;

    /*
     * Re-enter the spinlock and re-check for the condition.
     * If the condition is met, return.
     * Otherwise link us into the HGCM wait list and go to sleep.
     */
    RTSpinlockAcquireNoInts(pDevExt->WaitSpinlock, &Tmp);
    if ((pHdr->fu32Flags & VBOX_HGCM_REQ_DONE) != 0)
    {
        VBoxGuestWaitFreeLocked(pDevExt, pWait);
        RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, &Tmp);
        return;
    }
    VBoxGuestWaitAppend(&pDevExt->HGCMWaitList, pWait);
    RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, &Tmp);

    int rc;
    if (fInterruptible)
        rc = RTSemEventMultiWaitNoResume(pWait->Event, RT_INDEFINITE_WAIT);
    else
        rc = RTSemEventMultiWait(pWait->Event, RT_INDEFINITE_WAIT);

    /*
     * This shouldn't ever return failure...
     * Unlink, free and return.
     */
    if (rc == VERR_SEM_DESTROYED)
        return;
    if (RT_FAILURE(rc))
        LogRel(("VBoxGuestHGCMAsyncWaitCallback: wait failed! %Rrc\n", rc));

    RTSpinlockAcquireNoInts(pDevExt->WaitSpinlock, &Tmp);
    VBoxGuestWaitUnlink(&pDevExt->HGCMWaitList, pWait);
    VBoxGuestWaitFreeLocked(pDevExt, pWait);
    RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, &Tmp);
}


static int VBoxGuestCommonIOCtl_HGCMConnect(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, VBoxGuestHGCMConnectInfo *pInfo,
                                            size_t *pcbDataReturned)
{
    /*
     * The VbglHGCMConnect call will invoke the callback if the HGCM
     * call is performed in an ASYNC fashion. The function is not able
     * to deal with cancelled requests.
     */
    Log(("VBoxGuestCommonIOCtl: HGCM_CONNECT: %.128s\n",
         pInfo->Loc.type == VMMDevHGCMLoc_LocalHost || pInfo->Loc.type == VMMDevHGCMLoc_LocalHost_Existing
         ? pInfo->Loc.u.host.achName : "<not local host>"));

    int rc = VbglHGCMConnect(pInfo, VBoxGuestHGCMAsyncWaitCallback, pDevExt, false /* uninterruptible */);
    if (RT_SUCCESS(rc))
    {
        Log(("VBoxGuestCommonIOCtl: HGCM_CONNECT: u32Client=%RX32 result=%Rrc (rc=%Rrc)\n",
             pInfo->u32ClientID, pInfo->result, rc));
        if (RT_SUCCESS(pInfo->result))
        {
            /*
             * Append the client id to the client id table.
             * If the table has somehow become filled up, we'll disconnect the session.
             */
            unsigned i;
            RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
            RTSpinlockAcquireNoInts(pDevExt->SessionSpinlock, &Tmp);
            for (i = 0; i < RT_ELEMENTS(pSession->aHGCMClientIds); i++)
                if (!pSession->aHGCMClientIds[i])
                {
                    pSession->aHGCMClientIds[i] = pInfo->u32ClientID;
                    break;
                }
            RTSpinlockReleaseNoInts(pDevExt->SessionSpinlock, &Tmp);
            if (i >= RT_ELEMENTS(pSession->aHGCMClientIds))
            {
                static unsigned s_cErrors = 0;
                if (s_cErrors++ < 32)
                    LogRel(("VBoxGuestCommonIOCtl: HGCM_CONNECT: too many HGCMConnect calls for one session!\n"));

                VBoxGuestHGCMDisconnectInfo Info;
                Info.result = 0;
                Info.u32ClientID = pInfo->u32ClientID;
                VbglHGCMDisconnect(&Info, VBoxGuestHGCMAsyncWaitCallback, pDevExt, false /* uninterruptible */);
                return VERR_TOO_MANY_OPEN_FILES;
            }
        }
        if (pcbDataReturned)
            *pcbDataReturned = sizeof(*pInfo);
    }
    return rc;
}


static int VBoxGuestCommonIOCtl_HGCMDisconnect(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, VBoxGuestHGCMDisconnectInfo *pInfo,
                                               size_t *pcbDataReturned)
{
    /*
     * Validate the client id and invalidate its entry while we're in the call.
     */
    const uint32_t u32ClientId = pInfo->u32ClientID;
    unsigned i;
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(pDevExt->SessionSpinlock, &Tmp);
    for (i = 0; i < RT_ELEMENTS(pSession->aHGCMClientIds); i++)
        if (pSession->aHGCMClientIds[i] == u32ClientId)
        {
            pSession->aHGCMClientIds[i] = UINT32_MAX;
            break;
        }
    RTSpinlockReleaseNoInts(pDevExt->SessionSpinlock, &Tmp);
    if (i >= RT_ELEMENTS(pSession->aHGCMClientIds))
    {
        static unsigned s_cErrors = 0;
        if (s_cErrors++ > 32)
            LogRel(("VBoxGuestCommonIOCtl: HGCM_DISCONNECT: u32Client=%RX32\n", u32ClientId));
        return VERR_INVALID_HANDLE;
    }

    /*
     * The VbglHGCMConnect call will invoke the callback if the HGCM
     * call is performed in an ASYNC fashion. The function is not able
     * to deal with cancelled requests.
     */
    Log(("VBoxGuestCommonIOCtl: HGCM_DISCONNECT: u32Client=%RX32\n", pInfo->u32ClientID));
    int rc = VbglHGCMDisconnect(pInfo, VBoxGuestHGCMAsyncWaitCallback, pDevExt, false /* uninterruptible */);
    if (RT_SUCCESS(rc))
    {
        Log(("VBoxGuestCommonIOCtl: HGCM_DISCONNECT: result=%Rrc\n", pInfo->result));
        if (pcbDataReturned)
            *pcbDataReturned = sizeof(*pInfo);
    }

    /* Update the client id array according to the result. */
    RTSpinlockAcquireNoInts(pDevExt->SessionSpinlock, &Tmp);
    if (pSession->aHGCMClientIds[i] == UINT32_MAX)
        pSession->aHGCMClientIds[i] = RT_SUCCESS(rc) && RT_SUCCESS(pInfo->result) ? 0 : u32ClientId;
    RTSpinlockReleaseNoInts(pDevExt->SessionSpinlock, &Tmp);

    return rc;
}


static int VBoxGuestCommonIOCtl_HGCMCall(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, VBoxGuestHGCMCallInfo *pInfo,
                                         size_t cbData, size_t *pcbDataReturned)
{
    /*
     * Some more validations.
     */
    if (pInfo->cParms > 4096) /* (Just make sure it doesn't overflow the next check.) */
    {
        Log(("VBoxGuestCommonIOCtl: HGCM_CALL: cParm=%RX32 is not sane\n", pInfo->cParms));
        return VERR_INVALID_PARAMETER;
    }
    const size_t cbActual = sizeof(*pInfo) + pInfo->cParms * sizeof(HGCMFunctionParameter);
    if (cbData < cbActual)
    {
        Log(("VBoxGuestCommonIOCtl: HGCM_CALL: cbData=%#zx (%zu) required size is %#zx (%zu)\n",
             cbData, cbActual));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Validate the client id.
     */
    const uint32_t u32ClientId = pInfo->u32ClientID;
    unsigned i;
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(pDevExt->SessionSpinlock, &Tmp);
    for (i = 0; i < RT_ELEMENTS(pSession->aHGCMClientIds); i++)
        if (pSession->aHGCMClientIds[i] == u32ClientId)
            break;
    RTSpinlockReleaseNoInts(pDevExt->SessionSpinlock, &Tmp);
    if (RT_UNLIKELY(i >= RT_ELEMENTS(pSession->aHGCMClientIds)))
    {
        static unsigned s_cErrors = 0;
        if (s_cErrors++ > 32)
            LogRel(("VBoxGuestCommonIOCtl: HGCM_CALL: Invalid handle. u32Client=%RX32\n", u32ClientId));
        return VERR_INVALID_HANDLE;
    }

    /*
     * The VbglHGCMCall call will invoke the callback if the HGCM
     * call is performed in an ASYNC fashion. This function can
     * deal with cancelled requests, so we let user more requests
     * be interruptible (should add a flag for this later I guess).
     */
    Log(("VBoxGuestCommonIOCtl: HGCM_CALL: u32Client=%RX32\n", pInfo->u32ClientID));
    int rc = VbglHGCMCall(pInfo, VBoxGuestHGCMAsyncWaitCallback, pDevExt, pSession->R0Process != NIL_RTR0PROCESS);
    if (RT_SUCCESS(rc))
    {
        Log(("VBoxGuestCommonIOCtl: HGCM_CALL: result=%Rrc\n", pInfo->result));
        if (pcbDataReturned)
            *pcbDataReturned = cbActual;
    }
    Log(("VBoxGuestCommonIOCtl: HGCM_CALL: Failed. rc=%Rrc.\n", rc));
    return rc;
}


/**
 * @returns VBox status code. Unlike the other HGCM IOCtls this will combine
 *          the VbglHGCMConnect/Disconnect return code with the Info.result.
 */
static int VBoxGuestCommonIOCtl_HGCMClipboardReConnect(PVBOXGUESTDEVEXT pDevExt, uint32_t *pu32ClientId, size_t *pcbDataReturned)
{
    int rc;
    Log(("VBoxGuestCommonIOCtl: CLIPBOARD_CONNECT: Current u32ClientId=%RX32\n", pDevExt->u32ClipboardClientId));


    /*
     * If there is an old client, try disconnect it first.
     */
    if (pDevExt->u32ClipboardClientId != 0)
    {
        VBoxGuestHGCMDisconnectInfo Info;
        Info.result = (uint32_t)VERR_WRONG_ORDER;           /** @todo Vitali, why is this member unsigned? */
        Info.u32ClientID = pDevExt->u32ClipboardClientId;
        rc = VbglHGCMDisconnect(&Info, VBoxGuestHGCMAsyncWaitCallback, pDevExt, 0);
        if (RT_SUCCESS(rc))
        {
            LogRel(("VBoxGuestCommonIOCtl: CLIPBOARD_CONNECT: failed to disconnect old client. VbglHGCMDisconnect -> rc=%Rrc\n", rc));
            return rc;
        }
        if (RT_FAILURE((int32_t)Info.result))
        {
            Log(("VBoxGuestCommonIOCtl: CLIPBOARD_CONNECT: failed to disconnect old client. Info.result=%Rrc\n", rc));
            return Info.result;
        }
        pDevExt->u32ClipboardClientId = 0;
    }

    /*
     * Try connect.
     */
    VBoxGuestHGCMConnectInfo Info;
    Info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    strcpy(Info.Loc.u.host.achName, "VBoxSharedClipboard");
    Info.u32ClientID = 0;
    Info.result = (uint32_t)VERR_WRONG_ORDER;

    rc = VbglHGCMConnect(&Info,VBoxGuestHGCMAsyncWaitCallback, pDevExt, 0);
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxGuestCommonIOCtl: CLIPBOARD_CONNECT: VbglHGCMConnected -> rc=%Rrc\n", rc));
        return rc;
    }
    if (RT_FAILURE((int32_t)Info.result))
    {
        LogRel(("VBoxGuestCommonIOCtl: CLIPBOARD_CONNECT: VbglHGCMConnected -> rc=%Rrc\n", rc));
        return rc;
    }

    Log(("VBoxGuestCommonIOCtl: CLIPBOARD_CONNECT: connected successfully u32ClientId=%RX32\n", Info.u32ClientID));

    pDevExt->u32ClipboardClientId = Info.u32ClientID;
    *pu32ClientId = Info.u32ClientID;
    if (pcbDataReturned)
        *pcbDataReturned = sizeof(uint32_t);

    return VINF_SUCCESS;
}

#endif /* VBOX_HGCM */


/**
 * Common IOCtl for user to kernel and kernel to kernel communcation.
 *
 * This function only does the basic validation and then invokes
 * worker functions that takes care of each specific function.
 *
 * @returns VBox status code.
 *
 * @param   iFunction           The requested function.
 * @param   pDevExt             The device extension.
 * @param   pSession            The client session.
 * @param   pvData              The input/output data buffer. Can be NULL depending on the function.
 * @param   cbData              The max size of the data buffer.
 * @param   pcbDataReturned     Where to store the amount of returned data. Can be NULL.
 */
int  VBoxGuestCommonIOCtl(unsigned iFunction, PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                          void *pvData, size_t cbData, size_t *pcbDataReturned)
{
    Log(("VBoxGuestCommonIOCtl: iFunction=%#x pDevExt=%p pSession=%p pvData=%p cbData=%zu\n",
         iFunction, pDevExt, pSession, pvData, cbData));

    /*
     * Define some helper macros to simplify validation.
     */
#define CHECKRET_RING0(mnemonic) \
    do { \
        if (pSession->R0Process != NIL_RTR0PROCESS) \
        { \
            Log(("VBoxGuestCommonIOCtl: " mnemonic ": Ring-0 only, caller is %RTproc/%p\n", \
                 pSession->Process, (uintptr_t)pSession->R0Process)); \
            return VERR_PERMISSION_DENIED; \
        } \
    } while (0)
#define CHECKRET_MIN_SIZE(mnemonic, cbMin) \
    do { \
        if (cbData < (cbMin)) \
        { \
            Log(("VBoxGuestCommonIOCtl: " mnemonic ": cbData=%#zx (%zu) min is %#zx (%zu)\n", \
                 cbData, cbData, (size_t)(cbMin), (size_t)(cbMin))); \
            return VERR_BUFFER_OVERFLOW; \
        } \
        if ((cbMin) != 0 && !VALID_PTR(pvData)) \
        { \
            Log(("VBoxGuestCommonIOCtl: " mnemonic ": Invalid pointer %p\n", pvData)); \
            return VERR_INVALID_POINTER; \
        } \
    } while (0)


    /*
     * Deal with variably sized requests first.
     */
    int rc = VINF_SUCCESS;
    if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_VMMREQUEST(0)))
    {
        CHECKRET_MIN_SIZE("VMMREQUEST", sizeof(VMMDevRequestHeader));
        rc = VBoxGuestCommonIOCtl_VMMRequest(pDevExt, (VMMDevRequestHeader *)pvData, cbData, pcbDataReturned);
    }
#ifdef VBOX_HGCM
    /*
     * This one is tricky and can be done later.
     */
    else if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL(0)))
    {
        CHECKRET_MIN_SIZE("HGCM_CALL", sizeof(VBoxGuestHGCMCallInfo));
        rc = VBoxGuestCommonIOCtl_HGCMCall(pDevExt, pSession, (VBoxGuestHGCMCallInfo *)pvData, cbData, pcbDataReturned);
    }
#endif /* VBOX_HGCM */
    else
    {
        switch (iFunction)
        {
            case VBOXGUEST_IOCTL_GETVMMDEVPORT:
                CHECKRET_RING0("GETVMMDEVPORT");
                CHECKRET_MIN_SIZE("GETVMMDEVPORT", sizeof(VBoxGuestPortInfo));
                rc = VBoxGuestCommonIOCtl_GetVMMDevPort(pDevExt, (VBoxGuestPortInfo *)pvData, pcbDataReturned);
                break;

            case VBOXGUEST_IOCTL_WAITEVENT:
                CHECKRET_MIN_SIZE("WAITEVENT", sizeof(VBoxGuestWaitEventInfo));
                rc = VBoxGuestCommonIOCtl_WaitEvent(pDevExt, (VBoxGuestWaitEventInfo *)pvData, pcbDataReturned,
                                                    pSession->R0Process != NIL_RTR0PROCESS);
                break;

            case VBOXGUEST_IOCTL_CTL_FILTER_MASK:
                CHECKRET_MIN_SIZE("CTL_FILTER_MASK", sizeof(VBoxGuestFilterMaskInfo));
                rc = VBoxGuestCommonIOCtl_CtlFilterMask(pDevExt, (VBoxGuestFilterMaskInfo *)pvData);
                break;

#ifdef VBOX_HGCM
            case VBOXGUEST_IOCTL_HGCM_CONNECT:
                CHECKRET_MIN_SIZE("HGCM_CONNECT", sizeof(VBoxGuestHGCMConnectInfo));
                rc = VBoxGuestCommonIOCtl_HGCMConnect(pDevExt, pSession, (VBoxGuestHGCMConnectInfo *)pvData, pcbDataReturned);
                break;

            case VBOXGUEST_IOCTL_HGCM_DISCONNECT:
                CHECKRET_MIN_SIZE("HGCM_DISCONNECT", sizeof(VBoxGuestHGCMDisconnectInfo));
                rc = VBoxGuestCommonIOCtl_HGCMDisconnect(pDevExt, pSession, (VBoxGuestHGCMDisconnectInfo *)pvData, pcbDataReturned);
                break;


            case VBOXGUEST_IOCTL_CLIPBOARD_CONNECT:
                CHECKRET_MIN_SIZE("CLIPBOARD_CONNECT", sizeof(uint32_t));
                rc = VBoxGuestCommonIOCtl_HGCMClipboardReConnect(pDevExt, (uint32_t *)pvData, pcbDataReturned);
                break;
#endif /* VBOX_HGCM */

            default:
            {
                Log(("VBoxGuestCommonIOCtl: Unkown request %#x\n", iFunction));
                rc = VERR_NOT_SUPPORTED;
                break;
            }
        }
    }

    Log(("VBoxGuestCommonIOCtl: returns %Rrc *pcbDataReturned=%zu\n", rc, pcbDataReturned ? *pcbDataReturned : 0));
    return rc;
}



/**
 * Common interrupt service routine.
 *
 * This deals with events and with waking up thread waiting for those events.
 *
 * @returns true if it was our interrupt, false if it wasn't.
 * @param   pDevExt     The VBoxGuest device extension.
 */
bool VBoxGuestCommonISR(PVBOXGUESTDEVEXT pDevExt)
{
    /*
     * Now we have to find out whether it was our IRQ. Read the event mask
     * from our device to see if there are any pending events.
     */
    bool fOurIrq = pDevExt->pVMMDevMemory->V.V1_04.fHaveEvents;
    if (fOurIrq)
    {
        /* Acknowlegde events. */
        VMMDevEvents *pReq = pDevExt->pIrqAckEvents;
        int rc = VbglGRPerform(&pReq->header);
        if (    RT_SUCCESS(rc)
            &&  RT_SUCCESS(pReq->header.rc))
        {
            uint32_t fEvents = pReq->events;
            Log(("VBoxGuestCommonISR: acknowledge events succeeded %#RX32\n", fEvents));

            /*
             * Enter the spinlock and examin the waiting threads.
             */
            int rc2 = 0;
            PVBOXGUESTWAIT pWait;
            RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
            RTSpinlockAcquireNoInts(pDevExt->WaitSpinlock, &Tmp);

#ifdef VBOX_HGCM
            /* The HGCM event/list is kind of different in that we evaluate all entries. */
            if (fEvents & VMMDEV_EVENT_HGCM)
                for (pWait = pDevExt->HGCMWaitList.pHead; pWait; pWait = pWait->pNext)
                    if (    !pWait->fResEvents
                        &&  (pWait->pHGCMReq->fu32Flags & VBOX_HGCM_REQ_DONE))
                    {
                        pWait->fResEvents = VMMDEV_EVENT_HGCM;
                        rc2 |= RTSemEventMultiSignal(pWait->Event);
                    }
#endif

            /* Normal FIFO evaluation. */
            fEvents |= pDevExt->f32PendingEvents;
            for (pWait = pDevExt->WaitList.pHead; pWait; pWait = pWait->pNext)
                if (!pWait->fResEvents)
                {
                    pWait->fResEvents = pWait->fReqEvents & fEvents;
                    fEvents &= ~pWait->fResEvents;
                    rc2 |= RTSemEventMultiSignal(pWait->Event);
                    if (!fEvents)
                        break;
                }

            ASMAtomicXchgU32(&pDevExt->f32PendingEvents, fEvents);
            RTSpinlockReleaseNoInts(pDevExt->WaitSpinlock, &Tmp);
            Assert(rc2 == 0);
        }
        else /* something is serious wrong... */
            Log(("VBoxGuestCommonISR: acknowledge events failed rc=%d, header rc=%d (events=%#x)!!\n",
                 rc, pReq->header.rc, pReq->events));
    }
    else
        LogFlow(("VBoxGuestCommonISR: not ours\n"));

    return fOurIrq;
}

