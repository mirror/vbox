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


/* DISPLAY */

//static DECLCALLBACK(int) crDpCbRegionsChanged(struct CR_PRESENTER *pPresenter)
//{
//    uint32_t cRegions;
//    const RTRECT *paRegions;
//    int rc = CrPtGetRegions(pPresenter, &cRegions, &paRegions);
//    if (!RT_SUCCESS(rc))
//    {
//        crWarning("CrPtGetRegions failed, rc %d", rc);
//        return rc;
//    }
//
//    PCR_DISPLAY pDisplay = CR_DISPLAY_FROM_PRESENTER(pPresenter);
//
//    cr_server.head_spu->dispatch_table.WindowVisibleRegion(pDisplay->Mural.spuWindow, cRegions, (GLint*)paRegions);
//
//    if (pDisplay->Mural.pvOutputRedirectInstance)
//    {
//        /* @todo the code assumes that RTRECT == four GLInts. */
//        cr_server.outputRedirect.CRORVisibleRegion(pDisplay->Mural.pvOutputRedirectInstance,
//                                                        cRegions, paRegions);
//    }
//
//    return VINF_SUCCESS;
//}

int CrDpInit(PCR_DISPLAY pDisplay)
{
    const GLint visBits = cr_server.MainContextInfo.CreateInfo.visualBits;
    if (crServerMuralInit(&pDisplay->Mural, "", visBits, -1, GL_FALSE) < 0)
    {
        crWarning("crServerMuralInit failed!");
        return VERR_GENERAL_FAILURE;
    }
    pDisplay->fForcePresent = GL_FALSE;
    return VINF_SUCCESS;
}

void CrDpTerm(PCR_DISPLAY pDisplay)
{
    crServerMuralTerm(&pDisplay->Mural);
}

void CrDpResize(PCR_DISPLAY pDisplay, int32_t xPos, int32_t yPos, uint32_t width, uint32_t height)
{
    crServerMuralVisibleRegion(&pDisplay->Mural, 0, NULL);
    crServerMutalPosition(&pDisplay->Mural, xPos, yPos);
    crServerMuralSize(&pDisplay->Mural, width, height);
    crServerMuralShow(&pDisplay->Mural, GL_TRUE);
    CrVrScrCompositorSetStretching(&pDisplay->Mural.Compositor, 1., 1.);
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

int CrDpEntryRegionsAdd(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions)
{
    uint32_t fChangeFlags = 0;
    int rc = CrVrScrCompositorEntryRegionsAdd(&pDisplay->Mural.Compositor, pEntry ? &pEntry->CEntry : NULL, pPos, cRegions, paRegions, false, &fChangeFlags);
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
        }
    }
    else
        crWarning("CrVrScrCompositorEntryRegionsAdd failed, rc %d", rc);

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

void CrDpEntryInit(PCR_DISPLAY_ENTRY pEntry, const VBOXVR_TEXTURE *pTextureData, void *pvUserData1, void *pvUserData2)
{
    CrVrScrCompositorEntryInit(&pEntry->CEntry, pTextureData, crDpEntryCEntryReleaseCB);
    CrVrScrCompositorEntryFlagsSet(&pEntry->CEntry, CRBLT_F_INVERT_SRC_YCOORDS);
    CrVrScrCompositorEntryInit(&pEntry->RootVrCEntry, pTextureData, NULL);
    CrVrScrCompositorEntryFlagsSet(&pEntry->RootVrCEntry, CRBLT_F_INVERT_SRC_YCOORDS);
    pEntry->pvUserData1 = pvUserData1;
    pEntry->pvUserData2 = pvUserData2;
}

void CrDpEntryCleanup(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry)
{
    CrVrScrCompositorEntryRemove(&pDisplay->Mural.Compositor, &pEntry->CEntry);
}

void CrDpEnter(PCR_DISPLAY pDisplay)
{
    pDisplay->fForcePresent = crServerVBoxCompositionPresentNeeded(&pDisplay->Mural);
    crServerVBoxCompositionDisableEnter(&pDisplay->Mural);
}

void CrDpLeave(PCR_DISPLAY pDisplay)
{
    pDisplay->Mural.fDataPresented = GL_TRUE;
    crServerVBoxCompositionDisableLeave(&pDisplay->Mural, pDisplay->fForcePresent);
}

int CrDemInit(PCR_DISPLAY_ENTRY_MAP pMap)
{
    pMap->pTextureMap = crAllocHashtable();
    if (pMap->pTextureMap)
        return VINF_SUCCESS;

    crWarning("crAllocHashtable failed!");
    return VERR_NO_MEMORY;
}

