/* $Id$ */

/** @file
 * Implementation of all the Display Lists related routines:
 *
 *   glGenLists, glDeleteLists, glNewList, glEndList, glCallList, glCallLists,
 *   glListBase and glIsList.
 *
 * Provide OpenGL IDs mapping between host and guest.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <float.h>
#include "cr_dlm.h"
#include "cr_mem.h"
#include "dlm.h"


/**
 * Destroy each list entry.
 */
static void crdlmFreeDisplayListElements(DLMInstanceList *instance)
{
    while (instance)
    {
        DLMInstanceList *nextInstance = instance->next;
        crFree(instance);
        instance = nextInstance;
    }
}


/**
 * A callback routine used when iterating over all
 * available lists in order to remove them.
 *
 * NOTE: @param pParam2 might be NULL.
 */
void crdlmFreeDisplayListResourcesCb(void *pParm1, void *pParam2)
{
    DLMListInfo      *pListInfo     = (DLMListInfo *)pParm1;
    SPUDispatchTable *dispatchTable = (SPUDispatchTable *)pParam2;

    if (pListInfo)
    {
        crdlmFreeDisplayListElements(pListInfo->first);
        pListInfo->first = pListInfo->last = NULL;

        /* Free host OpenGL resources. */
        if (dispatchTable)
            dispatchTable->DeleteLists(pListInfo->hwid, 1);

        crFree(pListInfo);
    }
}


/**
 * Generate host and guest IDs, setup IDs mapping between host and guest.
 */
GLuint DLM_APIENTRY crDLMGenLists(GLsizei range, SPUDispatchTable *dispatchTable)
{
    CRDLMContextState *listState = CURRENT_STATE();
    GLuint             idHostRangeStart = 0;
    GLuint             idGuestRangeStart = 0;

    crDebug("DLM: GenLists(%d) (DLM=%p).", range, listState ? listState->dlm : 0);

    if (listState)
    {
        idHostRangeStart = dispatchTable->GenLists(range);
        if (idHostRangeStart > 0)
        {
            idGuestRangeStart = crHashtableAllocKeys(listState->dlm->displayLists, range);
            if (idGuestRangeStart > 0)
            {
                GLuint i;
                bool fSuccess = true;

                /* Now have successfully generated IDs range for host and guest. Let's make IDs association. */
                for (i = 0; i < (GLuint)range; i++)
                {
                    DLMListInfo *pListInfo;

                    pListInfo = (DLMListInfo *)crCalloc(sizeof(DLMListInfo));
                    if (pListInfo)
                    {
                        crMemset(pListInfo, 0, sizeof(DLMListInfo));
                        pListInfo->hwid = idHostRangeStart + i;

                        /* Insert pre-initialized list data which contains IDs mapping into the hash. */
                        crHashtableReplace(listState->dlm->displayLists, idGuestRangeStart + i, pListInfo, NULL);
                    }
                    else
                    {
                        fSuccess = false;
                        break;
                    }
                }

                /* All structures allocated and initialized successfully. */
                if (fSuccess)
                    return idGuestRangeStart;

                /* Rollback some data was not allocated. */
                crDLMDeleteLists(idGuestRangeStart, range, NULL /* we do DeleteLists() later in this routine */ );
            }
            else
                crDebug("DLM: Can't allocate Display List IDs range for the guest.");

            dispatchTable->DeleteLists(idHostRangeStart, range);
        }
        else
            crDebug("DLM: Can't allocate Display List IDs range on the host side.");
    }
    else
        crDebug("DLM: GenLists(%u) called with no current state.", range);

    /* Can't reserve IDs range.  */
    return 0;
}


/**
 * Release host and guest IDs, free memory resources.
 */
void DLM_APIENTRY crDLMDeleteLists(GLuint list, GLsizei range, SPUDispatchTable *dispatchTable)
{
    CRDLMContextState *listState = CURRENT_STATE();

    crDebug("DLM: DeleteLists(%u, %d) (DLM=%p).", list, range, listState ? listState->dlm : 0);

    if (listState)
    {
        if (range >= 0)
        {
            int i;

            /* Free resources: host memory, host IDs and guest IDs. */
            DLM_LOCK(listState->dlm)
            for (i = 0; i < range; i++)
                crHashtableDeleteEx(listState->dlm->displayLists, list + i, crdlmFreeDisplayListResourcesCb, dispatchTable);
            DLM_UNLOCK(listState->dlm)
        }
        else
            crDebug("DLM: DeleteLists(%u, %d) not allowed.", list, range);
    }
    else
        crDebug("DLM: DeleteLists(%u, %d) called with no current state.", list, range);
}


