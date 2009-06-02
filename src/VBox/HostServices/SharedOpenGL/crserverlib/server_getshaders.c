/* $Id$ */

/** @file
 * VBox OpenGL DRI driver functions
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include "cr_spu.h"
#include "chromium.h"
#include "cr_error.h"
#include "cr_mem.h"
#include "cr_net.h"
#include "server_dispatch.h"
#include "server.h"

#ifdef CR_OPENGL_VERSION_2_0

typedef struct _crGetActive_t
{
    GLsizei length;
    GLint   size;
    GLenum  type;
} crGetActive_t;

/*@todo: most of those functions would fail. Either need to support more than 1 cr_server.return_ptr(s) or 
 *       change pack_spu to pack output args into single buffer.
 */

void SERVER_DISPATCH_APIENTRY crServerDispatchGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, char *name)
{
    crGetActive_t *pLocal;

    pLocal = (crGetActive_t*) crAlloc(bufSize+sizeof(crGetActive_t));
    if (!pLocal)
    {
        crGetActive_t zero;
        zero.length = 0;
        crServerReturnValue(&zero, sizeof(zero));
    }
    cr_server.head_spu->dispatch_table.GetActiveAttrib(program, index, bufSize, &pLocal->length, &pLocal->size, &pLocal->type, (char*)&pLocal[1]);
    crServerReturnValue(pLocal, pLocal->length+1+sizeof(crGetActive_t));
    crFree(pLocal);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, char *name)
{
    crGetActive_t *pLocal;

    pLocal = (crGetActive_t*) crAlloc(bufSize+sizeof(crGetActive_t));
    if (!pLocal)
    {
        crGetActive_t zero;
        zero.length = 0;
        crServerReturnValue(&zero, sizeof(zero));
    }
    cr_server.head_spu->dispatch_table.GetActiveUniform(program, index, bufSize, &pLocal->length, &pLocal->size, &pLocal->type, (char*)&pLocal[1]);
    crServerReturnValue(pLocal, pLocal->length+1+sizeof(crGetActive_t));
    crFree(pLocal);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders)
{
    GLsizei *pLocal;

    pLocal = (GLsizei*) crAlloc(maxCount*sizeof(GLuint)+sizeof(GLsizei));
    if (!pLocal)
    {
        GLsizei zero=0;
        crServerReturnValue(&zero, sizeof(zero));
    }
    cr_server.head_spu->dispatch_table.GetAttachedShaders(program, maxCount, pLocal, (GLuint*)&pLocal[1]);
    crServerReturnValue(pLocal, (*pLocal)*sizeof(GLuint)+sizeof(GLsizei));
    crFree(pLocal);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, char *infoLog)
{
    GLsizei *pLocal;

    pLocal = (GLsizei*) crAlloc(bufSize+sizeof(GLsizei));
    if (!pLocal)
    {
        GLsizei zero=0;
        crServerReturnValue(&zero, sizeof(zero));
    }
    cr_server.head_spu->dispatch_table.GetShaderInfoLog(shader, bufSize, pLocal, (char*)&pLocal[1]);
    crServerReturnValue(pLocal, (*pLocal)+1+sizeof(GLsizei));
    crFree(pLocal);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, char *infoLog)
{
    GLsizei *pLocal;

    pLocal = (GLsizei*) crAlloc(bufSize+sizeof(GLsizei));
    if (!pLocal)
    {
        GLsizei zero=0;
        crServerReturnValue(&zero, sizeof(zero));
    }
    cr_server.head_spu->dispatch_table.GetProgramInfoLog(program, bufSize, pLocal, (char*)&pLocal[1]);
    crServerReturnValue(pLocal, (*pLocal)+1+sizeof(GLsizei));
    crFree(pLocal);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, char *source)
{
    GLsizei *pLocal;

    pLocal = (GLsizei*) crAlloc(bufSize+sizeof(GLsizei));
    if (!pLocal)
    {
        GLsizei zero=0;
        crServerReturnValue(&zero, sizeof(zero));
    }
    cr_server.head_spu->dispatch_table.GetShaderSource(shader, bufSize, pLocal, (char*)pLocal+1);
    crServerReturnValue(pLocal, (*pLocal)+1+sizeof(GLsizei));
    crFree(pLocal);
}

static GLint __GetUniformSize(GLuint program, GLint location)
{
    GLint  size;
    GLenum type;

    /*@todo: check if index and location is the same*/
    cr_server.head_spu->dispatch_table.GetActiveUniform(program, location, 0, NULL, &size, &type, NULL);

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
            crWarning("__GetUniformSize: unknown uniform type 0x%x", (GLint)type);
            size = 16;
            break;
    }

    return size;
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGetUniformfv(GLuint program, GLint location, GLfloat *params)
{
    int size = __GetUniformSize(program, location) * sizeof(GLfloat);
    GLfloat *pLocal;

    pLocal = (GLfloat*) crAlloc(size);
    if (!pLocal)
    {
        GLsizei zero=0;
        crServerReturnValue(&zero, sizeof(zero));
    }

    cr_server.head_spu->dispatch_table.GetUniformfv(program, location, pLocal);

    crServerReturnValue(pLocal, size);
    crFree(pLocal);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGetUniformiv(GLuint program, GLint location, GLint *params)
{
    int size = __GetUniformSize(program, location) * sizeof(GLint);
    GLint *pLocal;

    pLocal = (GLint*) crAlloc(size);
    if (!pLocal)
    {
        GLsizei zero=0;
        crServerReturnValue(&zero, sizeof(zero));
    }

    cr_server.head_spu->dispatch_table.GetUniformiv(program, location, pLocal);

    crServerReturnValue(pLocal, size);
    crFree(pLocal);
}

#endif /* #ifdef CR_OPENGL_VERSION_2_0 */
