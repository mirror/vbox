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

#include "../VBoxVideo.h"
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
    pResult->cRects = 0;
    for (uint32_t i = 0; i < pRects->cRects; ++i)
    {
        if (vboxWddmRectIntersection(pArea, &pRects->aRects[i], &pResult->aRects[pResult->cRects]))
        {
            ++pResult->cRects;
        }
    }
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

/**
 * @param pDevExt
 */
static NTSTATUS vboxVdmaGgDirtyRectsProcess(VBOXVDMAPIPE_CMD_RECTSINFO *pRectsInfo)
{
    PVBOXWDDM_CONTEXT pContext = pRectsInfo->pContext;
    PDEVICE_EXTENSION pDevExt = pContext->pDevice->pAdapter;
    PVBOXWDDM_RECTS_INFO pRects = &pRectsInfo->ContextsRects.UpdateRects;
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDEOCM_CMD_RECTS_INTERNAL pCmdInternal = NULL;
    uint32_t cbCmdInternal = VBOXVIDEOCM_CMD_RECTS_INTERNAL_SIZE4CRECTS(pRects->cRects);
    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
    ExAcquireFastMutex(&pDevExt->ContextMutex);
    for (PLIST_ENTRY pCur = pDevExt->ContextList3D.Flink; pCur != &pDevExt->ContextList3D; pCur = pCur->Flink)
    {
        if (pCur != &pContext->ListEntry)
        {
            PVBOXWDDM_CONTEXT pCurContext = VBOXWDDMENTRY_2_CONTEXT(pCur);
            if (!pCmdInternal)
            {
                pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdCreate(&pCurContext->CmContext, cbCmdInternal);
                Assert(pCmdInternal);
                if (!pCmdInternal)
                {
                    Status = STATUS_NO_MEMORY;
                    break;
                }
            }
            else
            {
                pCmdInternal = (PVBOXVIDEOCM_CMD_RECTS_INTERNAL)vboxVideoCmCmdReinitForContext(pCmdInternal, &pCurContext->CmContext);
            }

            vboxVdmaDirtyRectsCalcIntersection(&pCurContext->ViewRect, pRects, &pCmdInternal->Cmd.RectsInfo);
            if (pCmdInternal->Cmd.RectsInfo.cRects)
            {
                bool bSend = false;
                pCmdInternal->Cmd.fFlags.Value = 0;
                pCmdInternal->Cmd.fFlags.bAddHiddenRects = 1;
                if (pCurContext->pLastReportedRects)
                {
                    if (pCurContext->pLastReportedRects->Cmd.fFlags.bSetVisibleRects)
                    {
                        RECT *paPrevRects;
                        uint32_t cPrevRects;
                        if (pCurContext->pLastReportedRects->Cmd.fFlags.bSetViewRect)
                        {
                            paPrevRects = &pCurContext->pLastReportedRects->Cmd.RectsInfo.aRects[1];
                            cPrevRects = pCurContext->pLastReportedRects->Cmd.RectsInfo.cRects - 1;
                        }
                        else
                        {
                            paPrevRects = &pCurContext->pLastReportedRects->Cmd.RectsInfo.aRects[0];
                            cPrevRects = pCurContext->pLastReportedRects->Cmd.RectsInfo.cRects;
                        }

                        if (vboxVdmaDirtyRectsHasIntersections(paPrevRects, cPrevRects,
                                pCmdInternal->Cmd.RectsInfo.aRects, pCmdInternal->Cmd.RectsInfo.cRects))
                        {
                            bSend = true;
                        }
                    }
                    else
                    {
                        Assert(pCurContext->pLastReportedRects->Cmd.fFlags.bAddHiddenRects);
                        if (!vboxVdmaDirtyRectsIsCover(pCurContext->pLastReportedRects->Cmd.RectsInfo.aRects,
                                pCurContext->pLastReportedRects->Cmd.RectsInfo.cRects,
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
                    if (pCurContext->pLastReportedRects)
                        vboxVideoCmCmdRelease(pCurContext->pLastReportedRects);

                    pCmdInternal->VidPnSourceId = pRectsInfo->VidPnSourceId;

                    vboxVideoCmCmdRetain(pCmdInternal);
                    pCurContext->pLastReportedRects = pCmdInternal;
                    vboxVideoCmCmdSubmit(pCmdInternal, VBOXVIDEOCM_CMD_RECTS_INTERNAL_SIZE4CRECTS(pCmdInternal->Cmd.RectsInfo.cRects));
                    pCmdInternal = NULL;
                }
            }
        }
        else
        {
            RECT * pContextRect = &pRectsInfo->ContextsRects.ContextRect;
            bool bRectShanged = (pContext->ViewRect.left != pContextRect->left
                    || pContext->ViewRect.top != pContextRect->top
                    || pContext->ViewRect.right != pContextRect->right
                    || pContext->ViewRect.bottom != pContextRect->bottom);
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
                pContext->ViewRect = *pContextRect;
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

                if (pContext->pLastReportedRects)
                {
                    if (pContext->pLastReportedRects->Cmd.fFlags.bSetVisibleRects)
                    {
                        RECT *paRects;
                        uint32_t cRects;
                        if (pContext->pLastReportedRects->Cmd.fFlags.bSetViewRect)
                        {
                            paRects = &pContext->pLastReportedRects->Cmd.RectsInfo.aRects[1];
                            cRects = pContext->pLastReportedRects->Cmd.RectsInfo.cRects - 1;
                        }
                        else
                        {
                            paRects = &pContext->pLastReportedRects->Cmd.RectsInfo.aRects[0];
                            cRects = pContext->pLastReportedRects->Cmd.RectsInfo.cRects;
                        }
                        bSend = (pDrCmdInternal->Cmd.RectsInfo.cRects != cRects)
                                || memcmp(paRects, pDrCmdInternal->Cmd.RectsInfo.aRects, cRects * sizeof (RECT));
                    }
                    else
                    {
                        Assert(pContext->pLastReportedRects->Cmd.fFlags.bAddHiddenRects);
                        bSend = true;
                    }
                }
                else
                    bSend = true;

            }

            Assert(pRects->cRects);
            if (bSend)
            {
                if (pContext->pLastReportedRects)
                    vboxVideoCmCmdRelease(pContext->pLastReportedRects);

                pDrCmdInternal->Cmd.fFlags.bSetVisibleRects = 1;
                pDrCmdInternal->VidPnSourceId = pRectsInfo->VidPnSourceId;

                vboxVideoCmCmdRetain(pDrCmdInternal);
                pContext->pLastReportedRects = pDrCmdInternal;
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
    PVBOXWDDM_CONTEXT pContext = pCF->pContext;
    PDEVICE_EXTENSION pDevExt = pContext->pDevice->pAdapter;
    Assert (pDevExt->pvVisibleVram);
    if (pDevExt->pvVisibleVram)
    {
        PVBOXWDDM_ALLOCATION pAlloc = pCF->pAllocation;
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
                    for (UINT i = 0; i < pCF->Rects.cRects; ++i)
                    {
                        RECT *pRect = &pCF->Rects.aRects[i];
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
                                *pvU32Mem = pCF->Color;
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
                if (pCF->VidPnSourceId != D3DDDI_ID_UNINITIALIZED
                        && pAlloc->bAssigned
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
                        && pAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE
#else
                        && pAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE
#endif
                        )
                {
                    if (!vboxWddmRectIsEmpty(&UnionRect))
                    {
                        PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[pCF->VidPnSourceId];
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


    uint32_t cNew = ASMAtomicDecU32(&pDevExt->cDMACmdsOutstanding);
    Assert(cNew < UINT32_MAX/2);

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
                for (PLIST_ENTRY pCur = CmdList.Blink; pCur != &CmdList; pCur = CmdList.Blink)
                {
                    PVBOXVDMAPIPE_CMD_DR pDr = VBOXVDMAPIPE_CMD_DR_FROM_ENTRY(pCur);
                    switch (pDr->enmType)
                    {
                        case VBOXVDMAPIPE_CMD_TYPE_RECTSINFO:
                        {
                            PVBOXVDMAPIPE_CMD_RECTSINFO pRects = (PVBOXVDMAPIPE_CMD_RECTSINFO)pDr;
                            Status = vboxVdmaGgDirtyRectsProcess(pRects);
                            Assert(Status == STATUS_SUCCESS);
                            break;
                        }
                        case VBOXVDMAPIPE_CMD_TYPE_DMACMD_CLRFILL:
                        {
                            PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL pCF = (PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL)pDr;
                            Status = vboxVdmaGgDmaColorFill(pCF);
                            Assert(Status == STATUS_SUCCESS);
                            break;
                        }
                        default:
                            AssertBreakpoint();
                    }
                    RemoveEntryList(pCur);
                    vboxVdmaGgCmdDestroy(pDr);
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

#ifdef VBOXVDMA
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
    VBoxHGSMIGuestWrite(pDevExt, offDr);
    return VINF_SUCCESS;
}
#define vboxWddmVdmaSubmit vboxWddmVdmaSubmitHgsmi
#endif

static int vboxVdmaInformHost(PDEVICE_EXTENSION pDevExt, PVBOXVDMAINFO pInfo, VBOXVDMA_CTL_TYPE enmCtl)
{
    int rc = VINF_SUCCESS;

    PVBOXVDMA_CTL pCmd = (PVBOXVDMA_CTL)VBoxSHGSMICommandAlloc(&pDevExt->u.primary.hgsmiAdapterHeap, sizeof (VBOXVDMA_CTL), HGSMI_CH_VBVA, VBVA_VDMA_CTL);
    if (pCmd)
    {
        pCmd->enmCtl = enmCtl;
        pCmd->u32Offset = pInfo->CmdHeap.area.offBase;
        pCmd->i32Result = VERR_NOT_SUPPORTED;

        const VBOXSHGSMIHEADER* pHdr = VBoxSHGSMICommandPrepSynch(&pDevExt->u.primary.hgsmiAdapterHeap, pCmd);
        Assert(pHdr);
        if (pHdr)
        {
            do
            {
                HGSMIOFFSET offCmd = VBoxSHGSMICommandOffset(&pDevExt->u.primary.hgsmiAdapterHeap, pHdr);
                Assert(offCmd != HGSMIOFFSET_VOID);
                if (offCmd != HGSMIOFFSET_VOID)
                {
                    rc = vboxWddmVdmaSubmit(pDevExt, pInfo, offCmd);
                    AssertRC(rc);
                    if (RT_SUCCESS(rc))
                    {
                        rc = VBoxSHGSMICommandDoneSynch(&pDevExt->u.primary.hgsmiAdapterHeap, pHdr);
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
                VBoxSHGSMICommandCancelSynch(&pDevExt->u.primary.hgsmiAdapterHeap, pHdr);
            } while (0);
        }

        VBoxSHGSMICommandFree (&pDevExt->u.primary.hgsmiAdapterHeap, pCmd);
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
#ifdef VBOXVDMA
        , ULONG offBuffer, ULONG cbBuffer
#endif
        )
{
    int rc;
    pInfo->fEnabled           = FALSE;

#ifdef VBOXVDMA
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

    rc = VBoxMapAdapterMemory (pDevExt,
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
#ifdef VBOXVDMA
        else
            drprintf((__FUNCTION__": HGSMIHeapSetup failed rc = 0x%x\n", rc));

        VBoxUnmapAdapterMemory(pDevExt, &pvBuffer, cbBuffer);
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
#ifdef VBOXVDMA
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
#ifdef VBOXVDMA
    int rc = vboxVdmaInformHost (pDevExt, pInfo, VBOXVDMA_CTL_TYPE_ENABLE);
    Assert(RT_SUCCESS(rc));
    if (RT_SUCCESS(rc))
        pInfo->fEnabled        = TRUE;

    return rc;
#else
    return VINF_SUCCESS;
#endif
}

#ifdef VBOXVDMA
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
#ifdef VBOXVDMA
        VBoxUnmapAdapterMemory (pDevExt, (void**)&pInfo->CmdHeap.area.pu8Base, pInfo->CmdHeap.area.cbArea);
#endif
    }
    else
        rc = VERR_GENERAL_FAILURE;
    return rc;
}

#ifdef VBOXVDMA
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
    DXGKARGCB_NOTIFY_INTERRUPT_DATA notify;
    PVBOXVDMACBUF_DR pDr = (PVBOXVDMACBUF_DR)pvCmd;

    memset(&notify, 0, sizeof(DXGKARGCB_NOTIFY_INTERRUPT_DATA));

    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)pDr->u64GuestContext;

    if (RT_SUCCESS(pDr->rc))
    {
        notify.InterruptType = DXGK_INTERRUPT_DMA_COMPLETED;
        notify.DmaCompleted.SubmissionFenceId = pDr->u32FenceId;
        if (pContext)
        {
            notify.DmaCompleted.NodeOrdinal = pContext->NodeOrdinal;
            notify.DmaCompleted.EngineOrdinal = 0;
            pContext->uLastCompletedCmdFenceId = pDr->u32FenceId;
        }
        else
            pVdma->uLastCompletedPagingBufferCmdFenceId = pDr->u32FenceId;
        pDevExt->bSetNotifyDxDpc = TRUE;
    }
    else if (pDr->rc == VERR_INTERRUPTED)
    {
        notify.InterruptType = DXGK_INTERRUPT_DMA_PREEMPTED;
        notify.DmaPreempted.PreemptionFenceId = pDr->u32FenceId;
        if (pContext)
        {
            notify.DmaPreempted.LastCompletedFenceId = pContext->uLastCompletedCmdFenceId;
            notify.DmaPreempted.NodeOrdinal = pContext->NodeOrdinal;
            notify.DmaPreempted.EngineOrdinal = 0;
        }
        else
            notify.DmaPreempted.LastCompletedFenceId = pVdma->uLastCompletedPagingBufferCmdFenceId;

        pDevExt->bSetNotifyDxDpc = TRUE;
    }
    else
    {
        AssertBreakpoint();
        notify.InterruptType = DXGK_INTERRUPT_DMA_FAULTED;
        notify.DmaFaulted.FaultedFenceId = pDr->u32FenceId;
        notify.DmaFaulted.Status = STATUS_UNSUCCESSFUL; /* @todo: better status ? */
        if (pContext)
        {
            notify.DmaFaulted.NodeOrdinal = pContext->NodeOrdinal;
            notify.DmaFaulted.EngineOrdinal = 0;
        }
        pDevExt->bSetNotifyDxDpc = TRUE;
    }

    pDevExt->u.primary.DxgkInterface.DxgkCbNotifyInterrupt(pDevExt->u.primary.DxgkInterface.DeviceHandle, &notify);

    /* inform SHGSMI we want to be called at DPC later */
    *ppfnCompletion = vboxVdmaCBufDrCompletion;
    *ppvCompletion = pvContext;
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
#endif
