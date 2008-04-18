/** @file
 *
 * VirtualBox Windows NT/2000/XP guest video driver
 *
 * Display driver screen draw entry points.
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

/* The driver operates in 3 modes:
 * 1) BASE        : Driver does not report to host about any operations.
 *                  All Drv* are immediately routed to Eng*.
 * 2) VBVA        : Driver reports dirty rectangles to host.
 * 3) VBVA + VRDP : Driver also creates orders pipeline from which VRDP
 *                  can reconstruct all drawing operations, including
 *                  bitmap updates.
 *
 * These modes affect only Drv* functions in this file.
 *
 * VBVA mode is enabled by a registry key for the miniport driver
 * (as it is implemented now).
 *
 * VRDP mode is enabled when a VRDP client connects and VBVA is enabled.
 * The host sets a bit flag in VBVAMemory when VRDP client is connected.
 *
 * The VRDP mode pipeline consists of 3 types of commands:
 *
 * 1) RDP orders: BitBlt, RectFill, Text.
 *        These are the simpliest ones.
 *
 * 2) Caching: Bitmap, glyph, brush.
 *        The driver maintains a bitmap (or other objects) cache.
 *        All source bitmaps are cached. The driver verifies
 *        iUniq and also computes CRC for these bitmaps
 *        for searching. The driver will use SURFOBJ::dhsurf
 *        field to save a pointer to in driver structure, even
 *        for Engine managed bitmaps (hope that will work).
 *
 *
 * 3) Bitmap updates, when given draw operation can not be done
 *    using orders.
 *
 */

#include "driver.h"

#ifdef STAT_sunlover
void dumpsurf (SURFOBJ *pso, char *s)
{
    DISPDBG((1, "Surface %s: %p\n", s, pso));
    if (pso)
    {
        DISPDBG((1, "    DHSURF  dhsurf        = %p\n", pso->dhsurf));
        DISPDBG((1, "    HSURF   hsurf         = %p\n", pso->hsurf));
        DISPDBG((1, "    DHPDEV  dhpdev        = %p\n", pso->dhpdev));
        DISPDBG((1, "    HDEV    hdev          = %p\n", pso->hdev));
        DISPDBG((1, "    SIZEL   sizlBitmap    = %dx%d\n", pso->sizlBitmap.cx, pso->sizlBitmap.cy));
        DISPDBG((1, "    ULONG   cjBits        = %p\n", pso->cjBits));
        DISPDBG((1, "    PVOID   pvBits        = %p\n", pso->pvBits));
        DISPDBG((1, "    PVOID   pvScan0       = %p\n", pso->pvScan0));
        DISPDBG((1, "    LONG    lDelta        = %p\n", pso->lDelta));
        DISPDBG((1, "    ULONG   iUniq         = %p\n", pso->iUniq));
        DISPDBG((1, "    ULONG   iBitmapFormat = %p\n", pso->iBitmapFormat));
        DISPDBG((1, "    USHORT  iType         = %p\n", pso->iType));
        DISPDBG((1, "    USHORT  fjBitmap      = %p\n", pso->fjBitmap));
    }
}
#else
#define dumpsurf(a, b)
#endif /* STAT_sunlover */

BOOL bIsScreenSurface (SURFOBJ *pso)
{
    if (pso)
    {
        PPDEV ppdev = (PPDEV)pso->dhpdev;
        
        /* The screen surface has the 'pso->dhpdev' field, 
         * and is either the screen device surface with handle = hsurfScreen,
         * or a surface derived from DDRAW with address equal to the framebuffer.
         */
        if (   ppdev
            && (   pso->hsurf == ppdev->hsurfScreen
                || pso->pvBits == ppdev->pjScreen
               )
           )
        {
            return TRUE;
        }
    }

    return FALSE;
}

#define VBVA_OPERATION(__psoDest, __fn, __a) do {                     \
    if (bIsScreenSurface(__psoDest))                                  \
    {                                                                 \
        PPDEV ppdev = (PPDEV)__psoDest->dhpdev;                       \
                                                                      \
        if (ppdev->pInfo && vboxHwBufferBeginUpdate (ppdev))          \
        {                                                             \
            vbva##__fn __a;                                           \
                                                                      \
            if (  ppdev->pInfo->hostEvents.fu32Events                 \
                & VBOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET)           \
            {                                                         \
                vrdpReset (ppdev);                                    \
                                                                      \
                ppdev->pInfo->hostEvents.fu32Events &=                \
                          ~VBOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET;  \
            }                                                         \
                                                                      \
            if (ppdev->vbva.pVbvaMemory->fu32ModeFlags                \
                & VBVA_F_MODE_VRDP)                                   \
            {                                                         \
                vrdp##__fn __a;                                       \
            }                                                         \
                                                                      \
            vboxHwBufferEndUpdate (ppdev);                            \
        }                                                             \
    }                                                                 \
} while (0)

