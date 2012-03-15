/* $Id$ */

/** @file
 * VBox WDDM Miniport driver
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

#ifndef ___VBoxMPTypes_h___
#define ___VBoxMPTypes_h___

typedef struct _VBOXMP_DEVEXT *PVBOXMP_DEVEXT;
typedef struct VBOXWDDM_SWAPCHAIN *PVBOXWDDM_SWAPCHAIN;
typedef struct VBOXWDDM_CONTEXT *PVBOXWDDM_CONTEXT;
typedef struct VBOXWDDM_ALLOCATION *PVBOXWDDM_ALLOCATION;

#include "common/wddm/VBoxMPIf.h"
#include "VBoxMPMisc.h"
#include "VBoxMPCm.h"
#include "VBoxMPVdma.h"
#include "VBoxMPShgsmi.h"
#include "VBoxMPVbva.h"
#include "VBoxMPCr.h"

#if 0
#include <iprt/avl.h>
#endif

/* one page size */
#define VBOXWDDM_C_DMA_BUFFER_SIZE         0x1000
#define VBOXWDDM_C_DMA_PRIVATEDATA_SIZE    0x4000
#define VBOXWDDM_C_ALLOC_LIST_SIZE         0xc00
#define VBOXWDDM_C_PATH_LOCATION_LIST_SIZE 0xc00

#define VBOXWDDM_C_POINTER_MAX_WIDTH  64
#define VBOXWDDM_C_POINTER_MAX_HEIGHT 64

#ifdef VBOX_WITH_VDMA
#define VBOXWDDM_C_VDMA_BUFFER_SIZE   (64*_1K)
#endif

#ifndef VBOXWDDM_RENDER_FROM_SHADOW
# ifndef VBOX_WITH_VDMA
#  error "VBOX_WITH_VDMA must be defined!!!"
# endif
#endif

#define VBOXWDDM_POINTER_ATTRIBUTES_SIZE VBOXWDDM_ROUNDBOUND( \
         VBOXWDDM_ROUNDBOUND( sizeof (VIDEO_POINTER_ATTRIBUTES), 4 ) + \
         VBOXWDDM_ROUNDBOUND(VBOXWDDM_C_POINTER_MAX_WIDTH * VBOXWDDM_C_POINTER_MAX_HEIGHT * 4, 4) + \
         VBOXWDDM_ROUNDBOUND((VBOXWDDM_C_POINTER_MAX_WIDTH * VBOXWDDM_C_POINTER_MAX_HEIGHT + 7) >> 3, 4) \
          , 8)

typedef struct _VBOXWDDM_POINTER_INFO
{
    uint32_t xPos;
    uint32_t yPos;
    union
    {
        VIDEO_POINTER_ATTRIBUTES data;
        char buffer[VBOXWDDM_POINTER_ATTRIBUTES_SIZE];
    } Attributes;
} VBOXWDDM_POINTER_INFO, *PVBOXWDDM_POINTER_INFO;

typedef struct _VBOXWDDM_GLOBAL_POINTER_INFO
{
    uint32_t iLastReportedScreen;
    uint32_t cVisible;
} VBOXWDDM_GLOBAL_POINTER_INFO, *PVBOXWDDM_GLOBAL_POINTER_INFO;

#ifdef VBOX_WITH_VIDEOHWACCEL
typedef struct VBOXWDDM_VHWA
{
    VBOXVHWA_INFO Settings;
    volatile uint32_t cOverlaysCreated;
} VBOXWDDM_VHWA;
#endif

typedef struct VBOXWDDM_SOURCE
{
    struct VBOXWDDM_ALLOCATION * pPrimaryAllocation;
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
    struct VBOXWDDM_ALLOCATION * pShadowAllocation;
    VBOXVIDEOOFFSET offVram;
    VBOXWDDM_SURFACE_DESC SurfDesc;
    VBOXVBVAINFO Vbva;
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* @todo: in our case this seems more like a target property,
     * but keep it here for now */
    VBOXWDDM_VHWA Vhwa;
    volatile uint32_t cOverlays;
    LIST_ENTRY OverlayList;
    KSPIN_LOCK OverlayListLock;
#endif
    POINT VScreenPos;
    VBOXWDDM_POINTER_INFO PointerInfo;
} VBOXWDDM_SOURCE, *PVBOXWDDM_SOURCE;

typedef struct VBOXWDDM_TARGET
{
    uint32_t ScanLineState;
    uint32_t HeightVisible;
    uint32_t HeightTotal;
} VBOXWDDM_TARGET, *PVBOXWDDM_TARGET;

