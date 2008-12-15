/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_error.h"
#include "unpack_extend.h"
#include "unpacker.h"
/**
 * \mainpage Unpacker 
 *
 * \section UnpackerIntroduction Introduction
 *
 * Chromium consists of all the top-level files in the cr
 * directory.  The unpacker module basically takes care of API dispatch,
 * and OpenGL state management.
 *
 */

void crUnpackExtendVertexPointer(void)
{
	GLint size = READ_DATA( 8, GLint );
	GLenum type = READ_DATA( 12, GLenum );
	GLsizei stride = READ_DATA( 16, GLsizei );
	GLintptrARB pointer = READ_DATA( 20, GLintptrARB );
	cr_unpackDispatch.VertexPointer( size, type, stride, (void *) pointer );
}

void crUnpackExtendTexCoordPointer(void)
{
	GLint size = READ_DATA( 8, GLint );
	GLenum type = READ_DATA( 12, GLenum );
	GLsizei stride = READ_DATA( 16, GLsizei );
	GLintptrARB pointer = READ_DATA( 20, GLintptrARB );
	cr_unpackDispatch.TexCoordPointer( size, type, stride, (void *) pointer );
}

void crUnpackExtendNormalPointer(void)
{
	GLenum type = READ_DATA( 8, GLenum );
	GLsizei stride = READ_DATA( 12, GLsizei );
	GLintptrARB pointer = READ_DATA( 16, GLintptrARB );
	cr_unpackDispatch.NormalPointer( type, stride, (void *) pointer );
}

void crUnpackExtendIndexPointer(void)
{
	GLenum type = READ_DATA( 8, GLenum );
	GLsizei stride = READ_DATA( 12, GLsizei );
	GLintptrARB pointer = READ_DATA( 16, GLintptrARB );
	cr_unpackDispatch.IndexPointer( type, stride, (void *) pointer );
}

void crUnpackExtendEdgeFlagPointer(void)
{
	GLsizei stride = READ_DATA( 8, GLsizei );
	GLintptrARB pointer = READ_DATA( 12, GLintptrARB );
	cr_unpackDispatch.EdgeFlagPointer( stride, (void *) pointer );
}

void crUnpackExtendColorPointer(void)
{
	GLint size = READ_DATA( 8, GLint );
	GLenum type = READ_DATA( 12, GLenum );
	GLsizei stride = READ_DATA( 16, GLsizei );
	GLintptrARB pointer = READ_DATA( 20, GLintptrARB );
	cr_unpackDispatch.ColorPointer( size, type, stride, (void *) pointer );
}

void crUnpackExtendFogCoordPointerEXT(void)
{
	GLenum type = READ_DATA( 8, GLenum );
	GLsizei stride = READ_DATA( 12, GLsizei );
	GLintptrARB pointer = READ_DATA( 16, GLintptrARB );
	cr_unpackDispatch.FogCoordPointerEXT( type, stride, (void *) pointer );
}

void crUnpackExtendSecondaryColorPointerEXT(void)
{
	GLint size = READ_DATA( 8, GLint );
	GLenum type = READ_DATA( 12, GLenum );
	GLsizei stride = READ_DATA( 16, GLsizei );
	GLintptrARB pointer = READ_DATA( 20, GLintptrARB );
	cr_unpackDispatch.SecondaryColorPointerEXT( size, type, stride, (void *) pointer );
}

void crUnpackExtendVertexAttribPointerARB(void)
{
	GLuint index = READ_DATA( 8, GLuint);
	GLint size = READ_DATA( 12, GLint );
	GLenum type = READ_DATA( 16, GLenum );
	GLboolean normalized = READ_DATA( 20, GLboolean );
	GLsizei stride = READ_DATA( 24, GLsizei );
	GLintptrARB pointer = READ_DATA( 28, GLintptrARB );
	cr_unpackDispatch.VertexAttribPointerARB( index, size, type, normalized, stride, (void *) pointer );
}

void crUnpackExtendVertexAttribPointerNV(void)
{
	GLuint index = READ_DATA( 8, GLuint);
	GLint size = READ_DATA( 12, GLint );
	GLenum type = READ_DATA( 16, GLenum );
	GLsizei stride = READ_DATA( 20, GLsizei );
	GLintptrARB pointer = READ_DATA( 24, GLintptrARB );
	cr_unpackDispatch.VertexAttribPointerNV( index, size, type, stride, (void *) pointer );
}

void crUnpackExtendInterleavedArrays(void)
{
	GLenum format = READ_DATA( 8, GLenum );
	GLsizei stride = READ_DATA( 12, GLsizei );
	GLintptrARB pointer = READ_DATA( 16, GLintptrARB );
	cr_unpackDispatch.InterleavedArrays( format, stride, (void *) pointer );
}

void crUnpackExtendDrawElements(void)
{
	GLenum mode = READ_DATA( 8, GLenum );
	GLsizei count = READ_DATA( 12, GLsizei );
	GLenum type = READ_DATA( 16, GLenum );
	GLintptrARB indices = READ_DATA( 20, GLintptrARB );
	cr_unpackDispatch.DrawElements( mode, count, type, (void *) indices);
}

void crUnpackExtendDrawRangeElements(void)
{
	GLenum mode = READ_DATA( 8, GLenum );
	GLuint start = READ_DATA( 12, GLuint );
	GLuint end = READ_DATA( 16, GLuint );
	GLsizei count = READ_DATA( 20, GLsizei );
	GLenum type = READ_DATA( 24, GLenum );
	GLintptrARB indices = READ_DATA( 28, GLintptrARB );
	cr_unpackDispatch.DrawRangeElements( mode, start, end, count, type, (void *) indices);
}

void crUnpackMultiDrawArraysEXT(void)
{
	crError( "Can't decode MultiDrawArraysEXT" );
}

void crUnpackMultiDrawElementsEXT(void)
{
	crError( "Can't decode MultiDrawElementsEXT" );
}
