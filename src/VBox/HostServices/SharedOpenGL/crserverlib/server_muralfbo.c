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
            cr_server.outputRedirect.CRORGeometry(mural->pvOutputRedirectInstance,
                                                  mural->hX, mural->hY,
                                                  mural->width, mural->height);
            // @todo the code assumes that RTRECT == four of GLInts
            cr_server.outputRedirect.CRORVisibleRegion(mural->pvOutputRedirectInstance,
                                                       mural->cVisibleRects, (RTRECT *)mural->pVisibleRects);
        }
    }
}

void crServerCheckMuralGeometry(CRMuralInfo *mural)
{
    int tlS, brS, trS, blS;
    int overlappingScreenCount, primaryS, i;

    if (!mural->width || !mural->height)
        return;

    if (cr_server.screenCount<2 && !cr_server.bForceOffscreenRendering)
    {
        CRScreenViewportInfo *pVieport = &cr_server.screenVieport[mural->screenId];
        CRASSERT(cr_server.screenCount>0);
        CRASSERT(mural->fUseFBO == CR_SERVER_REDIR_NONE);

        mural->hX = mural->gX-cr_server.screen[0].x;
        mural->hY = mural->gY-cr_server.screen[0].y;

        cr_server.head_spu->dispatch_table.WindowPosition(mural->spuWindow, mural->hX - pVieport->x, mural->hY - pVieport->y);

        return;
    }

    tlS = crServerGetPointScreen(mural->gX, mural->gY);
    brS = crServerGetPointScreen(mural->gX+mural->width-1, mural->gY+mural->height-1);

    if (tlS==brS && tlS>=0)
    {
        overlappingScreenCount = 1;
        primaryS = tlS;
    }
    else
    {
        trS = crServerGetPointScreen(mural->gX+mural->width-1, mural->gY);
        blS = crServerGetPointScreen(mural->gX, mural->gY+mural->height-1);

        primaryS = -1; overlappingScreenCount = 0;
        for (i=0; i<cr_server.screenCount; ++i)
        {
            if ((i==tlS) || (i==brS) || (i==trS) || (i==blS)
                || crServerMuralCoverScreen(mural, i))
            {
                overlappingScreenCount++;
                primaryS = primaryS<0 ? i:primaryS;
            }
        }

        if (!overlappingScreenCount)
        {
            primaryS = 0;
        }
    }

    if (primaryS!=mural->screenId)
    {
        mural->screenId = primaryS;

        renderspuSetWindowId(cr_server.screen[primaryS].winID);
        renderspuReparentWindow(mural->spuWindow);
        renderspuSetWindowId(cr_server.screen[0].winID);
    }

    mural->hX = mural->gX-cr_server.screen[primaryS].x;
    mural->hY = mural->gY-cr_server.screen[primaryS].y;

    if (overlappingScreenCount<2 && !cr_server.bForceOffscreenRendering)
    {
        CRScreenViewportInfo *pVieport = &cr_server.screenVieport[mural->screenId];

        if (mural->fUseFBO)
        {
            crServerRedirMuralFBO(mural, CR_SERVER_REDIR_NONE);
            crServerDeleteMuralFBO(mural);
        }

        cr_server.head_spu->dispatch_table.WindowPosition(mural->spuWindow, mural->hX - pVieport->x, mural->hY - pVieport->y);
    }
    else
    {
        GLubyte redir = overlappingScreenCount >= 2 ? CR_SERVER_REDIR_FBO_RAM : cr_server.bForceOffscreenRendering;

        if (mural->spuWindow)
        {
            if (mural->fUseFBO)
            {
                if (mural->width!=mural->fboWidth
                    || mural->height!=mural->height)
                {
                    crServerRedirMuralFBO(mural, CR_SERVER_REDIR_NONE);
                    crServerDeleteMuralFBO(mural);
                }
            }

            if (mural->fUseFBO != redir)
            {
                crServerRedirMuralFBO(mural, redir);
            }
        }
#ifdef DEBUG_misha
        else
        {
            Assert(!mural->fUseFBO);
        }
#endif

        if (mural->fUseFBO != CR_SERVER_REDIR_FBO_RAM)
        {
            CRScreenViewportInfo *pVieport = &cr_server.screenVieport[mural->screenId];

            cr_server.head_spu->dispatch_table.WindowPosition(mural->spuWindow, mural->hX - pVieport->x, mural->hY - pVieport->y);
        }
    }

    if (mural->pvOutputRedirectInstance)
    {
        cr_server.outputRedirect.CRORGeometry(mural->pvOutputRedirectInstance,
                                              mural->hX, mural->hY,
                                              mural->width, mural->height);
    }
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

static void crServerCreateMuralFBO(CRMuralInfo *mural);

void crServerRedirMuralFBO(CRMuralInfo *mural, GLubyte redir)
{
    if (mural->fUseFBO == redir)
    {
        if (redir)
            crWarning("crServerRedirMuralFBO called with the same redir status %d", redir);
        return;
    }

    if (redir)
    {
        if (!crServerSupportRedirMuralFBO())
        {
            crWarning("FBO not supported, can't redirect window output");
            return;
        }

        if (redir == CR_SERVER_REDIR_FBO_RAM)
            cr_server.head_spu->dispatch_table.WindowShow(mural->spuWindow, GL_FALSE);
        else
            cr_server.head_spu->dispatch_table.WindowShow(mural->spuWindow, mural->bVisible);

        if (mural->aidFBOs[0]==0)
        {
            crServerCreateMuralFBO(mural);
        }

        if (!crStateGetCurrent()->framebufferobject.drawFB)
        {
            cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, mural->aidFBOs[mural->iCurDrawBuffer]);
        }
        if (!crStateGetCurrent()->framebufferobject.readFB)
        {
            cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_READ_FRAMEBUFFER, mural->aidFBOs[mural->iCurReadBuffer]);
        }

        if (cr_server.curClient && cr_server.curClient->currentMural == mural)
        {
            crStateGetCurrent()->buffer.width = 0;
            crStateGetCurrent()->buffer.height = 0;
        }
    }
    else
    {
        if (mural->fUseFBO == CR_SERVER_REDIR_FBO_RAM)
            cr_server.head_spu->dispatch_table.WindowShow(mural->spuWindow, mural->bVisible);

        if (mural->fUseFBO && crServerSupportRedirMuralFBO())
        {
            if (!crStateGetCurrent()->framebufferobject.drawFB)
            {
                cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
            }
            if (!crStateGetCurrent()->framebufferobject.readFB)
            {
                cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_READ_FRAMEBUFFER, 0);
            }
        }

        if (cr_server.curClient && cr_server.curClient->currentMural == mural)
        {
            crStateGetCurrent()->buffer.width = mural->width;
            crStateGetCurrent()->buffer.height = mural->height;
        }
    }

    mural->fUseFBO = redir;
}

