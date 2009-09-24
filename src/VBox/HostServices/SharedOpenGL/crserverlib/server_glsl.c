/* $Id$ */

/** @file
 * VBox OpenGL: GLSL related fucntions
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
#include "cr_error.h"
#include "cr_mem.h"
#include "cr_net.h"
#include "server_dispatch.h"
#include "server.h"

#ifdef CR_OPENGL_VERSION_2_0

void SERVER_DISPATCH_APIENTRY crServerDispatchShaderSource(GLuint shader, GLsizei count, const char ** string, const GLint * length)
{
    /*@todo?crStateShaderSource(shader...);*/
    cr_server.head_spu->dispatch_table.ShaderSource(crStateGetShaderHWID(shader), count, string, length);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchCompileShader(GLuint shader)
{
    crStateCompileShader(shader);
    cr_server.head_spu->dispatch_table.CompileShader(crStateGetShaderHWID(shader));
}

void SERVER_DISPATCH_APIENTRY crServerDispatchDeleteShader(GLuint shader)
{
    crStateDeleteShader(shader);
    cr_server.head_spu->dispatch_table.DeleteShader(crStateGetShaderHWID(shader));
}

void SERVER_DISPATCH_APIENTRY crServerDispatchAttachShader(GLuint program, GLuint shader)
{
    crStateAttachShader(program, shader);
    cr_server.head_spu->dispatch_table.AttachShader(crStateGetProgramHWID(program), crStateGetShaderHWID(shader));
}

void SERVER_DISPATCH_APIENTRY crServerDispatchDetachShader(GLuint program, GLuint shader)
{
    crStateDetachShader(program, shader);
    cr_server.head_spu->dispatch_table.DetachShader(crStateGetProgramHWID(program), crStateGetShaderHWID(shader));
}

void SERVER_DISPATCH_APIENTRY crServerDispatchLinkProgram(GLuint program)
{
    crStateLinkProgram(program);
    cr_server.head_spu->dispatch_table.LinkProgram(crStateGetProgramHWID(program));
}

void SERVER_DISPATCH_APIENTRY crServerDispatchUseProgram(GLuint program)
{
    crStateUseProgram(program);
    cr_server.head_spu->dispatch_table.UseProgram(crStateGetProgramHWID(program));
}

void SERVER_DISPATCH_APIENTRY crServerDispatchDeleteProgram(GLuint program)
{
    crStateDeleteProgram(program);
    cr_server.head_spu->dispatch_table.DeleteProgram(crStateGetProgramHWID(program));
}

void SERVER_DISPATCH_APIENTRY crServerDispatchValidateProgram(GLuint program)
{
    crStateValidateProgram(program);
    cr_server.head_spu->dispatch_table.ValidateProgram(crStateGetProgramHWID(program));
}

void SERVER_DISPATCH_APIENTRY crServerDispatchBindAttribLocation(GLuint program, GLuint index, const char * name)
{
    crStateBindAttribLocation(program, index, name);
    cr_server.head_spu->dispatch_table.BindAttribLocation(crStateGetProgramHWID(program), index, name);
}

void SERVER_DISPATCH_APIENTRY crServerDispatchDeleteObjectARB(GLhandleARB obj)
{
    GLuint hwid = crStateGetProgramHWID(obj);

    if (!hwid)
    {
        hwid = crStateGetShaderHWID(obj);
        crStateDeleteShader(obj);
    }
    else
    {
        crStateDeleteProgram(obj);
    }

    cr_server.head_spu->dispatch_table.DeleteObjectARB(hwid);
}

#endif /* #ifdef CR_OPENGL_VERSION_2_0 */
