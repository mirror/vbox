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

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/list.h>
#include <iprt/memcache.h>

#include "render/renderspu.h"

/* DISPLAY */

int CrDpInit(PCR_DISPLAY pDisplay)
{
    const GLint visBits = cr_server.MainContextInfo.CreateInfo.visualBits;
    if (crServerMuralInit(&pDisplay->Mural, "", visBits, -1, GL_FALSE) < 0)
    {
        crWarning("crServerMuralInit failed!");
        return VERR_GENERAL_FAILURE;
    }

    crServerMuralVisibleRegion(&pDisplay->Mural, 0, NULL);
    crServerMuralShow(&pDisplay->Mural, GL_TRUE);

    pDisplay->fForcePresent = GL_FALSE;
    return VINF_SUCCESS;
}

void CrDpTerm(PCR_DISPLAY pDisplay)
{
    crServerMuralTerm(&pDisplay->Mural);
}

void CrDpResize(PCR_DISPLAY pDisplay, int32_t xPos, int32_t yPos, uint32_t width, uint32_t height)
{
    if (xPos != pDisplay->Mural.gX
            || yPos != pDisplay->Mural.gY
            || width != pDisplay->Mural.width
            || height != pDisplay->Mural.height)
    {
        crServerMuralPosition(&pDisplay->Mural, xPos, yPos, GL_TRUE);
        if (!crServerMuralSize(&pDisplay->Mural, width, height))
            crServerCheckMuralGeometry(&pDisplay->Mural);
    }
    else
        crServerCheckMuralGeometry(&pDisplay->Mural);
}

void CrDpReparent(PCR_DISPLAY pDisplay, CRScreenInfo *pScreen)
{
    renderspuSetWindowId(pScreen->winID);
    crServerWindowReparent(&pDisplay->Mural);
    renderspuSetWindowId(cr_server.screen[0].winID);

    CrDpResize(pDisplay, pScreen->x, pScreen->y, pScreen->w, pScreen->h);
}


