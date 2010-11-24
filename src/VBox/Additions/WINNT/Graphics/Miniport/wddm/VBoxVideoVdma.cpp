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

#include "../VBoxVideo-win.h"
#include "../Helper.h"

#include <VBox/VBoxGuestLib.h>
#include <VBox/VBoxVideo.h>
#include "VBoxVideoVdma.h"
#include "../VBoxVideo.h"

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

PVBOXVDMAPIPE_CMD_DR vboxVdmaGgCmdCreate(PVBOXVDMAGG pVdma, VBOXVDMAPIPE_CMD_TYPE enmType, uint32_t cbCmd)
{
    PVBOXVDMAPIPE_CMD_DR pHdr = (PVBOXVDMAPIPE_CMD_DR)vboxWddmMemAllocZero(cbCmd);
    Assert(pHdr);
    if (pHdr)
    {
        pHdr->enmType = enmType;
        return pHdr;
    }
    return NULL;
}

void vboxVdmaGgCmdDestroy(PVBOXVDMAPIPE_CMD_DR pDr)
{
    vboxWddmMemFree(pDr);
}

DECLCALLBACK(VOID) vboxVdmaGgDdiCmdDestroy(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext)
{
    vboxVdmaGgCmdDestroy((PVBOXVDMAPIPE_CMD_DR)pvContext);
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
static NTSTATUS vboxVdmaGgDirtyRectsProcess(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain, VBOXVDMAPIPE_RECTS *pContextRects)
{
    PVBOXWDDM_RECTS_INFO pRects = &pContextRects->UpdateRects;
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDEOCM_CMD_RECTS_INTERNAL pCmdInternal = NULL;
    uint32_t cbCmdInternal = VBOXVIDEOCM_CMD_RECTS_INTERNAL_SIZE4CRECTS(pRects->cRects);
    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
    ExAcquireFastMutex(&pDevExt->ContextMutex);
    for (PLIST_ENTRY pCur = pDevExt->SwapchainList3D.Flink; pCur != &pDevExt->SwapchainList3D; pCur = pCur->Flink)
    {
        if (pCur != &pSwapchain->DevExtListEntry)
        {
            PVBOXWDDM_SWAPCHAIN pCurSwapchain = VBOXWDDMENTRY_2_SWAPCHAIN(pCur);
            if (!pCmdInternal)
            {
                pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdCreate(&pCurSwapchain->pContext->CmContext, cbCmdInternal);
                Assert(pCmdInternal);
                if (!pCmdInternal)
                {
                    Status = STATUS_NO_MEMORY;
                    break;
                }
            }
            else
            {
                pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdReinitForContext(pCmdInternal, &pCurSwapchain->pContext->CmContext);
            }

            vboxVdmaDirtyRectsCalcIntersection(&pCurSwapchain->ViewRect, pRects, &pCmdInternal->Cmd.RectsInfo);
            if (pCmdInternal->Cmd.RectsInfo.cRects)
            {
                bool bSend = false;
                pCmdInternal->Cmd.fFlags.Value = 0;
                pCmdInternal->Cmd.fFlags.bAddHiddenRects = 1;
                if (pCurSwapchain->pLastReportedRects)
                {
                    if (pCurSwapchain->pLastReportedRects->Cmd.fFlags.bSetVisibleRects)
                    {
                        RECT *paPrevRects;
                        uint32_t cPrevRects;
                        if (pCurSwapchain->pLastReportedRects->Cmd.fFlags.bSetViewRect)
                        {
                            paPrevRects = &pCurSwapchain->pLastReportedRects->Cmd.RectsInfo.aRects[1];
                            cPrevRects = pCurSwapchain->pLastReportedRects->Cmd.RectsInfo.cRects - 1;
                        }
                        else
                        {
                            paPrevRects = &pCurSwapchain->pLastReportedRects->Cmd.RectsInfo.aRects[0];
                            cPrevRects = pCurSwapchain->pLastReportedRects->Cmd.RectsInfo.cRects;
                        }

                        if (vboxVdmaDirtyRectsHasIntersections(paPrevRects, cPrevRects,
                                pCmdInternal->Cmd.RectsInfo.aRects, pCmdInternal->Cmd.RectsInfo.cRects))
                        {
                            bSend = true;
                        }
                    }
                    else
                    {
                        Assert(pCurSwapchain->pLastReportedRects->Cmd.fFlags.bAddHiddenRects);
                        if (!vboxVdmaDirtyRectsIsCover(pCurSwapchain->pLastReportedRects->Cmd.RectsInfo.aRects,
                                pCurSwapchain->pLastReportedRects->Cmd.RectsInfo.cRects,
                                pCmdInternal->Cmd.RectsInfo.aRects, pCmdInternal->Cmd.RectsInfo.cRects))
                        {
                            bSend = true;
                        }
                    }
                }
                else
                    bSend = true;

                if (bSend)
                {
                    if (pCurSwapchain->pLastReportedRects)
                        vboxVideoCmCmdRelease(pCurSwapchain->pLastReportedRects);

                    pCmdInternal->hSwapchainUm = pCurSwapchain->hSwapchainUm;

                    vboxVideoCmCmdRetain(pCmdInternal);
                    pCurSwapchain->pLastReportedRects = pCmdInternal;
                    vboxVideoCmCmdSubmit(pCmdInternal, VBOXVIDEOCM_CMD_RECTS_INTERNAL_SIZE4CRECTS(pCmdInternal->Cmd.RectsInfo.cRects));
                    pCmdInternal = NULL;
                }
            }
        }
        else
        {
            RECT * pContextRect = &pContextRects->ContextRect;
            bool bRectShanged = (pSwapchain->ViewRect.left != pContextRect->left
                    || pSwapchain->ViewRect.top != pContextRect->top
                    || pSwapchain->ViewRect.right != pContextRect->right
                    || pSwapchain->ViewRect.bottom != pContextRect->bottom);
            PVBOXVIDEOCM_CMD_RECTS_INTERNAL pDrCmdInternal;

            bool bSend = false;

            if (bRectShanged)
            {
                uint32_t cbDrCmdInternal = VBOXVIDEOCM_CMD_RECTS_INTERNAL_SIZE4CRECTS(pRects->cRects + 1);
                pDrCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdCreate(&pContext->CmContext, cbDrCmdInternal);
                Assert(pDrCmdInternal);
                if (!pDrCmdInternal)
                {
                    Status = STATUS_NO_MEMORY;
                    break;
                }
                pDrCmdInternal->Cmd.fFlags.Value = 0;
                pDrCmdInternal->Cmd.RectsInfo.cRects = pRects->cRects + 1;
                pDrCmdInternal->Cmd.fFlags.bSetViewRect = 1;
                pDrCmdInternal->Cmd.RectsInfo.aRects[0] = *pContextRect;
                pSwapchain->ViewRect = *pContextRect;
                memcpy(&pDrCmdInternal->Cmd.RectsInfo.aRects[1], pRects->aRects, sizeof (RECT) * pRects->cRects);
                bSend = true;
            }
            else
            {
                if (pCmdInternal)
                {
                    pDrCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdReinitForContext(pCmdInternal, &pContext->CmContext);
                    pCmdInternal = NULL;
                }
                else
                {
                    pDrCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdCreate(&pContext->CmContext, cbCmdInternal);
                    Assert(pDrCmdInternal);
                    if (!pDrCmdInternal)
                    {
                        Status = STATUS_NO_MEMORY;
                        break;
                    }
                }
                pDrCmdInternal->Cmd.fFlags.Value = 0;
                pDrCmdInternal->Cmd.RectsInfo.cRects = pRects->cRects;
                memcpy(&pDrCmdInternal->Cmd.RectsInfo.aRects[0], pRects->aRects, sizeof (RECT) * pRects->cRects);

                if (pSwapchain->pLastReportedRects)
                {
                    if (pSwapchain->pLastReportedRects->Cmd.fFlags.bSetVisibleRects)
                    {
                        RECT *paRects;
                        uint32_t cRects;
                        if (pSwapchain->pLastReportedRects->Cmd.fFlags.bSetViewRect)
                        {
                            paRects = &pSwapchain->pLastReportedRects->Cmd.RectsInfo.aRects[1];
                            cRects = pSwapchain->pLastReportedRects->Cmd.RectsInfo.cRects - 1;
                        }
                        else
                        {
                            paRects = &pSwapchain->pLastReportedRects->Cmd.RectsInfo.aRects[0];
                            cRects = pSwapchain->pLastReportedRects->Cmd.RectsInfo.cRects;
                        }
                        bSend = (pDrCmdInternal->Cmd.RectsInfo.cRects != cRects)
                                || memcmp(paRects, pDrCmdInternal->Cmd.RectsInfo.aRects, cRects * sizeof (RECT));
                    }
                    else
                    {
                        Assert(pSwapchain->pLastReportedRects->Cmd.fFlags.bAddHiddenRects);
                        bSend = true;
                    }
                }
                else
                    bSend = true;

            }

            Assert(pRects->cRects);
            if (bSend)
            {
                if (pSwapchain->pLastReportedRects)
                    vboxVideoCmCmdRelease(pSwapchain->pLastReportedRects);

                pDrCmdInternal->Cmd.fFlags.bSetVisibleRects = 1;
                pDrCmdInternal->hSwapchainUm = pSwapchain->hSwapchainUm;

                vboxVideoCmCmdRetain(pDrCmdInternal);
                pSwapchain->pLastReportedRects = pDrCmdInternal;
                vboxVideoCmCmdSubmit(pDrCmdInternal, VBOXVIDEOCM_SUBMITSIZE_DEFAULT);
            }
            else
            {
                if (!pCmdInternal)
                    pCmdInternal = pDrCmdInternal;
                else
                    vboxVideoCmCmdRelease(pDrCmdInternal);
            }
        }
    }
    ExReleaseFastMutex(&pDevExt->ContextMutex);


    if (pCmdInternal)
        vboxVideoCmCmdRelease(pCmdInternal);

    return Status;
}

static NTSTATUS vboxVdmaGgDmaColorFill(PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL pCF)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PVBOXWDDM_CONTEXT pContext = pCF->Hdr.DdiCmd.pContext;
    PDEVICE_EXTENSION pDevExt = pContext->pDevice->pAdapter;
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
                        VBOXVBVA_OP_WITHLOCK(ReportDirtyRect, pDevExt, pSource, &UnionRect);
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

NTSTATUS vboxVdmaGgDmaBltPerform(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pSrcAlloc, RECT* pSrcRect,
        PVBOXWDDM_ALLOCATION pDstAlloc, RECT* pDstRect)
{
    uint8_t* pvVramBase = pDevExt->pvVisibleVram;
    /* we do not support color conversion */
    Assert(pSrcAlloc->SurfDesc.format == pDstAlloc->SurfDesc.format);
    /* we do not support stretching */
    uint32_t srcWidth = pSrcRect->right - pSrcRect->left;
    uint32_t srcHeight = pSrcRect->bottom - pSrcRect->top;
    uint32_t dstWidth = pDstRect->right - pDstRect->left;
    uint32_t dstHeight = pDstRect->bottom - pDstRect->top;
    Assert(srcHeight == dstHeight);
    Assert(dstWidth == srcWidth);
    Assert(pDstAlloc->offVram != VBOXVIDEOOFFSET_VOID);
    Assert(pSrcAlloc->offVram != VBOXVIDEOOFFSET_VOID);

    if (pSrcAlloc->SurfDesc.format != pDstAlloc->SurfDesc.format)
        return STATUS_INVALID_PARAMETER;
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
        uint32_t cbOff = pDstAlloc->SurfDesc.pitch * pDstRect->top;
        uint32_t cbSize = pDstAlloc->SurfDesc.pitch * dstHeight;
        memcpy(pvDstSurf + cbOff, pvSrcSurf + cbOff, cbSize);
    }
    else
    {
        uint32_t offDstLineStart = pDstRect->left * pDstAlloc->SurfDesc.bpp >> 3;
        uint32_t offDstLineEnd = ((pDstRect->left * pDstAlloc->SurfDesc.bpp + 7) >> 3) + ((pDstAlloc->SurfDesc.bpp * dstWidth + 7) >> 3);
        uint32_t cbDstLine = offDstLineEnd - offDstLineStart;
        uint32_t offDstStart = pDstAlloc->SurfDesc.pitch * pDstRect->top + offDstLineStart;
        Assert(cbDstLine <= pDstAlloc->SurfDesc.pitch);
        uint32_t cbDstSkip = pDstAlloc->SurfDesc.pitch;
        uint8_t * pvDstStart = pvDstSurf + offDstStart;

        uint32_t offSrcLineStart = pSrcRect->left * pSrcAlloc->SurfDesc.bpp >> 3;
        uint32_t offSrcLineEnd = ((pSrcRect->left * pSrcAlloc->SurfDesc.bpp + 7) >> 3) + ((pSrcAlloc->SurfDesc.bpp * srcWidth + 7) >> 3);
        uint32_t cbSrcLine = offSrcLineEnd - offSrcLineStart;
        uint32_t offSrcStart = pSrcAlloc->SurfDesc.pitch * pSrcRect->top + offSrcLineStart;
        Assert(cbSrcLine <= pSrcAlloc->SurfDesc.pitch);
        uint32_t cbSrcSkip = pSrcAlloc->SurfDesc.pitch;
        const uint8_t * pvSrcStart = pvSrcSurf + offSrcStart;

        Assert(cbDstLine == cbSrcLine);

        for (uint32_t i = 0; ; ++i)
        {
            memcpy (pvDstStart, pvSrcStart, cbDstLine);
            if (i == dstHeight)
                break;
            pvDstStart += cbDstSkip;
            pvSrcStart += cbSrcSkip;
        }
    }
    return STATUS_SUCCESS;
}

