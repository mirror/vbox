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

#include "VBoxMPWddm.h"
#include "common/VBoxMPCommon.h"
#include "VBoxMPVdma.h"
#include "VBoxMPVhwa.h"
#include <iprt/asm.h>

NTSTATUS vboxVdmaPipeConstruct(PVBOXVDMAPIPE pPipe)
{
    KeInitializeSpinLock(&pPipe->SinchLock);
    KeInitializeEvent(&pPipe->Event, SynchronizationEvent, FALSE);
    InitializeListHead(&pPipe->CmdListHead);
    pPipe->enmState = VBOXVDMAPIPE_STATE_CREATED;
    pPipe->bNeedNotify = true;
    return STATUS_SUCCESS;
}

NTSTATUS vboxVdmaPipeSvrOpen(PVBOXVDMAPIPE pPipe)
{
    NTSTATUS Status = STATUS_SUCCESS;
    KIRQL OldIrql;
    KeAcquireSpinLock(&pPipe->SinchLock, &OldIrql);
    Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_CREATED);
    switch (pPipe->enmState)
    {
        case VBOXVDMAPIPE_STATE_CREATED:
            pPipe->enmState = VBOXVDMAPIPE_STATE_OPENNED;
            pPipe->bNeedNotify = false;
            break;
        case VBOXVDMAPIPE_STATE_OPENNED:
            pPipe->bNeedNotify = false;
            break;
        default:
            AssertBreakpoint();
            Status = STATUS_INVALID_PIPE_STATE;
            break;
    }

    KeReleaseSpinLock(&pPipe->SinchLock, OldIrql);
    return Status;
}

NTSTATUS vboxVdmaPipeSvrClose(PVBOXVDMAPIPE pPipe)
{
    NTSTATUS Status = STATUS_SUCCESS;
    KIRQL OldIrql;
    KeAcquireSpinLock(&pPipe->SinchLock, &OldIrql);
    Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSED
            || pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSING);
    switch (pPipe->enmState)
    {
        case VBOXVDMAPIPE_STATE_CLOSING:
            pPipe->enmState = VBOXVDMAPIPE_STATE_CLOSED;
            break;
        case VBOXVDMAPIPE_STATE_CLOSED:
            break;
        default:
            AssertBreakpoint();
            Status = STATUS_INVALID_PIPE_STATE;
            break;
    }

    KeReleaseSpinLock(&pPipe->SinchLock, OldIrql);
    return Status;
}

NTSTATUS vboxVdmaPipeCltClose(PVBOXVDMAPIPE pPipe)
{
    NTSTATUS Status = STATUS_SUCCESS;
    KIRQL OldIrql;
    KeAcquireSpinLock(&pPipe->SinchLock, &OldIrql);
    bool bNeedNotify = false;
    Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_OPENNED
                || pPipe->enmState == VBOXVDMAPIPE_STATE_CREATED
                ||  pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSED);
    switch (pPipe->enmState)
    {
        case VBOXVDMAPIPE_STATE_OPENNED:
            pPipe->enmState = VBOXVDMAPIPE_STATE_CLOSING;
            bNeedNotify = pPipe->bNeedNotify;
            pPipe->bNeedNotify = false;
            break;
        case VBOXVDMAPIPE_STATE_CREATED:
            pPipe->enmState = VBOXVDMAPIPE_STATE_CLOSED;
            pPipe->bNeedNotify = false;
            break;
        case VBOXVDMAPIPE_STATE_CLOSED:
            break;
        default:
            AssertBreakpoint();
            Status = STATUS_INVALID_PIPE_STATE;
            break;
    }

    KeReleaseSpinLock(&pPipe->SinchLock, OldIrql);

    if (bNeedNotify)
    {
        KeSetEvent(&pPipe->Event, 0, FALSE);
    }
    return Status;
}

NTSTATUS vboxVdmaPipeDestruct(PVBOXVDMAPIPE pPipe)
{
    Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSED
            || pPipe->enmState == VBOXVDMAPIPE_STATE_CREATED);
    /* ensure the pipe is closed */
    NTSTATUS Status = vboxVdmaPipeCltClose(pPipe);
    Assert(Status == STATUS_SUCCESS);

    Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSED);

    return Status;
}

NTSTATUS vboxVdmaPipeSvrCmdGetList(PVBOXVDMAPIPE pPipe, PLIST_ENTRY pDetachHead)
{
    PLIST_ENTRY pEntry = NULL;
    KIRQL OldIrql;
    NTSTATUS Status = STATUS_SUCCESS;
    VBOXVDMAPIPE_STATE enmState = VBOXVDMAPIPE_STATE_CLOSED;
    do
    {
        bool bListEmpty = true;
        KeAcquireSpinLock(&pPipe->SinchLock, &OldIrql);
        Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_OPENNED
                || pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSING);
        Assert(pPipe->enmState >= VBOXVDMAPIPE_STATE_OPENNED);
        enmState = pPipe->enmState;
        if (enmState >= VBOXVDMAPIPE_STATE_OPENNED)
        {
            vboxVideoLeDetach(&pPipe->CmdListHead, pDetachHead);
            bListEmpty = !!(IsListEmpty(pDetachHead));
            pPipe->bNeedNotify = bListEmpty;
        }
        else
        {
            KeReleaseSpinLock(&pPipe->SinchLock, OldIrql);
            Status = STATUS_INVALID_PIPE_STATE;
            break;
        }

        KeReleaseSpinLock(&pPipe->SinchLock, OldIrql);

        if (!bListEmpty)
        {
            Assert(Status == STATUS_SUCCESS);
            break;
        }

        if (enmState == VBOXVDMAPIPE_STATE_OPENNED)
        {
            Status = KeWaitForSingleObject(&pPipe->Event, Executive, KernelMode, FALSE, NULL /* PLARGE_INTEGER Timeout */);
            Assert(Status == STATUS_SUCCESS);
            if (Status != STATUS_SUCCESS)
                break;
        }
        else
        {
            Assert(enmState == VBOXVDMAPIPE_STATE_CLOSING);
            Status = STATUS_PIPE_CLOSING;
            break;
        }
    } while (1);

    return Status;
}

NTSTATUS vboxVdmaPipeCltCmdPut(PVBOXVDMAPIPE pPipe, PVBOXVDMAPIPE_CMD_HDR pCmd)
{
    NTSTATUS Status = STATUS_SUCCESS;
    KIRQL OldIrql;
    bool bNeedNotify = false;

    KeAcquireSpinLock(&pPipe->SinchLock, &OldIrql);

    Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_OPENNED);
    if (pPipe->enmState == VBOXVDMAPIPE_STATE_OPENNED)
    {
        bNeedNotify = pPipe->bNeedNotify;
        InsertHeadList(&pPipe->CmdListHead, &pCmd->ListEntry);
        pPipe->bNeedNotify = false;
    }
    else
        Status = STATUS_INVALID_PIPE_STATE;

    KeReleaseSpinLock(&pPipe->SinchLock, OldIrql);

    if (bNeedNotify)
    {
        KeSetEvent(&pPipe->Event, 0, FALSE);
    }

    return Status;
}

PVBOXVDMAPIPE_CMD_DR vboxVdmaGgCmdCreate(PVBOXMP_DEVEXT pDevExt, VBOXVDMAPIPE_CMD_TYPE enmType, uint32_t cbCmd)
{
    PVBOXVDMAPIPE_CMD_DR pHdr;
#ifdef VBOX_WDDM_IRQ_COMPLETION
    if (enmType == VBOXVDMAPIPE_CMD_TYPE_DMACMD)
    {
        UINT cbAlloc = VBOXVDMACMD_SIZE_FROMBODYSIZE(cbCmd);
        VBOXVDMACBUF_DR* pDr = vboxVdmaCBufDrCreate(&pDevExt->u.primary.Vdma, cbAlloc);
        if (!pDr)
        {
            WARN(("dr allocation failed"));
            return NULL;
        }
        pDr->fFlags = VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR;
        pDr->cbBuf = VBOXVDMACMD_HEADER_SIZE();
        pDr->rc = VINF_SUCCESS;


        PVBOXVDMACMD pDmaHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
        pDmaHdr->enmType = VBOXVDMACMD_TYPE_DMA_NOP;
        pDmaHdr->u32CmdSpecific = 0;

        pHdr = VBOXVDMACMD_BODY(pDmaHdr, VBOXVDMAPIPE_CMD_DR);
    }
    else
#endif
    {
        pHdr = (PVBOXVDMAPIPE_CMD_DR)vboxWddmMemAllocZero(cbCmd);
        if (!pHdr)
        {
            WARN(("cmd allocation failed"));
            return NULL;
        }
    }
    pHdr->enmType = enmType;
    pHdr->cRefs = 1;
    return pHdr;
}

#ifdef VBOX_WDDM_IRQ_COMPLETION
DECLINLINE(VBOXVDMACBUF_DR*) vboxVdmaGgCmdDmaGetDr(PVBOXVDMAPIPE_CMD_DMACMD pDr)
{
    VBOXVDMACMD* pDmaCmd = VBOXVDMACMD_FROM_BODY(pDr);
    VBOXVDMACBUF_DR* pDmaDr = VBOXVDMACBUF_DR_FROM_TAIL(pDmaCmd);
    return pDmaDr;
}

DECLINLINE(PVBOXVDMADDI_CMD) vboxVdmaGgCmdDmaGetDdiCmd(PVBOXVDMAPIPE_CMD_DMACMD pDr)
{
    VBOXVDMACBUF_DR* pDmaDr = vboxVdmaGgCmdDmaGetDr(pDr);
    return VBOXVDMADDI_CMD_FROM_BUF_DR(pDmaDr);
}

#endif

void vboxVdmaGgCmdDestroy(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAPIPE_CMD_DR pDr)
{
#ifdef VBOX_WDDM_IRQ_COMPLETION
    if (pDr->enmType == VBOXVDMAPIPE_CMD_TYPE_DMACMD)
    {
        VBOXVDMACBUF_DR* pDmaDr = vboxVdmaGgCmdDmaGetDr((PVBOXVDMAPIPE_CMD_DMACMD)pDr);
        vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDmaDr);
        return;
    }
