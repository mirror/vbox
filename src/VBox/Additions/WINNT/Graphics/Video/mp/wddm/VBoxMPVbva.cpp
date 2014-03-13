/* $Id$ */

/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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

/*
 * Public hardware buffer methods.
 */
int vboxVbvaEnable (PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva)
{
    if (VBoxVBVAEnable(&pVbva->Vbva, &VBoxCommonFromDeviceExt(pDevExt)->guestCtx,
            pVbva->Vbva.pVBVA, pVbva->srcId))
        return VINF_SUCCESS;

    WARN(("VBoxVBVAEnable failed!"));
    return VERR_GENERAL_FAILURE;
}

int vboxVbvaDisable (PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva)
{
    VBoxVBVADisable(&pVbva->Vbva, &VBoxCommonFromDeviceExt(pDevExt)->guestCtx, pVbva->srcId);
    return VINF_SUCCESS;
}

int vboxVbvaCreate(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva, ULONG offBuffer, ULONG cbBuffer, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    memset(pVbva, 0, sizeof(VBOXVBVAINFO));

    KeInitializeSpinLock(&pVbva->Lock);

    int rc = VBoxMPCmnMapAdapterMemory(VBoxCommonFromDeviceExt(pDevExt),
                                       (void**)&pVbva->Vbva.pVBVA,
                                       offBuffer,
                                       cbBuffer);
    if (RT_SUCCESS(rc))
    {
        Assert(pVbva->Vbva.pVBVA);
        VBoxVBVASetupBufferContext(&pVbva->Vbva, offBuffer, cbBuffer);
        pVbva->srcId = srcId;
    }
    else
    {
        WARN(("VBoxMPCmnMapAdapterMemory failed rc %d", rc));
    }


    return rc;
}

int vboxVbvaDestroy(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva)
{
    int rc = VINF_SUCCESS;
    VBoxMPCmnUnmapAdapterMemory(VBoxCommonFromDeviceExt(pDevExt), (void**)&pVbva->Vbva.pVBVA);
    memset(pVbva, 0, sizeof (VBOXVBVAINFO));
    return rc;
}

int vboxVbvaReportDirtyRect (PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SOURCE pSrc, RECT *pRectOrig)
{
    VBVACMDHDR hdr;

    RECT rect = *pRectOrig;

//        if (rect.left < 0) rect.left = 0;
//        if (rect.top < 0) rect.top = 0;
//        if (rect.right > (int)ppdev->cxScreen) rect.right = ppdev->cxScreen;
//        if (rect.bottom > (int)ppdev->cyScreen) rect.bottom = ppdev->cyScreen;

    hdr.x = (int16_t)rect.left;
    hdr.y = (int16_t)rect.top;
    hdr.w = (uint16_t)(rect.right - rect.left);
    hdr.h = (uint16_t)(rect.bottom - rect.top);

    hdr.x += (int16_t)pSrc->VScreenPos.x;
    hdr.y += (int16_t)pSrc->VScreenPos.y;

    if (VBoxVBVAWrite(&pSrc->Vbva.Vbva, &VBoxCommonFromDeviceExt(pDevExt)->guestCtx, &hdr, sizeof(hdr)))
        return VINF_SUCCESS;

    WARN(("VBoxVBVAWrite failed"));
    return VERR_GENERAL_FAILURE;
}

/* command vbva ring buffer */

/* customized VBVA implementation */

/* Forward declarations of internal functions. */
static void vboxHwBufferPlaceDataAt(PVBVAEXBUFFERCONTEXT pCtx, const void *p,
                                    uint32_t cb, uint32_t offset);
static bool vboxHwBufferWrite(PVBVAEXBUFFERCONTEXT pCtx,
                              PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx,
                              const void *p, uint32_t cb);

DECLINLINE(void) vboxVBVAExFlush(struct VBVAEXBUFFERCONTEXT *pCtx, PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx)
{
    pCtx->pfnFlush(pCtx, pHGSMICtx, pCtx->pvFlush);
}

static int vboxCmdVbvaSubmitHgsmi(PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx, HGSMIOFFSET offDr)
{
    VBoxVideoCmnPortWriteUlong(pHGSMICtx->port, offDr);
    return VINF_SUCCESS;
}
#define vboxCmdVbvaSubmit vboxCmdVbvaSubmitHgsmi

static VBOXCMDVBVA_CTL * vboxCmdVbvaCtlCreate(PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx, uint32_t cbCtl)
{
    Assert(cbCtl >= sizeof (VBOXCMDVBVA_CTL));
    return (VBOXCMDVBVA_CTL*)VBoxSHGSMICommandAlloc(&pHGSMICtx->heapCtx, cbCtl, HGSMI_CH_VBVA, VBVA_CMDVBVA_CTL);
}

static void vboxCmdVbvaCtlFree(PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx, VBOXCMDVBVA_CTL * pCtl)
{
    VBoxSHGSMICommandFree(&pHGSMICtx->heapCtx, pCtl);
}

static int vboxCmdVbvaCtlSubmitSync(PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx, VBOXCMDVBVA_CTL * pCtl)
{
    const VBOXSHGSMIHEADER* pHdr = VBoxSHGSMICommandPrepSynch(&pHGSMICtx->heapCtx, pCtl);
    if (!pHdr)
    {
        WARN(("VBoxSHGSMICommandPrepSynch returnd NULL"));
        return VERR_INVALID_PARAMETER;
    }

    HGSMIOFFSET offCmd = VBoxSHGSMICommandOffset(&pHGSMICtx->heapCtx, pHdr);
    if (offCmd == HGSMIOFFSET_VOID)
    {
        WARN(("VBoxSHGSMICommandOffset returnd NULL"));
        VBoxSHGSMICommandCancelSynch(&pHGSMICtx->heapCtx, pHdr);
        return VERR_INVALID_PARAMETER;
    }

    int rc = vboxCmdVbvaSubmit(pHGSMICtx, offCmd);
    if (RT_SUCCESS(rc))
    {
        rc = VBoxSHGSMICommandDoneSynch(&pHGSMICtx->heapCtx, pHdr);
        if (RT_SUCCESS(rc))
        {
            rc = pCtl->i32Result;
            if (!RT_SUCCESS(rc))
                WARN(("pCtl->i32Result %d", pCtl->i32Result));

            return rc;
        }
        else
            WARN(("VBoxSHGSMICommandDoneSynch returnd %d", rc));
    }
    else
        WARN(("vboxCmdVbvaSubmit returnd %d", rc));

    VBoxSHGSMICommandCancelSynch(&pHGSMICtx->heapCtx, pHdr);

    return rc;
}

