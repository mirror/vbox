/* $Id$ */
/** @file
 * VBoxGuest - Guest Additions Driver, Common Code.
 */

/*
 * Copyright (C) 2007-2009 Sun Microsystems, Inc.
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
#define LOG_GROUP   LOG_GROUP_DEFAULT
#include "VBoxGuestInternal.h"
#include <VBox/VMMDev.h> /* for VMMDEV_RAM_SIZE */
#include <VBox/log.h>
#include <iprt/mem.h>
#include <iprt/time.h>
#include <iprt/memobj.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/process.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#ifdef VBOX_WITH_HGCM
# include <iprt/thread.h>
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef VBOX_WITH_HGCM
static DECLCALLBACK(int) VBoxGuestHGCMAsyncWaitCallback(VMMDevHGCMRequestHeader *pHdrNonVolatile, void *pvUser, uint32_t u32User);
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
    /*
     * Query the required space.
     */
    VMMDevReqHypervisorInfo *pReq;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(VMMDevReqHypervisorInfo), VMMDevReq_GetHypervisorInfo);
    if (RT_FAILURE(rc))
        return rc;
    pReq->hypervisorStart = 0;
    pReq->hypervisorSize  = 0;
    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc)) /* this shouldn't happen! */
    {
        VbglGRFree(&pReq->header);
        return rc;
    }

    /*
     * The VMM will report back if there is nothing it wants to map, like for
     * insance in VT-x and AMD-V mode.
     */
    if (pReq->hypervisorSize == 0)
        Log(("vboxGuestInitFixateGuestMappings: nothing to do\n"));
    else
    {
        /*
         * We have to try several times since the host can be picky
         * about certain addresses.
         */
        RTR0MEMOBJ  hFictive     = NIL_RTR0MEMOBJ;
        uint32_t    cbHypervisor = pReq->hypervisorSize;
        RTR0MEMOBJ  ahTries[5];
        uint32_t    iTry;
        bool        fBitched = false;
        Log(("vboxGuestInitFixateGuestMappings: cbHypervisor=%#x\n", cbHypervisor));
        for (iTry = 0; iTry < RT_ELEMENTS(ahTries); iTry++)
        {
            /*
             * Reserve space, or if that isn't supported, create a object for
             * some fictive physical memory and map that in to kernel space.
             *
             * To make the code a bit uglier, most systems cannot help with
             * 4MB alignment, so we have to deal with that in addition to
             * having two ways of getting the memory.
             */
            uint32_t    uAlignment = _4M;
            RTR0MEMOBJ  hObj;
            rc = RTR0MemObjReserveKernel(&hObj, (void *)-1, RT_ALIGN_32(cbHypervisor, _4M), uAlignment);
            if (rc == VERR_NOT_SUPPORTED)
            {
                uAlignment = PAGE_SIZE;
                rc = RTR0MemObjReserveKernel(&hObj, (void *)-1, RT_ALIGN_32(cbHypervisor, _4M) + _4M, uAlignment);
            }
            if (rc == VERR_NOT_SUPPORTED)
            {
                if (hFictive == NIL_RTR0MEMOBJ)
                {
                    rc = RTR0MemObjEnterPhys(&hObj, VBOXGUEST_HYPERVISOR_PHYSICAL_START, cbHypervisor + _4M);
                    if (RT_FAILURE(rc))
                        break;
                    hFictive = hObj;
                }
                uAlignment = _4M;
                rc = RTR0MemObjMapKernel(&hObj, hFictive, (void *)-1, uAlignment, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
                if (rc == VERR_NOT_SUPPORTED)
                {
                    uAlignment = PAGE_SIZE;
                    rc = RTR0MemObjMapKernel(&hObj, hFictive, (void *)-1, uAlignment, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
                }
            }
            if (RT_FAILURE(rc))
            {
                LogRel(("VBoxGuest: Failed to reserve memory for the hypervisor: rc=%Rrc (cbHypervisor=%#x uAlignment=%#x iTry=%u)\n",
                        rc, cbHypervisor, uAlignment, iTry));
                fBitched = true;
                break;
            }

            /*
             * Try set it.
             */
            pReq->header.requestType = VMMDevReq_SetHypervisorInfo;
            pReq->header.rc          = VERR_INTERNAL_ERROR;
            pReq->hypervisorSize     = cbHypervisor;
            pReq->hypervisorStart    = (uintptr_t)RTR0MemObjAddress(hObj);
            if (    uAlignment == PAGE_SIZE
                &&  pReq->hypervisorStart & (_4M - 1))
                pReq->hypervisorStart = RT_ALIGN_32(pReq->hypervisorStart, _4M);
            AssertMsg(RT_ALIGN_32(pReq->hypervisorStart, _4M) == pReq->hypervisorStart, ("%#x\n", pReq->hypervisorStart));

            rc = VbglGRPerform(&pReq->header);
            if (RT_SUCCESS(rc))
            {
                pDevExt->hGuestMappings = hFictive != NIL_RTR0MEMOBJ ? hFictive : hObj;
                LogRel(("VBoxGuest: %p LB %#x; uAlignment=%#x iTry=%u hGuestMappings=%p (%s)\n",
                     	RTR0MemObjAddress(pDevExt->hGuestMappings),
                     	RTR0MemObjSize(pDevExt->hGuestMappings),
                     	uAlignment, iTry, pDevExt->hGuestMappings, hFictive != NIL_RTR0PTR ? "fictive" : "reservation"));
                break;
            }
            ahTries[iTry] = hObj;
        }

        /*
         * Cleanup failed attempts.
         */
        while (iTry-- > 0)
            RTR0MemObjFree(ahTries[iTry], false /* fFreeMappings */);
        if (    RT_FAILURE(rc)
            &&  hFictive != NIL_RTR0PTR)
            RTR0MemObjFree(hFictive, false /* fFreeMappings */);
        if (RT_FAILURE(rc) && !fBitched)
            LogRel(("VBoxGuest: Warning: failed to reserve %#d of memory for guest mappings.\n", cbHypervisor));
    }
    VbglGRFree(&pReq->header);

    /*
     * We ignore failed attempts for now.
     */
    return VINF_SUCCESS;
}


/**
 * Undo what vboxGuestInitFixateGuestMappings did.
 *
 * @param   pDevExt     The device extension.
 */
static void vboxGuestTermUnfixGuestMappings(PVBOXGUESTDEVEXT pDevExt)
{
    if (pDevExt->hGuestMappings != NIL_RTR0PTR)
    {
        /*
         * Tell the host that we're going to free the memory we reserved for
         * it, the free it up. (Leak the memory if anything goes wrong here.)
         */
        VMMDevReqHypervisorInfo *pReq;
        int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(VMMDevReqHypervisorInfo), VMMDevReq_SetHypervisorInfo);
        if (RT_SUCCESS(rc))
        {
            pReq->hypervisorStart = 0;
            pReq->hypervisorSize  = 0;
            rc = VbglGRPerform(&pReq->header);
            VbglGRFree(&pReq->header);
        }
        if (RT_SUCCESS(rc))
        {
            rc = RTR0MemObjFree(pDevExt->hGuestMappings, true /* fFreeMappings */);
            AssertRC(rc);
        }
        else
            LogRel(("vboxGuestTermUnfixGuestMappings: Failed to unfix the guest mappings! rc=%Rrc\n", rc));

        pDevExt->hGuestMappings = NIL_RTR0MEMOBJ;
    }
}


