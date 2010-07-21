/*
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
#ifndef ___VBoxVideoWddm_h___
#define ___VBoxVideoWddm_h___

#include "../VBoxVideo.h"

/* one page size */
#define VBOXWDDM_C_DMA_BUFFER_SIZE         0x1000
#define VBOXWDDM_C_DMA_PRIVATEDATA_SIZE    0x4000
#define VBOXWDDM_C_ALLOC_LIST_SIZE         0xc00
#define VBOXWDDM_C_PATH_LOCATION_LIST_SIZE 0xc00

#define VBOXWDDM_C_POINTER_MAX_WIDTH  64
#define VBOXWDDM_C_POINTER_MAX_HEIGHT 64

#define VBOXWDDM_C_VDMA_BUFFER_SIZE   (64*_1K)

#ifdef VBOXWDDM_WITH_VBVA
# define VBOXWDDM_RENDER_FROM_SHADOW
#endif

#ifndef DEBUG_misha
# ifdef Assert
#  undef Assert
#  define Assert(_a) do{}while(0)
# endif
# ifdef AssertBreakpoint
#  undef AssertBreakpoint
#  define AssertBreakpoint() do{}while(0)
# endif
# ifdef AssertFailed
#  undef AssertFailed
#  define AssertFailed() do{}while(0)
# endif
#endif

PVOID vboxWddmMemAlloc(IN SIZE_T cbSize);
PVOID vboxWddmMemAllocZero(IN SIZE_T cbSize);
VOID vboxWddmMemFree(PVOID pvMem);

NTSTATUS vboxWddmDmaCmdNotifyCompletion(PDEVICE_EXTENSION pDevExt, struct VBOXWDDM_CONTEXT *pContext, UINT SubmissionFenceId);
NTSTATUS vboxWddmCallIsr(PDEVICE_EXTENSION pDevExt);

/* allocation */
//#define VBOXWDDM_ALLOCATIONINDEX_VOID (~0U)
typedef struct VBOXWDDM_ALLOCATION
{
    VBOXWDDM_ALLOC_TYPE enmType;
//    VBOXWDDM_ALLOCUSAGE_TYPE enmCurrentUsage;
    D3DDDI_RESOURCEFLAGS fRcFlags;
    UINT SegmentId;
    VBOXVIDEOOFFSET offVram;
#ifdef VBOX_WITH_VIDEOHWACCEL
    VBOXVHWA_SURFHANDLE hHostHandle;
#endif
    BOOLEAN bVisible;
    BOOLEAN bAssigned;
    VBOXWDDM_SURFACE_DESC SurfDesc;
    struct VBOXWDDM_RESOURCE *pResource;
    /* to return to the Runtime on DxgkDdiCreateAllocation */
    DXGK_ALLOCATIONUSAGEHINT UsageHint;
    uint32_t iIndex;
} VBOXWDDM_ALLOCATION, *PVBOXWDDM_ALLOCATION;

typedef struct VBOXWDDM_RESOURCE
{
    uint32_t fFlags;
    VBOXWDDM_RC_DESC RcDesc;
    uint32_t cAllocations;
    VBOXWDDM_ALLOCATION aAllocations[1];
} VBOXWDDM_RESOURCE, *PVBOXWDDM_RESOURCE;

DECLINLINE(PVBOXWDDM_RESOURCE) vboxWddmResourceForAlloc(PVBOXWDDM_ALLOCATION pAlloc)
{
#if 0
    if(pAlloc->iIndex == VBOXWDDM_ALLOCATIONINDEX_VOID)
        return NULL;
    PVBOXWDDM_RESOURCE pRc = (PVBOXWDDM_RESOURCE)(((uint8_t*)pAlloc) - RT_OFFSETOF(VBOXWDDM_RESOURCE, aAllocations[pAlloc->iIndex]));
    return pRc;
#else
    return pAlloc->pResource;
#endif
}

typedef struct VBOXWDDM_OVERLAY
{
    PDEVICE_EXTENSION pDevExt;
    PVBOXWDDM_RESOURCE pResource;
    PVBOXWDDM_ALLOCATION pCurentAlloc;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
} VBOXWDDM_OVERLAY, *PVBOXWDDM_OVERLAY;

typedef enum
{
    VBOXWDDM_DEVICE_TYPE_UNDEFINED = 0,
    VBOXWDDM_DEVICE_TYPE_SYSTEM
} VBOXWDDM_DEVICE_TYPE;

typedef struct VBOXWDDM_DEVICE
{
    struct _DEVICE_EXTENSION * pAdapter; /* Adapder info */
    HANDLE hDevice; /* handle passed to CreateDevice */
    VBOXWDDM_DEVICE_TYPE enmType; /* device creation flags passed to DxgkDdiCreateDevice, not sure we need it */
} VBOXWDDM_DEVICE, *PVBOXWDDM_DEVICE;

