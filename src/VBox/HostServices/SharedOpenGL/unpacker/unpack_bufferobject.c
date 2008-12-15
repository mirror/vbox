/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "unpacker.h"
#include "cr_mem.h"
#include "cr_error.h"


void crUnpackExtendGetBufferSubDataARB( void )
{
	GLenum target = READ_DATA( 8, GLenum );
	GLintptrARB offset = READ_DATA( 12, GLint );
	GLsizeiptrARB size = READ_DATA( 16, GLint );
#if 0
	GLvoid *data;
#endif

	SET_RETURN_PTR( 20 );
	SET_WRITEBACK_PTR( 28 );
#if 0
	crMemcpy( &data, DATA_POINTER( 20, GLvoid ), sizeof(data) );
	printf("%s data=%p\n", __FUNCTION__, data);
	cr_unpackDispatch.GetBufferSubDataARB( target, offset, size, data );
#endif
	cr_unpackDispatch.GetBufferSubDataARB( target, offset, size, NULL );
}


void crUnpackExtendBufferDataARB( void )
{
	GLenum target = READ_DATA( sizeof(int) + 4, GLenum );
	GLsizeiptrARB size = READ_DATA( sizeof(int) + 8, GLsizeiptrARB );
	GLenum usage = READ_DATA( sizeof(int) + 12, GLenum );
	GLvoid *data = DATA_POINTER( sizeof(int) + 16, GLvoid );

	CRASSERT(sizeof(GLsizeiptrARB) == 4);
	CRASSERT(usage == GL_STATIC_DRAW_ARB);

	cr_unpackDispatch.BufferDataARB( target, size, data, usage );
}


void crUnpackExtendBufferSubDataARB( void )
{
	GLenum target = READ_DATA( sizeof(int) + 4, GLenum );
	GLintptrARB offset = READ_DATA( sizeof(int) + 8, GLintptrARB );
	GLsizeiptrARB size = READ_DATA( sizeof(int) + 12, GLsizeiptrARB );
	GLvoid *data = DATA_POINTER( sizeof(int) + 16, GLvoid );

	cr_unpackDispatch.BufferSubDataARB( target, offset, size, data );
}


void crUnpackExtendDeleteBuffersARB(void)
{
	GLsizei n = READ_DATA( 8, GLsizei );
	const GLuint *buffers = DATA_POINTER( 12, GLuint );
	cr_unpackDispatch.DeleteBuffersARB( n, buffers );
}

