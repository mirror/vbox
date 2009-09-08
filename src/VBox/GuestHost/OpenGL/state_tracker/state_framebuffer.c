/* $Id$ */

/** @file
 * VBox OpenGL: EXT_framebuffer_object state tracking
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

#include "state.h"
#include "state/cr_statetypes.h"
#include "state/cr_statefuncs.h"
#include "state_internals.h"
#include "cr_mem.h"

#define CRSTATE_FBO_CHECKERR(expr, result, message)         \
    if (expr) {                                             \
        crStateError(__LINE__, __FILE__, result, message);  \
        return;                                             \
    }
    
DECLEXPORT(void) STATE_APIENTRY
crStateFramebufferObjectInit(CRContext *ctx)
{
    CRFramebufferObjectState *fbo = &ctx->framebufferobject;

    fbo->framebuffer = NULL;
    fbo->renderbuffer = NULL;
    fbo->framebuffers = crAllocHashtable();
    fbo->renderbuffers = crAllocHashtable();
}

static void crStateFreeFBO(void *data)
{
    CRFramebufferObject *pObj = (CRFramebufferObject *)data;

    if (diff_api.AlphaFunc)
    {
        diff_api.DeleteFramebuffersEXT(1, &pObj->id);
    }

    crFree(pObj);
}

static void crStateFreeRBO(void *data)
{
    CRRenderbufferObject *pObj = (CRRenderbufferObject *)data;

    if (diff_api.AlphaFunc)
    {
        diff_api.DeleteRenderbuffersEXT(1, &pObj->id);
    }

    crFree(pObj);
}

DECLEXPORT(void) STATE_APIENTRY
crStateFramebufferObjectDestroy(CRContext *ctx)
{
    CRFramebufferObjectState *fbo = &ctx->framebufferobject;

    fbo->framebuffer = NULL;
    fbo->renderbuffer = NULL;

    crFreeHashtable(fbo->framebuffers, crStateFreeFBO);
    crFreeHashtable(fbo->renderbuffers, crStateFreeRBO);
}

DECLEXPORT(void) STATE_APIENTRY
crStateBindRenderbufferEXT(GLenum target, GLuint renderbuffer)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(target!=GL_RENDERBUFFER_EXT, GL_INVALID_ENUM, "invalid target");

    if (renderbuffer)
    {
        fbo->renderbuffer = (CRRenderbufferObject*) crHashtableSearch(fbo->renderbuffers, renderbuffer);
        if (!fbo->renderbuffer)
        {
            fbo->renderbuffer = (CRRenderbufferObject*) crCalloc(sizeof(CRRenderbufferObject));
            CRSTATE_FBO_CHECKERR(!fbo->renderbuffer, GL_OUT_OF_MEMORY, "glBindRenderbufferEXT");
            fbo->renderbuffer->id = renderbuffer;
            fbo->renderbuffer->internalformat = GL_RGBA;
            crHashtableAdd(fbo->renderbuffers, renderbuffer, fbo->renderbuffer);
        }
    }
    else fbo->renderbuffer = NULL;
}

DECLEXPORT(void) STATE_APIENTRY
crStateDeleteRenderbuffersEXT(GLsizei n, const GLuint *renderbuffers)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    int i;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(n<0, GL_INVALID_OPERATION, "n<0");

    for (i = 0; i < n; i++)
    {
        if (renderbuffers[i])
        {
            CRRenderbufferObject *rbo;
            rbo = (CRRenderbufferObject*) crHashtableSearch(fbo->renderbuffers, renderbuffers[i]);
            if (rbo)
            {
                if (fbo->renderbuffer==rbo)
                {
                    fbo->renderbuffer = NULL;
                }

                /* check the attachments of current framebuffer */
                if (fbo->framebuffer)
                {
                    CRFBOAttachmentPoint *ap;
                    int u;

                    for (u=0; u<CR_MAX_COLOR_ATTACHMENTS; ++u)
                    {
                        ap = &fbo->framebuffer->color[u];
                        if (ap->type==GL_RENDERBUFFER_EXT && ap->name==renderbuffers[i])
                        {
                            crStateFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, u+GL_COLOR_ATTACHMENT0_EXT, 0, 0);
                        }
                    }

                    ap = &fbo->framebuffer->depth;
                    if (ap->type==GL_RENDERBUFFER_EXT && ap->name==renderbuffers[i])
                    {
                        crStateFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, 0, 0);
                    }
                    ap = &g->framebufferobject.framebuffer->stencil;
                    if (ap->type==GL_RENDERBUFFER_EXT && ap->name==renderbuffers[i])
                    {
                        crStateFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, 0, 0);
                    }
                }

                crHashtableDelete(fbo->renderbuffers, renderbuffers[i], crFree);
            }
        }
    }
}

