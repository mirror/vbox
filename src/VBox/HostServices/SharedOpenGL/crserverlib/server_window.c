/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "server.h"
#include "server_dispatch.h"
#include "cr_mem.h"
#include "cr_rand.h"
#include "cr_string.h"

#include "render/renderspu.h"

static GLboolean crServerWindowCalcIsVisible(CRMuralInfo *pMural)
{
    uint32_t cRegions;
    int rc;
    if (!pMural->width || !pMural->height)
        return GL_FALSE;

    if (!pMural->bVisible || !(pMural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY))
        return GL_FALSE;

    rc = CrVrScrCompositorRegionsGet(pMural->fRootVrOn ? &pMural->RootVrCompositor : &pMural->Compositor, &cRegions, NULL, NULL, NULL);
    if (RT_FAILURE(rc))
    {
        crWarning("CrVrScrCompositorRegionsGet failed, rc %d", rc);
        return GL_FALSE;
    }

    if (!cRegions)
        return GL_FALSE;

    return GL_TRUE;
}

void crServerWindowSetIsVisible(CRMuralInfo *pMural, GLboolean fIsVisible)
{
    if (!fIsVisible == !pMural->fIsVisible)
        return;

    pMural->fIsVisible = fIsVisible;

    CRASSERT(pMural->screenId < cr_server.screenCount);

    if (fIsVisible)
    {
        ++cr_server.aWinVisibilityInfos[pMural->screenId].cVisibleWindows;
        cr_server.aWinVisibilityInfos[pMural->screenId].fVisibleChanged = 1;
    }
    else
    {
        --cr_server.aWinVisibilityInfos[pMural->screenId].cVisibleWindows;
        CRASSERT(cr_server.aWinVisibilityInfos[pMural->screenId].cVisibleWindows < UINT32_MAX/2);
        if (!cr_server.aWinVisibilityInfos[pMural->screenId].cVisibleWindows)
            cr_server.aWinVisibilityInfos[pMural->screenId].fVisibleChanged = 0;
    }

    crVBoxServerCheckVisibilityEvent(pMural->screenId);
}

void crServerWindowCheckIsVisible(CRMuralInfo *pMural)
{
    GLboolean fIsVisible = crServerWindowCalcIsVisible(pMural);

    crServerWindowSetIsVisible(pMural, fIsVisible);
}

void crServerWindowSize(CRMuralInfo *pMural)
{
    cr_server.head_spu->dispatch_table.WindowSize(pMural->spuWindow, pMural->width, pMural->height);

    crServerWindowCheckIsVisible(pMural);
}

void crServerWindowShow(CRMuralInfo *pMural)
{
    cr_server.head_spu->dispatch_table.WindowShow(pMural->spuWindow,
            !!(pMural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY) && pMural->bVisible);

    crServerWindowCheckIsVisible(pMural);
}

void crServerWindowVisibleRegion(CRMuralInfo *pMural)
{
    uint32_t cRects;
    const RTRECT *pRects;
    int rc = CrVrScrCompositorRegionsGet(pMural->fRootVrOn ? &pMural->RootVrCompositor : &pMural->Compositor, &cRects, NULL, &pRects, NULL);
    if (RT_SUCCESS(rc))
    {
        cr_server.head_spu->dispatch_table.WindowVisibleRegion(pMural->spuWindow, cRects, (const GLint*)pRects);

        crServerWindowCheckIsVisible(pMural);
    }
    else
        crWarning("CrVrScrCompositorRegionsGet failed rc %d", rc);

}

void crServerWindowReparent(CRMuralInfo *pMural)
{
    crServerVBoxCompositionDisableEnter(pMural);

    pMural->fHasParentWindow = !!cr_server.screen[pMural->screenId].winID;

    renderspuReparentWindow(pMural->spuWindow);

    crServerVBoxCompositionDisableLeave(pMural, GL_FALSE);
}

GLint SERVER_DISPATCH_APIENTRY
crServerDispatchWindowCreate(const char *dpyName, GLint visBits)
{
    return crServerDispatchWindowCreateEx(dpyName, visBits, -1);
}

