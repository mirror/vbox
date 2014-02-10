/* $Id$ */

/** @file
 * Presenter API
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "cr_spu.h"
#include "chromium.h"
#include "cr_error.h"
#include "cr_net.h"
#include "cr_rand.h"
#include "server_dispatch.h"
#include "server.h"
#include "cr_mem.h"
#include "cr_string.h"
#include <cr_vreg.h>
#include <cr_htable.h>
#include <cr_bmpscale.h>

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/list.h>


#ifdef DEBUG_misha
# define VBOXVDBG_MEMCACHE_DISABLE
#endif

#ifndef VBOXVDBG_MEMCACHE_DISABLE
# include <iprt/memcache.h>
#endif

#include "render/renderspu.h"

class ICrFbDisplay
{
public:
    virtual int UpdateBegin(struct CR_FRAMEBUFFER *pFb) = 0;
    virtual void UpdateEnd(struct CR_FRAMEBUFFER *pFb) = 0;

    virtual int EntryCreated(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry) = 0;
    virtual int EntryAdded(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry) = 0;
    virtual int EntryReplaced(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hNewEntry, HCR_FRAMEBUFFER_ENTRY hReplacedEntry) = 0;
    virtual int EntryTexChanged(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry) = 0;
    virtual int EntryRemoved(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry) = 0;
    virtual int EntryDestroyed(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry) = 0;
    virtual int EntryPosChanged(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry) = 0;

    virtual int RegionsChanged(struct CR_FRAMEBUFFER *pFb) = 0;

    virtual int FramebufferChanged(struct CR_FRAMEBUFFER *pFb) = 0;

    virtual ~ICrFbDisplay() {}
};

class CrFbDisplayComposite;
class CrFbDisplayBase;
class CrFbDisplayWindow;
class CrFbDisplayWindowRootVr;
class CrFbDisplayVrdp;

typedef struct CR_FRAMEBUFFER
{
    VBOXVR_SCR_COMPOSITOR Compositor;
    struct VBVAINFOSCREEN ScreenInfo;
    void *pvVram;
    ICrFbDisplay *pDisplay;
    RTLISTNODE EntriesList;
    uint32_t cEntries; /* <- just for debugging */
    uint32_t cUpdating;
    CRHTABLE SlotTable;
} CR_FRAMEBUFFER;

typedef union CR_FBENTRY_FLAGS
{
    struct {
        uint32_t fCreateNotified : 1;
        uint32_t fInList         : 1;
        uint32_t Reserved        : 30;
    };
    uint32_t Value;
} CR_FBENTRY_FLAGS;

typedef struct CR_FRAMEBUFFER_ENTRY
{
    VBOXVR_SCR_COMPOSITOR_ENTRY Entry;
    RTLISTNODE Node;
    uint32_t cRefs;
    CR_FBENTRY_FLAGS Flags;
    CRHTABLE HTable;
} CR_FRAMEBUFFER_ENTRY;

typedef struct CR_FBTEX
{
    CR_TEXDATA Tex;
    CRTextureObj *pTobj;
} CR_FBTEX;

#define PCR_FBTEX_FROM_TEX(_pTex) ((CR_FBTEX*)((uint8_t*)(_pTex) - RT_OFFSETOF(CR_FBTEX, Tex)))
#define PCR_FRAMEBUFFER_FROM_COMPOSITOR(_pCompositor) ((CR_FRAMEBUFFER*)((uint8_t*)(_pCompositor) - RT_OFFSETOF(CR_FRAMEBUFFER, Compositor)))
#define PCR_FBENTRY_FROM_ENTRY(_pEntry) ((CR_FRAMEBUFFER_ENTRY*)((uint8_t*)(_pEntry) - RT_OFFSETOF(CR_FRAMEBUFFER_ENTRY, Entry)))


typedef struct CR_FBDISPLAY_INFO
{
    uint32_t u32Mode;
    CrFbDisplayWindow *pDpWin;
    CrFbDisplayWindowRootVr *pDpWinRootVr;
    CrFbDisplayVrdp *pDpVrdp;
    CrFbDisplayComposite *pDpComposite;
} CR_FBDISPLAY_INFO;

typedef struct CR_PRESENTER_GLOBALS
{
#ifndef VBOXVDBG_MEMCACHE_DISABLE
    RTMEMCACHE FbEntryLookasideList;
    RTMEMCACHE FbTexLookasideList;
    RTMEMCACHE CEntryLookasideList;
#endif
    uint32_t u32DisplayMode;
    CRHashTable *pFbTexMap;
    CR_FBDISPLAY_INFO aDisplayInfos[CR_MAX_GUEST_MONITORS];
    CR_FBMAP FramebufferInitMap;
    CR_FRAMEBUFFER aFramebuffers[CR_MAX_GUEST_MONITORS];
    bool fWindowsForceHidden;
    uint32_t cbTmpBuf;
    void *pvTmpBuf;
    uint32_t cbTmpBuf2;
    void *pvTmpBuf2;
} CR_PRESENTER_GLOBALS;

static CR_PRESENTER_GLOBALS g_CrPresenter;

/* FRAMEBUFFER */

void CrFbInit(CR_FRAMEBUFFER *pFb, uint32_t idScreen)
{
    RTRECT Rect;
    Rect.xLeft = 0;
    Rect.yTop = 0;
    Rect.xRight = 1;
    Rect.yBottom = 1;
    memset(pFb, 0, sizeof (*pFb));
    pFb->ScreenInfo.u16Flags = VBVA_SCREEN_F_DISABLED;
    pFb->ScreenInfo.u32ViewIndex = idScreen;
    CrVrScrCompositorInit(&pFb->Compositor, &Rect);
    RTListInit(&pFb->EntriesList);
    CrHTableCreate(&pFb->SlotTable, 0);
}

bool CrFbIsEnabled(CR_FRAMEBUFFER *pFb)
{
    return !(pFb->ScreenInfo.u16Flags & VBVA_SCREEN_F_DISABLED);
}

HCR_FRAMEBUFFER_ENTRY CrFbEntryFromCompositorEntry(const struct VBOXVR_SCR_COMPOSITOR_ENTRY* pCEntry);

const struct VBOXVR_SCR_COMPOSITOR* CrFbGetCompositor(CR_FRAMEBUFFER *pFb)
{
    return &pFb->Compositor;
}

DECLINLINE(CR_FRAMEBUFFER*) CrFbFromCompositor(const struct VBOXVR_SCR_COMPOSITOR* pCompositor)
{
    return RT_FROM_MEMBER(pCompositor, CR_FRAMEBUFFER, Compositor);
}

const struct VBVAINFOSCREEN* CrFbGetScreenInfo(HCR_FRAMEBUFFER hFb)
{
    return &hFb->ScreenInfo;
}

void* CrFbGetVRAM(HCR_FRAMEBUFFER hFb)
{
    return hFb->pvVram;
}

int CrFbUpdateBegin(CR_FRAMEBUFFER *pFb)
{
    ++pFb->cUpdating;

    if (pFb->cUpdating == 1)
    {
        if (pFb->pDisplay)
            pFb->pDisplay->UpdateBegin(pFb);
    }

    return VINF_SUCCESS;
}

void CrFbUpdateEnd(CR_FRAMEBUFFER *pFb)
{
    if (!pFb->cUpdating)
    {
        WARN(("invalid UpdateEnd call!"));
        return;
    }

    --pFb->cUpdating;

    if (!pFb->cUpdating)
    {
        if (pFb->pDisplay)
            pFb->pDisplay->UpdateEnd(pFb);
    }
}

bool CrFbIsUpdating(const CR_FRAMEBUFFER *pFb)
{
    return !!pFb->cUpdating;
}

bool CrFbHas3DData(HCR_FRAMEBUFFER hFb)
{
    return !CrVrScrCompositorIsEmpty(&hFb->Compositor);
}

static void crFbBltMem(uint8_t *pu8Src, int32_t cbSrcPitch, uint8_t *pu8Dst, int32_t cbDstPitch, uint32_t width, uint32_t height)
{
    uint32_t cbCopyRow = width * 4;

    for (uint32_t i = 0; i < height; ++i)
    {
        memcpy(pu8Dst, pu8Src, cbCopyRow);

        pu8Src += cbSrcPitch;
        pu8Dst += cbDstPitch;
    }
}

static void crFbBltImg(const CR_BLITTER_IMG *pSrc, const RTPOINT *pSrcDataPoint, bool fSrcInvert, const RTRECT *pCopyRect, const RTPOINT *pDstDataPoint, CR_BLITTER_IMG *pDst)
{
    int32_t srcX = pCopyRect->xLeft - pSrcDataPoint->x;
    int32_t srcY = pCopyRect->yTop - pSrcDataPoint->y;
    Assert(srcX >= 0);
    Assert(srcY >= 0);
    Assert(srcX < pSrc->width);
    Assert(srcY < pSrc->height);

    int32_t dstX = pCopyRect->xLeft - pDstDataPoint->x;
    int32_t dstY = pCopyRect->yTop - pDstDataPoint->y;
    Assert(dstX >= 0);
    Assert(dstY >= 0);

    uint8_t *pu8Src = ((uint8_t*)pSrc->pvData) + pSrc->pitch * (!fSrcInvert ? srcY : pSrc->height - srcY - 1) + srcX * 4;
    uint8_t *pu8Dst = ((uint8_t*)pDst->pvData) + pDst->pitch * dstY + dstX * 4;

    crFbBltMem(pu8Src, fSrcInvert ? -pSrc->pitch : pSrc->pitch, pu8Dst, pDst->pitch, pCopyRect->xRight - pCopyRect->xLeft, pCopyRect->yBottom - pCopyRect->yTop);
}

static void crFbBltImgStretched(const CR_BLITTER_IMG *pSrc, const RTPOINT *pSrcDataPoint, bool fSrcInvert, const RTRECT *pCopyRect, const RTPOINT *pDstDataPoint, float strX, float strY, CR_BLITTER_IMG *pDst)
{
    int32_t srcX = pCopyRect->xLeft - pSrcDataPoint->x;
    int32_t srcY = pCopyRect->yTop - pSrcDataPoint->y;
    Assert(srcX >= 0);
    Assert(srcY >= 0);
    Assert(srcX < pSrc->width);
    Assert(srcY < pSrc->height);

    int32_t dstX = CR_FLOAT_RCAST(int32_t, strX * (pCopyRect->xLeft - pDstDataPoint->x));
    int32_t dstY = CR_FLOAT_RCAST(int32_t, strY * (pCopyRect->yTop - pDstDataPoint->y));
    Assert(dstX >= 0);
    Assert(dstY >= 0);

    uint8_t *pu8Src = ((uint8_t*)pSrc->pvData) + pSrc->pitch * (!fSrcInvert ? srcY : pSrc->height - srcY - 1) + srcX * 4;
    uint8_t *pu8Dst = ((uint8_t*)pDst->pvData) + pDst->pitch * dstY + dstX * 4;

    CrBmpScale32(pu8Dst, pDst->pitch,
                        CR_FLOAT_RCAST(int32_t, strX * (pCopyRect->xRight - pCopyRect->xLeft)),
                        CR_FLOAT_RCAST(int32_t, strY * (pCopyRect->yBottom - pCopyRect->yTop)),
                        pu8Src,
                        fSrcInvert ? -pSrc->pitch : pSrc->pitch,
                        pCopyRect->xRight - pCopyRect->xLeft, pCopyRect->yBottom - pCopyRect->yTop);
}

static void crFbImgFromScreenVram(const VBVAINFOSCREEN *pScreen, void *pvVram, CR_BLITTER_IMG *pImg)
{
    pImg->pvData = pvVram;
    pImg->cbData = pScreen->u32LineSize * pScreen->u32Height;
    pImg->enmFormat = GL_BGRA;
    pImg->width = pScreen->u32Width;
    pImg->height = pScreen->u32Height;
    pImg->bpp = pScreen->u16BitsPerPixel;
    pImg->pitch = pScreen->u32LineSize;
}

static void crFbImgFromFb(HCR_FRAMEBUFFER hFb, CR_BLITTER_IMG *pImg)
{
    const VBVAINFOSCREEN *pScreen = CrFbGetScreenInfo(hFb);
    void *pvVram = CrFbGetVRAM(hFb);
    crFbImgFromScreenVram(pScreen, pvVram, pImg);
}

static int crFbBltGetContentsDirect(HCR_FRAMEBUFFER hFb, const RTRECT *pSrcRect, uint32_t cRects, const RTRECT *pRects, CR_BLITTER_IMG *pImg)
{
    VBOXVR_LIST List;
    uint32_t c2DRects = 0;
    CR_TEXDATA *pEnteredTex = NULL;
    PCR_BLITTER pEnteredBlitter = NULL;
    uint32_t width = 0, height = 0;
    RTPOINT StretchedEntryPoint = {0};

    VBOXVR_SCR_COMPOSITOR_CONST_ITERATOR Iter;
    RTPOINT SrcPoint = {pSrcRect->xLeft, pSrcRect->yTop};
    float strX = ((float)pImg->width) / (pSrcRect->xRight - pSrcRect->xLeft);
    float strY = ((float)pImg->height) / (pSrcRect->yBottom - pSrcRect->yTop);

    RTPOINT StretchedSrcPoint;
    StretchedSrcPoint.x = CR_FLOAT_RCAST(int32_t, strX * SrcPoint.x);
    StretchedSrcPoint.y = CR_FLOAT_RCAST(int32_t, strY * SrcPoint.y);

    RTPOINT ZeroPoint = {0, 0};

    VBoxVrListInit(&List);
    int rc = VBoxVrListRectsAdd(&List, 1, CrVrScrCompositorRectGet(&hFb->Compositor), NULL);
    if (!RT_SUCCESS(rc))
    {
        WARN(("VBoxVrListRectsAdd failed rc %d", rc));
        goto end;
    }

    CrVrScrCompositorConstIterInit(&hFb->Compositor, &Iter);

    for(const VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry = CrVrScrCompositorConstIterNext(&Iter);
            pEntry;
            pEntry = CrVrScrCompositorConstIterNext(&Iter))
    {
        uint32_t cRegions;
        const RTRECT *pRegions;
        rc = CrVrScrCompositorEntryRegionsGet(&hFb->Compositor, pEntry, &cRegions, NULL, NULL, &pRegions);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrVrScrCompositorEntryRegionsGet failed rc %d", rc));
            goto end;
        }

        rc = VBoxVrListRectsSubst(&List, cRegions, pRegions, NULL);
        if (!RT_SUCCESS(rc))
        {
            WARN(("VBoxVrListRectsSubst failed rc %d", rc));
            goto end;
        }

        for (uint32_t i = 0; i < cRects; ++i)
        {
            const RTRECT * pRect = &pRects[i];
            for (uint32_t j = 0; j < cRegions; ++j)
            {
                const RTRECT * pReg = &pRegions[j];
                RTRECT Intersection;
                VBoxRectIntersected(pRect, pReg, &Intersection);
                if (VBoxRectIsZero(&Intersection))
                    continue;

                VBoxRectStretch(&Intersection, strX, strY);
                if (VBoxRectIsZero(&Intersection))
                    continue;

                CR_TEXDATA *pTex = CrVrScrCompositorEntryTexGet(pEntry);
                const CR_BLITTER_IMG *pSrcImg;

                if (pEnteredTex != pTex)
                {
                    if (!pEnteredBlitter)
                    {
                        pEnteredBlitter = CrTdBlitterGet(pTex);
                        rc = CrBltEnter(pEnteredBlitter);
                        if (!RT_SUCCESS(rc))
                        {
                            WARN(("CrBltEnter failed %d", rc));
                            pEnteredBlitter = NULL;
                            goto end;
                        }
                    }

                    if (pEnteredTex)
                    {
                        CrTdBltLeave(pEnteredTex);

                        pEnteredTex = NULL;

                        if (pEnteredBlitter != CrTdBlitterGet(pTex))
                        {
                            WARN(("blitters not equal!"));
                            CrBltLeave(pEnteredBlitter);

                            pEnteredBlitter = CrTdBlitterGet(pTex);
                            rc = CrBltEnter(pEnteredBlitter);
                             if (!RT_SUCCESS(rc))
                             {
                                 WARN(("CrBltEnter failed %d", rc));
                                 pEnteredBlitter = NULL;
                                 goto end;
                             }
                        }
                    }

                    rc = CrTdBltEnter(pTex);
                    if (!RT_SUCCESS(rc))
                    {
                        WARN(("CrTdBltEnter failed %d", rc));
                        goto end;
                    }

                    pEnteredTex = pTex;

                    const VBOXVR_TEXTURE *pVrTex = CrTdTexGet(pTex);

                    width = CR_FLOAT_RCAST(uint32_t, strX * pVrTex->width);
                    height = CR_FLOAT_RCAST(uint32_t, strY * pVrTex->height);
                    StretchedEntryPoint.x = CR_FLOAT_RCAST(int32_t, strX * CrVrScrCompositorEntryRectGet(pEntry)->xLeft);
                    StretchedEntryPoint.y = CR_FLOAT_RCAST(int32_t, strY * CrVrScrCompositorEntryRectGet(pEntry)->yTop);
                }

                rc = CrTdBltDataAcquireStretched(pTex, GL_BGRA, false, width, height, &pSrcImg);
                if (!RT_SUCCESS(rc))
                {
                    WARN(("CrTdBltDataAcquire failed rc %d", rc));
                    goto end;
                }

                bool fInvert = !(CrVrScrCompositorEntryFlagsGet(pEntry) & CRBLT_F_INVERT_SRC_YCOORDS);

                crFbBltImg(pSrcImg, &StretchedEntryPoint, fInvert, &Intersection, &StretchedSrcPoint, pImg);

                CrTdBltDataReleaseStretched(pTex, pSrcImg);
            }
        }
    }

    c2DRects = VBoxVrListRectsCount(&List);
    if (c2DRects)
    {
        if (g_CrPresenter.cbTmpBuf2 < c2DRects * sizeof (RTRECT))
        {
            if (g_CrPresenter.pvTmpBuf2)
                RTMemFree(g_CrPresenter.pvTmpBuf2);

            g_CrPresenter.cbTmpBuf2 = (c2DRects + 10) * sizeof (RTRECT);
            g_CrPresenter.pvTmpBuf2 = RTMemAlloc(g_CrPresenter.cbTmpBuf2);
            if (!g_CrPresenter.pvTmpBuf2)
            {
                WARN(("RTMemAlloc failed!"));
                g_CrPresenter.cbTmpBuf2 = 0;
                rc = VERR_NO_MEMORY;
                goto end;
            }
        }

        RTRECT *p2DRects  = (RTRECT *)g_CrPresenter.pvTmpBuf2;

        rc = VBoxVrListRectsGet(&List, c2DRects, p2DRects);
        if (!RT_SUCCESS(rc))
        {
            WARN(("VBoxVrListRectsGet failed, rc %d", rc));
            goto end;
        }

        RTPOINT Pos = {0};
        const RTRECT *pCompRect = CrVrScrCompositorRectGet(&hFb->Compositor);

        uint32_t fbWidth = (pCompRect->xRight - pCompRect->xLeft);
        uint32_t fbHeight = pCompRect->yBottom - pCompRect->yTop;

        uint32_t stretchedWidth = CR_FLOAT_RCAST(uint32_t, strX * fbWidth);
        uint32_t stretchedHeight = CR_FLOAT_RCAST(uint32_t, strY * fbHeight);

        CR_BLITTER_IMG FbImg;

        bool fStretch = fbWidth != stretchedWidth || fbHeight != stretchedHeight;

        crFbImgFromFb(hFb, &FbImg);

        for (uint32_t i = 0; i < cRects; ++i)
        {
            const RTRECT * pRect = &pRects[i];
            for (uint32_t j = 0; j < c2DRects; ++j)
            {
                const RTRECT * p2DRect = &p2DRects[j];
                RTRECT Intersection;
                VBoxRectIntersected(pRect, p2DRect, &Intersection);
                if (VBoxRectIsZero(&Intersection))
                    continue;

                if (!fStretch)
                    crFbBltImg(&FbImg, &ZeroPoint, false, &Intersection, &SrcPoint, pImg);
                else
                    crFbBltImgStretched(&FbImg, &ZeroPoint, false, &Intersection, &SrcPoint, strX, strY, pImg);
            }
        }
    }

