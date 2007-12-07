/** @file
 *
 * VirtualBox Windows NT/2000/XP guest video driver
 *
 * VBVA dirty rectangles calculations.
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "driver.h"

#ifdef VBOX_VBVA_ADJUST_RECT
static ULONG vbvaConvertPixel ( /*from*/ BYTE *pu8PixelFrom, int cbPixelFrom,
                                /*to*/ int cbPixelTo)
{
    BYTE r, g, b;
    ULONG ulConvertedPixel = 0;

    switch (cbPixelFrom)
    {
        case 4:
        {
           switch (cbPixelTo)
           {
               case 3:
               {
                   memcpy (&ulConvertedPixel, pu8PixelFrom, 3);
               } break;

               case 2:
               {
                   ulConvertedPixel = *(ULONG *)pu8PixelFrom;

                   r = (BYTE)(ulConvertedPixel >> 16);
                   g = (BYTE)(ulConvertedPixel >> 8);
                   b = (BYTE)(ulConvertedPixel);

                   ulConvertedPixel = ((r >> 3) << 11) + ((g >> 2) << 5) + (b >> 3);
               } break;
           }
        } break;

        case 3:
        {
           switch (cbPixelTo)
           {
               case 2:
               {
                   memcpy (&ulConvertedPixel, pu8PixelFrom, 3);

                   r = (BYTE)(ulConvertedPixel >> 16);
                   g = (BYTE)(ulConvertedPixel >> 8);
                   b = (BYTE)(ulConvertedPixel);

                   ulConvertedPixel = ((r >> 3) << 11) + ((g >> 2) << 5) + (b >> 3);
               } break;
           }
        } break;
    }

    return ulConvertedPixel;
}

