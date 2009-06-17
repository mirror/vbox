/** @file
 *
 * VBoxGuest -- VirtualBox Win 2000/XP guest display driver
 *
 * VBox support functions.
 *
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#include "driver.h"

#include <VBox/VBoxGuest.h>
#include <VBox/err.h>
#include <iprt/asm.h>

/*
 * There is a hardware ring buffer in the VBox VMMDev PCI memory space.
 * All graphics commands go there serialized by vboxHwBufferBeginUpdate.
 * and vboxHwBufferEndUpdate.
 *
 * off32Free is writing position. off32Data is reading position.
 * off32Free == off32Data means buffer is empty.
 * There must be always gap between off32Data and off32Free when data
 * are in the buffer.
 * Guest only changes off32Free, host changes off32Data.
 */

/* Forward declarations of internal functions. */
static void vboxHwBufferFlush (PPDEV ppdev);
static void vboxHwBufferPlaceDataAt (PPDEV ppdev, void *p, uint32_t cb, uint32_t offset);
static BOOL vboxHwBufferWrite (PPDEV ppdev, const void *p, uint32_t cb);

#ifndef VBOX_WITH_HGSMI
/*
 * Public hardware buffer methods.
 */
BOOL vboxVbvaEnable (PPDEV ppdev)
{
    BOOL bRc = FALSE;

    ULONG returnedDataLength;
    ULONG ulEnable = TRUE;

    DISPDBG((1, "VBoxDisp::vboxVbvaEnable called\n"));

    if (!ghsemHwBuffer)
    {
        return FALSE;
    }

    if (EngDeviceIoControl(ppdev->hDriver,
                           IOCTL_VIDEO_VBVA_ENABLE,
                           &ulEnable,
                           sizeof (ulEnable),
                           &ppdev->vbva,
                           sizeof (ppdev->vbva),
                           &returnedDataLength) == 0)
    {
        DISPDBG((1, "VBoxDisp::vboxVbvaEnable: vbva: pVbvaMemory = %p, pfnFlush = %p, pvFlush = %p.\n",
                     ppdev->vbva.pVbvaMemory, ppdev->vbva.pfnFlush, ppdev->vbva.pvFlush));

        if (ppdev->vbva.pVbvaMemory
            && ppdev->vbva.pfnFlush
            && ppdev->vbva.pvFlush)
        {
            ppdev->fHwBufferOverflow = FALSE;
            ppdev->pRecord           = NULL;

            /* All have been initialized. */
            bRc = TRUE;
        }
    }

    if (!bRc)
    {
        vboxVbvaDisable (ppdev);
    }

    return bRc;
}

void vboxVbvaDisable (PPDEV ppdev)
{
    DISPDBG((1, "VBoxDisp::vbvaDisable called.\n"));

    RtlZeroMemory (&ppdev->vbva, sizeof (ppdev->vbva));

    ppdev->fHwBufferOverflow = FALSE;
    ppdev->pRecord           = NULL;

    return;
}

BOOL vboxHwBufferBeginUpdate (PPDEV ppdev)
{
    BOOL bRc = FALSE;

    VBVAMEMORY *pVbvaMemory = ppdev->vbva.pVbvaMemory;

    DISPDBG((1, "VBoxDisp::vboxHwBufferBeginUpdate called flags = 0x%08X\n", pVbvaMemory? pVbvaMemory->fu32ModeFlags: -1));

    if (   pVbvaMemory
        && (pVbvaMemory->fu32ModeFlags & VBVA_F_MODE_ENABLED))
    {
        uint32_t indexRecordNext;

        EngAcquireSemaphore (ghsemHwBuffer);

        VBVA_ASSERT (!ppdev->fHwBufferOverflow);
        VBVA_ASSERT (ppdev->pRecord == NULL);

        indexRecordNext = (pVbvaMemory->indexRecordFree + 1) % VBVA_MAX_RECORDS;

        if (indexRecordNext == pVbvaMemory->indexRecordFirst)
        {
            /* All slots in the records queue are used. */
            vboxHwBufferFlush (ppdev);
        }

        if (indexRecordNext == pVbvaMemory->indexRecordFirst)
        {
            /* Even after flush there is no place. Fail the request. */
            DISPDBG((1, "VBoxDisp::vboxHwBufferBeginUpdate no space in the queue of records!!! first %d, last %d\n",
                     pVbvaMemory->indexRecordFirst, pVbvaMemory->indexRecordFree));
            EngReleaseSemaphore (ghsemHwBuffer);
        }
        else
        {
            /* Initialize the record. */
            VBVARECORD *pRecord = &pVbvaMemory->aRecords[pVbvaMemory->indexRecordFree];

            pRecord->cbRecord = VBVA_F_RECORD_PARTIAL;

            pVbvaMemory->indexRecordFree = indexRecordNext;

            DISPDBG((1, "VBoxDisp::vboxHwBufferBeginUpdate indexRecordNext = %d\n", indexRecordNext));

            /* Remember which record we are using. */
            ppdev->pRecord = pRecord;

            bRc = TRUE;
        }
    }

    return bRc;
}

