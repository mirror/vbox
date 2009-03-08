/* Copyright (c) 2001, Stanford University
   All rights reserved.

   See the file LICENSE.txt for information on redistributing this software. */
	
#include "cr_packfunctions.h"
#include "cr_glstate.h"
#include "cr_pixeldata.h"
#include "cr_version.h"
#include "packspu.h"
#include "packspu_proto.h"

void PACKSPU_APIENTRY packspu_PixelStoref( GLenum pname, GLfloat param )
{
	/* NOTE: we do not send pixel store parameters to the server!
	 * When we pack a glDrawPixels or glTexImage2D image we interpret
	 * the user's pixel store parameters at that time and pack the
	 * image in a canonical layout (see util/pixel.c).
	 */
	crStatePixelStoref( pname, param );
}

void PACKSPU_APIENTRY packspu_PixelStorei( GLenum pname, GLint param )
{
	crStatePixelStorei( pname, param );
}

void PACKSPU_APIENTRY packspu_DrawPixels( GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels )
{
	GET_THREAD(thread);
	ContextInfo *ctx = thread->currentContext;
	CRClientState *clientState = &(ctx->clientState->client);
	if (pack_spu.swap)
		crPackDrawPixelsSWAP( width, height, format, type, pixels, &(clientState->unpack) );
	else
		crPackDrawPixels( width, height, format, type, pixels, &(clientState->unpack) );
}

void PACKSPU_APIENTRY packspu_ReadPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels )
{
	GET_THREAD(thread);
	ContextInfo *ctx = thread->currentContext;
	CRClientState *clientState = &(ctx->clientState->client);
	int writeback;
	pack_spu.ReadPixels++;
	if (pack_spu.swap)
		crPackReadPixelsSWAP( x, y, width, height, format, type, pixels,
							&(clientState->pack), &writeback );
	else
		crPackReadPixels( x, y, width, height, format, type, pixels,
							&(clientState->pack), &writeback );
	packspuFlush( (void *) thread );
	while (pack_spu.ReadPixels) 
		crNetRecv();
}

void PACKSPU_APIENTRY packspu_CopyPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum type )
{
	GET_THREAD(thread);
	if (pack_spu.swap)
		crPackCopyPixelsSWAP( x, y, width, height, type );
	else
		crPackCopyPixels( x, y, width, height, type );
	/* XXX why flush here? */
	packspuFlush( (void *) thread );
}

void PACKSPU_APIENTRY packspu_Bitmap( GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap )
{
	GET_CONTEXT(ctx);
	CRClientState *clientState = &(ctx->clientState->client);
	if (pack_spu.swap)
		crPackBitmapSWAP( width, height, xorig, yorig, xmove, ymove, bitmap, &(clientState->unpack) );
	else
		crPackBitmap( width, height, xorig, yorig, xmove, ymove, bitmap, &(clientState->unpack) );
}

void PACKSPU_APIENTRY packspu_TexImage1D( GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels )
{
	GET_CONTEXT(ctx);
	CRClientState *clientState = &(ctx->clientState->client);
	if (pack_spu.swap)
		crPackTexImage1DSWAP( target, level, internalformat, width, border, format, type, pixels, &(clientState->unpack) );
	else
		crPackTexImage1D( target, level, internalformat, width, border, format, type, pixels, &(clientState->unpack) );
}

void PACKSPU_APIENTRY packspu_TexImage2D( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels )
{
	GET_CONTEXT(ctx);
	CRClientState *clientState = &(ctx->clientState->client);
	if (pack_spu.swap)
		crPackTexImage2DSWAP( target, level, internalformat, width, height, border, format, type, pixels, &(clientState->unpack) );
	else
		crPackTexImage2D( target, level, internalformat, width, height, border, format, type, pixels, &(clientState->unpack) );
}

