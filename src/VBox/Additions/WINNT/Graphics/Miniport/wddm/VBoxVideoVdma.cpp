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
        KeAcquireSpinLock(&pPipe->SinchLock, &OldIrql);
        Assert(pPipe->enmState == VBOXVDMAPIPE_STATE_OPENNED
                || pPipe->enmState == VBOXVDMAPIPE_STATE_CLOSING);
        Assert(pPipe->enmState >= VBOXVDMAPIPE_STATE_OPENNED);
        enmState = pPipe->enmState;
        if (enmState >= VBOXVDMAPIPE_STATE_OPENNED)
        {
            vboxVideoLeDetach(&pPipe->CmdListHead, pDetachHead);
            pPipe->bNeedNotify = false;
        }
        else
        {
            KeReleaseSpinLock(&pPipe->SinchLock, OldIrql);
            Status = STATUS_INVALID_PIPE_STATE;
            break;
        }

        KeReleaseSpinLock(&pPipe->SinchLock, OldIrql);

        if (!IsListEmpty(pDetachHead))
        {
            Assert(Status == STATUS_SUCCESS);
            break;
        }

        if (enmState == VBOXVDMAPIPE_STATE_OPENNED)
        {
            KeAcquireSpinLock(&pPipe->SinchLock, &OldIrql);
            pPipe->bNeedNotify = true;
            KeReleaseSpinLock(&pPipe->SinchLock, OldIrql);

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
/**
 * @param pDevExt
 */
NTSTATUS vboxVdmaGgDirtyRectsProcess(VBOXVDMAPIPE_CMD_RECTSINFO *pRectsInfo)
{
    PVBOXWDDM_CONTEXT pContext = pRectsInfo->pContext;
    PDEVICE_EXTENSION pDevExt = pContext->pDevice->pAdapter;
    RECT * pContextRect = &pRectsInfo->ContextsRects.ContextRect;
    PVBOXWDDM_RECTS_INFO pRects = &pRectsInfo->ContextsRects.UpdateRects;
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDEOCM_CMD_RECTS pCmd = NULL;
    uint32_t cbCmd = VBOXVIDEOCM_CMD_RECTS_SIZE4CRECTS(pRects->cRects);
    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
    ExAcquireFastMutex(&pDevExt->ContextMutex);
    for (PLIST_ENTRY pCur = pDevExt->ContextList3D.Flink; pCur != &pDevExt->ContextList3D; pCur = pCur->Flink)
    {
        if (pCur != &pContext->ListEntry)
        {
            PVBOXWDDM_CONTEXT pCurContext = VBOXWDDMENTRY_2_CONTEXT(pCur);
            if (!pCmd)
            {
                pCmd = (PVBOXVIDEOCM_CMD_RECTS)vboxVideoCmCmdCreate(&pCurContext->CmContext, cbCmd);
                Assert(pCmd);
                if (!pCmd)
                {
                    Status = STATUS_NO_MEMORY;
                    break;
                }
            }
            else
            {
                pCmd = (PVBOXVIDEOCM_CMD_RECTS)vboxVideoCmCmdReinitForContext(pCmd, &pCurContext->CmContext);
            }

            vboxVdmaDirtyRectsCalcIntersection(&pContext->ViewRect, pRects, &pCmd->RectsInfo);
            if (pCmd->RectsInfo.cRects)
            {
                Assert(pCmd->fFlags.Value == 0);
                pCmd->fFlags.bAddHiddenRects = 1;
                vboxVideoCmCmdSubmit(pCmd, VBOXVIDEOCM_CMD_RECTS_SIZE4CRECTS(pCmd->RectsInfo.cRects));
                pCmd = NULL;
            }
        }
        else
        {
            bool bRectShanged = (pContext->ViewRect.left != pContextRect->left
                    || pContext->ViewRect.top != pContextRect->top
                    || pContext->ViewRect.right != pContextRect->right
                    || pContext->ViewRect.bottom != pContextRect->bottom);
            PVBOXVIDEOCM_CMD_RECTS pDrCmd;

            if (bRectShanged)
            {
                uint32_t cbDrCmd = VBOXVIDEOCM_CMD_RECTS_SIZE4CRECTS(pRects->cRects + 1);
                pDrCmd = (PVBOXVIDEOCM_CMD_RECTS)vboxVideoCmCmdCreate(&pContext->CmContext, cbDrCmd);
                Assert(pDrCmd);
                if (!pDrCmd)
                {
                    Status = STATUS_NO_MEMORY;
                    break;
                }
                pDrCmd->RectsInfo.cRects = pRects->cRects + 1;
            }
            else
            {
                if (pCmd)
                {
                    pDrCmd = (PVBOXVIDEOCM_CMD_RECTS)vboxVideoCmCmdReinitForContext(pCmd, &pContext->CmContext);
                    pCmd = NULL;
                }
                else
                {
                    pDrCmd = (PVBOXVIDEOCM_CMD_RECTS)vboxVideoCmCmdCreate(&pContext->CmContext, cbCmd);
                    Assert(pDrCmd);
                    if (!pDrCmd)
                    {
                        Status = STATUS_NO_MEMORY;
                        break;
                    }
                }
                pDrCmd->RectsInfo.cRects = pRects->cRects;
            }

            Assert(pDrCmd->fFlags.Value == 0);
            RECT *pDirtyRect;
            if (bRectShanged)
            {
                pDrCmd->fFlags.bPositionRect = 1;
                pDrCmd->RectsInfo.aRects[0] = *pContextRect;
                pDirtyRect = &pDrCmd->RectsInfo.aRects[1];
            }
            else
                pDirtyRect = &pDrCmd->RectsInfo.aRects[0];

            pDrCmd->fFlags.bAddVisibleRects = 1;
            memcpy (pDirtyRect, pRects->aRects, sizeof (RECT) * pRects->cRects);

            vboxVideoCmCmdSubmit(pDrCmd, VBOXVIDEOCM_SUBMITSIZE_DEFAULT);
        }
    }
    ExReleaseFastMutex(&pDevExt->ContextMutex);


    if (pCmd)
        vboxVideoCmCmdRelease(pCmd);

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
                        case VBOXVDMAPIPE_CMD_TYPE_DMACMD:
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

static int vboxVdmaInformHost (PDEVICE_EXTENSION pDevExt, PVBOXVDMAINFO pInfo, VBOXVDMA_CTL_TYPE enmCtl)
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

/* create a DMACommand buffer */
int vboxVdmaCreate (PDEVICE_EXTENSION pDevExt, VBOXVDMAINFO *pInfo, ULONG offBuffer, ULONG cbBuffer)
{
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

    pInfo->fEnabled           = FALSE;
    PVOID pvBuffer;

    int rc = VBoxMapAdapterMemory (pDevExt,
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
        {
            NTSTATUS Status = vboxVdmaGgConstruct(&pInfo->DmaGg);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
                return VINF_SUCCESS;
            rc = VERR_GENERAL_FAILURE;
        }
        else
            drprintf((__FUNCTION__": HGSMIHeapSetup failed rc = 0x%x\n", rc));

        VBoxUnmapAdapterMemory(pDevExt, &pvBuffer, cbBuffer);
    }
    else
        drprintf((__FUNCTION__": VBoxMapAdapterMemory failed rc = 0x%x\n", rc));

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

    int rc = vboxVdmaInformHost (pDevExt, pInfo, VBOXVDMA_CTL_TYPE_DISABLE);
    AssertRC(rc);
    return rc;
}

int vboxVdmaEnable (PDEVICE_EXTENSION pDevExt, PVBOXVDMAINFO pInfo)
{
    dfprintf((__FUNCTION__"\n"));

    Assert(!pInfo->fEnabled);
    if (pInfo->fEnabled)
        return VINF_ALREADY_INITIALIZED;

    int rc = vboxVdmaInformHost (pDevExt, pInfo, VBOXVDMA_CTL_TYPE_ENABLE);
    Assert(RT_SUCCESS(rc));
    if (RT_SUCCESS(rc))
        pInfo->fEnabled        = TRUE;

    return rc;
}

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
        VBoxUnmapAdapterMemory (pDevExt, (void**)&pInfo->CmdHeap.area.pu8Base, pInfo->CmdHeap.area.cbArea);
    }
    else
        rc = VERR_GENERAL_FAILURE;
    return rc;
}

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
