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

GLint SERVER_DISPATCH_APIENTRY
crServerDispatchWindowCreate(const char *dpyName, GLint visBits)
{
    return crServerDispatchWindowCreateEx(dpyName, visBits, -1);
}


GLint
crServerDispatchWindowCreateEx(const char *dpyName, GLint visBits, GLint preloadWinID)
{
    CRMuralInfo *mural;
    GLint windowID = -1;
    GLint spuWindow;
    GLint dims[2];
    CRCreateInfo_t *pCreateInfo;

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
     * Have first SPU make a new window.
     */
    spuWindow = cr_server.head_spu->dispatch_table.WindowCreate( dpyName, visBits );
    if (spuWindow < 0) {
        crServerReturnValue( &spuWindow, sizeof(spuWindow) );
        return spuWindow;
    }

    /* get initial window size */
    cr_server.head_spu->dispatch_table.GetChromiumParametervCR(GL_WINDOW_SIZE_CR, spuWindow, GL_INT, 2, dims);

    /*
     * Create a new mural for the new window.
     */
    mural = (CRMuralInfo *) crCalloc(sizeof(CRMuralInfo));
    if (mural) {
        CRMuralInfo *defaultMural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, 0);
        CRASSERT(defaultMural);
        mural->width = defaultMural->width;
        mural->height = defaultMural->height;
        mural->optimizeBucket = 0; /* might get enabled later */
        mural->numExtents = defaultMural->numExtents;
        mural->curExtent = 0;
        crMemcpy(mural->extents, defaultMural->extents,
                 defaultMural->numExtents * sizeof(CRExtent));
        mural->underlyingDisplay[0] = 0;
        mural->underlyingDisplay[1] = 0;
        mural->underlyingDisplay[2] = dims[0];
        mural->underlyingDisplay[3] = dims[1];

        mural->spuWindow = spuWindow;
        crServerInitializeTiling(mural);

        /* generate ID for this new window/mural (special-case for file conns) */
        if (cr_server.curClient && cr_server.curClient->conn->type == CR_FILE)
            windowID = spuWindow;
        else
            windowID = preloadWinID<0 ? crServerGenerateID(&cr_server.idsPool.freeWindowID) : preloadWinID;
        crHashtableAdd(cr_server.muralTable, windowID, mural);

        pCreateInfo = (CRCreateInfo_t *) crAlloc(sizeof(CRCreateInfo_t));
        pCreateInfo->pszDpyName = dpyName ? crStrdup(dpyName) : NULL;
        pCreateInfo->visualBits = visBits;
        crHashtableAdd(cr_server.pWindowCreateInfoTable, windowID, pCreateInfo);
    }

    crDebug("CRServer: client %p created new window %d (SPU window %d)",
                    cr_server.curClient, windowID, spuWindow);

    if (windowID != -1 && !cr_server.bIsInLoadingState) {
        int pos;
        for (pos = 0; pos < CR_MAX_WINDOWS; pos++) {
            if (cr_server.curClient->windowList[pos] == 0) {
                cr_server.curClient->windowList[pos] = windowID;
                break;
            }
        }
    }

    crServerReturnValue( &windowID, sizeof(windowID) );
    return windowID;
}

void crServerCheckCurrentCtxWindowCB(unsigned long key, void *data1, void *data2)
{
    CRContext *crCtx = (CRContext *) data1;
    GLint window = *(GLint*)data2;

    (void) key;
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchWindowDestroy( GLint window )
{
    CRMuralInfo *mural;
    int pos;

    mural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, window);
    if (!mural) {
         crWarning("CRServer: invalid window %d passed to WindowDestroy()", window);
         return;
    }

    crDebug("CRServer: Destroying window %d (spu window %d)", window, mural->spuWindow);
    cr_server.head_spu->dispatch_table.WindowDestroy( mural->spuWindow );

    if (cr_server.curClient)
    {
        for (pos = 0; pos < CR_MAX_WINDOWS; ++pos)
            if (cr_server.curClient->windowList[pos] == window)
            {
                cr_server.curClient->windowList[pos] = 0;
                break;
            }
        CRASSERT(pos<CR_MAX_WINDOWS);

        if (cr_server.curClient->currentMural == mural)
        {
            cr_server.curClient->currentMural = NULL;
            cr_server.curClient->currentWindow = -1;
        }
    }

    if (cr_server.currentWindow == window)
    {
        cr_server.currentWindow = -1;
    }

    crHashtableWalk(cr_server.contextTable, crServerCheckCurrentCtxWindowCB, &window);
    crHashtableDelete(cr_server.pWindowCreateInfoTable, window, crServerCreateInfoDeleteCB);
    crHashtableDelete(cr_server.muralTable, window, crFree);
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
    mural->underlyingDisplay[2] = width;
    mural->underlyingDisplay[3] = height;
    crServerInitializeTiling(mural);

    cr_server.head_spu->dispatch_table.WindowSize(mural->spuWindow, width, height);
}


void SERVER_DISPATCH_APIENTRY
crServerDispatchWindowPosition( GLint window, GLint x, GLint y )
{
  CRMuralInfo *mural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, window);
    /*  crDebug("CRServer: Window %d pos %d, %d", window, x, y);*/
    if (!mural) {
#if EXTRA_WARN
         crWarning("CRServer: invalid window %d passed to WindowPosition()", window);
#endif
         return;
    }
    mural->underlyingDisplay[0] = x;
    mural->underlyingDisplay[1] = y;

#if EXTRA_WARN /* don't believe this is needed */
    crServerInitializeTiling(mural);
#endif
    cr_server.head_spu->dispatch_table.WindowPosition(mural->spuWindow, x, y);
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchWindowVisibleRegion( GLint window, GLint cRects, GLint *pRects )
{
  CRMuralInfo *mural = (CRMuralInfo *) crHashtableSearch(cr_server.muralTable, window);
    if (!mural) {
#if EXTRA_WARN
         crWarning("CRServer: invalid window %d passed to WindowVisibleRegion()", window);
#endif
         return;
    }
#if EXTRA_WARN /* don't believe this is needed */
    crServerInitializeTiling(mural);
#endif
    cr_server.head_spu->dispatch_table.WindowVisibleRegion(mural->spuWindow, cRects, pRects);
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
    cr_server.head_spu->dispatch_table.WindowShow(mural->spuWindow, state);
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
