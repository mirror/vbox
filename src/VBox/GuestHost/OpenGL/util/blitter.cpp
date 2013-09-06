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

/* @param pCtxBase      - contains the blitter context info. Its value is treated differently depending on the fCreateNewCtx value
 * @param fCreateNewCtx - if true  - the pCtxBase must NOT be NULL. its visualBits is used as a visual bits info for the new context,
 *                                   its id field is used to specified the shared context id to be used for blitter context.
 *                                   The id can be null to specify no shared context is needed
 *                        if false - if pCtxBase is NOT null AND its id field is NOT null -
 *                                     specified the blitter context to be used
 *                                     blitter treats it as if it has default ogl state.
 *                                   otherwise -
 *                                     the blitter works in a "no-context" mode, i.e. a caller is responsible
 *                                     to making a proper context current before calling the blitter.
 *                                     Note that BltEnter/Leave MUST still be called, but the proper context
 *                                     must be set before doing BltEnter, and ResoreContext info is ignored in that case.
 *                                     Also note that blitter caches the current window info, and assumes the current context's values are preserved
 *                                     wrt that window before the calls, so if one uses different contexts for one blitter,
 *                                     the blitter current window values must be explicitly reset by doing CrBltMuralSetCurrent(pBlitter, NULL)
 * @param fForceDrawBlt - if true  - forces the blitter to always use glDrawXxx-based blits even if GL_EXT_framebuffer_blit.
 *                                   This is needed because BlitFramebufferEXT is known to be often buggy, and glDrawXxx-based blits appear to be more reliable
 */
VBOXBLITTERDECL(int) CrBltInit(PCR_BLITTER pBlitter, const CR_BLITTER_CONTEXT *pCtxBase, bool fCreateNewCtx, bool fForceDrawBlt, const CR_GLSL_CACHE *pShaders, SPUDispatchTable *pDispatch)
{
    if (pCtxBase && pCtxBase->Base.id < 0)
    {
        crWarning("Default share context not initialized!");
        return VERR_INVALID_PARAMETER;
    }

    if (!pCtxBase && fCreateNewCtx)
    {
        crWarning("pCtxBase is zero while fCreateNewCtx is set!");
        return VERR_INVALID_PARAMETER;
    }

    memset(pBlitter, 0, sizeof (*pBlitter));

    pBlitter->pDispatch = pDispatch;
    if (pCtxBase)
        pBlitter->CtxInfo = *pCtxBase;

    pBlitter->Flags.ForceDrawBlit = fForceDrawBlt;

    if (fCreateNewCtx)
    {
        pBlitter->CtxInfo.Base.id = pDispatch->CreateContext("", pCtxBase->Base.visualBits, pCtxBase->Base.id);
        if (!pBlitter->CtxInfo.Base.id)
        {
            memset(pBlitter, 0, sizeof (*pBlitter));
            crWarning("CreateContext failed!");
            return VERR_GENERAL_FAILURE;
        }
        pBlitter->Flags.CtxCreated = 1;
    }

    if (pShaders)
    {
        pBlitter->pGlslCache = pShaders;
        pBlitter->Flags.ShadersGloal = 1;
    }
    else
    {
        CrGlslInit(&pBlitter->LocalGlslCache, pDispatch);
        pBlitter->pGlslCache = &pBlitter->LocalGlslCache;
    }

    return VINF_SUCCESS;
}

VBOXBLITTERDECL(int) CrBltCleanup(PCR_BLITTER pBlitter, const CR_BLITTER_CONTEXT *pRestoreCtxInfo, const CR_BLITTER_WINDOW *pRestoreMural)
{
    if (CrBltIsEntered(pBlitter))
    {
        crWarning("CrBltBlitTexTex: blitter is entered");
        return VERR_INVALID_STATE;
    }

    if (pBlitter->Flags.ShadersGloal || !CrGlslNeedsCleanup(&pBlitter->LocalGlslCache))
        return VINF_SUCCESS;

    int rc = CrBltEnter(pBlitter, pRestoreCtxInfo, pRestoreMural);
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrBltEnter failed, rc %d", rc);
        return rc;
    }

    CrGlslCleanup(&pBlitter->LocalGlslCache);

    CrBltLeave(pBlitter);

    return VINF_SUCCESS;
}

void CrBltTerm(PCR_BLITTER pBlitter)
{
    if (pBlitter->Flags.CtxCreated)
        pBlitter->pDispatch->DestroyContext(pBlitter->CtxInfo.Base.id);
    memset(pBlitter, 0, sizeof (*pBlitter));
}