end:

    if (pEnteredTex)
        CrTdBltLeave(pEnteredTex);

    if (pEnteredBlitter)
        CrBltLeave(pEnteredBlitter);

    VBoxVrListClear(&List);

    return rc;
}

static int crFbBltGetContentsStretchCPU(HCR_FRAMEBUFFER hFb, const RTRECT *pSrcRect, uint32_t cRects, const RTRECT *pRects, CR_BLITTER_IMG *pImg)
{
    uint32_t srcWidth = pSrcRect->xRight - pSrcRect->xLeft;
    uint32_t srcHeight = pSrcRect->yBottom - pSrcRect->yTop;

    /* destination is bigger than the source, do 3D data stretching with CPU */
    CR_BLITTER_IMG Img;
    Img.cbData = srcWidth * srcHeight * 4;
    Img.pvData = RTMemAlloc(Img.cbData);
    if (!Img.pvData)
    {
        WARN(("RTMemAlloc Failed"));
        return VERR_NO_MEMORY;
    }
    Img.enmFormat = pImg->enmFormat;
    Img.width = srcWidth;
    Img.height = srcHeight;
    Img.bpp = pImg->bpp;
    Img.pitch = Img.width * 4;

    int rc = CrFbBltGetContents(hFb, pSrcRect, cRects, pRects, &Img);
    if (RT_SUCCESS(rc))
    {
        CrBmpScale32((uint8_t *)pImg->pvData,
                            pImg->pitch,
                            pImg->width, pImg->height,
                            (const uint8_t *)Img.pvData,
                            Img.pitch,
                            Img.width, Img.height);
    }
    else
        WARN(("CrFbBltGetContents failed %d", rc));

    RTMemFree(Img.pvData);

    return rc;

}

int CrFbBltGetContents(HCR_FRAMEBUFFER hFb, const RTRECT *pSrcRect, uint32_t cRects, const RTRECT *pRects, CR_BLITTER_IMG *pImg)
{
    uint32_t srcWidth = pSrcRect->xRight - pSrcRect->xLeft;
    uint32_t srcHeight = pSrcRect->yBottom - pSrcRect->yTop;
    if ((srcWidth == pImg->width
            && srcHeight == pImg->height)
            || !CrFbHas3DData(hFb)
            || (srcWidth * srcHeight > pImg->width * pImg->height))
    {
        return crFbBltGetContentsDirect(hFb, pSrcRect, cRects, pRects, pImg);
    }

    return crFbBltGetContentsStretchCPU(hFb, pSrcRect, cRects, pRects, pImg);
}


int CrFbResize(CR_FRAMEBUFFER *pFb, const struct VBVAINFOSCREEN * pScreen, void *pvVRAM)
{
    if (!pFb->cUpdating)
    {
        WARN(("no update in progress"));
        return VERR_INVALID_STATE;
    }

    if (pScreen->u16Flags & VBVA_SCREEN_F_DISABLED)
    {
        CrVrScrCompositorClear(&pFb->Compositor);
    }

    RTRECT Rect;
    Rect.xLeft = 0;
    Rect.yTop = 0;
    Rect.xRight = pScreen->u32Width;
    Rect.yBottom = pScreen->u32Height;
    int rc = CrVrScrCompositorRectSet(&pFb->Compositor, &Rect, NULL);
    if (!RT_SUCCESS(rc))
    {
        WARN(("CrVrScrCompositorRectSet failed rc %d", rc));
        return rc;
    }

    pFb->ScreenInfo = *pScreen;
    pFb->pvVram = pvVRAM;

    if (pFb->pDisplay)
        pFb->pDisplay->FramebufferChanged(pFb);

    return VINF_SUCCESS;
}

void CrFbTerm(CR_FRAMEBUFFER *pFb)
{
    if (pFb->cUpdating)
    {
        WARN(("update in progress"));
        return;
    }
    uint32_t idScreen = pFb->ScreenInfo.u32ViewIndex;

    CrVrScrCompositorClear(&pFb->Compositor);
    CrHTableDestroy(&pFb->SlotTable);

    Assert(RTListIsEmpty(&pFb->EntriesList));
    Assert(!pFb->cEntries);

    memset(pFb, 0, sizeof (*pFb));

    pFb->ScreenInfo.u16Flags = VBVA_SCREEN_F_DISABLED;
    pFb->ScreenInfo.u32ViewIndex = idScreen;
}

ICrFbDisplay* CrFbDisplayGet(CR_FRAMEBUFFER *pFb)
{
    return pFb->pDisplay;
}

int CrFbDisplaySet(CR_FRAMEBUFFER *pFb, ICrFbDisplay *pDisplay)
{
    if (pFb->cUpdating)
    {
        WARN(("update in progress"));
        return VERR_INVALID_STATE;
    }

    if (pFb->pDisplay == pDisplay)
        return VINF_SUCCESS;

    pFb->pDisplay = pDisplay;

    return VINF_SUCCESS;
}

#define CR_PMGR_MODE_WINDOW 0x1
/* mutually exclusive with CR_PMGR_MODE_WINDOW */
#define CR_PMGR_MODE_ROOTVR 0x2
#define CR_PMGR_MODE_VRDP   0x4
#define CR_PMGR_MODE_ALL    0x7

static int crPMgrModeModifyGlobal(uint32_t u32ModeAdd, uint32_t u32ModeRemove);

static CR_FBTEX* crFbTexAlloc()
{
#ifndef VBOXVDBG_MEMCACHE_DISABLE
    return (CR_FBTEX*)RTMemCacheAlloc(g_CrPresenter.FbTexLookasideList);
#else
    return (CR_FBTEX*)RTMemAlloc(sizeof (CR_FBTEX));
#endif
}

static void crFbTexFree(CR_FBTEX *pTex)
{
#ifndef VBOXVDBG_MEMCACHE_DISABLE
    RTMemCacheFree(g_CrPresenter.FbTexLookasideList, pTex);
#else
    RTMemFree(pTex);
#endif
}

static CR_FRAMEBUFFER_ENTRY* crFbEntryAlloc()
{
#ifndef VBOXVDBG_MEMCACHE_DISABLE
    return (CR_FRAMEBUFFER_ENTRY*)RTMemCacheAlloc(g_CrPresenter.FbEntryLookasideList);
#else
    return (CR_FRAMEBUFFER_ENTRY*)RTMemAlloc(sizeof (CR_FRAMEBUFFER_ENTRY));
#endif
}

static void crFbEntryFree(CR_FRAMEBUFFER_ENTRY *pEntry)
{
    Assert(!CrVrScrCompositorEntryIsUsed(&pEntry->Entry));
#ifndef VBOXVDBG_MEMCACHE_DISABLE
    RTMemCacheFree(g_CrPresenter.FbEntryLookasideList, pEntry);
#else
    RTMemFree(pEntry);
#endif
}

DECLCALLBACK(void) crFbTexRelease(CR_TEXDATA *pTex)
{
    CR_FBTEX *pFbTex = PCR_FBTEX_FROM_TEX(pTex);
    CRTextureObj *pTobj = pFbTex->pTobj;

    CrTdBltDataCleanupNe(pTex);

    if (pTobj)
    {
        CR_STATE_SHAREDOBJ_USAGE_CLEAR(pTobj, cr_server.MainContextInfo.pContext);

        crHashtableDelete(g_CrPresenter.pFbTexMap, pTobj->id, NULL);

        if (!CR_STATE_SHAREDOBJ_USAGE_IS_USED(pTobj))
        {
            CRSharedState *pShared = crStateGlobalSharedAcquire();

            CRASSERT(pShared);
            /* on the host side, we need to delete an ogl texture object here as well, which crStateDeleteTextureCallback will do
             * in addition to calling crStateDeleteTextureObject to delete a state object */
            crHashtableDelete(pShared->textureTable, pTobj->id, crStateDeleteTextureCallback);

            crStateGlobalSharedRelease();
        }

        crStateGlobalSharedRelease();
    }

    crFbTexFree(pFbTex);
}

void CrFbTexDataInit(CR_TEXDATA* pFbTex, const VBOXVR_TEXTURE *pTex, PFNCRTEXDATA_RELEASED pfnTextureReleased)
{
    PCR_BLITTER pBlitter = crServerVBoxBlitterGet();

    CrTdInit(pFbTex, pTex, pBlitter, pfnTextureReleased);
}

static CR_FBTEX* crFbTexCreate(const VBOXVR_TEXTURE *pTex)
{
    CR_FBTEX *pFbTex = crFbTexAlloc();
    if (!pFbTex)
    {
        WARN(("crFbTexAlloc failed!"));
        return NULL;
    }

    CrFbTexDataInit(&pFbTex->Tex, pTex, crFbTexRelease);
    pFbTex->pTobj = NULL;

    return pFbTex;
}


CR_TEXDATA* CrFbTexDataCreate(const VBOXVR_TEXTURE *pTex)
{
    CR_FBTEX *pFbTex = crFbTexCreate(pTex);
    if (!pFbTex)
    {
        WARN(("crFbTexCreate failed!"));
        return NULL;
    }

    return &pFbTex->Tex;
}

static CR_FBTEX* crFbTexAcquire(GLuint idTexture)
{
    CR_FBTEX *pFbTex = (CR_FBTEX *)crHashtableSearch(g_CrPresenter.pFbTexMap, idTexture);
    if (pFbTex)
    {
        CrTdAddRef(&pFbTex->Tex);
        return pFbTex;
    }

    CRSharedState *pShared = crStateGlobalSharedAcquire();
    if (!pShared)
    {
        WARN(("pShared is null!"));
        return NULL;
    }

    CRTextureObj *pTobj = (CRTextureObj*)crHashtableSearch(pShared->textureTable, idTexture);
    if (!pTobj)
    {
        LOG(("pTobj is null!"));
        crStateGlobalSharedRelease();
        return NULL;
    }

    Assert(pTobj->id == idTexture);

    GLuint hwid = crStateGetTextureObjHWID(pTobj);
    if (!hwid)
    {
        WARN(("hwId is null!"));
        crStateGlobalSharedRelease();
        return NULL;
    }

    VBOXVR_TEXTURE Tex;
    Tex.width = pTobj->level[0]->width;
    Tex.height = pTobj->level[0]->height;
    Tex.hwid = hwid;
    Tex.target = pTobj->target;

    pFbTex = crFbTexCreate(&Tex);
    if (!pFbTex)
    {
        WARN(("crFbTexCreate failed!"));
        crStateGlobalSharedRelease();
        return NULL;
    }

    CR_STATE_SHAREDOBJ_USAGE_SET(pTobj, cr_server.MainContextInfo.pContext);

    pFbTex->pTobj = pTobj;

    crHashtableAdd(g_CrPresenter.pFbTexMap, idTexture, pFbTex);

    return pFbTex;
}

static void crFbEntryMarkDestroyed(CR_FRAMEBUFFER *pFb, CR_FRAMEBUFFER_ENTRY* pEntry)
{
    if (pEntry->Flags.fCreateNotified)
    {
        pEntry->Flags.fCreateNotified = 0;
        if (pFb->pDisplay)
            pFb->pDisplay->EntryDestroyed(pFb, pEntry);

        CR_TEXDATA *pTex = CrVrScrCompositorEntryTexGet(&pEntry->Entry);
        if (pTex)
            CrTdBltDataInvalidateNe(pTex);
    }
}

static void crFbEntryDestroy(CR_FRAMEBUFFER *pFb, CR_FRAMEBUFFER_ENTRY* pEntry)
{
    crFbEntryMarkDestroyed(pFb, pEntry);
    CrVrScrCompositorEntryCleanup(&pEntry->Entry);
    CrHTableDestroy(&pEntry->HTable);
    Assert(pFb->cEntries);
    RTListNodeRemove(&pEntry->Node);
    --pFb->cEntries;
    crFbEntryFree(pEntry);
}

DECLINLINE(uint32_t) crFbEntryAddRef(CR_FRAMEBUFFER_ENTRY* pEntry)
{
    return ++pEntry->cRefs;
}

DECLINLINE(uint32_t) crFbEntryRelease(CR_FRAMEBUFFER *pFb, CR_FRAMEBUFFER_ENTRY* pEntry)
{
    uint32_t cRefs = --pEntry->cRefs;
    if (!cRefs)
        crFbEntryDestroy(pFb, pEntry);
    return cRefs;
}

static DECLCALLBACK(void) crFbEntryReleased(const struct VBOXVR_SCR_COMPOSITOR *pCompositor, struct VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry, struct VBOXVR_SCR_COMPOSITOR_ENTRY *pReplacingEntry)
{
    CR_FRAMEBUFFER *pFb = PCR_FRAMEBUFFER_FROM_COMPOSITOR(pCompositor);
    CR_FRAMEBUFFER_ENTRY *pFbEntry = PCR_FBENTRY_FROM_ENTRY(pEntry);
    CR_FRAMEBUFFER_ENTRY *pFbReplacingEntry = pReplacingEntry ? PCR_FBENTRY_FROM_ENTRY(pReplacingEntry) : NULL;
    if (pFbReplacingEntry)
    {
        /*replace operation implies the replaced entry gets auto-destroyed,
         * while all its data gets moved to the *clean* replacing entry
         * 1. ensure the replacing entry is cleaned up */
        crFbEntryMarkDestroyed(pFb, pFbReplacingEntry);

        CrHTableMoveTo(&pFbEntry->HTable, &pFbReplacingEntry->HTable);

        CR_TEXDATA *pTex = CrVrScrCompositorEntryTexGet(&pFbEntry->Entry);
        CR_TEXDATA *pReplacingTex = CrVrScrCompositorEntryTexGet(&pFbReplacingEntry->Entry);

        CrTdBltStretchCacheMoveTo(pTex, pReplacingTex);

        if (pFb->pDisplay)
            pFb->pDisplay->EntryReplaced(pFb, pFbReplacingEntry, pFbEntry);

        CrTdBltDataInvalidateNe(pTex);

        /* 2. mark the replaced entry is destroyed */
        Assert(pFbEntry->Flags.fCreateNotified);
        Assert(pFbEntry->Flags.fInList);
        pFbEntry->Flags.fCreateNotified = 0;
        pFbEntry->Flags.fInList = 0;
        pFbReplacingEntry->Flags.fCreateNotified = 1;
        pFbReplacingEntry->Flags.fInList = 1;
    }
    else
    {
        if (pFbEntry->Flags.fInList)
        {
            pFbEntry->Flags.fInList = 0;
            if (pFb->pDisplay)
                pFb->pDisplay->EntryRemoved(pFb, pFbEntry);

            CR_TEXDATA *pTex = CrVrScrCompositorEntryTexGet(&pFbEntry->Entry);
            if (pTex)
                CrTdBltDataInvalidateNe(pTex);
        }
    }

    crFbEntryRelease(pFb, pFbEntry);
}

