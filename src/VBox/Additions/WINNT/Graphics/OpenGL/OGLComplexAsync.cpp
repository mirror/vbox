/** @file
 *
 * VirtualBox Windows NT/2000/XP guest OpenGL ICD
 *
 * Complex buffered OpenGL functions
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxOGL.h"
#include <iprt/cdefs.h>
#include <iprt/assert.h>

void APIENTRY glFogfv(GLenum pname, const GLfloat *params)
{
    if (pname == GL_FOG_COLOR)
    {
        VBOX_OGL_GEN_OP2PTR(Fogfv, pname, 4*sizeof(*params), params);
    }
    else
    {
        VBOX_OGL_GEN_OP2PTR(Fogfv, pname, 1*sizeof(*params), params);
    }
}

void APIENTRY glFogiv(GLenum pname, const GLint *params)
{
    if (pname == GL_FOG_COLOR)
    {
        VBOX_OGL_GEN_OP2PTR(Fogiv, pname, 4*sizeof(*params), params);
    }
    else
    {
        VBOX_OGL_GEN_OP2PTR(Fogiv, pname, 1*sizeof(*params), params);
    }
}

void APIENTRY glLightModelfv(GLenum pname, const GLfloat *params)
{
    if (pname == GL_LIGHT_MODEL_AMBIENT)
    {
        VBOX_OGL_GEN_OP2PTR(LightModelfv, pname, 4*sizeof(*params), params);
    }
    else
    {
        VBOX_OGL_GEN_OP2PTR(LightModelfv, pname, 1*sizeof(*params), params);
    }
}

void APIENTRY glLightModeliv(GLenum pname, const GLint *params)
{
    if (pname == GL_LIGHT_MODEL_AMBIENT)
    {
        VBOX_OGL_GEN_OP2PTR(LightModeliv, pname, 4*sizeof(*params), params);
    }
    else
    {
        VBOX_OGL_GEN_OP2PTR(LightModeliv, pname, 1*sizeof(*params), params);
    }
}

void APIENTRY glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
    uint32_t n = glInternalLightvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(Lightfv, light, pname, n*sizeof(*params), params);
}

void APIENTRY glLightiv(GLenum light, GLenum pname, const GLint *params)
{
    uint32_t n = glInternalLightvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(Lightiv, light, pname, n*sizeof(*params), params);
}

void APIENTRY glMaterialfv (GLenum face, GLenum pname, const GLfloat *params)
{
    uint32_t n = glInternalMaterialvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(Materialfv, face, pname, n*sizeof(*params), params);
}


void APIENTRY glMaterialiv (GLenum face, GLenum pname, const GLint *params)
{
    uint32_t n = glInternalMaterialvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(Materialfv, face, pname, n*sizeof(*params), params);
}

void APIENTRY glPixelMapfv (GLenum map, GLsizei mapsize, const GLfloat *values)
{
    VBOX_OGL_GEN_OP2PTR(PixelMapfv, map, mapsize*sizeof(*values), values);
}

void APIENTRY glPixelMapuiv (GLenum map, GLsizei mapsize, const GLuint *values)
{
    VBOX_OGL_GEN_OP2PTR(PixelMapuiv, map, mapsize*sizeof(*values), values);
}

void APIENTRY glPixelMapusv (GLenum map, GLsizei mapsize, const GLushort *values)
{
    VBOX_OGL_GEN_OP2PTR(PixelMapusv, map, mapsize*sizeof(*values), values);
}

void APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures)
{
    VBOX_OGL_GEN_OP1PTR(DeleteTextures, n*sizeof(*textures), textures);
}

void APIENTRY glTexEnviv (GLenum target, GLenum pname, const GLint *params)
{
    uint32_t n = glInternalTexEnvvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(TexEnviv, target, pname, n*sizeof(*params), params);
    return;
}

void APIENTRY glTexEnvfv (GLenum target, GLenum pname, const GLfloat *params)
{
    uint32_t n = glInternalTexEnvvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(TexEnvfv, target, pname, n*sizeof(*params), params);
}

void APIENTRY glTexGendv (GLenum coord, GLenum pname, const GLdouble *params)
{
    uint32_t n = glInternalTexGenvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(TexGendv, coord, pname, n*sizeof(*params), params);
    return;
}

void APIENTRY glTexGenfv (GLenum coord, GLenum pname, const GLfloat *params)
{
    uint32_t n = glInternalTexGenvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(TexGenfv, coord, pname, n*sizeof(*params), params);
    return;
}

void APIENTRY glTexGeniv (GLenum coord, GLenum pname, const GLint *params)
{
    uint32_t n = glInternalTexGenvElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(TexGeniv, coord, pname, n*sizeof(*params), params);
    return;
}


void APIENTRY glTexParameterfv (GLenum target, GLenum pname, const GLfloat *params)
{
    uint32_t n = glInternalTexParametervElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(TexParameterfv, target, pname, n*sizeof(*params), params);
    return;
}

void APIENTRY glTexParameteriv (GLenum target, GLenum pname, const GLint *params)
{
    uint32_t n = glInternalTexParametervElem(pname);

    if (!n)
    {
        AssertFailed();
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(TexParameteriv, target, pname, n*sizeof(*params), params);
    return;
}

void APIENTRY glLoadMatrixd (const GLdouble *m)
{
   VBOX_OGL_GEN_OP1PTR(LoadMatrixd, 16*sizeof(*m), m);
}

void APIENTRY glLoadMatrixf (const GLfloat *m)
{
   VBOX_OGL_GEN_OP1PTR(LoadMatrixf, 16*sizeof(*m), m);
}


void APIENTRY glMultMatrixd (const GLdouble *m)
{
   VBOX_OGL_GEN_OP1PTR(MultMatrixd, 16*sizeof(*m), m);
}

void APIENTRY glMultMatrixf (const GLfloat *m)
{
   VBOX_OGL_GEN_OP1PTR(MultMatrixf, 16*sizeof(*m), m);
}

void APIENTRY glPolygonStipple (const GLubyte *mask)
{
   VBOX_OGL_GEN_OP1PTR(PolygonStipple, 32*32/8, mask);
}

void APIENTRY glClipPlane (GLenum plane, const GLdouble *equation)
{
   VBOX_OGL_GEN_OP2PTR(ClipPlane, plane, 4*sizeof(*equation), equation);
}



void APIENTRY glDisableClientState(GLenum type)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    switch(type)
    {
    case GL_VERTEX_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_VERTEX].fValid  = false;
        break;
    case GL_NORMAL_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_NORMAL].fValid  = false;
        break;
    case GL_COLOR_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_COLOR].fValid  = false;
        break;
    case GL_INDEX_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_INDEX].fValid  = false;
        break;
    case GL_TEXTURE_COORD_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_TEXCOORD].fValid  = false;
        break;
    case GL_EDGE_FLAG_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_EDGEFLAG].fValid  = false;
        break;
    default:
        AssertMsgFailed(("invalid type %x\n", type));
        break;
    }
    VBOX_OGL_GEN_OP1(DisableClientState, type);
}

void APIENTRY glEnableClientState(GLenum type)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    switch(type)
    {
    case GL_VERTEX_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_VERTEX].fValid  = true;
        break;
    case GL_NORMAL_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_NORMAL].fValid  = true;
        break;
    case GL_COLOR_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_COLOR].fValid  = true;
        break;
    case GL_INDEX_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_INDEX].fValid  = true;
        break;
    case GL_TEXTURE_COORD_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_TEXCOORD].fValid  = true;
        break;
    case GL_EDGE_FLAG_ARRAY:
        pCtx->Pointer[VBOX_OGL_DRAWELEMENT_EDGEFLAG].fValid  = true;
        break;
    default:
        AssertMsgFailed(("invalid type %x\n", type));
        break;
    }
    VBOX_OGL_GEN_OP1(EnableClientState, type);
}

void APIENTRY glVertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();

    if (size < 2 || size > 4)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }

    GLint cbDataType = glVBoxGetDataTypeSize(type);
    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }

    GLint minStride = size*cbDataType;
    if (stride < minStride)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }

    if (stride == 0)
        stride = minStride;

    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_VERTEX].pointer = pointer;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_VERTEX].size    = size;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_VERTEX].stride  = stride;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_VERTEX].type    = type;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_VERTEX].cbDataType = cbDataType;
    return;
}

void APIENTRY glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    if (size < 1 || size > 4)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }

    GLint cbDataType = glVBoxGetDataTypeSize(type);

    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }

    GLint minStride = size*cbDataType;

    if (stride < minStride)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }

    if (stride == 0)
        stride = minStride;

    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_TEXCOORD].pointer = pointer;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_TEXCOORD].size    = size;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_TEXCOORD].stride  = stride;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_TEXCOORD].type    = type;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_TEXCOORD].cbDataType = cbDataType;
    return;
}

/** @todo might not work as the caller could change the array contents afterwards */
void APIENTRY glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    if (size < 3 || size > 4)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }

    GLint cbDataType = glVBoxGetDataTypeSize(type);

    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }

    GLint minStride = size*cbDataType;

    if (stride < minStride)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }

    if (stride == 0)
        stride = minStride;

    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_COLOR].pointer = pointer;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_COLOR].size    = size;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_COLOR].stride  = stride;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_COLOR].type    = type;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_COLOR].cbDataType = cbDataType;
    return;
}

