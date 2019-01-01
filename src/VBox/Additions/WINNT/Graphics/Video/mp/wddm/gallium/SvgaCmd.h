/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - VMSVGA command encoders.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_SvgaCmd_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_SvgaCmd_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "SvgaHw.h"

void SvgaCmdDefineScreen(void *pvCmd, uint32_t u32Id, bool fActivate,
                         int32_t xOrigin, int32_t yOrigin, uint32_t u32Width, uint32_t u32Height,
                         bool fPrimary, uint32_t u32VRAMOffset, bool fBlank);
void SvgaCmdDestroyScreen(void *pvCmd, uint32_t u32Id);
void SvgaCmdUpdate(void *pvCmd, uint32_t u32X, uint32_t u32Y, uint32_t u32Width, uint32_t u32Height);
void SvgaCmdFence(void *pvCmd, uint32_t u32Fence);
void SvgaCmdDefineGMRFB(void *pvCmd, uint32_t u32Offset, uint32_t u32BytesPerLine);

void Svga3dCmdDefineContext(void *pvCmd, uint32_t u32Cid);
void Svga3dCmdDestroyContext(void *pvCmd, uint32_t u32Cid);

void SvgaCmdDefineGMR2(void *pvCmd, uint32_t u32GmrId, uint32_t cPages);
void SvgaCmdRemapGMR2(void *pvCmd, uint32_t u32GmrId, SVGARemapGMR2Flags flags, uint32 offsetPages, uint32_t numPages);

void Svga3dCmdPresent(void *pvCmd, uint32_t u32Sid, uint32_t u32Width, uint32_t u32Height);

void Svga3dCmdDefineSurface(void *pvCmd, uint32_t sid, GASURFCREATE const *pCreateParms,
                            GASURFSIZE const *paSizes, uint32_t cSizes);
void Svga3dCmdDestroySurface(void *pvCmd, uint32_t u32Sid);

void Svga3dCmdSurfaceDMAToFB(void *pvCmd, uint32_t u32Sid, uint32_t u32Width, uint32_t u32Height, uint32_t u32Offset);

void Svga3dCmdSurfaceDMA(void *pvCmd, SVGAGuestImage const *pGuestImage, SVGA3dSurfaceImageId const *pSurfId,
                         SVGA3dTransferType enmTransferType, uint32_t xSrc, uint32_t ySrc,
                         uint32_t xDst, uint32_t yDst, uint32_t cWidth, uint32_t cHeight);

void SvgaCmdBlitGMRFBToScreen(void *pvCmd, uint32_t idDstScreen, int32_t xSrc, int32_t ySrc,
                              int32_t xLeft, int32_t yTop, int32_t xRight, int32_t yBottom);
void SvgaCmdBlitScreenToGMRFB(void *pvCmd, uint32_t idSrcScreen, int32_t xSrc, int32_t ySrc,
                              int32_t xLeft, int32_t yTop, int32_t xRight, int32_t yBottom);

void Svga3dCmdBlitSurfaceToScreen(void *pvCmd, uint32_t sid,
                                  RECT const *pSrcRect,
                                  uint32_t idDstScreen,
                                  RECT const *pDstRect,
                                  uint32_t cDstClipRects,
                                  RECT const *paDstClipRects);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_SvgaCmd_h */
