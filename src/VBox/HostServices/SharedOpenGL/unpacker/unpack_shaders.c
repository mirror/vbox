/* $Id$ */

/** @file
 * VBox OpenGL DRI driver functions
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include "unpacker.h"
#include "cr_error.h"
#include "cr_protocol.h"
#include "cr_mem.h"
#include "cr_string.h"
#include "cr_version.h"

#ifdef CR_OPENGL_VERSION_2_0

void crUnpackExtendBindAttribLocation(void)
{
    GLuint program   = READ_DATA(8, GLuint);
    GLuint index     = READ_DATA(12, GLuint);
    const char *name = DATA_POINTER(16, const char);

    cr_unpackDispatch.BindAttribLocation(program, index, name);
}

void crUnpackExtendShaderSource(void)
{
    GLint *length = NULL;
    GLuint shader = READ_DATA(8, GLuint);
    GLsizei count = READ_DATA(12, GLsizei);
    GLint hasNonLocalLen = READ_DATA(16, GLsizei);
    GLint *pLocalLength = DATA_POINTER(20, GLint);
    char **ppStrings = NULL;
    GLsizei i;
    int pos=20+count*sizeof(*pLocalLength);

    if (hasNonLocalLen>0)
    {
        length = DATA_POINTER(pos, GLint);
        pos += count*sizeof(*length);
    }

    ppStrings = crAlloc(count*sizeof(char*));
    if (!ppStrings) return;

    for (i=0; i<count; ++i)
    {
        ppStrings[i] = DATA_POINTER(pos, char);
        pos += pLocalLength[i];
        if (!length)
        {
            pLocalLength[i] -= 1;
        }
    }

    cr_unpackDispatch.ShaderSource(shader, count, ppStrings, length ? length : pLocalLength);
    crFree(ppStrings);
}

void crUnpackExtendUniform1fv(void)
{
    GLint location = READ_DATA(8, GLint);
    GLsizei count = READ_DATA(12, GLsizei);
    const GLfloat *value = DATA_POINTER(16, const GLfloat);
    cr_unpackDispatch.Uniform1fv(location, count, value);
}

void crUnpackExtendUniform1iv(void)
{
    GLint location = READ_DATA(8, GLint);
    GLsizei count = READ_DATA(12, GLsizei);
    const GLint *value = DATA_POINTER(16, const GLint);
    cr_unpackDispatch.Uniform1iv(location, count, value);
}

void crUnpackExtendUniform2fv(void)
{
    GLint location = READ_DATA(8, GLint);
    GLsizei count = READ_DATA(12, GLsizei);
    const GLfloat *value = DATA_POINTER(16, const GLfloat);
    cr_unpackDispatch.Uniform2fv(location, count, value);
}

void crUnpackExtendUniform2iv(void)
{
    GLint location = READ_DATA(8, GLint);
    GLsizei count = READ_DATA(12, GLsizei);
    const GLint *value = DATA_POINTER(16, const GLint);
    cr_unpackDispatch.Uniform2iv(location, count, value);
}

void crUnpackExtendUniform3fv(void)
{
    GLint location = READ_DATA(8, GLint);
    GLsizei count = READ_DATA(12, GLsizei);
    const GLfloat *value = DATA_POINTER(16, const GLfloat);
    cr_unpackDispatch.Uniform3fv(location, count, value);
}

void crUnpackExtendUniform3iv(void)
{
    GLint location = READ_DATA(8, GLint);
    GLsizei count = READ_DATA(12, GLsizei);
    const GLint *value = DATA_POINTER(16, const GLint);
    cr_unpackDispatch.Uniform3iv(location, count, value);
}

void crUnpackExtendUniform4fv(void)
{
    GLint location = READ_DATA(8, GLint);
    GLsizei count = READ_DATA(12, GLsizei);
    const GLfloat *value = DATA_POINTER(16, const GLfloat);
    cr_unpackDispatch.Uniform4fv(location, count, value);
}

void crUnpackExtendUniform4iv(void)
{
    GLint location = READ_DATA(8, GLint);
    GLsizei count = READ_DATA(12, GLsizei);
    const GLint *value = DATA_POINTER(16, const GLint);
    cr_unpackDispatch.Uniform4iv(location, count, value);
}

void crUnpackExtendUniformMatrix2fv(void)
{
    GLint location = READ_DATA(8, GLint);
    GLsizei count = READ_DATA(12, GLsizei);
    GLboolean transpose = READ_DATA(16, GLboolean);
    const GLfloat *value = DATA_POINTER(16+sizeof(GLboolean), const GLfloat);
    cr_unpackDispatch.UniformMatrix2fv(location, count, transpose, value);
}

void crUnpackExtendUniformMatrix3fv(void)
{
    GLint location = READ_DATA(8, GLint);
    GLsizei count = READ_DATA(12, GLsizei);
    GLboolean transpose = READ_DATA(16, GLboolean);
    const GLfloat *value = DATA_POINTER(16+sizeof(GLboolean), const GLfloat);
    cr_unpackDispatch.UniformMatrix3fv(location, count, transpose, value);
}

void crUnpackExtendUniformMatrix4fv(void)
{
    GLint location = READ_DATA(8, GLint);
    GLsizei count = READ_DATA(12, GLsizei);
    GLboolean transpose = READ_DATA(16, GLboolean);
    const GLfloat *value = DATA_POINTER(16+sizeof(GLboolean), const GLfloat);
    cr_unpackDispatch.UniformMatrix4fv(location, count, transpose, value);
}

#endif /* #ifdef CR_OPENGL_VERSION_2_0 */