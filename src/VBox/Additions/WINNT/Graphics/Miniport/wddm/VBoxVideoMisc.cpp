/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "../VBoxVideo.h"

#include <iprt/asm.h>

NTSTATUS vboxWddmHTableCreate(PVBOXWDDM_HTABLE pTbl, uint32_t cSize)
{
    memset(pTbl, 0, sizeof (*pTbl));
    pTbl->paData = (PVOID*)vboxWddmMemAllocZero(sizeof (pTbl->paData[0]) * cSize);
    if (pTbl->paData)
    {
        pTbl->cSize = cSize;
        return STATUS_SUCCESS;
    }
    return STATUS_NO_MEMORY;
}

VOID vboxWddmHTableDestroy(PVBOXWDDM_HTABLE pTbl)
{
    vboxWddmMemFree(pTbl->paData);
}

DECLINLINE(VBOXWDDM_HANDLE) vboxWddmHTableIndex2Handle(uint32_t iIndex)
{
    return iIndex+1;
}

DECLINLINE(uint32_t) vboxWddmHTableHandle2Index(VBOXWDDM_HANDLE hHandle)
{
    return hHandle-1;
}

NTSTATUS vboxWddmHTableRealloc(PVBOXWDDM_HTABLE pTbl, uint32_t cNewSize)
{
    Assert(cNewSize > pTbl->cSize);
    if (cNewSize > pTbl->cSize)
    {
        PVOID *pvNewData = (PVOID*)vboxWddmMemAllocZero(sizeof (pTbl->paData[0]) * cNewSize);
        memcpy(pvNewData, pTbl->paData, sizeof (pTbl->paData[0]) * pTbl->cSize);
        pTbl->iNext2Search = pTbl->cSize;
        pTbl->cSize = cNewSize;
        pTbl->paData = pvNewData;
        return STATUS_SUCCESS;
    }
    else if (cNewSize >= pTbl->cData)
        return STATUS_NOT_IMPLEMENTED;
    return STATUS_INVALID_PARAMETER;

}
VBOXWDDM_HANDLE vboxWddmHTablePut(PVBOXWDDM_HTABLE pTbl, PVOID pvData)
{
    if (pTbl->cSize == pTbl->cData)
    {
        Assert(0);
        NTSTATUS Status = vboxWddmHTableRealloc(pTbl, pTbl->cSize + RT_MAX(10, pTbl->cSize/4));
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
            return VBOXWDDM_HANDLE_INVALID;
    }
    for (UINT i = pTbl->iNext2Search; ; ++i, i %= pTbl->cSize)
    {
        Assert(i < pTbl->cSize);
        if (!pTbl->paData[i])
        {
            pTbl->paData[i] = pvData;
            ++pTbl->cData;
            Assert(pTbl->cData <= pTbl->cSize);
            ++pTbl->iNext2Search;
            pTbl->iNext2Search %= pTbl->cSize;
            return vboxWddmHTableIndex2Handle(i);
        }
    }
    Assert(0);
    return VBOXWDDM_HANDLE_INVALID;
}

PVOID vboxWddmHTableRemove(PVBOXWDDM_HTABLE pTbl, VBOXWDDM_HANDLE hHandle)
{
    uint32_t iIndex = vboxWddmHTableHandle2Index(hHandle);
    Assert(iIndex < pTbl->cSize);
    if (iIndex < pTbl->cSize)
    {
        PVOID pvData = pTbl->paData[iIndex];
        pTbl->paData[iIndex] = NULL;
        --pTbl->cData;
        Assert(pTbl->cData <= pTbl->cSize);
        pTbl->iNext2Search = iIndex;
        return pvData;
    }
    return NULL;
}

PVOID vboxWddmHTableGet(PVBOXWDDM_HTABLE pTbl, VBOXWDDM_HANDLE hHandle)
{
    uint32_t iIndex = vboxWddmHTableHandle2Index(hHandle);
    Assert(iIndex < pTbl->cSize);
    if (iIndex < pTbl->cSize)
        return pTbl->paData[iIndex];
    return NULL;
}

