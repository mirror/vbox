/** @file
 *
 * VBox OpenGL
 *
 * Simple buffered OpenGL functions
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


void vboxglReadPixels (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP7_PASS_PTR(ReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid);
}

/** @todo */
void vboxglFeedbackBuffer (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    AssertFailed();
//    VBOX_OGL_GEN_SYNC_OP3_PTR(FeedbackBuffer, GLsizei, GLenum, GLfloat);
}

/** @todo */
void vboxglSelectBuffer (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    AssertFailed();
//    VBOX_OGL_GEN_SYNC_OP2_PTR(SelectBuffer, GLsizei, GLuint);
}

/* Note: when in GL_FEEDBACK or GL_SELECT mode -> fill those buffers
 *       when switching to GL_FEEDBACK or GL_SELECT mode -> pass pointers
 */
void vboxglRenderMode (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    /** @todo */
    AssertFailed();
    VBOX_OGL_GEN_SYNC_OP1_RET(GLint, RenderMode, GLenum);
}

void vboxglGenTextures (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GenTextures, GLsizei, GLuint);
}

void vboxglGetBooleanv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetBooleanv, GLenum, GLboolean);
}

void vboxglGetDoublev (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetDoublev, GLenum, GLdouble);
}

void vboxglGetFloatv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetFloatv, GLenum, GLfloat);
}

void vboxglGetIntegerv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetIntegerv, GLenum, GLint);
}

void vboxglGetLightfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetLightfv, GLenum ,GLenum, GLfloat);
}

void vboxglGetLightiv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetLightiv, GLenum ,GLenum, GLint);
}

void vboxglGetMaterialfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetMaterialfv, GLenum ,GLenum, GLfloat);
}

void vboxglGetMaterialiv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetMaterialiv, GLenum ,GLenum, GLint);
}

void vboxglGetPixelMapfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetPixelMapfv, GLenum, GLfloat);
}

void vboxglGetPixelMapuiv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetPixelMapuiv, GLenum, GLuint);
}

void vboxglGetPixelMapusv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetPixelMapusv, GLenum, GLushort);
}

void vboxglGetTexEnviv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexEnviv, GLenum, GLenum, GLint);
}

void vboxglGetTexEnvfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexEnvfv, GLenum, GLenum, GLfloat);
}

void vboxglGetTexGendv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexGendv, GLenum, GLenum, GLdouble);
}

void vboxglGetTexGenfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexGenfv, GLenum, GLenum, GLfloat);
}

void vboxglGetTexGeniv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexGeniv, GLenum, GLenum, GLint);
}

void vboxglGetTexParameterfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
   VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexParameterfv, GLenum, GLenum, GLfloat);
}

void vboxglGetTexParameteriv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
   VBOX_OGL_GEN_SYNC_OP3_PASS_PTR(GetTexParameteriv, GLenum, GLenum, GLint);
}

void vboxglGetClipPlane (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP2_PASS_PTR(GetClipPlane, GLenum, GLdouble);
}

void vboxglGetPolygonStipple (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
   VBOX_OGL_GEN_SYNC_OP1_PASS_PTR(GetPolygonStipple, GLubyte);
}

void vboxglGetTexLevelParameterfv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP4_PASS_PTR(GetTexLevelParameterfv, GLenum, GLint, GLenum, GLfloat);
}

void vboxglGetTexLevelParameteriv (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP4_PASS_PTR(GetTexLevelParameteriv, GLenum, GLint, GLenum, GLint);
}

void vboxglGetTexImage (VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    VBOX_OGL_GEN_SYNC_OP5_PASS_PTR(GetTexImage, GLenum, GLint, GLenum, GLenum, GLvoid);
}
