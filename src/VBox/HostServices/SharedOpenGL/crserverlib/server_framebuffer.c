/* $Id$ */

/** @file
 * VBox OpenGL: EXT_framebuffer_object
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

#include "cr_spu.h"
#include "chromium.h"
#include "cr_mem.h"
#include "cr_net.h"
#include "server_dispatch.h"
#include "server.h"

void SERVER_DISPATCH_APIENTRY
crServerDispatchGenFramebuffersEXT(GLsizei n, GLuint *framebuffers)
{
    GLuint *local_buffers = (GLuint *) crAlloc(n * sizeof(*local_buffers));
    (void) framebuffers;
    cr_server.head_spu->dispatch_table.GenFramebuffersEXT(n, local_buffers);
    crServerReturnValue(local_buffers, n * sizeof(*local_buffers));
    crFree(local_buffers);
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchGenRenderbuffersEXT(GLsizei n, GLuint *renderbuffers)
{
    GLuint *local_buffers = (GLuint *) crAlloc(n * sizeof(*local_buffers));
    (void) renderbuffers;
    cr_server.head_spu->dispatch_table.GenFramebuffersEXT(n, local_buffers);
    crServerReturnValue(local_buffers, n * sizeof(*local_buffers));
    crFree(local_buffers);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    if (texture)
    {
        texture = crServerTranslateTextureID(texture);
    }

    crStateFramebufferTexture1DEXT(target, attachment, textarget, texture, level);
    cr_server.head_spu->dispatch_table.FramebufferTexture1DEXT(target, attachment, textarget, texture, level);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    if (texture)
    {
        texture = crServerTranslateTextureID(texture);
    }

    crStateFramebufferTexture2DEXT(target, attachment, textarget, texture, level);
    cr_server.head_spu->dispatch_table.FramebufferTexture2DEXT(target, attachment, textarget, texture, level);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
    if (texture)
    {
        texture = crServerTranslateTextureID(texture);
    }

    crStateFramebufferTexture3DEXT(target, attachment, textarget, texture, level, zoffset);
    cr_server.head_spu->dispatch_table.FramebufferTexture3DEXT(target, attachment, textarget, texture, level, zoffset);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchBindFramebufferEXT(GLenum target, GLuint framebuffer)
{
	crStateBindFramebufferEXT(target, framebuffer);
	cr_server.head_spu->dispatch_table.BindFramebufferEXT(target, crStateGetFramebufferHWID(framebuffer));
}

void SERVER_DISPATCH_APIENTRY crServerDispatchBindRenderbufferEXT(GLenum target, GLuint renderbuffer)
{
	crStateBindRenderbufferEXT(target, renderbuffer);
	cr_server.head_spu->dispatch_table.BindRenderbufferEXT(target, crStateGetRenderbufferHWID(renderbuffer));
}

void SERVER_DISPATCH_APIENTRY crServerDispatchDeleteFramebuffersEXT(GLsizei n, const GLuint * framebuffers)
{
	crStateDeleteFramebuffersEXT(n, framebuffers);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchDeleteRenderbuffersEXT(GLsizei n, const GLuint * renderbuffers)
{
	crStateDeleteRenderbuffersEXT(n, renderbuffers);
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	crStateFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, renderbuffer);
	cr_server.head_spu->dispatch_table.FramebufferRenderbufferEXT(target, attachment, renderbuffertarget, crStateGetRenderbufferHWID(renderbuffer));
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment, GLenum pname, GLint * params)
{
	GLint local_params[1];
	(void) params;
	crStateGetFramebufferAttachmentParameterivEXT(target, attachment, pname, local_params);

    if (GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT==pname)
    {
        GLint type;
        crStateGetFramebufferAttachmentParameterivEXT(target, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT, &type);
        if (GL_TEXTURE==type)
        {
            /*todo, add reverse of crServerTranslateTextureID*/
            if (!cr_server.sharedTextureObjects && local_params[0])
            {
                int client = cr_server.curClient->number;
                local_params[0] = local_params[0] - client * 100000;
            }
        }
    }

	crServerReturnValue(&(local_params[0]), 1*sizeof(GLint));
}
