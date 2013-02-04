/* $Id$ */

/** @file
 * VBox OpenGL: GLSL state tracking
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

#include "state.h"
#include "state/cr_statetypes.h"
#include "state/cr_statefuncs.h"
#include "state_internals.h"
#include "cr_mem.h"
#include "cr_string.h"

static CRGLSLShader* crStateGetShaderObj(GLuint id)
{
    CRContext *g = GetCurrentContext();

    if (!g)
    {
        crWarning("crStateGetShaderObj called without current ctx");
    }

    return !g ? NULL : (CRGLSLShader *) crHashtableSearch(g->glsl.shaders, id);
}

static CRGLSLProgram* crStateGetProgramObj(GLuint id)
{
    CRContext *g = GetCurrentContext();

    if (!g)
    {
        crWarning("crStateGetProgramObj called without current ctx");
    }

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

static void crStateFreeProgramUniforms(CRGLSLProgram* pProgram)
{
    GLuint i;

    for (i=0; i<pProgram->cUniforms; ++i)
    {
        if (pProgram->pUniforms[i].name) crFree(pProgram->pUniforms[i].name);
        if (pProgram->pUniforms[i].data) crFree(pProgram->pUniforms[i].data);
    }

    if (pProgram->pUniforms) crFree(pProgram->pUniforms);

    pProgram->pUniforms = NULL;
    pProgram->cUniforms = 0;

#ifdef IN_GUEST
    pProgram->bUniformsSynced = GL_FALSE;
#endif
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
    
    if (pRealShader)
    {
        crStateShaderDecRefCount(pRealShader);
    }
    else
    {
        crWarning("crStateFakeDecRefCountCB: NULL pRealShader");
    }
}

static void crStateFreeGLSLProgram(void *data)
{
    CRGLSLProgram* pProgram = (CRGLSLProgram *) data;

    crFreeHashtable(pProgram->currentState.attachedShaders, crStateShaderDecRefCount);

    if (pProgram->activeState.attachedShaders)
    {
        CRContext *g = GetCurrentContext();
        crHashtableWalk(pProgram->activeState.attachedShaders, crStateFakeDecRefCountCB, g);
        crFreeHashtable(pProgram->activeState.attachedShaders, crStateFreeGLSLShader);
    }

    crStateFreeProgramAttribs(pProgram);

    crStateFreeProgramUniforms(pProgram);

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
    CRContext *g = GetCurrentContext();

    /*@todo: hack to allow crStateFreeGLSLProgram to work correctly, 
      as the current context isn't the one being destroyed*/
#ifdef CHROMIUM_THREADSAFE
    CRASSERT(g != ctx);
    VBoxTlsRefAddRef(ctx); /* <- this is a hack to avoid subsequent SetCurrentContext(g) do recursive Destroy for ctx */
    if (g)
        VBoxTlsRefAddRef(g); /* <- ensure the g is not destroyed by the following SetCurrentContext call */
    SetCurrentContext(ctx);
#else
    __currentContext = ctx;
#endif

    crFreeHashtable(ctx->glsl.programs, crStateFreeGLSLProgram);
    crFreeHashtable(ctx->glsl.shaders, crStateFreeGLSLShader);

#ifdef CHROMIUM_THREADSAFE
    SetCurrentContext(g);
    if (g)
        VBoxTlsRefRelease(g);
    VBoxTlsRefRelease(ctx); /* <- restore back the cRefs (see above) */
#else
    __currentContext = g;
#endif

}

DECLEXPORT(GLuint) STATE_APIENTRY crStateGetShaderHWID(GLuint id)
{
    CRGLSLShader *pShader = crStateGetShaderObj(id);
#ifdef IN_GUEST
    CRASSERT(!pShader || pShader->hwid == id);
#endif
    return pShader ? pShader->hwid : 0;
}

DECLEXPORT(GLuint) STATE_APIENTRY crStateGetProgramHWID(GLuint id)
{
    CRGLSLProgram *pProgram = crStateGetProgramObj(id);
#ifdef IN_GUEST
    CRASSERT(!pProgram || pProgram->hwid == id);
#endif
    return pProgram ? pProgram->hwid : 0;
}

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

