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

#include "packer.h"
#include "cr_error.h"
#include "cr_string.h"

void PACK_APIENTRY crPackBindAttribLocation(GLuint program, GLuint index, const char *name)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int cbName = crStrlen(name)+1;
    int packet_length = sizeof(int)+sizeof(program)+sizeof(index) + cbName*sizeof(*name);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLuint, program);
    WRITE_DATA(sizeof(int) + sizeof(program), GLuint, index);
    crMemcpy(data_ptr + sizeof(int)+sizeof(program)+sizeof(index), name, cbName*sizeof(*name));
    WRITE_OPCODE(pc, CR_BINDATTRIBLOCATION_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackShaderSource(GLuint shader, GLsizei count, const char **string, const GLint *length)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    GLint *pLocalLength;
    int packet_length = sizeof(int)+sizeof(shader)+sizeof(count)+sizeof(GLint)+count*sizeof(*pLocalLength);
    GLsizei i;

    if ((0==count) || (!string)) return;

    pLocalLength = crAlloc(count*sizeof(*length));
    if (!pLocalLength) return;

    for (i=0; i<count; ++i)
    {
        pLocalLength[i] = (length && (length[i]>=0)) ? length[i] : crStrlen(string[i])+1;
        packet_length += pLocalLength[i];
    }
    
    if (length)
    {
        packet_length += count*sizeof(*length);
    }

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    data_ptr += sizeof(int);
    WRITE_DATA(0, GLuint, shader);
    data_ptr += sizeof(GLuint);
    WRITE_DATA(0, GLsizei, count);
    data_ptr += sizeof(GLsizei);
    WRITE_DATA(0, GLint, (GLint)(length ? 1:0));
    data_ptr += sizeof(GLint);
    crMemcpy(data_ptr, pLocalLength, count*sizeof(*pLocalLength));
    data_ptr += count*sizeof(*pLocalLength);

    if (length)
    {
        crMemcpy(data_ptr, length, count*sizeof(*length));
        data_ptr += count*sizeof(*length);
    }

    for (i=0; i<count; ++i)
    {
        crMemcpy(data_ptr, string[i], pLocalLength[i]);
        data_ptr += pLocalLength[i];
    }
    WRITE_OPCODE(pc, CR_SHADERSOURCE_EXTEND_OPCODE);

    crFree(pLocalLength);
}