#endif
    vboxWddmMemFree(pDr);
}

DECLCALLBACK(VOID) vboxVdmaGgDdiCmdRelease(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext)
{
    vboxVdmaGgCmdRelease(pDevExt, (PVBOXVDMAPIPE_CMD_DR)pvContext);
}

/**
 * helper function used for system thread creation
 */
static NTSTATUS vboxVdmaGgThreadCreate(PKTHREAD * ppThread, PKSTART_ROUTINE  pStartRoutine, PVOID  pStartContext)
{
    NTSTATUS fStatus;
    HANDLE hThread;
    OBJECT_ATTRIBUTES fObjectAttributes;

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    InitializeObjectAttributes(&fObjectAttributes, NULL, OBJ_KERNEL_HANDLE,
                        NULL, NULL);

    fStatus = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS,
                        &fObjectAttributes, NULL, NULL,
                        (PKSTART_ROUTINE) pStartRoutine, pStartContext);
    if (!NT_SUCCESS(fStatus))
      return fStatus;

    ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, NULL,
                        KernelMode, (PVOID*) ppThread, NULL);
    ZwClose(hThread);
    return STATUS_SUCCESS;
}

DECLINLINE(void) vboxVdmaDirtyRectsCalcIntersection(const RECT *pArea, const PVBOXWDDM_RECTS_INFO pRects, PVBOXWDDM_RECTS_INFO pResult)
{
    uint32_t cRects = 0;
    for (uint32_t i = 0; i < pRects->cRects; ++i)
    {
        if (vboxWddmRectIntersection(pArea, &pRects->aRects[i], &pResult->aRects[cRects]))
        {
            ++cRects;
        }
    }

    pResult->cRects = cRects;
}

DECLINLINE(bool) vboxVdmaDirtyRectsHasIntersections(const RECT *paRects1, uint32_t cRects1, const RECT *paRects2, uint32_t cRects2)
{
    RECT tmpRect;
    for (uint32_t i = 0; i < cRects1; ++i)
    {
        const RECT * pRect1 = &paRects1[i];
        for (uint32_t j = 0; j < cRects2; ++j)
        {
            const RECT * pRect2 = &paRects2[j];
            if (vboxWddmRectIntersection(pRect1, pRect2, &tmpRect))
                return true;
        }
    }
    return false;
}

DECLINLINE(bool) vboxVdmaDirtyRectsIsCover(const RECT *paRects, uint32_t cRects, const RECT *paRectsCovered, uint32_t cRectsCovered)
{
    for (uint32_t i = 0; i < cRectsCovered; ++i)
    {
        const RECT * pRectCovered = &paRectsCovered[i];
        uint32_t j = 0;
        for (; j < cRects; ++j)
        {
            const RECT * pRect = &paRects[j];
            if (vboxWddmRectIsCoveres(pRect, pRectCovered))
                break;
        }
        if (j == cRects)
            return false;
    }
    return true;
}

NTSTATUS vboxVdmaPostHideSwapchain(PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
    uint32_t cbCmdInternal = VBOXVIDEOCM_CMD_RECTS_INTERNAL_SIZE4CRECTS(0);
    PVBOXVIDEOCM_CMD_RECTS_INTERNAL pCmdInternal =
            (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdCreate(&pSwapchain->pContext->CmContext, cbCmdInternal);
    Assert(pCmdInternal);
    if (pCmdInternal)
    {
        pCmdInternal->hSwapchainUm = pSwapchain->hSwapchainUm;
        pCmdInternal->Cmd.fFlags.Value = 0;
        pCmdInternal->Cmd.fFlags.bAddHiddenRects = 1;
        pCmdInternal->Cmd.fFlags.bHide = 1;
        pCmdInternal->Cmd.RectsInfo.cRects = 0;
        vboxVideoCmCmdSubmit(pCmdInternal, VBOXVIDEOCM_SUBMITSIZE_DEFAULT);
        return STATUS_SUCCESS;
    }
    return STATUS_NO_MEMORY;
}

/**
 * @param pDevExt
 */
static NTSTATUS vboxVdmaGgDirtyRectsProcess(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain, RECT *pSrcRect, VBOXVDMAPIPE_RECTS *pContextRects)
{
    PVBOXWDDM_RECTS_INFO pRects = &pContextRects->UpdateRects;
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDEOCM_CMD_RECTS_INTERNAL pCmdInternal = NULL;
    uint32_t cbCmdInternal = VBOXVIDEOCM_CMD_RECTS_INTERNAL_SIZE4CRECTS(pRects->cRects);
    BOOLEAN fCurChanged = FALSE, fCurRectChanged = FALSE;
    POINT CurPos;
    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);

    ExAcquireFastMutex(&pDevExt->ContextMutex);

    if (pSwapchain)
    {
        CurPos.x = pContextRects->ContextRect.left - pSrcRect->left;
        CurPos.y = pContextRects->ContextRect.top - pSrcRect->top;

        if (CurPos.x != pSwapchain->Pos.x || CurPos.y != pSwapchain->Pos.y)
        {
#if 0
            if (pSwapchain->Pos.x != VBOXWDDM_INVALID_COORD)
                VBoxWddmVrListTranslate(&pSwapchain->VisibleRegions, pSwapchain->Pos.x - CurPos.x, pSwapchain->Pos.y - CurPos.y);
            else
#endif
                VBoxWddmVrListClear(&pSwapchain->VisibleRegions);
            fCurRectChanged = TRUE;
            pSwapchain->Pos = CurPos;
        }

        Status = VBoxWddmVrListRectsAdd(&pSwapchain->VisibleRegions, pRects->cRects, pRects->aRects, &fCurChanged);
        if (!NT_SUCCESS(Status))
        {
            WARN(("VBoxWddmVrListRectsAdd failed!"));
            goto done;
        }


        /* visible rects of different windows do not intersect,
         * so if the given window visible rects did not increase, others have not changed either */
        if (!fCurChanged && !fCurRectChanged)
            goto done;
    }

    /* before posting the add visible rects diff, we need to first hide rects for other windows */

    for (PLIST_ENTRY pCur = pDevExt->SwapchainList3D.Flink; pCur != &pDevExt->SwapchainList3D; pCur = pCur->Flink)
    {
        if (pCur != &pSwapchain->DevExtListEntry)
        {
            PVBOXWDDM_SWAPCHAIN pCurSwapchain = VBOXWDDMENTRY_2_SWAPCHAIN(pCur);
            BOOLEAN fChanged = FALSE;

            Status = VBoxWddmVrListRectsSubst(&pCurSwapchain->VisibleRegions, pRects->cRects, pRects->aRects, &fChanged);
            if (!NT_SUCCESS(Status))
            {
                WARN(("vboxWddmVrListRectsAdd failed!"));
                goto done;
            }

            if (!fChanged)
                continue;

            if (!pCmdInternal)
            {
                pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdCreate(&pCurSwapchain->pContext->CmContext, cbCmdInternal);
                if (!pCmdInternal)
                {
                    WARN(("vboxVideoCmCmdCreate failed!"));
                    Status = STATUS_NO_MEMORY;
                    goto done;
                }
            }
            else
            {
                pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdReinitForContext(pCmdInternal, &pCurSwapchain->pContext->CmContext);
            }

            pCmdInternal->Cmd.fFlags.Value = 0;
            pCmdInternal->Cmd.fFlags.bAddHiddenRects = 1;
            memcpy(&pCmdInternal->Cmd.RectsInfo, pRects, RT_OFFSETOF(VBOXWDDM_RECTS_INFO, aRects[pRects->cRects]));

            pCmdInternal->hSwapchainUm = pCurSwapchain->hSwapchainUm;

            vboxVideoCmCmdSubmit(pCmdInternal, VBOXVIDEOCM_CMD_RECTS_INTERNAL_SIZE4CRECTS(pCmdInternal->Cmd.RectsInfo.cRects));
            pCmdInternal = NULL;
        }
    }

    if (!pSwapchain)
        goto done;

    RECT *pVisRects;

    if (fCurRectChanged && fCurChanged)
    {
        cbCmdInternal = VBOXVIDEOCM_CMD_RECTS_INTERNAL_SIZE4CRECTS(pRects->cRects + 1);
        if (pCmdInternal)
            vboxVideoCmCmdRelease(pCmdInternal);
        pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdCreate(&pContext->CmContext, cbCmdInternal);
        pCmdInternal->Cmd.fFlags.Value = 0;
        pCmdInternal->Cmd.fFlags.bSetViewRect = 1;
        pCmdInternal->Cmd.fFlags.bAddVisibleRects = 1;
        pCmdInternal->Cmd.RectsInfo.cRects = pRects->cRects + 1;
        pCmdInternal->Cmd.RectsInfo.aRects[0].left = CurPos.x;
        pCmdInternal->Cmd.RectsInfo.aRects[0].top = CurPos.y;
        pCmdInternal->Cmd.RectsInfo.aRects[0].right = CurPos.x + pSwapchain->width;
        pCmdInternal->Cmd.RectsInfo.aRects[0].bottom = CurPos.y + pSwapchain->height;
        pVisRects = &pCmdInternal->Cmd.RectsInfo.aRects[1];
    }
    else
    {
        if (!pCmdInternal)
        {
            Assert(pContext == pSwapchain->pContext);
            pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdCreate(&pContext->CmContext, cbCmdInternal);
            if (!pCmdInternal)
            {
                WARN(("vboxVideoCmCmdCreate failed!"));
                Status = STATUS_NO_MEMORY;
                goto done;
            }
        }
        else
        {
            pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdReinitForContext(pCmdInternal, &pContext->CmContext);
        }

        pCmdInternal->Cmd.fFlags.Value = 0;
        pCmdInternal->Cmd.fFlags.bAddVisibleRects = 1;
        pCmdInternal->Cmd.RectsInfo.cRects = pRects->cRects;
        pVisRects = &pCmdInternal->Cmd.RectsInfo.aRects[0];
    }

    pCmdInternal->hSwapchainUm = pSwapchain->hSwapchainUm;

    if (pRects->cRects)
        memcpy(pVisRects, pRects->aRects, sizeof (RECT) * pRects->cRects);

    vboxVideoCmCmdSubmit(pCmdInternal, VBOXVIDEOCM_CMD_RECTS_INTERNAL_SIZE4CRECTS(pCmdInternal->Cmd.RectsInfo.cRects));
    pCmdInternal = NULL;

done:
    ExReleaseFastMutex(&pDevExt->ContextMutex);

    if (pCmdInternal)
        vboxVideoCmCmdRelease(pCmdInternal);

    return Status;
}

