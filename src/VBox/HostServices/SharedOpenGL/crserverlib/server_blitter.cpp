/* $Id$ */

/** @file
 * Blitter API
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

#include <iprt/cdefs.h>
#include <iprt/types.h>

typedef struct CR_BLITTER_STATE
{
    GLint drawFB;
    GLint readFB;
} CR_BLITTER_STATE, *PCR_BLITTER_STATE;

typedef struct CR_BLITTER_BUFFER
{
    GLuint cbBuffer;
    GLvoid * pvBuffer;
} CR_BLITTER_BUFFER, *PCR_BLITTER_BUFFER;

typedef struct CR_BLITTER_TEXTURE
{
    GLint width;
    GLint height;
    GLenum target;
    GLuint hwid;
} CR_BLITTER_TEXTURE, *PCR_BLITTER_TEXTURE;

typedef DECLCALLBACK(int) FNCRBLT_BLITTER(struct CR_BLITTER *pBlitter, CR_BLITTER_TEXTURE *pSrc, RECT *paSrcRect, PRTRECTSIZE pDstSize, RECT *paDstRect, uint32_t cRects, uint32_t fFlags);
typedef FNCRBLT_BLITTER *PFNCRBLT_BLITTER;

typedef union CR_BLITTER_FLAGS
{
    struct
    {
        uint32_t Initialized     : 1;
        uint32_t SupportsFBO     : 1;
        uint32_t SupportsFBOBlit : 1;
        uint32_t Reserved        : 29;
    };
    uint32_t Value;
} CR_BLITTER_FLAGS, *PCR_BLITTER_FLAGS;

typedef struct CR_BLITTER
{
    GLint idFBO;
    CR_BLITTER_FLAGS Flags;
    PFNCRBLT_BLITTER pfnBlt;
    CR_BLITTER_BUFFER Verticies;
    CR_BLITTER_BUFFER Indicies;
    RTRECTSIZE CurrentSetSize;
    CRMuralInfo *pCurrentMural;
    CRContextInfo CtxInfo;
    CRContextInfo *pRestoreCtxInfo;
    CRMuralInfo *pRestoreMural;
} CR_BLITTER, *PCR_BLITTER;

int crBltInit(PCR_BLITTER pBlitter, CRMuralInfo *pCurrentMural)
{
    memset(pBlitter, 0, sizeof (*pBlitter));

    if (!cr_server.MainContextInfo.SpuContext)
    {
        crWarning("Default share context not initialized!");
        return VERR_INVALID_STATE;
    }


    pBlitter->CtxInfo.CreateInfo.pszDpyName = "";
    pBlitter->CtxInfo.CreateInfo.visualBits = CR_RGB_BIT | CR_DOUBLE_BIT;
    pBlitter->CtxInfo.SpuContext = cr_server.head_spu->dispatch_table.CreateContext(pBlitter->CtxInfo.CreateInfo.pszDpyName,
                                        pBlitter->CtxInfo.CreateInfo.visualBits,
                                        cr_server.MainContextInfo.SpuContext);
    if (!pBlitter->CtxInfo.SpuContext)
    {
        crWarning("CreateContext failed!");
        return VERR_GENERAL_FAILURE;
    }

    return VINF_SUCCESS;
}

void crBltTerm(PCR_BLITTER pBlitter)
{
    cr_server.head_spu->dispatch_table.DestroyContext(pBlitter->CtxInfo.SpuContext);
}

static DECLINLINE(GLboolean) crBltSupportsTexTex(PCR_BLITTER pBlitter)
{
    return pBlitter->Flags.SupportsFBO;
}

DECLINLINE(GLboolean) crBltIsEntered(PCR_BLITTER pBlitter)
{
    return !!pBlitter->pRestoreCtxInfo;
}

void crBltMuralSetCurrent(PCR_BLITTER pBlitter, CRMuralInfo *pMural)
{
    if (pBlitter->pCurrentMural == pMural)
        return;

    pBlitter->pCurrentMural = pMural;

    if (!crBltIsEntered(pBlitter))
        return;

    if (pMural)
        cr_server.head_spu->dispatch_table.MakeCurrent(pMural->spuWindow, 0, pBlitter->CtxInfo.SpuContext);
    else
        cr_server.head_spu->dispatch_table.MakeCurrent(0, 0, 0);
}

#define CRBLT_F_LINEAR 0x00000001

#define CRBLT_FILTER_FROM_FLAGS(_f) (((_f) & CRBLT_F_LINEAR) ? GL_LINEAR : GL_NEAREST)

static DECLCALLBACK(int) crBltBlitTexBufImplFbo(PCR_BLITTER pBlitter, CR_BLITTER_TEXTURE *pSrc, RECT *paSrcRect, PRTRECTSIZE pDstSize, RECT *paDstRect, uint32_t cRects, uint32_t fFlags)
{
    GLenum filter = CRBLT_FILTER_FROM_FLAGS(fFlags);
    cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_READ_FRAMEBUFFER, pBlitter->idFBO);
    cr_server.head_spu->dispatch_table.FramebufferTexture2DEXT(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pSrc->target, pSrc->hwid, 0);
    cr_server.head_spu->dispatch_table.ReadBuffer(GL_COLOR_ATTACHMENT0);

    for (UINT i = 0; i < cRects; ++i)
    {
        RECT * pSrcRect = &paSrcRect[i];
        RECT * pDstRect = &paDstRect[i];
        cr_server.head_spu->dispatch_table.BlitFramebufferEXT(
                pSrcRect->left, pSrcRect->top, pSrcRect->right, pSrcRect->bottom,
                pDstRect->left, pDstRect->top, pDstRect->right, pDstRect->bottom,
                GL_COLOR_BUFFER_BIT, filter);
    }

    return VINF_SUCCESS;
}

/* GL_TRIANGLE_FAN */
static DECLINLINE(GLfloat*) crBltVtRectTFNormalized(RECT *pRect, uint32_t normalX, uint32_t normalY, GLfloat* pBuff)
{
    /* left top */
    pBuff[0] = ((float)pRect->left)/((float)normalX);
    pBuff[1] = ((float)pRect->top)/((float)normalY);

    /* left bottom */
    pBuff[2] = pBuff[0];
    pBuff[3] = ((float)pRect->bottom)/((float)normalY);

    /* right bottom */
    pBuff[4] = ((float)pRect->right)/((float)normalX);
    pBuff[5] = pBuff[3];

    /* right top */
    pBuff[6] = pBuff[4];
    pBuff[7] = pBuff[1];
    return &pBuff[8];
}