void vboxHwBufferEndUpdate (PPDEV ppdev)
{
    VBVAMEMORY *pVbvaMemory;
    VBVARECORD *pRecord;

    DISPDBG((1, "VBoxDisp::vboxHwBufferEndUpdate called\n"));

    pVbvaMemory = ppdev->vbva.pVbvaMemory;
    VBVA_ASSERT(pVbvaMemory);

    pRecord = ppdev->pRecord;
    VBVA_ASSERT (pRecord && (pRecord->cbRecord & VBVA_F_RECORD_PARTIAL));

    /* Mark the record completed. */
    pRecord->cbRecord &= ~VBVA_F_RECORD_PARTIAL;

    ppdev->fHwBufferOverflow = FALSE;
    ppdev->pRecord = NULL;

    EngReleaseSemaphore (ghsemHwBuffer);

    return;
}

/*
 * Private operations.
 */
static uint32_t vboxHwBufferAvail (VBVAMEMORY *pVbvaMemory)
{
    int32_t i32Diff = pVbvaMemory->off32Data - pVbvaMemory->off32Free;

    return i32Diff > 0? i32Diff: VBVA_RING_BUFFER_SIZE + i32Diff;
}

static void vboxHwBufferFlush (PPDEV ppdev)
{
    VBVAMEMORY *pVbvaMemory = ppdev->vbva.pVbvaMemory;

    VBVA_ASSERT (pVbvaMemory);

    ppdev->vbva.pfnFlush (ppdev->vbva.pvFlush);

    return;
}

static void vboxHwBufferPlaceDataAt (PPDEV ppdev, void *p, uint32_t cb, uint32_t offset)
{
    VBVAMEMORY *pVbvaMemory = ppdev->vbva.pVbvaMemory;

    uint32_t u32BytesTillBoundary = VBVA_RING_BUFFER_SIZE - offset;
    uint8_t  *dst                 = &pVbvaMemory->au8RingBuffer[offset];
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
        memcpy (&pVbvaMemory->au8RingBuffer[0], (uint8_t *)p + u32BytesTillBoundary, i32Diff);
    }

    return;
}

static BOOL vboxHwBufferWrite (PPDEV ppdev, const void *p, uint32_t cb)
{
    VBVAMEMORY *pVbvaMemory;
    VBVARECORD *pRecord;
    uint32_t cbHwBufferAvail;

    uint32_t cbWritten = 0;

    VBVA_ASSERT(ppdev);

    if (ppdev->fHwBufferOverflow)
    {
        return FALSE;
    }

    pVbvaMemory = ppdev->vbva.pVbvaMemory;
    VBVA_ASSERT (pVbvaMemory->indexRecordFirst != pVbvaMemory->indexRecordFree);

    pRecord = ppdev->pRecord;
    VBVA_ASSERT (pRecord && (pRecord->cbRecord & VBVA_F_RECORD_PARTIAL));

    DISPDBG((1, "VW %d\n", cb));

    cbHwBufferAvail = vboxHwBufferAvail (pVbvaMemory);

    while (cb > 0)
    {
        uint32_t cbChunk = cb;

//        DISPDBG((1, "VBoxDisp::vboxHwBufferWrite pVbvaMemory->off32Free %d, pRecord->cbRecord 0x%08X, cbHwBufferAvail %d, cb %d, cbWritten %d\n", pVbvaMemory->off32Free, pRecord->cbRecord, cbHwBufferAvail, cb, cbWritten));

        if (cbChunk >= cbHwBufferAvail)
        {
            DISPDBG((1, "VBoxDisp::vboxHwBufferWrite 1) avail %d, chunk %d\n", cbHwBufferAvail, cbChunk));

            vboxHwBufferFlush (ppdev);

            cbHwBufferAvail = vboxHwBufferAvail (pVbvaMemory);

            if (cbChunk >= cbHwBufferAvail)
            {
                DISPDBG((1, "VBoxDisp::vboxHwBufferWrite: no place for %d bytes. Only %d bytes available after flush. Going to partial writes.\n", cb, cbHwBufferAvail));

                if (cbHwBufferAvail <= VBVA_RING_BUFFER_THRESHOLD)
                {
                    DISPDBG((1, "VBoxDisp::vboxHwBufferWrite: Buffer overflow!!!\n"));
                    ppdev->fHwBufferOverflow = TRUE;
                    VBVA_ASSERT(FALSE);
                    return FALSE;
                }

                cbChunk = cbHwBufferAvail - VBVA_RING_BUFFER_THRESHOLD;
            }
        }

        VBVA_ASSERT(cbChunk <= cb);
        VBVA_ASSERT(cbChunk <= vboxHwBufferAvail (pVbvaMemory));

        vboxHwBufferPlaceDataAt (ppdev, (uint8_t *)p + cbWritten, cbChunk, pVbvaMemory->off32Free);

        pVbvaMemory->off32Free  = (pVbvaMemory->off32Free + cbChunk) % VBVA_RING_BUFFER_SIZE;
        pRecord->cbRecord      += cbChunk;
        cbHwBufferAvail -= cbChunk;

        cb        -= cbChunk;
        cbWritten += cbChunk;
    }

    return TRUE;
}