VOID vboxWddmHTableIterInit(PVBOXWDDM_HTABLE pTbl, PVBOXWDDM_HTABLE_ITERATOR pIter)
{
    pIter->pTbl = pTbl;
    pIter->iCur = ~0UL;
    pIter->cLeft = pTbl->cData;
}

BOOL vboxWddmHTableIterHasNext(PVBOXWDDM_HTABLE_ITERATOR pIter)
{
    return pIter->cLeft;
}


PVOID vboxWddmHTableIterNext(PVBOXWDDM_HTABLE_ITERATOR pIter, VBOXWDDM_HANDLE *phHandle)
{
    if (vboxWddmHTableIterHasNext(pIter))
    {
        for (uint32_t i = pIter->iCur+1; i < pIter->pTbl->cSize ; ++i)
        {
            if (pIter->pTbl->paData[i])
            {
                pIter->iCur = i;
                --pIter->cLeft;
                VBOXWDDM_HANDLE hHandle = vboxWddmHTableIndex2Handle(i);
                Assert(hHandle);
                if (phHandle)
                    *phHandle = hHandle;
                return pIter->pTbl->paData[i];
            }
        }
    }

    Assert(!vboxWddmHTableIterHasNext(pIter));
    if (phHandle)
        *phHandle = VBOXWDDM_HANDLE_INVALID;
    return NULL;
}


PVOID vboxWddmHTableIterRemoveCur(PVBOXWDDM_HTABLE_ITERATOR pIter)
{
    VBOXWDDM_HANDLE hHandle = vboxWddmHTableIndex2Handle(pIter->iCur);
    Assert(hHandle);
    if (hHandle)
    {
        PVOID pRet = vboxWddmHTableRemove(pIter->pTbl, hHandle);
        Assert(pRet);
        return pRet;
    }
    return NULL;
}

PVBOXWDDM_SWAPCHAIN vboxWddmSwapchainCreate()
{
    PVBOXWDDM_SWAPCHAIN pSwapchain = (PVBOXWDDM_SWAPCHAIN)vboxWddmMemAllocZero(sizeof (VBOXWDDM_SWAPCHAIN));
    Assert(pSwapchain);
    if (pSwapchain)
    {
        InitializeListHead(&pSwapchain->AllocList);
        pSwapchain->enmState = VBOXWDDM_OBJSTATE_TYPE_INITIALIZED;
        pSwapchain->cRefs = 1;
    }
    return pSwapchain;
}

DECLINLINE(BOOLEAN) vboxWddmSwapchainRetainLocked(PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    if (pSwapchain->enmState == VBOXWDDM_OBJSTATE_TYPE_INITIALIZED)
    {
        ASMAtomicIncU32(&pSwapchain->cRefs);
        return TRUE;
    }
    return FALSE;
}

DECLINLINE(BOOLEAN) vboxWddmSwapchainRetain(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    KIRQL OldIrql;
    BOOLEAN bRc;
    KeAcquireSpinLock(&pDevExt->SynchLock, &OldIrql);
    bRc = vboxWddmSwapchainRetainLocked(pSwapchain);
    KeReleaseSpinLock(&pDevExt->SynchLock, OldIrql);
    return bRc;
}

DECLINLINE(VOID) vboxWddmSwapchainRelease(PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    uint32_t cRefs = ASMAtomicDecU32(&pSwapchain->cRefs);
    Assert(cRefs < UINT32_MAX/2);
    if (!cRefs)
        vboxWddmMemFree(pSwapchain);
}

PVBOXWDDM_SWAPCHAIN vboxWddmSwapchainRetainByAlloc(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAlloc)
{
    KIRQL OldIrql;
    PVBOXWDDM_SWAPCHAIN pSwapchain;
    KeAcquireSpinLock(&pDevExt->SynchLock, &OldIrql);
    pSwapchain = pAlloc->pSwapchain;
    if (pSwapchain && !vboxWddmSwapchainRetainLocked(pSwapchain))
        pSwapchain = NULL;
    KeReleaseSpinLock(&pDevExt->SynchLock, OldIrql);
    return pSwapchain;
}

