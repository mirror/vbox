/* $Id$ */

/** @file
 * VBox OpenGL: EXT_framebuffer_object state tracking
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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

    fbo->readFB = NULL;
    fbo->drawFB = NULL;
    fbo->renderbuffer = NULL;
    ctx->shared->bFBOResyncNeeded = GL_FALSE;
}

void crStateFreeFBO(void *data)
{
    CRFramebufferObject *pObj = (CRFramebufferObject *)data;

#ifndef IN_GUEST
    if (diff_api.DeleteFramebuffersEXT)
    {
        diff_api.DeleteFramebuffersEXT(1, &pObj->hwid);
    }
#endif

    crFree(pObj);
}

void crStateFreeRBO(void *data)
{
    CRRenderbufferObject *pObj = (CRRenderbufferObject *)data;

#ifndef IN_GUEST
    if (diff_api.DeleteRenderbuffersEXT)
    {
        diff_api.DeleteRenderbuffersEXT(1, &pObj->hwid);
    }
#endif

    crFree(pObj);
}

DECLEXPORT(void) STATE_APIENTRY
crStateFramebufferObjectDestroy(CRContext *ctx)
{
    CRFramebufferObjectState *fbo = &ctx->framebufferobject;

    fbo->readFB = NULL;
    fbo->drawFB = NULL;
    fbo->renderbuffer = NULL;
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
        fbo->renderbuffer = (CRRenderbufferObject*) crHashtableSearch(g->shared->rbTable, renderbuffer);
        if (!fbo->renderbuffer)
        {
            fbo->renderbuffer = (CRRenderbufferObject*) crCalloc(sizeof(CRRenderbufferObject));
            CRSTATE_FBO_CHECKERR(!fbo->renderbuffer, GL_OUT_OF_MEMORY, "glBindRenderbufferEXT");
            fbo->renderbuffer->id = renderbuffer;
            fbo->renderbuffer->hwid = renderbuffer;
            fbo->renderbuffer->internalformat = GL_RGBA;
            crHashtableAdd(g->shared->rbTable, renderbuffer, fbo->renderbuffer);
#ifndef IN_GUEST
        CR_STATE_SHAREDOBJ_USAGE_INIT(fbo->renderbuffer);
#endif
        }
#ifndef IN_GUEST
        CR_STATE_SHAREDOBJ_USAGE_SET(fbo->renderbuffer, g);
#endif

    }
    else fbo->renderbuffer = NULL;
}

static void crStateCheckFBOAttachments(CRFramebufferObject *pFBO, GLuint rbo, GLenum target)
{
    CRFBOAttachmentPoint *ap;
    int u;

    if (!pFBO) 
        return;

    for (u=0; u<CR_MAX_COLOR_ATTACHMENTS; ++u)
    {
        ap = &pFBO->color[u];
        if (ap->type==GL_RENDERBUFFER_EXT && ap->name==rbo)
        {
            crStateFramebufferRenderbufferEXT(target, u+GL_COLOR_ATTACHMENT0_EXT, 0, 0);
#ifdef IN_GUEST
            pFBO->status = GL_FRAMEBUFFER_UNDEFINED;
#endif
        }
    }

    ap = &pFBO->depth;
    if (ap->type==GL_RENDERBUFFER_EXT && ap->name==rbo)
    {
        crStateFramebufferRenderbufferEXT(target, GL_DEPTH_ATTACHMENT_EXT, 0, 0);
#ifdef IN_GUEST
        pFBO->status = GL_FRAMEBUFFER_UNDEFINED;
#endif
    }
    ap = &pFBO->stencil;
    if (ap->type==GL_RENDERBUFFER_EXT && ap->name==rbo)
    {
        crStateFramebufferRenderbufferEXT(target, GL_STENCIL_ATTACHMENT_EXT, 0, 0);
#ifdef IN_GUEST
        pFBO->status = GL_FRAMEBUFFER_UNDEFINED;
#endif
    }
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
            rbo = (CRRenderbufferObject*) crHashtableSearch(g->shared->rbTable, renderbuffers[i]);
            if (rbo)
            {
                if (fbo->renderbuffer==rbo)
                {
                    fbo->renderbuffer = NULL;
                }

                /* check the attachments of current framebuffers */
                crStateCheckFBOAttachments(fbo->readFB, renderbuffers[i], GL_READ_FRAMEBUFFER);
                crStateCheckFBOAttachments(fbo->drawFB, renderbuffers[i], GL_DRAW_FRAMEBUFFER);

                crHashtableDelete(g->shared->rbTable, renderbuffers[i], crStateFreeRBO);
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
    CRSTATE_FBO_CHECKERR(!rb, GL_INVALID_OPERATION, "no bound renderbuffer");

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
    CRSTATE_FBO_CHECKERR(!rb, GL_INVALID_OPERATION, "no bound renderbuffer");

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