/**
 * Sets the interrupt filter mask during initialization and termination.
 *
 * This will ASSUME that we're the ones in carge over the mask, so
 * we'll simply clear all bits we don't set.
 *
 * @returns VBox status code (ignored).
 * @param   pDevExt     The device extension.
 * @param   fMask       The new mask.
 */
static int vboxGuestSetFilterMask(PVBOXGUESTDEVEXT pDevExt, uint32_t fMask)
{
    VMMDevCtlGuestFilterMask *pReq;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(*pReq), VMMDevReq_CtlGuestFilterMask);
    if (RT_SUCCESS(rc))
    {
        pReq->u32OrMask = fMask;
        pReq->u32NotMask = ~fMask;
        rc = VbglGRPerform(&pReq->header);
        if (    RT_FAILURE(rc)
            ||  RT_FAILURE(pReq->header.rc))
            LogRel(("vboxGuestSetFilterMask: failed with rc=%Rrc and VMMDev rc=%Rrc\n",
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
 * @param   fFixedEvents    Events that will be enabled upon init and no client
 *                          will ever be allowed to mask.
 */
int VBoxGuestInitDevExt(PVBOXGUESTDEVEXT pDevExt, uint16_t IOPortBase,
                        void *pvMMIOBase, uint32_t cbMMIO, VBOXOSTYPE enmOSType, uint32_t fFixedEvents)
{
    int rc, rc2;

    /*
     * Adjust fFixedEvents.
     */
#ifdef VBOX_WITH_HGCM
    fFixedEvents |= VMMDEV_EVENT_HGCM;
#endif

    /*
     * Initalize the data.
     */
    pDevExt->IOPortBase = IOPortBase;
    pDevExt->pVMMDevMemory = NULL;
    pDevExt->fFixedEvents = fFixedEvents;
    pDevExt->hGuestMappings = NIL_RTR0MEMOBJ;
    pDevExt->pIrqAckEvents = NULL;
    pDevExt->PhysIrqAckEvents = NIL_RTCCPHYS;
    pDevExt->WaitList.pHead = NULL;
    pDevExt->WaitList.pTail = NULL;
#ifdef VBOX_WITH_HGCM
    pDevExt->HGCMWaitList.pHead = NULL;
    pDevExt->HGCMWaitList.pTail = NULL;
#endif
    pDevExt->FreeList.pHead = NULL;
    pDevExt->FreeList.pTail = NULL;
    pDevExt->f32PendingEvents = 0;
    pDevExt->u32ClipboardClientId = 0;
    pDevExt->u32MousePosChangedSeq = 0;

    /*
     * If there is an MMIO region validate the version and size.
     */
    if (pvMMIOBase)
    {
        VMMDevMemory *pVMMDev = (VMMDevMemory *)pvMMIOBase;
        Assert(cbMMIO);
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
    rc = RTSpinlockCreate(&pDevExt->EventSpinlock);
    if (RT_SUCCESS(rc))
        rc = RTSpinlockCreate(&pDevExt->SessionSpinlock);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxGuestInitDevExt: failed to spinlock, rc=%d!\n", rc));
        if (pDevExt->EventSpinlock != NIL_RTSPINLOCK)
            RTSpinlockDestroy(pDevExt->EventSpinlock);
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
            pDevExt->PhysIrqAckEvents = VbglPhysHeapGetPhysAddr(pDevExt->pIrqAckEvents);
            Assert(pDevExt->PhysIrqAckEvents != 0);

            rc = vboxGuestInitReportGuestInfo(pDevExt, enmOSType);
            if (RT_SUCCESS(rc))
            {
                rc = vboxGuestSetFilterMask(pDevExt, fFixedEvents);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Disable guest graphics capability by default. The guest specific
                     * graphics driver will re-enable this when it is necessary.
                     */
                    rc = VBoxGuestSetGuestCapabilities(0, VMMDEV_GUEST_SUPPORTS_GRAPHICS);
                    if (RT_SUCCESS(rc))
                    {
                        vboxGuestInitFixateGuestMappings(pDevExt);
                        Log(("VBoxGuestInitDevExt: returns success\n"));
                        return VINF_SUCCESS;
                    }
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

    rc2 = RTSpinlockDestroy(pDevExt->EventSpinlock); AssertRC(rc2);
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
        int             rc2;
        PVBOXGUESTWAIT  pWait = pList->pHead;
        pList->pHead = pWait->pNext;

        pWait->pNext = NULL;
        pWait->pPrev = NULL;
        rc2 = RTSemEventMultiDestroy(pWait->Event); AssertRC(rc2);
        pWait->Event = NIL_RTSEMEVENTMULTI;
        pWait->pSession = NULL;
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
    LogRel(("VBoxGuest: The additions driver is terminating.\n"));

    /*
     * Unfix the guest mappings, filter all events and clear
     * all capabilities.
     */
    vboxGuestTermUnfixGuestMappings(pDevExt);
    VBoxGuestSetGuestCapabilities(0, UINT32_MAX);
    vboxGuestSetFilterMask(pDevExt, 0);

    /*
     * Cleanup resources.
     */
    rc2 = RTSpinlockDestroy(pDevExt->EventSpinlock); AssertRC(rc2);
    rc2 = RTSpinlockDestroy(pDevExt->SessionSpinlock); AssertRC(rc2);

    VBoxGuestDeleteWaitList(&pDevExt->WaitList);
#ifdef VBOX_WITH_HGCM
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
    unsigned i; NOREF(i);
    Log(("VBoxGuestCloseSession: pSession=%p proc=%RTproc (%d) r0proc=%p\n",
         pSession, pSession->Process, (int)pSession->Process, (uintptr_t)pSession->R0Process)); /** @todo %RTr0proc */

#ifdef VBOX_WITH_HGCM
    for (i = 0; i < RT_ELEMENTS(pSession->aHGCMClientIds); i++)
        if (pSession->aHGCMClientIds[i])
        {
            VBoxGuestHGCMDisconnectInfo Info;
            Info.result = 0;
            Info.u32ClientID = pSession->aHGCMClientIds[i];
            pSession->aHGCMClientIds[i] = 0;
            Log(("VBoxGuestCloseSession: disconnecting client id %#RX32\n", Info.u32ClientID));
            VbglR0HGCMInternalDisconnect(&Info, VBoxGuestHGCMAsyncWaitCallback, pDevExt, RT_INDEFINITE_WAIT);
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
 * @param   pSession        The session that's allocating this. Can be NULL.
 */
static PVBOXGUESTWAIT VBoxGuestWaitAlloc(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession)
{
    /*
     * Allocate it one way or the other.
     */
    PVBOXGUESTWAIT pWait = pDevExt->FreeList.pTail;
    if (pWait)
    {
        RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
        RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);

        pWait = pDevExt->FreeList.pTail;
        if (pWait)
            VBoxGuestWaitUnlink(&pDevExt->FreeList, pWait);

        RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);
    }
    if (!pWait)
    {
        static unsigned s_cErrors = 0;
        int rc;

        pWait = (PVBOXGUESTWAIT)RTMemAlloc(sizeof(*pWait));
        if (!pWait)
        {
            if (s_cErrors++ < 32)
                LogRel(("VBoxGuestWaitAlloc: out-of-memory!\n"));
            return NULL;
        }

        rc = RTSemEventMultiCreate(&pWait->Event);
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
    pWait->pSession = pSession;
#ifdef VBOX_WITH_HGCM
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
#ifdef VBOX_WITH_HGCM
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
    RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);
    VBoxGuestWaitFreeLocked(pDevExt, pWait);
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);
}


/**
 * Modifies the guest capabilities.
 *
 * Should be called during driver init and termination.
 *
 * @returns VBox status code.
 * @param   fOr             The Or mask (what to enable).
 * @param   fNot            The Not mask (what to disable).
 */
int VBoxGuestSetGuestCapabilities(uint32_t fOr, uint32_t fNot)
{
    VMMDevReqGuestCapabilities2 *pReq;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(*pReq), VMMDevReq_SetGuestCapabilities);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxGuestSetGuestCapabilities: failed to allocate %u (%#x) bytes to cache the request. rc=%d!!\n",
             sizeof(*pReq), sizeof(*pReq), rc));
        return rc;
    }

    pReq->u32OrMask = fOr;
    pReq->u32NotMask = fNot;

    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
        Log(("VBoxGuestSetGuestCapabilities:VbglGRPerform failed, rc=%Rrc!\n", rc));
    else if (RT_FAILURE(pReq->header.rc))
    {
        Log(("VBoxGuestSetGuestCapabilities: The request failed; VMMDev rc=%Rrc!\n", pReq->header.rc));
        rc = pReq->header.rc;
    }

    VbglGRFree(&pReq->header);
    return rc;
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
        RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, pTmp);

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


static int VBoxGuestCommonIOCtl_WaitEvent(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                                          VBoxGuestWaitEventInfo *pInfo,  size_t *pcbDataReturned, bool fInterruptible)
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
    RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);
    int rc = WaitEventCheckCondition(pDevExt, pInfo, iEvent, fReqEvents, &Tmp);
    if (rc == VINF_SUCCESS)
        return rc;
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);

    if (!pInfo->u32TimeoutIn)
    {
        pInfo->u32Result = VBOXGUEST_WAITEVENT_TIMEOUT;
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns VERR_TIMEOUT\n"));
        return VERR_TIMEOUT;
    }

    PVBOXGUESTWAIT pWait = VBoxGuestWaitAlloc(pDevExt, pSession);
    if (!pWait)
        return VERR_NO_MEMORY;
    pWait->fReqEvents = fReqEvents;

    /*
     * We've got the wait entry now, re-enter the spinlock and check for the condition.
     * If the wait condition is met, return.
     * Otherwise enter into the list and go to sleep waiting for the ISR to signal us.
     */
    RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);
    rc = WaitEventCheckCondition(pDevExt, pInfo, iEvent, fReqEvents, &Tmp);
    if (rc == VINF_SUCCESS)
    {
        VBoxGuestWaitFreeUnlocked(pDevExt, pWait);
        return rc;
    }
    VBoxGuestWaitAppend(&pDevExt->WaitList, pWait);
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);

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
    RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);
    VBoxGuestWaitUnlink(&pDevExt->WaitList, pWait);
    const uint32_t fResEvents = pWait->fResEvents;
    VBoxGuestWaitFreeLocked(pDevExt, pWait);
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);

    /*
     * Now deal with the return code.
     */
    if (    fResEvents
        &&  fResEvents != UINT32_MAX)
    {
        pInfo->u32EventFlagsOut = fResEvents;
        pInfo->u32Result = VBOXGUEST_WAITEVENT_OK;
        if (fReqEvents & ~((uint32_t)1 << iEvent))
            Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns %#x\n", pInfo->u32EventFlagsOut));
        else
            Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns %#x/%d\n", pInfo->u32EventFlagsOut, iEvent));
        rc = VINF_SUCCESS;
    }
    else if (   fResEvents == UINT32_MAX
             || rc == VERR_INTERRUPTED)
    {
        pInfo->u32Result = VBOXGUEST_WAITEVENT_INTERRUPTED;
        rc == VERR_INTERRUPTED;
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns VERR_INTERRUPTED\n"));
    }
    else if (rc == VERR_TIMEOUT)
    {
        pInfo->u32Result = VBOXGUEST_WAITEVENT_TIMEOUT;
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns VERR_TIMEOUT\n"));
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


static int VBoxGuestCommonIOCtl_CancelAllWaitEvents(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession)
{
    RTSPINLOCKTMP           Tmp   = RTSPINLOCKTMP_INITIALIZER;
#if defined(RT_OS_SOLARIS)
    RTTHREADPREEMPTSTATE    State = RTTHREADPREEMPTSTATE_INITIALIZER;
#endif
    PVBOXGUESTWAIT          pWait;
    int                     rc = 0;

    Log(("VBoxGuestCommonIOCtl: CANCEL_ALL_WAITEVENTS\n"));

    /*
     * Walk the event list and wake up anyone with a matching session.
     *
     * Note! On Solaris we have to do really ugly stuff here because
     *       RTSemEventMultiSignal cannot be called with interrupts disabled.
     *       The hack is racy, but what we can we do... (Eliminate this
     *       termination hack, perhaps?)
     */
#if defined(RT_OS_SOLARIS)
    RTThreadPreemptDisable(&State);
    RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);
    do
    {
        for (pWait = pDevExt->WaitList.pHead; pWait; pWait = pWait->pNext)
            if (    pWait->pSession   == pSession
                &&  pWait->fResEvents != UINT32_MAX)
            {
                RTSEMEVENTMULTI     hEvent;
                pWait->fResEvents = UINT32_MAX;
                RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);
                /* HACK ALRET! This races wakeup + reuse! */
                rc |= RTSemEventMultiSignal(hEvent);
                RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);
                break;
            }
    } while (pWait);
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);
    RTThreadPreemptDisable(&State);