int CrDpSaveState(PCR_DISPLAY pDisplay, PSSMHANDLE pSSM)
{
    VBOXVR_SCR_COMPOSITOR_ITERATOR Iter;
    CrVrScrCompositorIterInit(&pDisplay->Mural.Compositor, &Iter);
    PVBOXVR_SCR_COMPOSITOR_ENTRY pEntry;
    uint32_t u32 = 0;
    while ((pEntry = CrVrScrCompositorIterNext(&Iter)) != NULL)
    {
        ++u32;
    }

    int rc = SSMR3PutU32(pSSM, u32);
    AssertRCReturn(rc, rc);

    CrVrScrCompositorIterInit(&pDisplay->Mural.Compositor, &Iter);

    while ((pEntry = CrVrScrCompositorIterNext(&Iter)) != NULL)
    {
        CR_DISPLAY_ENTRY *pDEntry = CR_DENTRY_FROM_CENTRY(pEntry);
        rc = CrDemEntrySaveState(pDEntry, pSSM);
        AssertRCReturn(rc, rc);

        u32 = CrVrScrCompositorEntryFlagsGet(&pDEntry->CEntry);
        rc = SSMR3PutU32(pSSM, u32);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutS32(pSSM, CrVrScrCompositorEntryPosGet(&pDEntry->CEntry)->x);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutS32(pSSM, CrVrScrCompositorEntryPosGet(&pDEntry->CEntry)->y);
        AssertRCReturn(rc, rc);

        const RTRECT * pRects;
        rc = CrVrScrCompositorEntryRegionsGet(&pDisplay->Mural.Compositor, &pDEntry->CEntry, &u32, NULL, NULL, &pRects);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutU32(pSSM, u32);
        AssertRCReturn(rc, rc);

        if (u32)
        {
            rc = SSMR3PutMem(pSSM, pRects, u32 * sizeof (*pRects));
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}

int CrDpLoadState(PCR_DISPLAY pDisplay, PSSMHANDLE pSSM, uint32_t version)
{
    uint32_t u32 = 0;
    int rc = SSMR3GetU32(pSSM, &u32);
    AssertRCReturn(rc, rc);

    if (!u32)
        return VINF_SUCCESS;

    CrDpEnter(pDisplay);

    for (uint32_t i = 0; i < u32; ++i)
    {
        CR_DISPLAY_ENTRY *pDEntry;
        rc = CrDemEntryLoadState(&cr_server.PresentTexturepMap, &pDEntry, pSSM);
        AssertRCReturn(rc, rc);

        uint32_t fFlags;
        rc = SSMR3GetU32(pSSM, &fFlags);
        AssertRCReturn(rc, rc);

        CrVrScrCompositorEntryFlagsSet(&pDEntry->CEntry, fFlags);

        RTPOINT Pos;
        rc = SSMR3GetS32(pSSM, &Pos.x);
        AssertRCReturn(rc, rc);

        rc = SSMR3GetS32(pSSM, &Pos.y);
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

        rc = CrDpEntryRegionsAdd(pDisplay, pDEntry, &Pos, (uint32_t)cRects, (const RTRECT*)pRects, NULL);
        AssertRCReturn(rc, rc);

        if (pRects)
            crFree(pRects);
    }

    CrDpLeave(pDisplay);

    return VINF_SUCCESS;
}


int CrDpEntryRegionsSet(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions)
{
    int rc = CrVrScrCompositorEntryRegionsSet(&pDisplay->Mural.Compositor, pEntry ? &pEntry->CEntry : NULL, pPos, cRegions, paRegions, false, NULL);
    return rc;
}

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

int CrDpEntryRegionsAdd(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions, CR_DISPLAY_ENTRY_MAP *pMap)
{
    uint32_t fChangeFlags = 0;
    VBOXVR_SCR_COMPOSITOR_ENTRY *pReplacedScrEntry = NULL;

    if (pMap)
        CrDemEnter(pMap);

    int rc = CrVrScrCompositorEntryRegionsAdd(&pDisplay->Mural.Compositor, pEntry ? &pEntry->CEntry : NULL, pPos, cRegions, paRegions, false, &pReplacedScrEntry, &fChangeFlags);
    if (RT_SUCCESS(rc))
    {
        if (fChangeFlags & VBOXVR_COMPOSITOR_CF_REGIONS_CHANGED)
        {
            uint32_t cRects;
            const RTRECT *pRects;
            rc = CrVrScrCompositorRegionsGet(&pDisplay->Mural.Compositor, &cRects, NULL, &pRects, NULL);
            if (RT_SUCCESS(rc))
                crServerMuralVisibleRegion(&pDisplay->Mural, cRects, (GLint *)pRects);
            else
                crWarning("CrVrScrCompositorRegionsGet failed, rc %d", rc);

            Assert(!pReplacedScrEntry);
        }
        else if (fChangeFlags & VBOXVR_COMPOSITOR_CF_ENTRY_REPLACED)
        {
            Assert(pReplacedScrEntry);
        }
        else
        {
            Assert(!pReplacedScrEntry);
        }
    }
    else
        crWarning("CrVrScrCompositorEntryRegionsAdd failed, rc %d", rc);

    if (pMap)
        CrDemLeave(pMap, CR_DENTRY_FROM_CENTRY(pEntry), CR_DENTRY_FROM_CENTRY(pReplacedScrEntry));

    return rc;
}

void CrDpEntryRegionsClear(PCR_DISPLAY pDisplay)
{
    bool fChanged = false;
    CrVrScrCompositorRegionsClear(&pDisplay->Mural.Compositor, &fChanged);
    if (fChanged)
    {
        crServerMuralVisibleRegion(&pDisplay->Mural, 0, NULL);
    }
}

#define PCR_DISPLAY_ENTRY_FROM_CENTRY(_pe) ((PCR_DISPLAY_ENTRY)((uint8_t*)(_pe) - RT_OFFSETOF(CR_DISPLAY_ENTRY, CEntry)))
static DECLCALLBACK(void) crDpEntryCEntryReleaseCB(const struct VBOXVR_SCR_COMPOSITOR *pCompositor, struct VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry, struct VBOXVR_SCR_COMPOSITOR_ENTRY *pReplacingEntry)
{
    PCR_DISPLAY_ENTRY pCEntry = PCR_DISPLAY_ENTRY_FROM_CENTRY(pEntry);
    CrDemEntryRelease(pCEntry);
}

void CrDpEntryInit(PCR_DISPLAY_ENTRY pEntry, const VBOXVR_TEXTURE *pTextureData, uint32_t fFlags)
{
    CrVrScrCompositorEntryInit(&pEntry->CEntry, pTextureData, crDpEntryCEntryReleaseCB);
    CrVrScrCompositorEntryFlagsSet(&pEntry->CEntry, fFlags);
    CrVrScrCompositorEntryInit(&pEntry->RootVrCEntry, pTextureData, NULL);
    CrVrScrCompositorEntryFlagsSet(&pEntry->RootVrCEntry, fFlags);
    pEntry->pvORInstance = NULL;
}

void CrDpEntryCleanup(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry)
{
    CrVrScrCompositorEntryRemove(&pDisplay->Mural.Compositor, &pEntry->CEntry);
}

void CrDpEnter(PCR_DISPLAY pDisplay)
{
    pDisplay->fForcePresent |= crServerVBoxCompositionPresentNeeded(&pDisplay->Mural);
    crServerVBoxCompositionDisableEnter(&pDisplay->Mural);
}

void CrDpLeave(PCR_DISPLAY pDisplay)
{
    pDisplay->Mural.fDataPresented = GL_TRUE;
    pDisplay->Mural.fOrPresentOnReenable = GL_TRUE;
    crServerVBoxCompositionDisableLeave(&pDisplay->Mural, pDisplay->fForcePresent);
    pDisplay->fForcePresent = GL_FALSE;
}

typedef struct CR_DEM_ENTRY_INFO
{
    CRTextureObj *pTobj;
    uint32_t cEntries;
} CR_DEM_ENTRY_INFO;

typedef struct CR_DEM_ENTRY
{
    CR_DISPLAY_ENTRY Entry;
    CR_DEM_ENTRY_INFO *pInfo;
    CR_DISPLAY_ENTRY_MAP *pMap;
    RTLISTNODE Node;
} CR_DEM_ENTRY;

#define PCR_DEM_ENTRY_FROM_ENTRY(_pEntry) ((CR_DEM_ENTRY*)((uint8_t*)(_pEntry) - RT_OFFSETOF(CR_DEM_ENTRY, Entry)))

static RTMEMCACHE g_VBoxCrDemLookasideList;
static RTMEMCACHE g_VBoxCrDemInfoLookasideList;

int CrDemGlobalInit()
{
    int rc = RTMemCacheCreate(&g_VBoxCrDemLookasideList, sizeof (CR_DEM_ENTRY),
                            0, /* size_t cbAlignment */
                            UINT32_MAX, /* uint32_t cMaxObjects */
                            NULL, /* PFNMEMCACHECTOR pfnCtor*/
                            NULL, /* PFNMEMCACHEDTOR pfnDtor*/
                            NULL, /* void *pvUser*/
                            0 /* uint32_t fFlags*/
                            );
    if (RT_SUCCESS(rc))
    {
        rc = RTMemCacheCreate(&g_VBoxCrDemInfoLookasideList, sizeof (CR_DEM_ENTRY_INFO),
                                    0, /* size_t cbAlignment */
                                    UINT32_MAX, /* uint32_t cMaxObjects */
                                    NULL, /* PFNMEMCACHECTOR pfnCtor*/
                                    NULL, /* PFNMEMCACHEDTOR pfnDtor*/
                                    NULL, /* void *pvUser*/
                                    0 /* uint32_t fFlags*/
                                    );
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;
        else
            crWarning("RTMemCacheCreate failed rc %d", rc);

        RTMemCacheDestroy(g_VBoxCrDemLookasideList);
    }
    else
        crWarning("RTMemCacheCreate failed rc %d", rc);
    return VINF_SUCCESS;
}

void CrDemTeGlobalTerm()
{
    RTMemCacheDestroy(g_VBoxCrDemLookasideList);
    RTMemCacheDestroy(g_VBoxCrDemInfoLookasideList);
}

static CR_DEM_ENTRY* crDemEntryAlloc()
{
    return (CR_DEM_ENTRY*)RTMemCacheAlloc(g_VBoxCrDemLookasideList);
}

static CR_DEM_ENTRY_INFO* crDemEntryInfoAlloc()
{
    return (CR_DEM_ENTRY_INFO*)RTMemCacheAlloc(g_VBoxCrDemInfoLookasideList);
}

static void crDemEntryFree(CR_DEM_ENTRY* pDemEntry)
{
    crServerDEntryCleanup(&pDemEntry->Entry);
    RTMemCacheFree(g_VBoxCrDemLookasideList, pDemEntry);
}

static void crDemEntryInfoFree(CR_DEM_ENTRY_INFO* pDemEntryInfo)
{
    RTMemCacheFree(g_VBoxCrDemInfoLookasideList, pDemEntryInfo);
}

void crDemEntryRelease(PCR_DISPLAY_ENTRY_MAP pMap, CR_DEM_ENTRY *pDemEntry)
{
    CR_DEM_ENTRY_INFO *pInfo = pDemEntry->pInfo;
    CRTextureObj *pTobj = pInfo->pTobj;

    --pInfo->cEntries;

    if (!pInfo->cEntries)
    {
        CR_STATE_SHAREDOBJ_USAGE_CLEAR(pInfo->pTobj, cr_server.MainContextInfo.pContext);

        crHashtableDelete(pMap->pTexIdToDemInfoMap, pTobj->id, NULL);

        crDemEntryInfoFree(pInfo);
    }

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

    if (!pMap->cEntered)
        crDemEntryFree(pDemEntry);
    else
        RTListNodeInsertAfter(&pMap->ReleasedList, &pDemEntry->Node);
}

int CrDemInit(PCR_DISPLAY_ENTRY_MAP pMap)
{
    pMap->pTexIdToDemInfoMap = crAllocHashtable();
    if (pMap->pTexIdToDemInfoMap)
    {
        RTListInit(&pMap->ReleasedList);
        return VINF_SUCCESS;
    }

    crWarning("crAllocHashtable failed");
    return VERR_NO_MEMORY;
}

void CrDemTerm(PCR_DISPLAY_ENTRY_MAP pMap)
{
    CRASSERT(RTListIsEmpty(&pMap->ReleasedList));
    CRASSERT(!pMap->cEntered);
    crFreeHashtable(pMap->pTexIdToDemInfoMap, NULL);
    pMap->pTexIdToDemInfoMap = NULL;
}

void CrDemEnter(PCR_DISPLAY_ENTRY_MAP pMap)
{
    ++pMap->cEntered;
    Assert(pMap->cEntered);
}

void CrDemLeave(PCR_DISPLAY_ENTRY_MAP pMap, PCR_DISPLAY_ENTRY pNewEntry, PCR_DISPLAY_ENTRY pReplacedEntry)
{
    Assert(pMap->cEntered);
    --pMap->cEntered;
    Assert(!pReplacedEntry || pNewEntry);
    if (pNewEntry && pReplacedEntry)
    {
        CR_DEM_ENTRY *pNewDemEntry = PCR_DEM_ENTRY_FROM_ENTRY(pNewEntry);
        CR_DEM_ENTRY *pReplacedDemEntry = PCR_DEM_ENTRY_FROM_ENTRY(pReplacedEntry);
        Assert(!RTListIsEmpty(&pMap->ReleasedList));
        Assert(!RTListIsEmpty(&pReplacedDemEntry->Node));
        Assert(RTListIsEmpty(&pNewDemEntry->Node));
        Assert(!pNewDemEntry->Entry.pvORInstance);
        if (!pNewDemEntry->Entry.pvORInstance)
        {
            pNewDemEntry->Entry.pvORInstance = pReplacedDemEntry->Entry.pvORInstance;
            pReplacedDemEntry->Entry.pvORInstance = NULL;
        }
        RTListNodeRemove(&pReplacedDemEntry->Node);
        crDemEntryFree(pReplacedDemEntry);
    }

    if (!pMap->cEntered)
    {
        CR_DEM_ENTRY *pCurEntry, *pNextEntry;
        RTListForEachSafe(&pMap->ReleasedList, pCurEntry, pNextEntry, CR_DEM_ENTRY, Node)
        {
            RTListNodeRemove(&pCurEntry->Node);
            crDemEntryFree(pCurEntry);
        }
    }
}

void CrDemEntryRelease(PCR_DISPLAY_ENTRY pEntry)
{
    CR_DEM_ENTRY *pDemEntry = PCR_DEM_ENTRY_FROM_ENTRY(pEntry);
    crDemEntryRelease(pDemEntry->pMap, pDemEntry);
}

int CrDemEntrySaveState(PCR_DISPLAY_ENTRY pEntry, PSSMHANDLE pSSM)
{
    CR_DEM_ENTRY *pDemEntry = PCR_DEM_ENTRY_FROM_ENTRY(pEntry);
    int  rc = SSMR3PutU32(pSSM, pDemEntry->pInfo->pTobj->id);
    AssertRCReturn(rc, rc);
    return rc;
}

int CrDemEntryLoadState(PCR_DISPLAY_ENTRY_MAP pMap, PCR_DISPLAY_ENTRY *ppEntry, PSSMHANDLE pSSM)
{
    uint32_t u32;
    int  rc = SSMR3GetU32(pSSM, &u32);
    AssertRCReturn(rc, rc);

    PCR_DISPLAY_ENTRY pEntry = CrDemEntryAcquire(pMap, u32, CRBLT_F_INVERT_SRC_YCOORDS);
    if (!pEntry)
    {
        crWarning("CrDemEntryAcquire failed");
        return VERR_NO_MEMORY;
    }

    *ppEntry = pEntry;
    return VINF_SUCCESS;
}

PCR_DISPLAY_ENTRY CrDemEntryAcquire(PCR_DISPLAY_ENTRY_MAP pMap, GLuint idTexture, uint32_t fFlags)
{
    CR_DEM_ENTRY *pDemEntry = NULL;

    CRSharedState *pShared = crStateGlobalSharedAcquire();
    if (!pShared)
    {
        crWarning("pShared is null!");
        return NULL;
    }

    CRTextureObj *pTobj = (CRTextureObj*)crHashtableSearch(pShared->textureTable, idTexture);
    if (!pTobj)
    {
        crWarning("pTobj is null!");
        crStateGlobalSharedRelease();
        return NULL;
    }

    Assert(pTobj->id == idTexture);

    GLuint hwId = crStateGetTextureObjHWID(pTobj);
    if (!hwId)
    {
        crWarning("hwId is null!");
        crStateGlobalSharedRelease();
        return NULL;
    }

    VBOXVR_TEXTURE TextureData;
    TextureData.width = pTobj->level[0]->width;
    TextureData.height = pTobj->level[0]->height;
    TextureData.target = pTobj->target;
    TextureData.hwid = hwId;

    pDemEntry = crDemEntryAlloc();
    if (!pDemEntry)
    {
        crWarning("crDemEntryAlloc failed allocating CR_DEM_ENTRY");
        crStateGlobalSharedRelease();
        return NULL;
    }

    CrDpEntryInit(&pDemEntry->Entry, &TextureData, fFlags);

    CR_DEM_ENTRY_INFO *pInfo = (CR_DEM_ENTRY_INFO*)crHashtableSearch(pMap->pTexIdToDemInfoMap, pTobj->id);
    if (!pInfo)
    {
        pInfo = crDemEntryInfoAlloc();
        CRASSERT(pInfo);
        crHashtableAdd(pMap->pTexIdToDemInfoMap, pTobj->id, pInfo);
        pInfo->cEntries = 0;
        pInfo->pTobj = pTobj;
    }

    ++pInfo->cEntries;
    pDemEntry->pInfo = pInfo;
    pDemEntry->pMap = pMap;
    RTListInit(&pDemEntry->Node);

    /* just use main context info's context to hold the texture reference */
    CR_STATE_SHAREDOBJ_USAGE_SET(pTobj, cr_server.MainContextInfo.pContext);

    return &pDemEntry->Entry;
}

PCR_DISPLAY crServerDisplayGetInitialized(uint32_t idScreen)
{
    if (idScreen >= CR_MAX_GUEST_MONITORS)
    {
        crWarning("invalid idScreen %d", idScreen);
        return NULL;
    }

    if (ASMBitTest(cr_server.DisplaysInitMap, idScreen))
    {
        Assert(cr_server.aDispplays[idScreen].Mural.screenId == idScreen);
        return &cr_server.aDispplays[idScreen];
    }
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
    {
        Assert(cr_server.aDispplays[idScreen].Mural.screenId == idScreen);
        return &cr_server.aDispplays[idScreen];
    }

     int rc = CrDpInit(&cr_server.aDispplays[idScreen]);
     if (RT_SUCCESS(rc))
     {
         CrDpResize(&cr_server.aDispplays[idScreen],
                 cr_server.screen[idScreen].x, cr_server.screen[idScreen].y,
                 cr_server.screen[idScreen].w, cr_server.screen[idScreen].h);
         ASMBitSet(cr_server.DisplaysInitMap, idScreen);
         return &cr_server.aDispplays[idScreen];
     }
     else
     {
         crWarning("CrDpInit failed for screen %d", idScreen);
     }

    return NULL;
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
    int rc;
    int cDisplays, screenCount, i;

    rc = SSMR3GetS32(pSSM, &cDisplays);
    AssertRCReturn(rc, rc);

    if (!cDisplays)
        return VINF_SUCCESS;

    rc = SSMR3GetS32(pSSM, &screenCount);
    AssertRCReturn(rc, rc);

    CRASSERT(screenCount == cr_server.screenCount);

    crServerVBoxCompositionSetEnableStateGlobal(GL_FALSE);

    for (i = 0; i < cr_server.screenCount; ++i)
    {
        int32_t    x, y;
        uint32_t   w, h;
        rc = SSMR3GetS32(pSSM, &x);
        AssertRCReturn(rc, rc);

        rc = SSMR3GetS32(pSSM, &y);
        AssertRCReturn(rc, rc);

        rc = SSMR3GetU32(pSSM, &w);
        AssertRCReturn(rc, rc);

        rc = SSMR3GetU32(pSSM, &h);
        AssertRCReturn(rc, rc);

        rc = crVBoxServerMapScreen(i, x, y, w, h, cr_server.screen[i].winID);
        AssertRCReturn(rc, rc);
    }

    crServerVBoxCompositionSetEnableStateGlobal(GL_TRUE);

    for (i = 0; i < cDisplays; ++i)
    {
        int iScreen;

        rc = SSMR3GetS32(pSSM, &iScreen);
        AssertRCReturn(rc, rc);

        PCR_DISPLAY pDisplay = crServerDisplayGet((uint32_t)iScreen);
        if (!pDisplay)
        {
            crWarning("crServerDisplayGet failed");
            return VERR_GENERAL_FAILURE;
        }

        rc = CrDpLoadState(pDisplay, pSSM, u32Version);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}

void crServerDisplayTermAll()
{
    int i;
    for (i = 0; i < cr_server.screenCount; ++i)
    {
        if (ASMBitTest(cr_server.DisplaysInitMap, i))
        {
            CrDpTerm(&cr_server.aDispplays[i]);
            ASMBitClear(cr_server.DisplaysInitMap, i);
        }
    }
}

void CrHlpFreeTexImage(CRContext *pCurCtx, GLuint idPBO, void *pvData)
{
    if (idPBO)
    {
        cr_server.head_spu->dispatch_table.UnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
        if (pCurCtx)
            cr_server.head_spu->dispatch_table.BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pCurCtx->bufferobject.packBuffer->hwid);
        else
            cr_server.head_spu->dispatch_table.BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
    }
    else
    {
        crFree(pvData);
        if (pCurCtx && crStateIsBufferBoundForCtx(pCurCtx, GL_PIXEL_PACK_BUFFER_ARB))
            cr_server.head_spu->dispatch_table.BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pCurCtx->bufferobject.packBuffer->hwid);
    }
}

void CrHlpPutTexImage(CRContext *pCurCtx, const VBOXVR_TEXTURE *pTexture, GLenum enmFormat, void *pvData)
{
    CRASSERT(pTexture->hwid);
    cr_server.head_spu->dispatch_table.BindTexture(pTexture->target, pTexture->hwid);

    if (!pCurCtx || crStateIsBufferBoundForCtx(pCurCtx, GL_PIXEL_UNPACK_BUFFER_ARB))
    {
        cr_server.head_spu->dispatch_table.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
    }

    /*read the texture, note pixels are NULL for PBO case as it's offset in the buffer*/
    cr_server.head_spu->dispatch_table.TexSubImage2D(GL_TEXTURE_2D,  0 /* level*/,  0 /*xoffset*/,  0 /*yoffset*/,  pTexture->width, pTexture->height, enmFormat, GL_UNSIGNED_BYTE, pvData);

    /*restore gl state*/
    if (pCurCtx)
    {
        CRTextureObj *pTObj;
        CRTextureLevel *pTImg;
        crStateGetTextureObjectAndImage(pCurCtx, pTexture->target, 0, &pTObj, &pTImg);

        GLuint uid = pTObj->hwid;
        cr_server.head_spu->dispatch_table.BindTexture(pTexture->target, uid);
    }
    else
    {
        cr_server.head_spu->dispatch_table.BindTexture(pTexture->target, 0);
    }

    if (pCurCtx && crStateIsBufferBoundForCtx(pCurCtx, GL_PIXEL_UNPACK_BUFFER_ARB))
        cr_server.head_spu->dispatch_table.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pCurCtx->bufferobject.unpackBuffer->hwid);
}