#ifdef IN_GUEST
    fbo->status = GL_FRAMEBUFFER_UNDEFINED;
#endif
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
    CRFramebufferObject *pFBO=NULL;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(((target!=GL_FRAMEBUFFER_EXT) && (target!=GL_READ_FRAMEBUFFER) && (target!=GL_DRAW_FRAMEBUFFER)),
                         GL_INVALID_ENUM, "invalid target");

    if (framebuffer)
    {
        pFBO = (CRFramebufferObject*) crHashtableSearch(g->shared->fbTable, framebuffer);
        if (!pFBO)
        {
            pFBO = (CRFramebufferObject*) crCalloc(sizeof(CRFramebufferObject));
            CRSTATE_FBO_CHECKERR(!pFBO, GL_OUT_OF_MEMORY, "glBindFramebufferEXT");
            pFBO->id = framebuffer;
            pFBO->hwid = framebuffer;
            crStateInitFrameBuffer(pFBO);
            crHashtableAdd(g->shared->fbTable, framebuffer, pFBO);
#ifndef IN_GUEST
            CR_STATE_SHAREDOBJ_USAGE_INIT(pFBO);
#endif
        }

#ifndef IN_GUEST
        CR_STATE_SHAREDOBJ_USAGE_SET(pFBO, g);
#endif
    }

    /* @todo: http://www.opengl.org/registry/specs/ARB/framebuffer_object.txt 
     * FBO status might change when binding a different FBO here...but I doubt it happens.
     * So no status reset here until a proper check.
     */

    switch (target)
    {
        case GL_FRAMEBUFFER_EXT:
            fbo->readFB = pFBO;
            fbo->drawFB = pFBO;
            break;
        case GL_READ_FRAMEBUFFER:
            fbo->readFB = pFBO;
            break;
        case GL_DRAW_FRAMEBUFFER:
            fbo->drawFB = pFBO;
            break;
    }
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
            fb = (CRFramebufferObject*) crHashtableSearch(g->shared->fbTable, framebuffers[i]);
            if (fb)
            {
                if (fbo->readFB==fb)
                {
                    fbo->readFB = NULL;
                }
                if (fbo->drawFB==fb)
                {
                    fbo->drawFB = NULL;
                }
                crHashtableDelete(g->shared->fbTable, framebuffers[i], crStateFreeFBO);
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
    CRFramebufferObject *pFBO;
    GLuint maxtexsizelog2;

    *failed = GL_TRUE;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(((target!=GL_FRAMEBUFFER_EXT) && (target!=GL_READ_FRAMEBUFFER) && (target!=GL_DRAW_FRAMEBUFFER)),
                         GL_INVALID_ENUM, "invalid target");
    pFBO = GL_READ_FRAMEBUFFER==target ? fbo->readFB : fbo->drawFB;
    CRSTATE_FBO_CHECKERR(!pFBO, GL_INVALID_OPERATION, "no fbo bound");
    CRSTATE_FBO_CHECKERR(!crStateGetFBOAttachmentPoint(pFBO, attachment, ap), GL_INVALID_ENUM, "invalid attachment");

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

#ifdef IN_GUEST
    if ((*ap)->type!=GL_TEXTURE || (*ap)->name!=texture || (*ap)->level!=level)
    {
        pFBO->status = GL_FRAMEBUFFER_UNDEFINED;
    }
#endif
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

#ifndef IN_GUEST
    CR_STATE_SHAREDOBJ_USAGE_SET(tobj, g);
#endif

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

#ifndef IN_GUEST
    CR_STATE_SHAREDOBJ_USAGE_SET(tobj, g);
#endif

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

#ifndef IN_GUEST
    CR_STATE_SHAREDOBJ_USAGE_SET(tobj, g);
#endif

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
    CRFramebufferObject *pFBO;
    CRFBOAttachmentPoint *ap;
    CRRenderbufferObject *rb;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(((target!=GL_FRAMEBUFFER_EXT) && (target!=GL_READ_FRAMEBUFFER) && (target!=GL_DRAW_FRAMEBUFFER)),
                         GL_INVALID_ENUM, "invalid target");
    pFBO = GL_READ_FRAMEBUFFER==target ? fbo->readFB : fbo->drawFB;
    CRSTATE_FBO_CHECKERR(!pFBO, GL_INVALID_OPERATION, "no fbo bound");
    CRSTATE_FBO_CHECKERR(!crStateGetFBOAttachmentPoint(pFBO, attachment, &ap), GL_INVALID_ENUM, "invalid attachment");

    if (!renderbuffer)
    {
#ifdef IN_GUEST
        if (ap->type!=GL_NONE)
        {
            pFBO->status = GL_FRAMEBUFFER_UNDEFINED;
        }
#endif
        crStateInitFBOAttachmentPoint(ap);
        return;
    }

    rb = (CRRenderbufferObject*) crHashtableSearch(g->shared->rbTable, renderbuffer);
    CRSTATE_FBO_CHECKERR(!rb, GL_INVALID_OPERATION, "rb doesn't exist");

#ifdef IN_GUEST
        if (ap->type!=GL_RENDERBUFFER_EXT || ap->name!=renderbuffer)
        {
            pFBO->status = GL_FRAMEBUFFER_UNDEFINED;
        }
#endif
    crStateInitFBOAttachmentPoint(ap);
    ap->type = GL_RENDERBUFFER_EXT;
    ap->name = renderbuffer;
}

