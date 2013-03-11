/* $Id$ */

/** @file
 * Blitter API implementation
 */
/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "cr_blitter.h"
#include "cr_spu.h"
#include "chromium.h"
#include "cr_error.h"
#include "cr_net.h"
#include "cr_rand.h"
#include "cr_mem.h"
#include "cr_string.h"

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/mem.h>


int CrBltInit(PCR_BLITTER pBlitter, CR_BLITTER_CONTEXT *pCtxBase, bool fCreateNewCtx, SPUDispatchTable *pDispatch)
{
    memset(pBlitter, 0, sizeof (*pBlitter));

    pBlitter->pDispatch = pDispatch;

    if (pCtxBase->Base.id == 0 || pCtxBase->Base.id < -1)
    {
        crWarning("Default share context not initialized!");
        return VERR_INVALID_PARAMETER;
    }

    pBlitter->CtxInfo = *pCtxBase;
    if (fCreateNewCtx)
    {
        pBlitter->CtxInfo.Base.id = pDispatch->CreateContext("", pCtxBase->Base.visualBits, pCtxBase->Base.id);
        if (!pBlitter->CtxInfo.Base.id)
        {
            crWarning("CreateContext failed!");
            return VERR_GENERAL_FAILURE;
        }
        pBlitter->Flags.CtxCreated = 1;
    }

    return VINF_SUCCESS;
}

void CrBltTerm(PCR_BLITTER pBlitter)
{
    if (pBlitter->Flags.CtxCreated)
        pBlitter->pDispatch->DestroyContext(pBlitter->CtxInfo.Base.id);
}

void CrBltMuralSetCurrent(PCR_BLITTER pBlitter, CR_BLITTER_WINDOW *pMural)
{
    if (pBlitter->pCurrentMural == pMural)
        return;

    pBlitter->pCurrentMural = pMural;
    pBlitter->Flags.CurrentMuralChanged = 1;

    if (!CrBltIsEntered(pBlitter))
        return;

    if (pMural)
        pBlitter->pDispatch->MakeCurrent(pMural->Base.id, 0, pBlitter->CtxInfo.Base.id);
    else
        pBlitter->pDispatch->MakeCurrent(0, 0, 0);
}

#define CRBLT_FILTER_FROM_FLAGS(_f) (((_f) & CRBLT_F_LINEAR) ? GL_LINEAR : GL_NEAREST)

static DECLCALLBACK(int) crBltBlitTexBufImplFbo(PCR_BLITTER pBlitter, VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRect, const PRTRECTSIZE pDstSize, const RTRECT *paDstRect, uint32_t cRects, uint32_t fFlags)
{
    GLenum filter = CRBLT_FILTER_FROM_FLAGS(fFlags);
    pBlitter->pDispatch->BindFramebufferEXT(GL_READ_FRAMEBUFFER, pBlitter->idFBO);
    pBlitter->pDispatch->FramebufferTexture2DEXT(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pSrc->target, pSrc->hwid, 0);
    pBlitter->pDispatch->ReadBuffer(GL_COLOR_ATTACHMENT0);

    for (uint32_t i = 0; i < cRects; ++i)
    {
        const RTRECT * pSrcRect = &paSrcRect[i];
        const RTRECT * pDstRect = &paDstRect[i];
        pBlitter->pDispatch->BlitFramebufferEXT(
                pSrcRect->xLeft, pSrcRect->yTop, pSrcRect->xRight, pSrcRect->yBottom,
                pDstRect->xLeft, pDstRect->yTop, pDstRect->xRight, pDstRect->yBottom,
                GL_COLOR_BUFFER_BIT, filter);
    }

    return VINF_SUCCESS;
}

/* GL_TRIANGLE_FAN */
DECLINLINE(GLfloat*) crBltVtRectTFNormalized(const RTRECT *pRect, uint32_t normalX, uint32_t normalY, GLfloat* pBuff)
{
    /* going ccw:
     * 1. (left;top)        4. (right;top)
     *        |                    ^
     *        >                    |
     * 2. (left;bottom)  -> 3. (right;bottom) */
    /* xLeft yTop */
    pBuff[0] = ((float)pRect->xLeft)/((float)normalX);
    pBuff[1] = ((float)pRect->yTop)/((float)normalY);

    /* xLeft yBottom */
    pBuff[2] = pBuff[0];
    pBuff[3] = ((float)pRect->yBottom)/((float)normalY);

    /* xRight yBottom */
    pBuff[4] = ((float)pRect->xRight)/((float)normalX);
    pBuff[5] = pBuff[3];

    /* xRight yTop */
    pBuff[6] = pBuff[4];
    pBuff[7] = pBuff[1];
    return &pBuff[8];
}

