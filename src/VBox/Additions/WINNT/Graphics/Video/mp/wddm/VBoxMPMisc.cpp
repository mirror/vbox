/* $Id$ */

/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxMPWddm.h"
#include <VBox/Hardware/VBoxVideoVBE.h>
#include <stdio.h>

/* simple handle -> value table API */
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
    if (!pTbl->paData)
        return;

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
        if (!pvNewData)
        {
            WARN(("vboxWddmMemAllocZero failed for size (%d)", sizeof (pTbl->paData[0]) * cNewSize));
            return STATUS_NO_MEMORY;
        }
        memcpy(pvNewData, pTbl->paData, sizeof (pTbl->paData[0]) * pTbl->cSize);
        vboxWddmMemFree(pTbl->paData);
        pTbl->iNext2Search = pTbl->cSize;
        pTbl->cSize = cNewSize;
        pTbl->paData = pvNewData;
        return STATUS_SUCCESS;
    }
    else if (cNewSize >= pTbl->cData)
    {
        AssertFailed();
        return STATUS_NOT_IMPLEMENTED;
    }
    return STATUS_INVALID_PARAMETER;

}
VBOXWDDM_HANDLE vboxWddmHTablePut(PVBOXWDDM_HTABLE pTbl, PVOID pvData)
{
    if (pTbl->cSize == pTbl->cData)
    {
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

DECLINLINE(BOOLEAN) vboxWddmSwapchainRetain(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain)
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
    const uint32_t cRefs = ASMAtomicDecU32(&pSwapchain->cRefs);
    Assert(cRefs < UINT32_MAX/2);
    if (!cRefs)
    {
        vboxWddmMemFree(pSwapchain);
    }
}

PVBOXWDDM_SWAPCHAIN vboxWddmSwapchainRetainByAlloc(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAlloc)
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

VOID vboxWddmSwapchainAllocRemove(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain, PVBOXWDDM_ALLOCATION pAlloc)
{
    KIRQL OldIrql;
    KeAcquireSpinLock(&pDevExt->SynchLock, &OldIrql);
    Assert(pAlloc->pSwapchain == pSwapchain);
    pAlloc->pSwapchain = NULL;
    RemoveEntryList(&pAlloc->SwapchainEntry);
    KeReleaseSpinLock(&pDevExt->SynchLock, OldIrql);
    vboxWddmSwapchainRelease(pSwapchain);
}

BOOLEAN vboxWddmSwapchainAllocAdd(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain, PVBOXWDDM_ALLOCATION pAlloc)
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

static VOID vboxWddmSwapchainAllocRemoveAllInternal(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain, BOOLEAN bOnDestroy)
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

VOID vboxWddmSwapchainAllocRemoveAll(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    vboxWddmSwapchainAllocRemoveAllInternal(pDevExt, pSwapchain, FALSE);
}

VOID vboxWddmSwapchainDestroy(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    vboxWddmSwapchainAllocRemoveAllInternal(pDevExt, pSwapchain, TRUE);

    Assert(pSwapchain->pContext);
    if (pSwapchain->pContext)
    {
        NTSTATUS tmpStatus = vboxVdmaGgCmdCancel(pDevExt, pSwapchain->pContext, pSwapchain);
        if (tmpStatus != STATUS_SUCCESS)
        {
            WARN(("vboxVdmaGgCmdCancel returned Status (0x%x)", tmpStatus));
        }
    }

    vboxWddmSwapchainRelease(pSwapchain);
}

static BOOLEAN vboxWddmSwapchainCtxAddLocked(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
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

static VOID vboxWddmSwapchainCtxRemoveLocked(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    Assert(pSwapchain->hSwapchainKm);
    Assert(pSwapchain->pContext);
    void * pTst = vboxWddmHTableRemove(&pContext->Swapchains, pSwapchain->hSwapchainKm);
    Assert(pTst == pSwapchain);
    RemoveEntryList(&pSwapchain->DevExtListEntry);
    pSwapchain->hSwapchainKm = NULL;
    if (pSwapchain->pLastReportedRects)
    {
        vboxVideoCmCmdRelease(pSwapchain->pLastReportedRects);
        pSwapchain->pLastReportedRects = NULL;
    }
    vboxWddmSwapchainRelease(pSwapchain);
}

/* adds the given swapchain to the context's swapchain list
 * @return true on success */
BOOLEAN vboxWddmSwapchainCtxAdd(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    BOOLEAN bRc;
    ExAcquireFastMutex(&pDevExt->ContextMutex);
    bRc = vboxWddmSwapchainCtxAddLocked(pDevExt, pContext, pSwapchain);
    ExReleaseFastMutex(&pDevExt->ContextMutex);
    return bRc;
}

/* removes the given swapchain from the context's swapchain list
 * */
VOID vboxWddmSwapchainCtxRemove(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    ExAcquireFastMutex(&pDevExt->ContextMutex);
    vboxWddmSwapchainCtxRemoveLocked(pDevExt, pContext, pSwapchain);
    ExReleaseFastMutex(&pDevExt->ContextMutex);
}

/* destroys all swapchains for the given context
 * */
VOID vboxWddmSwapchainCtxDestroyAll(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext)
{
    VBOXWDDM_HTABLE_ITERATOR Iter;
    do
    {
        ExAcquireFastMutex(&pDevExt->ContextMutex);
        vboxWddmHTableIterInit(&pContext->Swapchains, &Iter);
        PVBOXWDDM_SWAPCHAIN pSwapchain = (PVBOXWDDM_SWAPCHAIN)vboxWddmHTableIterNext(&Iter, NULL);
        if (!pSwapchain)
            break;

        /* yes, we can call remove locked even when using iterator */
        vboxWddmSwapchainCtxRemoveLocked(pDevExt, pContext, pSwapchain);

        ExReleaseFastMutex(&pDevExt->ContextMutex);
        /* we must not do vboxWddmSwapchainDestroy inside a context mutex */
        vboxWddmSwapchainDestroy(pDevExt, pSwapchain);
        /* start from the very beginning, we will quit the loop when no swapchains left */
    } while (1);

    /* no swapchains left, we exiteed the while loop via the "break", and we still owning the mutex */
    ExReleaseFastMutex(&pDevExt->ContextMutex);
}

/* process the swapchain info passed from user-mode display driver & synchronizes the driver state with it */
NTSTATUS vboxWddmSwapchainCtxEscape(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXDISPIFESCAPE_SWAPCHAININFO pSwapchainInfo, UINT cbSize)
{
    Assert((cbSize >= RT_OFFSETOF(VBOXDISPIFESCAPE_SWAPCHAININFO, SwapchainInfo.ahAllocs[0])));
    if (cbSize < RT_OFFSETOF(VBOXDISPIFESCAPE_SWAPCHAININFO, SwapchainInfo.ahAllocs[0]))
        return STATUS_INVALID_PARAMETER;
    Assert(cbSize >= RT_OFFSETOF(VBOXDISPIFESCAPE_SWAPCHAININFO, SwapchainInfo.ahAllocs[pSwapchainInfo->SwapchainInfo.cAllocs]));
    if (cbSize < RT_OFFSETOF(VBOXDISPIFESCAPE_SWAPCHAININFO, SwapchainInfo.ahAllocs[pSwapchainInfo->SwapchainInfo.cAllocs]))
        return STATUS_INVALID_PARAMETER;

    PVBOXWDDM_SWAPCHAIN pSwapchain = NULL;
    PVBOXWDDM_ALLOCATION *apAlloc = NULL;
    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    NTSTATUS Status = STATUS_SUCCESS;

    do {
        if (pSwapchainInfo->SwapchainInfo.cAllocs)
        {
            /* ensure we do not overflow the 32bit buffer size value */
            if (VBOXWDDM_ARRAY_MAXELEMENTSU32(VBOXWDDM_ALLOCATION) < pSwapchainInfo->SwapchainInfo.cAllocs)
            {
                WARN(("number of allocations passed in too big (%d), max is (%d)", pSwapchainInfo->SwapchainInfo.cAllocs, VBOXWDDM_ARRAY_MAXELEMENTSU32(VBOXWDDM_ALLOCATION)));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            apAlloc = (PVBOXWDDM_ALLOCATION *)vboxWddmMemAlloc(sizeof (PVBOXWDDM_ALLOCATION) * pSwapchainInfo->SwapchainInfo.cAllocs);
            Assert(apAlloc);
            if (!apAlloc)
            {
                Status = STATUS_NO_MEMORY;
                break;
            }
            for (UINT i = 0; i < pSwapchainInfo->SwapchainInfo.cAllocs; ++i)
            {
                DXGKARGCB_GETHANDLEDATA GhData;
                GhData.hObject = pSwapchainInfo->SwapchainInfo.ahAllocs[i];
                GhData.Type = DXGK_HANDLE_ALLOCATION;
                GhData.Flags.Value = 0;
                PVBOXWDDM_ALLOCATION pAlloc = (PVBOXWDDM_ALLOCATION)pDevExt->u.primary.DxgkInterface.DxgkCbGetHandleData(&GhData);
                Assert(pAlloc);
                if (!pAlloc)
                {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }
                apAlloc[i] = pAlloc;
            }

            if (!NT_SUCCESS(Status))
                break;
        }

        if (pSwapchainInfo->SwapchainInfo.hSwapchainKm)
        {
            ExAcquireFastMutex(&pDevExt->ContextMutex);
            pSwapchain = (PVBOXWDDM_SWAPCHAIN)vboxWddmHTableGet(&pContext->Swapchains, (VBOXWDDM_HANDLE)pSwapchainInfo->SwapchainInfo.hSwapchainKm);
            Assert(pSwapchain);
            if (!pSwapchain)
            {
                ExReleaseFastMutex(&pDevExt->ContextMutex);
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
            Assert(pSwapchain->hSwapchainKm == pSwapchainInfo->SwapchainInfo.hSwapchainKm);
            Assert(pSwapchain->pContext == pContext);
            if (pSwapchain->pContext != pContext)
            {
                ExReleaseFastMutex(&pDevExt->ContextMutex);
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
        }
        else if (pSwapchainInfo->SwapchainInfo.cAllocs)
        {
            pSwapchain = vboxWddmSwapchainCreate();
            if (!pSwapchain)
            {
                Status = STATUS_NO_MEMORY;
                break;
            }

            ExAcquireFastMutex(&pDevExt->ContextMutex);
            BOOLEAN bRc = vboxWddmSwapchainCtxAddLocked(pDevExt, pContext, pSwapchain);
            Assert(bRc);
        }
        else
        {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        /* do not zero up the view rect since it may still be valid */
//        memset(&pSwapchain->ViewRect, 0, sizeof (pSwapchain->ViewRect));
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

        Assert(Status == STATUS_SUCCESS);
    } while (0);

    /* cleanup */
    if (apAlloc)
        vboxWddmMemFree(apAlloc);

    return Status;
}

NTSTATUS vboxWddmSwapchainCtxInit(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext)
{
    NTSTATUS Status = vboxWddmHTableCreate(&pContext->Swapchains, 4);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vboxWddmHTableCreate failes, Status (x%x)", Status));
        return Status;
    }

    return STATUS_SUCCESS;
}

VOID vboxWddmSwapchainCtxTerm(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext)
{
    vboxWddmSwapchainCtxDestroyAll(pDevExt, pContext);
    vboxWddmHTableDestroy(&pContext->Swapchains);
}

#define VBOXWDDM_REG_DRVKEY_PREFIX L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Class\\"

NTSTATUS vboxWddmRegQueryDrvKeyName(PVBOXMP_DEVEXT pDevExt, ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult)
{
    WCHAR fallBackBuf[2];
    PWCHAR pSuffix;
    bool bFallback = false;

    if (cbBuf > sizeof(VBOXWDDM_REG_DRVKEY_PREFIX))
    {
        memcpy(pBuf, VBOXWDDM_REG_DRVKEY_PREFIX, sizeof (VBOXWDDM_REG_DRVKEY_PREFIX));
        pSuffix = pBuf + (sizeof (VBOXWDDM_REG_DRVKEY_PREFIX)-2)/2;
        cbBuf -= sizeof (VBOXWDDM_REG_DRVKEY_PREFIX)-2;
    }
    else
    {
        pSuffix = fallBackBuf;
        cbBuf = sizeof (fallBackBuf);
        bFallback = true;
    }

    NTSTATUS Status = IoGetDeviceProperty (pDevExt->pPDO,
                                  DevicePropertyDriverKeyName,
                                  cbBuf,
                                  pSuffix,
                                  &cbBuf);
    if (Status == STATUS_SUCCESS && bFallback)
        Status = STATUS_BUFFER_TOO_SMALL;
    if (Status == STATUS_BUFFER_TOO_SMALL)
        *pcbResult = cbBuf + sizeof (VBOXWDDM_REG_DRVKEY_PREFIX)-2;

    return Status;
}

#define VBOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_VISTA L"\\Registry\\Machine\\System\\CurrentControlSet\\Hardware Profiles\\Current\\System\\CurrentControlSet\\Control\\VIDEO\\"
#define VBOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_WIN7 L"\\Registry\\Machine\\System\\CurrentControlSet\\Hardware Profiles\\UnitedVideo\\CONTROL\\VIDEO\\"

#define VBOXWDDM_REG_DISPLAYSETTINGS_ATTACH_RELX L"Attach.RelativeX"
#define VBOXWDDM_REG_DISPLAYSETTINGS_ATTACH_RELY L"Attach.RelativeY"
#define VBOXWDDM_REG_DISPLAYSETTINGS_ATTACH_DESKTOP L"Attach.ToDesktop"

NTSTATUS vboxWddmRegQueryDisplaySettingsKeyName(PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
        ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PWCHAR pSuffix;
    bool bFallback = false;
    const WCHAR* pKeyPrefix;
    UINT cbKeyPrefix;
    UNICODE_STRING* pVGuid = vboxWddmVGuidGet(pDevExt);
    Assert(pVGuid);
    if (!pVGuid)
        return STATUS_UNSUCCESSFUL;

    vboxWinVersion_t ver = VBoxQueryWinVersion();
    if (ver == WINVISTA)
    {
        pKeyPrefix = VBOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_VISTA;
        cbKeyPrefix = sizeof (VBOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_VISTA);
    }
    else
    {
        Assert(ver == WIN7 || ver == WIN8);
        pKeyPrefix = VBOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_WIN7;
        cbKeyPrefix = sizeof (VBOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_WIN7);
    }

    ULONG cbResult = cbKeyPrefix + pVGuid->Length + 2 + 8; // L"\\" + "XXXX"
    if (cbBuf >= cbResult)
    {
        wcscpy(pBuf, pKeyPrefix);
        pSuffix = pBuf + (cbKeyPrefix-2)/2;
        memcpy(pSuffix, pVGuid->Buffer, pVGuid->Length);
        pSuffix += pVGuid->Length/2;
        pSuffix[0] = L'\\';
        pSuffix += 1;
        swprintf(pSuffix, L"%04d", VidPnSourceId);
    }
    else
    {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    *pcbResult = cbResult;

    return Status;
}

#define VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Video\\"
#define VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY_SUBKEY L"\\Video"

NTSTATUS vboxWddmRegQueryVideoGuidString(ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult)
{
    HANDLE hKey;
    NTSTATUS Status = vboxWddmRegOpenKey(&hKey, VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY, GENERIC_READ);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        struct
        {
            KEY_BASIC_INFORMATION Name;
            WCHAR Buf[256];
        } Buf;
        WCHAR KeyBuf[sizeof (VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY)/2 + 256 + 64];
        wcscpy(KeyBuf, VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY);
        ULONG ResultLength;
        BOOL bFound = FALSE;
        for (ULONG i = 0; !bFound; ++i)
        {
            RtlZeroMemory(&Buf, sizeof (Buf));
            Status = ZwEnumerateKey(hKey, i, KeyBasicInformation, &Buf, sizeof (Buf), &ResultLength);
            Assert(Status == STATUS_SUCCESS);
            /* we should not encounter STATUS_NO_MORE_ENTRIES here since this would mean we did not find our entry */
            if (Status != STATUS_SUCCESS)
                break;

            HANDLE hSubKey;
            PWCHAR pSubBuf = KeyBuf + (sizeof (VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY) - 2)/2;
            memcpy(pSubBuf, Buf.Name.Name, Buf.Name.NameLength);
            pSubBuf += Buf.Name.NameLength/2;
            memcpy(pSubBuf, VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY_SUBKEY, sizeof (VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY_SUBKEY));
            Status = vboxWddmRegOpenKey(&hSubKey, KeyBuf, GENERIC_READ);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                struct
                {
                    KEY_VALUE_PARTIAL_INFORMATION Info;
                    UCHAR Buf[sizeof (L"VBoxVideoWddm")]; /* should be enough */
                } KeyData;
                ULONG cbResult;
                UNICODE_STRING RtlStr;
                RtlInitUnicodeString(&RtlStr, L"Service");
                Status = ZwQueryValueKey(hSubKey,
                            &RtlStr,
                            KeyValuePartialInformation,
                            &KeyData.Info,
                            sizeof(KeyData),
                            &cbResult);
                Assert(Status == STATUS_SUCCESS || STATUS_BUFFER_TOO_SMALL || STATUS_BUFFER_OVERFLOW);
                if (Status == STATUS_SUCCESS)
                {
                    if (KeyData.Info.Type == REG_SZ)
                    {
                        if (KeyData.Info.DataLength == sizeof (L"VBoxVideoWddm"))
                        {
                            if (!wcscmp(L"VBoxVideoWddm", (PWCHAR)KeyData.Info.Data))
                            {
                                bFound = TRUE;
                                *pcbResult = Buf.Name.NameLength + 2;
                                if (cbBuf >= Buf.Name.NameLength + 2)
                                {
                                    memcpy(pBuf, Buf.Name.Name, Buf.Name.NameLength + 2);
                                }
                                else
                                {
                                    Status = STATUS_BUFFER_TOO_SMALL;
                                }
                            }
                        }
                    }
                }

                NTSTATUS tmpStatus = ZwClose(hSubKey);
                Assert(tmpStatus == STATUS_SUCCESS);
            }
            else
                break;
        }
        NTSTATUS tmpStatus = ZwClose(hKey);
        Assert(tmpStatus == STATUS_SUCCESS);
    }

    return Status;
}

NTSTATUS vboxWddmRegOpenKey(OUT PHANDLE phKey, IN PWCHAR pName, IN ACCESS_MASK fAccess)
{
    OBJECT_ATTRIBUTES ObjAttr;
    UNICODE_STRING RtlStr;

    RtlInitUnicodeString(&RtlStr, pName);
    InitializeObjectAttributes(&ObjAttr, &RtlStr, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    return ZwOpenKey(phKey, fAccess, &ObjAttr);
}

NTSTATUS vboxWddmRegOpenDisplaySettingsKey(IN PVBOXMP_DEVEXT pDeviceExtension, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, OUT PHANDLE phKey)
{
    WCHAR Buf[512];
    ULONG cbBuf = sizeof(Buf);
    NTSTATUS Status = vboxWddmRegQueryDisplaySettingsKeyName(pDeviceExtension, VidPnSourceId, cbBuf, Buf, &cbBuf);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxWddmRegOpenKey(phKey, Buf, GENERIC_READ);
        Assert(Status == STATUS_SUCCESS);
        if(Status == STATUS_SUCCESS)
            return STATUS_SUCCESS;
    }

    /* fall-back to make the subsequent VBoxVideoCmnRegXxx calls treat the fail accordingly
     * basically needed to make as less modifications to the current XPDM code as possible */
    *phKey = NULL;

    return Status;
}

NTSTATUS vboxWddmRegDisplaySettingsQueryRelX(HANDLE hKey, int * pResult)
{
    DWORD dwVal;
    NTSTATUS Status = vboxWddmRegQueryValueDword(hKey, VBOXWDDM_REG_DISPLAYSETTINGS_ATTACH_RELX, &dwVal);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        *pResult = (int)dwVal;
    }

    return Status;
}

NTSTATUS vboxWddmRegDisplaySettingsQueryRelY(HANDLE hKey, int * pResult)
{
    DWORD dwVal;
    NTSTATUS Status = vboxWddmRegQueryValueDword(hKey, VBOXWDDM_REG_DISPLAYSETTINGS_ATTACH_RELY, &dwVal);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        *pResult = (int)dwVal;
    }

    return Status;
}

NTSTATUS vboxWddmDisplaySettingsQueryPos(IN PVBOXMP_DEVEXT pDeviceExtension, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, POINT * pPos)
{
    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    HANDLE hKey;
    NTSTATUS Status = vboxWddmRegOpenDisplaySettingsKey(pDeviceExtension, VidPnSourceId, &hKey);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        int x, y;
        Status = vboxWddmRegDisplaySettingsQueryRelX(hKey, &x);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Status = vboxWddmRegDisplaySettingsQueryRelY(hKey, &y);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                pPos->x = x;
                pPos->y = y;
            }
        }
        NTSTATUS tmpStatus = ZwClose(hKey);
        Assert(tmpStatus == STATUS_SUCCESS);
    }
    return Status;
}

