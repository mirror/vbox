/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_spu.h"
#include "cr_error.h" 
#include "cr_mem.h" 
#include "stub.h"

#ifdef GLX
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#endif

/**
 * Returns -1 on error
 */
GLint APIENTRY crCreateContext( const char *dpyName, GLint visBits )
{
    ContextInfo *context;
    stubInit();
    /* XXX in Chromium 1.5 and earlier, the last parameter was UNDECIDED.
     * That didn't seem right so it was changed to CHROMIUM. (Brian)
     */
    context = stubNewContext(dpyName, visBits, CHROMIUM, 0);
    return context ? (int) context->id : -1;
}

void APIENTRY crDestroyContext( GLint context )
{
    stubDestroyContext(context);
}

void APIENTRY crMakeCurrent( GLint window, GLint context )
{
    WindowInfo *winInfo = (WindowInfo *)
        crHashtableSearch(stub.windowTable, (unsigned int) window);
    ContextInfo *contextInfo = (ContextInfo *)
        crHashtableSearch(stub.contextTable, context);
    if (contextInfo && contextInfo->type == NATIVE) {
        crWarning("Can't call crMakeCurrent with native GL context");
        return;
    }

    stubMakeCurrent(winInfo, contextInfo);
}

GLint APIENTRY crGetCurrentContext( void )
{
    stubInit();
    if (stub.currentContext)
      return (GLint) stub.currentContext->id;
    else
      return 0;
}

GLint APIENTRY crGetCurrentWindow( void )
{
    stubInit();
    if (stub.currentContext && stub.currentContext->currentDrawable)
      return stub.currentContext->currentDrawable->spuWindow;
    else
      return -1;
}

void APIENTRY crSwapBuffers( GLint window, GLint flags )
{
    const WindowInfo *winInfo = (const WindowInfo *)
        crHashtableSearch(stub.windowTable, (unsigned int) window);
    if (winInfo)
        stubSwapBuffers(winInfo, flags);
}

/**
 * Returns -1 on error
 */
GLint APIENTRY crWindowCreate( const char *dpyName, GLint visBits )
{
    stubInit();
    return stubNewWindow( dpyName, visBits );
}

void APIENTRY crWindowDestroy( GLint window )
{
    WindowInfo *winInfo = (WindowInfo *)
        crHashtableSearch(stub.windowTable, (unsigned int) window);
    if (winInfo && winInfo->type == CHROMIUM && stub.spu) {
        stub.spu->dispatch_table.WindowDestroy( winInfo->spuWindow );
#ifdef WINDOWS
        if (winInfo->hVisibleRegion != INVALID_HANDLE_VALUE)
        {
            DeleteObject(winInfo->hVisibleRegion);
        }
#elif defined(GLX)
        if (winInfo->pVisibleRegions)
        {
            XFree(winInfo->pVisibleRegions);
        }
#endif
        stub.spu->dispatch_table.Flush();

        crHashtableDelete(stub.windowTable, window, crFree);
    }
}

void APIENTRY crWindowSize( GLint window, GLint w, GLint h )
{
    const WindowInfo *winInfo = (const WindowInfo *)
        crHashtableSearch(stub.windowTable, (unsigned int) window);
    if (winInfo && winInfo->type == CHROMIUM)
    {
        crDebug("Dispatched crWindowSize (%i)", window);
        stub.spu->dispatch_table.WindowSize( window, w, h );
    }
}

void APIENTRY crWindowPosition( GLint window, GLint x, GLint y )
{
    const WindowInfo *winInfo = (const WindowInfo *)
        crHashtableSearch(stub.windowTable, (unsigned int) window);
    if (winInfo && winInfo->type == CHROMIUM)
    {
        crDebug("Dispatched crWindowPosition (%i)", window);
        stub.spu->dispatch_table.WindowPosition( window, x, y );
    }
}

void APIENTRY crWindowVisibleRegion( GLint window, GLint cRects, void *pRects )
{
    const WindowInfo *winInfo = (const WindowInfo *)
        crHashtableSearch(stub.windowTable, (unsigned int) window);
    if (winInfo && winInfo->type == CHROMIUM)
    {
        crDebug("Dispatched crWindowVisibleRegion (%i, cRects=%i)", window, cRects);
        stub.spu->dispatch_table.WindowVisibleRegion( window, cRects, pRects );
    }
}