//#undef VBVA_OPERATION
//#define VBVA_OPERATION(_psoDest, __fn, __a) do { } while (0)


#if 0
typedef struct _SURFOBJ {
+00  DHSURF  dhsurf;
+04  HSURF  hsurf;
+08  DHPDEV  dhpdev;
+0c  HDEV  hdev;
+10  SIZEL  sizlBitmap;
+18  ULONG  cjBits;
+1c  PVOID  pvBits;
+20  PVOID  pvScan0;
+24  LONG  lDelta;
+28  ULONG  iUniq;
+2c  ULONG  iBitmapFormat;
+30  USHORT  iType;
+32  USHORT  fjBitmap;
} SURFOBJ;
#endif

BOOL APIENTRY DrvBitBlt(
    SURFOBJ  *psoTrg,
    SURFOBJ  *psoSrc,
    SURFOBJ  *psoMask,
    CLIPOBJ  *pco,
    XLATEOBJ *pxlo,
    RECTL    *prclTrg,
    POINTL   *pptlSrc,
    POINTL   *pptlMask,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrush,
    ROP4      rop4
    )
{
    BOOL bRc;

    DISPDBG((1, "%s\n", __FUNCTION__));

    DISPDBG((1, "psoTrg = %p, psoSrc = %p, psoMask = %p, pco = %p, pxlo = %p, prclTrg = %p, pptlSrc = %p, pptlMask = %p, pbo = %p, pptlBrush = %p, rop4 = %08X\n",
             psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg, pptlSrc, pptlMask, pbo, pptlBrush, rop4));

    STATDRVENTRY(BitBlt, psoTrg);

    bRc = EngBitBlt(CONV_SURF(psoTrg), CONV_SURF(psoSrc), psoMask, pco, pxlo, prclTrg, pptlSrc, pptlMask, pbo, pptlBrush, rop4);

    VBVA_OPERATION(psoTrg,
                   BitBlt,
                   (psoTrg, psoSrc, psoMask, pco, pxlo, prclTrg, pptlSrc, pptlMask, pbo, pptlBrush, rop4));

    return bRc;
}

BOOL APIENTRY DrvTextOut(
    SURFOBJ  *pso,
    STROBJ   *pstro,
    FONTOBJ  *pfo,
    CLIPOBJ  *pco,
    RECTL    *prclExtra,        // Obsolete, always NULL
    RECTL    *prclOpaque,
    BRUSHOBJ *pboFore,
    BRUSHOBJ *pboOpaque,
    POINTL   *pptlOrg,
    MIX       mix
    )
{
    BOOL bRc;

    DISPDBG((1, "%s\n", __FUNCTION__));

    STATDRVENTRY(TextOut, pso);

    bRc = EngTextOut(CONV_SURF(pso), pstro, pfo, pco, prclExtra, prclOpaque, pboFore, pboOpaque, pptlOrg, mix);

    VBVA_OPERATION(pso,
                   TextOut,
                   (pso, pstro, pfo, pco, prclExtra, prclOpaque, pboFore, pboOpaque, pptlOrg, mix));

    return bRc;
}


BOOL APIENTRY DrvLineTo(
    SURFOBJ   *pso,
    CLIPOBJ   *pco,
    BRUSHOBJ  *pbo,
    LONG       x1,
    LONG       y1,
    LONG       x2,
    LONG       y2,
    RECTL     *prclBounds,
    MIX        mix
    )
{
    BOOL bRc;

    DISPDBG((1, "%s\n", __FUNCTION__));

    STATDRVENTRY(LineTo, pso);

    bRc = EngLineTo(CONV_SURF(pso), pco, pbo, x1, y1, x2, y2, prclBounds, mix);

    VBVA_OPERATION(pso,
                   LineTo,
                   (pso, pco, pbo, x1, y1, x2, y2, prclBounds, mix));

    return bRc;
}

