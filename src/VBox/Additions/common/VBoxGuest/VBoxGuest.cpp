/* $Id$ */
/** @file
 * VBoxGuest - Guest Additions Driver, Common Code.
 */

/*
 * Copyright (C) 2007-2014 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP   LOG_GROUP_DEFAULT
#include "VBoxGuestInternal.h"
#include "VBoxGuest2.h"
#include <VBox/VMMDev.h> /* for VMMDEV_RAM_SIZE */
#include <VBox/log.h>
#include <iprt/mem.h>
#include <iprt/time.h>
#include <iprt/memobj.h>
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/string.h>
#include <iprt/process.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#ifdef VBOX_WITH_HGCM
# include <iprt/thread.h>
#endif
#include "version-generated.h"
#if defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)
# include "revision-generated.h"
#endif
#ifdef RT_OS_WINDOWS
# ifndef CTL_CODE
#  include <Windows.h>
# endif
#endif
#if defined(RT_OS_SOLARIS) || defined(RT_OS_DARWIN)
# include <iprt/rand.h>
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef VBOX_WITH_HGCM
static DECLCALLBACK(int) VBoxGuestHGCMAsyncWaitCallback(VMMDevHGCMRequestHeader *pHdrNonVolatile, void *pvUser, uint32_t u32User);
#endif

static int VBoxGuestCommonGuestCapsAcquire(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, uint32_t fOrMask, uint32_t fNotMask, VBOXGUESTCAPSACQUIRE_FLAGS enmFlags);

#define VBOXGUEST_ACQUIRE_STYLE_EVENTS (VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST | VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST)

/** Return the mask of VMM device events that this session is allowed to see,
 *  ergo, all events except those in "acquire" mode which have not been acquired
 *  by this session. */
DECLINLINE(uint32_t) VBoxGuestCommonGetHandledEventsLocked(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession)
{
    if (!pDevExt->u32AcquireModeGuestCaps)
        return VMMDEV_EVENT_VALID_EVENT_MASK;

    /** @note VMMDEV_EVENT_VALID_EVENT_MASK should actually be the mask of valid
     *        capabilities, but that doesn't affect this code. */
    uint32_t u32AllowedGuestCaps = pSession->u32AquiredGuestCaps | (VMMDEV_EVENT_VALID_EVENT_MASK & ~pDevExt->u32AcquireModeGuestCaps);
    uint32_t u32CleanupEvents = VBOXGUEST_ACQUIRE_STYLE_EVENTS;
    if (u32AllowedGuestCaps & VMMDEV_GUEST_SUPPORTS_GRAPHICS)
        u32CleanupEvents &= ~VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
    if (u32AllowedGuestCaps & VMMDEV_GUEST_SUPPORTS_SEAMLESS)
        u32CleanupEvents &= ~VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST;

    return VMMDEV_EVENT_VALID_EVENT_MASK & ~u32CleanupEvents;
}

DECLINLINE(uint32_t) VBoxGuestCommonGetAndCleanPendingEventsLocked(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, uint32_t fReqEvents)
{
    uint32_t fMatches = pDevExt->f32PendingEvents & fReqEvents & VBoxGuestCommonGetHandledEventsLocked(pDevExt, pSession);
    if (fMatches)
        ASMAtomicAndU32(&pDevExt->f32PendingEvents, ~fMatches);
    return fMatches;
}

/** Puts a capability in "acquire" or "set" mode and returns the mask of
 * capabilities currently in the other mode.  Once a capability has been put in
 * one of the two modes it can no longer be removed from that mode. */
DECLINLINE(bool) VBoxGuestCommonGuestCapsModeSet(PVBOXGUESTDEVEXT pDevExt, uint32_t fCaps, bool fAcquire, uint32_t *pu32OtherVal)
{
    uint32_t *pVal = fAcquire ? &pDevExt->u32AcquireModeGuestCaps : &pDevExt->u32SetModeGuestCaps;
    const uint32_t fNotVal = !fAcquire ? pDevExt->u32AcquireModeGuestCaps : pDevExt->u32SetModeGuestCaps;
    bool fResult = true;
    RTSpinlockAcquire(pDevExt->EventSpinlock);

    if (!(fNotVal & fCaps))
        *pVal |= fCaps;
    else
    {
        AssertMsgFailed(("trying to change caps mode\n"));
        fResult = false;
    }

    RTSpinlockRelease(pDevExt->EventSpinlock);

    if (pu32OtherVal)
        *pu32OtherVal = fNotVal;
    return fResult;
}


/**
 * Sets the interrupt filter mask during initialization and termination.
 *
 * This will ASSUME that we're the ones in carge over the mask, so
 * we'll simply clear all bits we don't set.
 *
 * @returns VBox status code (ignored).
 * @param   fMask       The new mask.
 */
static int vboxGuestSetFilterMask(VMMDevCtlGuestFilterMask *pReq,
                                  uint32_t fMask)
{
    int rc;

    pReq->u32OrMask = fMask;
    pReq->u32NotMask = ~fMask;
    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
        LogRel(("vboxGuestSetFilterMask: failed with rc=%Rrc\n", rc));
    return rc;
}


/**
 * Sets the guest capabilities to the host.
 *
 * This will ASSUME that we're the ones in charge of the mask, so
 * we'll simply clear all bits we don't set.
 *
 * @returns VBox status code.
 * @param   fMask       The new mask.
 */
static int vboxGuestSetCapabilities(VMMDevReqGuestCapabilities2 *pReq,
                                    uint32_t fMask)
{
    int rc;

    pReq->u32OrMask = fMask;
    pReq->u32NotMask = ~fMask;
    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
        LogRelFunc(("failed with rc=%Rrc\n", rc));
    return rc;
}


/**
 * Sets the mouse status to the host.
 *
 * This will ASSUME that we're the ones in charge of the mask, so
 * we'll simply clear all bits we don't set.
 *
 * @returns VBox status code.
 * @param   fMask       The new mask.
 */
static int vboxGuestSetMouseStatus(VMMDevReqMouseStatus *pReq, uint32_t fMask)
{
    int rc;

    pReq->mouseFeatures = fMask;
    pReq->pointerXPos   = 0;
    pReq->pointerYPos   = 0;
    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
        LogRelFunc(("failed with rc=%Rrc\n", rc));
    return rc;
}


/** Host flags to be updated by a given invocation of the
 * vboxGuestUpdateHostFlags() method. */
enum
{
    HostFlags_FilterMask   = 1,
    HostFlags_Capabilities = 2,
    HostFlags_MouseStatus  = 4,
    HostFlags_All          = 7,
    HostFlags_SizeHack = (unsigned)-1
};


static int vboxGuestGetHostFlagsFromSessions(PVBOXGUESTDEVEXT pDevExt,
                                             PVBOXGUESTSESSION pSession,
                                             uint32_t *pfFilterMask,
                                             uint32_t *pfCapabilities,
                                             uint32_t *pfMouseStatus)
{
    PVBOXGUESTSESSION pIterator;
    uint32_t fFilterMask = 0, fCapabilities = 0, fMouseStatus = 0;
    unsigned cSessions = 0;
    int rc = VINF_SUCCESS;

    RTListForEach(&pDevExt->SessionList, pIterator, VBOXGUESTSESSION, ListNode)
    {
        fFilterMask   |= pIterator->fFilterMask;
        fCapabilities |= pIterator->fCapabilities;
        fMouseStatus  |= pIterator->fMouseStatus;
        ++cSessions;
    }
    if (!cSessions)
        if (fFilterMask | fCapabilities | fMouseStatus)
            rc = VERR_INTERNAL_ERROR;
    if (cSessions == 1 && pSession)
        if (   fFilterMask   != pSession->fFilterMask
            || fCapabilities != pSession->fCapabilities
            || fMouseStatus  != pSession->fMouseStatus)
            rc = VERR_INTERNAL_ERROR;
    if (cSessions > 1 && pSession)
        if (   ~fFilterMask   & pSession->fFilterMask
            || ~fCapabilities & pSession->fCapabilities
            || ~fMouseStatus  & pSession->fMouseStatus)
            rc = VERR_INTERNAL_ERROR;
    *pfFilterMask = fFilterMask;
    *pfCapabilities = fCapabilities;
    *pfMouseStatus = fMouseStatus;
    return rc;
}


/** Check which host flags in a given category are being asserted by some guest
 * session and assert exactly those on the host which are being asserted by one
 * or more sessions.  pCallingSession is purely for sanity checking and can be
 * NULL.
 * @note Takes the session spin-lock.
 */
static int vboxGuestUpdateHostFlags(PVBOXGUESTDEVEXT pDevExt,
                                    PVBOXGUESTSESSION pSession,
                                    unsigned enmFlags)
{
    int rc;
    VMMDevCtlGuestFilterMask    *pFilterReq = NULL;
    VMMDevReqGuestCapabilities2 *pCapabilitiesReq = NULL;
    VMMDevReqMouseStatus        *pStatusReq = NULL;
    uint32_t fFilterMask = 0, fCapabilities = 0, fMouseStatus = 0;

    rc = VbglGRAlloc((VMMDevRequestHeader **)&pFilterReq, sizeof(*pFilterReq),
                     VMMDevReq_CtlGuestFilterMask);
    if (RT_SUCCESS(rc))
        rc = VbglGRAlloc((VMMDevRequestHeader **)&pCapabilitiesReq,
                         sizeof(*pCapabilitiesReq),
                         VMMDevReq_SetGuestCapabilities);
    if (RT_SUCCESS(rc))
        rc = VbglGRAlloc((VMMDevRequestHeader **)&pStatusReq,
                         sizeof(*pStatusReq), VMMDevReq_SetMouseStatus);
    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    if (RT_SUCCESS(rc))
        rc = vboxGuestGetHostFlagsFromSessions(pDevExt, pSession, &fFilterMask,
                                               &fCapabilities, &fMouseStatus);
    if (RT_SUCCESS(rc))
    {
        fFilterMask |= pDevExt->fFixedEvents;
        /* Since VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR is inverted in the session
         * capabilities we invert it again before sending it to the host. */
        fMouseStatus ^= VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR;
        if (enmFlags & HostFlags_FilterMask)
            vboxGuestSetFilterMask(pFilterReq, fFilterMask);
        fCapabilities |= pDevExt->u32GuestCaps;
        if (enmFlags & HostFlags_Capabilities)
            vboxGuestSetCapabilities(pCapabilitiesReq, fCapabilities);
        if (enmFlags & HostFlags_MouseStatus)
            vboxGuestSetMouseStatus(pStatusReq, fMouseStatus);
    }
    RTSpinlockRelease(pDevExt->SessionSpinlock);
    if (pFilterReq)
        VbglGRFree(&pFilterReq->header);
    if (pCapabilitiesReq)
        VbglGRFree(&pCapabilitiesReq->header);
    if (pStatusReq)
        VbglGRFree(&pStatusReq->header);
    return rc;
}


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static const uint32_t cbChangeMemBalloonReq = RT_OFFSETOF(VMMDevChangeMemBalloon, aPhysPage[VMMDEV_MEMORY_BALLOON_CHUNK_PAGES]);

#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
/**
 * Drag in the rest of IRPT since we share it with the
 * rest of the kernel modules on Solaris.
 */