NTSTATUS vboxWddmRegQueryValueDword(IN HANDLE hKey, IN PWCHAR pName, OUT PDWORD pDword)
{
    struct
    {
        KEY_VALUE_PARTIAL_INFORMATION Info;
        UCHAR Buf[32]; /* should be enough */
    } Buf;
    ULONG cbBuf;
    UNICODE_STRING RtlStr;
    RtlInitUnicodeString(&RtlStr, pName);
    NTSTATUS Status = ZwQueryValueKey(hKey,
                &RtlStr,
                KeyValuePartialInformation,
                &Buf.Info,
                sizeof(Buf),
                &cbBuf);
    if (Status == STATUS_SUCCESS)
    {
        if (Buf.Info.Type == REG_DWORD)
        {
            Assert(Buf.Info.DataLength == 4);
            *pDword = *((PULONG)Buf.Info.Data);
            return STATUS_SUCCESS;
        }
    }

    return STATUS_INVALID_PARAMETER;
}

NTSTATUS vboxWddmRegSetValueDword(IN HANDLE hKey, IN PWCHAR pName, OUT DWORD val)
{
    UNICODE_STRING RtlStr;
    RtlInitUnicodeString(&RtlStr, pName);
    return ZwSetValueKey(hKey, &RtlStr,
            NULL, /* IN ULONG  TitleIndex  OPTIONAL, reserved */
            REG_DWORD,
            &val,
            sizeof(val));
}

