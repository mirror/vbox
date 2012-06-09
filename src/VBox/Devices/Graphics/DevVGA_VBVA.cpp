/** @file
 * VirtualBox Video Acceleration (VBVA).
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_DEV_VGA
#include <VBox/vmm/pdmifs.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/ssm.h>
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

#include "DevVGA.h"

/* A very detailed logging. */
#if 0 // def DEBUG_sunlover
#define LOGVBVABUFFER(a) LogFlow(a)
#else
#define LOGVBVABUFFER(a) do {} while(0)
#endif

typedef struct VBVAPARTIALRECORD
{
    uint8_t *pu8;
    uint32_t cb;
} VBVAPARTIALRECORD;

typedef struct VBVAVIEW
{
    VBVAINFOVIEW    view;
    VBVAINFOSCREEN  screen;
    VBVABUFFER     *pVBVA;
    uint32_t        u32VBVAOffset;
    VBVAPARTIALRECORD partialRecord;
} VBVAVIEW;

typedef struct VBVAMOUSESHAPEINFO
{
    bool fSet;
    bool fVisible;
    bool fAlpha;
    uint32_t u32HotX;
    uint32_t u32HotY;
    uint32_t u32Width;
    uint32_t u32Height;
    uint32_t cbShape;
    uint32_t cbAllocated;
    uint8_t *pu8Shape;
} VBVAMOUSESHAPEINFO;

