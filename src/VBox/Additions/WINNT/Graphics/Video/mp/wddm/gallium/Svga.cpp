/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - VMSVGA.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "Svga.h"
#include "SvgaFifo.h"
#include "SvgaHw.h"
#include "SvgaCmd.h"

#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>

static NTSTATUS svgaHwInit(VBOXWDDM_EXT_VMSVGA *pSvga)
{
    pSvga->u32Caps      = SVGARegRead(pSvga, SVGA_REG_CAPABILITIES);
    pSvga->u32VramSize  = SVGARegRead(pSvga, SVGA_REG_VRAM_SIZE);
    pSvga->u32FifoSize  = SVGARegRead(pSvga, SVGA_REG_MEM_SIZE);
    pSvga->u32MaxWidth  = SVGARegRead(pSvga, SVGA_REG_MAX_WIDTH);
    pSvga->u32MaxHeight = SVGARegRead(pSvga, SVGA_REG_MAX_HEIGHT);

    if (pSvga->u32Caps & SVGA_CAP_GMR2)
    {
        pSvga->u32GmrMaxIds   = SVGARegRead(pSvga, SVGA_REG_GMR_MAX_IDS);
        pSvga->u32GmrMaxPages = SVGARegRead(pSvga, SVGA_REG_GMRS_MAX_PAGES);
        pSvga->u32MemorySize  = SVGARegRead(pSvga, SVGA_REG_MEMORY_SIZE);
        pSvga->u32MemorySize -= pSvga->u32VramSize;
    }
    else
    {
        /*
         * An arbitrary limit of 512MiB on surface
         * memory. But all HWV8 hardware supports GMR2.
         */
        /** @todo not supported */
        pSvga->u32MemorySize = 512*1024*1024;
    }

    if (pSvga->u32Caps & SVGA_CAP_GBOBJECTS)
    {
        /** @todo Support when the host device implements this. */
        RT_BREAKPOINT();
    }
    else
    {
        pSvga->u32MaxTextureWidth = 8192;
        pSvga->u32MaxTextureHeight = 8192;
    }

    NTSTATUS Status = SvgaFifoInit(pSvga);
    if (NT_SUCCESS(Status))
    {
        /* Enable SVGA device. */
        SVGARegWrite(pSvga, SVGA_REG_ENABLE, SVGA_REG_ENABLE_ENABLE);
        SVGARegWrite(pSvga, SVGA_REG_IRQMASK, SVGA_IRQFLAG_ANY_FENCE);
    }

    return Status;
}

void SvgaAdapterStop(PVBOXWDDM_EXT_VMSVGA pSvga,
                     DXGKRNL_INTERFACE *pDxgkInterface)
{
    if (pSvga)
    {
        if (pSvga->pu32GMRBits)
        {
            if (pSvga->GMRTree != NULL)
            {
                /* Normally it is expected that all GMRs are freed already. */
                AssertFailed();

                /* Free GMRs. */
                SvgaRegionsDestroy(pSvga, NULL);

            }
            GaMemFree(pSvga->pu32GMRBits);
            pSvga->pu32GMRBits = NULL;
            pSvga->cbGMRBits = 0;
        }

        /* Disable SVGA device. */
        SVGARegWrite(pSvga, SVGA_REG_IRQMASK, 0);
        SVGARegWrite(pSvga, SVGA_REG_ENABLE, SVGA_REG_ENABLE_DISABLE);

        NTSTATUS Status = pDxgkInterface->DxgkCbUnmapMemory(pDxgkInterface->DeviceHandle,
                                                            (PVOID)pSvga->pu32FIFO);
        Assert(Status == STATUS_SUCCESS); RT_NOREF(Status);

        GaMemFree(pSvga);
    }
}

