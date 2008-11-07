/** @file
 *
 * VBox OpenGL
 *
 * Complex buffered OpenGL functions
 */

/*
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

#include "vboxgl.h"

void  vboxglFogfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP2PTR(Fogfv, GLenum, GLfloat);
}

void  vboxglFogiv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP2PTR(Fogiv, GLenum, GLint);
}

void  vboxglLightModelfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP2PTR(LightModelfv, GLenum, GLfloat);
}

void  vboxglLightModeliv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP2PTR(LightModeliv, GLenum, GLint);
}

void  vboxglLightfv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(Lightfv, GLenum, GLenum, GLfloat);
}

void  vboxglLightiv(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(Lightiv, GLenum, GLenum, GLint);
}

void  vboxglMaterialfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(Materialfv, GLenum, GLenum, GLfloat);
}

void  vboxglMaterialiv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(Materialiv, GLenum, GLenum, GLint);
}

void  vboxglPixelMapfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(PixelMapfv, GLenum, GLsizei, GLfloat);
}

void  vboxglPixelMapuiv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(PixelMapuiv, GLenum, GLsizei, GLuint);
}

void  vboxglPixelMapusv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(PixelMapusv, GLenum, GLsizei, GLushort);
}

void  vboxglDeleteTextures(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP2PTR(DeleteTextures, GLsizei, GLuint);
}

void  vboxglTexEnviv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(TexEnviv, GLenum, GLenum, GLint);
}

void  vboxglTexEnvfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(TexEnvfv, GLenum, GLenum, GLfloat);
}

void  vboxglTexGendv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(TexGendv, GLenum, GLenum, GLdouble);
}

void  vboxglTexGenfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(TexGenfv, GLenum, GLenum, GLfloat);
}

void  vboxglTexGeniv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(TexGeniv, GLenum, GLenum, GLint);
}

void  vboxglTexParameterfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(TexParameterfv, GLenum, GLenum, GLfloat);
}


void  vboxglTexParameteriv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(TexParameteriv, GLenum, GLenum, GLint);
}

void  vboxglLoadMatrixd (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
   VBOX_OGL_GEN_OP1PTR(LoadMatrixd, GLdouble);
}

void  vboxglLoadMatrixf (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
   VBOX_OGL_GEN_OP1PTR(LoadMatrixf, GLfloat);
}


void  vboxglMultMatrixd (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
   VBOX_OGL_GEN_OP1PTR(MultMatrixd, GLdouble);
}

void  vboxglMultMatrixf (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
   VBOX_OGL_GEN_OP1PTR(MultMatrixf, GLfloat);
}

void  vboxglPolygonStipple (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
   VBOX_OGL_GEN_OP1PTR(PolygonStipple, GLubyte);
}

void  vboxglClipPlane (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
   VBOX_OGL_GEN_OP2PTR(ClipPlane, GLenum, GLdouble);
}

/** @todo might not work as the caller could change the array contents afterwards */
void  vboxglVertexPointer (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP4PTR(VertexPointer, GLint, GLenum, GLsizei, GLvoid);
}

/** @todo might not work as the caller could change the array contents afterwards */
void  vboxglTexCoordPointer (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP4PTR(TexCoordPointer, GLint, GLenum, GLsizei, GLvoid);
    return;
}

/** @todo might not work as the caller could change the array contents afterwards */
void  vboxglColorPointer (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP4PTR(ColorPointer, GLint, GLenum, GLsizei, GLvoid);
}

/** @todo might not work as the caller could change the array contents afterwards */
void  vboxglEdgeFlagPointer (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP2PTR(EdgeFlagPointer, GLsizei, GLvoid);
}

/** @todo might not work as the caller could change the array contents afterwards */
void  vboxglIndexPointer (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(IndexPointer, GLenum, GLsizei, GLvoid);
}

/** @todo might not work as the caller could change the array contents afterwards */
void  vboxglNormalPointer (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(NormalPointer, GLenum, GLsizei, GLvoid);
}

void  vboxglCallLists (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTR(CallLists, GLsizei, GLenum, GLvoid);
}

void  vboxglMap1d (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{

    VBOX_OGL_GEN_OP6PTR(Map1d, GLenum, GLdouble, GLdouble, GLint, GLint, GLdouble);
}

void  vboxglMap1f(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP6PTR(Map1f, GLenum, GLfloat, GLfloat, GLint, GLint, GLfloat);
}

void  vboxglMap2d (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP10PTR(Map2d, GLenum, GLdouble, GLdouble, GLint, GLint, GLdouble, GLdouble, GLint, GLint, GLdouble);
}

void  vboxglMap2f (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP10PTR(Map2f, GLenum, GLfloat, GLfloat, GLint, GLint, GLfloat, GLfloat, GLint, GLint, GLfloat);
}

void  vboxglTexImage1D (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP8PTR(TexImage1D, GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, GLvoid);
}

void  vboxglTexImage2D (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP9PTR(TexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, GLvoid);
}


void  vboxglTexSubImage1D (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP7PTR(TexSubImage1D, GLenum, GLint, GLint, GLsizei, GLenum, GLenum, GLvoid)
}

void  vboxglTexSubImage2D (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP9PTR(TexSubImage2D, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid);
}

void  vboxglPrioritizeTextures (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP3PTRPTR(PrioritizeTextures, GLsizei, GLuint, GLclampf);
}


void  vboxglRectdv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP2PTRPTR(Rectdv, GLdouble, GLdouble);
}

void  vboxglRectfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP2PTRPTR(Rectfv, GLfloat, GLfloat);
}

void  vboxglRectiv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP2PTRPTR(Rectiv, GLint, GLint);
}

void  vboxglRectsv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_OP2PTRPTR(Rectsv, GLshort, GLshort);
}