void APIENTRY glEdgeFlagPointer (GLsizei stride, const GLvoid *pointer)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    GLint minStride = sizeof(GLboolean);

    if (stride < minStride)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }

    if (stride == 0)
        stride = minStride;

    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_EDGEFLAG].size    = 1;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_EDGEFLAG].pointer = pointer;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_EDGEFLAG].stride  = stride;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_EDGEFLAG].cbDataType = sizeof(GLboolean);
    return;
}

void APIENTRY glIndexPointer (GLenum type, GLsizei stride, const GLvoid *pointer)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    GLint cbDataType = glVBoxGetDataTypeSize(type);

    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }

    GLint minStride = cbDataType;

    if (stride < minStride)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }

    if (stride == 0)
        stride = minStride;

    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_INDEX].size    = 1;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_INDEX].pointer = pointer;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_INDEX].stride  = stride;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_INDEX].type    = type;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_INDEX].cbDataType = cbDataType;
}

void APIENTRY glNormalPointer (GLenum type, GLsizei stride, const GLvoid *pointer)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    GLint cbDataType = glVBoxGetDataTypeSize(type);

    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }

    GLint minStride = cbDataType;

    if (stride < minStride)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }

    if (stride == 0)
        stride = minStride;

    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_NORMAL].size    = 1;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_NORMAL].pointer = pointer;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_NORMAL].stride  = stride;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_NORMAL].type    = type;
    pCtx->Pointer[VBOX_OGL_DRAWELEMENT_NORMAL].cbDataType = cbDataType;
    return;
}

