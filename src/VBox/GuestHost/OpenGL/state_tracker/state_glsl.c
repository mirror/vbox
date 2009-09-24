/* $Id$ */

/** @file
 * VBox OpenGL: GLSL state tracking
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include "state/cr_statefuncs.h"
#include "state_internals.h"
#include "cr_mem.h"
#include "cr_string.h"

static CRGLSLShader* crStateGetShaderObj(GLuint id)
{
    CRContext *g = GetCurrentContext();

    return !g ? NULL : (CRGLSLShader *) crHashtableSearch(g->glsl.shaders, id);
}

static CRGLSLProgram* crStateGetProgramObj(GLuint id)
{
    CRContext *g = GetCurrentContext();

    return !g ? NULL : (CRGLSLProgram *) crHashtableSearch(g->glsl.programs, id);
}

static void crStateFreeGLSLShader(void *data)
{
    CRGLSLShader* pShader = (CRGLSLShader *) data;

    if (pShader->source)
        crFree(pShader->source);

    crFree(pShader);
}

static void crStateFreeProgramAttribs(CRGLSLProgram* pProgram)
{
    GLuint i;

    for (i=0; i<pProgram->activeState.cAttribs; ++i)
    {
        crFree(pProgram->activeState.pAttribs[i].name);
    }

    for (i=0; i<pProgram->currentState.cAttribs; ++i)
    {
        crFree(pProgram->currentState.pAttribs[i].name);
    }

    if (pProgram->activeState.pAttribs)
        crFree(pProgram->activeState.pAttribs);

    if (pProgram->currentState.pAttribs)
        crFree(pProgram->currentState.pAttribs);
}

static void crStateFreeGLSLProgram(void *data)
{
    CRGLSLProgram* pProgram = (CRGLSLProgram *) data;

    if (pProgram->activeState.attachedShaders)
    {
        crFreeHashtable(pProgram->activeState.attachedShaders, crStateFreeGLSLShader);
    }

    crFreeHashtable(pProgram->currentState.attachedShaders, NULL);

    crStateFreeProgramAttribs(pProgram);

    if (pProgram->pUniforms) crFree(pProgram->pUniforms);

    crFree(pProgram);
}

DECLEXPORT(void) STATE_APIENTRY crStateGLSLInit(CRContext *ctx)
{
    ctx->glsl.shaders = crAllocHashtable();
    ctx->glsl.programs = crAllocHashtable();
    ctx->glsl.activeProgram = NULL;
    ctx->glsl.bResyncNeeded = GL_FALSE;

    if (!ctx->glsl.shaders || !ctx->glsl.programs)
    {
        crWarning("crStateGLSLInit: Out of memory!");
        return;
    }
}

DECLEXPORT(void) STATE_APIENTRY crStateGLSLDestroy(CRContext *ctx)
{
    crFreeHashtable(ctx->glsl.programs, crStateFreeGLSLProgram);
    crFreeHashtable(ctx->glsl.shaders, crStateFreeGLSLShader);
}

DECLEXPORT(GLuint) STATE_APIENTRY crStateGetShaderHWID(GLuint id)
{
    CRGLSLShader *pShader = crStateGetShaderObj(id);
    return pShader ? pShader->hwid : 0;
}

DECLEXPORT(GLuint) STATE_APIENTRY crStateGetProgramHWID(GLuint id)
{
    CRGLSLProgram *pProgram = crStateGetProgramObj(id);
    return pProgram ? pProgram->hwid : 0;
}

typedef struct _crCheckIDHWID {
    GLuint id, hwid;
} crCheckIDHWID_t;

static void crStateCheckShaderHWIDCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLShader *pShader = (CRGLSLShader *) data1;
    crCheckIDHWID_t *pParms = (crCheckIDHWID_t*) data2;
    (void) key;

    if (pShader->hwid==pParms->hwid)
        pParms->id = pShader->id;
}

static void crStateCheckProgramHWIDCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLProgram *pProgram = (CRGLSLProgram *) data1;
    crCheckIDHWID_t *pParms = (crCheckIDHWID_t*) data2;
    (void) key;

    if (pProgram->hwid==pParms->hwid)
        pParms->id = pProgram->id;
}

DECLEXPORT(GLuint) STATE_APIENTRY crStateGLSLShaderHWIDtoID(GLuint hwid)
{
    CRContext *g = GetCurrentContext();
    crCheckIDHWID_t parms;

    parms.id = hwid;
    parms.hwid = hwid;

    crHashtableWalk(g->glsl.shaders, crStateCheckShaderHWIDCB, &parms);
    return parms.id;
}

DECLEXPORT(GLuint) STATE_APIENTRY crStateGLSLProgramHWIDtoID(GLuint hwid)
{
    CRContext *g = GetCurrentContext();
    crCheckIDHWID_t parms;

    parms.id = hwid;
    parms.hwid = hwid;

    crHashtableWalk(g->glsl.programs, crStateCheckProgramHWIDCB, &parms);
    return parms.id;
}

DECLEXPORT(void) STATE_APIENTRY crStateCreateShader(GLuint id, GLenum type)
{
    CRGLSLShader *pShader;
    CRContext *g = GetCurrentContext();

    CRASSERT(!crStateGetShaderObj(id));

    pShader = (CRGLSLShader *) crAlloc(sizeof(*pShader));
    if (!pShader)
    {
        crWarning("crStateCreateShader: Out of memory!");
        return;
    }

    pShader->id = id;
    pShader->hwid = id;
    pShader->type = type;
    pShader->source = NULL;
    pShader->compiled = GL_FALSE;
    pShader->deleted = GL_FALSE;
    pShader->refCount = 0;

    crHashtableAdd(g->glsl.shaders, id, pShader);
}

DECLEXPORT(void) STATE_APIENTRY crStateCreateProgram(GLuint id)
{
    CRGLSLProgram *pProgram;
    CRContext *g = GetCurrentContext();

    CRASSERT(!crStateGetProgramObj(id));

    pProgram = (CRGLSLProgram *) crAlloc(sizeof(*pProgram));
    if (!pProgram)
    {
        crWarning("crStateCreateShader: Out of memory!");
        return;
    }

    pProgram->id = id;
    pProgram->hwid = id;
    pProgram->validated = GL_FALSE;
    pProgram->linked = GL_FALSE;
    pProgram->deleted = GL_FALSE;
    pProgram->activeState.attachedShaders = NULL;
    pProgram->currentState.attachedShaders = crAllocHashtable();

    pProgram->activeState.cAttribs = 0;
    pProgram->activeState.pAttribs = NULL;
    pProgram->currentState.cAttribs = 0;
    pProgram->currentState.pAttribs = NULL;

    pProgram->pUniforms = NULL;
    pProgram->cUniforms = 0;

    crHashtableAdd(g->glsl.programs, id, pProgram);
}

DECLEXPORT(void) STATE_APIENTRY crStateCompileShader(GLuint shader)
{
    CRGLSLShader *pShader = crStateGetShaderObj(shader);
    if (!pShader)
    {
        crWarning("Unknown shader %d", shader);
        return;
    }

    pShader->compiled = GL_TRUE;
}

DECLEXPORT(void) STATE_APIENTRY crStateDeleteShader(GLuint shader)
{
    CRGLSLShader *pShader = crStateGetShaderObj(shader);
    if (!pShader)
    {
        crWarning("Unknown shader %d", shader);
        return;
    }

    pShader->deleted = GL_TRUE;

    if (0==pShader->refCount)
    {
        CRContext *g = GetCurrentContext();
        crHashtableDelete(g->glsl.shaders, shader, crStateFreeGLSLShader);
    }
}

DECLEXPORT(void) STATE_APIENTRY crStateAttachShader(GLuint program, GLuint shader)
{
    CRGLSLProgram *pProgram = crStateGetProgramObj(program);
    CRGLSLShader *pShader;

    if (!pProgram)
    {
        crWarning("Unknown program %d", program);
        return;
    }

    if (crHashtableSearch(pProgram->currentState.attachedShaders, shader))
    {
        /*shader already attached to this program*/
        return;
    }

    pShader = crStateGetShaderObj(shader);

    if (!pShader)
    {
        crWarning("Unknown shader %d", shader);
        return;
    }

    pShader->refCount++;

    crHashtableAdd(pProgram->currentState.attachedShaders, shader, pShader);
}