VOID vboxWddmSwapchainAllocRemove(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain, PVBOXWDDM_ALLOCATION pAlloc)
{
    KIRQL OldIrql;
    KeAcquireSpinLock(&pDevExt->SynchLock, &OldIrql);
    Assert(pAlloc->pSwapchain == pSwapchain);
    pAlloc->pSwapchain = NULL;
    RemoveEntryList(&pAlloc->SwapchainEntry);
    KeReleaseSpinLock(&pDevExt->SynchLock, OldIrql);
    vboxWddmSwapchainRelease(pSwapchain);
}

BOOLEAN vboxWddmSwapchainAllocAdd(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain, PVBOXWDDM_ALLOCATION pAlloc)
{
    KIRQL OldIrql;
    BOOLEAN bRc;
    Assert(!pAlloc->pSwapchain);
    KeAcquireSpinLock(&pDevExt->SynchLock, &OldIrql);
    bRc = vboxWddmSwapchainRetainLocked(pSwapchain);
    if (bRc)
    {
        if (pAlloc->pSwapchain)
        {
            RemoveEntryList(&pAlloc->SwapchainEntry);
        }
        InsertTailList(&pSwapchain->AllocList, &pAlloc->SwapchainEntry);
        pAlloc->pSwapchain = pSwapchain;
    }
    KeReleaseSpinLock(&pDevExt->SynchLock, OldIrql);
    return bRc;
}

#define VBOXSCENTRY_2_ALLOC(_pE) ((PVBOXWDDM_ALLOCATION)((uint8_t*)(_pE) - RT_OFFSETOF(VBOXWDDM_ALLOCATION, SwapchainEntry)))

static VOID vboxWddmSwapchainAllocRemoveAllInternal(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain, BOOLEAN bOnDestroy)
{
    KIRQL OldIrql;
    UINT cRemoved = 0;
    KeAcquireSpinLock(&pDevExt->SynchLock, &OldIrql);
    PLIST_ENTRY pEntry = pSwapchain->AllocList.Flink;
    do
    {
        if (pEntry != &pSwapchain->AllocList)
        {
            PVBOXWDDM_ALLOCATION pAlloc = VBOXSCENTRY_2_ALLOC(pEntry);
            pEntry = pEntry->Flink;
            Assert(pAlloc->pSwapchain == pSwapchain);
            pAlloc->pSwapchain = NULL;
            RemoveEntryList(&pAlloc->SwapchainEntry);
            ++cRemoved;
        }
        else
            break;
    } while (1);

    if (bOnDestroy)
        pSwapchain->enmState = VBOXWDDM_OBJSTATE_TYPE_TERMINATED;
    KeReleaseSpinLock(&pDevExt->SynchLock, OldIrql);

    for (UINT i = 0; i < cRemoved; ++i)
        vboxWddmSwapchainRelease(pSwapchain);
}

VOID vboxWddmSwapchainAllocRemoveAll(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    vboxWddmSwapchainAllocRemoveAllInternal(pDevExt, pSwapchain, FALSE);
}

VOID vboxWddmSwapchainDestroy(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    vboxWddmSwapchainAllocRemoveAllInternal(pDevExt, pSwapchain, TRUE);
    vboxWddmSwapchainRelease(pSwapchain);
}

static BOOLEAN vboxWddmSwapchainCtxAddLocked(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    if (vboxWddmSwapchainRetain(pDevExt, pSwapchain))
    {
        Assert(!pSwapchain->hSwapchainKm);
        Assert(!pSwapchain->pContext);
        pSwapchain->pContext = pContext;
        pSwapchain->hSwapchainKm = vboxWddmHTablePut(&pContext->Swapchains, pSwapchain);
        InsertHeadList(&pDevExt->SwapchainList3D, &pSwapchain->DevExtListEntry);
        return TRUE;
    }
    return FALSE;
}

