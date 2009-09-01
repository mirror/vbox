/** @file
 *
 * VBox Host Guest Shared Memory Interface (HGSMI).
 * Host part helpers.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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