#ifdef GL_EXT_texture3D
void PACKSPU_APIENTRY packspu_TexImage3DEXT( GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels )
{
	GET_CONTEXT(ctx);
	CRClientState *clientState = &(ctx->clientState->client);
	if (pack_spu.swap)
		crPackTexImage3DEXTSWAP( target, level, internalformat, width, height, depth, border, format, type, pixels, &(clientState->unpack) );
	else
		crPackTexImage3DEXT( target, level, internalformat, width, height, depth, border, format, type, pixels, &(clientState->unpack) );
}
#endif

#ifdef CR_OPENGL_VERSION_1_2
void PACKSPU_APIENTRY packspu_TexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels )
{
	GET_CONTEXT(ctx);
	CRClientState *clientState = &(ctx->clientState->client);
	if (pack_spu.swap)
		crPackTexImage3DSWAP( target, level, internalformat, width, height, depth, border, format, type, pixels, &(clientState->unpack) );
	else
		crPackTexImage3D( target, level, internalformat, width, height, depth, border, format, type, pixels, &(clientState->unpack) );
}
#endif /* CR_OPENGL_VERSION_1_2 */

void PACKSPU_APIENTRY packspu_TexSubImage1D( GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels )
{
	GET_CONTEXT(ctx);
	CRClientState *clientState = &(ctx->clientState->client);
	if (pack_spu.swap)
		crPackTexSubImage1DSWAP( target, level, xoffset, width, format, type, pixels, &(clientState->unpack) );
	else
		crPackTexSubImage1D( target, level, xoffset, width, format, type, pixels, &(clientState->unpack) );
}

void PACKSPU_APIENTRY packspu_TexSubImage2D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels )
{
	GET_CONTEXT(ctx);
	CRClientState *clientState = &(ctx->clientState->client);
	if (pack_spu.swap)
		crPackTexSubImage2DSWAP( target, level, xoffset, yoffset, width, height, format, type, pixels, &(clientState->unpack) );
	else
		crPackTexSubImage2D( target, level, xoffset, yoffset, width, height, format, type, pixels, &(clientState->unpack) );
}

#ifdef CR_OPENGL_VERSION_1_2
void PACKSPU_APIENTRY packspu_TexSubImage3D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels )
{
	GET_CONTEXT(ctx);
	CRClientState *clientState = &(ctx->clientState->client);
	if (pack_spu.swap)
		crPackTexSubImage3DSWAP( target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels, &(clientState->unpack) );
	else
		crPackTexSubImage3D( target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels, &(clientState->unpack) );
}
#endif /* CR_OPENGL_VERSION_1_2 */

void PACKSPU_APIENTRY packspu_ZPixCR( GLsizei width, GLsizei height, GLenum format, GLenum type, GLenum ztype, GLint zparm, GLint length, const GLvoid *pixels )
{
	GET_CONTEXT(ctx);
	CRClientState *clientState = &(ctx->clientState->client);
	if (pack_spu.swap)
		crPackZPixCRSWAP( width, height, format, type, ztype, zparm, length, pixels, &(clientState->unpack) );
	else
		crPackZPixCR( width, height, format, type, ztype, zparm, length, pixels, &(clientState->unpack) );
}

void PACKSPU_APIENTRY packspu_GetTexImage (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
	GET_THREAD(thread);
	ContextInfo *ctx = thread->currentContext;
	CRClientState *clientState = &(ctx->clientState->client);
	int writeback = 1;
	/* XXX note: we're not observing the pixel pack parameters here.
	 * To do so, we'd have to allocate a temporary image buffer (how large???)
	 * and copy the image to the user's buffer using the pixel pack params.
	 */
	if (pack_spu.swap)
		crPackGetTexImageSWAP( target, level, format, type, pixels, &(clientState->pack), &writeback );
	else
		crPackGetTexImage( target, level, format, type, pixels, &(clientState->pack), &writeback );
	packspuFlush( (void *) thread );
	while (writeback) 
		crNetRecv();
}
