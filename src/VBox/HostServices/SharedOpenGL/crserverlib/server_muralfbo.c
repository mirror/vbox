/* $Id$ */

/** @file
 * VBox crOpenGL: Window to FBO redirect support.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "server.h"
#include "cr_string.h"
#include "cr_mem.h"
#include "cr_vreg.h"
#include "render/renderspu.h"

static int crServerGetPointScreen(GLint x, GLint y)
{
    int i;

    for (i=0; i<cr_server.screenCount; ++i)
    {
        if ((x>=cr_server.screen[i].x && x<cr_server.screen[i].x+(int)cr_server.screen[i].w)
           && (y>=cr_server.screen[i].y && y<cr_server.screen[i].y+(int)cr_server.screen[i].h))
        {
            return i;
        }
    }

    return -1;
}

static GLboolean crServerMuralCoverScreen(CRMuralInfo *mural, int sId)
{
    return mural->gX < cr_server.screen[sId].x
           && mural->gX+(int)mural->width > cr_server.screen[sId].x+(int)cr_server.screen[sId].w
           && mural->gY < cr_server.screen[sId].y
           && mural->gY+(int)mural->height > cr_server.screen[sId].y+(int)cr_server.screen[sId].h;
}

/* Called when a new CRMuralInfo is created
 * or when OutputRedirect status is changed.
 */
void crServerSetupOutputRedirect(CRMuralInfo *mural)
{
    /* Unset the previous redirect. */
    if (mural->pvOutputRedirectInstance)
    {
        cr_server.outputRedirect.CROREnd(mural->pvOutputRedirectInstance);
        mural->pvOutputRedirectInstance = NULL;
    }

    /* Setup a new redirect. */
    if (cr_server.bUseOutputRedirect)
    {
        /* Query supported formats. */
        uint32_t cbFormats = 4096;
        char *pachFormats = (char *)crAlloc(cbFormats);

        if (pachFormats)
        {
            int rc = cr_server.outputRedirect.CRORContextProperty(cr_server.outputRedirect.pvContext,
                                                                  0 /* H3DOR_PROP_FORMATS */, // @todo from a header
                                                                  pachFormats, cbFormats, &cbFormats);
            if (RT_SUCCESS(rc))
            {
                if (RTStrStr(pachFormats, "H3DOR_FMT_RGBA_TOPDOWN"))
                {
                    cr_server.outputRedirect.CRORBegin(cr_server.outputRedirect.pvContext,
                                                       &mural->pvOutputRedirectInstance,
                                                       "H3DOR_FMT_RGBA_TOPDOWN"); // @todo from a header
                }
            }

            crFree(pachFormats);
        }

        /* If this is not NULL then there was a supported format. */
        if (mural->pvOutputRedirectInstance)
        {
            uint32_t cRects;
            const RTRECT *pRects;

            int rc = CrVrScrCompositorRegionsGet(&mural->Compositor, &cRects, NULL, &pRects, NULL);
            if (!RT_SUCCESS(rc))
            {
                crWarning("CrVrScrCompositorEntryRegionsGet failed, rc %d", rc);
                return;
            }

            cr_server.outputRedirect.CRORGeometry(mural->pvOutputRedirectInstance,
                                                  mural->hX, mural->hY,
                                                  mural->width, mural->height);
            // @todo the code assumes that RTRECT == four of GLInts
            cr_server.outputRedirect.CRORVisibleRegion(mural->pvOutputRedirectInstance,
                                                       mural->cVisibleRects, (RTRECT *)mural->pVisibleRects);

            crServerPresentFBO(mural);
        }
    }
}

