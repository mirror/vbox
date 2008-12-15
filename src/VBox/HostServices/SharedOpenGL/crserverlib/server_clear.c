/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_spu.h"
#include "chromium.h"
#include "cr_mem.h"
#include "cr_net.h"
#include "server_dispatch.h"
#include "server.h"


void SERVER_DISPATCH_APIENTRY crServerDispatchClear( GLenum mask )
{
	CRMuralInfo *mural = cr_server.curClient->currentMural;
	const RunQueue *q = cr_server.run_queue;

	if (cr_server.only_swap_once)
	{
		/* NOTE: we only do the clear for the _last_ client in the list.
		 * This is because in multi-threaded apps the zeroeth client may
		 * be idle and never call glClear at all.  See threadtest.c
		 * It's pretty likely that the last client will be active.
		 */
		if ((mask & GL_COLOR_BUFFER_BIT) &&
			(cr_server.curClient != cr_server.clients[cr_server.numClients - 1]))
		   return;
	}

	if (mural->numExtents == 0)
	{
		cr_server.head_spu->dispatch_table.Clear( mask );
	}
	else
	{
		int scissor_on, i;

		if (!mural->viewportValidated) {
			crServerComputeViewportBounds(&(cr_server.curClient->currentCtx->viewport), mural);
		}


		scissor_on = q->client->currentCtx->viewport.scissorTest;

		if (!scissor_on)
		{
			cr_server.head_spu->dispatch_table.Enable( GL_SCISSOR_TEST );
		}

		for ( i = 0; i < mural->numExtents; i++ )
		{
			crServerSetOutputBounds( mural, i );
			cr_server.head_spu->dispatch_table.Clear( mask );
		}
		if (!scissor_on)
		{
			cr_server.head_spu->dispatch_table.Disable( GL_SCISSOR_TEST );
		}
	}
}

static void __draw_poly(CRPoly *p)
{
	int b;

	cr_server.head_spu->dispatch_table.Begin(GL_POLYGON);
	for (b=0; b<p->npoints; b++)
		cr_server.head_spu->dispatch_table.Vertex2dv(p->points+2*b);
	cr_server.head_spu->dispatch_table.End();
}


