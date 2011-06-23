/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "state.h"
#include "cr_mem.h"
#include "cr_error.h"
#include "cr_spu.h"

#ifdef CHROMIUM_THREADSAFE
CRtsd __contextTSD;
#else
CRContext *__currentContext = NULL;
#endif

CRStateBits *__currentBits = NULL;
GLboolean g_availableContexts[CR_MAX_CONTEXTS];

static CRSharedState *gSharedState=NULL;

static CRContext *defaultContext = NULL;



/**
 * Allocate a new shared state object.
 * Contains texture objects, display lists, etc.
 */
static CRSharedState *
crStateAllocShared(void)
{
    CRSharedState *s = (CRSharedState *) crCalloc(sizeof(CRSharedState));
    if (s) {
        s->textureTable = crAllocHashtable();
        s->dlistTable = crAllocHashtable();
        s->buffersTable = crAllocHashtable();
        s->fbTable = crAllocHashtable();
        s->rbTable = crAllocHashtable();
        s->refCount = 1; /* refcount is number of contexts using this state */
        s->saveCount = 0;
    }
    return s;
}



/**
 * Callback used for crFreeHashtable().
 */
static void
DeleteTextureCallback(void *texObj)
{
#ifndef IN_GUEST
    diff_api.DeleteTextures(1, &((CRTextureObj *)texObj)->hwid);
#endif
    crStateDeleteTextureObject((CRTextureObj *) texObj);
}

/**
 * Decrement shared state's refcount and delete when it hits zero.
 */
DECLEXPORT(void)
crStateFreeShared(CRSharedState *s)
{
    s->refCount--;
    if (s->refCount <= 0) {
        if (s==gSharedState)
        {
            gSharedState = NULL;
        }
        crFreeHashtable(s->textureTable, DeleteTextureCallback);
        crFreeHashtable(s->dlistTable, crFree); /* call crFree for each entry */
        crFreeHashtable(s->buffersTable, crStateFreeBufferObject);
        crFreeHashtable(s->fbTable, crStateFreeFBO);
        crFreeHashtable(s->rbTable, crStateFreeRBO);
        crFree(s);
    }
}

DECLEXPORT(void) STATE_APIENTRY
crStateShareContext(GLboolean value)
{
    CRContext *pCtx = GetCurrentContext();
    CRASSERT(pCtx && pCtx->shared);

    if (value)
    {
        if (pCtx->shared == gSharedState)
        {
            return;
        }

        crDebug("Context(%i) shared", pCtx->id);

        if (!gSharedState)
        {
            gSharedState = pCtx->shared;
        }
        else
        {
            crStateFreeShared(pCtx->shared);
            pCtx->shared = gSharedState;
            gSharedState->refCount++;
        }
    }
    else
    {
        if (pCtx->shared != gSharedState)
        {
            return;
        }

        crDebug("Context(%i) unshared", pCtx->id);

        if (gSharedState->refCount==1)
        {
            gSharedState = NULL;
        }
        else
        {
            pCtx->shared = crStateAllocShared();
            pCtx->shared->id = pCtx->id;
            crStateFreeShared(gSharedState);
        }
    }
}

DECLEXPORT(GLboolean) STATE_APIENTRY
crStateContextIsShared(CRContext *pCtx)
{
    return pCtx->shared==gSharedState;
}

DECLEXPORT(void) STATE_APIENTRY
crStateSetSharedContext(CRContext *pCtx)
{
    if (gSharedState)
    {
        crWarning("crStateSetSharedContext: shared is being changed from %p to %p", gSharedState, pCtx->shared);
    }

    gSharedState = pCtx->shared;
}

/*
 * Helper for crStateCreateContext, below.
 */