DECLEXPORT(void) STATE_APIENTRY crStateDetachShader(GLuint program, GLuint shader)
{
    CRGLSLProgram *pProgram = crStateGetProgramObj(program);
    CRGLSLShader *pShader;

    if (!pProgram)
    {
        crWarning("Unknown program %d", program);
        return;
    }

    pShader = (CRGLSLShader *) crHashtableSearch(pProgram->currentState.attachedShaders, shader);
    if (!pShader)
    {
        crWarning("Shader %d isn't attached to program %d", shader, program);
        return;
    }

    crHashtableDelete(pProgram->currentState.attachedShaders, shader, NULL);

    CRASSERT(pShader->refCount>0);
    pShader->refCount--;

    if (0==pShader->refCount)
    {
        CRContext *g = GetCurrentContext();
        crHashtableDelete(g->glsl.shaders, shader, crStateFreeGLSLShader);
    }
}

DECLEXPORT(void) STATE_APIENTRY crStateUseProgram(GLuint program)
{
    CRContext *g = GetCurrentContext();

    if (program>0)
    {
        CRGLSLProgram *pProgram = crStateGetProgramObj(program);

        if (!pProgram)
        {
            crWarning("Unknown program %d", program);
            return;
        }

        g->glsl.activeProgram = pProgram;
    }
    else
    {
        g->glsl.activeProgram = NULL;
    }
}