#else
    RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);
    for (pWait = pDevExt->WaitList.pHead; pWait; pWait = pWait->pNext)
        if (pWait->pSession == pSession)
        {
            pWait->fResEvents = UINT32_MAX;
            rc |= RTSemEventMultiSignal(pWait->Event);
        }
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);
#endif
    Assert(rc == 0);

    return VINF_SUCCESS;
}


static int VBoxGuestCommonIOCtl_VMMRequest(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                                           VMMDevRequestHeader *pReqHdr, size_t cbData, size_t *pcbDataReturned)
{
    Log(("VBoxGuestCommonIOCtl: VMMREQUEST type %d\n", pReqHdr->requestType));

    /*
     * Validate the header and request size.
     */
    const VMMDevRequestType enmType   = pReqHdr->requestType;
    const uint32_t          cbReq     = pReqHdr->size;
    const uint32_t          cbMinSize = vmmdevGetRequestSize(enmType);
    if (cbReq < cbMinSize)
    {
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: invalid hdr size %#x, expected >= %#x; type=%#x!!\n",
             cbReq, cbMinSize, enmType));
        return VERR_INVALID_PARAMETER;
    }
    if (cbReq > cbData)
    {
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: invalid size %#x, expected >= %#x (hdr); type=%#x!!\n",
             cbData, cbReq, enmType));
        return VERR_INVALID_PARAMETER;
    }
    int rc = VbglGRVerify(pReqHdr, cbData);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: invalid header: size %#x, expected >= %#x (hdr); type=%#x; rc %d!!\n",
             cbData, cbReq, enmType, rc));
        return rc;
    }

    /*
     * Make a copy of the request in the physical memory heap so
     * the VBoxGuestLibrary can more easily deal with the request.
     * (This is really a waste of time since the OS or the OS specific
     * code has already buffered or locked the input/output buffer, but
     * it does makes things a bit simpler wrt to phys address.)
     */
    VMMDevRequestHeader *pReqCopy;
    rc = VbglGRAlloc(&pReqCopy, cbReq, enmType);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: failed to allocate %u (%#x) bytes to cache the request. rc=%d!!\n",
             cbReq, cbReq, rc));
        return rc;
    }
    memcpy(pReqCopy, pReqHdr, cbReq);

    if (enmType == VMMDevReq_GetMouseStatus) /* clear poll condition. */
        pSession->u32MousePosChangedSeq = ASMAtomicUoReadU32(&pDevExt->u32MousePosChangedSeq);

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
    pReq->u32NotMask &= ~pDevExt->fFixedEvents; /* don't permit these to be cleared! */
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