void crServerCheckMuralGeometry(CRMuralInfo *mural)
{
    int tlS, brS, trS, blS;
    int overlappingScreenCount = 0, primaryS = -1 , i;
    uint64_t winID = 0;
    GLuint fPresentMode;

    if (!mural->CreateInfo.externalID)
        return;

    CRASSERT(mural->spuWindow);
    CRASSERT(mural->spuWindow != CR_RENDER_DEFAULT_WINDOW_ID);

    crServerVBoxCompositionDisableEnter(mural);

    if (!mural->width || !mural->height)
    {
        crServerRedirMuralFBO(mural, CR_SERVER_REDIR_F_NONE);
        crServerDeleteMuralFBO(mural);
        crServerVBoxCompositionDisableLeave(mural, GL_FALSE);
        return;
    }

    tlS = crServerGetPointScreen(mural->gX, mural->gY);
    brS = crServerGetPointScreen(mural->gX+mural->width-1, mural->gY+mural->height-1);

    if ((tlS==brS && tlS>=0) || cr_server.screenCount <= 1)
    {
        if (cr_server.screenCount <= 1)
        {
            if (tlS != brS)
            {
                if (tlS >= 0)
                    brS = tlS;
                else
                    tlS = brS;
            }

            primaryS = 0;
        }
        else
        {
            Assert(brS == tlS);

            primaryS = brS;
        }


        Assert(brS == tlS);

        if (tlS>=0 && cr_server.screen[tlS].winID)
        {
            overlappingScreenCount = 1;
        }
    }
    else
    {
        bool fFoundWindIdScreen = false;
        trS = crServerGetPointScreen(mural->gX+mural->width-1, mural->gY);
        blS = crServerGetPointScreen(mural->gX, mural->gY+mural->height-1);

        primaryS = -1; overlappingScreenCount = 0;
        for (i=0; i<cr_server.screenCount; ++i)
        {
            if ((i==tlS) || (i==brS) || (i==trS) || (i==blS)
                || crServerMuralCoverScreen(mural, i))
            {
                if ((!fFoundWindIdScreen && cr_server.screen[i].winID) || primaryS<0)
                    primaryS = i;

                if (cr_server.screen[i].winID)
                {
                    overlappingScreenCount++;
                    fFoundWindIdScreen = true;
                }
            }
        }

        if (primaryS<0)
        {
            primaryS = 0;
        }
    }

    CRASSERT(primaryS >= 0);

    winID = overlappingScreenCount ? cr_server.screen[primaryS].winID : 0;

    if (!winID != !mural->fHasParentWindow
            || (winID && primaryS!=mural->screenId))
    {
        mural->fHasParentWindow = !!winID;

        renderspuSetWindowId(winID);
        renderspuReparentWindow(mural->spuWindow);
        renderspuSetWindowId(cr_server.screen[0].winID);
    }

    if (primaryS != mural->screenId)
    {
        /* mark it invisible on the old screen */
        crServerWindowSetIsVisible(mural, GL_FALSE);
        mural->screenId = primaryS;
        /* check if mural is visivle on the new screen, and mark it as such */
        crServerWindowCheckIsVisible(mural);
    }

    mural->hX = mural->gX-cr_server.screen[primaryS].x;
    mural->hY = mural->gY-cr_server.screen[primaryS].y;

    fPresentMode = cr_server.fPresentMode;

    if (!mural->fHasParentWindow)
        fPresentMode &= ~CR_SERVER_REDIR_F_DISPLAY;

    if (!overlappingScreenCount)
        fPresentMode &= ~CR_SERVER_REDIR_F_DISPLAY;
    else if (overlappingScreenCount > 1)
        fPresentMode = (fPresentMode | CR_SERVER_REDIR_F_FBO_RAM_VMFB | cr_server.fVramPresentModeDefault) & ~CR_SERVER_REDIR_F_DISPLAY;

    if (!mural->fUseDefaultDEntry)
    {
        /* only display matters */
        fPresentMode &= CR_SERVER_REDIR_F_DISPLAY;
    }

    fPresentMode = crServerRedirModeAdjust(fPresentMode);

    if (!(fPresentMode & CR_SERVER_REDIR_F_FBO))
    {
        crServerRedirMuralFBO(mural, fPresentMode);
        crServerDeleteMuralFBO(mural);
    }
    else
    {
        if (mural->fPresentMode & CR_SERVER_REDIR_F_FBO)
        {
            if (mural->width!=mural->fboWidth
                || mural->height!=mural->fboHeight)
            {
                crServerRedirMuralFBO(mural, fPresentMode & CR_SERVER_REDIR_F_DISPLAY);
                crServerDeleteMuralFBO(mural);
            }
        }

        crServerRedirMuralFBO(mural, fPresentMode);
    }

    if (mural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY)
    {
        CRScreenViewportInfo *pVieport = &cr_server.screenVieport[mural->screenId];

        cr_server.head_spu->dispatch_table.WindowPosition(mural->spuWindow, mural->hX - pVieport->x, mural->hY - pVieport->y);
    }

    if (mural->pvOutputRedirectInstance)
    {
        cr_server.outputRedirect.CRORGeometry(mural->pvOutputRedirectInstance,
                                              mural->hX, mural->hY,
                                              mural->width, mural->height);
    }

    crServerVBoxCompositionDisableLeave(mural, GL_FALSE);
}

GLboolean crServerSupportRedirMuralFBO(void)
{
    static GLboolean fInited = GL_FALSE;
    static GLboolean fSupported = GL_FALSE;
    if (!fInited)
    {
        const GLubyte* pExt = cr_server.head_spu->dispatch_table.GetString(GL_REAL_EXTENSIONS);

        fSupported = ( NULL!=crStrstr((const char*)pExt, "GL_ARB_framebuffer_object")
                 || NULL!=crStrstr((const char*)pExt, "GL_EXT_framebuffer_object"))
               && NULL!=crStrstr((const char*)pExt, "GL_ARB_texture_non_power_of_two");
        fInited = GL_TRUE;
    }
    return fSupported;
}

static void crServerPresentMuralVRAM(CRMuralInfo *mural, char *pixels);

