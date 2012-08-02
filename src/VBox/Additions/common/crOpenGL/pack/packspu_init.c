/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_mem.h"
#include "cr_spu.h"
#include "cr_glstate.h"
#include "packspu.h"
#include "cr_packfunctions.h"
#include <stdio.h>

extern SPUNamedFunctionTable _cr_pack_table[];

SPUFunctions pack_functions = {
    NULL, /* CHILD COPY */
    NULL, /* DATA */
    _cr_pack_table /* THE ACTUAL FUNCTIONS */
};

PackSPU pack_spu;

#ifdef CHROMIUM_THREADSAFE
CRtsd _PackTSD;
CRmutex _PackMutex;
#endif

static SPUFunctions *
packSPUInit( int id, SPU *child, SPU *self,
                         unsigned int context_id,
                         unsigned int num_contexts )
{
    ThreadInfo *thread;

    (void) context_id;
    (void) num_contexts;
    (void) child;
    (void) self;

#if defined(CHROMIUM_THREADSAFE) && !defined(WINDOWS)
    crInitMutex(&_PackMutex);
#endif

    pack_spu.id = id;

    packspuSetVBoxConfiguration( child );

#if defined(WINDOWS) && defined(VBOX_WITH_WDDM)
    pack_spu.bRunningUnderWDDM = !!GetModuleHandle("VBoxDispD3D");
#endif

    if (!CRPACKSPU_IS_WDDM_CRHGSMI())
    {
        /* This connects to the server, sets up the packer, etc. */
        thread = packspuNewThread(
#if defined(VBOX_WITH_CRHGSMI) && defined(IN_GUEST)
            NULL
#endif
                 );

        if (!thread) {
            return NULL;
        }
        CRASSERT( thread == &(pack_spu.thread[0]) );
        pack_spu.idxThreadInUse = 0;
    }

    packspuCreateFunctions();
    crStateInit();

    return &pack_functions;
}

static void
packSPUSelfDispatch(SPUDispatchTable *self)
{
    (void)self;
}

static int
packSPUCleanup(void)
{
    int i;
#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&_PackMutex);
#endif
    for (i=0; i<MAX_THREADS; ++i)
    {
        if (pack_spu.thread[i].inUse && pack_spu.thread[i].packer)
        {
            crPackDeleteContext(pack_spu.thread[i].packer);
        }
    }

    crFreeTSD(&_PackerTSD);
    crFreeTSD(&_PackTSD);
    
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&_PackMutex);
# ifndef WINDOWS
    crFreeMutex(&_PackMutex);
# endif
#endif
    return 1;
}

extern SPUOptions packSPUOptions[];

int SPULoad( char **name, char **super, SPUInitFuncPtr *init,
         SPUSelfDispatchFuncPtr *self, SPUCleanupFuncPtr *cleanup,
         SPUOptionsPtr *options, int *flags )
{
    *name = "pack";
    *super = NULL;
    *init = packSPUInit;
    *self = packSPUSelfDispatch;
    *cleanup = packSPUCleanup;
    *options = packSPUOptions;
    *flags = (SPU_HAS_PACKER|SPU_IS_TERMINAL|SPU_MAX_SERVERS_ONE);

    return 1;
}