static VOID vboxWddmSwapchainCtxRemoveLocked(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    Assert(pSwapchain->hSwapchainKm);
    Assert(pSwapchain->pContext);
    void * pTst = vboxWddmHTableRemove(&pContext->Swapchains, pSwapchain->hSwapchainKm);
    Assert(pTst == pSwapchain);
    RemoveEntryList(&pSwapchain->DevExtListEntry);
    pSwapchain->pContext = NULL;
    pSwapchain->hSwapchainKm = NULL;
    if (pSwapchain->pLastReportedRects)
    {
        vboxVideoCmCmdRelease(pSwapchain->pLastReportedRects);
        pSwapchain->pLastReportedRects = NULL;
    }
    vboxWddmSwapchainRelease(pSwapchain);
}

BOOLEAN vboxWddmSwapchainCtxAdd(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    BOOLEAN bRc;
    ExAcquireFastMutex(&pDevExt->ContextMutex);
    bRc = vboxWddmSwapchainCtxAddLocked(pDevExt, pContext, pSwapchain);
    ExReleaseFastMutex(&pDevExt->ContextMutex);
    return bRc;
}

VOID vboxWddmSwapchainCtxRemove(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    ExAcquireFastMutex(&pDevExt->ContextMutex);
    vboxWddmSwapchainCtxRemoveLocked(pDevExt, pContext, pSwapchain);
    ExReleaseFastMutex(&pDevExt->ContextMutex);
}

VOID vboxWddmSwapchainCtxDestroyAll(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext)
{
    VBOXWDDM_HTABLE_ITERATOR Iter;
    ExAcquireFastMutex(&pDevExt->ContextMutex);
    vboxWddmHTableIterInit(&pContext->Swapchains, &Iter);
    do
    {
        PVBOXWDDM_SWAPCHAIN pSwapchain = (PVBOXWDDM_SWAPCHAIN)vboxWddmHTableIterNext(&Iter, NULL);
        if (!pSwapchain)
            break;

        /* yes, we can call remove locked even when using iterator */
        vboxWddmSwapchainCtxRemoveLocked(pDevExt, pContext, pSwapchain);
//        vboxWddmHTableIterRemoveCur(&Iter);

        vboxWddmSwapchainDestroy(pDevExt, pSwapchain);
    } while (1);
    ExReleaseFastMutex(&pDevExt->ContextMutex);
}

