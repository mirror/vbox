/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_mem.h"
#include "cr_spu.h"
#include "cr_error.h"
#include "cr_string.h"
#include "cr_url.h"
#include "renderspu.h"
#include <stdio.h>

#ifdef RT_OS_DARWIN
# include <iprt/semaphore.h>
#endif /* RT_OS_DARWIN */

static SPUNamedFunctionTable _cr_render_table[1000];

SPUFunctions render_functions = {
    NULL, /* CHILD COPY */
    NULL, /* DATA */
    _cr_render_table /* THE ACTUAL FUNCTIONS */
};

RenderSPU render_spu;
unsigned int render_spu_parent_window_id;

#ifdef CHROMIUM_THREADSAFE
CRtsd _RenderTSD;
#endif

static void swapsyncConnect(void)
{
    char hostname[4096], protocol[4096];
    unsigned short port;

    crNetInit(NULL, NULL);

    if (!crParseURL( render_spu.swap_master_url, protocol, hostname,
                    &port, 9876))
        crError( "Bad URL: %s", render_spu.swap_master_url );

    if (render_spu.is_swap_master)
    {
        int a;

        render_spu.swap_conns = (CRConnection **)crAlloc(
                        render_spu.num_swap_clients*sizeof(CRConnection *));
        for (a=0; a<render_spu.num_swap_clients; a++)
        {
            render_spu.swap_conns[a] = crNetAcceptClient( protocol, hostname, port,
                                                        render_spu.swap_mtu, 1);
        }
    }
    else
    {
        render_spu.swap_conns = (CRConnection **)crAlloc(sizeof(CRConnection *));

        render_spu.swap_conns[0] = crNetConnectToServer(render_spu.swap_master_url,
                                    port, render_spu.swap_mtu, 1);
        if (!render_spu.swap_conns[0])
            crError("Failed connection");
    }
}