DECLEXPORT(void) STATE_APIENTRY
crStateRenderbufferStorageEXT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    CRRenderbufferObject *rb = fbo->renderbuffer;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(target!=GL_RENDERBUFFER_EXT, GL_INVALID_ENUM, "invalid target");
    CRSTATE_FBO_CHECKERR(rb, GL_INVALID_OPERATION, "no bound renderbuffer");

    rb->width = width;
    rb->height = height;
    rb->internalformat = internalformat;
}

DECLEXPORT(void) STATE_APIENTRY
crStateGetRenderbufferParameterivEXT(GLenum target, GLenum pname, GLint *params)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    CRRenderbufferObject *rb = fbo->renderbuffer;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(target!=GL_RENDERBUFFER_EXT, GL_INVALID_ENUM, "invalid target");
    CRSTATE_FBO_CHECKERR(rb, GL_INVALID_OPERATION, "no bound renderbuffer");

    switch (pname)
    {
        case GL_RENDERBUFFER_WIDTH_EXT:
            *params = rb->width;
            break;
        case GL_RENDERBUFFER_HEIGHT_EXT:
            *params = rb->height;
            break;
        case GL_RENDERBUFFER_INTERNAL_FORMAT_EXT:
            *params = rb->internalformat;
            break;
        case GL_RENDERBUFFER_RED_SIZE_EXT:
        case GL_RENDERBUFFER_GREEN_SIZE_EXT:
        case GL_RENDERBUFFER_BLUE_SIZE_EXT:
        case GL_RENDERBUFFER_ALPHA_SIZE_EXT:
        case GL_RENDERBUFFER_DEPTH_SIZE_EXT:
        case GL_RENDERBUFFER_STENCIL_SIZE_EXT:
            CRSTATE_FBO_CHECKERR(GL_TRUE, GL_INVALID_OPERATION, "unimplemented");
            break;
        default:
            CRSTATE_FBO_CHECKERR(GL_TRUE, GL_INVALID_ENUM, "invalid pname");
    }
}

static void crStateInitFBOAttachmentPoint(CRFBOAttachmentPoint *fboap)
{
    fboap->type = GL_NONE;
    fboap->name = 0;
    fboap->level = 0;
    fboap->face = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    fboap->zoffset = 0;
}

static void crStateInitFrameBuffer(CRFramebufferObject *fbo)
{
    int i;

    for (i=0; i<CR_MAX_COLOR_ATTACHMENTS; ++i)
        crStateInitFBOAttachmentPoint(&fbo->color[i]);

    crStateInitFBOAttachmentPoint(&fbo->depth);
    crStateInitFBOAttachmentPoint(&fbo->stencil);

    fbo->readbuffer = GL_COLOR_ATTACHMENT0_EXT;
    fbo->drawbuffer[0] = GL_COLOR_ATTACHMENT0_EXT;
}

static GLboolean crStateGetFBOAttachmentPoint(CRFramebufferObject *fb, GLenum attachment, CRFBOAttachmentPoint **ap)
{
    switch (attachment)
    {
        case GL_DEPTH_ATTACHMENT_EXT:
            *ap = &fb->depth;
            break;
        case GL_STENCIL_ATTACHMENT_EXT:
            *ap = &fb->stencil;
            break;
        default:
            if (attachment>=GL_COLOR_ATTACHMENT0_EXT && attachment<=GL_COLOR_ATTACHMENT15_EXT)
            {
                *ap = &fb->color[attachment-GL_COLOR_ATTACHMENT0_EXT];
            }
            else return GL_FALSE;
    }

    return GL_TRUE;
}

DECLEXPORT(void) STATE_APIENTRY
crStateBindFramebufferEXT(GLenum target, GLuint framebuffer)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(target!=GL_FRAMEBUFFER_EXT, GL_INVALID_ENUM, "invalid target");

    if (framebuffer)
    {
        fbo->framebuffer = (CRFramebufferObject*) crHashtableSearch(fbo->framebuffers, framebuffer);
        if (!fbo->framebuffer)
        {
            fbo->framebuffer = (CRFramebufferObject*) crCalloc(sizeof(CRFramebufferObject));
            CRSTATE_FBO_CHECKERR(!fbo->framebuffer, GL_OUT_OF_MEMORY, "glBindFramebufferEXT");
            fbo->framebuffer->id = framebuffer;
            crStateInitFrameBuffer(fbo->framebuffer);
            crHashtableAdd(fbo->framebuffers, framebuffer, fbo->framebuffer);
        }
    }
    else fbo->framebuffer = NULL;
}