#ifdef VBOX_WITH_HGCM

AssertCompile(RT_INDEFINITE_WAIT == (uint32_t)RT_INDEFINITE_WAIT); /* assumed by code below */

/** Worker for VBoxGuestHGCMAsyncWaitCallback*. */
static int VBoxGuestHGCMAsyncWaitCallbackWorker(VMMDevHGCMRequestHeader volatile *pHdr, PVBOXGUESTDEVEXT pDevExt,
                                                 bool fInterruptible, uint32_t cMillies)
{

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
        RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);
        if ((pHdr->fu32Flags & VBOX_HGCM_REQ_DONE) != 0)
        {
            RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);
            return VINF_SUCCESS;
        }
        RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);

        pWait = VBoxGuestWaitAlloc(pDevExt, NULL);
        if (pWait)
            break;
        if (fInterruptible)
            return VERR_INTERRUPTED;
        RTThreadSleep(1);
    }
    pWait->fReqEvents = VMMDEV_EVENT_HGCM;
    pWait->pHGCMReq = pHdr;

    /*
     * Re-enter the spinlock and re-check for the condition.
     * If the condition is met, return.
     * Otherwise link us into the HGCM wait list and go to sleep.
     */
    RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);
    if ((pHdr->fu32Flags & VBOX_HGCM_REQ_DONE) != 0)
    {
        VBoxGuestWaitFreeLocked(pDevExt, pWait);
        RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);
        return VINF_SUCCESS;
    }
    VBoxGuestWaitAppend(&pDevExt->HGCMWaitList, pWait);
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);

    int rc;
    if (fInterruptible)
        rc = RTSemEventMultiWaitNoResume(pWait->Event, cMillies);
    else
        rc = RTSemEventMultiWait(pWait->Event, cMillies);
    if (rc == VERR_SEM_DESTROYED)
        return rc;

    /*
     * Unlink, free and return.
     */
    if (    RT_FAILURE(rc)
        &&  rc != VERR_TIMEOUT
        &&  (    !fInterruptible
             ||  rc != VERR_INTERRUPTED))
        LogRel(("VBoxGuestHGCMAsyncWaitCallback: wait failed! %Rrc\n", rc));

    RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);
    VBoxGuestWaitUnlink(&pDevExt->HGCMWaitList, pWait);
    VBoxGuestWaitFreeLocked(pDevExt, pWait);
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);
    return rc;
}