UNICODE_STRING* vboxWddmVGuidGet(PVBOXMP_DEVEXT pDevExt)
{
    if (pDevExt->VideoGuid.Buffer)
        return &pDevExt->VideoGuid;

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    WCHAR VideoGuidBuf[512];
    ULONG cbVideoGuidBuf = sizeof (VideoGuidBuf);
    NTSTATUS Status = vboxWddmRegQueryVideoGuidString(cbVideoGuidBuf, VideoGuidBuf, &cbVideoGuidBuf);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        PWCHAR pBuf = (PWCHAR)vboxWddmMemAllocZero(cbVideoGuidBuf);
        Assert(pBuf);
        if (pBuf)
        {
            memcpy(pBuf, VideoGuidBuf, cbVideoGuidBuf);
            RtlInitUnicodeString(&pDevExt->VideoGuid, pBuf);
            return &pDevExt->VideoGuid;
        }
    }

    return NULL;
}

VOID vboxWddmVGuidFree(PVBOXMP_DEVEXT pDevExt)
{
    if (pDevExt->VideoGuid.Buffer)
    {
        vboxWddmMemFree(pDevExt->VideoGuid.Buffer);
        pDevExt->VideoGuid.Buffer = NULL;
    }
}

/* mm */

NTSTATUS vboxMmInit(PVBOXWDDM_MM pMm, UINT cPages)
{
    UINT cbBuffer = VBOXWDDM_ROUNDBOUND(cPages, 8) >> 3;
    cbBuffer = VBOXWDDM_ROUNDBOUND(cbBuffer, 4);
    PULONG pBuf = (PULONG)vboxWddmMemAllocZero(cbBuffer);
    if (!pBuf)
    {
        Assert(0);
        return STATUS_NO_MEMORY;
    }
    RtlInitializeBitMap(&pMm->BitMap, pBuf, cPages);
    pMm->cPages = cPages;
    pMm->cAllocs = 0;
    pMm->pBuffer = pBuf;
    return STATUS_SUCCESS;
}