static int vboxCmdVbvaCtlSubmitAsync(PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx, VBOXCMDVBVA_CTL * pCtl, PFNVBOXSHGSMICMDCOMPLETION_IRQ pfnCompletionIrq, void *pvCompletionIrq)
{
    const VBOXSHGSMIHEADER* pHdr = VBoxSHGSMICommandPrepAsynchIrq(&pHGSMICtx->heapCtx, pCtl, pfnCompletionIrq, pvCompletionIrq, VBOXSHGSMI_FLAG_GH_ASYNCH_FORCE);
    if (!pHdr)
    {
        WARN(("VBoxSHGSMICommandPrepAsynchIrq returnd NULL"));
        return VERR_INVALID_PARAMETER;
    }

    HGSMIOFFSET offCmd = VBoxSHGSMICommandOffset(&pHGSMICtx->heapCtx, pHdr);
    if (offCmd == HGSMIOFFSET_VOID)
    {
        WARN(("VBoxSHGSMICommandOffset returnd NULL"));
        VBoxSHGSMICommandCancelAsynch(&pHGSMICtx->heapCtx, pHdr);
        return VERR_INVALID_PARAMETER;
    }

    int rc = vboxCmdVbvaSubmit(pHGSMICtx, offCmd);
    if (RT_SUCCESS(rc))
    {
        VBoxSHGSMICommandDoneAsynch(&pHGSMICtx->heapCtx, pHdr);
        return rc;
    }
    else
        WARN(("vboxCmdVbvaSubmit returnd %d", rc));

    VBoxSHGSMICommandCancelAsynch(&pHGSMICtx->heapCtx, pHdr);

    return rc;
}

static int vboxVBVAExCtlSubmitEnableDisable(PVBVAEXBUFFERCONTEXT pCtx, PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx, bool fEnable)
{
    VBOXCMDVBVA_CTL_ENABLE *pCtl = (VBOXCMDVBVA_CTL_ENABLE*)vboxCmdVbvaCtlCreate(pHGSMICtx, sizeof (*pCtl));
    if (!pCtl)
    {
        WARN(("vboxCmdVbvaCtlCreate failed"));
        return VERR_NO_MEMORY;
    }

    pCtl->Hdr.u32Type = VBOXCMDVBVACTL_TYPE_ENABLE;
    pCtl->Hdr.i32Result = VERR_NOT_IMPLEMENTED;
    memset(&pCtl->Enable, 0, sizeof (pCtl->Enable));
    pCtl->Enable.u32Flags  = fEnable? VBVA_F_ENABLE: VBVA_F_DISABLE;
    pCtl->Enable.u32Offset = pCtx->offVRAMBuffer;
    pCtl->Enable.i32Result = VERR_NOT_SUPPORTED;
    pCtl->Enable.u32Flags |= VBVA_F_ABSOFFSET;

    int rc = vboxCmdVbvaCtlSubmitSync(pHGSMICtx, &pCtl->Hdr);
    if (RT_SUCCESS(rc))
    {
        rc = pCtl->Hdr.i32Result;
        if (!RT_SUCCESS(rc))
            WARN(("vboxCmdVbvaCtlSubmitSync Disable failed %d", rc));
    }
    else
        WARN(("vboxCmdVbvaCtlSubmitSync returnd %d", rc));

    vboxCmdVbvaCtlFree(pHGSMICtx, &pCtl->Hdr);

    return rc;
}

/*
 * Public hardware buffer methods.
 */
RTDECL(bool) VBoxVBVAExEnable(PVBVAEXBUFFERCONTEXT pCtx,
                            PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx,
                            VBVABUFFER *pVBVA)
{
    bool bRc = false;

    LogFlowFunc(("pVBVA %p\n", pVBVA));

#if 0  /* All callers check this */
    if (ppdev->bHGSMISupported)
#endif
    {
        LogFunc(("pVBVA %p vbva off 0x%x\n", pVBVA, pCtx->offVRAMBuffer));

        pVBVA->hostFlags.u32HostEvents      = 0;
        pVBVA->hostFlags.u32SupportedOrders = 0;
        pVBVA->off32Data          = 0;
        pVBVA->off32Free          = 0;
        memset(pVBVA->aRecords, 0, sizeof (pVBVA->aRecords));
        pVBVA->indexRecordFirst   = 0;
        pVBVA->indexRecordFree    = 0;
        pVBVA->cbPartialWriteThreshold = 256;
        pVBVA->cbData             = pCtx->cbBuffer - sizeof (VBVABUFFER) + sizeof (pVBVA->au8Data);

        pCtx->fHwBufferOverflow = false;
        pCtx->pRecord    = NULL;
        pCtx->pVBVA      = pVBVA;

        bRc = vboxVBVAExCtlSubmitEnableDisable(pCtx, pHGSMICtx, true);
    }

    if (!bRc)
    {
        VBoxVBVAExDisable(pCtx, pHGSMICtx);
    }

    return bRc;
}

RTDECL(void) VBoxVBVAExDisable(PVBVAEXBUFFERCONTEXT pCtx,
                             PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx)
{
    LogFlowFunc(("\n"));

    pCtx->fHwBufferOverflow = false;
    pCtx->pRecord           = NULL;
    pCtx->pVBVA             = NULL;

    vboxVBVAExCtlSubmitEnableDisable(pCtx, pHGSMICtx, false);

    return;
}

