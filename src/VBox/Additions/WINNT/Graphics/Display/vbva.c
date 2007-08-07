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
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "driver.h"

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

    vbvaReportDirtyRect (ppdev, prclTrg);
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