/* @todo saved state: save and restore VBVACONTEXT */
typedef struct VBVACONTEXT
{
    uint32_t cViews;
    VBVAVIEW aViews[64 /* @todo SchemaDefs::MaxGuestMonitors*/];
    VBVAMOUSESHAPEINFO mouseShapeInfo;
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

    uint32_t cbRecordCurrent = ASMAtomicReadU32(&pVBVA->aRecords[indexRecordFirst].cbRecord);

    LOGVBVABUFFER(("cbRecord = 0x%08X, pPartialRecord->cb = 0x%08X\n", cbRecordCurrent, pPartialRecord->cb));

    uint32_t cbRecord = cbRecordCurrent & ~VBVA_F_RECORD_PARTIAL;

    if (pPartialRecord->cb)
    {
        /* There is a partial read in process. Continue with it. */
        Assert (pPartialRecord->pu8);

        LOGVBVABUFFER(("continue partial record cb = %d cbRecord 0x%08X, first = %d, free = %d\n",
                      pPartialRecord->cb, cbRecordCurrent, indexRecordFirst, indexRecordFree));

        if (cbRecord > pPartialRecord->cb)
        {
            /* New data has been added to the record. */
            if (!vbvaPartialRead (pPartialRecord, cbRecord, pVBVA))
            {
                return false;
            }
        }

        if (!(cbRecordCurrent & VBVA_F_RECORD_PARTIAL))
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
    if (cbRecordCurrent & VBVA_F_RECORD_PARTIAL)
    {
        /* Current record is being written by guest. '=' is important here,
         * because the guest will do a FLUSH at this condition.
         * This partial record is too large for the ring buffer and must
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
                          pPartialRecord->cb, cbRecordCurrent, indexRecordFirst, indexRecordFree));
        }

        return true;
    }

    /* Current record is complete. If it is not empty, process it. */
    if (cbRecord)
    {
        /* The size of largest contiguous chunk in the ring biffer. */
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
    RT_ZERO(dirtyRect);

    bool fUpdate = false; /* Whether there were any updates. */
    bool fDirtyEmpty = true;

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
            if (fDirtyEmpty)
            {
                /* This is the first rectangle to be added. */
                dirtyRect.xLeft   = phdr->x;
                dirtyRect.yTop    = phdr->y;
                dirtyRect.xRight  = xRight;
                dirtyRect.yBottom = yBottom;
                fDirtyEmpty       = false;
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
        if (dirtyRect.xRight - dirtyRect.xLeft)
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

static int vbvaEnable (unsigned uScreenId, PVGASTATE pVGAState, VBVACONTEXT *pCtx, VBVABUFFER *pVBVA, uint32_t u32Offset, bool fRestored)
{
    /* @todo old code did a UpdateDisplayAll at this place. */

    int rc;

    if (pVGAState->pDrv->pfnVBVAEnable)
    {
        pVBVA->hostFlags.u32HostEvents = 0;
        pVBVA->hostFlags.u32SupportedOrders = 0;

        rc = pVGAState->pDrv->pfnVBVAEnable (pVGAState->pDrv, uScreenId, &pVBVA->hostFlags);
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS (rc))
    {
        /* pVBVA->hostFlags has been set up by pfnVBVAEnable. */
        LogFlowFunc(("u32HostEvents 0x%08X, u32SupportedOrders 0x%08X\n",
                     pVBVA->hostFlags.u32HostEvents, pVBVA->hostFlags.u32SupportedOrders));

        if (!fRestored)
        {
            /* @todo Actually this function must not touch the partialRecord structure at all,
             * because initially it is a zero and when VBVA is disabled this should be set to zero.
             * But I'm not sure that no code depends on zeroing partialRecord here.
             * So for now (a quick fix for 4.1) just do not do this if the VM was restored,
             * when partialRecord might be loaded already from the saved state.
             */
            pCtx->aViews[uScreenId].partialRecord.pu8 = NULL;
            pCtx->aViews[uScreenId].partialRecord.cb = 0;
        }

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
        pView->pVBVA->hostFlags.u32HostEvents = 0;
        pView->pVBVA->hostFlags.u32SupportedOrders = 0;

        pView->partialRecord.pu8 = NULL;
        pView->partialRecord.cb = 0;

        pView->pVBVA = NULL;
        pView->u32VBVAOffset = HGSMIOFFSET_VOID;
    }

    pVGAState->pDrv->pfnVBVADisable (pVGAState->pDrv, uScreenId);
    return VINF_SUCCESS;
}

bool VBVAIsEnabled(PVGASTATE pVGAState)
{
    PHGSMIINSTANCE pHGSMI = pVGAState->pHGSMI;
    if (pHGSMI)
    {
        VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pHGSMI);
        if (pCtx)
        {
            if (pCtx->cViews)
            {
                VBVAVIEW * pView = &pCtx->aViews[0];
                if (pView->pVBVA)
                    return true;
            }
        }
    }
    return false;
}

#ifdef DEBUG_sunlover
void dumpMouseShapeInfo(const VBVAMOUSESHAPEINFO *pMouseShapeInfo)
{
    LogFlow(("fSet = %d, fVisible %d, fAlpha %d, @%d,%d %dx%d (%p, %d/%d)\n",
             pMouseShapeInfo->fSet,
             pMouseShapeInfo->fVisible,
             pMouseShapeInfo->fAlpha,
             pMouseShapeInfo->u32HotX,
             pMouseShapeInfo->u32HotY,
             pMouseShapeInfo->u32Width,
             pMouseShapeInfo->u32Height,
             pMouseShapeInfo->pu8Shape,
             pMouseShapeInfo->cbShape,
             pMouseShapeInfo->cbAllocated
             ));
}
#endif

static int vbvaUpdateMousePointerShape(PVGASTATE pVGAState, VBVAMOUSESHAPEINFO *pMouseShapeInfo, bool fShape, const uint8_t *pu8Shape)
{
    int rc;
    LogFlowFunc(("pVGAState %p, pMouseShapeInfo %p, fShape %d, pu8Shape %p\n",
                  pVGAState, pMouseShapeInfo, fShape, pu8Shape));
#ifdef DEBUG_sunlover
    dumpMouseShapeInfo(pMouseShapeInfo);
#endif

    if (fShape && pu8Shape != NULL)
    {
        rc = pVGAState->pDrv->pfnVBVAMousePointerShape (pVGAState->pDrv,
                                                        pMouseShapeInfo->fVisible,
                                                        pMouseShapeInfo->fAlpha,
                                                        pMouseShapeInfo->u32HotX,
                                                        pMouseShapeInfo->u32HotY,
                                                        pMouseShapeInfo->u32Width,
                                                        pMouseShapeInfo->u32Height,
                                                        pu8Shape);
    }
    else
    {
        rc = pVGAState->pDrv->pfnVBVAMousePointerShape (pVGAState->pDrv,
                                                        pMouseShapeInfo->fVisible,
                                                        false,
                                                        0, 0,
                                                        0, 0,
                                                        NULL);
    }

    return rc;
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

    /* Save mouse info it will be used to restore mouse pointer after restoring saved state. */
    pCtx->mouseShapeInfo.fSet = true;
    pCtx->mouseShapeInfo.fVisible = fVisible;
    pCtx->mouseShapeInfo.fAlpha = fAlpha;
    if (fShape)
    {
        /* Data related to shape. */
        pCtx->mouseShapeInfo.u32HotX = pShape->u32HotX;
        pCtx->mouseShapeInfo.u32HotY = pShape->u32HotY;
        pCtx->mouseShapeInfo.u32Width = pShape->u32Width;
        pCtx->mouseShapeInfo.u32Height = pShape->u32Height;

        /* Reallocate memory buffer if necessary. */
        if (cbPointerData > pCtx->mouseShapeInfo.cbAllocated)
        {
            RTMemFree (pCtx->mouseShapeInfo.pu8Shape);
            pCtx->mouseShapeInfo.pu8Shape = NULL;
            pCtx->mouseShapeInfo.cbShape = 0;

            uint8_t *pu8Shape = (uint8_t *)RTMemAlloc (cbPointerData);
            if (pu8Shape)
            {
                pCtx->mouseShapeInfo.pu8Shape = pu8Shape;
                pCtx->mouseShapeInfo.cbAllocated = cbPointerData;
            }
        }

        /* Copy shape bitmaps. */
        if (pCtx->mouseShapeInfo.pu8Shape)
        {
            memcpy (pCtx->mouseShapeInfo.pu8Shape, &pShape->au8Data[0], cbPointerData);
            pCtx->mouseShapeInfo.cbShape = cbPointerData;
        }
    }

    if (pVGAState->pDrv->pfnVBVAMousePointerShape == NULL)
    {
        return VERR_NOT_SUPPORTED;
    }

    int rc = vbvaUpdateMousePointerShape(pVGAState, &pCtx->mouseShapeInfo, fShape, &pShape->au8Data[0]);

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

    dumpMouseShapeInfo(&pCtx->mouseShapeInfo);
}
#endif /* DEBUG_sunlover */

#define VBOXVBVASAVEDSTATE_VHWAAVAILABLE_MAGIC   0x12345678
#define VBOXVBVASAVEDSTATE_VHWAUNAVAILABLE_MAGIC 0x9abcdef0

#ifdef VBOX_WITH_VIDEOHWACCEL
static void vbvaVHWAHHCommandReinit(VBOXVHWACMD* pHdr, VBOXVHWACMD_TYPE enmCmd, int32_t iDisplay)
{
    memset(pHdr, 0, VBOXVHWACMD_HEADSIZE());
    pHdr->cRefs = 1;
    pHdr->iDisplay = iDisplay;
    pHdr->rc = VERR_NOT_IMPLEMENTED;
    pHdr->enmCmd = enmCmd;
    pHdr->Flags = VBOXVHWACMD_FLAG_HH_CMD;
}

static VBOXVHWACMD* vbvaVHWAHHCommandCreate (PVGASTATE pVGAState, VBOXVHWACMD_TYPE enmCmd, int32_t iDisplay, VBOXVHWACMD_LENGTH cbCmd)
{
    VBOXVHWACMD* pHdr = (VBOXVHWACMD*)RTMemAlloc(cbCmd + VBOXVHWACMD_HEADSIZE());
    Assert(pHdr);
    if (pHdr)
        vbvaVHWAHHCommandReinit(pHdr, enmCmd, iDisplay);

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
    if (pVGAState->pDrv->pfnVHWACommandProcess)
        pVGAState->pDrv->pfnVHWACommandProcess(pVGAState->pDrv, pCmd);
#ifdef DEBUG_misha
    else
        AssertFailed();
#endif
    return 0;
}

static DECLCALLBACK(void) vbvaVHWAHHCommandSetEventCallback(void * pContext)
{
    RTSemEventSignal((RTSEMEVENT)pContext);
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
        VBOXVHWA_HH_CALLBACK_SET(pCmd, vbvaVHWAHHCommandSetEventCallback, (void*)hComplEvent);
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
    VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(pVGAState, VBOXVHWACMD_TYPE_HH_CONSTRUCT, 0, sizeof(VBOXVHWACMD_HH_CONSTRUCT));
    Assert(pCmd);
    if(pCmd)
    {
        uint32_t iDisplay = 0;
        int rc = VINF_SUCCESS;
        VBOXVHWACMD_HH_CONSTRUCT * pBody = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_HH_CONSTRUCT);

        do
        {
            memset(pBody, 0, sizeof(VBOXVHWACMD_HH_CONSTRUCT));

            PPDMDEVINS pDevIns = pVGAState->pDevInsR3;
            PVM pVM = PDMDevHlpGetVM(pDevIns);

            pBody->pVM = pVM;
            pBody->pvVRAM = pVGAState->vram_ptrR3;
            pBody->cbVRAM = pVGAState->vram_size;

            rc = vbvaVHWAHHCommandPost(pVGAState, pCmd);
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

                if (!RT_SUCCESS(rc))
                    break;
            }
            else
                break;

            ++iDisplay;
            if (iDisplay >= pVGAState->cMonitors)
                break;
            vbvaVHWAHHCommandReinit(pCmd, VBOXVHWACMD_TYPE_HH_CONSTRUCT, (int32_t)iDisplay);
        } while (true);

        vbvaVHWAHHCommandRelease(pCmd);

        return rc;
    }
    return VERR_OUT_OF_RESOURCES;
}

int vbvaVHWAReset (PVGASTATE pVGAState)
{
    /* ensure we have all pending cmds processed and h->g cmds disabled */
    VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(pVGAState, VBOXVHWACMD_TYPE_HH_RESET, 0, 0);
    Assert(pCmd);
    if(pCmd)
    {
        int rc = VINF_SUCCESS;
        uint32_t iDisplay = 0;

        do
        {
            rc =vbvaVHWAHHCommandPost(pVGAState, pCmd);
            AssertRC(rc);
            if(RT_SUCCESS(rc))
            {
                rc = pCmd->rc;
                AssertMsg(RT_SUCCESS(rc) || rc == VERR_NOT_IMPLEMENTED, ("%Rrc\n", rc));
                if (rc == VERR_NOT_IMPLEMENTED)
                    rc = VINF_SUCCESS;
            }

            if (!RT_SUCCESS(rc))
                break;

            ++iDisplay;
            if (iDisplay >= pVGAState->cMonitors)
                break;
            vbvaVHWAHHCommandReinit(pCmd, VBOXVHWACMD_TYPE_HH_RESET, (int32_t)iDisplay);

        } while (true);

        vbvaVHWAHHCommandRelease(pCmd);

        return rc;
    }
    return VERR_OUT_OF_RESOURCES;
}

typedef DECLCALLBACK(bool) FNVBOXVHWAHHCMDPRECB(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, void *pvContext);
typedef FNVBOXVHWAHHCMDPRECB *PFNVBOXVHWAHHCMDPRECB;

typedef DECLCALLBACK(bool) FNVBOXVHWAHHCMDPOSTCB(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, int rc, void *pvContext);
typedef FNVBOXVHWAHHCMDPOSTCB *PFNVBOXVHWAHHCMDPOSTCB;

int vbvaVHWAHHPost(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, PFNVBOXVHWAHHCMDPRECB pfnPre, PFNVBOXVHWAHHCMDPOSTCB pfnPost, void *pvContext)
{
    const VBOXVHWACMD_TYPE enmType = pCmd->enmCmd;
    int rc = VINF_SUCCESS;
    uint32_t iDisplay = 0;

    do
    {
        if (!pfnPre || pfnPre(pVGAState, pCmd, iDisplay, pvContext))
        {
            rc = vbvaVHWAHHCommandPost(pVGAState, pCmd);
            AssertRC(rc);
            if (pfnPost)
            {
                if (!pfnPost(pVGAState, pCmd, iDisplay, rc, pvContext))
                {
                    rc = VINF_SUCCESS;
                    break;
                }
                rc = VINF_SUCCESS;
            }
            else if(RT_SUCCESS(rc))
            {
                rc = pCmd->rc;
                AssertMsg(RT_SUCCESS(rc) || rc == VERR_NOT_IMPLEMENTED, ("%Rrc\n", rc));
                if(rc == VERR_NOT_IMPLEMENTED)
                {
                    rc = VINF_SUCCESS;
                }
            }

            if (!RT_SUCCESS(rc))
                break;
        }

        ++iDisplay;
        if (iDisplay >= pVGAState->cMonitors)
            break;
        vbvaVHWAHHCommandReinit(pCmd, enmType, (int32_t)iDisplay);
    } while (true);

    return rc;
}

/* @todo call this also on reset? */
int vbvaVHWAEnable (PVGASTATE pVGAState, bool bEnable)
{
    const VBOXVHWACMD_TYPE enmType = bEnable ? VBOXVHWACMD_TYPE_HH_ENABLE : VBOXVHWACMD_TYPE_HH_DISABLE;
    VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(pVGAState,
                        enmType,
                    0, 0);
    Assert(pCmd);
    if(pCmd)
    {
        int rc = vbvaVHWAHHPost (pVGAState, pCmd, NULL, NULL, NULL);
        vbvaVHWAHHCommandRelease(pCmd);
        return rc;
    }
    return VERR_OUT_OF_RESOURCES;
}

int vboxVBVASaveStatePrep (PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    /* ensure we have no pending commands */
    return vbvaVHWAEnable(PDMINS_2_DATA(pDevIns, PVGASTATE), false);
}

int vboxVBVASaveStateDone (PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    /* ensure we have no pending commands */
    return vbvaVHWAEnable(PDMINS_2_DATA(pDevIns, PVGASTATE), true);
}

int vbvaVHWACommandCompleteAsynch(PPDMIDISPLAYVBVACALLBACKS pInterface, PVBOXVHWACMD pCmd)
{
    int rc;
    if((pCmd->Flags & VBOXVHWACMD_FLAG_HH_CMD) == 0)
    {
        PVGASTATE pVGAState = PPDMIDISPLAYVBVACALLBACKS_2_PVGASTATE(pInterface);
        PHGSMIINSTANCE pIns = pVGAState->pHGSMI;

        Assert(pCmd->Flags & VBOXVHWACMD_FLAG_HG_ASYNCH);
#ifdef VBOX_WITH_WDDM
        if (pVGAState->fGuestCaps & VBVACAPS_COMPLETEGCMD_BY_IOREAD)
        {
            rc = HGSMICompleteGuestCommand(pIns, pCmd, !!(pCmd->Flags & VBOXVHWACMD_FLAG_GH_ASYNCH_IRQ));
            AssertRC(rc);
        }
        else
#endif
        {
            VBVAHOSTCMD *pHostCmd;
            int32_t iDisplay = pCmd->iDisplay;

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
    }
    else
    {
        PFNVBOXVHWA_HH_CALLBACK pfn = VBOXVHWA_HH_CALLBACK_GET(pCmd);
        if(pfn)
        {
            pfn(VBOXVHWA_HH_CALLBACK_GET_ARG(pCmd));
        }
        rc = VINF_SUCCESS;
    }
    return rc;
}

typedef struct VBOXVBVASAVEDSTATECBDATA
{
    PSSMHANDLE pSSM;
    int rc;
    bool ab2DOn[VBOX_VIDEO_MAX_SCREENS];
} VBOXVBVASAVEDSTATECBDATA, *PVBOXVBVASAVEDSTATECBDATA;

static DECLCALLBACK(bool) vboxVBVASaveStateBeginPostCb(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, int rc, void *pvContext)
{
    PVBOXVBVASAVEDSTATECBDATA pData = (PVBOXVBVASAVEDSTATECBDATA)pvContext;
    if (RT_FAILURE(pData->rc))
        return false;
    if (RT_FAILURE(rc))
    {
        pData->rc = rc;
        return false;
    }

    Assert(iDisplay < RT_ELEMENTS(pData->ab2DOn));
    if (iDisplay >= RT_ELEMENTS(pData->ab2DOn))
    {
        pData->rc = VERR_INVALID_PARAMETER;
        return false;
    }

    Assert(RT_SUCCESS(pCmd->rc) || pCmd->rc == VERR_NOT_IMPLEMENTED);
    if (RT_SUCCESS(pCmd->rc))
    {
        pData->ab2DOn[iDisplay] = true;
    }
    else if (pCmd->rc != VERR_NOT_IMPLEMENTED)
    {
        pData->rc = pCmd->rc;
        return false;
    }

    return true;
}

static DECLCALLBACK(bool) vboxVBVASaveStatePerformPreCb(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, void *pvContext)
{
    PVBOXVBVASAVEDSTATECBDATA pData = (PVBOXVBVASAVEDSTATECBDATA)pvContext;
    if (RT_FAILURE(pData->rc))
        return false;

    Assert(iDisplay < RT_ELEMENTS(pData->ab2DOn));
    if (iDisplay >= RT_ELEMENTS(pData->ab2DOn))
    {
        pData->rc = VERR_INVALID_PARAMETER;
        return false;
    }

    int rc;

    if (pData->ab2DOn[iDisplay])
    {
        rc = SSMR3PutU32 (pData->pSSM, VBOXVBVASAVEDSTATE_VHWAAVAILABLE_MAGIC); AssertRC(rc);
        if (RT_FAILURE(rc))
        {
            pData->rc = rc;
            return false;
        }
        return true;
    }

    rc = SSMR3PutU32 (pData->pSSM, VBOXVBVASAVEDSTATE_VHWAUNAVAILABLE_MAGIC); AssertRC(rc);
    if (RT_FAILURE(rc))
    {
        pData->rc = rc;
        return false;
    }

    return false;
}

static DECLCALLBACK(bool) vboxVBVASaveStateEndPreCb(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, void *pvContext)
{
    PVBOXVBVASAVEDSTATECBDATA pData = (PVBOXVBVASAVEDSTATECBDATA)pvContext;
    Assert(iDisplay < RT_ELEMENTS(pData->ab2DOn));
    if (pData->ab2DOn[iDisplay])
    {
        return true;
    }

    return false;
}

static DECLCALLBACK(bool) vboxVBVALoadStatePerformPostCb(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, int rc, void *pvContext)
{
    PVBOXVBVASAVEDSTATECBDATA pData = (PVBOXVBVASAVEDSTATECBDATA)pvContext;
    if (RT_FAILURE(pData->rc))
        return false;
    if (RT_FAILURE(rc))
    {
        pData->rc = rc;
        return false;
    }

    Assert(iDisplay < RT_ELEMENTS(pData->ab2DOn));
    if (iDisplay >= RT_ELEMENTS(pData->ab2DOn))
    {
        pData->rc = VERR_INVALID_PARAMETER;
        return false;
    }

    Assert(RT_SUCCESS(pCmd->rc) || pCmd->rc == VERR_NOT_IMPLEMENTED);
    if (pCmd->rc == VERR_NOT_IMPLEMENTED)
    {
        pData->rc = SSMR3SkipToEndOfUnit(pData->pSSM);
        AssertRC(pData->rc);
        return false;
    }
    if (RT_FAILURE(pCmd->rc))
    {
        pData->rc = pCmd->rc;
        return false;
    }

    return true;
}

static DECLCALLBACK(bool) vboxVBVALoadStatePerformPreCb(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, void *pvContext)
{
    PVBOXVBVASAVEDSTATECBDATA pData = (PVBOXVBVASAVEDSTATECBDATA)pvContext;
    if (RT_FAILURE(pData->rc))
        return false;

    Assert(iDisplay < RT_ELEMENTS(pData->ab2DOn));
    if (iDisplay >= RT_ELEMENTS(pData->ab2DOn))
    {
        pData->rc = VERR_INVALID_PARAMETER;
        return false;
    }

    int rc;
    uint32_t u32;
    rc = SSMR3GetU32(pData->pSSM, &u32); AssertRC(rc);
    if (RT_FAILURE(rc))
    {
        pData->rc = rc;
        return false;
    }

    switch (u32)
    {
        case VBOXVBVASAVEDSTATE_VHWAAVAILABLE_MAGIC:
            return true;
        case VBOXVBVASAVEDSTATE_VHWAUNAVAILABLE_MAGIC:
            return false;
        default:
            pData->rc = VERR_INVALID_STATE;
            return false;
    }
}
#endif /* #ifdef VBOX_WITH_VIDEOHWACCEL */

int vboxVBVASaveDevStateExec (PVGASTATE pVGAState, PSSMHANDLE pSSM)
{
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

            /* Save mouse pointer shape information. */
            rc = SSMR3PutBool (pSSM, pCtx->mouseShapeInfo.fSet);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutBool (pSSM, pCtx->mouseShapeInfo.fVisible);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutBool (pSSM, pCtx->mouseShapeInfo.fAlpha);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pCtx->mouseShapeInfo.u32HotX);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pCtx->mouseShapeInfo.u32HotY);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pCtx->mouseShapeInfo.u32Width);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pCtx->mouseShapeInfo.u32Height);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pCtx->mouseShapeInfo.cbShape);
            AssertRCReturn(rc, rc);
            if (pCtx->mouseShapeInfo.cbShape)
            {
                rc = SSMR3PutMem (pSSM, pCtx->mouseShapeInfo.pu8Shape, pCtx->mouseShapeInfo.cbShape);
                AssertRCReturn(rc, rc);
            }