/*
 * @return on success the number of bytes the command contained, otherwise - VERR_xxx error code
 */
static NTSTATUS vboxVdmaGgDmaBlt(PVBOXVDMAPIPE_CMD_DMACMD_BLT pBlt)
{
    /* we do not support stretching for now */
    Assert(pBlt->Blt.SrcRect.right - pBlt->Blt.SrcRect.left == pBlt->Blt.DstRects.ContextRect.right - pBlt->Blt.DstRects.ContextRect.left);
    Assert(pBlt->Blt.SrcRect.bottom - pBlt->Blt.SrcRect.top == pBlt->Blt.DstRects.ContextRect.bottom - pBlt->Blt.DstRects.ContextRect.top);
    if (pBlt->Blt.SrcRect.right - pBlt->Blt.SrcRect.left != pBlt->Blt.DstRects.ContextRect.right - pBlt->Blt.DstRects.ContextRect.left)
        return STATUS_INVALID_PARAMETER;
    if (pBlt->Blt.SrcRect.bottom - pBlt->Blt.SrcRect.top != pBlt->Blt.DstRects.ContextRect.bottom - pBlt->Blt.DstRects.ContextRect.top)
        return STATUS_INVALID_PARAMETER;
    Assert(pBlt->Blt.DstRects.UpdateRects.cRects);

    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION pDevExt = pBlt->Hdr.pDevExt;

    if (pBlt->Blt.DstRects.UpdateRects.cRects)
    {
        for (uint32_t i = 0; i < pBlt->Blt.DstRects.UpdateRects.cRects; ++i)
        {
            RECT DstRect;
            RECT SrcRect;
            vboxWddmRectTranslated(&DstRect, &pBlt->Blt.DstRects.UpdateRects.aRects[i], pBlt->Blt.DstRects.ContextRect.left, pBlt->Blt.DstRects.ContextRect.top);
            vboxWddmRectTranslated(&SrcRect, &pBlt->Blt.DstRects.UpdateRects.aRects[i], pBlt->Blt.SrcRect.left, pBlt->Blt.SrcRect.top);

            Status = vboxVdmaGgDmaBltPerform(pDevExt, pBlt->Blt.SrcAlloc.pAlloc, &SrcRect,
                    pBlt->Blt.DstAlloc.pAlloc, &DstRect);
            Assert(Status == STATUS_SUCCESS);
            if (Status != STATUS_SUCCESS)
                return Status;
        }
    }
    else
    {
        Status = vboxVdmaGgDmaBltPerform(pDevExt, pBlt->Blt.SrcAlloc.pAlloc, &pBlt->Blt.SrcRect,
                pBlt->Blt.DstAlloc.pAlloc, &pBlt->Blt.DstRects.ContextRect);
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

static NTSTATUS vboxVdmaGgDmaCmdProcess(VBOXVDMAPIPE_CMD_DMACMD *pDmaCmd)
{
    PDEVICE_EXTENSION pDevExt = pDmaCmd->pDevExt;
    PVBOXWDDM_CONTEXT pContext = pDmaCmd->DdiCmd.pContext;
    NTSTATUS Status = STATUS_SUCCESS;
    DXGK_INTERRUPT_TYPE enmComplType = DXGK_INTERRUPT_DMA_COMPLETED;
    switch (pDmaCmd->enmCmd)
    {
        case VBOXVDMACMD_TYPE_DMA_PRESENT_BLT:
        {
            PVBOXVDMAPIPE_CMD_DMACMD_BLT pBlt = (PVBOXVDMAPIPE_CMD_DMACMD_BLT)pDmaCmd;
            PVBOXWDDM_ALLOCATION pDstAlloc = pBlt->Blt.DstAlloc.pAlloc;
            PVBOXWDDM_ALLOCATION pSrcAlloc = pBlt->Blt.SrcAlloc.pAlloc;
            BOOLEAN bComplete = TRUE;
            switch (pDstAlloc->enmType)
            {
                case VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE:
                case VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC:
                {
                    if (pDstAlloc->bAssigned)
                    {
                        VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pDstAlloc->SurfDesc.VidPnSourceId];
                        Assert(pSource->pPrimaryAllocation == pDstAlloc);
                        switch (pSrcAlloc->enmType)
                        {
                            case VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE:
                            {
                                Assert(pContext->enmType == VBOXWDDM_CONTEXT_TYPE_SYSTEM);

                                if (pBlt->Hdr.fFlags.b2DRelated || pBlt->Hdr.fFlags.b3DRelated)
                                {
                                    POINT pos;
                                    BOOLEAN bPosMoved = FALSE;
                                    if (pBlt->Hdr.fFlags.b3DRelated)
                                    {
                                        pos = pSource->VScreenPos;
                                        if (pos.x || pos.y)
                                        {
                                            vboxWddmBltPipeRectsTranslate(&pBlt->Blt.DstRects, pos.x, pos.y);
                                            bPosMoved = TRUE;
                                        }
                                        Status = vboxVdmaGgDirtyRectsProcess(pDevExt, pContext, NULL, &pBlt->Blt.DstRects);
                                        Assert(Status == STATUS_SUCCESS);
                                    }


                                    if (pBlt->Hdr.fFlags.b2DRelated)
                                    {
                                        if (bPosMoved)
                                        {
                                            vboxWddmBltPipeRectsTranslate(&pBlt->Blt.DstRects, -pos.x, -pos.y);
                                        }

                                        RECT OverlayUnionRect;
                                        RECT UpdateRect;
                                        UpdateRect = pBlt->Blt.DstRects.UpdateRects.aRects[0];
                                        for (UINT i = 1; i < pBlt->Blt.DstRects.UpdateRects.cRects; ++i)
                                        {
                                            vboxWddmRectUnite(&UpdateRect, &pBlt->Blt.DstRects.UpdateRects.aRects[i]);
                                        }
                                        vboxVhwaHlpOverlayDstRectUnion(pDevExt, pDstAlloc->SurfDesc.VidPnSourceId, &OverlayUnionRect);
                                        Assert(pBlt->Blt.DstRects.ContextRect.left == 0); /* <-| otherwise we would probably need to translate the UpdateRects to left;top first??*/
                                        Assert(pBlt->Blt.DstRects.ContextRect.top == 0); /* <--| */
                                        vboxVdmaDirtyRectsCalcIntersection(&OverlayUnionRect, &pBlt->Blt.DstRects.UpdateRects, &pBlt->Blt.DstRects.UpdateRects);
                                        if (pBlt->Blt.DstRects.UpdateRects.cRects)
                                        {
                                            vboxVdmaGgDmaBlt(pBlt);
                                        }
                                        VBOXVBVA_OP_WITHLOCK(ReportDirtyRect, pDevExt, pSource, &UpdateRect);
                                    }
                                }

                                break;
                            }
                            case VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC:
                            {
                                Assert(pContext->enmType == VBOXWDDM_CONTEXT_TYPE_CUSTOM_3D);
                                Assert(pSrcAlloc->fRcFlags.RenderTarget);
                                if (pSrcAlloc->fRcFlags.RenderTarget)
                                {
                                    if (pBlt->Hdr.fFlags.b3DRelated)
                                    {
                                        PVBOXWDDM_SWAPCHAIN pSwapchain;
                                        pSwapchain = vboxWddmSwapchainRetainByAlloc(pDevExt, pSrcAlloc);
                                        if (pSwapchain)
                                        {
                                            Status = vboxVdmaGgDirtyRectsProcess(pDevExt, pContext, pSwapchain, &pBlt->Blt.DstRects);
                                            Assert(Status == STATUS_SUCCESS);
                                            vboxWddmSwapchainRelease(pSwapchain);
                                        }
                                    }
                                }
                                break;
                            }
                            default:
                                AssertBreakpoint();
                                break;
                        }
                    }
                    break;
                }
                default:
                    Assert(0);
            }

            break;
        }
        case VBOXVDMACMD_TYPE_DMA_PRESENT_FLIP:
        {
            PVBOXVDMAPIPE_CMD_DMACMD_FLIP pFlip = (PVBOXVDMAPIPE_CMD_DMACMD_FLIP)pDmaCmd;
            Assert(pFlip->Hdr.fFlags.b3DRelated);
            Assert(!pFlip->Hdr.fFlags.bDecVBVAUnlock);
            Assert(!pFlip->Hdr.fFlags.b2DRelated);
            PVBOXWDDM_ALLOCATION pAlloc = pFlip->Flip.Alloc.pAlloc;
            VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pAlloc->SurfDesc.VidPnSourceId];
            if (pFlip->Hdr.fFlags.b3DRelated)
            {
                PVBOXWDDM_SWAPCHAIN pSwapchain;
                pSwapchain = vboxWddmSwapchainRetainByAlloc(pDevExt, pAlloc);
                if (pSwapchain)
                {
                    POINT pos = pSource->VScreenPos;
                    VBOXVDMAPIPE_RECTS Rects;
                    Rects.ContextRect.left = pos.x;
                    Rects.ContextRect.top = pos.y;
                    Rects.ContextRect.right = pAlloc->SurfDesc.width + pos.x;
                    Rects.ContextRect.bottom = pAlloc->SurfDesc.height + pos.y;
                    Rects.UpdateRects.cRects = 1;
                    Rects.UpdateRects.aRects[0] = Rects.ContextRect;
                    Status = vboxVdmaGgDirtyRectsProcess(pDevExt, pContext, pSwapchain, &Rects);
                    Assert(Status == STATUS_SUCCESS);
                    vboxWddmSwapchainRelease(pSwapchain);
                }
            }

            break;
        }
        case VBOXVDMACMD_TYPE_DMA_PRESENT_CLRFILL:
        {
            PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL pCF = (PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL)pDmaCmd;
            Assert(pCF->Hdr.fFlags.b2DRelated);
            Assert(pCF->Hdr.fFlags.bDecVBVAUnlock);
            Assert(!pCF->Hdr.fFlags.b3DRelated);
            Status = vboxVdmaGgDmaColorFill(pCF);
            Assert(Status == STATUS_SUCCESS);
            break;
        }
        default:
            Assert(0);
            break;
    }

    if (pDmaCmd->fFlags.bDecVBVAUnlock)
    {
        uint32_t cNew = ASMAtomicDecU32(&pDevExt->cUnlockedVBVADisabled);
        Assert(cNew < UINT32_MAX/2);
    }

    Status = vboxVdmaDdiCmdCompleted(pDevExt, &pDevExt->DdiCmdQueue, &pDmaCmd->DdiCmd, enmComplType);
    Assert(Status == STATUS_SUCCESS);

    return Status;
}