DECLEXPORT(void) STATE_APIENTRY
crStateDeleteFramebuffersEXT(GLsizei n, const GLuint *framebuffers)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    int i;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(n<0, GL_INVALID_OPERATION, "n<0");

    for (i = 0; i < n; i++)
    {
        if (framebuffers[i])
        {
            CRFramebufferObject *fb;
            fb = (CRFramebufferObject*) crHashtableSearch(fbo->framebuffers, framebuffers[i]);
            if (fb)
            {
                if (fbo->framebuffer==fb)
                {
                    fbo->framebuffer = NULL;
                }
                crHashtableDelete(fbo->framebuffers, framebuffers[i], crFree);
            }
        }
    }
}

/*@todo: move this function somewhere else*/
/*return floor of base 2 log of x. log(0)==0*/
unsigned int crLog2Floor(unsigned int x)
{
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    x -= ((x >> 1) & 0x55555555);
    x  = (((x >> 2) & 0x33333333) + (x & 0x33333333));
    x  = (((x >> 4) + x) & 0x0f0f0f0f);
    x += (x >> 8);
    x += (x >> 16);
    return (x & 0x0000003f) - 1;
}

static void crStateFramebufferTextureCheck(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level,
                                           GLboolean *failed, CRFBOAttachmentPoint **ap, CRTextureObj **tobj)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    GLuint maxtexsizelog2;

    *failed = GL_TRUE;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(target!=GL_FRAMEBUFFER_EXT, GL_INVALID_ENUM, "invalid target");
    CRSTATE_FBO_CHECKERR(!fbo->framebuffer, GL_INVALID_OPERATION, "no fbo bound");
    CRSTATE_FBO_CHECKERR(!crStateGetFBOAttachmentPoint(fbo->framebuffer, attachment, ap), GL_INVALID_ENUM, "invalid attachment");

    if (!texture)
    {
        *failed = GL_FALSE;
        return;
    }

    switch (textarget)
    {
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            maxtexsizelog2 = crLog2Floor(g->limits.maxCubeMapTextureSize);
            *tobj = crStateTextureGet(GL_TEXTURE_CUBE_MAP_ARB, texture);
            break;
        case GL_TEXTURE_RECTANGLE_ARB:
            maxtexsizelog2 = 0;
            *tobj = crStateTextureGet(textarget, texture);
            break;
        case GL_TEXTURE_3D:
            maxtexsizelog2 = crLog2Floor(g->limits.max3DTextureSize);
            *tobj = crStateTextureGet(textarget, texture);
            break;
        case GL_TEXTURE_2D:
        case GL_TEXTURE_1D:
            maxtexsizelog2 = crLog2Floor(g->limits.maxTextureSize);
            *tobj = crStateTextureGet(textarget, texture);
            break;
        default:
            CRSTATE_FBO_CHECKERR(GL_TRUE, GL_INVALID_OPERATION, "invalid textarget");
    }

    CRSTATE_FBO_CHECKERR(!*tobj, GL_INVALID_OPERATION, "invalid textarget/texture combo");

    if (GL_TEXTURE_RECTANGLE_ARB==textarget)
    {
        CRSTATE_FBO_CHECKERR(level!=0, GL_INVALID_VALUE, "non zero mipmap level");
    }

    CRSTATE_FBO_CHECKERR(level<0, GL_INVALID_VALUE, "level<0");
    CRSTATE_FBO_CHECKERR(level>maxtexsizelog2, GL_INVALID_VALUE, "level too big");

    *failed = GL_FALSE;
}

DECLEXPORT(void) STATE_APIENTRY
crStateFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    CRFBOAttachmentPoint *ap;
    CRTextureObj *tobj;
    GLboolean failed;

    crStateFramebufferTextureCheck(target, attachment, textarget, texture, level, &failed, &ap, &tobj);
    if (failed) return;

    if (!texture)
    {
        crStateInitFBOAttachmentPoint(ap);
        return;
    }

    CRSTATE_FBO_CHECKERR(textarget!=GL_TEXTURE_1D, GL_INVALID_OPERATION, "textarget");

    crStateInitFBOAttachmentPoint(ap);
    ap->type = GL_TEXTURE;
    ap->name = texture;
    ap->level = level;
}