int CrBltMuralSetCurrent(PCR_BLITTER pBlitter, const CR_BLITTER_WINDOW *pMural)
{
    if (pMural)
    {
        if (!memcmp(&pBlitter->CurrentMural, pMural, sizeof (pBlitter->CurrentMural)))
            return VINF_SUCCESS;
        memcpy(&pBlitter->CurrentMural, pMural, sizeof (pBlitter->CurrentMural));
    }
    else
    {
        if (CrBltIsEntered(pBlitter))
        {
            crWarning("can not set null mural for entered bleater");
            return VERR_INVALID_STATE;
        }
        if (!pBlitter->CurrentMural.Base.id)
            return VINF_SUCCESS;
        pBlitter->CurrentMural.Base.id = 0;
    }

    pBlitter->Flags.CurrentMuralChanged = 1;

    if (!CrBltIsEntered(pBlitter))
        return VINF_SUCCESS;
    else if (!pBlitter->CtxInfo.Base.id)
    {
        crWarning("setting current mural for entered no-context blitter");
        return VERR_INVALID_STATE;
    }

    crWarning("changing mural for entered blitter, is is somewhat expected?");

    pBlitter->pDispatch->Flush();

    pBlitter->pDispatch->MakeCurrent(pMural->Base.id, pBlitter->i32MakeCurrentUserData, pBlitter->CtxInfo.Base.id);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) crBltBlitTexBufImplFbo(PCR_BLITTER pBlitter, const VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRect, const RTRECTSIZE *pDstSize, const RTRECT *paDstRect, uint32_t cRects, uint32_t fFlags)
{
    GLenum filter = CRBLT_FILTER_FROM_FLAGS(fFlags);
    pBlitter->pDispatch->BindFramebufferEXT(GL_READ_FRAMEBUFFER, pBlitter->idFBO);
    pBlitter->pDispatch->FramebufferTexture2DEXT(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pSrc->target, pSrc->hwid, 0);
    pBlitter->pDispatch->ReadBuffer(GL_COLOR_ATTACHMENT0);

    for (uint32_t i = 0; i < cRects; ++i)
    {
        const RTRECT * pSrcRect = &paSrcRect[i];
        const RTRECT * pDstRect = &paDstRect[i];
        int32_t srcY1;
        int32_t srcY2;
        int32_t dstY1;
        int32_t dstY2;
        int32_t srcX1 = pSrcRect->xLeft;
        int32_t srcX2 = pSrcRect->xRight;
        int32_t dstX1 = pDstRect->xLeft;
        int32_t dstX2 = pDstRect->xRight;

        if (CRBLT_F_INVERT_SRC_YCOORDS & fFlags)
        {
            srcY1 = pSrc->height - pSrcRect->yTop;
            srcY2 = pSrc->height - pSrcRect->yBottom;
        }
        else
        {
            srcY1 = pSrcRect->yTop;
            srcY2 = pSrcRect->yBottom;
        }

        if (CRBLT_F_INVERT_DST_YCOORDS & fFlags)
        {
            dstY1 = pDstSize->cy - pDstRect->yTop;
            dstY2 = pDstSize->cy - pDstRect->yBottom;
        }
        else
        {
            dstY1 = pDstRect->yTop;
            dstY2 = pDstRect->yBottom;
        }

        if (srcY1 > srcY2)
        {
            if (dstY1 > dstY2)
            {
                /* use srcY1 < srcY2 && dstY1 < dstY2 whenever possible to avoid GPU driver bugs */
                int32_t tmp = srcY1;
                srcY1 = srcY2;
                srcY2 = tmp;
                tmp = dstY1;
                dstY1 = dstY2;
                dstY2 = tmp;
            }
        }

        if (srcX1 > srcX2)
        {
            if (dstX1 > dstX2)
            {
                /* use srcX1 < srcX2 && dstX1 < dstX2 whenever possible to avoid GPU driver bugs */
                int32_t tmp = srcX1;
                srcX1 = srcX2;
                srcX2 = tmp;
                tmp = dstX1;
                dstX1 = dstX2;
                dstX2 = tmp;
            }
        }

        pBlitter->pDispatch->BlitFramebufferEXT(srcX1, srcY1, srcX2, srcY2,
                    dstX1, dstY1, dstX2, dstY2,
                    GL_COLOR_BUFFER_BIT, filter);
    }

    return VINF_SUCCESS;
}

/* GL_TRIANGLE_FAN */
DECLINLINE(GLfloat*) crBltVtRectTFNormalized(const RTRECT *pRect, uint32_t normalX, uint32_t normalY, GLfloat* pBuff, uint32_t height)
{
    /* going ccw:
     * 1. (left;top)        4. (right;top)
     *        |                    ^
     *        >                    |
     * 2. (left;bottom)  -> 3. (right;bottom) */
    /* xLeft yTop */
    pBuff[0] = ((float)pRect->xLeft)/((float)normalX);
    pBuff[1] = ((float)(height ? height - pRect->yTop : pRect->yTop))/((float)normalY);

    /* xLeft yBottom */
    pBuff[2] = pBuff[0];
    pBuff[3] = ((float)(height ? height - pRect->yBottom : pRect->yBottom))/((float)normalY);

    /* xRight yBottom */
    pBuff[4] = ((float)pRect->xRight)/((float)normalX);
    pBuff[5] = pBuff[3];

    /* xRight yTop */
    pBuff[6] = pBuff[4];
    pBuff[7] = pBuff[1];
    return &pBuff[8];
}