RTDECL(bool) VBoxVBVAExBufferBeginUpdate(PVBVAEXBUFFERCONTEXT pCtx,
                                       PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx)
{
    bool bRc = false;

    // LogFunc(("flags = 0x%08X\n", pCtx->pVBVA? pCtx->pVBVA->u32HostEvents: -1));

    if (   pCtx->pVBVA
        && (pCtx->pVBVA->hostFlags.u32HostEvents & VBVA_F_MODE_ENABLED))
    {
        uint32_t indexRecordNext;

        Assert(!pCtx->fHwBufferOverflow);
        Assert(pCtx->pRecord == NULL);

        indexRecordNext = (pCtx->pVBVA->indexRecordFree + 1) % VBVA_MAX_RECORDS;

        if (indexRecordNext == pCtx->indexRecordFirstUncompleted)
        {
            /* All slots in the records queue are used. */
            vboxVBVAExFlush (pCtx, pHGSMICtx);
        }

        if (indexRecordNext == pCtx->indexRecordFirstUncompleted)
        {
            /* Even after flush there is no place. Fail the request. */
            LogFunc(("no space in the queue of records!!! first %d, last %d\n",
                    indexRecordNext, pCtx->pVBVA->indexRecordFree));
        }
        else
        {
            /* Initialize the record. */
            VBVARECORD *pRecord = &pCtx->pVBVA->aRecords[pCtx->pVBVA->indexRecordFree];

            pRecord->cbRecord = VBVA_F_RECORD_PARTIAL;

            pCtx->pVBVA->indexRecordFree = indexRecordNext;

            // LogFunc(("indexRecordNext = %d\n", indexRecordNext));

            /* Remember which record we are using. */
            pCtx->pRecord = pRecord;

            bRc = true;
        }
    }

    return bRc;
}

RTDECL(void) VBoxVBVAExBufferEndUpdate(PVBVAEXBUFFERCONTEXT pCtx)
{
    VBVARECORD *pRecord;

    // LogFunc(("\n"));

    Assert(pCtx->pVBVA);

    pRecord = pCtx->pRecord;
    Assert(pRecord && (pRecord->cbRecord & VBVA_F_RECORD_PARTIAL));

    /* Mark the record completed. */
    pRecord->cbRecord &= ~VBVA_F_RECORD_PARTIAL;

    pCtx->fHwBufferOverflow = false;
    pCtx->pRecord = NULL;

    return;
}

DECLINLINE(bool) vboxVBVAExIsEntryInRange(uint32_t u32First, uint32_t u32Entry, uint32_t u32Free)
{
    return (     u32First != u32Free
             && (
                     (u32First < u32Free && u32Entry >= u32First && u32Entry < u32Free)
                  || (u32First > u32Free && (u32Entry >= u32First || u32Entry < u32Free))
                 )
           );
}

#ifdef DEBUG

DECLINLINE(void) vboxHwBufferVerifyCompleted(PVBVAEXBUFFERCONTEXT pCtx)
{
    VBVABUFFER *pVBVA = pCtx->pVBVA;
    if (!vboxVBVAExIsEntryInRange(pCtx->indexRecordFirstUncompleted, pVBVA->indexRecordFirst, pVBVA->indexRecordFree))
    {
        WARN(("invalid record set"));
    }

    if (!vboxVBVAExIsEntryInRange(pCtx->off32DataUncompleted, pVBVA->off32Data, pVBVA->off32Free))
    {
        WARN(("invalid data set"));
    }
}
#endif

/*
 * Private operations.
 */
static uint32_t vboxHwBufferAvail(PVBVAEXBUFFERCONTEXT pCtx, const VBVABUFFER *pVBVA)
{
    int32_t i32Diff = pCtx->off32DataUncompleted - pVBVA->off32Free;

    return i32Diff > 0? i32Diff: pVBVA->cbData + i32Diff;
}

static uint32_t vboxHwBufferContiguousAvail(PVBVAEXBUFFERCONTEXT pCtx, const VBVABUFFER *pVBVA)
{
    int32_t i32Diff = pCtx->off32DataUncompleted - pVBVA->off32Free;

    return i32Diff > 0 ? i32Diff: pVBVA->cbData - pVBVA->off32Free;
}

static void vboxHwBufferPlaceDataAt(PVBVAEXBUFFERCONTEXT pCtx, const void *p,
                                    uint32_t cb, uint32_t offset)
{
    VBVABUFFER *pVBVA = pCtx->pVBVA;
    uint32_t u32BytesTillBoundary = pVBVA->cbData - offset;
    uint8_t  *dst                 = &pVBVA->au8Data[offset];
    int32_t i32Diff               = cb - u32BytesTillBoundary;

    if (i32Diff <= 0)
    {
        /* Chunk will not cross buffer boundary. */
        memcpy (dst, p, cb);
    }
    else
    {
        /* Chunk crosses buffer boundary. */
        memcpy (dst, p, u32BytesTillBoundary);
        memcpy (&pVBVA->au8Data[0], (uint8_t *)p + u32BytesTillBoundary, i32Diff);
    }

    return;
}

static bool vboxHwBufferWrite(PVBVAEXBUFFERCONTEXT pCtx,
                              PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx,
                              const void *p, uint32_t cb)
{
    VBVARECORD *pRecord;
    uint32_t cbHwBufferAvail;

    uint32_t cbWritten = 0;

    VBVABUFFER *pVBVA = pCtx->pVBVA;
    Assert(pVBVA);

    if (!pVBVA || pCtx->fHwBufferOverflow)
    {
        return false;
    }

    Assert(pVBVA->indexRecordFirst != pVBVA->indexRecordFree);
    Assert(pCtx->indexRecordFirstUncompleted != pVBVA->indexRecordFree);

    pRecord = pCtx->pRecord;
    Assert(pRecord && (pRecord->cbRecord & VBVA_F_RECORD_PARTIAL));

    // LogFunc(("%d\n", cb));

    cbHwBufferAvail = vboxHwBufferAvail(pCtx, pVBVA);

    while (cb > 0)
    {
        uint32_t cbChunk = cb;

        // LogFunc(("pVBVA->off32Free %d, pRecord->cbRecord 0x%08X, cbHwBufferAvail %d, cb %d, cbWritten %d\n",
        //             pVBVA->off32Free, pRecord->cbRecord, cbHwBufferAvail, cb, cbWritten));

        if (cbChunk >= cbHwBufferAvail)
        {
            LogFunc(("1) avail %d, chunk %d\n", cbHwBufferAvail, cbChunk));

            vboxVBVAExFlush(pCtx, pHGSMICtx);

            cbHwBufferAvail = vboxHwBufferAvail(pCtx, pVBVA);

            if (cbChunk >= cbHwBufferAvail)
            {
                WARN(("no place for %d bytes. Only %d bytes available after flush. Going to partial writes.\n",
                            cb, cbHwBufferAvail));

                if (cbHwBufferAvail <= pVBVA->cbPartialWriteThreshold)
                {
                    WARN(("Buffer overflow!!!\n"));
                    pCtx->fHwBufferOverflow = true;
                    Assert(false);
                    return false;
                }

                cbChunk = cbHwBufferAvail - pVBVA->cbPartialWriteThreshold;
            }
        }

        Assert(cbChunk <= cb);
        Assert(cbChunk <= vboxHwBufferAvail(pCtx, pVBVA));

        vboxHwBufferPlaceDataAt (pCtx, (uint8_t *)p + cbWritten, cbChunk, pVBVA->off32Free);

        pVBVA->off32Free   = (pVBVA->off32Free + cbChunk) % pVBVA->cbData;
        pRecord->cbRecord += cbChunk;
        cbHwBufferAvail -= cbChunk;

        cb        -= cbChunk;
        cbWritten += cbChunk;
    }

    return true;
}