ULONG vboxMmAlloc(PVBOXWDDM_MM pMm, UINT cPages)
{
    ULONG iPage = RtlFindClearBitsAndSet(&pMm->BitMap, cPages, 0);
    if (iPage == 0xFFFFFFFF)
    {
        Assert(0);
        return VBOXWDDM_MM_VOID;
    }

    ++pMm->cAllocs;
    return iPage;
}

VOID vboxMmFree(PVBOXWDDM_MM pMm, UINT iPage, UINT cPages)
{
    Assert(RtlAreBitsSet(&pMm->BitMap, iPage, cPages));
    RtlClearBits(&pMm->BitMap, iPage, cPages);
    --pMm->cAllocs;
    Assert(pMm->cAllocs < UINT32_MAX);
}

NTSTATUS vboxMmTerm(PVBOXWDDM_MM pMm)
{
    Assert(!pMm->cAllocs);
    vboxWddmMemFree(pMm->pBuffer);
    pMm->pBuffer = NULL;
    return STATUS_SUCCESS;
}



typedef struct VBOXVIDEOCM_ALLOC
{
    VBOXWDDM_HANDLE hGlobalHandle;
    uint32_t offData;
    uint32_t cbData;
} VBOXVIDEOCM_ALLOC, *PVBOXVIDEOCM_ALLOC;

typedef struct VBOXVIDEOCM_ALLOC_REF
{
    PVBOXVIDEOCM_ALLOC_CONTEXT pContext;
    VBOXWDDM_HANDLE hSessionHandle;
    PVBOXVIDEOCM_ALLOC pAlloc;
    union
    {
        PKEVENT pSynchEvent;
        PRKSEMAPHORE pSynchSemaphore;
    };
    VBOXUHGSMI_SYNCHOBJECT_TYPE enmSynchType;
    volatile uint32_t cRefs;
    MDL Mdl;
} VBOXVIDEOCM_ALLOC_REF, *PVBOXVIDEOCM_ALLOC_REF;


NTSTATUS vboxVideoCmAllocAlloc(PVBOXVIDEOCM_ALLOC_MGR pMgr, PVBOXVIDEOCM_ALLOC pAlloc)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    UINT cbSize = pAlloc->cbData;
    UINT cPages = BYTES_TO_PAGES(cbSize);
    ExAcquireFastMutex(&pMgr->Mutex);
    UINT iPage = vboxMmAlloc(&pMgr->Mm, cPages);
    if (iPage != VBOXWDDM_MM_VOID)
    {
        uint32_t offData = pMgr->offData + (iPage << PAGE_SHIFT);
        Assert(offData + cbSize <= pMgr->offData + pMgr->cbData);
        pAlloc->offData = offData;
        pAlloc->hGlobalHandle = vboxWddmHTablePut(&pMgr->AllocTable, pAlloc);
        ExReleaseFastMutex(&pMgr->Mutex);
        if (VBOXWDDM_HANDLE_INVALID != pAlloc->hGlobalHandle)
            return STATUS_SUCCESS;

        Assert(0);
        Status = STATUS_NO_MEMORY;
        vboxMmFree(&pMgr->Mm, iPage, cPages);
    }
    else
    {
        Assert(0);
        ExReleaseFastMutex(&pMgr->Mutex);
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }
    return Status;
}