DECLINLINE(GLfloat*) crBltVtRectsTFNormalized(const RTRECT *paRects, uint32_t cRects, uint32_t normalX, uint32_t normalY, GLfloat* pBuff, uint32_t height)
{
    for (uint32_t i = 0; i < cRects; ++i)
    {
        pBuff = crBltVtRectTFNormalized(&paRects[i], normalX, normalY, pBuff, height);
    }
    return pBuff;
}

DECLINLINE(GLint*) crBltVtRectTF(const RTRECT *pRect, uint32_t normalX, uint32_t normalY, GLint* pBuff, uint32_t height)
{
    /* xLeft yTop */
    pBuff[0] = pRect->xLeft;
    pBuff[1] = height ? height - pRect->yTop : pRect->yTop;

    /* xLeft yBottom */
    pBuff[2] = pBuff[0];
    pBuff[3] = height ? height - pRect->yBottom : pRect->yBottom;

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
    *piBase = iBase + 4;
    return pIndex + 6;
}

/* Indexed GL_TRIANGLES */
DECLINLINE(GLfloat*) crBltVtRectITNormalized(const RTRECT *pRect, uint32_t normalX, uint32_t normalY, GLfloat* pBuff, uint32_t height)
{
    GLfloat* ret = crBltVtRectTFNormalized(pRect, normalX, normalY, pBuff, height);
    return ret;
}