DECLINLINE(GLint*) crBltVtRectTF(const RTRECT *pRect, uint32_t normalX, uint32_t normalY, GLint* pBuff)
{
    /* xLeft yTop */
    pBuff[0] = pRect->xLeft;
    pBuff[1] = pRect->yTop;

    /* xLeft yBottom */
    pBuff[2] = pBuff[0];
    pBuff[3] = pRect->yBottom;

    /* xRight yBottom */
    pBuff[4] = pRect->xRight;
    pBuff[5] = pBuff[3];

    /* xRight yTop */
    pBuff[6] = pBuff[4];
    pBuff[7] = pBuff[1];
    return &pBuff[8];
}

DECLINLINE(GLubyte*) crBltVtFillRectIndicies(GLubyte *pIndex, GLubyte *piBase)
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
DECLINLINE(GLfloat*) crBltVtRectITNormalized(const RTRECT *pRect, uint32_t normalX, uint32_t normalY, GLfloat* pBuff)
{
    GLfloat* ret = crBltVtRectTFNormalized(pRect, normalX, normalY, pBuff);
    return ret;
}

DECLINLINE(GLint*) crBltVtRectIT(RTRECT *pRect, uint32_t normalX, uint32_t normalY, GLint* pBuff, GLubyte **ppIndex, GLubyte *piBase)
{
    GLint* ret = crBltVtRectTF(pRect, normalX, normalY, pBuff);

    if (ppIndex)
        *ppIndex = crBltVtFillRectIndicies(*ppIndex, piBase);

    return ret;
}

DECLINLINE(GLuint) crBltVtGetNumVerticiesTF(GLuint cRects)
{
    return cRects * 4;
}

#define crBltVtGetNumVerticiesIT crBltVtGetNumVerticiesTF

DECLINLINE(GLuint) crBltVtGetNumIndiciesIT(GLuint cRects)
{
    return 6 * cRects;
}


