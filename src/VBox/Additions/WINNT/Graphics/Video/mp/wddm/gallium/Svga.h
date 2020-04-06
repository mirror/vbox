/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver VMSVGA.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_Svga_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_Svga_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VBoxMPGaUtils.h"

#include <iprt/assert.h>
#include <iprt/avl.h>
#include <iprt/list.h>

#include <VBoxGaTypes.h>
#include <VBoxGaHWSVGA.h>

#include <svga_reg.h>
#include <svga3d_reg.h>

#ifndef SVGA_CAP_GBOBJECTS
#define SVGA_CAP_GBOBJECTS 0x08000000
#define SVGA_REG_DEV_CAP 52
#endif

#define SVGA_SYNC_GENERIC         1
#define SVGA_SYNC_FIFOFULL        2

/* Make sure that correct svga3d_reg.h was included. */
AssertCompileSize(SVGA3dCmdHeader, 8);

typedef struct VMSVGAFIFO
{
    /** SVGA_FIFO_CAPABILITIES */
    uint32_t u32FifoCaps;

    /** How many bytes were reserved for a command. */
    uint32_t cbReserved;

    /** Temporary buffer for commands crossing the FIFO ring boundary. */
    void *pvBuffer;

    /** FIFO access. */
    FAST_MUTEX FifoMutex;

} VMSVGAFIFO;
typedef VMSVGAFIFO *PVMSVGAFIFO;

/* VMSVGA specific part of Gallium device extension. */
typedef struct VBOXWDDM_EXT_VMSVGA
{
    /** First IO port. SVGA_*_PORT are relative to it. */
    RTIOPORT ioportBase;
    /** Pointer to FIFO MMIO region. */
    volatile uint32_t *pu32FIFO;

    /**
     * Hardware capabilities.
     */
    uint32_t u32Caps;         /** SVGA_REG_CAPABILITIES */
    uint32_t u32VramSize;     /** SVGA_REG_VRAM_SIZE */
    uint32_t u32FifoSize;     /** SVGA_REG_MEM_SIZE */
    uint32_t u32MaxWidth;     /** SVGA_REG_MAX_WIDTH */
    uint32_t u32MaxHeight;    /** SVGA_REG_MAX_HEIGHT */
    uint32_t u32GmrMaxIds;    /** SVGA_REG_GMR_MAX_IDS */
    uint32_t u32GmrMaxPages;  /** SVGA_REG_GMRS_MAX_PAGES */
    uint32_t u32MemorySize;   /** SVGA_REG_MEMORY_SIZE */
    uint32_t u32MaxTextureWidth;  /** SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH */
    uint32_t u32MaxTextureHeight; /** SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT */

    uint32_t u32MaxTextureLevels; /** 1 + floor(log2(max(u32MaxTextureWidth, u32MaxTextureHeight))) */

    /** Fifo state. */
    VMSVGAFIFO fifo;

    /** For atomic hardware access. */
    KSPIN_LOCK HwSpinLock;

    /** Maintaining the host objects lists. */
    KSPIN_LOCK HostObjectsSpinLock;

    /** AVL tree for mapping GMR id to the corresponding structure. */
    AVLU32TREE GMRTree;

    /** AVL tree for mapping sids to the surface objects. */
    AVLU32TREE SurfaceTree;

    /** List of host objects, which must be deleted at PASSIVE_LEVEL. */
    RTLISTANCHOR DeletedHostObjectsList;

    /** SVGA data access. */
    FAST_MUTEX SvgaMutex;

    struct
    {
        uint32_t u32Offset;
        uint32_t u32BytesPerLine;
    } lastGMRFB;

    /** Bitmap of used GMR ids. Bit 0 - GMR id 0, etc. */
    uint32_t *pu32GMRBits; /* Number of GMRs is controlled by the host (u32GmrMaxIds), so allocate the bitmap. */
    uint32_t cbGMRBits;    /* Bytes allocated for pu32GMRBits */

    /** Bitmap of used context ids. Bit 0 - context id 0, etc. */
    uint32_t au32ContextBits[(SVGA3D_MAX_CONTEXT_IDS + 31) / 32];

    /** Bitmap of used surface ids. Bit 0 - surface id 0, etc. */
    uint32_t au32SurfaceBits[(SVGA3D_MAX_SURFACE_IDS + 31) / 32];
} VBOXWDDM_EXT_VMSVGA;
typedef struct VBOXWDDM_EXT_VMSVGA *PVBOXWDDM_EXT_VMSVGA;