VOID vboxVideoCmAllocDealloc(PVBOXVIDEOCM_ALLOC_MGR pMgr, PVBOXVIDEOCM_ALLOC pAlloc)
{
    UINT cbSize = pAlloc->cbData;
    UINT cPages = BYTES_TO_PAGES(cbSize);
    UINT iPage = BYTES_TO_PAGES(pAlloc->offData - pMgr->offData);
    ExAcquireFastMutex(&pMgr->Mutex);
    vboxWddmHTableRemove(&pMgr->AllocTable, pAlloc->hGlobalHandle);
    vboxMmFree(&pMgr->Mm, iPage, cPages);
    ExReleaseFastMutex(&pMgr->Mutex);
}


NTSTATUS vboxVideoAMgrAllocCreate(PVBOXVIDEOCM_ALLOC_MGR pMgr, UINT cbSize, PVBOXVIDEOCM_ALLOC *ppAlloc)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDEOCM_ALLOC pAlloc = (PVBOXVIDEOCM_ALLOC)vboxWddmMemAllocZero(sizeof (*pAlloc));
    if (pAlloc)
    {
        pAlloc->cbData = cbSize;
        Status = vboxVideoCmAllocAlloc(pMgr, pAlloc);
        if (Status == STATUS_SUCCESS)
        {
            *ppAlloc = pAlloc;
            return STATUS_SUCCESS;
        }

        Assert(0);
        vboxWddmMemFree(pAlloc);
    }
    else
    {
        Assert(0);
        Status = STATUS_NO_MEMORY;
    }

    return Status;
}

VOID vboxVideoAMgrAllocDestroy(PVBOXVIDEOCM_ALLOC_MGR pMgr, PVBOXVIDEOCM_ALLOC pAlloc)
{
    vboxVideoCmAllocDealloc(pMgr, pAlloc);
    vboxWddmMemFree(pAlloc);
}

NTSTATUS vboxVideoAMgrCtxAllocMap(PVBOXVIDEOCM_ALLOC_CONTEXT pContext, PVBOXVIDEOCM_ALLOC pAlloc, PVBOXVIDEOCM_UM_ALLOC pUmAlloc)
{
    PVBOXVIDEOCM_ALLOC_MGR pMgr = pContext->pMgr;
    NTSTATUS Status = STATUS_SUCCESS;

    union
    {
        PKEVENT pSynchEvent;
        PRKSEMAPHORE pSynchSemaphore;
    };

    switch (pUmAlloc->enmSynchType)
    {
        case VBOXUHGSMI_SYNCHOBJECT_TYPE_EVENT:
            Status = ObReferenceObjectByHandle((HANDLE)pUmAlloc->hSynch, EVENT_MODIFY_STATE, *ExEventObjectType, UserMode,
                    (PVOID*)&pSynchEvent,
                    NULL);
            Assert(Status == STATUS_SUCCESS);
            Assert(pSynchEvent);
            break;
        case VBOXUHGSMI_SYNCHOBJECT_TYPE_SEMAPHORE:
            Status = ObReferenceObjectByHandle((HANDLE)pUmAlloc->hSynch, EVENT_MODIFY_STATE, *ExSemaphoreObjectType, UserMode,
                    (PVOID*)&pSynchSemaphore,
                    NULL);
            Assert(Status == STATUS_SUCCESS);
            Assert(pSynchSemaphore);
            break;
        case VBOXUHGSMI_SYNCHOBJECT_TYPE_NONE:
            pSynchEvent = NULL;
            Status = STATUS_SUCCESS;
            break;
        default:
            LOGREL(("ERROR: invalid synch info type(%d)", pUmAlloc->enmSynchType));
            AssertBreakpoint();
            Status = STATUS_INVALID_PARAMETER;
            break;
    }

    if (Status == STATUS_SUCCESS)
    {
        PVOID BaseVa = pMgr->pvData + pAlloc->offData - pMgr->offData;
        SIZE_T cbLength = pAlloc->cbData;

        PVBOXVIDEOCM_ALLOC_REF pAllocRef = (PVBOXVIDEOCM_ALLOC_REF)vboxWddmMemAllocZero(sizeof (*pAllocRef) + sizeof (PFN_NUMBER) * ADDRESS_AND_SIZE_TO_SPAN_PAGES(BaseVa, cbLength));
        if (pAllocRef)
        {
            pAllocRef->cRefs = 1;
            MmInitializeMdl(&pAllocRef->Mdl, BaseVa, cbLength);
            __try
            {
                MmProbeAndLockPages(&pAllocRef->Mdl, KernelMode, IoWriteAccess);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                Assert(0);
                Status = STATUS_UNSUCCESSFUL;
            }

            if (Status == STATUS_SUCCESS)
            {
                PVOID pvUm = MmMapLockedPagesSpecifyCache(&pAllocRef->Mdl, UserMode, MmNonCached,
                          NULL, /* PVOID BaseAddress */
                          FALSE, /* ULONG BugCheckOnFailure */
                          NormalPagePriority);
                if (pvUm)
                {
                    pAllocRef->pContext = pContext;
                    pAllocRef->pAlloc = pAlloc;
                    pAllocRef->enmSynchType = pUmAlloc->enmSynchType;
                    pAllocRef->pSynchEvent = pSynchEvent;
                    ExAcquireFastMutex(&pContext->Mutex);
                    pAllocRef->hSessionHandle = vboxWddmHTablePut(&pContext->AllocTable, pAllocRef);
                    ExReleaseFastMutex(&pContext->Mutex);
                    if (VBOXWDDM_HANDLE_INVALID != pAllocRef->hSessionHandle)
                    {
                        pUmAlloc->hAlloc = pAllocRef->hSessionHandle;
                        pUmAlloc->cbData = pAlloc->cbData;
                        pUmAlloc->pvData = (uint64_t)pvUm;
                        return STATUS_SUCCESS;
                    }
                }
                else
                {
                    Assert(0);
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                }

                MmUnlockPages(&pAllocRef->Mdl);
            }

            vboxWddmMemFree(pAllocRef);
        }
        else
        {
            Assert(0);
            Status = STATUS_NO_MEMORY;
        }

        if (pSynchEvent)
        {
            ObDereferenceObject(pSynchEvent);
        }
    }
    else
    {
        Assert(0);
    }


    return Status;
}