BOOL APIENTRY DrvStretchBlt(
    SURFOBJ         *psoDest,
    SURFOBJ         *psoSrc,
    SURFOBJ         *psoMask,
    CLIPOBJ         *pco,
    XLATEOBJ        *pxlo,
    COLORADJUSTMENT *pca,
    POINTL          *pptlHTOrg,
    RECTL           *prclDest,
    RECTL           *prclSrc,
    POINTL          *pptlMask,
    ULONG            iMode
    )
{
    BOOL bRc;

    DISPDBG((1, "%s\n", __FUNCTION__));

    STATDRVENTRY(StretchBlt, psoDest);

    bRc = EngStretchBlt(CONV_SURF(psoDest), CONV_SURF(psoSrc), psoMask, pco, pxlo, pca, pptlHTOrg,
                        prclDest, prclSrc, pptlMask, iMode);

    VBVA_OPERATION(psoDest,
                   StretchBlt,
                   (psoDest, psoSrc, psoMask, pco, pxlo, pca, pptlHTOrg,
                    prclDest, prclSrc, pptlMask, iMode));

    return bRc;
}


BOOL APIENTRY DrvCopyBits(
    SURFOBJ  *psoDest,
    SURFOBJ  *psoSrc,
    CLIPOBJ  *pco,
    XLATEOBJ *pxlo,
    RECTL    *prclDest,
    POINTL   *pptlSrc
    )
{
    RECTL rclDest = *prclDest;
    POINTL ptlSrc = *pptlSrc;

    BOOL bRc;
    BOOL bDo = TRUE;

    DISPDBG((1, "%s\n", __FUNCTION__));

    DISPDBG((1, "psoDest = %p, psoSrc = %p, pco = %p, pxlo = %p, prclDest = %p, pptlSrc = %p\n",
             psoDest, psoSrc, pco, pxlo, prclDest, pptlSrc));

    STATDRVENTRY(CopyBits, psoDest);

    dumpsurf(psoSrc, "psoSrc");
    dumpsurf(psoDest, "psoDest");

    STATPRINT;
    
#ifdef VBOX_VBVA_ADJUST_RECT
    /* Experimental fix for too large bitmap updates. 
     *
     * Some application do a large bitmap update event if only
     * a small part of the bitmap is actually changed.
     *
     * The driver will find the changed rectangle by comparing
     * the current framebuffer content with the source bitmap.
     *
     * The optimization is only active when: 
     *  - the VBVA extension is enabled;
     *  - the source bitmap is not cacheable;
     *  - the bitmap formats of both the source and the screen surfaces are equal.
     *
     */
    if (   psoSrc
        && !bIsScreenSurface(psoSrc)
        && bIsScreenSurface(psoDest))
    {
        PPDEV ppdev = (PPDEV)psoDest->dhpdev;

        VBVAMEMORY *pVbvaMemory = ppdev->vbva.pVbvaMemory;

        DISPDBG((1, "offscreen->screen\n"));

        if (   pVbvaMemory
            && (pVbvaMemory->fu32ModeFlags & VBVA_F_MODE_ENABLED))
        {
            if (   (psoSrc->fjBitmap & BMF_DONTCACHE) != 0
                || psoSrc->iUniq == 0)
            {
                DISPDBG((1, "non-cacheable %d->%d (ppdev %p)\n", psoSrc->iBitmapFormat, psoDest->iBitmapFormat, ppdev));

                /* It is possible to apply the fix. */
                bDo = vbvaFindChangedRect (CONV_SURF(psoDest), CONV_SURF(psoSrc), &rclDest, &ptlSrc);
            }
        }
    }

    if (!bDo)
    {
        /* The operation is a NOP. Just return success. */
        return TRUE;
    }
#endif /* VBOX_VBVA_ADJUST_RECT */

    bRc = EngCopyBits(CONV_SURF(psoDest), CONV_SURF(psoSrc), pco, pxlo, &rclDest, &ptlSrc);

    VBVA_OPERATION(psoDest,
                   CopyBits,
                   (psoDest, psoSrc, pco, pxlo, &rclDest, &ptlSrc));

    return bRc;
}