#ifdef VBOX_WITH_WDDM
            /* Size of some additional data. For future extensions. */
            rc = SSMR3PutU32 (pSSM, 4);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pVGAState->fGuestCaps);
            AssertRCReturn(rc, rc);
#else
            /* Size of some additional data. For future extensions. */
            rc = SSMR3PutU32 (pSSM, 0);
            AssertRCReturn(rc, rc);
#endif
        }
    }

    return rc;
}

int vboxVBVASaveStateExec (PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVGASTATE pVGAState = PDMINS_2_DATA(pDevIns, PVGASTATE);
    int rc;
#ifdef VBOX_WITH_VIDEOHWACCEL
    VBOXVBVASAVEDSTATECBDATA VhwaData = {0};
    VhwaData.pSSM = pSSM;
    uint32_t cbCmd = sizeof (VBOXVHWACMD_HH_SAVESTATE_SAVEPERFORM); /* maximum cmd size */
    VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(pVGAState, VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEBEGIN, 0, cbCmd);
    Assert(pCmd);
    if(pCmd)
    {
        vbvaVHWAHHPost (pVGAState, pCmd, NULL, vboxVBVASaveStateBeginPostCb, &VhwaData);
        rc = VhwaData.rc;
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
#endif
            rc = vboxVBVASaveDevStateExec (pVGAState, pSSM);
            AssertRC(rc);
#ifdef VBOX_WITH_VIDEOHWACCEL
            if (RT_SUCCESS(rc))
            {
                vbvaVHWAHHCommandReinit(pCmd, VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEPERFORM, 0);
                VBOXVHWACMD_HH_SAVESTATE_SAVEPERFORM *pSave = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_HH_SAVESTATE_SAVEPERFORM);
                pSave->pSSM = pSSM;
                vbvaVHWAHHPost (pVGAState, pCmd, vboxVBVASaveStatePerformPreCb, NULL, &VhwaData);
                rc = VhwaData.rc;
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    vbvaVHWAHHCommandReinit(pCmd, VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEEND, 0);
                    vbvaVHWAHHPost (pVGAState, pCmd, vboxVBVASaveStateEndPreCb, NULL, &VhwaData);
                    rc = VhwaData.rc;
                    AssertRC(rc);
                }
            }
        }

        vbvaVHWAHHCommandRelease(pCmd);
    }
    else
        rc = VERR_OUT_OF_RESOURCES;