NTSTATUS vboxVideoAMgrCtxAllocUnmap(PVBOXVIDEOCM_ALLOC_CONTEXT pContext, VBOXDISP_KMHANDLE hSesionHandle, PVBOXVIDEOCM_ALLOC *ppAlloc)
{
    NTSTATUS Status = STATUS_SUCCESS;
    ExAcquireFastMutex(&pContext->Mutex);
    PVBOXVIDEOCM_ALLOC_REF pAllocRef = (PVBOXVIDEOCM_ALLOC_REF)vboxWddmHTableRemove(&pContext->AllocTable, hSesionHandle);
    ExReleaseFastMutex(&pContext->Mutex);
    if (pAllocRef)
    {
        /* wait for the dereference, i.e. for all commands involving this allocation to complete */
        vboxWddmCounterU32Wait(&pAllocRef->cRefs, 1);

        MmUnlockPages(&pAllocRef->Mdl);
        *ppAlloc = pAllocRef->pAlloc;
        if (pAllocRef->pSynchEvent)
        {
            ObDereferenceObject(pAllocRef->pSynchEvent);
        }
        vboxWddmMemFree(pAllocRef);
    }
    else
    {
        Assert(0);
        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

static PVBOXVIDEOCM_ALLOC_REF vboxVideoAMgrCtxAllocRefAcquire(PVBOXVIDEOCM_ALLOC_CONTEXT pContext, VBOXDISP_KMHANDLE hSesionHandle)
{
    ExAcquireFastMutex(&pContext->Mutex);
    PVBOXVIDEOCM_ALLOC_REF pAllocRef = (PVBOXVIDEOCM_ALLOC_REF)vboxWddmHTableGet(&pContext->AllocTable, hSesionHandle);
    ASMAtomicIncU32(&pAllocRef->cRefs);
    ExReleaseFastMutex(&pContext->Mutex);
    return pAllocRef;
}

static VOID vboxVideoAMgrCtxAllocRefRelease(PVBOXVIDEOCM_ALLOC_REF pRef)
{
    uint32_t cRefs = ASMAtomicDecU32(&pRef->cRefs);
    Assert(cRefs < UINT32_MAX/2);
    Assert(cRefs >= 1); /* we do not do cleanup-on-zero here, instead we wait for the cRefs to reach 1 in vboxVideoAMgrCtxAllocUnmap before unmapping */
}



NTSTATUS vboxVideoAMgrCtxAllocCreate(PVBOXVIDEOCM_ALLOC_CONTEXT pContext, PVBOXVIDEOCM_UM_ALLOC pUmAlloc)
{
    PVBOXVIDEOCM_ALLOC pAlloc;
    PVBOXVIDEOCM_ALLOC_MGR pMgr = pContext->pMgr;
    NTSTATUS Status = vboxVideoAMgrAllocCreate(pMgr, pUmAlloc->cbData, &pAlloc);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxVideoAMgrCtxAllocMap(pContext, pAlloc, pUmAlloc);
        if (Status == STATUS_SUCCESS)
            return STATUS_SUCCESS;
        else
        {
            Assert(0);
        }
        vboxVideoAMgrAllocDestroy(pMgr, pAlloc);
    }
    else
    {
        Assert(0);
    }
    return Status;
}

NTSTATUS vboxVideoAMgrCtxAllocDestroy(PVBOXVIDEOCM_ALLOC_CONTEXT pContext, VBOXDISP_KMHANDLE hSesionHandle)
{
    PVBOXVIDEOCM_ALLOC pAlloc;
    PVBOXVIDEOCM_ALLOC_MGR pMgr = pContext->pMgr;
    NTSTATUS Status = vboxVideoAMgrCtxAllocUnmap(pContext, hSesionHandle, &pAlloc);
    if (Status == STATUS_SUCCESS)
    {
        vboxVideoAMgrAllocDestroy(pMgr, pAlloc);
    }
    else
    {
        Assert(0);
    }
    return Status;
}

#ifdef VBOX_WITH_CRHGSMI
static DECLCALLBACK(VOID) vboxVideoAMgrAllocSubmitCompletion(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext)
{
    /* we should be called from our DPC routine */
    Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

    PVBOXVDMACBUF_DR pDr = (PVBOXVDMACBUF_DR)pvContext;
    PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
    VBOXVDMACMD_CHROMIUM_CMD *pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHROMIUM_CMD);
    UINT cBufs = pBody->cBuffers;
    for (UINT i = 0; i < cBufs; ++i)
    {
        VBOXVDMACMD_CHROMIUM_BUFFER *pBufCmd = &pBody->aBuffers[i];
        PVBOXVIDEOCM_ALLOC_REF pRef = (PVBOXVIDEOCM_ALLOC_REF)pBufCmd->u64GuesData;
        if (!pBufCmd->u32GuesData)
        {
            /* signal completion */
            switch (pRef->enmSynchType)
            {
                case VBOXUHGSMI_SYNCHOBJECT_TYPE_EVENT:
                    KeSetEvent(pRef->pSynchEvent, 3, FALSE);
                    break;
                case VBOXUHGSMI_SYNCHOBJECT_TYPE_SEMAPHORE:
                    KeReleaseSemaphore(pRef->pSynchSemaphore,
                        3,
                        1,
                        FALSE);
                    break;
                case VBOXUHGSMI_SYNCHOBJECT_TYPE_NONE:
                    break;
                default:
                    Assert(0);
            }
        }

        vboxVideoAMgrCtxAllocRefRelease(pRef);
    }

    vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);
}