/* allocation */
//#define VBOXWDDM_ALLOCATIONINDEX_VOID (~0U)
typedef struct VBOXWDDM_ALLOCATION
{
    LIST_ENTRY SwapchainEntry;
    struct VBOXWDDM_SWAPCHAIN *pSwapchain;
    VBOXWDDM_ALLOC_TYPE enmType;
    volatile uint32_t cRefs;
    D3DDDI_RESOURCEFLAGS fRcFlags;
    UINT SegmentId;
    VBOXVIDEOOFFSET offVram;
#ifdef VBOX_WITH_VIDEOHWACCEL
    VBOXVHWA_SURFHANDLE hHostHandle;
#endif
    BOOLEAN fDeleted;
    BOOLEAN bVisible;
    BOOLEAN bAssigned;
#ifdef DEBUG
    /* current for shared rc handling assumes that once resource has no opens, it can not be openned agaion */
    BOOLEAN fAssumedDeletion;
#endif
    VBOXWDDM_SURFACE_DESC SurfDesc;
    struct VBOXWDDM_RESOURCE *pResource;
    /* to return to the Runtime on DxgkDdiCreateAllocation */
    DXGK_ALLOCATIONUSAGEHINT UsageHint;
    uint32_t iIndex;
    uint32_t cOpens;
    LIST_ENTRY OpenList;
    /* helps tracking when to release wine shared resource */
    uint32_t cShRcRefs;
    HANDLE hSharedHandle;
#if 0
    AVLPVNODECORE ShRcTreeEntry;
#endif
    VBOXUHGSMI_SYNCHOBJECT_TYPE enmSynchType;
    union
    {
        PKEVENT pSynchEvent;
        PRKSEMAPHORE pSynchSemaphore;
    };
} VBOXWDDM_ALLOCATION, *PVBOXWDDM_ALLOCATION;

typedef struct VBOXWDDM_RESOURCE
{
    VBOXWDDMDISP_RESOURCE_FLAGS fFlags;
    volatile uint32_t cRefs;
    VBOXWDDM_RC_DESC RcDesc;
    BOOLEAN fDeleted;
    uint32_t cAllocations;
    VBOXWDDM_ALLOCATION aAllocations[1];
} VBOXWDDM_RESOURCE, *PVBOXWDDM_RESOURCE;

typedef struct VBOXWDDM_OVERLAY
{
    LIST_ENTRY ListEntry;
    PVBOXMP_DEVEXT pDevExt;
    PVBOXWDDM_RESOURCE pResource;
    PVBOXWDDM_ALLOCATION pCurentAlloc;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
    RECT DstRect;
} VBOXWDDM_OVERLAY, *PVBOXWDDM_OVERLAY;

typedef enum
{
    VBOXWDDM_DEVICE_TYPE_UNDEFINED = 0,
    VBOXWDDM_DEVICE_TYPE_SYSTEM
} VBOXWDDM_DEVICE_TYPE;

typedef struct VBOXWDDM_DEVICE
{
    PVBOXMP_DEVEXT pAdapter; /* Adapder info */
    HANDLE hDevice; /* handle passed to CreateDevice */
    VBOXWDDM_DEVICE_TYPE enmType; /* device creation flags passed to DxgkDdiCreateDevice, not sure we need it */
} VBOXWDDM_DEVICE, *PVBOXWDDM_DEVICE;

typedef enum
{
    VBOXWDDM_OBJSTATE_TYPE_UNKNOWN = 0,
    VBOXWDDM_OBJSTATE_TYPE_INITIALIZED,
    VBOXWDDM_OBJSTATE_TYPE_TERMINATED
} VBOXWDDM_OBJSTATE_TYPE;

#define VBOXWDDM_INVALID_COORD ((LONG)((~0UL) >> 1))

typedef struct VBOXWDDM_SWAPCHAIN
{
    LIST_ENTRY DevExtListEntry;
    LIST_ENTRY AllocList;
    struct VBOXWDDM_CONTEXT *pContext;
    VBOXWDDM_OBJSTATE_TYPE enmState;
    volatile uint32_t cRefs;
    VBOXDISP_UMHANDLE hSwapchainUm;
    VBOXDISP_KMHANDLE hSwapchainKm;
    POINT Pos;
    UINT width;
    UINT height;
    VBOXWDDMVR_LIST VisibleRegions;
}VBOXWDDM_SWAPCHAIN, *PVBOXWDDM_SWAPCHAIN;