DECLEXPORT(GLuint) STATE_APIENTRY crStateCreateShader(GLuint hwid, GLenum type)
{
    CRGLSLShader *pShader;
    CRContext *g = GetCurrentContext();
    GLuint stateId = hwid;

#ifdef IN_GUEST
    CRASSERT(!crStateGetShaderObj(stateId));
#else
    /* the id may not necesserily be hwid after save state restoration */
    while ((pShader = crStateGetShaderObj(stateId)) != NULL)
    {
        GLuint newStateId = stateId + 7;
        crDebug("Shader object %d already exists, generating a new one, %d", stateId, newStateId);
        stateId = newStateId;
    }
#endif

    pShader = (CRGLSLShader *) crAlloc(sizeof(*pShader));
    if (!pShader)
    {
        crWarning("crStateCreateShader: Out of memory!");
        return 0;
    }

    pShader->id = stateId;
    pShader->hwid = hwid;
    pShader->type = type;
    pShader->source = NULL;
    pShader->compiled = GL_FALSE;
    pShader->deleted = GL_FALSE;
    pShader->refCount = 0;

    crHashtableAdd(g->glsl.shaders, stateId, pShader);

    return stateId;
}

DECLEXPORT(GLuint) STATE_APIENTRY crStateCreateProgram(GLuint hwid)
{
    CRGLSLProgram *pProgram;
    CRContext *g = GetCurrentContext();
    GLuint stateId = hwid;

#ifdef IN_GUEST
    pProgram = crStateGetProgramObj(stateId);
    if (pProgram)
    {
        crWarning("Program object %d already exists!", stateId);
        crStateDeleteProgram(stateId);
        CRASSERT(!crStateGetProgramObj(stateId));
    }
#else
    /* the id may not necesserily be hwid after save state restoration */
    while ((pProgram = crStateGetProgramObj(stateId)) != NULL)
    {
        GLuint newStateId = stateId + 7;
        crDebug("Program object %d already exists, generating a new one, %d", stateId, newStateId);
        stateId = newStateId;
    }
#endif

    pProgram = (CRGLSLProgram *) crAlloc(sizeof(*pProgram));
    if (!pProgram)
    {
        crWarning("crStateCreateShader: Out of memory!");
        return 0;
    }

    pProgram->id = stateId;
    pProgram->hwid = hwid;
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
#ifdef IN_GUEST
    pProgram->bUniformsSynced = GL_FALSE;
#endif

    crHashtableAdd(g->glsl.programs, stateId, pProgram);

    return stateId;
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

    CRASSERT(pRealShader);
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

    /*that's not a bug, note the memcpy above*/
    if (pProgram->activeState.pAttribs)
    {
        pProgram->activeState.pAttribs = (CRGLSLAttrib *) crAlloc(pProgram->activeState.cAttribs * sizeof(CRGLSLAttrib));
    }

    for (i=0; i<pProgram->activeState.cAttribs; ++i)
    {
        crMemcpy(&pProgram->activeState.pAttribs[i], &pProgram->currentState.pAttribs[i], sizeof(CRGLSLAttrib));
        pProgram->activeState.pAttribs[i].name = crStrdup(pProgram->currentState.pAttribs[i].name);
    }

    crStateFreeProgramUniforms(pProgram);
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

    if (pProgram->currentState.cAttribs)
    {
        crMemcpy(&pAttribs[0], &pProgram->currentState.pAttribs[0], pProgram->currentState.cAttribs*sizeof(CRGLSLAttrib));
    }
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
        case GL_SAMPLER_2D_RECT_ARB:
        case GL_SAMPLER_2D_RECT_SHADOW_ARB:
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

DECLEXPORT(GLboolean) STATE_APIENTRY crStateIsIntUniform(GLenum type)
{
    if (GL_INT==type
        || GL_INT_VEC2==type
        || GL_INT_VEC3==type
        || GL_INT_VEC4==type
        || GL_BOOL==type
        || GL_BOOL_VEC2==type
        || GL_BOOL_VEC3==type
        || GL_BOOL_VEC4==type
        || GL_SAMPLER_1D==type
        || GL_SAMPLER_2D==type
        || GL_SAMPLER_3D==type
        || GL_SAMPLER_CUBE==type
        || GL_SAMPLER_1D_SHADOW==type
        || GL_SAMPLER_2D_SHADOW==type
        || GL_SAMPLER_2D_RECT_ARB==type
        || GL_SAMPLER_2D_RECT_SHADOW_ARB==type)
    {
        return GL_TRUE;
    }
    else return GL_FALSE;
}

DECLEXPORT(GLboolean) STATE_APIENTRY crStateIsProgramUniformsCached(GLuint program)
{
    CRGLSLProgram *pProgram = crStateGetProgramObj(program);

    if (!pProgram)
    {
        crWarning("Unknown program %d", program);
        return GL_FALSE;
    }

#ifdef IN_GUEST
    return pProgram->bUniformsSynced;
#else
    crWarning("crStateIsProgramUniformsCached called on host side!!");
    return GL_FALSE;
#endif
}

/*@todo: one of those functions should ignore uniforms starting with "gl"*/

#ifdef IN_GUEST
DECLEXPORT(void) STATE_APIENTRY 
crStateGLSLProgramCacheUniforms(GLuint program, GLsizei cbData, GLvoid *pData)
{
    CRGLSLProgram *pProgram = crStateGetProgramObj(program);
    char *pCurrent = pData;
    GLsizei cbRead, cbName;
    GLuint i;

    if (!pProgram)
    {
        crWarning("Unknown program %d", program);
        return;
    }

    if (pProgram->bUniformsSynced)
    {
        crWarning("crStateGLSLProgramCacheUniforms: this shouldn't happen!");
        crStateFreeProgramUniforms(pProgram);
    }

    if (cbData<sizeof(GLsizei))
    {
        crWarning("crStateGLSLProgramCacheUniforms: data too short");
        return;
    }

    pProgram->cUniforms = ((GLsizei*)pCurrent)[0];
    pCurrent += sizeof(GLsizei);
    cbRead = sizeof(GLsizei);

    crDebug("crStateGLSLProgramCacheUniforms: %i active uniforms", pProgram->cUniforms);

    if (pProgram->cUniforms)
    {
        pProgram->pUniforms = crAlloc(pProgram->cUniforms*sizeof(CRGLSLUniform));
        if (!pProgram->pUniforms)
        {
            crWarning("crStateGLSLProgramCacheUniforms: no memory");
            pProgram->cUniforms = 0;
            return;
        }
    }

    for (i=0; i<pProgram->cUniforms; ++i)
    {
        cbRead += sizeof(GLuint)+sizeof(GLsizei);
        if (cbRead>cbData)
        {
            crWarning("crStateGLSLProgramCacheUniforms: out of data reading uniform %i", i);
            return;
        }
        pProgram->pUniforms[i].data = NULL;
        pProgram->pUniforms[i].location = ((GLint*)pCurrent)[0];
        pCurrent += sizeof(GLint);
        cbName = ((GLsizei*)pCurrent)[0];
        pCurrent += sizeof(GLsizei);

        cbRead += cbName;
        if (cbRead>cbData)
        {
            crWarning("crStateGLSLProgramCacheUniforms: out of data reading uniform's name %i", i);
            return;
        }

        pProgram->pUniforms[i].name = crStrndup(pCurrent, cbName);
        pCurrent += cbName;

        crDebug("crStateGLSLProgramCacheUniforms: uniform[%i]=%d, %s", i, pProgram->pUniforms[i].location, pProgram->pUniforms[i].name);
    }

    pProgram->bUniformsSynced = GL_TRUE;

    CRASSERT((pCurrent-((char*)pData))==cbRead);
    CRASSERT(cbRead==cbData);
}
#else
static GLboolean crStateGLSLProgramCacheOneUniform(GLuint location, GLsizei cbName, GLchar *pName, 
                                                   char **pCurrent, GLsizei *pcbWritten, GLsizei maxcbData)
{
    *pcbWritten += sizeof(GLint)+sizeof(GLsizei)+cbName;
    if (*pcbWritten>maxcbData)
    {
        crWarning("crStateGLSLProgramCacheUniforms: buffer too small");
        crFree(pName);
        return GL_FALSE;
    }

    crDebug("crStateGLSLProgramCacheUniforms: uniform[%i]=%s.", location, pName);

    ((GLint*)*pCurrent)[0] = location;
    *pCurrent += sizeof(GLint);
    ((GLsizei*)*pCurrent)[0] = cbName;
    *pCurrent += sizeof(GLsizei);
    crMemcpy(*pCurrent, pName, cbName);
    *pCurrent += cbName;

    return GL_TRUE;
}

DECLEXPORT(void) STATE_APIENTRY 
crStateGLSLProgramCacheUniforms(GLuint program, GLsizei maxcbData, GLsizei *cbData, GLvoid *pData)
{
    CRGLSLProgram *pProgram = crStateGetProgramObj(program);
    GLint maxUniformLen, activeUniforms=0, fakeUniformsCount, i, j;
    char *pCurrent = pData;
    GLsizei cbWritten;

    if (!pProgram)
    {
        crWarning("Unknown program %d", program);
        return;
    }

    diff_api.GetProgramiv(pProgram->hwid, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformLen);
    diff_api.GetProgramiv(pProgram->hwid, GL_ACTIVE_UNIFORMS, &activeUniforms);

    *cbData = 0;

    cbWritten = sizeof(GLsizei);
    if (cbWritten>maxcbData)
    {
        crWarning("crStateGLSLProgramCacheUniforms: buffer too small");
        return;
    }
    ((GLsizei*)pCurrent)[0] = activeUniforms;
    fakeUniformsCount = activeUniforms;
    pCurrent += sizeof(GLsizei);

    crDebug("crStateGLSLProgramCacheUniforms: %i active uniforms", activeUniforms);

    if (activeUniforms>0)
    {
        /*+8 to make sure our array uniforms with higher indices and [] will fit in as well*/
        GLchar *name = (GLchar *) crAlloc(maxUniformLen+8);
        GLenum type;
        GLint size;
        GLsizei cbName;
        GLint location;

        if (!name)
        {
            crWarning("crStateGLSLProgramCacheUniforms: no memory");
            return;
        }

        for (i=0; i<activeUniforms; ++i)
        {
            diff_api.GetActiveUniform(pProgram->hwid, i, maxUniformLen, &cbName, &size, &type, name);
            location = diff_api.GetUniformLocation(pProgram->hwid, name);

            if (!crStateGLSLProgramCacheOneUniform(location, cbName, name, &pCurrent, &cbWritten, maxcbData))
                return;

            /* Only one active uniform variable will be reported for a uniform array by glGetActiveUniform,
             * so we insert fake elements for other array elements.
             */
            if (size!=1)
            {
                char *pIndexStr = crStrchr(name, '[');
                GLint firstIndex=1;
                fakeUniformsCount += size;

                crDebug("crStateGLSLProgramCacheUniforms: expanding array uniform, size=%i", size);

                /*For array uniforms it's valid to query location of 1st element as both uniform and uniform[0].
                 *The name returned by glGetActiveUniform is driver dependent,
                 *atleast it's with [0] on win/ati and without [0] on linux/nvidia.
                 */                
                if (!pIndexStr)
                {
                    pIndexStr = name+cbName;
                    firstIndex=0;
                }
                else
                {
                    cbName = pIndexStr-name;
                    if (!crStateGLSLProgramCacheOneUniform(location, cbName, name, &pCurrent, &cbWritten, maxcbData))
                        return;
                }

                for (j=firstIndex; j<size; ++j)
                {
                    sprintf(pIndexStr, "[%i]", j);
                    cbName = crStrlen(name);

                    location = diff_api.GetUniformLocation(pProgram->hwid, name);

                    if (!crStateGLSLProgramCacheOneUniform(location, cbName, name, &pCurrent, &cbWritten, maxcbData))
                        return;
                }
            }
        }

        crFree(name);
    }

    if (fakeUniformsCount!=activeUniforms)
    {
        ((GLsizei*)pData)[0] = fakeUniformsCount;
        crDebug("FakeCount %i", fakeUniformsCount);
    }

    *cbData = cbWritten;

    CRASSERT((pCurrent-((char*)pData))==cbWritten);
}
#endif

DECLEXPORT(GLint) STATE_APIENTRY crStateGetUniformLocation(GLuint program, const char * name)
{
#ifdef IN_GUEST
    CRGLSLProgram *pProgram = crStateGetProgramObj(program);
    GLint result=-1;
    GLuint i;

    if (!pProgram)
    {
        crWarning("Unknown program %d", program);
        return -1;
    }

    if (!pProgram->bUniformsSynced)
    {
        crWarning("crStateGetUniformLocation called for uncached uniforms");
        return -1;
    }

    for (i=0; i<pProgram->cUniforms; ++i)
    {
        if (!crStrcmp(name, pProgram->pUniforms[i].name))
        {
            result = pProgram->pUniforms[i].location;
            break;
        }
    }

    return result;
#else
    crWarning("crStateGetUniformLocation called on host side!!");
    return -1;
#endif
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
        diff_api.ShaderSource(pShader->hwid, 1, (const char**)&pShader->source, NULL);
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
        diff_api.ShaderSource(pShader->hwid, 1, (const char**)&pShader->source, NULL);
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
            case GL_SAMPLER_2D_RECT_ARB:
            case GL_SAMPLER_2D_RECT_SHADOW_ARB:
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
