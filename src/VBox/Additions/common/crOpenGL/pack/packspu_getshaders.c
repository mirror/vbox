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

#include "packspu.h"
#include "cr_packfunctions.h"
#include "cr_net.h"
#include "packspu_proto.h"
#include "cr_mem.h"

/*@todo combine with the one from server_getshaders.c*/
typedef struct _crGetActive_t
{
    GLsizei length;
    GLint   size;
    GLenum  type;
} crGetActive_t;

void PACKSPU_APIENTRY packspu_GetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, char * name)
{
    GET_THREAD(thread);
    int writeback = 1;
    crGetActive_t *pLocal;

    if (!size || !type || !name) return;

    pLocal = (crGetActive_t*) crAlloc(bufSize+sizeof(crGetActive_t));
    if (!pLocal) return;

    crPackGetActiveAttrib(program, index, bufSize, (GLsizei*)pLocal, NULL, NULL, NULL, &writeback);

    packspuFlush((void *) thread);
    while (writeback)
        crNetRecv();

    if (length) *length = pLocal->length;
    *size   = pLocal->size;
    *type   = pLocal->type;
    crMemcpy(name, (char*)&pLocal[1], pLocal->length+1);
    crFree(pLocal);
}

void PACKSPU_APIENTRY packspu_GetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, char * name)
{
    GET_THREAD(thread);
    int writeback = 1;
    crGetActive_t *pLocal;

    if (!size || !type || !name) return;

    pLocal = (crGetActive_t*) crAlloc(bufSize+sizeof(crGetActive_t));
    if (!pLocal) return;

    crPackGetActiveUniform(program, index, bufSize, (GLsizei*)pLocal, NULL, NULL, NULL, &writeback);

    packspuFlush((void *) thread);
    while (writeback)
        crNetRecv();

    if (length) *length = pLocal->length;
    *size   = pLocal->size;
    *type   = pLocal->type;
    crMemcpy(name, &pLocal[1], pLocal->length+1);
    crFree(pLocal);
}

void PACKSPU_APIENTRY packspu_GetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei * count, GLuint * shaders)
{
    GET_THREAD(thread);
    int writeback = 1;
    GLsizei *pLocal;

    if (!shaders) return;

    pLocal = (GLsizei*) crAlloc(maxCount*sizeof(GLuint)+sizeof(GLsizei));
    if (!pLocal) return;

    crPackGetAttachedShaders(program, maxCount, pLocal, NULL, &writeback);

    packspuFlush((void *) thread);
    while (writeback)
        crNetRecv();

    if (count) *count=*pLocal;
    crMemcpy(shaders, &pLocal[1], *pLocal*sizeof(GLuint));
    crFree(pLocal);
}

void PACKSPU_APIENTRY packspu_GetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei * length, char * infoLog)
{
    GET_THREAD(thread);
    int writeback = 1;
    GLsizei *pLocal;

    if (!infoLog) return;

    pLocal = (GLsizei*) crAlloc(bufSize+sizeof(GLsizei));
    if (!pLocal) return;

    crPackGetProgramInfoLog(program, bufSize, pLocal, NULL, &writeback);

    packspuFlush((void *) thread);
    while (writeback)
        crNetRecv();

    if (length) *length=*pLocal;
    crMemcpy(infoLog, &pLocal[1], (*pLocal)+1);
    crFree(pLocal);
}

void PACKSPU_APIENTRY packspu_GetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei * length, char * infoLog)
{
    GET_THREAD(thread);
    int writeback = 1;
    GLsizei *pLocal;

    if (!infoLog) return;

    pLocal = (GLsizei*) crAlloc(bufSize+sizeof(GLsizei));
    if (!pLocal) return;

    crPackGetShaderInfoLog(shader, bufSize, pLocal, NULL, &writeback);

    packspuFlush((void *) thread);
    while (writeback)
        crNetRecv();

    if (length) *length=*pLocal;
    crMemcpy(infoLog, &pLocal[1], (*pLocal)+1);
    crFree(pLocal);
}

void PACKSPU_APIENTRY packspu_GetShaderSource(GLuint shader, GLsizei bufSize, GLsizei * length, char * source)
{
    GET_THREAD(thread);
    int writeback = 1;
    GLsizei *pLocal;

    if (!source) return;

    pLocal = (GLsizei*) crAlloc(bufSize+sizeof(GLsizei));
    if (!pLocal) return;

    crPackGetShaderSource(shader, bufSize, pLocal, NULL, &writeback);

    packspuFlush((void *) thread);
    while (writeback)
        crNetRecv();

    if (length) *length=*pLocal;
    crMemcpy(source, &pLocal[1], (*pLocal)+1);
    crFree(pLocal);
}
