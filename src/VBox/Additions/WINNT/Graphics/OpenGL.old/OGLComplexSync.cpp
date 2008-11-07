/** @file
 *
 * VirtualBox Windows NT/2000/XP guest OpenGL ICD
 *
 * Complex buffered OpenGL functions
 *
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "VBoxOGL.h"


void APIENTRY glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
    GLint cbDataType = glVBoxGetDataTypeSize(type);

    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }

    GLint cbPixel = cbDataType * glInternalGetPixelFormatElements(format);

    VBOX_OGL_GEN_SYNC_OP7_PASS_PTR(ReadPixels, x, y, width, height, format, type, cbPixel*width*height, pixels);
    return;
}


/** @todo */
void APIENTRY glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer)
{
    AssertFailed();
}

/** @todo */
void APIENTRY glSelectBuffer(GLsizei size, GLuint *buffer)
{
    AssertFailed();
}

/** @todo */
/* Note: when in GL_FEEDBACK or GL_SELECT mode -> fill those buffers
 *       when switching to GL_FEEDBACK or GL_SELECT mode -> pass pointers 
 */
GLint APIENTRY glRenderMode (GLenum mode)
{
    AssertFailed();
    VBOX_OGL_GEN_SYNC_OP1_RET(GLint, RenderMode, mode);
    return retval;
}


void APIENTRY glGenTextures (GLsizei n, GLuint *textures)
{
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GenTextures, n, n*sizeof(GLuint), textures);
    return;
}

GLboolean APIENTRY glAreTexturesResident (GLsizei n, const GLuint *textures, GLboolean *residences)
{
    AssertFailed();
    return 1;
}

void APIENTRY glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    if (!count)
        return; /* no error?? */

    AssertFailed();
    return;
}

void APIENTRY glBitmap (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap)
{
    AssertFailed();
    return;
}

void APIENTRY glDrawPixels (GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
    AssertFailed();
    return;
}

void APIENTRY glGetTexParameterfv (GLenum target, GLenum pname, GLfloat *params)
{
    uint32_t n = glInternalTexParametervElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexParameterfv, target, pname, n*sizeof(*params), params);
    return;
}

void APIENTRY glGetTexParameteriv (GLenum target, GLenum pname, GLint *params)
{
    uint32_t n = glInternalTexParametervElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexParameteriv, target, pname, n*sizeof(*params), params);
    return;
}

void APIENTRY glGetTexGenfv (GLenum coord, GLenum pname, GLfloat *params)
{
    uint32_t n = glInternalTexGenvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexGenfv, coord, pname, n*sizeof(*params), params);
    return;
}

void APIENTRY glGetTexGeniv (GLenum coord, GLenum pname, GLint *params)
{
    uint32_t n = glInternalTexGenvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexGeniv, coord, pname, n*sizeof(*params), params);
    return;
}

void APIENTRY glGetTexGendv (GLenum coord, GLenum pname, GLdouble *params)
{
    uint32_t n = glInternalTexGenvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexGendv, coord, pname, n*sizeof(*params), params);
    return;
}

void APIENTRY glGetTexEnviv (GLenum target, GLenum pname, GLint *params)
{
    uint32_t n = glInternalTexEnvvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexEnviv, target, pname, n*sizeof(*params), params);
}

void APIENTRY glGetTexEnvfv (GLenum target, GLenum pname, GLfloat *params)
{
    uint32_t n = glInternalTexEnvvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexEnvfv, target, pname, n*sizeof(*params), params);
}

void APIENTRY glGetPixelMapfv (GLenum map, GLfloat *values)
{
    uint32_t mapsize = glInternalGetIntegerv(map);
    if (!mapsize)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetPixelMapfv, map, mapsize*sizeof(*values), values);
}

void APIENTRY glGetPixelMapuiv (GLenum map, GLuint *values)
{
    uint32_t mapsize = glInternalGetIntegerv(map);
    if (!mapsize)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetPixelMapuiv, map, mapsize*sizeof(*values), values);
}

void APIENTRY glGetPixelMapusv (GLenum map, GLushort *values)
{
    uint32_t mapsize = glInternalGetIntegerv(map);
    if (!mapsize)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetPixelMapusv, map, mapsize*sizeof(*values), values);
}

void APIENTRY glGetMaterialiv (GLenum face, GLenum pname, GLint *params)
{
    uint32_t n = glInternalMaterialvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetMaterialiv, face, pname, n*sizeof(*params), params);
}

void APIENTRY glGetMaterialfv (GLenum face, GLenum pname, GLfloat *params)
{
    uint32_t n = glInternalMaterialvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetMaterialfv, face, pname, n*sizeof(*params), params);
}

void APIENTRY glGetLightiv (GLenum light, GLenum pname, GLint *params)
{
    uint32_t n = glInternalLightvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetLightiv, light, pname, n*sizeof(*params), params);
}

void APIENTRY glGetLightfv (GLenum light, GLenum pname, GLfloat *params)
{
    uint32_t n = glInternalLightvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetLightfv, light, pname, n*sizeof(*params), params);
}

void APIENTRY glGetClipPlane (GLenum plane, GLdouble *equation)
{
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetClipPlane, plane, 4*sizeof(GLdouble), equation);
    return;
}

void APIENTRY glGetPolygonStipple (GLubyte *mask)
{
   VBOX_OGL_GEN_SYNC_OP1_PASS_PTR(GetPolygonStipple, 32*32/8, mask);
}

void APIENTRY glGetTexImage (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
    GLint cbDataType = glVBoxGetDataTypeSize(type);

    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }

    /** @todo check dimensions of texture */
    AssertFailed();
    GLint cbPixel = cbDataType * glInternalGetPixelFormatElements(format);
    VBOX_OGL_GEN_SYNC_OP5_PASS_PTR(GetTexImage, target, level, format, type, cbPixel, pixels);
}

void APIENTRY glGetTexLevelParameterfv (GLenum target, GLint level, GLenum pname, GLfloat *params)
{
    VBOX_OGL_GEN_SYNC_OP4_PASS_PTR(GetTexLevelParameterfv, target, level, pname, sizeof(*params), params);
}

void APIENTRY glGetTexLevelParameteriv (GLenum target, GLint level, GLenum pname, GLint *params)
{
    VBOX_OGL_GEN_SYNC_OP4_PASS_PTR(GetTexLevelParameteriv, target, level, pname, sizeof(*params), params);
}