DECLEXPORT(void) STATE_APIENTRY
crStateFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    CRFBOAttachmentPoint *ap;
    CRTextureObj *tobj;
    GLboolean failed;

    crStateFramebufferTextureCheck(target, attachment, textarget, texture, level, &failed, &ap, &tobj);
    if (failed) return;

    if (!texture)
    {
        crStateInitFBOAttachmentPoint(ap);
        return;
    }

    CRSTATE_FBO_CHECKERR(GL_TEXTURE_1D==textarget || GL_TEXTURE_3D==textarget, GL_INVALID_OPERATION, "textarget");

    crStateInitFBOAttachmentPoint(ap);
    ap->type = GL_TEXTURE;
    ap->name = texture;
    ap->level = level;
    if (textarget!=GL_TEXTURE_2D && textarget!=GL_TEXTURE_RECTANGLE_ARB)
    {
        ap->face = textarget;
    }
}

DECLEXPORT(void) STATE_APIENTRY
crStateFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    CRFBOAttachmentPoint *ap;
    CRTextureObj *tobj;
    GLboolean failed;

    crStateFramebufferTextureCheck(target, attachment, textarget, texture, level, &failed, &ap, &tobj);
    if (failed) return;

    if (!texture)
    {
        crStateInitFBOAttachmentPoint(ap);
        return;
    }

    CRSTATE_FBO_CHECKERR(zoffset>(g->limits.max3DTextureSize-1), GL_INVALID_VALUE, "zoffset too big");
    CRSTATE_FBO_CHECKERR(textarget!=GL_TEXTURE_3D, GL_INVALID_OPERATION, "textarget");

    crStateInitFBOAttachmentPoint(ap);
    ap->type = GL_TEXTURE;
    ap->name = texture;
    ap->level = level;
    ap->zoffset = zoffset;
}

DECLEXPORT(void) STATE_APIENTRY
crStateFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    CRFBOAttachmentPoint *ap;
    CRRenderbufferObject *rb;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(target!=GL_FRAMEBUFFER_EXT, GL_INVALID_ENUM, "invalid target");
    CRSTATE_FBO_CHECKERR(!fbo->framebuffer, GL_INVALID_OPERATION, "no fbo bound");
    CRSTATE_FBO_CHECKERR(!crStateGetFBOAttachmentPoint(fbo->framebuffer, attachment, &ap), GL_INVALID_ENUM, "invalid attachment");

    if (!renderbuffer)
    {
        crStateInitFBOAttachmentPoint(ap);
        return;
    }

    rb = (CRRenderbufferObject*) crHashtableSearch(fbo->renderbuffers, renderbuffer);
    CRSTATE_FBO_CHECKERR(!rb, GL_INVALID_OPERATION, "rb doesn't exist");

    crStateInitFBOAttachmentPoint(ap);
    ap->type = GL_RENDERBUFFER_EXT;
    ap->name = renderbuffer;
}

DECLEXPORT(void) STATE_APIENTRY
crStateGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    CRFBOAttachmentPoint *ap;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(target!=GL_FRAMEBUFFER_EXT, GL_INVALID_ENUM, "invalid target");
    CRSTATE_FBO_CHECKERR(!fbo->framebuffer, GL_INVALID_OPERATION, "no fbo bound");
    CRSTATE_FBO_CHECKERR(!crStateGetFBOAttachmentPoint(fbo->framebuffer, attachment, &ap), GL_INVALID_ENUM, "invalid attachment");

    switch (pname)
    {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT:
            *params = ap->type;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT:
            CRSTATE_FBO_CHECKERR(ap->type!=GL_RENDERBUFFER_EXT && ap->type!=GL_TEXTURE, GL_INVALID_ENUM, "can't query object name when it's not bound")
            *params = ap->name;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT:
            CRSTATE_FBO_CHECKERR(ap->type!=GL_TEXTURE, GL_INVALID_ENUM, "not a texture");
            *params = ap->level;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT:
            CRSTATE_FBO_CHECKERR(ap->type!=GL_TEXTURE, GL_INVALID_ENUM, "not a texture");
            *params = ap->face;
            break;
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT:
            CRSTATE_FBO_CHECKERR(ap->type!=GL_TEXTURE, GL_INVALID_ENUM, "not a texture");
            *params = ap->zoffset;
            break;
        default:
            CRSTATE_FBO_CHECKERR(GL_TRUE, GL_INVALID_ENUM, "invalid pname");
    }
}

DECLEXPORT(void) STATE_APIENTRY
crStateGenerateMipmapEXT(GLenum target)
{
    /*@todo*/
}

DECLEXPORT(void) STATE_APIENTRY
crStateFramebufferObjectSwitch(CRContext *from, CRContext *to)
{
    if (to->framebufferobject.framebuffer!=from->framebufferobject.framebuffer)
    {
        diff_api.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, to->framebufferobject.framebuffer?
            to->framebufferobject.framebuffer->id:0);
    }
}
