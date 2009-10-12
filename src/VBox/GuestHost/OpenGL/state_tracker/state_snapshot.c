/* $Id$ */

/** @file
 * VBox Context state saving/loading used by VM snapshot
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "state.h"
#include "state/cr_statetypes.h"
#include "state/cr_texture.h"
#include "cr_mem.h"
#include "cr_string.h"
#include <stdio.h>

#include <iprt/assert.h>
#include <iprt/types.h>
#include <iprt/err.h>
#include <VBox/err.h>

/* @todo
 * We have two ways of saving/loading states.
 *
 * First which is being used atm, just pure saving/loading of structures.
 * The drawback is we have to deal with all the pointers around those structures,
 * we'd have to update this code if we'd change state tracking.
 * On the bright side it's fast, though it's not really needed as it's not that often operation.
 * It could also worth to split those functions into appropriate parts, 
 * similar to the way context creation is being done.
 *
 * Second way would be to implement full dispatch api table and substitute diff_api during saving/loading.
 * Then if we implement that api in a similar way to packer/unpacker with a change to store/load 
 * via provided pSSM handle instead of pack buffer,
 * saving state could be done by simple diffing against empty "dummy" context.
 * Restoring state in such case would look like unpacking commands from pSSM instead of network buffer.
 * This would be slower (who cares) but most likely will not require any code changes to support in future.
 * We will reduce amount of saved data as we'd save only changed state parts, but I doubt it'd be that much.
 * It could be done for the first way as well, but requires tons of bit checks.
 */

/*@todo move with the one from state_texture.c to some header*/
#define MAX_MIPMAP_LEVELS 20

static int32_t crStateAllocAndSSMR3GetMem(PSSMHANDLE pSSM, void **pBuffer, size_t cbBuffer)
{
    CRASSERT(pSSM && pBuffer && cbBuffer>0);

    *pBuffer = crAlloc(cbBuffer);
    if (!*pBuffer)
        return VERR_NO_MEMORY;

    return SSMR3GetMem(pSSM, *pBuffer, cbBuffer);
}

static int32_t crStateSaveTextureObjData(CRTextureObj *pTexture, PSSMHANDLE pSSM)
{
    int32_t rc, face, i;
    
    CRASSERT(pTexture && pSSM);

    for (face = 0; face < 6; face++) {
        CRASSERT(pTexture->level[face]);

        /*@todo, check if safe to go till MAX_MIPMAP_LEVELS intead of TextureState->maxLevel*/
        for (i = 0; i < MAX_MIPMAP_LEVELS; i++) {
            CRTextureLevel *ptl = &(pTexture->level[face][i]);
            rc = SSMR3PutMem(pSSM, ptl, sizeof(*ptl));
            AssertRCReturn(rc, rc);
            if (ptl->img)
            {
                CRASSERT(ptl->bytes);
                rc = SSMR3PutMem(pSSM, ptl->img, ptl->bytes);
                AssertRCReturn(rc, rc);
            }
#ifdef CR_STATE_NO_TEXTURE_IMAGE_STORE
            /* Note, this is not a bug. 
             * Even with CR_STATE_NO_TEXTURE_IMAGE_STORE defined, it's possible that ptl->img!=NULL.
             * For ex. we're saving snapshot right after it was loaded
             * and some context hasn't been used by the guest application yet
             * (pContext->texture.bResyncNeeded==GL_TRUE).
             */
            else if (ptl->bytes)
            {
                char *pImg;

                pImg = crAlloc(ptl->bytes);
                if (!pImg) return VERR_NO_MEMORY;

                diff_api.BindTexture(pTexture->target, pTexture->name);
                diff_api.GetTexImage(pTexture->target, i, ptl->format, ptl->type, pImg);

                rc = SSMR3PutMem(pSSM, pImg, ptl->bytes);
                crFree(pImg);
                AssertRCReturn(rc, rc);
            }
#endif
        }
    }

    return VINF_SUCCESS;
}

static int32_t crStateLoadTextureObjData(CRTextureObj *pTexture, PSSMHANDLE pSSM)
{
    int32_t rc, face, i;
    
    CRASSERT(pTexture && pSSM);

    for (face = 0; face < 6; face++) {
        CRASSERT(pTexture->level[face]);

        for (i = 0; i < MAX_MIPMAP_LEVELS; i++) {
            CRTextureLevel *ptl = &(pTexture->level[face][i]);
            CRASSERT(!ptl->img);

            rc = SSMR3GetMem(pSSM, ptl, sizeof(*ptl));
            AssertRCReturn(rc, rc);
            if (ptl->img)
            {
                CRASSERT(ptl->bytes);

                ptl->img = crAlloc(ptl->bytes);
                if (!ptl->img) return VERR_NO_MEMORY;

                rc = SSMR3GetMem(pSSM, ptl->img, ptl->bytes);
                AssertRCReturn(rc, rc);
            }
#ifdef CR_STATE_NO_TEXTURE_IMAGE_STORE
            /* Same story as in crStateSaveTextureObjData */
            else if (ptl->bytes)
            {
                ptl->img = crAlloc(ptl->bytes);
                if (!ptl->img) return VERR_NO_MEMORY;

                rc = SSMR3GetMem(pSSM, ptl->img, ptl->bytes);
                AssertRCReturn(rc, rc);
            }
#endif
            crStateTextureInitTextureFormat(ptl, ptl->internalFormat);

            //FILLDIRTY(ptl->dirty);
        }
    }

    return VINF_SUCCESS;
}

static void crStateSaveSharedTextureCB(unsigned long key, void *data1, void *data2)
{
    CRTextureObj *pTexture = (CRTextureObj *) data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    int32_t rc;

    CRASSERT(pTexture && pSSM);

    rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);
    rc = SSMR3PutMem(pSSM, pTexture, sizeof(*pTexture));
    CRASSERT(rc == VINF_SUCCESS);
    rc = crStateSaveTextureObjData(pTexture, pSSM);
    CRASSERT(rc == VINF_SUCCESS);
}

static int32_t crStateSaveMatrixStack(CRMatrixStack *pStack, PSSMHANDLE pSSM)
{
    return SSMR3PutMem(pSSM, pStack->stack, sizeof(CRmatrix) * pStack->maxDepth);
}

static int32_t crStateLoadMatrixStack(CRMatrixStack *pStack, PSSMHANDLE pSSM)
{
    int32_t rc;
    
    CRASSERT(pStack && pSSM);

    rc = SSMR3GetMem(pSSM, pStack->stack, sizeof(CRmatrix) * pStack->maxDepth);
    /* fixup stack top pointer */
    pStack->top = &pStack->stack[pStack->depth];
    return rc;
}

static int32_t crStateSaveTextureObjPtr(CRTextureObj *pTexture, PSSMHANDLE pSSM)
{
    /* Current texture pointer can't be NULL for real texture unit states,
     * but it could be NULL for unused attribute stack depths.
     */
    if (pTexture)
        return SSMR3PutU32(pSSM, pTexture->name);
    else
        return VINF_SUCCESS;
}

static int32_t crStateLoadTextureObjPtr(CRTextureObj **pTexture, CRContext *pContext, GLenum target, PSSMHANDLE pSSM)
{
    uint32_t texName;
    int32_t rc;

    /* We're loading attrib stack with unused state */
    if (!*pTexture)
        return VINF_SUCCESS;

    rc = SSMR3GetU32(pSSM, &texName);
    AssertRCReturn(rc, rc);

    if (texName)
    {
        *pTexture = (CRTextureObj *) crHashtableSearch(pContext->shared->textureTable, texName);
    }
    else
    {
        switch (target)
        {
            case GL_TEXTURE_1D:
                *pTexture = &(pContext->texture.base1D);
                break;
            case GL_TEXTURE_2D:
                *pTexture = &(pContext->texture.base2D);
                break;
#ifdef CR_OPENGL_VERSION_1_2
            case GL_TEXTURE_3D:
                *pTexture = &(pContext->texture.base3D);
                break;
#endif
#ifdef CR_ARB_texture_cube_map
            case GL_TEXTURE_CUBE_MAP_ARB:
                *pTexture = &(pContext->texture.baseCubeMap);
                break;
#endif
#ifdef CR_NV_texture_rectangle
            case GL_TEXTURE_RECTANGLE_NV:
                *pTexture = &(pContext->texture.baseRect);
                break;
#endif
            default:
                crError("LoadTextureObjPtr: Unknown texture target %d", target);
        }
    }

    return rc;
}