DECLEXPORT(void) STATE_APIENTRY
crStateGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    CRFramebufferObject *pFBO;
    CRFBOAttachmentPoint *ap;

    CRSTATE_FBO_CHECKERR(g->current.inBeginEnd, GL_INVALID_OPERATION, "called in begin/end");
    CRSTATE_FBO_CHECKERR(((target!=GL_FRAMEBUFFER_EXT) && (target!=GL_READ_FRAMEBUFFER) && (target!=GL_DRAW_FRAMEBUFFER)),
                         GL_INVALID_ENUM, "invalid target");
    pFBO = GL_READ_FRAMEBUFFER==target ? fbo->readFB : fbo->drawFB;
    CRSTATE_FBO_CHECKERR(!pFBO, GL_INVALID_OPERATION, "no fbo bound");
    CRSTATE_FBO_CHECKERR(!crStateGetFBOAttachmentPoint(pFBO, attachment, &ap), GL_INVALID_ENUM, "invalid attachment");

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

static void crStateSyncRenderbuffersCB(unsigned long key, void *data1, void *data2)
{
    CRRenderbufferObject *pRBO = (CRRenderbufferObject*) data1;

    diff_api.GenRenderbuffersEXT(1, &pRBO->hwid);

    diff_api.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, pRBO->hwid);
    diff_api.RenderbufferStorageEXT(GL_RENDERBUFFER_EXT, pRBO->internalformat, pRBO->width, pRBO->height);
}