static CR_FRAMEBUFFER_ENTRY* crFbEntryCreate(CR_FRAMEBUFFER *pFb, CR_TEXDATA* pTex, const RTRECT *pRect, uint32_t fFlags)
{
    CR_FRAMEBUFFER_ENTRY *pEntry = crFbEntryAlloc();
    if (!pEntry)
    {
        WARN(("crFbEntryAlloc failed!"));
        return NULL;
    }

    CrVrScrCompositorEntryInit(&pEntry->Entry, pRect, pTex, crFbEntryReleased);
    CrVrScrCompositorEntryFlagsSet(&pEntry->Entry, fFlags);
    pEntry->cRefs = 1;
    pEntry->Flags.Value = 0;
    CrHTableCreate(&pEntry->HTable, 0);

    RTListAppend(&pFb->EntriesList, &pEntry->Node);
    ++pFb->cEntries;

    return pEntry;
}

int CrFbEntryCreateForTexData(CR_FRAMEBUFFER *pFb, struct CR_TEXDATA *pTex, uint32_t fFlags, HCR_FRAMEBUFFER_ENTRY *phEntry)
{
    RTRECT Rect;
    Rect.xLeft = 0;
    Rect.yTop = 0;
    Rect.xRight = pTex->Tex.width;
    Rect.yBottom = pTex->Tex.height;
    CR_FRAMEBUFFER_ENTRY* pEntry = crFbEntryCreate(pFb, pTex, &Rect, fFlags);
    if (!pEntry)
    {
        WARN(("crFbEntryCreate failed"));
        return VERR_NO_MEMORY;
    }

    *phEntry = pEntry;
    return VINF_SUCCESS;
}

int CrFbEntryTexDataUpdate(CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY pEntry, struct CR_TEXDATA *pTex)
{
    if (!pFb->cUpdating)
    {
        WARN(("framebuffer not updating"));
        return VERR_INVALID_STATE;
    }

    if (pTex)
        CrVrScrCompositorEntryTexSet(&pEntry->Entry, pTex);

    if (CrVrScrCompositorEntryIsUsed(&pEntry->Entry))
    {
        if (pFb->pDisplay)
            pFb->pDisplay->EntryTexChanged(pFb, pEntry);

        CR_TEXDATA *pTex = CrVrScrCompositorEntryTexGet(&pEntry->Entry);
        if (pTex)
            CrTdBltDataInvalidateNe(pTex);
    }

    return VINF_SUCCESS;
}


int CrFbEntryCreateForTexId(CR_FRAMEBUFFER *pFb, GLuint idTexture, uint32_t fFlags, HCR_FRAMEBUFFER_ENTRY *phEntry)
{
    CR_FBTEX* pFbTex = crFbTexAcquire(idTexture);
    if (!pFbTex)
    {
        LOG(("crFbTexAcquire failed"));
        return VERR_INVALID_PARAMETER;
    }

    CR_TEXDATA* pTex = &pFbTex->Tex;
    int rc = CrFbEntryCreateForTexData(pFb, pTex, fFlags, phEntry);
    if (!RT_SUCCESS(rc))
    {
    	WARN(("CrFbEntryCreateForTexData failed rc %d", rc));
    }

    /*always release the tex, the CrFbEntryCreateForTexData will do incref as necessary */
    CrTdRelease(pTex);
    return rc;
}

void CrFbEntryAddRef(CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
{
    ++hEntry->cRefs;
}

void CrFbEntryRelease(CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
{
    crFbEntryRelease(pFb, hEntry);
}

int CrFbRegionsClear(HCR_FRAMEBUFFER hFb)
{
    if (!hFb->cUpdating)
    {
        WARN(("framebuffer not updating"));
        return VERR_INVALID_STATE;
    }

    bool fChanged = false;
    CrVrScrCompositorRegionsClear(&hFb->Compositor, &fChanged);
    if (fChanged)
    {
        if (hFb->pDisplay)
            hFb->pDisplay->RegionsChanged(hFb);
    }

    return VINF_SUCCESS;
}

int CrFbEntryRegionsAdd(CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions, bool fPosRelated)
{
    if (!pFb->cUpdating)
    {
        WARN(("framebuffer not updating"));
        return VERR_INVALID_STATE;
    }

    uint32_t fChangeFlags = 0;
    VBOXVR_SCR_COMPOSITOR_ENTRY *pReplacedScrEntry = NULL;
    VBOXVR_SCR_COMPOSITOR_ENTRY *pNewEntry;
    bool fEntryWasInList;

    if (hEntry)
    {
        crFbEntryAddRef(hEntry);
        pNewEntry = &hEntry->Entry;
        fEntryWasInList = CrVrScrCompositorEntryIsUsed(pNewEntry);

        Assert(!hEntry->Flags.fInList == !fEntryWasInList);
    }
    else
    {
        pNewEntry = NULL;
        fEntryWasInList = false;
    }

    int rc = CrVrScrCompositorEntryRegionsAdd(&pFb->Compositor, hEntry ? &hEntry->Entry : NULL, pPos, cRegions, paRegions, fPosRelated, &pReplacedScrEntry, &fChangeFlags);
    if (RT_SUCCESS(rc))
    {
        if (fChangeFlags & VBOXVR_COMPOSITOR_CF_REGIONS_CHANGED)
        {
            if (!fEntryWasInList && pNewEntry)
            {
                Assert(CrVrScrCompositorEntryIsUsed(pNewEntry));
                if (!hEntry->Flags.fCreateNotified)
                {
                    hEntry->Flags.fCreateNotified = 1;
                    if (pFb->pDisplay)
                        pFb->pDisplay->EntryCreated(pFb, hEntry);
                }

#ifdef DEBUG_misha
                /* in theory hEntry->Flags.fInList can be set if entry is replaced,
                 * but then modified to fit the compositor rects,
                 * and so we get the regions changed notification as a result
                 * this should not generally happen though, so put an assertion to debug that situation */
                Assert(!hEntry->Flags.fInList);
#endif
                if (!hEntry->Flags.fInList)
                {
                    hEntry->Flags.fInList = 1;

                    if (pFb->pDisplay)
                        pFb->pDisplay->EntryAdded(pFb, hEntry);
                }
            }
            if (pFb->pDisplay)
                pFb->pDisplay->RegionsChanged(pFb);

            Assert(!pReplacedScrEntry);
        }
        else if (fChangeFlags & VBOXVR_COMPOSITOR_CF_ENTRY_REPLACED)
        {
            Assert(pReplacedScrEntry);
            /* we have already processed that in a "release" callback */
            Assert(hEntry);
        }
        else
        {
            Assert(!fChangeFlags);
            Assert(!pReplacedScrEntry);
        }

        if (hEntry)
        {
            if (CrVrScrCompositorEntryIsUsed(&hEntry->Entry))
            {
                if (pFb->pDisplay)
                    pFb->pDisplay->EntryTexChanged(pFb, hEntry);

                CR_TEXDATA *pTex = CrVrScrCompositorEntryTexGet(&hEntry->Entry);
                if (pTex)
                    CrTdBltDataInvalidateNe(pTex);
            }
        }
    }
    else
        WARN(("CrVrScrCompositorEntryRegionsAdd failed, rc %d", rc));

    return rc;
}

int CrFbEntryRegionsSet(CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions, bool fPosRelated)
{
    if (!pFb->cUpdating)
    {
        WARN(("framebuffer not updating"));
        return VERR_INVALID_STATE;
    }

    bool fChanged = 0;
    VBOXVR_SCR_COMPOSITOR_ENTRY *pReplacedScrEntry = NULL;
    VBOXVR_SCR_COMPOSITOR_ENTRY *pNewEntry;
    bool fEntryWasInList;

    if (hEntry)
    {
        crFbEntryAddRef(hEntry);
        pNewEntry = &hEntry->Entry;
        fEntryWasInList = CrVrScrCompositorEntryIsUsed(pNewEntry);
        Assert(!hEntry->Flags.fInList == !fEntryWasInList);
    }
    else
    {
        pNewEntry = NULL;
        fEntryWasInList = false;
    }

    int rc = CrVrScrCompositorEntryRegionsSet(&pFb->Compositor, pNewEntry, pPos, cRegions, paRegions, fPosRelated, &fChanged);
    if (RT_SUCCESS(rc))
    {
        if (fChanged)
        {
            if (!fEntryWasInList && pNewEntry)
            {
                if (CrVrScrCompositorEntryIsUsed(pNewEntry))
                {
                    if (!hEntry->Flags.fCreateNotified)
                    {
                        hEntry->Flags.fCreateNotified = 1;

                        if (pFb->pDisplay)
                            pFb->pDisplay->EntryCreated(pFb, hEntry);
                    }

                    Assert(!hEntry->Flags.fInList);
                    hEntry->Flags.fInList = 1;

                    if (pFb->pDisplay)
                        pFb->pDisplay->EntryAdded(pFb, hEntry);
                }
            }

            if (pFb->pDisplay)
                pFb->pDisplay->RegionsChanged(pFb);
        }

        if (hEntry)
        {
            if (CrVrScrCompositorEntryIsUsed(&hEntry->Entry))
            {
                if (pFb->pDisplay)
                    pFb->pDisplay->EntryTexChanged(pFb, hEntry);

                CR_TEXDATA *pTex = CrVrScrCompositorEntryTexGet(&hEntry->Entry);
                if (pTex)
                    CrTdBltDataInvalidateNe(pTex);
            }
        }
    }
    else
        WARN(("CrVrScrCompositorEntryRegionsSet failed, rc %d", rc));

    return rc;
}

const struct VBOXVR_SCR_COMPOSITOR_ENTRY* CrFbEntryGetCompositorEntry(HCR_FRAMEBUFFER_ENTRY hEntry)
{
    return &hEntry->Entry;
}

HCR_FRAMEBUFFER_ENTRY CrFbEntryFromCompositorEntry(const struct VBOXVR_SCR_COMPOSITOR_ENTRY* pCEntry)
{
    return RT_FROM_MEMBER(pCEntry, CR_FRAMEBUFFER_ENTRY, Entry);
}

void CrFbVisitCreatedEntries(HCR_FRAMEBUFFER hFb, PFNCR_FRAMEBUFFER_ENTRIES_VISITOR_CB pfnVisitorCb, void *pvContext)
{
    HCR_FRAMEBUFFER_ENTRY hEntry, hNext;
    RTListForEachSafe(&hFb->EntriesList, hEntry, hNext, CR_FRAMEBUFFER_ENTRY, Node)
    {
        if (hEntry->Flags.fCreateNotified)
        {
            if (!pfnVisitorCb(hFb, hEntry, pvContext))
                return;
        }
    }
}


CRHTABLE_HANDLE CrFbDDataAllocSlot(CR_FRAMEBUFFER *pFb)
{
    return CrHTablePut(&pFb->SlotTable, (void*)1);
}

void CrFbDDataReleaseSlot(CR_FRAMEBUFFER *pFb, CRHTABLE_HANDLE hSlot, PFNCR_FRAMEBUFFER_SLOT_RELEASE_CB pfnReleaseCb, void *pvContext)
{
    HCR_FRAMEBUFFER_ENTRY hEntry, hNext;
    RTListForEachSafe(&pFb->EntriesList, hEntry, hNext, CR_FRAMEBUFFER_ENTRY, Node)
    {
        if (CrFbDDataEntryGet(hEntry, hSlot))
        {
            if (pfnReleaseCb)
                pfnReleaseCb(pFb, hEntry, pvContext);

            CrFbDDataEntryClear(hEntry, hSlot);
        }
    }

    CrHTableRemove(&pFb->SlotTable, hSlot);
}

int CrFbDDataEntryPut(HCR_FRAMEBUFFER_ENTRY hEntry, CRHTABLE_HANDLE hSlot, void *pvData)
{
    return CrHTablePutToSlot(&hEntry->HTable, hSlot, pvData);
}

void* CrFbDDataEntryClear(HCR_FRAMEBUFFER_ENTRY hEntry, CRHTABLE_HANDLE hSlot)
{
    return CrHTableRemove(&hEntry->HTable, hSlot);
}

void* CrFbDDataEntryGet(HCR_FRAMEBUFFER_ENTRY hEntry, CRHTABLE_HANDLE hSlot)
{
    return CrHTableGet(&hEntry->HTable, hSlot);
}

typedef union CR_FBDISPBASE_FLAGS
{
    struct {
        uint32_t fRegionsShanged : 1;
        uint32_t Reserved        : 31;
    };
    uint32_t u32Value;
} CR_FBDISPBASE_FLAGS;

class CrFbDisplayBase : public ICrFbDisplay
{
public:
    CrFbDisplayBase() :
        mpContainer(NULL),
        mpFb(NULL),
        mcUpdates(0),
        mhSlot(CRHTABLE_HANDLE_INVALID)
    {
        mFlags.u32Value = 0;
    }

    virtual bool isComposite()
    {
        return false;
    }

    class CrFbDisplayComposite* getContainer()
    {
        return mpContainer;
    }

    bool isInList()
    {
        return !!mpContainer;
    }

    bool isUpdating()
    {
        return !!mcUpdates;
    }

    int setRegionsChanged()
    {
        if (!mcUpdates)
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }

        mFlags.fRegionsShanged = 1;
        return VINF_SUCCESS;
    }

    int setFramebuffer(struct CR_FRAMEBUFFER *pFb)
    {
        if (mcUpdates)
        {
            WARN(("trying to set framebuffer while update is in progress"));
            return VERR_INVALID_STATE;
        }

        if (mpFb == pFb)
            return VINF_SUCCESS;

        int rc = setFramebufferBegin(pFb);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        if (mpFb)
        {
            rc = fbCleanup();
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                setFramebufferEnd(pFb);
                return rc;
            }
        }

        mpFb = pFb;

        if (mpFb)
        {
            rc = fbSync();
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                setFramebufferEnd(pFb);
                return rc;
            }
        }

        setFramebufferEnd(pFb);
        return VINF_SUCCESS;
    }

    struct CR_FRAMEBUFFER* getFramebuffer()
    {
        return mpFb;
    }

    virtual int UpdateBegin(struct CR_FRAMEBUFFER *pFb)
    {
        ++mcUpdates;
        Assert(!mFlags.fRegionsShanged || mcUpdates > 1);
        return VINF_SUCCESS;
    }

    virtual void UpdateEnd(struct CR_FRAMEBUFFER *pFb)
    {
        --mcUpdates;
        Assert(mcUpdates < UINT32_MAX/2);
        if (!mcUpdates)
            onUpdateEnd();
    }

    virtual int EntryCreated(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        if (!mcUpdates)
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }
        return VINF_SUCCESS;
    }

    virtual int EntryAdded(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        if (!mcUpdates)
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }
        mFlags.fRegionsShanged = 1;
        return VINF_SUCCESS;
    }

    virtual int EntryReplaced(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hNewEntry, HCR_FRAMEBUFFER_ENTRY hReplacedEntry)
    {
        if (!mcUpdates)
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }
        return VINF_SUCCESS;
    }

    virtual int EntryTexChanged(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        if (!mcUpdates)
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }
        return VINF_SUCCESS;
    }

    virtual int EntryRemoved(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        if (!mcUpdates)
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }
        mFlags.fRegionsShanged = 1;
        return VINF_SUCCESS;
    }

    virtual int EntryDestroyed(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        return VINF_SUCCESS;
    }

    virtual int EntryPosChanged(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        if (!mcUpdates)
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }
        mFlags.fRegionsShanged = 1;
        return VINF_SUCCESS;
    }

    virtual int RegionsChanged(struct CR_FRAMEBUFFER *pFb)
    {
        if (!mcUpdates)
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }
        mFlags.fRegionsShanged = 1;
        return VINF_SUCCESS;
    }

    virtual int FramebufferChanged(struct CR_FRAMEBUFFER *pFb)
    {
        if (!mcUpdates)
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }
        return VINF_SUCCESS;
    }

    virtual ~CrFbDisplayBase();

    /*@todo: move to protected and switch from RTLISTNODE*/
    RTLISTNODE mNode;
    class CrFbDisplayComposite* mpContainer;