static DECLCALLBACK(void) crServerMuralDefaultEntryReleasedCB(const struct VBOXVR_SCR_COMPOSITOR *pCompositor, struct VBOXVR_SCR_COMPOSITOR_ENTRY *pEntry, struct VBOXVR_SCR_COMPOSITOR_ENTRY *pReplacingEntry)
{
    CR_DISPLAY_ENTRY *pDEntry = CR_DENTRY_FROM_CENTRY(pEntry);
    CrDpEntryCleanup(pDEntry);
}

GLint crServerMuralInit(CRMuralInfo *mural, const char *dpyName, GLint visBits, GLint preloadWinID, GLboolean fUseDefaultDEntry)
{
    CRMuralInfo *defaultMural;
    GLint dims[2];
    GLint windowID = -1;
    GLint spuWindow;
    int rc;

    crMemset(mural, 0, sizeof (*mural));

    CrVrScrCompositorInit(&mural->Compositor);

    if (cr_server.fRootVrOn)
    {
        CrVrScrCompositorInit(&mural->RootVrCompositor);
    }

    /*
     * Have first SPU make a new window.
     */
    spuWindow = cr_server.head_spu->dispatch_table.WindowCreate( dpyName, visBits );
    if (spuWindow < 0) {
        CrVrScrCompositorClear(&mural->Compositor);
        if (cr_server.fRootVrOn)
            CrVrScrCompositorClear(&mural->RootVrCompositor);
        return spuWindow;
    }

    /* get initial window size */
    cr_server.head_spu->dispatch_table.GetChromiumParametervCR(GL_WINDOW_SIZE_CR, spuWindow, GL_INT, 2, dims);

    mural->fUseDefaultDEntry = fUseDefaultDEntry;

    if (fUseDefaultDEntry)
    {
        VBOXVR_TEXTURE Tex = {0};
        Tex.width = dims[0];
        Tex.height = dims[1];
        Tex.target = GL_TEXTURE_2D;
        Tex.hwid = 0;

        CrDpEntryInit(&mural->DefaultDEntry, &Tex, 0, crServerMuralDefaultEntryReleasedCB);

        mural->fRootVrOn = cr_server.fRootVrOn;
    }

    defaultMural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, 0);
    CRASSERT(defaultMural);
    mural->gX = 0;
    mural->gY = 0;
    mural->width = dims[0];
    mural->height = dims[1];
    
    mural->spuWindow = spuWindow;
    mural->screenId = 0;
    mural->fHasParentWindow = !!cr_server.screen[0].winID;
    mural->bVisible = !cr_server.bWindowsInitiallyHidden;
    mural->fPresentMode = CR_SERVER_REDIR_F_NONE;

    mural->cVisibleRects = 0;
    mural->pVisibleRects = NULL;
    mural->bReceivedRects = GL_FALSE;

    /* generate ID for this new window/mural (special-case for file conns) */
    if (cr_server.curClient && cr_server.curClient->conn->type == CR_FILE)
        windowID = spuWindow;
    else
        windowID = preloadWinID<0 ? (GLint)crHashtableAllocKeys( cr_server.muralTable, 1 ) : preloadWinID;

    mural->CreateInfo.visualBits = visBits;
    mural->CreateInfo.externalID = windowID;
    mural->CreateInfo.pszDpyName = dpyName ? crStrdup(dpyName) : NULL;

    CR_STATE_SHAREDOBJ_USAGE_INIT(mural);

    if (fUseDefaultDEntry)
    {
        RTRECT Rect;
        Rect.xLeft = 0;
        Rect.xRight = mural->width;
        Rect.yTop = 0;
        Rect.yBottom = mural->height;
        rc = CrVrScrCompositorEntryRegionsSet(&mural->Compositor, &mural->DefaultDEntry.CEntry, NULL, 1, &Rect, false, NULL);
        if (!RT_SUCCESS(rc))
        {
            crWarning("CrVrScrCompositorEntryRegionsSet failed, rc %d", rc);
            return -1;
        }
    }

    if (mural->fRootVrOn)
    {
        uint32_t cRects;
        const RTRECT *pRects;
        int rc = crServerMuralSynchRootVr(mural, NULL);
        if (RT_SUCCESS(rc))
        {
            rc = CrVrScrCompositorRegionsGet(&mural->RootVrCompositor, &cRects, NULL, &pRects, NULL);
            if (RT_SUCCESS(rc))
            {
                if (cRects != 1
                        || pRects[0].xLeft != 0 || pRects[0].yTop != 0
                        || pRects[0].xRight != mural->width || pRects[0].yBottom != mural->height)
                {
                    /* do visible rects only if they differ from the default */
                    crServerWindowVisibleRegion(mural);
                }
            }
            else
            {
                crWarning("CrVrScrCompositorRegionsGet failed, rc %d", rc);
            }
        }
    }

    return windowID;
}

