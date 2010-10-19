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

typedef struct VBOXUHGSMI_BUFFER_PRIVATE_KMT
{
    VBOXUHGSMI_BUFFER_PRIVATE_BASE BasePrivate;
    PVBOXUHGSMI_PRIVATE_KMT pHgsmi;
    CRITICAL_SECTION CritSect;
    UINT aLockPageIndices[1];
} VBOXUHGSMI_BUFFER_PRIVATE_KMT, *PVBOXUHGSMI_BUFFER_PRIVATE_KMT;

DECLCALLBACK(int) vboxUhgsmiKmtBufferDestroy(PVBOXUHGSMI_BUFFER pBuf)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_KMT pBuffer = VBOXUHGSMIKMT_GET_BUFFER(pBuf);
    D3DKMT_DESTROYALLOCATION DdiDealloc;
    DdiDealloc.hDevice = pBuffer->pHgsmi->Device.hDevice;
    DdiDealloc.hResource = NULL;
    DdiDealloc.phAllocationList = &pBuffer->BasePrivate.hAllocation;
    DdiDealloc.AllocationCount = 1;
    NTSTATUS Status = pBuffer->pHgsmi->Callbacks.pfnD3DKMTDestroyAllocation(&DdiDealloc);
    Assert(!Status);
    if (!Status)
    {
        if (pBuffer->BasePrivate.Base.bSynchCreated)
        {
            CloseHandle(pBuffer->BasePrivate.Base.hSynch);
        }
        RTMemFree(pBuffer);
        return VINF_SUCCESS;
    }
    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiKmtBufferLock(PVBOXUHGSMI_BUFFER pBuf, uint32_t offLock, uint32_t cbLock, VBOXUHGSMI_BUFFER_LOCK_FLAGS fFlags, void**pvLock)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_KMT pBuffer = VBOXUHGSMIKMT_GET_BUFFER(pBuf);
    D3DKMT_LOCK DdiLock = {0};
    DdiLock.hDevice = pBuffer->pHgsmi->Device.hDevice;
    DdiLock.hAllocation = pBuffer->BasePrivate.hAllocation;
    DdiLock.PrivateDriverData = NULL;

    EnterCriticalSection(&pBuffer->CritSect);

    int rc = vboxUhgsmiBaseLockData(pBuf, offLock, cbLock, fFlags,
                                         &DdiLock.Flags, &DdiLock.NumPages, pBuffer->aLockPageIndices);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    if (DdiLock.NumPages)
        DdiLock.pPages = pBuffer->aLockPageIndices;
    else
        DdiLock.pPages = NULL;

    NTSTATUS Status = pBuffer->pHgsmi->Callbacks.pfnD3DKMTLock(&DdiLock);
    Assert(!Status);
    LeaveCriticalSection(&pBuffer->CritSect);
    if (!Status)
    {
        *pvLock = (void*)(((uint8_t*)DdiLock.pData) + (offLock & 0xfff));
        return VINF_SUCCESS;
    }
    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiKmtBufferUnlock(PVBOXUHGSMI_BUFFER pBuf)
{
    PVBOXUHGSMI_BUFFER_PRIVATE_KMT pBuffer = VBOXUHGSMIKMT_GET_BUFFER(pBuf);
    D3DKMT_UNLOCK DdiUnlock;

    DdiUnlock.hDevice = pBuffer->pHgsmi->Device.hDevice;
    DdiUnlock.NumAllocations = 1;
    DdiUnlock.phAllocations = &pBuffer->BasePrivate.hAllocation;
    NTSTATUS Status = pBuffer->pHgsmi->Callbacks.pfnD3DKMTUnlock(&DdiUnlock);
    Assert(!Status);
    if (!Status)
        return VINF_SUCCESS;
    return VERR_GENERAL_FAILURE;
}