void crDemEntryRelease(PCR_DISPLAY_ENTRY_MAP pMap, PCR_DISPLAY_ENTRY pEntry, bool fForceDelete)
{
    CRTextureObj *pTobj = (CRTextureObj *)pEntry->pvUserData2;
    if (!pTobj)
    {
        crWarning("Trying to release entry that does not have tobj specified");
        return;
    }

    CR_STATE_SHAREDOBJ_USAGE_CLEAR(pTobj, cr_server.MainContextInfo.pContext);

    bool fDeleteEntry = fForceDelete;
    GLuint idTexture = pTobj->id;

    if (!CR_STATE_SHAREDOBJ_USAGE_IS_USED(pTobj))
    {
        CRSharedState *pShared = crStateGlobalSharedAcquire();

        CRASSERT(pShared);
        /* on the host side, we need to delete an ogl texture object here as well, which crStateDeleteTextureCallback will do
         * in addition to calling crStateDeleteTextureObject to delete a state object */
        crHashtableDelete(pShared->textureTable, idTexture, crStateDeleteTextureCallback);

        crStateGlobalSharedRelease();

        fDeleteEntry = true;
    }
    else
    {
        /* this is something we would not generally expect */
        CRASSERT(!fForceDelete);
    }

    if (fDeleteEntry)
    {
        if (pMap)
            crHashtableDelete(pMap->pTextureMap, idTexture, crFree);
        else
            crFree(pEntry); /* <- when called from crDemTermEntryCb */

        crStateGlobalSharedRelease();
    }
}

void crDemTermEntryCb(void *pvEntry)
{
    crDemEntryRelease(NULL, (PCR_DISPLAY_ENTRY)pvEntry, true);
}

void CrDemTerm(PCR_DISPLAY_ENTRY_MAP pMap)
{
    crFreeHashtable(pMap->pTextureMap, crDemTermEntryCb);
}

void CrDemEntryRelease(PCR_DISPLAY_ENTRY pEntry)
{
    PCR_DISPLAY_ENTRY_MAP pMap = (PCR_DISPLAY_ENTRY_MAP)pEntry->pvUserData1;
    Assert(pMap);
    crDemEntryRelease(pMap, pEntry, false);
}

PCR_DISPLAY_ENTRY CrDemEntryAcquire(PCR_DISPLAY_ENTRY_MAP pMap, GLuint idTexture)
{
    PCR_DISPLAY_ENTRY pEntry = (PCR_DISPLAY_ENTRY)crHashtableSearch(pMap->pTextureMap, idTexture);
    if (pEntry)
        return pEntry;

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

    pEntry = (PCR_DISPLAY_ENTRY)crAlloc(sizeof (*pEntry));
    if (!pEntry)
    {
        crWarning("crAlloc failed allocating CR_DISPLAY_ENTRY");
        crStateGlobalSharedRelease();
        return NULL;
    }

    CrDpEntryInit(pEntry, &TextureData, pMap, pTobj);

    /* just use main context info's context to hold the texture reference */
    CR_STATE_SHAREDOBJ_USAGE_SET(pTobj, cr_server.MainContextInfo.pContext);

    crHashtableAdd(pMap->pTextureMap, idTexture, pEntry);
    return pEntry;

}
#if 0
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
#endif
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

void CrHlpPutTexImage(CRContext *pCurCtx, PVBOXVR_TEXTURE pTexture, GLenum enmFormat, void *pvData)
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

void* CrHlpGetTexImage(CRContext *pCurCtx, PVBOXVR_TEXTURE pTexture, GLuint idPBO, GLenum enmFormat)
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
    PCR_DISPLAY pDisplay = crServerDisplayGet(idScreen);
    if (!pDisplay)
    {
        crWarning("crServerDisplayGet Failed");
        return;
    }

    PCR_DISPLAY_ENTRY pEntry = NULL;
    if (texture)
    {
        pEntry = CrDemEntryAcquire(&cr_server.PresentTexturepMap, texture);
        if (!pEntry)
        {
            crWarning("CrDemEntryAcquire Failed");
            return;
        }
    }

    CrDpEnter(pDisplay);

    if (!(cfg & CR_PRESENT_FLAG_CLEAR_RECTS))
    {
        RTPOINT Point = {xPos, yPos};
        int rc = CrDpEntryRegionsAdd(pDisplay, pEntry, &Point, (uint32_t)cRects, (const RTRECT*)pRects);
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
