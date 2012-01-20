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

#ifndef ___VBoxDispD3D_h___
#define ___VBoxDispD3D_h___

#include "VBoxDispD3DIf.h"
#include "common/wddm/VBoxMPIf.h"
#ifdef VBOX_WITH_CRHGSMI
#include "VBoxUhgsmiDisp.h"
#endif

#include <iprt/cdefs.h>
#include <iprt/list.h>

#define VBOXWDDMDISP_MAX_VERTEX_STREAMS 16
#define VBOXWDDMDISP_MAX_SWAPCHAIN_SIZE 16
/* maximum number of direct render targets to be used before
 * switching to offscreen rendering */
#ifdef VBOXWDDMDISP_DEBUG
# define VBOXWDDMDISP_MAX_DIRECT_RTS      g_VBoxVDbgCfgMaxDirectRts
#else
# define VBOXWDDMDISP_MAX_DIRECT_RTS      3
#endif

#define VBOXWDDMDISP_IS_TEXTURE(_f) ((_f).Texture || (_f).Value == 0)

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
            UINT bChanged                : 1;
            UINT bRtReportingPresent     : 1; /* use VBox extension method for performing present */
            UINT bSwitchReportingPresent : 1; /* switch to use VBox extension method for performing present on next present */
            UINT Reserved                : 29;
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
    BOOL bRTFbCopyUpToDate;
#endif
    IDirect3DSwapChain9 *pSwapChainIf;
    /* a read-only hWnd we receive from wine
     * we use it for visible region notifications only,
     * it MUST NOT be destroyed on swapchain destruction,
     * wine will handle that for us */
    HWND hWnd;
    VBOXDISP_KMHANDLE hSwapchainKm;
    VBOXWDDMDISP_RENDERTGT aRTs[VBOXWDDMDISP_MAX_SWAPCHAIN_SIZE];
} VBOXWDDMDISP_SWAPCHAIN, *PVBOXWDDMDISP_SWAPCHAIN;

typedef struct VBOXWDDMDISP_DEVICE
{
    HANDLE hDevice;
    PVBOXWDDMDISP_ADAPTER pAdapter;
    IDirect3DDevice9 *pDevice9If;
    RTLISTANCHOR SwapchainList;
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
#ifdef VBOX_WITH_CRHGSMI
    VBOXUHGSMI_PRIVATE_D3D Uhgsmi;
#endif

    /* no lock is needed for this since we're guaranteed the per-device calls are not reentrant */
    RTLISTANCHOR DirtyAllocList;

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
#ifdef VBOXWDDMDISP_DEBUG
    PVOID pvData;
#endif
} VBOXWDDMDISP_LOCKINFO;

typedef enum
{
    VBOXDISP_D3DIFTYPE_UNDEFINED = 0,
    VBOXDISP_D3DIFTYPE_SURFACE,
    VBOXDISP_D3DIFTYPE_TEXTURE,
    VBOXDISP_D3DIFTYPE_CUBE_TEXTURE,
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
    VBOXDISP_D3DIFTYPE enmD3DIfType;
    /* list entry used to add allocation to the dirty alloc list */
    RTLISTNODE DirtyAllocListEntry;
    BOOLEAN fDirtyWrite;
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
    VBOXWDDMDISP_RESOURCE_FLAGS fFlags;
    VBOXWDDM_RC_DESC RcDesc;
    UINT cAllocations;
    VBOXWDDMDISP_ALLOCATION aAllocations[1];
} VBOXWDDMDISP_RESOURCE, *PVBOXWDDMDISP_RESOURCE;

typedef struct VBOXWDDMDISP_QUERY
{
    D3DDDIQUERYTYPE enmType;
    D3DDDI_ISSUEQUERYFLAGS fQueryState;
    union
    {
        BOOL bData;
        UINT u32Data;
    } data ;
} VBOXWDDMDISP_QUERY, *PVBOXWDDMDISP_QUERY;

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

#define VBOXDISP_CUBEMAP_LEVELS_COUNT(pRc) (((pRc)->cAllocations)/6)
#define VBOXDISP_CUBEMAP_INDEX_TO_FACE(pRc, idx) ((D3DCUBEMAP_FACES)(D3DCUBEMAP_FACE_POSITIVE_X+(idx)%VBOXDISP_CUBEMAP_LEVELS_COUNT(pRc)))
#define VBOXDISP_CUBEMAP_INDEX_TO_LEVEL(pRc, idx) ((idx)%VBOXDISP_CUBEMAP_LEVELS_COUNT(pRc))

#ifdef VBOX_WITH_CRHGSMI
HRESULT vboxUhgsmiGlobalSetCurrent();
HRESULT vboxUhgsmiGlobalClearCurrent();
#endif

