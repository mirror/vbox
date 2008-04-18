/** @file
 * VBox OpenGL
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <VBox/err.h>
#include "vboxgl.h"

#define LOG_GROUP LOG_GROUP_SHARED_OPENGL
#include <VBox/log.h>
#include "gldrv.h"

#ifdef VBOX_OGL_DEBUG_WINDOW_OUTPUT
LRESULT CALLBACK VBoxOGLWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        return 0;

    case WM_CLOSE:
        PostQuitMessage( 0 );
        return 0;

    case WM_DESTROY:
        return 0;

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }
}

DECLCALLBACK(int) vboxWndThread(RTTHREAD ThreadSelf, void *pvUser)
{
    VBOXOGLCTX *pClient = (VBOXOGLCTX *)pvUser;
    HWND        hwnd;

    hwnd = pClient->hwnd= CreateWindow("VBoxOGL", "VirtualBox OpenGL",
		                               WS_CAPTION | WS_POPUPWINDOW | WS_VISIBLE,
		                               0, 0, 0, 0,
        	                        NULL, NULL, 0, NULL);
    Assert(hwnd);
    while(true)
    {
        MSG msg;

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    DestroyWindow(hwnd);
    return VINF_SUCCESS;
}

#endif

/**
 * Global init of VBox OpenGL for windows
 *
 * @returns VBox error code
 */
int vboxglGlobalInit()
{
#ifdef VBOX_OGL_DEBUG_WINDOW_OUTPUT
    WNDCLASS wc;

    wc.style            = CS_OWNDC;
    wc.lpfnWndProc      = VBoxOGLWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = 0;
    wc.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "VBoxOGL";
    RegisterClass(&wc);
#endif

    PIXELFORMATDESCRIPTOR pfd;
    int iFormat;
    /** @todo should NOT use the desktop window -> crashes the Intel OpenGL driver */
    HDC hdc = GetDC(0);

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize       = sizeof(pfd);
    pfd.nVersion    = 1;
    pfd.dwFlags     = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType  = PFD_TYPE_RGBA;
    pfd.cColorBits  = 24;
    pfd.cDepthBits  = 16;
    pfd.iLayerType  = PFD_MAIN_PLANE;
    iFormat         = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, iFormat, &pfd);

    HGLRC hRC = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hRC);

    vboxInitOpenGLExtensions();

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(0, hdc);

    return VINF_SUCCESS;
}

/**
 * Global deinit of VBox OpenGL
 *
 * @returns VBox error code
 */
int vboxglGlobalUnload()
{
    Log(("vboxglGlobalUnload"));

    return VINF_SUCCESS;
}

/**
 * Enable OpenGL
 *
 * @returns VBox error code
 * @param   pClient         Client context
 */
int vboxglEnableOpenGL(PVBOXOGLCTX pClient)
{
    PIXELFORMATDESCRIPTOR pfd;
    int iFormat;

    /** @todo should NOT use the desktop window -> crashes the Intel OpenGL driver */
    pClient->enable.hdc = GetDC(0);

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize       = sizeof(pfd);
    pfd.nVersion    = 1;
    pfd.dwFlags     = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType  = PFD_TYPE_RGBA;
    pfd.cColorBits  = 24;
    pfd.cDepthBits  = 16;
    pfd.iLayerType  = PFD_MAIN_PLANE;
    iFormat         = ChoosePixelFormat(pClient->enable.hdc, &pfd);
    SetPixelFormat(pClient->enable.hdc, iFormat, &pfd);

    pClient->enable.hglrc = wglCreateContext(pClient->enable.hdc);
    wglMakeCurrent(pClient->enable.hdc, pClient->enable.hglrc);
    return VINF_SUCCESS;
}


/**
 * Disable OpenGL
 *
 * @returns VBox error code
 * @param   pClient         Client context
 */
int vboxglDisableOpenGL(PVBOXOGLCTX pClient)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(pClient->enable.hglrc);
    ReleaseDC(0, pClient->enable.hdc);
    return VINF_SUCCESS;
}

/**
 * Client connect init
 *
 * @returns VBox error code
 * @param   pClient         Client context
 */
int vboxglConnect(PVBOXOGLCTX pClient)
{
    Log(("vboxglConnect\n"));
    return VINF_SUCCESS;
}

/**
 * Client disconnect cleanup
 *
 * @returns VBox error code
 * @param   pClient         Client context
 */