void APIENTRY crWindowShow( GLint window, GLint flag )
{
    WindowInfo *winInfo = (WindowInfo *)
        crHashtableSearch(stub.windowTable, (unsigned int) window);
    if (winInfo && winInfo->type == CHROMIUM)
        stub.spu->dispatch_table.WindowShow( window, flag );
    winInfo->mapped = flag ? GL_TRUE : GL_FALSE;
}

void APIENTRY stub_GetChromiumParametervCR( GLenum target, GLuint index, GLenum type, GLsizei count, GLvoid *values )
{
    char **ret;
    switch( target )
    {
        case GL_HEAD_SPU_NAME_CR:
            ret = (char **) values;
            *ret = stub.spu->name;
            return;
        default:
            stub.spu->dispatch_table.GetChromiumParametervCR( target, index, type, count, values );
            break;
    }
}

/*
 *  Updates geometry info for given spu window.
 *  Returns GL_TRUE if it changed since last call, GL_FALSE overwise.
 *  bForceUpdate - forces dispatching of geometry info even if it's unchanged
 */
GLboolean stubUpdateWindowGeometry(WindowInfo *pWindow, GLboolean bForceUpdate)
{
    int winX, winY;
    unsigned int winW, winH;
    GLboolean res = GL_FALSE;

    CRASSERT(pWindow);

    stubGetWindowGeometry(pWindow, &winX, &winY, &winW, &winH);

    /* @todo remove "if (winW && winH)"?*/
    if (winW && winH) {
        if (stub.trackWindowSize) {
            if (bForceUpdate || winW != pWindow->width || winH != pWindow->height) {
                crDebug("Dispatched WindowSize (%i)", pWindow->spuWindow);
                stub.spuDispatch.WindowSize(pWindow->spuWindow, winW, winH);
                pWindow->width = winW;
                pWindow->height = winH;
                res = GL_TRUE;
            }
        }
        if (stub.trackWindowPos) {
            if (bForceUpdate || winX != pWindow->x || winY != pWindow->y) {
                crDebug("Dispatched WindowPosition (%i)", pWindow->spuWindow);
                stub.spuDispatch.WindowPosition(pWindow->spuWindow, winX, winY);
                pWindow->x = winX;
                pWindow->y = winY;
                res = GL_TRUE;
            }
        }
    }

    return res;
}

#ifdef WINDOWS
/*
 *  Updates visible regions for given spu window.
 *  Returns GL_TRUE if regions changed since last call, GL_FALSE overwise.
 */
GLboolean stubUpdateWindowVisibileRegions(WindowInfo *pWindow)
{
    HRGN hVisRgn;
    HWND hwnd;
    DWORD dwCount;
    LPRGNDATA lpRgnData;
    POINT pt;
    int iret;

    if (!pWindow) return GL_FALSE;
    hwnd = WindowFromDC(pWindow->drawable);
    if (!hwnd) return GL_FALSE;

    
    hVisRgn = CreateRectRgn(0,0,0,0);
    iret = GetRandomRgn(pWindow->drawable, hVisRgn, SYSRGN);

    if (iret==1)
    {
        /*@todo check win95/win98 here, as rects should be already in client space there*/
        /* Convert screen related rectangles to client related rectangles */
        pt.x = 0;
        pt.y = 0;
        ScreenToClient(hwnd, &pt);
        OffsetRgn(hVisRgn, pt.x, pt.y);

        /*
        dwCount = GetRegionData(hVisRgn, 0, NULL);
        lpRgnData = crAlloc(dwCount);
        crDebug("GetRandomRgn returned 1, dwCount=%d", dwCount);
        GetRegionData(hVisRgn, dwCount, lpRgnData);
        crDebug("Region consists of %d rects", lpRgnData->rdh.nCount);

        pRects = (RECT*) lpRgnData->Buffer;
        for (i=0; i<lpRgnData->rdh.nCount; ++i)
        {
            crDebug("Rgn[%d] = (%d, %d, %d, %d)", i, pRects[i].left, pRects[i].top, pRects[i].right, pRects[i].bottom);
        }
        crFree(lpRgnData);
        */

        if (pWindow->hVisibleRegion==INVALID_HANDLE_VALUE 
            || !EqualRgn(pWindow->hVisibleRegion, hVisRgn))
        {
            DeleteObject(pWindow->hVisibleRegion);
            pWindow->hVisibleRegion = hVisRgn;

            dwCount = GetRegionData(hVisRgn, 0, NULL);
            lpRgnData = crAlloc(dwCount);

            if (lpRgnData)
            {
                GetRegionData(hVisRgn, dwCount, lpRgnData);
                crDebug("Dispatched WindowVisibleRegion (%i, cRects=%i)", pWindow->spuWindow, lpRgnData->rdh.nCount);
                stub.spuDispatch.WindowVisibleRegion(pWindow->spuWindow, lpRgnData->rdh.nCount, (GLint*) lpRgnData->Buffer);
                crFree(lpRgnData);
                return GL_TRUE;
            }
            else crWarning("GetRegionData failed, VisibleRegions update failed");
        }
        else
        {
            DeleteObject(hVisRgn);
        }
    }
    else 
    {
        crWarning("GetRandomRgn returned (%d) instead of (1), VisibleRegions update failed", iret);
        DeleteObject(hVisRgn);
    }

    return GL_FALSE;
}