GLint
crServerDispatchWindowCreateEx(const char *dpyName, GLint visBits, GLint preloadWinID)
{
    CRMuralInfo *mural;
    GLint windowID = -1;

    dpyName = "";

    if (cr_server.sharedWindows) {
        int pos, j;

        /* find empty position in my (curclient) windowList */
        for (pos = 0; pos < CR_MAX_WINDOWS; pos++) {
            if (cr_server.curClient->windowList[pos] == 0) {
                break;
            }
        }
        if (pos == CR_MAX_WINDOWS) {
            crWarning("Too many windows in crserver!");
            return -1;
        }

        /* Look if any other client has a window for this slot */
        for (j = 0; j < cr_server.numClients; j++) {
            if (cr_server.clients[j]->windowList[pos] != 0) {
                /* use that client's window */
                windowID = cr_server.clients[j]->windowList[pos];
                cr_server.curClient->windowList[pos] = windowID;
                crServerReturnValue( &windowID, sizeof(windowID) ); /* real return value */
                crDebug("CRServer: client %p sharing window %d",
                                cr_server.curClient, windowID);
                return windowID;
            }
        }
    }


    /*
     * Create a new mural for the new window.
     */
    mural = (CRMuralInfo *) crCalloc(sizeof(CRMuralInfo));
    if (!mural)
    {
        crWarning("crCalloc failed!");
        return -1;
    }

    windowID = crServerMuralInit(mural, dpyName, visBits, preloadWinID, GL_TRUE);
    if (windowID < 0)
    {
        crWarning("crServerMuralInit failed!");
        crServerReturnValue( &windowID, sizeof(windowID) );
        crFree(mural);
        return windowID;
    }

    crHashtableAdd(cr_server.muralTable, windowID, mural);

    crDebug("CRServer: client %p created new window %d (SPU window %d)",
                    cr_server.curClient, windowID, mural->spuWindow);

    if (windowID != -1 && !cr_server.bIsInLoadingState) {
        int pos;
        for (pos = 0; pos < CR_MAX_WINDOWS; pos++) {
            if (cr_server.curClient->windowList[pos] == 0) {
                cr_server.curClient->windowList[pos] = windowID;
                break;
            }
        }
    }

    /* ensure we have a dummy mural created right away to avoid potential deadlocks on VM shutdown */
    crServerGetDummyMural(mural->CreateInfo.visualBits);

    crServerReturnValue( &windowID, sizeof(windowID) );
    return windowID;
}

static bool crServerVBoxTranslateIntersectRect(CRMuralInfo *mural, const RTRECT *pSrcRect, PRTRECT pDstRect)
{
    int32_t xLeft = RT_MAX(mural->gX, pSrcRect->xRight);
    int32_t yTop = RT_MAX(mural->gY, pSrcRect->yBottom);
    int32_t xRight = RT_MIN(mural->gX + mural->width, pSrcRect->xLeft);
    int32_t yBottom = RT_MIN(mural->gY + mural->height, pSrcRect->yTop);

    if (xLeft < xRight && yTop < yBottom)
    {
        pDstRect->xLeft = xLeft;
        pDstRect->yTop = yTop;
        pDstRect->xRight = xRight;
        pDstRect->yBottom = yBottom;
        return true;
    }

    return false;
}

static void crServerVBoxRootVrTranslateForMural(CRMuralInfo *mural)
{
    int32_t dx = cr_server.RootVrCurPoint.x - mural->gX;
    int32_t dy = cr_server.RootVrCurPoint.y - mural->gY;

    cr_server.RootVrCurPoint.x = mural->gX;
    cr_server.RootVrCurPoint.y = mural->gY;

    VBoxVrListTranslate(&cr_server.RootVr, dx, dy);
}