DECLINLINE(GLint*) crBltVtRectIT(RTRECT *pRect, uint32_t normalX, uint32_t normalY, GLint* pBuff, GLubyte **ppIndex, GLubyte *piBase, uint32_t height)
{
    GLint* ret = crBltVtRectTF(pRect, normalX, normalY, pBuff, height);

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


static GLfloat* crBltVtRectsITNormalized(const RTRECT *paRects, uint32_t cRects, uint32_t normalX, uint32_t normalY, GLfloat* pBuff, GLubyte **ppIndex, GLubyte *piBase, uint32_t height)
{
    uint32_t i;
    for (i = 0; i < cRects; ++i)
    {
        pBuff = crBltVtRectITNormalized(&paRects[i], normalX, normalY, pBuff, height);
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

#ifndef DEBUG_misha
        /* debugging: ensure we calculate proper buffer size */
        cbBuffer += 16;
#endif

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

static void crBltCheckSetupViewport(PCR_BLITTER pBlitter, const RTRECTSIZE *pDstSize, bool fFBODraw)
{
    bool fUpdateViewport = pBlitter->Flags.CurrentMuralChanged;
    if (pBlitter->CurrentSetSize.cx != pDstSize->cx
            || pBlitter->CurrentSetSize.cy != pDstSize->cy)
    {
        pBlitter->CurrentSetSize = *pDstSize;
        pBlitter->pDispatch->MatrixMode(GL_PROJECTION);
        pBlitter->pDispatch->LoadIdentity();
        pBlitter->pDispatch->Ortho(0, pDstSize->cx, 0, pDstSize->cy, -1, 1);
        fUpdateViewport = true;
    }

    if (fUpdateViewport)
    {
        pBlitter->pDispatch->Viewport(0, 0, pBlitter->CurrentSetSize.cx, pBlitter->CurrentSetSize.cy);
        pBlitter->Flags.CurrentMuralChanged = 0;
    }

    pBlitter->Flags.LastWasFBODraw = fFBODraw;
}

static DECLCALLBACK(int) crBltBlitTexBufImplDraw2D(PCR_BLITTER pBlitter, const VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRect, const RTRECTSIZE *pDstSize, const RTRECT *paDstRect, uint32_t cRects, uint32_t fFlags)
{
    GLuint normalX, normalY;
    uint32_t srcHeight = (fFlags & CRBLT_F_INVERT_SRC_YCOORDS) ? pSrc->height : 0;
    uint32_t dstHeight = (fFlags & CRBLT_F_INVERT_DST_YCOORDS) ? pDstSize->cy : 0;

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
        /* just optimization to draw a single rect with GL_TRIANGLE_FAN */
        GLfloat *pVerticies;
        GLfloat *pTexCoords;
        GLuint cElements = crBltVtGetNumVerticiesTF(cRects);

        pVerticies = (GLfloat*)crBltBufGet(&pBlitter->Verticies, cElements * 2 * 2 * sizeof (*pVerticies));
        pTexCoords = crBltVtRectsTFNormalized(paDstRect, cRects, 1, 1, pVerticies, dstHeight);
        crBltVtRectsTFNormalized(paSrcRect, cRects, normalX, normalY, pTexCoords, srcHeight);

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
        GLfloat *pVerticies;
        GLfloat *pTexCoords;
        GLubyte *pIndicies;
        GLuint cElements = crBltVtGetNumVerticiesIT(cRects);
        GLuint cIndicies = crBltVtGetNumIndiciesIT(cRects);
        GLubyte iIdxBase = 0;

        pVerticies = (GLfloat*)crBltBufGet(&pBlitter->Verticies, cElements * 2 * 2 * sizeof (*pVerticies) + cIndicies * sizeof (*pIndicies));
        pTexCoords = crBltVtRectsITNormalized(paDstRect, cRects, 1, 1, pVerticies, &pIndicies, &iIdxBase, dstHeight);
        crBltVtRectsITNormalized(paSrcRect, cRects, normalX, normalY, pTexCoords, NULL, NULL, srcHeight);

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

    pBlitter->pDispatch->BindTexture(pSrc->target, 0);

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

    /* BlitFramebuffer seems to be buggy on Intel, 
     * try always glDrawXxx for now */
    if (!pBlitter->Flags.ForceDrawBlit && crStrstr(pszExtension, "GL_EXT_framebuffer_blit"))
    {
        pBlitter->pfnBlt = crBltBlitTexBufImplFbo;
    }
    else
    {
//        crWarning("GL_EXT_framebuffer_blit not supported, will use Draw functions for blitting, which might be less efficient");
        pBlitter->pfnBlt = crBltBlitTexBufImplDraw2D;
    }

    /* defaults. but just in case */
    pBlitter->pDispatch->MatrixMode(GL_TEXTURE);
    pBlitter->pDispatch->LoadIdentity();
    pBlitter->pDispatch->MatrixMode(GL_MODELVIEW);
    pBlitter->pDispatch->LoadIdentity();

    return VINF_SUCCESS;
}

void CrBltLeave(PCR_BLITTER pBlitter)
{
    if (!CrBltIsEntered(pBlitter))
    {
        crWarning("CrBltLeave: blitter not entered");
        return;
    }

    if (pBlitter->Flags.SupportsFBO)
    {
        pBlitter->pDispatch->BindFramebufferEXT(GL_FRAMEBUFFER, 0);
        pBlitter->pDispatch->DrawBuffer(GL_BACK);
        pBlitter->pDispatch->ReadBuffer(GL_BACK);
    }

    pBlitter->pDispatch->Flush();

    if (pBlitter->CtxInfo.Base.id)
    {
        if (pBlitter->pRestoreCtxInfo != &pBlitter->CtxInfo)
        {
            pBlitter->pDispatch->MakeCurrent(pBlitter->pRestoreMural->Base.id, 0, pBlitter->pRestoreCtxInfo->Base.id);
        }
        else
        {
            pBlitter->pDispatch->MakeCurrent(0, 0, 0);
        }
    }

    pBlitter->pRestoreCtxInfo = NULL;
}

int CrBltEnter(PCR_BLITTER pBlitter, const CR_BLITTER_CONTEXT *pRestoreCtxInfo, const CR_BLITTER_WINDOW *pRestoreMural)
{
    if (!pBlitter->CurrentMural.Base.id && pBlitter->CtxInfo.Base.id)
    {
        crWarning("current mural not initialized!");
        return VERR_INVALID_STATE;
    }

    if (CrBltIsEntered(pBlitter))
    {
        crWarning("blitter is entered already!");
        return VERR_INVALID_STATE;
    }

    if (pBlitter->CurrentMural.Base.id) /* <- pBlitter->CurrentMural.Base.id can be null if the blitter is in a "no-context" mode (see comments to BltInit for detail)*/
    {
        if (pRestoreCtxInfo)
            pBlitter->pDispatch->Flush();
        pBlitter->pDispatch->MakeCurrent(pBlitter->CurrentMural.Base.id, pBlitter->i32MakeCurrentUserData, pBlitter->CtxInfo.Base.id);
    }
    else
    {
        if (pRestoreCtxInfo)
        {
            crWarning("pRestoreCtxInfo is not NULL for \"no-context\" blitter");
            pRestoreCtxInfo = NULL;
        }
    }

    if (pRestoreCtxInfo)
    {
        pBlitter->pRestoreCtxInfo = pRestoreCtxInfo;
        pBlitter->pRestoreMural = pRestoreMural;
    }
    else
    {
        pBlitter->pRestoreCtxInfo = &pBlitter->CtxInfo;
    }

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

static void crBltBlitTexBuf(PCR_BLITTER pBlitter, const VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRects, GLenum enmDstBuff, const RTRECTSIZE *pDstSize, const RTRECT *paDstRects, uint32_t cRects, uint32_t fFlags)
{
    pBlitter->pDispatch->DrawBuffer(enmDstBuff);

    crBltCheckSetupViewport(pBlitter, pDstSize, enmDstBuff == GL_DRAW_FRAMEBUFFER);

    if (!(fFlags & CRBLT_F_NOALPHA))
        pBlitter->pfnBlt(pBlitter, pSrc, paSrcRects, pDstSize, paDstRects, cRects, fFlags);
    else
    {
        int rc = pBlitter->Flags.ShadersGloal ?
                CrGlslProgUseNoAlpha(pBlitter->pGlslCache, pSrc->target)
                :
                CrGlslProgUseGenNoAlpha(&pBlitter->LocalGlslCache, pSrc->target);

        if (!RT_SUCCESS(rc))
        {
            crWarning("Failed to use no-alpha program rc %d!, falling back to default blit", rc);
            pBlitter->pfnBlt(pBlitter, pSrc, paSrcRects, pDstSize, paDstRects, cRects, fFlags);
            return;
        }

        /* since we use shaders, we need to use draw commands rather than framebuffer blits.
         * force using draw-based blitting */
        crBltBlitTexBufImplDraw2D(pBlitter, pSrc, paSrcRects, pDstSize, paDstRects, cRects, fFlags);

        Assert(pBlitter->Flags.ShadersGloal || &pBlitter->LocalGlslCache == pBlitter->pGlslCache);

        CrGlslProgClear(pBlitter->pGlslCache);
    }
}

void CrBltCheckUpdateViewport(PCR_BLITTER pBlitter)
{
    RTRECTSIZE DstSize = {pBlitter->CurrentMural.width, pBlitter->CurrentMural.height};
    crBltCheckSetupViewport(pBlitter, &DstSize, false);
}

void CrBltBlitTexMural(PCR_BLITTER pBlitter, bool fBb, const VBOXVR_TEXTURE *pSrc, const RTRECT *paSrcRects, const RTRECT *paDstRects, uint32_t cRects, uint32_t fFlags)
{
    if (!CrBltIsEntered(pBlitter))
    {
        crWarning("CrBltBlitTexMural: blitter not entered");
        return;
    }

    RTRECTSIZE DstSize = {pBlitter->CurrentMural.width, pBlitter->CurrentMural.height};

    pBlitter->pDispatch->BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);

    crBltBlitTexBuf(pBlitter, pSrc, paSrcRects, fBb ? GL_BACK : GL_FRONT, &DstSize, paDstRects, cRects, fFlags);
}

void CrBltBlitTexTex(PCR_BLITTER pBlitter, const VBOXVR_TEXTURE *pSrc, const RTRECT *pSrcRect, const VBOXVR_TEXTURE *pDst, const RTRECT *pDstRect, uint32_t cRects, uint32_t fFlags)
{
    if (!CrBltIsEntered(pBlitter))
    {
        crWarning("CrBltBlitTexTex: blitter not entered");
        return;
    }

    RTRECTSIZE DstSize = {(uint32_t)pDst->width, (uint32_t)pDst->height};

    pBlitter->pDispatch->BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, pBlitter->idFBO);

    /* TODO: mag/min filters ? */

    pBlitter->pDispatch->FramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pDst->target, pDst->hwid, 0);

//    pBlitter->pDispatch->FramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
//    pBlitter->pDispatch->FramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

    crBltBlitTexBuf(pBlitter, pSrc, pSrcRect, GL_DRAW_FRAMEBUFFER, &DstSize, pDstRect, cRects, fFlags);

    pBlitter->pDispatch->FramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pDst->target, 0, 0);
}