/*
 * Public writer to hardware buffer.
 */
BOOL vboxWrite (PPDEV ppdev, const void *pv, uint32_t cb)
{
    return vboxHwBufferWrite (ppdev, pv, cb);
}

BOOL vboxOrderSupported (PPDEV ppdev, unsigned code)
{
    VBVAMEMORY *pVbvaMemory;

    pVbvaMemory = ppdev->vbva.pVbvaMemory;

    if (pVbvaMemory->fu32ModeFlags & VBVA_F_MODE_VRDP_ORDER_MASK)
    {
        /* Order masking enabled. */
        if (pVbvaMemory->fu32SupportedOrders & (1 << code))
        {
            return TRUE;
        }
    }

    return FALSE;
}

void VBoxProcessDisplayInfo(PPDEV ppdev)
{
    DWORD returnedDataLength;

    DISPDBG((1, "Process: %d,%d\n", ppdev->ptlDevOrg.x, ppdev->ptlDevOrg.y));

    EngDeviceIoControl(ppdev->hDriver,
                       IOCTL_VIDEO_INTERPRET_DISPLAY_MEMORY,
                       NULL,
                       0,
                       NULL,
                       0,
                       &returnedDataLength);
}

#else /* VBOX_WITH_HGSMI */

static void vboxHGSMIBufferSubmit (PPDEV ppdev, void *p)
{
    HGSMIOFFSET offBuffer = HGSMIHeapBufferOffset (&ppdev->hgsmiDisplayHeap, p);

    ASMOutU16 (VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_VBVA_GUEST);
    ASMOutU32 (VBE_DISPI_IOPORT_DATA, offBuffer);
}

static BOOL vboxVBVAInformHost (PPDEV ppdev, BOOL bEnable)
{
    BOOL bRc = FALSE;

    if (ppdev->bHGSMISupported)
    {
        void *p = HGSMIHeapAlloc (&ppdev->hgsmiDisplayHeap,
                                  sizeof (VBVAENABLE),
                                  HGSMI_CH_VBVA,
                                  VBVA_ENABLE);
        if (!p)
        {
            DISPDBG((0, "VBoxDISP::vboxVBVAInformHost: HGSMIHeapAlloc failed\n"));
        }
        else
        {
            VBVAENABLE *pEnable = (VBVAENABLE *)p;

            pEnable->u32Flags  = bEnable? VBVA_F_ENABLE: VBVA_F_DISABLE;
            pEnable->u32Offset = ppdev->layout.offVBVABuffer;

            vboxHGSMIBufferSubmit (ppdev, p);

            HGSMIHeapFree (&ppdev->hgsmiDisplayHeap, p);

            bRc = TRUE;
        }
    }

    return bRc;
}

/*
 * Public hardware buffer methods.
 */
