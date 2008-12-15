/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "chromium.h"
#include "cr_error.h" 
#include "cr_mem.h"
#include "cr_pixeldata.h"
#include "server_dispatch.h"
#include "server.h"

void SERVER_DISPATCH_APIENTRY
crServerDispatchGetTexImage(GLenum target, GLint level, GLenum format,
														GLenum type, GLvoid * pixels)
{
	GLsizei width, height, depth, size;
	GLvoid *buffer = NULL;

	cr_server.head_spu->dispatch_table.GetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
	cr_server.head_spu->dispatch_table.GetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);
	cr_server.head_spu->dispatch_table.GetTexLevelParameteriv(target, level, GL_TEXTURE_DEPTH, &depth);

	size = crTextureSize(format, type, width, height, depth);

	if (size && (buffer = crAlloc(size))) {
		/* Note, the other pixel PACK parameters (default values) should
		 * be OK at this point.
		 */
		cr_server.head_spu->dispatch_table.PixelStorei(GL_PACK_ALIGNMENT, 1);
		cr_server.head_spu->dispatch_table.GetTexImage(target, level, format,
																									 type, buffer);
		crServerReturnValue( buffer, size );
		crFree(buffer);
	}
	else {
		/* need to return _something_ to avoid blowing up */
		GLuint dummy = 0;
		crServerReturnValue( (GLvoid *) &dummy, sizeof(dummy) );
	}
}


#if CR_ARB_texture_compression

void SERVER_DISPATCH_APIENTRY
crServerDispatchGetCompressedTexImageARB(GLenum target, GLint level,
																				 GLvoid *img)
{
	GLint size;
	GLvoid *buffer=NULL;

	cr_server.head_spu->dispatch_table.GetTexLevelParameteriv(target, level, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);

	if (size && (buffer = crAlloc(size))) {
		/* XXX the pixel PACK parameter should be OK at this point */
		cr_server.head_spu->dispatch_table.GetCompressedTexImageARB(target, level,
																																buffer);
		crServerReturnValue( buffer, size );
		crFree(buffer);
	}
	else {
		/* need to return _something_ to avoid blowing up */
		GLuint dummy = 0;
		crServerReturnValue( (GLvoid *) &dummy, sizeof(dummy) );
	}
}

#endif /* CR_ARB_texture_compression */