static NTSTATUS vboxVdmaGgDmaColorFill(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL pCF)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    Assert (pDevExt->pvVisibleVram);
    if (pDevExt->pvVisibleVram)
    {
        PVBOXWDDM_ALLOCATION pAlloc = pCF->ClrFill.Alloc.pAlloc;
        Assert(pAlloc->offVram != VBOXVIDEOOFFSET_VOID);
        if (pAlloc->offVram != VBOXVIDEOOFFSET_VOID)
        {
            RECT UnionRect = {0};
            uint8_t *pvMem = pDevExt->pvVisibleVram + pAlloc->offVram;
            UINT bpp = pAlloc->SurfDesc.bpp;
            Assert(bpp);
            Assert(((bpp * pAlloc->SurfDesc.width) >> 3) == pAlloc->SurfDesc.pitch);
            switch (bpp)
            {
                case 32:
                {
                    uint8_t bytestPP = bpp >> 3;
                    for (UINT i = 0; i < pCF->ClrFill.Rects.cRects; ++i)
                    {
                        RECT *pRect = &pCF->ClrFill.Rects.aRects[i];
                        for (LONG ir = pRect->top; ir < pRect->bottom; ++ir)
                        {
                            uint32_t * pvU32Mem = (uint32_t*)(pvMem + (ir * pAlloc->SurfDesc.pitch) + (pRect->left * bytestPP));
                            uint32_t cRaw = pRect->right - pRect->left;
                            Assert(pRect->left >= 0);
                            Assert(pRect->right <= (LONG)pAlloc->SurfDesc.width);
                            Assert(pRect->top >= 0);
                            Assert(pRect->bottom <= (LONG)pAlloc->SurfDesc.height);
                            for (UINT j = 0; j < cRaw; ++j)
                            {
                                *pvU32Mem = pCF->ClrFill.Color;
                                ++pvU32Mem;
                            }
                        }
                        vboxWddmRectUnited(&UnionRect, &UnionRect, pRect);
                    }
                    Status = STATUS_SUCCESS;
                    break;
                }
                case 16:
                case 8:
                default:
                    AssertBreakpoint();
                    break;
            }

            if (Status == STATUS_SUCCESS)
            {
                if (pAlloc->SurfDesc.VidPnSourceId != D3DDDI_ID_UNINITIALIZED
                        && pAlloc->bAssigned
#if 0//def VBOXWDDM_RENDER_FROM_SHADOW
                        && pAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE
#else
                        && pAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE
#endif
                        )
                {
                    if (!vboxWddmRectIsEmpty(&UnionRect))
                    {
                        PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[pCF->ClrFill.Alloc.pAlloc->SurfDesc.VidPnSourceId];
                        uint32_t cUnlockedVBVADisabled = ASMAtomicReadU32(&pDevExt->cUnlockedVBVADisabled);
                        if (!cUnlockedVBVADisabled)
                        {
                            VBOXVBVA_OP(ReportDirtyRect, pDevExt, pSource, &UnionRect);
                        }
                        else
                        {
                            VBOXVBVA_OP_WITHLOCK(ReportDirtyRect, pDevExt, pSource, &UnionRect);
                        }
                    }
                }
                else
                {
                    AssertBreakpoint();
                }
            }
        }
    }

    return Status;
}