void SERVER_DISPATCH_APIENTRY
crServerDispatchSwapBuffers( GLint window, GLint flags )
{
  CRMuralInfo *mural;

	mural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, window);
	if (!mural) {
		 return;
	}


	if (cr_server.only_swap_once)
	{
		/* NOTE: we only do the clear for the _last_ client in the list.
		 * This is because in multi-threaded apps the zeroeth client may
		 * be idle and never call glClear at all.  See threadtest.c
		 * It's pretty likely that the last client will be active.
		 */
		if (cr_server.curClient != cr_server.clients[cr_server.numClients - 1])
		{
			return;
		}
	}

	if (cr_server.overlapBlending)
	{
		int a;
		CRPoly *p;
		GLboolean lighting, fog, blend, cull, tex[3];
		GLenum mm, blendSrc, blendDst;
		GLcolorf col;
		CRContext *ctx = crStateGetCurrent();
		const CRmatrix *baseProj;

		/* 
		 * I've probably missed some state here, or it
	 	 * might be easier just to push/pop it....
		 */
		lighting = ctx->lighting.lighting;
		fog		 = ctx->fog.enable;
		tex[0] = 0;
		for (a=0; a<CR_MAX_TEXTURE_UNITS; a++)
		{
			if (!ctx->texture.unit[a].enabled1D) continue;
	
			tex[0] 	 = 1;
			break;
		}
		tex[1] = 0;
		for	(a=0; a<CR_MAX_TEXTURE_UNITS; a++)
		{
			if (!ctx->texture.unit[a].enabled2D) continue;

			tex[1] 	 = 1;
			break;
		}
		tex[2] = 0;
		for (a=0; a<CR_MAX_TEXTURE_UNITS; a++)
		{	
			if (!ctx->texture.unit[a].enabled3D) continue;
	
			tex[2] 	 = 1;
			break;
		}

		cull	 = ctx->polygon.cullFace;
		blend	 = ctx->buffer.blend;
		blendSrc = ctx->buffer.blendSrcRGB;
		blendDst = ctx->buffer.blendDstRGB;
		mm = ctx->transform.matrixMode;
		col.r = ctx->current.vertexAttrib[VERT_ATTRIB_COLOR0][0];
		col.g = ctx->current.vertexAttrib[VERT_ATTRIB_COLOR0][1];
		col.b = ctx->current.vertexAttrib[VERT_ATTRIB_COLOR0][2];
		col.a = ctx->current.vertexAttrib[VERT_ATTRIB_COLOR0][3];

		baseProj = &(cr_server.curClient->currentMural->extents[0].baseProjection);
	
		switch(mm)
		{
				case GL_PROJECTION:
					cr_server.head_spu->dispatch_table.PushMatrix();
					cr_server.head_spu->dispatch_table.LoadMatrixf((GLfloat *) baseProj);
					cr_server.head_spu->dispatch_table.MultMatrixf(cr_server.unnormalized_alignment_matrix);
					cr_server.head_spu->dispatch_table.MatrixMode(GL_MODELVIEW);
					cr_server.head_spu->dispatch_table.PushMatrix();
					cr_server.head_spu->dispatch_table.LoadIdentity();
					break;
				
				default:
					cr_server.head_spu->dispatch_table.MatrixMode(GL_MODELVIEW);
					/* fall through */
	
				case GL_MODELVIEW:
					cr_server.head_spu->dispatch_table.PushMatrix();
					cr_server.head_spu->dispatch_table.LoadIdentity();
					cr_server.head_spu->dispatch_table.MatrixMode(GL_PROJECTION);
					cr_server.head_spu->dispatch_table.PushMatrix();
					cr_server.head_spu->dispatch_table.LoadMatrixf((GLfloat *) baseProj);
					cr_server.head_spu->dispatch_table.MultMatrixf(cr_server.unnormalized_alignment_matrix);
					break;	
		}
	
		/* fix state */
		if (lighting)
			cr_server.head_spu->dispatch_table.Disable(GL_LIGHTING);
		if (fog)
			cr_server.head_spu->dispatch_table.Disable(GL_FOG);
		if (tex[0])
			cr_server.head_spu->dispatch_table.Disable(GL_TEXTURE_1D);
		if (tex[1])
			cr_server.head_spu->dispatch_table.Disable(GL_TEXTURE_2D);
		if (tex[2])
			cr_server.head_spu->dispatch_table.Disable(GL_TEXTURE_3D);
		if (cull)
			cr_server.head_spu->dispatch_table.Disable(GL_CULL_FACE);

		/* Regular Blending */
		if (cr_server.overlapBlending == 1)
		{
			if (!blend)
				cr_server.head_spu->dispatch_table.Enable(GL_BLEND);
			if ((blendSrc != GL_ZERO) && (blendDst != GL_SRC_ALPHA))
				cr_server.head_spu->dispatch_table.BlendFunc(GL_ZERO, GL_SRC_ALPHA);

			/* draw the blends */
			for (a=1; a<cr_server.num_overlap_levels; a++)
			{
				if (a-1 < cr_server.num_overlap_intens)
				{
					cr_server.head_spu->dispatch_table.Color4f(0, 0, 0, 
											cr_server.overlap_intens[a-1]);
				}
				else
				{
					cr_server.head_spu->dispatch_table.Color4f(0, 0, 0, 1);
				}
		
				p = cr_server.overlap_geom[a];
				while (p)
				{
					/* hopefully this isnt concave... */
					__draw_poly(p);
					p = p->next;
				}	
			}

			if (!blend)
				cr_server.head_spu->dispatch_table.Disable(GL_BLEND);
			if ((blendSrc != GL_ZERO) && (blendDst != GL_SRC_ALPHA))
				cr_server.head_spu->dispatch_table.BlendFunc(blendSrc, blendDst);
		}
		else
		/* Knockout Blending */
		{
			cr_server.head_spu->dispatch_table.Color4f(0, 0, 0, 1);

			if (blend)
				cr_server.head_spu->dispatch_table.Disable(GL_BLEND);
			p = cr_server.overlap_knockout;
			while (p)
			{
				__draw_poly(p);
				p = p->next;
			}	
			if (blend)
				cr_server.head_spu->dispatch_table.Enable(GL_BLEND);
		}


		/* return things to normal */
		switch (mm)
		{
			case GL_PROJECTION:
				cr_server.head_spu->dispatch_table.PopMatrix();
				cr_server.head_spu->dispatch_table.MatrixMode(GL_PROJECTION);
				cr_server.head_spu->dispatch_table.PopMatrix();
				break;
			case GL_MODELVIEW:
				cr_server.head_spu->dispatch_table.PopMatrix();
				cr_server.head_spu->dispatch_table.MatrixMode(GL_MODELVIEW);
				cr_server.head_spu->dispatch_table.PopMatrix();
				break;
			default:
				cr_server.head_spu->dispatch_table.PopMatrix();
				cr_server.head_spu->dispatch_table.MatrixMode(GL_MODELVIEW);
				cr_server.head_spu->dispatch_table.PopMatrix();
				cr_server.head_spu->dispatch_table.MatrixMode(mm);
				break;
		}

		if (lighting)
			cr_server.head_spu->dispatch_table.Enable(GL_LIGHTING);
		if (fog)
			cr_server.head_spu->dispatch_table.Enable(GL_FOG);
		if (tex[0])
			cr_server.head_spu->dispatch_table.Enable(GL_TEXTURE_1D);
		if (tex[1])
			cr_server.head_spu->dispatch_table.Enable(GL_TEXTURE_2D);
		if (tex[2])
			cr_server.head_spu->dispatch_table.Enable(GL_TEXTURE_3D);
		if (cull)
			cr_server.head_spu->dispatch_table.Enable(GL_CULL_FACE);
	
		cr_server.head_spu->dispatch_table.Color4f(col.r, col.g, col.b, col.a);
	}

	/* Check if using a file network */
	if (!cr_server.clients[0]->conn->actual_network && window == MAGIC_OFFSET)
		window = 0;

	cr_server.head_spu->dispatch_table.SwapBuffers( mural->spuWindow, flags );
}