/* submits a set of chromium uhgsmi buffers to host for processing */
NTSTATUS vboxVideoAMgrCtxAllocSubmit(PVBOXMP_DEVEXT pDevExt, PVBOXVIDEOCM_ALLOC_CONTEXT pContext, UINT cBuffers, VBOXWDDM_UHGSMI_BUFFER_UI_INFO_ESCAPE *paBuffers)
{
    /* ensure we do not overflow the 32bit buffer size value */
    if (VBOXWDDM_TRAILARRAY_MAXELEMENTSU32(VBOXVDMACMD_CHROMIUM_CMD, aBuffers) < cBuffers)
    {
        WARN(("number of buffers passed too big (%d), max is (%d)", cBuffers, VBOXWDDM_TRAILARRAY_MAXELEMENTSU32(VBOXVDMACMD_CHROMIUM_CMD, aBuffers)));
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS Status = STATUS_SUCCESS;
    UINT cbCmd = VBOXVDMACMD_SIZE_FROMBODYSIZE(RT_OFFSETOF(VBOXVDMACMD_CHROMIUM_CMD, aBuffers[cBuffers]));

    PVBOXVDMACBUF_DR pDr = vboxVdmaCBufDrCreate (&pDevExt->u.primary.Vdma, cbCmd);
    if (pDr)
    {
        // vboxVdmaCBufDrCreate zero initializes the pDr
        pDr->fFlags = VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR;
        pDr->cbBuf = cbCmd;
        pDr->rc = VERR_NOT_IMPLEMENTED;

        PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
        pHdr->enmType = VBOXVDMACMD_TYPE_CHROMIUM_CMD;
        pHdr->u32CmdSpecific = 0;
        VBOXVDMACMD_CHROMIUM_CMD *pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHROMIUM_CMD);
        pBody->cBuffers = cBuffers;
        for (UINT i = 0; i < cBuffers; ++i)
        {
            VBOXVDMACMD_CHROMIUM_BUFFER *pBufCmd = &pBody->aBuffers[i];
            VBOXWDDM_UHGSMI_BUFFER_UI_INFO_ESCAPE *pBufInfo = &paBuffers[i];
            PVBOXVIDEOCM_ALLOC_REF pRef = vboxVideoAMgrCtxAllocRefAcquire(pContext, pBufInfo->hAlloc);
            if (pRef)
            {
#ifdef DEBUG_misha
                Assert(pRef->cRefs == 2);
#endif
                pBufCmd->offBuffer = pRef->pAlloc->offData + pBufInfo->Info.offData;
                pBufCmd->cbBuffer = pBufInfo->Info.cbData;
                pBufCmd->u32GuesData = pBufInfo->Info.fSubFlags.bDoNotSignalCompletion;
                pBufCmd->u64GuesData = (uint64_t)pRef;
            }
            else
            {
                WARN(("vboxVideoAMgrCtxAllocRefAcquire failed for hAlloc(0x%x)\n", pBufInfo->hAlloc));
                /* release all previously acquired aloc references */
                for (UINT j = 0; j < i; ++j)
                {
                    VBOXVDMACMD_CHROMIUM_BUFFER *pBufCmdJ = &pBody->aBuffers[j];
                    PVBOXVIDEOCM_ALLOC_REF pRefJ = (PVBOXVIDEOCM_ALLOC_REF)pBufCmdJ;
                    vboxVideoAMgrCtxAllocRefRelease(pRefJ);
                }
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
        }

        if (Status == STATUS_SUCCESS)
        {
            PVBOXVDMADDI_CMD pDdiCmd = VBOXVDMADDI_CMD_FROM_BUF_DR(pDr);
            vboxVdmaDdiCmdInit(pDdiCmd, 0, 0, vboxVideoAMgrAllocSubmitCompletion, pDr);
            /* mark command as submitted & invisible for the dx runtime since dx did not originate it */
            vboxVdmaDdiCmdSubmittedNotDx(pDdiCmd);
            int rc = vboxVdmaCBufDrSubmit(pDevExt, &pDevExt->u.primary.Vdma, pDr);
            if (RT_SUCCESS(rc))
            {
                return STATUS_SUCCESS;
            }

            WARN(("vboxVdmaCBufDrSubmit failed with rc (%d)\n", rc));

            /* failure branch */
            /* release all previously acquired aloc references */
            for (UINT i = 0; i < cBuffers; ++i)
            {
                VBOXVDMACMD_CHROMIUM_BUFFER *pBufCmd = &pBody->aBuffers[i];
                PVBOXVIDEOCM_ALLOC_REF pRef = (PVBOXVIDEOCM_ALLOC_REF)pBufCmd;
                vboxVideoAMgrCtxAllocRefRelease(pRef);
            }
        }

        vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);
    }
    else
    {
        Assert(0);
        /* @todo: try flushing.. */
        LOGREL(("vboxVdmaCBufDrCreate returned NULL"));
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}
#endif

NTSTATUS vboxVideoAMgrCreate(PVBOXMP_DEVEXT pDevExt, PVBOXVIDEOCM_ALLOC_MGR pMgr, uint32_t offData, uint32_t cbData)
{
    Assert(!(offData & (PAGE_SIZE -1)));
    Assert(!(cbData & (PAGE_SIZE -1)));
    offData = VBOXWDDM_ROUNDBOUND(offData, PAGE_SIZE);
    cbData &= (~(PAGE_SIZE -1));
    Assert(cbData);
    if (!cbData)
        return STATUS_INVALID_PARAMETER;

    ExInitializeFastMutex(&pMgr->Mutex);
    NTSTATUS Status = vboxWddmHTableCreate(&pMgr->AllocTable, 64);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxMmInit(&pMgr->Mm, BYTES_TO_PAGES(cbData));
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            PHYSICAL_ADDRESS PhysicalAddress = {0};
            PhysicalAddress.QuadPart = VBE_DISPI_LFB_PHYSICAL_ADDRESS + offData;
            pMgr->pvData = (uint8_t*)MmMapIoSpace(PhysicalAddress, cbData, MmNonCached);
            Assert(pMgr->pvData);
            if (pMgr->pvData)
            {
                pMgr->offData = offData;
                pMgr->cbData = cbData;
                return STATUS_SUCCESS;
            }
            else
            {
                Status = STATUS_UNSUCCESSFUL;
            }
            vboxMmTerm(&pMgr->Mm);
        }
        vboxWddmHTableDestroy(&pMgr->AllocTable);
    }

    return Status;
}

NTSTATUS vboxVideoAMgrDestroy(PVBOXMP_DEVEXT pDevExt, PVBOXVIDEOCM_ALLOC_MGR pMgr)
{
    MmUnmapIoSpace(pMgr->pvData, pMgr->cbData);
    vboxMmTerm(&pMgr->Mm);
    vboxWddmHTableDestroy(&pMgr->AllocTable);
    return STATUS_SUCCESS;
}

NTSTATUS vboxVideoAMgrCtxCreate(PVBOXVIDEOCM_ALLOC_MGR pMgr, PVBOXVIDEOCM_ALLOC_CONTEXT pCtx)
{
    NTSTATUS Status = STATUS_NOT_SUPPORTED;
    if (pMgr->pvData)
    {
        ExInitializeFastMutex(&pCtx->Mutex);
        Status = vboxWddmHTableCreate(&pCtx->AllocTable, 32);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            pCtx->pMgr = pMgr;
            return STATUS_SUCCESS;
        }
    }
    return Status;
}

NTSTATUS vboxVideoAMgrCtxDestroy(PVBOXVIDEOCM_ALLOC_CONTEXT pCtx)
{
    if (!pCtx->pMgr)
        return STATUS_SUCCESS;

    VBOXWDDM_HTABLE_ITERATOR Iter;
    NTSTATUS Status = STATUS_SUCCESS;

    vboxWddmHTableIterInit(&pCtx->AllocTable, &Iter);
    do
    {
        PVBOXVIDEOCM_ALLOC_REF pRef = (PVBOXVIDEOCM_ALLOC_REF)vboxWddmHTableIterNext(&Iter, NULL);
        if (!pRef)
            break;

        Status = vboxVideoAMgrCtxAllocDestroy(pCtx, pRef->hSessionHandle);
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
            break;
        //        vboxWddmHTableIterRemoveCur(&Iter);
    } while (1);

    if (Status == STATUS_SUCCESS)
    {
        vboxWddmHTableDestroy(&pCtx->AllocTable);
    }

    return Status;
}


VOID vboxWddmSleep(uint32_t u32Val)
{
    LARGE_INTEGER Interval;
    Interval.QuadPart = -(int64_t) 2 /* ms */ * 10000;

    KeDelayExecutionThread(KernelMode, FALSE, &Interval);
}

VOID vboxWddmCounterU32Wait(uint32_t volatile * pu32, uint32_t u32Val)
{
    LARGE_INTEGER Interval;
    Interval.QuadPart = -(int64_t) 2 /* ms */ * 10000;
    uint32_t u32CurVal;

    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);

    while ((u32CurVal = ASMAtomicReadU32(pu32)) != u32Val)
    {
        Assert(u32CurVal >= u32Val);
        Assert(u32CurVal < UINT32_MAX/2);

        KeDelayExecutionThread(KernelMode, FALSE, &Interval);
    }
}

