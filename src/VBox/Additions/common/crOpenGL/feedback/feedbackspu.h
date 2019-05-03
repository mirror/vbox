/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef GA_INCLUDED_SRC_common_crOpenGL_feedback_feedbackspu_h
#define GA_INCLUDED_SRC_common_crOpenGL_feedback_feedbackspu_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef WINDOWS
#define FEEDBACKSPU_APIENTRY __stdcall
#else
#define FEEDBACKSPU_APIENTRY
#endif

#include "cr_spu.h"
#include "cr_glstate.h"

typedef struct context_info_t ContextInfo;

struct context_info_t {
    CRContext *clientState;  /* used to store client-side GL state */
    GLint clientCtx;         /* client context ID */
};

typedef struct {
	int id;
	int has_child;
	SPUDispatchTable self, child, super;

	int render_mode;

	int default_viewport;

	CRCurrentStatePointers current;

    CRContext *defaultctx;
    int numContexts;
    ContextInfo context[CR_MAX_CONTEXTS];

    CRmutex mutex;

    /** The state tracker state. */
    CRStateTracker StateTracker;
} feedbackSPU;

extern feedbackSPU feedback_spu;

extern SPUNamedFunctionTable _cr_feedback_table[];

extern void feedbackspuGatherConfiguration( void );

#endif /* !GA_INCLUDED_SRC_common_crOpenGL_feedback_feedbackspu_h */
