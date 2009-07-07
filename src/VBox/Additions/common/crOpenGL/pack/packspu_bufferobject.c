/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_error.h"
#include "cr_mem.h"
#include "cr_string.h"
#include "packspu.h"
#include "packspu_proto.h"


void * PACKSPU_APIENTRY
packspu_MapBufferARB( GLenum target, GLenum access )
{
    GET_CONTEXT(ctx);
    void *buffer;
#if 0
    CRBufferObject *bufObj;
    GLint size = -1;

    (void) crStateMapBufferARB( target, access );

    crStateGetBufferParameterivARB(target, GL_BUFFER_SIZE_ARB, &size);
    if (size <= 0)
        return NULL;

    if (crStateGetError()) {
        /* something may have gone wrong already */
        return NULL;
    }

    /* allocate buffer space */
    buffer = crAlloc(size);
    if (!buffer) {
        return NULL;
    }

    /* update state tracker info */
    if (target == GL_ARRAY_BUFFER_ARB) {
        bufObj = ctx->clientState->bufferobject.arrayBuffer;
    }
    else {
        CRASSERT(target == GL_ELEMENT_ARRAY_BUFFER_ARB);
        bufObj = ctx->clientState->bufferobject.elementsBuffer;
    }
    bufObj->pointer = buffer;

    /* Get current buffer data from server.
     * Ideally, if we could detect that the entire buffer was being
     * rewritten, we wouldn't have to fetch the current data here.
     */
    packspu_GetBufferSubDataARB(target, 0, bufObj->size, buffer);
#else
    CRASSERT(GL_TRUE == ctx->clientState->bufferobject.retainBufferData);
    buffer = crStateMapBufferARB(target, access); 
#endif

    return buffer;
}

void PACKSPU_APIENTRY packspu_GetBufferSubDataARB( GLenum target, GLintptrARB offset, GLsizeiptrARB size, void * data )
{
    crStateGetBufferSubDataARB(target, offset, size, data);
}


GLboolean PACKSPU_APIENTRY
packspu_UnmapBufferARB( GLenum target )
{
    GET_CONTEXT(ctx);

#if 0
    CRBufferObject *bufObj;

    if (target == GL_ARRAY_BUFFER_ARB) {
        bufObj = ctx->clientState->bufferobject.arrayBuffer;
    }
    else {
        CRASSERT(target == GL_ELEMENT_ARRAY_BUFFER_ARB);
        bufObj = ctx->clientState->bufferobject.elementsBuffer;
    }

    /* send new buffer contents to server */
    crPackBufferDataARB( target, bufObj->size, bufObj->pointer, bufObj->usage );

    /* free the buffer / unmap it */
    crFree(bufObj->pointer);
#endif

    crStateUnmapBufferARB( target );

    return GL_TRUE;
}


void PACKSPU_APIENTRY
packspu_BufferDataARB(GLenum target, GLsizeiptrARB size, const GLvoid * data, GLenum usage)
{
    /*crDebug("packspu_BufferDataARB size:%d", size);*/
    crStateBufferDataARB(target, size, data, usage);
    crPackBufferDataARB(target, size, data, usage);
}

void PACKSPU_APIENTRY
packspu_BufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid * data)
{
    /*crDebug("packspu_BufferSubDataARB size:%d", size);*/
    crStateBufferSubDataARB(target, offset, size, data);
    crPackBufferSubDataARB(target, offset, size, data);
}


void PACKSPU_APIENTRY
packspu_GetBufferPointervARB(GLenum target, GLenum pname, GLvoid **params)
{
    crStateGetBufferPointervARB( target, pname, params );
}


void PACKSPU_APIENTRY
packspu_GetBufferParameterivARB( GLenum target, GLenum pname, GLint * params )
{
    crStateGetBufferParameterivARB( target, pname, params );
}


/*
 * Need to update our local state for vertex arrays.
 */
void PACKSPU_APIENTRY
packspu_BindBufferARB( GLenum target, GLuint buffer )
{
    crStateBindBufferARB(target, buffer);
    crPackBindBufferARB(target, buffer);
}