void CrBltPresent(PCR_BLITTER pBlitter)
{
    if (!CrBltIsEntered(pBlitter))
    {
        crWarning("CrBltPresent: blitter not entered");
        return;
    }

    if (pBlitter->CtxInfo.Base.visualBits & CR_DOUBLE_BIT)
        pBlitter->pDispatch->SwapBuffers(pBlitter->CurrentMural.Base.id, 0);
    else
        pBlitter->pDispatch->Flush();
}

static int crBltImgCreateForTex(const VBOXVR_TEXTURE *pSrc, CR_BLITTER_IMG *pDst, GLenum enmFormat)
{
    memset(pDst, 0, sizeof (*pDst));
    if (enmFormat != GL_RGBA
            && enmFormat != GL_BGRA)
    {
        crWarning("unsupported format 0x%x", enmFormat);
        return VERR_NOT_IMPLEMENTED;
    }

    uint32_t bpp = 32;

    uint32_t pitch = ((bpp * pSrc->width) + 7) >> 3;
    uint32_t cbData = pitch * pSrc->height;
    pDst->pvData = RTMemAllocZ(cbData);
    if (!pDst->pvData)
    {
        crWarning("RTMemAlloc failed");
        return VERR_NO_MEMORY;
    }

#ifdef DEBUG_misha
    {
        char *pTmp = (char*)pDst->pvData;
        for (uint32_t i = 0; i < cbData; ++i)
        {
            pTmp[i] = (char)((1 << i) % 255);
        }
    }
#endif

    pDst->cbData = cbData;
    pDst->enmFormat = enmFormat;
    pDst->width = pSrc->width;
    pDst->height = pSrc->height;
    pDst->bpp = bpp;
    pDst->pitch = pitch;
    return VINF_SUCCESS;
}