#else
    if (RT_SUCCESS(rc))
    {
        for (uint32_t i = 0; i < pVGAState->cMonitors; ++i)
        {
            rc = SSMR3PutU32 (pSSM, VBOXVBVASAVEDSTATE_VHWAUNAVAILABLE_MAGIC);
            AssertRCReturn(rc, rc);
        }
    }
#endif
    return rc;
}

int vboxVBVALoadStateExec (PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t u32Version)
{
    if (u32Version < VGA_SAVEDSTATE_VERSION_HGSMI)
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

            if (u32Version > VGA_SAVEDSTATE_VERSION_WITH_CONFIG)
            {
                /* Read mouse pointer shape information. */
                rc = SSMR3GetBool (pSSM, &pCtx->mouseShapeInfo.fSet);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetBool (pSSM, &pCtx->mouseShapeInfo.fVisible);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetBool (pSSM, &pCtx->mouseShapeInfo.fAlpha);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pCtx->mouseShapeInfo.u32HotX);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pCtx->mouseShapeInfo.u32HotY);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pCtx->mouseShapeInfo.u32Width);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pCtx->mouseShapeInfo.u32Height);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pCtx->mouseShapeInfo.cbShape);
                AssertRCReturn(rc, rc);
                if (pCtx->mouseShapeInfo.cbShape)
                {
                    pCtx->mouseShapeInfo.pu8Shape = (uint8_t *)RTMemAlloc(pCtx->mouseShapeInfo.cbShape);
                    if (pCtx->mouseShapeInfo.pu8Shape == NULL)
                    {
                        return VERR_NO_MEMORY;
                    }
                    pCtx->mouseShapeInfo.cbAllocated = pCtx->mouseShapeInfo.cbShape;
                    rc = SSMR3GetMem (pSSM, pCtx->mouseShapeInfo.pu8Shape, pCtx->mouseShapeInfo.cbShape);
                    AssertRCReturn(rc, rc);
                }
                else
                {
                    pCtx->mouseShapeInfo.pu8Shape = NULL;
                }

                /* Size of some additional data. For future extensions. */
                uint32_t cbExtra = 0;
                rc = SSMR3GetU32 (pSSM, &cbExtra);
                AssertRCReturn(rc, rc);
