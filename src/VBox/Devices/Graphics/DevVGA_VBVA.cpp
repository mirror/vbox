/** @file
 * VirtualBox Video Acceleration (VBVA).
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#define LOG_GROUP LOG_GROUP_DEV_VGA
#include <VBox/pdmifs.h>
#include <VBox/pdmdev.h>
#include <VBox/pgm.h>
#include <VBox/ssm.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxVideo.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/param.h>
#ifdef VBOX_WITH_VIDEOHWACCEL
#include <iprt/semaphore.h>
#endif

/** The default amount of VRAM. */
#define VGA_VRAM_DEFAULT    (_4M)
/** The maximum amount of VRAM. */
#define VGA_VRAM_MAX        (128 * _1M)
/** The minimum amount of VRAM. */
#define VGA_VRAM_MIN        (_1M)

#include "DevVGA.h"

/* A very detailed logging. */
#if 0 // def DEBUG_sunlover
#define LOGVBVABUFFER(a) LogFlow(a)
#else
#define LOGVBVABUFFER(a) do {} while(0)
#endif

typedef struct _VBVAPARTIALRECORD
{
    uint8_t *pu8;
    uint32_t cb;
} VBVAPARTIALRECORD;

typedef struct _VBVAVIEW
{
    VBVAINFOVIEW    view;
    VBVAINFOSCREEN  screen;
    VBVABUFFER     *pVBVA;
    uint32_t        u32VBVAOffset;
    VBVAPARTIALRECORD partialRecord;
} VBVAVIEW;

/* @todo saved state: save and restore VBVACONTEXT */
typedef struct _VBVACONTEXT
{
    uint32_t cViews;
    VBVAVIEW aViews[64 /* @todo SchemaDefs::MaxGuestMonitors*/];
} VBVACONTEXT;

/* Copies 'cb' bytes from the VBVA ring buffer to the 'pu8Dst'.
 * Used for partial records or for records which cross the ring boundary.
 */
static void vbvaFetchBytes (VBVABUFFER *pVBVA, uint8_t *pu8Dst, uint32_t cb)
{
    /* @todo replace the 'if' with an assert. The caller must ensure this condition. */
    if (cb >= pVBVA->cbData)
    {
        AssertMsgFailed (("cb = 0x%08X, ring buffer size 0x%08X", cb, pVBVA->cbData));
        return;
    }

    const uint32_t u32BytesTillBoundary = pVBVA->cbData - pVBVA->off32Data;
    const uint8_t  *src                 = &pVBVA->au8Data[pVBVA->off32Data];
    const int32_t i32Diff               = cb - u32BytesTillBoundary;

    if (i32Diff <= 0)
    {
        /* Chunk will not cross buffer boundary. */
        memcpy (pu8Dst, src, cb);
    }
    else
    {
        /* Chunk crosses buffer boundary. */
        memcpy (pu8Dst, src, u32BytesTillBoundary);
        memcpy (pu8Dst + u32BytesTillBoundary, &pVBVA->au8Data[0], i32Diff);
    }

    /* Advance data offset. */
    pVBVA->off32Data = (pVBVA->off32Data + cb) % pVBVA->cbData;

    return;
}


static bool vbvaPartialRead (VBVAPARTIALRECORD *pPartialRecord, uint32_t cbRecord, VBVABUFFER *pVBVA)
{
    uint8_t *pu8New;

    LOGVBVABUFFER(("vbvaPartialRead: p = %p, cb = %d, cbRecord 0x%08X\n",
                   pPartialRecord->pu8, pPartialRecord->cb, cbRecord));

    if (pPartialRecord->pu8)
    {
        Assert (pPartialRecord->cb);
        pu8New = (uint8_t *)RTMemRealloc (pPartialRecord->pu8, cbRecord);
    }
    else
    {
        Assert (!pPartialRecord->cb);
        pu8New = (uint8_t *)RTMemAlloc (cbRecord);
    }

    if (!pu8New)
    {
        /* Memory allocation failed, fail the function. */
        Log(("vbvaPartialRead: failed to (re)alocate memory for partial record!!! cbRecord 0x%08X\n",
             cbRecord));

        if (pPartialRecord->pu8)
        {
            RTMemFree (pPartialRecord->pu8);
        }

        pPartialRecord->pu8 = NULL;
        pPartialRecord->cb = 0;

        return false;
    }

    /* Fetch data from the ring buffer. */
    vbvaFetchBytes (pVBVA, pu8New + pPartialRecord->cb, cbRecord - pPartialRecord->cb);

    pPartialRecord->pu8 = pu8New;
    pPartialRecord->cb = cbRecord;

    return true;
}

/* For contiguous chunks just return the address in the buffer.
 * For crossing boundary - allocate a buffer from heap.
 */
