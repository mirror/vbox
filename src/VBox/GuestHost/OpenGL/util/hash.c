/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_threads.h"
#include "cr_hash.h"
#include "cr_mem.h"
#include "cr_error.h"

#define CR_MAXUINT             ((GLuint) 0xFFFFFFFF)

#define CR_NUM_BUCKETS 1047

typedef struct FreeElemRec {
    GLuint min;
    GLuint max;
    struct FreeElemRec *next;
    struct FreeElemRec *prev;
} FreeElem;

typedef struct CRHashIdPoolRec {
    FreeElem *freeList;
} CRHashIdPool;

typedef struct CRHashNode {
        unsigned long key;
        void *data;
        struct CRHashNode *next;
} CRHashNode;

struct CRHashTable {
        unsigned int num_elements;
        CRHashNode *buckets[CR_NUM_BUCKETS];
        CRHashIdPool   *idPool;
#ifdef CHROMIUM_THREADSAFE
    CRmutex mutex;
#endif
};


static CRHashIdPool *crAllocHashIdPool( void )
{
    CRHashIdPool *pool = (CRHashIdPool *) crCalloc(sizeof(CRHashIdPool));
    pool->freeList = (FreeElem *) crCalloc(sizeof(FreeElem));
    pool->freeList->min = 1;
    pool->freeList->max = CR_MAXUINT;
    pool->freeList->next = NULL;
    pool->freeList->prev = NULL;
    return pool;
}

static void crFreeHashIdPool( CRHashIdPool *pool )
{
    FreeElem *i, *next;
    for (i = pool->freeList; i; i = next)
    {
        next = i->next;
        crFree(i);
    }
    crFree(pool);
}

/*
 * Allocate a block of <count> IDs.  Return index of first one.
 * Return 0 if we fail.
 */
static GLuint crHashIdPoolAllocBlock( CRHashIdPool *pool, GLuint count )
{
    FreeElem *f;
    GLuint ret;

    CRASSERT(count > 0);

    f = pool->freeList;
    while (f)
    {
        if (f->max - f->min + 1 >= (GLuint) count)
        {
            /* found a sufficiently large enough block */
            ret = f->min;
            f->min += count;

            if (f->min == f->max)
            {
                /* remove this block from linked list */
                if (f == pool->freeList) 
                {
                    /* remove from head */
                    pool->freeList = pool->freeList->next;
                    pool->freeList->prev = NULL;
                }
                else 
                {
                    /* remove from elsewhere */
                    f->prev->next = f->next;
                    f->next->prev = f->prev;
                }
                crFree(f);
            }

#ifdef DEBUG
            /* make sure the IDs really are allocated */
            {
                GLuint i;
                for (i = 0; i < count; i++)
                {
                    //CRASSERT(crHashIdPoolIsIdUsed(pool, ret + i));
                }
            }
#endif

            return ret;
        }
        else {
            f = f->next;
        }
    }

    /* failed to find free block */
    crDebug("crHashIdPoolAllocBlock failed");
    return 0;
}


/*
 * Free a block of <count> IDs starting at <first>.
 */
static void crHashIdPoolFreeBlock( CRHashIdPool *pool, GLuint first, GLuint count )
{
    FreeElem *i;
    FreeElem *newelem;

    /*********************************/
    /* Add the name to the freeList  */
    /* Find the bracketing sequences */

    for (i = pool->freeList; i && i->next && i->next->min < first; i = i->next)
    {
        /* EMPTY BODY */
    }

    /* j will always be valid */
    if (!i) {
        return;
    }
    if (!i->next && i->max == first) {
        return;
    }

    /* Case:  j:(~,first-1) */
    if (i->max + 1 == first) 
    {
        i->max += count;
        if (i->next && i->max+1 >= i->next->min) 
        {
            /* Collapse */
            i->next->min = i->min;
            i->next->prev = i->prev;
            if (i->prev)
            {
                i->prev->next = i->next;
            }
            if (i == pool->freeList)
            {
                pool->freeList = i->next;
            }
            crFree(i);
        }
        return;
    }

    /* Case: j->next: (first+1, ~) */
    if (i->next && i->next->min - count == first) 
    {
        i->next->min -= count;
        if (i->max + 1 >= i->next->min) 
        {
            /* Collapse */
            i->next->min = i->min;
            i->next->prev = i->prev;
            if (i->prev)
            {
                i->prev->next = i->next;
            }
            if (i == pool->freeList) 
            {
                pool->freeList = i->next;
            }
            crFree(i);
        }
        return;
    }

    /* Case: j: (first+1, ~) j->next: null */
    if (!i->next && i->min - count == first) 
    {
        i->min -= count;
        return;
    }

    /* allocate a new FreeElem node */
    newelem = (FreeElem *) crCalloc(sizeof(FreeElem));
    newelem->min = first;
    newelem->max = first + count - 1;

    /* Case: j: (~,first-(2+))  j->next: (first+(2+), ~) or null */
    if (first > i->max) 
    {
        newelem->prev = i;
        newelem->next = i->next;
        if (i->next)
        {
            i->next->prev = newelem;
        }
        i->next = newelem;
        return;
    }

    /* Case: j: (first+(2+), ~) */
    /* Can only happen if j = t->freeList! */
    if (i == pool->freeList && i->min > first) 
    {
        newelem->next = i;
        newelem->prev = i->prev;
        i->prev = newelem;
        pool->freeList = newelem;
        return;
    }
}



