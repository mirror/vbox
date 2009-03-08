/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "packspu.h"
#include "cr_packfunctions.h"
#include "cr_glstate.h"
#include "packspu_proto.h"

void PACKSPU_APIENTRY packspu_FogCoordPointerEXT( GLenum type, GLsizei stride, const GLvoid *pointer )
{
#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object) {
		if (pack_spu.swap)
			crPackFogCoordPointerEXTSWAP( type, stride, pointer );
		else
			crPackFogCoordPointerEXT( type, stride, pointer );
	}
#endif
	crStateFogCoordPointerEXT( type, stride, pointer );
}

void PACKSPU_APIENTRY packspu_ColorPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *pointer )
{
#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object) {
		if (pack_spu.swap)
			crPackColorPointerSWAP( size, type, stride, pointer );
		else
			crPackColorPointer( size, type, stride, pointer );
	}
#endif
	crStateColorPointer( size, type, stride, pointer );
}

void PACKSPU_APIENTRY packspu_SecondaryColorPointerEXT( GLint size, GLenum type, GLsizei stride, const GLvoid *pointer )
{
#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object) {
		if (pack_spu.swap)
			crPackSecondaryColorPointerEXTSWAP( size, type, stride, pointer );
		else
			crPackSecondaryColorPointerEXT( size, type, stride, pointer );
	}
#endif
	crStateSecondaryColorPointerEXT( size, type, stride, pointer );
}

void PACKSPU_APIENTRY packspu_VertexPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *pointer )
{
#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	CRASSERT(ctx->clientState->extensions.ARB_vertex_buffer_object);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object) {
		if (pack_spu.swap)
			crPackVertexPointerSWAP( size, type, stride, pointer );
		else
			crPackVertexPointer( size, type, stride, pointer );
	}
#endif
	crStateVertexPointer( size, type, stride, pointer );
}

void PACKSPU_APIENTRY packspu_TexCoordPointer( GLint size, GLenum type, GLsizei stride, const GLvoid *pointer )
{
#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object) {
		if (pack_spu.swap)
			crPackTexCoordPointerSWAP( size, type, stride, pointer );
		else
			crPackTexCoordPointer( size, type, stride, pointer );
	}
#endif
	crStateTexCoordPointer( size, type, stride, pointer );
}

void PACKSPU_APIENTRY packspu_NormalPointer( GLenum type, GLsizei stride, const GLvoid *pointer )
{
#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object) {
		if (pack_spu.swap)
			crPackNormalPointerSWAP( type, stride, pointer );
		else
			crPackNormalPointer( type, stride, pointer );
	}
#endif
	crStateNormalPointer( type, stride, pointer );
}

void PACKSPU_APIENTRY packspu_EdgeFlagPointer( GLsizei stride, const GLvoid *pointer )
{
#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object) {
		if (pack_spu.swap)
			crPackEdgeFlagPointerSWAP( stride, pointer );
		else
			crPackEdgeFlagPointer( stride, pointer );
	}
#endif
	crStateEdgeFlagPointer( stride, pointer );
}

void PACKSPU_APIENTRY packspu_VertexAttribPointerARB( GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer )
{
#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object) {
		if (pack_spu.swap)
			crPackVertexAttribPointerARBSWAP( index, size, type, normalized, stride, pointer );
		else
			crPackVertexAttribPointerARB( index, size, type, normalized, stride, pointer );
	}
#endif
	crStateVertexAttribPointerARB( index, size, type, normalized, stride, pointer );
}

void PACKSPU_APIENTRY packspu_VertexAttribPointerNV( GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer )
{
#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object) {
		if (pack_spu.swap)
			crPackVertexAttribPointerNVSWAP( index, size, type, stride, pointer );
		else
			crPackVertexAttribPointerNV( index, size, type, stride, pointer );
	}
#endif
	crStateVertexAttribPointerNV( index, size, type, stride, pointer );
}

void PACKSPU_APIENTRY packspu_GetPointerv( GLenum pname, GLvoid **params )
{
	crStateGetPointerv( pname, params );
}

void PACKSPU_APIENTRY packspu_InterleavedArrays( GLenum format, GLsizei stride, const GLvoid *pointer )
{
#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object) {
		if (pack_spu.swap)
			crPackInterleavedArraysSWAP( format, stride, pointer );
		else
			crPackInterleavedArrays( format, stride, pointer );
	}
#endif
	crStateInterleavedArrays( format, stride, pointer );
}