static DECLINLINE(GLint*) crBltVtRectTF(RECT *pRect, uint32_t normalX, uint32_t normalY, GLint* pBuff)
{
    /* left top */
    pBuff[0] = pRect->left;
    pBuff[1] = pRect->top;

    /* left bottom */
    pBuff[2] = pBuff[0];
    pBuff[3] = pRect->bottom;

    /* right bottom */
    pBuff[4] = pRect->right;
    pBuff[5] = pBuff[3];

    /* right top */
    pBuff[6] = pBuff[4];
    pBuff[7] = pBuff[1];
    return &pBuff[8];
}

static DECLINLINE(GLubyte*) crBltVtFillRectIndicies(GLubyte *pIndex, GLubyte *piBase)
{
    GLubyte iBase = *piBase;
    /* triangle 1 */
    pIndex[0] = iBase;
    pIndex[1] = iBase + 1;
    pIndex[2] = iBase + 2;

    /* triangle 2 */
    pIndex[3] = iBase;
    pIndex[4] = iBase + 2;
    pIndex[5] = iBase + 3;
    *piBase = iBase + 6;
    return pIndex + 6;
}

/* Indexed GL_TRIANGLES */
static DECLINLINE(GLfloat*) crBltVtRectITNormalized(RECT *pRect, uint32_t normalX, uint32_t normalY, GLfloat* pBuff, GLubyte **ppIndex, GLubyte *piBase)
{
    GLfloat* ret = crBltVtRectTFNormalized(pRect, normalX, normalY, pBuff);

    if (ppIndex)
        *ppIndex = crBltVtFillRectIndicies(*ppIndex, *piBase);

    return ret;
}

