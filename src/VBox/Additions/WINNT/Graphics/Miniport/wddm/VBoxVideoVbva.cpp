/** @file
 *
 * VirtualBox Windows Wddm guest video driver
 *
 * VBVA dirty rectangles calculations.
 *
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "../VBoxVideo.h"
#include "../Helper.h"

static int vboxVBVAInformHost (PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO * pVbva, BOOL bEnable)
{
    int rc = VERR_NO_MEMORY;
    void *p = vboxHGSMIBufferAlloc (pDevExt,
                                  sizeof (VBVAENABLE_EX),
                                  HGSMI_CH_VBVA,
                                  VBVA_ENABLE);
    Assert(p);
    if (!p)
    {
        drprintf((__FUNCTION__":vboxVBVAInformHost: HGSMIHeapAlloc failed\n"));
        rc = VERR_NO_MEMORY;
    }
    else
    {
        VBVAENABLE_EX *pEnableEx = (VBVAENABLE_EX *)p;
        pEnableEx->u32ScreenId = pVbva->srcId;

        VBVAENABLE *pEnable = &pEnableEx->Base;
        pEnable->u32Flags  = bEnable? VBVA_F_ENABLE: VBVA_F_DISABLE;
        pEnable->u32Flags |= VBVA_F_EXTENDED;
        pEnable->u32Offset = (uint32_t)pVbva->offVBVA;
        pEnable->i32Result = VERR_NOT_SUPPORTED;

        vboxHGSMIBufferSubmit (pDevExt, p);

        if (bEnable)
        {
            rc = pEnable->i32Result;
            AssertRC(rc);
        }
        else
            rc = VINF_SUCCESS;

        vboxHGSMIBufferFree (pDevExt, p);
    }
    return rc;
}

/*
 * Public hardware buffer methods.
 */
int vboxVbvaEnable (PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO *pVbva)
{
        VBVABUFFER *pVBVA = pVbva->pVBVA;

//        DISPDBG((1, "VBoxDisp::vboxVbvaEnable screen %p vbva off 0x%x\n", ppdev->pjScreen, ppdev->layout.offVBVABuffer));

        pVBVA->hostFlags.u32HostEvents      = 0;
        pVBVA->hostFlags.u32SupportedOrders = 0;
        pVBVA->off32Data          = 0;
        pVBVA->off32Free          = 0;
        RtlZeroMemory (pVBVA->aRecords, sizeof (pVBVA->aRecords));
        pVBVA->indexRecordFirst   = 0;
        pVBVA->indexRecordFree    = 0;
        pVBVA->cbPartialWriteThreshold = 256;
        pVBVA->cbData             = pVbva->cbVBVA - sizeof (VBVABUFFER) + sizeof (pVBVA->au8Data);

        pVbva->fHwBufferOverflow = FALSE;
        pVbva->pRecord           = NULL;

        int rc = vboxVBVAInformHost (pDevExt, pVbva, TRUE);
        AssertRC(rc);

    if (!RT_SUCCESS(rc))
        vboxVbvaDisable (pDevExt, pVbva);

    return rc;
}

int vboxVbvaDisable (PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO *pVbva)
{
//    DISPDBG((1, "VBoxDisp::vbvaDisable called.\n"));

    pVbva->fHwBufferOverflow = FALSE;
    pVbva->pRecord           = NULL;
//    ppdev->pVBVA             = NULL;

    return vboxVBVAInformHost (pDevExt, pVbva, FALSE);
}

int vboxVbvaCreate(PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO *pVbva, ULONG offBuffer, ULONG cbBuffer, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    memset(pVbva, 0, sizeof(VBOXVBVAINFO));
    int rc = VBoxMapAdapterMemory (pDevExt,
                                       (void**)&pVbva->pVBVA,
                                       offBuffer,
                                       cbBuffer);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        Assert(pVbva->pVBVA);
        pVbva->offVBVA = offBuffer;
        pVbva->cbVBVA = cbBuffer;
        pVbva->srcId = srcId;
    }

    return rc;
}

/*
 * Private operations.
 */