typedef struct SVGAHOSTOBJECT SVGAHOSTOBJECT;
typedef DECLCALLBACK(NTSTATUS) FNHostObjectDestroy(SVGAHOSTOBJECT *pThis);
typedef FNHostObjectDestroy *PFNHostObjectDestroy;

#define SVGA_HOST_OBJECT_UNDEFINED 0
#define SVGA_HOST_OBJECT_SURFACE   1

/* Information about an allocated surface. */
typedef struct SVGAHOSTOBJECT
{
    union {
        /* core.Key is the object id: surface id (sid), or (curently not required) context id (cid), gmr id (gid). */
        struct
        {
            AVLU32NODECORE core;
        } avl;

        struct {
            RTLISTNODE node;
            uint32_t u32Key;
        } list;
    } u;

    /* By UM driver, during submission of commands, by shared surface, etc. */
    uint32_t volatile cRefs;

    /* SVGA_HOST_OBJECT_ type. */
    uint32_t uType;

    /* Device the object is associated with. */
    VBOXWDDM_EXT_VMSVGA *pSvga;

    /* Destructor. */
    PFNHostObjectDestroy pfnHostObjectDestroy;
} SVGAHOSTOBJECT;

#define SVGAHOSTOBJECTID(pHO) ((pHO)->u.avl.core.Key)

/* Information about an allocated surface. */
typedef struct SURFACEOBJECT
{
    SVGAHOSTOBJECT ho;

    /* The actual sid, which must be used by the commands instead of the ho.core.Key.
     * Equal to the AVL node key for non-shared surfaces.
     */
    uint32_t u32SharedSid;
} SURFACEOBJECT;
AssertCompile(RT_OFFSETOF(SURFACEOBJECT, ho) == 0);

typedef struct GAHWRENDERDATA
{
    RTLISTNODE node;
    uint32_t u32SubmissionFenceId;
    uint32_t u32Reserved;
    /* Private data follows. */
} GAHWRENDERDATA;

DECLINLINE(void) SvgaHostObjectsLock(VBOXWDDM_EXT_VMSVGA *pSvga, KIRQL *pOldIrql)
{
    KeAcquireSpinLock(&pSvga->HostObjectsSpinLock, pOldIrql);
}

DECLINLINE(void) SvgaHostObjectsUnlock(VBOXWDDM_EXT_VMSVGA *pSvga, KIRQL OldIrql)
{
    KeReleaseSpinLock(&pSvga->HostObjectsSpinLock, OldIrql);
}

NTSTATUS SvgaHostObjectsCleanup(VBOXWDDM_EXT_VMSVGA *pSvga);

NTSTATUS SvgaAdapterStart(PVBOXWDDM_EXT_VMSVGA *ppSvga,
                          DXGKRNL_INTERFACE *pDxgkInterface,
                          PHYSICAL_ADDRESS physFIFO,
                          ULONG cbFIFO,
                          PHYSICAL_ADDRESS physIO,
                          ULONG cbIO);
void SvgaAdapterStop(PVBOXWDDM_EXT_VMSVGA pSvga,
                     DXGKRNL_INTERFACE *pDxgkInterface);

NTSTATUS SvgaQueryInfo(PVBOXWDDM_EXT_VMSVGA pSvga,
                       VBOXGAHWINFOSVGA *pSvgaInfo);

NTSTATUS SvgaScreenDefine(PVBOXWDDM_EXT_VMSVGA pSvga,
                          uint32_t u32Offset,
                          uint32_t u32ScreenId,
                          int32_t xOrigin,
                          int32_t yOrigin,
                          uint32_t u32Width,
                          uint32_t u32Height,
                          bool fBlank);
NTSTATUS SvgaScreenDestroy(PVBOXWDDM_EXT_VMSVGA pSvga,
                           uint32_t u32ScreenId);

NTSTATUS SvgaContextCreate(PVBOXWDDM_EXT_VMSVGA pSvga,
                           uint32_t u32Cid);
NTSTATUS SvgaContextDestroy(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t u32Cid);

NTSTATUS SvgaSurfaceCreate(VBOXWDDM_EXT_VMSVGA *pSvga,
                           GASURFCREATE *pCreateParms,
                           GASURFSIZE *paSizes,
                           uint32_t cSizes,
                           uint32_t *pu32Sid);