static int32_t crStateSaveTexUnitCurrentTexturePtrs(CRTextureUnit *pTexUnit, PSSMHANDLE pSSM)
{
    int32_t rc;

    rc = crStateSaveTextureObjPtr(pTexUnit->currentTexture1D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateSaveTextureObjPtr(pTexUnit->currentTexture2D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateSaveTextureObjPtr(pTexUnit->currentTexture3D, pSSM);
    AssertRCReturn(rc, rc);
#ifdef CR_ARB_texture_cube_map
    rc = crStateSaveTextureObjPtr(pTexUnit->currentTextureCubeMap, pSSM);
    AssertRCReturn(rc, rc);
#endif
#ifdef CR_NV_texture_rectangle
    rc = crStateSaveTextureObjPtr(pTexUnit->currentTextureRect, pSSM);
    AssertRCReturn(rc, rc);
#endif

    return rc;
}

static int32_t crStateLoadTexUnitCurrentTexturePtrs(CRTextureUnit *pTexUnit, CRContext *pContext, PSSMHANDLE pSSM)
{
    int32_t rc;

    rc = crStateLoadTextureObjPtr(&pTexUnit->currentTexture1D, pContext, GL_TEXTURE_1D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateLoadTextureObjPtr(&pTexUnit->currentTexture2D, pContext, GL_TEXTURE_1D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateLoadTextureObjPtr(&pTexUnit->currentTexture3D, pContext, GL_TEXTURE_2D, pSSM);
    AssertRCReturn(rc, rc);
#ifdef CR_ARB_texture_cube_map
    rc = crStateLoadTextureObjPtr(&pTexUnit->currentTextureCubeMap, pContext, GL_TEXTURE_CUBE_MAP_ARB, pSSM);
    AssertRCReturn(rc, rc);
#endif
#ifdef CR_NV_texture_rectangle
    rc = crStateLoadTextureObjPtr(&pTexUnit->currentTextureRect, pContext, GL_TEXTURE_RECTANGLE_NV, pSSM);
    AssertRCReturn(rc, rc);
#endif

    return rc;
}

static int32_t crSateSaveEvalCoeffs1D(CREvaluator1D *pEval, PSSMHANDLE pSSM)
{
    int32_t rc, i;

    for (i=0; i<GLEVAL_TOT; ++i)
    {
        if (pEval[i].coeff)
        {
            rc = SSMR3PutMem(pSSM, pEval[i].coeff, pEval[i].order * gleval_sizes[i] * sizeof(GLfloat));
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}

static int32_t crSateSaveEvalCoeffs2D(CREvaluator2D *pEval, PSSMHANDLE pSSM)
{
    int32_t rc, i;

    for (i=0; i<GLEVAL_TOT; ++i)
    {
        if (pEval[i].coeff)
        {
            rc = SSMR3PutMem(pSSM, pEval[i].coeff, pEval[i].uorder * pEval[i].vorder * gleval_sizes[i] * sizeof(GLfloat));
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}

static int32_t crSateLoadEvalCoeffs1D(CREvaluator1D *pEval, GLboolean bReallocMem, PSSMHANDLE pSSM)
{
    int32_t rc, i;
    size_t size;

    for (i=0; i<GLEVAL_TOT; ++i)
    {
        if (pEval[i].coeff)
        {
            size = pEval[i].order * gleval_sizes[i] * sizeof(GLfloat);
            if (bReallocMem)
            {
                pEval[i].coeff = (GLfloat*) crAlloc(size);
                if (!pEval[i].coeff) return VERR_NO_MEMORY;
            }
            rc = SSMR3GetMem(pSSM, pEval[i].coeff, size);
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}

static int32_t crSateLoadEvalCoeffs2D(CREvaluator2D *pEval, GLboolean bReallocMem, PSSMHANDLE pSSM)
{
    int32_t rc, i;
    size_t size;

    for (i=0; i<GLEVAL_TOT; ++i)
    {
        if (pEval[i].coeff)
        {
            size = pEval[i].uorder * pEval[i].vorder * gleval_sizes[i] * sizeof(GLfloat);
            if (bReallocMem)
            {
                pEval[i].coeff = (GLfloat*) crAlloc(size);
                if (!pEval[i].coeff) return VERR_NO_MEMORY;
            }
            rc = SSMR3GetMem(pSSM, pEval[i].coeff, size);
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}

static void crStateCopyEvalPtrs1D(CREvaluator1D *pDst, CREvaluator1D *pSrc)
{
    int32_t i;

    for (i=0; i<GLEVAL_TOT; ++i)
        pDst[i].coeff = pSrc[i].coeff;

    /*
    pDst[GL_MAP1_VERTEX_3-GL_MAP1_COLOR_4].coeff = pSrc[GL_MAP1_VERTEX_3-GL_MAP1_COLOR_4].coeff;
    pDst[GL_MAP1_VERTEX_4-GL_MAP1_COLOR_4].coeff = pSrc[GL_MAP1_VERTEX_4-GL_MAP1_COLOR_4].coeff;
    pDst[GL_MAP1_INDEX-GL_MAP1_COLOR_4].coeff = pSrc[GL_MAP1_INDEX-GL_MAP1_COLOR_4].coeff;
    pDst[GL_MAP1_COLOR_4-GL_MAP1_COLOR_4].coeff = pSrc[GL_MAP1_COLOR_4-GL_MAP1_COLOR_4].coeff;
    pDst[GL_MAP1_NORMAL-GL_MAP1_COLOR_4].coeff = pSrc[GL_MAP1_NORMAL-GL_MAP1_COLOR_4].coeff;
    pDst[GL_MAP1_TEXTURE_COORD_1-GL_MAP1_COLOR_4].coeff = pSrc[GL_MAP1_TEXTURE_COORD_1-GL_MAP1_COLOR_4].coeff;
    pDst[GL_MAP1_TEXTURE_COORD_2-GL_MAP1_COLOR_4].coeff = pSrc[GL_MAP1_TEXTURE_COORD_2-GL_MAP1_COLOR_4].coeff;
    pDst[GL_MAP1_TEXTURE_COORD_3-GL_MAP1_COLOR_4].coeff = pSrc[GL_MAP1_TEXTURE_COORD_3-GL_MAP1_COLOR_4].coeff;
    pDst[GL_MAP1_TEXTURE_COORD_4-GL_MAP1_COLOR_4].coeff = pSrc[GL_MAP1_TEXTURE_COORD_4-GL_MAP1_COLOR_4].coeff;
    */
}

static void crStateCopyEvalPtrs2D(CREvaluator2D *pDst, CREvaluator2D *pSrc)
{
    int32_t i;

    for (i=0; i<GLEVAL_TOT; ++i)
        pDst[i].coeff = pSrc[i].coeff;

    /*
    pDst[GL_MAP2_VERTEX_3-GL_MAP2_COLOR_4].coeff = pSrc[GL_MAP2_VERTEX_3-GL_MAP2_COLOR_4].coeff;
    pDst[GL_MAP2_VERTEX_4-GL_MAP2_COLOR_4].coeff = pSrc[GL_MAP2_VERTEX_4-GL_MAP2_COLOR_4].coeff;
    pDst[GL_MAP2_INDEX-GL_MAP2_COLOR_4].coeff = pSrc[GL_MAP2_INDEX-GL_MAP2_COLOR_4].coeff;
    pDst[GL_MAP2_COLOR_4-GL_MAP2_COLOR_4].coeff = pSrc[GL_MAP2_COLOR_4-GL_MAP2_COLOR_4].coeff;
    pDst[GL_MAP2_NORMAL-GL_MAP2_COLOR_4].coeff = pSrc[GL_MAP2_NORMAL-GL_MAP2_COLOR_4].coeff;
    pDst[GL_MAP2_TEXTURE_COORD_1-GL_MAP2_COLOR_4].coeff = pSrc[GL_MAP2_TEXTURE_COORD_1-GL_MAP2_COLOR_4].coeff;
    pDst[GL_MAP2_TEXTURE_COORD_2-GL_MAP2_COLOR_4].coeff = pSrc[GL_MAP2_TEXTURE_COORD_2-GL_MAP2_COLOR_4].coeff;
    pDst[GL_MAP2_TEXTURE_COORD_3-GL_MAP2_COLOR_4].coeff = pSrc[GL_MAP2_TEXTURE_COORD_3-GL_MAP2_COLOR_4].coeff;
    pDst[GL_MAP2_TEXTURE_COORD_4-GL_MAP2_COLOR_4].coeff = pSrc[GL_MAP2_TEXTURE_COORD_4-GL_MAP2_COLOR_4].coeff;
    */
}

static void crStateSaveBufferObjectCB(unsigned long key, void *data1, void *data2)
{
    CRBufferObject *pBufferObj = (CRBufferObject *) data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    int32_t rc;

    CRASSERT(pBufferObj && pSSM);

    rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);
    rc = SSMR3PutMem(pSSM, pBufferObj, sizeof(*pBufferObj));
    CRASSERT(rc == VINF_SUCCESS);

    if (pBufferObj->data)
    {
        /*We could get here even though retainBufferData is false on host side, in case when we're taking snapshot
          after state load and before this context was ever made current*/
        CRASSERT(pBufferObj->size>0);
        rc = SSMR3PutMem(pSSM, pBufferObj->data, pBufferObj->size);
        CRASSERT(rc == VINF_SUCCESS);
    }
    else if (pBufferObj->name!=0 && pBufferObj->size>0)
    {
        diff_api.BindBufferARB(GL_ARRAY_BUFFER_ARB, pBufferObj->name);
        pBufferObj->pointer = diff_api.MapBufferARB(GL_ARRAY_BUFFER_ARB, GL_READ_ONLY_ARB);
        rc = SSMR3PutMem(pSSM, &pBufferObj->pointer, sizeof(pBufferObj->pointer));
        CRASSERT(rc == VINF_SUCCESS);
        if (pBufferObj->pointer)
        {
            rc = SSMR3PutMem(pSSM, pBufferObj->pointer, pBufferObj->size);
            CRASSERT(rc == VINF_SUCCESS);
        }
        diff_api.UnmapBufferARB(GL_ARRAY_BUFFER_ARB);
        pBufferObj->pointer = NULL;
    }
}

static void crStateSaveProgramCB(unsigned long key, void *data1, void *data2)
{
    CRProgram *pProgram = (CRProgram *) data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    CRProgramSymbol *pSymbol;
    int32_t rc;

    CRASSERT(pProgram && pSSM);

    rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);
    rc = SSMR3PutMem(pSSM, pProgram, sizeof(*pProgram));
    CRASSERT(rc == VINF_SUCCESS);
    if (pProgram->string)
    {
        CRASSERT(pProgram->length);
        rc = SSMR3PutMem(pSSM, pProgram->string, pProgram->length);
        CRASSERT(rc == VINF_SUCCESS);
    }

    for (pSymbol = pProgram->symbolTable; pSymbol; pSymbol=pSymbol->next)
    {
        rc = SSMR3PutMem(pSSM, pSymbol, sizeof(*pSymbol));
        CRASSERT(rc == VINF_SUCCESS);
        if (pSymbol->name)
        {
            CRASSERT(pSymbol->cbName>0);
            rc = SSMR3PutMem(pSSM, pSymbol->name, pSymbol->cbName);
            CRASSERT(rc == VINF_SUCCESS);
        }
    }
}

static void crStateSaveFramebuffersCB(unsigned long key, void *data1, void *data2)
{
    CRFramebufferObject *pFBO = (CRFramebufferObject*) data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    int32_t rc;

    rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);

    rc = SSMR3PutMem(pSSM, pFBO, sizeof(*pFBO));
    CRASSERT(rc == VINF_SUCCESS);
}

static void crStateSaveRenderbuffersCB(unsigned long key, void *data1, void *data2)
{
    CRRenderbufferObject *pRBO = (CRRenderbufferObject*) data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    int32_t rc;

    rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);

    rc = SSMR3PutMem(pSSM, pRBO, sizeof(*pRBO));
    CRASSERT(rc == VINF_SUCCESS);
}

static int32_t crStateLoadProgram(CRProgram **ppProgram, PSSMHANDLE pSSM)
{
    CRProgramSymbol **ppSymbol;
    int32_t rc;
    unsigned long key;

    rc = SSMR3GetMem(pSSM, &key, sizeof(key));
    AssertRCReturn(rc, rc);

    /* we're loading default vertex or pixel program*/
    if (*ppProgram)
    {
        if (key!=0) return VERR_SSM_UNEXPECTED_DATA;
    }
    else
    {
        *ppProgram = (CRProgram*) crAlloc(sizeof(CRProgram));
        if (!ppProgram) return VERR_NO_MEMORY;
        if (key==0) return VERR_SSM_UNEXPECTED_DATA;
    }

    rc = SSMR3GetMem(pSSM, *ppProgram, sizeof(**ppProgram));
    AssertRCReturn(rc, rc);

    if ((*ppProgram)->string)
    {
        CRASSERT((*ppProgram)->length);
        (*ppProgram)->string = crAlloc((*ppProgram)->length);
        if (!(*ppProgram)->string) return VERR_NO_MEMORY;
        rc = SSMR3GetMem(pSSM, (void*) (*ppProgram)->string, (*ppProgram)->length);
        AssertRCReturn(rc, rc);
    }

    for (ppSymbol = &(*ppProgram)->symbolTable; *ppSymbol; ppSymbol=&(*ppSymbol)->next)
    {
        *ppSymbol = crAlloc(sizeof(CRProgramSymbol));
        if (!ppSymbol) return VERR_NO_MEMORY;

        rc = SSMR3GetMem(pSSM, *ppSymbol, sizeof(**ppSymbol));
        AssertRCReturn(rc, rc);

        if ((*ppSymbol)->name)
        {
            CRASSERT((*ppSymbol)->cbName>0);
            (*ppSymbol)->name = crAlloc((*ppSymbol)->cbName);
            if (!(*ppSymbol)->name) return VERR_NO_MEMORY;

            rc = SSMR3GetMem(pSSM, (void*) (*ppSymbol)->name, (*ppSymbol)->cbName);
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}

static void crStateSaveString(const char *pStr, PSSMHANDLE pSSM)
{
    int32_t len;
    int32_t rc;

    if (pStr)
    {
        len = crStrlen(pStr)+1;

        rc = SSMR3PutS32(pSSM, len);
        CRASSERT(rc == VINF_SUCCESS);

        rc = SSMR3PutMem(pSSM, pStr, len*sizeof(*pStr));
        CRASSERT(rc == VINF_SUCCESS);
    }
    else
    {
        rc = SSMR3PutS32(pSSM, 0);
        CRASSERT(rc == VINF_SUCCESS);
    }
}

static char* crStateLoadString(PSSMHANDLE pSSM)
{
    int32_t len, rc;
    char* pStr = NULL;

    rc = SSMR3GetS32(pSSM, &len);
    CRASSERT(rc == VINF_SUCCESS);

    if (len!=0)
    {
        pStr = crAlloc(len*sizeof(*pStr));

        rc = SSMR3GetMem(pSSM, pStr, len*sizeof(*pStr));
        CRASSERT(rc == VINF_SUCCESS);
    }

    return pStr;
}

static void crStateSaveGLSLShaderCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLShader *pShader = (CRGLSLShader*) data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    int32_t rc;

    rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);

    rc = SSMR3PutMem(pSSM, pShader, sizeof(*pShader));
    CRASSERT(rc == VINF_SUCCESS);

    if (pShader->source)
    {
        crStateSaveString(pShader->source, pSSM);
    }
    else
    {
        GLint sLen=0;
        GLchar *source=NULL;

        diff_api.GetShaderiv(pShader->hwid, GL_SHADER_SOURCE_LENGTH, &sLen);
        if (sLen>0)
        {
            source = (GLchar*) crAlloc(sLen);
            diff_api.GetShaderSource(pShader->hwid, sLen, NULL, source);
        }

        crStateSaveString(source, pSSM);
        if (source) crFree(source);
    }
}

static CRGLSLShader* crStateLoadGLSLShader(PSSMHANDLE pSSM)
{
    CRGLSLShader *pShader;
    int32_t rc;
    unsigned long key;

    pShader = crAlloc(sizeof(*pShader));
    if (!pShader) return NULL;

    rc = SSMR3GetMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);

    rc = SSMR3GetMem(pSSM, pShader, sizeof(*pShader));
    CRASSERT(rc == VINF_SUCCESS);

    pShader->source = crStateLoadString(pSSM);

    return pShader;
}


static void crStateSaveGLSLShaderKeyCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLShader *pShader = (CRGLSLShader*) data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    int32_t rc;

    rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);
}

static void crStateSaveGLSLProgramAttribs(CRGLSLProgramState *pState, PSSMHANDLE pSSM)
{
    GLuint i;
    int32_t rc;

    for (i=0; i<pState->cAttribs; ++i)
    {
        rc = SSMR3PutMem(pSSM, &pState->pAttribs[i].index, sizeof(pState->pAttribs[i].index));
        CRASSERT(rc == VINF_SUCCESS);
        crStateSaveString(pState->pAttribs[i].name, pSSM);
    }
}

static void crStateSaveGLSLProgramCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLProgram *pProgram = (CRGLSLProgram*) data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    int32_t rc;
    uint32_t ui32;
    GLint maxUniformLen, activeUniforms=0, uniformsCount=0, i, j;
    GLchar *name;
    GLenum type;
    GLint size, location;

    rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);

    rc = SSMR3PutMem(pSSM, pProgram, sizeof(*pProgram));
    CRASSERT(rc == VINF_SUCCESS);

    ui32 = crHashtableNumElements(pProgram->currentState.attachedShaders);
    rc = SSMR3PutU32(pSSM, ui32);
    CRASSERT(rc == VINF_SUCCESS);

    crHashtableWalk(pProgram->currentState.attachedShaders, crStateSaveGLSLShaderKeyCB, pSSM);

    if (pProgram->activeState.attachedShaders)
    {
        ui32 = crHashtableNumElements(pProgram->activeState.attachedShaders);
        rc = SSMR3PutU32(pSSM, ui32);
        CRASSERT(rc == VINF_SUCCESS);
        crHashtableWalk(pProgram->currentState.attachedShaders, crStateSaveGLSLShaderCB, pSSM);
    }

    crStateSaveGLSLProgramAttribs(&pProgram->currentState, pSSM);
    crStateSaveGLSLProgramAttribs(&pProgram->activeState, pSSM);

    diff_api.GetProgramiv(pProgram->hwid, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformLen);
    diff_api.GetProgramiv(pProgram->hwid, GL_ACTIVE_UNIFORMS, &activeUniforms);

    if (activeUniforms>0)
    {
        name = (GLchar *) crAlloc((maxUniformLen+8)*sizeof(GLchar));

        if (!name)
        {
            crWarning("crStateSaveGLSLProgramCB: out of memory");
            return;
        }
    }

    for (i=0; i<activeUniforms; ++i)
    {
        diff_api.GetActiveUniform(pProgram->hwid, i, maxUniformLen, NULL, &size, &type, name);
        uniformsCount += size;
    }
    CRASSERT(uniformsCount>=activeUniforms);

    rc = SSMR3PutS32(pSSM, uniformsCount);
    CRASSERT(rc == VINF_SUCCESS);

    if (activeUniforms>0)
    {
        GLfloat fdata[16];
        GLint idata[16];
        char *pIndexStr=NULL;

        for (i=0; i<activeUniforms; ++i)
        {
            diff_api.GetActiveUniform(pProgram->hwid, i, maxUniformLen, NULL, &size, &type, name);

            if (size>1)
            {
                pIndexStr = crStrchr(name, '[');
                if (!pIndexStr)
                {
                    pIndexStr = name+crStrlen(name);
                }
            }

            for (j=0; j<size; ++j)
            {
                if (size>1)
                {
                    sprintf(pIndexStr, "[%i]", j);
                    location = diff_api.GetUniformLocation(pProgram->hwid, name);
                }
                else
                {
                    location = i;
                }

                rc = SSMR3PutMem(pSSM, &type, sizeof(type));
                CRASSERT(rc == VINF_SUCCESS);

                crStateSaveString(name, pSSM);
            
                if (crStateIsIntUniform(type))
                {
                    diff_api.GetUniformiv(pProgram->hwid, location, &idata[0]);
                    rc = SSMR3PutMem(pSSM, &idata[0], crStateGetUniformSize(type)*sizeof(idata[0]));
                    CRASSERT(rc == VINF_SUCCESS);
                }
                else
                {
                    diff_api.GetUniformfv(pProgram->hwid, location, &fdata[0]);
                    rc = SSMR3PutMem(pSSM, &fdata[0], crStateGetUniformSize(type)*sizeof(fdata[0]));
                    CRASSERT(rc == VINF_SUCCESS);
                }
            }
        }

        crFree(name);
    }
}

int32_t crStateSaveContext(CRContext *pContext, PSSMHANDLE pSSM)
{
    int32_t rc, i;
    uint32_t ui32, j;

    CRASSERT(pContext && pSSM);

    rc = SSMR3PutMem(pSSM, pContext, sizeof(*pContext));
    AssertRCReturn(rc, rc);

    if (crHashtableNumElements(pContext->shared->dlistTable)>0)
        crWarning("Saving state with %d display lists, unsupported", crHashtableNumElements(pContext->shared->dlistTable));

    if (crHashtableNumElements(pContext->program.programHash)>0)
        crDebug("Saving state with %d programs", crHashtableNumElements(pContext->program.programHash));

    /* Save transform state */
    rc = SSMR3PutMem(pSSM, pContext->transform.clipPlane, sizeof(GLvectord)*CR_MAX_CLIP_PLANES);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutMem(pSSM, pContext->transform.clip, sizeof(GLboolean)*CR_MAX_CLIP_PLANES);
    AssertRCReturn(rc, rc);
    rc = crStateSaveMatrixStack(&pContext->transform.modelViewStack, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateSaveMatrixStack(&pContext->transform.projectionStack, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateSaveMatrixStack(&pContext->transform.colorStack, pSSM);
    AssertRCReturn(rc, rc);
    for (i = 0 ; i < CR_MAX_TEXTURE_UNITS ; i++)
    {
        rc = crStateSaveMatrixStack(&pContext->transform.textureStack[i], pSSM);
        AssertRCReturn(rc, rc);
    }
    for (i = 0 ; i < CR_MAX_PROGRAM_MATRICES ; i++)
    {
        rc = crStateSaveMatrixStack(&pContext->transform.programStack[i], pSSM);
        AssertRCReturn(rc, rc);
    }

    /* Save textures */
    rc = crStateSaveTextureObjData(&pContext->texture.base1D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateSaveTextureObjData(&pContext->texture.base2D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateSaveTextureObjData(&pContext->texture.base3D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateSaveTextureObjData(&pContext->texture.proxy1D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateSaveTextureObjData(&pContext->texture.proxy2D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateSaveTextureObjData(&pContext->texture.proxy3D, pSSM);
#ifdef CR_ARB_texture_cube_map
    rc = crStateSaveTextureObjData(&pContext->texture.baseCubeMap, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateSaveTextureObjData(&pContext->texture.proxyCubeMap, pSSM);
    AssertRCReturn(rc, rc);
#endif
#ifdef CR_NV_texture_rectangle
    rc = crStateSaveTextureObjData(&pContext->texture.baseRect, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateSaveTextureObjData(&pContext->texture.proxyRect, pSSM);
    AssertRCReturn(rc, rc);
#endif

    /* Save shared textures */
    CRASSERT(pContext->shared && pContext->shared->textureTable);
    ui32 = crHashtableNumElements(pContext->shared->textureTable);
    rc = SSMR3PutU32(pSSM, ui32);
    AssertRCReturn(rc, rc);
    crHashtableWalk(pContext->shared->textureTable, crStateSaveSharedTextureCB, pSSM);

#ifdef CR_STATE_NO_TEXTURE_IMAGE_STORE
    /* Restore previous texture bindings via diff_api */
    if (ui32)
    {
        CRTextureUnit *pTexUnit;

        pTexUnit = &pContext->texture.unit[pContext->texture.curTextureUnit];

        diff_api.BindTexture(GL_TEXTURE_1D, pTexUnit->currentTexture1D->name);
        diff_api.BindTexture(GL_TEXTURE_2D, pTexUnit->currentTexture2D->name);
        diff_api.BindTexture(GL_TEXTURE_3D, pTexUnit->currentTexture3D->name);
#ifdef CR_ARB_texture_cube_map
        diff_api.BindTexture(GL_TEXTURE_CUBE_MAP_ARB, pTexUnit->currentTextureCubeMap->name);
#endif
#ifdef CR_NV_texture_rectangle
        diff_api.BindTexture(GL_TEXTURE_RECTANGLE_NV, pTexUnit->currentTextureRect->name);
#endif
    }
#endif

    /* Save current texture pointers */
    for (i=0; i<CR_MAX_TEXTURE_UNITS; ++i)
    {
        rc = crStateSaveTexUnitCurrentTexturePtrs(&pContext->texture.unit[i], pSSM);
        AssertRCReturn(rc, rc);
    }

    /* Save lights */
    CRASSERT(pContext->lighting.light);
    rc = SSMR3PutMem(pSSM, pContext->lighting.light, CR_MAX_LIGHTS * sizeof(*pContext->lighting.light));
    AssertRCReturn(rc, rc);

    /* Save attrib stack*/
    /*@todo could go up to used stack depth here?*/
    for ( i = 0 ; i < CR_MAX_ATTRIB_STACK_DEPTH ; i++)
    {
        if (pContext->attrib.enableStack[i].clip)
        {
            rc = SSMR3PutMem(pSSM, pContext->attrib.enableStack[i].clip,
                             pContext->limits.maxClipPlanes*sizeof(GLboolean));
            AssertRCReturn(rc, rc);
        }

        if (pContext->attrib.enableStack[i].light)
        {
            rc = SSMR3PutMem(pSSM, pContext->attrib.enableStack[i].light,
                             pContext->limits.maxLights*sizeof(GLboolean));
            AssertRCReturn(rc, rc);
        }

        if (pContext->attrib.lightingStack[i].light)
        {
            rc = SSMR3PutMem(pSSM, pContext->attrib.lightingStack[i].light,
                             pContext->limits.maxLights*sizeof(CRLight));
            AssertRCReturn(rc, rc);
        }

        for (j=0; j<pContext->limits.maxTextureUnits; ++j)
        {
            rc = crStateSaveTexUnitCurrentTexturePtrs(&pContext->attrib.textureStack[i].unit[j], pSSM);
            AssertRCReturn(rc, rc);
        }

        if (pContext->attrib.transformStack[i].clip)
        {
            rc = SSMR3PutMem(pSSM, pContext->attrib.transformStack[i].clip,
                             pContext->limits.maxClipPlanes*sizeof(GLboolean));
            AssertRCReturn(rc, rc);
        }

        if (pContext->attrib.transformStack[i].clipPlane)
        {
            rc = SSMR3PutMem(pSSM, pContext->attrib.transformStack[i].clipPlane,
                             pContext->limits.maxClipPlanes*sizeof(GLvectord));
            AssertRCReturn(rc, rc);
        }

        rc = crSateSaveEvalCoeffs1D(pContext->attrib.evalStack[i].eval1D, pSSM);
        AssertRCReturn(rc, rc);
        rc = crSateSaveEvalCoeffs2D(pContext->attrib.evalStack[i].eval2D, pSSM);
        AssertRCReturn(rc, rc);
    }

    /* Save evaluator coeffs */
    rc = crSateSaveEvalCoeffs1D(pContext->eval.eval1D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crSateSaveEvalCoeffs2D(pContext->eval.eval2D, pSSM);
    AssertRCReturn(rc, rc);

#ifdef CR_ARB_vertex_buffer_object
    /* Save buffer objects */
    ui32 = crHashtableNumElements(pContext->bufferobject.buffers);
    rc = SSMR3PutU32(pSSM, ui32);
    AssertRCReturn(rc, rc);
    /* Save default one*/
    crStateSaveBufferObjectCB(0, pContext->bufferobject.nullBuffer, pSSM);
    /* Save all the rest */
    crHashtableWalk(pContext->bufferobject.buffers, crStateSaveBufferObjectCB, pSSM);
    /* Restore binding */
    diff_api.BindBufferARB(GL_ARRAY_BUFFER_ARB, pContext->bufferobject.arrayBuffer->name);
    /* Save pointers */
    rc = SSMR3PutU32(pSSM, pContext->bufferobject.arrayBuffer->name);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pContext->bufferobject.elementsBuffer->name);
    AssertRCReturn(rc, rc);
    /* Save bound array buffers*/ /*@todo vertexArrayStack*/
    rc = SSMR3PutU32(pSSM, pContext->client.array.v.buffer->name);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pContext->client.array.c.buffer->name);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pContext->client.array.f.buffer->name);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pContext->client.array.s.buffer->name);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pContext->client.array.e.buffer->name);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pContext->client.array.i.buffer->name);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pContext->client.array.n.buffer->name);
    AssertRCReturn(rc, rc);
    for (i = 0; i < CR_MAX_TEXTURE_UNITS; i++)
    {
        rc = SSMR3PutU32(pSSM, pContext->client.array.t[i].buffer->name);
        AssertRCReturn(rc, rc);
    }
# ifdef CR_NV_vertex_program
    for (i = 0; i < CR_MAX_VERTEX_ATTRIBS; i++)
    {
        rc = SSMR3PutU32(pSSM, pContext->client.array.a[i].buffer->name);
        AssertRCReturn(rc, rc);
    }
# endif
#endif /*CR_ARB_vertex_buffer_object*/

    /* Save pixel/vertex programs */
    ui32 = crHashtableNumElements(pContext->program.programHash);
    rc = SSMR3PutU32(pSSM, ui32);
    AssertRCReturn(rc, rc);
    /* Save defauls programs */
    crStateSaveProgramCB(0, pContext->program.defaultVertexProgram, pSSM);
    crStateSaveProgramCB(0, pContext->program.defaultFragmentProgram, pSSM);
    /* Save all the rest */
    crHashtableWalk(pContext->program.programHash, crStateSaveProgramCB, pSSM);
    /* Save Pointers */
    rc = SSMR3PutU32(pSSM, pContext->program.currentVertexProgram->id);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pContext->program.currentFragmentProgram->id);
    AssertRCReturn(rc, rc);
    /* This one is unused it seems*/
    CRASSERT(!pContext->program.errorString);

#ifdef CR_EXT_framebuffer_object
    /* Save FBOs */
    ui32 = crHashtableNumElements(pContext->framebufferobject.framebuffers);
    rc = SSMR3PutU32(pSSM, ui32);
    AssertRCReturn(rc, rc);
    crHashtableWalk(pContext->framebufferobject.framebuffers, crStateSaveFramebuffersCB, pSSM);
    ui32 = crHashtableNumElements(pContext->framebufferobject.renderbuffers);
    rc = SSMR3PutU32(pSSM, ui32);
    AssertRCReturn(rc, rc);
    crHashtableWalk(pContext->framebufferobject.renderbuffers, crStateSaveRenderbuffersCB, pSSM);
    rc = SSMR3PutU32(pSSM, pContext->framebufferobject.drawFB?pContext->framebufferobject.drawFB->id:0);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pContext->framebufferobject.readFB?pContext->framebufferobject.readFB->id:0);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pContext->framebufferobject.renderbuffer?pContext->framebufferobject.renderbuffer->id:0);
    AssertRCReturn(rc, rc);
#endif

#ifdef CR_OPENGL_VERSION_2_0
    /* Save GLSL related info */
    ui32 = crHashtableNumElements(pContext->glsl.shaders);
    rc = SSMR3PutU32(pSSM, ui32);
    AssertRCReturn(rc, rc);
    crHashtableWalk(pContext->glsl.shaders, crStateSaveGLSLShaderCB, pSSM);
    ui32 = crHashtableNumElements(pContext->glsl.programs);
    rc = SSMR3PutU32(pSSM, ui32);
    AssertRCReturn(rc, rc);
    crHashtableWalk(pContext->glsl.programs, crStateSaveGLSLProgramCB, pSSM);
    rc = SSMR3PutU32(pSSM, pContext->glsl.activeProgram?pContext->glsl.activeProgram->id:0);
    AssertRCReturn(rc, rc);
#endif

    return VINF_SUCCESS;
}

#define SLC_COPYPTR(ptr) pTmpContext->ptr = pContext->ptr
#define SLC_ASSSERT_NULL_PTR(ptr) CRASSERT(!pContext->ptr)

int32_t crStateLoadContext(CRContext *pContext, PSSMHANDLE pSSM)
{
    CRContext* pTmpContext;
    int32_t rc, i, j;
    uint32_t uiNumElems, ui, k;
    unsigned long key;

    CRASSERT(pContext && pSSM);

    /* This one is rather big for stack allocation and causes macs to crash */
    pTmpContext = (CRContext*)crAlloc(sizeof(*pTmpContext));
    if (!pTmpContext)
        return VERR_NO_MEMORY;

    rc = SSMR3GetMem(pSSM, pTmpContext, sizeof(*pTmpContext));
    AssertRCReturn(rc, rc);

    SLC_COPYPTR(shared);
    SLC_COPYPTR(flush_func);
    SLC_COPYPTR(flush_arg);

    /* We're supposed to be loading into an empty context, so those pointers should be NULL */
    for ( i = 0 ; i < CR_MAX_ATTRIB_STACK_DEPTH ; i++)
    {
        SLC_ASSSERT_NULL_PTR(attrib.enableStack[i].clip);
        SLC_ASSSERT_NULL_PTR(attrib.enableStack[i].light);

        SLC_ASSSERT_NULL_PTR(attrib.lightingStack[i].light);
        SLC_ASSSERT_NULL_PTR(attrib.transformStack[i].clip);
        SLC_ASSSERT_NULL_PTR(attrib.transformStack[i].clipPlane);

        for (j=0; j<GLEVAL_TOT; ++j)
        {
            SLC_ASSSERT_NULL_PTR(attrib.evalStack[i].eval1D[j].coeff);
            SLC_ASSSERT_NULL_PTR(attrib.evalStack[i].eval2D[j].coeff);
        }
    }

#ifdef CR_ARB_vertex_buffer_object
    SLC_COPYPTR(bufferobject.buffers);
    SLC_COPYPTR(bufferobject.nullBuffer);
#endif

/*@todo, that should be removed probably as those should hold the offset values, so loading should be fine
  but better check*/
#if 0
#ifdef CR_EXT_compiled_vertex_array
    SLC_COPYPTR(client.array.v.prevPtr);
    SLC_COPYPTR(client.array.c.prevPtr);
    SLC_COPYPTR(client.array.f.prevPtr);
    SLC_COPYPTR(client.array.s.prevPtr);
    SLC_COPYPTR(client.array.e.prevPtr);
    SLC_COPYPTR(client.array.i.prevPtr);
    SLC_COPYPTR(client.array.n.prevPtr);
    for (i = 0 ; i < CR_MAX_TEXTURE_UNITS ; i++)
    {
        SLC_COPYPTR(client.array.t[i].prevPtr);
    }

# ifdef CR_NV_vertex_program
    for (i = 0; i < CR_MAX_VERTEX_ATTRIBS; i++)
    {
        SLC_COPYPTR(client.array.a[i].prevPtr);
    }
# endif
#endif
#endif

#ifdef CR_ARB_vertex_buffer_object
    /*That just sets those pointers to NULL*/
    SLC_COPYPTR(client.array.v.buffer);
    SLC_COPYPTR(client.array.c.buffer);
    SLC_COPYPTR(client.array.f.buffer);
    SLC_COPYPTR(client.array.s.buffer);
    SLC_COPYPTR(client.array.e.buffer);
    SLC_COPYPTR(client.array.i.buffer);
    SLC_COPYPTR(client.array.n.buffer);
    for (i = 0 ; i < CR_MAX_TEXTURE_UNITS ; i++)
    {
        SLC_COPYPTR(client.array.t[i].buffer);
    }
# ifdef CR_NV_vertex_program
    for (i = 0; i < CR_MAX_VERTEX_ATTRIBS; i++)
    {
        SLC_COPYPTR(client.array.a[i].buffer);
    }
# endif
#endif /*CR_ARB_vertex_buffer_object*/

    /*@todo CR_NV_vertex_program*/
    crStateCopyEvalPtrs1D(pTmpContext->eval.eval1D, pContext->eval.eval1D);
    crStateCopyEvalPtrs2D(pTmpContext->eval.eval2D, pContext->eval.eval2D);
    
    SLC_COPYPTR(feedback.buffer);  /*@todo*/
    SLC_COPYPTR(selection.buffer); /*@todo*/

    SLC_COPYPTR(lighting.light);

    /*This one could be tricky if we're loading snapshot on host with different GPU*/
    SLC_COPYPTR(limits.extensions);

#if CR_ARB_occlusion_query
    SLC_COPYPTR(occlusion.objects); /*@todo*/
#endif

    SLC_COPYPTR(program.errorString);
    SLC_COPYPTR(program.programHash);
    SLC_COPYPTR(program.defaultVertexProgram);
    SLC_COPYPTR(program.defaultFragmentProgram);

    /* Texture pointers */
    for (i=0; i<6; ++i)
    {
        SLC_COPYPTR(texture.base1D.level[i]);
        SLC_COPYPTR(texture.base2D.level[i]);
        SLC_COPYPTR(texture.base3D.level[i]);
        SLC_COPYPTR(texture.proxy1D.level[i]);
        SLC_COPYPTR(texture.proxy2D.level[i]);
        SLC_COPYPTR(texture.proxy3D.level[i]);
#ifdef CR_ARB_texture_cube_map
        SLC_COPYPTR(texture.baseCubeMap.level[i]);
        SLC_COPYPTR(texture.proxyCubeMap.level[i]);
#endif
#ifdef CR_NV_texture_rectangle
        SLC_COPYPTR(texture.baseRect.level[i]);
        SLC_COPYPTR(texture.proxyRect.level[i]);
#endif
    }

    /* Load transform state */
    SLC_COPYPTR(transform.clipPlane);
    SLC_COPYPTR(transform.clip);
    /* Don't have to worry about pContext->transform.current as it'd be set in crStateSetCurrent call */
    /*SLC_COPYPTR(transform.currentStack);*/
    SLC_COPYPTR(transform.modelViewStack.stack);
    SLC_COPYPTR(transform.projectionStack.stack);
    SLC_COPYPTR(transform.colorStack.stack);

    for (i = 0 ; i < CR_MAX_TEXTURE_UNITS ; i++)
    {
        SLC_COPYPTR(transform.textureStack[i].stack);
    }
    for (i = 0 ; i < CR_MAX_PROGRAM_MATRICES ; i++)
    {
        SLC_COPYPTR(transform.programStack[i].stack);
    }

#ifdef CR_EXT_framebuffer_object
    SLC_COPYPTR(framebufferobject.framebuffers);
    SLC_COPYPTR(framebufferobject.renderbuffers);
#endif

#ifdef CR_OPENGL_VERSION_2_0
    SLC_COPYPTR(glsl.shaders);
    SLC_COPYPTR(glsl.programs);
#endif

    /* Have to preserve original context id */
    CRASSERT(pTmpContext->id == pContext->id);
    /* Copy ordinary state to real context */
    crMemcpy(pContext, pTmpContext, sizeof(*pTmpContext));
    crFree(pTmpContext);
    pTmpContext = NULL;

    /* Now deal with pointers */

    /* Load transform state */
    rc = SSMR3GetMem(pSSM, pContext->transform.clipPlane, sizeof(GLvectord)*CR_MAX_CLIP_PLANES);
    AssertRCReturn(rc, rc);
    rc = SSMR3GetMem(pSSM, pContext->transform.clip, sizeof(GLboolean)*CR_MAX_CLIP_PLANES);
    AssertRCReturn(rc, rc);
    rc = crStateLoadMatrixStack(&pContext->transform.modelViewStack, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateLoadMatrixStack(&pContext->transform.projectionStack, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateLoadMatrixStack(&pContext->transform.colorStack, pSSM);
    AssertRCReturn(rc, rc);
    for (i = 0 ; i < CR_MAX_TEXTURE_UNITS ; i++)
    {
        rc = crStateLoadMatrixStack(&pContext->transform.textureStack[i], pSSM);
        AssertRCReturn(rc, rc);
    }
    for (i = 0 ; i < CR_MAX_PROGRAM_MATRICES ; i++)
    {
        rc = crStateLoadMatrixStack(&pContext->transform.programStack[i], pSSM);
        AssertRCReturn(rc, rc);
    }

    /* Load Textures */
    rc = crStateLoadTextureObjData(&pContext->texture.base1D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateLoadTextureObjData(&pContext->texture.base2D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateLoadTextureObjData(&pContext->texture.base3D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateLoadTextureObjData(&pContext->texture.proxy1D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateLoadTextureObjData(&pContext->texture.proxy2D, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateLoadTextureObjData(&pContext->texture.proxy3D, pSSM);
    AssertRCReturn(rc, rc);
#ifdef CR_ARB_texture_cube_map
    rc = crStateLoadTextureObjData(&pContext->texture.baseCubeMap, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateLoadTextureObjData(&pContext->texture.proxyCubeMap, pSSM);
    AssertRCReturn(rc, rc);
#endif
#ifdef CR_NV_texture_rectangle
    rc = crStateLoadTextureObjData(&pContext->texture.baseRect, pSSM);
    AssertRCReturn(rc, rc);
    rc = crStateLoadTextureObjData(&pContext->texture.proxyRect, pSSM);
    AssertRCReturn(rc, rc);
#endif

    /* Load shared textures */
    CRASSERT(pContext->shared && pContext->shared->textureTable);
    rc = SSMR3GetU32(pSSM, &uiNumElems);
    AssertRCReturn(rc, rc);
    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRTextureObj *pTexture;

        rc = SSMR3GetMem(pSSM, &key, sizeof(key));
        AssertRCReturn(rc, rc);

        pTexture = (CRTextureObj *) crCalloc(sizeof(CRTextureObj));
        if (!pTexture) return VERR_NO_MEMORY;

        rc = SSMR3GetMem(pSSM, pTexture, sizeof(*pTexture));
        AssertRCReturn(rc, rc);

        //DIRTY(pTexture->dirty, pContext->neg_bitid);
        //DIRTY(pTexture->imageBit, pContext->neg_bitid);

        /*allocate actual memory*/
        for (i=0; i<6; ++i) {
            pTexture->level[i] = (CRTextureLevel *) crCalloc(sizeof(CRTextureLevel) * MAX_MIPMAP_LEVELS);
            if (!pTexture->level[i]) return VERR_NO_MEMORY;
        }

        rc = crStateLoadTextureObjData(pTexture, pSSM);
        AssertRCReturn(rc, rc);

        crHashtableAdd(pContext->shared->textureTable, key, pTexture);
    }

    /* Load current texture pointers */
    for (i=0; i<CR_MAX_TEXTURE_UNITS; ++i)
    {
        rc = crStateLoadTexUnitCurrentTexturePtrs(&pContext->texture.unit[i], pContext, pSSM);
        AssertRCReturn(rc, rc);
    }
    //FILLDIRTY(GetCurrentBits()->texture.dirty);

    /* Mark textures for resending to GPU */
    pContext->texture.bResyncNeeded = GL_TRUE;

    /* Load lights */
    CRASSERT(pContext->lighting.light);
    rc = SSMR3GetMem(pSSM, pContext->lighting.light, CR_MAX_LIGHTS * sizeof(*pContext->lighting.light));
    AssertRCReturn(rc, rc);

    /* Load attrib stack*/
    for ( i = 0 ; i < CR_MAX_ATTRIB_STACK_DEPTH ; i++)
    {
        if (pContext->attrib.enableStack[i].clip)
        {
            rc = crStateAllocAndSSMR3GetMem(pSSM, (void**)&pContext->attrib.enableStack[i].clip,
                                            pContext->limits.maxClipPlanes*sizeof(GLboolean));
            AssertRCReturn(rc, rc);
        }

        if (pContext->attrib.enableStack[i].light)
        {
            rc = crStateAllocAndSSMR3GetMem(pSSM, (void**)&pContext->attrib.enableStack[i].light,
                                            pContext->limits.maxLights*sizeof(GLboolean));
            AssertRCReturn(rc, rc);
        }

        if (pContext->attrib.lightingStack[i].light)
        {
            rc = crStateAllocAndSSMR3GetMem(pSSM, (void**)&pContext->attrib.lightingStack[i].light,
                                            pContext->limits.maxLights*sizeof(CRLight));
            AssertRCReturn(rc, rc);
        }

        for (k=0; k<pContext->limits.maxTextureUnits; ++k)
        {
            rc = crStateLoadTexUnitCurrentTexturePtrs(&pContext->attrib.textureStack[i].unit[k], pContext, pSSM);
            AssertRCReturn(rc, rc);
        }

        if (pContext->attrib.transformStack[i].clip)
        {
            rc = crStateAllocAndSSMR3GetMem(pSSM, (void*)&pContext->attrib.transformStack[i].clip,
                                            pContext->limits.maxClipPlanes*sizeof(GLboolean));
            AssertRCReturn(rc, rc);
        }

        if (pContext->attrib.transformStack[i].clipPlane)
        {
            rc = crStateAllocAndSSMR3GetMem(pSSM, (void**)&pContext->attrib.transformStack[i].clipPlane,
                                            pContext->limits.maxClipPlanes*sizeof(GLvectord));
            AssertRCReturn(rc, rc);
        }
        rc = crSateLoadEvalCoeffs1D(pContext->attrib.evalStack[i].eval1D, GL_TRUE, pSSM);
        AssertRCReturn(rc, rc);
        rc = crSateLoadEvalCoeffs2D(pContext->attrib.evalStack[i].eval2D, GL_TRUE, pSSM);
        AssertRCReturn(rc, rc);
    }

    /* Load evaluator coeffs */
    rc = crSateLoadEvalCoeffs1D(pContext->eval.eval1D, GL_FALSE, pSSM);
    AssertRCReturn(rc, rc);
    rc = crSateLoadEvalCoeffs2D(pContext->eval.eval2D, GL_FALSE, pSSM);
    AssertRCReturn(rc, rc);

    /* Load buffer objects */
#ifdef CR_ARB_vertex_buffer_object
    rc = SSMR3GetU32(pSSM, &uiNumElems);
    AssertRCReturn(rc, rc);
    for (ui=0; ui<=uiNumElems; ++ui) /*ui<=uiNumElems to load nullBuffer in same loop*/
    {
        CRBufferObject *pBufferObj;

        rc = SSMR3GetMem(pSSM, &key, sizeof(key));
        AssertRCReturn(rc, rc);

        /* default one should be already allocated */
        if (key==0)
        {
            pBufferObj = pContext->bufferobject.nullBuffer;
            if (!pBufferObj) return VERR_SSM_UNEXPECTED_DATA;
        }
        else
        {
            pBufferObj = (CRBufferObject *) crCalloc(sizeof(*pBufferObj));
            if (!pBufferObj) return VERR_NO_MEMORY;
        }

        rc = SSMR3GetMem(pSSM, pBufferObj, sizeof(*pBufferObj));
        AssertRCReturn(rc, rc);

        if (pBufferObj->data)
        {
            CRASSERT(pBufferObj->size>0);
            pBufferObj->data = crAlloc(pBufferObj->size);
            rc = SSMR3GetMem(pSSM, pBufferObj->data, pBufferObj->size);
            AssertRCReturn(rc, rc);

            //DIRTY(pBufferObj->dirty, pContext->neg_bitid);
            //pBufferObj->dirtyStart = 0;
            //pBufferObj->dirtyLength = pBufferObj->size;
        } 
        else if (pBufferObj->name!=0 && pBufferObj->size>0)
        {
            rc = SSMR3GetMem(pSSM, &pBufferObj->data, sizeof(pBufferObj->data));
            AssertRCReturn(rc, rc);

            if (pBufferObj->data)
            {
                pBufferObj->data = crAlloc(pBufferObj->size);
                rc = SSMR3GetMem(pSSM, pBufferObj->data, pBufferObj->size);
                AssertRCReturn(rc, rc);
            }
        }


        if (key!=0)
            crHashtableAdd(pContext->bufferobject.buffers, key, pBufferObj);        
    }
    //FILLDIRTY(GetCurrentBits()->bufferobject.dirty);
    /* Load pointers */
#define CRS_GET_BO(name) (((name)==0) ? (pContext->bufferobject.nullBuffer) : crHashtableSearch(pContext->bufferobject.buffers, name))
    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->bufferobject.arrayBuffer = CRS_GET_BO(ui);
    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->bufferobject.elementsBuffer = CRS_GET_BO(ui);

    /* Load bound array buffers*/
    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->client.array.v.buffer = CRS_GET_BO(ui);

    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->client.array.c.buffer = CRS_GET_BO(ui);

    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->client.array.f.buffer = CRS_GET_BO(ui);

    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->client.array.s.buffer = CRS_GET_BO(ui);

    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->client.array.e.buffer = CRS_GET_BO(ui);

    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->client.array.i.buffer = CRS_GET_BO(ui);

    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->client.array.n.buffer = CRS_GET_BO(ui);

    for (i = 0; i < CR_MAX_TEXTURE_UNITS; i++)
    {
        rc = SSMR3GetU32(pSSM, &ui);
        AssertRCReturn(rc, rc);
        pContext->client.array.t[i].buffer = CRS_GET_BO(ui);
    }
# ifdef CR_NV_vertex_program
    for (i = 0; i < CR_MAX_VERTEX_ATTRIBS; i++)
    {
        rc = SSMR3GetU32(pSSM, &ui);
        AssertRCReturn(rc, rc);
        pContext->client.array.a[i].buffer = CRS_GET_BO(ui);
    }
# endif
#undef CRS_GET_BO

    pContext->bufferobject.bResyncNeeded = GL_TRUE;
#endif

    /* Load pixel/vertex programs */
    rc = SSMR3GetU32(pSSM, &uiNumElems);
    AssertRCReturn(rc, rc);
    /* Load defauls programs */
    rc = crStateLoadProgram(&pContext->program.defaultVertexProgram, pSSM);
    AssertRCReturn(rc, rc);
    //FILLDIRTY(pContext->program.defaultVertexProgram->dirtyProgram);
    rc = crStateLoadProgram(&pContext->program.defaultFragmentProgram, pSSM);
    AssertRCReturn(rc, rc);
    //FILLDIRTY(pContext->program.defaultFragmentProgram->dirtyProgram);
    /* Load all the rest */
    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRProgram *pProgram = NULL;
        rc = crStateLoadProgram(&pProgram, pSSM);
        AssertRCReturn(rc, rc);
        crHashtableAdd(pContext->program.programHash, pProgram->id, pProgram);
        //DIRTY(pProgram->dirtyProgram, pContext->neg_bitid);
        
    }
    //FILLDIRTY(GetCurrentBits()->program.dirty);
    /* Load Pointers */
    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->program.currentVertexProgram = ui==0 ? pContext->program.defaultVertexProgram
                                                     : crHashtableSearch(pContext->program.programHash, ui);
    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->program.currentFragmentProgram = ui==0 ? pContext->program.defaultFragmentProgram
                                                       : crHashtableSearch(pContext->program.programHash, ui);

    /* Mark programs for resending to GPU */
    pContext->program.bResyncNeeded = GL_TRUE;

#ifdef CR_EXT_framebuffer_object
    /* Load FBOs */
    rc = SSMR3GetU32(pSSM, &uiNumElems);
    AssertRCReturn(rc, rc);
    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRFramebufferObject *pFBO;
        pFBO = crAlloc(sizeof(*pFBO));
        if (!pFBO) return VERR_NO_MEMORY;

        rc = SSMR3GetMem(pSSM, &key, sizeof(key));
        AssertRCReturn(rc, rc);

        rc = SSMR3GetMem(pSSM, pFBO, sizeof(*pFBO));
        AssertRCReturn(rc, rc);

        crHashtableAdd(pContext->framebufferobject.framebuffers, key, pFBO);
    }

    rc = SSMR3GetU32(pSSM, &uiNumElems);
    AssertRCReturn(rc, rc);
    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRRenderbufferObject *pRBO;
        pRBO = crAlloc(sizeof(*pRBO));
        if (!pRBO) return VERR_NO_MEMORY;

        rc = SSMR3GetMem(pSSM, &key, sizeof(key));
        AssertRCReturn(rc, rc);

        rc = SSMR3GetMem(pSSM, pRBO, sizeof(*pRBO));
        AssertRCReturn(rc, rc);

        crHashtableAdd(pContext->framebufferobject.renderbuffers, key, pRBO);
    }

    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->framebufferobject.drawFB = ui==0 ? NULL 
                                               : crHashtableSearch(pContext->framebufferobject.framebuffers, ui);

    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->framebufferobject.readFB = ui==0 ? NULL 
                                               : crHashtableSearch(pContext->framebufferobject.framebuffers, ui);

    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->framebufferobject.renderbuffer = ui==0 ? NULL 
                                                     : crHashtableSearch(pContext->framebufferobject.renderbuffers, ui);

    /* Mark FBOs/RBOs for resending to GPU */
    pContext->framebufferobject.bResyncNeeded = GL_TRUE;
#endif

#ifdef CR_OPENGL_VERSION_2_0
    /* Load GLSL related info */
    rc = SSMR3GetU32(pSSM, &uiNumElems);
    AssertRCReturn(rc, rc);

    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRGLSLShader *pShader = crStateLoadGLSLShader(pSSM);
        if (!pShader) return VERR_SSM_UNEXPECTED_DATA;
        crHashtableAdd(pContext->glsl.shaders, pShader->id, pShader);
    }

    rc = SSMR3GetU32(pSSM, &uiNumElems);
    AssertRCReturn(rc, rc);

    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRGLSLProgram *pProgram;
        uint32_t numShaders;

        pProgram = crAlloc(sizeof(*pProgram));
        if (!pProgram) return VERR_NO_MEMORY;

        rc = SSMR3GetMem(pSSM, &key, sizeof(key));
        AssertRCReturn(rc, rc);

        rc = SSMR3GetMem(pSSM, pProgram, sizeof(*pProgram));
        AssertRCReturn(rc, rc);

        crHashtableAdd(pContext->glsl.programs, key, pProgram);

        pProgram->currentState.attachedShaders = crAllocHashtable();

        rc = SSMR3GetU32(pSSM, &numShaders);
        AssertRCReturn(rc, rc);

        for (k=0; k<numShaders; ++k)
        {
            rc = SSMR3GetMem(pSSM, &key, sizeof(key));
            AssertRCReturn(rc, rc);
            crHashtableAdd(pProgram->currentState.attachedShaders, key, crHashtableSearch(pContext->glsl.shaders, key));
        }

        if (pProgram->activeState.attachedShaders)  
        {
            pProgram->activeState.attachedShaders = crAllocHashtable();

            rc = SSMR3GetU32(pSSM, &numShaders);
            AssertRCReturn(rc, rc);

            for (k=0; k<numShaders; ++k)
            {
                CRGLSLShader *pShader = crStateLoadGLSLShader(pSSM);
                if (!pShader) return VERR_SSM_UNEXPECTED_DATA;
                crHashtableAdd(pProgram->activeState.attachedShaders, pShader->id, pShader);
            }
        }

        if (pProgram->currentState.cAttribs) 
            pProgram->currentState.pAttribs = (CRGLSLAttrib*) crAlloc(pProgram->currentState.cAttribs*sizeof(CRGLSLAttrib));
        for (k=0; k<pProgram->currentState.cAttribs; ++k)
        {
            rc = SSMR3GetMem(pSSM, &pProgram->currentState.pAttribs[k].index, sizeof(pProgram->currentState.pAttribs[k].index));
            AssertRCReturn(rc, rc);
            pProgram->currentState.pAttribs[k].name = crStateLoadString(pSSM);
        }

        if (pProgram->activeState.cAttribs) 
            pProgram->activeState.pAttribs = (CRGLSLAttrib*) crAlloc(pProgram->activeState.cAttribs*sizeof(CRGLSLAttrib));
        for (k=0; k<pProgram->activeState.cAttribs; ++k)
        {
            rc = SSMR3GetMem(pSSM, &pProgram->activeState.pAttribs[k].index, sizeof(pProgram->activeState.pAttribs[k].index));
            AssertRCReturn(rc, rc);
            pProgram->activeState.pAttribs[k].name = crStateLoadString(pSSM);
        }

        rc = SSMR3GetS32(pSSM, &pProgram->cUniforms);
        AssertRCReturn(rc, rc);

        if (pProgram->cUniforms)
        {
            pProgram->pUniforms = crAlloc(pProgram->cUniforms*sizeof(CRGLSLUniform));
            if (!pProgram) return VERR_NO_MEMORY;

            for (k=0; k<pProgram->cUniforms; ++k)
            {
                size_t itemsize, datasize;

                rc = SSMR3GetMem(pSSM, &pProgram->pUniforms[k].type, sizeof(GLenum));
                pProgram->pUniforms[k].name = crStateLoadString(pSSM);

                if (crStateIsIntUniform(pProgram->pUniforms[k].type))
                {
                    itemsize = sizeof(GLint);
                } else itemsize = sizeof(GLfloat);

                datasize = crStateGetUniformSize(pProgram->pUniforms[k].type)*itemsize;
                pProgram->pUniforms[k].data = crAlloc(datasize);
                if (!pProgram->pUniforms[k].data) return VERR_NO_MEMORY;

                rc = SSMR3GetMem(pSSM, pProgram->pUniforms[k].data, datasize);
            }
        }
    }

    rc = SSMR3GetU32(pSSM, &ui);
    AssertRCReturn(rc, rc);
    pContext->glsl.activeProgram = ui==0 ? NULL
                                         : crHashtableSearch(pContext->glsl.programs, ui);

    /*Mark for resending to GPU*/
    pContext->glsl.bResyncNeeded = GL_TRUE;
#endif

    return VINF_SUCCESS;
}
