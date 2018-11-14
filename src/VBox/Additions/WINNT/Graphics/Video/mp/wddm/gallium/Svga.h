/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver VMSVGA.
 */

/*
 * Copyright (C) 2016-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___Svga_h__
#define ___Svga_h__

#include "VBoxMPGaUtils.h"

#include <iprt/assert.h>
#include <iprt/avl.h>

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

    /** Fifo state. */
    VMSVGAFIFO fifo;

    /** Offset of the screen relative to VRAM start. */
    uint32_t u32ScreenOffset;

    /** Scanline width in bytes of the screen. */
    uint32_t cbScreenPitch;

    /** For atomic hardware access. */
    KSPIN_LOCK HwSpinLock;

    /** AVL tree for mapping GMR id to the corresponding structure. */
    AVLU32TREE GMRTree;

    /** Bitmap of used GMR ids. Bit 0 - GMR id 0, etc. */
    uint32_t *pu32GMRBits; /* Number of GMRs is controlled by the host (u32GmrMaxIds), so allocte the bitmap. */
    uint32_t cbGMRBits;    /* Bytes allocated for pu32GMRBits */

    /** SVGA data access. */
    FAST_MUTEX SvgaMutex;

    /** AVL tree for mapping sids to the original sid for shared resources. */
    AVLU32TREE SharedSidMap;

    /** Bitmap of used context ids. Bit 0 - context id 0, etc. */
    uint32_t au32ContextBits[(SVGA3D_MAX_CONTEXT_IDS + 31) / 32];

    /** Bitmap of used surface ids. Bit 0 - surface id 0, etc. */
    uint32_t au32SurfaceBits[(SVGA3D_MAX_SURFACE_IDS + 31) / 32];
} VBOXWDDM_EXT_VMSVGA;
typedef struct VBOXWDDM_EXT_VMSVGA *PVBOXWDDM_EXT_VMSVGA;


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
                          uint32_t u32Width,
                          uint32_t u32Height);

NTSTATUS SvgaContextCreate(PVBOXWDDM_EXT_VMSVGA pSvga,
                           uint32_t u32Cid);
NTSTATUS SvgaContextDestroy(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t u32Cid);

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
                            uint32_t *pu32ProcessedLength);

typedef struct GASHAREDSID
{
    /** Key is sid. */
    AVLU32NODECORE Core;
    /** The original sid, the key maps to it. */
    uint32_t u32SharedSid;
} GASHAREDSID;

NTSTATUS SvgaSharedSidInsert(VBOXWDDM_EXT_VMSVGA *pSvga,
                             GASHAREDSID *pNode,
                             uint32_t u32Sid,
                             uint32_t u32SharedSid);
GASHAREDSID *SvgaSharedSidRemove(VBOXWDDM_EXT_VMSVGA *pSvga,
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
                            void *pvDst,
                            uint32_t cbDst,
                            uint32_t *pcbOut);
NTSTATUS SvgaPresentVRAM(PVBOXWDDM_EXT_VMSVGA pSvga,
                         uint32_t u32Sid,
                         uint32_t u32Width,
                         uint32_t u32Height);

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

NTSTATUS SvgaGenDefineGMRFB(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t u32Offset,
                            uint32_t u32BytesPerLine,
                            void *pvDst,
                            uint32_t cbDst,
                            uint32_t *pcbOut);

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

#endif