static DECLINLINE(GLint*) crBltVtRectIT(RECT *pRect, uint32_t normalX, uint32_t normalY, GLint* pBuff, GLubyte **ppIndex, GLubyte *piBase)
{
    GLint* ret = crBltVtRectTF(pRect, normalX, normalY, pBuff);

    if (ppIndex)
        *ppIndex = crBltVtFillRectIndicies(*ppIndex, *piBase);

    return ret;
}

static DECLINLINE(GLuint) crBltVtGetNumVerticiesTF(GLuint cRects)
{
    return cRects * 4;
}

#define crBltVtGetNumVerticiesIT crBltVtGetNumVerticiesTF

static DECLINLINE(GLuint) crBltVtGetNumIndiciesIT(GLuint cRects)
{
    return 6 * cRects;
}


static GLfloat* crBltVtRectsITNormalized(RECT *paRects, uint32_t cRects, uint32_t normalX, uint32_t normalY, GLfloat* pBuff, GLubyte **ppIndex, GLubyte *piBase)
{
    for (uint32_t i = 0; i < cRects; ++i)
    {
        pBuff = crBltVtRectITNormalized(*paRects[i], normalX, normalY, pBuff, ppIndex, piBase);
    }
    return pBuff;
}

static void* crBltBufGet(PCR_BLITTER_BUFFER pBuffer, GLuint cbBuffer)
{
    if (pBuffer->cbBuffer < cbBuffer)
    {
        if (pBuffer->pvBuffer)
        {
            RTMemFree(pBuffer->pvBuffer);
        }

        cbBuffer += 16;

        pBuffer->pvBuffer = RTMemAlloc(cbBuffer);
        if (pBuffer->pvBuffer)
            pBuffer->cbBuffer = cbBuffer;
        else
        {
            crWarning("failed to allocate buffer of size %d", cbBuffer);
            pBuffer->cbBuffer = 0;
        }
    }
    return pBuffer->pvBuffer;
}

