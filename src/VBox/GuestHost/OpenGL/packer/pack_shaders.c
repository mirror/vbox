/* $Id$ */

/** @file
 * VBox OpenGL DRI driver functions
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

#include "packer.h"
#include "cr_error.h"
#include "cr_string.h"

void PACK_APIENTRY crPackBindAttribLocation(GLuint program, GLuint index, const char *name)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int cbName = crStrlen(name)+1;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(program)+sizeof(index) + cbName*sizeof(*name);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_BINDATTRIBLOCATION_EXTEND_OPCODE);
    WRITE_DATA_AI(GLuint, program);
    WRITE_DATA_AI(GLuint, index);
    crMemcpy(data_ptr, name, cbName*sizeof(*name));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackShaderSource(GLuint shader, GLsizei count, const char **string, const GLint *length)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    GLint *pLocalLength;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(shader)+sizeof(count)+sizeof(GLint)+count*sizeof(*pLocalLength);
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
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_SHADERSOURCE_EXTEND_OPCODE);
    WRITE_DATA_AI(GLuint, shader);
    WRITE_DATA_AI(GLsizei, count);
    WRITE_DATA_AI(GLint, (GLint)(length ? 1:0));
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
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);

    crFree(pLocalLength);
}

void PACK_APIENTRY crPackUniform1fv(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(location)+sizeof(count) + count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_UNIFORM1FV_EXTEND_OPCODE);
    WRITE_DATA_AI(GLint, location);
    WRITE_DATA_AI(GLsizei, count);
    crMemcpy(data_ptr, value, count*sizeof(*value));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform1iv(GLint location, GLsizei count, const GLint *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(location)+sizeof(count) + count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_UNIFORM1IV_EXTEND_OPCODE);
    WRITE_DATA_AI(GLint, location);
    WRITE_DATA_AI(GLsizei, count);
    crMemcpy(data_ptr, value, count*sizeof(*value));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform2fv(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(location)+sizeof(count) + 2*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_UNIFORM2FV_EXTEND_OPCODE);
    WRITE_DATA_AI(GLint, location);
    WRITE_DATA_AI(GLsizei, count);
    crMemcpy(data_ptr, value, 2*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform2iv(GLint location, GLsizei count, const GLint *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(location)+sizeof(count) + 2*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_UNIFORM2IV_EXTEND_OPCODE);
    WRITE_DATA_AI(GLint, location);
    WRITE_DATA_AI(GLsizei, count);
    crMemcpy(data_ptr, value, 2*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform3fv(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(location)+sizeof(count) + 3*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_UNIFORM3FV_EXTEND_OPCODE);
    WRITE_DATA_AI(GLint, location);
    WRITE_DATA_AI(GLsizei, count);
    crMemcpy(data_ptr, value, 3*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform3iv(GLint location, GLsizei count, const GLint *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(location)+sizeof(count) + 3*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_UNIFORM3IV_EXTEND_OPCODE);
    WRITE_DATA_AI(GLint, location);
    WRITE_DATA_AI(GLsizei, count);
    crMemcpy(data_ptr, value, 3*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(location)+sizeof(count) + 4*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_UNIFORM4FV_EXTEND_OPCODE);
    WRITE_DATA_AI(GLint, location);
    WRITE_DATA_AI(GLsizei, count);
    crMemcpy(data_ptr, value, 4*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniform4iv(GLint location, GLsizei count, const GLint *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(location)+sizeof(count) + 4*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_UNIFORM4IV_EXTEND_OPCODE);
    WRITE_DATA_AI(GLint, location);
    WRITE_DATA_AI(GLsizei, count);
    crMemcpy(data_ptr, value, 4*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(location)+sizeof(count)+sizeof(transpose) + 2*2*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_UNIFORMMATRIX2FV_EXTEND_OPCODE);
    WRITE_DATA_AI(GLint, location);
    WRITE_DATA_AI(GLsizei, count);
    WRITE_DATA_AI(GLboolean, transpose);
    crMemcpy(data_ptr, value, 2*2*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(location)+sizeof(count)+sizeof(transpose) + 3*3*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_UNIFORMMATRIX3FV_EXTEND_OPCODE);
    WRITE_DATA_AI(GLint, location);
    WRITE_DATA_AI(GLsizei, count);
    WRITE_DATA_AI(GLboolean, transpose);
    crMemcpy(data_ptr, value, 3*3*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(location)+sizeof(count)+sizeof(transpose) + 4*4*count*sizeof(*value);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_UNIFORMMATRIX4FV_EXTEND_OPCODE);
    WRITE_DATA_AI(GLint, location);
    WRITE_DATA_AI(GLsizei, count);
    WRITE_DATA_AI(GLboolean, transpose);
    crMemcpy(data_ptr, value, 4*4*count*sizeof(*value));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackDrawBuffers(GLsizei n, const GLenum *bufs)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(n) + n*sizeof(*bufs);

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_DRAWBUFFERS_EXTEND_OPCODE);
    WRITE_DATA_AI(GLsizei, n);
    crMemcpy(data_ptr, bufs, n*sizeof(*bufs));
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

/*@todo next 8 functions are bit hacky, 
 * we expect packspu to pass a single structure with all output parameters via first output pointer.
 * it'd be better to add CRMessageMultiReadback one day.
 */