protected:
    virtual void onUpdateEnd()
    {
        if (mFlags.fRegionsShanged)
        {
            mFlags.fRegionsShanged = 0;
            if (getFramebuffer()) /*<-dont't do anything on cleanup*/
                ueRegions();
        }
    }

    virtual void ueRegions()
    {
    }

    static DECLCALLBACK(bool) entriesCreateCb(HCR_FRAMEBUFFER hFb, HCR_FRAMEBUFFER_ENTRY hEntry, void *pvContext)
    {
        int rc = ((ICrFbDisplay*)(pvContext))->EntryCreated(hFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
        }
        return true;
    }

    static DECLCALLBACK(bool) entriesDestroyCb(HCR_FRAMEBUFFER hFb, HCR_FRAMEBUFFER_ENTRY hEntry, void *pvContext)
    {
        int rc = ((ICrFbDisplay*)(pvContext))->EntryDestroyed(hFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
        }
        return true;
    }

    int fbSynchAddAllEntries()
    {
        VBOXVR_SCR_COMPOSITOR_CONST_ITERATOR Iter;
        const VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry;

        CrVrScrCompositorConstIterInit(CrFbGetCompositor(mpFb), &Iter);

        int rc = VINF_SUCCESS;

        CrFbVisitCreatedEntries(mpFb, entriesCreateCb, this);

        while ((pEntry = CrVrScrCompositorConstIterNext(&Iter)) != NULL)
        {
            HCR_FRAMEBUFFER_ENTRY hEntry = CrFbEntryFromCompositorEntry(pEntry);

            rc = EntryAdded(mpFb, hEntry);
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                EntryDestroyed(mpFb, hEntry);
                break;
            }
        }

        return rc;
    }

    int fbCleanupRemoveAllEntries()
    {
        VBOXVR_SCR_COMPOSITOR_CONST_ITERATOR Iter;
        const VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry;

        CrVrScrCompositorConstIterInit(CrFbGetCompositor(mpFb), &Iter);

        int rc = VINF_SUCCESS;

        while ((pEntry = CrVrScrCompositorConstIterNext(&Iter)) != NULL)
        {
            HCR_FRAMEBUFFER_ENTRY hEntry = CrFbEntryFromCompositorEntry(pEntry);
            rc = EntryRemoved(mpFb, hEntry);
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                break;
            }

            CrFbVisitCreatedEntries(mpFb, entriesDestroyCb, this);
        }

        return rc;
    }

    virtual int setFramebufferBegin(struct CR_FRAMEBUFFER *pFb)
    {
        return UpdateBegin(pFb);
    }
    virtual void setFramebufferEnd(struct CR_FRAMEBUFFER *pFb)
    {
        UpdateEnd(pFb);
    }

    static DECLCALLBACK(void) slotEntryReleaseCB(HCR_FRAMEBUFFER hFb, HCR_FRAMEBUFFER_ENTRY hEntry, void *pvContext)
    {
    }

    virtual void slotRelease()
    {
        Assert(mhSlot);
        CrFbDDataReleaseSlot(mpFb, mhSlot, slotEntryReleaseCB, this);
    }

    virtual int fbCleanup()
    {
        if (mhSlot)
        {
            slotRelease();
            mhSlot = 0;
        }
        mpFb = NULL;
        return VINF_SUCCESS;
    }

    virtual int fbSync()
    {
        return VINF_SUCCESS;
    }

    CRHTABLE_HANDLE slotGet()
    {
        if (!mhSlot)
        {
            if (mpFb)
                mhSlot = CrFbDDataAllocSlot(mpFb);
        }

        return mhSlot;
    }

private:
    struct CR_FRAMEBUFFER *mpFb;
    uint32_t mcUpdates;
    CRHTABLE_HANDLE mhSlot;
    CR_FBDISPBASE_FLAGS mFlags;
};

class CrFbDisplayComposite : public CrFbDisplayBase
{
public:
    CrFbDisplayComposite() :
        mcDisplays(0)
    {
        RTListInit(&mDisplays);
    }

    virtual bool isComposite()
    {
        return true;
    }

    uint32_t getDisplayCount()
    {
        return mcDisplays;
    }

    bool add(CrFbDisplayBase *pDisplay)
    {
        if (pDisplay->isInList())
        {
            WARN(("entry in list already"));
            return false;
        }

        RTListAppend(&mDisplays, &pDisplay->mNode);
        pDisplay->mpContainer = this;
        pDisplay->setFramebuffer(getFramebuffer());
        ++mcDisplays;
        return true;
    }

    bool remove(CrFbDisplayBase *pDisplay, bool fCleanupDisplay = true)
    {
        if (pDisplay->getContainer() != this)
        {
            WARN(("invalid entry container"));
            return false;
        }

        RTListNodeRemove(&pDisplay->mNode);
        pDisplay->mpContainer = NULL;
        if (fCleanupDisplay)
            pDisplay->setFramebuffer(NULL);
        --mcDisplays;
        return true;
    }

    CrFbDisplayBase* first()
    {
        return RTListGetFirstCpp(&mDisplays, CrFbDisplayBase, mNode);
    }

    CrFbDisplayBase* next(CrFbDisplayBase* pDisplay)
    {
        if (pDisplay->getContainer() != this)
        {
            WARN(("invalid entry container"));
            return NULL;
        }

        return RTListGetNextCpp(&mDisplays, pDisplay, CrFbDisplayBase, mNode);
    }

    virtual int setFramebuffer(struct CR_FRAMEBUFFER *pFb)
    {
        CrFbDisplayBase::setFramebuffer(pFb);

        CrFbDisplayBase *pIter;
        RTListForEachCpp(&mDisplays, pIter, CrFbDisplayBase, mNode)
        {
            pIter->setFramebuffer(pFb);
        }

        return VINF_SUCCESS;
    }

    virtual int UpdateBegin(struct CR_FRAMEBUFFER *pFb)
    {
        int rc = CrFbDisplayBase::UpdateBegin(pFb);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        CrFbDisplayBase *pIter;
        RTListForEachCpp(&mDisplays, pIter, CrFbDisplayBase, mNode)
        {
            rc = pIter->UpdateBegin(pFb);
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }
        return VINF_SUCCESS;
    }

    virtual void UpdateEnd(struct CR_FRAMEBUFFER *pFb)
    {
        CrFbDisplayBase *pIter;
        RTListForEachCpp(&mDisplays, pIter, CrFbDisplayBase, mNode)
        {
            pIter->UpdateEnd(pFb);
        }

        CrFbDisplayBase::UpdateEnd(pFb);
    }

    virtual int EntryAdded(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryAdded(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        CrFbDisplayBase *pIter;
        RTListForEachCpp(&mDisplays, pIter, CrFbDisplayBase, mNode)
        {
            int rc = pIter->EntryAdded(pFb, hEntry);
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }
        return VINF_SUCCESS;
    }

    virtual int EntryCreated(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryAdded(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        CrFbDisplayBase *pIter;
        RTListForEachCpp(&mDisplays, pIter, CrFbDisplayBase, mNode)
        {
            int rc = pIter->EntryCreated(pFb, hEntry);
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }
        return VINF_SUCCESS;
    }

    virtual int EntryReplaced(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hNewEntry, HCR_FRAMEBUFFER_ENTRY hReplacedEntry)
    {
        int rc = CrFbDisplayBase::EntryReplaced(pFb, hNewEntry, hReplacedEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        CrFbDisplayBase *pIter;
        RTListForEachCpp(&mDisplays, pIter, CrFbDisplayBase, mNode)
        {
            int rc = pIter->EntryReplaced(pFb, hNewEntry, hReplacedEntry);
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }
        return VINF_SUCCESS;
    }

    virtual int EntryTexChanged(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryTexChanged(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        CrFbDisplayBase *pIter;
        RTListForEachCpp(&mDisplays, pIter, CrFbDisplayBase, mNode)
        {
            int rc = pIter->EntryTexChanged(pFb, hEntry);
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }
        return VINF_SUCCESS;
    }

    virtual int EntryRemoved(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryRemoved(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        CrFbDisplayBase *pIter;
        RTListForEachCpp(&mDisplays, pIter, CrFbDisplayBase, mNode)
        {
            int rc = pIter->EntryRemoved(pFb, hEntry);
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }
        return VINF_SUCCESS;
    }

    virtual int EntryDestroyed(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryDestroyed(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        CrFbDisplayBase *pIter;
        RTListForEachCpp(&mDisplays, pIter, CrFbDisplayBase, mNode)
        {
            int rc = pIter->EntryDestroyed(pFb, hEntry);
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }
        return VINF_SUCCESS;
    }

    virtual int RegionsChanged(struct CR_FRAMEBUFFER *pFb)
    {
        int rc = CrFbDisplayBase::RegionsChanged(pFb);
          if (!RT_SUCCESS(rc))
          {
              WARN(("err"));
              return rc;
          }

        CrFbDisplayBase *pIter;
        RTListForEachCpp(&mDisplays, pIter, CrFbDisplayBase, mNode)
        {
            int rc = pIter->RegionsChanged(pFb);
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }
        return VINF_SUCCESS;
    }

    virtual int FramebufferChanged(struct CR_FRAMEBUFFER *pFb)
    {
        int rc = CrFbDisplayBase::FramebufferChanged(pFb);
          if (!RT_SUCCESS(rc))
          {
              WARN(("err"));
              return rc;
          }

        CrFbDisplayBase *pIter;
        RTListForEachCpp(&mDisplays, pIter, CrFbDisplayBase, mNode)
        {
            int rc = pIter->FramebufferChanged(pFb);
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }
        return VINF_SUCCESS;
    }

    virtual ~CrFbDisplayComposite()
    {
        cleanup();
    }

    void cleanup(bool fCleanupDisplays = true)
    {
        CrFbDisplayBase *pIter, *pIterNext;
        RTListForEachSafeCpp(&mDisplays, pIter, pIterNext, CrFbDisplayBase, mNode)
        {
            remove(pIter, fCleanupDisplays);
        }
    }
private:
    RTLISTNODE mDisplays;
    uint32_t mcDisplays;
};

typedef union CR_FBWIN_FLAGS
{
    struct {
        uint32_t fVisible : 1;
        uint32_t fDataPresented : 1;
        uint32_t fForcePresentOnReenable : 1;
        uint32_t fCompositoEntriesModified : 1;
        uint32_t Reserved : 28;
    };
    uint32_t Value;
} CR_FBWIN_FLAGS;

class CrFbWindow
{
public:
    CrFbWindow(uint64_t parentId) :
        mSpuWindow(0),
        mpCompositor(NULL),
        mcUpdates(0),
        mxPos(0),
        myPos(0),
        mWidth(0),
        mHeight(0),
        mParentId(parentId)
    {
        mFlags.Value = 0;
    }

    bool IsCreated()
    {
        return !!mSpuWindow;
    }

    void Destroy()
    {
        CRASSERT(!mcUpdates);

        if (!mSpuWindow)
            return;

        cr_server.head_spu->dispatch_table.WindowDestroy(mSpuWindow);

        mSpuWindow = 0;
        mFlags.fDataPresented = 0;
    }

    int Reparent(uint64_t parentId)
    {
        if (!checkInitedUpdating())
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }

        uint64_t oldParentId = mParentId;

        mParentId = parentId;

        if (mSpuWindow)
        {
            if (oldParentId && !parentId && mFlags.fVisible)
                cr_server.head_spu->dispatch_table.WindowShow(mSpuWindow, false);

            renderspuSetWindowId(mParentId);
            renderspuReparentWindow(mSpuWindow);
            renderspuSetWindowId(cr_server.screen[0].winID);

            if (parentId)
                cr_server.head_spu->dispatch_table.WindowPosition(mSpuWindow, mxPos, myPos);

            if (!oldParentId && parentId && mFlags.fVisible)
                cr_server.head_spu->dispatch_table.WindowShow(mSpuWindow, true);
        }

        return VINF_SUCCESS;
    }

    int SetVisible(bool fVisible)
    {
        if (!checkInitedUpdating())
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }

        LOG(("CrWIN: Vidible [%d]", fVisible));

        if (!fVisible != !mFlags.fVisible)
        {
            mFlags.fVisible = fVisible;
            if (mSpuWindow && mParentId)
                cr_server.head_spu->dispatch_table.WindowShow(mSpuWindow, fVisible);
        }

        return VINF_SUCCESS;
    }

    int SetSize(uint32_t width, uint32_t height)
    {
        if (!checkInitedUpdating())
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }

        LOG(("CrWIN: Size [%d ; %d]", width, height));

        if (mWidth != width || mHeight != height)
        {
            mFlags.fCompositoEntriesModified = 1;
            mWidth = width;
            mHeight = height;
            if (mSpuWindow)
                cr_server.head_spu->dispatch_table.WindowSize(mSpuWindow, width, height);
        }

        return VINF_SUCCESS;
    }

    int SetPosition(int32_t x, int32_t y)
    {
        if (!checkInitedUpdating())
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }

        LOG(("CrWIN: Pos [%d ; %d]", x, y));
//      always do WindowPosition to ensure window is adjusted properly
//        if (x != mxPos || y != myPos)
        {
            mxPos = x;
            myPos = y;
            if (mSpuWindow)
                cr_server.head_spu->dispatch_table.WindowPosition(mSpuWindow, x, y);
        }

        return VINF_SUCCESS;
    }

    int SetVisibleRegionsChanged()
    {
        if (!checkInitedUpdating())
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }

        mFlags.fCompositoEntriesModified = 1;
        return VINF_SUCCESS;
    }

    int SetCompositor(const struct VBOXVR_SCR_COMPOSITOR * pCompositor)
    {
        if (!checkInitedUpdating())
        {
            WARN(("err"));
            return VERR_INVALID_STATE;
        }

        mpCompositor = pCompositor;
        mFlags.fCompositoEntriesModified = 1;
        return VINF_SUCCESS;
    }

    int UpdateBegin()
    {
        ++mcUpdates;
        if (mcUpdates > 1)
            return VINF_SUCCESS;

        Assert(!mFlags.fForcePresentOnReenable);
//        Assert(!mFlags.fCompositoEntriesModified);

        if (mFlags.fDataPresented)
        {
            Assert(mSpuWindow);
            cr_server.head_spu->dispatch_table.VBoxPresentComposition(mSpuWindow, NULL, NULL);
            mFlags.fForcePresentOnReenable = isPresentNeeded();
        }

        return VINF_SUCCESS;
    }

    void UpdateEnd()
    {
        --mcUpdates;
        Assert(mcUpdates < UINT32_MAX/2);
        if (mcUpdates)
            return;

        checkRegions();

        if (mSpuWindow)
        {
            bool fPresentNeeded = isPresentNeeded();
            if (fPresentNeeded || mFlags.fForcePresentOnReenable)
            {
                mFlags.fForcePresentOnReenable = false;
                cr_server.head_spu->dispatch_table.VBoxPresentComposition(mSpuWindow, mpCompositor, NULL);
            }

            /* even if the above branch is entered due to mFlags.fForcePresentOnReenable,
             * the backend should clean up the compositor as soon as presentation is performed */
            mFlags.fDataPresented = fPresentNeeded;
        }
        else
        {
            Assert(!mFlags.fDataPresented);
            Assert(!mFlags.fForcePresentOnReenable);
        }
    }

    uint64_t GetParentId()
    {
        return mParentId;
    }

    int Create()
    {
        if (mSpuWindow)
        {
            //WARN(("window already created"));
            return VINF_ALREADY_INITIALIZED;
        }

        CRASSERT(cr_server.fVisualBitsDefault);
        renderspuSetWindowId(mParentId);
        mSpuWindow = cr_server.head_spu->dispatch_table.WindowCreate("", cr_server.fVisualBitsDefault);
        renderspuSetWindowId(cr_server.screen[0].winID);
        if (mSpuWindow < 0) {
            WARN(("WindowCreate failed"));
            return VERR_GENERAL_FAILURE;
        }

        cr_server.head_spu->dispatch_table.WindowSize(mSpuWindow, mWidth, mHeight);
        cr_server.head_spu->dispatch_table.WindowPosition(mSpuWindow, mxPos, myPos);

        checkRegions();

        if (mParentId && mFlags.fVisible)
            cr_server.head_spu->dispatch_table.WindowShow(mSpuWindow, true);

        return VINF_SUCCESS;
    }

    ~CrFbWindow()
    {
        Destroy();
    }
protected:
    void checkRegions()
    {
        if (!mSpuWindow)
            return;

        if (!mFlags.fCompositoEntriesModified)
            return;

        uint32_t cRects;
        const RTRECT *pRects;
        if (mpCompositor)
        {
            int rc = CrVrScrCompositorRegionsGet(mpCompositor, &cRects, NULL, &pRects, NULL);
            if (!RT_SUCCESS(rc))
            {
                WARN(("CrVrScrCompositorRegionsGet failed rc %d", rc));
                cRects = 0;
                pRects = NULL;
            }
        }
        else
        {
            cRects = 0;
            pRects = NULL;
        }

        cr_server.head_spu->dispatch_table.WindowVisibleRegion(mSpuWindow, cRects, (const GLint*)pRects);

        mFlags.fCompositoEntriesModified = 0;
    }

    bool isPresentNeeded()
    {
        return mFlags.fVisible && mWidth && mHeight && mpCompositor && !CrVrScrCompositorIsEmpty(mpCompositor);
    }

    bool checkInitedUpdating()
    {
        if (!mcUpdates)
        {
            WARN(("not updating"));
            return false;
        }

        return true;
    }
private:
    GLint mSpuWindow;
    const struct VBOXVR_SCR_COMPOSITOR * mpCompositor;
    uint32_t mcUpdates;
    int32_t mxPos;
    int32_t myPos;
    uint32_t mWidth;
    uint32_t mHeight;
    CR_FBWIN_FLAGS mFlags;
    uint64_t mParentId;
};

typedef union CR_FBDISPWINDOW_FLAGS
{
    struct {
        uint32_t fNeVisible : 1;
        uint32_t fNeForce   : 1;
        uint32_t Reserved   : 30;
    };
    uint32_t u32Value;
} CR_FBDISPWINDOW_FLAGS;
class CrFbDisplayWindow : public CrFbDisplayBase
{
public:
    CrFbDisplayWindow(CrFbWindow *pWindow, const RTRECT *pViewportRect) :
        mpWindow(pWindow),
        mViewportRect(*pViewportRect),
        mu32Screen(~0)
    {
        mFlags.u32Value = 0;
        CRASSERT(pWindow);
    }

    virtual ~CrFbDisplayWindow()
    {
        if (mpWindow)
            delete mpWindow;
    }

    virtual int UpdateBegin(struct CR_FRAMEBUFFER *pFb)
    {
        int rc = mpWindow->UpdateBegin();
        if (RT_SUCCESS(rc))
        {
            rc = CrFbDisplayBase::UpdateBegin(pFb);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
            else
                WARN(("err"));
        }
        else
            WARN(("err"));

        return rc;
    }

    virtual void UpdateEnd(struct CR_FRAMEBUFFER *pFb)
    {
        CrFbDisplayBase::UpdateEnd(pFb);

        mpWindow->UpdateEnd();
    }