static bool vbvaFetchCmd (VBVAPARTIALRECORD *pPartialRecord, VBVABUFFER *pVBVA, VBVACMDHDR **ppHdr, uint32_t *pcbCmd)
{
    uint32_t indexRecordFirst = pVBVA->indexRecordFirst;
    uint32_t indexRecordFree = pVBVA->indexRecordFree;

    LOGVBVABUFFER(("first = %d, free = %d\n",
                   indexRecordFirst, indexRecordFree));

    if (indexRecordFirst == indexRecordFree)
    {
        /* No records to process. Return without assigning output variables. */
        return true;
    }

    VBVARECORD *pRecord = &pVBVA->aRecords[indexRecordFirst];

    LOGVBVABUFFER(("cbRecord = 0x%08X\n", pRecord->cbRecord));

    uint32_t cbRecord = pRecord->cbRecord & ~VBVA_F_RECORD_PARTIAL;

    if (pPartialRecord->cb)
    {
        /* There is a partial read in process. Continue with it. */
        Assert (pPartialRecord->pu8);

        LOGVBVABUFFER(("continue partial record cb = %d cbRecord 0x%08X, first = %d, free = %d\n",
                      pPartialRecord->cb, pRecord->cbRecord, indexRecordFirst, indexRecordFree));

        if (cbRecord > pPartialRecord->cb)
        {
            /* New data has been added to the record. */
            if (!vbvaPartialRead (pPartialRecord, cbRecord, pVBVA))
            {
                return false;
            }
        }

        if (!(pRecord->cbRecord & VBVA_F_RECORD_PARTIAL))
        {
            /* The record is completed by guest. Return it to the caller. */
            *ppHdr = (VBVACMDHDR *)pPartialRecord->pu8;
            *pcbCmd = pPartialRecord->cb;

            pPartialRecord->pu8 = NULL;
            pPartialRecord->cb = 0;

            /* Advance the record index. */
            pVBVA->indexRecordFirst = (indexRecordFirst + 1) % RT_ELEMENTS(pVBVA->aRecords);

            LOGVBVABUFFER(("partial done ok, data = %d, free = %d\n",
                          pVBVA->off32Data, pVBVA->off32Free));
        }

        return true;
    }

    /* A new record need to be processed. */
    if (pRecord->cbRecord & VBVA_F_RECORD_PARTIAL)
    {
        /* Current record is being written by guest. '=' is important here,
         * because the guest will do a FLUSH at this condition.
         * This partual record is too large for the ring buffer and must
         * be accumulated in an allocated buffer.
         */
        if (cbRecord >= pVBVA->cbData - pVBVA->cbPartialWriteThreshold)
        {
            /* Partial read must be started. */
            if (!vbvaPartialRead (pPartialRecord, cbRecord, pVBVA))
            {
                return false;
            }

            LOGVBVABUFFER(("started partial record cb = 0x%08X cbRecord 0x%08X, first = %d, free = %d\n",
                          pPartialRecord->cb, pRecord->cbRecord, indexRecordFirst, indexRecordFree));
        }

        return true;
    }

    /* Current record is complete. If it is not empty, process it. */
    if (cbRecord)
    {
        /* The size of largest contiguos chunk in the ring biffer. */
        uint32_t u32BytesTillBoundary = pVBVA->cbData - pVBVA->off32Data;

        /* The pointer to data in the ring buffer. */
        uint8_t *src = &pVBVA->au8Data[pVBVA->off32Data];

        /* Fetch or point the data. */
        if (u32BytesTillBoundary >= cbRecord)
        {
            /* The command does not cross buffer boundary. Return address in the buffer. */
            *ppHdr = (VBVACMDHDR *)src;

            /* Advance data offset. */
            pVBVA->off32Data = (pVBVA->off32Data + cbRecord) % pVBVA->cbData;
        }
        else
        {
            /* The command crosses buffer boundary. Rare case, so not optimized. */
            uint8_t *dst = (uint8_t *)RTMemAlloc (cbRecord);

            if (!dst)
            {
                LogFlowFunc (("could not allocate %d bytes from heap!!!\n", cbRecord));
                pVBVA->off32Data = (pVBVA->off32Data + cbRecord) % pVBVA->cbData;
                return false;
            }

            vbvaFetchBytes (pVBVA, dst, cbRecord);

            *ppHdr = (VBVACMDHDR *)dst;

            LOGVBVABUFFER(("Allocated from heap %p\n", dst));
        }
    }

    *pcbCmd = cbRecord;

    /* Advance the record index. */
    pVBVA->indexRecordFirst = (indexRecordFirst + 1) % RT_ELEMENTS(pVBVA->aRecords);

    LOGVBVABUFFER(("done ok, data = %d, free = %d\n",
                  pVBVA->off32Data, pVBVA->off32Free));

    return true;
}

static void vbvaReleaseCmd (VBVAPARTIALRECORD *pPartialRecord, VBVABUFFER *pVBVA, VBVACMDHDR *pHdr, uint32_t cbCmd)
{
    uint8_t *au8RingBuffer = &pVBVA->au8Data[0];

    if (   (uint8_t *)pHdr >= au8RingBuffer
        && (uint8_t *)pHdr < &au8RingBuffer[pVBVA->cbData])
    {
        /* The pointer is inside ring buffer. Must be continuous chunk. */
        Assert (pVBVA->cbData - ((uint8_t *)pHdr - au8RingBuffer) >= cbCmd);

        /* Do nothing. */

        Assert (!pPartialRecord->pu8 && pPartialRecord->cb == 0);
    }
    else
    {
        /* The pointer is outside. It is then an allocated copy. */
        LOGVBVABUFFER(("Free heap %p\n", pHdr));

        if ((uint8_t *)pHdr == pPartialRecord->pu8)
        {
            pPartialRecord->pu8 = NULL;
            pPartialRecord->cb = 0;
        }
        else
        {
            Assert (!pPartialRecord->pu8 && pPartialRecord->cb == 0);
        }

        RTMemFree (pHdr);
    }

    return;
}