BOOL vbvaFindChangedRect (SURFOBJ *psoDest, SURFOBJ *psoSrc, RECTL *prclDest, POINTL *pptlSrc)
{
    int x, y;
    int fTopNonEqualFound;
    int yTopmost;
    int yBottommost;
    int cbPixelSrc;
    int cbPixelDest;
    RECTL rclDest;
    RECTL rclSrc;
    BYTE *pu8Src;
    BYTE *pu8Dest;
    
    if (!prclDest || !pptlSrc)
    {
        return TRUE;
    }
    
    DISPDBG((1, "vbvaFindChangedRect: dest %d,%d %dx%d from %d,%d\n",
             prclDest->left, prclDest->top, prclDest->right - prclDest->left, prclDest->bottom - prclDest->top,
             pptlSrc->x, pptlSrc->y
            ));
            
    switch (psoDest->iBitmapFormat)
    {
        case BMF_16BPP: cbPixelDest = 2; break;
        case BMF_24BPP: cbPixelDest = 3; break;
        case BMF_32BPP: cbPixelDest = 4; break;
        default: cbPixelDest = 0;
    }

    switch (psoSrc->iBitmapFormat)
    {
        case BMF_16BPP: cbPixelSrc = 2; break;
        case BMF_24BPP: cbPixelSrc = 3; break;
        case BMF_32BPP: cbPixelSrc = 4; break;
        default: cbPixelSrc = 0;
    }

    if (cbPixelDest == 0 || cbPixelSrc == 0)
    {
        DISPDBG((1, "vbvaFindChangedRect: unsupported pixel format src %d dst %d\n", psoDest->iBitmapFormat, psoSrc->iBitmapFormat));
        return TRUE;
    }
    
    rclDest = *prclDest;
    
    vrdpAdjustRect (psoDest, &rclDest);
    
    pptlSrc->x += rclDest.left - prclDest->left;
    pptlSrc->y += rclDest.top - prclDest->top;
    
    *prclDest = rclDest;
    
    if (   rclDest.right == rclDest.left
        || rclDest.bottom == rclDest.top)
    {
        DISPDBG((1, "vbvaFindChangedRect: empty dest rect: %d-%d, %d-%d\n", rclDest.left, rclDest.right, rclDest.top, rclDest.bottom));
        return FALSE;
    }
    
    rclSrc.left   = pptlSrc->x;
    rclSrc.top    = pptlSrc->y;
    rclSrc.right  = pptlSrc->x + (rclDest.right - rclDest.left);
    rclSrc.bottom = pptlSrc->y + (rclDest.bottom - rclDest.top);
    vrdpAdjustRect (psoSrc, &rclSrc);
    
    if (   rclSrc.right == rclSrc.left
        || rclSrc.bottom == rclSrc.top)
    {
         prclDest->right = prclDest->left;
         prclDest->bottom = prclDest->top;
         
         DISPDBG((1, "vbvaFindChangedRect: empty src rect: %d-%d, %d-%d\n", rclSrc.left, rclSrc.right, rclSrc.top, rclSrc.bottom));
         return FALSE;
    }
    
    VBVA_ASSERT(pptlSrc->x == rclSrc.left);
    VBVA_ASSERT(pptlSrc->y == rclSrc.top);
    
    /*
     * Compare the content of the screen surface (psoDest) with the source surface (psoSrc).
     * Update the prclDest with the rectangle that will be actually changed after
     * copying the source bits to the screen.
     */
    pu8Src = (BYTE *)psoSrc->pvScan0 + psoSrc->lDelta * pptlSrc->y + cbPixelSrc * pptlSrc->x;
    pu8Dest = (BYTE *)psoDest->pvScan0 + psoDest->lDelta * prclDest->top + cbPixelDest * prclDest->left;
    
    /* Use the rclDest as the bounding rectangle for the changed area. */
    rclDest.left   = prclDest->right;  /* +inf */
    rclDest.right  = prclDest->left;   /* -inf */
    rclDest.top    = prclDest->bottom; /* +inf */
    rclDest.bottom = prclDest->top;    /* -inf */
    
    fTopNonEqualFound = 0;
    yTopmost = prclDest->top;        /* inclusive */
    yBottommost = prclDest->top - 1; /* inclusive */
    
    for (y = prclDest->top; y < prclDest->bottom; y++)
    {
        int fLeftNonEqualFound = 0;
        
        /* Init to an empty line. */
        int xLeftmost = prclDest->left;      /* inclusive */
        int xRightmost = prclDest->left - 1; /* inclusive */
        
        BYTE *pu8SrcLine = pu8Src;
        BYTE *pu8DestLine = pu8Dest;
        
        for (x = prclDest->left; x < prclDest->right; x++)
        {
            int fEqualPixels;
            
            if (cbPixelSrc == cbPixelDest)
            {
                fEqualPixels = (memcmp (pu8SrcLine, pu8DestLine, cbPixelDest) == 0);
            }
            else
            {
                /* Convert larger pixel to the smaller pixel format. */
                ULONG ulConvertedPixel;
                if (cbPixelSrc > cbPixelDest)
                {
                    /* Convert the source pixel to the destination pixel format. */
                    ulConvertedPixel = vbvaConvertPixel ( /*from*/ pu8SrcLine, cbPixelSrc,
                                                          /*to*/ cbPixelDest);
                    fEqualPixels = (memcmp (&ulConvertedPixel, pu8DestLine, cbPixelDest) == 0);
                }
                else
                {
                    /* Convert the destination pixel to the source pixel format. */
                    ulConvertedPixel = vbvaConvertPixel ( /*from*/ pu8DestLine, cbPixelDest,
                                                          /*to*/ cbPixelSrc);
                    fEqualPixels = (memcmp (&ulConvertedPixel, pu8SrcLine, cbPixelSrc) == 0);
                }
            }
            
            if (fEqualPixels)
            {
                /* Equal pixels. */
                if (!fLeftNonEqualFound)
                {
                    xLeftmost = x;
                }
            }
            else
            {
                fLeftNonEqualFound = 1;
                xRightmost = x;
            }
            
            pu8SrcLine += cbPixelSrc;
            pu8DestLine += cbPixelDest;
        }
        
        /* min */
        if (rclDest.left > xLeftmost)
        {
            rclDest.left = xLeftmost;
        }
        
        /* max */
        if (rclDest.right < xRightmost)
        {
            rclDest.right = xRightmost;
        }
        
        if (xLeftmost > xRightmost) /* xRightmost is inclusive, so '>', not '>='. */
        {
            /* Empty line. */
            if (!fTopNonEqualFound)
            {
                yTopmost = y;
            }
        }
        else
        {
            fTopNonEqualFound = 1;
            yBottommost = y;
        }
        
        pu8Src += psoSrc->lDelta;
        pu8Dest += psoDest->lDelta;
    }
    
    /* min */
    if (rclDest.top > yTopmost)
    {
        rclDest.top = yTopmost;
    }
        
    /* max */
    if (rclDest.bottom < yBottommost)
    {
        rclDest.bottom = yBottommost;
    }
        
    /* rclDest was calculated with right-bottom inclusive.
     * The following checks and the caller require exclusive coords.
     */
    rclDest.right++;
    rclDest.bottom++;
    
    DISPDBG((1, "vbvaFindChangedRect: new dest %d,%d %dx%d from %d,%d\n",
             rclDest.left, rclDest.top, rclDest.right - rclDest.left, rclDest.bottom - rclDest.top,
             pptlSrc->x, pptlSrc->y
            ));
            
    /* Update the rectangle with the changed area. */
    if (   rclDest.left >= rclDest.right
        || rclDest.top >= rclDest.bottom)
    {
        /* Empty rect. */
        DISPDBG((1, "vbvaFindChangedRect: empty\n"));
        prclDest->right = prclDest->left;
        prclDest->bottom = prclDest->top;
        return FALSE;
    }
    
    DISPDBG((1, "vbvaFindChangedRect: not empty\n"));
    
    pptlSrc->x += rclDest.left - prclDest->left;
    pptlSrc->y += rclDest.top - prclDest->top;
    
    *prclDest = rclDest;
    
    return TRUE;
}
#endif /* VBOX_VBVA_ADJUST_RECT */