static int crServerRemoveClientWindow(CRClient *pClient, GLint window)
{
    int pos;

    for (pos = 0; pos < CR_MAX_WINDOWS; ++pos)
    {
        if (pClient->windowList[pos] == window)
        {
            pClient->windowList[pos] = 0;
            return true;
        }
    }

    return false;
}

void crServerMuralTerm(CRMuralInfo *mural)
{
    crServerRedirMuralFBO(mural, CR_SERVER_REDIR_F_NONE);
    crServerDeleteMuralFBO(mural);

    if (cr_server.currentMural == mural)
    {
        CRMuralInfo *dummyMural = crServerGetDummyMural(cr_server.MainContextInfo.CreateInfo.visualBits);
        /* reset the current context to some dummy values to ensure render spu does not switch to a default "0" context,
         * which might lead to muralFBO (offscreen rendering) gl entities being created in a scope of that context */
        cr_server.head_spu->dispatch_table.MakeCurrent(dummyMural->spuWindow, 0, cr_server.MainContextInfo.SpuContext);
        cr_server.currentWindow = -1;
        cr_server.currentMural = NULL;
    }
    else
    {
        CRASSERT(cr_server.currentWindow != mural->CreateInfo.externalID);
    }


    cr_server.head_spu->dispatch_table.WindowDestroy( mural->spuWindow );

    if (mural->pVisibleRects)
    {
        crFree(mural->pVisibleRects);
    }

    if (mural->CreateInfo.pszDpyName)
        crFree(mural->CreateInfo.pszDpyName);

    CrVrScrCompositorClear(&mural->Compositor);

    if (mural->fRootVrOn)
        CrVrScrCompositorClear(&mural->RootVrCompositor);
}

static void crServerCleanupCtxMuralRefsCB(unsigned long key, void *data1, void *data2)
{
    CRContextInfo *ctxInfo = (CRContextInfo *) data1;
    CRMuralInfo *mural = (CRMuralInfo *) data2;

    if (ctxInfo->currentMural == mural)
        ctxInfo->currentMural = NULL;
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchWindowDestroy( GLint window )
{
    CRMuralInfo *mural;
    int32_t client;
    CRClientNode *pNode;
    int found=false;

    if (!window)
    {
        crWarning("Unexpected attempt to delete default mural, ignored!");
        return;
    }

    mural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, window);
    if (!mural) {
         crWarning("CRServer: invalid window %d passed to WindowDestroy()", window);
         return;
    }

    crDebug("CRServer: Destroying window %d (spu window %d)", window, mural->spuWindow);

    crHashtableWalk(cr_server.contextTable, crServerCleanupCtxMuralRefsCB, mural);

    crServerMuralTerm(mural);

    CRASSERT(cr_server.currentWindow != window);

    if (cr_server.curClient)
    {
        if (cr_server.curClient->currentMural == mural)
        {
            cr_server.curClient->currentMural = NULL;
            cr_server.curClient->currentWindow = -1;
        }

        found = crServerRemoveClientWindow(cr_server.curClient, window);

        /*Same as with contexts, some apps destroy it not in a thread where it was created*/
        if (!found)
        {
            for (client=0; client<cr_server.numClients; ++client)
            {
                if (cr_server.clients[client]==cr_server.curClient)
                    continue;

                found = crServerRemoveClientWindow(cr_server.clients[client], window);

                if (found) break;
            }
        }

        if (!found)
        {
            pNode=cr_server.pCleanupClient;

            while (pNode && !found)
            {
                found = crServerRemoveClientWindow(pNode->pClient, window);
                pNode = pNode->next;
            }
        }

        CRASSERT(found);
    }

    /*Make sure this window isn't active in other clients*/
    for (client=0; client<cr_server.numClients; ++client)
    {
        if (cr_server.clients[client]->currentMural == mural)
        {
            cr_server.clients[client]->currentMural = NULL;
            cr_server.clients[client]->currentWindow = -1;
        }
    }

    pNode=cr_server.pCleanupClient;
    while (pNode)
    {
        if (pNode->pClient->currentMural == mural)
        {
            pNode->pClient->currentMural = NULL;
            pNode->pClient->currentWindow = -1;
        }
        pNode = pNode->next;
    }

    crHashtableDelete(cr_server.muralTable, window, crFree);
}

