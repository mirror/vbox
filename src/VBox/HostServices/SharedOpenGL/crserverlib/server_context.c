/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */
    
#include "cr_spu.h"
#include "chromium.h"
#include "cr_error.h"
#include "cr_net.h"
#include "cr_rand.h"
#include "server_dispatch.h"
#include "server.h"
#include "cr_mem.h"
#include "cr_string.h"

GLint SERVER_DISPATCH_APIENTRY
crServerDispatchCreateContext(const char *dpyName, GLint visualBits, GLint shareCtx)
{
    return crServerDispatchCreateContextEx(dpyName, visualBits, shareCtx, -1, -1);
}

GLint crServerDispatchCreateContextEx(const char *dpyName, GLint visualBits, GLint shareCtx, GLint preloadCtxID, int32_t internalID)
{
    GLint retVal = -1;
    CRContext *newCtx;
    CRContextInfo *pContextInfo;
    GLboolean fFirst = GL_FALSE;

    if (shareCtx > 0) {
        crWarning("CRServer: context sharing not implemented.");
        shareCtx = 0;
    }

    pContextInfo = (CRContextInfo *) crAlloc(sizeof (CRContextInfo));
    if (!pContextInfo)
    {
        crWarning("failed to alloc context info!");
        return -1;
    }

    pContextInfo->CreateInfo.visualBits = visualBits;

    /* Since the Cr server serialized all incoming clients/contexts into
     * one outgoing GL stream, we only need to create one context for the
     * head SPU.  We'll only have to make it current once too, below.
     */
    if (cr_server.firstCallCreateContext) {
        cr_server.MainContextInfo.CreateInfo.visualBits = visualBits;
        cr_server.MainContextInfo.SpuContext = cr_server.head_spu->dispatch_table.
            CreateContext(dpyName, cr_server.MainContextInfo.CreateInfo.visualBits, shareCtx);
        if (cr_server.MainContextInfo.SpuContext < 0) {
            crWarning("crServerDispatchCreateContext() failed.");
            crFree(pContextInfo);
            return -1;
        }
        cr_server.firstCallCreateContext = GL_FALSE;
        fFirst = GL_TRUE;
    }
    else {
        /* second or third or ... context */
        if (!cr_server.bUseMultipleContexts && ((visualBits & cr_server.MainContextInfo.CreateInfo.visualBits) != visualBits)) {
            int oldSpuContext;

            /* the new context needs new visual attributes */
            cr_server.MainContextInfo.CreateInfo.visualBits |= visualBits;
            crDebug("crServerDispatchCreateContext requires new visual (0x%x).",
                    cr_server.MainContextInfo.CreateInfo.visualBits);

            /* Here, we used to just destroy the old rendering context.
             * Unfortunately, this had the side effect of destroying
             * all display lists and textures that had been loaded on
             * the old context as well.
             *
             * Now, first try to create a new context, with a suitable
             * visual, sharing display lists and textures with the
             * old context.  Then destroy the old context.
             */

            /* create new rendering context with suitable visual */
            oldSpuContext = cr_server.MainContextInfo.SpuContext;
            cr_server.MainContextInfo.SpuContext = cr_server.head_spu->dispatch_table.
                CreateContext(dpyName, cr_server.MainContextInfo.CreateInfo.visualBits, cr_server.MainContextInfo.SpuContext);
            /* destroy old rendering context */
            cr_server.head_spu->dispatch_table.DestroyContext(oldSpuContext);
            if (cr_server.MainContextInfo.SpuContext < 0) {
                crWarning("crServerDispatchCreateContext() failed.");
                crFree(pContextInfo);
                return -1;
            }
        }
    }

    if (cr_server.bUseMultipleContexts) {
        pContextInfo->SpuContext = cr_server.head_spu->dispatch_table.
                CreateContext(dpyName, cr_server.MainContextInfo.CreateInfo.visualBits, cr_server.MainContextInfo.SpuContext);
        if (pContextInfo->SpuContext < 0) {
            crWarning("crServerDispatchCreateContext() failed.");
            crStateEnableDiffOnMakeCurrent(GL_TRUE);
            cr_server.bUseMultipleContexts = GL_FALSE;
            if (!fFirst)
                crError("creating shared context failed, while it is expected to work!");
        }
        else if (fFirst)
        {
            crStateEnableDiffOnMakeCurrent(GL_FALSE);
        }
    }
    else
    {
        pContextInfo->SpuContext = -1;
    }

    /* Now create a new state-tracker context and initialize the
     * dispatch function pointers.
     */
    newCtx = crStateCreateContextEx(&cr_server.limits, visualBits, NULL, internalID);
    if (newCtx) {
        crStateSetCurrentPointers( newCtx, &(cr_server.current) );
        crStateResetCurrentPointers(&(cr_server.current));
        retVal = preloadCtxID<0 ? crServerGenerateID(&cr_server.idsPool.freeContextID) : preloadCtxID;

        pContextInfo->pContext = newCtx;
        pContextInfo->CreateInfo.visualBits = visualBits;
        pContextInfo->CreateInfo.externalID = retVal;
        pContextInfo->CreateInfo.pszDpyName = dpyName ? crStrdup(dpyName) : NULL;
        crHashtableAdd(cr_server.contextTable, retVal, pContextInfo);
    }

    if (retVal != -1 && !cr_server.bIsInLoadingState) {
        int pos;
        for (pos = 0; pos < CR_MAX_CONTEXTS; pos++) {
            if (cr_server.curClient->contextList[pos] == 0) {
                cr_server.curClient->contextList[pos] = retVal;
                break;
            }
        }
    }

    {
        /* As we're using only one host context to serve all client contexts, newly created context will still
         * hold last error value from any previous failed opengl call. Proper solution would be to redirect any
         * client glGetError calls to our state tracker, but right now it's missing quite a lot of checks and doesn't
         * reflect host driver/gpu specific issues. Thus we just reset last opengl error at context creation.
         */
        GLint err;

        err = cr_server.head_spu->dispatch_table.GetError();
        if (err!=GL_NO_ERROR)
        {
#ifdef DEBUG_misha
            crDebug("Cleared gl error %#x on context creation", err);
#else
            crWarning("Cleared gl error %#x on context creation", err);
#endif
        }
    }

    crServerReturnValue( &retVal, sizeof(retVal) );

    return retVal;
}