VBOXBLITTERDECL(int) CrBltImgGetTex(PCR_BLITTER pBlitter, const VBOXVR_TEXTURE *pSrc, GLenum enmFormat, CR_BLITTER_IMG *pDst)
{
    if (!CrBltIsEntered(pBlitter))
    {
        crWarning("CrBltImgGetTex: blitter not entered");
        return VERR_INVALID_STATE;
    }

    int rc = crBltImgCreateForTex(pSrc, pDst, enmFormat);
    if (!RT_SUCCESS(rc))
    {
        crWarning("crBltImgCreateForTex failed, rc %d", rc);
        return rc;
    }
    pBlitter->pDispatch->BindTexture(pSrc->target, pSrc->hwid);

#ifdef DEBUG_misha
    {
        GLint width = 0, height = 0, depth = 0;
        pBlitter->pDispatch->GetTexLevelParameteriv(pSrc->target, 0, GL_TEXTURE_WIDTH, &width);
        pBlitter->pDispatch->GetTexLevelParameteriv(pSrc->target, 0, GL_TEXTURE_HEIGHT, &height);
        pBlitter->pDispatch->GetTexLevelParameteriv(pSrc->target, 0, GL_TEXTURE_DEPTH, &depth);

        Assert(width == pSrc->width);
        Assert(height == pSrc->height);
//        Assert(depth == pSrc->depth);
    }
#endif

    pBlitter->pDispatch->GetTexImage(pSrc->target, 0, enmFormat, GL_UNSIGNED_BYTE, pDst->pvData);

    pBlitter->pDispatch->BindTexture(pSrc->target, 0);
    return VINF_SUCCESS;
}

VBOXBLITTERDECL(int) CrBltImgGetMural(PCR_BLITTER pBlitter, bool fBb, CR_BLITTER_IMG *pDst)
{
    if (!CrBltIsEntered(pBlitter))
    {
        crWarning("CrBltImgGetMural: blitter not entered");
        return VERR_INVALID_STATE;
    }

    crWarning("NOT IMPLEMENTED");
    return VERR_NOT_IMPLEMENTED;
}

VBOXBLITTERDECL(void) CrBltImgFree(PCR_BLITTER pBlitter, CR_BLITTER_IMG *pDst)
{
    if (!CrBltIsEntered(pBlitter))
    {
        crWarning("CrBltImgFree: blitter not entered");
        return;
    }

    if (pDst->pvData)
    {
        RTMemFree(pDst->pvData);
        pDst->pvData = NULL;
    }
}


VBOXBLITTERDECL(bool) CrGlslIsSupported(CR_GLSL_CACHE *pCache)
{
    if (pCache->iGlVersion == 0)
    {
        const char * pszStr = (const char*)pCache->pDispatch->GetString(GL_VERSION);
        pCache->iGlVersion = crStrParseGlVersion(pszStr);
        if (pCache->iGlVersion <= 0)
        {
            crWarning("crStrParseGlVersion returned %d", pCache->iGlVersion);
            pCache->iGlVersion = -1;
        }
    }

    if (pCache->iGlVersion >= CR_GLVERSION_COMPOSE(2, 0, 0))
        return true;

    crWarning("GLSL unsuported, gl version %d", pCache->iGlVersion);

    /* @todo: we could also check for GL_ARB_shader_objects and GL_ARB_fragment_shader,
     * but seems like chromium does not support properly gl*Object versions of shader functions used with those extensions */
    return false;
}

#define CR_GLSL_STR_V_120 "#version 120\n"
#define CR_GLSL_STR_EXT_TR "#extension GL_ARB_texture_rectangle : enable\n"
#define CR_GLSL_STR_2D "2D"
#define CR_GLSL_STR_2DRECT "2DRect"

#define CR_GLSL_PATTERN_FS_NOALPHA(_ver, _ext, _tex) \
        _ver \
        _ext \
        "uniform sampler" _tex " sampler0;\n" \
        "void main()\n" \
        "{\n" \
            "vec2 srcCoord = vec2(gl_TexCoord[0]);\n" \
            "gl_FragData[0].xyz = (texture" _tex "(sampler0, srcCoord).xyz);\n" \
            "gl_FragData[0].w = 1.0;\n" \
        "}\n"

