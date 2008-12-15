/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "packer.h"
#include "cr_mem.h"

static unsigned char * __gl_HandlePixelMapData( GLenum map, GLsizei mapsize, int size_of_value, const GLvoid *values )
{
	int packet_length = 
		sizeof( map ) + 
		sizeof( mapsize ) + 
		mapsize*size_of_value;
	unsigned char *data_ptr = (unsigned char *) crPackAlloc( packet_length );

	WRITE_DATA( 0, GLenum, map );
	WRITE_DATA( 4, GLsizei, mapsize );
	crMemcpy( data_ptr + 8, values, mapsize*size_of_value );
	return data_ptr;
}

void PACK_APIENTRY crPackPixelMapfv(GLenum map, GLsizei mapsize, 
		const GLfloat *values )
{
	unsigned char *data_ptr = __gl_HandlePixelMapData( map, mapsize, sizeof( *values ), values );

	crHugePacket( CR_PIXELMAPFV_OPCODE, data_ptr );
    crPackFree( data_ptr );
}

void PACK_APIENTRY crPackPixelMapuiv(GLenum map, GLsizei mapsize, 
		const GLuint *values )
{
	unsigned char *data_ptr = __gl_HandlePixelMapData( map, mapsize, sizeof( *values ), values );

	crHugePacket( CR_PIXELMAPUIV_OPCODE, data_ptr );
    crPackFree( data_ptr );
}

void PACK_APIENTRY crPackPixelMapusv(GLenum map, GLsizei mapsize, 
		const GLushort *values )
{
	unsigned char *data_ptr = __gl_HandlePixelMapData( map, mapsize, sizeof( *values ), values );

	crHugePacket( CR_PIXELMAPUSV_OPCODE, data_ptr );
    crPackFree( data_ptr );
}
