/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "state.h"
#include "cr_error.h"
#include "cr_mem.h"
#include "cr_pixeldata.h"

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

void crStateApplyFBImage(CRContext *to)
{
    if (to->buffer.pFrontImg || to->buffer.pBackImg)
    {
        CRBufferState *pBuf = &to->buffer;
        CRPixelPackState unpack = to->client.unpack;

        diff_api.PixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        diff_api.PixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        diff_api.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
        diff_api.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        diff_api.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
        diff_api.PixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
        diff_api.PixelStorei(GL_UNPACK_SWAP_BYTES, 0);
        diff_api.PixelStorei(GL_UNPACK_LSB_FIRST, 0);

        if (to->framebufferobject.drawFB)
        {
            diff_api.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
        }

        if (to->bufferobject.unpackBuffer->hwid>0)
        {
            diff_api.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
        }

        diff_api.Disable(GL_ALPHA_TEST);
        diff_api.Disable(GL_SCISSOR_TEST);
        diff_api.Disable(GL_BLEND);
        diff_api.Disable(GL_COLOR_LOGIC_OP);

        if (pBuf->pFrontImg)
        {
            diff_api.DrawBuffer(GL_FRONT);
            diff_api.WindowPos2iARB(0, 0);
            diff_api.DrawPixels(pBuf->storedWidth, pBuf->storedHeight, GL_RGBA, GL_UNSIGNED_BYTE, pBuf->pFrontImg);
            crDebug("Applied %ix%i fb image", pBuf->storedWidth, pBuf->storedHeight);
            crFree(pBuf->pFrontImg);
            pBuf->pFrontImg = NULL;
        }

        if (pBuf->pBackImg)
        {
            diff_api.DrawBuffer(GL_BACK);
            diff_api.WindowPos2iARB(0, 0);
            diff_api.DrawPixels(pBuf->storedWidth, pBuf->storedHeight, GL_RGBA, GL_UNSIGNED_BYTE, pBuf->pBackImg);
            crDebug("Applied %ix%i bb image", pBuf->storedWidth, pBuf->storedHeight);
            crFree(pBuf->pBackImg);
            pBuf->pBackImg = NULL;
        }

        diff_api.WindowPos3fvARB(to->current.rasterAttrib[VERT_ATTRIB_POS]);
        if (to->bufferobject.unpackBuffer->hwid>0)
        {
            diff_api.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, to->bufferobject.unpackBuffer->hwid);
        }
        if (to->framebufferobject.drawFB)
        {
            diff_api.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, to->framebufferobject.drawFB->hwid);
        }
        diff_api.DrawBuffer(to->framebufferobject.drawFB ? 
                            to->framebufferobject.drawFB->drawbuffer[0] : to->buffer.drawBuffer);
        if (to->buffer.alphaTest)
        {
            diff_api.Enable(GL_ALPHA_TEST);
        }
        if (to->viewport.scissorTest)
        {
            diff_api.Enable(GL_SCISSOR_TEST);
        }
        if (to->buffer.blend)
        {
            diff_api.Enable(GL_BLEND);
        }
        if (to->buffer.logicOp)
        {
            diff_api.Enable(GL_COLOR_LOGIC_OP);
        }

        diff_api.PixelStorei(GL_UNPACK_SKIP_ROWS, unpack.skipRows);
        diff_api.PixelStorei(GL_UNPACK_SKIP_PIXELS, unpack.skipPixels);
        diff_api.PixelStorei(GL_UNPACK_ALIGNMENT, unpack.alignment);
        diff_api.PixelStorei(GL_UNPACK_ROW_LENGTH, unpack.rowLength);
        diff_api.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, unpack.imageHeight);
        diff_api.PixelStorei(GL_UNPACK_SKIP_IMAGES, unpack.skipImages);
        diff_api.PixelStorei(GL_UNPACK_SWAP_BYTES, unpack.swapBytes);
        diff_api.PixelStorei(GL_UNPACK_LSB_FIRST, unpack.psLSBFirst);

        diff_api.Finish();
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
#ifdef CR_EXT_framebuffer_object
    /*Note, this should go after crStateTextureSwitch*/
    crStateFramebufferObjectSwitch(from, to);
#endif
#ifdef CR_OPENGL_VERSION_2_0
    crStateGLSLSwitch(from, to);
#endif
	if (CHECKDIRTY(sb->current.dirty, bitID))
	{
		crStateCurrentSwitch( &(sb->current), bitID, from, to );
	}

#ifdef WINDOWS
    crStateApplyFBImage(to);
#endif
}

CRContext * crStateSwichPrepare(CRContext *toCtx, GLboolean fMultipleContexts, GLuint idFBO)
{
    CRContext *fromCtx = GetCurrentContext();

    if (!fMultipleContexts)
    {
#ifdef CR_EXT_framebuffer_object
        if (fromCtx)
            crStateFramebufferObjectDisableHW(fromCtx, idFBO);
#endif
    }
    return fromCtx;
}

void crStateSwichPostprocess(CRContext *fromCtx, GLboolean fMultipleContexts, GLuint idFBO)
{
    CRContext *toCtx = GetCurrentContext();;
    if (!fromCtx || !toCtx)
        return;

    if (!fMultipleContexts)
    {
#ifdef CR_EXT_framebuffer_object
        crStateFramebufferObjectReenableHW(fromCtx, toCtx, idFBO);
#endif
    }
}
