/* $Id$ */
/** @file
 * VBoxGuest - Guest Additions Driver, Common Code.
 */

/*
 * Copyright (C) 2007-2010 Oracle Corporation
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
#if defined(RT_OS_SOLARIS)
# include <iprt/rand.h>
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef VBOX_WITH_HGCM
static DECLCALLBACK(int) VBoxGuestHGCMAsyncWaitCallback(VMMDevHGCMRequestHeader *pHdrNonVolatile, void *pvUser, uint32_t u32User);
#endif
#ifdef DEBUG
static void testSetMouseStatus(void);
#endif
static int VBoxGuestCommonIOCtl_SetMouseStatus(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, uint32_t fFeatures);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static const size_t cbChangeMemBalloonReq = RT_OFFSETOF(VMMDevChangeMemBalloon, aPhysPage[VMMDEV_MEMORY_BALLOON_CHUNK_PAGES]);

#if defined(RT_OS_SOLARIS)
/**
 * Drag in the rest of IRPT since we share it with the
 * rest of the kernel modules on Solaris.
 */
PFNRT g_apfnVBoxGuestIPRTDeps[] =
{
    /* VirtioNet */
    (PFNRT)RTRandBytes,
    NULL
};
#endif  /* RT_OS_SOLARIS */


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
            pReq->hypervisorStart    = (uintptr_t)RTR0MemObjAddress(hObj);
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
        if (RT_FAILURE(rc))
            LogRel(("vboxGuestSetFilterMask: failed with rc=%Rrc\n", rc));
        VbglGRFree(&pReq->header);
    }
    return rc;
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
                LogRel(("VBoxGuestSetBalloonSizeKernel: no memory for paMemObj!\n"));
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
            Log(("VBoxGuest::VBoxGuestReinitDevExtAfterHibernation: could not report guest driver status, rc=%Rrc\n", rc));
    }
    else
        Log(("VBoxGuest::VBoxGuestReinitDevExtAfterHibernation: could not report guest information to host, rc=%Rrc\n", rc));
    Log(("VBoxGuest::VBoxGuestReinitDevExtAfterHibernation: returned with rc=%Rrc\n", rc));
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
        rc = RTR0MemObjLockUser(pMemObj, u64ChunkAddr, VMMDEV_MEMORY_BALLOON_CHUNK_SIZE,
                                RTMEM_PROT_READ | RTMEM_PROT_WRITE, NIL_RTR0PROCESS);
        if (RT_SUCCESS(rc))
        {
            rc = vboxGuestBalloonInflate(pMemObj, pReq);
            if (RT_SUCCESS(rc))
                pDevExt->MemBalloon.cChunks++;
            else
            {
                Log(("vboxGuestSetBalloonSize(inflate): failed, rc=%Rrc!\n", rc));
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
            Log(("vboxGuestSetBalloonSize(deflate): failed, rc=%Rrc!\n", rc));
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
                        LogRel(("vboxGuestCloseMemBalloon: Deflate failed with rc=%Rrc.  Will leak %u chunks.\n",
                                rc, pDevExt->MemBalloon.cChunks));
                        break;
                    }
                    pDevExt->MemBalloon.paMemObj[i] = NIL_RTR0MEMOBJ;
                    pDevExt->MemBalloon.cChunks--;
                }
                VbglGRFree(&pReq->header);
            }
            else
                LogRel(("vboxGuestCloseMemBalloon: Failed to allocate VMMDev request buffer (rc=%Rrc).  Will leak %u chunks.\n",
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
    unsigned i;

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
#ifdef VBOX_WITH_VRDP_SESSION_HANDLING
    pDevExt->fVRDPEnabled = false;
#endif
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
    for (i = 0; i < RT_ELEMENTS(pDevExt->acMouseFeatureUsage); ++i)
        pDevExt->acMouseFeatureUsage[i] = 0;
    pDevExt->fMouseStatus = 0;
    pDevExt->MouseNotifyCallback.pfnNotify = NULL;
    pDevExt->MouseNotifyCallback.pvUser = NULL;
    pDevExt->cISR = 0;

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
     * Create the wait and session spinlocks as well as the ballooning mutex.
     */
    rc = RTSpinlockCreate(&pDevExt->EventSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VBoxGuestEvent");
    if (RT_SUCCESS(rc))
        rc = RTSpinlockCreate(&pDevExt->SessionSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VBoxGuestSession");
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxGuestInitDevExt: failed to create spinlock, rc=%Rrc!\n", rc));
        if (pDevExt->EventSpinlock != NIL_RTSPINLOCK)
            RTSpinlockDestroy(pDevExt->EventSpinlock);
        return rc;
    }

    rc = RTSemFastMutexCreate(&pDevExt->MemBalloon.hMtx);
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxGuestInitDevExt: failed to create mutex, rc=%Rrc!\n", rc));
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

#ifdef DEBUG
                        testSetMouseStatus();  /* Other tests? */
#endif

                        rc = VBoxGuestReportDriverStatus(true /* Driver is active */);
                        if (RT_FAILURE(rc))
                            LogRel(("VBoxGuestInitDevExt: VBoxReportGuestDriverStatus failed, rc=%Rrc\n", rc));

                        Log(("VBoxGuestInitDevExt: returns success\n"));
                        return VINF_SUCCESS;
                    }

                    LogRel(("VBoxGuestInitDevExt: VBoxGuestSetGuestCapabilities failed, rc=%Rrc\n", rc));
                }
                else
                    LogRel(("VBoxGuestInitDevExt: vboxGuestSetFilterMask failed, rc=%Rrc\n", rc));
            }
            else
                LogRel(("VBoxGuestInitDevExt: VBoxReportGuestInfo failed, rc=%Rrc\n", rc));
            VbglGRFree((VMMDevRequestHeader *)pDevExt->pIrqAckEvents);
        }
        else
            LogRel(("VBoxGuestInitDevExt: VBoxGRAlloc failed, rc=%Rrc\n", rc));

        VbglTerminate();
    }
    else
        LogRel(("VBoxGuestInitDevExt: VbglInit failed, rc=%Rrc\n", rc));

    rc2 = RTSemFastMutexDestroy(pDevExt->MemBalloon.hMtx); AssertRC(rc2);
    rc2 = RTSpinlockDestroy(pDevExt->EventSpinlock); AssertRC(rc2);
    rc2 = RTSpinlockDestroy(pDevExt->SessionSpinlock); AssertRC(rc2);
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
    VBoxGuestSetGuestCapabilities(0, UINT32_MAX);       /* clears all capabilities */
    vboxGuestSetFilterMask(pDevExt, 0);                 /* filter all events */
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
    vboxGuestCloseMemBalloon(pDevExt, pSession);
    /* Reset any mouse status flags which the session may have set. */
    VBoxGuestCommonIOCtl_SetMouseStatus(pDevExt, pSession, 0);
    RTMemFree(pSession);
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

        RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);
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
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);
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
            RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);

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
        RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);
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
        Log(("VBoxGuestSetGuestCapabilities: failed to allocate %u (%#x) bytes to cache the request. rc=%Rrc!!\n",
             sizeof(*pReq), sizeof(*pReq), rc));
        return rc;
    }

    pReq->u32OrMask = fOr;
    pReq->u32NotMask = fNot;

    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
        Log(("VBoxGuestSetGuestCapabilities: VbglGRPerform failed, rc=%Rrc!\n", rc));

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
    Log(("VBoxGuestCommonIOCtl: GETVMMDEVPORT\n"));
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
 * @note  This function takes the session spinlock to update the callback
 *        information, but the interrupt handler will not do this.  To make
 *        sure that the interrupt handler sees a consistent structure, we
 *        set the function pointer to NULL before updating the data and only
 *        set it to the correct value once the data is updated.  Since the
 *        interrupt handler executes atomically this ensures that the data is
 *        valid if the function pointer is non-NULL.
 */