#define CR_SERVER_MURAL_FROM_RPW_ENTRY(_pEntry) ((CRMuralInfo*)(((uint8_t*)(_pEntry)) - RT_OFFSETOF(CRMuralInfo, RpwEntry)))

static DECLCALLBACK(void) crServerMuralRpwDataCB(const struct CR_SERVER_RPW_ENTRY* pEntry, void *pvEntryTexData)
{
    CRMuralInfo *pMural = CR_SERVER_MURAL_FROM_RPW_ENTRY(pEntry);

    Assert(&pMural->RpwEntry == pEntry);

    crServerPresentMuralVRAM(pMural, pvEntryTexData);
}

static void crServerCreateMuralFBO(CRMuralInfo *mural);

static bool crServerEnableMuralRpw(CRMuralInfo *mural, GLboolean fEnable)
{
    if (!mural->CreateInfo.externalID)
    {
        crWarning("trying to change Rpw setting for internal mural %d", mural->spuWindow);
        return !fEnable;
    }

    if (fEnable)
    {
        if (!(mural->fPresentMode & CR_SERVER_REDIR_F_FBO_RPW))
        {
            int rc;
            if (!crServerRpwIsInitialized(&cr_server.RpwWorker))
            {
                rc = crServerRpwInit(&cr_server.RpwWorker);
                if (!RT_SUCCESS(rc))
                {
                    crWarning("crServerRpwInit failed rc %d", rc);
                    return false;
                }
            }

            CRASSERT(!mural->RpwEntry.Size.cx);
            CRASSERT(!mural->RpwEntry.Size.cy);

            if (!crServerRpwEntryIsInitialized(&mural->RpwEntry))
            {
                rc = crServerRpwEntryInit(&cr_server.RpwWorker, &mural->RpwEntry, mural->width, mural->height, crServerMuralRpwDataCB);
                if (!RT_SUCCESS(rc))
                {
                    crWarning("crServerRpwEntryInit failed rc %d", rc);
                    return false;
                }
            }
            else
            {
                rc = crServerRpwEntryResize(&cr_server.RpwWorker, &mural->RpwEntry, mural->width, mural->height);
                if (!RT_SUCCESS(rc))
                {
                    crWarning("crServerRpwEntryResize failed rc %d", rc);
                    return false;
                }
            }

            mural->fPresentMode |= CR_SERVER_REDIR_F_FBO_RPW;
        }
    }
    else
    {
        if ((mural->fPresentMode & CR_SERVER_REDIR_F_FBO_RPW))
        {
//            crServerRpwEntryCleanup(&cr_server.RpwWorker, &mural->RpwEntry);
            mural->fPresentMode &= ~CR_SERVER_REDIR_F_FBO_RPW;
        }
    }

    return true;
}

static void crServerEnableDisplayMuralFBO(CRMuralInfo *mural, GLboolean fEnable)
{
    if (!mural->CreateInfo.externalID)
    {
        crWarning("trying to change display setting for internal mural %d", mural->spuWindow);
        return;
    }

    if (fEnable)
    {
        if (!(mural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY))
        {
            mural->fPresentMode |= CR_SERVER_REDIR_F_DISPLAY;

            if  (mural->bVisible)
                crServerWindowShow(mural);
        }
    }
    else
    {
        if ((mural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY))
        {
            mural->fPresentMode &= ~CR_SERVER_REDIR_F_DISPLAY;

            if (mural->bVisible)
                crServerWindowShow(mural);
        }
    }
}

void crServerRedirMuralFBO(CRMuralInfo *mural, GLuint redir)
{
    if (mural->fPresentMode == redir)
    {
//        if (redir)
//            crWarning("crServerRedirMuralFBO called with the same redir status %d", redir);
        return;
    }

    if (!mural->CreateInfo.externalID)
    {
        crWarning("trying to change redir setting for internal mural %d", mural->spuWindow);
        return;
    }

    crServerVBoxCompositionDisableEnter(mural);

    if (redir & CR_SERVER_REDIR_F_FBO)
    {
        if (!crServerSupportRedirMuralFBO())
        {
            crWarning("FBO not supported, can't redirect window output");
            goto end;
        }

        if (mural->aidFBOs[0]==0)
        {
            crServerCreateMuralFBO(mural);
        }

        if (cr_server.curClient && cr_server.curClient->currentMural == mural)
        {
            if (!crStateGetCurrent()->framebufferobject.drawFB)
            {
                cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, CR_SERVER_FBO_FOR_IDX(mural, mural->iCurDrawBuffer));
            }
            if (!crStateGetCurrent()->framebufferobject.readFB)
            {
                cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_READ_FRAMEBUFFER, CR_SERVER_FBO_FOR_IDX(mural, mural->iCurReadBuffer));
            }

            crStateGetCurrent()->buffer.width = 0;
            crStateGetCurrent()->buffer.height = 0;
        }
    }
    else
    {
        if (cr_server.curClient && cr_server.curClient->currentMural == mural)
        {
            if (!crStateGetCurrent()->framebufferobject.drawFB)
            {
                cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
            }
            if (!crStateGetCurrent()->framebufferobject.readFB)
            {
                cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_READ_FRAMEBUFFER, 0);
            }

            crStateGetCurrent()->buffer.width = mural->width;
            crStateGetCurrent()->buffer.height = mural->height;
        }
    }

    crServerEnableMuralRpw(mural, !!(redir & CR_SERVER_REDIR_F_FBO_RPW));

    crServerEnableDisplayMuralFBO(mural, !!(redir & CR_SERVER_REDIR_F_DISPLAY));

    mural->fPresentMode = redir;