static DECLCALLBACK(VBOXVR_SCR_COMPOSITOR_ENTRY*) crServerMuralGetRootVrCEntry(VBOXVR_SCR_COMPOSITOR_ENTRY*pEntry, void *pvContext)
{
    CR_DISPLAY_ENTRY *pDEntry = CR_DENTRY_FROM_CENTRY(pEntry);
    Assert(!CrVrScrCompositorEntryIsUsed(&pDEntry->RootVrCEntry));
    CrVrScrCompositorEntryInit(&pDEntry->RootVrCEntry, CrVrScrCompositorEntryTexGet(pEntry), NULL);
    CrVrScrCompositorEntryFlagsSet(&pDEntry->RootVrCEntry, CrVrScrCompositorEntryFlagsGet(pEntry));
    return &pDEntry->RootVrCEntry;
}

int crServerMuralSynchRootVr(CRMuralInfo *mural, bool *pfChanged)
{
    int rc;

    crServerVBoxRootVrTranslateForMural(mural);

    /* ensure the rootvr compositor does not hold any data,
     * i.e. cleanup all rootvr entries data */
    CrVrScrCompositorClear(&mural->RootVrCompositor);

    rc = CrVrScrCompositorIntersectedList(&mural->Compositor, &cr_server.RootVr, &mural->RootVrCompositor, crServerMuralGetRootVrCEntry, NULL, pfChanged);
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrVrScrCompositorIntersectedList failed, rc %d", rc);
        return rc;
    }

    return VINF_SUCCESS;
}

GLboolean crServerMuralSize(CRMuralInfo *mural, GLint width, GLint height)
{
    RTRECT Rect;
    VBOXVR_TEXTURE Tex;
    int rc = VINF_SUCCESS;
    Tex.width = width;
    Tex.height = height;
    Tex.target = GL_TEXTURE_2D;
    Tex.hwid = 0;

    if (mural->width == width && mural->height == height)
        return GL_FALSE;


    /* since we're going to change the current compositor & the window we need to avoid
     * renderspu fron dealing with inconsistent data, i.e. modified compositor and
     * still unmodified window.
     * So what we do is:
     * 1. tell renderspu to stop using the current compositor -> renderspu would do necessary synchronization with its redraw thread to ensure compositor is no longer used
     * 2. do necessary modifications
     * 3. (so far not needed for resize, but in case it is in the future) re-set the compositor */

    /* 1. tell renderspu to stop using the current compositor (see above comment) */
    crServerVBoxCompositionDisableEnter(mural);

    /* 2. do necessary modifications (see above comment) */
    /* NOTE: we can do it even if mural->fPresentMode == CR_SERVER_REDIR_F_NONE to make sure the compositor data is always up to date */
    /* the compositor lock is not needed actually since we have prevented renderspu from using the compositor */
    /* CrVrScrCompositorLock(&mural->Compositor); */
    if (mural->fUseDefaultDEntry)
    {
        if (!mural->bReceivedRects)
        {
            rc = CrVrScrCompositorEntryRemove(&mural->Compositor, &mural->DefaultDEntry.CEntry);
            if (!RT_SUCCESS(rc))
            {
                crWarning("CrVrScrCompositorEntryRemove failed, rc %d", rc);
                goto end;
            }
            CrVrScrCompositorEntryInit(&mural->DefaultDEntry.CEntry, &Tex, NULL);
            /* initially set regions to all visible since this is what some guest assume
             * and will not post any more visible regions command */
            Rect.xLeft = 0;
            Rect.xRight = width;
            Rect.yTop = 0;
            Rect.yBottom = height;
            rc = CrVrScrCompositorEntryRegionsSet(&mural->Compositor, &mural->DefaultDEntry.CEntry, NULL, 1, &Rect, false, NULL);
            if (!RT_SUCCESS(rc))
            {
                crWarning("CrVrScrCompositorEntryRegionsSet failed, rc %d", rc);
                goto end;
            }
        }
        else
        {
            rc = CrVrScrCompositorEntryTexUpdate(&mural->Compositor, &mural->DefaultDEntry.CEntry, &Tex);
            if (!RT_SUCCESS(rc))
            {
                crWarning("CrVrScrCompositorEntryTexUpdate failed, rc %d", rc);
                goto end;
            }
        }
    }
    else
    {
        CrVrScrCompositorClear(&mural->Compositor);
    }

    /* CrVrScrCompositorUnlock(&mural->Compositor); */
    mural->width = width;
    mural->height = height;

    mural->fDataPresented = GL_FALSE;

    if (cr_server.curClient && cr_server.curClient->currentMural == mural)
    {
        crStateGetCurrent()->buffer.width = mural->width;
        crStateGetCurrent()->buffer.height = mural->height;
    }

    if (mural->fRootVrOn)
    {
        rc = crServerMuralSynchRootVr(mural, NULL);
        if (!RT_SUCCESS(rc))
        {
            crWarning("crServerMuralSynchRootVr failed, rc %d", rc);
            goto end;
        }
    }

    crServerCheckMuralGeometry(mural);

    crServerWindowSize(mural);

    crServerWindowVisibleRegion(mural);

    crServerDEntryAllResized(mural);
end:
    /* 3. (so far not needed for resize, but in case it is in the future) re-set the compositor (see above comment) */
    /* uncomment when needed */
    /* NOTE: !!! we have mural->fHasPresentationData set to GL_FALSE above, so crServerVBoxCompositionReenable will have no effect in any way

    */
    crServerVBoxCompositionDisableLeave(mural, GL_FALSE);

    return GL_TRUE;
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchWindowSize( GLint window, GLint width, GLint height )
{
    CRMuralInfo *mural;

    /*  crDebug("CRServer: Window %d size %d x %d", window, width, height);*/
    mural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, window);
    if (!mural) {
#if EXTRA_WARN
         crWarning("CRServer: invalid window %d passed to WindowSize()", window);
#endif
         return;
    }

    crServerMuralSize(mural, width, height);

    if (cr_server.currentMural == mural)
    {
        crServerPerformMakeCurrent( mural, cr_server.currentCtxInfo );
    }
}