int VBoxGuestCommonIOCtl_SetMouseNotifyCallback(PVBOXGUESTDEVEXT pDevExt, VBoxGuestMouseSetNotifyCallback *pNotify)
{
    Log(("VBoxGuestCommonIOCtl: SET_MOUSE_NOTIFY_CALLBACK\n"));

    RTSpinlockAcquire(pDevExt->EventSpinlock);
    pDevExt->MouseNotifyCallback = *pNotify;
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);

    /* Make sure no active ISR is referencing the old data - hacky but should be
     * effective. */
    while (pDevExt->cISR > 0)
        ASMNopPause();

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
DECLINLINE(int) WaitEventCheckCondition(PVBOXGUESTDEVEXT pDevExt, VBoxGuestWaitEventInfo *pInfo,
                                        int iEvent, const uint32_t fReqEvents)
{
    uint32_t fMatches = pDevExt->f32PendingEvents & fReqEvents;
    if (fMatches)
    {
        ASMAtomicAndU32(&pDevExt->f32PendingEvents, ~fMatches);
        RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);

        pInfo->u32EventFlagsOut = fMatches;
        pInfo->u32Result = VBOXGUEST_WAITEVENT_OK;
        if (fReqEvents & ~((uint32_t)1 << iEvent))
            Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns %#x\n", pInfo->u32EventFlagsOut));
        else
            Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns %#x/%d\n", pInfo->u32EventFlagsOut, iEvent));
        return VINF_SUCCESS;
    }
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);
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
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: Invalid input mask %#x!!\n", fReqEvents));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Check the condition up front, before doing the wait-for-event allocations.
     */
    RTSpinlockAcquire(pDevExt->EventSpinlock);
    rc = WaitEventCheckCondition(pDevExt, pInfo, iEvent, fReqEvents);
    if (rc == VINF_SUCCESS)
        return rc;

    if (!pInfo->u32TimeoutIn)
    {
        pInfo->u32Result = VBOXGUEST_WAITEVENT_TIMEOUT;
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns VERR_TIMEOUT\n"));
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
    rc = WaitEventCheckCondition(pDevExt, pInfo, iEvent, fReqEvents);
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
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);

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
        rc = VERR_INTERRUPTED;
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns VERR_INTERRUPTED\n"));
    }
    else if (rc == VERR_TIMEOUT)
    {
        pInfo->u32Result = VBOXGUEST_WAITEVENT_TIMEOUT;
        Log(("VBoxGuestCommonIOCtl: WAITEVENT: returns VERR_TIMEOUT (2)\n"));
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
    PVBOXGUESTWAIT          pWait;
    PVBOXGUESTWAIT          pSafe;
    int                     rc = 0;

    Log(("VBoxGuestCommonIOCtl: CANCEL_ALL_WAITEVENTS\n"));

    /*
     * Walk the event list and wake up anyone with a matching session.
     */
    RTSpinlockAcquire(pDevExt->EventSpinlock);
    RTListForEachSafe(&pDevExt->WaitList, pWait, pSafe, VBOXGUESTWAIT, ListNode)
    {
        if (pWait->pSession == pSession)
        {
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
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);
    Assert(rc == 0);

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
static int VBoxGuestCheckIfVMMReqAllowed(PVBOXGUESTSESSION pSession, VMMDevRequestType enmType,
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
        case VMMDevReq_GetStatisticsChangeRequest:
        case VMMDevReq_ChangeMemBalloon:
            enmRequired = kLevel_TrustedUsers;
            break;

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
        case VMMDevReq_SetGuestCapabilities:
        case VMMDevReq_VideoModeSupported2:
        case VMMDevReq_VideoAccelEnable:
        case VMMDevReq_VideoAccelFlush:
        case VMMDevReq_VideoSetVisibleRegion:
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
    const uint32_t          cbMinSize = vmmdevGetRequestSize(enmType);

    Log(("VBoxGuestCommonIOCtl: VMMREQUEST type %d\n", pReqHdr->requestType));

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
    rc = VbglGRVerify(pReqHdr, cbData);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: invalid header: size %#x, expected >= %#x (hdr); type=%#x; rc=%Rrc!!\n",
             cbData, cbReq, enmType, rc));
        return rc;
    }

    rc = VBoxGuestCheckIfVMMReqAllowed(pSession, enmType, pReqHdr);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: Operation not allowed! type=%#x rc=%Rrc\n", enmType, rc));
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
        Log(("VBoxGuestCommonIOCtl: VMMREQUEST: failed to allocate %u (%#x) bytes to cache the request. rc=%Rrc!!\n",
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
        Log(("VBoxGuestCommonIOCtl: CTL_FILTER_MASK: failed to allocate %u (%#x) bytes to cache the request. rc=%Rrc!!\n",
             sizeof(*pReq), sizeof(*pReq), rc));
        return rc;
    }

    pReq->u32OrMask = pInfo->u32OrMask;
    pReq->u32NotMask = pInfo->u32NotMask;
    pReq->u32NotMask &= ~pDevExt->fFixedEvents; /* don't permit these to be cleared! */
    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
        Log(("VBoxGuestCommonIOCtl: CTL_FILTER_MASK: VbglGRPerform failed, rc=%Rrc!\n", rc));

    VbglGRFree(&pReq->header);
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
            RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);
            return VINF_SUCCESS;
        }
        RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);

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
        RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);
        return VINF_SUCCESS;
    }
    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);

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


