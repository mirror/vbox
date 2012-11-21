/* $Id$ */

/** @file
 * Presenter API
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/asm.h>
#include <iprt/mem.h>

#define CR_DISPLAY_RECTS_UNDEFINED UINT32_MAX


#define CR_PRESENTER_ENTRY_FROM_ENTRY(_p) ((PCR_PRESENTER_ENTRY)(((uint8_t*)(_p)) - RT_OFFSETOF(CR_PRESENTER_ENTRY, Ce)))
#define CR_PRESENTER_FROM_COMPOSITOR(_p) ((PCR_PRESENTER)(((uint8_t*)(_p)) - RT_OFFSETOF(CR_PRESENTER, Compositor)))


static int crPtRectsAssignBuffer(PCR_PRESENTER pPresenter, uint32_t cRects)
{
    Assert(cRects);

    if (pPresenter->cRectsBuffer >= cRects)
    {
        pPresenter->cRects = cRects;
        return VINF_SUCCESS;
    }

    if (pPresenter->cRectsBuffer)
    {
        Assert(pPresenter->paSrcRects);
        RTMemFree(pPresenter->paSrcRects);
        Assert(pPresenter->paDstRects);
        RTMemFree(pPresenter->paDstRects);
    }

    pPresenter->paSrcRects = (PRTRECT)RTMemAlloc(sizeof (*pPresenter->paSrcRects) * cRects);
    if (pPresenter->paSrcRects)
    {
        pPresenter->paDstRects = (PRTRECT)RTMemAlloc(sizeof (*pPresenter->paDstRects) * cRects);
        if (pPresenter->paDstRects)
        {
            pPresenter->cRects = cRects;
            pPresenter->cRectsBuffer = cRects;
            return VINF_SUCCESS;
        }
        else
        {
            crWarning("RTMemAlloc failed!");
            RTMemFree(pPresenter->paSrcRects);
            pPresenter->paSrcRects = NULL;
        }
    }
    else
    {
        crWarning("RTMemAlloc failed!");
        pPresenter->paDstRects = NULL;
    }

    pPresenter->cRects = CR_DISPLAY_RECTS_UNDEFINED;
    pPresenter->cRectsBuffer = 0;

    return VERR_NO_MEMORY;
}

static void crPtRectsInvalidate(PCR_PRESENTER pPresenter)
{
    pPresenter->cRects = CR_DISPLAY_RECTS_UNDEFINED;
}

static DECLCALLBACK(bool) crPtRectsCounterCb(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pEntry, void *pvVisitor)
{
    uint32_t* pCounter = (uint32_t*)pvVisitor;
    Assert(VBoxVrListRectsCount(&pEntry->Vr));
    *pCounter += VBoxVrListRectsCount(&pEntry->Vr);
    return true;
}

typedef struct CR_PRESENTOR_RECTS_ASSIGNER
{
    PRTRECT paSrcRects;
    PRTRECT paDstRects;
    uint32_t cRects;
} CR_PRESENTOR_RECTS_ASSIGNER, *PCR_PRESENTOR_RECTS_ASSIGNER;

static DECLCALLBACK(bool) crPtRectsAssignerCb(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pCEntry, void *pvVisitor)
{
    PCR_PRESENTOR_RECTS_ASSIGNER pData = (PCR_PRESENTOR_RECTS_ASSIGNER)pvVisitor;
    PCR_PRESENTER pPresenter = CR_PRESENTER_FROM_COMPOSITOR(pCompositor);
    PCR_PRESENTER_ENTRY pEntry = CR_PRESENTER_ENTRY_FROM_ENTRY(pCEntry);
    pEntry->paSrcRects = pData->paSrcRects;
    pEntry->paDstRects = pData->paDstRects;
    uint32_t cRects = VBoxVrListRectsCount(&pCEntry->Vr);
    Assert(cRects);
    Assert(cRects >= pData->cRects);
    int rc = VBoxVrListRectsGet(&pCEntry->Vr, cRects, pEntry->paDstRects);
    AssertRC(rc);
    if (pPresenter->StretchX >= 1. && pPresenter->StretchY >= 1. /* <- stretching can not zero some rects */
            && !pEntry->Pos.x && !pEntry->Pos.y)
    {
        memcpy(pEntry->paSrcRects, pEntry->paDstRects, cRects * sizeof (*pEntry->paSrcRects));
    }
    else
    {
        for (uint32_t i = 0; i < cRects; ++i)
        {
            pEntry->paSrcRects[i].xLeft = (int32_t)((pEntry->paDstRects[i].xLeft - pEntry->Pos.x) * pPresenter->StretchX);
            pEntry->paSrcRects[i].yTop = (int32_t)((pEntry->paDstRects[i].yTop - pEntry->Pos.y) * pPresenter->StretchY);
            pEntry->paSrcRects[i].xRight = (int32_t)((pEntry->paDstRects[i].xRight - pEntry->Pos.x) * pPresenter->StretchX);
            pEntry->paSrcRects[i].yBottom = (int32_t)((pEntry->paDstRects[i].yBottom - pEntry->Pos.y) * pPresenter->StretchY);
        }

        bool canZeroX = (pPresenter->StretchX < 1);
        bool canZeroY = (pPresenter->StretchY < 1);
        if (canZeroX && canZeroY)
        {
            /* filter out zero rectangles*/
            uint32_t iOrig, iNew;
            for (iOrig = 0, iNew = 0; iOrig < cRects; ++iOrig)
            {
                PRTRECT pOrigRect = &pEntry->paSrcRects[iOrig];
                if (pOrigRect->xLeft == pOrigRect->xRight
                        || pOrigRect->yTop == pOrigRect->yBottom)
                    continue;

                if (iNew != iOrig)
                {
                    PRTRECT pNewRect = &pEntry->paSrcRects[iNew];
                    *pNewRect = *pOrigRect;
                }

                ++iNew;
            }

            Assert(iNew <= iOrig);

            uint32_t cDiff = iOrig - iNew;

            if (cDiff)
            {
                pPresenter->cRects -= cDiff;
                cRects -= cDiff;
            }
        }
    }

    pEntry->cRects = cRects;
    pData->paDstRects += cRects;
    pData->paSrcRects += cRects;
    pData->cRects -= cRects;
    return true;
}