BOOL APIENTRY DrvPaint(
    SURFOBJ  *pso,
    CLIPOBJ  *pco,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrushOrg,
    MIX       mix
    )
{
    BOOL bRc;

    DISPDBG((1, "%s\n", __FUNCTION__));

    STATDRVENTRY(Paint, pso);

    bRc = EngPaint (CONV_SURF(pso), pco, pbo, pptlBrushOrg, mix);

    VBVA_OPERATION(pso,
                   Paint,
                   (pso, pco, pbo, pptlBrushOrg, mix));

    return bRc;
}

BOOL APIENTRY DrvFillPath(
    SURFOBJ  *pso,
    PATHOBJ  *ppo,
    CLIPOBJ  *pco,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrushOrg,
    MIX       mix,
    FLONG     flOptions
    )
{
    BOOL bRc;

    DISPDBG((1, "%s\n", __FUNCTION__));

    STATDRVENTRY(FillPath, pso);

    bRc = EngFillPath(CONV_SURF(pso), ppo, pco, pbo, pptlBrushOrg, mix, flOptions);

    VBVA_OPERATION(pso,
                   FillPath,
                   (pso, ppo, pco, pbo, pptlBrushOrg, mix, flOptions));

    return bRc;
}



BOOL APIENTRY DrvStrokePath(
    SURFOBJ   *pso,
    PATHOBJ   *ppo,
    CLIPOBJ   *pco,
    XFORMOBJ  *pxo,
    BRUSHOBJ  *pbo,
    POINTL    *pptlBrushOrg,
    LINEATTRS *plineattrs,
    MIX        mix
    )
{
    BOOL bRc;

    DISPDBG((1, "%s\n", __FUNCTION__));

    STATDRVENTRY(StrokePath, pso);

    bRc = EngStrokePath(CONV_SURF(pso), ppo, pco, pxo, pbo, pptlBrushOrg, plineattrs, mix);

    VBVA_OPERATION(pso,
                   StrokePath,
                   (pso, ppo, pco, pxo, pbo, pptlBrushOrg, plineattrs, mix));

    return bRc;
}

static void ssbDiscardTopSlot (PPDEV ppdev)
{
    SSB *pSSB = &ppdev->aSSB[--ppdev->cSSB];

    if (pSSB->pBuffer)
    {
        EngFreeMem (pSSB->pBuffer);
        pSSB->pBuffer = NULL;
    }

    pSSB->ident = 0;
}

static void ssbDiscardUpperSlots (PPDEV ppdev, ULONG_PTR ident)
{
    while (ppdev->cSSB > ident)
    {
        ssbDiscardTopSlot (ppdev);
    }
}

static BOOL ssbCopy (SSB *pSSB, SURFOBJ *pso, RECTL *prcl, BOOL bToScreen)
{
    BYTE *pSrc;
    BYTE *pDst;

    LONG lDeltaSrc;
    LONG lDeltaDst;

    ULONG cWidth;
    ULONG cHeight;

    int cbPixel = format2BytesPerPixel(pso);

    DISPDBG((1, "ssbCopy: pSSB = %p, pso = %p, prcl = %p, bToScreen = %d\n", pSSB, pso, prcl, bToScreen));

    if (cbPixel == 0)
    {
        DISPDBG((1, "ssbCopy: unsupported pixel format!!!\n"));
        return FALSE;
    }

    cWidth  = prcl->right - prcl->left;
    cHeight = prcl->bottom - prcl->top;

    if (bToScreen)
    {
        if (pSSB->pBuffer == NULL)
        {
            DISPDBG((1, "ssbCopy: source buffer is NULL!!!\n"));
            return FALSE;
        }

        pSrc = pSSB->pBuffer;
        lDeltaSrc = cWidth * cbPixel;

        pDst = (BYTE *)pso->pvScan0 +
                       pso->lDelta * prcl->top +
                       cbPixel * prcl->left;
        lDeltaDst  = pso->lDelta;
    }
    else
    {
        if (pSSB->pBuffer != NULL)
        {
            DISPDBG((1, "ssbCopy: source buffer is not NULL!!!\n"));
            return FALSE;
        }

        pSSB->pBuffer = (BYTE *)EngAllocMem (0, cWidth * cHeight * cbPixel, ALLOC_TAG);

        if (pSSB->pBuffer == NULL)
        {
            DISPDBG((1, "ssbCopy: Failed to allocate buffer!!!\n"));
            return FALSE;
        }

        pDst = pSSB->pBuffer;
        lDeltaDst = cWidth * cbPixel;

        pSrc = (BYTE *)pso->pvScan0 +
                       pso->lDelta * prcl->top +
                       cbPixel * prcl->left;
        lDeltaSrc  = pso->lDelta;
    }

    DISPDBG((1, "ssbCopy: cHeight = %d, pDst = %p, pSrc = %p, lDeltaSrc = %d, lDeltaDst = %d\n",
                 cHeight, pDst, pSrc, lDeltaSrc, lDeltaDst));

    while (cHeight--)
    {
        memcpy (pDst, pSrc, cWidth * cbPixel);

        pDst += lDeltaDst;
        pSrc += lDeltaSrc;
    }

    DISPDBG((1, "ssbCopy: completed.\n"));
    return TRUE;
}