static int VBoxGuestCommonIOCtl_HGCMConnect(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                                            VBoxGuestHGCMConnectInfo *pInfo, size_t *pcbDataReturned)
{
    int rc;

    /*
     * The VbglHGCMConnect call will invoke the callback if the HGCM
     * call is performed in an ASYNC fashion. The function is not able
     * to deal with cancelled requests.
     */
    Log(("VBoxGuestCommonIOCtl: HGCM_CONNECT: %.128s\n",
         pInfo->Loc.type == VMMDevHGCMLoc_LocalHost || pInfo->Loc.type == VMMDevHGCMLoc_LocalHost_Existing
         ? pInfo->Loc.u.host.achName : "<not local host>"));

    rc = VbglR0HGCMInternalConnect(pInfo, VBoxGuestHGCMAsyncWaitCallback, pDevExt, RT_INDEFINITE_WAIT);
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
            RTSpinlockAcquire(pDevExt->SessionSpinlock);
            for (i = 0; i < RT_ELEMENTS(pSession->aHGCMClientIds); i++)
                if (!pSession->aHGCMClientIds[i])
                {
                    pSession->aHGCMClientIds[i] = pInfo->u32ClientID;
                    break;
                }
            RTSpinlockReleaseNoInts(pDevExt->SessionSpinlock);
            if (i >= RT_ELEMENTS(pSession->aHGCMClientIds))
            {
                static unsigned             s_cErrors = 0;
                VBoxGuestHGCMDisconnectInfo Info;

                if (s_cErrors++ < 32)
                    LogRel(("VBoxGuestCommonIOCtl: HGCM_CONNECT: too many HGCMConnect calls for one session!\n"));

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
    RTSpinlockReleaseNoInts(pDevExt->SessionSpinlock);
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
    rc = VbglR0HGCMInternalDisconnect(pInfo, VBoxGuestHGCMAsyncWaitCallback, pDevExt, RT_INDEFINITE_WAIT);
    if (RT_SUCCESS(rc))
    {
        Log(("VBoxGuestCommonIOCtl: HGCM_DISCONNECT: result=%Rrc\n", pInfo->result));
        if (pcbDataReturned)
            *pcbDataReturned = sizeof(*pInfo);
    }

    /* Update the client id array according to the result. */
    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    if (pSession->aHGCMClientIds[i] == UINT32_MAX)
        pSession->aHGCMClientIds[i] = RT_SUCCESS(rc) && RT_SUCCESS(pInfo->result) ? 0 : u32ClientId;
    RTSpinlockReleaseNoInts(pDevExt->SessionSpinlock);

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
        LogRel(("VBoxGuestCommonIOCtl: HGCM_CALL: cParm=%RX32 is not sane\n", pInfo->cParms));
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
        LogRel(("VBoxGuestCommonIOCtl: HGCM_CALL: cbData=%#zx (%zu) required size is %#zx (%zu)\n",
             cbData, cbActual));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Validate the client id.
     */
    RTSpinlockAcquire(pDevExt->SessionSpinlock);
    for (i = 0; i < RT_ELEMENTS(pSession->aHGCMClientIds); i++)
        if (pSession->aHGCMClientIds[i] == u32ClientId)
            break;
    RTSpinlockReleaseNoInts(pDevExt->SessionSpinlock);
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
        Log(("VBoxGuestCommonIOCtl: HGCM_CALL: result=%Rrc\n", pInfo->result));
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
                LogRel(("VBoxGuestCommonIOCtl: HGCM_CALL: %s Failed. rc=%Rrc.\n", f32bit ? "32" : "64", rc));
        }
        else
            Log(("VBoxGuestCommonIOCtl: HGCM_CALL: %s Failed. rc=%Rrc.\n", f32bit ? "32" : "64", rc));
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
    VMMDevGetMemBalloonChangeRequest *pReq;
    int rc;

    Log(("VBoxGuestCommonIOCtl: CHECK_MEMORY_BALLOON\n"));
    rc = RTSemFastMutexRequest(pDevExt->MemBalloon.hMtx);
    AssertRCReturn(rc, rc);

    /*
     * The first user trying to query/change the balloon becomes the
     * owner and owns it until the session is closed (vboxGuestCloseMemBalloon).
     */
    if (   pDevExt->MemBalloon.pOwner != pSession
        && pDevExt->MemBalloon.pOwner == NULL)
        pDevExt->MemBalloon.pOwner = pSession;

    if (pDevExt->MemBalloon.pOwner == pSession)
    {
        rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(VMMDevGetMemBalloonChangeRequest), VMMDevReq_GetMemBalloonChangeRequest);
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
                LogRel(("VBoxGuestCommonIOCtl: CHECK_MEMORY_BALLOON: VbglGRPerform failed. rc=%Rrc\n", rc));
            VbglGRFree(&pReq->header);
        }
    }
    else
        rc = VERR_PERMISSION_DENIED;

    RTSemFastMutexRelease(pDevExt->MemBalloon.hMtx);
    Log(("VBoxGuestCommonIOCtl: CHECK_MEMORY_BALLOON returns %Rrc\n", rc));
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
            rc = vboxGuestSetBalloonSizeFromUser(pDevExt, pSession, pInfo->u64ChunkAddr, !!pInfo->fInflate);
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
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(*pReq), VMMDevReq_WriteCoreDump);
    if (RT_FAILURE(rc))
    {
        Log(("VBoxGuestCommonIOCtl: WRITE_CORE_DUMP: failed to allocate %u (%#x) bytes to cache the request. rc=%Rrc!!\n",
             sizeof(*pReq), sizeof(*pReq), rc));
        return rc;
    }

    pReq->fFlags = pInfo->fFlags;
    rc = VbglGRPerform(&pReq->header);
    if (RT_FAILURE(rc))
        Log(("VBoxGuestCommonIOCtl: WRITE_CORE_DUMP: VbglGRPerform failed, rc=%Rrc!\n", rc));

    VbglGRFree(&pReq->header);
    return rc;
}