static GLfloat* crBltVtRectsITNormalized(const RTRECT *paRects, uint32_t cRects, uint32_t normalX, uint32_t normalY, GLfloat* pBuff, GLubyte **ppIndex, GLubyte *piBase)
{
    uint32_t i;
    for (i = 0; i < cRects; ++i)
    {
        pBuff = crBltVtRectITNormalized(&paRects[i], normalX, normalY, pBuff);
    }


    if (ppIndex)
    {
        GLubyte *pIndex = (GLubyte*)pBuff;
        *ppIndex = pIndex;
        for (i = 0; i < cRects; ++i)
        {
            pIndex = crBltVtFillRectIndicies(pIndex, piBase);
        }
        pBuff = (GLfloat*)pIndex;
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

static void crBltCheckSetupViewport(PCR_BLITTER pBlitter, const PRTRECTSIZE pDstSize, bool fFBODraw)
{
    if (!pBlitter->Flags.LastWasFBODraw != !fFBODraw
            || pBlitter->Flags.CurrentMuralChanged
            || pBlitter->CurrentSetSize.cx != pDstSize->cx
            || pBlitter->CurrentSetSize.cy != pDstSize->cy)
    {
#if 0
        const GLdouble aProjection[] =
        {
                2.0 / pDstSize->cx,               0.0,  0.0, 0.0,
                              0.0, 2.0 / pDstSize->cy,  0.0, 0.0,
                              0.0,               0.0,  2.0, 0.0,
                             -1.0,              -1.0, -1.0, 1.0
        };
        pBlitter->pDispatch->MatrixMode(GL_PROJECTION);
        pBlitter->pDispatch->LoadMatrixd(aProjection);
        pBlitter->pDispatch->Viewport(0, 0, pDstSize->cx, pDstSize->cy);
#else
        pBlitter->pDispatch->MatrixMode(GL_PROJECTION);
        pBlitter->pDispatch->LoadIdentity();
        pBlitter->pDispatch->Viewport(0, 0, pDstSize->cx, pDstSize->cy);
        pBlitter->pDispatch->Ortho(0, pDstSize->cx, 0, pDstSize->cy, -1, 1);
        pBlitter->pDispatch->MatrixMode(GL_TEXTURE);
        pBlitter->pDispatch->LoadIdentity();
        pBlitter->pDispatch->MatrixMode(GL_MODELVIEW);
        pBlitter->pDispatch->LoadIdentity();

        /* Clear background to transparent */
        pBlitter->pDispatch->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
#endif
        pBlitter->CurrentSetSize.cx = pDstSize->cx;
        pBlitter->CurrentSetSize.cy = pDstSize->cy;
        pBlitter->Flags.LastWasFBODraw = fFBODraw;
        if (!fFBODraw)
            pBlitter->Flags.CurrentMuralChanged = 0;
    }
}

static DECLCALLBACK(int) crBltBlitTexBufImplDraw2D(PCR_BLITTER pBlitter, VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRect, const PRTRECTSIZE pDstSize, const RTRECT *paDstRect, uint32_t cRects, uint32_t fFlags)
{
    GLuint normalX, normalY;

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
            return VERR_INVALID_PARAMETER;
        }
    }

    Assert(pSrc->hwid);

    pBlitter->pDispatch->BindTexture(pSrc->target, pSrc->hwid);

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
            pTexCoords = crBltVtRectTFNormalized(paDstRect, 1, 1, pVerticies);
            crBltVtRectTFNormalized(paSrcRect, normalX, normalY, pTexCoords);
        }

        pBlitter->pDispatch->EnableClientState(GL_VERTEX_ARRAY);
        pBlitter->pDispatch->VertexPointer(2, GL_FLOAT, 0, pVerticies);

        pBlitter->pDispatch->EnableClientState(GL_TEXTURE_COORD_ARRAY);
        pBlitter->pDispatch->TexCoordPointer(2, GL_FLOAT, 0, pTexCoords);

        pBlitter->pDispatch->Enable(pSrc->target);

        pBlitter->pDispatch->DrawArrays(GL_TRIANGLE_FAN, 0, cElements);

        pBlitter->pDispatch->Disable(pSrc->target);

        pBlitter->pDispatch->DisableClientState(GL_TEXTURE_COORD_ARRAY);
        pBlitter->pDispatch->DisableClientState(GL_VERTEX_ARRAY);
    }
    else
    {
        bool bUseSameVerticies = paSrcRect == paDstRect && normalX == 1 && normalY == 1;
        GLfloat *pVerticies;
        GLfloat *pTexCoords;
        GLubyte *pIndicies;
        GLuint cElements = crBltVtGetNumVerticiesIT(cRects);
        GLuint cIndicies = crBltVtGetNumIndiciesIT(cRects);
        GLubyte iIdxBase = 0;
        if (bUseSameVerticies)
        {
            pVerticies = (GLfloat*)crBltBufGet(&pBlitter->Verticies, cElements * 2 * sizeof (*pVerticies));
            crBltVtRectsITNormalized(paDstRect, cRects, normalX, normalY, pVerticies, &pIndicies, &iIdxBase);
            pTexCoords = pVerticies;
        }
        else
        {
            pVerticies = (GLfloat*)crBltBufGet(&pBlitter->Verticies, cElements * 2 * 2 * sizeof (*pVerticies));
            pTexCoords = crBltVtRectsITNormalized(paDstRect, cRects, 1, 1, pVerticies, &pIndicies, &iIdxBase);
            crBltVtRectsITNormalized(paSrcRect, cRects, normalX, normalY, pTexCoords, NULL, NULL);
        }

        pBlitter->pDispatch->EnableClientState(GL_VERTEX_ARRAY);
        pBlitter->pDispatch->VertexPointer(2, GL_FLOAT, 0, pVerticies);

        pBlitter->pDispatch->EnableClientState(GL_TEXTURE_COORD_ARRAY);
        pBlitter->pDispatch->TexCoordPointer(2, GL_FLOAT, 0, pTexCoords);

        pBlitter->pDispatch->Enable(pSrc->target);

        pBlitter->pDispatch->DrawElements(GL_TRIANGLES, cIndicies, GL_UNSIGNED_BYTE, pIndicies);

        pBlitter->pDispatch->Disable(pSrc->target);

        pBlitter->pDispatch->DisableClientState(GL_TEXTURE_COORD_ARRAY);
        pBlitter->pDispatch->DisableClientState(GL_VERTEX_ARRAY);
    }

    return VINF_SUCCESS;
}