NTSTATUS vboxVdmaGgDmaBltPerform(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pSrcAlloc, RECT* pSrcRect,
        PVBOXWDDM_ALLOCATION pDstAlloc, RECT* pDstRect)
{
    uint8_t* pvVramBase = pDevExt->pvVisibleVram;
    /* we do not support stretching */
    uint32_t srcWidth = pSrcRect->right - pSrcRect->left;
    uint32_t srcHeight = pSrcRect->bottom - pSrcRect->top;
    uint32_t dstWidth = pDstRect->right - pDstRect->left;
    uint32_t dstHeight = pDstRect->bottom - pDstRect->top;
    Assert(srcHeight == dstHeight);
    Assert(dstWidth == srcWidth);
    Assert(pDstAlloc->offVram != VBOXVIDEOOFFSET_VOID);
    Assert(pSrcAlloc->offVram != VBOXVIDEOOFFSET_VOID);
    D3DDDIFORMAT enmSrcFormat, enmDstFormat;

    enmSrcFormat = pSrcAlloc->SurfDesc.format;
    enmDstFormat = pDstAlloc->SurfDesc.format;

    if (enmSrcFormat != enmDstFormat)
    {
        /* just ignore the alpha component
         * this is ok since our software-based stuff can not handle alpha channel in any way */
        enmSrcFormat = vboxWddmFmtNoAlphaFormat(enmSrcFormat);
        enmDstFormat = vboxWddmFmtNoAlphaFormat(enmDstFormat);
        if (enmSrcFormat != enmDstFormat)
        {
            WARN(("color conversion src(%d), dst(%d) not supported!", pSrcAlloc->SurfDesc.format, pDstAlloc->SurfDesc.format));
            return STATUS_INVALID_PARAMETER;
        }
    }
    if (srcHeight != dstHeight)
            return STATUS_INVALID_PARAMETER;
    if (srcWidth != dstWidth)
            return STATUS_INVALID_PARAMETER;
    if (pDstAlloc->offVram == VBOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;
    if (pSrcAlloc->offVram == VBOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;

    uint8_t *pvDstSurf = pvVramBase + pDstAlloc->offVram;
    uint8_t *pvSrcSurf = pvVramBase + pSrcAlloc->offVram;

    if (pDstAlloc->SurfDesc.width == dstWidth
            && pSrcAlloc->SurfDesc.width == srcWidth
            && pSrcAlloc->SurfDesc.width == pDstAlloc->SurfDesc.width)
    {
        Assert(!pDstRect->left);
        Assert(!pSrcRect->left);
        uint32_t cbDstOff = vboxWddmCalcOffXYrd(0 /* x */, pDstRect->top, pDstAlloc->SurfDesc.pitch, pDstAlloc->SurfDesc.format);
        uint32_t cbSrcOff = vboxWddmCalcOffXYrd(0 /* x */, pSrcRect->top, pSrcAlloc->SurfDesc.pitch, pSrcAlloc->SurfDesc.format);
        uint32_t cbSize = vboxWddmCalcSize(pDstAlloc->SurfDesc.pitch, dstHeight, pDstAlloc->SurfDesc.format);
        memcpy(pvDstSurf + cbDstOff, pvSrcSurf + cbSrcOff, cbSize);
    }
    else
    {
        uint32_t cbDstLine =  vboxWddmCalcRowSize(pDstRect->left, pDstRect->right, pDstAlloc->SurfDesc.format);
        uint32_t offDstStart = vboxWddmCalcOffXYrd(pDstRect->left, pDstRect->top, pDstAlloc->SurfDesc.pitch, pDstAlloc->SurfDesc.format);
        Assert(cbDstLine <= pDstAlloc->SurfDesc.pitch);
        uint32_t cbDstSkip = pDstAlloc->SurfDesc.pitch;
        uint8_t * pvDstStart = pvDstSurf + offDstStart;

        uint32_t cbSrcLine = vboxWddmCalcRowSize(pSrcRect->left, pSrcRect->right, pSrcAlloc->SurfDesc.format);
        uint32_t offSrcStart = vboxWddmCalcOffXYrd(pSrcRect->left, pSrcRect->top, pSrcAlloc->SurfDesc.pitch, pSrcAlloc->SurfDesc.format);
        Assert(cbSrcLine <= pSrcAlloc->SurfDesc.pitch);
        uint32_t cbSrcSkip = pSrcAlloc->SurfDesc.pitch;
        const uint8_t * pvSrcStart = pvSrcSurf + offSrcStart;

        uint32_t cRows = vboxWddmCalcNumRows(pDstRect->top, pDstRect->bottom, pDstAlloc->SurfDesc.format);

        Assert(cbDstLine == cbSrcLine);

        for (uint32_t i = 0; i < cRows; ++i)
        {
            memcpy(pvDstStart, pvSrcStart, cbDstLine);
            pvDstStart += cbDstSkip;
            pvSrcStart += cbSrcSkip;
        }
    }
    return STATUS_SUCCESS;
}

/*
 * @return on success the number of bytes the command contained, otherwise - VERR_xxx error code
 */
static NTSTATUS vboxVdmaGgDmaBlt(PVBOXMP_DEVEXT pDevExt, PVBOXVDMA_BLT pBlt)
{
    /* we do not support stretching for now */
    Assert(pBlt->SrcRect.right - pBlt->SrcRect.left == pBlt->DstRects.ContextRect.right - pBlt->DstRects.ContextRect.left);
    Assert(pBlt->SrcRect.bottom - pBlt->SrcRect.top == pBlt->DstRects.ContextRect.bottom - pBlt->DstRects.ContextRect.top);
    if (pBlt->SrcRect.right - pBlt->SrcRect.left != pBlt->DstRects.ContextRect.right - pBlt->DstRects.ContextRect.left)
        return STATUS_INVALID_PARAMETER;
    if (pBlt->SrcRect.bottom - pBlt->SrcRect.top != pBlt->DstRects.ContextRect.bottom - pBlt->DstRects.ContextRect.top)
        return STATUS_INVALID_PARAMETER;
    Assert(pBlt->DstRects.UpdateRects.cRects);

    NTSTATUS Status = STATUS_SUCCESS;

    if (pBlt->DstRects.UpdateRects.cRects)
    {
        for (uint32_t i = 0; i < pBlt->DstRects.UpdateRects.cRects; ++i)
        {
            RECT SrcRect;
            vboxWddmRectTranslated(&SrcRect, &pBlt->DstRects.UpdateRects.aRects[i], -pBlt->DstRects.ContextRect.left, -pBlt->DstRects.ContextRect.top);

            Status = vboxVdmaGgDmaBltPerform(pDevExt, pBlt->SrcAlloc.pAlloc, &SrcRect,
                    pBlt->DstAlloc.pAlloc, &pBlt->DstRects.UpdateRects.aRects[i]);
            Assert(Status == STATUS_SUCCESS);
            if (Status != STATUS_SUCCESS)
                return Status;
        }
    }
    else
    {
        Status = vboxVdmaGgDmaBltPerform(pDevExt, pBlt->SrcAlloc.pAlloc, &pBlt->SrcRect,
                pBlt->DstAlloc.pAlloc, &pBlt->DstRects.ContextRect);
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
            return Status;
    }

    return Status;
}

static VOID vboxWddmBltPipeRectsTranslate(VBOXVDMAPIPE_RECTS *pRects, int x, int y)
{
    vboxWddmRectTranslate(&pRects->ContextRect, x, y);

    for (UINT i = 0; i < pRects->UpdateRects.cRects; ++i)
    {
        vboxWddmRectTranslate(&pRects->UpdateRects.aRects[i], x, y);
    }
}

static NTSTATUS vboxVdmaGgDmaCmdProcessFast(PVBOXMP_DEVEXT pDevExt, VBOXVDMAPIPE_CMD_DMACMD *pDmaCmd)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_CONTEXT pContext = pDmaCmd->pContext;
    DXGK_INTERRUPT_TYPE enmComplType = DXGK_INTERRUPT_DMA_COMPLETED;
    switch (pDmaCmd->enmCmd)
    {
        case VBOXVDMACMD_TYPE_DMA_PRESENT_BLT:
        {
            PVBOXVDMAPIPE_CMD_DMACMD_BLT pBlt = (PVBOXVDMAPIPE_CMD_DMACMD_BLT)pDmaCmd;
            PVBOXWDDM_ALLOCATION pDstAlloc = pBlt->Blt.DstAlloc.pAlloc;
            PVBOXWDDM_ALLOCATION pSrcAlloc = pBlt->Blt.SrcAlloc.pAlloc;

            if (pBlt->Hdr.fFlags.fRealOp)
            {
                vboxVdmaGgDmaBlt(pDevExt, &pBlt->Blt);

                if ((pDstAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE
                        || pDstAlloc->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC)
                        && pDstAlloc->bAssigned)
                {
                    VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pDstAlloc->SurfDesc.VidPnSourceId];
                    Assert(pDstAlloc->SurfDesc.VidPnSourceId < VBOX_VIDEO_MAX_SCREENS);
                    Assert(pSource->pPrimaryAllocation == pDstAlloc);

                    RECT UpdateRect;
                    UpdateRect = pBlt->Blt.DstRects.UpdateRects.aRects[0];
                    for (UINT i = 1; i < pBlt->Blt.DstRects.UpdateRects.cRects; ++i)
                    {
                        vboxWddmRectUnite(&UpdateRect, &pBlt->Blt.DstRects.UpdateRects.aRects[i]);
                    }

                    uint32_t cUnlockedVBVADisabled = ASMAtomicReadU32(&pDevExt->cUnlockedVBVADisabled);
                    if (!cUnlockedVBVADisabled)
                    {
                        VBOXVBVA_OP(ReportDirtyRect, pDevExt, pSource, &UpdateRect);
                    }
                    else
                    {
                        VBOXVBVA_OP_WITHLOCK(ReportDirtyRect, pDevExt, pSource, &UpdateRect);
                    }
                }
            }

            if (pBlt->Hdr.fFlags.fVisibleRegions)
            {
                Status = STATUS_MORE_PROCESSING_REQUIRED;
                vboxWddmAllocationRetain(pDstAlloc);
                vboxWddmAllocationRetain(pSrcAlloc);
            }
            break;
        }

        case VBOXVDMACMD_TYPE_DMA_PRESENT_FLIP:
        {
            PVBOXVDMAPIPE_CMD_DMACMD_FLIP pFlip = (PVBOXVDMAPIPE_CMD_DMACMD_FLIP)pDmaCmd;
            Assert(pFlip->Hdr.fFlags.fVisibleRegions);
            Assert(!pFlip->Hdr.fFlags.fRealOp);
            if (pFlip->Hdr.fFlags.fVisibleRegions)
            {
                Status = STATUS_MORE_PROCESSING_REQUIRED;
                vboxWddmAllocationRetain(pFlip->Flip.Alloc.pAlloc);
            }

            break;
        }
        case VBOXVDMACMD_TYPE_DMA_PRESENT_CLRFILL:
        {
            PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL pCF = (PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL)pDmaCmd;
            Assert(pCF->Hdr.fFlags.fRealOp);
            Assert(!pCF->Hdr.fFlags.fVisibleRegions);
            Status = vboxVdmaGgDmaColorFill(pDevExt, pCF);
            Assert(Status == STATUS_SUCCESS);
            break;
        }

        default:
            Assert(0);
            break;
    }

    /* Corresponding Release is done by dma command completion handler */
    vboxVdmaGgCmdAddRef(&pDmaCmd->Hdr);

    NTSTATUS tmpStatus = vboxVdmaGgCmdDmaNotifyCompleted(pDevExt, pDmaCmd, enmComplType);
    if (!NT_SUCCESS(tmpStatus))
    {
        WARN(("vboxVdmaGgCmdDmaNotifyCompleted failed, Status 0x%x", tmpStatus));
        /* the command was NOT submitted, and thus will not be released, release it here */
        vboxVdmaGgCmdRelease(pDevExt, &pDmaCmd->Hdr);
        Status = tmpStatus;
    }

    return Status;
}

static NTSTATUS vboxVdmaGgDmaCmdProcessSlow(PVBOXMP_DEVEXT pDevExt, VBOXVDMAPIPE_CMD_DMACMD *pDmaCmd)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_CONTEXT pContext = pDmaCmd->pContext;
    DXGK_INTERRUPT_TYPE enmComplType = DXGK_INTERRUPT_DMA_COMPLETED;

    Assert(pDmaCmd->fFlags.Value);
    Assert(!pDmaCmd->fFlags.fRealOp);

    switch (pDmaCmd->enmCmd)
    {
        case VBOXVDMACMD_TYPE_DMA_PRESENT_BLT:
        {
            PVBOXVDMAPIPE_CMD_DMACMD_BLT pBlt = (PVBOXVDMAPIPE_CMD_DMACMD_BLT)pDmaCmd;
            PVBOXWDDM_ALLOCATION pDstAlloc = pBlt->Blt.DstAlloc.pAlloc;
            PVBOXWDDM_ALLOCATION pSrcAlloc = pBlt->Blt.SrcAlloc.pAlloc;
            BOOLEAN bComplete = TRUE;
            VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pDstAlloc->SurfDesc.VidPnSourceId];
            Assert(pDstAlloc->SurfDesc.VidPnSourceId < VBOX_VIDEO_MAX_SCREENS);

            if (pBlt->Hdr.fFlags.fVisibleRegions)
            {
                PVBOXWDDM_SWAPCHAIN pSwapchain = vboxWddmSwapchainRetainByAlloc(pDevExt, pSrcAlloc);
                POINT pos = pSource->VScreenPos;
                if (pos.x || pos.y)
                {
                    /* note: do NOT translate the src rect since it is used for screen pos calculation */
                    vboxWddmBltPipeRectsTranslate(&pBlt->Blt.DstRects, pos.x, pos.y);
                }

                Status = vboxVdmaGgDirtyRectsProcess(pDevExt, pContext, pSwapchain, &pBlt->Blt.SrcRect, &pBlt->Blt.DstRects);
                Assert(Status == STATUS_SUCCESS);

                if (pSwapchain)
                    vboxWddmSwapchainRelease(pSwapchain);
            }
            else
            {
                WARN(("not expected!"));
            }

            vboxWddmAllocationRelease(pDstAlloc);
            vboxWddmAllocationRelease(pSrcAlloc);

            break;
        }

        case VBOXVDMACMD_TYPE_DMA_PRESENT_FLIP:
        {
            PVBOXVDMAPIPE_CMD_DMACMD_FLIP pFlip = (PVBOXVDMAPIPE_CMD_DMACMD_FLIP)pDmaCmd;
            PVBOXWDDM_ALLOCATION pAlloc = pFlip->Flip.Alloc.pAlloc;
            VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pAlloc->SurfDesc.VidPnSourceId];
            if (pFlip->Hdr.fFlags.fVisibleRegions)
            {
                PVBOXWDDM_SWAPCHAIN pSwapchain;
                pSwapchain = vboxWddmSwapchainRetainByAlloc(pDevExt, pAlloc);
                if (pSwapchain)
                {
                    POINT pos = pSource->VScreenPos;
                    RECT SrcRect;
                    VBOXVDMAPIPE_RECTS Rects;
                    SrcRect.left = 0;
                    SrcRect.top = 0;
                    SrcRect.right = pAlloc->SurfDesc.width;
                    SrcRect.bottom = pAlloc->SurfDesc.height;
                    Rects.ContextRect.left = pos.x;
                    Rects.ContextRect.top = pos.y;
                    Rects.ContextRect.right = pAlloc->SurfDesc.width + pos.x;
                    Rects.ContextRect.bottom = pAlloc->SurfDesc.height + pos.y;
                    Rects.UpdateRects.cRects = 1;
                    Rects.UpdateRects.aRects[0] = Rects.ContextRect;
                    Status = vboxVdmaGgDirtyRectsProcess(pDevExt, pContext, pSwapchain, &SrcRect, &Rects);
                    Assert(Status == STATUS_SUCCESS);
                    vboxWddmSwapchainRelease(pSwapchain);
                }
            }
            else
            {
                WARN(("not expected!"));
            }

            vboxWddmAllocationRelease(pAlloc);

            break;
        }

        default:
        {
            WARN(("not expected!"));
            break;
        }
    }

    vboxVdmaGgCmdRelease(pDevExt, &pDmaCmd->Hdr);

    return Status;
}