DECLINLINE(PVBOXWDDMDISP_SWAPCHAIN) vboxWddmSwapchainForAlloc(PVBOXWDDMDISP_ALLOCATION pAlloc)
{
    return pAlloc->pSwapchain;
}

DECLINLINE(UINT) vboxWddmSwapchainIdxFb(PVBOXWDDMDISP_SWAPCHAIN pSwapchain)
{
    return (pSwapchain->iBB + pSwapchain->cRTs - 1) % pSwapchain->cRTs;
}

/* if swapchain contains only one surface returns this surface */
DECLINLINE(PVBOXWDDMDISP_RENDERTGT) vboxWddmSwapchainGetBb(PVBOXWDDMDISP_SWAPCHAIN pSwapchain)
{
    if (pSwapchain->cRTs)
    {
        Assert(pSwapchain->iBB < pSwapchain->cRTs);
        return &pSwapchain->aRTs[pSwapchain->iBB];
    }
    return NULL;
}

DECLINLINE(PVBOXWDDMDISP_RENDERTGT) vboxWddmSwapchainGetFb(PVBOXWDDMDISP_SWAPCHAIN pSwapchain)
{
    if (pSwapchain->cRTs)
    {
        UINT iFb = vboxWddmSwapchainIdxFb(pSwapchain);
        return &pSwapchain->aRTs[iFb];
    }
    return NULL;
}

/* on success increments the surface ref counter,
 * i.e. one must call pSurf->Release() once the surface is not needed*/
DECLINLINE(HRESULT) vboxWddmSurfGet(PVBOXWDDMDISP_RESOURCE pRc, UINT iAlloc, IDirect3DSurface9 **ppSurf)
{
    HRESULT hr = S_OK;
    Assert(pRc->cAllocations > iAlloc);
    switch (pRc->aAllocations[0].enmD3DIfType)
    {
        case VBOXDISP_D3DIFTYPE_SURFACE:
        {
            IDirect3DSurface9 *pD3DIfSurf = (IDirect3DSurface9*)pRc->aAllocations[iAlloc].pD3DIf;
            Assert(pD3DIfSurf);
            pD3DIfSurf->AddRef();
            *ppSurf = pD3DIfSurf;
            break;
        }
        case VBOXDISP_D3DIFTYPE_TEXTURE:
        {
            Assert(pRc->cAllocations == 1); /* <- vboxWddmSurfGet is typically used in Blt & ColorFill functions
                                             * in this case, if texture is used as a destination,
                                             * we should update sub-layers as well which is not done currently
                                             * so for now check vboxWddmSurfGet is used for one-level textures */
            IDirect3DTexture9 *pD3DIfTex = (IDirect3DTexture9*)pRc->aAllocations[0].pD3DIf;
            IDirect3DSurface9 *pSurfaceLevel;
            Assert(pD3DIfTex);
            hr = pD3DIfTex->GetSurfaceLevel(iAlloc, &pSurfaceLevel);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                *ppSurf = pSurfaceLevel;
            }
            break;
        }
        case VBOXDISP_D3DIFTYPE_CUBE_TEXTURE:
        {
            Assert(0);
            IDirect3DCubeTexture9 *pD3DIfCubeTex = (IDirect3DCubeTexture9*)pRc->aAllocations[0].pD3DIf;
            IDirect3DSurface9 *pSurfaceLevel;
            Assert(pD3DIfCubeTex);
            hr = pD3DIfCubeTex->GetCubeMapSurface(VBOXDISP_CUBEMAP_INDEX_TO_FACE(pRc, iAlloc),
                                                  VBOXDISP_CUBEMAP_INDEX_TO_LEVEL(pRc, iAlloc), &pSurfaceLevel);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                *ppSurf = pSurfaceLevel;
            }
            break;
        }
        default:
            Assert(0);
            hr = E_FAIL;
            break;
    }
    return hr;
}

HRESULT vboxWddmLockRect(PVBOXWDDMDISP_RESOURCE pRc, UINT iAlloc,
        D3DLOCKED_RECT * pLockedRect,
        CONST RECT *pRect,
        DWORD fLockFlags);
HRESULT vboxWddmUnlockRect(PVBOXWDDMDISP_RESOURCE pRc, UINT iAlloc);

#define VBOXDISPMODE_IS_3D(_p) (!!((_p)->pD3D9If))
#ifdef VBOXDISP_EARLYCREATEDEVICE
#define VBOXDISP_D3DEV(_p) (_p)->pDevice9If
#else
#define VBOXDISP_D3DEV(_p) vboxWddmD3DDeviceGet(_p)
#endif

#endif /* #ifndef ___VBoxDispD3D_h___ */
