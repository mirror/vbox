/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_error.h"
#include "cr_mem.h"
#include "packer.h"
#include <stdio.h>

#ifdef CHROMIUM_THREADSAFE
CRtsd _PackerTSD;
int cr_packer_globals;  /* dummy - for the sake of packer.def */
#else
int _PackerTSD;         /* dummy - for the sake of packer.def */  /* drm1 */
DLLDATA(CRPackContext) cr_packer_globals;
#endif

CRPackContext *crPackNewContext( int swapping )
{
#ifdef CHROMIUM_THREADSAFE
    CRPackContext *pc = crCalloc(sizeof(CRPackContext));
    if (!pc)
        return NULL;
    crInitMutex(&pc->mutex);
#else
    GET_PACKER_CONTEXT(pc);
        crMemZero( pc, sizeof(CRPackContext));
#endif
    pc->enmBeginEndState = CRPackBeginEndStateNone;
    pc->swapping = swapping;
    pc->Flush = NULL;
    pc->SendHuge = NULL;
    pc->updateBBOX = 0;
    return pc;
}

void crPackDeleteContext(CRPackContext *pc)
{
#ifdef CHROMIUM_THREADSAFE
    crFreeMutex(&pc->mutex);
    crFree(pc);
#endif
}

/* Set packing context for the calling thread */
void crPackSetContext( CRPackContext *pc )
{
#ifdef CHROMIUM_THREADSAFE
    crSetTSD( &_PackerTSD, pc );
#else
    CRASSERT( pc == &cr_packer_globals );
    (void)pc;
#endif
}


/* Return packing context for the calling thread */
CRPackContext *crPackGetContext( void )
{
#ifdef CHROMIUM_THREADSAFE
    return (CRPackContext *) crGetTSD( &_PackerTSD );
#else
    return &cr_packer_globals;
#endif
}

