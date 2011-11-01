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

        /*Assert(bSupported);*/
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

HRESULT vboxDispKmtAdpHdcCreate(HDC *phDc)
{
    HRESULT hr = E_FAIL;
    DISPLAY_DEVICE DDev;
    memset(&DDev, 0, sizeof (DDev));
    DDev.cb = sizeof (DDev);

    *phDc = NULL;

    for (int i = 0; ; ++i)
    {
        if (EnumDisplayDevices(NULL, /* LPCTSTR lpDevice */ i, /* DWORD iDevNum */
                &DDev, 0 /* DWORD dwFlags*/))
        {
            if (DDev.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
            {
                HDC hDc = CreateDC(NULL, DDev.DeviceName, NULL, NULL);
                if (hDc)
                {
                    *phDc = hDc;
                    return S_OK;
                }
                else
                {
                    DWORD winEr = GetLastError();
                    Assert(0);
                    hr = HRESULT_FROM_WIN32(winEr);
                    Assert(FAILED(hr));
                    break;
                }
            }
        }
        else
        {
            DWORD winEr = GetLastError();
//            BP_WARN();
            hr = HRESULT_FROM_WIN32(winEr);
#ifdef DEBUG_misha
            Assert(FAILED(hr));
#endif
            if (!FAILED(hr))
            {
                hr = E_FAIL;
            }
            break;
        }
    }

    return hr;
}

HRESULT vboxDispKmtOpenAdapter(PVBOXDISPKMT_CALLBACKS pCallbacks, PVBOXDISPKMT_ADAPTER pAdapter)
{
    D3DKMT_OPENADAPTERFROMHDC OpenAdapterData = {0};
    HRESULT hr = vboxDispKmtAdpHdcCreate(&OpenAdapterData.hDc);
    if (hr == S_OK)
    {
        Assert(OpenAdapterData.hDc);
        NTSTATUS Status = pCallbacks->pfnD3DKMTOpenAdapterFromHdc(&OpenAdapterData);
#ifdef DEBUG_misha
        /* may fail with xpdm driver */
        Assert(!Status);
#endif
        if (!Status)
        {
            pAdapter->hAdapter = OpenAdapterData.hAdapter;
            pAdapter->hDc = OpenAdapterData.hDc;
            pAdapter->pCallbacks = pCallbacks;
            return S_OK;
        }
        else
        {
            Log((__FUNCTION__": pfnD3DKMTOpenAdapterFromGdiDisplayName failed, Status (0x%x)\n", Status));
            hr = E_FAIL;
        }

        DeleteDC(OpenAdapterData.hDc);
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
        DeleteDC(pAdapter->hDc);
        return S_OK;
    }

    Log((__FUNCTION__": pfnD3DKMTCloseAdapter failed, Status (0x%x)\n", Status));

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

HRESULT vboxDispKmtCreateContext(PVBOXDISPKMT_DEVICE pDevice, PVBOXDISPKMT_CONTEXT pContext,
                                    VBOXWDDM_CONTEXT_TYPE enmType, HANDLE hEvent, uint64_t u64UmInfo)
{
    VBOXWDDM_CREATECONTEXT_INFO Info = {0};
    Info.u32IfVersion = 9;
    Info.enmType = enmType;
    Info.hUmEvent = (uint64_t)hEvent;
    Info.u64UmInfo = u64UmInfo;
    D3DKMT_CREATECONTEXT ContextData = {0};
    ContextData.hDevice = pDevice->hDevice;
    ContextData.NodeOrdinal = VBOXWDDM_NODE_ID_3D_KMT;
    ContextData.EngineAffinity = VBOXWDDM_ENGINE_ID_3D_KMT;
    ContextData.pPrivateDriverData = &Info;
    ContextData.PrivateDriverDataSize = sizeof (Info);
    ContextData.ClientHint = enmType == VBOXWDDM_CONTEXT_TYPE_CUSTOM_UHGSMI_GL ? D3DKMT_CLIENTHINT_OPENGL : D3DKMT_CLIENTHINT_DX9;
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