PFNRT g_apfnVBoxGuestIPRTDeps[] =
{
    /* VirtioNet */
    (PFNRT)RTRandBytes,
    /* RTSemMutex* */
    (PFNRT)RTSemMutexCreate,
    (PFNRT)RTSemMutexDestroy,
    (PFNRT)RTSemMutexRequest,
    (PFNRT)RTSemMutexRequestNoResume,
    (PFNRT)RTSemMutexRequestDebug,
    (PFNRT)RTSemMutexRequestNoResumeDebug,
    (PFNRT)RTSemMutexRelease,
    (PFNRT)RTSemMutexIsOwned,
    NULL
};
#endif  /* RT_OS_DARWIN || RT_OS_SOLARIS  */


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
     * instance in VT-x and AMD-V mode.
     */
    if (pReq->hypervisorSize == 0)
        LogFlowFunc(("Nothing to do\n"));
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
        LogFlowFunc(("cbHypervisor=%#x\n", cbHypervisor));
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
            /*
             * If both RTR0MemObjReserveKernel calls above failed because either not supported or
             * not implemented at all at the current platform, try to map the memory object into the
             * virtual kernel space.
             */
            if (rc == VERR_NOT_SUPPORTED)
            {
                if (hFictive == NIL_RTR0MEMOBJ)
                {
                    rc = RTR0MemObjEnterPhys(&hObj, VBOXGUEST_HYPERVISOR_PHYSICAL_START, cbHypervisor + _4M, RTMEM_CACHE_POLICY_DONT_CARE);
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
            pReq->hypervisorStart    = (RTGCPTR32)(uintptr_t)RTR0MemObjAddress(hObj);
            if (    uAlignment == PAGE_SIZE
                &&  pReq->hypervisorStart & (_4M - 1))
                pReq->hypervisorStart = RT_ALIGN_32(pReq->hypervisorStart, _4M);
            AssertMsg(RT_ALIGN_32(pReq->hypervisorStart, _4M) == pReq->hypervisorStart, ("%#x\n", pReq->hypervisorStart));

            rc = VbglGRPerform(&pReq->header);
            if (RT_SUCCESS(rc))
            {
                pDevExt->hGuestMappings = hFictive != NIL_RTR0MEMOBJ ? hFictive : hObj;
                Log(("VBoxGuest: %p LB %#x; uAlignment=%#x iTry=%u hGuestMappings=%p (%s)\n",
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
 * Inflate the balloon by one chunk represented by an R0 memory object.
 *
 * The caller owns the balloon mutex.
 *
 * @returns IPRT status code.
 * @param   pMemObj     Pointer to the R0 memory object.
 * @param   pReq        The pre-allocated request for performing the VMMDev call.
 */
static int vboxGuestBalloonInflate(PRTR0MEMOBJ pMemObj, VMMDevChangeMemBalloon *pReq)
{
    uint32_t iPage;
    int rc;

    for (iPage = 0; iPage < VMMDEV_MEMORY_BALLOON_CHUNK_PAGES; iPage++)
    {
        RTHCPHYS phys = RTR0MemObjGetPagePhysAddr(*pMemObj, iPage);
        pReq->aPhysPage[iPage] = phys;
    }

    pReq->fInflate = true;
    pReq->header.size = cbChangeMemBalloonReq;
    pReq->cPages = VMMDEV_MEMORY_BALLOON_CHUNK_PAGES;

    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
        LogRel(("vboxGuestBalloonInflate: VbglGRPerform failed. rc=%Rrc\n", rc));
    return rc;
}


/**
 * Deflate the balloon by one chunk - info the host and free the memory object.
 *
 * The caller owns the balloon mutex.
 *
 * @returns IPRT status code.
 * @param   pMemObj     Pointer to the R0 memory object.
 *                      The memory object will be freed afterwards.
 * @param   pReq        The pre-allocated request for performing the VMMDev call.
 */
static int vboxGuestBalloonDeflate(PRTR0MEMOBJ pMemObj, VMMDevChangeMemBalloon *pReq)
{
    uint32_t iPage;
    int rc;

    for (iPage = 0; iPage < VMMDEV_MEMORY_BALLOON_CHUNK_PAGES; iPage++)
    {
        RTHCPHYS phys = RTR0MemObjGetPagePhysAddr(*pMemObj, iPage);
        pReq->aPhysPage[iPage] = phys;
    }

    pReq->fInflate = false;
    pReq->header.size = cbChangeMemBalloonReq;
    pReq->cPages = VMMDEV_MEMORY_BALLOON_CHUNK_PAGES;

    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxGuestBalloonDeflate: VbglGRPerform failed. rc=%Rrc\n", rc));
        return rc;
    }

    rc = RTR0MemObjFree(*pMemObj, true);
    if (RT_FAILURE(rc))
    {
        LogRel(("vboxGuestBalloonDeflate: RTR0MemObjFree(%p,true) -> %Rrc; this is *BAD*!\n", *pMemObj, rc));
        return rc;
    }

    *pMemObj = NIL_RTR0MEMOBJ;
    return VINF_SUCCESS;
}


/**
 * Inflate/deflate the memory balloon and notify the host.
 *
 * This is a worker used by VBoxGuestCommonIOCtl_CheckMemoryBalloon - it takes
 * the mutex.
 *
 * @returns VBox status code.
 * @param   pDevExt         The device extension.
 * @param   pSession        The session.
 * @param   cBalloonChunks  The new size of the balloon in chunks of 1MB.
 * @param   pfHandleInR3    Where to return the handle-in-ring3 indicator
 *                          (VINF_SUCCESS if set).
 */
static int vboxGuestSetBalloonSizeKernel(PVBOXGUESTDEVEXT pDevExt, uint32_t cBalloonChunks, uint32_t *pfHandleInR3)
{
    int rc = VINF_SUCCESS;

    if (pDevExt->MemBalloon.fUseKernelAPI)
    {
        VMMDevChangeMemBalloon *pReq;
        uint32_t i;

        if (cBalloonChunks > pDevExt->MemBalloon.cMaxChunks)
        {
            LogRel(("vboxGuestSetBalloonSizeKernel: illegal balloon size %u (max=%u)\n",
                    cBalloonChunks, pDevExt->MemBalloon.cMaxChunks));
            return VERR_INVALID_PARAMETER;
        }

        if (cBalloonChunks == pDevExt->MemBalloon.cMaxChunks)
            return VINF_SUCCESS;   /* nothing to do */

        if (   cBalloonChunks > pDevExt->MemBalloon.cChunks
            && !pDevExt->MemBalloon.paMemObj)
        {
            pDevExt->MemBalloon.paMemObj = (PRTR0MEMOBJ)RTMemAllocZ(sizeof(RTR0MEMOBJ) * pDevExt->MemBalloon.cMaxChunks);
            if (!pDevExt->MemBalloon.paMemObj)
            {
                LogRel(("vboxGuestSetBalloonSizeKernel: no memory for paMemObj!\n"));
                return VERR_NO_MEMORY;
            }
        }

        rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, cbChangeMemBalloonReq, VMMDevReq_ChangeMemBalloon);
        if (RT_FAILURE(rc))
            return rc;

        if (cBalloonChunks > pDevExt->MemBalloon.cChunks)
        {
            /* inflate */
            for (i = pDevExt->MemBalloon.cChunks; i < cBalloonChunks; i++)
            {
                rc = RTR0MemObjAllocPhysNC(&pDevExt->MemBalloon.paMemObj[i],
                                           VMMDEV_MEMORY_BALLOON_CHUNK_SIZE, NIL_RTHCPHYS);
                if (RT_FAILURE(rc))
                {
                    if (rc == VERR_NOT_SUPPORTED)
                    {
                        /* not supported -- fall back to the R3-allocated memory. */
                        rc = VINF_SUCCESS;
                        pDevExt->MemBalloon.fUseKernelAPI = false;
                        Assert(pDevExt->MemBalloon.cChunks == 0);
                        Log(("VBoxGuestSetBalloonSizeKernel: PhysNC allocs not supported, falling back to R3 allocs.\n"));
                    }
                    /* else if (rc == VERR_NO_MEMORY || rc == VERR_NO_PHYS_MEMORY):
                     *      cannot allocate more memory => don't try further, just stop here */
                    /* else: XXX what else can fail?  VERR_MEMOBJ_INIT_FAILED for instance. just stop. */
                    break;
                }

                rc = vboxGuestBalloonInflate(&pDevExt->MemBalloon.paMemObj[i], pReq);
                if (RT_FAILURE(rc))
                {
                    Log(("vboxGuestSetBalloonSize(inflate): failed, rc=%Rrc!\n", rc));
                    RTR0MemObjFree(pDevExt->MemBalloon.paMemObj[i], true);
                    pDevExt->MemBalloon.paMemObj[i] = NIL_RTR0MEMOBJ;
                    break;
                }
                pDevExt->MemBalloon.cChunks++;
            }
        }
        else
        {
            /* deflate */
            for (i = pDevExt->MemBalloon.cChunks; i-- > cBalloonChunks;)
            {
                rc = vboxGuestBalloonDeflate(&pDevExt->MemBalloon.paMemObj[i], pReq);
                if (RT_FAILURE(rc))
                {
                    Log(("vboxGuestSetBalloonSize(deflate): failed, rc=%Rrc!\n", rc));
                    break;
                }
                pDevExt->MemBalloon.cChunks--;
            }
        }

        VbglGRFree(&pReq->header);
    }

    /*
     * Set the handle-in-ring3 indicator.  When set Ring-3 will have to work
     * the balloon changes via the other API.
     */
    *pfHandleInR3 = pDevExt->MemBalloon.fUseKernelAPI ? false : true;

    return rc;
}


/**
 * Helper to reinit the VBoxVMM communication after hibernation.
 *
 * @returns VBox status code.
 * @param   pDevExt         The device extension.
 * @param   enmOSType       The OS type.
 */
int VBoxGuestReinitDevExtAfterHibernation(PVBOXGUESTDEVEXT pDevExt, VBOXOSTYPE enmOSType)
{
    int rc = VBoxGuestReportGuestInfo(enmOSType);
    if (RT_SUCCESS(rc))
    {
        rc = VBoxGuestReportDriverStatus(true /* Driver is active */);
        if (RT_FAILURE(rc))
            LogFlowFunc(("Could not report guest driver status, rc=%Rrc\n", rc));
    }
    else
        LogFlowFunc(("Could not report guest information to host, rc=%Rrc\n", rc));

    LogFlowFunc(("Returned with rc=%Rrc\n", rc));
    return rc;
}


/**
 * Inflate/deflate the balloon by one chunk.
 *
 * Worker for VBoxGuestCommonIOCtl_ChangeMemoryBalloon - it takes the mutex.
 *
 * @returns VBox status code.
 * @param   pDevExt         The device extension.
 * @param   pSession        The session.
 * @param   u64ChunkAddr    The address of the chunk to add to / remove from the
 *                          balloon.
 * @param   fInflate        Inflate if true, deflate if false.
 */
static int vboxGuestSetBalloonSizeFromUser(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                                           uint64_t u64ChunkAddr, bool fInflate)
{
    VMMDevChangeMemBalloon *pReq;
    int rc = VINF_SUCCESS;
    uint32_t i;
    PRTR0MEMOBJ pMemObj = NULL;

    if (fInflate)
    {
        if (   pDevExt->MemBalloon.cChunks > pDevExt->MemBalloon.cMaxChunks - 1
            || pDevExt->MemBalloon.cMaxChunks == 0 /* If called without first querying. */)
        {
            LogRel(("vboxGuestSetBalloonSize: cannot inflate balloon, already have %u chunks (max=%u)\n",
                    pDevExt->MemBalloon.cChunks, pDevExt->MemBalloon.cMaxChunks));
            return VERR_INVALID_PARAMETER;
        }

        if (!pDevExt->MemBalloon.paMemObj)
        {
            pDevExt->MemBalloon.paMemObj = (PRTR0MEMOBJ)RTMemAlloc(sizeof(RTR0MEMOBJ) * pDevExt->MemBalloon.cMaxChunks);
            if (!pDevExt->MemBalloon.paMemObj)
            {
                LogRel(("VBoxGuestSetBalloonSizeFromUser: no memory for paMemObj!\n"));
                return VERR_NO_MEMORY;
            }
            for (i = 0; i < pDevExt->MemBalloon.cMaxChunks; i++)
                pDevExt->MemBalloon.paMemObj[i] = NIL_RTR0MEMOBJ;
        }
    }
    else
    {
        if (pDevExt->MemBalloon.cChunks == 0)
        {
            AssertMsgFailed(("vboxGuestSetBalloonSize: cannot decrease balloon, already at size 0\n"));
            return VERR_INVALID_PARAMETER;
        }
    }

    /*
     * Enumerate all memory objects and check if the object is already registered.
     */
    for (i = 0; i < pDevExt->MemBalloon.cMaxChunks; i++)
    {
        if (   fInflate
            && !pMemObj
            && pDevExt->MemBalloon.paMemObj[i] == NIL_RTR0MEMOBJ)
            pMemObj = &pDevExt->MemBalloon.paMemObj[i]; /* found free object pointer */
        if (RTR0MemObjAddressR3(pDevExt->MemBalloon.paMemObj[i]) == u64ChunkAddr)
        {
            if (fInflate)
                return VERR_ALREADY_EXISTS; /* don't provide the same memory twice */
            pMemObj = &pDevExt->MemBalloon.paMemObj[i];
            break;
        }
    }
    if (!pMemObj)
    {
        if (fInflate)
        {
            /* no free object pointer found -- should not happen */
            return VERR_NO_MEMORY;
        }

        /* cannot free this memory as it wasn't provided before */
        return VERR_NOT_FOUND;
    }

    /*
     * Try inflate / default the balloon as requested.
     */
    rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, cbChangeMemBalloonReq, VMMDevReq_ChangeMemBalloon);
    if (RT_FAILURE(rc))
        return rc;

    if (fInflate)
    {
        rc = RTR0MemObjLockUser(pMemObj, (RTR3PTR)u64ChunkAddr, VMMDEV_MEMORY_BALLOON_CHUNK_SIZE,
                                RTMEM_PROT_READ | RTMEM_PROT_WRITE, NIL_RTR0PROCESS);
        if (RT_SUCCESS(rc))
        {
            rc = vboxGuestBalloonInflate(pMemObj, pReq);
            if (RT_SUCCESS(rc))
                pDevExt->MemBalloon.cChunks++;
            else
            {
                LogFlowFunc(("Inflating failed, rc=%Rrc\n", rc));
                RTR0MemObjFree(*pMemObj, true);
                *pMemObj = NIL_RTR0MEMOBJ;
            }
        }
    }
    else
    {
        rc = vboxGuestBalloonDeflate(pMemObj, pReq);
        if (RT_SUCCESS(rc))
            pDevExt->MemBalloon.cChunks--;
        else
            LogFlowFunc(("Deflating failed, rc=%Rrc\n", rc));
    }

    VbglGRFree(&pReq->header);
    return rc;
}


/**
 * Cleanup the memory balloon of a session.
 *
 * Will request the balloon mutex, so it must be valid and the caller must not
 * own it already.
 *
 * @param   pDevExt     The device extension.
 * @param   pDevExt     The session.  Can be NULL at unload.
 */
static void vboxGuestCloseMemBalloon(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession)
{
    RTSemFastMutexRequest(pDevExt->MemBalloon.hMtx);
    if (    pDevExt->MemBalloon.pOwner == pSession
        ||  pSession == NULL /*unload*/)
    {
        if (pDevExt->MemBalloon.paMemObj)
        {
            VMMDevChangeMemBalloon *pReq;
            int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, cbChangeMemBalloonReq, VMMDevReq_ChangeMemBalloon);
            if (RT_SUCCESS(rc))
            {
                uint32_t i;
                for (i = pDevExt->MemBalloon.cChunks; i-- > 0;)
                {
                    rc = vboxGuestBalloonDeflate(&pDevExt->MemBalloon.paMemObj[i], pReq);
                    if (RT_FAILURE(rc))
                    {
                        LogRelFunc(("Deflating balloon failed with rc=%Rrc; will leak %u chunks\n",
                                    rc, pDevExt->MemBalloon.cChunks));
                        break;
                    }
                    pDevExt->MemBalloon.paMemObj[i] = NIL_RTR0MEMOBJ;
                    pDevExt->MemBalloon.cChunks--;
                }
                VbglGRFree(&pReq->header);
            }
            else
                LogRelFunc(("Failed to allocate VMMDev request buffer, rc=%Rrc; will leak %u chunks\n",
                            rc, pDevExt->MemBalloon.cChunks));
            RTMemFree(pDevExt->MemBalloon.paMemObj);
            pDevExt->MemBalloon.paMemObj = NULL;
        }

        pDevExt->MemBalloon.pOwner = NULL;
    }
    RTSemFastMutexRelease(pDevExt->MemBalloon.hMtx);
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

#ifdef VBOX_GUESTDRV_WITH_RELEASE_LOGGER
    /*
     * Create the release log.
     */
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    RTUINT fFlags =   RTLOGFLAGS_PREFIX_TIME | RTLOGFLAGS_PREFIX_TID
                    | RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG;
    PRTLOGGER pRelLogger;
    rc = RTLogCreate(&pRelLogger, fFlags, "all",
#ifdef DEBUG
                     "VBOXGUEST_LOG",
#else
                     "VBOXGUEST_RELEASE_LOG",
#endif
                     RT_ELEMENTS(s_apszGroups), s_apszGroups,
                     RTLOGDEST_STDOUT | RTLOGDEST_DEBUGGER, NULL);
    if (RT_SUCCESS(rc))
    {
        RTLogRelSetDefaultInstance(pRelLogger);

        /* Explicitly flush the log in case of VBOXGUEST_RELEASE_LOG=buffered. */
        RTLogFlush(pRelLogger);
    }
    /** @todo Add native hook for getting logger config parameters and setting
     *        them. On Linux we use the module parameter stuff (see vboxguestLinuxModInit). */
#endif

    /*
     * Adjust fFixedEvents.
     */
#ifdef VBOX_WITH_HGCM
    fFixedEvents |= VMMDEV_EVENT_HGCM;
#endif

    /*
     * Initialize the data.
     */
    pDevExt->IOPortBase = IOPortBase;
    pDevExt->pVMMDevMemory = NULL;
    pDevExt->fFixedEvents = fFixedEvents;
    pDevExt->hGuestMappings = NIL_RTR0MEMOBJ;
    pDevExt->EventSpinlock = NIL_RTSPINLOCK;
    pDevExt->pIrqAckEvents = NULL;
    pDevExt->PhysIrqAckEvents = NIL_RTCCPHYS;
    RTListInit(&pDevExt->WaitList);
#ifdef VBOX_WITH_HGCM
    RTListInit(&pDevExt->HGCMWaitList);
#endif
#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
    RTListInit(&pDevExt->WakeUpList);
#endif
    RTListInit(&pDevExt->WokenUpList);
    RTListInit(&pDevExt->FreeList);
    RTListInit(&pDevExt->SessionList);
    pDevExt->fLoggingEnabled = false;
    pDevExt->f32PendingEvents = 0;
    pDevExt->u32MousePosChangedSeq = 0;
    pDevExt->SessionSpinlock = NIL_RTSPINLOCK;
    pDevExt->MemBalloon.hMtx = NIL_RTSEMFASTMUTEX;
    pDevExt->MemBalloon.cChunks = 0;
    pDevExt->MemBalloon.cMaxChunks = 0;
    pDevExt->MemBalloon.fUseKernelAPI = true;
    pDevExt->MemBalloon.paMemObj = NULL;
    pDevExt->MemBalloon.pOwner = NULL;
    pDevExt->MouseNotifyCallback.pfnNotify = NULL;
    pDevExt->MouseNotifyCallback.pvUser = NULL;

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
            LogFlowFunc(("VMMDevMemory: mapping=%p size=%#RX32 (%#RX32), version=%#RX32\n",
                         pVMMDev, pVMMDev->u32Size, cbMMIO, pVMMDev->u32Version));
        }
        else /* try live without it. */
            LogRelFunc(("Bogus VMMDev memory; u32Version=%RX32 (expected %RX32), u32Size=%RX32 (expected <= %RX32)\n",
                        pVMMDev->u32Version, VMMDEV_MEMORY_VERSION, pVMMDev->u32Size, cbMMIO));
    }

    pDevExt->u32AcquireModeGuestCaps = 0;
    pDevExt->u32SetModeGuestCaps = 0;
    pDevExt->u32GuestCaps = 0;

    /*
     * Create the wait and session spinlocks as well as the ballooning mutex.
     */
    rc = RTSpinlockCreate(&pDevExt->EventSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VBoxGuestEvent");
    if (RT_SUCCESS(rc))
        rc = RTSpinlockCreate(&pDevExt->SessionSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VBoxGuestSession");
    if (RT_FAILURE(rc))
    {
        LogRelFunc(("Failed to create spinlock, rc=%Rrc\n", rc));
        if (pDevExt->EventSpinlock != NIL_RTSPINLOCK)
            RTSpinlockDestroy(pDevExt->EventSpinlock);
        return rc;
    }

    rc = RTSemFastMutexCreate(&pDevExt->MemBalloon.hMtx);
    if (RT_FAILURE(rc))
    {
        LogRelFunc(("Failed to create mutex, rc=%Rrc\n", rc));
        RTSpinlockDestroy(pDevExt->SessionSpinlock);
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

            rc = VBoxGuestReportGuestInfo(enmOSType);
            if (RT_SUCCESS(rc))
            {
                /* Set the fixed event and disable the guest graphics capability
                 * by default. The guest specific graphics driver will re-enable
                 * the graphics capability if and when appropriate. */
                rc = vboxGuestUpdateHostFlags(pDevExt, NULL,
                                                HostFlags_FilterMask
                                              | HostFlags_Capabilities);
                if (RT_SUCCESS(rc))
                {
                    vboxGuestInitFixateGuestMappings(pDevExt);

                    rc = VBoxGuestReportDriverStatus(true /* Driver is active */);
                    if (RT_FAILURE(rc))
                        LogRelFunc(("VBoxReportGuestDriverStatus failed, rc=%Rrc\n", rc));

                    LogFlowFunc(("VBoxGuestInitDevExt: returns success\n"));
                    return VINF_SUCCESS;
                }
                else
                    LogRelFunc(("Failed to set host flags, rc=%Rrc\n", rc));
            }
            else
                LogRelFunc(("VBoxGuestInitDevExt: VBoxReportGuestInfo failed, rc=%Rrc\n", rc));
            VbglGRFree((VMMDevRequestHeader *)pDevExt->pIrqAckEvents);
        }
        else
            LogRelFunc(("VBoxGRAlloc failed, rc=%Rrc\n", rc));

        VbglTerminate();
    }
    else
        LogRelFunc(("VbglInit failed, rc=%Rrc\n", rc));

    rc2 = RTSemFastMutexDestroy(pDevExt->MemBalloon.hMtx); AssertRC(rc2);
    rc2 = RTSpinlockDestroy(pDevExt->EventSpinlock); AssertRC(rc2);
    rc2 = RTSpinlockDestroy(pDevExt->SessionSpinlock); AssertRC(rc2);

#ifdef VBOX_GUESTDRV_WITH_RELEASE_LOGGER
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogDestroy(RTLogSetDefaultInstance(NULL));
#endif
    return rc; /* (failed) */
}