/**
 * This is a callback for dealing with async waits.
 *
 * It operates in a manner similar to VBoxGuestCommonIOCtl_WaitEvent.
 */
static DECLCALLBACK(int) VBoxGuestHGCMAsyncWaitCallback(VMMDevHGCMRequestHeader *pHdr, void *pvUser, uint32_t u32User)
{
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pvUser;
    Log(("VBoxGuestHGCMAsyncWaitCallback: requestType=%d\n", pHdr->header.requestType));
    return VBoxGuestHGCMAsyncWaitCallbackWorker((VMMDevHGCMRequestHeader volatile *)pHdr,
                                                pDevExt,
                                                false /* fInterruptible */,
                                                u32User  /* cMillies */);
}


/**
 * This is a callback for dealing with async waits with a timeout.
 *
 * It operates in a manner similar to VBoxGuestCommonIOCtl_WaitEvent.
 */
static DECLCALLBACK(int) VBoxGuestHGCMAsyncWaitCallbackInterruptible(VMMDevHGCMRequestHeader *pHdr,
                                                                      void *pvUser, uint32_t u32User)
{
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pvUser;
    Log(("VBoxGuestHGCMAsyncWaitCallbackInterruptible: requestType=%d\n", pHdr->header.requestType));
    return VBoxGuestHGCMAsyncWaitCallbackWorker((VMMDevHGCMRequestHeader volatile *)pHdr,
                                                pDevExt,
                                                true /* fInterruptible */,
                                                u32User /* cMillies */ );

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

    int rc = VbglR0HGCMInternalConnect(pInfo, VBoxGuestHGCMAsyncWaitCallback, pDevExt, RT_INDEFINITE_WAIT);
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
                VbglR0HGCMInternalDisconnect(&Info, VBoxGuestHGCMAsyncWaitCallback, pDevExt, RT_INDEFINITE_WAIT);
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
    int rc = VbglR0HGCMInternalDisconnect(pInfo, VBoxGuestHGCMAsyncWaitCallback, pDevExt, RT_INDEFINITE_WAIT);
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


static int VBoxGuestCommonIOCtl_HGCMCall(PVBOXGUESTDEVEXT pDevExt,
                                         PVBOXGUESTSESSION pSession,
                                         VBoxGuestHGCMCallInfo *pInfo,
                                         uint32_t cMillies, bool fInterruptible, bool f32bit,
                                         size_t cbExtra, size_t cbData, size_t *pcbDataReturned)
{
    /*
     * Some more validations.
     */
    if (pInfo->cParms > 4096) /* (Just make sure it doesn't overflow the next check.) */
    {
        Log(("VBoxGuestCommonIOCtl: HGCM_CALL: cParm=%RX32 is not sane\n", pInfo->cParms));
        return VERR_INVALID_PARAMETER;
    }
    size_t cbActual = cbExtra + sizeof(*pInfo);
#ifdef RT_ARCH_AMD64
    if (f32bit)
        cbActual += pInfo->cParms * sizeof(HGCMFunctionParameter32);
    else
#endif
        cbActual += pInfo->cParms * sizeof(HGCMFunctionParameter);
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
    int rc;
    uint32_t fFlags = pSession->R0Process == NIL_RTR0PROCESS ? VBGLR0_HGCMCALL_F_KERNEL : VBGLR0_HGCMCALL_F_USER;
#ifdef RT_ARCH_AMD64
    if (f32bit)
    {
        if (fInterruptible)
            rc = VbglR0HGCMInternalCall32(pInfo, cbData - cbExtra, fFlags, VBoxGuestHGCMAsyncWaitCallbackInterruptible, pDevExt, cMillies);
        else
            rc = VbglR0HGCMInternalCall32(pInfo, cbData - cbExtra, fFlags, VBoxGuestHGCMAsyncWaitCallback, pDevExt, cMillies);
    }
    else
#endif
    {
        if (fInterruptible)
            rc = VbglR0HGCMInternalCall(pInfo, cbData - cbExtra, fFlags, VBoxGuestHGCMAsyncWaitCallbackInterruptible, pDevExt, cMillies);
        else
            rc = VbglR0HGCMInternalCall(pInfo, cbData - cbExtra, fFlags, VBoxGuestHGCMAsyncWaitCallback, pDevExt, cMillies);
    }
    if (RT_SUCCESS(rc))
    {
        Log(("VBoxGuestCommonIOCtl: HGCM_CALL: result=%Rrc\n", pInfo->result));
        if (pcbDataReturned)
            *pcbDataReturned = cbActual;
    }
    else
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
        Info.result = VERR_WRONG_ORDER;
        Info.u32ClientID = pDevExt->u32ClipboardClientId;
        rc = VbglR0HGCMInternalDisconnect(&Info, VBoxGuestHGCMAsyncWaitCallback, pDevExt, RT_INDEFINITE_WAIT);
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
    Info.result = VERR_WRONG_ORDER;

    rc = VbglR0HGCMInternalConnect(&Info, VBoxGuestHGCMAsyncWaitCallback, pDevExt, RT_INDEFINITE_WAIT);
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxGuestCommonIOCtl: CLIPBOARD_CONNECT: VbglHGCMConnected -> rc=%Rrc\n", rc));
        return rc;
    }
    if (RT_FAILURE(Info.result))
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

#endif /* VBOX_WITH_HGCM */

/**
 * Guest backdoor logging.
 *
 * @returns VBox status code.
 *
 * @param   pch                 The log message (need not be NULL terminated).
 * @param   cbData              Size of the buffer.
 * @param   pcbDataReturned     Where to store the amount of returned data. Can be NULL.
 */
static int VBoxGuestCommonIOCtl_Log(const char *pch, size_t cbData, size_t *pcbDataReturned)
{
    Log(("%.*s", cbData, pch));
    if (pcbDataReturned)
        *pcbDataReturned = 0;
    return VINF_SUCCESS;
}


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
int VBoxGuestCommonIOCtl(unsigned iFunction, PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                         void *pvData, size_t cbData, size_t *pcbDataReturned)
{
    Log(("VBoxGuestCommonIOCtl: iFunction=%#x pDevExt=%p pSession=%p pvData=%p cbData=%zu\n",
         iFunction, pDevExt, pSession, pvData, cbData));

    /*
     * Make sure the returned data size is set to zero.
     */
    if (pcbDataReturned)
        *pcbDataReturned = 0;

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
        rc = VBoxGuestCommonIOCtl_VMMRequest(pDevExt, pSession, (VMMDevRequestHeader *)pvData, cbData, pcbDataReturned);
    }
#ifdef VBOX_WITH_HGCM
    /*
     * These ones are a bit tricky.
     */
    else if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL(0)))
    {
        CHECKRET_MIN_SIZE("HGCM_CALL", sizeof(VBoxGuestHGCMCallInfo));
        bool fInterruptible = pSession->R0Process != NIL_RTR0PROCESS;
        rc = VBoxGuestCommonIOCtl_HGCMCall(pDevExt, pSession, (VBoxGuestHGCMCallInfo *)pvData, RT_INDEFINITE_WAIT,
                                           fInterruptible, false /*f32bit*/,
                                           0, cbData, pcbDataReturned);
    }
    else if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL_TIMED(0)))
    {
        CHECKRET_MIN_SIZE("HGCM_CALL_TIMED", sizeof(VBoxGuestHGCMCallInfoTimed));
        VBoxGuestHGCMCallInfoTimed *pInfo = (VBoxGuestHGCMCallInfoTimed *)pvData;
        rc = VBoxGuestCommonIOCtl_HGCMCall(pDevExt, pSession, &pInfo->info, pInfo->u32Timeout,
                                           !!pInfo->fInterruptible || pSession->R0Process != NIL_RTR0PROCESS,
                                           false /*f32bit*/,
                                           RT_OFFSETOF(VBoxGuestHGCMCallInfoTimed, info), cbData, pcbDataReturned);
    }