static int vbvaFlushProcess (unsigned uScreenId, PVGASTATE pVGAState, VBVAPARTIALRECORD *pPartialRecord, VBVABUFFER *pVBVA)
{
    LOGVBVABUFFER(("uScreenId %d, indexRecordFirst = %d, indexRecordFree = %d, off32Data = %d, off32Free = %d\n",
                  uScreenId, pVBVA->indexRecordFirst, pVBVA->indexRecordFree, pVBVA->off32Data, pVBVA->off32Free));
    struct {
        /* The rectangle that includes all dirty rectangles. */
        int32_t xLeft;
        int32_t xRight;
        int32_t yTop;
        int32_t yBottom;
    } dirtyRect;
    memset(&dirtyRect, 0, sizeof(dirtyRect));

    bool fUpdate = false; /* Whether there were any updates. */

    for (;;)
    {
        VBVACMDHDR *phdr = NULL;
        uint32_t cbCmd = ~0;

        /* Fetch the command data. */
        if (!vbvaFetchCmd (pPartialRecord, pVBVA, &phdr, &cbCmd))
        {
            LogFunc(("unable to fetch command. off32Data = %d, off32Free = %d!!!\n",
                  pVBVA->off32Data, pVBVA->off32Free));

            /* @todo old code disabled VBVA processing here. */
            return VERR_NOT_SUPPORTED;
        }

        if (cbCmd == uint32_t(~0))
        {
            /* No more commands yet in the queue. */
            break;
        }

        if (cbCmd != 0)
        {
            if (!fUpdate)
            {
                pVGAState->pDrv->pfnVBVAUpdateBegin (pVGAState->pDrv, uScreenId);
                fUpdate = true;
            }

            /* Updates the rectangle and sends the command to the VRDP server. */
            pVGAState->pDrv->pfnVBVAUpdateProcess (pVGAState->pDrv, uScreenId, phdr, cbCmd);

            int32_t xRight  = phdr->x + phdr->w;
            int32_t yBottom = phdr->y + phdr->h;

            /* These are global coords, relative to the primary screen. */

            LOGVBVABUFFER(("cbCmd = %d, x=%d, y=%d, w=%d, h=%d\n",
                           cbCmd, phdr->x, phdr->y, phdr->w, phdr->h));

            /* Collect all rects into one. */
            if (dirtyRect.xRight == 0)
            {
                /* This is the first rectangle to be added. */
                dirtyRect.xLeft   = phdr->x;
                dirtyRect.yTop    = phdr->y;
                dirtyRect.xRight  = xRight;
                dirtyRect.yBottom = yBottom;
            }
            else
            {
                /* Adjust region coordinates. */
                if (dirtyRect.xLeft > phdr->x)
                {
                    dirtyRect.xLeft = phdr->x;
                }

                if (dirtyRect.yTop > phdr->y)
                {
                    dirtyRect.yTop = phdr->y;
                }

                if (dirtyRect.xRight < xRight)
                {
                    dirtyRect.xRight = xRight;
                }

                if (dirtyRect.yBottom < yBottom)
                {
                    dirtyRect.yBottom = yBottom;
                }
            }
        }

        vbvaReleaseCmd (pPartialRecord, pVBVA, phdr, cbCmd);
    }

    if (fUpdate)
    {
        if(dirtyRect.xRight)
        {
            pVGAState->pDrv->pfnVBVAUpdateEnd (pVGAState->pDrv, uScreenId, dirtyRect.xLeft, dirtyRect.yTop,
                                               dirtyRect.xRight - dirtyRect.xLeft, dirtyRect.yBottom - dirtyRect.yTop);
        }
        else
        {
            pVGAState->pDrv->pfnVBVAUpdateEnd (pVGAState->pDrv, uScreenId, 0, 0, 0, 0);
        }
    }

    return VINF_SUCCESS;
}

static int vbvaFlush (PVGASTATE pVGAState, VBVACONTEXT *pCtx)
{
    unsigned uScreenId;

    for (uScreenId = 0; uScreenId < pCtx->cViews; uScreenId++)
    {
        VBVAPARTIALRECORD *pPartialRecord = &pCtx->aViews[uScreenId].partialRecord;
        VBVABUFFER *pVBVA = pCtx->aViews[uScreenId].pVBVA;

        if (pVBVA)
        {
            vbvaFlushProcess (uScreenId, pVGAState, pPartialRecord, pVBVA);
        }
    }

    /* @todo rc */
    return VINF_SUCCESS;
}

static int vbvaResize (PVGASTATE pVGAState, VBVAVIEW *pView, const VBVAINFOSCREEN *pNewScreen)
{
    /* Verify pNewScreen. */
    /* @todo */

    /* Apply these changes. */
    pView->screen = *pNewScreen;

    uint8_t *pu8VRAM = pVGAState->vram_ptrR3 + pView->view.u32ViewOffset;

    int rc = pVGAState->pDrv->pfnVBVAResize (pVGAState->pDrv, &pView->view, &pView->screen, pu8VRAM);

    /* @todo process VINF_VGA_RESIZE_IN_PROGRESS? */

    return rc;
}

static int vbvaEnable (unsigned uScreenId, PVGASTATE pVGAState, VBVACONTEXT *pCtx, VBVABUFFER *pVBVA, uint32_t u32Offset)
{
    /* @todo old code did a UpdateDisplayAll at this place. */

    int rc;

    if (pVGAState->pDrv->pfnVBVAEnable)
    {
        rc = pVGAState->pDrv->pfnVBVAEnable (pVGAState->pDrv, uScreenId);
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS (rc))
    {
        /* Setup flags. */
        pVBVA->u32HostEvents = VBVA_F_MODE_ENABLED |
                               VBVA_F_MODE_VRDP |
                               VBOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET;

        pVBVA->u32SupportedOrders = ~0;

        pCtx->aViews[uScreenId].partialRecord.pu8 = NULL;
        pCtx->aViews[uScreenId].partialRecord.cb = 0;

        pCtx->aViews[uScreenId].pVBVA = pVBVA;
        pCtx->aViews[uScreenId].u32VBVAOffset = u32Offset;
    }

    return rc;
}

static int vbvaDisable (unsigned uScreenId, PVGASTATE pVGAState, VBVACONTEXT *pCtx)
{
    /* Process any pending orders and empty the VBVA ring buffer. */
    vbvaFlush (pVGAState, pCtx);

    VBVAVIEW *pView = &pCtx->aViews[uScreenId];

    if (pView->pVBVA)
    {
        pView->pVBVA->u32HostEvents = 0;
        pView->pVBVA->u32SupportedOrders = 0;

        pView->partialRecord.pu8 = NULL;
        pView->partialRecord.cb = 0;

        pView->pVBVA = NULL;
        pView->u32VBVAOffset = HGSMIOFFSET_VOID;
    }

    pVGAState->pDrv->pfnVBVADisable (pVGAState->pDrv, uScreenId);
    return VINF_SUCCESS;
}

static int vbvaMousePointerShape (PVGASTATE pVGAState, VBVACONTEXT *pCtx, const VBVAMOUSEPOINTERSHAPE *pShape, HGSMISIZE cbShape)
{
    bool fVisible = (pShape->fu32Flags & VBOX_MOUSE_POINTER_VISIBLE) != 0;
    bool fAlpha =   (pShape->fu32Flags & VBOX_MOUSE_POINTER_ALPHA) != 0;
    bool fShape =   (pShape->fu32Flags & VBOX_MOUSE_POINTER_SHAPE) != 0;

    HGSMISIZE cbPointerData = 0;

    if (fShape)
    {
         cbPointerData = ((((pShape->u32Width + 7) / 8) * pShape->u32Height + 3) & ~3)
                         + pShape->u32Width * 4 * pShape->u32Height;
    }

    if (cbPointerData > cbShape - RT_OFFSETOF(VBVAMOUSEPOINTERSHAPE, au8Data))
    {
        Log(("vbvaMousePointerShape: calculated pointer data size is too big (%d bytes, limit %d)\n",
              cbPointerData, cbShape - RT_OFFSETOF(VBVAMOUSEPOINTERSHAPE, au8Data)));
        return VERR_INVALID_PARAMETER;
    }

    if (pVGAState->pDrv->pfnVBVAMousePointerShape == NULL)
    {
        return VERR_NOT_SUPPORTED;
    }

    int rc;

    if (fShape)
    {
        rc = pVGAState->pDrv->pfnVBVAMousePointerShape (pVGAState->pDrv,
                                                        fVisible,
                                                        fAlpha,
                                                        pShape->u32HotX, pShape->u32HotY,
                                                        pShape->u32Width, pShape->u32Height,
                                                        &pShape->au8Data[0]);
    }
    else
    {
        rc = pVGAState->pDrv->pfnVBVAMousePointerShape (pVGAState->pDrv,
                                                        fVisible,
                                                        false,
                                                        0, 0,
                                                        0, 0,
                                                        NULL);
    }

    return rc;
}