static DECLCALLBACK(UINT) vboxVdmaGgCmdCancelVisitor(PVBOXVIDEOCM_CTX pContext, PVOID pvCmd, uint32_t cbCmd, PVOID pvVisitor)
{
    PVBOXWDDM_SWAPCHAIN pSwapchain = (PVBOXWDDM_SWAPCHAIN)pvVisitor;
    if (!pSwapchain)
        return VBOXVIDEOCMCMDVISITOR_RETURN_RMCMD;
    PVBOXVIDEOCM_CMD_RECTS_INTERNAL pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)pvCmd;
    if (pCmdInternal->hSwapchainUm == pSwapchain->hSwapchainUm)
        return VBOXVIDEOCMCMDVISITOR_RETURN_RMCMD;
    return 0;
}

static VOID vboxVdmaGgWorkerThread(PVOID pvUser)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)pvUser;
    PVBOXVDMAGG pVdma = &pDevExt->u.primary.Vdma.DmaGg;

    NTSTATUS Status = vboxVdmaPipeSvrOpen(&pVdma->CmdPipe);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        do
        {
            LIST_ENTRY CmdList;
            Status = vboxVdmaPipeSvrCmdGetList(&pVdma->CmdPipe, &CmdList);
            Assert(Status == STATUS_SUCCESS || Status == STATUS_PIPE_CLOSING);
            if (Status == STATUS_SUCCESS)
            {
                for (PLIST_ENTRY pCur = CmdList.Blink; pCur != &CmdList;)
                {
                    PVBOXVDMAPIPE_CMD_DR pDr = VBOXVDMAPIPE_CMD_DR_FROM_ENTRY(pCur);
                    RemoveEntryList(pCur);
                    pCur = CmdList.Blink;
                    switch (pDr->enmType)
                    {
                        case VBOXVDMAPIPE_CMD_TYPE_DMACMD:
                        {
                            PVBOXVDMAPIPE_CMD_DMACMD pDmaCmd = (PVBOXVDMAPIPE_CMD_DMACMD)pDr;
                            Status = vboxVdmaGgDmaCmdProcessSlow(pDevExt, pDmaCmd);
                            Assert(Status == STATUS_SUCCESS);
                        } break;
#if 0
                        case VBOXVDMAPIPE_CMD_TYPE_RECTSINFO:
                        {
                            PVBOXVDMAPIPE_CMD_RECTSINFO pRects = (PVBOXVDMAPIPE_CMD_RECTSINFO)pDr;
                            Status = vboxVdmaGgDirtyRectsProcess(pDevExt, pRects->pContext, pRects->pSwapchain, &pRects->ContextsRects);
                            Assert(Status == STATUS_SUCCESS);
                            vboxVdmaGgCmdRelease(pDevExt, pDr);
                            break;
                        }
#endif
                        case VBOXVDMAPIPE_CMD_TYPE_FINISH:
                        {
                            PVBOXVDMAPIPE_CMD_FINISH pCmd = (PVBOXVDMAPIPE_CMD_FINISH)pDr;
                            PVBOXWDDM_CONTEXT pContext = pCmd->pContext;
                            Assert(pCmd->pEvent);
                            Status = vboxVideoCmCmdSubmitCompleteEvent(&pContext->CmContext, pCmd->pEvent);
                            if (Status != STATUS_SUCCESS)
                            {
                                WARN(("vboxVideoCmCmdWaitCompleted failedm Status (0x%x)", Status));
                            }
                            vboxVdmaGgCmdRelease(pDevExt, &pCmd->Hdr);
                            break;
                        }
                        case VBOXVDMAPIPE_CMD_TYPE_CANCEL:
                        {
                            PVBOXVDMAPIPE_CMD_CANCEL pCmd = (PVBOXVDMAPIPE_CMD_CANCEL)pDr;
                            PVBOXWDDM_CONTEXT pContext = pCmd->pContext;
                            Status = vboxVideoCmCmdVisit(&pContext->CmContext, FALSE, vboxVdmaGgCmdCancelVisitor, pCmd->pSwapchain);
                            if (Status != STATUS_SUCCESS)
                            {
                                WARN(("vboxVideoCmCmdWaitCompleted failedm Status (0x%x)", Status));
                            }
                            Assert(pCmd->pEvent);
                            KeSetEvent(pCmd->pEvent, 0, FALSE);
                            vboxVdmaGgCmdRelease(pDevExt, &pCmd->Hdr);
                            break;
                        }
                        default:
                            AssertBreakpoint();
                    }
                }
            }
            else
                break;
        } while (1);
    }

    /* always try to close the pipe to make sure the client side is notified */
    Status = vboxVdmaPipeSvrClose(&pVdma->CmdPipe);
    Assert(Status == STATUS_SUCCESS);
}

NTSTATUS vboxVdmaGgConstruct(PVBOXMP_DEVEXT pDevExt)
{
    PVBOXVDMAGG pVdma = &pDevExt->u.primary.Vdma.DmaGg;
    NTSTATUS Status = vboxVdmaPipeConstruct(&pVdma->CmdPipe);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxVdmaGgThreadCreate(&pVdma->pThread, vboxVdmaGgWorkerThread, pDevExt);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
            return STATUS_SUCCESS;

        NTSTATUS tmpStatus = vboxVdmaPipeDestruct(&pVdma->CmdPipe);
        Assert(tmpStatus == STATUS_SUCCESS);
    }

    /* we're here ONLY in case of an error */
    Assert(Status != STATUS_SUCCESS);
    return Status;
}

NTSTATUS vboxVdmaGgDestruct(PVBOXMP_DEVEXT pDevExt)
{
    PVBOXVDMAGG pVdma = &pDevExt->u.primary.Vdma.DmaGg;
    /* this informs the server thread that it should complete all current commands and exit */
    NTSTATUS Status = vboxVdmaPipeCltClose(&pVdma->CmdPipe);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = KeWaitForSingleObject(pVdma->pThread, Executive, KernelMode, FALSE, NULL /* PLARGE_INTEGER Timeout */);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Status = vboxVdmaPipeDestruct(&pVdma->CmdPipe);
            Assert(Status == STATUS_SUCCESS);
        }
    }

    return Status;
}

NTSTATUS vboxVdmaGgCmdSubmit(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAPIPE_CMD_DR pCmd)
{
    switch (pCmd->enmType)
    {
        case VBOXVDMAPIPE_CMD_TYPE_DMACMD:
        {
            PVBOXVDMAPIPE_CMD_DMACMD pDmaCmd = (PVBOXVDMAPIPE_CMD_DMACMD)pCmd;
            NTSTATUS Status = vboxVdmaGgDmaCmdProcessFast(pDevExt, pDmaCmd);
            if (Status == STATUS_MORE_PROCESSING_REQUIRED)
                break;
            return Status;
        }
        default:
            break;
    }
    /* correspondinf Release is done by the pipe command handler */
    vboxVdmaGgCmdAddRef(pCmd);
    return vboxVdmaPipeCltCmdPut(&pDevExt->u.primary.Vdma.DmaGg.CmdPipe, &pCmd->PipeHdr);
}

NTSTATUS vboxVdmaGgCmdDmaNotifySubmitted(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAPIPE_CMD_DMACMD pCmd)
{
    PVBOXVDMADDI_CMD pDdiCmd;
#ifdef VBOX_WDDM_IRQ_COMPLETION
    pDdiCmd = vboxVdmaGgCmdDmaGetDdiCmd(pCmd);
#else
    pDdiCmd = &pCmd->DdiCmd;
#endif
    NTSTATUS Status = vboxVdmaDdiCmdSubmitted(pDevExt, pDdiCmd);
    Assert(Status == STATUS_SUCCESS);
    return Status;
}

NTSTATUS vboxVdmaGgCmdDmaNotifyCompleted(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAPIPE_CMD_DMACMD pCmd, DXGK_INTERRUPT_TYPE enmComplType)
{
#ifdef VBOX_WDDM_IRQ_COMPLETION
    VBOXVDMACBUF_DR* pDr = vboxVdmaGgCmdDmaGetDr(pCmd);
    int rc = vboxVdmaCBufDrSubmit(pDevExt, &pDevExt->u.primary.Vdma, pDr);
    Assert(rc == VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        return STATUS_SUCCESS;
    }
    return STATUS_UNSUCCESSFUL;
#else
    return vboxVdmaDdiCmdCompleted(pDevExt, &pCmd->DdiCmd, enmComplType);
#endif
}

VOID vboxVdmaGgCmdDmaNotifyInit(PVBOXVDMAPIPE_CMD_DMACMD pCmd,
        uint32_t u32NodeOrdinal, uint32_t u32FenceId,
        PFNVBOXVDMADDICMDCOMPLETE_DPC pfnComplete, PVOID pvComplete)
{
    PVBOXVDMADDI_CMD pDdiCmd;
#ifdef VBOX_WDDM_IRQ_COMPLETION
    pDdiCmd = vboxVdmaGgCmdDmaGetDdiCmd(pCmd);
#else
    pDdiCmd = &pCmd->DdiCmd;
#endif
    vboxVdmaDdiCmdInit(pDdiCmd, u32NodeOrdinal, u32FenceId, pfnComplete, pvComplete);
}