NTSTATUS SvgaAdapterStart(PVBOXWDDM_EXT_VMSVGA *ppSvga,
                          DXGKRNL_INTERFACE *pDxgkInterface,
                          PHYSICAL_ADDRESS physFIFO,
                          ULONG cbFIFO,
                          PHYSICAL_ADDRESS physIO,
                          ULONG cbIO)
{
    RT_NOREF(cbIO);

    NTSTATUS Status;
// ASMBreakpoint();

    VBOXWDDM_EXT_VMSVGA *pSvga = (VBOXWDDM_EXT_VMSVGA *)GaMemAllocZero(sizeof(VBOXWDDM_EXT_VMSVGA));
    if (!pSvga)
        return STATUS_INSUFFICIENT_RESOURCES;

    /* The spinlock is required for hardware access. Init it as the very first. */
    KeInitializeSpinLock(&pSvga->HwSpinLock);
    ExInitializeFastMutex(&pSvga->SvgaMutex);

    /* The port IO address is also needed for hardware access. */
    pSvga->ioportBase = (RTIOPORT)physIO.QuadPart;

    /* FIFO pointer is also needed for hardware access. */
    Status = pDxgkInterface->DxgkCbMapMemory(pDxgkInterface->DeviceHandle,
                 physFIFO,
                 cbFIFO,
                 FALSE, /* IN BOOLEAN InIoSpace */
                 FALSE, /* IN BOOLEAN MapToUserMode */
                 MmNonCached, /* IN MEMORY_CACHING_TYPE CacheType */
                 (PVOID *)&pSvga->pu32FIFO /*OUT PVOID *VirtualAddress*/);

    if (NT_SUCCESS(Status))
    {
        SVGARegWrite(pSvga, SVGA_REG_ID, SVGA_ID_2);
        const uint32_t u32SvgaId = SVGARegRead(pSvga, SVGA_REG_ID);
        if (u32SvgaId == SVGA_ID_2)
        {
            Status = svgaHwInit(pSvga);

            if (NT_SUCCESS(Status))
            {
                /*
                 * Check hardware capabilities.
                 */
                if (pSvga->u32GmrMaxIds > 0)
                {
                    pSvga->GMRTree = NULL;
                    pSvga->cbGMRBits = ((pSvga->u32GmrMaxIds + 31) / 32) * 4; /* 32bit align and 4 bytes per 32 bit. */
                    pSvga->pu32GMRBits = (uint32_t *)GaMemAllocZero(pSvga->cbGMRBits);
                    if (pSvga->pu32GMRBits)
                    {
                        /* Do not use id == 0. */
                        ASMBitSet(pSvga->pu32GMRBits, 0);
                        ASMBitSet(pSvga->au32ContextBits, 0);
                        ASMBitSet(pSvga->au32SurfaceBits, 0);
                    }
                    else
                    {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                    }
                }
            }
        }
        else
        {
            GALOGREL(("SVGA_ID_2 not supported. Device returned %d\n", u32SvgaId));
            Status = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(Status))
    {
        *ppSvga = pSvga;
    }

    return Status;
}

NTSTATUS SvgaQueryInfo(PVBOXWDDM_EXT_VMSVGA pSvga,
                       VBOXGAHWINFOSVGA *pSvgaInfo)
{
    pSvgaInfo->cbInfoSVGA = sizeof(VBOXGAHWINFOSVGA);

    int i;
    for (i = 0; i < RT_ELEMENTS(pSvgaInfo->au32Regs); ++i)
    {
        pSvgaInfo->au32Regs[i] = SVGARegRead(pSvga, i);
    }

    /* Beginning of FIFO. */
    memcpy(pSvgaInfo->au32Fifo, (void *)&pSvga->pu32FIFO[0], sizeof(pSvgaInfo->au32Fifo));

    if (pSvgaInfo->au32Regs[SVGA_REG_CAPABILITIES] & SVGA_CAP_GBOBJECTS)
    {
        for (i = 0; i < RT_ELEMENTS(pSvgaInfo->au32Caps); ++i)
        {
            SVGARegWrite(pSvga, SVGA_REG_DEV_CAP, i);
            pSvgaInfo->au32Caps[i] = SVGARegRead(pSvga, SVGA_REG_DEV_CAP);
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS SvgaScreenDefine(PVBOXWDDM_EXT_VMSVGA pSvga,
                          uint32_t u32Offset,
                          uint32_t u32ScreenId,
                          int32_t xOrigin,
                          int32_t yOrigin,
                          uint32_t u32Width,
                          uint32_t u32Height)
{
    NTSTATUS Status = STATUS_SUCCESS;

    const uint32_t cbSubmit =   sizeof(uint32_t)
                              + sizeof(SVGAScreenObject)
                              + sizeof(uint32_t)
                              + sizeof(SVGAFifoCmdDefineGMRFB);
    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        SvgaCmdDefineScreen(pvCmd, u32ScreenId, true,
                            xOrigin, yOrigin, u32Width, u32Height,
                            /* fPrimary = */ false, u32Offset, /* fBlank = */ false);
        pvCmd = (uint8_t *)pvCmd + sizeof(uint32_t) + sizeof(SVGAScreenObject);

        uint32_t u32BytesPerLine = u32Width * 4;
        /* Start at offset 0, because we use it to address entire VRAM. */
        SvgaCmdDefineGMRFB(pvCmd, 0, u32BytesPerLine);
        SvgaFifoCommit(pSvga, cbSubmit);

        /* Save the screen offset. */
        pSvga->u32ScreenOffset = u32Offset;
        pSvga->cbScreenPitch = u32BytesPerLine;
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

NTSTATUS SvgaScreenDestroy(PVBOXWDDM_EXT_VMSVGA pSvga,
                           uint32_t u32ScreenId)
{
    NTSTATUS Status = STATUS_SUCCESS;

    const uint32_t cbSubmit =   sizeof(uint32_t)
                              + sizeof(SVGAFifoCmdDestroyScreen);
    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        SvgaCmdDestroyScreen(pvCmd, u32ScreenId);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}


NTSTATUS SvgaIdAlloc(PVBOXWDDM_EXT_VMSVGA pSvga,
                     uint32_t *pu32Bits,
                     uint32_t cbBits,
                     uint32_t u32Limit,
                     uint32_t *pu32Id)
{
    NTSTATUS Status;
    ExAcquireFastMutex(&pSvga->SvgaMutex);

    Status = GaIdAlloc(pu32Bits, cbBits, u32Limit, pu32Id);

    ExReleaseFastMutex(&pSvga->SvgaMutex);
    return Status;
}

NTSTATUS SvgaIdFree(PVBOXWDDM_EXT_VMSVGA pSvga,
                    uint32_t *pu32Bits,
                    uint32_t cbBits,
                    uint32_t u32Limit,
                    uint32_t u32Id)
{
    NTSTATUS Status;
    ExAcquireFastMutex(&pSvga->SvgaMutex);

    Status = GaIdFree(pu32Bits, cbBits, u32Limit, u32Id);

    ExReleaseFastMutex(&pSvga->SvgaMutex);
    return Status;
}

NTSTATUS SvgaContextIdAlloc(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t *pu32Cid)
{
    return SvgaIdAlloc(pSvga, pSvga->au32ContextBits, sizeof(pSvga->au32ContextBits),
                       SVGA3D_MAX_CONTEXT_IDS, pu32Cid);
}

NTSTATUS SvgaContextIdFree(PVBOXWDDM_EXT_VMSVGA pSvga,
                           uint32_t u32Cid)
{
    return SvgaIdFree(pSvga, pSvga->au32ContextBits, sizeof(pSvga->au32ContextBits),
                       SVGA3D_MAX_CONTEXT_IDS, u32Cid);
}

NTSTATUS SvgaSurfaceIdAlloc(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t *pu32Sid)
{
    return SvgaIdAlloc(pSvga, pSvga->au32SurfaceBits, sizeof(pSvga->au32SurfaceBits),
                       SVGA3D_MAX_SURFACE_IDS, pu32Sid);
}

NTSTATUS SvgaSurfaceIdFree(PVBOXWDDM_EXT_VMSVGA pSvga,
                           uint32_t u32Sid)
{
    return SvgaIdFree(pSvga, pSvga->au32SurfaceBits, sizeof(pSvga->au32SurfaceBits),
                      SVGA3D_MAX_SURFACE_IDS, u32Sid);
}

NTSTATUS SvgaGMRIdAlloc(PVBOXWDDM_EXT_VMSVGA pSvga,
                        uint32_t *pu32GMRId)
{
    return SvgaIdAlloc(pSvga, pSvga->pu32GMRBits, pSvga->cbGMRBits,
                       pSvga->u32GmrMaxIds, pu32GMRId);
}

NTSTATUS SvgaGMRIdFree(PVBOXWDDM_EXT_VMSVGA pSvga,
                       uint32_t u32GMRId)
{
    return SvgaIdFree(pSvga, pSvga->pu32GMRBits, pSvga->cbGMRBits,
                      pSvga->u32GmrMaxIds, u32GMRId);
}

NTSTATUS SvgaContextCreate(PVBOXWDDM_EXT_VMSVGA pSvga,
                           uint32_t u32Cid)
{
    NTSTATUS Status = STATUS_SUCCESS;

    /*
     * Issue SVGA_3D_CMD_CONTEXT_DEFINE.
     */
    uint32_t cbSubmit =   sizeof(SVGA3dCmdHeader)
                        + sizeof(SVGA3dCmdDefineContext);
    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        Svga3dCmdDefineContext(pvCmd, u32Cid);
        SvgaFifoCommit(pSvga, cbSubmit);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

NTSTATUS SvgaContextDestroy(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t u32Cid)
{
    NTSTATUS Status = STATUS_SUCCESS;

    /*
     * Issue SVGA_3D_CMD_CONTEXT_DESTROY.
     */
    const uint32_t cbSubmit =   sizeof(SVGA3dCmdHeader)
                              + sizeof(SVGA3dCmdDestroyContext);
    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        Svga3dCmdDestroyContext(pvCmd, u32Cid);
        SvgaFifoCommit(pSvga, cbSubmit);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

NTSTATUS SvgaFence(PVBOXWDDM_EXT_VMSVGA pSvga,
                   uint32_t u32Fence)
{
    NTSTATUS Status = STATUS_SUCCESS;

    const uint32_t cbSubmit =  sizeof(uint32_t)
                             + sizeof(SVGAFifoCmdFence);
    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        SvgaCmdFence(pvCmd, u32Fence);

        SvgaFifoCommit(pSvga, cbSubmit);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

NTSTATUS SvgaSurfaceDefine(PVBOXWDDM_EXT_VMSVGA pSvga,
                           GASURFCREATE const *pCreateParms,
                           GASURFSIZE const *paSizes,
                           uint32_t cSizes,
                           uint32_t u32Sid)
{
    NTSTATUS Status = STATUS_SUCCESS;

    /* Size of SVGA_3D_CMD_SURFACE_DEFINE command for this surface. */
    const uint32_t cbSubmit =   sizeof(SVGA3dCmdHeader)
                              + sizeof(SVGA3dCmdDefineSurface)
                              + cSizes * sizeof(SVGA3dSize);

    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        Svga3dCmdDefineSurface(pvCmd, u32Sid, pCreateParms, paSizes, cSizes);
        SvgaFifoCommit(pSvga, cbSubmit);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

NTSTATUS SvgaSurfaceDestroy(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t u32Sid)
{
    NTSTATUS Status = STATUS_SUCCESS;

    const uint32_t cbSubmit =   sizeof(SVGA3dCmdHeader)
                              + sizeof(SVGA3dCmdDestroySurface);
    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        Svga3dCmdDestroySurface(pvCmd, u32Sid);
        SvgaFifoCommit(pSvga, cbSubmit);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

NTSTATUS SvgaSharedSidInsert(VBOXWDDM_EXT_VMSVGA *pSvga,
                             GASHAREDSID *pNode,
                             uint32_t u32Sid,
                             uint32_t u32SharedSid)
{
    ExAcquireFastMutex(&pSvga->SvgaMutex);

    GALOG(("[%p] %d -> shared %d\n", pSvga, u32Sid, u32SharedSid));

    pNode->Core.Key = u32Sid;
    pNode->u32SharedSid = u32SharedSid;
    RTAvlU32Insert(&pSvga->SharedSidMap, &pNode->Core);

    ExReleaseFastMutex(&pSvga->SvgaMutex);
    return STATUS_SUCCESS;
}


GASHAREDSID *SvgaSharedSidRemove(VBOXWDDM_EXT_VMSVGA *pSvga,
                                 uint32_t u32Sid)
{
    ExAcquireFastMutex(&pSvga->SvgaMutex);

    GALOG(("[%p] %d\n", pSvga, u32Sid));
    GASHAREDSID *pNode = (GASHAREDSID *)RTAvlU32Remove(&pSvga->SharedSidMap, u32Sid);

    ExReleaseFastMutex(&pSvga->SvgaMutex);
    return pNode;
}

uint32_t SvgaSharedSidGet(VBOXWDDM_EXT_VMSVGA *pSvga,
                          uint32_t u32Sid)
{
    ExAcquireFastMutex(&pSvga->SvgaMutex);

    GASHAREDSID *p = (GASHAREDSID *)RTAvlU32Get(&pSvga->SharedSidMap, u32Sid);
    if (p)
    {
        GALOG(("%d -> shared %d\n", u32Sid, p->u32SharedSid));
        u32Sid = p->u32SharedSid;
    }

    ExReleaseFastMutex(&pSvga->SvgaMutex);
    return u32Sid;
}

static NTSTATUS svgaUpdateCommand(VBOXWDDM_EXT_VMSVGA *pSvga,
                                  uint32_t u32CmdId,
                                  uint8_t *pu8Cmd,
                                  uint32_t cbCmd)
{
    RT_NOREF4(pSvga, u32CmdId, pu8Cmd, cbCmd);

    NTSTATUS Status = STATUS_SUCCESS;

    const SVGA3dCmdHeader *pHeader = (SVGA3dCmdHeader *)pu8Cmd;
    uint8_t *pCommand = (uint8_t *)&pHeader[1];

    switch (u32CmdId)
    {
        case SVGA_3D_CMD_PRESENT:
        case SVGA_3D_CMD_PRESENT_READBACK:
        {
            SVGA3dCmdPresent *p = (SVGA3dCmdPresent *)pCommand;
            p->sid = SvgaSharedSidGet(pSvga, p->sid);
        } break;
        case SVGA_3D_CMD_SETRENDERTARGET:
        {
             SVGA3dCmdSetRenderTarget *p = (SVGA3dCmdSetRenderTarget *)pCommand;
             p->target.sid = SvgaSharedSidGet(pSvga, p->target.sid);
        } break;
        case SVGA_3D_CMD_SURFACE_COPY:
        {
            SVGA3dCmdSurfaceCopy *p = (SVGA3dCmdSurfaceCopy *)pCommand;
            p->src.sid = SvgaSharedSidGet(pSvga, p->src.sid);
            p->dest.sid = SvgaSharedSidGet(pSvga, p->dest.sid);
        } break;
        case SVGA_3D_CMD_SURFACE_STRETCHBLT:
        {
            SVGA3dCmdSurfaceStretchBlt *p = (SVGA3dCmdSurfaceStretchBlt *)pCommand;
            p->src.sid = SvgaSharedSidGet(pSvga, p->src.sid);
            p->dest.sid = SvgaSharedSidGet(pSvga, p->dest.sid);
        } break;
        case SVGA_3D_CMD_SURFACE_DMA:
        {
            SVGA3dCmdSurfaceDMA *p = (SVGA3dCmdSurfaceDMA *)pCommand;
            p->host.sid = SvgaSharedSidGet(pSvga, p->host.sid);
        } break;
        case SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN:
        {
            SVGA3dCmdBlitSurfaceToScreen *p = (SVGA3dCmdBlitSurfaceToScreen *)pCommand;
            p->srcImage.sid = SvgaSharedSidGet(pSvga, p->srcImage.sid);
        } break;
        case SVGA_3D_CMD_GENERATE_MIPMAPS:
        {
            SVGA3dCmdGenerateMipmaps *p = (SVGA3dCmdGenerateMipmaps *)pCommand;
            p->sid = SvgaSharedSidGet(pSvga, p->sid);
        } break;
        case SVGA_3D_CMD_ACTIVATE_SURFACE:
        {
            SVGA3dCmdActivateSurface *p = (SVGA3dCmdActivateSurface *)pCommand;
            p->sid = SvgaSharedSidGet(pSvga, p->sid);
        } break;
        case SVGA_3D_CMD_DEACTIVATE_SURFACE:
        {
            SVGA3dCmdDeactivateSurface *p = (SVGA3dCmdDeactivateSurface *)pCommand;
            p->sid = SvgaSharedSidGet(pSvga, p->sid);
        } break;
        case SVGA_3D_CMD_SETTEXTURESTATE:
        {
            SVGA3dCmdSetTextureState *p = (SVGA3dCmdSetTextureState *)pCommand;
            uint32_t cStates = (pHeader->size - sizeof(SVGA3dCmdSetTextureState)) / sizeof(SVGA3dTextureState);
            SVGA3dTextureState *pState = (SVGA3dTextureState *)&p[1];
            while (cStates > 0)
            {
                if (pState->name == SVGA3D_TS_BIND_TEXTURE)
                {
                    pState->value = SvgaSharedSidGet(pSvga, pState->value);
                }

                ++pState;
                --cStates;
            }
        } break;

        /** @todo Might need (unlikely) SVGA_3D_CMD_DRAWPRIMITIVES SVGA3dVertexDecl::array::surfaceId */

        /*
         * Unsupported commands, which might include a sid.
         * The VBox VMSVGA device does not implement them and most of them are not used by SVGA driver.
         */
        case SVGA_3D_CMD_SET_VERTEX_STREAMS:
        case SVGA_3D_CMD_LOGICOPS_BITBLT:
        case SVGA_3D_CMD_LOGICOPS_TRANSBLT:
        case SVGA_3D_CMD_LOGICOPS_STRETCHBLT:
        case SVGA_3D_CMD_LOGICOPS_COLORFILL:
        case SVGA_3D_CMD_LOGICOPS_ALPHABLEND:
        case SVGA_3D_CMD_LOGICOPS_CLEARTYPEBLEND:
        case SVGA_3D_CMD_DEFINE_GB_SURFACE:
        case SVGA_3D_CMD_DESTROY_GB_SURFACE:
        case SVGA_3D_CMD_BIND_GB_SURFACE:
        case SVGA_3D_CMD_BIND_GB_SURFACE_WITH_PITCH:
        case SVGA_3D_CMD_COND_BIND_GB_SURFACE:
        case SVGA_3D_CMD_UPDATE_GB_SURFACE:
        case SVGA_3D_CMD_READBACK_GB_SURFACE:
        case SVGA_3D_CMD_INVALIDATE_GB_SURFACE:
        case SVGA_3D_CMD_UPDATE_GB_IMAGE:
        case SVGA_3D_CMD_READBACK_GB_IMAGE:
        case SVGA_3D_CMD_READBACK_GB_IMAGE_PARTIAL:
        case SVGA_3D_CMD_INVALIDATE_GB_IMAGE:
        case SVGA_3D_CMD_INVALIDATE_GB_IMAGE_PARTIAL:
        case SVGA_3D_CMD_BIND_GB_SCREENTARGET:
        case SVGA_3D_CMD_SET_OTABLE_BASE:
        case SVGA_3D_CMD_SET_OTABLE_BASE64:
        case SVGA_3D_CMD_READBACK_OTABLE:
        case SVGA_3D_CMD_DRAW_INDEXED:
            AssertFailed();
            break;
        default:
            if (SVGA_3D_CMD_DX_MIN <= u32CmdId && u32CmdId <= SVGA_3D_CMD_DX_MAX)
            {
                /** @todo  Also do not support DX commands for now, they are not supported by the host. */
                AssertFailed();
            }
            break;
    }
    return Status;
}

/** Copy SVGA commands from pvSource to pvTarget and does the following:
 *    - verifies that all commands are valid;
 *    - tweaks and substitutes command parameters if necessary.
 *
 * Command parameters are changed when:
 *    - a command contains a shared surface id, which will be replaced by the original surface id.
 *
 * @param pSvga .
 * @param pvTarget .
 * @param cbTarget .
 * @param pvSource .
 * @param cbSource .
 * @param pu32TargetLength How many bytes were copied to pvTarget buffer.
 * @param pu32ProcessedLength How many bytes were processed in the pvSource buffer.
 */
NTSTATUS SvgaRenderCommands(PVBOXWDDM_EXT_VMSVGA pSvga,
                            void *pvTarget,
                            uint32_t cbTarget,
                            const void *pvSource,
                            uint32_t cbSource,
                            uint32_t *pu32TargetLength,
                            uint32_t *pu32ProcessedLength)
{
    /* All commands consist of 32 bit dwords. */
    AssertReturn(cbSource % sizeof(uint32_t) == 0, STATUS_ILLEGAL_INSTRUCTION);

    NTSTATUS Status = STATUS_SUCCESS;

    const uint8_t *pu8Src = (uint8_t *)pvSource;
    const uint8_t *pu8SrcEnd = (uint8_t *)pvSource + cbSource;
    uint8_t *pu8Dst = (uint8_t *)pvTarget;
    uint8_t *pu8DstEnd = (uint8_t *)pvTarget + cbTarget;

    while (pu8SrcEnd > pu8Src)
    {
        const uint32_t cbSrcLeft = pu8SrcEnd - pu8Src;
        AssertBreakStmt(cbSrcLeft >= sizeof(uint32_t), Status = STATUS_ILLEGAL_INSTRUCTION);

        /* Get the command id and command length. */
        const uint32_t u32CmdId = *(uint32_t *)pu8Src;
        uint32_t cbCmd = 0;

        if (SVGA_3D_CMD_BASE <= u32CmdId && u32CmdId < SVGA_3D_CMD_MAX)
        {
            AssertBreakStmt(cbSrcLeft >= sizeof(SVGA3dCmdHeader), Status = STATUS_ILLEGAL_INSTRUCTION);

            const SVGA3dCmdHeader *pHeader = (SVGA3dCmdHeader *)pu8Src;
            cbCmd = sizeof(SVGA3dCmdHeader) + pHeader->size;
            AssertBreakStmt(cbCmd % sizeof(uint32_t) == 0, Status = STATUS_ILLEGAL_INSTRUCTION);
            AssertBreakStmt(cbSrcLeft >= cbCmd, Status = STATUS_ILLEGAL_INSTRUCTION);
        }
        else
        {
            /* It is not expected that any of common SVGA commands will be in the command buffer
             * because the SVGA gallium driver does not use them.
             */
            AssertBreakStmt(0, Status = STATUS_ILLEGAL_INSTRUCTION);
        }

        const uint32_t cbDstLeft = pu8DstEnd - pu8Dst;
        if (cbCmd > cbDstLeft)
        {
            Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
            break;
        }

        memcpy(pu8Dst, pu8Src, cbCmd);

        /* Update the command in dst place if necessary. */
        Status = svgaUpdateCommand(pSvga, u32CmdId, pu8Dst, cbCmd);
        AssertBreak(Status == STATUS_SUCCESS);

        pu8Src += cbCmd;
        pu8Dst += cbCmd;
    }

    *pu32TargetLength = pu8Dst - (uint8_t *)pvTarget;
    *pu32ProcessedLength = pu8Src - (uint8_t *)pvSource;

    return Status;
}

NTSTATUS SvgaGenPresent(uint32_t u32Sid,
                        uint32_t u32Width,
                        uint32_t u32Height,
                        void *pvDst,
                        uint32_t cbDst,
                        uint32_t *pcbOut)
{
    const uint32_t cbRequired =   sizeof(SVGA3dCmdHeader)
                                + sizeof(SVGA3dCmdPresent)
                                + sizeof(SVGA3dCopyRect);
    if (pcbOut)
    {
        *pcbOut = cbRequired;
    }

    if (cbDst < cbRequired)
    {
        return STATUS_BUFFER_OVERFLOW;
    }

    Svga3dCmdPresent(pvDst, u32Sid, u32Width, u32Height);

    return STATUS_SUCCESS;
}

NTSTATUS SvgaPresent(PVBOXWDDM_EXT_VMSVGA pSvga,
                     uint32_t u32Sid,
                     uint32_t u32Width,
                     uint32_t u32Height)
{
    NTSTATUS Status = STATUS_SUCCESS;

    uint32_t cbSubmit = 0;
    SvgaGenPresent(0, 0, 0, NULL, 0, &cbSubmit);

    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        SvgaGenPresent(u32Sid, u32Width, u32Height, pvCmd, cbSubmit, NULL);
        SvgaFifoCommit(pSvga, cbSubmit);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

NTSTATUS SvgaGenPresentVRAM(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t u32Sid,
                            uint32_t u32Width,
                            uint32_t u32Height,
                            void *pvDst,
                            uint32_t cbDst,
                            uint32_t *pcbOut)
{
    const uint32_t cbCmdSurfaceDMAToFB =   sizeof(SVGA3dCmdHeader)
                                         + sizeof(SVGA3dCmdSurfaceDMA)
                                         + sizeof(SVGA3dCopyBox)
                                         + sizeof(SVGA3dCmdSurfaceDMASuffix);
    const uint32_t cbCmdUpdate =  sizeof(uint32_t)
                                + sizeof(SVGAFifoCmdUpdate);

    const uint32_t cbRequired = cbCmdSurfaceDMAToFB + cbCmdUpdate;
    if (pcbOut)
    {
        *pcbOut = cbRequired;
    }

    if (cbDst < cbRequired)
    {
        return STATUS_BUFFER_OVERFLOW;
    }

    Svga3dCmdSurfaceDMAToFB(pvDst, u32Sid, u32Width, u32Height, pSvga->u32ScreenOffset);
    SvgaCmdUpdate((uint8_t *)pvDst + cbCmdSurfaceDMAToFB, 0, 0, u32Width, u32Height);

    return STATUS_SUCCESS;
}

NTSTATUS SvgaPresentVRAM(PVBOXWDDM_EXT_VMSVGA pSvga,
                         uint32_t u32Sid,
                         uint32_t u32Width,
                         uint32_t u32Height)
{
    NTSTATUS Status = STATUS_SUCCESS;

    uint32_t cbSubmit = 0;
    SvgaGenPresentVRAM(pSvga, 0, 0, 0, NULL, 0, &cbSubmit);

    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        Status = SvgaGenPresentVRAM(pSvga, u32Sid, u32Width, u32Height, pvCmd, cbSubmit, NULL);
        Assert(Status == STATUS_SUCCESS);
        SvgaFifoCommit(pSvga, cbSubmit);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

NTSTATUS SvgaGenSurfaceDMA(PVBOXWDDM_EXT_VMSVGA pSvga,
                           SVGAGuestImage const *pGuestImage,
                           SVGA3dSurfaceImageId const *pSurfId,
                           SVGA3dTransferType enmTransferType, uint32_t xSrc, uint32_t ySrc,
                           uint32_t xDst, uint32_t yDst, uint32_t cWidth, uint32_t cHeight,
                           void *pvDst,
                           uint32_t cbDst,
                           uint32_t *pcbOut)
{
    RT_NOREF(pSvga);

    const uint32_t cbCmdSurfaceDMA =   sizeof(SVGA3dCmdHeader)
                                     + sizeof(SVGA3dCmdSurfaceDMA)
                                     + sizeof(SVGA3dCopyBox)
                                     + sizeof(SVGA3dCmdSurfaceDMASuffix);

    const uint32_t cbRequired = cbCmdSurfaceDMA;
    if (pcbOut)
    {
        *pcbOut = cbRequired;
    }

    if (cbDst < cbRequired)
    {
        return STATUS_BUFFER_OVERFLOW;
    }

    Svga3dCmdSurfaceDMA(pvDst, pGuestImage, pSurfId, enmTransferType,
                        xSrc, ySrc, xDst, yDst, cWidth, cHeight);

    return STATUS_SUCCESS;
}

NTSTATUS SvgaGenBlitGMRFBToScreen(PVBOXWDDM_EXT_VMSVGA pSvga,
                                  uint32_t idDstScreen,
                                  int32_t xSrc,
                                  int32_t ySrc,
                                  RECT const *pDstRect,
                                  void *pvDst,
                                  uint32_t cbDst,
                                  uint32_t *pcbOut)
{
    RT_NOREF(pSvga);

    const uint32_t cbRequired =   sizeof(uint32_t)
                                + sizeof(SVGAFifoCmdBlitGMRFBToScreen);
    if (pcbOut)
    {
        *pcbOut = cbRequired;
    }

    if (cbDst < cbRequired)
    {
        return STATUS_BUFFER_OVERFLOW;
    }

    SvgaCmdBlitGMRFBToScreen(pvDst, idDstScreen, xSrc, ySrc,
                             pDstRect->left, pDstRect->top,
                             pDstRect->right, pDstRect->bottom);

    return STATUS_SUCCESS;
}

NTSTATUS SvgaGenBlitScreenToGMRFB(PVBOXWDDM_EXT_VMSVGA pSvga,
                                  uint32_t idSrcScreen,
                                  int32_t xSrc,
                                  int32_t ySrc,
                                  RECT const *pDstRect,
                                  void *pvDst,
                                  uint32_t cbDst,
                                  uint32_t *pcbOut)
{
    RT_NOREF(pSvga);

    const uint32_t cbRequired =   sizeof(uint32_t)
                                + sizeof(SVGAFifoCmdBlitScreenToGMRFB);
    if (pcbOut)
    {
        *pcbOut = cbRequired;
    }

    if (cbDst < cbRequired)
    {
        return STATUS_BUFFER_OVERFLOW;
    }

    SvgaCmdBlitScreenToGMRFB(pvDst, idSrcScreen, xSrc, ySrc,
                             pDstRect->left, pDstRect->top,
                             pDstRect->right, pDstRect->bottom);

    return STATUS_SUCCESS;
}

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
                                    uint32_t *pcOutDstClipRects)
{
    RT_NOREF(pSvga);

    uint32_t const cbCmd =  sizeof(SVGA3dCmdHeader)
                          + sizeof(SVGA3dCmdBlitSurfaceToScreen);

    /* How many rectangles can fit into the buffer. */
    uint32_t const cMaxDstClipRects = cbDst >= cbCmd ? (cbDst - cbCmd) / sizeof(SVGASignedRect): 0;

    /* How many should be put into the buffer. */
    uint32_t const cOutDstClipRects = RT_MIN(cDstClipRects, cMaxDstClipRects);

    if (pcOutDstClipRects)
    {
        *pcOutDstClipRects = cOutDstClipRects;
    }

    /* Check if the command does not fit in any case. */
    if (   cbDst < cbCmd
        || (cDstClipRects > 0 && cOutDstClipRects == 0))
    {
        /* Command would not fit or no rectangles would fit. */
        if (pcbOut)
        {
            /* Return full size required for the command and ALL rectangles. */
            *pcbOut = cbCmd + cDstClipRects * sizeof(SVGASignedRect);
        }

        return STATUS_BUFFER_OVERFLOW;
    }

    /* Put as many rectangles as possible. */
    if (pcbOut)
    {
        /* Return full size required for the command and ALL rectangles. */
        *pcbOut = cbCmd + cOutDstClipRects * sizeof(SVGASignedRect);
    }

    Svga3dCmdBlitSurfaceToScreen(pvDst, sid, pSrcRect, idDstScreen, pDstRect, cOutDstClipRects, paDstClipRects);

    return STATUS_SUCCESS;
}


NTSTATUS SvgaUpdate(PVBOXWDDM_EXT_VMSVGA pSvga,
                    uint32_t u32X,
                    uint32_t u32Y,
                    uint32_t u32Width,
                    uint32_t u32Height)
{
    NTSTATUS Status = STATUS_SUCCESS;

    uint32_t cbSubmit =  sizeof(uint32_t)
                       + sizeof(SVGAFifoCmdUpdate);

    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        /** @todo Multi-monitor. */
        SvgaCmdUpdate(pvCmd, u32X, u32Y, u32Width, u32Height);
        SvgaFifoCommit(pSvga, cbSubmit);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

NTSTATUS SvgaGenDefineGMRFB(PVBOXWDDM_EXT_VMSVGA pSvga,
                            uint32_t u32Offset,
                            uint32_t u32BytesPerLine,
                            void *pvDst,
                            uint32_t cbDst,
                            uint32_t *pcbOut)
{
    RT_NOREF(pSvga);

    const uint32_t cbCmd =   sizeof(uint32_t)
                           + sizeof(SVGAFifoCmdDefineGMRFB);

    const uint32_t cbRequired = cbCmd;
    if (pcbOut)
    {
        *pcbOut = cbRequired;
    }

    if (cbDst < cbRequired)
    {
        return STATUS_BUFFER_OVERFLOW;
    }

    SvgaCmdDefineGMRFB(pvDst, u32Offset, u32BytesPerLine);

    return STATUS_SUCCESS;
}

NTSTATUS SvgaDefineGMRFB(PVBOXWDDM_EXT_VMSVGA pSvga,
                         uint32_t u32Offset,
                         uint32_t u32BytesPerLine)
{
    NTSTATUS Status = STATUS_SUCCESS;

    uint32_t cbSubmit = 0;
    SvgaGenDefineGMRFB(pSvga, u32Offset, u32BytesPerLine,
                       NULL, 0, &cbSubmit);

    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        Status = SvgaGenDefineGMRFB(pSvga, u32Offset, u32BytesPerLine,
                                    pvCmd, cbSubmit, NULL);
        Assert(Status == STATUS_SUCCESS);
        SvgaFifoCommit(pSvga, cbSubmit);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

NTSTATUS SvgaGenGMRReport(PVBOXWDDM_EXT_VMSVGA pSvga,
                          uint32_t u32GmrId,
                          SVGARemapGMR2Flags fRemapGMR2Flags,
                          uint32_t u32NumPages,
                          RTHCPHYS *paPhysAddresses,
                          void *pvDst,
                          uint32_t cbDst,
                          uint32_t *pcbOut)
{
    /*
     * SVGA_CMD_DEFINE_GMR2 + SVGA_CMD_REMAP_GMR2.
     */

    AssertReturn(u32NumPages <= pSvga->u32GmrMaxPages, STATUS_INVALID_PARAMETER);

    NTSTATUS Status = STATUS_SUCCESS;

    uint32_t const cbCmdDefineGMR2 =   sizeof(uint32_t)
                                     + sizeof(SVGAFifoCmdDefineGMR2);
    uint32_t const cbCmdRemapGMR2 =   sizeof(uint32_t)
                                    + sizeof(SVGAFifoCmdRemapGMR2);
    uint32_t const cbPPN = (fRemapGMR2Flags & SVGA_REMAP_GMR2_PPN64) ? sizeof(uint64_t) : sizeof(uint32_t);
    uint32_t const cbPPNArray = u32NumPages * cbPPN;

    uint32_t const cbCmd = cbCmdDefineGMR2 + cbCmdRemapGMR2 + cbPPNArray;
    if (pcbOut)
        *pcbOut = cbCmd;

    if (cbCmd <= cbDst)
    {
        uint8_t *pu8Dst = (uint8_t *)pvDst;

        SvgaCmdDefineGMR2(pu8Dst, u32GmrId, u32NumPages);
        pu8Dst += cbCmdDefineGMR2;

        SvgaCmdRemapGMR2(pu8Dst, u32GmrId, fRemapGMR2Flags, 0, u32NumPages);
        pu8Dst += cbCmdRemapGMR2;

        uint32_t iPage;
        if (fRemapGMR2Flags & SVGA_REMAP_GMR2_PPN64)
        {
           uint64_t *paPPN64 = (uint64_t *)pu8Dst;
           for (iPage = 0; iPage < u32NumPages; ++iPage)
           {
               RTHCPHYS const Phys = paPhysAddresses[iPage];
               paPPN64[iPage] = Phys >> PAGE_SHIFT;
           }
        }
        else
        {
           uint32_t *paPPN32 = (uint32_t *)pu8Dst;
           for (iPage = 0; iPage < u32NumPages; ++iPage)
           {
               RTHCPHYS const Phys = paPhysAddresses[iPage];
               AssertBreakStmt((Phys & UINT32_C(0xFFFFFFFF)) == Phys, Status = STATUS_INVALID_PARAMETER);
               paPPN32[iPage] = (uint32_t)(Phys >> PAGE_SHIFT);
           }
        }
    }
    else
        Status = STATUS_BUFFER_OVERFLOW;

    return Status;
}

NTSTATUS SvgaGMRReport(PVBOXWDDM_EXT_VMSVGA pSvga,
                       uint32_t u32GmrId,
                       SVGARemapGMR2Flags fRemapGMR2Flags,
                       uint32_t u32NumPages,
                       RTHCPHYS *paPhysAddresses)
{
    /*
     * Issue SVGA_CMD_DEFINE_GMR2 + SVGA_CMD_REMAP_GMR2.
     */

    NTSTATUS Status;

    uint32_t cbSubmit = 0;
    SvgaGenGMRReport(pSvga, u32GmrId, fRemapGMR2Flags, u32NumPages, paPhysAddresses,
                     NULL, 0, &cbSubmit);

    void *pvCmd = SvgaFifoReserve(pSvga, cbSubmit);
    if (pvCmd)
    {
        Status = SvgaGenGMRReport(pSvga, u32GmrId, fRemapGMR2Flags, u32NumPages, paPhysAddresses,
                                  pvCmd, cbSubmit, NULL);
        AssertStmt(Status == STATUS_SUCCESS, cbSubmit = 0);
        SvgaFifoCommit(pSvga, cbSubmit);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

/* SVGA Guest Memory Region (GMR). Memory known for both host and guest.
 * There can be many (hundreds) of regions, therefore use AVL tree.
 */
typedef struct GAWDDMREGION
{
    /* Key is GMR id (equal to u32GmrId). */
    AVLU32NODECORE Core;
    /* Device the GMR is associated with. */
    void      *pvOwner;
    /* The memory object handle. */
    RTR0MEMOBJ MemObj;
    /* The ring-3 mapping memory object handle. */
    RTR0MEMOBJ MapObjR3;
    RTR0PTR    pvR0;
    RTR3PTR    pvR3;
    /* The Guest Memory Region ID. */
    uint32_t   u32GmrId;
    /* The allocated size in pages. */
    uint32_t   u32NumPages;
    /* Physical addresses of the pages. */
    RTHCPHYS   aPhys[1];
} GAWDDMREGION;

static void gmrFree(GAWDDMREGION *pRegion)
{
    if (pRegion->MapObjR3 != NIL_RTR0MEMOBJ)
    {
        int rc = RTR0MemObjFree(pRegion->MapObjR3, false);
        AssertRC(rc);
        pRegion->MapObjR3 = NIL_RTR0MEMOBJ;
    }
    if (pRegion->MemObj != NIL_RTR0MEMOBJ)
    {
        int rc = RTR0MemObjFree(pRegion->MemObj, true /* fFreeMappings */);
        AssertRC(rc);
        pRegion->MemObj = NIL_RTR0MEMOBJ;
    }
}

static NTSTATUS gmrAlloc(GAWDDMREGION *pRegion)
{
    int rc = RTR0MemObjAllocLowTag(&pRegion->MemObj, pRegion->u32NumPages << PAGE_SHIFT,
                                   false /* executable R0 mapping */, "WDDMGA");
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        rc = RTR0MemObjMapUser(&pRegion->MapObjR3, pRegion->MemObj, (RTR3PTR)-1, 0,
                               RTMEM_PROT_WRITE | RTMEM_PROT_READ, NIL_RTR0PROCESS);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            pRegion->pvR0 = RTR0MemObjAddress(pRegion->MemObj);
            pRegion->pvR3 = RTR0MemObjAddressR3(pRegion->MapObjR3);

            uint32_t iPage;
            for (iPage = 0; iPage < pRegion->u32NumPages; ++iPage)
            {
                pRegion->aPhys[iPage] = RTR0MemObjGetPagePhysAddr(pRegion->MemObj, iPage);
            }

            return STATUS_SUCCESS;
        }

        int rc2 = RTR0MemObjFree(pRegion->MemObj, false);
        AssertRC(rc2);
    }

    return STATUS_INSUFFICIENT_RESOURCES;
}

static void gaRegionFree(VBOXWDDM_EXT_VMSVGA *pSvga, GAWDDMREGION *pRegion)
{
    Assert(pRegion);
    gmrFree(pRegion);
    SvgaGMRIdFree(pSvga, pRegion->u32GmrId);
    GaMemFree(pRegion);
}

typedef struct GAREGIONSDESTROYCTX
{
    void *pvOwner;
    uint32_t cMaxIds;
    uint32_t cIds;
    uint32_t au32Ids[1]; /* [cMaxIds] */
} GAREGIONSDESTROYCTX;

static DECLCALLBACK(int) gaRegionsDestroyCb(PAVLU32NODECORE pNode, void *pvUser)
{
    GAWDDMREGION *pRegion = (GAWDDMREGION *)pNode;
    GAREGIONSDESTROYCTX *pCtx = (GAREGIONSDESTROYCTX *)pvUser;

    if (   pCtx->pvOwner == NULL
        || (uintptr_t)pCtx->pvOwner == (uintptr_t)pRegion->pvOwner)
    {
        AssertReturn(pCtx->cIds < pCtx->cMaxIds, -1);
        pCtx->au32Ids[pCtx->cIds++] = pRegion->u32GmrId;
    }
    return 0;
}

void SvgaRegionsDestroy(VBOXWDDM_EXT_VMSVGA *pSvga,
                        void *pvOwner)
{
    const uint32_t cbCtx = RT_UOFFSETOF(GAREGIONSDESTROYCTX, au32Ids) + pSvga->u32GmrMaxIds * sizeof(uint32_t);
    GAREGIONSDESTROYCTX *pCtx = (GAREGIONSDESTROYCTX *)GaMemAlloc(cbCtx);
    AssertReturnVoid(pCtx);

    pCtx->pvOwner = pvOwner;
    pCtx->cMaxIds = pSvga->u32GmrMaxIds;
    pCtx->cIds = 0;

    ExAcquireFastMutex(&pSvga->SvgaMutex);
    /* Fetch GMR ids associated with this device. */
    RTAvlU32DoWithAll(&pSvga->GMRTree, 0, gaRegionsDestroyCb, pCtx);
    ExReleaseFastMutex(&pSvga->SvgaMutex);

    /* Free all found GMRs. */
    uint32_t i;
    for (i = 0; i < pCtx->cIds; ++i)
    {
        ExAcquireFastMutex(&pSvga->SvgaMutex);
        GAWDDMREGION *pRegion = (GAWDDMREGION *)RTAvlU32Remove(&pSvga->GMRTree, pCtx->au32Ids[i]);
        ExReleaseFastMutex(&pSvga->SvgaMutex);

        if (pRegion)
        {
            Assert(pRegion->u32GmrId == pCtx->au32Ids[i]);
            GALOG(("Deallocate gmrId %d, pv %p, aPhys[0] %RHp\n",
                   pRegion->u32GmrId, pRegion->pvR3, pRegion->aPhys[0]));

            gaRegionFree(pSvga, pRegion);
        }
    }

    GaMemFree(pCtx);
}

NTSTATUS SvgaRegionDestroy(VBOXWDDM_EXT_VMSVGA *pSvga,
                           uint32_t u32GmrId)
{
    AssertReturn(u32GmrId <= pSvga->u32GmrMaxIds, STATUS_INVALID_PARAMETER);

    GALOG(("[%p] gmrId %d\n", pSvga, u32GmrId));

    ExAcquireFastMutex(&pSvga->SvgaMutex);

    GAWDDMREGION *pRegion = (GAWDDMREGION *)RTAvlU32Remove(&pSvga->GMRTree, u32GmrId);

    ExReleaseFastMutex(&pSvga->SvgaMutex);

    if (pRegion)
    {
        Assert(pRegion->u32GmrId == u32GmrId);
        GALOG(("Freed gmrId %d, pv %p, aPhys[0] %RHp\n",
               pRegion->u32GmrId, pRegion->pvR3, pRegion->aPhys[0]));
        gaRegionFree(pSvga, pRegion);
        return STATUS_SUCCESS;
    }

    AssertFailed();
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS SvgaRegionCreate(VBOXWDDM_EXT_VMSVGA *pSvga,
                          void *pvOwner,
                          uint32_t u32NumPages,
                          uint32_t *pu32GmrId,
                          uint64_t *pu64UserAddress)
{
    AssertReturn(u32NumPages > 0 && u32NumPages <= pSvga->u32GmrMaxPages, STATUS_INVALID_PARAMETER);

    GALOG(("[%p] %d pages\n", pSvga, u32NumPages));

    NTSTATUS Status;

    const uint32_t cbAlloc = RT_UOFFSETOF(GAWDDMREGION, aPhys) + u32NumPages * sizeof(RTHCPHYS);
    GAWDDMREGION *pRegion = (GAWDDMREGION *)GaMemAllocZero(cbAlloc);
    if (pRegion)
    {
        Status = SvgaGMRIdAlloc(pSvga, &pRegion->u32GmrId);
        Assert(NT_SUCCESS(Status));
        if (NT_SUCCESS(Status))
        {
            if (pRegion->u32GmrId < pSvga->u32GmrMaxIds)
            {
                pRegion->pvOwner = pvOwner;
                pRegion->u32NumPages = u32NumPages;
                pRegion->MemObj = NIL_RTR0MEMOBJ;
                pRegion->MapObjR3 = NIL_RTR0MEMOBJ;

                Status = gmrAlloc(pRegion);
                Assert(NT_SUCCESS(Status));
                if (NT_SUCCESS(Status))
                {
                    GALOG(("Allocated gmrId %d, pv %p, aPhys[0] %RHp\n",
                           pRegion->u32GmrId, pRegion->pvR3, pRegion->aPhys[0]));

                    /* Report the GMR to the host vmsvga device. */
                    Status = SvgaGMRReport(pSvga,
                                           pRegion->u32GmrId,
                                           SVGA_REMAP_GMR2_PPN32,
                                           pRegion->u32NumPages,
                                           &pRegion->aPhys[0]);
                    Assert(NT_SUCCESS(Status));
                    if (NT_SUCCESS(Status))
                    {
                        /* Add to the container. */
                        ExAcquireFastMutex(&pSvga->SvgaMutex);

                        pRegion->Core.Key = pRegion->u32GmrId;
                        RTAvlU32Insert(&pSvga->GMRTree, &pRegion->Core);

                        ExReleaseFastMutex(&pSvga->SvgaMutex);

                        *pu32GmrId = pRegion->u32GmrId;
                        *pu64UserAddress = (uint64_t)pRegion->pvR3;

                        /* Everything OK. */
                        return STATUS_SUCCESS;
                    }

                    gmrFree(pRegion);
                }
            }
            else
            {
                AssertFailed();
                Status = STATUS_INSUFFICIENT_RESOURCES;
            }

            SvgaGMRIdFree(pSvga, pRegion->u32GmrId);
        }

        GaMemFree(pRegion);
    }
    else
    {
        AssertFailed();
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}
