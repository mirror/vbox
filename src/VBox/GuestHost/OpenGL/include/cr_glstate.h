/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef CR_GLSTATE_H
#define CR_GLSTATE_H

/* Forward declaration since some of the state/cr_*.h files need the CRContext type */
struct CRContext;
typedef struct CRContext CRContext;

#include "cr_version.h"

#include "state/cr_buffer.h"
#include "state/cr_bufferobject.h"
#include "state/cr_client.h"
#include "state/cr_current.h"
#include "state/cr_evaluators.h"
#include "state/cr_feedback.h"
#include "state/cr_fog.h"
#include "state/cr_hint.h"
#include "state/cr_lighting.h"
#include "state/cr_limits.h"
#include "state/cr_line.h"
#include "state/cr_lists.h"
#include "state/cr_multisample.h"
#include "state/cr_occlude.h"
#include "state/cr_pixel.h"
#include "state/cr_point.h"
#include "state/cr_polygon.h"
#include "state/cr_program.h"
#include "state/cr_regcombiner.h"
#include "state/cr_stencil.h"
#include "state/cr_texture.h"
#include "state/cr_transform.h"
#include "state/cr_viewport.h"
#include "state/cr_attrib.h"
#include "state/cr_framebuffer.h"
#include "state/cr_glsl.h"

#include "state/cr_statefuncs.h"
#include "state/cr_stateerror.h"

#include "spu_dispatch_table.h"

#ifdef CHROMIUM_THREADSAFE
# include <cr_threads.h>
#endif

#include <iprt/cdefs.h>

#ifndef IN_GUEST
# include <VBox/vmm/ssm.h>
# include <iprt/asm.h>

# define CR_STATE_SHAREDOBJ_USAGE_INIT(_pObj) (crMemset((_pObj)->ctxUsage, 0, sizeof ((_pObj)->ctxUsage)))
# define CR_STATE_SHAREDOBJ_USAGE_SET(_pObj, _pCtx) (ASMBitSet((_pObj)->ctxUsage, (_pCtx)->id))
# define CR_STATE_SHAREDOBJ_USAGE_CLEAR(_pObj, _pCtx) (ASMBitClear((_pObj)->ctxUsage, (_pCtx)->id))
# define CR_STATE_SHAREDOBJ_USAGE_IS_USED(_pObj) (ASMBitFirstSet((_pObj)->ctxUsage, sizeof ((_pObj)->ctxUsage)<<3) >= 0)
#else
# define CR_STATE_SHAREDOBJ_USAGE_INIT(_pObj) do {} while (0)
# define CR_STATE_SHAREDOBJ_USAGE_SET(_pObj, _pCtx) do {} while (0)
# define CR_STATE_SHAREDOBJ_USAGE_CLEAR(_pObj, _pCtx) do {} while (0)
# define CR_STATE_SHAREDOBJ_USAGE_IS_USED(_pObj) (GL_FALSE)
#endif

#define CR_MAX_EXTENTS 256

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bit vectors describing GL state
 */
typedef struct {
    CRAttribBits      attrib;
    CRBufferBits      buffer;
#ifdef CR_ARB_vertex_buffer_object
    CRBufferObjectBits bufferobject;
#endif
    CRClientBits      client;
    CRCurrentBits     current;
    CREvaluatorBits   eval;
    CRFeedbackBits    feedback;
    CRFogBits         fog;
    CRHintBits        hint;
    CRLightingBits    lighting;
    CRLineBits        line;
    CRListsBits       lists;
    CRMultisampleBits multisample;
#if CR_ARB_occlusion_query
    CROcclusionBits   occlusion;
#endif
    CRPixelBits       pixel;
    CRPointBits       point;
    CRPolygonBits     polygon;
    CRProgramBits     program;
    CRRegCombinerBits regcombiner;
    CRSelectionBits   selection;
    CRStencilBits     stencil;
    CRTextureBits     texture;
    CRTransformBits   transform;
    CRViewportBits    viewport;
} CRStateBits;

typedef void (*CRStateFlushFunc)( void *arg );


typedef struct _CRSharedState {
    CRHashTable *textureTable;  /* all texture objects */
    CRHashTable *dlistTable;    /* all display lists */
    CRHashTable *buffersTable;  /* vbo/pbo */
    CRHashTable *fbTable;       /* frame buffers */
    CRHashTable *rbTable;       /* render buffers */

    GLint refCount;
    GLint id;                   /*unique shared state id, it's not always matching some existing context id!*/
    GLint saveCount;

    /* Indicates that we have to resend data to GPU on first glMakeCurrent call with owning context */
    GLboolean   bTexResyncNeeded;
    GLboolean   bVBOResyncNeeded;
    GLboolean   bFBOResyncNeeded;
} CRSharedState;

/**
 * Chromium version of the state variables in OpenGL
 */
struct CRContext {
    int id;

#ifdef CHROMIUM_THREADSAFE
    /* we keep reference counting of context's makeCurrent for different threads
     * this is primarily needed to avoid having an invalid memory reference in the TLS
     * when the context is assigned to more than one threads and then destroyed from
     * one of those, i.e.
     * 1. Thread1 -> MakeCurrent(ctx1);
     * 2. Thread2 -> MakeCurrent(ctx1);
     * 3. Thread1 -> Destroy(ctx1);
     * => Thread2 still refers to destroyed ctx1
     * */
    VBOXTLSREFDATA
#endif

    CRbitvalue bitid[CR_MAX_BITARRAY];
    CRbitvalue neg_bitid[CR_MAX_BITARRAY];

    CRSharedState *shared;

    GLenum     renderMode;

    GLenum     error;