/**
 * Deletes all the items in a wait chain.
 * @param   pList       The head of the chain.
 */
static void VBoxGuestDeleteWaitList(PRTLISTNODE pList)
{
    while (!RTListIsEmpty(pList))
    {
        int             rc2;
        PVBOXGUESTWAIT  pWait = RTListGetFirst(pList, VBOXGUESTWAIT, ListNode);
        RTListNodeRemove(&pWait->ListNode);

        rc2 = RTSemEventMultiDestroy(pWait->Event); AssertRC(rc2);
        pWait->Event = NIL_RTSEMEVENTMULTI;
        pWait->pSession = NULL;
        RTMemFree(pWait);
    }
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
    Log(("VBoxGuest: The additions driver is terminating.\n"));

    /*
     * Clean up the bits that involves the host first.
     */
    vboxGuestTermUnfixGuestMappings(pDevExt);
    if (!RTListIsEmpty(&pDevExt->SessionList))
    {
        LogRelFunc(("session list not empty!\n"));
        RTListInit(&pDevExt->SessionList);
    }
    /* Update the host flags (mouse status etc) not to reflect this session. */
    pDevExt->fFixedEvents = 0;
    vboxGuestUpdateHostFlags(pDevExt, NULL, HostFlags_All);
    vboxGuestCloseMemBalloon(pDevExt, (PVBOXGUESTSESSION)NULL);

    /*
     * Cleanup all the other resources.
     */
    rc2 = RTSpinlockDestroy(pDevExt->EventSpinlock); AssertRC(rc2);
    rc2 = RTSpinlockDestroy(pDevExt->SessionSpinlock); AssertRC(rc2);
    rc2 = RTSemFastMutexDestroy(pDevExt->MemBalloon.hMtx); AssertRC(rc2);

    VBoxGuestDeleteWaitList(&pDevExt->WaitList);
#ifdef VBOX_WITH_HGCM
    VBoxGuestDeleteWaitList(&pDevExt->HGCMWaitList);
#endif
#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
    VBoxGuestDeleteWaitList(&pDevExt->WakeUpList);
#endif
    VBoxGuestDeleteWaitList(&pDevExt->WokenUpList);
    VBoxGuestDeleteWaitList(&pDevExt->FreeList);

    VbglTerminate();

    pDevExt->pVMMDevMemory = NULL;

    pDevExt->IOPortBase = 0;
    pDevExt->pIrqAckEvents = NULL;

#ifdef VBOX_GUESTDRV_WITH_RELEASE_LOGGER
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogDestroy(RTLogSetDefaultInstance(NULL));
#endif

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
    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    RTListAppend(&pDevExt->SessionList, &pSession->ListNode);
    RTSpinlockRelease(pDevExt->SessionSpinlock);

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
    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    RTListAppend(&pDevExt->SessionList, &pSession->ListNode);
    RTSpinlockRelease(pDevExt->SessionSpinlock);

    *ppSession = pSession;
    LogFlowFunc(("pSession=%p proc=%RTproc (%d) r0proc=%p\n",
                 pSession, pSession->Process, (int)pSession->Process, (uintptr_t)pSession->R0Process)); /** @todo %RTr0proc */
    return VINF_SUCCESS;
}

static int VBoxGuestCommonIOCtl_CancelAllWaitEvents(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession);

/**
 * Closes a VBoxGuest session.
 *
 * @param   pDevExt         The device extension.
 * @param   pSession        The session to close (and free).
 */
void VBoxGuestCloseSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession)
{
    unsigned i; NOREF(i);
    LogFlowFunc(("pSession=%p proc=%RTproc (%d) r0proc=%p\n",
                 pSession, pSession->Process, (int)pSession->Process, (uintptr_t)pSession->R0Process)); /** @todo %RTr0proc */

    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    RTListNodeRemove(&pSession->ListNode);
    RTSpinlockRelease(pDevExt->SessionSpinlock);
    VBoxGuestCommonGuestCapsAcquire(pDevExt, pSession, 0, UINT32_MAX, VBOXGUESTCAPSACQUIRE_FLAGS_NONE);

    VBoxGuestCommonIOCtl_CancelAllWaitEvents(pDevExt, pSession);

#ifdef VBOX_WITH_HGCM
    for (i = 0; i < RT_ELEMENTS(pSession->aHGCMClientIds); i++)
        if (pSession->aHGCMClientIds[i])
        {
            VBoxGuestHGCMDisconnectInfo Info;
            Info.result = 0;
            Info.u32ClientID = pSession->aHGCMClientIds[i];
            pSession->aHGCMClientIds[i] = 0;
            LogFlowFunc(("Disconnecting client ID=%#RX32\n", Info.u32ClientID));
            VbglR0HGCMInternalDisconnect(&Info, VBoxGuestHGCMAsyncWaitCallback, pDevExt, RT_INDEFINITE_WAIT);
        }
#endif

    pSession->pDevExt = NULL;
    pSession->Process = NIL_RTPROCESS;
    pSession->R0Process = NIL_RTR0PROCESS;
    vboxGuestCloseMemBalloon(pDevExt, pSession);
    RTMemFree(pSession);
    /* Update the host flags (mouse status etc) not to reflect this session. */
    vboxGuestUpdateHostFlags(pDevExt, NULL, HostFlags_All
#ifdef RT_OS_WINDOWS
                & (~HostFlags_MouseStatus)
#endif
            );
}


/**
 * Allocates a wait-for-event entry.
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
    PVBOXGUESTWAIT pWait = RTListGetFirst(&pDevExt->FreeList, VBOXGUESTWAIT, ListNode);
    if (pWait)
    {
        RTSpinlockAcquire(pDevExt->EventSpinlock);

        pWait = RTListGetFirst(&pDevExt->FreeList, VBOXGUESTWAIT, ListNode);
        if (pWait)
            RTListNodeRemove(&pWait->ListNode);

        RTSpinlockRelease(pDevExt->EventSpinlock);
    }
    if (!pWait)
    {
        static unsigned s_cErrors = 0;
        int rc;

        pWait = (PVBOXGUESTWAIT)RTMemAlloc(sizeof(*pWait));
        if (!pWait)
        {
            if (s_cErrors++ < 32)
                LogRelFunc(("Out of memory, returning NULL\n"));
            return NULL;
        }

        rc = RTSemEventMultiCreate(&pWait->Event);
        if (RT_FAILURE(rc))
        {
            if (s_cErrors++ < 32)
                LogRelFunc(("RTSemEventMultiCreate failed with rc=%Rrc\n", rc));
            RTMemFree(pWait);
            return NULL;
        }

        pWait->ListNode.pNext = NULL;
        pWait->ListNode.pPrev = NULL;
    }

    /*
     * Zero members just as an precaution.
     */
    pWait->fReqEvents = 0;
    pWait->fResEvents = 0;
#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
    pWait->fPendingWakeUp = false;
    pWait->fFreeMe = false;
#endif
    pWait->pSession = pSession;
#ifdef VBOX_WITH_HGCM
    pWait->pHGCMReq = NULL;
#endif
    RTSemEventMultiReset(pWait->Event);
    return pWait;
}


/**
 * Frees the wait-for-event entry.
 *
 * The caller must own the wait spinlock !
 * The entry must be in a list!
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
#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
    Assert(!pWait->fFreeMe);
    if (pWait->fPendingWakeUp)
        pWait->fFreeMe = true;
    else
#endif
    {
        RTListNodeRemove(&pWait->ListNode);
        RTListAppend(&pDevExt->FreeList, &pWait->ListNode);
    }
}


/**
 * Frees the wait-for-event entry.
 *
 * @param   pDevExt         The device extension.
 * @param   pWait           The wait-for-event entry to free.
 */
static void VBoxGuestWaitFreeUnlocked(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTWAIT pWait)
{
    RTSpinlockAcquire(pDevExt->EventSpinlock);
    VBoxGuestWaitFreeLocked(pDevExt, pWait);
    RTSpinlockRelease(pDevExt->EventSpinlock);
}


#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
/**
 * Processes the wake-up list.
 *
 * All entries in the wake-up list gets signalled and moved to the woken-up
 * list.
 *
 * @param   pDevExt         The device extension.
 */
void VBoxGuestWaitDoWakeUps(PVBOXGUESTDEVEXT pDevExt)
{
    if (!RTListIsEmpty(&pDevExt->WakeUpList))
    {
        RTSpinlockAcquire(pDevExt->EventSpinlock);
        for (;;)
        {
            int            rc;
            PVBOXGUESTWAIT pWait = RTListGetFirst(&pDevExt->WakeUpList, VBOXGUESTWAIT, ListNode);
            if (!pWait)
                break;
            pWait->fPendingWakeUp = true;
            RTSpinlockRelease(pDevExt->EventSpinlock);

            rc = RTSemEventMultiSignal(pWait->Event);
            AssertRC(rc);

            RTSpinlockAcquire(pDevExt->EventSpinlock);
            pWait->fPendingWakeUp = false;
            if (!pWait->fFreeMe)
            {
                RTListNodeRemove(&pWait->ListNode);
                RTListAppend(&pDevExt->WokenUpList, &pWait->ListNode);
            }
            else
            {
                pWait->fFreeMe = false;
                VBoxGuestWaitFreeLocked(pDevExt, pWait);
            }
        }
        RTSpinlockRelease(pDevExt->EventSpinlock);
    }
}
#endif /* VBOXGUEST_USE_DEFERRED_WAKE_UP */


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
        LogFlowFunc(("Failed to allocate %u (%#x) bytes to cache the request; rc=%Rrc\n",
                     sizeof(*pReq), sizeof(*pReq), rc));
        return rc;
    }

    pReq->u32OrMask = fOr;
    pReq->u32NotMask = fNot;

    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
        LogFlowFunc(("VbglGRPerform failed, rc=%Rrc\n", rc));

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
    LogFlowFunc(("iFunction=%#x pDevExt=%p pSession=%p\n", iFunction, pDevExt, pSession));

    NOREF(iFunction);
    NOREF(pDevExt);
    NOREF(pSession);
    return VERR_NOT_SUPPORTED;
}


/**
 * Return the VMM device port.
 *
 * returns IPRT status code.
 * @param   pDevExt         The device extension.
 * @param   pInfo           The request info.
 * @param   pcbDataReturned (out) contains the number of bytes to return.
 */
static int VBoxGuestCommonIOCtl_GetVMMDevPort(PVBOXGUESTDEVEXT pDevExt, VBoxGuestPortInfo *pInfo, size_t *pcbDataReturned)
{
    LogFlowFuncEnter();

    pInfo->portAddress = pDevExt->IOPortBase;
    pInfo->pVMMDevMemory = (VMMDevMemory *)pDevExt->pVMMDevMemory;
    if (pcbDataReturned)
        *pcbDataReturned = sizeof(*pInfo);
    return VINF_SUCCESS;
}


#ifndef RT_OS_WINDOWS
/**
 * Set the callback for the kernel mouse handler.
 *
 * returns IPRT status code.
 * @param   pDevExt         The device extension.
 * @param   pNotify         The new callback information.
 */
int VBoxGuestCommonIOCtl_SetMouseNotifyCallback(PVBOXGUESTDEVEXT pDevExt, VBoxGuestMouseSetNotifyCallback *pNotify)
{
    LogFlowFuncEnter();

    RTSpinlockAcquire(pDevExt->EventSpinlock);
    pDevExt->MouseNotifyCallback = *pNotify;
    RTSpinlockRelease(pDevExt->EventSpinlock);
    return VINF_SUCCESS;
}
#endif


/**
 * Worker VBoxGuestCommonIOCtl_WaitEvent.
 *
 * The caller enters the spinlock, we leave it.
 *
 * @returns VINF_SUCCESS if we've left the spinlock and can return immediately.
 */
DECLINLINE(int) WaitEventCheckCondition(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, VBoxGuestWaitEventInfo *pInfo,
                                        int iEvent, const uint32_t fReqEvents)
{
    uint32_t fMatches = VBoxGuestCommonGetAndCleanPendingEventsLocked(pDevExt, pSession, fReqEvents);
    if (fMatches || pSession->fPendingCancelWaitEvents)
    {
        RTSpinlockRelease(pDevExt->EventSpinlock);

        pInfo->u32EventFlagsOut = fMatches;
        pInfo->u32Result = VBOXGUEST_WAITEVENT_OK;
        if (fReqEvents & ~((uint32_t)1 << iEvent))
            LogFlowFunc(("WAITEVENT: returns %#x\n", pInfo->u32EventFlagsOut));
        else
            LogFlowFunc(("WAITEVENT: returns %#x/%d\n", pInfo->u32EventFlagsOut, iEvent));
        pSession->fPendingCancelWaitEvents = false;
        return VINF_SUCCESS;
    }

    RTSpinlockRelease(pDevExt->EventSpinlock);
    return VERR_TIMEOUT;
}