end:
    crServerVBoxCompositionDisableLeave(mural, GL_FALSE);
}

static void crServerCreateMuralFBO(CRMuralInfo *mural)
{
    CRContext *ctx = crStateGetCurrent();
    GLuint uid, i;
    GLenum status;
    SPUDispatchTable *gl = &cr_server.head_spu->dispatch_table;
    CRContextInfo *pMuralContextInfo;

    CRASSERT(mural->aidFBOs[0]==0);
    CRASSERT(mural->aidFBOs[1]==0);
    CRASSERT(mural->fUseDefaultDEntry);
    CRASSERT(mural->width == mural->DefaultDEntry.CEntry.Tex.width);
    CRASSERT(mural->height == mural->DefaultDEntry.CEntry.Tex.height);

    pMuralContextInfo = cr_server.currentCtxInfo;
    if (!pMuralContextInfo)
    {
        /* happens on saved state load */
        CRASSERT(cr_server.MainContextInfo.SpuContext);
        pMuralContextInfo = &cr_server.MainContextInfo;
        cr_server.head_spu->dispatch_table.MakeCurrent(mural->spuWindow, 0, cr_server.MainContextInfo.SpuContext);
    }

    if (pMuralContextInfo->CreateInfo.visualBits != mural->CreateInfo.visualBits)
    {
        crWarning("mural visual bits do not match with current context visual bits!");
    }

    mural->cBuffers = 2;
    mural->iBbBuffer = 0;
    /*Color texture*/

    if (crStateIsBufferBound(GL_PIXEL_UNPACK_BUFFER_ARB))
    {
        gl->BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
    }

    for (i = 0; i < mural->cBuffers; ++i)
    {
        gl->GenTextures(1, &mural->aidColorTexs[i]);
        gl->BindTexture(GL_TEXTURE_2D, mural->aidColorTexs[i]);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mural->width, mural->height,
                       0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    }

    /*Depth&Stencil*/
    gl->GenRenderbuffersEXT(1, &mural->idDepthStencilRB);
    gl->BindRenderbufferEXT(GL_RENDERBUFFER_EXT, mural->idDepthStencilRB);
    gl->RenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT,
                           mural->width, mural->height);

    /*FBO*/
    for (i = 0; i < mural->cBuffers; ++i)
    {
        gl->GenFramebuffersEXT(1, &mural->aidFBOs[i]);
        gl->BindFramebufferEXT(GL_FRAMEBUFFER_EXT, mural->aidFBOs[i]);

        gl->FramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                    GL_TEXTURE_2D, mural->aidColorTexs[i], 0);
        gl->FramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                       GL_RENDERBUFFER_EXT, mural->idDepthStencilRB);
        gl->FramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT,
                                       GL_RENDERBUFFER_EXT, mural->idDepthStencilRB);

        status = gl->CheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
        if (status!=GL_FRAMEBUFFER_COMPLETE_EXT)
        {
            crWarning("FBO status(0x%x) isn't complete", status);
        }
    }

    mural->fboWidth = mural->width;
    mural->fboHeight = mural->height;

    mural->iCurDrawBuffer = crServerMuralFBOIdxFromBufferName(mural, ctx->buffer.drawBuffer);
    mural->iCurReadBuffer = crServerMuralFBOIdxFromBufferName(mural, ctx->buffer.readBuffer);

    /*PBO*/
    if (cr_server.bUsePBOForReadback)
    {
        gl->GenBuffersARB(1, &mural->idPBO);
        gl->BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mural->idPBO);
        gl->BufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, mural->width*mural->height*4, 0, GL_STREAM_READ_ARB);
        gl->BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, ctx->bufferobject.packBuffer->hwid);

        if (!mural->idPBO)
        {
            crWarning("PBO create failed");
        }
    }

    /*Restore gl state*/
    uid = ctx->texture.unit[ctx->texture.curTextureUnit].currentTexture2D->hwid;
    gl->BindTexture(GL_TEXTURE_2D, uid);

    uid = ctx->framebufferobject.renderbuffer ? ctx->framebufferobject.renderbuffer->hwid:0;
    gl->BindRenderbufferEXT(GL_RENDERBUFFER_EXT, uid);

    uid = ctx->framebufferobject.drawFB ? ctx->framebufferobject.drawFB->hwid:0;
    gl->BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, uid);

    uid = ctx->framebufferobject.readFB ? ctx->framebufferobject.readFB->hwid:0;
    gl->BindFramebufferEXT(GL_READ_FRAMEBUFFER, uid);

    if (crStateIsBufferBound(GL_PIXEL_UNPACK_BUFFER_ARB))
    {
        gl->BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->bufferobject.unpackBuffer->hwid);
    }

    if (crStateIsBufferBound(GL_PIXEL_PACK_BUFFER_ARB))
    {
        gl->BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, ctx->bufferobject.packBuffer->hwid);
    }
    else
    {
        gl->BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
    }

    CRASSERT(mural->aidColorTexs[CR_SERVER_FBO_FB_IDX(mural)]);

    CrVrScrCompositorEntryTexNameUpdate(&mural->DefaultDEntry.CEntry, mural->aidColorTexs[CR_SERVER_FBO_FB_IDX(mural)]);