BOOL vboxVbvaEnable (PPDEV ppdev)
{
    BOOL bRc = FALSE;

    DISPDBG((1, "VBoxDisp::vboxVbvaEnable called\n"));

    if (ppdev->bHGSMISupported)
    {
        VBVABUFFER *pVBVA = (VBVABUFFER *)((uint8_t *)ppdev->pjScreen + ppdev->layout.offVBVABuffer);

        pVBVA->u32HostEvents      = 0;
        pVBVA->u32SupportedOrders = 0;
        pVBVA->off32Data          = 0;
        pVBVA->off32Free          = 0;
        RtlZeroMemory (pVBVA->aRecords, sizeof (pVBVA->aRecords));
        pVBVA->indexRecordFirst   = 0;
        pVBVA->indexRecordFree    = 0;
        pVBVA->cbPartialWriteThreshold = 256;
        pVBVA->cbData             = ppdev->layout.cbVBVABuffer - sizeof (VBVABUFFER) + sizeof (pVBVA->au8Data);

        ppdev->fHwBufferOverflow = FALSE;
        ppdev->pRecord           = NULL;
        ppdev->pVBVA             = pVBVA;

        bRc = vboxVBVAInformHost (ppdev, TRUE);
    }

    if (!bRc)
    {
        vboxVbvaDisable (ppdev);
    }

    return bRc;
}

void vboxVbvaDisable (PPDEV ppdev)
{
    DISPDBG((1, "VBoxDisp::vbvaDisable called.\n"));

    ppdev->fHwBufferOverflow = FALSE;
    ppdev->pRecord           = NULL;
    ppdev->pVBVA             = NULL;

    vboxVBVAInformHost (ppdev, FALSE);

    return;
}

BOOL vboxHwBufferBeginUpdate (PPDEV ppdev)
{
    BOOL bRc = FALSE;

    // DISPDBG((1, "VBoxDisp::vboxHwBufferBeginUpdate called flags = 0x%08X\n",
    //          ppdev->pVBVA? ppdev->pVBVA->u32HostEvents: -1));

    if (   ppdev->pVBVA
        && (ppdev->pVBVA->u32HostEvents & VBVA_F_MODE_ENABLED))
    {
        uint32_t indexRecordNext;

        VBVA_ASSERT (!ppdev->fHwBufferOverflow);
        VBVA_ASSERT (ppdev->pRecord == NULL);

        indexRecordNext = (ppdev->pVBVA->indexRecordFree + 1) % VBVA_MAX_RECORDS;

        if (indexRecordNext == ppdev->pVBVA->indexRecordFirst)
        {
            /* All slots in the records queue are used. */
            vboxHwBufferFlush (ppdev);
        }

        if (indexRecordNext == ppdev->pVBVA->indexRecordFirst)
        {
            /* Even after flush there is no place. Fail the request. */
            DISPDBG((1, "VBoxDisp::vboxHwBufferBeginUpdate no space in the queue of records!!! first %d, last %d\n",
                     ppdev->pVBVA->indexRecordFirst, ppdev->pVBVA->indexRecordFree));
        }
        else
        {
            /* Initialize the record. */
            VBVARECORD *pRecord = &ppdev->pVBVA->aRecords[ppdev->pVBVA->indexRecordFree];

            pRecord->cbRecord = VBVA_F_RECORD_PARTIAL;

            ppdev->pVBVA->indexRecordFree = indexRecordNext;

            // DISPDBG((1, "VBoxDisp::vboxHwBufferBeginUpdate indexRecordNext = %d\n", indexRecordNext));

            /* Remember which record we are using. */
            ppdev->pRecord = pRecord;

            bRc = TRUE;
        }
    }

    return bRc;
}

void vboxHwBufferEndUpdate (PPDEV ppdev)
{
    VBVARECORD *pRecord;

    // DISPDBG((1, "VBoxDisp::vboxHwBufferEndUpdate called\n"));

    VBVA_ASSERT(ppdev->pVBVA);

    pRecord = ppdev->pRecord;
    VBVA_ASSERT (pRecord && (pRecord->cbRecord & VBVA_F_RECORD_PARTIAL));

    /* Mark the record completed. */
    pRecord->cbRecord &= ~VBVA_F_RECORD_PARTIAL;

    ppdev->fHwBufferOverflow = FALSE;
    ppdev->pRecord = NULL;

    return;
}

/*
 * Private operations.
 */
static uint32_t vboxHwBufferAvail (const VBVABUFFER *pVBVA)
{
    int32_t i32Diff = pVBVA->off32Data - pVBVA->off32Free;

    return i32Diff > 0? i32Diff: pVBVA->cbData + i32Diff;
}