static int VBoxGuestCommonIOCtl_WaitEvent(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                                          VBoxGuestWaitEventInfo *pInfo,  size_t *pcbDataReturned, bool fInterruptible)
{
    const uint32_t  fReqEvents = pInfo->u32EventMaskIn;
    uint32_t        fResEvents;
    int             iEvent;
    PVBOXGUESTWAIT  pWait;
    int             rc;

    pInfo->u32EventFlagsOut = 0;
    pInfo->u32Result = VBOXGUEST_WAITEVENT_ERROR;
    if (pcbDataReturned)
        *pcbDataReturned = sizeof(*pInfo);

    /*
     * Copy and verify the input mask.
     */
    iEvent = ASMBitFirstSetU32(fReqEvents) - 1;
    if (RT_UNLIKELY(iEvent < 0))
    {
        LogRel(("Invalid input mask %#x\n", fReqEvents));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Check the condition up front, before doing the wait-for-event allocations.
     */
    RTSpinlockAcquire(pDevExt->EventSpinlock);
    rc = WaitEventCheckCondition(pDevExt, pSession, pInfo, iEvent, fReqEvents);
    if (rc == VINF_SUCCESS)
        return rc;

    if (!pInfo->u32TimeoutIn)
    {
        pInfo->u32Result = VBOXGUEST_WAITEVENT_TIMEOUT;
        LogFlowFunc(("Returning VERR_TIMEOUT\n"));
        return VERR_TIMEOUT;
    }

    pWait = VBoxGuestWaitAlloc(pDevExt, pSession);
    if (!pWait)
        return VERR_NO_MEMORY;
    pWait->fReqEvents = fReqEvents;

    /*
     * We've got the wait entry now, re-enter the spinlock and check for the condition.
     * If the wait condition is met, return.
     * Otherwise enter into the list and go to sleep waiting for the ISR to signal us.
     */
    RTSpinlockAcquire(pDevExt->EventSpinlock);
    RTListAppend(&pDevExt->WaitList, &pWait->ListNode);
    rc = WaitEventCheckCondition(pDevExt, pSession, pInfo, iEvent, fReqEvents);
    if (rc == VINF_SUCCESS)
    {
        VBoxGuestWaitFreeUnlocked(pDevExt, pWait);
        return rc;
    }

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
    RTSpinlockAcquire(pDevExt->EventSpinlock);
    fResEvents = pWait->fResEvents;
    VBoxGuestWaitFreeLocked(pDevExt, pWait);
    RTSpinlockRelease(pDevExt->EventSpinlock);

    /*
     * Now deal with the return code.
     */
    if (    fResEvents
        &&  fResEvents != UINT32_MAX)
    {
        pInfo->u32EventFlagsOut = fResEvents;
        pInfo->u32Result = VBOXGUEST_WAITEVENT_OK;
        if (fReqEvents & ~((uint32_t)1 << iEvent))
            LogFlowFunc(("Returning %#x\n", pInfo->u32EventFlagsOut));
        else
            LogFlowFunc(("Returning %#x/%d\n", pInfo->u32EventFlagsOut, iEvent));
        rc = VINF_SUCCESS;
    }
    else if (   fResEvents == UINT32_MAX
             || rc == VERR_INTERRUPTED)
    {
        pInfo->u32Result = VBOXGUEST_WAITEVENT_INTERRUPTED;
        rc = VERR_INTERRUPTED;
        LogFlowFunc(("Returning VERR_INTERRUPTED\n"));
    }
    else if (rc == VERR_TIMEOUT)
    {
        pInfo->u32Result = VBOXGUEST_WAITEVENT_TIMEOUT;
        LogFlowFunc(("Returning VERR_TIMEOUT (2)\n"));
    }
    else
    {
        if (RT_SUCCESS(rc))
        {
            static unsigned s_cErrors = 0;
            if (s_cErrors++ < 32)
                LogRelFunc(("Returning %Rrc but no events\n", rc));
            rc = VERR_INTERNAL_ERROR;
        }
        pInfo->u32Result = VBOXGUEST_WAITEVENT_ERROR;
        LogFlowFunc(("Returning %Rrc\n", rc));
    }

    return rc;
}


static int VBoxGuestCommonIOCtl_CancelAllWaitEvents(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession)
{
    PVBOXGUESTWAIT          pWait;
    PVBOXGUESTWAIT          pSafe;
    int                     rc = 0;
    /* Was as least one WAITEVENT in process for this session?  If not we
     * set a flag that the next call should be interrupted immediately.  This
     * is needed so that a user thread can reliably interrupt another one in a
     * WAITEVENT loop. */
    bool                    fCancelledOne = false;

    LogFlowFunc(("CANCEL_ALL_WAITEVENTS\n"));

    /*
     * Walk the event list and wake up anyone with a matching session.
     */
    RTSpinlockAcquire(pDevExt->EventSpinlock);
    RTListForEachSafe(&pDevExt->WaitList, pWait, pSafe, VBOXGUESTWAIT, ListNode)
    {
        if (pWait->pSession == pSession)
        {
            fCancelledOne = true;
            pWait->fResEvents = UINT32_MAX;
            RTListNodeRemove(&pWait->ListNode);
#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
            RTListAppend(&pDevExt->WakeUpList, &pWait->ListNode);
#else
            rc |= RTSemEventMultiSignal(pWait->Event);
            RTListAppend(&pDevExt->WokenUpList, &pWait->ListNode);
#endif
        }
    }
    if (!fCancelledOne)
        pSession->fPendingCancelWaitEvents = true;
    RTSpinlockRelease(pDevExt->EventSpinlock);
    Assert(rc == 0);
    NOREF(rc);

#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
    VBoxGuestWaitDoWakeUps(pDevExt);
#endif

    return VINF_SUCCESS;
}

/**
 * Checks if the VMM request is allowed in the context of the given session.
 *
 * @returns VINF_SUCCESS or VERR_PERMISSION_DENIED.
 * @param   pSession            The calling session.
 * @param   enmType             The request type.
 * @param   pReqHdr             The request.
 */
static int VBoxGuestCheckIfVMMReqAllowed(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, VMMDevRequestType enmType,
                                         VMMDevRequestHeader const *pReqHdr)
{
    /*
     * Categorize the request being made.
     */
    /** @todo This need quite some more work! */
    enum
    {
        kLevel_Invalid, kLevel_NoOne, kLevel_OnlyVBoxGuest, kLevel_OnlyKernel, kLevel_TrustedUsers, kLevel_AllUsers
    } enmRequired;
    switch (enmType)
    {
        /*
         * Deny access to anything we don't know or provide specialized I/O controls for.
         */
#ifdef VBOX_WITH_HGCM
        case VMMDevReq_HGCMConnect:
        case VMMDevReq_HGCMDisconnect:
# ifdef VBOX_WITH_64_BITS_GUESTS
        case VMMDevReq_HGCMCall32:
        case VMMDevReq_HGCMCall64:
# else
        case VMMDevReq_HGCMCall:
# endif /* VBOX_WITH_64_BITS_GUESTS */
        case VMMDevReq_HGCMCancel:
        case VMMDevReq_HGCMCancel2:
#endif /* VBOX_WITH_HGCM */
        default:
            enmRequired = kLevel_NoOne;
            break;

        /*
         * There are a few things only this driver can do (and it doesn't use
         * the VMMRequst I/O control route anyway, but whatever).
         */
        case VMMDevReq_ReportGuestInfo:
        case VMMDevReq_ReportGuestInfo2:
        case VMMDevReq_GetHypervisorInfo:
        case VMMDevReq_SetHypervisorInfo:
        case VMMDevReq_RegisterPatchMemory:
        case VMMDevReq_DeregisterPatchMemory:
        case VMMDevReq_GetMemBalloonChangeRequest:
            enmRequired = kLevel_OnlyVBoxGuest;
            break;

        /*
         * Trusted users apps only.
         */
        case VMMDevReq_QueryCredentials:
        case VMMDevReq_ReportCredentialsJudgement:
        case VMMDevReq_RegisterSharedModule:
        case VMMDevReq_UnregisterSharedModule:
        case VMMDevReq_WriteCoreDump:
        case VMMDevReq_GetCpuHotPlugRequest:
        case VMMDevReq_SetCpuHotPlugStatus:
        case VMMDevReq_CheckSharedModules:
        case VMMDevReq_GetPageSharingStatus:
        case VMMDevReq_DebugIsPageShared:
        case VMMDevReq_ReportGuestStats:
        case VMMDevReq_ReportGuestUserState:
        case VMMDevReq_GetStatisticsChangeRequest:
        case VMMDevReq_ChangeMemBalloon:
            enmRequired = kLevel_TrustedUsers;
            break;

        /*
         * Anyone. But not for CapsAcquire mode
         */
        case VMMDevReq_SetGuestCapabilities:
        {
            VMMDevReqGuestCapabilities2 *pCaps = (VMMDevReqGuestCapabilities2*)pReqHdr;
            uint32_t fAcquireCaps = 0;
            if (!VBoxGuestCommonGuestCapsModeSet(pDevExt, pCaps->u32OrMask, false, &fAcquireCaps))
            {
                AssertFailed();
                LogRel(("calling caps set for acquired caps %d\n", pCaps->u32OrMask));
                enmRequired = kLevel_NoOne;
                break;
            }
            /* hack to adjust the notcaps.
             * @todo: move to a better place
             * user-mode apps are allowed to pass any mask to the notmask,
             * the driver cleans up them accordingly */
            pCaps->u32NotMask &= ~fAcquireCaps;
            /* do not break, make it fall through to the below enmRequired setting */
        }
        /*
         * Anyone.
         */
        case VMMDevReq_GetMouseStatus:
        case VMMDevReq_SetMouseStatus:
        case VMMDevReq_SetPointerShape:
        case VMMDevReq_GetHostVersion:
        case VMMDevReq_Idle:
        case VMMDevReq_GetHostTime:
        case VMMDevReq_SetPowerStatus:
        case VMMDevReq_AcknowledgeEvents:
        case VMMDevReq_CtlGuestFilterMask:
        case VMMDevReq_ReportGuestStatus:
        case VMMDevReq_GetDisplayChangeRequest:
        case VMMDevReq_VideoModeSupported:
        case VMMDevReq_GetHeightReduction:
        case VMMDevReq_GetDisplayChangeRequest2:
        case VMMDevReq_VideoModeSupported2:
        case VMMDevReq_VideoAccelEnable:
        case VMMDevReq_VideoAccelFlush:
        case VMMDevReq_VideoSetVisibleRegion:
        case VMMDevReq_GetDisplayChangeRequestEx:
        case VMMDevReq_GetSeamlessChangeRequest:
        case VMMDevReq_GetVRDPChangeRequest:
        case VMMDevReq_LogString:
        case VMMDevReq_GetSessionId:
            enmRequired = kLevel_AllUsers;
            break;

        /*
         * Depends on the request parameters...
         */
        /** @todo this have to be changed into an I/O control and the facilities
         *        tracked in the session so they can automatically be failed when the
         *        session terminates without reporting the new status.
         *
         *  The information presented by IGuest is not reliable without this! */
        case VMMDevReq_ReportGuestCapabilities:
            switch (((VMMDevReportGuestStatus const *)pReqHdr)->guestStatus.facility)
            {
                case VBoxGuestFacilityType_All:
                case VBoxGuestFacilityType_VBoxGuestDriver:
                    enmRequired = kLevel_OnlyVBoxGuest;
                    break;
                case VBoxGuestFacilityType_VBoxService:
                    enmRequired = kLevel_TrustedUsers;
                    break;
                case VBoxGuestFacilityType_VBoxTrayClient:
                case VBoxGuestFacilityType_Seamless:
                case VBoxGuestFacilityType_Graphics:
                default:
                    enmRequired = kLevel_AllUsers;
                    break;
            }
            break;
    }

    /*
     * Check against the session.
     */
    switch (enmRequired)
    {
        default:
        case kLevel_NoOne:
            break;
        case kLevel_OnlyVBoxGuest:
        case kLevel_OnlyKernel:
            if (pSession->R0Process == NIL_RTR0PROCESS)
                return VINF_SUCCESS;
            break;
        case kLevel_TrustedUsers:
        case kLevel_AllUsers:
            return VINF_SUCCESS;
    }

    return VERR_PERMISSION_DENIED;
}

static int VBoxGuestCommonIOCtl_VMMRequest(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                                           VMMDevRequestHeader *pReqHdr, size_t cbData, size_t *pcbDataReturned)
{
    int                     rc;
    VMMDevRequestHeader    *pReqCopy;

    /*
     * Validate the header and request size.
     */
    const VMMDevRequestType enmType   = pReqHdr->requestType;
    const uint32_t          cbReq     = pReqHdr->size;
    const uint32_t          cbMinSize = (uint32_t)vmmdevGetRequestSize(enmType);

    LogFlowFunc(("Type=%d\n", pReqHdr->requestType));

    if (cbReq < cbMinSize)
    {
        LogRelFunc(("Invalid hdr size %#x, expected >= %#x; type=%#x!!\n",
                    cbReq, cbMinSize, enmType));
        return VERR_INVALID_PARAMETER;
    }
    if (cbReq > cbData)
    {
        LogRelFunc(("Invalid size %#x, expected >= %#x (hdr); type=%#x!!\n",
                    cbData, cbReq, enmType));
        return VERR_INVALID_PARAMETER;
    }
    rc = VbglGRVerify(pReqHdr, cbData);
    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("Invalid header: size %#x, expected >= %#x (hdr); type=%#x; rc=%Rrc\n",
                     cbData, cbReq, enmType, rc));
        return rc;
    }

    rc = VBoxGuestCheckIfVMMReqAllowed(pDevExt, pSession, enmType, pReqHdr);
    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("Operation not allowed! type=%#x, rc=%Rrc\n", enmType, rc));
        return rc;
    }

    /*
     * Make a copy of the request in the physical memory heap so
     * the VBoxGuestLibrary can more easily deal with the request.
     * (This is really a waste of time since the OS or the OS specific
     * code has already buffered or locked the input/output buffer, but
     * it does makes things a bit simpler wrt to phys address.)
     */
    rc = VbglGRAlloc(&pReqCopy, cbReq, enmType);
    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("Failed to allocate %u (%#x) bytes to cache the request; rc=%Rrc\n",
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
        LogFlowFunc(("VbglGRPerform failed; rc=%Rrc\n", rc));
    else
    {
        LogFlowFunc(("Request execution failed; VMMDev rc=%Rrc\n",
                     pReqCopy->rc));
        rc = pReqCopy->rc;
    }

    VbglGRFree(pReqCopy);
    return rc;
}


static int VBoxGuestCommonIOCtl_CtlFilterMask(PVBOXGUESTDEVEXT pDevExt,
                                              PVBOXGUESTSESSION pSession,
                                              VBoxGuestFilterMaskInfo *pInfo)
{
    int rc;

    if ((pInfo->u32OrMask | pInfo->u32NotMask) & ~VMMDEV_EVENT_VALID_EVENT_MASK)
        return VERR_INVALID_PARAMETER;
    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    pSession->fFilterMask |= pInfo->u32OrMask;
    pSession->fFilterMask &= ~pInfo->u32NotMask;
    RTSpinlockRelease(pDevExt->SessionSpinlock);
    rc = vboxGuestUpdateHostFlags(pDevExt, pSession, HostFlags_FilterMask);
    return rc;
}


static int VBoxGuestCommonIOCtl_SetCapabilities(PVBOXGUESTDEVEXT pDevExt,
                                            PVBOXGUESTSESSION pSession,
                                            VBoxGuestSetCapabilitiesInfo *pInfo)
{
    int rc;

    if (  (pInfo->u32OrMask | pInfo->u32NotMask)
        & ~VMMDEV_GUEST_CAPABILITIES_MASK)
        return VERR_INVALID_PARAMETER;
    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    pSession->fCapabilities |= pInfo->u32OrMask;
    pSession->fCapabilities &= ~pInfo->u32NotMask;
    RTSpinlockRelease(pDevExt->SessionSpinlock);
    rc = vboxGuestUpdateHostFlags(pDevExt, pSession, HostFlags_Capabilities);
    return rc;
}


/**
 * Sets the mouse status features for this session and updates them
 * globally.
 *
 * @returns VBox status code.
 *
 * @param   pDevExt             The device extention.
 * @param   pSession            The session.
 * @param   fFeatures           New bitmap of enabled features.
 */