static void crStateShaderDecRefCount(void *data)
{
    CRGLSLShader *pShader = (CRGLSLShader *) data;

    CRASSERT(pShader->refCount>0);

    pShader->refCount--;

    if (0==pShader->refCount && pShader->deleted)
    {
        CRContext *g = GetCurrentContext();
        crHashtableDelete(g->glsl.shaders, pShader->id, crStateFreeGLSLShader);
    }
}

static void crStateFakeDecRefCountCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLShader *pShader = (CRGLSLShader *) data1;
    CRContext *ctx = (CRContext*) data2;
    CRGLSLShader *pRealShader;
    (void) key;

    pRealShader = crStateGetShaderObj(pShader->id);
    CRASSERT(pRealShader);
    crStateShaderDecRefCount(pRealShader);
}

DECLEXPORT(void) STATE_APIENTRY crStateDeleteProgram(GLuint program)
{
    CRContext *g = GetCurrentContext();
    CRGLSLProgram *pProgram = crStateGetProgramObj(program);

    if (!pProgram)
    {
        crWarning("Unknown program %d", program);
        return;
    }

    if (g->glsl.activeProgram == pProgram)
    {
        g->glsl.activeProgram = NULL;
    }

    crFreeHashtable(pProgram->currentState.attachedShaders, crStateShaderDecRefCount);

    if (pProgram->activeState.attachedShaders)
    {
        crHashtableWalk(pProgram->activeState.attachedShaders, crStateFakeDecRefCountCB, g);
    }

    crHashtableDelete(g->glsl.programs, program, crStateFreeGLSLProgram);
}

DECLEXPORT(void) STATE_APIENTRY crStateValidateProgram(GLuint program)
{
    CRGLSLProgram *pProgram = crStateGetProgramObj(program);

    if (!pProgram)
    {
        crWarning("Unknown program %d", program);
        return;
    }

    pProgram->validated = GL_TRUE;
}

static void crStateCopyShaderCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLShader *pRealShader = (CRGLSLShader *) data1;
    CRGLSLProgram *pProgram = (CRGLSLProgram *) data2;
    CRGLSLShader *pShader;
    GLint sLen=0;

    pRealShader->refCount++;

    pShader = (CRGLSLShader *) crAlloc(sizeof(*pShader));
    if (!pShader)
    {
        crWarning("crStateCopyShaderCB: Out of memory!");
        return;
    }

    crMemcpy(pShader, pRealShader, sizeof(*pShader));

    diff_api.GetShaderiv(pShader->hwid, GL_SHADER_SOURCE_LENGTH, &sLen);
    if (sLen>0)
    {
        pShader->source = (GLchar*) crAlloc(sLen);
        diff_api.GetShaderSource(pShader->hwid, sLen, NULL, pShader->source);
    }

    crHashtableAdd(pProgram->activeState.attachedShaders, key, pShader);
}