void APIENTRY glCallLists (GLsizei n, GLenum type, const GLvoid *lists)
{
    GLint cbDataType = glVBoxGetDataTypeSize(type);

    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }
    VBOX_OGL_GEN_OP3PTR(CallLists, n, type, n * cbDataType, lists);
    return;
}


void APIENTRY glMap1d (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points)
{
    GLint cElement, minStride;

    switch (target)
    {
    case GL_MAP1_INDEX:
    case GL_MAP1_TEXTURE_COORD_1:
        cElement = 1;
        break;

    case GL_MAP1_TEXTURE_COORD_2:
        cElement = 2;
        break;

    case GL_MAP1_NORMAL:
    case GL_MAP1_TEXTURE_COORD_3:
    case GL_MAP1_VERTEX_3:
        cElement = 3;
        break;

    case GL_MAP1_TEXTURE_COORD_4:
    case GL_MAP1_VERTEX_4:
    case GL_MAP1_COLOR_4:
        cElement = 4;
        break;

    default:
        glLogError(GL_INVALID_ENUM);
        break;
    }
    minStride = cElement;

    if (    stride < minStride
        ||  order < 0)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    /* stride is in doubles/floats this time */
    if (stride == 0)
        stride = minStride;

    VBOX_OGL_GEN_OP6PTR(Map1d, target, u1, u2, stride, order, stride*order*sizeof(*points), points);
    return;
}

