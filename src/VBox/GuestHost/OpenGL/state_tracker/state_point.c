/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "state.h"
#include "state/cr_statetypes.h"
#include "state_internals.h"

void crStatePointInit (CRContext *ctx)
{
	CRPointState *p = &ctx->point;
	CRStateBits *sb = GetCurrentBits();
	CRPointBits *pb = &(sb->point);
	int i;

	p->pointSmooth = GL_FALSE;
	RESET(pb->enableSmooth, ctx->bitid);
	p->pointSize = 1.0f;
	RESET(pb->size, ctx->bitid);
#ifdef CR_ARB_point_parameters
	p->minSize = 0.0f;
	RESET(pb->minSize, ctx->bitid);
	p->maxSize = CR_ALIASED_POINT_SIZE_MAX;
	RESET(pb->maxSize, ctx->bitid);
	p->fadeThresholdSize = 1.0f;
	RESET(pb->fadeThresholdSize, ctx->bitid);
	p->distanceAttenuation[0] = 1.0f;
	p->distanceAttenuation[1] = 0.0f;
	p->distanceAttenuation[2] = 0.0f;
	RESET(pb->distanceAttenuation, ctx->bitid);
#endif
#ifdef CR_ARB_point_sprite
	p->pointSprite = GL_FALSE;
	RESET(pb->enableSprite, ctx->bitid);
	for (i = 0; i < CR_MAX_TEXTURE_UNITS; i++) {
		p->coordReplacement[i] = GL_FALSE;
		RESET(pb->coordReplacement[i], ctx->bitid);
	}
#endif

	RESET(pb->dirty, ctx->bitid);

	/*
	 *p->aliasedpointsizerange_min = c->aliasedpointsizerange_min; 
	 *p->aliasedpointsizerange_max = c->aliasedpointsizerange_max; 
	 *p->aliasedpointsizegranularity = c->aliasedpointsizegranularity; 
	 *p->smoothpointsizerange_min = c->smoothpointsizerange_min; 
	 *p->smoothpointsizerange_max = c->smoothpointsizerange_max; 
	 *p->smoothpointgranularity = c->smoothpointgranularity;
	 */
}

void STATE_APIENTRY crStatePointSize(GLfloat size) 
{
	CRContext *g = GetCurrentContext();
	CRPointState *p = &(g->point);
	CRStateBits *sb = GetCurrentBits();
	CRPointBits *pb = &(sb->point);

	if (g->current.inBeginEnd)
	{
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, "glPointSize called in begin/end");
		return;
	}

	FLUSH();

	if (size <= 0.0f) 
	{
		crStateError(__LINE__, __FILE__, GL_INVALID_VALUE, "glPointSize called with size <= 0.0: %f", size);
		return;
	}
		
	p->pointSize = size;
	DIRTY(pb->size, g->neg_bitid);
	DIRTY(pb->dirty, g->neg_bitid);
}

void STATE_APIENTRY crStatePointParameterfARB(GLenum pname, GLfloat param)
{
	CRContext *g = GetCurrentContext();

	if (g->current.inBeginEnd)
	{
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, "glPointParameterfARB called in begin/end");
		return;
	}

	FLUSH();

	crStatePointParameterfvARB(pname, &param);
}

void STATE_APIENTRY crStatePointParameterfvARB(GLenum pname, const GLfloat *params)
{
	CRContext *g = GetCurrentContext();
	CRPointState *p = &(g->point);
	CRStateBits *sb = GetCurrentBits();
	CRPointBits *pb = &(sb->point);

	if (g->current.inBeginEnd)
	{
		crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, "glPointParameterfvARB called in begin/end");
		return;
	}

	FLUSH();

	switch (pname) {
	case GL_DISTANCE_ATTENUATION_EXT:
		if (g->extensions.ARB_point_parameters) {
			p->distanceAttenuation[0] = params[0];
			p->distanceAttenuation[1] = params[1];
			p->distanceAttenuation[2] = params[2];
			DIRTY(pb->distanceAttenuation, g->neg_bitid);
		}
		else 
		{
			crStateError(__LINE__, __FILE__, GL_INVALID_ENUM, "glPointParameterfvARB invalid enum: %f", pname);
			return;
		}
		break;
	case GL_POINT_SIZE_MIN_EXT:
		if (g->extensions.ARB_point_parameters) {
			if (params[0] < 0.0F) {
				crStateError(__LINE__, __FILE__, GL_INVALID_VALUE, "glPointParameterfvARB invalid value: %f", params[0]);
				return;
			}
            		p->minSize = params[0];
			DIRTY(pb->minSize, g->neg_bitid);
         	}
		else
		{
			crStateError(__LINE__, __FILE__, GL_INVALID_ENUM, "glPointParameterfvARB invalid enum: %f", pname);
			return;
		}
		break;
	case GL_POINT_SIZE_MAX_EXT:
		if (g->extensions.ARB_point_parameters) {
			if (params[0] < 0.0F) {
				crStateError(__LINE__, __FILE__, GL_INVALID_VALUE, "glPointParameterfvARB invalid value: %f", params[0]);
				return;
			}
            		p->maxSize = params[0];
			DIRTY(pb->maxSize, g->neg_bitid);
         	}
		else
		{
			crStateError(__LINE__, __FILE__, GL_INVALID_ENUM, "glPointParameterfvARB invalid enum: %f", pname);
			return;
		}
		break;
	case GL_POINT_FADE_THRESHOLD_SIZE_EXT:
		if (g->extensions.ARB_point_parameters) {
			if (params[0] < 0.0F) {
				crStateError(__LINE__, __FILE__, GL_INVALID_VALUE, "glPointParameterfvARB invalid value: %f", params[0]);
				return;
			}
            		p->fadeThresholdSize = params[0];
			DIRTY(pb->fadeThresholdSize, g->neg_bitid);
         	}
		else
		{
			crStateError(__LINE__, __FILE__, GL_INVALID_ENUM, "glPointParameterfvARB invalid enum: %f", pname);
			return;
		}
		break;
	default:
		crStateError(__LINE__, __FILE__, GL_INVALID_ENUM, "glPointParameterfvARB invalid enum: %f", pname);
		return;
	}

	DIRTY(pb->dirty, g->neg_bitid);
}

void STATE_APIENTRY crStatePointParameteri(GLenum pname, GLint param)
{
	GLfloat f_param = (GLfloat) param;
	crStatePointParameterfvARB( pname, &f_param );
}

void STATE_APIENTRY crStatePointParameteriv(GLenum pname, const GLint *params)
{
	GLfloat f_param = (GLfloat) (*params);
	crStatePointParameterfvARB( pname, &f_param );
}