DECLEXPORT(void) STATE_APIENTRY crStateLinkProgram(GLuint program)
{
    CRGLSLProgram *pProgram = crStateGetProgramObj(program);
    GLuint i;

    if (!pProgram)
    {
        crWarning("Unknown program %d", program);
        return;
    }

    pProgram->linked = GL_TRUE;

    /*Free program's active state*/
    if (pProgram->activeState.attachedShaders)
    {
        crHashtableWalk(pProgram->activeState.attachedShaders, crStateFakeDecRefCountCB, NULL);
        crFreeHashtable(pProgram->activeState.attachedShaders, crStateFreeGLSLShader);
        pProgram->activeState.attachedShaders = NULL;
    }
    for (i=0; i<pProgram->activeState.cAttribs; ++i)
    {
        crFree(pProgram->activeState.pAttribs[i].name);
    }
    if (pProgram->activeState.pAttribs) crFree(pProgram->activeState.pAttribs);

    /*copy current state to active state*/
    crMemcpy(&pProgram->activeState, &pProgram->currentState, sizeof(CRGLSLProgramState));

    pProgram->activeState.attachedShaders = crAllocHashtable();
    if (!pProgram->activeState.attachedShaders)
    {
        crWarning("crStateLinkProgram: Out of memory!");
        return;
    }
    crHashtableWalk(pProgram->currentState.attachedShaders, crStateCopyShaderCB, pProgram);

    if (pProgram->activeState.pAttribs)
    {
        pProgram->activeState.pAttribs = (CRGLSLAttrib *) crAlloc(pProgram->activeState.cAttribs * sizeof(CRGLSLAttrib));
    }

    for (i=0; i<pProgram->activeState.cAttribs; ++i)
    {
        crMemcpy(&pProgram->activeState.pAttribs[i], &pProgram->currentState.pAttribs[i], sizeof(CRGLSLAttrib));
        pProgram->activeState.pAttribs[i].name = crStrdup(pProgram->currentState.pAttribs[i].name);
    }
}

DECLEXPORT(void) STATE_APIENTRY crStateBindAttribLocation(GLuint program, GLuint index, const char * name)
{
    CRGLSLProgram *pProgram = crStateGetProgramObj(program);
    GLuint i;
    CRGLSLAttrib *pAttribs;

    if (!pProgram)
    {
        crWarning("Unknown program %d", program);
        return;
    }

    if (index>=CR_MAX_VERTEX_ATTRIBS)
    {
        crWarning("crStateBindAttribLocation: Index too big %d", index);
        return;
    }

    for (i=0; i<pProgram->currentState.cAttribs; ++i)
    {
        if (!crStrcmp(pProgram->currentState.pAttribs[i].name, name))
        {
            crFree(pProgram->currentState.pAttribs[i].name);
            pProgram->currentState.pAttribs[i].name = crStrdup(name);
            return;
        }
    }

    pAttribs = (CRGLSLAttrib*) crAlloc((pProgram->currentState.cAttribs+1)*sizeof(CRGLSLAttrib));
    if (!pAttribs)
    {
        crWarning("crStateBindAttribLocation: Out of memory!");
        return;
    }

    crMemcpy(&pAttribs[0], &pProgram->currentState.pAttribs[0], pProgram->currentState.cAttribs*sizeof(CRGLSLAttrib));
    pAttribs[pProgram->currentState.cAttribs].index = index;
    pAttribs[pProgram->currentState.cAttribs].name = crStrdup(name);
    
    pProgram->currentState.cAttribs++;
    if (pProgram->currentState.pAttribs) crFree(pProgram->currentState.pAttribs);
    pProgram->currentState.pAttribs = pAttribs;
}

DECLEXPORT(GLint) STATE_APIENTRY crStateGetUniformSize(GLenum type)
{
    GLint size;

    switch (type)
    {
        case GL_FLOAT:
            size = 1;
            break;
        case GL_FLOAT_VEC2:
            size = 2;
            break;
        case GL_FLOAT_VEC3:
            size = 3;
            break;
        case GL_FLOAT_VEC4:
            size = 4;
            break;
        case GL_INT:
            size = 1;
            break;
        case GL_INT_VEC2:
            size = 2;
            break;
        case GL_INT_VEC3:
            size = 3;
            break;
        case GL_INT_VEC4:
            size = 4;
            break;
        case GL_BOOL:
            size = 1;
            break;
        case GL_BOOL_VEC2:
            size = 2;
            break;
        case GL_BOOL_VEC3:
            size = 3;
            break;
        case GL_BOOL_VEC4:
            size = 4;
            break;
        case GL_FLOAT_MAT2:
            size = 8;
            break;
        case GL_FLOAT_MAT3:
            size = 12;
            break;
        case GL_FLOAT_MAT4:
            size = 16;
            break;
        case GL_SAMPLER_1D:
        case GL_SAMPLER_2D:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_1D_SHADOW:
        case GL_SAMPLER_2D_SHADOW:
            size = 1;
            break;
#ifdef CR_OPENGL_VERSION_2_1
        case GL_FLOAT_MAT2x3:
            size = 8;
            break;
        case GL_FLOAT_MAT2x4:
            size = 8;
            break;
        case GL_FLOAT_MAT3x2:
            size = 12;
            break;
        case GL_FLOAT_MAT3x4:
            size = 12;
            break;
        case GL_FLOAT_MAT4x2:
            size = 16;
            break;
        case GL_FLOAT_MAT4x3:
            size = 16;
            break;
#endif
        default:
            crWarning("crStateGetUniformSize: unknown uniform type 0x%x", (GLint)type);
            size = 16;
            break;
    }

    return size;
}

static void crStateGLSLCreateShadersCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLShader *pShader = (CRGLSLShader*) data1;
    CRContext *ctx = (CRContext *) data2;

    pShader->hwid = diff_api.CreateShader(pShader->type);
}

static void crStateFixAttachedShaderHWIDsCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLShader *pShader = (CRGLSLShader*) data1;
    CRGLSLShader *pRealShader;
    CRContext *pCtx = (CRContext *) data2;

    pRealShader = (CRGLSLShader *) crHashtableSearch(pCtx->glsl.shaders, key);
    CRASSERT(pRealShader);

    pShader->hwid = pRealShader->hwid;
}

static void crStateGLSLSyncShadersCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLShader *pShader = (CRGLSLShader*) data1;
    (void) key;
    (void) data2;

    if (pShader->source)
    {
        diff_api.ShaderSource(pShader->hwid, 1, &pShader->source, NULL);
        if (pShader->compiled)
            diff_api.CompileShader(pShader->hwid);
        crFree(pShader->source);
        pShader->source = NULL;
    }

    if (pShader->deleted)
        diff_api.DeleteShader(pShader->hwid);
}

static void crStateAttachShaderCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLShader *pShader = (CRGLSLShader*) data1;
    CRGLSLProgram *pProgram = (CRGLSLProgram *) data2;
    (void) key;
    
    if (pShader->source)
    {
        diff_api.ShaderSource(pShader->hwid, 1, &pShader->source, NULL);
        if (pShader->compiled)
            diff_api.CompileShader(pShader->hwid);
    }

    diff_api.AttachShader(pProgram->hwid, pShader->hwid);
}

static void crStateDetachShaderCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLShader *pShader = (CRGLSLShader*) data1;
    CRGLSLProgram *pProgram = (CRGLSLProgram *) data2;
    (void) key;
    
    diff_api.DetachShader(pProgram->hwid, pShader->hwid);
}