void APIENTRY glMap1f (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points)
{
    GLint cElement, minStride;

    switch (target)
    {
    case GL_MAP1_INDEX:
    case GL_MAP1_TEXTURE_COORD_1:
        cElement = 1;
        break;

    case GL_MAP1_TEXTURE_COORD_2:
        cElement = 2;
        break;

    case GL_MAP1_NORMAL:
    case GL_MAP1_TEXTURE_COORD_3:
    case GL_MAP1_VERTEX_3:
        cElement = 3;
        break;

    case GL_MAP1_TEXTURE_COORD_4:
    case GL_MAP1_VERTEX_4:
    case GL_MAP1_COLOR_4:
        cElement = 4;
        break;

    default:
        glLogError(GL_INVALID_ENUM);
        break;
    }
    minStride = cElement;

    if (    stride < minStride
        ||  order < 0)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    /* stride is in doubles/floats this time */
    if (stride == 0)
        stride = minStride;

    VBOX_OGL_GEN_OP6PTR(Map1f, target, u1, u2, stride, order, stride*order*sizeof(*points), points);
    return;
}

void APIENTRY glMap2d (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points)
{
    GLint cElement, minuStride, minvStride;

    switch (target)
    {
    case GL_MAP2_INDEX:
    case GL_MAP2_TEXTURE_COORD_1:
        cElement = 1;
        break;

    case GL_MAP2_TEXTURE_COORD_2:
        cElement = 2;
        break;

    case GL_MAP2_NORMAL:
    case GL_MAP2_TEXTURE_COORD_3:
    case GL_MAP2_VERTEX_3:
        cElement = 3;
        break;

    case GL_MAP2_TEXTURE_COORD_4:
    case GL_MAP2_VERTEX_4:
    case GL_MAP2_COLOR_4:
        cElement = 4;
        break;

    default:
        glLogError(GL_INVALID_ENUM);
        break;
    }
    minuStride = cElement;
    minvStride = minuStride*uorder;

    if (    ustride < minuStride
        ||  uorder < 0
        ||  vstride < minvStride
        ||  vorder < 0)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    /* stride is in doubles/floats this time */
    if (ustride == 0)
        ustride = minuStride;

    if (vstride == 0)
        vstride = minuStride*uorder;

    VBOX_OGL_GEN_OP10PTR(Map2d, target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, ustride*uorder * vstride*vorder * sizeof(*points), points);
    return;
}

void APIENTRY glMap2f (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points)
{
    GLint cElement, minuStride, minvStride;

    switch (target)
    {
    case GL_MAP2_INDEX:
    case GL_MAP2_TEXTURE_COORD_1:
        cElement = 1;
        break;

    case GL_MAP2_TEXTURE_COORD_2:
        cElement = 2;
        break;

    case GL_MAP2_NORMAL:
    case GL_MAP2_TEXTURE_COORD_3:
    case GL_MAP2_VERTEX_3:
        cElement = 3;
        break;

    case GL_MAP2_TEXTURE_COORD_4:
    case GL_MAP2_VERTEX_4:
    case GL_MAP2_COLOR_4:
        cElement = 4;
        break;

    default:
        glLogError(GL_INVALID_ENUM);
        break;
    }
    minuStride = cElement;
    minvStride = minuStride*uorder;

    if (    ustride < minuStride
        ||  uorder < 0
        ||  vstride < minvStride
        ||  vorder < 0)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    /* stride is in doubles/floats this time */
    if (ustride == 0)
        ustride = minuStride;

    if (vstride == 0)
        vstride = minuStride*uorder;

    VBOX_OGL_GEN_OP10PTR(Map2f, target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, ustride*uorder * vstride*vorder * sizeof(*points), points);
    return;
}

void APIENTRY glTexImage1D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
    GLint cbDataType = glVBoxGetDataTypeSize(type);

    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }
    if (target != GL_TEXTURE_1D)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }
    if (    level < 0 
        ||  (border != 0 && border != 1))
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    if (    width < 0
        ||  width > glInternalGetIntegerv(GL_MAX_TEXTURE_SIZE)+2)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    GLint cbPixel = cbDataType * glInternalGetPixelFormatElements(format);

    VBOX_OGL_GEN_OP8PTR(TexImage1D, target, level, internalformat, width, border, format, type, cbPixel*width, pixels);
    return;
}

