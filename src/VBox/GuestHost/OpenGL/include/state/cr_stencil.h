/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef CR_STATE_STENCIL_H
#define CR_STATE_STENCIL_H

#include "cr_glstate.h"
#include "state/cr_statetypes.h"

#include <iprt/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	CRbitvalue dirty[CR_MAX_BITARRAY];
	CRbitvalue enable[CR_MAX_BITARRAY];
	CRbitvalue func[CR_MAX_BITARRAY];
	CRbitvalue op[CR_MAX_BITARRAY];
	CRbitvalue clearValue[CR_MAX_BITARRAY];
	CRbitvalue writeMask[CR_MAX_BITARRAY];
} CRStencilBits;

typedef struct {
	GLboolean	stencilTest;
	GLenum		func;
	GLint		mask;
	GLint		ref;
	GLenum		fail;
	GLenum		passDepthFail;
	GLenum		passDepthPass;
	GLint		clearValue;
	GLint		writeMask;
} CRStencilState;

DECLEXPORT(void) crStateStencilInit(CRContext *ctx);

DECLEXPORT(void) crStateStencilDiff(CRStencilBits *bb, CRbitvalue *bitID,
                                    CRContext *fromCtx, CRContext *toCtx);
DECLEXPORT(void) crStateStencilSwitch(CRStencilBits *bb, CRbitvalue *bitID, 
                                      CRContext *fromCtx, CRContext *toCtx);

#ifdef __cplusplus
}
#endif

#endif /* CR_STATE_STENCIL_H */
