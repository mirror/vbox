/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

/*
 * Lifetime-based fast allocation, inspired by much prior art, including
 * "Fast Allocation and Deallocation of Memory Based on Object Lifetimes"
 * David R. Hanson, Software -- Practice and Experience, Vol. 20(1).
 */
#include <stdlib.h>
#include <string.h>
#include "plarena.h"
#include "prbit.h"

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/semaphore.h>

static PLArena *arena_freelist;

#define PL_ARENA_DEFAULT_ALIGN  sizeof(double)

static RTSEMFASTMUTEX g_hMtxArena = NIL_RTSEMFASTMUTEX;
static RTONCE         g_rtOnce    = RTONCE_INITIALIZER;

/*
** InitializeArenas() -- Initialize arena operations.
**
** InitializeArenas() is called exactly once and only once from 
** LockArena(). This function creates the arena protection 
** lock: arenaLock.
**
** Note: If the arenaLock cannot be created, InitializeArenas()
** fails quietly, returning only PR_FAILURE. This percolates up
** to the application using the Arena API. He gets no arena
** from PL_ArenaAllocate(). It's up to him to fail gracefully
** or recover.
**
*/
static DECLCALLBACK(int) InitializeArenas(void *pvUser)
{
    RT_NOREF(pvUser);
    Assert(g_hMtxArena == NIL_RTSEMFASTMUTEX);

    return RTSemFastMutexCreate(&g_hMtxArena);
} /* end ArenaInitialize() */

static PRStatus LockArena( void )
{
    int vrc = RTOnce(&g_rtOnce, InitializeArenas, NULL /*pvUser*/);
    if (RT_FAILURE(vrc))
        return PR_FAILURE;

    RTSemFastMutexRequest(g_hMtxArena);
    return PR_SUCCESS;
} /* end LockArena() */

static void UnlockArena( void )
{
    RTSemFastMutexRelease(g_hMtxArena);
    return;
} /* end UnlockArena() */

PR_IMPLEMENT(void) PL_InitArenaPool(
    PLArenaPool *pool, const char *name, PRUint32 size, PRUint32 align)
{
    if (align == 0)
        align = PL_ARENA_DEFAULT_ALIGN;
    pool->mask = PR_BITMASK(PR_CeilingLog2(align));
    pool->first.next = NULL;
    /* Set all three addresses in pool->first to the same dummy value.
     * These addresses are only compared with each other, but never
     * dereferenced. */
    pool->first.base = pool->first.avail = pool->first.limit =
        (PRUword)PL_ARENA_ALIGN(pool, &pool->first + 1);
    pool->current = &pool->first;
    pool->arenasize = size;
}


/*
** PL_ArenaAllocate() -- allocate space from an arena pool
** 
** Description: PL_ArenaAllocate() allocates space from an arena
** pool. 
**
** First, try to satisfy the request from arenas starting at
** pool->current.
**
** If there is not enough space in the arena pool->current, try
** to claim an arena, on a first fit basis, from the global
** freelist (arena_freelist).
** 
** If no arena in arena_freelist is suitable, then try to
** allocate a new arena from the heap.
**
** Returns: pointer to allocated space or NULL
** 
** Notes: The original implementation had some difficult to
** solve bugs; the code was difficult to read. Sometimes it's
** just easier to rewrite it. I did that. larryh.
**
** See also: bugzilla: 45343.
**
*/

