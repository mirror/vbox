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
#include <iprt/list.h>

#define VBOXWDDMDISP_MAX_VERTEX_STREAMS 16
#define VBOXWDDMDISP_MAX_SWAPCHAIN_SIZE 16

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
    VBOXDISPWORKER WndWorker;
    IDirect3D9Ex * pD3D9If;
    D3DDDI_ADAPTERCALLBACKS RtCallbacks;
    uint32_t cFormstOps;
    FORMATOP *paFormstOps;
    uint32_t cSurfDescs;
    DDSURFACEDESC *paSurfDescs;
    UINT cMaxSimRTs;
#ifdef VBOX_WITH_VIDEOHWACCEL
    uint32_t cHeads;
    VBOXWDDMDISP_HEAD aHeads[1];
#endif
} VBOXWDDMDISP_ADAPTER, *PVBOXWDDMDISP_ADAPTER;

typedef struct VBOXWDDMDISP_CONTEXT
{
    RTLISTNODE ListNode;
    struct VBOXWDDMDISP_DEVICE *pDevice;
    D3DDDICB_CREATECONTEXT ContextInfo;
} VBOXWDDMDISP_CONTEXT, *PVBOXWDDMDISP_CONTEXT;

typedef struct VBOXWDDMDISP_STREAMSOURCEUM
{
    CONST VOID* pvBuffer;
    UINT cbStride;
} VBOXWDDMDISP_STREAMSOURCEUM, *PVBOXWDDMDISP_STREAMSOURCEUM;

typedef struct VBOXWDDMDISP_INDICIESUM
{
    CONST VOID* pvBuffer;
    UINT cbSize;
} VBOXWDDMDISP_INDICIESUM, *PVBOXWDDMDISP_INDICIESUM;

struct VBOXWDDMDISP_ALLOCATION;

typedef struct VBOXWDDMDISP_STREAM_SOURCE_INFO
{
  UINT   uiOffset;
  UINT   uiStride;
} VBOXWDDMDISP_STREAM_SOURCE_INFO;

typedef struct VBOXWDDMDISP_INDICES_INFO
{
  UINT   uiStride;
} VBOXWDDMDISP_INDICES_INFO;

typedef struct VBOXWDDMDISP_RENDERTGT_FLAGS
{
    union
    {
        struct
        {
            UINT bAdded : 1;
            UINT bRemoved : 1;
            UINT Reserved : 30;
        };
        uint32_t Value;
    };
}VBOXWDDMDISP_RENDERTGT_FLAGS;

typedef struct VBOXWDDMDISP_RENDERTGT
{
    struct VBOXWDDMDISP_ALLOCATION *pAlloc;
    UINT cNumFlips;
    VBOXWDDMDISP_RENDERTGT_FLAGS fFlags;
} VBOXWDDMDISP_RENDERTGT, *PVBOXWDDMDISP_RENDERTGT;

#define VBOXWDDMDISP_INDEX_UNDEFINED (~0)
typedef struct VBOXWDDMDISP_SWAPCHAIN_FLAGS
{
    union
    {
        struct
        {
            UINT bChanged : 1;
            UINT Reserved : 31;
        };
        uint32_t Value;
    };
}VBOXWDDMDISP_SWAPCHAIN_FLAGS;

typedef struct VBOXWDDMDISP_SWAPCHAIN
{
    RTLISTNODE ListEntry;
    UINT iBB; /* Backbuffer index */
    UINT cRTs; /* Number of render targets in the swapchain */
    VBOXWDDMDISP_SWAPCHAIN_FLAGS fFlags;
#ifndef VBOXWDDM_WITH_VISIBLE_FB
    IDirect3DSurface9 *pRenderTargetFbCopy;
#endif
    IDirect3DSwapChain9 *pSwapChainIf;
    HWND hWnd;
    VBOXDISP_KMHANDLE hSwapchainKm;
    VBOXWDDMDISP_RENDERTGT aRTs[VBOXWDDMDISP_MAX_SWAPCHAIN_SIZE];
} VBOXWDDMDISP_SWAPCHAIN, *PVBOXWDDMDISP_SWAPCHAIN;


//typedef struct VBOXWDDMDISP_SCREEN
//{
//    RTLISTNODE SwapchainList;
//    IDirect3DDevice9 *pDevice9If;
////    struct VBOXWDDMDISP_RESOURCE *pDstSharedRc;
//    uint32_t iRenderTargetFrontBuf;
//    HWND hWnd;
//} VBOXWDDMDISP_SCREEN, *PVBOXWDDMDISP_SCREEN;

