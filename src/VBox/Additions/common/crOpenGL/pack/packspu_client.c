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

    /*crDebug("packspu_InterleavedArrays");*/

    crStateInterleavedArrays( format, stride, pointer );
}


void PACKSPU_APIENTRY
packspu_ArrayElement( GLint index )
{
#if 0
    GLboolean serverArrays = GL_FALSE;

#if CR_ARB_vertex_buffer_object
    GET_CONTEXT(ctx);
    /*crDebug("packspu_ArrayElement index:%i", index);*/
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
#else
    GET_CONTEXT(ctx);
    CRClientState *clientState = &(ctx->clientState->client);
    crPackExpandArrayElement(index, clientState);
#endif
}


void PACKSPU_APIENTRY
packspu_DrawElements( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices )
{
    GLboolean serverArrays = GL_FALSE;

#if CR_ARB_vertex_buffer_object
    GET_CONTEXT(ctx);
    /*crDebug("DrawElements count=%d, indices=%p", count, indices);*/
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
        {
            //packspu_Begin(mode);
            crPackExpandDrawElements( mode, count, type, indices, clientState );
            //packspu_End();
        }
    }
}


void PACKSPU_APIENTRY
packspu_DrawRangeElements( GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices )
{
    GLboolean serverArrays = GL_FALSE;

#if CR_ARB_vertex_buffer_object
    GET_CONTEXT(ctx);
    /*crDebug("DrawRangeElements count=%d", count);*/
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
        {
            crPackExpandDrawRangeElements( mode, start, end, count, type, indices, clientState );
        }
    }
}


void PACKSPU_APIENTRY
packspu_DrawArrays( GLenum mode, GLint first, GLsizei count )
{
    GLboolean serverArrays = GL_FALSE;

#if CR_ARB_vertex_buffer_object
    GET_CONTEXT(ctx);
    /*crDebug("DrawArrays count=%d", count);*/
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
    crStateEnableClientState(array);
    crPackEnableClientState(array);
}

void PACKSPU_APIENTRY packspu_DisableClientState( GLenum array )
{
    crStateDisableClientState(array);
    crPackDisableClientState(array);
}

void PACKSPU_APIENTRY packspu_ClientActiveTextureARB( GLenum texUnit )
{
    crStateClientActiveTextureARB(texUnit);
    crPackClientActiveTextureARB(texUnit);
}

void PACKSPU_APIENTRY packspu_EnableVertexAttribArrayARB(GLuint index)
{
    crStateEnableVertexAttribArrayARB(index);
    crPackEnableVertexAttribArrayARB(index);
}


void PACKSPU_APIENTRY packspu_DisableVertexAttribArrayARB(GLuint index)
{
    crStateDisableVertexAttribArrayARB(index);
    crPackDisableVertexAttribArrayARB(index);
}

void PACKSPU_APIENTRY packspu_Enable( GLenum cap )
{
    crStateEnable(cap);

    if (pack_spu.swap)
        crPackEnableSWAP(cap);
    else
        crPackEnable(cap);
}


void PACKSPU_APIENTRY packspu_Disable( GLenum cap )
{
    crStateDisable(cap);

    if (pack_spu.swap)
        crPackDisableSWAP(cap);
    else
        crPackDisable(cap);
}

GLboolean PACKSPU_APIENTRY packspu_IsEnabled(GLenum cap)
{
    GLboolean res = crStateIsEnabled(cap);
#ifdef DEBUG
    {    
    	GET_THREAD(thread);
	    int writeback = 1;
	    GLboolean return_val = (GLboolean) 0;
        crPackIsEnabled(cap, &return_val, &writeback);
	    packspuFlush( (void *) thread );
	    while (writeback)
		  crNetRecv();
        CRASSERT(return_val==res);
    }
#endif

    return res;
}

void PACKSPU_APIENTRY packspu_PushClientAttrib( GLbitfield mask )
{
    crStatePushClientAttrib(mask);
    crPackPushClientAttrib(mask);
}

void PACKSPU_APIENTRY packspu_PopClientAttrib( void )
{
    crStatePopClientAttrib();
    crPackPopClientAttrib();
}
