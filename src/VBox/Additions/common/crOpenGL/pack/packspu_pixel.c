/* Copyright (c) 2001, Stanford University
   All rights reserved.

   See the file LICENSE.txt for information on redistributing this software. */
    
#include "cr_packfunctions.h"
#include "cr_glstate.h"
#include "cr_pixeldata.h"
#include "cr_version.h"
#include "packspu.h"
#include "packspu_proto.h"

static GLboolean packspu_CheckTexImageFormat(GLenum format)
{
    if (format!=GL_COLOR_INDEX
        && format!=GL_RED
        && format!=GL_GREEN
        && format!=GL_BLUE
        && format!=GL_ALPHA
        && format!=GL_RGB
        && format!=GL_BGR
        && format!=GL_RGBA
        && format!=GL_BGRA
        && format!=GL_LUMINANCE
        && format!=GL_LUMINANCE_ALPHA
        && format!=GL_DEPTH_COMPONENT)
    {
        /*crWarning("crPackCheckTexImageFormat FAILED format 0x%x isn't valid", format);*/
        return GL_FALSE;
    }

    return GL_TRUE;
}

static GLboolean packspu_CheckTexImageType(GLenum type)
{
    if (type!=GL_UNSIGNED_BYTE
        && type!=GL_BYTE
        && type!=GL_BITMAP
        && type!=GL_UNSIGNED_SHORT
        && type!=GL_SHORT
        && type!=GL_UNSIGNED_INT
        && type!=GL_INT
        && type!=GL_FLOAT
        && type!=GL_UNSIGNED_BYTE_3_3_2
        && type!=GL_UNSIGNED_BYTE_2_3_3_REV
        && type!=GL_UNSIGNED_SHORT_5_6_5
        && type!=GL_UNSIGNED_SHORT_5_6_5_REV
        && type!=GL_UNSIGNED_SHORT_4_4_4_4
        && type!=GL_UNSIGNED_SHORT_4_4_4_4_REV
        && type!=GL_UNSIGNED_SHORT_5_5_5_1
        && type!=GL_UNSIGNED_SHORT_1_5_5_5_REV
        && type!=GL_UNSIGNED_INT_8_8_8_8
        && type!=GL_UNSIGNED_INT_8_8_8_8_REV
        && type!=GL_UNSIGNED_INT_10_10_10_2
        && type!=GL_UNSIGNED_INT_2_10_10_10_REV)
    {
        /*crWarning("crPackCheckTexImageType FAILED type 0x%x isn't valid", type);*/
        return GL_FALSE;
    }

    return GL_TRUE;
}

static GLboolean packspu_CheckTexImageInternalFormat(GLint internalformat)
{
    if (internalformat!=1
        && internalformat!=2
        && internalformat!=3
        && internalformat!=4
        && internalformat!=GL_ALPHA
        && internalformat!=GL_ALPHA4
        && internalformat!=GL_ALPHA8
        && internalformat!=GL_ALPHA12
        && internalformat!=GL_ALPHA16
        && internalformat!=GL_COMPRESSED_ALPHA
        && internalformat!=GL_COMPRESSED_LUMINANCE
        && internalformat!=GL_COMPRESSED_LUMINANCE_ALPHA
        && internalformat!=GL_COMPRESSED_INTENSITY
        && internalformat!=GL_COMPRESSED_RGB
        && internalformat!=GL_COMPRESSED_RGBA
        && internalformat!=GL_DEPTH_COMPONENT
        && internalformat!=GL_DEPTH_COMPONENT16
        && internalformat!=GL_DEPTH_COMPONENT24
        && internalformat!=GL_DEPTH_COMPONENT32
        && internalformat!=GL_LUMINANCE
        && internalformat!=GL_LUMINANCE4
        && internalformat!=GL_LUMINANCE8
        && internalformat!=GL_LUMINANCE12
        && internalformat!=GL_LUMINANCE16
        && internalformat!=GL_LUMINANCE_ALPHA
        && internalformat!=GL_LUMINANCE4_ALPHA4
        && internalformat!=GL_LUMINANCE6_ALPHA2
        && internalformat!=GL_LUMINANCE8_ALPHA8
        && internalformat!=GL_LUMINANCE12_ALPHA4
        && internalformat!=GL_LUMINANCE12_ALPHA12
        && internalformat!=GL_LUMINANCE16_ALPHA16
        && internalformat!=GL_INTENSITY
        && internalformat!=GL_INTENSITY4
        && internalformat!=GL_INTENSITY8
        && internalformat!=GL_INTENSITY12
        && internalformat!=GL_INTENSITY16
        && internalformat!=GL_R3_G3_B2
        && internalformat!=GL_RGB
        && internalformat!=GL_RGB4
        && internalformat!=GL_RGB5
        && internalformat!=GL_RGB8
        && internalformat!=GL_RGB10
        && internalformat!=GL_RGB12
        && internalformat!=GL_RGB16
        && internalformat!=GL_RGBA
        && internalformat!=GL_RGBA2
        && internalformat!=GL_RGBA4
        && internalformat!=GL_RGB5_A1
        && internalformat!=GL_RGBA8
        && internalformat!=GL_RGB10_A2
        && internalformat!=GL_RGBA12
        && internalformat!=GL_RGBA16
        && internalformat!=GL_SLUMINANCE
        && internalformat!=GL_SLUMINANCE8
        && internalformat!=GL_SLUMINANCE_ALPHA
        && internalformat!=GL_SLUMINANCE8_ALPHA8
        && internalformat!=GL_SRGB
        && internalformat!=GL_SRGB8
        && internalformat!=GL_SRGB_ALPHA
        && internalformat!=GL_SRGB8_ALPHA8
#ifdef CR_EXT_texture_compression_s3tc
        && internalformat!=GL_COMPRESSED_RGB_S3TC_DXT1_EXT
        && internalformat!=GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
        && internalformat!=GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
        && internalformat!=GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#endif
        )
    {
        /*crWarning("crPackCheckTexImageInternalFormat FAILED internalformat 0x%x isn't valid", internalformat);*/
        return GL_FALSE;
    }

    return GL_TRUE;
}