static const char* crGlslGetFsStringNoAlpha(CR_GLSL_CACHE *pCache, GLenum enmTexTarget)
{
    if (!CrGlslIsSupported(pCache))
    {
        crWarning("CrGlslIsSupported is false");
        return NULL;
    }

    if (pCache->iGlVersion >= CR_GLVERSION_COMPOSE(2, 1, 0))
    {
        if (enmTexTarget == GL_TEXTURE_2D)
            return CR_GLSL_PATTERN_FS_NOALPHA(CR_GLSL_STR_V_120, "", CR_GLSL_STR_2D);
        else if (enmTexTarget == GL_TEXTURE_RECTANGLE_ARB)
            return CR_GLSL_PATTERN_FS_NOALPHA(CR_GLSL_STR_V_120, CR_GLSL_STR_EXT_TR, CR_GLSL_STR_2DRECT);

        crWarning("invalid enmTexTarget %#x", enmTexTarget);
        return NULL;
    }
    else if (pCache->iGlVersion >= CR_GLVERSION_COMPOSE(2, 0, 0))
    {
        if (enmTexTarget == GL_TEXTURE_2D)
            return CR_GLSL_PATTERN_FS_NOALPHA("", "", CR_GLSL_STR_2D);
        else if (enmTexTarget == GL_TEXTURE_RECTANGLE_ARB)
            return CR_GLSL_PATTERN_FS_NOALPHA("", CR_GLSL_STR_EXT_TR, CR_GLSL_STR_2DRECT);

        crWarning("invalid enmTexTarget %#x", enmTexTarget);
        return NULL;
    }

    crError("crGlslGetFsStringNoAlpha: we should not be here!");
    return NULL;
}

static int crGlslProgGenNoAlpha(CR_GLSL_CACHE *pCache, GLenum enmTexTarget, GLuint *puiProgram)
{
    *puiProgram = 0;

    const char*pStrFsShader = crGlslGetFsStringNoAlpha(pCache, enmTexTarget);
    if (!pStrFsShader)
    {
        crWarning("crGlslGetFsStringNoAlpha failed");
        return VERR_NOT_SUPPORTED;
    }

    int rc = VINF_SUCCESS;
    GLchar * pBuf = NULL;
    GLuint uiProgram = 0;
    GLint iUniform = -1;
    GLuint uiShader = pCache->pDispatch->CreateShader(GL_FRAGMENT_SHADER);
    if (!uiShader)
    {
        crWarning("CreateShader failed");
        return VERR_NOT_SUPPORTED;
    }

    pCache->pDispatch->ShaderSource(uiShader, 1, &pStrFsShader, NULL);

    pCache->pDispatch->CompileShader(uiShader);

    GLint compiled = 0;
    pCache->pDispatch->GetShaderiv(uiShader, GL_COMPILE_STATUS, &compiled);

#ifndef DEBUG_misha
    if(!compiled)
#endif
    {
        if (!pBuf)
            pBuf = (GLchar *)RTMemAlloc(16300);
        pCache->pDispatch->GetShaderInfoLog(uiShader, 16300, NULL, pBuf);
#ifdef DEBUG_misha
        if (compiled)
            crDebug("compile success:\n-------------------\n%s\n--------\n", pBuf);
        else
#endif
        {
            crWarning("compile FAILURE:\n-------------------\n%s\n--------\n", pBuf);
            rc = VERR_NOT_SUPPORTED;
            goto end;
        }
    }

    Assert(compiled);

    uiProgram = pCache->pDispatch->CreateProgram();
    if (!uiProgram)
    {
        rc = VERR_NOT_SUPPORTED;
        goto end;
    }

    pCache->pDispatch->AttachShader(uiProgram, uiShader);

    pCache->pDispatch->LinkProgram(uiProgram);

    GLint linked;
    pCache->pDispatch->GetProgramiv(uiProgram, GL_LINK_STATUS, &linked);
#ifndef DEBUG_misha
    if(!linked)
#endif
    {
        if (!pBuf)
            pBuf = (GLchar *)RTMemAlloc(16300);
        pCache->pDispatch->GetProgramInfoLog(uiProgram, 16300, NULL, pBuf);
#ifdef DEBUG_misha
        if (linked)
            crDebug("link success:\n-------------------\n%s\n--------\n", pBuf);
        else
#endif
        {
            crWarning("link FAILURE:\n-------------------\n%s\n--------\n", pBuf);
            rc = VERR_NOT_SUPPORTED;
            goto end;
        }
    }

    Assert(linked);

    iUniform = pCache->pDispatch->GetUniformLocation(uiProgram, "sampler0");
    if (iUniform == -1)
    {
        crWarning("GetUniformLocation failed for sampler0");
    }
    else
    {
        pCache->pDispatch->Uniform1i(iUniform, 0);
    }

    *puiProgram = uiProgram;

    /* avoid end finalizer from cleaning it */
    uiProgram = 0;

    end:
    if (uiShader)
        pCache->pDispatch->DeleteShader(uiShader);
    if (uiProgram)
        pCache->pDispatch->DeleteProgram(uiProgram);
    if (pBuf)
        RTMemFree(pBuf);
    return rc;
}