/*
 * Mark the given Id as being allocated.
 */
static GLboolean crHashIdPoolAllocId( CRHashIdPool *pool, GLuint id )
{
    FreeElem *f;

    f = pool->freeList;
    while (f)
    {
        if (id >= f->min && id <= f->max)
        {
            /* found the block */
            if (id == f->min)
            {
                f->min++;
            }
            else if (id == f->max)
            {
                f->max--;
            }
            else
            {
                /* somewhere in the middle - split the block */
                FreeElem *newelem = (FreeElem *) crCalloc(sizeof(FreeElem));
                newelem->min = id + 1;
                newelem->max = f->max;
                f->max = id - 1;
                newelem->next = f->next;
                if (f->next)
                   f->next->prev = newelem;
                newelem->prev = f;
                f->next = newelem;
            }
            return GL_TRUE;
        }
        f = f->next;
    }

    /* if we get here, the ID was already allocated - that's OK */
    return GL_FALSE;
}


/*
 * Determine if the given id is free.  Return GL_TRUE if so.
 */
static GLboolean crHashIdPoolIsIdFree( const CRHashIdPool *pool, GLuint id )
{
    FreeElem *i;

    /* First find which region it fits in */
    for (i = pool->freeList; i && !(i->min <= id && id <= i->max); i=i->next)
    {
        /* EMPTY BODY */
    }

    if (i)
        return GL_TRUE;
    else
        return GL_FALSE;
}



CRHashTable *crAllocHashtable( void )
{
    int i;
    CRHashTable *hash = (CRHashTable *) crCalloc( sizeof( CRHashTable )) ;
    hash->num_elements = 0;
    for (i = 0 ; i < CR_NUM_BUCKETS ; i++)
    {
        hash->buckets[i] = NULL;
    }
    hash->idPool = crAllocHashIdPool();
#ifdef CHROMIUM_THREADSAFE
    crInitMutex(&hash->mutex);
#endif
    return hash;
}

void crFreeHashtable( CRHashTable *hash, CRHashtableCallback deleteFunc )
{
    int i;
    CRHashNode *entry, *next;

    if ( !hash) return;

#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&hash->mutex);
#endif

    for ( i = 0; i < CR_NUM_BUCKETS; i++ )
    {
        entry = hash->buckets[i];
        while (entry)
        {
            next = entry->next;
            /* Clear the key in case crHashtableDelete() is called
             * from this callback.
             */
            entry->key = 0;
            if (deleteFunc && entry->data)
            {
                (*deleteFunc)(entry->data);
            }
            crFree(entry);
            entry = next;

        }
    }
    crFreeHashIdPool( hash->idPool );

#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&hash->mutex);
    crFreeMutex(&hash->mutex);
#endif

    crFree( hash );
}

void crHashtableLock(CRHashTable *h)
{
#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&h->mutex);
#endif
}

void crHashtableUnlock(CRHashTable *h)
{
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&h->mutex);
#endif
}

void crHashtableWalk( CRHashTable *hash, CRHashtableWalkCallback walkFunc , void *dataPtr2)
{
    int i;
    CRHashNode *entry, *next;

    if (!hash)
        return;

#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&hash->mutex);
#endif
    for (i = 0; i < CR_NUM_BUCKETS; i++)
    {
        entry = hash->buckets[i];
        while (entry) 
        {
            /* save next ptr here, in case walkFunc deletes this entry */
            next = entry->next;
            if (entry->data && walkFunc) {
                (*walkFunc)( entry->key, entry->data, dataPtr2 );
            }
            entry = next;
        }
    }
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&hash->mutex);
#endif
}

static unsigned int crHash( unsigned long key )
{
    return key % CR_NUM_BUCKETS;
}