    CRStateFlushFunc flush_func;
    void            *flush_arg;

    CRAttribState      attrib;
    CRBufferState      buffer;
#ifdef CR_ARB_vertex_buffer_object
    CRBufferObjectState bufferobject;
#endif
    CRClientState      client;
    CRCurrentState     current;
    CREvaluatorState   eval;
    CRExtensionState   extensions;
    CRFeedbackState    feedback;
    CRFogState         fog;
    CRHintState        hint;
    CRLightingState    lighting;
    CRLimitsState      limits;
    CRLineState        line;
    CRListsState       lists;
    CRMultisampleState multisample;
#if CR_ARB_occlusion_query
    CROcclusionState   occlusion;
#endif
    CRPixelState       pixel;
    CRPointState       point;
    CRPolygonState     polygon;
    CRProgramState     program;
    CRRegCombinerState regcombiner;
    CRSelectionState   selection;
    CRStencilState     stencil;
    CRTextureState     texture;
    CRTransformState   transform;
    CRViewportState    viewport;

#ifdef CR_EXT_framebuffer_object
    CRFramebufferObjectState    framebufferobject;
#endif

#ifdef CR_OPENGL_VERSION_2_0
    CRGLSLState        glsl;
#endif

    /** For buffering vertices for selection/feedback */
    /*@{*/
    GLuint    vCount;
    CRVertex  vBuffer[4];
    GLboolean lineReset;
    GLboolean lineLoop;
    /*@}*/
};


DECLEXPORT(void) crStateInit(void);
DECLEXPORT(void) crStateDestroy(void);
DECLEXPORT(void) crStateVBoxDetachThread();
DECLEXPORT(void) crStateVBoxAttachThread();
DECLEXPORT(CRContext *) crStateCreateContext(const CRLimitsState *limits, GLint visBits, CRContext *share);
DECLEXPORT(CRContext *) crStateCreateContextEx(const CRLimitsState *limits, GLint visBits, CRContext *share, GLint presetID);
DECLEXPORT(void) crStateMakeCurrent(CRContext *ctx);
DECLEXPORT(void) crStateSetCurrent(CRContext *ctx);
DECLEXPORT(CRContext *) crStateGetCurrent(void);
DECLEXPORT(void) crStateDestroyContext(CRContext *ctx);
DECLEXPORT(GLboolean) crStateEnableDiffOnMakeCurrent(GLboolean fEnable);

CRContext * crStateSwichPrepare(CRContext *toCtx);
void crStateSwichPostprocess(CRContext *fromCtx);

DECLEXPORT(void) crStateFlushFunc( CRStateFlushFunc ff );
DECLEXPORT(void) crStateFlushArg( void *arg );
DECLEXPORT(void) crStateDiffAPI( SPUDispatchTable *api );
DECLEXPORT(void) crStateUpdateColorBits( void );

DECLEXPORT(void) crStateSetCurrentPointers( CRContext *ctx, CRCurrentStatePointers *current );
DECLEXPORT(void) crStateResetCurrentPointers( CRCurrentStatePointers *current );

DECLEXPORT(void) crStateSetExtensionString( CRContext *ctx, const GLubyte *extensions );

DECLEXPORT(void) crStateDiffContext( CRContext *from, CRContext *to );
DECLEXPORT(void) crStateSwitchContext( CRContext *from, CRContext *to );
DECLEXPORT(void) crStateApplyFBImage(CRContext *to);

#ifndef IN_GUEST
DECLEXPORT(int32_t) crStateSaveContext(CRContext *pContext, PSSMHANDLE pSSM);
typedef DECLCALLBACK(CRContext*) FNCRSTATE_CONTEXT_GET(void*);
typedef FNCRSTATE_CONTEXT_GET *PFNCRSTATE_CONTEXT_GET;
DECLEXPORT(int32_t) crStateLoadContext(CRContext *pContext, CRHashTable * pCtxTable, PFNCRSTATE_CONTEXT_GET pfnCtxGet, PSSMHANDLE pSSM);
DECLEXPORT(void)    crStateFreeShared(CRContext *pContext, CRSharedState *s);
DECLEXPORT(void) crStateFreeShared(CRContext *pContext, CRSharedState *s);
#endif

DECLEXPORT(void) crStateSetTextureUsed(GLuint texture, GLboolean used);
DECLEXPORT(void) crStateDeleteTextureCallback(void *texObj);

   /* XXX move these! */

DECLEXPORT(void) STATE_APIENTRY
crStateChromiumParameteriCR( GLenum target, GLint value );

DECLEXPORT(void) STATE_APIENTRY
crStateChromiumParameterfCR( GLenum target, GLfloat value );

DECLEXPORT(void) STATE_APIENTRY
crStateChromiumParametervCR( GLenum target, GLenum type, GLsizei count, const GLvoid *values );

DECLEXPORT(void) STATE_APIENTRY
crStateGetChromiumParametervCR( GLenum target, GLuint index, GLenum type,
                                GLsizei count, GLvoid *values );

DECLEXPORT(void) STATE_APIENTRY
crStateReadPixels( GLint x, GLint y, GLsizei width, GLsizei height,
                   GLenum format, GLenum type, GLvoid *pixels );

DECLEXPORT(void) STATE_APIENTRY crStateShareContext(GLboolean value);
DECLEXPORT(void) STATE_APIENTRY crStateSetSharedContext(CRContext *pCtx);
DECLEXPORT(GLboolean) STATE_APIENTRY crStateContextIsShared(CRContext *pCtx);

DECLEXPORT(void) STATE_APIENTRY crStateQueryHWState();
#ifdef __cplusplus
}
#endif

#endif /* CR_GLSTATE_H */
