/* $Id$ */

/** @file
 * VBox OpenGL GLSL related functions
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


GLuint PACKSPU_APIENTRY packspu_CreateProgram(void)
{
    GET_THREAD(thread);
    int writeback = 1;
    GLuint return_val = (GLuint) 0;
    if (!(pack_spu.thread[0].netServer.conn->actual_network))
    {
        crError("packspu_CreateProgram doesn't work when there's no actual network involved!\nTry using the simplequery SPU in your chain!");
    }
    if (pack_spu.swap)
    {
        crPackCreateProgramSWAP(&return_val, &writeback);
    }
    else
    {
        crPackCreateProgram(&return_val, &writeback);
    }
    packspuFlush((void *) thread);
    while (writeback)
        crNetRecv();
    if (pack_spu.swap)
    {
        return_val = (GLuint) SWAP32(return_val);
    }

    crStateCreateProgram(return_val);

    return return_val;
}

static GLint packspu_GetUniformLocationUncached(GLuint program, const char * name)
{
    GET_THREAD(thread);
    int writeback = 1;
    GLint return_val = (GLint) 0;
    if (!(pack_spu.thread[0].netServer.conn->actual_network))
    {
        crError("packspu_GetUniformLocation doesn't work when there's no actual network involved!\nTry using the simplequery SPU in your chain!");
    }
    if (pack_spu.swap)
    {
        crPackGetUniformLocationSWAP(program, name, &return_val, &writeback);
    }
    else
    {
        crPackGetUniformLocation(program, name, &return_val, &writeback);
    }
    packspuFlush((void *) thread);
    while (writeback)
        crNetRecv();
    if (pack_spu.swap)
    {
        return_val = (GLint) SWAP32(return_val);
    }
    return return_val;
}

GLint PACKSPU_APIENTRY packspu_GetUniformLocation(GLuint program, const char * name)
{
    if (!crStateIsProgramUniformsCached(program))  
    {
        GET_THREAD(thread);
        int writeback = 1;
        GLsizei maxcbData = 16*1024*sizeof(char);
        GLsizei *pData;

        pData = (GLsizei *) crAlloc(maxcbData+sizeof(GLsizei));
        if (!pData)
        {
            crWarning("packspu_GetUniformLocation: not enough memory, fallback to single query");
            return packspu_GetUniformLocationUncached(program, name);
        }

        crPackGetUniformsLocations(program, maxcbData, pData, NULL, &writeback);

        packspuFlush((void *) thread);
        while (writeback)
            crNetRecv();

        crStateGLSLProgramCacheUniforms(program, pData[0], &pData[1]);

        CRASSERT(crStateIsProgramUniformsCached(program));

        crFree(pData);
    }

    /*crDebug("packspu_GetUniformLocation(%d, %s)=%i", program, name, crStateGetUniformLocation(program, name));*/
    return crStateGetUniformLocation(program, name);
}

void PACKSPU_APIENTRY packspu_GetUniformsLocations(GLuint program, GLsizei maxcbData, GLsizei * cbData, GLvoid * pData)
{
    (void) program;
    (void) maxcbData;
    (void) cbData;
    (void) pData;
    crWarning("packspu_GetUniformsLocations shouldn't be called directly");
}

void PACKSPU_APIENTRY packspu_DeleteProgram(GLuint program)
{
    crStateDeleteProgram(program);
    crPackDeleteProgram(program);
}

void PACKSPU_APIENTRY packspu_LinkProgram(GLuint program)
{
    crStateLinkProgram(program);
    crPackLinkProgram(program);
}
