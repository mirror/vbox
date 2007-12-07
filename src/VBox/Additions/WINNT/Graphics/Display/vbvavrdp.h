/** @file
 *
 * VBoxGuest -- VirtualBox Win 2000/XP guest display driver
 *
 * VRDP and VBVA handlers header.
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

#ifndef __DISPLAY_VBVAVRDP__H
#define __DISPLAY_VBVAVRDP__H


#define VBVA_DECL_OP(__fn, __args) \
    void vbva##__fn __args;        \
    void vrdp##__fn __args;

VBVA_DECL_OP(BitBlt, (                 \
    SURFOBJ  *psoTrg,                  \
    SURFOBJ  *psoSrc,                  \
    SURFOBJ  *psoMask,                 \
    CLIPOBJ  *pco,                     \
    XLATEOBJ *pxlo,                    \
    RECTL    *prclTrg,                 \
    POINTL   *pptlSrc,                 \
    POINTL   *pptlMask,                \
    BRUSHOBJ *pbo,                     \
    POINTL   *pptlBrush,               \
    ROP4      rop4                     \
    ));

VBVA_DECL_OP(TextOut, (                \
    SURFOBJ  *pso,                     \
    STROBJ   *pstro,                   \
    FONTOBJ  *pfo,                     \
    CLIPOBJ  *pco,                     \
    RECTL    *prclExtra,               \
    RECTL    *prclOpaque,              \
    BRUSHOBJ *pboFore,                 \
    BRUSHOBJ *pboOpaque,               \
    POINTL   *pptlOrg,                 \
    MIX       mix                      \
    ));

VBVA_DECL_OP(LineTo, (                 \
    SURFOBJ   *pso,                    \
    CLIPOBJ   *pco,                    \
    BRUSHOBJ  *pbo,                    \
    LONG       x1,                     \
    LONG       y1,                     \
    LONG       x2,                     \
    LONG       y2,                     \
    RECTL     *prclBounds,             \
    MIX        mix                     \
    ));

VBVA_DECL_OP(StretchBlt, (             \
    SURFOBJ         *psoDest,          \
    SURFOBJ         *psoSrc,           \
    SURFOBJ         *psoMask,          \
    CLIPOBJ         *pco,              \
    XLATEOBJ        *pxlo,             \
    COLORADJUSTMENT *pca,              \
    POINTL          *pptlHTOrg,        \
    RECTL           *prclDest,         \
    RECTL           *prclSrc,          \
    POINTL          *pptlMask,         \
    ULONG            iMode             \
    ));

VBVA_DECL_OP(CopyBits, (               \
    SURFOBJ  *psoDest,                 \
    SURFOBJ  *psoSrc,                  \
    CLIPOBJ  *pco,                     \
    XLATEOBJ *pxlo,                    \
    RECTL    *prclDest,                \
    POINTL   *pptlSrc                  \
    ));

VBVA_DECL_OP(Paint, (                  \
    SURFOBJ  *pso,                     \
    CLIPOBJ  *pco,                     \
    BRUSHOBJ *pbo,                     \
    POINTL   *pptlBrushOrg,            \
    MIX       mix                      \
    ));

VBVA_DECL_OP(FillPath, (               \
    SURFOBJ  *pso,                     \
    PATHOBJ  *ppo,                     \
    CLIPOBJ  *pco,                     \
    BRUSHOBJ *pbo,                     \
    POINTL   *pptlBrushOrg,            \
    MIX       mix,                     \
    FLONG     flOptions                \
    ));

VBVA_DECL_OP(StrokePath, (             \
    SURFOBJ   *pso,                    \
    PATHOBJ   *ppo,                    \
    CLIPOBJ   *pco,                    \
    XFORMOBJ  *pxo,                    \
    BRUSHOBJ  *pbo,                    \
    POINTL    *pptlBrushOrg,           \
    LINEATTRS *plineattrs,             \
    MIX        mix                     \
    ));

VBVA_DECL_OP(SaveScreenBits, (         \
    SURFOBJ  *pso,                     \
    ULONG    iMode,                    \
    ULONG_PTR ident,                   \
    RECTL    *prcl                     \
    ))

#undef VBVA_DECL_OP

BOOL vrdpRealizeBrush(
    BRUSHOBJ *pbo,
    SURFOBJ  *psoTarget,
    SURFOBJ  *psoPattern,
    SURFOBJ  *psoMask,
    XLATEOBJ *pxlo,
    ULONG    iHatch
    );

void vrdpReset (PPDEV ppdev);

#endif /* __DISPLAY_VBVAVRDP__H */