#ifdef VBOX_WITH_WDDM
                if (cbExtra >= 4)
                {
                    rc = SSMR3GetU32 (pSSM, &pVGAState->fGuestCaps);
                    AssertRCReturn(rc, rc);
                    cbExtra -= 4;
                }
#endif
                if (cbExtra > 0)
                {
                    rc = SSMR3Skip(pSSM, cbExtra);
                    AssertRCReturn(rc, rc);
                }
            }

            pCtx->cViews = iView;
            LogFlowFunc(("%d views loaded\n", pCtx->cViews));

            if (u32Version > VGA_SAVEDSTATE_VERSION_WDDM)
            {
#ifdef VBOX_WITH_VIDEOHWACCEL
                uint32_t cbCmd = sizeof (VBOXVHWACMD_HH_SAVESTATE_LOADPERFORM); /* maximum cmd size */
                VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(pVGAState, VBOXVHWACMD_TYPE_HH_SAVESTATE_LOADPERFORM, 0, cbCmd);
                Assert(pCmd);
                if(pCmd)
                {
                    VBOXVBVASAVEDSTATECBDATA VhwaData = {0};
                    VhwaData.pSSM = pSSM;
                    VBOXVHWACMD_HH_SAVESTATE_LOADPERFORM *pLoad = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_HH_SAVESTATE_LOADPERFORM);
                    pLoad->pSSM = pSSM;
                    vbvaVHWAHHPost (pVGAState, pCmd, vboxVBVALoadStatePerformPreCb, vboxVBVALoadStatePerformPostCb, &VhwaData);
                    rc = VhwaData.rc;
                    AssertRC(rc);
                    vbvaVHWAHHCommandRelease(pCmd);
                }
                else
                {
                    rc = VERR_OUT_OF_RESOURCES;
                }