/*
 * Public writer to the hardware buffer.
 */
RTDECL(uint32_t) VBoxVBVAExGetFreeTail(PVBVAEXBUFFERCONTEXT pCtx)
{
    VBVABUFFER *pVBVA = pCtx->pVBVA;
    if (pVBVA->off32Data <= pVBVA->off32Free)
        return pVBVA->cbData - pVBVA->off32Free;
    return 0;
}

RTDECL(void*) VBoxVBVAExAllocContiguous(PVBVAEXBUFFERCONTEXT pCtx, PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx, uint32_t cb)
{
    VBVARECORD *pRecord;
    uint32_t cbHwBufferContiguousAvail;
    uint32_t offset;

    VBVABUFFER *pVBVA = pCtx->pVBVA;
    Assert(pVBVA);

    if (!pVBVA || pCtx->fHwBufferOverflow)
    {
        return NULL;
    }

    Assert(pVBVA->indexRecordFirst != pVBVA->indexRecordFree);
    Assert(pCtx->indexRecordFirstUncompleted != pVBVA->indexRecordFree);

    pRecord = pCtx->pRecord;
    Assert(pRecord && (pRecord->cbRecord & VBVA_F_RECORD_PARTIAL));

    // LogFunc(("%d\n", cb));

    if (pVBVA->cbData < cb)
    {
        WARN(("requested to allocate buffer of size %d bigger than the VBVA ring buffer size %d", cb, pVBVA->cbData));
        return NULL;
    }

    cbHwBufferContiguousAvail = vboxHwBufferContiguousAvail(pCtx, pVBVA);

    if (cbHwBufferContiguousAvail < cb)
    {
        if (cb < pVBVA->cbData - pVBVA->off32Free)
        {
            /* the entire contiguous part is smaller than the requested buffer */
            return NULL;
        }

        vboxVBVAExFlush(pCtx, pHGSMICtx);

        cbHwBufferContiguousAvail = vboxHwBufferContiguousAvail(pCtx, pVBVA);
        if (cbHwBufferContiguousAvail < cb)
        {
            /* this is really bad - the host did not clean up buffer even after we requested it to flush */
            WARN(("Host did not clean up the buffer!"));
            return NULL;
        }
    }

    offset = pVBVA->off32Free;

    pVBVA->off32Free = (pVBVA->off32Free + cb) % pVBVA->cbData;
    pRecord->cbRecord += cb;

    return &pVBVA->au8Data[offset];
}

RTDECL(bool) VBoxVBVAExIsProcessing(PVBVAEXBUFFERCONTEXT pCtx)
{
    uint32_t u32HostEvents = pCtx->pVBVA->hostFlags.u32HostEvents;
    return !!(u32HostEvents & VBVA_F_STATE_PROCESSING);
}

RTDECL(void) VBoxVBVAExCBufferCompleted(PVBVAEXBUFFERCONTEXT pCtx)
{
    VBVABUFFER *pVBVA = pCtx->pVBVA;
    uint32_t cbBuffer = pVBVA->aRecords[pCtx->indexRecordFirstUncompleted].cbRecord;
    pCtx->indexRecordFirstUncompleted = (pCtx->indexRecordFirstUncompleted + 1) % VBVA_MAX_RECORDS;
    pCtx->off32DataUncompleted = (pCtx->off32DataUncompleted + cbBuffer) % pVBVA->cbData;
#ifdef DEBUG
    vboxHwBufferVerifyCompleted(pCtx);
#endif
}

RTDECL(bool) VBoxVBVAExWrite(PVBVAEXBUFFERCONTEXT pCtx,
                           PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx,
                           const void *pv, uint32_t cb)
{
    return vboxHwBufferWrite(pCtx, pHGSMICtx, pv, cb);
}

RTDECL(bool) VBoxVBVAExOrderSupported(PVBVAEXBUFFERCONTEXT pCtx, unsigned code)
{
    VBVABUFFER *pVBVA = pCtx->pVBVA;

    if (!pVBVA)
    {
        return false;
    }

    if (pVBVA->hostFlags.u32SupportedOrders & (1 << code))
    {
        return true;
    }

    return false;
}

RTDECL(void) VBoxVBVAExSetupBufferContext(PVBVAEXBUFFERCONTEXT pCtx,
                                        uint32_t offVRAMBuffer,
                                        uint32_t cbBuffer,
                                        PFNVBVAEXBUFFERFLUSH pfnFlush,
                                        void *pvFlush)
{
    memset(pCtx, 0, RT_OFFSETOF(VBVAEXBUFFERCONTEXT, pVBVA));
    pCtx->offVRAMBuffer = offVRAMBuffer;
    pCtx->cbBuffer      = cbBuffer;
    pCtx->pfnFlush = pfnFlush;
    pCtx->pvFlush = pvFlush;
}