PR_IMPLEMENT(void *) PL_ArenaAllocate(PLArenaPool *pool, PRUint32 nb)
{
    PLArena *a;   
    char *rp;     /* returned pointer */
    PRUint32 nbOld;

    Assert((nb & pool->mask) == 0);

    nbOld = nb;
    nb = (PRUword)PL_ARENA_ALIGN(pool, nb); /* force alignment */
    if (nb < nbOld)
        return NULL;

    /* attempt to allocate from arenas at pool->current */
    {
        a = pool->current;
        do {
            if ( a->avail +nb <= a->limit )  {
                pool->current = a;
                rp = (char *)a->avail;
                a->avail += nb;
                return rp;
            }
        } while( NULL != (a = a->next) );
    }

    /* attempt to allocate from arena_freelist */
    {
        PLArena *p; /* previous pointer, for unlinking from freelist */

        /* lock the arena_freelist. Make access to the freelist MT-Safe */
        if ( PR_FAILURE == LockArena())
            return(0);

        for ( a = p = arena_freelist; a != NULL ; p = a, a = a->next ) {
            if ( a->base +nb <= a->limit )  {
                if ( p == arena_freelist )
                    arena_freelist = a->next;
                else
                    p->next = a->next;
                UnlockArena();
                a->avail = a->base;
                rp = (char *)a->avail;
                a->avail += nb;
                /* the newly allocated arena is linked after pool->current 
                *  and becomes pool->current */
                a->next = pool->current->next;
                pool->current->next = a;
                pool->current = a;
                if ( NULL == pool->first.next )
                    pool->first.next = a;
                return(rp);
            }
        }
        UnlockArena();
    }

    /* attempt to allocate from the heap */ 
    {  
        PRUint32 sz = PR_MAX(pool->arenasize, nb);
        sz += sizeof *a + pool->mask;  /* header and alignment slop */
        a = (PLArena*)RTMemAlloc(sz);
        if ( NULL != a )  {
            a->limit = (PRUword)a + sz;
            a->base = a->avail = (PRUword)PL_ARENA_ALIGN(pool, a + 1);
            rp = (char *)a->avail;
            a->avail += nb;
            Assert(a->avail <= a->limit);
            /* the newly allocated arena is linked after pool->current 
            *  and becomes pool->current */
            a->next = pool->current->next;
            pool->current->next = a;
            pool->current = a;
            if ( NULL == pool->first.next )
                pool->first.next = a;
            PL_COUNT_ARENA(pool,++);
            return(rp);
        }
    }

    /* we got to here, and there's no memory to allocate */
    return(NULL);
} /* --- end PL_ArenaAllocate() --- */

PR_IMPLEMENT(void *) PL_ArenaGrow(
    PLArenaPool *pool, void *p, PRUint32 size, PRUint32 incr)
{
    void *newp;

    if (PR_UINT32_MAX - size < incr)
        return NULL;
    PL_ARENA_ALLOCATE(newp, pool, size + incr);
    if (newp)
        memcpy(newp, p, size);
    return newp;
}

/*
 * Free tail arenas linked after head, which may not be the true list head.
 * Reset pool->current to point to head in case it pointed at a tail arena.
 */
static void FreeArenaList(PLArenaPool *pool, PLArena *head, PRBool reallyFree)
{
    PLArena **ap, *a;

    ap = &head->next;
    a = *ap;
    if (!a)
        return;

#ifdef DEBUG
    do {
        Assert(a->base <= a->avail && a->avail <= a->limit);
        a->avail = a->base;
        PL_CLEAR_UNUSED(a);
    } while ((a = a->next) != 0);
    a = *ap;
#endif

    if (reallyFree) {
        do {
            *ap = a->next;
            PL_CLEAR_ARENA(a);
            PL_COUNT_ARENA(pool,--);
            RTMemFree(a);
        } while ((a = *ap) != 0);
    } else {
        /* Insert the whole arena chain at the front of the freelist. */
        do {
            ap = &(*ap)->next;
        } while (*ap);
        LockArena();
        *ap = arena_freelist;
        arena_freelist = a;
        head->next = 0;
        UnlockArena();
    }

    pool->current = head;
}

PR_IMPLEMENT(void) PL_ArenaRelease(PLArenaPool *pool, char *mark)
{
    PLArena *a;

    for (a = pool->first.next; a; a = a->next) {
        if (PR_UPTRDIFF(mark, a->base) < PR_UPTRDIFF(a->avail, a->base)) {
            a->avail = (PRUword)PL_ARENA_ALIGN(pool, mark);
            FreeArenaList(pool, a, PR_FALSE);
            return;
        }
    }
}

PR_IMPLEMENT(void) PL_FreeArenaPool(PLArenaPool *pool)
{
    FreeArenaList(pool, &pool->first, PR_FALSE);
}

PR_IMPLEMENT(void) PL_FinishArenaPool(PLArenaPool *pool)
{
    FreeArenaList(pool, &pool->first, PR_TRUE);
}

PR_IMPLEMENT(void) PL_CompactArenaPool(PLArenaPool *ap)
{
}

PR_IMPLEMENT(void) PL_ArenaFinish(void)
{
    PLArena *a, *next;

    for (a = arena_freelist; a; a = next) {
        next = a->next;
        RTMemFree(a);
    }
    arena_freelist = NULL;

    if (g_hMtxArena != NIL_RTSEMFASTMUTEX) {
        RTSemFastMutexDestroy(g_hMtxArena);
        g_hMtxArena = NIL_RTSEMFASTMUTEX;
    }
}