static int vboxGuestCommonIOCtl_SetMouseStatus(PVBOXGUESTDEVEXT pDevExt,
                                               PVBOXGUESTSESSION pSession,
                                               uint32_t fFeatures)
{
    int rc;

    if (fFeatures & ~VMMDEV_MOUSE_GUEST_MASK)
        return VERR_INVALID_PARAMETER;
    /* Since this is more of a negative feature we invert it to get the real
     * feature (when the guest does not need the host cursor). */
    fFeatures ^= VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR;
    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    pSession->fMouseStatus = fFeatures;
    RTSpinlockRelease(pDevExt->SessionSpinlock);
    rc = vboxGuestUpdateHostFlags(pDevExt, pSession, HostFlags_MouseStatus);
    return rc;
}

#ifdef VBOX_WITH_HGCM

AssertCompile(RT_INDEFINITE_WAIT == (uint32_t)RT_INDEFINITE_WAIT); /* assumed by code below */

/** Worker for VBoxGuestHGCMAsyncWaitCallback*. */
static int VBoxGuestHGCMAsyncWaitCallbackWorker(VMMDevHGCMRequestHeader volatile *pHdr, PVBOXGUESTDEVEXT pDevExt,
                                                 bool fInterruptible, uint32_t cMillies)
{
    int rc;

    /*
     * Check to see if the condition was met by the time we got here.
     *
     * We create a simple poll loop here for dealing with out-of-memory
     * conditions since the caller isn't necessarily able to deal with
     * us returning too early.
     */
    PVBOXGUESTWAIT pWait;
    for (;;)
    {
        RTSpinlockAcquire(pDevExt->EventSpinlock);
        if ((pHdr->fu32Flags & VBOX_HGCM_REQ_DONE) != 0)
        {
            RTSpinlockRelease(pDevExt->EventSpinlock);
            return VINF_SUCCESS;
        }
        RTSpinlockRelease(pDevExt->EventSpinlock);

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
    RTSpinlockAcquire(pDevExt->EventSpinlock);
    RTListAppend(&pDevExt->HGCMWaitList, &pWait->ListNode);
    if ((pHdr->fu32Flags & VBOX_HGCM_REQ_DONE) != 0)
    {
        VBoxGuestWaitFreeLocked(pDevExt, pWait);
        RTSpinlockRelease(pDevExt->EventSpinlock);
        return VINF_SUCCESS;
    }
    RTSpinlockRelease(pDevExt->EventSpinlock);

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
        LogRelFlow(("wait failed! %Rrc\n", rc));

    VBoxGuestWaitFreeUnlocked(pDevExt, pWait);
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
    LogFlowFunc(("requestType=%d\n", pHdr->header.requestType));
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
    LogFlowFunc(("requestType=%d\n", pHdr->header.requestType));
    return VBoxGuestHGCMAsyncWaitCallbackWorker((VMMDevHGCMRequestHeader volatile *)pHdr,
                                                pDevExt,
                                                true /* fInterruptible */,
                                                u32User /* cMillies */ );

}


static int VBoxGuestCommonIOCtl_HGCMConnect(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                                            VBoxGuestHGCMConnectInfo *pInfo, size_t *pcbDataReturned)
{
    int rc;

    /*
     * The VbglHGCMConnect call will invoke the callback if the HGCM
     * call is performed in an ASYNC fashion. The function is not able
     * to deal with cancelled requests.
     */
    LogFlowFunc(("%.128s\n",
                   pInfo->Loc.type == VMMDevHGCMLoc_LocalHost || pInfo->Loc.type == VMMDevHGCMLoc_LocalHost_Existing
                 ? pInfo->Loc.u.host.achName : "<not local host>"));

    rc = VbglR0HGCMInternalConnect(pInfo, VBoxGuestHGCMAsyncWaitCallback, pDevExt, RT_INDEFINITE_WAIT);
    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("u32Client=%RX32 result=%Rrc (rc=%Rrc)\n",
                     pInfo->u32ClientID, pInfo->result, rc));
        if (RT_SUCCESS(pInfo->result))
        {
            /*
             * Append the client id to the client id table.
             * If the table has somehow become filled up, we'll disconnect the session.
             */
            unsigned i;
            RTSpinlockAcquire(pDevExt->SessionSpinlock);
            for (i = 0; i < RT_ELEMENTS(pSession->aHGCMClientIds); i++)
                if (!pSession->aHGCMClientIds[i])
                {
                    pSession->aHGCMClientIds[i] = pInfo->u32ClientID;
                    break;
                }
            RTSpinlockRelease(pDevExt->SessionSpinlock);
            if (i >= RT_ELEMENTS(pSession->aHGCMClientIds))
            {
                static unsigned             s_cErrors = 0;
                VBoxGuestHGCMDisconnectInfo Info;

                if (s_cErrors++ < 32)
                    LogRelFunc(("Too many HGCMConnect calls for one session\n"));

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
    int             rc;
    const uint32_t  u32ClientId = pInfo->u32ClientID;
    unsigned        i;
    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    for (i = 0; i < RT_ELEMENTS(pSession->aHGCMClientIds); i++)
        if (pSession->aHGCMClientIds[i] == u32ClientId)
        {
            pSession->aHGCMClientIds[i] = UINT32_MAX;
            break;
        }
    RTSpinlockRelease(pDevExt->SessionSpinlock);
    if (i >= RT_ELEMENTS(pSession->aHGCMClientIds))
    {
        static unsigned s_cErrors = 0;
        if (s_cErrors++ > 32)
            LogRelFunc(("u32Client=%RX32\n", u32ClientId));
        return VERR_INVALID_HANDLE;
    }

    /*
     * The VbglHGCMConnect call will invoke the callback if the HGCM
     * call is performed in an ASYNC fashion. The function is not able
     * to deal with cancelled requests.
     */
    LogFlowFunc(("u32Client=%RX32\n", pInfo->u32ClientID));
    rc = VbglR0HGCMInternalDisconnect(pInfo, VBoxGuestHGCMAsyncWaitCallback, pDevExt, RT_INDEFINITE_WAIT);
    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("Disconnected with rc=%Rrc\n", pInfo->result)); /** int32_t vs. int! */
        if (pcbDataReturned)
            *pcbDataReturned = sizeof(*pInfo);
    }

    /* Update the client id array according to the result. */
    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    if (pSession->aHGCMClientIds[i] == UINT32_MAX)
        pSession->aHGCMClientIds[i] = RT_SUCCESS(rc) && RT_SUCCESS(pInfo->result) ? 0 : u32ClientId;
    RTSpinlockRelease(pDevExt->SessionSpinlock);

    return rc;
}


static int VBoxGuestCommonIOCtl_HGCMCall(PVBOXGUESTDEVEXT pDevExt,
                                         PVBOXGUESTSESSION pSession,
                                         VBoxGuestHGCMCallInfo *pInfo,
                                         uint32_t cMillies, bool fInterruptible, bool f32bit, bool fUserData,
                                         size_t cbExtra, size_t cbData, size_t *pcbDataReturned)
{
    const uint32_t  u32ClientId = pInfo->u32ClientID;
    uint32_t        fFlags;
    size_t          cbActual;
    unsigned        i;
    int             rc;

    /*
     * Some more validations.
     */
    if (pInfo->cParms > 4096) /* (Just make sure it doesn't overflow the next check.) */
    {
        LogRelFunc(("cParm=%RX32 is not sane\n", pInfo->cParms));
        return VERR_INVALID_PARAMETER;
    }

    cbActual = cbExtra + sizeof(*pInfo);
#ifdef RT_ARCH_AMD64
    if (f32bit)
        cbActual += pInfo->cParms * sizeof(HGCMFunctionParameter32);
    else
#endif
        cbActual += pInfo->cParms * sizeof(HGCMFunctionParameter);
    if (cbData < cbActual)
    {
        LogRelFunc(("cbData=%#zx (%zu) required size is %#zx (%zu)\n",
                    cbData, cbData, cbActual, cbActual));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Validate the client id.
     */
    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    for (i = 0; i < RT_ELEMENTS(pSession->aHGCMClientIds); i++)
        if (pSession->aHGCMClientIds[i] == u32ClientId)
            break;
    RTSpinlockRelease(pDevExt->SessionSpinlock);
    if (RT_UNLIKELY(i >= RT_ELEMENTS(pSession->aHGCMClientIds)))
    {
        static unsigned s_cErrors = 0;
        if (s_cErrors++ > 32)
            LogRelFunc(("Invalid handle; u32Client=%RX32\n", u32ClientId));
        return VERR_INVALID_HANDLE;
    }

    /*
     * The VbglHGCMCall call will invoke the callback if the HGCM
     * call is performed in an ASYNC fashion. This function can
     * deal with cancelled requests, so we let user more requests
     * be interruptible (should add a flag for this later I guess).
     */
    LogFlowFunc(("u32Client=%RX32\n", pInfo->u32ClientID));
    fFlags = !fUserData && pSession->R0Process == NIL_RTR0PROCESS ? VBGLR0_HGCMCALL_F_KERNEL : VBGLR0_HGCMCALL_F_USER;
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
        LogFlowFunc(("Result rc=%Rrc\n", pInfo->result)); /** int32_t vs. int! */
        if (pcbDataReturned)
            *pcbDataReturned = cbActual;
    }
    else
    {
        if (   rc != VERR_INTERRUPTED
            && rc != VERR_TIMEOUT)
        {
            static unsigned s_cErrors = 0;
            if (s_cErrors++ < 32)
                LogRelFunc(("%s-bit call failed; rc=%Rrc\n",
                            f32bit ? "32" : "64", rc));
        }
        else
            LogFlowFunc(("%s-bit call failed; rc=%Rrc\n",
                         f32bit ? "32" : "64", rc));
    }
    return rc;
}
#endif /* VBOX_WITH_HGCM */


/**
 * Handle VBOXGUEST_IOCTL_CHECK_BALLOON from R3.
 *
 * Ask the host for the size of the balloon and try to set it accordingly.  If
 * this approach fails because it's not supported, return with fHandleInR3 set
 * and let the user land supply memory we can lock via the other ioctl.
 *
 * @returns VBox status code.
 *
 * @param   pDevExt             The device extension.
 * @param   pSession            The session.
 * @param   pInfo               The output buffer.
 * @param   pcbDataReturned     Where to store the amount of returned data. Can
 *                              be NULL.
 */
static int VBoxGuestCommonIOCtl_CheckMemoryBalloon(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                                                   VBoxGuestCheckBalloonInfo *pInfo, size_t *pcbDataReturned)
{
    LogFlowFuncEnter();

    int rc = RTSemFastMutexRequest(pDevExt->MemBalloon.hMtx);
    AssertRCReturn(rc, rc);

    /*
     * The first user trying to query/change the balloon becomes the
     * owner and owns it until the session is closed (vboxGuestCloseMemBalloon).
     */
    if (   pDevExt->MemBalloon.pOwner != pSession
        && pDevExt->MemBalloon.pOwner == NULL)
    {
        pDevExt->MemBalloon.pOwner = pSession;
    }

    if (pDevExt->MemBalloon.pOwner == pSession)
    {
        VMMDevGetMemBalloonChangeRequest *pReq;
        rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(VMMDevGetMemBalloonChangeRequest),
                         VMMDevReq_GetMemBalloonChangeRequest);
        if (RT_SUCCESS(rc))
        {
            /*
             * This is a response to that event. Setting this bit means that
             * we request the value from the host and change the guest memory
             * balloon according to this value.
             */
            pReq->eventAck = VMMDEV_EVENT_BALLOON_CHANGE_REQUEST;
            rc = VbglGRPerform(&pReq->header);
            if (RT_SUCCESS(rc))
            {
                Assert(pDevExt->MemBalloon.cMaxChunks == pReq->cPhysMemChunks || pDevExt->MemBalloon.cMaxChunks == 0);
                pDevExt->MemBalloon.cMaxChunks = pReq->cPhysMemChunks;

                pInfo->cBalloonChunks = pReq->cBalloonChunks;
                pInfo->fHandleInR3    = false;

                rc = vboxGuestSetBalloonSizeKernel(pDevExt, pReq->cBalloonChunks, &pInfo->fHandleInR3);
                /* Ignore various out of memory failures. */
                if (   rc == VERR_NO_MEMORY
                    || rc == VERR_NO_PHYS_MEMORY
                    || rc == VERR_NO_CONT_MEMORY)
                    rc = VINF_SUCCESS;

                if (pcbDataReturned)
                    *pcbDataReturned = sizeof(VBoxGuestCheckBalloonInfo);
            }
            else
                LogRelFunc(("VbglGRPerform failed; rc=%Rrc\n", rc));
            VbglGRFree(&pReq->header);
        }
    }
    else
        rc = VERR_PERMISSION_DENIED;

    RTSemFastMutexRelease(pDevExt->MemBalloon.hMtx);

    LogFlowFunc(("Returns %Rrc\n", rc));
    return rc;
}