static void vboxHwBufferFlush (PPDEV ppdev)
{
    /* Issue the flush command. */
    void *p = HGSMIHeapAlloc (&ppdev->hgsmiDisplayHeap,
                              sizeof (VBVA_FLUSH),
                              HGSMI_CH_VBVA,
                              VBVA_FLUSH);
    if (!p)
    {
        DISPDBG((0, "VBoxDISP::vboxHwBufferFlush: HGSMIHeapAlloc failed\n"));
    }
    else
    {
        VBVAFLUSH *pFlush = (VBVAFLUSH *)p;

        pFlush->u32Reserved = 0;

        vboxHGSMIBufferSubmit (ppdev, p);

        HGSMIHeapFree (&ppdev->hgsmiDisplayHeap, p);
    }

    return;
}

static void vboxHwBufferPlaceDataAt (PPDEV ppdev, const void *p, uint32_t cb, uint32_t offset)
{
    VBVABUFFER *pVBVA = ppdev->pVBVA;
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

static BOOL vboxHwBufferWrite (PPDEV ppdev, const void *p, uint32_t cb)
{
    VBVARECORD *pRecord;
    uint32_t cbHwBufferAvail;

    uint32_t cbWritten = 0;

    VBVABUFFER *pVBVA = ppdev->pVBVA;
    VBVA_ASSERT(pVBVA);

    if (!pVBVA || ppdev->fHwBufferOverflow)
    {
        return FALSE;
    }

    VBVA_ASSERT (pVBVA->indexRecordFirst != pVBVA->indexRecordFree);

    pRecord = ppdev->pRecord;
    VBVA_ASSERT (pRecord && (pRecord->cbRecord & VBVA_F_RECORD_PARTIAL));

    DISPDBG((1, "VW %d\n", cb));

    cbHwBufferAvail = vboxHwBufferAvail (pVBVA);

    while (cb > 0)
    {
        uint32_t cbChunk = cb;

        // DISPDBG((1, "VBoxDisp::vboxHwBufferWrite pVBVA->off32Free %d, pRecord->cbRecord 0x%08X, cbHwBufferAvail %d, cb %d, cbWritten %d\n",
        //             pVBVA->off32Free, pRecord->cbRecord, cbHwBufferAvail, cb, cbWritten));

        if (cbChunk >= cbHwBufferAvail)
        {
            DISPDBG((1, "VBoxDisp::vboxHwBufferWrite 1) avail %d, chunk %d\n", cbHwBufferAvail, cbChunk));

            vboxHwBufferFlush (ppdev);

            cbHwBufferAvail = vboxHwBufferAvail (pVBVA);

            if (cbChunk >= cbHwBufferAvail)
            {
                DISPDBG((1, "VBoxDisp::vboxHwBufferWrite: no place for %d bytes. Only %d bytes available after flush. Going to partial writes.\n",
                            cb, cbHwBufferAvail));

                if (cbHwBufferAvail <= pVBVA->cbPartialWriteThreshold)
                {
                    DISPDBG((1, "VBoxDisp::vboxHwBufferWrite: Buffer overflow!!!\n"));
                    ppdev->fHwBufferOverflow = TRUE;
                    VBVA_ASSERT(FALSE);
                    return FALSE;
                }

                cbChunk = cbHwBufferAvail - pVBVA->cbPartialWriteThreshold;
            }
        }

        VBVA_ASSERT(cbChunk <= cb);
        VBVA_ASSERT(cbChunk <= vboxHwBufferAvail (pVBVA));

        vboxHwBufferPlaceDataAt (ppdev, (uint8_t *)p + cbWritten, cbChunk, pVBVA->off32Free);

        pVBVA->off32Free   = (pVBVA->off32Free + cbChunk) % pVBVA->cbData;
        pRecord->cbRecord += cbChunk;
        cbHwBufferAvail -= cbChunk;

        cb        -= cbChunk;
        cbWritten += cbChunk;
    }

    return TRUE;
}

/*
 * Public writer to the hardware buffer.
 */
BOOL vboxWrite (PPDEV ppdev, const void *pv, uint32_t cb)
{
    return vboxHwBufferWrite (ppdev, pv, cb);
}

BOOL vboxOrderSupported (PPDEV ppdev, unsigned code)
{
    VBVABUFFER *pVBVA = ppdev->pVBVA;

    if (!pVBVA)
    {
        return FALSE;
    }

    if (pVBVA->u32SupportedOrders & (1 << code))
    {
        return TRUE;
    }

    return FALSE;
}

void VBoxProcessDisplayInfo (PPDEV ppdev)
{
    if (ppdev->bHGSMISupported)
    {
        /* Issue the screen info command. */
        void *p = HGSMIHeapAlloc (&ppdev->hgsmiDisplayHeap,
                                  sizeof (VBVAINFOSCREEN),
                                  HGSMI_CH_VBVA,
                                  VBVA_INFO_SCREEN);
        if (!p)
        {
            DISPDBG((0, "VBoxDISP::vboxHwBufferFlush: HGSMIHeapAlloc failed\n"));
        }
        else
        {
            VBVAINFOSCREEN *pScreen = (VBVAINFOSCREEN *)p;

            pScreen->u32ViewIndex    = ppdev->iDevice;
            pScreen->i32OriginX      = ppdev->ptlDevOrg.x;
            pScreen->i32OriginY      = ppdev->ptlDevOrg.y;
            pScreen->u32LineSize     = ppdev->lDeltaScreen > 0?ppdev->lDeltaScreen: -ppdev->lDeltaScreen;
            pScreen->u32Width        = ppdev->cxScreen;
            pScreen->u32Height       = ppdev->cyScreen;
            pScreen->u16BitsPerPixel = (uint16_t)ppdev->ulBitCount;
            pScreen->u16Flags        = VBVA_SCREEN_F_ACTIVE;

            vboxHGSMIBufferSubmit (ppdev, p);

            HGSMIHeapFree (&ppdev->hgsmiDisplayHeap, p);
        }
    }

    return;
}

# ifdef VBOX_WITH_VIDEOHWACCEL

VBOXVHWACMD* vboxVHWACommandCreate (PPDEV ppdev, VBOXVHWACMD_TYPE enmCmd, VBOXVHWACMD_LENGTH cbCmd)
{
    VBOXVHWACMD* pHdr = (VBOXVHWACMD*)HGSMIHeapAlloc (&ppdev->hgsmiDisplayHeap,
                              cbCmd + VBOXVHWACMD_HEADSIZE(),
                              HGSMI_CH_VBVA,
                              VBVA_VHWA_CMD);
    if (!pHdr)
    {
        DISPDBG((0, "VBoxDISP::vboxVHWACommandCreate: HGSMIHeapAlloc failed\n"));
    }
    else
    {
        memset(pHdr, 0, sizeof(VBOXVHWACMD));
        pHdr->iDisplay = ppdev->iDevice;
        pHdr->rc = VERR_GENERAL_FAILURE;
        pHdr->enmCmd = enmCmd;
    }

    /* temporary hack */
    vboxVHWACommandCheckHostCmds(ppdev);

    return pHdr;
}

void vboxVHWACommandFree (PPDEV ppdev, VBOXVHWACMD* pCmd)
{
    HGSMIHeapFree (&ppdev->hgsmiDisplayHeap, pCmd);
}

static DECLCALLBACK(void) vboxVHWACommandCompletionCallbackEvent(PPDEV ppdev, VBOXVHWACMD * pCmd, void * pContext)
{
    PEVENT pEvent = (PEVENT)pContext;
    LONG oldState = EngSetEvent(pEvent);
    Assert(!oldState);
}

static int vboxVHWAHanldeVHWACmdCompletion(PPDEV ppdev, VBVAHOSTCMD * pHostCmd)
{
    VBVAHOSTCMDVHWACMDCOMPLETE * pComplete = VBVAHOSTCMD_BODY(pHostCmd, VBVAHOSTCMDVHWACMDCOMPLETE);
    VBOXVHWACMD* pComplCmd = (VBOXVHWACMD*)HGSMIOffsetToPointer (&ppdev->hgsmiDisplayHeap.area, pComplete->offCmd);
    PFNVBOXVHWACMDCOMPLETION pfnCompletion = (PFNVBOXVHWACMDCOMPLETION)pComplCmd->GuestVBVAReserved1;
    void * pContext = (void *)pComplCmd->GuestVBVAReserved2;

    pfnCompletion(ppdev, pComplCmd, pContext);

    vboxVBVAHostCommandComplete(ppdev, pHostCmd);

    return 0;
}

static void vboxVBVAHostCommandHanlder(PPDEV ppdev, VBVAHOSTCMD * pCmd)
{
    int rc = VINF_SUCCESS;
    switch(pCmd->customOpCode)
    {
# ifdef VBOX_WITH_VIDEOHWACCEL
        case VBVAHG_DCUSTOM_VHWA_CMDCOMPLETE:
        {
            vboxVHWAHanldeVHWACmdCompletion(ppdev, pCmd);
            break;
        }
# endif
        default:
        {
            vboxVBVAHostCommandComplete(ppdev, pCmd);
        }
    }
}

void vboxVHWACommandCheckHostCmds(PPDEV ppdev)
{
    VBVAHOSTCMD * pCmd;
    int rc = ppdev->pfnHGSMIRequestCommands(ppdev->hMpHGSMI, HGSMI_CH_VBVA, &pCmd);
    if(RT_SUCCESS(rc))
    {
        for(; pCmd; pCmd = pCmd->u.pNext)
        {
            vboxVBVAHostCommandHanlder(ppdev, pCmd);
        }
    }
}

void vboxVHWACommandSubmitAsynchByEvent (PPDEV ppdev, VBOXVHWACMD* pCmd, PEVENT pEvent)
{
//    Assert(0);
    pCmd->GuestVBVAReserved1 = (uintptr_t)pEvent;
    pCmd->GuestVBVAReserved2 = 0;

    /* complete it asynchronously by setting event */
    pCmd->Flags = VBOXVHWACMD_FLAG_ASYNCH_EVENT;
    vboxHGSMIBufferSubmit (ppdev, pCmd);

    if(!(ASMAtomicReadU32((volatile uint32_t *)&pCmd->Flags)  & VBOXVHWACMD_FLAG_ASYNCH))
    {
        /* the command is completed */
        EngSetEvent(pEvent);
    }
}

BOOL vboxVHWACommandSubmit (PPDEV ppdev, VBOXVHWACMD* pCmd)
{
    PEVENT pEvent;
    BOOL brc = EngCreateEvent(&pEvent);
    Assert(brc);

    if(brc)
    {
        vboxVHWACommandSubmitAsynchByEvent (ppdev, pCmd, pEvent);

        brc = EngWaitForSingleObject(pEvent,
                NULL /*IN PLARGE_INTEGER  pTimeOut*/
                );
        Assert(brc);
        if(brc)
        {
            EngDeleteEvent(pEvent);
        }
    }
    return brc;
}

/* do not wait for completion */
void vboxVHWACommandSubmitAsynch (PPDEV ppdev, VBOXVHWACMD* pCmd, PFNVBOXVHWACMDCOMPLETION pfnCompletion, void * pContext)
{
//    Assert(0);
    pCmd->GuestVBVAReserved1 = (uintptr_t)pfnCompletion;
    pCmd->GuestVBVAReserved2 = (uintptr_t)pContext;

    pCmd->Flags = 0;
    vboxHGSMIBufferSubmit (ppdev, pCmd);

    if(!(pCmd->Flags  & VBOXVHWACMD_FLAG_ASYNCH))
    {
        /* the command is completed */
        pfnCompletion(ppdev, pCmd, pContext);
    }
}

void vboxVHWAFreeHostInfo1(PPDEV ppdev, VBOXVHWACMD_QUERYINFO1* pInfo)
{
    VBOXVHWACMD* pCmd = VBOXVHWACMD_HEAD(pInfo);
    vboxVHWACommandFree (ppdev, pCmd);
}

void vboxVHWAFreeHostInfo2(PPDEV ppdev, VBOXVHWACMD_QUERYINFO2* pInfo)
{
    VBOXVHWACMD* pCmd = VBOXVHWACMD_HEAD(pInfo);
    vboxVHWACommandFree (ppdev, pCmd);
}

VBOXVHWACMD_QUERYINFO1* vboxVHWAQueryHostInfo1(PPDEV ppdev)
{
    VBOXVHWACMD* pCmd = vboxVHWACommandCreate (ppdev, VBOXVHWACMD_TYPE_QUERY_INFO1, sizeof(VBOXVHWACMD_QUERYINFO1));
    if (!pCmd)
    {
        DISPDBG((0, "VBoxDISP::vboxVHWAQueryHostInfo1: vboxVHWACommandCreate failed\n"));
        return NULL;
    }

    if(vboxVHWACommandSubmit (ppdev, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            return VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO1);
        }
    }

    vboxVHWACommandFree (ppdev, pCmd);
    return NULL;
}

