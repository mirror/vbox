/** @file
 *
 * VBoxVideo Display D3D User mode dll
 *
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
#include "VBoxDispD3DCmn.h"

#include <iprt/mem.h>
#include <iprt/err.h>

typedef struct VBOXUHGSMI_BUFFER_PRIVATE_D3D
{
    VBOXUHGSMI_BUFFER Base;
    PVBOXWDDMDISP_DEVICE pDevice;
    D3DKMT_HANDLE hAllocation;
    UINT aLockPageIndices[1];
} VBOXUHGSMI_BUFFER_PRIVATE_D3D, *PVBOXUHGSMI_BUFFER_PRIVATE_D3D;

#define VBOXUHGSMID3D_GET_PRIVATE(_p, _t) ((_t*)(((uint8_t*)_p) - RT_OFFSETOF(_t, Base)))
#define VBOXUHGSMID3D_GET(_p) VBOXUHGSMID3D_GET_PRIVATE(_p, VBOXUHGSMI_PRIVATE_D3D)
#define VBOXUHGSMID3D_GET_BUFFER(_p) VBOXUHGSMID3D_GET_PRIVATE(_p, VBOXUHGSMI_BUFFER_PRIVATE_D3D)

DECLCALLBACK(int) vboxUhgsmiD3DBufferDestroy(PVBOXUHGSMI_BUFFER pBuf)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_D3D pBuffer = VBOXUHGSMID3D_GET_BUFFER(pBuf);
    D3DDDICB_DEALLOCATE DdiDealloc;
    DdiDealloc.hResource = 0;
    DdiDealloc.NumAllocations = 1;
    DdiDealloc.HandleList = &pBuffer->hAllocation;
    HRESULT hr = pBuffer->pDevice->RtCallbacks.pfnDeallocateCb(pBuffer->pDevice->hDevice, &DdiDealloc);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        if (pBuffer->Base.bSynchCreated)
        {
            CloseHandle(pBuffer->Base.hSynch);
        }
        RTMemFree(pBuffer);
        return VINF_SUCCESS;
    }
    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiD3DBufferLock(PVBOXUHGSMI_BUFFER pBuf, uint32_t offLock, uint32_t cbLock, VBOXUHGSMI_BUFFER_LOCK_FLAGS fFlags, void**pvLock)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_D3D pBuffer = VBOXUHGSMID3D_GET_BUFFER(pBuf);
    D3DDDICB_LOCK DdiLock;
    DdiLock.hAllocation = pBuffer->hAllocation;
    DdiLock.PrivateDriverData = 0;

    if (fFlags.bLockEntire)
    {
        offLock = 0;
        DdiLock.Flags.Value = 0;
        DdiLock.Flags.LockEntire = 1;
    }
    else
    {
        if (!cbLock)
        {
            Assert(0);
            return VERR_INVALID_PARAMETER;
        }
        if (offLock + cbLock > pBuf->cbBuffer)
        {
            Assert(0);
            return VERR_INVALID_PARAMETER;
        }

        uint32_t iFirstPage = offLock >> 0x1000;
        uint32_t iAfterLastPage = (cbLock + 0xfff) >> 0x1000;
        uint32_t cPages = iAfterLastPage - iFirstPage;
        uint32_t cBufPages = pBuf->cbBuffer >> 0x1000;
        Assert(cPages <= (cBufPages));

        if (cPages == cBufPages)
        {
            DdiLock.NumPages = 0;
            DdiLock.pPages = NULL;
            DdiLock.Flags.Value = 0;
            DdiLock.Flags.LockEntire = 1;
        }
        else
        {
            DdiLock.NumPages = cPages;
            DdiLock.pPages = pBuffer->aLockPageIndices;
            DdiLock.Flags.Value = 0;
            for (UINT i = 0, j = iFirstPage; i < cPages; ++i, ++j)
            {
                pBuffer->aLockPageIndices[i] = j;
            }
        }

    }
    DdiLock.pData = NULL;
    DdiLock.Flags.ReadOnly = fFlags.bReadOnly;
    DdiLock.Flags.WriteOnly = fFlags.bWriteOnly;
    DdiLock.Flags.DonotWait = fFlags.bDonotWait;
//    DdiLock.Flags.Discard = fFlags.bDiscard;

    HRESULT hr = pBuffer->pDevice->RtCallbacks.pfnLockCb(pBuffer->pDevice->hDevice, &DdiLock);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        *pvLock = (void*)(((uint8_t*)DdiLock.pData) + (offLock & 0xfff));
        return VINF_SUCCESS;
    }
    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiD3DBufferUnlock(PVBOXUHGSMI_BUFFER pBuf)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_D3D pBuffer = VBOXUHGSMID3D_GET_BUFFER(pBuf);
    D3DDDICB_UNLOCK DdiUnlock;
    DdiUnlock.NumAllocations = 1;
    DdiUnlock.phAllocations = &pBuffer->hAllocation;
    HRESULT hr = pBuffer->pDevice->RtCallbacks.pfnUnlockCb(pBuffer->pDevice->hDevice, &DdiUnlock);
    Assert(hr == S_OK);
    if (hr == S_OK)
        return VINF_SUCCESS;
    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiD3DBufferCreate(PVBOXUHGSMI pHgsmi, uint32_t cbBuf,
        VBOXUHGSMI_SYNCHOBJECT_TYPE enmSynchType, HVBOXUHGSMI_SYNCHOBJECT hSynch,
        PVBOXUHGSMI_BUFFER* ppBuf)
{
    bool bSynchCreated = false;
    if (!cbBuf)
        return VERR_INVALID_PARAMETER;

    switch (enmSynchType)
    {
        case VBOXUHGSMI_SYNCHOBJECT_TYPE_EVENT:
            if (!hSynch)
            {
                hSynch = CreateEvent(
                  NULL, /* LPSECURITY_ATTRIBUTES lpEventAttributes */
                  FALSE, /* BOOL bManualReset */
                  FALSE, /* BOOL bInitialState */
                  NULL /* LPCTSTR lpName */
                );
                Assert(hSynch);
                if (!hSynch)
                {
                    DWORD winEr = GetLastError();
                    /* todo: translate winer */
                    return VERR_GENERAL_FAILURE;
                }
                bSynchCreated = true;
            }
            break;
        case VBOXUHGSMI_SYNCHOBJECT_TYPE_SEMAPHORE:
            if (!hSynch)
            {
                hSynch = CreateSemaphore(
                  NULL, /* LPSECURITY_ATTRIBUTES lpSemaphoreAttributes */
                  0, /* LONG lInitialCount */
                  ~0L, /* LONG lMaximumCount */
                  NULL /* LPCTSTR lpName */
                );
                Assert(hSynch);
                if (!hSynch)
                {
                    DWORD winEr = GetLastError();
                    /* todo: translate winer */
                    return VERR_GENERAL_FAILURE;
                }
                bSynchCreated = true;
            }
            break;
        default:
            Assert(0);
            return VERR_INVALID_PARAMETER;
    }

    cbBuf = VBOXWDDM_ROUNDBOUND(cbBuf, 0x1000);
    Assert(cbBuf);
    uint32_t cPages = cbBuf >> 0x1000;
    Assert(cPages);

    int rc;
    PVBOXUHGSMI_PRIVATE_D3D pPrivate = VBOXUHGSMID3D_GET(pHgsmi);
    PVBOXUHGSMI_BUFFER_PRIVATE_D3D pBuf = (PVBOXUHGSMI_BUFFER_PRIVATE_D3D)RTMemAllocZ(RT_OFFSETOF(VBOXUHGSMI_BUFFER_PRIVATE_D3D, aLockPageIndices));
    Assert(pBuf);
    if (pBuf)
    {
        struct
        {
            D3DDDICB_ALLOCATE DdiAlloc;
            D3DDDI_ALLOCATIONINFO DdiAllocInfo;
            VBOXWDDM_ALLOCINFO AllocInfo;
        } Buf;
        memset(&Buf, 0, sizeof (Buf));
        Buf.DdiAlloc.hResource = NULL;
        Buf.DdiAlloc.hKMResource = NULL;
        Buf.DdiAlloc.NumAllocations = 1;
        Buf.DdiAlloc.pAllocationInfo = &Buf.DdiAllocInfo;
        Buf.DdiAllocInfo.pPrivateDriverData = &Buf.AllocInfo;
        Buf.DdiAllocInfo.PrivateDriverDataSize = sizeof (Buf.AllocInfo);
        Buf.AllocInfo.enmType = VBOXWDDM_ALLOC_TYPE_UMD_HGSMI_BUFFER;
        Buf.AllocInfo.cbBuffer = cbBuf;
        Buf.AllocInfo.hSynch = hSynch;
        Buf.AllocInfo.enmSynchType = enmSynchType;

        HRESULT hr = pPrivate->pDevice->RtCallbacks.pfnAllocateCb(pPrivate->pDevice->hDevice, &Buf.DdiAlloc);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            Assert(Buf.DdiAllocInfo.hAllocation);
            pBuf->Base.pfnLock = vboxUhgsmiD3DBufferLock;
            pBuf->Base.pfnUnlock = vboxUhgsmiD3DBufferUnlock;
//            pBuf->Base.pfnAdjustValidDataRange = vboxUhgsmiD3DBufferAdjustValidDataRange;
            pBuf->Base.pfnDestroy = vboxUhgsmiD3DBufferDestroy;

            pBuf->Base.hSynch = hSynch;
            pBuf->Base.enmSynchType =enmSynchType;
            pBuf->Base.cbBuffer = cbBuf;
            pBuf->Base.bSynchCreated = bSynchCreated;

            pBuf->pDevice = pPrivate->pDevice;
            pBuf->hAllocation = Buf.DdiAllocInfo.hAllocation;

            *ppBuf = &pBuf->Base;

            return VINF_SUCCESS;
        }

        RTMemFree(pBuf);
    }
    else
        rc = VERR_NO_MEMORY;

    if (bSynchCreated)
        CloseHandle(hSynch);

    return rc;
}