ULONG_PTR APIENTRY DrvSaveScreenBits(
    SURFOBJ  *pso,
    ULONG    iMode,
    ULONG_PTR ident,
    RECTL    *prcl
    )
{
    ULONG_PTR rc = 0; /* 0 means the function failure for every iMode. */

    RECTL rcl;
    SSB *pSSB;

    SURFOBJ *psoOrg = pso;

    BOOL bCallVBVA = FALSE;

    PPDEV ppdev = (PPDEV)pso->dhpdev;

    DISPDBG((1, "%s: %p, %d, %d, %d,%d %d,%d\n",
                __FUNCTION__, pso, iMode, ident, prcl->left, prcl->top, prcl->right, prcl->bottom));

    if (!ppdev)
    {
        return rc;
    }

    pso = CONV_SURF(pso);

    /* Order the rectangle. */
    if (prcl->left <= prcl->right)
    {
        rcl.left = prcl->left;
        rcl.right = prcl->right;
    }
    else
    {
        rcl.left = prcl->right;
        rcl.right = prcl->left;
    }

    if (prcl->top <= prcl->bottom)
    {
        rcl.top = prcl->top;
        rcl.bottom = prcl->bottom;
    }
    else
    {
        rcl.top = prcl->bottom;
        rcl.bottom = prcl->top;
    }

    /* Implementation of the save/restore is a bit complicated because RDP
     * requires "the sequencing of saves and restores is such that they
     * behave as a last-in, first-out stack.".
     */
    switch (iMode)
    {
        case SS_SAVE:
        {
            DISPDBG((1, "DrvSaveScreenBits: SS_SAVE %d\n", ppdev->cSSB));

            if (ppdev->cSSB >= ELEMENTS(ppdev->aSSB))
            {
                /* All slots are already in use. Fail. */
                DISPDBG((1, "DrvSaveScreenBits: no more slots %d!!!\n", ppdev->cSSB));
                break;
            }

            /* Get pointer to the slot where bits will be saved. */
            pSSB = &ppdev->aSSB[ppdev->cSSB];

            /* Allocate memory for screen bits and copy them to the buffer. */
            if (ssbCopy (pSSB, pso, &rcl, FALSE /* bToScreen */))
            {
                /* Bits where successfully copied. Increase the active slot number
                 * and call VBVA levels, 'ident' is also assigned, the VBVA level
                 * will use it even for the SS_SAVE.
                 */
                ident = rc = pSSB->ident = ++ppdev->cSSB;
                bCallVBVA = TRUE;
            }
        } break;

        case SS_RESTORE:
        {
            DISPDBG((1, "DrvSaveScreenBits: SS_RESTORE\n"));

            if (   ppdev->cSSB == 0
                || ident == 0
                || ident > ppdev->cSSB)
            {
                DISPDBG((1, "DrvSaveScreenBits: no slot: ppdev->cSSB = %d!!!\n", ppdev->cSSB));
                break;
            }

            if (ident < ppdev->cSSB)
            {
                ssbDiscardUpperSlots (ppdev, ident);
            }

            VBVA_ASSERT(ident == ppdev->cSSB);
            VBVA_ASSERT(ident != 0);

            pSSB = &ppdev->aSSB[ident - 1];

            ssbCopy (pSSB, pso, &rcl, TRUE /* bToScreen */);

            /* Bits must be discarded. */
            ssbDiscardTopSlot (ppdev);

            rc = TRUE;
            bCallVBVA = TRUE;
        } break;

        case SS_FREE:
        {
            DISPDBG((1, "DrvSaveScreenBits: SS_FREE\n"));

            if (   ppdev->cSSB == 0
                || ident == 0
                || ident > ppdev->cSSB)
            {
                DISPDBG((1, "DrvSaveScreenBits: no slot: ppdev->cSSB = %d!!!\n", ppdev->cSSB));
                break;
            }

            if (ident < ppdev->cSSB)
            {
                ssbDiscardUpperSlots (ppdev, ident);
            }

            VBVA_ASSERT(ident == ppdev->cSSB);
            VBVA_ASSERT(ident != 0);

            /* Bits must be discarded. */
            ssbDiscardTopSlot (ppdev);

            rc = TRUE;
        } break;
    }

    /* Now call the VBVA/VRDP levels. */
    if (bCallVBVA)
    {
        DISPDBG((1, "DrvSaveScreenBits: calling VBVA\n"));
        VBVA_OPERATION(psoOrg,
                       SaveScreenBits,
                       (psoOrg, iMode, ident, &rcl));
    }

    return rc;
}