void APIENTRY glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
    GLint maxTextureSize = glInternalGetIntegerv(GL_MAX_TEXTURE_SIZE) + 2;
    GLint cbDataType = glVBoxGetDataTypeSize(type);

    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }
    if (target != GL_TEXTURE_1D)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }
    if (    level < 0 
        ||  (border != 0 && border != 1))
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    if (    width < 0
        ||  width > maxTextureSize
        ||  height < 0
        ||  height > maxTextureSize)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    GLint cbPixel = cbDataType * glInternalGetPixelFormatElements(format);

    VBOX_OGL_GEN_OP9PTR(TexImage2D, target, level, internalformat, width, height, border, format, type, cbPixel*width*height, pixels);
    return;
}


void APIENTRY glTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels)
{
    GLint w = glInternalGetTexLevelParameteriv(GL_TEXTURE_WIDTH);
    GLint b = glInternalGetTexLevelParameteriv(GL_TEXTURE_BORDER);
    GLint cbDataType = glVBoxGetDataTypeSize(type);

    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }
    if (target != GL_TEXTURE_1D)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }
    if (    level < 0 
        ||  xoffset < -b
        ||  (xoffset + width) > (w - b))
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    if (    width < 0
        ||  width > glInternalGetIntegerv(GL_MAX_TEXTURE_SIZE)+2)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    GLint cbFormat = glInternalGetPixelFormatElements(format);
    if (cbFormat == 0)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }

    GLint cbPixel = cbDataType * cbFormat;

    /** @todo could optimize by skipping unused texture data */
    VBOX_OGL_GEN_OP7PTR(TexSubImage1D, target, level, xoffset, width, format, type, cbPixel*width, pixels);
    return;
}

void APIENTRY glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
    GLint w = glInternalGetTexLevelParameteriv(GL_TEXTURE_WIDTH);
    GLint h = glInternalGetTexLevelParameteriv(GL_TEXTURE_HEIGHT);
    GLint b = glInternalGetTexLevelParameteriv(GL_TEXTURE_BORDER);
    GLint cbDataType = glVBoxGetDataTypeSize(type);

    if (!cbDataType)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }
    if (target != GL_TEXTURE_1D)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }
    if (    level < 0 
        ||  xoffset < -b
        ||  (xoffset + width) > (w - b)
        ||  yoffset < -b
        ||  (yoffset + height) > (h - b))
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    if (    width < 0
        ||  width > glInternalGetIntegerv(GL_MAX_TEXTURE_SIZE)+2)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    GLint cbFormat = glInternalGetPixelFormatElements(format);
    if (cbFormat == 0)
    {
        glLogError(GL_INVALID_ENUM);
        return;
    }

    GLint cbPixel = cbDataType * cbFormat;

    /** @todo could optimize by skipping unused texture data */
    VBOX_OGL_GEN_OP9PTR(TexSubImage2D, target, level, xoffset, yoffset, width, height, format, type, cbPixel*width*height, pixels);
    return;
}

void APIENTRY glPrioritizeTextures (GLsizei n, const GLuint *textures, const GLclampf *priorities)
{
    if (n < 0)
    {
        glLogError(GL_INVALID_VALUE);
        return;
    }
    VBOX_OGL_GEN_OP3PTRPTR(PrioritizeTextures, n, n*sizeof(*textures), textures, n*sizeof(*priorities), priorities);
    return;
}


void APIENTRY glRectdv (const GLdouble *v1, const GLdouble *v2)
{
    VBOX_OGL_GEN_OP2PTRPTR(Rectdv, sizeof(GLdouble)*2, v1, sizeof(GLdouble)*2, v2);
}

void APIENTRY glRectfv (const GLfloat *v1, const GLfloat *v2)
{
    VBOX_OGL_GEN_OP2PTRPTR(Rectfv, sizeof(GLfloat)*2, v1, sizeof(GLfloat)*2, v2);
}

void APIENTRY glRectiv (const GLint *v1, const GLint *v2)
{
    VBOX_OGL_GEN_OP2PTRPTR(Rectiv, sizeof(GLint)*2, v1, sizeof(GLint)*2, v2);
}

void APIENTRY glRectsv (const GLshort *v1, const GLshort *v2)
{
    VBOX_OGL_GEN_OP2PTRPTR(Rectsv, sizeof(GLshort)*2, v1, sizeof(GLshort)*2, v2);
}