NTSTATUS vboxVdmaGgCmdFinish(PVBOXMP_DEVEXT pDevExt, VBOXWDDM_CONTEXT *pContext, PKEVENT pEvent)
{
    NTSTATUS Status = STATUS_SUCCESS;

    PVBOXVDMAPIPE_CMD_FINISH pCmd = (PVBOXVDMAPIPE_CMD_FINISH)vboxVdmaGgCmdCreate(pDevExt, VBOXVDMAPIPE_CMD_TYPE_FINISH, sizeof (*pCmd));
    if (pCmd)
    {
        pCmd->pContext = pContext;
        pCmd->pEvent = pEvent;
        Status = vboxVdmaGgCmdSubmit(pDevExt, &pCmd->Hdr);
        if (!NT_SUCCESS(Status))
        {
            WARN(("vboxVdmaGgCmdSubmit returned 0x%x", Status));
        }
        vboxVdmaGgCmdRelease(pDevExt, &pCmd->Hdr);
    }
    else
    {
        WARN(("vboxVdmaGgCmdCreate failed"));
        Status = STATUS_NO_MEMORY;
    }
    return Status;
}

NTSTATUS vboxVdmaGgCmdCancel(PVBOXMP_DEVEXT pDevExt, VBOXWDDM_CONTEXT *pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    NTSTATUS Status = STATUS_SUCCESS;

    PVBOXVDMAPIPE_CMD_CANCEL pCmd = (PVBOXVDMAPIPE_CMD_CANCEL)vboxVdmaGgCmdCreate(pDevExt, VBOXVDMAPIPE_CMD_TYPE_CANCEL, sizeof (*pCmd));
    if (pCmd)
    {
        KEVENT Event;
        KeInitializeEvent(&Event, NotificationEvent, FALSE);
        pCmd->pContext = pContext;
        pCmd->pSwapchain = pSwapchain;
        pCmd->pEvent = &Event;
        Status = vboxVdmaGgCmdSubmit(pDevExt, &pCmd->Hdr);
        if (NT_SUCCESS(Status))
        {
            Status = KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Assert(Status == STATUS_SUCCESS);
        }
        else
        {
            WARN(("vboxVdmaGgCmdSubmit returned 0x%x", Status));
        }
        vboxVdmaGgCmdRelease(pDevExt, &pCmd->Hdr);
    }
    else
    {
        WARN(("vboxVdmaGgCmdCreate failed"));
        Status = STATUS_NO_MEMORY;
    }
    return Status;
}

/* end */

#ifdef VBOX_WITH_VDMA
/*
 * This is currently used by VDMA. It is invisible for Vdma API clients since
 * Vdma transport may change if we choose to use another (e.g. more light-weight)
 * transport for DMA commands submission
 */

#ifdef VBOXVDMA_WITH_VBVA
static int vboxWddmVdmaSubmitVbva(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo, HGSMIOFFSET offDr)
{
    int rc;
    if (vboxVbvaBufferBeginUpdate (pDevExt, &pDevExt->u.primary.Vbva))
    {
        rc = vboxVbvaReportCmdOffset(pDevExt, &pDevExt->u.primary.Vbva, offDr);
        vboxVbvaBufferEndUpdate (pDevExt, &pDevExt->u.primary.Vbva);
    }
    else
    {
        AssertBreakpoint();
        rc = VERR_INVALID_STATE;
    }
    return rc;
}
#define vboxWddmVdmaSubmit vboxWddmVdmaSubmitVbva
#else
static int vboxWddmVdmaSubmitHgsmi(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo, HGSMIOFFSET offDr)
{
    VBoxVideoCmnPortWriteUlong(VBoxCommonFromDeviceExt(pDevExt)->guestCtx.port, offDr);
    return VINF_SUCCESS;
}
#define vboxWddmVdmaSubmit vboxWddmVdmaSubmitHgsmi
#endif

static int vboxVdmaInformHost(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo, VBOXVDMA_CTL_TYPE enmCtl)
{
    int rc = VINF_SUCCESS;

    PVBOXVDMA_CTL pCmd = (PVBOXVDMA_CTL)VBoxSHGSMICommandAlloc(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, sizeof (VBOXVDMA_CTL), HGSMI_CH_VBVA, VBVA_VDMA_CTL);
    if (pCmd)
    {
        pCmd->enmCtl = enmCtl;
        pCmd->u32Offset = pInfo->CmdHeap.Heap.area.offBase;
        pCmd->i32Result = VERR_NOT_SUPPORTED;

        const VBOXSHGSMIHEADER* pHdr = VBoxSHGSMICommandPrepSynch(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pCmd);
        Assert(pHdr);
        if (pHdr)
        {
            do
            {
                HGSMIOFFSET offCmd = VBoxSHGSMICommandOffset(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pHdr);
                Assert(offCmd != HGSMIOFFSET_VOID);
                if (offCmd != HGSMIOFFSET_VOID)
                {
                    rc = vboxWddmVdmaSubmit(pDevExt, pInfo, offCmd);
                    AssertRC(rc);
                    if (RT_SUCCESS(rc))
                    {
                        rc = VBoxSHGSMICommandDoneSynch(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pHdr);
                        AssertRC(rc);
                        if (RT_SUCCESS(rc))
                        {
                            rc = pCmd->i32Result;
                            AssertRC(rc);
                        }
                        break;
                    }
                }
                else
                    rc = VERR_INVALID_PARAMETER;
                /* fail to submit, cancel it */
                VBoxSHGSMICommandCancelSynch(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pHdr);
            } while (0);
        }

        VBoxSHGSMICommandFree (&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pCmd);
    }
    else
    {
        LOGREL(("HGSMIHeapAlloc failed"));
        rc = VERR_OUT_OF_RESOURCES;
    }

    return rc;
}
#endif

/* create a DMACommand buffer */
int vboxVdmaCreate(PVBOXMP_DEVEXT pDevExt, VBOXVDMAINFO *pInfo
#ifdef VBOX_WITH_VDMA
        , ULONG offBuffer, ULONG cbBuffer
#endif
        )
{
    int rc;
    pInfo->fEnabled           = FALSE;

#ifdef VBOX_WITH_VDMA
    Assert((offBuffer & 0xfff) == 0);
    Assert((cbBuffer & 0xfff) == 0);
    Assert(offBuffer);
    Assert(cbBuffer);

    if((offBuffer & 0xfff)
            || (cbBuffer & 0xfff)
            || !offBuffer
            || !cbBuffer)
    {
        LOGREL(("invalid parameters: offBuffer(0x%x), cbBuffer(0x%x)", offBuffer, cbBuffer));
        return VERR_INVALID_PARAMETER;
    }
    PVOID pvBuffer;

    rc = VBoxMPCmnMapAdapterMemory(VBoxCommonFromDeviceExt(pDevExt),
                                   &pvBuffer,
                                   offBuffer,
                                   cbBuffer);
    Assert(RT_SUCCESS(rc));
    if (RT_SUCCESS(rc))
    {
        /* Setup a HGSMI heap within the adapter information area. */
        rc = VBoxSHGSMIInit(&pInfo->CmdHeap,
                             pvBuffer,
                             cbBuffer,
                             offBuffer,
                             false /*fOffsetBased*/);
        Assert(RT_SUCCESS(rc));
        if(RT_SUCCESS(rc))
#endif
        {
            NTSTATUS Status = vboxVdmaGgConstruct(pDevExt);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
                return VINF_SUCCESS;
            rc = VERR_GENERAL_FAILURE;
        }
#ifdef VBOX_WITH_VDMA
        else
            LOGREL(("HGSMIHeapSetup failed rc = 0x%x", rc));

        VBoxMPCmnUnmapAdapterMemory(VBoxCommonFromDeviceExt(pDevExt), &pvBuffer);
    }
    else
        LOGREL(("VBoxMapAdapterMemory failed rc = 0x%x\n", rc));
#endif
    return rc;
}

int vboxVdmaDisable (PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo)
{
    LOGF(("."));

    Assert(pInfo->fEnabled);
    if (!pInfo->fEnabled)
        return VINF_ALREADY_INITIALIZED;

    /* ensure nothing else is submitted */
    pInfo->fEnabled        = FALSE;
#ifdef VBOX_WITH_VDMA
    int rc = vboxVdmaInformHost (pDevExt, pInfo, VBOXVDMA_CTL_TYPE_DISABLE);
    AssertRC(rc);
    return rc;
#else
    return VINF_SUCCESS;
#endif
}

int vboxVdmaEnable (PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo)
{
    LOGF(("."));

    Assert(!pInfo->fEnabled);
    if (pInfo->fEnabled)
        return VINF_ALREADY_INITIALIZED;
#ifdef VBOX_WITH_VDMA
    int rc = vboxVdmaInformHost (pDevExt, pInfo, VBOXVDMA_CTL_TYPE_ENABLE);
    Assert(RT_SUCCESS(rc));
    if (RT_SUCCESS(rc))
        pInfo->fEnabled        = TRUE;

    return rc;
#else
    return VINF_SUCCESS;
#endif
}

#ifdef VBOX_WITH_VDMA
int vboxVdmaFlush (PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo)
{
    LOGF(("."));

    Assert(pInfo->fEnabled);
    if (!pInfo->fEnabled)
        return VINF_ALREADY_INITIALIZED;

    int rc = vboxVdmaInformHost (pDevExt, pInfo, VBOXVDMA_CTL_TYPE_FLUSH);
    Assert(RT_SUCCESS(rc));

    return rc;
}
#endif

int vboxVdmaDestroy (PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo)
{
    int rc = VINF_SUCCESS;
    NTSTATUS Status = vboxVdmaGgDestruct(pDevExt);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Assert(!pInfo->fEnabled);
        if (pInfo->fEnabled)
            rc = vboxVdmaDisable (pDevExt, pInfo);
#ifdef VBOX_WITH_VDMA
        VBoxSHGSMITerm(&pInfo->CmdHeap);
        VBoxMPCmnUnmapAdapterMemory(VBoxCommonFromDeviceExt(pDevExt), (void**)&pInfo->CmdHeap.Heap.area.pu8Base);
#endif
    }
    else
        rc = VERR_GENERAL_FAILURE;
    return rc;
}