# ifdef RT_ARCH_AMD64
    else if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL_32(0)))
    {
        CHECKRET_MIN_SIZE("HGCM_CALL", sizeof(VBoxGuestHGCMCallInfo));
        bool fInterruptible = pSession->R0Process != NIL_RTR0PROCESS;
        rc = VBoxGuestCommonIOCtl_HGCMCall(pDevExt, pSession, (VBoxGuestHGCMCallInfo *)pvData, RT_INDEFINITE_WAIT,
                                           fInterruptible, true /*f32bit*/,
                                           0, cbData, pcbDataReturned);
    }
    else if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL_TIMED_32(0)))
    {
        CHECKRET_MIN_SIZE("HGCM_CALL_TIMED", sizeof(VBoxGuestHGCMCallInfoTimed));
        VBoxGuestHGCMCallInfoTimed *pInfo = (VBoxGuestHGCMCallInfoTimed *)pvData;
        rc = VBoxGuestCommonIOCtl_HGCMCall(pDevExt, pSession, &pInfo->info, pInfo->u32Timeout,
                                           !!pInfo->fInterruptible || pSession->R0Process != NIL_RTR0PROCESS,
                                           true /*f32bit*/,
                                           RT_OFFSETOF(VBoxGuestHGCMCallInfoTimed, info), cbData, pcbDataReturned);
    }