#else
                rc = SSMR3SkipToEndOfUnit(pSSM);
                AssertRCReturn(rc, rc);
#endif
            }

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
                vbvaEnable (iView, pVGAState, pCtx, pView->pVBVA, pView->u32VBVAOffset, true /* fRestored */);
                vbvaResize (pVGAState, pView, &pView->screen);
            }
        }

        if (pCtx->mouseShapeInfo.fSet)
        {
            vbvaUpdateMousePointerShape(pVGAState, &pCtx->mouseShapeInfo, true, pCtx->mouseShapeInfo.pu8Shape);
        }
    }

    return VINF_SUCCESS;
}

void VBVARaiseIrq (PVGASTATE pVGAState, uint32_t fFlags)
{
    PPDMDEVINS pDevIns = pVGAState->pDevInsR3;
    PDMCritSectEnter(&pVGAState->lock, VERR_SEM_BUSY);
    HGSMISetHostGuestFlags(pVGAState->pHGSMI, HGSMIHOSTFLAGS_IRQ | fFlags);
    PDMDevHlpPCISetIrq(pDevIns, 0, PDM_IRQ_LEVEL_HIGH);
    PDMCritSectLeave(&pVGAState->lock);
}

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
#if defined(VBOX_WITH_HGSMI) && (defined(VBOX_WITH_VIDEOHWACCEL) || defined(VBOX_WITH_VDMA) || defined(VBOX_WITH_WDDM))
    PVGASTATE pVGAState = (PVGASTATE)pvCallback;
    VBVARaiseIrq (pVGAState, 0);
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
#ifdef VBOX_WITH_VDMA
        case VBVA_VDMA_CMD:
        {
            if (cbBuffer < VBoxSHGSMIBufferHeaderSize() + sizeof (VBOXVDMACBUF_DR))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }
            PVBOXVDMACBUF_DR pCmd = (PVBOXVDMACBUF_DR)VBoxSHGSMIBufferData ((PVBOXSHGSMIHEADER)pvBuffer);
            vboxVDMACommand(pVGAState->pVdma, pCmd, cbBuffer - VBoxSHGSMIBufferHeaderSize());
            rc = VINF_SUCCESS;
            break;
        }
        case VBVA_VDMA_CTL:
        {
            if (cbBuffer < VBoxSHGSMIBufferHeaderSize() + sizeof (VBOXVDMA_CTL))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }
            PVBOXVDMA_CTL pCmd = (PVBOXVDMA_CTL)VBoxSHGSMIBufferData ((PVBOXSHGSMIHEADER)pvBuffer);
            vboxVDMAControl(pVGAState->pVdma, pCmd, cbBuffer - VBoxSHGSMIBufferHeaderSize());
            rc = VINF_SUCCESS;
            break;
        }