#ifdef VBOX_WITH_VDMA
void vboxVdmaCBufDrFree (PVBOXVDMAINFO pInfo, PVBOXVDMACBUF_DR pDr)
{
    VBoxSHGSMICommandFree (&pInfo->CmdHeap, pDr);
}

PVBOXVDMACBUF_DR vboxVdmaCBufDrCreate (PVBOXVDMAINFO pInfo, uint32_t cbTrailingData)
{
    uint32_t cbDr = VBOXVDMACBUF_DR_SIZE(cbTrailingData);
    PVBOXVDMACBUF_DR pDr = (PVBOXVDMACBUF_DR)VBoxSHGSMICommandAlloc (&pInfo->CmdHeap, cbDr, HGSMI_CH_VBVA, VBVA_VDMA_CMD);
    Assert(pDr);
    if (pDr)
        memset (pDr, 0, cbDr);
    else
        LOGREL(("VBoxSHGSMICommandAlloc returned NULL"));

    return pDr;
}

static DECLCALLBACK(void) vboxVdmaCBufDrCompletion(PVBOXSHGSMI pHeap, void *pvCmd, void *pvContext)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)pvContext;
    PVBOXVDMAINFO pInfo = &pDevExt->u.primary.Vdma;

    vboxVdmaCBufDrFree (pInfo, (PVBOXVDMACBUF_DR)pvCmd);
}

static DECLCALLBACK(void) vboxVdmaCBufDrCompletionIrq(PVBOXSHGSMI pHeap, void *pvCmd, void *pvContext,
                                        PFNVBOXSHGSMICMDCOMPLETION *ppfnCompletion, void **ppvCompletion)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)pvContext;
    PVBOXVDMAINFO pVdma = &pDevExt->u.primary.Vdma;
    PVBOXVDMACBUF_DR pDr = (PVBOXVDMACBUF_DR)pvCmd;

    DXGK_INTERRUPT_TYPE enmComplType;

    if (RT_SUCCESS(pDr->rc))
    {
        enmComplType = DXGK_INTERRUPT_DMA_COMPLETED;
    }
    else if (pDr->rc == VERR_INTERRUPTED)
    {
        Assert(0);
        enmComplType = DXGK_INTERRUPT_DMA_PREEMPTED;
    }
    else
    {
        Assert(0);
        enmComplType = DXGK_INTERRUPT_DMA_FAULTED;
    }

    if (vboxVdmaDdiCmdCompletedIrq(pDevExt, VBOXVDMADDI_CMD_FROM_BUF_DR(pDr), enmComplType))
    {
        pDevExt->bNotifyDxDpc = TRUE;
    }

    /* inform SHGSMI we DO NOT want to be called at DPC later */
    *ppfnCompletion = NULL;
//    *ppvCompletion = pvContext;
}

int vboxVdmaCBufDrSubmit(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo, PVBOXVDMACBUF_DR pDr)
{
    const VBOXSHGSMIHEADER* pHdr = VBoxSHGSMICommandPrepAsynchIrq (&pInfo->CmdHeap, pDr, vboxVdmaCBufDrCompletionIrq, pDevExt, VBOXSHGSMI_FLAG_GH_ASYNCH_FORCE);
    Assert(pHdr);
    int rc = VERR_GENERAL_FAILURE;
    if (pHdr)
    {
        do
        {
            HGSMIOFFSET offCmd = VBoxSHGSMICommandOffset(&pInfo->CmdHeap, pHdr);
            Assert(offCmd != HGSMIOFFSET_VOID);
            if (offCmd != HGSMIOFFSET_VOID)
            {
                rc = vboxWddmVdmaSubmit(pDevExt, pInfo, offCmd);
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    VBoxSHGSMICommandDoneAsynch(&pInfo->CmdHeap, pHdr);
                    AssertRC(rc);
                    break;
                }
            }
            else
                rc = VERR_INVALID_PARAMETER;
            /* fail to submit, cancel it */
            VBoxSHGSMICommandCancelAsynch(&pInfo->CmdHeap, pHdr);
        } while (0);
    }
    else
        rc = VERR_INVALID_PARAMETER;
    return rc;
}

int vboxVdmaCBufDrSubmitSynch(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo, PVBOXVDMACBUF_DR pDr)
{
    const VBOXSHGSMIHEADER* pHdr = VBoxSHGSMICommandPrepAsynch (&pInfo->CmdHeap, pDr, NULL, NULL, VBOXSHGSMI_FLAG_GH_SYNCH);
    Assert(pHdr);
    int rc = VERR_GENERAL_FAILURE;
    if (pHdr)
    {
        do
        {
            HGSMIOFFSET offCmd = VBoxSHGSMICommandOffset(&pInfo->CmdHeap, pHdr);
            Assert(offCmd != HGSMIOFFSET_VOID);
            if (offCmd != HGSMIOFFSET_VOID)
            {
                rc = vboxWddmVdmaSubmit(pDevExt, pInfo, offCmd);
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    VBoxSHGSMICommandDoneAsynch(&pInfo->CmdHeap, pHdr);
                    AssertRC(rc);
                    break;
                }
            }
            else
                rc = VERR_INVALID_PARAMETER;
            /* fail to submit, cancel it */
            VBoxSHGSMICommandCancelAsynch(&pInfo->CmdHeap, pHdr);
        } while (0);
    }
    else
        rc = VERR_INVALID_PARAMETER;
    return rc;
}
#endif


/* ddi dma command queue */

VOID vboxVdmaDdiCmdGetCompletedListIsr(PVBOXMP_DEVEXT pDevExt, LIST_ENTRY *pList)
{
    vboxVideoLeDetach(&pDevExt->DpcCmdQueue, pList);
}

BOOLEAN vboxVdmaDdiCmdIsCompletedListEmptyIsr(PVBOXMP_DEVEXT pDevExt)
{
    return IsListEmpty(&pDevExt->DpcCmdQueue);
}

DECLINLINE(BOOLEAN) vboxVdmaDdiCmdCanComplete(PVBOXMP_DEVEXT pDevExt, UINT u32NodeOrdinal)
{
    PVBOXVDMADDI_CMD_QUEUE pQueue = &pDevExt->aNodes[u32NodeOrdinal].CmdQueue;
    return ASMAtomicUoReadU32(&pQueue->cQueuedCmds) == 0;
}

DECLCALLBACK(VOID) vboxVdmaDdiCmdCompletionCbFree(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext)
{
    vboxWddmMemFree(pCmd);
}

static VOID vboxVdmaDdiCmdNotifyCompletedIrq(PVBOXMP_DEVEXT pDevExt, UINT u32NodeOrdinal, UINT u32FenceId, DXGK_INTERRUPT_TYPE enmComplType)
{
    PVBOXVDMADDI_NODE pNode = &pDevExt->aNodes[u32NodeOrdinal];
    DXGKARGCB_NOTIFY_INTERRUPT_DATA notify;
    memset(&notify, 0, sizeof(DXGKARGCB_NOTIFY_INTERRUPT_DATA));
    switch (enmComplType)
    {
        case DXGK_INTERRUPT_DMA_COMPLETED:
            notify.InterruptType = DXGK_INTERRUPT_DMA_COMPLETED;
            notify.DmaCompleted.SubmissionFenceId = u32FenceId;
            notify.DmaCompleted.NodeOrdinal = u32NodeOrdinal;
            pNode->uLastCompletedFenceId = u32FenceId;
            break;

        case DXGK_INTERRUPT_DMA_PREEMPTED:
            Assert(0);
            notify.InterruptType = DXGK_INTERRUPT_DMA_PREEMPTED;
            notify.DmaPreempted.PreemptionFenceId = u32FenceId;
            notify.DmaPreempted.NodeOrdinal = u32NodeOrdinal;
            notify.DmaPreempted.LastCompletedFenceId = pNode->uLastCompletedFenceId;
            break;

        case DXGK_INTERRUPT_DMA_FAULTED:
            Assert(0);
            notify.InterruptType = DXGK_INTERRUPT_DMA_FAULTED;
            notify.DmaFaulted.FaultedFenceId = u32FenceId;
            notify.DmaFaulted.Status = STATUS_UNSUCCESSFUL; /* @todo: better status ? */
            notify.DmaFaulted.NodeOrdinal = u32NodeOrdinal;
            break;

        default:
            Assert(0);
            break;
    }

    pDevExt->u.primary.DxgkInterface.DxgkCbNotifyInterrupt(pDevExt->u.primary.DxgkInterface.DeviceHandle, &notify);
}

static VOID vboxVdmaDdiCmdProcessCompletedIrq(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, DXGK_INTERRUPT_TYPE enmComplType)
{
    vboxVdmaDdiCmdNotifyCompletedIrq(pDevExt, pCmd->u32NodeOrdinal, pCmd->u32FenceId, enmComplType);
    switch (enmComplType)
    {
        case DXGK_INTERRUPT_DMA_COMPLETED:
            InsertTailList(&pDevExt->DpcCmdQueue, &pCmd->QueueEntry);
            break;
        default:
            AssertFailed();
            break;
    }
}

DECLINLINE(VOID) vboxVdmaDdiCmdDequeueIrq(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd)
{
    PVBOXVDMADDI_CMD_QUEUE pQueue = &pDevExt->aNodes[pCmd->u32NodeOrdinal].CmdQueue;
    ASMAtomicDecU32(&pQueue->cQueuedCmds);
    RemoveEntryList(&pCmd->QueueEntry);
}

DECLINLINE(VOID) vboxVdmaDdiCmdEnqueueIrq(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd)
{
    PVBOXVDMADDI_CMD_QUEUE pQueue = &pDevExt->aNodes[pCmd->u32NodeOrdinal].CmdQueue;
    ASMAtomicIncU32(&pQueue->cQueuedCmds);
    InsertTailList(&pQueue->CmdQueue, &pCmd->QueueEntry);
}