void* CrHlpGetTexImage(CRContext *pCurCtx, const VBOXVR_TEXTURE *pTexture, GLuint idPBO, GLenum enmFormat)
{
    void *pvData = NULL;
    cr_server.head_spu->dispatch_table.BindTexture(pTexture->target, pTexture->hwid);

    if (idPBO)
    {
        cr_server.head_spu->dispatch_table.BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, idPBO);
    }
    else
    {
        if (!pCurCtx || crStateIsBufferBoundForCtx(pCurCtx, GL_PIXEL_PACK_BUFFER_ARB))
        {
            cr_server.head_spu->dispatch_table.BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
        }

        pvData = crAlloc(4*pTexture->width*pTexture->height);
        if (!pvData)
        {
            crWarning("Out of memory in CrHlpGetTexImage");
            return NULL;
        }
    }

    /*read the texture, note pixels are NULL for PBO case as it's offset in the buffer*/
    cr_server.head_spu->dispatch_table.GetTexImage(GL_TEXTURE_2D, 0, enmFormat, GL_UNSIGNED_BYTE, pvData);

    /*restore gl state*/
    if (pCurCtx)
    {
        CRTextureObj *pTObj;
        CRTextureLevel *pTImg;
        crStateGetTextureObjectAndImage(pCurCtx, pTexture->target, 0, &pTObj, &pTImg);

        GLuint uid = pTObj->hwid;
        cr_server.head_spu->dispatch_table.BindTexture(pTexture->target, uid);
    }
    else
    {
        cr_server.head_spu->dispatch_table.BindTexture(pTexture->target, 0);
    }

    if (idPBO)
    {
        pvData = cr_server.head_spu->dispatch_table.MapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
        if (!pvData)
        {
            crWarning("Failed to MapBuffer in CrHlpGetTexImage");
            return NULL;
        }
    }

    CRASSERT(pvData);
    return pvData;
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchVBoxTexPresent(GLuint texture, GLuint cfg, GLint xPos, GLint yPos, GLint cRects, const GLint *pRects)
{
    uint32_t idScreen = CR_PRESENT_GET_SCREEN(cfg);
    if (idScreen >= CR_MAX_GUEST_MONITORS)
    {
        crWarning("Invalid guest screen");
        return;
    }

    PCR_DISPLAY pDisplay;
    PCR_DISPLAY_ENTRY pEntry = NULL;

    if (texture)
    {
        pEntry = CrDemEntryAcquire(&cr_server.PresentTexturepMap, texture, (cfg & CR_PRESENT_FLAG_TEX_NONINVERT_YCOORD) ? 0 : CRBLT_F_INVERT_SRC_YCOORDS);
        if (!pEntry)
        {
            crWarning("CrDemEntryAcquire Failed");
            return;
        }

        pDisplay = crServerDisplayGet(idScreen);
        if (!pDisplay)
        {
            crWarning("crServerDisplayGet Failed");
            return;
        }
    }
    else
    {
        pDisplay = crServerDisplayGetInitialized(idScreen);
        if (!pDisplay)
        {
            /* no display initialized, and nothing to present */
            return;
        }
    }

    CrDpEnter(pDisplay);

    if (!(cfg & CR_PRESENT_FLAG_CLEAR_RECTS))
    {
        RTPOINT Point = {xPos, yPos};
        int rc = CrDpEntryRegionsAdd(pDisplay, pEntry, &Point, (uint32_t)cRects, (const RTRECT*)pRects, &cr_server.PresentTexturepMap);
        if (!RT_SUCCESS(rc))
        {
            crWarning("CrDpEntryRegionsAdd Failed rc %d", rc);
//            if (pEntry)
//                CrDemEntryRelease(pEntry);
            return;
        }
    }
    else
    {
        CrDpEntryRegionsClear(pDisplay);
    }

    CrDpLeave(pDisplay);
}
