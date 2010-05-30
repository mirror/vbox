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
#ifndef ___VBoxDispD3D_h___
#define ___VBoxDispD3D_h___

#include "VBoxDispD3DIf.h"
#include "../../Miniport/wddm/VBoxVideoIf.h"

#include <iprt/cdefs.h>

#ifdef VBOX_WITH_VIDEOHWACCEL
typedef struct VBOXDISPVHWA_INFO
{
    VBOXVHWA_INFO Settings;
}VBOXDISPVHWA_INFO;

/* represents settings secific to
 * display device (head) on the multiple-head graphics card
 * currently used for 2D (overlay) only since in theory its settings
 * can differ per each frontend's framebuffer. */
typedef struct VBOXWDDMDISP_HEAD
{
    VBOXDISPVHWA_INFO Vhwa;
} VBOXWDDMDISP_HEAD;
#endif

typedef struct VBOXWDDMDISP_ADAPTER
{
    HANDLE hAdapter;
    UINT uIfVersion;
    UINT uRtVersion;
    VBOXDISPD3D D3D;
    IDirect3D9Ex * pD3D9If;
    D3DDDI_ADAPTERCALLBACKS RtCallbacks;
    uint32_t cFormstOps;
    FORMATOP *paFormstOps;
    uint32_t cSurfDescs;
    DDSURFACEDESC *paSurfDescs;
#ifdef VBOX_WITH_VIDEOHWACCEL
    uint32_t cHeads;
    VBOXWDDMDISP_HEAD aHeads[1];
#endif
} VBOXWDDMDISP_ADAPTER, *PVBOXWDDMDISP_ADAPTER;

typedef struct VBOXWDDMDISP_CONTEXT
{
    struct VBOXWDDMDISP_DEVICE *pDevice;
    D3DDDICB_CREATECONTEXT ContextInfo;
} VBOXWDDMDISP_CONTEXT, *PVBOXWDDMDISP_CONTEXT;

typedef struct VBOXWDDMDISP_DEVICE
{
    HANDLE hDevice;
    PVBOXWDDMDISP_ADAPTER pAdapter;
    UINT u32IfVersion;
    UINT uRtVersion;
    D3DDDI_DEVICECALLBACKS RtCallbacks;
    VOID *pvCmdBuffer;
    UINT cbCmdBuffer;
    D3DDDI_CREATEDEVICEFLAGS fFlags;
    VBOXWDDMDISP_CONTEXT DefaultContext;
} VBOXWDDMDISP_DEVICE, *PVBOXWDDMDISP_DEVICE;

typedef struct VBOXWDDMDISP_ALLOCATION
{
    D3DKMT_HANDLE hAllocation;
    VBOXWDDM_ALLOC_TYPE enmType;
    CONST VOID* pvMem;
    VBOXWDDM_SURFACE_DESC SurfDesc;
} VBOXWDDMDISP_ALLOCATION, *PVBOXWDDMDISP_ALLOCATION;

typedef struct VBOXWDDMDISP_RESOURCE
{
    HANDLE hResource;
    D3DKMT_HANDLE hKMResource;
    PVBOXWDDMDISP_DEVICE pDevice;
    uint32_t fFlags;
    VBOXWDDM_RC_DESC RcDesc;
    UINT cAllocations;
    VBOXWDDMDISP_ALLOCATION aAllocations[1];
} VBOXWDDMDISP_RESOURCE, *PVBOXWDDMDISP_RESOURCE;

DECLINLINE(bool) vboxDispD3DIs3DEnabled(VBOXWDDMDISP_ADAPTER * pAdapter)
{
    return !!(pAdapter->pD3D9If);
}

#endif /* #ifndef ___VBoxDispD3D_h___ */