DECLCALLBACK(int) vboxUhgsmiKmtBufferCreate(PVBOXUHGSMI pHgsmi, uint32_t cbBuf,
        VBOXUHGSMI_SYNCHOBJECT_TYPE enmSynchType, HVBOXUHGSMI_SYNCHOBJECT hSynch,
        PVBOXUHGSMI_BUFFER* ppBuf)
{
    bool bSynchCreated = false;
    if (!cbBuf)
        return VERR_INVALID_PARAMETER;

    int rc = vboxUhgsmiBaseEventChkCreate(enmSynchType, &hSynch, &bSynchCreated);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    cbBuf = VBOXWDDM_ROUNDBOUND(cbBuf, 0x1000);
    Assert(cbBuf);
    uint32_t cPages = cbBuf >> 12;
    Assert(cPages);

    PVBOXUHGSMI_PRIVATE_KMT pPrivate = VBOXUHGSMIKMT_GET(pHgsmi);
    PVBOXUHGSMI_BUFFER_PRIVATE_KMT pBuf = (PVBOXUHGSMI_BUFFER_PRIVATE_KMT)RTMemAllocZ(RT_OFFSETOF(VBOXUHGSMI_BUFFER_PRIVATE_KMT, aLockPageIndices[cPages]));
    Assert(pBuf);
    if (pBuf)
    {
        struct
        {
            D3DKMT_CREATEALLOCATION DdiAlloc;
            D3DDDI_ALLOCATIONINFO DdiAllocInfo;
            VBOXWDDM_ALLOCINFO AllocInfo;
        } Buf;
        memset(&Buf, 0, sizeof (Buf));
        Buf.DdiAlloc.hDevice = pPrivate->Device.hDevice;
        Buf.DdiAlloc.NumAllocations = 1;
        Buf.DdiAlloc.pAllocationInfo = &Buf.DdiAllocInfo;
        Buf.DdiAllocInfo.pPrivateDriverData = &Buf.AllocInfo;
        Buf.DdiAllocInfo.PrivateDriverDataSize = sizeof (Buf.AllocInfo);
        Buf.AllocInfo.enmType = VBOXWDDM_ALLOC_TYPE_UMD_HGSMI_BUFFER;
        Buf.AllocInfo.cbBuffer = cbBuf;
        Buf.AllocInfo.hSynch = hSynch;
        Buf.AllocInfo.enmSynchType = enmSynchType;

        HRESULT hr = pPrivate->Callbacks.pfnD3DKMTCreateAllocation(&Buf.DdiAlloc);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            InitializeCriticalSection(&pBuf->CritSect);

            Assert(Buf.DdiAllocInfo.hAllocation);
            pBuf->BasePrivate.Base.pfnLock = vboxUhgsmiKmtBufferLock;
            pBuf->BasePrivate.Base.pfnUnlock = vboxUhgsmiKmtBufferUnlock;
//            pBuf->Base.pfnAdjustValidDataRange = vboxUhgsmiKmtBufferAdjustValidDataRange;
            pBuf->BasePrivate.Base.pfnDestroy = vboxUhgsmiKmtBufferDestroy;

            pBuf->BasePrivate.Base.hSynch = hSynch;
            pBuf->BasePrivate.Base.enmSynchType = enmSynchType;
            pBuf->BasePrivate.Base.cbBuffer = cbBuf;
            pBuf->BasePrivate.Base.bSynchCreated = bSynchCreated;

            pBuf->pHgsmi = pPrivate;
            pBuf->BasePrivate.hAllocation = Buf.DdiAllocInfo.hAllocation;

            *ppBuf = &pBuf->BasePrivate.Base;

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

DECLCALLBACK(int) vboxUhgsmiKmtBufferSubmitAsynch(PVBOXUHGSMI pHgsmi, PVBOXUHGSMI_BUFFER_SUBMIT aBuffers, uint32_t cBuffers)
{
    PVBOXUHGSMI_PRIVATE_KMT pHg = VBOXUHGSMIKMT_GET(pHgsmi);
    UINT cbDmaCmd = pHg->Context.CommandBufferSize;
    int rc = vboxUhgsmiBaseDmaFill(aBuffers, cBuffers,
            pHg->Context.pCommandBuffer, &cbDmaCmd,
            pHg->Context.pAllocationList, pHg->Context.AllocationListSize,
            pHg->Context.pPatchLocationList, pHg->Context.PatchLocationListSize);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        return rc;

    D3DKMT_RENDER DdiRender = {0};
    DdiRender.hContext = pHg->Context.hContext;
    DdiRender.CommandLength = cbDmaCmd;
    DdiRender.AllocationCount = cBuffers;
    Assert(DdiRender.CommandLength);
    Assert(DdiRender.CommandLength < UINT32_MAX/2);

    HRESULT hr = pHg->Callbacks.pfnD3DKMTRender(&DdiRender);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        pHg->Context.CommandBufferSize = DdiRender.NewCommandBufferSize;
        pHg->Context.pCommandBuffer = DdiRender.pNewCommandBuffer;
        pHg->Context.AllocationListSize = DdiRender.NewAllocationListSize;
        pHg->Context.pAllocationList = DdiRender.pNewAllocationList;
        pHg->Context.PatchLocationListSize = DdiRender.NewPatchLocationListSize;
        pHg->Context.pPatchLocationList = DdiRender.pNewPatchLocationList;

        return VINF_SUCCESS;
    }

    return VERR_GENERAL_FAILURE;
}

HRESULT vboxUhgsmiKmtCreate(PVBOXUHGSMI_PRIVATE_KMT pHgsmi, BOOL bD3D)
{
    pHgsmi->BasePrivate.Base.pfnBufferCreate = vboxUhgsmiKmtBufferCreate;
    pHgsmi->BasePrivate.Base.pfnBufferSubmitAsynch = vboxUhgsmiKmtBufferSubmitAsynch;
    pHgsmi->BasePrivate.hClient = NULL;

    HRESULT hr = vboxDispKmtCallbacksInit(&pHgsmi->Callbacks);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        hr = vboxDispKmtOpenAdapter(&pHgsmi->Callbacks, &pHgsmi->Adapter);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            hr = vboxDispKmtCreateDevice(&pHgsmi->Adapter, &pHgsmi->Device);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                hr = vboxDispKmtCreateContext(&pHgsmi->Device, &pHgsmi->Context, bD3D);
                Assert(hr == S_OK);
                if (hr == S_OK)
                {
                    return S_OK;
                }
                vboxDispKmtDestroyDevice(&pHgsmi->Device);
            }
            vboxDispKmtCloseAdapter(&pHgsmi->Adapter);
        }
        vboxDispKmtCallbacksTerm(&pHgsmi->Callbacks);
    }
    return hr;
}


