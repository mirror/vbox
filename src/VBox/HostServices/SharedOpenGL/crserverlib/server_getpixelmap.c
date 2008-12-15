/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_spu.h"
#include "chromium.h"
#include "cr_error.h"
#include "cr_mem.h"
#include "cr_net.h"
#include "server_dispatch.h"
#include "server.h"

static GLint __sizeQuery( GLenum map )
{
	GLint get_values;
	/* Windows compiler gets mad if variables might be uninitialized */
	GLenum newmap = GL_PIXEL_MAP_I_TO_I_SIZE;

	switch( map )
	{
		case GL_PIXEL_MAP_I_TO_I: 
			newmap = GL_PIXEL_MAP_I_TO_I_SIZE;
			break;
		case GL_PIXEL_MAP_S_TO_S: 
			newmap = GL_PIXEL_MAP_S_TO_S_SIZE;
			break;
		case GL_PIXEL_MAP_I_TO_R: 
			newmap = GL_PIXEL_MAP_I_TO_R_SIZE;
			break;
		case GL_PIXEL_MAP_I_TO_G: 
			newmap = GL_PIXEL_MAP_I_TO_G_SIZE;
			break;
		case GL_PIXEL_MAP_I_TO_B: 
			newmap = GL_PIXEL_MAP_I_TO_B_SIZE;
			break;
		case GL_PIXEL_MAP_I_TO_A: 
			newmap = GL_PIXEL_MAP_I_TO_A_SIZE;
			break;
		case GL_PIXEL_MAP_R_TO_R: 
			newmap = GL_PIXEL_MAP_R_TO_R_SIZE;
			break;
		case GL_PIXEL_MAP_G_TO_G: 
			newmap = GL_PIXEL_MAP_G_TO_G_SIZE;
			break;
		case GL_PIXEL_MAP_B_TO_B: 
			newmap = GL_PIXEL_MAP_B_TO_B_SIZE;
			break;
		case GL_PIXEL_MAP_A_TO_A: 
			newmap = GL_PIXEL_MAP_A_TO_A_SIZE;
			break;
		default: 
			crError( "Bad map in crServerDispatchGetPixelMap: %d", map );
			break;
	}

	cr_server.head_spu->dispatch_table.GetIntegerv( newmap, &get_values );

	return get_values;
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGetPixelMapfv( GLenum map, GLfloat *values )
{
	int size = sizeof( GLfloat );
	int tabsize = __sizeQuery( map );
	GLfloat *local_values;

	(void) values;

	size *= tabsize;
	local_values = (GLfloat*)crAlloc( size );

	cr_server.head_spu->dispatch_table.GetPixelMapfv( map, local_values );
	crServerReturnValue( local_values, size );
	crFree( local_values );
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGetPixelMapuiv( GLenum map, GLuint *values )
{
	int size = sizeof( GLuint );
	int tabsize = __sizeQuery( map );
	GLuint *local_values;
	(void) values;

	size *= tabsize;
	local_values = (GLuint*)crAlloc( size );

	cr_server.head_spu->dispatch_table.GetPixelMapuiv( map, local_values );
	crServerReturnValue( local_values, size );
	crFree( local_values );
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGetPixelMapusv( GLenum map, GLushort *values )
{
	int size = sizeof( GLushort );
	int tabsize = __sizeQuery( map );
	GLushort *local_values;
	(void) values;

	size *= tabsize;
	local_values = (GLushort*)crAlloc( size );

	cr_server.head_spu->dispatch_table.GetPixelMapusv( map, local_values );
	crServerReturnValue( local_values, size );
	crFree( local_values );
}
