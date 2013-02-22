/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "state.h"
#include "cr_error.h"
#include "cr_mem.h"
#include "cr_pixeldata.h"
#include <iprt/err.h>

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

void crStateFreeFBImageLegacy(CRContext *to)
{
    if (to->buffer.pFrontImg)
    {
        crFree(to->buffer.pFrontImg);
        to->buffer.pFrontImg = NULL;
    }
    if (to->buffer.pBackImg)
    {
        crFree(to->buffer.pBackImg);
        to->buffer.pBackImg = NULL;
    }

    to->buffer.storedWidth = 0;
    to->buffer.storedHeight = 0;
}

int crStateAcquireFBImage(CRContext *to, CRFBData *data)
{
    CRBufferState *pBuf = &to->buffer;
    CRPixelPackState packing = to->client.pack;
    uint32_t i;

    diff_api.PixelStorei(GL_PACK_SKIP_ROWS, 0);
    diff_api.PixelStorei(GL_PACK_SKIP_PIXELS, 0);
    diff_api.PixelStorei(GL_PACK_ALIGNMENT, 1);
    diff_api.PixelStorei(GL_PACK_ROW_LENGTH, 0);
    diff_api.PixelStorei(GL_PACK_IMAGE_HEIGHT, 0);
    diff_api.PixelStorei(GL_PACK_SKIP_IMAGES, 0);
    diff_api.PixelStorei(GL_PACK_SWAP_BYTES, 0);
    diff_api.PixelStorei(GL_PACK_LSB_FIRST, 0);

    if (to->bufferobject.packBuffer->hwid>0)
    {
        diff_api.BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
    }

    for (i = 0; i < data->cElements; ++i)
    {
        CRFBDataElement *el = &data->aElements[i];

        diff_api.BindFramebufferEXT(GL_READ_FRAMEBUFFER, el->idFBO);

        if (el->enmBuffer)
            diff_api.ReadBuffer(el->enmBuffer);

        diff_api.ReadPixels(el->posX, el->posY, el->width, el->height, el->enmFormat, el->enmType, el->pvData);
        crDebug("Acquired %d;%d;%d;%d;%d;0x%p fb image", el->enmBuffer, el->width, el->height, el->enmFormat, el->enmType, el->pvData);
    }

    if (to->bufferobject.packBuffer->hwid>0)
    {
        diff_api.BindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, to->bufferobject.packBuffer->hwid);
    }
    if (to->framebufferobject.readFB)
    {
        CRASSERT(to->framebufferobject.readFB->hwid);
        diff_api.BindFramebufferEXT(GL_READ_FRAMEBUFFER, to->framebufferobject.readFB->hwid);
        diff_api.ReadBuffer(to->framebufferobject.readFB->readbuffer);

    }
    else if (data->idFBO)
    {
        diff_api.BindFramebufferEXT(GL_READ_FRAMEBUFFER, data->idFBO);
        diff_api.ReadBuffer(GL_COLOR_ATTACHMENT0);
    }
    else
    {
        diff_api.BindFramebufferEXT(GL_READ_FRAMEBUFFER, 0);
        diff_api.ReadBuffer(to->buffer.readBuffer);
    }

    diff_api.PixelStorei(GL_PACK_SKIP_ROWS, packing.skipRows);
    diff_api.PixelStorei(GL_PACK_SKIP_PIXELS, packing.skipPixels);
    diff_api.PixelStorei(GL_PACK_ALIGNMENT, packing.alignment);
    diff_api.PixelStorei(GL_PACK_ROW_LENGTH, packing.rowLength);
    diff_api.PixelStorei(GL_PACK_IMAGE_HEIGHT, packing.imageHeight);
    diff_api.PixelStorei(GL_PACK_SKIP_IMAGES, packing.skipImages);
    diff_api.PixelStorei(GL_PACK_SWAP_BYTES, packing.swapBytes);
    diff_api.PixelStorei(GL_PACK_LSB_FIRST, packing.psLSBFirst);
    return VINF_SUCCESS;
}