int vboxglDisconnect(PVBOXOGLCTX pClient)
{
    Log(("vboxglDisconnect\n"));
#ifdef VBOX_OGL_DEBUG_WINDOW_OUTPUT
    if (pClient->hwnd)
    {
        if (pClient->hdc)
            ReleaseDC(pClient->hwnd, pClient->hdc);
    	PostMessage(pClient->hwnd, WM_CLOSE, 0, 0);
        pClient->hwnd = 0;
    }
#endif
    return VINF_SUCCESS;
}


/* Driver functions */
void vboxglDrvCreateContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    HGLRC glrc = 0;

    OGL_CMD(DrvCreateContext, 1);
    OGL_PARAM(HDC, hdc);

    Log(("DrvCreateContext %x\n", hdc));
    Assert(VBOX_OGL_GUEST_TO_HOST_HDC(hdc));
    glrc = wglCreateContext(pClient->hdc);

    pClient->lastretval    = (uint64_t)glrc;
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvSetContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvSetContext, 2);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(HGLRC, hglrc);

    Log(("DrvSetyContext %x %x\n", hdc, hglrc));
    pClient->lastretval = wglMakeCurrent(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), hglrc);
    if (!pClient->lastretval)
        Log(("wglMakeCurrent failed with %d\n", GetLastError()));

    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvCopyContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvDeleteContext, 3);
    OGL_PARAM(HGLRC, hglrcSrc);
    OGL_PARAM(HGLRC, hglrcDst);
    OGL_PARAM(UINT,  mask);
    Log(("DrvCopyContext %x %x %x\n", hglrcSrc, hglrcDst, mask));
    pClient->lastretval = wglCopyContext(hglrcSrc, hglrcDst, mask);
    if (!pClient->lastretval)
        Log(("wglCopyContext failed with %d\n", GetLastError()));

    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvReleaseContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvReleaseContext, 1);
    OGL_PARAM(HGLRC, hglrc);

    Log(("DrvReleaseContext %x\n", hglrc));
    /* clear current selection */
    pClient->lastretval = wglMakeCurrent(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), NULL);
    if (!pClient->lastretval)
        Log(("wglMakeCurrent failed with %d\n", GetLastError()));
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvDeleteContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvDeleteContext, 1);
    OGL_PARAM(HGLRC, hglrc);

    Log(("DrvDeleteContext %x\n", hglrc));
    pClient->lastretval = wglDeleteContext(hglrc);
    if (!pClient->lastretval)
        Log(("wglDeleteContext failed with %d\n", GetLastError()));
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvCreateLayerContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    HGLRC glrc = 0;

    OGL_CMD(DrvCreateLayerContext, 2);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iLayerPlane);

    Log(("DrvCreateLayerContext %x %d\n", hdc, iLayerPlane));
    Assert(VBOX_OGL_GUEST_TO_HOST_HDC(hdc));
    glrc = wglCreateLayerContext(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), iLayerPlane);

    pClient->lastretval    = (uint64_t)glrc;
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvShareLists(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvShareLists, 3);
    OGL_PARAM(HGLRC, hglrc1);
    OGL_PARAM(HGLRC, hglrc2);

    Log(("DrvShareLists %x %x\n", hglrc1, hglrc2));
    pClient->lastretval = wglShareLists(hglrc1, hglrc2);
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}


void vboxglDrvRealizeLayerPalette(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvRealizeLayerPalette, 3);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iLayerPlane);
    OGL_PARAM(BOOL, bRealize);
    Log(("DrvRealizeLayerPalette %x %d %d\n", hdc, iLayerPlane, bRealize));
    pClient->lastretval = wglRealizeLayerPalette(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), iLayerPlane, bRealize);
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvSwapLayerBuffers(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvSwapLayerBuffers, 2);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(UINT, fuPlanes);
    Log(("DrvSwapLayerBuffers %x %d\n", hdc, fuPlanes));
    pClient->lastretval = wglSwapLayerBuffers(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), fuPlanes);
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvSetPixelFormat(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    int rc;
    PIXELFORMATDESCRIPTOR pfd;

    OGL_CMD(DrvSetPixelFormat, 4);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iPixelFormat);
    OGL_PARAM(uint32_t, cx);
    OGL_PARAM(uint32_t, cy);

#ifdef VBOX_OGL_DEBUG_WINDOW_OUTPUT
    if (!pClient->hwnd)
    {
        RTThreadCreate(NULL, vboxWndThread, pClient, 0, RTTHREADTYPE_DEFAULT, 0, "OpenGLWnd");
        while (!pClient->hwnd)
            RTThreadSleep(100);
    }
    SetWindowPos(pClient->hwnd, NULL, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    pClient->hdc = GetDC(pClient->hwnd);

    rc = DescribePixelFormat(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), iPixelFormat, sizeof(pfd), &pfd);
    if (rc)
    {
        pClient->lastretval = SetPixelFormat(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), iPixelFormat, &pfd);
    }
    else
    {
        Log(("DescribePixelFormat %d failed with 0 (%d)\n", iPixelFormat, GetLastError()));
        pClient->lastretval = 0;
    }