static unsigned vbvaViewFromOffset (PHGSMIINSTANCE pIns, VBVACONTEXT *pCtx, const void *pvBuffer)
{
    /* Check which view contains the buffer. */
    HGSMIOFFSET offBuffer = HGSMIPointerToOffsetHost (pIns, pvBuffer);

    if (offBuffer != HGSMIOFFSET_VOID)
    {
        unsigned uScreenId;

        for (uScreenId = 0; uScreenId < pCtx->cViews; uScreenId++)
        {
            VBVAINFOVIEW *pView = &pCtx->aViews[uScreenId].view;

            if (   pView->u32ViewSize > 0
                && pView->u32ViewOffset <= offBuffer
                && offBuffer <= pView->u32ViewOffset + pView->u32ViewSize - 1)
            {
                return pView->u32ViewIndex;
            }
        }
    }

    return ~0U;
}

#ifdef DEBUG_sunlover
static void dumpctx(const VBVACONTEXT *pCtx)
{
    Log(("VBVACONTEXT dump: cViews %d\n", pCtx->cViews));

    uint32_t iView;
    for (iView = 0; iView < pCtx->cViews; iView++)
    {
        const VBVAVIEW *pView = &pCtx->aViews[iView];

        Log(("                  view %d o 0x%x s 0x%x m 0x%x\n",
              pView->view.u32ViewIndex,
              pView->view.u32ViewOffset,
              pView->view.u32ViewSize,
              pView->view.u32MaxScreenSize));

        Log(("                  screen %d @%d,%d s 0x%x l 0x%x %dx%d bpp %d f 0x%x\n",
              pView->screen.u32ViewIndex,
              pView->screen.i32OriginX,
              pView->screen.i32OriginY,
              pView->screen.u32StartOffset,
              pView->screen.u32LineSize,
              pView->screen.u32Width,
              pView->screen.u32Height,
              pView->screen.u16BitsPerPixel,
              pView->screen.u16Flags));

        Log(("                  VBVA o 0x%x p %p\n",
              pView->u32VBVAOffset,
              pView->pVBVA));

        Log(("                  PR cb 0x%x p %p\n",
              pView->partialRecord.cb,
              pView->partialRecord.pu8));
    }
}
#endif /* DEBUG_sunlover */

int vboxVBVASaveStateExec (PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVGASTATE pVGAState = PDMINS_2_DATA(pDevIns, PVGASTATE);
    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;

    int rc = HGSMIHostSaveStateExec (pIns, pSSM);
    if (RT_SUCCESS(rc))
    {
        /* Save VBVACONTEXT. */
        VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pIns);

        if (!pCtx)
        {
            AssertFailed();

            /* Still write a valid value to the SSM. */
            rc = SSMR3PutU32 (pSSM, 0);
            AssertRCReturn(rc, rc);
        }
        else
        {
#ifdef DEBUG_sunlover
            dumpctx(pCtx);
#endif

            rc = SSMR3PutU32 (pSSM, pCtx->cViews);
            AssertRCReturn(rc, rc);

            uint32_t iView;
            for (iView = 0; iView < pCtx->cViews; iView++)
            {
                VBVAVIEW *pView = &pCtx->aViews[iView];

                rc = SSMR3PutU32 (pSSM, pView->view.u32ViewIndex);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->view.u32ViewOffset);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->view.u32ViewSize);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->view.u32MaxScreenSize);
                AssertRCReturn(rc, rc);

                rc = SSMR3PutU32 (pSSM, pView->screen.u32ViewIndex);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutS32 (pSSM, pView->screen.i32OriginX);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutS32 (pSSM, pView->screen.i32OriginY);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->screen.u32StartOffset);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->screen.u32LineSize);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->screen.u32Width);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->screen.u32Height);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU16 (pSSM, pView->screen.u16BitsPerPixel);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU16 (pSSM, pView->screen.u16Flags);
                AssertRCReturn(rc, rc);

                rc = SSMR3PutU32 (pSSM, pView->pVBVA? pView->u32VBVAOffset: HGSMIOFFSET_VOID);
                AssertRCReturn(rc, rc);

                rc = SSMR3PutU32 (pSSM, pView->partialRecord.cb);
                AssertRCReturn(rc, rc);

                if (pView->partialRecord.cb > 0)
                {
                    rc = SSMR3PutMem (pSSM, pView->partialRecord.pu8, pView->partialRecord.cb);
                    AssertRCReturn(rc, rc);
                }
            }
        }
    }

    return rc;
}