static CRContext *
crStateCreateContextId(int i, const CRLimitsState *limits,
                                             GLint visBits, CRContext *shareCtx)
{
    CRContext *ctx = (CRContext *) crCalloc( sizeof( *ctx ) );
    int j;
    int node32 = i >> 5;
    int node = i & 0x1f;

    ctx->id = i;
    ctx->flush_func = NULL;
    for (j=0;j<CR_MAX_BITARRAY;j++){
        if (j == node32) {
            ctx->bitid[j] = (1 << node);
        } else {
            ctx->bitid[j] = 0;
        }
        ctx->neg_bitid[j] = ~(ctx->bitid[j]);
    }

    if (shareCtx) {
        CRASSERT(shareCtx->shared);
        ctx->shared = shareCtx->shared;
        ctx->shared->refCount ++;
    }
    else {
        ctx->shared = crStateAllocShared();
        ctx->shared->id = ctx->id;
    }

    /* use Chromium's OpenGL defaults */
    crStateLimitsInit( &(ctx->limits) );
    crStateExtensionsInit( &(ctx->limits), &(ctx->extensions) );

    crStateBufferObjectInit( ctx ); /* must precede client state init! */
    crStateClientInit( &(ctx->client) );

    crStateBufferInit( ctx );
    crStateCurrentInit( ctx );
    crStateEvaluatorInit( ctx );
    crStateFogInit( ctx );
    crStateHintInit( ctx );
    crStateLightingInit( ctx );
    crStateLineInit( ctx );
    crStateListsInit( ctx );
    crStateMultisampleInit( ctx );
    crStateOcclusionInit( ctx );
    crStatePixelInit( ctx );
    crStatePolygonInit( ctx );
    crStatePointInit( ctx );
    crStateProgramInit( ctx );
    crStateRegCombinerInit( ctx );
    crStateStencilInit( ctx );
    crStateTextureInit( ctx );
    crStateTransformInit( ctx );
    crStateViewportInit ( ctx );
    crStateFramebufferObjectInit(ctx);
    crStateGLSLInit(ctx);

    /* This has to come last. */
    crStateAttribInit( &(ctx->attrib) );

    ctx->renderMode = GL_RENDER;

    /* Initialize values that depend on the visual mode */
    if (visBits & CR_DOUBLE_BIT) {
        ctx->limits.doubleBuffer = GL_TRUE;
    }
    if (visBits & CR_RGB_BIT) {
        ctx->limits.redBits = 8;
        ctx->limits.greenBits = 8;
        ctx->limits.blueBits = 8;
        if (visBits & CR_ALPHA_BIT) {
            ctx->limits.alphaBits = 8;
        }
    }
    else {
        ctx->limits.indexBits = 8;
    }
    if (visBits & CR_DEPTH_BIT) {
        ctx->limits.depthBits = 24;
    }
    if (visBits & CR_STENCIL_BIT) {
        ctx->limits.stencilBits = 8;
    }
    if (visBits & CR_ACCUM_BIT) {
        ctx->limits.accumRedBits = 16;
        ctx->limits.accumGreenBits = 16;
        ctx->limits.accumBlueBits = 16;
        if (visBits & CR_ALPHA_BIT) {
            ctx->limits.accumAlphaBits = 16;
        }
    }
    if (visBits & CR_STEREO_BIT) {
        ctx->limits.stereo = GL_TRUE;
    }
    if (visBits & CR_MULTISAMPLE_BIT) {
        ctx->limits.sampleBuffers = 1;
        ctx->limits.samples = 4;
        ctx->multisample.enabled = GL_TRUE;
    }

    if (visBits & CR_OVERLAY_BIT) {
        ctx->limits.level = 1;
    }

    return ctx;
}

/*@todo crStateAttribDestroy*/
static void
crStateFreeContext(CRContext *ctx)
{
    crStateClientDestroy( &(ctx->client) );
    crStateLimitsDestroy( &(ctx->limits) );
    crStateBufferObjectDestroy( ctx );
    crStateEvaluatorDestroy( ctx );
    crStateListsDestroy( ctx );
    crStateLightingDestroy( ctx );
    crStateOcclusionDestroy( ctx );
    crStateProgramDestroy( ctx );
    crStateTextureDestroy( ctx );
    crStateTransformDestroy( ctx );
    crStateFreeShared(ctx->shared);
    crStateFramebufferObjectDestroy(ctx);
    crStateGLSLDestroy(ctx);
    if (ctx->buffer.pFrontImg) crFree(ctx->buffer.pFrontImg);
    if (ctx->buffer.pBackImg) crFree(ctx->buffer.pBackImg);
    crFree( ctx );
}