/**
 * Start recording a list.
 */
void DLM_APIENTRY
crDLMNewList(GLuint list, GLenum mode, SPUDispatchTable *dispatchTable)
{
    DLMListInfo       *listInfo;
    CRDLMContextState *listState = CURRENT_STATE();

    crDebug("DLM: NewList(%u, %u) (DLM=%p).", list, mode, listState ? listState->dlm : 0);

    if (listState)
    {
        /* Valid list ID should be > 0. */
        if (list > 0)
        {
            if (listState->currentListInfo == NULL)
            {
                listInfo = (DLMListInfo *)crHashtableSearch(listState->dlm->displayLists, list);
                if (listInfo)
                {
                    listInfo->first = listInfo->last = NULL;
                    listInfo->stateFirst = listInfo->stateLast = NULL;

                    listInfo->numInstances = 0;

                    listState->currentListInfo = listInfo;
                    listState->currentListIdentifier = list;
                    listState->currentListMode = mode;

                    dispatchTable->NewList(listInfo->hwid, mode);

                    crDebug("DLM: create new list with [guest, host] ID pair [%u, %u].", list, listInfo->hwid);

                    return;
                }
                else
                    crDebug("DLM: Requested Display List %u was not previously reserved with glGenLists().", list);
            }
            else
                crDebug("DLM: NewList called with display list %u while display list %u was already open.", list, listState->currentListIdentifier);
        }
        else
            crDebug("DLM: NewList called with a list identifier of 0.");
    }
    else
        crDebug("DLM: NewList(%u, %u) called with no current state.\n", list, mode);
}


/**
 * Stop recording a list.
 */
void DLM_APIENTRY crDLMEndList(SPUDispatchTable *dispatchTable)
{
    CRDLMContextState *listState = CURRENT_STATE();

    crDebug("DLM: EndList() (DLM=%p).", listState ? listState->dlm : 0);

    if (listState)
    {
        /* Check if list was ever started. */
        if (listState->currentListInfo)
        {
            /* reset the current state to show the list had been ended */
            listState->currentListIdentifier = 0;
            listState->currentListInfo = NULL;
            listState->currentListMode = GL_FALSE;

            dispatchTable->EndList();
        }
        else
            crDebug("DLM: glEndList() is assuming glNewList() was issued previously.");
    } 
    else
        crDebug("DLM: EndList called with no current state.");
}


/**
 * Execute list on hardware and cach ethis call if we currently recording a list.
 */
void DLM_APIENTRY crDLMCallList(GLuint list, SPUDispatchTable *dispatchTable)
{
    CRDLMContextState *listState = CURRENT_STATE();

    //crDebug("DLM: CallList(%u).", list);

    if (listState)
    {
        DLMListInfo *listInfo;

        /* Add to calls cache if we recording a list. */
        if (listState->currentListInfo)
            crDLMCompileCallList(list);

        /* Find hwid for list. */
        listInfo = (DLMListInfo *)crHashtableSearch(listState->dlm->displayLists, list);
        if (listInfo)
            dispatchTable->CallList(listInfo->hwid);
        else
            crDebug("DLM: CallList(%u) issued for non-existent list.", list);
    }
    else
        crDebug("DLM: CallList(%u) called with no current state.", list);
}


/* Helper for crDLMCallLists().
 * We need to extract list ID by index from array of given type and cast it to GLuint.
 * Please replece it w/ something more elegant if better solution will be found!
 */