BOOL APIENTRY DrvRealizeBrush(
    BRUSHOBJ *pbo,
    SURFOBJ  *psoTarget,
    SURFOBJ  *psoPattern,
    SURFOBJ  *psoMask,
    XLATEOBJ *pxlo,
    ULONG    iHatch
    )
{
    BOOL bRc = FALSE;

    DISPDBG((1, "%s\n", __FUNCTION__));

    if (bIsScreenSurface(psoTarget))
    {
        PPDEV ppdev = (PPDEV)psoTarget->dhpdev;

        if (   ppdev->vbva.pVbvaMemory
            && (ppdev->vbva.pVbvaMemory->fu32ModeFlags & VBVA_F_MODE_ENABLED))
        {
            if (ppdev->vbva.pVbvaMemory->fu32ModeFlags
                & VBVA_F_MODE_VRDP_RESET)
            {
                vrdpReset (ppdev);

                ppdev->vbva.pVbvaMemory->fu32ModeFlags &=
                    ~VBVA_F_MODE_VRDP_RESET;
            }

            if (ppdev->vbva.pVbvaMemory->fu32ModeFlags
                & VBVA_F_MODE_VRDP)
            {
                bRc = vrdpRealizeBrush (pbo, psoTarget, psoPattern, psoMask, pxlo, iHatch);
            }
        }
    }

    return bRc;
}

#ifdef STAT_sunlover
ULONG gStatCopyBitsOffscreenToScreen = 0;
ULONG gStatCopyBitsScreenToScreen = 0;
ULONG gStatBitBltOffscreenToScreen = 0;
ULONG gStatBitBltScreenToScreen = 0;
ULONG gStatUnchangedOffscreenToScreen = 0;
ULONG gStatUnchangedOffscreenToScreenCRC = 0;
ULONG gStatNonTransientEngineBitmaps = 0;
ULONG gStatTransientEngineBitmaps = 0;
ULONG gStatUnchangedBitmapsCRC = 0;
ULONG gStatUnchangedBitmapsDeviceCRC = 0;
ULONG gStatBitmapsCRC = 0;
ULONG gStatBitBltScreenPattern = 0;
ULONG gStatBitBltScreenSquare = 0;
ULONG gStatBitBltScreenPatternReported = 0;
ULONG gStatBitBltScreenSquareReported = 0;
ULONG gStatCopyBitsScreenSquare = 0;

ULONG gStatEnablePDEV = 0;
ULONG gStatCompletePDEV = 0;
ULONG gStatDisablePDEV = 0;
ULONG gStatEnableSurface = 0;
ULONG gStatDisableSurface = 0;
ULONG gStatAssertMode = 0;
ULONG gStatDisableDriver = 0;
ULONG gStatCreateDeviceBitmap = 0;
ULONG gStatDeleteDeviceBitmap = 0;
ULONG gStatDitherColor = 0;
ULONG gStatStrokePath = 0;
ULONG gStatFillPath = 0;
ULONG gStatStrokeAndFillPath = 0;
ULONG gStatPaint = 0;
ULONG gStatBitBlt = 0;
ULONG gStatCopyBits = 0;
ULONG gStatStretchBlt = 0;
ULONG gStatSetPalette = 0;
ULONG gStatTextOut = 0;
ULONG gStatSetPointerShape = 0;
ULONG gStatMovePointer = 0;
ULONG gStatLineTo = 0;
ULONG gStatSynchronize = 0;
ULONG gStatGetModes = 0;
ULONG gStatGradientFill = 0;
ULONG gStatStretchBltROP = 0;
ULONG gStatPlgBlt = 0;
ULONG gStatAlphaBlend = 0;
ULONG gStatTransparentBlt = 0;