static int crServerRemoveClientContext(CRClient *pClient, GLint ctx)
{
    int pos;

    for (pos = 0; pos < CR_MAX_CONTEXTS; ++pos)
    {
        if (pClient->contextList[pos] == ctx)
        {
            pClient->contextList[pos] = 0;
            return true;
        }
    }

    return false;
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchDestroyContext( GLint ctx )
{
    CRContextInfo *crCtxInfo;
    CRContext *crCtx;
    int32_t client;
    CRClientNode *pNode;
    int found=false;

    crCtxInfo = (CRContextInfo *) crHashtableSearch(cr_server.contextTable, ctx);
    if (!crCtxInfo) {
        crWarning("CRServer: DestroyContext invalid context %d", ctx);
        return;
    }
    crCtx = crCtxInfo->pContext;
    CRASSERT(crCtx);

    crDebug("CRServer: DestroyContext context %d", ctx);

    crHashtableDelete(cr_server.contextTable, ctx, NULL);
    crStateDestroyContext( crCtx );

    if (crCtxInfo->CreateInfo.pszDpyName)
        crFree(crCtxInfo->CreateInfo.pszDpyName);

    if (crCtxInfo->SpuContext >= 0)
        cr_server.head_spu->dispatch_table.DestroyContext(crCtxInfo->SpuContext);

    crFree(crCtxInfo);

    if (cr_server.curClient)
    {
        /* If we delete our current context, default back to the null context */
        if (cr_server.curClient->currentCtxInfo == crCtxInfo) {
            cr_server.curClient->currentContextNumber = -1;
            cr_server.curClient->currentCtxInfo = &cr_server.MainContextInfo;
        }

        found = crServerRemoveClientContext(cr_server.curClient, ctx);

        /*Some application call destroy context not in a thread where it was created...have do deal with it.*/
        if (!found)
        {
            for (client=0; client<cr_server.numClients; ++client)
            {
                if (cr_server.clients[client]==cr_server.curClient)
                    continue;

                found = crServerRemoveClientContext(cr_server.clients[client], ctx);

                if (found) break;
            }
        }

        if (!found)
        {
            pNode=cr_server.pCleanupClient;

            while (pNode && !found)
            {
                found = crServerRemoveClientContext(pNode->pClient, ctx);
                pNode = pNode->next;
            }
        }

        CRASSERT(found);
    }

    /*Make sure this context isn't active in other clients*/
    for (client=0; client<cr_server.numClients; ++client)
    {
        if (cr_server.clients[client]->currentCtxInfo == crCtxInfo)
        {
            cr_server.clients[client]->currentContextNumber = -1;
            cr_server.clients[client]->currentCtxInfo = &cr_server.MainContextInfo;
        }
    }

    pNode=cr_server.pCleanupClient;
    while (pNode)
    {
        if (pNode->pClient->currentCtxInfo == crCtxInfo)
        {
            pNode->pClient->currentContextNumber = -1;
            pNode->pClient->currentCtxInfo = &cr_server.MainContextInfo;
        }
        pNode = pNode->next;
    }
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchMakeCurrent( GLint window, GLint nativeWindow, GLint context )
{
    CRMuralInfo *mural, *oldMural;
    CRContextInfo *ctxInfo = NULL;
    CRContext *ctx, *oldCtx = NULL;

    if (context >= 0 && window >= 0) {
        mural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, window);
        if (!mural)
        {
            crWarning("CRServer: invalid window %d passed to crServerDispatchMakeCurrent()", window);
            return;
        }

        /* Update the state tracker's current context */
        ctxInfo = (CRContextInfo *) crHashtableSearch(cr_server.contextTable, context);
        if (!ctxInfo) {
            crWarning("CRserver: NULL context in MakeCurrent %d", context);
            return;
        }
    }
    else {
#if 0
        oldMural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, cr_server.currentWindow);
        if (oldMural && oldMural->bUseFBO && crServerSupportRedirMuralFBO())
        {
            if (!crStateGetCurrent()->framebufferobject.drawFB)
            {
                cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
            }
            if (!crStateGetCurrent()->framebufferobject.readFB)
            {
                cr_server.head_spu->dispatch_table.BindFramebufferEXT(GL_READ_FRAMEBUFFER, 0);
            }
        }

        ctxInfo = &cr_server.MainContextInfo;
        window = -1;
        mural = NULL;
#endif
        cr_server.bForceMakeCurrentOnClientSwitch = GL_TRUE;
        return;
    }

    cr_server.bForceMakeCurrentOnClientSwitch = GL_FALSE;

    ctx = ctxInfo->pContext;
    CRASSERT(ctx);

    oldMural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, cr_server.currentWindow);

    /* Ubuntu 11.04 hosts misbehave if context window switch is
     * done with non-default framebuffer object settings.
     * crStateSwichPrepare & crStateSwichPostprocess are supposed to work around this problem
     * crStateSwichPrepare restores the FBO state to its default values before the context window switch,
     * while crStateSwichPostprocess restores it back to the original values */
    oldCtx = crStateSwichPrepare(ctx, cr_server.bUseMultipleContexts, oldMural && oldMural->bUseFBO && crServerSupportRedirMuralFBO() ? oldMural->idFBO : 0);

    /*
    crDebug("**** %s client %d  curCtx=%d curWin=%d", __func__,
                    cr_server.curClient->number, ctxPos, window);
    */
    cr_server.curClient->currentContextNumber = context;
    cr_server.curClient->currentCtxInfo = ctxInfo;
    cr_server.curClient->currentMural = mural;
    cr_server.curClient->currentWindow = window;

    CRASSERT(cr_server.curClient->currentCtxInfo);
    CRASSERT(cr_server.curClient->currentCtxInfo->pContext);

    /* This is a hack to force updating the 'current' attribs */
    crStateUpdateColorBits();

    if (ctx)
        crStateSetCurrentPointers( ctx, &(cr_server.current) );

    /* check if being made current for first time, update viewport */
