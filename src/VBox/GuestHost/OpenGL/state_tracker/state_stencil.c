/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include <stdio.h>
#include "state.h"
#include "state/cr_statetypes.h"
#include "state_internals.h"

void crStateStencilInit(CRContext *ctx)
{
	CRStencilState *s = &ctx->stencil;
	CRStateBits *stateb = GetCurrentBits();
	CRStencilBits *sb = &(stateb->stencil);

	s->stencilTest = GL_FALSE;
	RESET(sb->enable, ctx->bitid);

	s->func = GL_ALWAYS;
	s->mask = 0xFFFFFFFF;
	s->ref = 0;
	RESET(sb->func, ctx->bitid);

	s->fail = GL_KEEP;
	s->passDepthFail = GL_KEEP;
	s->passDepthPass = GL_KEEP;
	RESET(sb->op, ctx->bitid);

	s->clearValue = 0;
	RESET(sb->clearValue, ctx->bitid);

	s->writeMask = 0xFFFFFFFF;
	RESET(sb->writeMask, ctx->bitid);

	RESET(sb->dirty, ctx->bitid);
}

void STATE_APIENTRY crStateStencilFunc(GLenum func, GLint ref, GLuint mask) 
{
	CRContext *g = GetCurrentContext();
	CRStencilState *s = &(g->stencil);
	CRStateBits *stateb = GetCurrentBits();
	CRStencilBits *sb = &(stateb->stencil);


	if (g->current.inBeginEnd)
	{
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, 
			"glStencilFunc called in begin/end");
		return;
	}

	FLUSH();

	if (func != GL_NEVER &&
		func != GL_LESS &&
		func != GL_LEQUAL &&
		func != GL_GREATER &&
		func != GL_GEQUAL &&
		func != GL_EQUAL &&
		func != GL_NOTEQUAL &&
		func != GL_ALWAYS)
	{
		crStateError(__LINE__, __FILE__, GL_INVALID_ENUM, 
			"glStencilFunc called with bogu func: %d", func);
		return;
	}

	s->func = func;
	s->ref = ref;
	s->mask = mask;

	DIRTY(sb->func, g->neg_bitid);
	DIRTY(sb->dirty, g->neg_bitid);
}

void STATE_APIENTRY crStateStencilOp (GLenum fail, GLenum zfail, GLenum zpass) 
{
	CRContext *g = GetCurrentContext();
	CRStencilState *s = &(g->stencil);
	CRStateBits *stateb = GetCurrentBits();
	CRStencilBits *sb = &(stateb->stencil);

	if (g->current.inBeginEnd)
	{
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, 
			"glStencilOp called in begin/end");
		return;
	}

	FLUSH();

	switch (fail) {
	case GL_KEEP:
	case GL_ZERO:
	case GL_REPLACE:
	case GL_INCR:
	case GL_DECR:
	case GL_INVERT:
#ifdef CR_EXT_stencil_wrap
	case GL_INCR_WRAP_EXT:
	case GL_DECR_WRAP_EXT:
#endif
		break;
	default:
		crStateError(__LINE__, __FILE__, GL_INVALID_ENUM, 
			"glStencilOp called with bogus fail: %d", fail);
		return;
	}

	switch (zfail) {
	case GL_KEEP:
	case GL_ZERO:
	case GL_REPLACE:
	case GL_INCR:
	case GL_DECR:
	case GL_INVERT:
#ifdef CR_EXT_stencil_wrap
	case GL_INCR_WRAP_EXT:
	case GL_DECR_WRAP_EXT:
#endif
		break;
	default:
		crStateError(__LINE__, __FILE__, GL_INVALID_ENUM, 
			"glStencilOp called with bogus zfail: %d", zfail);
		return;
	}

	switch (zpass) {
	case GL_KEEP:
	case GL_ZERO:
	case GL_REPLACE:
	case GL_INCR:
	case GL_DECR:
	case GL_INVERT:
#ifdef CR_EXT_stencil_wrap
	case GL_INCR_WRAP_EXT:
	case GL_DECR_WRAP_EXT:
#endif
		break;
	default:
		crStateError(__LINE__, __FILE__, GL_INVALID_ENUM, 
			"glStencilOp called with bogus zpass: %d", zpass);
		return;
	}

	s->fail = fail;
	s->passDepthFail = zfail;
	s->passDepthPass = zpass;

	DIRTY(sb->op, g->neg_bitid);
	DIRTY(sb->dirty, g->neg_bitid);
}


void STATE_APIENTRY crStateClearStencil (GLint c) 
{
	CRContext *g = GetCurrentContext();
	CRStencilState *s = &(g->stencil);
	CRStateBits *stateb = GetCurrentBits();
	CRStencilBits *sb = &(stateb->stencil);

	if (g->current.inBeginEnd)
	{
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, 
			"glClearStencil called in begin/end");
		return;
	}

	FLUSH();


	s->clearValue = c;
	
	DIRTY(sb->clearValue, g->neg_bitid);
	DIRTY(sb->dirty, g->neg_bitid);
}

void STATE_APIENTRY crStateStencilMask (GLuint mask) 
{
	CRContext *g = GetCurrentContext();
	CRStencilState *s = &(g->stencil);
	CRStateBits *stateb = GetCurrentBits();
	CRStencilBits *sb = &(stateb->stencil);

	if (g->current.inBeginEnd)
	{
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, 
			"glStencilMask called in begin/end");
		return;
	}

	FLUSH();

	s->writeMask = mask;

	DIRTY(sb->writeMask, g->neg_bitid);
	DIRTY(sb->dirty, g->neg_bitid);
}