//    if (mural->fRootVrOn)
//        CrVrScrCompositorEntryTexNameUpdate(&mural->DefaultDEntry.RootVrCEntry, mural->aidColorTexs[CR_SERVER_FBO_FB_IDX(mural)]);
}

void crServerDeleteMuralFBO(CRMuralInfo *mural)
{
    CRASSERT(!(mural->fPresentMode & CR_SERVER_REDIR_F_FBO));

    if (mural->aidFBOs[0]!=0)
    {
        GLuint i;
        for (i = 0; i < mural->cBuffers; ++i)
        {
            cr_server.head_spu->dispatch_table.DeleteTextures(1, &mural->aidColorTexs[i]);
            mural->aidColorTexs[i] = 0;
        }

        cr_server.head_spu->dispatch_table.DeleteRenderbuffersEXT(1, &mural->idDepthStencilRB);
        mural->idDepthStencilRB = 0;

        for (i = 0; i < mural->cBuffers; ++i)
        {
            cr_server.head_spu->dispatch_table.DeleteFramebuffersEXT(1, &mural->aidFBOs[i]);
            mural->aidFBOs[i] = 0;
        }
    }

    if (mural->idPBO!=0)
    {
        CRASSERT(cr_server.bUsePBOForReadback);
        cr_server.head_spu->dispatch_table.DeleteBuffersARB(1, &mural->idPBO);
        mural->idPBO = 0;
    }

    mural->cBuffers = 0;

    if (crServerRpwEntryIsInitialized(&mural->RpwEntry))
        crServerRpwEntryCleanup(&cr_server.RpwWorker, &mural->RpwEntry);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static GLboolean crServerIntersectRect(CRrecti *a, CRrecti *b, CRrecti *rect)
{
    CRASSERT(a && b && rect);

    rect->x1 = MAX(a->x1, b->x1);
    rect->x2 = MIN(a->x2, b->x2);
    rect->y1 = MAX(a->y1, b->y1);
    rect->y2 = MIN(a->y2, b->y2);

    return (rect->x2>rect->x1) && (rect->y2>rect->y1);
}

static GLboolean crServerIntersectScreen(CRMuralInfo *mural, int sId, CRrecti *rect)
{
    rect->x1 = MAX(mural->gX, cr_server.screen[sId].x);
    rect->x2 = MIN(mural->gX+(int)mural->fboWidth, cr_server.screen[sId].x+(int)cr_server.screen[sId].w);
    rect->y1 = MAX(mural->gY, cr_server.screen[sId].y);
    rect->y2 = MIN(mural->gY+(int)mural->fboHeight, cr_server.screen[sId].y+(int)cr_server.screen[sId].h);

    return (rect->x2>rect->x1) && (rect->y2>rect->y1);
}

static void crServerCopySubImage(char *pDst, char* pSrc, CRrecti *pRect, int srcWidth, int srcHeight)
{
    int i;
    int dstrowsize = 4*(pRect->x2-pRect->x1);
    int srcrowsize = 4*srcWidth;
    int height = pRect->y2-pRect->y1;

    pSrc += 4*pRect->x1 + srcrowsize*(srcHeight-1-pRect->y1);

    for (i=0; i<height; ++i)
    {
        crMemcpy(pDst, pSrc, dstrowsize);

        pSrc -= srcrowsize;
        pDst += dstrowsize;
    }
}

static void crServerTransformRect(CRrecti *pDst, CRrecti *pSrc, int dx, int dy)
{
    pDst->x1 = pSrc->x1+dx;
    pDst->x2 = pSrc->x2+dx;
    pDst->y1 = pSrc->y1+dy;
    pDst->y2 = pSrc->y2+dy;
}

static void crServerVBoxCompositionPresentPerform(CRMuralInfo *mural)
{
    CRMuralInfo *currentMural = cr_server.currentMural;
    CRContextInfo *curCtxInfo = cr_server.currentCtxInfo;
    GLuint idDrawFBO, idReadFBO;
    CRContext *curCtx = curCtxInfo ? curCtxInfo->pContext : NULL;

    CRASSERT(curCtx == crStateGetCurrent());

    Assert((mural->fPresentMode & CR_SERVER_REDIR_F_FBO) || !mural->fUseDefaultDEntry);
    Assert(mural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY);

    mural->fDataPresented = GL_TRUE;

    if (currentMural)
    {
        idDrawFBO = CR_SERVER_FBO_FOR_IDX(currentMural, currentMural->iCurDrawBuffer);
        idReadFBO = CR_SERVER_FBO_FOR_IDX(currentMural, currentMural->iCurReadBuffer);
    }
    else
    {
        idDrawFBO = 0;
        idReadFBO = 0;
    }

    crStateSwitchPrepare(NULL, curCtx, idDrawFBO, idReadFBO);

    if (!mural->fRootVrOn)
        cr_server.head_spu->dispatch_table.VBoxPresentComposition(mural->spuWindow, &mural->Compositor, NULL);
    else
        cr_server.head_spu->dispatch_table.VBoxPresentComposition(mural->spuWindow, &mural->RootVrCompositor, NULL);

    crStateSwitchPostprocess(curCtx, NULL, idDrawFBO, idReadFBO);
}

void crServerVBoxCompositionPresent(CRMuralInfo *mural)
{
    if (!crServerVBoxCompositionPresentNeeded(mural))
        return;
    crServerVBoxCompositionPresentPerform(mural);
}

static void crServerVBoxCompositionReenable(CRMuralInfo *mural, GLboolean fForcePresent)
{
    if (!(mural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY)
            || (mural->fUseDefaultDEntry && !(mural->fPresentMode & CR_SERVER_REDIR_F_FBO))
            || !mural->fDataPresented
            || (!fForcePresent
                    && !crServerVBoxCompositionPresentNeeded(mural)))
        return;

    crServerVBoxCompositionPresentPerform(mural);
}

static void crServerVBoxCompositionDisable(CRMuralInfo *mural)
{
    if ((mural->fPresentMode & (CR_SERVER_REDIR_F_FBO | CR_SERVER_REDIR_F_DISPLAY)) != (CR_SERVER_REDIR_F_FBO | CR_SERVER_REDIR_F_DISPLAY)
            || !mural->fDataPresented)
        return;
    cr_server.head_spu->dispatch_table.VBoxPresentComposition(mural->spuWindow, NULL, NULL);
}

void crServerVBoxCompositionDisableEnter(CRMuralInfo *mural)
{
    ++mural->cDisabled;
    Assert(mural->cDisabled);
    if (mural->cDisabled == 1)
    {
        crServerVBoxCompositionDisable(mural);
    }
}

void crServerVBoxCompositionDisableLeave(CRMuralInfo *mural, GLboolean fForcePresentOnEnabled)
{
    mural->fForcePresentState = fForcePresentOnEnabled;
    --mural->cDisabled;
    Assert(mural->cDisabled < UINT32_MAX/2);
    if (!mural->cDisabled)
    {
        crServerVBoxCompositionReenable(mural, mural->fForcePresentState);
        mural->fForcePresentState = GL_FALSE;
    }
}

static void crServerVBoxCompositionSetEnableStateGlobalCB(unsigned long key, void *data1, void *data2)
{
    CRMuralInfo *mural = (CRMuralInfo *)data1;

    if (data2)
        crServerVBoxCompositionDisableLeave(mural, GL_FALSE);
    else
        crServerVBoxCompositionDisableEnter(mural);
}

DECLEXPORT(void) crServerVBoxCompositionSetEnableStateGlobal(GLboolean fEnable)
{
    crHashtableWalk(cr_server.muralTable, crServerVBoxCompositionSetEnableStateGlobalCB, (void*)(uintptr_t)fEnable);

    crHashtableWalk(cr_server.dummyMuralTable, crServerVBoxCompositionSetEnableStateGlobalCB, (void*)(uintptr_t)fEnable);
}

static void crServerPresentMuralVRAM(CRMuralInfo *mural, char *pixels)
{
    char *tmppixels;
    CRrecti rect, rectwr, sectr;
    int i, j;

    if (mural->fPresentMode & CR_SERVER_REDIR_F_FBO_RAM_VMFB)
    {
        for (i=0; i<cr_server.screenCount; ++i)
        {
            if (crServerIntersectScreen(mural, i, &rect))
            {
                /* rect in window relative coords */
                crServerTransformRect(&rectwr, &rect, -mural->gX, -mural->gY);

                if (!mural->pVisibleRects)
                {
                    /*we don't get any rects info for guest compiz windows, so we treat windows as visible unless explicitly received 0 visible rects*/
                    if (!mural->bReceivedRects)
                    {
                        tmppixels = crAlloc(4*(rect.x2-rect.x1)*(rect.y2-rect.y1));
                        if (!tmppixels)
                        {
                            crWarning("Out of memory in crServerPresentFBO");
                            crFree(pixels);
                            return;
                        }

                        crServerCopySubImage(tmppixels, pixels, &rectwr, mural->fboWidth, mural->fboHeight);
                        /*Note: pfnPresentFBO would free tmppixels*/
                        cr_server.pfnPresentFBO(tmppixels, i, rect.x1-cr_server.screen[i].x, rect.y1-cr_server.screen[i].y, rect.x2-rect.x1, rect.y2-rect.y1);
                    }
                }
                else
                {
                    for (j=0; j<mural->cVisibleRects; ++j)
                    {
                        if (crServerIntersectRect(&rectwr, (CRrecti*) &mural->pVisibleRects[4*j], &sectr))
                        {
                            tmppixels = crAlloc(4*(sectr.x2-sectr.x1)*(sectr.y2-sectr.y1));
                            if (!tmppixels)
                            {
                                crWarning("Out of memory in crServerPresentFBO");
                                crFree(pixels);
                                return;
                            }

                            crServerCopySubImage(tmppixels, pixels, &sectr, mural->fboWidth, mural->fboHeight);
                            /*Note: pfnPresentFBO would free tmppixels*/
                            cr_server.pfnPresentFBO(tmppixels, i,
                                                    sectr.x1+mural->gX-cr_server.screen[i].x,
                                                    sectr.y1+mural->gY-cr_server.screen[i].y,
                                                    sectr.x2-sectr.x1, sectr.y2-sectr.y1);
                        }
                    }
                }
            }
        }
    }

    if (mural->pvOutputRedirectInstance)
    {
        /* @todo find out why presentfbo is not called but crorframe is called. */
        cr_server.outputRedirect.CRORFrame(mural->pvOutputRedirectInstance,
                                           pixels,
                                           4 * mural->fboWidth * mural->fboHeight);
    }
}

void crServerPresentFBO(CRMuralInfo *mural)
{
    char *pixels=NULL;
    GLuint idPBO;
    CRContext *ctx = crStateGetCurrent();
    VBOXVR_TEXTURE Tex;

    CRASSERT(mural->fPresentMode & CR_SERVER_REDIR_F_FBO);
    CRASSERT(cr_server.pfnPresentFBO || (mural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY));

    if (!crServerVBoxCompositionPresentNeeded(mural))
        return;

    if (mural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY)
    {
        crServerVBoxCompositionPresentPerform(mural);
    }

    mural->fDataPresented = GL_TRUE;

    if (!(mural->fPresentMode & CR_SERVER_REDIR_FGROUP_REQUIRE_FBO_RAM))
        return;

    Tex.width = mural->width;
    Tex.height = mural->height;
    Tex.target = GL_TEXTURE_2D;
    Tex.hwid = mural->aidColorTexs[CR_SERVER_FBO_FB_IDX(mural)];
    CRASSERT(Tex.hwid);

    if (mural->fPresentMode & CR_SERVER_REDIR_F_FBO_RPW)
    {
        /* 1. blit to RPW entry draw texture */
        CRMuralInfo *pCurrentMural = cr_server.currentMural;
        CRContextInfo *pCurCtxInfo = cr_server.currentCtxInfo;
        PCR_BLITTER pBlitter = crServerVBoxBlitterGet();
        CRMuralInfo *pBlitterMural;
        CR_SERVER_CTX_SWITCH CtxSwitch;
        RTRECT Rect;
        VBOXVR_TEXTURE DstTex;
        CR_BLITTER_WINDOW BlitterBltInfo, CurrentBltInfo;
        CR_BLITTER_CONTEXT CtxBltInfo;
        int rc;

        Rect.xLeft = 0;
        Rect.yTop = 0;
        Rect.xRight = Tex.width;
        Rect.yBottom = Tex.height;

        if (pCurrentMural && pCurrentMural->CreateInfo.visualBits == CrBltGetVisBits(pBlitter))
        {
            pBlitterMural = pCurrentMural;
        }
        else
        {
            pBlitterMural = crServerGetDummyMural(pCurrentMural->CreateInfo.visualBits);
            if (!pBlitterMural)
            {
                crWarning("crServerGetDummyMural failed for blitter mural");
                return;
            }
        }

        crServerRpwEntryDrawSettingsToTex(&mural->RpwEntry, &DstTex);

        crServerCtxSwitchPrepare(&CtxSwitch, NULL);

        crServerVBoxBlitterWinInit(&CurrentBltInfo, pCurrentMural);
        crServerVBoxBlitterWinInit(&BlitterBltInfo, pBlitterMural);
        crServerVBoxBlitterCtxInit(&CtxBltInfo, pCurCtxInfo);

        CrBltMuralSetCurrent(pBlitter, &BlitterBltInfo);

        rc =  CrBltEnter(pBlitter, &CtxBltInfo, &CurrentBltInfo);
        if (RT_SUCCESS(rc))
        {
            CrBltBlitTexTex(pBlitter, &Tex, &Rect, &DstTex, &Rect, 1, 0);
            CrBltLeave(pBlitter);
        }
        else
        {
            crWarning("CrBltEnter failed rc %d", rc);
        }

        crServerCtxSwitchPostprocess(&CtxSwitch);

#if 1
        if (RT_SUCCESS(rc))
        {
            /* 2. submit RPW entry */
            rc =  crServerRpwEntrySubmit(&cr_server.RpwWorker, &mural->RpwEntry);
            if (!RT_SUCCESS(rc))
            {
                crWarning("crServerRpwEntrySubmit failed rc %d", rc);
            }
        }
#endif
        return;
    }

    if (cr_server.bUsePBOForReadback && !mural->idPBO)
    {
        crWarning("Mural doesn't have PBO even though bUsePBOForReadback is set!");
    }

    idPBO = cr_server.bUsePBOForReadback ? mural->idPBO : 0;
    if (idPBO)
    {
        CRASSERT(mural->fboWidth == mural->width);
        CRASSERT(mural->fboHeight == mural->height);
    }

    pixels = CrHlpGetTexImage(ctx, &Tex, idPBO, GL_BGRA);
    if (!pixels)
    {
        crWarning("CrHlpGetTexImage failed in crServerPresentFBO");
        return;
    }

    crServerPresentMuralVRAM(mural, pixels);

    CrHlpFreeTexImage(ctx, idPBO, pixels);
}

GLboolean crServerIsRedirectedToFBO()
{
#ifdef DEBUG_misha
    Assert(cr_server.curClient);
    if (cr_server.curClient)
    {
        Assert(cr_server.curClient->currentMural == cr_server.currentMural);
        Assert(cr_server.curClient->currentCtxInfo == cr_server.currentCtxInfo);
    }
#endif
    return cr_server.curClient
           && cr_server.curClient->currentMural
           && (cr_server.curClient->currentMural->fPresentMode & CR_SERVER_REDIR_F_FBO);
}

GLint crServerMuralFBOIdxFromBufferName(CRMuralInfo *mural, GLenum buffer)
{
    switch (buffer)
    {
        case GL_FRONT:
        case GL_FRONT_LEFT:
        case GL_FRONT_RIGHT:
            return CR_SERVER_FBO_FB_IDX(mural);
        case GL_BACK:
        case GL_BACK_LEFT:
        case GL_BACK_RIGHT:
            return CR_SERVER_FBO_BB_IDX(mural);
        case GL_NONE:
        case GL_AUX0:
        case GL_AUX1:
        case GL_AUX2:
        case GL_AUX3:
        case GL_LEFT:
        case GL_RIGHT:
        case GL_FRONT_AND_BACK:
            return -1;
        default:
            crWarning("crServerMuralFBOIdxFromBufferName: invalid buffer passed 0x%x", buffer);
            return -2;
    }
}

void crServerMuralFBOSwapBuffers(CRMuralInfo *mural)
{
    CRContext *ctx = crStateGetCurrent();
    GLuint iOldCurDrawBuffer = mural->iCurDrawBuffer;
    GLuint iOldCurReadBuffer = mural->iCurReadBuffer;
    mural->iBbBuffer = ((mural->iBbBuffer + 1) % (mural->cBuffers));
    if (mural->iCurDrawBuffer >= 0)
        mural->iCurDrawBuffer = ((mural->iCurDrawBuffer + 1) % (mural->cBuffers));
    if (mural->iCurReadBuffer >= 0)
        mural->iCurReadBuffer = ((mural->iCurReadBuffer + 1) % (mural->cBuffers));
    Assert(iOldCurDrawBuffer != mural->iCurDrawBuffer || mural->cBuffers == 1 || mural->iCurDrawBuffer < 0);
    Assert(iOldCurReadBuffer != mural->iCurReadBuffer || mural->cBuffers == 1 || mural->iCurReadBuffer < 0);
    if (!ctx->framebufferobject.drawFB && iOldCurDrawBuffer != mural->iCurDrawBuffer)
    {
        cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, CR_SERVER_FBO_FOR_IDX(mural, mural->iCurDrawBuffer));
    }
    if (!ctx->framebufferobject.readFB && iOldCurReadBuffer != mural->iCurReadBuffer)
    {
        cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_READ_FRAMEBUFFER, CR_SERVER_FBO_FOR_IDX(mural, mural->iCurReadBuffer));
    }
    Assert(mural->aidColorTexs[CR_SERVER_FBO_FB_IDX(mural)]);
    Assert(mural->fUseDefaultDEntry);
    CrVrScrCompositorEntryTexNameUpdate(&mural->DefaultDEntry.CEntry, mural->aidColorTexs[CR_SERVER_FBO_FB_IDX(mural)]);
    if (mural->fRootVrOn)
        CrVrScrCompositorEntryTexNameUpdate(&mural->DefaultDEntry.RootVrCEntry, mural->aidColorTexs[CR_SERVER_FBO_FB_IDX(mural)]);
}
