/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef CR_HASH_H
#define CR_HASH_H

#include "chromium.h"

#include <iprt/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CRHashTable CRHashTable;

/* Callback function used for freeing/deleting table entries */
typedef void (*CRHashtableCallback)(void *data);
/* Callback function used for walking through table entries */
typedef void (*CRHashtableWalkCallback)(unsigned long key, void *data1, void *data2);

DECLEXPORT(CRHashTable *) crAllocHashtable( void );
DECLEXPORT(void) crFreeHashtable( CRHashTable *hash, CRHashtableCallback deleteCallback );
DECLEXPORT(void) crHashtableAdd( CRHashTable *h, unsigned long key, void *data );
DECLEXPORT(GLuint) crHashtableAllocKeys( CRHashTable *h, GLsizei range );
GLboolean crHashtableAllocRegisterKey( CRHashTable *h,  GLuint key);
DECLEXPORT(void) crHashtableDelete( CRHashTable *h, unsigned long key, CRHashtableCallback deleteCallback );
DECLEXPORT(void) crHashtableDeleteBlock( CRHashTable *h, unsigned long key, GLsizei range, CRHashtableCallback deleteFunc );
DECLEXPORT(void *) crHashtableSearch( const CRHashTable *h, unsigned long key );
DECLEXPORT(void) crHashtableReplace( CRHashTable *h, unsigned long key, void *data, CRHashtableCallback deleteFunc);
DECLEXPORT(unsigned int) crHashtableNumElements( const CRHashTable *h) ;
DECLEXPORT(GLboolean) crHashtableIsKeyUsed( const CRHashTable *h, GLuint id );
DECLEXPORT(void) crHashtableWalk( CRHashTable *hash, CRHashtableWalkCallback walkFunc , void *data);
/*Returns GL_TRUE if given hashtable hold the data, pKey is updated with key value for data in this case*/
DECLEXPORT(GLboolean) crHashtableGetDataKey(CRHashTable *pHash, void *pData, unsigned long *pKey);
DECLEXPORT(void) crHashtableLock(CRHashTable *h);
DECLEXPORT(void) crHashtableUnlock(CRHashTable *h);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CR_HASH_H */