static void* vboxVBVAExIterCur(PVBVAEXBUFFERITERBASE pIter, struct VBVABUFFER *pVBVA, uint32_t *pcbBuffer, bool *pfProcessed)
{
    uint32_t cbRecord = pVBVA->aRecords[pIter->iCurRecord].cbRecord;
    if (cbRecord == VBVA_F_RECORD_PARTIAL)
        return NULL;
    if (pcbBuffer)
        *pcbBuffer = cbRecord;
    if (pfProcessed)
        *pfProcessed = !vboxVBVAExIsEntryInRange(pVBVA->indexRecordFirst, pIter->iCurRecord, pVBVA->indexRecordFree);
    return &pVBVA->au8Data[pIter->off32CurCmd];
}

DECLINLINE(uint32_t) vboxVBVAExSubst(uint32_t x, uint32_t val, uint32_t maxVal)
{
    int32_t result = (int32_t)(x - val);
    return result >= 0 ? (uint32_t)result : maxVal - (((uint32_t)(-result)) % maxVal);
}

RTDECL(void) VBoxVBVAExBIterInit(PVBVAEXBUFFERCONTEXT pCtx, PVBVAEXBUFFERBACKWARDITER pIter)
{
    struct VBVABUFFER *pVBVA = pCtx->pVBVA;
    pIter->Base.pCtx = pCtx;
    uint32_t iCurRecord = vboxVBVAExSubst(pVBVA->indexRecordFree, 1, VBVA_MAX_RECORDS);
    if (vboxVBVAExIsEntryInRange(pCtx->indexRecordFirstUncompleted, iCurRecord, pVBVA->indexRecordFree))
    {
        /* even if the command gets completed by the time we're doing the pCtx->pVBVA->aRecords[iCurRecord].cbRecord below,
         * the pCtx->pVBVA->aRecords[iCurRecord].cbRecord will still be valid, as it can only be modified by a submitter,
         * and we are in a submitter context now */
        pIter->Base.iCurRecord = iCurRecord;
        pIter->Base.off32CurCmd = vboxVBVAExSubst(pVBVA->off32Free, pCtx->pVBVA->aRecords[iCurRecord].cbRecord, pVBVA->cbData);
    }
    else
    {
        /* no data */
        pIter->Base.iCurRecord = pVBVA->indexRecordFree;
        pIter->Base.off32CurCmd = pVBVA->off32Free;
    }
}

RTDECL(void*) VBoxVBVAExBIterNext(PVBVAEXBUFFERBACKWARDITER pIter, uint32_t *pcbBuffer, bool *pfProcessed)
{
    PVBVAEXBUFFERCONTEXT pCtx = pIter->Base.pCtx;
    struct VBVABUFFER *pVBVA = pCtx->pVBVA;
    uint32_t indexRecordFirstUncompleted = pCtx->indexRecordFirstUncompleted;
    if (!vboxVBVAExIsEntryInRange(indexRecordFirstUncompleted, pIter->Base.iCurRecord, pVBVA->indexRecordFree))
        return NULL;

    void *pvBuffer = vboxVBVAExIterCur(&pIter->Base, pVBVA, pcbBuffer, pfProcessed);
    AssertRelease(pvBuffer);

    /* even if the command gets completed by the time we're doing the pCtx->pVBVA->aRecords[pIter->Base.iCurRecord].cbRecord below,
     * the pCtx->pVBVA->aRecords[pIter->Base.iCurRecord].cbRecord will still be valid, as it can only be modified by a submitter,
     * and we are in a submitter context now */
    pIter->Base.iCurRecord = vboxVBVAExSubst(pIter->Base.iCurRecord, 1, VBVA_MAX_RECORDS);
    pIter->Base.off32CurCmd = vboxVBVAExSubst(pIter->Base.off32CurCmd, pCtx->pVBVA->aRecords[pIter->Base.iCurRecord].cbRecord, pVBVA->cbData);

    return pvBuffer;
}

RTDECL(void) VBoxVBVAExCFIterInit(PVBVAEXBUFFERCONTEXT pCtx, PVBVAEXBUFFERFORWARDITER pIter)
{
    pIter->Base.pCtx = pCtx;
    pIter->Base.iCurRecord = pCtx->indexRecordFirstUncompleted;
    pIter->Base.off32CurCmd = pCtx->off32DataUncompleted;
}

RTDECL(void*) VBoxVBVAExCFIterNext(PVBVAEXBUFFERFORWARDITER pIter, uint32_t *pcbBuffer, bool *pfProcessed)
{
    PVBVAEXBUFFERCONTEXT pCtx = pIter->Base.pCtx;
    struct VBVABUFFER *pVBVA = pCtx->pVBVA;
    uint32_t indexRecordFree = pVBVA->indexRecordFree;
    if (!vboxVBVAExIsEntryInRange(pCtx->indexRecordFirstUncompleted, pIter->Base.iCurRecord, indexRecordFree))
        return NULL;

    uint32_t cbBuffer;
    void *pvData = vboxVBVAExIterCur(&pIter->Base, pVBVA, &cbBuffer, pfProcessed);
    if (!pvData)
        return NULL;

    pIter->Base.iCurRecord = (pIter->Base.iCurRecord + 1) % VBVA_MAX_RECORDS;
    pIter->Base.off32CurCmd = (pIter->Base.off32CurCmd + cbBuffer) % pVBVA->cbData;

    if (pcbBuffer)
        *pcbBuffer = cbBuffer;

    return pvData;
}

/**/

int VBoxCmdVbvaEnable(PVBOXMP_DEVEXT pDevExt, VBOXCMDVBVA *pVbva)
{
    if (VBoxVBVAExEnable(&pVbva->Vbva, &VBoxCommonFromDeviceExt(pDevExt)->guestCtx, pVbva->Vbva.pVBVA))
        return VINF_SUCCESS;

    WARN(("VBoxVBVAExEnable failed!"));
    return VERR_GENERAL_FAILURE;
}

int VBoxCmdVbvaDisable(PVBOXMP_DEVEXT pDevExt, VBOXCMDVBVA *pVbva)
{
    VBoxVBVAExDisable(&pVbva->Vbva, &VBoxCommonFromDeviceExt(pDevExt)->guestCtx);
    return VINF_SUCCESS;
}