#ifdef RT_OS_WINDOWS
static DWORD WINAPI renderSPUWindowThreadProc(void* unused)
{
    MSG msg;
    bool bRet;

    (void) unused;

    /* Force system to create the message queue.
     * Else, there's a chance that render spu will issue PostThreadMessage
     * before this thread calls GetMessage for first time.
     */
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    crDebug("RenderSPU: Window thread started (%x)", crThreadID());
    SetEvent(render_spu.hWinThreadReadyEvent);

    while( (bRet = GetMessage( &msg, 0, 0, 0 )) != 0)
    {
        if (bRet == -1)
        {
            crError("RenderSPU: Window thread GetMessage failed (%x)", GetLastError());
            break;
        }
        else
        {
            if (msg.message == WM_VBOX_RENDERSPU_CREATE_WINDOW)
            {
                LPCREATESTRUCT pCS = (LPCREATESTRUCT) msg.lParam;
                HWND *phWnd;

                CRASSERT(msg.lParam && !msg.wParam && pCS->lpCreateParams);

                phWnd = pCS->lpCreateParams;

                *phWnd = CreateWindowEx(pCS->dwExStyle, pCS->lpszName, pCS->lpszClass, pCS->style,
                                        pCS->x, pCS->y, pCS->cx, pCS->cy,
                                        pCS->hwndParent, pCS->hMenu, pCS->hInstance, &render_spu);

                SetEvent(render_spu.hWinThreadReadyEvent);
            }
            else if (msg.message == WM_VBOX_RENDERSPU_DESTROY_WINDOW)
            {
                CRASSERT(msg.lParam && !msg.wParam);

                DestroyWindow(((VBOX_RENDERSPU_DESTROY_WINDOW*) msg.lParam)->hWnd);

                SetEvent(render_spu.hWinThreadReadyEvent);
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    render_spu.dwWinThreadId = 0;

    crDebug("RenderSPU: Window thread stopped (%x)", crThreadID());
    SetEvent(render_spu.hWinThreadReadyEvent);

    return 0;
}
#endif

static SPUFunctions *
renderSPUInit( int id, SPU *child, SPU *self,
               unsigned int context_id, unsigned int num_contexts )
{
    int numFuncs, numSpecial;
    GLint defaultWin, defaultCtx;
    WindowInfo *windowInfo;

    (void) child;
    (void) context_id;
    (void) num_contexts;

    self->privatePtr = (void *) &render_spu;

#ifdef CHROMIUM_THREADSAFE
    crDebug("Render SPU: thread-safe");
#endif

    crMemZero(&render_spu, sizeof(render_spu));

    render_spu.id = id;
    renderspuSetVBoxConfiguration(&render_spu);

    if (render_spu.swap_master_url)
        swapsyncConnect();


    /* Get our special functions. */
    numSpecial = renderspuCreateFunctions( _cr_render_table );

#ifdef RT_OS_WINDOWS
    /* Start thread to create windows and process window messages */
    crDebug("RenderSPU: Starting windows serving thread");
    render_spu.hWinThreadReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!render_spu.hWinThreadReadyEvent)
    {
        crError("RenderSPU: Failed to create WinThreadReadyEvent! (%x)", GetLastError());
        return NULL;
    }

    if (!CreateThread(NULL, 0, renderSPUWindowThreadProc, 0, 0, &render_spu.dwWinThreadId))
    {
        crError("RenderSPU: Failed to start windows thread! (%x)", GetLastError());
        return NULL;
    }
    WaitForSingleObject(render_spu.hWinThreadReadyEvent, INFINITE);
#endif

    /* Get the OpenGL functions. */
    numFuncs = crLoadOpenGL( &render_spu.ws, _cr_render_table + numSpecial );
    if (numFuncs == 0) {
        crError("The render SPU was unable to load the native OpenGL library");
        return NULL;
    }

    numFuncs += numSpecial;

#ifdef GLX
    if (!render_spu.use_glxchoosevisual) {
        /* sometimes want to set this option with ATI drivers */
        render_spu.ws.glXChooseVisual = NULL;
    }
#endif

    render_spu.window_id = 0;
    render_spu.context_id = 0;
    render_spu.contextTable = crAllocHashtable();
    render_spu.windowTable = crAllocHashtable();

    CRASSERT(render_spu.default_visual & CR_RGB_BIT);

#ifdef USE_OSMESA
    if (render_spu.use_osmesa) {
        if (!crLoadOSMesa(&render_spu.OSMesaCreateContext,
                          &render_spu.OSMesaMakeCurrent,
                          &render_spu.OSMesaDestroyContext)) {
            crError("Unable to load OSMesa library");
        }
    }
#endif

#ifdef DARWIN
# ifndef __LP64__ /** @todo port to 64-bit darwin. */
    render_spu.hRootVisibleRegion = 0;
    render_spu.currentBufferName = 1;
    render_spu.uiDockUpdateTS = 0;
    /* Create a mutex for syncronizing events from the main Qt thread & this
       thread */
    RTSemFastMutexCreate(&render_spu.syncMutex);
    /* Create our window groups */
    CreateWindowGroup(kWindowGroupAttrMoveTogether | kWindowGroupAttrLayerTogether | kWindowGroupAttrSharedActivation | kWindowGroupAttrHideOnCollapse | kWindowGroupAttrFixedLevel, &render_spu.pMasterGroup);
    CreateWindowGroup(kWindowGroupAttrMoveTogether | kWindowGroupAttrLayerTogether | kWindowGroupAttrSharedActivation | kWindowGroupAttrHideOnCollapse | kWindowGroupAttrFixedLevel, &render_spu.pParentGroup);
    /* Make the correct z-layering */
    SendWindowGroupBehind (render_spu.pParentGroup, render_spu.pMasterGroup);
    /* and set the gParentGroup as parent for gMasterGroup. */
    SetWindowGroupParent (render_spu.pMasterGroup, render_spu.pParentGroup);
    /* Install the event handlers */
    EventTypeSpec eventList[] =
    {
        {kEventClassVBox, kEventVBoxUpdateContext}, /* Update the context after show/size/move events */
        {kEventClassVBox, kEventVBoxBoundsChanged}  /* Clip/Pos the OpenGL windows when the main window is changed in pos/size */
    };
    /* We need to process events from our main window */
    render_spu.hParentEventHandler = NewEventHandlerUPP(windowEvtHndlr);
    InstallApplicationEventHandler (render_spu.hParentEventHandler,
                                    GetEventTypeCount(eventList), eventList,
                                    NULL, NULL);
    render_spu.fInit = true;
# endif /* !__LP64__ */
#endif /* DARWIN */

    /*
     * Create the default window and context.  Their indexes are zero and
     * a client can use them without calling CreateContext or WindowCreate.
     */
    crDebug("Render SPU: Creating default window (visBits=0x%x, id=0)",
            render_spu.default_visual);
    defaultWin = renderspuWindowCreate( NULL, render_spu.default_visual );
    if (defaultWin != 0) {
        crError("Render SPU: Couldn't get a double-buffered, RGB visual with Z!");
        return NULL;
    }
    crDebug( "Render SPU: WindowCreate returned %d (0=normal)", defaultWin );

    crDebug("Render SPU: Creating default context, visBits=0x%x",
            render_spu.default_visual );
    defaultCtx = renderspuCreateContext( NULL, render_spu.default_visual, 0 );
    CRASSERT(defaultCtx == 0);

    renderspuMakeCurrent( defaultWin, 0, defaultCtx );

    /* Get windowInfo for the default window */
    windowInfo = (WindowInfo *) crHashtableSearch(render_spu.windowTable, 0);
    CRASSERT(windowInfo);
    windowInfo->mapPending = GL_TRUE;

    /*
     * Get the OpenGL extension functions.
     * SIGH -- we have to wait until the very bitter end to load the
     * extensions, because the context has to be bound before
     * wglGetProcAddress will work correctly.  No such issue with GLX though.
     */
    numFuncs += crLoadOpenGLExtensions( &render_spu.ws, _cr_render_table + numFuncs );
    CRASSERT(numFuncs < 1000);

#ifdef WINDOWS
    /*
     * Same problem as above, these are extensions so we need to
     * load them after a context has been bound. As they're WGL
     * extensions too, we can't simply tag them into the spu_loader.
     * So we do them here for now.
     * Grrr, NVIDIA driver uses EXT for GetExtensionsStringEXT,
     * but ARB for others. Need furthur testing here....
     */
    render_spu.ws.wglGetExtensionsStringEXT =
        (wglGetExtensionsStringEXTFunc_t)
        render_spu.ws.wglGetProcAddress( "wglGetExtensionsStringEXT" );
    render_spu.ws.wglChoosePixelFormatEXT =
        (wglChoosePixelFormatEXTFunc_t)
        render_spu.ws.wglGetProcAddress( "wglChoosePixelFormatARB" );
    render_spu.ws.wglGetPixelFormatAttribivEXT =
        (wglGetPixelFormatAttribivEXTFunc_t)
        render_spu.ws.wglGetProcAddress( "wglGetPixelFormatAttribivARB" );
    render_spu.ws.wglGetPixelFormatAttribfvEXT =
        (wglGetPixelFormatAttribfvEXTFunc_t)
        render_spu.ws.wglGetProcAddress( "wglGetPixelFormatAttribfvARB" );

    if (render_spu.ws.wglGetProcAddress("glTexImage3D"))
    {
        _cr_render_table[numFuncs].name = crStrdup("TexImage3D");
        _cr_render_table[numFuncs].fn = (SPUGenericFunction) render_spu.ws.wglGetProcAddress("glTexImage3D");
        ++numFuncs;
        crDebug("Render SPU: Found glTexImage3D function");
    }

    if (render_spu.ws.wglGetExtensionsStringEXT) {
        crDebug("WGL - found wglGetExtensionsStringEXT\n");
    }
    if (render_spu.ws.wglChoosePixelFormatEXT) {
        crDebug("WGL - found wglChoosePixelFormatEXT\n");
    }
#endif

    render_spu.barrierHash = crAllocHashtable();

    render_spu.cursorX = 0;
    render_spu.cursorY = 0;
    render_spu.use_L2 = 0;

    render_spu.gather_conns = NULL;

    crDebug("Render SPU: ---------- End of Init -------------");

    return &render_functions;
}


static void renderSPUSelfDispatch(SPUDispatchTable *self)
{
    crSPUInitDispatchTable( &(render_spu.self) );
    crSPUCopyDispatchTable( &(render_spu.self), self );

    render_spu.server = (CRServer *)(self->server);
}


static void DeleteContextCallback( void *data )
{
    ContextInfo *context = (ContextInfo *) data;
    renderspu_SystemDestroyContext(context);
    crFree(context);
}

static void DeleteWindowCallback( void *data )
{
    WindowInfo *window = (WindowInfo *) data;
    renderspu_SystemDestroyWindow(window);
    crFree(window);
}

static int renderSPUCleanup(void)
{
    crFreeHashtable(render_spu.contextTable, DeleteContextCallback);
    render_spu.contextTable = NULL;
    crFreeHashtable(render_spu.windowTable, DeleteWindowCallback);
    render_spu.windowTable = NULL;
    crFreeHashtable(render_spu.barrierHash, crFree);
    render_spu.barrierHash = NULL;

#ifdef RT_OS_DARWIN
# ifndef __LP64__ /** @todo port to 64-bit darwin. */
    render_spu.fInit = false;
    DisposeEventHandlerUPP(render_spu.hParentEventHandler);
    ReleaseWindowGroup(render_spu.pMasterGroup);
    ReleaseWindowGroup(render_spu.pParentGroup);
    if (render_spu.hRootVisibleRegion)
    {
        DisposeRgn(render_spu.hRootVisibleRegion);
        render_spu.hRootVisibleRegion = 0;
    }
    render_spu.currentBufferName = 1;
    render_spu.uiDockUpdateTS = 0;
    RTSemFastMutexDestroy(render_spu.syncMutex);
# endif /* __LP64__ */
#endif /* RT_OS_DARWIN */

#ifdef RT_OS_WINDOWS
    if (render_spu.dwWinThreadId)
    {
        PostThreadMessage(render_spu.dwWinThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(render_spu.hWinThreadReadyEvent, INFINITE);
    }
    CloseHandle(render_spu.hWinThreadReadyEvent);
    render_spu.hWinThreadReadyEvent = NULL;
#endif

    crUnloadOpenGL();

    return 1;
}


extern SPUOptions renderSPUOptions[];

int SPULoad( char **name, char **super, SPUInitFuncPtr *init,
         SPUSelfDispatchFuncPtr *self, SPUCleanupFuncPtr *cleanup,
         SPUOptionsPtr *options, int *flags )
{
    *name = "render";
    *super = NULL;
    *init = renderSPUInit;
    *self = renderSPUSelfDispatch;
    *cleanup = renderSPUCleanup;
    *options = renderSPUOptions;
    *flags = (SPU_NO_PACKER|SPU_IS_TERMINAL|SPU_MAX_SERVERS_ZERO);

    return 1;
}

DECLEXPORT(void) renderspuSetWindowId(unsigned int winId)
{
    render_spu_parent_window_id = winId;
}

static void renderspuWindowVisibleRegionCB(unsigned long key, void *data1, void *data2)
{
    WindowInfo *window = (WindowInfo *) data1;
    CRASSERT(window);

    renderspu_SystemWindowApplyVisibleRegion(window);
}

DECLEXPORT(void) renderspuSetRootVisibleRegion(GLint cRects, GLint *pRects)
{
#ifdef RT_OS_DARWIN
    renderspu_SystemSetRootVisibleRegion(cRects, pRects);

    crHashtableWalk(render_spu.windowTable, renderspuWindowVisibleRegionCB, NULL);
#endif
}

#ifndef RT_OS_DARWIN
void renderspu_SystemWindowApplyVisibleRegion(WindowInfo *window)
{
}
#endif
