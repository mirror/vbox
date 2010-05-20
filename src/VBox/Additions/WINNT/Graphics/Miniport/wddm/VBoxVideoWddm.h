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

#define VBOXWDDM_ROUNDBOUND(_v, _b) (((_v) + ((_b) - 1)) & ~((_b) - 1))

PVOID vboxWddmMemAlloc(IN SIZE_T cbSize);
PVOID vboxWddmMemAllocZero(IN SIZE_T cbSize);
VOID vboxWddmMemFree(PVOID pvMem);

typedef struct VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE
{
    D3DDDI_RATIONAL                 RefreshRate;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID  VidPnSourceId;
} VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE, *PVBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE;

/* allocation */
typedef struct VBOXWDDM_ALLOCATION
{
    VBOXWDDM_ALLOC_TYPE enmType;
    UINT SegmentId;
    VBOXVIDEOOFFSET offVram;
    union
    {
        VBOXWDDM_SURFACE_DESC SurfInfo;
    } u;
} VBOXWDDM_ALLOCATION, *PVBOXWDDM_ALLOCATION;

#define VBOXWDDM_ALLOCATION_HEADSIZE() (sizeof (VBOXWDDM_ALLOCATION))
#define VBOXWDDM_ALLOCATION_SIZE_FROMBODYSIZE(_s) (VBOXWDDM_ALLOCATION_HEADSIZE() + (_s))
#define VBOXWDDM_ALLOCATION_SIZE(_tCmd) (VBOXWDDM_ALLOCATION_SIZE_FROMBODYSIZE(sizeof(_tCmd)))
#define VBOXWDDM_ALLOCATION_BODY(_p, _t) ( (_t*)(((uint8_t*)(_p)) + VBOXWDDM_ALLOCATION_HEADSIZE()) )
#define VBOXWDDM_ALLOCATION_HEAD(_pb) ((VBOXWDDM_ALLOCATION*)((uint8_t *)(_pb) - VBOXWDDM_ALLOCATION_HEADSIZE()))


typedef struct VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE
{
    D3DDDI_RATIONAL RefreshRate;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
//    VBOXVIDEOOFFSET offAddress;
    BOOLEAN bVisible;
    BOOLEAN bAssigned;
} VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE, *PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE;

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
typedef struct VBOXWDDM_ALLOCATION_SHADOWSURFACE
{
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
//    VBOXVIDEOOFFSET offAddress;
    BOOLEAN bVisible;
    BOOLEAN bAssigned;
} VBOXWDDM_ALLOCATION_SHADOWSURFACE, *PVBOXWDDM_ALLOCATION_SHADOWSURFACE;
#endif

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
    VBOXWDDM_CONTEXT_TYPE_SYSTEM
} VBOXWDDM_CONTEXT_TYPE;

typedef struct VBOXWDDM_CONTEXT
{
    struct VBOXWDDM_DEVICE * pDevice;
    HANDLE hContext;
    VBOXWDDM_CONTEXT_TYPE enmType;
    UINT  NodeOrdinal;
    UINT  EngineAffinity;
    UINT uLastCompletedCmdFenceId;
} VBOXWDDM_CONTEXT, *PVBOXWDDM_CONTEXT;

typedef struct VBOXWDDM_DMA_PRIVATE_DATA
{
    PVBOXWDDM_CONTEXT pContext;
    VBOXVDMACMD_TYPE enmCmd;
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
    RECT rect;
    VBOXVIDEOOFFSET offShadow;
    UINT segmentIdShadow;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId;
#endif
//    uint8_t Reserved[8];
}VBOXWDDM_DMA_PRIVATE_DATA, *PVBOXWDDM_DMA_PRIVATE_DATA;

typedef struct VBOXWDDM_OPENALLOCATION
{
    D3DKMT_HANDLE  hAllocation;
} VBOXWDDM_OPENALLOCATION, *PVBOXWDDM_OPENALLOCATION;

#endif /* #ifndef ___VBoxVideoWddm_h___ */