    virtual int EntryCreated(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryCreated(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        if (mpWindow->GetParentId())
        {
            rc = mpWindow->Create();
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }

        return VINF_SUCCESS;
    }

    virtual int EntryReplaced(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hNewEntry, HCR_FRAMEBUFFER_ENTRY hReplacedEntry)
    {
        int rc = CrFbDisplayBase::EntryReplaced(pFb, hNewEntry, hReplacedEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        if (mpWindow->GetParentId())
        {
            rc = mpWindow->Create();
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }

        return VINF_SUCCESS;
    }

    virtual int EntryTexChanged(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryTexChanged(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        if (mpWindow->GetParentId())
        {
            rc = mpWindow->Create();
            if (!RT_SUCCESS(rc))
            {
                WARN(("err"));
                return rc;
            }
        }

        return VINF_SUCCESS;
    }

    virtual int FramebufferChanged(struct CR_FRAMEBUFFER *pFb)
    {
        int rc = CrFbDisplayBase::FramebufferChanged(pFb);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        return screenChanged();
    }

    const RTRECT* getViewportRect()
    {
        return &mViewportRect;
    }

    virtual int setViewportRect(const RTRECT *pViewportRect)
    {
        if (!isUpdating())
        {
            WARN(("not updating!"));
            return VERR_INVALID_STATE;
        }

// always call SetPosition to ensure window is adjustep properly
//        if (pViewportRect->xLeft != mViewportRect.xLeft || pViewportRect->yTop != mViewportRect.yTop)
        {
            const RTRECT* pRect = getRect();
            int rc = mpWindow->SetPosition(pRect->xLeft - pViewportRect->xLeft, pRect->yTop - pViewportRect->yTop);
            if (!RT_SUCCESS(rc))
            {
                WARN(("SetPosition failed"));
                return rc;
            }
        }

        mViewportRect = *pViewportRect;

        return VINF_SUCCESS;
    }

    virtual CrFbWindow * windowDetach()
    {
        if (isUpdating())
        {
            WARN(("updating!"));
            return NULL;
        }

        CrFbWindow * pWindow = mpWindow;
        if (mpWindow)
        {
            windowCleanup();
            mpWindow = NULL;
        }
        return pWindow;
    }

    virtual CrFbWindow * windowAttach(CrFbWindow * pNewWindow)
    {
        if (isUpdating())
        {
            WARN(("updating!"));
            return NULL;
        }

        CrFbWindow * pOld = mpWindow;
        if (mpWindow)
            windowDetach();

        mpWindow = pNewWindow;
        if (pNewWindow)
            windowSync();

        return mpWindow;
    }

    virtual int reparent(uint64_t parentId)
    {
        if (!isUpdating())
        {
            WARN(("not updating!"));
            return VERR_INVALID_STATE;
        }

        int rc = mpWindow->Reparent(parentId);
        if (!RT_SUCCESS(rc))
            WARN(("window reparent failed"));

        mFlags.fNeForce = 1;

        return rc;
    }

    virtual bool isVisible()
    {
        HCR_FRAMEBUFFER hFb = getFramebuffer();
        if (!hFb)
            return false;
        const struct VBOXVR_SCR_COMPOSITOR* pCompositor = CrFbGetCompositor(hFb);
        return !CrVrScrCompositorIsEmpty(pCompositor);
    }

    int winVisibilityChanged()
    {
        int rc = mpWindow->UpdateBegin();
        if (RT_SUCCESS(rc))
        {
            rc = mpWindow->SetVisible(!g_CrPresenter.fWindowsForceHidden);
            if (!RT_SUCCESS(rc))
                WARN(("SetVisible failed, rc %d", rc));

            mpWindow->UpdateEnd();
        }
        else
            WARN(("UpdateBegin failed, rc %d", rc));

        return rc;
    }

protected:
    virtual void onUpdateEnd()
    {
        CrFbDisplayBase::onUpdateEnd();
        bool fVisible = isVisible();
        if (mFlags.fNeVisible != fVisible || mFlags.fNeForce)
        {
            crVBoxServerNotifyEvent(mu32Screen, VBOX3D_NOTIFY_EVENT_TYPE_VISIBLE_3DDATA, fVisible ? (void*)1 : NULL);
            mFlags.fNeVisible = fVisible;
            mFlags.fNeForce = 0;
        }
    }

    virtual void ueRegions()
    {
        mpWindow->SetVisibleRegionsChanged();
    }

    virtual int screenChanged()
    {
        if (!isUpdating())
        {
            WARN(("not updating!"));
            return VERR_INVALID_STATE;
        }

        if (CrFbIsEnabled(getFramebuffer()))
        {
            const RTRECT* pRect = getRect();
            int rc = mpWindow->SetPosition(pRect->xLeft - mViewportRect.xLeft, pRect->yTop - mViewportRect.yTop);
            if (!RT_SUCCESS(rc))
            {
                WARN(("SetComposition failed rc %d", rc));
                return rc;
            }

            setRegionsChanged();

            return mpWindow->SetSize((uint32_t)(pRect->xRight - pRect->xLeft), (uint32_t)(pRect->yBottom - pRect->yTop));
        }

        return mpWindow->SetVisible(false);
    }

    virtual int windowSetCompositor(bool fSet)
    {
        if (fSet)
        {
            const struct VBOXVR_SCR_COMPOSITOR* pCompositor = CrFbGetCompositor(getFramebuffer());
            return mpWindow->SetCompositor(pCompositor);
        }
        return mpWindow->SetCompositor(NULL);
    }

    virtual int windowCleanup()
    {
        int rc = mpWindow->UpdateBegin();
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        rc = mpWindow->SetVisible(false);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            mpWindow->UpdateEnd();
            return rc;
        }

        rc = windowSetCompositor(false);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            mpWindow->UpdateEnd();
            return rc;
        }

        mpWindow->UpdateEnd();

        return VINF_SUCCESS;
    }

    virtual int fbCleanup()
    {
        int rc = windowCleanup();
        if (!RT_SUCCESS(rc))
        {
            WARN(("windowCleanup failed"));
            return rc;
        }
        return CrFbDisplayBase::fbCleanup();
    }

    virtual int windowSync()
    {
        const RTRECT* pRect = getRect();

        int rc = mpWindow->UpdateBegin();
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        rc = windowSetCompositor(true);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            mpWindow->UpdateEnd();
            return rc;
        }

        rc = mpWindow->SetPosition(pRect->xLeft - mViewportRect.xLeft, pRect->yTop - mViewportRect.yTop);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            mpWindow->UpdateEnd();
            return rc;
        }

        rc = mpWindow->SetSize((uint32_t)(pRect->xRight - pRect->xLeft), (uint32_t)(pRect->yBottom - pRect->yTop));
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            mpWindow->UpdateEnd();
            return rc;
        }

        rc = mpWindow->SetVisible(!g_CrPresenter.fWindowsForceHidden);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            mpWindow->UpdateEnd();
            return rc;
        }

        mpWindow->UpdateEnd();

        return rc;
    }

    virtual int fbSync()
    {
        int rc = CrFbDisplayBase::fbSync();
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        mu32Screen = CrFbGetScreenInfo(getFramebuffer())->u32ViewIndex;

        return windowSync();
    }

    virtual const struct RTRECT* getRect()
    {
        const struct VBOXVR_SCR_COMPOSITOR* pCompositor = CrFbGetCompositor(getFramebuffer());
        return CrVrScrCompositorRectGet(pCompositor);
    }

    CrFbWindow* getWindow() {return mpWindow;}
private:
    CrFbWindow *mpWindow;
    RTRECT mViewportRect;
    CR_FBDISPWINDOW_FLAGS mFlags;
    uint32_t mu32Screen;
};

class CrFbDisplayWindowRootVr : public CrFbDisplayWindow
{
public:
    CrFbDisplayWindowRootVr(CrFbWindow *pWindow, const RTRECT *pViewportRect) :
        CrFbDisplayWindow(pWindow, pViewportRect)
    {
        CrVrScrCompositorInit(&mCompositor, NULL);
    }

