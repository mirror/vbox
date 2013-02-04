/* $Id$ */

/** @file
 * VBox OpenGL: GLSL related functions
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
    GLuint shaderHW = crStateGetShaderHWID(shader);
    crStateDeleteShader(shader);
    if (shaderHW)
        cr_server.head_spu->dispatch_table.DeleteShader(shaderHW);
    else
        crWarning("crServerDispatchDeleteShader: hwid not found for shader(%d)", shader);
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
    GLuint hwId = crStateGetProgramHWID(program);
    crStateDeleteProgram(program);
    if (hwId)
        cr_server.head_spu->dispatch_table.DeleteProgram(hwId);
    else
        crWarning("crServerDispatchDeleteProgram: hwid not found for program(%d)", program);
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
        CRASSERT(hwid);
        crStateDeleteShader(obj);
    }
    else
    {
        crStateDeleteProgram(obj);
    }

    if (hwid)
        cr_server.head_spu->dispatch_table.DeleteObjectARB(hwid);
}

GLint SERVER_DISPATCH_APIENTRY crServerDispatchGetAttribLocation( GLuint program, const char * name )
{
    GLint retval;
    retval = cr_server.head_spu->dispatch_table.GetAttribLocation(crStateGetProgramHWID(program), name );
    crServerReturnValue( &retval, sizeof(retval) );
    return retval; /* WILL PROBABLY BE IGNORED */
}

GLhandleARB SERVER_DISPATCH_APIENTRY crServerDispatchGetHandleARB( GLenum pname )
{
    GLhandleARB retval;
    retval = cr_server.head_spu->dispatch_table.GetHandleARB(pname);
    if (pname==GL_PROGRAM_OBJECT_ARB)
    {
        retval = crStateGLSLProgramHWIDtoID(retval);
    }
    crServerReturnValue( &retval, sizeof(retval) );
    return retval; /* WILL PROBABLY BE IGNORED */
}

GLint SERVER_DISPATCH_APIENTRY crServerDispatchGetUniformLocation(GLuint program, const char * name)
{
    GLint retval;
    retval = cr_server.head_spu->dispatch_table.GetUniformLocation(crStateGetProgramHWID(program), name);
    crServerReturnValue( &retval, sizeof(retval) );
    return retval; /* WILL PROBABLY BE IGNORED */
}

#endif /* #ifdef CR_OPENGL_VERSION_2_0 */