static void crStateGLSLCreateProgramCB(unsigned long key, void *data1, void *data2)
{
    CRGLSLProgram *pProgram = (CRGLSLProgram*) data1;
    CRContext *ctx = (CRContext *) data2;
    GLuint i;

    pProgram->hwid = diff_api.CreateProgram();

    if (pProgram->linked)
    {
        CRASSERT(pProgram->activeState.attachedShaders);

        crHashtableWalk(pProgram->activeState.attachedShaders, crStateFixAttachedShaderHWIDsCB, ctx);
        crHashtableWalk(pProgram->activeState.attachedShaders, crStateAttachShaderCB, pProgram);

        for (i=0; i<pProgram->activeState.cAttribs; ++i)
        {
            diff_api.BindAttribLocation(pProgram->hwid, pProgram->activeState.pAttribs[i].index, pProgram->activeState.pAttribs[i].name);
        }

        if (pProgram->validated)
            diff_api.ValidateProgram(pProgram->hwid);

        diff_api.LinkProgram(pProgram->hwid);
    }

    diff_api.UseProgram(pProgram->hwid);

    for (i=0; i<pProgram->cUniforms; ++i)
    {
        GLint location;
        GLfloat *pFdata = (GLfloat*)pProgram->pUniforms[i].data;
        GLint *pIdata = (GLint*)pProgram->pUniforms[i].data;

        location = diff_api.GetUniformLocation(pProgram->hwid, pProgram->pUniforms[i].name);
        switch (pProgram->pUniforms[i].type)
        {
            case GL_FLOAT:
                diff_api.Uniform1fv(location, 1, pFdata);
                break;
            case GL_FLOAT_VEC2:
                diff_api.Uniform2fv(location, 1, pFdata);
                break;
            case GL_FLOAT_VEC3:
                diff_api.Uniform3fv(location, 1, pFdata);
                break;
            case GL_FLOAT_VEC4:
                diff_api.Uniform4fv(location, 1, pFdata);
                break;
            case GL_INT:
            case GL_BOOL:
                diff_api.Uniform1iv(location, 1, pIdata);
                break;
            case GL_INT_VEC2:
            case GL_BOOL_VEC2:
                diff_api.Uniform2iv(location, 1, pIdata);
                break;
            case GL_INT_VEC3:
            case GL_BOOL_VEC3:
                diff_api.Uniform3iv(location, 1, pIdata);
                break;
            case GL_INT_VEC4:
            case GL_BOOL_VEC4:
                diff_api.Uniform4iv(location, 1, pIdata);
                break;
            case GL_FLOAT_MAT2:
                diff_api.UniformMatrix2fv(location, 1, GL_FALSE, pFdata);
                break;
            case GL_FLOAT_MAT3:
                diff_api.UniformMatrix3fv(location, 1, GL_FALSE, pFdata);
                break;
            case GL_FLOAT_MAT4:
                diff_api.UniformMatrix4fv(location, 1, GL_FALSE, pFdata);
                break;
            case GL_SAMPLER_1D:
            case GL_SAMPLER_2D:
            case GL_SAMPLER_3D:
            case GL_SAMPLER_CUBE:
            case GL_SAMPLER_1D_SHADOW:
            case GL_SAMPLER_2D_SHADOW:
                diff_api.Uniform1iv(location, 1, pIdata);
                break;
#ifdef CR_OPENGL_VERSION_2_1
            case GL_FLOAT_MAT2x3:
                diff_api.UniformMatrix2x3fv(location, 1, GL_FALSE, pFdata);
                break;
            case GL_FLOAT_MAT2x4:
                diff_api.UniformMatrix2x4fv(location, 1, GL_FALSE, pFdata);
                break;
            case GL_FLOAT_MAT3x2:
                diff_api.UniformMatrix3x2fv(location, 1, GL_FALSE, pFdata);
                break;
            case GL_FLOAT_MAT3x4:
                diff_api.UniformMatrix3x4fv(location, 1, GL_FALSE, pFdata);
                break;
            case GL_FLOAT_MAT4x2:
                diff_api.UniformMatrix4x2fv(location, 1, GL_FALSE, pFdata);
                break;
            case GL_FLOAT_MAT4x3:
                diff_api.UniformMatrix4x3fv(location, 1, GL_FALSE, pFdata);
                break;
#endif
        default:
            crWarning("crStateGLSLCreateProgramCB: unknown uniform type 0x%x", (GLint)pProgram->pUniforms[i].type);
            break;
        }
        crFree(pProgram->pUniforms[i].data);
        crFree(pProgram->pUniforms[i].name);
    } /*for (i=0; i<pProgram->cUniforms; ++i)*/
    if (pProgram->pUniforms) crFree(pProgram->pUniforms);
    pProgram->pUniforms = NULL;
    pProgram->cUniforms = 0;

    crHashtableWalk(pProgram->activeState.attachedShaders, crStateDetachShaderCB, pProgram);
    crHashtableWalk(pProgram->currentState.attachedShaders, crStateAttachShaderCB, pProgram);
}

DECLEXPORT(void) STATE_APIENTRY crStateGLSLSwitch(CRContext *from, CRContext *to)
{
    if (to->glsl.bResyncNeeded)
    {
        to->glsl.bResyncNeeded = GL_FALSE;

        crHashtableWalk(to->glsl.shaders, crStateGLSLCreateShadersCB, to);

        crHashtableWalk(to->glsl.programs, crStateGLSLCreateProgramCB, to);

        crHashtableWalk(to->glsl.shaders, crStateGLSLSyncShadersCB, NULL);
    }

    if (to->glsl.activeProgram != from->glsl.activeProgram)
    {
        diff_api.UseProgram(to->glsl.activeProgram ? to->glsl.activeProgram->hwid : 0);
    }
}