void PACKSPU_APIENTRY
packspu_ArrayElement( GLint index )
{
	GLboolean serverArrays = GL_FALSE;

#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object)
		serverArrays = crStateUseServerArrays();
#endif

	if (serverArrays) {
		/* Send the DrawArrays command over the wire */
		if (pack_spu.swap)
			crPackArrayElementSWAP( index );
		else
			crPackArrayElement( index );
	}
	else {
		/* evaluate locally */
		GET_CONTEXT(ctx);
		CRClientState *clientState = &(ctx->clientState->client);
		if (pack_spu.swap)
			crPackExpandArrayElementSWAP( index, clientState );
		else
			crPackExpandArrayElement( index, clientState );
	}
}


void PACKSPU_APIENTRY
packspu_DrawElements( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices )
{
	GLboolean serverArrays = GL_FALSE;

#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object)
		serverArrays = crStateUseServerArrays();
#endif

	if (serverArrays) {
		/* Send the DrawArrays command over the wire */
		if (pack_spu.swap)
			crPackDrawElementsSWAP( mode, count, type, indices );
		else
			crPackDrawElements( mode, count, type, indices );
	}
	else {
		/* evaluate locally */
		GET_CONTEXT(ctx);
		CRClientState *clientState = &(ctx->clientState->client);
		if (pack_spu.swap)
			crPackExpandDrawElementsSWAP( mode, count, type, indices, clientState );
		else
			crPackExpandDrawElements( mode, count, type, indices, clientState );
	}
}


void PACKSPU_APIENTRY
packspu_DrawRangeElements( GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices )
{
	GLboolean serverArrays = GL_FALSE;

#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object)
		 serverArrays = crStateUseServerArrays();
#endif

	if (serverArrays) {
		/* Send the DrawRangeElements command over the wire */
		if (pack_spu.swap)
			crPackDrawRangeElementsSWAP( mode, start, end, count, type, indices );
		else
			crPackDrawRangeElements( mode, start, end, count, type, indices );
	}
	else {
		/* evaluate locally */
		GET_CONTEXT(ctx);
		CRClientState *clientState = &(ctx->clientState->client);
		if (pack_spu.swap)
			crPackExpandDrawRangeElementsSWAP( mode, start, end, count, type, indices, clientState );
		else
			crPackExpandDrawRangeElements( mode, start, end, count, type, indices, clientState );
	}
}


void PACKSPU_APIENTRY
packspu_DrawArrays( GLenum mode, GLint first, GLsizei count )
{
	GLboolean serverArrays = GL_FALSE;

#if CR_ARB_vertex_buffer_object
	GET_CONTEXT(ctx);
	if (ctx->clientState->extensions.ARB_vertex_buffer_object)
		 serverArrays = crStateUseServerArrays();
#endif

	if (serverArrays) {
		/* Send the DrawArrays command over the wire */
		if (pack_spu.swap)
			crPackDrawArraysSWAP( mode, first, count );
		else
			crPackDrawArrays( mode, first, count );
	}
	else {
		/* evaluate locally */
		GET_CONTEXT(ctx);
		CRClientState *clientState = &(ctx->clientState->client);
		if (pack_spu.swap)
			crPackExpandDrawArraysSWAP( mode, first, count, clientState );
		else
			crPackExpandDrawArrays( mode, first, count, clientState );
	}
}


#ifdef CR_EXT_multi_draw_arrays
void PACKSPU_APIENTRY packspu_MultiDrawArraysEXT( GLenum mode, GLint *first, GLsizei *count, GLsizei primcount )
{
	GLint i;
	for (i = 0; i < primcount; i++) {
		if (count[i] > 0) {
			packspu_DrawArrays(mode, first[i], count[i]);
		}
	}
}

void PACKSPU_APIENTRY packspu_MultiDrawElementsEXT( GLenum mode, const GLsizei *count, GLenum type, const GLvoid **indices, GLsizei primcount )
{
	GLint i;
	for (i = 0; i < primcount; i++) {
		if (count[i] > 0) {
			packspu_DrawElements(mode, count[i], type, indices[i]);
		}
	}
}
#endif


void PACKSPU_APIENTRY packspu_EnableClientState( GLenum array )
{
	crStateEnableClientState( array );
}

void PACKSPU_APIENTRY packspu_DisableClientState( GLenum array )
{
	crStateDisableClientState( array );
}

