/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "chromium.h"
#include "cr_error.h"
#include "cr_mem.h"
#include "server_dispatch.h"
#include "server.h"

void * SERVER_DISPATCH_APIENTRY
crServerDispatchMapBufferARB( GLenum target, GLenum access )
{
	return NULL;
}

GLboolean SERVER_DISPATCH_APIENTRY
crServerDispatchUnmapBufferARB( GLenum target )
{
	return GL_FALSE;
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchGenBuffersARB(GLsizei n, GLuint *buffers)
{
	GLuint *local_buffers = (GLuint *) crAlloc( n * sizeof(*local_buffers) );
	(void) buffers;
	cr_server.head_spu->dispatch_table.GenBuffersARB( n, local_buffers );
	crServerReturnValue( local_buffers, n * sizeof(*local_buffers) );
	crFree( local_buffers );
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchGetBufferPointervARB(GLenum target, GLenum pname, GLvoid **params)
{
	crError( "glGetBufferPointervARB isn't *ever* allowed to be on the wire!" );
	(void) target;
	(void) pname;
	(void) params;
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchGetBufferSubDataARB(GLenum target, GLintptrARB offset,
																		GLsizeiptrARB size, void * data)
{
	void *b;

	b = crAlloc(size);
	if (b) {
		cr_server.head_spu->dispatch_table.GetBufferSubDataARB( target, offset, size, b );

		crServerReturnValue( b, size );
		crFree( b );
	}
	else {
		crError("Out of memory in crServerDispatchGetBufferSubDataARB");
	}
}

