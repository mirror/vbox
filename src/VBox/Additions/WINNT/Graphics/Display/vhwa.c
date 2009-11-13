/* $Id$ */
/** @file
 * VBoxGuest -- VirtualBox Win 2000/XP guest display driver
 *
 * Video HW Acceleration support functions.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include "dd.h"
#undef CO_E_NOTINITIALIZED
#include <winerror.h>
#include <iprt/asm.h>

#define MEMTAG 'AWHV'
PVBOXVHWASURFDESC vboxVHWASurfDescAlloc()
{
    return EngAllocMem(FL_NONPAGED_MEMORY | FL_ZERO_MEMORY, sizeof(VBOXVHWASURFDESC), MEMTAG);
}

void vboxVHWASurfDescFree(PVBOXVHWASURFDESC pDesc)
{
    EngFreeMem(pDesc);
}

#define VBOX_DD(_f) DD##_f
#define VBOX_VHWA(_f) VBOXVHWA_##_f
#define VBOX_DD2VHWA(_out, _in, _f) do {if((_in) & VBOX_DD(_f)) _out |= VBOX_VHWA(_f); }while(0)
#define VBOX_DD_VHWA_PAIR(_v) {VBOX_DD(_v), VBOX_VHWA(_v)}
#define VBOX_DD_DUMMY_PAIR(_v) {VBOX_DD(_v), 0}

#define VBOXVHWA_SUPPORTED_CAPS ( \
        VBOXVHWA_CAPS_BLT \
        | VBOXVHWA_CAPS_BLTCOLORFILL \
        | VBOXVHWA_CAPS_BLTFOURCC \
        | VBOXVHWA_CAPS_BLTSTRETCH \
        | VBOXVHWA_CAPS_BLTQUEUE \
        | VBOXVHWA_CAPS_OVERLAY \
        | VBOXVHWA_CAPS_OVERLAYFOURCC \
        | VBOXVHWA_CAPS_OVERLAYSTRETCH \
        | VBOXVHWA_CAPS_OVERLAYCANTCLIP \
        | VBOXVHWA_CAPS_COLORKEY \
        | VBOXVHWA_CAPS_COLORKEYHWASSIST \
        )

#define VBOXVHWA_SUPPORTED_SCAPS ( \
        VBOXVHWA_SCAPS_BACKBUFFER \
        | VBOXVHWA_SCAPS_COMPLEX \
        | VBOXVHWA_SCAPS_FLIP \
        | VBOXVHWA_SCAPS_FRONTBUFFER \
        | VBOXVHWA_SCAPS_OFFSCREENPLAIN \
        | VBOXVHWA_SCAPS_OVERLAY \
        | VBOXVHWA_SCAPS_PRIMARYSURFACE \
        | VBOXVHWA_SCAPS_SYSTEMMEMORY \
        | VBOXVHWA_SCAPS_VIDEOMEMORY \
        | VBOXVHWA_SCAPS_VISIBLE \
        | VBOXVHWA_SCAPS_LOCALVIDMEM \
        )

#define VBOXVHWA_SUPPORTED_SCAPS2 ( \
        VBOXVHWA_CAPS2_CANRENDERWINDOWED \
        | VBOXVHWA_CAPS2_WIDESURFACES \
        | VBOXVHWA_CAPS2_COPYFOURCC \
        )

#define VBOXVHWA_SUPPORTED_PF ( \
        VBOXVHWA_PF_RGB \
        | VBOXVHWA_PF_RGBTOYUV \
        | VBOXVHWA_PF_YUV \
        | VBOXVHWA_PF_FOURCC \
        )

#define VBOXVHWA_SUPPORTED_SD ( \
        VBOXVHWA_SD_BACKBUFFERCOUNT \
        | VBOXVHWA_SD_CAPS \
        | VBOXVHWA_SD_CKDESTBLT \
        | VBOXVHWA_SD_CKDESTOVERLAY \
        | VBOXVHWA_SD_CKSRCBLT \
        | VBOXVHWA_SD_CKSRCOVERLAY \
        | VBOXVHWA_SD_HEIGHT \
        | VBOXVHWA_SD_PITCH \
        | VBOXVHWA_SD_PIXELFORMAT \
        | VBOXVHWA_SD_WIDTH \
        )

#define VBOXVHWA_SUPPORTED_CKEYCAPS ( \
        VBOXVHWA_CKEYCAPS_DESTBLT \
        | VBOXVHWA_CKEYCAPS_DESTBLTCLRSPACE \
        | VBOXVHWA_CKEYCAPS_DESTBLTCLRSPACEYUV \
        | VBOXVHWA_CKEYCAPS_DESTBLTYUV \
        | VBOXVHWA_CKEYCAPS_DESTOVERLAY \
        | VBOXVHWA_CKEYCAPS_DESTOVERLAYCLRSPACE \
        | VBOXVHWA_CKEYCAPS_DESTOVERLAYCLRSPACEYUV \
        | VBOXVHWA_CKEYCAPS_DESTOVERLAYONEACTIVE \
        | VBOXVHWA_CKEYCAPS_DESTOVERLAYYUV \
        | VBOXVHWA_CKEYCAPS_SRCBLT \
        | VBOXVHWA_CKEYCAPS_SRCBLTCLRSPACE \
        | VBOXVHWA_CKEYCAPS_SRCBLTCLRSPACEYUV \
        | VBOXVHWA_CKEYCAPS_SRCBLTYUV \
        | VBOXVHWA_CKEYCAPS_SRCOVERLAY \
        | VBOXVHWA_CKEYCAPS_SRCOVERLAYCLRSPACE \
        | VBOXVHWA_CKEYCAPS_SRCOVERLAYCLRSPACEYUV \
        | VBOXVHWA_CKEYCAPS_SRCOVERLAYONEACTIVE \
        | VBOXVHWA_CKEYCAPS_SRCOVERLAYYUV \
        | VBOXVHWA_CKEYCAPS_NOCOSTOVERLAY \
        )

#define VBOXVHWA_SUPPORTED_CKEY ( \
        VBOXVHWA_CKEY_COLORSPACE \
        | VBOXVHWA_CKEY_DESTBLT \
        | VBOXVHWA_CKEY_DESTOVERLAY \
        | VBOXVHWA_CKEY_SRCBLT \
        | VBOXVHWA_CKEY_SRCOVERLAY \
        )

#define VBOXVHWA_SUPPORTED_OVER ( \
        VBOXVHWA_OVER_DDFX \
        | VBOXVHWA_OVER_HIDE \
        | VBOXVHWA_OVER_KEYDEST \
        | VBOXVHWA_OVER_KEYDESTOVERRIDE \
        | VBOXVHWA_OVER_KEYSRC \
        | VBOXVHWA_OVER_KEYSRCOVERRIDE \
        | VBOXVHWA_OVER_SHOW \
        )

void vboxVHWAInit()
{
}

void vboxVHWATerm()
{
}

uint32_t vboxVHWAUnsupportedDDCAPS(uint32_t caps)
{
    return caps & (~VBOXVHWA_SUPPORTED_CAPS);
}

uint32_t vboxVHWAUnsupportedDDSCAPS(uint32_t caps)
{
    return caps & (~VBOXVHWA_SUPPORTED_SCAPS);
}

uint32_t vboxVHWAUnsupportedDDPFS(uint32_t caps)
{
    return caps & (~VBOXVHWA_SUPPORTED_PF);
}

uint32_t vboxVHWAUnsupportedDSS(uint32_t caps)
{
    return caps & (~VBOXVHWA_SUPPORTED_SD);
}

uint32_t vboxVHWAUnsupportedDDCEYCAPS(uint32_t caps)
{
    return caps & (~VBOXVHWA_SUPPORTED_CKEYCAPS);
}

uint32_t vboxVHWASupportedDDCEYCAPS(uint32_t caps)
{
    return caps & (VBOXVHWA_SUPPORTED_CKEYCAPS);
}


uint32_t vboxVHWASupportedDDCAPS(uint32_t caps)
{
    return caps & (VBOXVHWA_SUPPORTED_CAPS);
}

uint32_t vboxVHWASupportedDDSCAPS(uint32_t caps)
{
    return caps & (VBOXVHWA_SUPPORTED_SCAPS);
}

uint32_t vboxVHWASupportedDDPFS(uint32_t caps)
{
    return caps & (VBOXVHWA_SUPPORTED_PF);
}

uint32_t vboxVHWASupportedDSS(uint32_t caps)
{
    return caps & (VBOXVHWA_SUPPORTED_SD);
}

uint32_t vboxVHWASupportedOVERs(uint32_t caps)
{
    return caps & (VBOXVHWA_SUPPORTED_OVER);
}

uint32_t vboxVHWAUnsupportedOVERs(uint32_t caps)
{
    return caps & (~VBOXVHWA_SUPPORTED_OVER);
}

uint32_t vboxVHWASupportedCKEYs(uint32_t caps)
{
    return caps & (VBOXVHWA_SUPPORTED_CKEY);
}

uint32_t vboxVHWAUnsupportedCKEYs(uint32_t caps)
{
    return caps & (~VBOXVHWA_SUPPORTED_CKEY);
}

uint32_t vboxVHWAFromDDOVERs(uint32_t caps) { return caps; }
uint32_t vboxVHWAToDDOVERs(uint32_t caps)   { return caps; }
uint32_t vboxVHWAFromDDCKEYs(uint32_t caps) { return caps; }
uint32_t vboxVHWAToDDCKEYs(uint32_t caps)   { return caps; }

void vboxVHWAFromDDOVERLAYFX(VBOXVHWA_OVERLAYFX *pVHWAOverlay, DDOVERLAYFX *pDdOverlay)
{
    //TODO: fxFlags
    vboxVHWAFromDDCOLORKEY(&pVHWAOverlay->DstCK, &pDdOverlay->dckDestColorkey);
    vboxVHWAFromDDCOLORKEY(&pVHWAOverlay->SrcCK, &pDdOverlay->dckSrcColorkey);
}

uint32_t vboxVHWAFromDDCAPS(uint32_t caps)
{
    return caps;
}

uint32_t vboxVHWAToDDCAPS(uint32_t caps)
{
    return caps;
}

uint32_t vboxVHWAFromDDCAPS2(uint32_t caps)
{
    return caps;
}

uint32_t vboxVHWAToDDCAPS2(uint32_t caps)
{
    return caps;
}

uint32_t vboxVHWAFromDDSCAPS(uint32_t caps)
{
    return caps;
}

uint32_t vboxVHWAToDDSCAPS(uint32_t caps)
{
    return caps;
}

uint32_t vboxVHWAFromDDPFS(uint32_t caps)
{
    return caps;
}

uint32_t vboxVHWAToDDPFS(uint32_t caps)
{
    return caps;
}

uint32_t vboxVHWAFromDDCKEYCAPS(uint32_t caps)
{
    return caps;
}

uint32_t vboxVHWAToDDCKEYCAPS(uint32_t caps)
{
    return caps;
}

uint32_t vboxVHWAToDDBLTs(uint32_t caps)
{
    return caps;
}

uint32_t vboxVHWAFromDDBLTs(uint32_t caps)
{
    return caps;
}

void vboxVHWAFromDDBLTFX(VBOXVHWA_BLTFX *pVHWABlt, DDBLTFX *pDdBlt)
{
//    pVHWABlt->flags;
//    uint32_t rop;
//    uint32_t rotationOp;
//    uint32_t rotation;

    pVHWABlt->fillColor = pDdBlt->dwFillColor;

    vboxVHWAFromDDCOLORKEY(&pVHWABlt->DstCK, &pDdBlt->ddckDestColorkey);
    vboxVHWAFromDDCOLORKEY(&pVHWABlt->SrcCK, &pDdBlt->ddckSrcColorkey);
}

int vboxVHWAFromDDPIXELFORMAT(VBOXVHWA_PIXELFORMAT *pVHWAFormat, DDPIXELFORMAT *pDdFormat)
{
    uint32_t unsup = vboxVHWAUnsupportedDDPFS(pDdFormat->dwFlags);
    Assert(!unsup);
    if(unsup)
        return VERR_GENERAL_FAILURE;

    pVHWAFormat->flags = vboxVHWAFromDDPFS(pDdFormat->dwFlags);
    pVHWAFormat->fourCC = pDdFormat->dwFourCC;
    pVHWAFormat->c.rgbBitCount = pDdFormat->dwRGBBitCount;
    pVHWAFormat->m1.rgbRBitMask = pDdFormat->dwRBitMask;
    pVHWAFormat->m2.rgbGBitMask = pDdFormat->dwGBitMask;
    pVHWAFormat->m3.rgbBBitMask = pDdFormat->dwBBitMask;
    return VINF_SUCCESS;
}

void vboxVHWAFromDDCOLORKEY(VBOXVHWA_COLORKEY *pVHWACKey, DDCOLORKEY  *pDdCKey)
{
    pVHWACKey->low = pDdCKey->dwColorSpaceLowValue;
    pVHWACKey->high = pDdCKey->dwColorSpaceHighValue;
}

int vboxVHWAFromDDSURFACEDESC(VBOXVHWA_SURFACEDESC *pVHWADesc, DDSURFACEDESC *pDdDesc)
{
    uint32_t unsupds = vboxVHWAUnsupportedDSS(pDdDesc->dwFlags);
    Assert(!unsupds);
    if(unsupds)
        return VERR_GENERAL_FAILURE;

    pVHWADesc->flags = 0;

    if(pDdDesc->dwFlags & DDSD_BACKBUFFERCOUNT)
    {
        pVHWADesc->flags |= VBOXVHWA_SD_BACKBUFFERCOUNT;
        pVHWADesc->cBackBuffers = pDdDesc->dwBackBufferCount;
    }
    if(pDdDesc->dwFlags & DDSD_CAPS)
    {
        uint32_t unsup = vboxVHWAUnsupportedDDSCAPS(pDdDesc->ddsCaps.dwCaps);
        Assert(!unsup);
        if(unsup)
            return VERR_GENERAL_FAILURE;
        pVHWADesc->flags |= VBOXVHWA_SD_CAPS;
        pVHWADesc->surfCaps = vboxVHWAFromDDSCAPS(pDdDesc->ddsCaps.dwCaps);
    }
    if(pDdDesc->dwFlags & DDSD_CKDESTBLT)
    {
        pVHWADesc->flags |= VBOXVHWA_SD_CKDESTBLT;
        vboxVHWAFromDDCOLORKEY(&pVHWADesc->DstBltCK, &pDdDesc->ddckCKDestBlt);
    }
    if(pDdDesc->dwFlags & DDSD_CKDESTOVERLAY)
    {
        pVHWADesc->flags |= VBOXVHWA_SD_CKDESTOVERLAY;
        vboxVHWAFromDDCOLORKEY(&pVHWADesc->DstOverlayCK, &pDdDesc->ddckCKDestOverlay);
    }
    if(pDdDesc->dwFlags & DDSD_CKSRCBLT)
    {
        pVHWADesc->flags |= VBOXVHWA_SD_CKSRCBLT;
        vboxVHWAFromDDCOLORKEY(&pVHWADesc->SrcBltCK, &pDdDesc->ddckCKSrcBlt);
    }
    if(pDdDesc->dwFlags & DDSD_CKSRCOVERLAY)
    {
        pVHWADesc->flags |= VBOXVHWA_SD_CKSRCOVERLAY;
        vboxVHWAFromDDCOLORKEY(&pVHWADesc->SrcOverlayCK, &pDdDesc->ddckCKSrcOverlay);
    }
    if(pDdDesc->dwFlags & DDSD_HEIGHT)
    {
        pVHWADesc->flags |= VBOXVHWA_SD_HEIGHT;
        pVHWADesc->height = pDdDesc->dwHeight;
    }
    if(pDdDesc->dwFlags & DDSD_WIDTH)
    {
        pVHWADesc->flags |= VBOXVHWA_SD_WIDTH;
        pVHWADesc->width = pDdDesc->dwWidth;
    }
    if(pDdDesc->dwFlags & DDSD_PITCH)
    {
        pVHWADesc->flags |= VBOXVHWA_SD_PITCH;
        pVHWADesc->pitch = pDdDesc->lPitch;
    }
    if(pDdDesc->dwFlags & DDSD_PIXELFORMAT)
    {
        int rc = vboxVHWAFromDDPIXELFORMAT(&pVHWADesc->PixelFormat, &pDdDesc->ddpfPixelFormat);
        if(RT_FAILURE(rc))
            return rc;
        pVHWADesc->flags |= VBOXVHWA_SD_PIXELFORMAT;
    }
    return VINF_SUCCESS;
}

void vboxVHWAFromRECTL(VBOXVHWA_RECTL *pDst, RECTL *pSrc)
{
//    Assert(pSrc->left <= pSrc->right);
//    Assert(pSrc->top <= pSrc->bottom);
    pDst->left = pSrc->left;
    pDst->top = pSrc->top;
    pDst->right = pSrc->right;
    pDst->bottom = pSrc->bottom;
}

#define MIN(_a, _b) (_a) < (_b) ? (_a) : (_b)
#define MAX(_a, _b) (_a) > (_b) ? (_a) : (_b)

void vboxVHWARectUnited(RECTL * pDst, RECTL * pRect1, RECTL * pRect2)
{
    pDst->left = MIN(pRect1->left, pRect2->left);
    pDst->top = MIN(pRect1->top, pRect2->top);
    pDst->right = MAX(pRect1->right, pRect2->right);
    pDst->bottom = MAX(pRect1->bottom, pRect2->bottom);
}

bool vboxVHWARectIsEmpty(RECTL * pRect)
{
    return pRect->left == pRect->right-1 && pRect->top == pRect->bottom-1;
}

bool vboxVHWARectIntersect(RECTL * pRect1, RECTL * pRect2)
{
    return !((pRect1->left < pRect2->left && pRect1->right < pRect2->left)
            || (pRect2->left < pRect1->left && pRect2->right < pRect1->left)
            || (pRect1->top < pRect2->top && pRect1->bottom < pRect2->top)
            || (pRect2->top < pRect1->top && pRect2->bottom < pRect1->top));
}

bool vboxVHWARectInclude(RECTL * pRect1, RECTL * pRect2)
{
    return ((pRect1->left <= pRect2->left && pRect1->right >= pRect2->right)
            && (pRect1->top <= pRect2->top && pRect1->bottom >= pRect2->bottom));
}


bool vboxVHWARegionIntersects(PVBOXVHWAREGION pReg, RECTL * pRect)
{
    if(!pReg->bValid)
        return false;
    return vboxVHWARectIntersect(&pReg->Rect, pRect);
}

bool vboxVHWARegionIncludes(PVBOXVHWAREGION pReg, RECTL * pRect)
{
    if(!pReg->bValid)
        return false;
    return vboxVHWARectInclude(&pReg->Rect, pRect);
}

bool vboxVHWARegionIncluded(PVBOXVHWAREGION pReg, RECTL * pRect)
{
    if(!pReg->bValid)
        return true;
    return vboxVHWARectInclude(pRect, &pReg->Rect);
}

void vboxVHWARegionSet(PVBOXVHWAREGION pReg, RECTL * pRect)
{
    if(vboxVHWARectIsEmpty(pRect))
    {
        pReg->bValid = false;
    }
    else
    {
        pReg->Rect = *pRect;
        pReg->bValid = true;
    }
}

void vboxVHWARegionAdd(PVBOXVHWAREGION pReg, RECTL * pRect)
{
    if(vboxVHWARectIsEmpty(pRect))
    {
        return;
    }
    else if(!pReg->bValid)
    {
        vboxVHWARegionSet(pReg, pRect);
    }
    else
    {
        vboxVHWARectUnited(&pReg->Rect, &pReg->Rect, pRect);
    }
}

void vboxVHWARegionInit(PVBOXVHWAREGION pReg)
{
    pReg->bValid = false;
}

void vboxVHWARegionClear(PVBOXVHWAREGION pReg)
{
    pReg->bValid = false;
}

bool vboxVHWARegionValid(PVBOXVHWAREGION pReg)
{
    return pReg->bValid;
}

void vboxVHWARegionTrySubstitute(PVBOXVHWAREGION pReg, const RECTL *pRect)
{
    if(!pReg->bValid)
        return;

    if(pReg->Rect.left >= pRect->left && pReg->Rect.right <= pRect->right)
    {
        LONG t = MAX(pReg->Rect.top, pRect->top);
        LONG b = MIN(pReg->Rect.bottom, pRect->bottom);
        if(t < b)
        {
            pReg->Rect.top = t;
            pReg->Rect.bottom = b;
        }
        else
        {
            pReg->bValid = false;
        }
    }
    else if(pReg->Rect.top >= pRect->top && pReg->Rect.bottom <= pRect->bottom)
    {
        LONG l = MAX(pReg->Rect.left, pRect->left);
        LONG r = MIN(pReg->Rect.right, pRect->right);
        if(l < r)
        {
            pReg->Rect.left = l;
            pReg->Rect.right = r;
        }
        else
        {
            pReg->bValid = false;
        }
    }
}