static void crStateSyncAP(CRFBOAttachmentPoint *pAP, GLenum ap, CRContext *ctx)
{
    CRRenderbufferObject *pRBO;
    CRTextureObj *tobj;

    switch (pAP->type)
    {
        case GL_TEXTURE:
            CRASSERT(pAP->name!=0);
            
            tobj = (CRTextureObj *) crHashtableSearch(ctx->shared->textureTable, pAP->name);
            if (tobj)
            {
                CRASSERT(!tobj->id || tobj->hwid);

                switch (tobj->target)
                {
                    case GL_TEXTURE_1D:
                        diff_api.FramebufferTexture1DEXT(GL_FRAMEBUFFER_EXT, ap, tobj->target, crStateGetTextureObjHWID(tobj), pAP->level);
                        break;
                    case GL_TEXTURE_2D:
                    case GL_TEXTURE_RECTANGLE_ARB:
                        diff_api.FramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, ap, tobj->target, crStateGetTextureObjHWID(tobj), pAP->level);
                        break;
                    case GL_TEXTURE_CUBE_MAP_ARB:
                        diff_api.FramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, ap, pAP->face, crStateGetTextureObjHWID(tobj), pAP->level);
                        break;
                    case GL_TEXTURE_3D:
                        diff_api.FramebufferTexture3DEXT(GL_FRAMEBUFFER_EXT, ap, tobj->target, crStateGetTextureObjHWID(tobj), pAP->level, pAP->zoffset);
                        break;
                    default:
                        crWarning("Unexpected textarget %d", tobj->target);
                }
            }
            else
            {
                crWarning("Unknown texture id %d", pAP->name);
            }
            break;
        case GL_RENDERBUFFER_EXT:
            pRBO = (CRRenderbufferObject*) crHashtableSearch(ctx->shared->rbTable, pAP->name);
            diff_api.FramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, ap, GL_RENDERBUFFER_EXT, pRBO->hwid);
            break;
        case GL_NONE:
            /* Intentionally left blank */
            break;
        default: crWarning("Invalid attachment point type %d (ap: %i)", pAP->type, ap);
    }
}

static void crStateSyncFramebuffersCB(unsigned long key, void *data1, void *data2)
{
    CRFramebufferObject *pFBO = (CRFramebufferObject*) data1;
    CRContext *ctx = (CRContext*) data2;
    GLint i;

    diff_api.GenFramebuffersEXT(1, &pFBO->hwid);

    diff_api.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, pFBO->hwid);

    for (i=0; i<CR_MAX_COLOR_ATTACHMENTS; ++i)
    {
        crStateSyncAP(&pFBO->color[i], GL_COLOR_ATTACHMENT0_EXT+i, ctx);
    }

    crStateSyncAP(&pFBO->depth, GL_DEPTH_ATTACHMENT_EXT, ctx);
    crStateSyncAP(&pFBO->stencil, GL_STENCIL_ATTACHMENT_EXT, ctx);
}

