/* $Id$ */

/** @file
 * VBox crOpenGL: Window to FBO redirect support.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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

#include "server.h"
#include "cr_string.h"
#include "cr_mem.h"
#include "render/renderspu.h"

static int crServerGetPointScreen(GLint x, GLint y)
{
    int i;

    for (i=0; i<cr_server.screenCount; ++i)
    {
        if ((x>=cr_server.screen[i].x && x<=cr_server.screen[i].x+(int)cr_server.screen[i].w)
           && (y>=cr_server.screen[i].y && y<=cr_server.screen[i].y+(int)cr_server.screen[i].h))
        {
            return i;
        }
    }

    return -1;
}

static GLboolean crServerMuralCoverScreen(CRMuralInfo *mural, int sId)
{
    return mural->gX<=cr_server.screen[sId].x
           && mural->gX>=cr_server.screen[sId].x+(int)cr_server.screen[sId].w
           && mural->gY<=cr_server.screen[sId].y
           && mural->gY>=cr_server.screen[sId].y+(int)cr_server.screen[sId].h;
}

void crServerCheckMuralGeometry(CRMuralInfo *mural)
{
    int tlS, brS, trS, blS;
    int overlappingScreenCount, primaryS, i;

    if (cr_server.screenCount<2)
    {
        CRASSERT(cr_server.screenCount>0);

        mural->hX = mural->gX-cr_server.screen[0].x;
        mural->hY = mural->gY-cr_server.screen[0].y;

        cr_server.head_spu->dispatch_table.WindowPosition(mural->spuWindow, mural->hX, mural->hY);

        return;
    }

    tlS = crServerGetPointScreen(mural->gX, mural->gY);
    brS = crServerGetPointScreen(mural->gX+mural->width, mural->gY+mural->height);

    if (tlS==brS && tlS>=0)
    {
        overlappingScreenCount = 1;
        primaryS = tlS;
    }
    else
    {
        trS = crServerGetPointScreen(mural->gX+mural->width, mural->gY);
        blS = crServerGetPointScreen(mural->gX, mural->gY+mural->height);

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

    if (overlappingScreenCount<2)
    {
        if (mural->bUseFBO)
        {
            crServerRedirMuralFBO(mural, GL_FALSE);
            crServerDeleteMuralFBO(mural);
        }

        cr_server.head_spu->dispatch_table.WindowPosition(mural->spuWindow, mural->hX, mural->hY);
    }
    else
    {
        if (!mural->bUseFBO)
        {
            crServerRedirMuralFBO(mural, GL_TRUE);
        }
        else
        {
            if (mural->width!=mural->fboWidth
                || mural->height!=mural->height)
            {
                crServerRedirMuralFBO(mural, GL_FALSE);
                crServerDeleteMuralFBO(mural);
                crServerRedirMuralFBO(mural, GL_TRUE);
            }
        }

        if (!mural->bUseFBO)
        {
            cr_server.head_spu->dispatch_table.WindowPosition(mural->spuWindow, mural->hX, mural->hY);
        }
    }
}

GLboolean crServerSupportRedirMuralFBO(void)
{
    const GLubyte* pExt = cr_server.head_spu->dispatch_table.GetString(GL_REAL_EXTENSIONS);

    return NULL!=crStrstr(pExt, "GL_ARB_framebuffer_object")
           && NULL!=crStrstr(pExt, "GL_ARB_texture_non_power_of_two");
}

void crServerRedirMuralFBO(CRMuralInfo *mural, GLboolean redir)
{
    if (redir)
    {
        if (!crServerSupportRedirMuralFBO())
        {
            crWarning("FBO not supported, can't redirect window output");
            return;
        }

        cr_server.head_spu->dispatch_table.WindowShow(mural->spuWindow, GL_FALSE);

        if (mural->idFBO==0)
        {
            crServerCreateMuralFBO(mural);
        }

        if (!crStateGetCurrent()->framebufferobject.drawFB)
        {
            cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, mural->idFBO);
        }
    }
    else
    {
        cr_server.head_spu->dispatch_table.WindowShow(mural->spuWindow, mural->bVisible);

        if (mural->bUseFBO && crServerSupportRedirMuralFBO()
            && !crStateGetCurrent()->framebufferobject.drawFB)
        {
            cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        }
    }

    mural->bUseFBO = redir;
}

void crServerCreateMuralFBO(CRMuralInfo *mural)
{
    CRContext *ctx = crStateGetCurrent();
    GLuint uid;
    GLenum status;

    CRASSERT(mural->idFBO==0);

    /*Color texture*/
    cr_server.head_spu->dispatch_table.GenTextures(1, &mural->idColorTex);
    cr_server.head_spu->dispatch_table.BindTexture(GL_TEXTURE_2D, mural->idColorTex);
    cr_server.head_spu->dispatch_table.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    cr_server.head_spu->dispatch_table.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    cr_server.head_spu->dispatch_table.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    cr_server.head_spu->dispatch_table.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    cr_server.head_spu->dispatch_table.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mural->width, mural->height,
                                                  0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

    /*Depth&Stencil*/
    cr_server.head_spu->dispatch_table.GenRenderbuffersEXT(1, &mural->idDepthStencilRB);
    cr_server.head_spu->dispatch_table.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, mural->idDepthStencilRB);
    cr_server.head_spu->dispatch_table.RenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT,
                                                              mural->width, mural->height);

    /*FBO*/
    cr_server.head_spu->dispatch_table.GenFramebuffersEXT(1, &mural->idFBO);
    cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, mural->idFBO);

    cr_server.head_spu->dispatch_table.FramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                               GL_TEXTURE_2D, mural->idColorTex, 0);
    cr_server.head_spu->dispatch_table.FramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                                                  GL_RENDERBUFFER_EXT, mural->idDepthStencilRB);
    cr_server.head_spu->dispatch_table.FramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT,
                                                                  GL_RENDERBUFFER_EXT, mural->idDepthStencilRB);

    status = cr_server.head_spu->dispatch_table.CheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status!=GL_FRAMEBUFFER_COMPLETE_EXT)
    {
        crWarning("FBO status(0x%x) isn't complete", status);
    }

    mural->fboWidth = mural->width;
    mural->fboHeight = mural->height;

    /*Restore gl state*/
    uid = ctx->texture.unit[ctx->texture.curTextureUnit].currentTexture2D->name;
    cr_server.head_spu->dispatch_table.BindTexture(GL_TEXTURE_2D, uid);

    uid = ctx->framebufferobject.renderbuffer ? ctx->framebufferobject.renderbuffer->hwid:0;
    cr_server.head_spu->dispatch_table.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, mural->idDepthStencilRB);

    uid = ctx->framebufferobject.drawFB ? ctx->framebufferobject.drawFB->hwid:0;
    cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, uid);
}