#ifdef VBOX_WITH_VRDP_SESSION_HANDLING
/**
 * Enables the VRDP session and saves its session ID.
 *
 * @returns VBox status code.
 *
 * @param   pDevExt             The device extention.
 * @param   pSession            The session.
 */
static int VBoxGuestCommonIOCtl_EnableVRDPSession(VBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession)
{
    /* Nothing to do here right now, since this only is supported on Windows at the moment. */
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Disables the VRDP session.
 *
 * @returns VBox status code.
 *
 * @param   pDevExt             The device extention.
 * @param   pSession            The session.
 */
static int VBoxGuestCommonIOCtl_DisableVRDPSession(VBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession)
{
    /* Nothing to do here right now, since this only is supported on Windows at the moment. */
    return VERR_NOT_IMPLEMENTED;
}
#endif /* VBOX_WITH_VRDP_SESSION_HANDLING */

#ifdef DEBUG
/** Unit test SetMouseStatus instead of really executing the request. */
static bool     g_test_fSetMouseStatus = false;
/** When unit testing SetMouseStatus, the fake RC for the GR to return. */
static int      g_test_SetMouseStatusGRRC;
/** When unit testing SetMouseStatus this will be set to the status passed to
 * the GR. */
static uint32_t g_test_statusSetMouseStatus;
#endif

static int vboxguestcommonSetMouseStatus(uint32_t fFeatures)
{
    VMMDevReqMouseStatus *pReq;
    int rc;

    LogRelFlowFunc(("fFeatures=%u\n", (int) fFeatures));
    rc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof(*pReq), VMMDevReq_SetMouseStatus);
    if (RT_SUCCESS(rc))
    {
        pReq->mouseFeatures = fFeatures;
        pReq->pointerXPos = 0;
        pReq->pointerYPos = 0;
#ifdef DEBUG
        if (g_test_fSetMouseStatus)
        {
            g_test_statusSetMouseStatus = pReq->mouseFeatures;
            rc = g_test_SetMouseStatusGRRC;
        }
        else
#endif
            rc = VbglGRPerform(&pReq->header);
        VbglGRFree(&pReq->header);
    }
    LogRelFlowFunc(("rc=%Rrc\n", rc));
    return rc;
}