static void crServerCreateMuralFBO(CRMuralInfo *mural)
{
    CRContext *ctx = crStateGetCurrent();
    GLuint uid, i;
    GLenum status;
    SPUDispatchTable *gl = &cr_server.head_spu->dispatch_table;
    CRContextInfo *pMuralContextInfo;
    int RestoreSpuWindow = -1;
    int RestoreSpuContext = -1;

    CRASSERT(mural->aidFBOs[0]==0);
    CRASSERT(mural->aidFBOs[1]==0);

    pMuralContextInfo = cr_server.currentCtxInfo;
    if (!pMuralContextInfo)
    {
        /* happens on saved state load */
        CRASSERT(cr_server.MainContextInfo.SpuContext);
        pMuralContextInfo = &cr_server.MainContextInfo;
        cr_server.head_spu->dispatch_table.MakeCurrent(mural->spuWindow, 0, cr_server.MainContextInfo.SpuContext);
        RestoreSpuWindow = 0;
        RestoreSpuContext = 0;
    }

    if (pMuralContextInfo->CreateInfo.visualBits != mural->CreateInfo.visualBits)
    {
        crWarning("mural visual bits do not match with current context visual bits!");
    }

    mural->cBuffers = 2;
    mural->iBbBuffer = 0;
    /*Color texture*/
    for (i = 0; i < mural->cBuffers; ++i)
    {
        gl->GenTextures(1, &mural->aidColorTexs[i]);
        gl->BindTexture(GL_TEXTURE_2D, mural->aidColorTexs[i]);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        if (crStateIsBufferBound(GL_PIXEL_UNPACK_BUFFER_ARB))
        {
            gl->BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
        }
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

    if (RestoreSpuWindow >= 0 && RestoreSpuContext >= 0)
    {
        cr_server.head_spu->dispatch_table.MakeCurrent(RestoreSpuWindow, 0, RestoreSpuContext);
    }
}

void crServerDeleteMuralFBO(CRMuralInfo *mural)
{
    CRASSERT(!mural->fUseFBO);

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

void crServerPresentFBO(CRMuralInfo *mural)
{
    char *pixels=NULL, *tmppixels;
    int i, j;
    CRrecti rect, rectwr, sectr;
    GLuint idPBO;
    CRContext *ctx = crStateGetCurrent();
    CR_BLITTER_TEXTURE Tex;

    CRASSERT(mural->fUseFBO);
    CRASSERT(cr_server.pfnPresentFBO || mural->fUseFBO == CR_SERVER_REDIR_FBO_BLT);

    if (!mural->bVisible)
    {
        return;
    }

    if (!mural->width || !mural->height)
    {
        return;
    }

    Tex.width = mural->width;
    Tex.height = mural->height;
    Tex.target = GL_TEXTURE_2D;
    Tex.hwid = mural->aidColorTexs[CR_SERVER_FBO_FB_IDX(mural)];
    CRASSERT(Tex.hwid);

    if (mural->fUseFBO == CR_SERVER_REDIR_FBO_BLT)
    {
        int rc;
        PCR_BLITTER pBlitter = crServerGetFBOPresentBlitter(mural);
        if (!pBlitter)
        {
            static int cPrintedWarnings = 0;
            if (++cPrintedWarnings <= 5)
            {
                crWarning("crServerGetFBOPresentBlitter returned no blitter %d", cPrintedWarnings);
                if (cPrintedWarnings == 5)
                    crWarning("won't print the above crServerGetFBOPresentBlitter warning any more", cPrintedWarnings);
            }
            return;
        }

        CrBltMuralSetCurrent(pBlitter, mural);
        rc = CrBltEnter(pBlitter, cr_server.currentCtxInfo, cr_server.currentMural);
        if (RT_SUCCESS(rc))
        {
            RTRECT Rect;
            Rect.xLeft = 0;
            Rect.yTop = 0;
            Rect.xRight = Tex.width;
            Rect.yBottom = Tex.height;

            CrBltBlitTexMural(pBlitter, &Tex, &Rect, &Rect, 1, 0);
            CrBltPresent(pBlitter);

            CrBltLeave(pBlitter);
        }
        else
        {
            crWarning("CrBltEnter failed rc %d", rc);
            return;
        }
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

    pixels = CrHlpGetTexImage(ctx, &Tex, idPBO);
    if (!pixels)
    {
        crWarning("CrHlpGetTexImage failed in crServerPresentFBO");
        return;
    }

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

    if (mural->pvOutputRedirectInstance)
    {
        /* @todo find out why presentfbo is not called but crorframe is called. */
        cr_server.outputRedirect.CRORFrame(mural->pvOutputRedirectInstance,
                                           pixels,
                                           4 * mural->fboWidth * mural->fboHeight);
    }

    CrHlpFreeTexImage(ctx, idPBO, pixels);
}

GLboolean crServerIsRedirectedToFBO()
{
    return cr_server.curClient
           && cr_server.curClient->currentMural
           && cr_server.curClient->currentMural->fUseFBO;
}

GLuint crServerMuralFBOIdxFromBufferName(CRMuralInfo *mural, GLenum buffer)
{
    if (buffer == GL_FRONT
            || buffer == GL_FRONT_LEFT
            || buffer == GL_FRONT_RIGHT)
        return CR_SERVER_FBO_FB_IDX(mural);
    return CR_SERVER_FBO_BB_IDX(mural);
}

void crServerMuralFBOSwapBuffers(CRMuralInfo *mural)
{
    CRContext *ctx = crStateGetCurrent();
    GLuint iOldCurDrawBuffer = mural->iCurDrawBuffer;
    GLuint iOldCurReadBuffer = mural->iCurReadBuffer;
    mural->iBbBuffer = ((mural->iBbBuffer + 1) % (mural->cBuffers));
    mural->iCurDrawBuffer = ((mural->iCurDrawBuffer + 1) % (mural->cBuffers));
    mural->iCurReadBuffer = ((mural->iCurReadBuffer + 1) % (mural->cBuffers));
    Assert(iOldCurDrawBuffer != mural->iCurDrawBuffer || mural->cBuffers == 1);
    Assert(iOldCurReadBuffer != mural->iCurReadBuffer || mural->cBuffers == 1);
    if (!ctx->framebufferobject.drawFB && iOldCurDrawBuffer != mural->iCurDrawBuffer)
    {
        cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, mural->aidFBOs[mural->iCurDrawBuffer]);
    }
    if (!ctx->framebufferobject.readFB && iOldCurReadBuffer != mural->iCurReadBuffer)
    {
        cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_READ_FRAMEBUFFER, mural->aidFBOs[mural->iCurReadBuffer]);
    }
}
