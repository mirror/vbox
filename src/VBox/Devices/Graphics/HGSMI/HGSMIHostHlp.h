/** @file
 *
 * VBox Host Guest Shared Memory Interface (HGSMI).
 * Host part helpers.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef __HGSMIHostHlp_h__
#define __HGSMIHostHlp_h__

#include <iprt/assert.h>
#include <iprt/types.h>

typedef struct _HGSMILISTENTRY
{
    struct _HGSMILISTENTRY *pNext;
} HGSMILISTENTRY;

typedef struct _HGSMILIST
{
    HGSMILISTENTRY *pHead;
    HGSMILISTENTRY *pTail;
} HGSMILIST;

void hgsmiListAppend (HGSMILIST *pList, HGSMILISTENTRY *pEntry);
DECLINLINE(void) hgsmiListPrepend (HGSMILIST *pList, HGSMILISTENTRY *pEntry)
{
    HGSMILISTENTRY * pHead = pList->pHead;
    pList->pHead = pEntry;
    pEntry->pNext = pHead;
    if (!pHead)
        pList->pTail = pEntry;
}

void hgsmiListRemove (HGSMILIST *pList, HGSMILISTENTRY *pEntry, HGSMILISTENTRY *pPrev);

DECLINLINE(HGSMILISTENTRY*) hgsmiListRemoveHead (HGSMILIST *pList)
{
    HGSMILISTENTRY *pHead = pList->pHead;
    if (pHead)
        hgsmiListRemove (pList, pHead, NULL);
    return pHead;
}

DECLINLINE(bool) hgsmiListIsEmpty (HGSMILIST *pList)
{
    return !pList->pHead;
}

DECLINLINE(void) hgsmiListInit (HGSMILIST *pList)
{
    pList->pHead = NULL;
    pList->pTail = NULL;
}

HGSMILISTENTRY * hgsmiListRemoveAll (HGSMILIST *pList, HGSMILISTENTRY ** ppTail /* optional */);

DECLINLINE(void) hgsmiListAppendAll (HGSMILIST *pList, HGSMILISTENTRY *pHead, HGSMILISTENTRY *pTail)
{
    if(hgsmiListIsEmpty (pList))
    {
        pList->pHead = pHead;
        pList->pTail = pTail;
    }
    else
    {
        pList->pTail->pNext = pHead;
        pList->pTail = pTail;
    }
}

DECLINLINE(void) hgsmiListPrependAll (HGSMILIST *pList, HGSMILISTENTRY *pHead, HGSMILISTENTRY *pTail)
{
    HGSMILISTENTRY *pOldHead = pList->pHead;
    if(!pOldHead)
    {
        pList->pHead = pHead;
        pList->pTail = pTail;
    }
    else
    {
        pList->pHead = pHead;
        pTail->pNext = pOldHead;
    }
}

DECLINLINE(void) hgsmiListCat (HGSMILIST *pList, HGSMILIST *pList2)
{
    hgsmiListAppendAll (pList, pList2->pHead, pList2->pTail);
    hgsmiListInit (pList2);
}

DECLINLINE(void) hgsmiListPrepCat (HGSMILIST *pList, HGSMILIST *pList2)
{
    hgsmiListPrependAll (pList, pList2->pHead, pList2->pTail);
    hgsmiListInit (pList2);
}


#endif /* !__HGSMIHostHlp_h__*/
