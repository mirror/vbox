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
    int rc = CrVrScrCompositorInit(&pDisplay->Compositor);
    if (RT_SUCCESS(rc))
    {
        const GLint visBits = CR_RGB_BIT | CR_DOUBLE_BIT;
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
        CrVrScrCompositorTerm(&pDisplay->Compositor);
    }
    else
    {
        crWarning("CrVrScrCompositorInit failed, rc %d", rc);
    }
    CRASSERT(RT_FAILURE(rc));
    return rc;
}

void CrDpTerm(PCR_DISPLAY pDisplay)
{
    CrVrScrCompositorTerm(&pDisplay->Compositor);
    crServerMuralTerm(&pDisplay->Mural);
}

void CrDpResize(PCR_DISPLAY pDisplay, uint32_t width, uint32_t height,
        uint32_t stretchedWidth, uint32_t stretchedHeight)
{
    float StretchX, StretchY;
    StretchX = ((float)stretchedWidth)/width;
    StretchY = ((float)stretchedHeight)/height;
    crServerMuralSize(&pDisplay->Mural, stretchedWidth, stretchedHeight);
    CrVrScrCompositorLock(&pDisplay->Compositor);
    CrVrScrCompositorSetStretching(&pDisplay->Compositor, StretchX, StretchY);
    CrVrScrCompositorUnlock(&pDisplay->Compositor);
}

int CrDpEntryRegionsSet(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions)
{
    CrVrScrCompositorLock(&pDisplay->Compositor);
    int rc = CrVrScrCompositorEntryRegionsSet(&pDisplay->Compositor, &pEntry->CEntry, pPos, cRegions, paRegions);
    CrVrScrCompositorUnlock(&pDisplay->Compositor);
    return rc;
}

int CrDpEntryRegionsAdd(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions)
{
    CrVrScrCompositorLock(&pDisplay->Compositor);
    int rc = CrVrScrCompositorEntryRegionsAdd(&pDisplay->Compositor, &pEntry->CEntry, pPos, cRegions, paRegions);
    CrVrScrCompositorUnlock(&pDisplay->Compositor);
    return rc;
}

int CrDpPresentEntry(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry)
{
    GLuint idDrawFBO = 0, idReadFBO = 0;
    CRMuralInfo *pMural = cr_server.currentMural;
    CRContext *pCtx = cr_server.currentCtxInfo ? cr_server.currentCtxInfo->pContext : cr_server.MainContextInfo.pContext;

    if (pMural)
    {
        idDrawFBO = pMural->aidFBOs[pMural->iCurDrawBuffer];
        idReadFBO = pMural->aidFBOs[pMural->iCurReadBuffer];
    }

    crStateSwitchPrepare(NULL, pCtx, idDrawFBO, idReadFBO);

    cr_server.head_spu->dispatch_table.VBoxPresentComposition(pDisplay->Mural.spuWindow, &pDisplay->Compositor, &pEntry->CEntry);

    crStateSwitchPostprocess(pCtx, NULL, idDrawFBO, idReadFBO);

    return VINF_SUCCESS;
}

void CrDpEntryInit(PCR_DISPLAY_ENTRY pEntry, const PVBOXVR_TEXTURE pTextureData)
{
    CrVrScrCompositorEntryInit(&pEntry->CEntry, pTextureData);
}

void CrDpEntryCleanup(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry)
{
    CrVrScrCompositorLock(&pDisplay->Compositor);
    CrVrScrCompositorEntryRemove(&pDisplay->Compositor, &pEntry->CEntry);
    CrVrScrCompositorUnlock(&pDisplay->Compositor);
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

    VBOXVR_TEXTURE TextureData;
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

    /* the display (screen id == 0) can be initialized while doing crServerCheckInitDisplayBlitter,
     * so re-check the bit map */
     if (ASMBitTest(cr_server.DisplaysInitMap, idScreen))
         return &cr_server.aDispplays[idScreen];

     int rc = CrDpInit(&cr_server.aDispplays[idScreen]);
     if (RT_SUCCESS(rc))
     {
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

    return NULL;
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
crServerDispatchVBoxTexPresent(GLuint texture, GLuint cfg, GLint xPos, GLint yPos, GLint cRects, GLint *pRects)
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