int VBoxCmdVbvaDestroy(PVBOXMP_DEVEXT pDevExt, VBOXCMDVBVA *pVbva)
{
    int rc = VINF_SUCCESS;
    VBoxMPCmnUnmapAdapterMemory(VBoxCommonFromDeviceExt(pDevExt), (void**)&pVbva->Vbva.pVBVA);
    memset(pVbva, 0, sizeof (*pVbva));
    return rc;
}

static void vboxCmdVbvaDdiNotifyCompleteIrq(PVBOXMP_DEVEXT pDevExt, VBOXCMDVBVA *pVbva, UINT u32FenceId, DXGK_INTERRUPT_TYPE enmComplType)
{
    DXGKARGCB_NOTIFY_INTERRUPT_DATA notify;
    memset(&notify, 0, sizeof(DXGKARGCB_NOTIFY_INTERRUPT_DATA));
    switch (enmComplType)
    {
        case DXGK_INTERRUPT_DMA_COMPLETED:
            notify.InterruptType = DXGK_INTERRUPT_DMA_COMPLETED;
            notify.DmaCompleted.SubmissionFenceId = u32FenceId;
            notify.DmaCompleted.NodeOrdinal = pVbva->idNode;
            break;

        case DXGK_INTERRUPT_DMA_PREEMPTED:
            notify.InterruptType = DXGK_INTERRUPT_DMA_PREEMPTED;
            notify.DmaPreempted.PreemptionFenceId = u32FenceId;
            notify.DmaPreempted.NodeOrdinal = pVbva->idNode;
            notify.DmaPreempted.LastCompletedFenceId = pVbva->u32FenceCompleted;
            break;

        case DXGK_INTERRUPT_DMA_FAULTED:
            Assert(0);
            notify.InterruptType = DXGK_INTERRUPT_DMA_FAULTED;
            notify.DmaFaulted.FaultedFenceId = u32FenceId;
            notify.DmaFaulted.Status = STATUS_UNSUCCESSFUL; /* @todo: better status ? */
            notify.DmaFaulted.NodeOrdinal = pVbva->idNode;
            break;

        default:
            WARN(("unrecognized completion type %d", enmComplType));
            break;
    }

    pDevExt->u.primary.DxgkInterface.DxgkCbNotifyInterrupt(pDevExt->u.primary.DxgkInterface.DeviceHandle, &notify);
}

typedef struct VBOXCMDVBVA_NOTIFYCOMPLETED_CB
{
    PVBOXMP_DEVEXT pDevExt;
    VBOXCMDVBVA *pVbva;
    volatile UINT *pu32FenceId;
    DXGK_INTERRUPT_TYPE enmComplType;
} VBOXCMDVBVA_NOTIFYCOMPLETED_CB, *PVBOXCMDVBVA_NOTIFYCOMPLETED_CB;

static BOOLEAN vboxCmdVbvaDdiNotifyCompleteCb(PVOID pvContext)
{
    PVBOXCMDVBVA_NOTIFYCOMPLETED_CB pData = (PVBOXCMDVBVA_NOTIFYCOMPLETED_CB)pvContext;
    if (*pData->pu32FenceId)
    {
        UINT u32FenceId = *pData->pu32FenceId;
        *pData->pu32FenceId = 0;

        vboxCmdVbvaDdiNotifyCompleteIrq(pData->pDevExt, pData->pVbva, u32FenceId, pData->enmComplType);

        pData->pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pData->pDevExt->u.primary.DxgkInterface.DeviceHandle);

        return TRUE;
    }

    return FALSE;
}

static int vboxCmdVbvaDdiNotifyComplete(PVBOXMP_DEVEXT pDevExt, VBOXCMDVBVA *pVbva, volatile UINT *pu32FenceId, DXGK_INTERRUPT_TYPE enmComplType)
{
    VBOXCMDVBVA_NOTIFYCOMPLETED_CB Data;
    Data.pDevExt = pDevExt;
    Data.pVbva = pVbva;
    Data.pu32FenceId = pu32FenceId;
    Data.enmComplType = enmComplType;
    BOOLEAN bDummy;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vboxCmdVbvaDdiNotifyCompleteCb,
            &Data,
            0, /* IN ULONG MessageNumber */
            &bDummy);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbSynchronizeExecution failed Status %#x", Status));
        return VERR_GENERAL_FAILURE;
    }
    return Status;
}

static int vboxCmdVbvaFlush(PVBOXMP_DEVEXT pDevExt, HGSMIGUESTCOMMANDCONTEXT *pCtx, bool fBufferOverflow)
{
    /* Issue the flush command. */
    VBVACMDVBVAFLUSH *pFlush = (VBVACMDVBVAFLUSH*)VBoxHGSMIBufferAlloc(pCtx,
                                   sizeof (VBVACMDVBVAFLUSH),
                                   HGSMI_CH_VBVA,
                                   VBVA_CMDVBVA_FLUSH);
    if (!pFlush)
    {
        WARN(("VBoxHGSMIBufferAlloc failed\n"));
        return VERR_OUT_OF_RESOURCES;
    }

    pFlush->u32Flags = fBufferOverflow ?  VBVACMDVBVAFLUSH_F_GUEST_BUFFER_OVERFLOW : 0;

    VBoxHGSMIBufferSubmit(pCtx, pFlush);

    VBoxHGSMIBufferFree(pCtx, pFlush);

    return VINF_SUCCESS;
}

typedef struct VBOXCMDVBVA_CHECK_COMPLETED_CB
{
    PVBOXMP_DEVEXT pDevExt;
    VBOXCMDVBVA *pVbva;
    uint32_t u32FenceID;
} VBOXCMDVBVA_CHECK_COMPLETED_CB;

static BOOLEAN vboxCmdVbvaCheckCompletedIrqCb(PVOID pContext)
{
    VBOXCMDVBVA_CHECK_COMPLETED_CB *pCompleted = (VBOXCMDVBVA_CHECK_COMPLETED_CB*)pContext;
    BOOLEAN bRc = DxgkDdiInterruptRoutineNew(pCompleted->pDevExt, 0);
    if (pCompleted->pVbva)
        pCompleted->u32FenceID = pCompleted->pVbva->u32FenceCompleted;
    return bRc;
}