void crHashtableAdd( CRHashTable *h, unsigned long key, void *data )
{
    CRHashNode *node = (CRHashNode *) crCalloc( sizeof( CRHashNode ) );
#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&h->mutex);
#endif
    node->key = key;
    node->data = data;
    node->next = h->buckets[crHash( key )];
    h->buckets[ crHash( key ) ] = node;
    h->num_elements++;
    crHashIdPoolAllocId (h->idPool, key);
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&h->mutex);
#endif
}

GLboolean crHashtableAllocRegisterKey( CRHashTable *h,  GLuint key)
{
    GLboolean fAllocated;
#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&h->mutex);
#endif
    fAllocated = crHashIdPoolAllocId (h->idPool, key);
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&h->mutex);
#endif
    return fAllocated;
}

GLuint crHashtableAllocKeys( CRHashTable *h,  GLsizei range)
{
    GLuint res;
    int i;

#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&h->mutex);
#endif
    res = crHashIdPoolAllocBlock (h->idPool, range);
#ifdef DEBUG_misha
    Assert(res);
    for (i = 0; i < range; ++i)
    {
        void *search = crHashtableSearch( h, res+i );
        Assert(!search);
    }
#endif
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&h->mutex);
#endif
    return res;
}

void crHashtableDelete( CRHashTable *h, unsigned long key, CRHashtableCallback deleteFunc )
{
    unsigned int index = crHash( key );
    CRHashNode *temp, *beftemp = NULL;

#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&h->mutex);
#endif
    for ( temp = h->buckets[index]; temp; temp = temp->next )
    {
        if ( temp->key == key )
            break;
        beftemp = temp;
    }
    if ( !temp ) {
#ifdef CHROMIUM_THREADSAFE
        crUnlockMutex(&h->mutex);
#endif
        return; /* not an error */
    }
    if ( beftemp )
        beftemp->next = temp->next;
    else
        h->buckets[index] = temp->next;
    h->num_elements--;
    if (temp->data && deleteFunc) {
        (*deleteFunc)( temp->data );
    }

    crFree( temp );

    crHashIdPoolFreeBlock( h->idPool, key, 1 );
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&h->mutex);
#endif
}

void crHashtableDeleteBlock( CRHashTable *h, unsigned long key, GLsizei range, CRHashtableCallback deleteFunc )
{
    /* XXX optimize someday */
    GLuint i;
    for (i = 0; i < (GLuint)range; i++) {
        crHashtableDelete( h, key, deleteFunc );
    }
}

void *crHashtableSearch( const CRHashTable *h, unsigned long key )
{
    unsigned int index = crHash( key );
    CRHashNode *temp;
#ifdef CHROMIUM_THREADSAFE
    crLockMutex((CRmutex *)&h->mutex);
#endif
    for ( temp = h->buckets[index]; temp; temp = temp->next )
    {
        if ( temp->key == key )
            break;
    }
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex((CRmutex *)&h->mutex);
#endif
    if ( !temp )
    {
        return NULL;
    }
    return temp->data;
}

void crHashtableReplace( CRHashTable *h, unsigned long key, void *data,
                         CRHashtableCallback deleteFunc)
{
    unsigned int index = crHash( key );
    CRHashNode *temp;
#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&h->mutex);
#endif
    for ( temp = h->buckets[index]; temp; temp = temp->next )
    {
        if ( temp->key == key )
            break;
    }
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&h->mutex);
#endif
    if ( !temp )
    {
        crHashtableAdd( h, key, data );
        return;
    }
#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&h->mutex);
#endif
    if ( temp->data && deleteFunc )
    {
        (*deleteFunc)( temp->data );
    }
    temp->data = data;
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&h->mutex);
#endif
}

unsigned int crHashtableNumElements( const CRHashTable *h) 
{
    if (h)
        return h->num_elements;
    else
        return 0;
}

/*
 * Determine if the given key is used.  Return GL_TRUE if so.
 */
GLboolean crHashtableIsKeyUsed( const CRHashTable *h, GLuint id )
{
    return (GLboolean) !crHashIdPoolIsIdFree( h->idPool, id);
}

GLboolean crHashtableGetDataKey(CRHashTable *pHash, void *pData, unsigned long *pKey)
{
    int i;
    CRHashNode *entry;
    GLboolean rc = GL_FALSE;

    if (!pHash)
        return rc;

#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&pHash->mutex);
#endif
    for (i = 0; i<CR_NUM_BUCKETS && !rc; i++)
    {
        entry = pHash->buckets[i];
        while (entry) 
        {
            if (entry->data == pData) {
                if (pKey)
                    *pKey = entry->key;
                rc = GL_TRUE;
                break;              
            }
            entry = entry->next;
        }
    }
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&pHash->mutex);
#endif

    return rc;
}