void PACKSPU_APIENTRY packspu_ClientActiveTextureARB( GLenum texUnit )
{
	crStateClientActiveTextureARB( texUnit );
	/* XXX also send to server for texcoord arrays? */
}


void PACKSPU_APIENTRY packspu_Enable( GLenum cap )
{
	switch (cap) {
	case GL_VERTEX_ARRAY:
	case GL_NORMAL_ARRAY:
	case GL_COLOR_ARRAY:
	case GL_INDEX_ARRAY:
	case GL_TEXTURE_COORD_ARRAY:
	case GL_EDGE_FLAG_ARRAY:
	case GL_FOG_COORDINATE_ARRAY_EXT:
	case GL_SECONDARY_COLOR_ARRAY_EXT:
#if CR_NV_vertex_program
	case GL_VERTEX_ATTRIB_ARRAY0_NV:
	case GL_VERTEX_ATTRIB_ARRAY1_NV:
	case GL_VERTEX_ATTRIB_ARRAY2_NV:
	case GL_VERTEX_ATTRIB_ARRAY3_NV:
	case GL_VERTEX_ATTRIB_ARRAY4_NV:
	case GL_VERTEX_ATTRIB_ARRAY5_NV:
	case GL_VERTEX_ATTRIB_ARRAY6_NV:
	case GL_VERTEX_ATTRIB_ARRAY7_NV:
	case GL_VERTEX_ATTRIB_ARRAY8_NV:
	case GL_VERTEX_ATTRIB_ARRAY9_NV:
	case GL_VERTEX_ATTRIB_ARRAY10_NV:
	case GL_VERTEX_ATTRIB_ARRAY11_NV:
	case GL_VERTEX_ATTRIB_ARRAY12_NV:
	case GL_VERTEX_ATTRIB_ARRAY13_NV:
	case GL_VERTEX_ATTRIB_ARRAY14_NV:
	case GL_VERTEX_ATTRIB_ARRAY15_NV:
#endif /* CR_NV_vertex_program */
		crStateEnableClientState(cap);
		break;
	default:
		;
	}

	if (pack_spu.swap)
		crPackEnableSWAP(cap);
	else
		crPackEnable(cap);
}


void PACKSPU_APIENTRY packspu_Disable( GLenum cap )
{
	switch (cap) {
	case GL_VERTEX_ARRAY:
	case GL_NORMAL_ARRAY:
	case GL_COLOR_ARRAY:
	case GL_INDEX_ARRAY:
	case GL_TEXTURE_COORD_ARRAY:
	case GL_EDGE_FLAG_ARRAY:
	case GL_FOG_COORDINATE_ARRAY_EXT:
	case GL_SECONDARY_COLOR_ARRAY_EXT:
#if CR_NV_vertex_program
	case GL_VERTEX_ATTRIB_ARRAY0_NV:
	case GL_VERTEX_ATTRIB_ARRAY1_NV:
	case GL_VERTEX_ATTRIB_ARRAY2_NV:
	case GL_VERTEX_ATTRIB_ARRAY3_NV:
	case GL_VERTEX_ATTRIB_ARRAY4_NV:
	case GL_VERTEX_ATTRIB_ARRAY5_NV:
	case GL_VERTEX_ATTRIB_ARRAY6_NV:
	case GL_VERTEX_ATTRIB_ARRAY7_NV:
	case GL_VERTEX_ATTRIB_ARRAY8_NV:
	case GL_VERTEX_ATTRIB_ARRAY9_NV:
	case GL_VERTEX_ATTRIB_ARRAY10_NV:
	case GL_VERTEX_ATTRIB_ARRAY11_NV:
	case GL_VERTEX_ATTRIB_ARRAY12_NV:
	case GL_VERTEX_ATTRIB_ARRAY13_NV:
	case GL_VERTEX_ATTRIB_ARRAY14_NV:
	case GL_VERTEX_ATTRIB_ARRAY15_NV:
#endif /* CR_NV_vertex_program */
		crStateDisableClientState(cap);
		break;
	default:
		;
	}

	if (pack_spu.swap)
		crPackDisableSWAP(cap);
	else
		crPackDisable(cap);
}


void PACKSPU_APIENTRY packspu_PushClientAttrib( GLbitfield mask )
{
	crStatePushClientAttrib(mask);
}

void PACKSPU_APIENTRY packspu_PopClientAttrib( void )
{
	crStatePopClientAttrib();
}