/*
 * Allocate the state (dirty) bits data structures.
 * This should be called before we create any contexts.
 * We'll also create the default/NULL context at this time and make
 * it the current context by default.  This means that if someone
 * tries to set GL state before calling MakeCurrent() they'll be
 * modifying the default state object, and not segfaulting on a NULL
 * pointer somewhere.
 */
void crStateInit(void)
{
    unsigned int i;

    /* Purely initialize the context bits */
    if (!__currentBits) {
        __currentBits = (CRStateBits *) crCalloc( sizeof(CRStateBits) );
        crStateClientInitBits( &(__currentBits->client) );
        crStateLightingInitBits( &(__currentBits->lighting) );
    } else
        crWarning("State tracker is being re-initialized..\n");

    for (i=0;i<CR_MAX_CONTEXTS;i++)
        g_availableContexts[i] = 0;

    if (defaultContext) {
        /* Free the default/NULL context.
         * Ensures context bits are reset */
        crStateFreeContext(defaultContext);
#ifdef CHROMIUM_THREADSAFE
        crSetTSD(&__contextTSD, NULL);
#else
        __currentContext = NULL;
#endif
    }

    /* Reset diff_api */
    crMemZero(&diff_api, sizeof(SPUDispatchTable));

    /* Allocate the default/NULL context */
    defaultContext = crStateCreateContextId(0, NULL, CR_RGB_BIT, NULL);
    CRASSERT(g_availableContexts[0] == 0);
    g_availableContexts[0] = 1; /* in use forever */

#ifdef CHROMIUM_THREADSAFE
    crSetTSD(&__contextTSD, defaultContext);
#else
    __currentContext = defaultContext;
#endif
}

void crStateDestroy(void)
{
    if (__currentBits)
    {
        crStateClientDestroyBits(&(__currentBits->client));
        crStateLightingDestroyBits(&(__currentBits->lighting));
        crFree(__currentBits);
        __currentBits = NULL;
    }

#ifdef CHROMIUM_THREADSAFE
    crFreeTSD(&__contextTSD);
#endif
}

/*
 * Notes on context switching and the "default context".
 *
 * See the paper "Tracking Graphics State for Networked Rendering"
 * by Ian Buck, Greg Humphries and Pat Hanrahan for background
 * information about how the state tracker and context switching
 * works.
 *
 * When we make a new context current, we call crStateSwitchContext()
 * in order to transform the 'from' context into the 'to' context
 * (i.e. the old context to the new context).  The transformation
 * is accomplished by calling GL functions through the 'diff_api'
 * so that the downstream GL machine (represented by the __currentContext
 * structure) is updated to reflect the new context state.  Finally, 
 * we point __currentContext to the new context.
 *
 * A subtle problem we have to deal with is context destruction.
 * This issue arose while testing with Glean.  We found that when
 * the currently bound context was getting destroyed that state
 * tracking was incorrect when a subsequent new context was activated.
 * In DestroyContext, the __hwcontext was being set to NULL and effectively
 * going away.  Later in MakeCurrent we had no idea what the state of the
 * downstream GL machine was (since __hwcontext was gone).  This meant
 * we had nothing to 'diff' against and the downstream GL machine was
 * in an unknown state.
 *
 * The solution to this problem is the "default/NULL" context.  The
 * default context is created the first time CreateContext is called
 * and is never freed.  Whenever we get a crStateMakeCurrent(NULL) call
 * or destroy the currently bound context in crStateDestroyContext()
 * we call crStateSwitchContext() to switch to the default context and
 * then set the __currentContext pointer to point to the default context.
 * This ensures that the dirty bits are updated and the diff_api functions
 * are called to keep the downstream GL machine in a known state.
 * Finally, the __hwcontext variable is no longer needed now.
 *
 * Yeah, this is kind of a mind-bender, but it really solves the problem
 * pretty cleanly.
 *
 * -Brian
 */


