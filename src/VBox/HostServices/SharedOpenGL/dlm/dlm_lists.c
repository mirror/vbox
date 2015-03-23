/* $Id$ */
#include <float.h>
#include "cr_dlm.h"
#include "cr_mem.h"
#include "dlm.h"



/* This file defines the display list functions such as NewList, EndList,
 * IsList, DeleteLists, etc.
 * Generally, SPUs will call these as needed to implement display lists.
 * See the expando, replicate, and tilesort SPUs for examples.
 *
 * The functions which compile GL functions into our display lists are named:
 *     void DLM_APIENTRY crdlm_<function name>
 * where <function_name> is the Chromium function name (which in 
 * turn is the GL function name with the "gl" prefix removed).
 *
 * All these entry points require that a CRDLMContextState structure
 * be created (with crDLMNewContext()) and assigned to the current
 * thread (with crDLMSetCurrentState()).
 */


/*
 * Begin compiling a list.
 */
void DLM_APIENTRY
crDLMNewList(GLuint listIdentifier, GLenum mode)
{
    DLMListInfo *listInfo;
    CRDLMContextState *listState = CURRENT_STATE();

    /* Error checks: 0 is not a valid identifier, and
     * we can't call NewList if NewList has been called
     * more recently than EndList.
     *
     * The caller is expected to check for an improper
     * mode parameter (GL_INVALID_ENUM), or for a NewList
     * within a glBegin/glEnd (GL_INVALID_OPERATION).
     */
    if (listState == NULL)
    {
	crWarning("DLM error: NewList(%d,%d) called with no current state (%s line %d)\n",
	    (int) listIdentifier, (int) mode, __FILE__, __LINE__);
	return;
    }

    if (listIdentifier == 0)
    {
	crdlm_error(__LINE__, __FILE__, GL_INVALID_VALUE,
	     "NewList called with a list identifier of 0");
	return;
    }

    if (listState->currentListInfo != NULL)
    {
	char msg[1000];
	sprintf(msg, "NewList called with display list %d while display list %d was already open",
	    (int) listIdentifier, (int) listState->currentListIdentifier);
	crdlm_error(__LINE__, __FILE__, GL_INVALID_OPERATION, msg);
	return;
    }

    listInfo = (DLMListInfo *) crCalloc(sizeof(DLMListInfo));
    if (!(listInfo))
    {
	char msg[1000];
	sprintf(msg, "could not allocate %u bytes of memory in NewList",
	    (unsigned) sizeof(DLMListInfo));
	crdlm_error(__LINE__, __FILE__, GL_OUT_OF_MEMORY, msg);									 
	return;
    }

    listInfo->first = listInfo->last = NULL;
    listInfo->stateFirst = listInfo->stateLast = NULL;
    listInfo->references = crAllocHashtable();
    if (!(listInfo->references))
    {
	crFree(listInfo);
	crdlm_error(__LINE__, __FILE__, GL_OUT_OF_MEMORY,
	    "could not allocate memory in NewList");
	return;
    }
    listInfo->numInstances = 0;
    listInfo->listSent = GL_FALSE;
    listInfo->bbox.xmin = FLT_MAX;
    listInfo->bbox.xmax = -FLT_MAX;
    listInfo->bbox.ymin = FLT_MAX;
    listInfo->bbox.ymax = -FLT_MAX;
    listInfo->bbox.zmin = FLT_MAX;
    listInfo->bbox.zmax = -FLT_MAX;

    listState->currentListInfo = listInfo;
    listState->currentListIdentifier = listIdentifier;
    listState->currentListMode = mode;
}


/* This small utility routine is used to traverse a buffer
 * list, freeing each buffer.  It is used to free the buffer
 * list in the DLMListInfo structure, both when freeing the
 * entire structure and when freeing just the retained content.
 */
static void free_instance_list(DLMInstanceList * instance)
{
	while (instance)
	{
		DLMInstanceList *nextInstance = instance->next;
		crFree(instance);
		instance = nextInstance;
	}
}

/* This utility routine frees a DLMListInfo structure and all
 * of its components.  It is used directly, when deleting a
 * single list; it is also used as a callback function for
 * hash tree operations (Free and Replace).
 *
 * The parameter is listed as a (void *) instead of a (DLMListInfo *)
 * in order that the function can be used as a callback routine for
 * the hash table functions.  The (void *) designation causes no
 * other harm, save disabling type-checking on the pointer argument
 * of the function.
 */
