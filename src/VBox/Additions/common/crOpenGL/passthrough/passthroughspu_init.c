/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_error.h"
#include "passthroughspu.h"

static SPUFunctions passthrough_functions = {
	NULL, /* CHILD COPY */
	NULL, /* DATA */
	_cr_passthrough_table /* THE ACTUAL FUNCTIONS */
};

static SPUFunctions *
passthroughSPUInit( int id, SPU *child, SPU *self,
										unsigned int context_id,
										unsigned int num_contexts )
{
	(void) id;
	(void) self;
	(void) context_id;
	(void) num_contexts;

	if (child == NULL)
	{
		crError( "You can't load the passthrough SPU as the last SPU in a chain!" );
	}
	BuildPassthroughTable( child );
	return &passthrough_functions;
}

static void
passthroughSPUSelfDispatch(SPUDispatchTable *parent)
{
	(void)parent;
}

static int
passthroughSPUCleanup(void)
{
	return 1;
}


int SPULoad( char **name, char **super, SPUInitFuncPtr *init,
	     SPUSelfDispatchFuncPtr *self, SPUCleanupFuncPtr *cleanup,
	     int *flags )
{
	*name = "passthrough";
	*super = NULL;
	*init = passthroughSPUInit;
	*self = passthroughSPUSelfDispatch;
	*cleanup = passthroughSPUCleanup;
	*flags = (SPU_NO_PACKER|SPU_NOT_TERMINAL|SPU_MAX_SERVERS_ZERO);

	return 1;
}
