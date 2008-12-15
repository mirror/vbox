/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "packer.h"
#include "cr_opcodes.h"
#include "cr_mem.h"


void PACK_APIENTRY crPackPolygonStipple( const GLubyte *mask )
{
	GET_PACKER_CONTEXT(pc);
	unsigned char *data_ptr;
	int packet_length = 32*32/8;
	GET_BUFFERED_POINTER(pc, packet_length );
	crMemcpy( data_ptr, mask, 32*32/8 );
	WRITE_OPCODE( pc, CR_POLYGONSTIPPLE_OPCODE );
}