NTSTATUS vboxWddmSwapchainCtxEscape(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXDISPIFESCAPE_SWAPCHAININFO pSwapchainInfo, UINT cbSize)
{
    Assert((cbSize >= RT_OFFSETOF(VBOXDISPIFESCAPE_SWAPCHAININFO, SwapchainInfo.ahAllocs[0])));
    if (cbSize < RT_OFFSETOF(VBOXDISPIFESCAPE_SWAPCHAININFO, SwapchainInfo.ahAllocs[0]))
        return STATUS_INVALID_PARAMETER;
    Assert(cbSize >= RT_OFFSETOF(VBOXDISPIFESCAPE_SWAPCHAININFO, SwapchainInfo.ahAllocs[pSwapchainInfo->SwapchainInfo.cAllocs]));
    if (cbSize < RT_OFFSETOF(VBOXDISPIFESCAPE_SWAPCHAININFO, SwapchainInfo.ahAllocs[pSwapchainInfo->SwapchainInfo.cAllocs]))
        return STATUS_INVALID_PARAMETER;

    PVBOXWDDM_SWAPCHAIN pSwapchain;
    PVBOXWDDM_ALLOCATION *apAlloc = NULL;
    if (pSwapchainInfo->SwapchainInfo.cAllocs)
    {
        apAlloc = (PVBOXWDDM_ALLOCATION *)vboxWddmMemAlloc(sizeof (PVBOXWDDM_ALLOCATION) * pSwapchainInfo->SwapchainInfo.cAllocs);
        Assert(apAlloc);
        if (!apAlloc)
            return STATUS_NO_MEMORY;
        for (UINT i = 0; i < pSwapchainInfo->SwapchainInfo.cAllocs; ++i)
        {
            DXGKARGCB_GETHANDLEDATA GhData;
            GhData.hObject = pSwapchainInfo->SwapchainInfo.ahAllocs[i];
            GhData.Type = DXGK_HANDLE_ALLOCATION;
            GhData.Flags.Value = 0;
            PVBOXWDDM_ALLOCATION pAlloc = (PVBOXWDDM_ALLOCATION)pDevExt->u.primary.DxgkInterface.DxgkCbGetHandleData(&GhData);
            Assert(pAlloc);
            if (!pAlloc)
                return STATUS_INVALID_PARAMETER;
            apAlloc[i] = pAlloc;
        }
    }

    if (pSwapchainInfo->SwapchainInfo.hSwapchainKm)
    {
//            ExAcquireFastMutex(&pContext->SwapchainMutex);
        ExAcquireFastMutex(&pDevExt->ContextMutex);
        pSwapchain = (PVBOXWDDM_SWAPCHAIN)vboxWddmHTableGet(&pContext->Swapchains, (VBOXWDDM_HANDLE)pSwapchainInfo->SwapchainInfo.hSwapchainKm);
        Assert(pSwapchain);
        if (!pSwapchain)
        {
            ExReleaseFastMutex(&pDevExt->ContextMutex);
            return STATUS_INVALID_PARAMETER;
        }
        Assert(pSwapchain->hSwapchainKm == pSwapchainInfo->SwapchainInfo.hSwapchainKm);
        Assert(pSwapchain->pContext == pContext);
        if (pSwapchain->pContext != pContext)
        {
            ExReleaseFastMutex(&pDevExt->ContextMutex);
            return STATUS_INVALID_PARAMETER;
        }
    }
    else if (pSwapchainInfo->SwapchainInfo.cAllocs)
    {
        pSwapchain = vboxWddmSwapchainCreate();
        if (!pSwapchain)
            return STATUS_NO_MEMORY;

        ExAcquireFastMutex(&pDevExt->ContextMutex);
        BOOLEAN bRc = vboxWddmSwapchainCtxAddLocked(pDevExt, pContext, pSwapchain);
        Assert(bRc);
    }
    else
        return STATUS_INVALID_PARAMETER;

    memset(&pSwapchain->ViewRect, 0, sizeof (pSwapchain->ViewRect));
    if (pSwapchain->pLastReportedRects)
    {
        vboxVideoCmCmdRelease(pSwapchain->pLastReportedRects);
        pSwapchain->pLastReportedRects = NULL;
    }

    vboxWddmSwapchainAllocRemoveAll(pDevExt, pSwapchain);

    if (pSwapchainInfo->SwapchainInfo.cAllocs)
    {
        for (UINT i = 0; i < pSwapchainInfo->SwapchainInfo.cAllocs; ++i)
        {
            vboxWddmSwapchainAllocAdd(pDevExt, pSwapchain, apAlloc[i]);
        }
        pSwapchain->hSwapchainUm = pSwapchainInfo->SwapchainInfo.hSwapchainUm;
    }
    else
    {
        vboxWddmSwapchainCtxRemoveLocked(pDevExt, pContext, pSwapchain);
    }

//    ExReleaseFastMutex(&pContext->SwapchainMutex);
    ExReleaseFastMutex(&pDevExt->ContextMutex);

    if (pSwapchainInfo->SwapchainInfo.cAllocs)
    {
        Assert(pSwapchain->pContext);
        Assert(pSwapchain->hSwapchainKm);
        pSwapchainInfo->SwapchainInfo.hSwapchainKm = pSwapchain->hSwapchainKm;
    }
    else
    {
        vboxWddmSwapchainDestroy(pDevExt, pSwapchain);
        pSwapchainInfo->SwapchainInfo.hSwapchainKm = 0;
    }

    return STATUS_SUCCESS;
}