VBOXVHWACMD_QUERYINFO2* vboxVHWAQueryHostInfo2(PPDEV ppdev, uint32_t numFourCC)
{
    VBOXVHWACMD* pCmd = vboxVHWACommandCreate (ppdev, VBOXVHWACMD_TYPE_QUERY_INFO2, sizeof(VBOXVHWACMD_QUERYINFO2));
    VBOXVHWACMD_QUERYINFO2 *pInfo2;
    if (!pCmd)
    {
        DISPDBG((0, "VBoxDISP::vboxVHWAQueryHostInfo1: vboxVHWACommandCreate failed\n"));
        return NULL;
    }

    pInfo2 = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO2);
    pInfo2->numFourCC = numFourCC;

    if(vboxVHWACommandSubmit (ppdev, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            if(pInfo2->numFourCC == numFourCC)
            {
                return pInfo2;
            }
        }
    }

    vboxVHWACommandFree (ppdev, pCmd);
    return NULL;
}

int vboxVHWAInitHostInfo1(PPDEV ppdev)
{
    VBOXVHWACMD_QUERYINFO1* pInfo = vboxVHWAQueryHostInfo1(ppdev);
    if(!pInfo)
    {
        ppdev->vhwaInfo.bVHWAEnabled = false;
        return VERR_OUT_OF_RESOURCES;
    }

    ppdev->vhwaInfo.caps = pInfo->caps;
    ppdev->vhwaInfo.colorKeyCaps = pInfo->colorKeyCaps;
    ppdev->vhwaInfo.stretchCaps = pInfo->stretchCaps;
    ppdev->vhwaInfo.surfaceCaps = pInfo->surfaceCaps;
    ppdev->vhwaInfo.numOverlays = pInfo->numOverlays;
    ppdev->vhwaInfo.numFourCC = pInfo->numFourCC;
    ppdev->vhwaInfo.bVHWAEnabled = (pInfo->cfgFlags & VBOXVHWA_CFG_ENABLED);

    vboxVHWAFreeHostInfo1(ppdev, pInfo);
    return VINF_SUCCESS;
}