HRESULT vboxUhgsmiKmtDestroy(PVBOXUHGSMI_PRIVATE_KMT pHgsmi)
{
    Assert(0);
    HRESULT hr = vboxDispKmtDestroyContext(&pHgsmi->Context);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        hr = vboxDispKmtDestroyDevice(&pHgsmi->Device);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            hr = vboxDispKmtCloseAdapter(&pHgsmi->Adapter);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                hr = vboxDispKmtCallbacksTerm(&pHgsmi->Callbacks);
                Assert(hr == S_OK);
                if (hr == S_OK)
                    return S_OK;
            }
        }
    }
    return hr;
}

HRESULT vboxDispKmtCallbacksInit(PVBOXDISPKMT_CALLBACKS pCallbacks)
{
    HRESULT hr = S_OK;

    memset(pCallbacks, 0, sizeof (*pCallbacks));

    pCallbacks->hGdi32 = LoadLibraryW(L"gdi32.dll");
    if (pCallbacks->hGdi32 != NULL)
    {
        bool bSupported = true;
        pCallbacks->pfnD3DKMTOpenAdapterFromHdc = (PFND3DKMT_OPENADAPTERFROMHDC)GetProcAddress(pCallbacks->hGdi32, "D3DKMTOpenAdapterFromHdc");
        Log((__FUNCTION__"pfnD3DKMTOpenAdapterFromHdc = %p\n", pCallbacks->pfnD3DKMTOpenAdapterFromHdc));
        bSupported &= !!(pCallbacks->pfnD3DKMTOpenAdapterFromHdc);

        pCallbacks->pfnD3DKMTOpenAdapterFromGdiDisplayName = (PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME)GetProcAddress(pCallbacks->hGdi32, "D3DKMTOpenAdapterFromGdiDisplayName");
        Log((__FUNCTION__": pfnD3DKMTOpenAdapterFromGdiDisplayName = %p\n", pCallbacks->pfnD3DKMTOpenAdapterFromGdiDisplayName));
        bSupported &= !!(pCallbacks->pfnD3DKMTOpenAdapterFromGdiDisplayName);

        pCallbacks->pfnD3DKMTCloseAdapter = (PFND3DKMT_CLOSEADAPTER)GetProcAddress(pCallbacks->hGdi32, "D3DKMTCloseAdapter");
        Log((__FUNCTION__": pfnD3DKMTCloseAdapter = %p\n", pCallbacks->pfnD3DKMTCloseAdapter));
        bSupported &= !!(pCallbacks->pfnD3DKMTCloseAdapter);

        pCallbacks->pfnD3DKMTEscape = (PFND3DKMT_ESCAPE)GetProcAddress(pCallbacks->hGdi32, "D3DKMTEscape");
        Log((__FUNCTION__": pfnD3DKMTEscape = %p\n", pCallbacks->pfnD3DKMTEscape));
        bSupported &= !!(pCallbacks->pfnD3DKMTEscape);

        pCallbacks->pfnD3DKMTCreateDevice = (PFND3DKMT_CREATEDEVICE)GetProcAddress(pCallbacks->hGdi32, "D3DKMTCreateDevice");
        Log((__FUNCTION__": pfnD3DKMTCreateDevice = %p\n", pCallbacks->pfnD3DKMTCreateDevice));
        bSupported &= !!(pCallbacks->pfnD3DKMTCreateDevice);

        pCallbacks->pfnD3DKMTDestroyDevice = (PFND3DKMT_DESTROYDEVICE)GetProcAddress(pCallbacks->hGdi32, "D3DKMTDestroyDevice");
        Log((__FUNCTION__": pfnD3DKMTDestroyDevice = %p\n", pCallbacks->pfnD3DKMTDestroyDevice));
        bSupported &= !!(pCallbacks->pfnD3DKMTDestroyDevice);

        pCallbacks->pfnD3DKMTCreateContext = (PFND3DKMT_CREATECONTEXT)GetProcAddress(pCallbacks->hGdi32, "D3DKMTCreateContext");
        Log((__FUNCTION__": pfnD3DKMTCreateContext = %p\n", pCallbacks->pfnD3DKMTCreateContext));
        bSupported &= !!(pCallbacks->pfnD3DKMTCreateContext);

        pCallbacks->pfnD3DKMTDestroyContext = (PFND3DKMT_DESTROYCONTEXT)GetProcAddress(pCallbacks->hGdi32, "D3DKMTDestroyContext");
        Log((__FUNCTION__": pfnD3DKMTDestroyContext = %p\n", pCallbacks->pfnD3DKMTDestroyContext));
        bSupported &= !!(pCallbacks->pfnD3DKMTDestroyContext);

        pCallbacks->pfnD3DKMTRender = (PFND3DKMT_RENDER)GetProcAddress(pCallbacks->hGdi32, "D3DKMTRender");
        Log((__FUNCTION__": pfnD3DKMTRender = %p\n", pCallbacks->pfnD3DKMTRender));
        bSupported &= !!(pCallbacks->pfnD3DKMTRender);

        pCallbacks->pfnD3DKMTCreateAllocation = (PFND3DKMT_CREATEALLOCATION)GetProcAddress(pCallbacks->hGdi32, "D3DKMTCreateAllocation");
        Log((__FUNCTION__": pfnD3DKMTCreateAllocation = %p\n", pCallbacks->pfnD3DKMTCreateAllocation));
        bSupported &= !!(pCallbacks->pfnD3DKMTCreateAllocation);

        pCallbacks->pfnD3DKMTDestroyAllocation = (PFND3DKMT_DESTROYALLOCATION)GetProcAddress(pCallbacks->hGdi32, "D3DKMTDestroyAllocation");
        Log((__FUNCTION__": pfnD3DKMTDestroyAllocation = %p\n", pCallbacks->pfnD3DKMTDestroyAllocation));
        bSupported &= !!(pCallbacks->pfnD3DKMTDestroyAllocation);

        pCallbacks->pfnD3DKMTLock = (PFND3DKMT_LOCK)GetProcAddress(pCallbacks->hGdi32, "D3DKMTLock");
        Log((__FUNCTION__": pfnD3DKMTLock = %p\n", pCallbacks->pfnD3DKMTLock));
        bSupported &= !!(pCallbacks->pfnD3DKMTLock);

        pCallbacks->pfnD3DKMTUnlock = (PFND3DKMT_UNLOCK)GetProcAddress(pCallbacks->hGdi32, "D3DKMTUnlock");
        Log((__FUNCTION__": pfnD3DKMTUnlock = %p\n", pCallbacks->pfnD3DKMTUnlock));
        bSupported &= !!(pCallbacks->pfnD3DKMTUnlock);

        Assert(bSupported);
        if (bSupported)
        {
            return S_OK;
        }
        else
        {
            Log((__FUNCTION__": one of pfnD3DKMT function pointers failed to initialize\n"));
            hr = E_NOINTERFACE;
        }

        FreeLibrary(pCallbacks->hGdi32);
    }
    else
    {
        DWORD winEr = GetLastError();
        hr = HRESULT_FROM_WIN32(winEr);
        Assert(0);
        Assert(hr != S_OK);
        Assert(hr != S_FALSE);
        if (hr == S_OK || hr == S_FALSE)
            hr = E_FAIL;
    }

    return hr;
}