NTSTATUS SvgaSurfaceUnref(VBOXWDDM_EXT_VMSVGA *pSvga,
                          uint32_t u32Sid);
SURFACEOBJECT *SvgaSurfaceObjectQuery(VBOXWDDM_EXT_VMSVGA *pSvga,
                                      uint32_t u32Sid);
NTSTATUS SvgaSurfaceObjectRelease(SURFACEOBJECT *pSO);

NTSTATUS SvgaFence(PVBOXWDDM_EXT_VMSVGA pSvga,
                   uint32_t u32Fence);
NTSTATUS SvgaSurfaceDefine(PVBOXWDDM_EXT_VMSVGA pSvga,
                           GASURFCREATE const *pCreateParms,
                           GASURFSIZE const *paSizes,
                           uint32_t cSizes,
                           uint32_t u32Sid);
NTSTATUS SvgaSurfaceDestroy(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t u32Sid);

NTSTATUS SvgaContextIdAlloc(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t *pu32Cid);
NTSTATUS SvgaContextIdFree(PVBOXWDDM_EXT_VMSVGA pSvga,
                           uint32_t u32Cid);
NTSTATUS SvgaSurfaceIdAlloc(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t *pu32Sid);
NTSTATUS SvgaSurfaceIdFree(PVBOXWDDM_EXT_VMSVGA pSvga,
                           uint32_t u32Sid);
NTSTATUS SvgaGMRIdAlloc(PVBOXWDDM_EXT_VMSVGA pSvga,
                        uint32_t *pu32GMRId);
NTSTATUS SvgaGMRIdFree(PVBOXWDDM_EXT_VMSVGA pSvga,
                       uint32_t u32GMRId);


NTSTATUS SvgaRenderCommands(PVBOXWDDM_EXT_VMSVGA pSvga,
                            void *pvTarget,
                            uint32_t cbTarget,
                            const void *pvSource,
                            uint32_t cbSource,
                            uint32_t *pu32TargetLength,
                            uint32_t *pu32ProcessedLength,
                            GAHWRENDERDATA **ppHwRenderData);
NTSTATUS SvgaRenderComplete(PVBOXWDDM_EXT_VMSVGA pSvga,
                            GAHWRENDERDATA *pHwRenderData);

NTSTATUS SvgaSharedSidInsert(VBOXWDDM_EXT_VMSVGA *pSvga,
                             uint32_t u32Sid,
                             uint32_t u32SharedSid);
NTSTATUS SvgaSharedSidRemove(VBOXWDDM_EXT_VMSVGA *pSvga,
                             uint32_t u32Sid);
uint32_t SvgaSharedSidGet(VBOXWDDM_EXT_VMSVGA *pSvga,
                          uint32_t u32Sid);

NTSTATUS SvgaGenPresent(uint32_t u32Sid,
                        uint32_t u32Width,
                        uint32_t u32Height,
                        void *pvDst,
                        uint32_t cbDst,
                        uint32_t *pcbOut);
NTSTATUS SvgaPresent(PVBOXWDDM_EXT_VMSVGA pSvga,
                     uint32_t u32Sid,
                     uint32_t u32Width,
                     uint32_t u32Height);

NTSTATUS SvgaGenPresentVRAM(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t u32Sid,
                            uint32_t u32Width,
                            uint32_t u32Height,
                            uint32_t u32VRAMOffset,
                            void *pvDst,
                            uint32_t cbDst,
                            uint32_t *pcbOut);
NTSTATUS SvgaPresentVRAM(PVBOXWDDM_EXT_VMSVGA pSvga,
                         uint32_t u32Sid,
                         uint32_t u32Width,
                         uint32_t u32Height,
                         uint32_t u32VRAMOffset);

NTSTATUS SvgaGenSurfaceDMA(PVBOXWDDM_EXT_VMSVGA pSvga,
                           SVGAGuestImage const *pGuestImage,
                           SVGA3dSurfaceImageId const *pSurfId,
                           SVGA3dTransferType enmTransferType, uint32_t xSrc, uint32_t ySrc,
                           uint32_t xDst, uint32_t yDst, uint32_t cWidth, uint32_t cHeight,
                           void *pvDst,
                           uint32_t cbDst,
                           uint32_t *pcbOut);