typedef struct VBOXWDDM_CONTEXT
{
    struct VBOXWDDM_DEVICE * pDevice;
    HANDLE hContext;
    VBOXWDDM_CONTEXT_TYPE enmType;
    UINT  NodeOrdinal;
    UINT  EngineAffinity;
    uint32_t u32CrConClientID;
    VBOXWDDM_HTABLE Swapchains;
    VBOXVIDEOCM_CTX CmContext;
    VBOXVIDEOCM_ALLOC_CONTEXT AllocContext;
} VBOXWDDM_CONTEXT, *PVBOXWDDM_CONTEXT;

typedef struct VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR
{
    VBOXWDDM_DMA_PRIVATEDATA_BASEHDR BaseHdr;
}VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR, *PVBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR;

#ifdef VBOXWDDM_RENDER_FROM_SHADOW

typedef struct VBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY
{
    VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR Hdr;
    VBOXVDMA_SHADOW2PRIMARY Shadow2Primary;
} VBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY, *PVBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY;

#endif

typedef struct VBOXWDDM_DMA_PRIVATEDATA_BLT
{
    VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR Hdr;
    VBOXVDMA_BLT Blt;
} VBOXWDDM_DMA_PRIVATEDATA_BLT, *PVBOXWDDM_DMA_PRIVATEDATA_BLT;

typedef struct VBOXWDDM_DMA_PRIVATEDATA_FLIP
{
    VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR Hdr;
    VBOXVDMA_FLIP Flip;
} VBOXWDDM_DMA_PRIVATEDATA_FLIP, *PVBOXWDDM_DMA_PRIVATEDATA_FLIP;

typedef struct VBOXWDDM_DMA_PRIVATEDATA_CLRFILL
{
    VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR Hdr;
    VBOXVDMA_CLRFILL ClrFill;
} VBOXWDDM_DMA_PRIVATEDATA_CLRFILL, *PVBOXWDDM_DMA_PRIVATEDATA_CLRFILL;

typedef struct VBOXWDDM_UHGSMI_BUFFER_SUBMIT_INFO
{
    VBOXWDDM_DMA_ALLOCINFO Alloc;
    uint32_t cbData;
    uint32_t bDoNotSignalCompletion;
} VBOXWDDM_UHGSMI_BUFFER_SUBMIT_INFO, *PVBOXWDDM_UHGSMI_BUFFER_SUBMIT_INFO;

typedef struct VBOXWDDM_DMA_PRIVATEDATA_CHROMIUM_CMD
{
    VBOXWDDM_DMA_PRIVATEDATA_BASEHDR Base;
    VBOXWDDM_UHGSMI_BUFFER_SUBMIT_INFO aBufInfos[1];
} VBOXWDDM_DMA_PRIVATEDATA_CHROMIUM_CMD, *PVBOXWDDM_DMA_PRIVATEDATA_CHROMIUM_CMD;

typedef struct VBOXWDDM_DMA_PRIVATEDATA_ALLOCINFO_ON_SUBMIT
{
    VBOXWDDM_DMA_PRIVATEDATA_BASEHDR Base;
    VBOXWDDM_DMA_ALLOCINFO aInfos[1];
} VBOXWDDM_DMA_PRIVATEDATA_ALLOCINFO_ON_SUBMIT, *PVBOXWDDM_DMA_PRIVATEDATA_ALLOCINFO_ON_SUBMIT;

typedef struct VBOXWDDM_OPENALLOCATION
{
    LIST_ENTRY ListEntry;
    D3DKMT_HANDLE  hAllocation;
    PVBOXWDDM_ALLOCATION pAllocation;
    PVBOXWDDM_DEVICE pDevice;
    uint32_t cShRcRefs;
} VBOXWDDM_OPENALLOCATION, *PVBOXWDDM_OPENALLOCATION;

#define VBOXWDDM_MAX_VIDEOMODES 128
typedef struct VBOXWDDM_VIDEOMODES_INFO
{
    int32_t iPreferredMode;
    uint32_t cModes;
    VIDEO_MODE_INFORMATION aModes[VBOXWDDM_MAX_VIDEOMODES];
    int32_t iPreferredResolution;
    uint32_t cResolutions;
    D3DKMDT_2DREGION aResolutions[VBOXWDDM_MAX_VIDEOMODES];
} VBOXWDDM_VIDEOMODES_INFO, *PVBOXWDDM_VIDEOMODES_INFO;

#endif /* #ifndef ___VBoxMPTypes_h___ */