static int crBltInitOnMakeCurent(PCR_BLITTER pBlitter)
{
    const char * pszExtension = (const char*)pBlitter->pDispatch->GetString(GL_EXTENSIONS);
    if (crStrstr(pszExtension, "GL_EXT_framebuffer_object"))
    {
        pBlitter->Flags.SupportsFBO = 1;
        pBlitter->pDispatch->GenFramebuffersEXT(1, &pBlitter->idFBO);
        Assert(pBlitter->idFBO);
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

void CrBltLeave(PCR_BLITTER pBlitter)
{
    Assert(CrBltIsEntered(pBlitter));

    if (pBlitter->Flags.SupportsFBO)
    {
        pBlitter->pDispatch->BindFramebufferEXT(GL_FRAMEBUFFER, 0);
        pBlitter->pDispatch->DrawBuffer(GL_BACK);
        pBlitter->pDispatch->ReadBuffer(GL_BACK);
    }

    pBlitter->pDispatch->Flush();

    if (pBlitter->pRestoreCtxInfo != &pBlitter->CtxInfo)
    {

        pBlitter->pDispatch->MakeCurrent(pBlitter->pRestoreMural->Base.id, 0,
                pBlitter->pRestoreCtxInfo->Base.id >= 0
                    ? pBlitter->pRestoreCtxInfo->Base.id : pBlitter->pRestoreCtxInfo->Base.id);
    }
    else
    {
        pBlitter->pDispatch->MakeCurrent(0, 0, 0);
    }

    pBlitter->pRestoreCtxInfo = NULL;
}

int CrBltEnter(PCR_BLITTER pBlitter, CR_BLITTER_CONTEXT *pRestoreCtxInfo, CR_BLITTER_WINDOW *pRestoreMural)
{
    if (!pBlitter->pCurrentMural)
    {
        crWarning("current mural not initialized!");
        return VERR_INVALID_STATE;
    }

    if (CrBltIsEntered(pBlitter))
    {
        crWarning("blitter is entered already!");
        return VERR_INVALID_STATE;
    }

    if (pRestoreCtxInfo)
    {
        pBlitter->pRestoreCtxInfo = pRestoreCtxInfo;
        pBlitter->pRestoreMural = pRestoreMural;

        pBlitter->pDispatch->Flush();
    }
    else
    {
        pBlitter->pRestoreCtxInfo = &pBlitter->CtxInfo;
    }

    pBlitter->pDispatch->MakeCurrent(pBlitter->pCurrentMural->Base.id, 0, pBlitter->CtxInfo.Base.id);

    if (pBlitter->Flags.Initialized)
        return VINF_SUCCESS;

    int rc = crBltInitOnMakeCurent(pBlitter);
    if (RT_SUCCESS(rc))
    {
        pBlitter->Flags.Initialized = 1;
        return VINF_SUCCESS;
    }

    crWarning("crBltInitOnMakeCurent failed, rc %d", rc);
    CrBltLeave(pBlitter);
    return rc;
}

static void crBltBlitTexBuf(PCR_BLITTER pBlitter, VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRects, GLenum enmDstBuff, const PRTRECTSIZE pDstSize, const RTRECT *paDstRects, uint32_t cRects, uint32_t fFlags)
{
    pBlitter->pDispatch->DrawBuffer(enmDstBuff);

    crBltCheckSetupViewport(pBlitter, pDstSize, enmDstBuff == GL_DRAW_FRAMEBUFFER);

    pBlitter->pfnBlt(pBlitter, pSrc, paSrcRects, pDstSize, paDstRects, cRects, fFlags);
}

void CrBltBlitTexMural(PCR_BLITTER pBlitter, VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRects, const RTRECT *paDstRects, uint32_t cRects, uint32_t fFlags)
{
    RTRECTSIZE DstSize = {pBlitter->pCurrentMural->width, pBlitter->pCurrentMural->height};

    pBlitter->pDispatch->BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);

    crBltBlitTexBuf(pBlitter, pSrc, paSrcRects, GL_BACK, &DstSize, paDstRects, cRects, fFlags);
}

void CrBltBlitTexTex(PCR_BLITTER pBlitter, VBOXVR_TEXTURE *pSrc, const RTRECT *pSrcRect, VBOXVR_TEXTURE *pDst, const RTRECT *pDstRect, uint32_t cRects, uint32_t fFlags)
{
    RTRECTSIZE DstSize = {(uint32_t)pDst->width, (uint32_t)pDst->height};

    pBlitter->pDispatch->BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, pBlitter->idFBO);

    /* TODO: mag/min filters ? */

    pBlitter->pDispatch->FramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pDst->target, pDst->hwid, 0);

//    pBlitter->pDispatch->FramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
//    pBlitter->pDispatch->FramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

    crBltBlitTexBuf(pBlitter, pSrc, pSrcRect, GL_DRAW_FRAMEBUFFER, &DstSize, pDstRect, cRects, fFlags);
}

void CrBltPresent(PCR_BLITTER pBlitter)
{
    if (pBlitter->CtxInfo.Base.visualBits & CR_DOUBLE_BIT)
        pBlitter->pDispatch->SwapBuffers(pBlitter->pCurrentMural->Base.id, 0);
    else
        pBlitter->pDispatch->Flush();
}
