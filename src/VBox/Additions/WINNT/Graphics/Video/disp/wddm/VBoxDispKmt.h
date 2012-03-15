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

#ifndef ___VBoxDispKmt_h__
#define ___VBoxDispKmt_h__

#include <D3dkmthk.h>

typedef struct VBOXDISPKMT_CALLBACKS
{
    HMODULE hGdi32;
    /* open adapter */
    PFND3DKMT_OPENADAPTERFROMHDC pfnD3DKMTOpenAdapterFromHdc;
    PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME pfnD3DKMTOpenAdapterFromGdiDisplayName;
    /* close adapter */
    PFND3DKMT_CLOSEADAPTER pfnD3DKMTCloseAdapter;
    /* escape */
    PFND3DKMT_ESCAPE pfnD3DKMTEscape;

    PFND3DKMT_CREATEDEVICE pfnD3DKMTCreateDevice;
    PFND3DKMT_DESTROYDEVICE pfnD3DKMTDestroyDevice;
    PFND3DKMT_CREATECONTEXT pfnD3DKMTCreateContext;
    PFND3DKMT_DESTROYCONTEXT pfnD3DKMTDestroyContext;

    PFND3DKMT_RENDER pfnD3DKMTRender;

    PFND3DKMT_CREATEALLOCATION pfnD3DKMTCreateAllocation;
    PFND3DKMT_DESTROYALLOCATION pfnD3DKMTDestroyAllocation;

    PFND3DKMT_LOCK pfnD3DKMTLock;
    PFND3DKMT_UNLOCK pfnD3DKMTUnlock;
} VBOXDISPKMT_CALLBACKS, *PVBOXDISPKMT_CALLBACKS;

typedef struct VBOXDISPKMT_ADAPTER
{
    D3DKMT_HANDLE hAdapter;
    HDC hDc;
    PVBOXDISPKMT_CALLBACKS pCallbacks;
}VBOXDISPKMT_ADAPTER, *PVBOXDISPKMT_ADAPTER;

typedef struct VBOXDISPKMT_DEVICE
{
    struct VBOXDISPKMT_ADAPTER *pAdapter;
    D3DKMT_HANDLE hDevice;
    VOID *pCommandBuffer;
    UINT CommandBufferSize;
    D3DDDI_ALLOCATIONLIST *pAllocationList;
    UINT AllocationListSize;
    D3DDDI_PATCHLOCATIONLIST *pPatchLocationList;
    UINT PatchLocationListSize;
}VBOXDISPKMT_DEVICE, *PVBOXDISPKMT_DEVICE;

typedef struct VBOXDISPKMT_CONTEXT
{
    struct VBOXDISPKMT_DEVICE *pDevice;
    D3DKMT_HANDLE hContext;
    VOID *pCommandBuffer;
    UINT CommandBufferSize;
    D3DDDI_ALLOCATIONLIST *pAllocationList;
    UINT AllocationListSize;
    D3DDDI_PATCHLOCATIONLIST *pPatchLocationList;
    UINT PatchLocationListSize;
} VBOXDISPKMT_CONTEXT, *PVBOXDISPKMT_CONTEXT;

HRESULT vboxDispKmtCallbacksInit(PVBOXDISPKMT_CALLBACKS pCallbacks);
HRESULT vboxDispKmtCallbacksTerm(PVBOXDISPKMT_CALLBACKS pCallbacks);

HRESULT vboxDispKmtOpenAdapter(PVBOXDISPKMT_CALLBACKS pCallbacks, PVBOXDISPKMT_ADAPTER pAdapter);
HRESULT vboxDispKmtCloseAdapter(PVBOXDISPKMT_ADAPTER pAdapter);
HRESULT vboxDispKmtCreateDevice(PVBOXDISPKMT_ADAPTER pAdapter, PVBOXDISPKMT_DEVICE pDevice);
HRESULT vboxDispKmtDestroyDevice(PVBOXDISPKMT_DEVICE pDevice);
HRESULT vboxDispKmtCreateContext(PVBOXDISPKMT_DEVICE pDevice, PVBOXDISPKMT_CONTEXT pContext,
        VBOXWDDM_CONTEXT_TYPE enmType,
        uint32_t crVersionMajor, uint32_t crVersionMinor,
        HANDLE hEvent, uint64_t u64UmInfo);
HRESULT vboxDispKmtDestroyContext(PVBOXDISPKMT_CONTEXT pContext);


#endif /* #ifndef ___VBoxDispKmt_h__ */