HRESULT vboxDispKmtCallbacksTerm(PVBOXDISPKMT_CALLBACKS pCallbacks)
{
    FreeLibrary(pCallbacks->hGdi32);
    return S_OK;
}

HRESULT vboxDispKmtOpenAdapter(PVBOXDISPKMT_CALLBACKS pCallbacks, PVBOXDISPKMT_ADAPTER pAdapter)
{
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME OpenAdapterData = {0};
    wcsncpy(OpenAdapterData.DeviceName, L"\\\\.\\DISPLAY1", RT_ELEMENTS(OpenAdapterData.DeviceName) - 1 /* the last one is always \0 */);
    HRESULT hr = S_OK;
    NTSTATUS Status = pCallbacks->pfnD3DKMTOpenAdapterFromGdiDisplayName(&OpenAdapterData);
    Assert(!Status);
    if (!Status)
    {
        pAdapter->hAdapter = OpenAdapterData.hAdapter;
        pAdapter->pCallbacks = pCallbacks;
        return S_OK;
    }
    else
    {
        Log((__FUNCTION__": pfnD3DKMTOpenAdapterFromGdiDisplayName failed, Status (0x%x)\n", Status));
        hr = E_FAIL;
    }

    return hr;

}

HRESULT vboxDispKmtCloseAdapter(PVBOXDISPKMT_ADAPTER pAdapter)
{
    D3DKMT_CLOSEADAPTER ClosaAdapterData = {0};
    ClosaAdapterData.hAdapter = pAdapter->hAdapter;
    NTSTATUS Status = pAdapter->pCallbacks->pfnD3DKMTCloseAdapter(&ClosaAdapterData);
    Assert(!Status);
    if (!Status)
    {
        return S_OK;
    }

    Log((__FUNCTION__": pfnD3DKMTCloseAdapter failed, Status (0x%x)\n", Status));
    /* ignore */
    Status = 0;
    return E_FAIL;
}