#else
    AssertFailed();
#endif

    Log(("DrvSetPixelFormat %x %d (%d,%d)\n", hdc, iPixelFormat, cx, cy));
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvSwapBuffers(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvSwapBuffers, 1);
    OGL_PARAM(HDC, hdc);

    Log(("DrvSwapBuffers %x\n", hdc));
    pClient->lastretval = SwapBuffers(VBOX_OGL_GUEST_TO_HOST_HDC(hdc));
    if (!pClient->lastretval)
        Log(("SwapBuffers failed with %d\n", GetLastError()));

    /** @todo sync bitmap/screen contents */
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvDescribeLayerPlane(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    PLAYERPLANEDESCRIPTOR plpd;

    OGL_CMD(DrvDescribeLayerPlane, 4);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iPixelFormat);
    OGL_PARAM(int, iLayerPlane);
    OGL_PARAM(UINT, nBytes);
    Assert(pClient->cbLastParam == nBytes);
    plpd = (PLAYERPLANEDESCRIPTOR)pClient->pLastParam;

    Log(("DrvDescribeLayerPlane %x %d %d %d %x\n", hdc, iPixelFormat, iLayerPlane, nBytes, plpd));
    pClient->lastretval = wglDescribeLayerPlane(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), iPixelFormat, iLayerPlane, nBytes, plpd);
    if (!pClient->lastretval)
        Log(("wglDescribeLayerPlane failed with %d\n", GetLastError()));
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvSetLayerPaletteEntries(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvSetLayerPaletteEntries, 5);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iLayerPlane);
    OGL_PARAM(int, iStart);
    OGL_PARAM(int, cEntries);
    OGL_MEMPARAM(COLORREF, pcr);

    Log(("DrvSetLayerPaletteEntries %x %d %d %d %x\n", hdc, iLayerPlane, iStart, cEntries, pcr));
    pClient->lastretval = wglSetLayerPaletteEntries(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), iLayerPlane, iStart, cEntries, pcr);
    if (!pClient->lastretval)
        Log(("wglSetLayerPaletteEntries failed with %d\n", GetLastError()));
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvGetLayerPaletteEntries(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    COLORREF *pcr;

    OGL_CMD(DrvGetLayerPaletteEntries, 4);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iLayerPlane);
    OGL_PARAM(int, iStart);
    OGL_PARAM(int, cEntries);

    Assert(pClient->cbLastParam == sizeof(COLORREF)*cEntries);
    pcr = (COLORREF *)pClient->pLastParam;

    Log(("DrvGetLayerPaletteEntries %x %d %d %d %x\n", hdc, iLayerPlane, iStart, cEntries, pcr));
    pClient->lastretval = wglGetLayerPaletteEntries(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), iLayerPlane, iStart, cEntries, pcr);
    if (!pClient->lastretval)
        Log(("wglGetLayerPaletteEntries failed with %d\n", GetLastError()));
    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();
}

void vboxglDrvDescribePixelFormat(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    LPPIXELFORMATDESCRIPTOR ppfd;

    OGL_CMD(DrvDescribePixelFormat, 3);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iPixelFormat);
    OGL_PARAM(UINT, nBytes);
    Assert(pClient->cbLastParam == nBytes);
    ppfd = (LPPIXELFORMATDESCRIPTOR)pClient->pLastParam;

    hdc = VBOX_OGL_GUEST_TO_HOST_HDC(hdc);
    if (!hdc)
        hdc = GetDC(0);

    Log(("DrvDescribePixelFormat %x %d %d %x\n", hdc, iPixelFormat, nBytes, ppfd));
    pClient->lastretval = DescribePixelFormat(hdc, iPixelFormat, nBytes, ppfd);
    if (!pClient->lastretval)
        Log(("DescribePixelFormat failed with %d\n", GetLastError()));

    pClient->fHasLastError = true;
    pClient->ulLastError   = GetLastError();

    if (!VBOX_OGL_GUEST_TO_HOST_HDC(hdc))
        ReleaseDC(0, pClient->hdc);
}

RTUINTPTR vboxDrvIsExtensionAvailable(char *pszExtFunctionName)
{
    RTUINTPTR pfnProc = (RTUINTPTR)wglGetProcAddress(pszExtFunctionName);

    Log(("vboxDrvIsExtensionAvailable %s -> %d\n", pszExtFunctionName, !!pfnProc));
    return pfnProc;
}
