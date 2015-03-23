/* $Id$ */
#include <stdio.h>
#include "cr_spu.h"
#include "cr_dlm.h"
#include "cr_mem.h"
#include "cr_error.h"
#include "dlm.h"
#include <VBox/VBoxUhgsmi.h>
/* The headers and structures are still auto-generated */
#include "dlm_generated.h"

/* The CallList functions have a special implementation.  They aren't commonly
 * listed as state-changers, but they can cause state to change.
 */

static void DLM_APIENTRY executeCallList(DLMInstanceList *x, SPUDispatchTable *dispatchTable)
{
	struct instanceCallList *instance = (struct instanceCallList *)x;
	dispatchTable->CallList(instance->list);
}
void DLM_APIENTRY crDLMCompileCallList( GLuint list )
{
	struct instanceCallList *instance;
	instance = crCalloc(sizeof(struct instanceCallList));
	if (!instance) {
		crdlm_error(__LINE__, __FILE__, GL_OUT_OF_MEMORY,
			"out of memory adding CallList to display list");
		return;
	}
	/* Put in the parameters */
	instance->list = list;

	/* Add to the display list correctly */
	crdlm_add_to_list((DLMInstanceList *)instance, executeCallList);
}

/*** CallLists ***/
static void DLM_APIENTRY executeCallLists(DLMInstanceList *x, SPUDispatchTable *dispatchTable)
{
	struct instanceCallLists *instance = (struct instanceCallLists *)x;
	dispatchTable->CallLists(instance->n, instance->type, instance->lists);
}
void DLM_APIENTRY crDLMCompileCallLists( GLsizei n, GLenum type, const GLvoid * lists )
{
	struct instanceCallLists *instance;
	instance = crCalloc(sizeof(struct instanceCallLists) + crdlm_pointers_CallLists(NULL, n, type, lists));
	if (!instance) {
		crdlm_error(__LINE__, __FILE__, GL_OUT_OF_MEMORY,
			"out of memory adding CallLists to display list");
		return;
	}
	instance->n = n;
	instance->type = type;
	if (lists == NULL) {
		instance->lists = NULL;
	}
	else {
		instance->lists = instance->listsData;
	}
	(void) crdlm_pointers_CallLists(instance, n, type, lists);

	crdlm_add_to_list((DLMInstanceList *)instance, executeCallLists);
}