/**
 * Handle a request for changing the memory balloon.
 *
 * @returns VBox status code.
 *
 * @param   pDevExt             The device extention.
 * @param   pSession            The session.
 * @param   pInfo               The change request structure (input).
 * @param   pcbDataReturned     Where to store the amount of returned data. Can
 *                              be NULL.
 */
static int VBoxGuestCommonIOCtl_ChangeMemoryBalloon(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                                                    VBoxGuestChangeBalloonInfo *pInfo, size_t *pcbDataReturned)
{
    int rc = RTSemFastMutexRequest(pDevExt->MemBalloon.hMtx);
    AssertRCReturn(rc, rc);

    if (!pDevExt->MemBalloon.fUseKernelAPI)
    {
        /*
         * The first user trying to query/change the balloon becomes the
         * owner and owns it until the session is closed (vboxGuestCloseMemBalloon).
         */
        if (   pDevExt->MemBalloon.pOwner != pSession
            && pDevExt->MemBalloon.pOwner == NULL)
            pDevExt->MemBalloon.pOwner = pSession;

        if (pDevExt->MemBalloon.pOwner == pSession)
        {
            rc = vboxGuestSetBalloonSizeFromUser(pDevExt, pSession, pInfo->u64ChunkAddr,
                                                 !!pInfo->fInflate);
            if (pcbDataReturned)
                *pcbDataReturned = 0;
        }
        else
            rc = VERR_PERMISSION_DENIED;
    }
    else
        rc = VERR_PERMISSION_DENIED;

    RTSemFastMutexRelease(pDevExt->MemBalloon.hMtx);
    return rc;
}


/**
 * Handle a request for writing a core dump of the guest on the host.
 *
 * @returns VBox status code.
 *
 * @param pDevExt               The device extension.
 * @param pInfo                 The output buffer.
 */
static int VBoxGuestCommonIOCtl_WriteCoreDump(PVBOXGUESTDEVEXT pDevExt, VBoxGuestWriteCoreDump *pInfo)
{
    VMMDevReqWriteCoreDump *pReq = NULL;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(VMMDevReqWriteCoreDump),
                         VMMDevReq_WriteCoreDump);
    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("Failed to allocate %u (%#x) bytes to cache the request; rc=%Rrc\n",
                     sizeof(VMMDevReqWriteCoreDump), sizeof(VMMDevReqWriteCoreDump), rc));
        return rc;
    }

    pReq->fFlags = pInfo->fFlags;
    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
        LogFlowFunc(("VbglGRPerform failed, rc=%Rrc\n", rc));

    VbglGRFree(&pReq->header);
    return rc;
}


/**
 * Guest backdoor logging.
 *
 * @returns VBox status code.
 *
 * @param   pDevExt             The device extension.
 * @param   pch                 The log message (need not be NULL terminated).
 * @param   cbData              Size of the buffer.
 * @param   pcbDataReturned     Where to store the amount of returned data. Can be NULL.
 */
static int VBoxGuestCommonIOCtl_Log(PVBOXGUESTDEVEXT pDevExt, const char *pch, size_t cbData, size_t *pcbDataReturned, bool fUserSession)
{
    NOREF(pch);
    NOREF(cbData);
    if (pDevExt->fLoggingEnabled)
        RTLogBackdoorPrintf("%.*s", cbData, pch);
    else if (!fUserSession)
        LogRel(("%.*s", cbData, pch));
    else
        Log(("%.*s", cbData, pch));
    if (pcbDataReturned)
        *pcbDataReturned = 0;
    return VINF_SUCCESS;
}

static bool VBoxGuestCommonGuestCapsValidateValues(uint32_t fCaps)
{
    if (fCaps & (~(VMMDEV_GUEST_SUPPORTS_SEAMLESS | VMMDEV_GUEST_SUPPORTS_GUEST_HOST_WINDOW_MAPPING | VMMDEV_GUEST_SUPPORTS_GRAPHICS)))
        return false;

    return true;
}

/** Check whether any unreported VMM device events should be reported to any of
 * the currently listening sessions.  In addition, report any events in
 * @a fGenFakeEvents.
 * @note This is called by GUEST_CAPS_ACQUIRE in case any pending events can now
 *       be dispatched to the session which acquired capabilities.  The fake
 *       events are a hack to wake up threads in that session which would not
 *       otherwise be woken.
 * @todo Why not just use CANCEL_ALL_WAITEVENTS to do the waking up rather than
 *       adding additional code to the driver?
 * @todo Why does acquiring capabilities block and unblock events?  Capabilities
 *       are supposed to control what is reported to the host, we already have
 *       separate requests for blocking and unblocking events. */
static void VBoxGuestCommonCheckEvents(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, uint32_t fGenFakeEvents)
{
    RTSpinlockAcquire(pDevExt->EventSpinlock);
    uint32_t fEvents = fGenFakeEvents | pDevExt->f32PendingEvents;
    PVBOXGUESTWAIT  pWait;
    PVBOXGUESTWAIT  pSafe;

    RTListForEachSafe(&pDevExt->WaitList, pWait, pSafe, VBOXGUESTWAIT, ListNode)
    {
        uint32_t fHandledEvents = VBoxGuestCommonGetHandledEventsLocked(pDevExt, pWait->pSession);
        if (    (pWait->fReqEvents & fEvents & fHandledEvents)
                    &&  !pWait->fResEvents)
        {
            pWait->fResEvents = pWait->fReqEvents & fEvents & fHandledEvents;
            Assert(!(fGenFakeEvents & pWait->fResEvents) || pSession == pWait->pSession);
            fEvents &= ~pWait->fResEvents;
            RTListNodeRemove(&pWait->ListNode);
#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
            RTListAppend(&pDevExt->WakeUpList, &pWait->ListNode);
#else
            RTListAppend(&pDevExt->WokenUpList, &pWait->ListNode);
            int rc = RTSemEventMultiSignal(pWait->Event);
            AssertRC(rc);
#endif
            if (!fEvents)
                break;
        }
    }
    ASMAtomicWriteU32(&pDevExt->f32PendingEvents, fEvents);

    RTSpinlockRelease(pDevExt->EventSpinlock);

#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
    VBoxGuestWaitDoWakeUps(pDevExt);
#endif
}

/** Switch the capabilities in @a fOrMask to "acquire" mode if they are not
 * already in "set" mode.  If @a enmFlags is not set to
 * VBOXGUESTCAPSACQUIRE_FLAGS_CONFIG_ACQUIRE_MODE, also try to acquire those
 * capabilities for the current session and release those in @a fNotFlag. */
static int VBoxGuestCommonGuestCapsAcquire(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, uint32_t fOrMask, uint32_t fNotMask, VBOXGUESTCAPSACQUIRE_FLAGS enmFlags)
{
    uint32_t fSetCaps = 0;

    if (!VBoxGuestCommonGuestCapsValidateValues(fOrMask))
    {
        LogRelFunc(("pSession(0x%p), OR(0x%x), NOT(0x%x), flags(0x%x) -- invalid fOrMask\n",
                    pSession, fOrMask, fNotMask, enmFlags));
        return VERR_INVALID_PARAMETER;
    }

    if (   enmFlags != VBOXGUESTCAPSACQUIRE_FLAGS_CONFIG_ACQUIRE_MODE
        && enmFlags != VBOXGUESTCAPSACQUIRE_FLAGS_NONE)
    {
        LogRelFunc(("pSession(0x%p), OR(0x%x), NOT(0x%x), flags(0x%x) -- invalid enmFlags %d\n",
                    pSession, fOrMask, fNotMask, enmFlags));
        return VERR_INVALID_PARAMETER;
    }

    if (!VBoxGuestCommonGuestCapsModeSet(pDevExt, fOrMask, true, &fSetCaps))
    {
        LogRelFunc(("pSession(0x%p), OR(0x%x), NOT(0x%x), flags(0x%x) -- calling caps acquire for set caps\n",
                    pSession, fOrMask, fNotMask, enmFlags));
        return VERR_INVALID_STATE;
    }

    if (enmFlags & VBOXGUESTCAPSACQUIRE_FLAGS_CONFIG_ACQUIRE_MODE)
    {
        LogRelFunc(("pSession(0x%p), OR(0x%x), NOT(0x%x), flags(0x%x) -- configured acquire caps: 0x%x\n",
                    pSession, fOrMask, fNotMask, enmFlags));
        return VINF_SUCCESS;
    }

    /* the fNotMask no need to have all values valid,
     * invalid ones will simply be ignored */
    uint32_t fCurrentOwnedCaps;
    uint32_t fSessionNotCaps;
    uint32_t fSessionOrCaps;
    uint32_t fOtherConflictingCaps;

    fNotMask &= ~fOrMask;

    RTSpinlockAcquire(pDevExt->EventSpinlock);

    fCurrentOwnedCaps      = pSession->u32AquiredGuestCaps;
    fSessionNotCaps        = fCurrentOwnedCaps & fNotMask;
    fSessionOrCaps         = fOrMask & ~fCurrentOwnedCaps;
    fOtherConflictingCaps  = pDevExt->u32GuestCaps & ~fCurrentOwnedCaps;
    fOtherConflictingCaps &= fSessionOrCaps;

    if (!fOtherConflictingCaps)
    {
        if (fSessionOrCaps)
        {
            pSession->u32AquiredGuestCaps |= fSessionOrCaps;
            pDevExt->u32GuestCaps |= fSessionOrCaps;
        }

        if (fSessionNotCaps)
        {
            pSession->u32AquiredGuestCaps &= ~fSessionNotCaps;
            pDevExt->u32GuestCaps &= ~fSessionNotCaps;
        }
    }

    RTSpinlockRelease(pDevExt->EventSpinlock);

    if (fOtherConflictingCaps)
    {
        LogFlowFunc(("Caps 0x%x were busy\n", fOtherConflictingCaps));
        return VERR_RESOURCE_BUSY;
    }

    /* now do host notification outside the lock */
    if (!fSessionOrCaps && !fSessionNotCaps)
    {
        /* no changes, return */
        return VINF_SUCCESS;
    }

    int rc = VBoxGuestSetGuestCapabilities(fSessionOrCaps, fSessionNotCaps);
    if (RT_FAILURE(rc))
    {
        LogRelFunc(("VBoxGuestSetGuestCapabilities failed, rc=%Rrc\n", rc));

        /* Failure branch
         * this is generally bad since e.g. failure to release the caps may result in other sessions not being able to use it
         * so we are not trying to restore the caps back to their values before the VBoxGuestCommonGuestCapsAcquire call,
         * but just pretend everithing is OK.
         * @todo: better failure handling mechanism? */
    }

    /* success! */
    uint32_t fGenFakeEvents = 0;

    if (fSessionOrCaps & VMMDEV_GUEST_SUPPORTS_SEAMLESS)
    {
        /* generate the seamless change event so that the r3 app could synch with the seamless state
         * although this introduces a false alarming of r3 client, it still solve the problem of
         * client state inconsistency in multiuser environment */
        fGenFakeEvents |= VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST;
    }

    /* since the acquire filter mask has changed, we need to process events in any way to ensure they go from pending events field
     * to the proper (un-filtered) entries */
    VBoxGuestCommonCheckEvents(pDevExt, pSession, fGenFakeEvents);

    return VINF_SUCCESS;
}

static int VBoxGuestCommonIOCTL_GuestCapsAcquire(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, VBoxGuestCapsAquire *pAcquire)
{
    int rc = VBoxGuestCommonGuestCapsAcquire(pDevExt, pSession, pAcquire->u32OrMask, pAcquire->u32NotMask, pAcquire->enmFlags);
    if (RT_FAILURE(rc))
        LogRelFunc(("Failed, rc=%Rrc\n", rc));
    pAcquire->rc = rc;
    return VINF_SUCCESS;
}


