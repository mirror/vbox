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
            return rc;
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
    Assert(!pInfo->fEnabled);
    if (pInfo->fEnabled)
        rc = vboxVdmaDisable (pDevExt, pInfo);
    VBoxUnmapAdapterMemory (pDevExt, (void**)&pInfo->CmdHeap.area.pu8Base, pInfo->CmdHeap.area.cbArea);
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