void crStateApplyFBImage(CRContext *to, CRFBData *data)
{
    {
        CRBufferState *pBuf = &to->buffer;
        CRPixelPackState unpack = to->client.unpack;
        uint32_t i;

        diff_api.PixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        diff_api.PixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        diff_api.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
        diff_api.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        diff_api.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
        diff_api.PixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
        diff_api.PixelStorei(GL_UNPACK_SWAP_BYTES, 0);
        diff_api.PixelStorei(GL_UNPACK_LSB_FIRST, 0);

        if (to->bufferobject.unpackBuffer->hwid>0)
        {
            diff_api.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
        }

        diff_api.Disable(GL_ALPHA_TEST);
        diff_api.Disable(GL_SCISSOR_TEST);
        diff_api.Disable(GL_BLEND);
        diff_api.Disable(GL_COLOR_LOGIC_OP);

        for (i = 0; i < data->cElements; ++i)
        {
            CRFBDataElement *el = &data->aElements[i];

            switch (el->enmFormat)
            {
                case GL_DEPTH_COMPONENT:
                    diff_api.Enable(GL_DEPTH_TEST);
                    break;
                case GL_STENCIL_INDEX:
                    diff_api.Enable(GL_STENCIL_TEST);
                    break;
                default:
                    break;
            }

            diff_api.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, el->idFBO);

            if (el->enmBuffer)
                diff_api.DrawBuffer(el->enmBuffer);

            diff_api.WindowPos2iARB(el->posX, el->posY);
            diff_api.DrawPixels(el->width, el->height, el->enmFormat, el->enmType, el->pvData);
            crDebug("Applied %d;%d;%d;%d;%d;0x%p fb image", el->enmBuffer, el->width, el->height, el->enmFormat, el->enmType, el->pvData);

            switch (el->enmFormat)
            {
                case GL_DEPTH_COMPONENT:
                    if (!to->buffer.depthTest)
                    {
                        diff_api.Disable(GL_DEPTH_TEST);
                    }
                    break;
                case GL_STENCIL_INDEX:
                    if (!to->stencil.stencilTest)
                    {
                        diff_api.Disable(GL_STENCIL_TEST);
                    }
                    break;
                default:
                    break;
            }

        }

        diff_api.WindowPos3fvARB(to->current.rasterAttrib[VERT_ATTRIB_POS]);
        if (to->bufferobject.unpackBuffer->hwid>0)
        {
            diff_api.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, to->bufferobject.unpackBuffer->hwid);
        }
        if (to->framebufferobject.drawFB)
        {
            CRASSERT(to->framebufferobject.drawFB->hwid);
            diff_api.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, to->framebufferobject.drawFB->hwid);
            diff_api.DrawBuffer(to->framebufferobject.drawFB->drawbuffer[0]);
        }
        else if (data->idFBO)
        {
            diff_api.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, data->idFBO);
            diff_api.DrawBuffer(GL_COLOR_ATTACHMENT0);
        }
        else
        {
            diff_api.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
            diff_api.DrawBuffer(to->buffer.drawBuffer);
        }
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
	if (to->buffer.pFrontImg)
	{
	    CRFBData *pLazyData = (CRFBData *)to->buffer.pFrontImg;
	    crStateApplyFBImage(to, pLazyData);
	    crStateFreeFBImageLegacy(to);
	}
#endif
}

void crStateSyncHWErrorState(CRContext *ctx)
{
    GLenum err;
    while ((err = diff_api.GetError()) != GL_NO_ERROR)
    {
        if (ctx->error != GL_NO_ERROR)
            ctx->error = err;
    }
}

void crStateSwitchPrepare(CRContext *toCtx, CRContext *fromCtx, GLuint idDrawFBO, GLuint idReadFBO)
{
    if (!fromCtx)
        return;

    if (g_bVBoxEnableDiffOnMakeCurrent && toCtx && toCtx != fromCtx)
        crStateSyncHWErrorState(fromCtx);

#ifdef CR_EXT_framebuffer_object
    crStateFramebufferObjectDisableHW(fromCtx, idDrawFBO, idReadFBO);
#endif
}

void crStateSwitchPostprocess(CRContext *toCtx, CRContext *fromCtx, GLuint idDrawFBO, GLuint idReadFBO)
{
    if (!toCtx)
        return;

    if (g_bVBoxEnableDiffOnMakeCurrent && fromCtx && toCtx != fromCtx)
    {
        GLenum err;
        while ((err = diff_api.GetError()) != GL_NO_ERROR)
        {
            crWarning("gl error (0x%x) after context switch, ignoring..", err);
        }
    }
#ifdef CR_EXT_framebuffer_object
    crStateFramebufferObjectReenableHW(fromCtx, toCtx, idDrawFBO, idReadFBO);
#endif
}