void crServerMuralPosition(CRMuralInfo *mural, GLint x, GLint y, GLboolean fSkipCheckGeometry)
{
    GLboolean fForcePresent = GL_FALSE;
    /*  crDebug("CRServer: Window %d pos %d, %d", window, x, y);*/

//    if (mural->gX != x || mural->gY != y)
    {
        /* since we're going to change the current compositor & the window we need to avoid
         * renderspu fron dealing with inconsistent data, i.e. modified compositor and
         * still unmodified window.
         * So what we do is:
         * 1. tell renderspu to stop using the current compositor -> renderspu would do necessary synchronization with its redraw thread to ensure compositor is no longer used
         * 2. do necessary modifications
         * 3. re-set the compositor */

        /* 1. tell renderspu to stop using the current compositor (see above comment) */
        crServerVBoxCompositionDisableEnter(mural);

        /* 2. do necessary modifications (see above comment) */
        /* NOTE: we can do it even if !(mural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY) to make sure the compositor data is always up to date */

        if (mural->gX != x || mural->gY != y)
        {
            if (mural->fRootVrOn)
            {
                fForcePresent = crServerVBoxCompositionPresentNeeded(mural);
            }

            mural->gX = x;
            mural->gY = y;

            /* no need to set position because the position is relative to window */
            /*CrVrScrCompositorEntryPosSet(&mural->Compositor, &mural->CEntry, &Pos);*/

            if (mural->fRootVrOn)
            {
                int rc = crServerMuralSynchRootVr(mural, NULL);
                if (RT_SUCCESS(rc))
                {
                    crServerWindowVisibleRegion(mural);
                }
                else
                {
                    crWarning("crServerMuralSynchRootVr failed, rc %d", rc);
                }
            }
        }

        if (!fSkipCheckGeometry)
            crServerCheckMuralGeometry(mural);

        crServerDEntryAllMoved(mural);

        /* 3. re-set the compositor (see above comment) */
        crServerVBoxCompositionDisableLeave(mural, fForcePresent);
    }
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchWindowPosition( GLint window, GLint x, GLint y )
{
    CRMuralInfo *mural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, window);
    if (!mural) {
#if EXTRA_WARN
         crWarning("CRServer: invalid window %d passed to WindowPosition()", window);
#endif
         return;
    }
    crServerMuralPosition(mural, x, y, GL_FALSE);
}