int vboxVBVALoadStateExec (PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t u32Version)
{
    if (u32Version < 3)
    {
        /* Nothing was saved. */
        return VINF_SUCCESS;
    }

    PVGASTATE pVGAState = PDMINS_2_DATA(pDevIns, PVGASTATE);
    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
    int rc = HGSMIHostLoadStateExec (pIns, pSSM, u32Version);
    if (RT_SUCCESS(rc))
    {
        /* Load VBVACONTEXT. */
        VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pIns);

        if (!pCtx)
        {
            /* This should not happen. */
            AssertFailed();
            rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            uint32_t cViews = 0;
            rc = SSMR3GetU32 (pSSM, &cViews);
            AssertRCReturn(rc, rc);

            uint32_t iView;
            for (iView = 0; iView < cViews; iView++)
            {
                VBVAVIEW *pView = &pCtx->aViews[iView];

                rc = SSMR3GetU32 (pSSM, &pView->view.u32ViewIndex);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->view.u32ViewOffset);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->view.u32ViewSize);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->view.u32MaxScreenSize);
                AssertRCReturn(rc, rc);

                rc = SSMR3GetU32 (pSSM, &pView->screen.u32ViewIndex);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetS32 (pSSM, &pView->screen.i32OriginX);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetS32 (pSSM, &pView->screen.i32OriginY);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->screen.u32StartOffset);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->screen.u32LineSize);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->screen.u32Width);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->screen.u32Height);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU16 (pSSM, &pView->screen.u16BitsPerPixel);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU16 (pSSM, &pView->screen.u16Flags);
                AssertRCReturn(rc, rc);

                rc = SSMR3GetU32 (pSSM, &pView->u32VBVAOffset);
                AssertRCReturn(rc, rc);

                rc = SSMR3GetU32 (pSSM, &pView->partialRecord.cb);
                AssertRCReturn(rc, rc);

                if (pView->partialRecord.cb == 0)
                {
                    pView->partialRecord.pu8 = NULL;
                }
                else
                {
                    Assert(pView->partialRecord.pu8 == NULL); /* Should be it. */

                    uint8_t *pu8 = (uint8_t *)RTMemAlloc (pView->partialRecord.cb);

                    if (!pu8)
                    {
                        return VERR_NO_MEMORY;
                    }

                    pView->partialRecord.pu8 = pu8;

                    rc = SSMR3GetMem (pSSM, pView->partialRecord.pu8, pView->partialRecord.cb);
                    AssertRCReturn(rc, rc);
                }

                if (   pView->u32VBVAOffset == HGSMIOFFSET_VOID
                    || pView->screen.u32LineSize == 0) /* Earlier broken saved states. */
                {
                    pView->pVBVA = NULL;
                }
                else
                {
                    pView->pVBVA = (VBVABUFFER *)HGSMIOffsetToPointerHost (pIns, pView->u32VBVAOffset);
                }
            }

            pCtx->cViews = iView;
            LogFlowFunc(("%d views loaded\n", pCtx->cViews));

#ifdef DEBUG_sunlover
            dumpctx(pCtx);
#endif
        }
    }

    return rc;
}

int vboxVBVALoadStateDone (PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVGASTATE pVGAState = PDMINS_2_DATA(pDevIns, PVGASTATE);
    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pVGAState->pHGSMI);

    if (pCtx)
    {
        uint32_t iView;
        for (iView = 0; iView < pCtx->cViews; iView++)
        {
            VBVAVIEW *pView = &pCtx->aViews[iView];

            if (pView->pVBVA)
            {
                vbvaEnable (iView, pVGAState, pCtx, pView->pVBVA, pView->u32VBVAOffset);
                vbvaResize (pVGAState, pView, &pView->screen);
            }
        }
    }

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_VIDEOHWACCEL
static VBOXVHWACMD* vbvaVHWAHHCommandCreate (PVGASTATE pVGAState, VBOXVHWACMD_TYPE enmCmd, VBOXVHWACMD_LENGTH cbCmd)
{
    VBOXVHWACMD* pHdr = (VBOXVHWACMD*)RTMemAlloc(cbCmd + VBOXVHWACMD_HEADSIZE());
    Assert(pHdr);
    if (pHdr)
    {
        memset(pHdr, 0, VBOXVHWACMD_HEADSIZE());
        pHdr->cRefs = 1;
        pHdr->rc = VERR_GENERAL_FAILURE;
        pHdr->enmCmd = enmCmd;
        pHdr->Flags = VBOXVHWACMD_FLAG_HH_CMD;
    }

    return pHdr;
}

DECLINLINE(void) vbvaVHWAHHCommandRelease (VBOXVHWACMD* pCmd)
{
    uint32_t cRefs = ASMAtomicDecU32(&pCmd->cRefs);
    if(!cRefs)
    {
        RTMemFree(pCmd);
    }
}

DECLINLINE(void) vbvaVHWAHHCommandRetain (VBOXVHWACMD* pCmd)
{
    ASMAtomicIncU32(&pCmd->cRefs);
}

static unsigned vbvaVHWAHandleCommand (PVGASTATE pVGAState, VBVACONTEXT *pCtx, PVBOXVHWACMD pCmd)
{
    pVGAState->pDrv->pfnVHWACommandProcess(pVGAState->pDrv, pCmd);
    return 0;
}

static int vbvaVHWAHHCommandPost(PVGASTATE pVGAState, VBOXVHWACMD* pCmd)
{
    RTSEMEVENT hComplEvent;
    int rc = RTSemEventCreate(&hComplEvent);
    AssertRC(rc);
    if(RT_SUCCESS(rc))
    {
        /* ensure the cmd is not deleted until we process it */
        vbvaVHWAHHCommandRetain (pCmd);
        pCmd->GuestVBVAReserved1 = (uint64_t)hComplEvent;
        vbvaVHWAHandleCommand(pVGAState, NULL, pCmd);
        if((ASMAtomicReadU32((volatile uint32_t *)&pCmd->Flags)  & VBOXVHWACMD_FLAG_HG_ASYNCH) != 0)
        {
            rc = RTSemEventWaitNoResume(hComplEvent, RT_INDEFINITE_WAIT);
        }
        else
        {
            /* the command is completed */
        }

        AssertRC(rc);
        if(RT_SUCCESS(rc))
        {
            RTSemEventDestroy(hComplEvent);
        }
        vbvaVHWAHHCommandRelease(pCmd);
    }
    return rc;
}