#endif
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
                /* @todo a value calculated from the vram size */
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
            VBVAINFOVIEW *pView = &pCtx->aViews[pScreen->u32ViewIndex].view;
            /* Calculate the offset of the  end of the screen so we can make
             * sure it is inside the view.  I assume that screen rollover is not
             * implemented. */
            int64_t offEnd =   (int64_t)pScreen->u32Height * pScreen->u32LineSize
                             + pScreen->u32Width + pScreen->u32StartOffset;
            LogRelFlowFunc(("VBVA_INFO_SCREEN: [%d] @%d,%d %dx%d, line 0x%x, BPP %d, flags 0x%x\n",
                            pScreen->u32ViewIndex, pScreen->i32OriginX, pScreen->i32OriginY,
                            pScreen->u32Width, pScreen->u32Height,
                            pScreen->u32LineSize,  pScreen->u16BitsPerPixel, pScreen->u16Flags));

            if (   pScreen->u32ViewIndex < RT_ELEMENTS (pCtx->aViews)
                && pScreen->u16BitsPerPixel <= 32
                && pScreen->u32Width <= UINT16_MAX
                && pScreen->u32Height <= UINT16_MAX
                && pScreen->u32LineSize <= UINT16_MAX * 4
                && offEnd < pView->u32MaxScreenSize)
            {
                vbvaResize (pVGAState, &pCtx->aViews[pScreen->u32ViewIndex], pScreen);
            }
            else
            {
                LogRelFlow(("VBVA_INFO_SCREEN [%lu]: bad data: %lux%lu, line 0x%lx, BPP %u, start offset %lu, max screen size %lu\n",
                            (unsigned long)pScreen->u32ViewIndex,
                            (unsigned long)pScreen->u32Width,
                            (unsigned long)pScreen->u32Height,
                            (unsigned long)pScreen->u32LineSize,
                            (unsigned long)pScreen->u16BitsPerPixel,
                            (unsigned long)pScreen->u32StartOffset,
                            (unsigned long)pView->u32MaxScreenSize));
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

            VBVAENABLE *pEnable = (VBVAENABLE *)pvBuffer;
            unsigned uScreenId;
            if (pEnable->u32Flags & VBVA_F_EXTENDED)
            {
                if (cbBuffer < sizeof (VBVAENABLE_EX))
                {
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }

                VBVAENABLE_EX *pEnableEx = (VBVAENABLE_EX *)pvBuffer;
                uScreenId = pEnableEx->u32ScreenId;
            }
            else
            {
                uScreenId = vbvaViewFromOffset (pIns, pCtx, pvBuffer);
            }

            if (uScreenId == ~0U)
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            LogFlowFunc(("VBVA_ENABLE[%d]: u32Flags 0x%x u32Offset 0x%x\n",
                         uScreenId, pEnable->u32Flags, pEnable->u32Offset));

            if ((pEnable->u32Flags & (VBVA_F_ENABLE | VBVA_F_DISABLE)) == VBVA_F_ENABLE)
            {
                /* Guest reported offset relative to view. */
                uint32_t u32Offset = pEnable->u32Offset;
                if (!(pEnable->u32Flags & VBVA_F_ABSOFFSET))
                {
                    u32Offset += pCtx->aViews[uScreenId].view.u32ViewOffset;
                }

                VBVABUFFER *pVBVA = (VBVABUFFER *)HGSMIOffsetToPointerHost (pIns, u32Offset);

                if (pVBVA)
                {
                    /* Process any pending orders and empty the VBVA ring buffer. */
                    vbvaFlush (pVGAState, pCtx);

                    rc = vbvaEnable (uScreenId, pVGAState, pCtx, pVBVA, u32Offset, false /* fRestored */);
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

#ifdef VBOX_WITH_WDDM
        case VBVA_INFO_CAPS:
        {
            if (cbBuffer < sizeof (VBVACAPS))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            VBVACAPS *pCaps = (VBVACAPS*)pvBuffer;
            pVGAState->fGuestCaps = pCaps->fCaps;
            pCaps->rc = VINF_SUCCESS;
        } break;
#endif
        case VBVA_SCANLINE_CFG:
        {
            if (cbBuffer < sizeof (VBVASCANLINECFG))
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            VBVASCANLINECFG *pCfg = (VBVASCANLINECFG*)pvBuffer;
            pVGAState->fScanLineCfg = pCfg->fFlags;
            pCfg->rc = VINF_SUCCESS;
        } break;
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

    uint32_t HgFlags = HGSMIReset (pVGAState->pHGSMI);
    if(HgFlags & HGSMIHOSTFLAGS_IRQ)
    {
        /* this means the IRQ is LEVEL_HIGH, need to reset it */
        PDMDevHlpPCISetIrq(pVGAState->pDevInsR3, 0, PDM_IRQ_LEVEL_LOW);
    }

    if (pCtx)
    {
        vbvaFlush (pVGAState, pCtx);

        unsigned uScreenId;

        for (uScreenId = 0; uScreenId < pCtx->cViews; uScreenId++)
        {
            vbvaDisable (uScreenId, pVGAState, pCtx);
        }

        pCtx->mouseShapeInfo.fSet = false;
        RTMemFree(pCtx->mouseShapeInfo.pu8Shape);
        pCtx->mouseShapeInfo.pu8Shape = NULL;
        pCtx->mouseShapeInfo.cbAllocated = 0;
        pCtx->mouseShapeInfo.cbShape = 0;
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
    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pVGAState->pHGSMI);

    if (pCtx)
    {
        pCtx->mouseShapeInfo.fSet = false;
        RTMemFree(pCtx->mouseShapeInfo.pu8Shape);
        pCtx->mouseShapeInfo.pu8Shape = NULL;
        pCtx->mouseShapeInfo.cbAllocated = 0;
        pCtx->mouseShapeInfo.cbShape = 0;
    }

    HGSMIDestroy (pVGAState->pHGSMI);
    pVGAState->pHGSMI = NULL;
}