static uint32_t vboxHwBufferAvail (const VBVABUFFER *pVBVA)
{
    int32_t i32Diff = pVBVA->off32Data - pVBVA->off32Free;

    return i32Diff > 0? i32Diff: pVBVA->cbData + i32Diff;
}

static void vboxHwBufferFlush (PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO *pVbva)
{
    /* Issue the flush command. */
    void *p = vboxHGSMIBufferAlloc (pDevExt,
                              sizeof (VBVAFLUSH),
                              HGSMI_CH_VBVA,
                              VBVA_FLUSH);
    Assert(p);
    if (!p)
    {
        drprintf((__FUNCTION__":vboxHwBufferFlush: HGSMIHeapAlloc failed\n"));
    }
    else
    {
        VBVAFLUSH *pFlush = (VBVAFLUSH *)p;

        pFlush->u32Reserved = 0;

        vboxHGSMIBufferSubmit (pDevExt, p);

        vboxHGSMIBufferFree (pDevExt, p);
    }

    return;
}

static void vboxHwBufferPlaceDataAt (VBVABUFFER *pVBVA, const void *p, uint32_t cb, uint32_t offset)
{
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

BOOL vboxVbvaBufferBeginUpdate (PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO *pVbva)
{
    BOOL bRc = FALSE;

    // DISPDBG((1, "VBoxDisp::vboxHwBufferBeginUpdate called flags = 0x%08X\n",
    //          ppdev->pVBVA? ppdev->pVBVA->u32HostEvents: -1));

    if (   pVbva->pVBVA
        && (pVbva->pVBVA->hostFlags.u32HostEvents & VBVA_F_MODE_ENABLED))
    {
        uint32_t indexRecordNext;

        Assert (!pVbva->fHwBufferOverflow);
        Assert (pVbva->pRecord == NULL);

        indexRecordNext = (pVbva->pVBVA->indexRecordFree + 1) % VBVA_MAX_RECORDS;

        if (indexRecordNext == pVbva->pVBVA->indexRecordFirst)
        {
            /* All slots in the records queue are used. */
            vboxHwBufferFlush (pDevExt, pVbva);
        }

        if (indexRecordNext == pVbva->pVBVA->indexRecordFirst)
        {
//            /* Even after flush there is no place. Fail the request. */
//            DISPDBG((1, "VBoxDisp::vboxHwBufferBeginUpdate no space in the queue of records!!! first %d, last %d\n",
//                     ppdev->pVBVA->indexRecordFirst, ppdev->pVBVA->indexRecordFree));
        }
        else
        {
            /* Initialize the record. */
            VBVARECORD *pRecord = &pVbva->pVBVA->aRecords[pVbva->pVBVA->indexRecordFree];

            pRecord->cbRecord = VBVA_F_RECORD_PARTIAL;

            pVbva->pVBVA->indexRecordFree = indexRecordNext;

            // DISPDBG((1, "VBoxDisp::vboxHwBufferBeginUpdate indexRecordNext = %d\n", indexRecordNext));

            /* Remember which record we are using. */
            pVbva->pRecord = pRecord;

            bRc = TRUE;
        }
    }

    return bRc;
}

void vboxVbvaBufferEndUpdate (PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO *pVbva)
{
    VBVARECORD *pRecord;

    // DISPDBG((1, "VBoxDisp::vboxHwBufferEndUpdate called\n"));

    Assert(pVbva->pVBVA);

    pRecord = pVbva->pRecord;
    Assert (pRecord && (pRecord->cbRecord & VBVA_F_RECORD_PARTIAL));

    /* Mark the record completed. */
    pRecord->cbRecord &= ~VBVA_F_RECORD_PARTIAL;

    pVbva->fHwBufferOverflow = FALSE;
    pVbva->pRecord = NULL;

    return;
}

static int vboxHwBufferWrite (PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO *pVbva, const void *p, uint32_t cb)
{
    VBVARECORD *pRecord;
    uint32_t cbHwBufferAvail;

    uint32_t cbWritten = 0;

    VBVABUFFER *pVBVA = pVbva->pVBVA;
    Assert(pVBVA);

    if (!pVBVA || pVbva->fHwBufferOverflow)
    {
        return VERR_INVALID_STATE;
    }

    Assert (pVBVA->indexRecordFirst != pVBVA->indexRecordFree);

    pRecord = pVbva->pRecord;
    Assert (pRecord && (pRecord->cbRecord & VBVA_F_RECORD_PARTIAL));

    dfprintf((__FUNCTION__": VW %d\n", cb));

    cbHwBufferAvail = vboxHwBufferAvail (pVBVA);

    while (cb > 0)
    {
        uint32_t cbChunk = cb;

        // DISPDBG((1, "VBoxDisp::vboxHwBufferWrite pVBVA->off32Free %d, pRecord->cbRecord 0x%08X, cbHwBufferAvail %d, cb %d, cbWritten %d\n",
        //             pVBVA->off32Free, pRecord->cbRecord, cbHwBufferAvail, cb, cbWritten));

        if (cbChunk >= cbHwBufferAvail)
        {
            dfprintf((__FUNCTION__": 1) avail %d, chunk %d\n", cbHwBufferAvail, cbChunk));

            vboxHwBufferFlush (pDevExt, pVbva);

            cbHwBufferAvail = vboxHwBufferAvail (pVBVA);

            if (cbChunk >= cbHwBufferAvail)
            {
                dprintf((__FUNCTION__": no place for %d bytes. Only %d bytes available after flush. Going to partial writes.\n",
                            cb, cbHwBufferAvail));

                if (cbHwBufferAvail <= pVBVA->cbPartialWriteThreshold)
                {
                    drprintf((__FUNCTION__": Buffer overflow!!!\n"));
                    pVbva->fHwBufferOverflow = TRUE;
                    Assert(FALSE);
                    return VERR_NO_MEMORY;
                }

                cbChunk = cbHwBufferAvail - pVBVA->cbPartialWriteThreshold;
            }
        }

        Assert(cbChunk <= cb);
        Assert(cbChunk <= vboxHwBufferAvail (pVBVA));

        vboxHwBufferPlaceDataAt (pVbva->pVBVA, (uint8_t *)p + cbWritten, cbChunk, pVBVA->off32Free);

        pVBVA->off32Free   = (pVBVA->off32Free + cbChunk) % pVBVA->cbData;
        pRecord->cbRecord += cbChunk;
        cbHwBufferAvail -= cbChunk;

        cb        -= cbChunk;
        cbWritten += cbChunk;
    }

    return VINF_SUCCESS;
}

/*
 * Public writer to the hardware buffer.
 */
int vboxWrite (PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO *pVbva, const void *pv, uint32_t cb)
{
    return vboxHwBufferWrite (pDevExt, pVbva, pv, cb);
}


int vboxVbvaReportDirtyRect (PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO *pVbva, RECT *pRectOrig)
{
        VBVACMDHDR hdr;

        RECT rect = *pRectOrig;

        if (rect.left < 0) rect.left = 0;
        if (rect.top < 0) rect.top = 0;
//        if (rect.right > (int)ppdev->cxScreen) rect.right = ppdev->cxScreen;
//        if (rect.bottom > (int)ppdev->cyScreen) rect.bottom = ppdev->cyScreen;

        hdr.x = (int16_t)rect.left;
        hdr.y = (int16_t)rect.top;
        hdr.w = (uint16_t)(rect.right - rect.left);
        hdr.h = (uint16_t)(rect.bottom - rect.top);

//        hdr.x += (int16_t)ppdev->ptlDevOrg.x;
//        hdr.y += (int16_t)ppdev->ptlDevOrg.y;

        return vboxWrite (pDevExt, pVbva, &hdr, sizeof(hdr));
}

#ifdef VBOXVDMA_WITH_VBVA
int vboxVbvaReportCmdOffset (PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO *pVbva, uint32_t offCmd)
{
    VBOXVDMAVBVACMD cmd;
    cmd.offCmd = offCmd;
    return vboxWrite (pDevExt, pVbva, &cmd, sizeof(cmd));
}
#endif