/* dump user-mode driver debug info */
static char    g_aVBoxUmdD3DCAPS9[304];
static VBOXDISPIFESCAPE_DBGDUMPBUF_FLAGS g_VBoxUmdD3DCAPS9Flags;
static BOOLEAN g_bVBoxUmdD3DCAPS9IsInited = FALSE;

static void vboxUmdDumpDword(DWORD *pvData, DWORD cData)
{
    char aBuf[16*4];
    DWORD dw1, dw2, dw3, dw4;
    for (UINT i = 0; i < (cData & (~3)); i+=4)
    {
        dw1 = *pvData++;
        dw2 = *pvData++;
        dw3 = *pvData++;
        dw4 = *pvData++;
        sprintf(aBuf, "0x%08x, 0x%08x, 0x%08x, 0x%08x,\n", dw1, dw2, dw3, dw4);
        LOGREL(("%s", aBuf));
    }

    cData = cData % 4;
    switch (cData)
    {
        case 3:
            dw1 = *pvData++;
            dw2 = *pvData++;
            dw3 = *pvData++;
            sprintf(aBuf, "0x%08x, 0x%08x, 0x%08x\n", dw1, dw2, dw3);
            LOGREL(("%s", aBuf));
            break;
        case 2:
            dw1 = *pvData++;
            dw2 = *pvData++;
            sprintf(aBuf, "0x%08x, 0x%08x\n", dw1, dw2);
            LOGREL(("%s", aBuf));
            break;
        case 1:
            dw1 = *pvData++;
            sprintf(aBuf, "0x%8x\n", dw1);
            LOGREL(("%s", aBuf));
            break;
        default:
            break;
    }
}

static void vboxUmdDumpD3DCAPS9(void *pvData, PVBOXDISPIFESCAPE_DBGDUMPBUF_FLAGS pFlags)
{
    AssertCompile(!(sizeof (g_aVBoxUmdD3DCAPS9) % sizeof (DWORD)));
    LOGREL(("*****Start Dumping D3DCAPS9:*******"));
    LOGREL(("WoW64 flag(%d)", (UINT)pFlags->WoW64));
    vboxUmdDumpDword((DWORD*)pvData, sizeof (g_aVBoxUmdD3DCAPS9) / sizeof (DWORD));
    LOGREL(("*****End Dumping D3DCAPS9**********"));
}

NTSTATUS vboxUmdDumpBuf(PVBOXDISPIFESCAPE_DBGDUMPBUF pBuf, uint32_t cbBuffer)
{
    if (cbBuffer < RT_OFFSETOF(VBOXDISPIFESCAPE_DBGDUMPBUF, aBuf[0]))
    {
        WARN(("Buffer too small"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    NTSTATUS Status = STATUS_SUCCESS;
    uint32_t cbString = cbBuffer - RT_OFFSETOF(VBOXDISPIFESCAPE_DBGDUMPBUF, aBuf[0]);
    switch (pBuf->enmType)
    {
        case VBOXDISPIFESCAPE_DBGDUMPBUF_TYPE_D3DCAPS9:
        {
            if (cbString != sizeof (g_aVBoxUmdD3DCAPS9))
            {
                WARN(("wrong caps size, expected %d, but was %d", sizeof (g_aVBoxUmdD3DCAPS9), cbString));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (g_bVBoxUmdD3DCAPS9IsInited)
            {
                if (!memcmp(g_aVBoxUmdD3DCAPS9, pBuf->aBuf, sizeof (g_aVBoxUmdD3DCAPS9)))
                    break;

                WARN(("caps do not match!"));
                vboxUmdDumpD3DCAPS9(pBuf->aBuf, &pBuf->Flags);
                break;
            }

            memcpy(g_aVBoxUmdD3DCAPS9, pBuf->aBuf, sizeof (g_aVBoxUmdD3DCAPS9));
            g_VBoxUmdD3DCAPS9Flags = pBuf->Flags;
            g_bVBoxUmdD3DCAPS9IsInited = TRUE;
            vboxUmdDumpD3DCAPS9(pBuf->aBuf, &pBuf->Flags);
        }
    }

    return Status;
}

#if 0
VOID vboxShRcTreeInit(PVBOXMP_DEVEXT pDevExt)
{
    ExInitializeFastMutex(&pDevExt->ShRcTreeMutex);
    pDevExt->ShRcTree = NULL;
}

VOID vboxShRcTreeTerm(PVBOXMP_DEVEXT pDevExt)
{
    Assert(!pDevExt->ShRcTree);
    pDevExt->ShRcTree = NULL;
}

BOOLEAN vboxShRcTreePut(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAlloc)
{
    HANDLE hSharedRc = pAlloc->hSharedHandle;
    if (!hSharedRc)
    {
        WARN(("invalid call with zero shared handle!"));
        return FALSE;
    }
    pAlloc->ShRcTreeEntry.Key = (AVLPVKEY)hSharedRc;
    ExAcquireFastMutex(&pDevExt->ShRcTreeMutex);
    bool bRc = RTAvlPVInsert(&pDevExt->ShRcTree, &pAlloc->ShRcTreeEntry);
    ExReleaseFastMutex(&pDevExt->ShRcTreeMutex);
    Assert(bRc);
    return (BOOLEAN)bRc;
}

#define PVBOXWDDM_ALLOCATION_FROM_SHRCTREENODE(_p) ((PVBOXWDDM_ALLOCATION)(((uint8_t*)(_p)) - RT_OFFSETOF(VBOXWDDM_ALLOCATION, ShRcTreeEntry)))
PVBOXWDDM_ALLOCATION vboxShRcTreeGet(PVBOXMP_DEVEXT pDevExt, HANDLE hSharedRc)
{
    ExAcquireFastMutex(&pDevExt->ShRcTreeMutex);
    PAVLPVNODECORE pNode = RTAvlPVGet(&pDevExt->ShRcTree, (AVLPVKEY)hSharedRc);
    ExReleaseFastMutex(&pDevExt->ShRcTreeMutex);
    if (!pNode)
        return NULL;
    PVBOXWDDM_ALLOCATION pAlloc = PVBOXWDDM_ALLOCATION_FROM_SHRCTREENODE(pNode);
    return pAlloc;
}

BOOLEAN vboxShRcTreeRemove(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAlloc)
{
    HANDLE hSharedRc = pAlloc->hSharedHandle;
    if (!hSharedRc)
    {
        WARN(("invalid call with zero shared handle!"));
        return FALSE;
    }
    ExAcquireFastMutex(&pDevExt->ShRcTreeMutex);
    PAVLPVNODECORE pNode = RTAvlPVRemove(&pDevExt->ShRcTree, (AVLPVKEY)hSharedRc);
    ExReleaseFastMutex(&pDevExt->ShRcTreeMutex);
    if (!pNode)
        return NULL;
    PVBOXWDDM_ALLOCATION pRetAlloc = PVBOXWDDM_ALLOCATION_FROM_SHRCTREENODE(pNode);
    Assert(pRetAlloc == pAlloc);
    return !!pRetAlloc;
}
#endif