NTSTATUS SvgaGenBlitGMRFBToScreen(PVBOXWDDM_EXT_VMSVGA pSvga,
                                  uint32_t idDstScreen,
                                  int32_t xSrc,
                                  int32_t ySrc,
                                  RECT const *pDstRect,
                                  void *pvDst,
                                  uint32_t cbDst,
                                  uint32_t *pcbOut);
NTSTATUS SvgaGenBlitScreenToGMRFB(PVBOXWDDM_EXT_VMSVGA pSvga,
                                  uint32_t idSrcScreen,
                                  int32_t xSrc,
                                  int32_t ySrc,
                                  RECT const *pDstRect,
                                  void *pvDst,
                                  uint32_t cbDst,
                                  uint32_t *pcbOut);

NTSTATUS SvgaBlitGMRFBToScreen(PVBOXWDDM_EXT_VMSVGA pSvga,
                               uint32_t idDstScreen,
                               int32_t xSrc,
                               int32_t ySrc,
                               RECT const *pDstRect);

NTSTATUS SvgaGenBlitSurfaceToScreen(PVBOXWDDM_EXT_VMSVGA pSvga,
                                    uint32_t sid,
                                    RECT const *pSrcRect,
                                    uint32 idDstScreen,
                                    RECT const *pDstRect,
                                    uint32_t cDstClipRects,
                                    RECT const *paDstClipRects,
                                    void *pvDst,
                                    uint32_t cbDst,
                                    uint32_t *pcbOut,
                                    uint32_t *pcOutDstClipRects);

NTSTATUS SvgaUpdate(PVBOXWDDM_EXT_VMSVGA pSvga,
                    uint32_t u32X,
                    uint32_t u32Y,
                    uint32_t u32Width,
                    uint32_t u32Height);

NTSTATUS SvgaDefineCursor(PVBOXWDDM_EXT_VMSVGA pSvga,
                          uint32_t u32HotspotX,
                          uint32_t u32HotspotY,
                          uint32_t u32Width,
                          uint32_t u32Height,
                          uint32_t u32AndMaskDepth,
                          uint32_t u32XorMaskDepth,
                          void const *pvAndMask,
                          uint32_t cbAndMask,
                          void const *pvXorMask,
                          uint32_t cbXorMask);

NTSTATUS SvgaDefineAlphaCursor(PVBOXWDDM_EXT_VMSVGA pSvga,
                               uint32_t u32HotspotX,
                               uint32_t u32HotspotY,
                               uint32_t u32Width,
                               uint32_t u32Height,
                               void const *pvImage,
                               uint32_t cbImage);

NTSTATUS SvgaGenDefineGMRFB(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t u32Offset,
                            uint32_t u32BytesPerLine,
                            void *pvDst,
                            uint32_t cbDst,
                            uint32_t *pcbOut);

NTSTATUS SvgaDefineGMRFB(PVBOXWDDM_EXT_VMSVGA pSvga,
                         uint32_t u32Offset,
                         uint32_t u32BytesPerLine,
                         bool fForce);

NTSTATUS SvgaGenGMRReport(PVBOXWDDM_EXT_VMSVGA pSvga,
                          uint32_t u32GmrId,
                          SVGARemapGMR2Flags fRemapGMR2Flags,
                          uint32_t u32NumPages,
                          RTHCPHYS *paPhysAddresses,
                          void *pvDst,
                          uint32_t cbDst,
                          uint32_t *pcbOut);
NTSTATUS SvgaGMRReport(PVBOXWDDM_EXT_VMSVGA pSvga,
                       uint32_t u32GmrId,
                       SVGARemapGMR2Flags fRemapGMR2Flags,
                       uint32_t u32NumPages,
                       RTHCPHYS *paPhysAddresses);

NTSTATUS SvgaRegionCreate(VBOXWDDM_EXT_VMSVGA *pSvga,
                          void *pvOwner,
                          uint32_t u32NumPages,
                          uint32_t *pu32GmrId,
                          uint64_t *pu64UserAddress);
NTSTATUS SvgaRegionDestroy(VBOXWDDM_EXT_VMSVGA *pSvga,
                           uint32_t u32GmrId);
void SvgaRegionsDestroy(VBOXWDDM_EXT_VMSVGA *pSvga,
                        void *pvOwner);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_Svga_h */
