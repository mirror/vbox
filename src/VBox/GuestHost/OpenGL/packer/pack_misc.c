/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "packer.h"
#include "cr_error.h"

void PACK_APIENTRY crPackChromiumParametervCR(GLenum target, GLenum type, GLsizei count, const GLvoid *values)
{
	GET_PACKER_CONTEXT(pc);
	unsigned int header_length = 2 * sizeof(int) + sizeof(target) + sizeof(type) + sizeof(count);
	unsigned int packet_length;
	unsigned int params_length = 0;
	unsigned char *data_ptr;
	int i, pos;

	switch (type) {
	case GL_BYTE:
	case GL_UNSIGNED_BYTE:
		params_length = sizeof(GLbyte) * count;
		break;
	case GL_SHORT:
	case GL_UNSIGNED_SHORT:
		params_length = sizeof(GLshort) * count;
		break;
	case GL_INT:
	case GL_UNSIGNED_INT:
		params_length = sizeof(GLint) * count;
		break;
	case GL_FLOAT:
		params_length = sizeof(GLfloat) * count;
		break;
#if 0
	case GL_DOUBLE:
		params_length = sizeof(GLdouble) * count;
		break;
#endif
	default:
		__PackError( __LINE__, __FILE__, GL_INVALID_ENUM,
								 "crPackChromiumParametervCR(bad type)" );
		return;
	}

	packet_length = header_length + params_length;

	GET_BUFFERED_POINTER(pc, packet_length );
	WRITE_DATA( 0, GLint, packet_length );
	WRITE_DATA( 4, GLenum, CR_CHROMIUMPARAMETERVCR_EXTEND_OPCODE );
	WRITE_DATA( 8, GLenum, target );
	WRITE_DATA( 12, GLenum, type );
	WRITE_DATA( 16, GLsizei, count );
	WRITE_OPCODE( pc, CR_EXTEND_OPCODE );

	pos = header_length;

	switch (type) {
	case GL_BYTE:
	case GL_UNSIGNED_BYTE:
		for (i = 0; i < count; i++, pos += sizeof(GLbyte)) {
			WRITE_DATA( pos, GLbyte, ((GLbyte *) values)[i]);
		}
		break;
	case GL_SHORT:
	case GL_UNSIGNED_SHORT:
		for (i = 0; i < count; i++, pos += sizeof(GLshort)) {
			WRITE_DATA( pos, GLshort, ((GLshort *) values)[i]);
		}
		break;
	case GL_INT:
	case GL_UNSIGNED_INT:
		for (i = 0; i < count; i++, pos += sizeof(GLint)) {
			WRITE_DATA( pos, GLint, ((GLint *) values)[i]);
		}
		break;
	case GL_FLOAT:
		for (i = 0; i < count; i++, pos += sizeof(GLfloat)) {
			WRITE_DATA( pos, GLfloat, ((GLfloat *) values)[i]);
		}
		break;
#if 0
	case GL_DOUBLE:
		for (i = 0; i < count; i++) {
			WRITE_foo_DATA( sizeof(int) + 12, GLdouble, ((GLdouble *) values)[i]);
		}
		break;
#endif
	default:
		__PackError( __LINE__, __FILE__, GL_INVALID_ENUM,
								 "crPackChromiumParametervCR(bad type)" );
		return;
	}
}
