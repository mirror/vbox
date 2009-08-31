/** @file
 *
 * VBox Host Guest Shared Memory Interface (HGSMI).
 * Host part helpers.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */


#include "HGSMIHostHlp.h"


void hgsmiListAppend (HGSMILIST *pList, HGSMILISTENTRY *pEntry)
{
    AssertPtr(pEntry);
    Assert(pEntry->pNext == NULL);

    if (pList->pTail)
    {
        Assert (pList->pTail->pNext == NULL);
        pList->pTail->pNext = pEntry;
    }
    else
    {
        Assert (pList->pHead == NULL);
        pList->pHead = pEntry;
    }

    pList->pTail = pEntry;
}


void hgsmiListRemove (HGSMILIST *pList, HGSMILISTENTRY *pEntry, HGSMILISTENTRY *pPrev)
{
    AssertPtr(pEntry);

    if (pEntry->pNext == NULL)
    {
        Assert (pList->pTail == pEntry);
        pList->pTail = pPrev;
    }
    else
    {
        /* Do nothing. The *pTail is not changed. */
    }

    if (pPrev == NULL)
    {
        Assert (pList->pHead == pEntry);
        pList->pHead = pEntry->pNext;
    }
    else
    {
        pPrev->pNext = pEntry->pNext;
    }

    pEntry->pNext = NULL;
}