DECLEXPORT(void) STATE_APIENTRY
crStateFramebufferObjectSwitch(CRContext *from, CRContext *to)
{
    if (to->shared->bFBOResyncNeeded)
    {
        to->shared->bFBOResyncNeeded = GL_FALSE;

        crHashtableWalk(to->shared->rbTable, crStateSyncRenderbuffersCB, NULL);
        crHashtableWalk(to->shared->fbTable, crStateSyncFramebuffersCB, to);

        if (to->framebufferobject.drawFB==to->framebufferobject.readFB)
        {
            diff_api.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, to->framebufferobject.drawFB?
                to->framebufferobject.drawFB->hwid:0);
        }
        else
        {
            diff_api.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, to->framebufferobject.drawFB?
                to->framebufferobject.drawFB->hwid:0);

            diff_api.BindFramebufferEXT(GL_READ_FRAMEBUFFER, to->framebufferobject.readFB?
                to->framebufferobject.readFB->hwid:0);
        }

        diff_api.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, to->framebufferobject.renderbuffer?
            to->framebufferobject.renderbuffer->hwid:0);
    } 
    else 
    {
        if (to->framebufferobject.drawFB!=from->framebufferobject.drawFB
            || to->framebufferobject.readFB!=from->framebufferobject.readFB)
        {
            if (to->framebufferobject.drawFB==to->framebufferobject.readFB)
            {
                diff_api.BindFramebufferEXT(GL_FRAMEBUFFER_EXT, to->framebufferobject.drawFB?
                    to->framebufferobject.drawFB->hwid:0);
            }
            else
            {
                diff_api.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, to->framebufferobject.drawFB?
                    to->framebufferobject.drawFB->hwid:0);

                diff_api.BindFramebufferEXT(GL_READ_FRAMEBUFFER, to->framebufferobject.readFB?
                    to->framebufferobject.readFB->hwid:0);
            }

            diff_api.DrawBuffer(to->framebufferobject.drawFB?to->framebufferobject.drawFB->drawbuffer[0]:to->buffer.drawBuffer);
            diff_api.ReadBuffer(to->framebufferobject.readFB?to->framebufferobject.readFB->readbuffer:to->buffer.readBuffer);
        }

        if (to->framebufferobject.renderbuffer!=from->framebufferobject.renderbuffer)
        {
            diff_api.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, to->framebufferobject.renderbuffer?
                to->framebufferobject.renderbuffer->hwid:0);
        }
    }
}

DECLEXPORT(void) STATE_APIENTRY
crStateFramebufferObjectDisableHW(CRContext *ctx)
{
    GLboolean fAdjustDrawReadBuffers = GL_FALSE;

    if (ctx->framebufferobject.drawFB)
    {
        diff_api.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
        fAdjustDrawReadBuffers = GL_TRUE;
    }

    if (ctx->framebufferobject.readFB)
    {
        diff_api.BindFramebufferEXT(GL_READ_FRAMEBUFFER, 0);
        fAdjustDrawReadBuffers = GL_TRUE;
    }

    if (fAdjustDrawReadBuffers)
    {
        diff_api.DrawBuffer(GL_BACK);
        diff_api.ReadBuffer(GL_BACK);
    }

    if (ctx->framebufferobject.renderbuffer)
        diff_api.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
}

DECLEXPORT(void) STATE_APIENTRY
crStateFramebufferObjectReenableHW(CRContext *fromCtx, CRContext *toCtx)
{
    GLboolean fAdjustDrawReadBuffers = GL_FALSE;

    if (fromCtx->framebufferobject.drawFB /* <- the FBO state was reset in crStateFramebufferObjectDisableHW */
            && fromCtx->framebufferobject.drawFB == toCtx->framebufferobject.drawFB) /* .. and it was NOT restored properly in crStateFramebufferObjectSwitch */
    {
        diff_api.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, toCtx->framebufferobject.drawFB->hwid);
        fAdjustDrawReadBuffers = GL_TRUE;
    }

    if (fromCtx->framebufferobject.readFB /* <- the FBO state was reset in crStateFramebufferObjectDisableHW */
            && fromCtx->framebufferobject.readFB == toCtx->framebufferobject.readFB) /* .. and it was NOT restored properly in crStateFramebufferObjectSwitch */
    {
        diff_api.BindFramebufferEXT(GL_READ_FRAMEBUFFER, toCtx->framebufferobject.readFB->hwid);
        fAdjustDrawReadBuffers = GL_TRUE;
    }

    if (fAdjustDrawReadBuffers)
    {
        diff_api.DrawBuffer(toCtx->framebufferobject.drawFB?toCtx->framebufferobject.drawFB->drawbuffer[0]:toCtx->buffer.drawBuffer);
        diff_api.ReadBuffer(toCtx->framebufferobject.readFB?toCtx->framebufferobject.readFB->readbuffer:toCtx->buffer.readBuffer);
    }

    if (fromCtx->framebufferobject.renderbuffer /* <- the FBO state was reset in crStateFramebufferObjectDisableHW */
            && fromCtx->framebufferobject.renderbuffer==toCtx->framebufferobject.renderbuffer) /* .. and it was NOT restored properly in crStateFramebufferObjectSwitch */
    {
        diff_api.BindRenderbufferEXT(GL_RENDERBUFFER_EXT, toCtx->framebufferobject.renderbuffer->hwid);
    }
}