typedef struct VBOXWDDMDISP_DEVICE
{
    HANDLE hDevice;
    PVBOXWDDMDISP_ADAPTER pAdapter;
    IDirect3DDevice9 *pDevice9If;
    RTLISTNODE SwapchainList;
    UINT u32IfVersion;
    UINT uRtVersion;
    D3DDDI_DEVICECALLBACKS RtCallbacks;
    VOID *pvCmdBuffer;
    UINT cbCmdBuffer;
    D3DDDI_CREATEDEVICEFLAGS fFlags;
    /* number of StreamSources set */
    UINT cStreamSources;
    VBOXWDDMDISP_STREAMSOURCEUM aStreamSourceUm[VBOXWDDMDISP_MAX_VERTEX_STREAMS];
    struct VBOXWDDMDISP_ALLOCATION *aStreamSource[VBOXWDDMDISP_MAX_VERTEX_STREAMS];
    VBOXWDDMDISP_STREAM_SOURCE_INFO StreamSourceInfo[VBOXWDDMDISP_MAX_VERTEX_STREAMS];
    VBOXWDDMDISP_INDICIESUM IndiciesUm;
    struct VBOXWDDMDISP_ALLOCATION *pIndicesAlloc;
    VBOXWDDMDISP_INDICES_INFO IndiciesInfo;
    /* need to cache the ViewPort data because IDirect3DDevice9::SetViewport
     * is split into two calls : SetViewport & SetZRange */
    D3DVIEWPORT9 ViewPort;
    VBOXWDDMDISP_CONTEXT DefaultContext;

    CRITICAL_SECTION DirtyAllocListLock;
    RTLISTNODE DirtyAllocList;
    UINT cRTs;
    struct VBOXWDDMDISP_ALLOCATION * apRTs[1];
} VBOXWDDMDISP_DEVICE, *PVBOXWDDMDISP_DEVICE;

typedef struct VBOXWDDMDISP_LOCKINFO
{
    uint32_t cLocks;
    union {
        D3DDDIRANGE  Range;
        RECT  Area;
        D3DDDIBOX  Box;
    };
    D3DDDI_LOCKFLAGS fFlags;
    D3DLOCKED_RECT LockedRect;
} VBOXWDDMDISP_LOCKINFO;

typedef enum
{
    VBOXDISP_D3DIFTYPE_UNDEFINED = 0,
    VBOXDISP_D3DIFTYPE_SURFACE,
    VBOXDISP_D3DIFTYPE_TEXTURE,
    VBOXDISP_D3DIFTYPE_VERTEXBUFFER,
    VBOXDISP_D3DIFTYPE_INDEXBUFFER
} VBOXDISP_D3DIFTYPE;

typedef struct VBOXWDDMDISP_ALLOCATION
{
    D3DKMT_HANDLE hAllocation;
    VBOXWDDM_ALLOC_TYPE enmType;
    UINT iAlloc;
    struct VBOXWDDMDISP_RESOURCE *pRc;
    void* pvMem;
    UINT D3DWidth;
    /* object type is defined by enmD3DIfType enum */
    IUnknown *pD3DIf;
    IUnknown *pSecondaryOpenedD3DIf;
    VBOXDISP_D3DIFTYPE enmD3DIfType;
    RTLISTNODE DirtyAllocListEntry;
    HANDLE hSharedHandle;
    VBOXWDDMDISP_LOCKINFO LockInfo;
    VBOXWDDM_DIRTYREGION DirtyRegion; /* <- dirty region to notify host about */
    VBOXWDDM_SURFACE_DESC SurfDesc;
    PVBOXWDDMDISP_SWAPCHAIN pSwapchain;
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

typedef struct VBOXWDDMDISP_TSS_LOOKUP
{
    BOOL  bSamplerState;
    DWORD dType;
} VBOXWDDMDISP_TSS_LOOKUP;

typedef struct VBOXWDDMDISP_OVERLAY
{
    D3DKMT_HANDLE hOverlay;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
    PVBOXWDDMDISP_RESOURCE *pResource;
} VBOXWDDMDISP_OVERLAY, *PVBOXWDDMDISP_OVERLAY;

#define VBOXDISPMODE_IS_3D(_p) (!!((_p)->pD3D9If))
#ifdef VBOXDISP_EARLYCREATEDEVICE
#define VBOXDISP_D3DEV(_p) (_p)->pDevice9If
#else
#define VBOXDISP_D3DEV(_p) vboxWddmD3DDeviceGet(_p)
#endif

#endif /* #ifndef ___VBoxDispD3D_h___ */