/**
 * Sets the mouse status features for this session and updates them
 * globally.  We aim to ensure that if several threads call this in
 * parallel the most recent status will always end up being set.
 *
 * @returns VBox status code.
 *
 * @param   pDevExt             The device extention.
 * @param   pSession            The session.
 * @param   fFeatures           New bitmap of enabled features.
 */
static int VBoxGuestCommonIOCtl_SetMouseStatus(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession, uint32_t fFeatures)
{
    uint32_t    fNewDevExtStatus = 0;
    unsigned    i;
    int         rc;
    /* Exit early if nothing has changed - hack to work around the
     * Windows Additions not using the common code. */
    bool        fNoAction;

    RTSpinlockAcquire(pDevExt->SessionSpinlock);

    for (i = 0; i < sizeof(fFeatures) * 8; i++)
    {
        if (RT_BIT_32(i) & VMMDEV_MOUSE_GUEST_MASK)
        {
            if (   (RT_BIT_32(i) & fFeatures)
                && !(RT_BIT_32(i) & pSession->fMouseStatus))
                pDevExt->acMouseFeatureUsage[i]++;
            else if (   !(RT_BIT_32(i) & fFeatures)
                     && (RT_BIT_32(i) & pSession->fMouseStatus))
                pDevExt->acMouseFeatureUsage[i]--;
        }
        if (pDevExt->acMouseFeatureUsage[i] > 0)
            fNewDevExtStatus |= RT_BIT_32(i);
    }

    pSession->fMouseStatus = fFeatures & VMMDEV_MOUSE_GUEST_MASK;
    fNoAction = (pDevExt->fMouseStatus == fNewDevExtStatus);
    pDevExt->fMouseStatus = fNewDevExtStatus;

    RTSpinlockReleaseNoInts(pDevExt->SessionSpinlock);
    if (fNoAction)
        return VINF_SUCCESS;

    do
    {
        fNewDevExtStatus = pDevExt->fMouseStatus;
        rc = vboxguestcommonSetMouseStatus(fNewDevExtStatus);
    } while (   RT_SUCCESS(rc)
             && fNewDevExtStatus != pDevExt->fMouseStatus);

    return rc;
}


