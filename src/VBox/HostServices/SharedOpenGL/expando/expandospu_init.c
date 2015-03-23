/* $Id$ */
/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include <stdio.h>
#include "cr_spu.h"
#include "cr_dlm.h"
#include "cr_hash.h"
#include "expandospu.h"

ExpandoSPU expando_spu;

static SPUFunctions expando_functions = {
	NULL, /* CHILD COPY */
	NULL, /* DATA */
	_cr_expando_table /* THE ACTUAL FUNCTIONS */
};

static SPUFunctions *
expandoSPUInit( int id, SPU *child, SPU *self,
								 unsigned int context_id,
								 unsigned int num_contexts )
{

	(void) self;
	(void) context_id;
	(void) num_contexts;

	expando_spu.id = id;
	expando_spu.has_child = 0;
	expando_spu.server = NULL;
	if (child)
	{
		crSPUInitDispatchTable( &(expando_spu.child) );
		crSPUCopyDispatchTable( &(expando_spu.child), &(child->dispatch_table) );
		expando_spu.has_child = 1;
	}
	crSPUInitDispatchTable( &(expando_spu.super) );
	crSPUCopyDispatchTable( &(expando_spu.super), &(self->superSPU->dispatch_table) );
	expandospuGatherConfiguration();

	/* Expando-specific initialization */
	expando_spu.contextTable = crAllocHashtable();

	/* We'll be using the state tracker for each context */
	crStateInit();

	return &expando_functions;
}

static void
expandoSPUSelfDispatch(SPUDispatchTable *self)
{
	crSPUInitDispatchTable( &(expando_spu.self) );
	crSPUCopyDispatchTable( &(expando_spu.self), self );

	expando_spu.server = (CRServer *)(self->server);
}


static int
expandoSPUCleanup(void)
{
    crFreeHashtable(expando_spu.contextTable, expando_free_context_state);
    crStateDestroy();
    return 1;
}

int
SPULoad( char **name, char **super, SPUInitFuncPtr *init,
				 SPUSelfDispatchFuncPtr *self, SPUCleanupFuncPtr *cleanup,
				 SPUOptionsPtr *options, int *flags )
{
	*name = "expando";
	//*super = "passthrough";
	*super = "render";
	*init = expandoSPUInit;
	*self = expandoSPUSelfDispatch;
	*cleanup = expandoSPUCleanup;
	*options = expandoSPUOptions;
	*flags = (SPU_NO_PACKER|SPU_NOT_TERMINAL|SPU_MAX_SERVERS_ZERO);
	
	return 1;
}