static void stubCBCheckWindowsInfo(unsigned long key, void *data1, void *data2)
{
    WindowInfo *winInfo = (WindowInfo *) data1;
    CWPRETSTRUCT *pMsgInfo = (PCWPRETSTRUCT) data2;

    (void) key;

    if (winInfo && pMsgInfo && winInfo->type == CHROMIUM)
    {
        switch (pMsgInfo->message)
        {
            case WM_MOVING:
            case WM_SIZING:
            case WM_MOVE:
            case WM_CREATE:
            case WM_SIZE:
            {
                GLboolean changed = stub.trackWindowVisibleRgn && stubUpdateWindowVisibileRegions(winInfo);

                if (stubUpdateWindowGeometry(winInfo, GL_FALSE) || changed)
                {
                    stub.spuDispatch.Flush();
                }
                break;
            }

            case WM_SHOWWINDOW:
            case WM_ACTIVATEAPP:
            case WM_PAINT:
            case WM_NCPAINT:
            case WM_NCACTIVATE:
            case WM_ERASEBKGND:
            {
                if (stub.trackWindowVisibleRgn && stubUpdateWindowVisibileRegions(winInfo))
                {
                    stub.spuDispatch.Flush();
                }
                break;
            }

            default:
            {
                if (stub.trackWindowVisibleRgn && stubUpdateWindowVisibileRegions(winInfo))
                {
                    crDebug("Visibility info updated due to unknown hooked message (%d)", pMsgInfo->message);
                    stub.spuDispatch.Flush();
                }
                break;
            }
        }
    }
}

LRESULT CALLBACK stubCBWindowMessageHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    CWPRETSTRUCT *pMsgInfo = (PCWPRETSTRUCT) lParam;

    if (nCode>=0 && pMsgInfo)
    {
#ifdef CHROMIUM_THREADSAFE
        crLockMutex(&stub.mutex);
#endif
        switch (pMsgInfo->message)
        {
            case WM_MOVING:
            case WM_SIZING:
            case WM_MOVE:
            case WM_ACTIVATEAPP:
            case WM_NCPAINT:
            case WM_NCACTIVATE:
            case WM_ERASEBKGND:
            case WM_CREATE:
            case WM_SIZE:
            case WM_SHOWWINDOW:
            {
                crHashtableWalk(stub.windowTable, stubCBCheckWindowsInfo, (void *) lParam);
                break;
            }

            /* @todo remove it*/
            default:
            {
                /*crDebug("hook: unknown message (%d)", pMsgInfo->message);*/
                crHashtableWalk(stub.windowTable, stubCBCheckWindowsInfo, (void *) lParam);
                break;
            }
        }
#ifdef CHROMIUM_THREADSAFE
        crUnlockMutex(&stub.mutex);
#endif
    }

    return CallNextHookEx(stub.hMessageHook, nCode, wParam, lParam);
}

void stubInstallWindowMessageHook()
{
    stub.hMessageHook = SetWindowsHookEx(WH_CALLWNDPROCRET, stubCBWindowMessageHookProc, 0, crThreadID());

    if (!stub.hMessageHook)
        crWarning("Window message hook install failed! (not fatal)");
}