HRESULT vboxDispKmtCreateDevice(PVBOXDISPKMT_ADAPTER pAdapter, PVBOXDISPKMT_DEVICE pDevice)
{
    D3DKMT_CREATEDEVICE CreateDeviceData = {0};
    CreateDeviceData.hAdapter = pAdapter->hAdapter;
    NTSTATUS Status = pAdapter->pCallbacks->pfnD3DKMTCreateDevice(&CreateDeviceData);
    Assert(!Status);
    if (!Status)
    {
        pDevice->pAdapter = pAdapter;
        pDevice->hDevice = CreateDeviceData.hDevice;
        pDevice->pCommandBuffer = CreateDeviceData.pCommandBuffer;
        pDevice->CommandBufferSize = CreateDeviceData.CommandBufferSize;
        pDevice->pAllocationList = CreateDeviceData.pAllocationList;
        pDevice->AllocationListSize = CreateDeviceData.AllocationListSize;
        pDevice->pPatchLocationList = CreateDeviceData.pPatchLocationList;
        pDevice->PatchLocationListSize = CreateDeviceData.PatchLocationListSize;

        return S_OK;
    }

    return E_FAIL;
}

HRESULT vboxDispKmtDestroyDevice(PVBOXDISPKMT_DEVICE pDevice)
{
    D3DKMT_DESTROYDEVICE DestroyDeviceData = {0};
    DestroyDeviceData.hDevice = pDevice->hDevice;
    NTSTATUS Status = pDevice->pAdapter->pCallbacks->pfnD3DKMTDestroyDevice(&DestroyDeviceData);
    Assert(!Status);
    if (!Status)
    {
        return S_OK;
    }
    return E_FAIL;
}