    virtual int EntryCreated(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayWindow::EntryCreated(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        Assert(!CrFbDDataEntryGet(hEntry, slotGet()));

        const VBOXVR_SCR_COMPOSITOR_ENTRY* pSrcEntry = CrFbEntryGetCompositorEntry(hEntry);
        VBOXVR_SCR_COMPOSITOR_ENTRY *pMyEntry = entryAlloc();
        CrVrScrCompositorEntryInit(pMyEntry, CrVrScrCompositorEntryRectGet(pSrcEntry), CrVrScrCompositorEntryTexGet(pSrcEntry), NULL);
        CrVrScrCompositorEntryFlagsSet(pMyEntry, CrVrScrCompositorEntryFlagsGet(pSrcEntry));
        rc = CrFbDDataEntryPut(hEntry, slotGet(), pMyEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrFbDDataEntryPut failed rc %d", rc));
            entryFree(pMyEntry);
            return rc;
        }

        return VINF_SUCCESS;
    }

    virtual int EntryAdded(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayWindow::EntryAdded(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        const VBOXVR_SCR_COMPOSITOR_ENTRY* pSrcEntry = CrFbEntryGetCompositorEntry(hEntry);
        VBOXVR_SCR_COMPOSITOR_ENTRY *pMyEntry = (VBOXVR_SCR_COMPOSITOR_ENTRY*)CrFbDDataEntryGet(hEntry, slotGet());
        Assert(pMyEntry);
        CrVrScrCompositorEntryTexSet(pMyEntry, CrVrScrCompositorEntryTexGet(pSrcEntry));

        return VINF_SUCCESS;
    }

    virtual int EntryReplaced(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hNewEntry, HCR_FRAMEBUFFER_ENTRY hReplacedEntry)
    {
        int rc = CrFbDisplayWindow::EntryReplaced(pFb, hNewEntry, hReplacedEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        const VBOXVR_SCR_COMPOSITOR_ENTRY* pSrcNewEntry = CrFbEntryGetCompositorEntry(hNewEntry);
        VBOXVR_SCR_COMPOSITOR_ENTRY *pMyEntry = (VBOXVR_SCR_COMPOSITOR_ENTRY*)CrFbDDataEntryGet(hNewEntry, slotGet());
        CrVrScrCompositorEntryTexSet(pMyEntry, CrVrScrCompositorEntryTexGet(pSrcNewEntry));

        return VINF_SUCCESS;
    }

    virtual int EntryTexChanged(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayWindow::EntryTexChanged(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        const VBOXVR_SCR_COMPOSITOR_ENTRY* pSrcEntry = CrFbEntryGetCompositorEntry(hEntry);
        VBOXVR_SCR_COMPOSITOR_ENTRY *pMyEntry = (VBOXVR_SCR_COMPOSITOR_ENTRY*)CrFbDDataEntryGet(hEntry, slotGet());
        CrVrScrCompositorEntryTexSet(pMyEntry, CrVrScrCompositorEntryTexGet(pSrcEntry));

        return VINF_SUCCESS;
    }

    virtual int EntryRemoved(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayWindow::EntryRemoved(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        VBOXVR_SCR_COMPOSITOR_ENTRY *pMyEntry = (VBOXVR_SCR_COMPOSITOR_ENTRY*)CrFbDDataEntryGet(hEntry, slotGet());
        rc = CrVrScrCompositorEntryRegionsSet(&mCompositor, pMyEntry, NULL, 0, NULL, false, NULL);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        return VINF_SUCCESS;
    }

    virtual int EntryDestroyed(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayWindow::EntryDestroyed(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        const VBOXVR_SCR_COMPOSITOR_ENTRY* pSrcEntry = CrFbEntryGetCompositorEntry(hEntry);
        VBOXVR_SCR_COMPOSITOR_ENTRY *pMyEntry = (VBOXVR_SCR_COMPOSITOR_ENTRY*)CrFbDDataEntryGet(hEntry, slotGet());
        CrVrScrCompositorEntryCleanup(pMyEntry);
        entryFree(pMyEntry);

        return VINF_SUCCESS;
    }

    virtual int setViewportRect(const RTRECT *pViewportRect)
    {
        int rc = CrFbDisplayWindow::setViewportRect(pViewportRect);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        rc = setRegionsChanged();
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        return VINF_SUCCESS;
    }

protected:
    virtual int windowSetCompositor(bool fSet)
    {
        if (fSet)
            return getWindow()->SetCompositor(&mCompositor);
        return getWindow()->SetCompositor(NULL);
    }

    virtual void ueRegions()
    {
        synchCompositorRegions();
    }

    int compositorMarkUpdated()
    {
        CrVrScrCompositorClear(&mCompositor);

        int rc = CrVrScrCompositorRectSet(&mCompositor, CrVrScrCompositorRectGet(CrFbGetCompositor(getFramebuffer())), NULL);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        rc = setRegionsChanged();
        if (!RT_SUCCESS(rc))
        {
            WARN(("screenChanged failed %d", rc));
            return rc;
        }

        return VINF_SUCCESS;
    }

    virtual int screenChanged()
    {
        int rc = compositorMarkUpdated();
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        rc = CrFbDisplayWindow::screenChanged();
        if (!RT_SUCCESS(rc))
        {
            WARN(("screenChanged failed %d", rc));
            return rc;
        }

        return VINF_SUCCESS;
    }

    virtual const struct RTRECT* getRect()
    {
        return CrVrScrCompositorRectGet(&mCompositor);
    }

    virtual int fbCleanup()
    {
        int rc = clearCompositor();
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        return CrFbDisplayWindow::fbCleanup();
    }

    virtual int fbSync()
    {
        int rc = synchCompositor();
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        return CrFbDisplayWindow::fbSync();
    }

    VBOXVR_SCR_COMPOSITOR_ENTRY* entryAlloc()
    {
#ifndef VBOXVDBG_MEMCACHE_DISABLE
        return (VBOXVR_SCR_COMPOSITOR_ENTRY*)RTMemCacheAlloc(g_CrPresenter.CEntryLookasideList);
#else
        return (VBOXVR_SCR_COMPOSITOR_ENTRY*)RTMemAlloc(sizeof (VBOXVR_SCR_COMPOSITOR_ENTRY));
#endif
    }

    void entryFree(VBOXVR_SCR_COMPOSITOR_ENTRY* pEntry)
    {
        Assert(!CrVrScrCompositorEntryIsUsed(pEntry));
#ifndef VBOXVDBG_MEMCACHE_DISABLE
        RTMemCacheFree(g_CrPresenter.CEntryLookasideList, pEntry);
#else
        RTMemFree(pEntry);
#endif
    }

    int synchCompositorRegions()
    {
        int rc;

        rootVrTranslateForPos();

        /* ensure the rootvr compositor does not hold any data,
         * i.e. cleanup all rootvr entries data */
        CrVrScrCompositorClear(&mCompositor);

        rc = CrVrScrCompositorIntersectedList(CrFbGetCompositor(getFramebuffer()), &cr_server.RootVr, &mCompositor, rootVrGetCEntry, this, NULL);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrVrScrCompositorIntersectedList failed, rc %d", rc));
            return rc;
        }

        return getWindow()->SetVisibleRegionsChanged();
    }

    virtual int synchCompositor()
    {
        int rc = compositorMarkUpdated();
        if (!RT_SUCCESS(rc))
        {
            WARN(("compositorMarkUpdated failed, rc %d", rc));
            return rc;
        }

        rc = fbSynchAddAllEntries();
        if (!RT_SUCCESS(rc))
        {
            WARN(("fbSynchAddAllEntries failed, rc %d", rc));
            return rc;
        }

        return rc;
    }

    virtual int clearCompositor()
    {
        return fbCleanupRemoveAllEntries();
    }

    void rootVrTranslateForPos()
    {
        const RTRECT *pRect = getViewportRect();
        const struct VBVAINFOSCREEN* pScreen = CrFbGetScreenInfo(getFramebuffer());
        int32_t x = pScreen->i32OriginX;
        int32_t y = pScreen->i32OriginY;
        int32_t dx = cr_server.RootVrCurPoint.x - x;
        int32_t dy = cr_server.RootVrCurPoint.y - y;

        cr_server.RootVrCurPoint.x = x;
        cr_server.RootVrCurPoint.y = y;

        VBoxVrListTranslate(&cr_server.RootVr, dx, dy);
    }

    static DECLCALLBACK(VBOXVR_SCR_COMPOSITOR_ENTRY*) rootVrGetCEntry(const VBOXVR_SCR_COMPOSITOR_ENTRY*pEntry, void *pvContext)
    {
        CrFbDisplayWindowRootVr *pThis = (CrFbDisplayWindowRootVr*)pvContext;
        HCR_FRAMEBUFFER_ENTRY hEntry = CrFbEntryFromCompositorEntry(pEntry);
        VBOXVR_SCR_COMPOSITOR_ENTRY *pMyEntry = (VBOXVR_SCR_COMPOSITOR_ENTRY*)CrFbDDataEntryGet(hEntry, pThis->slotGet());
        Assert(!CrVrScrCompositorEntryIsUsed(pMyEntry));
        CrVrScrCompositorEntryRectSet(&pThis->mCompositor, pMyEntry, CrVrScrCompositorEntryRectGet(pEntry));
        return pMyEntry;
    }
private:
    VBOXVR_SCR_COMPOSITOR mCompositor;
};

class CrFbDisplayVrdp : public CrFbDisplayBase
{
public:
    CrFbDisplayVrdp()
    {
        memset(&mPos, 0, sizeof (mPos));
    }

    virtual int EntryCreated(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryCreated(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("EntryAdded failed rc %d", rc));
            return rc;
        }

        Assert(!CrFbDDataEntryGet(hEntry, slotGet()));
        rc = vrdpCreate(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("vrdpCreate failed rc %d", rc));
            return rc;
        }

        return VINF_SUCCESS;
    }

    virtual int EntryReplaced(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hNewEntry, HCR_FRAMEBUFFER_ENTRY hReplacedEntry)
    {
        int rc = CrFbDisplayBase::EntryReplaced(pFb, hNewEntry, hReplacedEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        const VBOXVR_SCR_COMPOSITOR_ENTRY* pReplacedEntry = CrFbEntryGetCompositorEntry(hReplacedEntry);
        CR_TEXDATA *pReplacedTex = CrVrScrCompositorEntryTexGet(pReplacedEntry);
        const VBOXVR_SCR_COMPOSITOR_ENTRY* pNewEntry = CrFbEntryGetCompositorEntry(hNewEntry);
        CR_TEXDATA *pNewTex = CrVrScrCompositorEntryTexGet(pNewEntry);

        CrTdBltDataInvalidateNe(pReplacedTex);

        rc = CrTdBltEnter(pNewTex);
        if (RT_SUCCESS(rc))
        {
            rc = vrdpFrame(hNewEntry);
            CrTdBltLeave(pNewTex);
        }
        else
            WARN(("CrTdBltEnter failed %d", rc));

        return rc;
    }

    virtual int EntryTexChanged(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryTexChanged(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        const VBOXVR_SCR_COMPOSITOR_ENTRY* pEntry = CrFbEntryGetCompositorEntry(hEntry);
        CR_TEXDATA *pTex = CrVrScrCompositorEntryTexGet(pEntry);

        rc = CrTdBltEnter(pTex);
        if (RT_SUCCESS(rc))
        {
            rc = vrdpFrame(hEntry);
            CrTdBltLeave(pTex);
        }
        else
            WARN(("CrTdBltEnter failed %d", rc));

    	return rc;
    }

    virtual int EntryRemoved(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryRemoved(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        const VBOXVR_SCR_COMPOSITOR_ENTRY* pEntry = CrFbEntryGetCompositorEntry(hEntry);
        CR_TEXDATA *pTex = CrVrScrCompositorEntryTexGet(pEntry);
        CrTdBltDataInvalidateNe(pTex);

        return vrdpRegions(pFb, hEntry);
    }

    virtual int EntryDestroyed(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryDestroyed(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        vrdpDestroy(hEntry);
        return VINF_SUCCESS;
    }

    virtual int EntryPosChanged(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        int rc = CrFbDisplayBase::EntryPosChanged(pFb, hEntry);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        vrdpGeometry(hEntry);

        return VINF_SUCCESS;
    }

    virtual int RegionsChanged(struct CR_FRAMEBUFFER *pFb)
    {
        int rc = CrFbDisplayBase::RegionsChanged(pFb);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        return vrdpRegionsAll(pFb);
    }

    virtual int FramebufferChanged(struct CR_FRAMEBUFFER *pFb)
    {
        int rc = CrFbDisplayBase::FramebufferChanged(pFb);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        syncPos();

        rc = vrdpSyncEntryAll(pFb);
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        return vrdpRegionsAll(pFb);
    }

protected:
    void syncPos()
    {
        const struct VBVAINFOSCREEN* pScreenInfo = CrFbGetScreenInfo(getFramebuffer());
        mPos.x = pScreenInfo->i32OriginX;
        mPos.y = pScreenInfo->i32OriginY;
    }

    virtual int fbCleanup()
    {
        int rc = fbCleanupRemoveAllEntries();
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        return CrFbDisplayBase::fbCleanup();
    }

    virtual int fbSync()
    {
        syncPos();

        int rc = fbSynchAddAllEntries();
        if (!RT_SUCCESS(rc))
        {
            WARN(("err"));
            return rc;
        }

        return CrFbDisplayBase::fbSync();
    }
protected:
    void vrdpDestroy(HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        void *pVrdp = CrFbDDataEntryGet(hEntry, slotGet());
        cr_server.outputRedirect.CROREnd(pVrdp);
    }

    void vrdpGeometry(HCR_FRAMEBUFFER_ENTRY hEntry)
    {
    	void *pVrdp = CrFbDDataEntryGet(hEntry, slotGet());
        const VBOXVR_SCR_COMPOSITOR_ENTRY* pEntry = CrFbEntryGetCompositorEntry(hEntry);

        cr_server.outputRedirect.CRORGeometry(pVrdp,
        										mPos.x + CrVrScrCompositorEntryRectGet(pEntry)->xLeft,
        										mPos.y + CrVrScrCompositorEntryRectGet(pEntry)->yTop,
        									   CrVrScrCompositorEntryTexGet(pEntry)->Tex.width,
                                               CrVrScrCompositorEntryTexGet(pEntry)->Tex.height);
    }

    int vrdpRegions(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        void *pVrdp = CrFbDDataEntryGet(hEntry, slotGet());
    	const struct VBOXVR_SCR_COMPOSITOR* pCompositor = CrFbGetCompositor(pFb);
        const VBOXVR_SCR_COMPOSITOR_ENTRY* pEntry = CrFbEntryGetCompositorEntry(hEntry);
        uint32_t cRects;
        const RTRECT *pRects;

        int rc = CrVrScrCompositorEntryRegionsGet(pCompositor, pEntry, &cRects, NULL, &pRects, NULL);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrVrScrCompositorEntryRegionsGet failed, rc %d", rc));
            return rc;
        }

        cr_server.outputRedirect.CRORVisibleRegion(pVrdp, cRects, pRects);
        return VINF_SUCCESS;
    }

    int vrdpFrame(HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        void *pVrdp = CrFbDDataEntryGet(hEntry, slotGet());
        const VBOXVR_SCR_COMPOSITOR_ENTRY* pEntry = CrFbEntryGetCompositorEntry(hEntry);
    	CR_TEXDATA *pTex = CrVrScrCompositorEntryTexGet(pEntry);
    	const CR_BLITTER_IMG *pImg;
    	CrTdBltDataInvalidateNe(pTex);
    	int rc = CrTdBltDataAcquire(pTex, GL_BGRA, !!(CrVrScrCompositorEntryFlagsGet(pEntry) & CRBLT_F_INVERT_SRC_YCOORDS), &pImg);
    	if (!RT_SUCCESS(rc))
    	{
    		WARN(("CrTdBltDataAcquire failed rc %d", rc));
    		return rc;
    	}

        cr_server.outputRedirect.CRORFrame(pVrdp, pImg->pvData, pImg->cbData);
        CrTdBltDataRelease(pTex);
        return VINF_SUCCESS;
    }

    int vrdpRegionsAll(struct CR_FRAMEBUFFER *pFb)
    {
    	const struct VBOXVR_SCR_COMPOSITOR* pCompositor = CrFbGetCompositor(pFb);
        VBOXVR_SCR_COMPOSITOR_CONST_ITERATOR Iter;
        CrVrScrCompositorConstIterInit(pCompositor, &Iter);
        const VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry;
        while ((pEntry = CrVrScrCompositorConstIterNext(&Iter)) != NULL)
        {
        	HCR_FRAMEBUFFER_ENTRY hEntry = CrFbEntryFromCompositorEntry(pEntry);
        	vrdpRegions(pFb, hEntry);
        }

        return VINF_SUCCESS;
    }

    int vrdpSynchEntry(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        vrdpGeometry(hEntry);

        return vrdpRegions(pFb, hEntry);;
    }

    int vrdpSyncEntryAll(struct CR_FRAMEBUFFER *pFb)
    {
        const struct VBOXVR_SCR_COMPOSITOR* pCompositor = CrFbGetCompositor(pFb);
        VBOXVR_SCR_COMPOSITOR_CONST_ITERATOR Iter;
        CrVrScrCompositorConstIterInit(pCompositor, &Iter);
        const VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry;
        while ((pEntry = CrVrScrCompositorConstIterNext(&Iter)) != NULL)
        {
            HCR_FRAMEBUFFER_ENTRY hEntry = CrFbEntryFromCompositorEntry(pEntry);
            int rc = vrdpSynchEntry(pFb, hEntry);
            if (!RT_SUCCESS(rc))
            {
                WARN(("vrdpSynchEntry failed rc %d", rc));
                return rc;
            }
        }

        return VINF_SUCCESS;
    }

    int vrdpCreate(HCR_FRAMEBUFFER hFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
    	void *pVrdp;

        /* Query supported formats. */
        uint32_t cbFormats = 4096;
        char *pachFormats = (char *)crAlloc(cbFormats);

        if (!pachFormats)
        {
            WARN(("crAlloc failed"));
            return VERR_NO_MEMORY;
        }

        int rc = cr_server.outputRedirect.CRORContextProperty(cr_server.outputRedirect.pvContext,
                                                                  0 /* H3DOR_PROP_FORMATS */, // @todo from a header
                                                                  pachFormats, cbFormats, &cbFormats);
        if (RT_SUCCESS(rc))
        {
            if (RTStrStr(pachFormats, "H3DOR_FMT_RGBA_TOPDOWN"))
            {
                cr_server.outputRedirect.CRORBegin(cr_server.outputRedirect.pvContext,
                		        &pVrdp,
                                "H3DOR_FMT_RGBA_TOPDOWN"); // @todo from a header

                if (pVrdp)
                {
                    rc = CrFbDDataEntryPut(hEntry, slotGet(), pVrdp);
                    if (RT_SUCCESS(rc))
                    {
                    	vrdpGeometry(hEntry);
                    	vrdpRegions(hFb, hEntry);
                    	//vrdpFrame(hEntry);
                        return VINF_SUCCESS;
                    }
                    else
                    	WARN(("CrFbDDataEntryPut failed rc %d", rc));

                    cr_server.outputRedirect.CROREnd(pVrdp);
                }
                else
                {
                    WARN(("CRORBegin failed"));
                    rc = VERR_GENERAL_FAILURE;
                }
            }
        }
        else
            WARN(("CRORContextProperty failed rc %d", rc));

        crFree(pachFormats);

        return rc;
    }
private:
    RTPOINT mPos;
};

CrFbDisplayBase::~CrFbDisplayBase()
{
    Assert(!mcUpdates);

    if (mpContainer)
        mpContainer->remove(this);
}


#if 0





void crDbgDumpRect(uint32_t i, const RTRECT *pRect)
{
    crDebug("%d: (%d;%d) X (%d;%d)", i, pRect->xLeft, pRect->yTop, pRect->xRight, pRect->yBottom);
}

void crDbgDumpRects(uint32_t cRects, const RTRECT *paRects)
{
    crDebug("Dumping rects (%d)", cRects);
    for (uint32_t i = 0; i < cRects; ++i)
    {
        crDbgDumpRect(i, &paRects[i]);
    }
    crDebug("End Dumping rects (%d)", cRects);
}

int crServerDisplaySaveState(PSSMHANDLE pSSM)
{
    int rc;
    int cDisplays = 0, i;
    for (i = 0; i < cr_server.screenCount; ++i)
    {
        if (ASMBitTest(cr_server.DisplaysInitMap, i) && !CrDpIsEmpty(&cr_server.aDispplays[i]))
            ++cDisplays;
    }

    rc = SSMR3PutS32(pSSM, cDisplays);
    AssertRCReturn(rc, rc);

    if (!cDisplays)
        return VINF_SUCCESS;

    rc = SSMR3PutS32(pSSM, cr_server.screenCount);
    AssertRCReturn(rc, rc);

    for (i = 0; i < cr_server.screenCount; ++i)
    {
        rc = SSMR3PutS32(pSSM, cr_server.screen[i].x);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutS32(pSSM, cr_server.screen[i].y);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutU32(pSSM, cr_server.screen[i].w);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutU32(pSSM, cr_server.screen[i].h);
        AssertRCReturn(rc, rc);
    }

    for (i = 0; i < cr_server.screenCount; ++i)
    {
        if (ASMBitTest(cr_server.DisplaysInitMap, i) && !CrDpIsEmpty(&cr_server.aDispplays[i]))
        {
            rc = SSMR3PutS32(pSSM, i);
            AssertRCReturn(rc, rc);

            rc = CrDpSaveState(&cr_server.aDispplays[i], pSSM);
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}

int crServerDisplayLoadState(PSSMHANDLE pSSM, uint32_t u32Version)
{

}
#endif

class CrFbDisplayEntryDataMonitor : public CrFbDisplayBase
{
public:
    virtual int EntryReplaced(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hNewEntry, HCR_FRAMEBUFFER_ENTRY hReplacedEntry)
    {
        entryDataChanged(pFb, hReplacedEntry);
        return VINF_SUCCESS;
    }

    virtual int EntryTexChanged(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        entryDataChanged(pFb, hEntry);
        return VINF_SUCCESS;
    }

    virtual int EntryRemoved(struct CR_FRAMEBUFFER *pFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {
        entryDataChanged(pFb, hEntry);
        return VINF_SUCCESS;
    }
protected:
    virtual void entryDataChanged(HCR_FRAMEBUFFER hFb, HCR_FRAMEBUFFER_ENTRY hEntry)
    {

    }
};

int CrPMgrInit()
{
    int rc = VINF_SUCCESS;
    memset(&g_CrPresenter, 0, sizeof (g_CrPresenter));
    g_CrPresenter.pFbTexMap = crAllocHashtable();
    if (g_CrPresenter.pFbTexMap)
    {
#ifndef VBOXVDBG_MEMCACHE_DISABLE
        rc = RTMemCacheCreate(&g_CrPresenter.FbEntryLookasideList, sizeof (CR_FRAMEBUFFER_ENTRY),
                                0, /* size_t cbAlignment */
                                UINT32_MAX, /* uint32_t cMaxObjects */
                                NULL, /* PFNMEMCACHECTOR pfnCtor*/
                                NULL, /* PFNMEMCACHEDTOR pfnDtor*/
                                NULL, /* void *pvUser*/
                                0 /* uint32_t fFlags*/
                                );
        if (RT_SUCCESS(rc))
        {
            rc = RTMemCacheCreate(&g_CrPresenter.FbTexLookasideList, sizeof (CR_FBTEX),
                                        0, /* size_t cbAlignment */
                                        UINT32_MAX, /* uint32_t cMaxObjects */
                                        NULL, /* PFNMEMCACHECTOR pfnCtor*/
                                        NULL, /* PFNMEMCACHEDTOR pfnDtor*/
                                        NULL, /* void *pvUser*/
                                        0 /* uint32_t fFlags*/
                                        );
            if (RT_SUCCESS(rc))
            {
                rc = RTMemCacheCreate(&g_CrPresenter.CEntryLookasideList, sizeof (VBOXVR_SCR_COMPOSITOR_ENTRY),
                                            0, /* size_t cbAlignment */
                                            UINT32_MAX, /* uint32_t cMaxObjects */
                                            NULL, /* PFNMEMCACHECTOR pfnCtor*/
                                            NULL, /* PFNMEMCACHEDTOR pfnDtor*/
                                            NULL, /* void *pvUser*/
                                            0 /* uint32_t fFlags*/
                                            );
                if (RT_SUCCESS(rc))
                {
#endif
                    rc = crPMgrModeModifyGlobal(CR_PMGR_MODE_WINDOW, 0);
                    if (RT_SUCCESS(rc))
                        return VINF_SUCCESS;
                    else
                        WARN(("crPMgrModeModifyGlobal failed rc %d", rc));
#ifndef VBOXVDBG_MEMCACHE_DISABLE
                    RTMemCacheDestroy(g_CrPresenter.CEntryLookasideList);
                }
                else
                    WARN(("RTMemCacheCreate failed rc %d", rc));

                RTMemCacheDestroy(g_CrPresenter.FbTexLookasideList);
            }
            else
                WARN(("RTMemCacheCreate failed rc %d", rc));

            RTMemCacheDestroy(g_CrPresenter.FbEntryLookasideList);
        }
        else
            WARN(("RTMemCacheCreate failed rc %d", rc));
#endif
    }
    else
    {
        WARN(("crAllocHashtable failed"));
        rc = VERR_NO_MEMORY;
    }
    return rc;
}

void CrPMgrTerm()
{
    crPMgrModeModifyGlobal(0, CR_PMGR_MODE_ALL);

    HCR_FRAMEBUFFER hFb;

    for (hFb = CrPMgrFbGetFirstInitialized();
            hFb;
            hFb = CrPMgrFbGetNextInitialized(hFb))
    {
        uint32_t idScreen = CrFbGetScreenInfo(hFb)->u32ViewIndex;
        CrFbDisplaySet(hFb, NULL);
        CR_FBDISPLAY_INFO *pInfo = &g_CrPresenter.aDisplayInfos[idScreen];

        if (pInfo->pDpComposite)
            delete pInfo->pDpComposite;

        Assert(!pInfo->pDpWin);
        Assert(!pInfo->pDpWinRootVr);
        Assert(!pInfo->pDpVrdp);

        CrFbTerm(hFb);
    }

#ifndef VBOXVDBG_MEMCACHE_DISABLE
    RTMemCacheDestroy(g_CrPresenter.FbEntryLookasideList);
    RTMemCacheDestroy(g_CrPresenter.FbTexLookasideList);
    RTMemCacheDestroy(g_CrPresenter.CEntryLookasideList);
#endif
    crFreeHashtable(g_CrPresenter.pFbTexMap, NULL);

    if (g_CrPresenter.pvTmpBuf)
        RTMemFree(g_CrPresenter.pvTmpBuf);

    if (g_CrPresenter.pvTmpBuf2)
        RTMemFree(g_CrPresenter.pvTmpBuf2);

    memset(&g_CrPresenter, 0, sizeof (g_CrPresenter));
}

HCR_FRAMEBUFFER CrPMgrFbGet(uint32_t idScreen)
{
    if (idScreen >= CR_MAX_GUEST_MONITORS)
    {
        WARN(("invalid idScreen %d", idScreen));
        return NULL;
    }

    if (!CrFBmIsSet(&g_CrPresenter.FramebufferInitMap, idScreen))
    {
        CrFbInit(&g_CrPresenter.aFramebuffers[idScreen], idScreen);
        CrFBmSetAtomic(&g_CrPresenter.FramebufferInitMap, idScreen);
    }
    else
        Assert(g_CrPresenter.aFramebuffers[idScreen].ScreenInfo.u32ViewIndex == idScreen);

    return &g_CrPresenter.aFramebuffers[idScreen];
}

HCR_FRAMEBUFFER CrPMgrFbGetInitialized(uint32_t idScreen)
{
    if (idScreen >= CR_MAX_GUEST_MONITORS)
    {
        WARN(("invalid idScreen %d", idScreen));
        return NULL;
    }

    if (!CrFBmIsSet(&g_CrPresenter.FramebufferInitMap, idScreen))
    {
        return NULL;
    }
    else
        Assert(g_CrPresenter.aFramebuffers[idScreen].ScreenInfo.u32ViewIndex == idScreen);

    return &g_CrPresenter.aFramebuffers[idScreen];
}

HCR_FRAMEBUFFER CrPMgrFbGetEnabled(uint32_t idScreen)
{
    HCR_FRAMEBUFFER hFb = CrPMgrFbGetInitialized(idScreen);

    if(hFb && CrFbIsEnabled(hFb))
        return hFb;

    return NULL;
}

static HCR_FRAMEBUFFER crPMgrFbGetNextEnabled(uint32_t i)
{
    for (;i < cr_server.screenCount; ++i)
    {
        HCR_FRAMEBUFFER hFb = CrPMgrFbGetEnabled(i);
        if (hFb)
            return hFb;
    }

    return NULL;
}

static HCR_FRAMEBUFFER crPMgrFbGetNextInitialized(uint32_t i)
{
    for (;i < cr_server.screenCount; ++i)
    {
        HCR_FRAMEBUFFER hFb = CrPMgrFbGetInitialized(i);
        if (hFb)
            return hFb;
    }

    return NULL;
}

HCR_FRAMEBUFFER CrPMgrFbGetFirstEnabled()
{
    HCR_FRAMEBUFFER hFb = crPMgrFbGetNextEnabled(0);
//    if (!hFb)
//        WARN(("no enabled framebuffer found"));
    return hFb;
}

HCR_FRAMEBUFFER CrPMgrFbGetNextEnabled(HCR_FRAMEBUFFER hFb)
{
    return crPMgrFbGetNextEnabled(hFb->ScreenInfo.u32ViewIndex+1);
}

HCR_FRAMEBUFFER CrPMgrFbGetFirstInitialized()
{
    HCR_FRAMEBUFFER hFb = crPMgrFbGetNextInitialized(0);
//    if (!hFb)
//        WARN(("no initialized framebuffer found"));
    return hFb;
}

HCR_FRAMEBUFFER CrPMgrFbGetNextInitialized(HCR_FRAMEBUFFER hFb)
{
    return crPMgrFbGetNextInitialized(hFb->ScreenInfo.u32ViewIndex+1);
}

static uint32_t crPMgrModeAdjustVal(uint32_t u32Mode)
{
    u32Mode = CR_PMGR_MODE_ALL & u32Mode;
    if (CR_PMGR_MODE_ROOTVR & u32Mode)
        u32Mode &= ~CR_PMGR_MODE_WINDOW;
    return u32Mode;
}

int CrPMgrScreenChanged(uint32_t idScreen)
{
    if (idScreen >= CR_MAX_GUEST_MONITORS)
    {
        WARN(("invalid idScreen %d", idScreen));
        return VERR_INVALID_PARAMETER;
    }

    CR_FBDISPLAY_INFO *pInfo = &g_CrPresenter.aDisplayInfos[idScreen];
    if (pInfo->pDpWin)
    {
        HCR_FRAMEBUFFER hFb = CrPMgrFbGet(idScreen);
        if (CrFbIsUpdating(hFb))
        {
            WARN(("trying to update viewport while framebuffer is being updated"));
            return VERR_INVALID_STATE;
        }

        int rc = pInfo->pDpWin->UpdateBegin(hFb);
        if (RT_SUCCESS(rc))
        {
            pInfo->pDpWin->reparent(cr_server.screen[idScreen].winID);

            pInfo->pDpWin->UpdateEnd(hFb);
        }
        else
            WARN(("UpdateBegin failed %d", rc));
    }

    return VINF_SUCCESS;
}

int CrPMgrViewportUpdate(uint32_t idScreen)
{
    if (idScreen >= CR_MAX_GUEST_MONITORS)
    {
        WARN(("invalid idScreen %d", idScreen));
        return VERR_INVALID_PARAMETER;
    }

    CR_FBDISPLAY_INFO *pInfo = &g_CrPresenter.aDisplayInfos[idScreen];
    if (pInfo->pDpWin)
    {
        HCR_FRAMEBUFFER hFb = CrPMgrFbGet(idScreen);
        if (CrFbIsUpdating(hFb))
        {
            WARN(("trying to update viewport while framebuffer is being updated"));
            return VERR_INVALID_STATE;
        }

        int rc = pInfo->pDpWin->UpdateBegin(hFb);
        if (RT_SUCCESS(rc))
        {
            pInfo->pDpWin->setViewportRect(&cr_server.screenVieport[idScreen].Rect);
            pInfo->pDpWin->UpdateEnd(hFb);
        }
        else
            WARN(("UpdateBegin failed %d", rc));
    }

    return VINF_SUCCESS;
}

int CrPMgrModeModify(HCR_FRAMEBUFFER hFb, uint32_t u32ModeAdd, uint32_t u32ModeRemove)
{
    uint32_t idScreen = CrFbGetScreenInfo(hFb)->u32ViewIndex;

    CR_FBDISPLAY_INFO *pInfo = &g_CrPresenter.aDisplayInfos[idScreen];
    u32ModeRemove = ((u32ModeRemove | crPMgrModeAdjustVal(u32ModeRemove)) & CR_PMGR_MODE_ALL);
    u32ModeAdd = crPMgrModeAdjustVal(u32ModeAdd);
    u32ModeRemove &= pInfo->u32Mode;
    u32ModeAdd &= ~(u32ModeRemove | pInfo->u32Mode);
    uint32_t u32ModeResulting = ((pInfo->u32Mode | u32ModeAdd) & ~u32ModeRemove);
    uint32_t u32Tmp = crPMgrModeAdjustVal(u32ModeResulting);
    if (u32Tmp != u32ModeResulting)
    {
        u32ModeAdd |= (u32Tmp & ~u32ModeResulting);
        u32ModeRemove |= (~u32Tmp & u32ModeResulting);
        u32ModeResulting = u32Tmp;
        Assert(u32ModeResulting == ((pInfo->u32Mode | u32ModeAdd) & ~u32ModeRemove));
    }
    if (!u32ModeRemove && !u32ModeAdd)
        return VINF_SUCCESS;

    if (!pInfo->pDpComposite)
    {
        pInfo->pDpComposite = new CrFbDisplayComposite();
        pInfo->pDpComposite->setFramebuffer(hFb);
    }

    CrFbWindow * pOldWin = NULL;

    if (u32ModeRemove & CR_PMGR_MODE_ROOTVR)
    {
        CRASSERT(pInfo->pDpWinRootVr);
        CRASSERT(pInfo->pDpWin == pInfo->pDpWinRootVr);
        pInfo->pDpComposite->remove(pInfo->pDpWinRootVr);
        pOldWin = pInfo->pDpWinRootVr->windowDetach();
        CRASSERT(pOldWin);
        delete pInfo->pDpWinRootVr;
        pInfo->pDpWinRootVr = NULL;
        pInfo->pDpWin = NULL;
    }
    else if (u32ModeRemove & CR_PMGR_MODE_WINDOW)
    {
        CRASSERT(!pInfo->pDpWinRootVr);
        CRASSERT(pInfo->pDpWin);
        pInfo->pDpComposite->remove(pInfo->pDpWin);
        pOldWin = pInfo->pDpWin->windowDetach();
        CRASSERT(pOldWin);
        delete pInfo->pDpWin;
        pInfo->pDpWin = NULL;
    }

    if (u32ModeRemove & CR_PMGR_MODE_VRDP)
    {
        CRASSERT(pInfo->pDpVrdp);
        if (pInfo->pDpComposite)
            pInfo->pDpComposite->remove(pInfo->pDpVrdp);
        else
            CrFbDisplaySet(hFb, NULL);

        delete pInfo->pDpVrdp;
        pInfo->pDpVrdp = NULL;
    }

    CrFbDisplayBase *pDpToSet = NULL;

    if (u32ModeAdd & CR_PMGR_MODE_ROOTVR)
    {
        CRASSERT(!pInfo->pDpWin);
        CRASSERT(!pInfo->pDpWinRootVr);

        if (!pOldWin)
            pOldWin = new CrFbWindow(cr_server.screen[idScreen].winID);

        pInfo->pDpWinRootVr = new CrFbDisplayWindowRootVr(pOldWin, &cr_server.screenVieport[idScreen].Rect);
        pOldWin = NULL;
        pInfo->pDpWin = pInfo->pDpWinRootVr;
        pInfo->pDpComposite->add(pInfo->pDpWinRootVr);
    }
    else if (u32ModeAdd & CR_PMGR_MODE_WINDOW)
    {
        CRASSERT(!pInfo->pDpWin);
        CRASSERT(!pInfo->pDpWinRootVr);

        if (!pOldWin)
            pOldWin = new CrFbWindow(cr_server.screen[idScreen].winID);

        pInfo->pDpWin = new CrFbDisplayWindow(pOldWin, &cr_server.screenVieport[idScreen].Rect);
        pOldWin = NULL;
        pInfo->pDpComposite->add(pInfo->pDpWin);
    }

    if (u32ModeAdd & CR_PMGR_MODE_VRDP)
    {
        CRASSERT(!pInfo->pDpVrdp);
        pInfo->pDpVrdp = new CrFbDisplayVrdp();
        pInfo->pDpComposite->add(pInfo->pDpVrdp);
    }

    if (pInfo->pDpComposite->getDisplayCount() > 1)
    {
        ICrFbDisplay* pCur = CrFbDisplayGet(hFb);
        if (pCur != (ICrFbDisplay*)pInfo->pDpComposite)
            CrFbDisplaySet(hFb, pInfo->pDpComposite);
    }
    else
    {
        ICrFbDisplay* pCur = CrFbDisplayGet(hFb);
        ICrFbDisplay* pFirst = pInfo->pDpComposite->first();
        if (pCur != pFirst)
            CrFbDisplaySet(hFb, pFirst);
    }

    if (pOldWin)
        delete pOldWin;

    pInfo->u32Mode = u32ModeResulting;

    return VINF_SUCCESS;
}

static int crPMgrModeModifyGlobal(uint32_t u32ModeAdd, uint32_t u32ModeRemove)
{
    g_CrPresenter.u32DisplayMode = (g_CrPresenter.u32DisplayMode | u32ModeAdd) & ~u32ModeRemove;

    for (HCR_FRAMEBUFFER hFb = CrPMgrFbGetFirstEnabled();
            hFb;
            hFb = CrPMgrFbGetNextEnabled(hFb))
    {
        CrPMgrModeModify(hFb, u32ModeAdd, u32ModeRemove);
    }

    return VINF_SUCCESS;
}

int CrPMgrModeVrdp(bool fEnable)
{
    uint32_t u32ModeAdd, u32ModeRemove;
    if (fEnable)
    {
        u32ModeAdd = CR_PMGR_MODE_VRDP;
        u32ModeRemove = 0;
    }
    else
    {
        u32ModeAdd = 0;
        u32ModeRemove = CR_PMGR_MODE_VRDP;
    }
    return crPMgrModeModifyGlobal(u32ModeAdd, u32ModeRemove);
}

int CrPMgrModeRootVr(bool fEnable)
{
    uint32_t u32ModeAdd, u32ModeRemove;
    if (fEnable)
    {
        u32ModeAdd = CR_PMGR_MODE_ROOTVR;
        u32ModeRemove = CR_PMGR_MODE_WINDOW;
    }
    else
    {
        u32ModeAdd = CR_PMGR_MODE_WINDOW;
        u32ModeRemove = CR_PMGR_MODE_ROOTVR;
    }

    return crPMgrModeModifyGlobal(u32ModeAdd, u32ModeRemove);
}

int CrPMgrModeWinVisible(bool fEnable)
{
    if (!g_CrPresenter.fWindowsForceHidden == !!fEnable)
        return VINF_SUCCESS;

    g_CrPresenter.fWindowsForceHidden = !fEnable;

    for (HCR_FRAMEBUFFER hFb = CrPMgrFbGetFirstEnabled();
            hFb;
            hFb = CrPMgrFbGetNextEnabled(hFb))
    {
        uint32_t idScreen = CrFbGetScreenInfo(hFb)->u32ViewIndex;

        CR_FBDISPLAY_INFO *pInfo = &g_CrPresenter.aDisplayInfos[idScreen];

        if (pInfo->pDpWin)
            pInfo->pDpWin->winVisibilityChanged();
    }

    return VINF_SUCCESS;
}

int CrPMgrRootVrUpdate()
{
    for (HCR_FRAMEBUFFER hFb = CrPMgrFbGetFirstEnabled();
            hFb;
            hFb = CrPMgrFbGetNextEnabled(hFb))
    {
        uint32_t idScreen = CrFbGetScreenInfo(hFb)->u32ViewIndex;
        CR_FBDISPLAY_INFO *pInfo = &g_CrPresenter.aDisplayInfos[idScreen];
        int rc = CrFbUpdateBegin(hFb);
        if (RT_SUCCESS(rc))
        {
            pInfo->pDpWinRootVr->RegionsChanged(hFb);
            CrFbUpdateEnd(hFb);
        }
        else
            WARN(("CrFbUpdateBegin failed %d", rc));
    }

    return VINF_SUCCESS;
}

/*helper function that calls CrFbUpdateBegin for all enabled framebuffers */
int CrPMgrHlpGlblUpdateBegin(CR_FBMAP *pMap)
{
    CrFBmInit(pMap);
    for (HCR_FRAMEBUFFER hFb = CrPMgrFbGetFirstEnabled();
            hFb;
            hFb = CrPMgrFbGetNextEnabled(hFb))
    {
        int rc = CrFbUpdateBegin(hFb);
        if (!RT_SUCCESS(rc))
        {
            WARN(("UpdateBegin failed, rc %d", rc));
            for (HCR_FRAMEBUFFER hTmpFb = CrPMgrFbGetFirstEnabled();
                        hFb != hTmpFb;
                        hTmpFb = CrPMgrFbGetNextEnabled(hTmpFb))
            {
                CrFbUpdateEnd(hTmpFb);
                CrFBmClear(pMap, CrFbGetScreenInfo(hFb)->u32ViewIndex);
            }
            return rc;
        }

        CrFBmSet(pMap, CrFbGetScreenInfo(hFb)->u32ViewIndex);
    }

    return VINF_SUCCESS;
}

/*helper function that calls CrFbUpdateEnd for all framebuffers being updated */
void CrPMgrHlpGlblUpdateEnd(CR_FBMAP *pMap)
{
    for (uint32_t i = 0; i < cr_server.screenCount; ++i)
    {
        if (!CrFBmIsSet(pMap, i))
            continue;

        HCR_FRAMEBUFFER hFb = CrPMgrFbGetInitialized(i);
        CRASSERT(hFb);
        CrFbUpdateEnd(hFb);
    }
}

/*client should notify the manager about the framebuffer resize via this function */
int CrPMgrNotifyResize(HCR_FRAMEBUFFER hFb)
{
    int rc = VINF_SUCCESS;
    if (CrFbIsEnabled(hFb))
    {
        rc = CrPMgrModeModify(hFb, g_CrPresenter.u32DisplayMode, 0);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrPMgrModeModify failed rc %d", rc));
            return rc;
        }
    }
    else
    {
        rc = CrPMgrModeModify(hFb, 0, CR_PMGR_MODE_ALL);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrPMgrModeModify failed rc %d", rc));
            return rc;
        }
    }

    return VINF_SUCCESS;
}

int CrFbEntrySaveState(CR_FRAMEBUFFER *pFb, CR_FRAMEBUFFER_ENTRY *hEntry, PSSMHANDLE pSSM)
{
    const struct VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry = CrFbEntryGetCompositorEntry(hEntry);
    CR_TEXDATA *pTexData = CrVrScrCompositorEntryTexGet(pEntry);
    CR_FBTEX *pFbTex = PCR_FBTEX_FROM_TEX(pTexData);
    int rc = SSMR3PutU32(pSSM, pFbTex->pTobj->id);
    AssertRCReturn(rc, rc);
    uint32_t u32 = 0;

    u32 = CrVrScrCompositorEntryFlagsGet(pEntry);
    rc = SSMR3PutU32(pSSM, u32);
    AssertRCReturn(rc, rc);

    const RTRECT *pRect = CrVrScrCompositorEntryRectGet(pEntry);

    rc = SSMR3PutS32(pSSM, pRect->xLeft);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutS32(pSSM, pRect->yTop);
    AssertRCReturn(rc, rc);
#if 0
    rc = SSMR3PutS32(pSSM, pRect->xRight);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutS32(pSSM, pRect->yBottom);
    AssertRCReturn(rc, rc);
#endif

    rc = CrVrScrCompositorEntryRegionsGet(&pFb->Compositor, pEntry, &u32, NULL, NULL, &pRect);
    AssertRCReturn(rc, rc);

    rc = SSMR3PutU32(pSSM, u32);
    AssertRCReturn(rc, rc);

    if (u32)
    {
        rc = SSMR3PutMem(pSSM, pRect, u32 * sizeof (*pRect));
        AssertRCReturn(rc, rc);
    }
    return rc;
}

int CrFbSaveState(CR_FRAMEBUFFER *pFb, PSSMHANDLE pSSM)
{
    VBOXVR_SCR_COMPOSITOR_CONST_ITERATOR Iter;
    CrVrScrCompositorConstIterInit(&pFb->Compositor, &Iter);
    const VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry;
    uint32_t u32 = 0;
    while ((pEntry = CrVrScrCompositorConstIterNext(&Iter)) != NULL)
    {
        CR_TEXDATA *pTexData = CrVrScrCompositorEntryTexGet(pEntry);
        CRASSERT(pTexData);
        CR_FBTEX *pFbTex = PCR_FBTEX_FROM_TEX(pTexData);
        if (pFbTex->pTobj)
            ++u32;
    }

    int rc = SSMR3PutU32(pSSM, u32);
    AssertRCReturn(rc, rc);

    CrVrScrCompositorConstIterInit(&pFb->Compositor, &Iter);

    while ((pEntry = CrVrScrCompositorConstIterNext(&Iter)) != NULL)
    {
        CR_TEXDATA *pTexData = CrVrScrCompositorEntryTexGet(pEntry);
        CR_FBTEX *pFbTex = PCR_FBTEX_FROM_TEX(pTexData);
        if (pFbTex->pTobj)
        {
            HCR_FRAMEBUFFER_ENTRY hEntry = CrFbEntryFromCompositorEntry(pEntry);
            rc = CrFbEntrySaveState(pFb, hEntry, pSSM);
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}

int CrPMgrSaveState(PSSMHANDLE pSSM)
{
    int rc;
    int cDisplays = 0, i;
    for (i = 0; i < cr_server.screenCount; ++i)
    {
        if (CrPMgrFbGetEnabled(i))
            ++cDisplays;
    }

    rc = SSMR3PutS32(pSSM, cDisplays);
    AssertRCReturn(rc, rc);

    if (!cDisplays)
        return VINF_SUCCESS;

    rc = SSMR3PutS32(pSSM, cr_server.screenCount);
    AssertRCReturn(rc, rc);

    for (i = 0; i < cr_server.screenCount; ++i)
    {
        CR_FRAMEBUFFER *hFb = CrPMgrFbGetEnabled(i);
        if (hFb)
        {
            Assert(hFb->ScreenInfo.u32ViewIndex == i);
            rc = SSMR3PutU32(pSSM, hFb->ScreenInfo.u32ViewIndex);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutS32(pSSM, hFb->ScreenInfo.i32OriginX);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutS32(pSSM, hFb->ScreenInfo.i32OriginY);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutU32(pSSM, hFb->ScreenInfo.u32StartOffset);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutU32(pSSM, hFb->ScreenInfo.u32LineSize);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutU32(pSSM, hFb->ScreenInfo.u32Width);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutU32(pSSM, hFb->ScreenInfo.u32Height);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutU16(pSSM, hFb->ScreenInfo.u16BitsPerPixel);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutU16(pSSM, hFb->ScreenInfo.u16Flags);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutU32(pSSM, (uint32_t)(((uintptr_t)hFb->pvVram) - ((uintptr_t)g_pvVRamBase)));
            AssertRCReturn(rc, rc);

            rc = CrFbSaveState(hFb, pSSM);
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}

int CrFbEntryLoadState(CR_FRAMEBUFFER *pFb, PSSMHANDLE pSSM, uint32_t version)
{
    uint32_t texture;
    int  rc = SSMR3GetU32(pSSM, &texture);
    AssertRCReturn(rc, rc);

    uint32_t fFlags;
    rc = SSMR3GetU32(pSSM, &fFlags);
    AssertRCReturn(rc, rc);


    HCR_FRAMEBUFFER_ENTRY hEntry;

    rc = CrFbEntryCreateForTexId(pFb, texture, fFlags, &hEntry);
    if (!RT_SUCCESS(rc))
    {
        WARN(("CrFbEntryCreateForTexId Failed"));
        return rc;
    }

    Assert(hEntry);

    const struct VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry = CrFbEntryGetCompositorEntry(hEntry);
    CR_TEXDATA *pTexData = CrVrScrCompositorEntryTexGet(pEntry);
    CR_FBTEX *pFbTex = PCR_FBTEX_FROM_TEX(pTexData);

    RTPOINT Point;
    rc = SSMR3GetS32(pSSM, &Point.x);
    AssertRCReturn(rc, rc);

    rc = SSMR3GetS32(pSSM, &Point.y);
    AssertRCReturn(rc, rc);

    uint32_t cRects;
    rc = SSMR3GetU32(pSSM, &cRects);
    AssertRCReturn(rc, rc);

    RTRECT * pRects = NULL;
    if (cRects)
    {
        pRects = (RTRECT *)crAlloc(cRects * sizeof (*pRects));
        AssertReturn(pRects, VERR_NO_MEMORY);

        rc = SSMR3GetMem(pSSM, pRects, cRects * sizeof (*pRects));
        AssertRCReturn(rc, rc);
    }

    rc = CrFbEntryRegionsSet(pFb, hEntry, &Point, cRects, pRects, false);
    AssertRCReturn(rc, rc);

    if (pRects)
        crFree(pRects);

    CrFbEntryRelease(pFb, hEntry);

    return VINF_SUCCESS;
}

int CrFbLoadState(CR_FRAMEBUFFER *pFb, PSSMHANDLE pSSM, uint32_t version)
{
    uint32_t u32 = 0;
    int rc = SSMR3GetU32(pSSM, &u32);
    AssertRCReturn(rc, rc);

    if (!u32)
        return VINF_SUCCESS;

    rc = CrFbUpdateBegin(pFb);
    AssertRCReturn(rc, rc);

    for (uint32_t i = 0; i < u32; ++i)
    {
        rc = CrFbEntryLoadState(pFb, pSSM, version);
        AssertRCReturn(rc, rc);

    }

    CrFbUpdateEnd(pFb);

    return VINF_SUCCESS;
}

int CrPMgrLoadState(PSSMHANDLE pSSM, uint32_t version)
{
    int rc;
    int cDisplays, screenCount, i;

    rc = SSMR3GetS32(pSSM, &cDisplays);
    AssertRCReturn(rc, rc);

    if (!cDisplays)
        return VINF_SUCCESS;

    rc = SSMR3GetS32(pSSM, &screenCount);
    AssertRCReturn(rc, rc);

    CRASSERT(screenCount == cr_server.screenCount);

    CRScreenInfo screen[CR_MAX_GUEST_MONITORS];

    if (version < SHCROGL_SSM_VERSION_WITH_FB_INFO)
    {
        for (i = 0; i < cr_server.screenCount; ++i)
        {
            rc = SSMR3GetS32(pSSM, &screen[i].x);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetS32(pSSM, &screen[i].y);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &screen[i].w);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &screen[i].h);
            AssertRCReturn(rc, rc);
        }
    }

    for (i = 0; i < cDisplays; ++i)
    {
        int iScreen;

        rc = SSMR3GetS32(pSSM, &iScreen);
        AssertRCReturn(rc, rc);

        CR_FRAMEBUFFER *pFb = CrPMgrFbGet(iScreen);
        Assert(pFb);

        rc = CrFbUpdateBegin(pFb);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrFbUpdateBegin failed %d", rc));
            return rc;
        }

        VBVAINFOSCREEN Screen;
        void *pvVRAM;

        Screen.u32ViewIndex = iScreen;

        if (version < SHCROGL_SSM_VERSION_WITH_FB_INFO)
        {
            memset(&Screen, 0, sizeof (Screen));
            Screen.u32LineSize = 4 * screen[iScreen].w;
            Screen.u32Width = screen[iScreen].w;
            Screen.u32Height = screen[iScreen].h;
            Screen.u16BitsPerPixel = 4;
            Screen.u16Flags = VBVA_SCREEN_F_ACTIVE;

            pvVRAM = g_pvVRamBase;
        }
        else
        {
            rc = SSMR3GetS32(pSSM, &Screen.i32OriginX);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetS32(pSSM, &Screen.i32OriginY);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &Screen.u32StartOffset);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &Screen.u32LineSize);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &Screen.u32Width);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &Screen.u32Height);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU16(pSSM, &Screen.u16BitsPerPixel);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU16(pSSM, &Screen.u16Flags);
            AssertRCReturn(rc, rc);

            uint32_t offVram = 0;
            rc = SSMR3GetU32(pSSM, &offVram);
            AssertRCReturn(rc, rc);

            pvVRAM = (void*)(((uintptr_t)g_pvVRamBase) + offVram);
        }

        crVBoxServerMuralFbResizeBegin(pFb);

        rc = CrFbResize(pFb, &Screen, pvVRAM);
        if (!RT_SUCCESS(rc))
        {
            WARN(("CrFbResize failed %d", rc));
            return rc;
        }

        rc = CrFbLoadState(pFb, pSSM, version);
        AssertRCReturn(rc, rc);

        crVBoxServerMuralFbResizeEnd(pFb);

        CrFbUpdateEnd(pFb);

        CrPMgrNotifyResize(pFb);
    }

    return VINF_SUCCESS;
}


void SERVER_DISPATCH_APIENTRY
crServerDispatchVBoxTexPresent(GLuint texture, GLuint cfg, GLint xPos, GLint yPos, GLint cRects, const GLint *pRects)
{
    uint32_t idScreen = CR_PRESENT_GET_SCREEN(cfg);
    if (idScreen >= CR_MAX_GUEST_MONITORS)
    {
        WARN(("Invalid guest screen"));
        return;
    }

    HCR_FRAMEBUFFER hFb = CrPMgrFbGetEnabled(idScreen);
    if (!hFb)
    {
        WARN(("request to present on disabled framebuffer, ignore"));
        return;
    }

    HCR_FRAMEBUFFER_ENTRY hEntry;
    int rc;
    if (texture)
    {
        rc = CrFbEntryCreateForTexId(hFb, texture, (cfg & CR_PRESENT_FLAG_TEX_NONINVERT_YCOORD) ? 0 : CRBLT_F_INVERT_SRC_YCOORDS, &hEntry);
        if (!RT_SUCCESS(rc))
        {
            LOG(("CrFbEntryCreateForTexId Failed"));
            return;
        }

        Assert(hEntry);

#if 0
        if (!(cfg & CR_PRESENT_FLAG_CLEAR_RECTS))
        {
            CR_SERVER_DUMP_TEXPRESENT(&pEntry->CEntry.Tex);
        }
#endif
    }
    else
        hEntry = NULL;

    rc = CrFbUpdateBegin(hFb);
    if (RT_SUCCESS(rc))
    {
        if (!(cfg & CR_PRESENT_FLAG_CLEAR_RECTS))
        {
            RTPOINT Point = {xPos, yPos};
            rc = CrFbEntryRegionsAdd(hFb, hEntry, &Point, (uint32_t)cRects, (const RTRECT*)pRects, false);
        }
        else
        {
            CrFbRegionsClear(hFb);
        }

        CrFbUpdateEnd(hFb);
    }
    else
    {
        WARN(("CrFbUpdateBegin Failed"));
    }

    if (hEntry)
        CrFbEntryRelease(hFb, hEntry);
}

DECLINLINE(void) crVBoxPRectUnpack(const VBOXCMDVBVA_RECT *pVbvaRect, RTRECT *pRect)
{
    pRect->xLeft = pVbvaRect->xLeft;
    pRect->yTop = pVbvaRect->yTop;
    pRect->xRight = pVbvaRect->xRight;
    pRect->yBottom = pVbvaRect->yBottom;
}

DECLINLINE(void) crVBoxPRectUnpacks(const VBOXCMDVBVA_RECT *paVbvaRects, RTRECT *paRects, uint32_t cRects)
{
    uint32_t i = 0;
    for (; i < cRects; ++i)
    {
        crVBoxPRectUnpack(&paVbvaRects[i], &paRects[i]);
    }
}

int32_t crVBoxServerCrCmdBltProcess(PVBOXCMDVBVA_HDR pCmd, uint32_t cbCmd)
{
    uint8_t u8Flags = pCmd->u8Flags;
    if (u8Flags & (VBOXCMDVBVA_OPF_ALLOC_DSTPRIMARY | VBOXCMDVBVA_OPF_ALLOC_SRCPRIMARY))
    {
        VBOXCMDVBVA_BLT_PRIMARY *pBlt = (VBOXCMDVBVA_BLT_PRIMARY*)pCmd;
        uint8_t u8PrimaryID = pBlt->Hdr.u8PrimaryID;
        HCR_FRAMEBUFFER hFb = CrPMgrFbGetEnabled(u8PrimaryID);
        if (!hFb)
        {
            WARN(("request to present on disabled framebuffer, ignore"));
            pCmd->i8Result = -1;
            return VINF_SUCCESS;
        }

        const VBOXCMDVBVA_RECT *pPRects = pBlt->aRects;
        uint32_t cRects = (cbCmd - RT_OFFSETOF(VBOXCMDVBVA_BLT_PRIMARY, aRects)) / sizeof (VBOXCMDVBVA_RECT);
        RTRECT *pRects;
        if (g_CrPresenter.cbTmpBuf < cRects * sizeof (RTRECT))
        {
            if (g_CrPresenter.pvTmpBuf)
                RTMemFree(g_CrPresenter.pvTmpBuf);

            g_CrPresenter.cbTmpBuf = (cRects + 10) * sizeof (RTRECT);
            g_CrPresenter.pvTmpBuf = RTMemAlloc(g_CrPresenter.cbTmpBuf);
            if (!g_CrPresenter.pvTmpBuf)
            {
                WARN(("RTMemAlloc failed!"));
                g_CrPresenter.cbTmpBuf = 0;
                pCmd->i8Result = -1;
                return VINF_SUCCESS;
            }
        }

        pRects = (RTRECT *)g_CrPresenter.pvTmpBuf;

        crVBoxPRectUnpacks(pPRects, pRects, cRects);

        Assert(!((cbCmd - RT_OFFSETOF(VBOXCMDVBVA_BLT_PRIMARY, aRects)) % sizeof (VBOXCMDVBVA_RECT)));

        if (u8Flags & VBOXCMDVBVA_OPF_ALLOC_DSTPRIMARY)
        {
            if (!(u8Flags & VBOXCMDVBVA_OPF_ALLOC_SRCPRIMARY))
            {
                /* blit to primary from non-primary */
                uint32_t texId;
                if (u8Flags & VBOXCMDVBVA_OPF_ALLOC_DSTID)
                {
                    /* TexPresent */
                    texId = pBlt->alloc.id;
                }
                else
                {
                    VBOXCMDVBVAOFFSET offVRAM = pBlt->alloc.offVRAM;
                    const VBVAINFOSCREEN *pScreen = CrFbGetScreenInfo(hFb);
                    uint32_t cbScreen = pScreen->u32LineSize * pScreen->u32Height;
                    if (offVRAM >= g_cbVRam
                            || offVRAM + cbScreen >= g_cbVRam)
                    {
                        WARN(("invalid param"));
                        pCmd->i8Result = -1;
                        return VINF_SUCCESS;
                    }

                    uint8_t *pu8Buf = g_pvVRamBase + offVRAM;
                    texId = 0;
                    /*todo: notify VGA device to perform updates */
                }

                crServerDispatchVBoxTexPresent(texId, u8PrimaryID, pBlt->Pos.x, pBlt->Pos.y, cRects, (const GLint*)pRects);
            }
            else
            {
                /* blit from one primary to another primary, wow */
                WARN(("not implemented"));
                pCmd->i8Result = -1;
                return VINF_SUCCESS;
            }
        }
        else
        {
            Assert(u8Flags & VBOXCMDVBVA_OPF_ALLOC_SRCPRIMARY);
            /* blit from primary to non-primary */
            if (u8Flags & VBOXCMDVBVA_OPF_ALLOC_DSTID)
            {
                uint32_t texId = pBlt->alloc.id;
                WARN(("not implemented"));
                pCmd->i8Result = -1;
                return VINF_SUCCESS;
            }
            else
            {
                VBOXCMDVBVAOFFSET offVRAM = pBlt->alloc.offVRAM;
                const VBVAINFOSCREEN *pScreen = CrFbGetScreenInfo(hFb);
                uint32_t cbScreen = pScreen->u32LineSize * pScreen->u32Height;
                if (offVRAM >= g_cbVRam
                        || offVRAM + cbScreen >= g_cbVRam)
                {
                    WARN(("invalid param"));
                    pCmd->i8Result = -1;
                    return VINF_SUCCESS;
                }

                uint8_t *pu8Buf = g_pvVRamBase + offVRAM;

                RTRECT Rect;
                Rect.xLeft = pBlt->Pos.x;
                Rect.yTop = pBlt->Pos.y;
                Rect.xRight = Rect.xLeft + pScreen->u32Width;
                Rect.yBottom = Rect.yTop + pScreen->u32Height;
                CR_BLITTER_IMG Img;
                crFbImgFromScreenVram(pScreen, pu8Buf, &Img);
                int rc = CrFbBltGetContents(hFb, &Rect, cRects, pRects, &Img);
                if (!RT_SUCCESS(rc))
                {
                    WARN(("CrFbBltGetContents failed %d", rc));
                    pCmd->i8Result = -1;
                    return VINF_SUCCESS;
                }
            }
        }
    }
    else
    {
        WARN(("not implemented"));
        pCmd->i8Result = -1;
        return VINF_SUCCESS;
    }

    pCmd->i8Result = 0;
    return VINF_SUCCESS;
}