static uint32_t vboxCmdVbvaCheckCompleted(PVBOXMP_DEVEXT pDevExt, VBOXCMDVBVA *pVbva, bool fPingHost, HGSMIGUESTCOMMANDCONTEXT *pCtx, bool fBufferOverflow)
{
    if (fPingHost)
        vboxCmdVbvaFlush(pDevExt, pCtx, fBufferOverflow);

    VBOXCMDVBVA_CHECK_COMPLETED_CB context;
    context.pDevExt = pDevExt;
    context.pVbva = pVbva;
    context.u32FenceID = 0;
    BOOLEAN bRet;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
                            pDevExt->u.primary.DxgkInterface.DeviceHandle,
                            vboxCmdVbvaCheckCompletedIrqCb,
                            &context,
                            0, /* IN ULONG MessageNumber */
                            &bRet);
    Assert(Status == STATUS_SUCCESS);

    return context.u32FenceID;
}

DECLCALLBACK(void) voxCmdVbvaFlushCb(struct VBVAEXBUFFERCONTEXT *pCtx, PHGSMIGUESTCOMMANDCONTEXT pHGSMICtx, void *pvFlush)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)pvFlush;

    vboxCmdVbvaCheckCompleted(pDevExt, NULL,  true /*fPingHost*/, pHGSMICtx, true /*fBufferOverflow*/);
}

int VBoxCmdVbvaCreate(PVBOXMP_DEVEXT pDevExt, VBOXCMDVBVA *pVbva, ULONG offBuffer, ULONG cbBuffer)
{
    memset(pVbva, 0, sizeof (*pVbva));

    int rc = VBoxMPCmnMapAdapterMemory(VBoxCommonFromDeviceExt(pDevExt),
                                       (void**)&pVbva->Vbva.pVBVA,
                                       offBuffer,
                                       cbBuffer);
    if (RT_SUCCESS(rc))
    {
        Assert(pVbva->Vbva.pVBVA);
        VBoxVBVAExSetupBufferContext(&pVbva->Vbva, offBuffer, cbBuffer, voxCmdVbvaFlushCb, pDevExt);
    }
    else
    {
        WARN(("VBoxMPCmnMapAdapterMemory failed rc %d", rc));
    }

    return rc;
}

int VBoxCmdVbvaSubmit(PVBOXMP_DEVEXT pDevExt, VBOXCMDVBVA *pVbva, struct VBOXCMDVBVA_HDR *pCmd, uint32_t cbCmd)
{
    int rc = VINF_SUCCESS;

    pCmd->u8State = VBOXCMDVBVA_STATE_SUBMITTED;
    pVbva->u32FenceSubmitted = pCmd->u32FenceID;

    if (VBoxVBVAExGetSize(&pVbva->Vbva) > cbCmd)
    {
        WARN(("buffer does not fit the vbva buffer, we do not support splitting buffers"));
        return VERR_NOT_SUPPORTED;
    }

    if (!VBoxVBVAExBufferBeginUpdate(&pVbva->Vbva, &VBoxCommonFromDeviceExt(pDevExt)->guestCtx))
    {
        WARN(("VBoxVBVAExBufferBeginUpdate failed!"));
        return VERR_GENERAL_FAILURE;
    }

    void* pvBuffer = VBoxVBVAExAllocContiguous(&pVbva->Vbva, &VBoxCommonFromDeviceExt(pDevExt)->guestCtx, cbCmd);
    if (!pvBuffer)
    {
        WARN(("failed to allocate contiguous buffer, trying nopping the tail"));
        uint32_t cbTail = VBoxVBVAExGetFreeTail(&pVbva->Vbva);
        if (!cbTail)
        {
            WARN(("this is not a free tail case, cbTail is NULL"));
            return VERR_BUFFER_OVERFLOW;
        }

        Assert(cbTail < cbCmd);

        pvBuffer = VBoxVBVAExAllocContiguous(&pVbva->Vbva, &VBoxCommonFromDeviceExt(pDevExt)->guestCtx, cbTail);

        Assert(pvBuffer);

        *((uint8_t*)pvBuffer) = VBOXCMDVBVA_OPTYPE_NOP;

        VBoxVBVAExBufferEndUpdate(&pVbva->Vbva);

        if (!VBoxVBVAExBufferBeginUpdate(&pVbva->Vbva, &VBoxCommonFromDeviceExt(pDevExt)->guestCtx))
        {
            WARN(("VBoxVBVAExBufferBeginUpdate 2 failed!"));
            return VERR_GENERAL_FAILURE;
        }

        pvBuffer = VBoxVBVAExAllocContiguous(&pVbva->Vbva, &VBoxCommonFromDeviceExt(pDevExt)->guestCtx, cbCmd);
        if (!pvBuffer)
        {
            WARN(("failed to allocate contiguous buffer, failing"));
            return VERR_GENERAL_FAILURE;
        }
    }

    Assert(pvBuffer);

    memcpy(pvBuffer, pCmd, cbCmd);

    VBoxVBVAExBufferEndUpdate(&pVbva->Vbva);

    if (!VBoxVBVAExIsProcessing(&pVbva->Vbva))
    {
        /* Issue the submit command. */
        HGSMIGUESTCOMMANDCONTEXT *pCtx = &VBoxCommonFromDeviceExt(pDevExt)->guestCtx;
        VBVACMDVBVASUBMIT *pSubmit = (VBVACMDVBVASUBMIT*)VBoxHGSMIBufferAlloc(pCtx,
                                       sizeof (VBVACMDVBVASUBMIT),
                                       HGSMI_CH_VBVA,
                                       VBVA_CMDVBVA_SUBMIT);
        if (!pSubmit)
        {
            WARN(("VBoxHGSMIBufferAlloc failed\n"));
            return VERR_OUT_OF_RESOURCES;
        }

        pSubmit->u32Reserved = 0;

        VBoxHGSMIBufferSubmit(pCtx, pSubmit);

        VBoxHGSMIBufferFree(pCtx, pSubmit);
    }

    return VINF_SUCCESS;
}

