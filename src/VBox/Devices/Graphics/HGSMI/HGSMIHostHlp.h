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


#endif /* !__HGSMIHostHlp_h__*/