void crdlm_free_list(void *parm)
{
	DLMListInfo *listInfo = (DLMListInfo *) parm;

	free_instance_list(listInfo->first);
	listInfo->first = listInfo->last = NULL;

	/* The references list has no allocated information; it's
	 * just a set of entries.  So we don't need to free any
	 * information as each entry is deleted.
	 */
	crFreeHashtable(listInfo->references, NULL);

	crFree(listInfo);
}


void DLM_APIENTRY crDLMEndList(void)
{
    CRDLMContextState *listState = CURRENT_STATE();

    /* Error check: cannot call EndList without a (successful)
     * preceding NewList.
     *
     * The caller is expected to check for glNewList within
     * a glBegin/glEnd sequence.
     */
    if (listState == NULL)
    {
	crWarning("DLM error: EndList called with no current state (%s line %d)\n",
	    __FILE__, __LINE__);
	return;
    }
    if (listState->currentListInfo == NULL)
    {
	crdlm_error(__LINE__, __FILE__, GL_INVALID_OPERATION,
	    "EndList called while no display list was open");
	return;
    }

    DLM_LOCK(listState->dlm)

    /* This function will either replace the list information that's
     * already present with our new list information, freeing the
     * former list information; or will add the new information
     * to the set of display lists, depending on whether the
     * list already exists or not.
     */
    crHashtableReplace(listState->dlm->displayLists,
	listState->currentListIdentifier,
	listState->currentListInfo, crdlm_free_list);

    DLM_UNLOCK(listState->dlm)

    /* reset the current state to show the list had been ended */
    listState->currentListIdentifier = 0;
    listState->currentListInfo = NULL;
    listState->currentListMode = GL_FALSE;
}


void DLM_APIENTRY crDLMDeleteLists(GLuint firstListIdentifier, GLsizei range)
{
	CRDLMContextState *listState = CURRENT_STATE();
	register int i;

	if (listState == NULL)
	{
		crWarning
			("DLM error: DeleteLists(%d,%d) called with no current state (%s line %d)\n",
			 (int) firstListIdentifier, (int) range, __FILE__, __LINE__);
		return;
	}
	if (range < 0)
	{
		char msg[1000];
		sprintf(msg, "DeleteLists called with range (%d) less than zero", (int) range);
		crdlm_error(__LINE__, __FILE__, GL_INVALID_VALUE, msg);								 
		return;
	}

	/* Interestingly, there doesn't seem to be an error for deleting
	 * display list 0, which cannot exist.
	 *
	 * We could delete the desired lists by walking the entire hash of
	 * display lists and looking for and deleting any in our range; or we
	 * could delete lists one by one.  The former is faster if the hashing
	 * algorithm is inefficient or if we're deleting all or most of our
	 * lists; the latter is faster if we're deleting a relatively small
	 * number of lists.
	 *
	 * For now, we'll go with the latter; it's also easier to implement
	 * given the current functions available.
	 */
	DLM_LOCK(listState->dlm)
	for (i = 0; i < range; i++)
	{
		crHashtableDelete(listState->dlm->displayLists, 
				  firstListIdentifier + i, crdlm_free_list);
	}
	DLM_UNLOCK(listState->dlm)
}

GLboolean DLM_APIENTRY crDLMIsList(GLuint list)
{
	CRDLMContextState *listState = CURRENT_STATE();

	if (listState == NULL)
	{
		crWarning
			("DLM error: IsLists(%d) called with no current state (%s line %d)\n",
			 (int) list, __FILE__, __LINE__);
		return 0;
	}

	if (list == 0)
		return GL_FALSE;

	return crHashtableIsKeyUsed(listState->dlm->displayLists, list);
}

GLuint DLM_APIENTRY crDLMGenLists(GLsizei range)
{
	CRDLMContextState *listState = CURRENT_STATE();

	if (listState == NULL)
	{
		crWarning
			("DLM error: GenLists(%d) called with no current state (%s line %d)\n",
			 (int) range, __FILE__, __LINE__);
		return 0;
	}

	return crHashtableAllocKeys(listState->dlm->displayLists, range);
}

void DLM_APIENTRY crDLMListBase( GLuint base )
{
	CRDLMContextState *listState = CURRENT_STATE();

	if (listState == NULL)
	{
		crWarning
			("DLM error: ListBase(%d) called with no current state (%s line %d)\n",
			 (int) base, __FILE__, __LINE__);
		return;
	}

	listState->listBase = base;
}