void crServerDeleteMuralFBO(CRMuralInfo *mural)
{
    CRASSERT(!mural->bUseFBO);

    if (mural->idFBO!=0)
    {
        cr_server.head_spu->dispatch_table.DeleteTextures(1, &mural->idColorTex);
        cr_server.head_spu->dispatch_table.DeleteRenderbuffersEXT(1, &mural->idDepthStencilRB);
        cr_server.head_spu->dispatch_table.DeleteFramebuffersEXT(1, &mural->idFBO);

        mural->idFBO = 0;
        mural->idColorTex = 0;
        mural->idDepthStencilRB = 0;
    }
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static GLboolean crServerIntersectScreen(CRMuralInfo *mural, int sId, CRrecti *rect)
{
    rect->x1 = MAX(mural->gX, cr_server.screen[sId].x);
    rect->x2 = MIN(mural->gX+(int)mural->fboWidth, cr_server.screen[sId].x+(int)cr_server.screen[sId].w);
    rect->y1 = MAX(mural->gY, cr_server.screen[sId].y);
    rect->y2 = MIN(mural->gY+(int)mural->fboHeight, cr_server.screen[sId].y+(int)cr_server.screen[sId].h);

    return (rect->x2>rect->x1) && (rect->y2>rect->y1);
}

void crServerPresentFBO(CRMuralInfo *mural)
{
    char *pixels, *tmppixels, *pSrc, *pDst;
    GLuint uid;
    int i, j, realrowsize, rowsize, rowstride, height;
    CRrecti rect;
    CRContext *ctx = crStateGetCurrent();

    CRASSERT(cr_server.pfnPresentFBO);

    pixels = crAlloc(4*mural->fboWidth*mural->fboHeight);
    if (!pixels)
    {
        crWarning("Out of memory in crServerPresentFBO");
        return;
    }

    cr_server.head_spu->dispatch_table.BindTexture(GL_TEXTURE_2D, mural->idColorTex);
    cr_server.head_spu->dispatch_table.GetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    uid = ctx->texture.unit[ctx->texture.curTextureUnit].currentTexture2D->name;
    cr_server.head_spu->dispatch_table.BindTexture(GL_TEXTURE_2D, uid);

    for (i=0; i<cr_server.screenCount; ++i)
    {
        if (crServerIntersectScreen(mural, i, &rect))
        {
            rowsize = 4*RT_ALIGN_Z(rect.x2-rect.x1, 4);
            realrowsize = 4*(rect.x2-rect.x1);
            tmppixels = crAlloc(rowsize*(rect.y2-rect.y1));
            
            if (!tmppixels)
            {
                crWarning("Out of memory in crServerPresentFBO");
                crFree(pixels);
                return;
            }
            rowstride = 4*mural->fboWidth;
            height = rect.y2-rect.y1;

            pSrc = pixels + 4*(rect.x1-mural->gX) + rowstride*(rect.y2-mural->gY-1);
            pDst = tmppixels;

            for (j=0; j<height; ++j)
            {
                crMemcpy(pDst, pSrc, realrowsize);

                pSrc -= rowstride;
                pDst += rowsize;
            }

            cr_server.pfnPresentFBO(tmppixels, i, rect.x1, rect.y1, rect.x2-rect.x1, height);

            crFree(tmppixels);
        }
    }
    crFree(pixels);
}

GLboolean crServerIsRedirectedToFBO()
{
    return cr_server.curClient
           && cr_server.curClient->currentMural
           && cr_server.curClient->currentMural->bUseFBO;
}