CRContext *
crStateCreateContext(const CRLimitsState *limits, GLint visBits, CRContext *share)
{
    int i;

    /* Must have created the default context via crStateInit() first */
    CRASSERT(defaultContext);

    for (i = 1 ; i < CR_MAX_CONTEXTS ; i++)
    {
        if (!g_availableContexts[i])
        {
            g_availableContexts[i] = 1; /* it's no longer available */
            return crStateCreateContextId( i, limits, visBits, share );
        }
    }
    crError( "Out of available contexts in crStateCreateContexts (max %d)",
                     CR_MAX_CONTEXTS );
    /* never get here */
    return NULL;
}

CRContext *
crStateCreateContextEx(const CRLimitsState *limits, GLint visBits, CRContext *share, GLint presetID)
{
    if (presetID>0)
    {
        CRASSERT(!g_availableContexts[presetID]);
        g_availableContexts[presetID] = 1;
        return crStateCreateContextId(presetID, limits, visBits, share);
    }
    else return crStateCreateContext(limits, visBits, share);
}

void crStateDestroyContext( CRContext *ctx )
{
    CRContext *current = GetCurrentContext();

    if (current == ctx) {
        /* destroying the current context - have to be careful here */
        CRASSERT(defaultContext);
        /* Check to see if the differencer exists first,
           we may not have one, aka the packspu */
        if (diff_api.AlphaFunc)
            crStateSwitchContext(current, defaultContext);
#ifdef CHROMIUM_THREADSAFE
        crSetTSD(&__contextTSD, defaultContext);
#else
        __currentContext = defaultContext;
#endif
        /* ensure matrix state is also current */
        crStateMatrixMode(defaultContext->transform.matrixMode);
    }
    g_availableContexts[ctx->id] = 0;

    crStateFreeContext(ctx);
}


void crStateMakeCurrent( CRContext *ctx )
{
    CRContext *current = GetCurrentContext();

    if (ctx == NULL)
        ctx = defaultContext;

    if (current == ctx)
        return; /* no-op */

    CRASSERT(ctx);

    if (current) {
        /* Check to see if the differencer exists first,
           we may not have one, aka the packspu */
        if (diff_api.AlphaFunc)
            crStateSwitchContext( current, ctx );
    }

#ifdef CHROMIUM_THREADSAFE
    crSetTSD(&__contextTSD, ctx);
#else
    __currentContext = ctx;
#endif

    /* ensure matrix state is also current */
    crStateMatrixMode(ctx->transform.matrixMode);
}


/*
 * As above, but don't call crStateSwitchContext().
 */
void crStateSetCurrent( CRContext *ctx )
{
    CRContext *current = GetCurrentContext();

    if (ctx == NULL)
        ctx = defaultContext;

    if (current == ctx)
        return; /* no-op */

    CRASSERT(ctx);

#ifdef CHROMIUM_THREADSAFE
    crSetTSD(&__contextTSD, ctx);
#else
    __currentContext = ctx;
#endif

    /* ensure matrix state is also current */
    crStateMatrixMode(ctx->transform.matrixMode);
}


CRContext *crStateGetCurrent(void)
{
    return GetCurrentContext();
}


void crStateUpdateColorBits(void)
{
    /* This is a hack to force updating the 'current' attribs */
    CRStateBits *sb = GetCurrentBits();
    FILLDIRTY(sb->current.dirty);
    FILLDIRTY(sb->current.vertexAttrib[VERT_ATTRIB_COLOR0]);
}


void STATE_APIENTRY
crStateChromiumParameteriCR( GLenum target, GLint value )
{
    /* This no-op function helps smooth code-gen */
}

void STATE_APIENTRY
crStateChromiumParameterfCR( GLenum target, GLfloat value )
{
    /* This no-op function helps smooth code-gen */
}

void STATE_APIENTRY
crStateChromiumParametervCR( GLenum target, GLenum type, GLsizei count, const GLvoid *values )
{
    /* This no-op function helps smooth code-gen */
}

void STATE_APIENTRY
crStateGetChromiumParametervCR( GLenum target, GLuint index, GLenum type, GLsizei count, GLvoid *values )
{
    /* This no-op function helps smooth code-gen */
}

void STATE_APIENTRY
crStateReadPixels( GLint x, GLint y, GLsizei width, GLsizei height,
                                     GLenum format, GLenum type, GLvoid *pixels )
{
    /* This no-op function helps smooth code-gen */
}

