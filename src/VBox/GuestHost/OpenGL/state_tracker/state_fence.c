/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "state.h"
#include "state_internals.h"
#include "state/cr_statetypes.h"



void APIENTRY crStateGenFencesNV(GLsizei n, GLuint *fences)
{
}


void APIENTRY crStateDeleteFencesNV(GLsizei n, const GLuint *fences)
{
}

void APIENTRY crStateSetFenceNV(GLuint fence, GLenum condition)
{
}

GLboolean APIENTRY crStateTestFenceNV(GLuint fence)
{
   return GL_FALSE;
}

void APIENTRY crStateFinishFenceNV(GLuint fence)
{
}

GLboolean APIENTRY crStateIsFenceNV(GLuint fence)
{
   return GL_FALSE;
}

void APIENTRY crStateGetFenceivNV(GLuint fence, GLenum pname, GLint *params)
{
}