# endif
#endif /* VBOX_WITH_HGCM */
    else if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_LOG(0)))
    {
        CHECKRET_MIN_SIZE("LOG", 1);
        rc = VBoxGuestCommonIOCtl_Log((char *)pvData, cbData, pcbDataReturned);
    }
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
                rc = VBoxGuestCommonIOCtl_WaitEvent(pDevExt, pSession, (VBoxGuestWaitEventInfo *)pvData,
                                                    pcbDataReturned, pSession->R0Process != NIL_RTR0PROCESS);
                break;

            case VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS:
                if (cbData != 0)
                    rc = VERR_INVALID_PARAMETER;
                rc = VBoxGuestCommonIOCtl_CancelAllWaitEvents(pDevExt, pSession);
                break;

            case VBOXGUEST_IOCTL_CTL_FILTER_MASK:
                CHECKRET_MIN_SIZE("CTL_FILTER_MASK", sizeof(VBoxGuestFilterMaskInfo));
                rc = VBoxGuestCommonIOCtl_CtlFilterMask(pDevExt, (VBoxGuestFilterMaskInfo *)pvData);
                break;

#ifdef VBOX_WITH_HGCM
            case VBOXGUEST_IOCTL_HGCM_CONNECT:
# ifdef RT_ARCH_AMD64
            case VBOXGUEST_IOCTL_HGCM_CONNECT_32:
# endif
                CHECKRET_MIN_SIZE("HGCM_CONNECT", sizeof(VBoxGuestHGCMConnectInfo));
                rc = VBoxGuestCommonIOCtl_HGCMConnect(pDevExt, pSession, (VBoxGuestHGCMConnectInfo *)pvData, pcbDataReturned);
                break;

            case VBOXGUEST_IOCTL_HGCM_DISCONNECT:
# ifdef RT_ARCH_AMD64
            case VBOXGUEST_IOCTL_HGCM_DISCONNECT_32:
