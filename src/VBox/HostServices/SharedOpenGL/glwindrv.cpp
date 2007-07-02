/** @file
 *
 * VBox OpenGL
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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

    pClient->hwnd= CreateWindow("VBoxOGL", "VirtualBox OpenGL", 
		                        WS_CAPTION | WS_POPUPWINDOW | WS_VISIBLE,
		                        0, 0, 0, 0,
		                        NULL, NULL, 0, NULL);	
    Assert(pClient->hwnd);
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
            DeleteDC(pClient->hdc);
    	PostMessage(pClient->hwnd, WM_CLOSE, 0, 0);
    }
#endif
    return VINF_SUCCESS;
}


/* Driver functions */
void vboxglDrvCreateContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    HGLRC glrc;

    OGL_CMD(DrvCreateContext, 6);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(uint32_t, cx);
    OGL_PARAM(uint32_t, cy);
    OGL_PARAM(BYTE, cColorBits);
    OGL_PARAM(BYTE, iPixelType);
    OGL_PARAM(BYTE, cDepthBits);

    Log(("DrvCreateContext %x (%d,%d) bpp=%d type=%x depth=%d\n", hdc, cx, cy, cColorBits, iPixelType, cDepthBits));
#ifdef VBOX_OGL_DEBUG_WINDOW_OUTPUT
    if (!pClient->hwnd)
    {
        RTThreadCreate(NULL, vboxWndThread, pClient, 0, RTTHREADTYPE_DEFAULT, 0, "OpenGLWnd");
        while (!pClient->hwnd)
            RTThreadSleep(100);
    }
    SetWindowPos(pClient->hwnd, NULL, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    PIXELFORMATDESCRIPTOR pfd = {0};
    int format;

    pClient->hdc = GetDC(pClient->hwnd);

    pfd.nSize       = sizeof(pfd);
    pfd.nVersion    = 1;
    pfd.dwFlags     = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType  = iPixelType;
    pfd.cColorBits  = cColorBits;
    pfd.cDepthBits  = cDepthBits;
    pfd.iLayerType  = PFD_MAIN_PLANE;
    uint32_t lasterr = glGetError();
    format = ChoosePixelFormat(pClient->hdc, &pfd);
    lasterr = glGetError();
    SetPixelFormat(pClient->hdc, format, &pfd);

    lasterr = glGetError();
    glrc = wglCreateContext(pClient->hdc);
    lasterr = glGetError();
    Assert(glrc);
#else
    AssertFailed();
    glrc = 0;
#endif

    pClient->lastretval = (uint64_t)glrc;
}

void vboxglDrvSetContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    BOOL ret;

    OGL_CMD(DrvSetContext, 2);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(HGLRC, hglrc);

    Log(("DrvSetyContext %x %x\n", hdc, hglrc));
#ifdef VBOX_OGL_DEBUG_WINDOW_OUTPUT
    ret = wglMakeCurrent(pClient->hdc, hglrc);
    if (!ret)
        Log(("wglMakeCurrent failed with %d\n", GetLastError()));
#else
    AssertFailed();
#endif
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
}

void vboxglDrvReleaseContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    BOOL ret;

    OGL_CMD(DrvReleaseContext, 1);
    OGL_PARAM(HGLRC, hglrc);

    Log(("DrvReleaseContext %x\n", hglrc));
    /* clear current selection */
    ret = wglMakeCurrent(pClient->hdc, NULL);
    if (!ret)
        Log(("wglMakeCurrent failed with %d\n", GetLastError()));
}

void vboxglDrvDeleteContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    BOOL ret;

    OGL_CMD(DrvDeleteContext, 1);
    OGL_PARAM(HGLRC, hglrc);

    Log(("DrvDeleteContext %x\n", hglrc));
    ret = wglDeleteContext(hglrc);
    if (!ret)
        Log(("wglDeleteContext failed with %d\n", GetLastError()));
}

void vboxglDrvCreateLayerContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    HGLRC glrc;

    OGL_CMD(DrvCreateLayerContext, 7);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iLayerPlane);
    OGL_PARAM(uint32_t, cx);
    OGL_PARAM(uint32_t, cy);
    OGL_PARAM(BYTE, cColorBits);
    OGL_PARAM(BYTE, iPixelType);
    OGL_PARAM(BYTE, cDepthBits);

    Log(("DrvCreateLayerContext %x (%d,%d) bpp=%d type=%x depth=%d\n", hdc, cx, cy, cColorBits, iPixelType, cDepthBits));
#ifdef VBOX_OGL_DEBUG_WINDOW_OUTPUT
    if (!pClient->hwnd)
    {
	    pClient->hwnd= CreateWindow("VBoxOGL", "VirtualBox OpenGL", 
		                            WS_CAPTION | WS_POPUPWINDOW | WS_VISIBLE,
		                            0, 0, cx, cy,
		                            NULL, NULL, 0, NULL);	
    }
    else
    {
        SetWindowPos(pClient->hwnd, NULL, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
    PIXELFORMATDESCRIPTOR pfd = {0};
    int format;

    pClient->hdc = GetDC(pClient->hwnd);

    pfd.nSize       = sizeof(pfd);
    pfd.nVersion    = 1;
    pfd.dwFlags     = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType  = iPixelType;
    pfd.cColorBits  = cColorBits;
    pfd.cDepthBits  = cDepthBits;
    pfd.iLayerType  = PFD_MAIN_PLANE;
    format = ChoosePixelFormat(pClient->hdc, &pfd);
    SetPixelFormat(pClient->hdc, format, &pfd);

    glrc = wglCreateLayerContext(pClient->hdc, iLayerPlane);
#else
    AssertFailed();
#endif
}

void vboxglDrvShareLists(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvShareLists, 3);
    OGL_PARAM(HGLRC, hglrc1);
    OGL_PARAM(HGLRC, hglrc2);

    Log(("DrvShareLists %x %x\n", hglrc1, hglrc2));
    pClient->lastretval = wglShareLists(hglrc1, hglrc2);
}


void vboxglDrvRealizeLayerPalette(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvRealizeLayerPalette, 3);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iLayerPlane);
    OGL_PARAM(BOOL, bRealize);
    Log(("DrvRealizeLayerPalette %x %d %d\n", hdc, iLayerPlane, bRealize));
    pClient->lastretval = wglRealizeLayerPalette(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), iLayerPlane, bRealize);
}

void vboxglDrvSwapLayerBuffers(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvSwapLayerBuffers, 2);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(UINT, fuPlanes);
    Log(("DrvSwapLayerBuffers %x %d\n", hdc, fuPlanes));
    pClient->lastretval = wglSwapLayerBuffers(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), fuPlanes);
}

void vboxglDrvSetPixelFormat(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    int rc;
    PIXELFORMATDESCRIPTOR pfd;

    OGL_CMD(DrvSetPixelFormat, 2);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iPixelFormat);

    Log(("DrvSetPixelFormat %x %d\n", hdc, iPixelFormat));
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

    Log(("DrvDescribePixelFormat %x %d %d %x\n", hdc, iPixelFormat, nBytes, ppfd));
    pClient->lastretval = DescribePixelFormat(VBOX_OGL_GUEST_TO_HOST_HDC(hdc), iPixelFormat, nBytes, ppfd);
    if (!pClient->lastretval)
        Log(("DescribePixelFormat failed with %d\n", GetLastError()));
}
