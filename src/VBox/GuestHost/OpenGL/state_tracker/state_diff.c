/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "state.h"
#include "cr_error.h"

void crStateDiffContext( CRContext *from, CRContext *to )
{
	CRbitvalue *bitID = from->bitid;
	CRStateBits *sb = GetCurrentBits();

	/*crDebug( "Diffing two contexts!" ); */

	if (CHECKDIRTY(sb->transform.dirty, bitID))
	{
		crStateTransformDiff( &(sb->transform), bitID, from, to );
	}
	if (CHECKDIRTY(sb->pixel.dirty, bitID))
	{
		crStatePixelDiff( &(sb->pixel), bitID, from, to );
	}
	if (CHECKDIRTY(sb->viewport.dirty, bitID))
	{
		crStateViewportDiff( &(sb->viewport), bitID, from, to );
	}
	if (CHECKDIRTY(sb->fog.dirty, bitID))
	{
		crStateFogDiff( &(sb->fog), bitID, from, to );
	}
	if (CHECKDIRTY(sb->texture.dirty, bitID))
	{
		crStateTextureDiff( &(sb->texture), bitID, from, to );
	}
	if (CHECKDIRTY(sb->lists.dirty, bitID))
	{
		crStateListsDiff( &(sb->lists), bitID, from, to );
	}
	if (CHECKDIRTY(sb->buffer.dirty, bitID))
	{
		crStateBufferDiff( &(sb->buffer), bitID, from, to );
	}
#ifdef CR_ARB_vertex_buffer_object
	if (CHECKDIRTY(sb->bufferobject.dirty, bitID))
	{
		crStateBufferObjectDiff( &(sb->bufferobject), bitID, from, to );
	}
#endif
	if (CHECKDIRTY(sb->client.dirty, bitID))
	{
		crStateClientDiff(&(sb->client), bitID, from, to );
	}
	if (CHECKDIRTY(sb->hint.dirty, bitID))
	{
		crStateHintDiff( &(sb->hint), bitID, from, to );
	}
	if (CHECKDIRTY(sb->lighting.dirty, bitID))
	{
		crStateLightingDiff( &(sb->lighting), bitID, from, to );
	}
	if (CHECKDIRTY(sb->line.dirty, bitID))
	{
		crStateLineDiff( &(sb->line), bitID, from, to );
	}
	if (CHECKDIRTY(sb->occlusion.dirty, bitID))
	{
		crStateOcclusionDiff( &(sb->occlusion), bitID, from, to );
	}
	if (CHECKDIRTY(sb->point.dirty, bitID))
	{
		crStatePointDiff( &(sb->point), bitID, from, to );
	}
	if (CHECKDIRTY(sb->polygon.dirty, bitID))
	{
		crStatePolygonDiff( &(sb->polygon), bitID, from, to );
	}
	if (CHECKDIRTY(sb->program.dirty, bitID))
	{
		crStateProgramDiff( &(sb->program), bitID, from, to );
	}
	if (CHECKDIRTY(sb->stencil.dirty, bitID))
	{
		crStateStencilDiff( &(sb->stencil), bitID, from, to );
	}
	if (CHECKDIRTY(sb->eval.dirty, bitID))
	{
		crStateEvaluatorDiff( &(sb->eval), bitID, from, to );
	}
#ifdef CR_ARB_imaging
	if (CHECKDIRTY(sb->imaging.dirty, bitID))
	{
		crStateImagingDiff( &(sb->imaging), bitID, from, to );
	}
#endif
#if 0
	if (CHECKDIRTY(sb->selection.dirty, bitID))
	{
		crStateSelectionDiff( &(sb->selection), bitID, from, to );
	}
#endif
#ifdef CR_NV_register_combiners
	if (CHECKDIRTY(sb->regcombiner.dirty, bitID) && to->extensions.NV_register_combiners)
	{
		crStateRegCombinerDiff( &(sb->regcombiner), bitID, from, to );
	}
#endif
#ifdef CR_ARB_multisample
	if (CHECKDIRTY(sb->multisample.dirty, bitID) &&
			from->extensions.ARB_multisample)
	{
		crStateMultisampleDiff( &(sb->multisample), bitID, from, to );
	}
#endif
	if (CHECKDIRTY(sb->current.dirty, bitID))
	{
		crStateCurrentDiff( &(sb->current), bitID, from, to );
	}
}