void statPrint (void)
{
    DISPDBG((0,
             "BMPSTAT:\n"
             "    gStatCopyBitsOffscreenToScreen = %u\n"
             "    gStatCopyBitsScreenToScreen = %u\n"
             "    gStatBitBltOffscreenToScreen = %u\n"
             "    gStatBitBltScreenToScreen = %u\n"
             "    gStatUnchangedOffscreenToScreen = %u\n"
             "    gStatUnchangedOffscreenToScreenCRC = %u\n"
             "    gStatNonTransientEngineBitmaps = %u\n"
             "    gStatTransientEngineBitmaps = %u\n"
             "    gStatUnchangedBitmapsCRC = %u\n"
             "    gStatUnchangedBitmapsDeviceCRC = %u\n"
             "    gStatBitmapsCRC = %u\n"
             "    gStatBitBltScreenPattern = %u\n"
             "    gStatBitBltScreenSquare = %u\n"
             "    gStatBitBltScreenPatternReported = %u\n"
             "    gStatBitBltScreenSquareReported = %u\n"
             "    gStatCopyBitsScreenSquare = %u\n"
             "\n"
             "    gStatEnablePDEV = %u\n"
             "    gStatCompletePDEV = %u\n"
             "    gStatDisablePDEV = %u\n"
             "    gStatEnableSurface = %u\n"
             "    gStatDisableSurface = %u\n"
             "    gStatAssertMode = %u\n"
             "    gStatDisableDriver = %u\n"
             "    gStatCreateDeviceBitmap = %u\n"
             "    gStatDeleteDeviceBitmap = %u\n"
             "    gStatDitherColor = %u\n"
             "    gStatStrokePath = %u\n"
             "    gStatFillPath = %u\n"
             "    gStatStrokeAndFillPath = %u\n"
             "    gStatPaint = %u\n"
             "    gStatBitBlt = %u\n"
             "    gStatCopyBits = %u\n"
             "    gStatStretchBlt = %u\n"
             "    gStatSetPalette = %u\n"
             "    gStatTextOut = %u\n"
             "    gStatSetPointerShape = %u\n"
             "    gStatMovePointer = %u\n"
             "    gStatLineTo = %u\n"
             "    gStatSynchronize = %u\n"
             "    gStatGetModes = %u\n"
             "    gStatGradientFill = %u\n"
             "    gStatStretchBltROP = %u\n"
             "    gStatPlgBlt = %u\n"
             "    gStatAlphaBlend = %u\n"
             "    gStatTransparentBlt = %u\n",
             gStatCopyBitsOffscreenToScreen,
             gStatCopyBitsScreenToScreen,
             gStatBitBltOffscreenToScreen,
             gStatBitBltScreenToScreen,
             gStatUnchangedOffscreenToScreen,
             gStatUnchangedOffscreenToScreenCRC,
             gStatNonTransientEngineBitmaps,
             gStatTransientEngineBitmaps,
             gStatUnchangedBitmapsCRC,
             gStatUnchangedBitmapsDeviceCRC,
             gStatBitmapsCRC,
             gStatBitBltScreenPattern,
             gStatBitBltScreenSquare,
             gStatBitBltScreenPatternReported,
             gStatBitBltScreenSquareReported,
             gStatCopyBitsScreenSquare,
             gStatEnablePDEV,
             gStatCompletePDEV,
             gStatDisablePDEV,
             gStatEnableSurface,
             gStatDisableSurface,
             gStatAssertMode,
             gStatDisableDriver,
             gStatCreateDeviceBitmap,
             gStatDeleteDeviceBitmap,
             gStatDitherColor,
             gStatStrokePath,
             gStatFillPath,
             gStatStrokeAndFillPath,
             gStatPaint,
             gStatBitBlt,
             gStatCopyBits,
             gStatStretchBlt,
             gStatSetPalette,
             gStatTextOut,
             gStatSetPointerShape,
             gStatMovePointer,
             gStatLineTo,
             gStatSynchronize,
             gStatGetModes,
             gStatGradientFill,
             gStatStretchBltROP,
             gStatPlgBlt,
             gStatAlphaBlend,
             gStatTransparentBlt
           ));
}
#endif /* STAT_sunlover */

