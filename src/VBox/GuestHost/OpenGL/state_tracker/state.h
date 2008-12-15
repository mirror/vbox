/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef STATE_H
#define STATE_H

#include "cr_glstate.h"
#ifdef CHROMIUM_THREADSAFE
#include "cr_threads.h"
#endif

extern SPUDispatchTable diff_api;
extern CRStateBits *__currentBits;

#define GetCurrentBits() __currentBits

#ifdef CHROMIUM_THREADSAFE
extern CRtsd __contextTSD;
#define GetCurrentContext() (CRContext *) crGetTSD(&__contextTSD)
#else
extern CRContext *__currentContext;
#define GetCurrentContext() __currentContext
#endif

extern void crStateTextureInitTextureObj (CRContext *ctx, CRTextureObj *tobj, GLuint name, GLenum target);
extern void crStateTextureInitTextureFormat( CRTextureLevel *tl, GLenum internalFormat );

/* Normally these functions would have been in cr_bufferobject.h but
 * that led to a number of issues.
 */
void crStateBufferObjectInit(CRContext *ctx);

void crStateBufferObjectDestroy (CRContext *ctx);

void crStateBufferObjectDiff(CRBufferObjectBits *bb, CRbitvalue *bitID,
														 CRContext *fromCtx, CRContext *toCtx);

void crStateBufferObjectSwitch(CRBufferObjectBits *bb, CRbitvalue *bitID, 
															 CRContext *fromCtx, CRContext *toCtx);


/* These would normally be in cr_client.h */

void crStateClientDiff(CRClientBits *cb, CRbitvalue *bitID, CRContext *from, CRContext *to);
											 
void crStateClientSwitch(CRClientBits *cb, CRbitvalue *bitID,	CRContext *from, CRContext *to);


void
crStateGetTextureObjectAndImage(CRContext *g, GLenum texTarget, GLint level,
																CRTextureObj **obj, CRTextureLevel **img);

#endif