DECLCALLBACK(int) vboxUhgsmiD3DBufferSubmitAsynch(PVBOXUHGSMI pHgsmi, PVBOXUHGSMI_BUFFER_SUBMIT aBuffers, uint32_t cBuffers)
{
    PVBOXUHGSMI_PRIVATE_D3D pHg = VBOXUHGSMID3D_GET(pHgsmi);
    PVBOXWDDMDISP_DEVICE pDevice = pHg->pDevice;
    const uint32_t cbDmaCmd = RT_OFFSETOF(VBOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD, aBufInfos[cBuffers]);
    if (pDevice->DefaultContext.ContextInfo.CommandBufferSize < cbDmaCmd)
    {
        Assert(0);
        return VERR_GENERAL_FAILURE;
    }
    if (pDevice->DefaultContext.ContextInfo.AllocationListSize < cBuffers)
    {
        Assert(0);
        return VERR_GENERAL_FAILURE;
    }

    PVBOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD pHdr = (PVBOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD)pDevice->DefaultContext.ContextInfo.pCommandBuffer;
    pHdr->Base.enmCmd = VBOXVDMACMD_TYPE_CHROMIUM_CMD;
    pHdr->Base.u32CmdReserved = cBuffers;

    D3DDDI_ALLOCATIONLIST* pAllocationList = pDevice->DefaultContext.ContextInfo.pAllocationList;
    PVBOXWDDM_UHGSMI_BUFFER_UI_SUBMIT_INFO pBufSubmInfo = pHdr->aBufInfos;

    for (uint32_t i = 0; i < cBuffers; ++i)
    {
        PVBOXUHGSMI_BUFFER_SUBMIT pBufInfo = &aBuffers[i];
        PVBOXUHGSMI_BUFFER_PRIVATE_D3D pBuffer = VBOXUHGSMID3D_GET_BUFFER(pBufInfo->pBuf);

        memset(pAllocationList, 0, sizeof (D3DDDI_ALLOCATIONLIST));
        pAllocationList->hAllocation = pBuffer->hAllocation;
        pAllocationList->WriteOperation = !pBufInfo->fFlags.bHostReadOnly;
        pAllocationList->DoNotRetireInstance = pBufInfo->fFlags.bDoNotRetire;
        pBufSubmInfo->fSubFlags = pBufInfo->fFlags;
        if (pBufInfo->fFlags.bEntireBuffer)
        {
            pBufSubmInfo->offData = 0;
            pBufSubmInfo->cbData = pBuffer->Base.cbBuffer;
        }
        else
        {
            pBufSubmInfo->offData = pBufInfo->offData;
            pBufSubmInfo->cbData = pBufInfo->cbData;
        }

        ++pAllocationList;
        ++pBufSubmInfo;
    }


    D3DDDICB_RENDER DdiRender = {0};
    DdiRender.CommandLength = cbDmaCmd;
    Assert(DdiRender.CommandLength);
    Assert(DdiRender.CommandLength < UINT32_MAX/2);
    DdiRender.CommandOffset = 0;
    DdiRender.NumAllocations = cBuffers;
    DdiRender.NumPatchLocations = 0;
//    DdiRender.NewCommandBufferSize = sizeof (VBOXVDMACMD) + 4 * (100);
//    DdiRender.NewAllocationListSize = 100;
//    DdiRender.NewPatchLocationListSize = 100;
    DdiRender.hContext = pDevice->DefaultContext.ContextInfo.hContext;

    HRESULT hr = pDevice->RtCallbacks.pfnRenderCb(pDevice->hDevice, &DdiRender);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        pDevice->DefaultContext.ContextInfo.CommandBufferSize = DdiRender.NewCommandBufferSize;
        pDevice->DefaultContext.ContextInfo.pCommandBuffer = DdiRender.pNewCommandBuffer;
        pDevice->DefaultContext.ContextInfo.AllocationListSize = DdiRender.NewAllocationListSize;
        pDevice->DefaultContext.ContextInfo.pAllocationList = DdiRender.pNewAllocationList;
        pDevice->DefaultContext.ContextInfo.PatchLocationListSize = DdiRender.NewPatchLocationListSize;
        pDevice->DefaultContext.ContextInfo.pPatchLocationList = DdiRender.pNewPatchLocationList;

        return VINF_SUCCESS;
    }

    return VERR_GENERAL_FAILURE;
}

HRESULT vboxUhgsmiD3DInit(PVBOXUHGSMI_PRIVATE_D3D pHgsmi, PVBOXWDDMDISP_DEVICE pDevice)
{
    pHgsmi->Base.pfnBufferCreate = vboxUhgsmiD3DBufferCreate;
    pHgsmi->Base.pfnBufferSubmitAsynch = vboxUhgsmiD3DBufferSubmitAsynch;
    pHgsmi->pDevice = pDevice;
    return S_OK;
}