void PACK_APIENTRY crPackGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, char * name, int * writeback)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pc;
    (void) size;
    (void) type;
    (void) name;
    GET_BUFFERED_POINTER(pc, 36);
    WRITE_DATA(0, GLint, 36);
    WRITE_DATA(4, GLenum, CR_GETACTIVEATTRIB_EXTEND_OPCODE);
    WRITE_DATA(8, GLuint, program);
    WRITE_DATA(12, GLuint, index);
    WRITE_DATA(16, GLsizei, bufSize);
    WRITE_NETWORK_POINTER(20, (void *) length);
    WRITE_NETWORK_POINTER(28, (void *) writeback);
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, char * name, int * writeback)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pc;
    (void) size;
    (void) type;
    (void) name;
    GET_BUFFERED_POINTER(pc, 36);
    WRITE_DATA(0, GLint, 36);
    WRITE_DATA(4, GLenum, CR_GETACTIVEUNIFORM_EXTEND_OPCODE);
    WRITE_DATA(8, GLuint, program);
    WRITE_DATA(12, GLuint, index);
    WRITE_DATA(16, GLsizei, bufSize);
    WRITE_NETWORK_POINTER(20, (void *) length);
    WRITE_NETWORK_POINTER(28, (void *) writeback);
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei * count, GLuint * shaders, int * writeback)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pc;
    (void) shaders;
    GET_BUFFERED_POINTER(pc, 32);
    WRITE_DATA(0, GLint, 32);
    WRITE_DATA(4, GLenum, CR_GETATTACHEDSHADERS_EXTEND_OPCODE);
    WRITE_DATA(8, GLuint, program);
    WRITE_DATA(12, GLsizei, maxCount);
    WRITE_NETWORK_POINTER(16, (void *) count);
    WRITE_NETWORK_POINTER(24, (void *) writeback);
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackGetAttachedObjectsARB(GLhandleARB containerObj, GLsizei maxCount, GLsizei * count, GLhandleARB * obj, int * writeback)
{
	GET_PACKER_CONTEXT(pc);
	unsigned char *data_ptr;
	(void) pc;
	GET_BUFFERED_POINTER(pc, 32);
	WRITE_DATA(0, GLint, 32);
	WRITE_DATA(4, GLenum, CR_GETATTACHEDOBJECTSARB_EXTEND_OPCODE);
	WRITE_DATA(8, GLhandleARB, containerObj);
	WRITE_DATA(12, GLsizei, maxCount);
	WRITE_NETWORK_POINTER(16, (void *) count);
	WRITE_NETWORK_POINTER(24, (void *) writeback);
	WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackGetInfoLogARB(GLhandleARB obj, GLsizei maxLength, GLsizei * length, GLcharARB * infoLog, int * writeback)
{
	GET_PACKER_CONTEXT(pc);
	unsigned char *data_ptr;
	(void) pc;
	GET_BUFFERED_POINTER(pc, 32);
	WRITE_DATA(0, GLint, 32);
	WRITE_DATA(4, GLenum, CR_GETINFOLOGARB_EXTEND_OPCODE);
	WRITE_DATA(8, GLhandleARB, obj);
	WRITE_DATA(12, GLsizei, maxLength);
	WRITE_NETWORK_POINTER(16, (void *) length);
	WRITE_NETWORK_POINTER(24, (void *) writeback);
	WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei * length, char * infoLog, int * writeback)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pc;
    (void) infoLog;
    GET_BUFFERED_POINTER(pc, 32);
    WRITE_DATA(0, GLint, 32);
    WRITE_DATA(4, GLenum, CR_GETPROGRAMINFOLOG_EXTEND_OPCODE);
    WRITE_DATA(8, GLuint, program);
    WRITE_DATA(12, GLsizei, bufSize);
    WRITE_NETWORK_POINTER(16, (void *) length);
    WRITE_NETWORK_POINTER(24, (void *) writeback);
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei * length, char * infoLog, int * writeback)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pc;
    (void) infoLog;
    GET_BUFFERED_POINTER(pc, 32);
    WRITE_DATA(0, GLint, 32);
    WRITE_DATA(4, GLenum, CR_GETSHADERINFOLOG_EXTEND_OPCODE);
    WRITE_DATA(8, GLuint, shader);
    WRITE_DATA(12, GLsizei, bufSize);
    WRITE_NETWORK_POINTER(16, (void *) length);
    WRITE_NETWORK_POINTER(24, (void *) writeback);
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei * length, char * source, int * writeback)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pc;
    (void) source;
    GET_BUFFERED_POINTER(pc, 32);
    WRITE_DATA(0, GLint, 32);
    WRITE_DATA(4, GLenum, CR_GETSHADERSOURCE_EXTEND_OPCODE);
    WRITE_DATA(8, GLuint, shader);
    WRITE_DATA(12, GLsizei, bufSize);
    WRITE_NETWORK_POINTER(16, (void *) length);
    WRITE_NETWORK_POINTER(24, (void *) writeback);
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackGetUniformsLocations(GLuint program, GLsizei maxcbData, GLsizei * cbData, GLvoid * pData, int * writeback)
{
	GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pData;
    GET_BUFFERED_POINTER(pc, 32);
    WRITE_DATA(0, GLint, 32);
    WRITE_DATA(4, GLenum, CR_GETUNIFORMSLOCATIONS_EXTEND_OPCODE);
    WRITE_DATA(8, GLuint, program);
    WRITE_DATA(12, GLsizei, maxcbData);
    WRITE_NETWORK_POINTER(16, (void *) cbData);
    WRITE_NETWORK_POINTER(24, (void *) writeback);
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackGetAttribLocation(GLuint program, const char * name, GLint * return_value, int * writeback)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int cbName = crStrlen(name)+1;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(program)+cbName*sizeof(*name)+16;

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_GETATTRIBLOCATION_EXTEND_OPCODE);
    WRITE_DATA_AI(GLuint, program);
    crMemcpy(data_ptr, name, cbName*sizeof(*name));
    data_ptr +=  cbName*sizeof(*name);
    WRITE_NETWORK_POINTER(0, (void *) return_value);
    WRITE_NETWORK_POINTER(8, (void *) writeback);
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
}