void crStateSwitchContext( CRContext *from, CRContext *to )
{
	CRbitvalue *bitID = to->bitid;
	CRStateBits *sb = GetCurrentBits();

	if (CHECKDIRTY(sb->attrib.dirty, bitID))
	{
		crStateAttribSwitch(&(sb->attrib), bitID, from, to );
	}
	if (CHECKDIRTY(sb->transform.dirty, bitID))
	{
		crStateTransformSwitch( &(sb->transform), bitID, from, to );
	}
	if (CHECKDIRTY(sb->pixel.dirty, bitID))
	{
		crStatePixelSwitch(&(sb->pixel), bitID, from, to );
	}
	if (CHECKDIRTY(sb->viewport.dirty, bitID))
	{
		crStateViewportSwitch(&(sb->viewport), bitID, from, to );
	}
	if (CHECKDIRTY(sb->fog.dirty, bitID))
	{
		crStateFogSwitch(&(sb->fog), bitID, from, to );
	}
	if (CHECKDIRTY(sb->texture.dirty, bitID))
	{
		crStateTextureSwitch( &(sb->texture), bitID, from, to );
	}
	if (CHECKDIRTY(sb->lists.dirty, bitID))
	{
		crStateListsSwitch(&(sb->lists), bitID, from, to );
	}
	if (CHECKDIRTY(sb->buffer.dirty, bitID))
	{
		crStateBufferSwitch( &(sb->buffer), bitID, from, to );
	}
#ifdef CR_ARB_vertex_buffer_object
	if (CHECKDIRTY(sb->bufferobject.dirty, bitID))
	{
		crStateBufferObjectSwitch( &(sb->bufferobject), bitID, from, to );
	}
#endif
	if (CHECKDIRTY(sb->client.dirty, bitID))
	{
		crStateClientSwitch( &(sb->client), bitID, from, to );
	}
#if 0
	if (CHECKDIRTY(sb->hint.dirty, bitID))
	{
		crStateHintSwitch( &(sb->hint), bitID, from, to );
	}
#endif
	if (CHECKDIRTY(sb->lighting.dirty, bitID))
	{
		crStateLightingSwitch( &(sb->lighting), bitID, from, to );
	}
	if (CHECKDIRTY(sb->occlusion.dirty, bitID))
	{
		crStateOcclusionSwitch( &(sb->occlusion), bitID, from, to );
	}
	if (CHECKDIRTY(sb->line.dirty, bitID))
	{
		crStateLineSwitch( &(sb->line), bitID, from, to );
	}
	if (CHECKDIRTY(sb->point.dirty, bitID))
	{
		crStatePointSwitch( &(sb->point), bitID, from, to );
	}
	if (CHECKDIRTY(sb->polygon.dirty, bitID))
	{
		crStatePolygonSwitch( &(sb->polygon), bitID, from, to );
	}
	if (CHECKDIRTY(sb->program.dirty, bitID))
	{
		crStateProgramSwitch( &(sb->program), bitID, from, to );
	}
	if (CHECKDIRTY(sb->stencil.dirty, bitID))
	{
		crStateStencilSwitch( &(sb->stencil), bitID, from, to );
	}
	if (CHECKDIRTY(sb->eval.dirty, bitID))
	{
		crStateEvaluatorSwitch( &(sb->eval), bitID, from, to );
	}
#ifdef CR_ARB_imaging
	if (CHECKDIRTY(sb->imaging.dirty, bitID))
	{
		crStateImagingSwitch( &(sb->imaging), bitID, from, to );
	}
#endif
#if 0
	if (CHECKDIRTY(sb->selection.dirty, bitID))
	{
		crStateSelectionSwitch( &(sb->selection), bitID, from, to );
	}
#endif
#ifdef CR_NV_register_combiners
	if (CHECKDIRTY(sb->regcombiner.dirty, bitID) && to->extensions.NV_register_combiners)
	{
		crStateRegCombinerSwitch( &(sb->regcombiner), bitID, from, to );
	}
#endif
#ifdef CR_ARB_multisample
	if (CHECKDIRTY(sb->multisample.dirty, bitID))
	{
		crStateMultisampleSwitch( &(sb->multisample), bitID, from, to );
	}
#endif
#ifdef CR_ARB_multisample
	if (CHECKDIRTY(sb->multisample.dirty, bitID))
	{
		crStateMultisampleSwitch(&(sb->multisample), bitID, from, to );
	}
#endif
	if (CHECKDIRTY(sb->current.dirty, bitID))
	{
		crStateCurrentSwitch( &(sb->current), bitID, from, to );
	}
}