# endif
                CHECKRET_MIN_SIZE("HGCM_DISCONNECT", sizeof(VBoxGuestHGCMDisconnectInfo));
                rc = VBoxGuestCommonIOCtl_HGCMDisconnect(pDevExt, pSession, (VBoxGuestHGCMDisconnectInfo *)pvData, pcbDataReturned);
                break;

            case VBOXGUEST_IOCTL_CLIPBOARD_CONNECT:
                CHECKRET_MIN_SIZE("CLIPBOARD_CONNECT", sizeof(uint32_t));
                rc = VBoxGuestCommonIOCtl_HGCMClipboardReConnect(pDevExt, (uint32_t *)pvData, pcbDataReturned);
                break;
#endif /* VBOX_WITH_HGCM */

            default:
            {
                Log(("VBoxGuestCommonIOCtl: Unknown request iFunction=%#x Stripped size=%#x\n", iFunction,
                                VBOXGUEST_IOCTL_STRIP_SIZE(iFunction)));
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
    bool                    fMousePositionChanged = false;
    RTSPINLOCKTMP           Tmp                   = RTSPINLOCKTMP_INITIALIZER;
    VMMDevEvents volatile  *pReq                  = pDevExt->pIrqAckEvents;
    int                     rc                    = 0;
    bool                    fOurIrq;

    /*
     * Make sure we've initalized the device extension.
     */
    if (RT_UNLIKELY(!pReq))
        return false;

    /*
     * Enter the spinlock and check if it's our IRQ or not.
     *
     * Note! Solaris cannot do RTSemEventMultiSignal with interrupts disabled
     *       so we're entering the spinlock without disabling them.  This works
     *       fine as long as we never called in a nested fashion.
     */
#if defined(RT_OS_SOLARIS)
    RTSpinlockAcquire(pDevExt->EventSpinlock, &Tmp);
#else
    RTSpinlockAcquireNoInts(pDevExt->EventSpinlock, &Tmp);
#endif
    fOurIrq = pDevExt->pVMMDevMemory->V.V1_04.fHaveEvents;
    if (fOurIrq)
    {
        /*
         * Acknowlegde events.
         * We don't use VbglGRPerform here as it may take another spinlocks.
         */
        pReq->header.rc = VERR_INTERNAL_ERROR;
        pReq->events    = 0;
        ASMCompilerBarrier();
        ASMOutU32(pDevExt->IOPortBase + VMMDEV_PORT_OFF_REQUEST, (uint32_t)pDevExt->PhysIrqAckEvents);
        ASMCompilerBarrier();   /* paranoia */
        if (RT_SUCCESS(pReq->header.rc))
        {
            uint32_t        fEvents = pReq->events;
            PVBOXGUESTWAIT  pWait;

            Log(("VBoxGuestCommonISR: acknowledge events succeeded %#RX32\n", fEvents));

            /*
             * VMMDEV_EVENT_MOUSE_POSITION_CHANGED can only be polled for.
             */
            if (fEvents & VMMDEV_EVENT_MOUSE_POSITION_CHANGED)
            {
                fMousePositionChanged = true;
                fEvents &= ~VMMDEV_EVENT_MOUSE_POSITION_CHANGED;
            }

#ifdef VBOX_WITH_HGCM
            /*
             * The HGCM event/list is kind of different in that we evaluate all entries.
             */
            if (fEvents & VMMDEV_EVENT_HGCM)
            {
                for (pWait = pDevExt->HGCMWaitList.pHead; pWait; pWait = pWait->pNext)
                    if (    !pWait->fResEvents
                        &&  (pWait->pHGCMReq->fu32Flags & VBOX_HGCM_REQ_DONE))
                    {
                        pWait->fResEvents = VMMDEV_EVENT_HGCM;
                        rc |= RTSemEventMultiSignal(pWait->Event);
                    }
                fEvents &= ~VMMDEV_EVENT_HGCM;
            }
#endif

            /*
             * Normal FIFO waiter evaluation.
             */
            fEvents |= pDevExt->f32PendingEvents;
            for (pWait = pDevExt->WaitList.pHead; pWait; pWait = pWait->pNext)
                if (    (pWait->fReqEvents & fEvents)
                    &&  !pWait->fResEvents)
                {
                    pWait->fResEvents = pWait->fReqEvents & fEvents;
                    fEvents &= ~pWait->fResEvents;
                    rc |= RTSemEventMultiSignal(pWait->Event);
                    if (!fEvents)
                        break;
                }
            ASMAtomicWriteU32(&pDevExt->f32PendingEvents, fEvents);
        }
        else /* something is serious wrong... */
            Log(("VBoxGuestCommonISR: acknowledge events failed rc=%d (events=%#x)!!\n",
                 pReq->header.rc, pReq->events));
    }
    else
        LogFlow(("VBoxGuestCommonISR: not ours\n"));

    /*
     * Work the poll and async notification queues on OSes that implements that.
     * Do this outside the spinlock to prevent some recursive spinlocking.
     */
#if defined(RT_OS_SOLARIS)
    RTSpinlockRelease(pDevExt->EventSpinlock, &Tmp);
#else
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock, &Tmp);
#endif

    if (fMousePositionChanged)
    {
        ASMAtomicIncU32(&pDevExt->u32MousePosChangedSeq);
        VBoxGuestNativeISRMousePollEvent(pDevExt);
    }

    Assert(rc == 0);
    return fOurIrq;
}