void PACK_APIENTRY crPackGetUniformLocation(GLuint program, const char * name, GLint * return_value, int * writeback)
{
    GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    int cbName = crStrlen(name)+1;
    int packet_length = sizeof(int)+sizeof(GLenum)+sizeof(program)+cbName*sizeof(*name)+16;

    GET_BUFFERED_POINTER(pc, packet_length);
    WRITE_DATA_AI(int, packet_length);
    WRITE_DATA_AI(GLenum, CR_GETUNIFORMLOCATION_EXTEND_OPCODE);
    WRITE_DATA_AI(GLuint, program);
    crMemcpy(data_ptr, name, cbName*sizeof(*name));
    data_ptr +=  cbName*sizeof(*name);
    WRITE_NETWORK_POINTER(0, (void *) return_value);
    WRITE_NETWORK_POINTER(8, (void *) writeback);
    WRITE_OPCODE(pc, CR_EXTEND_OPCODE);
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

void PACK_APIENTRY crPackDrawBuffersSWAP(GLsizei n, const GLenum *bufs)
{
    GET_PACKER_CONTEXT(pc);
    (void)n;
    (void)bufs;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackGetAttribLocationSWAP(GLuint program, const char * name, GLint * return_value, int * writeback)
{
    GET_PACKER_CONTEXT(pc);
    (void)program;
    (void)name;
    (void)return_value;
    (void)writeback;
    crError ("No swap version");
    (void) pc;
}

void PACK_APIENTRY crPackGetUniformLocationSWAP(GLuint program, const char * name, GLint * return_value, int * writeback)
{
    GET_PACKER_CONTEXT(pc);
    (void)program;
    (void)name;
    (void)return_value;
    (void)writeback;
    crError ("No swap version");
    (void) pc;
}
