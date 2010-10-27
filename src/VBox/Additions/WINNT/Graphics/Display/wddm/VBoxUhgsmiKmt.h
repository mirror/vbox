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
#ifndef ___VBoxUhgsmiKmt_h__
#define ___VBoxUhgsmiKmt_h__

#include "VBoxUhgsmiBase.h"
#include "VBoxDispD3DCmn.h"

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

HRESULT vboxDispKmtCallbacksInit(PVBOXDISPKMT_CALLBACKS pCallbacks);
HRESULT vboxDispKmtCallbacksTerm(PVBOXDISPKMT_CALLBACKS pCallbacks);

typedef struct VBOXDISPKMT_ADAPTER
{
    D3DKMT_HANDLE hAdapter;
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

typedef struct VBOXUHGSMI_PRIVATE_KMT
{
    VBOXUHGSMI_PRIVATE_BASE BasePrivate;
    VBOXDISPKMT_CALLBACKS Callbacks;
    VBOXDISPKMT_ADAPTER Adapter;
    VBOXDISPKMT_DEVICE Device;
    VBOXDISPKMT_CONTEXT Context;
} VBOXUHGSMI_PRIVATE_KMT, *PVBOXUHGSMI_PRIVATE_KMT;

#define VBOXUHGSMIKMT_GET_PRIVATE(_p, _t) ((_t*)(((uint8_t*)_p) - RT_OFFSETOF(_t, BasePrivate.Base)))
#define VBOXUHGSMIKMT_GET(_p) VBOXUHGSMIKMT_GET_PRIVATE(_p, VBOXUHGSMI_PRIVATE_KMT)

HRESULT vboxDispKmtOpenAdapter(PVBOXDISPKMT_CALLBACKS pCallbacks, PVBOXDISPKMT_ADAPTER pAdapter);
HRESULT vboxDispKmtCloseAdapter(PVBOXDISPKMT_ADAPTER pAdapter);
HRESULT vboxDispKmtCreateDevice(PVBOXDISPKMT_ADAPTER pAdapter, PVBOXDISPKMT_DEVICE pDevice);
HRESULT vboxDispKmtDestroyDevice(PVBOXDISPKMT_DEVICE pDevice);
HRESULT vboxDispKmtCreateContext(PVBOXDISPKMT_DEVICE pDevice, PVBOXDISPKMT_CONTEXT pContext, BOOL bD3D);
HRESULT vboxDispKmtDestroyContext(PVBOXDISPKMT_CONTEXT pContext);

HRESULT vboxUhgsmiKmtCreate(PVBOXUHGSMI_PRIVATE_KMT pHgsmi, BOOL bD3D);
HRESULT vboxUhgsmiKmtDestroy(PVBOXUHGSMI_PRIVATE_KMT pHgsmi);

HRESULT vboxUhgsmiKmtEscCreate(PVBOXUHGSMI_PRIVATE_KMT pHgsmi, BOOL bD3D);


#endif /* #ifndef ___VBoxUhgsmiKmt_h__ */