int vboxVHWAInitHostInfo2(PPDEV ppdev, DWORD *pFourCC)
{
    VBOXVHWACMD_QUERYINFO2* pInfo = vboxVHWAQueryHostInfo2(ppdev, ppdev->vhwaInfo.numFourCC);
    int rc = VINF_SUCCESS;

    if(pInfo)
        return VERR_OUT_OF_RESOURCES;

    if(ppdev->vhwaInfo.numFourCC)
    {
        memcpy(pFourCC, pInfo->FourCC, ppdev->vhwaInfo.numFourCC * sizeof(pFourCC[0]));
    }
    else
    {
        Assert(0);
        rc = VERR_GENERAL_FAILURE;
    }

    vboxVHWAFreeHostInfo2(ppdev, pInfo);

    return rc;
}

int vboxVHWAEnable(PPDEV ppdev)
{
    int rc = VERR_GENERAL_FAILURE;
    VBOXVHWACMD* pCmd = vboxVHWACommandCreate (ppdev, VBOXVHWACMD_TYPE_ENABLE, 0);
    if (!pCmd)
    {
        DISPDBG((0, "VBoxDISP::vboxVHWAQueryHostInfo1: vboxVHWACommandCreate failed\n"));
        return NULL;
    }

    if(vboxVHWACommandSubmit (ppdev, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            rc = VINF_SUCCESS;
        }
    }

    vboxVHWACommandFree (ppdev, pCmd);
    return rc;
}

int vboxVHWADisable(PPDEV ppdev)
{
    int rc = VERR_GENERAL_FAILURE;
    VBOXVHWACMD* pCmd = vboxVHWACommandCreate (ppdev, VBOXVHWACMD_TYPE_DISABLE, 0);
    if (!pCmd)
    {
        DISPDBG((0, "VBoxDISP::vboxVHWAQueryHostInfo1: vboxVHWACommandCreate failed\n"));
        return NULL;
    }

    if(vboxVHWACommandSubmit (ppdev, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            rc = VINF_SUCCESS;
        }
    }

    vboxVHWACommandFree (ppdev, pCmd);

    vboxVHWACommandCheckHostCmds(ppdev);

    return rc;
}

# endif

void vboxVBVAHostCommandComplete(PPDEV ppdev, VBVAHOSTCMD * pCmd)
{
    ppdev->pfnHGSMICommandComplete(ppdev->hMpHGSMI, pCmd);
}

#endif /* VBOX_WITH_HGSMI */