static DECLCALLBACK(int) crBltBlitTexBufImplDraw2D(PCR_BLITTER pBlitter, CR_BLITTER_TEXTURE *pSrc, RECT *paSrcRect, PRTRECTSIZE pDstSize, RECT *paDstRect, uint32_t cRects, uint32_t fFlags)
{
    GLuint normalX, normalY;

    if (pBlitter->CurrentSetSize.cx != pDstSize->cx
            || pBlitter->CurrentSetSize.cy != pDstSize->cy)
    {
        const GLdouble aProjection[] =
        {
                2.0 / pDstSize->cx,               0.0,  0.0, 0.0,
                              0.0, 2.0 / pDstSize->cy,  0.0, 0.0,
                              0.0,               0.0,  2.0, 0.0,
                             -1.0,              -1.0, -1.0, 1.0
        };
        cr_server.head_spu->dispatch_table.MatrixMode(GL_PROJECTION);
        cr_server.head_spu->dispatch_table.LoadMatrixd(aProjection);
        cr_server.head_spu->dispatch_table.Viewport(0, 0, pDstSize->cx, pDstSize->cy);
        pBlitter->CurrentSetSize.cx = pDstSize->cx;
        pBlitter->CurrentSetSize.cy = pDstSize->cy;
    }

    switch (pSrc->target)
    {
        case GL_TEXTURE_2D:
        {
            normalX = pSrc->width;
            normalY = pSrc->height;
            break;
        }

        case GL_TEXTURE_RECTANGLE_ARB:
        {
            normalX = 1;
            normalY = 1;
            break;
        }

        default:
        {
            crWarning("Unsupported texture target 0x%x", pSrc->target);
            return;
        }
    }

    CRASSERT(pSrc->hwid);

    cr_server.head_spu->dispatch_table.BindTexture(pSrc->target, pSrc->hwid);

    if (cRects == 1)
    {
        /* just optimizatino to draw a single rect with GL_TRIANGLE_FAN */
        bool bUseSameVerticies = paSrcRect == paDstRect && normalX == 1 && normalY == 1;
        GLfloat *pVerticies;
        GLfloat *pTexCoords;
        GLuint cElements = crBltVtGetNumVerticiesTF(1);
        if (bUseSameVerticies)
        {
            pVerticies = (GLfloat*)crBltBufGet(&pBlitter->Verticies, cElements * 2 * sizeof (*pVerticies));
            crBltVtRectTFNormalized(paDstRect, normalX, normalY, pVerticies);
            pTexCoords = pVerticies;
        }
        else
        {
            pVerticies = (GLfloat*)crBltBufGet(&pBlitter->Verticies, cElements * 2 * 2 * sizeof (*pVerticies));
            pTexCoords = crBltVtRectTFNormalized(paDstRect, normalX, normalY, pVerticies);
            crBltVtRectTFNormalized(paSrcRect, normalX, normalY, pTexCoords);
        }

        cr_server.head_spu->dispatch_table.EnableClientState(GL_VERTEX_ARRAY);
        cr_server.head_spu->dispatch_table.VertexPointer(2, GL_FLOAT, 0, pVerticies);

        cr_server.head_spu->dispatch_table.EnableClientState(GL_TEXTURE_COORD_ARRAY);
        cr_server.head_spu->dispatch_table.VertexPointer(2, GL_FLOAT, 0, pTexCoords);

        cr_server.head_spu->dispatch_table.Enable(pSrc->target);

        cr_server.head_spu->dispatch_table.DrawArrays(GL_TRIANGLE_FAN, 0, cElements);

        cr_server.head_spu->dispatch_table.Disable(pSrc->target);

        cr_server.head_spu->dispatch_table.DisableClientState(GL_TEXTURE_COORD_ARRAY);
        cr_server.head_spu->dispatch_table.DisableClientState(GL_VERTEX_ARRAY);
    }
    else
    {
        bool bUseSameVerticies = paSrcRect == paDstRect && normalX == 1 && normalY == 1;
        GLfloat *pVerticies;
        GLfloat *pTexCoords;
        GLubyte *pIndicies;
        GLuint cElements = crBltVtGetNumVerticiesIT(cRects);
        GLuint cIndicies = crBltVtGetNumIndiciesIT(cRects);
        GLuint iIdxBase = 0;
        if (bUseSameVerticies)
        {
            pVerticies = (GLfloat*)crBltBufGet(&pBlitter->Verticies, cElements * 2 * sizeof (*pVerticies));
            crBltVtRectsITNormalized(paDstRect, cRects, normalX, normalY, pVerticies, &pIndicies, &iIdxBase);
            pTexCoords = pVerticies;
        }
        else
        {
            pVerticies = (GLfloat*)crBltBufGet(&pBlitter->Verticies, cElements * 2 * 2 * sizeof (*pVerticies));
            pTexCoords = crBltVtRectsITNormalized(paDstRect, cRects, normalX, normalY, pVerticies, &pIndicies, &iIdxBase);
            crBltVtRectsITNormalized(paSrcRect, cRects, normalX, normalY, pTexCoords, NULL, NULL);
        }

        cr_server.head_spu->dispatch_table.EnableClientState(GL_VERTEX_ARRAY);
        cr_server.head_spu->dispatch_table.VertexPointer(2, GL_FLOAT, 0, pVerticies);

        cr_server.head_spu->dispatch_table.EnableClientState(GL_TEXTURE_COORD_ARRAY);
        cr_server.head_spu->dispatch_table.VertexPointer(2, GL_FLOAT, 0, pTexCoords);

        cr_server.head_spu->dispatch_table.Enable(pSrc->target);

        cr_server.head_spu->dispatch_table.DrawElements(GL_TRIANGLES, cIndicies, GL_UNSIGNED_BYTE, pIndicies);

        cr_server.head_spu->dispatch_table.Disable(pSrc->target);

        cr_server.head_spu->dispatch_table.DisableClientState(GL_TEXTURE_COORD_ARRAY);
        cr_server.head_spu->dispatch_table.DisableClientState(GL_VERTEX_ARRAY);
    }
}

