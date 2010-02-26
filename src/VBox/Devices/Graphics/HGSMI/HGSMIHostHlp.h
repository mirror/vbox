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
void hgsmiListRemove (HGSMILIST *pList, HGSMILISTENTRY *pEntry, HGSMILISTENTRY *pPrev);

DECLINLINE(HGSMILISTENTRY*) hgsmiListRemoveHead (HGSMILIST *pList)
{
    HGSMILISTENTRY *pHead = pList->pHead;
    if (pHead)
        hgsmiListRemove (pList, pHead, NULL);
    return pHead;
}

DECLINLINE(void) hgsmiListInit (HGSMILIST *pList)
{
    pList->pHead = NULL;
    pList->pTail = NULL;
}

HGSMILISTENTRY * hgsmiListRemoveAll (HGSMILIST *pList, HGSMILISTENTRY ** ppTail /* optional */);


#endif /* !__HGSMIHostHlp_h__*/
