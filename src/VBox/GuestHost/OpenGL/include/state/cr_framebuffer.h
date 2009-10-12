/* $Id$ */

/** @file
 * VBox crOpenGL: FBO related state info
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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


#ifndef CR_STATE_FRAMEBUFFEROBJECT_H
#define CR_STATE_FRAMEBUFFEROBJECT_H

#include "cr_hash.h"
#include "state/cr_statetypes.h"
#include "state/cr_statefuncs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CR_MAX_COLOR_ATTACHMENTS 16

typedef struct {
    GLenum  type; /*one of GL_NONE GL_TEXTURE GL_RENDERBUFFER_EXT*/
    GLuint  name;
    GLint   level;
    GLint   face;
    GLint   zoffset;
} CRFBOAttachmentPoint;

typedef struct {
    GLuint                  id, hwid;
    CRFBOAttachmentPoint    color[CR_MAX_COLOR_ATTACHMENTS];
    CRFBOAttachmentPoint    depth;
    CRFBOAttachmentPoint    stencil;
    GLenum                  readbuffer;
    /*@todo: we don't support drawbufferS yet, so it's a stub*/
    GLenum                  drawbuffer[1];
#ifdef IN_GUEST
    GLenum                  status;
#endif
} CRFramebufferObject;

typedef struct {
    GLuint   id, hwid;
    GLsizei  width, height;
    GLenum   internalformat;
    GLuint   redBits, greenBits, blueBits, alphaBits, depthBits, stencilBits;
} CRRenderbufferObject;

typedef struct {
    CRFramebufferObject     *readFB, *drawFB;
    CRRenderbufferObject    *renderbuffer;
    CRHashTable             *framebuffers;
    CRHashTable             *renderbuffers;

    /* Indicates that we have to resend FBO data to GPU on first glMakeCurrent call with owning context */
    GLboolean   bResyncNeeded;
} CRFramebufferObjectState;

DECLEXPORT(void) STATE_APIENTRY crStateFramebufferObjectInit(CRContext *ctx);
DECLEXPORT(void) STATE_APIENTRY crStateFramebufferObjectDestroy(CRContext *ctx);
DECLEXPORT(void) STATE_APIENTRY crStateFramebufferObjectSwitch(CRContext *from, CRContext *to);
DECLEXPORT(GLuint) STATE_APIENTRY crStateGetFramebufferHWID(GLuint id);
DECLEXPORT(GLuint) STATE_APIENTRY crStateGetRenderbufferHWID(GLuint id);

DECLEXPORT(void) STATE_APIENTRY crStateBindRenderbufferEXT(GLenum target, GLuint renderbuffer);
DECLEXPORT(void) STATE_APIENTRY crStateDeleteRenderbuffersEXT(GLsizei n, const GLuint *renderbuffers);
DECLEXPORT(void) STATE_APIENTRY crStateRenderbufferStorageEXT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
DECLEXPORT(void) STATE_APIENTRY crStateGetRenderbufferParameterivEXT(GLenum target, GLenum pname, GLint *params);
DECLEXPORT(void) STATE_APIENTRY crStateBindFramebufferEXT(GLenum target, GLuint framebuffer);
DECLEXPORT(void) STATE_APIENTRY crStateDeleteFramebuffersEXT(GLsizei n, const GLuint *framebuffers);
DECLEXPORT(void) STATE_APIENTRY crStateFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
DECLEXPORT(void) STATE_APIENTRY crStateFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
DECLEXPORT(void) STATE_APIENTRY crStateFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
DECLEXPORT(void) STATE_APIENTRY crStateFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
DECLEXPORT(void) STATE_APIENTRY crStateGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment, GLenum pname, GLint *params);
DECLEXPORT(void) STATE_APIENTRY crStateGenerateMipmapEXT(GLenum target);

#ifdef IN_GUEST
DECLEXPORT(GLenum) STATE_APIENTRY crStateCheckFramebufferStatusEXT(GLenum target);
DECLEXPORT(GLenum) STATE_APIENTRY crStateSetFramebufferStatus(GLenum target, GLenum status);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CR_STATE_FRAMEBUFFEROBJECT_H */