static int crPtRectsCheckInit(PCR_PRESENTER pPresenter)
{
    if (pPresenter->cRects != CR_DISPLAY_RECTS_UNDEFINED)
        return VINF_SUCCESS;

    uint32_t cRects = 0;
    VBoxVrCompositorVisit(&pPresenter->Compositor, crPtRectsCounterCb, &cRects);

    if (!cRects)
    {
        pPresenter->cRects = 0;
        return VINF_SUCCESS;
    }

    int rc = crPtRectsAssignBuffer(pPresenter, cRects);
    if (!RT_SUCCESS(rc))
        return rc;

    CR_PRESENTOR_RECTS_ASSIGNER AssignerData;
    AssignerData.paSrcRects = pPresenter->paSrcRects;
    AssignerData.paDstRects = pPresenter->paDstRects;
    AssignerData.cRects = pPresenter->cRects;
    VBoxVrCompositorVisit(&pPresenter->Compositor, crPtRectsAssignerCb, &AssignerData);
    Assert(!AssignerData.cRects);
    return VINF_SUCCESS;
}

DECLCALLBACK(int) CrPtCbDrawEntrySingle(struct CR_PRESENTER *pPresenter, struct CR_PRESENTER_ENTRY *pEntry, PCR_BLITTER pBlitter, bool *pfAllEntriesDrawn)
{
    if (pfAllEntriesDrawn)
        *pfAllEntriesDrawn = false;

    int rc = crPtRectsCheckInit(pPresenter);
    if (!RT_SUCCESS(rc))
    {
        crWarning("crPtRectsCheckInit failed, rc %d", rc);
        return rc;
    }

    Assert(VBoxVrListRectsCount(&pEntry->Ce.Vr));

    if (!pEntry->cRects)
        return VINF_SUCCESS;

    rc = pPresenter->pfnDrawTexture(pPresenter, pBlitter, &pEntry->Texture, pEntry->paSrcRects, pEntry->paDstRects, pEntry->cRects);
    if (!RT_SUCCESS(rc))
    {
        crWarning("pfnDrawTexture failed, rc %d", rc);
        return rc;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(bool) crPtDrawEntryAllCb(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pCEntry, void *pvVisitor)
{
    struct CR_PRESENTER *pPresenter = CR_PRESENTER_FROM_COMPOSITOR(pCompositor);
    struct CR_PRESENTER_ENTRY *pEntry = CR_PRESENTER_ENTRY_FROM_ENTRY(pCEntry);
    PCR_BLITTER pBlitter = (PCR_BLITTER)pvVisitor;
    bool fAllEntriesDrawn;
    int rc = CrPtCbDrawEntrySingle(pPresenter, pEntry, pBlitter, &fAllEntriesDrawn);
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrPtCbDrawEntrySingle failed, rc %d", rc);
    }
    return !fAllEntriesDrawn;
}

DECLCALLBACK(int) CrPtCbDrawEntryAll(struct CR_PRESENTER *pPresenter, struct CR_PRESENTER_ENTRY *pEntry, PCR_BLITTER pBlitter, bool *pfAllEntriesDrawn)
{
    int rc = crPtRectsCheckInit(pPresenter);
    if (!RT_SUCCESS(rc))
    {
        crWarning("crPtRectsCheckInit failed, rc %d", rc);
        return rc;
    }

    VBoxVrCompositorVisit(&pPresenter->Compositor, crPtDrawEntryAllCb, pBlitter);

    if (pfAllEntriesDrawn)
        *pfAllEntriesDrawn = true;

    return VINF_SUCCESS;
}

static int crPtEntryRegionsAdd(PCR_PRESENTER pPresenter, PCR_PRESENTER_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged)
{
    uint32_t fChangedFlags = 0;
    int rc = VBoxVrCompositorEntryRegionsAdd(&pPresenter->Compositor, &pEntry->Ce, cRegions, paRegions, &fChangedFlags);
    if (!RT_SUCCESS(rc))
    {
        crWarning("VBoxVrCompositorEntryRegionsAdd failed, rc %d", rc);
        return rc;
    }

    if (fChangedFlags & VBOXVR_COMPOSITOR_CF_COMPOSITED_REGIONS_CHANGED)
    {
        crPtRectsInvalidate(pPresenter);
        rc = pPresenter->pfnRegionsChanged(pPresenter);
        if (!RT_SUCCESS(rc))
        {
            crWarning("pfnRegionsChanged failed, rc %d", rc);
            return rc;
        }
    }

    if (pfChanged)
        *pfChanged = !!fChangedFlags;
    return VINF_SUCCESS;
}

static int crPtEntryRegionsSet(PCR_PRESENTER pPresenter, PCR_PRESENTER_ENTRY pEntry, uint32_t cRegions, const RTRECT *paRegions, bool *pfChanged)
{
    bool fChanged;
    int rc = VBoxVrCompositorEntryRegionsSet(&pPresenter->Compositor, &pEntry->Ce, cRegions, paRegions, &fChanged);
    if (!RT_SUCCESS(rc))
    {
        crWarning("VBoxVrCompositorEntryRegionsSet failed, rc %d", rc);
        return rc;
    }

    if (fChanged)
    {
        crPtRectsInvalidate(pPresenter);
        rc = pPresenter->pfnRegionsChanged(pPresenter);
        if (!RT_SUCCESS(rc))
        {
            crWarning("pfnRegionsChanged failed, rc %d", rc);
            return rc;
        }
    }

    if (pfChanged)
        *pfChanged = fChanged;
    return VINF_SUCCESS;
}

static void crPtEntryPositionSet(PCR_PRESENTER pPresenter, PCR_PRESENTER_ENTRY pEntry, const RTPOINT *pPos)
{
    if (pEntry && (pEntry->Pos.x != pPos->x || pEntry->Pos.y != pPos->y))
    {
        if (VBoxVrCompositorEntryIsInList(&pEntry->Ce))
        {
            VBoxVrCompositorEntryRemove(&pPresenter->Compositor, &pEntry->Ce);
            crPtRectsInvalidate(pPresenter);
        }
        pEntry->Pos = *pPos;
    }
}

int CrPtPresentEntry(PCR_PRESENTER pPresenter, PCR_PRESENTER_ENTRY pEntry, PCR_BLITTER pBlitter)
{
    int rc = CrBltEnter(pBlitter, cr_server.currentCtxInfo, cr_server.currentMural);
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrBltEnter failed, rc %d", rc);
        return rc;
    }

    rc = pPresenter->pfnDrawEntry(pPresenter, pEntry, pBlitter, NULL);

    CrBltLeave(pBlitter);

    if (!RT_SUCCESS(rc))
    {
        crWarning("pfnDraw failed, rc %d", rc);
        return rc;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(bool) crPtPresentCb(PVBOXVR_COMPOSITOR pCompositor, PVBOXVR_COMPOSITOR_ENTRY pCEntry, void *pvVisitor)
{
    struct CR_PRESENTER *pPresenter = CR_PRESENTER_FROM_COMPOSITOR(pCompositor);
    struct CR_PRESENTER_ENTRY *pEntry = CR_PRESENTER_ENTRY_FROM_ENTRY(pCEntry);
    PCR_BLITTER pBlitter = (PCR_BLITTER)pvVisitor;
    bool fAllDrawn = false;
    int rc = pPresenter->pfnDrawEntry(pPresenter, pEntry, pBlitter, &fAllDrawn);
    if (!RT_SUCCESS(rc))
    {
        crWarning("pfnDrawEntry failed, rc %d", rc);
    }
    return !fAllDrawn;
}

int CrPtPresent(PCR_PRESENTER pPresenter, PCR_BLITTER pBlitter)
{
    int rc = CrBltEnter(pBlitter, cr_server.currentCtxInfo, cr_server.currentMural);
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrBltEnter failed, rc %d", rc);
        return rc;
    }

    VBoxVrCompositorVisit(&pPresenter->Compositor, crPtPresentCb, pBlitter);

    CrBltLeave(pBlitter);

    if (!RT_SUCCESS(rc))
    {
        crWarning("pfnDraw failed, rc %d", rc);
        return rc;
    }

    return VINF_SUCCESS;
}

int CrPtEntryRegionsAdd(PCR_PRESENTER pPresenter, PCR_PRESENTER_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions)
{
    crPtEntryPositionSet(pPresenter, pEntry, pPos);

    int rc = crPtEntryRegionsAdd(pPresenter, pEntry, cRegions, paRegions, NULL);
    if (!RT_SUCCESS(rc))
    {
        crWarning("crPtEntryRegionsAdd failed, rc %d", rc);
        return rc;
    }

    return VINF_SUCCESS;
}

int CrPtEntryRegionsSet(PCR_PRESENTER pPresenter, PCR_PRESENTER_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions)
{
    crPtEntryPositionSet(pPresenter, pEntry, pPos);

    int rc = crPtEntryRegionsSet(pPresenter, pEntry, cRegions, paRegions, NULL);
    if (!RT_SUCCESS(rc))
    {
        crWarning("crPtEntryRegionsAdd failed, rc %d", rc);
        return rc;
    }

    return VINF_SUCCESS;
}

int CrPtEntryRemove(PCR_PRESENTER pPresenter, PCR_PRESENTER_ENTRY pEntry)
{
    if (!VBoxVrCompositorEntryRemove(&pPresenter->Compositor, &pEntry->Ce))
        return VINF_SUCCESS;

    crPtRectsInvalidate(pPresenter);
    int rc = pPresenter->pfnRegionsChanged(pPresenter);
    if (!RT_SUCCESS(rc))
    {
        crWarning("pfnRegionsChanged failed, rc %d", rc);
        return rc;
    }
    return VINF_SUCCESS;
}

int CrPtInit(PCR_PRESENTER pPresenter, PFNCRDISPLAY_REGIONS_CHANGED pfnRegionsChanged, PFNCRDISPLAY_DRAW_ENTRY pfnDrawEntry, PFNCRDISPLAY_DRAW_TEXTURE pfnDrawTexture)
{
    memset(pPresenter, 0, sizeof (*pPresenter));
    VBoxVrCompositorInit(&pPresenter->Compositor, NULL);
    pPresenter->StretchX = 1.0;
    pPresenter->StretchY = 1.0;
    pPresenter->pfnRegionsChanged = pfnRegionsChanged;
    pPresenter->pfnDrawEntry = pfnDrawEntry;
    pPresenter->pfnDrawTexture = pfnDrawTexture;
    return VINF_SUCCESS;
}

void CrPtTerm(PCR_PRESENTER pPresenter)
{
    VBoxVrCompositorTerm(&pPresenter->Compositor);
    if (pPresenter->paDstRects)
        RTMemFree(pPresenter->paDstRects);
    if (pPresenter->paSrcRects)
        RTMemFree(pPresenter->paSrcRects);
}

void CrPtSetStretching(PCR_PRESENTER pPresenter, float StretchX, float StretchY)
{
    pPresenter->StretchX = StretchX;
    pPresenter->StretchY = StretchY;
    crPtRectsInvalidate(pPresenter);
}

int CrPtGetRegions(PCR_PRESENTER pPresenter, uint32_t *pcRegions, const RTRECT **ppaRegions)
{
    int rc = crPtRectsCheckInit(pPresenter);
    if (!RT_SUCCESS(rc))
    {
        crWarning("crPtRectsCheckInit failed, rc %d", rc);
        return rc;
    }

    Assert(pPresenter->cRects != CR_DISPLAY_RECTS_UNDEFINED);

    *pcRegions = pPresenter->cRects;
    *ppaRegions = pPresenter->paDstRects;
    return VINF_SUCCESS;
}

/* DISPLAY */
#define CR_DISPLAY_FROM_PRESENTER(_p) ((PCR_DISPLAY)(((uint8_t*)(_p)) - RT_OFFSETOF(CR_DISPLAY, Presenter)))

static DECLCALLBACK(int) crDpCbRegionsChanged(struct CR_PRESENTER *pPresenter)
{
    uint32_t cRegions;
    const RTRECT *paRegions;
    int rc = CrPtGetRegions(pPresenter, &cRegions, &paRegions);
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrPtGetRegions failed, rc %d", rc);
        return rc;
    }

    PCR_DISPLAY pDisplay = CR_DISPLAY_FROM_PRESENTER(pPresenter);

    cr_server.head_spu->dispatch_table.WindowVisibleRegion(pDisplay->Mural.spuWindow, cRegions, (GLint*)paRegions);

    if (pDisplay->Mural.pvOutputRedirectInstance)
    {
        /* @todo the code assumes that RTRECT == four GLInts. */
        cr_server.outputRedirect.CRORVisibleRegion(pDisplay->Mural.pvOutputRedirectInstance,
                                                        cRegions, paRegions);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) crDpCbDrawTextureWindow(struct CR_PRESENTER *pPresenter, PCR_BLITTER pBlitter,
                                            CR_BLITTER_TEXTURE *pTexture, const RTRECT *paSrcRects, const RTRECT *paDstRects, uint32_t cRects)
{
    Assert(CrBltIsEntered(pBlitter));
    CrBltBlitTexMural(pBlitter, pTexture, paSrcRects, paDstRects, cRects, 0);
    return VINF_SUCCESS;
}

int CrDpInit(PCR_DISPLAY pDisplay)
{
    const GLint visBits = CR_RGB_BIT | CR_DOUBLE_BIT;
    int rc = CrPtInit(&pDisplay->Presenter, crDpCbRegionsChanged, CrPtCbDrawEntryAll, crDpCbDrawTextureWindow);
    if (RT_SUCCESS(rc))
    {
        if (crServerMuralInit(&pDisplay->Mural, "", visBits, -1) >= 0)
        {
            return VINF_SUCCESS;
//            crServerMuralTerm(&pDisplay->Mural);
        }
        else
        {
            crWarning("crServerMuralInit failed!");
            rc = VERR_GENERAL_FAILURE;
        }
        CrPtTerm(&pDisplay->Presenter);
    }
    CRASSERT(RT_FAILURE(rc));
    return rc;
}

void CrDpTerm(PCR_DISPLAY pDisplay)
{
    CrPtTerm(&pDisplay->Presenter);
    crServerMuralTerm(&pDisplay->Mural);
}

int CrDpBlitterTestWithMural(PCR_BLITTER pBlitter, CRMuralInfo *pMural)
{
    CrBltMuralSetCurrent(pBlitter, pMural);
    /* try to enter to make sure the blitter is initialized completely and to make sure we actually can do that */
    int rc = CrBltEnter(pBlitter, cr_server.currentCtxInfo, cr_server.currentMural);
    if (RT_SUCCESS(rc))
    {
        CrBltLeave(pBlitter);
        return VINF_SUCCESS;
    }
    else
    {
        crWarning("CrBltEnter failed, rc %d", rc);
    }
    return rc;
}

int CrDpBlitterTest(PCR_DISPLAY pDisplay, PCR_BLITTER pBlitter)
{
    return CrDpBlitterTestWithMural(pBlitter, &pDisplay->Mural);
}

void CrDpResize(PCR_DISPLAY pDisplay, uint32_t width, uint32_t height,
        uint32_t stretchedWidth, uint32_t stretchedHeight)
{
    float StretchX, StretchY;
    StretchX = ((float)stretchedWidth)/width;
    StretchY = ((float)stretchedHeight)/height;
    crServerMuralSize(&pDisplay->Mural, stretchedWidth, stretchedHeight);
    CrPtSetStretching(&pDisplay->Presenter, StretchX, StretchY);
}

int CrDpEntryRegionsSet(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions)
{
    return CrPtEntryRegionsSet(&pDisplay->Presenter, &pEntry->Pe, pPos, cRegions, paRegions);
}

int CrDpEntryRegionsAdd(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions)
{
    return CrPtEntryRegionsAdd(&pDisplay->Presenter, &pEntry->Pe, pPos, cRegions, paRegions);
}

int CrDpPresentEntry(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry)
{
    return CrPtPresentEntry(&pDisplay->Presenter, &pEntry->Pe, pDisplay->pBlitter);
}

void CrDpEntryInit(PCR_DISPLAY_ENTRY pEntry, PCR_BLITTER_TEXTURE pTextureData)
{
    CrPtEntryInit(&pEntry->Pe, pTextureData);
}

void CrDpEntryCleanup(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry)
{
    CrPtEntryRemove(&pDisplay->Presenter, &pEntry->Pe);
}

int CrDemInit(PCR_DISPLAY_ENTRY_MAP pMap)
{
    pMap->pTextureMap = crAllocHashtable();
    if (pMap->pTextureMap)
        return VINF_SUCCESS;

    crWarning("crAllocHashtable failed!");
    return VERR_NO_MEMORY;
}

void CrDemTerm(PCR_DISPLAY_ENTRY_MAP pMap)
{
    crFreeHashtable(pMap->pTextureMap, crFree);
}

PCR_DISPLAY_ENTRY CrDemEntryGetCreate(PCR_DISPLAY_ENTRY_MAP pMap, GLuint idTexture, CRContextInfo *pCtxInfo)
{
    PCR_DISPLAY_ENTRY pEntry = (PCR_DISPLAY_ENTRY)crHashtableSearch(pMap->pTextureMap, idTexture);
    if (pEntry)
        return pEntry;

    CRContext *pContext = pCtxInfo->pContext;
    if (!pContext)
    {
        crWarning("pContext is null!");
        return NULL;
    }

    CRTextureObj *pTobj = (CRTextureObj*)crHashtableSearch(pContext->shared->textureTable, idTexture);
    if (!pTobj)
    {
        crWarning("pTobj is null!");
        return NULL;
    }

    GLuint hwId = crStateGetTextureObjHWID(pTobj);
    if (!hwId)
    {
        crWarning("hwId is null!");
        return NULL;
    }

    CR_BLITTER_TEXTURE TextureData;
    TextureData.width = pTobj->level[0]->width;
    TextureData.height = pTobj->level[0]->height;
    TextureData.target = pTobj->target;
    TextureData.hwid = hwId;

    pEntry = (PCR_DISPLAY_ENTRY)crAlloc(sizeof (*pEntry));
    if (!pEntry)
    {
        crWarning("crAlloc failed allocating CR_DISPLAY_ENTRY");
        return NULL;
    }

    CrDpEntryInit(pEntry, &TextureData);

    crHashtableAdd(pMap->pTextureMap, idTexture, pEntry);
    return pEntry;

}

void CrDemEntryDestroy(PCR_DISPLAY_ENTRY_MAP pMap, GLuint idTexture)
{
#ifdef DEBUG
    {
        PCR_DISPLAY_ENTRY pEntry = (PCR_DISPLAY_ENTRY)crHashtableSearch(pMap->pTextureMap, idTexture);
        if (!pEntry)
        {
            crWarning("request to delete inexistent entry");
            return;
        }

        Assert(!CrDpEntryIsUsed(pEntry));
    }
#endif
    crHashtableDelete(pMap->pTextureMap, idTexture, crFree);
}

#define CR_PRESENT_SCREEN_MASK 0xffff
#define CR_PRESENT_FLAGS_OFFSET 16

#define CR_PRESENT_GET_SCREEN(_cfg) ((_cfg) & CR_PRESENT_SCREEN_MASK)
#define CR_PRESENT_GET_FLAGS(_cfg) ((_cfg) >> CR_PRESENT_FLAGS_OFFSET)

int crServerBlitterInit(PCR_BLITTER pBlitter, CRMuralInfo*pMural)
{
    int rc = CrBltInit(pBlitter, pMural);
    if (RT_SUCCESS(rc))
    {
        rc = CrDpBlitterTestWithMural(pBlitter, pMural);
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;
        else
            crWarning("CrDpBlitterTestWithMural failed, rc %d", rc);
        CrBltTerm(pBlitter);
    }
    else
        crWarning("CrBltInit failed, rc %d", rc);
    return rc;
}

PCR_BLITTER crServerGetFBOPresentBlitter(CRMuralInfo*pMural)
{
    if (cr_server.fFBOModeBlitterInited > 0)
        return &cr_server.FBOModeBlitter;
    if (!cr_server.fFBOModeBlitterInited)
    {
        int rc = crServerBlitterInit(&cr_server.FBOModeBlitter, pMural);
        if (RT_SUCCESS(rc))
        {
            cr_server.fFBOModeBlitterInited = 1;
            return &cr_server.FBOModeBlitter;
        }
        crWarning("crServerBlitterInit failed rc %d", rc);
        cr_server.fFBOModeBlitterInited = -1;
    }
    return NULL;
}

static int8_t crServerCheckInitDisplayBlitter()
{
    if (cr_server.fPresentBlitterInited)
        return cr_server.fPresentBlitterInited;

    crDebug("Display Functionality is requested");
    Assert(!ASMBitTest(cr_server.DisplaysInitMap, 0));

    int rc = CrDemInit(&cr_server.PresentTexturepMap);
    if (RT_SUCCESS(rc))
    {
        rc = CrDpInit(&cr_server.aDispplays[0]);
        if (RT_SUCCESS(rc))
        {
            CRMuralInfo*pMural = CrDpGetMural(&cr_server.aDispplays[0]);
            rc = crServerBlitterInit(&cr_server.PresentBlitter, pMural);
            if (RT_SUCCESS(rc))
            {
                CrDpBlitterSet(&cr_server.aDispplays[0], &cr_server.PresentBlitter);
                CrDpResize(&cr_server.aDispplays[0],
                            cr_server.screen[0].w, cr_server.screen[0].h,
                            cr_server.screen[0].w, cr_server.screen[0].h);
                ASMBitSet(cr_server.DisplaysInitMap, 0);
                cr_server.fPresentBlitterInited = 1;
                return 1;
            }
            else
            {
                crWarning("crServerBlitterInit failed, rc %d", rc);
            }
            CrDpTerm(&cr_server.aDispplays[0]);
        }
        else
        {
            crWarning("CrDpInit failed, rc %d", rc);
        }
        CrDemTerm(&cr_server.PresentTexturepMap);
    }
    else
    {
        crWarning("CrDemInit failed, rc %d", rc);
    }

    cr_server.fPresentBlitterInited = -1;
    return -1;
}

static bool crServerDisplayIsSupported()
{
    return crServerCheckInitDisplayBlitter() > 0;
}

PCR_DISPLAY crServerDisplayGetInitialized(uint32_t idScreen)
{
    if (ASMBitTest(cr_server.DisplaysInitMap, idScreen))
        return &cr_server.aDispplays[idScreen];
    return NULL;
}

static PCR_DISPLAY crServerDisplayGet(uint32_t idScreen)
{
    if (idScreen >= CR_MAX_GUEST_MONITORS)
    {
        crWarning("invalid idScreen %d", idScreen);
        return NULL;
    }

    if (ASMBitTest(cr_server.DisplaysInitMap, idScreen))
        return &cr_server.aDispplays[idScreen];

    if (crServerCheckInitDisplayBlitter() > 0)
    {
        /* the display (screen id == 0) can be initialized while doing crServerCheckInitDisplayBlitter,
         * so re-check the bit map */
        if (ASMBitTest(cr_server.DisplaysInitMap, idScreen))
            return &cr_server.aDispplays[idScreen];

        int rc = CrDpInit(&cr_server.aDispplays[idScreen]);
        if (RT_SUCCESS(rc))
        {
            CrDpBlitterSet(&cr_server.aDispplays[idScreen], &cr_server.PresentBlitter);
            CrDpResize(&cr_server.aDispplays[idScreen],
                    cr_server.screen[idScreen].w, cr_server.screen[idScreen].h,
                    cr_server.screen[idScreen].w, cr_server.screen[idScreen].h);
            ASMBitSet(cr_server.DisplaysInitMap, idScreen);
            return &cr_server.aDispplays[idScreen];
        }
        else
        {
            crWarning("CrDpInit failed for screen %d", idScreen);
        }
    }
    else
    {
        crWarning("crServerCheckInitDisplayBlitter said \"UNSUPPORTED\"");
    }

    return NULL;
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchTexPresent(GLuint texture, GLuint cfg, GLint xPos, GLint yPos, GLint cRects, GLint *pRects)
{
    uint32_t idScreen = CR_PRESENT_GET_SCREEN(cfg);
    PCR_DISPLAY pDisplay = crServerDisplayGet(idScreen);
    if (!pDisplay)
    {
        crWarning("crServerDisplayGet Failed");
        return;
    }

    PCR_DISPLAY_ENTRY pEntry = CrDemEntryGetCreate(&cr_server.PresentTexturepMap, texture, cr_server.currentCtxInfo);
    if (!pEntry)
    {
        crWarning("CrDemEntryGetCreate Failed");
        return;
    }

    RTPOINT Point = {xPos, yPos};
    int rc = CrDpEntryRegionsAdd(pDisplay, pEntry, &Point, (uint32_t)cRects, (const RTRECT*)pRects);
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrDpEntrySetRegions Failed rc %d", rc);
        return;
    }

    rc = CrDpPresentEntry(pDisplay, pEntry);
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrDpEntrySetRegions Failed rc %d", rc);
        return;
    }
}