/**
 * Common IOCtl for user to kernel and kernel to kernel communication.
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
    int rc;
    LogFlowFunc(("iFunction=%#x, pDevExt=%p, pSession=%p, pvData=%p, cbData=%zu\n",
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
            LogFunc((mnemonic ": Ring-0 only, caller is %RTproc/%p\n", \
                     pSession->Process, (uintptr_t)pSession->R0Process)); \
            return VERR_PERMISSION_DENIED; \
        } \
    } while (0)
#define CHECKRET_MIN_SIZE(mnemonic, cbMin) \
    do { \
        if (cbData < (cbMin)) \
        { \
            LogFunc((mnemonic ": cbData=%#zx (%zu) min is %#zx (%zu)\n", \
                     cbData, cbData, (size_t)(cbMin), (size_t)(cbMin))); \
            return VERR_BUFFER_OVERFLOW; \
        } \
        if ((cbMin) != 0 && !VALID_PTR(pvData)) \
        { \
            LogFunc((mnemonic ": Invalid pointer %p\n", pvData)); \
            return VERR_INVALID_POINTER; \
        } \
    } while (0)
#define CHECKRET_SIZE(mnemonic, cb) \
    do { \
        if (cbData != (cb)) \
        { \
            LogFunc((mnemonic ": cbData=%#zx (%zu) expected is %#zx (%zu)\n", \
                     cbData, cbData, (size_t)(cb), (size_t)(cb))); \
            return VERR_BUFFER_OVERFLOW; \
        } \
        if ((cb) != 0 && !VALID_PTR(pvData)) \
        { \
            LogFunc((mnemonic ": Invalid pointer %p\n", pvData)); \
            return VERR_INVALID_POINTER; \
        } \
    } while (0)


    /*
     * Deal with variably sized requests first.
     */
    rc = VINF_SUCCESS;
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
        bool fInterruptible = pSession->R0Process != NIL_RTR0PROCESS;
        CHECKRET_MIN_SIZE("HGCM_CALL", sizeof(VBoxGuestHGCMCallInfo));
        rc = VBoxGuestCommonIOCtl_HGCMCall(pDevExt, pSession, (VBoxGuestHGCMCallInfo *)pvData, RT_INDEFINITE_WAIT,
                                           fInterruptible, false /*f32bit*/, false /* fUserData */,
                                           0, cbData, pcbDataReturned);
    }
    else if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL_TIMED(0)))
    {
        VBoxGuestHGCMCallInfoTimed *pInfo = (VBoxGuestHGCMCallInfoTimed *)pvData;
        CHECKRET_MIN_SIZE("HGCM_CALL_TIMED", sizeof(VBoxGuestHGCMCallInfoTimed));
        rc = VBoxGuestCommonIOCtl_HGCMCall(pDevExt, pSession, &pInfo->info, pInfo->u32Timeout,
                                           !!pInfo->fInterruptible || pSession->R0Process != NIL_RTR0PROCESS,
                                           false /*f32bit*/, false /* fUserData */,
                                           RT_OFFSETOF(VBoxGuestHGCMCallInfoTimed, info), cbData, pcbDataReturned);
    }
    else if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL_USERDATA(0)))
    {
        bool fInterruptible = true;
        CHECKRET_MIN_SIZE("HGCM_CALL", sizeof(VBoxGuestHGCMCallInfo));
        rc = VBoxGuestCommonIOCtl_HGCMCall(pDevExt, pSession, (VBoxGuestHGCMCallInfo *)pvData, RT_INDEFINITE_WAIT,
                                           fInterruptible, false /*f32bit*/, true /* fUserData */,
                                           0, cbData, pcbDataReturned);
    }
# ifdef RT_ARCH_AMD64
    else if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL_32(0)))
    {
        bool fInterruptible = pSession->R0Process != NIL_RTR0PROCESS;
        CHECKRET_MIN_SIZE("HGCM_CALL", sizeof(VBoxGuestHGCMCallInfo));
        rc = VBoxGuestCommonIOCtl_HGCMCall(pDevExt, pSession, (VBoxGuestHGCMCallInfo *)pvData, RT_INDEFINITE_WAIT,
                                           fInterruptible, true /*f32bit*/, false /* fUserData */,
                                           0, cbData, pcbDataReturned);
    }
    else if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL_TIMED_32(0)))
    {
        CHECKRET_MIN_SIZE("HGCM_CALL_TIMED", sizeof(VBoxGuestHGCMCallInfoTimed));
        VBoxGuestHGCMCallInfoTimed *pInfo = (VBoxGuestHGCMCallInfoTimed *)pvData;
        rc = VBoxGuestCommonIOCtl_HGCMCall(pDevExt, pSession, &pInfo->info, pInfo->u32Timeout,
                                           !!pInfo->fInterruptible || pSession->R0Process != NIL_RTR0PROCESS,
                                           true /*f32bit*/, false /* fUserData */,
                                           RT_OFFSETOF(VBoxGuestHGCMCallInfoTimed, info), cbData, pcbDataReturned);
    }
# endif
#endif /* VBOX_WITH_HGCM */
    else if (VBOXGUEST_IOCTL_STRIP_SIZE(iFunction) == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_LOG(0)))
    {
        CHECKRET_MIN_SIZE("LOG", 1);
        rc = VBoxGuestCommonIOCtl_Log(pDevExt, (char *)pvData, cbData, pcbDataReturned, pSession->fUserSession);
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

#ifndef RT_OS_WINDOWS  /* Windows has its own implementation of this. */
            case VBOXGUEST_IOCTL_SET_MOUSE_NOTIFY_CALLBACK:
                CHECKRET_RING0("SET_MOUSE_NOTIFY_CALLBACK");
                CHECKRET_SIZE("SET_MOUSE_NOTIFY_CALLBACK", sizeof(VBoxGuestMouseSetNotifyCallback));
                rc = VBoxGuestCommonIOCtl_SetMouseNotifyCallback(pDevExt, (VBoxGuestMouseSetNotifyCallback *)pvData);
                break;
#endif

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
                CHECKRET_MIN_SIZE("CTL_FILTER_MASK",
                                  sizeof(VBoxGuestFilterMaskInfo));
                rc = VBoxGuestCommonIOCtl_CtlFilterMask(pDevExt, pSession,
                                             (VBoxGuestFilterMaskInfo *)pvData);
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
#endif /* VBOX_WITH_HGCM */

            case VBOXGUEST_IOCTL_CHECK_BALLOON:
                CHECKRET_MIN_SIZE("CHECK_MEMORY_BALLOON", sizeof(VBoxGuestCheckBalloonInfo));
                rc = VBoxGuestCommonIOCtl_CheckMemoryBalloon(pDevExt, pSession, (VBoxGuestCheckBalloonInfo *)pvData, pcbDataReturned);
                break;

            case VBOXGUEST_IOCTL_CHANGE_BALLOON:
                CHECKRET_MIN_SIZE("CHANGE_MEMORY_BALLOON", sizeof(VBoxGuestChangeBalloonInfo));
                rc = VBoxGuestCommonIOCtl_ChangeMemoryBalloon(pDevExt, pSession, (VBoxGuestChangeBalloonInfo *)pvData, pcbDataReturned);
                break;

            case VBOXGUEST_IOCTL_WRITE_CORE_DUMP:
                CHECKRET_MIN_SIZE("WRITE_CORE_DUMP", sizeof(VBoxGuestWriteCoreDump));
                rc = VBoxGuestCommonIOCtl_WriteCoreDump(pDevExt, (VBoxGuestWriteCoreDump *)pvData);
                break;

            case VBOXGUEST_IOCTL_SET_MOUSE_STATUS:
                CHECKRET_SIZE("SET_MOUSE_STATUS", sizeof(uint32_t));
                rc = vboxGuestCommonIOCtl_SetMouseStatus(pDevExt, pSession,
                                                         *(uint32_t *)pvData);
                break;

#ifdef VBOX_WITH_DPC_LATENCY_CHECKER
            case VBOXGUEST_IOCTL_DPC_LATENCY_CHECKER:
                CHECKRET_SIZE("DPC_LATENCY_CHECKER", 0);
                rc = VbgdNtIOCtl_DpcLatencyChecker();
                break;
#endif

            case VBOXGUEST_IOCTL_GUEST_CAPS_ACQUIRE:
                CHECKRET_SIZE("GUEST_CAPS_ACQUIRE", sizeof(VBoxGuestCapsAquire));
                rc = VBoxGuestCommonIOCTL_GuestCapsAcquire(pDevExt, pSession, (VBoxGuestCapsAquire*)pvData);
                *pcbDataReturned = sizeof(VBoxGuestCapsAquire);
                break;

            case VBOXGUEST_IOCTL_SET_GUEST_CAPABILITIES:
                CHECKRET_MIN_SIZE("SET_GUEST_CAPABILITIES",
                                  sizeof(VBoxGuestSetCapabilitiesInfo));
                rc = VBoxGuestCommonIOCtl_SetCapabilities(pDevExt, pSession,
                                        (VBoxGuestSetCapabilitiesInfo *)pvData);
                break;

            default:
            {
                LogRelFunc(("Unknown request iFunction=%#x, stripped size=%#x\n",
                            iFunction, VBOXGUEST_IOCTL_STRIP_SIZE(iFunction)));
                rc = VERR_NOT_SUPPORTED;
                break;
            }
        }
    }

    LogFlowFunc(("Returning %Rrc *pcbDataReturned=%zu\n", rc, pcbDataReturned ? *pcbDataReturned : 0));
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
    bool                            fMousePositionChanged = false;
    VMMDevEvents volatile          *pReq                  = pDevExt->pIrqAckEvents;
    int                             rc                    = 0;
    bool                            fOurIrq;

    /*
     * Make sure we've initialized the device extension.
     */
    if (RT_UNLIKELY(!pReq))
        return false;

    /*
     * Enter the spinlock and check if it's our IRQ or not.
     */
    RTSpinlockAcquire(pDevExt->EventSpinlock);
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
            PVBOXGUESTWAIT  pSafe;

#ifndef DEBUG_andy
            LogFlowFunc(("Acknowledge events succeeded: %#RX32\n", fEvents));
#endif
            /*
             * VMMDEV_EVENT_MOUSE_POSITION_CHANGED can only be polled for.
             */
            if (fEvents & VMMDEV_EVENT_MOUSE_POSITION_CHANGED)
            {
                fMousePositionChanged = true;
                fEvents &= ~VMMDEV_EVENT_MOUSE_POSITION_CHANGED;
#ifndef RT_OS_WINDOWS
                if (pDevExt->MouseNotifyCallback.pfnNotify)
                    pDevExt->MouseNotifyCallback.pfnNotify
                                          (pDevExt->MouseNotifyCallback.pvUser);
#endif
            }

#ifdef VBOX_WITH_HGCM
            /*
             * The HGCM event/list is kind of different in that we evaluate all entries.
             */
            if (fEvents & VMMDEV_EVENT_HGCM)
            {
                RTListForEachSafe(&pDevExt->HGCMWaitList, pWait, pSafe, VBOXGUESTWAIT, ListNode)
                {
                    if (pWait->pHGCMReq->fu32Flags & VBOX_HGCM_REQ_DONE)
                    {
                        pWait->fResEvents = VMMDEV_EVENT_HGCM;
                        RTListNodeRemove(&pWait->ListNode);
# ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
                        RTListAppend(&pDevExt->WakeUpList, &pWait->ListNode);
# else
                        RTListAppend(&pDevExt->WokenUpList, &pWait->ListNode);
                        rc |= RTSemEventMultiSignal(pWait->Event);
# endif
                    }
                }
                fEvents &= ~VMMDEV_EVENT_HGCM;
            }
#endif

            /*
             * Normal FIFO waiter evaluation.
             */
            fEvents |= pDevExt->f32PendingEvents;
            RTListForEachSafe(&pDevExt->WaitList, pWait, pSafe, VBOXGUESTWAIT, ListNode)
            {
                uint32_t fHandledEvents = VBoxGuestCommonGetHandledEventsLocked(pDevExt, pWait->pSession);
                if (    (pWait->fReqEvents & fEvents & fHandledEvents)
                    &&  !pWait->fResEvents)
                {
                    pWait->fResEvents = pWait->fReqEvents & fEvents & fHandledEvents;
                    fEvents &= ~pWait->fResEvents;
                    RTListNodeRemove(&pWait->ListNode);
#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
                    RTListAppend(&pDevExt->WakeUpList, &pWait->ListNode);
#else
                    RTListAppend(&pDevExt->WokenUpList, &pWait->ListNode);
                    rc |= RTSemEventMultiSignal(pWait->Event);
#endif
                    if (!fEvents)
                        break;
                }
            }
            ASMAtomicWriteU32(&pDevExt->f32PendingEvents, fEvents);
        }
        else /* something is serious wrong... */
            LogFlowFunc(("Acknowledging events failed, rc=%Rrc (events=%#x)\n",
                         pReq->header.rc, pReq->events));
    }
#ifndef DEBUG_andy
    else
        LogFlowFunc(("Not ours\n"));
#endif

    RTSpinlockRelease(pDevExt->EventSpinlock);

#if defined(VBOXGUEST_USE_DEFERRED_WAKE_UP) && !defined(RT_OS_DARWIN) && !defined(RT_OS_WINDOWS)
    /*
     * Do wake-ups.
     * Note. On Windows this isn't possible at this IRQL, so a DPC will take
     *       care of it.  Same on darwin, doing it in the work loop callback.
     */
    VBoxGuestWaitDoWakeUps(pDevExt);
#endif

    /*
     * Work the poll and async notification queues on OSes that implements that.
     * (Do this outside the spinlock to prevent some recursive spinlocking.)
     */
    if (fMousePositionChanged)
    {
        ASMAtomicIncU32(&pDevExt->u32MousePosChangedSeq);
        VBoxGuestNativeISRMousePollEvent(pDevExt);
    }

    Assert(rc == 0);
    NOREF(rc);
    return fOurIrq;
}

