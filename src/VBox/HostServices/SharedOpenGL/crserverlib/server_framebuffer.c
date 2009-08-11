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