int vbvaVHWAConstruct (PVGASTATE pVGAState)
{
    VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(pVGAState, VBOXVHWACMD_TYPE_HH_CONSTRUCT, sizeof(VBOXVHWACMD_HH_CONSTRUCT));
    Assert(pCmd);
    if(pCmd)
    {
        VBOXVHWACMD_HH_CONSTRUCT * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_HH_CONSTRUCT);
        memset(pBody, 0, sizeof(VBOXVHWACMD_HH_CONSTRUCT));

        PPDMDEVINS pDevIns = pVGAState->pDevInsR3;
        PVM pVM = PDMDevHlpGetVM(pDevIns);

        pBody->pVM = pVM;

        int rc = vbvaVHWAHHCommandPost(pVGAState, pCmd);
        AssertRC(rc);
        if(RT_SUCCESS(rc))
        {
            rc = pCmd->rc;
            AssertMsg(RT_SUCCESS(rc) || rc == VERR_NOT_IMPLEMENTED, ("%Rrc\n", rc));
            if(rc == VERR_NOT_IMPLEMENTED)
            {
                /* @todo: set some flag in pVGAState indicating VHWA is not supported */
                /* VERR_NOT_IMPLEMENTED is not a failure, we just do not support it */
                rc = VINF_SUCCESS;
            }
        }

        vbvaVHWAHHCommandRelease(pCmd);

        return rc;
    }
    return VERR_OUT_OF_RESOURCES;
}

int vbvaVHWAReset (PVGASTATE pVGAState)
{
    /* ensure we have all pending cmds processed and h->g cmds disabled */
    VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(pVGAState, VBOXVHWACMD_TYPE_HH_RESET, 0);
    Assert(pCmd);
    if(pCmd)
    {
        int rc = vbvaVHWAHHCommandPost(pVGAState, pCmd);
        AssertRC(rc);
        if(RT_SUCCESS(rc))
        {
            rc = pCmd->rc;
#ifndef DEBUG_misha /** @todo the assertion below hits when booting dsl here and resetting during early boot... */
            AssertMsg(RT_SUCCESS(rc) || rc == VERR_NOT_IMPLEMENTED, ("%Rrc\n", rc));
            if (rc == VERR_NOT_IMPLEMENTED)
                rc = VINF_SUCCESS;
#else
            AssertRC(rc);
#endif
        }

        vbvaVHWAHHCommandRelease(pCmd);

        return rc;
    }
    return VERR_OUT_OF_RESOURCES;
}

/* @todo call this also on reset? */
int vbvaVHWADisable (PVGASTATE pVGAState)
{
    VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(pVGAState, VBOXVHWACMD_TYPE_DISABLE, 0);
    Assert(pCmd);
    if(pCmd)
    {
        int rc = vbvaVHWAHHCommandPost(pVGAState, pCmd);
        AssertRC(rc);
        if(RT_SUCCESS(rc))
        {
            rc = pCmd->rc;
            AssertMsg(RT_SUCCESS(rc) || rc == VERR_NOT_IMPLEMENTED, ("%Rrc\n", rc));
            if(rc == VERR_NOT_IMPLEMENTED)
            {
                rc = VINF_SUCCESS;
            }
        }

        vbvaVHWAHHCommandRelease(pCmd);

        return rc;
    }
    return VERR_OUT_OF_RESOURCES;
}

int vboxVBVASaveStatePrep (PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    /* ensure we have no pending commands */
    return vbvaVHWADisable(PDMINS_2_DATA(pDevIns, PVGASTATE));
}

#define PPDMDDISPLAYVBVACALLBACKS_2_PVGASTATE(_pcb) ( (PVGASTATE)((uint8_t *)(_pcb) - RT_OFFSETOF(VGASTATE, VBVACallbacks)) )