VOID vboxVdmaDdiNodesInit(PVBOXMP_DEVEXT pDevExt)
{
    for (UINT i = 0; i < RT_ELEMENTS(pDevExt->aNodes); ++i)
    {
        pDevExt->aNodes[i].uLastCompletedFenceId = 0;
        PVBOXVDMADDI_CMD_QUEUE pQueue = &pDevExt->aNodes[i].CmdQueue;
        pQueue->cQueuedCmds = 0;
        InitializeListHead(&pQueue->CmdQueue);
    }
    InitializeListHead(&pDevExt->DpcCmdQueue);
}

BOOLEAN vboxVdmaDdiCmdCompletedIrq(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, DXGK_INTERRUPT_TYPE enmComplType)
{
    if (VBOXVDMADDI_STATE_NOT_DX_CMD == pCmd->enmState)
    {
        InsertTailList(&pDevExt->DpcCmdQueue, &pCmd->QueueEntry);
        return FALSE;
    }

    PVBOXVDMADDI_CMD_QUEUE pQueue = &pDevExt->aNodes[pCmd->u32NodeOrdinal].CmdQueue;
    BOOLEAN bQueued = pCmd->enmState > VBOXVDMADDI_STATE_NOT_QUEUED;
    BOOLEAN bComplete = FALSE;
    Assert(!bQueued || pQueue->cQueuedCmds);
    Assert(!bQueued || !IsListEmpty(&pQueue->CmdQueue));
    pCmd->enmState = VBOXVDMADDI_STATE_COMPLETED;
    if (bQueued)
    {
        if (pQueue->CmdQueue.Flink == &pCmd->QueueEntry)
        {
            vboxVdmaDdiCmdDequeueIrq(pDevExt, pCmd);
            bComplete = TRUE;
        }
    }
    else if (IsListEmpty(&pQueue->CmdQueue))
    {
        bComplete = TRUE;
    }
    else
    {
        vboxVdmaDdiCmdEnqueueIrq(pDevExt, pCmd);
    }

    if (bComplete)
    {
        vboxVdmaDdiCmdProcessCompletedIrq(pDevExt, pCmd, enmComplType);

        while (!IsListEmpty(&pQueue->CmdQueue))
        {
            pCmd = VBOXVDMADDI_CMD_FROM_ENTRY(pQueue->CmdQueue.Flink);
            if (pCmd->enmState == VBOXVDMADDI_STATE_COMPLETED)
            {
                vboxVdmaDdiCmdDequeueIrq(pDevExt, pCmd);
                vboxVdmaDdiCmdProcessCompletedIrq(pDevExt, pCmd, pCmd->enmComplType);
            }
            else
                break;
        }
    }
    else
    {
        pCmd->enmState = VBOXVDMADDI_STATE_COMPLETED;
        pCmd->enmComplType = enmComplType;
    }

    return bComplete;
}

VOID vboxVdmaDdiCmdSubmittedIrq(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd)
{
    BOOLEAN bQueued = pCmd->enmState >= VBOXVDMADDI_STATE_PENDING;
    Assert(pCmd->enmState < VBOXVDMADDI_STATE_SUBMITTED);
    pCmd->enmState = VBOXVDMADDI_STATE_SUBMITTED;
    if (!bQueued)
        vboxVdmaDdiCmdEnqueueIrq(pDevExt, pCmd);
}

typedef struct VBOXVDMADDI_CMD_COMPLETED_CB
{
    PVBOXMP_DEVEXT pDevExt;
    PVBOXVDMADDI_CMD pCmd;
    DXGK_INTERRUPT_TYPE enmComplType;
} VBOXVDMADDI_CMD_COMPLETED_CB, *PVBOXVDMADDI_CMD_COMPLETED_CB;

static BOOLEAN vboxVdmaDdiCmdCompletedCb(PVOID Context)
{
    PVBOXVDMADDI_CMD_COMPLETED_CB pdc = (PVBOXVDMADDI_CMD_COMPLETED_CB)Context;
    PVBOXMP_DEVEXT pDevExt = pdc->pDevExt;
    BOOLEAN bNeedDpc = vboxVdmaDdiCmdCompletedIrq(pDevExt, pdc->pCmd, pdc->enmComplType);
    pDevExt->bNotifyDxDpc |= bNeedDpc;

    if (bNeedDpc)
    {
        pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
    }

    return bNeedDpc;
}

NTSTATUS vboxVdmaDdiCmdCompleted(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, DXGK_INTERRUPT_TYPE enmComplType)
{
    VBOXVDMADDI_CMD_COMPLETED_CB context;
    context.pDevExt = pDevExt;
    context.pCmd = pCmd;
    context.enmComplType = enmComplType;
    BOOLEAN bNeedDps;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vboxVdmaDdiCmdCompletedCb,
            &context,
            0, /* IN ULONG MessageNumber */
            &bNeedDps);
    Assert(Status == STATUS_SUCCESS);
    return Status;
}

typedef struct VBOXVDMADDI_CMD_SUBMITTED_CB
{
    PVBOXMP_DEVEXT pDevExt;
    PVBOXVDMADDI_CMD pCmd;
} VBOXVDMADDI_CMD_SUBMITTED_CB, *PVBOXVDMADDI_CMD_SUBMITTED_CB;

static BOOLEAN vboxVdmaDdiCmdSubmittedCb(PVOID Context)
{
    PVBOXVDMADDI_CMD_SUBMITTED_CB pdc = (PVBOXVDMADDI_CMD_SUBMITTED_CB)Context;
    vboxVdmaDdiCmdSubmittedIrq(pdc->pDevExt, pdc->pCmd);

    return FALSE;
}

NTSTATUS vboxVdmaDdiCmdSubmitted(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd)
{
    VBOXVDMADDI_CMD_SUBMITTED_CB context;
    context.pDevExt = pDevExt;
    context.pCmd = pCmd;
    BOOLEAN bRc;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vboxVdmaDdiCmdSubmittedCb,
            &context,
            0, /* IN ULONG MessageNumber */
            &bRc);
    Assert(Status == STATUS_SUCCESS);
    return Status;
}

typedef struct VBOXVDMADDI_CMD_COMPLETE_CB
{
    PVBOXMP_DEVEXT pDevExt;
    UINT u32NodeOrdinal;
    uint32_t u32FenceId;
} VBOXVDMADDI_CMD_COMPLETE_CB, *PVBOXVDMADDI_CMD_COMPLETE_CB;

static BOOLEAN vboxVdmaDdiCmdFenceCompleteCb(PVOID Context)
{
    PVBOXVDMADDI_CMD_COMPLETE_CB pdc = (PVBOXVDMADDI_CMD_COMPLETE_CB)Context;
    PVBOXMP_DEVEXT pDevExt = pdc->pDevExt;

    vboxVdmaDdiCmdNotifyCompletedIrq(pDevExt, pdc->u32NodeOrdinal, pdc->u32FenceId, DXGK_INTERRUPT_DMA_COMPLETED);

    pDevExt->bNotifyDxDpc = TRUE;
    pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);

    return TRUE;
}

static NTSTATUS vboxVdmaDdiCmdFenceNotifyComplete(PVBOXMP_DEVEXT pDevExt, uint32_t u32NodeOrdinal, uint32_t u32FenceId)
{
    VBOXVDMADDI_CMD_COMPLETE_CB context;
    context.pDevExt = pDevExt;
    context.u32NodeOrdinal = u32NodeOrdinal;
    context.u32FenceId = u32FenceId;
    BOOLEAN bRet;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vboxVdmaDdiCmdFenceCompleteCb,
            &context,
            0, /* IN ULONG MessageNumber */
            &bRet);
    Assert(Status == STATUS_SUCCESS);
    return Status;
}

NTSTATUS vboxVdmaDdiCmdFenceComplete(PVBOXMP_DEVEXT pDevExt, uint32_t u32NodeOrdinal, uint32_t u32FenceId, DXGK_INTERRUPT_TYPE enmComplType)
{
    if (vboxVdmaDdiCmdCanComplete(pDevExt, u32NodeOrdinal))
        return vboxVdmaDdiCmdFenceNotifyComplete(pDevExt, u32NodeOrdinal, u32FenceId);

    PVBOXVDMADDI_CMD pCmd = (PVBOXVDMADDI_CMD)vboxWddmMemAlloc(sizeof (VBOXVDMADDI_CMD));
    Assert(pCmd);
    if (pCmd)
    {
        vboxVdmaDdiCmdInit(pCmd, u32NodeOrdinal, u32FenceId, vboxVdmaDdiCmdCompletionCbFree, NULL);
        NTSTATUS Status = vboxVdmaDdiCmdCompleted(pDevExt, pCmd, enmComplType);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
            return STATUS_SUCCESS;
        vboxWddmMemFree(pCmd);
        return Status;
    }
    return STATUS_NO_MEMORY;
}

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
NTSTATUS vboxVdmaHlpUpdatePrimary(PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, RECT* pRect)
{
    PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[VidPnSourceId];
    Assert(pSource->pPrimaryAllocation);
    Assert(pSource->pShadowAllocation);
    if (!pSource->pPrimaryAllocation)
        return STATUS_INVALID_PARAMETER;
    if (!pSource->pShadowAllocation)
        return STATUS_INVALID_PARAMETER;

    Assert(pSource->pPrimaryAllocation->offVram != VBOXVIDEOOFFSET_VOID);
    Assert(pSource->pShadowAllocation->offVram != VBOXVIDEOOFFSET_VOID);
    if (pSource->pPrimaryAllocation->offVram == VBOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;
    if (pSource->pShadowAllocation->offVram == VBOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;

    NTSTATUS Status = vboxVdmaGgDmaBltPerform(pDevExt, pSource->pShadowAllocation, pRect, pSource->pPrimaryAllocation, pRect);
    Assert(Status == STATUS_SUCCESS);
    return Status;
}
#endif