inline GLuint crDLMGetListByIndex(const GLvoid *aValues, GLsizei index, GLenum type)
{
    GLuint element = 0;

    switch (type)
    {
#define CRDLM_LIST_BY_INDEX_HANDLE_TYPE(_type_name, _size_of_type) \
        case _type_name: \
        { \
            crMemcpy((void *)&element, (void *)((void *)(aValues) + (index * (_size_of_type))) \
            , (_size_of_type)); \
            break; \
        }

        CRDLM_LIST_BY_INDEX_HANDLE_TYPE(GL_BYTE,            sizeof(GLbyte));
        CRDLM_LIST_BY_INDEX_HANDLE_TYPE(GL_UNSIGNED_BYTE,   sizeof(GLubyte));
        CRDLM_LIST_BY_INDEX_HANDLE_TYPE(GL_SHORT,           sizeof(GLshort));
        CRDLM_LIST_BY_INDEX_HANDLE_TYPE(GL_UNSIGNED_SHORT,  sizeof(GLushort));
        CRDLM_LIST_BY_INDEX_HANDLE_TYPE(GL_INT,             sizeof(GLint));
        CRDLM_LIST_BY_INDEX_HANDLE_TYPE(GL_UNSIGNED_INT,    sizeof(GLuint));
        CRDLM_LIST_BY_INDEX_HANDLE_TYPE(GL_FLOAT,           sizeof(GLfloat));
        CRDLM_LIST_BY_INDEX_HANDLE_TYPE(GL_2_BYTES,         2);
        CRDLM_LIST_BY_INDEX_HANDLE_TYPE(GL_3_BYTES,         3);
        CRDLM_LIST_BY_INDEX_HANDLE_TYPE(GL_4_BYTES,         4);

        default:
            crError("DLM: attempt to pass to crDLMCallLists() unknown type: %u.", index);

#undef CRDLM_LIST_BY_INDEX_HANDLE_TYPE
    }

    return element;
}

/**
 * Execute lists on hardware and cach ethis call if we currently recording a list.
 */
void DLM_APIENTRY crDLMCallLists(GLsizei n, GLenum type, const GLvoid *lists, SPUDispatchTable *dispatchTable)
{
    CRDLMContextState *listState = CURRENT_STATE();

    crDebug("DLM: CallLists(%d, %u, %p).", n, type, lists);

    if (listState)
    {
        GLsizei i;

        /* Add to calls cache if we recording a list. */
        if (listState->currentListInfo)
            crDLMCompileCallLists(n, type, lists);

        /* This is sad, but we need to translate guest IDs into host ones.
         * Since spec does not promise that @param lists conain contiguous set of IDs,
         * the only way to do that is to iterate over each guest ID and perform translation.
         * This might have negative performance impact. */
        for (i = 0; i < n; i++)
        {
            DLMListInfo *listInfo;
            GLuint guest_id = crDLMGetListByIndex(lists, n, type);

            if (guest_id > 0)
            {
                listInfo = (DLMListInfo *)crHashtableSearch(listState->dlm->displayLists, guest_id);
                if (listInfo)
                    dispatchTable->CallList(listInfo->hwid);
                else
                    crDebug("DLM: CallLists(%d, %u, %p) was unabbe to resolve host ID for guest ID %u.", n, type, lists, guest_id);
            }
            else
                crDebug("DLM: CallLists(%d, %u, %p) received bad array of IDs.", n, type, lists);
        }
    }
    else
        crDebug("DLM: CallLists(%d, %u, %p) called with no current state.", n, type, lists);
}


/**
 * Set list base, remember its value and add call to the cache.
 */
void DLM_APIENTRY crDLMListBase(GLuint base, SPUDispatchTable *dispatchTable)
{
    CRDLMContextState *listState = CURRENT_STATE();

    crDebug("DLM: ListBase(%u).", base);

    if (listState)
    {
        listState->listBase = base;
        crDLMCompileListBase(base);
        dispatchTable->ListBase(base);
    }
    else
        crDebug("DLM: ListBase(%u) called with no current state.", base);
}


/**
 * Check if specified list ID belongs to valid Display List.
 * Positive result is only returned in case both conditions below are satisfied:
 *
 *   - given list found in DLM hash table (i.e., it was previously allocated
 *     with crDLMGenLists and still not released with crDLMDeleteLists);
 *
 *   - list is valid on the host side.
 */
GLboolean DLM_APIENTRY crDLMIsList(GLuint list, SPUDispatchTable *dispatchTable)
{
    CRDLMContextState *listState = CURRENT_STATE();

    crDebug("DLM: IsList(%u).", list);

    if (listState)
    {
        if (list > 0)
        {
            DLMListInfo *listInfo = (DLMListInfo *)crHashtableSearch(listState->dlm->displayLists, list);
            if (listInfo)
            {
                if (dispatchTable->IsList(listInfo->hwid))
                    return true;
                else
                    crDebug("DLM: list [%u, %u] not found on the host side.", list, listInfo->hwid);
            }
            else
                crDebug("DLM: list %u not found in guest cache.", list);
        }
        else
            crDebug("DLM: IsList(%u) is not allowed.", list);
    }
    else
        crDebug("DLM: IsList(%u) called with no current state.", list);

    return false;
}