static GLboolean packspu_CheckTexImageParams(GLint internalformat, GLenum format, GLenum type)
{
    return packspu_CheckTexImageFormat(format)
           && packspu_CheckTexImageType(type)
           && packspu_CheckTexImageInternalFormat(internalformat);
}

static GLboolean packspu_CheckTexImageFormatType(GLenum format, GLenum type)
{
    return packspu_CheckTexImageFormat(format)
           && packspu_CheckTexImageType(type);
}

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

    if (pack_spu.swap)
    {
        crPackReadPixelsSWAP(x, y, width, height, format, type, pixels,
                             &(clientState->pack), &writeback);
    }
    else
    {
        crPackReadPixels(x, y, width, height, format, type, pixels,
                         &(clientState->pack), &writeback);
    }

#ifdef CR_ARB_pixel_buffer_object
    if (!crStateIsBufferBound(GL_PIXEL_PACK_BUFFER_ARB))
#endif
    {
        pack_spu.ReadPixels++;

        packspuFlush((void *) thread);
        while (pack_spu.ReadPixels) 
            crNetRecv();
    }
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

    if (!packspu_CheckTexImageParams(internalformat, format, type))
    {
        if (pixels || crStateIsBufferBound(GL_PIXEL_UNPACK_BUFFER_ARB))
        {
            crWarning("packspu_TexImage1D invalid internalFormat(%x)/format(%x)/type(%x)", internalformat, format, type);
            return;
        }
        internalformat = packspu_CheckTexImageInternalFormat(internalformat) ? internalformat:GL_RGBA;
        format = packspu_CheckTexImageFormat(format) ? format:GL_RGBA;
        type = packspu_CheckTexImageType(type) ? type:GL_UNSIGNED_BYTE;
    }

    if (pack_spu.swap)
        crPackTexImage1DSWAP( target, level, internalformat, width, border, format, type, pixels, &(clientState->unpack) );
    else
        crPackTexImage1D( target, level, internalformat, width, border, format, type, pixels, &(clientState->unpack) );
}

void PACKSPU_APIENTRY packspu_TexImage2D( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels )
{
    GET_CONTEXT(ctx);
    CRClientState *clientState = &(ctx->clientState->client);

    if (!packspu_CheckTexImageParams(internalformat, format, type))
    {
        if (pixels || crStateIsBufferBound(GL_PIXEL_UNPACK_BUFFER_ARB))
        {
            crWarning("packspu_TexImage2D invalid internalFormat(%x)/format(%x)/type(%x)", internalformat, format, type);
            return;
        }
        internalformat = packspu_CheckTexImageInternalFormat(internalformat) ? internalformat:GL_RGBA;
        format = packspu_CheckTexImageFormat(format) ? format:GL_RGBA;
        type = packspu_CheckTexImageType(type) ? type:GL_UNSIGNED_BYTE;
    }

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

    if (!packspu_CheckTexImageFormatType(format, type))
    {
        crWarning("packspu_TexSubImage1D invalid format(%x)/type(%x)", format, type);
        return;
    }

    if (pack_spu.swap)
        crPackTexSubImage1DSWAP( target, level, xoffset, width, format, type, pixels, &(clientState->unpack) );
    else
        crPackTexSubImage1D( target, level, xoffset, width, format, type, pixels, &(clientState->unpack) );
}

void PACKSPU_APIENTRY packspu_TexSubImage2D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels )
{
    GET_CONTEXT(ctx);
    CRClientState *clientState = &(ctx->clientState->client);

    if (!packspu_CheckTexImageFormatType(format, type))
    {
        crWarning("packspu_TexSubImage2D invalid format(%x)/type(%x)", format, type);
        return;
    }

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

#ifdef CR_ARB_pixel_buffer_object
    if (!crStateIsBufferBound(GL_PIXEL_PACK_BUFFER_ARB))
#endif
    {
        packspuFlush( (void *) thread );
        while (writeback) 
            crNetRecv();
    }
}

void PACKSPU_APIENTRY packspu_GetCompressedTexImageARB( GLenum target, GLint level, GLvoid * img )
{
    GET_THREAD(thread);
    int writeback = 1;

    if (pack_spu.swap)
    {
        crPackGetCompressedTexImageARBSWAP( target, level, img, &writeback );
    }
    else
    {
        crPackGetCompressedTexImageARB( target, level, img, &writeback );
    }

#ifdef CR_ARB_pixel_buffer_object
    if (!crStateIsBufferBound(GL_PIXEL_PACK_BUFFER_ARB))
#endif
    {
        packspuFlush( (void *) thread );
        while (writeback)
            crNetRecv();
    }
}