typedef enum
{
    VBOXWDDM_CONTEXT_TYPE_UNDEFINED = 0,
    VBOXWDDM_CONTEXT_TYPE_SYSTEM,
    VBOXWDDM_CONTEXT_TYPE_CUSTOM_3D,
    VBOXWDDM_CONTEXT_TYPE_CUSTOM_2D
} VBOXWDDM_CONTEXT_TYPE;

typedef struct VBOXWDDM_CONTEXT
{
    LIST_ENTRY ListEntry;
    struct VBOXWDDM_DEVICE * pDevice;
    HANDLE hContext;
    VBOXWDDM_CONTEXT_TYPE enmType;
    UINT  NodeOrdinal;
    UINT  EngineAffinity;
    UINT uLastCompletedCmdFenceId;
    RECT ViewRect;
    PVBOXVIDEOCM_CMD_RECTS pLastReportedRects;
    VBOXVIDEOCM_CTX CmContext;
} VBOXWDDM_CONTEXT, *PVBOXWDDM_CONTEXT;

#define VBOXWDDMENTRY_2_CONTEXT(_pE) ((PVBOXWDDM_CONTEXT)((uint8_t*)(_pE) - RT_OFFSETOF(VBOXWDDM_CONTEXT, ListEntry)))

typedef struct VBOXWDDM_DMA_ALLOCINFO
{
    PVBOXWDDM_ALLOCATION pAlloc;
    VBOXVIDEOOFFSET offAlloc;
    UINT segmentIdAlloc;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId;
} VBOXWDDM_DMA_ALLOCINFO, *PVBOXWDDM_DMA_ALLOCINFO;

typedef struct VBOXWDDM_DMA_PRIVATEDATA_FLAFS
{
    union
    {
        struct
        {
            UINT bCmdInDmaBuffer : 1;
            UINT bReserved : 31;
        };
        uint32_t Value;
    };
} VBOXWDDM_DMA_PRIVATEDATA_FLAFS, *PVBOXWDDM_DMA_PRIVATEDATA_FLAFS;

typedef struct VBOXWDDM_DMA_PRIVATEDATA_HDR
{
    PVBOXWDDM_CONTEXT pContext;
    VBOXWDDM_DMA_PRIVATEDATA_FLAFS fFlags;
    VBOXVDMACMD_TYPE enmCmd;
    VBOXWDDM_DMA_ALLOCINFO SrcAllocInfo;
    VBOXWDDM_DMA_ALLOCINFO DstAllocInfo;
}VBOXWDDM_DMA_PRIVATEDATA_HDR, *PVBOXWDDM_DMA_PRIVATEDATA_HDR;

#ifdef VBOXWDDM_RENDER_FROM_SHADOW

typedef struct VBOXWDDM_DMA_PRESENT_RENDER_FROM_SHADOW
{
    VBOXWDDM_DMA_PRIVATEDATA_HDR Hdr;
    RECT rect;
} VBOXWDDM_DMA_PRESENT_RENDER_FROM_SHADOW, *PVBOXWDDM_DMA_PRESENT_RENDER_FROM_SHADOW;

#endif

typedef struct VBOXWDDM_DMA_PRESENT_BLT
{
    VBOXWDDM_DMA_PRIVATEDATA_HDR Hdr;
    RECT SrcRect;
    VBOXVDMAPIPE_RECTS DstRects;
}VBOXWDDM_DMA_PRESENT_BLT, *PVBOXWDDM_DMA_PRESENT_BLT;

typedef struct VBOXVDMACMD_DMA_PRESENT_FLIP
{
    VBOXWDDM_DMA_PRIVATEDATA_HDR Hdr;
} VBOXVDMACMD_DMA_PRESENT_FLIP, *PVBOXVDMACMD_DMA_PRESENT_FLIP;

typedef struct VBOXWDDM_DMA_PRESENT_CLRFILL
{
    VBOXWDDM_DMA_PRIVATEDATA_HDR Hdr;
    UINT Color;
    VBOXWDDM_RECTS_INFO Rects;
} VBOXWDDM_DMA_PRESENT_CLRFILL, *PVBOXWDDM_DMA_PRESENT_CLRFILL;

typedef struct VBOXWDDM_OPENALLOCATION
{
    D3DKMT_HANDLE  hAllocation;
} VBOXWDDM_OPENALLOCATION, *PVBOXWDDM_OPENALLOCATION;

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
# define VBOXWDDM_FB_ALLOCATION(_pSrc) ((_pSrc)->pShadowAllocation)
#else
# define VBOXWDDM_FB_ALLOCATION(_pSrc) ((_pSrc)->pPrimaryAllocation)
#endif

//DECLINLINE(VBOXVIDEOOFFSET) vboxWddmOffsetFromPhAddress(PHYSICAL_ADDRESS phAddr)
//{
//    return (VBOXVIDEOOFFSET)(phAddr.QuadPart ? phAddr.QuadPart - VBE_DISPI_LFB_PHYSICAL_ADDRESS : VBOXVIDEOOFFSET_VOID);
//}

#endif /* #ifndef ___VBoxVideoWddm_h___ */
