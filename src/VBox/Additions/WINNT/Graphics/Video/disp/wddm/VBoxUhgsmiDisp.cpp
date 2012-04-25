/* $Id$ */

/** @file
 * VBoxVideo Display D3D User mode dll
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

#include "VBoxDispD3DCmn.h"

#include <iprt/mem.h>
#include <iprt/err.h>

typedef struct VBOXUHGSMI_BUFFER_PRIVATE_D3D
{
    VBOXUHGSMI_BUFFER_PRIVATE_BASE BasePrivate;
    PVBOXWDDMDISP_DEVICE pDevice;
    UINT aLockPageIndices[1];
} VBOXUHGSMI_BUFFER_PRIVATE_D3D, *PVBOXUHGSMI_BUFFER_PRIVATE_D3D;

#define VBOXUHGSMID3D_GET_PRIVATE(_p, _t) ((_t*)(((uint8_t*)_p) - RT_OFFSETOF(_t, BasePrivate.Base)))
#define VBOXUHGSMID3D_GET(_p) VBOXUHGSMID3D_GET_PRIVATE(_p, VBOXUHGSMI_PRIVATE_D3D)
#define VBOXUHGSMID3D_GET_BUFFER(_p) VBOXUHGSMID3D_GET_PRIVATE(_p, VBOXUHGSMI_BUFFER_PRIVATE_D3D)

DECLCALLBACK(int) vboxUhgsmiD3DBufferDestroy(PVBOXUHGSMI_BUFFER pBuf)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_D3D pBuffer = VBOXUHGSMID3D_GET_BUFFER(pBuf);
    D3DDDICB_DEALLOCATE DdiDealloc;
    DdiDealloc.hResource = 0;
    DdiDealloc.NumAllocations = 1;
    DdiDealloc.HandleList = &pBuffer->BasePrivate.hAllocation;
    HRESULT hr = pBuffer->pDevice->RtCallbacks.pfnDeallocateCb(pBuffer->pDevice->hDevice, &DdiDealloc);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        if (pBuffer->BasePrivate.hSynch)
            CloseHandle(pBuffer->BasePrivate.hSynch);
        RTMemFree(pBuffer);
        return VINF_SUCCESS;
    }
    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiD3DBufferLock(PVBOXUHGSMI_BUFFER pBuf, uint32_t offLock, uint32_t cbLock, VBOXUHGSMI_BUFFER_LOCK_FLAGS fFlags, void**pvLock)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_D3D pBuffer = VBOXUHGSMID3D_GET_BUFFER(pBuf);
    D3DDDICB_LOCK DdiLock = {0};
    DdiLock.hAllocation = pBuffer->BasePrivate.hAllocation;
    DdiLock.PrivateDriverData = 0;

    int rc = vboxUhgsmiBaseLockData(pBuf, offLock, cbLock, fFlags,
                                         &DdiLock.Flags, &DdiLock.NumPages, pBuffer->aLockPageIndices);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    if (DdiLock.NumPages)
        DdiLock.pPages = pBuffer->aLockPageIndices;
    else
        DdiLock.pPages = NULL;

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
    DdiUnlock.phAllocations = &pBuffer->BasePrivate.hAllocation;
    HRESULT hr = pBuffer->pDevice->RtCallbacks.pfnUnlockCb(pBuffer->pDevice->hDevice, &DdiUnlock);
    Assert(hr == S_OK);
    if (hr == S_OK)
        return VINF_SUCCESS;
    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiD3DBufferCreate(PVBOXUHGSMI pHgsmi, uint32_t cbBuf, VBOXUHGSMI_BUFFER_TYPE_FLAGS fUhgsmiType, PVBOXUHGSMI_BUFFER* ppBuf)
{
    HANDLE hSynch = NULL;
    if (!cbBuf)
        return VERR_INVALID_PARAMETER;

    int rc = vboxUhgsmiBaseEventChkCreate(fUhgsmiType, &hSynch);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    cbBuf = VBOXWDDM_ROUNDBOUND(cbBuf, 0x1000);
    Assert(cbBuf);
    uint32_t cPages = cbBuf >> 12;
    Assert(cPages);

    PVBOXUHGSMI_PRIVATE_D3D pPrivate = VBOXUHGSMID3D_GET(pHgsmi);
    PVBOXUHGSMI_BUFFER_PRIVATE_D3D pBuf = (PVBOXUHGSMI_BUFFER_PRIVATE_D3D)RTMemAllocZ(RT_OFFSETOF(VBOXUHGSMI_BUFFER_PRIVATE_D3D, aLockPageIndices[cPages]));
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
        Buf.AllocInfo.fUhgsmiType = fUhgsmiType;

        HRESULT hr = pPrivate->pDevice->RtCallbacks.pfnAllocateCb(pPrivate->pDevice->hDevice, &Buf.DdiAlloc);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            Assert(Buf.DdiAllocInfo.hAllocation);
            pBuf->BasePrivate.Base.pfnLock = vboxUhgsmiD3DBufferLock;
            pBuf->BasePrivate.Base.pfnUnlock = vboxUhgsmiD3DBufferUnlock;
//            pBuf->Base.pfnAdjustValidDataRange = vboxUhgsmiD3DBufferAdjustValidDataRange;
            pBuf->BasePrivate.Base.pfnDestroy = vboxUhgsmiD3DBufferDestroy;

            pBuf->BasePrivate.Base.fType = fUhgsmiType;
            pBuf->BasePrivate.Base.cbBuffer = cbBuf;

            pBuf->pDevice = pPrivate->pDevice;
            pBuf->BasePrivate.hAllocation = Buf.DdiAllocInfo.hAllocation;

            *ppBuf = &pBuf->BasePrivate.Base;

            return VINF_SUCCESS;
        }

        RTMemFree(pBuf);
    }
    else
        rc = VERR_NO_MEMORY;

    if (hSynch)
        CloseHandle(hSynch);

    return rc;
}

DECLCALLBACK(int) vboxUhgsmiD3DBufferSubmit(PVBOXUHGSMI pHgsmi, PVBOXUHGSMI_BUFFER_SUBMIT aBuffers, uint32_t cBuffers)
{
    PVBOXUHGSMI_PRIVATE_D3D pHg = VBOXUHGSMID3D_GET(pHgsmi);
    PVBOXWDDMDISP_DEVICE pDevice = pHg->pDevice;
    UINT cbDmaCmd = pDevice->DefaultContext.ContextInfo.CommandBufferSize;
    int rc = vboxUhgsmiBaseDmaFill(aBuffers, cBuffers,
            pDevice->DefaultContext.ContextInfo.pCommandBuffer, &cbDmaCmd,
            pDevice->DefaultContext.ContextInfo.pAllocationList, pDevice->DefaultContext.ContextInfo.AllocationListSize,
            pDevice->DefaultContext.ContextInfo.pPatchLocationList, pDevice->DefaultContext.ContextInfo.PatchLocationListSize);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

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
    pHgsmi->BasePrivate.Base.pfnBufferCreate = vboxUhgsmiD3DBufferCreate;
    pHgsmi->BasePrivate.Base.pfnBufferSubmit = vboxUhgsmiD3DBufferSubmit;
    pHgsmi->BasePrivate.hClient = NULL;
    pHgsmi->pDevice = pDevice;
    return S_OK;
}