#ifdef DEBUG
/** Unit test for the SET_MOUSE_STATUS IoCtl.  Since this is closely tied to
 * the code in question it probably makes most sense to keep it next to the
 * code. */
static void testSetMouseStatus(void)
{
    uint32_t u32Data;
    int rc;
    RTSPINLOCK Spinlock;

    g_test_fSetMouseStatus = true;
    rc = RTSpinlockCreate(&Spinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "VBoxGuestTest");
    AssertRCReturnVoid(rc);
    {
        VBOXGUESTDEVEXT  DevExt  = { 0 };
        VBOXGUESTSESSION Session = { 0 };

        g_test_statusSetMouseStatus = ~0;
        g_test_SetMouseStatusGRRC = VINF_SUCCESS;
        DevExt.SessionSpinlock = Spinlock;
        u32Data = VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE;
        rc = VBoxGuestCommonIOCtl(VBOXGUEST_IOCTL_SET_MOUSE_STATUS, &DevExt,
                                  &Session, &u32Data, sizeof(u32Data), NULL);
        AssertRCSuccess(rc);
        AssertMsg(   g_test_statusSetMouseStatus
                  == VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE,
                  ("Actual status: 0x%x\n", g_test_statusSetMouseStatus));
        DevExt.acMouseFeatureUsage[ASMBitFirstSetU32(VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR) - 1] = 1;
        rc = VBoxGuestCommonIOCtl(VBOXGUEST_IOCTL_SET_MOUSE_STATUS, &DevExt,
                                  &Session, &u32Data, sizeof(u32Data), NULL);
        AssertRCSuccess(rc);
        AssertMsg(   g_test_statusSetMouseStatus
                  == (  VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE
                      | VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR),
                  ("Actual status: 0x%x\n", g_test_statusSetMouseStatus));
        u32Data = VMMDEV_MOUSE_HOST_WANTS_ABSOLUTE;  /* Can't change this */
        rc = VBoxGuestCommonIOCtl(VBOXGUEST_IOCTL_SET_MOUSE_STATUS, &DevExt,
                                  &Session, &u32Data, sizeof(u32Data), NULL);
        AssertRCSuccess(rc);
        AssertMsg(   g_test_statusSetMouseStatus
                  == VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR,
                  ("Actual status: 0x%x\n", g_test_statusSetMouseStatus));
        u32Data = VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR;
        rc = VBoxGuestCommonIOCtl(VBOXGUEST_IOCTL_SET_MOUSE_STATUS, &DevExt,
                                  &Session, &u32Data, sizeof(u32Data), NULL);
        AssertRCSuccess(rc);
        AssertMsg(   g_test_statusSetMouseStatus
                  == VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR,
                  ("Actual status: 0x%x\n", g_test_statusSetMouseStatus));
        u32Data = 0;
        rc = VBoxGuestCommonIOCtl(VBOXGUEST_IOCTL_SET_MOUSE_STATUS, &DevExt,
                                  &Session, &u32Data, sizeof(u32Data), NULL);
        AssertRCSuccess(rc);
        AssertMsg(   g_test_statusSetMouseStatus
                  == VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR,
                  ("Actual status: 0x%x\n", g_test_statusSetMouseStatus));
        AssertMsg(DevExt.acMouseFeatureUsage[ASMBitFirstSetU32(VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR) - 1] == 1,
                  ("Actual value: %d\n", DevExt.acMouseFeatureUsage[ASMBitFirstSetU32(VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR)]));
        g_test_SetMouseStatusGRRC = VERR_UNRESOLVED_ERROR;
        /* This should succeed as the host request should not be made
         * since nothing has changed. */
        rc = VBoxGuestCommonIOCtl(VBOXGUEST_IOCTL_SET_MOUSE_STATUS, &DevExt,
                                  &Session, &u32Data, sizeof(u32Data), NULL);
        AssertRCSuccess(rc);
        /* This should fail with VERR_UNRESOLVED_ERROR as set above. */
        u32Data = VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE;
        rc = VBoxGuestCommonIOCtl(VBOXGUEST_IOCTL_SET_MOUSE_STATUS, &DevExt,
                                  &Session, &u32Data, sizeof(u32Data), NULL);
        AssertMsg(rc == VERR_UNRESOLVED_ERROR, ("rc == %Rrc\n", rc));
        /* Untested paths: out of memory; race setting status to host */
   }
    RTSpinlockDestroy(Spinlock);
    g_test_fSetMouseStatus = false;
}
#endif


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
static int VBoxGuestCommonIOCtl_Log(PVBOXGUESTDEVEXT pDevExt, const char *pch, size_t cbData, size_t *pcbDataReturned)
{
    NOREF(pch);
    NOREF(cbData);
    if (pDevExt->fLoggingEnabled)
        RTLogBackdoorPrintf("%.*s", cbData, pch);
    else
        Log(("%.*s", cbData, pch));
    if (pcbDataReturned)
        *pcbDataReturned = 0;
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
        rc = VBoxGuestCommonIOCtl_Log(pDevExt, (char *)pvData, cbData, pcbDataReturned);
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

#ifdef VBOX_WITH_VRDP_SESSION_HANDLING
            case VBOXGUEST_IOCTL_ENABLE_VRDP_SESSION:
                rc = VBoxGuestCommonIOCtl_EnableVRDPSession(pDevExt, pSession);
                break;

            case VBOXGUEST_IOCTL_DISABLE_VRDP_SESSION:
                rc = VBoxGuestCommonIOCtl_DisableVRDPSession(pDevExt, pSession);
                break;
#endif /* VBOX_WITH_VRDP_SESSION_HANDLING */
            case VBOXGUEST_IOCTL_SET_MOUSE_STATUS:
                CHECKRET_SIZE("SET_MOUSE_STATUS", sizeof(uint32_t));
                rc = VBoxGuestCommonIOCtl_SetMouseStatus(pDevExt, pSession,
                                                         *(uint32_t *)pvData);
                break;

            default:
            {
                LogRel(("VBoxGuestCommonIOCtl: Unknown request iFunction=%#x Stripped size=%#x\n", iFunction,
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
#ifndef RT_OS_WINDOWS
    VBoxGuestMouseSetNotifyCallback MouseNotifyCallback   = { NULL, NULL };
#endif
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
     * Enter the spinlock, increase the ISR count and check if it's our IRQ or
     * not.
     */
    RTSpinlockAcquire(pDevExt->EventSpinlock);
    ASMAtomicIncU32(&pDevExt->cISR);
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

            Log(("VBoxGuestCommonISR: acknowledge events succeeded %#RX32\n", fEvents));

            /*
             * VMMDEV_EVENT_MOUSE_POSITION_CHANGED can only be polled for.
             */
            if (fEvents & VMMDEV_EVENT_MOUSE_POSITION_CHANGED)
            {
                MouseNotifyCallback = pDevExt->MouseNotifyCallback;
                fMousePositionChanged = true;
                fEvents &= ~VMMDEV_EVENT_MOUSE_POSITION_CHANGED;
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
                if (    (pWait->fReqEvents & fEvents)
                    &&  !pWait->fResEvents)
                {
                    pWait->fResEvents = pWait->fReqEvents & fEvents;
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
            Log(("VBoxGuestCommonISR: acknowledge events failed rc=%Rrc (events=%#x)!!\n",
                 pReq->header.rc, pReq->events));
    }
    else
        LogFlow(("VBoxGuestCommonISR: not ours\n"));

    RTSpinlockReleaseNoInts(pDevExt->EventSpinlock);

#if defined(VBOXGUEST_USE_DEFERRED_WAKE_UP) && !defined(RT_OS_WINDOWS)
    /*
     * Do wake-ups.
     * Note. On Windows this isn't possible at this IRQL, so a DPC will take
     *       care of it.
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
#ifndef RT_OS_WINDOWS
        if (MouseNotifyCallback.pfnNotify)
            MouseNotifyCallback.pfnNotify(MouseNotifyCallback.pvUser);
#endif
    }

    ASMAtomicDecU32(&pDevExt->cISR);
    Assert(rc == 0);
    return fOurIrq;
}