int vbvaVHWACommandCompleteAsynch(PPDMDDISPLAYVBVACALLBACKS pInterface, PVBOXVHWACMD pCmd)
{
    int rc;
    if((pCmd->Flags & VBOXVHWACMD_FLAG_HH_CMD) == 0)
    {
        PVGASTATE pVGAState = PPDMDDISPLAYVBVACALLBACKS_2_PVGASTATE(pInterface);
        PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
        VBVAHOSTCMD *pHostCmd;
    //    Assert(0);

        int32_t iDisplay = pCmd->iDisplay;
        Assert(pCmd->Flags & VBOXVHWACMD_FLAG_HG_ASYNCH);
        if(pCmd->Flags & VBOXVHWACMD_FLAG_GH_ASYNCH_EVENT)
        {
            rc = HGSMIHostCommandAlloc (pIns,
                                          (void**)&pHostCmd,
                                          VBVAHOSTCMD_SIZE(sizeof(VBVAHOSTCMDEVENT)),
                                          HGSMI_CH_VBVA,
                                          VBVAHG_EVENT);
            AssertRC(rc);
            if(RT_SUCCESS(rc))
            {
                memset(pHostCmd, 0 , VBVAHOSTCMD_SIZE(sizeof(VBVAHOSTCMDEVENT)));
                pHostCmd->iDstID = pCmd->iDisplay;
                pHostCmd->customOpCode = 0;
                VBVAHOSTCMDEVENT *pBody = VBVAHOSTCMD_BODY(pHostCmd, VBVAHOSTCMDEVENT);
                pBody->pEvent = pCmd->GuestVBVAReserved1;
            }
        }
        else
        {
            HGSMIOFFSET offCmd = HGSMIPointerToOffsetHost (pIns, pCmd);
            Assert(offCmd != HGSMIOFFSET_VOID);
            if(offCmd != HGSMIOFFSET_VOID)
            {
                rc = HGSMIHostCommandAlloc (pIns,
                                          (void**)&pHostCmd,
                                          VBVAHOSTCMD_SIZE(sizeof(VBVAHOSTCMDVHWACMDCOMPLETE)),
                                          HGSMI_CH_VBVA,
                                          VBVAHG_DISPLAY_CUSTOM);
                AssertRC(rc);
                if(RT_SUCCESS(rc))
                {
                    memset(pHostCmd, 0 , VBVAHOSTCMD_SIZE(sizeof(VBVAHOSTCMDVHWACMDCOMPLETE)));
                    pHostCmd->iDstID = pCmd->iDisplay;
                    pHostCmd->customOpCode = VBVAHG_DCUSTOM_VHWA_CMDCOMPLETE;
                    VBVAHOSTCMDVHWACMDCOMPLETE *pBody = VBVAHOSTCMD_BODY(pHostCmd, VBVAHOSTCMDVHWACMDCOMPLETE);
                    pBody->offCmd = offCmd;
                }
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        }

        if(RT_SUCCESS(rc))
        {
            rc = HGSMIHostCommandProcessAndFreeAsynch(pIns, pHostCmd, (pCmd->Flags & VBOXVHWACMD_FLAG_GH_ASYNCH_IRQ) != 0);
            AssertRC(rc);
            if(RT_SUCCESS(rc))
            {
                return rc;
            }
            HGSMIHostCommandFree (pIns, pHostCmd);
        }
    }
    else
    {
        if(pCmd->GuestVBVAReserved1)
        {
            RTSEMEVENT hComplEvent = (RTSEMEVENT)pCmd->GuestVBVAReserved1;
            RTSemEventSignal(hComplEvent);
        }
        rc = VINF_SUCCESS;
    }
    return rc;
}
#endif


/*
 *
 * New VBVA uses a new interface id: #define VBE_DISPI_ID_VBOX_VIDEO         0xBE01
 *
 * VBVA uses two 32 bits IO ports to write VRAM offsets of shared memory blocks for commands.
 *                                 Read                        Write
 * Host port 0x3b0                 to process                  completed
 * Guest port 0x3d0                control value?              to process
 *
 */

static DECLCALLBACK(void) vbvaNotifyGuest (void *pvCallback)
{
#if defined(VBOX_WITH_HGSMI) && defined(VBOX_WITH_VIDEOHWACCEL)
    PVGASTATE pVGAState = (PVGASTATE)pvCallback;
    PPDMDEVINS pDevIns = pVGAState->pDevInsR3;
    PDMCritSectEnter(&pVGAState->lock, VERR_SEM_BUSY);
    HGSMISetHostGuestFlags(pVGAState->pHGSMI, HGSMIHOSTFLAGS_IRQ);
    PDMDevHlpPCISetIrq(pDevIns, 0, PDM_IRQ_LEVEL_HIGH);
    PDMCritSectLeave(&pVGAState->lock);
#else
    NOREF(pvCallback);
    /* Do nothing. Later the VMMDev/VGA IRQ can be used for the notification. */
#endif
}

/* The guest submitted a buffer. @todo Verify all guest data. */
static DECLCALLBACK(int) vbvaChannelHandler (void *pvHandler, uint16_t u16ChannelInfo, void *pvBuffer, HGSMISIZE cbBuffer)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pvHandler %p, u16ChannelInfo %d, pvBuffer %p, cbBuffer %u\n",
            pvHandler, u16ChannelInfo, pvBuffer, cbBuffer));

    PVGASTATE pVGAState = (PVGASTATE)pvHandler;
    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pIns);

    switch (u16ChannelInfo)
    {
        case VBVA_QUERY_CONF32:
        {
            if (cbBuffer < sizeof (VBVACONF32))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            VBVACONF32 *pConf32 = (VBVACONF32 *)pvBuffer;
            LogFlowFunc(("VBVA_QUERY_CONF32: u32Index %d, u32Value 0x%x\n",
                         pConf32->u32Index, pConf32->u32Value));

            if (pConf32->u32Index == VBOX_VBVA_CONF32_MONITOR_COUNT)
            {
                pConf32->u32Value = pCtx->cViews;
            }
            else if (pConf32->u32Index == VBOX_VBVA_CONF32_HOST_HEAP_SIZE)
            {
                /* @todo a value caclucated from the vram size */
                pConf32->u32Value = 64*_1K;
            }
            else
            {
                Log(("Unsupported VBVA_QUERY_CONF32 index %d!!!\n",
                     pConf32->u32Index));
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VBVA_SET_CONF32:
        {
            if (cbBuffer < sizeof (VBVACONF32))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            VBVACONF32 *pConf32 = (VBVACONF32 *)pvBuffer;
            LogFlowFunc(("VBVA_SET_CONF32: u32Index %d, u32Value 0x%x\n",
                         pConf32->u32Index, pConf32->u32Value));

            if (pConf32->u32Index == VBOX_VBVA_CONF32_MONITOR_COUNT)
            {
                /* do nothing. this is a const. */
            }
            else if (pConf32->u32Index == VBOX_VBVA_CONF32_HOST_HEAP_SIZE)
            {
                /* do nothing. this is a const. */
            }
            else
            {
                Log(("Unsupported VBVA_SET_CONF32 index %d!!!\n",
                     pConf32->u32Index));
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VBVA_INFO_VIEW:
        {
            if (cbBuffer < sizeof (VBVAINFOVIEW))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            /* Guest submits an array of VBVAINFOVIEW structures. */
            VBVAINFOVIEW *pView = (VBVAINFOVIEW *)pvBuffer;

            for (;
                 cbBuffer >= sizeof (VBVAINFOVIEW);
                 pView++, cbBuffer -= sizeof (VBVAINFOVIEW))
            {
                LogFlowFunc(("VBVA_INFO_VIEW: index %d, offset 0x%x, size 0x%x, max screen size 0x%x\n",
                             pView->u32ViewIndex, pView->u32ViewOffset, pView->u32ViewSize, pView->u32MaxScreenSize));

                /* @todo verify view data. */
                if (pView->u32ViewIndex >= pCtx->cViews)
                {
                    Log(("View index too large %d!!!\n",
                         pView->u32ViewIndex));
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }

                pCtx->aViews[pView->u32ViewIndex].view = *pView;
            }
        } break;

        case VBVA_INFO_HEAP:
        {
            if (cbBuffer < sizeof (VBVAINFOHEAP))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            VBVAINFOHEAP *pHeap = (VBVAINFOHEAP *)pvBuffer;
            LogFlowFunc(("VBVA_INFO_HEAP: offset 0x%x, size 0x%x\n",
                         pHeap->u32HeapOffset, pHeap->u32HeapSize));

            rc = HGSMISetupHostHeap (pIns, pHeap->u32HeapOffset, pHeap->u32HeapSize);
        } break;

        case VBVA_FLUSH:
        {
            if (cbBuffer < sizeof (VBVAFLUSH))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            VBVAFLUSH *pFlush = (VBVAFLUSH *)pvBuffer;
            LogFlowFunc(("VBVA_FLUSH: u32Reserved 0x%x\n",
                         pFlush->u32Reserved));

            rc = vbvaFlush (pVGAState, pCtx);
        } break;

        case VBVA_INFO_SCREEN:
        {
            if (cbBuffer < sizeof (VBVAINFOSCREEN))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            VBVAINFOSCREEN *pScreen = (VBVAINFOSCREEN *)pvBuffer;
            LogFlowFunc(("VBVA_INFO_SCREEN: [%d] @%d,%d %dx%d, line 0x%x, BPP %d, flags 0x%x\n",
                         pScreen->u32ViewIndex, pScreen->i32OriginX, pScreen->i32OriginY,
                         pScreen->u32Width, pScreen->u32Height,
                         pScreen->u32LineSize,  pScreen->u16BitsPerPixel, pScreen->u16Flags));

            if (pScreen->u32ViewIndex < RT_ELEMENTS (pCtx->aViews))
            {
                vbvaResize (pVGAState, &pCtx->aViews[pScreen->u32ViewIndex], pScreen);
            }
            else
            {
                Log(("View index too large %d!!!\n",
                     pScreen->u32ViewIndex));
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VBVA_ENABLE:
        {
            if (cbBuffer < sizeof (VBVAENABLE))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            unsigned uScreenId = vbvaViewFromOffset (pIns, pCtx, pvBuffer);

            if (uScreenId == ~0U)
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            VBVAENABLE *pEnable = (VBVAENABLE *)pvBuffer;
            LogFlowFunc(("VBVA_ENABLE[%d]: u32Flags 0x%x u32Offset 0x%x\n",
                         uScreenId, pEnable->u32Flags, pEnable->u32Offset));

            if ((pEnable->u32Flags & (VBVA_F_ENABLE | VBVA_F_DISABLE)) == VBVA_F_ENABLE)
            {
                /* Guest reported offset relative to view. */
                uint32_t u32Offset = pEnable->u32Offset + pCtx->aViews[uScreenId].view.u32ViewOffset;

                VBVABUFFER *pVBVA = (VBVABUFFER *)HGSMIOffsetToPointerHost (pIns, u32Offset);

                if (pVBVA)
                {
                    /* Process any pending orders and empty the VBVA ring buffer. */
                    vbvaFlush (pVGAState, pCtx);

                    rc = vbvaEnable (uScreenId, pVGAState, pCtx, pVBVA, u32Offset);
                }
                else
                {
                    Log(("Invalid VBVABUFFER offset 0x%x!!!\n",
                         pEnable->u32Offset));
                    rc = VERR_INVALID_PARAMETER;
                }
            }
            else if ((pEnable->u32Flags & (VBVA_F_ENABLE | VBVA_F_DISABLE)) == VBVA_F_DISABLE)
            {
                rc = vbvaDisable (uScreenId, pVGAState, pCtx);
            }
            else
            {
                Log(("Invalid VBVA_ENABLE flags 0x%x!!!\n",
                     pEnable->u32Flags));
                rc = VERR_INVALID_PARAMETER;
            }

            pEnable->i32Result = rc;
        } break;

        case VBVA_MOUSE_POINTER_SHAPE:
        {
            if (cbBuffer < sizeof (VBVAMOUSEPOINTERSHAPE))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            VBVAMOUSEPOINTERSHAPE *pShape = (VBVAMOUSEPOINTERSHAPE *)pvBuffer;

            LogFlowFunc(("VBVA_MOUSE_POINTER_SHAPE: i32Result 0x%x, fu32Flags 0x%x, hot spot %d,%d, size %dx%d\n",
                         pShape->i32Result,
                         pShape->fu32Flags,
                         pShape->u32HotX,
                         pShape->u32HotY,
                         pShape->u32Width,
                         pShape->u32Height));

            rc = vbvaMousePointerShape (pVGAState, pCtx, pShape, cbBuffer);

            pShape->i32Result = rc;
        } break;


#ifdef VBOX_WITH_VIDEOHWACCEL
        case VBVA_VHWA_CMD:
        {
            rc = vbvaVHWAHandleCommand (pVGAState, pCtx, (PVBOXVHWACMD)pvBuffer);
        } break;
#endif

        default:
            Log(("Unsupported VBVA guest command %d!!!\n",
                 u16ChannelInfo));
            break;
    }

    return rc;
}

void VBVAReset (PVGASTATE pVGAState)
{
    if (!pVGAState || !pVGAState->pHGSMI)
    {
        return;
    }

    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pVGAState->pHGSMI);

#ifdef VBOX_WITH_VIDEOHWACCEL
    vbvaVHWAReset (pVGAState);
#endif

    HGSMIReset (pVGAState->pHGSMI);

    if (pCtx)
    {
        vbvaFlush (pVGAState, pCtx);

        unsigned uScreenId;

        for (uScreenId = 0; uScreenId < pCtx->cViews; uScreenId++)
        {
            vbvaDisable (uScreenId, pVGAState, pCtx);
        }
    }

}

int VBVAUpdateDisplay (PVGASTATE pVGAState)
{
    int rc = VERR_NOT_SUPPORTED; /* Assuming that the VGA device will have to do updates. */

    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pVGAState->pHGSMI);

    if (pCtx)
    {
        rc = vbvaFlush (pVGAState, pCtx);

        if (RT_SUCCESS (rc))
        {
            if (!pCtx->aViews[0].pVBVA)
            {
                /* VBVA is not enabled for the first view, so VGA device must do updates. */
                rc = VERR_NOT_SUPPORTED;
            }
        }
    }

    return rc;
}

static HGSMICHANNELHANDLER sOldChannelHandler;

int VBVAInit (PVGASTATE pVGAState)
{
    PPDMDEVINS pDevIns = pVGAState->pDevInsR3;

    PVM pVM = PDMDevHlpGetVM(pDevIns);

    int rc = HGSMICreate (&pVGAState->pHGSMI,
                          pVM,
                          "VBVA",
                          0,
                          pVGAState->vram_ptrR3,
                          pVGAState->vram_size,
                          vbvaNotifyGuest,
                          pVGAState,
                          sizeof (VBVACONTEXT));

     if (RT_SUCCESS (rc))
     {
         rc = HGSMIHostChannelRegister (pVGAState->pHGSMI,
                                    HGSMI_CH_VBVA,
                                    vbvaChannelHandler,
                                    pVGAState,
                                    &sOldChannelHandler);
         if (RT_SUCCESS (rc))
         {
             VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pVGAState->pHGSMI);
             pCtx->cViews = pVGAState->cMonitors;
         }
     }

     return rc;

}

void VBVADestroy (PVGASTATE pVGAState)
{
    HGSMIDestroy (pVGAState->pHGSMI);
    pVGAState->pHGSMI = NULL;
}
