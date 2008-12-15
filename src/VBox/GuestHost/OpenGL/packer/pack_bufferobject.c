/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "packer.h"
#include "cr_error.h"
#include "cr_string.h"
#include "cr_version.h"


void PACK_APIENTRY
crPackMapBufferARB( GLenum target, GLenum access,
		    void * return_value, int * writeback )
{
     (void)writeback;
     (void)return_value;
     (void)target;
     (void)access;
     crWarning("Can't pack MapBufferARB command!");
}


void PACK_APIENTRY
crPackUnmapBufferARB( GLenum target, GLboolean* return_value, int * writeback )
{
     (void)target;
     (void)return_value;
     (void)writeback;
     crWarning("Can't pack UnmapBufferARB command!");
}


void PACK_APIENTRY
crPackBufferDataARB( GLenum target, GLsizeiptrARB size,
										 const GLvoid * data, GLenum usage )
{
	unsigned char *data_ptr;
	int packet_length;

	packet_length = sizeof(GLenum)
		+ sizeof(target) + sizeof(size) + sizeof(usage) + size;

	data_ptr = (unsigned char *) crPackAlloc( packet_length );

	WRITE_DATA( 0, GLenum, CR_BUFFERDATAARB_EXTEND_OPCODE );
	WRITE_DATA( 4, GLenum, target );
	WRITE_DATA( 8, GLsizeiptrARB, size ); /* XXX or 8 bytes? */
	WRITE_DATA( 12, GLenum, usage );
	if (data)
		 crMemcpy( data_ptr + 16, data, size );

	crHugePacket( CR_EXTEND_OPCODE, data_ptr );
    crPackFree( data_ptr );
}


void PACK_APIENTRY
crPackBufferSubDataARB( GLenum target, GLintptrARB offset, GLsizeiptrARB size,
												const GLvoid * data )
{
	unsigned char *data_ptr;
	int packet_length;

	if (!data)
		return;

	packet_length = sizeof(GLenum)
		+ sizeof(target) + sizeof(offset) + sizeof(size) + size;

	data_ptr = (unsigned char *) crPackAlloc( packet_length );
	WRITE_DATA( 0, GLenum, CR_BUFFERSUBDATAARB_EXTEND_OPCODE );
	WRITE_DATA( 4, GLenum, target );
	WRITE_DATA( 8, GLintptrARB, offset ); /* XXX or 8 bytes? */
	WRITE_DATA( 12, GLsizeiptrARB, size ); /* XXX or 8 bytes? */
	crMemcpy( data_ptr + 16, data, size );

	crHugePacket( CR_EXTEND_OPCODE, data_ptr );
    crPackFree( data_ptr );
}


void PACK_APIENTRY
crPackDeleteBuffersARB(GLsizei n, const GLuint * buffers)
{
	unsigned char *data_ptr;
	int packet_length = sizeof(GLenum) + sizeof(n) + n * sizeof(*buffers);

	if (!buffers)
		return;

	data_ptr = (unsigned char *) crPackAlloc(packet_length);
	WRITE_DATA( 0, GLenum, CR_DELETEBUFFERSARB_EXTEND_OPCODE );
	WRITE_DATA( 4, GLsizei, n );
	crMemcpy( data_ptr + 8, buffers, n * sizeof(*buffers) );
	crHugePacket( CR_EXTEND_OPCODE, data_ptr );
    crPackFree( data_ptr );
}
