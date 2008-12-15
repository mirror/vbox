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
	GLint size = -1;
	GET_CONTEXT(ctx);
	void *buffer;
	CRBufferObject *bufObj;

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

	return buffer;
}


GLboolean PACKSPU_APIENTRY
packspu_UnmapBufferARB( GLenum target )
{
	CRBufferObject *bufObj;
	GET_CONTEXT(ctx);

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

	crStateUnmapBufferARB( target );

	return GL_TRUE;
}


void PACKSPU_APIENTRY
packspu_BufferDataARB( GLenum target, GLsizeiptrARB size,
											 const GLvoid * data, GLenum usage )
{
	crStateBufferDataARB(target, size, data, usage);
	crPackBufferDataARB(target, size, data, usage);
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
