/* $Id$ */
/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include <stdio.h>
#include "cr_spu.h"
#include "cr_dlm.h"
#include "cr_mem.h"
#include "expandospu.h"

extern GLint EXPANDOSPU_APIENTRY
expandoCreateContext(const char *displayName, GLint visBits, GLint shareCtx)
{
	ExpandoContextState *contextState;
	GLint contextId;

	/* Allocate our own per-context record */
	contextState = crCalloc(sizeof(ExpandoContextState));
	if (contextState == NULL) {
	    crError("expando: couldn't allocate per-context state");
	    return 0;
	}

	/* Get an official context ID from our super */
	contextId = expando_spu.super.CreateContext(displayName, visBits, shareCtx);

	/* Supplement that with our DLM.  In a more correct situation, we should
	 * see if we've been called through glXCreateContext, which has a parameter
	 * for sharing DLMs.  We don't currently get that information, so for now
	 * give each context its own DLM.
	 */
	contextState->dlm = crDLMNewDLM(0, NULL);
	if (!contextState->dlm) {
		crError("expando: couldn't get DLM!");
	}

	contextState->dlmContext = crDLMNewContext(contextState->dlm);
	if (!contextState->dlmContext) {
		crError("expando: couldn't get dlmContext");
	}

	/* The DLM needs us to use the state tracker to track client
	 * state, so we can compile client-state-using functions correctly.
	 */
	contextState->State = crStateCreateContext(NULL, visBits, NULL);

	/* Associate the Expando context with the user context. */
	crHashtableAdd(expando_spu.contextTable, contextId, (void *)contextState);

	return contextId;
}

void expando_free_context_state(void *data)
{
    ExpandoContextState *expandoContextState = (ExpandoContextState *)data;

    crDLMFreeContext(expandoContextState->dlmContext);
    crDLMFreeDLM(expandoContextState->dlm);
    crStateDestroyContext(expandoContextState->State);
    crFree(expandoContextState);
}

extern void EXPANDOSPU_APIENTRY
expandoDestroyContext(GLint contextId)
{
	/* Destroy our context information */
	crHashtableDelete(expando_spu.contextTable, contextId, 
				expando_free_context_state);

	/* Pass along the destruction to our super. */
	expando_spu.super.DestroyContext(contextId);
}

extern void EXPANDOSPU_APIENTRY
expandoMakeCurrent(GLint crWindow, GLint nativeWindow, GLint contextId)
{
	ExpandoContextState *expandoContextState;

	expando_spu.super.MakeCurrent(crWindow, nativeWindow, contextId);

	expandoContextState = crHashtableSearch(expando_spu.contextTable, contextId);
	if (expandoContextState) {
	    crDLMSetCurrentState(expandoContextState->dlmContext);
	    crStateMakeCurrent(expandoContextState->State);
	}
	else {
	    crDLMSetCurrentState(NULL);
	    crStateMakeCurrent(NULL);
	}
}

extern void EXPANDOSPU_APIENTRY
expandoNewList(GLuint list, GLenum mode)
{
    crDebug("Expando SPU: expandoNewList()");
	crDLMNewList(list, mode);
}

extern void EXPANDOSPU_APIENTRY
expandoEndList(void)
{
    crDebug("Expando SPU: expandoEndList()");
	crDLMEndList();
}

extern void EXPANDOSPU_APIENTRY
expandoDeleteLists(GLuint first, GLsizei range)
{
	crDLMDeleteLists(first, range);
}

extern GLuint EXPANDOSPU_APIENTRY
expandoGenLists(GLsizei range)
{
	 return crDLMGenLists(range);
}

extern GLboolean EXPANDOSPU_APIENTRY
expandoIsList(GLuint list)
{
	 return crDLMIsList(list);
}

extern void EXPANDOSPU_APIENTRY
expandoCallList(GLuint list)
{
	GLenum mode = crDLMGetCurrentMode();
	if (mode != GL_FALSE) {
		crDLMCompileCallList(list);
		if (mode == GL_COMPILE) return;
	}

	/* Instead of passing through the CallList,
	 * expand it into its components.  This will cause
	 * a recursion if there are any compiled CallList
	 * elements within the display list.
	 */
	crDLMReplayList(list, &expando_spu.self);
}

extern void EXPANDOSPU_APIENTRY
expandoCallLists(GLsizei n, GLenum type, const GLvoid *lists)
{
	GLenum mode = crDLMGetCurrentMode();
	if (mode != GL_FALSE) {
		crDLMCompileCallLists(n, type, lists);
		if (mode == GL_COMPILE) return;
	}
	/* Instead of passing through the CallLists,
	 * expand it into its components.  This will cause
	 * a recursion if there are any compiled CallLists
	 * elements within the display list.
	 */
	crDLMReplayLists(n, type, lists, &expando_spu.self);
}