int crBltInitOnMakeCurent(PCR_BLITTER pBlitter)
{
    const char * pszExtension = cr_server.head_spu->dispatch_table.GetString(GL_EXTENSIONS);
    if (crStrstr(pszExtension, "GL_EXT_framebuffer_object"))
    {
        pBlitter->Flags.SupportsFBO = 1;
        cr_server.head_spu->dispatch_table.GenFramebuffersEXT(1, &pBlitter->idFBO);
        CRASSERT(pBlitter->idFBO);
    }
    else
        crWarning("GL_EXT_framebuffer_object not supported, blitter can only blit to window");

    if (crStrstr(pszExtension, "GL_EXT_framebuffer_blit"))
    {
        pBlitter->Flags.SupportsFBOBlit = 1;
        pBlitter->pfnBlt = crBltBlitTexBufImplFbo;
    }
    else
    {
        crWarning("GL_EXT_framebuffer_blit not supported, will use Draw functions for blitting, which might be less efficient");
        pBlitter->pfnBlt = crBltBlitTexBufImplDraw2D;
    }

    return VINF_SUCCESS;
}

void crBltLeave(PCR_BLITTER pBlitter)
{
    CRASSERT(crBltIsEntered(pBlitter));

    if (pBlitter->pRestoreCtxInfo != &pBlitter->CtxInfo)
        cr_server.head_spu->dispatch_table.MakeCurrent(pBlitter->pRestoreMural->spuWindow, 0, pBlitter->pRestoreCtxInfo->SpuContext);
    else
        cr_server.head_spu->dispatch_table.MakeCurrent(0, 0, 0);

    pBlitter->pRestoreCtxInfo = NULL;
}

int crBltEnter(PCR_BLITTER pBlitter, CRContextInfo *pRestoreCtxInfo, CRMuralInfo *pRestoreMural)
{
    if (!pBlitter->pCurrentMural)
    {
        crWarning("current mural not initialized!");
        return VERR_INVALID_STATE;
    }

    if (crBltIsEntered(pBlitter))
    {
        crWarning("blitter is entered already!");
        return VERR_INVALID_STATE;
    }

    pBlitter->pRestoreCtxInfo = pRestoreCtxInfo ? pRestoreCtxInfo : &pBlitter->CtxInfo;
    pBlitter->pRestoreMural = pRestoreMural;

    cr_server.head_spu->dispatch_table.MakeCurrent(pBlitter->pCurrentMural->spuWindow, 0, pBlitter->CtxInfo.SpuContext);

    if (pBlitter->Flags.Initialized)
        return VINF_SUCCESS;

    int rc = crBltInitOnMakeCurent(pBlitter);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    crWarning("crBltInitOnMakeCurent failed, rc %d", rc);
    crBltLeave(pBlitter);
    return rc;
}

static void crBltBlitTexBuf(PCR_BLITTER pBlitter, CR_BLITTER_TEXTURE *pSrc, RECT *paSrcRects, GLenum enmDstBuff, PRTRECTSIZE pDstSize, RECT *paDstRects, uint32_t cRects, uint32_t fFlags)
{
    cr_server.head_spu->dispatch_table.DrawBuffer(enmDstBuff);

    pBlitter->pfnBlt(pBlitter, pSrc, paSrcRects, pDstSize, paDstRects, cRects, fFlags);
}

void crBltBlitTexMural(PCR_BLITTER pBlitter, CR_BLITTER_TEXTURE *pSrc, RECT *paSrcRects, RECT *paDstRects, uint32_t cRects, uint32_t fFlags)
{
    RTRECTSIZE DstSize = {pBlitter->pCurrentMural->width, pBlitter->pCurrentMural->height};

    crBltBlitTexBuf(pBlitter, pSrc, paSrcRects, GL_BACK, &DstSize, paDstRects, cRects, fFlags);
}

void crBltBlitTexTex(PCR_BLITTER pBlitter, CR_BLITTER_TEXTURE *pSrc, RECT *pSrcRect, CR_BLITTER_TEXTURE *pDst, RECT *pDstRect, uint32_t cRects, uint32_t fFlags)
{
    RTRECTSIZE DstSize = {pDst->width, pDst->height};

    cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, pBlitter->idFBO);

    /* TODO: mag/min filters ? */

    cr_server.head_spu->dispatch_table.FramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pDst->target, pDst->hwid, 0);

//    cr_server.head_spu->dispatch_table.FramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
//    cr_server.head_spu->dispatch_table.FramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

    crBltBlitTexBuf(pBlitter, pSrc, pSrcRect, GL_DRAW_FRAMEBUFFER, DstSize, pDstRect, cRects, fFlags);
}
