/* $Id$ */

/** @file
 * VBox crOpenGL: teximage functions.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "chromium.h"
#include "cr_error.h" 
#include "server_dispatch.h"
#include "server.h"

#define CR_NOTHING()

#define CR_CHECKPTR(name)                                   \
    if (!realptr)                                           \
    {                                                       \
        crWarning(#name " with NULL ptr, ignored!");        \
        return;                                             \
    }

#if !defined(CR_STATE_NO_TEXTURE_IMAGE_STORE)
# define CR_FIXPTR() (uintptr_t) realptr += (uintptr_t) cr_server.head_spu->dispatch_table.MapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_READ_ONLY_ARB)
#else
# define CR_FIXPTR()
#endif

#if defined(CR_ARB_pixel_buffer_object)
# define CR_CHECKBUFFER(name, checkptr)                     \
    if (crStateIsBufferBound(GL_PIXEL_UNPACK_BUFFER_ARB))   \
    {                                                       \
        CR_FIXPTR();                                        \
    }                                                       \
    else                                                    \
    {                                                       \
        checkptr                                            \
    }
#else
# define CR_CHECKBUFFER(name, checkptr) checkptr
#endif

#if defined(CR_ARB_pixel_buffer_object) && !defined(CR_STATE_NO_TEXTURE_IMAGE_STORE)
# define CR_FINISHBUFFER()                                                                  \
    if (crStateIsBufferBound(GL_PIXEL_UNPACK_BUFFER_ARB))                                   \
    {                                                                                       \
        if (!cr_server.head_spu->dispatch_table.UnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB)) \
        {                                                                                   \
            crWarning("UnmapBufferARB failed");                                             \
        }                                                                                   \
    }
#else 
#define CR_FINISHBUFFER()
#endif

#define CR_FUNC_SUBIMAGE(name, def, call, ptrname)          \
void SERVER_DISPATCH_APIENTRY                               \
crServerDispatch##name##def                                 \
{                                                           \
    const GLvoid *realptr = ptrname;                        \
    CR_CHECKBUFFER(name, CR_CHECKPTR(name))                 \
    crState##name##call;                                    \
    CR_FINISHBUFFER()                                       \
    realptr = ptrname;                                      \
    cr_server.head_spu->dispatch_table.##name##call;        \
}

#define CR_FUNC_IMAGE(name, def, call, ptrname)             \
void SERVER_DISPATCH_APIENTRY                               \
crServerDispatch##name##def                                 \
{                                                           \
    const GLvoid *realptr = ptrname;                        \
    CR_CHECKBUFFER(name, CR_NOTHING())                      \
    crState##name##call;                                    \
    CR_FINISHBUFFER()                                       \
    realptr = ptrname;                                      \
    cr_server.head_spu->dispatch_table.##name##call;        \
}

#if defined(CR_ARB_texture_compression)
CR_FUNC_SUBIMAGE(CompressedTexSubImage1DARB,
    (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imagesize, const GLvoid * data),
    (target, level, xoffset, width, format, imagesize, realptr), data)

CR_FUNC_SUBIMAGE(CompressedTexSubImage2DARB,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imagesize, const GLvoid * data),
    (target, level, xoffset, yoffset, width, height, format, imagesize, realptr), data)

CR_FUNC_SUBIMAGE(CompressedTexSubImage3DARB,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imagesize, const GLvoid * data),
    (target, level, xoffset, yoffset, zoffset, width, height, depth, format, imagesize, realptr), data)

CR_FUNC_IMAGE(CompressedTexImage1DARB,
    (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLint border, GLsizei imagesize, const GLvoid * data),
    (target, level, internalFormat, width, border, imagesize, realptr), data)

CR_FUNC_IMAGE(CompressedTexImage2DARB,
    (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLint border, GLsizei imagesize, const GLvoid * data),
    (target, level, internalFormat, width, height, border, imagesize, realptr), data)

CR_FUNC_IMAGE(CompressedTexImage3DARB,
    (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imagesize, const GLvoid * data),
    (target, level, internalFormat, width, height, depth, border, imagesize, realptr), data)
#endif

CR_FUNC_SUBIMAGE(TexSubImage1D,
    (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, xoffset, width, format, type, realptr), pixels)

CR_FUNC_SUBIMAGE(TexSubImage2D,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, xoffset, yoffset, width, height, format, type, realptr), pixels)

CR_FUNC_SUBIMAGE(TexSubImage3D,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, realptr), pixels)

CR_FUNC_IMAGE(TexImage1D,
    (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, internalFormat, width, border, format, type, realptr), pixels)

CR_FUNC_IMAGE(TexImage2D,
    (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, internalFormat, width, height, border, format, type, realptr), pixels)

CR_FUNC_IMAGE(TexImage3D,
    (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * pixels),
    (target, level, internalFormat, width, height, depth, border, format, type, realptr), pixels)