void stubUninstallWindowMessageHook()
{
    if (stub.hMessageHook)
        UnhookWindowsHookEx(stub.hMessageHook);
}
#elif defined(GLX) //#ifdef WINDOWS
static GLboolean stubCheckXExtensions(WindowInfo *pWindow)
{
    int evb, erb, vmi=0, vma=0;

    if (XCompositeQueryExtension(pWindow->dpy, &evb, &erb) 
        && XCompositeQueryVersion(pWindow->dpy, &vma, &vmi) 
        && (vma>0 || vmi>=4))
    {
        crDebug("XComposite %i.%i", vma, vmi);
        vma=0;
        vmi=0;
        if (XFixesQueryExtension(pWindow->dpy, &evb, &erb) 
            && XFixesQueryVersion(pWindow->dpy, &vma, &vmi)
            && vma>=2)
        {
            crDebug("XFixes %i.%i", vma, vmi);
            return GL_TRUE;
        }
        else
        {
            crWarning("XFixes not found or old version (%i.%i), no VisibilityTracking", vma, vmi);
        }
    }
    else
    {
        crWarning("XComposite not found or old version (%i.%i), no VisibilityTracking", vma, vmi);
    }
    return GL_FALSE;
}

/*
 *  Updates visible regions for given spu window.
 *  Returns GL_TRUE if regions changed since last call, GL_FALSE overwise.
 */
GLboolean stubUpdateWindowVisibileRegions(WindowInfo *pWindow)
{
    static GLboolean bExtensionsChecked = GL_FALSE;

    XserverRegion xreg;
    int cRects, i;
    XRectangle *pXRects;
    GLint* pGLRects;

    if (bExtensionsChecked || stubCheckXExtensions(pWindow))
    {
        bExtensionsChecked = GL_TRUE;
    }
    else
    {
        stub.trackWindowVisibleRgn = 0;
        return GL_FALSE;
    }

    /*@todo see comment regarding size/position updates and XSync, same applies to those functions but
    * it seems there's no way to get even based updates for this. Or I've failed to find the appropriate extension.
    */
    xreg = XCompositeCreateRegionFromBorderClip(pWindow->dpy, pWindow->drawable);
    pXRects = XFixesFetchRegion(pWindow->dpy, xreg, &cRects);
    XFixesDestroyRegion(pWindow->dpy, xreg);

    /* @todo For some odd reason *first* run of compiz on freshly booted VM gives us 0 cRects all the time.
     * In (!pWindow->pVisibleRegions && cRects) "&& cRects" is a workaround for that case, especially as this
     * information is useless for full screen composing managers anyway.
     */
    if ((!pWindow->pVisibleRegions && cRects)
        || pWindow->cVisibleRegions!=cRects 
        || (pWindow->pVisibleRegions && crMemcmp(pWindow->pVisibleRegions, pXRects, cRects * sizeof(XRectangle))))
    {
        pWindow->pVisibleRegions = pXRects;
        pWindow->cVisibleRegions = cRects;

        pGLRects = crAlloc(4*cRects*sizeof(GLint));
        if (!pGLRects)
        {
            crWarning("stubUpdateWindowVisibileRegions: failed to allocate %i bytes", 4*cRects*sizeof(GLint));
            return GL_FALSE;
        }

        //crDebug("Got %i rects.", cRects);
        for (i=0; i<cRects; ++i)
        {
            pGLRects[4*i+0] = pXRects[i].x;
            pGLRects[4*i+1] = pXRects[i].y;
            pGLRects[4*i+2] = pXRects[i].x+pXRects[i].width;
            pGLRects[4*i+3] = pXRects[i].y+pXRects[i].height;
            //crDebug("Rect[%i]=(%i,%i,%i,%i)", i, pGLRects[4*i+0], pGLRects[4*i+1], pGLRects[4*i+2], pGLRects[4*i+3]);                   
        }

        crDebug("Dispatched WindowVisibleRegion (%i, cRects=%i)", pWindow->spuWindow, cRects);
        stub.spuDispatch.WindowVisibleRegion(pWindow->spuWindow, cRects, pGLRects);
        crFree(pGLRects);
        return GL_TRUE;
    }
    else
    {
        XFree(pXRects);
    }

    return GL_FALSE;
}
#endif //#ifdef WINDOWS