HRESULT vboxDispKmtCreateContext(PVBOXDISPKMT_DEVICE pDevice, PVBOXDISPKMT_CONTEXT pContext, BOOL bD3D)
{
    VBOXWDDM_CREATECONTEXT_INFO Info = {0};
    Info.u32IfVersion = 9;
    Info.enmType = bD3D ? VBOXWDDM_CONTEXT_TYPE_CUSTOM_UHGSMI_3D : VBOXWDDM_CONTEXT_TYPE_CUSTOM_UHGSMI_GL;
    D3DKMT_CREATECONTEXT ContextData = {0};
    ContextData.hDevice = pDevice->hDevice;
    ContextData.NodeOrdinal = 0;
    ContextData.EngineAffinity = 0;
    ContextData.pPrivateDriverData = &Info;
    ContextData.PrivateDriverDataSize = sizeof (Info);
    ContextData.ClientHint = bD3D ? D3DKMT_CLIENTHINT_DX9 : D3DKMT_CLIENTHINT_OPENGL;
    NTSTATUS Status = pDevice->pAdapter->pCallbacks->pfnD3DKMTCreateContext(&ContextData);
    Assert(!Status);
    if (!Status)
    {
        pContext->pDevice = pDevice;
        pContext->hContext = ContextData.hContext;
        pContext->pCommandBuffer = ContextData.pCommandBuffer;
        pContext->CommandBufferSize = ContextData.CommandBufferSize;
        pContext->pAllocationList = ContextData.pAllocationList;
        pContext->AllocationListSize = ContextData.AllocationListSize;
        pContext->pPatchLocationList = ContextData.pPatchLocationList;
        pContext->PatchLocationListSize = ContextData.PatchLocationListSize;
        return S_OK;
    }
    return E_FAIL;
}

HRESULT vboxDispKmtDestroyContext(PVBOXDISPKMT_CONTEXT pContext)
{
    D3DKMT_DESTROYCONTEXT DestroyContextData = {0};
    DestroyContextData.hContext = pContext->hContext;
    NTSTATUS Status = pContext->pDevice->pAdapter->pCallbacks->pfnD3DKMTDestroyContext(&DestroyContextData);
    Assert(!Status);
    if (!Status)
        return S_OK;
    return E_FAIL;
}