static VOID vboxVdmaGgWorkerThread(PVOID pvUser)
{
    PVBOXVDMAGG pVdma = (PVBOXVDMAGG)pvUser;

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
#if 0
                        case VBOXVDMAPIPE_CMD_TYPE_RECTSINFO:
                        {
                            PVBOXVDMAPIPE_CMD_RECTSINFO pRects = (PVBOXVDMAPIPE_CMD_RECTSINFO)pDr;
                            Status = vboxVdmaGgDirtyRectsProcess(pRects);
                            Assert(Status == STATUS_SUCCESS);
                            vboxVdmaGgCmdDestroy(pDr);
                            break;
                        }
#endif
                        case VBOXVDMAPIPE_CMD_TYPE_DMACMD:
                        {
                            PVBOXVDMAPIPE_CMD_DMACMD pDmaCmd = (PVBOXVDMAPIPE_CMD_DMACMD)pDr;
                            Status = vboxVdmaGgDmaCmdProcess(pDmaCmd);
                            Assert(Status == STATUS_SUCCESS);
                        } break;
                        case VBOXVDMAPIPE_CMD_TYPE_RECTSINFO:
                        {
                            PVBOXVDMAPIPE_CMD_RECTSINFO pRects = (PVBOXVDMAPIPE_CMD_RECTSINFO)pDr;
                            Status = vboxVdmaGgDirtyRectsProcess(pRects->pDevExt, pRects->pContext, pRects->pSwapchain, &pRects->ContextsRects);
                            Assert(Status == STATUS_SUCCESS);
                            vboxVdmaGgCmdDestroy(pDr);
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

NTSTATUS vboxVdmaGgConstruct(PVBOXVDMAGG pVdma)
{
    NTSTATUS Status = vboxVdmaPipeConstruct(&pVdma->CmdPipe);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxVdmaGgThreadCreate(&pVdma->pThread, vboxVdmaGgWorkerThread, pVdma);
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

NTSTATUS vboxVdmaGgDestruct(PVBOXVDMAGG pVdma)
{
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

NTSTATUS vboxVdmaGgCmdSubmit(PVBOXVDMAGG pVdma, PVBOXVDMAPIPE_CMD_DR pCmd)
{
    return vboxVdmaPipeCltCmdPut(&pVdma->CmdPipe, &pCmd->PipeHdr);
}

/* end */

#ifdef VBOX_WITH_VDMA
/*
 * This is currently used by VDMA. It is invisible for Vdma API clients since
 * Vdma transport may change if we choose to use another (e.g. more light-weight)
 * transport for DMA commands submission
 */

#ifdef VBOXVDMA_WITH_VBVA
static int vboxWddmVdmaSubmitVbva(struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo, HGSMIOFFSET offDr)
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
static int vboxWddmVdmaSubmitHgsmi(struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo, HGSMIOFFSET offDr)
{
    VBoxVideoCmnPortWriteUlong(commonFromDeviceExt(pDevExt)->guestCtx.port, offDr);
    return VINF_SUCCESS;
}
#define vboxWddmVdmaSubmit vboxWddmVdmaSubmitHgsmi
#endif

static int vboxVdmaInformHost(PDEVICE_EXTENSION pDevExt, PVBOXVDMAINFO pInfo, VBOXVDMA_CTL_TYPE enmCtl)
{
    int rc = VINF_SUCCESS;

    PVBOXVDMA_CTL pCmd = (PVBOXVDMA_CTL)VBoxSHGSMICommandAlloc(&commonFromDeviceExt(pDevExt)->guestCtx.heapCtx, sizeof (VBOXVDMA_CTL), HGSMI_CH_VBVA, VBVA_VDMA_CTL);
    if (pCmd)
    {
        pCmd->enmCtl = enmCtl;
        pCmd->u32Offset = pInfo->CmdHeap.area.offBase;
        pCmd->i32Result = VERR_NOT_SUPPORTED;

        const VBOXSHGSMIHEADER* pHdr = VBoxSHGSMICommandPrepSynch(&commonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pCmd);
        Assert(pHdr);
        if (pHdr)
        {
            do
            {
                HGSMIOFFSET offCmd = VBoxSHGSMICommandOffset(&commonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pHdr);
                Assert(offCmd != HGSMIOFFSET_VOID);
                if (offCmd != HGSMIOFFSET_VOID)
                {
                    rc = vboxWddmVdmaSubmit(pDevExt, pInfo, offCmd);
                    AssertRC(rc);
                    if (RT_SUCCESS(rc))
                    {
                        rc = VBoxSHGSMICommandDoneSynch(&commonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pHdr);
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
                VBoxSHGSMICommandCancelSynch(&commonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pHdr);
            } while (0);
        }

        VBoxSHGSMICommandFree (&commonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pCmd);
    }
    else
    {
        drprintf((__FUNCTION__": HGSMIHeapAlloc failed\n"));
        rc = VERR_OUT_OF_RESOURCES;
    }

    return rc;
}
#endif

/* create a DMACommand buffer */
int vboxVdmaCreate(PDEVICE_EXTENSION pDevExt, VBOXVDMAINFO *pInfo
#ifdef VBOX_WITH_VDMA
        , ULONG offBuffer, ULONG cbBuffer
#endif
        )
{
    int rc;
    pInfo->fEnabled           = FALSE;

#ifdef VBOX_WITH_VDMA
    KeInitializeSpinLock(&pInfo->HeapLock);
    Assert((offBuffer & 0xfff) == 0);
    Assert((cbBuffer & 0xfff) == 0);
    Assert(offBuffer);
    Assert(cbBuffer);

    if((offBuffer & 0xfff)
            || (cbBuffer & 0xfff)
            || !offBuffer
            || !cbBuffer)
    {
        drprintf((__FUNCTION__": invalid parameters: offBuffer(0x%x), cbBuffer(0x%x)", offBuffer, cbBuffer));
        return VERR_INVALID_PARAMETER;
    }
    PVOID pvBuffer;

    rc = VBoxMapAdapterMemory (commonFromDeviceExt(pDevExt),
                                   &pvBuffer,
                                   offBuffer,
                                   cbBuffer);
    Assert(RT_SUCCESS(rc));
    if (RT_SUCCESS(rc))
    {
        /* Setup a HGSMI heap within the adapter information area. */
        rc = HGSMIHeapSetup (&pInfo->CmdHeap,
                             pvBuffer,
                             cbBuffer,
                             offBuffer,
                             false /*fOffsetBased*/);
        Assert(RT_SUCCESS(rc));
        if(RT_SUCCESS(rc))
#endif
        {
            NTSTATUS Status = vboxVdmaGgConstruct(&pInfo->DmaGg);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
                return VINF_SUCCESS;
            rc = VERR_GENERAL_FAILURE;
        }
#ifdef VBOX_WITH_VDMA
        else
            drprintf((__FUNCTION__": HGSMIHeapSetup failed rc = 0x%x\n", rc));

        VBoxUnmapAdapterMemory(commonFromDeviceExt(pDevExt), &pvBuffer);
    }
    else
        drprintf((__FUNCTION__": VBoxMapAdapterMemory failed rc = 0x%x\n", rc));
#endif
    return rc;
}

int vboxVdmaDisable (PDEVICE_EXTENSION pDevExt, PVBOXVDMAINFO pInfo)
{
    dfprintf((__FUNCTION__"\n"));

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

int vboxVdmaEnable (PDEVICE_EXTENSION pDevExt, PVBOXVDMAINFO pInfo)
{
    dfprintf((__FUNCTION__"\n"));

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
int vboxVdmaFlush (PDEVICE_EXTENSION pDevExt, PVBOXVDMAINFO pInfo)
{
    dfprintf((__FUNCTION__"\n"));

    Assert(pInfo->fEnabled);
    if (!pInfo->fEnabled)
        return VINF_ALREADY_INITIALIZED;

    int rc = vboxVdmaInformHost (pDevExt, pInfo, VBOXVDMA_CTL_TYPE_FLUSH);
    Assert(RT_SUCCESS(rc));

    return rc;
}
#endif

int vboxVdmaDestroy (PDEVICE_EXTENSION pDevExt, PVBOXVDMAINFO pInfo)
{
    int rc = VINF_SUCCESS;
    NTSTATUS Status = vboxVdmaGgDestruct(&pInfo->DmaGg);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Assert(!pInfo->fEnabled);
        if (pInfo->fEnabled)
            rc = vboxVdmaDisable (pDevExt, pInfo);
#ifdef VBOX_WITH_VDMA
        VBoxUnmapAdapterMemory (commonFromDeviceExt(pDevExt), (void**)&pInfo->CmdHeap.area.pu8Base);
#endif
    }
    else
        rc = VERR_GENERAL_FAILURE;
    return rc;
}

#ifdef VBOX_WITH_VDMA
void vboxVdmaCBufDrFree (PVBOXVDMAINFO pInfo, PVBOXVDMACBUF_DR pDr)
{
    KIRQL OldIrql;
    KeAcquireSpinLock(&pInfo->HeapLock, &OldIrql);
    VBoxSHGSMICommandFree (&pInfo->CmdHeap, pDr);
    KeReleaseSpinLock(&pInfo->HeapLock, OldIrql);
}

PVBOXVDMACBUF_DR vboxVdmaCBufDrCreate (PVBOXVDMAINFO pInfo, uint32_t cbTrailingData)
{
    uint32_t cbDr = VBOXVDMACBUF_DR_SIZE(cbTrailingData);
    KIRQL OldIrql;
    KeAcquireSpinLock(&pInfo->HeapLock, &OldIrql);
    PVBOXVDMACBUF_DR pDr = (PVBOXVDMACBUF_DR)VBoxSHGSMICommandAlloc (&pInfo->CmdHeap, cbDr, HGSMI_CH_VBVA, VBVA_VDMA_CMD);
    KeReleaseSpinLock(&pInfo->HeapLock, OldIrql);
    Assert(pDr);
    if (pDr)
        memset (pDr, 0, cbDr);
    else
        drprintf((__FUNCTION__": VBoxSHGSMICommandAlloc returned NULL\n"));

    return pDr;
}

static DECLCALLBACK(void) vboxVdmaCBufDrCompletion(struct _HGSMIHEAP * pHeap, void *pvCmd, void *pvContext)
{
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pvContext;
    PVBOXVDMAINFO pInfo = &pDevExt->u.primary.Vdma;

    vboxVdmaCBufDrFree (pInfo, (PVBOXVDMACBUF_DR)pvCmd);
}

static DECLCALLBACK(void) vboxVdmaCBufDrCompletionIrq(struct _HGSMIHEAP * pHeap, void *pvCmd, void *pvContext,
                                        PFNVBOXSHGSMICMDCOMPLETION *ppfnCompletion, void **ppvCompletion)
{
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pvContext;
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

    if (vboxVdmaDdiCmdCompletedIrq(pDevExt, &pDevExt->DdiCmdQueue, VBOXVDMADDI_CMD_FROM_BUF_DR(pDr), enmComplType))
    {
        pDevExt->bNotifyDxDpc = TRUE;
    }

    /* inform SHGSMI we DO NOT want to be called at DPC later */
    *ppfnCompletion = NULL;
//    *ppvCompletion = pvContext;
}

int vboxVdmaCBufDrSubmit(PDEVICE_EXTENSION pDevExt, PVBOXVDMAINFO pInfo, PVBOXVDMACBUF_DR pDr)
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

int vboxVdmaCBufDrSubmitSynch(PDEVICE_EXTENSION pDevExt, PVBOXVDMAINFO pInfo, PVBOXVDMACBUF_DR pDr)
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
DECLINLINE(BOOLEAN) vboxVdmaDdiCmdCanComplete(PVBOXVDMADDI_CMD_QUEUE pQueue)
{
    return ASMAtomicUoReadU32(&pQueue->cQueuedCmds) == 0;
}

DECLCALLBACK(VOID) vboxVdmaDdiCmdCompletionCbFree(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext)
{
    vboxWddmMemFree(pCmd);
}

static VOID vboxVdmaDdiCmdNotifyCompletedIrq(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD_QUEUE pQueue, PVBOXVDMADDI_CMD pCmd, DXGK_INTERRUPT_TYPE enmComplType)
{
    DXGKARGCB_NOTIFY_INTERRUPT_DATA notify;
    memset(&notify, 0, sizeof(DXGKARGCB_NOTIFY_INTERRUPT_DATA));
    switch (enmComplType)
    {
        case DXGK_INTERRUPT_DMA_COMPLETED:
            notify.InterruptType = DXGK_INTERRUPT_DMA_COMPLETED;
            notify.DmaCompleted.SubmissionFenceId = pCmd->u32FenceId;
//            if (pCmd->pContext)
//            {
//                notify.DmaCompleted.NodeOrdinal = pCmd->pContext->NodeOrdinal;
//                pCmd->pContext->uLastCompletedCmdFenceId = pCmd->u32FenceId;
//            }
//            else
            {
                pDevExt->u.primary.Vdma.uLastCompletedPagingBufferCmdFenceId = pCmd->u32FenceId;
            }

            InsertTailList(&pQueue->DpcCmdQueue, &pCmd->QueueEntry);

            break;
        case DXGK_INTERRUPT_DMA_PREEMPTED:
            Assert(0);
            notify.InterruptType = DXGK_INTERRUPT_DMA_PREEMPTED;
            notify.DmaPreempted.PreemptionFenceId = pCmd->u32FenceId;
//            if (pCmd->pContext)
//            {
//                notify.DmaPreempted.LastCompletedFenceId = pCmd->pContext->uLastCompletedCmdFenceId;
//                notify.DmaPreempted.NodeOrdinal = pCmd->pContext->NodeOrdinal;
//            }
//            else
            {
                notify.DmaPreempted.LastCompletedFenceId = pDevExt->u.primary.Vdma.uLastCompletedPagingBufferCmdFenceId;
            }
            break;

        case DXGK_INTERRUPT_DMA_FAULTED:
            Assert(0);
            notify.InterruptType = DXGK_INTERRUPT_DMA_FAULTED;
            notify.DmaFaulted.FaultedFenceId = pCmd->u32FenceId;
            notify.DmaFaulted.Status = STATUS_UNSUCCESSFUL; /* @todo: better status ? */
            if (pCmd->pContext)
            {
                notify.DmaFaulted.NodeOrdinal = pCmd->pContext->NodeOrdinal;
            }
            break;
        default:
            Assert(0);
            break;
    }

    pDevExt->u.primary.DxgkInterface.DxgkCbNotifyInterrupt(pDevExt->u.primary.DxgkInterface.DeviceHandle, &notify);
}

DECLINLINE(VOID) vboxVdmaDdiCmdDequeueIrq(PVBOXVDMADDI_CMD_QUEUE pQueue, PVBOXVDMADDI_CMD pCmd)
{
    ASMAtomicDecU32(&pQueue->cQueuedCmds);
    RemoveEntryList(&pCmd->QueueEntry);
}

DECLINLINE(VOID) vboxVdmaDdiCmdEnqueueIrq(PVBOXVDMADDI_CMD_QUEUE pQueue, PVBOXVDMADDI_CMD pCmd)
{
    ASMAtomicIncU32(&pQueue->cQueuedCmds);
    InsertTailList(&pQueue->CmdQueue, &pCmd->QueueEntry);
}

VOID vboxVdmaDdiQueueInit(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD_QUEUE pQueue)
{
    pQueue->cQueuedCmds = 0;
    InitializeListHead(&pQueue->CmdQueue);
    InitializeListHead(&pQueue->DpcCmdQueue);
}

BOOLEAN vboxVdmaDdiCmdCompletedIrq(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD_QUEUE pQueue, PVBOXVDMADDI_CMD pCmd, DXGK_INTERRUPT_TYPE enmComplType)
{
    if (VBOXVDMADDI_STATE_NOT_DX_CMD == pCmd->enmState)
    {
        InsertTailList(&pQueue->DpcCmdQueue, &pCmd->QueueEntry);
        return FALSE;
    }

    BOOLEAN bQueued = pCmd->enmState > VBOXVDMADDI_STATE_NOT_QUEUED;
    BOOLEAN bComplete = FALSE;
    Assert(!bQueued || pQueue->cQueuedCmds);
    Assert(!bQueued || !IsListEmpty(&pQueue->CmdQueue));
    pCmd->enmState = VBOXVDMADDI_STATE_COMPLETED;
    if (bQueued)
    {
        if (pQueue->CmdQueue.Flink == &pCmd->QueueEntry)
        {
            vboxVdmaDdiCmdDequeueIrq(pQueue, pCmd);
            bComplete = TRUE;
        }
    }
    else if (IsListEmpty(&pQueue->CmdQueue))
    {
        bComplete = TRUE;
    }
    else
    {
        vboxVdmaDdiCmdEnqueueIrq(pQueue, pCmd);
    }

    if (bComplete)
    {
        vboxVdmaDdiCmdNotifyCompletedIrq(pDevExt, pQueue, pCmd, enmComplType);

        while (!IsListEmpty(&pQueue->CmdQueue))
        {
            pCmd = VBOXVDMADDI_CMD_FROM_ENTRY(pQueue->CmdQueue.Flink);
            if (pCmd->enmState == VBOXVDMADDI_STATE_COMPLETED)
            {
                vboxVdmaDdiCmdDequeueIrq(pQueue, pCmd);
                vboxVdmaDdiCmdNotifyCompletedIrq(pDevExt, pQueue, pCmd, pCmd->enmComplType);
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

VOID vboxVdmaDdiCmdSubmittedIrq(PVBOXVDMADDI_CMD_QUEUE pQueue, PVBOXVDMADDI_CMD pCmd)
{
    BOOLEAN bQueued = pCmd->enmState >= VBOXVDMADDI_STATE_PENDING;
    Assert(pCmd->enmState < VBOXVDMADDI_STATE_SUBMITTED);
    pCmd->enmState = VBOXVDMADDI_STATE_SUBMITTED;
    if (!bQueued)
        vboxVdmaDdiCmdEnqueueIrq(pQueue, pCmd);
}

typedef struct VBOXVDMADDI_CMD_COMPLETED_CB
{
    PDEVICE_EXTENSION pDevExt;
    PVBOXVDMADDI_CMD_QUEUE pQueue;
    PVBOXVDMADDI_CMD pCmd;
    DXGK_INTERRUPT_TYPE enmComplType;
} VBOXVDMADDI_CMD_COMPLETED_CB, *PVBOXVDMADDI_CMD_COMPLETED_CB;

static BOOLEAN vboxVdmaDdiCmdCompletedCb(PVOID Context)
{
    PVBOXVDMADDI_CMD_COMPLETED_CB pdc = (PVBOXVDMADDI_CMD_COMPLETED_CB)Context;
    BOOLEAN bNeedDps = vboxVdmaDdiCmdCompletedIrq(pdc->pDevExt, pdc->pQueue, pdc->pCmd, pdc->enmComplType);
    pdc->pDevExt->bNotifyDxDpc |= bNeedDps;

    return bNeedDps;
}

NTSTATUS vboxVdmaDdiCmdCompleted(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD_QUEUE pQueue, PVBOXVDMADDI_CMD pCmd, DXGK_INTERRUPT_TYPE enmComplType)
{
    VBOXVDMADDI_CMD_COMPLETED_CB context;
    context.pDevExt = pDevExt;
    context.pQueue = pQueue;
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
    if (Status == STATUS_SUCCESS && bNeedDps)
    {
        BOOLEAN bRc = pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
        Assert(bRc);
    }
    return Status;
}

typedef struct VBOXVDMADDI_CMD_SUBMITTED_CB
{
//    PDEVICE_EXTENSION pDevExt;
    PVBOXVDMADDI_CMD_QUEUE pQueue;
    PVBOXVDMADDI_CMD pCmd;
} VBOXVDMADDI_CMD_SUBMITTED_CB, *PVBOXVDMADDI_CMD_SUBMITTED_CB;

static BOOLEAN vboxVdmaDdiCmdSubmittedCb(PVOID Context)
{
    PVBOXVDMADDI_CMD_SUBMITTED_CB pdc = (PVBOXVDMADDI_CMD_SUBMITTED_CB)Context;
    vboxVdmaDdiCmdSubmittedIrq(pdc->pQueue, pdc->pCmd);

    return FALSE;
}

NTSTATUS vboxVdmaDdiCmdSubmitted(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD_QUEUE pQueue, PVBOXVDMADDI_CMD pCmd)
{
    VBOXVDMADDI_CMD_SUBMITTED_CB context;
    context.pQueue = pQueue;
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
    PDEVICE_EXTENSION pDevExt;
    PVBOXWDDM_CONTEXT pContext;
    uint32_t u32FenceId;
} VBOXVDMADDI_CMD_COMPLETE_CB, *PVBOXVDMADDI_CMD_COMPLETE_CB;

static BOOLEAN vboxVdmaDdiCmdFenceCompleteCb(PVOID Context)
{
    PVBOXVDMADDI_CMD_COMPLETE_CB pdc = (PVBOXVDMADDI_CMD_COMPLETE_CB)Context;
    PDEVICE_EXTENSION pDevExt = pdc->pDevExt;
    DXGKARGCB_NOTIFY_INTERRUPT_DATA notify;
    memset(&notify, 0, sizeof(DXGKARGCB_NOTIFY_INTERRUPT_DATA));

    notify.InterruptType = DXGK_INTERRUPT_DMA_COMPLETED;
    notify.DmaCompleted.SubmissionFenceId = pdc->u32FenceId;
    notify.DmaCompleted.NodeOrdinal = pdc->pContext->NodeOrdinal;
    notify.DmaCompleted.EngineOrdinal = 0;

    pDevExt->u.primary.DxgkInterface.DxgkCbNotifyInterrupt(pDevExt->u.primary.DxgkInterface.DeviceHandle, &notify);

    pDevExt->bNotifyDxDpc = TRUE;
    BOOLEAN bDpcQueued = pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);

    return bDpcQueued;
}

static NTSTATUS vboxVdmaDdiCmdFenceNotifyComplete(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext, uint32_t u32FenceId)
{
    VBOXVDMADDI_CMD_COMPLETE_CB context;
    context.pDevExt = pDevExt;
    context.pContext = pContext;
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

NTSTATUS vboxVdmaDdiCmdFenceComplete(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext, uint32_t u32FenceId, DXGK_INTERRUPT_TYPE enmComplType)
{
    if (vboxVdmaDdiCmdCanComplete(&pDevExt->DdiCmdQueue))
        return vboxVdmaDdiCmdFenceNotifyComplete(pDevExt, pContext, u32FenceId);

    PVBOXVDMADDI_CMD pCmd = (PVBOXVDMADDI_CMD)vboxWddmMemAlloc(sizeof (VBOXVDMADDI_CMD));
    Assert(pCmd);
    if (pCmd)
    {
        vboxVdmaDdiCmdInit(pCmd, u32FenceId, pContext, vboxVdmaDdiCmdCompletionCbFree, NULL);
        NTSTATUS Status = vboxVdmaDdiCmdCompleted(pDevExt, &pDevExt->DdiCmdQueue, pCmd, enmComplType);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
            return STATUS_SUCCESS;
        vboxWddmMemFree(pCmd);
        return Status;
    }
    return STATUS_NO_MEMORY;
}

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
NTSTATUS vboxVdmaHlpUpdatePrimary(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, RECT* pRect)
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