DECLEXPORT(GLuint) STATE_APIENTRY crStateGetFramebufferHWID(GLuint id)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObject *pFBO = (CRFramebufferObject*) crHashtableSearch(g->shared->fbTable, id);
#ifdef DEBUG_misha
    crDebug("FB id(%d) hw(%d)", id, pFBO ? pFBO->hwid : 0);
#endif
    return pFBO ? pFBO->hwid : 0;
}

DECLEXPORT(GLuint) STATE_APIENTRY crStateGetRenderbufferHWID(GLuint id)
{
    CRContext *g = GetCurrentContext();
    CRRenderbufferObject *pRBO = (CRRenderbufferObject*) crHashtableSearch(g->shared->rbTable, id);

    return pRBO ? pRBO->hwid : 0;
}

static void crStateCheckFBOHWIDCB(unsigned long key, void *data1, void *data2)
{
    CRFramebufferObject *pFBO = (CRFramebufferObject *) data1;
    crCheckIDHWID_t *pParms = (crCheckIDHWID_t*) data2;
    (void) key;

    if (pFBO->hwid==pParms->hwid)
        pParms->id = pFBO->id;
}

static void crStateCheckRBOHWIDCB(unsigned long key, void *data1, void *data2)
{
    CRRenderbufferObject *pRBO = (CRRenderbufferObject *) data1;
    crCheckIDHWID_t *pParms = (crCheckIDHWID_t*) data2;
    (void) key;

    if (pRBO->hwid==pParms->hwid)
        pParms->id = pRBO->id;
}

DECLEXPORT(GLuint) STATE_APIENTRY crStateFBOHWIDtoID(GLuint hwid)
{
    CRContext *g = GetCurrentContext();
    crCheckIDHWID_t parms;

    parms.id = hwid;
    parms.hwid = hwid;

    crHashtableWalk(g->shared->fbTable, crStateCheckFBOHWIDCB, &parms);
    return parms.id;
}

DECLEXPORT(GLuint) STATE_APIENTRY crStateRBOHWIDtoID(GLuint hwid)
{
    CRContext *g = GetCurrentContext();
    crCheckIDHWID_t parms;

    parms.id = hwid;
    parms.hwid = hwid;

    crHashtableWalk(g->shared->rbTable, crStateCheckRBOHWIDCB, &parms);
    return parms.id;
}

#ifdef IN_GUEST
DECLEXPORT(GLenum) STATE_APIENTRY crStateCheckFramebufferStatusEXT(GLenum target)
{
    GLenum status = GL_FRAMEBUFFER_UNDEFINED;
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    CRFramebufferObject *pFBO=NULL;

    switch (target)
    {
        case GL_FRAMEBUFFER_EXT:
            pFBO = fbo->drawFB;
            break;
        case GL_READ_FRAMEBUFFER:
            pFBO = fbo->readFB;
            break;
        case GL_DRAW_FRAMEBUFFER:
            pFBO = fbo->drawFB;
            break;
    }

    if (pFBO) status = pFBO->status;

    return status;
}

DECLEXPORT(GLenum) STATE_APIENTRY crStateSetFramebufferStatus(GLenum target, GLenum status)
{
    CRContext *g = GetCurrentContext();
    CRFramebufferObjectState *fbo = &g->framebufferobject;
    CRFramebufferObject *pFBO=NULL;

    switch (target)
    {
        case GL_FRAMEBUFFER_EXT:
            pFBO = fbo->drawFB;
            break;
        case GL_READ_FRAMEBUFFER:
            pFBO = fbo->readFB;
            break;
        case GL_DRAW_FRAMEBUFFER:
            pFBO = fbo->drawFB;
            break;
    }

    if (pFBO) pFBO->status = status;

    return status;
}
#endif