void vboxReportDirtyRect (PPDEV ppdev, RECTL *pRectOrig)
{
    if (ppdev)
    {
        VBVACMDHDR hdr;
        
        RECTL rect = *pRectOrig;

        if (rect.left < 0) rect.left = 0;
        if (rect.top < 0) rect.top = 0;
        if (rect.right > (int)ppdev->cxScreen) rect.right = ppdev->cxScreen;
        if (rect.bottom > (int)ppdev->cyScreen) rect.bottom = ppdev->cyScreen;

        hdr.x = (int16_t)rect.left;
        hdr.y = (int16_t)rect.top;
        hdr.w = (uint16_t)(rect.right - rect.left);
        hdr.h = (uint16_t)(rect.bottom - rect.top);

        hdr.x += (int16_t)ppdev->ptlDevOrg.x;
        hdr.y += (int16_t)ppdev->ptlDevOrg.y;

        vboxWrite (ppdev, &hdr, sizeof(hdr));
    }

    return;
}

void vbvaReportDirtyRect (PPDEV ppdev, RECTL *prcl)
{
    if (prcl)
    {
        DISPDBG((1, "DISP VBVA dirty rect: left %d, top: %d, width: %d, height: %d\n",
                 prcl->left, prcl->top, prcl->right - prcl->left, prcl->bottom - prcl->top));

        vboxReportDirtyRect(ppdev, prcl);
    }
}

void vbvaReportDirtyPath (PPDEV ppdev, PATHOBJ *ppo)
{
    RECTFX rcfxBounds;
    RECTL rclBounds;

    PATHOBJ_vGetBounds(ppo, &rcfxBounds);

    rclBounds.left   = FXTOLFLOOR(rcfxBounds.xLeft);
    rclBounds.right  = FXTOLCEILING(rcfxBounds.xRight);
    rclBounds.top    = FXTOLFLOOR(rcfxBounds.yTop);
    rclBounds.bottom = FXTOLCEILING(rcfxBounds.yBottom);

    vbvaReportDirtyRect (ppdev, &rclBounds);
}

__inline void vbvaReportDirtyClip (PPDEV ppdev, CLIPOBJ *pco, RECTL *prcl)
{
    if (prcl)
    {
        vbvaReportDirtyRect (ppdev, prcl);
    }
    else if (pco)
    {
        vbvaReportDirtyRect (ppdev, &pco->rclBounds);
    }
}


void vbvaBitBlt (
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
    ROP4      rop4)
{
    PPDEV ppdev = (PPDEV)psoTrg->dhpdev;

    vbvaReportDirtyClip (ppdev, pco, prclTrg);
}

void vbvaTextOut(
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
    PPDEV ppdev = (PPDEV)pso->dhpdev;

    vbvaReportDirtyClip (ppdev, pco, prclOpaque? prclOpaque: &pstro->rclBkGround);
}

void vbvaLineTo(
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
    PPDEV ppdev = (PPDEV)pso->dhpdev;

    vbvaReportDirtyClip (ppdev, pco, prclBounds);
}

void vbvaStretchBlt(
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
    PPDEV ppdev = (PPDEV)psoDest->dhpdev;

    vbvaReportDirtyClip (ppdev, pco, prclDest);
}

void vbvaCopyBits(
    SURFOBJ  *psoDest,
    SURFOBJ  *psoSrc,
    CLIPOBJ  *pco,
    XLATEOBJ *pxlo,
    RECTL    *prclDest,
    POINTL   *pptlSrc
    )
{
    PPDEV ppdev = (PPDEV)psoDest->dhpdev;

    vbvaReportDirtyClip (ppdev, pco, prclDest);
}

void vbvaPaint(
    SURFOBJ  *pso,
    CLIPOBJ  *pco,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrushOrg,
    MIX       mix
    )
{
    PPDEV ppdev = (PPDEV)pso->dhpdev;

    vbvaReportDirtyClip (ppdev, pco, NULL);
}

void vbvaFillPath(
    SURFOBJ  *pso,
    PATHOBJ  *ppo,
    CLIPOBJ  *pco,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrushOrg,
    MIX       mix,
    FLONG     flOptions
    )
{
    PPDEV ppdev = (PPDEV)pso->dhpdev;

    vbvaReportDirtyPath (ppdev, ppo);
}

void vbvaStrokePath(
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
    PPDEV ppdev = (PPDEV)pso->dhpdev;

    vbvaReportDirtyPath (ppdev, ppo);
}

void vbvaSaveScreenBits(
    SURFOBJ  *pso,
    ULONG    iMode,
    ULONG_PTR ident,
    RECTL    *prcl
    )
{
    PPDEV ppdev = (PPDEV)pso->dhpdev;

    VBVA_ASSERT(iMode == SS_RESTORE || iMode == SS_SAVE);

    vbvaReportDirtyRect (ppdev, prcl);
}