DECLINLINE(GLuint) crGlslProgGetNoAlpha(const CR_GLSL_CACHE *pCache, GLenum enmTexTarget)
{
    switch (enmTexTarget)
    {
        case GL_TEXTURE_2D:
            return  pCache->uNoAlpha2DProg;
        case GL_TEXTURE_RECTANGLE_ARB:
            return pCache->uNoAlpha2DRectProg;
        default:
            crWarning("invalid tex enmTexTarget %#x", enmTexTarget);
            return 0;
    }
}

DECLINLINE(GLuint*) crGlslProgGetNoAlphaPtr(CR_GLSL_CACHE *pCache, GLenum enmTexTarget)
{
    switch (enmTexTarget)
    {
        case GL_TEXTURE_2D:
            return  &pCache->uNoAlpha2DProg;
        case GL_TEXTURE_RECTANGLE_ARB:
            return &pCache->uNoAlpha2DRectProg;
        default:
            crWarning("invalid tex enmTexTarget %#x", enmTexTarget);
            return NULL;
    }
}

VBOXBLITTERDECL(int) CrGlslProgGenNoAlpha(CR_GLSL_CACHE *pCache, GLenum enmTexTarget)
{
    GLuint*puiProgram = crGlslProgGetNoAlphaPtr(pCache, enmTexTarget);
    if (!puiProgram)
        return VERR_INVALID_PARAMETER;

    if (*puiProgram)
        return VINF_SUCCESS;

    return crGlslProgGenNoAlpha(pCache, enmTexTarget, puiProgram);
}

VBOXBLITTERDECL(int) CrGlslProgGenAllNoAlpha(CR_GLSL_CACHE *pCache)
{
    int rc = CrGlslProgGenNoAlpha(pCache, GL_TEXTURE_2D);
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrGlslProgGenNoAlpha GL_TEXTURE_2D failed rc %d", rc);
        return rc;
    }

    rc = CrGlslProgGenNoAlpha(pCache, GL_TEXTURE_RECTANGLE_ARB);
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrGlslProgGenNoAlpha GL_TEXTURE_RECTANGLE failed rc %d", rc);
        return rc;
    }

    return VINF_SUCCESS;
}

VBOXBLITTERDECL(void) CrGlslProgClear(const CR_GLSL_CACHE *pCache)
{
    pCache->pDispatch->UseProgram(0);
}

VBOXBLITTERDECL(int) CrGlslProgUseNoAlpha(const CR_GLSL_CACHE *pCache, GLenum enmTexTarget)
{
    GLuint uiProg = crGlslProgGetNoAlpha(pCache, enmTexTarget);
    if (!uiProg)
    {
        crWarning("request to use inexistent program!");
        return VERR_INVALID_STATE;
    }

    Assert(uiProg);

    pCache->pDispatch->UseProgram(uiProg);

    return VINF_SUCCESS;
}

VBOXBLITTERDECL(int) CrGlslProgUseGenNoAlpha(CR_GLSL_CACHE *pCache, GLenum enmTexTarget)
{
    GLuint uiProg = crGlslProgGetNoAlpha(pCache, enmTexTarget);
    if (!uiProg)
    {
        int rc = CrGlslProgGenNoAlpha(pCache, enmTexTarget);
        if (!RT_SUCCESS(rc))
        {
            crWarning("CrGlslProgGenNoAlpha failed, rc %d", rc);
            return rc;
        }

        uiProg = crGlslProgGetNoAlpha(pCache, enmTexTarget);
        CRASSERT(uiProg);
    }

    Assert(uiProg);

    pCache->pDispatch->UseProgram(uiProg);

    return VINF_SUCCESS;
}

VBOXBLITTERDECL(bool) CrGlslNeedsCleanup(const CR_GLSL_CACHE *pCache)
{
    return pCache->uNoAlpha2DProg || pCache->uNoAlpha2DRectProg;
}

VBOXBLITTERDECL(void) CrGlslCleanup(CR_GLSL_CACHE *pCache)
{
    if (pCache->uNoAlpha2DProg)
    {
        pCache->pDispatch->DeleteProgram(pCache->uNoAlpha2DProg);
        pCache->uNoAlpha2DProg = 0;
    }

    if (pCache->uNoAlpha2DRectProg)
    {
        pCache->pDispatch->DeleteProgram(pCache->uNoAlpha2DRectProg);
        pCache->uNoAlpha2DRectProg = 0;
    }
}

VBOXBLITTERDECL(void) CrGlslTerm(CR_GLSL_CACHE *pCache)
{
    CRASSERT(!CrGlslNeedsCleanup(pCache));

    CrGlslCleanup(pCache);

    /* sanity */
    memset(pCache, 0, sizeof (*pCache));
}