#if 0
    if (ctx) {
        /* initialize the viewport */
        if (ctx->viewport.viewportW == 0) {
            ctx->viewport.viewportW = mural->width;
            ctx->viewport.viewportH = mural->height;
            ctx->viewport.scissorW = mural->width;
            ctx->viewport.scissorH = mural->height;
        }
    }
#endif

    /*
    crDebug("**** %s  currentWindow %d  newWindow %d", __func__,
                    cr_server.currentWindow, window);
    */

    if (1/*cr_server.firstCallMakeCurrent ||
            cr_server.currentWindow != window ||
            cr_server.currentNativeWindow != nativeWindow*/) {
        /* Since the cr server serialized all incoming contexts/clients into
         * one output stream of GL commands, we only need to call the head
         * SPU's MakeCurrent() function once.
         * BUT, if we're rendering to multiple windows, we do have to issue
         * MakeCurrent() calls sometimes.  The same GL context will always be
         * used though.
         */
        cr_server.head_spu->dispatch_table.MakeCurrent( mural->spuWindow,
                                                        nativeWindow,
                                                        ctxInfo->SpuContext >= 0
                                                            ? ctxInfo->SpuContext
                                                              : cr_server.MainContextInfo.SpuContext);
        cr_server.firstCallMakeCurrent = GL_FALSE;
        cr_server.currentCtxInfo = ctxInfo;
        cr_server.currentWindow = window;
        cr_server.currentNativeWindow = nativeWindow;
    }

    /* This used to be earlier, after crStateUpdateColorBits() call */
    crStateMakeCurrent( ctx );

    crStateSwichPostprocess(oldCtx, cr_server.bUseMultipleContexts, mural->bUseFBO && crServerSupportRedirMuralFBO() ? mural->idFBO : 0);

    if (!ctx->framebufferobject.drawFB
            && (ctx->buffer.drawBuffer == GL_FRONT || ctx->buffer.drawBuffer == GL_FRONT_LEFT))
        cr_server.curClient->currentMural->bFbDraw = GL_TRUE;

    if (!mural->bUseFBO)
    {
        ctx->buffer.width = mural->width;
        ctx->buffer.height = mural->height;
    }
    else
    {
        ctx->buffer.width = 0;
        ctx->buffer.height = 0;
    }
}