void crServerMuralVisibleRegion( CRMuralInfo *mural, GLint cRects, const GLint *pRects )
{
    GLboolean fForcePresent = crServerVBoxCompositionPresentNeeded(mural);
    bool fRegionsChanged = false;
    int rc = VINF_SUCCESS;

    /* since we're going to change the current compositor & the window we need to avoid
     * renderspu fron dealing with inconsistent data, i.e. modified compositor and
     * still unmodified window.
     * So what we do is:
     * 1. tell renderspu to stop using the current compositor -> renderspu would do necessary synchronization with its redraw thread to ensure compositor is no longer used
     * 2. do necessary modifications
     * 3. re-set the compositor */

    /* 1. tell renderspu to stop using the current compositor (see above comment) */
    crServerVBoxCompositionDisableEnter(mural);

    /* 2. do necessary modifications (see above comment) */
    if (mural->pVisibleRects)
    {
        crFree(mural->pVisibleRects);
        mural->pVisibleRects = NULL;
    }

    mural->cVisibleRects = cRects;
    mural->bReceivedRects = GL_TRUE;
    if (cRects)
    {
        mural->pVisibleRects = (GLint*) crAlloc(4*sizeof(GLint)*cRects);
        if (!mural->pVisibleRects)
        {
            crError("Out of memory in crServerDispatchWindowVisibleRegion");
        }
        crMemcpy(mural->pVisibleRects, pRects, 4*sizeof(GLint)*cRects);
    }

    Assert(mural->fUseDefaultDEntry);
    /* NOTE: we can do it even if !(mural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY) to make sure the compositor data is always up to date */
    /* the compositor lock is not needed actually since we have prevented renderspu from using the compositor */
    /* CrVrScrCompositorLock(&mural->Compositor); */
    rc = CrVrScrCompositorEntryRegionsSet(&mural->Compositor, &mural->DefaultDEntry.CEntry, NULL, cRects, (const RTRECT *)pRects, false, &fRegionsChanged);
    /*CrVrScrCompositorUnlock(&mural->Compositor);*/
    if (!RT_SUCCESS(rc))
    {
        crWarning("CrVrScrCompositorEntryRegionsSet failed, rc %d", rc);
        goto end;
    }

    if (fRegionsChanged)
    {
        if (mural->fRootVrOn)
        {
            rc = crServerMuralSynchRootVr(mural, NULL);
            if (!RT_SUCCESS(rc))
            {
                crWarning("crServerMuralSynchRootVr failed, rc %d", rc);
                goto end;
            }
        }

        crServerWindowVisibleRegion(mural);

        crServerDEntryAllVibleRegions(mural);
    }
end:
    /* 3. re-set the compositor (see above comment) */
    crServerVBoxCompositionDisableLeave(mural, fForcePresent);
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchWindowVisibleRegion( GLint window, GLint cRects, const GLint *pRects )
{
    CRMuralInfo *mural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, window);
    if (!mural) {
#if EXTRA_WARN
         crWarning("CRServer: invalid window %d passed to WindowVisibleRegion()", window);
#endif
         return;
    }

    crServerMuralVisibleRegion( mural, cRects, pRects );
}

void crServerMuralShow( CRMuralInfo *mural, GLint state )
{
    mural->bVisible = !!state;

    if (mural->fPresentMode & CR_SERVER_REDIR_F_DISPLAY)
        crServerWindowShow(mural);
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchWindowShow( GLint window, GLint state )
{
    CRMuralInfo *mural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, window);
    if (!mural) {
#if EXTRA_WARN
         crWarning("CRServer: invalid window %d passed to WindowShow()", window);
#endif
         return;
    }

    crServerMuralShow( mural, state );
}

GLint
crServerSPUWindowID(GLint serverWindow)
{
    CRMuralInfo *mural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, serverWindow);
    if (!mural) {
#if EXTRA_WARN
         crWarning("CRServer: invalid window %d passed to crServerSPUWindowID()",
                             serverWindow);
#endif
         return -1;
    }
    return mural->spuWindow;
}