bool VBoxCmdVbvaPreempt(PVBOXMP_DEVEXT pDevExt, VBOXCMDVBVA *pVbva, uint32_t u32FenceID)
{
    VBVAEXBUFFERBACKWARDITER Iter;
    VBoxVBVAExBIterInit(&pVbva->Vbva, &Iter);

    uint32_t cbBuffer;
    bool fProcessed;
    uint8_t* pu8Cmd;

    while ((pu8Cmd = (uint8_t*)VBoxVBVAExBIterNext(&Iter, &cbBuffer, &fProcessed)) != NULL)
    {
        if (*pu8Cmd == VBOXCMDVBVA_OPTYPE_NOP)
            continue;

        VBOXCMDVBVA_HDR *pCmd = (VBOXCMDVBVA_HDR*)pu8Cmd;

        if (pCmd->u32FenceID != u32FenceID)
            continue;

        if (!ASMAtomicCmpXchgU8(&pCmd->u8State, VBOXCMDVBVA_STATE_CANCELLED, VBOXCMDVBVA_STATE_SUBMITTED))
        {
            Assert(pCmd->u8State == VBOXCMDVBVA_STATE_IN_PROGRESS);
            break;
        }

        /* we have canceled the command successfully */
        vboxCmdVbvaDdiNotifyComplete(pDevExt, pVbva, &pCmd->u32FenceID, DXGK_INTERRUPT_DMA_PREEMPTED);
        return true;
    }

    return false;
}

bool VBoxCmdVbvaCheckCompletedIrq(PVBOXMP_DEVEXT pDevExt, VBOXCMDVBVA *pVbva)
{
    VBVAEXBUFFERFORWARDITER Iter;
    VBoxVBVAExCFIterInit(&pVbva->Vbva, &Iter);

    bool fHasCommandsCompletedPreempted = false;
    bool fProcessed;
    uint8_t* pu8Cmd;


    while ((pu8Cmd = (uint8_t*)VBoxVBVAExCFIterNext(&Iter, NULL, &fProcessed)) != NULL)
    {
        if (!fProcessed)
            break;

        if (*pu8Cmd == VBOXCMDVBVA_OPTYPE_NOP)
            continue;

        VBOXCMDVBVA_HDR *pCmd = (VBOXCMDVBVA_HDR*)pu8Cmd;
        uint8_t u8State = pCmd->u8State;
        uint32_t u32FenceID = pCmd->u32FenceID;

        Assert(u8State == VBOXCMDVBVA_STATE_IN_PROGRESS
                || u8State == VBOXCMDVBVA_STATE_CANCELLED);
        Assert(u32FenceID);
        VBoxVBVAExCBufferCompleted(&pVbva->Vbva);
        DXGK_INTERRUPT_TYPE enmDdiNotify;

        if (u8State == VBOXCMDVBVA_STATE_IN_PROGRESS)
        {
            if (u32FenceID)
                pVbva->u32FenceCompleted = u32FenceID;
            enmDdiNotify = DXGK_INTERRUPT_DMA_COMPLETED;
        }
        else
        {
            Assert(u8State == VBOXCMDVBVA_STATE_CANCELLED);
            enmDdiNotify = DXGK_INTERRUPT_DMA_PREEMPTED;
            /* to prevent concurrent notifications from DdiPreemptCommand */
            pCmd->u32FenceID = 0;
        }

        if (u32FenceID)
            vboxCmdVbvaDdiNotifyCompleteIrq(pDevExt, pVbva, u32FenceID, enmDdiNotify);

        fHasCommandsCompletedPreempted = true;
    }

    return fHasCommandsCompletedPreempted;
}

uint32_t VBoxCmdVbvaCheckCompleted(PVBOXMP_DEVEXT pDevExt, VBOXCMDVBVA *pVbva, bool fPingHost)
{
    return vboxCmdVbvaCheckCompleted(pDevExt, pVbva, fPingHost, &VBoxCommonFromDeviceExt(pDevExt)->guestCtx, false /* fBufferOverflow */);
}


static uint32_t vboxCVDdiSysMemElBuild(VBOXCMDVBVA_SYSMEMEL *pEl, PMDL pMdl, uint32_t iPfn, uint32_t cPages)
{
    PFN_NUMBER cur = MmGetMdlPfnArray(pMdl)[iPfn];
    uint32_t cbEl = sizeof (*pEl);
    uint32_t cStoredPages = 1;
    PFN_NUMBER next;
    pEl->iPage1 = (uint32_t)(cur & 0xfffff);
    pEl->iPage2 = (uint32_t)(cur >> 20);
    --cPages;
    for ( ; cPages && cStoredPages < VBOXCMDVBVA_SYSMEMEL_CPAGES_MAX; --cPages, ++cStoredPages, cur = next;)
    {
        next = MmGetMdlPfnArray(pMdl)[iPfn+cStoredPages];
        if (next != cur+1)
            break;
    }

    Assert(cStoredPages);
    pEl->cPagesAfterFirst = cStoredPages - 1;

    return cPages;
}

uint32_t VBoxCVDdiPTransferVRamSysBuildEls(VBOXCMDVBVA_PAGING_TRANSFER *pCmd, PMDL pMdl, uint32_t iPfn, uint32_t cPages, uint32_t cbBuffer, uint32_t *pcPagesWritten)
{
    uint32_t cInitPages = cPages;
    uint32_t cbInitBuffer = cbBuffer;
    uint32_t cEls = 0;
    VBOXCMDVBVA_SYSMEMEL *pEl = pCmd->aSysMem;

    if (cbBuffer < sizeof (VBOXCMDVBVA_PAGING_TRANSFER))
    {
        WARN(("cbBuffer < sizeof (VBOXCMDVBVA_PAGING_TRANSFER)"));
        goto done;
    }

    cbBuffer -= RT_OFFSETOF(VBOXCMDVBVA_PAGING_TRANSFER, aSysMem);
    uint32_t i = 0;

    for (; cPages && cbBuffer >= sizeof (VBOXCMDVBVA_PAGING_TRANSFER); ++cEls, cbBuffer-=sizeof (VBOXCMDVBVA_SYSMEMEL), ++pEl, ++i)
    {
        cPages = vboxCVDdiSysMemElBuild(pEl, pMdl, iPfn + cInitPages - cPages, cPages);
    }

    pCmd->cSysMem = i;

done:
    *pcPagesWritten = cInitPages - cPages;
    return cbInitBuffer - cbBuffer;
}