void PACK_APIENTRY crPackUniform1fv(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(location)+sizeof(count) + count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLint, location);
    WRITE_DATA(sizeof(int) + sizeof(location), GLsizei, count);
    crMemcpy(data_ptr + sizeof(int)+sizeof(location)+sizeof(count), value, count*sizeof(*value));
    WRITE_OPCODE(pc, CR_UNIFORM1FV_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform1iv(GLint location, GLsizei count, const GLint *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(location)+sizeof(count) + count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLint, location);
    WRITE_DATA(sizeof(int) + sizeof(location), GLsizei, count);
    crMemcpy(data_ptr + sizeof(int)+sizeof(location)+sizeof(count), value, count*sizeof(*value));
    WRITE_OPCODE(pc, CR_UNIFORM1IV_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform2fv(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(location)+sizeof(count) + 2*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLint, location);
    WRITE_DATA(sizeof(int) + sizeof(location), GLsizei, count);
    crMemcpy(data_ptr + sizeof(int)+sizeof(location)+sizeof(count), value, 2*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_UNIFORM2FV_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform2iv(GLint location, GLsizei count, const GLint *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(location)+sizeof(count) + 2*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLint, location);
    WRITE_DATA(sizeof(int) + sizeof(location), GLsizei, count);
    crMemcpy(data_ptr + sizeof(int)+sizeof(location)+sizeof(count), value, 2*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_UNIFORM2IV_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform3fv(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(location)+sizeof(count) + 3*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLint, location);
    WRITE_DATA(sizeof(int) + sizeof(location), GLsizei, count);
    crMemcpy(data_ptr + sizeof(int)+sizeof(location)+sizeof(count), value, 3*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_UNIFORM3FV_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform3iv(GLint location, GLsizei count, const GLint *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(location)+sizeof(count) + 3*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLint, location);
    WRITE_DATA(sizeof(int) + sizeof(location), GLsizei, count);
    crMemcpy(data_ptr + sizeof(int)+sizeof(location)+sizeof(count), value, 3*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_UNIFORM3IV_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(location)+sizeof(count) + 4*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLint, location);
    WRITE_DATA(sizeof(int) + sizeof(location), GLsizei, count);
    crMemcpy(data_ptr + sizeof(int)+sizeof(location)+sizeof(count), value, 4*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_UNIFORM4FV_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform4iv(GLint location, GLsizei count, const GLint *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(location)+sizeof(count) + 4*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLint, location);
    WRITE_DATA(sizeof(int) + sizeof(location), GLsizei, count);
    crMemcpy(data_ptr + sizeof(int)+sizeof(location)+sizeof(count), value, 4*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_UNIFORM4IV_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(location)+sizeof(count)+sizeof(transpose) + 2*2*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLint, location);
    WRITE_DATA(sizeof(int) + sizeof(location), GLsizei, count);
    WRITE_DATA(sizeof(int) + sizeof(location) + sizeof(count), GLboolean, transpose);
    crMemcpy(data_ptr + sizeof(int)+sizeof(location)+sizeof(count)+sizeof(transpose), value, 2*2*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_UNIFORMMATRIX2FV_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(location)+sizeof(count)+sizeof(transpose) + 3*3*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLint, location);
    WRITE_DATA(sizeof(int) + sizeof(location), GLsizei, count);
    WRITE_DATA(sizeof(int) + sizeof(location) + sizeof(count), GLboolean, transpose);
    crMemcpy(data_ptr + sizeof(int)+sizeof(location)+sizeof(count)+sizeof(transpose), value, 3*3*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_UNIFORMMATRIX3FV_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(location)+sizeof(count)+sizeof(transpose) + 4*4*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA(0, int, packet_length);
    WRITE_DATA(sizeof(int), GLint, location);
    WRITE_DATA(sizeof(int) + sizeof(location), GLsizei, count);
    WRITE_DATA(sizeof(int) + sizeof(location) + sizeof(count), GLboolean, transpose);
    crMemcpy(data_ptr + sizeof(int)+sizeof(location)+sizeof(count)+sizeof(transpose), value, 4*4*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_UNIFORMMATRIX4FV_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackBindAttribLocationSWAP(GLuint program, GLuint index, const char *name)
{
    GET_PACKER_CONTEXT(pc);
    (void)program;
    (void)index;
    (void)name;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackShaderSourceSWAP(GLuint shader, GLsizei count, const char **string, const GLint *length)
{
    GET_PACKER_CONTEXT(pc);
    (void)shader;
    (void)count;
    (void)string;
    (void)length;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackUniform1fvSWAP(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    (void)location;
    (void)count;
    (void)value;
    crError ("No swap version");
    (void) pc;
}


void PACK_APIENTRY crPackUniform1ivSWAP(GLint location, GLsizei count, const GLint *value)
{
    GET_PACKER_CONTEXT(pc);
    (void)location;
    (void)count;
    (void)value;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackUniform2fvSWAP(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    (void)location;
    (void)count;
    (void)value;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackUniform2ivSWAP(GLint location, GLsizei count, const GLint * value)
{
    GET_PACKER_CONTEXT(pc);
    (void)location;
    (void)count;
    (void)value;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackUniform3fvSWAP(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    (void)location;
    (void)count;
    (void)value;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackUniform3ivSWAP(GLint location, GLsizei count, const GLint *value)
{
    GET_PACKER_CONTEXT(pc);
    (void)location;
    (void)count;
    (void)value;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackUniform4fvSWAP(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    (void)location;
    (void)count;
    (void)value;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackUniform4ivSWAP(GLint location, GLsizei count, const GLint *value)
{
    GET_PACKER_CONTEXT(pc);
    (void)location;
    (void)count;
    (void)value;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackUniformMatrix2fvSWAP(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackUniformMatrix3fvSWAP(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackUniformMatrix4fvSWAP(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    crError ("No swap version");
    (void) pc;
}
